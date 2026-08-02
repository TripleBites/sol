/* SPDX short identifier: MIT */

/* io_tui.c — TUI platform backend.
 *
 * Combines ALSA audio (from io_headless.c) with terminal UI rendering
 * (from debug/tui.c) and raw keyboard input. This is the default
 * operating mode for headless development and LLM-driven iteration.
 *
 * Usage:
 *   ./neptune --tui       # TUI mode with keyboard + audio
 *   ./neptune --headless  # original ALSA-only headless
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include "io.h"
#include "../debug/tui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* Window stubs                                                        */
/* ------------------------------------------------------------------ */
static int  g_width = 80, g_height = 40;

/* ------------------------------------------------------------------ */
/* Audio state (same as headless)                                       */
/* ------------------------------------------------------------------ */
static snd_pcm_t*      g_pcm = NULL;
static pthread_t        g_audio_thread;
static SolAudioCallback g_audio_callback;
static void*            g_audio_userdata;
static int              g_sample_rate;
static int              g_channels;
static int              g_buffer_frames;
static bool             g_running = false;
static pthread_mutex_t  g_audio_mutex;

/* ------------------------------------------------------------------ */
/* Audio thread                                                        */
/* ------------------------------------------------------------------ */
static void* audio_thread_func(void* arg) {
    (void)arg;
    float* buffer = malloc((size_t)g_buffer_frames * (size_t)g_channels * sizeof(float));
    if (!buffer) {
        fprintf(stderr, "[tui] failed to allocate audio buffer\n");
        return NULL;
    }

    while (g_running) {
        int err = snd_pcm_wait(g_pcm, 50);
        if (!g_running) break;
        if (err == 0) continue;
        if (err < 0) {
            if (err == -EPIPE) {
                snd_pcm_prepare(g_pcm);
                continue;
            }
            fprintf(stderr, "[tui] snd_pcm_wait error: %s\n", snd_strerror(err));
            break;
        }

        g_audio_callback(buffer, g_buffer_frames, g_channels, g_audio_userdata);

        int frames_written = snd_pcm_writei(g_pcm, buffer,
                                             (snd_pcm_uframes_t)g_buffer_frames);
        if (frames_written < 0) {
            if (frames_written == -EPIPE) {
                snd_pcm_prepare(g_pcm);
            } else if (frames_written != -ESTRPIPE) {
                fprintf(stderr, "[tui] ALSA write error: %s\n",
                        snd_strerror(frames_written));
                break;
            }
        }
    }

    free(buffer);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Audio init / shutdown                                               */
/* ------------------------------------------------------------------ */
static bool tui_audio_init(int sample_rate, int channels,
                            SolAudioCallback callback, void* userdata) {
    g_sample_rate    = sample_rate;
    g_channels       = channels;
    g_buffer_frames  = 512;
    g_audio_callback = callback;
    g_audio_userdata = userdata;

    int err = snd_pcm_open(&g_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "[tui] snd_pcm_open failed: %s\n", snd_strerror(err));
        return false;
    }

    err = snd_pcm_set_params(g_pcm,
                             SND_PCM_FORMAT_FLOAT,
                             SND_PCM_ACCESS_RW_INTERLEAVED,
                             (unsigned int)channels,
                             (unsigned int)sample_rate,
                             1,
                             500000);
    if (err < 0) {
        fprintf(stderr, "[tui] snd_pcm_set_params failed: %s\n", snd_strerror(err));
        snd_pcm_close(g_pcm);
        g_pcm = NULL;
        return false;
    }

    snd_pcm_uframes_t actual_buf;
    snd_pcm_hw_params_t* hw;
    snd_pcm_hw_params_malloc(&hw);
    snd_pcm_hw_params_current(g_pcm, hw);
    snd_pcm_hw_params_get_buffer_size(hw, &actual_buf);
    snd_pcm_hw_params_free(hw);
    printf("[tui] Audio: %dHz, %dch, buffer=%lu frames (%.1fms)\n",
           sample_rate, channels, actual_buf,
           1000.0 * actual_buf / sample_rate);

    pthread_mutex_init(&g_audio_mutex, NULL);
    g_running = true;
    err = pthread_create(&g_audio_thread, NULL, audio_thread_func, NULL);
    if (err != 0) {
        fprintf(stderr, "[tui] pthread_create failed\n");
        snd_pcm_close(g_pcm);
        g_pcm = NULL;
        g_running = false;
        return false;
    }

    return true;
}

static void tui_audio_shutdown(void) {
    g_running = false;
    if (g_pcm) {
        pthread_join(g_audio_thread, NULL);
        snd_pcm_close(g_pcm);
        g_pcm = NULL;
    }
    pthread_mutex_destroy(&g_audio_mutex);
}

static void tui_audio_lock(void) {
    pthread_mutex_lock(&g_audio_mutex);
}

