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


/**
 * SECTION:gsktransform
 * @Title: GskTransform
 * @Short_description: A description for transform operations
 *
 * #GskTransform is an object to describe transform matrices. Unlike
 * #graphene_matrix_t, #GskTransform retains the steps in how a transform was
 * constructed, and allows inspecting them. It is modeled after the way
 * CSS describes transforms.
 *
 * #GskTransform objects are immutable and cannot be changed after creation.
 * This means code can safely expose them as properties of objects without
 * having to worry about others changing them.
 */

#include "config.h"

#include "gsktransformprivate.h"

typedef struct _GskTransformClass GskTransformClass;

#define GSK_IS_TRANSFORM_TYPE(self,type) ((self) == NULL ? (type) == GSK_TRANSFORM_TYPE_IDENTITY : (self)->transform_class->transform_type == (type))

struct _GskTransform
{
  const GskTransformClass *transform_class;
  
  volatile int ref_count;
  GskTransform *next;
};

struct _GskTransformClass
{
  GskTransformType transform_type;
  gsize struct_size;
  const char *type_name;

  void                  (* finalize)            (GskTransform           *transform);
  GskMatrixCategory     (* categorize)          (GskTransform           *transform);
  void                  (* to_matrix)           (GskTransform           *transform,
                                                 graphene_matrix_t      *out_matrix);
  gboolean              (* apply_2d)            (GskTransform           *transform,
                                                 float                  *out_xx,
                                                 float                  *out_yx,
                                                 float                  *out_xy,
                                                 float                  *out_yy,
                                                 float                  *out_dx,
                                                 float                  *out_dy);
  gboolean              (* apply_affine)        (GskTransform           *transform,
                                                 float                  *out_scale_x,
                                                 float                  *out_scale_y,
                                                 float                  *out_dx,
                                                 float                  *out_dy);
  gboolean              (* apply_translate)     (GskTransform           *transform,
                                                 float                  *out_dx,
                                                 float                  *out_dy);
  void                  (* print)               (GskTransform           *transform,
                                                 GString                *string);
  GskTransform *        (* apply)               (GskTransform           *transform,
                                                 GskTransform           *apply_to);
  /* both matrices have the same type */
  gboolean              (* equal)               (GskTransform           *first_transform,
                                                 GskTransform           *second_transform);
};

/**
 * GskTransform: (ref-func gsk_transform_ref) (unref-func gsk_transform_unref)
 *
 * The `GskTransform` structure contains only private data.
 */

G_DEFINE_BOXED_TYPE (GskTransform, gsk_transform,
                     gsk_transform_ref,
                     gsk_transform_unref)

/*<private>
 * gsk_transform_is_identity:
 * @transform: (allow-none): A transform or %NULL
 *
 * Checks if the transform is a representation of the identity
 * transform.
 *
 * This is different from a transform like `scale(2) scale(0.5)`
 * which just results in an identity transform when simplified.
 *
 * Returns: %TRUE  if this transform is a representation of
 *     the identity transform
 **/
static gboolean
gsk_transform_is_identity (GskTransform *self)
{
  return self == NULL ||
         (GSK_IS_TRANSFORM_TYPE (self, GSK_TRANSFORM_TYPE_IDENTITY) && gsk_transform_is_identity (self->next));
}

/*< private >
 * gsk_transform_alloc:
 * @transform_class: class structure for this self
 * @next: (transfer full) Next matrix to multiply with or %NULL if none
 *
 * Returns: (transfer full): the newly created #GskTransform
 */
static gpointer
gsk_transform_alloc (const GskTransformClass *transform_class,
                     GskTransform            *next)
{
  GskTransform *self;

  g_return_val_if_fail (transform_class != NULL, NULL);

  self = g_malloc0 (transform_class->struct_size);

  self->transform_class = transform_class;
  self->ref_count = 1;
  self->next = gsk_transform_is_identity (next) ? NULL : next;

  return self;
}

/*** IDENTITY ***/

static void
gsk_identity_transform_finalize (GskTransform *transform)
{
}

static GskMatrixCategory
gsk_identity_transform_categorize (GskTransform *transform)
{
  return GSK_MATRIX_CATEGORY_IDENTITY;
}

static void
gsk_identity_transform_to_matrix (GskTransform      *transform,
                                  graphene_matrix_t *out_matrix)
{
  graphene_matrix_init_identity (out_matrix);
}

static gboolean
gsk_identity_transform_apply_2d (GskTransform *transform,
                                 float        *out_xx,
                                 float        *out_yx,
                                 float        *out_xy,
                                 float        *out_yy,
                                 float        *out_dx,
                                 float        *out_dy)
{
  return TRUE;
}

static gboolean
gsk_identity_transform_apply_affine (GskTransform *transform,
                                     float        *out_scale_x,
                                     float        *out_scale_y,
                                     float        *out_dx,
                                     float        *out_dy)
{
  return TRUE;
}

