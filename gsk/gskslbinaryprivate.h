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

#ifndef __GSK_SL_BINARY_PRIVATE_H__
#define __GSK_SL_BINARY_PRIVATE_H__

#include "gsk/gsksltypesprivate.h"
#include "gsk/gsksltokenizerprivate.h"

G_BEGIN_DECLS

typedef struct _GskSlBinary GskSlBinary;

const GskSlBinary * gsk_sl_binary_get_for_token                 (GskSlTokenType          token);

const char *        gsk_sl_binary_get_sign                      (const GskSlBinary      *binary);

GskSlType *         gsk_sl_binary_check_type                    (const GskSlBinary      *binary,
                                                                 GskSlPreprocessor      *stream,
                                                                 GskSlType              *ltype,
                                                                 GskSlType              *rtype);
GskSlValue *        gsk_sl_binary_get_constant                  (const GskSlBinary      *binary,
                                                                 GskSlType              *type,
                                                                 GskSlValue             *lvalue,
                                                                 GskSlValue             *rvalue);
guint32             gsk_sl_binary_write_spv                     (const GskSlBinary      *binary,
                                                                 GskSpvWriter           *writer,
                                                                 GskSlType              *type,
                                                                 GskSlType              *ltype,
                                                                 guint32                 left_id,
                                                                 GskSlType              *rtype,
                                                                 guint32                 right_id);


G_END_DECLS

#endif /* __GSK_SL_BINARY_PRIVATE_H__ */
