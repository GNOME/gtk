#pragma once

#include <gtk/gtk.h>

G_DECLARE_FINAL_TYPE (GlyphChooser, glyph_chooser, GLYPH, CHOOSER, GtkWidget)

GtkWidget * glyph_chooser_new (void);
