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
#include "gdksurface-wayland.h"

#include "gdkwaylanddisplay.h"
#include "gdkwaylandglcontext.h"
#include "gdkwaylandsurface.h"
#include "gdkprivate-wayland.h"

#include "gdkinternals.h"
#include "gdk-private.h"
#include "gdksurfaceprivate.h"
#include "gdkprofilerprivate.h"

#include "gdkintl.h"

/**
 * GdkWaylandGLContext:
 *
 * The Wayland implementation of `GdkGLContext`.
 */

G_DEFINE_TYPE (GdkWaylandGLContext, gdk_wayland_gl_context, GDK_TYPE_GL_CONTEXT)

static void gdk_wayland_gl_context_dispose (GObject *gobject);

#define N_EGL_ATTRS     16

static gboolean
gdk_wayland_gl_context_realize (GdkGLContext *context,
                                GError      **error)
{
  GdkWaylandGLContext *context_wayland = GDK_WAYLAND_GL_CONTEXT (context);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  GdkGLContext *share = gdk_display_get_gl_context (display);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  EGLContext ctx;
  EGLint context_attribs[N_EGL_ATTRS];
  int major, minor, flags;
  gboolean debug_bit, forward_bit, legacy_bit, use_es;
  int i = 0;
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;

  gdk_gl_context_get_required_version (context, &major, &minor);
  debug_bit = gdk_gl_context_get_debug_enabled (context);
  forward_bit = gdk_gl_context_get_forward_compatible (context);
  legacy_bit = GDK_DISPLAY_DEBUG_CHECK (display, GL_LEGACY) ||
               (share != NULL && gdk_gl_context_is_legacy (share));
  use_es = GDK_DISPLAY_DEBUG_CHECK (display, GL_GLES) ||
           (share != NULL && gdk_gl_context_get_use_es (share));

  flags = 0;

  if (debug_bit)
    flags |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
  if (forward_bit)
    flags |= EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;

  if (!use_es)
    {
      eglBindAPI (EGL_OPENGL_API);

      /* We want a core profile, unless in legacy mode */
      context_attribs[i++] = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
      context_attribs[i++] = legacy_bit
                           ? EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR
                           : EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;

      /* Specify the version */
      context_attribs[i++] = EGL_CONTEXT_MAJOR_VERSION_KHR;
      context_attribs[i++] = legacy_bit ? 3 : major;
      context_attribs[i++] = EGL_CONTEXT_MINOR_VERSION_KHR;
      context_attribs[i++] = legacy_bit ? 0 : minor;
    }
  else
    {
      eglBindAPI (EGL_OPENGL_ES_API);

      context_attribs[i++] = EGL_CONTEXT_CLIENT_VERSION;
      if (major == 3)
        context_attribs[i++] = 3;
      else
        context_attribs[i++] = 2;
    }

  /* Specify the flags */
  context_attribs[i++] = EGL_CONTEXT_FLAGS_KHR;
  context_attribs[i++] = flags;

  context_attribs[i++] = EGL_NONE;
  g_assert (i < N_EGL_ATTRS);

  GDK_DISPLAY_NOTE (display, OPENGL,
            g_message ("Creating EGL context version %d.%d (debug:%s, forward:%s, legacy:%s, es:%s)",
                               major, minor,
                               debug_bit ? "yes" : "no",
                               forward_bit ? "yes" : "no",
                               legacy_bit ? "yes" : "no",
                               use_es ? "yes" : "no"));

  ctx = eglCreateContext (display_wayland->egl_display,
                          display_wayland->egl_config,
                          share != NULL ? GDK_WAYLAND_GL_CONTEXT (share)->egl_context
                                        : EGL_NO_CONTEXT,
                          context_attribs);

  /* If context creation failed without the ES bit, let's try again with it */
  if (ctx == NULL)
    {
      i = 0;
      context_attribs[i++] = EGL_CONTEXT_MAJOR_VERSION;
      context_attribs[i++] = 2;
      context_attribs[i++] = EGL_CONTEXT_MINOR_VERSION;
      context_attribs[i++] = 0;
      context_attribs[i++] = EGL_CONTEXT_FLAGS_KHR;
      context_attribs[i++] = flags & ~EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;
      context_attribs[i++] = EGL_NONE;
      g_assert (i < N_EGL_ATTRS);

      eglBindAPI (EGL_OPENGL_ES_API);

      legacy_bit = FALSE;
      use_es = TRUE;

      GDK_DISPLAY_NOTE (display, OPENGL,
                g_message ("eglCreateContext failed, switching to OpenGL ES"));
      ctx = eglCreateContext (display_wayland->egl_display,
                              display_wayland->egl_config,
                              share != NULL ? GDK_WAYLAND_GL_CONTEXT (share)->egl_context
                                            : EGL_NO_CONTEXT,
                              context_attribs);
    }

  /* If context creation failed without the legacy bit, let's try again with it */
  if (ctx == NULL)
    {
      i = 0;
      context_attribs[i++] = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
      context_attribs[i++] = EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR;
      context_attribs[i++] = EGL_CONTEXT_MAJOR_VERSION;
      context_attribs[i++] = 3;
      context_attribs[i++] = EGL_CONTEXT_MINOR_VERSION;
      context_attribs[i++] = 0;
      context_attribs[i++] = EGL_CONTEXT_FLAGS_KHR;
      context_attribs[i++] = flags & ~EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;
      context_attribs[i++] = EGL_NONE;
      g_assert (i < N_EGL_ATTRS);

      eglBindAPI (EGL_OPENGL_API);

      legacy_bit = TRUE;
      use_es = FALSE;

      GDK_DISPLAY_NOTE (display, OPENGL,
                g_message ("eglCreateContext failed, switching to legacy"));
      ctx = eglCreateContext (display_wayland->egl_display,
                              display_wayland->egl_config,
                              share != NULL ? GDK_WAYLAND_GL_CONTEXT (share)->egl_context
                                            : EGL_NO_CONTEXT,
                              context_attribs);
    }

  if (ctx == NULL)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Unable to create a GL context"));
      return FALSE;
    }

  GDK_DISPLAY_NOTE (display, OPENGL, g_message ("Created EGL context[%p]", ctx));

  context_wayland->egl_context = ctx;

  gdk_gl_context_set_is_legacy (context, legacy_bit);
  gdk_gl_context_set_use_es (context, use_es);

  gdk_profiler_end_mark (start_time, "realize GdkWaylandGLContext", NULL);

  return TRUE;
}

