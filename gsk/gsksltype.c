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

#include "gskslexpressionprivate.h"
#include "gskslfunctionprivate.h"
#include "gskslimagetypeprivate.h"
#include "gskslpreprocessorprivate.h"
#include "gskslprinterprivate.h"
#include "gskslscopeprivate.h"
#include "gsksltokenizerprivate.h"
#include "gskslvalueprivate.h"
#include "gskspvwriterprivate.h"

#include <string.h>

#define N_SCALAR_TYPES 6

typedef struct _GskSlTypeMember GskSlTypeMember;
typedef struct _GskSlTypeClass GskSlTypeClass;

struct _GskSlTypeMember {
  GskSlType *type;
  char *name;
  gsize offset;
};

struct _GskSlType
{
  const GskSlTypeClass *class;

  int ref_count;
};

struct _GskSlTypeClass {
  void                  (* free)                                (GskSlType           *type);
  const char *          (* get_name)                            (const GskSlType     *type);
  GskSlScalarType       (* get_scalar_type)                     (const GskSlType     *type);
  const GskSlImageType *(* get_image_type)                      (const GskSlType     *type);
  GskSlType *           (* get_index_type)                      (const GskSlType     *type);
  gsize                 (* get_index_stride)                    (const GskSlType     *type);
  guint                 (* get_length)                          (const GskSlType     *type);
  gsize                 (* get_size)                            (const GskSlType     *type);
  gsize                 (* get_n_components)                    (const GskSlType     *type);
  guint                 (* get_n_members)                       (const GskSlType     *type);
  const GskSlTypeMember * (* get_member)                        (const GskSlType     *type,
                                                                 guint                i);
  gboolean              (* can_convert)                         (const GskSlType     *target,
                                                                 const GskSlType     *source);
  guint32               (* write_spv)                           (GskSlType           *type,
                                                                 GskSpvWriter        *writer);
  void                  (* print_value)                         (const GskSlType     *type,
                                                                 GskSlPrinter        *printer,
                                                                 gconstpointer        value);
  gboolean              (* value_equal)                         (const GskSlType     *type,
                                                                 gconstpointer        a,
                                                                 gconstpointer        b);
  guint32               (* write_value_spv)                     (GskSlType           *type,
                                                                 GskSpvWriter        *writer,
                                                                 gconstpointer        value);
};

