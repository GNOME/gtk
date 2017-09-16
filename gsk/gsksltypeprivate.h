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
  GSK_SL_BOOL,
  GSK_SL_VEC2,
  GSK_SL_VEC3,
  GSK_SL_VEC4,
  /* add more above */
  GSK_SL_N_BUILTIN_TYPES
} GskSlBuiltinType;

GskSlType *             gsk_sl_type_new_parse                   (GskSlPreprocessor   *stream);
GskSlType *             gsk_sl_type_get_builtin                 (GskSlBuiltinType     builtin);

GskSlType *             gsk_sl_type_ref                         (GskSlType           *type);
void                    gsk_sl_type_unref                       (GskSlType           *type);

void                    gsk_sl_type_print                       (const GskSlType     *type,
                                                                 GString             *string);

G_END_DECLS

#endif /* __GSK_SL_TYPE_PRIVATE_H__ */
