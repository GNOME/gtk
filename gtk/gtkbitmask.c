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

typedef struct {
  GtkBitmask bitmask;
  VALUE_TYPE more_data[3];
  const GtkBitmask *orig;
} GtkBitmaskPrealloc;

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

static gboolean
gtk_bitmask_is_bit (const GtkBitmask *mask)
{
  return (((gsize)mask) & 1);
}

static GtkBitmask *
gtk_bitmask_for_bit (guint bit)
{
  gsize mask;

  mask = bit;
  mask <<= 1;
  mask |= 1;
  return (GtkBitmask *)mask;
}

static guint
gtk_bitmask_get_bit (const GtkBitmask *mask)
{
  return ((gsize)mask) >> 1;
}

static gboolean
gtk_bitmask_is_inline (const GtkBitmask *mask)
{
  return mask == NULL || gtk_bitmask_is_bit (mask);
}

static void
gtk_bitmask_realize_for_write (GtkBitmask **mask, guint size_hint)
{
  GtkBitmask *realized;

  if (*mask == NULL)
    {
      realized = _gtk_bitmask_allocate (MAX (size_hint, 1));
      realized->len = 0;
      *mask = realized;
    }
  else if (gtk_bitmask_is_bit (*mask))
    {
      guint bit = gtk_bitmask_get_bit (*mask);
      guint len = (bit / VALUE_SIZE_BITS) + 1;
      int i;

      realized = _gtk_bitmask_allocate (MAX (len, size_hint));
      realized->len = len;
      for (i = 0; i < len; i++)
	realized->data[i] = 0;

      _gtk_bitmask_set (&realized, bit, TRUE);

      *mask = realized;
    }
}


static void
gtk_bitmask_realize (const GtkBitmask **mask, GtkBitmaskPrealloc *prealloc)
{
  prealloc->orig = *mask;
  if (*mask == NULL)
    {
      prealloc->bitmask.len = 0;
      prealloc->bitmask.alloc = 4;
      *mask = &prealloc->bitmask;
    }
  else if (gtk_bitmask_is_bit (*mask))
    {
      GtkBitmask *realized;
      int i;
      guint bit = gtk_bitmask_get_bit (*mask);
      guint len = (bit / VALUE_SIZE_BITS) + 1;

      if (len <= 4)
	{
	  prealloc->bitmask.alloc = 4;
	  realized = &prealloc->bitmask;
	}
      else
	realized = _gtk_bitmask_allocate (len);

      for (i = 0; i < len; i++)
	realized->data[i] = 0;
      realized->len = len;

      _gtk_bitmask_set (&realized, bit, TRUE);

      *mask = realized;
    }
}

static void
gtk_bitmask_unrealize (const GtkBitmask *mask, GtkBitmaskPrealloc *prealloc)
{
  if (mask != prealloc->orig &&
      mask != &prealloc->bitmask)
    g_free ((GtkBitmask *)mask);
}

GtkBitmask *
_gtk_bitmask_new (void)
{
  return NULL;
}

GtkBitmask *
_gtk_bitmask_copy (const GtkBitmask *mask)
{
  GtkBitmask *copy;

  if (gtk_bitmask_is_inline (mask))
    return (GtkBitmask *)mask;

  copy = _gtk_bitmask_new ();
  _gtk_bitmask_union (&copy, mask);

  return copy;
}

void
_gtk_bitmask_free (GtkBitmask *mask)
{
  if (!gtk_bitmask_is_inline (mask))
    g_free (mask);
}

