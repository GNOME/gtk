/*
 * Copyright © 2011 Red Hat Inc.
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include <config.h>

#include "gtk/gtkbitmaskprivate.h"

#define VALUE_TYPE gsize

#define VALUE_SIZE_BITS (sizeof (VALUE_TYPE) * 8)
#define VALUE_BIT(idx) (((VALUE_TYPE) 1) << (idx))

struct _GtkBitmask {
  guint len;
  guint alloc;
  VALUE_TYPE data[1];
};

static GtkBitmask *
_gtk_bitmask_allocate (guint size)
{
  GtkBitmask *mask;

  mask = g_malloc (sizeof (GtkBitmask) + sizeof(VALUE_TYPE) * (size - 1));
  mask->len = 0;
  mask->alloc = size;

  return mask;
}

static void
_gtk_bitmask_resize (GtkBitmask **mask, guint len)
{
  GtkBitmask *new, *old;
  int i;

  old = *mask;

  if (old->alloc < len)
    {
      new = _gtk_bitmask_allocate (len);
      for (i = 0; i < old->len; i++)
	new->data[i] = old->data[i];
      new->len = old->len;
      g_free (old);
      *mask = new;
    }

  /* If expanding, clear the new data */
  if (len > (*mask)->len)
    {
      for (i = (*mask)->len; i < len; i++)
	(*mask)->data[i] = 0;
    }

  (*mask)->len = len;
}



GtkBitmask *
_gtk_bitmask_new (void)
{
  return _gtk_bitmask_allocate (1);
}

GtkBitmask *
_gtk_bitmask_copy (const GtkBitmask *mask)
{
  GtkBitmask *copy;

  g_return_val_if_fail (mask != NULL, NULL);

  copy = _gtk_bitmask_new ();
  _gtk_bitmask_union (&copy, mask);

  return copy;
}

void
_gtk_bitmask_free (GtkBitmask *mask)
{
  g_return_if_fail (mask != NULL);

  g_free (mask);
}

void
_gtk_bitmask_print (const GtkBitmask *mask,
		    GString          *string)
{
  int i;

  g_return_if_fail (mask != NULL);
  g_return_if_fail (string != NULL);

  for (i = mask->len * VALUE_SIZE_BITS - 1; i >= 0; i--)
    {
      if (_gtk_bitmask_get (mask, i))
	break;
    }

  if (i < 0)
    {
      g_string_append_c (string, '0');
      return;
    }

  for (; i >= 0; i--)
    {
      g_string_append_c (string, _gtk_bitmask_get (mask, i) ? '1' : '0');
    }
}

char *
_gtk_bitmask_to_string (const GtkBitmask *mask)
{
  GString *string;

  string = g_string_new (NULL);
  _gtk_bitmask_print (mask, string);
  return g_string_free (string, FALSE);
}

/* NB: Call this function whenever the
 * array might have become too large.
 * _gtk_bitmask_is_empty() depends on this.
 */
static void
gtk_bitmask_shrink (GtkBitmask **mask)
{
  guint i;

  for (i = (*mask)->len; i; i--)
    {
      if ((*mask)->data[i - 1])
	break;
    }

  (*mask)->len = i;
}

void
_gtk_bitmask_intersect (GtkBitmask      **mask,
			const GtkBitmask *other)
{
  guint i;

  g_return_if_fail (mask != NULL);
  g_return_if_fail (other != NULL);

  _gtk_bitmask_resize (mask, MIN ((*mask)->len, other->len));
  for (i = 0; i < (*mask)->len; i++)
    (*mask)->data[i] &= other->data[i];

  gtk_bitmask_shrink (mask);
}

void
_gtk_bitmask_union (GtkBitmask      **mask,
		    const GtkBitmask *other)
{
  guint i;

  g_return_if_fail (mask != NULL);
  g_return_if_fail (other != NULL);

  _gtk_bitmask_resize (mask, MAX ((*mask)->len, other->len));
  for (i = 0; i < other->len; i++)
    (*mask)->data[i] |= other->data[i];
}

void
_gtk_bitmask_subtract (GtkBitmask      **mask,
		       const GtkBitmask *other)
{
  guint i;

  g_return_if_fail (mask != NULL);
  g_return_if_fail (other != NULL);

  for (i = 0; i < other->len; i++)
    (*mask)->data[i] &= ~(other->data[i]);

  gtk_bitmask_shrink (mask);
}

static void
gtk_bitmask_indexes (guint index_,
		     guint *array_index,
		     guint *bit_index)
{
  *array_index = index_ / VALUE_SIZE_BITS;
  *bit_index = index_ % VALUE_SIZE_BITS;
}

gboolean
_gtk_bitmask_get (const GtkBitmask *mask,
		  guint             index_)
{
  guint array_index, bit_index;

  g_return_val_if_fail (mask != NULL, FALSE);

  gtk_bitmask_indexes (index_, &array_index, &bit_index);

  if (array_index >= mask->len)
    return FALSE;

  return (mask->data[array_index] & VALUE_BIT (bit_index)) ? TRUE : FALSE;
}

void
_gtk_bitmask_set (GtkBitmask **mask,
		  guint        index_,
		  gboolean     value)
{
  guint array_index, bit_index;

  g_return_if_fail (mask != NULL);

  gtk_bitmask_indexes (index_, &array_index, &bit_index);

  if (value)
    {
      if (array_index >= (*mask)->len)
	_gtk_bitmask_resize (mask, array_index + 1);

      (*mask)->data[array_index] |= VALUE_BIT (bit_index);
    }
  else
    {
      if (array_index < (*mask)->len)
	{
	  (*mask)->data[array_index] &= ~ VALUE_BIT (bit_index);
	  gtk_bitmask_shrink (mask);
	}
    }
}

