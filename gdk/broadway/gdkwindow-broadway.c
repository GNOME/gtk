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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gdkwindow-broadway.h"
#include "gdkscreen-broadway.h"

#include "gdkwindow.h"
#include "gdkwindowimpl.h"
#include "gdkdisplay-broadway.h"
#include "gdkprivate-broadway.h"
#include "gdkinternals.h"
#include "gdkdeviceprivate.h"
#include "gdkeventsource.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>

/* Forward declarations */
static void     gdk_window_x11_set_background     (GdkWindow      *window,
                                                   cairo_pattern_t *pattern);

static void        gdk_window_impl_x11_finalize   (GObject            *object);
static void        gdk_window_impl_iface_init     (GdkWindowImplIface *iface);

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

G_DEFINE_TYPE_WITH_CODE (GdkWindowImplX11,
                         gdk_window_impl_x11,
                         GDK_TYPE_DRAWABLE_IMPL_X11,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_WINDOW_IMPL,
                                                gdk_window_impl_iface_init));

static GList *all_windows;

GType
_gdk_window_impl_get_type (void)
{
  return gdk_window_impl_x11_get_type ();
}

static void
gdk_window_impl_x11_init (GdkWindowImplX11 *impl)
{
  all_windows = g_list_prepend (all_windows, impl);
  impl->toplevel_window_type = -1;
  impl->device_cursor = g_hash_table_new_full (NULL, NULL, NULL,
                                               (GDestroyNotify) gdk_cursor_unref);
}

static void
gdk_window_impl_x11_class_init (GdkWindowImplX11Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_window_impl_x11_finalize;
}

static void
gdk_window_impl_x11_finalize (GObject *object)
{
  GdkWindowObject *wrapper;
  GdkDrawableImplX11 *draw_impl;
  GdkWindowImplX11 *window_impl;

  g_return_if_fail (GDK_IS_WINDOW_IMPL_X11 (object));

  draw_impl = GDK_DRAWABLE_IMPL_X11 (object);
  window_impl = GDK_WINDOW_IMPL_X11 (object);

  wrapper = (GdkWindowObject*) draw_impl->wrapper;

  if (window_impl->cursor)
    gdk_cursor_unref (window_impl->cursor);

  g_hash_table_destroy (window_impl->device_cursor);

  all_windows = g_list_remove (all_windows, window_impl);

  G_OBJECT_CLASS (gdk_window_impl_x11_parent_class)->finalize (object);
}

void
_gdk_windowing_window_init (GdkScreen * screen)
{
  GdkWindowObject *private;
  GdkDrawableImplX11 *draw_impl;
  GdkScreenX11 *screen_x11;

  screen_x11 = GDK_SCREEN_X11 (screen);

  g_assert (screen_x11->root_window == NULL);

  screen_x11->root_window = g_object_new (GDK_TYPE_WINDOW, NULL);

  private = (GdkWindowObject *) screen_x11->root_window;
  private->impl = g_object_new (_gdk_window_impl_get_type (), NULL);
  private->impl_window = private;
  private->visual = gdk_screen_get_system_visual (screen);

  draw_impl = GDK_DRAWABLE_IMPL_X11 (private->impl);

  draw_impl->screen = screen;
  draw_impl->wrapper = GDK_DRAWABLE (private);

  private->window_type = GDK_WINDOW_ROOT;
  private->depth = 24;

  private->x = 0;
  private->y = 0;
  private->abs_x = 0;
  private->abs_y = 0;
  private->width = gdk_screen_get_width (screen);
  private->height = gdk_screen_get_height (screen);
  private->viewable = TRUE;

  _gdk_window_update_size (screen_x11->root_window);
}