static void
print_void (GskSlPrinter  *printer,
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
print_float (GskSlPrinter  *printer,
             gconstpointer  value)
{
  const gfloat *f = value;
      
  gsk_sl_printer_append_float (printer, *f);
}

static guint32
write_float_spv (GskSpvWriter  *writer,
                 gconstpointer  value)
{
  return gsk_spv_writer_constant (writer,
                                  gsk_sl_type_get_scalar (GSK_SL_FLOAT),
                                  (guint32 *) value,
                                  1);
}

static void
print_double (GskSlPrinter  *printer,
              gconstpointer  value)
{
  const gdouble *d = value;
      
  gsk_sl_printer_append_double (printer, *d);
}

static guint32
write_double_spv (GskSpvWriter  *writer,
                  gconstpointer  value)
{
  return gsk_spv_writer_constant (writer,
                                  gsk_sl_type_get_scalar (GSK_SL_DOUBLE),
                                  (guint32 *) value,
                                  2);
}

static void
print_int (GskSlPrinter  *printer,
           gconstpointer  value)
{
  const gint32 *i = value;

  gsk_sl_printer_append_int (printer, *i);
}

static guint32
write_int_spv (GskSpvWriter  *writer,
               gconstpointer  value)
{
  return gsk_spv_writer_constant (writer,
                                  gsk_sl_type_get_scalar (GSK_SL_INT),
                                  (guint32 *) value,
                                  1);
}

static void
print_uint (GskSlPrinter  *printer,
            gconstpointer  value)
{
  const guint32 *u = value;
  
  gsk_sl_printer_append_uint (printer, *u);
  gsk_sl_printer_append_c (printer, 'u');
}

static guint32
write_uint_spv (GskSpvWriter  *writer,
                gconstpointer  value)
{
  return gsk_spv_writer_constant (writer,
                                  gsk_sl_type_get_scalar (GSK_SL_UINT),
                                  (guint32 *) value,
                                  1);
}

static void
print_bool (GskSlPrinter  *printer,
            gconstpointer  value)
{
  const guint32 *u = value;
  
  gsk_sl_printer_append (printer, *u ? "true" : "false");
}

static guint32
write_bool_spv (GskSpvWriter  *writer,
                gconstpointer  value)
{
  if (*(const guint32 *) value)
    return gsk_spv_writer_constant_true (writer, 
                                         gsk_sl_type_get_scalar (GSK_SL_BOOL));
  else
    return gsk_spv_writer_constant_false (writer, 
                                          gsk_sl_type_get_scalar (GSK_SL_BOOL));
}

#define SIMPLE_CONVERSION(source_name, target_name, source_type, target_type) \
static void \
source_name ## _to_ ## target_name (gpointer target, gconstpointer source) \
{ \
  *(target_type *) target = *(const source_type *) source; \
}

SIMPLE_CONVERSION(float, float, float, float);
SIMPLE_CONVERSION(float, double, float, double);
SIMPLE_CONVERSION(float, int, float, gint32);
SIMPLE_CONVERSION(float, uint, float, guint32);
static void
float_to_bool (gpointer target, gconstpointer source)
{
  *(guint32 *) target = *(const float *) source ? TRUE : FALSE;
}

SIMPLE_CONVERSION(double, float, double, float);
SIMPLE_CONVERSION(double, double, double, double);
SIMPLE_CONVERSION(double, int, double, gint32);
SIMPLE_CONVERSION(double, uint, double, guint32);
static void
double_to_bool (gpointer target, gconstpointer source)
{
  *(guint32 *) target = *(const double *) source ? TRUE : FALSE;
}

SIMPLE_CONVERSION(int, float, gint32, float);
SIMPLE_CONVERSION(int, double, gint32, double);
SIMPLE_CONVERSION(int, int, gint32, gint32);
SIMPLE_CONVERSION(int, uint, gint32, guint32);
static void
int_to_bool (gpointer target, gconstpointer source)
{
  *(guint32 *) target = *(const gint32 *) source ? TRUE : FALSE;
}

SIMPLE_CONVERSION(uint, float, guint32, float);
SIMPLE_CONVERSION(uint, double, guint32, double);
SIMPLE_CONVERSION(uint, int, guint32, gint32);
SIMPLE_CONVERSION(uint, uint, guint32, guint32);
static void
uint_to_bool (gpointer target, gconstpointer source)
{
  *(guint32 *) target = *(const guint32 *) source ? TRUE : FALSE;
}

static void
bool_to_float (gpointer target, gconstpointer source)
{
  *(float *) target = *(const guint32 *) source ? 1.0 : 0.0;
}

static void
bool_to_double (gpointer target, gconstpointer source)
{
  *(double *) target = *(const guint32 *) source ? 1.0 : 0.0;
}

static void
bool_to_int (gpointer target, gconstpointer source)
{
  *(gint32 *) target = *(const guint32 *) source ? 1 : 0;
}

static void
bool_to_uint (gpointer target, gconstpointer source)
{
  *(guint32 *) target = *(const guint32 *) source ? 1 : 0;
}

static void
bool_to_bool (gpointer target, gconstpointer source)
{
  *(guint32 *) target = *(const guint32 *) source;
}

static gboolean
void_equal (gconstpointer a, gconstpointer b)
{
  return FALSE;
}

static gboolean
int_equal (gconstpointer a, gconstpointer b)
{
  return *(const gint32 *) a == *(const gint32 *) b;
}

static gboolean
uint_equal (gconstpointer a, gconstpointer b)
{
  return *(const guint32 *) a == *(const guint32 *) b;
}

static gboolean
float_equal (gconstpointer a, gconstpointer b)
{
  return *(const float *) a == *(const float *) b;
}

static gboolean
double_equal (gconstpointer a, gconstpointer b)
{
  return *(const double *) a == *(const double *) b;
}

static gboolean
bool_equal (gconstpointer a, gconstpointer b)
{
  return !*(const guint32 *) a == !*(const guint32 *) b;
}

#define CONVERSIONS(name) { NULL, name ## _to_float, name ## _to_double, name ## _to_int, name ## _to_uint, name ## _to_bool }
struct {
  const char *name;
  gsize size;
  void (* print_value) (GskSlPrinter *printer, gconstpointer value);
  gboolean (* value_equal) (gconstpointer a, gconstpointer b);
  void (* convert_value[N_SCALAR_TYPES]) (gpointer target, gconstpointer source);
  guint32 (* write_value_spv) (GskSpvWriter *writer, gconstpointer value);
} scalar_infos[] = {
  [GSK_SL_VOID] =   { "void",   0, print_void,   void_equal,   { NULL, },            write_void_spv, },
  [GSK_SL_FLOAT] =  { "float",  4, print_float,  float_equal,  CONVERSIONS (float),  write_float_spv },
  [GSK_SL_DOUBLE] = { "double", 8, print_double, double_equal, CONVERSIONS (double), write_double_spv },
  [GSK_SL_INT] =    { "int",    4, print_int,    int_equal,    CONVERSIONS (int),    write_int_spv },
  [GSK_SL_UINT] =   { "uint",   4, print_uint,   uint_equal,   CONVERSIONS (uint),   write_uint_spv },
  [GSK_SL_BOOL] =   { "bool",   4, print_bool,   bool_equal,   CONVERSIONS (bool),   write_bool_spv }
};
#undef SIMPLE_CONVERSION
#undef CONVERSIONS

static GskSlType *
gsk_sl_type_alloc (const GskSlTypeClass *klass,
                   gsize                 size)
{
  GskSlType *type;

  type = g_slice_alloc0 (size);

  type->class = klass;
  type->ref_count = 1;

  return type;
}
#define gsk_sl_type_new(_name, _klass) ((_name *) gsk_sl_type_alloc ((_klass), sizeof (_name)))

/* VOID */

typedef struct _GskSlTypeVoid GskSlTypeVoid;

struct _GskSlTypeVoid {
  GskSlType parent;
};

static void
gsk_sl_type_void_free (GskSlType *type)
{
  g_assert_not_reached ();
}

static const char *
gsk_sl_type_void_get_name (const GskSlType *type)
{
  return "void";
}

static GskSlScalarType
gsk_sl_type_void_get_scalar_type (const GskSlType *type)
{
  return GSK_SL_VOID;
}

static const GskSlImageType *
gsk_sl_type_void_get_image_type (const GskSlType *type)
{
  return NULL;
}

static GskSlType *
gsk_sl_type_void_get_index_type (const GskSlType *type)
{
  return NULL;
}

static gsize
gsk_sl_type_void_get_index_stride (const GskSlType *type)
{
  return 0;
}

static guint
gsk_sl_type_void_get_length (const GskSlType *type)
{
  return 0;
}

static gsize
gsk_sl_type_void_get_size (const GskSlType *type)
{
  return 0;
}

static gsize
gsk_sl_type_void_get_n_components (const GskSlType *type)
{
  return 0;
}

static guint
gsk_sl_type_void_get_n_members (const GskSlType *type)
{
  return 0;
}

static const GskSlTypeMember *
gsk_sl_type_void_get_member (const GskSlType *type,
                             guint            n)
{
  return NULL;
}

static gboolean
gsk_sl_type_void_can_convert (const GskSlType *target,
                              const GskSlType *source)
{
  return FALSE;
}

static guint32
gsk_sl_type_void_write_spv (GskSlType    *type,
                            GskSpvWriter *writer)
{
  return gsk_spv_writer_type_void (writer);
}

static void
gsk_sl_type_void_print_value (const GskSlType *type,
                              GskSlPrinter    *printer,
                              gconstpointer    value)
{
  g_assert_not_reached ();
}

static gboolean
gsk_sl_type_void_value_equal (const GskSlType *type,
                              gconstpointer    a,
                              gconstpointer    b)
{
  return FALSE;
}

static guint32
gsk_sl_type_void_write_value_spv (GskSlType     *type,
                                  GskSpvWriter  *writer,
                                  gconstpointer  value)
{
  g_assert_not_reached ();

  return 0;
}

static const GskSlTypeClass GSK_SL_TYPE_VOID = {
  gsk_sl_type_void_free,
  gsk_sl_type_void_get_name,
  gsk_sl_type_void_get_scalar_type,
  gsk_sl_type_void_get_image_type,
  gsk_sl_type_void_get_index_type,
  gsk_sl_type_void_get_index_stride,
  gsk_sl_type_void_get_length,
  gsk_sl_type_void_get_size,
  gsk_sl_type_void_get_n_components,
  gsk_sl_type_void_get_n_members,
  gsk_sl_type_void_get_member,
  gsk_sl_type_void_can_convert,
  gsk_sl_type_void_write_spv,
  gsk_sl_type_void_print_value,
  gsk_sl_type_void_value_equal,
  gsk_sl_type_void_write_value_spv
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

static const GskSlImageType *
gsk_sl_type_scalar_get_image_type (const GskSlType *type)
{
  return NULL;
}

static GskSlType *
gsk_sl_type_scalar_get_index_type (const GskSlType *type)
{
  return NULL;
}

static gsize
gsk_sl_type_scalar_get_index_stride (const GskSlType *type)
{
  return 0;
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

static gsize
gsk_sl_type_scalar_get_n_components (const GskSlType *type)
{
  return 1;
}

static guint
gsk_sl_type_scalar_get_n_members (const GskSlType *type)
{
  return 0;
}

static const GskSlTypeMember *
gsk_sl_type_scalar_get_member (const GskSlType *type,
                               guint            n)
{
  return NULL;
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

  switch (scalar->scalar)
  {
    case GSK_SL_FLOAT:
      return gsk_spv_writer_type_float (writer, 32);

    case GSK_SL_DOUBLE:
      return gsk_spv_writer_type_float (writer, 64);

    case GSK_SL_INT:
      return gsk_spv_writer_type_int (writer, 32, 1);

    case GSK_SL_UINT:
      return gsk_spv_writer_type_int (writer, 32, 0);

    case GSK_SL_BOOL:
      return gsk_spv_writer_type_bool (writer);

    case GSK_SL_VOID:
    default:
      g_assert_not_reached ();
      return 0;
  }
}

static void
gsk_sl_type_scalar_print_value (const GskSlType *type,
                                GskSlPrinter    *printer,
                                gconstpointer    value)
{
  const GskSlTypeScalar *scalar = (const GskSlTypeScalar *) type;

  scalar_infos[scalar->scalar].print_value (printer, value);
}

static gboolean
gsk_sl_type_scalar_value_equal (const GskSlType *type,
                                gconstpointer    a,
                                gconstpointer    b)
{
  GskSlTypeScalar *scalar = (GskSlTypeScalar *) type;

  return scalar_infos[scalar->scalar].value_equal (a, b);
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
  gsk_sl_type_scalar_get_image_type,
  gsk_sl_type_scalar_get_index_type,
  gsk_sl_type_scalar_get_index_stride,
  gsk_sl_type_scalar_get_length,
  gsk_sl_type_scalar_get_size,
  gsk_sl_type_scalar_get_n_components,
  gsk_sl_type_scalar_get_n_members,
  gsk_sl_type_scalar_get_member,
  gsk_sl_type_scalar_can_convert,
  gsk_sl_type_scalar_write_spv,
  gsk_sl_type_scalar_print_value,
  gsk_sl_type_scalar_value_equal,
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

static const GskSlImageType *
gsk_sl_type_vector_get_image_type (const GskSlType *type)
{
  return NULL;
}

static GskSlType *
gsk_sl_type_vector_get_index_type (const GskSlType *type)
{
  const GskSlTypeVector *vector = (const GskSlTypeVector *) type;

  return gsk_sl_type_get_scalar (vector->scalar);
}

static gsize
gsk_sl_type_vector_get_index_stride (const GskSlType *type)
{
  const GskSlTypeVector *vector = (const GskSlTypeVector *) type;

  return scalar_infos[vector->scalar].size;
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

static gsize
gsk_sl_type_vector_get_n_components (const GskSlType *type)
{
  const GskSlTypeVector *vector = (const GskSlTypeVector *) type;

  return vector->length;
}

static guint
gsk_sl_type_vector_get_n_members (const GskSlType *type)
{
  return 0;
}

static const GskSlTypeMember *
gsk_sl_type_vector_get_member (const GskSlType *type,
                               guint            n)
{
  return NULL;
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

  return gsk_spv_writer_type_vector (writer,
                                     gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_scalar (vector->scalar)),
                                     vector->length);
}

static void
gsk_sl_type_vector_print_value (const GskSlType *type,
                                GskSlPrinter    *printer,
                                gconstpointer    value)
{
  const GskSlTypeVector *vector = (const GskSlTypeVector *) type;
  guint i;
  const guchar *data;

  data = value;

  gsk_sl_printer_append (printer, vector->name);
  gsk_sl_printer_append (printer, "(");
  for (i = 0; i < vector->length; i++)
    {
      if (i > 0)
        gsk_sl_printer_append (printer, ", ");
      scalar_infos[vector->scalar].print_value (printer, data);
      data += scalar_infos[vector->scalar].size;
    }

  gsk_sl_printer_append (printer, ")");
}

static gboolean
gsk_sl_type_vector_value_equal (const GskSlType *type,
                                gconstpointer    a,
                                gconstpointer    b)
{
  GskSlTypeVector *vector = (GskSlTypeVector *) type;
  guint i;
  const guchar *adata, *bdata;

  adata = a;
  bdata = b;

  for (i = 0; i < vector->length; i++)
    {
      if (!scalar_infos[vector->scalar].value_equal (adata, bdata))
        return FALSE;
      adata += scalar_infos[vector->scalar].size;
      bdata += scalar_infos[vector->scalar].size;
    }

  return TRUE;
}

static guint32
gsk_sl_type_vector_write_value_spv (GskSlType     *type,
                                    GskSpvWriter  *writer,
                                    gconstpointer  value)
{
  GskSlTypeVector *vector = (GskSlTypeVector *) type;
  guint32 ids[vector->length];
  GskSlType *scalar_type;
  GskSlValue *v;
  const guchar *data;
  guint i;

  data = value;
  scalar_type = gsk_sl_type_get_scalar (vector->scalar);

  for (i = 0; i < vector->length; i++)
    {
      v = gsk_sl_value_new_for_data (scalar_type, (gpointer) data, NULL, NULL);
      ids[i] = gsk_spv_writer_get_id_for_value (writer, v);
      gsk_sl_value_free (v);
      data += scalar_infos[vector->scalar].size;
    }

  return gsk_spv_writer_constant_composite (writer,
                                            type,
                                            ids,
                                            vector->length);
}

static const GskSlTypeClass GSK_SL_TYPE_VECTOR = {
  gsk_sl_type_vector_free,
  gsk_sl_type_vector_get_name,
  gsk_sl_type_vector_get_scalar_type,
  gsk_sl_type_vector_get_image_type,
  gsk_sl_type_vector_get_index_type,
  gsk_sl_type_vector_get_index_stride,
  gsk_sl_type_vector_get_length,
  gsk_sl_type_vector_get_size,
  gsk_sl_type_vector_get_n_components,
  gsk_sl_type_vector_get_n_members,
  gsk_sl_type_vector_get_member,
  gsk_sl_type_vector_can_convert,
  gsk_sl_type_vector_write_spv,
  gsk_sl_type_vector_print_value,
  gsk_sl_type_vector_value_equal,
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

static const GskSlImageType *
gsk_sl_type_matrix_get_image_type (const GskSlType *type)
{
  return NULL;
}

static GskSlType *
gsk_sl_type_matrix_get_index_type (const GskSlType *type)
{
  const GskSlTypeMatrix *matrix = (const GskSlTypeMatrix *) type;

  return gsk_sl_type_get_vector (matrix->scalar, matrix->rows);
}

static gsize
gsk_sl_type_matrix_get_index_stride (const GskSlType *type)
{
  const GskSlTypeMatrix *matrix = (const GskSlTypeMatrix *) type;

  return scalar_infos[matrix->scalar].size * matrix->rows;
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

static gsize
gsk_sl_type_matrix_get_n_components (const GskSlType *type)
{
  const GskSlTypeMatrix *matrix = (const GskSlTypeMatrix *) type;

  return matrix->columns * matrix->rows;
}

static guint
gsk_sl_type_matrix_get_n_members (const GskSlType *type)
{
  return 0;
}

static const GskSlTypeMember *
gsk_sl_type_matrix_get_member (const GskSlType *type,
                               guint            n)
{
  return NULL;
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

  return gsk_spv_writer_type_matrix (writer,
                                     gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_index_type (type)),
                                     matrix->columns);
}

static void
gsk_sl_type_matrix_print_value (const GskSlType *type,
                                GskSlPrinter    *printer,
                                gconstpointer    value)
{
  const GskSlTypeMatrix *matrix = (const GskSlTypeMatrix *) type;
  guint i;
  const guchar *data;

  data = value;

  gsk_sl_printer_append (printer, matrix->name);
  gsk_sl_printer_append (printer, "(");
  for (i = 0; i < matrix->rows * matrix->columns; i++)
    {
      if (i > 0)
        gsk_sl_printer_append (printer, ", ");
      scalar_infos[matrix->scalar].print_value (printer, data);
      data += scalar_infos[matrix->scalar].size;
    }

  gsk_sl_printer_append (printer, ")");
}

static gboolean
gsk_sl_type_matrix_value_equal (const GskSlType *type,
                                gconstpointer    a,
                                gconstpointer    b)
{
  GskSlTypeMatrix *matrix = (GskSlTypeMatrix *) type;
  guint i;
  const guchar *adata, *bdata;

  adata = a;
  bdata = b;

  for (i = 0; i < matrix->rows * matrix->columns; i++)
    {
      if (!scalar_infos[matrix->scalar].value_equal (adata, bdata))
        return FALSE;
      adata += scalar_infos[matrix->scalar].size;
      bdata += scalar_infos[matrix->scalar].size;
    }

  return TRUE;
}

static guint32
gsk_sl_type_matrix_write_value_spv (GskSlType     *type,
                                    GskSpvWriter  *writer,
                                    gconstpointer  value)
{
  GskSlTypeMatrix *matrix = (GskSlTypeMatrix *) type;
  guint32 ids[matrix->rows];
  GskSlType *vector_type;
  GskSlValue *v;
  const guchar *data;
  guint i;

  data = value;
  vector_type = gsk_sl_type_get_index_type (type);

  for (i = 0; i < matrix->columns; i++)
    {
      v = gsk_sl_value_new_for_data (vector_type, (gpointer) data, NULL, NULL);
      ids[i] = gsk_spv_writer_get_id_for_value (writer, v);
      gsk_sl_value_free (v);
      data += gsk_sl_type_get_size (vector_type);
    }

  return gsk_spv_writer_constant_composite (writer,
                                            type,
                                            ids,
                                            matrix->columns);
}

static const GskSlTypeClass GSK_SL_TYPE_MATRIX = {
  gsk_sl_type_matrix_free,
  gsk_sl_type_matrix_get_name,
  gsk_sl_type_matrix_get_scalar_type,
  gsk_sl_type_matrix_get_image_type,
  gsk_sl_type_matrix_get_index_type,
  gsk_sl_type_matrix_get_index_stride,
  gsk_sl_type_matrix_get_length,
  gsk_sl_type_matrix_get_size,
  gsk_sl_type_matrix_get_n_components,
  gsk_sl_type_matrix_get_n_members,
  gsk_sl_type_matrix_get_member,
  gsk_sl_type_matrix_can_convert,
  gsk_sl_type_matrix_write_spv,
  gsk_sl_type_matrix_print_value,
  gsk_sl_type_matrix_value_equal,
  gsk_sl_type_matrix_write_value_spv
};

/* ARRAY */

typedef struct _GskSlTypeArray GskSlTypeArray;

struct _GskSlTypeArray {
  GskSlType parent;

  char *name;
  GskSlType *type;
  gsize length;
};

static void
gsk_sl_type_array_free (GskSlType *type)
{
  GskSlTypeArray *array = (GskSlTypeArray *) type;

  gsk_sl_type_unref (array->type);
  g_free (array->name);

  g_slice_free (GskSlTypeArray, array);
}

static const char *
gsk_sl_type_array_get_name (const GskSlType *type)
{
  const GskSlTypeArray *array = (const GskSlTypeArray *) type;

  return array->name;
}

static GskSlScalarType
gsk_sl_type_array_get_scalar_type (const GskSlType *type)
{
  const GskSlTypeArray *array = (const GskSlTypeArray *) type;

  return gsk_sl_type_get_scalar_type (array->type);
}

static const GskSlImageType *
gsk_sl_type_array_get_image_type (const GskSlType *type)
{
  return NULL;
}

static GskSlType *
gsk_sl_type_array_get_index_type (const GskSlType *type)
{
  const GskSlTypeArray *array = (const GskSlTypeArray *) type;

  return array->type;
}

static gsize
gsk_sl_type_array_get_index_stride (const GskSlType *type)
{
  const GskSlTypeArray *array = (const GskSlTypeArray *) type;

  return gsk_sl_type_get_size (array->type);
}

static guint
gsk_sl_type_array_get_length (const GskSlType *type)
{
  const GskSlTypeArray *array = (const GskSlTypeArray *) type;

  return array->length;
}

static gsize
gsk_sl_type_array_get_size (const GskSlType *type)
{
  const GskSlTypeArray *array = (const GskSlTypeArray *) type;

  return array->length * gsk_sl_type_get_size (array->type);
}

static gsize
gsk_sl_type_array_get_n_components (const GskSlType *type)
{
  const GskSlTypeArray *array = (const GskSlTypeArray *) type;

  return gsk_sl_type_array_get_n_components (array->type) * array->length;
}

static guint
gsk_sl_type_array_get_n_members (const GskSlType *type)
{
  return 0;
}

static const GskSlTypeMember *
gsk_sl_type_array_get_member (const GskSlType *type,
                               guint            n)
{
  return NULL;
}

static gboolean
gsk_sl_type_array_can_convert (const GskSlType *target,
                               const GskSlType *source)
{
  return gsk_sl_type_equal (target, source);
}

static guint32
gsk_sl_type_array_write_spv (GskSlType    *type,
                             GskSpvWriter *writer)
{
  GskSlTypeArray *array = (GskSlTypeArray *) type;
  GskSlValue *value;
  guint32 type_id, length_id;

  type_id = gsk_spv_writer_get_id_for_type (writer, array->type);
  value = gsk_sl_value_new (gsk_sl_type_get_scalar (GSK_SL_INT));
  *(gint32 *) gsk_sl_value_get_data (value) = array->length;
  length_id = gsk_spv_writer_get_id_for_value (writer, value);
  gsk_sl_value_free (value);

  return gsk_spv_writer_type_array (writer,
                                    type_id,
                                    length_id);
}

static void
gsk_sl_type_array_print_value (const GskSlType *type,
                               GskSlPrinter    *printer,
                               gconstpointer    value)
{
  GskSlTypeArray *array = (GskSlTypeArray *) type;
  gsize i, stride;
  const guchar *data;

  data = value;
  stride = gsk_sl_type_array_get_index_stride (type);
  gsk_sl_printer_append (printer, array->name);
  gsk_sl_printer_append (printer, " (");

  for (i = 0; i < array->length; i++)
    {
      if (i > 0)
        gsk_sl_printer_append (printer, ", ");
      gsk_sl_type_print_value (array->type, printer, data + i * stride);
    }

  gsk_sl_printer_append (printer, ")");
}

static gboolean
gsk_sl_type_array_value_equal (const GskSlType *type,
                               gconstpointer    a,
                               gconstpointer    b)
{
  GskSlTypeArray *array = (GskSlTypeArray *) type;
  gsize i, stride;
  const guchar *adata;
  const guchar *bdata;

  adata = a;
  bdata = b;
  stride = gsk_sl_type_array_get_index_stride (type);
  for (i = 0; i < array->length; i++)
    {
      if (!gsk_sl_type_value_equal (array->type, adata + i * stride, bdata + i * stride))
        return FALSE;
    }

  return TRUE;
}

static guint32
gsk_sl_type_array_write_value_spv (GskSlType     *type,
                                   GskSpvWriter  *writer,
                                   gconstpointer  value)
{
  GskSlTypeArray *array = (GskSlTypeArray *) type;
  guint32 value_ids[array->length];
  gsize i, stride;
  const guchar *data;

  data = value;
  stride = gsk_sl_type_array_get_index_stride (type);

  for (i = 0; i < array->length; i++)
    {
      value_ids[i] = gsk_sl_type_write_value_spv (array->type, writer, data + i * stride);
    }

  return gsk_spv_writer_constant_composite (writer,
                                            type,
                                            value_ids,
                                            array->length);
}

static const GskSlTypeClass GSK_SL_TYPE_ARRAY = {
  gsk_sl_type_array_free,
  gsk_sl_type_array_get_name,
  gsk_sl_type_array_get_scalar_type,
  gsk_sl_type_array_get_image_type,
  gsk_sl_type_array_get_index_type,
  gsk_sl_type_array_get_index_stride,
  gsk_sl_type_array_get_length,
  gsk_sl_type_array_get_size,
  gsk_sl_type_array_get_n_components,
  gsk_sl_type_array_get_n_members,
  gsk_sl_type_array_get_member,
  gsk_sl_type_array_can_convert,
  gsk_sl_type_array_write_spv,
  gsk_sl_type_array_print_value,
  gsk_sl_type_array_value_equal,
  gsk_sl_type_array_write_value_spv
};

/* SAMPLER */

typedef struct _GskSlTypeSampler GskSlTypeSampler;

struct _GskSlTypeSampler {
  GskSlType parent;

  const char *name;
  GskSlSamplerType sampler;

  GskSlImageType image_type;
};

static void
gsk_sl_type_sampler_free (GskSlType *type)
{
  g_assert_not_reached ();
}

static const char *
gsk_sl_type_sampler_get_name (const GskSlType *type)
{
  const GskSlTypeSampler *sampler = (const GskSlTypeSampler *) type;

  return sampler->name;
}

static GskSlScalarType
gsk_sl_type_sampler_get_scalar_type (const GskSlType *type)
{
  return GSK_SL_VOID;
}

static const GskSlImageType *
gsk_sl_type_sampler_get_image_type (const GskSlType *type)
{
  const GskSlTypeSampler *sampler = (const GskSlTypeSampler *) type;

  return &sampler->image_type;
}

static GskSlType *
gsk_sl_type_sampler_get_index_type (const GskSlType *type)
{
  return NULL;
}

static gsize
gsk_sl_type_sampler_get_index_stride (const GskSlType *type)
{
  return 0;
}

static guint
gsk_sl_type_sampler_get_length (const GskSlType *type)
{
  return 0;
}

static gsize
gsk_sl_type_sampler_get_size (const GskSlType *type)
{
  /* XXX: setting this to 0 would make it not work in GskSlValue */
  return sizeof (gpointer);
}

static gsize
gsk_sl_type_sampler_get_n_components (const GskSlType *type)
{
  return 1;
}

static guint
gsk_sl_type_sampler_get_n_members (const GskSlType *type)
{
  return 0;
}

static const GskSlTypeMember *
gsk_sl_type_sampler_get_member (const GskSlType *type,
                               guint            n)
{
  return NULL;
}

static gboolean
gsk_sl_type_sampler_can_convert (const GskSlType *target,
                                 const GskSlType *source)
{
  return gsk_sl_type_equal (target, source);
}

static guint32
gsk_sl_type_sampler_write_spv (GskSlType    *type,
                               GskSpvWriter *writer)
{
  GskSlTypeSampler *sampler = (GskSlTypeSampler *) type;
  guint32 image_id;

  image_id = gsk_spv_writer_get_id_for_image_type (writer, &sampler->image_type);

  return gsk_spv_writer_type_sampled_image (writer, image_id);
}

static void
gsk_sl_type_sampler_print_value (const GskSlType *type,
                                 GskSlPrinter    *printer,
                                 gconstpointer    value)
{
  g_assert_not_reached ();
}

static gboolean
gsk_sl_type_sampler_value_equal (const GskSlType *type,
                                 gconstpointer    a,
                                 gconstpointer    b)
{
  return *(gconstpointer *) a == *(gconstpointer *) b;
}

static guint32
gsk_sl_type_sampler_write_value_spv (GskSlType     *type,
                                     GskSpvWriter  *writer,
                                     gconstpointer  value)
{
  g_assert_not_reached ();
}

static const GskSlTypeClass GSK_SL_TYPE_SAMPLER = {
  gsk_sl_type_sampler_free,
  gsk_sl_type_sampler_get_name,
  gsk_sl_type_sampler_get_scalar_type,
  gsk_sl_type_sampler_get_image_type,
  gsk_sl_type_sampler_get_index_type,
  gsk_sl_type_sampler_get_index_stride,
  gsk_sl_type_sampler_get_length,
  gsk_sl_type_sampler_get_size,
  gsk_sl_type_sampler_get_n_components,
  gsk_sl_type_sampler_get_n_members,
  gsk_sl_type_sampler_get_member,
  gsk_sl_type_sampler_can_convert,
  gsk_sl_type_sampler_write_spv,
  gsk_sl_type_sampler_print_value,
  gsk_sl_type_sampler_value_equal,
  gsk_sl_type_sampler_write_value_spv
};

/* STRUCT */

typedef struct _GskSlTypeStruct GskSlTypeStruct;

struct _GskSlTypeStruct {
  GskSlType parent;

  char *name;
  gsize size;

  GskSlTypeMember *members;
  guint n_members;
};

static void
gsk_sl_type_struct_free (GskSlType *type)
{
  GskSlTypeStruct *struc = (GskSlTypeStruct *) type;
  guint i;

  for (i = 0; i < struc->n_members; i++)
    {
      gsk_sl_type_unref (struc->members[i].type);
      g_free (struc->members[i].name);
    }

  g_free (struc->members);
  g_free (struc->name);

  g_slice_free (GskSlTypeStruct, struc);
}

static const char *
gsk_sl_type_struct_get_name (const GskSlType *type)
{
  const GskSlTypeStruct *struc = (const GskSlTypeStruct *) type;

  return struc->name;
}

static GskSlScalarType
gsk_sl_type_struct_get_scalar_type (const GskSlType *type)
{
  return GSK_SL_VOID;
}

static const GskSlImageType *
gsk_sl_type_struct_get_image_type (const GskSlType *type)
{
  return NULL;
}

static GskSlType *
gsk_sl_type_struct_get_index_type (const GskSlType *type)
{
  return NULL;
}

static gsize
gsk_sl_type_struct_get_index_stride (const GskSlType *type)
{
  return 0;
}

static guint
gsk_sl_type_struct_get_length (const GskSlType *type)
{
  return 0;
}

static gsize
gsk_sl_type_struct_get_size (const GskSlType *type)
{
  const GskSlTypeStruct *struc = (const GskSlTypeStruct *) type;

  return struc->size;
}

static gsize
gsk_sl_type_struct_get_n_components (const GskSlType *type)
{
  return 0;
}

static guint
gsk_sl_type_struct_get_n_members (const GskSlType *type)
{
  const GskSlTypeStruct *struc = (const GskSlTypeStruct *) type;

  return struc->n_members;
}

static const GskSlTypeMember *
gsk_sl_type_struct_get_member (const GskSlType *type,
                               guint            n)
{
  const GskSlTypeStruct *struc = (const GskSlTypeStruct *) type;

  return &struc->members[n];
}

static gboolean
gsk_sl_type_struct_can_convert (const GskSlType *target,
                                const GskSlType *source)
{
  return gsk_sl_type_equal (target, source);
}

static gboolean
gsk_sl_type_struct_has_name (const GskSlTypeStruct *struc)
{
  return !g_str_has_prefix (struc->name, "struct { ");
}

static void
gsk_sl_type_write_member_decoration (GskSpvWriter *writer,
                                     guint32       type_id,
                                     gsize         index,
                                     GskSlType    *member_type,
                                     const char   *member_name,
                                     gsize         member_offset)
{
  gsk_spv_writer_member_name (writer, type_id, index, member_name);

  gsk_spv_writer_member_decorate (writer, type_id, index,
                                  GSK_SPV_DECORATION_OFFSET,
                                  (guint32[1]) { member_offset }, 1);

  if (gsk_sl_type_is_matrix (member_type))
    {
      gsk_spv_writer_member_decorate (writer, type_id, index,
                                      GSK_SPV_DECORATION_COL_MAJOR,
                                      NULL, 0);
      gsk_spv_writer_member_decorate (writer, type_id, index,
                                      GSK_SPV_DECORATION_MATRIX_STRIDE,
                                      (guint32[1]) { gsk_sl_type_get_size (gsk_sl_type_get_index_type (member_type)) }, 1);
    }
}

static guint32
gsk_sl_type_struct_write_spv (GskSlType    *type,
                              GskSpvWriter *writer)
{
  GskSlTypeStruct *struc = (GskSlTypeStruct *) type;
  guint32 ids[struc->n_members];
  guint32 result_id;
  guint i;

  for (i = 0; i < struc->n_members; i++)
    {
      ids[i] = gsk_spv_writer_get_id_for_type (writer, struc->members[i].type);
    }

  result_id = gsk_spv_writer_type_struct (writer,
                                          ids,
                                          struc->n_members);

  
  if (gsk_sl_type_struct_has_name (struc))
    gsk_spv_writer_name (writer, result_id, struc->name);
  else
    gsk_spv_writer_name (writer, result_id, "");

  for (i = 0; i < struc->n_members; i++)
    {
      gsk_sl_type_write_member_decoration (writer, result_id, i, struc->members[i].type,
          struc->members[i].name, struc->members[i].offset);
    }

  return result_id;
}

static void
gsk_sl_type_struct_print_value (const GskSlType *type,
                                GskSlPrinter    *printer,
                                gconstpointer    value)
{
  const GskSlTypeStruct *struc = (const GskSlTypeStruct *) type;
  guint i;

  gsk_sl_printer_append (printer, struc->name);
  gsk_sl_printer_append (printer, "(");

  for (i = 0; i < struc->n_members; i++)
    {
      if (i > 0)
        gsk_sl_printer_append (printer, ", ");
      gsk_sl_type_print_value (struc->members[i].type,
                               printer,
                               (guchar *) value + struc->members[i].offset);
    }

  gsk_sl_printer_append (printer, ")");
}

static gboolean
gsk_sl_type_struct_value_equal (const GskSlType *type,
                                gconstpointer    a,
                                gconstpointer    b)
{
  GskSlTypeStruct *struc = (GskSlTypeStruct *) type;
  guint i;
  const guchar *adata, *bdata;

  adata = a;
  bdata = b;

  for (i = 0; i < struc->n_members; i++)
    {
      if (!gsk_sl_type_value_equal (struc->members[i].type,
                                    adata + struc->members[i].offset,
                                    bdata + struc->members[i].offset))
        return FALSE;
    }

  return TRUE;
}

static guint32
gsk_sl_type_struct_write_value_spv (GskSlType     *type,
                                    GskSpvWriter  *writer,
                                    gconstpointer  value)
{
  GskSlTypeStruct *struc = (GskSlTypeStruct *) type;
  guint32 ids[struc->n_members];
  GskSlValue *v;
  guint i;

  for (i = 0; i < struc->n_members; i++)
    {
      v = gsk_sl_value_new_for_data (struc->members[i].type,
                                     (guchar *) value + struc->members[i].offset,
                                     NULL, NULL);
      ids[i] = gsk_spv_writer_get_id_for_value (writer, v);
      gsk_sl_value_free (v);
    }

  return gsk_spv_writer_constant_composite (writer,
                                            type,
                                            ids,
                                            struc->n_members);
}

static const GskSlTypeClass GSK_SL_TYPE_STRUCT = {
  gsk_sl_type_struct_free,
  gsk_sl_type_struct_get_name,
  gsk_sl_type_struct_get_scalar_type,
  gsk_sl_type_struct_get_image_type,
  gsk_sl_type_struct_get_index_type,
  gsk_sl_type_struct_get_index_stride,
  gsk_sl_type_struct_get_length,
  gsk_sl_type_struct_get_size,
  gsk_sl_type_struct_get_n_components,
  gsk_sl_type_struct_get_n_members,
  gsk_sl_type_struct_get_member,
  gsk_sl_type_struct_can_convert,
  gsk_sl_type_struct_write_spv,
  gsk_sl_type_struct_print_value,
  gsk_sl_type_struct_value_equal,
  gsk_sl_type_struct_write_value_spv
};

/* BLOCK */

typedef struct _GskSlTypeBlock GskSlTypeBlock;

struct _GskSlTypeBlock {
  GskSlType parent;

  char *name;
  gsize size;

  GskSlTypeMember *members;
  guint n_members;
};

static void
gsk_sl_type_block_free (GskSlType *type)
{
  GskSlTypeBlock *block = (GskSlTypeBlock *) type;
  guint i;

  for (i = 0; i < block->n_members; i++)
    {
      gsk_sl_type_unref (block->members[i].type);
      g_free (block->members[i].name);
    }

  g_free (block->members);
  g_free (block->name);

  g_slice_free (GskSlTypeBlock, block);
}

static const char *
gsk_sl_type_block_get_name (const GskSlType *type)
{
  GskSlTypeBlock *block = (GskSlTypeBlock *) type;

  return block->name;
}

static GskSlScalarType
gsk_sl_type_block_get_scalar_type (const GskSlType *type)
{
  return GSK_SL_VOID;
}

static const GskSlImageType *
gsk_sl_type_block_get_image_type (const GskSlType *type)
{
  return NULL;
}

static GskSlType *
gsk_sl_type_block_get_index_type (const GskSlType *type)
{
  return NULL;
}

static gsize
gsk_sl_type_block_get_index_stride (const GskSlType *type)
{
  return 0;
}

static guint
gsk_sl_type_block_get_length (const GskSlType *type)
{
  return 0;
}

static gsize
gsk_sl_type_block_get_size (const GskSlType *type)
{
  const GskSlTypeBlock *block = (const GskSlTypeBlock *) type;

  return block->size;
}

static gsize
gsk_sl_type_block_get_n_components (const GskSlType *type)
{
  return 0;
}

static guint
gsk_sl_type_block_get_n_members (const GskSlType *type)
{
  const GskSlTypeBlock *block = (const GskSlTypeBlock *) type;

  return block->n_members;
}

static const GskSlTypeMember *
gsk_sl_type_block_get_member (const GskSlType *type,
                              guint            n)
{
  const GskSlTypeBlock *block = (const GskSlTypeBlock *) type;

  return &block->members[n];
}

static gboolean
gsk_sl_type_block_can_convert (const GskSlType *target,
                               const GskSlType *source)
{
  return gsk_sl_type_equal (target, source);
}

static guint32
gsk_sl_type_block_write_spv (GskSlType    *type,
                             GskSpvWriter *writer)
{
  GskSlTypeBlock *block = (GskSlTypeBlock *) type;
  guint32 ids[block->n_members], result_id;
  guint i;

  for (i = 0; i < block->n_members; i++)
    {
      ids[i] = gsk_spv_writer_get_id_for_type (writer, block->members[i].type);
    }

  result_id = gsk_spv_writer_type_struct (writer,
                                          ids, 
                                          block->n_members);
  
  gsk_spv_writer_decorate (writer, result_id, GSK_SPV_DECORATION_BLOCK, NULL, 0);

  for (i = 0; i < block->n_members; i++)
    {
      gsk_sl_type_write_member_decoration (writer, result_id, i, block->members[i].type,
          block->members[i].name, block->members[i].offset);
    }

  return result_id;
}

static void
gsk_sl_type_block_print_value (const GskSlType *type,
                               GskSlPrinter    *printer,
                               gconstpointer    value)
{
  const GskSlTypeBlock *block = (const GskSlTypeBlock *) type;
  guint i;

  gsk_sl_printer_append (printer, block->name);
  gsk_sl_printer_append (printer, "(");

  for (i = 0; i < block->n_members; i++)
    {
      if (i > 0)
        gsk_sl_printer_append (printer, ", ");
      gsk_sl_type_print_value (block->members[i].type,
                               printer,
                               (guchar *) value + block->members[i].offset);
    }

  gsk_sl_printer_append (printer, ")");
}

static gboolean
gsk_sl_type_block_value_equal (const GskSlType *type,
                               gconstpointer    a,
                               gconstpointer    b)
{
  GskSlTypeBlock *block = (GskSlTypeBlock *) type;
  guint i;
  const guchar *adata, *bdata;

  adata = a;
  bdata = b;

  for (i = 0; i < block->n_members; i++)
    {
      if (!gsk_sl_type_value_equal (block->members[i].type,
                                    adata + block->members[i].offset,
                                    bdata + block->members[i].offset))
        return FALSE;
    }

  return TRUE;
}

static guint32
gsk_sl_type_block_write_value_spv (GskSlType     *type,
                                   GskSpvWriter  *writer,
                                   gconstpointer  value)
{
  GskSlTypeBlock *block = (GskSlTypeBlock *) type;
  guint32 ids[block->n_members];
  GskSlValue *v;
  guint i;

  for (i = 0; i < block->n_members; i++)
    {
      v = gsk_sl_value_new_for_data (block->members[i].type,
                                     (guchar *) value + block->members[i].offset,
                                     NULL, NULL);
      ids[i] = gsk_spv_writer_get_id_for_value (writer, v);
      gsk_sl_value_free (v);
    }

  return gsk_spv_writer_constant_composite (writer,
                                            type,
                                            ids,
                                            block->n_members);
}

static const GskSlTypeClass GSK_SL_TYPE_BLOCK = {
  gsk_sl_type_block_free,
  gsk_sl_type_block_get_name,
  gsk_sl_type_block_get_scalar_type,
  gsk_sl_type_block_get_image_type,
  gsk_sl_type_block_get_index_type,
  gsk_sl_type_block_get_index_stride,
  gsk_sl_type_block_get_length,
  gsk_sl_type_block_get_size,
  gsk_sl_type_block_get_n_components,
  gsk_sl_type_block_get_n_members,
  gsk_sl_type_block_get_member,
  gsk_sl_type_block_can_convert,
  gsk_sl_type_block_write_spv,
  gsk_sl_type_block_print_value,
  gsk_sl_type_block_value_equal,
  gsk_sl_type_block_write_value_spv
};

/* API */

gsize
gsk_sl_scalar_type_get_size (GskSlScalarType type)
{
  return scalar_infos[type].size;
}

static GskSlType *
gsk_sl_type_parse_struct (GskSlScope        *scope,
                          GskSlPreprocessor *preproc)
{
  GskSlType *type;
  const GskSlToken *token;
  GskSlTypeBuilder *builder;
  gboolean add_type = FALSE;

  /* the struct token */
  gsk_sl_preprocessor_consume (preproc, NULL);

  token = gsk_sl_preprocessor_get (preproc);
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
    {    
      if (gsk_sl_scope_is_global (scope))
        {
          add_type = TRUE;
          builder = gsk_sl_type_builder_new_struct (token->str);
        }
      else
        {
          builder = gsk_sl_type_builder_new_struct (NULL);
        }
      gsk_sl_preprocessor_consume (preproc, NULL);
    }
  else
    {
      builder = gsk_sl_type_builder_new_struct (NULL);
    }

  token = gsk_sl_preprocessor_get (preproc);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_BRACE))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected opening \"{\" after struct declaration.");
      goto out;
    }
  gsk_sl_preprocessor_consume (preproc, NULL);

  for (token = gsk_sl_preprocessor_get (preproc);
       !gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_BRACE) && !gsk_sl_token_is (token, GSK_SL_TOKEN_EOF);
       token = gsk_sl_preprocessor_get (preproc))
    {
      type = gsk_sl_type_new_parse (scope, preproc);

      while (TRUE)
        {
          token = gsk_sl_preprocessor_get (preproc);
          if (!gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
            {
              gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected identifier for type name.");
              break;
            }
          if (gsk_sl_type_builder_has_member (builder, token->str))
            gsk_sl_preprocessor_error (preproc, DECLARATION, "struct already has a member named \"%s\".", token->str);
          else
            gsk_sl_type_builder_add_member (builder, type, token->str);
          gsk_sl_preprocessor_consume (preproc, NULL);

          token = gsk_sl_preprocessor_get (preproc);
          if (!gsk_sl_token_is (token, GSK_SL_TOKEN_COMMA))
            break;

          gsk_sl_preprocessor_consume (preproc, NULL);
        }
      gsk_sl_type_unref (type);

      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_SEMICOLON))
        gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected semicolon after struct member declaration.");
      else
        gsk_sl_preprocessor_consume (preproc, NULL);
    }

  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_BRACE))
    gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected closing \"}\" after struct declaration.");
  else
    gsk_sl_preprocessor_consume (preproc, NULL);
  
