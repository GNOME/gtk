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

#pragma once

#if !defined (__GSK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif

#include <stdarg.h>

#include <gsk/gsktypes.h>
#include <gsk/gskenums.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

G_BEGIN_DECLS

#define GSK_TYPE_SHADER_ARGS_BUILDER    (gsk_shader_args_builder_get_type ())

/**
 * GskShaderArgsBuilder:
 *
 * An object to build the uniforms data for a `GskGLShader`.
 */
typedef struct _GskGLShader GskGLShader GDK_DEPRECATED_TYPE_IN_4_16_FOR(GtkGLArea);
typedef struct _GskShaderArgsBuilder GskShaderArgsBuilder GDK_DEPRECATED_TYPE_IN_4_16_FOR(GtkGLArea);

#define GSK_TYPE_GL_SHADER (gsk_gl_shader_get_type ())

GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
G_DECLARE_FINAL_TYPE (GskGLShader, gsk_gl_shader, GSK, GL_SHADER, GObject)

GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
GskGLShader *    gsk_gl_shader_new_from_bytes          (GBytes           *sourcecode);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
GskGLShader *    gsk_gl_shader_new_from_resource       (const char       *resource_path);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
gboolean         gsk_gl_shader_compile                 (GskGLShader      *shader,
                                                        GskRenderer      *renderer,
                                                        GError          **error);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
GBytes *         gsk_gl_shader_get_source              (GskGLShader      *shader);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
const char *     gsk_gl_shader_get_resource            (GskGLShader      *shader);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
int              gsk_gl_shader_get_n_textures          (GskGLShader      *shader);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
int              gsk_gl_shader_get_n_uniforms          (GskGLShader      *shader);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
const char *     gsk_gl_shader_get_uniform_name        (GskGLShader      *shader,
                                                        int               idx);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
int              gsk_gl_shader_find_uniform_by_name    (GskGLShader      *shader,
                                                        const char       *name);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
GskGLUniformType gsk_gl_shader_get_uniform_type        (GskGLShader      *shader,
                                                        int               idx);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
int              gsk_gl_shader_get_uniform_offset      (GskGLShader      *shader,
                                                        int               idx);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
gsize            gsk_gl_shader_get_args_size           (GskGLShader      *shader);


/* Helpers for managing shader args */

GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
GBytes * gsk_gl_shader_format_args_va (GskGLShader     *shader,
                                       va_list          uniforms);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
GBytes * gsk_gl_shader_format_args    (GskGLShader     *shader,
                                       ...) G_GNUC_NULL_TERMINATED;

GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
float    gsk_gl_shader_get_arg_float (GskGLShader     *shader,
                                      GBytes          *args,
                                      int              idx);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
gint32   gsk_gl_shader_get_arg_int   (GskGLShader     *shader,
                                      GBytes          *args,
                                      int              idx);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
guint32  gsk_gl_shader_get_arg_uint  (GskGLShader     *shader,
                                      GBytes          *args,
                                      int              idx);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
gboolean gsk_gl_shader_get_arg_bool  (GskGLShader     *shader,
                                      GBytes          *args,
                                      int              idx);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
void     gsk_gl_shader_get_arg_vec2  (GskGLShader     *shader,
                                      GBytes          *args,
                                      int              idx,
                                      graphene_vec2_t *out_value);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
void     gsk_gl_shader_get_arg_vec3  (GskGLShader     *shader,
                                      GBytes          *args,
                                      int              idx,
                                      graphene_vec3_t *out_value);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
void     gsk_gl_shader_get_arg_vec4  (GskGLShader     *shader,
                                      GBytes          *args,
                                      int              idx,
                                      graphene_vec4_t *out_value);

GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
GType   gsk_shader_args_builder_get_type  (void) G_GNUC_CONST;

GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
GskShaderArgsBuilder *gsk_shader_args_builder_new           (GskGLShader *shader,
                                                             GBytes      *initial_values);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
GBytes *               gsk_shader_args_builder_to_args      (GskShaderArgsBuilder *builder);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
GBytes *               gsk_shader_args_builder_free_to_args (GskShaderArgsBuilder *builder);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
GskShaderArgsBuilder  *gsk_shader_args_builder_ref          (GskShaderArgsBuilder *builder);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
void                   gsk_shader_args_builder_unref        (GskShaderArgsBuilder *builder);

GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
void    gsk_shader_args_builder_set_float (GskShaderArgsBuilder *builder,
                                           int                    idx,
                                           float                  value);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
void    gsk_shader_args_builder_set_int   (GskShaderArgsBuilder *builder,
                                           int                    idx,
                                           gint32                 value);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
void    gsk_shader_args_builder_set_uint  (GskShaderArgsBuilder *builder,
                                           int                    idx,
                                           guint32                value);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
void    gsk_shader_args_builder_set_bool  (GskShaderArgsBuilder *builder,
                                           int                    idx,
                                           gboolean               value);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
void    gsk_shader_args_builder_set_vec2  (GskShaderArgsBuilder *builder,
                                           int                    idx,
                                           const graphene_vec2_t *value);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
void    gsk_shader_args_builder_set_vec3  (GskShaderArgsBuilder *builder,
                                           int                    idx,
                                           const graphene_vec3_t *value);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
void    gsk_shader_args_builder_set_vec4  (GskShaderArgsBuilder *builder,
                                           int                    idx,
                                           const graphene_vec4_t *value);


G_END_DECLS

G_GNUC_END_IGNORE_DEPRECATIONS
