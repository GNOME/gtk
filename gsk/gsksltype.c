/* GTK - The GIMP Toolkit
 *   
 * Copyright © 2017 Benjamin Otte <otte@gnome.org>
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

#include "gsksltypeprivate.h"

#include "gsksltokenizerprivate.h"
#include "gskslpreprocessorprivate.h"

#include <string.h>

#define N_SCALAR_TYPES 6

typedef struct _GskSlTypeClass GskSlTypeClass;

struct _GskSlType
{
  const GskSlTypeClass *class;

  int ref_count;
};

struct _GskSlTypeClass {
  void                  (* free)                                (GskSlType           *type);

  void                  (* print)                               (const GskSlType     *type,
                                                                 GString             *string);
  GskSlScalarType       (* get_scalar_type)                     (const GskSlType     *type);
  GskSlType *           (* get_index_type)                      (const GskSlType     *type);
  guint                 (* get_length)                          (const GskSlType     *type);
  gboolean              (* can_convert)                         (const GskSlType     *target,
                                                                 const GskSlType     *source);
};

/* SCALAR */

typedef struct _GskSlTypeScalar GskSlTypeScalar;

struct _GskSlTypeScalar {
  GskSlType parent;

  GskSlScalarType scalar;
};

static void
gsk_sl_type_scalar_free (GskSlType *type)
{
  g_assert_not_reached ();
}