out:
  type = gsk_sl_type_builder_free (builder);
  if (add_type)
    {
      if (gsk_sl_scope_lookup_type (scope, gsk_sl_type_get_name (type)))
        gsk_sl_preprocessor_error (preproc, DECLARATION, "Redefinition of struct \"%s\".", gsk_sl_type_get_name (type));
      else
        {
          GskSlFunctionMatcher matcher;
          
          gsk_sl_scope_match_function (scope, &matcher, gsk_sl_type_get_name (type));
          if (gsk_sl_function_matcher_has_matches (&matcher))
            gsk_sl_preprocessor_error (preproc, DECLARATION, "Constructor name \"%s\" would override function of same name.", gsk_sl_type_get_name (type));
          else
            gsk_sl_scope_add_type (scope, type);
          gsk_sl_function_matcher_finish (&matcher);
        }
    }
  return type;
}

static GskSlType *
gsk_sl_type_parse_block (GskSlScope        *scope,
                         GskSlPreprocessor *preproc)
{
  GskSlType *type;
  const GskSlToken *token;
  GskSlTypeBuilder *builder;

  if (!gsk_sl_scope_is_global (scope))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Blocks are only allowed in global scope.");
      return gsk_sl_type_ref (gsk_sl_type_get_scalar (GSK_SL_FLOAT));
    }

  token = gsk_sl_preprocessor_get (preproc);
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
    {    
      builder = gsk_sl_type_builder_new_block (token->str);
      gsk_sl_preprocessor_consume (preproc, NULL);
    }
  else
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected block name.");
      return gsk_sl_type_ref (gsk_sl_type_get_scalar (GSK_SL_FLOAT));
    }

  token = gsk_sl_preprocessor_get (preproc);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_BRACE))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected opening \"{\" after block declaration.");
      goto out;
    }
  gsk_sl_preprocessor_consume (preproc, NULL);

  for (token = gsk_sl_preprocessor_get (preproc);
       !gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_BRACE) && !gsk_sl_token_is (token, GSK_SL_TOKEN_EOF);
       token = gsk_sl_preprocessor_get (preproc))
    {
      type = gsk_sl_type_new_parse (scope, preproc);

      while (TRUE)
        {
          token = gsk_sl_preprocessor_get (preproc);
          if (!gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
            {
              gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected identifier for type name.");
              break;
            }
          if (gsk_sl_type_builder_has_member (builder, token->str))
            gsk_sl_preprocessor_error (preproc, DECLARATION, "struct already has a member named \"%s\".", token->str);
          else
            gsk_sl_type_builder_add_member (builder, type, token->str);
          gsk_sl_preprocessor_consume (preproc, NULL);

          token = gsk_sl_preprocessor_get (preproc);
          if (!gsk_sl_token_is (token, GSK_SL_TOKEN_COMMA))
            break;

          gsk_sl_preprocessor_consume (preproc, NULL);
        }
      gsk_sl_type_unref (type);

      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_SEMICOLON))
        gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected semicolon after block member declaration.");
      else
        gsk_sl_preprocessor_consume (preproc, NULL);
    }

  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_BRACE))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected closing \"}\" after block declaration.");
      gsk_sl_preprocessor_sync (preproc, GSK_SL_TOKEN_RIGHT_BRACE);
    }
  gsk_sl_preprocessor_consume (preproc, NULL);
  
