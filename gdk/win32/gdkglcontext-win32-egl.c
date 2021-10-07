/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext-win32.c: Win32 specific OpenGL wrappers
 *
 * Copyright © 2014 Emmanuele Bassi
 * Copyright © 2014 Alexander Larsson
 * Copyright © 2014 Chun-wei Fan
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

#include "config.h"

#include "gdkprivate-win32.h"
#include "gdksurface-win32.h"
#include "gdkglcontext-win32.h"
#include "gdkdisplay-win32.h"

#include "gdkwin32display.h"
#include "gdkwin32glcontext.h"
#include "gdkwin32misc.h"
#include "gdkwin32screen.h"
#include "gdkwin32surface.h"

#include "gdkglcontext.h"
#include "gdksurface.h"
#include "gdkintl.h"

#include <cairo.h>
#include <epoxy/egl.h>

struct _GdkWin32GLContextEGL
{
  GdkWin32GLContext parent_instance;

  /* EGL (Angle) Context Items */
  EGLContext egl_context;
  guint do_frame_sync : 1;
};

typedef struct _GdkWin32GLContextClass    GdkWin32GLContextEGLClass;

G_DEFINE_TYPE (GdkWin32GLContextEGL, gdk_win32_gl_context_egl, GDK_TYPE_WIN32_GL_CONTEXT)

static gboolean
is_egl_force_redraw (GdkSurface *surface)
{
  /* We only need to call gdk_window_invalidate_rect () if necessary */
  if (surface->gl_paint_context != NULL && gdk_gl_context_get_use_es (surface->gl_paint_context))
    {
      GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

      return impl->egl_force_redraw_all;
    }
  return FALSE;
}

static void
reset_egl_force_redraw (GdkSurface *surface)
{
  if (surface->gl_paint_context != NULL && gdk_gl_context_get_use_es (surface->gl_paint_context))
    {
      GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

      if (impl->egl_force_redraw_all)
        impl->egl_force_redraw_all = FALSE;
    }
}

static void
gdk_win32_gl_context_egl_end_frame (GdkDrawContext *draw_context,
                                    cairo_region_t *painted)
{
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);
  GdkWin32GLContextEGL *context_egl = GDK_WIN32_GL_CONTEXT_EGL (context);
  GdkSurface *surface = gdk_gl_context_get_surface (context);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  cairo_rectangle_int_t whole_window;
  EGLSurface egl_surface;

  GDK_DRAW_CONTEXT_CLASS (gdk_win32_gl_context_egl_parent_class)->end_frame (draw_context, painted);

  gdk_gl_context_make_current (context);
  whole_window =
    (GdkRectangle) { 0, 0,
	                 gdk_surface_get_width (surface),
					 gdk_surface_get_height (surface)
                   };

  egl_surface = gdk_surface_get_egl_surface (surface);

  if (is_egl_force_redraw (surface))
    {
      GdkRectangle rect = {0, 0, gdk_surface_get_width (surface), gdk_surface_get_height (surface)};

      /* We need to do gdk_window_invalidate_rect() so that we don't get glitches after maximizing or
       *  restoring or using aerosnap
       */
      gdk_surface_invalidate_rect (surface, &rect);
      reset_egl_force_redraw (surface);
    }

  eglSwapBuffers (gdk_display_get_egl_display (display), egl_surface);
}

#define N_EGL_ATTRS     16

static EGLContext
create_egl_context (EGLDisplay    display,
                    EGLConfig     config,
                    GdkGLContext *share,
                    int           flags,
                    int           major,
                    int           minor,
                    gboolean     *is_legacy)
{
  EGLContext ctx;
  EGLint context_attribs[N_EGL_ATTRS];
  int i = 0;

  /* ANGLE does not support the GL_OES_vertex_array_object extension, so we need to use ES3 directly */
  context_attribs[i++] = EGL_CONTEXT_CLIENT_VERSION;
  context_attribs[i++] = 3;

  /* Specify the flags */
  context_attribs[i++] = EGL_CONTEXT_FLAGS_KHR;
  context_attribs[i++] = flags;

  context_attribs[i++] = EGL_NONE;
  g_assert (i < N_EGL_ATTRS);

  ctx = eglCreateContext (display,
                          config,
                          share != NULL ? GDK_WIN32_GL_CONTEXT_EGL (share)->egl_context
                                        : EGL_NO_CONTEXT,
                          context_attribs);

  if (ctx != EGL_NO_CONTEXT)
    GDK_NOTE (OPENGL, g_message ("Created EGL context[%p]", ctx));

  return ctx;
}

