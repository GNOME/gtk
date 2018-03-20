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

#define SURFACE_IS_TOPLEVEL(window) \
  (GDK_SURFACE_TYPE (window) != GDK_SURFACE_CHILD)

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

static GdkDisplay *
find_broadway_display (void)
{
  GdkDisplay *display;
  GSList *list, *l;

  display = NULL;

  list = gdk_display_manager_list_displays (gdk_display_manager_get ());
  for (l = list; l; l = l->next)
    {
      if (GDK_IS_BROADWAY_DISPLAY (l->data))
        {
          display = l->data;
          break; 
        }
    }
  g_slist_free (list);

  return display;
}

static guint flush_id = 0;

static gboolean
flush_idle (gpointer data)
{
  flush_id = 0;

  gdk_display_flush (find_broadway_display ());

  return FALSE;
}

/* We need to flush in an idle rather than AFTER_PAINT, as the clock
   is frozen during e.g. window resizes so the paint will not happen
   and the window resize request is never flushed. */
static void
queue_flush (GdkSurface *window)
{
  if (flush_id == 0)
    {
      flush_id = g_idle_add (flush_idle, NULL);
      g_source_set_name_by_id (flush_id, "[gtk+] flush_idle");
    }
}

static void
gdk_surface_impl_broadway_init (GdkSurfaceImplBroadway *impl)
{
  impl->toplevel_surface_type = -1;
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
_gdk_broadway_roundtrip_notify (GdkSurface  *window,
                                guint32 tag,
                                gboolean local_reply)
{
  GdkFrameClock *clock = gdk_surface_get_frame_clock (window);

  /* If there is no remove web client, rate limit update to once a second */
  if (local_reply)
    g_timeout_add_seconds (1, (GSourceFunc)thaw_clock_cb, g_object_ref (clock));
  else
    _gdk_frame_clock_thaw (clock);
}

static void
on_frame_clock_after_paint (GdkFrameClock *clock,
                            GdkSurface     *window)
{
  GdkDisplay *display = gdk_surface_get_display (window);
  GdkSurfaceImplBroadway *impl = GDK_SURFACE_IMPL_BROADWAY (window->impl);
  GdkBroadwayDisplay *broadway_display;

  _gdk_frame_clock_freeze (gdk_surface_get_frame_clock (window));

  broadway_display = GDK_BROADWAY_DISPLAY (display);

  _gdk_broadway_server_roundtrip (broadway_display->server, impl->id, _gdk_display_get_next_serial (display));

  gdk_display_flush (display);
}

static void
connect_frame_clock (GdkSurface *window)
{
  if (SURFACE_IS_TOPLEVEL (window))
    {
      GdkFrameClock *frame_clock = gdk_surface_get_frame_clock (window);

      g_signal_connect (frame_clock, "after-paint",
                        G_CALLBACK (on_frame_clock_after_paint), window);
    }
}

void
_gdk_broadway_display_create_surface_impl (GdkDisplay    *display,
                                          GdkSurface     *window,
                                          GdkSurface     *real_parent,
                                          GdkEventMask   event_mask,
                                          GdkSurfaceAttr *attributes)
{
  GdkSurfaceImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;

  broadway_display = GDK_BROADWAY_DISPLAY (display);

  impl = g_object_new (GDK_TYPE_SURFACE_IMPL_BROADWAY, NULL);
  window->impl = (GdkSurfaceImpl *)impl;
  impl->id = _gdk_broadway_server_new_surface (broadway_display->server,
                                               window->x,
                                               window->y,
                                               window->width,
                                               window->height,
                                               window->surface_type == GDK_SURFACE_TEMP);
  g_hash_table_insert (broadway_display->id_ht, GINT_TO_POINTER(impl->id), window);
  impl->wrapper = window;

  g_assert (window->surface_type == GDK_SURFACE_TOPLEVEL ||
            window->surface_type == GDK_SURFACE_TEMP);
  g_assert (window->parent == NULL);

  broadway_display->toplevels = g_list_prepend (broadway_display->toplevels, impl);

  connect_frame_clock (window);
}

static cairo_surface_t *
gdk_surface_broadway_ref_cairo_surface (GdkSurface *window)
{
  GdkSurfaceImplBroadway *impl = GDK_SURFACE_IMPL_BROADWAY (window->impl);

  if (GDK_IS_SURFACE_IMPL_BROADWAY (window) &&
      GDK_SURFACE_DESTROYED (impl->wrapper))
    return NULL;

  return cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1, 1);
}

