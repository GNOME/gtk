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
#include <string.h>
#include "glib.h"


#define MIN_ARRAY_SIZE  16


typedef struct _GRealArray  GRealArray;

struct _GRealArray
{
  gpointer *data;
  guint   len;
  guint   alloc;
};

static gint g_nearest_pow        (gint        num);
static void g_array_maybe_expand (GRealArray *array,
				  gint        len);


static GMemChunk *array_mem_chunk = NULL;



GArray*
g_array_new ()
{
  GRealArray *array;

  if (!array_mem_chunk)
    array_mem_chunk = g_mem_chunk_new ("array mem chunk",
				       sizeof (GRealArray),
				       1024, G_ALLOC_AND_FREE);

  array = g_chunk_new (GRealArray, array_mem_chunk);

  array->data = NULL;
  array->len = 0;
  array->alloc = 0;

  return (GArray*) array;
}

void
g_array_free (GArray *array,
	      gboolean  free_segment)
{
  g_return_if_fail (array);

  if (free_segment)
    g_free (array->data);

  g_mem_chunk_free (array_mem_chunk, array);
}

static gint
g_nearest_pow (gint num)
{
  gint n = 1;

  while (n < num)
    n <<= 1;

  return n;
}

static void
g_array_maybe_expand (GRealArray *array,
		      gint        len)
{
  guint old_alloc;

  if ((array->len + len) > array->alloc)
    {
      old_alloc = array->alloc;

      array->alloc = g_nearest_pow (array->len + len);
      array->alloc = MAX (array->alloc, MIN_ARRAY_SIZE);
      if (array->data)
	array->data = g_realloc (array->data, sizeof(gpointer) * array->alloc);
      else
	array->data = g_new0 (gpointer, array->alloc);

      memset (array->data + old_alloc, 0, array->alloc - old_alloc);
    }
}

void
g_array_set_size  (GArray   *farray,
		   gint	     length)
{
  GRealArray* array = (GRealArray*) farray;

  g_return_if_fail (array);

  if (length > array->len)
    g_array_maybe_expand (array, (length - array->len));

  array->len = length;
}

void
g_array_remove_index (GArray* farray,
		      gint index)
{
  GRealArray* array = (GRealArray*) farray;

  g_return_if_fail (array);

  g_return_if_fail (index >= array->len);

  array->data[index] = array->data[array->len - 1];

  array->data[array->len - 1] = NULL;

  array->len -= 1;
}

gboolean
g_array_remove (GArray* farray,
		gpointer data)
{
  GRealArray* array = (GRealArray*) farray;
  int i;

  g_return_val_if_fail (array, FALSE);

  for (i = 0; i < array->len; i += 1)
    {
      if (array->data[i] == data)
	{
	  g_array_remove_index (farray, i);
	  return TRUE;
	}
    }

  return FALSE;
}

void
g_array_add (GArray* farray,
	     gpointer data)
{
  GRealArray* array = (GRealArray*) farray;

  g_return_if_fail (array);

  g_array_maybe_expand (array, 1);

  array->data[array->len++] = data;
}
