#pragma once

#include <gtk/gtk.h>

#define GLYPH_MODEL_TYPE (glyph_model_get_type ())
#define GLYPH_MODEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLYPH_MODEL_TYPE, GlyphModel))


typedef struct _GlyphModel         GlyphModel;
typedef struct _GlyphModelClass    GlyphModelClass;


GType        glyph_model_get_type (void);
GlyphModel * glyph_model_new      (Pango2Font *font);
