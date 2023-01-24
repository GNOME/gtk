#include "config.h"

#include "fontcolors.h"

#include <gtk/gtk.h>
#include <hb-ot.h>
#include <hb-gobject.h>

enum {
  PROP_FACE = 1,
  PROP_PALETTE_INDEX,
  PROP_CUSTOM_COLORS,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _FontColors
{
  GtkWidget parent;

  GtkGrid *label;
  GtkGrid *grid;
  GSimpleAction *reset_action;
  gboolean has_colors;
  unsigned int palette_index;
  GtkCheckButton *default_check;
  GList *custom_colors;
  gboolean has_custom_colors;

  hb_face_t *face;
};

struct _FontColorsClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (FontColors, font_colors, GTK_TYPE_WIDGET);

static void
palette_changed (GtkCheckButton *button,
                 FontColors     *self)
{
  self->palette_index = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), "palette"));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PALETTE_INDEX]);
  g_simple_action_set_enabled (self->reset_action,
                               (self->palette_index != 0) || self->has_custom_colors);
}

static char *
get_name (hb_face_t       *face,
          hb_ot_name_id_t  name_id)
{
  char *info;
  unsigned int len;

  len = hb_ot_name_get_utf8 (face, name_id, HB_LANGUAGE_INVALID, NULL, NULL);
  len++;
  info = g_new (char, len);
  hb_ot_name_get_utf8 (face, name_id, HB_LANGUAGE_INVALID, &len, info);

  return info;
}

static void
custom_color_changed (GtkColorButton *button,
                      GParamSpec     *pspec,
                      GtkWidget      *swatch)
{
  GdkRGBA rgba;
  FontColors *self;

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &rgba);
  g_object_set (swatch, "rgba", &rgba, NULL);

  self = FONT_COLORS (gtk_widget_get_ancestor (swatch, FONT_COLORS_TYPE));
  self->has_custom_colors = TRUE;
  g_simple_action_set_enabled (self->reset_action, TRUE);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CUSTOM_COLORS]);
}

static char *
get_custom_colors (FontColors *self)
{
  GString *s;
  GList *l;

  if (!self->has_custom_colors)
    return NULL;

  s = g_string_new ("");

  for (l = self->custom_colors; l; l = l->next)
    {
      GtkWidget *button = l->data;
      const GdkRGBA rgba;

      if (l != self->custom_colors)
        g_string_append (s, ",");

      gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &rgba);

      if (rgba.red != 0 || rgba.green != 0 || rgba.blue != 0 || rgba.alpha != 0)
        g_string_append_printf (s, "%.2x%.2x%.2x%.2x",
                                (unsigned int)(rgba.red * 255),
                                (unsigned int)(rgba.green * 255),
                                (unsigned int)(rgba.blue * 255),
                                (unsigned int)(rgba.alpha * 255));
    }

  g_print ("%s\n", s->str);

  return g_string_free (s, FALSE);
}

static GtkWidget *
make_palette (unsigned int   n_colors,
              hb_color_t    *colors,
              const char   **names)
{
  GtkWidget *palette;

  palette = gtk_grid_new ();

  /* HACK - defeat first-child/last-child theming */
  gtk_grid_attach (GTK_GRID (palette), gtk_picture_new (), -1, 0, 1, 1);

  for (int k = 0; k < n_colors; k++)
    {
      GtkWidget *swatch;
      swatch = g_object_new (g_type_from_name ("GtkColorSwatch"),
                             "rgba", &(GdkRGBA){ hb_color_get_red (colors[k])/255.,
                                                 hb_color_get_green (colors[k])/255.,
                                                 hb_color_get_blue (colors[k])/255.,
                                                 hb_color_get_alpha (colors[k])/255.},
                             "selectable", FALSE,
                             "has-menu", FALSE,
                             "can-drag", FALSE,
                             "width-request", 16,
                             "height-request", 16,
                             "can-focus", FALSE,
                             NULL);

      if (names[k])
        gtk_widget_set_tooltip_text (swatch, names[k]);

      gtk_grid_attach (GTK_GRID (palette), swatch, k % 6, k / 6, 1, 1);
    }

  /* HACK - defeat first-child/last-child theming */
  gtk_grid_attach (GTK_GRID (palette), gtk_picture_new (), 6, 0, 1, 1);

  return palette;
}

static void
reset_one_color (GtkWidget *button)
{
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (button), &(GdkRGBA) { 0,0,0,0 });
}

