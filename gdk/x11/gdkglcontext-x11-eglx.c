/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext-x11.c: X11 specific OpenGL wrappers
 *
 * Copyright © 2014  Emmanuele Bassi
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

#include "gdkglcontext-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkscreen-x11.h"

#include "gdkx11display.h"
#include "gdkx11glcontext.h"
#include "gdkx11screen.h"
#include "gdkx11window.h"
#include "gdkx11visual.h"
#include "gdkvisualprivate.h"
#include "gdkx11property.h"
#include <X11/Xatom.h>

#include "gdkinternals.h"

#include "gdkintl.h"

#include <cairo/cairo-xlib.h>

#include <epoxy/egl.h>

struct _GdkX11GLContext
{
  GdkGLContext parent_instance;

  EGLDisplay egl_display;
  EGLContext egl_context;
  EGLConfig egl_config;

  guint is_attached : 1;
  guint do_frame_sync : 1;
};

struct _GdkX11GLContextClass
{
  GdkGLContextClass parent_class;
};

typedef struct {
  EGLDisplay egl_display;
  EGLConfig egl_config;
  EGLSurface egl_surface;
} DrawableInfo;

static gboolean gdk_x11_display_init_gl (GdkDisplay *display);

G_DEFINE_TYPE (GdkX11GLContext, gdk_x11_gl_context, GDK_TYPE_GL_CONTEXT)

static EGLDisplay
gdk_x11_display_get_egl_display (GdkDisplay *display)
{
  EGLDisplay dpy = NULL;

  dpy = g_object_get_data (G_OBJECT (display), "-gdk-x11-egl-display");
  if (dpy != NULL)
    return dpy;

  if (epoxy_has_egl_extension (NULL, "EGL_KHR_platform_base"))
    {
      PFNEGLGETPLATFORMDISPLAYPROC getPlatformDisplay =
        (void *) eglGetProcAddress ("eglGetPlatformDisplay");

      if (getPlatformDisplay)
        dpy = getPlatformDisplay (EGL_PLATFORM_X11_KHR,
                                  gdk_x11_display_get_xdisplay (display),
                                  NULL);
      if (dpy != NULL)
        goto out;
    }

  if (epoxy_has_egl_extension (NULL, "EGL_EXT_platform_base"))
    {
      PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay =
        (void *) eglGetProcAddress ("eglGetPlatformDisplayEXT");

      if (getPlatformDisplay)
        dpy = getPlatformDisplay (EGL_PLATFORM_X11_EXT,
                                  gdk_x11_display_get_xdisplay (display),
                                  NULL);
      if (dpy != NULL)
        goto out;
    }

  dpy = eglGetDisplay ((EGLNativeDisplayType) gdk_x11_display_get_xdisplay (display));

out:
  if (dpy != NULL)
    g_object_set_data (G_OBJECT (display), "-gdk-x11-egl-display", dpy);

  return dpy;
}

typedef struct {
  EGLDisplay egl_display;
  EGLConfig egl_config;
  EGLSurface egl_surface;

  Display *xdisplay;
  Window dummy_xwin;
  XVisualInfo *xvisinfo;
} DummyInfo;

static void
dummy_info_free (gpointer data)
{
  DummyInfo *info = data;

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

  g_slice_free (DummyInfo, info);
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
  DummyInfo *info;
  XVisualInfo *xvisinfo;
  XSetWindowAttributes attrs;

  info = g_object_get_data (G_OBJECT (display), "-gdk-x11-egl-dummy-surface");
  if (info != NULL)
    return info->egl_surface;

  xvisinfo = get_visual_info_for_egl_config (display, egl_config);
  if (xvisinfo == NULL)
    return NULL;

  info = g_slice_new (DummyInfo);
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
                          dummy_info_free);

  return info->egl_surface;
}

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

  g_slice_free (DrawableInfo, data);
}