static void
_gdk_broadway_surface_destroy (GdkSurface *window,
                              gboolean   recursing,
                              gboolean   foreign_destroy)
{
  GdkSurfaceImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;

  g_return_if_fail (GDK_IS_SURFACE (window));

  impl = GDK_SURFACE_IMPL_BROADWAY (window->impl);

  if (impl->node_data)
    g_array_unref (impl->node_data);
  if (impl->node_data_textures)
    g_ptr_array_unref (impl->node_data_textures);

  _gdk_broadway_surface_grab_check_destroy (window);

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (window));
  g_hash_table_remove (broadway_display->id_ht, GINT_TO_POINTER (impl->id));

  _gdk_broadway_server_destroy_surface (broadway_display->server, impl->id);

}

void
gdk_broadway_surface_set_nodes (GdkSurface *window,
                               GArray *nodes,
                               GPtrArray *node_textures)
{
  GdkSurfaceImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;

  g_return_if_fail (GDK_IS_SURFACE (window));

  impl = GDK_SURFACE_IMPL_BROADWAY (window->impl);

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (window));

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

/* This function is called when the XWindow is really gone.
 */
static void
gdk_broadway_surface_destroy_notify (GdkSurface *window)
{
  if (!GDK_SURFACE_DESTROYED (window))
    _gdk_surface_destroy (window, TRUE);

  g_object_unref (window);
}

static void
gdk_surface_broadway_show (GdkSurface *window, gboolean already_mapped)
{
  GdkSurfaceImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;

  impl = GDK_SURFACE_IMPL_BROADWAY (window->impl);
  impl->visible = TRUE;

  if (window->event_mask & GDK_STRUCTURE_MASK)
    _gdk_make_event (GDK_SURFACE (window), GDK_MAP, NULL, FALSE);

  if (window->parent && window->parent->event_mask & GDK_SUBSTRUCTURE_MASK)
    _gdk_make_event (GDK_SURFACE (window), GDK_MAP, NULL, FALSE);

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (window));
  if (_gdk_broadway_server_surface_show (broadway_display->server, impl->id))
    queue_flush (window);

}

static void
gdk_surface_broadway_hide (GdkSurface *window)
{
  GdkSurfaceImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;

  impl = GDK_SURFACE_IMPL_BROADWAY (window->impl);
  impl->visible = FALSE;

  if (window->event_mask & GDK_STRUCTURE_MASK)
    _gdk_make_event (GDK_SURFACE (window), GDK_UNMAP, NULL, FALSE);

  if (window->parent && window->parent->event_mask & GDK_SUBSTRUCTURE_MASK)
    _gdk_make_event (GDK_SURFACE (window), GDK_UNMAP, NULL, FALSE);

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (window));

  _gdk_broadway_surface_grab_check_unmap (window,
                                         _gdk_broadway_server_get_next_serial (broadway_display->server));

  if (_gdk_broadway_server_surface_hide (broadway_display->server, impl->id))
    queue_flush (window);

  _gdk_surface_clear_update_area (window);
}

static void
gdk_surface_broadway_withdraw (GdkSurface *window)
{
  gdk_surface_broadway_hide (window);
}

static void
gdk_surface_broadway_move_resize (GdkSurface *window,
                                 gboolean   with_move,
                                 gint       x,
                                 gint       y,
                                 gint       width,
                                 gint       height)
{
  GdkSurfaceImplBroadway *impl = GDK_SURFACE_IMPL_BROADWAY (window->impl);
  GdkBroadwayDisplay *broadway_display;
  gboolean size_changed;

  size_changed = FALSE;

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (window));

  if (width > 0 || height > 0)
    {
      if (width < 1)
        width = 1;

      if (height < 1)
        height = 1;

      if (width != window->width ||
          height != window->height)
        {
          size_changed = TRUE;

          /* Resize clears the content */
          impl->dirty = TRUE;
          impl->last_synced = FALSE;

          window->width = width;
          window->height = height;
        }
    }

  _gdk_broadway_server_surface_move_resize (broadway_display->server,
                                            impl->id,
                                            with_move,
                                            x, y,
                                            window->width, window->height);
  queue_flush (window);
  if (size_changed)
    window->resize_count++;
}

