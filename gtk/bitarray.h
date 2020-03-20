/*
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __BITARRAY_H__
#define __BITARRAY_H__

#include <glib.h>

typedef struct _BitArray
{
  /*< private >*/
  guint32  default_value : 1;
  guint32  n_bytes : 31;
  guint8  *data;
} BitArray;

void     bit_array_init  (BitArray *ba,
                          gboolean  default_value);
void     bit_array_clear (BitArray *ba);
void     bit_array_set   (BitArray *ba,
                          guint     position,
                          gboolean  value);
gboolean bit_array_get   (BitArray *ba,
                          guint     position);

#endif /* __BITARRAY_H__ */
