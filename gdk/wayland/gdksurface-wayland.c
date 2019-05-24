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
#include "gdkwaylandsurface.h"

#include "gdksurfaceprivate.h"
#include "gdkdisplay-wayland.h"
#include "gdkglcontext-wayland.h"
#include "gdkframeclockidleprivate.h"
#include "gdkprivate-wayland.h"
#include "gdkdeviceprivate.h"
#include "gdkprivate-wayland.h"
#include "gdkmonitor-wayland.h"
#include "gdkseat-wayland.h"
#include <wayland/xdg-shell-unstable-v6-client-protocol.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

enum {
  COMMITTED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

#define SURFACE_IS_TOPLEVEL(surface)  TRUE

#define MAX_WL_BUFFER_SIZE (4083) /* 4096 minus header, string argument length and NUL byte */

typedef enum _PositionMethod
{
  POSITION_METHOD_NONE,
  POSITION_METHOD_MOVE_RESIZE,
  POSITION_METHOD_MOVE_TO_RECT
} PositionMethod;

struct _GdkWaylandSurface
{
  GdkSurface parent_instance;

  struct {
    /* The wl_outputs that this surface currently touches */
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
    struct wl_egl_window *egl_window;
    struct wl_egl_window *dummy_egl_window;
    struct zxdg_exported_v1 *xdg_exported;
    struct org_kde_kwin_server_decoration *server_decoration;
  } display_server;

  EGLSurface egl_surface;
  EGLSurface dummy_egl_surface;

  unsigned int initial_configure_received : 1;
  unsigned int mapped : 1;
  unsigned int pending_commit : 1;
  unsigned int awaiting_frame : 1;
  GdkSurfaceTypeHint hint;
  GdkSurface *transient_for;
  GdkSurface *popup_parent;
  PositionMethod position_method;

  int pending_buffer_offset_x;
  int pending_buffer_offset_y;

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
  GdkSurfaceHints geometry_mask;

  GdkSeat *grab_input_seat;

  gint64 pending_frame_counter;
  guint32 scale;

  int margin_left;
  int margin_right;
  int margin_top;
  int margin_bottom;
  gboolean margin_dirty;

  struct wl_output *initial_fullscreen_output;

  cairo_region_t *opaque_region;
  gboolean opaque_region_dirty;

  cairo_region_t *input_region;
  gboolean input_region_dirty;

  int saved_width;
  int saved_height;

  gulong parent_surface_committed_handler;

  struct {
    GdkRectangle rect;
    GdkGravity rect_anchor;
    GdkGravity surface_anchor;
    GdkAnchorHints anchor_hints;
    gint rect_anchor_dx;
    gint rect_anchor_dy;
  } pending_move_to_rect;

  struct {
    int width;
    int height;
    GdkSurfaceState state;
  } pending;

  struct {
    GdkWaylandSurfaceExported callback;
    gpointer user_data;
    GDestroyNotify destroy_func;
  } exported;

  struct zxdg_imported_v1 *imported_transient_for;
  GHashTable *shortcuts_inhibitors;
};

struct _GdkWaylandSurfaceClass
{
  GdkSurfaceClass parent_class;
};

static void gdk_wayland_surface_maybe_configure (GdkSurface *surface,
                                                 int         width,
                                                 int         height,
                                                 int         scale);

static void maybe_set_gtk_surface_dbus_properties (GdkSurface *surface);
static void maybe_set_gtk_surface_modal (GdkSurface *surface);

static void gdk_wayland_surface_sync_margin (GdkSurface *surface);
static void gdk_wayland_surface_sync_input_region (GdkSurface *surface);
static void gdk_wayland_surface_sync_opaque_region (GdkSurface *surface);

static void unset_transient_for_exported (GdkSurface *surface);

static void calculate_moved_to_rect_result (GdkSurface    *surface,
                                            int            x,
                                            int            y,
                                            int            width,
                                            int            height,
                                            GdkRectangle  *flipped_rect,
                                            GdkRectangle  *final_rect,
                                            gboolean      *flipped_x,
                                            gboolean      *flipped_y);

static gboolean gdk_wayland_surface_is_exported (GdkSurface *surface);

G_DEFINE_TYPE (GdkWaylandSurface, gdk_wayland_surface, GDK_TYPE_SURFACE)

static void
gdk_wayland_surface_init (GdkWaylandSurface *impl)
{
  impl->scale = 1;
  impl->initial_fullscreen_output = NULL;
  impl->saved_width = -1;
  impl->saved_height = -1;
  impl->shortcuts_inhibitors = g_hash_table_new (NULL, NULL);
}

static void
_gdk_wayland_screen_add_orphan_dialog (GdkSurface *surface)
{
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

  if (!g_list_find (display_wayland->orphan_dialogs, surface))
    display_wayland->orphan_dialogs =
      g_list_prepend (display_wayland->orphan_dialogs, surface);
}

static void
_gdk_wayland_surface_save_size (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (surface->state & (GDK_SURFACE_STATE_FULLSCREEN | GDK_SURFACE_STATE_MAXIMIZED))
    return;

  impl->saved_width = surface->width - impl->margin_left - impl->margin_right;
  impl->saved_height = surface->height - impl->margin_top - impl->margin_bottom;
}

static void
_gdk_wayland_surface_clear_saved_size (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (surface->state & (GDK_SURFACE_STATE_FULLSCREEN | GDK_SURFACE_STATE_MAXIMIZED))
    return;

  impl->saved_width = -1;
  impl->saved_height = -1;
}

static void
gdk_wayland_surface_update_size (GdkSurface *surface,
                                 int32_t     width,
                                 int32_t     height,
                                 int         scale)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if ((surface->width == width) &&
      (surface->height == height) &&
      (impl->scale == scale))
    return;

  surface->width = width;
  surface->height = height;
  impl->scale = scale;

  if (impl->display_server.egl_window)
    wl_egl_window_resize (impl->display_server.egl_window, width * scale, height * scale, 0, 0);
  if (impl->display_server.wl_surface)
    wl_surface_set_buffer_scale (impl->display_server.wl_surface, scale);

  gdk_surface_invalidate_rect (surface, NULL);
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
  GdkSurface *surface = data;
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkFrameClock *clock = gdk_surface_get_frame_clock (surface);
  GdkFrameTimings *timings;

  GDK_DISPLAY_NOTE (GDK_DISPLAY (display_wayland), EVENTS, g_message ("frame %p", surface));

  wl_callback_destroy (callback);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (!impl->awaiting_frame)
    return;

  impl->awaiting_frame = FALSE;
  _gdk_frame_clock_thaw (clock);

  timings = gdk_frame_clock_get_timings (clock, impl->pending_frame_counter);
  impl->pending_frame_counter = 0;

  if (timings == NULL)
    return;

  timings->refresh_interval = 16667; /* default to 1/60th of a second */
  if (impl->display_server.outputs)
    {
      /* We pick a random output out of the outputs that the surface touches
       * The rate here is in milli-hertz */
      int refresh_rate =
        gdk_wayland_display_get_output_refresh_rate (display_wayland,
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
                             GdkSurface     *surface)
{
  GdkFrameTimings *timings = gdk_frame_clock_get_current_timings (clock);
  gint64 presentation_time;
  gint64 refresh_interval;

  if (surface->update_freeze_count > 0)
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

void
gdk_wayland_surface_request_frame (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct wl_callback *callback;
  GdkFrameClock *clock;

  if (impl->awaiting_frame)
    return;

  clock = gdk_surface_get_frame_clock (surface);

  callback = wl_surface_frame (impl->display_server.wl_surface);
  wl_callback_add_listener (callback, &frame_listener, surface);
  impl->pending_frame_counter = gdk_frame_clock_get_frame_counter (clock);
  impl->awaiting_frame = TRUE;
}

static void
on_frame_clock_after_paint (GdkFrameClock *clock,
                            GdkSurface    *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (impl->pending_commit)
    {
      if (surface->update_freeze_count > 0)
        return;

      gdk_wayland_surface_request_frame (surface);

      /* From this commit forward, we can't write to the buffer,
       * it's "live".  In the future, if we need to stage more changes
       * we have to allocate a new staging buffer and draw to it instead.
       *
       * Our one saving grace is if the compositor releases the buffer
       * before we need to stage any changes, then we can take it back and
       * use it again.
       */
      wl_surface_commit (impl->display_server.wl_surface);

      impl->pending_commit = FALSE;

      g_signal_emit (impl, signals[COMMITTED], 0);
    }

  if (impl->awaiting_frame)
    _gdk_frame_clock_freeze (clock);
}

void
gdk_wayland_surface_update_scale (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  guint32 scale;
  GSList *l;

  if (display_wayland->compositor_version < WL_SURFACE_HAS_BUFFER_SCALE)
    {
      /* We can't set the scale on this surface */
      return;
    }

  scale = 1;
  for (l = impl->display_server.outputs; l != NULL; l = l->next)
    {
      guint32 output_scale = gdk_wayland_display_get_output_scale (display_wayland, l->data);
      scale = MAX (scale, output_scale);
    }

  /* Notify app that scale changed */
  gdk_wayland_surface_maybe_configure (surface, surface->width, surface->height, scale);
}

static void gdk_wayland_surface_create_surface (GdkSurface *surface);

GdkSurface *
_gdk_wayland_display_create_surface (GdkDisplay     *display,
                                     GdkSurfaceType  surface_type,
                                     GdkSurface     *parent,
                                     int             x,
                                     int             y,
                                     int             width,
                                     int             height)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GdkSurface *surface;
  GdkWaylandSurface *impl;
  GdkFrameClock *frame_clock;

  if (parent)
    frame_clock = g_object_ref (gdk_surface_get_frame_clock (parent));
  else
    frame_clock = _gdk_frame_clock_idle_new ();

  surface = g_object_new (GDK_TYPE_WAYLAND_SURFACE,
                          "surface-type", surface_type,
                          "display", display,
                          "parent", parent,
                          "frame-clock", frame_clock,
                          NULL);

  impl = GDK_WAYLAND_SURFACE (surface);

  if (width > 65535)
    {
      g_warning ("Native Surfaces wider than 65535 pixels are not supported");
      width = 65535;
    }
  if (height > 65535)
    {
      g_warning ("Native Surfaces taller than 65535 pixels are not supported");
      height = 65535;
    }

  surface->x = x;
  surface->y = y;
  surface->width = width;
  surface->height = height;

  g_object_ref (surface);

  /* More likely to be right than just assuming 1 */
  if (display_wayland->compositor_version >= WL_SURFACE_HAS_BUFFER_SCALE &&
      gdk_display_get_n_monitors (display) > 0)
    impl->scale = gdk_monitor_get_scale_factor (gdk_display_get_monitor (display, 0));

  gdk_surface_set_title (surface, get_default_title ());

  if (parent == NULL)
    display_wayland->toplevels = g_list_prepend (display_wayland->toplevels, surface);

  impl->transient_for = parent;

  gdk_wayland_surface_create_surface (surface);

  g_signal_connect (frame_clock, "before-paint", G_CALLBACK (on_frame_clock_before_paint), surface);
  g_signal_connect (frame_clock, "after-paint", G_CALLBACK (on_frame_clock_after_paint), surface);

  g_object_unref (frame_clock);

  return surface;
}

void
gdk_wayland_surface_attach_image (GdkSurface           *surface,
                                  cairo_surface_t      *cairo_surface,
                                  const cairo_region_t *damage)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display;
  cairo_rectangle_int_t rect;
  int i, n;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  g_assert (_gdk_wayland_is_shm_surface (cairo_surface));

  /* Attach this new buffer to the surface */
  wl_surface_attach (impl->display_server.wl_surface,
                     _gdk_wayland_shm_surface_get_wl_buffer (cairo_surface),
                     impl->pending_buffer_offset_x,
                     impl->pending_buffer_offset_y);
  impl->pending_buffer_offset_x = 0;
  impl->pending_buffer_offset_y = 0;

  /* Only set the buffer scale if supported by the compositor */
  display = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  if (display->compositor_version >= WL_SURFACE_HAS_BUFFER_SCALE)
    wl_surface_set_buffer_scale (impl->display_server.wl_surface, impl->scale);

  n = cairo_region_num_rectangles (damage);
  for (i = 0; i < n; i++)
    {
      cairo_region_get_rectangle (damage, i, &rect);
      wl_surface_damage (impl->display_server.wl_surface, rect.x, rect.y, rect.width, rect.height);
    }
  impl->pending_commit = TRUE;
}

void
gdk_wayland_surface_sync (GdkSurface *surface)
{
  gdk_wayland_surface_sync_margin (surface);
  gdk_wayland_surface_sync_opaque_region (surface);
  gdk_wayland_surface_sync_input_region (surface);
}

static gboolean
gdk_wayland_surface_beep (GdkSurface *surface)
{
  gdk_wayland_display_system_bell (gdk_surface_get_display (surface),
                                   surface);

  return TRUE;
}

static void
gdk_wayland_surface_finalize (GObject *object)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkWaylandSurface *impl;

