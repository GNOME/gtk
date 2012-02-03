/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gtkcolorchooserprivate.h"
#include "gtkcolorchooserwidget.h"
#include "gtkcoloreditor.h"
#include "gtkcolorswatch.h"
#include "gtkbox.h"
#include "gtkgrid.h"
#include "gtkhsv.h"
#include "gtklabel.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtksizegroup.h"
#include "gtkalignment.h"

struct _GtkColorChooserWidgetPrivate
{
  GtkWidget *palette;
  GtkWidget *editor;

  GtkWidget *colors;
  GtkWidget *grays;
  GtkWidget *custom;

  GtkWidget *button;
  GtkColorSwatch *current;
  gboolean show_alpha;

  GtkSizeGroup *size_group;

  GSettings *settings;
};

enum
{
  PROP_ZERO,
  PROP_COLOR,
  PROP_SHOW_ALPHA,
  PROP_SHOW_EDITOR
};

static void gtk_color_chooser_widget_iface_init (GtkColorChooserInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkColorChooserWidget, gtk_color_chooser_widget, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_COLOR_CHOOSER,
                                                gtk_color_chooser_widget_iface_init))

static void
select_swatch (GtkColorChooserWidget *cc,
               GtkColorSwatch        *swatch)
{
  GdkRGBA color;

  if (cc->priv->current == swatch)
    return;
  if (cc->priv->current != NULL)
    gtk_color_swatch_set_selected (cc->priv->current, FALSE);
  gtk_color_swatch_set_selected (swatch, TRUE);
  cc->priv->current = swatch;
  gtk_color_swatch_get_color (swatch, &color);
  g_settings_set (cc->priv->settings, "selected-color", "(bdddd)",
                  TRUE, color.red, color.green, color.blue, color.alpha);

  g_object_notify (G_OBJECT (cc), "color");
}

static void save_custom_colors (GtkColorChooserWidget *cc);

static void
button_activate (GtkColorSwatch        *swatch,
                 GtkColorChooserWidget *cc)
{
  GdkRGBA color;

  color.red = 0.75;
  color.green = 0.25;
  color.blue = 0.25;
  color.alpha = 1.0;

  gtk_color_chooser_set_color (GTK_COLOR_CHOOSER (cc->priv->editor), &color);

  gtk_widget_hide (cc->priv->palette);
  gtk_widget_show (cc->priv->editor);
}

static void
swatch_activate (GtkColorSwatch        *swatch,
                 GtkColorChooserWidget *cc)
{
  GdkRGBA color;

  gtk_color_swatch_get_color (swatch, &color);
  _gtk_color_chooser_color_activated (GTK_COLOR_CHOOSER (cc), &color);
}

static void
swatch_customize (GtkColorSwatch        *swatch,
                  GtkColorChooserWidget *cc)
{
  GdkRGBA color;

  gtk_color_swatch_get_color (swatch, &color);
  gtk_color_chooser_set_color (GTK_COLOR_CHOOSER (cc->priv->editor), &color);

  gtk_widget_hide (cc->priv->palette);
  gtk_widget_show (cc->priv->editor);
}

static void
swatch_selected (GtkColorSwatch        *swatch,
                 GParamSpec            *pspec,
                 GtkColorChooserWidget *cc)
{
  select_swatch (cc, swatch);
}

static void
connect_swatch_signals (GtkWidget *p, gpointer data)
{
  g_signal_connect (p, "activate", G_CALLBACK (swatch_activate), data);
  g_signal_connect (p, "customize", G_CALLBACK (swatch_customize), data);
  g_signal_connect (p, "notify::selected", G_CALLBACK (swatch_selected), data);
}

static void
connect_button_signals (GtkWidget *p, gpointer data)
{
  g_signal_connect (p, "activate", G_CALLBACK (button_activate), data);
}

