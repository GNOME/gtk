/* GAIL - The GNOME Accessibility Enabling Library
 * Copyright 2001 Sun Microsystems Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <gtk/gtk.h>
#include "gtkcontainercellaccessible.h"


G_DEFINE_TYPE (GtkContainerCellAccessible, _gtk_container_cell_accessible, GTK_TYPE_CELL_ACCESSIBLE)


static void
gtk_container_cell_accessible_finalize (GObject *obj)
{
  GtkContainerCellAccessible *container = GTK_CONTAINER_CELL_ACCESSIBLE (obj);

  g_list_free_full (container->children, g_object_unref);

  G_OBJECT_CLASS (_gtk_container_cell_accessible_parent_class)->finalize (obj);
}


static gint
gtk_container_cell_accessible_get_n_children (AtkObject *obj)
{
  GtkContainerCellAccessible *cell = GTK_CONTAINER_CELL_ACCESSIBLE (obj);

  return cell->NChildren;
}

static AtkObject *
gtk_container_cell_accessible_ref_child (AtkObject *obj,
                                         gint       child)
{
  GtkContainerCellAccessible *cell = GTK_CONTAINER_CELL_ACCESSIBLE (obj);
  GList *l;

  l = g_list_nth (cell->children, child);
  if (l == NULL)
    return NULL;

  return g_object_ref (ATK_OBJECT (l->data));
}

static void
_gtk_container_cell_accessible_class_init (GtkContainerCellAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS(klass);
  GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize = gtk_container_cell_accessible_finalize;

  class->get_n_children = gtk_container_cell_accessible_get_n_children;
  class->ref_child = gtk_container_cell_accessible_ref_child;
}

static void
_gtk_container_cell_accessible_init (GtkContainerCellAccessible *cell)
{
}

GtkContainerCellAccessible *
_gtk_container_cell_accessible_new (void)
{
  GObject *object;
  AtkObject *atk_object;
  GtkContainerCellAccessible *container;

  object = g_object_new (GTK_TYPE_CONTAINER_CELL_ACCESSIBLE, NULL);

  g_return_val_if_fail (object != NULL, NULL);

  atk_object = ATK_OBJECT (object);
  atk_object->role = ATK_ROLE_TABLE_CELL;

  container = GTK_CONTAINER_CELL_ACCESSIBLE (object);
  container->children = NULL;
  container->NChildren = 0;
  return container;
}

static void
recompute_child_indices (GtkContainerCellAccessible *container)
{
  gint cur_index = 0;
  GList *l;

  for (l = container->children; l; l = l->next)
    {
      GTK_CELL_ACCESSIBLE (l->data)->index = cur_index;
      cur_index++;
    }
}

static void
refresh_child_index (GtkCellAccessible *cell)
{
  AtkObject *parent;

  parent = atk_object_get_parent (ATK_OBJECT (cell));

  recompute_child_indices (GTK_CONTAINER_CELL_ACCESSIBLE (parent));
}

void
_gtk_container_cell_accessible_add_child (GtkContainerCellAccessible *container,
                                          GtkCellAccessible          *child)
{
  gint child_index;

  g_return_if_fail (GTK_IS_CONTAINER_CELL_ACCESSIBLE (container));
  g_return_if_fail (GTK_IS_CELL_ACCESSIBLE (child));

  child_index = container->NChildren++;
  container->children = g_list_append (container->children, child);
  child->index = child_index;
  atk_object_set_parent (ATK_OBJECT (child), ATK_OBJECT (container));
  child->refresh_index = refresh_child_index;
}

void
_gtk_container_cell_accessible_remove_child (GtkContainerCellAccessible *container,
                                             GtkCellAccessible          *child)
{
  g_return_if_fail (GTK_IS_CONTAINER_CELL_ACCESSIBLE (container));
  g_return_if_fail (GTK_IS_CELL_ACCESSIBLE (child));
  g_return_if_fail (container->NChildren > 0);

  container->children = g_list_remove (container->children, child);
  recompute_child_indices (container);
  container->NChildren--;
}
