/* GTK - The GIMP Toolkit
 * Copyright (C) 2026, Red Hat, Inc.
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

#include "config.h"

#include "gtksvgeventcontrollerprivate.h"
#include "gtkeventcontrollerprivate.h"
#include "gtksvgprivate.h"

struct _GtkSvgEventController
{
  GtkEventController parent_instance;
  GtkSvg *svg;
};

struct _GtkSvgEventControllerClass
{
  GtkEventControllerClass parent_class;
};

G_DEFINE_TYPE (GtkSvgEventController, gtk_svg_event_controller, GTK_TYPE_EVENT_CONTROLLER)

static gboolean
gtk_svg_event_controller_handle_event (GtkEventController *controller,
                                       GdkEvent           *event,
                                       double              x,
                                       double              y)
{
  GtkSvgEventController *self = GTK_SVG_EVENT_CONTROLLER (controller);
  return gtk_svg_handle_event (self->svg, event, x, y);
}

static void
gtk_svg_event_controller_handle_crossing (GtkEventController    *controller,
                                          const GtkCrossingData *crossing,
                                          double                 x,
                                          double                 y)
{
  GtkSvgEventController *self = GTK_SVG_EVENT_CONTROLLER (controller);
  gtk_svg_handle_crossing (self->svg, crossing, x, y);
}

static void
gtk_svg_event_controller_class_init (GtkSvgEventControllerClass *klass)
{
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);

  controller_class->handle_event = gtk_svg_event_controller_handle_event;
  controller_class->handle_crossing = gtk_svg_event_controller_handle_crossing;
}

static void
gtk_svg_event_controller_init (GtkSvgEventController *controller)
{
}

GtkEventController *
gtk_svg_event_controller_new (GtkSvg *svg)
{
  GtkSvgEventController *self = g_object_new (gtk_svg_event_controller_get_type (), NULL);
  self->svg = svg;
  return GTK_EVENT_CONTROLLER (self);
}
