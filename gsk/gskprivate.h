#pragma once

#include <glib.h>
#include <pango/pango.h>

G_BEGIN_DECLS

void       gsk_ensure_resources  (void);

PangoFont *gsk_get_scaled_font   (PangoFont *font,
                                  float      scale);

G_END_DECLS

