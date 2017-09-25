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
#include "gskslvalueprivate.h"
#include "gskspvwriterprivate.h"

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

  const char *          (* get_name)                            (const GskSlType     *type);
  GskSlScalarType       (* get_scalar_type)                     (const GskSlType     *type);
  GskSlType *           (* get_index_type)                      (const GskSlType     *type);
  guint                 (* get_length)                          (const GskSlType     *type);
  gsize                 (* get_size)                            (const GskSlType     *type);
  gboolean              (* can_convert)                         (const GskSlType     *target,
                                                                 const GskSlType     *source);
  guint32               (* write_spv)                           (GskSlType           *type,
                                                                 GskSpvWriter        *writer);
  void                  (* print_value)                         (const GskSlType     *type,
                                                                 GString             *string,
                                                                 gconstpointer        value);
  guint32               (* write_value_spv)                     (GskSlType           *type,
                                                                 GskSpvWriter        *writer,
                                                                 gconstpointer        value);
};

static void
print_void (GString       *string,
            gconstpointer  value)
{
  g_assert_not_reached ();
}

static guint32
write_void_spv (GskSpvWriter  *writer,
                gconstpointer  value)
{
  g_assert_not_reached ();
}

static void
print_float (GString       *string,
             gconstpointer  value)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];
  const gfloat *f = value;
      
  g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, *f);
  g_string_append (string, buf);
  if (strchr (buf, '.') == NULL)
    g_string_append (string, ".0");
}

static guint32
write_float_spv (GskSpvWriter  *writer,
                 gconstpointer  value)
{
  guint32 type_id, result_id;

  type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_scalar (GSK_SL_FLOAT));
  result_id = gsk_spv_writer_next_id (writer);
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_DECLARE,
                      4, GSK_SPV_OP_CONSTANT,
                      (guint32[3]) { type_id,
                                     result_id,
                                     *(const guint32 *) value });

  return result_id;
}

static void
print_double (GString       *string,
              gconstpointer  value)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];
  const gdouble *d = value;
      
  g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, *d);
  g_string_append (string, buf);
  if (strchr (buf, '.') == NULL)
    g_string_append (string, ".0");
  g_string_append (string, "lf");
}

static guint32
write_double_spv (GskSpvWriter  *writer,
                  gconstpointer  value)
{
  guint32 type_id, result_id;

  type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_scalar (GSK_SL_DOUBLE));
  result_id = gsk_spv_writer_next_id (writer);
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_DECLARE,
                      5, GSK_SPV_OP_CONSTANT,
                      (guint32[4]) { type_id,
                                     result_id,
                                     *(const guint32 *) value,
                                     *(((const guint32 *) value) + 1) });

  return result_id;
}

static void
print_int (GString       *string,
           gconstpointer  value)
{
  const gint32 *i = value;

  g_string_append_printf (string, "%i", (gint) *i);
}

static guint32
write_int_spv (GskSpvWriter  *writer,
               gconstpointer  value)
{
  guint32 type_id, result_id;

  type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_scalar (GSK_SL_INT));
  result_id = gsk_spv_writer_next_id (writer);
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_DECLARE,
                      4, GSK_SPV_OP_CONSTANT,
                      (guint32[3]) { type_id,
                                     result_id,
                                     *(const guint32 *) value });

  return result_id;
}

static void
print_uint (GString       *string,
            gconstpointer  value)
{
  const guint32 *u = value;
  
  g_string_append_printf (string, "%uu", (guint) *u);
}

static guint32
write_uint_spv (GskSpvWriter  *writer,
                gconstpointer  value)
{
  guint32 type_id, result_id;

  type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_scalar (GSK_SL_UINT));
  result_id = gsk_spv_writer_next_id (writer);
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_DECLARE,
                      4, GSK_SPV_OP_CONSTANT,
                      (guint32[3]) { type_id,
                                     result_id,
                                     *(const guint32 *) value });

  return result_id;
}

static void
print_bool (GString       *string,
            gconstpointer  value)
{
  const guint32 *u = value;
  
  g_string_append_printf (string, *u ? "true" : "false");
}

static guint32
write_bool_spv (GskSpvWriter  *writer,
                gconstpointer  value)
{
  guint32 type_id, result_id;

  type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_scalar (GSK_SL_BOOL));
  result_id = gsk_spv_writer_next_id (writer);
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_DECLARE,
                      3, *(const guint32 *) value ? GSK_SPV_OP_CONSTANT_TRUE : GSK_SPV_OP_CONSTANT_FALSE,
                      (guint32[2]) { type_id,
                                     result_id });
  
  return result_id;
}

