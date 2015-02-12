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

#include <epoxy/gl.h>
#include <epoxy/wgl.h>

#include "gdkglcontextprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkvisual.h"
#include "gdkwindow.h"
#include "gdkinternals.h"
#include "gdkmain.h"

G_BEGIN_DECLS

struct _GdkWin32GLContext
{
  GdkGLContext parent_instance;

  /* WGL Context Items */
  HGLRC hglrc;
  HDC gl_hdc;
  guint need_alpha_bits : 1;

  /* other items */
  guint is_attached : 1;
  guint do_frame_sync : 1;
  guint do_blit_swap : 1;
};

struct _GdkWin32GLContextClass
{
  GdkGLContextClass parent_class;
};

GdkGLContext *
_gdk_win32_window_create_gl_context (GdkWindow *window,
                                     gboolean attached,
                                     GdkGLContext *share,
                                     GError **error);

void
_gdk_win32_window_invalidate_for_new_frame (GdkWindow *window,
                                            cairo_region_t *update_area);

void
_gdk_win32_gl_context_end_frame (GdkGLContext *context,
                                 cairo_region_t *painted,
                                 cairo_region_t *damage);

gboolean
_gdk_win32_display_make_gl_context_current (GdkDisplay *display,
                                            GdkGLContext *context);

gboolean
_gdk_win32_gl_context_realize (GdkGLContext *context,
                               GError **error);

G_END_DECLS

#endif /* __GDK_WIN32_GL_CONTEXT__ */