void
_gtk_bitmask_invert_range (GtkBitmask **mask,
			   guint        start,
			   guint        end)
{
  guint i;

  g_return_if_fail (mask != NULL);
  g_return_if_fail (start < end);

  /* I CAN HAS SPEEDUP? */

  for (i = start; i < end; i++)
    _gtk_bitmask_set (mask, i, !_gtk_bitmask_get (*mask, i));
}

gboolean
_gtk_bitmask_is_empty (const GtkBitmask *mask)
{
  g_return_val_if_fail (mask != NULL, FALSE);

  return mask->len == 0;
}

gboolean
_gtk_bitmask_equals (const GtkBitmask  *mask,
		     const GtkBitmask  *other)
{
  guint i;

  g_return_val_if_fail (mask != NULL, FALSE);
  g_return_val_if_fail (other != NULL, FALSE);

  if (mask->len != other->len)
    return FALSE;

  for (i = 0; i < mask->len; i++)
    {
      if (mask->data[i] != other->data[i])
	return FALSE;
    }

  return TRUE;
}

gboolean
_gtk_bitmask_intersects (const GtkBitmask *mask,
			 const GtkBitmask *other)
{
  int i;

  g_return_val_if_fail (mask != NULL, FALSE);
  g_return_val_if_fail (other != NULL, FALSE);

  for (i = MIN (mask->len, other->len) - 1; i >= 0; i--)
    {
      if (mask->data[i] & other->data[i])
	return TRUE;
    }

  return FALSE;
}

void
_gtk_bitmask_clear (GtkBitmask **mask)
{
  g_return_if_fail (mask != NULL);

  (*mask)->len = 0;
}

guint
_gtk_bitmask_get_uint (const GtkBitmask  *mask,
		       guint              index_)
{
  guint array_index, bit_index;
  VALUE_TYPE value1, value2;

  g_return_val_if_fail (mask != NULL, FALSE);

  gtk_bitmask_indexes (index_, &array_index, &bit_index);

  value1 = value2 = 0;
  if (array_index < mask->len)
    value1 = mask->data[array_index];
  if (array_index + 1 < mask->len)
    value2 = mask->data[array_index + 1];

  value1 >>= bit_index;
  if (bit_index != 0)
    value2 <<= VALUE_SIZE_BITS - bit_index;
  else
    value2 = 0;

  return (guint)(value1 | value2);
}

void
_gtk_bitmask_set_uint (GtkBitmask  **mask,
		       guint         index_,
		       guint         bits)
{
  guint array_index, bit_index;
  VALUE_TYPE value1, value2;
  VALUE_TYPE new_value1, new_value2;

  g_return_if_fail (mask != NULL);

  gtk_bitmask_indexes (index_, &array_index, &bit_index);

  value1 = value2 = 0;
  if (array_index < (*mask)->len)
    value1 = (*mask)->data[array_index];
  if (array_index + 1 < (*mask)->len)
    value2 = (*mask)->data[array_index + 1];

  /* Mask out old uint value */
  new_value1 = ((VALUE_TYPE)G_MAXUINT) << bit_index;
  if (bit_index != 0)
    new_value2 = ((VALUE_TYPE)G_MAXUINT) >> (VALUE_SIZE_BITS - bit_index);
  else
    new_value2 = 0;
  value1 &= ~new_value1;
  value2 &= ~new_value2;

  /* merge in new uint value */
  new_value1 = ((VALUE_TYPE)bits) << bit_index;
  if (bit_index != 0)
    new_value2 = ((VALUE_TYPE)bits) >> (VALUE_SIZE_BITS - bit_index);
  else
    new_value2 = 0;
  value1 |= new_value1;
  value2 |= new_value2;

  /* Ensure there is space to write back */
  _gtk_bitmask_resize (mask, MAX ((*mask)->len, array_index + 2));

  (*mask)->data[array_index] = value1;
  (*mask)->data[array_index + 1] = value2;

  gtk_bitmask_shrink (mask);
}

guint
_gtk_bitmask_hash (const GtkBitmask *mask)
{
  int i;
  VALUE_TYPE value, hash;

  g_return_val_if_fail (mask != NULL, FALSE);

  hash = 0;

  for (i = mask->len - 1; i >= 0; i--)
    {
      value = mask->data[i];
      hash = hash ^ value;
    }

  if (sizeof (hash) > sizeof (guint))
    hash = hash ^ (hash >> 32);

  return hash;
}

gboolean
_gtk_bitmask_find_next_set (const GtkBitmask  *mask,
			    guint             *pos)
{
  guint array_index, bit_index;
  VALUE_TYPE value;

  gtk_bitmask_indexes (*pos, &array_index, &bit_index);

  while (array_index < mask->len)
    {
      value = mask->data[array_index];

      /* TODO: This could use ffsl if bit_index == 0 */
      while (bit_index < VALUE_SIZE_BITS)
	{
	  if (value & VALUE_BIT (bit_index))
	    {
	      *pos = array_index * VALUE_SIZE_BITS + bit_index;
	      return TRUE;
	    }
	  bit_index++;
	}
      array_index++;
      bit_index = 0;
    }

  return FALSE;
}
