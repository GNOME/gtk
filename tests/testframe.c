/* testframe.c
 * Copyright (C) 2007  Xan López <xan@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>
#include <math.h>

static void
spin_hpadding_cb (GtkSpinButton *spin, gpointer user_data)
{
  GtkWidget *frame = user_data;
  GtkCssProvider *provider;
  GtkStyleContext *context;
  gchar *data;
  GtkBorder pad;

  context = gtk_widget_get_style_context (frame);
  provider = g_object_get_data (G_OBJECT (frame), "provider");
  if (provider == NULL)
    {
      provider = gtk_css_provider_new ();
      g_object_set_data (G_OBJECT (frame), "provider", provider);
      gtk_style_context_add_provider (context,
                                      GTK_STYLE_PROVIDER (provider),
                                      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  gtk_style_context_save (context);
  gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
  gtk_style_context_get_padding (context, &pad);
  gtk_style_context_restore (context);


  data = g_strdup_printf ("frame { padding: %dpx %dpx }",
                          pad.top,
                          (gint)gtk_spin_button_get_value (spin));

  gtk_css_provider_load_from_data (provider, data, -1);
  g_free (data);

  gtk_widget_queue_resize (frame);
}

static void
spin_vpadding_cb (GtkSpinButton *spin, gpointer user_data)
{
  GtkWidget *frame = user_data;
  GtkCssProvider *provider;
  GtkStyleContext *context;
  gchar *data;
  GtkBorder pad;

  context = gtk_widget_get_style_context (frame);
  provider = g_object_get_data (G_OBJECT (frame), "provider");
  if (provider == NULL)
    {
      provider = gtk_css_provider_new ();
      g_object_set_data (G_OBJECT (frame), "provider", provider);
      gtk_style_context_add_provider (context,
                                      GTK_STYLE_PROVIDER (provider),
                                      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  gtk_style_context_save (context);
  gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
  gtk_style_context_get_padding (context, &pad);
  gtk_style_context_restore (context);


  data = g_strdup_printf ("frame { padding: %dpx %dpx }",
                          (gint)gtk_spin_button_get_value (spin),
                          pad.left);

  gtk_css_provider_load_from_data (provider, data, -1);
  g_free (data);

  gtk_widget_queue_resize (frame);
}

/* Function to normalize rounding errors in FP arithmetic to
   our desired limits */

#define EPSILON 1e-10

static gdouble
double_normalize (gdouble n)
{
  if (fabs (1.0 - n) < EPSILON)
    n = 1.0;
  else if (n < EPSILON)
    n = 0.0;

  return n;
}

static void
spin_xalign_cb (GtkSpinButton *spin, GtkFrame *frame)
{
  gdouble xalign;

  xalign = double_normalize (gtk_spin_button_get_value (spin));
  gtk_frame_set_label_align (frame, xalign);
}

static void
draw_border_cb (GtkToggleButton *toggle_button, GtkFrame *frame)
{
  GtkShadowType shadow_type = gtk_toggle_button_get_active (toggle_button)
                              ? GTK_SHADOW_IN : GTK_SHADOW_NONE;

  gtk_frame_set_shadow_type (frame, shadow_type);
}

int main (int argc, char **argv)
{
  GtkWidget *window, *widget;
  GtkBox *vbox;
  GtkFrame *frame;
  GtkGrid *grid;
  gfloat xalign;
  gboolean draw_border;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);

  g_signal_connect (window, "destroy", gtk_main_quit, NULL);

  vbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 5));
  g_object_set (vbox, "margin", 12, NULL);
  gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (vbox));

  frame = GTK_FRAME (gtk_frame_new ("Test GtkFrame"));
  gtk_widget_set_vexpand (GTK_WIDGET (frame), TRUE);
  gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (frame));

  widget = gtk_button_new_with_label ("Hello!");
  gtk_container_add (GTK_CONTAINER (frame), widget);

  grid = GTK_GRID (gtk_grid_new ());
  gtk_grid_set_row_spacing (grid, 12);
  gtk_grid_set_column_spacing (grid, 6);
  gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (grid));

  xalign = gtk_frame_get_label_align (frame);

  /* Spin to control :label-xalign */
  widget = gtk_label_new ("label xalign:");
  gtk_grid_attach (grid, widget, 0, 0, 1, 1);

  widget = gtk_spin_button_new_with_range (0.0, 1.0, 0.1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), xalign);
  g_signal_connect (widget, "value-changed", G_CALLBACK (spin_xalign_cb), frame);
  gtk_grid_attach (grid, widget, 1, 0, 1, 1);

  /* Spin to control vertical padding */
  widget = gtk_label_new ("vertical padding:");
  gtk_grid_attach (grid, widget, 0, 1, 1, 1);

  widget = gtk_spin_button_new_with_range (0, 250, 1);
  g_signal_connect (widget, "value-changed", G_CALLBACK (spin_vpadding_cb), frame);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), 0);
  gtk_grid_attach (grid, widget, 1, 1, 1, 1);

  /* Spin to control horizontal padding */
  widget = gtk_label_new ("horizontal padding:");
  gtk_grid_attach (grid, widget, 0, 2, 1, 1);

  widget = gtk_spin_button_new_with_range (0, 250, 1);
  g_signal_connect (widget, "value-changed", G_CALLBACK (spin_hpadding_cb), frame);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), 0);
  gtk_grid_attach (grid, widget, 1, 2, 1, 1);

  /* CheckButton to control whether to draw border */
  draw_border = gtk_frame_get_shadow_type (frame) != GTK_SHADOW_NONE;
  widget = gtk_check_button_new_with_label ("draw border");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), draw_border);
  g_signal_connect (widget, "toggled", G_CALLBACK (draw_border_cb), frame);
  gtk_grid_attach (grid, widget, 0, 3, 1, 1);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
