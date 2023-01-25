#include "config.h"

#include "glyphmodel.h"

#include <gtk/gtk.h>
#include <hb-ot.h>
#include <hb-gobject.h>

enum {
  PROP_FACE = 1,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _GlyphModel
{
  GObject parent;

  hb_face_t *face;
  GPtrArray *items;
  unsigned int n_items;
};

struct _GlyphModelClass
{
  GObjectClass parent_class;
};

static void
update_n_items (GlyphModel *self)
{
  unsigned int n_items = 0;
  unsigned int n_removed;

  if (self->face)
    n_items = hb_face_get_glyph_count (self->face);

  g_clear_pointer (&self->items, g_ptr_array_unref);
  self->items = g_ptr_array_new_with_free_func (g_object_unref);

  for (unsigned int i = 0; i < n_items; i++)
    {
      GdkPaintable *item = gtk_glyph_paintable_new (self->face);
      gtk_glyph_paintable_set_glyph (GTK_GLYPH_PAINTABLE (item), i);
      g_ptr_array_add (self->items, item);
    }

  n_removed = self->n_items;
  self->n_items = n_items;
  g_list_model_items_changed (G_LIST_MODEL (self), 0, n_removed, n_items);
}

void
glyph_model_set_face (GlyphModel *self,
                      hb_face_t  *face)
{
  if (self->face)
    hb_face_destroy (self->face);
  self->face = face;
  if (self->face)
    hb_face_reference (self->face);

  update_n_items (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACE]);
}

static GType
glyph_list_model_get_item_type (GListModel *model)
{
  return GTK_TYPE_GLYPH_PAINTABLE;
}

static guint
glyph_list_model_get_n_items (GListModel *model)
{
  GlyphModel *self = GLYPH_MODEL (model);

  return self->n_items;
}

static gpointer
glyph_list_model_get_item (GListModel *model,
                           guint       position)
{
  GlyphModel *self = GLYPH_MODEL (model);

  if (position >= self->n_items)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->items, position));
}

static void
glyph_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = glyph_list_model_get_item_type;
  iface->get_n_items = glyph_list_model_get_n_items;
  iface->get_item = glyph_list_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GlyphModel, glyph_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, glyph_list_model_init))

static void
glyph_model_init (GlyphModel *self)
{
}

static void
glyph_model_finalize (GObject *object)
{
  GlyphModel *self = GLYPH_MODEL (object);

  g_clear_pointer (&self->items, g_ptr_array_unref);
  g_clear_pointer (&self->face, hb_face_destroy);

  G_OBJECT_CLASS (glyph_model_parent_class)->finalize (object);
}

static void
glyph_model_set_property (GObject      *object,
                          unsigned int  prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GlyphModel *self = GLYPH_MODEL (object);

  switch (prop_id)
    {
    case PROP_FACE:
      glyph_model_set_face (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
glyph_model_get_property (GObject      *object,
                          unsigned int  prop_id,
                          GValue       *value,
                          GParamSpec   *pspec)
{
  GlyphModel *self = GLYPH_MODEL (object);

  switch (prop_id)
    {
    case PROP_FACE:
      g_value_set_object (value, self->face);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
glyph_model_class_init (GlyphModelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = glyph_model_finalize;
  object_class->get_property = glyph_model_get_property;
  object_class->set_property = glyph_model_set_property;

  properties[PROP_FACE] = g_param_spec_boxed ("face", "", "",
                                              HB_GOBJECT_TYPE_FACE,
                                              G_PARAM_READWRITE);

  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);
}
