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
 * SECTION:GtkMatrix
 * @Title: GtkMatrix
 * @Short_description: A matrix description
 *
 * #GtkMatrix is an object to describe matrices. Unlike #graphene_matrix_t,
 * #GtkMatrix retains the steps in how a matrix was constructed, and allows
 * inspecting them. It is modeled after the way CSS describes matrices.
 *
 * #GtkMatrix objects are immutable and cannot be changed after creation.
 * This means code can safely expose them as properties of objects without
 * having to worry about others changing them.
 */

#include "config.h"

#include "gtkmatrixprivate.h"

typedef struct _GtkMatrixClass GtkMatrixClass;

#define GTK_IS_MATRIX_TYPE(self,type) (GTK_IS_MATRIX (self) && (self)->matrix_class->matrix_type == (type))

struct _GtkMatrix
{
  const GtkMatrixClass *matrix_class;
  
  volatile int ref_count;
  GtkMatrix *next;
};

struct _GtkMatrixClass
{
  GtkMatrixType matrix_type;
  gsize struct_size;
  const char *type_name;

  void                  (* finalize)            (GtkMatrix              *matrix);
  GskMatrixCategory     (* categorize)          (GtkMatrix              *matrix);
  void                  (* compute)             (GtkMatrix              *matrix,
                                                 graphene_matrix_t      *out_matrix);
  void                  (* print)               (GtkMatrix              *matrix,
                                                 GString                *string);
  /* both matrices have the same type */
  gboolean              (* equal)               (GtkMatrix              *first_matrix,
                                                 GtkMatrix              *second_matrix);
  /* end is either NULL (to transition to identity) or same type as end */
  GtkMatrix *           (* transition)          (GtkMatrix              *start_matrix,
                                                 GtkMatrix              *end_matrix,
                                                 double                  progress,
                                                 GtkMatrix              *next);
};

/**
 * GtkMatrix: (ref-func gtk_matrix_ref) (unref-func gtk_matrix_unref)
 *
 * The `GtkMatrix` structure contains only private data.
 */

G_DEFINE_BOXED_TYPE (GtkMatrix, gtk_matrix,
                     gtk_matrix_ref,
                     gtk_matrix_unref)

/*< private >
 * gtk_matrix_new:
 * @matrix_class: class structure for this self
 * @next: (transfer full) Next matrix to multiply with or %NULL if none
 *
 * Returns: (transfer full): the newly created #GtkMatrix
 */
static gpointer
gtk_matrix_new (const GtkMatrixClass *matrix_class,
                GtkMatrix            *next)
{
  GtkMatrix *self;

  g_return_val_if_fail (matrix_class != NULL, NULL);

  self = g_malloc0 (matrix_class->struct_size);

  self->matrix_class = matrix_class;
  self->ref_count = 1;
  self->next = next;

  return self;
}

/*** IDENTITY ***/

static void
gtk_identity_matrix_finalize (GtkMatrix *matrix)
{
}

static GskMatrixCategory
gtk_identity_matrix_categorize (GtkMatrix *matrix)
{
  return GSK_MATRIX_CATEGORY_IDENTITY;
}

static void
gtk_identity_matrix_compute (GtkMatrix         *matrix,
                             graphene_matrix_t *out_matrix)
{
  graphene_matrix_init_identity (out_matrix);
}

static void
gtk_identity_matrix_print (GtkMatrix *matrix,
                           GString   *string)
{
  g_string_append (string, "identity");
}

static gboolean
gtk_identity_matrix_equal (GtkMatrix *first_matrix,
                           GtkMatrix *second_matrix)
{
  return TRUE;
}

static GtkMatrix *
gtk_identity_matrix_transition (GtkMatrix *start_matrix,
                                GtkMatrix *end_matrix,
                                double     progress,
                                GtkMatrix *next)
{
  return gtk_matrix_identity (next);
}

