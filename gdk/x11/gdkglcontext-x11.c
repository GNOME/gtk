#include "config.h"

#include "gdkglcontext-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkscreen-x11.h"

#include "gdkx11display.h"
#include "gdkx11glcontext.h"
#include "gdkx11screen.h"
#include "gdkx11window.h"
#include "gdkx11visual.h"

#include "gdkintl.h"

#include <GL/glx.h>

G_DEFINE_TYPE (GdkX11GLContext, gdk_x11_gl_context, GDK_TYPE_GL_CONTEXT)

typedef struct {
  GLXDrawable drawable;

  GdkDisplay *display;
  GdkGLContext *context;
  GdkWindow *window;

  guint32 last_frame_counter;
} DrawableInfo;

static void
drawable_info_free (gpointer data_)
{
  DrawableInfo *data = data_;

  glXDestroyWindow (gdk_x11_display_get_xdisplay (data->display), data->drawable);

  g_slice_free (DrawableInfo, data);
}

static DrawableInfo *
get_glx_drawable_info (GdkWindow *window)
{
  return g_object_get_data (G_OBJECT (window), "-gdk-x11-window-glx-info");
}

static void
set_glx_drawable_info (GdkWindow    *window,
                       DrawableInfo *info)
{
  g_object_set_data_full (G_OBJECT (window), "-gdk-x11-window-glx-info",
                          info,
                          drawable_info_free);
}

static void
gdk_x11_gl_context_set_window (GdkGLContext *context,
                               GdkWindow    *window)
{
  GdkX11GLContext *context_x11 = GDK_X11_GL_CONTEXT (context);
  GdkDisplay *display = gdk_gl_context_get_display (context);
  DrawableInfo *info;

  if (window == NULL)
    {
      gdk_x11_display_make_gl_context_current (display, context, NULL);
      return;
    }

  /* we need to make sure that the GdkWindow is backed by
   * an actual native surface
   */
  gdk_window_ensure_native (window);

  /* GLX < 1.3 accepts X11 drawables, so there's no need to
   * go through the creation of a GLX drawable
   */
  if (GDK_X11_DISPLAY (display)->glx_version < 13)
    return;

  info = get_glx_drawable_info (window);
  if (info != NULL)
    return;

  gdk_x11_display_error_trap_push (display);

  info = g_slice_new (DrawableInfo);
  info->window = window;
  info->context = context;
  info->display = display;
  info->drawable = glXCreateWindow (gdk_x11_display_get_xdisplay (display),
                                    context_x11->glx_config,
                                    gdk_x11_window_get_xid (window),
                                    NULL);

  gdk_x11_display_error_trap_pop_ignored (display);

  set_glx_drawable_info (window, info);
}

static void
gdk_x11_gl_context_update (GdkGLContext *context)
{
  GdkWindow *window = gdk_gl_context_get_window (context);
  int x, y, width, height;

  if (window == NULL)
    return;

  if (!gdk_gl_context_make_current (context))
    return;

  gdk_window_get_geometry (window, &x, &y, &width, &height);
  glViewport (0, 0, width, height);
}

static void
maybe_wait_for_vblank (GdkDisplay  *display,
                       GLXDrawable  drawable)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  Display *dpy = gdk_x11_display_get_xdisplay (display);

  if (display_x11->has_glx_sync_control)
    {
      gint64 ust, msc, sbc;

      glXGetSyncValuesOML (dpy, drawable, &ust, &msc, &sbc);
      glXWaitForMscOML (dpy, drawable,
                        0, 2, (msc + 1) % 2,
                        &ust, &msc, &sbc);
    }
  else if (display_x11->has_glx_video_sync)
    {
      guint32 current_count;

      glXGetVideoSyncSGI (&current_count);
      glXWaitVideoSyncSGI (2, (current_count + 1) % 2, &current_count);
    }
}