static void
update_colors (FontColors *self)
{
  GtkWidget *child;
  GtkWidget *check;
  unsigned int n_palettes;
  unsigned int n_colors;
  hb_color_t *colors;
  char **color_names;
  GtkWidget *palette;
  GList *custom_colors;
  hb_ot_name_id_t name_id;
  char *name;

  self->has_custom_colors = FALSE;

  g_object_ref (self->label);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->grid))))
    gtk_grid_remove (self->grid, child);

  gtk_grid_attach (self->grid, GTK_WIDGET (self->label), 0, -4, 2, 1);
  g_object_unref (self->label);

  self->default_check = NULL;

  self->has_colors = hb_ot_color_has_palettes (self->face);
  gtk_widget_set_visible (GTK_WIDGET (self), self->has_colors);
  if (!self->has_colors)
    {
      g_simple_action_set_enabled (self->reset_action, FALSE);
      return;
    }

  n_palettes = hb_ot_color_palette_get_count (self->face);
  n_colors = hb_ot_color_palette_get_colors (self->face, 0, 0, NULL, NULL);

  colors = g_new (hb_color_t, n_colors);
  color_names = g_new0 (char *, n_colors);
  for (int k = 0; k < n_colors; k++)
    {
      name_id = hb_ot_color_palette_color_get_name_id (self->face, k);
      if (name_id != HB_OT_NAME_ID_INVALID)
        color_names[k] = get_name (self->face, name_id);
    }

  for (int i = 0; i < n_palettes; i++)
    {
      name_id = hb_ot_color_palette_get_name_id (self->face, i);
      if (name_id != HB_OT_NAME_ID_INVALID)
        name = get_name (self->face, name_id);
      else if (i != 0)
        name = g_strdup_printf ("Palette %d", i);
      else
        name = g_strdup ("Default");

      check = gtk_check_button_new_with_label (name);
      g_free (name);

      g_object_set_data (G_OBJECT (check), "palette", GUINT_TO_POINTER (i));
      if (self->palette_index == i)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (check), TRUE);
      g_signal_connect (check, "toggled", G_CALLBACK (palette_changed), self);
      if (i == 0)
        self->default_check = GTK_CHECK_BUTTON (check);
      else
        gtk_check_button_set_group (GTK_CHECK_BUTTON (check), self->default_check);
      gtk_grid_attach (self->grid, check, 0, i, 1, 1);

      hb_ot_color_palette_get_colors (self->face, i, 0, &n_colors, colors);

      palette = make_palette (n_colors, colors, (const char **)color_names);
      gtk_widget_set_valign (palette, GTK_ALIGN_CENTER);
      gtk_grid_attach (self->grid, palette, 1, i, 1, 1);
    }

#ifdef HAVE_CAIRO_FONT_OPTIONS_SET_CUSTOM_PALETTE_COLOR
  {
    GtkWidget *expander;
    GtkWidget *custom_grid;
    GtkWidget *swatch;

    expander = gtk_expander_new ("Overrides");
    gtk_grid_attach (self->grid, expander, 0, n_palettes, 1, 1);

    for (int k = 0; k < n_colors; k++)
      colors[k] = HB_COLOR (0, 0, 0, 0);
    palette = make_palette (n_colors, colors, (const char **)color_names);
    gtk_grid_attach (self->grid, palette, 1, n_palettes, 1, 1);

    g_list_free (self->custom_colors);

    custom_colors = NULL;

    custom_grid = gtk_grid_new ();
    gtk_widget_add_css_class (custom_grid, "custom-colors");
    gtk_widget_set_hexpand (custom_grid, FALSE);
    gtk_grid_attach (GTK_GRID (self->grid), custom_grid, 0, n_palettes + 1, 2, 1);
    g_object_bind_property (expander, "expanded", custom_grid, "visible", G_BINDING_SYNC_CREATE);

    swatch = gtk_widget_get_first_child (palette);
    swatch = gtk_widget_get_next_sibling (swatch);

    for (int k = 0; k < n_colors; k++)
      {
        GtkWidget *label;
        GtkWidget *color_button;
        GtkWidget *reset_button;

        label = gtk_label_new (color_names[k] ? color_names[k] : "");
        gtk_label_set_xalign (label, 0);
        gtk_widget_set_hexpand (label, TRUE);
        gtk_grid_attach (GTK_GRID (custom_grid), label, 0, k, 1, 1);

        color_button = gtk_color_button_new ();
        gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (color_button), TRUE);
        gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (color_button), &(GdkRGBA){ 0,0,0,0 });
        g_signal_connect (color_button, "notify::rgba",
                          G_CALLBACK (custom_color_changed), swatch);

        custom_colors = g_list_prepend (custom_colors, color_button);

        gtk_grid_attach (GTK_GRID (custom_grid), color_button, 1, k, 1, 1);

        reset_button = gtk_button_new_from_icon_name ("view-refresh-symbolic");
        gtk_widget_add_css_class (reset_button, "circular");
        gtk_widget_add_css_class (reset_button, "flat");
        g_signal_connect_swapped (reset_button, "clicked",
                                  G_CALLBACK (reset_one_color), color_button);

        gtk_grid_attach (GTK_GRID (custom_grid), reset_button, 2, k, 1, 1);

        swatch = gtk_widget_get_next_sibling (swatch);
      }

    self->custom_colors = g_list_reverse (custom_colors);
  }
