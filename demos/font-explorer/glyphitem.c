#include "glyphitem.h"

struct _GlyphItem {
  GObject parent;
  Pango2Font *font;
  hb_codepoint_t glyph;
};

struct _GlyphItemClass {
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GlyphItem, glyph_item, G_TYPE_OBJECT)

static void
glyph_item_init (GlyphItem *self)
{
}

static void
glyph_item_finalize (GObject *object)
{
  GlyphItem *item = GLYPH_ITEM (object);

  g_object_unref (item->font);

  G_OBJECT_CLASS (glyph_item_parent_class)->finalize (object);
}

static void
glyph_item_class_init (GlyphItemClass *class)
{
  G_OBJECT_CLASS (class)->finalize = glyph_item_finalize;
}

GlyphItem *
glyph_item_new (Pango2Font *font,
                hb_codepoint_t  glyph)
{
  GlyphItem *item = g_object_new (GLYPH_ITEM_TYPE, NULL);
  item->font = g_object_ref (font);
  item->glyph = glyph;
  return item;
}

Pango2Font *
glyph_item_get_font (GlyphItem *item)
{
  return item->font;
}

hb_codepoint_t
glyph_item_get_glyph (GlyphItem *item)
{
  return item->glyph;
}