static void
gdk_x11_gl_context_flush_buffer (GdkGLContext *context)
{
  GdkDisplay *display = gdk_gl_context_get_display (context);
  GdkWindow *window = gdk_gl_context_get_window (context);
  Display *dpy = gdk_x11_display_get_xdisplay (display);
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  DrawableInfo *info;
  GLXDrawable drawable;

  if (window == NULL)
    return;

  info = get_glx_drawable_info (window);
  if (info != NULL && info->drawable != None)
    drawable = info->drawable;
  else
    drawable = gdk_x11_window_get_xid (window);

  GDK_NOTE (OPENGL, g_print ("Flushing GLX buffers for %lu\n", (unsigned long) drawable));

  /* if we are going to wait for the vertical refresh manually
   * we need to flush pending redraws, and we also need to wait
   * for that to finish, otherwise we are going to tear.
   *
   * obviously, this condition should not be hit if we have
   * GLX_SGI_swap_control, and we ask the driver to do the right
   * thing.
   */
  {
    guint32 end_frame_counter = 0;
    gboolean has_counter = display_x11->has_glx_video_sync;
    gboolean can_wait = display_x11->has_glx_video_sync ||
                        display_x11->has_glx_sync_control;

    if (display_x11->has_glx_video_sync)
      glXGetVideoSyncSGI (&end_frame_counter);

    if (!display_x11->has_glx_swap_interval)
      {
        glFinish ();

        if (has_counter && can_wait)
          {
            guint32 last_counter = info != NULL ? info->last_frame_counter : 0;

            if (last_counter == end_frame_counter)
              maybe_wait_for_vblank (display, drawable);
          }
        else if (can_wait)
          maybe_wait_for_vblank (display, drawable);
      }
  }

  glXSwapBuffers (dpy, drawable);

  if (info != NULL && display_x11->has_glx_video_sync)
    glXGetVideoSyncSGI (&info->last_frame_counter);
}

static void
gdk_x11_gl_context_class_init (GdkX11GLContextClass *klass)
{
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);

  context_class->set_window = gdk_x11_gl_context_set_window;
  context_class->update = gdk_x11_gl_context_update;
  context_class->flush_buffer = gdk_x11_gl_context_flush_buffer;
}

static void
gdk_x11_gl_context_init (GdkX11GLContext *self)
{
}

gboolean
gdk_x11_display_init_gl (GdkDisplay *display)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  GdkScreen *screen;
  Display *dpy;
  int error_base, event_base;
  int screen_num;

  if (display_x11->have_glx)
    return TRUE;

  dpy = gdk_x11_display_get_xdisplay (display);

  if (!glXQueryExtension (dpy, &error_base, &event_base))
    return FALSE;

  screen = gdk_display_get_default_screen (display);
  screen_num = GDK_X11_SCREEN (screen)->screen_num;

  display_x11->have_glx = TRUE;

  display_x11->glx_version = epoxy_glx_version (dpy, screen_num);
  display_x11->glx_error_base = error_base;
  display_x11->glx_event_base = event_base;

  display_x11->has_glx_create_context =
    epoxy_has_glx_extension (dpy, screen_num, "GLX_ARB_create_context_profile");
  display_x11->has_glx_swap_interval =
    epoxy_has_glx_extension (dpy, screen_num, "GLX_SGI_swap_control");
  display_x11->has_glx_texture_from_pixmap =
    epoxy_has_glx_extension (dpy, screen_num, "GLX_EXT_texture_from_pixmap");
  display_x11->has_glx_video_sync =
    epoxy_has_glx_extension (dpy, screen_num, "GLX_SGI_video_sync");
  display_x11->has_glx_buffer_age =
    epoxy_has_glx_extension (dpy, screen_num, "GLX_EXT_buffer_age");
  display_x11->has_glx_sync_control =
    epoxy_has_glx_extension (dpy, screen_num, "GLX_OML_sync_control");

  GDK_NOTE (OPENGL,
            g_print ("GLX version %d.%d found\n"
                     " - Vendor: %s\n"
                     " - Checked extensions:\n"
                     "\t* GLX_ARB_create_context_profile: %s\n"
                     "\t* GLX_SGI_swap_control: %s\n"
                     "\t* GLX_EXT_texture_from_pixmap: %s\n"
                     "\t* GLX_SGI_video_sync: %s\n"
                     "\t* GLX_EXT_buffer_age: %s\n"
                     "\t* GLX_OML_sync_control: %s\n",
                     display_x11->glx_version / 10,
                     display_x11->glx_version % 10,
                     glXGetClientString (dpy, GLX_VENDOR),
                     display_x11->has_glx_create_context ? "yes" : "no",
                     display_x11->has_glx_swap_interval ? "yes" : "no",
                     display_x11->has_glx_texture_from_pixmap ? "yes" : "no",
                     display_x11->has_glx_video_sync ? "yes" : "no",
                     display_x11->has_glx_buffer_age ? "yes" : "no",
                     display_x11->has_glx_sync_control ? "yes" : "no"));

  return TRUE;
}