static void
gsk_sl_type_scalar_print (const GskSlType *type,
                          GString         *string)
{
  GskSlTypeScalar *scalar = (GskSlTypeScalar *) type;

  switch (scalar->scalar)
  {
    case GSK_SL_VOID:
      g_string_append (string, "void");
      break;
    case GSK_SL_FLOAT:
      g_string_append (string, "float");
      break;
    case GSK_SL_DOUBLE:
      g_string_append (string, "double");
      break;
    case GSK_SL_INT:
      g_string_append (string, "int");
      break;
    case GSK_SL_UINT:
      g_string_append (string, "uint");
      break;
    case GSK_SL_BOOL:
      g_string_append (string, "bool");
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

static GskSlScalarType
gsk_sl_type_scalar_get_scalar_type (const GskSlType *type)
{
  GskSlTypeScalar *scalar = (GskSlTypeScalar *) type;

  return scalar->scalar;
}

static GskSlType *
gsk_sl_type_scalar_get_index_type (const GskSlType *type)
{
  return NULL;
}

static guint
gsk_sl_type_scalar_get_length (const GskSlType *type)
{
  return 0;
}

static gboolean
gsk_sl_type_scalar_can_convert (const GskSlType *target,
                                const GskSlType *source)
{
  const GskSlTypeScalar *target_scalar = (const GskSlTypeScalar *) target;
  const GskSlTypeScalar *source_scalar = (const GskSlTypeScalar *) source;

  if (target->class != source->class)
    return FALSE;
  
  return gsk_sl_scalar_type_can_convert (target_scalar->scalar, source_scalar->scalar);
}

static const GskSlTypeClass GSK_SL_TYPE_SCALAR = {
  gsk_sl_type_scalar_free,
  gsk_sl_type_scalar_print,
  gsk_sl_type_scalar_get_scalar_type,
  gsk_sl_type_scalar_get_index_type,
  gsk_sl_type_scalar_get_length,
  gsk_sl_type_scalar_can_convert
};

/* VECTOR */

typedef struct _GskSlTypeVector GskSlTypeVector;

struct _GskSlTypeVector {
  GskSlType parent;

  GskSlScalarType scalar;
  guint length;
};

static void
gsk_sl_type_vector_free (GskSlType *type)
{
  g_assert_not_reached ();
}

static void
gsk_sl_type_vector_print (const GskSlType *type,
                          GString         *string)
{
  GskSlTypeVector *vector = (GskSlTypeVector *) type;

  switch (vector->scalar)
  {
    case GSK_SL_FLOAT:
      g_string_append (string, "vec");
      break;
    case GSK_SL_DOUBLE:
      g_string_append (string, "dvec");
      break;
    case GSK_SL_INT:
      g_string_append (string, "ivec");
      break;
    case GSK_SL_UINT:
      g_string_append (string, "uvec");
      break;
    case GSK_SL_BOOL:
      g_string_append (string, "bvec");
      break;
    case GSK_SL_VOID:
    default:
      g_assert_not_reached ();
      break;
  }

  g_string_append_printf (string, "%u", vector->length);
}

static GskSlScalarType
gsk_sl_type_vector_get_scalar_type (const GskSlType *type)
{
  const GskSlTypeVector *vector = (const GskSlTypeVector *) type;

  return vector->scalar;
}

static GskSlType *
gsk_sl_type_vector_get_index_type (const GskSlType *type)
{
  const GskSlTypeVector *vector = (const GskSlTypeVector *) type;

  return gsk_sl_type_get_scalar (vector->scalar);
}

static guint
gsk_sl_type_vector_get_length (const GskSlType *type)
{
  GskSlTypeVector *vector = (GskSlTypeVector *) type;

  return vector->length;
}

static gboolean
gsk_sl_type_vector_can_convert (const GskSlType *target,
                                const GskSlType *source)
{
  const GskSlTypeVector *target_vector = (const GskSlTypeVector *) target;
  const GskSlTypeVector *source_vector = (const GskSlTypeVector *) source;

  if (target->class != source->class)
    return FALSE;
  
  if (target_vector->length != source_vector->length)
    return FALSE;

  return gsk_sl_scalar_type_can_convert (target_vector->scalar, source_vector->scalar);
}

static const GskSlTypeClass GSK_SL_TYPE_VECTOR = {
  gsk_sl_type_vector_free,
  gsk_sl_type_vector_print,
  gsk_sl_type_vector_get_scalar_type,
  gsk_sl_type_vector_get_index_type,
  gsk_sl_type_vector_get_length,
  gsk_sl_type_vector_can_convert
};

/* MATRIX */

typedef struct _GskSlTypeMatrix GskSlTypeMatrix;

struct _GskSlTypeMatrix {
  GskSlType parent;

  GskSlScalarType scalar;
  guint columns;
  guint rows;
};

static void
gsk_sl_type_matrix_free (GskSlType *type)
{
  g_assert_not_reached ();
}

static void
gsk_sl_type_matrix_print (const GskSlType *type,
                          GString         *string)
{
  const GskSlTypeMatrix *matrix = (const GskSlTypeMatrix *) type;

  g_string_append (string, matrix->scalar == GSK_SL_DOUBLE ? "dmat" : "mat");
  g_string_append_printf (string, "%u", matrix->columns);
  if (matrix->columns != matrix->rows)
    {
      g_string_append_printf (string, "x%u", matrix->rows);
    }
}

static GskSlScalarType
gsk_sl_type_matrix_get_scalar_type (const GskSlType *type)
{
  const GskSlTypeMatrix *matrix = (const GskSlTypeMatrix *) type;

  return matrix->scalar;
}

static GskSlType *
gsk_sl_type_matrix_get_index_type (const GskSlType *type)
{
  const GskSlTypeMatrix *matrix = (const GskSlTypeMatrix *) type;

  return gsk_sl_type_get_vector (matrix->scalar, matrix->rows);
}

static guint
gsk_sl_type_matrix_get_length (const GskSlType *type)
{
  GskSlTypeMatrix *matrix = (GskSlTypeMatrix *) type;

  return matrix->columns;
}

static gboolean
gsk_sl_type_matrix_can_convert (const GskSlType *target,
                                const GskSlType *source)
{
  const GskSlTypeMatrix *target_matrix = (const GskSlTypeMatrix *) target;
  const GskSlTypeMatrix *source_matrix = (const GskSlTypeMatrix *) source;

  if (target->class != source->class)
    return FALSE;
  
  if (target_matrix->rows != source_matrix->rows ||
      target_matrix->columns != source_matrix->columns)
    return FALSE;

  return gsk_sl_scalar_type_can_convert (target_matrix->scalar, source_matrix->scalar);
}

static const GskSlTypeClass GSK_SL_TYPE_MATRIX = {
  gsk_sl_type_matrix_free,
  gsk_sl_type_matrix_print,
  gsk_sl_type_matrix_get_scalar_type,
  gsk_sl_type_matrix_get_index_type,
  gsk_sl_type_matrix_get_length,
  gsk_sl_type_matrix_can_convert
};

GskSlType *
gsk_sl_type_new_parse (GskSlPreprocessor *stream)
{
  GskSlType *type;
  const GskSlToken *token;

  token = gsk_sl_preprocessor_get (stream);

  switch ((guint) token->type)
  {
    case GSK_SL_TOKEN_VOID:
      type = gsk_sl_type_ref (gsk_sl_type_get_scalar (GSK_SL_VOID));
      break;
    case GSK_SL_TOKEN_FLOAT:
      type = gsk_sl_type_ref (gsk_sl_type_get_scalar (GSK_SL_FLOAT));
      break;
    case GSK_SL_TOKEN_DOUBLE:
      type = gsk_sl_type_ref (gsk_sl_type_get_scalar (GSK_SL_DOUBLE));
      break;
    case GSK_SL_TOKEN_INT:
      type = gsk_sl_type_ref (gsk_sl_type_get_scalar (GSK_SL_INT));
      break;
    case GSK_SL_TOKEN_UINT:
      type = gsk_sl_type_ref (gsk_sl_type_get_scalar (GSK_SL_UINT));
      break;
    case GSK_SL_TOKEN_BOOL:
      type = gsk_sl_type_ref (gsk_sl_type_get_scalar (GSK_SL_BOOL));
      break;
    case GSK_SL_TOKEN_BVEC2:
      type = gsk_sl_type_ref (gsk_sl_type_get_vector (GSK_SL_BOOL, 2));
      break;
    case GSK_SL_TOKEN_BVEC3:
      type = gsk_sl_type_ref (gsk_sl_type_get_vector (GSK_SL_BOOL, 3));
      break;
    case GSK_SL_TOKEN_BVEC4:
      type = gsk_sl_type_ref (gsk_sl_type_get_vector (GSK_SL_BOOL, 4));
      break;
    case GSK_SL_TOKEN_IVEC2:
      type = gsk_sl_type_ref (gsk_sl_type_get_vector (GSK_SL_INT, 2));
      break;
    case GSK_SL_TOKEN_IVEC3:
      type = gsk_sl_type_ref (gsk_sl_type_get_vector (GSK_SL_INT, 3));
      break;
    case GSK_SL_TOKEN_IVEC4:
      type = gsk_sl_type_ref (gsk_sl_type_get_vector (GSK_SL_INT, 4));
      break;
    case GSK_SL_TOKEN_UVEC2:
      type = gsk_sl_type_ref (gsk_sl_type_get_vector (GSK_SL_UINT, 2));
      break;
    case GSK_SL_TOKEN_UVEC3:
      type = gsk_sl_type_ref (gsk_sl_type_get_vector (GSK_SL_UINT, 3));
      break;
    case GSK_SL_TOKEN_UVEC4:
      type = gsk_sl_type_ref (gsk_sl_type_get_vector (GSK_SL_UINT, 4));
      break;
    case GSK_SL_TOKEN_VEC2:
      type = gsk_sl_type_ref (gsk_sl_type_get_vector (GSK_SL_FLOAT, 2));
      break;
    case GSK_SL_TOKEN_VEC3:
      type = gsk_sl_type_ref (gsk_sl_type_get_vector (GSK_SL_FLOAT, 3));
      break;
    case GSK_SL_TOKEN_VEC4:
      type = gsk_sl_type_ref (gsk_sl_type_get_vector (GSK_SL_FLOAT, 4));
      break;
    case GSK_SL_TOKEN_DVEC2:
      type = gsk_sl_type_ref (gsk_sl_type_get_vector (GSK_SL_DOUBLE, 2));
      break;
    case GSK_SL_TOKEN_DVEC3:
      type = gsk_sl_type_ref (gsk_sl_type_get_vector (GSK_SL_DOUBLE, 3));
      break;
    case GSK_SL_TOKEN_DVEC4:
      type = gsk_sl_type_ref (gsk_sl_type_get_vector (GSK_SL_DOUBLE, 4));
      break;
    case GSK_SL_TOKEN_MAT2:
    case GSK_SL_TOKEN_MAT2X2:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 2, 2));
      break;
    case GSK_SL_TOKEN_MAT2X3:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 2, 3));
      break;
    case GSK_SL_TOKEN_MAT2X4:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 2, 4));
      break;
    case GSK_SL_TOKEN_MAT3X2:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 3, 2));
      break;
    case GSK_SL_TOKEN_MAT3:
    case GSK_SL_TOKEN_MAT3X3:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 3, 3));
      break;
    case GSK_SL_TOKEN_MAT3X4:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 3, 4));
      break;
    case GSK_SL_TOKEN_MAT4X2:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 4, 2));
      break;
    case GSK_SL_TOKEN_MAT4X3:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 4, 3));
      break;
    case GSK_SL_TOKEN_MAT4:
    case GSK_SL_TOKEN_MAT4X4:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 4, 4));
      break;
    case GSK_SL_TOKEN_DMAT2:
    case GSK_SL_TOKEN_DMAT2X2:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 2, 2));
      break;
    case GSK_SL_TOKEN_DMAT2X3:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 2, 3));
      break;
    case GSK_SL_TOKEN_DMAT2X4:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 2, 4));
      break;
    case GSK_SL_TOKEN_DMAT3X2:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 3, 2));
      break;
    case GSK_SL_TOKEN_DMAT3:
    case GSK_SL_TOKEN_DMAT3X3:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 3, 3));
      break;
    case GSK_SL_TOKEN_DMAT3X4:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 3, 4));
      break;
    case GSK_SL_TOKEN_DMAT4X2:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 4, 2));
      break;
    case GSK_SL_TOKEN_DMAT4X3:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 4, 3));
      break;
    case GSK_SL_TOKEN_DMAT4:
    case GSK_SL_TOKEN_DMAT4X4:
      type = gsk_sl_type_ref (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 4, 4));
      break;
    default:
      gsk_sl_preprocessor_error (stream, "Expected type specifier");
      return NULL;
  }

  gsk_sl_preprocessor_consume (stream, NULL);

  return type;
}