static cairo_region_t *
gdk_wayland_gl_context_get_damage (GdkGLContext *context)
{
  GdkDisplay *display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  EGLSurface egl_surface;
  GdkSurface *surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (context));
  int buffer_age = 0;

  if (display_wayland->have_egl_buffer_age)
    {
      egl_surface = gdk_wayland_surface_get_egl_surface (surface);
      gdk_gl_context_make_current (context);
      eglQuerySurface (display_wayland->egl_display, egl_surface,
                       EGL_BUFFER_AGE_EXT, &buffer_age);

      switch (buffer_age)
        {
          case 1:
            return cairo_region_create ();
            break;

          case 2:
            if (context->old_updated_area[0])
              return cairo_region_copy (context->old_updated_area[0]);
            break;

          case 3:
            if (context->old_updated_area[0] &&
                context->old_updated_area[1])
              {
                cairo_region_t *damage = cairo_region_copy (context->old_updated_area[0]);
                cairo_region_union (damage, context->old_updated_area[1]);
                return damage;
              }
            break;

          default:
            ;
        }
    }

  return GDK_GL_CONTEXT_CLASS (gdk_wayland_gl_context_parent_class)->get_damage (context);
}

static gboolean
gdk_wayland_gl_context_clear_current (GdkGLContext *context)
{
  GdkDisplay *display = gdk_gl_context_get_display (context);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  return eglMakeCurrent (display_wayland->egl_display,
                         EGL_NO_SURFACE,
                         EGL_NO_SURFACE,
                         EGL_NO_CONTEXT);
}

static gboolean
gdk_wayland_gl_context_make_current (GdkGLContext *context,
                                     gboolean      surfaceless)
{
  GdkWaylandGLContext *context_wayland = GDK_WAYLAND_GL_CONTEXT (context);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  EGLSurface egl_surface;

  if (!surfaceless)
    egl_surface = gdk_wayland_surface_get_egl_surface (gdk_gl_context_get_surface (context));
  else
    egl_surface = EGL_NO_SURFACE;

  return eglMakeCurrent (display_wayland->egl_display,
                         egl_surface,
                         egl_surface,
                         context_wayland->egl_context);
}

static void
gdk_wayland_gl_context_begin_frame (GdkDrawContext *draw_context,
                                    cairo_region_t *region)
{
  GDK_DRAW_CONTEXT_CLASS (gdk_wayland_gl_context_parent_class)->begin_frame (draw_context, region);

  glDrawBuffers (1, (GLenum[1]) { GL_BACK });
}

static void
gdk_wayland_gl_context_end_frame (GdkDrawContext *draw_context,
                                  cairo_region_t *painted)
{
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);
  GdkSurface *surface = gdk_gl_context_get_surface (context);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  EGLSurface egl_surface;

  GDK_DRAW_CONTEXT_CLASS (gdk_wayland_gl_context_parent_class)->end_frame (draw_context, painted);

  gdk_gl_context_make_current (context);

  egl_surface = gdk_wayland_surface_get_egl_surface (surface);
  gdk_wayland_surface_sync (surface);
  gdk_wayland_surface_request_frame (surface);

  gdk_profiler_add_mark (GDK_PROFILER_CURRENT_TIME, 0, "wayland", "swap buffers");
  if (display_wayland->have_egl_swap_buffers_with_damage)
    {
      EGLint stack_rects[4 * 4]; /* 4 rects */
      EGLint *heap_rects = NULL;
      int i, j, n_rects = cairo_region_num_rectangles (painted);
      int surface_height = gdk_surface_get_height (surface);
      int scale = gdk_surface_get_scale_factor (surface);
      EGLint *rects;

      if (n_rects < G_N_ELEMENTS (stack_rects) / 4)
        rects = (EGLint *)&stack_rects;
      else
        heap_rects = rects = g_new (EGLint, n_rects * 4);

      for (i = 0, j = 0; i < n_rects; i++)
        {
          cairo_rectangle_int_t rect;

          cairo_region_get_rectangle (painted, i, &rect);
          rects[j++] = rect.x * scale;
          rects[j++] = (surface_height - rect.height - rect.y) * scale;
          rects[j++] = rect.width * scale;
          rects[j++] = rect.height * scale;
        }
      eglSwapBuffersWithDamageEXT (display_wayland->egl_display, egl_surface, rects, n_rects);
      g_free (heap_rects);
    }
  else
    eglSwapBuffers (display_wayland->egl_display, egl_surface);

  gdk_wayland_surface_notify_committed (surface);
}

static void
gdk_wayland_gl_context_class_init (GdkWaylandGLContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);

  gobject_class->dispose = gdk_wayland_gl_context_dispose;

  draw_context_class->begin_frame = gdk_wayland_gl_context_begin_frame;
  draw_context_class->end_frame = gdk_wayland_gl_context_end_frame;

  context_class->realize = gdk_wayland_gl_context_realize;
  context_class->make_current = gdk_wayland_gl_context_make_current;
  context_class->clear_current = gdk_wayland_gl_context_clear_current;
  context_class->get_damage = gdk_wayland_gl_context_get_damage;

  context_class->backend_type = GDK_GL_EGL;
}

static void
gdk_wayland_gl_context_init (GdkWaylandGLContext *self)
{
}

/**
 * gdk_wayland_display_get_egl_display:
 * @display: (type GdkWaylandDisplay): a Wayland display
 *
 * Retrieves the EGL display connection object for the given GDK display.
 *
 * Returns: (nullable): the EGL display
 *
 * Since: 4.4
 */
gpointer
gdk_wayland_display_get_egl_display (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland;

  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY (display), NULL);

  if (!gdk_display_prepare_gl (display, NULL))
    return NULL;

  display_wayland = GDK_WAYLAND_DISPLAY (display);

  return display_wayland->egl_display;
}

static EGLDisplay
get_egl_display (GdkWaylandDisplay *display_wayland)
{
  EGLDisplay dpy = NULL;

  if (epoxy_has_egl_extension (NULL, "EGL_KHR_platform_base"))
    {
      PFNEGLGETPLATFORMDISPLAYPROC getPlatformDisplay =
        (void *) eglGetProcAddress ("eglGetPlatformDisplay");

      if (getPlatformDisplay != NULL)
        dpy = getPlatformDisplay (EGL_PLATFORM_WAYLAND_EXT,
                                  display_wayland->wl_display,
                                  NULL);
      if (dpy != NULL)
        goto out;
    }

  if (epoxy_has_egl_extension (NULL, "EGL_EXT_platform_base"))
    {
      PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay =
        (void *) eglGetProcAddress ("eglGetPlatformDisplayEXT");

      if (getPlatformDisplay != NULL)
        dpy = getPlatformDisplay (EGL_PLATFORM_WAYLAND_EXT,
                                  display_wayland->wl_display,
                                  NULL);
      if (dpy != NULL)
        goto out;
    }

  dpy = eglGetDisplay ((EGLNativeDisplayType) display_wayland->wl_display);

out:
  return dpy;
}

#define MAX_EGL_ATTRS   30

static EGLConfig
get_eglconfig (EGLDisplay dpy)
{
  EGLint attrs[MAX_EGL_ATTRS];
  EGLint count;
  EGLConfig config;
  int i = 0;

  attrs[i++] = EGL_SURFACE_TYPE;
  attrs[i++] = EGL_WINDOW_BIT;

  attrs[i++] = EGL_COLOR_BUFFER_TYPE;
  attrs[i++] = EGL_RGB_BUFFER;

  attrs[i++] = EGL_RED_SIZE;
  attrs[i++] = 8;
  attrs[i++] = EGL_GREEN_SIZE;
  attrs[i++] = 8;
  attrs[i++] = EGL_BLUE_SIZE;
  attrs[i++] = 8;
  attrs[i++] = EGL_ALPHA_SIZE;
  attrs[i++] = 8;

  attrs[i++] = EGL_NONE;
  g_assert (i < MAX_EGL_ATTRS);

  /* Pick first valid configuration i guess? */
  if (!eglChooseConfig (dpy, attrs, &config, 1, &count) || count < 1)
    return NULL;

  return config;
}

#undef MAX_EGL_ATTRS

