/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "glib.h"


typedef struct _GRealListAllocator GRealListAllocator;

struct _GRealListAllocator
{
  GMemChunk *list_mem_chunk;
  GList     *free_list;
};


static GRealListAllocator *default_allocator = NULL;
static GRealListAllocator *current_allocator = NULL;


GListAllocator*
g_list_allocator_new ()
{
  GRealListAllocator* allocator = g_new (GRealListAllocator, 1);

  allocator->list_mem_chunk = NULL;
  allocator->free_list = NULL;

  return (GListAllocator*) allocator;
}

void
g_list_allocator_free (GListAllocator* fallocator)
{
  GRealListAllocator* allocator = (GRealListAllocator *) fallocator;

  if (allocator && allocator->list_mem_chunk)
    g_mem_chunk_destroy (allocator->list_mem_chunk);
  if (allocator)
    g_free (allocator);
}

GListAllocator*
g_list_set_allocator (GListAllocator* fallocator)
{
  GRealListAllocator* allocator = (GRealListAllocator *) fallocator;
  GRealListAllocator* old_allocator = current_allocator;

  if (allocator)
    current_allocator = allocator;
  else
    {
      if (!default_allocator)
	default_allocator = (GRealListAllocator*) g_list_allocator_new ();
      current_allocator = default_allocator;
    }

  if (!current_allocator->list_mem_chunk)
    current_allocator->list_mem_chunk = g_mem_chunk_new ("list mem chunk",
							 sizeof (GList),
							 1024,
							 G_ALLOC_ONLY);

  return (GListAllocator*) (old_allocator == default_allocator ? NULL : old_allocator);
}


GList*
g_list_alloc ()
{
  GList *new_list;

  g_list_set_allocator (NULL);
  if (current_allocator->free_list)
    {
      new_list = current_allocator->free_list;
      current_allocator->free_list = current_allocator->free_list->next;
    }
  else
    {
      new_list = g_chunk_new (GList, current_allocator->list_mem_chunk);
    }

  new_list->data = NULL;
  new_list->next = NULL;
  new_list->prev = NULL;

  return new_list;
}

void
g_list_free (GList *list)
{
  GList *last;

  if (list)
    {
      last = g_list_last (list);
      last->next = current_allocator->free_list;
      current_allocator->free_list = list;
    }
}

void
g_list_free_1 (GList *list)
{
  if (list)
    {
      list->next = current_allocator->free_list;
      current_allocator->free_list = list;
    }
}

GList*
g_list_append (GList    *list,
	       gpointer  data)
{
  GList *new_list;
  GList *last;

  new_list = g_list_alloc ();
  new_list->data = data;

  if (!list)
    {
      list = new_list;
    }
  else
    {
      last = g_list_last (list);
      g_assert (last != NULL);
      last->next = new_list;
      new_list->prev = last;
    }

  return list;
}

GList*
g_list_prepend (GList    *list,
		gpointer  data)
{
  GList *new_list;

  new_list = g_list_alloc ();
  new_list->data = data;

  if (list)
    {
      if (list->prev)
	list->prev->next = new_list;
      new_list->prev = list->prev;
      list->prev = new_list;
    }
  new_list->next = list;

  return new_list;
}

GList*
g_list_insert (GList    *list,
	       gpointer  data,
	       gint      position)
{
  GList *new_list;
  GList *tmp_list;

  if (position < 0)
    return g_list_append (list, data);
  else if (position == 0)
    return g_list_prepend (list, data);

  tmp_list = g_list_nth (list, position);
  if (!tmp_list)
    return g_list_append (list, data);

  new_list = g_list_alloc ();
  new_list->data = data;

  if (tmp_list->prev)
    tmp_list->prev->next = new_list;
  new_list->next = tmp_list;
  new_list->prev = tmp_list->prev;
  tmp_list->prev = new_list;

  if (tmp_list == list)
    list = new_list;

  return list;
}

GList*
g_list_remove (GList    *list,
	       gpointer  data)
{
  GList *tmp;

  tmp = list;
  while (tmp)
    {
      if (tmp->data == data)
	{
	  if (tmp->prev)
	    tmp->prev->next = tmp->next;
	  if (tmp->next)
	    tmp->next->prev = tmp->prev;

	  if (list == tmp)
	    list = list->next;

	  tmp->next = NULL;
	  tmp->prev = NULL;
	  g_list_free (tmp);

	  break;
	}

      tmp = tmp->next;
    }
  return list;
}

GList*
g_list_remove_link (GList *list,
		    GList *link)
{
  if (link)
    {
      if (link->prev)
	link->prev->next = link->next;
      if (link->next)
	link->next->prev = link->prev;

      if (link == list)
	list = list->next;

      link->next = NULL;
      link->prev = NULL;
    }

  return list;
}

GList*
g_list_reverse (GList *list)
{
  GList *tmp;
  GList *last;

  last = NULL;
  while (list)
    {
      last = list;
      tmp = list->next;
      list->next = list->prev;
      list->prev = tmp;
      list = tmp;
    }

  return last;
}

GList*
g_list_nth (GList *list,
	    gint   n)
{
  while ((n-- > 0) && list)
    list = list->next;

  return list;
}

GList*
g_list_find (GList    *list,
	     gpointer  data)
{
  while (list)
    {
      if (list->data == data)
	break;
      list = list->next;
    }

  return list;
}

GList*
g_list_last (GList *list)
{
  if (list)
    {
      while (list->next)
	list = list->next;
    }

  return list;
}

GList*
g_list_first (GList *list)
{
  if (list)
    {
      while (list->prev)
        list = list->prev;
    }

  return list;
}

gint
g_list_length (GList *list)
{
  gint length;

  length = 0;
  while (list)
    {
      length++;
      list = list->next;
    }

  return length;
}

void
g_list_foreach (GList    *list,
                GFunc     func,
		gpointer  user_data)
{
  while (list)
    {
      (*func) (list->data, user_data);
      list = list->next;
    }
}
