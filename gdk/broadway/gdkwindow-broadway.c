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
static void     gdk_window_broadway_set_background     (GdkWindow      *window,
                                                   cairo_pattern_t *pattern);

static void        gdk_window_impl_broadway_finalize   (GObject            *object);

static const cairo_user_data_key_t gdk_broadway_cairo_key;

#define WINDOW_IS_TOPLEVEL_OR_FOREIGN(window) \
  (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&   \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_OFFSCREEN)

#define WINDOW_IS_TOPLEVEL(window)		     \
  (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&   \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN && \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_OFFSCREEN)

struct _GdkBroadwayWindow {
  GdkWindow parent;
};

struct _GdkBroadwayWindowClass {
  GdkWindowClass parent_class;
};

G_DEFINE_TYPE (GdkBroadwayWindow, gdk_broadway_window, GDK_TYPE_WINDOW)

static void
gdk_broadway_window_class_init (GdkBroadwayWindowClass *broadway_window_class)
{
}

static void
gdk_broadway_window_init (GdkBroadwayWindow *broadway_window)
{
}

G_DEFINE_TYPE (GdkWindowImplBroadway,
	       gdk_window_impl_broadway,
	       GDK_TYPE_WINDOW_IMPL)

static void
diff_surfaces (cairo_surface_t *surface,
	       cairo_surface_t *old_surface)
{
  guint8 *data, *old_data;
  guint32 *line, *old_line;
  int w, h, stride, old_stride;
  int x, y;

  data = cairo_image_surface_get_data (surface);
  old_data = cairo_image_surface_get_data (old_surface);

  w = cairo_image_surface_get_width (surface);
  h = cairo_image_surface_get_height (surface);

  stride = cairo_image_surface_get_stride (surface);
  old_stride = cairo_image_surface_get_stride (old_surface);

  for (y = 0; y < h; y++)
    {
      line = (guint32 *)data;
      old_line = (guint32 *)old_data;

      for (x = 0; x < w; x++)
	{
	  if ((*line & 0xffffff) == (*old_line & 0xffffff))
	    *old_line = 0;
	  else
	    *old_line = *line | 0xff000000;
	  line ++;
	  old_line ++;
	}

      data += stride;
      old_data += old_stride;
    }
}

static guint dirty_flush_id = 0;

static void
window_data_send (BroadwayOutput *output, GdkWindowImplBroadway *impl)
{
  cairo_t *cr;

  if (impl->surface == NULL)
    return;

  if (impl->last_synced)
    {
      diff_surfaces (impl->surface,
		     impl->last_surface);
      broadway_output_put_rgba (output, impl->id, 0, 0,
				cairo_image_surface_get_width (impl->last_surface),
				cairo_image_surface_get_height (impl->last_surface),
				cairo_image_surface_get_stride (impl->last_surface),
				cairo_image_surface_get_data (impl->last_surface));
    }
  else
    {
      impl->last_synced = TRUE;
      broadway_output_put_rgb (output, impl->id, 0, 0,
			       cairo_image_surface_get_width (impl->surface),
			       cairo_image_surface_get_height (impl->surface),
			       cairo_image_surface_get_stride (impl->surface),
			       cairo_image_surface_get_data (impl->surface));
    }

  broadway_output_surface_flush (output, impl->id);

  cr = cairo_create (impl->last_surface);
  cairo_set_source_surface (cr, impl->surface, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
}

static gboolean
dirty_flush_idle (gpointer data)
{
  GList *l;
  GdkBroadwayDisplay *display;
  BroadwayOutput *output;

  dirty_flush_id = 0;

  display = GDK_BROADWAY_DISPLAY (gdk_display_get_default ());
  output = display->output;
  if (output == NULL)
    return FALSE;

  for (l = display->toplevels; l != NULL; l = l->next)
    {
      GdkWindowImplBroadway *impl = l->data;

      if (impl->dirty)
	{
	  impl->dirty = FALSE;
	  window_data_send (display->output, impl);
	}
    }

  gdk_display_flush (GDK_DISPLAY (display));

  return FALSE;
}

static void
queue_dirty_flush (GdkBroadwayDisplay *display)
{
  if (dirty_flush_id == 0 && display->output != NULL)
    dirty_flush_id = gdk_threads_add_idle (dirty_flush_idle, NULL);
}

void
_gdk_broadway_resync_windows (void)
{
  GdkBroadwayDisplay *display;
  GList *l;

  dirty_flush_id = 0;

  display = GDK_BROADWAY_DISPLAY (gdk_display_get_default ());

  for (l = display->toplevels; l != NULL; l = l->next)
    {
      GdkWindowImplBroadway *impl = l->data;
      GdkWindow *window;

      window = impl->wrapper;

      if (impl->id == 0)
	continue; /* Skip root */

      impl->dirty = FALSE;
      impl->last_synced = FALSE;
      broadway_output_new_surface (display->output,
				   impl->id,
				   window->x,
				   window->y,
				   window->width,
				   window->height,
				   window->window_type == GDK_WINDOW_TEMP);
      if (impl->transient_for)
	broadway_output_set_transient_for (display->output, impl->id, impl->transient_for);
      /* Can't check GDK_WINDOW_IS_MAPPED here, because that doesn't correctly handle
	 withdrawn windows like menus */
      if (impl->visible)
	{
	  broadway_output_show_surface (display->output, impl->id);
	  window_data_send (display->output, impl);
	}
    }

  gdk_display_flush (GDK_DISPLAY (display));
}

static void
gdk_window_impl_broadway_init (GdkWindowImplBroadway *impl)
{
  impl->toplevel_window_type = -1;
  impl->device_cursor = g_hash_table_new_full (NULL, NULL, NULL,
                                               (GDestroyNotify) gdk_cursor_unref);
}

static void
gdk_window_impl_broadway_finalize (GObject *object)
{
  GdkWindow *wrapper;
  GdkWindowImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;

  g_return_if_fail (GDK_IS_WINDOW_IMPL_BROADWAY (object));

  impl = GDK_WINDOW_IMPL_BROADWAY (object);

  wrapper = impl->wrapper;

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_window_get_display (impl->wrapper));

  if (broadway_display->mouse_in_toplevel == GDK_WINDOW (wrapper))
    {
      /* TODO: Send leave + enter event, update cursors, etc */
      broadway_display->mouse_in_toplevel = NULL;
    }

  g_hash_table_remove (broadway_display->id_ht, GINT_TO_POINTER(impl->id));

  if (impl->cursor)
    gdk_cursor_unref (impl->cursor);

  g_hash_table_destroy (impl->device_cursor);

  broadway_display->toplevels = g_list_remove (broadway_display->toplevels, impl);

  G_OBJECT_CLASS (gdk_window_impl_broadway_parent_class)->finalize (object);
}

void
_gdk_broadway_screen_init_root_window (GdkScreen * screen)
{
  GdkWindow *window;
  GdkWindowImplBroadway *impl;
  GdkBroadwayScreen *broadway_screen;

  broadway_screen = GDK_BROADWAY_SCREEN (screen);

  g_assert (broadway_screen->root_window == NULL);

  broadway_screen->root_window = g_object_new (GDK_TYPE_BROADWAY_WINDOW, NULL);

  window = broadway_screen->root_window;
  window->impl = g_object_new (GDK_TYPE_WINDOW_IMPL_BROADWAY, NULL);
  window->impl_window = window;
  window->visual = gdk_screen_get_system_visual (screen);

  impl = GDK_WINDOW_IMPL_BROADWAY (window->impl);

  impl->screen = screen;
  impl->wrapper = window;

  window->window_type = GDK_WINDOW_ROOT;
  window->depth = 24;

  window->x = 0;
  window->y = 0;
  window->abs_x = 0;
  window->abs_y = 0;
  window->width = gdk_screen_get_width (screen);
  window->height = gdk_screen_get_height (screen);
  window->viewable = TRUE;

  _gdk_window_update_size (broadway_screen->root_window);
}

void
_gdk_broadway_display_create_window_impl (GdkDisplay    *display,
					  GdkWindow     *window,
					  GdkWindow     *real_parent,
					  GdkScreen     *screen,
					  GdkEventMask   event_mask,
					  GdkWindowAttr *attributes,
					  gint           attributes_mask)
{
  GdkWindowImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;
  static int current_id = 1; /* 0 is the root window */

  broadway_display = GDK_BROADWAY_DISPLAY (display);

  impl = g_object_new (GDK_TYPE_WINDOW_IMPL_BROADWAY, NULL);
  window->impl = (GdkWindowImpl *)impl;
  impl->id = current_id++;
  g_hash_table_insert (broadway_display->id_ht, GINT_TO_POINTER(impl->id), window);
  impl->wrapper = window;

  impl->screen = screen;

  g_assert (window->window_type == GDK_WINDOW_TOPLEVEL ||
	    window->window_type == GDK_WINDOW_TEMP);
  g_assert (GDK_WINDOW_TYPE (window->parent) == GDK_WINDOW_ROOT);

  broadway_display->toplevels = g_list_prepend (broadway_display->toplevels, impl);

  if (broadway_display->output)
    broadway_output_new_surface (broadway_display->output,
				 impl->id,
				 window->x,
				 window->y,
				 window->width,
				 window->height,
				 window->window_type == GDK_WINDOW_TEMP);
}

