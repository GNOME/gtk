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
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkbitset.h"

#include "roaring.c"

/**
 * GtkBitset: (ref-func gtk_bitset_ref) (unref-func gtk_bitset_unref)
 *
 * The `GtkBitset` structure contains only private data.
 */
struct _GtkBitset
{
  gint ref_count;
  roaring_bitmap_t roaring;
};


G_DEFINE_BOXED_TYPE (GtkBitset, gtk_bitset,
                     gtk_bitset_ref,
                     gtk_bitset_unref)

/**
 * gtk_bitset_ref:
 * @self: (allow-none): a #GtkBitset
 *
 * Acquires a reference on the given #GtkBitset.
 *
 * Returns: (transfer none): the #GtkBitset with an additional reference
 */
GtkBitset *
gtk_bitset_ref (GtkBitset *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  self->ref_count += 1;

  return self;
}

/**
 * gtk_bitset_unref:
 * @self: (allow-none): a #GtkBitset
 *
 * Releases a reference on the given #GtkBitset.
 *
 * If the reference was the last, the resources associated to the @self are
 * freed.
 */
void
gtk_bitset_unref (GtkBitset *self)
{
  g_return_if_fail (self == NULL);
  g_return_if_fail (self->ref_count > 1);

  self->ref_count -= 1;
  if (self->ref_count > 0)
    return;

  ra_clear (&self->roaring.high_low_container);
  g_slice_free (GtkBitset, self);
}

/**
 * gtk_bitset_contains:
 * @self: a #GtkBitset
 * @value: the value to check
 *
 * Checks if the given @value has been added to @bitset
 *
 * Returns: %TRUE if @self contains @value
 **/
gboolean
gtk_bitset_contains (GtkBitset *self,
                     guint      value)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return roaring_bitmap_contains (&self->roaring, value);
}

/**
 * gtk_bitset_is_empty:
 * @self: a #GtkBitset
 *
 * Check if no value is contained in bitset.
 *
 * Returns: %TRUE if @self is empty
 **/
gboolean
gtk_bitset_is_empty (GtkBitset *self)
{
  g_return_val_if_fail (self != NULL, TRUE);

  return roaring_bitmap_is_empty (&self->roaring);
}

/**
 * gtk_bitset_new_empty:
 *
 * Creates a new empty bitset.
 *
 * Returns: A new empty bitset.
 **/
GtkBitset *
gtk_bitset_new_empty (void)
{
  GtkBitset *self;

  self = g_slice_new0 (GtkBitset);

  self->ref_count = 1;

  ra_init (&self->roaring.high_low_container);

  return self;
}

/**
 * gtk_bitset_add:
 * @self: a #GtkBitset
 * @value: value to add
 *
 * Adds @value to @self if it wasn't part of it before.
 *
 * Returns: %TRUE if @value was not part of @self and @self
 *     was changed.
 **/
gboolean
gtk_bitset_add (GtkBitset *self,
                guint      value)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return roaring_bitmap_add_checked (&self->roaring, value);
}

/**
 * gtk_bitset_remove:
 * @self: a #GtkBitset
 * @value: value to add
 *
 * Adds @value to @self if it wasn't part of it before.
 *
 * Returns: %TRUE if @value was part of @self and @self
 *     was changed.
 **/
gboolean
gtk_bitset_remove (GtkBitset *self,
                   guint      value)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return roaring_bitmap_remove_checked (&self->roaring, value);
}

#if 0
GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_add_range                    (GtkBitset              *self,
                                                                 guint                   start,
                                                                 guint                   n_items);
GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_remove_range                 (GtkBitset              *self,
                                                                 guint                   start,
                                                                 guint                   n_items);
GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_add_range_closed             (GtkBitset              *self,
                                                                 guint                   first,
                                                                 guint                   last);
GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_remove_range_closed          (GtkBitset              *self,
                                                                 guint                   first,
                                                                 guint                   last);

GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_union                        (GtkBitset              *self,
                                                                 const GtkBitset        *other);
GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_intersect                    (GtkBitset              *self,
                                                                 const GtkBitset        *other);
GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_subtract                     (GtkBitset              *self,
                                                                 const GtkBitset        *other);
GDK_AVAILABLE_IN_ALL
void                    gtk_bitset_difference                   (GtkBitset              *self,
                                                                 const GtkBitset        *other);
#endif
