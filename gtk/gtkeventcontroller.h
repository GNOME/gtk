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


typedef struct _GtkCrossingData GtkCrossingData;

/**
 * GtkCrossingData:
 * @type: the type of crossing event
 * @direction: whether this is a focus-in or focus-out event
 * @mode: the crossing mode
 * @old_target: the old target
 * @old_descendent: the direct child of the receiving widget that
 *     is an ancestor of @old_target, or %NULL if @old_target is not
 *     a descendent of the receiving widget
 * @new_target: the new target
 * @new_descendent: the direct child of the receiving widget that
 *     is an ancestor of @new_target, or %NULL if @new_target is not
 *     a descendent of the receiving widget
 *
 * The struct that is passed to gtk_event_controller_handle_crossing().
 *
 * The @old_target and @new_target fields are set to the old or new
 * focus or hover location.
 */
struct _GtkCrossingData {
  GtkCrossingType type;
  GtkCrossingDirection direction;
  GdkCrossingMode mode;
  GtkWidget *old_target;
  GtkWidget *old_descendent;
  GtkWidget *new_target;
  GtkWidget *new_descendent;
};

GDK_AVAILABLE_IN_ALL
GType               gtk_crossing_data_get_type (void) G_GNUC_CONST;


GDK_AVAILABLE_IN_ALL
GType        gtk_event_controller_get_type       (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkWidget  * gtk_event_controller_get_widget     (GtkEventController *controller);

GDK_AVAILABLE_IN_ALL
gboolean     gtk_event_controller_handle_event   (GtkEventController *controller,
                                                  GdkEvent           *event,
                                                  GtkWidget          *target,
                                                  double              x,
                                                  double              y);
GDK_AVAILABLE_IN_ALL
void         gtk_event_controller_handle_crossing (GtkEventController    *controller,
                                                   const GtkCrossingData *crossing,
                                                   double                 x,
                                                   double                 y);
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