#define MAX_GLX_ATTRS   30

static void
get_glx_attributes_for_pixel_format (GdkDisplay       *display,
                                     GdkGLPixelFormat *format,
                                     int              *attrs)
{
  GdkX11Display *display_x11;
  int i = 0;

  attrs[i++] = GLX_DRAWABLE_TYPE;
  attrs[i++] = GLX_WINDOW_BIT;

  attrs[i++] = GLX_RENDER_TYPE;
  attrs[i++] = GLX_RGBA_BIT;

  if (format->double_buffer)
    {
      attrs[i++] = GLX_DOUBLEBUFFER;
      attrs[i++] = GL_TRUE;
    }

  if (format->color_size < 0)
    {
      attrs[i++] = GLX_RED_SIZE;
      attrs[i++] = 1;
      attrs[i++] = GLX_GREEN_SIZE;
      attrs[i++] = 1;
      attrs[i++] = GLX_BLUE_SIZE;
      attrs[i++] = 1;
    }
  else
    {
      attrs[i++] = GLX_RED_SIZE;
      attrs[i++] = format->color_size;
      attrs[i++] = GLX_GREEN_SIZE;
      attrs[i++] = format->color_size;
      attrs[i++] = GLX_BLUE_SIZE;
      attrs[i++] = format->color_size;
    }

  if (format->alpha_size < 0)
    {
      attrs[i++] = GLX_ALPHA_SIZE;
      attrs[i++] = 1;
    }
  else if (format->alpha_size == 0)
    {
      attrs[i++] = GLX_ALPHA_SIZE;
      attrs[i++] = GLX_DONT_CARE;
    }
  else
    {
      attrs[i++] = GLX_ALPHA_SIZE;
      attrs[i++] = format->alpha_size;
    }

  if (format->depth_size < 0)
    {
      attrs[i++] = GLX_DEPTH_SIZE;
      attrs[i++] = 1;
    }
  else
    {
      attrs[i++] = GLX_DEPTH_SIZE;
      attrs[i++] = format->depth_size;
    }

  if (format->stencil_size < 0)
    {
      attrs[i++] = GLX_STENCIL_SIZE;
      attrs[i++] = GLX_DONT_CARE;
    }
  else
    {
      attrs[i++] = GLX_STENCIL_SIZE;
      attrs[i++] = format->stencil_size;
    }

  display_x11 = GDK_X11_DISPLAY (display);
  if (display_x11->glx_version >= 14 && format->multi_sample)
    {
      attrs[i++] = GLX_SAMPLE_BUFFERS;
      attrs[i++] = format->sample_buffers > 0 ? format->sample_buffers : 1;

      attrs[i++] = GLX_SAMPLES;
      attrs[i++] = format->samples > 0 ? format->samples : 1;
    }

  attrs[i++] = None;

  g_assert (i < MAX_GLX_ATTRS);
}

