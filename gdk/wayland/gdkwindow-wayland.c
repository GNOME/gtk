/*
 * Copyright © 2010 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#include <netinet/in.h>
#include <unistd.h>

#include "gdk.h"
#include "gdkwayland.h"

#include "gdkwindow.h"
#include "gdkwindowimpl.h"
#include "gdkdisplay-wayland.h"
#include "gdkprivate-wayland.h"
#include "gdkinternals.h"
#include "gdkdeviceprivate.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <wayland-egl.h>

#define WINDOW_IS_TOPLEVEL_OR_FOREIGN(window) \
  (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&   \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_OFFSCREEN)

#define WINDOW_IS_TOPLEVEL(window)		     \
  (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&   \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN && \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_OFFSCREEN)

/* Return whether time1 is considered later than time2 as far as xserver
 * time is concerned.  Accounts for wraparound.
 */
#define XSERVER_TIME_IS_LATER(time1, time2)                        \
  ( (( time1 > time2 ) && ( time1 - time2 < ((guint32)-1)/2 )) ||  \
    (( time1 < time2 ) && ( time2 - time1 > ((guint32)-1)/2 ))     \
  )

typedef struct _GdkWaylandWindow GdkWaylandWindow;
typedef struct _GdkWaylandWindowClass GdkWaylandWindowClass;

struct _GdkWaylandWindow {
  GdkWindow parent;
};

struct _GdkWaylandWindowClass {
  GdkWindowClass parent_class;
};

G_DEFINE_TYPE (GdkWaylandWindow, _gdk_wayland_window, GDK_TYPE_WINDOW)

static void
_gdk_wayland_window_class_init (GdkWaylandWindowClass *wayland_window_class)
{
}

static void
_gdk_wayland_window_init (GdkWaylandWindow *wayland_window)
{
}

#define GDK_TYPE_WINDOW_IMPL_WAYLAND              (_gdk_window_impl_wayland_get_type ())
#define GDK_WINDOW_IMPL_WAYLAND(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WINDOW_IMPL_WAYLAND, GdkWindowImplWayland))
#define GDK_WINDOW_IMPL_WAYLAND_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WINDOW_IMPL_WAYLAND, GdkWindowImplWaylandClass))
#define GDK_IS_WINDOW_IMPL_WAYLAND(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WINDOW_IMPL_WAYLAND))
#define GDK_IS_WINDOW_IMPL_WAYLAND_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WINDOW_IMPL_WAYLAND))
#define GDK_WINDOW_IMPL_WAYLAND_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WINDOW_IMPL_WAYLAND, GdkWindowImplWaylandClass))

typedef struct _GdkWindowImplWayland GdkWindowImplWayland;
typedef struct _GdkWindowImplWaylandClass GdkWindowImplWaylandClass;

struct _GdkWindowImplWayland
{
  GdkWindowImpl parent_instance;

  GdkWindow *wrapper;

  GdkCursor *cursor;

  gint8 toplevel_window_type;

  struct wl_surface *surface;
  unsigned int mapped : 1;
  GdkWindow *transient_for;

  cairo_surface_t *cairo_surface;
  cairo_surface_t *server_surface;
  GLuint texture;
  uint32_t resize_edges;

  /* Set if the window, or any descendent of it, is the server's focus window
   */
  guint has_focus_window : 1;

  /* Set if window->has_focus_window and the focus isn't grabbed elsewhere.
   */
  guint has_focus : 1;

  /* Set if the pointer is inside this window. (This is needed for
   * for focus tracking)
   */
  guint has_pointer : 1;
  
  /* Set if the window is a descendent of the focus window and the pointer is
   * inside it. (This is the case where the window will receive keystroke
   * events even window->has_focus_window is FALSE)
   */
  guint has_pointer_focus : 1;

  /* Set if we are requesting these hints */
  guint skip_taskbar_hint : 1;
  guint skip_pager_hint : 1;
  guint urgency_hint : 1;

  guint on_all_desktops : 1;   /* _NET_WM_STICKY == 0xFFFFFFFF */

  guint have_sticky : 1;	/* _NET_WM_STATE_STICKY */
  guint have_maxvert : 1;       /* _NET_WM_STATE_MAXIMIZED_VERT */
  guint have_maxhorz : 1;       /* _NET_WM_STATE_MAXIMIZED_HORZ */
  guint have_fullscreen : 1;    /* _NET_WM_STATE_FULLSCREEN */

  gulong map_serial;	/* Serial of last transition from unmapped */

  cairo_surface_t *icon_pixmap;
  cairo_surface_t *icon_mask;

  /* Time of most recent user interaction. */
  gulong user_time;
};

struct _GdkWindowImplWaylandClass
{
  GdkWindowImplClass parent_class;
};

G_DEFINE_TYPE (GdkWindowImplWayland, _gdk_window_impl_wayland, GDK_TYPE_WINDOW_IMPL)

