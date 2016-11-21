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
    graphene_matrix_t   matrix;
  }                   matrix;
  struct {
    GtkCssTransformType type;
    GtkCssValue        *x;
    GtkCssValue        *y;
    GtkCssValue        *z;
  }                   translate, scale;
  struct {
    GtkCssTransformType type;
    GtkCssValue        *x;
    GtkCssValue        *y;
  }                   skew;
  struct {
    GtkCssTransformType type;
    GtkCssValue        *x;
    GtkCssValue        *y;
    GtkCssValue        *z;
    GtkCssValue        *angle;
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
      _gtk_css_value_unref (transform->translate.z);
      break;
    case GTK_CSS_TRANSFORM_ROTATE:
      _gtk_css_value_unref (transform->rotate.x);
      _gtk_css_value_unref (transform->rotate.y);
      _gtk_css_value_unref (transform->rotate.z);
      _gtk_css_value_unref (transform->rotate.angle);
      break;
    case GTK_CSS_TRANSFORM_SCALE:
      _gtk_css_value_unref (transform->scale.x);
      _gtk_css_value_unref (transform->scale.y);
      _gtk_css_value_unref (transform->scale.z);
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
      graphene_matrix_init_identity (&transform->matrix.matrix);
      break;
    case GTK_CSS_TRANSFORM_TRANSLATE:
      transform->translate.x = _gtk_css_number_value_new (0, GTK_CSS_PX);
      transform->translate.y = _gtk_css_number_value_new (0, GTK_CSS_PX);
      transform->translate.z = _gtk_css_number_value_new (0, GTK_CSS_PX);
      break;
    case GTK_CSS_TRANSFORM_ROTATE:
      transform->rotate.x = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
      transform->rotate.y = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
      transform->rotate.z = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
      transform->rotate.angle = _gtk_css_number_value_new (0, GTK_CSS_DEG);
      break;
    case GTK_CSS_TRANSFORM_SCALE:
      transform->scale.x = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
      transform->scale.y = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
      transform->scale.z = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
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
gtk_css_transform_apply (const GtkCssTransform   *transform,
                         graphene_matrix_t       *matrix)
{
  graphene_matrix_t skew, tmp;

  switch (transform->type)
    {
    case GTK_CSS_TRANSFORM_MATRIX:
      graphene_matrix_multiply (matrix, &transform->matrix.matrix, &tmp);
      graphene_matrix_init_from_matrix (matrix, &tmp);
      break;
    case GTK_CSS_TRANSFORM_TRANSLATE:
      graphene_matrix_translate (matrix, &GRAPHENE_POINT3D_INIT(
                                 _gtk_css_number_value_get (transform->translate.x, 100),
                                 _gtk_css_number_value_get (transform->translate.y, 100),
                                 _gtk_css_number_value_get (transform->translate.z, 100)));
      break;
    case GTK_CSS_TRANSFORM_ROTATE:
      {
        graphene_vec3_t vec;

        graphene_vec3_init (&vec,
                            _gtk_css_number_value_get (transform->rotate.x, 1),
                            _gtk_css_number_value_get (transform->rotate.y, 1),
                            _gtk_css_number_value_get (transform->rotate.z, 1));
        graphene_matrix_rotate (matrix,
                                _gtk_css_number_value_get (transform->rotate.angle, 100),
                                &vec);
      }
      break;
    case GTK_CSS_TRANSFORM_SCALE:
      graphene_matrix_scale (matrix,
                             _gtk_css_number_value_get (transform->scale.x, 1),
                             _gtk_css_number_value_get (transform->scale.y, 1),
                             _gtk_css_number_value_get (transform->scale.z, 1));
      break;
    case GTK_CSS_TRANSFORM_SKEW:
      graphene_matrix_init_skew (&skew,
                                 _gtk_css_number_value_get (transform->skew.x, 100),
                                 _gtk_css_number_value_get (transform->skew.y, 100));
      graphene_matrix_multiply (matrix, &skew, &tmp);
      graphene_matrix_init_from_matrix (matrix, &tmp);
      break;
    case GTK_CSS_TRANSFORM_SKEW_X:
      graphene_matrix_init_skew (&skew,
                                 _gtk_css_number_value_get (transform->skew_x.skew, 100),
                                 0);
      graphene_matrix_multiply (matrix, &skew, &tmp);
      graphene_matrix_init_from_matrix (matrix, &tmp);
      break;
    case GTK_CSS_TRANSFORM_SKEW_Y:
      graphene_matrix_init_skew (&skew,
                                 0,
                                 _gtk_css_number_value_get (transform->skew_y.skew, 100));
      graphene_matrix_multiply (matrix, &skew, &tmp);
      graphene_matrix_init_from_matrix (matrix, &tmp);
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
                                        graphene_matrix_t *matrix)
{
  guint i;

  graphene_matrix_init_identity (matrix);

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
                           GtkCssStyle             *style,
                           GtkCssStyle             *parent_style)
{
  dest->type = src->type;

  switch (src->type)
    {
    case GTK_CSS_TRANSFORM_MATRIX:
      return TRUE;
    case GTK_CSS_TRANSFORM_TRANSLATE:
      dest->translate.x = _gtk_css_value_compute (src->translate.x, property_id, provider, style, parent_style);
      dest->translate.y = _gtk_css_value_compute (src->translate.y, property_id, provider, style, parent_style);
      dest->translate.z = _gtk_css_value_compute (src->translate.z, property_id, provider, style, parent_style);
      return dest->translate.x == src->translate.x
          && dest->translate.y == src->translate.y
          && dest->translate.z == src->translate.z;
    case GTK_CSS_TRANSFORM_ROTATE:
      dest->rotate.x = _gtk_css_value_compute (src->rotate.x, property_id, provider, style, parent_style);
      dest->rotate.y = _gtk_css_value_compute (src->rotate.y, property_id, provider, style, parent_style);
      dest->rotate.z = _gtk_css_value_compute (src->rotate.z, property_id, provider, style, parent_style);
      dest->rotate.angle = _gtk_css_value_compute (src->rotate.angle, property_id, provider, style, parent_style);
      return dest->rotate.x == src->rotate.x
          && dest->rotate.y == src->rotate.y
          && dest->rotate.z == src->rotate.z
          && dest->rotate.angle == src->rotate.angle;
    case GTK_CSS_TRANSFORM_SCALE:
      dest->scale.x = _gtk_css_value_compute (src->scale.x, property_id, provider, style, parent_style);
      dest->scale.y = _gtk_css_value_compute (src->scale.y, property_id, provider, style, parent_style);
      dest->scale.z = _gtk_css_value_compute (src->scale.z, property_id, provider, style, parent_style);
      return dest->scale.x == src->scale.x
          && dest->scale.y == src->scale.y
          && dest->scale.z == src->scale.z;
    case GTK_CSS_TRANSFORM_SKEW:
      dest->skew.x = _gtk_css_value_compute (src->skew.x, property_id, provider, style, parent_style);
      dest->skew.y = _gtk_css_value_compute (src->skew.y, property_id, provider, style, parent_style);
      return dest->skew.x == src->skew.x
          && dest->skew.y == src->skew.y;
    case GTK_CSS_TRANSFORM_SKEW_X:
      dest->skew_x.skew = _gtk_css_value_compute (src->skew_x.skew, property_id, provider, style, parent_style);
      return dest->skew_x.skew == src->skew_x.skew;
    case GTK_CSS_TRANSFORM_SKEW_Y:
      dest->skew_y.skew = _gtk_css_value_compute (src->skew_y.skew, property_id, provider, style, parent_style);
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
                                 GtkCssStyle             *style,
                                 GtkCssStyle             *parent_style)
{
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
                                             style,
                                             parent_style);
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
      {
        guint i, j;

        for (i = 0; i < 4; i++)
          for (j = 0; j < 4; j++)
            {
              if (graphene_matrix_get_value (&transform1->matrix.matrix, i, j)
                  != graphene_matrix_get_value (&transform2->matrix.matrix, i, j))
                return FALSE;
            }
        return TRUE;
      }
    case GTK_CSS_TRANSFORM_TRANSLATE:
      return _gtk_css_value_equal (transform1->translate.x, transform2->translate.x)
          && _gtk_css_value_equal (transform1->translate.y, transform2->translate.y)
          && _gtk_css_value_equal (transform1->translate.z, transform2->translate.z);
    case GTK_CSS_TRANSFORM_ROTATE:
      return _gtk_css_value_equal (transform1->rotate.x, transform2->rotate.x)
          && _gtk_css_value_equal (transform1->rotate.y, transform2->rotate.y)
          && _gtk_css_value_equal (transform1->rotate.z, transform2->rotate.z)
          && _gtk_css_value_equal (transform1->rotate.angle, transform2->rotate.angle);
    case GTK_CSS_TRANSFORM_SCALE:
      return _gtk_css_value_equal (transform1->scale.x, transform2->scale.x)
          && _gtk_css_value_equal (transform1->scale.y, transform2->scale.y)
          && _gtk_css_value_equal (transform1->scale.z, transform2->scale.z);
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
      graphene_matrix_interpolate (&start->matrix.matrix,
                                   &end->matrix.matrix,
                                   progress,
                                   &result->matrix.matrix);
      break;
    case GTK_CSS_TRANSFORM_TRANSLATE:
      result->translate.x = _gtk_css_value_transition (start->translate.x, end->translate.x, property_id, progress);
      result->translate.y = _gtk_css_value_transition (start->translate.y, end->translate.y, property_id, progress);
      result->translate.z = _gtk_css_value_transition (start->translate.z, end->translate.z, property_id, progress);
      break;
    case GTK_CSS_TRANSFORM_ROTATE:
      result->rotate.x = _gtk_css_value_transition (start->rotate.x, end->rotate.x, property_id, progress);
      result->rotate.y = _gtk_css_value_transition (start->rotate.y, end->rotate.y, property_id, progress);
      result->rotate.z = _gtk_css_value_transition (start->rotate.z, end->rotate.z, property_id, progress);
      result->rotate.angle = _gtk_css_value_transition (start->rotate.angle, end->rotate.angle, property_id, progress);
      break;
    case GTK_CSS_TRANSFORM_SCALE:
      result->scale.x = _gtk_css_value_transition (start->scale.x, end->scale.x, property_id, progress);
      result->scale.y = _gtk_css_value_transition (start->scale.y, end->scale.y, property_id, progress);
      result->scale.z = _gtk_css_value_transition (start->scale.z, end->scale.z, property_id, progress);
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
          graphene_matrix_t start_matrix, end_matrix;

          graphene_matrix_init_identity (&start_matrix);
          gtk_css_transform_value_compute_matrix (start, &start_matrix);
          graphene_matrix_init_identity (&end_matrix);
          gtk_css_transform_value_compute_matrix (end, &end_matrix);

          result = gtk_css_transform_value_alloc (1);
          result->transforms[0].type = GTK_CSS_TRANSFORM_MATRIX;
          graphene_matrix_interpolate (&start_matrix, &end_matrix, progress, &result->transforms[0].matrix.matrix);

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
      if (graphene_matrix_is_2d (&transform->matrix.matrix))
        {
          g_string_append (string, "matrix(");
          g_ascii_dtostr (buf, sizeof (buf), graphene_matrix_get_value (&transform->matrix.matrix, 0, 0));
          g_string_append (string, buf);
          g_string_append (string, ", ");
          g_ascii_dtostr (buf, sizeof (buf), graphene_matrix_get_value (&transform->matrix.matrix, 0, 1));
          g_string_append (string, buf);
          g_string_append (string, ", ");
          g_ascii_dtostr (buf, sizeof (buf), graphene_matrix_get_value (&transform->matrix.matrix, 0, 2));
          g_string_append (string, buf);
          g_string_append (string, ", ");
          g_ascii_dtostr (buf, sizeof (buf), graphene_matrix_get_value (&transform->matrix.matrix, 1, 0));
          g_string_append (string, buf);
          g_string_append (string, ", ");
          g_ascii_dtostr (buf, sizeof (buf), graphene_matrix_get_value (&transform->matrix.matrix, 1, 1));
          g_string_append (string, buf);
          g_string_append (string, ", ");
          g_ascii_dtostr (buf, sizeof (buf), graphene_matrix_get_value (&transform->matrix.matrix, 1, 2));
          g_string_append (string, buf);
          g_string_append (string, ")");
        }
      else
        {
          guint i;

          g_string_append (string, "matrix3d(");
          for (i = 0; i < 16; i++)
            {
              g_ascii_dtostr (buf, sizeof (buf), graphene_matrix_get_value (&transform->matrix.matrix, i / 4, i % 4));
              g_string_append (string, buf);
              if (i < 15)
                g_string_append (string, ", ");
            }
          g_string_append (string, ")");
        }
      break;
    case GTK_CSS_TRANSFORM_TRANSLATE:
      g_string_append (string, "translate3d(");
      _gtk_css_value_print (transform->translate.x, string);
      g_string_append (string, ", ");
      _gtk_css_value_print (transform->translate.y, string);
      g_string_append (string, ", ");
      _gtk_css_value_print (transform->translate.z, string);
      g_string_append (string, ")");
      break;
    case GTK_CSS_TRANSFORM_ROTATE:
      g_string_append (string, "rotate3d(");
      _gtk_css_value_print (transform->rotate.x, string);
      g_string_append (string, ", ");
      _gtk_css_value_print (transform->rotate.y, string);
      g_string_append (string, ", ");
      _gtk_css_value_print (transform->rotate.z, string);
      g_string_append (string, ", ");
      _gtk_css_value_print (transform->rotate.angle, string);
      g_string_append (string, ")");
      break;
    case GTK_CSS_TRANSFORM_SCALE:
      if (_gtk_css_number_value_get (transform->scale.z, 100) == 1)
        {
          g_string_append (string, "scale(");
          _gtk_css_value_print (transform->scale.x, string);
          if (!_gtk_css_value_equal (transform->scale.x, transform->scale.y))
            {
              g_string_append (string, ", ");
              _gtk_css_value_print (transform->scale.y, string);
            }
          g_string_append (string, ")");
        }
      else
        {
          g_string_append (string, "scale3d(");
          _gtk_css_value_print (transform->scale.x, string);
          g_string_append (string, ", ");
          _gtk_css_value_print (transform->scale.y, string);
          g_string_append (string, ", ");
          _gtk_css_value_print (transform->scale.z, string);
          g_string_append (string, ")");
        }
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
      double xx, xy, x0, yx, yy, y0;
      transform->type = GTK_CSS_TRANSFORM_MATRIX;

      /* FIXME: Improve error handling here */
      if (!_gtk_css_parser_try_double (parser, &xx)
          || !_gtk_css_parser_try (parser, ",", TRUE)
          || !_gtk_css_parser_try_double (parser, &xy)
          || !_gtk_css_parser_try (parser, ",", TRUE)
          || !_gtk_css_parser_try_double (parser, &x0)
          || !_gtk_css_parser_try (parser, ",", TRUE)
          || !_gtk_css_parser_try_double (parser, &yx)
          || !_gtk_css_parser_try (parser, ",", TRUE)
          || !_gtk_css_parser_try_double (parser, &yy)
          || !_gtk_css_parser_try (parser, ",", TRUE)
          || !_gtk_css_parser_try_double (parser, &y0))
        {
          _gtk_css_parser_error (parser, "invalid syntax for matrix()");
          return FALSE;
        }
      graphene_matrix_init_from_2d (&transform->matrix.matrix, 
                                    xx, yx, xy, yy, x0, y0);
    }
  else if (_gtk_css_parser_try (parser, "matrix3d(", TRUE))
    {
      float f[16];
      double d;
      guint i;

      transform->type = GTK_CSS_TRANSFORM_MATRIX;

      for (i = 0; i < 16; i++)
        {
          if (!_gtk_css_parser_try_double (parser, &d))
            break;
          f[i] = d;

          if (i < 15 && !_gtk_css_parser_try (parser, ",", TRUE))
            break;
        }

      if (i < 16)
        {
          /* FIXME: Improve error handling here */
          _gtk_css_parser_error (parser, "invalid syntax for matrix3d()");
          return FALSE;
        }
      graphene_matrix_init_from_float (&transform->matrix.matrix, f);
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
      transform->translate.z = _gtk_css_number_value_new (0, GTK_CSS_PX);
    }
  else if (_gtk_css_parser_try (parser, "translateX(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_TRANSLATE;

      transform->translate.x = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_LENGTH);
      if (transform->translate.x == NULL)
        return FALSE;
      
      transform->translate.y = _gtk_css_number_value_new (0, GTK_CSS_PX);
      transform->translate.z = _gtk_css_number_value_new (0, GTK_CSS_PX);
    }
  else if (_gtk_css_parser_try (parser, "translateY(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_TRANSLATE;

      transform->translate.y = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_LENGTH);
      if (transform->translate.y == NULL)
        return FALSE;
      
      transform->translate.x = _gtk_css_number_value_new (0, GTK_CSS_PX);
      transform->translate.z = _gtk_css_number_value_new (0, GTK_CSS_PX);
    }
  else if (_gtk_css_parser_try (parser, "translateZ(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_TRANSLATE;

      transform->translate.z = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_LENGTH);
      if (transform->translate.z == NULL)
        return FALSE;
      
      transform->translate.x = _gtk_css_number_value_new (0, GTK_CSS_PX);
      transform->translate.y = _gtk_css_number_value_new (0, GTK_CSS_PX);
    }
  else if (_gtk_css_parser_try (parser, "translate3d(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_TRANSLATE;

      transform->translate.x = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_LENGTH);
      if (transform->translate.x != NULL)
        {
          transform->translate.y = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_LENGTH);
          if (transform->translate.y != NULL)
            {
              transform->translate.z = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_LENGTH);
              if (transform->translate.z != NULL)
                goto out;
            }
          _gtk_css_value_unref (transform->translate.y);
        }
      
      _gtk_css_value_unref (transform->translate.x);
      return FALSE;
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
      transform->scale.z = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
    }
  else if (_gtk_css_parser_try (parser, "scaleX(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_SCALE;

      transform->scale.x = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
      if (transform->scale.x == NULL)
        return FALSE;
      
      transform->scale.y = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
      transform->scale.z = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
    }
  else if (_gtk_css_parser_try (parser, "scaleY(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_SCALE;

      transform->scale.y = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
      if (transform->scale.y == NULL)
        return FALSE;
      
      transform->scale.x = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
      transform->scale.z = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
    }
  else if (_gtk_css_parser_try (parser, "scaleZ(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_SCALE;

      transform->scale.z = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
      if (transform->scale.z == NULL)
        return FALSE;
      
      transform->scale.x = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
      transform->scale.y = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
    }
  else if (_gtk_css_parser_try (parser, "scale3d(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_SCALE;

      transform->scale.x = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
      if (transform->scale.x != NULL)
        {
          transform->scale.y = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
          if (transform->scale.y != NULL)
            {
              transform->scale.z = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
              if (transform->scale.z != NULL)
                goto out;
            }
          _gtk_css_value_unref (transform->scale.y);
        }
      
      _gtk_css_value_unref (transform->scale.x);
      return FALSE;
    }
  else if (_gtk_css_parser_try (parser, "rotate(", TRUE) || 
           _gtk_css_parser_try (parser, "rotateZ(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_ROTATE;

      transform->rotate.angle = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_ANGLE);
      if (transform->rotate.angle == NULL)
        return FALSE;
      transform->rotate.x = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
      transform->rotate.y = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
      transform->rotate.z = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
    }
  else if (_gtk_css_parser_try (parser, "rotateX(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_ROTATE;

      transform->rotate.angle = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_ANGLE);
      if (transform->rotate.angle == NULL)
        return FALSE;
      transform->rotate.x = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
      transform->rotate.y = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
      transform->rotate.z = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
    }
  else if (_gtk_css_parser_try (parser, "rotateY(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_ROTATE;

      transform->rotate.angle = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_ANGLE);
      if (transform->rotate.angle == NULL)
        return FALSE;
      transform->rotate.x = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
      transform->rotate.y = _gtk_css_number_value_new (1, GTK_CSS_NUMBER);
      transform->rotate.z = _gtk_css_number_value_new (0, GTK_CSS_NUMBER);
    }
  else if (_gtk_css_parser_try (parser, "rotate3d(", TRUE))
    {
      transform->type = GTK_CSS_TRANSFORM_ROTATE;

      transform->rotate.x = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
      if (transform->rotate.x != NULL)
        {
          transform->rotate.y = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
          if (transform->rotate.y != NULL)
            {
              transform->rotate.z = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
              if (transform->rotate.z != NULL)
                {
                  transform->rotate.angle = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_ANGLE);
                  if (transform->rotate.angle != NULL)
                    goto out;
                }
              _gtk_css_value_unref (transform->rotate.z);
            }
          _gtk_css_value_unref (transform->rotate.y);
        }
      _gtk_css_value_unref (transform->rotate.x);
      
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

out:
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
gtk_css_transform_value_get_matrix (const GtkCssValue *transform,
                                    graphene_matrix_t *matrix)
{
  graphene_matrix_t invert;

  g_return_val_if_fail (transform->class == &GTK_CSS_VALUE_TRANSFORM, FALSE);
  g_return_val_if_fail (matrix != NULL, FALSE);
  
  gtk_css_transform_value_compute_matrix (transform, matrix);

  return graphene_matrix_inverse (matrix, &invert);
}

