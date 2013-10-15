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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <netinet/in.h>
#include <unistd.h>

#include "gdk.h"
#include "gdkwayland.h"

#include "gdkwindow.h"
#include "gdkwindowimpl.h"
#include "gdkdisplay-wayland.h"
#include "gdkframeclockprivate.h"
#include "gdkprivate-wayland.h"
#include "gdkinternals.h"
#include "gdkdeviceprivate.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>

#define WL_SURFACE_HAS_BUFFER_SCALE 3

#define WINDOW_IS_TOPLEVEL_OR_FOREIGN(window) \
  (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&   \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_OFFSCREEN)

#define WINDOW_IS_TOPLEVEL(window)                   \
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

G_DEFINE_TYPE (GdkWaylandWindow, gdk_wayland_window, GDK_TYPE_WINDOW)

static void
gdk_wayland_window_class_init (GdkWaylandWindowClass *wayland_window_class)
{
}

static void
gdk_wayland_window_init (GdkWaylandWindow *wayland_window)
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

  /* The wl_outputs that this window currently touches */
  GSList *outputs;

  struct wl_surface *surface;
  struct wl_shell_surface *shell_surface;
  struct gtk_surface *gtk_surface;
  unsigned int mapped : 1;
  unsigned int fullscreen : 1;
  unsigned int use_custom_surface : 1;
  unsigned int pending_commit : 1;
  GdkWindowTypeHint hint;
  GdkWindow *transient_for;

  /* The surface which is being "drawn to" to */
  cairo_surface_t *cairo_surface;

  /* The surface that was the last surface the Wayland buffer from which was attached
   * to the Wayland surface. It will be the same as cairo_surface after a call
   * to gdk_wayland_window_attach_image. But after a call to
   * gdk_wayland_window_update_size and then
   * gdk_wayland_window_ref_cairo_surface the above pointer will be different.
   */
  cairo_surface_t *server_surface;

  gchar *title;

  uint32_t resize_edges;

  int focus_count;

  /* Time of most recent user interaction. */
  gulong user_time;

  GdkGeometry geometry_hints;
  GdkWindowHints geometry_mask;

  guint32 grab_time;
  GdkDevice *grab_device;
  struct wl_seat *grab_input_seat;

  struct
    {
      int width, height;
    } saved_fullscreen, saved_maximized;

  gint64 pending_frame_counter;
  guint32 scale;
};

struct _GdkWindowImplWaylandClass
{
  GdkWindowImplClass parent_class;
};

static void gdk_wayland_window_configure (GdkWindow *window,
                                          int        width,
                                          int        height,
                                          int        edges);

G_DEFINE_TYPE (GdkWindowImplWayland, _gdk_window_impl_wayland, GDK_TYPE_WINDOW_IMPL)

static void
_gdk_window_impl_wayland_init (GdkWindowImplWayland *impl)
{
  impl->scale = 1;
}

void
_gdk_wayland_window_add_focus (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  impl->focus_count++;
  if (impl->focus_count == 1)
    gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_FOCUSED);
}

void
_gdk_wayland_window_remove_focus (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  impl->focus_count--;
  if (impl->focus_count == 0)
    gdk_synthesize_window_state (window, GDK_WINDOW_STATE_FOCUSED, 0);
}

/*
 * gdk_wayland_window_update_size:
 * @drawable: a #GdkDrawableImplWayland.
 *
 * Updates the state of the drawable (in particular the drawable's
 * cairo surface) when its size has changed.
 */
static void
gdk_wayland_window_update_size (GdkWindow *window,
                                int32_t    width,
                                int32_t    height,
                                uint32_t   edges)
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
                                        int        width,
                                        int        height)
{
  GdkWindow *window;
  GdkWindowImplWayland *impl;

  window = _gdk_display_create_window (gdk_screen_get_display (screen));
  window->impl = g_object_new (GDK_TYPE_WINDOW_IMPL_WAYLAND, NULL);
  window->impl_window = window;
  window->visual = gdk_screen_get_system_visual (screen);

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  impl->wrapper = GDK_WINDOW (window);
  if (gdk_screen_get_n_monitors(screen) > 0)
    impl->scale = gdk_screen_get_monitor_scale_factor (screen, 0);
  else
    impl->scale = 1;

  /* logical 1x1 fake buffer */
  impl->cairo_surface =
          cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                      impl->scale,
                                      impl->scale);

#ifdef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
  cairo_surface_set_device_scale (impl->cairo_surface, impl->scale, impl->scale);
#endif

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

static void
fill_presentation_time_from_frame_time (GdkFrameTimings *timings,
                                        guint32          frame_time)
{
  /* The timestamp in a wayland frame is a msec time value that in some
   * way reflects the time at which the server started drawing the frame.
   * This is not useful from our perspective.
   *
   * However, for the DRM backend of Weston, on reasonably recent
   * Linux, we know that the time is the
   * clock_gettime (CLOCK_MONOTONIC) value at the vblank, and that
   * backend starts drawing immediately after receiving the vblank
   * notification. If we detect this, and make the assumption that the
   * compositor will finish drawing before the next vblank, we can
   * then determine the presentation time as the frame time we
   * received plus one refresh interval.
   *
   * If a backend is using clock_gettime(CLOCK_MONOTONIC), but not
   * picking values right at the vblank, then the presentation times
   * we compute won't be accurate, but not really worse than then
   * the alternative of not providing presentation times at all.
   *
   * The complexity here is dealing with the fact that we receive
   * only the low 32 bits of the CLOCK_MONOTONIC value in milliseconds.
   */
  gint64 now_monotonic = g_get_monotonic_time ();
  gint64 now_monotonic_msec = now_monotonic / 1000;
  uint32_t now_monotonic_low = (uint32_t)now_monotonic_msec;

  if (frame_time - now_monotonic_low < 1000 ||
      frame_time - now_monotonic_low > (uint32_t)-1000)
    {
      /* Timestamp we received is within one second of the current time.
       */
      gint64 last_frame_time = now_monotonic + (gint64)1000 * (gint32)(frame_time - now_monotonic_low);
      if ((gint32)now_monotonic_low < 0 && (gint32)frame_time > 0)
        last_frame_time += (gint64)1000 * G_GINT64_CONSTANT(0x100000000);
      else if ((gint32)now_monotonic_low > 0 && (gint32)frame_time < 0)
        last_frame_time -= (gint64)1000 * G_GINT64_CONSTANT(0x100000000);

      timings->presentation_time = last_frame_time + timings->refresh_interval;
    }
}

