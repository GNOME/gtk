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
#include "gdkglcontext-wayland.h"
#include "gdkframeclockprivate.h"
#include "gdkprivate-wayland.h"
#include "gdkprofilerprivate.h"
#include "gdkinternals.h"
#include "gdkdeviceprivate.h"
#include "gdkprivate-wayland.h"
#include "xdg-shell-unstable-v6-client-protocol.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

enum {
  COMMITTED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

#define WINDOW_IS_TOPLEVEL_OR_FOREIGN(window) \
  (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&   \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_OFFSCREEN)

#define WINDOW_IS_TOPLEVEL(window)                   \
  (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&   \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN && \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_OFFSCREEN)

#define MAX_WL_BUFFER_SIZE (4083) /* 4096 minus header, string argument length and NUL byte */

typedef struct _GdkWaylandWindow GdkWaylandWindow;
typedef struct _GdkWaylandWindowClass GdkWaylandWindowClass;

struct _GdkWaylandWindow
{
  GdkWindow parent;
};

struct _GdkWaylandWindowClass
{
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

typedef enum _PositionMethod
{
  POSITION_METHOD_NONE,
  POSITION_METHOD_MOVE_RESIZE,
  POSITION_METHOD_MOVE_TO_RECT
} PositionMethod;

typedef struct _ExportedClosure
{
  GdkWaylandWindowExported callback;
  gpointer user_data;
  GDestroyNotify destroy_func;
} ExportedClosure;

struct _GdkWindowImplWayland
{
  GdkWindowImpl parent_instance;

  GdkWindow *wrapper;

  struct {
    /* The wl_outputs that this window currently touches */
    GSList               *outputs;

    struct wl_surface    *wl_surface;

    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct xdg_popup *xdg_popup;

    /* Legacy xdg-shell unstable v6 fallback support */
    struct zxdg_surface_v6 *zxdg_surface_v6;
    struct zxdg_toplevel_v6 *zxdg_toplevel_v6;
    struct zxdg_popup_v6 *zxdg_popup_v6;

    struct gtk_surface1  *gtk_surface;
    struct wl_subsurface *wl_subsurface;
    struct wl_egl_window *egl_window;
    struct wl_egl_window *dummy_egl_window;
    struct zxdg_exported_v1 *xdg_exported_v1;
    struct zxdg_exported_v2 *xdg_exported_v2;
    struct org_kde_kwin_server_decoration *server_decoration;
  } display_server;

  EGLSurface egl_surface;
  EGLSurface dummy_egl_surface;

  unsigned int initial_configure_received : 1;
  unsigned int configuring_popup : 1;
  unsigned int mapped : 1;
  unsigned int use_custom_surface : 1;
  unsigned int pending_buffer_attached : 1;
  unsigned int pending_commit : 1;
  unsigned int awaiting_frame : 1;
  unsigned int using_csd : 1;
  unsigned int suspended : 1;
  GdkWindowTypeHint hint;
  GdkWindow *transient_for;
  GdkWindow *popup_parent;
  PositionMethod position_method;

  cairo_surface_t *staging_cairo_surface;
  cairo_surface_t *committed_cairo_surface;
  cairo_surface_t *backfill_cairo_surface;

  int pending_buffer_offset_x;
  int pending_buffer_offset_y;

  int subsurface_x;
  int subsurface_y;

  gchar *title;

  struct {
    gboolean was_set;

    gchar *application_id;
    gchar *app_menu_path;
    gchar *menubar_path;
    gchar *window_object_path;
    gchar *application_object_path;
    gchar *unique_bus_name;
  } application;

  GdkGeometry geometry_hints;
  GdkWindowHints geometry_mask;

  GdkSeat *grab_input_seat;

  gint64 pending_frame_counter;
  guint32 scale;

  int margin_left;
  int margin_right;
  int margin_top;
  int margin_bottom;
  gboolean margin_dirty;

  int initial_fullscreen_monitor;

  cairo_region_t *opaque_region;
  gboolean opaque_region_dirty;

  cairo_region_t *input_region;
  gboolean input_region_dirty;

  cairo_region_t *staged_updates_region;

  int saved_width;
  int saved_height;
  gboolean saved_size_changed;

  int unconfigured_width;
  int unconfigured_height;

  int fixed_size_width;
  int fixed_size_height;

  gulong parent_surface_committed_handler;

  struct {
    GdkRectangle rect;
    GdkGravity rect_anchor;
    GdkGravity window_anchor;
    GdkAnchorHints anchor_hints;
    gint rect_anchor_dx;
    gint rect_anchor_dy;
  } pending_move_to_rect;

  struct {
    int width;
    int height;
    GdkWindowState state;
    gboolean suspended;
  } pending;

  struct {
    char *handle;
    int export_count;
    GList *closures;
    guint idle_source_id;
  } exported;

  struct zxdg_imported_v1 *imported_v1_transient_for;
  struct zxdg_imported_v2 *imported_v2_transient_for;
  GHashTable *shortcuts_inhibitors;

  struct wl_callback *surface_callback;
  GHashTable *frame_callback_surfaces;
};

struct _GdkWindowImplWaylandClass
{
  GdkWindowImplClass parent_class;
};

static void gdk_wayland_window_maybe_configure (GdkWindow *window,
                                                int        width,
                                                int        height,
                                                int        scale);

static void maybe_set_gtk_surface_dbus_properties (GdkWindow *window);
static void maybe_set_gtk_surface_modal (GdkWindow *window);

static void gdk_window_request_transient_parent_commit (GdkWindow *window);

static void gdk_wayland_window_sync_margin (GdkWindow *window);
static void gdk_wayland_window_sync_input_region (GdkWindow *window);
static void gdk_wayland_window_sync_opaque_region (GdkWindow *window);

static void unset_transient_for_exported (GdkWindow *window);

static void calculate_moved_to_rect_result (GdkWindow    *window,
                                            int           x,
                                            int           y,
                                            int           width,
                                            int           height,
                                            GdkRectangle *flipped_rect,
                                            GdkRectangle *final_rect,
                                            gboolean     *flipped_x,
                                            gboolean     *flipped_y);

static gboolean gdk_wayland_window_is_exported (GdkWindow *window);
static void gdk_wayland_window_unexport (GdkWindow *window);
static void gdk_wayland_window_announce_decoration_mode (GdkWindow *window);

static gboolean should_map_as_subsurface (GdkWindow *window);
static gboolean should_map_as_popup (GdkWindow *window);

GType _gdk_window_impl_wayland_get_type (void);

G_DEFINE_TYPE (GdkWindowImplWayland, _gdk_window_impl_wayland, GDK_TYPE_WINDOW_IMPL)

static void
_gdk_window_impl_wayland_init (GdkWindowImplWayland *impl)
{
  impl->scale = 1;
  impl->initial_fullscreen_monitor = -1;
  impl->saved_width = -1;
  impl->saved_height = -1;
  impl->suspended = FALSE;
}

static void
_gdk_wayland_screen_add_orphan_dialog (GdkWindow *window)
{
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));

  if (!g_list_find (display_wayland->orphan_dialogs, window))
    display_wayland->orphan_dialogs =
      g_list_prepend (display_wayland->orphan_dialogs, window);
}

static void
drop_cairo_surfaces (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  g_clear_pointer (&impl->staging_cairo_surface, cairo_surface_destroy);
  g_clear_pointer (&impl->backfill_cairo_surface, cairo_surface_destroy);

  /* We nullify this so if a buffer release comes in later, we won't
   * try to reuse that buffer since it's no longer suitable
   */
  impl->committed_cairo_surface = NULL;
}

static int
calculate_width_without_margin (GdkWindow *window,
                                int        width)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  return width - (impl->margin_left + impl->margin_right);
}

static int
calculate_height_without_margin (GdkWindow *window,
                                 int        height)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  return height - (impl->margin_top + impl->margin_bottom);
}

static int
calculate_width_with_margin (GdkWindow *window,
                             int        width)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  return width + impl->margin_left + impl->margin_right;
}

static int
calculate_height_with_margin (GdkWindow *window,
                              int        height)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  return height + impl->margin_top + impl->margin_bottom;
}

static void
_gdk_wayland_window_save_size (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (window->state & (GDK_WINDOW_STATE_FULLSCREEN |
                       GDK_WINDOW_STATE_MAXIMIZED |
                       GDK_WINDOW_STATE_TILED))
    return;

  impl->saved_width = calculate_width_without_margin (window, window->width);
  impl->saved_height = calculate_height_without_margin (window, window->height);
}

static void
_gdk_wayland_window_clear_saved_size (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (window->state & (GDK_WINDOW_STATE_FULLSCREEN |
                       GDK_WINDOW_STATE_MAXIMIZED |
                       GDK_WINDOW_STATE_TILED))
    return;

  impl->saved_width = -1;
  impl->saved_height = -1;
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
                                int        scale)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkRectangle area;
  cairo_region_t *region;

  if ((window->width == width) &&
      (window->height == height) &&
      (impl->scale == scale))
    return;

  drop_cairo_surfaces (window);

  window->width = width;
  window->height = height;
  impl->scale = scale;

  if (impl->display_server.egl_window)
    wl_egl_window_resize (impl->display_server.egl_window, width * scale, height * scale, 0, 0);
  if (impl->display_server.wl_surface)
    wl_surface_set_buffer_scale (impl->display_server.wl_surface, scale);

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
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_screen_get_display (screen));
  GdkWindow *window;
  GdkWindowImplWayland *impl;

  window = _gdk_display_create_window (GDK_DISPLAY (display_wayland));
  window->impl = g_object_new (GDK_TYPE_WINDOW_IMPL_WAYLAND, NULL);
  window->impl_window = window;
  window->visual = gdk_screen_get_system_visual (screen);

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  impl->wrapper = GDK_WINDOW (window);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (display_wayland->compositor_version >= WL_SURFACE_HAS_BUFFER_SCALE &&
      gdk_screen_get_n_monitors (screen) > 0)
    impl->scale = gdk_screen_get_monitor_scale_factor (screen, 0);
G_GNUC_END_IGNORE_DEPRECATIONS

  impl->using_csd = TRUE;

  /* logical 1x1 fake buffer */
  impl->staging_cairo_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                            impl->scale,
                                                            impl->scale);

  cairo_surface_set_device_scale (impl->staging_cairo_surface, impl->scale, impl->scale);

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
read_back_cairo_surface (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  cairo_t *cr;
  cairo_region_t *paint_region = NULL;

  if (!impl->backfill_cairo_surface)
    goto out;

  paint_region = cairo_region_copy (window->clip_region);
  cairo_region_subtract (paint_region, impl->staged_updates_region);

  if (cairo_region_is_empty (paint_region))
    goto out;

  cr = cairo_create (impl->staging_cairo_surface);
  cairo_set_source_surface (cr, impl->backfill_cairo_surface, 0, 0);
  gdk_cairo_region (cr, paint_region);
  cairo_clip (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);
  cairo_destroy (cr);
  cairo_surface_flush (impl->staging_cairo_surface);

out:
  g_clear_pointer (&paint_region, cairo_region_destroy);
  g_clear_pointer (&impl->staged_updates_region, cairo_region_destroy);
  g_clear_pointer (&impl->backfill_cairo_surface, cairo_surface_destroy);
}

static void
frame_callback (void               *data,
                struct wl_callback *callback,
                uint32_t            time)
{
  GdkWindow *window = data;
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  GdkFrameClock *clock = gdk_window_get_frame_clock (window);
  GdkFrameTimings *timings;

  if (callback == impl->surface_callback)
    {
      impl->surface_callback = NULL;
    }
  else
    {
      GHashTableIter iter;
      gpointer surface_callback;

      g_hash_table_iter_init (&iter, impl->frame_callback_surfaces);
      while (g_hash_table_iter_next (&iter, NULL, &surface_callback))
        {
          if (callback == surface_callback)
            {
              g_hash_table_iter_replace (&iter, NULL);
              break;
            }
        }
    }
  wl_callback_destroy (callback);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (!impl->awaiting_frame)
    return;

  GDK_NOTE (EVENTS,
            g_message ("frame %p", window));

  impl->awaiting_frame = FALSE;
  _gdk_frame_clock_thaw (clock);

  timings = gdk_frame_clock_get_timings (clock, impl->pending_frame_counter);
  impl->pending_frame_counter = 0;

  if (timings == NULL)
    return;

  timings->refresh_interval = 16667; /* default to 1/60th of a second */
  if (impl->display_server.outputs)
    {
      /* We pick a random output out of the outputs that the window touches
       * The rate here is in milli-hertz */
      int refresh_rate =
        _gdk_wayland_screen_get_output_refresh_rate (display_wayland->screen,
                                                     impl->display_server.outputs->data);
      if (refresh_rate != 0)
        timings->refresh_interval = G_GINT64_CONSTANT(1000000000) / refresh_rate;
    }

  fill_presentation_time_from_frame_time (timings, time);

  timings->complete = TRUE;

#ifdef G_ENABLE_DEBUG
  if ((_gdk_debug_flags & GDK_DEBUG_FRAMES) != 0)
    _gdk_frame_clock_debug_print_timings (clock, timings);

  if (gdk_profiler_is_running ())
    _gdk_frame_clock_add_timings_to_profiler (clock, timings);
#endif
}

static const struct wl_callback_listener frame_listener = {
  frame_callback
};

static void
on_frame_clock_before_paint (GdkFrameClock *clock,
                             GdkWindow     *window)
{
  GdkFrameTimings *timings = gdk_frame_clock_get_current_timings (clock);
  gint64 presentation_time;
  gint64 refresh_interval;

  if (window->update_freeze_count > 0)
    return;

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

static void
on_frame_clock_after_paint (GdkFrameClock *clock,
                            GdkWindow     *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  struct wl_callback *callback;
  GHashTableIter iter;
  gpointer surface, surface_callback;

  if (!impl->pending_commit)
    return;

  if (window->update_freeze_count > 0)
    return;

  _gdk_frame_clock_freeze (clock);

  /* Before we commit a new buffer, make sure we've backfilled
   * undrawn parts from any old committed buffer
   */
  if (impl->pending_buffer_attached)
    read_back_cairo_surface (window);

  if (impl->surface_callback == NULL)
    {
      callback = wl_surface_frame (impl->display_server.wl_surface);
      wl_callback_add_listener (callback, &frame_listener, window);
      impl->surface_callback = callback;
    }

  g_hash_table_iter_init (&iter, impl->frame_callback_surfaces);
  while (g_hash_table_iter_next (&iter, &surface, &surface_callback))
    {
      if (surface_callback)
        continue;

      callback = wl_surface_frame (surface);
      wl_callback_add_listener (callback, &frame_listener, window);
      g_hash_table_iter_replace (&iter, callback);
    }

  /* From this commit forward, we can't write to the buffer,
   * it's "live".  In the future, if we need to stage more changes
   * we have to allocate a new staging buffer and draw to it instead.
   *
   * Our one saving grace is if the compositor releases the buffer
   * before we need to stage any changes, then we can take it back and
   * use it again.
   */
  wl_surface_commit (impl->display_server.wl_surface);

  if (impl->pending_buffer_attached)
    impl->committed_cairo_surface = g_steal_pointer (&impl->staging_cairo_surface);

  impl->pending_buffer_attached = FALSE;
  impl->pending_commit = FALSE;
  impl->pending_frame_counter = gdk_frame_clock_get_frame_counter (clock);
  impl->awaiting_frame = TRUE;

  g_signal_emit (impl, signals[COMMITTED], 0);
}

static void
window_update_scale (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  guint32 scale;
  GSList *l;

  if (display_wayland->compositor_version < WL_SURFACE_HAS_BUFFER_SCALE)
    {
      /* We can't set the scale on this surface */
      return;
    }

  if (!impl->display_server.outputs)
    {
      scale = impl->scale;
    }
  else
    {
      scale = 1;
      for (l = impl->display_server.outputs; l != NULL; l = l->next)
        {
          struct wl_output *output = l->data;
          uint32_t output_scale;

          output_scale =
            _gdk_wayland_screen_get_output_scale (display_wayland->screen,
                                                  output);
          scale = MAX (scale, output_scale);
        }
    }

  /* Notify app that scale changed */
  gdk_wayland_window_maybe_configure (window, window->width, window->height, scale);
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
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GdkWindowImplWayland *impl;
  GdkFrameClock *frame_clock;
  const char *title;

  impl = g_object_new (GDK_TYPE_WINDOW_IMPL_WAYLAND, NULL);
  window->impl = GDK_WINDOW_IMPL (impl);
  impl->unconfigured_width = window->width;
  impl->unconfigured_height = window->height;
  impl->wrapper = GDK_WINDOW (window);
  impl->shortcuts_inhibitors = g_hash_table_new (NULL, NULL);
  impl->using_csd = TRUE;
  impl->surface_callback = NULL;
  impl->frame_callback_surfaces = g_hash_table_new (NULL, NULL);

  if (window->width > 65535)
    {
      g_warning ("Native Windows wider than 65535 pixels are not supported");
      window->width = 65535;
    }
  if (window->height > 65535)
    {
      g_warning ("Native Windows taller than 65535 pixels are not supported");
      window->height = 65535;
    }

  g_object_ref (window);

  /* More likely to be right than just assuming 1 */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (display_wayland->compositor_version >= WL_SURFACE_HAS_BUFFER_SCALE &&
      gdk_screen_get_n_monitors (screen) > 0)
    impl->scale = gdk_screen_get_monitor_scale_factor (screen, 0);
G_GNUC_END_IGNORE_DEPRECATIONS

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

  if (GDK_WINDOW_DESTROYED (window))
    return;

  g_assert (_gdk_wayland_is_shm_surface (impl->staging_cairo_surface));

  /* Attach this new buffer to the surface */
  wl_surface_attach (impl->display_server.wl_surface,
                     _gdk_wayland_shm_surface_get_wl_buffer (impl->staging_cairo_surface),
                     impl->pending_buffer_offset_x,
                     impl->pending_buffer_offset_y);
  impl->pending_buffer_offset_x = 0;
  impl->pending_buffer_offset_y = 0;

  /* Only set the buffer scale if supported by the compositor */
  display = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  if (display->compositor_version >= WL_SURFACE_HAS_BUFFER_SCALE)
    wl_surface_set_buffer_scale (impl->display_server.wl_surface, impl->scale);

  impl->pending_buffer_attached = TRUE;
  impl->pending_commit = TRUE;
}

static const cairo_user_data_key_t gdk_wayland_window_cairo_key;

static void
buffer_release_callback (void             *_data,
                         struct wl_buffer *wl_buffer)
{
  cairo_surface_t *cairo_surface = _data;
  GdkWindowImplWayland *impl = cairo_surface_get_user_data (cairo_surface, &gdk_wayland_window_cairo_key);

  g_return_if_fail (GDK_IS_WINDOW_IMPL_WAYLAND (impl));

  /* The released buffer isn't the latest committed one, we have no further
   * use for it, so clean it up.
   */
  if (impl->committed_cairo_surface != cairo_surface)
    {
      /* If this fails, then the surface buffer got reused before it was
       * released from the compositor
       */
      g_warn_if_fail (impl->staging_cairo_surface != cairo_surface);

      cairo_surface_destroy (cairo_surface);
      return;
    }

  if (impl->staged_updates_region != NULL)
    {
      /* If this fails, then we're tracking staged updates on a staging surface
       * that doesn't exist.
       */
      g_warn_if_fail (impl->staging_cairo_surface != NULL);

      /* If we've staged updates into a new buffer before the release for this
       * buffer came in, then we can't reuse this buffer, so unref it. It may still
       * be alive as a readback buffer though (via impl->backfill_cairo_surface).
       *
       * It's possible a staging surface was allocated but no updates were staged.
       * If that happened, clean up that staging surface now, since the old commit
       * buffer is available again, and reusing the old commit buffer for future
       * updates will save having to do a read back later.
       */
      if (!cairo_region_is_empty (impl->staged_updates_region))
        {
          g_clear_pointer (&impl->committed_cairo_surface, cairo_surface_destroy);
          return;
        }
      else
        {
          g_clear_pointer (&impl->staged_updates_region, cairo_region_destroy);
          g_clear_pointer (&impl->staging_cairo_surface, cairo_surface_destroy);
        }
    }

  /* Release came in, we haven't done any interim updates, so we can just use
   * the old committed buffer again.
   */
  g_clear_pointer (&impl->staging_cairo_surface, cairo_surface_destroy);
  impl->staging_cairo_surface = g_steal_pointer (&impl->committed_cairo_surface);
}

static const struct wl_buffer_listener buffer_listener = {
  buffer_release_callback
};

static void
gdk_wayland_window_ensure_cairo_surface (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  /* If we are drawing using OpenGL then we only need a logical 1x1 surface. */
  if (impl->display_server.egl_window)
    {
      if (impl->staging_cairo_surface &&
          _gdk_wayland_is_shm_surface (impl->staging_cairo_surface))
        g_clear_pointer (&impl->staging_cairo_surface, cairo_surface_destroy);

      if (!impl->staging_cairo_surface)
        {
          impl->staging_cairo_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                                    impl->scale,
                                                                    impl->scale);
          cairo_surface_set_device_scale (impl->staging_cairo_surface,
                                          impl->scale, impl->scale);
        }
    }
  else if (!impl->staging_cairo_surface)
    {
      GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_window_get_display (impl->wrapper));
      struct wl_buffer *buffer;

      impl->staging_cairo_surface = _gdk_wayland_display_create_shm_surface (display_wayland,
                                                                             impl->wrapper->width,
                                                                             impl->wrapper->height,
                                                                             impl->scale);
      cairo_surface_set_user_data (impl->staging_cairo_surface,
                                   &gdk_wayland_window_cairo_key,
                                   g_object_ref (impl),
                                   (cairo_destroy_func_t)
                                   g_object_unref);
      buffer = _gdk_wayland_shm_surface_get_wl_buffer (impl->staging_cairo_surface);
      wl_buffer_add_listener (buffer, &buffer_listener, impl->staging_cairo_surface);
    }
}

