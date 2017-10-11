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

#ifndef __GSK_SL_DECLARATION_PRIVATE_H__
#define __GSK_SL_DECLARATION_PRIVATE_H__

#include <glib.h>

#include "gsk/gsksltypesprivate.h"

G_BEGIN_DECLS

GskSlDeclaration *      gsk_sl_declaration_parse                (GskSlScope             *scope,
                                                                 GskSlPreprocessor      *preproc);

GskSlDeclaration *      gsk_sl_declaration_ref                  (GskSlDeclaration       *declaration);
void                    gsk_sl_declaration_unref                (GskSlDeclaration       *declaration);

GskSlFunction *         gsk_sl_declaration_get_function         (GskSlDeclaration       *declaration);

void                    gsk_sl_declaration_print                (const GskSlDeclaration *declaration,
                                                                 GskSlPrinter           *printer);

void                    gsk_sl_declaration_write_initializer_spv(const GskSlDeclaration *declaration,
                                                                 GskSpvWriter           *writer);

G_END_DECLS

#endif /* __GSK_SL_DECLARATION_PRIVATE_H__ */