static void
frame_callback (void               *data,
                struct wl_callback *callback,
                uint32_t            time)
{
  GdkWindow *window = data;
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  GdkFrameClock *clock = gdk_window_get_frame_clock (window);
  GdkFrameTimings *timings;

  wl_callback_destroy (callback);
  _gdk_frame_clock_thaw (clock);

  timings = gdk_frame_clock_get_timings (clock, impl->pending_frame_counter);
  impl->pending_frame_counter = 0;

  if (timings == NULL)
    return;

  timings->refresh_interval = 16667; /* default to 1/60th of a second */
  if (impl->outputs)
    {
      /* We pick a random output out of the outputs that the window touches
       * The rate here is in milli-hertz */
      int refresh_rate = _gdk_wayland_screen_get_output_refresh_rate (wayland_display->screen,
                                                                      impl->outputs->data);
      if (refresh_rate != 0)
        timings->refresh_interval = G_GINT64_CONSTANT(1000000000) / refresh_rate;
    }

  fill_presentation_time_from_frame_time (timings, time);

  timings->complete = TRUE;

#ifdef G_ENABLE_DEBUG
  if ((_gdk_debug_flags & GDK_DEBUG_FRAMES) != 0)
    _gdk_frame_clock_debug_print_timings (clock, timings);
#endif
}

static const struct wl_callback_listener listener = {
  frame_callback
};

static void
on_frame_clock_before_paint (GdkFrameClock *clock,
                             GdkWindow     *window)
{
  GdkFrameTimings *timings = gdk_frame_clock_get_current_timings (clock);
  gint64 presentation_time;
  gint64 refresh_interval;

  gdk_frame_clock_get_refresh_info (clock,
                                    timings->frame_time,
                                    &refresh_interval, &presentation_time);

  if (presentation_time != 0)
    {
      /* Assume the algorithm used by the DRM backend of Weston - it
       * starts drawing at the next vblank after receiving the commit
       * for this frame, and presentation occurs at the vblank
       * after that.
       */
      timings->predicted_presentation_time = presentation_time + refresh_interval;
    }
  else
    {
      /* As above, but we don't actually know the phase of the vblank,
       * so just assume that we're half way through a refresh cycle.
       */
      timings->predicted_presentation_time = timings->frame_time + refresh_interval / 2 + refresh_interval;
    }
}

static const cairo_user_data_key_t gdk_wayland_cairo_key;

typedef struct _GdkWaylandCairoSurfaceData {
  gpointer buf;
  size_t buf_length;
  struct wl_shm_pool *pool;
  struct wl_buffer *buffer;
  GdkWaylandDisplay *display;
  int32_t width, height;
  uint32_t scale;
  gboolean busy;
} GdkWaylandCairoSurfaceData;

static void
on_frame_clock_after_paint (GdkFrameClock *clock,
                            GdkWindow     *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandCairoSurfaceData *data;
  struct wl_callback *callback;

  if (!impl->pending_commit)
    return;

  impl->pending_commit = FALSE;
  impl->pending_frame_counter = gdk_frame_clock_get_frame_counter (clock);

  callback = wl_surface_frame (impl->surface);
  wl_callback_add_listener (callback, &listener, window);
  _gdk_frame_clock_freeze (clock);

  wl_surface_commit (impl->surface);

  data = cairo_surface_get_user_data (impl->cairo_surface,
                                      &gdk_wayland_cairo_key);
  data->busy = TRUE;
}

static void
window_update_scale (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  guint32 scale;
  GSList *l;

  if (wayland_display->compositor_version < WL_SURFACE_HAS_BUFFER_SCALE)
    {
      /* We can't set the scale on this surface */
      impl->scale = 1;
      return;
    }

  scale = 1;
  for (l = impl->outputs; l != NULL; l = l->next)
    {
      guint32 output_scale =
        _gdk_wayland_screen_get_output_scale (wayland_display->screen,
                                              l->data);
      scale = MAX (scale, output_scale);
    }

#ifndef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
  /* Don't announce a scale if we can't support it */
  scale = 1;
#endif

  if (scale != impl->scale)
    {
      impl->scale = scale;

      /* Notify app that scale changed */
      gdk_wayland_window_configure (window, window->width, window->height, impl->resize_edges);
    }
}

static void
on_monitors_changed (GdkScreen *screen,
                     GdkWindow *window)
{
  window_update_scale (window);
}


static void gdk_wayland_window_create_surface (GdkWindow *window);

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
  GdkFrameClock *frame_clock;
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

  /* More likely to be right than just assuming 1 */
  impl->scale = gdk_screen_get_monitor_scale_factor (screen, 0);

  impl->title = NULL;

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

  gdk_wayland_window_create_surface (window);

  if (attributes_mask & GDK_WA_TYPE_HINT)
    gdk_window_set_type_hint (window, attributes->type_hint);

  frame_clock = gdk_window_get_frame_clock (window);

  g_signal_connect (frame_clock, "before-paint",
                    G_CALLBACK (on_frame_clock_before_paint), window);
  g_signal_connect (frame_clock, "after-paint",
                    G_CALLBACK (on_frame_clock_after_paint), window);

  g_signal_connect (screen, "monitors-changed",
                    G_CALLBACK (on_monitors_changed), window);
}

