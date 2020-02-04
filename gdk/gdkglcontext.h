/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext.h: GL context abstraction
 * 
 * Copyright © 2014  Emmanuele Bassi
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_GL_CONTEXT_H__
#define __GDK_GL_CONTEXT_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#define GDK_GL_ERROR       (gdk_gl_error_quark ())

#define GDK_TYPE_GL_CONTEXT (gdk_gl_context_get_type ())
GDK_AVAILABLE_IN_ALL
GDK_DECLARE_EXPORTED_TYPE (GdkGLContext, gdk_gl_context, GDK, GL_CONTEXT)

GDK_AVAILABLE_IN_ALL
GQuark gdk_gl_error_quark (void);

GDK_AVAILABLE_IN_ALL
GdkDisplay *            gdk_gl_context_get_display              (GdkGLContext  *context);
GDK_AVAILABLE_IN_ALL
GdkSurface *             gdk_gl_context_get_surface               (GdkGLContext  *context);
GDK_AVAILABLE_IN_ALL
GdkGLContext *          gdk_gl_context_get_shared_context       (GdkGLContext  *context);
GDK_AVAILABLE_IN_ALL
void                    gdk_gl_context_get_version              (GdkGLContext  *context,
                                                                 int           *major,
                                                                 int           *minor);
GDK_AVAILABLE_IN_ALL
gboolean                gdk_gl_context_is_legacy                (GdkGLContext  *context);

GDK_AVAILABLE_IN_ALL
void                    gdk_gl_context_set_required_version     (GdkGLContext  *context,
                                                                 int            major,
                                                                 int            minor);
GDK_AVAILABLE_IN_ALL
void                    gdk_gl_context_get_required_version     (GdkGLContext  *context,
                                                                 int           *major,
                                                                 int           *minor);
GDK_AVAILABLE_IN_ALL
void                    gdk_gl_context_set_debug_enabled        (GdkGLContext  *context,
                                                                 gboolean       enabled);
GDK_AVAILABLE_IN_ALL
gboolean                gdk_gl_context_get_debug_enabled        (GdkGLContext  *context);
GDK_AVAILABLE_IN_ALL
void                    gdk_gl_context_set_forward_compatible   (GdkGLContext  *context,
                                                                 gboolean       compatible);
GDK_AVAILABLE_IN_ALL
gboolean                gdk_gl_context_get_forward_compatible   (GdkGLContext  *context);
GDK_AVAILABLE_IN_ALL
void                    gdk_gl_context_set_use_es               (GdkGLContext  *context,
                                                                 int            use_es);
GDK_AVAILABLE_IN_ALL
gboolean                gdk_gl_context_get_use_es               (GdkGLContext  *context);

GDK_AVAILABLE_IN_ALL
gboolean                gdk_gl_context_realize                  (GdkGLContext  *context,
                                                                 GError       **error);

GDK_AVAILABLE_IN_ALL
void                    gdk_gl_context_make_current             (GdkGLContext  *context);
GDK_AVAILABLE_IN_ALL
GdkGLContext *          gdk_gl_context_get_current              (void);
GDK_AVAILABLE_IN_ALL
void                    gdk_gl_context_clear_current            (void);

G_END_DECLS

#endif /* __GDK_GL_CONTEXT_H__ */