struct {
  const char *name;
  gsize size;
  void (* print_value) (GString *string, gconstpointer value);
  guint32 (* write_value_spv) (GskSpvWriter *writer, gconstpointer value);
} scalar_infos[] = {
  [GSK_SL_VOID] =   { "void",   0, print_void,   write_void_spv, },
  [GSK_SL_FLOAT] =  { "float",  4, print_float,  write_float_spv },
  [GSK_SL_DOUBLE] = { "double", 8, print_double, write_double_spv },
  [GSK_SL_INT] =    { "int",    4, print_int,    write_int_spv },
  [GSK_SL_UINT] =   { "uint",   4, print_uint,   write_uint_spv },
  [GSK_SL_BOOL] =   { "bool",   4, print_bool,   write_bool_spv }
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

static const char *
gsk_sl_type_scalar_get_name (const GskSlType *type)
{
  const GskSlTypeScalar *scalar = (const GskSlTypeScalar *) type;

  return scalar_infos[scalar->scalar].name;
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

static gsize
gsk_sl_type_scalar_get_size (const GskSlType *type)
{
  const GskSlTypeScalar *scalar = (const GskSlTypeScalar *) type;

  return scalar_infos[scalar->scalar].size;
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

static guint32
gsk_sl_type_scalar_write_spv (GskSlType    *type,
                              GskSpvWriter *writer)
{
  GskSlTypeScalar *scalar = (GskSlTypeScalar *) type;
  guint32 result;

  switch (scalar->scalar)
  {
    case GSK_SL_VOID:
      result = gsk_spv_writer_next_id (writer);
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_DECLARE,
                          2, GSK_SPV_OP_TYPE_VOID,
                          (guint32[1]) { result });
      break;

    case GSK_SL_FLOAT:
      result = gsk_spv_writer_next_id (writer);
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_DECLARE,
                          3, GSK_SPV_OP_TYPE_FLOAT,
                          (guint32[2]) { result,
                                         32 });
      break;

    case GSK_SL_DOUBLE:
      result = gsk_spv_writer_next_id (writer);
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_DECLARE,
                          3, GSK_SPV_OP_TYPE_FLOAT,
                          (guint32[2]) { result,
                                         64 });
      break;

    case GSK_SL_INT:
      result = gsk_spv_writer_next_id (writer);
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_DECLARE,
                          4, GSK_SPV_OP_TYPE_INT,
                          (guint32[3]) { result,
                                         32,
                                         1 });
      break;

    case GSK_SL_UINT:
      result = gsk_spv_writer_next_id (writer);
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_DECLARE,
                          4, GSK_SPV_OP_TYPE_INT,
                          (guint32[3]) { result,
                                         32,
                                         0 });
      break;

    case GSK_SL_BOOL:
      result = gsk_spv_writer_next_id (writer);
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_DECLARE,
                          2, GSK_SPV_OP_TYPE_BOOL,
                          (guint32[1]) { result });
      break;

    default:
      g_assert_not_reached ();
      break;
  }

  return result;
}

static void
gsk_sl_type_scalar_print_value (const GskSlType *type,
                                GString         *string,
                                gconstpointer    value)
{
  const GskSlTypeScalar *scalar = (const GskSlTypeScalar *) type;

  scalar_infos[scalar->scalar].print_value (string, value);
}

static guint32
gsk_sl_type_scalar_write_value_spv (GskSlType     *type,
                                    GskSpvWriter  *writer,
                                    gconstpointer  value)
{
  GskSlTypeScalar *scalar = (GskSlTypeScalar *) type;

  return scalar_infos[scalar->scalar].write_value_spv (writer, value);
}

static const GskSlTypeClass GSK_SL_TYPE_SCALAR = {
  gsk_sl_type_scalar_free,
  gsk_sl_type_scalar_get_name,
  gsk_sl_type_scalar_get_scalar_type,
  gsk_sl_type_scalar_get_index_type,
  gsk_sl_type_scalar_get_length,
  gsk_sl_type_scalar_get_size,
  gsk_sl_type_scalar_can_convert,
  gsk_sl_type_scalar_write_spv,
  gsk_sl_type_scalar_print_value,
  gsk_sl_type_scalar_write_value_spv
};

