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

#include "gdkinternals.h"

#include "gdkintl.h"

#include <cairo/cairo-xlib.h>

#include <epoxy/glx.h>

G_DEFINE_TYPE (GdkX11GLContext, gdk_x11_gl_context, GDK_TYPE_GL_CONTEXT)

typedef struct {
  GdkDisplay *display;

  GLXDrawable glx_drawable;

  Window dummy_xwin;
  GLXWindow dummy_glx;

  guint32 last_frame_counter;
} DrawableInfo;

static void
drawable_info_free (gpointer data_)
{
  DrawableInfo *data = data_;
  Display *dpy;

  gdk_x11_display_error_trap_push (data->display);

  dpy = gdk_x11_display_get_xdisplay (data->display);

  if (data->glx_drawable)
    glXDestroyWindow (dpy, data->glx_drawable);

  if (data->dummy_glx)
    glXDestroyWindow (dpy, data->dummy_glx);

  if (data->dummy_xwin)
    XDestroyWindow (dpy, data->dummy_xwin);

  gdk_x11_display_error_trap_pop_ignored (data->display);

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
gdk_x11_gl_context_update (GdkGLContext *context)
{
  GdkWindow *window = gdk_gl_context_get_window (context);
  int width, height;

  gdk_gl_context_make_current (context);

  width = gdk_window_get_width (window);
  height = gdk_window_get_height (window);

  GDK_NOTE (OPENGL, g_print ("Updating GL viewport size to { %d, %d } for window %lu (context: %p)\n",
                             width, height,
                             (unsigned long) gdk_x11_window_get_xid (window),
                             context));

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

void
gdk_x11_window_invalidate_for_new_frame (GdkWindow      *window,
                                         cairo_region_t *update_area)
{
  cairo_rectangle_int_t window_rect;
  GdkDisplay *display = gdk_window_get_display (window);
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  Display *dpy = gdk_x11_display_get_xdisplay (display);
  GdkX11GLContext *context_x11;
  unsigned int buffer_age;
  gboolean invalidate_all;

  /* Minimal update is ok if we're not drawing with gl */
  if (window->gl_paint_context == NULL)
    return;

  context_x11 = GDK_X11_GL_CONTEXT (window->gl_paint_context);

  buffer_age = 0;

  if (display_x11->has_glx_buffer_age)
    {
      gdk_gl_context_make_current (window->gl_paint_context);
      glXQueryDrawable(dpy, context_x11->drawable,
		       GLX_BACK_BUFFER_AGE_EXT, &buffer_age);
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
gdk_x11_gl_context_end_frame (GdkGLContext *context,
                              cairo_region_t *painted,
                              cairo_region_t *damage)
{
  GdkX11GLContext *context_x11 = GDK_X11_GL_CONTEXT (context);
  GdkWindow *window = gdk_gl_context_get_window (context);
  GdkDisplay *display = gdk_window_get_display (window);
  Display *dpy = gdk_x11_display_get_xdisplay (display);
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);
  DrawableInfo *info;
  GLXDrawable drawable;

  gdk_gl_context_make_current (context);

  info = get_glx_drawable_info (window);

  drawable = context_x11->drawable;

  GDK_NOTE (OPENGL,
            g_print ("Flushing GLX buffers for drawable %lu (window: %lu), frame sync: %s\n",
                     (unsigned long) drawable,
                     (unsigned long) gdk_x11_window_get_xid (window),
                     context_x11->do_frame_sync ? "yes" : "no"));

  /* if we are going to wait for the vertical refresh manually
   * we need to flush pending redraws, and we also need to wait
   * for that to finish, otherwise we are going to tear.
   *
   * obviously, this condition should not be hit if we have
   * GLX_SGI_swap_control, and we ask the driver to do the right
   * thing.
   */
  if (context_x11->do_frame_sync)
    {
      guint32 end_frame_counter = 0;
      gboolean has_counter = display_x11->has_glx_video_sync;
      gboolean can_wait = display_x11->has_glx_video_sync || display_x11->has_glx_sync_control;

      if (display_x11->has_glx_video_sync)
        glXGetVideoSyncSGI (&end_frame_counter);

      if (context_x11->do_frame_sync && !display_x11->has_glx_swap_interval)
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

  if (context_x11->do_frame_sync && info != NULL && display_x11->has_glx_video_sync)
    glXGetVideoSyncSGI (&info->last_frame_counter);
}

typedef struct {
  Display *display;
  GLXDrawable drawable;
  gboolean y_inverted;
} GdkGLXPixmap;

static void
glx_pixmap_destroy (void *data)
{
  GdkGLXPixmap *glx_pixmap = data;

  glXDestroyPixmap (glx_pixmap->display, glx_pixmap->drawable);

  g_slice_free (GdkGLXPixmap, glx_pixmap);
}

static GdkGLXPixmap *
glx_pixmap_get (cairo_surface_t *surface)
{
  Display *display = cairo_xlib_surface_get_display (surface);
  Screen *screen = cairo_xlib_surface_get_screen (surface);
  Visual *visual = cairo_xlib_surface_get_visual (surface);;
  GdkGLXPixmap *glx_pixmap;
  GLXFBConfig *fbconfigs;
  int nfbconfigs;
  XVisualInfo *visinfo;
  int i, value;
  gboolean y_inverted;
  gboolean with_alpha;
  guint target = 0;
  guint format = 0;
  int pixmap_attributes[] = {
    GLX_TEXTURE_TARGET_EXT, 0,
    GLX_TEXTURE_FORMAT_EXT, 0,
    None
  };

  with_alpha = cairo_surface_get_content (surface) == CAIRO_CONTENT_COLOR_ALPHA;

  y_inverted = FALSE;
  fbconfigs = glXGetFBConfigs (display, XScreenNumberOfScreen (screen), &nfbconfigs);
  for (i = 0; i < nfbconfigs; i++)
    {
      visinfo = glXGetVisualFromFBConfig (display, fbconfigs[i]);
      if (!visinfo || visinfo->visualid != XVisualIDFromVisual (visual))
        continue;

      glXGetFBConfigAttrib (display, fbconfigs[i], GLX_DRAWABLE_TYPE, &value);
      if (!(value & GLX_PIXMAP_BIT))
        continue;

      glXGetFBConfigAttrib (display, fbconfigs[i],
                            GLX_BIND_TO_TEXTURE_TARGETS_EXT,
                            &value);
      if ((value & (GLX_TEXTURE_RECTANGLE_BIT_EXT | GLX_TEXTURE_2D_BIT_EXT)) == 0)
        continue;
      if ((value & GLX_TEXTURE_2D_BIT_EXT))
        target = GLX_TEXTURE_2D_EXT;
      else
        target = GLX_TEXTURE_RECTANGLE_EXT;

      if (!with_alpha)
        {
          glXGetFBConfigAttrib (display, fbconfigs[i],
                                GLX_BIND_TO_TEXTURE_RGB_EXT,
                                &value);
          if (!value)
            continue;

          format = GLX_TEXTURE_FORMAT_RGB_EXT;
        }
      else
        {
          glXGetFBConfigAttrib (display, fbconfigs[i],
                                GLX_BIND_TO_TEXTURE_RGBA_EXT,
                                &value);
          if (!value)
            continue;
          format = GLX_TEXTURE_FORMAT_RGBA_EXT;
        }

      glXGetFBConfigAttrib (display, fbconfigs[i],
                            GLX_Y_INVERTED_EXT,
                            &value);
      if (value == TRUE)
        y_inverted = TRUE;

      break;
    }

  if (i == nfbconfigs)
    return NULL;

  pixmap_attributes[1] = target;
  pixmap_attributes[3] = format;

  glx_pixmap = g_slice_new0 (GdkGLXPixmap);
  glx_pixmap->y_inverted = y_inverted;
  glx_pixmap->display = display;
  glx_pixmap->drawable = glXCreatePixmap (display, fbconfigs[i],
					  cairo_xlib_surface_get_drawable (surface),
					  pixmap_attributes);

  return glx_pixmap;
}

static gboolean
gdk_x11_gl_context_texture_from_surface (GdkGLContext *context,
					 cairo_surface_t *surface,
					 cairo_region_t *region)
{
  GdkGLContext *current;
  GdkGLXPixmap *glx_pixmap;
  double device_x_offset, device_y_offset;
  cairo_rectangle_int_t rect;
  int n_rects, i;
  GdkWindow *window;
  int window_height;
  int window_scale;
  unsigned int texture_id;
  gboolean use_texture_rectangle;
  guint target;
  double sx, sy;
  float uscale, vscale;

  if (cairo_surface_get_type (surface) != CAIRO_SURFACE_TYPE_XLIB)
    return FALSE;

  glx_pixmap = glx_pixmap_get (surface);
  if (glx_pixmap == NULL)
    return FALSE;

  current = gdk_gl_context_get_current ();

  use_texture_rectangle = gdk_gl_context_use_texture_rectangle (current);

  window = gdk_gl_context_get_window (current)->impl_window;
  window_scale = gdk_window_get_scale_factor (window);
  window_height = gdk_window_get_height (window);

  sx = sy = 1;
#ifdef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
  cairo_surface_get_device_scale (window->current_paint.surface, &sx, &sy);
#endif

  cairo_surface_get_device_offset (surface,
				   &device_x_offset, &device_y_offset);

  /* Ensure all the X stuff are synced before we read it back via texture-from-pixmap */
  glXWaitX();

  if (use_texture_rectangle)
    target = GL_TEXTURE_RECTANGLE_ARB;
  else
    target = GL_TEXTURE_2D;

  glGenTextures (1, &texture_id);
  glBindTexture (target, texture_id);
  glEnable (target);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glXBindTexImageEXT (glx_pixmap->display, glx_pixmap->drawable,
		      GLX_FRONT_LEFT_EXT, NULL);

  n_rects = cairo_region_num_rectangles (region);
  for (i = 0; i < n_rects; i++)
    {
      int src_x, src_y, src_height, src_width;

      cairo_region_get_rectangle (region, i, &rect);

      glScissor (rect.x * window_scale, (window_height - rect.y - rect.height) * window_scale,
		 rect.width * window_scale, rect.height * window_scale);

      src_x = rect.x * sx + device_x_offset;
      src_y = rect.y * sy + device_y_offset;
      src_width = rect.width * sx;
      src_height = rect.height * sy;

#define FLIP_Y(_y) (window_height - (_y))

      if (use_texture_rectangle)
        {
          uscale = 1.0;
          vscale = 1.0;
        }
      else
        {
          uscale = 1.0 / cairo_xlib_surface_get_width (surface);
          vscale = 1.0 / cairo_xlib_surface_get_height (surface);
        }

      glBegin (GL_QUADS);
      glTexCoord2f (uscale * src_x, vscale * (src_y + src_height));
      glVertex2f (rect.x * window_scale, FLIP_Y(rect.y + rect.height) * window_scale);

      glTexCoord2f (uscale * (src_x + src_width), vscale * (src_y + src_height));
      glVertex2f ((rect.x + rect.width) * window_scale, FLIP_Y(rect.y + rect.height) * window_scale);

      glTexCoord2f (uscale * (src_x + src_width), vscale * src_y);
      glVertex2f ((rect.x + rect.width) * window_scale, FLIP_Y(rect.y) * window_scale);

      glTexCoord2f (uscale * src_x, vscale * src_y);
      glVertex2f (rect.x * window_scale, FLIP_Y(rect.y) * window_scale);
      glEnd();
    }

  glXReleaseTexImageEXT (glx_pixmap->display, glx_pixmap->drawable,
  			 GLX_FRONT_LEFT_EXT);

  glDisable (target);
  glDeleteTextures (1, &texture_id);

  glx_pixmap_destroy(glx_pixmap);

  return TRUE;
}

static void
gdk_x11_gl_context_class_init (GdkX11GLContextClass *klass)
{
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);

  context_class->update = gdk_x11_gl_context_update;
  context_class->end_frame = gdk_x11_gl_context_end_frame;
  context_class->texture_from_surface = gdk_x11_gl_context_texture_from_surface;
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

static gboolean
find_fbconfig_for_visual (GdkDisplay        *display,
			  GdkVisual         *visual,
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

  i = 0;
  attrs[i++] = GLX_DRAWABLE_TYPE;
  attrs[i++] = GLX_WINDOW_BIT;

  attrs[i++] = GLX_RENDER_TYPE;
  attrs[i++] = GLX_RGBA_BIT;

  attrs[i++] = GLX_DOUBLEBUFFER;
  attrs[i++] = GL_TRUE;

  attrs[i++] = GLX_RED_SIZE;
  attrs[i++] = gdk_visual_get_bits_per_rgb (visual);
  attrs[i++] = GLX_GREEN_SIZE;
  attrs[i++] = gdk_visual_get_bits_per_rgb (visual);;
  attrs[i++] = GLX_BLUE_SIZE;
  attrs[i++] = gdk_visual_get_bits_per_rgb (visual);;

  use_rgba = (visual == gdk_screen_get_rgba_visual (gdk_display_get_default_screen (display)));

  if (use_rgba)
    {
      attrs[i++] = GLX_ALPHA_SIZE;
      attrs[i++] = 1;
    }
  else
    {
      attrs[i++] = GLX_ALPHA_SIZE;
      attrs[i++] = GLX_DONT_CARE;
    }

  attrs[i++] = None;

  g_assert (i < MAX_GLX_ATTRS);

  configs = glXChooseFBConfig (dpy, DefaultScreen (dpy), attrs, &n_configs);
  if (configs == NULL || n_configs == 0)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_FORMAT,
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

      XFree (visinfo);
    }

  g_set_error (error, GDK_GL_ERROR,
               GDK_GL_ERROR_UNSUPPORTED_FORMAT,
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
gdk_x11_window_create_gl_context (GdkWindow    *window,
				  gboolean      attached,
				  GdkGLProfile  profile,
				  GdkGLContext *share,
				  GError      **error)
{
  GdkDisplay *display;
  GdkX11GLContext *context;
  GdkVisual *visual;
  GdkVisual *gdk_visual;
  GLXFBConfig config;
  GLXContext glx_context;
  GLXWindow drawable;
  gboolean is_direct;
  XVisualInfo *xvisinfo;
  Display *dpy;
  DrawableInfo *info;

  display = gdk_window_get_display (window);

  if (!gdk_x11_display_init_gl (display))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL implementation is available"));
      return NULL;
    }

  if (profile == GDK_GL_PROFILE_3_2_CORE &&
      !GDK_X11_DISPLAY (display)->has_glx_create_context)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_PROFILE,
                           _("The GLX_ARB_create_context_profile extension "
                             "needed to create 3.2 core profiles is not "
                             "available"));
      return NULL;
    }

  visual = gdk_window_get_visual (window);

  if (!find_fbconfig_for_visual (display, visual, &config, &xvisinfo, error))
    return NULL;

  dpy = gdk_x11_display_get_xdisplay (display);

  /* we check for the GLX_ARB_create_context_profile extension
   * while validating the PixelFormat.
   */
  if (profile == GDK_GL_PROFILE_3_2_CORE)
    glx_context = create_gl3_context (display, config, share);
  else
    {
      /* GDK_GL_PROFILE_DEFAULT is currently
       * equivalent to the LEGACY profile
       */
      glx_context = create_gl_context (display, config, share);
    }

  if (glx_context == NULL)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("Unable to create a GL context"));
      return NULL;
    }

  is_direct = glXIsDirect (dpy, glx_context);

  info = get_glx_drawable_info (window->impl_window);
  if (info == NULL)
    {
      XSetWindowAttributes attrs;
      unsigned long mask;

      gdk_x11_display_error_trap_push (display);

      info = g_slice_new0 (DrawableInfo);
      info->display = display;
      info->last_frame_counter = 0;

      attrs.override_redirect = True;
      attrs.colormap = XCreateColormap (dpy, DefaultRootWindow (dpy), xvisinfo->visual, AllocNone);
      attrs.border_pixel = 0;
      mask = CWOverrideRedirect | CWColormap | CWBorderPixel;
      info->dummy_xwin = XCreateWindow (dpy, DefaultRootWindow (dpy),
					-100, -100, 1, 1,
					0,
					xvisinfo->depth,
					CopyFromParent,
					xvisinfo->visual,
					mask,
					&attrs);
      XMapWindow(dpy, info->dummy_xwin);

      if (GDK_X11_DISPLAY (display)->glx_version >= 13)
	{
	  info->glx_drawable = glXCreateWindow (dpy, config,
						gdk_x11_window_get_xid (window->impl_window),
						NULL);
	  info->dummy_glx = glXCreateWindow (dpy, config, info->dummy_xwin, NULL);
	}

      if (gdk_x11_display_error_trap_pop (display))
	{
	  g_set_error_literal (error, GDK_GL_ERROR,
			       GDK_GL_ERROR_NOT_AVAILABLE,
			       _("Unable to create a GL context"));

	  drawable_info_free (info);
	  glXDestroyContext (dpy, glx_context);

	  return NULL;
	}

      set_glx_drawable_info (window->impl_window, info);
    }

  gdk_visual = gdk_x11_screen_lookup_visual (gdk_display_get_default_screen (display),
                                             xvisinfo->visualid);

  XFree (xvisinfo);

  if (attached)
    drawable = info->glx_drawable ? info->glx_drawable : gdk_x11_window_get_xid (window->impl_window);
  else
    drawable = info->dummy_glx ? info->dummy_glx : info->dummy_xwin;

  GDK_NOTE (OPENGL,
            g_print ("Created GLX context[%p], %s\n",
                     glx_context,
                     is_direct ? "direct" : "indirect"));

  context = g_object_new (GDK_TYPE_X11_GL_CONTEXT,
                          "window", window,
                          "visual", gdk_visual,
                          NULL);

  context->profile = profile;
  context->glx_config = config;
  context->glx_context = glx_context;
  context->drawable = drawable;
  context->is_attached = attached;
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
}