/* The cairo surface returned here uses a memory segment that's shared
 * with the display server.  This is not a temporary buffer that gets
 * copied to the display server, but the actual buffer the display server
 * will ultimately end up sending to the GPU. At the time this happens
 * impl->committed_cairo_surface gets set to impl->staging_cairo_surface, and
 * impl->staging_cairo_surface gets nullified.
 */
static cairo_surface_t *
gdk_wayland_window_ref_cairo_surface (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  gdk_wayland_window_ensure_cairo_surface (window);

  cairo_surface_reference (impl->staging_cairo_surface);

  return impl->staging_cairo_surface;
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
gdk_window_impl_wayland_begin_paint (GdkWindow *window)
{
  gdk_wayland_window_ensure_cairo_surface (window);

  return FALSE;
}

static void
gdk_window_impl_wayland_end_paint (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  cairo_rectangle_int_t rect;
  int i, n;

  if (!GDK_WINDOW_IS_MAPPED (window))
    return;

  if (impl->staging_cairo_surface &&
      _gdk_wayland_is_shm_surface (impl->staging_cairo_surface) &&
      !window->current_paint.use_gl &&
      !cairo_region_is_empty (window->current_paint.region))
    {
      gdk_wayland_window_attach_image (window);

      /* If there's a committed buffer pending, then track which
       * updates are staged until the next frame, so we can back
       * fill the unstaged parts of the staging buffer with the
       * last frame.
       */
      if (impl->committed_cairo_surface != NULL)
        {
          if (impl->staged_updates_region == NULL)
            {
              impl->staged_updates_region = cairo_region_copy (window->current_paint.region);
              impl->backfill_cairo_surface = cairo_surface_reference (impl->committed_cairo_surface);
            }
          else
            {
              cairo_region_union (impl->staged_updates_region, window->current_paint.region);
            }
        }

      n = cairo_region_num_rectangles (window->current_paint.region);
      for (i = 0; i < n; i++)
        {
          cairo_region_get_rectangle (window->current_paint.region, i, &rect);
          wl_surface_damage (impl->display_server.wl_surface, rect.x, rect.y, rect.width, rect.height);
        }

      impl->pending_commit = TRUE;
    }
  else if (window->current_paint.use_gl &&
           window->current_paint.surface_needs_composite &&
           impl->pending_commit)
    {
      /* Discard issuing pending commit, as when we reach here, it means it'll
       * be done implicitly by eglSwapBuffers(). */
      impl->pending_commit = FALSE;
    }

  gdk_wayland_window_sync_margin (window);
  gdk_wayland_window_sync_opaque_region (window);
  gdk_wayland_window_sync_input_region (window);
}

static gboolean
gdk_window_impl_wayland_beep (GdkWindow *window)
{
  gdk_wayland_display_system_bell (gdk_window_get_display (window),
                                   window);

  return TRUE;
}

static void
gdk_window_impl_wayland_finalize (GObject *object)
{
  GdkWindowImplWayland *impl;

  g_return_if_fail (GDK_IS_WINDOW_IMPL_WAYLAND (object));

  impl = GDK_WINDOW_IMPL_WAYLAND (object);

  if (gdk_wayland_window_is_exported (impl->wrapper))
    gdk_wayland_window_unexport_handle (impl->wrapper);

  g_free (impl->title);

  g_free (impl->application.application_id);
  g_free (impl->application.app_menu_path);
  g_free (impl->application.menubar_path);
  g_free (impl->application.window_object_path);
  g_free (impl->application.application_object_path);
  g_free (impl->application.unique_bus_name);

  g_clear_pointer (&impl->opaque_region, cairo_region_destroy);
  g_clear_pointer (&impl->input_region, cairo_region_destroy);
  g_clear_pointer (&impl->staged_updates_region, cairo_region_destroy);

  g_clear_pointer (&impl->shortcuts_inhibitors, g_hash_table_unref);
  g_clear_pointer (&impl->frame_callback_surfaces, g_hash_table_unref);

  G_OBJECT_CLASS (_gdk_window_impl_wayland_parent_class)->finalize (object);
}

static void
gdk_wayland_window_configure (GdkWindow *window,
                              int        width,
                              int        height,
                              int        scale)
{
  GdkDisplay *display;
  GdkEvent *event;

  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  event = gdk_event_new (GDK_CONFIGURE);
  event->configure.window = g_object_ref (window);
  event->configure.send_event = FALSE;
  event->configure.width = width;
  event->configure.height = height;

  gdk_wayland_window_update_size (window, width, height, scale);
  _gdk_window_update_size (window);

  display = gdk_window_get_display (window);
  _gdk_wayland_display_deliver_event (display, event);
}

static gboolean
is_realized_shell_surface (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  return (impl->display_server.xdg_surface ||
          impl->display_server.zxdg_surface_v6);
}

static gboolean
is_realized_toplevel (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  return (impl->display_server.xdg_toplevel ||
          impl->display_server.zxdg_toplevel_v6);
}

static gboolean
is_realized_popup (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  return (impl->display_server.xdg_popup ||
          impl->display_server.zxdg_popup_v6);
}

static gboolean
should_inhibit_resize (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (impl->display_server.wl_subsurface)
    return FALSE;
  else if (impl->use_custom_surface)
    return FALSE;
  else if (impl->hint == GDK_WINDOW_TYPE_HINT_DND)
    return FALSE;
  else if (is_realized_popup (window))
    return FALSE;
  else if (should_map_as_popup (window))
    return FALSE;
  else if (should_map_as_subsurface (window))
    return FALSE;

  /* This should now either be, or eventually be, a toplevel window,
   * and we should wait for the initial configure to really configure it.
   */
  return !impl->initial_configure_received;
}

static void
gdk_wayland_window_maybe_configure (GdkWindow *window,
                                    int        width,
                                    int        height,
                                    int        scale)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  gboolean is_xdg_popup;
  gboolean is_visible;
  gboolean size_changed;

  impl->unconfigured_width = calculate_width_without_margin (window, width);
  impl->unconfigured_height = calculate_height_without_margin (window, height);

  if (should_inhibit_resize (window))
    return;

  size_changed = (window->width != width || window->height != height);
  if (!size_changed && impl->scale == scale)
    return;

  /* For xdg_popup using an xdg_positioner, there is a race condition if
   * the application tries to change the size after it's mapped, but before
   * the initial configure is received, so hide and show the surface again
   * force the new size onto the compositor. See bug #772505.
   */

  is_xdg_popup = is_realized_popup (window);
  is_visible = gdk_window_is_visible (window);

  if (is_xdg_popup &&
      is_visible &&
      size_changed &&
      !impl->initial_configure_received &&
      !impl->configuring_popup)
    gdk_window_hide (window);

  gdk_wayland_window_configure (window, width, height, scale);

  if (is_xdg_popup &&
      is_visible &&
      size_changed &&
      !impl->initial_configure_received &&
      !impl->configuring_popup)
    gdk_window_show (window);
}

static void
gdk_wayland_window_sync_parent (GdkWindow *window,
                                GdkWindow *parent)
{
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWindowImplWayland *impl_parent = NULL;

  g_assert (parent == NULL ||
            gdk_window_get_display (window) == gdk_window_get_display (parent));

  if (!is_realized_toplevel (window))
    return;

  if (impl->transient_for)
    impl_parent = GDK_WINDOW_IMPL_WAYLAND (impl->transient_for->impl);
  else if (parent)
    impl_parent = GDK_WINDOW_IMPL_WAYLAND (parent->impl);

  /* XXX: Is this correct? */
  if (impl_parent && !impl_parent->display_server.wl_surface)
    return;

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      {
        struct xdg_toplevel *parent_toplevel;

        if (impl_parent)
          parent_toplevel = impl_parent->display_server.xdg_toplevel;
        else
          parent_toplevel = NULL;

        xdg_toplevel_set_parent (impl->display_server.xdg_toplevel,
                                 parent_toplevel);
        break;
      }
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      {
        struct zxdg_toplevel_v6 *parent_toplevel;

        if (impl_parent)
          parent_toplevel = impl_parent->display_server.zxdg_toplevel_v6;
        else
          parent_toplevel = NULL;

        zxdg_toplevel_v6_set_parent (impl->display_server.zxdg_toplevel_v6,
                                     parent_toplevel);
        break;
      }
    }
}

static void
gdk_wayland_window_sync_parent_of_imported (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (!impl->display_server.wl_surface)
    return;

  if (!is_realized_toplevel (window))
    return;

  if (impl->imported_v2_transient_for)
    {
      zxdg_imported_v2_set_parent_of (impl->imported_v2_transient_for,
                                      impl->display_server.wl_surface);
    }
  else if (impl->imported_v1_transient_for)
    {
      zxdg_imported_v1_set_parent_of (impl->imported_v1_transient_for,
                                      impl->display_server.wl_surface);
    }
}

static void
gdk_wayland_window_update_dialogs (GdkWindow *window)
{
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  GList *l;

  if (!display_wayland->orphan_dialogs)
    return;

  for (l = display_wayland->orphan_dialogs; l; l = l->next)
    {
      GdkWindow *w = l->data;
      GdkWindowImplWayland *impl;

      if (!GDK_IS_WINDOW_IMPL_WAYLAND(w->impl))
        continue;

      impl = GDK_WINDOW_IMPL_WAYLAND (w->impl);
      if (w == window)
	continue;
      if (impl->hint != GDK_WINDOW_TYPE_HINT_DIALOG)
        continue;
      if (impl->transient_for)
        continue;

      /* Update the parent relationship only for dialogs without transients */
      gdk_wayland_window_sync_parent (w, window);
    }
}

static void
gdk_wayland_window_sync_title (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));

  if (!is_realized_toplevel (window))
    return;

  if (!impl->title)
    return;

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_set_title (impl->display_server.xdg_toplevel,
                              impl->title);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_set_title (impl->display_server.zxdg_toplevel_v6,
                                  impl->title);
      break;
    }
}

static void
gdk_wayland_window_get_window_geometry (GdkWindow    *window,
                                        GdkRectangle *geometry)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  *geometry = (GdkRectangle) {
    .x = impl->margin_left,
    .y = impl->margin_top,
    .width = calculate_width_without_margin (window, window->width),
    .height = calculate_height_without_margin (window, window->height)
  };
}

static void
gdk_wayland_window_sync_margin (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  GdkRectangle geometry;

  if (!is_realized_shell_surface (window))
    return;

  gdk_wayland_window_get_window_geometry (window, &geometry);

  g_return_if_fail (geometry.width > 0 && geometry.height > 0);

  gdk_window_set_geometry_hints (window,
                                 &impl->geometry_hints,
                                 impl->geometry_mask);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_surface_set_window_geometry (impl->display_server.xdg_surface,
                                       geometry.x,
                                       geometry.y,
                                       geometry.width,
                                       geometry.height);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_surface_v6_set_window_geometry (impl->display_server.zxdg_surface_v6,
                                           geometry.x,
                                           geometry.y,
                                           geometry.width,
                                           geometry.height);
      break;
    }
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
gdk_wayland_window_sync_opaque_region (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  struct wl_region *wl_region = NULL;

  if (!impl->display_server.wl_surface)
    return;

  if (!impl->opaque_region_dirty)
    return;

  if (impl->opaque_region != NULL)
    wl_region = wl_region_from_cairo_region (GDK_WAYLAND_DISPLAY (gdk_window_get_display (window)),
                                             impl->opaque_region);

  wl_surface_set_opaque_region (impl->display_server.wl_surface, wl_region);

  if (wl_region != NULL)
    wl_region_destroy (wl_region);

  impl->opaque_region_dirty = FALSE;
}

static void
gdk_wayland_window_sync_input_region (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  struct wl_region *wl_region = NULL;

  if (!impl->display_server.wl_surface)
    return;

  if (!impl->input_region_dirty)
    return;

  if (impl->input_region != NULL)
    wl_region = wl_region_from_cairo_region (GDK_WAYLAND_DISPLAY (gdk_window_get_display (window)),
                                             impl->input_region);

  wl_surface_set_input_region (impl->display_server.wl_surface, wl_region);

  if (wl_region != NULL)
    wl_region_destroy (wl_region);

  impl->input_region_dirty = FALSE;
}

static void
gdk_wayland_set_input_region_if_empty (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandDisplay *display;
  struct wl_region *empty;

  if (!impl->input_region_dirty)
    return;

  if (impl->input_region == NULL)
    return;

  if (!cairo_region_is_empty (impl->input_region))
    return;

  display = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  empty = wl_compositor_create_region (display->compositor);

  wl_surface_set_input_region (impl->display_server.wl_surface, empty);
  wl_region_destroy (empty);

  impl->input_region_dirty = FALSE;
}

static void
surface_enter (void              *data,
               struct wl_surface *wl_surface,
               struct wl_output  *output)
{
  GdkWindow *window = GDK_WINDOW (data);
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  gboolean output_is_unmanaged;

  output_is_unmanaged =
    _gdk_wayland_screen_get_output_scale (display_wayland->screen, output) == 0;
  if (output_is_unmanaged)
    return;

  GDK_NOTE (EVENTS,
            g_message ("surface enter, window %p output %p", window, output));

  impl->display_server.outputs = g_slist_prepend (impl->display_server.outputs, output);

  window_update_scale (window);
}

static void
surface_leave (void              *data,
               struct wl_surface *wl_surface,
               struct wl_output  *output)
{
  GdkWindow *window = GDK_WINDOW (data);
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  GDK_NOTE (EVENTS,
            g_message ("surface leave, window %p output %p", window, output));

  impl->display_server.outputs = g_slist_remove (impl->display_server.outputs, output);

  if (impl->display_server.outputs)
    window_update_scale (window);
}

static const struct wl_surface_listener surface_listener = {
  surface_enter,
  surface_leave
};

static void
on_parent_surface_committed (GdkWindowImplWayland *parent_impl,
                             GdkWindow            *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  g_signal_handler_disconnect (parent_impl,
                               impl->parent_surface_committed_handler);
  impl->parent_surface_committed_handler = 0;

  wl_subsurface_set_desync (impl->display_server.wl_subsurface);

  /* Special case if the input region is empty, it won't change on resize */
  gdk_wayland_set_input_region_if_empty (window);
}

static void
gdk_wayland_window_set_subsurface_position (GdkWindow *window,
                                            int        x,
                                            int        y)
{
  GdkWindowImplWayland *impl;

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  wl_subsurface_set_position (impl->display_server.wl_subsurface, x, y);
  impl->subsurface_x = x;
  impl->subsurface_y = y;

  gdk_window_request_transient_parent_commit (window);
}

static void
gdk_wayland_window_create_subsurface (GdkWindow *window)
{
  GdkWindowImplWayland *impl, *parent_impl = NULL;
  GdkWaylandDisplay *display_wayland;

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (!impl->display_server.wl_surface)
    return; /* Bail out, surface and subsurface will be created later when shown */

  if (impl->display_server.wl_subsurface)
    return;

  if (impl->transient_for)
    parent_impl = GDK_WINDOW_IMPL_WAYLAND (impl->transient_for->impl);

  if (parent_impl && parent_impl->display_server.wl_surface)
    {
      display_wayland = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
      impl->display_server.wl_subsurface =
        wl_subcompositor_get_subsurface (display_wayland->subcompositor,
                                         impl->display_server.wl_surface, parent_impl->display_server.wl_surface);

      /* In order to synchronize the initial position with the initial frame
       * content, wait with making the subsurface desynchronized until after
       * the parent was committed.
       */
      impl->parent_surface_committed_handler =
        g_signal_connect_object (parent_impl, "committed",
                                 G_CALLBACK (on_parent_surface_committed),
                                 window, 0);

      gdk_wayland_window_set_subsurface_position (window,
                                                  window->x + window->abs_x,
                                                  window->y + window->abs_y);
    }
}

static void
gdk_wayland_window_create_surface (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));

  impl->display_server.wl_surface = wl_compositor_create_surface (display_wayland->compositor);
  wl_surface_add_listener (impl->display_server.wl_surface, &surface_listener, window);
}

static gboolean
should_use_fixed_size (GdkWindowState state)
{
  return state & (GDK_WINDOW_STATE_MAXIMIZED |
                  GDK_WINDOW_STATE_FULLSCREEN |
                  GDK_WINDOW_STATE_TILED);
}

static gboolean
has_per_edge_tiling_info (GdkWindowState state)
{
  return state & (GDK_WINDOW_STATE_TOP_TILED |
                  GDK_WINDOW_STATE_RIGHT_TILED |
                  GDK_WINDOW_STATE_BOTTOM_TILED |
                  GDK_WINDOW_STATE_LEFT_TILED);
}

static GdkWindowState
infer_edge_constraints (GdkWindowState state)
{
  if (state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN))
    return state;

  if (!(state & GDK_WINDOW_STATE_TILED) || !has_per_edge_tiling_info (state))
    return state |
           GDK_WINDOW_STATE_TOP_RESIZABLE |
           GDK_WINDOW_STATE_RIGHT_RESIZABLE |
           GDK_WINDOW_STATE_BOTTOM_RESIZABLE |
           GDK_WINDOW_STATE_LEFT_RESIZABLE;

  if (!(state & GDK_WINDOW_STATE_TOP_TILED))
    state |= GDK_WINDOW_STATE_TOP_RESIZABLE;
  if (!(state & GDK_WINDOW_STATE_RIGHT_TILED))
    state |= GDK_WINDOW_STATE_RIGHT_RESIZABLE;
  if (!(state & GDK_WINDOW_STATE_BOTTOM_TILED))
    state |= GDK_WINDOW_STATE_BOTTOM_RESIZABLE;
  if (!(state & GDK_WINDOW_STATE_LEFT_TILED))
    state |= GDK_WINDOW_STATE_LEFT_RESIZABLE;

  return state;
}

static gboolean
supports_native_edge_constraints (GdkWindowImplWayland *impl)
{
  struct gtk_surface1 *gtk_surface = impl->display_server.gtk_surface;
  if (!gtk_surface)
    return FALSE;
  return gtk_surface1_get_version (gtk_surface) >= GTK_SURFACE1_CONFIGURE_EDGES_SINCE_VERSION;
}

static void
gdk_wayland_window_handle_configure (GdkWindow *window,
                                     uint32_t   serial)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  GdkFrameClock *frame_clock = gdk_window_get_frame_clock (window);
  GdkWindowState new_state;
  gboolean suspended;
  int width = impl->pending.width;
  int height = impl->pending.height;
  gboolean fixed_size;
  gboolean saved_size;

  if (!impl->initial_configure_received)
    {
      gdk_window_thaw_updates (window);
      impl->initial_configure_received = TRUE;
    }

  if (impl->display_server.xdg_popup)
    {
      xdg_surface_ack_configure (impl->display_server.xdg_surface, serial);
      return;
    }
  else if (impl->display_server.zxdg_popup_v6)
    {
      zxdg_surface_v6_ack_configure (impl->display_server.zxdg_surface_v6,
                                     serial);
      return;
    }

  suspended = impl->pending.suspended;
  impl->pending.suspended = FALSE;

  if (impl->suspended != suspended)
    {
      GdkEvent* event;

      impl->suspended = suspended;

      event = _gdk_make_event (window, GDK_VISIBILITY_NOTIFY, NULL, FALSE);
      event->visibility.state = suspended ? GDK_VISIBILITY_FULLY_OBSCURED : GDK_VISIBILITY_UNOBSCURED;
    }

  new_state = impl->pending.state;
  impl->pending.state = 0;

  if (!supports_native_edge_constraints (impl))
    new_state = infer_edge_constraints (new_state);

  fixed_size = should_use_fixed_size (new_state);

  saved_size = (width == 0 && height == 0);
  /* According to xdg_shell, an xdg_surface.configure with size 0x0
   * should be interpreted as that it is up to the client to set a
   * size.
   *
   * When transitioning from maximize or fullscreen state, this means
   * the client should configure its size back to what it was before
   * being maximize or fullscreen.
   * Additionally, if we receive a manual resize request, we must prefer this
   * new size instead of the compositor's size hints.
   * In such a scenario, and without letting the compositor know about the new
   * size, the client has to manage all dimensions and ignore any server hints.
   */
  if (!fixed_size && (saved_size || impl->saved_size_changed))
    {
      width = impl->saved_width;
      height = impl->saved_height;
      impl->saved_size_changed = FALSE;
    }

  if (width > 0 && height > 0)
    {
      GdkWindowHints geometry_mask = impl->geometry_mask;
      int configure_width;
      int configure_height;

      /* Ignore size increments for maximized/fullscreen windows */
      if (fixed_size)
        geometry_mask &= ~GDK_HINT_RESIZE_INC;
      if (!saved_size)
        {
          /* Do not reapply contrains if we are restoring original size */
          gdk_window_constrain_size (&impl->geometry_hints,
                                     geometry_mask,
                                     calculate_width_with_margin (window, width),
                                     calculate_height_with_margin (window, height),
                                     &width,
                                     &height);

          /* Save size for next time we get 0x0 */
          _gdk_wayland_window_save_size (window);
        }

      if (saved_size)
        {
          configure_width = calculate_width_with_margin (window, width);
          configure_height = calculate_height_with_margin (window, height);
        }
      else
        {
          configure_width = width;
          configure_height = height;
        }
      gdk_wayland_window_configure (window,
                                    configure_width,
                                    configure_height,
                                    impl->scale);
    }
  else
    {
      int unconfigured_width;
      int unconfigured_height;

      unconfigured_width =
        calculate_width_with_margin (window, impl->unconfigured_width);
      unconfigured_height =
        calculate_height_with_margin (window, impl->unconfigured_height);
      gdk_wayland_window_configure (window,
                                    unconfigured_width,
                                    unconfigured_height,
                                    impl->scale);
    }

  if (fixed_size)
    {
      impl->fixed_size_width = width;
      impl->fixed_size_height = height;
    }

  GDK_NOTE (EVENTS,
            g_message ("configure, window %p %dx%d,%s%s%s%s",
                       window, width, height,
                       (new_state & GDK_WINDOW_STATE_FULLSCREEN) ? " fullscreen" : "",
                       (new_state & GDK_WINDOW_STATE_MAXIMIZED) ? " maximized" : "",
                       (new_state & GDK_WINDOW_STATE_FOCUSED) ? " focused" : "",
                       (new_state & GDK_WINDOW_STATE_TILED) ? " tiled" : ""));

  _gdk_set_window_state (window, new_state);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_surface_ack_configure (impl->display_server.xdg_surface, serial);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_surface_v6_ack_configure (impl->display_server.zxdg_surface_v6,
                                     serial);
      break;
    }

  if (impl->hint != GDK_WINDOW_TYPE_HINT_DIALOG &&
      new_state & GDK_WINDOW_STATE_FOCUSED)
    gdk_wayland_window_update_dialogs (window);

  impl->pending_commit = TRUE;
  gdk_frame_clock_request_phase (frame_clock,
                                 GDK_FRAME_CLOCK_PHASE_AFTER_PAINT);
}

static void
gdk_wayland_window_handle_configure_toplevel (GdkWindow     *window,
                                              int32_t        width,
                                              int32_t        height,
                                              GdkWindowState state,
                                              gboolean       suspended)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  impl->pending.state |= state;
  impl->pending.suspended |= suspended;
  impl->pending.width = width;
  impl->pending.height = height;
}

static void
gdk_wayland_window_handle_close (GdkWindow *window)
{
  GdkDisplay *display;
  GdkEvent *event;

  GDK_NOTE (EVENTS,
            g_message ("close %p", window));

  event = gdk_event_new (GDK_DELETE);
  event->any.window = g_object_ref (window);
  event->any.send_event = TRUE;

  display = gdk_window_get_display (window);
  _gdk_wayland_display_deliver_event (display, event);
}

static void
xdg_surface_configure (void               *data,
                       struct xdg_surface *xdg_surface,
                       uint32_t            serial)
{
  GdkWindow *window = GDK_WINDOW (data);

  gdk_wayland_window_handle_configure (window, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
  xdg_surface_configure,
};

static void
xdg_toplevel_configure (void                *data,
                        struct xdg_toplevel *xdg_toplevel,
                        int32_t              width,
                        int32_t              height,
                        struct wl_array     *states)
{
  GdkWindow *window = GDK_WINDOW (data);
  uint32_t *p;
  GdkWindowState pending_state = 0;
  gboolean suspended = FALSE;

  wl_array_for_each (p, states)
    {
      uint32_t state = *p;

      switch (state)
        {
        case XDG_TOPLEVEL_STATE_FULLSCREEN:
          pending_state |= GDK_WINDOW_STATE_FULLSCREEN;
          break;
        case XDG_TOPLEVEL_STATE_MAXIMIZED:
          pending_state |= GDK_WINDOW_STATE_MAXIMIZED;
          break;
        case XDG_TOPLEVEL_STATE_ACTIVATED:
          pending_state |= GDK_WINDOW_STATE_FOCUSED;
          break;
        case XDG_TOPLEVEL_STATE_RESIZING:
          break;
        case XDG_TOPLEVEL_STATE_TILED_TOP:
          pending_state |= (GDK_WINDOW_STATE_TILED | GDK_WINDOW_STATE_TOP_TILED);
          break;
        case XDG_TOPLEVEL_STATE_TILED_RIGHT:
          pending_state |= (GDK_WINDOW_STATE_TILED | GDK_WINDOW_STATE_RIGHT_TILED);
          break;
        case XDG_TOPLEVEL_STATE_TILED_BOTTOM:
          pending_state |= (GDK_WINDOW_STATE_TILED | GDK_WINDOW_STATE_BOTTOM_TILED);
          break;
        case XDG_TOPLEVEL_STATE_TILED_LEFT:
          pending_state |= (GDK_WINDOW_STATE_TILED | GDK_WINDOW_STATE_LEFT_TILED);
          break;
#ifdef HAVE_TOPLEVEL_STATE_SUSPENDED
        case XDG_TOPLEVEL_STATE_SUSPENDED:
          suspended = TRUE;
          break;
#endif
        default:
          /* Unknown state */
          break;
        }
    }

  gdk_wayland_window_handle_configure_toplevel (window, width, height,
                                                pending_state, suspended);
}

static void
xdg_toplevel_close (void                *data,
                    struct xdg_toplevel *xdg_toplevel)
{
  GdkWindow *window = GDK_WINDOW (data);

  gdk_wayland_window_handle_close (window);
}

static void
xdg_toplevel_configure_bounds (void                *data,
                               struct xdg_toplevel *xdg_toplevel,
                               int32_t              width,
                               int32_t              height)
{
}

static void
xdg_toplevel_wm_capabilities (void                *data,
                              struct xdg_toplevel *xdg_toplevel,
                              struct wl_array     *capabilities)
{
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
  xdg_toplevel_configure,
  xdg_toplevel_close,
  xdg_toplevel_configure_bounds,
  xdg_toplevel_wm_capabilities,
};

static void
create_xdg_toplevel_resources (GdkWindow *window)
{
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  impl->display_server.xdg_surface =
    xdg_wm_base_get_xdg_surface (display_wayland->xdg_wm_base,
                                 impl->display_server.wl_surface);
  xdg_surface_add_listener (impl->display_server.xdg_surface,
                            &xdg_surface_listener,
                            window);

  impl->display_server.xdg_toplevel =
    xdg_surface_get_toplevel (impl->display_server.xdg_surface);
  xdg_toplevel_add_listener (impl->display_server.xdg_toplevel,
                             &xdg_toplevel_listener,
                             window);
}

static void
zxdg_surface_v6_configure (void                   *data,
                           struct zxdg_surface_v6 *xdg_surface,
                           uint32_t                serial)
{
  GdkWindow *window = GDK_WINDOW (data);

  gdk_wayland_window_handle_configure (window, serial);
}

static const struct zxdg_surface_v6_listener zxdg_surface_v6_listener = {
  zxdg_surface_v6_configure,
};

static void
zxdg_toplevel_v6_configure (void                    *data,
                            struct zxdg_toplevel_v6 *xdg_toplevel,
                            int32_t                  width,
                            int32_t                  height,
                            struct wl_array         *states)
{
  GdkWindow *window = GDK_WINDOW (data);
  uint32_t *p;
  GdkWindowState pending_state = 0;

  wl_array_for_each (p, states)
    {
      uint32_t state = *p;

      switch (state)
        {
        case ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN:
          pending_state |= GDK_WINDOW_STATE_FULLSCREEN;
          break;
        case ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED:
          pending_state |= GDK_WINDOW_STATE_MAXIMIZED;
          break;
        case ZXDG_TOPLEVEL_V6_STATE_ACTIVATED:
          pending_state |= GDK_WINDOW_STATE_FOCUSED;
          break;
        case ZXDG_TOPLEVEL_V6_STATE_RESIZING:
          break;
        default:
          /* Unknown state */
          break;
        }
    }

  gdk_wayland_window_handle_configure_toplevel (window, width, height,
                                                pending_state, FALSE);
}

static void
zxdg_toplevel_v6_close (void                    *data,
                        struct zxdg_toplevel_v6 *xdg_toplevel)
{
  GdkWindow *window = GDK_WINDOW (data);

  gdk_wayland_window_handle_close (window);
}

static const struct zxdg_toplevel_v6_listener zxdg_toplevel_v6_listener = {
  zxdg_toplevel_v6_configure,
  zxdg_toplevel_v6_close,
};

static void
create_zxdg_toplevel_v6_resources (GdkWindow *window)
{
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  impl->display_server.zxdg_surface_v6 =
    zxdg_shell_v6_get_xdg_surface (display_wayland->zxdg_shell_v6,
                                   impl->display_server.wl_surface);
  zxdg_surface_v6_add_listener (impl->display_server.zxdg_surface_v6,
                                &zxdg_surface_v6_listener,
                                window);

  impl->display_server.zxdg_toplevel_v6 =
    zxdg_surface_v6_get_toplevel (impl->display_server.zxdg_surface_v6);
  zxdg_toplevel_v6_add_listener (impl->display_server.zxdg_toplevel_v6,
                                 &zxdg_toplevel_v6_listener,
                                 window);
}

/**
 * gdk_wayland_window_set_application_id:
 * @window:
 * @application_id:
 *
 * Since: 3.24.22
 */
void
gdk_wayland_window_set_application_id (GdkWindow *window, const char* application_id)
{
  GdkWindowImplWayland *impl;
  GdkWaylandDisplay *display_wayland;

  g_return_if_fail (application_id != NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (!is_realized_toplevel (window))
    return;

  display_wayland = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_set_app_id (impl->display_server.xdg_toplevel,
                               application_id);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_set_app_id (impl->display_server.zxdg_toplevel_v6,
                                   application_id);
      break;
    }
}

static void
gdk_wayland_window_create_xdg_toplevel (GdkWindow *window)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  const gchar *app_id;
  GdkScreen *screen = gdk_window_get_screen (window);
  struct wl_output *fullscreen_output = NULL;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (impl->initial_fullscreen_monitor >= 0 &&
      impl->initial_fullscreen_monitor < gdk_screen_get_n_monitors (screen))
    fullscreen_output =
      _gdk_wayland_screen_get_wl_output (screen,
                                         impl->initial_fullscreen_monitor);
G_GNUC_END_IGNORE_DEPRECATIONS

  gdk_window_freeze_updates (window);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      create_xdg_toplevel_resources (window);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      create_zxdg_toplevel_v6_resources (window);
      break;
    }

  gdk_wayland_window_sync_parent (window, NULL);
  gdk_wayland_window_sync_parent_of_imported (window);
  gdk_wayland_window_sync_title (window);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      if (window->state & GDK_WINDOW_STATE_MAXIMIZED)
        xdg_toplevel_set_maximized (impl->display_server.xdg_toplevel);
      if (window->state & GDK_WINDOW_STATE_FULLSCREEN)
        xdg_toplevel_set_fullscreen (impl->display_server.xdg_toplevel,
                                     fullscreen_output);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      if (window->state & GDK_WINDOW_STATE_MAXIMIZED)
        zxdg_toplevel_v6_set_maximized (impl->display_server.zxdg_toplevel_v6);
      if (window->state & GDK_WINDOW_STATE_FULLSCREEN)
        zxdg_toplevel_v6_set_fullscreen (impl->display_server.zxdg_toplevel_v6,
                                         fullscreen_output);
      break;
    }

  app_id = g_get_prgname ();
  if (app_id == NULL)
    app_id = gdk_get_program_class ();

  gdk_wayland_window_set_application_id (window, app_id);

  maybe_set_gtk_surface_dbus_properties (window);
  maybe_set_gtk_surface_modal (window);

  if (impl->hint == GDK_WINDOW_TYPE_HINT_DIALOG)
    _gdk_wayland_screen_add_orphan_dialog (window);

  wl_surface_commit (impl->display_server.wl_surface);
}

