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
#include "gsksltokenizerprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvalueprivate.h"
#include "gskspvwriterprivate.h"

struct _GskSlPointerType {
  int ref_count;

  GskSlType *type;

  gboolean local;
  GskSlDecorationAccess access;
};

GskSlPointerType *
gsk_sl_pointer_type_new (GskSlType             *type,
                         gboolean               local,
                         GskSlDecorationAccess  access)
{
  GskSlPointerType *result;

  result = g_slice_new0 (GskSlPointerType);

  result->ref_count = 1;
  result->type = gsk_sl_type_ref (type);
  result->local = local;
  result->access = access;

  return result;
}

static void
gsk_sl_decoration_list_set (GskSlPreprocessor *preproc,
                            GskSlDecorations  *list,
                            GskSlDecoration    decoration,
                            gint               value)
{
  list->values[decoration].set = TRUE;
  list->values[decoration].value = value;
}

static void
gsk_sl_decoration_list_add_simple (GskSlPreprocessor *preproc,
                                   GskSlDecorations  *list,
                                   GskSlDecoration    decoration)
{
  if (list->values[decoration].set)
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Duplicate qualifier.");
      return;
    }

  gsk_sl_decoration_list_set (preproc, list, decoration, 1);
}

static void
gsk_sl_decoration_list_parse_assignment (GskSlPreprocessor *preproc,
                                         GskSlScope        *scope,
                                         GskSlDecorations  *list,
                                         GskSlDecoration    decoration)
{
  GskSlExpression *expression;
  const GskSlToken *token;
  GskSlValue *value;
  GskSlType *type;

  gsk_sl_preprocessor_consume (preproc, NULL);
  
  token = gsk_sl_preprocessor_get (preproc);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_EQUAL))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected \"=\" sign to assign a value.");
      return;
    }
  gsk_sl_preprocessor_consume (preproc, NULL);

  expression = gsk_sl_expression_parse_constant (scope, preproc);
  if (expression == NULL)
    return;

  value = gsk_sl_expression_get_constant (expression);
  gsk_sl_expression_unref (expression);

  if (value == NULL)
    {
      gsk_sl_preprocessor_error (preproc, CONSTANT, "Expression is not constant.");
      return;
    }

  type = gsk_sl_value_get_type (value);
  if (gsk_sl_type_is_scalar (type) && gsk_sl_type_get_scalar_type (type) == GSK_SL_INT)
    {
      gsk_sl_decoration_list_set (preproc, list, decoration, *(gint32 *) gsk_sl_value_get_data (value));
    }
  else if (gsk_sl_type_is_scalar (type) && gsk_sl_type_get_scalar_type (type) == GSK_SL_UINT)
    {
      gsk_sl_decoration_list_set (preproc, list, decoration, *(guint32 *) gsk_sl_value_get_data (value));
    }
  else
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Type of expression is not an integer type, but %s", gsk_sl_type_get_name (type));
    }
  gsk_sl_value_free (value);
}

static void
gsk_sl_decoration_list_parse_layout (GskSlPreprocessor *preproc,
                                     GskSlScope        *scope,
                                     GskSlDecorations  *list)
{
  const GskSlToken *token;

  memset (list, 0, sizeof (GskSlDecorations));

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (preproc);

      if (gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_PAREN))
        {
          gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected layout identifier.");
          break;
        }
      else if (gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
        {
          if (g_str_equal (token->str, "location"))
            {
              gsk_sl_decoration_list_parse_assignment (preproc,
                                                       scope,
                                                       list, 
                                                       GSK_SL_DECORATION_LAYOUT_LOCATION);
            }
          else if (g_str_equal (token->str, "component"))
            {
              gsk_sl_decoration_list_parse_assignment (preproc,
                                                       scope,
                                                       list, 
                                                       GSK_SL_DECORATION_LAYOUT_COMPONENT);
            }
          else if (g_str_equal (token->str, "binding"))
            {
              gsk_sl_decoration_list_parse_assignment (preproc,
                                                       scope,
                                                       list, 
                                                       GSK_SL_DECORATION_LAYOUT_BINDING);
            }
          else if (g_str_equal (token->str, "set"))
            {
              gsk_sl_decoration_list_parse_assignment (preproc,
                                                       scope,
                                                       list, 
                                                       GSK_SL_DECORATION_LAYOUT_SET);
            }
          else
            {
              gsk_sl_preprocessor_error (preproc, UNSUPPORTED, "Unknown layout identifier.");
              gsk_sl_preprocessor_consume (preproc, NULL);
            }
        }
      else
        {
          gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected layout identifier.");
          gsk_sl_preprocessor_consume (preproc, NULL);
          continue;
        }

      token = gsk_sl_preprocessor_get (preproc);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_COMMA))
        break;

      gsk_sl_preprocessor_consume (preproc, NULL);
    }
}

