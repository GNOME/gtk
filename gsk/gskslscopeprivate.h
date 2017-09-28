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

#ifndef __GSK_SL_SCOPE_PRIVATE_H__
#define __GSK_SL_SCOPE_PRIVATE_H__

#include <glib.h>

#include "gsksltypesprivate.h"

G_BEGIN_DECLS

GskSlScope *            gsk_sl_scope_new                        (GskSlScope           *parent,
                                                                 GskSlType            *return_type);

GskSlScope *            gsk_sl_scope_ref                        (GskSlScope           *scope);
void                    gsk_sl_scope_unref                      (GskSlScope           *scope);

GskSlType *             gsk_sl_scope_get_return_type            (const GskSlScope     *scope);
gboolean                gsk_sl_scope_is_global                  (const GskSlScope     *scope);

void                    gsk_sl_scope_add_variable               (GskSlScope           *scope,
                                                                 GskSlVariable        *variable);
GskSlVariable *         gsk_sl_scope_lookup_variable            (GskSlScope           *scope,
                                                                 const char           *name);
void                    gsk_sl_scope_add_function               (GskSlScope           *scope,
                                                                 GskSlFunction        *function);
GskSlFunction *         gsk_sl_scope_lookup_function            (GskSlScope           *scope,
                                                                 const char           *name);
void                    gsk_sl_scope_add_type                   (GskSlScope           *scope,
                                                                 GskSlType            *type);
GskSlType *             gsk_sl_scope_lookup_type                (GskSlScope           *scope,
                                                                 const char           *name);

G_END_DECLS

#endif /* __GSK_SL_SCOPE_PRIVATE_H__ */