static gboolean
find_fbconfig_for_pixel_format (GdkDisplay        *display,
                                GdkGLPixelFormat  *format,
                                GLXFBConfig       *fb_config_out,
                                XVisualInfo      **visinfo_out,
                                GError           **error)
{
  static int attrs[MAX_GLX_ATTRS];

  Display *dpy = gdk_x11_display_get_xdisplay (display);
  GLXFBConfig *configs;
  int n_configs, i;
  gboolean use_rgba;
  gboolean retval = FALSE;

  get_glx_attributes_for_pixel_format (display, format, attrs);

  use_rgba = format->alpha_size != 0;

  configs = glXChooseFBConfig (dpy, DefaultScreen (dpy), attrs, &n_configs);
  if (configs == NULL || n_configs == 0)
    {
      g_set_error_literal (error, GDK_GL_PIXEL_FORMAT_ERROR,
                           GDK_GL_PIXEL_FORMAT_ERROR_NOT_AVAILABLE,
                           _("No available configurations for the given pixel format"));
      return FALSE;
    }

  /* if we don't care about an alpha channel, then the first
   * valid configuration is the one we give back
   */
  if (!use_rgba)
    {
      if (fb_config_out != NULL)
        *fb_config_out = configs[0];

      if (visinfo_out != NULL)
        *visinfo_out = glXGetVisualFromFBConfig (dpy, configs[0]);

      retval = TRUE;
      goto out;
    }

  for (i = 0; i < n_configs; i++)
    {
      XVisualInfo *visinfo;
      unsigned long mask;

      visinfo = glXGetVisualFromFBConfig (dpy, configs[i]);
      if (visinfo == NULL)
        continue;

      mask = visinfo->red_mask | visinfo->green_mask | visinfo->blue_mask;
      if (visinfo->depth == 32 && mask != 0xffffffff)
        {
          if (fb_config_out != NULL)
            *fb_config_out = configs[i];

          if (visinfo_out != NULL)
            *visinfo_out = visinfo;

          retval = TRUE;
          goto out;
        }
    }
    
  g_set_error (error, GDK_GL_PIXEL_FORMAT_ERROR,
               GDK_GL_PIXEL_FORMAT_ERROR_NOT_AVAILABLE,
               _("No available configurations for the given RGBA pixel format"));

out:
  XFree (configs);

  return retval;
}

static GLXContext
create_gl3_context (GdkDisplay   *display,
                    GLXFBConfig   config,
                    GdkGLContext *share)
{
  static const int attrib_list[] = {
    GLX_CONTEXT_PROFILE_MASK_ARB,  GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
    GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
    GLX_CONTEXT_MAJOR_VERSION_ARB, 2,
    None,
  };

  GdkX11GLContext *context_x11 = NULL;

  if (share != NULL)
    context_x11 = GDK_X11_GL_CONTEXT (share);

  return glXCreateContextAttribsARB (gdk_x11_display_get_xdisplay (display),
                                     config,
                                     context_x11 != NULL ? context_x11->glx_context : NULL,
                                     True,
                                     attrib_list);
}

static GLXContext
create_gl_context (GdkDisplay   *display,
                   GLXFBConfig   config,
                   GdkGLContext *share)
{
  GdkX11GLContext *context_x11 = NULL;

  if (share != NULL)
    context_x11 = GDK_X11_GL_CONTEXT (share);

  return glXCreateNewContext (gdk_x11_display_get_xdisplay (display),
                              config,
                              GLX_RGBA_TYPE,
                              context_x11 != NULL ? context_x11->glx_context : NULL,
                              True);
}

