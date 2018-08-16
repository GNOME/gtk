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

#include "config.h"

#include "gtkshortcutmanager.h"

G_DEFINE_INTERFACE (GtkShortcutManager, gtk_shortcut_manager, G_TYPE_OBJECT)

static void
complain_if_reached (gpointer should_be_gone)
{
  g_critical ("Shortcut controllers failed to clean up.");
}

static void
gtk_shortcut_manager_default_add_controller (GtkShortcutManager    *self,
                                             GtkShortcutController *controller)
{
  GSList *controllers;

  controllers = g_object_steal_data (G_OBJECT (self), "gtk-shortcut-controllers");
  controllers = g_slist_prepend (controllers, g_object_ref (controller));
  g_object_set_data_full (G_OBJECT (self), "gtk-shortcut-controllers", controllers, complain_if_reached);
}

static void
gtk_shortcut_manager_default_remove_controller (GtkShortcutManager    *self,
                                                GtkShortcutController *controller)
{
  GSList *controllers;

  controllers = g_object_steal_data (G_OBJECT (self), "gtk-shortcut-controllers");
  controllers = g_slist_remove (controllers, controller);
  if (controllers)
    g_object_set_data_full (G_OBJECT (self), "gtk-shortcut-controllers", controllers, complain_if_reached);
  g_object_unref (controller);
}

static void
gtk_shortcut_manager_default_init (GtkShortcutManagerInterface *iface)
{
  iface->add_controller = gtk_shortcut_manager_default_add_controller;
  iface->remove_controller = gtk_shortcut_manager_default_remove_controller;
}