static void
_gdk_window_impl_wayland_init (GdkWindowImplWayland *impl)
{
  impl->toplevel_window_type = -1;
}

/**
 * _gdk_wayland_window_update_size:
 * @drawable: a #GdkDrawableImplWayland.
 * 
 * Updates the state of the drawable (in particular the drawable's
 * cairo surface) when its size has changed.
 **/
void
_gdk_wayland_window_update_size (GdkWindow *window,
				 int32_t width, int32_t height, uint32_t edges)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkRectangle area;
  cairo_region_t *region;

  if (impl->cairo_surface)
    {
      cairo_surface_destroy (impl->cairo_surface);
      impl->cairo_surface = NULL;
    }

  window->width = width;
  window->height = height;
  impl->resize_edges = edges;

  area.x = 0;
  area.y = 0;
  area.width = window->width;
  area.height = window->height;

  region = cairo_region_create_rectangle (&area);
  _gdk_window_invalidate_for_expose (window, region);
  cairo_region_destroy (region);
}

GdkWindow *
_gdk_wayland_screen_create_root_window (GdkScreen *screen,
					int width, int height)
{
  GdkWindow *window;
  GdkWindowImplWayland *impl;

  window = _gdk_display_create_window (gdk_screen_get_display (screen));
  window->impl = g_object_new (GDK_TYPE_WINDOW_IMPL_WAYLAND, NULL);
  window->impl_window = window;
  window->visual = gdk_screen_get_system_visual (screen);

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  impl->wrapper = GDK_WINDOW (window);

  window->window_type = GDK_WINDOW_ROOT;
  window->depth = 32;

  window->x = 0;
  window->y = 0;
  window->abs_x = 0;
  window->abs_y = 0;
  window->width = width;
  window->height = height;
  window->viewable = TRUE;

  /* see init_randr_support() in gdkscreen-wayland.c */
  window->event_mask = GDK_STRUCTURE_MASK;

  return window;
}

static const gchar *
get_default_title (void)
{
  const char *title;

  title = g_get_application_name ();
  if (!title)
    title = g_get_prgname ();
  if (!title)
    title = "";

  return title;
}

void
_gdk_wayland_display_create_window_impl (GdkDisplay    *display,
					 GdkWindow     *window,
					 GdkWindow     *real_parent,
					 GdkScreen     *screen,
					 GdkEventMask   event_mask,
					 GdkWindowAttr *attributes,
					 gint           attributes_mask)
{
  GdkWindowImplWayland *impl;
  const char *title;

  impl = g_object_new (GDK_TYPE_WINDOW_IMPL_WAYLAND, NULL);
  window->impl = GDK_WINDOW_IMPL (impl);
  impl->wrapper = GDK_WINDOW (window);

  if (window->width > 65535 ||
      window->height > 65535)
    {
      g_warning ("Native Windows wider or taller than 65535 pixels are not supported");

      if (window->width > 65535)
	window->width = 65535;
      if (window->height > 65535)
	window->height = 65535;
    }

  g_object_ref (window);

  switch (GDK_WINDOW_TYPE (window))
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_TEMP:
      if (attributes_mask & GDK_WA_TITLE)
	title = attributes->title;
      else
	title = get_default_title ();

      gdk_window_set_title (window, title);
      break;

    case GDK_WINDOW_CHILD:
    default:
      break;
    }

  if (attributes_mask & GDK_WA_TYPE_HINT)
    gdk_window_set_type_hint (window, attributes->type_hint);
}

static const cairo_user_data_key_t gdk_wayland_cairo_key;

typedef struct _GdkWaylandCairoSurfaceData {
  EGLImageKHR image;
  GLuint texture;
  struct wl_egl_pixmap *pixmap;
  struct wl_buffer *buffer;
  GdkDisplayWayland *display;
  int32_t width, height;
} GdkWaylandCairoSurfaceData;

static void
gdk_wayland_window_attach_image (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandCairoSurfaceData *data;
  int32_t server_width, server_height, dx, dy;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (impl->server_surface == impl->cairo_surface)
    return;

  if (impl->server_surface)
    {
      data = cairo_surface_get_user_data (impl->server_surface,
					  &gdk_wayland_cairo_key);
      server_width = data->width;
      server_height = data->height;
      cairo_surface_destroy (impl->server_surface);
    }
  else
    {
      server_width = 0;
      server_height = 0;
    }

  impl->server_surface = cairo_surface_reference (impl->cairo_surface);
  data = cairo_surface_get_user_data (impl->cairo_surface,
				      &gdk_wayland_cairo_key);
  if (!data->buffer)
    data->buffer =
      wl_egl_pixmap_create_buffer(data->pixmap);

  if (impl->resize_edges & WL_SHELL_RESIZE_LEFT)
    dx = server_width - data->width;
  else
    dx = 0;

  if (impl->resize_edges & WL_SHELL_RESIZE_TOP)
    dy = server_height - data->height;
  else
    dy = 0;

  wl_surface_attach (impl->surface, data->buffer, dx, dy);
}

