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

#include "gskslqualifierprivate.h"

#include "gskslexpressionprivate.h"
#include "gskslpreprocessorprivate.h"
#include "gskslprinterprivate.h"
#include "gsksltokenizerprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvalueprivate.h"

void
gsk_sl_qualifier_init (GskSlQualifier *qualifier)
{
  *qualifier = (GskSlQualifier) { .storage = GSK_SL_STORAGE_DEFAULT,
                                  .layout = { -1, -1, -1, -1 },
                                };
}

static void
gsk_sl_qualifier_parse_layout_assignment (GskSlPreprocessor *preproc,
                                          GskSlScope        *scope,
                                          int               *target)
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
      gint32 i = *(gint32 *) gsk_sl_value_get_data (value);

      if (i < 0)
        gsk_sl_preprocessor_error (preproc, CONSTANT, "Expression may not be negative.");
      else
        *target = i;
    }
  else if (gsk_sl_type_is_scalar (type) && gsk_sl_type_get_scalar_type (type) == GSK_SL_UINT)
    {
      *target = *(guint32 *) gsk_sl_value_get_data (value);
    }
  else
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Type of expression is not an integer type, but %s", gsk_sl_type_get_name (type));
    }
  gsk_sl_value_free (value);
}

static void
gsk_sl_qualifier_parse_layout (GskSlQualifier    *qualifier,
                               GskSlPreprocessor *preproc,
                               GskSlScope        *scope)
{
  const GskSlToken *token;

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
            gsk_sl_qualifier_parse_layout_assignment (preproc, scope, &qualifier->layout.location);
          else if (g_str_equal (token->str, "component"))
            gsk_sl_qualifier_parse_layout_assignment (preproc, scope, &qualifier->layout.component);
          else if (g_str_equal (token->str, "binding"))
            gsk_sl_qualifier_parse_layout_assignment (preproc, scope, &qualifier->layout.binding);
          else if (g_str_equal (token->str, "set"))
            gsk_sl_qualifier_parse_layout_assignment (preproc, scope, &qualifier->layout.set);
          else if (g_str_equal (token->str, "push_constant"))
            {
              qualifier->layout.push_constant = TRUE;
              gsk_sl_preprocessor_consume (preproc, NULL);
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
        }

      token = gsk_sl_preprocessor_get (preproc);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_COMMA))
        break;

      gsk_sl_preprocessor_consume (preproc, NULL);
    }
}

static const char * 
gsk_sl_storage_get_name (GskSlStorage storage)
{
  switch (storage)
    {
    default:
    case GSK_SL_STORAGE_DEFAULT:
      g_assert_not_reached ();
      return "???";
    case GSK_SL_STORAGE_GLOBAL:
    case GSK_SL_STORAGE_LOCAL:
    case GSK_SL_STORAGE_PARAMETER_IN:
      return "";
    case GSK_SL_STORAGE_GLOBAL_CONST:
    case GSK_SL_STORAGE_LOCAL_CONST:
    case GSK_SL_STORAGE_PARAMETER_CONST:
      return "const";
    case GSK_SL_STORAGE_GLOBAL_IN:
      return "in";
    case GSK_SL_STORAGE_GLOBAL_OUT:
    case GSK_SL_STORAGE_PARAMETER_OUT:
      return "out";
    case GSK_SL_STORAGE_PARAMETER_INOUT:
      return "inout";
    case GSK_SL_STORAGE_GLOBAL_UNIFORM:
      return "uniform";
    }
}

static gboolean
gsk_sl_storage_allows_const (GskSlStorage storage)
{
  switch (storage)
    {
    case GSK_SL_STORAGE_GLOBAL_CONST:
    case GSK_SL_STORAGE_LOCAL_CONST:
    case GSK_SL_STORAGE_PARAMETER_CONST:
    default:
      g_assert_not_reached ();
    case GSK_SL_STORAGE_GLOBAL_OUT:
    case GSK_SL_STORAGE_GLOBAL_UNIFORM:
    case GSK_SL_STORAGE_PARAMETER_OUT:
    case GSK_SL_STORAGE_PARAMETER_INOUT:
      return FALSE;
    case GSK_SL_STORAGE_DEFAULT:
    case GSK_SL_STORAGE_GLOBAL:
    case GSK_SL_STORAGE_GLOBAL_IN:
    case GSK_SL_STORAGE_LOCAL:
    case GSK_SL_STORAGE_PARAMETER_IN:
      return TRUE;
    }
}

