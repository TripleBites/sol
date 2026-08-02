#include "style_box.h"
#include <math.h>

/* ------------------------------------------------------------------ */
/* Constructors                                                        */
/* ------------------------------------------------------------------ */
StyleBox style_box_flat(Color bg) {
    StyleBox sb;
    sb.bg_color     = bg;
    sb.border_color = color_rgba(0, 0, 0, 0);
    sb.border_width = 0;
    sb.corner_radius = 0;
    sb.margin_left   = 0;
    sb.margin_top    = 0;
    sb.margin_right  = 0;
    sb.margin_bottom = 0;
    return sb;
}

StyleBox style_box_rounded(Color bg, float radius) {
    StyleBox sb = style_box_flat(bg);
    sb.corner_radius = radius;
    return sb;
}

StyleBox style_box_bordered(Color bg, Color border, float width) {
    StyleBox sb = style_box_flat(bg);
    sb.border_color = border;
    sb.border_width = width;
    return sb;
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */
void style_box_draw(const StyleBox *sb, DrawList *dl, Rect rect) {
    if (!sb || !dl) return;

    /* Background fill */
    if (sb->bg_color.a > 0.0f) {
        if (sb->corner_radius > 0.0f) {
            draw_list_add_rect_filled_rounded(dl, rect, sb->bg_color,
                                              sb->corner_radius);
        } else {
            draw_list_add_rect_filled(dl, rect, sb->bg_color);
        }
    }

    /* Border */
    if (sb->border_width > 0.0f && sb->border_color.a > 0.0f) {
        /* Draw border as a slightly larger rect behind, then bg on top.
           Simplified: just draw a filled rect at border color, then a
           smaller filled rect at bg color on top. */
        if (sb->corner_radius > 0.0f) {
            draw_list_add_rect_filled_rounded(dl, rect, sb->border_color,
                                              sb->corner_radius);

            Rect inner = style_box_get_inner_rect(sb, rect);
            /* Adjust inner to account for border */
            Rect border_inner = {
                rect.x + sb->border_width,
                rect.y + sb->border_width,
                rect.w - sb->border_width * 2,
                rect.h - sb->border_width * 2,
            };
            if (border_inner.w < 0) border_inner.w = 0;
            if (border_inner.h < 0) border_inner.h = 0;

            draw_list_add_rect_filled_rounded(
                dl, border_inner, sb->bg_color,
                sb->corner_radius > sb->border_width
                    ? sb->corner_radius - sb->border_width : 0);
        } else {
            /* Flat border: draw 4 thin rects for each edge */
            draw_list_add_rect_filled(
                dl, rect_make(rect.x, rect.y, rect.w, sb->border_width),
                sb->border_color);
            draw_list_add_rect_filled(
                dl, rect_make(rect.x, rect.y + rect.h - sb->border_width,
                              rect.w, sb->border_width),
                sb->border_color);
            draw_list_add_rect_filled(
                dl, rect_make(rect.x, rect.y, sb->border_width, rect.h),
                sb->border_color);
            draw_list_add_rect_filled(
                dl, rect_make(rect.x + rect.w - sb->border_width, rect.y,
                              sb->border_width, rect.h),
                sb->border_color);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Content area                                                        */
/* ------------------------------------------------------------------ */
Rect style_box_get_inner_rect(const StyleBox *sb, Rect outer) {
    Rect r;
    r.x = outer.x + sb->margin_left + sb->border_width;
    r.y = outer.y + sb->margin_top  + sb->border_width;
    r.w = outer.w - sb->margin_left - sb->margin_right
                   - sb->border_width * 2;
    r.h = outer.h - sb->margin_top - sb->margin_bottom
                   - sb->border_width * 2;
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}
