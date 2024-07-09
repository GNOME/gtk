/*
 * Copyright © 2019 Benjamin Otte
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

#include "gsktransform.h"

#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcssparserprivate.h"

/* declares GdkDihedralTransform */
#include "gdk/gdksubsurfaceprivate.h"

G_BEGIN_DECLS

typedef struct _GskTransformClass GskTransformClass;

/**
 * GskFineTransformCategory:
 * @GSK_FINE_TRANSFORM_CATEGORY_UNKNOWN: The category of the matrix has not been
 *   determined.
 * @GSK_FINE_TRANSFORM_CATEGORY_ANY: Analyzing the matrix concluded that it does
 *   not fit in any other category.
 * @GSK_FINE_TRANSFORM_CATEGORY_3D: The matrix is a 3D matrix. This means that
 *   the w column (the last column) has the values (0, 0, 0, 1).
 * @GSK_FINE_TRANSFORM_CATEGORY_2D: The matrix is a 2D matrix. This is equivalent
 *   to graphene_matrix_is_2d() returning %TRUE. In particular, this
 *   means that Cairo can deal with the matrix.
 * @GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL: The matrix is a combination of 2D scale,
 *   2D translation, and 90 degree rotation operations. In particular, this means
 *   that any rectangle can be transformed exactly using this matrix.
 * @GSK_FINE_TRANSFORM_CATEGORY_2D_NEGATIVE_AFFINE: The matrix is a combination of
 *   (positive or negative) 2D scale and 2D translation operations. This category
 *   only exists to ease mapping to GskTransform.
 * @GSK_FINE_TRANSFORM_CATEGORY_2D_AFFINE: The matrix is a combination of positive
 *   2D scale and 2D translation operations. In particular, this means that any
 *   rectangle can be transformed exactly using this matrix without flipping.
 * @GSK_FINE_TRANSFORM_CATEGORY_2D_TRANSLATE: The matrix is a 2D translation.
 * @GSK_FINE_TRANSFORM_CATEGORY_IDENTITY: The matrix is the identity matrix.
 *
 * The categories of matrices relevant for GSK and GTK.
 *
 * Note that any category includes matrices of all later categories.
 * So if you want to for example check if a matrix is a 2D matrix,
 * `category >= GSK_TRANSFORM_CATEGORY_2D` is the way to do this.
 *
 * Also keep in mind that rounding errors may cause matrices to not
 * conform to their categories. Otherwise, matrix operations done via
 * multiplication will not worsen categories. So for the matrix
 * multiplication `C = A * B`, `category(C) = MIN (category(A), category(B))`.
 */
typedef enum
{
  GSK_FINE_TRANSFORM_CATEGORY_UNKNOWN,
  GSK_FINE_TRANSFORM_CATEGORY_ANY,
  GSK_FINE_TRANSFORM_CATEGORY_3D,
  GSK_FINE_TRANSFORM_CATEGORY_2D,
  GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL,
  GSK_FINE_TRANSFORM_CATEGORY_2D_NEGATIVE_AFFINE,
  GSK_FINE_TRANSFORM_CATEGORY_2D_AFFINE,
  GSK_FINE_TRANSFORM_CATEGORY_2D_TRANSLATE,
  GSK_FINE_TRANSFORM_CATEGORY_IDENTITY
} GskFineTransformCategory;

struct _GskTransform
{
  const GskTransformClass *transform_class;

  GskFineTransformCategory category;
  GskTransform *next;
};

gboolean                gsk_transform_parser_parse              (GtkCssParser           *parser,
                                                                 GskTransform          **out_transform);

void                    gsk_transform_to_dihedral               (GskTransform           *self,
                                                                 GdkDihedral            *out_dihedral,
                                                                 float                  *out_scale_x,
                                                                 float                  *out_scale_y,
                                                                 float                  *out_dx,
                                                                 float                  *out_dy);

void gsk_matrix_transform_point   (const graphene_matrix_t  *m,
                                   const graphene_point_t   *p,
                                   graphene_point_t         *res);
void gsk_matrix_transform_point3d (const graphene_matrix_t  *m,
                                   const graphene_point3d_t *p,
                                   graphene_point3d_t       *res);
void gsk_matrix_transform_bounds  (const graphene_matrix_t  *m,
                                   const graphene_rect_t    *r,
                                   graphene_rect_t          *res);
void gsk_matrix_transform_rect    (const graphene_matrix_t  *m,
                                   const graphene_rect_t    *r,
                                   graphene_quad_t          *res);

#define gsk_transform_get_fine_category(t) ((t) ? (t)->category : GSK_FINE_TRANSFORM_CATEGORY_IDENTITY)

G_END_DECLS