/* VECTOR */

typedef struct _GskSlTypeVector GskSlTypeVector;

struct _GskSlTypeVector {
  GskSlType parent;

  const char *name;
  GskSlScalarType scalar;
  guint length;
};

static void
gsk_sl_type_vector_free (GskSlType *type)
{
  g_assert_not_reached ();
}

static const char *
gsk_sl_type_vector_get_name (const GskSlType *type)
{
  const GskSlTypeVector *vector = (const GskSlTypeVector *) type;

  return vector->name;
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

static gsize
gsk_sl_type_vector_get_size (const GskSlType *type)
{
  const GskSlTypeVector *vector = (const GskSlTypeVector *) type;

  return vector->length * scalar_infos[vector->scalar].size;
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

static guint32
gsk_sl_type_vector_write_spv (GskSlType    *type,
                              GskSpvWriter *writer)
{
  GskSlTypeVector *vector = (GskSlTypeVector *) type;
  guint32 result_id, scalar_id;

  scalar_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_scalar (vector->scalar));
  result_id = gsk_spv_writer_next_id (writer);
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_DECLARE,
                      4, GSK_SPV_OP_TYPE_VECTOR,
                      (guint32[3]) { result_id,
                                     scalar_id,
                                     vector->length });
  
  return result_id;
}

static void
gsk_sl_type_vector_print_value (const GskSlType *type,
                                GString         *string,
                                gconstpointer    value)
{
  const GskSlTypeVector *vector = (const GskSlTypeVector *) type;
  guint i;
  const guchar *data;

  data = value;

  g_string_append (string, vector->name);
  g_string_append (string, "(");
  for (i = 0; i < vector->length; i++)
    {
      if (i > 0)
        g_string_append (string, ", ");
      scalar_infos[vector->scalar].print_value (string, data);
      data += scalar_infos[vector->scalar].size;
    }

  g_string_append (string, ")");
}

static guint32
gsk_sl_type_vector_write_value_spv (GskSlType     *type,
                                    GskSpvWriter  *writer,
                                    gconstpointer  value)
{
  GskSlTypeVector *vector = (GskSlTypeVector *) type;
  guint32 ids[vector->length + 2];
  GskSlType *scalar_type;
  GskSlValue *v;
  const guchar *data;
  guint i;

  data = value;
  scalar_type = gsk_sl_type_get_scalar (vector->scalar);

  ids[0] = gsk_spv_writer_get_id_for_type (writer, type);
  for (i = 0; i < vector->length; i++)
    {
      v = gsk_sl_value_new_for_data (scalar_type, (gpointer) data, NULL, NULL);
      ids[2 + i] = gsk_spv_writer_get_id_for_value (writer, v);
      gsk_sl_value_free (v);
      data += scalar_infos[vector->scalar].size;
    }
  ids[1] = gsk_spv_writer_next_id (writer);

  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_DECLARE,
                      3 + vector->length,
                      GSK_SPV_OP_CONSTANT_COMPOSITE,
                      ids);
  
  return ids[1];
}

static const GskSlTypeClass GSK_SL_TYPE_VECTOR = {
  gsk_sl_type_vector_free,
  gsk_sl_type_vector_get_name,
  gsk_sl_type_vector_get_scalar_type,
  gsk_sl_type_vector_get_index_type,
  gsk_sl_type_vector_get_length,
  gsk_sl_type_vector_get_size,
  gsk_sl_type_vector_can_convert,
  gsk_sl_type_vector_write_spv,
  gsk_sl_type_vector_print_value,
  gsk_sl_type_vector_write_value_spv
};

/* MATRIX */

typedef struct _GskSlTypeMatrix GskSlTypeMatrix;

struct _GskSlTypeMatrix {
  GskSlType parent;

  const char *name;
  GskSlScalarType scalar;
  guint columns;
  guint rows;
};

static void
gsk_sl_type_matrix_free (GskSlType *type)
{
  g_assert_not_reached ();
}