static void
gdk_window_impl_wayland_finalize (GObject *object)
{
  GdkWindowImplWayland *impl;

  g_return_if_fail (GDK_IS_WINDOW_IMPL_WAYLAND (object));

  impl = GDK_WINDOW_IMPL_WAYLAND (object);

  if (impl->cursor)
    gdk_cursor_unref (impl->cursor);
  if (impl->server_surface)
    cairo_surface_destroy (impl->server_surface);

  G_OBJECT_CLASS (_gdk_window_impl_wayland_parent_class)->finalize (object);
}

static void
gdk_wayland_cairo_surface_destroy (void *p)
{
  GdkWaylandCairoSurfaceData *data = p;

  data->display->destroy_image (data->display->egl_display, data->image);
  cairo_device_acquire(data->display->cairo_device);
  glDeleteTextures(1, &data->texture);
  cairo_device_release(data->display->cairo_device);
  if (data->buffer)
    wl_buffer_destroy(data->buffer);
  g_free(data);
}

static cairo_surface_t *
gdk_wayland_create_cairo_surface (GdkDisplayWayland *display,
				  int width, int height)
{
  GdkWaylandCairoSurfaceData *data;
  cairo_surface_t *surface;
  struct wl_visual *visual;

  data = g_new (GdkWaylandCairoSurfaceData, 1);
  data->display = display;
  data->buffer = NULL;
  visual = display->premultiplied_argb_visual;
  data->width = width;
  data->height = height;
  data->pixmap = wl_egl_pixmap_create(width, height, visual, 0);
  data->image =
    display->create_image(display->egl_display, NULL, EGL_NATIVE_PIXMAP_KHR,
			  (EGLClientBuffer) data->pixmap, NULL);

  cairo_device_acquire(display->cairo_device);
  glGenTextures(1, &data->texture);
  glBindTexture(GL_TEXTURE_2D, data->texture);
  display->image_target_texture_2d(GL_TEXTURE_2D, data->image);
  cairo_device_release(display->cairo_device);

  surface = cairo_gl_surface_create_for_texture(display->cairo_device,
						CAIRO_CONTENT_COLOR_ALPHA,
						data->texture, width, height);

  cairo_surface_set_user_data (surface, &gdk_wayland_cairo_key,
			       data, gdk_wayland_cairo_surface_destroy);

  if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
    fprintf (stderr, "create gl surface failed\n");

  return surface;
}

static cairo_surface_t *
gdk_wayland_window_ref_cairo_surface (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkDisplayWayland *display_wayland =
    GDK_DISPLAY_WAYLAND (gdk_window_get_display (impl->wrapper));

  if (GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  if (!impl->cairo_surface)
    {
      impl->cairo_surface =
	gdk_wayland_create_cairo_surface (display_wayland,
				      impl->wrapper->width,
				      impl->wrapper->height);
    }

  cairo_surface_reference (impl->cairo_surface);

  return impl->cairo_surface;
}

static void
gdk_wayland_window_set_user_time (GdkWindow *window, guint32 user_time)
{
}

static void
gdk_wayland_window_map (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWindowImplWayland *parent;
  GdkDisplayWayland *display_wayland =
		GDK_DISPLAY_WAYLAND (gdk_window_get_display (impl->wrapper));

  if (!impl->mapped)
    {
      if (impl->transient_for)
	{
	  fprintf(stderr, "parent surface: %d, %d, transient surface %d, %d\n",
		  impl->transient_for->x,
		  impl->transient_for->y,
		  window->x,
		  window->y);

	  parent = GDK_WINDOW_IMPL_WAYLAND (impl->transient_for->impl);
	  wl_shell_set_transient (display_wayland->shell, impl->surface, parent->surface,
				    window->x, window->y, 0);
	}
      else
      wl_shell_set_toplevel (display_wayland->shell, impl->surface);
      impl->mapped = TRUE;
    }
}

static void
gdk_wayland_window_show (GdkWindow *window, gboolean already_mapped)
{
  GdkDisplay *display;
  GdkDisplayWayland *display_wayland;
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkEvent *event;

  display = gdk_window_get_display (window);
  display_wayland = GDK_DISPLAY_WAYLAND (display);

  if (impl->user_time != 0 &&
      display_wayland->user_time != 0 &&
      XSERVER_TIME_IS_LATER (display_wayland->user_time, impl->user_time))
    gdk_wayland_window_set_user_time (window, impl->user_time);

  impl->surface = wl_compositor_create_surface(display_wayland->compositor);
  wl_surface_set_user_data(impl->surface, window);

  _gdk_make_event (window, GDK_MAP, NULL, FALSE);
  event = _gdk_make_event (window, GDK_VISIBILITY_NOTIFY, NULL, FALSE);
  event->visibility.state = GDK_VISIBILITY_UNOBSCURED;

  if (impl->cairo_surface)
    gdk_wayland_window_attach_image (window);
}

static void
gdk_wayland_window_hide (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (impl->surface)
    {
      wl_surface_destroy(impl->surface);
      impl->surface = NULL;
      cairo_surface_destroy(impl->server_surface);
      impl->server_surface = NULL;
      impl->mapped = FALSE;
    }

  _gdk_window_clear_update_area (window);
}

static void
gdk_window_wayland_withdraw (GdkWindow *window)
{
  GdkWindowImplWayland *impl;

  if (!window->destroyed)
    {
      if (GDK_WINDOW_IS_MAPPED (window))
	gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_WITHDRAWN);

      g_assert (!GDK_WINDOW_IS_MAPPED (window));

      impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
      if (impl->surface)
	{
	  wl_surface_destroy(impl->surface);
	  impl->surface = NULL;
	  cairo_surface_destroy(impl->server_surface);
	  impl->server_surface = NULL;
	  impl->mapped = FALSE;
	}
    }
}