static void
connect_custom_signals (GtkWidget *p, gpointer data)
{
  connect_swatch_signals (p, data);
  g_signal_connect_swapped (p, "notify::color",
                            G_CALLBACK (save_custom_colors), data);
}

static void
save_custom_colors (GtkColorChooserWidget *cc)
{
  GVariantBuilder builder;
  GVariant *variant;
  GdkRGBA color;
  GtkWidget *child;
  gint i;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(dddd)"));

  i = 1;
  while ((child = gtk_grid_get_child_at (GTK_GRID (cc->priv->custom), i, 0)) != NULL)
    {
      i++;
      if (gtk_color_swatch_get_color (GTK_COLOR_SWATCH (child), &color))
        {
          g_variant_builder_add (&builder, "(dddd)",
                                 color.red, color.green, color.blue, color.alpha);
       }
    }

  variant = g_variant_builder_end (&builder);
  g_settings_set_value (cc->priv->settings, "custom-colors", variant);
}

static void
gtk_color_chooser_widget_init (GtkColorChooserWidget *cc)
{
  GtkWidget *grid;
  GtkWidget *p;
  GtkWidget *alignment;
  GtkWidget *button;
  GtkWidget *label;
  gint i, j;
  gint left;
  GdkRGBA color;
  GVariant *variant;
  GVariantIter iter;
  gboolean selected;
  const gchar *default_palette[9][3] = {
    { "#ef2929", "#cc0000", "#a40000" }, /* Scarlet Red */
    { "#fcaf3e", "#f57900", "#ce5c00" }, /* Orange */
    { "#fce94f", "#edd400", "#c4a000" }, /* Butter */
    { "#8ae234", "#73d216", "#4e9a06" }, /* Chameleon */
    { "#729fcf", "#3465a4", "#204a87" }, /* Sky Blue */
    { "#ad7fa8", "#75507b", "#5c3566" }, /* Plum */
    { "#e9b96e", "#c17d11", "#8f5902" }, /* Chocolate */
    { "#888a85", "#555753", "#2e3436" }, /* Aluminum 1 */
    { "#eeeeec", "#d3d7cf", "#babdb6" }  /* Aluminum 2 */
  };
  const gchar *default_grayscale[9] = {
    "#000000",
    "#2e3436",
    "#555753",
    "#888a85",
    "#babdb6",
    "#d3d7cf",
    "#eeeeec",
    "#f3f3f3",
    "#ffffff"
  };

  cc->priv = G_TYPE_INSTANCE_GET_PRIVATE (cc, GTK_TYPE_COLOR_CHOOSER_WIDGET, GtkColorChooserWidgetPrivate);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (cc), GTK_ORIENTATION_VERTICAL);
  cc->priv->palette = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (cc), cc->priv->palette);

  cc->priv->colors = grid = gtk_grid_new ();
  gtk_widget_set_margin_bottom (grid, 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 2);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 4);
  gtk_container_add (GTK_CONTAINER (cc->priv->palette), grid);

  for (i = 0; i < 9; i++)
    {
      for (j = 0; j < 3; j++)
        {
          gdk_rgba_parse (&color, default_palette[i][j]);

          p = gtk_color_swatch_new ();
          connect_swatch_signals (p, cc);

          if (j == 0)
            gtk_color_swatch_set_corner_radii (GTK_COLOR_SWATCH (p), 10, 10, 1, 1);
          else if (j == 2)
            gtk_color_swatch_set_corner_radii (GTK_COLOR_SWATCH (p), 1, 1, 10, 10);
          else
            gtk_color_swatch_set_corner_radii (GTK_COLOR_SWATCH (p), 1, 1, 1, 1);

          gtk_color_swatch_set_color (GTK_COLOR_SWATCH (p), &color);
          gtk_grid_attach (GTK_GRID (grid), p, i, j, 1, 1);
        }
    }

  cc->priv->grays = grid = gtk_grid_new ();
  g_object_set (grid, "margin-bottom", 18, NULL);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 4);
  gtk_container_add (GTK_CONTAINER (cc->priv->palette), grid);

  left = (gtk_widget_get_direction (GTK_WIDGET (cc)) == GTK_TEXT_DIR_LTR) ? 0 : 8;

  for (i = 0; i < 9; i++)
    {
       gdk_rgba_parse (&color, default_grayscale[i]);
       color.alpha = 1.0;

       p = gtk_color_swatch_new ();
       connect_swatch_signals (p, cc);
       if (i == left)
         gtk_color_swatch_set_corner_radii (GTK_COLOR_SWATCH (p), 10, 1, 1, 10);
       else if (i == (8 - left))
         gtk_color_swatch_set_corner_radii (GTK_COLOR_SWATCH (p), 1, 10, 10, 1);
       else
         gtk_color_swatch_set_corner_radii (GTK_COLOR_SWATCH (p), 1, 1, 1, 1);

       gtk_color_swatch_set_color (GTK_COLOR_SWATCH (p), &color);
       gtk_grid_attach (GTK_GRID (grid), p, i, 0, 1, 1);
    }

  label = gtk_label_new (_("Custom"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (cc->priv->palette), label);

  cc->priv->custom = grid = gtk_grid_new ();
  g_object_set (grid, "margin-top", 12, NULL);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 4);
  gtk_container_add (GTK_CONTAINER (cc->priv->palette), grid);

  cc->priv->button = button = gtk_color_swatch_new ();
  gtk_color_swatch_set_corner_radii (GTK_COLOR_SWATCH (button), 10, 10, 10, 10);
  connect_button_signals (button, cc);
  gtk_color_swatch_set_icon (GTK_COLOR_SWATCH (button), "list-add-symbolic");
  gtk_grid_attach (GTK_GRID (grid), button, 0, 0, 1, 1);

  cc->priv->settings = g_settings_new_with_path ("org.gtk.Settings.ColorChooser",
                                                 "/org/gtk/settings/color-chooser/");
  variant = g_settings_get_value (cc->priv->settings, "custom-colors");
  g_variant_iter_init (&iter, variant);
  i = 0;
  while (g_variant_iter_loop (&iter, "(dddd)", &color.red, &color.green, &color.blue, &color.alpha))
    {
      i++;
      p = gtk_color_swatch_new ();
      gtk_color_swatch_set_corner_radii (GTK_COLOR_SWATCH (p), 1, 1, 1, 1);
      gtk_color_swatch_set_color (GTK_COLOR_SWATCH (p), &color);
      gtk_color_swatch_set_can_drop (GTK_COLOR_SWATCH (p), TRUE);
      connect_custom_signals (p, cc);
      gtk_grid_attach (GTK_GRID (grid), p, i, 0, 1, 1);

      if (i == 8)
        break;
    }
  g_variant_unref (variant);

  if (i > 0)
    {
      if (gtk_widget_get_direction (GTK_WIDGET (cc)) == GTK_TEXT_DIR_LTR)
        {
          gtk_color_swatch_set_corner_radii (GTK_COLOR_SWATCH (p), 1, 10, 10, 1);
          gtk_color_swatch_set_corner_radii (GTK_COLOR_SWATCH (button), 10, 1, 1, 10);
        }
      else
        {
          gtk_color_swatch_set_corner_radii (GTK_COLOR_SWATCH (button), 1, 10, 10, 1);
          gtk_color_swatch_set_corner_radii (GTK_COLOR_SWATCH (p), 10, 1, 1, 10);
        }
    }

  cc->priv->editor = gtk_color_editor_new ();
  alignment = gtk_alignment_new (0.5, 0.5, 0, 0);
  gtk_container_add (GTK_CONTAINER (cc), alignment);
  gtk_container_add (GTK_CONTAINER (alignment), cc->priv->editor);

  g_settings_get (cc->priv->settings, "selected-color", "(bdddd)",
                  &selected,
                  &color.red, &color.green, &color.blue, &color.alpha);
  if (selected)
    gtk_color_chooser_set_color (GTK_COLOR_CHOOSER (cc), &color);

  gtk_widget_show_all (GTK_WIDGET (cc));
  gtk_widget_hide (GTK_WIDGET (cc->priv->editor));
  gtk_widget_hide (GTK_WIDGET (cc));

  gtk_widget_set_no_show_all (cc->priv->palette, TRUE);
  gtk_widget_set_no_show_all (cc->priv->editor, TRUE);

  cc->priv->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  gtk_size_group_add_widget (cc->priv->size_group, cc->priv->palette);
  gtk_size_group_add_widget (cc->priv->size_group, alignment);
}