#endif

  g_free (colors);
  g_free (color_names);
}

static void
reset (GSimpleAction *action,
       GVariant      *parameter,
       FontColors   *self)
{
  if (self->has_colors)
    {
      self->palette_index = 0;
      self->has_custom_colors = FALSE;
      gtk_check_button_set_active (self->default_check, TRUE);
      for (GList *l = self->custom_colors; l; l = l->next)
        reset_one_color (GTK_WIDGET (l->data));
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PALETTE_INDEX]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CUSTOM_COLORS]);
  g_simple_action_set_enabled (self->reset_action, FALSE);
}

static void
font_colors_init (FontColors *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->reset_action = g_simple_action_new ("reset", NULL);
  g_simple_action_set_enabled (self->reset_action, FALSE);
  g_signal_connect (self->reset_action, "activate", G_CALLBACK (reset), self);
}

static void
font_colors_dispose (GObject *object)
{
  FontColors *self = FONT_COLORS (object);

  g_list_free (self->custom_colors);

  gtk_widget_dispose_template (GTK_WIDGET (object), FONT_COLORS_TYPE);

  G_OBJECT_CLASS (font_colors_parent_class)->dispose (object);
}

static void
font_colors_finalize (GObject *object)
{
  FontColors *self = FONT_COLORS (object);

  g_clear_pointer (&self->face, hb_face_destroy);

  G_OBJECT_CLASS (font_colors_parent_class)->finalize (object);
}

static void
font_colors_set_property (GObject      *object,
                          unsigned int  prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  FontColors *self = FONT_COLORS (object);

  switch (prop_id)
    {
    case PROP_FACE:
      {
        hb_face_t *face = g_value_get_boxed (value);

        if (self->face == face)
          return;

        if (self->face)
          hb_face_destroy (self->face);
        self->face = face;
        if (self->face)
          hb_face_reference (self->face);

        update_colors (self);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
font_colors_get_property (GObject      *object,
                          unsigned int  prop_id,
                          GValue       *value,
                          GParamSpec   *pspec)
{
  FontColors *self = FONT_COLORS (object);

  switch (prop_id)
    {
    case PROP_FACE:
      g_value_set_boxed (value, self->face);
      break;

    case PROP_PALETTE_INDEX:
      g_value_set_uint (value, self->palette_index);
      break;

    case PROP_CUSTOM_COLORS:
      g_value_take_string (value, get_custom_colors (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
font_colors_class_init (FontColorsClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = font_colors_dispose;
  object_class->finalize = font_colors_finalize;
  object_class->get_property = font_colors_get_property;
  object_class->set_property = font_colors_set_property;

  properties[PROP_FACE] = g_param_spec_boxed ("face", "", "",
                                              HB_GOBJECT_TYPE_FACE,
                                              G_PARAM_READWRITE);

  properties[PROP_PALETTE_INDEX] = g_param_spec_uint ("palette-index", "", "",
                                                      0, G_MAXUINT, 0,
                                                      G_PARAM_READABLE);

  properties[PROP_CUSTOM_COLORS] = g_param_spec_string ("custom-colors", "", "",
                                                        NULL,
                                                        G_PARAM_READABLE);

  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/paintable_glyph/fontcolors.ui");
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontColors, grid);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontColors, label);

  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "fontcolors");
}

GAction *
font_colors_get_reset_action (FontColors *self)
{
  return G_ACTION (self->reset_action);
}