static void
gdk_window_wayland_set_events (GdkWindow    *window,
			       GdkEventMask  event_mask)
{
  GDK_WINDOW (window)->event_mask = event_mask;
}

static GdkEventMask
gdk_window_wayland_get_events (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return 0;
  else
    return GDK_WINDOW (window)->event_mask;
}

static void
gdk_window_wayland_raise (GdkWindow *window)
{
  /* FIXME: wl_shell_raise() */
}

static void
gdk_window_wayland_lower (GdkWindow *window)
{
  /* FIXME: wl_shell_lower() */
}

static void
gdk_window_wayland_restack_under (GdkWindow *window,
			      GList *native_siblings)
{
}

static void
gdk_window_wayland_restack_toplevel (GdkWindow *window,
				 GdkWindow *sibling,
				 gboolean   above)
{
}

static void
gdk_window_wayland_move_resize (GdkWindow *window,
				gboolean   with_move,
				gint       x,
				gint       y,
				gint       width,
				gint       height)
{
  window->x = x;
  window->y = y;

  _gdk_wayland_window_update_size (window, width, height, 0);
}

static void
gdk_window_wayland_set_background (GdkWindow      *window,
			       cairo_pattern_t *pattern)
{
}

static gboolean
gdk_window_wayland_reparent (GdkWindow *window,
			     GdkWindow *new_parent,
			     gint       x,
			     gint       y)
{
  return FALSE;
}

static void
gdk_window_wayland_set_device_cursor (GdkWindow *window,
				      GdkDevice *device,
				      GdkCursor *cursor)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_DEVICE (device));

  if (!GDK_WINDOW_DESTROYED (window))
    GDK_DEVICE_GET_CLASS (device)->set_window_cursor (device, window, cursor);
}

static void
gdk_window_wayland_get_geometry (GdkWindow *window,
				 gint      *x,
				 gint      *y,
				 gint      *width,
				 gint      *height)
{
  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (x)
	*x = window->x;
      if (y)
	*y = window->y;
      if (width)
	*width = window->width;
      if (height)
	*height = window->height;
    }
}

static gint
gdk_window_wayland_get_root_coords (GdkWindow *window,
				gint       x,
				gint       y,
				gint      *root_x,
				gint      *root_y)
{
  /* We can't do this. */
  if (root_x)
    *root_x = 0;
  if (root_y)
    *root_y = 0;

  return 1;
}

static gboolean
gdk_window_wayland_get_device_state (GdkWindow       *window,
				     GdkDevice       *device,
				     gint            *x,
				     gint            *y,
				     GdkModifierType *mask)
{
  gboolean return_val;

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), FALSE);

  return_val = TRUE;

  if (!GDK_WINDOW_DESTROYED (window))
    {
      GdkWindow *child;

      GDK_DEVICE_GET_CLASS (device)->query_state (device, window,
						  NULL, &child,
						  NULL, NULL,
						  x, y, mask);
      return_val = (child != NULL);
    }

  return return_val;
}

static void
gdk_window_wayland_shape_combine_region (GdkWindow       *window,
					 const cairo_region_t *shape_region,
					 gint             offset_x,
					 gint             offset_y)
{
}

static void 
gdk_window_wayland_input_shape_combine_region (GdkWindow       *window,
					       const cairo_region_t *shape_region,
					       gint             offset_x,
					       gint             offset_y)
{
}

static gboolean
gdk_window_wayland_set_static_gravities (GdkWindow *window,
					 gboolean   use_static)
{
  return TRUE;
}

static gboolean
gdk_wayland_window_queue_antiexpose (GdkWindow *window,
				     cairo_region_t *area)
{
  return FALSE;
}

