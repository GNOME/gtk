/*
 * Copyright © 2026 Benjamin Otte
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

#include "gtkallocationdetailsprivate.h"

/**
 * GtkAllocationDetails:
 *
 * Collects extra parameters that can be passed to [method@Gtk.Widget.allocate_detailed].
 *
 * Since: 4.26
 */

G_DEFINE_BOXED_TYPE (GtkAllocationDetails, gtk_allocation_details, gtk_allocation_details_copy, gtk_allocation_details_free)


/**
 * gtk_allocation_details_new:
 *
 * Creates a new `GtkAllocationDetails`.
 *
 * Returns: a new `GtkAllocationDetails`
 *
 * Since: 4.26
 */
GtkAllocationDetails *
gtk_allocation_details_new (void)
{
  GtkAllocationDetails *self;

  self = g_new0 (GtkAllocationDetails, 1);

  *self = GTK_ALLOCATION_DETAILS_INIT;

  return self;
}

/**
 * gtk_allocation_details_copy:
 * @other: the allocation_details to copy
 *
 * Creates a copy of a `GtkAllocationDetails`.
 *
 * Returns: a new `GtkAllocationDetails`. Use [method@Gtk.AllocationDetails.free] to free it
 *
 * Since: 4.26
 */
GtkAllocationDetails *
gtk_allocation_details_copy (const GtkAllocationDetails *other)
{
  GtkAllocationDetails *self;

  g_return_val_if_fail (other != NULL, NULL);

  self = g_new (GtkAllocationDetails, 1);

  *self = GTK_ALLOCATION_DETAILS_INIT_COPY (other);

  return self;
}

/**
 * gtk_allocation_details_free:
 * @self: a allocation_details
 *
 * Frees a `GtkAllocationDetails`.
 *
 * Since: 4.26
 */
void
gtk_allocation_details_free (GtkAllocationDetails *self)
{
  gtk_allocation_details_clear (self);

  g_free (self);
}

/**
 * gtk_allocation_details_set_baseline:
 * @self: the details
 * @baseline: the baseline
 *
 * Sets or unsets the baseline to use.
 *
 * A baseline of -1 unsets the baseline.
 *
 * By default, the baseline is unset.
 *
 * Since: 4.26
 */
void
gtk_allocation_details_set_baseline (GtkAllocationDetails *self,
                                     int                   baseline)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (baseline >= -1);

  self->baseline = baseline;
}

/**
 * gtk_allocation_details_get_baseline:
 * @self: the details
 *
 * Gets the baseline that was set via [method@Gtk.AllocationDetails.set_baseline]
 *
 * Returns: the baseline or -1 if unset
 *
 * Since: 4.26
 */
int
gtk_allocation_details_get_baseline (const GtkAllocationDetails *self)
{
  g_return_val_if_fail (self != NULL, -1);

  return self->baseline;
}

/**
 * gtk_allocation_details_set_transform:
 * @self: the details
 * @transform: (nullable): The transform to use
 *
 * Sets the transform that will be applied to the parent's coordinate space
 * before allocating the size.
 *
 * By default, the identity transform is used.
 *
 * Since: 4.26
 */
void
gtk_allocation_details_set_transform (GtkAllocationDetails  *self,
                                      GskTransform          *transform)
{
  g_return_if_fail (self != NULL);

  self->transform = gsk_transform_ref (transform);
}

/**
 * gtk_allocation_details_get_transform:
 * @self: the details
 *
 * Gets the transform that was set via [method@Gtk.AllocationDetails.set_transform]
 *
 * Returns: (nullable): the transform
 *
 * Since: 4.26
 */
GskTransform *
gtk_allocation_details_get_transform (const GtkAllocationDetails *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->transform;
}
