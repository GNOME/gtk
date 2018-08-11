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

#include "gtkshortcutcontrollerprivate.h"

#include "gtkeventcontrollerprivate.h"
#include "gtkbindings.h"
#include "gtkshortcut.h"
#include "gtkwidgetprivate.h"

#include <gdk/gdk.h>

struct _GtkShortcutController
{
  GtkEventController parent_instance;

  GSList *shortcuts;

  guint run_class : 1;
};

struct _GtkShortcutControllerClass
{
  GtkEventControllerClass parent_class;
};

G_DEFINE_TYPE (GtkShortcutController, gtk_shortcut_controller,
               GTK_TYPE_EVENT_CONTROLLER)

static void
gtk_shortcut_controller_dispose (GObject *object)
{
  GtkShortcutController *self = GTK_SHORTCUT_CONTROLLER (object);

  g_slist_free_full (self->shortcuts, g_object_unref);
  self->shortcuts = NULL;

  G_OBJECT_CLASS (gtk_shortcut_controller_parent_class)->dispose (object);
}

static gboolean
gtk_shortcut_controller_trigger_shortcut (GtkShortcutController *self,
                                          GtkShortcut           *shortcut,
                                          const GdkEvent        *event)
{
  if (!gtk_shortcut_trigger (shortcut, event))
    return FALSE;

  return gtk_shortcut_activate (shortcut, gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self)));
}

static gboolean
gtk_shortcut_controller_handle_event (GtkEventController *controller,
                                      const GdkEvent     *event)
{
  GtkShortcutController *self = GTK_SHORTCUT_CONTROLLER (controller);
  GtkWidget *widget;
  const GSList *l;

  for (l = self->shortcuts; l; l = l->next)
    {
      if (gtk_shortcut_controller_trigger_shortcut (self, l->data, event))
        return TRUE;
    }

  if (self->run_class)
    {
      widget = gtk_event_controller_get_widget (controller); 

      if (gtk_bindings_activate_event (G_OBJECT (widget), (GdkEventKey *) event))
        return TRUE;

      for (l = gtk_widget_class_get_shortcuts (GTK_WIDGET_GET_CLASS (widget)); l; l = l->next)
        {
          if (gtk_shortcut_controller_trigger_shortcut (self, l->data, event))
            return TRUE;
        }
    }

  return FALSE;
}

static void
gtk_shortcut_controller_class_init (GtkShortcutControllerClass *klass)
{
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_shortcut_controller_dispose;
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

void
gtk_shortcut_controller_set_run_class (GtkShortcutController  *controller,
                                       gboolean                run_class)
{
  controller->run_class = run_class;
}

/**
 * gtk_shortcut_controller_add_shortcut:
 * @self: the controller
 * @shortcut: a #GtkShortcut
 *
 * Adds @shortcut to the list of shortcuts handled by @self.
 *
 * The shortcut is added to the list so that it is triggered before
 * all existing shortcuts.
 * 
 * FIXME: What's supposed to happen if a shortcut gets added twice?
 **/
void
gtk_shortcut_controller_add_shortcut (GtkShortcutController *self,
                                      GtkShortcut           *shortcut)
{
  g_return_if_fail (GTK_IS_SHORTCUT_CONTROLLER (self));
  g_return_if_fail (GTK_IS_SHORTCUT (shortcut));

  g_object_ref (shortcut);
  self->shortcuts = g_slist_prepend (self->shortcuts, shortcut);
}

/**
 * gtk_shortcut_controller_remove_shortcut:
 * @self: the controller
 * @shortcut: a #GtkShortcut
 *
 * Removes @shortcut from the list of shortcuts handled by @self.
 *
 * If @shortcut had not been added to @controller, this function does
 * nothing.
 **/
void
gtk_shortcut_controller_remove_shortcut (GtkShortcutController  *self,
                                         GtkShortcut            *shortcut)
{
  GSList *l;

  g_return_if_fail (GTK_IS_SHORTCUT_CONTROLLER (self));
  g_return_if_fail (GTK_IS_SHORTCUT (shortcut));

  l = g_slist_find (self->shortcuts, shortcut);
  if (l == NULL)
    return;

  self->shortcuts = g_slist_delete_link (self->shortcuts, l);
  g_object_unref (shortcut);
}
