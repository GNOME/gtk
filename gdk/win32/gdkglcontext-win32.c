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
  GdkSurface *surface = gdk_gl_context_get_surface (context);

  if (display_win32 != NULL)
    {
      if (display_win32->have_wgl)
        {
          if (wglGetCurrentContext () == context_win32->hglrc)
            wglMakeCurrent (NULL, NULL);

          GDK_NOTE (OPENGL, g_print ("Destroying WGL context\n"));

          wglDeleteContext (context_win32->hglrc);
          context_win32->hglrc = NULL;

          ReleaseDC (display_win32->gl_hwnd, context_win32->gl_hdc);
        }
 
#ifdef GDK_WIN32_ENABLE_EGL
      else if (display_win32->have_egl)
        {
          if (eglGetCurrentContext () == context_win32->egl_context)
            eglMakeCurrent(display_win32->egl_disp, EGL_NO_SURFACE, EGL_NO_SURFACE,
                           EGL_NO_CONTEXT);

          GDK_NOTE (OPENGL, g_message ("Destroying EGL (ANGLE) context"));

          eglDestroyContext (display_win32->egl_disp,
                             context_win32->egl_context);
          context_win32->egl_context = EGL_NO_CONTEXT;

          ReleaseDC (display_win32->gl_hwnd, context_win32->gl_hdc);
        }
#endif
    }

  G_OBJECT_CLASS (gdk_win32_gl_context_parent_class)->dispose (gobject);
}

#ifdef GDK_WIN32_ENABLE_EGL
static gboolean
_get_is_egl_force_redraw (GdkSurface *surface)
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
_reset_egl_force_redraw (GdkSurface *surface)
{
  if (surface->gl_paint_context != NULL && gdk_gl_context_get_use_es (surface->gl_paint_context))
    {
      GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

      if (impl->egl_force_redraw_all)
        impl->egl_force_redraw_all = FALSE;
    }
}
#endif

static void
gdk_win32_gl_context_end_frame (GdkDrawContext *draw_context,
                                cairo_region_t *painted)
{
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);
  GdkWin32GLContext *context_win32 = GDK_WIN32_GL_CONTEXT (context);
  GdkSurface *surface = gdk_gl_context_get_surface (context);
  GdkWin32Display *display = (GDK_WIN32_DISPLAY (gdk_gl_context_get_display (context)));
  cairo_rectangle_int_t whole_window;

  GDK_DRAW_CONTEXT_CLASS (gdk_win32_gl_context_parent_class)->end_frame (draw_context, painted);
  if (gdk_gl_context_get_shared_context (context))
    return;

  gdk_gl_context_make_current (context);
  whole_window = (GdkRectangle) { 0, 0, gdk_surface_get_width (surface), gdk_surface_get_height (surface) };

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

      SwapBuffers (context_win32->gl_hdc);
    }
#ifdef GDK_WIN32_ENABLE_EGL
  else
    {
      EGLSurface egl_surface = _gdk_win32_surface_get_egl_surface (surface, context_win32->egl_config, FALSE);
      gboolean force_egl_redraw_all = _get_is_egl_force_redraw (surface);

	  if (force_egl_redraw_all)
        {
          GdkRectangle rect = {0, 0, gdk_surface_get_width (surface), gdk_surface_get_height (surface)};

          /* We need to do gdk_window_invalidate_rect() so that we don't get glitches after maximizing or
           *  restoring or using aerosnap
           */
          gdk_surface_invalidate_rect (surface, &rect);
          _reset_egl_force_redraw (surface);
        }

      eglSwapBuffers (display->egl_disp, egl_surface);
    }
#endif
}

static void
gdk_win32_gl_context_begin_frame (GdkDrawContext *draw_context,
                                  cairo_region_t *update_area)
{
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);
  GdkSurface *surface;

  surface = gdk_gl_context_get_surface (context);

  gdk_win32_surface_handle_queued_move_resize (draw_context);

  GDK_DRAW_CONTEXT_CLASS (gdk_win32_gl_context_parent_class)->begin_frame (draw_context, update_area);
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

