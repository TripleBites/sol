#include "osc.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* Case-insensitive string compare (C99, no POSIX dependency) */
static int str_case_eq(const char* a, const char* b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

/* ------------------------------------------------------------------ */
/* Process functions (hot path — called from audio thread)              */
/* ------------------------------------------------------------------ */

static void sine_process(AudioNode* self, float* buffer, int n_frames) {
    OscillatorNode* osc = (OscillatorNode*)self;
    float p = osc->phase;
    float s = osc->step;
    float a = osc->amp;
    for (int i = 0; i < n_frames; i++) {
        buffer[i] += a * sinf(p);
        p += s;
        if (p > 2.0f * M_PI) p -= 2.0f * M_PI;
    }
    osc->phase = p;
}

static void square_process(AudioNode* self, float* buffer, int n_frames) {
    OscillatorNode* osc = (OscillatorNode*)self;
    float p = osc->phase;
    float s = osc->step;
    float a = osc->amp;
    for (int i = 0; i < n_frames; i++) {
        buffer[i] += (sinf(p) >= 0.0f) ? a : -a;
        p += s;
        if (p > 2.0f * M_PI) p -= 2.0f * M_PI;
    }
    osc->phase = p;
}

static void saw_process(AudioNode* self, float* buffer, int n_frames) {
    OscillatorNode* osc = (OscillatorNode*)self;
    float p = osc->phase;
    float s = osc->step;
    float a = osc->amp;
    float inv_pi = 1.0f / (float)M_PI;
    for (int i = 0; i < n_frames; i++) {
        /* Sawtooth: 2*(phase/(2*PI) - floor(0.5 + phase/(2*PI))) */
        float t = p * (0.5f * inv_pi);  /* phase / (2*PI) */
        float val = 2.0f * (t - floorf(0.5f + t));
        buffer[i] += a * val;
        p += s;
        if (p > 2.0f * M_PI) p -= 2.0f * M_PI;
    }
    osc->phase = p;
}

static void tri_process(AudioNode* self, float* buffer, int n_frames) {
    OscillatorNode* osc = (OscillatorNode*)self;
    float p = osc->phase;
    float s = osc->step;
    float a = osc->amp;
    float inv_pi = 1.0f / (float)M_PI;
    for (int i = 0; i < n_frames; i++) {
        float t = p * (0.5f * inv_pi);
        float val = 2.0f * (t - floorf(0.5f + t));
        val = (fabsf(val) - 0.5f) * 2.0f;  /* triangle from saw */
        buffer[i] += a * val;
        p += s;
        if (p > 2.0f * M_PI) p -= 2.0f * M_PI;
    }
    osc->phase = p;
}

static void noise_process(AudioNode* self, float* buffer, int n_frames) {
    OscillatorNode* osc = (OscillatorNode*)self;
    float a = osc->amp;
    for (int i = 0; i < n_frames; i++) {
        /* Simple white noise: random float in [-1, 1] */
        float r = (float)rand() / (float)RAND_MAX;
        buffer[i] += a * (2.0f * r - 1.0f);
    }
}

static AudioProcessFunc osc_process_funcs[OSC_COUNT] = {
    [OSC_SINE]     = sine_process,
    [OSC_SQUARE]   = square_process,
    [OSC_SAW]      = saw_process,
    [OSC_TRIANGLE] = tri_process,
    [OSC_NOISE]    = noise_process,
};

/* ------------------------------------------------------------------ */
/* Param control                                                       */
/* ------------------------------------------------------------------ */
static void osc_set_param(AudioNode* self, const char* name, float value) {
    OscillatorNode* osc = (OscillatorNode*)self;
    if (strcmp(name, "freq") == 0) {
        osc_node_set_freq(osc, value);
    } else if (strcmp(name, "amp") == 0) {
        osc->amp = value;
    } else if (strcmp(name, "waveform") == 0) {
        int wf = (int)value;
        if (wf >= 0 && wf < OSC_COUNT) {
            osc->waveform = wf;
            osc->base.process_audio = osc_process_funcs[wf];
        }
    }
}

static float osc_get_param(AudioNode* self, const char* name) {
    OscillatorNode* osc = (OscillatorNode*)self;
    if (strcmp(name, "freq") == 0) return osc->freq;
    if (strcmp(name, "amp") == 0)  return osc->amp;
    if (strcmp(name, "waveform") == 0) return (float)osc->waveform;
    return 0.0f;
}

/* ------------------------------------------------------------------ */
/* Construction                                                        */
/* ------------------------------------------------------------------ */
OscillatorNode* osc_node_new(OscWaveform wf, float freq, float amp,
                              int sample_rate) {
    OscillatorNode* osc = (OscillatorNode*)audio_node_alloc(sizeof(OscillatorNode));
    if (!osc) return NULL;

    osc->waveform    = wf;
    osc->freq        = freq;
    osc->amp         = amp;
    osc->phase       = 0.0f;
    osc->sample_rate = sample_rate;
    osc->step        = 2.0f * M_PI * freq / (float)sample_rate;

    osc->base.process_audio = osc_process_funcs[wf];
    osc->base.set_param     = osc_set_param;
    osc->base.get_param     = osc_get_param;

    return osc;
}

void osc_node_set_freq(OscillatorNode* osc, float freq) {
    osc->freq = freq;
    osc->step = 2.0f * M_PI * freq / (float)osc->sample_rate;
}

void osc_node_set_amp(OscillatorNode* osc, float amp) {
    osc->amp = amp;
}

void osc_node_set_sample_rate(OscillatorNode* osc, int sr) {
    osc->sample_rate = sr;
    osc->step = 2.0f * M_PI * osc->freq / (float)sr;
}

/* ------------------------------------------------------------------ */
/* Name ↔ waveform                                                     */
/* ------------------------------------------------------------------ */
static const char* osc_names[OSC_COUNT] = {
    "SINE", "SQUARE", "SAW", "TRIANGLE", "NOISE"
};

const char* osc_waveform_name(OscWaveform wf) {
    if (wf < 0 || wf >= OSC_COUNT) return "SINE";
    return osc_names[wf];
}

OscWaveform osc_waveform_from_name(const char* name) {
    for (int i = 0; i < OSC_COUNT; i++) {
        if (str_case_eq(name, osc_names[i])) return (OscWaveform)i;
    }
    return OSC_SINE;
}