static void
gdk_wayland_window_handle_configure_popup (GdkWindow *window,
                                           int32_t    x,
                                           int32_t    y,
                                           int32_t    width,
                                           int32_t    height)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkRectangle flipped_rect;
  GdkRectangle final_rect;
  gboolean flipped_x;
  gboolean flipped_y;

  g_return_if_fail (impl->transient_for);

  if (impl->position_method != POSITION_METHOD_MOVE_TO_RECT)
    return;

  calculate_moved_to_rect_result (window, x, y, width, height,
                                  &flipped_rect,
                                  &final_rect,
                                  &flipped_x,
                                  &flipped_y);

  impl->position_method = POSITION_METHOD_MOVE_TO_RECT;

  g_signal_emit_by_name (window,
                         "moved-to-rect",
                         &flipped_rect,
                         &final_rect,
                         flipped_x,
                         flipped_y);
}

static void
xdg_popup_configure (void             *data,
                     struct xdg_popup *xdg_popup,
                     int32_t           x,
                     int32_t           y,
                     int32_t           width,
                     int32_t           height)
{
  GdkWindow *window = GDK_WINDOW (data);

  gdk_wayland_window_handle_configure_popup (window, x, y, width, height);
}

static void
xdg_popup_done (void             *data,
                struct xdg_popup *xdg_popup)
{
  GdkWindow *window = GDK_WINDOW (data);

  GDK_NOTE (EVENTS,
            g_message ("done %p", window));

  gdk_window_hide (window);
}

static const struct xdg_popup_listener xdg_popup_listener = {
  xdg_popup_configure,
  xdg_popup_done,
};

static void
zxdg_popup_v6_configure (void                 *data,
                         struct zxdg_popup_v6 *xdg_popup,
                         int32_t               x,
                         int32_t               y,
                         int32_t               width,
                         int32_t               height)
{
  GdkWindow *window = GDK_WINDOW (data);

  gdk_wayland_window_handle_configure_popup (window, x, y, width, height);
}

static void
zxdg_popup_v6_done (void                 *data,
                    struct zxdg_popup_v6 *xdg_popup)
{
  GdkWindow *window = GDK_WINDOW (data);

  GDK_NOTE (EVENTS,
            g_message ("done %p", window));

  gdk_window_hide (window);
}

static const struct zxdg_popup_v6_listener zxdg_popup_v6_listener = {
  zxdg_popup_v6_configure,
  zxdg_popup_v6_done,
};

static enum xdg_positioner_anchor
rect_anchor_to_anchor (GdkGravity rect_anchor)
{
  switch (rect_anchor)
    {
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_STATIC:
      return XDG_POSITIONER_ANCHOR_TOP_LEFT;
    case GDK_GRAVITY_NORTH:
      return XDG_POSITIONER_ANCHOR_TOP;
    case GDK_GRAVITY_NORTH_EAST:
      return XDG_POSITIONER_ANCHOR_TOP_RIGHT;
    case GDK_GRAVITY_WEST:
      return XDG_POSITIONER_ANCHOR_LEFT;
    case GDK_GRAVITY_CENTER:
      return XDG_POSITIONER_ANCHOR_NONE;
    case GDK_GRAVITY_EAST:
      return XDG_POSITIONER_ANCHOR_RIGHT;
    case GDK_GRAVITY_SOUTH_WEST:
      return XDG_POSITIONER_ANCHOR_BOTTOM_LEFT;
    case GDK_GRAVITY_SOUTH:
      return XDG_POSITIONER_ANCHOR_BOTTOM;
    case GDK_GRAVITY_SOUTH_EAST:
      return XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT;
    default:
      g_assert_not_reached ();
    }
}

static enum xdg_positioner_gravity
window_anchor_to_gravity (GdkGravity rect_anchor)
{
  switch (rect_anchor)
    {
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_STATIC:
      return XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT;
    case GDK_GRAVITY_NORTH:
      return XDG_POSITIONER_GRAVITY_BOTTOM;
    case GDK_GRAVITY_NORTH_EAST:
      return XDG_POSITIONER_GRAVITY_BOTTOM_LEFT;
    case GDK_GRAVITY_WEST:
      return XDG_POSITIONER_GRAVITY_RIGHT;
    case GDK_GRAVITY_CENTER:
      return XDG_POSITIONER_GRAVITY_NONE;
    case GDK_GRAVITY_EAST:
      return XDG_POSITIONER_GRAVITY_LEFT;
    case GDK_GRAVITY_SOUTH_WEST:
      return XDG_POSITIONER_GRAVITY_TOP_RIGHT;
    case GDK_GRAVITY_SOUTH:
      return XDG_POSITIONER_GRAVITY_TOP;
    case GDK_GRAVITY_SOUTH_EAST:
      return XDG_POSITIONER_GRAVITY_TOP_LEFT;
    default:
      g_assert_not_reached ();
    }
}

static enum zxdg_positioner_v6_anchor
rect_anchor_to_anchor_legacy (GdkGravity rect_anchor)
{
  switch (rect_anchor)
    {
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_STATIC:
      return (ZXDG_POSITIONER_V6_ANCHOR_TOP |
              ZXDG_POSITIONER_V6_ANCHOR_LEFT);
    case GDK_GRAVITY_NORTH:
      return ZXDG_POSITIONER_V6_ANCHOR_TOP;
    case GDK_GRAVITY_NORTH_EAST:
      return (ZXDG_POSITIONER_V6_ANCHOR_TOP |
              ZXDG_POSITIONER_V6_ANCHOR_RIGHT);
    case GDK_GRAVITY_WEST:
      return ZXDG_POSITIONER_V6_ANCHOR_LEFT;
    case GDK_GRAVITY_CENTER:
      return ZXDG_POSITIONER_V6_ANCHOR_NONE;
    case GDK_GRAVITY_EAST:
      return ZXDG_POSITIONER_V6_ANCHOR_RIGHT;
    case GDK_GRAVITY_SOUTH_WEST:
      return (ZXDG_POSITIONER_V6_ANCHOR_BOTTOM |
              ZXDG_POSITIONER_V6_ANCHOR_LEFT);
    case GDK_GRAVITY_SOUTH:
      return ZXDG_POSITIONER_V6_ANCHOR_BOTTOM;
    case GDK_GRAVITY_SOUTH_EAST:
      return (ZXDG_POSITIONER_V6_ANCHOR_BOTTOM |
              ZXDG_POSITIONER_V6_ANCHOR_RIGHT);
    default:
      g_assert_not_reached ();
    }
}

static enum zxdg_positioner_v6_gravity
window_anchor_to_gravity_legacy (GdkGravity rect_anchor)
{
  switch (rect_anchor)
    {
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_STATIC:
      return (ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
              ZXDG_POSITIONER_V6_GRAVITY_RIGHT);
    case GDK_GRAVITY_NORTH:
      return ZXDG_POSITIONER_V6_GRAVITY_BOTTOM;
    case GDK_GRAVITY_NORTH_EAST:
      return (ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
              ZXDG_POSITIONER_V6_GRAVITY_LEFT);
    case GDK_GRAVITY_WEST:
      return ZXDG_POSITIONER_V6_GRAVITY_RIGHT;
    case GDK_GRAVITY_CENTER:
      return ZXDG_POSITIONER_V6_GRAVITY_NONE;
    case GDK_GRAVITY_EAST:
      return ZXDG_POSITIONER_V6_GRAVITY_LEFT;
    case GDK_GRAVITY_SOUTH_WEST:
      return (ZXDG_POSITIONER_V6_GRAVITY_TOP |
              ZXDG_POSITIONER_V6_GRAVITY_RIGHT);
    case GDK_GRAVITY_SOUTH:
      return ZXDG_POSITIONER_V6_GRAVITY_TOP;
    case GDK_GRAVITY_SOUTH_EAST:
      return (ZXDG_POSITIONER_V6_GRAVITY_TOP |
              ZXDG_POSITIONER_V6_GRAVITY_LEFT);
    default:
      g_assert_not_reached ();
    }
}

static void
kwin_server_decoration_mode_set (void *data, struct org_kde_kwin_server_decoration *org_kde_kwin_server_decoration, uint32_t mode)
{
  GdkWindow *window = GDK_WINDOW (data);
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if ((mode == ORG_KDE_KWIN_SERVER_DECORATION_MODE_SERVER && impl->using_csd) ||
        (mode == ORG_KDE_KWIN_SERVER_DECORATION_MODE_CLIENT && !impl->using_csd))
    gdk_wayland_window_announce_decoration_mode (window);
}

static const struct org_kde_kwin_server_decoration_listener kwin_server_decoration_listener = {
  kwin_server_decoration_mode_set
};

static void
gdk_wayland_window_announce_decoration_mode (GdkWindow *window)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (!display_wayland->server_decoration_manager)
    return;
  if (!impl->display_server.server_decoration)
    {
      impl->display_server.server_decoration =
        org_kde_kwin_server_decoration_manager_create (display_wayland->server_decoration_manager,
                                                                                         impl->display_server.wl_surface);
      org_kde_kwin_server_decoration_add_listener (impl->display_server.server_decoration,
                                                                                &kwin_server_decoration_listener,
                                                                                window);
  }

  if (impl->display_server.server_decoration)
    {
      if (impl->using_csd)
        org_kde_kwin_server_decoration_request_mode (impl->display_server.server_decoration,
                                                                                     ORG_KDE_KWIN_SERVER_DECORATION_MODE_CLIENT);
      else
        org_kde_kwin_server_decoration_request_mode (impl->display_server.server_decoration,
                                                                                     ORG_KDE_KWIN_SERVER_DECORATION_MODE_SERVER);
    }
}

void
gdk_wayland_window_announce_csd (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  impl->using_csd = TRUE;
  if (impl->mapped)
    gdk_wayland_window_announce_decoration_mode (window);
}

void
gdk_wayland_window_announce_ssd (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  impl->using_csd = FALSE;
  if (impl->mapped)
    gdk_wayland_window_announce_decoration_mode (window);
}

static GdkWindow *
get_real_parent_and_translate (GdkWindow *window,
                               gint      *x,
                               gint      *y)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWindow *parent = impl->transient_for;

  while (parent)
    {
      GdkWindowImplWayland *parent_impl =
        GDK_WINDOW_IMPL_WAYLAND (parent->impl);
      GdkWindow *effective_parent = gdk_window_get_effective_parent (parent);

      if (parent == NULL || (gdk_window_has_native (parent) &&
           !parent_impl->display_server.wl_subsurface) ||
          !effective_parent)
        break;

      *x += parent->x;
      *y += parent->y;

      if (gdk_window_has_native (parent) &&
          parent_impl->display_server.wl_subsurface)
        parent = parent->transient_for;
      else
        parent = effective_parent;
    }

  return parent;
}

static void
translate_to_real_parent_window_geometry (GdkWindow  *window,
                                          gint       *x,
                                          gint       *y)
{
  GdkWindow *parent;

  parent = get_real_parent_and_translate (window, x, y);

  if (parent != NULL) {
    *x -= parent->shadow_left;
    *y -= parent->shadow_top;
  }
}

static GdkWindow *
translate_from_real_parent_window_geometry (GdkWindow *window,
                                            gint      *x,
                                            gint      *y)
{
  GdkWindow *parent;
  gint dx = 0;
  gint dy = 0;

  parent = get_real_parent_and_translate (window, &dx, &dy);

  *x -= dx;
  *y -= dy;

  if (parent != NULL) {
    *x += parent->shadow_left;
    *y += parent->shadow_top;
  }

  return parent;
}

static void
calculate_popup_rect (GdkWindow    *window,
                      GdkGravity    rect_anchor,
                      GdkGravity    window_anchor,
                      GdkRectangle *out_rect)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkRectangle geometry;
  GdkRectangle anchor_rect;
  int x = 0, y = 0;

  gdk_wayland_window_get_window_geometry (window, &geometry);

  anchor_rect = (GdkRectangle) {
    .x = (impl->pending_move_to_rect.rect.x +
          impl->pending_move_to_rect.rect_anchor_dx),
    .y = (impl->pending_move_to_rect.rect.y +
          impl->pending_move_to_rect.rect_anchor_dy),
    .width = impl->pending_move_to_rect.rect.width,
    .height = impl->pending_move_to_rect.rect.height
  };

  switch (rect_anchor)
    {
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      x = anchor_rect.x;
      y = anchor_rect.y;
      break;
    case GDK_GRAVITY_NORTH:
      x = anchor_rect.x + (anchor_rect.width / 2);
      y = anchor_rect.y;
      break;
    case GDK_GRAVITY_NORTH_EAST:
      x = anchor_rect.x + anchor_rect.width;
      y = anchor_rect.y;
      break;
    case GDK_GRAVITY_WEST:
      x = anchor_rect.x;
      y = anchor_rect.y + (anchor_rect.height / 2);
      break;
    case GDK_GRAVITY_CENTER:
      x = anchor_rect.x + (anchor_rect.width / 2);
      y = anchor_rect.y + (anchor_rect.height / 2);
      break;
    case GDK_GRAVITY_EAST:
      x = anchor_rect.x + anchor_rect.width;
      y = anchor_rect.y + (anchor_rect.height / 2);
      break;
    case GDK_GRAVITY_SOUTH_WEST:
      x = anchor_rect.x;
      y = anchor_rect.y + anchor_rect.height;
      break;
    case GDK_GRAVITY_SOUTH:
      x = anchor_rect.x + (anchor_rect.width / 2);
      y = anchor_rect.y + anchor_rect.height;
      break;
    case GDK_GRAVITY_SOUTH_EAST:
      x = anchor_rect.x + anchor_rect.width;
      y = anchor_rect.y + anchor_rect.height;
      break;
    }

  switch (window_anchor)
    {
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      break;
    case GDK_GRAVITY_NORTH:
      x -= geometry.width / 2;
      break;
    case GDK_GRAVITY_NORTH_EAST:
      x -= geometry.width;
      break;
    case GDK_GRAVITY_WEST:
      y -= geometry.height / 2;
      break;
    case GDK_GRAVITY_CENTER:
      x -= geometry.width / 2;
      y -= geometry.height / 2;
      break;
    case GDK_GRAVITY_EAST:
      x -= geometry.width;
      y -= geometry.height / 2;
      break;
    case GDK_GRAVITY_SOUTH_WEST:
      y -= geometry.height;
      break;
    case GDK_GRAVITY_SOUTH:
      x -= geometry.width / 2;
      y -= geometry.height;
      break;
    case GDK_GRAVITY_SOUTH_EAST:
      x -= geometry.width;
      y -= geometry.height;
      break;
    }

  *out_rect = (GdkRectangle) {
    .x = x,
    .y = y,
    .width = geometry.width,
    .height = geometry.height
  };
}

static GdkGravity
flip_anchor_horizontally (GdkGravity anchor)
{
  switch (anchor)
    {
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      return GDK_GRAVITY_NORTH_EAST;
    case GDK_GRAVITY_NORTH:
      return GDK_GRAVITY_NORTH;
    case GDK_GRAVITY_NORTH_EAST:
      return GDK_GRAVITY_NORTH_WEST;
    case GDK_GRAVITY_WEST:
      return GDK_GRAVITY_EAST;
    case GDK_GRAVITY_CENTER:
      return GDK_GRAVITY_CENTER;
    case GDK_GRAVITY_EAST:
      return GDK_GRAVITY_WEST;
    case GDK_GRAVITY_SOUTH_WEST:
      return GDK_GRAVITY_SOUTH_EAST;
    case GDK_GRAVITY_SOUTH:
      return GDK_GRAVITY_SOUTH;
    case GDK_GRAVITY_SOUTH_EAST:
      return GDK_GRAVITY_SOUTH_WEST;
    }

  g_assert_not_reached ();
}

static GdkGravity
flip_anchor_vertically (GdkGravity anchor)
{
  switch (anchor)
    {
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      return GDK_GRAVITY_SOUTH_WEST;
    case GDK_GRAVITY_NORTH:
      return GDK_GRAVITY_SOUTH;
    case GDK_GRAVITY_NORTH_EAST:
      return GDK_GRAVITY_SOUTH_EAST;
    case GDK_GRAVITY_WEST:
      return GDK_GRAVITY_WEST;
    case GDK_GRAVITY_CENTER:
      return GDK_GRAVITY_CENTER;
    case GDK_GRAVITY_EAST:
      return GDK_GRAVITY_EAST;
    case GDK_GRAVITY_SOUTH_WEST:
      return GDK_GRAVITY_NORTH_WEST;
    case GDK_GRAVITY_SOUTH:
      return GDK_GRAVITY_NORTH;
    case GDK_GRAVITY_SOUTH_EAST:
      return GDK_GRAVITY_NORTH_EAST;
    }

  g_assert_not_reached ();
}