void
_gtk_bitmask_print (const GtkBitmask *mask,
		    GString          *string)
{
  GtkBitmaskPrealloc mask_prealloc;
  int i;

  gtk_bitmask_realize (&mask, &mask_prealloc);

  g_return_if_fail (string != NULL);

  for (i = mask->len * VALUE_SIZE_BITS - 1; i >= 0; i--)
    {
      if (_gtk_bitmask_get (mask, i))
	break;
    }

  if (i < 0)
    {
      g_string_append_c (string, '0');
      gtk_bitmask_unrealize (mask, &mask_prealloc);
      return;
    }

  for (; i >= 0; i--)
    {
      g_string_append_c (string, _gtk_bitmask_get (mask, i) ? '1' : '0');
    }

  gtk_bitmask_unrealize (mask, &mask_prealloc);
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

  if (gtk_bitmask_is_inline (*mask))
    return;

  for (i = (*mask)->len; i; i--)
    {
      if ((*mask)->data[i - 1])
	break;
    }

  if (i == 0)
    {
      g_free (*mask);
      *mask = NULL;
    }
  else
    (*mask)->len = i;
}

void
_gtk_bitmask_intersect (GtkBitmask      **mask,
			const GtkBitmask *other)
{
  GtkBitmaskPrealloc other_prealloc;
  guint i;

  g_return_if_fail (mask != NULL);

  if (gtk_bitmask_is_inline (*mask) &&
      gtk_bitmask_is_inline (other))
    {
      if (mask == NULL || other == NULL)
	*mask = NULL;
      else
	{
	  /* Different bits, the intersection is empty */
	  if (*mask != other)
	    *mask = NULL;
	  /* If same bit, the value is already right */
	}

      return;
    }

  gtk_bitmask_realize (&other, &other_prealloc);
  gtk_bitmask_realize_for_write (mask, 0);

  _gtk_bitmask_resize (mask, MIN ((*mask)->len, other->len));
  for (i = 0; i < (*mask)->len; i++)
    (*mask)->data[i] &= other->data[i];

  gtk_bitmask_shrink (mask);

  gtk_bitmask_unrealize (other, &other_prealloc);
}

void
_gtk_bitmask_union (GtkBitmask      **mask,
		    const GtkBitmask *other)
{
  GtkBitmaskPrealloc other_prealloc;
  guint i;

  g_return_if_fail (mask != NULL);

  if (gtk_bitmask_is_inline (*mask) &&
      gtk_bitmask_is_inline (other))
    {
      /* Union nothing with something is something  */
      if (*mask == NULL)
	{
	  *mask = (GtkBitmask *)other;
	  return;
	}

      /* Union with nothing or same is same */
      if (other == NULL ||
	  *mask == other)
	return;
    }

  gtk_bitmask_realize (&other, &other_prealloc);
  gtk_bitmask_realize_for_write (mask, other->len);

  _gtk_bitmask_resize (mask, MAX ((*mask)->len, other->len));
  for (i = 0; i < other->len; i++)
    (*mask)->data[i] |= other->data[i];

  gtk_bitmask_unrealize (other, &other_prealloc);
}

void
_gtk_bitmask_subtract (GtkBitmask      **mask,
		       const GtkBitmask *other)
{
  GtkBitmaskPrealloc other_prealloc;
  guint i;

  g_return_if_fail (mask != NULL);

  if (gtk_bitmask_is_inline (*mask) &&
      gtk_bitmask_is_inline (other))
    {
      /* Subtract from nothing, or subtract nothing => no change */
      if (*mask == NULL ||
	  other == NULL)
	return;

      /* Subtract the same bit => nothing */
      if (*mask == other)
	*mask = NULL;
      /* Subtract other bit => no change */
      return;
    }

  gtk_bitmask_realize (&other, &other_prealloc);
  gtk_bitmask_realize_for_write (mask, 0);

  for (i = 0; i < other->len; i++)
    (*mask)->data[i] &= ~(other->data[i]);

  gtk_bitmask_shrink (mask);

  gtk_bitmask_unrealize (other, &other_prealloc);
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

  if (mask == NULL)
    return FALSE;

  if (gtk_bitmask_is_bit (mask))
    {
      guint bit = gtk_bitmask_get_bit (mask);
      if (bit == index_)
	return TRUE;
      else
	return FALSE;
    }

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

  if (*mask == NULL)
    {
      if (value)
	*mask = gtk_bitmask_for_bit (index_);
      return;
    }

  if (gtk_bitmask_is_bit (*mask) &&
      gtk_bitmask_get_bit (*mask) == index_)
    {
      if (!value)
	*mask = NULL;
      return;
    }

  gtk_bitmask_realize_for_write (mask, 0);

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
  return mask == NULL;
}

