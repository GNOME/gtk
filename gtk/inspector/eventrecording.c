/*
 * Copyright (c) 2021 Red Hat, Inc.
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
#include <gtk/gtknative.h>

#include "eventrecording.h"

G_DEFINE_TYPE (GtkInspectorEventRecording, gtk_inspector_event_recording, GTK_TYPE_INSPECTOR_RECORDING)

static void
gtk_inspector_event_recording_finalize (GObject *object)
{
  GtkInspectorEventRecording *recording = GTK_INSPECTOR_EVENT_RECORDING (object);

  g_clear_pointer (&recording->event, gdk_event_unref);
  g_array_unref (recording->traces);

  G_OBJECT_CLASS (gtk_inspector_event_recording_parent_class)->finalize (object);
}

static void
gtk_inspector_event_recording_class_init (GtkInspectorEventRecordingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_inspector_event_recording_finalize;
}

static void
gtk_inspector_event_recording_init (GtkInspectorEventRecording *vis)
{
}

GtkInspectorRecording *
gtk_inspector_event_recording_new (gint64    timestamp,
                                   GdkEvent *event)
{
  GtkInspectorEventRecording *recording;

  recording = g_object_new (GTK_TYPE_INSPECTOR_EVENT_RECORDING,
                            "timestamp", timestamp,
                            NULL);

  recording->event = gdk_event_ref (event);
  recording->target_type = G_TYPE_INVALID;
  recording->traces = g_array_new (FALSE, FALSE, sizeof (EventTrace));

  return GTK_INSPECTOR_RECORDING (recording);
}

GdkEvent *
gtk_inspector_event_recording_get_event (GtkInspectorEventRecording *recording)
{
  return recording->event;
}

void
gtk_inspector_event_recording_add_trace (GtkInspectorEventRecording *recording,
                                         GtkPropagationPhase         phase,
                                         GtkWidget                  *widget,
                                         GtkEventController         *controller,
                                         GtkWidget                  *target,
                                         gboolean                    handled)
{
  EventTrace trace;
  GtkNative *native;
  double x, y;

  trace.phase = phase;
  trace.widget_type = G_OBJECT_TYPE (widget);
  trace.controller_type = G_OBJECT_TYPE (controller);
  trace.handled = handled;

  native = gtk_widget_get_native (widget),
  gtk_native_get_surface_transform (native, &x, &y);

  if (!gtk_widget_compute_bounds (widget, GTK_WIDGET (native), &trace.bounds))
    return;

  trace.bounds.origin.x += x;
  trace.bounds.origin.y += y;

  if (recording->target_type == G_TYPE_INVALID)
    {
      recording->target_type = G_OBJECT_TYPE (target);

      if (!gtk_widget_compute_bounds (target, GTK_WIDGET (native), &recording->bounds))
        return;

      recording->bounds.origin.x += x;
      recording->bounds.origin.y += y;
    }

  g_array_append_val (recording->traces, trace);
}

EventTrace *
gtk_inspector_event_recording_get_traces (GtkInspectorEventRecording *recording,
                                          gsize                      *n_traces)
{
  *n_traces = recording->traces->len;

  return (EventTrace *) recording->traces->data;
}

GType
gtk_inspector_event_recording_get_target_type (GtkInspectorEventRecording *recording)
{
  return recording->target_type;
}

void
gtk_inspector_event_recording_get_target_bounds (GtkInspectorEventRecording *recording,
                                                 graphene_rect_t *bounds)
{
  *bounds = recording->bounds;
}


// vim: set et sw=2 ts=2:
