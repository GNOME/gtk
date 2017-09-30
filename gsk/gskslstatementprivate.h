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

#ifndef __GSK_SL_STATEMENT_PRIVATE_H__
#define __GSK_SL_STATEMENT_PRIVATE_H__

#include <glib.h>

#include "gsk/gsksltypesprivate.h"

G_BEGIN_DECLS

GskSlStatement *        gsk_sl_statement_parse                  (GskSlScope             *scope,
                                                                 GskSlPreprocessor      *preproc);
GskSlStatement *        gsk_sl_statement_parse_compound         (GskSlScope             *scope,
                                                                 GskSlPreprocessor      *preproc,
                                                                 gboolean                new_scope);

GskSlStatement *        gsk_sl_statement_ref                    (GskSlStatement         *statement);
void                    gsk_sl_statement_unref                  (GskSlStatement         *statement);

void                    gsk_sl_statement_print                  (const GskSlStatement   *statement,
                                                                 GskSlPrinter           *printer);

guint32                 gsk_sl_statement_write_spv              (const GskSlStatement   *statement,
                                                                 GskSpvWriter           *writer);

G_END_DECLS

#endif /* __GSK_SL_STATEMENT_PRIVATE_H__ */