gboolean
_gtk_bitmask_equals (const GtkBitmask  *mask,
		     const GtkBitmask  *other)
{
  GtkBitmaskPrealloc mask_prealloc;
  GtkBitmaskPrealloc other_prealloc;
  guint i;

  if (gtk_bitmask_is_inline (mask) &&
      gtk_bitmask_is_inline (other))
    return mask == other;

  gtk_bitmask_realize (&mask, &mask_prealloc);
  gtk_bitmask_realize (&other, &other_prealloc);

  if (mask->len != other->len)
    return FALSE;

  for (i = 0; i < mask->len; i++)
    {
      if (mask->data[i] != other->data[i])
	return FALSE;
    }

  gtk_bitmask_unrealize (mask, &mask_prealloc);
  gtk_bitmask_unrealize (other, &other_prealloc);

  return TRUE;
}

gboolean
_gtk_bitmask_intersects (const GtkBitmask *mask,
			 const GtkBitmask *other)
{
  GtkBitmaskPrealloc mask_prealloc;
  GtkBitmaskPrealloc other_prealloc;
  int i;
  gboolean res;

  if (gtk_bitmask_is_inline (mask) &&
      gtk_bitmask_is_inline (other))
    {
      if (mask == NULL || other == NULL)
	return FALSE;

      if (mask == other)
	return TRUE;

      return FALSE;
    }

  gtk_bitmask_realize (&mask, &mask_prealloc);
  gtk_bitmask_realize (&other, &other_prealloc);

  res = FALSE;
  for (i = MIN (mask->len, other->len) - 1; i >= 0; i--)
    {
      if (mask->data[i] & other->data[i])
	{
	  res = TRUE;
	  break;
	}
    }

  gtk_bitmask_unrealize (mask, &mask_prealloc);
  gtk_bitmask_unrealize (other, &other_prealloc);

  return res;
}

void
_gtk_bitmask_clear (GtkBitmask **mask)
{
  _gtk_bitmask_free (*mask);
  *mask = NULL;
}

guint
_gtk_bitmask_get_uint (const GtkBitmask  *mask,
		       guint              index_)
{
  GtkBitmaskPrealloc mask_prealloc;
  guint array_index, bit_index;
  VALUE_TYPE value1, value2;

  gtk_bitmask_realize (&mask, &mask_prealloc);

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

  gtk_bitmask_unrealize (mask, &mask_prealloc);

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

  gtk_bitmask_realize_for_write (mask, 0);

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
  GtkBitmaskPrealloc mask_prealloc;
  int i;
  VALUE_TYPE value, hash;

  if (mask == NULL)
    return 0;

  gtk_bitmask_realize (&mask, &mask_prealloc);

  hash = 0;

  for (i = mask->len - 1; i >= 0; i--)
    {
      value = mask->data[i];
      hash = hash ^ value;
    }

  if (sizeof (hash) > sizeof (guint))
    hash = hash ^ (hash >> 32);

  gtk_bitmask_unrealize (mask, &mask_prealloc);

  return hash;
}

gboolean
_gtk_bitmask_find_next_set (const GtkBitmask  *mask,
			    guint             *pos)
{
  GtkBitmaskPrealloc mask_prealloc;
  guint array_index, bit_index;
  VALUE_TYPE value;

  if (mask == NULL)
    return FALSE;

  gtk_bitmask_realize (&mask, &mask_prealloc);

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

  gtk_bitmask_unrealize (mask, &mask_prealloc);

  return FALSE;
}