  g_return_if_fail (GDK_IS_WAYLAND_SURFACE (object));

  impl = GDK_WAYLAND_SURFACE (object);

  if (gdk_wayland_surface_is_exported (surface))
    gdk_wayland_surface_unexport_handle (surface);

  g_free (impl->title);

  g_free (impl->application.application_id);
  g_free (impl->application.app_menu_path);
  g_free (impl->application.menubar_path);
  g_free (impl->application.window_object_path);
  g_free (impl->application.application_object_path);
  g_free (impl->application.unique_bus_name);

  g_clear_pointer (&impl->opaque_region, cairo_region_destroy);
  g_clear_pointer (&impl->input_region, cairo_region_destroy);
  g_clear_pointer (&impl->shortcuts_inhibitors, g_hash_table_unref);

  G_OBJECT_CLASS (gdk_wayland_surface_parent_class)->finalize (object);
}

static void
gdk_wayland_surface_configure (GdkSurface *surface,
                               int         width,
                               int         height,
                               int         scale)
{
  GdkDisplay *display;
  GdkEvent *event;

  event = gdk_event_new (GDK_CONFIGURE);
  event->any.surface = g_object_ref (surface);
  event->any.send_event = FALSE;
  event->configure.width = width;
  event->configure.height = height;

  gdk_wayland_surface_update_size (surface, width, height, scale);
  _gdk_surface_update_size (surface);

  display = gdk_surface_get_display (surface);
  _gdk_wayland_display_deliver_event (display, event);
}

static gboolean
is_realized_shell_surface (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  return (impl->display_server.xdg_surface ||
          impl->display_server.zxdg_surface_v6);
}

static gboolean
is_realized_toplevel (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  return (impl->display_server.xdg_toplevel ||
          impl->display_server.zxdg_toplevel_v6);
}

static gboolean
is_realized_popup (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  return (impl->display_server.xdg_popup ||
          impl->display_server.zxdg_popup_v6);
}

static void
gdk_wayland_surface_maybe_configure (GdkSurface *surface,
                                     int         width,
                                     int         height,
                                     int         scale)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  gboolean is_xdg_popup;
  gboolean is_visible;

  if (surface->width == width &&
      surface->height == height &&
      impl->scale == scale)
    return;

  /* For xdg_popup using an xdg_positioner, there is a race condition if
   * the application tries to change the size after it's mapped, but before
   * the initial configure is received, so hide and show the surface again
   * force the new size onto the compositor. See bug #772505.
   */

  is_xdg_popup = is_realized_popup (surface);
  is_visible = gdk_surface_is_visible (surface);

  if (is_xdg_popup && is_visible && !impl->initial_configure_received)
    gdk_surface_hide (surface);

  gdk_wayland_surface_configure (surface, width, height, scale);

  if (is_xdg_popup && is_visible && !impl->initial_configure_received)
    gdk_surface_show (surface);
}

static void
gdk_wayland_surface_sync_parent (GdkSurface *surface,
                                 GdkSurface *parent)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *impl_parent = NULL;

  g_assert (parent == NULL ||
            gdk_surface_get_display (surface) == gdk_surface_get_display (parent));

  if (!is_realized_toplevel (surface))
    return;

  if (impl->transient_for)
    impl_parent = GDK_WAYLAND_SURFACE (impl->transient_for);
  else if (parent)
    impl_parent = GDK_WAYLAND_SURFACE (parent);

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
    default:
      g_assert_not_reached ();
    }
}

static void
gdk_wayland_surface_sync_parent_of_imported (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (!impl->display_server.wl_surface)
    return;

  if (!impl->imported_transient_for)
    return;

  if (!is_realized_toplevel (surface))
    return;

  zxdg_imported_v1_set_parent_of (impl->imported_transient_for,
                                  impl->display_server.wl_surface);
}

static void
gdk_wayland_surface_update_dialogs (GdkSurface *surface)
{
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GList *l;

  if (!display_wayland->orphan_dialogs)
    return;

  for (l = display_wayland->orphan_dialogs; l; l = l->next)
    {
      GdkSurface *w = l->data;
      GdkWaylandSurface *impl;

      if (!GDK_IS_WAYLAND_SURFACE (w))
        continue;

      impl = GDK_WAYLAND_SURFACE (w);
      if (w == surface)
	continue;
      if (impl->hint != GDK_SURFACE_TYPE_HINT_DIALOG)
        continue;
      if (impl->transient_for)
        continue;

      /* Update the parent relationship only for dialogs without transients */
      gdk_wayland_surface_sync_parent (w, surface);
    }
}

static void
gdk_wayland_surface_sync_title (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

  if (!is_realized_toplevel (surface))
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
    default:
      g_assert_not_reached ();
    }
}

static void
gdk_wayland_surface_get_window_geometry (GdkSurface   *surface,
                                         GdkRectangle *geometry)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  *geometry = (GdkRectangle) {
    .x = impl->margin_left,
    .y = impl->margin_top,
    .width = surface->width - (impl->margin_left + impl->margin_right),
    .height = surface->height - (impl->margin_top + impl->margin_bottom)
  };
}

static void
gdk_wayland_surface_sync_margin (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkRectangle geometry;

  if (!is_realized_shell_surface (surface))
    return;

  gdk_wayland_surface_get_window_geometry (surface, &geometry);
  gdk_surface_set_geometry_hints (surface,
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
    default:
      g_assert_not_reached ();
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
gdk_wayland_surface_sync_opaque_region (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct wl_region *wl_region = NULL;

  if (!impl->display_server.wl_surface)
    return;

  if (!impl->opaque_region_dirty)
    return;

  if (impl->opaque_region != NULL)
    wl_region = wl_region_from_cairo_region (GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface)),
                                             impl->opaque_region);

  wl_surface_set_opaque_region (impl->display_server.wl_surface, wl_region);

  if (wl_region != NULL)
    wl_region_destroy (wl_region);

  impl->opaque_region_dirty = FALSE;
}

static void
gdk_wayland_surface_sync_input_region (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct wl_region *wl_region = NULL;

  if (!impl->display_server.wl_surface)
    return;

  if (!impl->input_region_dirty)
    return;

  if (impl->input_region != NULL)
    wl_region = wl_region_from_cairo_region (GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface)),
                                             impl->input_region);

  wl_surface_set_input_region (impl->display_server.wl_surface, wl_region);

  if (wl_region != NULL)
    wl_region_destroy (wl_region);

  impl->input_region_dirty = FALSE;
}

static void
surface_enter (void              *data,
               struct wl_surface *wl_surface,
               struct wl_output  *output)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  GDK_DISPLAY_NOTE (gdk_surface_get_display (surface), EVENTS,
            g_message ("surface enter, surface %p output %p", surface, output));

  impl->display_server.outputs = g_slist_prepend (impl->display_server.outputs, output);

  gdk_wayland_surface_update_scale (surface);
}

static void
surface_leave (void              *data,
               struct wl_surface *wl_surface,
               struct wl_output  *output)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  GDK_DISPLAY_NOTE (gdk_surface_get_display (surface), EVENTS,
            g_message ("surface leave, surface %p output %p", surface, output));

  impl->display_server.outputs = g_slist_remove (impl->display_server.outputs, output);

  if (impl->display_server.outputs)
    gdk_wayland_surface_update_scale (surface);
}

static const struct wl_surface_listener surface_listener = {
  surface_enter,
  surface_leave
};

static void
gdk_wayland_surface_create_surface (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

  impl->display_server.wl_surface = wl_compositor_create_surface (display_wayland->compositor);
  wl_surface_add_listener (impl->display_server.wl_surface, &surface_listener, surface);
}

static void
gdk_wayland_surface_handle_configure (GdkSurface *surface,
                                      uint32_t    serial)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkSurfaceState new_state;
  int width = impl->pending.width;
  int height = impl->pending.height;
  gboolean fixed_size;
  gboolean saved_size;

  if (!impl->initial_configure_received)
    {
      gdk_surface_thaw_updates (surface);
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

  new_state = impl->pending.state;
  impl->pending.state = 0;

  fixed_size =
    new_state & (GDK_SURFACE_STATE_MAXIMIZED |
                 GDK_SURFACE_STATE_FULLSCREEN |
                 GDK_SURFACE_STATE_TILED);

  saved_size = (width == 0 && height == 0);
  /* According to xdg_shell, an xdg_surface.configure with size 0x0
   * should be interpreted as that it is up to the client to set a
   * size.
   *
   * When transitioning from maximize or fullscreen state, this means
   * the client should configure its size back to what it was before
   * being maximize or fullscreen.
   */
  if (saved_size && !fixed_size)
    {
      width = impl->saved_width;
      height = impl->saved_height;
    }

  if (width > 0 && height > 0)
    {
      GdkSurfaceHints geometry_mask = impl->geometry_mask;

      /* Ignore size increments for maximized/fullscreen surfaces */
      if (fixed_size)
        geometry_mask &= ~GDK_HINT_RESIZE_INC;
      if (!saved_size)
        {
          /* Do not reapply contrains if we are restoring original size */
          gdk_surface_constrain_size (&impl->geometry_hints,
                                     geometry_mask,
                                     width + impl->margin_left + impl->margin_right,
                                     height + impl->margin_top + impl->margin_bottom,
                                     &width,
                                     &height);

          /* Save size for next time we get 0x0 */
          _gdk_wayland_surface_save_size (surface);
        }

      gdk_wayland_surface_configure (surface, width, height, impl->scale);
    }

  GDK_DISPLAY_NOTE (gdk_surface_get_display (surface), EVENTS,
            g_message ("configure, surface %p %dx%d,%s%s%s%s",
                       surface, width, height,
                       (new_state & GDK_SURFACE_STATE_FULLSCREEN) ? " fullscreen" : "",
                       (new_state & GDK_SURFACE_STATE_MAXIMIZED) ? " maximized" : "",
                       (new_state & GDK_SURFACE_STATE_FOCUSED) ? " focused" : "",
                       (new_state & GDK_SURFACE_STATE_TILED) ? " tiled" : ""));

  gdk_surface_set_state (surface, new_state);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_surface_ack_configure (impl->display_server.xdg_surface, serial);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_surface_v6_ack_configure (impl->display_server.zxdg_surface_v6,
                                     serial);
      break;
    default:
      g_assert_not_reached ();
    }

  if (impl->hint != GDK_SURFACE_TYPE_HINT_DIALOG &&
      new_state & GDK_SURFACE_STATE_FOCUSED)
    gdk_wayland_surface_update_dialogs (surface);
}

static void
gdk_wayland_surface_handle_configure_toplevel (GdkSurface      *surface,
                                               int32_t          width,
                                               int32_t          height,
                                               GdkSurfaceState  state)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->pending.state |= state;
  impl->pending.width = width;
  impl->pending.height = height;
}

static void
gdk_wayland_surface_handle_close (GdkSurface *surface)
{
  GdkDisplay *display;
  GdkEvent *event;

  display = gdk_surface_get_display (surface);

  GDK_DISPLAY_NOTE (display, EVENTS, g_message ("close %p", surface));

  event = gdk_event_new (GDK_DELETE);
  event->any.surface = g_object_ref (surface);
  event->any.send_event = TRUE;

  _gdk_wayland_display_deliver_event (display, event);
}

static void
xdg_surface_configure (void               *data,
                       struct xdg_surface *xdg_surface,
                       uint32_t            serial)
{
  GdkSurface *surface = GDK_SURFACE (data);

  gdk_wayland_surface_handle_configure (surface, serial);
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
  GdkSurface *surface = GDK_SURFACE (data);
  uint32_t *p;
  GdkSurfaceState pending_state = 0;

  wl_array_for_each (p, states)
    {
      uint32_t state = *p;

      switch (state)
        {
        case XDG_TOPLEVEL_STATE_FULLSCREEN:
          pending_state |= GDK_SURFACE_STATE_FULLSCREEN;
          break;
        case XDG_TOPLEVEL_STATE_MAXIMIZED:
          pending_state |= GDK_SURFACE_STATE_MAXIMIZED;
          break;
        case XDG_TOPLEVEL_STATE_ACTIVATED:
          pending_state |= GDK_SURFACE_STATE_FOCUSED;
          break;
        case XDG_TOPLEVEL_STATE_RESIZING:
          break;
        default:
          /* Unknown state */
          break;
        }
    }

  gdk_wayland_surface_handle_configure_toplevel (surface, width, height,
                                                 pending_state);
}

static void
xdg_toplevel_close (void                *data,
                    struct xdg_toplevel *xdg_toplevel)
{
  GdkSurface *surface = GDK_SURFACE (data);

  gdk_wayland_surface_handle_close (surface);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
  xdg_toplevel_configure,
  xdg_toplevel_close,
};

static void
create_xdg_toplevel_resources (GdkSurface *surface)
{
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->display_server.xdg_surface =
    xdg_wm_base_get_xdg_surface (display_wayland->xdg_wm_base,
                                 impl->display_server.wl_surface);
  xdg_surface_add_listener (impl->display_server.xdg_surface,
                            &xdg_surface_listener,
                            surface);

  impl->display_server.xdg_toplevel =
    xdg_surface_get_toplevel (impl->display_server.xdg_surface);
  xdg_toplevel_add_listener (impl->display_server.xdg_toplevel,
                             &xdg_toplevel_listener,
                             surface);
}

static void
zxdg_surface_v6_configure (void                   *data,
                           struct zxdg_surface_v6 *xdg_surface,
                           uint32_t                serial)
{
  GdkSurface *surface = GDK_SURFACE (data);

  gdk_wayland_surface_handle_configure (surface, serial);
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
  GdkSurface *surface = GDK_SURFACE (data);
  uint32_t *p;
  GdkSurfaceState pending_state = 0;

  wl_array_for_each (p, states)
    {
      uint32_t state = *p;

      switch (state)
        {
        case ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN:
          pending_state |= GDK_SURFACE_STATE_FULLSCREEN;
          break;
        case ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED:
          pending_state |= GDK_SURFACE_STATE_MAXIMIZED;
          break;
        case ZXDG_TOPLEVEL_V6_STATE_ACTIVATED:
          pending_state |= GDK_SURFACE_STATE_FOCUSED;
          break;
        case ZXDG_TOPLEVEL_V6_STATE_RESIZING:
          break;
        default:
          /* Unknown state */
          break;
        }
    }

  gdk_wayland_surface_handle_configure_toplevel (surface, width, height,
                                                 pending_state);
}

static void
zxdg_toplevel_v6_close (void                    *data,
                        struct zxdg_toplevel_v6 *xdg_toplevel)
{
  GdkSurface *surface = GDK_SURFACE (data);

  gdk_wayland_surface_handle_close (surface);
}

static const struct zxdg_toplevel_v6_listener zxdg_toplevel_v6_listener = {
  zxdg_toplevel_v6_configure,
  zxdg_toplevel_v6_close,
};

static void
create_zxdg_toplevel_v6_resources (GdkSurface *surface)
{
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->display_server.zxdg_surface_v6 =
    zxdg_shell_v6_get_xdg_surface (display_wayland->zxdg_shell_v6,
                                   impl->display_server.wl_surface);
  zxdg_surface_v6_add_listener (impl->display_server.zxdg_surface_v6,
                                &zxdg_surface_v6_listener,
                                surface);

  impl->display_server.zxdg_toplevel_v6 =
    zxdg_surface_v6_get_toplevel (impl->display_server.zxdg_surface_v6);
  zxdg_toplevel_v6_add_listener (impl->display_server.zxdg_toplevel_v6,
                                 &zxdg_toplevel_v6_listener,
                                 surface);
}

static void
gdk_wayland_surface_create_xdg_toplevel (GdkSurface *surface)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  const gchar *app_id;

  gdk_surface_freeze_updates (surface);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      create_xdg_toplevel_resources (surface);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      create_zxdg_toplevel_v6_resources (surface);
      break;
    default:
      g_assert_not_reached ();
    }

  gdk_wayland_surface_sync_parent (surface, NULL);
  gdk_wayland_surface_sync_parent_of_imported (surface);
  gdk_wayland_surface_sync_title (surface);

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      if (surface->state & GDK_SURFACE_STATE_MAXIMIZED)
        xdg_toplevel_set_maximized (impl->display_server.xdg_toplevel);
      if (surface->state & GDK_SURFACE_STATE_FULLSCREEN)
        xdg_toplevel_set_fullscreen (impl->display_server.xdg_toplevel,
                                     impl->initial_fullscreen_output);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      if (surface->state & GDK_SURFACE_STATE_MAXIMIZED)
        zxdg_toplevel_v6_set_maximized (impl->display_server.zxdg_toplevel_v6);
      if (surface->state & GDK_SURFACE_STATE_FULLSCREEN)
        zxdg_toplevel_v6_set_fullscreen (impl->display_server.zxdg_toplevel_v6,
                                         impl->initial_fullscreen_output);
      break;
    default:
      g_assert_not_reached ();
    }

  impl->initial_fullscreen_output = NULL;

  app_id = impl->application.application_id;
  if (app_id == NULL)
    app_id = g_get_prgname ();

  if (app_id == NULL)
    app_id = "GTK+ Application";

  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_set_app_id (impl->display_server.xdg_toplevel,
                               app_id);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_set_app_id (impl->display_server.zxdg_toplevel_v6,
                                   app_id);
      break;
    default:
      g_assert_not_reached ();
    }

  maybe_set_gtk_surface_dbus_properties (surface);
  maybe_set_gtk_surface_modal (surface);

  if (impl->hint == GDK_SURFACE_TYPE_HINT_DIALOG)
    _gdk_wayland_screen_add_orphan_dialog (surface);

  wl_surface_commit (impl->display_server.wl_surface);
}

static void
gdk_wayland_surface_handle_configure_popup (GdkSurface *surface,
                                            int32_t     x,
                                            int32_t     y,
                                            int32_t     width,
                                            int32_t     height)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkRectangle flipped_rect;
  GdkRectangle final_rect;
  gboolean flipped_x;
  gboolean flipped_y;

  g_return_if_fail (impl->transient_for);

  if (impl->position_method != POSITION_METHOD_MOVE_TO_RECT)
    return;

  calculate_moved_to_rect_result (surface, x, y, width, height,
                                  &flipped_rect,
                                  &final_rect,
                                  &flipped_x,
                                  &flipped_y);

  impl->position_method = POSITION_METHOD_MOVE_TO_RECT;

  g_signal_emit_by_name (surface,
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
  GdkSurface *surface = GDK_SURFACE (data);

  gdk_wayland_surface_handle_configure_popup (surface, x, y, width, height);
}

static void
xdg_popup_done (void             *data,
                struct xdg_popup *xdg_popup)
{
  GdkSurface *surface = GDK_SURFACE (data);

  GDK_DISPLAY_NOTE (gdk_surface_get_display (surface), EVENTS, g_message ("done %p", surface));

  gdk_surface_hide (surface);
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
  GdkSurface *surface = GDK_SURFACE (data);

  gdk_wayland_surface_handle_configure_popup (surface, x, y, width, height);
}

static void
zxdg_popup_v6_done (void                 *data,
                    struct zxdg_popup_v6 *xdg_popup)
{
  GdkSurface *surface = GDK_SURFACE (data);

  GDK_NOTE (EVENTS,
            g_message ("done %p", surface));

  gdk_surface_hide (surface);
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
surface_anchor_to_gravity (GdkGravity rect_anchor)
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

  return (ZXDG_POSITIONER_V6_ANCHOR_TOP |
          ZXDG_POSITIONER_V6_ANCHOR_LEFT);
}

static enum zxdg_positioner_v6_gravity
surface_anchor_to_gravity_legacy (GdkGravity rect_anchor)
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

  return (ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
          ZXDG_POSITIONER_V6_GRAVITY_RIGHT);
}

void
gdk_wayland_surface_announce_csd (GdkSurface *surface)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  if (!display_wayland->server_decoration_manager)
    return;
  impl->display_server.server_decoration =
    org_kde_kwin_server_decoration_manager_create (display_wayland->server_decoration_manager,
                                                  impl->display_server.wl_surface);
  if (impl->display_server.server_decoration)
    org_kde_kwin_server_decoration_request_mode (impl->display_server.server_decoration,
                                                ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_CLIENT);
}

static GdkSurface *
get_real_parent_and_translate (GdkSurface *surface,
                               gint       *x,
                               gint       *y)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkSurface *parent = impl->transient_for;

  return parent;
}

static void
translate_to_real_parent_surface_geometry (GdkSurface  *surface,
                                           gint        *x,
                                           gint        *y)
{
  GdkSurface *parent;

  parent = get_real_parent_and_translate (surface, x, y);

  *x -= parent->shadow_left;
  *y -= parent->shadow_top;
}

static GdkSurface *
translate_from_real_parent_surface_geometry (GdkSurface *surface,
                                             gint       *x,
                                             gint       *y)
{
  GdkSurface *parent;
  gint dx = 0;
  gint dy = 0;

  parent = get_real_parent_and_translate (surface, &dx, &dy);

  *x -= dx - parent->shadow_left;
  *y -= dy - parent->shadow_top;

  return parent;
}

static void
calculate_popup_rect (GdkSurface   *surface,
                      GdkGravity    rect_anchor,
                      GdkGravity    surface_anchor,
                      GdkRectangle *out_rect)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkRectangle geometry;
  GdkRectangle anchor_rect;
  int x = 0, y = 0;

  gdk_wayland_surface_get_window_geometry (surface, &geometry);

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
    default:
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

  switch (surface_anchor)
    {
    default:
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
    default:
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
    default:
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
calculate_moved_to_rect_result (GdkSurface   *surface,
                                int           x,
                                int           y,
                                int           width,
                                int           height,
                                GdkRectangle *flipped_rect,
                                GdkRectangle *final_rect,
                                gboolean     *flipped_x,
                                gboolean     *flipped_y)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkSurface *parent;
  gint surface_x, surface_y;
  gint surface_width, surface_height;
  GdkRectangle best_rect;

  parent = translate_from_real_parent_surface_geometry (surface, &x, &y);
  *final_rect = (GdkRectangle) {
    .x = x,
    .y = y,
    .width = width,
    .height = height,
  };

  surface_x = parent->x + x;
  surface_y = parent->y + y;
  surface_width = width + surface->shadow_left + surface->shadow_right;
  surface_height = height + surface->shadow_top + surface->shadow_bottom;

  gdk_surface_move_resize (surface,
                          surface_x, surface_y,
                          surface_width, surface_height);

  calculate_popup_rect (surface,
                        impl->pending_move_to_rect.rect_anchor,
                        impl->pending_move_to_rect.surface_anchor,
                        &best_rect);

  *flipped_rect = best_rect;

  if (x != best_rect.x &&
      impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_FLIP_X)
    {
      GdkRectangle flipped_x_rect;
      GdkGravity flipped_rect_anchor;
      GdkGravity flipped_surface_anchor;

      flipped_rect_anchor =
        flip_anchor_horizontally (impl->pending_move_to_rect.rect_anchor);
      flipped_surface_anchor =
        flip_anchor_horizontally (impl->pending_move_to_rect.surface_anchor),
      calculate_popup_rect (surface,
                            flipped_rect_anchor,
                            flipped_surface_anchor,
                            &flipped_x_rect);

      if (flipped_x_rect.x == x)
        flipped_rect->x = x;
    }
  if (y != best_rect.y &&
      impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_FLIP_Y)
    {
      GdkRectangle flipped_y_rect;
      GdkGravity flipped_rect_anchor;
      GdkGravity flipped_surface_anchor;

      flipped_rect_anchor =
        flip_anchor_vertically (impl->pending_move_to_rect.rect_anchor);
      flipped_surface_anchor =
        flip_anchor_vertically (impl->pending_move_to_rect.surface_anchor),
      calculate_popup_rect (surface,
                            flipped_rect_anchor,
                            flipped_surface_anchor,
                            &flipped_y_rect);

      if (flipped_y_rect.y == y)
        flipped_rect->y = y;
    }

  *flipped_x = flipped_rect->x != best_rect.x;
  *flipped_y = flipped_rect->y != best_rect.y;
}

static gpointer
create_dynamic_positioner (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkRectangle geometry;
  uint32_t constraint_adjustment = ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_NONE;
  gint real_anchor_rect_x, real_anchor_rect_y;
  gint anchor_rect_width, anchor_rect_height;

  gdk_wayland_surface_get_window_geometry (surface, &geometry);

  real_anchor_rect_x = impl->pending_move_to_rect.rect.x;
  real_anchor_rect_y = impl->pending_move_to_rect.rect.y;
  translate_to_real_parent_surface_geometry (surface,
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

        gravity = surface_anchor_to_gravity (impl->pending_move_to_rect.surface_anchor);
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

        gravity = surface_anchor_to_gravity_legacy (impl->pending_move_to_rect.surface_anchor);
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
    default:
      g_assert_not_reached ();
    }

  g_assert_not_reached ();
}

static gpointer
create_simple_positioner (GdkSurface *surface,
                          GdkSurface *parent)
{
  GdkWaylandDisplay *display =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkRectangle geometry;
  GdkRectangle parent_geometry;
  int parent_x, parent_y;

  gdk_wayland_surface_get_window_geometry (surface, &geometry);

  parent_x = parent->x;
  parent_y = parent->y;

  gdk_wayland_surface_get_window_geometry (parent, &parent_geometry);
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
                                        (surface->x + geometry.x) - parent_x,
                                        (surface->y + geometry.y) - parent_y,
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
                                            (surface->x + geometry.x) - parent_x,
                                            (surface->y + geometry.y) - parent_y,
                                            1, 1);
        zxdg_positioner_v6_set_anchor (positioner,
                                       (ZXDG_POSITIONER_V6_ANCHOR_TOP |
                                        ZXDG_POSITIONER_V6_ANCHOR_LEFT));
        zxdg_positioner_v6_set_gravity (positioner,
                                        (ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
                                         ZXDG_POSITIONER_V6_GRAVITY_RIGHT));

        return positioner;
      }
    default:
      g_assert_not_reached ();
    }

  g_assert_not_reached ();
}

static void
gdk_wayland_surface_create_xdg_popup (GdkSurface     *surface,
                                      GdkSurface     *parent,
                                      GdkWaylandSeat *grab_input_seat)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandSurface *parent_impl = GDK_WAYLAND_SURFACE (parent);
  gpointer positioner;

  if (!impl->display_server.wl_surface)
    return;

  if (!is_realized_shell_surface (parent))
    return;

  if (is_realized_toplevel (surface))
    {
      g_warning ("Can't map popup, already mapped as toplevel");
      return;
    }
  if (is_realized_popup (surface))
    {
      g_warning ("Can't map popup, already mapped");
      return;
    }
  if (grab_input_seat &&
      ((display->current_popups &&
        g_list_last (display->current_popups)->data != parent) ||
       (!display->current_popups &&
        !is_realized_toplevel (parent))))
    {
      g_warning ("Tried to map a popup with a non-top most parent");
      return;
    }

  gdk_surface_freeze_updates (surface);

  if (impl->position_method == POSITION_METHOD_MOVE_TO_RECT)
    positioner = create_dynamic_positioner (surface);
  else
    positioner = create_simple_positioner (surface, parent);

  switch (display->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      impl->display_server.xdg_surface =
        xdg_wm_base_get_xdg_surface (display->xdg_wm_base,
                                     impl->display_server.wl_surface);
      xdg_surface_add_listener (impl->display_server.xdg_surface,
                                &xdg_surface_listener,
                                surface);
      impl->display_server.xdg_popup =
        xdg_surface_get_popup (impl->display_server.xdg_surface,
                               parent_impl->display_server.xdg_surface,
                               positioner);
      xdg_popup_add_listener (impl->display_server.xdg_popup,
                              &xdg_popup_listener,
                              surface);
      xdg_positioner_destroy (positioner);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      impl->display_server.zxdg_surface_v6 =
        zxdg_shell_v6_get_xdg_surface (display->zxdg_shell_v6,
                                       impl->display_server.wl_surface);
      zxdg_surface_v6_add_listener (impl->display_server.zxdg_surface_v6,
                                    &zxdg_surface_v6_listener,
                                    surface);
      impl->display_server.zxdg_popup_v6 =
        zxdg_surface_v6_get_popup (impl->display_server.zxdg_surface_v6,
                                   parent_impl->display_server.zxdg_surface_v6,
                                   positioner);
      zxdg_popup_v6_add_listener (impl->display_server.zxdg_popup_v6,
                                  &zxdg_popup_v6_listener,
                                  surface);
      zxdg_positioner_v6_destroy (positioner);
      break;
    default:
      g_assert_not_reached ();
    }

  if (grab_input_seat)
    {
      struct wl_seat *seat;
      guint32 serial;

      seat = gdk_wayland_seat_get_wl_seat (GDK_SEAT (grab_input_seat));
      serial = _gdk_wayland_seat_get_last_implicit_grab_serial (grab_input_seat, NULL);

      switch (display->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_popup_grab (impl->display_server.xdg_popup, seat, serial);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_popup_v6_grab (impl->display_server.zxdg_popup_v6, seat, serial);
          break;
        default:
          g_assert_not_reached ();
        }
    }

  wl_surface_commit (impl->display_server.wl_surface);

  impl->popup_parent = parent;
  display->current_popups = g_list_append (display->current_popups, surface);
}

static GdkWaylandSeat *
find_grab_input_seat (GdkSurface *surface,
                      GdkSurface *transient_for)
{
  GdkSurface *attached_grab_surface;
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandSurface *tmp_impl;

  /* Use the device that was used for the grab as the device for
   * the popup surface setup - so this relies on GTK+ taking the
   * grab before showing the popup surface.
   */
  if (impl->grab_input_seat)
    return GDK_WAYLAND_SEAT (impl->grab_input_seat);

  /* HACK: GtkMenu grabs a special surface known as the "grab transfer surface"
   * and then transfers the grab over to the correct surface later. Look for
   * this surface when taking the grab to know it's correct.
   *
   * See: associate_menu_grab_transfer_surface in gtkmenu.c
   */
  attached_grab_surface = g_object_get_data (G_OBJECT (surface), "gdk-attached-grab-surface");
  if (attached_grab_surface)
    {
      tmp_impl = GDK_WAYLAND_SURFACE (attached_grab_surface);
      if (tmp_impl->grab_input_seat)
        return GDK_WAYLAND_SEAT (tmp_impl->grab_input_seat);
    }

  while (transient_for)
    {
      tmp_impl = GDK_WAYLAND_SURFACE (transient_for);

      if (tmp_impl->grab_input_seat)
        return GDK_WAYLAND_SEAT (tmp_impl->grab_input_seat);

      transient_for = tmp_impl->transient_for;
    }

  return NULL;
}

static gboolean
should_be_mapped (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  /* Don't map crazy temp that GTK+ uses for internal X11 shenanigans. */
  if (surface->surface_type == GDK_SURFACE_TEMP && surface->x < 0 && surface->y < 0)
    return FALSE;

  if (impl->hint == GDK_SURFACE_TYPE_HINT_DND)
    return FALSE;

  return TRUE;
}

static gboolean
should_map_as_popup (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_TYPE (surface) == GDK_SURFACE_POPUP)
    return TRUE;

  /* Ideally, popup would be temp surfaces with a parent and grab */
  if (GDK_SURFACE_TYPE (surface) == GDK_SURFACE_TEMP)
    {
      /* If a temp surface has a parent and a grab, we can use a popup */
      if (impl->transient_for)
        {
          if (impl->grab_input_seat)
            return TRUE;
        }
      else
        g_message ("Surface %p is a temporary surface without parent, "
                   "application will not be able to position it on screen.",
                   surface);
    }

  /* Yet we need to keep the surface type hint tests for compatibility */
  switch ((guint) impl->hint)
    {
    case GDK_SURFACE_TYPE_HINT_POPUP_MENU:
    case GDK_SURFACE_TYPE_HINT_DROPDOWN_MENU:
    case GDK_SURFACE_TYPE_HINT_COMBO:
      return TRUE;

    default:
      break;
    }

  if (impl->position_method == POSITION_METHOD_MOVE_TO_RECT)
    return TRUE;

  return FALSE;
}

/* Get the surface that can be used as a parent for a popup, i.e. a xdg_toplevel
 * or xdg_popup. If the surface is not, traverse up the transiency parents until
 * we find one.
 */
static GdkSurface *
get_popup_parent (GdkSurface *surface)
{
  while (surface)
    {
      GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

      if (is_realized_popup (surface) || is_realized_toplevel (surface))
        return surface;

      surface = impl->transient_for;
    }

  return NULL;
}

static void
gdk_wayland_surface_map (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkSurface *transient_for = NULL;

  if (!should_be_mapped (surface))
    return;

  if (impl->mapped)
    return;

  if (should_map_as_popup (surface))
    {
      gboolean create_fallback = FALSE;
      GdkWaylandSeat *grab_input_seat;

      /* Popup menus can appear without a transient parent, which means they
       * cannot be positioned properly on Wayland. This attempts to guess the
       * surface they should be positioned with by finding the surface beneath
       * the device that created the grab for the popup surface.
       */
      if (!impl->transient_for && impl->hint == GDK_SURFACE_TYPE_HINT_POPUP_MENU)
        {
          GdkDevice *grab_device = NULL;

          /* The popup menu surface is not the grabbed surface. This may mean
           * that a "transfer surface" (see gtkmenu.c) is used, and we need
           * to find that surface to get the grab device. If so is the case
           * the "transfer surface" can be retrieved via the
           * "gdk-attached-grab-surface" associated data field.
           */
          if (!impl->grab_input_seat)
            {
              GdkSurface *attached_grab_surface =
                g_object_get_data (G_OBJECT (surface),
                                   "gdk-attached-grab-surface");
              if (attached_grab_surface)
                {
                  GdkWaylandSurface *attached_impl =
                    GDK_WAYLAND_SURFACE (attached_grab_surface);
                  grab_device = gdk_seat_get_pointer (attached_impl->grab_input_seat);
                  transient_for =
                    gdk_device_get_surface_at_position (grab_device, NULL, NULL);
                }
            }
          else
            {
              grab_device = gdk_seat_get_pointer (impl->grab_input_seat);
              transient_for =
                gdk_device_get_surface_at_position (grab_device, NULL, NULL);
            }

          if (transient_for)
            transient_for = get_popup_parent (transient_for);

          /* If the position was not explicitly set, start the popup at the
           * position of the device that holds the grab.
           */
          if (impl->position_method == POSITION_METHOD_NONE && grab_device)
            {
              double px, py;
              gdk_surface_get_device_position (transient_for, grab_device,
                                               &px, &py, NULL);
              surface->x = round (px);
              surface->y = round (py);
            }
        }
      else
        {
          transient_for = impl->transient_for;
          transient_for = get_popup_parent (transient_for);
        }

      if (!transient_for)
        {
          g_warning ("Couldn't map as surface %p as popup because it doesn't have a parent",
                     surface);

          create_fallback = TRUE;
        }
      else
        {
          grab_input_seat = find_grab_input_seat (surface, transient_for);
        }

      if (!create_fallback)
        {
          gdk_wayland_surface_create_xdg_popup (surface,
                                                transient_for,
                                                grab_input_seat);
        }
      else
        {
          gdk_wayland_surface_create_xdg_toplevel (surface);
        }
    }
  else
    {
      gdk_wayland_surface_create_xdg_toplevel (surface);
    }

  impl->mapped = TRUE;
}

static void
gdk_wayland_surface_show (GdkSurface *surface,
                          gboolean    already_mapped)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (!impl->display_server.wl_surface)
    gdk_wayland_surface_create_surface (surface);

  gdk_wayland_surface_map (surface);
}

static void
unmap_popups_for_surface (GdkSurface *surface)
{
  GdkWaylandDisplay *display_wayland;
  GList *l;

  display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  for (l = display_wayland->current_popups; l; l = l->next)
    {
       GdkSurface *popup = l->data;
       GdkWaylandSurface *popup_impl = GDK_WAYLAND_SURFACE (popup);

       if (popup_impl->popup_parent == surface)
         {
           g_warning ("Tried to unmap the parent of a popup");
           gdk_surface_hide (popup);

           return;
         }
    }
}

static void
gdk_wayland_surface_hide_surface (GdkSurface *surface)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  unmap_popups_for_surface (surface);

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
            g_list_remove (display_wayland->current_popups, surface);
        }
      if (impl->display_server.xdg_surface)
        {
          xdg_surface_destroy (impl->display_server.xdg_surface);
          impl->display_server.xdg_surface = NULL;
          if (!impl->initial_configure_received)
            gdk_surface_thaw_updates (surface);
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
            g_list_remove (display_wayland->current_popups, surface);
        }
      if (impl->display_server.zxdg_surface_v6)
        {
          zxdg_surface_v6_destroy (impl->display_server.zxdg_surface_v6);
          impl->display_server.zxdg_surface_v6 = NULL;
          if (!impl->initial_configure_received)
            gdk_surface_thaw_updates (surface);
          else
            impl->initial_configure_received = FALSE;
        }

      if (impl->awaiting_frame)
        {
          GdkFrameClock *frame_clock;

          impl->awaiting_frame = FALSE;
          frame_clock = gdk_surface_get_frame_clock (surface);
          if (frame_clock)
            _gdk_frame_clock_thaw (frame_clock);
        }

      if (impl->display_server.gtk_surface)
        {
          gtk_surface1_destroy (impl->display_server.gtk_surface);
          impl->display_server.gtk_surface = NULL;
          impl->application.was_set = FALSE;
        }

      wl_surface_destroy (impl->display_server.wl_surface);
      impl->display_server.wl_surface = NULL;

      g_slist_free (impl->display_server.outputs);
      impl->display_server.outputs = NULL;

      if (impl->hint == GDK_SURFACE_TYPE_HINT_DIALOG && !impl->transient_for)
        display_wayland->orphan_dialogs =
          g_list_remove (display_wayland->orphan_dialogs, surface);
    }

  unset_transient_for_exported (surface);

  _gdk_wayland_surface_clear_saved_size (surface);
  impl->pending_commit = FALSE;
  impl->mapped = FALSE;
}

static void
gdk_wayland_surface_hide (GdkSurface *surface)
{
  gdk_wayland_surface_hide_surface (surface);
  _gdk_surface_clear_update_area (surface);
}

static void
gdk_wayland_surface_withdraw (GdkSurface *surface)
{
  if (!surface->destroyed)
    {
      if (GDK_SURFACE_IS_MAPPED (surface))
        gdk_synthesize_surface_state (surface, 0, GDK_SURFACE_STATE_WITHDRAWN);

      g_assert (!GDK_SURFACE_IS_MAPPED (surface));

      gdk_wayland_surface_hide_surface (surface);
    }
}

static void
gdk_wayland_surface_raise (GdkSurface *surface)
{
}

static void
gdk_wayland_surface_lower (GdkSurface *surface)
{
}

static void
gdk_wayland_surface_restack_toplevel (GdkSurface *surface,
                                      GdkSurface *sibling,
                                      gboolean    above)
{
}

static void
gdk_wayland_surface_move_resize (GdkSurface *surface,
                                 gboolean    with_move,
                                 gint        x,
                                 gint        y,
                                 gint        width,
                                 gint        height)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (with_move)
    {
      /* Each toplevel has in its own "root" coordinate system */
      if (GDK_SURFACE_TYPE (surface) != GDK_SURFACE_TOPLEVEL)
        {
          surface->x = x;
          surface->y = y;
          impl->position_method = POSITION_METHOD_MOVE_RESIZE;
        }
    }

  /* If this function is called with width and height = -1 then that means
   * just move the surface - don't update its size
   */
  if (width > 0 && height > 0)
    gdk_wayland_surface_maybe_configure (surface, width, height, impl->scale);
}

/* Avoid zero width/height as this is a protocol error */
static void
sanitize_anchor_rect (GdkSurface   *surface,
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
gdk_wayland_surface_move_to_rect (GdkSurface         *surface,
                                  const GdkRectangle *rect,
                                  GdkGravity          rect_anchor,
                                  GdkGravity          surface_anchor,
                                  GdkAnchorHints      anchor_hints,
                                  gint                rect_anchor_dx,
                                  gint                rect_anchor_dy)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->pending_move_to_rect.rect = *rect;
  sanitize_anchor_rect (surface, &impl->pending_move_to_rect.rect);

  impl->pending_move_to_rect.rect_anchor = rect_anchor;
  impl->pending_move_to_rect.surface_anchor = surface_anchor;
  impl->pending_move_to_rect.anchor_hints = anchor_hints;
  impl->pending_move_to_rect.rect_anchor_dx = rect_anchor_dx;
  impl->pending_move_to_rect.rect_anchor_dy = rect_anchor_dy;

  impl->position_method = POSITION_METHOD_MOVE_TO_RECT;
}

static void
gdk_wayland_surface_get_geometry (GdkSurface *surface,
                                  gint       *x,
                                  gint       *y,
                                  gint       *width,
                                  gint       *height)
{
  if (!GDK_SURFACE_DESTROYED (surface))
    {
      if (x)
        *x = surface->x;
      if (y)
        *y = surface->y;
      if (width)
        *width = surface->width;
      if (height)
        *height = surface->height;
    }
}

static void
gdk_wayland_surface_get_root_coords (GdkSurface *surface,
                                     gint        x,
                                     gint        y,
                                     gint       *root_x,
                                     gint       *root_y)
{
  /*
   * Wayland does not have a global coordinate space shared between surfaces. In
   * fact, for regular toplevels, we have no idea where our surfaces are
   * positioned, relatively.
   *
   * However, there are some cases like popups and subsurfaces where we do have
   * some amount of control over the placement of our surface, and we can
   * semi-accurately control the x/y position of these surfaces, if they are
   * relative to another surface.
   *
   * To pretend we have something called a root coordinate space, assume all
   * parent-less surfaces are positioned in (0, 0), and all relative positioned
   * popups and subsurfaces are placed within this fake root coordinate space.
   *
   * For example a 200x200 large toplevel surface will have the position (0, 0).
   * If a popup positioned in the middle of the toplevel will have the fake
   * position (100,100). Furthermore, if a positioned is placed in the middle
   * that popup, will have the fake position (150,150), even though it has the
   * relative position (50,50). These three surfaces would make up one single
   * fake root coordinate space.
   */

  if (root_x)
    *root_x = surface->x + x;

  if (root_y)
    *root_y = surface->y + y;
}

static gboolean
gdk_wayland_surface_get_device_state (GdkSurface       *surface,
                                      GdkDevice        *device,
                                      gdouble          *x,
                                      gdouble          *y,
                                      GdkModifierType  *mask)
{
  gboolean return_val;

  g_return_val_if_fail (surface == NULL || GDK_IS_SURFACE (surface), FALSE);

  return_val = TRUE;

  if (!GDK_SURFACE_DESTROYED (surface))
    {
      GdkSurface *child;

      GDK_DEVICE_GET_CLASS (device)->query_state (device, surface,
                                                  &child,
                                                  NULL, NULL,
                                                  x, y, mask);
      return_val = (child != NULL);
    }

  return return_val;
}

static void
gdk_wayland_surface_input_shape_combine_region (GdkSurface           *surface,
                                                const cairo_region_t *shape_region,
                                                gint                  offset_x,
                                                gint                  offset_y)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
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
gdk_wayland_surface_destroy (GdkSurface *surface,
                             gboolean    foreign_destroy)
{
  GdkWaylandDisplay *display;
  GdkFrameClock *frame_clock;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  /* Wayland surfaces can't be externally destroyed; we may possibly
   * eventually want to use this path at display close-down
   */
  g_return_if_fail (!foreign_destroy);

  gdk_wayland_surface_hide_surface (surface);

  frame_clock = gdk_surface_get_frame_clock (surface);
  g_signal_handlers_disconnect_by_func (frame_clock, on_frame_clock_before_paint, surface);
  g_signal_handlers_disconnect_by_func (frame_clock, on_frame_clock_after_paint, surface);

  display = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  display->toplevels = g_list_remove (display->toplevels, surface);
}

static void
gdk_wayland_surface_focus (GdkSurface *surface,
                           guint32     timestamp)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (!impl->display_server.gtk_surface)
    return;

  /* We didn't have an event to fetch a time from, meaning we have nothing valid
   * to send. This should rather be translated to a 'needs-attention' request or
   * something.
   */
  if (timestamp == GDK_CURRENT_TIME)
    return;

  gtk_surface1_present (impl->display_server.gtk_surface, timestamp);
}

static void
gdk_wayland_surface_set_type_hint (GdkSurface         *surface,
                                   GdkSurfaceTypeHint  hint)
{
  GdkWaylandSurface *impl;

  impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  impl->hint = hint;
}

static GdkSurfaceTypeHint
gdk_wayland_surface_get_type_hint (GdkSurface *surface)
{
  GdkWaylandSurface *impl;

  if (GDK_SURFACE_DESTROYED (surface))
    return GDK_SURFACE_TYPE_HINT_NORMAL;

  impl = GDK_WAYLAND_SURFACE (surface);

  return impl->hint;
}

static void
gtk_surface_configure (void                *data,
                       struct gtk_surface1 *gtk_surface,
                       struct wl_array     *states)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkSurfaceState new_state = 0;
  uint32_t *p;

  wl_array_for_each (p, states)
    {
      uint32_t state = *p;

      switch (state)
        {
        case GTK_SURFACE1_STATE_TILED:
          new_state |= GDK_SURFACE_STATE_TILED;
          break;

        /* Since v2 */
        case GTK_SURFACE1_STATE_TILED_TOP:
          new_state |= (GDK_SURFACE_STATE_TILED | GDK_SURFACE_STATE_TOP_TILED);
          break;
        case GTK_SURFACE1_STATE_TILED_RIGHT:
          new_state |= (GDK_SURFACE_STATE_TILED | GDK_SURFACE_STATE_RIGHT_TILED);
          break;
        case GTK_SURFACE1_STATE_TILED_BOTTOM:
          new_state |= (GDK_SURFACE_STATE_TILED | GDK_SURFACE_STATE_BOTTOM_TILED);
          break;
        case GTK_SURFACE1_STATE_TILED_LEFT:
          new_state |= (GDK_SURFACE_STATE_TILED | GDK_SURFACE_STATE_LEFT_TILED);
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
  GdkSurface *surface = GDK_SURFACE (data);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkSurfaceState new_state = 0;
  uint32_t *p;

  wl_array_for_each (p, edge_constraints)
    {
      uint32_t constraint = *p;

      switch (constraint)
        {
        case GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_TOP:
          new_state |= GDK_SURFACE_STATE_TOP_RESIZABLE;
          break;
        case GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_RIGHT:
          new_state |= GDK_SURFACE_STATE_RIGHT_RESIZABLE;
          break;
        case GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_BOTTOM:
          new_state |= GDK_SURFACE_STATE_BOTTOM_RESIZABLE;
          break;
        case GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_LEFT:
          new_state |= GDK_SURFACE_STATE_LEFT_RESIZABLE;
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
gdk_wayland_surface_init_gtk_surface (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

  if (impl->display_server.gtk_surface != NULL)
    return;
  if (!is_realized_toplevel (surface))
    return;
  if (display->gtk_shell == NULL)
    return;

  impl->display_server.gtk_surface =
    gtk_shell1_get_gtk_surface (display->gtk_shell,
                                impl->display_server.wl_surface);
  gdk_surface_set_geometry_hints (surface,
                                 &impl->geometry_hints,
                                 impl->geometry_mask);
  gtk_surface1_add_listener (impl->display_server.gtk_surface,
                             &gtk_surface_listener,
                             surface);
}

static void
maybe_set_gtk_surface_modal (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  gdk_wayland_surface_init_gtk_surface (surface);
  if (impl->display_server.gtk_surface == NULL)
    return;

  if (surface->modal_hint)
    gtk_surface1_set_modal (impl->display_server.gtk_surface);
  else
    gtk_surface1_unset_modal (impl->display_server.gtk_surface);

}

static void
gdk_wayland_surface_set_modal_hint (GdkSurface *surface,
                                    gboolean    modal)
{
  surface->modal_hint = modal;
  maybe_set_gtk_surface_modal (surface);
}

static void
gdk_wayland_surface_set_geometry_hints (GdkSurface         *surface,
                                        const GdkGeometry  *geometry,
                                        GdkSurfaceHints     geom_mask)
{
  GdkWaylandDisplay *display_wayland;
  GdkWaylandSurface *impl;
  int min_width, min_height;
  int max_width, max_height;

  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

  impl = GDK_WAYLAND_SURFACE (surface);
  display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

  impl->geometry_hints = *geometry;
  impl->geometry_mask = geom_mask;

  if (!is_realized_toplevel (surface))
    return;

  if (geom_mask & GDK_HINT_MIN_SIZE)
    {
      min_width = MAX (0, (geometry->min_width -
                           (impl->margin_left + impl->margin_right)));
      min_height = MAX (0, (geometry->min_height -
                            (impl->margin_top + impl->margin_bottom)));
    }
  else
    {
      min_width = 0;
      min_height = 0;
    }

  if (geom_mask & GDK_HINT_MAX_SIZE)
    {
      max_width = MAX (0, (geometry->max_width -
                           (impl->margin_left + impl->margin_right)));
      max_height = MAX (0, (geometry->max_height -
                            (impl->margin_top + impl->margin_bottom)));
    }
  else
    {
      max_width = 0;
      max_height = 0;
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
    default:
      g_assert_not_reached ();
    }
}

static void
gdk_wayland_surface_set_title (GdkSurface  *surface,
                               const gchar *title)
{
  GdkWaylandSurface *impl;
  const char *end;
  gsize title_length;

  g_return_if_fail (title != NULL);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  impl = GDK_WAYLAND_SURFACE (surface);

  if (g_strcmp0 (impl->title, title) == 0)
    return;

  g_free (impl->title);

  title_length = MIN (strlen (title), MAX_WL_BUFFER_SIZE);
  if (g_utf8_validate (title, title_length, &end))
    {
      impl->title = g_malloc (end - title + 1);
      memcpy (impl->title, title, end - title);
      impl->title[end - title] = '\0';
    }
  else
    {
      impl->title = g_utf8_make_valid (title, title_length);
      g_warning ("Invalid utf8 passed to gdk_surface_set_title: '%s'", title);
    }

  gdk_wayland_surface_sync_title (surface);
}

static void
gdk_wayland_surface_set_startup_id (GdkSurface  *surface,
                                    const gchar *startup_id)
{
}

static gboolean
check_transient_for_loop (GdkSurface *surface,
                          GdkSurface *parent)
{
  while (parent)
    {
      GdkWaylandSurface *impl;

      if (!GDK_IS_WAYLAND_SURFACE (parent))
        return FALSE;

      impl = GDK_WAYLAND_SURFACE (parent);
      if (impl->transient_for == surface)
        return TRUE;
      parent = impl->transient_for;
    }
  return FALSE;
}

static void
gdk_wayland_surface_set_transient_for (GdkSurface *surface,
                                       GdkSurface *parent)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkSurface *previous_parent;

  g_assert (parent == NULL ||
            gdk_surface_get_display (surface) == gdk_surface_get_display (parent));

  if (check_transient_for_loop (surface, parent))
    {
      g_warning ("Setting %p transient for %p would create a loop", surface, parent);
      return;
    }

  unset_transient_for_exported (surface);

  previous_parent = impl->transient_for;
  impl->transient_for = parent;

  if (impl->hint == GDK_SURFACE_TYPE_HINT_DIALOG)
    {
      if (!parent)
        _gdk_wayland_screen_add_orphan_dialog (surface);
      else if (!previous_parent)
        display_wayland->orphan_dialogs =
          g_list_remove (display_wayland->orphan_dialogs, surface);
    }
  gdk_wayland_surface_sync_parent (surface, NULL);
}

static void
gdk_wayland_surface_get_frame_extents (GdkSurface   *surface,
                                       GdkRectangle *rect)
{
  *rect = (GdkRectangle) {
    .x = surface->x,
    .y = surface->y,
    .width = surface->width,
    .height = surface->height
  };
}

static void
gdk_wayland_surface_set_accept_focus (GdkSurface *surface,
                                      gboolean    accept_focus)
{
}

static void
gdk_wayland_surface_set_focus_on_map (GdkSurface *surface,
                                      gboolean    focus_on_map)
{
}

static void
gdk_wayland_surface_set_icon_list (GdkSurface *surface,
                                   GList      *surfaces)
{
}

static void
gdk_wayland_surface_set_icon_name (GdkSurface  *surface,
                                   const gchar *name)
{
  if (GDK_SURFACE_DESTROYED (surface))
    return;
}

static void
gdk_wayland_surface_iconify (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland;

  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

  if (!is_realized_toplevel (surface))
    return;

  display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  switch (display_wayland->shell_variant)
    {
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
      xdg_toplevel_set_minimized (impl->display_server.xdg_toplevel);
      break;
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
      zxdg_toplevel_v6_set_minimized (impl->display_server.zxdg_toplevel_v6);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
gdk_wayland_surface_deiconify (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

  if (GDK_SURFACE_IS_MAPPED (surface))
    gdk_surface_show (surface);
  else
    /* Flip our client side flag, the real work happens on map. */
    gdk_synthesize_surface_state (surface, GDK_SURFACE_STATE_ICONIFIED, 0);
}

static void
gdk_wayland_surface_stick (GdkSurface *surface)
{
}

static void
gdk_wayland_surface_unstick (GdkSurface *surface)
{
}

static void
gdk_wayland_surface_maximize (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  _gdk_wayland_surface_save_size (surface);

  if (is_realized_toplevel (surface))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_set_maximized (impl->display_server.xdg_toplevel);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_set_maximized (impl->display_server.zxdg_toplevel_v6);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      gdk_synthesize_surface_state (surface, 0, GDK_SURFACE_STATE_MAXIMIZED);
    }
}

static void
gdk_wayland_surface_unmaximize (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (is_realized_toplevel (surface))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_unset_maximized (impl->display_server.xdg_toplevel);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_unset_maximized (impl->display_server.zxdg_toplevel_v6);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      gdk_synthesize_surface_state (surface, GDK_SURFACE_STATE_MAXIMIZED, 0);
    }
}

static void
gdk_wayland_surface_fullscreen_on_monitor (GdkSurface *surface,
                                           GdkMonitor *monitor)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct wl_output *output = ((GdkWaylandMonitor *)monitor)->output;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  _gdk_wayland_surface_save_size (surface);

  if (is_realized_toplevel (surface))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_set_fullscreen (impl->display_server.xdg_toplevel,
                                       output);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_set_fullscreen (impl->display_server.zxdg_toplevel_v6,
                                           output);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      gdk_synthesize_surface_state (surface, 0, GDK_SURFACE_STATE_FULLSCREEN);
      impl->initial_fullscreen_output = output;
    }
}

static void
gdk_wayland_surface_fullscreen (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  impl->initial_fullscreen_output = NULL;

  _gdk_wayland_surface_save_size (surface);

  if (is_realized_toplevel (surface))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

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
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      gdk_synthesize_surface_state (surface, 0, GDK_SURFACE_STATE_FULLSCREEN);
    }
}

