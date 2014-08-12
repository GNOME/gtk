/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext-x11.h: Private X11 specific OpenGL wrappers
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

#ifndef __GDK_X11_GL_CONTEXT__
#define __GDK_X11_GL_CONTEXT__

#include <X11/X.h>
#include <X11/Xlib.h>

#include <epoxy/gl.h>
#include <epoxy/glx.h>

#include "gdkglcontextprivate.h"
#include "gdkglpixelformatprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkglpixelformat.h"
#include "gdkvisual.h"
#include "gdkwindow.h"
#include "gdkinternals.h"
#include "gdkmain.h"

G_BEGIN_DECLS

struct _GdkX11GLContext
{
  GdkGLContext parent_instance;

  GLXContext glx_context;
  GLXFBConfig glx_config;

  GLXDrawable current_drawable;

  Window dummy_drawable;
  GLXWindow dummy_glx_drawable;

  guint is_direct : 1;
};

struct _GdkX11GLContextClass
{
  GdkGLContextClass parent_class;
};

gboolean        gdk_x11_display_init_gl                         (GdkDisplay        *display);
gboolean        gdk_x11_display_validate_gl_pixel_format        (GdkDisplay        *display,
                                                                 GdkGLPixelFormat  *format,
                                                                 GError           **error);
GdkGLContext *  gdk_x11_display_create_gl_context               (GdkDisplay        *display,
                                                                 GdkGLPixelFormat  *format,
                                                                 GdkGLContext      *share,
                                                                 GError           **error);
void            gdk_x11_display_destroy_gl_context              (GdkDisplay        *display,
                                                                 GdkGLContext      *context);
gboolean        gdk_x11_display_make_gl_context_current         (GdkDisplay        *display,
                                                                 GdkGLContext      *context,
                                                                 GdkWindow         *window);

G_END_DECLS

#endif /* __GDK_X11_GL_CONTEXT__ */
