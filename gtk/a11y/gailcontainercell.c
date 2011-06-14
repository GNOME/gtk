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
#include "gailcontainercell.h"

static void       gail_container_cell_class_init          (GailContainerCellClass *klass);
static void       gail_container_cell_init                (GailContainerCell   *cell);
static void       gail_container_cell_finalize            (GObject             *obj);


static void       _gail_container_cell_recompute_child_indices 
                                                          (GailContainerCell *container);

static void       gail_container_cell_refresh_child_index (GailCell *cell);

static gint       gail_container_cell_get_n_children      (AtkObject *obj);

static AtkObject* gail_container_cell_ref_child           (AtkObject *obj,
                                                           gint      child);

G_DEFINE_TYPE (GailContainerCell, gail_container_cell, GAIL_TYPE_CELL)

static void 
gail_container_cell_class_init (GailContainerCellClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS(klass);
  GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize = gail_container_cell_finalize;

  class->get_n_children = gail_container_cell_get_n_children;
  class->ref_child = gail_container_cell_ref_child;
}

static void
gail_container_cell_init (GailContainerCell   *cell)
{
}

GailContainerCell * 
gail_container_cell_new (void)
{
  GObject *object;
  AtkObject *atk_object;
  GailContainerCell *container;

  object = g_object_new (GAIL_TYPE_CONTAINER_CELL, NULL);

  g_return_val_if_fail (object != NULL, NULL);

  atk_object = ATK_OBJECT (object);
  atk_object->role = ATK_ROLE_TABLE_CELL;

  container = GAIL_CONTAINER_CELL(object);
  container->children = NULL;
  container->NChildren = 0;
  return container;
}

static void
gail_container_cell_finalize (GObject *obj)
{
  GailContainerCell *container = GAIL_CONTAINER_CELL (obj);
  GList *list;

  list = container->children;
  while (list)
  {
    g_object_unref (list->data);
    list = list->next;
  }
  g_list_free (container->children);
  
  G_OBJECT_CLASS (gail_container_cell_parent_class)->finalize (obj);
}


void
gail_container_cell_add_child (GailContainerCell *container,
			       GailCell *child)
{
  gint child_index;

  g_return_if_fail (GAIL_IS_CONTAINER_CELL(container));
  g_return_if_fail (GAIL_IS_CELL(child));

  child_index = container->NChildren++;
  container->children = g_list_append (container->children, (gpointer) child);
  child->index = child_index;
  atk_object_set_parent (ATK_OBJECT (child), ATK_OBJECT (container));
  child->refresh_index = gail_container_cell_refresh_child_index;
}


void
gail_container_cell_remove_child (GailContainerCell *container,
				  GailCell *child)
{
  g_return_if_fail (GAIL_IS_CONTAINER_CELL(container));
  g_return_if_fail (GAIL_IS_CELL(child));
  g_return_if_fail (container->NChildren > 0);

  container->children = g_list_remove (container->children, (gpointer) child);
  _gail_container_cell_recompute_child_indices (container);
  container->NChildren--;
}


static void
_gail_container_cell_recompute_child_indices (GailContainerCell *container)
{
  gint cur_index = 0;
  GList *temp_list;

  g_return_if_fail (GAIL_IS_CONTAINER_CELL(container));

  for (temp_list = container->children; temp_list; temp_list = temp_list->next)
    {
      GAIL_CELL(temp_list->data)->index = cur_index;
      cur_index++;
    }
}


static void
gail_container_cell_refresh_child_index (GailCell *cell)
{
  GailContainerCell *container;
  g_return_if_fail (GAIL_IS_CELL(cell));
  container = GAIL_CONTAINER_CELL (atk_object_get_parent (ATK_OBJECT(cell)));
  g_return_if_fail (GAIL_IS_CONTAINER_CELL (container));
  _gail_container_cell_recompute_child_indices (container);
}



static gint
gail_container_cell_get_n_children (AtkObject *obj)
{
  GailContainerCell *cell;
  g_return_val_if_fail (GAIL_IS_CONTAINER_CELL(obj), 0);
  cell = GAIL_CONTAINER_CELL(obj);
  return cell->NChildren;
}


static AtkObject *
gail_container_cell_ref_child (AtkObject *obj,
			       gint       child)
{
  GailContainerCell *cell;
  GList *list_node;

  g_return_val_if_fail (GAIL_IS_CONTAINER_CELL(obj), NULL);
  cell = GAIL_CONTAINER_CELL(obj);
  
  list_node = g_list_nth (cell->children, child);
  if (!list_node)
    return NULL;

  return g_object_ref (ATK_OBJECT (list_node->data));
}
