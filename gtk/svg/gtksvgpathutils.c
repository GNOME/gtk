/*
 * Copyright © 2025 Red Hat, Inc
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

#include "config.h"

#include "gtksvgpathutilsprivate.h"

#include "gsk/gskroundedrectprivate.h"
#include "gsk/gskpathprivate.h"
#include "gsk/gskcontourprivate.h"

#include <math.h>


/* {{{ Builder */

void
path_builder_add_ellipse (GskPathBuilder *builder,
                          double cx, double cy,
                          double rx, double ry)
{
  gsk_path_builder_move_to  (builder, cx + rx, cy);
  gsk_path_builder_conic_to (builder, cx + rx, cy + ry,
                                      cx,      cy + ry, M_SQRT1_2);
  gsk_path_builder_conic_to (builder, cx - rx, cy + ry,
                                      cx - rx, cy,      M_SQRT1_2);
  gsk_path_builder_conic_to (builder, cx - rx, cy - ry,
                                      cx,      cy - ry, M_SQRT1_2);
  gsk_path_builder_conic_to (builder, cx + rx, cy - ry,
                                      cx + rx, cy,      M_SQRT1_2);
  gsk_path_builder_close    (builder);
}

/* }}} */
/* {{{ Path transformation */

typedef struct
{
  GskPathBuilder *builder;
  GskTransform *transform;
} PathTransformData;

static gboolean
add_op_transformed (GskPathOperation        op,
                    const graphene_point_t *_pts,
                    size_t                   n_pts,
                    float                   weight,
                    gpointer                user_data)
{
  PathTransformData *t = user_data;
  graphene_point_t pts[4];

  for (unsigned int i = 0; i < n_pts; i++)
    gsk_transform_transform_point (t->transform, &_pts[i], &pts[i]);

  gsk_path_builder_add_op (t->builder, op, pts, n_pts, weight);

  return TRUE;
}

/* Transform a path by applying the transform
 * to every point.
 */
GskPath *
svg_transform_path (GskTransform *transform,
                    GskPath      *path)
{
  PathTransformData data;

  data.builder = gsk_path_builder_new ();
  data.transform = transform;

  gsk_path_foreach (path, (GskPathForeachFlags) -1, add_op_transformed, &data);

  return gsk_path_builder_free_to_path (data.builder);
}

/* }}} */
/* {{{ Snapshotting */

/* Like gtk_snapshot_push_fill, but more efficient */

void
svg_snapshot_push_fill (GtkSnapshot *snapshot,
                        GskPath     *path,
                        GskFillRule  rule)
{
  GskRoundedRect rect = { 0, };

  switch (gsk_path_classify (path, &rect))
    {
    case GSK_PATH_RECT:
      gtk_snapshot_push_clip (snapshot, &rect.bounds);
      break;
    case GSK_PATH_ROUNDED_RECT:
    case GSK_PATH_CIRCLE:
    case GSK_PATH_APPROXIMATE_ROUNDED_RECT:
    case GSK_PATH_APPROXIMATE_CIRCLE:
      gtk_snapshot_push_rounded_clip (snapshot, &rect);
      break;
    case GSK_PATH_GENERAL:
      gtk_snapshot_push_fill (snapshot, path, rule);
      break;
    case GSK_PATH_EMPTY:
      gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (0, 0, 0, 0));
      break;
    default:
      g_assert_not_reached ();
    }
}

/* }}} */

/* vim:set foldmethod=marker: */