static void
gdk_surface_broadway_raise (GdkSurface *window)
{
}

static void
gdk_surface_broadway_restack_toplevel (GdkSurface *window,
                                      GdkSurface *sibling,
                                      gboolean   above)
{
}

static void
gdk_surface_broadway_lower (GdkSurface *window)
{
}


static void
gdk_broadway_surface_focus (GdkSurface *window,
                           guint32    timestamp)
{
  GdkSurfaceImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window) ||
      !window->accept_focus)
    return;

  impl = GDK_SURFACE_IMPL_BROADWAY (window->impl);
  broadway_display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (window));
  _gdk_broadway_server_surface_focus (broadway_display->server,
                                      impl->id);
}

static void
gdk_broadway_surface_set_type_hint (GdkSurface        *window,
                                   GdkSurfaceTypeHint hint)
{
}

static GdkSurfaceTypeHint
gdk_broadway_surface_get_type_hint (GdkSurface *window)
{
  return GDK_SURFACE_TYPE_HINT_NORMAL;
}

static void
gdk_broadway_surface_set_modal_hint (GdkSurface *window,
                                    gboolean   modal)
{
}

static void
gdk_broadway_surface_set_skip_taskbar_hint (GdkSurface *window,
                                           gboolean   skips_taskbar)
{
}

static void
gdk_broadway_surface_set_skip_pager_hint (GdkSurface *window,
                                         gboolean   skips_pager)
{
}

static void
gdk_broadway_surface_set_urgency_hint (GdkSurface *window,
                                      gboolean   urgent)
{
}

static void
gdk_broadway_surface_set_geometry_hints (GdkSurface         *window,
                                        const GdkGeometry *geometry,
                                        GdkSurfaceHints     geom_mask)
{
  GdkSurfaceImplBroadway *impl;

  impl = GDK_SURFACE_IMPL_BROADWAY (window->impl);

  impl->geometry_hints = *geometry;
  impl->geometry_hints_mask = geom_mask;
}

static void
gdk_broadway_surface_set_title (GdkSurface   *window,
                               const gchar *title)
{
}

static void
gdk_broadway_surface_set_role (GdkSurface   *window,
                              const gchar *role)
{
}

static void
gdk_broadway_surface_set_startup_id (GdkSurface   *window,
                                    const gchar *startup_id)
{
}

static void
gdk_broadway_surface_set_transient_for (GdkSurface *window,
                                       GdkSurface *parent)
{
  GdkBroadwayDisplay *display;
  GdkSurfaceImplBroadway *impl;
  int parent_id;

  impl = GDK_SURFACE_IMPL_BROADWAY (window->impl);

  parent_id = 0;
  if (parent)
    parent_id = GDK_SURFACE_IMPL_BROADWAY (parent->impl)->id;

  impl->transient_for = parent_id;

  display = GDK_BROADWAY_DISPLAY (gdk_surface_get_display (impl->wrapper));
  _gdk_broadway_server_surface_set_transient_for (display->server, impl->id, impl->transient_for);
}

