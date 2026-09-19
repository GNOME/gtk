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

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif


#include <gtk/gtktypes.h>
#include <gsk/gsktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_ALLOCATION_DETAILS (gtk_allocation_details_get_type ())

GDK_AVAILABLE_IN_4_26
GType                   gtk_allocation_details_get_type                 (void);
GDK_AVAILABLE_IN_4_26
GtkAllocationDetails *  gtk_allocation_details_new                      (void);
GDK_AVAILABLE_IN_4_26
GtkAllocationDetails *  gtk_allocation_details_copy                     (const GtkAllocationDetails    *other);
GDK_AVAILABLE_IN_4_26
void                    gtk_allocation_details_free                     (GtkAllocationDetails          *self);

GDK_AVAILABLE_IN_4_26
void                    gtk_allocation_details_set_baseline             (GtkAllocationDetails          *self,
                                                                         int                            baseline);
GDK_AVAILABLE_IN_4_26
int                     gtk_allocation_details_get_baseline             (const GtkAllocationDetails    *self);
GDK_AVAILABLE_IN_4_26
void                    gtk_allocation_details_set_transform            (GtkAllocationDetails          *self,
                                                                         GskTransform                  *transform);
GDK_AVAILABLE_IN_4_26
GskTransform *          gtk_allocation_details_get_transform            (const GtkAllocationDetails    *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtkAllocationDetails, gtk_allocation_details_free)

G_END_DECLS