static void
calculate_moved_to_rect_result (GdkWindow    *window,
                                int           x,
                                int           y,
                                int           width,
                                int           height,
                                GdkRectangle *flipped_rect,
                                GdkRectangle *final_rect,
                                gboolean     *flipped_x,
                                gboolean     *flipped_y)
{
  GdkWindowImplWayland *impl;
  GdkWindow *parent;
  gint window_x, window_y;
  gint window_width, window_height;
  GdkRectangle best_rect;

  g_return_if_fail (GDK_IS_WAYLAND_WINDOW (window));
  g_return_if_fail (GDK_IS_WINDOW_IMPL_WAYLAND (window->impl));

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  parent = translate_from_real_parent_window_geometry (window, &x, &y);
  *final_rect = (GdkRectangle) {
    .x = x,
    .y = y,
    .width = width,
    .height = height,
  };

  window_x = parent->x + x;
  window_y = parent->y + y;
  window_width = width + window->shadow_left + window->shadow_right;
  window_height = height + window->shadow_top + window->shadow_bottom;

  impl->configuring_popup = TRUE;
  gdk_window_move_resize (window,
                          window_x, window_y,
                          window_width, window_height);
  impl->configuring_popup = FALSE;

  calculate_popup_rect (window,
                        impl->pending_move_to_rect.rect_anchor,
                        impl->pending_move_to_rect.window_anchor,
                        &best_rect);

  *flipped_rect = best_rect;

  if (x != best_rect.x &&
      impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_FLIP_X)
    {
      GdkRectangle flipped_x_rect;
      GdkGravity flipped_rect_anchor;
      GdkGravity flipped_window_anchor;

      flipped_rect_anchor =
        flip_anchor_horizontally (impl->pending_move_to_rect.rect_anchor);
      flipped_window_anchor =
        flip_anchor_horizontally (impl->pending_move_to_rect.window_anchor),
      calculate_popup_rect (window,
                            flipped_rect_anchor,
                            flipped_window_anchor,
                            &flipped_x_rect);

      if (flipped_x_rect.x == x)
        flipped_rect->x = x;
    }
  if (y != best_rect.y &&
      impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_FLIP_Y)
    {
      GdkRectangle flipped_y_rect;
      GdkGravity flipped_rect_anchor;
      GdkGravity flipped_window_anchor;

      flipped_rect_anchor =
        flip_anchor_vertically (impl->pending_move_to_rect.rect_anchor);
      flipped_window_anchor =
        flip_anchor_vertically (impl->pending_move_to_rect.window_anchor),
      calculate_popup_rect (window,
                            flipped_rect_anchor,
                            flipped_window_anchor,
                            &flipped_y_rect);

      if (flipped_y_rect.y == y)
        flipped_rect->y = y;
    }

  *flipped_x = flipped_rect->x != best_rect.x;
  *flipped_y = flipped_rect->y != best_rect.y;
}

static gpointer
create_dynamic_positioner (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandDisplay *display =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  GdkRectangle geometry;
  uint32_t constraint_adjustment = ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_NONE;
  gint real_anchor_rect_x, real_anchor_rect_y;
  gint anchor_rect_width, anchor_rect_height;

  gdk_wayland_window_get_window_geometry (window, &geometry);

  real_anchor_rect_x = impl->pending_move_to_rect.rect.x;
  real_anchor_rect_y = impl->pending_move_to_rect.rect.y;
  translate_to_real_parent_window_geometry (window,
                                            &real_anchor_rect_x,
                                            &real_anchor_rect_y);

  anchor_rect_width = impl->pending_move_to_rect.rect.width;
  anchor_rect_height = impl->pending_move_to_rect.rect.height;

  switch (display->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      {
        struct xdg_positioner *positioner;
        enum xdg_positioner_anchor anchor;
        enum xdg_positioner_gravity gravity;

        positioner = xdg_wm_base_create_positioner (display->xdg_wm_base);

        xdg_positioner_set_size (positioner, geometry.width, geometry.height);
        xdg_positioner_set_anchor_rect (positioner,
                                        real_anchor_rect_x,
                                        real_anchor_rect_y,
                                        anchor_rect_width,
                                        anchor_rect_height);
        xdg_positioner_set_offset (positioner,
                                   impl->pending_move_to_rect.rect_anchor_dx,
                                   impl->pending_move_to_rect.rect_anchor_dy);

        anchor = rect_anchor_to_anchor (impl->pending_move_to_rect.rect_anchor);
        xdg_positioner_set_anchor (positioner, anchor);

        gravity = window_anchor_to_gravity (impl->pending_move_to_rect.window_anchor);
        xdg_positioner_set_gravity (positioner, gravity);

        if (impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_FLIP_X)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X;
        if (impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_FLIP_Y)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y;
        if (impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_SLIDE_X)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X;
        if (impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_SLIDE_Y)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
        if (impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_RESIZE_X)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X;
        if (impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_RESIZE_Y)
          constraint_adjustment |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y;
        xdg_positioner_set_constraint_adjustment (positioner,
                                                  constraint_adjustment);

        return positioner;
      }
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      {
        struct zxdg_positioner_v6 *positioner;
        enum zxdg_positioner_v6_anchor anchor;
        enum zxdg_positioner_v6_gravity gravity;

        positioner = zxdg_shell_v6_create_positioner (display->zxdg_shell_v6);

        zxdg_positioner_v6_set_size (positioner, geometry.width, geometry.height);
        zxdg_positioner_v6_set_anchor_rect (positioner,
                                            real_anchor_rect_x,
                                            real_anchor_rect_y,
                                            anchor_rect_width,
                                            anchor_rect_height);
        zxdg_positioner_v6_set_offset (positioner,
                                       impl->pending_move_to_rect.rect_anchor_dx,
                                       impl->pending_move_to_rect.rect_anchor_dy);

        anchor = rect_anchor_to_anchor_legacy (impl->pending_move_to_rect.rect_anchor);
        zxdg_positioner_v6_set_anchor (positioner, anchor);

        gravity = window_anchor_to_gravity_legacy (impl->pending_move_to_rect.window_anchor);
        zxdg_positioner_v6_set_gravity (positioner, gravity);

        if (impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_FLIP_X)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_X;
        if (impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_FLIP_Y)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y;
        if (impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_SLIDE_X)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_X;
        if (impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_SLIDE_Y)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
        if (impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_RESIZE_X)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_X;
        if (impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_RESIZE_Y)
          constraint_adjustment |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_Y;
        zxdg_positioner_v6_set_constraint_adjustment (positioner,
                                                      constraint_adjustment);

        return positioner;
      }
    }

  g_assert_not_reached ();
}

static gpointer
create_simple_positioner (GdkWindow *window,
                          GdkWindow *parent)
{
  GdkWaylandDisplay *display =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  GdkRectangle geometry;
  GdkRectangle parent_geometry;
  int parent_x, parent_y;

  gdk_wayland_window_get_window_geometry (window, &geometry);

  parent_x = parent->x;
  parent_y = parent->y;

  gdk_wayland_window_get_window_geometry (parent, &parent_geometry);
  parent_x += parent_geometry.x;
  parent_y += parent_geometry.y;

  switch (display->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      {
        struct xdg_positioner *positioner;

        positioner = xdg_wm_base_create_positioner (display->xdg_wm_base);
        xdg_positioner_set_size (positioner, geometry.width, geometry.height);
        xdg_positioner_set_anchor_rect (positioner,
                                        (window->x + geometry.x) - parent_x,
                                        (window->y + geometry.y) - parent_y,
                                        1, 1);
        xdg_positioner_set_anchor (positioner,
                                   XDG_POSITIONER_ANCHOR_TOP_LEFT);
        xdg_positioner_set_gravity (positioner,
                                    XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);

        return positioner;
      }
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      {
        struct zxdg_positioner_v6 *positioner;

        positioner = zxdg_shell_v6_create_positioner (display->zxdg_shell_v6);
        zxdg_positioner_v6_set_size (positioner, geometry.width, geometry.height);
        zxdg_positioner_v6_set_anchor_rect (positioner,
                                            (window->x + geometry.x) - parent_x,
                                            (window->y + geometry.y) - parent_y,
                                            1, 1);
        zxdg_positioner_v6_set_anchor (positioner,
                                       (ZXDG_POSITIONER_V6_ANCHOR_TOP |
                                        ZXDG_POSITIONER_V6_ANCHOR_LEFT));
        zxdg_positioner_v6_set_gravity (positioner,
                                        (ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
                                         ZXDG_POSITIONER_V6_GRAVITY_RIGHT));

        return positioner;
      }
    }

  g_assert_not_reached ();
}

static void
gdk_wayland_window_create_xdg_popup (GdkWindow      *window,
                                     GdkWindow      *parent,
                                     struct wl_seat *seat)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWindowImplWayland *parent_impl = GDK_WINDOW_IMPL_WAYLAND (parent->impl);
  gpointer positioner;
  GdkSeat *gdk_seat;
  guint32 serial;

  if (!impl->display_server.wl_surface)
    return;

  if (!is_realized_shell_surface (parent))
    return;

  if (is_realized_toplevel (window))
    {
      g_warning ("Can't map popup, already mapped as toplevel");
      return;
    }
  if (is_realized_popup (window))
    {
      g_warning ("Can't map popup, already mapped");
      return;
    }
  if ((display->current_popups &&
       g_list_last (display->current_popups)->data != parent) ||
      (!display->current_popups &&
       !is_realized_toplevel (parent)))
    {
      g_warning ("Tried to map a popup with a non-top most parent");
      return;
    }

  gdk_window_freeze_updates (window);

  if (impl->position_method == POSITION_METHOD_MOVE_TO_RECT)
    positioner = create_dynamic_positioner (window);
  else
    positioner = create_simple_positioner (window, parent);

  switch (display->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      impl->display_server.xdg_surface =
        xdg_wm_base_get_xdg_surface (display->xdg_wm_base,
                                     impl->display_server.wl_surface);
      xdg_surface_add_listener (impl->display_server.xdg_surface,
                                &xdg_surface_listener,
                                window);
      impl->display_server.xdg_popup =
        xdg_surface_get_popup (impl->display_server.xdg_surface,
                               parent_impl->display_server.xdg_surface,
                               positioner);
      xdg_popup_add_listener (impl->display_server.xdg_popup,
                              &xdg_popup_listener,
                              window);
      xdg_positioner_destroy (positioner);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      impl->display_server.zxdg_surface_v6 =
        zxdg_shell_v6_get_xdg_surface (display->zxdg_shell_v6,
                                       impl->display_server.wl_surface);
      zxdg_surface_v6_add_listener (impl->display_server.zxdg_surface_v6,
                                    &zxdg_surface_v6_listener,
                                    window);
      impl->display_server.zxdg_popup_v6 =
        zxdg_surface_v6_get_popup (impl->display_server.zxdg_surface_v6,
                                   parent_impl->display_server.zxdg_surface_v6,
                                   positioner);
      zxdg_popup_v6_add_listener (impl->display_server.zxdg_popup_v6,
                                  &zxdg_popup_v6_listener,
                                  window);
      zxdg_positioner_v6_destroy (positioner);
      break;
    }

  if (seat)
    {
      gdk_seat = gdk_display_get_default_seat (GDK_DISPLAY (display));
      serial = _gdk_wayland_seat_get_last_implicit_grab_serial (gdk_seat, NULL);

      switch (display->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_popup_grab (impl->display_server.xdg_popup, seat, serial);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_popup_v6_grab (impl->display_server.zxdg_popup_v6, seat, serial);
          break;
        }
    }

  wl_surface_commit (impl->display_server.wl_surface);

  impl->popup_parent = parent;
  display->current_popups = g_list_append (display->current_popups, window);
}

static struct wl_seat *
find_grab_input_seat (GdkWindow *window, GdkWindow *transient_for)
{
  GdkWindow *attached_grab_window;
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWindowImplWayland *tmp_impl;

  /* Use the device that was used for the grab as the device for
   * the popup window setup - so this relies on GTK+ taking the
   * grab before showing the popup window.
   */
  if (impl->grab_input_seat)
    return gdk_wayland_seat_get_wl_seat (impl->grab_input_seat);

  /* HACK: GtkMenu grabs a special window known as the "grab transfer window"
   * and then transfers the grab over to the correct window later. Look for
   * this window when taking the grab to know it's correct.
   *
   * See: associate_menu_grab_transfer_window in gtkmenu.c
   */
  attached_grab_window = g_object_get_data (G_OBJECT (window), "gdk-attached-grab-window");
  if (attached_grab_window)
    {
      tmp_impl = GDK_WINDOW_IMPL_WAYLAND (attached_grab_window->impl);
      if (tmp_impl->grab_input_seat)
        return gdk_wayland_seat_get_wl_seat (tmp_impl->grab_input_seat);
    }

  while (transient_for)
    {
      tmp_impl = GDK_WINDOW_IMPL_WAYLAND (transient_for->impl);

      if (tmp_impl->grab_input_seat)
        return gdk_wayland_seat_get_wl_seat (tmp_impl->grab_input_seat);

      transient_for = tmp_impl->transient_for;
    }

  return NULL;
}

static gboolean
should_be_mapped (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  /* Don't map crazy temp that GTK+ uses for internal X11 shenanigans. */
  if (window->window_type == GDK_WINDOW_TEMP && window->x < 0 && window->y < 0)
    return FALSE;

  if (impl->hint == GDK_WINDOW_TYPE_HINT_DND)
    return FALSE;

  return TRUE;
}

static gboolean
should_map_as_popup (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  /* Ideally, popup would be temp windows with a parent and grab */
  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TEMP)
    {
      /* If a temp window has a parent and a grab, we can use a popup */
      if (impl->transient_for)
        {
          if (impl->grab_input_seat)
            return TRUE;
        }
    }

  /* Yet we need to keep the window type hint tests for compatibility */
  switch (impl->hint)
    {
    case GDK_WINDOW_TYPE_HINT_POPUP_MENU:
    case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU:
    case GDK_WINDOW_TYPE_HINT_COMBO:
      return TRUE;

    default:
      break;
    }

  if (impl->position_method == POSITION_METHOD_MOVE_TO_RECT)
    return TRUE;

  return FALSE;
}

static gboolean
should_map_as_subsurface (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_SUBSURFACE)
    return TRUE;

  if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_TEMP)
    return FALSE;

  /* if we want a popup, we do not want a subsurface */
  if (should_map_as_popup (window))
    return FALSE;

  if (impl->transient_for)
    {
      GdkWindowImplWayland *impl_parent;

      impl_parent = GDK_WINDOW_IMPL_WAYLAND (impl->transient_for->impl);
      /* subsurface require that the parent is mapped */
      if (impl_parent->mapped)
        return TRUE;
      else
        g_warning ("Couldn't map window %p as subsurface because its parent is not mapped.",
                   window);

    }

  return FALSE;
}

/* Get the window that can be used as a parent for a popup, i.e. a xdg_toplevel
 * or xdg_popup. If the window is not, traverse up the transiency parents until
 * we find one.
 */
static GdkWindow *
get_popup_parent (GdkWindow *window)
{
  while (window)
    {
      GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

      if (is_realized_popup (window) || is_realized_toplevel (window))
        return window;

      window = impl->transient_for;
    }

  return NULL;
}

static void
gdk_wayland_window_map (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWindow *transient_for = NULL;

  if (!should_be_mapped (window))
    return;

  if (impl->mapped || impl->use_custom_surface)
    return;

  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TEMP)
    {
      if (!impl->transient_for)
        {
          g_message ("Window %p is a temporary window without parent, "
                     "application will not be able to position it on screen.",
                     window);
        }
    }

  if (should_map_as_subsurface (window))
    {
      if (impl->transient_for)
        gdk_wayland_window_create_subsurface (window);
      else
        g_warning ("Couldn't map window %p as susburface yet because it doesn't have a parent",
                   window);
    }
  else if (should_map_as_popup (window))
    {
      gboolean create_fallback = FALSE;
      struct wl_seat *grab_input_seat;

      /* Popup menus can appear without a transient parent, which means they
       * cannot be positioned properly on Wayland. This attempts to guess the
       * surface they should be positioned with by finding the surface beneath
       * the device that created the grab for the popup window.
       */
      if (!impl->transient_for && impl->hint == GDK_WINDOW_TYPE_HINT_POPUP_MENU)
        {
          GdkDevice *grab_device = NULL;

          /* The popup menu window is not the grabbed window. This may mean
           * that a "transfer window" (see gtkmenu.c) is used, and we need
           * to find that window to get the grab device. If so is the case
           * the "transfer window" can be retrieved via the
           * "gdk-attached-grab-window" associated data field.
           */
          if (!impl->grab_input_seat)
            {
              GdkWindow *attached_grab_window =
                g_object_get_data (G_OBJECT (window),
                                   "gdk-attached-grab-window");
              if (attached_grab_window)
                {
                  GdkWindowImplWayland *attached_impl =
                    GDK_WINDOW_IMPL_WAYLAND (attached_grab_window->impl);
                  grab_device = gdk_seat_get_pointer (attached_impl->grab_input_seat);
                  transient_for =
                    gdk_device_get_window_at_position (grab_device,
                                                       NULL, NULL);
                }
            }
          else
            {
              grab_device = gdk_seat_get_pointer (impl->grab_input_seat);
              transient_for =
                gdk_device_get_window_at_position (grab_device, NULL, NULL);
            }

          if (transient_for)
            transient_for = get_popup_parent (gdk_window_get_effective_toplevel (transient_for));

          /* If the position was not explicitly set, start the popup at the
           * position of the device that holds the grab.
           */
          if (impl->position_method == POSITION_METHOD_NONE && grab_device)
            gdk_window_get_device_position (transient_for, grab_device,
                                            &window->x, &window->y, NULL);
        }
      else
        {
          transient_for = gdk_window_get_effective_toplevel (impl->transient_for);
          transient_for = get_popup_parent (transient_for);
        }

      if (!transient_for)
        {
          g_warning ("Couldn't map as window %p as popup because it doesn't have a parent",
                     window);

          create_fallback = TRUE;
        }
      else
        {
          grab_input_seat = find_grab_input_seat (window, transient_for);
        }

      if (!create_fallback)
        {
          gdk_wayland_window_create_xdg_popup (window,
                                               transient_for,
                                               grab_input_seat);
        }
      else
        {
          gdk_wayland_window_create_xdg_toplevel (window);
          gdk_wayland_window_announce_decoration_mode (window);
        }
    }
  else
    {
      gdk_wayland_window_create_xdg_toplevel (window);
      gdk_wayland_window_announce_decoration_mode (window);
    }

  impl->mapped = TRUE;
}

static void
gdk_wayland_window_show (GdkWindow *window,
                         gboolean   already_mapped)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (!impl->display_server.wl_surface)
    gdk_wayland_window_create_surface (window);

  gdk_wayland_window_map (window);

  _gdk_make_event (window, GDK_MAP, NULL, FALSE);

  if (impl->staging_cairo_surface &&
      _gdk_wayland_is_shm_surface (impl->staging_cairo_surface))
    gdk_wayland_window_attach_image (window);
}