void
_gdk_broadway_window_resize_surface (GdkWindow *window)
{
  GdkWindowImplBroadway *impl = GDK_WINDOW_IMPL_BROADWAY (window->impl);
  cairo_surface_t *old, *last_old;

  if (impl->surface)
    {
      old = impl->surface;
      last_old = impl->last_surface;

      impl->surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
						  gdk_window_get_width (impl->wrapper),
						  gdk_window_get_height (impl->wrapper));
      impl->last_surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
						       gdk_window_get_width (impl->wrapper),
						       gdk_window_get_height (impl->wrapper));

      cairo_surface_destroy (old);
      cairo_surface_destroy (last_old);
    }

  if (impl->ref_surface)
    {
      cairo_surface_set_user_data (impl->ref_surface, &gdk_broadway_cairo_key,
				   NULL, NULL);
      impl->ref_surface = NULL;
    }

  gdk_window_invalidate_rect (window, NULL, TRUE);
}

static void
ref_surface_destroyed (void *data)
{
  GdkWindowImplBroadway *impl = data;

  impl->ref_surface = NULL;
}

static cairo_surface_t *
gdk_window_broadway_ref_cairo_surface (GdkWindow *window)
{
  GdkWindowImplBroadway *impl = GDK_WINDOW_IMPL_BROADWAY (window->impl);
  cairo_t *cr;
  int w, h;

  if (GDK_IS_WINDOW_IMPL_BROADWAY (window) &&
      GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  w = gdk_window_get_width (impl->wrapper);
  h = gdk_window_get_height (impl->wrapper);

  /* Create actual backing store if missing */
  if (!impl->surface)
    {
      impl->surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, w, h);
      impl->last_surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, w, h);

      cr = cairo_create (impl->surface);
      cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
      cairo_rectangle (cr, 0, 0, w, h);
      cairo_fill (cr);
      cairo_destroy (cr);

      cr = cairo_create (impl->last_surface);
      cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
      cairo_rectangle (cr, 0, 0, w, h);
      cairo_fill (cr);
      cairo_destroy (cr);
    }

  /* Create a destroyable surface referencing the real one */
  if (!impl->ref_surface)
    {
      impl->ref_surface =
	cairo_surface_create_for_rectangle (impl->surface,
					    0, 0,
					    w, h);
      if (impl->ref_surface)
	cairo_surface_set_user_data (impl->ref_surface, &gdk_broadway_cairo_key,
				     impl, ref_surface_destroyed);
    }
  else
    cairo_surface_reference (impl->ref_surface);

  return impl->ref_surface;
}

static void
_gdk_broadway_window_destroy (GdkWindow *window,
			      gboolean   recursing,
			      gboolean   foreign_destroy)
{
  GdkWindowImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_BROADWAY (window->impl);

  _gdk_broadway_selection_window_destroyed (window);

  if (impl->ref_surface)
    {
      cairo_surface_finish (impl->ref_surface);
      cairo_surface_set_user_data (impl->ref_surface, &gdk_broadway_cairo_key,
				   NULL, NULL);
    }

  if (impl->surface)
    {
      cairo_surface_destroy (impl->surface);
      impl->surface = NULL;
      cairo_surface_destroy (impl->last_surface);
      impl->last_surface = NULL;
    }

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_window_get_display (window));
  if (broadway_display->output)
    broadway_output_destroy_surface (broadway_display->output,
				     impl->id);
}

static cairo_surface_t *
gdk_window_broadway_resize_cairo_surface (GdkWindow       *window,
					  cairo_surface_t *surface,
					  gint             width,
					  gint             height)
{
  /* Image surfaces cannot be resized */
  cairo_surface_destroy (surface);

  return NULL;
}

static void
gdk_broadway_window_destroy_foreign (GdkWindow *window)
{
}

/* This function is called when the XWindow is really gone.
 */
static void
gdk_broadway_window_destroy_notify (GdkWindow *window)
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
gdk_window_broadway_show (GdkWindow *window, gboolean already_mapped)
{
  GdkWindowImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;

  impl = GDK_WINDOW_IMPL_BROADWAY (window->impl);
  impl->visible = TRUE;

  if (window->event_mask & GDK_STRUCTURE_MASK)
    _gdk_make_event (GDK_WINDOW (window), GDK_MAP, NULL, FALSE);

  if (window->parent && window->parent->event_mask & GDK_SUBSTRUCTURE_MASK)
    _gdk_make_event (GDK_WINDOW (window), GDK_MAP, NULL, FALSE);

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_window_get_display (window));
  if (broadway_display->output)
    {
      broadway_output_show_surface (broadway_display->output, impl->id);
      queue_dirty_flush (broadway_display);
    }
}

