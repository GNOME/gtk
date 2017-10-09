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

#include "gskslfunctiontypeprivate.h"

#include "gsksltypeprivate.h"
#include "gskspvwriterprivate.h"

typedef struct _GskSlArgument GskSlArgument;

struct _GskSlArgument
{
  GskSlStorage storage;
  GskSlType *type;
};

struct _GskSlFunctionType
{
  int ref_count;

  GskSlType *return_type;
  gsize n_arguments;
  GskSlArgument arguments[0];
};

static GskSlFunctionType *
gsk_sl_function_type_alloc (gsize n_arguments)
{
  GskSlFunctionType *function_type;

  function_type = g_slice_alloc0 (sizeof (GskSlFunctionType) + n_arguments * sizeof (GskSlArgument));

  function_type->ref_count = 1;
  function_type->n_arguments = n_arguments;

  return function_type;
}

GskSlFunctionType *
gsk_sl_function_type_new (GskSlType *return_type)
{
  GskSlFunctionType *function_type;

  function_type = gsk_sl_function_type_alloc (0);
  function_type->return_type = gsk_sl_type_ref (return_type);

  return function_type;
}

GskSlFunctionType *
gsk_sl_function_type_add_argument (GskSlFunctionType *function_type,
                                   GskSlStorage       argument_storage,
                                   GskSlType         *argument_type)
{
  GskSlFunctionType *result_type;
  gsize i;

  g_assert (argument_storage >= GSK_SL_STORAGE_PARAMETER_IN && argument_storage <= GSK_SL_STORAGE_PARAMETER_CONST);

  result_type = gsk_sl_function_type_alloc (function_type->n_arguments + 1);
  result_type->return_type = gsk_sl_type_ref (function_type->return_type);

  for (i = 0; i < function_type->n_arguments; i++)
    {
      result_type->arguments[i].type = gsk_sl_type_ref (function_type->arguments[i].type);
      result_type->arguments[i].storage = function_type->arguments[i].storage;
    }

  result_type->arguments[i].type = gsk_sl_type_ref (argument_type);
  result_type->arguments[i].storage = argument_storage;

  gsk_sl_function_type_unref (function_type);

  return result_type;
}

GskSlFunctionType *
gsk_sl_function_type_ref (GskSlFunctionType *function_type)
{
  g_return_val_if_fail (function_type != NULL, NULL);

  function_type->ref_count += 1;

  return function_type;
}

void
gsk_sl_function_type_unref (GskSlFunctionType *function_type)
{
  gsize i;

  if (function_type == NULL)
    return;

  function_type->ref_count -= 1;
  if (function_type->ref_count > 0)
    return;

  for (i = 0; i < function_type->n_arguments; i++)
    {
      gsk_sl_type_unref (function_type->arguments[i].type);
    }
  gsk_sl_type_unref (function_type->return_type);

  g_slice_free1 (sizeof (GskSlFunctionType) + function_type->n_arguments * sizeof (GskSlArgument), function_type);
}

GskSlType *
gsk_sl_function_type_get_return_type (const GskSlFunctionType *function_type)
{
  return function_type->return_type;
}

gsize
gsk_sl_function_type_get_n_arguments (const GskSlFunctionType *function_type)
{
  return function_type->n_arguments;
}

GskSlType *
gsk_sl_function_type_get_argument_type (const GskSlFunctionType *function_type,
                                        gsize                    i)
{
  return function_type->arguments[i].type;
}

GskSlStorage
gsk_sl_function_type_get_argument_storage (const GskSlFunctionType *function_type,
                                           gsize                    i)
{
  return function_type->arguments[i].storage;
}

guint32
gsk_sl_function_type_write_spv (const GskSlFunctionType *function_type,
                                GskSpvWriter            *writer)
{
  guint32 argument_types[function_type->n_arguments], return_type_id;
  gsize i;

  return_type_id = gsk_spv_writer_get_id_for_type (writer, function_type->return_type);
  for (i = 0; i < function_type->n_arguments; i++)
    {
      argument_types[i] = gsk_spv_writer_get_id_for_type (writer, function_type->arguments[i].type);
    }

  return gsk_spv_writer_type_function (writer, return_type_id, argument_types, function_type->n_arguments);
}

gboolean
gsk_sl_function_type_equal (gconstpointer a,
                            gconstpointer b)
{
  const GskSlFunctionType *function_type_a = a;
  const GskSlFunctionType *function_type_b = b;
  gsize i;

  if (function_type_a->n_arguments != function_type_b->n_arguments)
    return FALSE;

  if (!gsk_sl_type_equal (function_type_a->return_type, function_type_b->return_type))
    return FALSE;

  for (i = 0; i < function_type_a->n_arguments; i++)
    {
      if (function_type_a->arguments[i].storage != function_type_b->arguments[i].storage ||
          !gsk_sl_type_equal (function_type_a->arguments[i].type, function_type_b->arguments[i].type))
        return FALSE;
    }

  return TRUE;
}

guint
gsk_sl_function_type_hash (gconstpointer value)
{
  const GskSlFunctionType *function_type = value;
  guint hash;
  gsize i;

  hash = gsk_sl_type_hash (function_type->return_type);

  for (i = 0; i < function_type->n_arguments; i++)
    {
      hash <<= 5;
      hash ^= gsk_sl_type_hash (function_type->arguments[i].type);
      hash ^= function_type->arguments[i].storage;
    }

  return hash;
}