static void
gdk_surface_broadway_get_geometry (GdkSurface *window,
                                  gint      *x,
                                  gint      *y,
                                  gint      *width,
                                  gint      *height)
{
  GdkSurfaceImplBroadway *impl;

  g_return_if_fail (GDK_IS_SURFACE (window));

  impl = GDK_SURFACE_IMPL_BROADWAY (window->impl);

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
gdk_surface_broadway_get_root_coords (GdkSurface *window,
                                     gint       x,
                                     gint       y,
                                     gint      *root_x,
                                     gint      *root_y)
{
  GdkSurfaceImplBroadway *impl;

  impl = GDK_SURFACE_IMPL_BROADWAY (window->impl);

  if (root_x)
    *root_x = x + impl->wrapper->x;
  if (root_y)
    *root_y = y + impl->wrapper->y;
}

static void
gdk_broadway_surface_get_frame_extents (GdkSurface    *window,
                                       GdkRectangle *rect)
{
  g_return_if_fail (rect != NULL);

  /* TODO: This should take wm frame into account */

  rect->x = window->x;
  rect->y = window->y;
  rect->width = window->width;
  rect->height = window->height;
}

static gboolean
gdk_surface_broadway_get_device_state (GdkSurface       *window,
                                      GdkDevice       *device,
                                      gdouble         *x,
                                      gdouble         *y,
                                      GdkModifierType *mask)
{
  GdkSurface *child;

  g_return_val_if_fail (window == NULL || GDK_IS_SURFACE (window), FALSE);

  if (GDK_SURFACE_DESTROYED (window))
    return FALSE;

  GDK_DEVICE_GET_CLASS (device)->query_state (device, window,
                                              &child,
                                              NULL, NULL,
                                              x, y, mask);
  return child != NULL;
}

static GdkEventMask
gdk_surface_broadway_get_events (GdkSurface *window)
{
  if (GDK_SURFACE_DESTROYED (window))
    return 0;

  return 0;
}

static void
gdk_surface_broadway_set_events (GdkSurface    *window,
                                GdkEventMask  event_mask)
{
  if (!GDK_SURFACE_DESTROYED (window))
    {
    }
}

static void
gdk_surface_broadway_shape_combine_region (GdkSurface       *window,
                                          const cairo_region_t *shape_region,
                                          gint             offset_x,
                                          gint             offset_y)
{
}

static void
gdk_surface_broadway_input_shape_combine_region (GdkSurface       *window,
                                                const cairo_region_t *shape_region,
                                                gint             offset_x,
                                                gint             offset_y)
{
}

static void
gdk_broadway_surface_set_accept_focus (GdkSurface *window,
                                      gboolean accept_focus)
{
  accept_focus = accept_focus != FALSE;

  if (window->accept_focus != accept_focus)
    {
      window->accept_focus = accept_focus;
    }
}

static void
gdk_broadway_surface_set_focus_on_map (GdkSurface *window,
                                      gboolean focus_on_map)
{
  focus_on_map = focus_on_map != FALSE;

  if (window->focus_on_map != focus_on_map)
    {
      window->focus_on_map = focus_on_map;
    }
}


static void
gdk_broadway_surface_set_icon_list (GdkSurface *window,
                                   GList     *surfaces)
{
}

static void
gdk_broadway_surface_set_icon_name (GdkSurface   *window,
                                   const gchar *name)
{
  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  g_object_set_qdata (G_OBJECT (window), g_quark_from_static_string ("gdk-icon-name-set"),
                      GUINT_TO_POINTER (name != NULL));
}

static void
gdk_broadway_surface_iconify (GdkSurface *window)
{
  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;
}

static void
gdk_broadway_surface_deiconify (GdkSurface *window)
{
  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;
}

static void
gdk_broadway_surface_stick (GdkSurface *window)
{
  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

}

static void
gdk_broadway_surface_unstick (GdkSurface *window)
{
  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

}

static void
gdk_broadway_surface_maximize (GdkSurface *window)
{
  GdkSurfaceImplBroadway *impl;
  GdkDisplay *display;
  GdkMonitor *monitor;
  GdkRectangle geom;

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  impl = GDK_SURFACE_IMPL_BROADWAY (window->impl);

  if (impl->maximized)
    return;

  impl->maximized = TRUE;

  gdk_synthesize_surface_state (window, 0, GDK_SURFACE_STATE_MAXIMIZED);

  impl->pre_maximize_x = window->x;
  impl->pre_maximize_y = window->y;
  impl->pre_maximize_width = window->width;
  impl->pre_maximize_height = window->height;

  display = gdk_surface_get_display (window);
  monitor = gdk_display_get_primary_monitor (display);
  gdk_monitor_get_geometry (monitor, &geom);

  gdk_surface_move_resize (window,
                          geom.x, geom.y,
                          geom.width, geom.height);
}

static void
gdk_broadway_surface_unmaximize (GdkSurface *window)
{
  GdkSurfaceImplBroadway *impl;

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  impl = GDK_SURFACE_IMPL_BROADWAY (window->impl);

  if (!impl->maximized)
    return;

  impl->maximized = FALSE;

  gdk_synthesize_surface_state (window, GDK_SURFACE_STATE_MAXIMIZED, 0);

  gdk_surface_move_resize (window,
                          impl->pre_maximize_x,
                          impl->pre_maximize_y,
                          impl->pre_maximize_width,
                          impl->pre_maximize_height);
}

static void
gdk_broadway_surface_fullscreen (GdkSurface *window)
{
  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

}

static void
gdk_broadway_surface_unfullscreen (GdkSurface *window)
{
  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

}

static void
gdk_broadway_surface_set_keep_above (GdkSurface *window,
                                    gboolean   setting)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

}

