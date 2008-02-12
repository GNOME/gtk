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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include <math.h>

static void
spin_ythickness_cb (GtkSpinButton *spin, gpointer user_data)
{
  GtkWidget *frame = user_data;
  GtkRcStyle *rcstyle;

  rcstyle = gtk_rc_style_new ();
  rcstyle->xthickness = GTK_WIDGET (frame)->style->xthickness;
  rcstyle->ythickness = gtk_spin_button_get_value (spin);
  gtk_widget_modify_style (frame, rcstyle);

  g_object_unref (rcstyle);
}

static void
spin_xthickness_cb (GtkSpinButton *spin, gpointer user_data)
{
  GtkWidget *frame = user_data;
  GtkRcStyle *rcstyle;

  rcstyle = gtk_rc_style_new ();
  rcstyle->xthickness = gtk_spin_button_get_value (spin);
  rcstyle->ythickness = GTK_WIDGET (frame)->style->ythickness;
  gtk_widget_modify_style (frame, rcstyle);

  g_object_unref (rcstyle);
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
  gdouble xalign = double_normalize (gtk_spin_button_get_value (spin));
  gtk_frame_set_label_align (frame, xalign, frame->label_yalign);
}

static void
spin_yalign_cb (GtkSpinButton *spin, GtkFrame *frame)
{
  gdouble yalign = double_normalize (gtk_spin_button_get_value (spin));
  gtk_frame_set_label_align (frame, frame->label_xalign, yalign);
}

int main (int argc, char **argv)
{
  GtkWidget *window, *frame, *xthickness_spin, *ythickness_spin, *vbox;
  GtkWidget *xalign_spin, *yalign_spin, *button, *table, *label;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 5);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);

  g_signal_connect (G_OBJECT (window), "delete-event", gtk_main_quit, NULL);

  vbox = gtk_vbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  frame = gtk_frame_new ("Testing");
  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Hello!");
  gtk_container_add (GTK_CONTAINER (frame), button);

  table = gtk_table_new (4, 2, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);

  /* Spin to control xthickness */
  label = gtk_label_new ("xthickness: ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);

  xthickness_spin = gtk_spin_button_new_with_range (0, 250, 1);
  g_signal_connect (G_OBJECT (xthickness_spin), "value-changed", G_CALLBACK (spin_xthickness_cb), frame);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (xthickness_spin), frame->style->xthickness);
  gtk_table_attach_defaults (GTK_TABLE (table), xthickness_spin, 1, 2, 0, 1);

  /* Spin to control ythickness */
  label = gtk_label_new ("ythickness: ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);

  ythickness_spin = gtk_spin_button_new_with_range (0, 250, 1);
  g_signal_connect (G_OBJECT (ythickness_spin), "value-changed", G_CALLBACK (spin_ythickness_cb), frame);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (ythickness_spin), frame->style->ythickness);
  gtk_table_attach_defaults (GTK_TABLE (table), ythickness_spin, 1, 2, 1, 2);

  /* Spin to control label xalign */
  label = gtk_label_new ("xalign: ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);

  xalign_spin = gtk_spin_button_new_with_range (0.0, 1.0, 0.1);
  g_signal_connect (G_OBJECT (xalign_spin), "value-changed", G_CALLBACK (spin_xalign_cb), frame);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (xalign_spin), GTK_FRAME (frame)->label_xalign);
  gtk_table_attach_defaults (GTK_TABLE (table), xalign_spin, 1, 2, 2, 3);

  /* Spin to control label yalign */
  label = gtk_label_new ("yalign: ");
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 3, 4);

  yalign_spin = gtk_spin_button_new_with_range (0.0, 1.0, 0.1);
  g_signal_connect (G_OBJECT (yalign_spin), "value-changed", G_CALLBACK (spin_yalign_cb), frame);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (yalign_spin), GTK_FRAME (frame)->label_yalign);
  gtk_table_attach_defaults (GTK_TABLE (table), yalign_spin, 1, 2, 3, 4);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