static void
gdk_wayland_window_attach_image (GdkWindow *window)
{
  GdkWaylandDisplay *display;
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandCairoSurfaceData *data;
  int32_t server_width, server_height, dx, dy;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* The wayland surface is attached to a buffer that is from the old "drawn
   * to" surface. Unref the surface and restore the state.
   */
  if (impl->server_surface)
    {
      data = cairo_surface_get_user_data (impl->server_surface,
                                          &gdk_wayland_cairo_key);

      /* Save the old dimensions used for the surface */
      server_width = data->width;
      server_height = data->height;

      cairo_surface_destroy (impl->server_surface);
    }
  else
    {
      server_width = 0;
      server_height = 0;
    }

  /* Save the current "drawn to" surface for future calls into here */
  impl->server_surface = cairo_surface_reference (impl->cairo_surface);

  /* Get a Wayland buffer from this new surface */
  data = cairo_surface_get_user_data (impl->cairo_surface,
                                      &gdk_wayland_cairo_key);

  if (impl->resize_edges & WL_SHELL_SURFACE_RESIZE_LEFT)
    dx = server_width - data->width;
  else
    dx = 0;

  if (impl->resize_edges & WL_SHELL_SURFACE_RESIZE_TOP)
    dy = server_height - data->height;
  else
    dy = 0;

  /* Attach this new buffer to the surface */
  wl_surface_attach (impl->surface, data->buffer, dx, dy);

  /* Only set the buffer scale if supported by the compositor */
  display = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  if (display->compositor_version >= WL_SURFACE_HAS_BUFFER_SCALE)
    wl_surface_set_buffer_scale (impl->surface, data->scale);

  impl->pending_commit = TRUE;
}

static void
gdk_wayland_cairo_surface_destroy (void *p)
{
  GdkWaylandCairoSurfaceData *data = p;

  if (data->buffer)
    wl_buffer_destroy (data->buffer);

  if (data->pool)
    wl_shm_pool_destroy (data->pool);

  munmap (data->buf, data->buf_length);
  g_free (data);
}


struct wl_shm_pool *
_create_shm_pool (struct wl_shm  *shm,
                  int             width,
                  int             height,
                  size_t         *buf_length,
                  void          **data_out)
{
  char filename[] = "/tmp/wayland-shm-XXXXXX";
  struct wl_shm_pool *pool;
  int fd, size, stride;
  void *data;

  fd = mkstemp (filename);
  if (fd < 0)
    {
      g_critical (G_STRLOC ": Unable to create temporary file (%s): %s",
                  filename, g_strerror (errno));
      return NULL;
    }

  stride = width * 4;
  size = stride * height;
  if (ftruncate (fd, size) < 0)
    {
      g_critical (G_STRLOC ": Truncating temporary file failed: %s",
                  g_strerror (errno));
      close (fd);
      return NULL;
    }

  data = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  unlink (filename);

  if (data == MAP_FAILED)
    {
      g_critical (G_STRLOC ": mmap'ping temporary file failed: %s",
                  g_strerror (errno));
      close (fd);
      return NULL;
    }

  pool = wl_shm_create_pool(shm, fd, size);

  close (fd);

  *data_out = data;
  *buf_length = size;

  return pool;
}


static void
buffer_release_callback (void             *_data,
                         struct wl_buffer *wl_buffer)
{
  GdkWaylandCairoSurfaceData *data = _data;

  data->busy = FALSE;
}

static const struct wl_buffer_listener buffer_listener = {
  buffer_release_callback
};

static cairo_surface_t *
gdk_wayland_create_cairo_surface (GdkWaylandDisplay *display,
                                  int                width,
                                  int                height,
                                  guint              scale)
{
  GdkWaylandCairoSurfaceData *data;
  cairo_surface_t *surface = NULL;
  cairo_status_t status;
  int stride;

  data = g_new (GdkWaylandCairoSurfaceData, 1);
  data->display = display;
  data->buffer = NULL;
  data->width = width;
  data->height = height;
  data->scale = scale;
  data->busy = FALSE;

  stride = width * 4;

  data->pool = _create_shm_pool (display->shm,
                                 width*scale, height*scale,
                                 &data->buf_length,
                                 &data->buf);

  data->buffer = wl_shm_pool_create_buffer (data->pool, 0,
                                            width*scale, height*scale,
                                            stride*scale, WL_SHM_FORMAT_ARGB8888);
  wl_buffer_add_listener (data->buffer, &buffer_listener, data);

  surface = cairo_image_surface_create_for_data (data->buf,
                                                 CAIRO_FORMAT_ARGB32,
                                                 width*scale,
                                                 height*scale,
                                                 stride*scale);

  cairo_surface_set_user_data (surface, &gdk_wayland_cairo_key,
                               data, gdk_wayland_cairo_surface_destroy);

#ifdef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
  cairo_surface_set_device_scale (surface, scale, scale);
#endif

  status = cairo_surface_status (surface);
  if (status != CAIRO_STATUS_SUCCESS)
    {
      g_critical (G_STRLOC ": Unable to create Cairo image surface: %s",
                  cairo_status_to_string (status));
    }

  return surface;
}

