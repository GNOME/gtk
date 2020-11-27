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


#ifndef __GSK_PATH_PRIVATE_H__
#define __GSK_PATH_PRIVATE_H__

#include "gskpath.h"

G_BEGIN_DECLS

/* Same as Skia, so looks like a good value. ¯\_(ツ)_/¯ */
#define GSK_PATH_TOLERANCE_DEFAULT (0.5)

typedef enum 
{
  GSK_PATH_FLAT,
  GSK_PATH_CLOSED
} GskPathFlags;

typedef struct _GskContour GskContour;
typedef struct _GskContourClass GskContourClass;

typedef struct _GskStandardOperation GskStandardOperation;

struct _GskStandardOperation {
  GskPathOperation op;
  gsize point; /* index into points array of the start point (last point of previous op) */
};

GskContour *            gsk_rect_contour_new                    (const graphene_rect_t  *rect);
GskContour *            gsk_circle_contour_new                  (const graphene_point_t *center,
                                                                 float                   radius,
                                                                 float                   start_angle,
                                                                 float                   end_angle);
GskContour *            gsk_standard_contour_new                (GskPathFlags            flags,
                                                                 const                   GskStandardOperation *ops,
                                                                 gsize                   n_ops,
                                                                 const                   graphene_point_t *points,
                                                                 gsize                   n_points);

GskPath *               gsk_path_new_from_contours              (const GSList           *contours);

gsize                   gsk_path_get_n_contours                 (GskPath              *path);
const GskContour *      gsk_path_get_contour                    (GskPath              *path,
                                                                 gsize                 i);
gboolean                gsk_path_foreach_with_tolerance         (GskPath              *self,
                                                                 double                tolerance,
                                                                 GskPathForeachFunc    func,
                                                                 gpointer              user_data);

GskContour *            gsk_contour_dup                         (const GskContour     *src);
gpointer                gsk_contour_init_measure                (GskPath              *path,
                                                                 gsize                 i,
                                                                 float                 tolerance,
                                                                 float                *out_length);
void                    gsk_contour_free_measure                (GskPath              *path,
                                                                 gsize                 i,
                                                                 gpointer              data);
void                    gsk_contour_get_start_end               (const GskContour       *self,
                                                                 graphene_point_t       *start,
                                                                 graphene_point_t       *end);
void                    gsk_contour_get_point                   (GskPath              *path,
                                                                 gsize                 i,
                                                                 gpointer              measure_data,
                                                                 float                 distance,
                                                                 graphene_point_t     *pos,
                                                                 graphene_vec2_t      *tangent);
gboolean                gsk_contour_get_closest_point           (GskPath                *path,
                                                                 gsize                   i,
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
                                                                 float                   start,
                                                                 float                   end);

void                    gsk_path_builder_add_contour            (GskPathBuilder         *builder,
                                                                 GskContour             *contour);

void                    gsk_path_builder_svg_arc_to             (GskPathBuilder         *builder,
                                                                 float                   rx,
                                                                 float                   ry,
                                                                 float                   x_axis_rotation,
                                                                 gboolean                large_arc,
                                                                 gboolean                positive_sweep,
                                                                 float                   x,
                                                                 float                   y);

G_END_DECLS

#endif /* __GSK_PATH_PRIVATE_H__ */

