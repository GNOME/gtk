#include "fontcolors.h"
#include "rangeedit.h"
#include <gtk/gtk.h>
#include <hb-ot.h>

enum {
  PROP_FONT_DESC = 1,
  PROP_PALETTE,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _FontColors
{
  GtkWidget parent;

  GtkGrid *label;
  GtkGrid *grid;
  Pango2FontDescription *font_desc;
  GSimpleAction *reset_action;
  gboolean has_colors;
  char *palette;
  GtkCheckButton *default_check;
};

struct _FontColorsClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (FontColors, font_colors, GTK_TYPE_WIDGET);

static Pango2Font *
get_font (FontColors *self)
{
  Pango2Context *context;

  context = gtk_widget_get_pango_context (GTK_WIDGET (self));
  return pango2_context_load_font (context, self->font_desc);
}

static void
palette_changed (GtkCheckButton *button,
                 FontColors     *self)
{
  g_free (self->palette);
  self->palette = g_strdup ((const char *) g_object_get_data (G_OBJECT (button), "palette"));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PALETTE]);
}

static void
update_colors (FontColors *self)
{
  Pango2Font *font;
  hb_font_t *hb_font;
  hb_face_t *hb_face;
  GtkWidget *child;
  GtkWidget *check;
  unsigned int n_colors;
  hb_color_t *colors;
  GtkWidget *box;

  g_object_ref (self->label);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->grid))))
    gtk_grid_remove (self->grid, child);

  gtk_grid_attach (self->grid, GTK_WIDGET (self->label), 0, -4, 2, 1);
  g_object_unref (self->label);

  self->default_check = NULL;

  font = get_font (self);
  hb_font = pango2_font_get_hb_font (font);
  hb_face = hb_font_get_face (hb_font);

  self->has_colors = hb_ot_color_has_palettes (hb_face);
  gtk_widget_set_visible (GTK_WIDGET (self), self->has_colors);
  if (!self->has_colors)
    {
      g_simple_action_set_enabled (self->reset_action, FALSE);
      return;
    }

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
  gtk_grid_attach (self->grid, box, 0, -3, 2, 1);

  check = gtk_check_button_new_with_label ("Default");
  g_object_set_data (G_OBJECT (check), "palette", (gpointer)"default");
  if (g_strcmp0 ("default", self->palette) == 0)
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check), TRUE);
  g_signal_connect (check, "toggled", G_CALLBACK (palette_changed), self);
  gtk_box_append (GTK_BOX (box), check);
  self->default_check = GTK_CHECK_BUTTON (check);

  check = gtk_check_button_new_with_label ("Light");
  g_object_set_data (G_OBJECT (check), "palette", (gpointer)"light");
  if (g_strcmp0 ("light", self->palette) == 0)
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check), TRUE);
  g_signal_connect (check, "toggled", G_CALLBACK (palette_changed), self);
  gtk_check_button_set_group (GTK_CHECK_BUTTON (check), self->default_check);
  gtk_box_append (GTK_BOX (box), check);

  check = gtk_check_button_new_with_label ("Dark");
  g_object_set_data (G_OBJECT (check), "palette", (gpointer)"dark");
  if (g_strcmp0 ("dark", self->palette) == 0)
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check), TRUE);
  g_signal_connect (check, "toggled", G_CALLBACK (palette_changed), self);
  gtk_check_button_set_group (GTK_CHECK_BUTTON (check), self->default_check);
  gtk_box_append (GTK_BOX (box), check);

  for (int i = 0; i < hb_ot_color_palette_get_count (hb_face); i++)
    {
      char *id = g_strdup_printf ("palette%d", i);
      char *label = g_strdup_printf ("Palette %d", i);
      GtkWidget *palette;

      check = gtk_check_button_new_with_label (label);
      g_object_set_data_full (G_OBJECT (check), "palette", id, g_free);
      if (g_strcmp0 (id, self->palette) == 0)
        gtk_check_button_set_active (GTK_CHECK_BUTTON (check), TRUE);
      g_signal_connect (check, "toggled", G_CALLBACK (palette_changed), self);
      gtk_check_button_set_group (GTK_CHECK_BUTTON (check), self->default_check);
      gtk_grid_attach (self->grid, check, 0, i, 1, 1);

      n_colors = hb_ot_color_palette_get_colors (hb_face, i, 0, NULL, NULL);
      colors = g_new (hb_color_t, n_colors);
      n_colors = hb_ot_color_palette_get_colors (hb_face, i, 0, &n_colors, colors);

      palette = gtk_grid_new ();
      gtk_widget_set_valign (palette, GTK_ALIGN_CENTER);
      gtk_grid_attach (self->grid, palette, 1, i, 1, 1);

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
                                 NULL);
          gtk_grid_attach (GTK_GRID (palette), swatch, k % 6, k / 6, 1, 1);
        }

       /* HACK - defeat first-child/last-child theming */
       gtk_grid_attach (GTK_GRID (palette), gtk_picture_new (), 6, 0, 1, 1);
    }
}

static void
reset (GSimpleAction *action,
       GVariant      *parameter,
       FontColors   *self)
{
  g_free (self->palette);
  self->palette = g_strdup (PANGO2_COLOR_PALETTE_DEFAULT);
  if (self->has_colors)
    gtk_check_button_set_active (self->default_check, TRUE);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PALETTE]);
  g_simple_action_set_enabled (self->reset_action, FALSE);
}

static void
font_colors_init (FontColors *self)
{
  self->palette = g_strdup (PANGO2_COLOR_PALETTE_DEFAULT);

  gtk_widget_init_template (GTK_WIDGET (self));

  self->reset_action = g_simple_action_new ("reset", NULL);
  g_simple_action_set_enabled (self->reset_action, FALSE);
  g_signal_connect (self->reset_action, "activate", G_CALLBACK (reset), self);
}

static void
font_colors_dispose (GObject *object)
{
  gtk_widget_clear_template (GTK_WIDGET (object), FONT_COLORS_TYPE);

  G_OBJECT_CLASS (font_colors_parent_class)->dispose (object);
}

static void
font_colors_finalize (GObject *object)
{
  FontColors *self = FONT_COLORS (object);

  g_clear_pointer (&self->font_desc, pango2_font_description_free);
  g_free (self->palette);

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
    case PROP_FONT_DESC:
      pango2_font_description_free (self->font_desc);
      self->font_desc = pango2_font_description_copy (g_value_get_boxed (value));
      update_colors (self);
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
    case PROP_PALETTE:
      g_value_set_string (value, self->palette);
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

  properties[PROP_FONT_DESC] =
      g_param_spec_boxed ("font-desc", "", "",
                          PANGO2_TYPE_FONT_DESCRIPTION,
                          G_PARAM_WRITABLE);

  properties[PROP_PALETTE] =
      g_param_spec_string ("palette", "", "",
                           PANGO2_COLOR_PALETTE_DEFAULT,
                           G_PARAM_READABLE);

  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/fontexplorer/fontcolors.ui");
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontColors, grid);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontColors, label);

  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "fontcolors");
}

FontColors *
font_colors_new (void)
{
  return g_object_new (FONT_COLORS_TYPE, NULL);
}

GAction *
font_colors_get_reset_action (FontColors *self)
{
  return G_ACTION (self->reset_action);
}