static void
gtk_color_chooser_widget_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GtkColorChooserWidget *cw = GTK_COLOR_CHOOSER_WIDGET (object);
  GtkColorChooser *cc = GTK_COLOR_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_COLOR:
      {
        GdkRGBA color;

        gtk_color_chooser_get_color (cc, &color);
        g_value_set_boxed (value, &color);
      }
      break;
    case PROP_SHOW_ALPHA:
      g_value_set_boolean (value, cw->priv->show_alpha);
      break;
    case PROP_SHOW_EDITOR:
      g_value_set_boolean (value, gtk_widget_get_visible (cw->priv->editor));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_chooser_widget_set_show_alpha (GtkColorChooserWidget *cc,
                                         gboolean               show_alpha)
{
  GtkWidget *grids[3];
  gint i;
  GList *children, *l;
  GtkWidget *swatch;

  cc->priv->show_alpha = show_alpha;
  gtk_color_chooser_set_show_alpha (GTK_COLOR_CHOOSER (cc->priv->editor), show_alpha);

  grids[0] = cc->priv->colors;
  grids[1] = cc->priv->grays;
  grids[2] = cc->priv->custom;

  for (i = 0; i < 3; i++)
    {
      children = gtk_container_get_children (GTK_CONTAINER (grids[i]));
      for (l = children; l; l = l->next)
        {
          swatch = l->data;
          gtk_color_swatch_set_show_alpha (GTK_COLOR_SWATCH (swatch), show_alpha);
        }
      g_list_free (children);
    }

  gtk_widget_queue_draw (GTK_WIDGET (cc));
}

