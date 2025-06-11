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
#include "gdkwin32surface.h"

#include "gdkglcontext.h"
#include "gdksurface.h"

#include <dwmapi.h>
#include <epoxy/wgl.h>

#ifdef HAVE_EGL
# include <epoxy/egl.h>
#endif

G_DEFINE_ABSTRACT_TYPE (GdkWin32GLContext, gdk_win32_gl_context, GDK_TYPE_GL_CONTEXT)

ATOM
gdk_win32_gl_context_get_class (void)
{
  static ATOM class_atom = 0;

  if (class_atom)
    return class_atom;

  class_atom = RegisterClassExW (&(WNDCLASSEX) {
                                     .cbSize = sizeof (WNDCLASSEX),
                                     .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
                                     .lpfnWndProc = DefWindowProc,
                                     .hInstance = this_module (),
                                     .lpszClassName  = L"GdkWin32GL",
                                 });
  if (class_atom == 0)
    WIN32_API_FAILED ("RegisterClassExW");

  return class_atom;
}

gboolean
gdk_win32_gl_context_surface_attach (GdkDrawContext  *context,
                                     GError         **error)
{
  GdkWin32GLContext *self = GDK_WIN32_GL_CONTEXT (context);
  GdkSurface *surface;
  GdkWin32Display *display;
  IUnknown *dcomp_surface;
  guint width, height;

  surface = gdk_draw_context_get_surface (context);
  display = GDK_WIN32_DISPLAY (gdk_draw_context_get_display (context));

  if (!gdk_win32_display_get_dcomp_device (display))
    {
      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE, "OpenGL requires Direct Composition");
      return FALSE;
    }
  gdk_draw_context_get_buffer_size (context, &width, &height);
  
  self->handle = CreateWindowExW (0,
                                  MAKEINTRESOURCEW (gdk_win32_gl_context_get_class ()),
                                  NULL,
                                  /* MSDN: We need WS_CLIPCHILDREN and WS_CLIPSIBLINGS for GL Context Creation */
                                  WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                  0, 0,
                                  width, height,
                                  GDK_SURFACE_HWND (surface),
                                  NULL,
                                  this_module (),
                                  self);
  if (self->handle == NULL)
    {
      DWORD err = GetLastError ();
      gdk_win32_check_hresult (HRESULT_FROM_WIN32 (err), error, "Failed to create rendering window");
      return FALSE;
    }
  
  hr_warn (DwmSetWindowAttribute (self->handle, DWMWA_CLOAK, (BOOL[1]) { true }, sizeof (BOOL)));

  ShowWindow (self->handle, SW_SHOWNOACTIVATE);

  hr_warn (IDCompositionDevice_CreateSurfaceFromHwnd (gdk_win32_display_get_dcomp_device (display),
                                                      self->handle,
                                                      &dcomp_surface));
  gdk_win32_surface_set_dcomp_content (GDK_WIN32_SURFACE (surface), dcomp_surface);

  return TRUE;
}

static void
gdk_win32_gl_context_surface_detach (GdkDrawContext *context)
{
  GdkWin32GLContext *self = GDK_WIN32_GL_CONTEXT (context);
  GdkSurface *surface = gdk_draw_context_get_surface (context);

  if (!GDK_SURFACE_DESTROYED (surface))
    {
      gdk_win32_surface_set_dcomp_content (GDK_WIN32_SURFACE (surface), NULL);

      if (!DestroyWindow (self->handle))
        WIN32_API_FAILED ("DestroyWindow");
      self->handle = NULL;
    }
}

static void
gdk_win32_gl_context_surface_resized (GdkDrawContext *draw_context)
{
  GdkWin32GLContext *self = GDK_WIN32_GL_CONTEXT (draw_context);
  guint width, height;

  if (self->handle)
    {
      gdk_draw_context_get_buffer_size (draw_context, &width, &height);

      API_CALL (SetWindowPos, (self->handle,
                              SWP_NOZORDER_SPECIFIED,
                              0,
                              0,
                              width, height,
                              SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOCOPYBITS | SWP_NOZORDER));
  }

  GDK_DRAW_CONTEXT_CLASS (gdk_win32_gl_context_parent_class)->surface_resized (draw_context);
}

static void
gdk_win32_gl_context_class_init (GdkWin32GLContextClass *klass)
{
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  draw_context_class->surface_attach = gdk_win32_gl_context_surface_attach;
  draw_context_class->surface_detach = gdk_win32_gl_context_surface_detach;
  draw_context_class->surface_resized = gdk_win32_gl_context_surface_resized;
}

static void
gdk_win32_gl_context_init (GdkWin32GLContext *self)
{
}
