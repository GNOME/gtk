/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext-egl.c: EGL-X11 specific wrappers
 *
 * SPDX-FileCopyrightText: 2014  Emmanuele Bassi
 * SPDX-FileCopyrightText: 2021  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gdkglcontext-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkprivate-x11.h"
#include "gdkscreen-x11.h"

#include "gdkx11display.h"
#include "gdkx11glcontext.h"
#include "gdkx11screen.h"
#include "gdkx11surface.h"
#include "gdkvisual-x11.h"
#include "gdkx11property.h"
#include <X11/Xatom.h>

#include "gdkinternals.h"
#include "gdkprofilerprivate.h"
#include "gdkintl.h"

#include <cairo-xlib.h>

#include <epoxy/egl.h>

struct _GdkX11GLContextEGL
{
  GdkX11GLContext parent_instance;

  EGLDisplay egl_display;
  EGLConfig egl_config;
  EGLContext egl_context;
};

typedef struct {
  EGLDisplay egl_display;
  EGLConfig egl_config;
  EGLSurface egl_surface;

  /* Only set by the dummy surface we attach to the display */
  Display *xdisplay;
  Window dummy_xwin;
  XVisualInfo *xvisinfo;
} DrawableInfo;

typedef struct _GdkX11GLContextClass    GdkX11GLContextEGLClass;

G_DEFINE_TYPE (GdkX11GLContextEGL, gdk_x11_gl_context_egl, GDK_TYPE_X11_GL_CONTEXT)

static void
drawable_info_free (gpointer data)
{
  DrawableInfo *info = data;

  if (data == NULL)
    return;

  if (info->egl_surface != NULL)
    {
      eglDestroySurface (info->egl_display, info->egl_surface);
      info->egl_surface = NULL;
    }

  if (info->dummy_xwin != None)
    {
      XDestroyWindow (info->xdisplay, info->dummy_xwin);
      info->dummy_xwin = None;
    }

  if (info->xvisinfo != NULL)
    {
      XFree (info->xvisinfo);
      info->xvisinfo = NULL;
    }

  g_free (info);
}

/**
 * gdk_x11_display_get_egl_display:
 * @display: (type GdkX11Display): an X11 display
 *
 * Retrieves the EGL display connection object for the given GDK display.
 *
 * This function returns `NULL` if GDK is using GLX.
 *
 * Returns: (nullable): the EGL display object
 *
 * Since: 4.4
 */
gpointer
gdk_x11_display_get_egl_display (GdkDisplay *display)
{
  EGLDisplay edpy = NULL;
  Display *dpy;

  g_return_val_if_fail (GDK_IS_X11_DISPLAY (display), NULL);

  if (GDK_X11_DISPLAY (display)->have_glx)
    return NULL;

  edpy = g_object_get_data (G_OBJECT (display), "-gdk-x11-egl-display");
  if (edpy != NULL)
    return edpy;

  dpy = gdk_x11_display_get_xdisplay (display);

  if (epoxy_has_egl_extension (NULL, "EGL_KHR_platform_base"))
    {
      PFNEGLGETPLATFORMDISPLAYPROC getPlatformDisplay =
        (void *) eglGetProcAddress ("eglGetPlatformDisplay");

      if (getPlatformDisplay != NULL)
        edpy = getPlatformDisplay (EGL_PLATFORM_X11_KHR, dpy, NULL);

      if (edpy != NULL)
        goto out;
    }

  if (epoxy_has_egl_extension (NULL, "EGL_EXT_platform_base"))
    {
      PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay =
        (void *) eglGetProcAddress ("eglGetPlatformDisplayEXT");

      if (getPlatformDisplay)
        edpy = getPlatformDisplay (EGL_PLATFORM_X11_EXT, dpy, NULL);

      if (edpy != NULL)
        goto out;
    }

  edpy = eglGetDisplay ((EGLNativeDisplayType) dpy);

out:
  if (edpy != NULL)
    g_object_set_data (G_OBJECT (display), "-gdk-x11-egl-display", edpy);

  return edpy;
}

