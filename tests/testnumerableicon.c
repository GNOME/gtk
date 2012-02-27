/* testnumerableicon.c
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

#include <stdlib.h>
#include <gtk/gtk.h>
#include "prop-editor.h"


typedef struct {
  GIcon *numerable;
  GtkWidget *image;
  gboolean odd;
  GtkIconSize size;
} PackData;

static void
button_clicked_cb (GtkButton *b,
                   gpointer user_data)
{
  PackData *d = user_data;
  GtkCssProvider *provider;
  GtkStyleContext *style;
  GError *error = NULL;
  gchar *data, *bg_str, *grad1, *grad2;
  const gchar data_format[] = "GtkNumerableIcon { background-color: %s; color: #000000;"
    "background-image: -gtk-gradient (linear, 0 0, 1 1, from(%s), to(%s));"
    "font: Monospace 12;"
    /* "background-image: url('apple-red.png');" */
    "}";

  bg_str = g_strdup_printf ("rgb(%d,%d,%d)", g_random_int_range (0, 255), g_random_int_range (0, 255), g_random_int_range (0, 255));
  grad1 = g_strdup_printf ("rgb(%d,%d,%d)", g_random_int_range (0, 255), g_random_int_range (0, 255), g_random_int_range (0, 255));
  grad2 = g_strdup_printf ("rgb(%d,%d,%d)", g_random_int_range (0, 255), g_random_int_range (0, 255), g_random_int_range (0, 255));

  data = g_strdup_printf (data_format, bg_str, grad1, grad2);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, data, -1, &error);

  g_assert (error == NULL);

  style = gtk_widget_get_style_context (d->image);
  gtk_style_context_add_provider (style, GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_USER);

  if (d->odd) {
    gtk_numerable_icon_set_background_icon_name (GTK_NUMERABLE_ICON (d->numerable), NULL);
    gtk_numerable_icon_set_count (GTK_NUMERABLE_ICON (d->numerable), g_random_int_range (-99, 99));
  } else {
    gtk_numerable_icon_set_background_icon_name (GTK_NUMERABLE_ICON (d->numerable),
                                                 "emblem-favorite");
    gtk_numerable_icon_set_label (GTK_NUMERABLE_ICON (d->numerable), "IVX");
  }
  
  gtk_image_set_from_gicon (GTK_IMAGE (d->image), d->numerable, d->size);

  d->odd = !d->odd;

  g_free (data);
  g_free (bg_str);
  g_free (grad1);
  g_free (grad2);
  
  g_object_unref (provider);
}

static gboolean
delete_event_cb (GtkWidget *editor,
                 gint       response,
                 gpointer   user_data)
{
  gtk_widget_hide (editor);

  return TRUE;
}

static void
properties_cb (GtkWidget *button,
               GObject   *entry)
{
  GtkWidget *editor;

  editor = g_object_get_data (entry, "properties-dialog");

  if (editor == NULL)
    {
      editor = create_prop_editor (G_OBJECT (entry), G_TYPE_INVALID);
      gtk_container_set_border_width (GTK_CONTAINER (editor), 12);
      gtk_window_set_transient_for (GTK_WINDOW (editor),
                                    GTK_WINDOW (gtk_widget_get_toplevel (button)));
      g_signal_connect (editor, "delete-event", G_CALLBACK (delete_event_cb), NULL);
      g_object_set_data (entry, "properties-dialog", editor);
    }

  gtk_window_present (GTK_WINDOW (editor));
}

static void
refresh_cb (GtkWidget *button,
            gpointer   user_data)
{
  PackData *d = user_data;

  gtk_image_set_from_gicon (GTK_IMAGE (d->image), d->numerable, d->size);
}

static void
pack_numerable (GtkWidget *parent,
                GtkIconSize size)
{
  PackData *d;
  GtkWidget *vbox, *label, *image, *button;
  gchar *str;
  GIcon *icon, *numerable;

  d = g_slice_new0 (PackData);

  image = gtk_image_new ();
  icon = g_themed_icon_new ("system-file-manager");
  numerable = gtk_numerable_icon_new (icon);

  d->image = image;
  d->numerable = numerable;
  d->odd = FALSE;
  d->size = size;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_box_pack_start (GTK_BOX (parent), vbox, FALSE, FALSE, 0);

  gtk_numerable_icon_set_count (GTK_NUMERABLE_ICON (numerable), 42);
  gtk_box_pack_start (GTK_BOX (vbox), image, FALSE, FALSE, 0);
  gtk_numerable_icon_set_style_context (GTK_NUMERABLE_ICON (numerable),
                                        gtk_widget_get_style_context (image));
  gtk_image_set_from_gicon (GTK_IMAGE (image), numerable, size);

  label = gtk_label_new (NULL);
  str = g_strdup_printf ("Numerable icon, hash %u", g_icon_hash (numerable));
  gtk_label_set_label (GTK_LABEL (label), str);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Change icon number");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (button_clicked_cb), d);

  button = gtk_button_new_with_label ("Properties");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (properties_cb), numerable);

  button = gtk_button_new_with_label ("Refresh");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (refresh_cb), d);
}

int
main (int argc,
      char **argv)
{
  GtkWidget *hbox, *toplevel;

  gtk_init (&argc, &argv);

  toplevel = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_add (GTK_CONTAINER (toplevel), hbox);

  pack_numerable (hbox, GTK_ICON_SIZE_DIALOG);
  pack_numerable (hbox, GTK_ICON_SIZE_BUTTON);

  gtk_widget_show_all (toplevel);

  g_signal_connect (toplevel, "delete-event",
		    G_CALLBACK (gtk_main_quit), NULL);

  gtk_main ();

  return 0;
}

