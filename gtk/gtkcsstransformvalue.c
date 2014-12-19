/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcssbgsizevalueprivate.h"

#include <math.h>
#include <string.h>

#include "gtkcsstransformvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"

typedef union _GtkCssTransform GtkCssTransform;

typedef enum {
  GTK_CSS_TRANSFORM_NONE,
  GTK_CSS_TRANSFORM_MATRIX,
  GTK_CSS_TRANSFORM_TRANSLATE,
  GTK_CSS_TRANSFORM_ROTATE,
  GTK_CSS_TRANSFORM_SCALE,
  GTK_CSS_TRANSFORM_SKEW,
  GTK_CSS_TRANSFORM_SKEW_X,
  GTK_CSS_TRANSFORM_SKEW_Y
} GtkCssTransformType;

union _GtkCssTransform {
  GtkCssTransformType type;
  struct {
    GtkCssTransformType type;
    cairo_matrix_t      matrix;
  }                   matrix;
  struct {
    GtkCssTransformType type;
    GtkCssValue        *x;
    GtkCssValue        *y;
  }                   translate, scale, skew;
  struct {
    GtkCssTransformType type;
    GtkCssValue        *rotate;
  }                   rotate;
  struct {
    GtkCssTransformType type;
    GtkCssValue        *skew;
  }                   skew_x, skew_y;
};

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  guint             n_transforms;
  GtkCssTransform   transforms[1];
};

static GtkCssValue *    gtk_css_transform_value_alloc           (guint                  n_values);
static gboolean         gtk_css_transform_value_is_none         (const GtkCssValue     *value);