static void
gdk_wayland_window_translate (GdkWindow      *window,
			      cairo_region_t *area,
			      gint            dx,
			      gint            dy)
{
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = gdk_wayland_window_ref_cairo_surface (window->impl_window);
  cr = cairo_create (surface);
  cairo_surface_destroy (surface);

  gdk_cairo_region (cr, area);
  cairo_clip (cr);
  cairo_set_source_surface (cr, cairo_get_target (cr), dx, dy);
  cairo_push_group (cr);
  cairo_paint (cr);
  cairo_pop_group_to_source (cr);
  cairo_paint (cr);
  cairo_destroy (cr);
}

static void
gdk_wayland_window_destroy (GdkWindow *window,
			    gboolean   recursing,
			    gboolean   foreign_destroy)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (impl->cairo_surface)
    {
      cairo_surface_finish (impl->cairo_surface);
      cairo_surface_set_user_data (impl->cairo_surface, &gdk_wayland_cairo_key,
				   NULL, NULL);
    }

  if (impl->texture)
    glDeleteTextures(1, &impl->texture);

  if (!recursing && !foreign_destroy)
    {
      if (GDK_WINDOW_IMPL_WAYLAND (window->impl)->surface)
	wl_surface_destroy(GDK_WINDOW_IMPL_WAYLAND (window->impl)->surface);
    }
}

static void
gdk_window_wayland_destroy_foreign (GdkWindow *window)
{
}

static cairo_surface_t *
gdk_window_wayland_resize_cairo_surface (GdkWindow       *window,
					 cairo_surface_t *surface,
					 gint             width,
					 gint             height)
{
  return surface;
}

static cairo_region_t *
gdk_wayland_window_get_shape (GdkWindow *window)
{
  return NULL;
}

static cairo_region_t *
gdk_wayland_window_get_input_shape (GdkWindow *window)
{
  return NULL;
}

static void
gdk_wayland_window_focus (GdkWindow *window,
			  guint32    timestamp)
{
  /* FIXME: wl_shell_focus() */
}

static void
gdk_wayland_window_set_type_hint (GdkWindow        *window,
				  GdkWindowTypeHint hint)
{
  if (GDK_WINDOW_DESTROYED (window))
    return;

  switch (hint)
    {
    case GDK_WINDOW_TYPE_HINT_DIALOG:
    case GDK_WINDOW_TYPE_HINT_MENU:
    case GDK_WINDOW_TYPE_HINT_TOOLBAR:
    case GDK_WINDOW_TYPE_HINT_UTILITY:
    case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
    case GDK_WINDOW_TYPE_HINT_DOCK:
    case GDK_WINDOW_TYPE_HINT_DESKTOP:
    case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU:
    case GDK_WINDOW_TYPE_HINT_POPUP_MENU:
    case GDK_WINDOW_TYPE_HINT_TOOLTIP:
    case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
    case GDK_WINDOW_TYPE_HINT_COMBO:
    case GDK_WINDOW_TYPE_HINT_DND:
      break;
    default:
      g_warning ("Unknown hint %d passed to gdk_window_set_type_hint", hint);
      /* Fall thru */
    case GDK_WINDOW_TYPE_HINT_NORMAL:
      break;
    }
}

static GdkWindowTypeHint
gdk_wayland_window_get_type_hint (GdkWindow *window)
{
  return GDK_WINDOW_TYPE_HINT_NORMAL;
}

void
gdk_wayland_window_set_modal_hint (GdkWindow *window,
				   gboolean   modal)
{
}

static void
gdk_wayland_window_set_skip_taskbar_hint (GdkWindow *window,
					  gboolean   skips_taskbar)
{
}

static void
gdk_wayland_window_set_skip_pager_hint (GdkWindow *window,
					gboolean   skips_pager)
{
}

static void
gdk_wayland_window_set_urgency_hint (GdkWindow *window,
				     gboolean   urgent)
{
}

static void
gdk_wayland_window_set_geometry_hints (GdkWindow         *window,
				       const GdkGeometry *geometry,
				       GdkWindowHints     geom_mask)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  /*
   * GDK_HINT_POS
   * GDK_HINT_USER_POS
   * GDK_HINT_USER_SIZE
   * GDK_HINT_MIN_SIZE
   * GDK_HINT_MAX_SIZE
   * GDK_HINT_BASE_SIZE
   * GDK_HINT_RESIZE_INC
   * GDK_HINT_ASPECT
   * GDK_HINT_WIN_GRAVITY
   */
}

