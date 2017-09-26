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

#ifndef __GSK_SL_VALUE_PRIVATE_H__
#define __GSK_SL_VALUE_PRIVATE_H__

#include "gsk/gsksltypesprivate.h"

G_BEGIN_DECLS

GskSlValue *            gsk_sl_value_new                        (GskSlType           *type);
GskSlValue *            gsk_sl_value_new_for_data               (GskSlType           *type,
                                                                 gpointer             data,
                                                                 GDestroyNotify       free_func,
                                                                 gpointer             user_data);
GskSlValue *            gsk_sl_value_new_convert                (GskSlValue          *source,
                                                                 GskSlType           *new_type);
GskSlValue *            gsk_sl_value_copy                       (const GskSlValue    *source);
void                    gsk_sl_value_free                       (GskSlValue          *value);

void                    gsk_sl_value_print                      (const GskSlValue    *value,
                                                                 GString             *string);

GskSlType *             gsk_sl_value_get_type                   (const GskSlValue    *value);
gpointer                gsk_sl_value_get_data                   (const GskSlValue    *value);

gboolean                gsk_sl_value_equal                      (gconstpointer        a,
                                                                 gconstpointer        b);
guint                   gsk_sl_value_hash                       (gconstpointer        type);

guint32                 gsk_sl_value_write_spv                  (const GskSlValue    *value,
                                                                 GskSpvWriter        *writer);

G_END_DECLS

#endif /* __GSK_SL_VALUE_PRIVATE_H__ */
