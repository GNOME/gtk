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

typedef enum {
  GSK_SL_VOID,
  GSK_SL_FLOAT,
  GSK_SL_DOUBLE,
  GSK_SL_INT,
  GSK_SL_UINT,
  GSK_SL_BOOL
} GskSlScalarType;

GskSlType *             gsk_sl_type_new_parse                   (GskSlPreprocessor   *stream);
GskSlType *             gsk_sl_type_get_scalar                  (GskSlScalarType      scalar);
GskSlType *             gsk_sl_type_get_vector                  (GskSlScalarType      scalar,
                                                                 guint                length);
GskSlType *             gsk_sl_type_get_matrix                  (GskSlScalarType      scalar,
                                                                 guint                columns,
                                                                 guint                rows);

GskSlType *             gsk_sl_type_ref                         (GskSlType           *type);
void                    gsk_sl_type_unref                       (GskSlType           *type);

void                    gsk_sl_type_print                       (const GskSlType     *type,
                                                                 GString             *string);
char *                  gsk_sl_type_to_string                   (const GskSlType     *type);

gboolean                gsk_sl_type_is_scalar                   (const GskSlType     *type);
gboolean                gsk_sl_type_is_vector                   (const GskSlType     *type);
gboolean                gsk_sl_type_is_matrix                   (const GskSlType     *type);
GskSlScalarType         gsk_sl_type_get_scalar_type             (const GskSlType     *type);
guint                   gsk_sl_type_get_length                  (const GskSlType     *type);
gboolean                gsk_sl_type_can_convert                 (const GskSlType     *target,
                                                                 const GskSlType     *source);

G_END_DECLS

#endif /* __GSK_SL_TYPE_PRIVATE_H__ */
