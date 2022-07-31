#include "waterfallview.h"

#include <gtk/gtk.h>
#include <hb-ot.h>

#include "glyphitem.h"
#include "glyphmodel.h"
#include "glyphview.h"

enum {
  PROP_FONT_MAP = 1,
  PROP_FONT_DESC,
  PROP_SIZE,
  PROP_LETTERSPACING,
  PROP_LINE_HEIGHT,
  PROP_FOREGROUND,
  PROP_BACKGROUND,
  PROP_VARIATIONS,
  PROP_FEATURES,
  PROP_PALETTE,
  PROP_SAMPLE_TEXT,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _WaterfallView
{
  GtkWidget parent;

  GtkLabel *content;
  GtkScrolledWindow *swin;

  Pango2FontMap *font_map;
  Pango2FontDescription *font_desc;
  float size;
  char *variations;
  char *features;
  char *palette;
  GQuark palette_quark;
  int letterspacing;
  float line_height;
  GdkRGBA foreground;
  GdkRGBA background;
  GtkCssProvider *bg_provider;
  char *sample_text;
};

struct _WaterfallViewClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE(WaterfallView, waterfall_view, GTK_TYPE_WIDGET);

static void
waterfall_view_init (WaterfallView *self)
{
  self->font_map = g_object_ref (pango2_font_map_get_default ());
  self->font_desc = pango2_font_description_from_string ("sans 12");
  self->size = 12.;
  self->letterspacing = 0;
  self->line_height = 1.;
  self->variations = g_strdup ("");
  self->features = g_strdup ("");
  self->palette = g_strdup (PANGO2_COLOR_PALETTE_DEFAULT);
  self->palette_quark = g_quark_from_string (self->palette);
  self->foreground = (GdkRGBA){0., 0., 0., 1. };
  self->background = (GdkRGBA){1., 1., 1., 1. };
  self->sample_text = g_strdup ("Some sample text is better than other sample text");

  gtk_widget_set_layout_manager (GTK_WIDGET (self),
                                 gtk_box_layout_new (GTK_ORIENTATION_VERTICAL));
  gtk_widget_init_template (GTK_WIDGET (self));

  self->bg_provider = gtk_css_provider_new ();
  gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (self->content)),
                                  GTK_STYLE_PROVIDER (self->bg_provider), 800);
}

static void
waterfall_view_dispose (GObject *object)
{
  gtk_widget_clear_template (GTK_WIDGET (object), WATERFALL_VIEW_TYPE);

  G_OBJECT_CLASS (waterfall_view_parent_class)->dispose (object);
}

static void
waterfall_view_finalize (GObject *object)
{
  WaterfallView *self = WATERFALL_VIEW (object);

  g_clear_object (&self->font_map);
  pango2_font_description_free (self->font_desc);
  g_free (self->variations);
  g_free (self->features);
  g_free (self->palette);

  G_OBJECT_CLASS (waterfall_view_parent_class)->finalize (object);
}

