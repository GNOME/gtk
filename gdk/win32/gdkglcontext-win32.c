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
#include <glib/gi18n-lib.h>

#include <cairo.h>
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


static void
gdk_win32_gl_context_class_init (GdkWin32GLContextClass *klass)
{
}

static void
gdk_win32_gl_context_init (GdkWin32GLContext *self)
{
}
