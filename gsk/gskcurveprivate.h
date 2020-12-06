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


#ifndef __GSK_CURVE_PRIVATE_H__
#define __GSK_CURVE_PRIVATE_H__

#include "gskpathopprivate.h"

G_BEGIN_DECLS

typedef gpointer gskpathop;

typedef union _GskCurve GskCurve;

typedef struct _GskLineCurve GskLineCurve;
typedef struct _GskCurveCurve GskCurveCurve;
typedef struct _GskConicCurve GskConicCurve;

struct _GskLineCurve
{
  GskPathOperation op;

  gboolean padding;

  graphene_point_t points[2];
};

struct _GskCurveCurve
{
  GskPathOperation op;

  gboolean has_coefficients;

  graphene_point_t points[4];

  graphene_point_t coeffs[4];
};

struct _GskConicCurve
{
  GskPathOperation op;

  gboolean has_coefficients;

  graphene_point_t points[4];

  graphene_point_t num[3];
  graphene_point_t denom[3];
};

union _GskCurve
{
  GskPathOperation op;
  GskLineCurve line;
  GskCurveCurve curve;
  GskConicCurve conic;
};

typedef gboolean (* GskCurveAddLineFunc) (const graphene_point_t *from,
                                          const graphene_point_t *to,
                                          float                   from_progress,
                                          float                   to_progress,
                                          gpointer                user_data);

void                    gsk_curve_init                          (GskCurve               *curve,
                                                                 gskpathop               op);

void                    gsk_curve_eval                          (const GskCurve         *curve,
                                                                 float                   progress,
                                                                 graphene_point_t       *pos,
                                                                 graphene_vec2_t        *tangent);
void                    gsk_curve_split                         (const GskCurve         *curve,
                                                                 float                   progress,
                                                                 GskCurve               *start,
                                                                 GskCurve               *end);
gboolean                gsk_curve_decompose                     (const GskCurve         *curve,
                                                                 float                   tolerance,
                                                                 GskCurveAddLineFunc     add_line_func,
                                                                 gpointer                user_data);
gskpathop               gsk_curve_pathop                        (const GskCurve         *curve);
#define gsk_curve_builder_to(curve, builder) gsk_path_builder_pathop_to ((builder), gsk_curve_pathop (curve))
const graphene_point_t *gsk_curve_get_start_point               (const GskCurve         *curve);
const graphene_point_t *gsk_curve_get_end_point                 (const GskCurve         *curve);




G_END_DECLS

#endif /* __GSK_CURVE_PRIVATE_H__ */

