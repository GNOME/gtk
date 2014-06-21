/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * Modified by the GTK+ Team and others 1997-2005.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gdkwindow.h"
#include "gdkinternals.h"
#include "gdkwindowimpl.h"

#include <math.h>

#include "fallback-c89.c"

/* LIMITATIONS:
 *
 * Offscreen windows can’t be the child of a foreign window,
 *   nor contain foreign windows
 * GDK_POINTER_MOTION_HINT_MASK isn't effective
 */

typedef struct _GdkOffscreenWindow      GdkOffscreenWindow;
typedef struct _GdkOffscreenWindowClass GdkOffscreenWindowClass;

struct _GdkOffscreenWindow
{
  GdkWindowImpl parent_instance;

  GdkWindow *wrapper;

  cairo_surface_t *surface;
  GdkWindow *embedder;
};

struct _GdkOffscreenWindowClass
{
  GdkWindowImplClass parent_class;
};

#define GDK_TYPE_OFFSCREEN_WINDOW            (gdk_offscreen_window_get_type())
#define GDK_OFFSCREEN_WINDOW(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_OFFSCREEN_WINDOW, GdkOffscreenWindow))
#define GDK_IS_OFFSCREEN_WINDOW(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_OFFSCREEN_WINDOW))
#define GDK_OFFSCREEN_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_OFFSCREEN_WINDOW, GdkOffscreenWindowClass))
#define GDK_IS_OFFSCREEN_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_OFFSCREEN_WINDOW))
#define GDK_OFFSCREEN_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_OFFSCREEN_WINDOW, GdkOffscreenWindowClass))

static void       gdk_offscreen_window_hide               (GdkWindow                  *window);

G_DEFINE_TYPE (GdkOffscreenWindow, gdk_offscreen_window, GDK_TYPE_WINDOW_IMPL)


static void
gdk_offscreen_window_finalize (GObject *object)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (object);

  if (offscreen->surface)
    cairo_surface_destroy (offscreen->surface);

  G_OBJECT_CLASS (gdk_offscreen_window_parent_class)->finalize (object);
}

static void
gdk_offscreen_window_init (GdkOffscreenWindow *window)
{
}

static void
gdk_offscreen_window_destroy (GdkWindow *window,
                              gboolean   recursing,
                              gboolean   foreign_destroy)
{
  gdk_offscreen_window_set_embedder (window, NULL);

  if (!recursing)
    gdk_offscreen_window_hide (window);
}

static cairo_surface_t *
get_surface (GdkOffscreenWindow *offscreen)
{
  if (! offscreen->surface)
    {
      GdkWindow *window = offscreen->wrapper;

      g_signal_emit_by_name (window, "create-surface",
                             window->width,
                             window->height,
                             &offscreen->surface);
    }

  return offscreen->surface;
}

static gboolean
is_parent_of (GdkWindow *parent,
	      GdkWindow *child)
{
  GdkWindow *w;

  w = child;
  while (w != NULL)
    {
      if (w == parent)
	return TRUE;

      w = gdk_window_get_parent (w);
    }

  return FALSE;
}

static cairo_surface_t *
gdk_offscreen_window_ref_cairo_surface (GdkWindow *window)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (window->impl);

  return cairo_surface_reference (get_surface (offscreen));
}

cairo_surface_t *
_gdk_offscreen_window_create_surface (GdkWindow *offscreen,
                                      gint       width,
                                      gint       height)
{
  g_return_val_if_fail (GDK_IS_OFFSCREEN_WINDOW (offscreen->impl), NULL);

  return gdk_window_create_similar_surface (offscreen->parent,
					    CAIRO_CONTENT_COLOR_ALPHA, 
					    width, height);
}

void
_gdk_offscreen_window_new (GdkWindow     *window,
			   GdkWindowAttr *attributes,
			   gint           attributes_mask)
{
  GdkOffscreenWindow *offscreen;

  g_return_if_fail (attributes != NULL);

  if (attributes->wclass != GDK_INPUT_OUTPUT)
    return; /* Can't support input only offscreens */

  if (window->parent != NULL && GDK_WINDOW_DESTROYED (window->parent))
    return;

  window->impl = g_object_new (GDK_TYPE_OFFSCREEN_WINDOW, NULL);
  offscreen = GDK_OFFSCREEN_WINDOW (window->impl);
  offscreen->wrapper = window;
}

