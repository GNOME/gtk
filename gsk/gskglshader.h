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

#ifndef __GSK_GL_SHADER_H__
#define __GSK_GL_SHADER_H__

#if !defined (__GSK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif

#include <stdarg.h>

#include <gsk/gsktypes.h>
#include <gsk/gskenums.h>

G_BEGIN_DECLS

#define GSK_TYPE_GLSHADER (gsk_gl_shader_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GskGLShader, gsk_gl_shader, GSK, GL_SHADER, GObject)

GDK_AVAILABLE_IN_ALL
GskGLShader *    gsk_gl_shader_new_from_bytes          (GBytes           *sourcecode);
GDK_AVAILABLE_IN_ALL
GskGLShader *    gsk_gl_shader_new_from_resource       (const char       *resource_path);
GDK_AVAILABLE_IN_ALL
gboolean         gsk_gl_shader_try_compile_for         (GskGLShader      *shader,
                                                        GskRenderer      *renderer,
                                                        GError          **error);
GDK_AVAILABLE_IN_ALL
GBytes *         gsk_gl_shader_get_bytes               (GskGLShader      *shader);
GDK_AVAILABLE_IN_ALL
int              gsk_gl_shader_get_n_required_textures (GskGLShader      *shader);
GDK_AVAILABLE_IN_ALL
int              gsk_gl_shader_get_n_uniforms          (GskGLShader      *shader);
GDK_AVAILABLE_IN_ALL
const char *     gsk_gl_shader_get_uniform_name        (GskGLShader      *shader,
                                                       int               idx);
GDK_AVAILABLE_IN_ALL
int              gsk_gl_shader_find_uniform_by_name    (GskGLShader      *shader,
                                                       const char       *name);
GDK_AVAILABLE_IN_ALL
GskGLUniformType gsk_gl_shader_get_uniform_type        (GskGLShader      *shader,
                                                       int               idx);
GDK_AVAILABLE_IN_ALL
int              gsk_gl_shader_get_uniform_offset      (GskGLShader      *shader,
                                                       int               idx);
GDK_AVAILABLE_IN_ALL
int              gsk_gl_shader_get_uniforms_size       (GskGLShader      *shader);


/* Helpers for managing uniform data */

GDK_AVAILABLE_IN_ALL
float    gsk_gl_shader_get_uniform_data_float (GskGLShader     *shader,
                                               GBytes          *uniform_data,
                                               int              idx);
GDK_AVAILABLE_IN_ALL
gint32   gsk_gl_shader_get_uniform_data_int   (GskGLShader     *shader,
                                               GBytes          *uniform_data,
                                               int              idx);
GDK_AVAILABLE_IN_ALL
guint32  gsk_gl_shader_get_uniform_data_uint  (GskGLShader     *shader,
                                               GBytes          *uniform_data,
                                               int              idx);
GDK_AVAILABLE_IN_ALL
gboolean gsk_gl_shader_get_uniform_data_bool  (GskGLShader     *shader,
                                               GBytes          *uniform_data,
                                               int              idx);
GDK_AVAILABLE_IN_ALL
void     gsk_gl_shader_get_uniform_data_vec2  (GskGLShader     *shader,
                                               GBytes          *uniform_data,
                                               int              idx,
                                               graphene_vec2_t *out_value);
GDK_AVAILABLE_IN_ALL
void     gsk_gl_shader_get_uniform_data_vec3  (GskGLShader     *shader,
                                               GBytes          *uniform_data,
                                               int              idx,
                                               graphene_vec3_t *out_value);
GDK_AVAILABLE_IN_ALL
void     gsk_gl_shader_get_uniform_data_vec4  (GskGLShader     *shader,
                                               GBytes          *uniform_data,
                                               int              idx,
                                               graphene_vec4_t *out_value);
GDK_AVAILABLE_IN_ALL
GBytes * gsk_gl_shader_format_uniform_data_va (GskGLShader     *shader,
                                               va_list          uniforms);

typedef struct _GskUniformDataBuilder GskUniformDataBuilder;

GDK_AVAILABLE_IN_ALL
GskUniformDataBuilder *gsk_gl_shader_build_uniform_data (GskGLShader *shader);


#define GSK_TYPE_UNIFORM_DATA_BUILDER    (gsk_uniform_data_builder_get_type ())

GDK_AVAILABLE_IN_ALL
GType   gsk_uniform_data_builder_get_type  (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GBytes *               gsk_uniform_data_builder_finish (GskUniformDataBuilder *builder);
GDK_AVAILABLE_IN_ALL
void                   gsk_uniform_data_builder_free   (GskUniformDataBuilder *builder);
GDK_AVAILABLE_IN_ALL
GskUniformDataBuilder *gsk_uniform_data_builder_copy   (GskUniformDataBuilder *builder);

GDK_AVAILABLE_IN_ALL
void    gsk_uniform_data_builder_set_float (GskUniformDataBuilder *builder,
                                            int                    idx,
                                            float                  value);
GDK_AVAILABLE_IN_ALL
void    gsk_uniform_data_builder_set_int   (GskUniformDataBuilder *builder,
                                            int                    idx,
                                            gint32                 value);
GDK_AVAILABLE_IN_ALL
void    gsk_uniform_data_builder_set_uint  (GskUniformDataBuilder *builder,
                                            int                    idx,
                                            guint32                value);
GDK_AVAILABLE_IN_ALL
void    gsk_uniform_data_builder_set_bool  (GskUniformDataBuilder *builder,
                                            int                    idx,
                                            gboolean               value);
GDK_AVAILABLE_IN_ALL
void    gsk_uniform_data_builder_set_vec2  (GskUniformDataBuilder *builder,
                                            int                    idx,
                                            const graphene_vec2_t *value);
GDK_AVAILABLE_IN_ALL
void    gsk_uniform_data_builder_set_vec3  (GskUniformDataBuilder *builder,
                                            int                    idx,
                                            const graphene_vec3_t *value);
GDK_AVAILABLE_IN_ALL
void    gsk_uniform_data_builder_set_vec4  (GskUniformDataBuilder *builder,
                                            int                    idx,
                                            const graphene_vec4_t *value);




G_END_DECLS

#endif /* __GSK_GL_SHADER_H__ */
