/*
 * Copyright © 2020 Benjamin Otte
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
 */

#include "config.h"

#include "gtksortkeysprivate.h"

#include "gtkcssstyleprivate.h"
#include "gtkstyleproviderprivate.h"

GtkSortKeys *
gtk_sort_keys_alloc (const GtkSortKeysClass *klass,
                     gsize                   size,
                     gsize                   key_size,
                     gsize                   key_align)
{
  GtkSortKeys *self;

  self = g_slice_alloc0 (size);

  self->klass = klass;
  self->ref_count = 1;

  self->key_size = key_size;
  self->key_align = key_align;

  return self;
}

GtkSortKeys *
gtk_sort_keys_ref (GtkSortKeys *self)
{
  self->ref_count += 1;

  return self;
}

void
gtk_sort_keys_unref (GtkSortKeys *self)
{
  self->ref_count -= 1;
  if (self->ref_count > 0)
    return;

  self->klass->free (self);
}

gsize
gtk_sort_keys_get_key_size (GtkSortKeys *self)
{
  return self->key_size;
}

gsize
gtk_sort_keys_get_key_align (GtkSortKeys *self)
{
  return self->key_align;
}

GCompareDataFunc
gtk_sort_keys_get_key_compare_func (GtkSortKeys *self)
{
  return self->klass->key_compare;
}

gboolean
gtk_sort_keys_is_compatible (GtkSortKeys *self,
                             GtkSortKeys *other)
{
  if (self == other)
    return TRUE;

  return self->klass->is_compatible (self, other);
}

gboolean
gtk_sort_keys_needs_clear_key (GtkSortKeys *self)
{
  return self->klass->clear_key != NULL;
}

