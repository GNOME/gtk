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

#ifndef __GDK_WIN32_GL_CONTEXT__
#define __GDK_WIN32_GL_CONTEXT__

#ifdef DONT_INCLUDE_LIBEPOXY
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <GL/gl.h>

# include <glib.h>
#else
# include <epoxy/gl.h>
# include <epoxy/wgl.h>

# ifdef GDK_WIN32_ENABLE_EGL
#  include <epoxy/egl.h>
# endif

# include "gdkglcontextprivate.h"
# include "gdkdisplayprivate.h"
# include "gdkvisual.h"
# include "gdkwindow.h"
#endif

G_BEGIN_DECLS

#ifndef DONT_INCLUDE_LIBEPOXY
void
gdk_win32_window_invalidate_egl_framebuffer (GdkWindow      *window);

GdkGLContext *
gdk_win32_window_create_gl_context          (GdkWindow      *window,
                                             gboolean        attached,
                                             GdkGLContext   *share,
                                             GError        **error);

void
gdk_win32_window_invalidate_for_new_frame   (GdkWindow      *window,
                                             cairo_region_t *update_area);

gboolean
gdk_win32_display_make_gl_context_current   (GdkDisplay     *display,
                                             GdkGLContext   *context);

#endif /* !DONT_INCLUDE_LIBEPOXY */

HGLRC     gdk_win32_private_wglGetCurrentContext (void);
BOOL      gdk_win32_private_wglMakeCurrent       (HDC hdc,
                                                  HGLRC hglrc);
void      gdk_win32_private_wglDeleteContext     (HGLRC hglrc);


G_END_DECLS

#endif /* __GDK_WIN32_GL_CONTEXT__ */
