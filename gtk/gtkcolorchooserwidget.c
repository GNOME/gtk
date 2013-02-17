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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcolorchooserprivate.h"
#include "gtkcolorchooserwidget.h"
#include "gtkcoloreditorprivate.h"
#include "gtkcolorswatchprivate.h"
#include "gtkbox.h"
#include "gtkgrid.h"
#include "gtklabel.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtksizegroup.h"

#include <math.h>

/**
 * SECTION:gtkcolorchooserwidget
 * @Short_description: A widget for choosing colors
 * @Title: GtkColorChooserWidget
 * @See_also: #GtkColorChooserDialog
 *
 * The #GtkColorChooserWidget widget lets the user select a
 * color. By default, the chooser presents a prefined palette
 * of colors, plus a small number of settable custom colors.
 * It is also possible to select a different color with the
 * single-color editor. To enter the single-color editing mode,
 * use the context menu of any color of the palette, or use the
 * '+' button to add a new custom color.
 *
 * The chooser automatically remembers the last selection, as well
 * as custom colors.
 *
 * To change the initially selected color, use gtk_color_chooser_set_rgba().
 * To get the selected font use gtk_color_chooser_get_rgba().
 *
 * The #GtkColorChooserWidget is used in the #GtkColorChooserDialog
 * to provide a dialog for selecting colors.
 *
 * Since: 3.4
 */

struct _GtkColorChooserWidgetPrivate
{
  GtkWidget *palette;
  GtkWidget *editor;
  GtkSizeGroup *size_group;

  GtkWidget *custom_label;
  GtkWidget *custom;

  GtkWidget *button;
  GtkColorSwatch *current;

  gboolean use_alpha;
  gboolean has_default_palette;

  GSettings *settings;
};

enum
{
  PROP_ZERO,
  PROP_RGBA,
  PROP_USE_ALPHA,
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
    gtk_widget_unset_state_flags (GTK_WIDGET (cc->priv->current), GTK_STATE_FLAG_SELECTED);
  gtk_widget_set_state_flags (GTK_WIDGET (swatch), GTK_STATE_FLAG_SELECTED, FALSE);
  cc->priv->current = swatch;

  gtk_color_swatch_get_rgba (swatch, &color);
  g_settings_set (cc->priv->settings, "selected-color", "(bdddd)",
                  TRUE, color.red, color.green, color.blue, color.alpha);

  g_object_notify (G_OBJECT (cc), "rgba");
}

static void
swatch_activate (GtkColorSwatch        *swatch,
                 GtkColorChooserWidget *cc)
{
  GdkRGBA color;

  gtk_color_swatch_get_rgba (swatch, &color);
  _gtk_color_chooser_color_activated (GTK_COLOR_CHOOSER (cc), &color);
}

static void
swatch_customize (GtkColorSwatch        *swatch,
                  GtkColorChooserWidget *cc)
{
  GdkRGBA color;

  gtk_color_swatch_get_rgba (swatch, &color);
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cc->priv->editor), &color);

  gtk_widget_hide (cc->priv->palette);
  gtk_widget_show (cc->priv->editor);
  g_object_notify (G_OBJECT (cc), "show-editor");
}

static void
swatch_selected (GtkColorSwatch        *swatch,
                 GtkStateFlags          previous,
                 GtkColorChooserWidget *cc)
{
  GtkStateFlags flags;

  flags = gtk_widget_get_state_flags (GTK_WIDGET (swatch));
  if ((flags & GTK_STATE_FLAG_SELECTED) != (previous & GTK_STATE_FLAG_SELECTED) &&
      (flags & GTK_STATE_FLAG_SELECTED) != 0)
    select_swatch (cc, swatch);
}

static void
connect_swatch_signals (GtkWidget *p,
                        gpointer   data)
{
  g_signal_connect (p, "activate", G_CALLBACK (swatch_activate), data);
  g_signal_connect (p, "customize", G_CALLBACK (swatch_customize), data);
  g_signal_connect (p, "state-flags-changed", G_CALLBACK (swatch_selected), data);
}

