/* GDK - The GIMP Drawing Kit
 *
 * gdkmirglcontext.c: Mir specific OpenGL wrappers
 *
 * Copyright © 2014 Canonical Ltd
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

#include "gdkmir-private.h"
#include "gdkinternals.h"
#include "gdkintl.h"

G_DEFINE_TYPE (GdkMirGLContext, gdk_mir_gl_context, GDK_TYPE_GL_CONTEXT)

#define N_EGL_ATTRS 16

static gboolean
gdk_mir_gl_context_realize (GdkGLContext *context,
                            GError      **error)
{
  GdkMirGLContext *context_mir = GDK_MIR_GL_CONTEXT (context);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  GdkGLContext *share = gdk_gl_context_get_shared_context (context);
  EGLContext ctx;
  EGLint context_attribs[N_EGL_ATTRS];
  int major, minor, flags;
  gboolean debug_bit, forward_bit;
  int i = 0;

  if (!_gdk_mir_display_init_egl_display (display))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL implementation is available"));
      return FALSE;
    }

  gdk_gl_context_get_required_version (context, &major, &minor);
  debug_bit = gdk_gl_context_get_debug_enabled (context);
  forward_bit = gdk_gl_context_get_forward_compatible (context);

  flags = 0;

  if (debug_bit)
    flags |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
  if (forward_bit)
    flags |= EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;

  /* We want a core profile */
  context_attribs[i++] = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
  context_attribs[i++] = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;

  /* Specify the version */
  context_attribs[i++] = EGL_CONTEXT_MAJOR_VERSION_KHR;
  context_attribs[i++] = major;
  context_attribs[i++] = EGL_CONTEXT_MINOR_VERSION_KHR;
  context_attribs[i++] = minor;

  /* Specify the flags */
  context_attribs[i++] = EGL_CONTEXT_FLAGS_KHR;
  context_attribs[i++] = flags;

  context_attribs[i++] = EGL_NONE;
  g_assert (i < N_EGL_ATTRS);

  ctx = eglCreateContext (_gdk_mir_display_get_egl_display (display),
                          context_mir->egl_config,
                          share != NULL ? GDK_MIR_GL_CONTEXT (share)->egl_context
                                        : EGL_NO_CONTEXT,
                          context_attribs);
  if (ctx == NULL)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Unable to create a GL context"));
      return FALSE;
    }

  GDK_NOTE (OPENGL, g_print ("Created EGL context[%p]\n", ctx));

  context_mir->egl_context = ctx;

  return TRUE;
}

static void
gdk_mir_gl_context_end_frame (GdkGLContext *context,
                              cairo_region_t *painted,
                              cairo_region_t *damage)
{
  GdkWindow *window = gdk_gl_context_get_window (context);
  GdkDisplay *display = gdk_window_get_display (window);
  GdkMirGLContext *context_mir = GDK_MIR_GL_CONTEXT (context);
  EGLDisplay egl_display = _gdk_mir_display_get_egl_display (display);
  EGLSurface egl_surface;

  gdk_gl_context_make_current (context);

  egl_surface = _gdk_mir_window_get_egl_surface (window,
                                                 context_mir->egl_config);

  if (_gdk_mir_display_have_egl_swap_buffers_with_damage (display))
    {
      int i, j, n_rects = cairo_region_num_rectangles (damage);
      EGLint *rects = g_new (EGLint, n_rects * 4);
      cairo_rectangle_int_t rect;
      int window_height = gdk_window_get_height (window);

      for (i = 0, j = 0; i < n_rects; i++)
        {
          cairo_region_get_rectangle (damage, i, &rect);
          rects[j++] = rect.x;
          rects[j++] = window_height - rect.height - rect.y;
          rects[j++] = rect.width;
          rects[j++] = rect.height;
        }
      eglSwapBuffersWithDamageEXT (egl_display, egl_surface, rects, n_rects);
      g_free (rects);
    }
  else
    {
      eglSwapBuffers (egl_display, egl_surface);
    }
}

static void
gdk_mir_gl_context_dispose (GObject *gobject)
{
  GdkMirGLContext *context_mir = GDK_MIR_GL_CONTEXT (gobject);

  if (context_mir->egl_context != NULL)
    {
      GdkGLContext *context = GDK_GL_CONTEXT (gobject);
      GdkWindow *window = gdk_gl_context_get_window (context);
      GdkDisplay *display = gdk_window_get_display (window);
      EGLDisplay egl_display = _gdk_mir_display_get_egl_display (display);

      if (eglGetCurrentContext () == context_mir->egl_context)
        eglMakeCurrent (egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

      GDK_NOTE (OPENGL, g_print ("Destroying EGL context\n"));

      eglDestroyContext (egl_display, context_mir->egl_context);
      context_mir->egl_context = NULL;
    }

  G_OBJECT_CLASS (gdk_mir_gl_context_parent_class)->dispose (gobject);
}

static void
gdk_mir_gl_context_class_init (GdkMirGLContextClass *klass)
{
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  context_class->realize = gdk_mir_gl_context_realize;
  context_class->end_frame = gdk_mir_gl_context_end_frame;
  gobject_class->dispose = gdk_mir_gl_context_dispose;
}

static void
gdk_mir_gl_context_init (GdkMirGLContext *self)
{
}
