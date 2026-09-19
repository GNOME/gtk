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


#pragma once

#include "gtkallocationdetails.h"

#include <gsk/gsktransform.h>

struct _GtkAllocationDetails
{
  int baseline;
  GskTransform *transform;
};

#define GTK_ALLOCATION_DETAILS_INIT ((GtkAllocationDetails) {\
    .baseline = -1, \
    .transform = NULL, \
})

#define GTK_ALLOCATION_DETAILS_INIT_COPY(_other) ((GtkAllocationDetails) {\
    .baseline = _other->baseline, \
    .transform = gsk_transform_ref (_other->transform), \
})

static inline void
gtk_allocation_details_clear (GtkAllocationDetails *self)
{
  g_clear_pointer (&self->transform, gsk_transform_unref);
}