static XVisualInfo *
get_visual_info_for_egl_config (GdkDisplay *display,
                                EGLConfig   egl_config)
{
  XVisualInfo visinfo_template;
  int template_mask = 0;
  XVisualInfo *visinfo = NULL;
  int visinfos_count;
  EGLint visualid, red_size, green_size, blue_size, alpha_size;
  EGLDisplay egl_display = gdk_x11_display_get_egl_display (display);

  eglGetConfigAttrib (egl_display, egl_config, EGL_NATIVE_VISUAL_ID, &visualid);

  if (visualid != 0)
    {
      visinfo_template.visualid = visualid;
      template_mask |= VisualIDMask;
    }
  else
    {
      /* some EGL drivers don't implement the EGL_NATIVE_VISUAL_ID
       * attribute, so attempt to find the closest match.
       */
      eglGetConfigAttrib (egl_display, egl_config, EGL_RED_SIZE, &red_size);
      eglGetConfigAttrib (egl_display, egl_config, EGL_GREEN_SIZE, &green_size);
      eglGetConfigAttrib (egl_display, egl_config, EGL_BLUE_SIZE, &blue_size);
      eglGetConfigAttrib (egl_display, egl_config, EGL_ALPHA_SIZE, &alpha_size);

      visinfo_template.depth = red_size + green_size + blue_size + alpha_size;
      template_mask |= VisualDepthMask;

      visinfo_template.screen = DefaultScreen (gdk_x11_display_get_xdisplay (display));
      template_mask |= VisualScreenMask;
    }

  visinfo = XGetVisualInfo (gdk_x11_display_get_xdisplay (display),
                            template_mask,
                            &visinfo_template,
                            &visinfos_count);

  if (visinfos_count < 1)
    return NULL;

  return visinfo;
}

static EGLSurface
gdk_x11_display_get_egl_dummy_surface (GdkDisplay *display,
                                       EGLConfig   egl_config)
{
  DrawableInfo *info;
  XVisualInfo *xvisinfo;
  XSetWindowAttributes attrs;

  info = g_object_get_data (G_OBJECT (display), "-gdk-x11-egl-dummy-surface");
  if (info != NULL)
    return info->egl_surface;

  xvisinfo = get_visual_info_for_egl_config (display, egl_config);
  if (xvisinfo == NULL)
    return NULL;

  info = g_new (DrawableInfo, 1);
  info->xdisplay = gdk_x11_display_get_xdisplay (display);
  info->xvisinfo = xvisinfo;
  info->egl_display = gdk_x11_display_get_egl_display (display);
  info->egl_config = egl_config;

  attrs.override_redirect = True;
  attrs.colormap = XCreateColormap (info->xdisplay,
                                    DefaultRootWindow (info->xdisplay),
                                    xvisinfo->visual,
                                    AllocNone);
  attrs.border_pixel = 0;

  info->dummy_xwin =
    XCreateWindow (info->xdisplay,
                   DefaultRootWindow (info->xdisplay),
                   -100, -100, 1, 1,
                   0,
                   xvisinfo->depth,
                   CopyFromParent,
                   xvisinfo->visual,
                   CWOverrideRedirect | CWColormap | CWBorderPixel,
                   &attrs);

  info->egl_surface =
    eglCreateWindowSurface (info->egl_display,
                            info->egl_config,
                            (EGLNativeWindowType) info->dummy_xwin,
                            NULL);

  g_object_set_data_full (G_OBJECT (display), "-gdk-x11-egl-dummy-surface",
                          info,
                          drawable_info_free);

  return info->egl_surface;
}

static EGLSurface
gdk_x11_surface_get_egl_surface (GdkSurface *surface,
                                 EGLConfig   config)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  EGLDisplay egl_display = gdk_x11_display_get_egl_display (display);
  DrawableInfo *info;

  info = g_object_get_data (G_OBJECT (surface), "-gdk-x11-egl-drawable");
  if (info != NULL)
    return info->egl_surface;

  info = g_new0 (DrawableInfo, 1);
  info->egl_display = egl_display;
  info->egl_config = config;
  info->egl_surface =
    eglCreateWindowSurface (info->egl_display, config,
                            (EGLNativeWindowType) gdk_x11_surface_get_xid (surface),
                            NULL);

  g_object_set_data_full (G_OBJECT (surface), "-gdk-x11-egl-drawable",
                          info,
                          drawable_info_free);

  return info->egl_surface;
}