void
gsk_sl_qualifier_parse (GskSlQualifier         *qualifier,
                        GskSlScope             *scope,
                        GskSlPreprocessor      *preproc,
                        GskSlQualifierLocation  location)
{
  const GskSlToken *token;
  gboolean seen_const = FALSE;

  gsk_sl_qualifier_init (qualifier);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (preproc);
      switch ((guint) token->type)
      {
        case GSK_SL_TOKEN_CONST:
          if (seen_const)
            gsk_sl_preprocessor_error (preproc, SYNTAX, "Duplicate \"const\" qualifier.");
          if (!gsk_sl_storage_allows_const (qualifier->storage))
            gsk_sl_preprocessor_error (preproc, SYNTAX, "\"%s\" qualifier cannot be const.", gsk_sl_storage_get_name (qualifier->storage));
          else
            seen_const = TRUE;
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_IN:
          if (qualifier->storage == GSK_SL_STORAGE_DEFAULT)
            {
              if (location == GSK_SL_QUALIFIER_LOCAL)
                gsk_sl_preprocessor_error (preproc, SYNTAX, "Local variables cannot have \"in\" qualifier.");
              else
                qualifier->storage = location == GSK_SL_QUALIFIER_GLOBAL ? GSK_SL_STORAGE_GLOBAL_IN
                                                                         : GSK_SL_STORAGE_PARAMETER_IN;
            }
          else if (qualifier->storage == GSK_SL_STORAGE_PARAMETER_OUT)
            qualifier->storage = GSK_SL_STORAGE_PARAMETER_INOUT;
          else
            gsk_sl_preprocessor_error (preproc, SYNTAX, "Qualifiers \"%s\" and \"in\" cannot be combined.", gsk_sl_storage_get_name (qualifier->storage));
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_OUT:
          if (seen_const)
            gsk_sl_preprocessor_error (preproc, SYNTAX, "Const variables cannot have \"out\" qualifier.");
          else if (qualifier->storage == GSK_SL_STORAGE_DEFAULT)
            {
              if (location == GSK_SL_QUALIFIER_LOCAL)
                gsk_sl_preprocessor_error (preproc, SYNTAX, "Local variables cannot have \"out\" qualifier.");
              else
                qualifier->storage = location == GSK_SL_QUALIFIER_GLOBAL ? GSK_SL_STORAGE_GLOBAL_OUT
                                                                         : GSK_SL_STORAGE_PARAMETER_OUT;
            }
          else if (qualifier->storage == GSK_SL_STORAGE_PARAMETER_IN)
            qualifier->storage = GSK_SL_STORAGE_PARAMETER_INOUT;
          else
            gsk_sl_preprocessor_error (preproc, SYNTAX, "Qualifiers \"%s\" and \"out\" cannot be combined.", gsk_sl_storage_get_name (qualifier->storage));
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_INOUT:
          if (seen_const)
            gsk_sl_preprocessor_error (preproc, SYNTAX, "Const variables cannot have \"inout\" qualifier.");
          else if (qualifier->storage == GSK_SL_STORAGE_DEFAULT)
            {
              if (location != GSK_SL_QUALIFIER_PARAMETER)
                gsk_sl_preprocessor_error (preproc, SYNTAX, "\"inout\" can only be used on parameters.");
              else
                qualifier->storage = GSK_SL_STORAGE_PARAMETER_INOUT;
            }
          else
            gsk_sl_preprocessor_error (preproc, SYNTAX, "Qualifiers \"%s\" and \"inout\" cannot be combined.", gsk_sl_storage_get_name (qualifier->storage));
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_UNIFORM:
          if (seen_const)
            gsk_sl_preprocessor_error (preproc, SYNTAX, "Const variables cannot have \"uniform\" qualifier.");
          else if (qualifier->storage == GSK_SL_STORAGE_DEFAULT)
            {
              if (location != GSK_SL_QUALIFIER_GLOBAL)
                gsk_sl_preprocessor_error (preproc, SYNTAX, "\"uniform\" can only be used on globals.");
              else
                qualifier->storage = GSK_SL_STORAGE_GLOBAL_UNIFORM;
            }
          else
            gsk_sl_preprocessor_error (preproc, SYNTAX, "Qualifiers \"%s\" and \"uniform\" cannot be combined.", gsk_sl_storage_get_name (qualifier->storage));
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_INVARIANT:
          if (qualifier->invariant)
            gsk_sl_preprocessor_error (preproc, SYNTAX, "Duplicate \"invariant\" qualifier.");
          qualifier->invariant = TRUE;
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_COHERENT:
          if (qualifier->coherent)
            gsk_sl_preprocessor_error (preproc, SYNTAX, "Duplicate \"coherent\" qualifier.");
          qualifier->coherent = TRUE;
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_VOLATILE:
          if (qualifier->volatile_)
            gsk_sl_preprocessor_error (preproc, SYNTAX, "Duplicate \"volatile\" qualifier.");
          qualifier->volatile_ = TRUE;
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_RESTRICT:
          if (qualifier->restrict_)
            gsk_sl_preprocessor_error (preproc, SYNTAX, "Duplicate \"restrict\" qualifier.");
          qualifier->restrict_ = TRUE;
          break;

        case GSK_SL_TOKEN_READONLY:
          if (qualifier->readonly)
            gsk_sl_preprocessor_error (preproc, SYNTAX, "Duplicate \"readonly\" qualifier.");
          qualifier->readonly = TRUE;
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_WRITEONLY:
          if (qualifier->writeonly)
            gsk_sl_preprocessor_error (preproc, SYNTAX, "Duplicate \"writeonly\" qualifier.");
          qualifier->writeonly = TRUE;
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        case GSK_SL_TOKEN_LAYOUT:
          if (location != GSK_SL_QUALIFIER_GLOBAL)
            gsk_sl_preprocessor_error (preproc, SYNTAX, "Only global variables can have layout qualifiers.");
          gsk_sl_preprocessor_consume (preproc, NULL);
          token = gsk_sl_preprocessor_get (preproc);
          if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_PAREN))
            {
              gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected opening \"(\" after layout specifier");
              break;
            }
          gsk_sl_preprocessor_consume (preproc, NULL);

          gsk_sl_qualifier_parse_layout (qualifier, preproc, scope);

          token = gsk_sl_preprocessor_get (preproc);
          if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_PAREN))
            {
              gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected closing \")\" at end of layout specifier");
              gsk_sl_preprocessor_sync (preproc, GSK_SL_TOKEN_RIGHT_PAREN);
            }
          gsk_sl_preprocessor_consume (preproc, NULL);
          break;

        default:
          goto out;
      }
    }

