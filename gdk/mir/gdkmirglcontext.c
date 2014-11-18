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
#include "gdkglcontextprivate.h"
#include "gdkintl.h"

#define GDK_MIR_GL_CONTEXT(obj)   (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_MIR_GL_CONTEXT, GdkMirGLContext))

struct _GdkMirGLContext
{
  GdkGLContext parent_instance;

  EGLContext egl_context;
  EGLConfig egl_config;
  gboolean is_attached;
};

struct _GdkMirGLContextClass
{
  GdkGLContextClass parent_class;
};

typedef struct _GdkMirGLContext GdkMirGLContext;
typedef struct _GdkMirGLContextClass GdkMirGLContextClass;

G_DEFINE_TYPE (GdkMirGLContext, gdk_mir_gl_context, GDK_TYPE_GL_CONTEXT)

static void gdk_mir_gl_context_dispose (GObject *gobject);

void
_gdk_mir_window_invalidate_for_new_frame (GdkWindow *window,
                                          cairo_region_t *update_area)
{
  cairo_rectangle_int_t window_rect;
  GdkDisplay *display = gdk_window_get_display (window);
  GdkMirGLContext *context_mir;
  int buffer_age;
  gboolean invalidate_all;
  EGLSurface egl_surface;

  /* Minimal update is ok if we're not drawing with gl */
  if (window->gl_paint_context == NULL)
    return;

  context_mir = GDK_MIR_GL_CONTEXT (window->gl_paint_context);
  buffer_age = 0;

  egl_surface = _gdk_mir_window_get_egl_surface (window, context_mir->egl_config);

  if (_gdk_mir_display_have_egl_buffer_age (display))
    {
      gdk_gl_context_make_current (window->gl_paint_context);
      eglQuerySurface (_gdk_mir_display_get_egl_display (display), egl_surface,
                       EGL_BUFFER_AGE_EXT, &buffer_age);
    }

  invalidate_all = FALSE;
  if (buffer_age == 0 || buffer_age >= 4)
    invalidate_all = TRUE;
  else
    {
      if (buffer_age >= 2)
        {
          if (window->old_updated_area[0])
            cairo_region_union (update_area, window->old_updated_area[0]);
          else
            invalidate_all = TRUE;
        }
      if (buffer_age >= 3)
        {
          if (window->old_updated_area[1])
            cairo_region_union (update_area, window->old_updated_area[1]);
          else
            invalidate_all = TRUE;
        }
    }

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

static void
gdk_mir_gl_context_update (GdkGLContext *context)
{
  GdkWindow *window = gdk_gl_context_get_window (context);
  int width, height;

  gdk_gl_context_make_current (context);

  width = gdk_window_get_width (window);
  height = gdk_window_get_height (window);

  GDK_NOTE (OPENGL, g_print ("Updating GL viewport size to { %d, %d } for window %p (context: %p)\n",
                             width, height,
                             window, context));

  glViewport (0, 0, width, height);
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
gdk_mir_gl_context_class_init (GdkMirGLContextClass *klass)
{
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  context_class->update = gdk_mir_gl_context_update;
  context_class->end_frame = gdk_mir_gl_context_end_frame;
  gobject_class->dispose = gdk_mir_gl_context_dispose;
}

static void
gdk_mir_gl_context_init (GdkMirGLContext *self)
{
}

#define MAX_EGL_ATTRS   30

static gboolean
find_eglconfig_for_window (GdkWindow  *window,
                           EGLConfig  *egl_config_out,
                           GError    **error)
{
  GdkDisplay *display = gdk_window_get_display (window);
  EGLDisplay *egl_display = _gdk_mir_display_get_egl_display (display);
  GdkVisual *visual = gdk_window_get_visual (window);
  EGLint attrs[MAX_EGL_ATTRS];
  EGLint count;
  EGLConfig *configs;
  gboolean use_rgba;

  int i = 0;

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

  use_rgba = (visual == gdk_screen_get_rgba_visual (gdk_display_get_default_screen (display)));

  if (use_rgba)
    {
      attrs[i++] = EGL_ALPHA_SIZE;
      attrs[i++] = 1;
    }
  else
    {
      attrs[i++] = EGL_ALPHA_SIZE;
      attrs[i++] = 0;
    }

  attrs[i++] = EGL_NONE;
  g_assert (i < MAX_EGL_ATTRS);

  if (!eglChooseConfig (egl_display, attrs, NULL, 0, &count) || count < 1)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                           _("No available configurations for the given pixel format"));
      return FALSE;
    }

  configs = g_new (EGLConfig, count);

  if (!eglChooseConfig (egl_display, attrs, configs, count, &count) || count < 1)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                           _("No available configurations for the given pixel format"));
      return FALSE;
    }

  /* Pick first valid configuration i guess? */

  if (egl_config_out != NULL)
    *egl_config_out = configs[0];

  g_free (configs);

  return TRUE;
}