static void
gdk_window_broadway_hide (GdkWindow *window)
{
  GdkWindowImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;

  impl = GDK_WINDOW_IMPL_BROADWAY (window->impl);
  impl->visible = FALSE;

  if (window->event_mask & GDK_STRUCTURE_MASK)
    _gdk_make_event (GDK_WINDOW (window), GDK_UNMAP, NULL, FALSE);

  if (window->parent && window->parent->event_mask & GDK_SUBSTRUCTURE_MASK)
    _gdk_make_event (GDK_WINDOW (window), GDK_UNMAP, NULL, FALSE);

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_window_get_display (window));
  if (broadway_display->output)
    {
      broadway_output_hide_surface (broadway_display->output, impl->id);
      queue_dirty_flush (broadway_display);
    }

  if (broadway_display->mouse_in_toplevel == window)
    {
      /* TODO: Send leave + enter event, update cursors, etc */
      broadway_display->mouse_in_toplevel = NULL;
    }

  _gdk_window_clear_update_area (window);
}

static void
gdk_window_broadway_withdraw (GdkWindow *window)
{
  gdk_window_broadway_hide (window);
}

static void
gdk_window_broadway_move_resize (GdkWindow *window,
				 gboolean   with_move,
				 gint       x,
				 gint       y,
				 gint       width,
				 gint       height)
{
  GdkWindowImplBroadway *impl = GDK_WINDOW_IMPL_BROADWAY (window->impl);
  GdkBroadwayDisplay *broadway_display;
  gboolean changed;

  changed = FALSE;

  broadway_display = GDK_BROADWAY_DISPLAY (gdk_window_get_display (window));
  if (with_move)
    {
      changed = TRUE;
      window->x = x;
      window->y = y;
      if (broadway_display->output != NULL)
	{
	  broadway_output_move_surface (broadway_display->output,
					impl->id, x, y);
	  queue_dirty_flush (broadway_display);
	}
    }


  if (width > 0 || height > 0)
    {
      if (width < 1)
	width = 1;

      if (height < 1)
	height = 1;

      if (width != window->width ||
	  height != window->height)
	{
	  changed = TRUE;

	  /* Resize clears the content */
	  impl->dirty = TRUE;
	  impl->last_synced = FALSE;

	  if (broadway_display->output != NULL)
	    {
	      broadway_output_resize_surface (broadway_display->output,
					      impl->id, width, height);
	      queue_dirty_flush (broadway_display);
	    }

	  window->width = width;
	  window->height = height;
	  _gdk_broadway_window_resize_surface (window);
	}
    }

  if (changed)
    {
      GdkEvent *event;
      GList *node;

      event = gdk_event_new (GDK_CONFIGURE);
      event->configure.window = g_object_ref (window);
      event->configure.x = window->x;
      event->configure.y = window->y;
      event->configure.width = window->width;
      event->configure.height = window->height;

      gdk_event_set_device (event, GDK_DISPLAY_OBJECT (broadway_display)->core_pointer);

      node = _gdk_event_queue_append (GDK_DISPLAY_OBJECT (broadway_display), event);
      _gdk_windowing_got_event (GDK_DISPLAY_OBJECT (broadway_display), node, event,
				_gdk_display_get_next_serial (GDK_DISPLAY (broadway_display)) - 1);
    }
}

static gboolean
gdk_window_broadway_reparent (GdkWindow *window,
			      GdkWindow *new_parent,
			      gint       x,
			      gint       y)
{
  return FALSE;
}

static void
gdk_window_broadway_raise (GdkWindow *window)
{
}

static void
gdk_window_broadway_restack_under (GdkWindow *window,
				   GList *native_siblings /* in requested order, first is bottom-most */)
{
}

static void
gdk_window_broadway_restack_toplevel (GdkWindow *window,
				      GdkWindow *sibling,
				      gboolean   above)
{
}

static void
gdk_window_broadway_lower (GdkWindow *window)
{
}


static void
gdk_broadway_window_focus (GdkWindow *window,
			   guint32    timestamp)
{
}

static void
gdk_broadway_window_set_type_hint (GdkWindow        *window,
				   GdkWindowTypeHint hint)
{
}

static GdkWindowTypeHint
gdk_broadway_window_get_type_hint (GdkWindow *window)
{
  return GDK_WINDOW_TYPE_HINT_NORMAL;
}