static void
gdk_wayland_window_set_title (GdkWindow   *window,
			      const gchar *title)
{
  g_return_if_fail (title != NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return;
}

static void
gdk_wayland_window_set_role (GdkWindow   *window,
			     const gchar *role)
{
}

static void
gdk_wayland_window_set_startup_id (GdkWindow   *window,
				   const gchar *startup_id)
{
}

static void
gdk_wayland_window_set_transient_for (GdkWindow *window,
				      GdkWindow *parent)
{
  GdkWindowImplWayland *impl;

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  impl->transient_for = parent;
}

static void
gdk_wayland_window_get_root_origin (GdkWindow *window,
				   gint      *x,
				   gint      *y)
{
  if (x)
    *x = 0;

  if (y)
    *y = 0;
}

static void
gdk_wayland_window_get_frame_extents (GdkWindow    *window,
				      GdkRectangle *rect)
{
  rect->x = window->x;
  rect->y = window->y;
  rect->width = window->width;
  rect->height = window->height;
}

static void
gdk_wayland_window_set_override_redirect (GdkWindow *window,
					  gboolean override_redirect)
{
}

static void
gdk_wayland_window_set_accept_focus (GdkWindow *window,
				     gboolean accept_focus)
{
}

static void
gdk_wayland_window_set_focus_on_map (GdkWindow *window,
				     gboolean focus_on_map)
{
  focus_on_map = focus_on_map != FALSE;

  if (window->focus_on_map != focus_on_map)
    {
      window->focus_on_map = focus_on_map;

      if ((!GDK_WINDOW_DESTROYED (window)) &&
	  (!window->focus_on_map) &&
	  WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
	gdk_wayland_window_set_user_time (window, 0);
    }
}

static void
gdk_wayland_window_set_icon_list (GdkWindow *window,
				  GList     *pixbufs)
{
}

static void
gdk_wayland_window_set_icon_name (GdkWindow   *window,
				  const gchar *name)
{
  if (GDK_WINDOW_DESTROYED (window))
    return;
}

static void
gdk_wayland_window_iconify (GdkWindow *window)
{
}

static void
gdk_wayland_window_deiconify (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    {  
      gdk_window_show (window);
    }
  else
    {
      /* Flip our client side flag, the real work happens on map. */
      gdk_synthesize_window_state (window, GDK_WINDOW_STATE_ICONIFIED, 0);
    }
}

static void
gdk_wayland_window_stick (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return;
}

static void
gdk_wayland_window_unstick (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return;
}

static void
gdk_wayland_window_maximize (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return;
}

static void
gdk_wayland_window_unmaximize (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return;
}

static void
gdk_wayland_window_fullscreen (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return;
}

static void
gdk_wayland_window_unfullscreen (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return;
}

static void
gdk_wayland_window_set_keep_above (GdkWindow *window,
				   gboolean   setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
}

static void
gdk_wayland_window_set_keep_below (GdkWindow *window, gboolean setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
}

static GdkWindow *
gdk_wayland_window_get_group (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return NULL;

  return NULL;
}

static void
gdk_wayland_window_set_group (GdkWindow *window,
			      GdkWindow *leader)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD);
  g_return_if_fail (leader == NULL || GDK_IS_WINDOW (leader));
}

static void
gdk_wayland_window_set_decorations (GdkWindow      *window,
				    GdkWMDecoration decorations)
{
}

static gboolean
gdk_wayland_window_get_decorations (GdkWindow       *window,
				    GdkWMDecoration *decorations)
{
  return FALSE;
}

static void
gdk_wayland_window_set_functions (GdkWindow    *window,
				  GdkWMFunction functions)
{
}

static void
gdk_wayland_window_begin_resize_drag (GdkWindow     *window,
				      GdkWindowEdge  edge,
				      gint           button,
				      gint           root_x,
				      gint           root_y,
				      guint32        timestamp)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkDeviceManager *dm;
  GdkWindowImplWayland *impl;
  GdkDevice *device;
  uint32_t grab_type;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  switch (edge)
    {
    case GDK_WINDOW_EDGE_NORTH_WEST:
      grab_type = WL_SHELL_RESIZE_TOP_LEFT;
      break;

    case GDK_WINDOW_EDGE_NORTH:
      grab_type = WL_SHELL_RESIZE_TOP;
      break;

    case GDK_WINDOW_EDGE_NORTH_EAST:
      grab_type = WL_SHELL_RESIZE_RIGHT;
      break;

    case GDK_WINDOW_EDGE_WEST:
      grab_type = WL_SHELL_RESIZE_LEFT;
      break;

    case GDK_WINDOW_EDGE_EAST:
      grab_type = WL_SHELL_RESIZE_RIGHT;
      break;

    case GDK_WINDOW_EDGE_SOUTH_WEST:
      grab_type = WL_SHELL_RESIZE_BOTTOM_LEFT;
      break;

    case GDK_WINDOW_EDGE_SOUTH:
      grab_type = WL_SHELL_RESIZE_BOTTOM;
      break;

    case GDK_WINDOW_EDGE_SOUTH_EAST:
      grab_type = WL_SHELL_RESIZE_BOTTOM_RIGHT;
      break;

    default:
      g_warning ("gdk_window_begin_resize_drag: bad resize edge %d!",
                 edge);
      return;
    }

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  dm = gdk_display_get_device_manager (display);
  device = gdk_device_manager_get_client_pointer (dm);

  wl_shell_resize(GDK_DISPLAY_WAYLAND (display)->shell, impl->surface,
		  _gdk_wayland_device_get_device (device),
		  timestamp, grab_type);
}