GdkGLContext *
_gdk_mir_window_create_gl_context (GdkWindow     *window,
                                   gboolean       attached,
                                   GdkGLProfile   profile,
                                   GdkGLContext  *share,
                                   GError       **error)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkMirGLContext *context;
  EGLContext ctx;
  EGLConfig config;
  int i;
  EGLint context_attribs[3];

  if (!_gdk_mir_display_init_egl_display (display))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL implementation is available"));
      return NULL;
    }

  if (profile == GDK_GL_PROFILE_DEFAULT)
    profile = GDK_GL_PROFILE_LEGACY;

  if (profile == GDK_GL_PROFILE_3_2_CORE &&
      !_gdk_mir_display_have_egl_khr_create_context (display))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_PROFILE,
                           _("3.2 core GL profile is not available on EGL implementation"));
      return NULL;
    }

  if (!find_eglconfig_for_window (window, &config, error))
    return NULL;

  i = 0;
  if (profile == GDK_GL_PROFILE_3_2_CORE)
    {
      context_attribs[i++] = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
      context_attribs[i++] = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;
    }
  context_attribs[i++] = EGL_NONE;

  ctx = eglCreateContext (_gdk_mir_display_get_egl_display (display),
                          config,
                          share ? GDK_MIR_GL_CONTEXT (share)->egl_context : EGL_NO_CONTEXT,
                          context_attribs);
  if (ctx == NULL)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Unable to create a GL context"));
      return NULL;
    }

  GDK_NOTE (OPENGL,
            g_print ("Created EGL context[%p]\n", ctx));

  context = g_object_new (GDK_TYPE_MIR_GL_CONTEXT,
                          "display", display,
                          "window", window,
                          "profile", profile,
                          "shared-context", share,
                          NULL);

  context->egl_config = config;
  context->egl_context = ctx;
  context->is_attached = attached;

  return GDK_GL_CONTEXT (context);
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

gboolean
_gdk_mir_display_make_gl_context_current (GdkDisplay   *display,
                                          GdkGLContext *context)
{
  EGLDisplay egl_display = _gdk_mir_display_get_egl_display (display);
  GdkMirGLContext *mir_context;
  GdkWindow *window;
  EGLSurface egl_surface;

  if (context == NULL)
    {
      eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      return TRUE;
    }

  mir_context = GDK_MIR_GL_CONTEXT (context);
  window = gdk_gl_context_get_window (context);

  if (mir_context->is_attached)
    {
      egl_surface = _gdk_mir_window_get_egl_surface (window,
                                                     mir_context->egl_config);
    }
  else
    {
      if (_gdk_mir_display_have_egl_surfaceless_context (display))
        egl_surface = EGL_NO_SURFACE;
      else
        egl_surface = _gdk_mir_window_get_dummy_egl_surface (window,
                                                             mir_context->egl_config);
    }

  if (!eglMakeCurrent (egl_display, egl_surface, egl_surface, mir_context->egl_context))
    {
      g_warning ("eglMakeCurrent failed");
      return FALSE;
    }

  return TRUE;
}