static int
_gdk_init_dummy_context (GdkWGLDummy *dummy);

#define PIXEL_ATTRIBUTES 17

static int
_get_wgl_pfd (HDC                    hdc,
              PIXELFORMATDESCRIPTOR *pfd,
              GdkWin32Display       *display)
{
  int best_pf = 0;

  pfd->nSize = sizeof (PIXELFORMATDESCRIPTOR);

  if (display != NULL && display->hasWglARBPixelFormat)
    {
      GdkWGLDummy dummy;
      UINT num_formats;
      int colorbits = GetDeviceCaps (hdc, BITSPIXEL);
      guint extra_fields = 1;
      int i = 0;
      int pixelAttribs[PIXEL_ATTRIBUTES];

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

      pixelAttribs[i++] = 0; /* end of pixelAttribs */

      memset (&dummy, 0, sizeof (GdkWGLDummy));

      /* acquire and cache dummy Window (HWND & HDC) and
       * dummy GL Context, we need it for wglChoosePixelFormatARB()
       */
      best_pf = _gdk_init_dummy_context (&dummy);

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
    }

  return best_pf;
}

/* in WGL, for many OpenGL items, we need a dummy WGL context, so create
 * one and cache it for later use
 */
static int
_gdk_init_dummy_context (GdkWGLDummy *dummy)
{
  PIXELFORMATDESCRIPTOR pfd;
  gboolean set_pixel_format_result = FALSE;
  int best_idx = 0;

  _get_dummy_window_hwnd (dummy);

  dummy->hdc = GetDC (dummy->hwnd);
  memset (&pfd, 0, sizeof (PIXELFORMATDESCRIPTOR));

  best_idx = _get_wgl_pfd (dummy->hdc, &pfd, NULL);

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
_gdk_win32_display_init_gl (GdkDisplay *display)
{
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  int best_idx = 0;

  gboolean disable_wgl = FALSE;

#ifdef GDK_WIN32_ENABLE_EGL
  EGLDisplay egl_disp;

  disable_wgl = GDK_DISPLAY_DEBUG_CHECK (display, GL_GLES) ||
                display_win32->running_on_arm64;
#endif

  if (display_win32->have_wgl
#ifdef GDK_WIN32_ENABLE_EGL
      || display_win32->have_egl
#endif
     )
    return TRUE;

  if (!disable_wgl)
    {
      GdkWGLDummy dummy;
      memset (&dummy, 0, sizeof (GdkWGLDummy));

      /* acquire and cache dummy Window (HWND & HDC) and
       * dummy GL Context, it is used to query functions
       * and used for other stuff as well
       */
      best_idx = _gdk_init_dummy_context (&dummy);

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
    }
#ifdef GDK_WIN32_ENABLE_EGL
  else
    {
      egl_disp = _gdk_win32_get_egl_display (display_win32);

      if (egl_disp == EGL_NO_DISPLAY || !eglInitialize (egl_disp, NULL, NULL))
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
    }
#endif

  return TRUE;
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
                        int             *best_idx,
                        GdkWin32Display *display)
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
      *best_idx = _get_wgl_pfd (hdc, &pfd, display);

      if (*best_idx != 0)
        set_pixel_format_result = SetPixelFormat (hdc, *best_idx, &pfd);

      /* ChoosePixelFormat() or SetPixelFormat() failed, bail out */
      if (*best_idx == 0 || !set_pixel_format_result)
        return FALSE;
    }

  GDK_NOTE (OPENGL, g_print ("%s""requested and set pixel format: %d\n", already_checked ? "already " : "", *best_idx));

  return TRUE;
}

#ifdef GDK_WIN32_ENABLE_EGL

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