static void
gdk_broadway_window_set_modal_hint (GdkWindow *window,
				    gboolean   modal)
{
}

static void
gdk_broadway_window_set_skip_taskbar_hint (GdkWindow *window,
					   gboolean   skips_taskbar)
{
}

static void
gdk_broadway_window_set_skip_pager_hint (GdkWindow *window,
					 gboolean   skips_pager)
{
}

static void
gdk_broadway_window_set_urgency_hint (GdkWindow *window,
				      gboolean   urgent)
{
}

static void
gdk_broadway_window_set_geometry_hints (GdkWindow         *window,
					const GdkGeometry *geometry,
					GdkWindowHints     geom_mask)
{
}

static void
gdk_broadway_window_set_title (GdkWindow   *window,
			       const gchar *title)
{
}

static void
gdk_broadway_window_set_role (GdkWindow   *window,
			      const gchar *role)
{
}

static void
gdk_broadway_window_set_startup_id (GdkWindow   *window,
				    const gchar *startup_id)
{
}

static void
gdk_broadway_window_set_transient_for (GdkWindow *window,
				       GdkWindow *parent)
{
  GdkBroadwayDisplay *display;
  GdkWindowImplBroadway *impl;
  int parent_id;

  impl = GDK_WINDOW_IMPL_BROADWAY (window->impl);

  parent_id = 0;
  if (parent)
    parent_id = GDK_WINDOW_IMPL_BROADWAY (parent->impl)->id;

  impl->transient_for = parent_id;

  display = GDK_BROADWAY_DISPLAY (gdk_window_get_display (impl->wrapper));
  if (display->output)
    {
      broadway_output_set_transient_for (display->output, impl->id, impl->transient_for);
      gdk_display_flush (GDK_DISPLAY (display));
    }
}

static void
gdk_window_broadway_set_background (GdkWindow      *window,
				    cairo_pattern_t *pattern)
{
  return;
}

static void
gdk_window_broadway_set_device_cursor (GdkWindow *window,
				       GdkDevice *device,
				       GdkCursor *cursor)
{
  GdkWindowImplBroadway *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_DEVICE (device));

  impl = GDK_WINDOW_IMPL_BROADWAY (window->impl);

  if (!cursor)
    g_hash_table_remove (impl->device_cursor, device);
  else
    {
      _gdk_broadway_cursor_update_theme (cursor);
      g_hash_table_replace (impl->device_cursor,
                            device, gdk_cursor_ref (cursor));
    }

  if (!GDK_WINDOW_DESTROYED (window))
    GDK_DEVICE_GET_CLASS (device)->set_window_cursor (device, window, cursor);
}

GdkCursor *
_gdk_broadway_window_get_cursor (GdkWindow *window)
{
  GdkWindowImplBroadway *impl;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  impl = GDK_WINDOW_IMPL_BROADWAY (window->impl);

  return impl->cursor;
}

static void
gdk_window_broadway_get_geometry (GdkWindow *window,
				  gint      *x,
				  gint      *y,
				  gint      *width,
				  gint      *height)
{
  GdkWindowImplBroadway *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_BROADWAY (window->impl);

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

static gint
gdk_window_broadway_get_root_coords (GdkWindow *window,
				     gint       x,
				     gint       y,
				     gint      *root_x,
				     gint      *root_y)
{
  GdkWindowImplBroadway *impl;

  impl = GDK_WINDOW_IMPL_BROADWAY (window->impl);

  if (root_x)
    *root_x = x + impl->wrapper->x;
  if (root_y)
    *root_y = y + impl->wrapper->y;

  return 1;
}

static void
gdk_broadway_window_get_root_origin (GdkWindow *window,
				     gint      *x,
				     gint      *y)
{
  GdkWindowImplBroadway *impl;

  impl = GDK_WINDOW_IMPL_BROADWAY (window->impl);

  /* TODO: This should take wm frame into account */

  if (x)
    *x = impl->wrapper->x;

  if (y)
    *y = impl->wrapper->x;
}

static void
gdk_broadway_window_get_frame_extents (GdkWindow    *window,
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
gdk_window_broadway_get_device_state (GdkWindow       *window,
				      GdkDevice       *device,
				      gint            *x,
				      gint            *y,
				      GdkModifierType *mask)
{
  GdkWindow *child;

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), FALSE);

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;

  GDK_DEVICE_GET_CLASS (device)->query_state (device, window,
                                              NULL, &child,
                                              NULL, NULL,
                                              x, y, mask);
  return child != NULL;
}

