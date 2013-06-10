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

#include "gtklistboxaccessibleprivate.h"

#include "gtk/gtklistbox.h"

static void atk_selection_interface_init (AtkSelectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkListBoxAccessible, gtk_list_box_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_SELECTION, atk_selection_interface_init))

static void
gtk_list_box_accessible_init (GtkListBoxAccessible *accessible)
{
}

static void
gtk_list_box_accessible_initialize (AtkObject *obj,
                                    gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_list_box_accessible_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_LIST_BOX;
}

static AtkStateSet*
gtk_list_box_accessible_ref_state_set (AtkObject *obj)
{
  AtkStateSet *state_set;
  GtkWidget *widget;

  state_set = ATK_OBJECT_CLASS (gtk_list_box_accessible_parent_class)->ref_state_set (obj);
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));

  if (widget != NULL)
    atk_state_set_add_state (state_set, ATK_STATE_MANAGES_DESCENDANTS);

  return state_set;
}

static void
gtk_list_box_accessible_class_init (GtkListBoxAccessibleClass *klass)
{
  AtkObjectClass *object_class = ATK_OBJECT_CLASS (klass);

  object_class->initialize = gtk_list_box_accessible_initialize;
  object_class->ref_state_set = gtk_list_box_accessible_ref_state_set;
}

static gboolean
gtk_list_box_accessible_add_selection (AtkSelection *selection,
                                       gint          idx)
{
  GtkWidget *box;
  GtkListBoxRow *row;

  box = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (box == NULL)
    return FALSE;

  row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (box), idx);
  if (row)
    {
      gtk_list_box_select_row (GTK_LIST_BOX (box), row);
      return TRUE;
    }
  return FALSE;
}

static gboolean
gtk_list_box_accessible_clear_selection (AtkSelection *selection)
{
  GtkWidget *box;

  box = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (box == NULL)
    return FALSE;

  gtk_list_box_select_row (GTK_LIST_BOX (box), NULL);
  return TRUE;
}

static AtkObject *
gtk_list_box_accessible_ref_selection (AtkSelection *selection,
                                       gint          idx)
{
  GtkWidget *box;
  GtkListBoxRow *row;
  AtkObject *accessible;

  if (idx != 0)
    return NULL;

  box = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (box == NULL)
    return NULL;

  row = gtk_list_box_get_selected_row (GTK_LIST_BOX (box));
  if (row == NULL)
    return NULL;

  accessible = gtk_widget_get_accessible (GTK_WIDGET (row));
  g_object_ref (accessible);
  return accessible;
}

static gint
gtk_list_box_accessible_get_selection_count (AtkSelection *selection)
{
  GtkWidget *box;
  GtkListBoxRow *row;

  box = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (box == NULL)
    return 0;

  row = gtk_list_box_get_selected_row (GTK_LIST_BOX (box));
  if (row == NULL)
    return 0;

  return 1;
}

static gboolean
gtk_list_box_accessible_is_child_selected (AtkSelection *selection,
                                           gint          idx)
{
  GtkWidget *box;
  GtkListBoxRow *row;

  box = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (box == NULL)
    return FALSE;

  row = gtk_list_box_get_selected_row (GTK_LIST_BOX (box));
  if (row == NULL)
    return FALSE;

  return row == gtk_list_box_get_row_at_index (GTK_LIST_BOX (box), idx);
}

static void atk_selection_interface_init (AtkSelectionIface *iface)
{
  iface->add_selection = gtk_list_box_accessible_add_selection;
  iface->clear_selection = gtk_list_box_accessible_clear_selection;
  iface->ref_selection = gtk_list_box_accessible_ref_selection;
  iface->get_selection_count = gtk_list_box_accessible_get_selection_count;
  iface->is_child_selected = gtk_list_box_accessible_is_child_selected;
}

void
_gtk_list_box_accessible_selection_changed (GtkListBox *box)
{
  AtkObject *accessible;
  accessible = gtk_widget_get_accessible (GTK_WIDGET (box));
  g_signal_emit_by_name (accessible, "selection-changed");
}

void
_gtk_list_box_accessible_update_cursor (GtkListBox *box,
                                        GtkListBoxRow *row)
{
  AtkObject *accessible;
  AtkObject *descendant;
  accessible = gtk_widget_get_accessible (GTK_WIDGET (box));
  descendant = row ? gtk_widget_get_accessible (GTK_WIDGET (row)) : NULL;
  g_signal_emit_by_name (accessible, "active-descendant-changed", descendant);
}