static gboolean
gsk_identity_transform_apply_translate (GskTransform *transform,
                                        float        *out_dx,
                                        float        *out_dy)
{
  return TRUE;
}

static void
gsk_identity_transform_print (GskTransform *transform,
                              GString      *string)
{
  g_string_append (string, "identity");
}

static GskTransform *
gsk_identity_transform_apply (GskTransform *transform,
                              GskTransform *apply_to)
{
  return gsk_transform_identity (apply_to);
}

static gboolean
gsk_identity_transform_equal (GskTransform *first_transform,
                              GskTransform *second_transform)
{
  return TRUE;
}

static const GskTransformClass GSK_IDENTITY_TRANSFORM_CLASS =
{
  GSK_TRANSFORM_TYPE_IDENTITY,
  sizeof (GskTransform),
  "GskIdentityMatrix",
  gsk_identity_transform_finalize,
  gsk_identity_transform_categorize,
  gsk_identity_transform_to_matrix,
  gsk_identity_transform_apply_2d,
  gsk_identity_transform_apply_affine,
  gsk_identity_transform_apply_translate,
  gsk_identity_transform_print,
  gsk_identity_transform_apply,
  gsk_identity_transform_equal,
};

/**
 * gsk_transform_identity:
 * @next: (allow-none): the next transform operation or %NULL
 *
 * Adds an identity multiplication into the list of matrix operations.
 *
 * This operation is generally useless, but may be useful when interpolating
 * matrices, because the identity matrix can be interpolated to and from
 * everything, so an identity matrix can be used as a keyframe between two
 * different types of matrices.
 *
 * Returns: The new matrix
 **/
GskTransform *
gsk_transform_identity (GskTransform *next)
{
  if (gsk_transform_is_identity (next))
    return next;

  return gsk_transform_alloc (&GSK_IDENTITY_TRANSFORM_CLASS, next);
}

/*** MATRIX ***/

typedef struct _GskMatrixTransform GskMatrixTransform;

struct _GskMatrixTransform
{
  GskTransform parent;

  graphene_matrix_t matrix;
  GskMatrixCategory category;
};

static void
gsk_matrix_transform_finalize (GskTransform *self)
{
}

static GskMatrixCategory
gsk_matrix_transform_categorize (GskTransform *transform)
{
  GskMatrixTransform *self = (GskMatrixTransform *) transform;

  return self->category;
}

static void
gsk_matrix_transform_to_matrix (GskTransform      *transform,
                                graphene_matrix_t *out_matrix)
{
  GskMatrixTransform *self = (GskMatrixTransform *) transform;

  graphene_matrix_init_from_matrix (out_matrix, &self->matrix);
}

static gboolean
gsk_matrix_transform_apply_2d (GskTransform *transform,
                               float        *out_xx,
                               float        *out_yx,
                               float        *out_xy,
                               float        *out_yy,
                               float        *out_dx,
                               float        *out_dy)
{
  return FALSE;
}

static gboolean
gsk_matrix_transform_apply_affine (GskTransform *transform,
                                   float        *out_scale_x,
                                   float        *out_scale_y,
                                   float        *out_dx,
                                   float        *out_dy)
{
  GskMatrixTransform *self = (GskMatrixTransform *) transform;

  switch (self->category)
  {
    case GSK_MATRIX_CATEGORY_UNKNOWN:
    case GSK_MATRIX_CATEGORY_ANY:
    case GSK_MATRIX_CATEGORY_INVERTIBLE:
    default:
      return FALSE;

    case GSK_MATRIX_CATEGORY_2D_AFFINE:
      *out_dx += *out_scale_x * graphene_matrix_get_value (&self->matrix, 3, 0);
      *out_dy += *out_scale_y * graphene_matrix_get_value (&self->matrix, 3, 1);
      *out_scale_x *= graphene_matrix_get_value (&self->matrix, 0, 0);
      *out_scale_y *= graphene_matrix_get_value (&self->matrix, 1, 1);
      return TRUE;

    case GSK_MATRIX_CATEGORY_2D_TRANSLATE:
      *out_dx += *out_scale_x * graphene_matrix_get_value (&self->matrix, 3, 0);
      *out_dy += *out_scale_y * graphene_matrix_get_value (&self->matrix, 3, 1);
      return TRUE;

    case GSK_MATRIX_CATEGORY_IDENTITY:
      return TRUE;
  }
}

static gboolean
gsk_matrix_transform_apply_translate (GskTransform *transform,
                                      float        *out_dx,
                                      float        *out_dy)
{
  GskMatrixTransform *self = (GskMatrixTransform *) transform;

  switch (self->category)
  {
    case GSK_MATRIX_CATEGORY_UNKNOWN:
    case GSK_MATRIX_CATEGORY_ANY:
    case GSK_MATRIX_CATEGORY_INVERTIBLE:
    case GSK_MATRIX_CATEGORY_2D_AFFINE:
    default:
      return FALSE;

    case GSK_MATRIX_CATEGORY_2D_TRANSLATE:
      *out_dx += graphene_matrix_get_value (&self->matrix, 3, 0);
      *out_dy += graphene_matrix_get_value (&self->matrix, 3, 1);
      return TRUE;

    case GSK_MATRIX_CATEGORY_IDENTITY:
      return TRUE;
  }
  return TRUE;
}

