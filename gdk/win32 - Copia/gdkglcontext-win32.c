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
#include "gdkwindow-win32.h"
#include "gdkglcontext-win32.h"
#include "gdkdisplay-win32.h"

#include "gdkwin32display.h"
#include "gdkwin32glcontext.h"
#include "gdkwin32misc.h"
#include "gdkwin32screen.h"
#include "gdkwin32window.h"

#include "gdkglcontext.h"
#include "gdkwindow.h"
#include "gdkinternals.h"
#include "gdkintl.h"

#include <cairo.h>
#include <epoxy/wgl.h>

#ifdef GDK_WIN32_ENABLE_EGL
# include <epoxy/egl.h>
#endif

G_DEFINE_TYPE (GdkWin32GLContext, gdk_win32_gl_context, GDK_TYPE_GL_CONTEXT)

static void
_gdk_win32_gl_context_dispose (GObject *gobject)
{
  GdkGLContext *context = GDK_GL_CONTEXT (gobject);
  GdkWin32GLContext *context_win32 = GDK_WIN32_GL_CONTEXT (gobject);
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (gdk_gl_context_get_display (context));
  GdkWindow *window = gdk_gl_context_get_window (context);
  GdkWindowImplWin32 *impl = NULL;

  if (context_win32->hglrc != NULL)
    {
      if (wglGetCurrentContext () == context_win32->hglrc)
        wglMakeCurrent (NULL, NULL);

      GDK_NOTE (OPENGL, g_print ("Destroying WGL context\n"));

      wglDeleteContext (context_win32->hglrc);
      context_win32->hglrc = NULL;

      ReleaseDC (display_win32->gl_hwnd, context_win32->gl_hdc);
    }

#ifdef GDK_WIN32_ENABLE_EGL
  if (context_win32->egl_context != EGL_NO_CONTEXT)
    {
      if (eglGetCurrentContext () == context_win32->egl_context)
        eglMakeCurrent(display_win32->egl_disp, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);

      GDK_NOTE (OPENGL, g_message ("Destroying EGL (ANGLE) context"));

      eglDestroyContext (display_win32->egl_disp,
                         context_win32->egl_context);
      context_win32->egl_context = EGL_NO_CONTEXT;

      impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

      ReleaseDC (display_win32->gl_hwnd, context_win32->gl_hdc);
    }
#endif

  if (window != NULL && window->impl != NULL)
    {
      impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

      if (impl->suppress_layered > 0)
        impl->suppress_layered--;

      /* If we don't have any window that forces layered windows off,
       * trigger update_style_bits() to enable layered windows again
       */
      if (impl->suppress_layered == 0)
        _gdk_win32_window_update_style_bits (window);
    }

  G_OBJECT_CLASS (gdk_win32_gl_context_parent_class)->dispose (gobject);
}

static void
gdk_win32_gl_context_class_init (GdkWin32GLContextClass *klass)
{
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  context_class->end_frame = _gdk_win32_gl_context_end_frame;
  context_class->realize = _gdk_win32_gl_context_realize;

  gobject_class->dispose = _gdk_win32_gl_context_dispose;
}

static void
gdk_win32_gl_context_init (GdkWin32GLContext *self)
{
}

