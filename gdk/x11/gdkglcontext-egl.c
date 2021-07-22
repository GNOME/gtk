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

  EGLContext egl_context;

  guint do_frame_sync : 1;
};

typedef struct _GdkX11GLContextClass    GdkX11GLContextEGLClass;

G_DEFINE_TYPE (GdkX11GLContextEGL, gdk_x11_gl_context_egl, GDK_TYPE_X11_GL_CONTEXT)

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
  GdkX11Display *self;

  g_return_val_if_fail (GDK_IS_X11_DISPLAY (display), NULL);

  self = GDK_X11_DISPLAY (display);

  return self->egl_display;
}

static void
gdk_x11_display_create_egl_display (GdkX11Display *self)
{
  Display *dpy;

  g_assert (self->egl_display == NULL);

  dpy = gdk_x11_display_get_xdisplay (GDK_DISPLAY (self));

  if (epoxy_has_egl_extension (NULL, "EGL_KHR_platform_base"))
    {
      PFNEGLGETPLATFORMDISPLAYPROC getPlatformDisplay =
        (void *) eglGetProcAddress ("eglGetPlatformDisplay");

      if (getPlatformDisplay != NULL)
        self->egl_display = getPlatformDisplay (EGL_PLATFORM_X11_KHR, dpy, NULL);

      if (self->egl_display != NULL)
        return;
    }

  if (epoxy_has_egl_extension (NULL, "EGL_EXT_platform_base"))
    {
      PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay =
        (void *) eglGetProcAddress ("eglGetPlatformDisplayEXT");

      if (getPlatformDisplay)
        self->egl_display = getPlatformDisplay (EGL_PLATFORM_X11_EXT, dpy, NULL);

      if (self->egl_display != NULL)
        return;
    }

  self->egl_display = eglGetDisplay ((EGLNativeDisplayType) dpy);
}

static XVisualInfo *
gdk_x11_display_get_visual_info_for_visual (GdkX11Display  *self,
                                            VisualID        visualid)
{
  XVisualInfo template, *visinfo;
  int nvisuals;

  template.screen = self->screen->screen_num;
  template.visualid = visualid;

  visinfo = XGetVisualInfo (gdk_x11_display_get_xdisplay (GDK_DISPLAY (self)),
                            VisualScreenMask | VisualIDMask,
                            &template,
                            &nvisuals);
  g_warn_if_fail (nvisuals == 1);
  
  return visinfo;
}

static gboolean
visual_is_rgba (XVisualInfo *visinfo)
{
  return
    visinfo->depth == 32 &&
    visinfo->visual->red_mask   == 0xff0000 &&
    visinfo->visual->green_mask == 0x00ff00 &&
    visinfo->visual->blue_mask  == 0x0000ff;
}

#define MAX_EGL_ATTRS   30

