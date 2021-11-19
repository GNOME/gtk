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
#include "gdkprofilerprivate.h"
#include "gdkintl.h"
#include "gdksurface.h"

#include <cairo.h>
#include <epoxy/wgl.h>

struct _GdkWin32GLContextWGL
{
  GdkWin32GLContext parent_instance;

  HGLRC wgl_context;
  guint do_frame_sync : 1;
};

typedef struct _GdkWin32GLContextClass    GdkWin32GLContextWGLClass;

G_DEFINE_TYPE (GdkWin32GLContextWGL, gdk_win32_gl_context_wgl, GDK_TYPE_WIN32_GL_CONTEXT)

static void
gdk_win32_gl_context_wgl_dispose (GObject *gobject)
{
  GdkWin32GLContextWGL *context_wgl = GDK_WIN32_GL_CONTEXT_WGL (gobject);

  if (context_wgl->wgl_context != NULL)
    {
      if (wglGetCurrentContext () == context_wgl->wgl_context)
        wglMakeCurrent (NULL, NULL);

      GDK_NOTE (OPENGL, g_print ("Destroying WGL context\n"));

      wglDeleteContext (context_wgl->wgl_context);
      context_wgl->wgl_context = NULL;
    }

  G_OBJECT_CLASS (gdk_win32_gl_context_wgl_parent_class)->dispose (gobject);
}

static void
gdk_win32_gl_context_wgl_end_frame (GdkDrawContext *draw_context,
                                    cairo_region_t *painted)
{
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);
  GdkWin32GLContextWGL *context_wgl = GDK_WIN32_GL_CONTEXT_WGL (context);
  GdkSurface *surface = gdk_gl_context_get_surface (context);
  GdkWin32Display *display_win32 = (GDK_WIN32_DISPLAY (gdk_gl_context_get_display (context)));
  gboolean can_wait = display_win32->hasWglOMLSyncControl;
  HDC hdc;

  GDK_DRAW_CONTEXT_CLASS (gdk_win32_gl_context_wgl_parent_class)->end_frame (draw_context, painted);

  gdk_gl_context_make_current (context);

  gdk_profiler_add_mark (GDK_PROFILER_CURRENT_TIME, 0, "win32", "swap buffers");

  if (surface != NULL)
    hdc = GDK_WIN32_SURFACE (surface)->hdc;
  else
    hdc = display_win32->dummy_context_wgl.hdc;

  if (context_wgl->do_frame_sync)
    {

      glFinish ();

      if (can_wait)
        {
          gint64 ust, msc, sbc;

          wglGetSyncValuesOML (hdc, &ust, &msc, &sbc);
          wglWaitForMscOML (hdc,
                            0,
                            2,
                            (msc + 1) % 2,
                           &ust, &msc, &sbc);
        }

    }

  SwapBuffers (hdc);
}

static void
gdk_win32_gl_context_wgl_begin_frame (GdkDrawContext *draw_context,
                                      gboolean        prefers_high_depth,
                                      cairo_region_t *update_area)
{
  gdk_win32_surface_handle_queued_move_resize (draw_context);

  GDK_DRAW_CONTEXT_CLASS (gdk_win32_gl_context_wgl_parent_class)->begin_frame (draw_context, prefers_high_depth, update_area);
}

static int
gdk_init_dummy_wgl_context (GdkWin32Display *display_win32);

#define PIXEL_ATTRIBUTES 17

static int
get_wgl_pfd (HDC                    hdc,
             PIXELFORMATDESCRIPTOR *pfd,
             GdkWin32Display       *display_win32)
{
  int best_pf = 0;

  pfd->nSize = sizeof (PIXELFORMATDESCRIPTOR);

  if (display_win32 != NULL && display_win32->hasWglARBPixelFormat)
    {
      UINT num_formats;
      int colorbits = GetDeviceCaps (hdc, BITSPIXEL);
      int i = 0;
      int pixelAttribs[PIXEL_ATTRIBUTES];

      /* Save up the HDC and HGLRC that we are currently using, to restore back to it when we are done here */
      HDC hdc_current = wglGetCurrentDC ();
      HGLRC hglrc_current = wglGetCurrentContext ();

      /* Update PIXEL_ATTRIBUTES above if any groups are added here! */
      pixelAttribs[i] = WGL_DRAW_TO_WINDOW_ARB;
      pixelAttribs[i++] = GL_TRUE;

      pixelAttribs[i++] = WGL_SUPPORT_OPENGL_ARB;
      pixelAttribs[i++] = GL_TRUE;

      pixelAttribs[i++] = WGL_DOUBLE_BUFFER_ARB;
      pixelAttribs[i++] = GL_TRUE;

      pixelAttribs[i++] = WGL_ACCELERATION_ARB;
      pixelAttribs[i++] = WGL_FULL_ACCELERATION_ARB;

      pixelAttribs[i++] = WGL_PIXEL_TYPE_ARB;
      pixelAttribs[i++] = WGL_TYPE_RGBA_ARB;

      pixelAttribs[i++] = WGL_COLOR_BITS_ARB;
      pixelAttribs[i++] = colorbits;

      /* end of "Update PIXEL_ATTRIBUTES above if any groups are added here!" */

      if (display_win32->hasWglARBmultisample)
        {
          pixelAttribs[i++] = WGL_SAMPLE_BUFFERS_ARB;
          pixelAttribs[i++] = 1;

          pixelAttribs[i++] = WGL_SAMPLES_ARB;
          pixelAttribs[i++] = 8;
        }

      pixelAttribs[i++] = 0; /* end of pixelAttribs */
      best_pf = gdk_init_dummy_wgl_context (display_win32);

      if (!wglMakeCurrent (display_win32->dummy_context_wgl.hdc,
                           display_win32->dummy_context_wgl.hglrc))
        {
          wglMakeCurrent (hdc_current, hglrc_current);
          return 0;
        }

      wglChoosePixelFormatARB (hdc,
                               pixelAttribs,
                               NULL,
                               1,
                              &best_pf,
                              &num_formats);

      /* Go back to the HDC that we were using, since we are done with the dummy HDC and GL Context */
      wglMakeCurrent (hdc_current, hglrc_current);
    }
  else
    {
      pfd->nVersion = 1;
      pfd->dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
      pfd->iPixelType = PFD_TYPE_RGBA;
      pfd->cColorBits = GetDeviceCaps (hdc, BITSPIXEL);
      pfd->cAlphaBits = 8;
      pfd->dwLayerMask = PFD_MAIN_PLANE;

      best_pf = ChoosePixelFormat (hdc, pfd);
    }

  return best_pf;
}

/* in WGL, for many OpenGL items, we need a dummy WGL context, so create
 * one and cache it for later use
 */
static int
gdk_init_dummy_wgl_context (GdkWin32Display *display_win32)
{
  PIXELFORMATDESCRIPTOR pfd;
  gboolean set_pixel_format_result = FALSE;
  int best_idx = 0;

  memset (&pfd, 0, sizeof (PIXELFORMATDESCRIPTOR));

  best_idx = get_wgl_pfd (display_win32->dummy_context_wgl.hdc, &pfd, NULL);

  if (best_idx != 0)
    set_pixel_format_result = SetPixelFormat (display_win32->dummy_context_wgl.hdc,
                                              best_idx,
                                             &pfd);

  if (best_idx == 0 || !set_pixel_format_result)
    return 0;

  display_win32->dummy_context_wgl.hglrc =
    wglCreateContext (display_win32->dummy_context_wgl.hdc);

  if (display_win32->dummy_context_wgl.hglrc == NULL)
    return 0;

  return best_idx;
}

gboolean
gdk_win32_display_init_wgl (GdkDisplay  *display,
                            GError     **error)
{
  int best_idx = 0;
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  HDC hdc;

  if (!gdk_gl_backend_can_be_used (GDK_GL_WGL, error))
    return FALSE;

  if (display_win32->wgl_pixel_format != 0)
    return TRUE;

  /* acquire and cache dummy Window (HWND & HDC) and
   * dummy GL Context, it is used to query functions
   * and used for other stuff as well
   */
  best_idx = gdk_init_dummy_wgl_context (display_win32);
  hdc = display_win32->dummy_context_wgl.hdc;

  if (best_idx == 0 ||
     !wglMakeCurrent (hdc, display_win32->dummy_context_wgl.hglrc))
    {
      if (display_win32->dummy_context_wgl.hglrc != NULL)
        wglDeleteContext (display_win32->dummy_context_wgl.hglrc);

      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL implementation is available"));

      return FALSE;
    }

  display_win32->gl_version = epoxy_gl_version ();

  /* We must have OpenGL/WGL 2.0 or later, or have the GL_ARB_shader_objects extension */
  if (display_win32->gl_version < 20)
    {
      if (!epoxy_has_gl_extension ("GL_ARB_shader_objects"))
        {
          wglMakeCurrent (NULL, NULL);
          wglDeleteContext (display_win32->dummy_context_wgl.hglrc);

          g_set_error_literal (error, GDK_GL_ERROR,
                               GDK_GL_ERROR_NOT_AVAILABLE,
                               _("No GL implementation is available"));

          return FALSE;
        }
    }

  display_win32->wgl_pixel_format = best_idx;

  display_win32->hasWglARBCreateContext =
    epoxy_has_wgl_extension (hdc, "WGL_ARB_create_context");
  display_win32->hasWglEXTSwapControl =
    epoxy_has_wgl_extension (hdc, "WGL_EXT_swap_control");
  display_win32->hasWglOMLSyncControl =
    epoxy_has_wgl_extension (hdc, "WGL_OML_sync_control");
  display_win32->hasWglARBPixelFormat =
    epoxy_has_wgl_extension (hdc, "WGL_ARB_pixel_format");
  display_win32->hasWglARBmultisample =
    epoxy_has_wgl_extension (hdc, "WGL_ARB_multisample");

  GDK_NOTE (OPENGL,
            g_print ("WGL API version %d.%d found\n"
                     " - Vendor: %s\n"
                     " - Checked extensions:\n"
                     "\t* WGL_ARB_pixel_format: %s\n"
                     "\t* WGL_ARB_create_context: %s\n"
                     "\t* WGL_EXT_swap_control: %s\n"
                     "\t* WGL_OML_sync_control: %s\n"
                     "\t* WGL_ARB_multisample: %s\n",
                     display_win32->gl_version / 10,
                     display_win32->gl_version % 10,
                     glGetString (GL_VENDOR),
                     display_win32->hasWglARBPixelFormat ? "yes" : "no",
                     display_win32->hasWglARBCreateContext ? "yes" : "no",
                     display_win32->hasWglEXTSwapControl ? "yes" : "no",
                     display_win32->hasWglOMLSyncControl ? "yes" : "no",
                     display_win32->hasWglARBmultisample ? "yes" : "no"));

  wglMakeCurrent (NULL, NULL);

  return TRUE;
}

/* Setup the legacy context after creating it */
static gboolean
ensure_legacy_wgl_context (HDC           hdc,
                           HGLRC         hglrc_legacy,
                           GdkGLContext *share)
{
  GdkWin32GLContextWGL *context_wgl;

  if (!wglMakeCurrent (hdc, hglrc_legacy))
    return FALSE;

  if (share != NULL)
    {
      context_wgl = GDK_WIN32_GL_CONTEXT_WGL (share);

      return wglShareLists (hglrc_legacy, context_wgl->wgl_context);
    }

  return TRUE;
}

