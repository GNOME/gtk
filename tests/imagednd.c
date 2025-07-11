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

static void
deserialize_finish (GObject      *source,
                    GAsyncResult *result,
                    gpointer      data)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GdkContentDeserializer *deserializer = data;
  GValue *value;
  GError *error = NULL;

  g_output_stream_splice_finish (stream, result, &error);
  if (error)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  value = gdk_content_deserializer_get_value (deserializer);
  g_assert (G_VALUE_HOLDS (value, G_TYPE_BYTES));

  g_value_take_boxed (value, g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (stream)));

  gdk_content_deserializer_return_success (deserializer);
}

static void
deserialize_svg_to_bytes (GdkContentDeserializer *deserializer)
{
  GOutputStream *output = g_memory_output_stream_new_resizable ();

  g_output_stream_splice_async (output,
                                gdk_content_deserializer_get_input_stream (deserializer),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                G_PRIORITY_DEFAULT,
                                gdk_content_deserializer_get_cancellable (deserializer),
                                deserialize_finish,
                                deserializer);

  g_object_unref (output);
}

static gboolean
drop_cb (GtkDropTarget *target,
         GValue        *value,
         double         x,
         double         y,
         gpointer       user_data)
{
  GtkWidget *widget;
  GBytes *bytes;
  const char *data;
  gsize length;
  GFile *file;
  GError *error = NULL;

  bytes = g_value_get_boxed (value);
  data = g_bytes_get_data (bytes, &length);

  if (!g_file_set_contents ("dropped.svg", data, length, NULL))
    g_error ("%s", error->message);

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (target));

  file = g_file_new_for_path ("dropped.svg");
  gtk_picture_set_file (GTK_PICTURE (widget), file);
  g_object_unref (file);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *picture;
  gboolean done = FALSE;
  GtkDropTarget *target;

  gtk_init ();

  gdk_content_register_deserializer ("image/svg+xml", G_TYPE_BYTES,
                                     deserialize_svg_to_bytes,
                                     NULL, NULL);

  window = gtk_window_new ();
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

  picture = gtk_picture_new_for_resource ("/org/gtk/libgtk/icons/16x16/status/image-missing.png");

  target = gtk_drop_target_new (G_TYPE_BYTES, GDK_ACTION_COPY);
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
