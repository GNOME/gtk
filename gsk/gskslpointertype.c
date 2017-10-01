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

#include "gskslpointertypeprivate.h"

#include "gskslexpressionprivate.h"
#include "gskslpreprocessorprivate.h"
#include "gskslprinterprivate.h"
#include "gskslqualifierprivate.h"
#include "gsksltokenizerprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvalueprivate.h"
#include "gskspvwriterprivate.h"

struct _GskSlPointerType {
  int ref_count;

  GskSlType *type;
  GskSlQualifier qualifier;
};

GskSlPointerType *
gsk_sl_pointer_type_new (GskSlType             *type,
                         const GskSlQualifier  *qualifier)
{
  GskSlPointerType *result;

  result = g_slice_new0 (GskSlPointerType);

  result->ref_count = 1;
  result->type = gsk_sl_type_ref (type);
  result->qualifier = *qualifier;

  return result;
}

GskSlPointerType *
gsk_sl_pointer_type_ref (GskSlPointerType *type)
{
  g_return_val_if_fail (type != NULL, NULL);

  type->ref_count += 1;

  return type;
}

void
gsk_sl_pointer_type_unref (GskSlPointerType *type)
{
  if (type == NULL)
    return;

  type->ref_count -= 1;
  if (type->ref_count > 0)
    return;

  gsk_sl_type_unref (type->type);

  g_slice_free (GskSlPointerType, type);
}

void
gsk_sl_pointer_type_print (const GskSlPointerType *type,
                           GskSlPrinter           *printer)
{
  if (gsk_sl_qualifier_print (&type->qualifier, printer))
    gsk_sl_printer_append (printer, " ");
  gsk_sl_printer_append (printer, gsk_sl_type_get_name (type->type));
}

GskSlType *
gsk_sl_pointer_type_get_type (const GskSlPointerType *type)
{
  return type->type;
}

const GskSlQualifier *
gsk_sl_pointer_type_get_qualifier (const GskSlPointerType *type)
{
  return &type->qualifier;
}

gboolean
gsk_sl_pointer_type_equal (gconstpointer a,
                           gconstpointer b)
{
  const GskSlPointerType *typea = a;
  const GskSlPointerType *typeb = b;

  if (!gsk_sl_type_equal (typea->type, typeb->type))
    return FALSE;

  return gsk_sl_qualifier_get_storage_class (&typea->qualifier)
      == gsk_sl_qualifier_get_storage_class (&typeb->qualifier);
}

guint
gsk_sl_pointer_type_hash (gconstpointer t)
{
  const GskSlPointerType *type = t;

  return gsk_sl_type_hash (type->type)
       ^ gsk_sl_qualifier_get_storage_class (&type->qualifier);
}

guint32
gsk_sl_pointer_type_write_spv (const GskSlPointerType *type,
                               GskSpvWriter           *writer)
{
  guint32 type_id, result_id;

  type_id = gsk_spv_writer_get_id_for_type (writer, type->type);
  result_id = gsk_spv_writer_next_id (writer);

  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_DECLARE,
                      4, GSK_SPV_OP_TYPE_POINTER,
                      (guint32[3]) { result_id,
                                     gsk_sl_qualifier_get_storage_class (&type->qualifier),
                                     type_id });

  return result_id;
}
