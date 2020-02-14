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
#include <glib/gstdio.h>

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc,
      char **argv)
{
  GdkPixbuf *pixbuf, *otherpix;
  GtkWidget *image, *image2, *hbox, *vbox, *label, *toplevel;
  GIcon *emblemed;
  GEmblem *emblem;
  gchar *str;
  gboolean done = FALSE;

#ifdef GTK_SRCDIR
  g_chdir (GTK_SRCDIR);
#endif

  gtk_init ();

  pixbuf = gdk_pixbuf_new_from_file ("apple-red.png", NULL);
  toplevel = gtk_window_new ();
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_add (GTK_CONTAINER (toplevel), hbox);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_add (GTK_CONTAINER (hbox), vbox);

  image = gtk_image_new_from_gicon (G_ICON (pixbuf));
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
  gtk_container_add (GTK_CONTAINER (vbox), image);

  label = gtk_label_new (NULL);
  str = g_strdup_printf ("Normal icon, hash %u", g_icon_hash (G_ICON (pixbuf)));
  gtk_label_set_label (GTK_LABEL (label), str);
  gtk_container_add (GTK_CONTAINER (vbox), label);

  otherpix = gdk_pixbuf_new_from_file ("gnome-textfile.png", NULL);
  emblem = g_emblem_new (G_ICON (otherpix));
  emblemed = g_emblemed_icon_new (G_ICON (pixbuf), emblem);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_add (GTK_CONTAINER (hbox), vbox);

  image2 = gtk_image_new_from_gicon (emblemed);
  gtk_image_set_icon_size (GTK_IMAGE (image2), GTK_ICON_SIZE_LARGE);
  gtk_container_add (GTK_CONTAINER (vbox), image2);

  label = gtk_label_new (NULL);
  str = g_strdup_printf ("Emblemed icon, hash %u", g_icon_hash (emblemed));
  gtk_label_set_label (GTK_LABEL (label), str);
  gtk_container_add (GTK_CONTAINER (vbox), label);

  gtk_widget_show (toplevel);

  g_signal_connect (toplevel, "destroy", G_CALLBACK (quit_cb), &done);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
