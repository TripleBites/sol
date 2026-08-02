#ifndef SOL_AUDIO_OSC_H
#define SOL_AUDIO_OSC_H

#include "audio_node.h"

/* --- Waveform types --- */
typedef enum {
    OSC_SINE = 0,
    OSC_SQUARE,
    OSC_SAW,
    OSC_TRIANGLE,
    OSC_NOISE,
    OSC_COUNT
} OscWaveform;

/* --- OscillatorNode --- */
typedef struct {
    AudioNode base;

    OscWaveform waveform;
    float freq;          /* Hz */
    float amp;           /* 0.0 – 1.0 */
    float phase;         /* current phase in radians */
    float step;          /* phase increment per sample */
    int   sample_rate;
} OscillatorNode;

/* --- API --- */
OscillatorNode* osc_node_new(OscWaveform wf, float freq, float amp, int sample_rate);
void osc_node_set_freq(OscillatorNode* osc, float freq);
void osc_node_set_amp(OscillatorNode* osc, float amp);
void osc_node_set_sample_rate(OscillatorNode* osc, int sr);

/* Name → waveform lookup for serialization */
OscWaveform osc_waveform_from_name(const char* name);
const char* osc_waveform_name(OscWaveform wf);

#endif /* SOL_AUDIO_OSC_H */