static void
gdk_wayland_window_ensure_cairo_surface (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  if (!impl->cairo_surface)
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_window_get_display (impl->wrapper));

      impl->cairo_surface =
        gdk_wayland_create_cairo_surface (display_wayland,
                                          impl->wrapper->width,
                                          impl->wrapper->height,
                                          impl->scale);
    }
}

/* Unlike other backends the Cairo surface is not just a cheap wrapper
 * around some other backing.  It is the buffer itself.
 */
static cairo_surface_t *
gdk_wayland_window_ref_cairo_surface (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  gdk_wayland_window_ensure_cairo_surface (window);

  cairo_surface_reference (impl->cairo_surface);

  return impl->cairo_surface;
}

static cairo_surface_t *
gdk_wayland_window_create_similar_image_surface (GdkWindow *     window,
                                                 cairo_format_t  format,
                                                 int             width,
                                                 int             height)
{
  return cairo_image_surface_create (format, width, height);
}

static gboolean
gdk_window_impl_wayland_begin_paint_region (GdkWindow            *window,
                                            const cairo_region_t *region)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandCairoSurfaceData *data;

  gdk_wayland_window_ensure_cairo_surface (window);
  data = cairo_surface_get_user_data (impl->cairo_surface,
                                      &gdk_wayland_cairo_key);

  return data->busy;
}

static void
gdk_window_impl_wayland_finalize (GObject *object)
{
  GdkWindowImplWayland *impl;

  g_return_if_fail (GDK_IS_WINDOW_IMPL_WAYLAND (object));

  impl = GDK_WINDOW_IMPL_WAYLAND (object);

  if (impl->cursor)
    g_object_unref (impl->cursor);

  g_free (impl->title);

  G_OBJECT_CLASS (_gdk_window_impl_wayland_parent_class)->finalize (object);
}

static void
gdk_wayland_window_configure (GdkWindow *window,
                              int        width,
                              int        height,
                              int        edges)
{
  GdkDisplay *display;
  GdkEvent *event;

  display = gdk_window_get_display (window);

  /* TODO: Only generate a configure event if width/height/scale have actually
   * changed?
   */
  event = gdk_event_new (GDK_CONFIGURE);
  event->configure.window = window;
  event->configure.send_event = FALSE;
  event->configure.width = width;
  event->configure.height = height;

  gdk_wayland_window_update_size (window, width, height, edges);
  _gdk_window_update_size (window);

  g_object_ref(window);

  _gdk_wayland_display_deliver_event (display, event);
}

static void
gdk_wayland_window_set_user_time (GdkWindow *window,
                                  guint32    user_time)
{
}

static void
gdk_wayland_window_map (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWindowImplWayland *parent;
  GdkWindow *transient_for;

  if (!impl->mapped && !impl->use_custom_surface)
    {
      /* Popup menus can appear without a transient parent, which means they
       * cannot be positioned properly on Wayland. This attempts to guess the
       * surface they should be positioned with by finding the surface beneath
       * the device that created the grab for the popup window
       */
      if (!impl->transient_for && impl->hint == GDK_WINDOW_TYPE_HINT_POPUP_MENU)
        {
          transient_for = gdk_device_get_window_at_position (impl->grab_device, NULL, NULL);
          transient_for = gdk_window_get_toplevel (transient_for);

          /* start the popup at the position of the device that holds the grab */
          gdk_window_get_device_position (transient_for,
                                          impl->grab_device,
                                          &window->x, &window->y, NULL);
        }
      else
        transient_for = impl->transient_for;

      if (transient_for)
        {
          struct wl_seat *grab_input_seat = NULL;
          GdkWindowImplWayland *tmp_impl;
          GdkWaylandDeviceData *device;

          parent = GDK_WINDOW_IMPL_WAYLAND (transient_for->impl);

          /* Use the device that was used for the grab as the device for
           * the popup window setup - so this relies on GTK+ taking the
           * grab before showing the popup window.
           */
          grab_input_seat = impl->grab_input_seat;

          tmp_impl = parent;
          while (!grab_input_seat)
            {
              grab_input_seat = tmp_impl->grab_input_seat;

              if (tmp_impl->transient_for)
                tmp_impl = GDK_WINDOW_IMPL_WAYLAND (tmp_impl->transient_for->impl);
              else
                break;
            }

          if (grab_input_seat &&
              (impl->hint == GDK_WINDOW_TYPE_HINT_POPUP_MENU ||
               impl->hint == GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU ||
               impl->hint == GDK_WINDOW_TYPE_HINT_COMBO))
            {
              device = wl_seat_get_user_data (grab_input_seat);
              wl_shell_surface_set_popup (impl->shell_surface,
                                          grab_input_seat,
                                          _gdk_wayland_device_get_button_press_serial (device),
                                          parent->surface,
                                          window->x, window->y, 0);
            }
          else
            {
              guint32 flags = 0;

              if (impl->hint == GDK_WINDOW_TYPE_HINT_TOOLTIP)
                flags = WL_SHELL_SURFACE_TRANSIENT_INACTIVE;

              wl_shell_surface_set_transient (impl->shell_surface, parent->surface,
                                              window->x, window->y, flags);
            }
        }
      else
        {
          wl_shell_surface_set_toplevel (impl->shell_surface);
        }
      impl->mapped = TRUE;
    }
}

static void
surface_enter (void              *data,
               struct wl_surface *wl_surface,
               struct wl_output  *output)
{
  GdkWindow *window = GDK_WINDOW (data);
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  impl->outputs = g_slist_prepend (impl->outputs, output);

  window_update_scale (window);
}

static void
surface_leave (void              *data,
               struct wl_surface *wl_surface,
               struct wl_output  *output)
{
  GdkWindow *window = GDK_WINDOW (data);
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  impl->outputs = g_slist_remove (impl->outputs, output);

  window_update_scale (window);
}


static void
shell_surface_handle_configure (void                    *data,
                                struct wl_shell_surface *shell_surface,
                                uint32_t                 edges,
                                int32_t                  width,
                                int32_t                  height)
{
  GdkWindow *window = GDK_WINDOW (data);
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  gdk_window_constrain_size (&impl->geometry_hints,
                             impl->geometry_mask,
                             width,
                             height,
                             &width,
                             &height);

  gdk_wayland_window_configure (window, width, height, edges);
}

static void
shell_surface_popup_done (void                    *data,
                          struct wl_shell_surface *shell_surface)
{
  GdkWindow *window = GDK_WINDOW (data);

  /* When the popup is complete hide the window - this really relies on the
   * fix in https://bugzilla.gnome.org/show_bug.cgi?id=670881 to work
   * effectively.
   */
  gdk_window_hide (window);
}

static void
shell_surface_ping (void                    *data,
                    struct wl_shell_surface *shell_surface,
                    uint32_t                 serial)
{

  GdkWindow *window = GDK_WINDOW (data);
  GdkWaylandDisplay *wayland_display =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));

  _gdk_wayland_display_update_serial (wayland_display, serial);

  wl_shell_surface_pong(shell_surface, serial);
}

static const struct wl_surface_listener surface_listener = {
  surface_enter,
  surface_leave
};

static const struct wl_shell_surface_listener shell_surface_listener = {
  shell_surface_ping,
  shell_surface_handle_configure,
  shell_surface_popup_done
};

static void
gdk_wayland_window_create_surface (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));

  impl->surface = wl_compositor_create_surface (display_wayland->compositor);

  wl_surface_set_user_data (impl->surface, window);
  wl_surface_add_listener (impl->surface,
                           &surface_listener, window);

  if (display_wayland->gtk_shell)
    impl->gtk_surface = gtk_shell_get_gtk_surface (display_wayland->gtk_shell,
                                                   impl->surface);
}

static void
gdk_wayland_window_show (GdkWindow *window,
                         gboolean   already_mapped)
{
  GdkDisplay *display;
  GdkWaylandDisplay *display_wayland;
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkEvent *event;

  display = gdk_window_get_display (window);
  display_wayland = GDK_WAYLAND_DISPLAY (display);

  if (impl->user_time != 0 &&
      display_wayland->user_time != 0 &&
      XSERVER_TIME_IS_LATER (display_wayland->user_time, impl->user_time))
    gdk_wayland_window_set_user_time (window, impl->user_time);

  if (!impl->surface)
    gdk_wayland_window_create_surface (window);

  if (!impl->shell_surface &&
      !impl->use_custom_surface &&
      display_wayland->shell)
    {
      impl->shell_surface = wl_shell_get_shell_surface (display_wayland->shell,
                                                        impl->surface);
      wl_shell_surface_add_listener (impl->shell_surface,
                                     &shell_surface_listener, window);
    }

  if (impl->shell_surface)
    {
      if (impl->title)
        wl_shell_surface_set_title (impl->shell_surface, impl->title);

      wl_shell_surface_set_class (impl->shell_surface, gdk_get_program_class ());
    }

  gdk_window_set_type_hint (window, impl->hint);

  _gdk_make_event (window, GDK_MAP, NULL, FALSE);
  event = _gdk_make_event (window, GDK_VISIBILITY_NOTIFY, NULL, FALSE);
  event->visibility.state = GDK_VISIBILITY_UNOBSCURED;

  if (impl->cairo_surface)
    gdk_wayland_window_attach_image (window);
}

static void
gdk_wayland_window_hide_surface (GdkWindow *window,
                                 gboolean   is_destroy)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (impl->surface)
    {
      if (impl->shell_surface)
        wl_shell_surface_destroy (impl->shell_surface);
      if (impl->use_custom_surface && !is_destroy)
        {
          wl_surface_attach (impl->surface, NULL, 0, 0);
          wl_surface_commit (impl->surface);
        }
      else if (impl->surface)
        {
          if (impl->gtk_surface)
            gtk_surface_destroy (impl->gtk_surface);
          impl->gtk_surface = NULL;

          wl_surface_destroy (impl->surface);
          impl->surface = NULL;

          g_slist_free (impl->outputs);
          impl->outputs = NULL;
        }
      impl->shell_surface = NULL;
      cairo_surface_destroy (impl->server_surface);
      impl->server_surface = NULL;
      impl->mapped = FALSE;
    }
}

static void
gdk_wayland_window_hide (GdkWindow *window)
{
  gdk_wayland_window_hide_surface (window, FALSE);
  _gdk_window_clear_update_area (window);
}

static void
gdk_window_wayland_withdraw (GdkWindow *window)
{
  if (!window->destroyed)
    {
      if (GDK_WINDOW_IS_MAPPED (window))
        gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_WITHDRAWN);

      g_assert (!GDK_WINDOW_IS_MAPPED (window));

      gdk_wayland_window_hide_surface (window, FALSE);
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
                                  GList     *native_siblings)
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
  if (with_move)
    {
      window->x = x;
      window->y = y;
    }

  /* If this function is called with width and height = -1 then that means
   * just move the window - don't update its size
   */
  if (width > 0 && height > 0)
    gdk_wayland_window_configure (window, width, height, 0);
}