static void
gtk_color_chooser_widget_set_show_editor (GtkColorChooserWidget *cc,
                                          gboolean               show_editor)
{
  gtk_widget_set_visible (cc->priv->editor, show_editor);
  gtk_widget_set_visible (cc->priv->palette, !show_editor);
}

static void
gtk_color_chooser_widget_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (object);

  switch (prop_id)
    {
    case PROP_COLOR:
      gtk_color_chooser_set_color (GTK_COLOR_CHOOSER (cc),
                                   g_value_get_boxed (value));
      break;
    case PROP_SHOW_ALPHA:
      gtk_color_chooser_widget_set_show_alpha (cc,
                                               g_value_get_boolean (value));
      break;
    case PROP_SHOW_EDITOR:
      gtk_color_chooser_widget_set_show_editor (cc,
                                                g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_chooser_widget_finalize (GObject *object)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (object);

  g_object_unref (cc->priv->size_group);
  g_object_unref (cc->priv->settings);

  G_OBJECT_CLASS (gtk_color_chooser_widget_parent_class)->finalize (object);
}

static void
gtk_color_chooser_widget_class_init (GtkColorChooserWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gtk_color_chooser_widget_get_property;
  object_class->set_property = gtk_color_chooser_widget_set_property;
  object_class->finalize = gtk_color_chooser_widget_finalize;

  g_object_class_override_property (object_class, PROP_COLOR, "color");
  g_object_class_override_property (object_class, PROP_SHOW_ALPHA, "show-alpha");

  g_object_class_install_property (object_class, PROP_SHOW_EDITOR,
      g_param_spec_boolean ("show-editor", P_("Show editor"), P_("Show editor"),
                            FALSE, GTK_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GtkColorChooserWidgetPrivate));
}

static void
gtk_color_chooser_widget_get_color (GtkColorChooser *chooser,
                                    GdkRGBA         *color)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (chooser);

  if (gtk_widget_get_visible (cc->priv->editor))
    gtk_color_chooser_get_color (GTK_COLOR_CHOOSER (cc->priv->editor), color);
  else if (cc->priv->current)
    gtk_color_swatch_get_color (cc->priv->current, color);
  else
    {
      color->red = 1.0;
      color->green = 1.0;
      color->blue = 1.0;
      color->alpha = 1.0;
    }

  if (!cc->priv->show_alpha)
    color->alpha = 1.0;
}