static void
gtk_css_transform_clear (GtkCssTransform *transform)
{
  switch (transform->type)
    {
    case GTK_CSS_TRANSFORM_MATRIX:
      break;
    case GTK_CSS_TRANSFORM_TRANSLATE:
      _gtk_css_value_unref (transform->translate.x);
      _gtk_css_value_unref (transform->translate.y);
      break;
    case GTK_CSS_TRANSFORM_ROTATE:
      _gtk_css_value_unref (transform->rotate.rotate);
      break;
    case GTK_CSS_TRANSFORM_SCALE:
      _gtk_css_value_unref (transform->scale.x);
      _gtk_css_value_unref (transform->scale.y);
      break;
    case GTK_CSS_TRANSFORM_SKEW:
      _gtk_css_value_unref (transform->skew.x);
      _gtk_css_value_unref (transform->skew.y);
      break;
    case GTK_CSS_TRANSFORM_SKEW_X:
      _gtk_css_value_unref (transform->skew_x.skew);
      break;
    case GTK_CSS_TRANSFORM_SKEW_Y:
      _gtk_css_value_unref (transform->skew_y.skew);
      break;
    case GTK_CSS_TRANSFORM_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
gtk_css_transform_init_identity (GtkCssTransform     *transform,
                                 GtkCssTransformType  type)
{
  switch (type)
    {
    case GTK_CSS_TRANSFORM_MATRIX:
      cairo_matrix_init_identity (&transform->matrix.matrix);
      break;
    case GTK_CSS_TRANSFORM_TRANSLATE:
      transform->translate.x = _gtk_css_number_value_new (0, GTK_CSS_PX);
      transform->translate.y = _gtk_css_number_value_new (0, GTK_CSS_PX);
      break;
    case GTK_CSS_TRANSFORM_ROTATE:
      transform->rotate.rotate = _gtk_css_number_value_new (0, GTK_CSS_DEG);
      break;
    case GTK_CSS_TRANSFORM_SCALE:
      transform->scale.x = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
      transform->scale.y = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
      break;
    case GTK_CSS_TRANSFORM_SKEW:
      transform->skew.x = _gtk_css_number_value_new (0, GTK_CSS_DEG);
      transform->skew.y = _gtk_css_number_value_new (0, GTK_CSS_DEG);
      break;
    case GTK_CSS_TRANSFORM_SKEW_X:
      transform->skew_x.skew = _gtk_css_number_value_new (0, GTK_CSS_DEG);
      break;
    case GTK_CSS_TRANSFORM_SKEW_Y:
      transform->skew_y.skew = _gtk_css_number_value_new (0, GTK_CSS_DEG);
      break;
    case GTK_CSS_TRANSFORM_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  transform->type = type;
}

static void
gtk_cairo_matrix_skew (cairo_matrix_t *matrix,
                       double          skew_x,
                       double          skew_y)
{
  cairo_matrix_t skew = { 1, tan (skew_x), tan (skew_y), 1, 0, 0 };

  cairo_matrix_multiply (matrix, &skew, matrix);
}

static void
gtk_css_transform_apply (const GtkCssTransform *transform,
                         cairo_matrix_t        *matrix)
{
  switch (transform->type)
    {
    case GTK_CSS_TRANSFORM_MATRIX:
      cairo_matrix_multiply (matrix, &transform->matrix.matrix, matrix);
      break;
    case GTK_CSS_TRANSFORM_TRANSLATE:
      cairo_matrix_translate (matrix,
                              _gtk_css_number_value_get (transform->translate.x, 100),
                              _gtk_css_number_value_get (transform->translate.y, 100));
      break;
    case GTK_CSS_TRANSFORM_ROTATE:
      cairo_matrix_rotate (matrix,
                           _gtk_css_number_value_get (transform->rotate.rotate, 100) * (2 * G_PI) / 360);
      break;
    case GTK_CSS_TRANSFORM_SCALE:
      cairo_matrix_scale (matrix,
                          _gtk_css_number_value_get (transform->scale.x, 1),
                          _gtk_css_number_value_get (transform->scale.y, 1));
      break;
    case GTK_CSS_TRANSFORM_SKEW:
      gtk_cairo_matrix_skew (matrix,
                             _gtk_css_number_value_get (transform->skew.x, 100),
                             _gtk_css_number_value_get (transform->skew.y, 100));
      break;
    case GTK_CSS_TRANSFORM_SKEW_X:
      gtk_cairo_matrix_skew (matrix,
                             _gtk_css_number_value_get (transform->skew_x.skew, 100),
                             0);
      break;
    case GTK_CSS_TRANSFORM_SKEW_Y:
      gtk_cairo_matrix_skew (matrix,
                             0,
                             _gtk_css_number_value_get (transform->skew_y.skew, 100));
      break;
    case GTK_CSS_TRANSFORM_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}

/* NB: The returned matrix may be invalid */
static void
gtk_css_transform_value_compute_matrix (const GtkCssValue *value,
                                        cairo_matrix_t    *matrix)
{
  guint i;

  cairo_matrix_init_identity (matrix);

  for (i = 0; i < value->n_transforms; i++)
    {
      gtk_css_transform_apply (&value->transforms[i], matrix);
    }
}

static void
gtk_css_value_transform_free (GtkCssValue *value)
{
  guint i;

  for (i = 0; i < value->n_transforms; i++)
    {
      gtk_css_transform_clear (&value->transforms[i]);
    }

  g_slice_free1 (sizeof (GtkCssValue) + sizeof (GtkCssTransform) * (value->n_transforms - 1), value);
}

/* returns TRUE if dest == src */
static gboolean
gtk_css_transform_compute (GtkCssTransform         *dest,
                           GtkCssTransform         *src,
                           guint                    property_id,
                           GtkStyleProviderPrivate *provider,
                           int                      scale,
                           GtkCssComputedValues    *values,
                           GtkCssComputedValues    *parent_values,
                           GtkCssDependencies      *dependencies)
{
  GtkCssDependencies x_deps, y_deps;

  dest->type = src->type;

  switch (src->type)
    {
    case GTK_CSS_TRANSFORM_MATRIX:
      return TRUE;
    case GTK_CSS_TRANSFORM_TRANSLATE:
      x_deps = y_deps = 0;
      dest->translate.x = _gtk_css_value_compute (src->translate.x, property_id, provider, scale, values, parent_values, &x_deps);
      dest->translate.y = _gtk_css_value_compute (src->translate.y, property_id, provider, scale, values, parent_values, &y_deps);
      *dependencies = _gtk_css_dependencies_union (x_deps, y_deps);
      return dest->translate.x == src->translate.x
          && dest->translate.y == src->translate.y;
    case GTK_CSS_TRANSFORM_ROTATE:
      dest->rotate.rotate = _gtk_css_value_compute (src->rotate.rotate, property_id, provider, scale, values, parent_values, dependencies);
      return dest->rotate.rotate == src->rotate.rotate;
    case GTK_CSS_TRANSFORM_SCALE:
      x_deps = y_deps = 0;
      dest->scale.x = _gtk_css_value_compute (src->scale.x, property_id, provider, scale, values, parent_values, &x_deps);
      dest->scale.y = _gtk_css_value_compute (src->scale.y, property_id, provider, scale, values, parent_values, &y_deps);
      *dependencies = _gtk_css_dependencies_union (x_deps, y_deps);
      return dest->scale.x == src->scale.x
          && dest->scale.y == src->scale.y;
    case GTK_CSS_TRANSFORM_SKEW:
      x_deps = y_deps = 0;
      dest->skew.x = _gtk_css_value_compute (src->skew.x, property_id, provider, scale, values, parent_values, &x_deps);
      dest->skew.y = _gtk_css_value_compute (src->skew.y, property_id, provider, scale, values, parent_values, &y_deps);
      *dependencies = _gtk_css_dependencies_union (x_deps, y_deps);
      return dest->skew.x == src->skew.x
          && dest->skew.y == src->skew.y;
    case GTK_CSS_TRANSFORM_SKEW_X:
      dest->skew_x.skew = _gtk_css_value_compute (src->skew_x.skew, property_id, provider, scale, values, parent_values, dependencies);
      return dest->skew_x.skew == src->skew_x.skew;
    case GTK_CSS_TRANSFORM_SKEW_Y:
      dest->skew_y.skew = _gtk_css_value_compute (src->skew_y.skew, property_id, provider, scale, values, parent_values, dependencies);
      return dest->skew_y.skew == src->skew_y.skew;
    case GTK_CSS_TRANSFORM_NONE:
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static GtkCssValue *
gtk_css_value_transform_compute (GtkCssValue             *value,
                                 guint                    property_id,
                                 GtkStyleProviderPrivate *provider,
			         int                      scale,
                                 GtkCssComputedValues    *values,
                                 GtkCssComputedValues    *parent_values,
                                 GtkCssDependencies      *dependencies)
{
  GtkCssDependencies transform_deps;
  GtkCssValue *result;
  gboolean changes;
  guint i;

  /* Special case the 99% case of "none" */
  if (gtk_css_transform_value_is_none (value))
    return _gtk_css_value_ref (value);

  changes = FALSE;
  result = gtk_css_transform_value_alloc (value->n_transforms);

  for (i = 0; i < value->n_transforms; i++)
    {
      changes |= !gtk_css_transform_compute (&result->transforms[i],
                                             &value->transforms[i],
                                             property_id,
                                             provider,
                                             scale,
                                             values,
                                             parent_values,
                                             &transform_deps);
      *dependencies = _gtk_css_dependencies_union (*dependencies, transform_deps);
    }

  if (!changes)
    {
      _gtk_css_value_unref (result);
      result = _gtk_css_value_ref (value);
    }

  return result;
}

static gboolean
gtk_css_transform_equal (const GtkCssTransform *transform1,
                         const GtkCssTransform *transform2)
{
  if (transform1->type != transform2->type)
    return FALSE;

  switch (transform1->type)
    {
    case GTK_CSS_TRANSFORM_MATRIX:
      return transform1->matrix.matrix.xx == transform2->matrix.matrix.xx
          && transform1->matrix.matrix.xy == transform2->matrix.matrix.xy
          && transform1->matrix.matrix.yx == transform2->matrix.matrix.yx
          && transform1->matrix.matrix.yy == transform2->matrix.matrix.yy
          && transform1->matrix.matrix.x0 == transform2->matrix.matrix.x0
          && transform1->matrix.matrix.y0 == transform2->matrix.matrix.y0;
    case GTK_CSS_TRANSFORM_TRANSLATE:
      return _gtk_css_value_equal (transform1->translate.x, transform2->translate.x)
          && _gtk_css_value_equal (transform1->translate.y, transform2->translate.y);
    case GTK_CSS_TRANSFORM_ROTATE:
      return _gtk_css_value_equal (transform1->rotate.rotate, transform2->rotate.rotate);
    case GTK_CSS_TRANSFORM_SCALE:
      return _gtk_css_value_equal (transform1->scale.x, transform2->scale.x)
          && _gtk_css_value_equal (transform1->scale.y, transform2->scale.y);
    case GTK_CSS_TRANSFORM_SKEW:
      return _gtk_css_value_equal (transform1->skew.x, transform2->skew.x)
          && _gtk_css_value_equal (transform1->skew.y, transform2->skew.y);
    case GTK_CSS_TRANSFORM_SKEW_X:
      return _gtk_css_value_equal (transform1->skew_x.skew, transform2->skew_x.skew);
    case GTK_CSS_TRANSFORM_SKEW_Y:
      return _gtk_css_value_equal (transform1->skew_y.skew, transform2->skew_y.skew);
    case GTK_CSS_TRANSFORM_NONE:
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static gboolean
gtk_css_value_transform_equal (const GtkCssValue *value1,
                               const GtkCssValue *value2)
{
  const GtkCssValue *larger;
  guint i, n;

  n = MIN (value1->n_transforms, value2->n_transforms);
  for (i = 0; i < n; i++)
    {
      if (!gtk_css_transform_equal (&value1->transforms[i], &value2->transforms[i]))
        return FALSE;
    }

  larger = value1->n_transforms > value2->n_transforms ? value1 : value2;

  for (; i < larger->n_transforms; i++)
    {
      GtkCssTransform transform;

      gtk_css_transform_init_identity (&transform, larger->transforms[i].type);

      if (!gtk_css_transform_equal (&larger->transforms[i], &transform))
        {
          gtk_css_transform_clear (&transform);
          return FALSE;
        }

      gtk_css_transform_clear (&transform);
    }

  return TRUE;
}

typedef struct _DecomposedMatrix DecomposedMatrix;
struct _DecomposedMatrix {
  double translate[2];
  double scale[2];
  double angle;
  double m11;
  double m12;
  double m21;
  double m22;
};

static void
decomposed_init (DecomposedMatrix     *decomposed,
                 const cairo_matrix_t *matrix)
{
  double row0x = matrix->xx;
  double row0y = matrix->xy;
  double row1x = matrix->yx;
  double row1y = matrix->yy;
  double determinant;

  decomposed->translate[0] = matrix->x0;
  decomposed->translate[1] = matrix->y0;

  decomposed->scale[0] = sqrt (row0x * row0x + row0y * row0y);
  decomposed->scale[1] = sqrt (row1x * row1x + row1y * row1y);

  /* If determinant is negative, one axis was flipped. */
  determinant = row0x * row1y - row0y * row1x;
  
  if (determinant < 0)
    {
      /* Flip axis with minimum unit vector dot product. */
      if (row0x < row1y)
        decomposed->scale[0] = - decomposed->scale[0];
      else
        decomposed->scale[1] = - decomposed->scale[1];
    }
    
  /* Renormalize matrix to remove scale. */
  if (decomposed->scale[0])
    {
      row0x /= decomposed->scale[0];
      row0y /= decomposed->scale[0];
    }
  if (decomposed->scale[1])
    {
      row1x /= decomposed->scale[1];
      row1y /= decomposed->scale[1];
    }

  /* Compute rotation and renormalize matrix. */
  decomposed->angle = atan2(row0y, row0x);

  if (decomposed->angle)
    {
      /* Rotate(-angle) = [cos(angle), sin(angle), -sin(angle), cos(angle)]
       *                = [row0x, -row0y, row0y, row0x]
       * Thanks to the normalization above.
       */
      decomposed->m11 = row0x;
      decomposed->m12 = row0y;
      decomposed->m21 = row1x;
      decomposed->m22 = row1y;
    }
  else
    {
      decomposed->m11 = row0x * row0x - row0y * row1x;
      decomposed->m12 = row0x * row0y - row0y * row1y;
      decomposed->m21 = row0y * row0x + row0x * row1x;
      decomposed->m22 = row0y * row0y + row0x * row1y;
    }
  
  /* Convert into degrees because our rotation functions expect it. */
  decomposed->angle = decomposed->angle * 360 / (2 * G_PI);
}

static void
decomposed_interpolate (DecomposedMatrix       *result,
                        const DecomposedMatrix *start,
                        const DecomposedMatrix *end,
                        double                  progress)
{
  double start_angle, end_angle;

  result->translate[0] = start->translate[0] + (end->translate[0] - start->translate[0]) * progress;
  result->translate[1] = start->translate[1] + (end->translate[1] - start->translate[1]) * progress;
  result->m11 = start->m11 + (end->m11 - start->m11) * progress;
  result->m12 = start->m12 + (end->m12 - start->m12) * progress;
  result->m21 = start->m21 + (end->m21 - start->m21) * progress;
  result->m22 = start->m22 + (end->m22 - start->m22) * progress;

  /* If x-axis of one is flipped, and y-axis of the other,
   * convert to an unflipped rotation.
   */
  if ((start->scale[0] < 0 && end->scale[1] < 0) || (start->scale[1] < 0 && end->scale[0] < 0))
    {
      result->scale[0] = - start->scale[0];
      result->scale[1] = - start->scale[1];
      start_angle = start->angle < 0 ? start->angle + 180 : start->angle - 180;
      end_angle = end->angle;
    }
  else
    {
      result->scale[0] = start->scale[0];
      result->scale[1] = start->scale[1];
      start_angle = start->angle;
      end_angle = end->angle;
    }

  result->scale[0] = result->scale[0] + (end->scale[0] - result->scale[0]) * progress;
  result->scale[1] = result->scale[1] + (end->scale[1] - result->scale[1]) * progress;

  /* Don’t rotate the long way around. */
  if (start_angle == 0)
    start_angle = 360;
  if (end_angle == 0)
    end_angle = 360;

  if (ABS (start_angle - end_angle) > 180)
    {
      if (start_angle > end_angle)
        start_angle -= 360;
      else
        end_angle -= 360;
    }

  result->angle = start_angle + (end_angle - start_angle) * progress;
}

static void
decomposed_apply (const DecomposedMatrix *decomposed,
                  cairo_matrix_t         *matrix)
{
  matrix->xx = decomposed->m11;
  matrix->xy = decomposed->m12;
  matrix->yx = decomposed->m21;
  matrix->yy = decomposed->m22;
  matrix->x0 = 0;
  matrix->y0 = 0;

  /* Translate matrix. */
  cairo_matrix_translate (matrix, decomposed->translate[0], decomposed->translate[1]);
  
  /* Rotate matrix. */
  cairo_matrix_rotate (matrix, decomposed->angle * (2 * G_PI) / 360);
  
  /* Scale matrix. */
  cairo_matrix_scale (matrix, decomposed->scale[0], decomposed->scale[1]);
}

static void
gtk_css_transform_matrix_transition (cairo_matrix_t       *result,
                                     const cairo_matrix_t *start,
                                     const cairo_matrix_t *end,
                                     double                progress)
{
  DecomposedMatrix dresult, dstart, dend;

  decomposed_init (&dstart, start);
  decomposed_init (&dend, end);
  decomposed_interpolate (&dresult, &dstart, &dend, progress);
  decomposed_apply (&dresult, result);
}

static void
gtk_css_transform_transition (GtkCssTransform       *result,
                              const GtkCssTransform *start,
                              const GtkCssTransform *end,
                              guint                  property_id,
                              double                 progress)
{
  result->type = start->type;

  switch (start->type)
    {
    case GTK_CSS_TRANSFORM_MATRIX:
      gtk_css_transform_matrix_transition (&result->matrix.matrix,
                                           &start->matrix.matrix,
                                           &end->matrix.matrix,
                                           progress);
      break;
    case GTK_CSS_TRANSFORM_TRANSLATE:
      result->translate.x = _gtk_css_value_transition (start->translate.x, end->translate.x, property_id, progress);
      result->translate.y = _gtk_css_value_transition (start->translate.y, end->translate.y, property_id, progress);
      break;
    case GTK_CSS_TRANSFORM_ROTATE:
      result->rotate.rotate = _gtk_css_value_transition (start->rotate.rotate, end->rotate.rotate, property_id, progress);
      break;
    case GTK_CSS_TRANSFORM_SCALE:
      result->scale.x = _gtk_css_value_transition (start->scale.x, end->scale.x, property_id, progress);
      result->scale.y = _gtk_css_value_transition (start->scale.y, end->scale.y, property_id, progress);
      break;
    case GTK_CSS_TRANSFORM_SKEW:
      result->skew.x = _gtk_css_value_transition (start->skew.x, end->skew.x, property_id, progress);
      result->skew.y = _gtk_css_value_transition (start->skew.y, end->skew.y, property_id, progress);
      break;
    case GTK_CSS_TRANSFORM_SKEW_X:
      result->skew_x.skew = _gtk_css_value_transition (start->skew_x.skew, end->skew_x.skew, property_id, progress);
      break;
    case GTK_CSS_TRANSFORM_SKEW_Y:
      result->skew_y.skew = _gtk_css_value_transition (start->skew_y.skew, end->skew_y.skew, property_id, progress);
      break;
    case GTK_CSS_TRANSFORM_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}

static GtkCssValue *
gtk_css_value_transform_transition (GtkCssValue *start,
                                    GtkCssValue *end,
                                    guint        property_id,
                                    double       progress)
{
  GtkCssValue *result;
  guint i, n;

  if (gtk_css_transform_value_is_none (start))
    {
      if (gtk_css_transform_value_is_none (end))
        return _gtk_css_value_ref (start);

      n = 0;
    }
  else if (gtk_css_transform_value_is_none (end))
    {
      n = 0;
    }
  else
    {
      n = MIN (start->n_transforms, end->n_transforms);
    }

  /* Check transforms are compatible. If not, transition between
   * their result matrices.
   */
  for (i = 0; i < n; i++)
    {
      if (start->transforms[i].type != end->transforms[i].type)
        {
          cairo_matrix_t start_matrix, end_matrix;

          cairo_matrix_init_identity (&start_matrix);
          gtk_css_transform_value_compute_matrix (start, &start_matrix);
          cairo_matrix_init_identity (&end_matrix);
          gtk_css_transform_value_compute_matrix (end, &end_matrix);

          result = gtk_css_transform_value_alloc (1);
          result->transforms[0].type = GTK_CSS_TRANSFORM_MATRIX;
          gtk_css_transform_matrix_transition (&result->transforms[0].matrix.matrix, &start_matrix, &end_matrix, progress);

          return result;
        }
    }

  result = gtk_css_transform_value_alloc (MAX (start->n_transforms, end->n_transforms));

  for (i = 0; i < n; i++)
    {
      gtk_css_transform_transition (&result->transforms[i],
                                    &start->transforms[i],
                                    &end->transforms[i],
                                    property_id,
                                    progress);
    }

  for (; i < start->n_transforms; i++)
    {
      GtkCssTransform transform;

      gtk_css_transform_init_identity (&transform, start->transforms[i].type);
      gtk_css_transform_transition (&result->transforms[i],
                                    &start->transforms[i],
                                    &transform,
                                    property_id,
                                    progress);
      gtk_css_transform_clear (&transform);
    }
  for (; i < end->n_transforms; i++)
    {
      GtkCssTransform transform;

      gtk_css_transform_init_identity (&transform, end->transforms[i].type);
      gtk_css_transform_transition (&result->transforms[i],
                                    &transform,
                                    &end->transforms[i],
                                    property_id,
                                    progress);
      gtk_css_transform_clear (&transform);
    }

  g_assert (i == MAX (start->n_transforms, end->n_transforms));

  return result;
}

static void
gtk_css_transform_print (const GtkCssTransform *transform,
                         GString               *string)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  switch (transform->type)
    {
    case GTK_CSS_TRANSFORM_MATRIX:
      g_string_append (string, "matrix(");
      g_ascii_dtostr (buf, sizeof (buf), transform->matrix.matrix.xx);
      g_string_append (string, buf);
      g_string_append (string, ", ");
      g_ascii_dtostr (buf, sizeof (buf), transform->matrix.matrix.xy);
      g_string_append (string, buf);
      g_string_append (string, ", ");
      g_ascii_dtostr (buf, sizeof (buf), transform->matrix.matrix.x0);
      g_string_append (string, buf);
      g_string_append (string, ", ");
      g_ascii_dtostr (buf, sizeof (buf), transform->matrix.matrix.yx);
      g_string_append (string, buf);
      g_string_append (string, ", ");
      g_ascii_dtostr (buf, sizeof (buf), transform->matrix.matrix.yy);
      g_string_append (string, buf);
      g_string_append (string, ", ");
      g_ascii_dtostr (buf, sizeof (buf), transform->matrix.matrix.y0);
      g_string_append (string, buf);
      g_string_append (string, ")");
      break;
    case GTK_CSS_TRANSFORM_TRANSLATE:
      g_string_append (string, "translate(");
      _gtk_css_value_print (transform->translate.x, string);
      g_string_append (string, ", ");
      _gtk_css_value_print (transform->translate.y, string);
      g_string_append (string, ")");
      break;
    case GTK_CSS_TRANSFORM_ROTATE:
      g_string_append (string, "rotate(");
      _gtk_css_value_print (transform->rotate.rotate, string);
      g_string_append (string, ")");
      break;
    case GTK_CSS_TRANSFORM_SCALE:
      g_string_append (string, "scale(");
      _gtk_css_value_print (transform->scale.x, string);
      if (!_gtk_css_value_equal (transform->scale.x, transform->scale.y))
        {
          g_string_append (string, ", ");
          _gtk_css_value_print (transform->scale.y, string);
        }
      g_string_append (string, ")");
      break;
    case GTK_CSS_TRANSFORM_SKEW:
      g_string_append (string, "skew(");
      _gtk_css_value_print (transform->skew.x, string);
      g_string_append (string, ", ");
      _gtk_css_value_print (transform->skew.y, string);
      g_string_append (string, ")");
      break;
    case GTK_CSS_TRANSFORM_SKEW_X:
      g_string_append (string, "skewX(");
      _gtk_css_value_print (transform->skew_x.skew, string);
      g_string_append (string, ")");
      break;
    case GTK_CSS_TRANSFORM_SKEW_Y:
      g_string_append (string, "skewY(");
      _gtk_css_value_print (transform->skew_y.skew, string);
      g_string_append (string, ")");
      break;
    case GTK_CSS_TRANSFORM_NONE:
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
gtk_css_value_transform_print (const GtkCssValue *value,
                               GString           *string)
{
  guint i;

  if (gtk_css_transform_value_is_none (value))
    {
      g_string_append (string, "none");
      return;
    }

  for (i = 0; i < value->n_transforms; i++)
    {
      if (i > 0)
        g_string_append_c (string, ' ');

      gtk_css_transform_print (&value->transforms[i], string);
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_TRANSFORM = {
  gtk_css_value_transform_free,
  gtk_css_value_transform_compute,
  gtk_css_value_transform_equal,
  gtk_css_value_transform_transition,
  gtk_css_value_transform_print
};

static GtkCssValue none_singleton = { &GTK_CSS_VALUE_TRANSFORM, 1, 0, {  { GTK_CSS_TRANSFORM_NONE } } };

static GtkCssValue *
gtk_css_transform_value_alloc (guint n_transforms)
{
  GtkCssValue *result;
           
  g_return_val_if_fail (n_transforms > 0, NULL);
         
  result = _gtk_css_value_alloc (&GTK_CSS_VALUE_TRANSFORM, sizeof (GtkCssValue) + sizeof (GtkCssTransform) * (n_transforms - 1));
  result->n_transforms = n_transforms;
            
  return result;
}

GtkCssValue *
_gtk_css_transform_value_new_none (void)
{
  return _gtk_css_value_ref (&none_singleton);
}

static gboolean
gtk_css_transform_value_is_none (const GtkCssValue *value)
{
  return value->n_transforms == 0;
}

static gboolean
gtk_css_transform_parse (GtkCssTransform *transform,
                         GtkCssParser    *parser)
{
  if (_gtk_css_parser_try (parser, "matrix(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_MATRIX;

      /* FIXME: Improve error handling here */
      if (!_gtk_css_parser_try_double (parser, &transform->matrix.matrix.xx)
          || !_gtk_css_parser_try (parser, ",", TRUE)
          || !_gtk_css_parser_try_double (parser, &transform->matrix.matrix.xy)
          || !_gtk_css_parser_try (parser, ",", TRUE)
          || !_gtk_css_parser_try_double (parser, &transform->matrix.matrix.x0)
          || !_gtk_css_parser_try (parser, ",", TRUE)
          || !_gtk_css_parser_try_double (parser, &transform->matrix.matrix.yx)
          || !_gtk_css_parser_try (parser, ",", TRUE)
          || !_gtk_css_parser_try_double (parser, &transform->matrix.matrix.yy)
          || !_gtk_css_parser_try (parser, ",", TRUE)
          || !_gtk_css_parser_try_double (parser, &transform->matrix.matrix.y0))
        {
          _gtk_css_parser_error (parser, "invalid syntax for matrix()");
          return FALSE;
        }
    }
  else if (_gtk_css_parser_try (parser, "translate(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_TRANSLATE;

      transform->translate.x = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_LENGTH);
      if (transform->translate.x == NULL)
        return FALSE;

      if (_gtk_css_parser_try (parser, ",", TRUE))
        {
          transform->translate.y = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_LENGTH);
          if (transform->translate.y == NULL)
            {
              _gtk_css_value_unref (transform->translate.x);
              return FALSE;
            }
        }
      else
        {
          transform->translate.y = _gtk_css_number_value_new (0, GTK_CSS_PX);
        }
    }
  else if (_gtk_css_parser_try (parser, "translateX(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_TRANSLATE;

      transform->translate.x = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_LENGTH);
      if (transform->translate.x == NULL)
        return FALSE;
      
      transform->translate.y = _gtk_css_number_value_new (0, GTK_CSS_PX);
    }
  else if (_gtk_css_parser_try (parser, "translateY(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_TRANSLATE;

      transform->translate.y = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_LENGTH);
      if (transform->translate.y == NULL)
        return FALSE;
      
      transform->translate.x = _gtk_css_number_value_new (0, GTK_CSS_PX);
    }
  else if (_gtk_css_parser_try (parser, "scale(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_SCALE;

      transform->scale.x = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
      if (transform->scale.x == NULL)
        return FALSE;

      if (_gtk_css_parser_try (parser, ",", TRUE))
        {
          transform->scale.y = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
          if (transform->scale.y == NULL)
            {
              _gtk_css_value_unref (transform->scale.x);
              return FALSE;
            }
        }
      else
        {
          transform->scale.y = _gtk_css_value_ref (transform->scale.x);
        }
    }
  else if (_gtk_css_parser_try (parser, "scaleX(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_SCALE;

      transform->scale.x = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
      if (transform->scale.x == NULL)
        return FALSE;
      
      transform->scale.y = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
    }
  else if (_gtk_css_parser_try (parser, "scaleY(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_SCALE;

      transform->scale.y = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
      if (transform->scale.y == NULL)
        return FALSE;
      
      transform->scale.x = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
    }
  else if (_gtk_css_parser_try (parser, "rotate(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_ROTATE;

      transform->rotate.rotate = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_ANGLE);
      if (transform->rotate.rotate == NULL)
        return FALSE;
    }
  else if (_gtk_css_parser_try (parser, "skew(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_SKEW;

      transform->skew.x = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_ANGLE);
      if (transform->skew.x == NULL)
        return FALSE;

      if (_gtk_css_parser_try (parser, ",", TRUE))
        {
          transform->skew.y = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_ANGLE);
          if (transform->skew.y == NULL)
            {
              _gtk_css_value_unref (transform->skew.x);
              return FALSE;
            }
        }
      else
        {
          transform->skew.y = _gtk_css_number_value_new (0, GTK_CSS_DEG);
        }
    }
  else if (_gtk_css_parser_try (parser, "skewX(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_SKEW_X;

      transform->skew_x.skew = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_ANGLE);
      if (transform->skew_x.skew == NULL)
        return FALSE;
    }
  else if (_gtk_css_parser_try (parser, "skewY(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_SKEW_Y;

      transform->skew_y.skew = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_ANGLE);
      if (transform->skew_y.skew == NULL)
        return FALSE;
    }
  else
    {
      _gtk_css_parser_error (parser, "unknown syntax for transform");
      return FALSE;
    }

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      gtk_css_transform_clear (transform);
      _gtk_css_parser_error (parser, "Expected closing ')'");
      return FALSE;
    }

  return TRUE;
}

GtkCssValue *
_gtk_css_transform_value_parse (GtkCssParser *parser)
{
  GtkCssValue *value;
  GArray *array;
  guint i;

  if (_gtk_css_parser_try (parser, "none", TRUE))
    return _gtk_css_transform_value_new_none ();

  array = g_array_new (FALSE, FALSE, sizeof (GtkCssTransform));

  do {
    GtkCssTransform transform;

    if (!gtk_css_transform_parse (&transform, parser))
      {
        for (i = 0; i < array->len; i++)
          {
            gtk_css_transform_clear (&g_array_index (array, GtkCssTransform, i));
          }
        g_array_free (array, TRUE);
        return NULL;
      }
    g_array_append_val (array, transform);
  } while (!_gtk_css_parser_begins_with (parser, ';'));

  value = gtk_css_transform_value_alloc (array->len);
  memcpy (value->transforms, array->data, sizeof (GtkCssTransform) * array->len);

  g_array_free (array, TRUE);

  return value;
}

gboolean
_gtk_css_transform_value_get_matrix (const GtkCssValue *transform,
                                     cairo_matrix_t    *matrix)
{
  cairo_matrix_t invert;

  g_return_val_if_fail (transform->class == &GTK_CSS_VALUE_TRANSFORM, FALSE);
  g_return_val_if_fail (matrix != NULL, FALSE);
  
  gtk_css_transform_value_compute_matrix (transform, &invert);

  *matrix = invert;

  if (cairo_matrix_invert (&invert) != CAIRO_STATUS_SUCCESS)
    return FALSE;

  return TRUE;
}

