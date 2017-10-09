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

#include "gskslvariableprivate.h"

#include "gskslprinterprivate.h"
#include "gskslqualifierprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvalueprivate.h"
#include "gskspvwriterprivate.h"

typedef struct _GskSlVariableClass GskSlVariableClass;

struct _GskSlVariable {
  const GskSlVariableClass *class;

  guint ref_count;

  char *name;
  GskSlType *type;
  GskSlQualifier qualifier;
  GskSlValue *initial_value;
};

struct _GskSlVariableClass
{
  gsize                 size;

  void                  (* free)                                (GskSlVariable          *variable);

  guint32               (* write_spv)                           (const GskSlVariable    *variable,
                                                                 GskSpvWriter           *writer);
  guint32               (* load_spv)                            (GskSlVariable          *variable,
                                                                 GskSpvWriter           *writer);
  void                  (* store_spv)                           (GskSlVariable          *variable,
                                                                 GskSpvWriter           *writer,
                                                                 guint32                 value);
};

static GskSlVariable *
gsk_sl_variable_alloc (const GskSlVariableClass *klass)
{
  GskSlVariable *variable;

  variable = g_slice_alloc0 (klass->size);

  variable->class = klass;
  variable->ref_count = 1;

  return variable;
}

static void
gsk_sl_variable_free (GskSlVariable *variable)
{
  if (variable->initial_value)
    gsk_sl_value_free (variable->initial_value);
  gsk_sl_type_unref (variable->type);
  g_free (variable->name);

  g_slice_free1 (variable->class->size, variable);
}

/* STANDARD */

static guint32
gsk_sl_variable_standard_write_spv (const GskSlVariable *variable,
                                    GskSpvWriter        *writer)
{
  guint32 result_id;
  guint32 value_id;
  GskSlQualifierLocation location;

  location = gsk_sl_qualifier_get_location (&variable->qualifier);

  if (variable->initial_value)
    value_id = gsk_spv_writer_get_id_for_value (writer, variable->initial_value);
  else
    value_id = 0;

  result_id = gsk_spv_writer_variable (writer,
                                       location == GSK_SL_QUALIFIER_GLOBAL ? GSK_SPV_WRITER_SECTION_DEFINE : GSK_SPV_WRITER_SECTION_DECLARE,
                                       variable->type,
                                       gsk_sl_qualifier_get_storage_class (&variable->qualifier),
                                       gsk_sl_qualifier_get_storage_class (&variable->qualifier),
                                       value_id);

  if (variable->name)
    gsk_spv_writer_name (writer, result_id, variable->name);

  return result_id;
}

static guint32
gsk_sl_variable_standard_load_spv (GskSlVariable *variable,
                                   GskSpvWriter  *writer)
{
  return gsk_spv_writer_load (writer,
                              gsk_sl_variable_get_type (variable),
                              gsk_spv_writer_get_id_for_variable (writer, variable),
                              0);
}

static void
gsk_sl_variable_standard_store_spv (GskSlVariable *variable,
                                    GskSpvWriter  *writer,
                                    guint32        value)
{
  gsk_spv_writer_store (writer,
                        gsk_spv_writer_get_id_for_variable (writer, variable),
                        value,
                        0);

}

static const GskSlVariableClass GSK_SL_VARIABLE_STANDARD = {
  sizeof (GskSlVariable),
  gsk_sl_variable_free,
  gsk_sl_variable_standard_write_spv,
  gsk_sl_variable_standard_load_spv,
  gsk_sl_variable_standard_store_spv,
};

/* PARAMETER */

static guint32
gsk_sl_variable_parameter_write_spv (const GskSlVariable *variable,
                                     GskSpvWriter        *writer)
{
  guint32 result_id;
  
  result_id = gsk_spv_writer_function_parameter (writer,
                                                 variable->type);

  if (variable->name)
    gsk_spv_writer_name (writer, result_id, variable->name);

  return result_id;
}

static guint32
gsk_sl_variable_parameter_load_spv (GskSlVariable *variable,
                                    GskSpvWriter  *writer)
{
  return gsk_spv_writer_load (writer,
                              gsk_sl_variable_get_type (variable),
                              gsk_spv_writer_get_id_for_variable (writer, variable),
                              0);
}

static void
gsk_sl_variable_parameter_store_spv (GskSlVariable *variable,
                                     GskSpvWriter  *writer,
                                     guint32        value)
{
  gsk_spv_writer_store (writer,
                        gsk_spv_writer_get_id_for_variable (writer, variable),
                        value,
                        0);

}
static const GskSlVariableClass GSK_SL_VARIABLE_PARAMETER = {
  sizeof (GskSlVariable),
  gsk_sl_variable_free,
  gsk_sl_variable_parameter_write_spv,
  gsk_sl_variable_parameter_load_spv,
  gsk_sl_variable_parameter_store_spv,
};

/* API */