static void
button_activate (GtkColorSwatch        *swatch,
                 GtkColorChooserWidget *cc)
{
  /* somewhat random, makes the hairline nicely visible */
  GdkRGBA color = { 0.75, 0.25, 0.25, 1.0 };

  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cc->priv->editor), &color);

  gtk_widget_hide (cc->priv->palette);
  gtk_widget_show (cc->priv->editor);
  g_object_notify (G_OBJECT (cc), "show-editor");
}

static void
connect_button_signals (GtkWidget *p,
                        gpointer   data)
{
  g_signal_connect (p, "activate", G_CALLBACK (button_activate), data);
}

static void
save_custom_colors (GtkColorChooserWidget *cc)
{
  GVariantBuilder builder;
  GVariant *variant;
  GdkRGBA color;
  GList *children, *l;
  GtkWidget *child;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(dddd)"));

  children = gtk_container_get_children (GTK_CONTAINER (cc->priv->custom));
  for (l = g_list_nth (children, 1); l != NULL; l = l->next)
    {
      child = l->data;
      if (gtk_color_swatch_get_rgba (GTK_COLOR_SWATCH (child), &color))
        g_variant_builder_add (&builder, "(dddd)",
                               color.red, color.green, color.blue, color.alpha);
    }

  variant = g_variant_builder_end (&builder);
  g_settings_set_value (cc->priv->settings, "custom-colors", variant);

  g_list_free (children);
}

static void
connect_custom_signals (GtkWidget *p,
                        gpointer   data)
{
  connect_swatch_signals (p, data);
  g_signal_connect_swapped (p, "notify::rgba",
                            G_CALLBACK (save_custom_colors), data);
}

static void
gtk_color_chooser_widget_set_use_alpha (GtkColorChooserWidget *cc,
                                        gboolean               use_alpha)
{
  GList *children, *l;
  GList *palettes, *p;
  GtkWidget *swatch;
  GtkWidget *grid;

  cc->priv->use_alpha = use_alpha;
  gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (cc->priv->editor), use_alpha);

  palettes = gtk_container_get_children (GTK_CONTAINER (cc->priv->palette));
  for (p = palettes; p; p = p->next)
    {
      grid = p->data;

      if (!GTK_IS_CONTAINER (grid))
        continue;

      children = gtk_container_get_children (GTK_CONTAINER (grid));
      for (l = children; l; l = l->next)
        {
          swatch = l->data;
          gtk_color_swatch_set_use_alpha (GTK_COLOR_SWATCH (swatch), use_alpha);
        }
      g_list_free (children);
    }
  g_list_free (palettes);

  gtk_widget_queue_draw (GTK_WIDGET (cc));
}

static void
gtk_color_chooser_widget_set_show_editor (GtkColorChooserWidget *cc,
                                          gboolean               show_editor)
{
  if (show_editor)
    {
      GdkRGBA color = { 0.75, 0.25, 0.25, 1.0 };

      if (cc->priv->current)
        gtk_color_swatch_get_rgba (cc->priv->current, &color);
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cc->priv->editor), &color);
    }

  gtk_widget_set_visible (cc->priv->editor, show_editor);
  gtk_widget_set_visible (cc->priv->palette, !show_editor);
}

/* UI construction {{{1 */

static guint
scale_round (gdouble value, gdouble scale)
{
  value = floor (value * scale + 0.5);
  value = MAX (value, 0);
  value = MIN (value, scale);
  return (guint)value;
}