static void
unmap_subsurface (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWindowImplWayland *parent_impl;

  g_return_if_fail (impl->display_server.wl_subsurface);
  g_return_if_fail (impl->transient_for);

  parent_impl = GDK_WINDOW_IMPL_WAYLAND (impl->transient_for->impl);
  wl_subsurface_destroy (impl->display_server.wl_subsurface);
  if (impl->parent_surface_committed_handler)
    {
      g_signal_handler_disconnect (parent_impl,
                                   impl->parent_surface_committed_handler);
      impl->parent_surface_committed_handler = 0;
    }
  impl->display_server.wl_subsurface = NULL;
}

static void
unmap_popups_for_window (GdkWindow *window)
{
  GdkWaylandDisplay *display_wayland;
  GList *l;

  display_wayland = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  for (l = display_wayland->current_popups; l; l = l->next)
    {
       GdkWindow *popup = l->data;
       GdkWindowImplWayland *popup_impl = GDK_WINDOW_IMPL_WAYLAND (popup->impl);

       if (popup_impl->popup_parent == window)
         {
           g_warning ("Tried to unmap the parent of a popup");
           gdk_window_hide (popup);

           return;
         }
    }
}

static void
gdk_wayland_window_hide_surface (GdkWindow *window)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  unmap_popups_for_window (window);

  if (impl->display_server.wl_surface)
    {
      if (impl->dummy_egl_surface)
        {
          eglDestroySurface (display_wayland->egl_display, impl->dummy_egl_surface);
          impl->dummy_egl_surface = NULL;
        }

      if (impl->display_server.dummy_egl_window)
        {
          wl_egl_window_destroy (impl->display_server.dummy_egl_window);
          impl->display_server.dummy_egl_window = NULL;
        }

      if (impl->egl_surface)
        {
          eglDestroySurface (display_wayland->egl_display, impl->egl_surface);
          impl->egl_surface = NULL;
        }

      if (impl->display_server.egl_window)
        {
          wl_egl_window_destroy (impl->display_server.egl_window);
          impl->display_server.egl_window = NULL;
        }

      if (impl->display_server.xdg_toplevel)
        {
          xdg_toplevel_destroy (impl->display_server.xdg_toplevel);
          impl->display_server.xdg_toplevel = NULL;
        }
      else if (impl->display_server.xdg_popup)
        {
          xdg_popup_destroy (impl->display_server.xdg_popup);
          impl->display_server.xdg_popup = NULL;
          display_wayland->current_popups =
            g_list_remove (display_wayland->current_popups, window);

          if (impl->position_method == POSITION_METHOD_MOVE_TO_RECT)
            {
              window->x = 0;
              window->y = 0;
            }
        }
      if (impl->display_server.xdg_surface)
        {
          xdg_surface_destroy (impl->display_server.xdg_surface);
          impl->display_server.xdg_surface = NULL;
          if (!impl->initial_configure_received)
            gdk_window_thaw_updates (window);
          else
            impl->initial_configure_received = FALSE;
        }

      if (impl->display_server.zxdg_toplevel_v6)
        {
          zxdg_toplevel_v6_destroy (impl->display_server.zxdg_toplevel_v6);
          impl->display_server.zxdg_toplevel_v6 = NULL;
        }
      else if (impl->display_server.zxdg_popup_v6)
        {
          zxdg_popup_v6_destroy (impl->display_server.zxdg_popup_v6);
          impl->display_server.zxdg_popup_v6 = NULL;
          display_wayland->current_popups =
            g_list_remove (display_wayland->current_popups, window);

          if (impl->position_method == POSITION_METHOD_MOVE_TO_RECT)
            {
              window->x = 0;
              window->y = 0;
            }
        }
      if (impl->display_server.zxdg_surface_v6)
        {
          zxdg_surface_v6_destroy (impl->display_server.zxdg_surface_v6);
          impl->display_server.zxdg_surface_v6 = NULL;
          if (!impl->initial_configure_received)
            gdk_window_thaw_updates (window);
          else
            impl->initial_configure_received = FALSE;
        }

      if (impl->display_server.wl_subsurface)
        unmap_subsurface (window);

      if (impl->awaiting_frame)
        {
          GdkFrameClock *frame_clock;

          impl->awaiting_frame = FALSE;
          frame_clock = gdk_window_get_frame_clock (window);
          if (frame_clock)
            _gdk_frame_clock_thaw (frame_clock);
        }

      if (impl->display_server.gtk_surface)
        {
          if (display_wayland->gtk_shell_version >=
              GTK_SURFACE1_RELEASE_SINCE_VERSION)
            gtk_surface1_release (impl->display_server.gtk_surface);
          else
            gtk_surface1_destroy (impl->display_server.gtk_surface);
          impl->display_server.gtk_surface = NULL;
          impl->application.was_set = FALSE;
        }

      if (impl->display_server.server_decoration)
        {
          org_kde_kwin_server_decoration_release (impl->display_server.server_decoration);
          impl->display_server.server_decoration = NULL;
        }

      wl_surface_destroy (impl->display_server.wl_surface);
      impl->display_server.wl_surface = NULL;
      impl->surface_callback = NULL;

      g_slist_free (impl->display_server.outputs);
      impl->display_server.outputs = NULL;

      if (impl->hint == GDK_WINDOW_TYPE_HINT_DIALOG && !impl->transient_for)
        display_wayland->orphan_dialogs =
          g_list_remove (display_wayland->orphan_dialogs, window);
    }

  unset_transient_for_exported (window);

  _gdk_wayland_window_clear_saved_size (window);
  drop_cairo_surfaces (window);
  impl->pending_commit = FALSE;
  impl->mapped = FALSE;
}

static void
gdk_wayland_window_hide (GdkWindow *window)
{
  gdk_wayland_window_hide_surface (window);
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

      gdk_wayland_window_hide_surface (window);
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
}

static void
gdk_window_wayland_lower (GdkWindow *window)
{
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
gdk_window_request_transient_parent_commit (GdkWindow *window)
{
  GdkWindowImplWayland *window_impl, *impl;
  GdkFrameClock *frame_clock;

  window_impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (!window_impl->transient_for)
    return;

  impl = GDK_WINDOW_IMPL_WAYLAND (window_impl->transient_for->impl);

  if (!impl->display_server.wl_surface || impl->pending_commit)
    return;

  frame_clock = gdk_window_get_frame_clock (window_impl->transient_for);

  if (!frame_clock)
    return;

  impl->pending_commit = TRUE;
  gdk_frame_clock_request_phase (frame_clock,
                                 GDK_FRAME_CLOCK_PHASE_AFTER_PAINT);
}

static void
gdk_window_wayland_move_resize (GdkWindow *window,
                                gboolean   with_move,
                                gint       x,
                                gint       y,
                                gint       width,
                                gint       height)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (with_move)
    {
      /* Each toplevel has in its own "root" coordinate system */
      if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_TOPLEVEL)
        {
          window->x = x;
          window->y = y;
          impl->position_method = POSITION_METHOD_MOVE_RESIZE;

          if (impl->display_server.wl_subsurface &&
	      (x + window->abs_x != impl->subsurface_x ||
	       y + window->abs_y != impl->subsurface_y))
            {
              gdk_wayland_window_set_subsurface_position (window,
                                                          x + window->abs_x,
                                                          y + window->abs_y);
            }
        }
    }

  if (window->state & (GDK_WINDOW_STATE_FULLSCREEN |
                       GDK_WINDOW_STATE_MAXIMIZED |
                       GDK_WINDOW_STATE_TILED))
    {
      impl->saved_width = width;
      impl->saved_height = height;
      impl->saved_size_changed = (width > 0 && height > 0);
    }

  /* If this function is called with width and height = -1 then that means
   * just move the window - don't update its size
   */
  if (width > 0 && height > 0)
    {
      if (!should_use_fixed_size (window->state) ||
          (width == impl->fixed_size_width &&
           height == impl->fixed_size_height))
        {
          gdk_wayland_window_maybe_configure (window,
                                              width,
                                              height,
                                              impl->scale);
        }
      else if (!should_inhibit_resize (window))
        {
          gdk_wayland_window_configure (window,
                                        window->width,
                                        window->height,
                                        impl->scale);
        }
    }
}

/* Avoid zero width/height as this is a protocol error */
static void
sanitize_anchor_rect (GdkWindow    *window,
                      GdkRectangle *rect)
{
  gint original_width = rect->width;
  gint original_height = rect->height;

  rect->width  = MAX (1, rect->width);
  rect->height = MAX (1, rect->height);
  rect->x = MAX (rect->x + original_width - rect->width, 0);
  rect->y = MAX (rect->y + original_height - rect->height, 0);
}

static void
gdk_window_wayland_move_to_rect (GdkWindow          *window,
                                 const GdkRectangle *rect,
                                 GdkGravity          rect_anchor,
                                 GdkGravity          window_anchor,
                                 GdkAnchorHints      anchor_hints,
                                 gint                rect_anchor_dx,
                                 gint                rect_anchor_dy)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  impl->pending_move_to_rect.rect = *rect;
  sanitize_anchor_rect (window, &impl->pending_move_to_rect.rect);

  impl->pending_move_to_rect.rect_anchor = rect_anchor;
  impl->pending_move_to_rect.window_anchor = window_anchor;
  impl->pending_move_to_rect.anchor_hints = anchor_hints;
  impl->pending_move_to_rect.rect_anchor_dx = rect_anchor_dx;
  impl->pending_move_to_rect.rect_anchor_dy = rect_anchor_dy;

  impl->position_method = POSITION_METHOD_MOVE_TO_RECT;
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

static void
gdk_window_wayland_get_root_coords (GdkWindow *window,
                                    gint       x,
                                    gint       y,
                                    gint      *root_x,
                                    gint      *root_y)
{
  /*
   * Wayland does not have a global coordinate space shared between surfaces. In
   * fact, for regular toplevels, we have no idea where our surfaces are
   * positioned, relatively.
   *
   * However, there are some cases like popups and subsurfaces where we do have
   * some amount of control over the placement of our window, and we can
   * semi-accurately control the x/y position of these windows, if they are
   * relative to another surface.
   *
   * To pretend we have something called a root coordinate space, assume all
   * parent-less windows are positioned in (0, 0), and all relative positioned
   * popups and subsurfaces are placed within this fake root coordinate space.
   *
   * For example a 200x200 large toplevel window will have the position (0, 0).
   * If a popup positioned in the middle of the toplevel will have the fake
   * position (100,100). Furthermore, if a positioned is placed in the middle
   * that popup, will have the fake position (150,150), even though it has the
   * relative position (50,50). These three windows would make up one single
   * fake root coordinate space.
   */

  if (root_x)
    *root_x = window->x + x;

  if (root_y)
    *root_y = window->y + y;
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
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  g_clear_pointer (&impl->input_region, cairo_region_destroy);

  if (shape_region)
    {
      impl->input_region = cairo_region_copy (shape_region);
      cairo_region_translate (impl->input_region, offset_x, offset_y);
    }

  impl->input_region_dirty = TRUE;
}

static void
gdk_wayland_window_destroy (GdkWindow *window,
                            gboolean   recursing,
                            gboolean   foreign_destroy)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* Wayland windows can't be externally destroyed; we may possibly
   * eventually want to use this path at display close-down
   */
  g_return_if_fail (!foreign_destroy);

  gdk_wayland_window_hide_surface (window);
}

static void
gdk_window_wayland_destroy_foreign (GdkWindow *window)
{
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

#ifdef HAVE_XDG_ACTIVATION
static void
token_done (gpointer                        data,
            struct xdg_activation_token_v1 *provider,
            const char                     *token)
{
  char **token_out = data;

  *token_out = g_strdup (token);
}

static const struct xdg_activation_token_v1_listener token_listener = {
  token_done,
};
#endif

static void
gdk_wayland_window_focus (GdkWindow *window,
                          guint32    timestamp)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkDisplay *display = gdk_window_get_display (window);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  gchar *startup_id = NULL;

  startup_id = g_steal_pointer (&display_wayland->startup_notification_id);

#ifdef HAVE_XDG_ACTIVATION
  if (display_wayland->xdg_activation)
    {
      GdkSeat *seat = gdk_display_get_default_seat (display);

      /* If the focus request does not have a startup ID associated, get a
       * new token to activate the window.
       */
      if (!startup_id)
        {
          struct xdg_activation_token_v1 *token;
          struct wl_event_queue *event_queue;
          struct wl_surface *wl_surface = NULL;
          GdkWindow *focus_window;

          event_queue = wl_display_create_queue (display_wayland->wl_display);

          token = xdg_activation_v1_get_activation_token (display_wayland->xdg_activation);
          wl_proxy_set_queue ((struct wl_proxy *) token, event_queue);

          xdg_activation_token_v1_add_listener (token,
                                                &token_listener,
                                                &startup_id);
          xdg_activation_token_v1_set_serial (token,
                                              _gdk_wayland_seat_get_last_implicit_grab_serial (seat, NULL),
                                              gdk_wayland_seat_get_wl_seat (seat));

          focus_window = gdk_wayland_device_get_focus (gdk_seat_get_keyboard (seat));
          if (focus_window)
            wl_surface = gdk_wayland_window_get_wl_surface (focus_window);
          if (wl_surface)
            xdg_activation_token_v1_set_surface (token, wl_surface);

          xdg_activation_token_v1_commit (token);

          while (startup_id == NULL)
            wl_display_dispatch_queue (display_wayland->wl_display, event_queue);

          xdg_activation_token_v1_destroy (token);
          wl_event_queue_destroy (event_queue);
        }

      xdg_activation_v1_activate (display_wayland->xdg_activation,
                                  startup_id,
                                  impl->display_server.wl_surface);
    }
  else
#endif
  if (impl->display_server.gtk_surface)
    {
      if (timestamp != GDK_CURRENT_TIME)
        gtk_surface1_present (impl->display_server.gtk_surface, timestamp);
      else if (startup_id && display_wayland->gtk_shell_version >= 3)
        gtk_surface1_request_focus (impl->display_server.gtk_surface,
                                    startup_id);
    }

  g_free (startup_id);
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
}

static GdkWindowTypeHint
gdk_wayland_window_get_type_hint (GdkWindow *window)
{
  GdkWindowImplWayland *impl;

  if (GDK_WINDOW_DESTROYED (window))
    return GDK_WINDOW_TYPE_HINT_NORMAL;

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  return impl->hint;
}

static void
gtk_surface_configure (void                *data,
                       struct gtk_surface1 *gtk_surface,
                       struct wl_array     *states)
{
  GdkWindow *window = GDK_WINDOW (data);
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWindowState new_state = 0;
  uint32_t *p;

  wl_array_for_each (p, states)
    {
      uint32_t state = *p;

      switch (state)
        {
        case GTK_SURFACE1_STATE_TILED:
          new_state |= GDK_WINDOW_STATE_TILED;
          break;

        /* Since v2 */
        case GTK_SURFACE1_STATE_TILED_TOP:
          new_state |= (GDK_WINDOW_STATE_TILED | GDK_WINDOW_STATE_TOP_TILED);
          break;
        case GTK_SURFACE1_STATE_TILED_RIGHT:
          new_state |= (GDK_WINDOW_STATE_TILED | GDK_WINDOW_STATE_RIGHT_TILED);
          break;
        case GTK_SURFACE1_STATE_TILED_BOTTOM:
          new_state |= (GDK_WINDOW_STATE_TILED | GDK_WINDOW_STATE_BOTTOM_TILED);
          break;
        case GTK_SURFACE1_STATE_TILED_LEFT:
          new_state |= (GDK_WINDOW_STATE_TILED | GDK_WINDOW_STATE_LEFT_TILED);
          break;
        default:
          /* Unknown state */
          break;
        }
    }

  impl->pending.state |= new_state;
}

static void
gtk_surface_configure_edges (void                *data,
                             struct gtk_surface1 *gtk_surface,
                             struct wl_array     *edge_constraints)
{
  GdkWindow *window = GDK_WINDOW (data);
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWindowState new_state = 0;
  uint32_t *p;

  wl_array_for_each (p, edge_constraints)
    {
      uint32_t constraint = *p;

      switch (constraint)
        {
        case GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_TOP:
          new_state |= GDK_WINDOW_STATE_TOP_RESIZABLE;
          break;
        case GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_RIGHT:
          new_state |= GDK_WINDOW_STATE_RIGHT_RESIZABLE;
          break;
        case GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_BOTTOM:
          new_state |= GDK_WINDOW_STATE_BOTTOM_RESIZABLE;
          break;
        case GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_LEFT:
          new_state |= GDK_WINDOW_STATE_LEFT_RESIZABLE;
          break;
        default:
          /* Unknown state */
          break;
        }
    }

  impl->pending.state |= new_state;
}

static const struct gtk_surface1_listener gtk_surface_listener = {
  gtk_surface_configure,
  gtk_surface_configure_edges
};

static void
gdk_wayland_window_init_gtk_surface (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandDisplay *display =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));

  if (impl->display_server.gtk_surface != NULL)
    return;
  if (!is_realized_toplevel (window))
    return;
  if (display->gtk_shell == NULL)
    return;

  impl->display_server.gtk_surface =
    gtk_shell1_get_gtk_surface (display->gtk_shell,
                                impl->display_server.wl_surface);
  gdk_window_set_geometry_hints (window,
                                 &impl->geometry_hints,
                                 impl->geometry_mask);
  gtk_surface1_add_listener (impl->display_server.gtk_surface,
                             &gtk_surface_listener,
                             window);
}

static void
maybe_set_gtk_surface_modal (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  gdk_wayland_window_init_gtk_surface (window);
  if (impl->display_server.gtk_surface == NULL)
    return;

  if (window->modal_hint)
    gtk_surface1_set_modal (impl->display_server.gtk_surface);
  else
    gtk_surface1_unset_modal (impl->display_server.gtk_surface);

}

