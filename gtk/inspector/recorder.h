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

#ifndef _GTK_INSPECTOR_RECORDER_H_
#define _GTK_INSPECTOR_RECORDER_H_

#include <gtk/gtkwidget.h>

#define GTK_TYPE_INSPECTOR_RECORDER            (gtk_inspector_recorder_get_type())
#define GTK_INSPECTOR_RECORDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_RECORDER, GtkInspectorRecorder))
#define GTK_INSPECTOR_IS_RECORDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_RECORDER))

typedef struct _GtkInspectorRecorder GtkInspectorRecorder;

G_BEGIN_DECLS

GType           gtk_inspector_recorder_get_type                 (void);

void            gtk_inspector_recorder_set_recording            (GtkInspectorRecorder   *recorder,
                                                                 gboolean                record);
gboolean        gtk_inspector_recorder_is_recording             (GtkInspectorRecorder   *recorder);

void            gtk_inspector_recorder_set_debug_nodes          (GtkInspectorRecorder   *recorder,
                                                                 gboolean                debug_nodes);

void            gtk_inspector_recorder_record_render            (GtkInspectorRecorder   *recorder,
                                                                 GtkWidget              *widget,
                                                                 GskRenderer            *renderer,
                                                                 GdkSurface             *surface,
                                                                 const cairo_region_t   *region,
                                                                 GskRenderNode          *node);

G_END_DECLS

#endif // _GTK_INSPECTOR_RECORDER_H_

// vim: set et sw=2 ts=2:
