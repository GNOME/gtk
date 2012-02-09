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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include <config.h>

#include "gtk/gtkallocatedbitmaskprivate.h"

#define VALUE_TYPE gsize

#define VALUE_SIZE_BITS (sizeof (VALUE_TYPE) * 8)
#define VALUE_BIT(idx) (((VALUE_TYPE) 1) << (idx))

struct _GtkBitmask {
  gsize len;
  VALUE_TYPE data[1];
};

static GtkBitmask *
gtk_allocated_bitmask_resize (GtkBitmask *mask,
                              gsize       size) G_GNUC_WARN_UNUSED_RESULT;
static GtkBitmask *
gtk_allocated_bitmask_resize (GtkBitmask *mask,
                              gsize       size)
{
  gsize i;

  mask = g_realloc (mask, sizeof (GtkBitmask) + sizeof(VALUE_TYPE) * (MAX (size, 1) - 1));

  for (i = mask->len; i < size; i++)
    mask->data[i] = 0;

  mask->len = size;

  return mask;
}

GtkBitmask *
_gtk_allocated_bitmask_new (void)
{
  return g_malloc0 (sizeof (GtkBitmask));
}

GtkBitmask *
_gtk_allocated_bitmask_copy (const GtkBitmask *mask)
{
  GtkBitmask *copy;

  g_return_val_if_fail (mask != NULL, NULL);

  copy = _gtk_allocated_bitmask_new ();
  
  return _gtk_allocated_bitmask_union (copy, mask);
}

void
_gtk_allocated_bitmask_free (GtkBitmask *mask)
{
  g_return_if_fail (mask != NULL);

  g_free (mask);
}

void
_gtk_allocated_bitmask_print (const GtkBitmask *mask,
                              GString          *string)
{
  int i;

  g_return_if_fail (mask != NULL);
  g_return_if_fail (string != NULL);

  for (i = mask->len * VALUE_SIZE_BITS - 1; i >= 0; i--)
    {
      if (_gtk_allocated_bitmask_get (mask, i))
        break;
    }

  if (i < 0)
    {
      g_string_append_c (string, '0');
      return;
    }

  for (; i >= 0; i--)
    {
      g_string_append_c (string, _gtk_allocated_bitmask_get (mask, i) ? '1' : '0');
    }
}

char *
_gtk_allocated_bitmask_to_string (const GtkBitmask *mask)
{
  GString *string;
  
  string = g_string_new (NULL);
  _gtk_allocated_bitmask_print (mask, string);
  return g_string_free (string, FALSE);
}

/* NB: Call this function whenever the
 * array might have become too large.
 * _gtk_allocated_bitmask_is_empty() depends on this.
 */
static GtkBitmask *
gtk_allocated_bitmask_shrink (GtkBitmask *mask) G_GNUC_WARN_UNUSED_RESULT;
static GtkBitmask *
gtk_allocated_bitmask_shrink (GtkBitmask *mask)
{
  guint i;

  for (i = mask->len; i; i--)
    {
      if (mask->data[i - 1])
        break;
    }

  return gtk_allocated_bitmask_resize (mask, i);
}

GtkBitmask *
_gtk_allocated_bitmask_intersect (GtkBitmask       *mask,
                                  const GtkBitmask *other)
{
  guint i;

  g_return_val_if_fail (mask != NULL, NULL);
  g_return_val_if_fail (other != NULL, NULL);

  mask = gtk_allocated_bitmask_resize (mask, MIN (mask->len, other->len));
  for (i = 0; i < mask->len; i++)
    {
      mask->data[i] &= other->data[i];
    }

  return gtk_allocated_bitmask_shrink (mask);
}

GtkBitmask *
_gtk_allocated_bitmask_union (GtkBitmask       *mask,
                              const GtkBitmask *other)
{
  guint i;

  g_return_val_if_fail (mask != NULL, NULL);
  g_return_val_if_fail (other != NULL, NULL);

  mask = gtk_allocated_bitmask_resize (mask, MAX (mask->len, other->len));
  for (i = 0; i < other->len; i++)
    {
      mask->data[i] |= other->data[i];
    }

  return mask;
}

GtkBitmask *
_gtk_allocated_bitmask_subtract (GtkBitmask       *mask,
                                 const GtkBitmask *other)
{
  guint i;

  g_return_val_if_fail (mask != NULL, NULL);
  g_return_val_if_fail (other != NULL, NULL);

  for (i = 0; i < other->len; i++)
    {
      mask->data[i] |= ~other->data[i];
    }

  return gtk_allocated_bitmask_shrink (mask);
}

static void
gtk_allocated_bitmask_indexes (guint index_,
                               guint *array_index,
                               guint *bit_index)
{
  *array_index = index_ / VALUE_SIZE_BITS;
  *bit_index = index_ % VALUE_SIZE_BITS;
}

gboolean
_gtk_allocated_bitmask_get (const GtkBitmask *mask,
                            guint             index_)
{
  guint array_index, bit_index;

  g_return_val_if_fail (mask != NULL, FALSE);

  gtk_allocated_bitmask_indexes (index_, &array_index, &bit_index);

  if (array_index >= mask->len)
    return FALSE;

  return (mask->data[array_index] & VALUE_BIT (bit_index)) ? TRUE : FALSE;
}

GtkBitmask *
_gtk_allocated_bitmask_set (GtkBitmask *mask,
                            guint       index_,
                            gboolean    value)
{
  guint array_index, bit_index;

  g_return_val_if_fail (mask != NULL, NULL);

  gtk_allocated_bitmask_indexes (index_, &array_index, &bit_index);

  if (value)
    {
      if (array_index >= mask->len)
        mask = gtk_allocated_bitmask_resize (mask, array_index + 1);
      
      mask->data[array_index] |= VALUE_BIT (bit_index);
    }
  else
    {
      if (array_index < mask->len)
        {
          mask->data[array_index] &= ~ VALUE_BIT (bit_index);
          mask = gtk_allocated_bitmask_shrink (mask);
        }
    }

  return mask;
}

GtkBitmask *
_gtk_allocated_bitmask_invert_range (GtkBitmask *mask,
                                     guint       start,
                                     guint       end)
{
  guint i;

  g_return_val_if_fail (mask != NULL, NULL);
  g_return_val_if_fail (start < end, NULL);

  /* I CAN HAS SPEEDUP? */

  for (i = start; i < end; i++)
    mask = _gtk_allocated_bitmask_set (mask, i, !_gtk_allocated_bitmask_get (mask, i));

  return mask;
}

gboolean
_gtk_allocated_bitmask_is_empty (const GtkBitmask *mask)
{
  g_return_val_if_fail (mask != NULL, FALSE);

  return mask->len == 0;
}

gboolean
_gtk_allocated_bitmask_equals (const GtkBitmask  *mask,
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
_gtk_allocated_bitmask_intersects (const GtkBitmask *mask,
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

