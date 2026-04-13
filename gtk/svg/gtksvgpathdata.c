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

#include "gtksvgpathdataprivate.h"
#include "gsk/gskpathprivate.h"
#include "gtksvgutilsprivate.h"
#include "gtksvgstringutilsprivate.h"

/* We have our own path data structure here instead of
 * just relying on GskPath for two reasons:
 * - SVG considers A to be a primitive and requires
 *   it to be interpolated as such
 * - SVG does not have conics (O)
 */

#define SVG_PATH_ARC 22

typedef struct
{
  unsigned int op;
  union {
    struct {
      graphene_point_t pts[4];
    } seg;
    struct {
      float rx;
      float ry;
      float x_axis_rotation;
      gboolean large_arc;
      gboolean positive_sweep;
      float x;
      float y;
    } arc;
  };
} SvgPathOp;

#define GDK_ARRAY_NAME svg_path_ops
#define GDK_ARRAY_TYPE_NAME SvgPathOps
#define GDK_ARRAY_ELEMENT_TYPE SvgPathOp
#define GDK_ARRAY_PREALLOC 112
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

/* We want to choose the array preallocations above so that the
 * struct fits in 1 page
 */
G_STATIC_ASSERT (sizeof (SvgPathOps) < 4096);

struct _SvgPathData
{
  SvgPathOp *ops;
  size_t n_ops;
};

static SvgPathData *
svg_path_data_new (size_t     n_ops,
                   SvgPathOp *ops)
{
  SvgPathData *p = g_new0 (SvgPathData, 1);
  p->n_ops = n_ops;
  p->ops = ops;
  return p;
}

void
svg_path_data_free (SvgPathData *p)
{
  g_free (p->ops);
  g_free (p);
}

static inline size_t
path_data_points_per_op (GskPathOperation op)
{
  size_t n_pts[] = {
    [GSK_PATH_MOVE] = 1,
    [GSK_PATH_CLOSE] = 0,
    [GSK_PATH_LINE] = 2,
    [GSK_PATH_QUAD] = 3,
    [GSK_PATH_CUBIC] = 4,
    [GSK_PATH_CONIC] = 3
  };

  return n_pts[op];
}

static gboolean
path_data_add_op (GskPathOperation        op,
                  const graphene_point_t *pts,
                  size_t                  n_pts,
                  float                   weight,
                  gpointer                user_data)
{
  SvgPathOps *ops = user_data;
  size_t size;
  SvgPathOp *pop;

  if (op == GSK_PATH_CONIC)
    return FALSE;

  size = svg_path_ops_get_size (ops);
  svg_path_ops_set_size (ops, size + 1);
  pop = svg_path_ops_index (ops, size);

  pop->op = op;
  memset (pop->seg.pts, 0, sizeof (graphene_point_t) * 4);
  memcpy (pop->seg.pts, pts, sizeof (graphene_point_t) * n_pts);

  return TRUE;
}

static gboolean
path_data_add_arc (float    rx,
                   float    ry,
                   float    x_axis_rotation,
                   gboolean large_arc,
                   gboolean positive_sweep,
                   float    x,
                   float    y,
                   gpointer user_data)
{
  SvgPathOps *ops = user_data;
  size_t size;
  SvgPathOp *pop;

  size = svg_path_ops_get_size (ops);
  svg_path_ops_set_size (ops, size + 1);
  pop = svg_path_ops_index (ops, size);

  pop->op = SVG_PATH_ARC;
  pop->arc.rx = rx;
  pop->arc.ry = ry;
  pop->arc.x_axis_rotation = x_axis_rotation;
  pop->arc.large_arc = large_arc;
  pop->arc.positive_sweep = positive_sweep;
  pop->arc.x = x;
  pop->arc.y = y;

  return TRUE;
}

gboolean
svg_path_data_parse_full (const char   *string,
                          SvgPathData **path_data)
{
  GskPathParser parser = {
    path_data_add_op,
    path_data_add_arc,
    NULL, NULL, NULL,
  };
  SvgPathOps ops;
  size_t size;
  SvgPathOp *data;
  gboolean ret;

  svg_path_ops_init (&ops);

  ret = gsk_path_parse_full (string, &parser, &ops);

  size = svg_path_ops_get_size (&ops);
  data = svg_path_ops_steal (&ops);
  *path_data = svg_path_data_new (size, data);

  return ret;
}

SvgPathData *
svg_path_data_parse (const char *string)
{
  SvgPathData *data = NULL;
  svg_path_data_parse_full (string, &data);
  return data;
}

SvgPathData *
svg_path_data_from_gsk (GskPath *path)
{
  SvgPathOps ops;
  size_t size;
  SvgPathOp *data;

  svg_path_ops_init (&ops);

  gsk_path_foreach (path,
                    GSK_PATH_FOREACH_ALLOW_QUAD |
                    GSK_PATH_FOREACH_ALLOW_CUBIC,
                    path_data_add_op,
                    &ops);

  size = svg_path_ops_get_size (&ops);
  data = svg_path_ops_steal (&ops);

  return svg_path_data_new (size, data);
}