GdkGLContext *
gdk_x11_display_create_gl_context (GdkDisplay        *display,
                                   GdkGLPixelFormat  *format,
                                   GdkGLContext      *share,
                                   GError           **error)
{
  GdkX11GLContext *context;
  GdkVisual *gdk_visual;
  GLXFBConfig config;
  GLXContext glx_context;
  Window dummy_xwin;
  GLXWindow dummy_glx;
  GLXWindow dummy_drawable;
  gboolean is_direct;
  XVisualInfo *xvisinfo;
  XSetWindowAttributes attrs;
  unsigned long mask;
  Display *dpy;

  if (!gdk_x11_display_validate_gl_pixel_format (display, format, error))
    return NULL;

  /* if validation succeeded, then we don't need to check for the result
   * here
   */
  find_fbconfig_for_pixel_format (display, format, &config, &xvisinfo, NULL);

  dpy = gdk_x11_display_get_xdisplay (display);

  /* we check for the GLX_ARB_create_context_profile extension
   * while validating the PixelFormat.
   */
  if (format->profile == GDK_GL_PIXEL_FORMAT_PROFILE_3_2_CORE)
    glx_context = create_gl3_context (display, config, share);
  else
    {
      /* GDK_GL_PIXEL_FORMAT_PROFILE_DEFAULT is currently
       * equivalent to the LEGACY profile
       */
      glx_context = create_gl_context (display, config, share);
    }

  if (glx_context == NULL)
    {
      g_set_error_literal (error, GDK_GL_PIXEL_FORMAT_ERROR,
                           GDK_GL_PIXEL_FORMAT_ERROR_NOT_AVAILABLE,
                           _("Unable to create a GL context"));
      return NULL;
    }

  is_direct = glXIsDirect (dpy, glx_context);

  gdk_x11_display_error_trap_push (display);

  /* create a dummy window; this is needed because GLX does not allow
   * us to query the context until it's bound to a drawable; we simply
   * create a small OR window, put it off screen, and never map it. in
   * order to keep the GL machinery in a sane state, we always make
   * the dummy window the current drawable if the user unsets the
   * GdkWindow bound to the GdkGLContext.
   */
  attrs.override_redirect = True;
  attrs.colormap = XCreateColormap (dpy, DefaultRootWindow (dpy), xvisinfo->visual, AllocNone);
  attrs.border_pixel = 0;
  mask = CWOverrideRedirect | CWColormap | CWBorderPixel;

  dummy_xwin = XCreateWindow (dpy, DefaultRootWindow (dpy),
                              -100, -100, 1, 1,
                              0,
                              xvisinfo->depth,
                              CopyFromParent,
                              xvisinfo->visual,
                              mask,
                              &attrs);

  /* GLX API introduced in 1.3 expects GLX drawables */
  if (GDK_X11_DISPLAY (display)->glx_version >= 13)
    dummy_glx = glXCreateWindow (dpy, config, dummy_xwin, NULL);
  else
    dummy_glx = None;

  dummy_drawable = dummy_glx != None
                 ? dummy_glx
                 : dummy_xwin;

  glXMakeContextCurrent (dpy, dummy_drawable, dummy_drawable, glx_context);

  gdk_visual = gdk_x11_screen_lookup_visual (gdk_display_get_default_screen (display),
                                             xvisinfo->visualid);

  XFree (xvisinfo);

  if (gdk_x11_display_error_trap_pop (display))
    {
      g_set_error_literal (error, GDK_GL_PIXEL_FORMAT_ERROR,
                           GDK_GL_PIXEL_FORMAT_ERROR_NOT_AVAILABLE,
                           _("Unable to create a GL context"));

      glXDestroyContext (dpy, glx_context);

      if (dummy_xwin)
        XDestroyWindow (dpy, dummy_xwin);
      if (dummy_glx)
        glXDestroyWindow (dpy, dummy_glx);

      return NULL;
    }

  GDK_NOTE (OPENGL,
            g_print ("Created GLX context[%p], %s, dummy drawable: %lu\n",
                     glx_context,
                     is_direct ? "direct" : "indirect",
                     (unsigned long) dummy_xwin));

  context = g_object_new (GDK_X11_TYPE_GL_CONTEXT,
                          "display", display,
                          "pixel-format", format,
                          "visual", gdk_visual,
                          NULL);

  context->glx_config = config;
  context->glx_context = glx_context;
  context->dummy_drawable = dummy_xwin;
  context->dummy_glx_drawable = dummy_glx;
  context->current_drawable = dummy_drawable;
  context->is_direct = is_direct;

  return GDK_GL_CONTEXT (context);
}

