#pragma once

#include <glib.h>
#include <pango/pango.h>

G_BEGIN_DECLS

void       gsk_ensure_resources  (void);

PangoFont *gsk_get_unhinted_font (PangoFont *font);

G_END_DECLS

