/*
 * Copyright © 2024 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gdk/gdk.h>
#include <gsk/gsk.h>

G_BEGIN_DECLS

#define GSK_TYPE_GL_RENDERER (gsk_gl_renderer_get_type())

#define GSK_GL_RENDERER(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSK_TYPE_GL_RENDERER, GskGLRenderer))
#define GSK_IS_GL_RENDERER(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSK_TYPE_GL_RENDERER))
#define GSK_GL_RENDERER_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_GL_RENDERER, GskGLRendererClass))
#define GSK_IS_GL_RENDERER_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_GL_RENDERER))
#define GSK_GL_RENDERER_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_GL_RENDERER, GskGLRendererClass))

typedef struct _GskGLRenderer      GskGLRenderer;
typedef struct _GskGLRendererClass GskGLRendererClass;

GDK_AVAILABLE_IN_ALL
GskRenderer *gsk_gl_renderer_new      (void);

GDK_AVAILABLE_IN_ALL
GType        gsk_gl_renderer_get_type (void) G_GNUC_CONST;

GDK_DEPRECATED_IN_4_18_FOR (gsk_gl_renderer_get_type)
GType        gsk_ngl_renderer_get_type (void) G_GNUC_CONST;

GDK_DEPRECATED_IN_4_18_FOR (gsk_gl_renderer_new)
GskRenderer *gsk_ngl_renderer_new      (void);

G_END_DECLS