static GdkEventMask
gdk_window_broadway_get_events (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return 0;

  return 0;
}

static void
gdk_window_broadway_set_events (GdkWindow    *window,
				GdkEventMask  event_mask)
{
  if (!GDK_WINDOW_DESTROYED (window))
    {
    }
}

static void
gdk_window_broadway_shape_combine_region (GdkWindow       *window,
					  const cairo_region_t *shape_region,
					  gint             offset_x,
					  gint             offset_y)
{
}

static void
gdk_window_broadway_input_shape_combine_region (GdkWindow       *window,
						const cairo_region_t *shape_region,
						gint             offset_x,
						gint             offset_y)
{
}


static void
gdk_broadway_window_set_override_redirect (GdkWindow *window,
					   gboolean override_redirect)
{
}

static void
gdk_broadway_window_set_accept_focus (GdkWindow *window,
				      gboolean accept_focus)
{
  accept_focus = accept_focus != FALSE;

  if (window->accept_focus != accept_focus)
    {
      window->accept_focus = accept_focus;
    }
}

static void
gdk_broadway_window_set_focus_on_map (GdkWindow *window,
				      gboolean focus_on_map)
{
  focus_on_map = focus_on_map != FALSE;

  if (window->focus_on_map != focus_on_map)
    {
      window->focus_on_map = focus_on_map;
    }
}


static void
gdk_broadway_window_set_icon_list (GdkWindow *window,
				   GList     *pixbufs)
{
}

static void
gdk_broadway_window_set_icon_name (GdkWindow   *window,
				   const gchar *name)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  g_object_set_qdata (G_OBJECT (window), g_quark_from_static_string ("gdk-icon-name-set"),
		      GUINT_TO_POINTER (name != NULL));
}

static void
gdk_broadway_window_iconify (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;
}

static void
gdk_broadway_window_deiconify (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;
}