void
_gdk_window_impl_new (GdkWindow     *window,
		      GdkWindow     *real_parent,
		      GdkScreen     *screen,
		      GdkEventMask   event_mask,
		      GdkWindowAttr *attributes,
		      gint           attributes_mask)
{
  GdkWindowObject *private;
  GdkWindowImplX11 *impl;
  GdkDrawableImplX11 *draw_impl;
  GdkScreenX11 *screen_x11;
  GdkDisplayX11 *display_x11;

  private = (GdkWindowObject *) window;

  screen_x11 = GDK_SCREEN_X11 (screen);
  display_x11 = GDK_DISPLAY_X11 (GDK_SCREEN_DISPLAY (screen));

  impl = g_object_new (_gdk_window_impl_get_type (), NULL);
  private->impl = (GdkDrawable *)impl;
  draw_impl = GDK_DRAWABLE_IMPL_X11 (impl);
  draw_impl->wrapper = GDK_DRAWABLE (window);

  draw_impl->screen = screen;

  g_assert (private->window_type == GDK_WINDOW_TOPLEVEL ||
	    private->window_type == GDK_WINDOW_TEMP);
  g_assert (GDK_WINDOW_TYPE (private->parent) == GDK_WINDOW_ROOT);
}


GdkWindow *
gdk_window_foreign_new_for_display (GdkDisplay     *display,
				    GdkNativeWindow anid)
{
  return NULL;
}

GdkWindow *
gdk_window_lookup_for_display (GdkDisplay *display, GdkNativeWindow anid)
{
  return NULL;
}

GdkWindow *
gdk_window_lookup (GdkNativeWindow anid)
{
  return NULL;
}

static void
_gdk_x11_window_destroy (GdkWindow *window,
			 gboolean   recursing,
			 gboolean   foreign_destroy)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  _gdk_selection_window_destroyed (window);

  _gdk_x11_drawable_finish (private->impl);
}

static cairo_surface_t *
gdk_window_x11_resize_cairo_surface (GdkWindow       *window,
                                     cairo_surface_t *surface,
                                     gint             width,
                                     gint             height)
{
  /* Image surfaces cannot be resized */
  cairo_surface_destroy (surface);

  return NULL;
}

void
_gdk_windowing_window_destroy_foreign (GdkWindow *window)
{
}

/* This function is called when the XWindow is really gone.
 */
void
gdk_window_destroy_notify (GdkWindow *window)
{
  GdkWindowImplX11 *window_impl;

  window_impl = GDK_WINDOW_IMPL_X11 (((GdkWindowObject *)window)->impl);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE(window) != GDK_WINDOW_FOREIGN)
	g_warning ("GdkWindow %p unexpectedly destroyed", window);

      _gdk_window_destroy (window, TRUE);
    }

  g_object_unref (window);
}

static void
gdk_window_x11_show (GdkWindow *window, gboolean already_mapped)
{
  /* TODO */
}

static void
gdk_window_x11_hide (GdkWindow *window)
{
  GdkWindowObject *private;

  private = (GdkWindowObject*) window;

  _gdk_window_clear_update_area (window);
}

static void
gdk_window_x11_withdraw (GdkWindow *window)
{
  gdk_window_x11_hide (window);
}

static inline void
window_x11_move (GdkWindow *window,
                 gint       x,
                 gint       y)
{
  GdkWindowObject *private = (GdkWindowObject *) window;

  // XMoveWindow (GDK_WINDOW_XDISPLAY (window),  GDK_WINDOW_XID (window),  x, y);
  private->x = x;
  private->y = y;
}

static inline void
window_x11_resize (GdkWindow *window,
                   gint       width,
                   gint       height)
{
  GdkWindowObject *private = (GdkWindowObject *) window;

  if (width < 1)
    width = 1;

  if (height < 1)
    height = 1;

  //XResizeWindow (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window), width, height);

  private->width = width;
  private->height = height;
  _gdk_x11_drawable_update_size (private->impl);

  _gdk_x11_drawable_update_size (private->impl);
}

static inline void
window_x11_move_resize (GdkWindow *window,
                        gint       x,
                        gint       y,
                        gint       width,
                        gint       height)
{
  GdkWindowObject *private = (GdkWindowObject *) window;;

  if (width < 1)
    width = 1;

  if (height < 1)
    height = 1;

  //XMoveResizeWindow (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window), x, y, width, height);

  private->x = x;
  private->y = y;

  private->width = width;
  private->height = height;

  _gdk_x11_drawable_update_size (private->impl);
}

static void
gdk_window_x11_move_resize (GdkWindow *window,
                            gboolean   with_move,
                            gint       x,
                            gint       y,
                            gint       width,
                            gint       height)
{
  if (with_move && (width < 0 && height < 0))
    window_x11_move (window, x, y);
  else
    {
      if (with_move)
        window_x11_move_resize (window, x, y, width, height);
      else
        window_x11_resize (window, width, height);
    }
}