static EGLSurface
gdk_x11_window_get_egl_surface (GdkWindow *window,
                                EGLConfig  config)
{
  GdkDisplay *display = gdk_window_get_display (window);
  EGLDisplay egl_display = gdk_x11_display_get_egl_display (display);
  DrawableInfo *info;

  info = g_object_get_data (G_OBJECT (window), "-gdk-x11-egl-drawable");
  if (info != NULL)
    return info->egl_surface;

  info = g_slice_new (DrawableInfo);
  info->egl_display = egl_display;
  info->egl_config = config;
  info->egl_surface =
    eglCreateWindowSurface (info->egl_display, config,
                            (EGLNativeWindowType) gdk_x11_window_get_xid (window),
                            NULL);

  g_object_set_data_full (G_OBJECT (window), "-gdk-x11-egl-drawable",
                          info,
                          drawable_info_free);

  return info->egl_surface;
}

void
gdk_x11_window_invalidate_for_new_frame (GdkWindow      *window,
                                         cairo_region_t *update_area)
{
  cairo_rectangle_int_t window_rect;
  GdkDisplay *display = gdk_window_get_display (window);
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  GdkX11GLContext *context_x11;
  EGLint buffer_age;
  gboolean invalidate_all;

  /* Minimal update is ok if we're not drawing with gl */
  if (window->gl_paint_context == NULL)
    return;

  context_x11 = GDK_X11_GL_CONTEXT (window->gl_paint_context);

  buffer_age = 0;

  if (display_x11->has_buffer_age)
    {
      gdk_gl_context_make_current (window->gl_paint_context);

      eglQuerySurface (gdk_x11_display_get_egl_display (display),
                       gdk_x11_window_get_egl_surface (window, context_x11->egl_config),
                       EGL_BUFFER_AGE_EXT,
                       &buffer_age);
    }


  invalidate_all = FALSE;
  if (buffer_age == 0 || buffer_age >= 4)
    {
      invalidate_all = TRUE;
    }
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
gdk_x11_gl_context_end_frame (GdkGLContext   *context,
                              cairo_region_t *painted,
                              cairo_region_t *damage)
{
  GdkX11GLContext *context_x11 = GDK_X11_GL_CONTEXT (context);
  GdkWindow *window = gdk_gl_context_get_window (context);
  GdkDisplay *display = gdk_window_get_display (window);
  EGLDisplay edpy = gdk_x11_display_get_egl_display (display);
  EGLSurface esurface;

  gdk_gl_context_make_current (context);

  esurface = gdk_x11_window_get_egl_surface (window, context_x11->egl_config);

  if (GDK_X11_DISPLAY (display)->has_swap_buffers_with_damage)
    {
      int i, j, n_rects = cairo_region_num_rectangles (damage);
      int window_height = gdk_window_get_height (window);
      gboolean free_rects = FALSE;
      cairo_rectangle_int_t rect;
      EGLint *rects;
      
      if (n_rects < 16)
        rects = g_newa (EGLint, n_rects * 4);
      else
        {
          rects = g_new (EGLint, n_rects * 4);
          free_rects = TRUE;
        }

      for (i = 0, j = 0; i < n_rects; i++)
        {
          cairo_region_get_rectangle (damage, i, &rect);
          rects[j++] = rect.x;
          rects[j++] = window_height - rect.height - rect.y;
          rects[j++] = rect.width;
          rects[j++] = rect.height;
        }

      eglSwapBuffersWithDamageEXT (edpy, esurface, rects, n_rects);

      if (free_rects)
        g_free (rects);
    }
  else
    eglSwapBuffers (edpy, esurface);
}

#define N_EGL_ATTRS 16

static gboolean
gdk_x11_gl_context_realize (GdkGLContext  *context,
                            GError       **error)
{
  GdkX11Display *display_x11;
  GdkDisplay *display;
  GdkX11GLContext *context_x11;
  GdkGLContext *share;
  GdkWindow *window;
  gboolean debug_bit, compat_bit, legacy_bit, es_bit;
  int major, minor;
  EGLint context_attrs[N_EGL_ATTRS];

  window = gdk_gl_context_get_window (context);
  display = gdk_window_get_display (window);

  if (!gdk_x11_display_init_gl (display))
    {
      g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL implementation available"));
      return FALSE;
    }

  context_x11 = GDK_X11_GL_CONTEXT (context);
  display_x11 = GDK_X11_DISPLAY (display);
  share = gdk_gl_context_get_shared_context (context);

  gdk_gl_context_get_required_version (context, &major, &minor);
  debug_bit = gdk_gl_context_get_debug_enabled (context);
  compat_bit = gdk_gl_context_get_forward_compatible (context);

  legacy_bit = !display_x11->has_create_context ||
               (_gdk_gl_flags & GDK_GL_LEGACY) != 0;

  es_bit = ((_gdk_gl_flags & GDK_GL_GLES) != 0 ||
            (share != NULL && gdk_gl_context_get_use_es (share)));

  if (es_bit)
    {
      context_attrs[0] = EGL_CONTEXT_CLIENT_VERSION;
      context_attrs[1] = major == 3 ? 3 : 2;
      context_attrs[2] = EGL_NONE;

      eglBindAPI (EGL_OPENGL_ES_API);
    }
  else
    {
      int flags = 0;

      if (!display_x11->has_create_context)
        {
          context_attrs[0] = EGL_NONE;
        }
      else
        {
          if (debug_bit)
            flags |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
          if (compat_bit)
            flags |= EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;

          context_attrs[0] = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
          context_attrs[1] = legacy_bit
                           ? EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR
                           : EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;
          context_attrs[2] = EGL_CONTEXT_MAJOR_VERSION_KHR;
          context_attrs[3] = legacy_bit ? 3 : major;
          context_attrs[4] = EGL_CONTEXT_MINOR_VERSION_KHR;
          context_attrs[5] = legacy_bit ? 0 : minor;
          context_attrs[6] = EGL_CONTEXT_FLAGS_KHR;
          context_attrs[7] = flags;
          context_attrs[8] = EGL_NONE;
        }

      eglBindAPI (EGL_OPENGL_API);
    }

  GDK_NOTE (OPENGL,
            g_message ("Creating EGL context (version:%d.%d, debug:%s, forward:%s, legacy:%s, es:%s)",
                       major, minor,
                       debug_bit ? "yes" : "no",
                       compat_bit ? "yes" : "no",
                       legacy_bit ? "yes" : "no",
                       es_bit ? "yes" : "no"));

  context_x11->egl_context =
    eglCreateContext (gdk_x11_display_get_egl_display (display),
                      context_x11->egl_config,
                      share != NULL ? GDK_X11_GL_CONTEXT (share)->egl_context
                                    : EGL_NO_CONTEXT,
                      context_attrs);

  /* If we're not asking for a GLES context, and we don't have the legacy bit set
   * already, try again with a legacy context
   */
  if (context_x11->egl_context == NULL && !es_bit && !legacy_bit)
    {
      context_attrs[1] = EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR;
      context_attrs[3] = 3;
      context_attrs[5] = 0;

      legacy_bit = TRUE;
      es_bit = FALSE;

      GDK_NOTE (OPENGL,
                g_message ("Context creation failed; trying legacy EGL context"));

      context_x11->egl_context =
        eglCreateContext (gdk_x11_display_get_egl_display (display),
                          context_x11->egl_config,
                          share != NULL ? GDK_X11_GL_CONTEXT (share)->egl_context
                                        : EGL_NO_CONTEXT,
                          context_attrs);
    }

  if (context_x11->egl_context == NULL)
    {
      g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Unable to create a GL context"));
      return FALSE;
    }

  gdk_gl_context_set_is_legacy (context, legacy_bit);
  gdk_gl_context_set_use_es (context, es_bit);

  GDK_NOTE (OPENGL,
            g_message ("Realized EGL context[%p]",
                       context_x11->egl_context));

  return TRUE;
}

static void
gdk_x11_gl_context_dispose (GObject *gobject)
{
  GdkX11GLContext *context_x11 = GDK_X11_GL_CONTEXT (gobject);

  if (context_x11->egl_context != NULL)
    {
      GdkGLContext *context = GDK_GL_CONTEXT (gobject);
      GdkDisplay *display = gdk_gl_context_get_display (context);

      if (eglGetCurrentContext () != context_x11->egl_context)
        eglMakeCurrent (gdk_x11_display_get_egl_display (display),
                        EGL_NO_SURFACE,
                        EGL_NO_SURFACE,
                        EGL_NO_CONTEXT);

      GDK_NOTE (OPENGL, g_message ("Destroying EGL context"));
      eglDestroyContext (gdk_x11_display_get_egl_display (display),
                         context_x11->egl_context);
      context_x11->egl_context = NULL;
    }

  G_OBJECT_CLASS (gdk_x11_gl_context_parent_class)->dispose (gobject);
}

static void
gdk_x11_gl_context_class_init (GdkX11GLContextClass *klass)
{
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  context_class->realize = gdk_x11_gl_context_realize;
  context_class->end_frame = gdk_x11_gl_context_end_frame;

  gobject_class->dispose = gdk_x11_gl_context_dispose;
}

static void
gdk_x11_gl_context_init (GdkX11GLContext *self)
{
  self->do_frame_sync = TRUE;
}

static gboolean
gdk_x11_display_init_gl (GdkDisplay *display)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  EGLDisplay edpy;
  int major, minor;

  if (display_x11->supports_gl)
    return TRUE;

  if (_gdk_gl_flags & GDK_GL_DISABLE)
    return FALSE;

  edpy = gdk_x11_display_get_egl_display (display);
  if (edpy == NULL)
    return FALSE;

  if (!eglInitialize (edpy, &major, &minor))
    return FALSE;

  if (!eglBindAPI (EGL_OPENGL_API))
    return FALSE;

  display_x11->supports_gl = TRUE;

  display_x11->glx_version = major * 10 + minor;
  display_x11->glx_error_base = 0; 
  display_x11->glx_event_base = 0;

  display_x11->has_create_context =
    epoxy_has_egl_extension (edpy, "EGL_KHR_create_context");
  display_x11->has_create_es2_context = FALSE;
  display_x11->has_swap_interval = TRUE;
  display_x11->has_texture_from_pixmap = FALSE;
  display_x11->has_video_sync = FALSE;
  display_x11->has_buffer_age =
    epoxy_has_egl_extension (edpy, "EGL_EXT_buffer_age");
  display_x11->has_sync_control = FALSE;
  display_x11->has_multisample = FALSE;
  display_x11->has_visual_rating = FALSE;
  display_x11->has_swap_buffers_with_damage =
    epoxy_has_egl_extension (edpy, "EGL_EXT_swap_buffers_with_damage");

  GDK_NOTE (OPENGL,
            g_message ("EGL X11 found\n"
                       " - Vendor: %s\n"
                       " - Version: %s\n"
                       " - Client APIs: %s\n"
                       " - Checked extensions:\n"
                       "\t* EGL_KHR_create_context: %s\n"
                       "\t* EGL_EXT_buffer_age: %s\n"
                       "\t* EGL_EXT_swap_buffers_with_damage: %s",
                       eglQueryString (edpy, EGL_VENDOR),
                       eglQueryString (edpy, EGL_VERSION),
                       eglQueryString (edpy, EGL_CLIENT_APIS),
                       display_x11->has_create_context ? "yes" : "no",
                       display_x11->has_buffer_age ? "yes" : "no",
                       display_x11->has_swap_buffers_with_damage ? "yes" : "no"));

  return TRUE;
}