static void
gdk_wayland_window_set_modal_hint (GdkWindow *window,
                                   gboolean   modal)
{
  window->modal_hint = modal;
  maybe_set_gtk_surface_modal (window);
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
  GdkWaylandDisplay *display_wayland;
  GdkWindowImplWayland *impl;
  int min_width = 0, min_height = 0, max_width = 0, max_height = 0;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  display_wayland = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));

  impl->geometry_hints = *geometry;
  impl->geometry_mask = geom_mask;

  if (!is_realized_toplevel (window))
    return;

  if (geom_mask & GDK_HINT_MIN_SIZE)
    {
      min_width =
        MAX (0, calculate_width_without_margin (window, geometry->min_width));
      min_height =
        MAX (0, calculate_height_without_margin (window, geometry->min_height));
    }

  if (geom_mask & GDK_HINT_MAX_SIZE)
    {
      max_width =
        MAX (0, calculate_width_without_margin (window, geometry->max_width));
      max_height =
        MAX (0, calculate_height_without_margin (window, geometry->max_height));
    }

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_set_min_size (impl->display_server.xdg_toplevel,
                                 min_width, min_height);
      xdg_toplevel_set_max_size (impl->display_server.xdg_toplevel,
                                 max_width, max_height);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_set_min_size (impl->display_server.zxdg_toplevel_v6,
                                     min_width, min_height);
      zxdg_toplevel_v6_set_max_size (impl->display_server.zxdg_toplevel_v6,
                                     max_width, max_height);
      break;
    }
}

static void
gdk_wayland_window_set_title (GdkWindow   *window,
                              const gchar *title)
{
  GdkWindowImplWayland *impl;
  const char *end;
  g_return_if_fail (title != NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (g_strcmp0 (impl->title, title) == 0)
    return;

  g_free (impl->title);

  g_utf8_validate (title, MAX_WL_BUFFER_SIZE, &end);
  impl->title = g_malloc (end - title + 1);
  memcpy (impl->title, title, end - title);
  impl->title[end - title] = '\0';

  gdk_wayland_window_sync_title (window);
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
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkDisplay *display = gdk_window_get_display (window);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  gchar *free_me = NULL;

  if (!startup_id)
    {
      free_me = g_steal_pointer (&display_wayland->startup_notification_id);
      startup_id = free_me;
    }

#ifdef HAVE_XDG_ACTIVATION
  if (display_wayland->xdg_activation && startup_id)
    {
      xdg_activation_v1_activate (display_wayland->xdg_activation,
                                  startup_id,
				  impl->display_server.wl_surface);
    }
#endif

  g_free (free_me);
}

static gboolean
check_transient_for_loop (GdkWindow *window,
                          GdkWindow *parent)
{
  while (parent)
    {
      GdkWindowImplWayland *impl;

      if (!GDK_IS_WINDOW_IMPL_WAYLAND(parent->impl))
        return FALSE;

      impl = GDK_WINDOW_IMPL_WAYLAND (parent->impl);
      if (impl->transient_for == window)
        return TRUE;
      parent = impl->transient_for;
    }
  return FALSE;
}

static void
gdk_wayland_window_set_transient_for (GdkWindow *window,
                                      GdkWindow *parent)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  GdkWindow *previous_parent;
  gboolean was_subsurface = FALSE;

  g_assert (parent == NULL ||
            gdk_window_get_display (window) == gdk_window_get_display (parent));

  if (check_transient_for_loop (window, parent))
    {
      g_warning ("Setting %p transient for %p would create a loop", window, parent);
      return;
    }

  unset_transient_for_exported (window);

  if (impl->display_server.wl_subsurface)
    {
      was_subsurface = TRUE;
      unmap_subsurface (window);
    }

  previous_parent = impl->transient_for;
  impl->transient_for = parent;

  if (impl->hint == GDK_WINDOW_TYPE_HINT_DIALOG)
    {
      if (!parent)
        _gdk_wayland_screen_add_orphan_dialog (window);
      else if (!previous_parent)
        display_wayland->orphan_dialogs =
          g_list_remove (display_wayland->orphan_dialogs, window);
    }

  gdk_wayland_window_sync_parent (window, NULL);

  if (was_subsurface && parent)
    gdk_wayland_window_create_subsurface (window);
}

static void
gdk_wayland_window_get_frame_extents (GdkWindow    *window,
                                      GdkRectangle *rect)
{
  *rect = (GdkRectangle) {
    .x = window->x,
    .y = window->y,
    .width = window->width,
    .height = window->height
  };
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
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandDisplay *display_wayland;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  if (!is_realized_toplevel (window))
    return;

  display_wayland = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_set_minimized (impl->display_server.xdg_toplevel);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_set_minimized (impl->display_server.zxdg_toplevel_v6);
      break;
    }
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
}

static void
gdk_wayland_window_unstick (GdkWindow *window)
{
}

static void
gdk_wayland_window_maximize (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  _gdk_wayland_window_save_size (window);
  if (is_realized_toplevel (window))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_set_maximized (impl->display_server.xdg_toplevel);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_set_maximized (impl->display_server.zxdg_toplevel_v6);
          break;
        }
    }
  else
    {
      gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_MAXIMIZED);
    }
}

static void
gdk_wayland_window_unmaximize (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (is_realized_toplevel (window))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_unset_maximized (impl->display_server.xdg_toplevel);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_unset_maximized (impl->display_server.zxdg_toplevel_v6);
          break;
        }
    }
  else
    {
      gdk_synthesize_window_state (window, GDK_WINDOW_STATE_MAXIMIZED, 0);
    }
}

static void
gdk_wayland_window_fullscreen_on_monitor (GdkWindow *window, gint monitor)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkScreen *screen = gdk_window_get_screen (window);
  struct wl_output *fullscreen_output = 
    _gdk_wayland_screen_get_wl_output (screen, monitor);
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  _gdk_wayland_window_save_size (window);
  if (is_realized_toplevel (window))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_set_fullscreen (impl->display_server.xdg_toplevel,
                                       fullscreen_output);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_set_fullscreen (impl->display_server.zxdg_toplevel_v6,
                                           fullscreen_output);
          break;
        }
    }
  else
    {
      gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_FULLSCREEN);
      impl->initial_fullscreen_monitor = monitor;
    }
}

static void
gdk_wayland_window_fullscreen (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl->initial_fullscreen_monitor = -1;

  _gdk_wayland_window_save_size (window);
  if (is_realized_toplevel (window))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_set_fullscreen (impl->display_server.xdg_toplevel,
                                       NULL);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_set_fullscreen (impl->display_server.zxdg_toplevel_v6,
                                           NULL);
          break;
        }
    }
  else
    {
      gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_FULLSCREEN);
    }
}

static void
gdk_wayland_window_unfullscreen (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl->initial_fullscreen_monitor = -1;

  if (is_realized_toplevel (window))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_unset_fullscreen (impl->display_server.xdg_toplevel);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_unset_fullscreen (impl->display_server.zxdg_toplevel_v6);
          break;
        }
    }
  else
    {
      gdk_synthesize_window_state (window, GDK_WINDOW_STATE_FULLSCREEN, 0);
    }
}

static void
gdk_wayland_window_set_keep_above (GdkWindow *window, gboolean setting)
{
}

static void
gdk_wayland_window_set_keep_below (GdkWindow *window, gboolean setting)
{
}

static GdkWindow *
gdk_wayland_window_get_group (GdkWindow *window)
{
  return NULL;
}

static void
gdk_wayland_window_set_group (GdkWindow *window,
                              GdkWindow *leader)
{
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
  GdkWaylandDisplay *display_wayland;
  GdkEventSequence *sequence;
  uint32_t resize_edges, serial;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  switch (edge)
    {
    case GDK_WINDOW_EDGE_NORTH_WEST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_LEFT;
      break;

    case GDK_WINDOW_EDGE_NORTH:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP;
      break;

    case GDK_WINDOW_EDGE_NORTH_EAST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_RIGHT;
      break;

    case GDK_WINDOW_EDGE_WEST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_LEFT;
      break;

    case GDK_WINDOW_EDGE_EAST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_RIGHT;
      break;

    case GDK_WINDOW_EDGE_SOUTH_WEST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_LEFT;
      break;

    case GDK_WINDOW_EDGE_SOUTH:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM;
      break;

    case GDK_WINDOW_EDGE_SOUTH_EAST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_RIGHT;
      break;

    default:
      g_warning ("gdk_window_begin_resize_drag: bad resize edge %d!", edge);
      return;
    }

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  display_wayland = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));

  if (!is_realized_toplevel (window))
    return;

  serial = _gdk_wayland_seat_get_last_implicit_grab_serial (gdk_device_get_seat (device),
                                                            &sequence);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_resize (impl->display_server.xdg_toplevel,
                           gdk_wayland_device_get_wl_seat (device),
                           serial, resize_edges);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_resize (impl->display_server.zxdg_toplevel_v6,
                               gdk_wayland_device_get_wl_seat (device),
                               serial, resize_edges);
      break;
    }

  if (sequence)
    gdk_wayland_device_unset_touch_grab (device, sequence);

  /* This is needed since Wayland will absorb all the pointer events after the
   * above function - FIXME: Is this always safe..?
   */
  gdk_seat_ungrab (gdk_device_get_seat (device));
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
  GdkWaylandDisplay *display_wayland;
  GdkEventSequence *sequence;
  uint32_t serial;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  display_wayland = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));

  if (!is_realized_toplevel (window))
    return;

  serial = _gdk_wayland_seat_get_last_implicit_grab_serial (gdk_device_get_seat (device),
                                                            &sequence);
  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_move (impl->display_server.xdg_toplevel,
                         gdk_wayland_device_get_wl_seat (device),
                         serial);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_move (impl->display_server.zxdg_toplevel_v6,
                             gdk_wayland_device_get_wl_seat (device),
                             serial);
      break;
    }

  if (sequence)
    gdk_wayland_device_unset_touch_grab (device, sequence);

  /* This is needed since Wayland will absorb all the pointer events after the
   * above function - FIXME: Is this always safe..?
   */
  gdk_seat_ungrab (gdk_device_get_seat (device));
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
  if (property == gdk_atom_intern_static_string ("GDK_SELECTION"))
    gdk_wayland_selection_store (window, type, mode, data, nelements * (format / 8));
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

static void
gdk_wayland_window_set_opaque_region (GdkWindow      *window,
                                      cairo_region_t *region)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  g_clear_pointer (&impl->opaque_region, cairo_region_destroy);
  impl->opaque_region = cairo_region_reference (region);
  impl->opaque_region_dirty = TRUE;
}

static void
gdk_wayland_window_set_shadow_width (GdkWindow *window,
                                     int        left,
                                     int        right,
                                     int        top,
                                     int        bottom)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  gint new_width, new_height;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* Reconfigure window to keep the same window geometry */
  new_width = (calculate_width_without_margin (window, window->width) +
               (left + right));
  new_height = (calculate_height_without_margin (window, window->height) +
                (top + bottom));
  gdk_wayland_window_maybe_configure (window, new_width, new_height, impl->scale);

  impl->margin_left = left;
  impl->margin_right = right;
  impl->margin_top = top;
  impl->margin_bottom = bottom;
}

static gboolean
gdk_wayland_window_show_window_menu (GdkWindow *window,
                                     GdkEvent  *event)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  struct wl_seat *seat;
  GdkWaylandDevice *device;
  GdkWindow *event_window;
  double x, y;
  uint32_t serial;

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_END:
      break;
    default:
      return FALSE;
    }

  if (!is_realized_toplevel (window))
    return FALSE;

  device = GDK_WAYLAND_DEVICE (gdk_event_get_device (event));
  seat = gdk_wayland_device_get_wl_seat (GDK_DEVICE (device));

  gdk_event_get_coords (event, &x, &y);
  event_window = gdk_event_get_window (event);
  while (gdk_window_get_window_type (event_window) != GDK_WINDOW_TOPLEVEL)
    {
      gdk_window_coords_to_parent (event_window, x, y, &x, &y);
      event_window = gdk_window_get_effective_parent (event_window);
    }

  serial = _gdk_wayland_device_get_implicit_grab_serial (device, event);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_show_window_menu (impl->display_server.xdg_toplevel,
                                     seat, serial, x, y);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_show_window_menu (impl->display_server.zxdg_toplevel_v6,
                                         seat, serial, x, y);
      break;
    }

  return TRUE;
}

static gboolean
translate_gesture (GdkTitlebarGesture         gesture,
                   enum gtk_surface1_gesture *out_gesture)
{
  switch (gesture)
    {
    case GDK_TITLEBAR_GESTURE_DOUBLE_CLICK:
      *out_gesture = GTK_SURFACE1_GESTURE_DOUBLE_CLICK;
      break;

    case GDK_TITLEBAR_GESTURE_RIGHT_CLICK:
      *out_gesture = GTK_SURFACE1_GESTURE_RIGHT_CLICK;
      break;

    case GDK_TITLEBAR_GESTURE_MIDDLE_CLICK:
      *out_gesture = GTK_SURFACE1_GESTURE_MIDDLE_CLICK;
      break;

    default:
      g_warning ("Not handling unknown titlebar gesture %u", gesture);
      return FALSE;
    }

  return TRUE;
}

static gboolean
gdk_wayland_window_titlebar_gesture (GdkWindow          *window,
                                     GdkTitlebarGesture  gesture)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  struct gtk_surface1 *gtk_surface = impl->display_server.gtk_surface;
  enum gtk_surface1_gesture gtk_gesture;
  GdkSeat *seat;
  struct wl_seat *wl_seat;
  uint32_t serial;

  if (!gtk_surface)
    return FALSE;

  if (gtk_surface1_get_version (gtk_surface) < GTK_SURFACE1_TITLEBAR_GESTURE_SINCE_VERSION)
    return FALSE;

  if (!translate_gesture (gesture, &gtk_gesture))
    return FALSE;

  seat = gdk_display_get_default_seat (gdk_window_get_display (window));
  wl_seat = gdk_wayland_seat_get_wl_seat (seat);

  serial = _gdk_wayland_seat_get_last_implicit_grab_serial (seat, NULL);

  gtk_surface1_titlebar_gesture (impl->display_server.gtk_surface,
                                 serial,
                                 wl_seat,
                                 gtk_gesture);

  return TRUE;
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
  impl_class->move_to_rect = gdk_window_wayland_move_to_rect;
  impl_class->set_background = gdk_window_wayland_set_background;
  impl_class->reparent = gdk_window_wayland_reparent;
  impl_class->set_device_cursor = gdk_window_wayland_set_device_cursor;
  impl_class->get_geometry = gdk_window_wayland_get_geometry;
  impl_class->get_root_coords = gdk_window_wayland_get_root_coords;
  impl_class->get_device_state = gdk_window_wayland_get_device_state;
  impl_class->shape_combine_region = gdk_window_wayland_shape_combine_region;
  impl_class->input_shape_combine_region = gdk_window_wayland_input_shape_combine_region;
  impl_class->destroy = gdk_wayland_window_destroy;
  impl_class->destroy_foreign = gdk_window_wayland_destroy_foreign;
  impl_class->get_shape = gdk_wayland_window_get_shape;
  impl_class->get_input_shape = gdk_wayland_window_get_input_shape;
  impl_class->begin_paint = gdk_window_impl_wayland_begin_paint;
  impl_class->end_paint = gdk_window_impl_wayland_end_paint;
  impl_class->beep = gdk_window_impl_wayland_beep;

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
  impl_class->fullscreen_on_monitor = gdk_wayland_window_fullscreen_on_monitor;
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
  impl_class->sync_rendering = gdk_wayland_window_sync_rendering;
  impl_class->simulate_key = gdk_wayland_window_simulate_key;
  impl_class->simulate_button = gdk_wayland_window_simulate_button;
  impl_class->get_property = gdk_wayland_window_get_property;
  impl_class->change_property = gdk_wayland_window_change_property;
  impl_class->delete_property = gdk_wayland_window_delete_property;
  impl_class->get_scale_factor = gdk_wayland_window_get_scale_factor;
  impl_class->set_opaque_region = gdk_wayland_window_set_opaque_region;
  impl_class->set_shadow_width = gdk_wayland_window_set_shadow_width;
  impl_class->show_window_menu = gdk_wayland_window_show_window_menu;
  impl_class->create_gl_context = gdk_wayland_window_create_gl_context;
  impl_class->invalidate_for_new_frame = gdk_wayland_window_invalidate_for_new_frame;
  impl_class->titlebar_gesture = gdk_wayland_window_titlebar_gesture;

  signals[COMMITTED] = g_signal_new ("committed",
                                     G_TYPE_FROM_CLASS (object_class),
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL, NULL,
                                     G_TYPE_NONE, 0);
}

void
_gdk_wayland_window_set_grab_seat (GdkWindow *window,
                                   GdkSeat   *seat)
{
  GdkWindowImplWayland *impl;

  g_return_if_fail (window != NULL);

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  impl->grab_input_seat = seat;
}

/**
 * gdk_wayland_window_get_wl_surface:
 * @window: (type GdkWaylandWindow): a #GdkWindow
 *
 * Returns the Wayland surface of a #GdkWindow.
 *
 * Returns: (transfer none): a Wayland wl_surface
 *
 * Since: 3.8
 */
struct wl_surface *
gdk_wayland_window_get_wl_surface (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_WINDOW (window), NULL);

  return GDK_WINDOW_IMPL_WAYLAND (window->impl)->display_server.wl_surface;
}

struct wl_output *
gdk_wayland_window_get_wl_output (GdkWindow *window)
{
  GdkWindowImplWayland *impl;

  g_return_val_if_fail (GDK_IS_WAYLAND_WINDOW (window), NULL);

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  /* We pick the head of the list as this is the last entered output */
  if (impl->display_server.outputs)
    return (struct wl_output *) impl->display_server.outputs->data;

  return NULL;
}

static struct wl_egl_window *
gdk_wayland_window_get_wl_egl_window (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (impl->display_server.egl_window == NULL)
    {
      impl->display_server.egl_window =
        wl_egl_window_create (impl->display_server.wl_surface,
                              impl->wrapper->width * impl->scale,
                              impl->wrapper->height * impl->scale);
      wl_surface_set_buffer_scale (impl->display_server.wl_surface, impl->scale);
    }

  return impl->display_server.egl_window;
}

EGLSurface
gdk_wayland_window_get_egl_surface (GdkWindow *window,
                                    EGLConfig  config)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  GdkWindowImplWayland *impl;
  struct wl_egl_window *egl_window;

  g_return_val_if_fail (GDK_IS_WAYLAND_WINDOW (window), NULL);

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (impl->egl_surface == NULL)
    {
      egl_window = gdk_wayland_window_get_wl_egl_window (window);

      impl->egl_surface =
        eglCreateWindowSurface (display->egl_display, config, egl_window, NULL);
    }

  return impl->egl_surface;
}