static void
gdk_wayland_surface_unfullscreen (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  impl->initial_fullscreen_output = NULL;

  if (is_realized_toplevel (surface))
    {
      GdkWaylandDisplay *display_wayland =
        GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

      switch (display_wayland->shell_variant)
        {
        case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL:
          xdg_toplevel_unset_fullscreen (impl->display_server.xdg_toplevel);
          break;
        case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6:
          zxdg_toplevel_v6_unset_fullscreen (impl->display_server.zxdg_toplevel_v6);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      gdk_synthesize_surface_state (surface, GDK_SURFACE_STATE_FULLSCREEN, 0);
    }
}

static void
gdk_wayland_surface_set_keep_above (GdkSurface *surface,
                                    gboolean    setting)
{
}

static void
gdk_wayland_surface_set_keep_below (GdkSurface *surface,
                                    gboolean    setting)
{
}

static void
gdk_wayland_surface_set_decorations (GdkSurface      *surface,
                                     GdkWMDecoration  decorations)
{
}

static gboolean
gdk_wayland_surface_get_decorations (GdkSurface      *surface,
                                     GdkWMDecoration *decorations)
{
  return FALSE;
}

static void
gdk_wayland_surface_set_functions (GdkSurface    *surface,
                                   GdkWMFunction  functions)
{
}

static void
gdk_wayland_surface_begin_resize_drag (GdkSurface     *surface,
                                       GdkSurfaceEdge  edge,
                                       GdkDevice      *device,
                                       gint            button,
                                       gint            x,
                                       gint            y,
                                       guint32         timestamp)
{
  GdkWaylandSurface *impl;
  GdkWaylandDisplay *display_wayland;
  GdkEventSequence *sequence;
  uint32_t resize_edges, serial;

  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

  switch (edge)
    {
    case GDK_SURFACE_EDGE_NORTH_WEST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_LEFT;
      break;

    case GDK_SURFACE_EDGE_NORTH:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP;
      break;

    case GDK_SURFACE_EDGE_NORTH_EAST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_RIGHT;
      break;

    case GDK_SURFACE_EDGE_WEST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_LEFT;
      break;

    case GDK_SURFACE_EDGE_EAST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_RIGHT;
      break;

    case GDK_SURFACE_EDGE_SOUTH_WEST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_LEFT;
      break;

    case GDK_SURFACE_EDGE_SOUTH:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM;
      break;

    case GDK_SURFACE_EDGE_SOUTH_EAST:
      resize_edges = ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_RIGHT;
      break;

    default:
      g_warning ("gdk_surface_begin_resize_drag: bad resize edge %d!", edge);
      return;
    }

  impl = GDK_WAYLAND_SURFACE (surface);
  display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

  if (!is_realized_toplevel (surface))
    return;

  serial = _gdk_wayland_seat_get_last_implicit_grab_serial (GDK_WAYLAND_SEAT (gdk_device_get_seat (device)),
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
    default:
      g_assert_not_reached ();
    }

  if (sequence)
    gdk_wayland_device_unset_touch_grab (device, sequence);

  /* This is needed since Wayland will absorb all the pointer events after the
   * above function - FIXME: Is this always safe..?
   */
  gdk_seat_ungrab (gdk_device_get_seat (device));
}

static void
gdk_wayland_surface_begin_move_drag (GdkSurface *surface,
                                     GdkDevice  *device,
                                     gint        button,
                                     gint        x,
                                     gint        y,
                                     guint32     timestamp)
{
  GdkWaylandSurface *impl;
  GdkWaylandDisplay *display_wayland;
  GdkEventSequence *sequence;
  uint32_t serial;

  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

  impl = GDK_WAYLAND_SURFACE (surface);
  display_wayland = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));

  if (!is_realized_toplevel (surface))
    return;

  serial = _gdk_wayland_seat_get_last_implicit_grab_serial (GDK_WAYLAND_SEAT (gdk_device_get_seat (device)),
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
    default:
      g_assert_not_reached ();
    }

  if (sequence)
    gdk_wayland_device_unset_touch_grab (device, sequence);

  /* This is needed since Wayland will absorb all the pointer events after the
   * above function - FIXME: Is this always safe..?
   */
  gdk_seat_ungrab (gdk_device_get_seat (device));
}

static void
gdk_wayland_surface_set_opacity (GdkSurface *surface,
                                 gdouble     opacity)
{
}

static void
gdk_wayland_surface_destroy_notify (GdkSurface *surface)
{
  if (!GDK_SURFACE_DESTROYED (surface))
    {
      g_warning ("GdkSurface %p unexpectedly destroyed", surface);
      _gdk_surface_destroy (surface, TRUE);
    }

  g_object_unref (surface);
}

static gint
gdk_wayland_surface_get_scale_factor (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return 1;

  return impl->scale;
}

static void
gdk_wayland_surface_set_opaque_region (GdkSurface     *surface,
                                       cairo_region_t *region)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  g_clear_pointer (&impl->opaque_region, cairo_region_destroy);
  impl->opaque_region = cairo_region_reference (region);
  impl->opaque_region_dirty = TRUE;
}

static void
gdk_wayland_surface_set_shadow_width (GdkSurface *surface,
                                      int         left,
                                      int         right,
                                      int         top,
                                      int         bottom)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  gint new_width, new_height;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  /* Reconfigure surface to keep the same surface geometry */
  new_width = surface->width -
    (impl->margin_left + impl->margin_right) + (left + right);
  new_height = surface->height -
    (impl->margin_top + impl->margin_bottom) + (top + bottom);
  gdk_wayland_surface_maybe_configure (surface, new_width, new_height, impl->scale);

  impl->margin_left = left;
  impl->margin_right = right;
  impl->margin_top = top;
  impl->margin_bottom = bottom;
}

static gboolean
gdk_wayland_surface_show_window_menu (GdkSurface *surface,
                                      GdkEvent   *event)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  struct wl_seat *seat;
  GdkWaylandDevice *device;
  double x, y;
  uint32_t serial;

  switch ((guint) event->any.type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_END:
      break;
    default:
      return FALSE;
    }

  if (!is_realized_toplevel (surface))
    return FALSE;

  device = GDK_WAYLAND_DEVICE (gdk_event_get_device (event));
  seat = gdk_wayland_device_get_wl_seat (GDK_DEVICE (device));
  gdk_event_get_coords (event, &x, &y);

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
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static gboolean
gdk_wayland_surface_supports_edge_constraints (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct gtk_surface1 *gtk_surface = impl->display_server.gtk_surface;

  if (!gtk_surface)
    return FALSE;

  return gtk_surface1_get_version (gtk_surface) >= GTK_SURFACE1_CONFIGURE_EDGES_SINCE_VERSION;
}

static void
gdk_wayland_surface_class_init (GdkWaylandSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceClass *impl_class = GDK_SURFACE_CLASS (klass);

  object_class->finalize = gdk_wayland_surface_finalize;

  impl_class->show = gdk_wayland_surface_show;
  impl_class->hide = gdk_wayland_surface_hide;
  impl_class->withdraw = gdk_wayland_surface_withdraw;
  impl_class->raise = gdk_wayland_surface_raise;
  impl_class->lower = gdk_wayland_surface_lower;
  impl_class->restack_toplevel = gdk_wayland_surface_restack_toplevel;
  impl_class->move_resize = gdk_wayland_surface_move_resize;
  impl_class->move_to_rect = gdk_wayland_surface_move_to_rect;
  impl_class->get_geometry = gdk_wayland_surface_get_geometry;
  impl_class->get_root_coords = gdk_wayland_surface_get_root_coords;
  impl_class->get_device_state = gdk_wayland_surface_get_device_state;
  impl_class->input_shape_combine_region = gdk_wayland_surface_input_shape_combine_region;
  impl_class->destroy = gdk_wayland_surface_destroy;
  impl_class->beep = gdk_wayland_surface_beep;

  impl_class->focus = gdk_wayland_surface_focus;
  impl_class->set_type_hint = gdk_wayland_surface_set_type_hint;
  impl_class->get_type_hint = gdk_wayland_surface_get_type_hint;
  impl_class->set_modal_hint = gdk_wayland_surface_set_modal_hint;
  impl_class->set_geometry_hints = gdk_wayland_surface_set_geometry_hints;
  impl_class->set_title = gdk_wayland_surface_set_title;
  impl_class->set_startup_id = gdk_wayland_surface_set_startup_id;
  impl_class->set_transient_for = gdk_wayland_surface_set_transient_for;
  impl_class->get_frame_extents = gdk_wayland_surface_get_frame_extents;
  impl_class->set_accept_focus = gdk_wayland_surface_set_accept_focus;
  impl_class->set_focus_on_map = gdk_wayland_surface_set_focus_on_map;
  impl_class->set_icon_list = gdk_wayland_surface_set_icon_list;
  impl_class->set_icon_name = gdk_wayland_surface_set_icon_name;
  impl_class->iconify = gdk_wayland_surface_iconify;
  impl_class->deiconify = gdk_wayland_surface_deiconify;
  impl_class->stick = gdk_wayland_surface_stick;
  impl_class->unstick = gdk_wayland_surface_unstick;
  impl_class->maximize = gdk_wayland_surface_maximize;
  impl_class->unmaximize = gdk_wayland_surface_unmaximize;
  impl_class->fullscreen = gdk_wayland_surface_fullscreen;
  impl_class->fullscreen_on_monitor = gdk_wayland_surface_fullscreen_on_monitor;
  impl_class->unfullscreen = gdk_wayland_surface_unfullscreen;
  impl_class->set_keep_above = gdk_wayland_surface_set_keep_above;
  impl_class->set_keep_below = gdk_wayland_surface_set_keep_below;
  impl_class->set_decorations = gdk_wayland_surface_set_decorations;
  impl_class->get_decorations = gdk_wayland_surface_get_decorations;
  impl_class->set_functions = gdk_wayland_surface_set_functions;
  impl_class->begin_resize_drag = gdk_wayland_surface_begin_resize_drag;
  impl_class->begin_move_drag = gdk_wayland_surface_begin_move_drag;
  impl_class->set_opacity = gdk_wayland_surface_set_opacity;
  impl_class->destroy_notify = gdk_wayland_surface_destroy_notify;
  impl_class->register_dnd = _gdk_wayland_surface_register_dnd;
  impl_class->drag_begin = _gdk_wayland_surface_drag_begin;
  impl_class->get_scale_factor = gdk_wayland_surface_get_scale_factor;
  impl_class->set_opaque_region = gdk_wayland_surface_set_opaque_region;
  impl_class->set_shadow_width = gdk_wayland_surface_set_shadow_width;
  impl_class->show_window_menu = gdk_wayland_surface_show_window_menu;
  impl_class->create_gl_context = gdk_wayland_surface_create_gl_context;
  impl_class->supports_edge_constraints = gdk_wayland_surface_supports_edge_constraints;

  signals[COMMITTED] = g_signal_new (g_intern_static_string ("committed"),
                                     G_TYPE_FROM_CLASS (object_class),
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL, NULL,
                                     G_TYPE_NONE, 0);
}

void
_gdk_wayland_surface_set_grab_seat (GdkSurface *surface,
                                    GdkSeat    *seat)
{
  GdkWaylandSurface *impl;

  g_return_if_fail (surface != NULL);

  impl = GDK_WAYLAND_SURFACE (surface);
  impl->grab_input_seat = seat;
}

/**
 * gdk_wayland_surface_get_wl_surface:
 * @surface: (type GdkWaylandSurface): a #GdkSurface
 *
 * Returns the Wayland surface of a #GdkSurface.
 *
 * Returns: (transfer none): a Wayland wl_surface
 */
struct wl_surface *
gdk_wayland_surface_get_wl_surface (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_SURFACE (surface), NULL);

  return GDK_WAYLAND_SURFACE (surface)->display_server.wl_surface;
}

struct wl_output *
gdk_wayland_surface_get_wl_output (GdkSurface *surface)
{
  GdkWaylandSurface *impl;

  g_return_val_if_fail (GDK_IS_WAYLAND_SURFACE (surface), NULL);

  impl = GDK_WAYLAND_SURFACE (surface);
  /* We pick the head of the list as this is the last entered output */
  if (impl->display_server.outputs)
    return (struct wl_output *) impl->display_server.outputs->data;

  return NULL;
}

