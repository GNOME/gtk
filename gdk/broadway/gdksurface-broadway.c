/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-2007 Peter Mattis, Spencer Kimball,
 * Josh MacDonald, Ryan Lortie
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gdksurface-broadway.h"

#include "gdkbroadwaydisplay.h"
#include "gdkdisplay.h"
#include "gdksurface.h"
#include "gdksurfaceimpl.h"
#include "gdkdisplay-broadway.h"
#include "gdkprivate-broadway.h"
#include "gdkinternals.h"
#include "gdkdeviceprivate.h"
#include "gdkeventsource.h"
#include <gdk/gdktextureprivate.h>
#include <gdk/gdkframeclockprivate.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Forward declarations */
static void        gdk_surface_impl_broadway_finalize   (GObject            *object);

#define SURFACE_IS_TOPLEVEL(surface)  TRUE

struct _GdkBroadwaySurface {
  GdkSurface parent;
};

struct _GdkBroadwaySurfaceClass {
  GdkSurfaceClass parent_class;
};

G_DEFINE_TYPE (GdkBroadwaySurface, gdk_broadway_surface, GDK_TYPE_SURFACE)

static void
gdk_broadway_surface_class_init (GdkBroadwaySurfaceClass *broadway_surface_class)
{
}

static void
gdk_broadway_surface_init (GdkBroadwaySurface *broadway_surface)
{
}

G_DEFINE_TYPE (GdkSurfaceImplBroadway,
               gdk_surface_impl_broadway,
               GDK_TYPE_SURFACE_IMPL)


/* We need to flush in an idle rather than AFTER_PAINT, as the clock
   is frozen during e.g. surface resizes so the paint will not happen
   and the surface resize request is never flushed. */
static void
queue_flush (GdkSurface *surface)
{
  gdk_broadway_display_flush_in_idle (gdk_surface_get_display (surface));
}

static void
gdk_surface_impl_broadway_init (GdkSurfaceImplBroadway *impl)
{
}

static void
gdk_surface_impl_broadway_finalize (GObject *object)
{
  GdkSurface *wrapper;
  GdkSurfaceImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;

  g_return_if_fail (GDK_IS_SURFACE_IMPL_BROADWAY (object));

  impl = GDK_SURFACE_IMPL_BROADWAY (object);

  wrapper = impl->wrapper;

  _gdk_broadway_surface_grab_check_destroy (wrapper);

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (impl->wrapper));

  g_hash_table_remove (broadway_display->id_ht, GINT_TO_POINTER(impl->id));

  if (impl->cursor)
    g_object_unref (impl->cursor);

  broadway_display->toplevels = g_list_remove (broadway_display->toplevels, impl);

  G_OBJECT_CLASS (gdk_surface_impl_broadway_parent_class)->finalize (object);
}

static gboolean
thaw_clock_cb (GdkFrameClock *clock)
{
  _gdk_frame_clock_thaw (clock);
  g_object_unref (clock);
  return G_SOURCE_REMOVE;
}

void
_gdk_broadway_roundtrip_notify (GdkSurface  *surface,
                                guint32 tag,
                                gboolean local_reply)
{
  GdkSurfaceImplBroadway *impl = GDK_SURFACE_IMPL_BROADWAY (surface->impl);
  GdkFrameClock *clock = gdk_surface_get_frame_clock (surface);
  GdkFrameTimings *timings;

  timings = gdk_frame_clock_get_timings (clock, impl->pending_frame_counter);
  impl->pending_frame_counter = 0;

  /* If there is no remote web client, rate limit update to once a second */
  if (local_reply)
    g_timeout_add_seconds (1, (GSourceFunc)thaw_clock_cb, g_object_ref (clock));
  else
    _gdk_frame_clock_thaw (clock);

  if (timings)
    {
      timings->refresh_interval = 33333; /* default to 1/30th of a second */
      // This isn't quite right, since we've done a rountrip back too, can we do better?
      timings->presentation_time = g_get_monotonic_time ();
      timings->complete = TRUE;


#ifdef G_ENABLE_DEBUG
      if ((_gdk_debug_flags & GDK_DEBUG_FRAMES) != 0)
        _gdk_frame_clock_debug_print_timings (clock, timings);

  if (gdk_profiler_is_running ())
    _gdk_frame_clock_add_timings_to_profiler (clock, timings);
#endif
    }
}

