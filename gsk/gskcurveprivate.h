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

#include "gskpathopprivate.h"
#include "gskpath.h"
#include "gskboundingboxprivate.h"

G_BEGIN_DECLS

typedef gpointer gskpathop;

typedef union _GskCurve GskCurve;

typedef struct _GskLineCurve GskLineCurve;
typedef struct _GskQuadCurve GskQuadCurve;
typedef struct _GskCubicCurve GskCubicCurve;

struct _GskLineCurve
{
  GskPathOperation op;

  gboolean padding;

  graphene_point_t points[2];
};

struct _GskQuadCurve
{
  GskPathOperation op;

  gboolean has_coefficients;

  graphene_point_t points[3];

  graphene_point_t coeffs[3];
};

struct _GskCubicCurve
{
  GskPathOperation op;

  gboolean has_coefficients;

  graphene_point_t points[4];

  graphene_point_t coeffs[4];
};

union _GskCurve
{
  GskPathOperation op;
  GskLineCurve line;
  GskQuadCurve quad;
  GskCubicCurve cubic;
};

typedef enum {
  GSK_CURVE_LINE_REASON_STRAIGHT,
  GSK_CURVE_LINE_REASON_SHORT
} GskCurveLineReason;

typedef gboolean (* GskCurveAddLineFunc) (const graphene_point_t *from,
                                          const graphene_point_t *to,
                                          float                   from_progress,
                                          float                   to_progress,
                                          GskCurveLineReason      reason,
                                          gpointer                user_data);

typedef gboolean (* GskCurveAddCurveFunc) (GskPathOperation        op,
                                           const graphene_point_t *pts,
                                           gsize                   n_pts,
                                           gpointer                user_data);

void                    gsk_curve_init                          (GskCurve               *curve,
                                                                 gskpathop               op);
void                    gsk_curve_init_foreach                  (GskCurve               *curve,
                                                                 GskPathOperation        op,
                                                                 const graphene_point_t *pts,
                                                                 gsize                   n_pts);

void                    gsk_curve_print                         (const GskCurve         *curve,
                                                                 GString                *string);

char *                  gsk_curve_to_string                     (const GskCurve         *curve);

gskpathop               gsk_curve_pathop                        (const GskCurve         *curve);
const graphene_point_t *gsk_curve_get_start_point               (const GskCurve         *curve);
const graphene_point_t *gsk_curve_get_end_point                 (const GskCurve         *curve);
void                    gsk_curve_get_start_tangent             (const GskCurve         *curve,
                                                                 graphene_vec2_t        *tangent);
void                    gsk_curve_get_end_tangent               (const GskCurve         *curve,
                                                                 graphene_vec2_t        *tangent);
void                    gsk_curve_get_point                     (const GskCurve         *curve,
                                                                 float                   progress,
                                                                 graphene_point_t       *pos);
void                    gsk_curve_get_tangent                   (const GskCurve         *curve,
                                                                 float                   progress,
                                                                 graphene_vec2_t        *tangent);
void                    gsk_curve_reverse                       (const GskCurve         *curve,
                                                                 GskCurve               *reverse);
void                    gsk_curve_split                         (const GskCurve         *curve,
                                                                 float                   progress,
                                                                 GskCurve               *start,
                                                                 GskCurve               *end);
void                    gsk_curve_segment                       (const GskCurve         *curve,
                                                                 float                   start,
                                                                 float                   end,
                                                                 GskCurve               *segment);
gboolean                gsk_curve_decompose                     (const GskCurve         *curve,
                                                                 float                   tolerance,
                                                                 GskCurveAddLineFunc     add_line_func,
                                                                 gpointer                user_data);
gboolean                gsk_curve_decompose_curve               (const GskCurve         *curve,
                                                                 GskPathForeachFlags     flags,
                                                                 float                   tolerance,
                                                                 GskCurveAddCurveFunc    add_curve_func,
                                                                 gpointer                user_data);

#define gsk_curve_builder_to(curve, builder) gsk_path_builder_pathop_to ((builder), gsk_curve_pathop (curve))

float                   gsk_curve_get_curvature                 (const GskCurve         *curve,
                                                                 float                   t,
                                                                 graphene_point_t       *center);
void                    gsk_curve_get_bounds                    (const GskCurve         *curve,
                                                                 GskBoundingBox         *bounds);
void                    gsk_curve_get_tight_bounds              (const GskCurve         *curve,
                                                                 GskBoundingBox         *bounds);

int                     gsk_curve_get_crossing                  (const GskCurve         *curve,
                                                                 const graphene_point_t *point);
gboolean                gsk_curve_get_closest_point             (const GskCurve         *curve,
                                                                 const graphene_point_t *point,
                                                                 float                   threshold,
                                                                 float                  *out_dist,
                                                                 float                  *out_t);

G_END_DECLS