static void
gdk_window_wayland_set_background (GdkWindow       *window,
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

void
_gdk_wayland_window_offset (GdkWindow *window,
                            gint      *x_out,
                            gint      *y_out)
{
  GdkWindowImplWayland *impl, *parent_impl;
  GdkWindow *parent_window;
  gint x_offset = 0, y_offset = 0;

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  parent_window = impl->transient_for;
  while (parent_window)
    {
      parent_impl = GDK_WINDOW_IMPL_WAYLAND (parent_window->impl);

      x_offset += window->x;
      y_offset += window->y;

      parent_window = parent_impl->transient_for;
    }

  *x_out = x_offset;
  *y_out = y_offset;
}

static gint
gdk_window_wayland_get_root_coords (GdkWindow *window,
                                    gint       x,
                                    gint       y,
                                    gint      *root_x,
                                    gint      *root_y)
{
  gint x_offset, y_offset;

  _gdk_wayland_window_offset (window, &x_offset, &y_offset);

  if (root_x)
    *root_x = x_offset + x;

  if (root_y)
    *root_y = y_offset + y;

  return 1;
}

static gboolean
gdk_window_wayland_get_device_state (GdkWindow       *window,
                                     GdkDevice       *device,
                                     gdouble         *x,
                                     gdouble         *y,
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
gdk_window_wayland_shape_combine_region (GdkWindow            *window,
                                         const cairo_region_t *shape_region,
                                         gint                  offset_x,
                                         gint                  offset_y)
{
}

static void
gdk_window_wayland_input_shape_combine_region (GdkWindow            *window,
                                               const cairo_region_t *shape_region,
                                               gint                  offset_x,
                                               gint                  offset_y)
{
}

static gboolean
gdk_window_wayland_set_static_gravities (GdkWindow *window,
                                         gboolean   use_static)
{
  return TRUE;
}

static gboolean
gdk_wayland_window_queue_antiexpose (GdkWindow      *window,
                                     cairo_region_t *area)
{
  return FALSE;
}

static void
gdk_wayland_window_destroy (GdkWindow *window,
                            gboolean   recursing,
                            gboolean   foreign_destroy)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  g_return_if_fail (GDK_IS_WINDOW (window));

  /* We don't have nested windows */
  g_return_if_fail (!recursing);
  /* Wayland windows can't be externally destroyed; we may possibly
   * eventually want to use this path at display close-down
   */
  g_return_if_fail (!foreign_destroy);

  gdk_wayland_window_hide_surface (window, TRUE);

  if (impl->cairo_surface)
    {
      cairo_surface_finish (impl->cairo_surface);
      cairo_surface_set_user_data (impl->cairo_surface, &gdk_wayland_cairo_key,
                                   NULL, NULL);
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
gdk_wayland_window_set_type_hint (GdkWindow         *window,
                                  GdkWindowTypeHint  hint)
{
  GdkWindowImplWayland *impl;

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl->hint = hint;

  switch (hint)
    {
    case GDK_WINDOW_TYPE_HINT_MENU:
    case GDK_WINDOW_TYPE_HINT_TOOLBAR:
    case GDK_WINDOW_TYPE_HINT_UTILITY:
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
    case GDK_WINDOW_TYPE_HINT_DIALOG:
    case GDK_WINDOW_TYPE_HINT_NORMAL:
    case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
      if (impl->shell_surface)
        wl_shell_surface_set_toplevel (impl->shell_surface);
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
  GdkWindowImplWayland *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  impl->geometry_hints = *geometry;
  impl->geometry_mask = geom_mask;

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
  GdkWindowImplWayland *impl;
  g_return_if_fail (title != NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  g_free (impl->title);
  impl->title = g_strdup (title);
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
                                          gboolean   override_redirect)
{
}

static void
gdk_wayland_window_set_accept_focus (GdkWindow *window,
                                     gboolean   accept_focus)
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
    gdk_window_show (window);
  else
    /* Flip our client side flag, the real work happens on map. */
    gdk_synthesize_window_state (window, GDK_WINDOW_STATE_ICONIFIED, 0);
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
  GdkWindowImplWayland *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      if (impl->surface)
        {
          impl->saved_maximized.width = gdk_window_get_width (window);
          impl->saved_maximized.height = gdk_window_get_height (window);

          if (impl->shell_surface)
            wl_shell_surface_set_maximized (impl->shell_surface, NULL);
        }

      gdk_synthesize_window_state (window,
                                   0,
                                   GDK_WINDOW_STATE_MAXIMIZED);
    }
}

static void
gdk_wayland_window_unmaximize (GdkWindow *window)
{
  GdkWindowImplWayland *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      if (impl->surface)
        {
          if (impl->shell_surface)
            wl_shell_surface_set_toplevel (impl->shell_surface);
        }

      gdk_synthesize_window_state (window, GDK_WINDOW_STATE_MAXIMIZED, 0);

      gdk_wayland_window_configure (window,
                                    impl->saved_maximized.width,
                                    impl->saved_maximized.height,
                                    0);
    }
}

static void
gdk_wayland_window_fullscreen (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (impl->fullscreen)
    return;

  if (!impl->shell_surface)
    return;

  impl->saved_fullscreen.width = gdk_window_get_width (window);
  impl->saved_fullscreen.height = gdk_window_get_height (window);

  wl_shell_surface_set_fullscreen (impl->shell_surface,
                                   WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
                                   0,
                                   NULL);

  gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_FULLSCREEN);

  impl->fullscreen = TRUE;
}

static void
gdk_wayland_window_unfullscreen (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (!impl->fullscreen)
    return;

  if (!impl->shell_surface)
    return;

  wl_shell_surface_set_toplevel (impl->shell_surface);
  gdk_synthesize_window_state (window, GDK_WINDOW_STATE_FULLSCREEN, 0);
  gdk_wayland_window_configure (window,
                                impl->saved_fullscreen.width,
                                impl->saved_fullscreen.height,
                                0);

  impl->fullscreen = FALSE;
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
gdk_wayland_window_set_decorations (GdkWindow       *window,
                                    GdkWMDecoration  decorations)
{
}

static gboolean
gdk_wayland_window_get_decorations (GdkWindow       *window,
                                    GdkWMDecoration *decorations)
{
  return FALSE;
}

static void
gdk_wayland_window_set_functions (GdkWindow     *window,
                                  GdkWMFunction  functions)
{
}

static void
gdk_wayland_window_begin_resize_drag (GdkWindow     *window,
                                      GdkWindowEdge  edge,
                                      GdkDevice     *device,
                                      gint           button,
                                      gint           root_x,
                                      gint           root_y,
                                      guint32        timestamp)
{
  GdkWindowImplWayland *impl;
  GdkWaylandDisplay *wayland_display =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));

  uint32_t grab_type;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  switch (edge)
    {
    case GDK_WINDOW_EDGE_NORTH_WEST:
      grab_type = WL_SHELL_SURFACE_RESIZE_TOP_LEFT;
      break;

    case GDK_WINDOW_EDGE_NORTH:
      grab_type = WL_SHELL_SURFACE_RESIZE_TOP;
      break;

    case GDK_WINDOW_EDGE_NORTH_EAST:
      grab_type = WL_SHELL_SURFACE_RESIZE_RIGHT;
      break;

    case GDK_WINDOW_EDGE_WEST:
      grab_type = WL_SHELL_SURFACE_RESIZE_LEFT;
      break;

    case GDK_WINDOW_EDGE_EAST:
      grab_type = WL_SHELL_SURFACE_RESIZE_RIGHT;
      break;

    case GDK_WINDOW_EDGE_SOUTH_WEST:
      grab_type = WL_SHELL_SURFACE_RESIZE_BOTTOM_LEFT;
      break;

    case GDK_WINDOW_EDGE_SOUTH:
      grab_type = WL_SHELL_SURFACE_RESIZE_BOTTOM;
      break;

    case GDK_WINDOW_EDGE_SOUTH_EAST:
      grab_type = WL_SHELL_SURFACE_RESIZE_BOTTOM_RIGHT;
      break;

    default:
      g_warning ("gdk_window_begin_resize_drag: bad resize edge %d!", edge);
      return;
    }

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (!impl->shell_surface)
    return;

  wl_shell_surface_resize (impl->shell_surface,
                           gdk_wayland_device_get_wl_seat (device),
                           _gdk_wayland_display_get_serial (wayland_display),
                           grab_type);

  /* This is needed since Wayland will absorb all the pointer events after the
   * above function - FIXME: Is this always safe..?
   */
  gdk_device_ungrab (device, timestamp);
}

