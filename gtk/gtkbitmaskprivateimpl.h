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

#include "gtkallocatedbitmaskprivate.h"

static inline GtkBitmask *
_gtk_bitmask_new (void)
{
  return _gtk_allocated_bitmask_new ();
}

static inline GtkBitmask *
_gtk_bitmask_copy (const GtkBitmask *mask)
{
  return _gtk_allocated_bitmask_copy (mask);
}

static inline void
_gtk_bitmask_free (GtkBitmask *mask)
{
  return _gtk_allocated_bitmask_free (mask);
}

static inline char *
_gtk_bitmask_to_string (const GtkBitmask *mask)
{
  return _gtk_allocated_bitmask_to_string (mask);
}

static inline void
_gtk_bitmask_print (const GtkBitmask *mask,
                    GString          *string)
{
  return _gtk_allocated_bitmask_print (mask, string);
}

static inline GtkBitmask *
_gtk_bitmask_intersect (GtkBitmask       *mask,
                        const GtkBitmask *other)
{
  return _gtk_allocated_bitmask_intersect (mask, other);
}

static inline GtkBitmask *
_gtk_bitmask_union (GtkBitmask       *mask,
                    const GtkBitmask *other)
{
  return _gtk_allocated_bitmask_union (mask, other);
}

static inline GtkBitmask *
_gtk_bitmask_subtract (GtkBitmask       *mask,
                       const GtkBitmask *other)
{
  return _gtk_allocated_bitmask_subtract (mask, other);
}

static inline gboolean
_gtk_bitmask_get (const GtkBitmask *mask,
                  guint             index_)
{
  return _gtk_allocated_bitmask_get (mask, index_);
}

static inline GtkBitmask *
_gtk_bitmask_set (GtkBitmask *mask,
                  guint       index_,
                  gboolean    value)
{
  return _gtk_allocated_bitmask_set (mask, index_, value);
}

static inline GtkBitmask *
_gtk_bitmask_invert_range (GtkBitmask *mask,
                           guint       start,
                           guint       end)
{
  return _gtk_allocated_bitmask_invert_range (mask, start, end);
}

static inline gboolean
_gtk_bitmask_is_empty (const GtkBitmask *mask)
{
  return _gtk_allocated_bitmask_is_empty (mask);
}

static inline gboolean
_gtk_bitmask_equals (const GtkBitmask *mask,
                     const GtkBitmask *other)
{
  return _gtk_allocated_bitmask_equals (mask, other);
}

static inline gboolean
_gtk_bitmask_intersects (const GtkBitmask *mask,
                         const GtkBitmask *other)
{
  return _gtk_allocated_bitmask_intersects (mask, other);
}
