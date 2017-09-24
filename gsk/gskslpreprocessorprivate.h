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

#ifndef __GSK_SL_PREPROCESSOR_PRIVATE_H__
#define __GSK_SL_PREPROCESSOR_PRIVATE_H__

#include <glib.h>

#include "gsksltypesprivate.h"
#include "gsksltokenizerprivate.h"

G_BEGIN_DECLS

GskSlPreprocessor *     gsk_sl_preprocessor_new                 (GskSlCompiler       *compiler,
                                                                 GBytes              *source);

GskSlPreprocessor *     gsk_sl_preprocessor_ref                 (GskSlPreprocessor   *preproc);
void                    gsk_sl_preprocessor_unref               (GskSlPreprocessor   *preproc);

const GskSlToken *      gsk_sl_preprocessor_get                 (GskSlPreprocessor   *preproc);
const GskCodeLocation * gsk_sl_preprocessor_get_location        (GskSlPreprocessor   *preproc);
void                    gsk_sl_preprocessor_consume             (GskSlPreprocessor   *preproc,
                                                                 GskSlNode           *consumer);

void                    gsk_sl_preprocessor_error               (GskSlPreprocessor   *preproc,
                                                                 const char          *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);
void                    gsk_sl_preprocessor_error_full          (GskSlPreprocessor   *preproc,
                                                                 const GskCodeLocation *location,
                                                                 const GskSlToken    *token,
                                                                 const char          *format,
                                                                 ...) G_GNUC_PRINTF(4, 5);

G_END_DECLS

#endif /* __GSK_SL_PREPROCESSOR_PRIVATE_H__ */
