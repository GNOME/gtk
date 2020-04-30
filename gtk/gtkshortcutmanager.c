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
#include "gtkshortcutmanagerprivate.h"
#include "gtkflattenlistmodel.h"

/**
 * SECTION:gtkshortcutmanager
 * @title: GtkShortcutManager
 * @short_description: Interface for managing shortcuts
 *
 * The GtkShortcutManager interface is used to implement
 * shortcut scopes.
 *
 * This is important for #GtkNative widgets that have their own surface,
 * since the event controllers that are used to implement managed and
 * global scopes are limited to the same native.
 *
 * Examples for widgets implementing #GtkShortcutManager are #GtkWindow
 * and #GtkPopover.
 */

G_DEFINE_INTERFACE (GtkShortcutManager, gtk_shortcut_manager, G_TYPE_OBJECT)

void
gtk_shortcut_manager_create_controllers (GtkWidget *widget)
{
  GListStore *store;
  GtkFlattenListModel *model;
  GtkEventController *controller;

  store = g_list_store_new (GTK_TYPE_SHORTCUT_CONTROLLER);
  model = gtk_flatten_list_model_new (GTK_TYPE_SHORTCUT, G_LIST_MODEL (store));
  g_object_unref (store);
  g_object_set_data_full (G_OBJECT (widget), "gtk-shortcut-manager-bubble", model, g_object_unref);
  controller = gtk_shortcut_controller_new_for_model (G_LIST_MODEL (model));
  gtk_event_controller_set_name (controller, "gtk-shortcut-manager-bubble");
  gtk_widget_add_controller (widget, controller);

  store = g_list_store_new (GTK_TYPE_SHORTCUT_CONTROLLER);
  model = gtk_flatten_list_model_new (GTK_TYPE_SHORTCUT, G_LIST_MODEL (store));
  g_object_unref (store);
  g_object_set_data_full (G_OBJECT (widget), "gtk-shortcut-manager-capture", model, g_object_unref);
  controller = gtk_shortcut_controller_new_for_model (G_LIST_MODEL (model));
  gtk_event_controller_set_name (controller, "gtk-shortcut-manager-capture");
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (widget, controller);
}

static GtkFlattenListModel *
gtk_shortcut_manager_get_model (GtkShortcutManager  *self,
                                GtkPropagationPhase  phase)
{
  switch (phase)
    {
    case GTK_PHASE_CAPTURE:
      return g_object_get_data (G_OBJECT (self), "gtk-shortcut-manager-capture");
    case GTK_PHASE_BUBBLE:
      return g_object_get_data (G_OBJECT (self), "gtk-shortcut-manager-bubble");
    case GTK_PHASE_NONE:
    case GTK_PHASE_TARGET:
      return NULL;
    default:
      g_assert_not_reached ();
      return NULL;
    }
}

static void
gtk_shortcut_manager_default_add_controller (GtkShortcutManager    *self,
                                             GtkShortcutController *controller)
{
  GtkFlattenListModel *model;
  GtkPropagationPhase phase;

  phase = gtk_event_controller_get_propagation_phase (GTK_EVENT_CONTROLLER (controller));
  model = gtk_shortcut_manager_get_model (self, phase);
  if (model)
    {
      GListModel *store = gtk_flatten_list_model_get_model (model); 
      g_list_store_append (G_LIST_STORE (store), controller);
    }
}

static void
gtk_shortcut_manager_default_remove_controller (GtkShortcutManager    *self,
                                                GtkShortcutController *controller)
{
  GtkFlattenListModel *model;
  GtkPropagationPhase phase;

  phase = gtk_event_controller_get_propagation_phase (GTK_EVENT_CONTROLLER (controller));
  model = gtk_shortcut_manager_get_model (self, phase);
  if (model)
    {
      GListModel *store;
      guint position, len;

      store = gtk_flatten_list_model_get_model (model);
#if 0 && GLIB_CHECK_VERSION(2,64,0)
      if (_g_list_store_find (G_LIST_STORE (store), controller, &position))
        g_list_store_remove (G_LIST_STORE (store), position);
#else
      for (position = 0, len = g_list_model_get_n_items (store); position < len; position++)
        {
          GtkShortcutController *item = g_list_model_get_item (store, position);
          g_object_unref (item);
          if (item == controller)
            {
              g_list_store_remove (G_LIST_STORE (store), position);
              break;
            }
        }
#endif
    }
}

static void
gtk_shortcut_manager_default_init (GtkShortcutManagerInterface *iface)
{
  iface->add_controller = gtk_shortcut_manager_default_add_controller;
  iface->remove_controller = gtk_shortcut_manager_default_remove_controller;
}