static gboolean
gdk_x11_display_create_egl_config (GdkX11Display  *display,
                                   gboolean        force,
                                   Visual        **out_visual,
                                   int            *out_depth,
                                   GError        **error)
{
  GdkX11Display *self = GDK_X11_DISPLAY (display);
  EGLint attrs[MAX_EGL_ATTRS];
  EGLConfig *configs;
  EGLint count, alloced;
  enum {
    NO_VISUAL_FOUND,
    WITH_MULTISAMPLING,
    WITH_STENCIL_AND_DEPTH_BUFFER,
    NO_ALPHA,
    NO_ALPHA_VISUAL,
    PERFECT
  } best_features;

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

  if (!eglChooseConfig (self->egl_display, attrs, NULL, -1, &alloced) || alloced == 0)
    {
      g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No EGL configuration available"));
      return FALSE;
    }

  configs = g_new (EGLConfig, alloced);
  if (!eglChooseConfig (self->egl_display, attrs, configs, alloced, &count))
    {
      g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Failed to get EGL configurations"));
      return FALSE;
    }
  g_warn_if_fail (alloced == count);

  best_features = NO_VISUAL_FOUND;

  for (i = 0; i < count; i++)
    {
      XVisualInfo *visinfo;
      int tmp, visualid;

      if (!eglGetConfigAttrib (self->egl_display, configs[i], EGL_NATIVE_VISUAL_ID, &visualid))
        continue;

      visinfo = gdk_x11_display_get_visual_info_for_visual (self, visualid);
      if (visinfo == NULL)
        continue;

      if (!eglGetConfigAttrib (self->egl_display, configs[i], EGL_SAMPLE_BUFFERS, &tmp) || tmp != 0)
        {
          if (best_features < WITH_MULTISAMPLING)
            {
              GDK_NOTE (OPENGL, g_message ("Best EGL config is %u for visual 0x%lX with multisampling", i, visinfo->visualid));
              best_features = WITH_MULTISAMPLING;
              *out_visual = visinfo->visual;
              *out_depth = visinfo->depth;
              self->egl_config = configs[i];
            }
          XFree (visinfo);
          continue;
        }

      if (!eglGetConfigAttrib (self->egl_display, configs[i], EGL_DEPTH_SIZE, &tmp) || tmp != 0 ||
          !eglGetConfigAttrib (self->egl_display, configs[i], EGL_STENCIL_SIZE, &tmp) || tmp != 0)
        {
          GDK_NOTE (OPENGL, g_message ("Best EGL config is %u for visual 0x%lX with stencil or depth buffer", i, visinfo->visualid));
          if (best_features < WITH_STENCIL_AND_DEPTH_BUFFER)
            {
              best_features = WITH_STENCIL_AND_DEPTH_BUFFER;
              *out_visual = visinfo->visual;
              *out_depth = visinfo->depth;
              self->egl_config = configs[i];
            }
          XFree (visinfo);
          continue;
        }
    
      if (!visual_is_rgba (visinfo))
        {
          GDK_NOTE (OPENGL, g_message ("Best EGL config is %u for visual 0x%lX without RGBA Visual", i, visinfo->visualid));
          if (best_features < NO_ALPHA_VISUAL)
            {
              best_features = NO_ALPHA_VISUAL;
              *out_visual = visinfo->visual;
              *out_depth = visinfo->depth;
              self->egl_config = configs[i];
            }
          XFree (visinfo);
          continue;
        }

      GDK_NOTE (OPENGL, g_message ("EGL Config %u for visual 0x%lX is the perfect choice", i, visinfo->visualid));
      *out_visual = visinfo->visual;
      *out_depth = visinfo->depth;
      self->egl_config = configs[i];
      XFree (visinfo);
      /* everything is perfect */
      best_features = PERFECT;
      break;
    }

  g_free (configs);

  if (best_features == NO_VISUAL_FOUND)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No EGL configuration with required features found"));
      return FALSE;
    }
  else if (!force && best_features != PERFECT)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No perfect EGL configuration found"));
      return FALSE;
    }

  return TRUE;
}

#undef MAX_EGL_ATTRS

static EGLSurface
gdk_x11_surface_get_egl_surface (GdkSurface *surface)
{
  GdkX11Surface *self = GDK_X11_SURFACE (surface);
  GdkDisplay *display = gdk_surface_get_display (GDK_SURFACE (self));
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  if (self->egl_surface)
    return self->egl_surface;

  self->egl_surface =
    eglCreateWindowSurface (display_x11->egl_display, 
                            display_x11->egl_config,
                            (EGLNativeWindowType) gdk_x11_surface_get_xid (surface),
                            NULL);

  return self->egl_surface;
}

void
gdk_x11_surface_destroy_egl_surface (GdkX11Surface *self)
{
  GdkX11Display *display_x11;

  if (self->egl_surface == NULL)
    return;

  display_x11 = GDK_X11_DISPLAY (gdk_surface_get_display (GDK_SURFACE (self)));

  eglDestroySurface (display_x11->egl_display, self->egl_surface);
  self->egl_surface = NULL;
}

static void
gdk_x11_gl_context_egl_begin_frame (GdkDrawContext *draw_context,
                                    cairo_region_t *region)
{
  GDK_DRAW_CONTEXT_CLASS (gdk_x11_gl_context_egl_parent_class)->begin_frame (draw_context, region);

  glDrawBuffers (1, (GLenum[1]) { GL_BACK });
}

