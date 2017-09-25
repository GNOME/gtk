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

#include "gskslpreprocessorprivate.h"
#include "gsksltokenizerprivate.h"
#include "gsksltypeprivate.h"
#include "gskspvwriterprivate.h"

struct _GskSlPointerType {
  int ref_count;

  GskSlType *type;

  GskSlPointerTypeFlags flags;
};

GskSlPointerType *
gsk_sl_pointer_type_new (GskSlType             *type,
                         GskSlPointerTypeFlags  flags)
{
  GskSlPointerType *result;

  result = g_slice_new0 (GskSlPointerType);

  result->ref_count = 1;
  result->type = gsk_sl_type_ref (type);
  result->flags = flags;

  return result;
}

gboolean
gsk_sl_type_qualifier_parse (GskSlPreprocessor     *stream,
                             GskSlPointerTypeFlags  allowed_flags,
                             GskSlPointerTypeFlags *parsed_flags)
{
  const GskSlToken *token;
  guint flags = 0;
  gboolean success = TRUE;

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      switch ((guint) token->type)
      {
        case GSK_SL_TOKEN_CONST:
          if (!(allowed_flags & GSK_SL_POINTER_TYPE_CONST))
            {
              gsk_sl_preprocessor_error (stream, "\"const\" qualifier not allowed here.");
              success = FALSE;
            }
          else if (flags & GSK_SL_POINTER_TYPE_CONST)
            {
              gsk_sl_preprocessor_error (stream, "\"const\" qualifier specified twice.");
              success = FALSE;
            }
          else
            {
              flags |=  GSK_SL_POINTER_TYPE_CONST;
            }
          gsk_sl_preprocessor_consume (stream, NULL);
          break;

        case GSK_SL_TOKEN_IN:
          if (!(allowed_flags & GSK_SL_POINTER_TYPE_IN))
            {
              gsk_sl_preprocessor_error (stream, "\"in\" qualifier not allowed here.");
              success = FALSE;
            }
          else if (flags & GSK_SL_POINTER_TYPE_IN)
            {
              gsk_sl_preprocessor_error (stream, "\"in\" qualifier specified twice.");
              success = FALSE;
            }
          else
            {
              flags |= GSK_SL_POINTER_TYPE_IN;
            }
          gsk_sl_preprocessor_consume (stream, NULL);
          break;

        case GSK_SL_TOKEN_OUT:
          if (!(allowed_flags & GSK_SL_POINTER_TYPE_OUT))
            {
              gsk_sl_preprocessor_error (stream, "\"out\" qualifier not allowed here.");
              success = FALSE;
            }
          else if (flags & GSK_SL_POINTER_TYPE_OUT)
            {
              gsk_sl_preprocessor_error (stream, "\"out\" qualifier specified twice.");
              success = FALSE;
            }
          else
            {
              flags |= GSK_SL_POINTER_TYPE_OUT;
            }
          gsk_sl_preprocessor_consume (stream, NULL);
          break;

        case GSK_SL_TOKEN_INOUT:
          if ((allowed_flags & (GSK_SL_POINTER_TYPE_IN | GSK_SL_POINTER_TYPE_OUT)) != (GSK_SL_POINTER_TYPE_IN | GSK_SL_POINTER_TYPE_OUT))
            {
              gsk_sl_preprocessor_error (stream, "\"inout\" qualifier not allowed here.");
              success = FALSE;
            }
          else if (flags & GSK_SL_POINTER_TYPE_IN)
            {
              gsk_sl_preprocessor_error (stream, "\"in\" qualifier already used.");
              success = FALSE;
            }
          else if (flags & GSK_SL_POINTER_TYPE_OUT)
            {
              gsk_sl_preprocessor_error (stream, "\"out\" qualifier already used.");
              success = FALSE;
            }
          else
            {
              flags |= GSK_SL_POINTER_TYPE_IN | GSK_SL_POINTER_TYPE_OUT;
            }
          gsk_sl_preprocessor_consume (stream, NULL);
          break;

        case GSK_SL_TOKEN_INVARIANT:
          if (!(allowed_flags & GSK_SL_POINTER_TYPE_INVARIANT))
            {
              gsk_sl_preprocessor_error (stream, "\"invariant\" qualifier not allowed here.");
              success = FALSE;
            }
          else if (flags & GSK_SL_POINTER_TYPE_INVARIANT)
            {
              gsk_sl_preprocessor_error (stream, "\"invariant\" qualifier specified twice.");
              success = FALSE;
            }
          else
            {
              flags |= GSK_SL_POINTER_TYPE_INVARIANT;
            }
          gsk_sl_preprocessor_consume (stream, NULL);
          break;

        case GSK_SL_TOKEN_COHERENT:
          if (!(allowed_flags & GSK_SL_POINTER_TYPE_COHERENT))
            {
              gsk_sl_preprocessor_error (stream, "\"coherent\" qualifier not allowed here.");
              success = FALSE;
            }
          else if (flags & GSK_SL_POINTER_TYPE_COHERENT)
            {
              gsk_sl_preprocessor_error (stream, "\"coherent\" qualifier specified twice.");
              success = FALSE;
            }
          else
            {
              flags |= GSK_SL_POINTER_TYPE_COHERENT;
            }
          gsk_sl_preprocessor_consume (stream, NULL);
          break;

        case GSK_SL_TOKEN_VOLATILE:
          if (!(allowed_flags & GSK_SL_POINTER_TYPE_VOLATILE))
            {
              gsk_sl_preprocessor_error (stream, "\"volatile\" qualifier not allowed here.");
              success = FALSE;
            }
          else if (flags & GSK_SL_POINTER_TYPE_VOLATILE)
            {
              gsk_sl_preprocessor_error (stream, "\"volatile\" qualifier specified twice.");
              success = FALSE;
            }
          else
            {
              flags |= GSK_SL_POINTER_TYPE_VOLATILE;
            }
          gsk_sl_preprocessor_consume (stream, NULL);
          break;

        case GSK_SL_TOKEN_RESTRICT:
          if (!(allowed_flags & GSK_SL_POINTER_TYPE_RESTRICT))
            {
              gsk_sl_preprocessor_error (stream, "\"restrict\" qualifier not allowed here.");
              success = FALSE;
            }
          else if (flags & GSK_SL_POINTER_TYPE_RESTRICT)
            {
              gsk_sl_preprocessor_error (stream, "\"restrict\" qualifier specified twice.");
              success = FALSE;
            }
          else
            {
              flags |= GSK_SL_POINTER_TYPE_RESTRICT;
            }
          gsk_sl_preprocessor_consume (stream, NULL);
          break;


        case GSK_SL_TOKEN_READONLY:
          if (!(allowed_flags & GSK_SL_POINTER_TYPE_READONLY))
            {
              gsk_sl_preprocessor_error (stream, "\"readonly\" qualifier not allowed here.");
              success = FALSE;
            }
          else if (flags & GSK_SL_POINTER_TYPE_READONLY)
            {
              gsk_sl_preprocessor_error (stream, "\"readonly\" qualifier specified twice.");
              success = FALSE;
            }
          else if (flags & GSK_SL_POINTER_TYPE_WRITEONLY)
            {
              gsk_sl_preprocessor_error (stream, "\"writeonly\" qualifier already used.");
              success = FALSE;
            }
          else
            {
              flags |= GSK_SL_POINTER_TYPE_READONLY;
            }
          gsk_sl_preprocessor_consume (stream, NULL);
          break;

        case GSK_SL_TOKEN_WRITEONLY:
          if (!(allowed_flags & GSK_SL_POINTER_TYPE_WRITEONLY))
            {
              gsk_sl_preprocessor_error (stream, "\"writeonly\" qualifier not allowed here.");
              success = FALSE;
            }
          else if (flags & GSK_SL_POINTER_TYPE_READONLY)
            {
              gsk_sl_preprocessor_error (stream, "\"readonly\" qualifier already used.");
              success = FALSE;
            }
          else if (flags & GSK_SL_POINTER_TYPE_WRITEONLY)
            {
              gsk_sl_preprocessor_error (stream, "\"writeonly\" qualifier specified twice.");
              success = FALSE;
            }
          else
            {
              flags |= GSK_SL_POINTER_TYPE_WRITEONLY;
            }
          gsk_sl_preprocessor_consume (stream, NULL);
          break;

        default:
          {
            *parsed_flags = flags;

            return success;
          }
      }
    }
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
                           GString                *string)
{
  if (type->flags & GSK_SL_POINTER_TYPE_CONST)
    g_string_append (string, "const ");
  if (type->flags & GSK_SL_POINTER_TYPE_OUT)
    {
      if (type->flags & GSK_SL_POINTER_TYPE_IN)
        g_string_append (string, "inout ");
      else
        g_string_append (string, "out ");
    }
  else if (type->flags & GSK_SL_POINTER_TYPE_IN)
    {
      g_string_append (string, "out ");
    }
  if (type->flags & GSK_SL_POINTER_TYPE_INVARIANT)
    g_string_append (string, "invariant ");
  if (type->flags & GSK_SL_POINTER_TYPE_COHERENT)
    g_string_append (string, "coherent ");
  if (type->flags & GSK_SL_POINTER_TYPE_VOLATILE)
    g_string_append (string, "volatile ");
  if (type->flags & GSK_SL_POINTER_TYPE_RESTRICT)
    g_string_append (string, "restrict ");
  if (type->flags & GSK_SL_POINTER_TYPE_READONLY)
    g_string_append (string, "readonly ");
  if (type->flags & GSK_SL_POINTER_TYPE_WRITEONLY)
    g_string_append (string, "writeonly ");

  g_string_append (string, gsk_sl_type_get_name (type->type));
}