static void
on_frame_clock_after_paint (GdkFrameClock *clock,
                            GdkSurface     *surface)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  GdkSurfaceImplBroadway *impl = GDK_SURFACE_IMPL_BROADWAY (surface->impl);
  GdkBroadwayDisplay *broadway_display;

  impl->pending_frame_counter = gdk_frame_clock_get_frame_counter (clock);
  _gdk_frame_clock_freeze (gdk_surface_get_frame_clock (surface));

  broadway_display = GDK_BROADWAY_DISPLAY (display);

  _gdk_broadway_server_roundtrip (broadway_display->server, impl->id, _gdk_display_get_next_serial (display));

  gdk_display_flush (display);
}

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
      timings->predicted_presentation_time = presentation_time + refresh_interval;
    }
  else
    {
      timings->predicted_presentation_time = timings->frame_time + refresh_interval / 2 + refresh_interval;
    }
}

static void
connect_frame_clock (GdkSurface *surface)
{
  if (SURFACE_IS_TOPLEVEL (surface))
    {
      GdkFrameClock *frame_clock = gdk_surface_get_frame_clock (surface);

      g_signal_connect (frame_clock, "before-paint",
                        G_CALLBACK (on_frame_clock_before_paint), surface);
      g_signal_connect (frame_clock, "after-paint",
                        G_CALLBACK (on_frame_clock_after_paint), surface);
    }
}

void
_gdk_broadway_display_create_surface_impl (GdkDisplay    *display,
                                           GdkSurface     *surface,
                                           GdkSurface     *real_parent,
                                           GdkSurfaceAttr *attributes)
{
  GdkSurfaceImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;

  broadway_display = GDK_BROADWAY_DISPLAY (display);

  impl = g_object_new (GDK_TYPE_SURFACE_IMPL_BROADWAY, NULL);
  surface->impl = (GdkSurfaceImpl *)impl;
  impl->id = _gdk_broadway_server_new_surface (broadway_display->server,
                                               surface->x,
                                               surface->y,
                                               surface->width,
                                               surface->height,
                                               surface->surface_type == GDK_SURFACE_TEMP);
  g_hash_table_insert (broadway_display->id_ht, GINT_TO_POINTER(impl->id), surface);
  impl->wrapper = surface;

  g_assert (surface->surface_type == GDK_SURFACE_TOPLEVEL ||
            surface->surface_type == GDK_SURFACE_TEMP);

  broadway_display->toplevels = g_list_prepend (broadway_display->toplevels, impl);

  connect_frame_clock (surface);
}

static cairo_surface_t *
gdk_surface_broadway_ref_cairo_surface (GdkSurface *surface)
{
  GdkSurfaceImplBroadway *impl = GDK_SURFACE_IMPL_BROADWAY (surface->impl);

  if (GDK_IS_SURFACE_IMPL_BROADWAY (surface) &&
      GDK_SURFACE_DESTROYED (impl->wrapper))
    return NULL;

  return cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1, 1);
}

static void
_gdk_broadway_surface_destroy (GdkSurface *surface,
                               gboolean   recursing,
                               gboolean   foreign_destroy)
{
  GdkSurfaceImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  impl = GDK_SURFACE_IMPL_BROADWAY (surface->impl);

  if (impl->node_data)
    g_array_unref (impl->node_data);
  if (impl->node_data_textures)
    g_ptr_array_unref (impl->node_data_textures);

  _gdk_broadway_surface_grab_check_destroy (surface);

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (surface));
  g_hash_table_remove (broadway_display->id_ht, GINT_TO_POINTER (impl->id));

  _gdk_broadway_server_destroy_surface (broadway_display->server, impl->id);

}

void
gdk_broadway_surface_set_nodes (GdkSurface *surface,
                                GArray *nodes,
                                GPtrArray *node_textures)
{
  GdkSurfaceImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  impl = GDK_SURFACE_IMPL_BROADWAY (surface->impl);

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (surface));

  if (nodes)
    g_array_ref (nodes);
  if (impl->node_data)
    g_array_unref (impl->node_data);
  impl->node_data = nodes;

  if (node_textures)
    g_ptr_array_ref (node_textures);
  if (impl->node_data_textures)
    g_ptr_array_unref (impl->node_data_textures);
  impl->node_data_textures = node_textures;

  gdk_broadway_server_surface_set_nodes (broadway_display->server, impl->id, impl->node_data);
}

/* This function is called when the XSurface is really gone.
 */
static void
gdk_broadway_surface_destroy_notify (GdkSurface *surface)
{
  if (!GDK_SURFACE_DESTROYED (surface))
    _gdk_surface_destroy (surface, TRUE);

  g_object_unref (surface);
}

static void
gdk_surface_broadway_show (GdkSurface *surface,
                           gboolean already_mapped)
{
  GdkSurfaceImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;

  impl = GDK_SURFACE_IMPL_BROADWAY (surface->impl);
  impl->visible = TRUE;

  /* FIXME: update state ? */

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (surface));
  if (_gdk_broadway_server_surface_show (broadway_display->server, impl->id))
    queue_flush (surface);

}