static void
gdk_x11_gl_context_egl_end_frame (GdkDrawContext *draw_context,
                                  cairo_region_t *painted)
{
  GdkGLContext *context = GDK_GL_CONTEXT (draw_context);
  GdkSurface *surface = gdk_gl_context_get_surface (context);
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  EGLSurface egl_surface;

  GDK_DRAW_CONTEXT_CLASS (gdk_x11_gl_context_egl_parent_class)->end_frame (draw_context, painted);

  gdk_gl_context_make_current (context);

  egl_surface = gdk_x11_surface_get_egl_surface (surface);

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

      eglSwapBuffersWithDamageEXT (display_x11->egl_display, egl_surface, rects, n_rects);
      g_free (heap_rects);
    }
  else
    eglSwapBuffers (display_x11->egl_display, egl_surface);
}

static gboolean
gdk_x11_gl_context_egl_clear_current (GdkGLContext *context)
{
  GdkDisplay *display = gdk_gl_context_get_display (context);
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  return eglMakeCurrent (display_x11->egl_display,
                         EGL_NO_SURFACE,
                         EGL_NO_SURFACE,
                         EGL_NO_CONTEXT);
}

static gboolean
gdk_x11_gl_context_egl_make_current (GdkGLContext *context,
                                     gboolean      surfaceless)
{
  GdkX11GLContextEGL *self = GDK_X11_GL_CONTEXT_EGL (context);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  GdkSurface *surface;
  EGLSurface egl_surface;
  gboolean do_frame_sync = FALSE;

  if (surfaceless)
    {
      return eglMakeCurrent (display_x11->egl_display,
                             EGL_NO_SURFACE,
                             EGL_NO_SURFACE,
                             self->egl_context);
    }

  surface = gdk_gl_context_get_surface (context);
  egl_surface = gdk_x11_surface_get_egl_surface (surface);

  GDK_DISPLAY_NOTE (display, OPENGL,
                    g_message ("Making EGL context %p current to surface %p",
                               self->egl_context, egl_surface));

  if (!eglMakeCurrent (display_x11->egl_display,
                       egl_surface,
                       egl_surface,
                       self->egl_context))
    return FALSE;

  /* If the WM is compositing there is no particular need to delay
   * the swap when drawing on the offscreen, rendering to the screen
   * happens later anyway, and its up to the compositor to sync that
   * to the vblank. */
  do_frame_sync = ! gdk_display_is_composited (display);

  if (do_frame_sync != self->do_frame_sync)
    {
      self->do_frame_sync = do_frame_sync;

      if (do_frame_sync)
        eglSwapInterval (display_x11->egl_display, 1);
      else
        eglSwapInterval (display_x11->egl_display, 0);
    }

  return TRUE;
}