static GskSlTypeScalar
builtin_scalar_types[N_SCALAR_TYPES] = {
  [GSK_SL_VOID] = { { &GSK_SL_TYPE_SCALAR, 1 }, GSK_SL_VOID },
  [GSK_SL_FLOAT] = { { &GSK_SL_TYPE_SCALAR, 1 }, GSK_SL_FLOAT },
  [GSK_SL_DOUBLE] = { { &GSK_SL_TYPE_SCALAR, 1 }, GSK_SL_DOUBLE },
  [GSK_SL_INT] = { { &GSK_SL_TYPE_SCALAR, 1 }, GSK_SL_INT },
  [GSK_SL_UINT] = { { &GSK_SL_TYPE_SCALAR, 1 }, GSK_SL_UINT },
  [GSK_SL_BOOL] = { { &GSK_SL_TYPE_SCALAR, 1 }, GSK_SL_BOOL },
};

GskSlType *
gsk_sl_type_get_scalar (GskSlScalarType scalar)
{
  g_assert (scalar < N_SCALAR_TYPES);

  return &builtin_scalar_types[scalar].parent;
}

static GskSlTypeVector
builtin_vector_types[3][N_SCALAR_TYPES] = {
  {
    [GSK_SL_FLOAT] = { { &GSK_SL_TYPE_VECTOR, 1 }, GSK_SL_FLOAT, 2 },
    [GSK_SL_DOUBLE] = { { &GSK_SL_TYPE_VECTOR, 1 }, GSK_SL_DOUBLE, 2 },
    [GSK_SL_INT] = { { &GSK_SL_TYPE_VECTOR, 1 }, GSK_SL_INT, 2 },
    [GSK_SL_UINT] = { { &GSK_SL_TYPE_VECTOR, 1 }, GSK_SL_UINT, 2 },
    [GSK_SL_BOOL] = { { &GSK_SL_TYPE_VECTOR, 1 }, GSK_SL_BOOL, 2 },
  },
  {
    [GSK_SL_FLOAT] = { { &GSK_SL_TYPE_VECTOR, 1 }, GSK_SL_FLOAT, 3 },
    [GSK_SL_DOUBLE] = { { &GSK_SL_TYPE_VECTOR, 1 }, GSK_SL_DOUBLE, 3 },
    [GSK_SL_INT] = { { &GSK_SL_TYPE_VECTOR, 1 }, GSK_SL_INT, 3 },
    [GSK_SL_UINT] = { { &GSK_SL_TYPE_VECTOR, 1 }, GSK_SL_UINT, 3 },
    [GSK_SL_BOOL] = { { &GSK_SL_TYPE_VECTOR, 1 }, GSK_SL_BOOL, 3 },
  },
  {
    [GSK_SL_FLOAT] = { { &GSK_SL_TYPE_VECTOR, 1 }, GSK_SL_FLOAT, 4 },
    [GSK_SL_DOUBLE] = { { &GSK_SL_TYPE_VECTOR, 1 }, GSK_SL_DOUBLE, 4 },
    [GSK_SL_INT] = { { &GSK_SL_TYPE_VECTOR, 1 }, GSK_SL_INT, 4 },
    [GSK_SL_UINT] = { { &GSK_SL_TYPE_VECTOR, 1 }, GSK_SL_UINT, 4 },
    [GSK_SL_BOOL] = { { &GSK_SL_TYPE_VECTOR, 1 }, GSK_SL_BOOL, 4 },
  }
};