static void
gdk_wayland_window_begin_move_drag (GdkWindow *window,
				    gint       button,
				    gint       root_x,
				    gint       root_y,
				    guint32    timestamp)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkDeviceManager *dm;
  GdkWindowImplWayland *impl;
  GdkDevice *device;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  dm = gdk_display_get_device_manager (display);
  device = gdk_device_manager_get_client_pointer (dm);

  wl_shell_move(GDK_DISPLAY_WAYLAND (display)->shell, impl->surface,
		_gdk_wayland_device_get_device (device), timestamp);
}

static void
gdk_wayland_window_enable_synchronized_configure (GdkWindow *window)
{
}

static void
gdk_wayland_window_configure_finished (GdkWindow *window)
{
  if (!WINDOW_IS_TOPLEVEL (window))
    return;

  if (!GDK_IS_WINDOW_IMPL_WAYLAND (window->impl))
    return;
}

static void
gdk_wayland_window_set_opacity (GdkWindow *window,
				gdouble    opacity)
{
}

static void
gdk_wayland_window_set_composited (GdkWindow *window,
				   gboolean   composited)
{
}

static void
gdk_wayland_window_destroy_notify (GdkWindow *window)
{
  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE(window) != GDK_WINDOW_FOREIGN)
	g_warning ("GdkWindow %p unexpectedly destroyed", window);

      _gdk_window_destroy (window, TRUE);
    }

  g_object_unref (window);
}

static void
gdk_wayland_window_process_updates_recurse (GdkWindow *window,
					    cairo_region_t *region)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  cairo_rectangle_int_t rect;
  int i, n;

  if (impl->cairo_surface)
    gdk_wayland_window_attach_image (window);

  gdk_wayland_window_map (window);

  n = cairo_region_num_rectangles(region);
  for (i = 0; i < n; i++)
    {
      cairo_region_get_rectangle (region, i, &rect);
      wl_surface_damage (impl->surface,
			 rect.x, rect.y, rect.width, rect.height);
    }

  _gdk_window_process_updates_recurse (window, region);
}

static void
gdk_wayland_window_sync_rendering (GdkWindow *window)
{
}

static gboolean
gdk_wayland_window_simulate_key (GdkWindow      *window,
				 gint            x,
				 gint            y,
				 guint           keyval,
				 GdkModifierType modifiers,
				 GdkEventType    key_pressrelease)
{
  return FALSE;
}

static gboolean
gdk_wayland_window_simulate_button (GdkWindow      *window,
				    gint            x,
				    gint            y,
				    guint           button, /*1..3*/
				    GdkModifierType modifiers,
				    GdkEventType    button_pressrelease)
{
  return FALSE;
}

static gboolean
gdk_wayland_window_get_property (GdkWindow   *window,
				 GdkAtom      property,
				 GdkAtom      type,
				 gulong       offset,
				 gulong       length,
				 gint         pdelete,
				 GdkAtom     *actual_property_type,
				 gint        *actual_format_type,
				 gint        *actual_length,
				 guchar     **data)
{
  return FALSE;
}

static void
gdk_wayland_window_change_property (GdkWindow    *window,
				    GdkAtom       property,
				    GdkAtom       type,
				    gint          format,
				    GdkPropMode   mode,
				    const guchar *data,
				    gint          nelements)
{
}

static void
gdk_wayland_window_delete_property (GdkWindow *window,
				    GdkAtom    property)
{
}

