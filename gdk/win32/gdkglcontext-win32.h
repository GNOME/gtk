/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext-win32.h: Private Win32 specific OpenGL wrappers
 *
 * Copyright © 2014 Chun-wei Fan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#ifndef DONT_INCLUDE_LIBEPOXY
#ifdef GDK_WINDOWING_WIN32
/* epoxy needs this, see https://github.com/anholt/libepoxy/issues/299 */
#include <windows.h>
#endif
#include <epoxy/gl.h>
#include <epoxy/wgl.h>

#ifdef HAVE_EGL
# include <epoxy/egl.h>
#endif

#include "gdkglcontextprivate.h"
#include "gdkdisplayprivate.h"
#include "gdksurface.h"
#else
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <GL/gl.h>

# include <glib.h>
#endif

G_BEGIN_DECLS

#ifndef DONT_INCLUDE_LIBEPOXY

#define GDK_WIN32_GL_CONTEXT_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WIN32_GL_CONTEXT, GdkWin32GLContextClass))
#define GDK_WIN32_GL_CONTEXT_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WIN32_GL_CONTEXT, GdkWin32GLContextClass))
#define GDK_WIN32_IS_GL_CONTEXT_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WIN32_GL_CONTEXT))

struct _GdkWin32GLContext
{
  GdkGLContext parent_instance;

  HWND handle;
};

struct _GdkWin32GLContextClass
{
  GdkGLContextClass parent_class;
};

ATOM            gdk_win32_gl_context_get_class          (void);

/* WGL */
#define GDK_TYPE_WIN32_GL_CONTEXT_WGL     (gdk_win32_gl_context_wgl_get_type())
#define GDK_WIN32_GL_CONTEXT_WGL(obj)     (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_WIN32_GL_CONTEXT_WGL, GdkWin32GLContextWGL))
#define GDK_IS_WIN32_GL_CONTEXT_WGL(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_WIN32_GL_CONTEXT_WGL))

typedef struct _GdkWin32GLContextWGL      GdkWin32GLContextWGL;

GdkGLContext *  gdk_win32_display_init_wgl              (GdkDisplay             *display,
                                                         GError                **error);

GType           gdk_win32_gl_context_wgl_get_type       (void) G_GNUC_CONST;

/* EGL */
#define GDK_TYPE_WIN32_GL_CONTEXT_EGL     (gdk_win32_gl_context_egl_get_type())
#define GDK_WIN32_GL_CONTEXT_EGL(obj)     (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_WIN32_GL_CONTEXT_EGL, GdkWin32GLContextEGL))
#define GDK_IS_WIN32_GL_CONTEXT_EGL(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_WIN32_GL_CONTEXT_EGL))

typedef struct _GdkWin32GLContextEGL      GdkWin32GLContextEGL;

GType     gdk_win32_gl_context_egl_get_type         (void) G_GNUC_CONST;

#endif /* !DONT_INCLUDE_LIBEPOXY */

HGLRC     gdk_win32_private_wglGetCurrentContext (void);
BOOL      gdk_win32_private_wglMakeCurrent       (HDC hdc,
                                                  HGLRC hglrc);
void      gdk_win32_private_wglDeleteContext     (HGLRC hglrc);

G_END_DECLS