static struct wl_egl_window *
gdk_wayland_surface_get_wl_egl_window (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (impl->display_server.egl_window == NULL)
    {
      impl->display_server.egl_window =
        wl_egl_window_create (impl->display_server.wl_surface,
                              surface->width * impl->scale,
                              surface->height * impl->scale);
      wl_surface_set_buffer_scale (impl->display_server.wl_surface, impl->scale);
    }

  return impl->display_server.egl_window;
}

EGLSurface
gdk_wayland_surface_get_egl_surface (GdkSurface *surface,
                                     EGLConfig   config)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *impl;
  struct wl_egl_window *egl_window;

  g_return_val_if_fail (GDK_IS_WAYLAND_SURFACE (surface), NULL);

  impl = GDK_WAYLAND_SURFACE (surface);

  if (impl->egl_surface == NULL)
    {
      egl_window = gdk_wayland_surface_get_wl_egl_window (surface);

      impl->egl_surface =
        eglCreateWindowSurface (display->egl_display, config, egl_window, NULL);
    }

  return impl->egl_surface;
}

EGLSurface
gdk_wayland_surface_get_dummy_egl_surface (GdkSurface *surface,
                                           EGLConfig   config)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  GdkWaylandSurface *impl;

  g_return_val_if_fail (GDK_IS_WAYLAND_SURFACE (surface), NULL);

  impl = GDK_WAYLAND_SURFACE (surface);

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
gdk_wayland_surface_get_gtk_surface (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_SURFACE (surface), NULL);

  return GDK_WAYLAND_SURFACE (surface)->display_server.gtk_surface;
}

static void
maybe_set_gtk_surface_dbus_properties (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (impl->application.was_set)
    return;

  if (impl->application.application_id == NULL &&
      impl->application.app_menu_path == NULL &&
      impl->application.menubar_path == NULL &&
      impl->application.window_object_path == NULL &&
      impl->application.application_object_path == NULL &&
      impl->application.unique_bus_name == NULL)
    return;

  gdk_wayland_surface_init_gtk_surface (surface);
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
gdk_wayland_surface_set_dbus_properties_libgtk_only (GdkSurface *surface,
                                                     const char *application_id,
                                                     const char *app_menu_path,
                                                     const char *menubar_path,
                                                     const char *window_object_path,
                                                     const char *application_object_path,
                                                     const char *unique_bus_name)
{
  GdkWaylandSurface *impl;

  g_return_if_fail (GDK_IS_WAYLAND_SURFACE (surface));

  impl = GDK_WAYLAND_SURFACE (surface);

  impl->application.application_id = g_strdup (application_id);
  impl->application.app_menu_path = g_strdup (app_menu_path);
  impl->application.menubar_path = g_strdup (menubar_path);
  impl->application.window_object_path = g_strdup (window_object_path);
  impl->application.application_object_path =
    g_strdup (application_object_path);
  impl->application.unique_bus_name = g_strdup (unique_bus_name);

  maybe_set_gtk_surface_dbus_properties (surface);
}

void
_gdk_wayland_surface_offset_next_wl_buffer (GdkSurface *surface,
                                            int         x,
                                            int         y)
{
  GdkWaylandSurface *impl;

  g_return_if_fail (GDK_IS_WAYLAND_SURFACE (surface));

  impl = GDK_WAYLAND_SURFACE (surface);

  impl->pending_buffer_offset_x = x;
  impl->pending_buffer_offset_y = y;
}

static void
xdg_exported_handle (void                    *data,
                     struct zxdg_exported_v1 *zxdg_exported_v1,
                     const char              *handle)
{
  GdkSurface *surface = data;
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  impl->exported.callback (surface, handle, impl->exported.user_data);
  g_clear_pointer (&impl->exported.user_data,
                   impl->exported.destroy_func);
}

static const struct zxdg_exported_v1_listener xdg_exported_listener = {
  xdg_exported_handle
};

/**
 * GdkWaylandSurfaceExported:
 * @surface: the #GdkSurface that is exported
 * @handle: the handle
 * @user_data: user data that was passed to gdk_wayland_surface_export_handle()
 *
 * Callback that gets called when the handle for a surface has been
 * obtained from the Wayland compositor. The handle can be passed
 * to other processes, for the purpose of marking surfaces as transient
 * for out-of-process surfaces.
 */

static gboolean
gdk_wayland_surface_is_exported (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  return !!impl->display_server.xdg_exported;
}

/**
 * gdk_wayland_surface_export_handle:
 * @surface: the #GdkSurface to obtain a handle for
 * @callback: callback to call with the handle
 * @user_data: user data for @callback
 * @destroy_func: destroy notify for @user_data
 *
 * Asynchronously obtains a handle for a surface that can be passed
 * to other processes. When the handle has been obtained, @callback
 * will be called.
 *
 * It is an error to call this function on a surface that is already
 * exported.
 *
 * When the handle is no longer needed, gdk_wayland_surface_unexport_handle()
 * should be called to clean up resources.
 *
 * The main purpose for obtaining a handle is to mark a surface
 * from another surface as transient for this one, see
 * gdk_wayland_surface_set_transient_for_exported().
 *
 * Note that this API depends on an unstable Wayland protocol,
 * and thus may require changes in the future.
 *
 * Return value: %TRUE if the handle has been requested, %FALSE if
 *     an error occurred.
 */
gboolean
gdk_wayland_surface_export_handle (GdkSurface                *surface,
                                   GdkWaylandSurfaceExported  callback,
                                   gpointer                   user_data,
                                   GDestroyNotify             destroy_func)
{
  GdkWaylandSurface *impl;
  GdkWaylandDisplay *display_wayland;
  GdkDisplay *display = gdk_surface_get_display (surface);
  struct zxdg_exported_v1 *xdg_exported;

  g_return_val_if_fail (GDK_IS_WAYLAND_SURFACE (surface), FALSE);
  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY (display), FALSE);

  impl = GDK_WAYLAND_SURFACE (surface);
  display_wayland = GDK_WAYLAND_DISPLAY (display);

  g_return_val_if_fail (!impl->display_server.xdg_exported, FALSE);

  if (!display_wayland->xdg_exporter)
    {
      g_warning ("Server is missing xdg_foreign support");
      return FALSE;
    }

  xdg_exported = zxdg_exporter_v1_export (display_wayland->xdg_exporter,
                                          impl->display_server.wl_surface);
  zxdg_exported_v1_add_listener (xdg_exported,  &xdg_exported_listener, surface);

  impl->display_server.xdg_exported = xdg_exported;
  impl->exported.callback = callback;
  impl->exported.user_data = user_data;
  impl->exported.destroy_func = destroy_func;

  return TRUE;
}

/**
 * gdk_wayland_surface_unexport_handle:
 * @surface: the #GdkSurface to unexport
 *
 * Destroys the handle that was obtained with
 * gdk_wayland_surface_export_handle().
 *
 * It is an error to call this function on a surface that
 * does not have a handle.
 *
 * Note that this API depends on an unstable Wayland protocol,
 * and thus may require changes in the future.
 */
void
gdk_wayland_surface_unexport_handle (GdkSurface *surface)
{
  GdkWaylandSurface *impl;

  g_return_if_fail (GDK_IS_WAYLAND_SURFACE (surface));

  impl = GDK_WAYLAND_SURFACE (surface);

  g_return_if_fail (impl->display_server.xdg_exported);

  g_clear_pointer (&impl->display_server.xdg_exported,
                   zxdg_exported_v1_destroy);
  g_clear_pointer (&impl->exported.user_data,
                   impl->exported.destroy_func);
}

static void
unset_transient_for_exported (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  g_clear_pointer (&impl->imported_transient_for, zxdg_imported_v1_destroy);
}

static void
xdg_imported_destroyed (void                    *data,
                        struct zxdg_imported_v1 *zxdg_imported_v1)
{
  GdkSurface *surface = data;

  unset_transient_for_exported (surface);
}

static const struct zxdg_imported_v1_listener xdg_imported_listener = {
  xdg_imported_destroyed,
};

/**
 * gdk_wayland_surface_set_transient_for_exported:
 * @surface: the #GdkSurface to make as transient
 * @parent_handle_str: an exported handle for a surface
 *
 * Marks @surface as transient for the surface to which the given
 * @parent_handle_str refers. Typically, the handle will originate
 * from a gdk_wayland_surface_export_handle() call in another process.
 *
 * Note that this API depends on an unstable Wayland protocol,
 * and thus may require changes in the future.
 *
 * Return value: %TRUE if the surface has been marked as transient,
 *     %FALSE if an error occurred.
 */
gboolean
gdk_wayland_surface_set_transient_for_exported (GdkSurface *surface,
                                                char       *parent_handle_str)
{
  GdkWaylandSurface *impl;
  GdkWaylandDisplay *display_wayland;
  GdkDisplay *display = gdk_surface_get_display (surface);

  g_return_val_if_fail (GDK_IS_WAYLAND_SURFACE (surface), FALSE);
  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY (display), FALSE);
  g_return_val_if_fail (!should_map_as_popup (surface), FALSE);

  impl = GDK_WAYLAND_SURFACE (surface);
  display_wayland = GDK_WAYLAND_DISPLAY (display);

  if (!display_wayland->xdg_importer)
    {
      g_warning ("Server is missing xdg_foreign support");
      return FALSE;
    }

  gdk_surface_set_transient_for (surface, NULL);

  impl->imported_transient_for =
    zxdg_importer_v1_import (display_wayland->xdg_importer, parent_handle_str);
  zxdg_imported_v1_add_listener (impl->imported_transient_for,
                                 &xdg_imported_listener,
                                 surface);

  gdk_wayland_surface_sync_parent_of_imported (surface);

  return TRUE;
}

static struct zwp_keyboard_shortcuts_inhibitor_v1 *
gdk_wayland_surface_get_inhibitor (GdkWaylandSurface *impl,
                                  struct wl_seat *seat)
{
  return g_hash_table_lookup (impl->shortcuts_inhibitors, seat);
}

void
gdk_wayland_surface_inhibit_shortcuts (GdkSurface *surface,
                                       GdkSeat    *gdk_seat)
{
  GdkWaylandSurface *impl= GDK_WAYLAND_SURFACE (surface);
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (gdk_surface_get_display (surface));
  struct wl_surface *wl_surface = impl->display_server.wl_surface;
  struct wl_seat *seat = gdk_wayland_seat_get_wl_seat (gdk_seat);
  struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor;

  if (display->keyboard_shortcuts_inhibit == NULL)
    return;

  if (gdk_wayland_surface_get_inhibitor (impl, seat))
    return; /* Already inhibitted */

  inhibitor =
      zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts (
          display->keyboard_shortcuts_inhibit, wl_surface, seat);

  g_hash_table_insert (impl->shortcuts_inhibitors, seat, inhibitor);
}

void
gdk_wayland_surface_restore_shortcuts (GdkSurface *surface,
                                       GdkSeat    *gdk_seat)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);
  struct wl_seat *seat = gdk_wayland_seat_get_wl_seat (gdk_seat);
  struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor;

  inhibitor = gdk_wayland_surface_get_inhibitor (impl, seat);
  if (inhibitor == NULL)
    return; /* Not inhibitted */

  zwp_keyboard_shortcuts_inhibitor_v1_destroy (inhibitor);
  g_hash_table_remove (impl->shortcuts_inhibitors, seat);
}

