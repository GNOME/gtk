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

#ifndef __GSK_SL_TYPE_PRIVATE_H__
#define __GSK_SL_TYPE_PRIVATE_H__

#include <glib.h>

#include "gsksltypesprivate.h"

G_BEGIN_DECLS

typedef struct _GskSlTypeBuilder GskSlTypeBuilder;

gsize                   gsk_sl_scalar_type_get_size             (GskSlScalarType      type);

GskSlType *             gsk_sl_type_new_parse                   (GskSlScope          *scope,
                                                                 GskSlPreprocessor   *preproc);
GskSlType *             gsk_sl_type_get_void                    (void);
GskSlType *             gsk_sl_type_get_scalar                  (GskSlScalarType      scalar);
GskSlType *             gsk_sl_type_get_vector                  (GskSlScalarType      scalar,
                                                                 guint                length);
GskSlType *             gsk_sl_type_get_matrix                  (GskSlScalarType      scalar,
                                                                 guint                columns,
                                                                 guint                rows);
GskSlType *             gsk_sl_type_get_builtin                 (GskSlBuiltinType     builtin);

GskSlType *             gsk_sl_type_ref                         (GskSlType           *type);
void                    gsk_sl_type_unref                       (GskSlType           *type);

gboolean                gsk_sl_type_is_void                     (const GskSlType     *type);
gboolean                gsk_sl_type_is_scalar                   (const GskSlType     *type);
gboolean                gsk_sl_type_is_vector                   (const GskSlType     *type);
gboolean                gsk_sl_type_is_matrix                   (const GskSlType     *type);
gboolean                gsk_sl_type_is_struct                   (const GskSlType     *type);
gboolean                gsk_sl_type_is_block                    (const GskSlType     *type);

const char *            gsk_sl_type_get_name                    (const GskSlType     *type);
GskSlScalarType         gsk_sl_type_get_scalar_type             (const GskSlType     *type);
GskSlType *             gsk_sl_type_get_index_type              (const GskSlType     *type);
gsize                   gsk_sl_type_get_index_stride            (const GskSlType     *type);
guint                   gsk_sl_type_get_length                  (const GskSlType     *type);
gsize                   gsk_sl_type_get_size                    (const GskSlType     *type);
gsize                   gsk_sl_type_get_n_components            (const GskSlType     *type);
guint                   gsk_sl_type_get_n_members               (const GskSlType     *type);
GskSlType *             gsk_sl_type_get_member_type             (const GskSlType     *type,
                                                                 guint                n);
const char *            gsk_sl_type_get_member_name             (const GskSlType     *type,
                                                                 guint                n);
gsize                   gsk_sl_type_get_member_offset           (const GskSlType     *type,
                                                                 guint                n);
gboolean                gsk_sl_type_find_member                 (const GskSlType     *type,
                                                                 const char          *name,
                                                                 guint               *out_index,
                                                                 GskSlType          **out_type,
                                                                 gsize               *out_offset);
gboolean                gsk_sl_scalar_type_can_convert          (GskSlScalarType      target,
                                                                 GskSlScalarType      source);
gboolean                gsk_sl_type_can_convert                 (const GskSlType     *target,
                                                                 const GskSlType     *source);

gboolean                gsk_sl_type_equal                       (gconstpointer        a,
                                                                 gconstpointer        b);
guint                   gsk_sl_type_hash                        (gconstpointer        type);

guint32                 gsk_sl_type_write_spv                   (GskSlType           *type,
                                                                 GskSpvWriter        *writer);

void                    gsk_sl_type_print_value                 (const GskSlType     *type,
                                                                 GskSlPrinter        *printer,
                                                                 gconstpointer        value);
guint32                 gsk_sl_type_write_value_spv             (GskSlType           *type,
                                                                 GskSpvWriter        *writer,
                                                                 gconstpointer        value);
void                    gsk_sl_scalar_type_convert_value        (GskSlScalarType      target_type,
                                                                 gpointer             target_value,
                                                                 GskSlScalarType      source_type,
                                                                 gconstpointer        source_value);

GskSlTypeBuilder *      gsk_sl_type_builder_new_struct          (const char          *name);
GskSlTypeBuilder *      gsk_sl_type_builder_new_block           (const char          *name);
GskSlType *             gsk_sl_type_builder_free                (GskSlTypeBuilder    *builder);
void                    gsk_sl_type_builder_add_member          (GskSlTypeBuilder    *builder,
                                                                 GskSlType           *type,
                                                                 const char          *name);
gboolean                gsk_sl_type_builder_has_member          (GskSlTypeBuilder    *builder,
                                                                 const char          *name);

G_END_DECLS

#endif /* __GSK_SL_TYPE_PRIVATE_H__ */