static void
gdk_surface_broadway_hide (GdkSurface *surface)
{
  GdkSurfaceImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;

  impl = GDK_SURFACE_IMPL_BROADWAY (surface->impl);
  impl->visible = FALSE;

  /* FIXME: update state ? */

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (surface));

  _gdk_broadway_surface_grab_check_unmap (surface,
                                         _gdk_broadway_server_get_next_serial (broadway_display->server));

  if (_gdk_broadway_server_surface_hide (broadway_display->server, impl->id))
    queue_flush (surface);

  _gdk_surface_clear_update_area (surface);
}

static void
gdk_surface_broadway_withdraw (GdkSurface *surface)
{
  gdk_surface_broadway_hide (surface);
}

static void
gdk_surface_broadway_move_resize (GdkSurface *surface,
                                  gboolean   with_move,
                                  gint       x,
                                  gint       y,
                                  gint       width,
                                  gint       height)
{
  GdkSurfaceImplBroadway *impl = GDK_SURFACE_IMPL_BROADWAY (surface->impl);
  GdkBroadwayDisplay *broadway_display;
  gboolean size_changed;

  size_changed = FALSE;

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (surface));

  if (width > 0 || height > 0)
    {
      if (width < 1)
        width = 1;

      if (height < 1)
        height = 1;

      if (width != surface->width ||
          height != surface->height)
        {
          size_changed = TRUE;

          /* Resize clears the content */
          impl->dirty = TRUE;
          impl->last_synced = FALSE;

          surface->width = width;
          surface->height = height;
        }
    }

  _gdk_broadway_server_surface_move_resize (broadway_display->server,
                                            impl->id,
                                            with_move,
                                            x, y,
                                            surface->width, surface->height);
  queue_flush (surface);
  if (size_changed)
    {
      surface->resize_count++;
      _gdk_surface_update_size (surface);
    }
}

static void
gdk_surface_broadway_raise (GdkSurface *surface)
{
}

static void
gdk_surface_broadway_restack_toplevel (GdkSurface *surface,
                                       GdkSurface *sibling,
                                       gboolean   above)
{
}

static void
gdk_surface_broadway_lower (GdkSurface *surface)
{
}


static void
gdk_broadway_surface_focus (GdkSurface *surface,
                            guint32    timestamp)
{
  GdkSurfaceImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface) ||
      !surface->accept_focus)
    return;

  impl = GDK_SURFACE_IMPL_BROADWAY (surface->impl);
  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (surface));
  _gdk_broadway_server_surface_focus (broadway_display->server,
                                      impl->id);
}

static void
gdk_broadway_surface_set_type_hint (GdkSurface        *surface,
                                    GdkSurfaceTypeHint hint)
{
}

static GdkSurfaceTypeHint
gdk_broadway_surface_get_type_hint (GdkSurface *surface)
{
  return GDK_SURFACE_TYPE_HINT_NORMAL;
}

static void
gdk_broadway_surface_set_modal_hint (GdkSurface *surface,
                                     gboolean   modal)
{
}

static void
gdk_broadway_surface_set_geometry_hints (GdkSurface         *surface,
                                         const GdkGeometry *geometry,
                                         GdkSurfaceHints     geom_mask)
{
  GdkSurfaceImplBroadway *impl;

  impl = GDK_SURFACE_IMPL_BROADWAY (surface->impl);

  impl->geometry_hints = *geometry;
  impl->geometry_hints_mask = geom_mask;
}

static void
gdk_broadway_surface_set_title (GdkSurface   *surface,
                                const gchar *title)
{
}

static void
gdk_broadway_surface_set_startup_id (GdkSurface   *surface,
                                     const gchar *startup_id)
{
}

static void
gdk_broadway_surface_set_transient_for (GdkSurface *surface,
                                        GdkSurface *parent)
{
  GdkBroadwayDisplay *display;
  GdkSurfaceImplBroadway *impl;
  int parent_id;

  impl = GDK_SURFACE_IMPL_BROADWAY (surface->impl);

  parent_id = 0;
  if (parent)
    parent_id = GDK_SURFACE_IMPL_BROADWAY (parent->impl)->id;

  impl->transient_for = parent_id;

  display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (impl->wrapper));
  _gdk_broadway_server_surface_set_transient_for (display->server, impl->id, impl->transient_for);
}

static void
gdk_surface_broadway_get_geometry (GdkSurface *surface,
                                   gint      *x,
                                   gint      *y,
                                   gint      *width,
                                   gint      *height)
{
  GdkSurfaceImplBroadway *impl;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  impl = GDK_SURFACE_IMPL_BROADWAY (surface->impl);

  /* TODO: These should really roundtrip to the client to get the current data */

  if (x)
    *x = impl->wrapper->x;
  if (y)
    *y = impl->wrapper->y;
  if (width)
    *width = impl->wrapper->width;
  if (height)
    *height = impl->wrapper->height;

}