static void
gdk_wayland_window_begin_move_drag (GdkWindow *window,
                                    GdkDevice *device,
                                    gint       button,
                                    gint       root_x,
                                    gint       root_y,
                                    guint32    timestamp)
{
  GdkWindowImplWayland *impl;
  GdkWaylandDisplay *wayland_display =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (!impl->shell_surface)
    return;

  wl_shell_surface_move (impl->shell_surface,
                         gdk_wayland_device_get_wl_seat (device),
                         _gdk_wayland_display_get_serial (wayland_display));

  /* This is needed since Wayland will absorb all the pointer events after the
   * above function - FIXME: Is this always safe..?
   */
  gdk_device_ungrab (device, timestamp);
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
      if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN)
        g_warning ("GdkWindow %p unexpectedly destroyed", window);

      _gdk_window_destroy (window, TRUE);
    }

  g_object_unref (window);
}

static void
gdk_wayland_window_process_updates_recurse (GdkWindow      *window,
                                            cairo_region_t *region)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  cairo_rectangle_int_t rect;
  int i, n;

  gdk_wayland_window_map (window);

  gdk_wayland_window_ensure_cairo_surface (window);
  gdk_wayland_window_attach_image (window);

  _gdk_window_process_updates_recurse (window, region);

  n = cairo_region_num_rectangles (region);
  for (i = 0; i < n; i++)
    {
      cairo_region_get_rectangle (region, i, &rect);
      wl_surface_damage (impl->surface,
                         rect.x, rect.y, rect.width, rect.height);
      impl->pending_commit = TRUE;
    }
}

static void
gdk_wayland_window_sync_rendering (GdkWindow *window)
{
}

static gboolean
gdk_wayland_window_simulate_key (GdkWindow       *window,
                                 gint             x,
                                 gint             y,
                                 guint            keyval,
                                 GdkModifierType  modifiers,
                                 GdkEventType     key_pressrelease)
{
  return FALSE;
}

static gboolean
gdk_wayland_window_simulate_button (GdkWindow       *window,
                                    gint             x,
                                    gint             y,
                                    guint            button,
                                    GdkModifierType  modifiers,
                                    GdkEventType     button_pressrelease)
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

static gint
gdk_wayland_window_get_scale_factor (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (GDK_WINDOW_DESTROYED (window))
    return 1;

  return impl->scale;
}

static struct wl_region *
wl_region_from_cairo_region (GdkWaylandDisplay *display,
                             cairo_region_t    *region)
{
  struct wl_region *wl_region;
  int i, n_rects;

  wl_region = wl_compositor_create_region (display->compositor);
  if (wl_region == NULL)
    return NULL;

  n_rects = cairo_region_num_rectangles (region);
  for (i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t rect;
      cairo_region_get_rectangle (region, i, &rect);
      wl_region_add (wl_region, rect.x, rect.y, rect.width, rect.height);
    }

  return wl_region;
}

static void
gdk_wayland_window_set_opaque_region (GdkWindow      *window,
                                      cairo_region_t *region)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  struct wl_region *wl_region = NULL;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (!impl->surface)
    gdk_wayland_window_create_surface (window);

  if (region != NULL)
    wl_region = wl_region_from_cairo_region (GDK_WAYLAND_DISPLAY (gdk_window_get_display (window)), region);

  wl_surface_set_opaque_region (impl->surface, wl_region);

  if (wl_region != NULL)
    wl_region_destroy (wl_region);
}


