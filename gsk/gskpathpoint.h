/*
 * Copyright © 2023 Red Hat, Inc.
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

#define GSK_TYPE_PATH_POINT (gsk_path_point_get_type ())

GDK_AVAILABLE_IN_4_14
GType                   gsk_path_point_get_type        (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_14
GskPathPoint *          gsk_path_point_ref             (GskPathPoint *self);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_point_unref           (GskPathPoint *self);

GDK_AVAILABLE_IN_4_14
void                    gsk_path_point_get_position    (GskPathPoint     *self,
                                                        graphene_point_t *position);

GDK_AVAILABLE_IN_4_14
void                    gsk_path_point_get_tangent     (GskPathPoint     *self,
                                                        GskPathDirection  direction,
                                                        graphene_vec2_t  *tangent);

GDK_AVAILABLE_IN_4_14
float                   gsk_path_point_get_curvature   (GskPathPoint     *self,
                                                        graphene_point_t *center);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskPathPoint, gsk_path_point_unref)

G_END_DECLS
