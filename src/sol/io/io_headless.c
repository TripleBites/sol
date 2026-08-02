/* SPDX short identifier: MIT */

/* _POSIX_C_SOURCE must come before any system headers to avoid
   struct timespec redefinition when both pthread.h and alsa/asoundlib.h
   are included. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include "io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

/* ------------------------------------------------------------------ */
/* Headless platform — no window, real audio via ALSA snd_pcm.        */
/* The audio callback runs on a dedicated high-priority thread.       */
/* This is the default headless backend for all Linux systems.        */
/* ------------------------------------------------------------------ */

/* --- Window/frame stubs (headless = no window) --- */
static int  g_width = 800, g_height = 600;

/* --- Audio state --- */
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
        fprintf(stderr, "[headless] failed to allocate audio buffer\n");
        return NULL;
    }

    while (g_running) {
        /* Wait for PCM buffer to be ready with a 50ms timeout.
           This lets us check g_running periodically so the
           thread can shut down cleanly. */
        int err = snd_pcm_wait(g_pcm, 50);
        if (!g_running) break;
        if (err == 0) continue;  /* timeout — loop back and re-check */
        if (err < 0) {
            if (err == -EPIPE) {
                snd_pcm_prepare(g_pcm);
                continue;
            }
            fprintf(stderr, "[headless] snd_pcm_wait error: %s\n", snd_strerror(err));
            break;
        }

        /* Fill buffer */
        g_audio_callback(buffer, g_buffer_frames, g_channels, g_audio_userdata);

        /* Write interleaved float samples */
        int frames_written = snd_pcm_writei(g_pcm, buffer,
                                             (snd_pcm_uframes_t)g_buffer_frames);
        if (frames_written < 0) {
            if (frames_written == -EPIPE) {
                snd_pcm_prepare(g_pcm);
            } else if (frames_written != -ESTRPIPE) {
                fprintf(stderr, "[headless] ALSA write error: %s\n",
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
static bool headless_audio_init(int sample_rate, int channels,
                                SolAudioCallback callback, void* userdata) {
    g_sample_rate    = sample_rate;
    g_channels       = channels;
    g_buffer_frames  = 512;
    g_audio_callback = callback;
    g_audio_userdata = userdata;

    int err = snd_pcm_open(&g_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "[headless] snd_pcm_open failed: %s\n", snd_strerror(err));
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
        fprintf(stderr, "[headless] snd_pcm_set_params failed: %s\n", snd_strerror(err));
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
    printf("[headless] Audio: %dHz, %dch, buffer=%lu frames (%.1fms)\n",
           sample_rate, channels, actual_buf,
           1000.0 * actual_buf / sample_rate);

    pthread_mutex_init(&g_audio_mutex, NULL);
    g_running = true;
    err = pthread_create(&g_audio_thread, NULL, audio_thread_func, NULL);
    if (err != 0) {
        fprintf(stderr, "[headless] pthread_create failed\n");
        snd_pcm_close(g_pcm);
        g_pcm = NULL;
        g_running = false;
        return false;
    }

    return true;
}

static void headless_audio_shutdown(void) {
    g_running = false;
    if (g_pcm) {
        pthread_join(g_audio_thread, NULL);
        snd_pcm_close(g_pcm);
        g_pcm = NULL;
    }
    pthread_mutex_destroy(&g_audio_mutex);
}

static void headless_audio_lock(void) {
    pthread_mutex_lock(&g_audio_mutex);
}

static void headless_audio_unlock(void) {
    pthread_mutex_unlock(&g_audio_mutex);
}

/* ------------------------------------------------------------------ */
/* Input (headless — stubs, no window system)                          */
/* ------------------------------------------------------------------ */
static void headless_poll_input(SolIO* self) {
    (void)self;
    /* Headless: no window events. MIDI events are pushed via
       sol_push_event() from a Python thread. */
}

/* ------------------------------------------------------------------ */
/* Window/frame stubs                                                  */
/* ------------------------------------------------------------------ */
static bool headless_init(const char* title, int width, int height) {
    (void)title;
    g_width  = width;
    g_height = height;
    printf("[headless] Initialized %dx%d\n", width, height);
    return true;
}

static void headless_shutdown(void) {
    printf("[headless] Shutdown\n");
}

static bool headless_update(void) {
    /* Headless runs until g_running is set false by audio_shutdown
       or until the caller stops looping. */
    return g_running;
}

static void headless_get_size(int* w, int* h) {
    *w = g_width;
    *h = g_height;
}

/* ------------------------------------------------------------------ */
/* Platform vtable                                                     */
/* ------------------------------------------------------------------ */
static SolIO headless_platform = {
    .init           = headless_init,
    .shutdown       = headless_shutdown,
    .update         = headless_update,
    .get_size       = headless_get_size,
    .poll_input     = headless_poll_input,
    .audio_init     = headless_audio_init,
    .audio_shutdown  = headless_audio_shutdown,
    .audio_lock     = headless_audio_lock,
    .audio_unlock   = headless_audio_unlock,
};

const SolIO* sol_io_headless(void) {
    return &headless_platform;
}