static const char *
gsk_sl_type_matrix_get_name (const GskSlType *type)
{
  const GskSlTypeMatrix *matrix = (const GskSlTypeMatrix *) type;

  return matrix->name;
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

static gsize
gsk_sl_type_matrix_get_size (const GskSlType *type)
{
  const GskSlTypeMatrix *matrix = (const GskSlTypeMatrix *) type;

  return matrix->columns * matrix->rows * scalar_infos[matrix->scalar].size;
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

static guint32
gsk_sl_type_matrix_write_spv (GskSlType    *type,
                              GskSpvWriter *writer)
{
  GskSlTypeMatrix *matrix = (GskSlTypeMatrix *) type;
  guint32 result_id, vector_id;

  vector_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_index_type (type));
  result_id = gsk_spv_writer_next_id (writer);
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_DECLARE,
                      4, GSK_SPV_OP_TYPE_MATRIX,
                      (guint32[3]) { result_id,
                                     vector_id,
                                     matrix->columns });
  
  return result_id;
}

static void
gsk_sl_type_matrix_print_value (const GskSlType *type,
                                GString         *string,
                                gconstpointer    value)
{
  const GskSlTypeMatrix *matrix = (const GskSlTypeMatrix *) type;
  guint i;
  const guchar *data;

  data = value;

  g_string_append (string, matrix->name);
  g_string_append (string, "(");
  for (i = 0; i < matrix->rows * matrix->columns; i++)
    {
      if (i > 0)
        g_string_append (string, ", ");
      scalar_infos[matrix->scalar].print_value (string, data);
      data += scalar_infos[matrix->scalar].size;
    }

  g_string_append (string, ")");
}

static guint32
gsk_sl_type_matrix_write_value_spv (GskSlType     *type,
                                    GskSpvWriter  *writer,
                                    gconstpointer  value)
{
  GskSlTypeMatrix *matrix = (GskSlTypeMatrix *) type;
  guint32 ids[matrix->rows + 2];
  GskSlType *vector_type;
  GskSlValue *v;
  const guchar *data;
  guint i;

  data = value;
  vector_type = gsk_sl_type_get_index_type (type);

  ids[0] = gsk_spv_writer_get_id_for_type (writer, type);
  for (i = 0; i < matrix->columns; i++)
    {
      v = gsk_sl_value_new_for_data (vector_type, (gpointer) data, NULL, NULL);
      ids[2 + i] = gsk_spv_writer_get_id_for_value (writer, v);
      gsk_sl_value_free (v);
      data += gsk_sl_type_get_size (vector_type);
    }
  ids[1] = gsk_spv_writer_next_id (writer);

  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_DECLARE,
                      3 + matrix->columns,
                      GSK_SPV_OP_CONSTANT_COMPOSITE,
                      ids);
  
  return ids[1];
}