GdkGLContext *
gdk_wayland_display_init_gl (GdkDisplay  *display,
                             GError     **error)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  EGLint major, minor;
  EGLDisplay dpy;
  GdkGLContext *ctx;
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;
  G_GNUC_UNUSED gint64 start_time2;

  if (!gdk_gl_backend_can_be_used (GDK_GL_EGL, error))
    return FALSE;

  if (!epoxy_has_egl ())
    {
      gboolean sandboxed = gdk_running_in_sandbox ();

      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           sandboxed ? _("libEGL not available in this sandbox")
                                     : _("libEGL not available"));
      return NULL;
    }

  start_time2 = GDK_PROFILER_CURRENT_TIME;
  dpy = get_egl_display (display_wayland);
  gdk_profiler_end_mark (start_time, "get_egl_display", NULL);
  if (dpy == NULL)
    {
      gboolean sandboxed = gdk_running_in_sandbox ();

      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           sandboxed ? _("Sandbox does not provide an OpenGL implementation")
                                     : _("No OpenGL implementation available"));
      return NULL;
    }

  start_time2 = GDK_PROFILER_CURRENT_TIME;
  if (!eglInitialize (dpy, &major, &minor))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Could not initialize EGL display"));
      return NULL;
    }
  gdk_profiler_end_mark (start_time2, "eglInitialize", NULL);

  if (major < GDK_EGL_MIN_VERSION_MAJOR ||
      (major == GDK_EGL_MIN_VERSION_MAJOR && minor < GDK_EGL_MIN_VERSION_MINOR))
    {
      eglTerminate (dpy);
      g_set_error (error, GDK_GL_ERROR,
                   GDK_GL_ERROR_NOT_AVAILABLE,
                   _("EGL version %d.%d is too old. GTK requires %d.%d"),
                   major, minor, GDK_EGL_MIN_VERSION_MAJOR, GDK_EGL_MIN_VERSION_MINOR);
      return NULL;
    }

  start_time2 = GDK_PROFILER_CURRENT_TIME;
  if (!eglBindAPI (EGL_OPENGL_API))
    {
      eglTerminate (dpy);
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL implementation is available"));
      return NULL;
    }
  gdk_profiler_end_mark (start_time2, "eglBindAPI", NULL);

  if (!epoxy_has_egl_extension (dpy, "EGL_KHR_create_context"))
    {
      eglTerminate (dpy);
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_PROFILE,
                           _("Core GL is not available on EGL implementation"));
      return NULL;
    }

  if (!epoxy_has_egl_extension (dpy, "EGL_KHR_surfaceless_context"))
    {
      eglTerminate (dpy);
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_PROFILE,
                           _("Surfaceless contexts are not supported on this EGL implementation"));
      return NULL;
    }

  start_time2 = GDK_PROFILER_CURRENT_TIME;
  display_wayland->egl_config = get_eglconfig (dpy);
  gdk_profiler_end_mark (start_time2, "get_eglconfig", NULL);
  if (!display_wayland->egl_config)
    {
      eglTerminate (dpy);
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                           _("No available configurations for the given pixel format"));
      return NULL;
    }

  display_wayland->egl_display = dpy;
  display_wayland->egl_major_version = major;
  display_wayland->egl_minor_version = minor;

  display_wayland->have_egl_buffer_age =
    epoxy_has_egl_extension (dpy, "EGL_EXT_buffer_age");

  display_wayland->have_egl_swap_buffers_with_damage =
    epoxy_has_egl_extension (dpy, "EGL_EXT_swap_buffers_with_damage");

  GDK_DISPLAY_NOTE (display, OPENGL,
            g_message ("EGL API version %d.%d found\n"
                       " - Vendor: %s\n"
                       " - Version: %s\n"
                       " - Client APIs: %s\n"
                       " - Extensions:\n"
                       "\t%s",
                       display_wayland->egl_major_version,
                       display_wayland->egl_minor_version,
                       eglQueryString (dpy, EGL_VENDOR),
                       eglQueryString (dpy, EGL_VERSION),
                       eglQueryString (dpy, EGL_CLIENT_APIS),
                       eglQueryString (dpy, EGL_EXTENSIONS)));

  ctx = g_object_new (GDK_TYPE_WAYLAND_GL_CONTEXT,
                      "display", display,
                      NULL);

  gdk_profiler_end_mark (start_time, "init Wayland GL", NULL);

  return ctx;
}

static void
gdk_wayland_gl_context_dispose (GObject *gobject)
{
  GdkWaylandGLContext *context_wayland = GDK_WAYLAND_GL_CONTEXT (gobject);

  if (context_wayland->egl_context != NULL)
    {
      GdkGLContext *context = GDK_GL_CONTEXT (gobject);
      GdkSurface *surface = gdk_gl_context_get_surface (context);
      GdkDisplay *display = gdk_surface_get_display (surface);
      GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

      if (eglGetCurrentContext () == context_wayland->egl_context)
        eglMakeCurrent(display_wayland->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);

      GDK_DISPLAY_NOTE (display, OPENGL, g_message ("Destroying EGL context"));

      eglDestroyContext (display_wayland->egl_display,
                         context_wayland->egl_context);
      context_wayland->egl_context = NULL;
    }

  G_OBJECT_CLASS (gdk_wayland_gl_context_parent_class)->dispose (gobject);
}

