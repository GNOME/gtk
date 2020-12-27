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


#ifndef __GSK_CONTOUR_PRIVATE_H__
#define __GSK_CONTOUR_PRIVATE_H__

#include <gskpath.h>

#include "gskpathopprivate.h"

G_BEGIN_DECLS

typedef enum 
{
  GSK_PATH_FLAT,
  GSK_PATH_CLOSED
} GskPathFlags;

typedef struct _GskContour GskContour;

GskContour *            gsk_rect_contour_new                    (const graphene_rect_t  *rect);
GskContour *            gsk_circle_contour_new                  (const graphene_point_t *center,
                                                                 float                   radius,
                                                                 float                   start_angle,
                                                                 float                   end_angle);
GskContour *            gsk_standard_contour_new                (GskPathFlags            flags,
                                                                 const graphene_point_t *points,
                                                                 gsize                   n_points,
                                                                 const gskpathop        *ops,
                                                                 gsize                   n_ops,
                                                                 gssize                  offset);

void                    gsk_contour_copy                        (GskContour *            dest,
                                                                 const GskContour       *src);
GskContour *            gsk_contour_dup                         (const GskContour       *src);

gsize                   gsk_contour_get_size                    (const GskContour       *self);
GskPathFlags            gsk_contour_get_flags                   (const GskContour       *self);
void                    gsk_contour_print                       (const GskContour       *self,
                                                                 GString                *string);
gboolean                gsk_contour_get_bounds                  (const GskContour       *self,
                                                                 graphene_rect_t        *bounds);
gpointer                gsk_contour_init_measure                (const GskContour       *self,
                                                                 float                   tolerance,
                                                                 float                  *out_length);
void                    gsk_contour_free_measure                (const GskContour       *self,
                                                                 gpointer                data);
gboolean                gsk_contour_foreach                     (const GskContour       *self,
                                                                 float                   tolerance,
                                                                 GskPathForeachFunc      func,
                                                                 gpointer                user_data);
void                    gsk_contour_get_start_end               (const GskContour       *self,
                                                                 graphene_point_t       *start,
                                                                 graphene_point_t       *end);
void                    gsk_contour_get_point                   (const GskContour       *self,
                                                                 gpointer                measure_data,
                                                                 float                   distance,
                                                                 graphene_point_t       *pos,
                                                                 graphene_vec2_t        *tangent);
gboolean                gsk_contour_get_closest_point           (const GskContour       *self,
                                                                 gpointer                measure_data,
                                                                 float                   tolerance,
                                                                 const graphene_point_t *point,
                                                                 float                   threshold,
                                                                 float                  *out_distance,
                                                                 graphene_point_t       *out_pos,
                                                                 float                  *out_offset,
                                                                 graphene_vec2_t        *out_tangent);
int                     gsk_contour_get_winding                 (const GskContour       *self,
                                                                 gpointer                measure_data,
                                                                 const graphene_point_t *point,
                                                                 gboolean               *on_edge);
void                    gsk_contour_add_segment                 (const GskContour       *self,
                                                                 GskPathBuilder         *builder,
                                                                 gpointer                measure_data,
                                                                 gboolean                emit_move_to,
                                                                 float                   start,
                                                                 float                   end);
gboolean                gsk_contour_get_stroke_bounds           (const GskContour       *self,
                                                                 const GskStroke        *stroke,
                                                                 graphene_rect_t        *bounds);
void                    gsk_contour_add_stroke                  (const GskContour       *contour,
                                                                 GskPathBuilder         *builder,
                                                                 GskStroke              *stroke);
void                    gsk_contour_default_add_stroke          (const GskContour       *contour,
                                                                 GskPathBuilder         *builder,
                                                                 GskStroke              *stroke);

void                    gsk_contour_offset                      (const GskContour       *contour,
                                                                 GskPathBuilder         *builder,
                                                                 float                   distance,
                                                                 GskLineJoin             line_join,
                                                                 float                   miter_limit);

G_END_DECLS

#endif /* __GSK_CONTOUR_PRIVATE_H__ */