static void
string_append_double (GString *string,
                      double   d)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_formatd (buf, G_ASCII_DTOSTR_BUF_SIZE, "%g", d);
  g_string_append (string, buf);
}

static void
gsk_matrix_transform_print (GskTransform *transform,
                            GString      *string)
{
  GskMatrixTransform *self = (GskMatrixTransform *) transform;
  guint i;
  float f[16];

  g_string_append (string, "matrix3d(");
  graphene_matrix_to_float (&self->matrix, f);
  for (i = 0; i < 16; i++)
    {
      if (i > 0)
        g_string_append (string, ", ");
      string_append_double (string, f[i]);
    }
  g_string_append (string, ")");
}

static GskTransform *
gsk_matrix_transform_apply (GskTransform *transform,
                            GskTransform *apply_to)
{
  GskMatrixTransform *self = (GskMatrixTransform *) transform;

  return gsk_transform_matrix_with_category (apply_to,
                                             &self->matrix,
                                             self->category);
}

static gboolean
gsk_matrix_transform_equal (GskTransform *first_transform,
                            GskTransform *second_transform)
{
  GskMatrixTransform *first = (GskMatrixTransform *) first_transform;
  GskMatrixTransform *second = (GskMatrixTransform *) second_transform;

  /* Crude, but better than just returning FALSE */
  return memcmp (&first->matrix, &second->matrix, sizeof (graphene_matrix_t)) == 0;
}

static const GskTransformClass GSK_TRANSFORM_TRANSFORM_CLASS =
{
  GSK_TRANSFORM_TYPE_TRANSFORM,
  sizeof (GskMatrixTransform),
  "GskMatrixTransform",
  gsk_matrix_transform_finalize,
  gsk_matrix_transform_categorize,
  gsk_matrix_transform_to_matrix,
  gsk_matrix_transform_apply_2d,
  gsk_matrix_transform_apply_affine,
  gsk_matrix_transform_apply_translate,
  gsk_matrix_transform_print,
  gsk_matrix_transform_apply,
  gsk_matrix_transform_equal,
};

GskTransform *
gsk_transform_matrix_with_category (GskTransform            *next,
                                    const graphene_matrix_t *matrix,
                                    GskMatrixCategory        category)
{
  GskMatrixTransform *result = gsk_transform_alloc (&GSK_TRANSFORM_TRANSFORM_CLASS, next);

  graphene_matrix_init_from_matrix (&result->matrix, matrix);
  result->category = category;

  return &result->parent;
}

/**
 * gsk_transform_matrix:
 * @next: (allow-none): the next transform
 * @matrix: the matrix to multiply @next with
 *
 * Multiplies @next with the given @matrix.
 *
 * Returns: The new matrix
 **/
GskTransform *
gsk_transform_matrix (GskTransform            *next,
                      const graphene_matrix_t *matrix)
{
  return gsk_transform_matrix_with_category (next, matrix, GSK_MATRIX_CATEGORY_UNKNOWN);
}

/*** TRANSLATE ***/

typedef struct _GskTranslateTransform GskTranslateTransform;

struct _GskTranslateTransform
{
  GskTransform parent;

  graphene_point3d_t point;
};

static void
gsk_translate_transform_finalize (GskTransform *self)
{
}

static GskMatrixCategory
gsk_translate_transform_categorize (GskTransform *transform)
{
  GskTranslateTransform *self = (GskTranslateTransform *) transform;

  if (self->point.z != 0.0)
    return GSK_MATRIX_CATEGORY_INVERTIBLE;

  return GSK_MATRIX_CATEGORY_2D_TRANSLATE;
}

static void
gsk_translate_transform_to_matrix (GskTransform      *transform,
                                   graphene_matrix_t *out_matrix)
{
  GskTranslateTransform *self = (GskTranslateTransform *) transform;

  graphene_matrix_init_translate (out_matrix, &self->point);
}

static gboolean
gsk_translate_transform_apply_2d (GskTransform *transform,
                                  float        *out_xx,
                                  float        *out_yx,
                                  float        *out_xy,
                                  float        *out_yy,
                                  float        *out_dx,
                                  float        *out_dy)
{
  GskTranslateTransform *self = (GskTranslateTransform *) transform;

  if (self->point.z != 0.0)
    return FALSE;

  *out_dx += *out_xx * self->point.x + *out_xy * self->point.y;
  *out_dy += *out_yx * self->point.x + *out_yy * self->point.y;

  return TRUE;
}

static gboolean
gsk_translate_transform_apply_affine (GskTransform *transform,
                                      float        *out_scale_x,
                                      float        *out_scale_y,
                                      float        *out_dx,
                                      float        *out_dy)
{
  GskTranslateTransform *self = (GskTranslateTransform *) transform;

  if (self->point.z != 0.0)
    return FALSE;

  *out_dx += *out_scale_x * self->point.x;
  *out_dy += *out_scale_y * self->point.y;

  return TRUE;
}

static gboolean
gsk_translate_transform_apply_translate (GskTransform *transform,
                                         float        *out_dx,
                                         float        *out_dy)
{
  GskTranslateTransform *self = (GskTranslateTransform *) transform;

  if (self->point.z != 0.0)
    return FALSE;

  *out_dx += self->point.x;
  *out_dy += self->point.y;

  return TRUE;
}

static GskTransform *
gsk_translate_transform_apply (GskTransform *transform,
                               GskTransform *apply_to)
{
  GskTranslateTransform *self = (GskTranslateTransform *) transform;

  return gsk_transform_translate_3d (apply_to, &self->point);
}

static gboolean
gsk_translate_transform_equal (GskTransform *first_transform,
                               GskTransform *second_transform)
{
  GskTranslateTransform *first = (GskTranslateTransform *) first_transform;
  GskTranslateTransform *second = (GskTranslateTransform *) second_transform;

  return graphene_point3d_equal (&first->point, &second->point);
}

static void
gsk_translate_transform_print (GskTransform *transform,
                               GString      *string)
{
  GskTranslateTransform *self = (GskTranslateTransform *) transform;

  if (self->point.z == 0)
    g_string_append (string, "translate(");
  else
    g_string_append (string, "translate3d(");

  string_append_double (string, self->point.x);
  g_string_append (string, ", ");
  string_append_double (string, self->point.y);
  if (self->point.z != 0)
    {
      g_string_append (string, ", ");
      string_append_double (string, self->point.y);
    }
  g_string_append (string, ")");
}

static const GskTransformClass GSK_TRANSLATE_TRANSFORM_CLASS =
{
  GSK_TRANSFORM_TYPE_TRANSLATE,
  sizeof (GskTranslateTransform),
  "GskTranslateTransform",
  gsk_translate_transform_finalize,
  gsk_translate_transform_categorize,
  gsk_translate_transform_to_matrix,
  gsk_translate_transform_apply_2d,
  gsk_translate_transform_apply_affine,
  gsk_translate_transform_apply_translate,
  gsk_translate_transform_print,
  gsk_translate_transform_apply,
  gsk_translate_transform_equal,
};

/**
 * gsk_transform_translate:
 * @next: (allow-none): the next transform
 * @point: the point to translate the matrix by
 *
 * Translates @next in 2dimensional space by @point.
 *
 * Returns: The new matrix
 **/
GskTransform *
gsk_transform_translate (GskTransform           *next,
                         const graphene_point_t *point)
{
  graphene_point3d_t point3d;

  graphene_point3d_init (&point3d, point->x, point->y, 0);

  return gsk_transform_translate_3d (next, &point3d);
}

/**
 * gsk_transform_translate_3d:
 * @next: (allow-none): the next transform
 * @point: the point to translate the matrix by
 *
 * Translates @next by @point.
 *
 * Returns: The new matrix
 **/
GskTransform *
gsk_transform_translate_3d (GskTransform             *next,
                            const graphene_point3d_t *point)
{
  GskTranslateTransform *result = gsk_transform_alloc (&GSK_TRANSLATE_TRANSFORM_CLASS, next);

  graphene_point3d_init_from_point (&result->point, point);

  return &result->parent;
}

/*** ROTATE ***/

typedef struct _GskRotateTransform GskRotateTransform;

struct _GskRotateTransform
{
  GskTransform parent;

  float angle;
  graphene_vec3_t axis;
};

static void
gsk_rotate_transform_finalize (GskTransform *self)
{
}

static GskMatrixCategory
gsk_rotate_transform_categorize (GskTransform *transform)
{
  return GSK_MATRIX_CATEGORY_INVERTIBLE;
}

static void
gsk_rotate_transform_to_matrix (GskTransform      *transform,
                                graphene_matrix_t *out_matrix)
{
  GskRotateTransform *self = (GskRotateTransform *) transform;

  graphene_matrix_init_rotate (out_matrix, self->angle, &self->axis);
}

static gboolean
gsk_rotate_transform_apply_2d (GskTransform *transform,
                               float        *out_xx,
                               float        *out_yx,
                               float        *out_xy,
                               float        *out_yy,
                               float        *out_dx,
                               float        *out_dy)
{
  /* FIXME: Do the right thing for normal rotations */
  return FALSE;
}

static gboolean
gsk_rotate_transform_apply_affine (GskTransform *transform,
                                   float        *out_scale_x,
                                   float        *out_scale_y,
                                   float        *out_dx,
                                   float        *out_dy)
{
  return FALSE;
}

static gboolean
gsk_rotate_transform_apply_translate (GskTransform *transform,
                                      float        *out_dx,
                                      float        *out_dy)
{
  return FALSE;
}

static GskTransform *
gsk_rotate_transform_apply (GskTransform *transform,
                            GskTransform *apply_to)
{
  GskRotateTransform *self = (GskRotateTransform *) transform;

  return gsk_transform_rotate_3d (apply_to, self->angle, &self->axis);
}

static gboolean
gsk_rotate_transform_equal (GskTransform *first_transform,
                            GskTransform *second_transform)
{
  GskRotateTransform *first = (GskRotateTransform *) first_transform;
  GskRotateTransform *second = (GskRotateTransform *) second_transform;

  return first->angle == second->angle
         && graphene_vec3_equal (&first->axis, &second->axis);
}

static void
gsk_rotate_transform_print (GskTransform *transform,
                            GString      *string)
{
  GskRotateTransform *self = (GskRotateTransform *) transform;
  graphene_vec3_t default_axis;

  graphene_vec3_init_from_vec3 (&default_axis, graphene_vec3_z_axis ());
  if (graphene_vec3_equal (&default_axis, &self->axis))
    {
      g_string_append (string, "rotate(");
      string_append_double (string, self->angle);
      g_string_append (string, ")");
    }
  else
    {
      float f[3];
      guint i;

      g_string_append (string, "rotate3d(");
      graphene_vec3_to_float (&self->axis, f);
      for (i = 0; i < 3; i++)
        {
          string_append_double (string, f[i]);
          g_string_append (string, ", ");
        }
      string_append_double (string, self->angle);
      g_string_append (string, ")");
    }
}

static const GskTransformClass GSK_ROTATE_TRANSFORM_CLASS =
{
  GSK_TRANSFORM_TYPE_ROTATE,
  sizeof (GskRotateTransform),
  "GskRotateTransform",
  gsk_rotate_transform_finalize,
  gsk_rotate_transform_categorize,
  gsk_rotate_transform_to_matrix,
  gsk_rotate_transform_apply_2d,
  gsk_rotate_transform_apply_affine,
  gsk_rotate_transform_apply_translate,
  gsk_rotate_transform_print,
  gsk_rotate_transform_apply,
  gsk_rotate_transform_equal,
};

/**
 * gsk_transform_rotate:
 * @next: (allow-none): the next transform
 * @angle: the rotation angle, in degrees (clockwise)
 *
 * Rotates @next @angle degrees in 2D - or in 3Dspeak, around the z axis.
 *
 * Returns: The new matrix
 **/
GskTransform *
gsk_transform_rotate (GskTransform *next,
                      float         angle)
{
  return gsk_transform_rotate_3d (next, angle, graphene_vec3_z_axis ());
}

/**
 * gsk_transform_rotate_3d:
 * @next: (allow-none): the next transform
 * @angle: the rotation angle, in degrees (clockwise)
 * @axis: The rotation axis
 *
 * Rotates @next @angle degrees around @axis.
 *
 * For a rotation in 2D space, use gsk_transform_rotate().
 *
 * Returns: The new matrix
 **/
GskTransform *
gsk_transform_rotate_3d (GskTransform          *next,
                         float                  angle,
                         const graphene_vec3_t *axis)
{
  GskRotateTransform *result = gsk_transform_alloc (&GSK_ROTATE_TRANSFORM_CLASS, next);

  result->angle = angle;
  graphene_vec3_init_from_vec3 (&result->axis, axis);

  return &result->parent;
}

/*** SCALE ***/

typedef struct _GskScaleTransform GskScaleTransform;

struct _GskScaleTransform
{
  GskTransform parent;

  float factor_x;
  float factor_y;
  float factor_z;
};

static void
gsk_scale_transform_finalize (GskTransform *self)
{
}

static GskMatrixCategory
gsk_scale_transform_categorize (GskTransform *transform)
{
  GskScaleTransform *self = (GskScaleTransform *) transform;

  if (self->factor_z != 1.0)
    return GSK_MATRIX_CATEGORY_INVERTIBLE;

  return GSK_MATRIX_CATEGORY_2D_AFFINE;
}

static void
gsk_scale_transform_to_matrix (GskTransform      *transform,
                               graphene_matrix_t *out_matrix)
{
  GskScaleTransform *self = (GskScaleTransform *) transform;

  graphene_matrix_init_scale (out_matrix, self->factor_x, self->factor_y, self->factor_z);
}

static gboolean
gsk_scale_transform_apply_2d (GskTransform *transform,
                              float        *out_xx,
                              float        *out_yx,
                              float        *out_xy,
                              float        *out_yy,
                              float        *out_dx,
                              float        *out_dy)
{
  GskScaleTransform *self = (GskScaleTransform *) transform;

  if (self->factor_z != 1.0)
    return FALSE;

  *out_xx *= self->factor_x;
  *out_yx *= self->factor_x;
  *out_xy *= self->factor_y;
  *out_yy *= self->factor_y;

  return TRUE;
}

static gboolean
gsk_scale_transform_apply_affine (GskTransform *transform,
                                  float        *out_scale_x,
                                  float        *out_scale_y,
                                  float        *out_dx,
                                  float        *out_dy)
{
  GskScaleTransform *self = (GskScaleTransform *) transform;

  if (self->factor_z != 1.0)
    return FALSE;

  *out_scale_x *= self->factor_x;
  *out_scale_y *= self->factor_y;

  return TRUE;
}

static gboolean
gsk_scale_transform_apply_translate (GskTransform *transform,
                                     float        *out_dx,
                                     float        *out_dy)
{
  return FALSE;
}

static GskTransform *
gsk_scale_transform_apply (GskTransform *transform,
                           GskTransform *apply_to)
{
  GskScaleTransform *self = (GskScaleTransform *) transform;

  return gsk_transform_scale_3d (apply_to, self->factor_x, self->factor_y, self->factor_z);
}

static gboolean
gsk_scale_transform_equal (GskTransform *first_transform,
                           GskTransform *second_transform)
{
  GskScaleTransform *first = (GskScaleTransform *) first_transform;
  GskScaleTransform *second = (GskScaleTransform *) second_transform;

  return first->factor_x == second->factor_x
         && first->factor_y == second->factor_y
         && first->factor_z == second->factor_z;
}

static void
gsk_scale_transform_print (GskTransform *transform,
                           GString      *string)
{
  GskScaleTransform *self = (GskScaleTransform *) transform;

  if (self->factor_z == 1.0)
    {
      g_string_append (string, "scale(");
      string_append_double (string, self->factor_x);
      if (self->factor_x != self->factor_y)
        {
          g_string_append (string, ", ");
          string_append_double (string, self->factor_y);
        }
      g_string_append (string, ")");
    }
  else
    {
      g_string_append (string, "scale3d(");
      string_append_double (string, self->factor_x);
      g_string_append (string, ", ");
      string_append_double (string, self->factor_y);
      g_string_append (string, ", ");
      string_append_double (string, self->factor_z);
      g_string_append (string, ")");
    }
}

static const GskTransformClass GSK_SCALE_TRANSFORM_CLASS =
{
  GSK_TRANSFORM_TYPE_SCALE,
  sizeof (GskScaleTransform),
  "GskScaleTransform",
  gsk_scale_transform_finalize,
  gsk_scale_transform_categorize,
  gsk_scale_transform_to_matrix,
  gsk_scale_transform_apply_2d,
  gsk_scale_transform_apply_affine,
  gsk_scale_transform_apply_translate,
  gsk_scale_transform_print,
  gsk_scale_transform_apply,
  gsk_scale_transform_equal,
};

/**
 * gsk_transform_scale:
 * @next: (allow-none): the next transform
 * @factor_x: scaling factor on the X axis
 * @factor_y: scaling factor on the Y axis
 *
 * Scales @next in 2-dimensional space by the given factors.
 * Use gsk_transform_scale_3d() to scale in all 3 dimensions.
 *
 * Returns: The new matrix
 **/
GskTransform *
gsk_transform_scale (GskTransform *next,
                     float         factor_x,
                     float         factor_y)
{
  return gsk_transform_scale_3d (next, factor_x, factor_y, 1.0);
}

/**
 * gsk_transform_scale_3d:
 * @next: (allow-none): the next transform
 * @factor_x: scaling factor on the X axis
 * @factor_y: scaling factor on the Y axis
 * @factor_z: scaling factor on the Z axis
 *
 * Scales @next by the given factors.
 *
 * Returns: The new matrix
 **/
GskTransform *
gsk_transform_scale_3d (GskTransform *next,
                        float         factor_x,
                        float         factor_y,
                        float         factor_z)
{
  GskScaleTransform *result = gsk_transform_alloc (&GSK_SCALE_TRANSFORM_CLASS, next);

  result->factor_x = factor_x;
  result->factor_y = factor_y;
  result->factor_z = factor_z;

  return &result->parent;
}

/*** PUBLIC API ***/

static void
gsk_transform_finalize (GskTransform *self)
{
  self->transform_class->finalize (self);

  gsk_transform_unref (self->next);

  g_free (self);
}

/**
 * gsk_transform_ref:
 * @self: (allow-none): a #GskTransform
 *
 * Acquires a reference on the given #GskTransform.
 *
 * Returns: (transfer none): the #GskTransform with an additional reference
 */
GskTransform *
gsk_transform_ref (GskTransform *self)
{
  if (self == NULL)
    return NULL;

  g_atomic_int_inc (&self->ref_count);

  return self;
}

/**
 * gsk_transform_unref:
 * @self: (allow-none): a #GskTransform
 *
 * Releases a reference on the given #GskTransform.
 *
 * If the reference was the last, the resources associated to the @self are
 * freed.
 */
void
gsk_transform_unref (GskTransform *self)
{
  if (self == NULL)
    return;

  if (g_atomic_int_dec_and_test (&self->ref_count))
    gsk_transform_finalize (self);
}

/**
 * gsk_transform_print:
 * @self: (allow-none): a #GskTransform
 * @string:  The string to print into
 *
 * Converts @self into a string representation suitable for printing that
 * can later be parsed with gsk_transform_parse().
 **/
void
gsk_transform_print (GskTransform *self,
                     GString      *string)
{
  g_return_if_fail (string != NULL);

  if (self == NULL)
    {
      g_string_append (string, "none");
      return;
    }

  if (self->next != NULL)
    {
      gsk_transform_print (self->next, string);
      g_string_append (string, " ");
    }

  self->transform_class->print (self, string);
}

/**
 * gsk_transform_to_string:
 * @self: (allow-none): a #GskTransform
 *
 * Converts a matrix into a string that is suitable for
 * printing and can later be parsed with gsk_transform_parse().
 *
 * This is a wrapper around gsk_transform_print(), see that function
 * for details.
 *
 * Returns: A new string for @self
 **/
char *
gsk_transform_to_string (GskTransform *self)
{
  GString *string;

  string = g_string_new ("");

  gsk_transform_print (self, string);

  return g_string_free (string, FALSE);
}

/**
 * gsk_transform_get_transform_type:
 * @self: (allow-none): a #GskTransform
 *
 * Returns the type of the @self.
 *
 * Returns: the type of the #GskTransform
 */
GskTransformType
gsk_transform_get_transform_type (GskTransform *self)
{
  if (self == NULL)
    return GSK_TRANSFORM_TYPE_IDENTITY;

  return self->transform_class->transform_type;
}

/**
 * gsk_transform_get_next:
 * @self: (allow-none): a #GskTransform
 *
 * Gets the rest of the matrix in the chain of operations.
 *
 * Returns: (transfer none) (nullable): The next transform or
 *     %NULL if this was the last operation.
 **/
GskTransform *
gsk_transform_get_next (GskTransform *self)
{
  if (self == NULL)
    return NULL;

  return self->next;
}

/**
 * gsk_transform_to_matrix:
 * @self: (allow-none): a #GskTransform
 * @out_matrix: (out caller-allocates): The matrix to set
 *
 * Computes the actual value of @self and stores it in @out_matrix.
 * The previous value of @out_matrix will be ignored.
 **/
void
gsk_transform_to_matrix (GskTransform      *self,
                         graphene_matrix_t *out_matrix)
{
  graphene_matrix_t m;

  if (self == NULL)
    {
      graphene_matrix_init_identity (out_matrix);
      return;
    }

  gsk_transform_to_matrix (self->next, out_matrix);
  self->transform_class->to_matrix (self, &m);
  graphene_matrix_multiply (&m, out_matrix, out_matrix);
}

/**
 * gsk_transform_to_2d:
 * @m: a #GskTransform
 * @out_xx: (out): return location for the xx member
 * @out_yx: (out): return location for the yx member
 * @out_xy: (out): return location for the xy member
 * @out_yy: (out): return location for the yy member
 * @out_dx: (out): return location for the x0 member
 * @out_dy: (out): return location for the y0 member
 *
 * Converts a #GskTransform to a 2D transformation
 * matrix, if the given matrix is compatible.
 *
 * The returned values have the following layout:
 *
 * |[<!-- language="plain" -->
 *   | xx yx |   |  a  b  0 |
 *   | xy yy | = |  c  d  0 |
 *   | x0 y0 |   | tx ty  1 |
 * ]|
 *
 * This function can be used to convert between a #GskTransform
 * and a matrix type from other 2D drawing libraries, in particular
 * Cairo.
 *
 * Returns: %TRUE if the matrix is compatible with an 2D
 *   transformation matrix.
 */
gboolean
gsk_transform_to_2d (GskTransform *self,
                     float        *out_xx,
                     float        *out_yx,
                     float        *out_xy,
                     float        *out_yy,
                     float        *out_dx,
                     float        *out_dy)
{
  if (self == NULL)
    {
      *out_xx = 1.0f;
      *out_yx = 0.0f;
      *out_xy = 0.0f;
      *out_yy = 1.0f;
      *out_dx = 0.0f;
      *out_dy = 0.0f;
      return TRUE;
    }

  if (!gsk_transform_to_2d (self->next,
                            out_xx, out_yx,
                            out_xy, out_yy,
                            out_dx, out_dy))
    return FALSE;

  return self->transform_class->apply_2d (self,
                                          out_xx, out_yx,
                                          out_xy, out_yy,
                                          out_dx, out_dy);
}

/**
 * gsk_transform_to_affine:
 * @m: a #GskTransform
 * @out_scale_x: (out): return location for the scale
 *     factor in the x direction
 * @out_scale_y: (out): return location for the scale
 *     factor in the y direction
 * @out_dx: (out): return location for the translation
 *     in the x direction
 * @out_dy: (out): return location for the translation
 *     in the y direction
 *
 * Converts a #GskTransform to 2D affine transformation
 * factors, if the given matrix is compatible.
 *
 * Returns: %TRUE if the matrix is compatible with an 2D
 *   affine transformation.
 */
gboolean
gsk_transform_to_affine (GskTransform *self,
                         float        *out_scale_x,
                         float        *out_scale_y,
                         float        *out_dx,
                         float        *out_dy)
{
  if (self == NULL)
    {
      *out_scale_x = 1.0f;
      *out_scale_y = 1.0f;
      *out_dx = 0.0f;
      *out_dy = 0.0f;
      return TRUE;
    }

  if (!gsk_transform_to_affine (self->next,
                                out_scale_x, out_scale_y,
                                out_dx, out_dy))
    return FALSE;

  return self->transform_class->apply_affine (self,
                                              out_scale_x, out_scale_y,
                                              out_dx, out_dy);
}

/**
 * gsk_transform_to_translate:
 * @m: a #GskTransform
 * @out_dx: (out): return location for the translation
 *     in the x direction
 * @out_dy: (out): return location for the translation
 *     in the y direction
 *
 * Converts a #GskTransform to a translation operation,
 * if the given matrix is compatible.
 *
 * Returns: %TRUE if the matrix is compatible with a
 *   translation transformation.
 */
gboolean
gsk_transform_to_translate (GskTransform *self,
                            float        *out_dx,
                            float        *out_dy)
{
  if (self == NULL)
    {
      *out_dx = 0.0f;
      *out_dy = 0.0f;
      return TRUE;
    }

  if (!gsk_transform_to_translate (self->next,
                                   out_dx, out_dy))
    return FALSE;

  return self->transform_class->apply_translate (self,
                                                 out_dx, out_dy);
}

/**
 * gsk_transform_transform:
 * @next: (allow-none) (transfer full): Transform to apply @other to
 * @other: (allow-none):  Transform to apply
 *
 * Applies all the operations from @other to @next. 
 *
 * Returns: The new matrix
 **/
GskTransform *
gsk_transform_transform (GskTransform *next,
                         GskTransform *other)
{
  if (other == NULL)
    return next;

  next = gsk_transform_transform (next, other->next);
  return other->transform_class->apply (other, next);
}

/**
 * gsk_transform_equal:
 * @first: the first matrix
 * @second: the second matrix
 *
 * Checks two matrices for equality. Note that matrices need to be literally
 * identical in their operations, it is not enough that they return the
 * same result in gsk_transform_to_matrix().
 *
 * Returns: %TRUE if the two matrices can be proven to be equal
 **/
gboolean
gsk_transform_equal (GskTransform *first,
                     GskTransform *second)
{
  if (first == second)
    return TRUE;

  if (first == NULL || second == NULL)
    return FALSE;

  if (!gsk_transform_equal (first->next, second->next))
    return FALSE;

  if (first->transform_class != second->transform_class)
    return FALSE;

  return first->transform_class->equal (first, second);
}

/*<private>
 * gsk_transform_categorize:
 * @self: (allow-none): A matrix
 *
 *
 *
 * Returns: The category this matrix belongs to
 **/
GskMatrixCategory
gsk_transform_categorize (GskTransform *self)
{
  if (self == NULL)
    return GSK_MATRIX_CATEGORY_IDENTITY;

  return MIN (gsk_transform_categorize (self->next),
              self->transform_class->categorize (self));
}

/*
 * gsk_transform_new: (constructor):
 *
 * Creates a new identity matrix. This function is meant to be used by language
 * bindings. For C code, this equivalent to using %NULL.
 *
 * See also gsk_transform_identity() for inserting identity matrix operations
 * when constructing matrices.
 *
 * Returns: A new identity matrix
 **/
GskTransform *
gsk_transform_new (void)
{
  return gsk_transform_alloc (&GSK_IDENTITY_TRANSFORM_CLASS, NULL);
}

/**
 * gsk_transform_transform_bounds:
 * @self: a #GskTransform
 * @rect: a #graphene_rect_t
 * @out_rect: (out caller-allocates): return location for the bounds
 *   of the transformed rectangle
 *
 * Transforms a #graphene_rect_t using the given matrix @m. The
 * result is the bounding box containing the coplanar quad.
 **/
void
gsk_transform_transform_bounds (GskTransform          *self,
                                const graphene_rect_t *rect,
                                graphene_rect_t       *out_rect)
{
  graphene_matrix_t mat;

  /* XXX: vfuncify */
  gsk_transform_to_matrix (self, &mat);
  graphene_matrix_transform_bounds (&mat, rect, out_rect);
}
