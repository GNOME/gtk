#include "glyphmodel.h"
#include "glyphitem.h"

struct _GlyphModel {
  GObject parent;
  Pango2Font *font;
  unsigned int num_glyphs;
  GlyphItem **glyphs;
};

struct _GlyphModelClass {
  GObjectClass parent_class;
};

static GType
glyph_model_get_item_type (GListModel *model)
{
  return GLYPH_ITEM_TYPE;
}

static guint
glyph_model_get_n_items (GListModel *model)
{
  GlyphModel *self = GLYPH_MODEL (model);

  return self->num_glyphs;
}

static gpointer
glyph_model_get_item (GListModel *model,
                      guint       position)
{
  GlyphModel *self = GLYPH_MODEL (model);

  if (position >= self->num_glyphs)
    return NULL;

  if (self->glyphs[position] == NULL)
    self->glyphs[position] = glyph_item_new (self->font, position);

  return g_object_ref (self->glyphs[position]);
}

static void
glyph_model_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = glyph_model_get_item_type;
  iface->get_n_items = glyph_model_get_n_items;
  iface->get_item = glyph_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GlyphModel, glyph_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, glyph_model_list_model_init))

static void
glyph_model_init (GlyphModel *self)
{
}

static void
glyph_model_finalize (GObject *object)
{
  GlyphModel *self = GLYPH_MODEL (object);

  g_object_unref (self->font);
  for (int i = 0; i < self->num_glyphs; i++)
    {
      if (self->glyphs[i])
        g_object_unref (self->glyphs[i]);
    }
  g_free (self->glyphs);

  G_OBJECT_CLASS (glyph_model_parent_class)->finalize (object);
}

static void
glyph_model_class_init (GlyphModelClass *class)
{
  G_OBJECT_CLASS (class)->finalize = glyph_model_finalize;
}

GlyphModel *
glyph_model_new (Pango2Font *font)
{
  GlyphModel *self;
  hb_face_t *hb_face;

  self = g_object_new (GLYPH_MODEL_TYPE, NULL);

  self->font = g_object_ref (font);
  hb_face = pango2_hb_face_get_hb_face (PANGO2_HB_FACE (pango2_font_get_face (font)));
  self->num_glyphs = hb_face_get_glyph_count (hb_face);
  self->glyphs = g_new0 (GlyphItem *, self->num_glyphs);

  return self;
}
