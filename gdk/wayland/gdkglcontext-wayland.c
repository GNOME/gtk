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
#include "gdkwaylandsurface.h"
#include "gdkprivate-wayland.h"

#include "gdkinternals.h"
#include "gdksurfaceprivate.h"

#include "gdkintl.h"

G_DEFINE_TYPE (GdkWaylandGLContext, gdk_wayland_gl_context, GDK_TYPE_GL_CONTEXT)

static void gdk_wayland_gl_context_dispose (GObject *gobject);

#define N_EGL_ATTRS     16

static int in_shared_data_creation;

static GdkGLContext *
ensure_shared_data_context (GdkSurface *surface)
{ 
  GdkDisplay *display;
  GdkGLContext *context;

  if (in_shared_data_creation)
    return NULL;

  in_shared_data_creation = 1;

  display = gdk_surface_get_display (surface);
  context = (GdkGLContext *)g_object_get_data (G_OBJECT (display), "shared_data_gl_context");
  if (context == NULL)
    {
      GError *error = NULL;
      context = GDK_SURFACE_GET_CLASS (surface)->create_gl_context (surface, FALSE, NULL, &error);
      if (context == NULL)
        {
          g_warning ("Failed to create shared context: %s", error->message);
          g_clear_error (&error);
        }

      gdk_gl_context_realize (context, &error);
      if (context == NULL)
        {
          g_warning ("Failed to realize shared context: %s", error->message);
          g_clear_error (&error);
        }


      g_object_set_data (G_OBJECT (display), "shared_data_gl_context", context);
    }

  in_shared_data_creation = 0;

  return context;
}