static cairo_region_t *
gdk_x11_gl_context_egl_get_damage (GdkGLContext *context)
{
  GdkDisplay *display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (context));
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  if (display_x11->has_egl_buffer_age)
    {
      GdkSurface *surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (context));
      EGLSurface egl_surface;
      int buffer_age = 0;

      egl_surface = gdk_x11_surface_get_egl_surface (surface);
      gdk_gl_context_make_current (context);

      eglQuerySurface (display_x11->egl_display,
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
  GdkGLContext *share;
  gboolean debug_bit, forward_bit, legacy_bit, use_es;
  int major, minor, i = 0;
  EGLint context_attrs[N_EGL_ATTRS];

  display = gdk_gl_context_get_display (context);

  context_egl = GDK_X11_GL_CONTEXT_EGL (context);
  display_x11 = GDK_X11_DISPLAY (display);
  share = gdk_display_get_gl_context (display);

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

  context_egl->egl_context =
    eglCreateContext (display_x11->egl_display,
                      display_x11->egl_config,
                      share != NULL
                        ? GDK_X11_GL_CONTEXT_EGL (share)->egl_context
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
        eglCreateContext (display_x11->egl_display,
                          display_x11->egl_config,
                          share != NULL
                            ? GDK_X11_GL_CONTEXT_EGL (share)->egl_context
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
      GdkX11Display *display_x11 = GDK_X11_DISPLAY (gdk_gl_context_get_display (context));

      /* Unset the current context if we're disposing it */
      if (eglGetCurrentContext () == context_egl->egl_context)
        eglMakeCurrent (display_x11->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

      GDK_NOTE (OPENGL, g_message ("Destroying EGL context"));
      eglDestroyContext (display_x11->egl_display, context_egl->egl_context);
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
  context_class->make_current = gdk_x11_gl_context_egl_make_current;
  context_class->clear_current = gdk_x11_gl_context_egl_clear_current;
  context_class->get_damage = gdk_x11_gl_context_egl_get_damage;

  draw_context_class->begin_frame = gdk_x11_gl_context_egl_begin_frame;
  draw_context_class->end_frame = gdk_x11_gl_context_egl_end_frame;

  gobject_class->dispose = gdk_x11_gl_context_egl_dispose;
}

static void
gdk_x11_gl_context_egl_init (GdkX11GLContextEGL *self)
{
  self->do_frame_sync = TRUE;
}

gboolean
gdk_x11_display_init_egl (GdkX11Display  *self,
                          gboolean        force,
                          Visual        **out_visual,
                          int            *out_depth,
                          GError        **error)
{
  GdkDisplay *display = GDK_DISPLAY (self);
  Display *dpy;
  int major, minor;

  dpy = gdk_x11_display_get_xdisplay (display);

  if (!epoxy_has_egl ())
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("EGL is not supported"));
      return FALSE;
    }

  gdk_x11_display_create_egl_display (self);
  if (self->egl_display == NULL)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Failed to create EGL display"));
      return FALSE;
    }

  if (!eglInitialize (self->egl_display, &major, &minor))
    {
      self->egl_display = NULL;
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Could not initialize EGL display"));
      return FALSE;
    }
  if (major < GDK_EGL_MIN_VERSION_MAJOR ||
      (major == GDK_EGL_MIN_VERSION_MAJOR && minor < GDK_EGL_MIN_VERSION_MINOR))
    {
      eglTerminate (dpy);
      self->egl_display = NULL;
      g_set_error (error, GDK_GL_ERROR,
                   GDK_GL_ERROR_NOT_AVAILABLE,
                   _("EGL version %d.%d is too old. GTK requires %d.%d"),
                   major, minor, GDK_EGL_MIN_VERSION_MAJOR, GDK_EGL_MIN_VERSION_MINOR);
      return FALSE;
    }

  if (!epoxy_has_egl_extension (self->egl_display, "EGL_KHR_surfaceless_context"))
    {
      eglTerminate (dpy);
      self->egl_display = NULL;
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_PROFILE,
                           _("Surfaceless contexts are not supported on this EGL implementation"));
      return FALSE;
    }

  if (!gdk_x11_display_create_egl_config (self, force, out_visual, out_depth, error))
    {
      eglTerminate (self->egl_display);
      self->egl_display = NULL;
      return FALSE;
    }

  self->egl_version = epoxy_egl_version (dpy);

  self->has_egl_khr_create_context =
    epoxy_has_egl_extension (self->egl_display, "EGL_KHR_create_context");
  self->has_egl_buffer_age =
    epoxy_has_egl_extension (self->egl_display, "EGL_EXT_buffer_age");
  self->has_egl_swap_buffers_with_damage =
    epoxy_has_egl_extension (self->egl_display, "EGL_EXT_swap_buffers_with_damage");

  GDK_DISPLAY_NOTE (display, OPENGL,
                    g_message ("EGL found\n"
                               " - Version: %s\n"
                               " - Vendor: %s\n"
                               " - Client API: %s\n"
                               " - Checked extensions:\n"
                               "\t* EGL_KHR_create_context: %s\n"
                               "\t* EGL_EXT_buffer_age: %s\n"
                               "\t* EGL_EXT_swap_buffers_with_damage: %s\n",
                               eglQueryString (self->egl_display, EGL_VERSION),
                               eglQueryString (self->egl_display, EGL_VENDOR),
                               eglQueryString (self->egl_display, EGL_CLIENT_APIS),
                               self->has_egl_khr_create_context ? "yes" : "no",
                               self->has_egl_buffer_age ? "yes" : "no",
                               self->has_egl_swap_buffers_with_damage ? "yes" : "no"));

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

  if (display_x11->egl_display == NULL)
    return FALSE;

  if (major != NULL)
    *major = display_x11->egl_version / 10;
  if (minor != NULL)
    *minor = display_x11->egl_version % 10;

  return TRUE;
}
