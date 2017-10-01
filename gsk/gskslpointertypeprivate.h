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

#ifndef __GSK_SL_POINTER_TYPE_PRIVATE_H__
#define __GSK_SL_POINTER_TYPE_PRIVATE_H__

#include <glib.h>

#include "gsksltypesprivate.h"
#include "gskspvwriterprivate.h"

G_BEGIN_DECLS

GskSlPointerType *      gsk_sl_pointer_type_new                         (GskSlType                  *type,
                                                                         const GskSlQualifier       *qualifier);

GskSlPointerType *      gsk_sl_pointer_type_ref                         (GskSlPointerType           *type);
void                    gsk_sl_pointer_type_unref                       (GskSlPointerType           *type);

GskSlType *             gsk_sl_pointer_type_get_type                    (const GskSlPointerType     *type);
const GskSlQualifier *  gsk_sl_pointer_type_get_qualifier               (const GskSlPointerType     *type);

gboolean                gsk_sl_pointer_type_equal                       (gconstpointer               a,
                                                                         gconstpointer               b);
guint                   gsk_sl_pointer_type_hash                        (gconstpointer               type);

guint32                 gsk_sl_pointer_type_write_spv                   (const GskSlPointerType     *type,
                                                                         GskSpvWriter               *writer);

G_END_DECLS

#endif /* __GSK_SL_POINTER_TYPE_PRIVATE_H__ */