GskSlType *
gsk_sl_pointer_type_get_type (const GskSlPointerType *type)
{
  return type->type;
}

gboolean
gsk_sl_pointer_type_is_const (const GskSlPointerType *type)
{
  return type->flags & GSK_SL_POINTER_TYPE_CONST ? TRUE : FALSE;
}

gboolean
gsk_sl_pointer_type_is_in (const GskSlPointerType *type)
{
  return type->flags & GSK_SL_POINTER_TYPE_IN ? TRUE : FALSE;
}

gboolean
gsk_sl_pointer_type_is_out (const GskSlPointerType *type)
{
  return type->flags & GSK_SL_POINTER_TYPE_OUT ? TRUE : FALSE;
}

gboolean
gsk_sl_pointer_type_is_invariant (const GskSlPointerType *type)
{
  return type->flags & GSK_SL_POINTER_TYPE_INVARIANT ? TRUE : FALSE;
}

gboolean
gsk_sl_pointer_type_is_coherent (const GskSlPointerType *type)
{
  return type->flags & GSK_SL_POINTER_TYPE_COHERENT ? TRUE : FALSE;
}

gboolean
gsk_sl_pointer_type_is_volatile (const GskSlPointerType *type)
{
  return type->flags & GSK_SL_POINTER_TYPE_VOLATILE ? TRUE : FALSE;
}