static gchar *
accessible_color_name (GdkRGBA *color)
{
  if (color->alpha < 1.0)
    return g_strdup_printf (_("Red %d%%, Green %d%%, Blue %d%%, Alpha %d%%"),
                            scale_round (color->red, 100),
                            scale_round (color->green, 100),
                            scale_round (color->blue, 100),
                            scale_round (color->alpha, 100));
  else
    return g_strdup_printf (_("Red %d%%, Green %d%%, Blue %d%%"),
                            scale_round (color->red, 100),
                            scale_round (color->green, 100),
                            scale_round (color->blue, 100));
}

static void
remove_palette (GtkColorChooserWidget *cc)
{
  GList *children, *l;
  GtkWidget *widget;

  if (cc->priv->current != NULL &&
      gtk_widget_get_parent (GTK_WIDGET (cc->priv->current)) != cc->priv->custom)
    cc->priv->current = NULL;

  children = gtk_container_get_children (GTK_CONTAINER (cc->priv->palette));
  for (l = children; l; l = l->next)
    {
      widget = l->data;
      if (widget == cc->priv->custom_label || widget == cc->priv->custom)
        continue;
      gtk_container_remove (GTK_CONTAINER (cc->priv->palette), widget);
    }
  g_list_free (children);
}

static void
add_palette (GtkColorChooserWidget  *cc,
             GtkOrientation          orientation,
             gint                    colors_per_line,
             gint                    n_colors,
             GdkRGBA                *colors,
             const gchar           **names)
{
  GtkWidget *grid;
  GtkWidget *p;
  AtkObject *atk_obj;
  gint line, pos;
  gint i;
  gint left, right;

  if (colors == NULL)
    {
      remove_palette (cc);
      return;
    }

  grid = gtk_grid_new ();
  gtk_widget_set_margin_bottom (grid, 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 2);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 4);
  gtk_container_add (GTK_CONTAINER (cc->priv->palette), grid);

  left = 0;
  right = colors_per_line - 1;
  if (gtk_widget_get_direction (GTK_WIDGET (cc)) == GTK_TEXT_DIR_RTL)
    {
      i = left;
      left = right;
      right = i;
    }

  for (i = 0; i < n_colors; i++)
    {
      p = gtk_color_swatch_new ();
      atk_obj = gtk_widget_get_accessible (p);
      if (names)
        {
          atk_object_set_description (atk_obj,
                                      g_dpgettext2 (GETTEXT_PACKAGE, "Color name", names[i]));
        }
      else
        {
          gchar *text, *name;

          name = accessible_color_name (&colors[i]);
          text = g_strdup_printf (_("Color: %s"), name);
          atk_object_set_description (atk_obj, text);
          g_free (text);
          g_free (name);
        }
      gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (p), &colors[i]);
      connect_swatch_signals (p, cc);

      line = i / colors_per_line;
      pos = i % colors_per_line;

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
            if (pos == left)
              gtk_style_context_add_class (gtk_widget_get_style_context (p), GTK_STYLE_CLASS_LEFT);
            else if (pos == right)
              gtk_style_context_add_class (gtk_widget_get_style_context (p), GTK_STYLE_CLASS_RIGHT);

            gtk_grid_attach (GTK_GRID (grid), p, pos, line, 1, 1);
        }
      else
        {
          if (pos == 0)
            gtk_style_context_add_class (gtk_widget_get_style_context (p), GTK_STYLE_CLASS_TOP);
          else if (pos == colors_per_line - 1)
            gtk_style_context_add_class (gtk_widget_get_style_context (p), GTK_STYLE_CLASS_BOTTOM);

          gtk_grid_attach (GTK_GRID (grid), p, line, pos, 1, 1);
       }
    }

  gtk_widget_show_all (grid);
}

static void
remove_default_palette (GtkColorChooserWidget *cc)
{
  if (!cc->priv->has_default_palette)
    return;

  remove_palette (cc);
  cc->priv->has_default_palette = FALSE;
}