static void
gdk_surface_broadway_get_root_coords (GdkSurface *surface,
                                      gint       x,
                                      gint       y,
                                      gint      *root_x,
                                      gint      *root_y)
{
  GdkSurfaceImplBroadway *impl;

  impl = GDK_SURFACE_IMPL_BROADWAY (surface->impl);

  if (root_x)
    *root_x = x + impl->wrapper->x;
  if (root_y)
    *root_y = y + impl->wrapper->y;
}

static void
gdk_broadway_surface_get_frame_extents (GdkSurface    *surface,
                                        GdkRectangle *rect)
{
  g_return_if_fail (rect != NULL);

  /* TODO: This should take wm frame into account */

  rect->x = surface->x;
  rect->y = surface->y;
  rect->width = surface->width;
  rect->height = surface->height;
}

static gboolean
gdk_surface_broadway_get_device_state (GdkSurface       *surface,
                                       GdkDevice       *device,
                                       gdouble         *x,
                                       gdouble         *y,
                                       GdkModifierType *mask)
{
  GdkSurface *child;

  g_return_val_if_fail (surface == NULL || GDK_IS_SURFACE (surface), FALSE);

  if (GDK_SURFACE_DESTROYED (surface))
    return FALSE;

  GDK_DEVICE_GET_CLASS (device)->query_state (device, surface,
                                              &child,
                                              NULL, NULL,
                                              x, y, mask);
  return child != NULL;
}

static void
gdk_surface_broadway_input_shape_combine_region (GdkSurface       *surface,
                                                 const cairo_region_t *shape_region,
                                                 gint             offset_x,
                                                 gint             offset_y)
{
}

static void
gdk_broadway_surface_set_accept_focus (GdkSurface *surface,
                                       gboolean accept_focus)
{
  accept_focus = accept_focus != FALSE;

  if (surface->accept_focus != accept_focus)
    {
      surface->accept_focus = accept_focus;
    }
}

static void
gdk_broadway_surface_set_focus_on_map (GdkSurface *surface,
                                       gboolean focus_on_map)
{
  focus_on_map = focus_on_map != FALSE;

  if (surface->focus_on_map != focus_on_map)
    {
      surface->focus_on_map = focus_on_map;
    }
}


static void
gdk_broadway_surface_set_icon_list (GdkSurface *surface,
                                    GList     *surfaces)
{
}

static void
gdk_broadway_surface_set_icon_name (GdkSurface   *surface,
                                   const gchar *name)
{
  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

  g_object_set_qdata (G_OBJECT (surface), g_quark_from_static_string ("gdk-icon-name-set"),
                      GUINT_TO_POINTER (name != NULL));
}

static void
gdk_broadway_surface_iconify (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;
}

static void
gdk_broadway_surface_deiconify (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;
}

static void
gdk_broadway_surface_stick (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

}

static void
gdk_broadway_surface_unstick (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

}

static void
gdk_broadway_surface_maximize (GdkSurface *surface)
{
  GdkSurfaceImplBroadway *impl;
  GdkDisplay *display;
  GdkMonitor *monitor;
  GdkRectangle geom;

  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

  impl = GDK_SURFACE_IMPL_BROADWAY (surface->impl);

  if (impl->maximized)
    return;

  impl->maximized = TRUE;

  gdk_synthesize_surface_state (surface, 0, GDK_SURFACE_STATE_MAXIMIZED);

  impl->pre_maximize_x = surface->x;
  impl->pre_maximize_y = surface->y;
  impl->pre_maximize_width = surface->width;
  impl->pre_maximize_height = surface->height;

  display = gdk_surface_get_display (surface);
  monitor = gdk_display_get_primary_monitor (display);
  gdk_monitor_get_geometry (monitor, &geom);

  gdk_surface_move_resize (surface,
                          geom.x, geom.y,
                          geom.width, geom.height);
}

static void
gdk_broadway_surface_unmaximize (GdkSurface *surface)
{
  GdkSurfaceImplBroadway *impl;

  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

  impl = GDK_SURFACE_IMPL_BROADWAY (surface->impl);

  if (!impl->maximized)
    return;

  impl->maximized = FALSE;

  gdk_synthesize_surface_state (surface, GDK_SURFACE_STATE_MAXIMIZED, 0);

  gdk_surface_move_resize (surface,
                          impl->pre_maximize_x,
                          impl->pre_maximize_y,
                          impl->pre_maximize_width,
                          impl->pre_maximize_height);
}

static void
gdk_broadway_surface_fullscreen (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

}

static void
gdk_broadway_surface_unfullscreen (GdkSurface *surface)
{
  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

}

static void
gdk_broadway_surface_set_keep_above (GdkSurface *surface,
                                     gboolean   setting)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

}