static void
gdk_x11_gl_context_egl_end_frame (GdkDrawContext *draw_context,
                                  cairo_region_t *painted)
{
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);
  GdkX11GLContextEGL *context_egl = GDK_X11_GL_CONTEXT_EGL (context);
  GdkSurface *surface = gdk_gl_context_get_surface (context);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  EGLDisplay egl_display = gdk_x11_display_get_egl_display (display);
  EGLSurface egl_surface;

  GDK_DRAW_CONTEXT_CLASS (gdk_x11_gl_context_egl_parent_class)->end_frame (draw_context, painted);
  if (gdk_gl_context_get_shared_context (context) != NULL)
    return;

  gdk_gl_context_make_current (context);

  egl_surface = gdk_x11_surface_get_egl_surface (surface, context_egl->egl_config);

  gdk_profiler_add_mark (GDK_PROFILER_CURRENT_TIME, 0, "x11", "swap buffers");
  if (display_x11->has_egl_swap_buffers_with_damage)
    {
      int i, j, n_rects = cairo_region_num_rectangles (painted);
      int surface_height = gdk_surface_get_height (surface);
      int scale = gdk_surface_get_scale_factor (surface);
      EGLint stack_rects[4 * 4]; /* 4 rects */
      EGLint *heap_rects = NULL;
      EGLint *rects;
      
      if (n_rects < G_N_ELEMENTS (stack_rects) / 4)
        rects = (EGLint *) &stack_rects; 
      else
        rects = heap_rects = g_new (EGLint, n_rects * 4);

      for (i = 0, j = 0; i < n_rects; i++)
        {
          cairo_rectangle_int_t rect;

          cairo_region_get_rectangle (painted, i, &rect);

          rects[j++] = rect.x * scale;
          rects[j++] = (surface_height - rect.height - rect.y) * scale;
          rects[j++] = rect.width * scale;
          rects[j++] = rect.height * scale;
        }

      eglSwapBuffersWithDamageEXT (egl_display, egl_surface, rects, n_rects);
      g_free (heap_rects);
    }
  else
    eglSwapBuffers (egl_display, egl_surface);
}

static cairo_region_t *
gdk_x11_gl_context_egl_get_damage (GdkGLContext *context)
{
  GdkDisplay *display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  if (display_x11->has_egl_buffer_age)
    {
      GdkSurface *surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (context));
      GdkGLContext *shared = gdk_gl_context_get_shared_context (context);
      GdkX11GLContextEGL *shared_egl;
      EGLSurface egl_surface;
      int buffer_age = 0;

      shared = gdk_gl_context_get_shared_context (context);
      if (shared == NULL)
        shared = context;
      shared_egl = GDK_X11_GL_CONTEXT_EGL (shared);

      egl_surface = gdk_x11_surface_get_egl_surface (surface, shared_egl->egl_config);
      gdk_gl_context_make_current (shared);

      eglQuerySurface (gdk_x11_display_get_egl_display (display),
                       egl_surface,
                       EGL_BUFFER_AGE_EXT,
                       &buffer_age);

      switch (buffer_age)
        {
        case 1:
          return cairo_region_create ();

        case 2:
          if (context->old_updated_area[0])
            return cairo_region_copy (context->old_updated_area[0]);
          break;

        case 3:
          if (context->old_updated_area[0] && context->old_updated_area[1])
            {
              cairo_region_t *damage = cairo_region_copy (context->old_updated_area[0]);
              cairo_region_union (damage, context->old_updated_area[1]);
              return damage;
            }
          break;

        default:
          break;
        }
    }

  return GDK_GL_CONTEXT_CLASS (gdk_x11_gl_context_egl_parent_class)->get_damage (context);
}

#define N_EGL_ATTRS 16

