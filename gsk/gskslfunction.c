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

#include "gskslfunctionprivate.h"

#include "gsksltypeprivate.h"

static GskSlFunction *
gsk_sl_function_alloc (const GskSlFunctionClass *klass,
                       gsize                     size)
{
  GskSlFunction *function;

  function = g_slice_alloc0 (size);

  function->class = klass;
  function->ref_count = 1;

  return function;
}
#define gsk_sl_function_new(_name, _klass) ((_name *) gsk_sl_function_alloc ((_klass), sizeof (_name)))

/* BUILTIN CONSTRUCTOR */

typedef struct _GskSlFunctionConstructor GskSlFunctionConstructor;

struct _GskSlFunctionConstructor {
  GskSlFunction parent;

  GskSlType *type;
};

static void
gsk_sl_function_constructor_free (GskSlFunction *function)
{
  GskSlFunctionConstructor *constructor = (GskSlFunctionConstructor *) function;

  gsk_sl_type_unref (constructor->type);

  g_slice_free (GskSlFunctionConstructor, constructor);
}

static GskSlType *
gsk_sl_function_constructor_get_return_type (GskSlFunction *function)
{
  GskSlFunctionConstructor *constructor = (GskSlFunctionConstructor *) function;

  return constructor->type;
}

static void
gsk_sl_function_constructor_print_name (GskSlFunction *function,
                                                GString       *string)
{
  GskSlFunctionConstructor *constructor = (GskSlFunctionConstructor *) function;

  gsk_sl_type_print (constructor->type, string);
}


static guint
gsk_sl_function_builtin_get_args_by_type (const GskSlType *type)
{
  if (gsk_sl_type_is_scalar (type))
    return 1;
  else if (gsk_sl_type_is_vector (type))
    return gsk_sl_type_get_length (type);
  else if (gsk_sl_type_is_matrix (type))
    return gsk_sl_type_get_length (type) * gsk_sl_function_builtin_get_args_by_type (gsk_sl_type_get_index_type (type));
  else
    return 0;
}

static gboolean
gsk_sl_function_constructor_matches (GskSlFunction  *function,
                                             GskSlType     **arguments,
                                             gsize           n_arguments,
                                             GError        **error)
{
  GskSlFunctionConstructor *constructor = (GskSlFunctionConstructor *) function;
  guint needed, provided;
  gsize i;

  if (n_arguments == 1 && gsk_sl_type_is_scalar (arguments[0]))
    return TRUE;

  needed = gsk_sl_function_builtin_get_args_by_type (constructor->type);

  for (i = 0; i < n_arguments; i++)
    {
      if (needed == 0)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Too many arguments given to constructor, only the first %"G_GSIZE_FORMAT" are necessary.", i);
          return FALSE;
        }

      provided = gsk_sl_function_builtin_get_args_by_type (arguments[i]);
      if (provided == 0)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Invalid type for constructor in argument %"G_GSIZE_FORMAT, i + 1);
          return FALSE;
        }

      needed -= MIN (needed, provided);
    }

  return TRUE;
}

static const GskSlFunctionClass GSK_SL_FUNCTION_CONSTRUCTOR = {
  gsk_sl_function_constructor_free,
  gsk_sl_function_constructor_get_return_type,
  gsk_sl_function_constructor_print_name,
  gsk_sl_function_constructor_matches,
};

/* API */

GskSlFunction *
gsk_sl_function_new_constructor (GskSlType *type)
{
  GskSlFunctionConstructor *constructor;

  constructor = gsk_sl_function_new (GskSlFunctionConstructor, &GSK_SL_FUNCTION_CONSTRUCTOR);

  constructor->type = gsk_sl_type_ref (type);

  return &constructor->parent;
}

GskSlFunction *
gsk_sl_function_ref (GskSlFunction *function)
{
  g_return_val_if_fail (function != NULL, NULL);

  function->ref_count += 1;

  return function;
}

void
gsk_sl_function_unref (GskSlFunction *function)
{
  if (function == NULL)
    return;

  function->ref_count -= 1;
  if (function->ref_count > 0)
    return;

  function->class->free (function);
}

GskSlType *
gsk_sl_function_get_return_type (GskSlFunction *function)
{
  return function->class->get_return_type (function);
}

void
gsk_sl_function_print_name (GskSlFunction *function,
                            GString       *string)
{
  function->class->print_name (function, string);
}

gboolean
gsk_sl_function_matches (GskSlFunction  *function,
                         GskSlType     **arguments,
                         gsize           n_arguments,
                         GError        **error)
{
  return function->class->matches (function, arguments, n_arguments, error);
}

