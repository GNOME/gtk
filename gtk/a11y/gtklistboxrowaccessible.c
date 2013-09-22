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

#include "gtklistboxrowaccessible.h"

#include "gtk/gtklistbox.h"


G_DEFINE_TYPE (GtkListBoxRowAccessible, gtk_list_box_row_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE)

static void
gtk_list_box_row_accessible_init (GtkListBoxRowAccessible *accessible)
{
}

static void
gtk_list_box_row_accessible_initialize (AtkObject *obj,
                                        gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_list_box_row_accessible_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_LIST_ITEM;
}

static AtkStateSet*
gtk_list_box_row_accessible_ref_state_set (AtkObject *obj)
{
  AtkStateSet *state_set;
  GtkWidget *widget, *parent;

  state_set = ATK_OBJECT_CLASS (gtk_list_box_row_accessible_parent_class)->ref_state_set (obj);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget != NULL)
    {
      parent = gtk_widget_get_parent (widget);
      if (gtk_list_box_get_selection_mode (GTK_LIST_BOX (parent)) != GTK_SELECTION_NONE)
        atk_state_set_add_state (state_set, ATK_STATE_SELECTABLE);

      if (widget == (GtkWidget*)gtk_list_box_get_selected_row (GTK_LIST_BOX (parent)))
        atk_state_set_add_state (state_set, ATK_STATE_SELECTED);
    }

  return state_set;
}

static void
gtk_list_box_row_accessible_class_init (GtkListBoxRowAccessibleClass *klass)
{
  AtkObjectClass *object_class = ATK_OBJECT_CLASS (klass);

  object_class->initialize = gtk_list_box_row_accessible_initialize;
  object_class->ref_state_set = gtk_list_box_row_accessible_ref_state_set;
}
