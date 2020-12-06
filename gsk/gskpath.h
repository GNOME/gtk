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

/**
 * GskPathForeachFlags:
 * @GSK_PATH_FOREACH_ALLOW_CURVE: Allow emission of `GSK_PATH_CURVE` operations.
 * @GSK_PATH_FOREACH_ALLOW_CONIC: Allow emission of `GSK_PATH_CONIC` operations.
 *
 * Flags that can be passed to [method@Gsk.Path.foreach] to enable additional
 * features.
 *
 * By default, [method@Gsk.Path.foreach] will only emit a path with all
 * operations flattened to straight lines to allow for maximum compatibility.
 * The only operations emitted will be `GSK_PATH_MOVE`, `GSK_PATH_LINE` and
 * `GSK_PATH_CLOSE`.
 */
typedef enum
{
  GSK_PATH_FOREACH_ALLOW_CURVE = (1 << 0),
  GSK_PATH_FOREACH_ALLOW_CONIC = (1 << 1)
} GskPathForeachFlags;

/**
 * GskPathForeachFunc:
 * @op: The operation to perform
 * @pts: The points of the operation
 * @n_pts: The number of points
 * @weight: The weight for conic curves, or unused if not a conic curve.
 * @user_data: The user data provided with the function
 *
 * Prototype of the callback to iterate throught the operations of
 * a path.
 *
 * Returns: %TRUE to continue evaluating the path, %FALSE to
 *   immediately abort and not call the function again.
 */
typedef gboolean (* GskPathForeachFunc) (GskPathOperation        op,
                                         const graphene_point_t *pts,
                                         gsize                   n_pts,
                                         float                   weight,
                                         gpointer                user_data);

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
GskPath *               gsk_path_parse                          (const char             *string);

GDK_AVAILABLE_IN_ALL
void                    gsk_path_to_cairo                       (GskPath                *self,
                                                                 cairo_t                *cr);

GDK_AVAILABLE_IN_ALL
gboolean                gsk_path_is_empty                       (GskPath                *self);
GDK_AVAILABLE_IN_ALL
gboolean                gsk_path_get_bounds                     (GskPath                *self,
                                                                 graphene_rect_t        *bounds);
GDK_AVAILABLE_IN_ALL
gboolean                gsk_path_get_stroke_bounds              (GskPath                *path,
                                                                 const GskStroke        *stroke,
                                                                 graphene_rect_t        *bounds);

GDK_AVAILABLE_IN_ALL
gboolean                gsk_path_foreach                        (GskPath                *self,
                                                                 GskPathForeachFlags     flags,
                                                                 GskPathForeachFunc      func,
                                                                 gpointer                user_data);

G_END_DECLS

#endif /* __GSK_PATH_H__ */