static gboolean
gdk_offscreen_window_reparent (GdkWindow *window,
			       GdkWindow *new_parent,
			       gint       x,
			       gint       y)
{
  GdkWindow *old_parent;
  gboolean was_mapped;

  if (new_parent)
    {
      /* No input-output children of input-only windows */
      if (new_parent->input_only && !window->input_only)
	return FALSE;

      /* Don't create loops in hierarchy */
      if (is_parent_of (window, new_parent))
	return FALSE;
    }

  was_mapped = GDK_WINDOW_IS_MAPPED (window);

  gdk_window_hide (window);

  if (window->parent)
    window->parent->children = g_list_remove (window->parent->children, window);

  old_parent = window->parent;
  window->parent = new_parent;
  window->x = x;
  window->y = y;

  if (new_parent)
    window->parent->children = g_list_prepend (window->parent->children, window);

  _gdk_synthesize_crossing_events_for_geometry_change (window);
  if (old_parent)
    _gdk_synthesize_crossing_events_for_geometry_change (old_parent);

  return was_mapped;
}

static void
gdk_offscreen_window_set_device_cursor (GdkWindow     *window,
					GdkDevice     *device,
					GdkCursor     *cursor)
{
}

static void
from_embedder (GdkWindow *window,
	       double embedder_x, double embedder_y,
	       double *offscreen_x, double *offscreen_y)
{
  g_signal_emit_by_name (window->impl_window,
			 "from-embedder",
			 embedder_x, embedder_y,
			 offscreen_x, offscreen_y,
			 NULL);
}

static void
to_embedder (GdkWindow *window,
	     double offscreen_x, double offscreen_y,
	     double *embedder_x, double *embedder_y)
{
  g_signal_emit_by_name (window->impl_window,
			 "to-embedder",
			 offscreen_x, offscreen_y,
			 embedder_x, embedder_y,
			 NULL);
}

static void
gdk_offscreen_window_get_root_coords (GdkWindow *window,
				      gint       x,
				      gint       y,
				      gint      *root_x,
				      gint      *root_y)
{
  GdkOffscreenWindow *offscreen;
  int tmpx, tmpy;

  tmpx = x;
  tmpy = y;

  offscreen = GDK_OFFSCREEN_WINDOW (window->impl);
  if (offscreen->embedder)
    {
      double dx, dy;
      to_embedder (window,
		   x, y,
		   &dx, &dy);
      tmpx = floor (dx + 0.5);
      tmpy = floor (dy + 0.5);
      gdk_window_get_root_coords (offscreen->embedder,
				  tmpx, tmpy,
				  &tmpx, &tmpy);

    }

  if (root_x)
    *root_x = tmpx;
  if (root_y)
    *root_y = tmpy;
}

static gboolean
gdk_offscreen_window_get_device_state (GdkWindow       *window,
                                       GdkDevice       *device,
                                       gdouble         *x,
                                       gdouble         *y,
                                       GdkModifierType *mask)
{
  GdkOffscreenWindow *offscreen;
  double tmpx, tmpy;
  double dtmpx, dtmpy;
  GdkModifierType tmpmask;

  tmpx = 0;
  tmpy = 0;
  tmpmask = 0;

  offscreen = GDK_OFFSCREEN_WINDOW (window->impl);
  if (offscreen->embedder != NULL)
    {
      gdk_window_get_device_position_double (offscreen->embedder, device, &tmpx, &tmpy, &tmpmask);
      from_embedder (window,
		     tmpx, tmpy,
		     &dtmpx, &dtmpy);
      tmpx = dtmpx;
      tmpy = dtmpy;
    }

  if (x)
    *x = round (tmpx);
  if (y)
    *y = round (tmpy);
  if (mask)
    *mask = tmpmask;
  return TRUE;
}

/**
 * gdk_offscreen_window_get_surface:
 * @window: a #GdkWindow
 *
 * Gets the offscreen surface that an offscreen window renders into.
 * If you need to keep this around over window resizes, you need to
 * add a reference to it.
 *
 * Returns: (nullable) (transfer none): The offscreen surface, or
 *   %NULL if not offscreen
 */
