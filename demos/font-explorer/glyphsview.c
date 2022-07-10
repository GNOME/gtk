#include "glyphsview.h"
#include "glyphitem.h"
#include "glyphmodel.h"
#include "glyphview.h"
#include <gtk/gtk.h>

#include <hb-ot.h>

enum {
  PROP_FONT_MAP = 1,
  PROP_FONT_DESC,
  PROP_VARIATIONS,
  PROP_PALETTE,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _GlyphsView
{
  GtkWidget parent;

  Pango2FontMap *font_map;
  GtkGridView *glyphs;

  Pango2FontDescription *font_desc;
  char *variations;
  char *palette;
  GQuark palette_quark;
};

struct _GlyphsViewClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE(GlyphsView, glyphs_view, GTK_TYPE_WIDGET);

static void
glyphs_view_init (GlyphsView *self)
{
  self->font_map = g_object_ref (pango2_font_map_get_default ());
  self->font_desc = pango2_font_description_from_string ("sans 12");
  self->variations = g_strdup ("");
  self->palette = g_strdup (PANGO2_COLOR_PALETTE_DEFAULT);
  self->palette_quark = g_quark_from_string (self->palette);

  gtk_widget_set_layout_manager (GTK_WIDGET (self),
                                 gtk_box_layout_new (GTK_ORIENTATION_VERTICAL));

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glyphs_view_dispose (GObject *object)
{
  gtk_widget_clear_template (GTK_WIDGET (object), GLYPHS_VIEW_TYPE);

  G_OBJECT_CLASS (glyphs_view_parent_class)->dispose (object);
}

static void
glyphs_view_finalize (GObject *object)
{
  GlyphsView *self = GLYPHS_VIEW (object);

  g_clear_object (&self->font_map);
  pango2_font_description_free (self->font_desc);
  g_free (self->variations);
  g_free (self->palette);

  G_OBJECT_CLASS (glyphs_view_parent_class)->finalize (object);
}

static Pango2Font *
get_font (GlyphsView *self,
          int       size)
{
  Pango2Context *context;
  Pango2FontDescription *desc;
  Pango2Font *font;

  context = pango2_context_new_with_font_map (self->font_map);
  desc = pango2_font_description_copy_static (self->font_desc);
  pango2_font_description_set_variations (desc, self->variations);
  pango2_font_description_set_size (desc, size);
  font = pango2_context_load_font (context, desc);
  pango2_font_description_free (desc);
  g_object_unref (context);

  return font;
}

static void
update_glyph_model (GlyphsView *self)
{
  Pango2Font *font = get_font (self, 60 * PANGO2_SCALE);
  GlyphModel *gm;
  GtkSelectionModel *model;

  gm = glyph_model_new (font);
  model = GTK_SELECTION_MODEL (gtk_no_selection_new (G_LIST_MODEL (gm)));
  gtk_grid_view_set_model (self->glyphs, model);
  g_object_unref (model);
  g_object_unref (font);
}

static void
setup_glyph (GtkSignalListItemFactory *factory,
             GObject                  *listitem)
{
  gtk_list_item_set_child (GTK_LIST_ITEM (listitem), GTK_WIDGET (glyph_view_new ()));
}

static void
bind_glyph (GtkSignalListItemFactory *factory,
            GObject                  *listitem,
            GlyphsView                 *self)
{
  GlyphView *view;
  GObject *item;

  view = GLYPH_VIEW (gtk_list_item_get_child (GTK_LIST_ITEM (listitem)));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  glyph_view_set_font (view, glyph_item_get_font (GLYPH_ITEM (item)));
  glyph_view_set_glyph (view, glyph_item_get_glyph (GLYPH_ITEM (item)));
  glyph_view_set_palette (view, self->palette_quark);
}

static void
glyphs_view_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GlyphsView *self = GLYPHS_VIEW (object);

  switch (prop_id)
    {
    case PROP_FONT_MAP:
      g_set_object (&self->font_map, g_value_get_object (value));
      break;

    case PROP_FONT_DESC:
      pango2_font_description_free (self->font_desc);
      self->font_desc = pango2_font_description_copy (g_value_get_boxed (value));
      break;

    case PROP_VARIATIONS:
      g_free (self->variations);
      self->variations = g_strdup (g_value_get_string (value));
      break;

    case PROP_PALETTE:
      g_free (self->palette);
      self->palette = g_strdup (g_value_get_string (value));
      self->palette_quark = g_quark_from_string (self->palette);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }

  update_glyph_model (self);
}

static void
glyphs_view_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GlyphsView *self = GLYPHS_VIEW (object);

  switch (prop_id)
    {
    case PROP_FONT_MAP:
      g_value_set_object (value, self->font_map);
      break;

    case PROP_FONT_DESC:
      g_value_set_boxed (value, self->font_desc);
      break;

    case PROP_VARIATIONS:
      g_value_set_string (value, self->variations);
      break;

    case PROP_PALETTE:
      g_value_set_string (value, self->palette);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
glyphs_view_class_init (GlyphsViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = glyphs_view_dispose;
  object_class->finalize = glyphs_view_finalize;
  object_class->get_property = glyphs_view_get_property;
  object_class->set_property = glyphs_view_set_property;

  properties[PROP_FONT_MAP] =
      g_param_spec_object ("font-map", "", "",
                           PANGO2_TYPE_FONT_MAP,
                           G_PARAM_READWRITE);

  properties[PROP_FONT_DESC] =
      g_param_spec_boxed ("font-desc", "", "",
                          PANGO2_TYPE_FONT_DESCRIPTION,
                          G_PARAM_READWRITE);

  properties[PROP_VARIATIONS] =
      g_param_spec_string ("variations", "", "",
                           "",
                           G_PARAM_READWRITE);

  properties[PROP_PALETTE] =
      g_param_spec_string ("palette", "", "",
                           PANGO2_COLOR_PALETTE_DEFAULT,
                           G_PARAM_READWRITE);

  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/fontexplorer/glyphsview.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), GlyphsView, glyphs);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), setup_glyph);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), bind_glyph);

  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "glyphsview");
}
