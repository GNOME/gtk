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
#include "gdkinternals.h"
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

static void
gdk_win32_gl_context_egl_dispose (GObject *gobject)
{
  GdkGLContext *context = GDK_GL_CONTEXT (gobject);
  GdkWin32GLContextEGL *context_egl = GDK_WIN32_GL_CONTEXT_EGL (gobject);
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (gdk_gl_context_get_display (context));
  GdkSurface *surface = gdk_gl_context_get_surface (context);

  if (display_win32 != NULL)
    {
      if (eglGetCurrentContext () == context_egl->egl_context)
        eglMakeCurrent(display_win32->egl_disp, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);

      GDK_NOTE (OPENGL, g_message ("Destroying EGL (ANGLE) context"));

      eglDestroyContext (display_win32->egl_disp,
                         context_egl->egl_context);
      context_egl->egl_context = EGL_NO_CONTEXT;
    }

  G_OBJECT_CLASS (gdk_win32_gl_context_egl_parent_class)->dispose (gobject);
}

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
  GdkWin32Display *display_win32 = (GDK_WIN32_DISPLAY (gdk_gl_context_get_display (context)));
  cairo_rectangle_int_t whole_window;
  EGLSurface egl_surface;

  GDK_DRAW_CONTEXT_CLASS (gdk_win32_gl_context_egl_parent_class)->end_frame (draw_context, painted);

  gdk_gl_context_make_current (context);
  whole_window =
    (GdkRectangle) { 0, 0,
	                 gdk_surface_get_width (surface),
					 gdk_surface_get_height (surface)
                   };

  egl_surface = gdk_win32_surface_get_egl_surface (surface, display_win32->egl_config, FALSE);

  if (is_egl_force_redraw (surface))
    {
      GdkRectangle rect = {0, 0, gdk_surface_get_width (surface), gdk_surface_get_height (surface)};

      /* We need to do gdk_window_invalidate_rect() so that we don't get glitches after maximizing or
       *  restoring or using aerosnap
       */
      gdk_surface_invalidate_rect (surface, &rect);
      reset_egl_force_redraw (surface);
    }

  eglSwapBuffers (display_win32->egl_disp, egl_surface);
}

#ifndef EGL_PLATFORM_ANGLE_ANGLE
#define EGL_PLATFORM_ANGLE_ANGLE          0x3202
#endif

#ifndef EGL_PLATFORM_ANGLE_TYPE_ANGLE
#define EGL_PLATFORM_ANGLE_TYPE_ANGLE     0x3203
#endif

#ifndef EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE
#define EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE 0x3208
#endif

static EGLDisplay
gdk_win32_get_egl_display (GdkWin32Display *display)
{
  EGLDisplay disp;

  if (epoxy_has_egl_extension (NULL, "EGL_EXT_platform_base"))
    {
      PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay = (void *) eglGetProcAddress ("eglGetPlatformDisplayEXT");
      if (getPlatformDisplay)
        {
          EGLint disp_attr[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, EGL_NONE};

          disp = getPlatformDisplay (EGL_PLATFORM_ANGLE_ANGLE, display->hdc_egl_temp, disp_attr);

          if (disp != EGL_NO_DISPLAY)
            return disp;
        }
    }

  return eglGetDisplay (display->hdc_egl_temp);
}

#define MAX_EGL_ATTRS   30

static gboolean
find_eglconfig_for_window (GdkWin32Display  *display,
                           EGLConfig        *egl_config_out,
                           EGLint           *min_swap_interval_out,
                           GError          **error)
{
  EGLint attrs[MAX_EGL_ATTRS];
  EGLint count;
  EGLConfig *configs, chosen_config;

  int i = 0;

  EGLDisplay egl_disp = display->egl_disp;

  attrs[i++] = EGL_CONFORMANT;
  attrs[i++] = EGL_OPENGL_ES2_BIT;
  attrs[i++] = EGL_SURFACE_TYPE;
  attrs[i++] = EGL_WINDOW_BIT;

  attrs[i++] = EGL_COLOR_BUFFER_TYPE;
  attrs[i++] = EGL_RGB_BUFFER;

  attrs[i++] = EGL_RED_SIZE;
  attrs[i++] = 1;
  attrs[i++] = EGL_GREEN_SIZE;
  attrs[i++] = 1;
  attrs[i++] = EGL_BLUE_SIZE;
  attrs[i++] = 1;
  attrs[i++] = EGL_ALPHA_SIZE;
  attrs[i++] = 1;

  attrs[i++] = EGL_NONE;
  g_assert (i < MAX_EGL_ATTRS);

  if (!eglChooseConfig (display->egl_disp, attrs, NULL, 0, &count) || count < 1)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                           _("No available configurations for the given pixel format"));
      return FALSE;
    }

  configs = g_new (EGLConfig, count);

  if (!eglChooseConfig (display->egl_disp, attrs, configs, count, &count) || count < 1)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                           _("No available configurations for the given pixel format"));
      return FALSE;
    }

  /* Pick first valid configuration i guess? */
  chosen_config = configs[0];

  if (!eglGetConfigAttrib (display->egl_disp, chosen_config,
                           EGL_MIN_SWAP_INTERVAL, min_swap_interval_out))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           "Could not retrieve the minimum swap interval");
      g_free (configs);
      return FALSE;
    }

  if (egl_config_out != NULL)
    *egl_config_out = chosen_config;

  g_free (configs);

  return TRUE;
}