GskSlType *
gsk_sl_type_get_vector (GskSlScalarType scalar,
                        guint           length)
{
  g_assert (scalar < N_SCALAR_TYPES);
  g_assert (scalar != GSK_SL_VOID);
  g_assert (length >= 2 && length <= 4);

  return &builtin_vector_types[length - 2][scalar].parent;
}

static GskSlTypeMatrix
builtin_matrix_types[3][3][2] = {
  {
    {
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_FLOAT, 2, 2 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_DOUBLE, 2, 2 }
    },
    {
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_FLOAT, 2, 3 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_DOUBLE, 2, 3 }
    },
    {
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_FLOAT, 2, 4 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_DOUBLE, 2, 4 }
    },
  },
  {
    {
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_FLOAT, 3, 2 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_DOUBLE, 3, 2 }
    },
    {
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_FLOAT, 3, 3 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_DOUBLE, 3, 3 }
    },
    {
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_FLOAT, 3, 4 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_DOUBLE, 3, 4 }
    },
  },
  {
    {
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_FLOAT, 4, 2 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_DOUBLE, 4, 2 }
    },
    {
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_FLOAT, 4, 3 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_DOUBLE, 4, 3 }
    },
    {
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_FLOAT, 4, 4 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, GSK_SL_DOUBLE, 4, 4 }
    },
  },
};

