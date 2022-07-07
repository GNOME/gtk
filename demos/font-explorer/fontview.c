#include "fontview.h"
#include "glyphitem.h"
#include "glyphmodel.h"
#include "glyphview.h"
#include <gtk/gtk.h>

enum {
  PROP_FONT_DESC = 1,
  PROP_SIZE,
  PROP_LETTERSPACING,
  PROP_LINE_HEIGHT,
  PROP_FOREGROUND,
  PROP_BACKGROUND,
  PROP_VARIATIONS,
  PROP_FEATURES,
  PROP_PALETTE,
  PROP_SAMPLE_TEXT,
  PROP_IGNORE_SIZE,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _FontView
{
  GtkWidget parent;

  GtkStack *stack;
  GtkTextView *edit;
  GtkLabel *content;
  GtkScrolledWindow *swin;
  GtkGridView *glyphs;
  GtkToggleButton *glyphs_toggle;

  Pango2FontDescription *font_desc;
  float size;
  char *variations;
  char *features;
  char *palette;
  int letterspacing;
  float line_height;
  GdkRGBA foreground;
  GdkRGBA background;
  GtkCssProvider *bg_provider;
  char *sample_text;
  gboolean do_waterfall;
};

struct _FontViewClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE(FontView, font_view, GTK_TYPE_WIDGET);

static void
font_view_init (FontView *self)
{
  self->font_desc = pango2_font_description_from_string ("sans 12");
  self->size = 12.;
  self->letterspacing = 0;
  self->line_height = 1.;
  self->variations = g_strdup ("");
  self->features = g_strdup ("");
  self->palette = g_strdup (PANGO2_COLOR_PALETTE_DEFAULT);
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
font_view_dispose (GObject *object)
{
  gtk_widget_clear_template (GTK_WIDGET (object), FONT_VIEW_TYPE);

  G_OBJECT_CLASS (font_view_parent_class)->dispose (object);
}

static void
font_view_finalize (GObject *object)
{
  FontView *self = FONT_VIEW (object);

  pango2_font_description_free (self->font_desc);
  g_free (self->variations);
  g_free (self->features);
  g_free (self->palette);

  G_OBJECT_CLASS (font_view_parent_class)->finalize (object);
}

static void
update_view (FontView *self)
{
  Pango2FontDescription *desc;
  Pango2AttrList *attrs;
  char *fg, *bg, *css;

  desc = pango2_font_description_copy_static (self->font_desc);
  pango2_font_description_set_size (desc, 12 * PANGO2_SCALE);
  pango2_font_description_set_variations (desc, self->variations);

  attrs = pango2_attr_list_new ();
  pango2_attr_list_insert (attrs, pango2_attr_font_desc_new (desc));
  pango2_attr_list_insert (attrs, pango2_attr_size_new (self->size * PANGO2_SCALE));
  pango2_attr_list_insert (attrs, pango2_attr_letter_spacing_new (self->letterspacing));
  pango2_attr_list_insert (attrs, pango2_attr_line_height_new (self->line_height));
  pango2_attr_list_insert (attrs, pango2_attr_foreground_new (&(Pango2Color){65535 * self->foreground.red,
                                                                             65535 * self->foreground.green,
                                                                             65535 * self->foreground.blue,
                                                                             65535 * self->foreground.alpha}));
  pango2_attr_list_insert (attrs, pango2_attr_font_features_new (self->features));
  pango2_attr_list_insert (attrs, pango2_attr_palette_new (self->palette));

  pango2_font_description_free (desc);

  gtk_scrolled_window_set_policy (self->swin,
                                  self->do_waterfall ? GTK_POLICY_AUTOMATIC : GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_label_set_wrap (self->content, !self->do_waterfall);

  if (self->do_waterfall)
    {
      GString *str;
      int sizes[] = { 7, 8, 9, 10, 12, 14, 16, 20, 24, 30, 40, 50, 60, 70, 90 };
      int start, text_len;

      str = g_string_new ("");
      start = 0;
      text_len = strlen (self->sample_text);
      for (int i = 0; i < G_N_ELEMENTS (sizes); i++)
        {
          Pango2Attribute *attr;

          g_string_append (str, self->sample_text);
          g_string_append (str, " "); /* Unicode line separator */

          attr = pango2_attr_size_new (sizes[i] * PANGO2_SCALE);
          pango2_attribute_set_range (attr, start, start + text_len);
          pango2_attr_list_insert (attrs, attr);
          start += text_len + strlen (" ");
        }
      gtk_label_set_text (self->content, str->str);
      gtk_label_set_attributes (self->content, attrs);
      g_string_free (str, TRUE);
    }
  else
    {
      gtk_label_set_label (self->content, self->sample_text);
      gtk_label_set_attributes (self->content, attrs);
    }

  pango2_attr_list_unref (attrs);

  fg = gdk_rgba_to_string (&self->foreground);
  bg = gdk_rgba_to_string (&self->background);
  css = g_strdup_printf (".view_background { caret-color: %s; background-color: %s; }", fg, bg);
  gtk_css_provider_load_from_data (self->bg_provider, css, strlen (css));
  g_free (css);
  g_free (fg);
  g_free (bg);
}

static void
toggle_edit (GtkToggleButton *button,
             FontView        *self)
{
  GtkTextBuffer *buffer;

  buffer = gtk_text_view_get_buffer (self->edit);

  if (gtk_toggle_button_get_active (button))
    {
      gtk_text_buffer_set_text (buffer, self->sample_text, -1);
      gtk_stack_set_visible_child_name (self->stack, "edit");
      gtk_widget_grab_focus (GTK_WIDGET (self->edit));
    }
  else
    {
      GtkTextIter start, end;

      g_free (self->sample_text);
      gtk_text_buffer_get_bounds (buffer, &start, &end);
      self->sample_text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

      update_view (self);

      if (gtk_toggle_button_get_active (self->glyphs_toggle))
        gtk_stack_set_visible_child_name (self->stack, "glyphs");
      else
        gtk_stack_set_visible_child_name (self->stack, "content");

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SAMPLE_TEXT]);
    }
}

static void
plain_changed (GtkToggleButton *button,
               GParamSpec      *pspec,
               FontView        *self)
{
  if (gtk_toggle_button_get_active (button))
    {
      gtk_stack_set_visible_child_name (self->stack, "content");
      self->do_waterfall = FALSE;
    }

  update_view (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_IGNORE_SIZE]);
}