static gboolean
gdk_x11_gl_context_egl_realize (GdkGLContext  *context,
                                GError       **error)
{
  GdkX11Display *display_x11;
  GdkDisplay *display;
  GdkX11GLContextEGL *context_egl;
  GdkGLContext *share, *shared_data_context;
  GdkSurface *surface;
  gboolean debug_bit, forward_bit, legacy_bit, use_es;
  int major, minor, i = 0;
  EGLint context_attrs[N_EGL_ATTRS];
  EGLDisplay egl_display;

  surface = gdk_gl_context_get_surface (context);
  display = gdk_surface_get_display (surface);

  context_egl = GDK_X11_GL_CONTEXT_EGL (context);
  display_x11 = GDK_X11_DISPLAY (display);
  share = gdk_gl_context_get_shared_context (context);
  shared_data_context = gdk_surface_get_shared_data_gl_context (surface);

  gdk_gl_context_get_required_version (context, &major, &minor);
  debug_bit = gdk_gl_context_get_debug_enabled (context);
  forward_bit = gdk_gl_context_get_forward_compatible (context);
  legacy_bit = GDK_DISPLAY_DEBUG_CHECK (display, GL_LEGACY) ||
               (share != NULL && gdk_gl_context_is_legacy (share));
  use_es = GDK_DISPLAY_DEBUG_CHECK (display, GL_GLES) ||
           (share != NULL && gdk_gl_context_get_use_es (share));

  if (!use_es)
    {
      eglBindAPI (EGL_OPENGL_API);

      if (display_x11->has_egl_khr_create_context)
        {
          int flags = 0;

          if (debug_bit)
            flags |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
          if (forward_bit)
            flags |= EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;

          context_attrs[i++] = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
          context_attrs[i++] = legacy_bit
                             ? EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR
                             : EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;
          context_attrs[i++] = EGL_CONTEXT_MAJOR_VERSION_KHR;
          context_attrs[i++] = legacy_bit ? 3 : major;
          context_attrs[i++] = EGL_CONTEXT_MINOR_VERSION_KHR;
          context_attrs[i++] = legacy_bit ? 0 : minor;
          context_attrs[i++] = EGL_CONTEXT_FLAGS_KHR;
          context_attrs[i++] = flags;
          context_attrs[i++] = EGL_NONE;
        }
      else
        {
          context_attrs[i++] = EGL_NONE;
        }
    }
  else
    {
      eglBindAPI (EGL_OPENGL_ES_API);

      context_attrs[i++] = EGL_CONTEXT_CLIENT_VERSION;
      if (major == 3)
        context_attrs[i++] = 3;
      else
        context_attrs[i++] = 2;
    }

  context_attrs[i++] = EGL_NONE;
  g_assert (i < N_EGL_ATTRS);

  GDK_DISPLAY_NOTE (display, OPENGL,
                    g_message ("Creating EGL context version %d.%d (shared:%s, debug:%s, forward:%s, legacy:%s, es:%s)",
                               major, minor,
                               share != NULL ? "yes" : "no",
                               debug_bit ? "yes" : "no",
                               forward_bit ? "yes" : "no",
                               legacy_bit ? "yes" : "no",
                               use_es ? "yes" : "no"));

  egl_display = gdk_x11_display_get_egl_display (display);

  context_egl->egl_context =
    eglCreateContext (egl_display,
                      context_egl->egl_config,
                      share != NULL
                        ? GDK_X11_GL_CONTEXT_EGL (share)->egl_context
                        : shared_data_context != NULL
                            ? GDK_X11_GL_CONTEXT_EGL (shared_data_context)->egl_context
                            : EGL_NO_CONTEXT,
                      context_attrs);

  /* If we're not asking for a GLES context, and we don't have the legacy bit set
   * already, try again with a legacy context
   */
  if (context_egl->egl_context == NULL && !use_es && !legacy_bit)
    {
      context_attrs[1] = EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR;
      context_attrs[3] = 3;
      context_attrs[5] = 0;

      legacy_bit = TRUE;
      use_es = FALSE;

      GDK_NOTE (OPENGL,
                g_message ("Context creation failed; trying legacy EGL context"));

      context_egl->egl_context =
        eglCreateContext (egl_display,
                          context_egl->egl_config,
                          share != NULL
                            ? GDK_X11_GL_CONTEXT_EGL (share)->egl_context
                            : shared_data_context != NULL
                                ? GDK_X11_GL_CONTEXT_EGL (shared_data_context)->egl_context
                                : EGL_NO_CONTEXT,
                          context_attrs);
    }

  if (context_egl->egl_context == NULL)
    {
      g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Unable to create a GL context"));
      return FALSE;
    }

  gdk_gl_context_set_is_legacy (context, legacy_bit);
  gdk_gl_context_set_use_es (context, use_es);

  GDK_NOTE (OPENGL,
            g_message ("Realized EGL context[%p]",
                       context_egl->egl_context));

  return TRUE;
}

#undef N_EGL_ATTRS

