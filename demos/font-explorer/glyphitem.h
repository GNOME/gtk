#pragma once

#include <gtk/gtk.h>

#define GLYPH_ITEM_TYPE (glyph_item_get_type ())
#define GLYPH_ITEM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLYPH_ITEM_TYPE, GlyphItem))


typedef struct _GlyphItem         GlyphItem;
typedef struct _GlyphItemClass    GlyphItemClass;


GType       glyph_item_get_type (void);
GlyphItem * glyph_item_new      (Pango2Font *font,
                                 hb_codepoint_t  glyph);
hb_codepoint_t glyph_item_get_glyph (GlyphItem *item);
Pango2Font * glyph_item_get_font (GlyphItem *item);