static HGLRC
create_wgl_context_with_attribs (HDC           hdc,
                                 HGLRC         hglrc_base,
                                 GdkGLContext *share,
                                 int           flags,
                                 int           major,
                                 int           minor,
                                 gboolean     *is_legacy)
{
  HGLRC hglrc;
  GdkWin32GLContextWGL *context_wgl;

  /* if we have wglCreateContextAttribsARB(), create a
  * context with the compatibility profile if a legacy
  * context is requested, or when we go into fallback mode
  */
  int profile = *is_legacy ? WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB :
                             WGL_CONTEXT_CORE_PROFILE_BIT_ARB;

  int attribs[] = {
    WGL_CONTEXT_PROFILE_MASK_ARB,   profile,
    WGL_CONTEXT_MAJOR_VERSION_ARB, *is_legacy ? 3 : major,
    WGL_CONTEXT_MINOR_VERSION_ARB, *is_legacy ? 0 : minor,
    WGL_CONTEXT_FLAGS_ARB,          flags,
    0
  };

  if (share != NULL)
    context_wgl = GDK_WIN32_GL_CONTEXT_WGL (share);

  hglrc = wglCreateContextAttribsARB (hdc,
                                      share != NULL ? context_wgl->wgl_context : NULL,
                                      attribs);

  return hglrc;
}

static HGLRC
create_wgl_context (HDC           hdc,
                    GdkGLContext *share,
                    int           flags,
                    int           major,
                    int           minor,
                    gboolean     *is_legacy,
                    gboolean      hasWglARBCreateContext)
{
  /* We need a legacy context for *all* cases */
  HGLRC hglrc_base = wglCreateContext (hdc);
  gboolean success = TRUE;

  /* Save up the HDC and HGLRC that we are currently using, to restore back to it when we are done here  */
  HDC hdc_current = wglGetCurrentDC ();
  HGLRC hglrc_current = wglGetCurrentContext ();

  /* if we have no wglCreateContextAttribsARB(), return the legacy context when all is set */
  if (*is_legacy && !hasWglARBCreateContext)
    {
      if (ensure_legacy_wgl_context (hdc, hglrc_base, share))
        {
          wglMakeCurrent (hdc_current, hglrc_current);
          return hglrc_base;
        }

      success = FALSE;
      goto gl_fail;
    }
  else
    {
      HGLRC hglrc = NULL;

      if (!wglMakeCurrent (hdc, hglrc_base))
        {
          success = FALSE;
          goto gl_fail;
        }

      /*
       * We need a Core GL 4.1 context in order to use the GL support in
       * the GStreamer media widget backend, but wglCreateContextAttribsARB()
       * may only give us the GL context version that we ask for here, and
       * nothing more.  So, if we are asking for a pre-GL 4.1 context,
       * try to ask for a 4.1 context explicitly first.  If that is not supported,
       * then we fall back to whatever version that we were asking for (or, even a
       * legacy context if that fails), at a price of not able to have GL support
       * for the media GStreamer backend.
       */
      if (major < 4 || (major == 4 && minor < 1))
        hglrc = create_wgl_context_with_attribs (hdc,
                                                 hglrc_base,
                                                 share,
                                                 flags,
                                                 4,
                                                 1,
                                                 is_legacy);

      if (hglrc == NULL)
        hglrc = create_wgl_context_with_attribs (hdc,
                                                 hglrc_base,
                                                 share,
                                                 flags,
                                                 major,
                                                 minor,
                                                 is_legacy);

      /* return the legacy context we have if it could be setup properly, in case the 3.0+ context creation failed */
      if (hglrc == NULL)
        {
          if (!(*is_legacy))
            {
              /* If we aren't using a legacy context in the beginning, try again with a compatibility profile 3.0 context */
              hglrc = create_wgl_context_with_attribs (hdc,
                                                       hglrc_base,
                                                       share,
                                                       flags,
                                                       0, 0,
                                                       is_legacy);

              *is_legacy = TRUE;
            }

          if (hglrc == NULL)
            {
              if (!ensure_legacy_wgl_context (hdc, hglrc_base, share))
                success = FALSE;
            }

          if (success)
            GDK_NOTE (OPENGL, g_print ("Using legacy context as fallback\n"));
        }

gl_fail:

      if (!success)
        {
          wglMakeCurrent (NULL, NULL);
          wglDeleteContext (hglrc_base);
          return NULL;
        }

      wglMakeCurrent (hdc_current, hglrc_current);

      if (hglrc != NULL)
        {
          wglDeleteContext (hglrc_base);
          return hglrc;
        }

      return hglrc_base;
  }
}