out:
  return gsk_sl_type_builder_free (builder);
}

GskSlType *
gsk_sl_type_get_matching (GskSlType       *type,
                          GskSlScalarType  scalar)
{
  if (gsk_sl_type_is_scalar (type))
    return gsk_sl_type_get_scalar (scalar);
  else if (gsk_sl_type_is_vector (type))
    return gsk_sl_type_get_vector (scalar, gsk_sl_type_get_length (type));
  else if (gsk_sl_type_is_matrix (type))
    return gsk_sl_type_get_matrix (scalar,
                                   gsk_sl_type_get_length (type),
                                   gsk_sl_type_get_length (gsk_sl_type_get_index_type (type)));
  else
    {
      g_return_val_if_reached (gsk_sl_type_get_void ());
    }
}

static char *
gsk_sl_array_type_generate_name (GskSlType *type)
{
  GString *string = g_string_new (NULL);

  for (; gsk_sl_type_is_array (type); type = gsk_sl_type_get_index_type (type))
    {
      g_string_append_printf (string, "[%u]", gsk_sl_type_get_length (type));
    }

  g_string_prepend (string, gsk_sl_type_get_name (type));

  return g_string_free (string, FALSE);
}

GskSlType *
gsk_sl_type_new_array (GskSlType *type,
                       gsize      length)
{
  GskSlTypeArray *result;

  result = gsk_sl_type_new (GskSlTypeArray, &GSK_SL_TYPE_ARRAY);

  result->type = gsk_sl_type_ref (type);
  result->length = length;

  result->name = gsk_sl_array_type_generate_name (&result->parent);

  return &result->parent;
}