EGLSurface
gdk_wayland_window_get_dummy_egl_surface (GdkWindow *window,
                                          EGLConfig  config)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  GdkWindowImplWayland *impl;

  g_return_val_if_fail (GDK_IS_WAYLAND_WINDOW (window), NULL);

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (impl->dummy_egl_surface == NULL)
    {
      impl->display_server.dummy_egl_window =
        wl_egl_window_create (impl->display_server.wl_surface, 1, 1);

      impl->dummy_egl_surface =
        eglCreateWindowSurface (display->egl_display, config, impl->display_server.dummy_egl_window, NULL);
    }

  return impl->dummy_egl_surface;
}

struct gtk_surface1 *
gdk_wayland_window_get_gtk_surface (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_WINDOW (window), NULL);

  return GDK_WINDOW_IMPL_WAYLAND (window->impl)->display_server.gtk_surface;
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
 * process. In this case the underlying surface isn’t an xdg_shell
 * surface and the panel or OSK client need to identify the wl_surface
 * as a panel or OSK to the compositor. The assumption is that the
 * compositor will expose a private interface to the special client
 * that lets the client identify the wl_surface as a panel or such.
 *
 * This function should be called before a #GdkWindow is shown. This is
 * best done by connecting to the #GtkWidget::realize signal:
 *
 * |[<!-- language="C" -->
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
 * ]|
 *
 * Since: 3.10
 */
void
gdk_wayland_window_set_use_custom_surface (GdkWindow *window)
{
  GdkWindowImplWayland *impl;

  g_return_if_fail (GDK_IS_WAYLAND_WINDOW (window));

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (!impl->display_server.wl_surface)
    gdk_wayland_window_create_surface (window);

  impl->use_custom_surface = TRUE;
}

static void
maybe_set_gtk_surface_dbus_properties (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (impl->application.was_set)
    return;

  if (impl->application.application_id == NULL &&
      impl->application.app_menu_path == NULL &&
      impl->application.menubar_path == NULL &&
      impl->application.window_object_path == NULL &&
      impl->application.application_object_path == NULL &&
      impl->application.unique_bus_name == NULL)
    return;

  gdk_wayland_window_init_gtk_surface (window);
  if (impl->display_server.gtk_surface == NULL)
    return;

  gtk_surface1_set_dbus_properties (impl->display_server.gtk_surface,
                                    impl->application.application_id,
                                    impl->application.app_menu_path,
                                    impl->application.menubar_path,
                                    impl->application.window_object_path,
                                    impl->application.application_object_path,
                                    impl->application.unique_bus_name);
  impl->application.was_set = TRUE;
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

  impl->application.application_id = g_strdup (application_id);
  impl->application.app_menu_path = g_strdup (app_menu_path);
  impl->application.menubar_path = g_strdup (menubar_path);
  impl->application.window_object_path = g_strdup (window_object_path);
  impl->application.application_object_path =
    g_strdup (application_object_path);
  impl->application.unique_bus_name = g_strdup (unique_bus_name);

  maybe_set_gtk_surface_dbus_properties (window);
}

void
_gdk_wayland_window_offset_next_wl_buffer (GdkWindow *window,
                                           int        x,
                                           int        y)
{
  GdkWindowImplWayland *impl;

  g_return_if_fail (GDK_IS_WAYLAND_WINDOW (window));

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  impl->pending_buffer_offset_x = x;
  impl->pending_buffer_offset_y = y;
}

static void
invoke_exported_closures (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GList *l;

  for (l = impl->exported.closures; l; l = l->next)
    {
      ExportedClosure *closure = l->data;

      closure->callback (window, impl->exported.handle, closure->user_data);

      if (closure->destroy_func)
        closure->destroy_func (closure->user_data);
    }

  g_list_free_full (impl->exported.closures, g_free);
  impl->exported.closures = NULL;
}

static void
xdg_exported_v1_handle (void                    *data,
                        struct zxdg_exported_v1 *zxdg_exported_v1,
                        const char              *handle)
{
  GdkWindow *window = data;
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  impl->exported.handle = g_strdup (handle);

  invoke_exported_closures (window);
}

static const struct zxdg_exported_v1_listener xdg_exported_v1_listener = {
  xdg_exported_v1_handle
};

static void
xdg_exported_v2_handle (void                    *data,
                        struct zxdg_exported_v2 *zxdg_exported_v2,
                        const char              *handle)
{
  xdg_exported_v1_handle(data, NULL, handle);
}

static const struct zxdg_exported_v2_listener xdg_exported_v2_listener = {
  xdg_exported_v2_handle
};

/**
 * GdkWaylandWindowExported:
 * @window: the #GdkWindow that is exported
 * @handle: the handle
 * @user_data: user data that was passed to gdk_wayland_window_export_handle()
 *
 * Callback that gets called when the handle for a window has been
 * obtained from the Wayland compositor. The handle can be passed
 * to other processes, for the purpose of marking windows as transient
 * for out-of-process surfaces.
 *
 * Since: 3.22
 */

static gboolean
gdk_wayland_window_is_exported (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  return impl->display_server.xdg_exported_v2 || impl->display_server.xdg_exported_v1;
}

static gboolean
exported_idle (gpointer user_data)
{
  GdkWindow *window = user_data;
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  invoke_exported_closures (window);

  impl->exported.idle_source_id = 0;

  return G_SOURCE_REMOVE;
}

/**
 * gdk_wayland_window_export_handle:
 * @window: the #GdkWindow to obtain a handle for
 * @callback: callback to call with the handle
 * @user_data: user data for @callback
 * @destroy_func: destroy notify for @user_data
 *
 * Asynchronously obtains a handle for a surface that can be passed
 * to other processes. When the handle has been obtained, @callback
 * will be called.
 *
 * Up until 3.22.15 it was an error to call this function on a window that is
 * already exported. When the handle is no longer needed,
 * gdk_wayland_window_unexport_handle() should be called to clean up resources.
 *
 * Starting with 3.22.16, calling this function on an already exported window
 * will cause the callback to be invoked with the same handle as was already
 * invoked, from an idle callback. To unexport the window,
 * gdk_wayland_window_unexport_handle() must be called the same number of times
 * as gdk_wayland_window_export_handle() was called. Any 'exported' callback
 * may still be invoked until the window is unexported or destroyed.
 *
 * The main purpose for obtaining a handle is to mark a surface
 * from another window as transient for this one, see
 * gdk_wayland_window_set_transient_for_exported().
 *
 * Note that this API depends on an unstable Wayland protocol,
 * and thus may require changes in the future.
 *
 * Return value: %TRUE if the handle has been requested, %FALSE if
 *     an error occurred.
 *
 * Since: 3.22
 */
gboolean
gdk_wayland_window_export_handle (GdkWindow                *window,
                                  GdkWaylandWindowExported  callback,
                                  gpointer                  user_data,
                                  GDestroyNotify            destroy_func)
{
  GdkWindowImplWayland *impl;
  GdkWaylandDisplay *display_wayland;
  GdkDisplay *display = gdk_window_get_display (window);
  ExportedClosure *closure;

  g_return_val_if_fail (GDK_IS_WAYLAND_WINDOW (window), FALSE);
  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY (display), FALSE);

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  display_wayland = GDK_WAYLAND_DISPLAY (display);

  if (display_wayland->xdg_exporter_v2)
    {
      if (!impl->display_server.xdg_exported_v2)
        {
          struct zxdg_exported_v2 *xdg_exported_v2;

          xdg_exported_v2 =
            zxdg_exporter_v2_export_toplevel (display_wayland->xdg_exporter_v2,
                                              impl->display_server.wl_surface);
          zxdg_exported_v2_add_listener (xdg_exported_v2,
                                         &xdg_exported_v2_listener,
                                         window);

          impl->display_server.xdg_exported_v2 = xdg_exported_v2;
        }
    }
  else if (display_wayland->xdg_exporter_v1)
    {
      if (!impl->display_server.xdg_exported_v1)
        {
          struct zxdg_exported_v1 *xdg_exported_v1;

          xdg_exported_v1 =
            zxdg_exporter_v1_export (display_wayland->xdg_exporter_v1,
                                     impl->display_server.wl_surface);
          zxdg_exported_v1_add_listener (xdg_exported_v1,
                                         &xdg_exported_v1_listener,
                                         window);

          impl->display_server.xdg_exported_v1 = xdg_exported_v1;
        }
    }
  else
    {
      g_warning ("Server is missing xdg_foreign support");
      return FALSE;
    }


  closure = g_new0 (ExportedClosure, 1);
  closure->callback = callback;
  closure->user_data = user_data;
  closure->destroy_func = destroy_func;

  impl->exported.closures = g_list_append (impl->exported.closures, closure);
  impl->exported.export_count++;

  if (impl->exported.handle && !impl->exported.idle_source_id)
    impl->exported.idle_source_id = g_idle_add (exported_idle, window);

  return TRUE;
}

static void
gdk_wayland_window_unexport (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GList *l;

  if (impl->display_server.xdg_exported_v2)
    {
      g_clear_pointer (&impl->display_server.xdg_exported_v2,
                       zxdg_exported_v2_destroy);
    }
  else if (impl->display_server.xdg_exported_v1)
    {
      g_clear_pointer (&impl->display_server.xdg_exported_v1,
                       zxdg_exported_v1_destroy);
    }

  for (l = impl->exported.closures; l; l = l->next)
    {
      ExportedClosure *closure = l->data;

      if (closure->destroy_func)
        closure->destroy_func (closure->user_data);
    }

  g_list_free_full (impl->exported.closures, g_free);
  impl->exported.closures = NULL;
  g_clear_pointer (&impl->exported.handle, g_free);

  if (impl->exported.idle_source_id)
    {
      g_source_remove (impl->exported.idle_source_id);
      impl->exported.idle_source_id = 0;
    }
}

/**
 * gdk_wayland_window_unexport_handle:
 * @window: the #GdkWindow to unexport
 *
 * Destroys the handle that was obtained with
 * gdk_wayland_window_export_handle().
 *
 * It is an error to call this function on a window that
 * does not have a handle.
 *
 * Note that this API depends on an unstable Wayland protocol,
 * and thus may require changes in the future.
 *
 * Since: 3.22
 */
void
gdk_wayland_window_unexport_handle (GdkWindow *window)
{
  GdkWindowImplWayland *impl;

  g_return_if_fail (GDK_IS_WAYLAND_WINDOW (window));

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  g_return_if_fail (impl->display_server.xdg_exported_v2 ||
                    impl->display_server.xdg_exported_v1);

  impl->exported.export_count--;
  if (impl->exported.export_count == 0)
    gdk_wayland_window_unexport (window);
}

static void
unset_transient_for_exported (GdkWindow *window)
{
  GdkWindowImplWayland *impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  if (impl->imported_v2_transient_for)
    {
      g_clear_pointer (&impl->imported_v2_transient_for,
                       zxdg_imported_v2_destroy);
    }
  else if (impl->imported_v1_transient_for)
    {
      g_clear_pointer (&impl->imported_v1_transient_for,
                       zxdg_imported_v1_destroy);
    }
}

static void
xdg_imported_v1_destroyed (void                    *data,
                           struct zxdg_imported_v1 *zxdg_imported_v1)
{
  GdkWindow *window = data;

  unset_transient_for_exported (window);
}

static const struct zxdg_imported_v1_listener xdg_imported_v1_listener = {
  xdg_imported_v1_destroyed,
};

static void
xdg_imported_v2_destroyed (void                    *data,
                           struct zxdg_imported_v2 *zxdg_imported_v2)
{
  xdg_imported_v1_destroyed (data, NULL);
}

static const struct zxdg_imported_v2_listener xdg_imported_v2_listener = {
  xdg_imported_v2_destroyed,
};

/**
 * gdk_wayland_window_set_transient_for_exported:
 * @window: the #GdkWindow to make as transient
 * @parent_handle_str: an exported handle for a surface
 *
 * Marks @window as transient for the surface to which the given
 * @parent_handle_str refers. Typically, the handle will originate
 * from a gdk_wayland_window_export_handle() call in another process.
 *
 * Note that this API depends on an unstable Wayland protocol,
 * and thus may require changes in the future.
 *
 * Return value: %TRUE if the window has been marked as transient,
 *     %FALSE if an error occurred.
 *
 * Since: 3.22
 */
gboolean
gdk_wayland_window_set_transient_for_exported (GdkWindow *window,
                                               char      *parent_handle_str)
{
  GdkWindowImplWayland *impl;
  GdkWaylandDisplay *display_wayland;
  GdkDisplay *display = gdk_window_get_display (window);

  g_return_val_if_fail (GDK_IS_WAYLAND_WINDOW (window), FALSE);
  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY (display), FALSE);
  g_return_val_if_fail (!should_map_as_subsurface (window) &&
                        !should_map_as_popup (window), FALSE);

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  display_wayland = GDK_WAYLAND_DISPLAY (display);

  if (display_wayland->xdg_importer_v2)
    {
      gdk_window_set_transient_for (window, NULL);

      impl->imported_v2_transient_for =
        zxdg_importer_v2_import_toplevel (display_wayland->xdg_importer_v2,
                                          parent_handle_str);
      zxdg_imported_v2_add_listener (impl->imported_v2_transient_for,
                                     &xdg_imported_v2_listener,
                                     window);
    }
  else if (display_wayland->xdg_importer_v1)
    {
      gdk_window_set_transient_for (window, NULL);

      impl->imported_v1_transient_for =
        zxdg_importer_v1_import (display_wayland->xdg_importer_v1,
                                 parent_handle_str);
      zxdg_imported_v1_add_listener (impl->imported_v1_transient_for,
                                     &xdg_imported_v1_listener,
                                     window);
    }
  else
    {
      g_warning ("Server is missing xdg_foreign support");
      return FALSE;
    }

  gdk_wayland_window_sync_parent_of_imported (window);

  return TRUE;
}

static struct zwp_keyboard_shortcuts_inhibitor_v1 *
gdk_wayland_window_get_inhibitor (GdkWindowImplWayland *impl,
                                  struct wl_seat *seat)
{
  return g_hash_table_lookup (impl->shortcuts_inhibitors, seat);
}

void
gdk_wayland_window_inhibit_shortcuts (GdkWindow *window,
                                      GdkSeat   *gdk_seat)
{
  GdkWindowImplWayland *impl= GDK_WINDOW_IMPL_WAYLAND (window->impl);
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  struct wl_surface *surface = impl->display_server.wl_surface;
  struct wl_seat *seat = gdk_wayland_seat_get_wl_seat (gdk_seat);
  struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor;

  if (display->keyboard_shortcuts_inhibit == NULL)
    return;

  if (gdk_wayland_window_get_inhibitor (impl, seat))
    return; /* Already inhibitted */

  inhibitor =
      zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts (
          display->keyboard_shortcuts_inhibit, surface, seat);

  g_hash_table_insert (impl->shortcuts_inhibitors, seat, inhibitor);
}

void
gdk_wayland_window_restore_shortcuts (GdkWindow *window,
                                      GdkSeat   *gdk_seat)
{
  GdkWindowImplWayland *impl;
  struct wl_seat *seat;
  struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor;

  g_return_if_fail (GDK_IS_WAYLAND_WINDOW (window));
  g_return_if_fail (GDK_IS_WINDOW_IMPL_WAYLAND (window->impl));

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);
  seat = gdk_wayland_seat_get_wl_seat (gdk_seat);

  inhibitor = gdk_wayland_window_get_inhibitor (impl, seat);
  if (inhibitor == NULL)
    return; /* Not inhibitted */

  zwp_keyboard_shortcuts_inhibitor_v1_destroy (inhibitor);
  g_hash_table_remove (impl->shortcuts_inhibitors, seat);
}

/**
 * gdk_wayland_window_add_frame_callback_surface:
 * @window: the #GdkWindow requesting callbacks
 * @surface: the wl_surface to add
 *
 * Add @surface to a list of surfaces for which frame callback
 * listeners will get set up, additionally to the one of @window.
 *
 * This is useful when clients use subsurfaces independently of GDK.
 * For example if a client creates a subsurface that covers @window
 * entirely. If this subsurface is opaque, Wayland compositors may not
 * emit callbacks for the surface of @window any more.
 * Adding the covering subsurface via this function ensures the
 * @window will continue to update.
 *
 * A single callback is sufficient to trigger the next update. If more
 * than one are triggered, the later ones will get ignored.
 *
 * Before @surface gets destroyed it must get removed again using
 * gdk_wayland_window_remove_frame_callback_surface().
 *
 * Note that the client is responsible to commit the @surface.
 * One way to archive this is to always commit after the
 * #GdkSurface::after-paint signal was triggered.
 *
 * Since: 3.24.25
 */
void
gdk_wayland_window_add_frame_callback_surface (GdkWindow         *window,
                                               struct wl_surface *surface)
{
  GdkWindowImplWayland *impl;

  g_return_if_fail (GDK_IS_WAYLAND_WINDOW (window));
  g_return_if_fail (GDK_IS_WINDOW_IMPL_WAYLAND (window->impl));
  g_return_if_fail (surface != NULL);

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  g_hash_table_insert (impl->frame_callback_surfaces, surface, NULL);
}

/**
 * gdk_wayland_window_remove_frame_callback_surface:
 * @window: the #GdkWindow requesting callbacks
 * @surface: the surface to remove
 *
 * Remove @surface from the list of surfaces again that got added via
 * gdk_wayland_window_add_frame_callback_surface().
 *
 * This function must be called before @surface gets destroyed.
 *
 * Since: 3.24.25
 */
void
gdk_wayland_window_remove_frame_callback_surface (GdkWindow         *window,
                                                  struct wl_surface *surface)
{
  GdkWindowImplWayland *impl;

  g_return_if_fail (GDK_IS_WAYLAND_WINDOW (window));
  g_return_if_fail (GDK_IS_WINDOW_IMPL_WAYLAND (window->impl));
  g_return_if_fail (surface != NULL);

  impl = GDK_WINDOW_IMPL_WAYLAND (window->impl);

  g_hash_table_remove (impl->frame_callback_surfaces, surface);
}
