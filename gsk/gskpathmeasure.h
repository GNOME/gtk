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

#ifndef __GSK_PATH_MEASURE_H__
#define __GSK_PATH_MEASURE_H__

#if !defined (__GSK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif


#include <gsk/gskpath.h>

G_BEGIN_DECLS

#define GSK_TYPE_PATH_MEASURE (gsk_path_measure_get_type ())

GDK_AVAILABLE_IN_ALL
GType                   gsk_path_measure_get_type               (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GskPathMeasure *        gsk_path_measure_new                    (GskPath                *path);
GDK_AVAILABLE_IN_ALL
GskPathMeasure *        gsk_path_measure_new_with_tolerance     (GskPath                *path,
                                                                 float                   tolerance);

GDK_AVAILABLE_IN_ALL
GskPathMeasure *        gsk_path_measure_ref                    (GskPathMeasure         *self);
GDK_AVAILABLE_IN_ALL
void                    gsk_path_measure_unref                  (GskPathMeasure         *self);

GDK_AVAILABLE_IN_ALL
float                   gsk_path_measure_get_length             (GskPathMeasure         *self);
GDK_AVAILABLE_IN_ALL
void                    gsk_path_measure_get_point              (GskPathMeasure         *self,
                                                                 float                   distance,
                                                                 graphene_point_t       *pos,
                                                                 graphene_vec2_t        *tangent);
GDK_AVAILABLE_IN_ALL
float                   gsk_path_measure_get_closest_point      (GskPathMeasure         *self,
                                                                 const graphene_point_t *point,
                                                                 graphene_point_t       *out_pos);
GDK_AVAILABLE_IN_ALL
gboolean                gsk_path_measure_get_closest_point_full (GskPathMeasure         *self,
                                                                 const graphene_point_t *point,
                                                                 float                   threshold,
                                                                 float                  *out_distance,
                                                                 graphene_point_t       *out_pos,
                                                                 float                  *out_offset,
                                                                 graphene_vec2_t        *out_tangent);

GDK_AVAILABLE_IN_ALL
void                    gsk_path_measure_add_segment            (GskPathMeasure         *self,
                                                                 GskPathBuilder         *builder,
                                                                 float                   start,
                                                                 float                   end);

GDK_AVAILABLE_IN_ALL
gboolean                gsk_path_measure_in_fill                (GskPathMeasure         *self,
                                                                 graphene_point_t       *point,
                                                                 GskFillRule             fill_rule);

G_END_DECLS

#endif /* __GSK_PATH_MEASURE_H__ */