gboolean
gdk_win32_display_init_egl (GdkDisplay  *display,
                            GError     **error)
{
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  int best_idx = 0;
  EGLDisplay egl_disp;

  if (display_win32->egl_disp != EGL_NO_DISPLAY)
    return TRUE;
  
  egl_disp = gdk_win32_get_egl_display (display_win32);

  if (egl_disp == EGL_NO_DISPLAY)
    return FALSE;

  if (!eglInitialize (egl_disp, NULL, NULL))
    {
      eglTerminate (egl_disp);
      egl_disp = EGL_NO_DISPLAY;
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL implementation is available"));
      return FALSE;
    }

  display_win32->egl_disp = egl_disp;
  display_win32->egl_version = epoxy_egl_version (egl_disp);

  eglBindAPI (EGL_OPENGL_ES_API);

  display_win32->hasEglSurfacelessContext =
    epoxy_has_egl_extension (egl_disp, "EGL_KHR_surfaceless_context");

  GDK_NOTE (OPENGL,
            g_print ("EGL API version %d.%d found\n"
                     " - Vendor: %s\n"
                     " - Checked extensions:\n"
                     "\t* EGL_KHR_surfaceless_context: %s\n",
                     display_win32->egl_version / 10,
                     display_win32->egl_version % 10,
                     eglQueryString (display_win32->egl_disp, EGL_VENDOR),
                     display_win32->hasEglSurfacelessContext ? "yes" : "no"));

  return find_eglconfig_for_window (display_win32, &display_win32->egl_config,
                                   &display_win32->egl_min_swap_interval, error);
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
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
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

  ctx = create_egl_context (display_win32->egl_disp,
                            display_win32->egl_config,
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

static gboolean
gdk_win32_gl_context_egl_clear_current (GdkGLContext *context)
{
  GdkDisplay *display = gdk_gl_context_get_display (context);
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);

  if (display_win32->egl_disp != EGL_NO_DISPLAY)
    return eglMakeCurrent (display_win32->egl_disp,
                           EGL_NO_SURFACE, 
                           EGL_NO_SURFACE, 
                           EGL_NO_CONTEXT);
  else
    return TRUE;
}

static gboolean
gdk_win32_gl_context_egl_make_current (GdkGLContext *context,
                                       gboolean      surfaceless)
{
  GdkWin32GLContextEGL *context_egl = GDK_WIN32_GL_CONTEXT_EGL (context);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  GdkSurface *surface;

  gboolean do_frame_sync = FALSE;

  EGLSurface egl_surface;

  surface = gdk_gl_context_get_surface (context);

  if (!surfaceless)
    egl_surface = gdk_win32_surface_get_egl_surface (surface, display_win32->egl_config, FALSE);
  else
    {
      if (display_win32->hasEglSurfacelessContext)
        egl_surface = EGL_NO_SURFACE;
      else
        egl_surface = gdk_win32_surface_get_egl_surface (surface, display_win32->egl_config, TRUE);
    }

  if (!eglMakeCurrent (display_win32->egl_disp,
                       egl_surface,
                       egl_surface,
                       context_egl->egl_context))
    return FALSE;

  if (display_win32->egl_min_swap_interval == 0)
    eglSwapInterval (display_win32->egl_disp, 0);
  else
    g_debug ("Can't disable GL swap interval");

  return TRUE;
}

static void
gdk_win32_gl_context_egl_begin_frame (GdkDrawContext *draw_context,
                                      cairo_region_t *update_area)
{
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);
  GdkSurface *surface;

  surface = gdk_gl_context_get_surface (context);

  gdk_win32_surface_handle_queued_move_resize (draw_context);

  GDK_DRAW_CONTEXT_CLASS (gdk_win32_gl_context_egl_parent_class)->begin_frame (draw_context, update_area);
}

static void
gdk_win32_gl_context_egl_class_init (GdkWin32GLContextClass *klass)
{
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS(klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS(klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  context_class->realize = gdk_win32_gl_context_egl_realize;
  context_class->make_current = gdk_win32_gl_context_egl_make_current;
  context_class->clear_current = gdk_win32_gl_context_egl_clear_current;

  draw_context_class->begin_frame = gdk_win32_gl_context_egl_begin_frame;
  draw_context_class->end_frame = gdk_win32_gl_context_egl_end_frame;

  gobject_class->dispose = gdk_win32_gl_context_egl_dispose;
}

static void
gdk_win32_gl_context_egl_init (GdkWin32GLContextEGL *egl_context)
{
}
