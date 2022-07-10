#include "styleview.h"
#include "glyphitem.h"
#include "glyphmodel.h"
#include "glyphview.h"
#include <gtk/gtk.h>

#include <hb-ot.h>

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
  PROP_HAS_STYLES,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _StyleView
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
  gboolean has_styles;
};

struct _StyleViewClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE(StyleView, style_view, GTK_TYPE_WIDGET);

static void
style_view_init (StyleView *self)
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
  self->has_styles = FALSE;

  gtk_widget_set_layout_manager (GTK_WIDGET (self),
                                 gtk_box_layout_new (GTK_ORIENTATION_VERTICAL));
  gtk_widget_init_template (GTK_WIDGET (self));

  self->bg_provider = gtk_css_provider_new ();
  gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (self->content)),
                                  GTK_STYLE_PROVIDER (self->bg_provider), 800);
}

static void
style_view_dispose (GObject *object)
{
  gtk_widget_clear_template (GTK_WIDGET (object), STYLE_VIEW_TYPE);

  G_OBJECT_CLASS (style_view_parent_class)->dispose (object);
}

static void
style_view_finalize (GObject *object)
{
  StyleView *self = STYLE_VIEW (object);

  g_clear_object (&self->font_map);
  pango2_font_description_free (self->font_desc);
  g_free (self->variations);
  g_free (self->features);
  g_free (self->palette);

  G_OBJECT_CLASS (style_view_parent_class)->finalize (object);
}

static Pango2Font *
get_font (StyleView *self)
{
  Pango2Context *context;
  Pango2FontDescription *desc;
  Pango2Font *font;

  context = pango2_context_new_with_font_map (self->font_map);
  desc = pango2_font_description_copy_static (self->font_desc);
  pango2_font_description_set_variations (desc, self->variations);
  pango2_font_description_set_size (desc, self->size * PANGO2_SCALE);
  font = pango2_context_load_font (context, desc);
  pango2_font_description_free (desc);
  g_object_unref (context);

  return font;
}

static void
update_has_styles (StyleView *self)
{
  Pango2Font *font = get_font (self);
  hb_face_t *hb_face = hb_font_get_face (pango2_font_get_hb_font (font));
  gboolean has_styles;

  has_styles = hb_ot_var_get_named_instance_count (hb_face) > 0;
  g_object_unref (font);

  if (self->has_styles == has_styles)
    return;

  self->has_styles = has_styles;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HAS_STYLES]);
}

static void
update_view (StyleView *self)
{
  Pango2FontDescription *desc;
  Pango2AttrList *attrs;
  char *fg, *bg, *css;
  GString *str;
  int start, end, text_len;
  Pango2Font *font = get_font (self);
  hb_face_t *hb_face = hb_font_get_face (pango2_font_get_hb_font (font));
  unsigned int n_axes;
  float *coords = NULL;
  hb_ot_var_axis_info_t *axes;

  desc = pango2_font_description_copy_static (self->font_desc);
  pango2_font_description_set_variations (desc, self->variations);
  pango2_font_description_set_size (desc, self->size * PANGO2_SCALE);

  attrs = pango2_attr_list_new ();
  pango2_attr_list_insert (attrs, pango2_attr_font_desc_new (desc));
  pango2_attr_list_insert (attrs, pango2_attr_letter_spacing_new (self->letterspacing));
  pango2_attr_list_insert (attrs, pango2_attr_line_height_new (self->line_height));
  pango2_attr_list_insert (attrs, pango2_attr_foreground_new (&(Pango2Color){65535 * self->foreground.red,
                                                                             65535 * self->foreground.green,
                                                                             65535 * self->foreground.blue,
                                                                             65535 * self->foreground.alpha}));
  pango2_attr_list_insert (attrs, pango2_attr_font_features_new (self->features));
  pango2_attr_list_insert (attrs, pango2_attr_palette_new (self->palette));

  str = g_string_new ("");
  start = 0;
  text_len = strlen (self->sample_text);

  n_axes = hb_ot_var_get_axis_count (hb_face);
  axes = g_newa (hb_ot_var_axis_info_t, n_axes);
  hb_ot_var_get_axis_infos (hb_face, 0, &n_axes, axes);
  coords = g_newa (float, n_axes);

  for (int i = 0; i < hb_ot_var_get_named_instance_count (hb_face); i++)
    {
      unsigned int n_coords = n_axes;
      Pango2Attribute *attr;
      GString *variations;

      hb_ot_var_named_instance_get_design_coords (hb_face, i, &n_coords, coords);
      variations = g_string_new ("");
      for (int j = 0; j < n_axes; j++)
        {
          char buf[5] = { 0, };

          if (variations->len > 0)
            g_string_append_c (variations, ',');
          hb_tag_to_string (axes[j].tag, buf);
          g_string_append_printf (variations, "%s=%f", buf, coords[j]);
        }

      g_string_append (str, self->sample_text);
      g_string_append (str, " "); /* Unicode line separator */
      end = start + text_len + strlen (" ");

      pango2_font_description_set_variations (desc, variations->str);
      g_string_free (variations, TRUE);

      attr = pango2_attr_font_desc_new (desc);
      pango2_attribute_set_range (attr, start, end);
      pango2_attr_list_insert (attrs, attr);
      start = end;
    }

  gtk_label_set_text (self->content, str->str);
  gtk_label_set_attributes (self->content, attrs);
  g_string_free (str, TRUE);

  pango2_attr_list_unref (attrs);
  pango2_font_description_free (desc);

  fg = gdk_rgba_to_string (&self->foreground);
  bg = gdk_rgba_to_string (&self->background);
  css = g_strdup_printf (".content { caret-color: %s; background-color: %s; }", fg, bg);
  gtk_css_provider_load_from_data (self->bg_provider, css, strlen (css));
  g_free (css);
  g_free (fg);
  g_free (bg);

  g_object_unref (font);
}

static void
style_view_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  StyleView *self = STYLE_VIEW (object);

  switch (prop_id)
    {
    case PROP_FONT_MAP:
      g_set_object (&self->font_map, g_value_get_object (value));
      gtk_widget_set_font_map (GTK_WIDGET (self->content), self->font_map);
      update_has_styles (self);
      break;

    case PROP_FONT_DESC:
      pango2_font_description_free (self->font_desc);
      self->font_desc = pango2_font_description_copy (g_value_get_boxed (value));
      update_has_styles (self);
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
style_view_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  StyleView *self = STYLE_VIEW (object);

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

    case PROP_HAS_STYLES:
      g_value_set_boolean (value, self->has_styles);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
style_view_class_init (StyleViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = style_view_dispose;
  object_class->finalize = style_view_finalize;
  object_class->get_property = style_view_get_property;
  object_class->set_property = style_view_set_property;

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

  properties[PROP_HAS_STYLES] =
      g_param_spec_boolean ("has-styles", "", "",
                            FALSE,
                            G_PARAM_READABLE);

  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/fontexplorer/styleview.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), StyleView, swin);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), StyleView, content);

  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "styleview");
}