GskSlType *
gsk_sl_type_get_matrix (GskSlScalarType      scalar,
                        guint                columns,
                        guint                rows)
{
  g_assert (scalar == GSK_SL_FLOAT || scalar == GSK_SL_DOUBLE);
  g_assert (columns >= 2 && columns <= 4);
  g_assert (rows >= 2 && rows <= 4);

  return &builtin_matrix_types[columns - 2][rows - 2][scalar == GSK_SL_FLOAT ? 0 : 1].parent;
}

GskSlType *
gsk_sl_type_ref (GskSlType *type)
{
  g_return_val_if_fail (type != NULL, NULL);

  type->ref_count += 1;

  return type;
}

void
gsk_sl_type_unref (GskSlType *type)
{
  if (type == NULL)
    return;

  type->ref_count -= 1;
  if (type->ref_count > 0)
    return;

  type->class->free (type);
}

void
gsk_sl_type_print (const GskSlType *type,
                   GString         *string)
{
  return type->class->print (type, string);
}

char *
gsk_sl_type_to_string (const GskSlType *type)
{
  GString *string;

  string = g_string_new (NULL);
  gsk_sl_type_print (type, string);
  return g_string_free (string, FALSE);
}

gboolean
gsk_sl_type_is_scalar (const GskSlType *type)
{
  return type->class == &GSK_SL_TYPE_SCALAR;
}

gboolean
gsk_sl_type_is_vector (const GskSlType *type)
{
  return type->class == &GSK_SL_TYPE_VECTOR;
}

gboolean
gsk_sl_type_is_matrix (const GskSlType *type)
{
  return type->class == &GSK_SL_TYPE_MATRIX;
}

GskSlScalarType
gsk_sl_type_get_scalar_type (const GskSlType *type)
{
  return type->class->get_scalar_type (type);
}

GskSlType *
gsk_sl_type_get_index_type (const GskSlType *type)
{
  return type->class->get_index_type (type);
}

GskSlScalarType
gsk_sl_type_get_length (const GskSlType *type)
{
  return type->class->get_length (type);
}

gboolean
gsk_sl_scalar_type_can_convert (GskSlScalarType target,
                                GskSlScalarType source)
{
  if (target == source)
    return TRUE;

  switch (source)
  {
    case GSK_SL_INT:
      return target == GSK_SL_UINT
          || target == GSK_SL_FLOAT
          || target == GSK_SL_DOUBLE;
    case GSK_SL_UINT:
      return target == GSK_SL_FLOAT
          || target == GSK_SL_DOUBLE;
    case GSK_SL_FLOAT:
      return target == GSK_SL_DOUBLE;
    case GSK_SL_DOUBLE:
    case GSK_SL_BOOL:
    case GSK_SL_VOID:
    default:
      return FALSE;
  }
}

gboolean
gsk_sl_type_can_convert (const GskSlType *target,
                         const GskSlType *source)
{
  return target->class->can_convert (target, source);
}
