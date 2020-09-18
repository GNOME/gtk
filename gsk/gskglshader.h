/* GSK - The GTK Scene Kit
 *
 * Copyright 2020  Red Hat Inc
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

#ifndef __GSK_GLSHADER_H__
#define __GSK_GLSHADER_H__

#if !defined (__GSK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif

#include <stdarg.h>

#include <gsk/gsktypes.h>
#include <gsk/gskenums.h>

G_BEGIN_DECLS

#define GSK_TYPE_GLSHADER (gsk_glshader_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GskGLShader, gsk_glshader, GSK, GLSHADER, GObject)

GDK_AVAILABLE_IN_ALL
GskGLShader *    gsk_glshader_new                     (const char       *sourcecode);
GDK_AVAILABLE_IN_ALL
const char *     gsk_glshader_get_sourcecode          (GskGLShader      *shader);
GDK_AVAILABLE_IN_ALL
int              gsk_glshader_get_n_required_sources  (GskGLShader      *shader);
GDK_AVAILABLE_IN_ALL
void             gsk_glshader_set_n_required_sources  (GskGLShader      *shader,
                                                       int               n_required_sources);
GDK_AVAILABLE_IN_ALL
void             gsk_glshader_add_uniform             (GskGLShader      *shader,
                                                       const char       *name,
                                                       GskGLUniformType  type);
GDK_AVAILABLE_IN_ALL
int              gsk_glshader_get_n_uniforms          (GskGLShader      *shader);
GDK_AVAILABLE_IN_ALL
const char *     gsk_glshader_get_uniform_name        (GskGLShader      *shader,
                                                       int               idx);
GDK_AVAILABLE_IN_ALL
GskGLUniformType gsk_glshader_get_uniform_type        (GskGLShader      *shader,
                                                       int               idx);
GDK_AVAILABLE_IN_ALL
int              gsk_glshader_get_uniform_offset      (GskGLShader      *shader,
                                                       int               idx);
GDK_AVAILABLE_IN_ALL
int              gsk_glshader_get_uniforms_size       (GskGLShader      *shader);
GDK_AVAILABLE_IN_ALL
guchar *         gsk_glshader_format_uniform_data_va  (GskGLShader  *shader,
                                                       va_list       uniforms);

G_END_DECLS

#endif /* __GSK_GLSHADER_H__ */
