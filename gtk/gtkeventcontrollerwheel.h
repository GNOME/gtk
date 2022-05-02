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
 * Author(s): Carlos Garnacho <carlosg@gnome.org>
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gtk/gtkeventcontroller.h>

G_BEGIN_DECLS

#define GTK_TYPE_EVENT_CONTROLLER_WHEEL         (gtk_event_controller_wheel_get_type ())
#define GTK_EVENT_CONTROLLER_WHEEL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_EVENT_CONTROLLER_WHEEL, GtkEventControllerWheel))
#define GTK_EVENT_CONTROLLER_WHEEL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_EVENT_CONTROLLER_WHEEL, GtkEventControllerWheelClass))
#define GTK_IS_EVENT_CONTROLLER_WHEEL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_EVENT_CONTROLLER_WHEEL))
#define GTK_IS_EVENT_CONTROLLER_WHEEL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_EVENT_CONTROLLER_WHEEL))
#define GTK_EVENT_CONTROLLER_WHEEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_EVENT_CONTROLLER_WHEEL, GtkEventControllerWheelClass))

typedef struct _GtkEventControllerWheel GtkEventControllerWheel;
typedef struct _GtkEventControllerWheelClass GtkEventControllerWheelClass;

GDK_AVAILABLE_IN_4_8
GType               gtk_event_controller_wheel_get_type  (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_8
GtkEventController *gtk_event_controller_wheel_new       (void);

GDK_AVAILABLE_IN_4_8
GdkScrollUnit       gtk_event_controller_wheel_get_unit (GtkEventControllerWheel *self);

G_END_DECLS