GskSlType *
gsk_sl_type_parse_array (GskSlType         *type,
                         GskSlScope        *scope,
                         GskSlPreprocessor *preproc)
{
  GskSlType *array;
  const GskSlToken *token;
  gsize length;

  token = gsk_sl_preprocessor_get (preproc);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_BRACKET))
    return type;

  gsk_sl_preprocessor_consume (preproc, NULL);

  length = gsk_sl_expression_parse_integral_constant (scope, preproc, 1, G_MAXINT);

  token = gsk_sl_preprocessor_get (preproc);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_BRACKET))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected closing \"]\"");
      return type;
    }
  gsk_sl_preprocessor_consume (preproc, NULL);

  type = gsk_sl_type_parse_array (type, scope, preproc);

  array = gsk_sl_type_new_array (type, length);
  gsk_sl_type_unref (type);

  return array;
}

GskSlType *
gsk_sl_type_new_parse (GskSlScope        *scope,
                       GskSlPreprocessor *preproc)
{
  GskSlType *type;
  const GskSlToken *token;

  token = gsk_sl_preprocessor_get (preproc);

  switch ((guint) token->type)
  {
    case GSK_SL_TOKEN_VOID:
      type = gsk_sl_type_ref (gsk_sl_type_get_void ());
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
    case GSK_SL_TOKEN_SAMPLER1D:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_1D));
      break;
    case GSK_SL_TOKEN_SAMPLER2D:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2D));
      break;
    case GSK_SL_TOKEN_SAMPLER3D:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_3D));
      break;
    case GSK_SL_TOKEN_SAMPLERCUBE:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_CUBE));
      break;
    case GSK_SL_TOKEN_SAMPLER1DSHADOW:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_1D_SHADOW));
      break;
    case GSK_SL_TOKEN_SAMPLER2DSHADOW:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2D_SHADOW));
      break;
    case GSK_SL_TOKEN_SAMPLERCUBESHADOW:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_CUBE_SHADOW));
      break;
    case GSK_SL_TOKEN_SAMPLER1DARRAY:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_1D_ARRAY));
      break;
    case GSK_SL_TOKEN_SAMPLER2DARRAY:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2D_ARRAY));
      break;
    case GSK_SL_TOKEN_SAMPLER1DARRAYSHADOW:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_1D_ARRAY_SHADOW));
      break;
    case GSK_SL_TOKEN_SAMPLER2DARRAYSHADOW:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2D_ARRAY_SHADOW));
      break;
    case GSK_SL_TOKEN_ISAMPLER1D:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_1D_INT));
      break;
    case GSK_SL_TOKEN_ISAMPLER2D:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2D_INT));
      break;
    case GSK_SL_TOKEN_ISAMPLER3D:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_3D_INT));
      break;
    case GSK_SL_TOKEN_ISAMPLERCUBE:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_CUBE_INT));
      break;
    case GSK_SL_TOKEN_ISAMPLER1DARRAY:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_1D_ARRAY_INT));
      break;
    case GSK_SL_TOKEN_ISAMPLER2DARRAY:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2D_ARRAY_INT));
      break;
    case GSK_SL_TOKEN_USAMPLER1D:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_1D_UINT));
      break;
    case GSK_SL_TOKEN_USAMPLER2D:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2D_UINT));
      break;
    case GSK_SL_TOKEN_USAMPLER3D:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_3D_UINT));
      break;
    case GSK_SL_TOKEN_USAMPLERCUBE:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_CUBE_UINT));
      break;
    case GSK_SL_TOKEN_USAMPLER1DARRAY:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_1D_ARRAY_UINT));
      break;
    case GSK_SL_TOKEN_USAMPLER2DARRAY:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2D_ARRAY_UINT));
      break;
    case GSK_SL_TOKEN_SAMPLER2DRECT:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2D_RECT));
      break;
    case GSK_SL_TOKEN_SAMPLER2DRECTSHADOW:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2D_RECT_SHADOW));
      break;
    case GSK_SL_TOKEN_ISAMPLER2DRECT:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2D_RECT_INT));
      break;
    case GSK_SL_TOKEN_USAMPLER2DRECT:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2D_RECT_UINT));
      break;
    case GSK_SL_TOKEN_SAMPLERBUFFER:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_BUFFER));
      break;
    case GSK_SL_TOKEN_ISAMPLERBUFFER:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_BUFFER_INT));
      break;
    case GSK_SL_TOKEN_USAMPLERBUFFER:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_BUFFER_UINT));
      break;
    case GSK_SL_TOKEN_SAMPLERCUBEARRAY:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_CUBE_ARRAY));
      break;
    case GSK_SL_TOKEN_SAMPLERCUBEARRAYSHADOW:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_CUBE_ARRAY_SHADOW));
      break;
    case GSK_SL_TOKEN_ISAMPLERCUBEARRAY:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_CUBE_ARRAY_INT));
      break;
    case GSK_SL_TOKEN_USAMPLERCUBEARRAY:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_CUBE_ARRAY_UINT));
      break;
    case GSK_SL_TOKEN_SAMPLER2DMS:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2DMS));
      break;
    case GSK_SL_TOKEN_ISAMPLER2DMS:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2DMS_INT));
      break;
    case GSK_SL_TOKEN_USAMPLER2DMS:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2DMS_UINT));
      break;
    case GSK_SL_TOKEN_SAMPLER2DMSARRAY:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2DMS_ARRAY));
      break;
    case GSK_SL_TOKEN_ISAMPLER2DMSARRAY:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2DMS_ARRAY_INT));
      break;
    case GSK_SL_TOKEN_USAMPLER2DMSARRAY:
      type = gsk_sl_type_ref (gsk_sl_type_get_sampler (GSK_SL_SAMPLER_2DMS_ARRAY_UINT));
      break;
    case GSK_SL_TOKEN_STRUCT:
      return gsk_sl_type_parse_struct (scope, preproc);
    case GSK_SL_TOKEN_IDENTIFIER:
      {
        type = gsk_sl_scope_lookup_type (scope, token->str);

        if (type)
          {
            type = gsk_sl_type_ref (type);
            break;
          }
  
        return gsk_sl_type_parse_block (scope, preproc);
      }
    default:
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected type specifier");
      return gsk_sl_type_ref (gsk_sl_type_get_scalar (GSK_SL_FLOAT));
  }

  gsk_sl_preprocessor_consume (preproc, NULL);

  type = gsk_sl_type_parse_array (type, scope, preproc);

  return type;
}