static gboolean
gdk_win32_gl_context_egl_realize (GdkGLContext *context,
                                  GError **error)
{
  GdkWin32GLContextEGL *context_egl = GDK_WIN32_GL_CONTEXT_EGL (context);

  gboolean debug_bit, compat_bit, legacy_bit;
  gboolean use_es = FALSE;
  EGLContext egl_context;
  EGLContext ctx;

  /* request flags and specific versions for core (3.2+) WGL context */
  int flags = 0;
  int major = 0;
  int minor = 0;

  GdkSurface *surface = gdk_gl_context_get_surface (context);
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  EGLDisplay egl_display = gdk_display_get_egl_display (display);
  EGLConfig egl_config = gdk_display_get_egl_config (display);
  GdkGLContext *share = gdk_display_get_gl_context (display);

  gdk_gl_context_get_required_version (context, &major, &minor);
  debug_bit = gdk_gl_context_get_debug_enabled (context);
  compat_bit = gdk_gl_context_get_forward_compatible (context);

  /*
   * A legacy context cannot be shared with core profile ones, so this means we
   * must stick to a legacy context if the shared context is a legacy context
   */

  /* if GDK_GL_LEGACY is set, we default to a legacy context */
  legacy_bit = GDK_DISPLAY_DEBUG_CHECK (display, GL_LEGACY) ?
               TRUE :
			   share != NULL && gdk_gl_context_is_legacy (share);

  use_es = GDK_DISPLAY_DEBUG_CHECK (display, GL_GLES) ||
    (share != NULL && gdk_gl_context_get_use_es (share));

  if (debug_bit)
    flags |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
  if (compat_bit)
    flags |= EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;

  GDK_NOTE (OPENGL, g_message ("Creating EGL context version %d.%d (debug:%s, forward:%s, legacy:%s)",
                               major, minor,
                               debug_bit ? "yes" : "no",
                               compat_bit ? "yes" : "no",
                               legacy_bit ? "yes" : "no"));

  ctx = create_egl_context (egl_display,
                            egl_config,
                            share,
                            flags,
                            major,
                            minor,
                           &legacy_bit);

  if (ctx == EGL_NO_CONTEXT)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Unable to create a GL context"));
      return FALSE;
    }

  GDK_NOTE (OPENGL, g_print ("Created EGL context[%p]\n", ctx));

  context_egl->egl_context = ctx;

  /* We are using GLES here */
  gdk_gl_context_set_use_es (context, TRUE);

  /* Ensure that any other context is created with a legacy bit set */
  gdk_gl_context_set_is_legacy (context, legacy_bit);

  return TRUE;
}

static void
gdk_win32_gl_context_egl_begin_frame (GdkDrawContext *draw_context,
                                      gboolean        prefers_high_depth,
                                      cairo_region_t *update_area)
{
  gdk_win32_surface_handle_queued_move_resize (draw_context);

  GDK_DRAW_CONTEXT_CLASS (gdk_win32_gl_context_egl_parent_class)->begin_frame (draw_context, prefers_high_depth, update_area);
}

static void
gdk_win32_gl_context_egl_class_init (GdkWin32GLContextClass *klass)
{
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS(klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS(klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  context_class->backend_type = GDK_GL_EGL;

  context_class->realize = gdk_win32_gl_context_egl_realize;

  draw_context_class->begin_frame = gdk_win32_gl_context_egl_begin_frame;
  draw_context_class->end_frame = gdk_win32_gl_context_egl_end_frame;
}

static void
gdk_win32_gl_context_egl_init (GdkWin32GLContextEGL *egl_context)
{
}