static void
gdk_x11_gl_context_egl_dispose (GObject *gobject)
{
  GdkX11GLContextEGL *context_egl = GDK_X11_GL_CONTEXT_EGL (gobject);

  if (context_egl->egl_context != NULL)
    {
      GdkGLContext *context = GDK_GL_CONTEXT (gobject);
      GdkDisplay *display = gdk_gl_context_get_display (context);
      EGLDisplay egl_display = gdk_x11_display_get_egl_display (display);

      /* Unset the current context if we're disposing it */
      if (eglGetCurrentContext () == context_egl->egl_context)
        eglMakeCurrent (egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

      GDK_NOTE (OPENGL, g_message ("Destroying EGL context"));
      eglDestroyContext (egl_display, context_egl->egl_context);
      context_egl->egl_context = NULL;
    }

  G_OBJECT_CLASS (gdk_x11_gl_context_egl_parent_class)->dispose (gobject);
}

static void
gdk_x11_gl_context_egl_class_init (GdkX11GLContextEGLClass *klass)
{
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  context_class->realize = gdk_x11_gl_context_egl_realize;
  context_class->get_damage = gdk_x11_gl_context_egl_get_damage;

  draw_context_class->end_frame = gdk_x11_gl_context_egl_end_frame;

  gobject_class->dispose = gdk_x11_gl_context_egl_dispose;
}

static void
gdk_x11_gl_context_egl_init (GdkX11GLContextEGL *self)
{
}

gboolean
gdk_x11_screen_init_egl (GdkX11Screen *screen)
{
  GdkDisplay *display = GDK_SCREEN_DISPLAY (screen);
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  EGLDisplay edpy;
  Display *dpy;
  int major, minor;

  if (display_x11->have_egl)
    return TRUE;

  dpy = gdk_x11_display_get_xdisplay (display);

  if (!epoxy_has_egl ())
    return FALSE;

  edpy = gdk_x11_display_get_egl_display (display);
  if (edpy == NULL)
    return FALSE;

  if (!eglInitialize (edpy, &major, &minor))
    return FALSE;

  /* While NVIDIA might support EGL, it might very well not support
   * all the EGL subset we rely on; we should be looking at more
   * EGL extensions, but for the time being, this is a blanket
   * fallback to GLX
   */
  const char *vendor = eglQueryString (edpy, EGL_VENDOR);
  if (strstr (vendor, "NVIDIA") != NULL)
    {
      eglTerminate (edpy);
      return FALSE;
    }

  display_x11->have_egl = TRUE;
  display_x11->egl_version = epoxy_egl_version (dpy);

  display_x11->has_egl_khr_create_context =
    epoxy_has_egl_extension (edpy, "EGL_KHR_create_context");
  display_x11->has_egl_buffer_age =
    epoxy_has_egl_extension (edpy, "EGL_EXT_buffer_age");
  display_x11->has_egl_swap_buffers_with_damage =
    epoxy_has_egl_extension (edpy, "EGL_EXT_swap_buffers_with_damage");
  display_x11->has_egl_surfaceless_context =
    epoxy_has_egl_extension (edpy, "EGL_KHR_surfaceless_context");

  GDK_DISPLAY_NOTE (display, OPENGL,
                    g_message ("EGL found\n"
                               " - Version: %s\n"
                               " - Vendor: %s\n"
                               " - Client API: %s\n"
                               " - Checked extensions:\n"
                               "\t* EGL_KHR_create_context: %s\n"
                               "\t* EGL_EXT_buffer_age: %s\n"
                               "\t* EGL_EXT_swap_buffers_with_damage: %s\n"
                               "\t* EGL_KHR_surfaceless_context: %s\n",
                               eglQueryString (edpy, EGL_VERSION),
                               eglQueryString (edpy, EGL_VENDOR),
                               eglQueryString (edpy, EGL_CLIENT_APIS),
                               display_x11->has_egl_khr_create_context ? "yes" : "no",
                               display_x11->has_egl_buffer_age ? "yes" : "no",
                               display_x11->has_egl_swap_buffers_with_damage ? "yes" : "no",
                               display_x11->has_egl_surfaceless_context ? "yes" : "no"));

  return TRUE;
}

#define MAX_EGL_ATTRS   30

static gboolean
find_eglconfig_for_display (GdkDisplay  *display,
                            EGLConfig   *egl_config_out,
                            GError     **error)
{
  EGLint attrs[MAX_EGL_ATTRS];
  EGLint count;
  EGLConfig egl_config;
  EGLDisplay egl_display;
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

  /* Pick first valid configuration that the driver returns us */
  egl_display = gdk_x11_display_get_egl_display (display);
  if (!eglChooseConfig (egl_display, attrs, &egl_config, 1, &count) || count < 1)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                           _("No available configurations for the given pixel format"));
      return FALSE;
    }

  g_assert (egl_config_out);
  *egl_config_out = egl_config;

  return TRUE;
}

