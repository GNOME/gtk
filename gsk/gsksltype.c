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

#include <string.h>

struct _GskSlType
{
  int ref_count;

  GskSlBuiltinType builtin;
};

static GskSlType
builtin_types[GSK_SL_N_BUILTIN_TYPES] = {
  [GSK_SL_VOID] = { 1, GSK_SL_VOID },
  [GSK_SL_FLOAT] = { 1, GSK_SL_FLOAT },
  [GSK_SL_DOUBLE] = { 1, GSK_SL_DOUBLE },
  [GSK_SL_INT] = { 1, GSK_SL_INT },
  [GSK_SL_UINT] = { 1, GSK_SL_UINT },
  [GSK_SL_BOOL] = { 1, GSK_SL_BOOL },
  [GSK_SL_VEC2] = { 1, GSK_SL_VEC2 },
  [GSK_SL_VEC3] = { 1, GSK_SL_VEC3 },
  [GSK_SL_VEC4] = { 1, GSK_SL_VEC4 }
};

GskSlType *
gsk_sl_type_new_parse (GskSlPreprocessor *stream)
{
  GskSlBuiltinType builtin;
  const GskSlToken *token;

  token = gsk_sl_preprocessor_get (stream);

  switch ((guint) token->type)
  {
    case GSK_SL_TOKEN_VOID:
      builtin = GSK_SL_VOID;
      break;
    case GSK_SL_TOKEN_FLOAT:
      builtin = GSK_SL_FLOAT;
      break;
    case GSK_SL_TOKEN_DOUBLE:
      builtin = GSK_SL_DOUBLE;
      break;
    case GSK_SL_TOKEN_INT:
      builtin = GSK_SL_INT;
      break;
    case GSK_SL_TOKEN_UINT:
      builtin = GSK_SL_UINT;
      break;
    case GSK_SL_TOKEN_BOOL:
      builtin = GSK_SL_BOOL;
      break;
    case GSK_SL_TOKEN_VEC2:
      builtin = GSK_SL_VEC2;
      break;
    case GSK_SL_TOKEN_VEC3:
      builtin = GSK_SL_VEC3;
      break;
    case GSK_SL_TOKEN_VEC4:
      builtin = GSK_SL_VEC4;
      break;
    default:
      gsk_sl_preprocessor_error (stream, "Expected type specifier");
      return NULL;
  }

  gsk_sl_preprocessor_consume (stream, NULL);
  return gsk_sl_type_ref (gsk_sl_type_get_builtin (builtin));
}

GskSlType *
gsk_sl_type_get_builtin (GskSlBuiltinType builtin)
{
  g_assert (builtin < GSK_SL_N_BUILTIN_TYPES);

  return &builtin_types[builtin];
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

  g_assert_not_reached ();
}

void
gsk_sl_type_print (const GskSlType *type,
                   GString         *string)
{
  switch (type->builtin)
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
    case GSK_SL_VEC2:
      g_string_append (string, "vec2");
      break;
    case GSK_SL_VEC3:
      g_string_append (string, "vec3");
      break;
    case GSK_SL_VEC4:
      g_string_append (string, "vec4");
      break;
    /* add more above */
    case GSK_SL_N_BUILTIN_TYPES:
    default:
      g_assert_not_reached ();
      break;
  }
}