static void
add_default_palette (GtkColorChooserWidget *cc)
{
  const gchar *default_colors[9][3] = {
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
  const gchar *color_names[] = {
    NC_("Color name", "Light Scarlet Red"),
    NC_("Color name", "Scarlet Red"),
    NC_("Color name", "Dark Scarlet Red"),
    NC_("Color name", "Light Orange"),
    NC_("Color name", "Orange"),
    NC_("Color name", "Dark Orange"),
    NC_("Color name", "Light Butter"),
    NC_("Color name", "Butter"),
    NC_("Color name", "Dark Butter"),
    NC_("Color name", "Light Chameleon"),
    NC_("Color name", "Chameleon"),
    NC_("Color name", "Dark Chameleon"),
    NC_("Color name", "Light Sky Blue"),
    NC_("Color name", "Sky Blue"),
    NC_("Color name", "Dark Sky Blue"),
    NC_("Color name", "Light Plum"),
    NC_("Color name", "Plum"),
    NC_("Color name", "Dark Plum"),
    NC_("Color name", "Light Chocolate"),
    NC_("Color name", "Chocolate"),
    NC_("Color name", "Dark Chocolate"),
    NC_("Color name", "Light Aluminum 1"),
    NC_("Color name", "Aluminum 1"),
    NC_("Color name", "Dark Aluminum 1"),
    NC_("Color name", "Light Aluminum 2"),
    NC_("Color name", "Aluminum 2"),
    NC_("Color name", "Dark Aluminum 2")
  };
  const gchar *default_grays[9] = {
    "#000000", /* black */
    "#2e3436", /* very dark gray */
    "#555753", /* darker gray */
    "#888a85", /* dark gray */
    "#babdb6", /* medium gray */
    "#d3d7cf", /* light gray */
    "#eeeeec", /* lighter gray */
    "#f3f3f3", /* very light gray */
    "#ffffff"  /* white */
  };
  const gchar *gray_names[] = {
    NC_("Color name", "Black"),
    NC_("Color name", "Very Dark Gray"),
    NC_("Color name", "Darker Gray"),
    NC_("Color name", "Dark Gray"),
    NC_("Color name", "Medium Gray"),
    NC_("Color name", "Light Gray"),
    NC_("Color name", "Lighter Gray"),
    NC_("Color name", "Very Light Gray"),
    NC_("Color name", "White")
  };
  GdkRGBA colors[9*3];
  gint i, j;

  for (i = 0; i < 9; i++)
    for (j = 0; j < 3; j++)
      gdk_rgba_parse (&colors[i*3 + j], default_colors[i][j]);

  add_palette (cc, GTK_ORIENTATION_VERTICAL, 3, 9*3, colors, color_names);

  for (i = 0; i < 9; i++)
    gdk_rgba_parse (&colors[i], default_grays[i]);

  add_palette (cc, GTK_ORIENTATION_HORIZONTAL, 9, 9, colors, gray_names);

  cc->priv->has_default_palette = TRUE;
}

static void
gtk_color_chooser_widget_init (GtkColorChooserWidget *cc)
{
  GtkWidget *box;
  GtkWidget *p;
  GtkWidget *button;
  GtkWidget *label;
  gint i;
  GdkRGBA color;
  GVariant *variant;
  GVariantIter iter;
  gboolean selected;
  AtkObject *atk_obj;
  gchar *text, *name;

  cc->priv = G_TYPE_INSTANCE_GET_PRIVATE (cc, GTK_TYPE_COLOR_CHOOSER_WIDGET, GtkColorChooserWidgetPrivate);

  cc->priv->use_alpha = TRUE;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (cc), GTK_ORIENTATION_VERTICAL);
  cc->priv->palette = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (cc), cc->priv->palette);

  add_default_palette (cc);

  cc->priv->custom = box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  g_object_set (box, "margin-top", 12, NULL);
  gtk_box_pack_end (GTK_BOX (cc->priv->palette), box, FALSE, TRUE, 0);

  /* translators: label for the custom section in the color chooser */
  cc->priv->custom_label = label = gtk_label_new (_("Custom"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_box_pack_end (GTK_BOX (cc->priv->palette), label, FALSE, TRUE, 0);

  cc->priv->button = button = gtk_color_swatch_new ();
  gtk_widget_set_name (button, "add-color-button");
  atk_obj = gtk_widget_get_accessible (button);
  atk_object_set_role (atk_obj, ATK_ROLE_PUSH_BUTTON);
  atk_object_set_description (atk_obj, _("Create custom color"));
  connect_button_signals (button, cc);
  gtk_color_swatch_set_icon (GTK_COLOR_SWATCH (button), "list-add-symbolic");
  gtk_container_add (GTK_CONTAINER (box), button);

  cc->priv->settings = g_settings_new ("org.gtk.Settings.ColorChooser");
  variant = g_settings_get_value (cc->priv->settings, "custom-colors");
  g_variant_iter_init (&iter, variant);
  i = 0;
  p = NULL;
  while (g_variant_iter_loop (&iter, "(dddd)", &color.red, &color.green, &color.blue, &color.alpha))
    {
      i++;
      p = gtk_color_swatch_new ();
      gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (p), &color);
      gtk_color_swatch_set_can_drop (GTK_COLOR_SWATCH (p), TRUE);
      atk_obj = gtk_widget_get_accessible (p);
      name = accessible_color_name (&color);
      text = g_strdup_printf (_("Custom color %d: %s"), i, name);
      atk_object_set_description (atk_obj, text);
      g_free (text);
      g_free (name);
      connect_custom_signals (p, cc);
      gtk_container_add (GTK_CONTAINER (box), p);

      if (i == 8)
        break;
    }
  g_variant_unref (variant);

  cc->priv->editor = gtk_color_editor_new ();
  gtk_widget_set_halign (cc->priv->editor, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (cc->priv->editor, TRUE);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (cc), box);
  gtk_container_add (GTK_CONTAINER (box), cc->priv->editor);

  g_settings_get (cc->priv->settings, "selected-color", "(bdddd)",
                  &selected,
                  &color.red, &color.green, &color.blue, &color.alpha);
  if (selected)
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cc), &color);

  gtk_widget_show_all (GTK_WIDGET (cc));
  gtk_widget_hide (GTK_WIDGET (cc->priv->editor));
  gtk_widget_hide (GTK_WIDGET (cc));

  gtk_widget_set_no_show_all (cc->priv->palette, TRUE);
  gtk_widget_set_no_show_all (cc->priv->editor, TRUE);

  cc->priv->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  gtk_size_group_add_widget (cc->priv->size_group, cc->priv->palette);
  gtk_size_group_add_widget (cc->priv->size_group, box);
}