static void
gdk_broadway_surface_set_keep_below (GdkSurface *surface, gboolean setting)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

}

static void
gdk_broadway_surface_set_decorations (GdkSurface      *surface,
                                      GdkWMDecoration decorations)
{
  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

}

static gboolean
gdk_broadway_surface_get_decorations (GdkSurface       *surface,
                                      GdkWMDecoration *decorations)
{
  gboolean result = FALSE;

  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return FALSE;

  return result;
}

static void
gdk_broadway_surface_set_functions (GdkSurface    *surface,
                                    GdkWMFunction functions)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;
}

typedef struct _MoveResizeData MoveResizeData;

struct _MoveResizeData
{
  GdkDisplay *display;

  GdkSurface *moveresize_surface;
  GdkSurface *moveresize_emulation_surface;
  gboolean is_resize;
  GdkSurfaceEdge resize_edge;
  gint moveresize_button;
  gint moveresize_x;
  gint moveresize_y;
  gint moveresize_orig_x;
  gint moveresize_orig_y;
  gint moveresize_orig_width;
  gint moveresize_orig_height;
  long moveresize_process_time;
  GdkSurfaceHints moveresize_geom_mask;
  GdkGeometry moveresize_geometry;
  BroadwayInputMsg *moveresize_pending_event;
};

static MoveResizeData *
get_move_resize_data (GdkDisplay *display,
                      gboolean    create)
{
  GdkBroadwayDisplay *broadway_display;
  MoveResizeData *mv_resize;

  broadway_display = GDK_BROADWAY_DISPLAY (display);

  mv_resize = broadway_display->move_resize_data;

  if (!mv_resize && create)
    {
      mv_resize = g_new0 (MoveResizeData, 1);
      mv_resize->display = display;

      broadway_display->move_resize_data = mv_resize;
    }

  return mv_resize;
}

static void
update_pos (MoveResizeData *mv_resize,
            gint            new_root_x,
            gint            new_root_y)
{
  gint dx, dy;

  dx = new_root_x - mv_resize->moveresize_x;
  dy = new_root_y - mv_resize->moveresize_y;

  if (mv_resize->is_resize)
    {
      gint x, y, w, h;

      x = mv_resize->moveresize_orig_x;
      y = mv_resize->moveresize_orig_y;

      w = mv_resize->moveresize_orig_width;
      h = mv_resize->moveresize_orig_height;

      switch (mv_resize->resize_edge)
        {
        case GDK_SURFACE_EDGE_NORTH_WEST:
          x += dx;
          y += dy;
          w -= dx;
          h -= dy;
          break;
        case GDK_SURFACE_EDGE_NORTH:
          y += dy;
          h -= dy;
          break;
        case GDK_SURFACE_EDGE_NORTH_EAST:
          y += dy;
          h -= dy;
          w += dx;
          break;
        case GDK_SURFACE_EDGE_SOUTH_WEST:
          h += dy;
          x += dx;
          w -= dx;
          break;
        case GDK_SURFACE_EDGE_SOUTH_EAST:
          w += dx;
          h += dy;
          break;
        case GDK_SURFACE_EDGE_SOUTH:
          h += dy;
          break;
        case GDK_SURFACE_EDGE_EAST:
          w += dx;
          break;
        case GDK_SURFACE_EDGE_WEST:
          x += dx;
          w -= dx;
          break;
        default:
          break;
        }

      x = MAX (x, 0);
      y = MAX (y, 0);
      w = MAX (w, 1);
      h = MAX (h, 1);

      if (mv_resize->moveresize_geom_mask)
        {
          gdk_surface_constrain_size (&mv_resize->moveresize_geometry,
                                     mv_resize->moveresize_geom_mask,
                                     w, h, &w, &h);
        }

      gdk_surface_move_resize (mv_resize->moveresize_surface, x, y, w, h);
    }
  else
    {
      gint x, y;

      x = mv_resize->moveresize_orig_x + dx;
      y = mv_resize->moveresize_orig_y + dy;

      gdk_surface_move (mv_resize->moveresize_surface, x, y);
    }
}

static void
finish_drag (MoveResizeData *mv_resize)
{
  gdk_surface_destroy (mv_resize->moveresize_emulation_surface);
  mv_resize->moveresize_emulation_surface = NULL;
  g_object_unref (mv_resize->moveresize_surface);
  mv_resize->moveresize_surface = NULL;
  g_clear_pointer (&mv_resize->moveresize_pending_event, g_free);
}

static gboolean
moveresize_lookahead (GdkDisplay *display,
                      MoveResizeData *mv_resize,
                      BroadwayInputMsg *event)
{
  GdkBroadwayDisplay *broadway_display;

  broadway_display = GDK_BROADWAY_DISPLAY (display);

  return !_gdk_broadway_server_lookahead_event (broadway_display->server, "mb");
}

