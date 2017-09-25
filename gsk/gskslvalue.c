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

#include "gskslvalueprivate.h"

#include "gsksltypeprivate.h"
#include "gskspvwriterprivate.h"

struct _GskSlValue {
  GskSlType *type;

  gpointer data;
  GDestroyNotify free_func;
  gpointer user_data;
};

GskSlValue *
gsk_sl_value_new (GskSlType *type)
{
  gpointer data;
  
  g_return_val_if_fail (gsk_sl_type_get_size (type) > 0, NULL);

  data = g_malloc0 (gsk_sl_type_get_size (type));

  return gsk_sl_value_new_for_data (type, data, g_free, data);
}

GskSlValue *
gsk_sl_value_new_for_data (GskSlType      *type,
                           gpointer        data,
                           GDestroyNotify  free_func,
                           gpointer        user_data)
{
  GskSlValue *value;

  g_return_val_if_fail (gsk_sl_type_get_size (type) > 0, NULL);
  g_return_val_if_fail (data != NULL, NULL);

  value = g_slice_new0 (GskSlValue);

  value->type = gsk_sl_type_ref (type);
  value->data = data;
  value->free_func = free_func;
  value->user_data = user_data;

  return value;
}

GskSlValue *
gsk_sl_value_copy (const GskSlValue *source)
{
  gpointer data;

  data = g_memdup (source->data, gsk_sl_type_get_size (source->type));

  return gsk_sl_value_new_for_data (source->type, data, g_free, data);
}

void
gsk_sl_value_free (GskSlValue *value)
{
  if (value == NULL)
    return;

  if (value->free_func)
    value->free_func (value->user_data);

  gsk_sl_type_unref (value->type);

  g_slice_free (GskSlValue, value);
}

void
gsk_sl_value_print (const GskSlValue *value,
                    GString          *string)
{
  gsk_sl_type_print_value (value->type, string, value->data);
}

GskSlType *
gsk_sl_value_get_type (const GskSlValue *value)
{
  return value->type;
}

gpointer
gsk_sl_value_get_data (const GskSlValue *value)
{
  return value->data;
}

gboolean
gsk_sl_value_equal (gconstpointer a_,
                    gconstpointer b_)
{
  const GskSlValue *a = a_;
  const GskSlValue *b = b_;

  if (!gsk_sl_type_equal (a->type, b->type))
    return FALSE;

  /* XXX: This is wrong */
  return memcmp (a->data, b->data, gsk_sl_type_get_size (a->type)) == 0;
}

guint
gsk_sl_value_hash (gconstpointer value_)
{
  const GskSlValue *value = value_;

  /* XXX: We definitely want to hash the data here ! */
  return gsk_sl_type_hash (value->type);
}

guint32
gsk_sl_value_write_spv (const GskSlValue *value,
                        GskSpvWriter     *writer)
{
  return gsk_sl_type_write_value_spv (value->type, writer, value->data);
}
