#ifndef SOL_UI_WAVEFORM_VIEW_H
#define SOL_UI_WAVEFORM_VIEW_H

#include "control.h"

/* WaveformView — draws a line-strip from a float sample buffer.
   The buffer is owned by the caller (typically an AudioPipeline probe).
   Call waveform_view_set_data() each frame to update. */

typedef struct {
    Control base;

    /* Data */
    const float *samples;      /* pointer to sample buffer (not owned) */
    int          sample_count;
    int          max_samples;

    /* Appearance */
    Color  line_color;
    Color  bg_color;
    float  line_width;
} WaveformView;

WaveformView *waveform_view_new(void);

/* Set the sample data to display. Buffer must remain valid until next call. */
void waveform_view_set_data(WaveformView *wv, const float *samples, int count);

/* Configure appearance */
void waveform_view_set_colors(WaveformView *wv, Color line, Color bg);
void waveform_view_set_max_samples(WaveformView *wv, int max_samples);

extern const NodeClass waveform_view_class;

#endif /* SOL_UI_WAVEFORM_VIEW_H */