gboolean
_gdk_broadway_moveresize_handle_event (GdkDisplay *display,
                                       BroadwayInputMsg *event)
{
  guint button_mask = 0;
  MoveResizeData *mv_resize = get_move_resize_data (display, FALSE);

  if (!mv_resize || !mv_resize->moveresize_surface)
    return FALSE;

  button_mask = GDK_BUTTON1_MASK << (mv_resize->moveresize_button - 1);

  switch (event->base.type)
    {
    case BROADWAY_EVENT_TOUCH:
      if (event->touch.touch_type == 2) /* END */
        {
          update_pos (mv_resize,
                      event->touch.root_x,
                      event->touch.root_y);

          finish_drag (mv_resize);
        }
      else if (event->touch.touch_type == 1) /* UPDATE */
        {
          if (mv_resize->moveresize_surface->resize_count > 0)
            {
              if (mv_resize->moveresize_pending_event)
                *mv_resize->moveresize_pending_event = *event;
              else
                mv_resize->moveresize_pending_event =
                  g_memdup (event, sizeof (BroadwayInputMsg));

              break;
            }
          update_pos (mv_resize,
                      event->touch.root_x,
                      event->touch.root_y);
        }

      break;

    case BROADWAY_EVENT_POINTER_MOVE:
      if (mv_resize->moveresize_surface->resize_count > 0)
        {
          if (mv_resize->moveresize_pending_event)
            *mv_resize->moveresize_pending_event = *event;
          else
            mv_resize->moveresize_pending_event =
              g_memdup (event, sizeof (BroadwayInputMsg));

          break;
        }
      if (!moveresize_lookahead (display, mv_resize, event))
        break;

      update_pos (mv_resize,
                  event->pointer.root_x,
                  event->pointer.root_y);

      /* This should never be triggered in normal cases, but in the
       * case where the drag started without an implicit grab being
       * in effect, we could miss the release if it occurs before
       * we grab the pointer; this ensures that we will never
       * get a permanently stuck grab.
       */
      if ((event->pointer.state & button_mask) == 0)
        finish_drag (mv_resize);
      break;

    case BROADWAY_EVENT_BUTTON_RELEASE:
      update_pos (mv_resize,
                  event->pointer.root_x,
                  event->pointer.root_y);

      if (event->button.button == mv_resize->moveresize_button)
        finish_drag (mv_resize);
      break;
    default:
      break;
    }
  return TRUE;
}

gboolean
_gdk_broadway_moveresize_configure_done (GdkDisplay *display,
                                         GdkSurface  *surface)
{
  BroadwayInputMsg *tmp_event;
  MoveResizeData *mv_resize = get_move_resize_data (display, FALSE);

  if (!mv_resize || surface != mv_resize->moveresize_surface)
    return FALSE;

  if (mv_resize->moveresize_pending_event)
    {
      tmp_event = mv_resize->moveresize_pending_event;
      mv_resize->moveresize_pending_event = NULL;
      _gdk_broadway_moveresize_handle_event (display, tmp_event);
      g_free (tmp_event);
    }

  return TRUE;
}

static void
create_moveresize_surface (MoveResizeData *mv_resize,
                           guint32         timestamp)
{
  GdkGrabStatus status;
  GdkSeat *seat;
  GdkDevice *pointer;

  g_assert (mv_resize->moveresize_emulation_surface == NULL);

  mv_resize->moveresize_emulation_surface = gdk_surface_new_temp (mv_resize->display);

  gdk_surface_show (mv_resize->moveresize_emulation_surface);

  seat = gdk_display_get_default_seat (mv_resize->display);
  pointer = gdk_seat_get_pointer (seat);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  status = gdk_device_grab (pointer,
                             mv_resize->moveresize_emulation_surface,
                             GDK_OWNERSHIP_APPLICATION,
                             FALSE,
                             GDK_BUTTON_RELEASE_MASK |
                             GDK_POINTER_MOTION_MASK,
                             NULL,
                             timestamp);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (status != GDK_GRAB_SUCCESS)
    {
      /* If this fails, some other client has grabbed the surface
       * already.
       */
      finish_drag (mv_resize);
    }

  mv_resize->moveresize_process_time = 0;
}

