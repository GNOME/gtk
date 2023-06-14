#pragma once

#include <gtk/gtk.h>

G_DECLARE_FINAL_TYPE (GlyphDemo, glyph_demo, GLYPH, DEMO, GtkWidget)

GtkWidget * glyph_demo_new (void);