void
_gdk_x11_screen_update_visuals_for_gl (GdkScreen *screen)
{
  /* No-op; there's just no way to do the same trick we use
   * with GLX to select an appropriate visual and cache it.
   * For EGL-X11 we always pick the first visual and stick
   * with it.
   */
}

#define MAX_EGL_ATTRS 30

static gboolean
find_egl_config_for_window (GdkWindow  *window,
                            EGLConfig  *config_out,
                            GError    **error)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkVisual *visual = gdk_window_get_visual (window);
  EGLint attrs[MAX_EGL_ATTRS];
  EGLint count;
  EGLDisplay egl_display;
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

  use_rgba = (visual == gdk_screen_get_rgba_visual (gdk_window_get_screen (window)));

  if (use_rgba)
    {
      attrs[i++] = EGL_ALPHA_SIZE;
      attrs[i++] = 1;
    }
  else
    {
      attrs[i++] = EGL_ALPHA_SIZE;
      attrs[i++] = EGL_DONT_CARE;
    }

  attrs[i++] = EGL_NONE;
  g_assert (i < MAX_EGL_ATTRS);

  egl_display = gdk_x11_display_get_egl_display (display);
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
      g_free (configs);
      return FALSE;
    }

  if (config_out != NULL)
    *config_out = configs[0];

  g_free (configs);

  return TRUE;
}