static void tui_audio_unlock(void) {
    pthread_mutex_unlock(&g_audio_mutex);
}

/* ------------------------------------------------------------------ */
/* Input (TUI — keyboard via raw terminal)                             */
/* ------------------------------------------------------------------ */
static void tui_poll_input(SolIO* self) {
    (void)self;
    /* Keyboard events are read via tui_input_poll() each frame.
       We map TUI key events to Sol input state here. */
    TuiKeyEvent ev = tui_input_poll();
    if (ev.keycode == 0) return;

    /* Map common keys to Sol keycodes */
    int sol_key = SOL_KEY_UNKNOWN;
    if (ev.is_special) {
        switch (ev.keycode) {
        case TUI_KEY_UP:        sol_key = SOL_KEY_UP; break;
        case TUI_KEY_DOWN:      sol_key = SOL_KEY_DOWN; break;
        case TUI_KEY_LEFT:      sol_key = SOL_KEY_LEFT; break;
        case TUI_KEY_RIGHT:     sol_key = SOL_KEY_RIGHT; break;
        case TUI_KEY_ESCAPE:    sol_key = SOL_KEY_ESCAPE; break;
        case TUI_KEY_ENTER:     sol_key = SOL_KEY_RETURN; break;
        case TUI_KEY_TAB:       sol_key = SOL_KEY_TAB; break;
        case TUI_KEY_BACKSPACE: sol_key = SOL_KEY_BACKSPACE; break;
        case TUI_KEY_SPACE:     sol_key = SOL_KEY_SPACE; break;
        default: break;
        }
    } else {
        /* ASCII range maps directly to USB HID keycodes */
        if (ev.keycode >= 'a' && ev.keycode <= 'z') {
            sol_key = SOL_KEY_A + (ev.keycode - 'a');
        } else if (ev.keycode >= 'A' && ev.keycode <= 'Z') {
            sol_key = SOL_KEY_A + (ev.keycode - 'A');
        } else if (ev.keycode >= '0' && ev.keycode <= '9') {
            if (ev.keycode == '0') sol_key = SOL_KEY_0;
            else sol_key = SOL_KEY_1 + (ev.keycode - '1');
        } else {
            switch (ev.keycode) {
            case ',': sol_key = SOL_KEY_UNKNOWN + 100; break;
            default:  sol_key = SOL_KEY_UNKNOWN; break;
            }
        }
    }

    if (sol_key != SOL_KEY_UNKNOWN) {
        if (ev.pressed) {
            input_state_key_down(self->input_state, sol_key);
        } else {
            input_state_key_up(self->input_state, sol_key);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Render — TUI: output DrawList as ANSI to terminal                   */
/* ------------------------------------------------------------------ */
static void tui_render_frame(void* draw_list_ptr, int width, int height) {
    if (!draw_list_ptr) return;
    DrawList* dl = (DrawList*)draw_list_ptr;

    /* Scale to terminal dimensions */
    int term_cols = 80;
    int term_rows = 24;
    if (width > 0 && height > 0) {
        term_rows = (height * term_cols) / width;
        if (term_rows < 10) term_rows = 10;
        if (term_rows > 60) term_rows = 60;
    }

    char* ansi = tui_render_ansi(dl, term_cols, term_rows);
    if (ansi) {
        fputs(ansi, stdout);
        fflush(stdout);
        free(ansi);
    }
}

/* ------------------------------------------------------------------ */
/* Window/frame                                                        */
/* ------------------------------------------------------------------ */
static bool tui_init(const char* title, int width, int height) {
    (void)title;
    g_width  = width;
    g_height = height;

    /* Terminal rendering doesn't need special setup —
       ANSI escapes work in any terminal. Keyboard input
       is handled by the app (e.g., Neptune's RawKeyboard). */
    printf("[tui] Initialized %dx%d virtual → terminal UI\n", g_width, g_height);
    return true;
}

static void tui_shutdown(void) {
    printf("\033[0m\033[2J\033[H"); /* Reset + clear */
    printf("[tui] Shutdown\n");
}

static bool tui_update(void) {
    /* TUI runs until the app stops calling update().
       Audio state is separate — managed by the app, not the platform. */
    return true;
}

static void tui_get_size(int* w, int* h) {
    *w = g_width;
    *h = g_height;
}

/* ------------------------------------------------------------------ */
/* Platform vtable                                                     */
/* ------------------------------------------------------------------ */
static SolIO tui_platform = {
    .init           = tui_init,
    .shutdown       = tui_shutdown,
    .update         = tui_update,
    .get_size       = tui_get_size,
    .poll_input     = tui_poll_input,
    .render         = tui_render_frame,
    .audio_init     = tui_audio_init,
    .audio_shutdown  = tui_audio_shutdown,
    .audio_lock     = tui_audio_lock,
    .audio_unlock   = tui_audio_unlock,
};

const SolIO* sol_io_tui(void) {
    return &tui_platform;
}