static void
update_view (WaterfallView *self)
{
  Pango2FontDescription *desc;
  Pango2AttrList *attrs;
  char *fg, *bg, *css;
  GString *str;
  int sizes[] = { 7, 8, 9, 10, 12, 14, 16, 20, 24, 30, 40, 50, 60, 70, 90, 120 };
  int start, end, text_len;
  Pango2TabArray *tabs;

  desc = pango2_font_description_copy_static (self->font_desc);
  pango2_font_description_set_size (desc, 12 * PANGO2_SCALE);
  pango2_font_description_set_variations (desc, self->variations);

  attrs = pango2_attr_list_new ();
  pango2_attr_list_insert (attrs, pango2_attr_font_desc_new (desc));
  pango2_attr_list_insert (attrs, pango2_attr_size_new (self->size * PANGO2_SCALE));
  pango2_attr_list_insert (attrs, pango2_attr_line_height_new (self->line_height));
  pango2_attr_list_insert (attrs, pango2_attr_letter_spacing_new (self->letterspacing));
  pango2_attr_list_insert (attrs, pango2_attr_foreground_new (&(Pango2Color){65535 * self->foreground.red,
                                                                             65535 * self->foreground.green,
                                                                             65535 * self->foreground.blue,
                                                                             65535 * self->foreground.alpha}));
  pango2_attr_list_insert (attrs, pango2_attr_font_features_new (self->features));
  pango2_attr_list_insert (attrs, pango2_attr_palette_new (self->palette));

  pango2_font_description_free (desc);

  tabs = pango2_tab_array_new (2, PANGO2_TAB_POSITIONS_SPACES);
  pango2_tab_array_set_tab (tabs, 0, PANGO2_TAB_RIGHT, 5);
  pango2_tab_array_set_tab (tabs, 1, PANGO2_TAB_LEFT, 8);

  gtk_label_set_tabs (self->content, tabs);
  pango2_tab_array_free (tabs);

  str = g_string_new ("");
  start = 0;
  text_len = strlen (self->sample_text);
  for (int i = 0; i < G_N_ELEMENTS (sizes); i++)
    {
      Pango2Attribute *attr;

      g_string_append_printf (str, "\t%d\t", sizes[i]);

      end = str->len;

      attr = pango2_attr_family_new ("Cantarell");
      pango2_attribute_set_range (attr, start, end);
      pango2_attr_list_insert (attrs, attr);

      attr = pango2_attr_weight_new (PANGO2_WEIGHT_NORMAL);
      pango2_attribute_set_range (attr, start, end);
      pango2_attr_list_insert (attrs, attr);

      attr = pango2_attr_style_new (PANGO2_STYLE_NORMAL);
      pango2_attribute_set_range (attr, start, end);
      pango2_attr_list_insert (attrs, attr);

      attr = pango2_attr_size_new (12 * PANGO2_SCALE);
      pango2_attribute_set_range (attr, start, end);
      pango2_attr_list_insert (attrs, attr);

      attr = pango2_attr_font_features_new ("tnum=1");
      pango2_attribute_set_range (attr, start, end);
      pango2_attr_list_insert (attrs, attr);

      attr = pango2_attr_letter_spacing_new (0);
      pango2_attribute_set_range (attr, start, end);
      pango2_attr_list_insert (attrs, attr);
      start = end;

      g_string_append (str, self->sample_text);
      g_string_append (str, " "); /* Unicode line separator */
      end = start + text_len + strlen (" ");

      attr = pango2_attr_size_new (sizes[i] * PANGO2_SCALE);
      pango2_attribute_set_range (attr, start, end);
      pango2_attr_list_insert (attrs, attr);

      start = end;
    }
  gtk_label_set_text (self->content, str->str);
  gtk_label_set_attributes (self->content, attrs);
  g_string_free (str, TRUE);

  pango2_attr_list_unref (attrs);

  fg = gdk_rgba_to_string (&self->foreground);
  bg = gdk_rgba_to_string (&self->background);
  css = g_strdup_printf (".content { caret-color: %s; background-color: %s; }", fg, bg);
  gtk_css_provider_load_from_data (self->bg_provider, css, strlen (css));
  g_free (css);
  g_free (fg);
  g_free (bg);
}

static void
waterfall_view_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  WaterfallView *self = WATERFALL_VIEW (object);

  switch (prop_id)
    {
    case PROP_FONT_MAP:
      g_set_object (&self->font_map, g_value_get_object (value));
      gtk_widget_set_font_map (GTK_WIDGET (self->content), self->font_map);
      break;

    case PROP_FONT_DESC:
      pango2_font_description_free (self->font_desc);
      self->font_desc = pango2_font_description_copy (g_value_get_boxed (value));
      break;

    case PROP_SIZE:
      self->size = g_value_get_float (value);
      break;

    case PROP_LETTERSPACING:
      self->letterspacing = g_value_get_int (value);
      break;

    case PROP_LINE_HEIGHT:
      self->line_height = g_value_get_float (value);
      break;

    case PROP_FOREGROUND:
      self->foreground = *(GdkRGBA *)g_value_get_boxed (value);
      break;

    case PROP_BACKGROUND:
      self->background = *(GdkRGBA *)g_value_get_boxed (value);
      break;

    case PROP_VARIATIONS:
      g_free (self->variations);
      self->variations = g_strdup (g_value_get_string (value));
      break;

    case PROP_FEATURES:
      g_free (self->features);
      self->features = g_strdup (g_value_get_string (value));
      break;

    case PROP_PALETTE:
      g_free (self->palette);
      self->palette = g_strdup (g_value_get_string (value));
      self->palette_quark = g_quark_from_string (self->palette);
      break;

    case PROP_SAMPLE_TEXT:
      g_free (self->sample_text);
      self->sample_text = g_strdup (g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }

  update_view (self);
}