GdkGLContext *
gdk_x11_window_create_gl_context (GdkWindow    *window,
                                  gboolean      attached,
                                  GdkGLContext *share,
                                  GError      **error)
{
  GdkDisplay *display;
  GdkX11GLContext *context;
  EGLConfig config;

  display = gdk_window_get_display (window);

  if (!gdk_x11_display_init_gl (display))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL implementation is available"));
      return NULL;
    }

  if (!find_egl_config_for_window (window, &config, error))
    return NULL;

  context = g_object_new (GDK_TYPE_X11_GL_CONTEXT,
                          "display", display,
                          "window", window,
                          "shared-context", share,
                          NULL);

  context->egl_config = config;
  context->is_attached = attached;

  return GDK_GL_CONTEXT (context);
}

gboolean
gdk_x11_display_make_gl_context_current (GdkDisplay   *display,
                                         GdkGLContext *context)
{
  GdkX11GLContext *context_x11;
  GdkWindow *window;
  gboolean do_frame_sync = FALSE;
  EGLSurface surface;
  EGLDisplay egl_display;

  egl_display = gdk_x11_display_get_egl_display (display);

  if (context == NULL)
    {
      eglMakeCurrent (egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      return TRUE;
    }

  window = gdk_gl_context_get_window (context);
  context_x11 = GDK_X11_GL_CONTEXT (context);
  if (context_x11->egl_context == NULL)
    {
      g_critical ("No EGL context associated to the GdkGLContext; you must "
                  "call gdk_gl_context_realize() first.");
      return FALSE;
    }

  GDK_NOTE (OPENGL,
            g_message ("Making EGL context current"));

  if (context_x11->is_attached)
    surface = gdk_x11_window_get_egl_surface (window, context_x11->egl_config);
  else
    surface = gdk_x11_display_get_egl_dummy_surface (display, context_x11->egl_config);

  if (!eglMakeCurrent (egl_display, surface, surface, context_x11->egl_context))
    {
      GDK_NOTE (OPENGL,
                g_message ("Making EGL context current failed"));
      return FALSE;
    }

  if (context_x11->is_attached)
    {
      /* If the WM is compositing there is no particular need to delay
       * the swap when drawing on the offscreen, rendering to the screen
       * happens later anyway, and its up to the compositor to sync that
       * to the vblank. */
      GdkScreen *screen = gdk_window_get_screen (window);

      do_frame_sync = ! gdk_screen_is_composited (screen);

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
 * gdk_x11_display_get_glx_version:
 * @display: a #GdkDisplay
 * @major: (out): return location for the GLX major version
 * @minor: (out): return location for the GLX minor version
 *
 * Retrieves the version of the GLX implementation.
 *
 * Returns: %TRUE if GLX is available
 *
 * Since: 3.16
 */
gboolean
gdk_x11_display_get_glx_version (GdkDisplay *display,
                                 gint       *major,
                                 gint       *minor)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  if (!GDK_IS_X11_DISPLAY (display))
    return FALSE;

  if (!gdk_x11_display_init_gl (display))
    return FALSE;

  if (major != NULL)
    *major = GDK_X11_DISPLAY (display)->glx_version / 10;
  if (minor != NULL)
    *minor = GDK_X11_DISPLAY (display)->glx_version % 10;

  return TRUE;
}