cairo_surface_t *
gdk_offscreen_window_get_surface (GdkWindow *window)
{
  GdkOffscreenWindow *offscreen;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (!GDK_IS_OFFSCREEN_WINDOW (window->impl))
    return NULL;

  offscreen = GDK_OFFSCREEN_WINDOW (window->impl);

  return get_surface (offscreen);
}

static void
gdk_offscreen_window_raise (GdkWindow *window)
{
  /* gdk_window_raise already changed the stacking order */
  _gdk_synthesize_crossing_events_for_geometry_change (window);
}

static void
gdk_offscreen_window_lower (GdkWindow *window)
{
  /* gdk_window_lower already changed the stacking order */
  _gdk_synthesize_crossing_events_for_geometry_change (window);
}

static void
gdk_offscreen_window_move_resize_internal (GdkWindow *window,
                                           gint       x,
                                           gint       y,
                                           gint       width,
                                           gint       height,
                                           gboolean   send_expose_events)
{
  GdkOffscreenWindow *offscreen;

  offscreen = GDK_OFFSCREEN_WINDOW (window->impl);

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  if (window->destroyed)
    return;

  window->x = x;
  window->y = y;

  if (window->width != width ||
      window->height != height)
    {
      window->width = width;
      window->height = height;

      if (offscreen->surface)
        {
          cairo_surface_t *old_surface;
          cairo_t *cr;

          old_surface = offscreen->surface;
          offscreen->surface = NULL;

          offscreen->surface = get_surface (offscreen);

          cr = cairo_create (offscreen->surface);
          cairo_set_source_surface (cr, old_surface, 0, 0);
          cairo_paint (cr);
          cairo_destroy (cr);

          cairo_surface_destroy (old_surface);
        }
    }

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      /* TODO: Only invalidate new area, i.e. for larger windows */
      gdk_window_invalidate_rect (window, NULL, TRUE);
      _gdk_synthesize_crossing_events_for_geometry_change (window);
    }
}

static void
gdk_offscreen_window_move_resize (GdkWindow *window,
                                  gboolean   with_move,
                                  gint       x,
                                  gint       y,
                                  gint       width,
                                  gint       height)
{
  if (!with_move)
    {
      x = window->x;
      y = window->y;
    }

  if (width < 0)
    width = window->width;

  if (height < 0)
    height = window->height;

  gdk_offscreen_window_move_resize_internal (window,
                                             x, y, width, height,
                                             TRUE);
}

static void
gdk_offscreen_window_show (GdkWindow *window,
			   gboolean already_mapped)
{
  GdkRectangle area = { 0, 0, window->width, window->height };

  gdk_window_invalidate_rect (window, &area, FALSE);
}


static void
gdk_offscreen_window_hide (GdkWindow *window)
{
  /* TODO: This needs updating to the new grab world */
#if 0
  GdkOffscreenWindow *offscreen;
  GdkDisplay *display;

  g_return_if_fail (window != NULL);

  offscreen = GDK_OFFSCREEN_WINDOW (window->impl);

  /* May need to break grabs on children */
  display = gdk_window_get_display (window);

  if (display->pointer_grab.window != NULL)
    {
      if (is_parent_of (window, display->pointer_grab.window))
	{
	  /* Call this ourselves, even though gdk_display_pointer_ungrab
	     does so too, since we want to pass implicit == TRUE so the
	     broken grab event is generated */
	  _gdk_display_unset_has_pointer_grab (display,
					       TRUE,
					       FALSE,
					       GDK_CURRENT_TIME);
	  gdk_display_pointer_ungrab (display, GDK_CURRENT_TIME);
	}
    }
#endif
}

static void
gdk_offscreen_window_withdraw (GdkWindow *window)
{
}

static GdkEventMask
gdk_offscreen_window_get_events (GdkWindow *window)
{
  return 0;
}

static void
gdk_offscreen_window_set_events (GdkWindow       *window,
				 GdkEventMask     event_mask)
{
}

static void
gdk_offscreen_window_set_background (GdkWindow      *window,
				     cairo_pattern_t *pattern)
{
}

static void
gdk_offscreen_window_shape_combine_region (GdkWindow       *window,
					   const cairo_region_t *shape_region,
					   gint             offset_x,
					   gint             offset_y)
{
}

static void
gdk_offscreen_window_input_shape_combine_region (GdkWindow       *window,
						 const cairo_region_t *shape_region,
						 gint             offset_x,
						 gint             offset_y)
{
}

