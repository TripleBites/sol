#include "waveform_view.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Init / destroy                                                      */
/* ------------------------------------------------------------------ */
static void wv_init(Node *self) {
    node_base_init(self, &waveform_view_class);
    WaveformView *wv = (WaveformView*)self;

    wv->base.anchor       = anchor_full_rect();
    wv->base.mouse_filter = MOUSE_FILTER_IGNORE;
    wv->base.focus_mode   = FOCUS_NONE;

    wv->samples      = NULL;
    wv->sample_count = 0;
    wv->max_samples  = 1024;

    wv->line_color   = color_rgba(0.3f, 0.8f, 0.4f, 1.0f);
    wv->bg_color     = color_rgba(0.05f, 0.05f, 0.08f, 1.0f);
    wv->line_width   = 1.0f;
}

static void wv_destroy(Node *self) {
    node_base_destroy(self);
}

/* ------------------------------------------------------------------ */
/* Layout                                                              */
/* ------------------------------------------------------------------ */
static void wv_get_minimum_size(Node *self, Vec2 *out) {
    WaveformView *wv = (WaveformView*)self;
    Control *c = (Control*)self;
    float w = 200.0f;
    float h = 80.0f;

    if (c->explicit_min_size.x > w) w = c->explicit_min_size.x;
    if (c->explicit_min_size.y > h) h = c->explicit_min_size.y;

    c->min_size.x = w;
    c->min_size.y = h;
    out->x = w;
    out->y = h;
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */
static void wv_draw(Node *self, DrawList *dl) {
    WaveformView *wv = (WaveformView*)self;
    Rect r = wv->base.global_rect;

    /* Background */
    draw_list_add_rect_filled(dl, r, wv->bg_color);

    /* Border */
    draw_list_add_rect_border(dl, r,
        color_rgba(0.2f, 0.2f, 0.25f, 1.0f), 1.0f);

    if (!wv->samples || wv->sample_count < 2) return;

    /* Compute line strip points */
    int n = wv->sample_count;
    if (n > wv->max_samples) n = wv->max_samples;

    /* Dynamic allocation for the frame — freed by draw_list_clear */
    Vec2 *pts = malloc(sizeof(Vec2) * (size_t)n);
    if (!pts) return;

    float pad = 2.0f;
    float draw_w = r.w - pad * 2.0f;
    float draw_h = r.h - pad * 2.0f;
    float mid_y = r.y + r.h * 0.5f;
    float half_h = draw_h * 0.5f;

    for (int i = 0; i < n; i++) {
        pts[i].x = r.x + pad + (draw_w * (float)i / (float)(n - 1));
        /* Clamp sample to [-1, 1] range before mapping */
        float s = wv->samples[i];
        if (s < -1.0f) s = -1.0f;
        if (s >  1.0f) s =  1.0f;
        pts[i].y = mid_y - s * half_h;
    }

    draw_list_add_line_strip(dl, pts, (size_t)n, wv->line_color);
    free(pts);
}

/* ------------------------------------------------------------------ */
/* VTable                                                              */
/* ------------------------------------------------------------------ */
const NodeClass waveform_view_class = {
    .type_name        = "WaveformView",
    .instance_size    = sizeof(WaveformView),
    .init             = wv_init,
    .destroy          = wv_destroy,
    .enter_tree       = NULL,
    .exit_tree        = NULL,
    .ready            = NULL,
    .process          = NULL,
    .draw             = wv_draw,
    .get_minimum_size = wv_get_minimum_size,
    .arrange_children = NULL,
    .handle_input     = NULL,
};

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
WaveformView *waveform_view_new(void) {
    return (WaveformView*)node_new(&waveform_view_class);
}

void waveform_view_set_data(WaveformView *wv, const float *samples, int count) {
    if (!wv) return;
    wv->samples      = samples;
    wv->sample_count = count;
}

void waveform_view_set_colors(WaveformView *wv, Color line, Color bg) {
    if (!wv) return;
    wv->line_color = line;
    wv->bg_color   = bg;
}

void waveform_view_set_max_samples(WaveformView *wv, int max_samples) {
    if (!wv) return;
    wv->max_samples = max_samples;
}
