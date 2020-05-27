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

#ifdef HAVE_XDAMAGE
#include <X11/extensions/Xdamage.h>
#endif

#include <epoxy/gl.h>
#include <epoxy/glx.h>

#include "gdkglcontextprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkvisual-x11.h"
#include "gdksurface.h"
#include "gdkinternals.h"

G_BEGIN_DECLS

struct _GdkX11GLContext
{
  GdkGLContext parent_instance;

  GLXContext glx_context;
  GLXFBConfig glx_config;
  GLXDrawable attached_drawable;
  GLXDrawable unattached_drawable;

#ifdef HAVE_XDAMAGE
  GLsync frame_fence;
  Damage xdamage;
#endif

  guint is_attached : 1;
  guint is_direct : 1;
  guint do_frame_sync : 1;
};

struct _GdkX11GLContextClass
{
  GdkGLContextClass parent_class;
};

gboolean        gdk_x11_screen_init_gl                          (GdkX11Screen      *screen);
GdkGLContext *  gdk_x11_surface_create_gl_context                (GdkSurface         *window,
								 gboolean           attached,
                                                                 GdkGLContext      *share,
                                                                 GError           **error);
gboolean        gdk_x11_display_make_gl_context_current         (GdkDisplay        *display,
                                                                 GdkGLContext      *context);

G_END_DECLS

#endif /* __GDK_X11_GL_CONTEXT__ */
