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

#include "gskslcompilerprivate.h"
#include "gsksltypesprivate.h"
#include "gsksltokenizerprivate.h"

G_BEGIN_DECLS

GskSlPreprocessor *     gsk_sl_preprocessor_new                 (GskSlCompiler       *compiler,
                                                                 GBytes              *source);

GskSlPreprocessor *     gsk_sl_preprocessor_ref                 (GskSlPreprocessor   *preproc);
void                    gsk_sl_preprocessor_unref               (GskSlPreprocessor   *preproc);

gboolean                gsk_sl_preprocessor_has_fatal_error     (GskSlPreprocessor   *preproc);
const GskSlToken *      gsk_sl_preprocessor_get                 (GskSlPreprocessor   *preproc);
const GskCodeLocation * gsk_sl_preprocessor_get_location        (GskSlPreprocessor   *preproc);
void                    gsk_sl_preprocessor_consume             (GskSlPreprocessor   *preproc,
                                                                 gpointer             consumer);

void                    gsk_sl_preprocessor_sync                (GskSlPreprocessor   *preproc,
                                                                 GskSlTokenType       token);
void                    gsk_sl_preprocessor_emit_error          (GskSlPreprocessor   *preproc,
                                                                 gboolean             fatal,
                                                                 const GskCodeLocation *location,
                                                                 const GError        *error);

#define gsk_sl_preprocessor_error(preproc, type, ...) \
  gsk_sl_preprocessor_error_full (preproc, type, gsk_sl_preprocessor_get_location (preproc), __VA_ARGS__)

#define gsk_sl_preprocessor_error_full(preproc, type, location, ...) G_STMT_START{\
  GError *error; \
\
  error = g_error_new (GSK_SL_COMPILER_ERROR, \
                       GSK_SL_COMPILER_ERROR_ ## type, \
                       __VA_ARGS__); \
\
  gsk_sl_preprocessor_emit_error (preproc, \
                                  TRUE, \
                                  location, \
                                  error); \
\
  g_error_free (error);\
}G_STMT_END

#define gsk_sl_preprocessor_warn(preproc, type, ...) \
  gsk_sl_preprocessor_warn_full (preproc, type, gsk_sl_preprocessor_get_location (preproc), __VA_ARGS__)

#define gsk_sl_preprocessor_warn_full(preproc, type, location, ...) G_STMT_START{\
  GError *error; \
\
  error = g_error_new (GSK_SL_COMPILER_WARNING, \
                       GSK_SL_COMPILER_WARNING_ ## type, \
                       __VA_ARGS__); \
\
  gsk_sl_preprocessor_emit_error (preproc, \
                                  FALSE, \
                                  location, \
                                  error); \
\
  g_error_free (error);\
}G_STMT_END

G_END_DECLS

#endif /* __GSK_SL_PREPROCESSOR_PRIVATE_H__ */
