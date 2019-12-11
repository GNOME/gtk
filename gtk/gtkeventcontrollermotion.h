/* GTK - The GIMP Toolkit
 * Copyright (C) 2017, Red Hat, Inc.
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
 *
 * Author(s): Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __GTK_EVENT_CONTROLLER_MOTION_H__
#define __GTK_EVENT_CONTROLLER_MOTION_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gtk/gtkeventcontroller.h>

G_BEGIN_DECLS

#define GTK_TYPE_EVENT_CONTROLLER_MOTION         (gtk_event_controller_motion_get_type ())
GDK_AVAILABLE_IN_ALL
GDK_DECLARE_EXPORTED_TYPE (GtkEventControllerMotion, gtk_event_controller_motion, GTK, EVENT_CONTROLLER_MOTION)

GDK_AVAILABLE_IN_ALL
GtkEventController *gtk_event_controller_motion_new      (void);

GDK_AVAILABLE_IN_ALL
gboolean            gtk_event_controller_motion_contains_pointer   (GtkEventControllerMotion *self);
GDK_AVAILABLE_IN_ALL
gboolean            gtk_event_controller_motion_is_pointer         (GtkEventControllerMotion *self);

G_END_DECLS

#endif /* __GTK_EVENT_CONTROLLER_MOTION_H__ */
