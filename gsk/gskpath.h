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


#include <gsk/gsktypes.h>

G_BEGIN_DECLS

/**
 * GskPathForeachFlags:
 * @GSK_PATH_FOREACH_ALLOW_ONLY_LINES: The default behavior, only allow lines.
 * @GSK_PATH_FOREACH_ALLOW_QUAD: Allow emission of `GSK_PATH_QUAD` operations
 * @GSK_PATH_FOREACH_ALLOW_CUBIC: Allow emission of `GSK_PATH_CUBIC` operations.
 * @GSK_PATH_FOREACH_ALLOW_CONIC: Allow emission of `GSK_PATH_CONIC` operations.
 *
 * Flags that can be passed to gsk_path_foreach() to influence what
 * kinds of operations the path is decomposed into.
 *
 * By default, [method@Gsk.Path.foreach] will only emit a path with all
 * operations flattened to straight lines to allow for maximum compatibility.
 * The only operations emitted will be `GSK_PATH_MOVE`, `GSK_PATH_LINE` and
 * `GSK_PATH_CLOSE`.
 *
 * Since: 4.14
 */
typedef enum
{
  GSK_PATH_FOREACH_ALLOW_ONLY_LINES = 0,
  GSK_PATH_FOREACH_ALLOW_QUAD       = (1 << 0),
  GSK_PATH_FOREACH_ALLOW_CUBIC      = (1 << 1),
  GSK_PATH_FOREACH_ALLOW_CONIC      = (1 << 2),
} GskPathForeachFlags;

typedef struct _GskQuadControl GskQuadControl;
struct _GskQuadControl
{
  graphene_point_t control;
};

typedef struct _GskCubicControl GskCubicControl;
struct _GskCubicControl
{
  graphene_point_t control1;
  graphene_point_t control2;
};

typedef struct _GskConicControl GskConicControl;
struct _GskConicControl
{
  graphene_point_t control;
  float weight;
};

typedef struct _GskPathControl GskPathControl;
struct _GskPathControl
{
  GskPathOperation op;

  union {
    GskQuadControl quad;
    GskCubicControl cubic;
    GskConicControl conic;
  };
};

/**
 * GskPathForeachFunc:
 * @start: The start point of the operation
 * @end: The end point of the operation
 * @control: The control data for the operation
 * @user_data: The user data provided with the function
 *
 * Prototype of the callback to iterate through the operations of
 * a path.
 *
 * For each operation, the callback is given the @op itself, the start-
 * and endpoint, and the control data, depending on the kind of operation.
 *
 * Each contour of the path starts with a @GSK_PATH_MOVE operation.
 * Closed contours end with a @GSK_PATH_CLOSE operation.
 *
 * Returns: %TRUE to continue iterating the path, %FALSE to
 *   immediately abort and not call the function again.
 */
typedef gboolean (* GskPathForeachFunc) (const graphene_point_t *start,
                                         const graphene_point_t *end,
                                         const GskPathControl   *control,
                                         gpointer                user_data);

#define GSK_TYPE_PATH (gsk_path_get_type ())

GDK_AVAILABLE_IN_4_14
GType                   gsk_path_get_type                       (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_14
GskPath *               gsk_path_ref                            (GskPath                *self);
GDK_AVAILABLE_IN_4_14
void                    gsk_path_unref                          (GskPath                *self);

GDK_AVAILABLE_IN_4_14
void                    gsk_path_print                          (GskPath                *self,
                                                                 GString                *string);
GDK_AVAILABLE_IN_4_14
char *                  gsk_path_to_string                      (GskPath                *self);

GDK_AVAILABLE_IN_4_14
GskPath *               gsk_path_parse                          (const char             *string);

GDK_AVAILABLE_IN_4_14
void                    gsk_path_to_cairo                       (GskPath                *self,
                                                                 cairo_t                *cr);

GDK_AVAILABLE_IN_4_14
gboolean                gsk_path_is_empty                       (GskPath                *self);

GDK_AVAILABLE_IN_4_14
gboolean                gsk_path_is_closed                      (GskPath                *self);

GDK_AVAILABLE_IN_4_14
gboolean                gsk_path_get_bounds                     (GskPath                *self,
                                                                 graphene_rect_t        *bounds);

GDK_AVAILABLE_IN_4_14
gboolean                gsk_path_get_stroke_bounds              (GskPath                *self,
                                                                 const GskStroke        *stroke,
                                                                 graphene_rect_t        *bounds);

GDK_AVAILABLE_IN_4_14
gboolean                gsk_path_in_fill                        (GskPath                *self,
                                                                 const graphene_point_t *point,
                                                                 GskFillRule             fill_rule);

GDK_AVAILABLE_IN_4_14
gboolean                gsk_path_get_start_point                (GskPath                *self,
                                                                 GskPathPoint           *result);
GDK_AVAILABLE_IN_4_14
gboolean                gsk_path_get_end_point                  (GskPath                *self,
                                                                 GskPathPoint           *result);

GDK_AVAILABLE_IN_4_14
gboolean                gsk_path_get_closest_point              (GskPath                *self,
                                                                 const graphene_point_t *point,
                                                                 float                   threshold,
                                                                 GskPathPoint           *result);

GDK_AVAILABLE_IN_4_14
gboolean                gsk_path_foreach                        (GskPath                *self,
                                                                 GskPathForeachFlags     flags,
                                                                 GskPathForeachFunc      func,
                                                                 gpointer                user_data);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskPath, gsk_path_unref)

G_END_DECLS
