#pragma once

#include <glib.h>
#include <pango/pango.h>
#include <cairo.h>

G_BEGIN_DECLS

#define RAD_TO_DEG(x)          ((x) / (G_PI / 180.f))
#define DEG_TO_RAD(x)          ((x) * (G_PI / 180.f))

void       gsk_ensure_resources  (void);

PangoFont *gsk_reload_font (PangoFont            *font,
                            float                 scale,
                            cairo_hint_metrics_t  hint_metrics,
                            cairo_hint_style_t    hint_style,
                            cairo_antialias_t     antialias);

cairo_hint_style_t gsk_font_get_hint_style (PangoFont *font);

void gsk_string_append_double (GString *string,
                               double   d);

void gsk_sincosf_deg (float  deg,
                      float *out_s,
                      float *out_c);
void gsk_sincosf     (float  rad,
                      float *out_s,
                      float *out_c);
void gsk_sincos      (double rad,
                      double *out_s,
                      double *out_c);


G_END_DECLS

