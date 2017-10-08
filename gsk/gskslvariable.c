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

struct _GskSlVariable {
  guint ref_count;

  char *name;
  GskSlType *type;
  GskSlQualifier qualifier;
  GskSlValue *initial_value;
};

GskSlVariable *
gsk_sl_variable_new (const char           *name,
                     GskSlType            *type,
                     const GskSlQualifier *qualifier,
                     GskSlValue           *initial_value)
{
  GskSlVariable *variable;

  g_return_val_if_fail (initial_value == NULL || gsk_sl_type_equal (type, gsk_sl_value_get_type (initial_value)), NULL);
  variable = g_slice_new0 (GskSlVariable);

  variable->ref_count = 1;
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

  if (variable->initial_value)
    gsk_sl_value_free (variable->initial_value);
  gsk_sl_type_unref (variable->type);
  g_free (variable->name);

  g_slice_free (GskSlVariable, variable);
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
  guint32 result_id;
  GskSlQualifierLocation location;
  
  location = gsk_sl_qualifier_get_location (&variable->qualifier);

  if (location == GSK_SL_QUALIFIER_PARAMETER)
    {
      result_id = gsk_spv_writer_function_parameter (writer,
                                                     variable->type);
    }
  else
    {
      guint32 value_id;

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
    }

  if (variable->name)
    gsk_spv_writer_name (writer, result_id, variable->name);

  return result_id;
}