static void
gdk_gl_blit_region (GdkWindow *window, cairo_region_t *region)
{
  int n_rects, i;
  int scale = gdk_window_get_scale_factor (window);
  int wh = gdk_window_get_height (window);
  cairo_rectangle_int_t rect;

  n_rects = cairo_region_num_rectangles (region);
  for (i = 0; i < n_rects; i++)
    {
      cairo_region_get_rectangle (region, i, &rect);
      glScissor (rect.x * scale, (wh - rect.y - rect.height) * scale, rect.width * scale, rect.height * scale);
      glBlitFramebuffer (rect.x * scale, (wh - rect.y - rect.height) * scale, (rect.x + rect.width) * scale, (wh - rect.y) * scale,
                         rect.x * scale, (wh - rect.y - rect.height) * scale, (rect.x + rect.width) * scale, (wh - rect.y) * scale,
                         GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
}

static gboolean
_get_is_egl_force_redraw (GdkWindow *window)
{
  /* We only need to call gdk_window_invalidate_rect () if necessary */
#ifdef GDK_WIN32_ENABLE_EGL
  if (window->gl_paint_context != NULL && gdk_gl_context_get_use_es (window->gl_paint_context))
    {
      GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

      return impl->egl_force_redraw_all;
    }
#endif
  return FALSE;
}

static void
_reset_egl_force_redraw (GdkWindow *window)
{
#ifdef GDK_WIN32_ENABLE_EGL
  if (window->gl_paint_context != NULL && gdk_gl_context_get_use_es (window->gl_paint_context))
    {
      GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

      if (impl->egl_force_redraw_all)
        impl->egl_force_redraw_all = FALSE;
    }
#endif
}

void
_gdk_win32_gl_context_end_frame (GdkGLContext *context,
                                 cairo_region_t *painted,
                                 cairo_region_t *damage)
{
  GdkWin32GLContext *context_win32 = GDK_WIN32_GL_CONTEXT (context);
  GdkWindow *window = gdk_gl_context_get_window (context);
  GdkWin32Display *display = (GDK_WIN32_DISPLAY (gdk_gl_context_get_display (context)));

  gdk_gl_context_make_current (context);

  if (!gdk_gl_context_get_use_es (context))
    {
      gboolean can_wait = display->hasWglOMLSyncControl;

      if (context_win32->do_frame_sync)
        {
          glFinish ();

          if (can_wait)
            {
              gint64 ust, msc, sbc;

              wglGetSyncValuesOML (context_win32->gl_hdc, &ust, &msc, &sbc);
              wglWaitForMscOML (context_win32->gl_hdc,
                                0,
                                2,
                                (msc + 1) % 2,
                                &ust, &msc, &sbc);
            }
        }

      if (context_win32->do_blit_swap)
        {
          glDrawBuffer(GL_FRONT);
          glReadBuffer(GL_BACK);
          gdk_gl_blit_region (window, painted);
          glDrawBuffer(GL_BACK);
          glFlush();

          if (gdk_gl_context_has_frame_terminator (context))
            glFrameTerminatorGREMEDY ();
        }
      else
        SwapBuffers (context_win32->gl_hdc);
    }

#ifdef GDK_WIN32_ENABLE_EGL
  else
    {
      EGLSurface egl_surface = _gdk_win32_window_get_egl_surface (window, context_win32->egl_config, FALSE);
      gboolean force_egl_redraw_all = _get_is_egl_force_redraw (window);

      if (context_win32->do_blit_swap && !force_egl_redraw_all)
        gdk_gl_blit_region (window, painted);
      else if (force_egl_redraw_all)
        {
          GdkRectangle rect = {0, 0, gdk_window_get_width (window), gdk_window_get_height (window)};

          /* We need to do gdk_window_invalidate_rect() so that we don't get glitches after maximizing or
           *  restoring or using aerosnap
           */
          gdk_window_invalidate_rect (window, &rect, TRUE);
          _reset_egl_force_redraw (window);
        }

      eglSwapBuffers (display->egl_disp, egl_surface);
    }
#endif
}

void
_gdk_win32_window_invalidate_for_new_frame (GdkWindow *window,
                                            cairo_region_t *update_area)
{
  cairo_rectangle_int_t window_rect;
  gboolean invalidate_all = FALSE;
  GdkWin32GLContext *context_win32;
  cairo_rectangle_int_t whole_window = { 0, 0, gdk_window_get_width (window), gdk_window_get_height (window) };

  /* Minimal update is ok if we're not drawing with gl */
  if (window->gl_paint_context == NULL)
    return;

  context_win32 = GDK_WIN32_GL_CONTEXT (window->gl_paint_context);
  context_win32->do_blit_swap = FALSE;

  if (gdk_gl_context_has_framebuffer_blit (window->gl_paint_context) &&
      cairo_region_contains_rectangle (update_area, &whole_window) != CAIRO_REGION_OVERLAP_IN)
    {
      context_win32->do_blit_swap = TRUE;
    }
  else
    invalidate_all = TRUE;

  if (invalidate_all)
    {
      window_rect.x = 0;
      window_rect.y = 0;
      window_rect.width = gdk_window_get_width (window);
      window_rect.height = gdk_window_get_height (window);

      /* If nothing else is known, repaint everything so that the back
         buffer is fully up-to-date for the swapbuffer */
      cairo_region_union_rectangle (update_area, &window_rect);
    }
}

typedef struct
{
  ATOM wc_atom;
  HWND hwnd;
  HDC hdc;
  HGLRC hglrc;
  gboolean inited;
} GdkWGLDummy;

static void
_destroy_dummy_gl_context (GdkWGLDummy dummy)
{
  if (dummy.hglrc != NULL)
    {
      wglDeleteContext (dummy.hglrc);
      dummy.hglrc = NULL;
    }
  if (dummy.hdc != NULL)
    {
      DeleteDC (dummy.hdc);
      dummy.hdc = NULL;
    }
  if (dummy.hwnd != NULL)
    {
      DestroyWindow (dummy.hwnd);
      dummy.hwnd = NULL;
    }
  if (dummy.wc_atom != 0)
    {
      UnregisterClass (MAKEINTATOM (dummy.wc_atom), GetModuleHandle (NULL));
      dummy.wc_atom = 0;
    }
  dummy.inited = FALSE;
}

/* Yup, we need to create a dummy window for the dummy WGL context */
static void
_get_dummy_window_hwnd (GdkWGLDummy *dummy)
{
  WNDCLASSEX dummy_wc;

  memset (&dummy_wc, 0, sizeof (WNDCLASSEX));

  dummy_wc.cbSize = sizeof( WNDCLASSEX );
  dummy_wc.style = CS_OWNDC;
  dummy_wc.lpfnWndProc = (WNDPROC) DefWindowProc;
  dummy_wc.cbClsExtra = 0;
  dummy_wc.cbWndExtra = 0;
  dummy_wc.hInstance = GetModuleHandle( NULL );
  dummy_wc.hIcon = 0;
  dummy_wc.hCursor = NULL;
  dummy_wc.hbrBackground = 0;
  dummy_wc.lpszMenuName = 0;
  dummy_wc.lpszClassName = "dummy";
  dummy_wc.hIconSm = 0;

  dummy->wc_atom = RegisterClassEx (&dummy_wc);

  dummy->hwnd =
    CreateWindowEx (WS_EX_APPWINDOW,
                    MAKEINTATOM (dummy->wc_atom),
                    "",
                    WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                    0,
                    0,
                    0,
                    0,
                    NULL,
                    NULL,
                    GetModuleHandle (NULL),
                    NULL);
}

static gint
_gdk_init_dummy_context (GdkWGLDummy    *dummy,
                         const gboolean  need_alpha_bits);

#define PIXEL_ATTRIBUTES 19

static gint
_get_wgl_pfd (HDC                    hdc,
              const gboolean         need_alpha_bits,
              PIXELFORMATDESCRIPTOR *pfd,
              GdkWin32Display       *display)
{
  gint best_pf = 0;

  pfd->nSize = sizeof (PIXELFORMATDESCRIPTOR);

  if (display != NULL && display->hasWglARBPixelFormat)
    {
      GdkWGLDummy dummy;
      UINT num_formats;
      gint colorbits = GetDeviceCaps (hdc, BITSPIXEL);
      guint extra_fields = 1;
      gint i = 0;
      int pixelAttribs[PIXEL_ATTRIBUTES];
      int alpha_idx = 0;

      /* Save up the HDC and HGLRC that we are currently using, to restore back to it when we are done here */
      HDC hdc_current = wglGetCurrentDC ();
      HGLRC hglrc_current = wglGetCurrentContext ();

      if (display->hasWglARBmultisample)
        {
          /* 2 pairs of values needed for multisampling/AA support */
          extra_fields += 2 * 2;
        }

      /* Update PIXEL_ATTRIBUTES above if any groups are added here! */
      /* one group contains a value pair for both pixelAttribs and pixelAttribsNoAlpha */
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

      if (display->hasWglARBmultisample)
        {
          pixelAttribs[i++] = WGL_SAMPLE_BUFFERS_ARB;
          pixelAttribs[i++] = 1;

          pixelAttribs[i++] = WGL_SAMPLES_ARB;
          pixelAttribs[i++] = 8;
        }

      pixelAttribs[i++] = WGL_ALPHA_BITS_ARB;

      /* track the spot where the alpha bits are, so that we can clear it if needed */
      alpha_idx = i;

      pixelAttribs[i++] = 8;
      pixelAttribs[i++] = 0; /* end of pixelAttribs */

      memset (&dummy, 0, sizeof (GdkWGLDummy));

      /* acquire and cache dummy Window (HWND & HDC) and
       * dummy GL Context, we need it for wglChoosePixelFormatARB()
       */
      best_pf = _gdk_init_dummy_context (&dummy, need_alpha_bits);

      if (best_pf == 0 || !wglMakeCurrent (dummy.hdc, dummy.hglrc))
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

      if (best_pf == 0)
        {
          if (!need_alpha_bits)
            {
              pixelAttribs[alpha_idx] = 0;
              pixelAttribs[alpha_idx + 1] = 0;

              /* give another chance if need_alpha_bits is FALSE,
               * meaning we prefer to have an alpha channel anyways
               */
              wglChoosePixelFormatARB (hdc,
                                       pixelAttribs,
                                       NULL,
                                       1,
                                       &best_pf,
                                       &num_formats);

            }
        }

      /* Go back to the HDC that we were using, since we are done with the dummy HDC and GL Context */
      wglMakeCurrent (hdc_current, hglrc_current);
      _destroy_dummy_gl_context (dummy);
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

      if (best_pf == 0)
        /* give another chance if need_alpha_bits is FALSE,
         * meaning we prefer to have an alpha channel anyways
         */
        if (!need_alpha_bits)
          {
            pfd->cAlphaBits = 0;
            best_pf = ChoosePixelFormat (hdc, pfd);
          }
    }

  return best_pf;
}

/* in WGL, for many OpenGL items, we need a dummy WGL context, so create
 * one and cache it for later use
 */
static gint
_gdk_init_dummy_context (GdkWGLDummy    *dummy,
                         const gboolean  need_alpha_bits)
{
  PIXELFORMATDESCRIPTOR pfd;
  gboolean set_pixel_format_result = FALSE;
  gint best_idx = 0;

  _get_dummy_window_hwnd (dummy);

  dummy->hdc = GetDC (dummy->hwnd);
  memset (&pfd, 0, sizeof (PIXELFORMATDESCRIPTOR));

  best_idx = _get_wgl_pfd (dummy->hdc, need_alpha_bits, &pfd, NULL);

  if (best_idx != 0)
    set_pixel_format_result = SetPixelFormat (dummy->hdc,
                                              best_idx,
                                              &pfd);

  if (best_idx == 0 || !set_pixel_format_result)
    return 0;

  dummy->hglrc = wglCreateContext (dummy->hdc);
  if (dummy->hglrc == NULL)
    return 0;

  dummy->inited = TRUE;

  return best_idx;
}

#ifdef GDK_WIN32_ENABLE_EGL

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
_gdk_win32_get_egl_display (GdkWin32Display *display)
{
  EGLDisplay disp;
  gboolean success = FALSE;

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
#endif

static gboolean
_gdk_win32_display_init_gl (GdkDisplay *display,
                            const gboolean need_alpha_bits)
{
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  gint best_idx = 0;
  gboolean disable_wgl = FALSE;

#ifdef GDK_WIN32_ENABLE_EGL
  EGLDisplay egl_disp;
  disable_wgl = (_gdk_gl_flags & GDK_GL_GLES) != 0;
#endif

  if (display_win32->have_wgl
#ifdef GDK_WIN32_ENABLE_EGL
      || display_win32->have_egl
#endif
     )
    return TRUE;

  if (!disable_wgl)
    {
      /* acquire and cache dummy Window (HWND & HDC) and
       * dummy GL Context, it is used to query functions
       * and used for other stuff as well
       */
      GdkWGLDummy dummy;
      memset (&dummy, 0, sizeof (GdkWGLDummy));

      best_idx = _gdk_init_dummy_context (&dummy, need_alpha_bits);

      if (best_idx == 0 || !wglMakeCurrent (dummy.hdc, dummy.hglrc))
        return FALSE;

      display_win32->have_wgl = TRUE;
      display_win32->gl_version = epoxy_gl_version ();

      display_win32->hasWglARBCreateContext =
        epoxy_has_wgl_extension (dummy.hdc, "WGL_ARB_create_context");
      display_win32->hasWglEXTSwapControl =
        epoxy_has_wgl_extension (dummy.hdc, "WGL_EXT_swap_control");
      display_win32->hasWglOMLSyncControl =
        epoxy_has_wgl_extension (dummy.hdc, "WGL_OML_sync_control");
      display_win32->hasWglARBPixelFormat =
        epoxy_has_wgl_extension (dummy.hdc, "WGL_ARB_pixel_format");
      display_win32->hasWglARBmultisample =
        epoxy_has_wgl_extension (dummy.hdc, "WGL_ARB_multisample");

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

      _destroy_dummy_gl_context (dummy);
      return TRUE;
    }

#ifdef GDK_WIN32_ENABLE_EGL
  egl_disp = _gdk_win32_get_egl_display (display_win32);

  if (egl_disp == EGL_NO_DISPLAY ||
      !eglInitialize (egl_disp, NULL, NULL))
    {
      if (egl_disp != EGL_NO_DISPLAY)
        {
          eglTerminate (egl_disp);
          egl_disp = EGL_NO_DISPLAY;
        }

      return FALSE;
    }

  display_win32->egl_disp = egl_disp;
  display_win32->have_egl = TRUE;
  display_win32->egl_version = epoxy_egl_version (egl_disp);

  eglBindAPI(EGL_OPENGL_ES_API);

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

  return TRUE;
#endif
}

/* Setup the legacy context after creating it */
static gboolean
_ensure_legacy_gl_context (HDC           hdc,
                           HGLRC         hglrc_legacy,
                           GdkGLContext *share)
{
  GdkWin32GLContext *context_win32;

  if (!wglMakeCurrent (hdc, hglrc_legacy))
    return FALSE;

  if (share != NULL)
    {
      context_win32 = GDK_WIN32_GL_CONTEXT (share);

      return wglShareLists (hglrc_legacy, context_win32->hglrc);
    }

  return TRUE;
}

static HGLRC
_create_gl_context_with_attribs (HDC           hdc,
                                 HGLRC         hglrc_base,
                                 GdkGLContext *share,
                                 int           flags,
                                 int           major,
                                 int           minor,
                                 gboolean     *is_legacy)
{
  HGLRC hglrc;
  GdkWin32GLContext *context_win32;

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
    context_win32 = GDK_WIN32_GL_CONTEXT (share);

  hglrc = wglCreateContextAttribsARB (hdc,
                                      share != NULL ? context_win32->hglrc : NULL,
                                      attribs);

  return hglrc;
}

static HGLRC
_create_gl_context (HDC           hdc,
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
      if (_ensure_legacy_gl_context (hdc, hglrc_base, share))
        {
          wglMakeCurrent (hdc_current, hglrc_current);
          return hglrc_base;
        }

      success = FALSE;
      goto gl_fail;
    }
  else
    {
      HGLRC hglrc;

      if (!wglMakeCurrent (hdc, hglrc_base))
        {
          success = FALSE;
          goto gl_fail;
        }

      hglrc = _create_gl_context_with_attribs (hdc,
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
              hglrc = _create_gl_context_with_attribs (hdc,
                                                       hglrc_base,
                                                       share,
                                                       flags,
                                                       0, 0,
                                                       is_legacy);

              *is_legacy = TRUE;
            }

          if (hglrc == NULL)
            {
              if (!_ensure_legacy_gl_context (hdc, hglrc_base, share))
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
_set_pixformat_for_hdc (HDC              hdc,
                        gint            *best_idx,
                        const gboolean   need_alpha_bits,
                        GdkWin32Display *display)
{
  PIXELFORMATDESCRIPTOR pfd;
  gboolean set_pixel_format_result = FALSE;

  /* one is only allowed to call SetPixelFormat(), and so ChoosePixelFormat()
   * one single time per window HDC
   */
  *best_idx = _get_wgl_pfd (hdc, need_alpha_bits, &pfd, display);
  if (*best_idx != 0)
    set_pixel_format_result = SetPixelFormat (hdc, *best_idx, &pfd);

  /* ChoosePixelFormat() or SetPixelFormat() failed, bail out */
  if (*best_idx == 0 || !set_pixel_format_result)
    return FALSE;

  return TRUE;
}

#ifdef GDK_WIN32_ENABLE_EGL

#define MAX_EGL_ATTRS   30

static gboolean
find_eglconfig_for_window (GdkWin32Display  *display,
                           EGLConfig        *egl_config_out,
                           EGLint           *min_swap_interval_out,
                           gboolean          need_alpha_bits,
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

  if (need_alpha_bits)
    {
      attrs[i++] = EGL_ALPHA_SIZE;
      attrs[i++] = 1;
    }
  else
    {
      attrs[i++] = EGL_ALPHA_SIZE;
      attrs[i++] = EGL_DONT_CARE;
    }

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

#define N_EGL_ATTRS     16

static EGLContext
_create_egl_context (EGLDisplay    display,
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
                          share != NULL ? GDK_WIN32_GL_CONTEXT (share)->egl_context
                                        : EGL_NO_CONTEXT,
                          context_attribs);

  if (ctx != EGL_NO_CONTEXT)
    GDK_NOTE (OPENGL, g_message ("Created EGL context[%p]", ctx));

  return ctx;
}
#endif /* GDK_WIN32_ENABLE_EGL */

gboolean
_gdk_win32_gl_context_realize (GdkGLContext *context,
                               GError **error)
{
  GdkGLContext *share = gdk_gl_context_get_shared_context (context);
  GdkWin32GLContext *context_win32 = GDK_WIN32_GL_CONTEXT (context);

  /* These are the real WGL/EGL context items that we will want to use later */
  gboolean debug_bit, compat_bit, legacy_bit;

  /* These are the real WGL context items that we will want to use later */
  HGLRC hglrc;
  gint pixel_format;
  gboolean use_es = FALSE;

  /* request flags and specific versions for core (3.2+) WGL context */
  gint flags = 0;
  gint major = 0;
  gint minor = 0;

#ifdef GDK_WIN32_ENABLE_EGL
  EGLContext egl_context;
#endif

  GdkWindow *window = gdk_gl_context_get_window (context);
  GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (window->impl);
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (gdk_window_get_display (window));

  /*
   * A legacy context cannot be shared with core profile ones, so this means we
   * must stick to a legacy context if the shared context is a legacy context
   */
  if ((_gdk_gl_flags & GDK_GL_LEGACY) != 0 ||
       share != NULL && gdk_gl_context_is_legacy (share))
    legacy_bit = TRUE;

  if ((_gdk_gl_flags & GDK_GL_GLES) != 0 ||
      (share != NULL && gdk_gl_context_get_use_es (share)))
    use_es = TRUE;

  gdk_gl_context_get_required_version (context, &major, &minor);
  debug_bit = gdk_gl_context_get_debug_enabled (context);
  compat_bit = gdk_gl_context_get_forward_compatible (context);

  if (win32_display->have_wgl)
    {
      if (!_set_pixformat_for_hdc (context_win32->gl_hdc,
                                   &pixel_format,
                                   context_win32->need_alpha_bits,
                                   win32_display))
        {
          g_set_error_literal (error, GDK_GL_ERROR,
                               GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                               _("No available configurations for the given pixel format"));

          return FALSE;
        }

      /* if there isn't wglCreateContextAttribsARB(), or if GDK_GL_LEGACY is set, we default to a legacy context */
      legacy_bit = !win32_display->hasWglARBCreateContext ||
                   g_getenv ("GDK_GL_LEGACY") != NULL;

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

      hglrc = _create_gl_context (context_win32->gl_hdc,
                                  share,
                                  flags,
                                  major,
                                  minor,
                                  &legacy_bit,
                                  win32_display->hasWglARBCreateContext);

      if (hglrc == NULL)
        {
          g_set_error_literal (error, GDK_GL_ERROR,
                               GDK_GL_ERROR_NOT_AVAILABLE,
                               _("Unable to create a GL context"));
          return FALSE;
        }

      GDK_NOTE (OPENGL,
                g_print ("Created WGL context[%p], pixel_format=%d\n",
                         hglrc,
                         pixel_format));

      context_win32->hglrc = hglrc;
    }

#ifdef GDK_WIN32_ENABLE_EGL
  if (win32_display->have_egl)
    {
      EGLContext ctx;

      if (debug_bit)
        flags |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
      if (compat_bit)
        flags |= EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;

      GDK_NOTE (OPENGL, g_message ("Creating EGL context version %d.%d (debug:%s, forward:%s, legacy:%s)",
                                   major, minor,
                                   debug_bit ? "yes" : "no",
                                   compat_bit ? "yes" : "no",
                                   legacy_bit ? "yes" : "no"));

      ctx = _create_egl_context (win32_display->egl_disp,
                                 context_win32->egl_config,
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

      GDK_NOTE (OPENGL,
                g_print ("Created EGL context[%p]\n",
                         ctx));

      context_win32->egl_context = ctx;
      use_es = TRUE;
    }
#endif

  /* set whether we are using GLES */
  gdk_gl_context_set_use_es(context, use_es);

  /* OpenGL does not work with WS_EX_LAYERED enabled, so we need to
   * disable WS_EX_LAYERED when we acquire a valid HGLRC
   */
  impl->suppress_layered++;

  /* if this is the first time a GL context is acquired for the window,
   * disable layered windows by triggering update_style_bits()
   */
  if (impl->suppress_layered == 1)
    _gdk_win32_window_update_style_bits (window);

  /* Ensure that any other context is created with a legacy bit set */
  gdk_gl_context_set_is_legacy (context, legacy_bit);

  return TRUE;
}

GdkGLContext *
_gdk_win32_window_create_gl_context (GdkWindow *window,
                                     gboolean attached,
                                     GdkGLContext *share,
                                     GError **error)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (gdk_window_get_display (window));
  GdkWin32GLContext *context = NULL;
  GdkVisual *visual = gdk_window_get_visual (window);

  gboolean need_alpha_bits = (visual == gdk_screen_get_rgba_visual (gdk_display_get_default_screen (display)));

  /* Acquire and store up the Windows-specific HWND and HDC */
  HDC hdc;

#ifdef GDK_WIN32_ENABLE_EGL
  EGLContext egl_context;
  EGLConfig config;
#endif

  display_win32->gl_hwnd = GDK_WINDOW_HWND (window);
  hdc = GetDC (display_win32->gl_hwnd);

#ifdef GDK_WIN32_ENABLE_EGL
  /* display_win32->hdc_egl_temp should *not* be destroyed here!  It is destroyed at dispose()! */
  display_win32->hdc_egl_temp = hdc;
#endif

  if (!_gdk_win32_display_init_gl (display, need_alpha_bits))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL implementation is available"));
      return NULL;
    }

#ifdef GDK_WIN32_ENABLE_EGL
  if (display_win32->have_egl && !find_eglconfig_for_window (display_win32, &config,
                                  &display_win32->egl_min_swap_interval, need_alpha_bits,
                                  error))
    return NULL;
#endif

  context = g_object_new (GDK_TYPE_WIN32_GL_CONTEXT,
                          "display", display,
                          "window", window,
                          "shared-context", share,
                          NULL);

  context->need_alpha_bits = need_alpha_bits;
  context->gl_hdc = hdc;

#ifdef GDK_WIN32_ENABLE_EGL
  if (display_win32->have_egl)
    context->egl_config = config;
#endif

  context->is_attached = attached;

  return GDK_GL_CONTEXT (context);
}

gboolean
_gdk_win32_display_make_gl_context_current (GdkDisplay *display,
                                            GdkGLContext *context)
{
  GdkWin32GLContext *context_win32;
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  GdkWindow *window;
  GdkScreen *screen;

#if GDK_WIN32_ENABLE_EGL
  EGLSurface egl_surface;
#endif

  gboolean do_frame_sync = FALSE;

  if (context == NULL)
    {
      if (display_win32->have_wgl)
        wglMakeCurrent(NULL, NULL);

#ifdef GDK_WIN32_ENABLE_EGL
      else if (display_win32->have_egl)
        eglMakeCurrent (display_win32->egl_disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
#endif
      return TRUE;
    }

  context_win32 = GDK_WIN32_GL_CONTEXT (context);
  window = gdk_gl_context_get_window (context);

  if (!gdk_gl_context_get_use_es (context))
    {
      if (!wglMakeCurrent (context_win32->gl_hdc, context_win32->hglrc))
        {
          GDK_NOTE (OPENGL,
                    g_print ("Making WGL context current failed\n"));
          return FALSE;
        }

      if (context_win32->is_attached)
        {
          if (display_win32->hasWglEXTSwapControl)
            {
              /* If there is compositing there is no particular need to delay
               * the swap when drawing on the offscreen, rendering to the screen
               * happens later anyway, and its up to the compositor to sync that
               * to the vblank. */
              screen = gdk_window_get_screen (window);
              do_frame_sync = ! gdk_screen_is_composited (screen);

              if (do_frame_sync != context_win32->do_frame_sync)
                {
                  context_win32->do_frame_sync = do_frame_sync;

                  if (do_frame_sync)
                    wglSwapIntervalEXT (1);
                  else
                    wglSwapIntervalEXT (0);
                }
            }
        }
    }

#ifdef GDK_WIN32_ENABLE_EGL
  else
    {
      if (context_win32->is_attached)
        egl_surface = _gdk_win32_window_get_egl_surface (window, context_win32->egl_config, FALSE);
      else
        {
          if (display_win32->hasEglSurfacelessContext)
            egl_surface = EGL_NO_SURFACE;
          else
            egl_surface = _gdk_win32_window_get_egl_surface (window, context_win32->egl_config, TRUE);
        }

      if (!eglMakeCurrent (display_win32->egl_disp,
                           egl_surface,
                           egl_surface,
                           context_win32->egl_context))
        {
          g_warning ("eglMakeCurrent failed");
          return FALSE;
        }

      if (display_win32->egl_min_swap_interval == 0)
        eglSwapInterval (display_win32->egl_disp, 0);
      else
        g_debug ("Can't disable GL swap interval");
    }
#endif

  return TRUE;
}

/**
 * gdk_win32_display_get_wgl_version:
 * @display: a #GdkDisplay
 * @major: (out): return location for the WGL major version
 * @minor: (out): return location for the WGL minor version
 *
 * Retrieves the version of the WGL implementation.
 *
 * Returns: %TRUE if WGL is available
 *
 * Since: 3.16
 */
gboolean
gdk_win32_display_get_wgl_version (GdkDisplay *display,
                                   gint *major,
                                   gint *minor)
{
  GdkWin32Display *display_win32 = NULL;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  if (!GDK_IS_WIN32_DISPLAY (display))
    return FALSE;

  display_win32 = GDK_WIN32_DISPLAY (display);

  if (!_gdk_win32_display_init_gl (display, FALSE) || !display_win32->have_wgl
#ifdef GDK_WIN32_ENABLE_EGL
      || !display_win32->have_egl
#endif
     )
    return FALSE;

  if (display_win32->have_wgl)
    {
      if (major != NULL)
        *major = GDK_WIN32_DISPLAY (display)->gl_version / 10;
      if (minor != NULL)
        *minor = GDK_WIN32_DISPLAY (display)->gl_version % 10;
    }

#ifdef GDK_WIN32_ENABLE_EGL
  else if (display_win32->have_egl)
    {
      if (major != NULL)
        *major = GDK_WIN32_DISPLAY (display)->egl_version / 10;
      if (minor != NULL)
        *minor = GDK_WIN32_DISPLAY (display)->egl_version % 10;
    }
#endif

  return TRUE;
}

void
_gdk_win32_window_invalidate_egl_framebuffer (GdkWindow *window)
{
/* If we are using ANGLE, we need to force redraw of the whole Window and its child windows
 *  as we need to re-acquire the EGL surfaces that we rendered to upload to Cairo explicitly,
 *  using gdk_window_invalidate_rect (), when we maximize or restore or use aerosnap
 */
#ifdef GDK_WIN32_ENABLE_EGL
  if (window->gl_paint_context != NULL && gdk_gl_context_get_use_es (window->gl_paint_context))
    {
      GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

      impl->egl_force_redraw_all = TRUE;
    }
#endif
}