out:
  /* fixup storage qualifier */
  switch (qualifier->storage)
    {
    case GSK_SL_STORAGE_DEFAULT:
      if (location == GSK_SL_QUALIFIER_GLOBAL)
        qualifier->storage = seen_const ? GSK_SL_STORAGE_GLOBAL_CONST : GSK_SL_STORAGE_GLOBAL;
      else if (location == GSK_SL_QUALIFIER_LOCAL)
        qualifier->storage = seen_const ? GSK_SL_STORAGE_LOCAL_CONST : GSK_SL_STORAGE_LOCAL;
      else if (location == GSK_SL_QUALIFIER_PARAMETER)
        qualifier->storage = seen_const ? GSK_SL_STORAGE_PARAMETER_CONST : GSK_SL_STORAGE_PARAMETER_IN;
      else
        {
          g_assert_not_reached ();
        }
      break;

    case GSK_SL_STORAGE_GLOBAL:
      if (seen_const)
        qualifier->storage = GSK_SL_STORAGE_GLOBAL_CONST;
      break;

    case GSK_SL_STORAGE_LOCAL:
      if (seen_const)
        qualifier->storage = GSK_SL_STORAGE_LOCAL_CONST;
      break;

    case GSK_SL_STORAGE_PARAMETER_IN:
      if (seen_const)
        qualifier->storage = GSK_SL_STORAGE_PARAMETER_CONST;
      break;

    case GSK_SL_STORAGE_GLOBAL_IN:
    case GSK_SL_STORAGE_GLOBAL_OUT:
    case GSK_SL_STORAGE_GLOBAL_UNIFORM:
    case GSK_SL_STORAGE_PARAMETER_OUT:
    case GSK_SL_STORAGE_PARAMETER_INOUT:
      g_assert (!seen_const);
      break;

    case GSK_SL_STORAGE_GLOBAL_CONST:
    case GSK_SL_STORAGE_LOCAL_CONST:
    case GSK_SL_STORAGE_PARAMETER_CONST:
    default:
      g_assert_not_reached ();
      break;
    }
}

static gboolean
gsk_sl_qualifier_has_layout (const GskSlQualifier *qualifier)
{
  return qualifier->layout.set >= 0
      || qualifier->layout.binding >= 0
      || qualifier->layout.location >= 0
      || qualifier->layout.component >= 0
      || qualifier->layout.push_constant;
}

static gboolean
print_qualifier (GskSlPrinter *printer,
                 const char   *name,
                 gint          value,
                 gboolean      needs_comma)
{
  if (value < 0)
    return needs_comma;

  if (needs_comma)
    gsk_sl_printer_append (printer, ", ");
  gsk_sl_printer_append (printer, name);
  gsk_sl_printer_append (printer, "=");
  gsk_sl_printer_append_uint (printer, value);

  return TRUE;
}

