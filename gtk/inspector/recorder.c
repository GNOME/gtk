/*
 * Copyright (c) 2016 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#include "recorder.h"

#include <gtk/gtklabel.h>
#include <gtk/gtklistbox.h>

#include "recording.h"
#include "renderrecording.h"

struct _GtkInspectorRecorderPrivate
{
  GListStore *recordings;
  GtkWidget *recordings_list;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorRecorder, gtk_inspector_recorder, GTK_TYPE_BIN)

static GtkWidget *
gtk_inspector_recorder_recordings_list_create_widget (gpointer item,
                                                      gpointer user_data)
{
  GtkInspectorRecording *recording = GTK_INSPECTOR_RECORDING (item);
  GtkWidget *widget;
  char *str;

  str = g_strdup_printf ("Cute drawing at time %lld", (long long) gtk_inspector_recording_get_timestamp (recording));
  widget = gtk_label_new (str);
  g_free (str);
  gtk_widget_show_all (widget);

  return widget;
}

static void
gtk_inspector_recorder_constructed (GObject *object)
{
  GtkInspectorRecorder *recorder = GTK_INSPECTOR_RECORDER (object);
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);

  G_OBJECT_CLASS (gtk_inspector_recorder_parent_class)->constructed (object);

  gtk_list_box_bind_model (GTK_LIST_BOX (priv->recordings_list),
                           G_LIST_MODEL (priv->recordings),
                           gtk_inspector_recorder_recordings_list_create_widget,
                           NULL,
                           NULL);
}

static void
gtk_inspector_recorder_class_init (GtkInspectorRecorderClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gtk_inspector_recorder_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/recorder.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorRecorder, recordings);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorRecorder, recordings_list);
}

static void
gtk_inspector_recorder_init (GtkInspectorRecorder *vis)
{
  gtk_widget_init_template (GTK_WIDGET (vis));
}

void
gtk_inspector_recorder_record_render (GtkInspectorRecorder *recorder,
                                      GtkWidget            *widget,
                                      GdkWindow            *window,
                                      const cairo_region_t *region,
                                      GskRenderNode        *node)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);
  GtkInspectorRecording *recording;
  GdkFrameClock *frame_clock;

  frame_clock = gtk_widget_get_frame_clock (widget);

  recording = gtk_inspector_render_recording_new (gdk_frame_clock_get_frame_time (frame_clock),
                                                  &(GdkRectangle) { 0, 0,
                                                    gdk_window_get_width (window),
                                                    gdk_window_get_height (window) },
                                                  region,
                                                  node);
  g_list_store_append (priv->recordings, recording);
  g_object_unref (recording);
}

// vim: set et sw=2 ts=2:
