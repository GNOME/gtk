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
 * SECTION:gtktransform
 * @Title: GtkTransform
 * @Short_description: A description for transform operations
 *
 * #GtkTransform is an object to describe transform matrices. Unlike
 * #graphene_matrix_t, #GtkTransform retains the steps in how a transform was
 * constructed, and allows inspecting them. It is modeled after the way
 * CSS describes transforms.
 *
 * #GtkTransform objects are immutable and cannot be changed after creation.
 * This means code can safely expose them as properties of objects without
 * having to worry about others changing them.
 */

#include "config.h"

#include "gtktransformprivate.h"

typedef struct _GtkTransformClass GtkTransformClass;

#define GTK_IS_TRANSFORM_TYPE(self,type) ((self) == NULL ? (type) == GTK_TRANSFORM_TYPE_IDENTITY : (self)->transform_class->transform_type == (type))

struct _GtkTransform
{
  const GtkTransformClass *transform_class;
  
  volatile int ref_count;
  GtkTransform *next;
};

struct _GtkTransformClass
{
  GtkTransformType transform_type;
  gsize struct_size;
  const char *type_name;

  void                  (* finalize)            (GtkTransform           *transform);
  GskMatrixCategory     (* categorize)          (GtkTransform           *transform);
  void                  (* to_matrix)           (GtkTransform           *transform,
                                                 graphene_matrix_t      *out_matrix);
  gboolean              (* apply_affine)        (GtkTransform           *transform,
                                                 float                  *out_scale_x,
                                                 float                  *out_scale_y,
                                                 float                  *out_dx,
                                                 float                  *out_dy);
  void                  (* print)               (GtkTransform           *transform,
                                                 GString                *string);
  GtkTransform *        (* apply)               (GtkTransform           *transform,
                                                 GtkTransform           *apply_to);
  /* both matrices have the same type */
  gboolean              (* equal)               (GtkTransform           *first_transform,
                                                 GtkTransform           *second_transform);
};

/**
 * GtkTransform: (ref-func gtk_transform_ref) (unref-func gtk_transform_unref)
 *
 * The `GtkTransform` structure contains only private data.
 */

G_DEFINE_BOXED_TYPE (GtkTransform, gtk_transform,
                     gtk_transform_ref,
                     gtk_transform_unref)

/*<private>
 * gtk_transform_is_identity:
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
gtk_transform_is_identity (GtkTransform *self)
{
  return self == NULL ||
         (GTK_IS_TRANSFORM_TYPE (self, GTK_TRANSFORM_TYPE_IDENTITY) && gtk_transform_is_identity (self->next));
}

/*< private >
 * gtk_transform_alloc:
 * @transform_class: class structure for this self
 * @next: (transfer full) Next matrix to multiply with or %NULL if none
 *
 * Returns: (transfer full): the newly created #GtkTransform
 */
static gpointer
gtk_transform_alloc (const GtkTransformClass *transform_class,
                     GtkTransform            *next)
{
  GtkTransform *self;

  g_return_val_if_fail (transform_class != NULL, NULL);

  self = g_malloc0 (transform_class->struct_size);

  self->transform_class = transform_class;
  self->ref_count = 1;
  self->next = gtk_transform_is_identity (next) ? NULL : next;

  return self;
}

/*** IDENTITY ***/

static void
gtk_identity_transform_finalize (GtkTransform *transform)
{
}

static GskMatrixCategory
gtk_identity_transform_categorize (GtkTransform *transform)
{
  return GSK_MATRIX_CATEGORY_IDENTITY;
}

static void
gtk_identity_transform_to_matrix (GtkTransform      *transform,
                                  graphene_matrix_t *out_matrix)
{
  graphene_matrix_init_identity (out_matrix);
}

static gboolean
gtk_identity_transform_apply_affine (GtkTransform *transform,
                                     float        *out_scale_x,
                                     float        *out_scale_y,
                                     float        *out_dx,
                                     float        *out_dy)
{
  return TRUE;
}

static void
gtk_identity_transform_print (GtkTransform *transform,
                              GString      *string)
{
  g_string_append (string, "identity");
}

static GtkTransform *
gtk_identity_transform_apply (GtkTransform *transform,
                              GtkTransform *apply_to)
{
  return gtk_transform_identity (apply_to);
}

static gboolean
gtk_identity_transform_equal (GtkTransform *first_transform,
                              GtkTransform *second_transform)
{
  return TRUE;
}

static const GtkTransformClass GTK_IDENTITY_TRANSFORM_CLASS =
{
  GTK_TRANSFORM_TYPE_IDENTITY,
  sizeof (GtkTransform),
  "GtkIdentityMatrix",
  gtk_identity_transform_finalize,
  gtk_identity_transform_categorize,
  gtk_identity_transform_to_matrix,
  gtk_identity_transform_apply_affine,
  gtk_identity_transform_print,
  gtk_identity_transform_apply,
  gtk_identity_transform_equal,
};

/**
 * gtk_transform_identity:
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
GtkTransform *
gtk_transform_identity (GtkTransform *next)
{
  if (gtk_transform_is_identity (next))
    return next;

  return gtk_transform_alloc (&GTK_IDENTITY_TRANSFORM_CLASS, next);
}

/*** MATRIX ***/

typedef struct _GtkMatrixTransform GtkMatrixTransform;

struct _GtkMatrixTransform
{
  GtkTransform parent;

  graphene_matrix_t matrix;
  GskMatrixCategory category;
};

static void
gtk_matrix_transform_finalize (GtkTransform *self)
{
}

static GskMatrixCategory
gtk_matrix_transform_categorize (GtkTransform *transform)
{
  GtkMatrixTransform *self = (GtkMatrixTransform *) transform;

  return self->category;
}

static void
gtk_matrix_transform_to_matrix (GtkTransform      *transform,
                                graphene_matrix_t *out_matrix)
{
  GtkMatrixTransform *self = (GtkMatrixTransform *) transform;

  graphene_matrix_init_from_matrix (out_matrix, &self->matrix);
}

static gboolean
gtk_matrix_transform_apply_affine (GtkTransform *transform,
                                   float        *out_scale_x,
                                   float        *out_scale_y,
                                   float        *out_dx,
                                   float        *out_dy)
{
  GtkMatrixTransform *self = (GtkMatrixTransform *) transform;

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

static void
string_append_double (GString *string,
                      double   d)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_formatd (buf, G_ASCII_DTOSTR_BUF_SIZE, "%g", d);
  g_string_append (string, buf);
}

static void
gtk_matrix_transform_print (GtkTransform *transform,
                            GString      *string)
{
  GtkMatrixTransform *self = (GtkMatrixTransform *) transform;
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

static GtkTransform *
gtk_matrix_transform_apply (GtkTransform *transform,
                            GtkTransform *apply_to)
{
  GtkMatrixTransform *self = (GtkMatrixTransform *) transform;

  return gtk_transform_matrix_with_category (apply_to,
                                             &self->matrix,
                                             self->category);
}

static gboolean
gtk_matrix_transform_equal (GtkTransform *first_transform,
                            GtkTransform *second_transform)
{
  GtkMatrixTransform *first = (GtkMatrixTransform *) first_transform;
  GtkMatrixTransform *second = (GtkMatrixTransform *) second_transform;

  /* Crude, but better than just returning FALSE */
  return memcmp (&first->matrix, &second->matrix, sizeof (graphene_matrix_t)) == 0;
}

static const GtkTransformClass GTK_TRANSFORM_TRANSFORM_CLASS =
{
  GTK_TRANSFORM_TYPE_TRANSFORM,
  sizeof (GtkMatrixTransform),
  "GtkMatrixTransform",
  gtk_matrix_transform_finalize,
  gtk_matrix_transform_categorize,
  gtk_matrix_transform_to_matrix,
  gtk_matrix_transform_apply_affine,
  gtk_matrix_transform_print,
  gtk_matrix_transform_apply,
  gtk_matrix_transform_equal,
};

GtkTransform *
gtk_transform_matrix_with_category (GtkTransform            *next,
                                    const graphene_matrix_t *matrix,
                                    GskMatrixCategory        category)
{
  GtkMatrixTransform *result = gtk_transform_alloc (&GTK_TRANSFORM_TRANSFORM_CLASS, next);

  graphene_matrix_init_from_matrix (&result->matrix, matrix);
  result->category = category;

  return &result->parent;
}

/**
 * gtk_transform_matrix:
 * @next: (allow-none): the next transform
 * @matrix: the matrix to multiply @next with
 *
 * Multiplies @next with the given @matrix.
 *
 * Returns: The new matrix
 **/
GtkTransform *
gtk_transform_matrix (GtkTransform            *next,
                      const graphene_matrix_t *matrix)
{
  return gtk_transform_matrix_with_category (next, matrix, GSK_MATRIX_CATEGORY_UNKNOWN);
}

/*** TRANSLATE ***/

typedef struct _GtkTranslateTransform GtkTranslateTransform;

struct _GtkTranslateTransform
{
  GtkTransform parent;

  graphene_point3d_t point;
};

static void
gtk_translate_transform_finalize (GtkTransform *self)
{
}

static GskMatrixCategory
gtk_translate_transform_categorize (GtkTransform *transform)
{
  GtkTranslateTransform *self = (GtkTranslateTransform *) transform;

  if (self->point.z != 0.0)
    return GSK_MATRIX_CATEGORY_INVERTIBLE;

  return GSK_MATRIX_CATEGORY_2D_TRANSLATE;
}

static void
gtk_translate_transform_to_matrix (GtkTransform      *transform,
                                   graphene_matrix_t *out_matrix)
{
  GtkTranslateTransform *self = (GtkTranslateTransform *) transform;

  graphene_matrix_init_translate (out_matrix, &self->point);
}

static gboolean
gtk_translate_transform_apply_affine (GtkTransform *transform,
                                      float        *out_scale_x,
                                      float        *out_scale_y,
                                      float        *out_dx,
                                      float        *out_dy)
{
  GtkTranslateTransform *self = (GtkTranslateTransform *) transform;

  if (self->point.z != 0.0)
    return FALSE;

  *out_dx += *out_scale_x * self->point.x;
  *out_dy += *out_scale_y * self->point.y;

  return TRUE;
}

static GtkTransform *
gtk_translate_transform_apply (GtkTransform *transform,
                               GtkTransform *apply_to)
{
  GtkTranslateTransform *self = (GtkTranslateTransform *) transform;

  return gtk_transform_translate_3d (apply_to, &self->point);
}

static gboolean
gtk_translate_transform_equal (GtkTransform *first_transform,
                               GtkTransform *second_transform)
{
  GtkTranslateTransform *first = (GtkTranslateTransform *) first_transform;
  GtkTranslateTransform *second = (GtkTranslateTransform *) second_transform;

  return graphene_point3d_equal (&first->point, &second->point);
}

static void
gtk_translate_transform_print (GtkTransform *transform,
                               GString      *string)
{
  GtkTranslateTransform *self = (GtkTranslateTransform *) transform;

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

static const GtkTransformClass GTK_TRANSLATE_TRANSFORM_CLASS =
{
  GTK_TRANSFORM_TYPE_TRANSLATE,
  sizeof (GtkTranslateTransform),
  "GtkTranslateTransform",
  gtk_translate_transform_finalize,
  gtk_translate_transform_categorize,
  gtk_translate_transform_to_matrix,
  gtk_translate_transform_apply_affine,
  gtk_translate_transform_print,
  gtk_translate_transform_apply,
  gtk_translate_transform_equal,
};

/**
 * gtk_transform_translate:
 * @next: (allow-none): the next transform
 * @point: the point to translate the matrix by
 *
 * Translates @next in 2dimensional space by @point.
 *
 * Returns: The new matrix
 **/
GtkTransform *
gtk_transform_translate (GtkTransform           *next,
                         const graphene_point_t *point)
{
  graphene_point3d_t point3d;

  graphene_point3d_init (&point3d, point->x, point->y, 0);

  return gtk_transform_translate_3d (next, &point3d);
}

/**
 * gtk_transform_translate_3d:
 * @next: (allow-none): the next transform
 * @point: the point to translate the matrix by
 *
 * Translates @next by @point.
 *
 * Returns: The new matrix
 **/
GtkTransform *
gtk_transform_translate_3d (GtkTransform             *next,
                            const graphene_point3d_t *point)
{
  GtkTranslateTransform *result = gtk_transform_alloc (&GTK_TRANSLATE_TRANSFORM_CLASS, next);

  graphene_point3d_init_from_point (&result->point, point);

  return &result->parent;
}

/*** ROTATE ***/

typedef struct _GtkRotateTransform GtkRotateTransform;

struct _GtkRotateTransform
{
  GtkTransform parent;

  float angle;
  graphene_vec3_t axis;
};

static void
gtk_rotate_transform_finalize (GtkTransform *self)
{
}

static GskMatrixCategory
gtk_rotate_transform_categorize (GtkTransform *transform)
{
  return GSK_MATRIX_CATEGORY_INVERTIBLE;
}

static void
gtk_rotate_transform_to_matrix (GtkTransform      *transform,
                                graphene_matrix_t *out_matrix)
{
  GtkRotateTransform *self = (GtkRotateTransform *) transform;

  graphene_matrix_init_rotate (out_matrix, self->angle, &self->axis);
}

static gboolean
gtk_rotate_transform_apply_affine (GtkTransform *transform,
                                   float        *out_scale_x,
                                   float        *out_scale_y,
                                   float        *out_dx,
                                   float        *out_dy)
{
  return FALSE;
}

static GtkTransform *
gtk_rotate_transform_apply (GtkTransform *transform,
                            GtkTransform *apply_to)
{
  GtkRotateTransform *self = (GtkRotateTransform *) transform;

  return gtk_transform_rotate_3d (apply_to, self->angle, &self->axis);
}

static gboolean
gtk_rotate_transform_equal (GtkTransform *first_transform,
                            GtkTransform *second_transform)
{
  GtkRotateTransform *first = (GtkRotateTransform *) first_transform;
  GtkRotateTransform *second = (GtkRotateTransform *) second_transform;

  return first->angle == second->angle
         && graphene_vec3_equal (&first->axis, &second->axis);
}

static void
gtk_rotate_transform_print (GtkTransform *transform,
                            GString      *string)
{
  GtkRotateTransform *self = (GtkRotateTransform *) transform;
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

static const GtkTransformClass GTK_ROTATE_TRANSFORM_CLASS =
{
  GTK_TRANSFORM_TYPE_ROTATE,
  sizeof (GtkRotateTransform),
  "GtkRotateTransform",
  gtk_rotate_transform_finalize,
  gtk_rotate_transform_categorize,
  gtk_rotate_transform_to_matrix,
  gtk_rotate_transform_apply_affine,
  gtk_rotate_transform_print,
  gtk_rotate_transform_apply,
  gtk_rotate_transform_equal,
};

/**
 * gtk_transform_rotate:
 * @next: (allow-none): the next transform
 * @angle: the rotation angle, in degrees (clockwise)
 *
 * Rotates @next @angle degrees in 2D - or in 3Dspeak, around the z axis.
 *
 * Returns: The new matrix
 **/
GtkTransform *
gtk_transform_rotate (GtkTransform *next,
                      float         angle)
{
  return gtk_transform_rotate_3d (next, angle, graphene_vec3_z_axis ());
}

/**
 * gtk_transform_rotate_3d:
 * @next: (allow-none): the next transform
 * @angle: the rotation angle, in degrees (clockwise)
 * @axis: The rotation axis
 *
 * Rotates @next @angle degrees around @axis.
 *
 * For a rotation in 2D space, use gtk_transform_rotate().
 *
 * Returns: The new matrix
 **/
GtkTransform *
gtk_transform_rotate_3d (GtkTransform          *next,
                         float                  angle,
                         const graphene_vec3_t *axis)
{
  GtkRotateTransform *result = gtk_transform_alloc (&GTK_ROTATE_TRANSFORM_CLASS, next);

  result->angle = angle;
  graphene_vec3_init_from_vec3 (&result->axis, axis);

  return &result->parent;
}

/*** SCALE ***/

typedef struct _GtkScaleTransform GtkScaleTransform;

struct _GtkScaleTransform
{
  GtkTransform parent;

  float factor_x;
  float factor_y;
  float factor_z;
};

static void
gtk_scale_transform_finalize (GtkTransform *self)
{
}

static GskMatrixCategory
gtk_scale_transform_categorize (GtkTransform *transform)
{
  GtkScaleTransform *self = (GtkScaleTransform *) transform;

  if (self->factor_z != 1.0)
    return GSK_MATRIX_CATEGORY_INVERTIBLE;

  return GSK_MATRIX_CATEGORY_2D_AFFINE;
}

static void
gtk_scale_transform_to_matrix (GtkTransform      *transform,
                               graphene_matrix_t *out_matrix)
{
  GtkScaleTransform *self = (GtkScaleTransform *) transform;

  graphene_matrix_init_scale (out_matrix, self->factor_x, self->factor_y, self->factor_z);
}

static gboolean
gtk_scale_transform_apply_affine (GtkTransform *transform,
                                  float        *out_scale_x,
                                  float        *out_scale_y,
                                  float        *out_dx,
                                  float        *out_dy)
{
  GtkScaleTransform *self = (GtkScaleTransform *) transform;

  if (self->factor_z != 1.0)
    return FALSE;

  *out_scale_x *= self->factor_x;
  *out_scale_y *= self->factor_y;

  return TRUE;
}

static GtkTransform *
gtk_scale_transform_apply (GtkTransform *transform,
                           GtkTransform *apply_to)
{
  GtkScaleTransform *self = (GtkScaleTransform *) transform;

  return gtk_transform_scale_3d (apply_to, self->factor_x, self->factor_y, self->factor_z);
}

static gboolean
gtk_scale_transform_equal (GtkTransform *first_transform,
                           GtkTransform *second_transform)
{
  GtkScaleTransform *first = (GtkScaleTransform *) first_transform;
  GtkScaleTransform *second = (GtkScaleTransform *) second_transform;

  return first->factor_x == second->factor_x
         && first->factor_y == second->factor_y
         && first->factor_z == second->factor_z;
}

static void
gtk_scale_transform_print (GtkTransform *transform,
                           GString      *string)
{
  GtkScaleTransform *self = (GtkScaleTransform *) transform;

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

static const GtkTransformClass GTK_SCALE_TRANSFORM_CLASS =
{
  GTK_TRANSFORM_TYPE_SCALE,
  sizeof (GtkScaleTransform),
  "GtkScaleTransform",
  gtk_scale_transform_finalize,
  gtk_scale_transform_categorize,
  gtk_scale_transform_to_matrix,
  gtk_scale_transform_apply_affine,
  gtk_scale_transform_print,
  gtk_scale_transform_apply,
  gtk_scale_transform_equal,
};

/**
 * gtk_transform_scale:
 * @next: (allow-none): the next transform
 * @factor_x: scaling factor on the X axis
 * @factor_y: scaling factor on the Y axis
 *
 * Scales @next in 2-dimensional space by the given factors.
 * Use gtk_transform_scale_3d() to scale in all 3 dimensions.
 *
 * Returns: The new matrix
 **/
GtkTransform *
gtk_transform_scale (GtkTransform *next,
                     float         factor_x,
                     float         factor_y)
{
  return gtk_transform_scale_3d (next, factor_x, factor_y, 1.0);
}

/**
 * gtk_transform_scale_3d:
 * @next: (allow-none): the next transform
 * @factor_x: scaling factor on the X axis
 * @factor_y: scaling factor on the Y axis
 * @factor_z: scaling factor on the Z axis
 *
 * Scales @next by the given factors.
 *
 * Returns: The new matrix
 **/
GtkTransform *
gtk_transform_scale_3d (GtkTransform *next,
                        float         factor_x,
                        float         factor_y,
                        float         factor_z)
{
  GtkScaleTransform *result = gtk_transform_alloc (&GTK_SCALE_TRANSFORM_CLASS, next);

  result->factor_x = factor_x;
  result->factor_y = factor_y;
  result->factor_z = factor_z;

  return &result->parent;
}

/*** PUBLIC API ***/

static void
gtk_transform_finalize (GtkTransform *self)
{
  self->transform_class->finalize (self);

  gtk_transform_unref (self->next);

  g_free (self);
}

/**
 * gtk_transform_ref:
 * @self: (allow-none): a #GtkTransform
 *
 * Acquires a reference on the given #GtkTransform.
 *
 * Returns: (transfer none): the #GtkTransform with an additional reference
 */
GtkTransform *
gtk_transform_ref (GtkTransform *self)
{
  if (self == NULL)
    return NULL;

  g_atomic_int_inc (&self->ref_count);

  return self;
}

/**
 * gtk_transform_unref:
 * @self: (allow-none): a #GtkTransform
 *
 * Releases a reference on the given #GtkTransform.
 *
 * If the reference was the last, the resources associated to the @self are
 * freed.
 */
void
gtk_transform_unref (GtkTransform *self)
{
  if (self == NULL)
    return;

  if (g_atomic_int_dec_and_test (&self->ref_count))
    gtk_transform_finalize (self);
}

/**
 * gtk_transform_print:
 * @self: (allow-none): a #GtkTransform
 * @string:  The string to print into
 *
 * Converts @self into a string representation suitable for printing that
 * can later be parsed with gtk_transform_parse().
 **/
void
gtk_transform_print (GtkTransform *self,
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
      gtk_transform_print (self->next, string);
      g_string_append (string, " ");
    }

  self->transform_class->print (self, string);
}

/**
 * gtk_transform_to_string:
 * @self: (allow-none): a #GtkTransform
 *
 * Converts a matrix into a string that is suitable for
 * printing and can later be parsed with gtk_transform_parse().
 *
 * This is a wrapper around gtk_transform_print(), see that function
 * for details.
 *
 * Returns: A new string for @self
 **/
char *
gtk_transform_to_string (GtkTransform *self)
{
  GString *string;

  string = g_string_new ("");

  gtk_transform_print (self, string);

  return g_string_free (string, FALSE);
}

/**
 * gtk_transform_get_transform_type:
 * @self: (allow-none): a #GtkTransform
 *
 * Returns the type of the @self.
 *
 * Returns: the type of the #GtkTransform
 */
GtkTransformType
gtk_transform_get_transform_type (GtkTransform *self)
{
  if (self == NULL)
    return GTK_TRANSFORM_TYPE_IDENTITY;

  return self->transform_class->transform_type;
}

/**
 * gtk_transform_get_next:
 * @self: (allow-none): a #GtkTransform
 *
 * Gets the rest of the matrix in the chain of operations.
 *
 * Returns: (transfer none) (nullable): The next transform or
 *     %NULL if this was the last operation.
 **/
GtkTransform *
gtk_transform_get_next (GtkTransform *self)
{
  if (self == NULL)
    return NULL;

  return self->next;
}

/**
 * gtk_transform_to_matrix:
 * @self: (allow-none): a #GtkTransform
 * @out_matrix: (out caller-allocates): The matrix to set
 *
 * Computes the actual value of @self and stores it in @out_matrix.
 * The previous value of @out_matrix will be ignored.
 **/
void
gtk_transform_to_matrix (GtkTransform      *self,
                         graphene_matrix_t *out_matrix)
{
  graphene_matrix_t m;

  if (self == NULL)
    {
      graphene_matrix_init_identity (out_matrix);
      return;
    }

  gtk_transform_to_matrix (self->next, out_matrix);
  self->transform_class->to_matrix (self, &m);
  graphene_matrix_multiply (&m, out_matrix, out_matrix);
}

gboolean
gtk_transform_to_affine (GtkTransform *self,
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

  if (!gtk_transform_to_affine (self->next,
                                out_scale_x, out_scale_y,
                                out_dx, out_dy))
    return FALSE;

  return self->transform_class->apply_affine (self,
                                              out_scale_x, out_scale_y,
                                              out_dx, out_dy);
}

/**
 * gtk_transform_transform:
 * @next: (allow-none) (transfer full): Transform to apply @other to
 * @other: (allow-none):  Transform to apply
 *
 * Applies all the operations from @other to @next. 
 *
 * Returns: The new matrix
 **/
GtkTransform *
gtk_transform_transform (GtkTransform *next,
                         GtkTransform *other)
{
  if (other == NULL)
    return next;

  next = gtk_transform_transform (next, other->next);
  return other->transform_class->apply (other, next);
}

/**
 * gtk_transform_equal:
 * @first: the first matrix
 * @second: the second matrix
 *
 * Checks two matrices for equality. Note that matrices need to be literally
 * identical in their operations, it is not enough that they return the
 * same result in gtk_transform_to_matrix().
 *
 * Returns: %TRUE if the two matrices can be proven to be equal
 **/
gboolean
gtk_transform_equal (GtkTransform *first,
                     GtkTransform *second)
{
  if (first == second)
    return TRUE;

  if (first == NULL || second == NULL)
    return FALSE;

  if (!gtk_transform_equal (first->next, second->next))
    return FALSE;

  if (first->transform_class != second->transform_class)
    return FALSE;

  return first->transform_class->equal (first, second);
}

/*<private>
 * gtk_transform_categorize:
 * @self: (allow-none): A matrix
 *
 *
 *
 * Returns: The category this matrix belongs to
 **/
GskMatrixCategory
gtk_transform_categorize (GtkTransform *self)
{
  if (self == NULL)
    return GSK_MATRIX_CATEGORY_IDENTITY;

  return MIN (gtk_transform_categorize (self->next),
              self->transform_class->categorize (self));
}

/*
 * gtk_transform_new: (constructor):
 *
 * Creates a new identity matrix. This function is meant to be used by language
 * bindings. For C code, this equivalent to using %NULL.
 *
 * See also gtk_transform_identity() for inserting identity matrix operations
 * when constructing matrices.
 *
 * Returns: A new identity matrix
 **/
GtkTransform *
gtk_transform_new (void)
{
  return gtk_transform_alloc (&GTK_IDENTITY_TRANSFORM_CLASS, NULL);
}
