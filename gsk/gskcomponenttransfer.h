/*
 * Copyright © 2025 Red Hat, Inc
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#if !defined (__GSK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif


#include <gsk/gsktypes.h>

G_BEGIN_DECLS

#define GSK_TYPE_COMPONENT_TRANSFER (gsk_component_transfer_get_type ())

GDK_AVAILABLE_IN_4_20
GType                   gsk_component_transfer_get_type         (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_4_20
GskComponentTransfer *  gsk_component_transfer_new_identity     (void);
GDK_AVAILABLE_IN_4_20
GskComponentTransfer *  gsk_component_transfer_new_levels       (float n);
GDK_AVAILABLE_IN_4_20
GskComponentTransfer *  gsk_component_transfer_new_linear       (float m,
                                                                 float b);
GDK_AVAILABLE_IN_4_20
GskComponentTransfer *  gsk_component_transfer_new_gamma        (float amp,
                                                                 float exp,
                                                                 float ofs);
GDK_AVAILABLE_IN_4_20
GskComponentTransfer *  gsk_component_transfer_new_discrete     (guint  n,
                                                                 float *values);
GDK_AVAILABLE_IN_4_20
GskComponentTransfer *  gsk_component_transfer_new_table        (guint  n,
                                                                 float *values);
GDK_AVAILABLE_IN_4_20
GskComponentTransfer *  gsk_component_transfer_copy             (const GskComponentTransfer *other);
GDK_AVAILABLE_IN_4_20
void                    gsk_component_transfer_free             (GskComponentTransfer *self);
GDK_AVAILABLE_IN_4_20
gboolean                gsk_component_transfer_equal            (gconstpointer self,
                                                                 gconstpointer other);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskComponentTransfer, gsk_component_transfer_free)

G_END_DECLS
