/*
 * Copyright 2025 Red Hat, Inc
 * Author: Matthias Clasen
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

typedef struct
{
  GtkWidget *picture;
  GdkDrop *drop;
  GFile *file;
} DropData;

static void
drop_data_free (DropData *data)
{
  g_object_unref (data->drop);
  g_object_unref (data->file);
  g_free (data);
}

static void
save_finish (GObject      *source,
             GAsyncResult *result,
             gpointer      user_data)
{
  GOutputStream *output = G_OUTPUT_STREAM (source);
  DropData *data = user_data;
  GError *error = NULL;

  g_output_stream_splice_finish (output, result, &error);
  if (error)
    {
      g_print ("Saving failed: %s\n", error->message);
      g_error_free (error);
      gdk_drop_finish (data->drop, GDK_ACTION_NONE);
      drop_data_free (data);
      return;
    }

  gtk_picture_set_file (GTK_PICTURE (data->picture), data->file);

  gdk_drop_finish (data->drop, GDK_ACTION_COPY);
  drop_data_free (data);
}

static void
drop_done (GObject      *source,
           GAsyncResult *result,
           gpointer      user_data)
{
  GdkDrop *drop = GDK_DROP (source);
  GtkWidget *picture = user_data;
  const char *mime_type;
  GInputStream *input;
  GFile *file;
  GOutputStream *output;
  DropData *data;
  GError *error = NULL;

  input = gdk_drop_read_finish (drop, result, &mime_type, &error);
  if (!input)
    {
      g_print ("Drop failed: %s\n", error->message);
      g_error_free (error);
      gdk_drop_finish (drop, GDK_ACTION_NONE);
      return;
    }

  if (strcmp (mime_type, "image/svg+xml") == 0)
    file = g_file_new_for_path ("dropped.svg");
  else if (strcmp (mime_type, "image/png") == 0)
    file = g_file_new_for_path ("dropped.png");
  else if (strcmp (mime_type, "image/jpeg") == 0)
    file = g_file_new_for_path ("dropped.jpeg");
  else if (strcmp (mime_type, "image/tiff") == 0)
    file = g_file_new_for_path ("dropped.tiff");
  else
    g_assert_not_reached ();

  output = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error));
  if (!output)
    {
      g_print ("Saving failed: %s\n", error->message);
      g_clear_error (&error);
      g_clear_object (&input);
      gdk_drop_finish (drop, GDK_ACTION_NONE);
      return;
    }

  data = g_new (DropData, 1);
  data->drop = g_object_ref (drop);
  data->picture = picture;
  data->file = g_object_ref (file);

  g_output_stream_splice_async (output, input,
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                G_PRIORITY_DEFAULT,
                                NULL,
                                save_finish,
                                data);

  g_object_unref (output);
  g_object_unref (file);
}

static const char *mime_types[] = {
  "image/svg+xml",
  "image/png",
  "image/jpeg",
  "image/tiff",
  NULL
};

static gboolean
drop_cb (GtkEventController *controller,
         GdkDrop            *drop,
         double              x,
         double              y,
         gpointer            user_data)
{
  gdk_drop_read_async (drop,
                       mime_types,
                       G_PRIORITY_DEFAULT,
                       NULL,
                       drop_done,
                       gtk_event_controller_get_widget (controller));

  return TRUE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *picture;
  gboolean done = FALSE;
  GdkContentFormats *formats;
  GtkDropTargetAsync *target;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

  picture = gtk_picture_new_for_resource ("/org/gtk/libgtk/icons/16x16/status/image-missing.png");

  formats = gdk_content_formats_new (mime_types, G_N_ELEMENTS (mime_types) - 1);
  target = gtk_drop_target_async_new (formats, GDK_ACTION_COPY);
  gdk_content_formats_unref (formats);

  g_signal_connect (target, "drop", G_CALLBACK (drop_cb), NULL);
  gtk_widget_add_controller (picture, GTK_EVENT_CONTROLLER (target));

  gtk_widget_set_margin_top (picture, 10);
  gtk_widget_set_margin_bottom (picture, 10);
  gtk_widget_set_margin_start (picture, 10);
  gtk_widget_set_margin_end (picture, 10);

  gtk_window_set_child (GTK_WINDOW (window), picture);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