gboolean
gsk_sl_pointer_type_is_restrict (const GskSlPointerType *type)
{
  return type->flags & GSK_SL_POINTER_TYPE_RESTRICT ? TRUE : FALSE;
}

gboolean
gsk_sl_pointer_type_is_readonly (const GskSlPointerType *type)
{
  return type->flags & GSK_SL_POINTER_TYPE_READONLY ? TRUE : FALSE;
}

gboolean
gsk_sl_pointer_type_is_writeonly (const GskSlPointerType *type)
{
  return type->flags & GSK_SL_POINTER_TYPE_WRITEONLY ? TRUE : FALSE;
}

GskSpvStorageClass
gsk_sl_pointer_type_get_storage_class (const GskSlPointerType *type)
{
  if (type->flags & GSK_SL_POINTER_TYPE_LOCAL)
    return GSK_SPV_STORAGE_CLASS_FUNCTION;

  if (type->flags & GSK_SL_POINTER_TYPE_OUT)
    return GSK_SPV_STORAGE_CLASS_OUTPUT;

  if (type->flags & GSK_SL_POINTER_TYPE_IN)
    return GSK_SPV_STORAGE_CLASS_INPUT;

  return GSK_SPV_STORAGE_CLASS_PRIVATE;
}

gboolean
gsk_sl_pointer_type_equal (gconstpointer a,
                           gconstpointer b)
{
  const GskSlPointerType *typea = a;
  const GskSlPointerType *typeb = b;

  if (!gsk_sl_type_equal (typea->type, typeb->type))
    return FALSE;

  return gsk_sl_pointer_type_get_storage_class (typea)
      == gsk_sl_pointer_type_get_storage_class (typeb);
}

guint
gsk_sl_pointer_type_hash (gconstpointer t)
{
  const GskSlPointerType *type = t;

  return gsk_sl_type_hash (type->type)
       ^ gsk_sl_pointer_type_get_storage_class (type);
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
                                     gsk_sl_pointer_type_get_storage_class (type),
                                     type_id });

  return result_id;
}
