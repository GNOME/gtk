/* simple.c
 * Copyright (C) 2017  Red Hat, Inc
 * Author: Benjamin Otte
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
#include "config.h"
#include <gtk/gtk.h>


void
copy (void)
{
  GdkClipboard *clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
  GFile *file = g_file_new_for_path ("/home/mclasen/faw-sig");

  gdk_clipboard_set (clipboard, G_TYPE_FILE, file);

  g_object_unref (file);
}

static void
value_received (GObject *object,
                GAsyncResult *result,
                gpointer data)
{
  const GValue *value;
  GError *error = NULL;
  GSList *l;

  value = gdk_clipboard_read_value_finish (GDK_CLIPBOARD (object), result, &error);
  if (value == NULL)
    {
      g_print ("Failed to read: %s\n", error->message);
      g_error_free (error);
      return;
    }

  for (l = g_value_get_boxed (value); l; l = l->next)
    g_print ("%s\n", g_file_get_path (l->data));
}

void
paste (void)
{
  GdkClipboard *clipboard = gdk_display_get_clipboard (gdk_display_get_default ());

  gdk_clipboard_read_value_async (clipboard, GDK_TYPE_FILE_LIST, 0, NULL, value_received, NULL);

}

static void
clipboard_changed (GdkClipboard *clipboard)
{
  GdkContentFormats *formats = gdk_clipboard_get_formats (clipboard);
  g_autofree char *s = gdk_content_formats_to_string (formats);
  g_print ("clipboard contents now: %s, local: %d\n", s, gdk_clipboard_is_local (clipboard));
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *button, *box;
  GdkClipboard *clipboard;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "hello world");
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  button = gtk_button_new ();
  gtk_button_set_label (GTK_BUTTON (button), "copy");
  g_signal_connect (button, "clicked", G_CALLBACK (copy), NULL);
  gtk_container_add (GTK_CONTAINER (box), button);

  button = gtk_button_new ();
  gtk_button_set_label (GTK_BUTTON (button), "paste");
  g_signal_connect (button, "clicked", G_CALLBACK (paste), NULL);
  gtk_container_add (GTK_CONTAINER (box), button);

  gtk_container_add (GTK_CONTAINER (window), box);

  clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
  g_signal_connect (clipboard, "changed", G_CALLBACK (clipboard_changed), NULL);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