static gboolean
gdk_window_x11_reparent (GdkWindow *window,
                         GdkWindow *new_parent,
                         gint       x,
                         gint       y)
{
  return FALSE;
}

static void
gdk_window_x11_raise (GdkWindow *window)
{
}

static void
gdk_window_x11_restack_under (GdkWindow *window,
			      GList *native_siblings /* in requested order, first is bottom-most */)
{
}

static void
gdk_window_x11_restack_toplevel (GdkWindow *window,
				 GdkWindow *sibling,
				 gboolean   above)
{
}

static void
gdk_window_x11_lower (GdkWindow *window)
{
}


void
gdk_window_focus (GdkWindow *window,
                  guint32    timestamp)
{
}

void
gdk_window_set_type_hint (GdkWindow        *window,
			  GdkWindowTypeHint hint)
{
}

GdkWindowTypeHint
gdk_window_get_type_hint (GdkWindow *window)
{
  return GDK_WINDOW_TYPE_HINT_NORMAL;
}

void
gdk_window_set_modal_hint (GdkWindow *window,
			   gboolean   modal)
{
}

void
gdk_window_set_skip_taskbar_hint (GdkWindow *window,
                                  gboolean   skips_taskbar)
{
}

void
gdk_window_set_skip_pager_hint (GdkWindow *window,
                                gboolean   skips_pager)
{
}

void
gdk_window_set_urgency_hint (GdkWindow *window,
			     gboolean   urgent)
{
}

void
gdk_window_set_geometry_hints (GdkWindow         *window,
			       const GdkGeometry *geometry,
			       GdkWindowHints     geom_mask)
{
}

void
gdk_window_set_title (GdkWindow   *window,
		      const gchar *title)
{
}

void
gdk_window_set_role (GdkWindow   *window,
		     const gchar *role)
{
}

void
gdk_window_set_startup_id (GdkWindow   *window,
			   const gchar *startup_id)
{
}

void
gdk_window_set_transient_for (GdkWindow *window,
			      GdkWindow *parent)
{
}

static void
gdk_window_x11_set_background (GdkWindow      *window,
                               cairo_pattern_t *pattern)
{
  return;
}

static void
gdk_window_x11_set_device_cursor (GdkWindow *window,
                                  GdkDevice *device,
                                  GdkCursor *cursor)
{
  GdkWindowObject *private;
  GdkWindowImplX11 *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_DEVICE (device));

  private = (GdkWindowObject *) window;
  impl = GDK_WINDOW_IMPL_X11 (private->impl);

  if (!cursor)
    g_hash_table_remove (impl->device_cursor, device);
  else
    {
      _gdk_x11_cursor_update_theme (cursor);
      g_hash_table_replace (impl->device_cursor,
                            device, gdk_cursor_ref (cursor));
    }

  if (!GDK_WINDOW_DESTROYED (window))
    GDK_DEVICE_GET_CLASS (device)->set_window_cursor (device, window, cursor);
}

GdkCursor *
_gdk_x11_window_get_cursor (GdkWindow *window)
{
  GdkWindowObject *private;
  GdkWindowImplX11 *impl;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  private = (GdkWindowObject *)window;
  impl = GDK_WINDOW_IMPL_X11 (private->impl);

  return impl->cursor;
}

static void
gdk_window_x11_get_geometry (GdkWindow *window,
                             gint      *x,
                             gint      *y,
                             gint      *width,
                             gint      *height,
                             gint      *depth)
{
}

static gint
gdk_window_x11_get_root_coords (GdkWindow *window,
				gint       x,
				gint       y,
				gint      *root_x,
				gint      *root_y)
{
  if (root_x)
    *root_x = x;
  if (root_y)
    *root_y = y;

  return 1;
}

void
gdk_window_get_root_origin (GdkWindow *window,
			    gint      *x,
			    gint      *y)
{
  if (x)
    *x = 0;

  if (y)
    *y = 0;
}

void
gdk_window_get_frame_extents (GdkWindow    *window,
                              GdkRectangle *rect)
{
  GdkWindowObject *private;

  g_return_if_fail (rect != NULL);

  private = (GdkWindowObject*) window;

  rect->x = private->x;
  rect->y = private->y;
  rect->width = private->width;
  rect->height = private->height;
}

void
_gdk_windowing_get_device_state (GdkDisplay       *display,
                                 GdkDevice        *device,
                                 GdkScreen       **screen,
                                 gint             *x,
                                 gint             *y,
                                 GdkModifierType  *mask)
{
  if (display->closed)
    return;

}

static gboolean
gdk_window_x11_get_device_state (GdkWindow       *window,
                                 GdkDevice       *device,
                                 gint            *x,
                                 gint            *y,
                                 GdkModifierType *mask)
{
  return FALSE;
}

void
gdk_display_warp_pointer (GdkDisplay *display,
			  GdkScreen  *screen,
			  gint        x,
			  gint        y)
{
  GdkDevice *device;

  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_return_if_fail (GDK_IS_SCREEN (screen));

  device = display->core_pointer;
  GDK_DEVICE_GET_CLASS (device)->warp (device, screen, x, y);
}

void
gdk_display_warp_device (GdkDisplay *display,
                         GdkDevice  *device,
                         GdkScreen  *screen,
                         gint        x,
                         gint        y)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (display == gdk_device_get_display (device));

  GDK_DEVICE_GET_CLASS (device)->warp (device, screen, x, y);
}

GdkWindow*
_gdk_windowing_window_at_device_position (GdkDisplay      *display,
                                          GdkDevice       *device,
                                          gint            *win_x,
                                          gint            *win_y,
                                          GdkModifierType *mask,
                                          gboolean         get_toplevel)
{
  return NULL;
}

static GdkEventMask
gdk_window_x11_get_events (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return 0;

  return 0;
}
static void
gdk_window_x11_set_events (GdkWindow    *window,
                           GdkEventMask  event_mask)
{
  if (!GDK_WINDOW_DESTROYED (window))
    {
    }
}

static void
gdk_window_x11_shape_combine_region (GdkWindow       *window,
                                     const cairo_region_t *shape_region,
                                     gint             offset_x,
                                     gint             offset_y)
{
}

static void
gdk_window_x11_input_shape_combine_region (GdkWindow       *window,
					   const cairo_region_t *shape_region,
					   gint             offset_x,
					   gint             offset_y)
{
}


void
gdk_window_set_override_redirect (GdkWindow *window,
				  gboolean override_redirect)
{
}

void
gdk_window_set_accept_focus (GdkWindow *window,
			     gboolean accept_focus)
{
  GdkWindowObject *private;

  private = (GdkWindowObject *)window;

  accept_focus = accept_focus != FALSE;

  if (private->accept_focus != accept_focus)
    {
      private->accept_focus = accept_focus;
    }
}

void
gdk_window_set_focus_on_map (GdkWindow *window,
			     gboolean focus_on_map)
{
  GdkWindowObject *private;

  private = (GdkWindowObject *)window;

  focus_on_map = focus_on_map != FALSE;

  if (private->focus_on_map != focus_on_map)
    {
      private->focus_on_map = focus_on_map;
    }
}


void
gdk_window_set_icon_list (GdkWindow *window,
			  GList     *pixbufs)
{
}

void
gdk_window_set_icon_name (GdkWindow   *window, 
			  const gchar *name)
{
  GdkDisplay *display;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  display = gdk_window_get_display (window);

  g_object_set_qdata (G_OBJECT (window), g_quark_from_static_string ("gdk-icon-name-set"),
                      GUINT_TO_POINTER (name != NULL));
}

void
gdk_window_iconify (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;
}

void
gdk_window_deiconify (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;
}