static gboolean
gdk_offscreen_window_set_static_gravities (GdkWindow *window,
					   gboolean   use_static)
{
  return TRUE;
}

static void
gdk_offscreen_window_get_geometry (GdkWindow *window,
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
gdk_offscreen_window_queue_antiexpose (GdkWindow *window,
				       cairo_region_t *area)
{
}

/**
 * gdk_offscreen_window_set_embedder:
 * @window: a #GdkWindow
 * @embedder: the #GdkWindow that @window gets embedded in
 *
 * Sets @window to be embedded in @embedder.
 *
 * To fully embed an offscreen window, in addition to calling this
 * function, it is also necessary to handle the #GdkWindow::pick-embedded-child
 * signal on the @embedder and the #GdkWindow::to-embedder and
 * #GdkWindow::from-embedder signals on @window.
 *
 * Since: 2.18
 */
void
gdk_offscreen_window_set_embedder (GdkWindow     *window,
				   GdkWindow     *embedder)
{
  GdkOffscreenWindow *offscreen;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (!GDK_IS_OFFSCREEN_WINDOW (window->impl))
    return;

  offscreen = GDK_OFFSCREEN_WINDOW (window->impl);

  if (embedder)
    {
      g_object_ref (embedder);
      embedder->num_offscreen_children++;
    }

  if (offscreen->embedder)
    {
      g_object_unref (offscreen->embedder);
      offscreen->embedder->num_offscreen_children--;
    }

  offscreen->embedder = embedder;
}

/**
 * gdk_offscreen_window_get_embedder:
 * @window: a #GdkWindow
 *
 * Gets the window that @window is embedded in.
 *
 * Returns: (nullable) (transfer none): the embedding #GdkWindow, or
 *     %NULL if @window is not an mbedded offscreen window
 *
 * Since: 2.18
 */
GdkWindow *
gdk_offscreen_window_get_embedder (GdkWindow *window)
{
  GdkOffscreenWindow *offscreen;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (!GDK_IS_OFFSCREEN_WINDOW (window->impl))
    return NULL;

  offscreen = GDK_OFFSCREEN_WINDOW (window->impl);

  return offscreen->embedder;
}

static void
gdk_offscreen_window_do_nothing (GdkWindow *window)
{
}

static void
gdk_offscreen_window_set_boolean (GdkWindow *window,
                                  gboolean   setting)
{
}

static void
gdk_offscreen_window_set_string (GdkWindow *window,
				 const gchar *setting)
{
}

static void
gdk_offscreen_window_set_list (GdkWindow *window,
                               GList *list)
{
}

static void
gdk_offscreen_window_set_wmfunctions (GdkWindow	    *window,
				      GdkWMFunction  functions)
{
}

static void
gdk_offscreen_window_set_transient_for (GdkWindow *window,
					GdkWindow *another)
{
}

static void
gdk_offscreen_window_process_updates_recurse (GdkWindow *window,
                                              cairo_region_t *region)
{
  _gdk_window_process_updates_recurse (window, region);
}

static void
gdk_offscreen_window_get_frame_extents (GdkWindow    *window,
					GdkRectangle *rect)
{
  rect->x = window->x;
  rect->y = window->y;
  rect->width = window->width;
  rect->height = window->height;
}

static gint
gdk_offscreen_window_get_scale_factor (GdkWindow *window)
{

  if (GDK_WINDOW_DESTROYED (window))
    return 1;

  return gdk_window_get_scale_factor (window->parent);
}

static void
gdk_offscreen_window_set_opacity (GdkWindow *window, gdouble opacity)
{
}

static void
gdk_offscreen_window_class_init (GdkOffscreenWindowClass *klass)
{
  GdkWindowImplClass *impl_class = GDK_WINDOW_IMPL_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_offscreen_window_finalize;

  impl_class->ref_cairo_surface = gdk_offscreen_window_ref_cairo_surface;
  impl_class->show = gdk_offscreen_window_show;
  impl_class->hide = gdk_offscreen_window_hide;
  impl_class->withdraw = gdk_offscreen_window_withdraw;
  impl_class->set_events = gdk_offscreen_window_set_events;
  impl_class->get_events = gdk_offscreen_window_get_events;
  impl_class->raise = gdk_offscreen_window_raise;
  impl_class->lower = gdk_offscreen_window_lower;
  impl_class->restack_under = NULL;
  impl_class->restack_toplevel = NULL;
  impl_class->move_resize = gdk_offscreen_window_move_resize;
  impl_class->set_background = gdk_offscreen_window_set_background;
  impl_class->reparent = gdk_offscreen_window_reparent;
  impl_class->set_device_cursor = gdk_offscreen_window_set_device_cursor;
  impl_class->get_geometry = gdk_offscreen_window_get_geometry;
  impl_class->get_root_coords = gdk_offscreen_window_get_root_coords;
  impl_class->get_device_state = gdk_offscreen_window_get_device_state;
  impl_class->shape_combine_region = gdk_offscreen_window_shape_combine_region;
  impl_class->input_shape_combine_region = gdk_offscreen_window_input_shape_combine_region;
  impl_class->set_static_gravities = gdk_offscreen_window_set_static_gravities;
  impl_class->queue_antiexpose = gdk_offscreen_window_queue_antiexpose;
  impl_class->destroy = gdk_offscreen_window_destroy;
  impl_class->destroy_foreign = NULL;
  impl_class->get_shape = NULL;
  impl_class->get_input_shape = NULL;
  impl_class->beep = NULL;

  impl_class->focus = NULL;
  impl_class->set_type_hint = NULL;
  impl_class->get_type_hint = NULL;
  impl_class->set_modal_hint = gdk_offscreen_window_set_boolean;
  impl_class->set_skip_taskbar_hint = gdk_offscreen_window_set_boolean;
  impl_class->set_skip_pager_hint = gdk_offscreen_window_set_boolean;
  impl_class->set_urgency_hint = gdk_offscreen_window_set_boolean;
  impl_class->set_geometry_hints = NULL;
  impl_class->set_title = gdk_offscreen_window_set_string;
  impl_class->set_role = gdk_offscreen_window_set_string;
  impl_class->set_startup_id = gdk_offscreen_window_set_string;
  impl_class->set_transient_for = gdk_offscreen_window_set_transient_for;
  impl_class->get_frame_extents = gdk_offscreen_window_get_frame_extents;
  impl_class->set_override_redirect = NULL;
  impl_class->set_accept_focus = gdk_offscreen_window_set_boolean;
  impl_class->set_focus_on_map = gdk_offscreen_window_set_boolean;
  impl_class->set_icon_list = gdk_offscreen_window_set_list;
  impl_class->set_icon_name = gdk_offscreen_window_set_string;
  impl_class->iconify = gdk_offscreen_window_do_nothing;
  impl_class->deiconify = gdk_offscreen_window_do_nothing;
  impl_class->stick = gdk_offscreen_window_do_nothing;
  impl_class->unstick = gdk_offscreen_window_do_nothing;
  impl_class->maximize = gdk_offscreen_window_do_nothing;
  impl_class->unmaximize = gdk_offscreen_window_do_nothing;
  impl_class->fullscreen = gdk_offscreen_window_do_nothing;
  impl_class->unfullscreen = gdk_offscreen_window_do_nothing;
  impl_class->set_keep_above = gdk_offscreen_window_set_boolean;
  impl_class->set_keep_below = gdk_offscreen_window_set_boolean;
  impl_class->get_group = NULL;
  impl_class->set_group = NULL;
  impl_class->set_decorations = NULL;
  impl_class->get_decorations = NULL;
  impl_class->set_functions = gdk_offscreen_window_set_wmfunctions;
  impl_class->begin_resize_drag = NULL;
  impl_class->begin_move_drag = NULL;
  impl_class->enable_synchronized_configure = gdk_offscreen_window_do_nothing;
  impl_class->configure_finished = NULL;
  impl_class->set_opacity = gdk_offscreen_window_set_opacity;
  impl_class->set_composited = NULL;
  impl_class->destroy_notify = NULL;
  impl_class->register_dnd = gdk_offscreen_window_do_nothing;
  impl_class->drag_begin = NULL;
  impl_class->process_updates_recurse = gdk_offscreen_window_process_updates_recurse;
  impl_class->sync_rendering = NULL;
  impl_class->simulate_key = NULL;
  impl_class->simulate_button = NULL;
  impl_class->get_property = NULL;
  impl_class->change_property = NULL;
  impl_class->delete_property = NULL;
  impl_class->get_scale_factor = gdk_offscreen_window_get_scale_factor;
}