static const GskSlVariableClass *
gsk_sl_variable_select_class (const GskSlQualifier *qualifier,
                              gboolean              has_initial_value)
{
  switch (qualifier->storage)
  {
    case GSK_SL_STORAGE_GLOBAL:
    case GSK_SL_STORAGE_GLOBAL_CONST:
    case GSK_SL_STORAGE_GLOBAL_IN:
    case GSK_SL_STORAGE_GLOBAL_OUT:
    case GSK_SL_STORAGE_GLOBAL_UNIFORM:
    case GSK_SL_STORAGE_LOCAL:
    case GSK_SL_STORAGE_LOCAL_CONST:
      return &GSK_SL_VARIABLE_STANDARD;

    case GSK_SL_STORAGE_PARAMETER_IN:
    case GSK_SL_STORAGE_PARAMETER_OUT:
    case GSK_SL_STORAGE_PARAMETER_INOUT:
    case GSK_SL_STORAGE_PARAMETER_CONST:
      return &GSK_SL_VARIABLE_PARAMETER;

    case GSK_SL_STORAGE_DEFAULT:
    default:
      g_assert_not_reached ();
      return &GSK_SL_VARIABLE_STANDARD;
  }
}

GskSlVariable *
gsk_sl_variable_new (const char           *name,
                     GskSlType            *type,
                     const GskSlQualifier *qualifier,
                     GskSlValue           *initial_value)
{
  GskSlVariable *variable;

  g_return_val_if_fail (initial_value == NULL || gsk_sl_type_equal (type, gsk_sl_value_get_type (initial_value)), NULL);

  variable = gsk_sl_variable_alloc (gsk_sl_variable_select_class (qualifier, initial_value != NULL));

  variable->type = gsk_sl_type_ref (type);
  variable->qualifier = *qualifier;
  variable->name = g_strdup (name);
  variable->initial_value = initial_value;

  return variable;
}

GskSlVariable *
gsk_sl_variable_ref (GskSlVariable *variable)
{
  g_return_val_if_fail (variable != NULL, NULL);

  variable->ref_count += 1;

  return variable;
}

void
gsk_sl_variable_unref (GskSlVariable *variable)
{
  if (variable == NULL)
    return;

  variable->ref_count -= 1;
  if (variable->ref_count > 0)
    return;

  variable->class->free (variable);
}

void
gsk_sl_variable_print (const GskSlVariable *variable,
                       GskSlPrinter        *printer)
{
  if (gsk_sl_qualifier_print (&variable->qualifier, printer))
    gsk_sl_printer_append (printer, " ");
  gsk_sl_printer_append (printer, gsk_sl_type_get_name (variable->type));
  if (gsk_sl_type_is_block (variable->type))
    {
      guint i, n;

      gsk_sl_printer_append (printer, " {");
      gsk_sl_printer_push_indentation (printer);
      n = gsk_sl_type_get_n_members (variable->type);
      for (i = 0; i < n; i++)
        {
          gsk_sl_printer_newline (printer);
          gsk_sl_printer_append (printer, gsk_sl_type_get_name (gsk_sl_type_get_member_type (variable->type, i)));
          gsk_sl_printer_append (printer, " ");
          gsk_sl_printer_append (printer, gsk_sl_type_get_member_name (variable->type, i));
          gsk_sl_printer_append (printer, ";");
        }
      gsk_sl_printer_pop_indentation (printer);
      gsk_sl_printer_newline (printer);
      gsk_sl_printer_append (printer, "}");
    }
  if (variable->name)
    {
      gsk_sl_printer_append (printer, " ");
      gsk_sl_printer_append (printer, variable->name);
    }
}

GskSlType *
gsk_sl_variable_get_type (const GskSlVariable *variable)
{
  return variable->type;
}

const GskSlQualifier *
gsk_sl_variable_get_qualifier (const GskSlVariable *variable)
{
  return &variable->qualifier;
}

const char *
gsk_sl_variable_get_name (const GskSlVariable *variable)
{
  return variable->name;
}

const GskSlValue *
gsk_sl_variable_get_initial_value (const GskSlVariable *variable)
{
  return variable->initial_value;
}

gboolean
gsk_sl_variable_is_constant (const GskSlVariable *variable)
{
  return gsk_sl_qualifier_is_constant (&variable->qualifier);
}

guint32
gsk_sl_variable_write_spv (const GskSlVariable *variable,
                           GskSpvWriter        *writer)
{
  return variable->class->write_spv (variable, writer);
}

guint32
gsk_sl_variable_load_spv (GskSlVariable *variable,
                          GskSpvWriter  *writer)
{
  return variable->class->load_spv (variable, writer);
}

void
gsk_sl_variable_store_spv (GskSlVariable *variable,
                           GskSpvWriter  *writer,
                           guint32        value)
{
  variable->class->store_spv (variable, writer, value);
}