/* GObject implementation {{{1 */

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
    case PROP_RGBA:
      {
        GdkRGBA color;

        gtk_color_chooser_get_rgba (cc, &color);
        g_value_set_boxed (value, &color);
      }
      break;
    case PROP_USE_ALPHA:
      g_value_set_boolean (value, cw->priv->use_alpha);
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
gtk_color_chooser_widget_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (object);

  switch (prop_id)
    {
    case PROP_RGBA:
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cc),
                                  g_value_get_boxed (value));
      break;
    case PROP_USE_ALPHA:
      gtk_color_chooser_widget_set_use_alpha (cc,
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

  g_object_class_override_property (object_class, PROP_RGBA, "rgba");
  g_object_class_override_property (object_class, PROP_USE_ALPHA, "use-alpha");

  /**
   * GtkColorChooserWidget:show-editor:
   *
   * The ::show-editor property is %TRUE when the color chooser
   * is showing the single-color editor. It can be set to switch
   * the color chooser into single-color editing mode.
   *
   * Since: 3.4
   */
  g_object_class_install_property (object_class, PROP_SHOW_EDITOR,
      g_param_spec_boolean ("show-editor", P_("Show editor"), P_("Show editor"),
                            FALSE, GTK_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GtkColorChooserWidgetPrivate));
}