static void
add_custom_color (GtkColorChooserWidget *cc,
                  const GdkRGBA         *color)
{
  GtkWidget *last;
  GtkWidget *p;

  last = gtk_grid_get_child_at (GTK_GRID (cc->priv->custom), 8, 0);
  if (last)
    {
      gtk_container_remove (GTK_CONTAINER (cc->priv->custom), last);
      last = gtk_grid_get_child_at (GTK_GRID (cc->priv->custom), 7, 0);
      gtk_color_swatch_set_corner_radii (GTK_COLOR_SWATCH (last), 1, 10, 10, 1);
    }

  gtk_color_swatch_set_corner_radii (GTK_COLOR_SWATCH (cc->priv->button), 10, 1, 1, 10);

  p = gtk_color_swatch_new ();
  gtk_color_swatch_set_color (GTK_COLOR_SWATCH (p), color);
  gtk_color_swatch_set_can_drop (GTK_COLOR_SWATCH (p), TRUE);
  connect_custom_signals (p, cc);

  if (gtk_grid_get_child_at (GTK_GRID (cc->priv->custom), 1, 0) != NULL)
    gtk_color_swatch_set_corner_radii (GTK_COLOR_SWATCH (p), 1, 1, 1, 1);
  else
    gtk_color_swatch_set_corner_radii (GTK_COLOR_SWATCH (p), 1, 10, 10, 1);

  gtk_grid_insert_next_to (GTK_GRID (cc->priv->custom), cc->priv->button, GTK_POS_RIGHT);
  gtk_grid_attach (GTK_GRID (cc->priv->custom), p, 1, 0, 1, 1);
  gtk_widget_show (p);

  select_swatch (cc, GTK_COLOR_SWATCH (p));
  save_custom_colors (cc);
}

static void
gtk_color_chooser_widget_set_color (GtkColorChooser *chooser,
                                    const GdkRGBA   *color)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (chooser);
  GList *children, *l;
  GtkColorSwatch *swatch;
  GdkRGBA c;
  GtkWidget *grids[3];
  gint i;

  grids[0] = cc->priv->colors;
  grids[1] = cc->priv->grays;
  grids[2] = cc->priv->custom;

  for (i = 0; i < 3; i++)
    {
      children = gtk_container_get_children (GTK_CONTAINER (grids[i]));
      for (l = children; l; l = l->next)
        {
          swatch = l->data;
          gtk_color_swatch_get_color (swatch, &c);
          if (!cc->priv->show_alpha)
            c.alpha = color->alpha;
          if (gdk_rgba_equal (color, &c))
            {
              select_swatch (cc, swatch);
              g_list_free (children);
              return;
            }
        }
      g_list_free (children);
    }

  add_custom_color (cc, color);
}

static void
gtk_color_chooser_widget_iface_init (GtkColorChooserInterface *iface)
{
  iface->get_color = gtk_color_chooser_widget_get_color;
  iface->set_color = gtk_color_chooser_widget_set_color;
}

GtkWidget *
gtk_color_chooser_widget_new (void)
{
  return g_object_new (GTK_TYPE_COLOR_CHOOSER_WIDGET, NULL);
}
