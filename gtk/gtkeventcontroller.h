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

typedef struct _GtkEventControllerClass GtkEventControllerClass;

#include <gdk/gdk.h>
#include <gtk/gtktypes.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

#define GTK_TYPE_EVENT_CONTROLLER         (gtk_event_controller_get_type ())
#define GTK_EVENT_CONTROLLER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_EVENT_CONTROLLER, GtkEventController))
#define GTK_EVENT_CONTROLLER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_EVENT_CONTROLLER, GtkEventControllerClass))
#define GTK_IS_EVENT_CONTROLLER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_EVENT_CONTROLLER))
#define GTK_IS_EVENT_CONTROLLER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_EVENT_CONTROLLER))
#define GTK_EVENT_CONTROLLER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_EVENT_CONTROLLER, GtkEventControllerClass))


typedef struct _GtkCrossingEvent GtkCrossingEvent;

/**
 * GtkCrossingEvent:
 * @in: whether this is a focus-in or focus-out event
 * @old_location: The old focus location
 * @new_location: The new focus location
 *
 * The struct that is passed to gtk_event_controller_handle_focus_change()
 * and is also passed to #GtkEventController::focus-change.
 *
 * The @old_location and @new_location fields are set to the old or new
 * location or to %NULL if the widgets are not descencents of the current
 * widget.
 */
struct _GtkCrossingEvent {
  gboolean in;
  GtkWidget *old_location;
  GtkWidget *new_location;
};

#define GTK_TYPE_CROSSING_EVENT (gtk_crossing_event_get_type ())

GDK_AVAILABLE_IN_ALL
GType               gtk_crossing_event_get_type (void) G_GNUC_CONST;


GDK_AVAILABLE_IN_ALL
GType        gtk_event_controller_get_type       (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkWidget  * gtk_event_controller_get_widget     (GtkEventController *controller);

GDK_AVAILABLE_IN_ALL
gboolean     gtk_event_controller_handle_event   (GtkEventController *controller,
                                                  const GdkEvent     *event,
                                                  double              x,
                                                  double              y);
GDK_AVAILABLE_IN_ALL
void         gtk_event_controller_handle_focus_change (GtkEventController     *controller,
                                                       const GtkCrossingEvent *event);
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
