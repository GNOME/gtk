/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext-wayland.c: Wayland specific OpenGL wrappers
 *
 * Copyright © 2014  Emmanuele Bassi
 * Copyright © 2014  Alexander Larsson
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

#include "gdkglcontext-wayland.h"
#include "gdkdisplay-wayland.h"

#include "gdkwaylanddisplay.h"
#include "gdkwaylandglcontext.h"
#include "gdkwaylandwindow.h"
#include "gdkprivate-wayland.h"

#include "gdkinternals.h"

#include "gdkintl.h"

G_DEFINE_TYPE (GdkWaylandGLContext, gdk_wayland_gl_context, GDK_TYPE_GL_CONTEXT)

static void gdk_x11_gl_context_dispose (GObject *gobject);

static void
gdk_wayland_gl_context_update (GdkGLContext *context)
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

void
gdk_wayland_window_invalidate_for_new_frame (GdkWindow      *window,
                                             cairo_region_t *update_area)
{
  cairo_rectangle_int_t window_rect;
  GdkDisplay *display = gdk_window_get_display (window);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GdkWaylandGLContext *context_wayland;
  int buffer_age;
  gboolean invalidate_all;
  EGLSurface egl_surface;

  /* Minimal update is ok if we're not drawing with gl */
  if (window->gl_paint_context == NULL)
    return;

  context_wayland = GDK_WAYLAND_GL_CONTEXT (window->gl_paint_context);
  buffer_age = 0;

  egl_surface = gdk_wayland_window_get_egl_surface (window->impl_window,
                                                    context_wayland->egl_config);

  if (display_wayland->have_egl_buffer_age)
    {
      gdk_gl_context_make_current (window->gl_paint_context);
      eglQuerySurface (display_wayland->egl_display, egl_surface,
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
gdk_wayland_gl_context_end_frame (GdkGLContext *context,
                                  cairo_region_t *painted,
                                  cairo_region_t *damage)
{
  GdkWindow *window = gdk_gl_context_get_window (context);
  GdkDisplay *display = gdk_window_get_display (window);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GdkWaylandGLContext *context_wayland = GDK_WAYLAND_GL_CONTEXT (context);
  EGLSurface egl_surface;

  gdk_gl_context_make_current (context);

  egl_surface = gdk_wayland_window_get_egl_surface (window->impl_window,
                                                    context_wayland->egl_config);

  // TODO: Use eglSwapBuffersWithDamageEXT if available
  if (display_wayland->have_egl_swap_buffers_with_damage)
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
      eglSwapBuffersWithDamageEXT (display_wayland->egl_display, egl_surface, rects, n_rects);
      g_free (rects);
    }
  else
    eglSwapBuffers (display_wayland->egl_display, egl_surface);
}

static void
gdk_wayland_gl_context_class_init (GdkWaylandGLContextClass *klass)
{
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  context_class->update = gdk_wayland_gl_context_update;
  context_class->end_frame = gdk_wayland_gl_context_end_frame;
  gobject_class->dispose = gdk_x11_gl_context_dispose;
}

static void
gdk_wayland_gl_context_init (GdkWaylandGLContext *self)
{
}

gboolean
gdk_wayland_display_init_gl (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  EGLint major, minor;
  EGLDisplay *dpy;

  if (display_wayland->have_egl)
    return TRUE;

  dpy = eglGetDisplay ((EGLNativeDisplayType)display_wayland->wl_display);
  if (dpy == NULL)
    return FALSE;

  if (!eglInitialize (dpy, &major, &minor))
    return FALSE;

  if (!eglBindAPI (EGL_OPENGL_API))
    return FALSE;

  display_wayland->egl_display = dpy;
  display_wayland->egl_major_version = major;
  display_wayland->egl_minor_version = minor;

  display_wayland->have_egl = TRUE;

  display_wayland->have_egl_khr_create_context =
    epoxy_has_egl_extension (dpy, "EGL_KHR_create_context");

  display_wayland->have_egl_buffer_age =
    epoxy_has_egl_extension (dpy, "EGL_EXT_buffer_age");

  display_wayland->have_egl_swap_buffers_with_damage =
    epoxy_has_egl_extension (dpy, "EGL_EXT_swap_buffers_with_damage");

  display_wayland->have_egl_surfaceless_context =
    epoxy_has_egl_extension (dpy, "EGL_KHR_surfaceless_context");

  GDK_NOTE (OPENGL,
            g_print ("EGL API version %d.%d found\n"
                     " - Vendor: %s\n"
                     " - Version: %s\n"
                     " - Client APIs: %s\n"
                     " - Extensions:\n"
                     "\t%s\n"
                     ,
                     display_wayland->egl_major_version,
                     display_wayland->egl_minor_version,
                     eglQueryString (dpy, EGL_VENDOR),
                     eglQueryString(dpy, EGL_VERSION),
                     eglQueryString(dpy, EGL_CLIENT_APIS),
                     eglQueryString(dpy, EGL_EXTENSIONS)));

  return TRUE;
}

#define MAX_EGL_ATTRS   30

static gboolean
find_eglconfig_for_window (GdkWindow        *window,
                           EGLConfig         *egl_config_out,
                           GError           **error)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
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

  if (!eglChooseConfig (display_wayland->egl_display, attrs, NULL, 0, &count) || count < 1)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                           _("No available configurations for the given pixel format"));
      return FALSE;
    }

  configs = g_new (EGLConfig, count);

  if (!eglChooseConfig (display_wayland->egl_display, attrs, configs, count, &count) || count < 1)
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
gdk_wayland_window_create_gl_context (GdkWindow     *window,
				      gboolean       attached,
                                      GdkGLProfile   profile,
                                      GdkGLContext  *share,
                                      GError       **error)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GdkWaylandGLContext *context;
  EGLContext ctx;
  EGLConfig config;
  int i;
  EGLint context_attribs[3];

  if (!gdk_wayland_display_init_gl (display))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL implementation is available"));
      return NULL;
    }

  if (profile == GDK_GL_PROFILE_DEFAULT)
    profile = GDK_GL_PROFILE_LEGACY;

  if (profile == GDK_GL_PROFILE_3_2_CORE &&
      !display_wayland->have_egl_khr_create_context)
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

  ctx = eglCreateContext (display_wayland->egl_display,
                          config,
                          share ? GDK_WAYLAND_GL_CONTEXT (share)->egl_context : EGL_NO_CONTEXT,
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

  context = g_object_new (GDK_TYPE_WAYLAND_GL_CONTEXT,
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
gdk_x11_gl_context_dispose (GObject *gobject)
{
  GdkWaylandGLContext *context_wayland = GDK_WAYLAND_GL_CONTEXT (gobject);

  if (context_wayland->egl_context != NULL)
    {
      GdkGLContext *context = GDK_GL_CONTEXT (gobject);
      GdkWindow *window = gdk_gl_context_get_window (context);
      GdkDisplay *display = gdk_window_get_display (window);
      GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

      if (eglGetCurrentContext () == context_wayland->egl_context)
        eglMakeCurrent(display_wayland->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);

      GDK_NOTE (OPENGL, g_print ("Destroying EGL context\n"));

      eglDestroyContext (display_wayland->egl_display,
                         context_wayland->egl_context);
      context_wayland->egl_context = NULL;
    }

  G_OBJECT_CLASS (gdk_wayland_gl_context_parent_class)->dispose (gobject);
}

gboolean
gdk_wayland_display_make_gl_context_current (GdkDisplay   *display,
                                             GdkGLContext *context)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GdkWaylandGLContext *context_wayland;
  GdkWindow *window;
  EGLSurface egl_surface;

  if (context == NULL)
    {
      eglMakeCurrent(display_wayland->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                     EGL_NO_CONTEXT);
      return TRUE;
    }

  context_wayland = GDK_WAYLAND_GL_CONTEXT (context);
  window = gdk_gl_context_get_window (context);

  if (context_wayland->is_attached)
    egl_surface = gdk_wayland_window_get_egl_surface (window->impl_window, context_wayland->egl_config);
  else
    {
      if (display_wayland->have_egl_surfaceless_context)
	egl_surface = EGL_NO_SURFACE;
      else
	egl_surface = gdk_wayland_window_get_dummy_egl_surface (window->impl_window,
								context_wayland->egl_config);
    }

  if (!eglMakeCurrent (display_wayland->egl_display, egl_surface,
                       egl_surface, context_wayland->egl_context))
    {
      g_warning ("eglMakeCurrent failed");
      return FALSE;
    }

  return TRUE;
}
