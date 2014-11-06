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
#include "gdkdisplayprivate.h"
#include "gdkvisual.h"
#include "gdkwindow.h"
#include "gdkinternals.h"
#include "gdkmain.h"

G_BEGIN_DECLS

struct _GdkX11GLContext
{
  GdkGLContext parent_instance;

  GdkGLProfile profile;
  GLXContext glx_context;
  GLXFBConfig glx_config;
  GLXDrawable drawable;

  guint is_attached : 1;
  guint is_direct : 1;
  guint do_frame_sync : 1;

  guint do_blit_swap : 1;
};

struct _GdkX11GLContextClass
{
  GdkGLContextClass parent_class;
};

gboolean        gdk_x11_screen_init_gl                          (GdkScreen         *screen);
GdkGLContext *  gdk_x11_window_create_gl_context                (GdkWindow         *window,
								 gboolean           attached,
                                                                 GdkGLProfile       profile,
                                                                 GdkGLContext      *share,
                                                                 GError           **error);
void            gdk_x11_window_invalidate_for_new_frame         (GdkWindow         *window,
                                                                 cairo_region_t    *update_area);
gboolean        gdk_x11_display_make_gl_context_current         (GdkDisplay        *display,
                                                                 GdkGLContext      *context);

G_END_DECLS

#endif /* __GDK_X11_GL_CONTEXT__ */