static void
_gdk_window_impl_wayland_class_init (GdkWindowImplWaylandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkWindowImplClass *impl_class = GDK_WINDOW_IMPL_CLASS (klass);

  object_class->finalize = gdk_window_impl_wayland_finalize;

  impl_class->ref_cairo_surface = gdk_wayland_window_ref_cairo_surface;
  impl_class->create_similar_image_surface = gdk_wayland_window_create_similar_image_surface;
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
  impl_class->destroy = gdk_wayland_window_destroy;
  impl_class->destroy_foreign = gdk_window_wayland_destroy_foreign;
  impl_class->resize_cairo_surface = gdk_window_wayland_resize_cairo_surface;
  impl_class->get_shape = gdk_wayland_window_get_shape;
  impl_class->get_input_shape = gdk_wayland_window_get_input_shape;
  impl_class->begin_paint_region = gdk_window_impl_wayland_begin_paint_region;
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
  impl_class->get_scale_factor = gdk_wayland_window_get_scale_factor;
  impl_class->set_opaque_region = gdk_wayland_window_set_opaque_region;
}


void
_gdk_wayland_window_set_device_grabbed (GdkWindow      *window,
                                        GdkDevice      *device,
                                        struct wl_seat *seat,
                                        guint32         time_)
{
  GdkWindowImplWayland *impl;

  g_return_if_fail (window != NULL);

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  impl->grab_device = device;
  impl->grab_input_seat = seat;
  impl->grab_time = time_;
}

/**
 * gdk_wayland_window_get_wl_surface
 * @window: (type GdkWaylandWindow): a #GdkWindow
 *
 * Returns the Wayland surface of a #GdkWindow
 *
 * Returns: (transfer none): a Wayland wl_surface
 *
 * Since: 3.8
 */
struct wl_surface *
gdk_wayland_window_get_wl_surface (GdkWindow *window)
{
  GdkWindowImplWayland *impl;

  g_return_val_if_fail (GDK_IS_WAYLAND_WINDOW (window), NULL);

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  return impl->surface;
}

/**
 * gdk_wayland_window_get_wl_shell_surface
 * @window: (type GdkWaylandWindow): a #GdkWindow
 *
 * Returns the Wayland shell surface of a #GdkWindow
 *
 * Returns: (transfer none): a Wayland wl_shell_surface
 *
 * Since: 3.8
 */
struct wl_shell_surface *
gdk_wayland_window_get_wl_shell_surface (GdkWindow *window)
{
  GdkWindowImplWayland *impl;

  g_return_val_if_fail (GDK_IS_WAYLAND_WINDOW (window), NULL);

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  return impl->shell_surface;
}

/**
 * gdk_wayland_window_set_use_custom_surface:
 * @window: (type GdkWaylandWindow): a #GdkWindow
 *
 * Marks a #GdkWindow as a custom Wayland surface. The application is
 * expected to register the surface as some type of surface using
 * some Wayland interface.
 *
 * A good example would be writing a panel or on-screen-keyboard as an
 * out-of-process helper - as opposed to having those in the compositor
 * process. In this case the underlying surface isn't a wl_shell
 * surface and the panel or OSK client need to identify the wl_surface
 * as a panel or OSK to the compositor. The assumption is that the
 * compositor will expose a private interface to the special client
 * that lets the client identify the wl_surface as a panel or such.
 *
 * This function should be called before a #GdkWindow is shown. This is
 * best done by connecting to the #GtkWidget::realized signal:
 *
 * <informalexample>
 * <programlisting>
 *   static void
 *   widget_realize_cb (GtkWidget *widget)
 *   {
 *     GdkWindow *window;
 *     struct wl_surface *surface;
 *     struct input_panel_surface *ip_surface;
 *
 *     window = gtk_widget_get_window (widget);
 *     gdk_wayland_window_set_custom_surface (window);
 *
 *     surface = gdk_wayland_window_get_wl_surface (window);
 *     ip_surface = input_panel_get_input_panel_surface (input_panel, surface);
 *     input_panel_surface_set_panel (ip_surface);
 *   }
 *
 *   static void
 *   setup_window (GtkWindow *window)
 *   {
 *     g_signal_connect (window, "realize", G_CALLBACK (widget_realize_cb), NULL);
 *   }
 * </programlisting>
 * </informalexample>
 *
 * Since: 3.10
 */
void
gdk_wayland_window_set_use_custom_surface (GdkWindow *window)
{
  GdkWindowImplWayland *impl;

  g_return_if_fail (GDK_IS_WAYLAND_WINDOW (window));

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (!impl->surface)
    gdk_wayland_window_create_surface (window);

  impl->use_custom_surface = TRUE;
}

void
gdk_wayland_window_set_dbus_properties_libgtk_only (GdkWindow  *window,
                                                    const char *application_id,
                                                    const char *app_menu_path,
                                                    const char *menubar_path,
                                                    const char *window_object_path,
                                                    const char *application_object_path,
                                                    const char *unique_bus_name)
{
  GdkWindowImplWayland *impl;

  g_return_if_fail (GDK_IS_WAYLAND_WINDOW (window));

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (!impl->surface)
    gdk_wayland_window_create_surface (window);

  if (impl->gtk_surface == NULL)
    return;

  gtk_surface_set_dbus_properties (impl->gtk_surface,
                                   application_id,
                                   app_menu_path,
                                   menubar_path,
                                   window_object_path,
                                   application_object_path,
                                   unique_bus_name);
}
