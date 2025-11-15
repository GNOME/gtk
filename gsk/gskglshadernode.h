/* GSK - The GTK Scene Kit
 *
 * Copyright 2016  Endless
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

#include <gsk/gsktypes.h>

#include <gsk/gskglshader.h>

G_BEGIN_DECLS

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

typedef struct _GskGLShaderNode                 GskGLShaderNode GDK_DEPRECATED_TYPE_IN_4_16_FOR(GtkGLArea);

#define GSK_TYPE_GL_SHADER_NODE (gsk_gl_shader_node_get_type())

GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
GType                   gsk_gl_shader_node_get_type             (void) G_GNUC_CONST;
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
GskRenderNode *         gsk_gl_shader_node_new                  (GskGLShader              *shader,
                                                                 const graphene_rect_t    *bounds,
                                                                 GBytes                   *args,
                                                                 GskRenderNode           **children,
                                                                 guint                     n_children);
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
guint                   gsk_gl_shader_node_get_n_children       (const GskRenderNode      *node) G_GNUC_PURE;
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
GskRenderNode *         gsk_gl_shader_node_get_child            (const GskRenderNode      *node,
                                                                 guint                     idx) G_GNUC_PURE;
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
GBytes *                gsk_gl_shader_node_get_args             (const GskRenderNode      *node) G_GNUC_PURE;
GDK_DEPRECATED_IN_4_16_FOR(GtkGLArea)
GskGLShader *           gsk_gl_shader_node_get_shader           (const GskRenderNode      *node) G_GNUC_PURE;

G_GNUC_END_IGNORE_DEPRECATIONS


G_END_DECLS