void
gsk_sl_decoration_list_parse (GskSlScope        *scope,
                              GskSlPreprocessor *preproc,
                              GskSlDecorations  *list)
{
  const GskSlToken *token;

  memset (list, 0, sizeof (GskSlDecorations));

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (preproc);
      switch ((guint) token->type)
      {
        case GSK_SL_TOKEN_CONST:
          gsk_sl_decoration_list_add_simple (preproc, list, GSK_SL_DECORATION_CONST);
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_IN:
          if (list->values[GSK_SL_DECORATION_CALLER_ACCESS].value & GSK_SL_DECORATION_ACCESS_READ)
            {
              gsk_sl_preprocessor_error (preproc, SYNTAX, "\"in\" qualifier specified twice.");
            }
          else
            {
              gsk_sl_decoration_list_set (preproc, list,
                                          GSK_SL_DECORATION_CALLER_ACCESS,
                                          list->values[GSK_SL_DECORATION_CALLER_ACCESS].value | GSK_SL_DECORATION_ACCESS_READ);
            }
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_OUT:
          if (list->values[GSK_SL_DECORATION_CALLER_ACCESS].value & GSK_SL_DECORATION_ACCESS_WRITE)
            {
              gsk_sl_preprocessor_error (preproc, SYNTAX, "\"out\" qualifier specified twice.");
            }
          else
            {
              gsk_sl_decoration_list_set (preproc, list,
                                          GSK_SL_DECORATION_CALLER_ACCESS,
                                          list->values[GSK_SL_DECORATION_CALLER_ACCESS].value | GSK_SL_DECORATION_ACCESS_WRITE);
            }
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_INOUT:
          if (list->values[GSK_SL_DECORATION_CALLER_ACCESS].value & GSK_SL_DECORATION_ACCESS_READ)
            {
              gsk_sl_preprocessor_error (preproc, SYNTAX, "\"in\" qualifier already used.");
            }
          else if (list->values[GSK_SL_DECORATION_CALLER_ACCESS].value & GSK_SL_DECORATION_ACCESS_WRITE)
            {
              gsk_sl_preprocessor_error (preproc, SYNTAX, "\"out\" qualifier already used.");
            }
          else
            {
              gsk_sl_decoration_list_set (preproc, list,
                                          GSK_SL_DECORATION_CALLER_ACCESS,
                                          GSK_SL_DECORATION_ACCESS_READWRITE);
            }
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_INVARIANT:
          gsk_sl_decoration_list_add_simple (preproc, list, GSK_SL_DECORATION_INVARIANT);
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_COHERENT:
          gsk_sl_decoration_list_add_simple (preproc, list, GSK_SL_DECORATION_COHERENT);
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_VOLATILE:
          gsk_sl_decoration_list_add_simple (preproc, list, GSK_SL_DECORATION_VOLATILE);
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_RESTRICT:
          gsk_sl_decoration_list_add_simple (preproc, list, GSK_SL_DECORATION_RESTRICT);
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;


        case GSK_SL_TOKEN_READONLY:
          if (list->values[GSK_SL_DECORATION_ACCESS].value & GSK_SL_DECORATION_ACCESS_READ)
            {
              gsk_sl_preprocessor_error (preproc, SYNTAX, "\"readonly\" qualifier specified twice.");
            }
          else if (list->values[GSK_SL_DECORATION_ACCESS].value & GSK_SL_DECORATION_ACCESS_WRITE)
            {
              gsk_sl_preprocessor_error (preproc, SYNTAX, "\"writeonly\" qualifier already used.");
            }
          else
            {
              gsk_sl_decoration_list_set (preproc, list,
                                          GSK_SL_DECORATION_ACCESS,
                                          GSK_SL_DECORATION_ACCESS_READ);
            }
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_WRITEONLY:
          if (list->values[GSK_SL_DECORATION_ACCESS].value & GSK_SL_DECORATION_ACCESS_READ)
            {
              gsk_sl_preprocessor_error (preproc, SYNTAX, "\"readonly\" qualifier already used.");
            }
          else if (list->values[GSK_SL_DECORATION_ACCESS].value & GSK_SL_DECORATION_ACCESS_WRITE)
            {
              gsk_sl_preprocessor_error (preproc, SYNTAX, "\"writeonly\" qualifier specified twice.");
            }
          else
            {
              gsk_sl_decoration_list_set (preproc, list,
                                          GSK_SL_DECORATION_ACCESS,
                                          GSK_SL_DECORATION_ACCESS_WRITE);
            }
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_LAYOUT:
          gsk_sl_preprocessor_consume (preproc, NULL);
          token = gsk_sl_preprocessor_get (preproc);
          if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_PAREN))
            {
              gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected opening \"(\" after layout specifier");
              break;
            }
          gsk_sl_preprocessor_consume (preproc, NULL);

          gsk_sl_decoration_list_parse_layout (preproc, scope, list);

          token = gsk_sl_preprocessor_get (preproc);
          if (gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_PAREN))
            {
              gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected closing \")\" at end of layout specifier");
              gsk_sl_preprocessor_sync (preproc, GSK_SL_TOKEN_RIGHT_PAREN);
            }
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        default:
          return;
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
  if (type->access == GSK_SL_DECORATION_ACCESS_READWRITE)
    g_string_append (string, "inout ");
  else if (type->access == GSK_SL_DECORATION_ACCESS_READ)
    g_string_append (string, "in ");
  else if (type->access == GSK_SL_DECORATION_ACCESS_WRITE)
    g_string_append (string, "out ");

  g_string_append (string, gsk_sl_type_get_name (type->type));
}

GskSlType *
gsk_sl_pointer_type_get_type (const GskSlPointerType *type)
{
  return type->type;
}

GskSpvStorageClass
gsk_sl_pointer_type_get_storage_class (const GskSlPointerType *type)
{
  if (type->local)
    return GSK_SPV_STORAGE_CLASS_FUNCTION;

  if (type->access & GSK_SL_DECORATION_ACCESS_WRITE)
    return GSK_SPV_STORAGE_CLASS_OUTPUT;

  if (type->access & GSK_SL_DECORATION_ACCESS_READ)
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