static void
_gdk_window_impl_wayland_class_init (GdkWindowImplWaylandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkWindowImplClass *impl_class = GDK_WINDOW_IMPL_CLASS (klass);

  object_class->finalize = gdk_window_impl_wayland_finalize;

  impl_class->ref_cairo_surface = gdk_wayland_window_ref_cairo_surface;
  impl_class->show = gdk_wayland_window_show;
  impl_class->hide = gdk_wayland_window_hide;
  impl_class->withdraw = gdk_window_wayland_withdraw;
  impl_class->set_events = gdk_window_wayland_set_events;
  impl_class->get_events = gdk_window_wayland_get_events;
  impl_class->raise = gdk_window_wayland_raise;
  impl_class->lower = gdk_window_wayland_lower;
  impl_class->restack_under = gdk_window_wayland_restack_under;
  impl_class->restack_toplevel = gdk_window_wayland_restack_toplevel;
  impl_class->move_resize = gdk_window_wayland_move_resize;
  impl_class->set_background = gdk_window_wayland_set_background;
  impl_class->reparent = gdk_window_wayland_reparent;
  impl_class->set_device_cursor = gdk_window_wayland_set_device_cursor;
  impl_class->get_geometry = gdk_window_wayland_get_geometry;
  impl_class->get_root_coords = gdk_window_wayland_get_root_coords;
  impl_class->get_device_state = gdk_window_wayland_get_device_state;
  impl_class->shape_combine_region = gdk_window_wayland_shape_combine_region;
  impl_class->input_shape_combine_region = gdk_window_wayland_input_shape_combine_region;
  impl_class->set_static_gravities = gdk_window_wayland_set_static_gravities;
  impl_class->queue_antiexpose = gdk_wayland_window_queue_antiexpose;
  impl_class->translate = gdk_wayland_window_translate;
  impl_class->destroy = gdk_wayland_window_destroy;
  impl_class->destroy_foreign = gdk_window_wayland_destroy_foreign;
  impl_class->resize_cairo_surface = gdk_window_wayland_resize_cairo_surface;
  impl_class->get_shape = gdk_wayland_window_get_shape;
  impl_class->get_input_shape = gdk_wayland_window_get_input_shape;
  /* impl_class->beep */

  impl_class->focus = gdk_wayland_window_focus;
  impl_class->set_type_hint = gdk_wayland_window_set_type_hint;
  impl_class->get_type_hint = gdk_wayland_window_get_type_hint;
  impl_class->set_modal_hint = gdk_wayland_window_set_modal_hint;
  impl_class->set_skip_taskbar_hint = gdk_wayland_window_set_skip_taskbar_hint;
  impl_class->set_skip_pager_hint = gdk_wayland_window_set_skip_pager_hint;
  impl_class->set_urgency_hint = gdk_wayland_window_set_urgency_hint;
  impl_class->set_geometry_hints = gdk_wayland_window_set_geometry_hints;
  impl_class->set_title = gdk_wayland_window_set_title;
  impl_class->set_role = gdk_wayland_window_set_role;
  impl_class->set_startup_id = gdk_wayland_window_set_startup_id;
  impl_class->set_transient_for = gdk_wayland_window_set_transient_for;
  impl_class->get_root_origin = gdk_wayland_window_get_root_origin;
  impl_class->get_frame_extents = gdk_wayland_window_get_frame_extents;
  impl_class->set_override_redirect = gdk_wayland_window_set_override_redirect;
  impl_class->set_accept_focus = gdk_wayland_window_set_accept_focus;
  impl_class->set_focus_on_map = gdk_wayland_window_set_focus_on_map;
  impl_class->set_icon_list = gdk_wayland_window_set_icon_list;
  impl_class->set_icon_name = gdk_wayland_window_set_icon_name;
  impl_class->iconify = gdk_wayland_window_iconify;
  impl_class->deiconify = gdk_wayland_window_deiconify;
  impl_class->stick = gdk_wayland_window_stick;
  impl_class->unstick = gdk_wayland_window_unstick;
  impl_class->maximize = gdk_wayland_window_maximize;
  impl_class->unmaximize = gdk_wayland_window_unmaximize;
  impl_class->fullscreen = gdk_wayland_window_fullscreen;
  impl_class->unfullscreen = gdk_wayland_window_unfullscreen;
  impl_class->set_keep_above = gdk_wayland_window_set_keep_above;
  impl_class->set_keep_below = gdk_wayland_window_set_keep_below;
  impl_class->get_group = gdk_wayland_window_get_group;
  impl_class->set_group = gdk_wayland_window_set_group;
  impl_class->set_decorations = gdk_wayland_window_set_decorations;
  impl_class->get_decorations = gdk_wayland_window_get_decorations;
  impl_class->set_functions = gdk_wayland_window_set_functions;
  impl_class->begin_resize_drag = gdk_wayland_window_begin_resize_drag;
  impl_class->begin_move_drag = gdk_wayland_window_begin_move_drag;
  impl_class->enable_synchronized_configure = gdk_wayland_window_enable_synchronized_configure;
  impl_class->configure_finished = gdk_wayland_window_configure_finished;
  impl_class->set_opacity = gdk_wayland_window_set_opacity;
  impl_class->set_composited = gdk_wayland_window_set_composited;
  impl_class->destroy_notify = gdk_wayland_window_destroy_notify;
  impl_class->get_drag_protocol = _gdk_wayland_window_get_drag_protocol;
  impl_class->register_dnd = _gdk_wayland_window_register_dnd;
  impl_class->drag_begin = _gdk_wayland_window_drag_begin;
  impl_class->process_updates_recurse = gdk_wayland_window_process_updates_recurse;
  impl_class->sync_rendering = gdk_wayland_window_sync_rendering;
  impl_class->simulate_key = gdk_wayland_window_simulate_key;
  impl_class->simulate_button = gdk_wayland_window_simulate_button;
  impl_class->get_property = gdk_wayland_window_get_property;
  impl_class->change_property = gdk_wayland_window_change_property;
  impl_class->delete_property = gdk_wayland_window_delete_property;
}
