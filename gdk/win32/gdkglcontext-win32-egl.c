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
#include <epoxy/egl.h>

struct _GdkWin32GLContextEGL
{
  GdkWin32GLContext parent_instance;
};

typedef struct _GdkWin32GLContextClass    GdkWin32GLContextEGLClass;

G_DEFINE_TYPE (GdkWin32GLContextEGL, gdk_win32_gl_context_egl, GDK_TYPE_WIN32_GL_CONTEXT)

static void
gdk_win32_gl_context_egl_empty_frame (GdkDrawContext *draw_context)
{
}

static void
gdk_win32_gl_context_egl_class_init (GdkWin32GLContextClass *klass)
{
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS(klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS(klass);

  context_class->backend_type = GDK_GL_EGL;

  draw_context_class->empty_frame = gdk_win32_gl_context_egl_empty_frame;
}

static void
gdk_win32_gl_context_egl_init (GdkWin32GLContextEGL *egl_context)
{
}