/* GtkColorChooser implementation {{{1 */

static void
gtk_color_chooser_widget_get_rgba (GtkColorChooser *chooser,
                                   GdkRGBA         *color)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (chooser);

  if (gtk_widget_get_visible (cc->priv->editor))
    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (cc->priv->editor), color);
  else if (cc->priv->current)
    gtk_color_swatch_get_rgba (cc->priv->current, color);
  else
    {
      color->red = 1.0;
      color->green = 1.0;
      color->blue = 1.0;
      color->alpha = 1.0;
    }

  if (!cc->priv->use_alpha)
    color->alpha = 1.0;
}

static void
add_custom_color (GtkColorChooserWidget *cc,
                  const GdkRGBA         *color)
{
  GtkWidget *last;
  GtkWidget *p;
  GList *children;

  children = gtk_container_get_children (GTK_CONTAINER (cc->priv->custom));
  if (g_list_length (children) >= 8)
    {
      last = g_list_last (children)->data;
      gtk_widget_destroy (last);
    }

  g_list_free (children);

  p = gtk_color_swatch_new ();
  gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (p), color);
  gtk_color_swatch_set_can_drop (GTK_COLOR_SWATCH (p), TRUE);
  connect_custom_signals (p, cc);

  gtk_container_add (GTK_CONTAINER (cc->priv->custom), p);
  gtk_box_reorder_child (GTK_BOX (cc->priv->custom), p, 1);
  gtk_widget_show (p);

  select_swatch (cc, GTK_COLOR_SWATCH (p));
  save_custom_colors (cc);
}

static void
gtk_color_chooser_widget_set_rgba (GtkColorChooser *chooser,
                                   const GdkRGBA   *color)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (chooser);
  GList *children, *l;
  GList *palettes, *p;
  GtkColorSwatch *swatch;
  GtkWidget *w;
  GdkRGBA c;

  palettes = gtk_container_get_children (GTK_CONTAINER (cc->priv->palette));
  for (p = palettes; p; p = p->next)
    {
      w = p->data;
      if (!GTK_IS_GRID (w) && !GTK_IS_BOX (w))
        continue;

      children = gtk_container_get_children (GTK_CONTAINER (w));
      for (l = children; l; l = l->next)
        {
          swatch = l->data;
          gtk_color_swatch_get_rgba (swatch, &c);
          if (!cc->priv->use_alpha)
            c.alpha = color->alpha;
          if (gdk_rgba_equal (color, &c))
            {
              select_swatch (cc, swatch);
              g_list_free (children);
              g_list_free (palettes);
              return;
            }
        }
      g_list_free (children);
    }
  g_list_free (palettes);

  add_custom_color (cc, color);
}

static void
gtk_color_chooser_widget_add_palette (GtkColorChooser *chooser,
                                      GtkOrientation   orientation,
                                      gint             colors_per_line,
                                      gint             n_colors,
                                      GdkRGBA         *colors)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (chooser);

  remove_default_palette (cc);
  add_palette (cc, orientation, colors_per_line, n_colors, colors, NULL);
}

static void
gtk_color_chooser_widget_iface_init (GtkColorChooserInterface *iface)
{
  iface->get_rgba = gtk_color_chooser_widget_get_rgba;
  iface->set_rgba = gtk_color_chooser_widget_set_rgba;
  iface->add_palette = gtk_color_chooser_widget_add_palette;
}

/* Public API {{{1 */

/**
 * gtk_color_chooser_widget_new:
 *
 * Creates a new #GtkColorChooserWidget.
 *
 * Returns: a new #GtkColorChooserWidget
 *
 * Since: 3.4
 */
GtkWidget *
gtk_color_chooser_widget_new (void)
{
  return g_object_new (GTK_TYPE_COLOR_CHOOSER_WIDGET, NULL);
}

/* vim:set foldmethod=marker: */