static void
gdk_broadway_surface_set_keep_below (GdkSurface *window, gboolean setting)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

}

static GdkSurface *
gdk_broadway_surface_get_group (GdkSurface *window)
{
  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return NULL;

  return window;
}

static void
gdk_broadway_surface_set_group (GdkSurface *window,
                               GdkSurface *leader)
{
}

static void
gdk_broadway_surface_set_decorations (GdkSurface      *window,
                                     GdkWMDecoration decorations)
{
  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

}

static gboolean
gdk_broadway_surface_get_decorations (GdkSurface       *window,
                                     GdkWMDecoration *decorations)
{
  gboolean result = FALSE;

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return FALSE;

  return result;
}

static void
gdk_broadway_surface_set_functions (GdkSurface    *window,
                                   GdkWMFunction functions)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;
}

static void
gdk_broadway_surface_end_paint (GdkSurface *window)
{
  GdkSurfaceImplBroadway *impl;
  impl = GDK_SURFACE_IMPL_BROADWAY (window->impl);
  impl->dirty = TRUE;
}

typedef struct _MoveResizeData MoveResizeData;

struct _MoveResizeData
{
  GdkDisplay *display;

  GdkSurface *moveresize_window;
  GdkSurface *moveresize_emulation_window;
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

      gdk_surface_move_resize (mv_resize->moveresize_window, x, y, w, h);
    }
  else
    {
      gint x, y;

      x = mv_resize->moveresize_orig_x + dx;
      y = mv_resize->moveresize_orig_y + dy;

      gdk_surface_move (mv_resize->moveresize_window, x, y);
    }
}

