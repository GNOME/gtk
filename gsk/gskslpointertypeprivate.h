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

typedef enum {
  GSK_SL_POINTER_TYPE_LOCAL = (1 << 0),
  GSK_SL_POINTER_TYPE_CONST = (1 << 1),
  GSK_SL_POINTER_TYPE_IN = (1 << 2),
  GSK_SL_POINTER_TYPE_OUT = (1 << 3),
  GSK_SL_POINTER_TYPE_INVARIANT = (1 << 4),
  GSK_SL_POINTER_TYPE_COHERENT = (1 << 5),
  GSK_SL_POINTER_TYPE_VOLATILE = (1 << 6),
  GSK_SL_POINTER_TYPE_RESTRICT = (1 << 7),
  GSK_SL_POINTER_TYPE_READONLY = (1 << 8),
  GSK_SL_POINTER_TYPE_WRITEONLY = (1 << 9)
} GskSlPointerTypeFlags;

#define GSK_SL_POINTER_TYPE_PARAMETER_QUALIFIER (GSK_SL_POINTER_TYPE_CONST \
                                               | GSK_SL_POINTER_TYPE_IN \
                                               | GSK_SL_POINTER_TYPE_OUT)
#define GSK_SL_POINTER_TYPE_MEMORY_QUALIFIER (GSK_SL_POINTER_TYPE_COHERENT \
                                            | GSK_SL_POINTER_TYPE_VOLATILE \
                                            | GSK_SL_POINTER_TYPE_RESTRICT \
                                            | GSK_SL_POINTER_TYPE_READONLY \
                                            | GSK_SL_POINTER_TYPE_WRITEONLY)

gboolean                gsk_sl_type_qualifier_parse                     (GskSlPreprocessor          *stream,
                                                                         GskSlPointerTypeFlags       allowed_flags,
                                                                         GskSlPointerTypeFlags      *parsed_flags);

GskSlPointerType *      gsk_sl_pointer_type_new                         (GskSlType                  *type,
                                                                         GskSlPointerTypeFlags       flags);

GskSlPointerType *      gsk_sl_pointer_type_ref                         (GskSlPointerType           *type);
void                    gsk_sl_pointer_type_unref                       (GskSlPointerType           *type);

void                    gsk_sl_pointer_type_print                       (const GskSlPointerType     *type,
                                                                         GString                    *string);

GskSlType *             gsk_sl_pointer_type_get_type                    (const GskSlPointerType     *type);
gboolean                gsk_sl_pointer_type_is_const                    (const GskSlPointerType     *type);
gboolean                gsk_sl_pointer_type_is_in                       (const GskSlPointerType     *type);
gboolean                gsk_sl_pointer_type_is_out                      (const GskSlPointerType     *type);
gboolean                gsk_sl_pointer_type_is_invariant                (const GskSlPointerType     *type);
gboolean                gsk_sl_pointer_type_is_coherent                 (const GskSlPointerType     *type);
gboolean                gsk_sl_pointer_type_is_volatile                 (const GskSlPointerType     *type);
gboolean                gsk_sl_pointer_type_is_restrict                 (const GskSlPointerType     *type);
gboolean                gsk_sl_pointer_type_is_readonly                 (const GskSlPointerType     *type);
gboolean                gsk_sl_pointer_type_is_writeonly                (const GskSlPointerType     *type);

gboolean                gsk_sl_pointer_type_equal                       (gconstpointer               a,
                                                                         gconstpointer               b);
guint                   gsk_sl_pointer_type_hash                        (gconstpointer               type);

GskSpvStorageClass      gsk_sl_pointer_type_get_storage_class           (const GskSlPointerType     *type);
guint32                 gsk_sl_pointer_type_write_spv                   (const GskSlPointerType     *type,
                                                                         GskSpvWriter               *writer);

G_END_DECLS

#endif /* __GSK_SL_POINTER_TYPE_PRIVATE_H__ */
