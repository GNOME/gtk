/* GTK - The GIMP Toolkit
 * Copyright (C) 2012, One Laptop Per Child.
 * Copyright (C) 2014, Red Hat, Inc.
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
#ifndef __GTK_EVENT_CONTROLLER_H__
#define __GTK_EVENT_CONTROLLER_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gtk/gtktypes.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

#define GTK_TYPE_EVENT_CONTROLLER         (gtk_event_controller_get_type ())
GDK_AVAILABLE_IN_ALL
GDK_DECLARE_EXPORTED_TYPE (GtkEventController, gtk_event_controller, GTK, EVENT_CONTROLLER)

GDK_AVAILABLE_IN_ALL
GtkWidget  * gtk_event_controller_get_widget     (GtkEventController *controller);

GDK_AVAILABLE_IN_ALL
gboolean     gtk_event_controller_handle_event   (GtkEventController *controller,
                                                  const GdkEvent     *event);
GDK_AVAILABLE_IN_ALL
void         gtk_event_controller_reset          (GtkEventController *controller);

GDK_AVAILABLE_IN_ALL
GtkPropagationPhase gtk_event_controller_get_propagation_phase (GtkEventController *controller);

GDK_AVAILABLE_IN_ALL
void                gtk_event_controller_set_propagation_phase (GtkEventController  *controller,
                                                                GtkPropagationPhase  phase);

GDK_AVAILABLE_IN_ALL
GtkPropagationLimit gtk_event_controller_get_propagation_limit (GtkEventController *controller);

GDK_AVAILABLE_IN_ALL
void                gtk_event_controller_set_propagation_limit (GtkEventController  *controller,
                                                                GtkPropagationLimit  limit);
GDK_AVAILABLE_IN_ALL
const char *        gtk_event_controller_get_name              (GtkEventController *controller);
GDK_AVAILABLE_IN_ALL
void                gtk_event_controller_set_name              (GtkEventController *controller,
                                                                const char         *name);

G_END_DECLS

#endif /* __GTK_EVENT_CONTROLLER_H__ */