static gboolean
set_wgl_pixformat_for_hdc (HDC              hdc,
                           int             *best_idx,
                           GdkWin32Display *display_win32)
{
  gboolean already_checked = TRUE;
  *best_idx = GetPixelFormat (hdc);

  /* one is only allowed to call SetPixelFormat(), and so ChoosePixelFormat()
   * one single time per window HDC
   */
  if (*best_idx == 0)
    {
      PIXELFORMATDESCRIPTOR pfd;
      gboolean set_pixel_format_result = FALSE;

      GDK_NOTE (OPENGL, g_print ("requesting pixel format...\n"));
	  already_checked = FALSE;
      *best_idx = get_wgl_pfd (hdc, &pfd, display_win32);

      if (*best_idx != 0)
        set_pixel_format_result = SetPixelFormat (hdc, *best_idx, &pfd);

      /* ChoosePixelFormat() or SetPixelFormat() failed, bail out */
      if (*best_idx == 0 || !set_pixel_format_result)
        return FALSE;
    }

  GDK_NOTE (OPENGL, g_print ("%s""requested and set pixel format: %d\n", already_checked ? "already " : "", *best_idx));

  return TRUE;
}

static GdkGLAPI
gdk_win32_gl_context_wgl_realize (GdkGLContext *context,
                                  GError **error)
{
  GdkWin32GLContextWGL *context_wgl = GDK_WIN32_GL_CONTEXT_WGL (context);

  gboolean debug_bit, compat_bit, legacy_bit;

  /* request flags and specific versions for core (3.2+) WGL context */
  int flags = 0;
  int major = 0;
  int minor = 0;
  HGLRC hglrc;
  int pixel_format;
  HDC hdc;

  GdkSurface *surface = gdk_gl_context_get_surface (context);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  GdkGLContext *share = gdk_display_get_gl_context (display);

  if (!gdk_gl_context_is_api_allowed (context, GDK_GL_API_GL, error))
    return 0;

  gdk_gl_context_get_required_version (context, &major, &minor);
  debug_bit = gdk_gl_context_get_debug_enabled (context);
  compat_bit = gdk_gl_context_get_forward_compatible (context);

  if (surface != NULL)
    hdc = GDK_WIN32_SURFACE (surface)->hdc;
  else
    hdc = display_win32->dummy_context_wgl.hdc;

  /*
   * A legacy context cannot be shared with core profile ones, so this means we
   * must stick to a legacy context if the shared context is a legacy context
   */
  legacy_bit = GDK_DISPLAY_DEBUG_CHECK (display, GL_LEGACY) ?
               TRUE :
			   share != NULL && gdk_gl_context_is_legacy (share);

  if (!set_wgl_pixformat_for_hdc (hdc,
                                 &pixel_format,
                                  display_win32))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                           _("No available configurations for the given pixel format"));

      return 0;
    }

  /* if there isn't wglCreateContextAttribsARB() on WGL, use a legacy context */
  if (!legacy_bit)
    legacy_bit = !display_win32->hasWglARBCreateContext;
  if (debug_bit)
    flags |= WGL_CONTEXT_DEBUG_BIT_ARB;
  if (compat_bit)
    flags |= WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;

  GDK_NOTE (OPENGL,
            g_print ("Creating %s WGL context (version:%d.%d, debug:%s, forward:%s, legacy: %s)\n",
                      compat_bit ? "core" : "compat",
                      major,
                      minor,
                      debug_bit ? "yes" : "no",
                      compat_bit ? "yes" : "no",
                      legacy_bit ? "yes" : "no"));

  hglrc = create_wgl_context (hdc,
                              share,
                              flags,
                              major,
                              minor,
                             &legacy_bit,
                              display_win32->hasWglARBCreateContext);

  if (hglrc == NULL)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Unable to create a GL context"));
      return 0;
    }

  GDK_NOTE (OPENGL,
            g_print ("Created WGL context[%p], pixel_format=%d\n",
                     hglrc,
                     pixel_format));

  context_wgl->wgl_context = hglrc;

  /* Ensure that any other context is created with a legacy bit set */
  gdk_gl_context_set_is_legacy (context, legacy_bit);

  return GDK_GL_API_GL;
}

