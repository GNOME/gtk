/* gsknglcompilerprivate.h
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GSK_NGL_COMPILER_PRIVATE_H__
#define __GSK_NGL_COMPILER_PRIVATE_H__

#include "gskngltypesprivate.h"

G_BEGIN_DECLS

typedef enum _GskNglCompilerKind
{
  GSK_NGL_COMPILER_ALL,
  GSK_NGL_COMPILER_FRAGMENT,
  GSK_NGL_COMPILER_VERTEX,
} GskNglCompilerKind;

#define GSK_TYPE_GL_COMPILER (gsk_ngl_compiler_get_type())

G_DECLARE_FINAL_TYPE (GskNglCompiler, gsk_ngl_compiler, GSK, NGL_COMPILER, GObject)

GskNglCompiler *gsk_ngl_compiler_new                        (GskNglDriver        *driver,
                                                             gboolean             debug);
void            gsk_ngl_compiler_set_preamble               (GskNglCompiler      *self,
                                                             GskNglCompilerKind   kind,
                                                             GBytes              *preamble_bytes);
void            gsk_ngl_compiler_set_preamble_from_resource (GskNglCompiler      *self,
                                                             GskNglCompilerKind   kind,
                                                             const char          *resource_path);
void            gsk_ngl_compiler_set_source                 (GskNglCompiler      *self,
                                                             GskNglCompilerKind   kind,
                                                             GBytes              *source_bytes);
void            gsk_ngl_compiler_set_source_from_resource   (GskNglCompiler      *self,
                                                             GskNglCompilerKind   kind,
                                                             const char          *resource_path);
void            gsk_ngl_compiler_set_suffix                 (GskNglCompiler      *self,
                                                             GskNglCompilerKind   kind,
                                                             GBytes              *suffix_bytes);
void            gsk_ngl_compiler_set_suffix_from_resource   (GskNglCompiler      *self,
                                                             GskNglCompilerKind   kind,
                                                             const char          *resource_path);
void            gsk_ngl_compiler_bind_attribute             (GskNglCompiler      *self,
                                                             const char          *name,
                                                             guint                location);
void            gsk_ngl_compiler_clear_attributes           (GskNglCompiler      *self);
GskNglProgram  *gsk_ngl_compiler_compile                    (GskNglCompiler      *self,
                                                             const char          *name,
                                                             const char          *clip,
                                                             GError             **error);

G_END_DECLS

#endif /* __GSK_NGL_COMPILER_PRIVATE_H__ */
