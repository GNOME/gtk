/* gtktreedatalist.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gtktreedatalist.h"
#include "gobject/gvalue.h"

/* node allocation
 */
struct _GAllocator /* from gmem.c */
{
  gchar           *name;
  guint16          n_preallocs;
  guint            is_unused : 1;
  guint            type : 4;
  GAllocator      *last;
  GMemChunk       *mem_chunk;
  GtkTreeDataList *free_nodes;
};


G_LOCK_DEFINE_STATIC (current_allocator);
static GAllocator *current_allocator = NULL;

/* HOLDS: current_allocator_lock */
static void
gtk_tree_data_list_validate_allocator (GAllocator *allocator)
{
  g_return_if_fail (allocator != NULL);
  g_return_if_fail (allocator->is_unused == TRUE);

  if (allocator->type != G_ALLOCATOR_NODE)
    {
      allocator->type = G_ALLOCATOR_NODE;
      if (allocator->mem_chunk)
	{
	  g_mem_chunk_destroy (allocator->mem_chunk);
	  allocator->mem_chunk = NULL;
	}
    }

  if (!allocator->mem_chunk)
    {
      allocator->mem_chunk = g_mem_chunk_new (allocator->name,
					      sizeof (GtkTreeDataList),
					      sizeof (GtkTreeDataList) * allocator->n_preallocs,
					      G_ALLOC_ONLY);
      allocator->free_nodes = NULL;
    }

  allocator->is_unused = FALSE;
}

void
gtk_tree_data_list_push_allocator (GAllocator *allocator)
{
  G_LOCK (current_allocator);
  gtk_tree_data_list_validate_allocator ( allocator );
  allocator->last = current_allocator;
  current_allocator = allocator;
  G_UNLOCK (current_allocator);
}

void
gtk_tree_data_list_pop_allocator (void)
{
  G_LOCK (current_allocator);
  if (current_allocator)
    {
      GAllocator *allocator;

      allocator = current_allocator;
      current_allocator = allocator->last;
      allocator->last = NULL;
      allocator->is_unused = TRUE;
    }
  G_UNLOCK (current_allocator);
}

GtkTreeDataList *
gtk_tree_data_list_alloc (void)
{
  GtkTreeDataList *list;

  G_LOCK (current_allocator);
  if (!current_allocator)
    {
       GAllocator *allocator = g_allocator_new ("GTK+ default GtkTreeDataList allocator",
						128);
       gtk_tree_data_list_validate_allocator (allocator);
       allocator->last = NULL;
       current_allocator = allocator;
    }
  if (!current_allocator->free_nodes)
    list = g_chunk_new (GtkTreeDataList, current_allocator->mem_chunk);
  else
    {
      list = current_allocator->free_nodes;
      current_allocator->free_nodes = list->next;
    }
  G_UNLOCK (current_allocator);

  return list;
}

void
gtk_tree_data_list_free (GtkTreeDataList *list)
{
  G_LOCK (current_allocator);
  list->next = current_allocator->free_nodes;
  current_allocator->free_nodes = list;
  G_UNLOCK (current_allocator);
}

void
gtk_tree_data_list_node_to_value (GtkTreeDataList *list,
				  GType            type,
				  GValue          *value)
{
  g_value_init (value, type);

  switch (type)
    {
    case G_TYPE_STRING:
      g_value_set_string (value, (gchar *) list->data.v_pointer);
      break;
    }
}

void
gtk_tree_data_list_value_to_node (GtkTreeDataList *list,
				  GValue          *value)
{
  switch (value->g_type)
    {
    case G_TYPE_STRING:
      list->data.v_pointer = g_value_dup_string (value);
      break;
    default:
      g_warning ("Unsupported type (%s) stored.", g_type_name (value->g_type));
      return;
    }
}