static gboolean
gdk_win32_gl_context_wgl_clear_current (GdkGLContext *context)
{
  return wglMakeCurrent (NULL, NULL);
}

static gboolean
gdk_win32_gl_context_wgl_make_current (GdkGLContext *context,
                                       gboolean      surfaceless)
{
  GdkWin32GLContextWGL *context_wgl = GDK_WIN32_GL_CONTEXT_WGL (context);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  GdkSurface *surface = gdk_gl_context_get_surface (context);
  HDC hdc;

  if (surfaceless || surface == NULL)
    hdc = display_win32->dummy_context_wgl.hdc;
  else
    hdc = GDK_WIN32_SURFACE (surface)->hdc;

  if (!wglMakeCurrent (hdc, context_wgl->wgl_context))
    return FALSE;

  if (!surfaceless && display_win32->hasWglEXTSwapControl)
    {
      gboolean do_frame_sync = FALSE;

      /* If there is compositing there is no particular need to delay
       * the swap when drawing on the offscreen, rendering to the screen
       * happens later anyway, and its up to the compositor to sync that
       * to the vblank. */
      do_frame_sync = ! gdk_display_is_composited (display);

      if (do_frame_sync != context_wgl->do_frame_sync)
        {
          context_wgl->do_frame_sync = do_frame_sync;

          wglSwapIntervalEXT (do_frame_sync ? 1 : 0);
        }
    }

  return TRUE;
}

static void
gdk_win32_gl_context_wgl_class_init (GdkWin32GLContextWGLClass *klass)
{
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  context_class->backend_type = GDK_GL_WGL;

  context_class->realize = gdk_win32_gl_context_wgl_realize;
  context_class->make_current = gdk_win32_gl_context_wgl_make_current;
  context_class->clear_current = gdk_win32_gl_context_wgl_clear_current;

  draw_context_class->begin_frame = gdk_win32_gl_context_wgl_begin_frame;
  draw_context_class->end_frame = gdk_win32_gl_context_wgl_end_frame;
  gobject_class->dispose = gdk_win32_gl_context_wgl_dispose;
}

static void
gdk_win32_gl_context_wgl_init (GdkWin32GLContextWGL *wgl_context)
{
}


/**
 * gdk_win32_display_get_wgl_version:
 * @display: a `GdkDisplay`
 * @major: (out): return location for the WGL major version
 * @minor: (out): return location for the WGL minor version
 *
 * Retrieves the version of the WGL implementation.
 *
 * Returns: %TRUE if WGL is available
 */
gboolean
gdk_win32_display_get_wgl_version (GdkDisplay *display,
                                   int *major,
                                   int *minor)
{
  GdkWin32Display *display_win32;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  if (!GDK_IS_WIN32_DISPLAY (display))
    return FALSE;

  display_win32 = GDK_WIN32_DISPLAY (display);
  if (display_win32->wgl_pixel_format == 0)
    return FALSE;

  if (major != NULL)
    *major = display_win32->gl_version / 10;
  if (minor != NULL)
    *minor = display_win32->gl_version % 10;

  return TRUE;
}
