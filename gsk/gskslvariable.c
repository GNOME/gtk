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

#include "gskslpointertypeprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvalueprivate.h"
#include "gskspvwriterprivate.h"

struct _GskSlVariable {
  guint ref_count;

  GskSlPointerType *type;
  char *name;
  GskSlValue *initial_value;
  gboolean constant;
};

GskSlVariable *
gsk_sl_variable_new (GskSlPointerType *type,
                     char             *name,
                     GskSlValue       *initial_value,
                     gboolean          constant)
{
  GskSlVariable *variable;

  g_return_val_if_fail (initial_value == NULL || gsk_sl_type_equal (gsk_sl_pointer_type_get_type (type), gsk_sl_value_get_type (initial_value)), NULL);
  variable = g_slice_new0 (GskSlVariable);

  variable->ref_count = 1;
  variable->type = gsk_sl_pointer_type_ref (type);
  variable->name = name;
  variable->initial_value = initial_value;
  variable->constant = constant;

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
  gsk_sl_pointer_type_unref (variable->type);
  g_free (variable->name);

  g_slice_free (GskSlVariable, variable);
}

void
gsk_sl_variable_print (const GskSlVariable *variable,
                       GString             *string)
{
  if (variable->constant)
    g_string_append (string, "const ");
  gsk_sl_pointer_type_print (variable->type, string);
  if (variable->name)
    {
      g_string_append (string, " ");
      g_string_append (string, variable->name);
    }
}

GskSlPointerType *
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
  return variable->constant;
}

guint32
gsk_sl_variable_write_spv (const GskSlVariable *variable,
                           GskSpvWriter        *writer)
{
  guint32 pointer_type_id, variable_id;

  pointer_type_id = gsk_spv_writer_get_id_for_pointer_type (writer, variable->type);
  variable_id = gsk_spv_writer_next_id (writer);
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_CODE,
                      4, GSK_SPV_OP_VARIABLE,
                      (guint32[3]) { pointer_type_id,
                                     variable_id,
                                     gsk_sl_pointer_type_get_storage_class (variable->type)});

  return variable_id;
}
