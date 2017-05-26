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

#include "config.h"
#include "gtkeventcontrollerlegacyprivate.h"

G_DEFINE_TYPE (GtkEventControllerLegacy, _gtk_event_controller_legacy,
               GTK_TYPE_EVENT_CONTROLLER)

static gboolean
gtk_event_controller_legacy_handle_event (GtkEventController *controller,
                                          const GdkEvent     *event)
{
  GtkWidget *widget = gtk_event_controller_get_widget (controller);

  return gtk_widget_emit_event_signals (widget, event);
}

static void
_gtk_event_controller_legacy_class_init (GtkEventControllerLegacyClass *klass)
{
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);

  controller_class->handle_event = gtk_event_controller_legacy_handle_event;
}

static void
_gtk_event_controller_legacy_init (GtkEventControllerLegacy *controller)
{
}

GtkEventController *
_gtk_event_controller_legacy_new (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return g_object_new (GTK_TYPE_EVENT_CONTROLLER_LEGACY,
                       "widget", widget,
                       NULL);
}