static gboolean
gdk_win32_gl_context_realize (GdkGLContext *context,
                              GError **error)
{
  GdkGLContext *share = gdk_gl_context_get_shared_context (context);
  GdkGLContext *shared_data_context;
  GdkWin32GLContext *context_win32 = GDK_WIN32_GL_CONTEXT (context);

  gboolean debug_bit, compat_bit, legacy_bit;
  gboolean use_es = FALSE;

  /* request flags and specific versions for core (3.2+) WGL context */
  int flags = 0;
  int major = 0;
  int minor = 0;

  GdkSurface *surface = gdk_gl_context_get_surface (context);
  GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (display);

  gdk_gl_context_get_required_version (context, &major, &minor);
  debug_bit = gdk_gl_context_get_debug_enabled (context);
  compat_bit = gdk_gl_context_get_forward_compatible (context);
  shared_data_context = gdk_surface_get_shared_data_gl_context (surface);

  /*
   * A legacy context cannot be shared with core profile ones, so this means we
   * must stick to a legacy context if the shared context is a legacy context
   */
  if (share != NULL && gdk_gl_context_is_legacy (share))
    legacy_bit = TRUE;

  /* if GDK_GL_LEGACY is set, we default to a legacy context */
  legacy_bit = g_getenv ("GDK_GL_LEGACY") != NULL;

  use_es = GDK_DISPLAY_DEBUG_CHECK (display, GL_GLES) ||
    (share != NULL && gdk_gl_context_get_use_es (share));

  if (win32_display->have_wgl || !use_es)
    {
      /* These are the real WGL context items that we will want to use later */
      HGLRC hglrc;
      int pixel_format;

      if (!_set_pixformat_for_hdc (context_win32->gl_hdc,
                                   &pixel_format,
                                   win32_display))
        {
          g_set_error_literal (error, GDK_GL_ERROR,
                               GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                               _("No available configurations for the given pixel format"));

          return FALSE;
        }

      /* if there isn't wglCreateContextAttribsARB() on WGL, use a legacy context */
      if (!legacy_bit)
        legacy_bit = !win32_display->hasWglARBCreateContext;
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
                                  share ? share : shared_data_context,
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
  else
    {
      EGLContext egl_context;
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
                                 share ? share : shared_data_context,
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
  gdk_gl_context_set_use_es (context, use_es);

  /* Ensure that any other context is created with a legacy bit set */
  gdk_gl_context_set_is_legacy (context, legacy_bit);

  return TRUE;
}

static void
gdk_win32_gl_context_class_init (GdkWin32GLContextClass *klass)
{
  GdkGLContextClass *gl_context_class = GDK_GL_CONTEXT_CLASS(klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS(klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gl_context_class->realize = gdk_win32_gl_context_realize;

  draw_context_class->begin_frame = gdk_win32_gl_context_begin_frame;
  draw_context_class->end_frame = gdk_win32_gl_context_end_frame;

  gobject_class->dispose = _gdk_win32_gl_context_dispose;
}

static void
gdk_win32_gl_context_init (GdkWin32GLContext *self)
{
}

GdkGLContext *
_gdk_win32_surface_create_gl_context (GdkSurface *surface,
                                     gboolean attached,
                                     GdkGLContext *share,
                                     GError **error)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  GdkWin32GLContext *context = NULL;

  /* Acquire and store up the Windows-specific HWND and HDC */
/*  HWND hwnd;*/
  HDC hdc;

#ifdef GDK_WIN32_ENABLE_EGL
  EGLContext egl_context;
  EGLConfig config;
#endif

  display_win32->gl_hwnd = GDK_SURFACE_HWND (surface);
  hdc = GetDC (display_win32->gl_hwnd);

#ifdef GDK_WIN32_ENABLE_EGL
  /* display_win32->hdc_egl_temp should *not* be destroyed here!  It is destroyed at dispose()! */
  display_win32->hdc_egl_temp = hdc;
#endif

  if (!_gdk_win32_display_init_gl (display))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL implementation is available"));
      return NULL;
    }

#if 0
  if (display_win32->have_wgl)
    {
      hwnd = GDK_SURFACE_HWND (surface);
      hdc = GetDC (hwnd);

      display_win32->gl_hwnd = hwnd;
    }
#endif

#ifdef GDK_WIN32_ENABLE_EGL
  if (display_win32->have_egl && !find_eglconfig_for_window (display_win32, &config,
                                  &display_win32->egl_min_swap_interval, error))
    return NULL;
#endif

  context = g_object_new (GDK_TYPE_WIN32_GL_CONTEXT,
                          "surface", surface,
                          "shared-context", share,
                          NULL);

  context->gl_hdc = hdc;
  context->is_attached = attached;

#ifdef GDK_WIN32_ENABLE_EGL
  if (display_win32->have_egl)
    context->egl_config = config;
#endif

  return GDK_GL_CONTEXT (context);
}

gboolean
_gdk_win32_display_make_gl_context_current (GdkDisplay *display,
                                            GdkGLContext *context)
{
  GdkWin32GLContext *context_win32;
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  GdkSurface *surface;

  gboolean do_frame_sync = FALSE;

  if (context == NULL)
    {
#ifdef GDK_WIN32_ENABLE_EGL
      if (display_win32->egl_disp != EGL_NO_DISPLAY)
        eglMakeCurrent(display_win32->egl_disp,
                       EGL_NO_SURFACE, 
                       EGL_NO_SURFACE, 
                       EGL_NO_CONTEXT);
      else
#endif
        wglMakeCurrent(NULL, NULL);

      return TRUE;
    }

  context_win32 = GDK_WIN32_GL_CONTEXT (context);

  if (!gdk_gl_context_get_use_es (context))
    {
      if (!wglMakeCurrent (context_win32->gl_hdc, context_win32->hglrc))
        {
          GDK_NOTE (OPENGL,
                    g_print ("Making WGL context current failed\n"));
          return FALSE;
        }

      if (context_win32->is_attached && display_win32->hasWglEXTSwapControl)
        {
          surface = gdk_gl_context_get_surface (context);

          /* If there is compositing there is no particular need to delay
           * the swap when drawing on the offscreen, rendering to the screen
           * happens later anyway, and its up to the compositor to sync that
           * to the vblank. */
          display = gdk_surface_get_display (surface);
          do_frame_sync = ! gdk_display_is_composited (display);

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

#ifdef GDK_WIN32_ENABLE_EGL
  else
    {
      EGLSurface egl_surface;

      surface = gdk_gl_context_get_surface (context);

      if (context_win32->is_attached)
        egl_surface = _gdk_win32_surface_get_egl_surface (surface, context_win32->egl_config, FALSE);
      else
        {
          if (display_win32->hasEglSurfacelessContext)
            egl_surface = EGL_NO_SURFACE;
          else
            egl_surface = _gdk_win32_surface_get_egl_surface (surface, context_win32->egl_config, TRUE);
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
 */
gboolean
gdk_win32_display_get_wgl_version (GdkDisplay *display,
                                   int *major,
                                   int *minor)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  if (!GDK_IS_WIN32_DISPLAY (display))
    return FALSE;

  if (!_gdk_win32_display_init_gl (display))
    return FALSE;

  if (major != NULL)
    *major = GDK_WIN32_DISPLAY (display)->gl_version / 10;
  if (minor != NULL)
    *minor = GDK_WIN32_DISPLAY (display)->gl_version % 10;

  return TRUE;
}

void
_gdk_win32_surface_invalidate_egl_framebuffer (GdkSurface *surface)
{
/* If we are using ANGLE, we need to force redraw of the whole Window and its child windows
 *  as we need to re-acquire the EGL surfaces that we rendered to upload to Cairo explicitly,
 *  using gdk_window_invalidate_rect (), when we maximize or restore or use aerosnap
 */
#ifdef GDK_WIN32_ENABLE_EGL
  if (surface->gl_paint_context != NULL && gdk_gl_context_get_use_es (surface->gl_paint_context))
    {
      GdkWin32Surface *impl = GDK_WIN32_SURFACE (surface);

      impl->egl_force_redraw_all = TRUE;
    }
#endif
}