static void
calculate_unmoving_origin (MoveResizeData *mv_resize)
{
  GdkRectangle rect;
  gint width, height;

  if (mv_resize->moveresize_geom_mask & GDK_HINT_WIN_GRAVITY &&
      mv_resize->moveresize_geometry.win_gravity == GDK_GRAVITY_STATIC)
    {
      gdk_surface_get_origin (mv_resize->moveresize_surface,
                             &mv_resize->moveresize_orig_x,
                             &mv_resize->moveresize_orig_y);
    }
  else
    {
      gdk_surface_get_frame_extents (mv_resize->moveresize_surface, &rect);
      gdk_surface_get_geometry (mv_resize->moveresize_surface,
                               NULL, NULL, &width, &height);

      switch (mv_resize->moveresize_geometry.win_gravity)
        {
        case GDK_GRAVITY_NORTH_WEST:
          mv_resize->moveresize_orig_x = rect.x;
          mv_resize->moveresize_orig_y = rect.y;
          break;
        case GDK_GRAVITY_NORTH:
          mv_resize->moveresize_orig_x = rect.x + rect.width / 2 - width / 2;
          mv_resize->moveresize_orig_y = rect.y;
          break;
        case GDK_GRAVITY_NORTH_EAST:
          mv_resize->moveresize_orig_x = rect.x + rect.width - width;
          mv_resize->moveresize_orig_y = rect.y;
          break;
        case GDK_GRAVITY_WEST:
          mv_resize->moveresize_orig_x = rect.x;
          mv_resize->moveresize_orig_y = rect.y + rect.height / 2 - height / 2;
          break;
        case GDK_GRAVITY_CENTER:
          mv_resize->moveresize_orig_x = rect.x + rect.width / 2 - width / 2;
          mv_resize->moveresize_orig_y = rect.y + rect.height / 2 - height / 2;
          break;
        case GDK_GRAVITY_EAST:
          mv_resize->moveresize_orig_x = rect.x + rect.width - width;
          mv_resize->moveresize_orig_y = rect.y + rect.height / 2 - height / 2;
          break;
        case GDK_GRAVITY_SOUTH_WEST:
          mv_resize->moveresize_orig_x = rect.x;
          mv_resize->moveresize_orig_y = rect.y + rect.height - height;
          break;
        case GDK_GRAVITY_SOUTH:
          mv_resize->moveresize_orig_x = rect.x + rect.width / 2 - width / 2;
          mv_resize->moveresize_orig_y = rect.y + rect.height - height;
          break;
        case GDK_GRAVITY_SOUTH_EAST:
          mv_resize->moveresize_orig_x = rect.x + rect.width - width;
          mv_resize->moveresize_orig_y = rect.y + rect.height - height;
          break;
        case GDK_GRAVITY_STATIC:
        default:
          mv_resize->moveresize_orig_x = rect.x;
          mv_resize->moveresize_orig_y = rect.y;
          break;
        }
    }
}

static void
gdk_broadway_surface_begin_resize_drag (GdkSurface     *surface,
                                        GdkSurfaceEdge  edge,
                                        GdkDevice     *device,
                                        gint           button,
                                        gint           x,
                                        gint           y,
                                        guint32        timestamp)
{
  MoveResizeData *mv_resize;
  GdkSurfaceImplBroadway *impl;

  impl = GDK_SURFACE_IMPL_BROADWAY (surface->impl);

  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

  if (impl->maximized)
    return;

  mv_resize = get_move_resize_data (gdk_surface_get_display (surface), TRUE);

  if (mv_resize->moveresize_surface != NULL)
    return; /* already a drag operation in progress */

  mv_resize->is_resize = TRUE;
  mv_resize->moveresize_button = button;
  mv_resize->resize_edge = edge;
  mv_resize->moveresize_x = x  + impl->wrapper->x;
  mv_resize->moveresize_y = y + impl->wrapper->y;
  mv_resize->moveresize_surface = g_object_ref (surface);

  mv_resize->moveresize_orig_width = gdk_surface_get_width (surface);
  mv_resize->moveresize_orig_height = gdk_surface_get_height (surface);

  mv_resize->moveresize_geom_mask = impl->geometry_hints_mask;
  mv_resize->moveresize_geometry = impl->geometry_hints;

  calculate_unmoving_origin (mv_resize);

  create_moveresize_surface (mv_resize, timestamp);
}

static void
gdk_broadway_surface_begin_move_drag (GdkSurface *surface,
                                      GdkDevice *device,
                                      gint       button,
                                      gint       x,
                                      gint       y,
                                      guint32    timestamp)
{
  MoveResizeData *mv_resize;
  GdkSurfaceImplBroadway *impl;

  impl = GDK_SURFACE_IMPL_BROADWAY (surface->impl);

  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

  if (impl->maximized)
    return;

  mv_resize = get_move_resize_data (gdk_surface_get_display (surface), TRUE);

  if (mv_resize->moveresize_surface != NULL)
    return; /* already a drag operation in progress */

  mv_resize->is_resize = FALSE;
  mv_resize->moveresize_button = button;
  mv_resize->moveresize_x = x + impl->wrapper->x;
  mv_resize->moveresize_y = y + impl->wrapper->y;
  mv_resize->moveresize_surface = g_object_ref (surface);

  mv_resize->moveresize_orig_width = gdk_surface_get_width (surface);
  mv_resize->moveresize_orig_height = gdk_surface_get_height (surface);

  mv_resize->moveresize_geom_mask = impl->geometry_hints_mask;
  mv_resize->moveresize_geometry = impl->geometry_hints;

  calculate_unmoving_origin (mv_resize);

  create_moveresize_surface (mv_resize, timestamp);
}

