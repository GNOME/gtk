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

#ifndef __GSK_PATH_H__
#define __GSK_PATH_H__

#if !defined (__GSK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif


#include <gsk/gsktypes.h>

G_BEGIN_DECLS

#define GSK_TYPE_PATH (gsk_path_get_type ())

GDK_AVAILABLE_IN_ALL
GType                   gsk_path_get_type                       (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GskPath *               gsk_path_new_from_cairo                 (const cairo_path_t     *path);

GDK_AVAILABLE_IN_ALL
GskPath *               gsk_path_ref                            (GskPath                *self);
GDK_AVAILABLE_IN_ALL
void                    gsk_path_unref                          (GskPath                *self);

GDK_AVAILABLE_IN_ALL
void                    gsk_path_print                          (GskPath                *self,
                                                                 GString                *string);
GDK_AVAILABLE_IN_ALL
char *                  gsk_path_to_string                      (GskPath                *self);
GDK_AVAILABLE_IN_ALL
void                    gsk_path_to_cairo                       (GskPath                *self,
                                                                 cairo_t                *cr);

GDK_AVAILABLE_IN_ALL
gboolean                gsk_path_is_empty                       (GskPath                *self);
GDK_AVAILABLE_IN_ALL
gboolean                gsk_path_get_bounds                     (GskPath                *self,
                                                                 graphene_rect_t        *bounds);

#define GSK_TYPE_PATH_BUILDER (gsk_path_builder_get_type ())

typedef struct _GskPathBuilder GskPathBuilder;

GDK_AVAILABLE_IN_ALL
GType                   gsk_path_builder_get_type               (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GskPathBuilder *        gsk_path_builder_new                    (void);
GDK_AVAILABLE_IN_ALL
GskPathBuilder *        gsk_path_builder_ref                    (GskPathBuilder         *builder);
GDK_AVAILABLE_IN_ALL
void                    gsk_path_builder_unref                  (GskPathBuilder         *builder);
GDK_AVAILABLE_IN_ALL
GskPath *               gsk_path_builder_free_to_path           (GskPathBuilder         *builder) G_GNUC_WARN_UNUSED_RESULT;
GDK_AVAILABLE_IN_ALL
GskPath *               gsk_path_builder_to_path                (GskPathBuilder         *builder) G_GNUC_WARN_UNUSED_RESULT;
GDK_AVAILABLE_IN_ALL
void                    gsk_path_builder_add_rect               (GskPathBuilder         *builder,
                                                                 const graphene_rect_t  *rect);
GDK_AVAILABLE_IN_ALL
void                    gsk_path_builder_move_to                (GskPathBuilder         *builder,
                                                                 float                   x,
                                                                 float                   y);
GDK_AVAILABLE_IN_ALL
void                    gsk_path_builder_line_to                (GskPathBuilder         *builder,
                                                                 float                   x,
                                                                 float                   y);
GDK_AVAILABLE_IN_ALL
void                    gsk_path_builder_curve_to               (GskPathBuilder         *builder,
                                                                 float                   x1,
                                                                 float                   y1,
                                                                 float                   x2,
                                                                 float                   y2,
                                                                 float                   x3,
                                                                 float                   y3);
GDK_AVAILABLE_IN_ALL
void                    gsk_path_builder_close                  (GskPathBuilder         *builder);

G_END_DECLS

#endif /* __GSK_PATH_H__ */
