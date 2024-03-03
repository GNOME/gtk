#pragma once

#include <glib.h>
#include <pango/pango.h>
#include <cairo.h>

G_BEGIN_DECLS

void       gsk_ensure_resources  (void);

PangoFont *gsk_get_scaled_font   (PangoFont *font,
                                  float      scale);

PangoFont *gsk_get_hinted_font (PangoFont            *font,
                                cairo_hint_style_t    hint_style,
                                cairo_antialias_t     antialias);

G_END_DECLS