static const GskSlTypeClass GSK_SL_TYPE_MATRIX = {
  gsk_sl_type_matrix_free,
  gsk_sl_type_matrix_get_name,
  gsk_sl_type_matrix_get_scalar_type,
  gsk_sl_type_matrix_get_index_type,
  gsk_sl_type_matrix_get_length,
  gsk_sl_type_matrix_get_size,
  gsk_sl_type_matrix_can_convert,
  gsk_sl_type_matrix_write_spv,
  gsk_sl_type_matrix_print_value,
  gsk_sl_type_matrix_write_value_spv
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
    [GSK_SL_FLOAT] = { { &GSK_SL_TYPE_VECTOR, 1 }, "vec2", GSK_SL_FLOAT, 2 },
    [GSK_SL_DOUBLE] = { { &GSK_SL_TYPE_VECTOR, 1 }, "dvec2", GSK_SL_DOUBLE, 2 },
    [GSK_SL_INT] = { { &GSK_SL_TYPE_VECTOR, 1 }, "ivec2", GSK_SL_INT, 2 },
    [GSK_SL_UINT] = { { &GSK_SL_TYPE_VECTOR, 1 }, "uvec2", GSK_SL_UINT, 2 },
    [GSK_SL_BOOL] = { { &GSK_SL_TYPE_VECTOR, 1 }, "bvec2", GSK_SL_BOOL, 2 },
  },
  {
    [GSK_SL_FLOAT] = { { &GSK_SL_TYPE_VECTOR, 1 }, "vec3", GSK_SL_FLOAT, 3 },
    [GSK_SL_DOUBLE] = { { &GSK_SL_TYPE_VECTOR, 1 }, "dvec3", GSK_SL_DOUBLE, 3 },
    [GSK_SL_INT] = { { &GSK_SL_TYPE_VECTOR, 1 }, "ivec3", GSK_SL_INT, 3 },
    [GSK_SL_UINT] = { { &GSK_SL_TYPE_VECTOR, 1 }, "uvec3", GSK_SL_UINT, 3 },
    [GSK_SL_BOOL] = { { &GSK_SL_TYPE_VECTOR, 1 }, "bvec3", GSK_SL_BOOL, 3 },
  },
  {
    [GSK_SL_FLOAT] = { { &GSK_SL_TYPE_VECTOR, 1 }, "vec4", GSK_SL_FLOAT, 4 },
    [GSK_SL_DOUBLE] = { { &GSK_SL_TYPE_VECTOR, 1 }, "dvec4", GSK_SL_DOUBLE, 4 },
    [GSK_SL_INT] = { { &GSK_SL_TYPE_VECTOR, 1 }, "ivec4", GSK_SL_INT, 4 },
    [GSK_SL_UINT] = { { &GSK_SL_TYPE_VECTOR, 1 }, "uvec4", GSK_SL_UINT, 4 },
    [GSK_SL_BOOL] = { { &GSK_SL_TYPE_VECTOR, 1 }, "bvec4", GSK_SL_BOOL, 4 },
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
      { { &GSK_SL_TYPE_MATRIX, 1 }, "mat2",    GSK_SL_FLOAT,  2, 2 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, "dmat2",   GSK_SL_DOUBLE, 2, 2 }
    },
    {
      { { &GSK_SL_TYPE_MATRIX, 1 }, "mat2x3",  GSK_SL_FLOAT,  2, 3 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, "dmat2x3", GSK_SL_DOUBLE, 2, 3 }
    },
    {
      { { &GSK_SL_TYPE_MATRIX, 1 }, "mat2x4",  GSK_SL_FLOAT,  2, 4 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, "dmat2x4", GSK_SL_DOUBLE, 2, 4 }
    },
  },
  {
    {
      { { &GSK_SL_TYPE_MATRIX, 1 }, "mat3x2",  GSK_SL_FLOAT,  3, 2 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, "dmat3x2", GSK_SL_DOUBLE, 3, 2 }
    },
    {
      { { &GSK_SL_TYPE_MATRIX, 1 }, "mat3",    GSK_SL_FLOAT,  3, 3 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, "dmat3",   GSK_SL_DOUBLE, 3, 3 }
    },
    {
      { { &GSK_SL_TYPE_MATRIX, 1 }, "mat3x4",  GSK_SL_FLOAT,  3, 4 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, "dmat3x4", GSK_SL_DOUBLE, 3, 4 }
    },
  },
  {
    {
      { { &GSK_SL_TYPE_MATRIX, 1 }, "mat4x2",  GSK_SL_FLOAT,  4, 2 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, "dmat4x2", GSK_SL_DOUBLE, 4, 2 }
    },
    {
      { { &GSK_SL_TYPE_MATRIX, 1 }, "mat4x3",  GSK_SL_FLOAT,  4, 3 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, "dmat4x3", GSK_SL_DOUBLE, 4, 3 }
    },
    {
      { { &GSK_SL_TYPE_MATRIX, 1 }, "mat4",    GSK_SL_FLOAT,  4, 4 },
      { { &GSK_SL_TYPE_MATRIX, 1 }, "dmat4",   GSK_SL_DOUBLE, 4, 4 }
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

const char *
gsk_sl_type_get_name (const GskSlType *type)
{
  return type->class->get_name (type);
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

guint
gsk_sl_type_get_length (const GskSlType *type)
{
  return type->class->get_length (type);
}

gsize
gsk_sl_type_get_size (const GskSlType *type)
{
  return type->class->get_size (type);
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

gboolean
gsk_sl_type_equal (gconstpointer a,
                   gconstpointer b)
{
  return a == b;
}

guint
gsk_sl_type_hash (gconstpointer type)
{
  return GPOINTER_TO_UINT (type);
}

guint32
gsk_sl_type_write_spv (GskSlType    *type,
                       GskSpvWriter *writer)
{
  return type->class->write_spv (type, writer);
}

void
gsk_sl_type_print_value (const GskSlType *type,
                         GString         *string,
                         gconstpointer    value)
{
  type->class->print_value (type, string, value);
}

guint32
gsk_sl_type_write_value_spv (GskSlType     *type,
                             GskSpvWriter  *writer,
                             gconstpointer  value)
{
  return type->class->write_value_spv (type, writer, value);
}