static void
gdk_broadway_window_stick (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

static void
gdk_broadway_window_unstick (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

static void
gdk_broadway_window_maximize (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

static void
gdk_broadway_window_unmaximize (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

static void
gdk_broadway_window_fullscreen (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

static void
gdk_broadway_window_unfullscreen (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

static void
gdk_broadway_window_set_keep_above (GdkWindow *window,
				    gboolean   setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

static void
gdk_broadway_window_set_keep_below (GdkWindow *window, gboolean setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

static GdkWindow *
gdk_broadway_window_get_group (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return NULL;

  return window;
}

static void
gdk_broadway_window_set_group (GdkWindow *window,
			       GdkWindow *leader)
{
}

static void
gdk_broadway_window_set_decorations (GdkWindow      *window,
				     GdkWMDecoration decorations)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

static gboolean
gdk_broadway_window_get_decorations (GdkWindow       *window,
				     GdkWMDecoration *decorations)
{
  gboolean result = FALSE;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return FALSE;

  return result;
}

static void
gdk_broadway_window_set_functions (GdkWindow    *window,
				   GdkWMFunction functions)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;
}

static cairo_region_t *
gdk_broadway_window_get_shape (GdkWindow *window)
{
  return NULL;
}

static cairo_region_t *
gdk_broadway_window_get_input_shape (GdkWindow *window)
{
  return NULL;
}


static gboolean
gdk_window_broadway_set_static_gravities (GdkWindow *window,
					  gboolean   use_static)
{
  return TRUE;
}

static void
gdk_broadway_window_begin_resize_drag (GdkWindow     *window,
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

static void
gdk_broadway_window_begin_move_drag (GdkWindow *window,
				     gint       button,
				     gint       root_x,
				     gint       root_y,
				     guint32    timestamp)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

}

static void
gdk_broadway_window_enable_synchronized_configure (GdkWindow *window)
{
  if (!GDK_IS_WINDOW_IMPL_BROADWAY (window->impl))
    return;
}

static void
gdk_broadway_window_configure_finished (GdkWindow *window)
{
  if (!WINDOW_IS_TOPLEVEL (window))
    return;
}

static gboolean
gdk_broadway_window_beep (GdkWindow *window)
{
  return FALSE;
}

static void
gdk_broadway_window_set_opacity (GdkWindow *window,
				 gdouble    opacity)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  if (opacity < 0)
    opacity = 0;
  else if (opacity > 1)
    opacity = 1;
}

static void
gdk_broadway_window_set_composited (GdkWindow *window,
				    gboolean   composited)
{
}

static void
gdk_broadway_window_process_updates_recurse (GdkWindow *window,
					     cairo_region_t *region)
{
  GdkWindowImplBroadway *impl;

  _gdk_window_process_updates_recurse (window, region);

  impl = GDK_WINDOW_IMPL_BROADWAY (window->impl);
  impl->dirty = TRUE;
  queue_dirty_flush (GDK_BROADWAY_DISPLAY (gdk_window_get_display (window)));
}

void
_gdk_broadway_display_before_process_all_updates (GdkDisplay *display)
{
}

void
_gdk_broadway_display_after_process_all_updates (GdkDisplay *display)
{
}

gboolean
_gdk_broadway_window_queue_antiexpose (GdkWindow *window,
				       cairo_region_t *area)
{
  return TRUE;
}

static void
copy_region (cairo_surface_t *surface,
	     cairo_region_t *area,
	     gint            dx,
	     gint            dy)
{
  cairo_t *cr;

  cr = cairo_create (surface);

  gdk_cairo_region (cr, area);
  cairo_clip (cr);

  /* NB: This is a self-copy and Cairo doesn't support that yet.
   * So we do a litle trick.
   */
  cairo_push_group (cr);

  cairo_set_source_surface (cr, surface, dx, dy);
  cairo_paint (cr);

  cairo_pop_group_to_source (cr);
  cairo_paint (cr);

  cairo_destroy (cr);
}

void
_gdk_broadway_window_translate (GdkWindow      *window,
				cairo_region_t *area,
				gint            dx,
				gint            dy)
{
  GdkWindowImplBroadway *impl;
  GdkBroadwayDisplay *broadway_display;
  int n_rects, i;
  BroadwayRect *rects;
  cairo_rectangle_int_t rect;

  impl = GDK_WINDOW_IMPL_BROADWAY (window->impl);

  if (impl->surface)
    {
      copy_region (impl->surface, area, dx, dy);
      broadway_display = GDK_BROADWAY_DISPLAY (gdk_window_get_display (window));
      if (GDK_WINDOW_IMPL_BROADWAY (impl)->last_synced &&
	  broadway_display->output)
	{
	  copy_region (impl->last_surface, area, dx, dy);
	  n_rects = cairo_region_num_rectangles (area);
	  rects = g_new (BroadwayRect, n_rects);
	  for (i = 0; i < n_rects; i++)
	    {
	      cairo_region_get_rectangle (area, i, &rect);
	      rects[i].x = rect.x;
	      rects[i].y = rect.y;
	      rects[i].width = rect.width;
	      rects[i].height = rect.height;
	    }
	  broadway_output_copy_rectangles (broadway_display->output,
					   GDK_WINDOW_IMPL_BROADWAY (impl)->id,
					   rects, n_rects, dx, dy);
	  queue_dirty_flush (broadway_display);
	  g_free (rects);
	}
    }
}

guint32
gdk_broadway_get_last_seen_time (GdkWindow  *window)
{
  GdkDisplay *display;

  display = gdk_window_get_display (window);
  _gdk_broadway_display_consume_all_input (display);
  return (guint32) GDK_BROADWAY_DISPLAY (display)->last_seen_time;
}

static void
gdk_window_impl_broadway_class_init (GdkWindowImplBroadwayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkWindowImplClass *impl_class = GDK_WINDOW_IMPL_CLASS (klass);

  object_class->finalize = gdk_window_impl_broadway_finalize;

  impl_class->ref_cairo_surface = gdk_window_broadway_ref_cairo_surface;
  impl_class->show = gdk_window_broadway_show;
  impl_class->hide = gdk_window_broadway_hide;
  impl_class->withdraw = gdk_window_broadway_withdraw;
  impl_class->set_events = gdk_window_broadway_set_events;
  impl_class->get_events = gdk_window_broadway_get_events;
  impl_class->raise = gdk_window_broadway_raise;
  impl_class->lower = gdk_window_broadway_lower;
  impl_class->restack_under = gdk_window_broadway_restack_under;
  impl_class->restack_toplevel = gdk_window_broadway_restack_toplevel;
  impl_class->move_resize = gdk_window_broadway_move_resize;
  impl_class->set_background = gdk_window_broadway_set_background;
  impl_class->reparent = gdk_window_broadway_reparent;
  impl_class->set_device_cursor = gdk_window_broadway_set_device_cursor;
  impl_class->get_geometry = gdk_window_broadway_get_geometry;
  impl_class->get_root_coords = gdk_window_broadway_get_root_coords;
  impl_class->get_device_state = gdk_window_broadway_get_device_state;
  impl_class->shape_combine_region = gdk_window_broadway_shape_combine_region;
  impl_class->input_shape_combine_region = gdk_window_broadway_input_shape_combine_region;
  impl_class->set_static_gravities = gdk_window_broadway_set_static_gravities;
  impl_class->queue_antiexpose = _gdk_broadway_window_queue_antiexpose;
  impl_class->translate = _gdk_broadway_window_translate;
  impl_class->destroy = _gdk_broadway_window_destroy;
  impl_class->destroy_foreign = gdk_broadway_window_destroy_foreign;
  impl_class->resize_cairo_surface = gdk_window_broadway_resize_cairo_surface;
  impl_class->get_shape = gdk_broadway_window_get_shape;
  impl_class->get_input_shape = gdk_broadway_window_get_input_shape;
  impl_class->beep = gdk_broadway_window_beep;

  impl_class->focus = gdk_broadway_window_focus;
  impl_class->set_type_hint = gdk_broadway_window_set_type_hint;
  impl_class->get_type_hint = gdk_broadway_window_get_type_hint;
  impl_class->set_modal_hint = gdk_broadway_window_set_modal_hint;
  impl_class->set_skip_taskbar_hint = gdk_broadway_window_set_skip_taskbar_hint;
  impl_class->set_skip_pager_hint = gdk_broadway_window_set_skip_pager_hint;
  impl_class->set_urgency_hint = gdk_broadway_window_set_urgency_hint;
  impl_class->set_geometry_hints = gdk_broadway_window_set_geometry_hints;
  impl_class->set_title = gdk_broadway_window_set_title;
  impl_class->set_role = gdk_broadway_window_set_role;
  impl_class->set_startup_id = gdk_broadway_window_set_startup_id;
  impl_class->set_transient_for = gdk_broadway_window_set_transient_for;
  impl_class->get_root_origin = gdk_broadway_window_get_root_origin;
  impl_class->get_frame_extents = gdk_broadway_window_get_frame_extents;
  impl_class->set_override_redirect = gdk_broadway_window_set_override_redirect;
  impl_class->set_accept_focus = gdk_broadway_window_set_accept_focus;
  impl_class->set_focus_on_map = gdk_broadway_window_set_focus_on_map;
  impl_class->set_icon_list = gdk_broadway_window_set_icon_list;
  impl_class->set_icon_name = gdk_broadway_window_set_icon_name;
  impl_class->iconify = gdk_broadway_window_iconify;
  impl_class->deiconify = gdk_broadway_window_deiconify;
  impl_class->stick = gdk_broadway_window_stick;
  impl_class->unstick = gdk_broadway_window_unstick;
  impl_class->maximize = gdk_broadway_window_maximize;
  impl_class->unmaximize = gdk_broadway_window_unmaximize;
  impl_class->fullscreen = gdk_broadway_window_fullscreen;
  impl_class->unfullscreen = gdk_broadway_window_unfullscreen;
  impl_class->set_keep_above = gdk_broadway_window_set_keep_above;
  impl_class->set_keep_below = gdk_broadway_window_set_keep_below;
  impl_class->get_group = gdk_broadway_window_get_group;
  impl_class->set_group = gdk_broadway_window_set_group;
  impl_class->set_decorations = gdk_broadway_window_set_decorations;
  impl_class->get_decorations = gdk_broadway_window_get_decorations;
  impl_class->set_functions = gdk_broadway_window_set_functions;
  impl_class->set_functions = gdk_broadway_window_set_functions;
  impl_class->begin_resize_drag = gdk_broadway_window_begin_resize_drag;
  impl_class->begin_move_drag = gdk_broadway_window_begin_move_drag;
  impl_class->enable_synchronized_configure = gdk_broadway_window_enable_synchronized_configure;
  impl_class->configure_finished = gdk_broadway_window_configure_finished;
  impl_class->set_opacity = gdk_broadway_window_set_opacity;
  impl_class->set_composited = gdk_broadway_window_set_composited;
  impl_class->destroy_notify = gdk_broadway_window_destroy_notify;
  impl_class->register_dnd = _gdk_broadway_window_register_dnd;
  impl_class->drag_begin = _gdk_broadway_window_drag_begin;
  impl_class->process_updates_recurse = gdk_broadway_window_process_updates_recurse;
  impl_class->sync_rendering = _gdk_broadway_window_sync_rendering;
  impl_class->simulate_key = _gdk_broadway_window_simulate_key;
  impl_class->simulate_button = _gdk_broadway_window_simulate_button;
  impl_class->get_property = _gdk_broadway_window_get_property;
  impl_class->change_property = _gdk_broadway_window_change_property;
  impl_class->delete_property = _gdk_broadway_window_delete_property;
  impl_class->get_drag_protocol = _gdk_broadway_window_get_drag_protocol;
}