void
svg_path_data_print (SvgPathData *p,
                     GString     *s)
{
  for (unsigned int i = 0; i < p->n_ops; i++)
    {
      SvgPathOp *op = &p->ops[i];

      if (i > 0)
        g_string_append_c (s, ' ');

      switch (op->op)
        {
        case GSK_PATH_MOVE:
          string_append_point (s, "M ", &op->seg.pts[0]);
          break;
        case GSK_PATH_CLOSE:
          g_string_append (s, "Z");
          break;
        case GSK_PATH_LINE:
          string_append_point (s, "L ", &op->seg.pts[1]);
          break;
        case GSK_PATH_QUAD:
          string_append_point (s, "Q ", &op->seg.pts[1]);
          string_append_point (s, ", ", &op->seg.pts[2]);
          break;
        case GSK_PATH_CUBIC:
          string_append_point (s, "C ", &op->seg.pts[1]);
          string_append_point (s, ", ", &op->seg.pts[2]);
          string_append_point (s, ", ", &op->seg.pts[3]);
          break;
        case SVG_PATH_ARC:
          string_append_double (s, "A ", op->arc.rx);
          string_append_double (s, " ", op->arc.ry);
          string_append_double (s, " ", op->arc.x_axis_rotation);
          g_string_append_printf (s, " %d %d", op->arc.large_arc, op->arc.positive_sweep);
          string_append_double (s, " ", op->arc.x);
          string_append_double (s, " ", op->arc.y);
          break;
        default:
          g_assert_not_reached ();
        }
    }
}

SvgPathData *
svg_path_data_interpolate (SvgPathData *p0,
                           SvgPathData *p1,
                           double       t)
{
  SvgPathOp *ops;

  if (p0->n_ops != p1->n_ops)
    return NULL;

  ops = g_new0 (SvgPathOp, p0->n_ops);

  for (unsigned int i = 0; i < p0->n_ops; i++)
    {
      SvgPathOp *op0 = &p0->ops[i];
      SvgPathOp *op1 = &p1->ops[i];
      SvgPathOp *op = &ops[i];

      if (op0->op != op1->op)
        {
          g_free (ops);
          return NULL;
        }

      op->op = op0->op;

      switch (op->op)
        {
        case GSK_PATH_CUBIC:
          lerp_point (t, &op0->seg.pts[3], &op1->seg.pts[3], &op->seg.pts[3]);
          G_GNUC_FALLTHROUGH;
        case GSK_PATH_QUAD:
          lerp_point (t, &op0->seg.pts[2], &op1->seg.pts[2], &op->seg.pts[2]);
          G_GNUC_FALLTHROUGH;
        case GSK_PATH_LINE:
        case GSK_PATH_MOVE:
          lerp_point (t, &op0->seg.pts[1], &op1->seg.pts[1], &op->seg.pts[1]);
          G_GNUC_FALLTHROUGH;
        case GSK_PATH_CLOSE:
          lerp_point (t, &op0->seg.pts[0], &op1->seg.pts[0], &op->seg.pts[0]);
          break;
        case SVG_PATH_ARC:
          op->arc.rx = lerp (t, op0->arc.rx, op1->arc.rx);
          op->arc.ry = lerp (t, op0->arc.ry, op1->arc.ry);
          op->arc.x_axis_rotation = lerp (t, op0->arc.x_axis_rotation, op1->arc.x_axis_rotation);
          op->arc.large_arc = lerp_bool (t, op0->arc.large_arc, op1->arc.large_arc);
          op->arc.positive_sweep = lerp_bool (t, op0->arc.positive_sweep, op1->arc.positive_sweep);
          op->arc.x = lerp (t, op0->arc.x, op1->arc.x);
          op->arc.y = lerp (t, op0->arc.y, op1->arc.y);

          break;
        default:
          g_assert_not_reached ();
        }
    }

  return svg_path_data_new (p0->n_ops, ops);
}

GskPath *
svg_path_data_to_gsk (SvgPathData *p)
{
  GskPathBuilder *builder;

  builder = gsk_path_builder_new ();

  for (unsigned int i = 0; i < p->n_ops; i++)
    {
      SvgPathOp *op = &p->ops[i];

      if (op->op == SVG_PATH_ARC)
        gsk_path_builder_svg_arc_to (builder,
                                     op->arc.rx, op->arc.ry,
                                     op->arc.x_axis_rotation,
                                     op->arc.large_arc,
                                     op->arc.positive_sweep,
                                     op->arc.x, op->arc.y);
      else
        gsk_path_builder_add_op (builder, op->op, op->seg.pts, path_data_points_per_op (op->op), 1);
    }

  return gsk_path_builder_free_to_path (builder);
}