void
gdk_window_stick (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

void
gdk_window_unstick (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

void
gdk_window_maximize (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

void
gdk_window_unmaximize (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

void
gdk_window_fullscreen (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

void
gdk_window_unfullscreen (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

void
gdk_window_set_keep_above (GdkWindow *window,
                           gboolean   setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

void
gdk_window_set_keep_below (GdkWindow *window, gboolean setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

GdkWindow *
gdk_window_get_group (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return NULL;

  return window;
}

void
gdk_window_set_group (GdkWindow *window,
		      GdkWindow *leader)
{
}

void
gdk_window_set_decorations (GdkWindow      *window,
			    GdkWMDecoration decorations)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

gboolean
gdk_window_get_decorations(GdkWindow       *window,
			   GdkWMDecoration *decorations)
{
  gboolean result = FALSE;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return FALSE;

  return result;
}

void
gdk_window_set_functions (GdkWindow    *window,
			  GdkWMFunction functions)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;
}

cairo_region_t *
_gdk_windowing_window_get_shape (GdkWindow *window)
{
  return NULL;
}

cairo_region_t *
_gdk_windowing_window_get_input_shape (GdkWindow *window)
{
  return NULL;
}


static gboolean
gdk_window_x11_set_static_gravities (GdkWindow *window,
                                     gboolean   use_static)
{
  return TRUE;
}

void
gdk_window_begin_resize_drag (GdkWindow     *window,
                              GdkWindowEdge  edge,
                              gint           button,
                              gint           root_x,
                              gint           root_y,
                              guint32        timestamp)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

void
gdk_window_begin_move_drag (GdkWindow *window,
                            gint       button,
                            gint       root_x,
                            gint       root_y,
                            guint32    timestamp)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

}

void
gdk_window_enable_synchronized_configure (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  if (!GDK_IS_WINDOW_IMPL_X11 (private->impl))
    return;
}

void
gdk_window_configure_finished (GdkWindow *window)
{
  if (!WINDOW_IS_TOPLEVEL (window))
    return;
}

void
_gdk_windowing_window_beep (GdkWindow *window)
{
  GdkDisplay *display;

  g_return_if_fail (GDK_IS_WINDOW (window));

  display = GDK_WINDOW_DISPLAY (window);

  gdk_display_beep (display);
}

void
gdk_window_set_opacity (GdkWindow *window,
			gdouble    opacity)
{
  GdkDisplay *display;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  display = gdk_window_get_display (window);

  if (opacity < 0)
    opacity = 0;
  else if (opacity > 1)
    opacity = 1;

}

void
_gdk_windowing_window_set_composited (GdkWindow *window,
                                      gboolean   composited)
{
}

void
_gdk_windowing_window_process_updates_recurse (GdkWindow *window,
                                               cairo_region_t *region)
{
  _gdk_window_process_updates_recurse (window, region);
}

void
_gdk_windowing_before_process_all_updates (void)
{
}

void
_gdk_windowing_after_process_all_updates (void)
{
}

gboolean
_gdk_x11_window_queue_antiexpose (GdkWindow *window,
				  cairo_region_t *area)
{
  return TRUE;
}

void
_gdk_x11_window_translate (GdkWindow      *window,
                           cairo_region_t *area,
                           gint            dx,
                           gint            dy)
{
  /* TODO: copy region */
}

static void
gdk_window_impl_iface_init (GdkWindowImplIface *iface)
{
  iface->show = gdk_window_x11_show;
  iface->hide = gdk_window_x11_hide;
  iface->withdraw = gdk_window_x11_withdraw;
  iface->set_events = gdk_window_x11_set_events;
  iface->get_events = gdk_window_x11_get_events;
  iface->raise = gdk_window_x11_raise;
  iface->lower = gdk_window_x11_lower;
  iface->restack_under = gdk_window_x11_restack_under;
  iface->restack_toplevel = gdk_window_x11_restack_toplevel;
  iface->move_resize = gdk_window_x11_move_resize;
  iface->set_background = gdk_window_x11_set_background;
  iface->reparent = gdk_window_x11_reparent;
  iface->set_device_cursor = gdk_window_x11_set_device_cursor;
  iface->get_geometry = gdk_window_x11_get_geometry;
  iface->get_root_coords = gdk_window_x11_get_root_coords;
  iface->get_device_state = gdk_window_x11_get_device_state;
  iface->shape_combine_region = gdk_window_x11_shape_combine_region;
  iface->input_shape_combine_region = gdk_window_x11_input_shape_combine_region;
  iface->set_static_gravities = gdk_window_x11_set_static_gravities;
  iface->queue_antiexpose = _gdk_x11_window_queue_antiexpose;
  iface->translate = _gdk_x11_window_translate;
  iface->destroy = _gdk_x11_window_destroy;
  iface->resize_cairo_surface = gdk_window_x11_resize_cairo_surface;
}
