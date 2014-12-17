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

G_DEFINE_TYPE (GdkWin32GLContext, gdk_win32_gl_context, GDK_TYPE_GL_CONTEXT)

static void
_gdk_win32_gl_context_dispose (GObject *gobject)
{
  GdkGLContext *context = GDK_GL_CONTEXT (gobject);
  GdkWin32GLContext *context_win32 = GDK_WIN32_GL_CONTEXT (gobject);
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (gdk_gl_context_get_display (context));

  if (context_win32->hglrc != NULL)
    {
      if (wglGetCurrentContext () == context_win32->hglrc)
        wglMakeCurrent (NULL, NULL);

      GDK_NOTE (OPENGL, g_print ("Destroying WGL context\n"));

      wglDeleteContext (context_win32->hglrc);
      context_win32->hglrc = NULL;

      ReleaseDC (display_win32->gl_hwnd, context_win32->gl_hdc);
    }

  G_OBJECT_CLASS (gdk_win32_gl_context_parent_class)->dispose (gobject);
}

static void
gdk_win32_gl_context_class_init (GdkWin32GLContextClass *klass)
{
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  context_class->end_frame = _gdk_win32_gl_context_end_frame;
  context_class->upload_texture = _gdk_win32_gl_context_upload_texture;

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

void
_gdk_win32_gl_context_end_frame (GdkGLContext *context,
                                 cairo_region_t *painted,
                                 cairo_region_t *damage)
{
  GdkWin32GLContext *context_win32 = GDK_WIN32_GL_CONTEXT (context);
  GdkWindow *window = gdk_gl_context_get_window (context);
  GdkWin32Display *display = (GDK_WIN32_DISPLAY (gdk_gl_context_get_display (context)));

  gboolean can_wait = display->hasWglOMLSyncControl;
  gdk_gl_context_make_current (context);

  if (context_win32->do_frame_sync)
    {
      guint32 end_frame_counter = 0;

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

void
_gdk_win32_window_invalidate_for_new_frame (GdkWindow *window,
                                            cairo_region_t *update_area)
{
  cairo_rectangle_int_t window_rect;
  unsigned int buffer_age;
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

void
_gdk_win32_gl_context_upload_texture (GdkGLContext *context,
                                      cairo_surface_t *image_surface,
                                      int width,
                                      int height,
                                      guint texture_target)
{
  g_return_if_fail (GDK_WIN32_IS_GL_CONTEXT (context));

  glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
  glPixelStorei (GL_UNPACK_ROW_LENGTH, cairo_image_surface_get_stride (image_surface)/4);
  glTexImage2D (texture_target, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE,
                cairo_image_surface_get_data (image_surface));
  glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
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
      if (wglGetCurrentContext () == dummy.hglrc)
        wglMakeCurrent (NULL, NULL);
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
  HWND dummy_hwnd;

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
_get_wgl_pfd (HDC hdc,
              const gboolean need_alpha_bits,
              PIXELFORMATDESCRIPTOR *pfd)
{
  gint configs;
  gint i;
  gint best_pf = 0;
  gboolean alpha_check;

  pfd->nSize = sizeof (PIXELFORMATDESCRIPTOR);
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

  return best_pf;
}

/* in WGL, for many OpenGL items, we need a dummy WGL context, so create
 * one and cache it for later use
 */
static gint
_gdk_init_dummy_context (GdkWGLDummy *dummy,
                         const gboolean need_alpha_bits)
{
  PIXELFORMATDESCRIPTOR pfd;
  gboolean set_pixel_format_result = FALSE;
  gint best_idx = 0;

  _get_dummy_window_hwnd (dummy);

  dummy->hdc = GetDC (dummy->hwnd);
  memset (&pfd, 0, sizeof (PIXELFORMATDESCRIPTOR));

  best_idx = _get_wgl_pfd (dummy->hdc, need_alpha_bits, &pfd);

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

gboolean
_gdk_win32_display_init_gl (GdkDisplay *display,
                            const gboolean need_alpha_bits)
{
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (display);
  gint glMajMinVersion;
  GdkWindowImplWin32 *impl;
  gint best_idx = 0;
  GdkWGLDummy dummy;

  if (display_win32->have_wgl)
    return TRUE;

  memset (&dummy, 0, sizeof (GdkWGLDummy));

  /* acquire and cache dummy Window (HWND & HDC) and
   * dummy GL Context, it is used to query functions
   * and used for other stuff as well
   */
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

  GDK_NOTE (OPENGL,
            g_print ("WGL API version %d.%d found\n"
                     " - Vendor: %s\n"
                     " - Checked extensions:\n"
                     "\t* WGL_ARB_create_context: %s\n"
                     "\t* WGL_EXT_swap_control: %s\n"
                     "\t* WGL_OML_sync_control: %s\n",
                     display_win32->gl_version / 10,
                     display_win32->gl_version % 10,
                     glGetString (GL_VENDOR),
                     display_win32->hasWglARBCreateContext ? "yes" : "no",
                     display_win32->hasWglEXTSwapControl ? "yes" : "no",
                     display_win32->hasWglOMLSyncControl ? "yes" : "no"));

  wglMakeCurrent (NULL, NULL);
  _destroy_dummy_gl_context (dummy);

  return TRUE;
}

static HGLRC
_create_gl_context (HDC hdc, GdkGLContext *share, GdkGLProfile profile)
{
  HGLRC hglrc;

  /* we need a legacy context first, for all cases */
  hglrc = wglCreateContext (hdc);

  /* Create a WGL 3.2 context, the legacy context *is* needed here */
  if (profile == GDK_GL_PROFILE_3_2_CORE)
    {
      HGLRC hglrc_32;
      GdkWin32GLContext *context_win32;

      gint attribs[] = {
        WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 2,
        0
      };

      if (!wglMakeCurrent (hdc, hglrc))
        return NULL;

      if (share != NULL)
        context_win32 = GDK_WIN32_GL_CONTEXT (share);

      hglrc_32 = wglCreateContextAttribsARB (hdc,
                                          share != NULL ? context_win32->hglrc : NULL,
                                          attribs);

      wglMakeCurrent (NULL, NULL);
      wglDeleteContext (hglrc);
      return hglrc_32;
    }
  else
    {
      /* for legacy WGL, we can't share lists during context creation,
       * so do so immediately afterwards
       */
      if (share != NULL)
        {
          HGLRC hglrc_shared = GDK_WIN32_GL_CONTEXT (share)->hglrc;
          wglShareLists (hglrc_shared, hglrc);
        }
      return hglrc;
    }
}

static gboolean
_set_pixformat_for_hdc (HDC hdc,
                        gint *best_idx,
                        const gboolean need_alpha_bits)
{
  PIXELFORMATDESCRIPTOR pfd;
  gboolean set_pixel_format_result = FALSE;

  /* one is only allowed to call SetPixelFormat(), and so ChoosePixelFormat()
   * one single time per window HDC
   */
  *best_idx = _get_wgl_pfd (hdc, need_alpha_bits, &pfd);
  if (*best_idx != 0)
    set_pixel_format_result = SetPixelFormat (hdc, *best_idx, &pfd);

  /* ChoosePixelFormat() or SetPixelFormat() failed, bail out */
  if (*best_idx == 0 || !set_pixel_format_result)
    return FALSE;
  return TRUE;
}

GdkGLContext *
_gdk_win32_window_create_gl_context (GdkWindow *window,
                                     gboolean attached,
                                     GdkGLProfile profile,
                                     GdkGLContext *share,
                                     GError **error)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkWin32Display *display_win32 = GDK_WIN32_DISPLAY (gdk_window_get_display (window));
  GdkWin32GLContext *context = NULL;
  GdkVisual *visual = gdk_window_get_visual (window);

  /* XXX: gdk_screen_get_rgba_visual() is not implemented on Windows, so
   * need_alpha_bits will always be FALSE for now.
   *
   * Please see bug https://bugzilla.gnome.org/show_bug.cgi?id=727316
   */
  gboolean need_alpha_bits = (visual == gdk_screen_get_rgba_visual (gdk_display_get_default_screen (display)));

  /* Real GL Context and Window items */
  WNDCLASSEX wc;
  ATOM wc_atom;
  HWND hwnd;
  HDC hdc;
  HGLRC hglrc;
  gint pixel_format;

  if (!_gdk_win32_display_init_gl (display, need_alpha_bits))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL implementation is available"));
      return NULL;
    }

  if (profile == GDK_GL_PROFILE_3_2_CORE &&
      !display_win32->hasWglARBCreateContext)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_PROFILE,
                           _("The WGL_ARB_create_context extension "
                             "needed to create 3.2 core profiles is not "
                             "available"));
      return NULL;
    }

  hwnd = GDK_WINDOW_HWND (window);
  hdc = GetDC (hwnd);

  if (!_set_pixformat_for_hdc (hdc, &pixel_format, need_alpha_bits))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                           _("No available configurations for the given pixel format"));
      return NULL;
    }

  if (profile != GDK_GL_PROFILE_3_2_CORE)
    profile = GDK_GL_PROFILE_LEGACY;

  hglrc = _create_gl_context (hdc, share, profile);


  if (hglrc == NULL)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Unable to create a GL context"));
      return NULL;
    }

  display_win32->gl_hdc = hdc;
  display_win32->gl_hwnd = hwnd;

  GDK_NOTE (OPENGL,
            g_print ("Created WGL context[%p], pixel_format=%d\n",
                     hglrc,
                     pixel_format));

  context = g_object_new (GDK_TYPE_WIN32_GL_CONTEXT,
                          "display", display,
                          "window", window,
                          "profile", profile,
                          "shared-context", share,
                          NULL);

  context->hglrc = hglrc;
  context->gl_hdc = hdc;
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

  gboolean do_frame_sync = FALSE;

  if (context == NULL)
    {
      wglMakeCurrent(NULL, NULL);
      return TRUE;
    }

  context_win32 = GDK_WIN32_GL_CONTEXT (context);

  if (!wglMakeCurrent (display_win32->gl_hdc, context_win32->hglrc))
    {
      GDK_NOTE (OPENGL,
                g_print ("Making WGL context current failed\n"));
      return FALSE;
    }

  context_win32->gl_hdc = display_win32->gl_hdc;

  if (context_win32->is_attached && display_win32->hasWglEXTSwapControl)
    {
      window = gdk_gl_context_get_window (context);

      /* If there is compositing there is no particular need to delay
       * the swap when drawing on the offscreen, rendering to the screen
       * happens later anyway, and its up to the compositor to sync that
       * to the vblank. */
      screen = gdk_window_get_screen (window);

      /* XXX: gdk_screen_is_composited () is always FALSE on Windows at the moment */
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
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  if (!GDK_IS_WIN32_DISPLAY (display))
    return FALSE;

  if (!_gdk_win32_display_init_gl (display, FALSE))
    return FALSE;

  if (major != NULL)
    *major = GDK_WIN32_DISPLAY (display)->gl_version / 10;
  if (minor != NULL)
    *minor = GDK_WIN32_DISPLAY (display)->gl_version % 10;

  return TRUE;
}