static gboolean
gdk_broadway_surface_beep (GdkSurface *surface)
{
  return FALSE;
}

static void
gdk_broadway_surface_set_opacity (GdkSurface *surface,
                                 gdouble    opacity)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface) ||
      !SURFACE_IS_TOPLEVEL (surface))
    return;

  if (opacity < 0)
    opacity = 0;
  else if (opacity > 1)
    opacity = 1;
}

guint32
gdk_broadway_get_last_seen_time (GdkSurface  *surface)
{
  GdkDisplay *display;

  display = gdk_surface_get_display (surface);
  return _gdk_broadway_server_get_last_seen_time (GDK_BROADWAY_DISPLAY (display)->server);
}

static void
gdk_surface_impl_broadway_class_init (GdkSurfaceImplBroadwayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceImplClass *impl_class = GDK_SURFACE_IMPL_CLASS (klass);

  object_class->finalize = gdk_surface_impl_broadway_finalize;

  impl_class->ref_cairo_surface = gdk_surface_broadway_ref_cairo_surface;
  impl_class->show = gdk_surface_broadway_show;
  impl_class->hide = gdk_surface_broadway_hide;
  impl_class->withdraw = gdk_surface_broadway_withdraw;
  impl_class->raise = gdk_surface_broadway_raise;
  impl_class->lower = gdk_surface_broadway_lower;
  impl_class->restack_toplevel = gdk_surface_broadway_restack_toplevel;
  impl_class->move_resize = gdk_surface_broadway_move_resize;
  impl_class->get_geometry = gdk_surface_broadway_get_geometry;
  impl_class->get_root_coords = gdk_surface_broadway_get_root_coords;
  impl_class->get_device_state = gdk_surface_broadway_get_device_state;
  impl_class->input_shape_combine_region = gdk_surface_broadway_input_shape_combine_region;
  impl_class->destroy = _gdk_broadway_surface_destroy;
  impl_class->beep = gdk_broadway_surface_beep;

  impl_class->focus = gdk_broadway_surface_focus;
  impl_class->set_type_hint = gdk_broadway_surface_set_type_hint;
  impl_class->get_type_hint = gdk_broadway_surface_get_type_hint;
  impl_class->set_modal_hint = gdk_broadway_surface_set_modal_hint;
  impl_class->set_geometry_hints = gdk_broadway_surface_set_geometry_hints;
  impl_class->set_title = gdk_broadway_surface_set_title;
  impl_class->set_startup_id = gdk_broadway_surface_set_startup_id;
  impl_class->set_transient_for = gdk_broadway_surface_set_transient_for;
  impl_class->get_frame_extents = gdk_broadway_surface_get_frame_extents;
  impl_class->set_accept_focus = gdk_broadway_surface_set_accept_focus;
  impl_class->set_focus_on_map = gdk_broadway_surface_set_focus_on_map;
  impl_class->set_icon_list = gdk_broadway_surface_set_icon_list;
  impl_class->set_icon_name = gdk_broadway_surface_set_icon_name;
  impl_class->iconify = gdk_broadway_surface_iconify;
  impl_class->deiconify = gdk_broadway_surface_deiconify;
  impl_class->stick = gdk_broadway_surface_stick;
  impl_class->unstick = gdk_broadway_surface_unstick;
  impl_class->maximize = gdk_broadway_surface_maximize;
  impl_class->unmaximize = gdk_broadway_surface_unmaximize;
  impl_class->fullscreen = gdk_broadway_surface_fullscreen;
  impl_class->unfullscreen = gdk_broadway_surface_unfullscreen;
  impl_class->set_keep_above = gdk_broadway_surface_set_keep_above;
  impl_class->set_keep_below = gdk_broadway_surface_set_keep_below;
  impl_class->set_decorations = gdk_broadway_surface_set_decorations;
  impl_class->get_decorations = gdk_broadway_surface_get_decorations;
  impl_class->set_functions = gdk_broadway_surface_set_functions;
  impl_class->begin_resize_drag = gdk_broadway_surface_begin_resize_drag;
  impl_class->begin_move_drag = gdk_broadway_surface_begin_move_drag;
  impl_class->set_opacity = gdk_broadway_surface_set_opacity;
  impl_class->destroy_notify = gdk_broadway_surface_destroy_notify;
  impl_class->register_dnd = _gdk_broadway_surface_register_dnd;
  impl_class->drag_begin = _gdk_broadway_surface_drag_begin;
}