static const GtkMatrixClass GTK_IDENTITY_MATRIX_CLASS = {
  GTK_MATRIX_TYPE_IDENTITY,
  sizeof (GtkMatrix),
  "GtkIdentityMatrix",
  gtk_identity_matrix_finalize,
  gtk_identity_matrix_categorize,
  gtk_identity_matrix_compute,
  gtk_identity_matrix_print,
  gtk_identity_matrix_equal,
  gtk_identity_matrix_transition,
};

/**
 * gtk_matrix_identity:
 * @next: (allow-none): the next matrix operation or %NULL
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
GtkMatrix *
gtk_matrix_identity (GtkMatrix *next)
{
  if (next == NULL)
    return next;

  return gtk_matrix_new (&GTK_IDENTITY_MATRIX_CLASS, next);
}

/*** TRANSFORM ***/

typedef struct _GtkTransformMatrix GtkTransformMatrix;

struct _GtkTransformMatrix
{
  GtkMatrix parent;

  graphene_matrix_t matrix;
};

static void
gtk_transform_matrix_finalize (GtkMatrix *self)
{
}

static GskMatrixCategory
gtk_transform_matrix_categorize (GtkMatrix *matrix)
{
  return GSK_MATRIX_CATEGORY_UNKNOWN;
}

static void
gtk_transform_matrix_compute (GtkMatrix         *matrix,
                              graphene_matrix_t *out_matrix)
{
  GtkTransformMatrix *self = (GtkTransformMatrix *) matrix;

  graphene_matrix_init_from_matrix (out_matrix, &self->matrix);
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
gtk_transform_matrix_print (GtkMatrix *matrix,
                            GString   *string)
{
  GtkTransformMatrix *self = (GtkTransformMatrix *) matrix;
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

static gboolean
gtk_transform_matrix_equal (GtkMatrix *first_matrix,
                            GtkMatrix *second_matrix)
{
  GtkTransformMatrix *first = (GtkTransformMatrix *) first_matrix;
  GtkTransformMatrix *second = (GtkTransformMatrix *) second_matrix;

  /* Crude, but better than just returning FALSE */
  return memcmp (&first->matrix, &second->matrix, sizeof (graphene_matrix_t)) == 0;
}

static GtkMatrix *
gtk_transform_matrix_transition (GtkMatrix *start_matrix,
                                 GtkMatrix *end_matrix,
                                 double     progress,
                                 GtkMatrix *next)
{
  GtkTransformMatrix *start = (GtkTransformMatrix *) start_matrix;
  GtkTransformMatrix *end = (GtkTransformMatrix *) end_matrix;
  graphene_matrix_t result;
  
  if (end == NULL)
    {
      graphene_matrix_t identity;
      graphene_matrix_init_identity (&identity);
      graphene_matrix_interpolate (&start->matrix, &identity, progress, &result);
    }
  else
    {
      graphene_matrix_interpolate (&start->matrix, &end->matrix, progress, &result);
    }

  return gtk_matrix_transform (next, &result);
}

static const GtkMatrixClass GTK_TRANSFORM_MATRIX_CLASS = {
  GTK_MATRIX_TYPE_TRANSFORM,
  sizeof (GtkTransformMatrix),
  "GtkTransformMatrix",
  gtk_transform_matrix_finalize,
  gtk_transform_matrix_categorize,
  gtk_transform_matrix_compute,
  gtk_transform_matrix_print,
  gtk_transform_matrix_equal,
  gtk_transform_matrix_transition,
};

/**
 * gtk_matrix_transform:
 * @next: (allow-none): the next matrix
 * @matrix: the matrix to multiply @next with
 *
 * Multiplies @next with the given @matrix.
 *
 * Returns: The new matrix
 **/
GtkMatrix *
gtk_matrix_transform (GtkMatrix               *next,
                      const graphene_matrix_t *matrix)
{
  GtkTransformMatrix *result = gtk_matrix_new (&GTK_TRANSFORM_MATRIX_CLASS, next);

  graphene_matrix_init_from_matrix (&result->matrix, matrix);

  return &result->parent;
}

/*** TRANSLATE ***/

typedef struct _GtkTranslateMatrix GtkTranslateMatrix;

struct _GtkTranslateMatrix
{
  GtkMatrix parent;

  graphene_point3d_t point;
};

static void
gtk_translate_matrix_finalize (GtkMatrix *self)
{
}

static GskMatrixCategory
gtk_translate_matrix_categorize (GtkMatrix *matrix)
{
  GtkTranslateMatrix *self = (GtkTranslateMatrix *) matrix;

  if (self->point.z != 0.0)
    return GSK_MATRIX_CATEGORY_LINEAR;

  return GSK_MATRIX_CATEGORY_2D_TRANSLATE;
}

static void
gtk_translate_matrix_compute (GtkMatrix         *matrix,
                              graphene_matrix_t *out_matrix)
{
  GtkTranslateMatrix *self = (GtkTranslateMatrix *) matrix;

  graphene_matrix_init_translate (out_matrix, &self->point);
}

static gboolean
gtk_translate_matrix_equal (GtkMatrix *first_matrix,
                            GtkMatrix *second_matrix)
{
  GtkTranslateMatrix *first = (GtkTranslateMatrix *) first_matrix;
  GtkTranslateMatrix *second = (GtkTranslateMatrix *) second_matrix;

  return graphene_point3d_equal (&first->point, &second->point);
}

static void
gtk_translate_matrix_print (GtkMatrix *matrix,
                            GString   *string)
{
  GtkTranslateMatrix *self = (GtkTranslateMatrix *) matrix;

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

static GtkMatrix *
gtk_translate_matrix_transition (GtkMatrix *start_matrix,
                                 GtkMatrix *end_matrix,
                                 double     progress,
                                 GtkMatrix *next)
{
  GtkTranslateMatrix *start = (GtkTranslateMatrix *) start_matrix;
  GtkTranslateMatrix *end = (GtkTranslateMatrix *) end_matrix;
  graphene_point3d_t result;
  
  if (end == NULL)
    graphene_point3d_scale (&start->point, (1.0 - progress), &result);
  else
    graphene_point3d_interpolate (&start->point, &end->point, progress, &result);

  return gtk_matrix_translate_3d (next, &result);
}

static const GtkMatrixClass GTK_TRANSLATE_MATRIX_CLASS = {
  GTK_MATRIX_TYPE_TRANSLATE,
  sizeof (GtkTranslateMatrix),
  "GtkTranslateMatrix",
  gtk_translate_matrix_finalize,
  gtk_translate_matrix_categorize,
  gtk_translate_matrix_compute,
  gtk_translate_matrix_print,
  gtk_translate_matrix_equal,
  gtk_translate_matrix_transition,
};

/**
 * gtk_matrix_translate:
 * @next: (allow-none): the next matrix
 * @point: the point to translate the matrix by
 *
 * Translates @next in 2dimensional space by @point.
 *
 * Returns: The new matrix
 **/
GtkMatrix *
gtk_matrix_translate (GtkMatrix              *next,
                      const graphene_point_t *point)
{
  graphene_point3d_t point3d;

  graphene_point3d_init (&point3d, point->x, point->y, 0);

  return gtk_matrix_translate_3d (next, &point3d);
}

/**
 * gtk_matrix_translate_3d:
 * @next: (allow-none): the next matrix
 * @point: the point to translate the matrix by
 *
 * Translates @next by @point.
 *
 * Returns: The new matrix
 **/
GtkMatrix *
gtk_matrix_translate_3d (GtkMatrix                *next,
                         const graphene_point3d_t *point)
{
  GtkTranslateMatrix *result = gtk_matrix_new (&GTK_TRANSLATE_MATRIX_CLASS, next);

  graphene_point3d_init_from_point (&result->point, point);

  return &result->parent;
}

/*** ROTATE ***/

typedef struct _GtkRotateMatrix GtkRotateMatrix;

struct _GtkRotateMatrix
{
  GtkMatrix parent;

  float angle;
  graphene_vec3_t axis;
};

static void
gtk_rotate_matrix_finalize (GtkMatrix *self)
{
}

static GskMatrixCategory
gtk_rotate_matrix_categorize (GtkMatrix *matrix)
{
  return GSK_MATRIX_CATEGORY_LINEAR;
}

static void
gtk_rotate_matrix_compute (GtkMatrix         *matrix,
                           graphene_matrix_t *out_matrix)
{
  GtkRotateMatrix *self = (GtkRotateMatrix *) matrix;

  graphene_matrix_init_rotate (out_matrix, self->angle, &self->axis);
}

static gboolean
gtk_rotate_matrix_equal (GtkMatrix *first_matrix,
                         GtkMatrix *second_matrix)
{
  GtkRotateMatrix *first = (GtkRotateMatrix *) first_matrix;
  GtkRotateMatrix *second = (GtkRotateMatrix *) second_matrix;

  return first->angle == second->angle
      && graphene_vec3_equal (&first->axis, &second->axis);
}

static void
gtk_rotate_matrix_print (GtkMatrix *matrix,
                         GString   *string)
{
  GtkRotateMatrix *self = (GtkRotateMatrix *) matrix;
  graphene_vec3_t default_axis;

  graphene_vec3_init (&default_axis, 0, 0, 1);
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

static GtkMatrix *
gtk_rotate_matrix_transition (GtkMatrix *start_matrix,
                              GtkMatrix *end_matrix,
                              double     progress,
                              GtkMatrix *next)
{
  GtkRotateMatrix *start = (GtkRotateMatrix *) start_matrix;
  GtkRotateMatrix *end = (GtkRotateMatrix *) end_matrix;
  graphene_vec3_t start_normalized, end_normalized;

  graphene_vec3_normalize (&start->axis, &start_normalized);
  if (end)
    graphene_vec3_normalize (&end->axis, &end_normalized);
  else
    graphene_vec3_init (&end_normalized, 0, 0, 1);

  if (start->angle == 0.0)
    {
      return gtk_matrix_rotate_3d (next,
                                   end ? end->angle * progress : 0.0,
                                   &end_normalized);
    }

  if (end == NULL || end->angle == 0.0)
    return gtk_matrix_rotate_3d (next, start->angle * progress, &start_normalized);

  if (graphene_vec3_equal (&start_normalized, &end_normalized))
    {
       return gtk_matrix_rotate_3d (next,
                                    start->angle * (1.0 - progress) + end->angle * progress,
                                    &start_normalized);
    }
  else
    {
      graphene_matrix_t sm, em, rm;

      gtk_rotate_matrix_compute (start_matrix, &sm);
      gtk_rotate_matrix_compute (end_matrix, &em);

      graphene_matrix_interpolate (&sm, &em, progress, &rm);

      return gtk_matrix_transform (next, &rm);
    }
}

static const GtkMatrixClass GTK_ROTATE_MATRIX_CLASS = {
  GTK_MATRIX_TYPE_ROTATE,
  sizeof (GtkRotateMatrix),
  "GtkRotateMatrix",
  gtk_rotate_matrix_finalize,
  gtk_rotate_matrix_categorize,
  gtk_rotate_matrix_compute,
  gtk_rotate_matrix_print,
  gtk_rotate_matrix_equal,
  gtk_rotate_matrix_transition,
};

/**
 * gtk_matrix_rotate:
 * @next: (allow-none): the next matrix
 * @angle: the rotation angle, in degrees (clockwise)
 *
 * Rotates @next @nagle degrees in 2D - or in 3Dspeak, around the z axis.
 *
 * Returns: The new matrix
 **/
GtkMatrix *
gtk_matrix_rotate (GtkMatrix *next,
                   float      angle)
{
  graphene_vec3_t axis;

  graphene_vec3_init (&axis, 0, 0, 1);

  return gtk_matrix_rotate_3d (next, angle, &axis);
}

/**
 * gtk_matrix_rotate_3d:
 * @next: (allow-none): the next matrix
 * @angle: the rotation angle, in degrees (clockwise)
 * @axis: The rotation axis
 *
 * Rotates @next @angle degrees around @axis.
 *
 * For a rotation in 2D space, use gtk_matrix_rotate_z().
 *
 * Returns: The new matrix
 **/
GtkMatrix *
gtk_matrix_rotate_3d (GtkMatrix             *next,
                      float                  angle,
                      const graphene_vec3_t *axis)
{
  GtkRotateMatrix *result = gtk_matrix_new (&GTK_ROTATE_MATRIX_CLASS, next);

  result->angle = angle;
  graphene_vec3_init_from_vec3 (&result->axis, axis);

  return &result->parent;
}

/*** SCALE ***/

typedef struct _GtkScaleMatrix GtkScaleMatrix;

struct _GtkScaleMatrix
{
  GtkMatrix parent;

  float factor_x;
  float factor_y;
  float factor_z;
};

static void
gtk_scale_matrix_finalize (GtkMatrix *self)
{
}

static GskMatrixCategory
gtk_scale_matrix_categorize (GtkMatrix *matrix)
{
  GtkScaleMatrix *self = (GtkScaleMatrix *) matrix;

  if (self->factor_z != 1.0)
    return GSK_MATRIX_CATEGORY_LINEAR;

  return GSK_MATRIX_CATEGORY_2D_SCALE;
}

static void
gtk_scale_matrix_compute (GtkMatrix         *matrix,
                          graphene_matrix_t *out_matrix)
{
  GtkScaleMatrix *self = (GtkScaleMatrix *) matrix;

  graphene_matrix_init_scale (out_matrix, self->factor_x, self->factor_y, self->factor_z);
}

static gboolean
gtk_scale_matrix_equal (GtkMatrix *first_matrix,
                        GtkMatrix *second_matrix)
{
  GtkScaleMatrix *first = (GtkScaleMatrix *) first_matrix;
  GtkScaleMatrix *second = (GtkScaleMatrix *) second_matrix;

  return first->factor_x == second->factor_x
      && first->factor_y == second->factor_y
      && first->factor_z == second->factor_z;
}

static void
gtk_scale_matrix_print (GtkMatrix *matrix,
                        GString   *string)
{
  GtkScaleMatrix *self = (GtkScaleMatrix *) matrix;

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

static GtkMatrix *
gtk_scale_matrix_transition (GtkMatrix *start_matrix,
                             GtkMatrix *end_matrix,
                             double     progress,
                             GtkMatrix *next)
{
  GtkScaleMatrix *start = (GtkScaleMatrix *) start_matrix;
  GtkScaleMatrix *end = (GtkScaleMatrix *) end_matrix;

  if (end == NULL)
    {
      return gtk_matrix_scale_3d (next,
                                  start->factor_x * (1.0 - progress),
                                  start->factor_y * (1.0 - progress),
                                  start->factor_z * (1.0 - progress));
    }
  else
    {
      return gtk_matrix_scale_3d (next,
                                  start->factor_x * (1.0 - progress) + end->factor_x * progress,
                                  start->factor_y * (1.0 - progress) + end->factor_y * progress,
                                  start->factor_z * (1.0 - progress) + end->factor_z * progress);
    }
}

static const GtkMatrixClass GTK_SCALE_MATRIX_CLASS = {
  GTK_MATRIX_TYPE_SCALE,
  sizeof (GtkScaleMatrix),
  "GtkScaleMatrix",
  gtk_scale_matrix_finalize,
  gtk_scale_matrix_categorize,
  gtk_scale_matrix_compute,
  gtk_scale_matrix_print,
  gtk_scale_matrix_equal,
  gtk_scale_matrix_transition,
};

/**
 * gtk_matrix_scale:
 * @next: (allow-none): the next matrix
 * @factor_x: scaling factor on the X axis
 * @factor_y: scaling factor on the Y axis
 *
 * Scales @next in 2-dimensional space by the given factors.
 * Use gtk_matrix_scale_3d() to scale in all 3 dimensions.
 *
 * Returns: The new matrix
 **/
GtkMatrix *
gtk_matrix_scale (GtkMatrix *next,
                  float      factor_x,
                  float      factor_y)
{
  return gtk_matrix_scale_3d (next, factor_x, factor_y, 1.0);
}

/**
 * gtk_matrix_scale_3d:
 * @next: (allow-none): the next matrix
 * @factor_x: scaling factor on the X axis
 * @factor_y: scaling factor on the Y axis
 * @factor_z: scaling factor on the Z axis
 *
 * Scales @next by the given factors.
 *
 * Returns: The new matrix
 **/
GtkMatrix *
gtk_matrix_scale_3d (GtkMatrix *next,
                     float      factor_x,
                     float      factor_y,
                     float      factor_z)
{
  GtkScaleMatrix *result = gtk_matrix_new (&GTK_SCALE_MATRIX_CLASS, next);

  result->factor_x = factor_x;
  result->factor_y = factor_y;
  result->factor_z = factor_z;

  return &result->parent;
}

/*** PUBLIC API ***/

static void
gtk_matrix_finalize (GtkMatrix *self)
{
  self->matrix_class->finalize (self);

  gtk_matrix_unref (self->next);

  g_free (self);
}

/**
 * gtk_matrix_ref:
 * @self: (allow-none): a #GtkMatrix
 *
 * Acquires a reference on the given #GtkMatrix.
 *
 * Returns: (transfer none): the #GtkMatrix with an additional reference
 */
GtkMatrix *
gtk_matrix_ref (GtkMatrix *self)
{
  if (self == NULL)
    return NULL;

  g_atomic_int_inc (&self->ref_count);

  return self;
}

/**
 * gtk_matrix_unref:
 * @self: (allow-none): a #GtkMatrix
 *
 * Releases a reference on the given #GtkMatrix.
 *
 * If the reference was the last, the resources associated to the @self are
 * freed.
 */
void
gtk_matrix_unref (GtkMatrix *self)
{
  if (self == NULL)
    return;

  if (g_atomic_int_dec_and_test (&self->ref_count))
    gtk_matrix_finalize (self);
}

/**
 * gtk_matrix_print:
 * @self: (allow-none): a #GtkMatrix
 * @string:  The string to print into
 *
 * Converts @self into a string representation suitable for printing that
 * can later be parsed with gtk_matrix_parse().
 **/
void
gtk_matrix_print (GtkMatrix *self,
                  GString   *string)
{
  g_return_if_fail (string != NULL);

  if (self == NULL)
    {
      g_string_append (string, "none");
      return;
    }

  if (self->next != NULL)
    {
      gtk_matrix_print (self->next, string);
      g_string_append (string, " ");
    }

  self->matrix_class->print (self, string);
}

/**
 * gtk_matrix_to_string:
 * @self: (allow-none): a #GtkMatrix
 *
 * Converts a matrix into a string that is suitable for
 * printing and can later be parsed with gtk_matrix_parse().
 *
 * This is a wrapper around gtk_matrix_print(), see that function
 * for details.
 *
 * Returns: A new string for @self
 **/
char *
gtk_matrix_to_string (GtkMatrix *self)
{
  GString *string;

  string = g_string_new ("");

  gtk_matrix_print (self, string);

  return g_string_free (string, FALSE);
}

/**
 * gtk_matrix_get_matrix_type:
 * @self: (allow-none): a #GtkMatrix
 *
 * Returns the type of the @self.
 *
 * Returns: the type of the #GtkMatrix
 */
GtkMatrixType
gtk_matrix_get_matrix_type (GtkMatrix *self)
{
  if (self == NULL)
    return GTK_MATRIX_TYPE_IDENTITY;

  return self->matrix_class->matrix_type;
}

/**
 * gtk_matrix_get_next:
 * @self: (allow-none): a #GtkMatrix
 *
 * Gets the rest of the matrix in the chain of operations.
 *
 * Returns: (transfer none) (nullable): The next matrix or
 *     %NULL if this was the last operation.
 **/
GtkMatrix *
gtk_matrix_get_next (GtkMatrix *self)
{
  if (self == NULL)
    return NULL;

  return self->next;
}

/**
 * gtk_matrix_compute:
 * @self: (allow-none): a #GtkMatrix
 * @out_matrix: (out): The location to set for the computed matrix
 *
 * Computes the actual value of @self and stores it in @out_matrix.
 * The previous value of @out_matrix will be ignored.
 **/
void
gtk_matrix_compute (GtkMatrix         *self,
                    graphene_matrix_t *out_matrix)
{
  graphene_matrix_t computed;

  if (self == NULL)
    {
      graphene_matrix_init_identity (out_matrix);
      return;
    }
  
  gtk_matrix_compute (self->next, out_matrix);
  self->matrix_class->compute (self, &computed);
  graphene_matrix_multiply (&computed, out_matrix, out_matrix);
}

/**
 * gtk_matrix_equal:
 * @first: the first matrix
 * @second: the second matrix
 *
 * Checks two matrices for equality. Note that matrices need to be literally
 * identical in their operations, it is not enough that they return the
 * same result in gtk_matrix_compute().
 *
 * Returns: %TRUE if the two matrices can be proven to be equal
 **/
gboolean
gtk_matrix_equal (GtkMatrix *first,
                  GtkMatrix *second)
{
  if (first == second)
    return TRUE;

  if (first == NULL || second == NULL)
    return FALSE;

  if (!gtk_matrix_equal (first->next, second->next))
    return FALSE;

  if (first->matrix_class != second->matrix_class)
    return FALSE;

  return first->matrix_class->equal (first, second);
}

static gboolean
operation_is_identity (GtkMatrix *matrix)
{
  return matrix == NULL || matrix->matrix_class->matrix_type == GTK_MATRIX_TYPE_IDENTITY;
}

/**
 * gtk_matrix_transition:
 * @start: (allow-none): start matrix for the transition
 * @end: (allow-none): end matrix for the transition
 * @progress: how far the transition has progressed, with 0.0 being equal
 *     to @start and 1.0 equal to @end. Values larger than 1.0 and lower
 *     than 0.0 are allowed and will produce "overshoot".
 *
 * Computes a transition matrix that smoothly transitions @start into @end.
 *
 * This transition is attempted operation-by-operation, so if the operations
 * that make up the given matrices match, the transition will attempt to scale
 * those operations.  
 * If that is not possible or if the transition is otherwise problematic, an
 * identity matrix may be returned.
 *
 * Returns: (nullable): The resulting transition.
 **/
GtkMatrix *
gtk_matrix_transition (GtkMatrix *start,
                       GtkMatrix *end,
                       double     progress)
{
  GtkMatrix *result;

  if (start == NULL && end == NULL)
    return NULL;

  result = gtk_matrix_transition (start ? start->next : NULL,
                                  end ? end->next : NULL,
                                  progress);

  if (operation_is_identity (start))
    {
      return end->matrix_class->transition (end,
                                            NULL,
                                            1.0 - progress,
                                            result);
    } 
  else if (operation_is_identity (end))
    {
      return start->matrix_class->transition (start,
                                              NULL,
                                              progress,
                                              result);
    }
  else if (start->matrix_class == end->matrix_class)
    {
      return start->matrix_class->transition (start,
                                              end,
                                              progress,
                                              result);
    }
  else
    {
      graphene_matrix_t sm, em, rm;

      /* call vfunc here, we just want the matrix for this operation */
      start->matrix_class->compute (start, &sm);
      end->matrix_class->compute (end, &em);

      graphene_matrix_interpolate (&sm, &em, progress, &rm);

      return gtk_matrix_transform (result, &rm);
    }
}

/*<private>
 * gtk_matrix_categorize:
 * @self: (allow-none): A matrix
 *
 * 
 *
 * Returns: The category this matrix belongs to
 **/
GskMatrixCategory
gtk_matrix_categorize (GtkMatrix *self)
{
  if (self == NULL)
    return GSK_MATRIX_CATEGORY_IDENTITY;

  return MIN (gtk_matrix_categorize (self->next),
              self->matrix_class->categorize (self));
}

/**
 * gtk_matrix_get_identity:
 *
 * Returns the identity matrix. In C code, this equivalent to
 * using %NULL.
 *
 * See also gtk_matrix_identity() for inserting identity matrix operations
 * when constructing matrices.
 *
 * Returns: The identity matrix
 **/
GtkMatrix *
gtk_matrix_get_identity (void)
{
  return NULL;
}