static void
finish_drag (MoveResizeData *mv_resize)
{
  gdk_surface_destroy (mv_resize->moveresize_emulation_window);
  mv_resize->moveresize_emulation_window = NULL;
  g_object_unref (mv_resize->moveresize_window);
  mv_resize->moveresize_window = NULL;
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

  if (!mv_resize || !mv_resize->moveresize_window)
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
          if (mv_resize->moveresize_window->resize_count > 0)
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
      if (mv_resize->moveresize_window->resize_count > 0)
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
                                         GdkSurface  *window)
{
  BroadwayInputMsg *tmp_event;
  MoveResizeData *mv_resize = get_move_resize_data (display, FALSE);

  if (!mv_resize || window != mv_resize->moveresize_window)
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
create_moveresize_window (MoveResizeData *mv_resize,
                          guint32         timestamp)
{
  GdkGrabStatus status;
  GdkSeat *seat;
  GdkDevice *pointer;

  g_assert (mv_resize->moveresize_emulation_window == NULL);

  mv_resize->moveresize_emulation_window = gdk_surface_new_temp (mv_resize->display);

  gdk_surface_show (mv_resize->moveresize_emulation_window);

  seat = gdk_display_get_default_seat (mv_resize->display);
  pointer = gdk_seat_get_pointer (seat);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  status = gdk_device_grab (pointer,
                             mv_resize->moveresize_emulation_window,
                             GDK_OWNERSHIP_APPLICATION,
                             FALSE,
                             GDK_BUTTON_RELEASE_MASK |
                             GDK_POINTER_MOTION_MASK,
                             NULL,
                             timestamp);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (status != GDK_GRAB_SUCCESS)
    {
      /* If this fails, some other client has grabbed the window
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
      gdk_surface_get_origin (mv_resize->moveresize_window,
                             &mv_resize->moveresize_orig_x,
                             &mv_resize->moveresize_orig_y);
    }
  else
    {
      gdk_surface_get_frame_extents (mv_resize->moveresize_window, &rect);
      gdk_surface_get_geometry (mv_resize->moveresize_window,
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
gdk_broadway_surface_begin_resize_drag (GdkSurface     *window,
                                       GdkSurfaceEdge  edge,
                                       GdkDevice     *device,
                                       gint           button,
                                       gint           root_x,
                                       gint           root_y,
                                       guint32        timestamp)
{
  MoveResizeData *mv_resize;
  GdkSurfaceImplBroadway *impl;

  impl = GDK_SURFACE_IMPL_BROADWAY (window->impl);

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  if (impl->maximized)
    return;

  mv_resize = get_move_resize_data (gdk_surface_get_display (window), TRUE);

  mv_resize->is_resize = TRUE;
  mv_resize->moveresize_button = button;
  mv_resize->resize_edge = edge;
  mv_resize->moveresize_x = root_x;
  mv_resize->moveresize_y = root_y;
  mv_resize->moveresize_window = g_object_ref (window);

  mv_resize->moveresize_orig_width = gdk_surface_get_width (window);
  mv_resize->moveresize_orig_height = gdk_surface_get_height (window);

  mv_resize->moveresize_geom_mask = impl->geometry_hints_mask;
  mv_resize->moveresize_geometry = impl->geometry_hints;

  calculate_unmoving_origin (mv_resize);

  create_moveresize_window (mv_resize, timestamp);
}

static void
gdk_broadway_surface_begin_move_drag (GdkSurface *window,
                                     GdkDevice *device,
                                     gint       button,
                                     gint       root_x,
                                     gint       root_y,
                                     guint32    timestamp)
{
  MoveResizeData *mv_resize;
  GdkSurfaceImplBroadway *impl;

  impl = GDK_SURFACE_IMPL_BROADWAY (window->impl);

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  if (impl->maximized)
    return;

  mv_resize = get_move_resize_data (gdk_surface_get_display (window), TRUE);

  mv_resize->is_resize = FALSE;
  mv_resize->moveresize_button = button;
  mv_resize->moveresize_x = root_x;
  mv_resize->moveresize_y = root_y;
  mv_resize->moveresize_window = g_object_ref (window);

  mv_resize->moveresize_orig_width = gdk_surface_get_width (window);
  mv_resize->moveresize_orig_height = gdk_surface_get_height (window);

  mv_resize->moveresize_geom_mask = impl->geometry_hints_mask;
  mv_resize->moveresize_geometry = impl->geometry_hints;

  calculate_unmoving_origin (mv_resize);

  create_moveresize_window (mv_resize, timestamp);
}

static gboolean
gdk_broadway_surface_beep (GdkSurface *window)
{
  return FALSE;
}

static void
gdk_broadway_surface_set_opacity (GdkSurface *window,
                                 gdouble    opacity)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  if (opacity < 0)
    opacity = 0;
  else if (opacity > 1)
    opacity = 1;
}

guint32
gdk_broadway_get_last_seen_time (GdkSurface  *window)
{
  GdkDisplay *display;

  display = gdk_surface_get_display (window);
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
  impl_class->set_events = gdk_surface_broadway_set_events;
  impl_class->get_events = gdk_surface_broadway_get_events;
  impl_class->raise = gdk_surface_broadway_raise;
  impl_class->lower = gdk_surface_broadway_lower;
  impl_class->restack_toplevel = gdk_surface_broadway_restack_toplevel;
  impl_class->move_resize = gdk_surface_broadway_move_resize;
  impl_class->get_geometry = gdk_surface_broadway_get_geometry;
  impl_class->get_root_coords = gdk_surface_broadway_get_root_coords;
  impl_class->get_device_state = gdk_surface_broadway_get_device_state;
  impl_class->shape_combine_region = gdk_surface_broadway_shape_combine_region;
  impl_class->input_shape_combine_region = gdk_surface_broadway_input_shape_combine_region;
  impl_class->destroy = _gdk_broadway_surface_destroy;
  impl_class->end_paint = gdk_broadway_surface_end_paint;
  impl_class->beep = gdk_broadway_surface_beep;

  impl_class->focus = gdk_broadway_surface_focus;
  impl_class->set_type_hint = gdk_broadway_surface_set_type_hint;
  impl_class->get_type_hint = gdk_broadway_surface_get_type_hint;
  impl_class->set_modal_hint = gdk_broadway_surface_set_modal_hint;
  impl_class->set_skip_taskbar_hint = gdk_broadway_surface_set_skip_taskbar_hint;
  impl_class->set_skip_pager_hint = gdk_broadway_surface_set_skip_pager_hint;
  impl_class->set_urgency_hint = gdk_broadway_surface_set_urgency_hint;
  impl_class->set_geometry_hints = gdk_broadway_surface_set_geometry_hints;
  impl_class->set_title = gdk_broadway_surface_set_title;
  impl_class->set_role = gdk_broadway_surface_set_role;
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
  impl_class->get_group = gdk_broadway_surface_get_group;
  impl_class->set_group = gdk_broadway_surface_set_group;
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