#undef MAX_EGL_ATTRS

GdkX11GLContext *
gdk_x11_gl_context_egl_new (GdkSurface    *surface,
                            gboolean       attached,
                            GdkGLContext  *share,
                            GError       **error)
{
  GdkDisplay *display;
  GdkX11GLContextEGL *context;
  EGLConfig egl_config;

  display = gdk_surface_get_display (surface);

  if (!find_eglconfig_for_display (display, &egl_config, error))
    return NULL;

  context = g_object_new (GDK_TYPE_X11_GL_CONTEXT_EGL,
                          "surface", surface,
                          "shared-context", share,
                          NULL);

  context->egl_config = egl_config;

  return GDK_X11_GL_CONTEXT (context);
}

gboolean
gdk_x11_gl_context_egl_make_current (GdkDisplay   *display,
                                     GdkGLContext *context)
{
  GdkX11GLContextEGL *context_egl = GDK_X11_GL_CONTEXT_EGL (context);
  GdkX11GLContext *context_x11 = GDK_X11_GL_CONTEXT (context);
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  GdkSurface *surface;
  EGLDisplay egl_display;
  EGLSurface egl_surface;

  egl_display = gdk_x11_display_get_egl_display (display);

  if (context == NULL)
    {
      eglMakeCurrent (egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      return TRUE;
    }

  if (context_egl->egl_context == NULL)
    {
      g_critical ("No EGL context associated to the GdkGLContext; you must "
                  "call gdk_gl_context_realize() first.");
      return FALSE;
    }

  surface = gdk_gl_context_get_surface (context);

  if (context_x11->is_attached || gdk_draw_context_is_in_frame (GDK_DRAW_CONTEXT (context)))
    egl_surface = gdk_x11_surface_get_egl_surface (surface, context_egl->egl_config);
  else
    {
      if (display_x11->has_egl_surfaceless_context)
        egl_surface = EGL_NO_SURFACE;
      else
        egl_surface = gdk_x11_display_get_egl_dummy_surface (display, context_egl->egl_config);
    }

  GDK_DISPLAY_NOTE (display, OPENGL,
                    g_message ("Making EGL context %p current to surface %p",
                               context_egl->egl_context, egl_surface));

  if (!eglMakeCurrent (egl_display, egl_surface, egl_surface, context_egl->egl_context))
    {
      GDK_DISPLAY_NOTE (display, OPENGL,
                        g_message ("Making EGL context current failed"));
      return FALSE;
    }

  if (context_x11->is_attached)
    {
      gboolean do_frame_sync = FALSE;

      /* If the WM is compositing there is no particular need to delay
       * the swap when drawing on the offscreen, rendering to the screen
       * happens later anyway, and its up to the compositor to sync that
       * to the vblank. */
      do_frame_sync = ! gdk_display_is_composited (display);

      if (do_frame_sync != context_x11->do_frame_sync)
        {
          context_x11->do_frame_sync = do_frame_sync;

          if (do_frame_sync)
            eglSwapInterval (egl_display, 1);
          else
            eglSwapInterval (egl_display, 0);
        }
    }

  return TRUE;
}

/**
 * gdk_x11_display_get_egl_version:
 * @display: (type GdkX11Display): a `GdkDisplay`
 * @major: (out): return location for the EGL major version
 * @minor: (out): return location for the EGL minor version
 *
 * Retrieves the version of the EGL implementation.
 *
 * Returns: %TRUE if EGL is available
 *
 * Since: 4.4
 */
gboolean
gdk_x11_display_get_egl_version (GdkDisplay *display,
                                 int        *major,
                                 int        *minor)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  if (!GDK_IS_X11_DISPLAY (display))
    return FALSE;

  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  if (display_x11->have_glx)
    return FALSE;

  if (!gdk_x11_screen_init_egl (display_x11->screen))
    return FALSE;

  if (major != NULL)
    *major = display_x11->egl_version / 10;
  if (minor != NULL)
    *minor = display_x11->egl_version % 10;

  return TRUE;
}
