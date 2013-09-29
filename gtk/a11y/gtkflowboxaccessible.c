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

#include "gtkflowboxaccessibleprivate.h"

#include "gtk/gtkflowbox.h"

static void atk_selection_interface_init (AtkSelectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkFlowBoxAccessible, gtk_flow_box_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_SELECTION, atk_selection_interface_init))

static void
gtk_flow_box_accessible_init (GtkFlowBoxAccessible *accessible)
{
}

static void
gtk_flow_box_accessible_initialize (AtkObject *obj,
                                    gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_flow_box_accessible_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_TABLE;
}

static AtkStateSet*
gtk_flow_box_accessible_ref_state_set (AtkObject *obj)
{
  AtkStateSet *state_set;
  GtkWidget *widget;

  state_set = ATK_OBJECT_CLASS (gtk_flow_box_accessible_parent_class)->ref_state_set (obj);
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));

  if (widget != NULL)
    atk_state_set_add_state (state_set, ATK_STATE_MANAGES_DESCENDANTS);

  return state_set;
}

static void
gtk_flow_box_accessible_class_init (GtkFlowBoxAccessibleClass *klass)
{
  AtkObjectClass *object_class = ATK_OBJECT_CLASS (klass);

  object_class->initialize = gtk_flow_box_accessible_initialize;
  object_class->ref_state_set = gtk_flow_box_accessible_ref_state_set;
}

static gboolean
gtk_flow_box_accessible_add_selection (AtkSelection *selection,
                                       gint          idx)
{
  GtkWidget *box;
  GList *children;
  GtkWidget *child;

  box = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (box == NULL)
    return FALSE;

  children = gtk_container_get_children (GTK_CONTAINER (box));
  child = g_list_nth_data (children, idx);
  g_list_free (children);
  if (child)
    {
      gtk_flow_box_select_child (GTK_FLOW_BOX (box), GTK_FLOW_BOX_CHILD (child));
      return TRUE;
    }
  return FALSE;
}

static gboolean
gtk_flow_box_accessible_remove_selection (AtkSelection *selection,
                                          gint          idx)
{
  GtkWidget *box;
  GList *children;
  GtkWidget *child;

  box = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (box == NULL)
    return FALSE;

  children = gtk_container_get_children (GTK_CONTAINER (box));
  child = g_list_nth_data (children, idx);
  g_list_free (children);
  if (child)
    {
      gtk_flow_box_unselect_child (GTK_FLOW_BOX (box), GTK_FLOW_BOX_CHILD (child));
      return TRUE;
    }
  return FALSE;
}

static gboolean
gtk_flow_box_accessible_clear_selection (AtkSelection *selection)
{
  GtkWidget *box;

  box = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (box == NULL)
    return FALSE;

  gtk_flow_box_unselect_all (GTK_FLOW_BOX (box));
  return TRUE;
}

static gboolean
gtk_flow_box_accessible_select_all (AtkSelection *selection)
{
  GtkWidget *box;

  box = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (box == NULL)
    return FALSE;

  gtk_flow_box_select_all (GTK_FLOW_BOX (box));
  return TRUE;
}

typedef struct
{
  gint idx;
  GtkWidget *child;
} FindSelectedData;

static void
find_selected_child (GtkFlowBox      *box,
                     GtkFlowBoxChild *child,
                     gpointer         data)
{
  FindSelectedData *d = data;

  if (d->idx == 0)
    {
      if (d->child == NULL)
        d->child = GTK_WIDGET (child);
    }
  else
    d->idx -= 1;
}

static AtkObject *
gtk_flow_box_accessible_ref_selection (AtkSelection *selection,
                                       gint          idx)
{
  GtkWidget *box;
  AtkObject *accessible;
  FindSelectedData data;

  box = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (box == NULL)
    return NULL;

  data.idx = idx;
  data.child = NULL;
  gtk_flow_box_selected_foreach (GTK_FLOW_BOX (box), find_selected_child, &data);

  if (data.child == NULL)
    return NULL;

  accessible = gtk_widget_get_accessible (data.child);
  g_object_ref (accessible);
  return accessible;
}

static void
count_selected (GtkFlowBox      *box,
                GtkFlowBoxChild *child,
                gpointer         data)
{
  gint *count = data;
  *count += 1;
}

static gint
gtk_flow_box_accessible_get_selection_count (AtkSelection *selection)
{
  GtkWidget *box;
  gint count;

  box = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (box == NULL)
    return 0;

  count = 0;
  gtk_flow_box_selected_foreach (GTK_FLOW_BOX (box), count_selected, &count);

  return count;
}

static gboolean
gtk_flow_box_accessible_is_child_selected (AtkSelection *selection,
                                           gint          idx)
{
  GtkWidget *box;
  GtkFlowBoxChild *child;

  box = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (box == NULL)
    return FALSE;

  child = gtk_flow_box_get_child_at_index (GTK_FLOW_BOX (box), idx);

  return gtk_flow_box_child_is_selected (child);
}

static void atk_selection_interface_init (AtkSelectionIface *iface)
{
  iface->add_selection = gtk_flow_box_accessible_add_selection;
  iface->remove_selection = gtk_flow_box_accessible_remove_selection;
  iface->clear_selection = gtk_flow_box_accessible_clear_selection;
  iface->ref_selection = gtk_flow_box_accessible_ref_selection;
  iface->get_selection_count = gtk_flow_box_accessible_get_selection_count;
  iface->is_child_selected = gtk_flow_box_accessible_is_child_selected;
  iface->select_all_selection = gtk_flow_box_accessible_select_all;
}

void
_gtk_flow_box_accessible_selection_changed (GtkWidget *box)
{
  AtkObject *accessible;
  accessible = gtk_widget_get_accessible (box);
  g_signal_emit_by_name (accessible, "selection-changed");
}

void
_gtk_flow_box_accessible_update_cursor (GtkWidget *box,
                                        GtkWidget *child)
{
  AtkObject *accessible;
  AtkObject *descendant;
  accessible = gtk_widget_get_accessible (box);
  descendant = child ? gtk_widget_get_accessible (child) : NULL;
  g_signal_emit_by_name (accessible, "active-descendant-changed", descendant);
}