static gboolean
gdk_wayland_gl_context_realize (GdkGLContext *context,
                                GError      **error)
{
  GdkWaylandGLContext *context_wayland = GDK_WAYLAND_GL_CONTEXT (context);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  GdkGLContext *share = gdk_gl_context_get_shared_context (context);
  GdkGLContext *shared_data_context = gdk_surface_get_shared_data_gl_context (gdk_gl_context_get_surface (context));
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  EGLContext ctx;
  EGLint context_attribs[N_EGL_ATTRS];
  int major, minor, flags;
  gboolean debug_bit, forward_bit, legacy_bit, use_es;
  int i = 0;

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
                          context_wayland->egl_config,
                          share != NULL ? GDK_WAYLAND_GL_CONTEXT (share)->egl_context
                             : shared_data_context != NULL ? GDK_WAYLAND_GL_CONTEXT (shared_data_context)->egl_context
                                                  : EGL_NO_CONTEXT,
                          context_attribs);

  /* If context creation failed without the legacy bit, let's try again with it */
  if (ctx == NULL && !legacy_bit)
    {
      /* Ensure that re-ordering does not break the offsets */
      g_assert (context_attribs[0] == EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR);
      context_attribs[1] = EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR;
      context_attribs[3] = 3;
      context_attribs[5] = 0;

      eglBindAPI (EGL_OPENGL_API);

      legacy_bit = TRUE;
      use_es = FALSE;

      GDK_DISPLAY_NOTE (display, OPENGL,
                g_message ("eglCreateContext failed, switching to legacy"));
      ctx = eglCreateContext (display_wayland->egl_display,
                              context_wayland->egl_config,
                              share != NULL ? GDK_WAYLAND_GL_CONTEXT (share)->egl_context
                                 : shared_data_context != NULL ? GDK_WAYLAND_GL_CONTEXT (shared_data_context)->egl_context
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
      GdkGLContext *shared;
      GdkWaylandGLContext *shared_wayland;

      shared = gdk_gl_context_get_shared_context (context);
      if (shared == NULL)
        shared = context;
      shared_wayland = GDK_WAYLAND_GL_CONTEXT (shared);

      egl_surface = gdk_wayland_surface_get_egl_surface (surface,
                                                        shared_wayland->egl_config);
      gdk_gl_context_make_current (shared);
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

static void
gdk_wayland_gl_context_end_frame (GdkDrawContext *draw_context,
                                  cairo_region_t *painted)
{
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);
  GdkSurface *surface = gdk_gl_context_get_surface (context);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GdkWaylandGLContext *context_wayland = GDK_WAYLAND_GL_CONTEXT (context);
  EGLSurface egl_surface;

  GDK_DRAW_CONTEXT_CLASS (gdk_wayland_gl_context_parent_class)->end_frame (draw_context, painted);
  if (gdk_gl_context_get_shared_context (context))
    return;

  gdk_gl_context_make_current (context);

  egl_surface = gdk_wayland_surface_get_egl_surface (surface,
                                                    context_wayland->egl_config);

  gdk_wayland_surface_sync (surface);
  gdk_wayland_surface_request_frame (surface);

  if (display_wayland->have_egl_swap_buffers_with_damage)
    {
      int i, j, n_rects = cairo_region_num_rectangles (painted);
      EGLint *rects = g_new (EGLint, n_rects * 4);
      cairo_rectangle_int_t rect;
      int surface_height = gdk_surface_get_height (surface);

      for (i = 0, j = 0; i < n_rects; i++)
        {
          cairo_region_get_rectangle (painted, i, &rect);
          rects[j++] = rect.x;
          rects[j++] = surface_height - rect.height - rect.y;
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
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);

  gobject_class->dispose = gdk_wayland_gl_context_dispose;

  draw_context_class->end_frame = gdk_wayland_gl_context_end_frame;

  context_class->realize = gdk_wayland_gl_context_realize;
  context_class->get_damage = gdk_wayland_gl_context_get_damage;
}

static void
gdk_wayland_gl_context_init (GdkWaylandGLContext *self)
{
}

static EGLDisplay
gdk_wayland_get_display (GdkWaylandDisplay *display_wayland)
{
  EGLDisplay dpy = NULL;

  if (epoxy_has_egl_extension (NULL, "EGL_KHR_platform_base"))
    {
      PFNEGLGETPLATFORMDISPLAYPROC getPlatformDisplay =
	(void *) eglGetProcAddress ("eglGetPlatformDisplay");

      if (getPlatformDisplay)
	dpy = getPlatformDisplay (EGL_PLATFORM_WAYLAND_EXT,
				  display_wayland->wl_display,
				  NULL);
      if (dpy)
	return dpy;
    }

  if (epoxy_has_egl_extension (NULL, "EGL_EXT_platform_base"))
    {
      PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay =
	(void *) eglGetProcAddress ("eglGetPlatformDisplayEXT");

      if (getPlatformDisplay)
	dpy = getPlatformDisplay (EGL_PLATFORM_WAYLAND_EXT,
				  display_wayland->wl_display,
				  NULL);
      if (dpy)
	return dpy;
    }

  return eglGetDisplay ((EGLNativeDisplayType) display_wayland->wl_display);
}

gboolean
gdk_wayland_display_init_gl (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  EGLint major, minor;
  EGLDisplay dpy;

  if (display_wayland->have_egl)
    return TRUE;

  dpy = gdk_wayland_get_display (display_wayland);

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

  return TRUE;
}

#define MAX_EGL_ATTRS   30

static gboolean
find_eglconfig_for_surface (GdkSurface  *surface,
                           EGLConfig  *egl_config_out,
                           GError    **error)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  EGLint attrs[MAX_EGL_ATTRS];
  EGLint count;
  EGLConfig *configs;
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
  attrs[i++] = EGL_ALPHA_SIZE;
  attrs[i++] = 1;

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
gdk_wayland_surface_create_gl_context (GdkSurface     *surface,
				      gboolean       attached,
                                      GdkGLContext  *share,
                                      GError       **error)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GdkWaylandGLContext *context;
  EGLConfig config;

  if (!gdk_wayland_display_init_gl (display))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL implementation is available"));
      return NULL;
    }

  if (!display_wayland->have_egl_khr_create_context)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_PROFILE,
                           _("Core GL is not available on EGL implementation"));
      return NULL;
    }

  if (!find_eglconfig_for_surface (surface, &config, error))
    return NULL;

  context = g_object_new (GDK_TYPE_WAYLAND_GL_CONTEXT,
                          "surface", surface,
                          "shared-context", share,
                          NULL);

  context->egl_config = config;
  context->is_attached = attached;

  return GDK_GL_CONTEXT (context);
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

gboolean
gdk_wayland_display_make_gl_context_current (GdkDisplay   *display,
                                             GdkGLContext *context)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GdkWaylandGLContext *context_wayland;
  GdkSurface *surface;
  EGLSurface egl_surface;

  if (context == NULL)
    {
      eglMakeCurrent(display_wayland->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                     EGL_NO_CONTEXT);
      return TRUE;
    }

  context_wayland = GDK_WAYLAND_GL_CONTEXT (context);
  surface = gdk_gl_context_get_surface (context);

  if (context_wayland->is_attached || gdk_draw_context_is_in_frame (GDK_DRAW_CONTEXT (context)))
    egl_surface = gdk_wayland_surface_get_egl_surface (surface, context_wayland->egl_config);
  else
    {
      if (display_wayland->have_egl_surfaceless_context)
	egl_surface = EGL_NO_SURFACE;
      else
	egl_surface = gdk_wayland_surface_get_dummy_egl_surface (surface,
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
