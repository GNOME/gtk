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

#pragma once

#if !defined (__GSK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif


#include <gsk/gskroundedrect.h>
#include <gsk/gsktypes.h>

G_BEGIN_DECLS

#define GSK_TYPE_PATH_BUILDER (gsk_path_builder_get_type ())

GType                   gsk_path_builder_get_type               (void) G_GNUC_CONST;

GskPathBuilder *        gsk_path_builder_new                    (void);
GskPathBuilder *        gsk_path_builder_ref                    (GskPathBuilder         *self);
void                    gsk_path_builder_unref                  (GskPathBuilder         *self);
GskPath *               gsk_path_builder_free_to_path           (GskPathBuilder         *self) G_GNUC_WARN_UNUSED_RESULT;
GskPath *               gsk_path_builder_to_path                (GskPathBuilder         *self) G_GNUC_WARN_UNUSED_RESULT;

const graphene_point_t *gsk_path_builder_get_current_point      (GskPathBuilder         *self);

void                    gsk_path_builder_add_path               (GskPathBuilder         *self,
                                                                 GskPath                *path);
void                    gsk_path_builder_add_reverse_path       (GskPathBuilder         *self,
                                                                 GskPath                *path);
void                    gsk_path_builder_add_cairo_path         (GskPathBuilder         *self,
                                                                 const cairo_path_t     *path);
void                    gsk_path_builder_add_layout             (GskPathBuilder         *self,
                                                                 PangoLayout            *layout);
void                    gsk_path_builder_add_rect               (GskPathBuilder         *self,
                                                                 const graphene_rect_t  *rect);
void                    gsk_path_builder_add_rounded_rect       (GskPathBuilder         *self,
                                                                 const GskRoundedRect   *rect);
void                    gsk_path_builder_add_circle             (GskPathBuilder         *self,
                                                                 const graphene_point_t *center,
                                                                 float                   radius);
void                    gsk_path_builder_add_ellipse            (GskPathBuilder         *self,
                                                                 const graphene_point_t *center,
                                                                 const graphene_size_t  *radius);
void                    gsk_path_builder_add_segment            (GskPathBuilder         *self,
                                                                 GskPathMeasure         *measure,
                                                                 float                   start,
                                                                 float                   end);
void                    gsk_path_builder_move_to                (GskPathBuilder         *self,
                                                                 float                   x,
                                                                 float                   y);
void                    gsk_path_builder_rel_move_to            (GskPathBuilder         *self,
                                                                 float                   x,
                                                                 float                   y);
void                    gsk_path_builder_line_to                (GskPathBuilder         *self,
                                                                 float                   x,
                                                                 float                   y);
void                    gsk_path_builder_rel_line_to            (GskPathBuilder         *self,
                                                                 float                   x,
                                                                 float                   y);
void                    gsk_path_builder_quad_to                (GskPathBuilder         *self,
                                                                 float                   x1,
                                                                 float                   y1,
                                                                 float                   x2,
                                                                 float                   y2);
void                    gsk_path_builder_rel_quad_to            (GskPathBuilder         *self,
                                                                 float                   x1,
                                                                 float                   y1,
                                                                 float                   x2,
                                                                 float                   y2);
void                    gsk_path_builder_cubic_to               (GskPathBuilder         *self,
                                                                 float                   x1,
                                                                 float                   y1,
                                                                 float                   x2,
                                                                 float                   y2,
                                                                 float                   x3,
                                                                 float                   y3);
void                    gsk_path_builder_rel_cubic_to           (GskPathBuilder         *self,
                                                                 float                   x1,
                                                                 float                   y1,
                                                                 float                   x2,
                                                                 float                   y2,
                                                                 float                   x3,
                                                                 float                   y3);
void                    gsk_path_builder_conic_to               (GskPathBuilder         *self,
                                                                 float                   x1,
                                                                 float                   y1,
                                                                 float                   x2,
                                                                 float                   y2,
                                                                 float                   weight);
void                    gsk_path_builder_rel_conic_to           (GskPathBuilder         *self,
                                                                 float                   x1,
                                                                 float                   y1,
                                                                 float                   x2,
                                                                 float                   y2,
                                                                 float                   weight);
void                    gsk_path_builder_close                  (GskPathBuilder         *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskPathBuilder, gsk_path_builder_unref)

G_END_DECLS