static GskSlTypeVoid void_type = { { &GSK_SL_TYPE_VOID, 1 } };

GskSlType *
gsk_sl_type_get_void (void)
{
  return &void_type.parent;
}

static GskSlTypeScalar
builtin_scalar_types[N_SCALAR_TYPES] = {
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
  g_return_val_if_fail (scalar != GSK_SL_VOID, &void_type.parent);

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

static GskSlTypeSampler
builtin_sampler_types[] = {
  [GSK_SL_SAMPLER_1D] =                { { &GSK_SL_TYPE_SAMPLER, 1 }, "sampler1D",              GSK_SL_SAMPLER_1D,                { GSK_SL_FLOAT, GSK_SPV_DIM_1_D,    0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_1D_INT] =            { { &GSK_SL_TYPE_SAMPLER, 1 }, "isampler1D",             GSK_SL_SAMPLER_1D_INT,            { GSK_SL_INT,   GSK_SPV_DIM_1_D,    0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_1D_UINT] =           { { &GSK_SL_TYPE_SAMPLER, 1 }, "usampler1D",             GSK_SL_SAMPLER_1D_UINT,           { GSK_SL_UINT,  GSK_SPV_DIM_1_D,    0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_1D_SHADOW] =         { { &GSK_SL_TYPE_SAMPLER, 1 }, "sampler1DShadow",        GSK_SL_SAMPLER_1D_SHADOW,         { GSK_SL_FLOAT, GSK_SPV_DIM_1_D,    1, 0, 0, 1 } },
  [GSK_SL_SAMPLER_2D] =                { { &GSK_SL_TYPE_SAMPLER, 1 }, "sampler2D",              GSK_SL_SAMPLER_2D,                { GSK_SL_FLOAT, GSK_SPV_DIM_2_D,    0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_2D_INT] =            { { &GSK_SL_TYPE_SAMPLER, 1 }, "isampler2D",             GSK_SL_SAMPLER_2D_INT,            { GSK_SL_INT,   GSK_SPV_DIM_2_D,    0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_2D_UINT] =           { { &GSK_SL_TYPE_SAMPLER, 1 }, "usampler2D",             GSK_SL_SAMPLER_2D_UINT,           { GSK_SL_UINT,  GSK_SPV_DIM_2_D,    0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_2D_SHADOW] =         { { &GSK_SL_TYPE_SAMPLER, 1 }, "sampler2DShadow",        GSK_SL_SAMPLER_2D_SHADOW,         { GSK_SL_FLOAT, GSK_SPV_DIM_2_D,    1, 0, 0, 1 } },
  [GSK_SL_SAMPLER_3D] =                { { &GSK_SL_TYPE_SAMPLER, 1 }, "sampler3D",              GSK_SL_SAMPLER_3D,                { GSK_SL_FLOAT, GSK_SPV_DIM_3_D,    0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_3D_INT] =            { { &GSK_SL_TYPE_SAMPLER, 1 }, "isampler3D",             GSK_SL_SAMPLER_3D_INT,            { GSK_SL_INT,   GSK_SPV_DIM_3_D,    0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_3D_UINT] =           { { &GSK_SL_TYPE_SAMPLER, 1 }, "usampler3D",             GSK_SL_SAMPLER_3D_UINT,           { GSK_SL_UINT,  GSK_SPV_DIM_3_D,    0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_CUBE] =              { { &GSK_SL_TYPE_SAMPLER, 1 }, "samplerCube",            GSK_SL_SAMPLER_CUBE,              { GSK_SL_FLOAT, GSK_SPV_DIM_CUBE,   0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_CUBE_INT] =          { { &GSK_SL_TYPE_SAMPLER, 1 }, "isamplerCube",           GSK_SL_SAMPLER_CUBE_INT,          { GSK_SL_INT,   GSK_SPV_DIM_CUBE,   0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_CUBE_UINT] =         { { &GSK_SL_TYPE_SAMPLER, 1 }, "usamplerCube",           GSK_SL_SAMPLER_CUBE_UINT,         { GSK_SL_UINT,  GSK_SPV_DIM_CUBE,   0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_CUBE_SHADOW] =       { { &GSK_SL_TYPE_SAMPLER, 1 }, "samplerCubeShadow",      GSK_SL_SAMPLER_CUBE_SHADOW,       { GSK_SL_FLOAT, GSK_SPV_DIM_CUBE,   1, 0, 0, 1 } },
  [GSK_SL_SAMPLER_2D_RECT] =           { { &GSK_SL_TYPE_SAMPLER, 1 }, "sampler2DRect",          GSK_SL_SAMPLER_2D_RECT,           { GSK_SL_FLOAT, GSK_SPV_DIM_RECT,   0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_2D_RECT_INT] =       { { &GSK_SL_TYPE_SAMPLER, 1 }, "isampler2DRect",         GSK_SL_SAMPLER_2D_RECT_INT,       { GSK_SL_INT,   GSK_SPV_DIM_RECT,   0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_2D_RECT_UINT] =      { { &GSK_SL_TYPE_SAMPLER, 1 }, "usampler2DRect",         GSK_SL_SAMPLER_2D_RECT_UINT,      { GSK_SL_UINT,  GSK_SPV_DIM_RECT,   0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_2D_RECT_SHADOW] =    { { &GSK_SL_TYPE_SAMPLER, 1 }, "sampler2DRectShadow",    GSK_SL_SAMPLER_2D_RECT_SHADOW,    { GSK_SL_FLOAT, GSK_SPV_DIM_RECT,   1, 0, 0, 1 } },
  [GSK_SL_SAMPLER_1D_ARRAY] =          { { &GSK_SL_TYPE_SAMPLER, 1 }, "sampler1DArray",         GSK_SL_SAMPLER_1D_ARRAY,          { GSK_SL_FLOAT, GSK_SPV_DIM_1_D,    0, 1, 0, 1 } },
  [GSK_SL_SAMPLER_1D_ARRAY_INT] =      { { &GSK_SL_TYPE_SAMPLER, 1 }, "isampler1DArray",        GSK_SL_SAMPLER_1D_ARRAY_INT,      { GSK_SL_INT,   GSK_SPV_DIM_1_D,    0, 1, 0, 1 } },
  [GSK_SL_SAMPLER_1D_ARRAY_UINT] =     { { &GSK_SL_TYPE_SAMPLER, 1 }, "usampler1DArray",        GSK_SL_SAMPLER_1D_ARRAY_UINT,     { GSK_SL_UINT,  GSK_SPV_DIM_1_D,    0, 1, 0, 1 } },
  [GSK_SL_SAMPLER_1D_ARRAY_SHADOW] =   { { &GSK_SL_TYPE_SAMPLER, 1 }, "sampler1DArrayShadow",   GSK_SL_SAMPLER_1D_ARRAY_SHADOW,   { GSK_SL_FLOAT, GSK_SPV_DIM_1_D,    1, 1, 0, 1 } },
  [GSK_SL_SAMPLER_2D_ARRAY] =          { { &GSK_SL_TYPE_SAMPLER, 1 }, "sampler2DArray",         GSK_SL_SAMPLER_2D_ARRAY,          { GSK_SL_FLOAT, GSK_SPV_DIM_2_D,    0, 1, 0, 1 } },
  [GSK_SL_SAMPLER_2D_ARRAY_INT] =      { { &GSK_SL_TYPE_SAMPLER, 1 }, "isampler2DArray",        GSK_SL_SAMPLER_2D_ARRAY_INT,      { GSK_SL_INT,   GSK_SPV_DIM_2_D,    0, 1, 0, 1 } },
  [GSK_SL_SAMPLER_2D_ARRAY_UINT] =     { { &GSK_SL_TYPE_SAMPLER, 1 }, "usampler2DArray",        GSK_SL_SAMPLER_2D_ARRAY_UINT,     { GSK_SL_UINT,  GSK_SPV_DIM_2_D,    0, 1, 0, 1 } },
  [GSK_SL_SAMPLER_2D_ARRAY_SHADOW] =   { { &GSK_SL_TYPE_SAMPLER, 1 }, "sampler2DArrayShadow",   GSK_SL_SAMPLER_2D_ARRAY_SHADOW,   { GSK_SL_FLOAT, GSK_SPV_DIM_2_D,    1, 1, 0, 1 } },
  [GSK_SL_SAMPLER_CUBE_ARRAY] =        { { &GSK_SL_TYPE_SAMPLER, 1 }, "samplerCubeArray",       GSK_SL_SAMPLER_CUBE_ARRAY,        { GSK_SL_FLOAT, GSK_SPV_DIM_CUBE,   0, 1, 0, 1 } },
  [GSK_SL_SAMPLER_CUBE_ARRAY_INT] =    { { &GSK_SL_TYPE_SAMPLER, 1 }, "isamplerCubeArray",      GSK_SL_SAMPLER_CUBE_ARRAY_INT,    { GSK_SL_INT,   GSK_SPV_DIM_CUBE,   0, 1, 0, 1 } },
  [GSK_SL_SAMPLER_CUBE_ARRAY_UINT] =   { { &GSK_SL_TYPE_SAMPLER, 1 }, "usamplerCubeArray",      GSK_SL_SAMPLER_CUBE_ARRAY_UINT,   { GSK_SL_UINT,  GSK_SPV_DIM_CUBE,   0, 1, 0, 1 } },
  [GSK_SL_SAMPLER_CUBE_ARRAY_SHADOW] = { { &GSK_SL_TYPE_SAMPLER, 1 }, "samplerCubeArrayShadow", GSK_SL_SAMPLER_CUBE_ARRAY_SHADOW, { GSK_SL_FLOAT, GSK_SPV_DIM_CUBE,   1, 1, 0, 1 } },
  [GSK_SL_SAMPLER_BUFFER] =            { { &GSK_SL_TYPE_SAMPLER, 1 }, "samplerBuffer",          GSK_SL_SAMPLER_BUFFER,            { GSK_SL_FLOAT, GSK_SPV_DIM_BUFFER, 0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_BUFFER_INT] =        { { &GSK_SL_TYPE_SAMPLER, 1 }, "isamplerBuffer",         GSK_SL_SAMPLER_BUFFER_INT,        { GSK_SL_INT,   GSK_SPV_DIM_BUFFER, 0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_BUFFER_UINT] =       { { &GSK_SL_TYPE_SAMPLER, 1 }, "usamplerBuffer",         GSK_SL_SAMPLER_BUFFER_UINT,       { GSK_SL_UINT,  GSK_SPV_DIM_BUFFER, 0, 0, 0, 1 } },
  [GSK_SL_SAMPLER_2DMS] =              { { &GSK_SL_TYPE_SAMPLER, 1 }, "sampler2DMS",            GSK_SL_SAMPLER_2DMS,              { GSK_SL_FLOAT, GSK_SPV_DIM_2_D,    0, 0, 1, 1 } },
  [GSK_SL_SAMPLER_2DMS_INT] =          { { &GSK_SL_TYPE_SAMPLER, 1 }, "isampler2DMS",           GSK_SL_SAMPLER_2DMS_INT,          { GSK_SL_INT,   GSK_SPV_DIM_2_D,    0, 0, 1, 1 } },
  [GSK_SL_SAMPLER_2DMS_UINT] =         { { &GSK_SL_TYPE_SAMPLER, 1 }, "usampler2DMS",           GSK_SL_SAMPLER_2DMS_UINT,         { GSK_SL_UINT,  GSK_SPV_DIM_2_D,    0, 0, 1, 1 } },
  [GSK_SL_SAMPLER_2DMS_ARRAY] =        { { &GSK_SL_TYPE_SAMPLER, 1 }, "sampler2DMSArray",       GSK_SL_SAMPLER_2DMS_ARRAY,        { GSK_SL_FLOAT, GSK_SPV_DIM_2_D,    0, 1, 1, 1 } },
  [GSK_SL_SAMPLER_2DMS_ARRAY_INT] =    { { &GSK_SL_TYPE_SAMPLER, 1 }, "isampler2DMSArray",      GSK_SL_SAMPLER_2DMS_ARRAY_INT,    { GSK_SL_INT,   GSK_SPV_DIM_2_D,    0, 1, 1, 1 } },
  [GSK_SL_SAMPLER_2DMS_ARRAY_UINT] =   { { &GSK_SL_TYPE_SAMPLER, 1 }, "usampler2DMSArray",      GSK_SL_SAMPLER_2DMS_ARRAY_UINT,   { GSK_SL_UINT,  GSK_SPV_DIM_2_D,    0, 1, 1, 1 } }
};

GskSlType *
gsk_sl_type_get_sampler (GskSlSamplerType sampler)
{
  return &builtin_sampler_types[sampler].parent;
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
gsk_sl_type_is_void (const GskSlType *type)
{
  return type->class == &GSK_SL_TYPE_VOID;
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

gboolean
gsk_sl_type_is_basic (const GskSlType *type)
{
  return gsk_sl_type_is_scalar (type)
      || gsk_sl_type_is_vector (type)
      || gsk_sl_type_is_matrix (type);
}

gboolean
gsk_sl_type_is_array (const GskSlType *type)
{
  return type->class == &GSK_SL_TYPE_ARRAY;
}

gboolean
gsk_sl_type_is_struct (const GskSlType *type)
{
  return type->class == &GSK_SL_TYPE_STRUCT;
}

gboolean
gsk_sl_type_is_block (const GskSlType *type)
{
  return type->class == &GSK_SL_TYPE_BLOCK;
}

gboolean
gsk_sl_type_is_sampler (const GskSlType *type)
{
  return type->class == &GSK_SL_TYPE_SAMPLER;
}

gboolean
gsk_sl_type_is_opaque (const GskSlType *type)
{
  return gsk_sl_type_is_sampler (type);
}

gboolean
gsk_sl_type_contains_opaque (const GskSlType *type)
{
  gsize i;

  if (gsk_sl_type_is_opaque (type))
    return TRUE;

  for (i = 0; i < gsk_sl_type_get_n_members (type); i++)
    {
      if (gsk_sl_type_contains_opaque (gsk_sl_type_get_member_type (type, i)))
        return TRUE;
    }

  return FALSE;
}

GskSlScalarType
gsk_sl_type_get_scalar_type (const GskSlType *type)
{
  return type->class->get_scalar_type (type);
}

const GskSlImageType *
gsk_sl_type_get_image_type (const GskSlType *type)
{
  return type->class->get_image_type (type);
}

GskSlType *
gsk_sl_type_get_index_type (const GskSlType *type)
{
  return type->class->get_index_type (type);
}

gsize
gsk_sl_type_get_index_stride (const GskSlType *type)
{
  return type->class->get_index_stride (type);
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

gsize
gsk_sl_type_get_n_components (const GskSlType *type)
{
  return type->class->get_n_components (type);
}

guint
gsk_sl_type_get_n_members (const GskSlType *type)
{
  return type->class->get_n_members (type);
}

static const GskSlTypeMember *
gsk_sl_type_get_member (const GskSlType *type,
                        guint            n)
{
  return type->class->get_member (type, n);
}

GskSlType *
gsk_sl_type_get_member_type (const GskSlType *type,
                             guint            n)
{
  const GskSlTypeMember *member;

  member = gsk_sl_type_get_member (type, n);

  return member->type;
}

const char *
gsk_sl_type_get_member_name (const GskSlType *type,
                             guint            n)
{
  const GskSlTypeMember *member;

  member = gsk_sl_type_get_member (type, n);

  return member->name;
}

gsize
gsk_sl_type_get_member_offset (const GskSlType *type,
                               guint            n)
{
  const GskSlTypeMember *member;

  member = gsk_sl_type_get_member (type, n);

  return member->offset;
}

gboolean
gsk_sl_type_find_member (const GskSlType *type,
                         const char      *name,
                         guint           *out_index,
                         GskSlType      **out_type,
                         gsize           *out_offset)
{
  const GskSlTypeMember *member;
  guint i, n;
  
  n = gsk_sl_type_get_n_members (type);
  for (i = 0; i < n; i++)
    {
      member = gsk_sl_type_get_member (type, i);
      if (g_str_equal (member->name, name))
        {
          if (out_index)
            *out_index = i;
          if (out_type)
            *out_type = member->type;
          if (out_offset)
            *out_offset = member->offset;
          return TRUE;
        }
    }

  return FALSE;
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

void
gsk_sl_scalar_type_convert_value (GskSlScalarType target_type,
                                  gpointer        target_value,
                                  GskSlScalarType source_type,
                                  gconstpointer   source_value)
{
  scalar_infos[source_type].convert_value[target_type] (target_value, source_value);
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
                         GskSlPrinter    *printer,
                         gconstpointer    value)
{
  type->class->print_value (type, printer, value);
}

gboolean
gsk_sl_type_value_equal (const GskSlType *type,
                         gconstpointer    a,
                         gconstpointer    b)
{
  return type->class->value_equal (type, a, b);
}

guint32
gsk_sl_type_write_value_spv (GskSlType     *type,
                             GskSpvWriter  *writer,
                             gconstpointer  value)
{
  return type->class->write_value_spv (type, writer, value);
}

struct _GskSlTypeBuilder {
  char *name;
  gsize size;
  GArray *members;
  guint is_block :1;
};

GskSlTypeBuilder *
gsk_sl_type_builder_new_struct (const char *name)
{
  GskSlTypeBuilder *builder;

  builder = g_slice_new0 (GskSlTypeBuilder);

  builder->name = g_strdup (name);
  builder->members = g_array_new (FALSE, FALSE, sizeof (GskSlTypeMember));

  return builder;
}

GskSlTypeBuilder *
gsk_sl_type_builder_new_block (const char *name)
{
  GskSlTypeBuilder *builder;

  g_assert (name != NULL);

  builder = g_slice_new0 (GskSlTypeBuilder);

  builder->name = g_strdup (name);
  builder->members = g_array_new (FALSE, FALSE, sizeof (GskSlTypeMember));
  builder->is_block = TRUE;

  return builder;
}

static char *
gsk_sl_type_builder_generate_name (GskSlTypeBuilder *builder)
{
  GString *string = g_string_new ("struct { ");
  guint i, j;

  for (i = 0; i < builder->members->len; i++)
    {
      GskSlTypeMember *m = &g_array_index (builder->members, GskSlTypeMember, i);
      g_string_append (string, gsk_sl_type_get_name (m->type));
      g_string_append (string, " ");
      g_string_append (string, m->name);
      for (j = i + 1; j < builder->members->len; j++)
        {
          GskSlTypeMember *n = &g_array_index (builder->members, GskSlTypeMember, j);
          if (!gsk_sl_type_equal (m->type, n->type))
            break;
          g_string_append (string, ", ");
          g_string_append (string, n->name);
        }
      i = j - 1;
      g_string_append (string, "; ");
    }
  g_string_append (string, "}");

  return g_string_free (string, FALSE);
}

static GskSlType *
gsk_sl_type_builder_free_to_struct (GskSlTypeBuilder *builder)
{
  GskSlTypeStruct *result;

  result = gsk_sl_type_new (GskSlTypeStruct, &GSK_SL_TYPE_STRUCT);

  if (builder->name)
    result->name = builder->name;
  else
    result->name = gsk_sl_type_builder_generate_name (builder);
  result->size = builder->size;
  result->n_members = builder->members->len;
  result->members = (GskSlTypeMember *) g_array_free (builder->members, FALSE);

  g_slice_free (GskSlTypeBuilder, builder);

  return &result->parent;
}

static GskSlType *
gsk_sl_type_builder_free_to_block (GskSlTypeBuilder *builder)
{
  GskSlTypeBlock *result;

  result = gsk_sl_type_new (GskSlTypeBlock, &GSK_SL_TYPE_BLOCK);

  result->name = builder->name;
  result->size = builder->size;
  result->n_members = builder->members->len;
  result->members = (GskSlTypeMember *) g_array_free (builder->members, FALSE);

  g_slice_free (GskSlTypeBuilder, builder);

  return &result->parent;
}

GskSlType *
gsk_sl_type_builder_free (GskSlTypeBuilder *builder)
{
  if (builder->is_block)
    return gsk_sl_type_builder_free_to_block (builder);
  else
    return gsk_sl_type_builder_free_to_struct (builder);
}

void
gsk_sl_type_builder_add_member (GskSlTypeBuilder *builder,
                                GskSlType        *type,
                                const char       *name)
{
  g_array_append_vals (builder->members,
                       &(GskSlTypeMember) {
                           gsk_sl_type_ref (type),
                           g_strdup (name),
                           builder->size }, 1);
  builder->size += gsk_sl_type_get_size (type);
}

gboolean
gsk_sl_type_builder_has_member (GskSlTypeBuilder *builder,
                                const char       *name)
{
  guint i;

  for (i = 0; i < builder->members->len; i++)
    {
      if (g_str_equal (g_array_index (builder->members, GskSlTypeMember, i).name, name))
        return TRUE;
    }

  return FALSE;
}

