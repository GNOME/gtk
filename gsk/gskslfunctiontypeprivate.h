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

#ifndef __GSK_SL_FUNCTION_TYPE_PRIVATE_H__
#define __GSK_SL_FUNCTION_TYPE_PRIVATE_H__

#include <glib.h>

#include "gsksltypesprivate.h"

G_BEGIN_DECLS

GskSlFunctionType *     gsk_sl_function_type_new                (GskSlType              *return_type);
GskSlFunctionType *     gsk_sl_function_type_add_argument       (GskSlFunctionType      *function_type,
                                                                 GskSlStorage            argument_storage,
                                                                 GskSlType              *argument_type);

GskSlFunctionType *     gsk_sl_function_type_ref                (GskSlFunctionType      *function_type);
void                    gsk_sl_function_type_unref              (GskSlFunctionType      *function_type);

GskSlType *             gsk_sl_function_type_get_return_type    (const GskSlFunctionType*function_type);
gsize                   gsk_sl_function_type_get_n_arguments    (const GskSlFunctionType*function_type);
GskSlType *             gsk_sl_function_type_get_argument_type  (const GskSlFunctionType*function_type,
                                                                 gsize                   i);
GskSlStorage            gsk_sl_function_type_get_argument_storage (const GskSlFunctionType*function_type,
                                                                 gsize                   i);

guint32                 gsk_sl_function_type_write_spv          (const GskSlFunctionType*function_type,
                                                                 GskSpvWriter           *writer);

gboolean                gsk_sl_function_type_equal              (gconstpointer           a,
                                                                 gconstpointer           b);
guint                   gsk_sl_function_type_hash               (gconstpointer           value);

G_END_DECLS

#endif /* __GSK_SL_FUNCTION_TYPE_PRIVATE_H__ */