static gboolean
append_with_space (GskSlPrinter *printer,
                   const char   *s,
                   gboolean      need_space)
{
  if (s[0] == '\0')
    return need_space;

  if (need_space)
    gsk_sl_printer_append_c (printer, ' ');
  gsk_sl_printer_append (printer, s);
  return TRUE;
}

gboolean
gsk_sl_qualifier_print (const GskSlQualifier *qualifier,
                        GskSlPrinter         *printer)
{
  gboolean need_space = FALSE;

  if (qualifier->invariant)
    need_space = append_with_space (printer, "invariant ", need_space);
  if (qualifier->volatile_)
    need_space = append_with_space (printer, "volatile ", need_space);
  if (qualifier->restrict_)
    need_space = append_with_space (printer, "restrict ", need_space);
  if (qualifier->coherent)
    need_space = append_with_space (printer, "coherent ", need_space);
  if (qualifier->readonly)
    need_space = append_with_space (printer, "readonly ", need_space);
  if (qualifier->writeonly)
    need_space = append_with_space (printer, "writeonly ", need_space);

  if (gsk_sl_qualifier_has_layout (qualifier))
    {
      gboolean had_value;
      gsk_sl_printer_append (printer, "layout(");
      had_value = print_qualifier (printer, "set", qualifier->layout.set, FALSE);
      had_value = print_qualifier (printer, "binding", qualifier->layout.binding, had_value);
      had_value = print_qualifier (printer, "location", qualifier->layout.location, had_value);
      had_value = print_qualifier (printer, "component", qualifier->layout.component, had_value);
      if (qualifier->layout.push_constant)
        {
          if (had_value)
            gsk_sl_printer_append (printer, ", ");
          gsk_sl_printer_append (printer, "push_constant");
        }
      gsk_sl_printer_append (printer, ")");
      need_space = TRUE;
    }

  need_space = append_with_space (printer, gsk_sl_storage_get_name (qualifier->storage), need_space);

  return need_space;
}

gboolean
gsk_sl_qualifier_is_constant (const GskSlQualifier *qualifier)
{
  switch (qualifier->storage)
    {
    case GSK_SL_STORAGE_DEFAULT:
    default:
      g_assert_not_reached ();
      return TRUE;

    case GSK_SL_STORAGE_GLOBAL_UNIFORM:
    case GSK_SL_STORAGE_GLOBAL_CONST:
    case GSK_SL_STORAGE_LOCAL_CONST:
    case GSK_SL_STORAGE_PARAMETER_CONST:
      return TRUE;

    case GSK_SL_STORAGE_GLOBAL:
    case GSK_SL_STORAGE_GLOBAL_IN:
    case GSK_SL_STORAGE_GLOBAL_OUT:
    case GSK_SL_STORAGE_LOCAL:
    case GSK_SL_STORAGE_PARAMETER_IN:
    case GSK_SL_STORAGE_PARAMETER_OUT:
    case GSK_SL_STORAGE_PARAMETER_INOUT:
      return FALSE;
    }
}

GskSpvStorageClass
gsk_sl_qualifier_get_storage_class (const GskSlQualifier *qualifier)
{
  switch (qualifier->storage)
    {
    case GSK_SL_STORAGE_DEFAULT:
    case GSK_SL_STORAGE_PARAMETER_IN:
    case GSK_SL_STORAGE_PARAMETER_OUT:
    case GSK_SL_STORAGE_PARAMETER_INOUT:
    case GSK_SL_STORAGE_PARAMETER_CONST:
    default:
      g_assert_not_reached ();
      return GSK_SPV_STORAGE_CLASS_FUNCTION;

    case GSK_SL_STORAGE_GLOBAL:
    case GSK_SL_STORAGE_GLOBAL_CONST:
      return GSK_SPV_STORAGE_CLASS_PRIVATE;

    case GSK_SL_STORAGE_GLOBAL_IN:
      return GSK_SPV_STORAGE_CLASS_INPUT;

    case GSK_SL_STORAGE_GLOBAL_OUT:
      return GSK_SPV_STORAGE_CLASS_OUTPUT;

    case GSK_SL_STORAGE_GLOBAL_UNIFORM:
      if (qualifier->layout.push_constant)
        return GSK_SPV_STORAGE_CLASS_PUSH_CONSTANT;
      else
        return GSK_SPV_STORAGE_CLASS_UNIFORM;

    case GSK_SL_STORAGE_LOCAL:
    case GSK_SL_STORAGE_LOCAL_CONST:
      return GSK_SPV_STORAGE_CLASS_FUNCTION;
    }
}
