/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext-wayland.h: Private Wayland specific OpenGL wrappers
 *
 * Copyright © 2014  Emmanuele Bassi
 * Copyright © 2014  Red Hat, Int
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

#ifndef __GDK_WAYLAND_GL_CONTEXT__
#define __GDK_WAYLAND_GL_CONTEXT__

#include "gdkglcontextprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkvisual.h"
#include "gdkwindow.h"
#include "gdkinternals.h"
#include "gdkmain.h"

#include <epoxy/egl.h>

G_BEGIN_DECLS

struct _GdkWaylandGLContext
{
  GdkGLContext parent_instance;

  EGLContext egl_context;
  EGLConfig egl_config;
  gboolean is_attached;
};

struct _GdkWaylandGLContextClass
{
  GdkGLContextClass parent_class;
};

gboolean        gdk_wayland_display_init_gl                         (GdkDisplay        *display);
GdkGLContext *  gdk_wayland_window_create_gl_context                (GdkWindow         *window,
								     gboolean           attach,
                                                                     GdkGLProfile       profile,
                                                                     GdkGLContext      *share,
                                                                     GError           **error);
void            gdk_wayland_window_invalidate_for_new_frame         (GdkWindow         *window,
                                                                     cairo_region_t    *update_area);
void            gdk_wayland_display_destroy_gl_context              (GdkDisplay        *display,
                                                                     GdkGLContext      *context);
gboolean        gdk_wayland_display_make_gl_context_current         (GdkDisplay        *display,
                                                                     GdkGLContext      *context);

G_END_DECLS

#endif /* __GDK_WAYLAND_GL_CONTEXT__ */