void
gdk_x11_display_make_gl_context_current (GdkDisplay   *display,
                                         GdkGLContext *context)
{
  GdkX11GLContext *context_x11;
  Display *dpy = gdk_x11_display_get_xdisplay (display);
  GdkWindow *window;
  GdkScreen *screen;
  gboolean do_frame_sync = FALSE;

  if (context == NULL)
    {
      glXMakeContextCurrent (dpy, None, None, NULL);
      return;
    }

  context_x11 = GDK_X11_GL_CONTEXT (context);

  window = gdk_gl_context_get_window (context);

  // If the WM is compositing there is no particular need to delay
  // the swap when drawing on the offscreen, rendering to the screen
  // happens later anyway, and its up to the compositor to sync that
  // to the vblank.
  screen = gdk_window_get_screen (window);
  do_frame_sync = ! gdk_screen_is_composited (screen);

  context_x11->do_frame_sync = do_frame_sync;

  GDK_NOTE (OPENGL,
            g_print ("Making GLX context current to drawable %lu\n",
                     (unsigned long) context_x11->drawable));

  glXMakeContextCurrent (dpy, context_x11->drawable, context_x11->drawable,
                         context_x11->glx_context);

  if (context_x11->is_attached && GDK_X11_DISPLAY (display)->has_glx_swap_interval)
    {
      if (context_x11->do_frame_sync)
        glXSwapIntervalSGI (1);
      else
        glXSwapIntervalSGI (0);
    }
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