static void
waterfall_changed (GtkToggleButton *button,
                   GParamSpec      *pspec,
                   FontView        *self)
{
  if (gtk_toggle_button_get_active (button))
    {
      gtk_stack_set_visible_child_name (self->stack, "content");
      self->do_waterfall = TRUE;
    }

  update_view (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_IGNORE_SIZE]);
}

static void
glyphs_changed (GtkToggleButton *button,
                GParamSpec      *pspec,
                FontView        *self)
{
  if (gtk_toggle_button_get_active (button))
    {
      gtk_stack_set_visible_child_name (self->stack, "glyphs");
      self->do_waterfall = FALSE;
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_IGNORE_SIZE]);
}

static Pango2Font *
get_font (FontView *self)
{
  Pango2Context *context;

  context = gtk_widget_get_pango_context (GTK_WIDGET (self));
  return pango2_context_load_font (context, self->font_desc);
}

static void
update_glyph_model (FontView *self)
{
  Pango2Font *font = get_font (self);
  GlyphModel *gm;
  GtkSelectionModel *model;

  gm = glyph_model_new (pango2_font_get_face (font));
  model = GTK_SELECTION_MODEL (gtk_no_selection_new (G_LIST_MODEL (gm)));
  gtk_grid_view_set_model (self->glyphs, model);
  g_object_unref (model);
}

static void
setup_glyph (GtkSignalListItemFactory *factory,
             GObject                  *listitem)
{
  gtk_list_item_set_child (GTK_LIST_ITEM (listitem), GTK_WIDGET (glyph_view_new ()));
}

static void
bind_glyph (GtkSignalListItemFactory *factory,
            GObject                  *listitem)
{
  GlyphView *view;
  GObject *item;

  view = GLYPH_VIEW (gtk_list_item_get_child (GTK_LIST_ITEM (listitem)));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  glyph_view_set_font (view, glyph_item_get_font (GLYPH_ITEM (item)));
  glyph_view_set_glyph (view, glyph_item_get_glyph (GLYPH_ITEM (item)));
}

static void
unbind_glyph (GtkSignalListItemFactory *factory,
              GObject                  *listitem)
{
}

static void
font_view_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  FontView *self = FONT_VIEW (object);

  switch (prop_id)
    {
    case PROP_FONT_DESC:
      pango2_font_description_free (self->font_desc);
      self->font_desc = pango2_font_description_copy (g_value_get_boxed (value));
      update_glyph_model (self);
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
font_view_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  FontView *self = FONT_VIEW (object);

  switch (prop_id)
    {
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

    case PROP_IGNORE_SIZE:
      g_value_set_boolean (value, self->do_waterfall);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
font_view_class_init (FontViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = font_view_dispose;
  object_class->finalize = font_view_finalize;
  object_class->get_property = font_view_get_property;
  object_class->set_property = font_view_set_property;

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

  properties[PROP_IGNORE_SIZE] =
      g_param_spec_boolean ("ignore-size", "", "",
                            FALSE,
                            G_PARAM_READWRITE);


  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/fontexplorer/fontview.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontView, swin);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontView, content);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontView, stack);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontView, edit);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontView, glyphs);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontView, glyphs_toggle);

  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), toggle_edit);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), plain_changed);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), waterfall_changed);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), glyphs_changed);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), setup_glyph);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), bind_glyph);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), unbind_glyph);

  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "fontview");
}

FontView *
font_view_new (void)
{
  return g_object_new (FONT_VIEW_TYPE, NULL);
}
