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

#include "gtkconcatmodelprivate.h"

G_DEFINE_INTERFACE (GtkShortcutManager, gtk_shortcut_manager, G_TYPE_OBJECT)

static GtkConcatModel *
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
  GtkConcatModel *model;

  model = gtk_shortcut_manager_get_model (self, 
                                          gtk_event_controller_get_propagation_phase (GTK_EVENT_CONTROLLER (controller)));
  if (model)
    gtk_concat_model_append (model, G_LIST_MODEL (controller));
}

static void
gtk_shortcut_manager_default_remove_controller (GtkShortcutManager    *self,
                                                GtkShortcutController *controller)
{
  GtkConcatModel *model;

  model = gtk_shortcut_manager_get_model (self, 
                                          gtk_event_controller_get_propagation_phase (GTK_EVENT_CONTROLLER (controller)));
  if (model)
    gtk_concat_model_remove (model, G_LIST_MODEL (controller));
}

static void
gtk_shortcut_manager_default_init (GtkShortcutManagerInterface *iface)
{
  iface->add_controller = gtk_shortcut_manager_default_add_controller;
  iface->remove_controller = gtk_shortcut_manager_default_remove_controller;
}