void
gdk_x11_display_destroy_gl_context (GdkDisplay   *display,
                                    GdkGLContext *context)
{
  GdkX11GLContext *context_x11 = GDK_X11_GL_CONTEXT (context);
  Display *dpy = gdk_x11_display_get_xdisplay (display);

  if (context_x11->glx_context != NULL)
    {
      if (glXGetCurrentContext () == context_x11->glx_context)
        glXMakeContextCurrent (dpy, None, None, NULL);

      GDK_NOTE (OPENGL, g_print ("Destroying GLX context\n"));
      glXDestroyContext (dpy, context_x11->glx_context);
      context_x11->glx_context = NULL;
    }

  if (context_x11->dummy_glx_drawable)
    {
      GDK_NOTE (OPENGL, g_print ("Destroying dummy GLX drawable\n"));
      glXDestroyWindow (dpy, context_x11->dummy_glx_drawable);
      context_x11->dummy_glx_drawable = None;
    }

  if (context_x11->dummy_drawable)
    {
      GDK_NOTE (OPENGL, g_print ("Destroying dummy drawable\n"));
      XDestroyWindow (dpy, context_x11->dummy_drawable);
      context_x11->dummy_drawable = None;
    }
}

gboolean
gdk_x11_display_make_gl_context_current (GdkDisplay   *display,
                                         GdkGLContext *context,
                                         GdkWindow    *window)
{
  GdkX11GLContext *context_x11 = GDK_X11_GL_CONTEXT (context);
  GLXDrawable drawable = None;

  if (context_x11->glx_context == NULL)
    return FALSE;

  if (window == NULL)
    {
      /* we re-bind our dummy drawable, so that the context
       * can still be used for queries
       */
      drawable = context_x11->dummy_glx_drawable != None
               ? context_x11->dummy_glx_drawable
               : context_x11->dummy_drawable;
    }
  else
    {
      DrawableInfo *info = get_glx_drawable_info (window);

      if (info != NULL && info->drawable != None)
        drawable = info->drawable;
      else
        drawable = gdk_x11_window_get_xid (window);
    }

  if (G_UNLIKELY (drawable == None))
    return FALSE;

  GDK_NOTE (OPENGL,
            g_print ("Making GLX context current to drawable %lu (dummy: %s)\n",
                     (unsigned long) drawable,
                     drawable == context_x11->dummy_drawable ? "yes" : "no"));

  if (drawable == context_x11->current_drawable)
    return TRUE;

  gdk_x11_display_error_trap_push (display);

  glXMakeContextCurrent (gdk_x11_display_get_xdisplay (display),
                         drawable, drawable,
                         context_x11->glx_context);

  if (GDK_X11_DISPLAY (display)->has_glx_swap_interval)
    {
      if (gdk_gl_context_get_swap_interval (context))
        glXSwapIntervalSGI (1);
      else
        glXSwapIntervalSGI (0);
    }

  XSync (gdk_x11_display_get_xdisplay (display), False);

  if (gdk_x11_display_error_trap_pop (display))
    {
      g_critical ("X Error received while calling glXMakeContextCurrent()");
      return FALSE;
    }

  context_x11->current_drawable = drawable;

  return TRUE;
}

gboolean
gdk_x11_display_validate_gl_pixel_format (GdkDisplay        *display,
                                          GdkGLPixelFormat  *format,
                                          GError           **error)
{
  if (!gdk_x11_display_init_gl (display))
    {
      g_set_error_literal (error, GDK_GL_PIXEL_FORMAT_ERROR,
                           GDK_GL_PIXEL_FORMAT_ERROR_NOT_AVAILABLE,
                           _("No GL implementation is available"));
      return FALSE;
    }

  if (format->profile == GDK_GL_PIXEL_FORMAT_PROFILE_3_2_CORE)
    {
      if (!GDK_X11_DISPLAY (display)->has_glx_create_context)
        {
          g_set_error_literal (error, GDK_GL_PIXEL_FORMAT_ERROR,
                               GDK_GL_PIXEL_FORMAT_ERROR_NOT_AVAILABLE,
                               _("The GLX_ARB_create_context_profile extension "
                                 "needed to create 3.2 core profiles is not "
                                 "available"));
          return FALSE;
        }
    }

  if (!find_fbconfig_for_pixel_format (display, format, NULL, NULL, error))
    return FALSE;

  return TRUE;
}
