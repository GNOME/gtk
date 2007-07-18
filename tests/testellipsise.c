/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <gtk/gtk.h>

static void
combo_changed_cb (GtkWidget *combo,
		  gpointer   data)
{
  GtkWidget *label = GTK_WIDGET (data);
  gint active;

  active = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
  
  gtk_label_set_ellipsize (GTK_LABEL (label), (PangoEllipsizeMode)active);
}

static void
scale_changed_cb (GtkRange *range,
		  gpointer   data)
{
  GtkWidget *label = GTK_WIDGET (data);
  double angle = gtk_range_get_value (range);
  
  gtk_label_set_angle (GTK_LABEL (label), angle);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *vbox, *label, *combo, *scale;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 12);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  label = gtk_label_new ("This label may be ellipsized\nto make it fit.");
  gtk_box_pack_end (GTK_BOX (vbox), label, TRUE, TRUE, 0);

  combo = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), "NONE");
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), "START");
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), "MIDDLE");
  gtk_combo_box_append_text (GTK_COMBO_BOX (combo), "END");
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
  gtk_box_pack_start (GTK_BOX (vbox), combo, FALSE, TRUE, 0);
  g_signal_connect (combo, "changed", G_CALLBACK (combo_changed_cb), label);

  scale = gtk_hscale_new_with_range (0, 360, 1);
  g_signal_connect (scale, "value-changed", G_CALLBACK (scale_changed_cb), label);
  gtk_box_pack_start (GTK_BOX (vbox), scale, FALSE, TRUE, 0);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
