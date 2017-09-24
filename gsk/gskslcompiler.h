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

#ifndef __GSK_SL_COMPILER_H__
#define __GSK_SL_COMPILER_H__

#if !defined (__GSK_H_INSIDE__) && !defined (GSK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif

#include <gsk/gsktypes.h>

G_BEGIN_DECLS

#define GSK_TYPE_SL_COMPILER (gsk_sl_compiler_get_type ())

G_DECLARE_FINAL_TYPE (GskSlCompiler, gsk_sl_compiler, GSK, SL_COMPILER, GObject)

GDK_AVAILABLE_IN_3_92
GskSlCompiler *         gsk_sl_compiler_new                     (void);

GDK_AVAILABLE_IN_3_92
gboolean                gsk_sl_compiler_add_define              (GskSlCompiler       *compiler,
                                                                 const char          *name,
                                                                 const char          *definition,
                                                                 GError             **error);
GDK_AVAILABLE_IN_3_92
void                    gsk_sl_compiler_remove_define           (GskSlCompiler       *compiler,
                                                                 const char          *name);

GDK_AVAILABLE_IN_3_92
GskSlProgram *          gsk_sl_compiler_compile                 (GskSlCompiler       *compiler,
                                                                 GBytes              *source);

G_END_DECLS

#endif /* __GSK_SL_COMPILER_H__ */
