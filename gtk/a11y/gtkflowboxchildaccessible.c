/*
 * Copyright (C) 2013 Red Hat, Inc.
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
 */

#include "config.h"

#include "gtkflowboxchildaccessible.h"

#include "gtk/gtkflowbox.h"


G_DEFINE_TYPE (GtkFlowBoxChildAccessible, gtk_flow_box_child_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE)

static void
gtk_flow_box_child_accessible_init (GtkFlowBoxChildAccessible *accessible)
{
}

static void
gtk_flow_box_child_accessible_initialize (AtkObject *obj,
                                          gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_flow_box_child_accessible_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_TABLE_CELL;
}

static AtkStateSet *
gtk_flow_box_child_accessible_ref_state_set (AtkObject *obj)
{
  AtkStateSet *state_set;
  GtkWidget *widget, *parent;

  state_set = ATK_OBJECT_CLASS (gtk_flow_box_child_accessible_parent_class)->ref_state_set (obj);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget != NULL)
    {
      parent = gtk_widget_get_parent (widget);
      if (gtk_flow_box_get_selection_mode (GTK_FLOW_BOX (parent)) != GTK_SELECTION_NONE)
        atk_state_set_add_state (state_set, ATK_STATE_SELECTABLE);

      if (gtk_flow_box_child_is_selected (GTK_FLOW_BOX_CHILD (widget)))
        atk_state_set_add_state (state_set, ATK_STATE_SELECTED);
    }

  return state_set;
}

static void
gtk_flow_box_child_accessible_class_init (GtkFlowBoxChildAccessibleClass *klass)
{
  AtkObjectClass *object_class = ATK_OBJECT_CLASS (klass);

  object_class->initialize = gtk_flow_box_child_accessible_initialize;
  object_class->ref_state_set = gtk_flow_box_child_accessible_ref_state_set;
}