static void
waterfall_view_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  WaterfallView *self = WATERFALL_VIEW (object);

  switch (prop_id)
    {
    case PROP_FONT_MAP:
      g_value_set_object (value, self->font_map);
      break;

    case PROP_FONT_DESC:
      g_value_set_boxed (value, self->font_desc);
      break;

    case PROP_SIZE:
      g_value_set_float (value, self->size);
      break;

    case PROP_LETTERSPACING:
      g_value_set_int (value, self->letterspacing);
      break;

    case PROP_LINE_HEIGHT:
      g_value_set_float (value, self->line_height);
      break;

    case PROP_FOREGROUND:
      g_value_set_boxed (value, &self->foreground);
      break;

    case PROP_BACKGROUND:
      g_value_set_boxed (value, &self->background);
      break;

    case PROP_VARIATIONS:
      g_value_set_string (value, self->variations);
      break;

    case PROP_FEATURES:
      g_value_set_string (value, self->features);
      break;

    case PROP_PALETTE:
      g_value_set_string (value, self->palette);
      break;

    case PROP_SAMPLE_TEXT:
      g_value_set_string (value, self->sample_text);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
waterfall_view_class_init (WaterfallViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = waterfall_view_dispose;
  object_class->finalize = waterfall_view_finalize;
  object_class->get_property = waterfall_view_get_property;
  object_class->set_property = waterfall_view_set_property;

  properties[PROP_FONT_MAP] =
      g_param_spec_object ("font-map", "", "",
                           PANGO2_TYPE_FONT_MAP,
                           G_PARAM_READWRITE);

  properties[PROP_FONT_DESC] =
      g_param_spec_boxed ("font-desc", "", "",
                          PANGO2_TYPE_FONT_DESCRIPTION,
                          G_PARAM_READWRITE);

  properties[PROP_SIZE] =
      g_param_spec_float ("size", "", "",
                          0., 100., 12.,
                          G_PARAM_READWRITE);

  properties[PROP_LETTERSPACING] =
      g_param_spec_int ("letterspacing", "", "",
                        -G_MAXINT, G_MAXINT, 0,
                        G_PARAM_READWRITE);

  properties[PROP_LINE_HEIGHT] =
      g_param_spec_float ("line-height", "", "",
                          0., 100., 1.,
                          G_PARAM_READWRITE);

  properties[PROP_FOREGROUND] =
      g_param_spec_boxed ("foreground", "", "",
                          GDK_TYPE_RGBA,
                          G_PARAM_READWRITE);

  properties[PROP_BACKGROUND] =
      g_param_spec_boxed ("background", "", "",
                          GDK_TYPE_RGBA,
                          G_PARAM_READWRITE);

  properties[PROP_VARIATIONS] =
      g_param_spec_string ("variations", "", "",
                           "",
                           G_PARAM_READWRITE);

  properties[PROP_FEATURES] =
      g_param_spec_string ("features", "", "",
                           "",
                           G_PARAM_READWRITE);

  properties[PROP_PALETTE] =
      g_param_spec_string ("palette", "", "",
                           PANGO2_COLOR_PALETTE_DEFAULT,
                           G_PARAM_READWRITE);

  properties[PROP_SAMPLE_TEXT] =
      g_param_spec_string ("sample-text", "", "",
                           "",
                           G_PARAM_READWRITE);

  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/fontexplorer/waterfallview.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), WaterfallView, swin);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), WaterfallView, content);

  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "waterfallview");
}
