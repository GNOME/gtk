/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */


/**
 * SECTION:gtkshortcutcontroller
 * @Short_description: Event controller for shortcuts
 * @Title: GtkShortcutController
 * @See_also: #GtkEventController, #GtkShortcut
 *
 * #GtkShortcutController is an event controller that manages shortcuts.
 **/

#include "config.h"

#include "gtkshortcutcontroller.h"

#include "gtkeventcontrollerprivate.h"
#include "gtkbindings.h"

#include <gdk/gdk.h>

struct _GtkShortcutController
{
  GtkEventController parent_instance;
};

struct _GtkShortcutControllerClass
{
  GtkEventControllerClass parent_class;
};

G_DEFINE_TYPE (GtkShortcutController, gtk_shortcut_controller,
               GTK_TYPE_EVENT_CONTROLLER)

static void
gtk_shortcut_controller_finalize (GObject *object)
{
  //GtkShortcutController *self = GTK_SHORTCUT_CONTROLLER (object);

  G_OBJECT_CLASS (gtk_shortcut_controller_parent_class)->finalize (object);
}

static gboolean
gtk_shortcut_controller_handle_event (GtkEventController *controller,
                                      const GdkEvent     *event)
{
  return gtk_bindings_activate_event (G_OBJECT (gtk_event_controller_get_widget (controller)),
                                      (GdkEventKey *) event);
}

static void
gtk_shortcut_controller_class_init (GtkShortcutControllerClass *klass)
{
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_shortcut_controller_finalize;
  controller_class->handle_event = gtk_shortcut_controller_handle_event;
}

static void
gtk_shortcut_controller_init (GtkShortcutController *controller)
{
}

GtkEventController *
gtk_shortcut_controller_new (void)
{
  return g_object_new (GTK_TYPE_SHORTCUT_CONTROLLER,
                       NULL);
}
