/* testgiconpixbuf.c
 * Copyright (C) 2010 Red Hat, Inc.
 * Authors: Cosimo Cecchi
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

int
main (int argc,
      char **argv)
{
  GdkPixbuf *pixbuf, *otherpix;
  GtkWidget *image, *image2, *hbox, *vbox, *label, *toplevel;
  GIcon *emblemed;
  GEmblem *emblem;
  gchar *str;

  gtk_init (&argc, &argv);

  pixbuf = gdk_pixbuf_new_from_file ("apple-red.png", NULL);
  toplevel = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_add (GTK_CONTAINER (toplevel), hbox);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

  image = gtk_image_new_from_gicon (G_ICON (pixbuf), GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (vbox), image, FALSE, FALSE, 0);

  label = gtk_label_new (NULL);
  str = g_strdup_printf ("Normal icon, hash %u", g_icon_hash (G_ICON (pixbuf)));
  gtk_label_set_label (GTK_LABEL (label), str);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

  otherpix = gdk_pixbuf_new_from_file ("gnome-textfile.png", NULL);
  emblem = g_emblem_new (G_ICON (otherpix));
  emblemed = g_emblemed_icon_new (G_ICON (pixbuf), emblem);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
  
  image2 = gtk_image_new_from_gicon (emblemed, GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (vbox), image2, FALSE, FALSE, 0);

  label = gtk_label_new (NULL);
  str = g_strdup_printf ("Emblemed icon, hash %u", g_icon_hash (emblemed));
  gtk_label_set_label (GTK_LABEL (label), str);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

  gtk_widget_show_all (toplevel);

  g_signal_connect (toplevel, "delete-event",
		    G_CALLBACK (gtk_main_quit), NULL);

  gtk_main ();

  return 0;
}
