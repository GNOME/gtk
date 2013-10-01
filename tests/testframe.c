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
spin_ythickness_cb (GtkSpinButton *spin, gpointer user_data)
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

  gtk_style_context_get_padding (context, GTK_STATE_FLAG_NORMAL, &pad);

  data = g_strdup_printf ("GtkFrame { padding: %d %d }",
                          pad.top,
                          (gint)gtk_spin_button_get_value (spin));

  gtk_css_provider_load_from_data (provider, data, -1, NULL);
  g_free (data);

  gtk_widget_queue_resize (frame);
}

static void
spin_xthickness_cb (GtkSpinButton *spin, gpointer user_data)
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

  gtk_style_context_get_padding (context, GTK_STATE_FLAG_NORMAL, &pad);

  data = g_strdup_printf ("GtkFrame { padding: %d %d }",
                          (gint)gtk_spin_button_get_value (spin),
                          pad.left);

  gtk_css_provider_load_from_data (provider, data, -1, NULL);
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
  gfloat yalign;

  xalign = double_normalize (gtk_spin_button_get_value (spin));
  gtk_frame_get_label_align (frame, NULL, &yalign);
  gtk_frame_set_label_align (frame, xalign, yalign);
}

static void
spin_yalign_cb (GtkSpinButton *spin, GtkFrame *frame)
{
  gdouble yalign;
  gfloat xalign;

  yalign = double_normalize (gtk_spin_button_get_value (spin));
  gtk_frame_get_label_align (frame, &xalign, NULL);
  gtk_frame_set_label_align (frame, xalign, yalign);
}

int main (int argc, char **argv)
{
  GtkStyleContext *context;
  GtkBorder pad;
  GtkWidget *window, *frame, *xthickness_spin, *ythickness_spin, *vbox;
  GtkWidget *xalign_spin, *yalign_spin, *button, *grid, *label;
  gfloat xalign, yalign;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 5);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);

  g_signal_connect (G_OBJECT (window), "delete-event", gtk_main_quit, NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  frame = gtk_frame_new ("Testing");
  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Hello!");
  gtk_container_add (GTK_CONTAINER (frame), button);

  grid = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (vbox), grid, FALSE, FALSE, 0);

  context = gtk_widget_get_style_context (frame);
  gtk_style_context_get_padding (context, GTK_STATE_FLAG_NORMAL, &pad);

  /* Spin to control xthickness */
  label = gtk_label_new ("xthickness: ");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  xthickness_spin = gtk_spin_button_new_with_range (0, 250, 1);
  g_signal_connect (G_OBJECT (xthickness_spin), "value-changed", G_CALLBACK (spin_xthickness_cb), frame);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (xthickness_spin), pad.left);
  gtk_grid_attach (GTK_GRID (grid), xthickness_spin, 1, 0, 1, 1);

  /* Spin to control ythickness */
  label = gtk_label_new ("ythickness: ");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

  ythickness_spin = gtk_spin_button_new_with_range (0, 250, 1);
  g_signal_connect (G_OBJECT (ythickness_spin), "value-changed", G_CALLBACK (spin_ythickness_cb), frame);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (ythickness_spin), pad.top);
  gtk_grid_attach (GTK_GRID (grid), ythickness_spin, 1, 1, 1, 1);

  gtk_frame_get_label_align (GTK_FRAME (frame), &xalign, &yalign);

  /* Spin to control label xalign */
  label = gtk_label_new ("xalign: ");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);

  xalign_spin = gtk_spin_button_new_with_range (0.0, 1.0, 0.1);
  g_signal_connect (G_OBJECT (xalign_spin), "value-changed", G_CALLBACK (spin_xalign_cb), frame);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (xalign_spin), xalign);
  gtk_grid_attach (GTK_GRID (grid), xalign_spin, 1, 2, 1, 1);

  /* Spin to control label yalign */
  label = gtk_label_new ("yalign: ");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);

  yalign_spin = gtk_spin_button_new_with_range (0.0, 1.0, 0.1);
  g_signal_connect (G_OBJECT (yalign_spin), "value-changed", G_CALLBACK (spin_yalign_cb), frame);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (yalign_spin), yalign);
  gtk_grid_attach (GTK_GRID (grid), yalign_spin, 1, 3, 1, 1);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
