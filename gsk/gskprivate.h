#pragma once

#include <glib.h>
#include <pango/pango.h>
#include <cairo.h>

G_BEGIN_DECLS

void       gsk_ensure_resources          (void);

PangoFont *gsk_reload_font               (PangoFont            *font,
                                          float                 scale,
                                          cairo_hint_metrics_t  hint_metrics,
                                          cairo_hint_style_t    hint_style,
                                          cairo_antialias_t     antialias);

void       gsk_font_get_rendering        (PangoFont            *font,
                                          float                 scale,
                                          cairo_hint_metrics_t *hint_metrics,
                                          cairo_hint_style_t   *hint_style,
                                          cairo_antialias_t    *antialias);

void       gsk_font_get_extents          (PangoFont            *font,
                                          PangoGlyphString     *glyphs,
                                          PangoRectangle       *ink_rect);

G_END_DECLS
