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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

/* LIMITATIONS:
 *
 * Offscreen windows can't be the child of a foreign window,
 *   nor contain foreign windows
 * GDK_POINTER_MOTION_HINT_MASK isn't effective
 */

typedef struct _GdkOffscreenWindow      GdkOffscreenWindow;
typedef struct _GdkOffscreenWindowClass GdkOffscreenWindowClass;

struct _GdkOffscreenWindow
{
  GdkDrawable parent_instance;

  GdkWindow *wrapper;

  cairo_surface_t *surface;
  GdkWindow *embedder;
};

struct _GdkOffscreenWindowClass
{
  GdkDrawableClass parent_class;
};

#define GDK_TYPE_OFFSCREEN_WINDOW            (gdk_offscreen_window_get_type())
#define GDK_OFFSCREEN_WINDOW(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_OFFSCREEN_WINDOW, GdkOffscreenWindow))
#define GDK_IS_OFFSCREEN_WINDOW(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_OFFSCREEN_WINDOW))
#define GDK_OFFSCREEN_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_OFFSCREEN_WINDOW, GdkOffscreenWindowClass))
#define GDK_IS_OFFSCREEN_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_OFFSCREEN_WINDOW))
#define GDK_OFFSCREEN_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_OFFSCREEN_WINDOW, GdkOffscreenWindowClass))

static void       gdk_offscreen_window_impl_iface_init    (GdkWindowImplIface         *iface);
static void       gdk_offscreen_window_hide               (GdkWindow                  *window);

G_DEFINE_TYPE_WITH_CODE (GdkOffscreenWindow,
			 gdk_offscreen_window,
			 GDK_TYPE_DRAWABLE,
			 G_IMPLEMENT_INTERFACE (GDK_TYPE_WINDOW_IMPL,
						gdk_offscreen_window_impl_iface_init));


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
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  GdkOffscreenWindow *offscreen;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);

  gdk_offscreen_window_set_embedder (window, NULL);
  
  if (!recursing)
    gdk_offscreen_window_hide (window);
}

static cairo_surface_t *
get_surface (GdkOffscreenWindow *offscreen)
{
  if (! offscreen->surface)
    {
      GdkWindowObject *private = (GdkWindowObject *) offscreen->wrapper;

      g_signal_emit_by_name (private, "create-surface",
                             private->width,
                             private->height,
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
gdk_offscreen_window_ref_cairo_surface (GdkDrawable *drawable)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);

  return cairo_surface_reference (get_surface (offscreen));
}

cairo_surface_t *
_gdk_offscreen_window_create_surface (GdkWindow *offscreen,
                                      gint       width,
                                      gint       height)
{
  GdkWindowObject *private = (GdkWindowObject *) offscreen;
  cairo_surface_t *similar;
  cairo_surface_t *surface;

  g_return_val_if_fail (GDK_IS_OFFSCREEN_WINDOW (private->impl), NULL);

  similar = _gdk_drawable_ref_cairo_surface ((GdkWindow *)private->parent);

  surface = cairo_surface_create_similar (similar,
                                          /* FIXME: use visual */
                                          CAIRO_CONTENT_COLOR,
                                          width,
                                          height);

  cairo_surface_destroy (similar);

  return surface;
}

void
_gdk_offscreen_window_new (GdkWindow     *window,
			   GdkWindowAttr *attributes,
			   gint           attributes_mask)
{
  GdkWindowObject *private;
  GdkOffscreenWindow *offscreen;

  g_return_if_fail (attributes != NULL);

  if (attributes->wclass != GDK_INPUT_OUTPUT)
    return; /* Can't support input only offscreens */

  private = (GdkWindowObject *)window;

  if (private->parent != NULL && GDK_WINDOW_DESTROYED (private->parent))
    return;

  private->impl = g_object_new (GDK_TYPE_OFFSCREEN_WINDOW, NULL);
  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);
  offscreen->wrapper = window;
}

static gboolean
gdk_offscreen_window_reparent (GdkWindow *window,
			       GdkWindow *new_parent,
			       gint       x,
			       gint       y)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *new_parent_private = (GdkWindowObject *)new_parent;
  GdkWindowObject *old_parent;
  gboolean was_mapped;

  if (new_parent)
    {
      /* No input-output children of input-only windows */
      if (new_parent_private->input_only && !private->input_only)
	return FALSE;

      /* Don't create loops in hierarchy */
      if (is_parent_of (window, new_parent))
	return FALSE;
    }

  was_mapped = GDK_WINDOW_IS_MAPPED (window);

  gdk_window_hide (window);

  if (private->parent)
    private->parent->children = g_list_remove (private->parent->children, window);

  old_parent = private->parent;
  private->parent = new_parent_private;
  private->x = x;
  private->y = y;

  if (new_parent_private)
    private->parent->children = g_list_prepend (private->parent->children, window);

  _gdk_synthesize_crossing_events_for_geometry_change (window);
  if (old_parent)
    _gdk_synthesize_crossing_events_for_geometry_change (GDK_WINDOW (old_parent));

  return was_mapped;
}

static void
from_embedder (GdkWindow *window,
	       double embedder_x, double embedder_y,
	       double *offscreen_x, double *offscreen_y)
{
  GdkWindowObject *private;

  private = (GdkWindowObject *)window;

  g_signal_emit_by_name (private->impl_window,
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
  GdkWindowObject *private;

  private = (GdkWindowObject *)window;

  g_signal_emit_by_name (private->impl_window,
			 "to-embedder",
			 offscreen_x, offscreen_y,
			 embedder_x, embedder_y,
			 NULL);
}

static gint
gdk_offscreen_window_get_root_coords (GdkWindow *window,
				      gint       x,
				      gint       y,
				      gint      *root_x,
				      gint      *root_y)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  GdkOffscreenWindow *offscreen;
  int tmpx, tmpy;

  tmpx = x;
  tmpy = y;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);
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

  return TRUE;
}

static gboolean
gdk_offscreen_window_get_device_state (GdkWindow       *window,
                                       GdkDevice       *device,
                                       gint            *x,
                                       gint            *y,
                                       GdkModifierType *mask)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  GdkOffscreenWindow *offscreen;
  int tmpx, tmpy;
  double dtmpx, dtmpy;
  GdkModifierType tmpmask;

  tmpx = 0;
  tmpy = 0;
  tmpmask = 0;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);
  if (offscreen->embedder != NULL)
    {
      gdk_window_get_device_position (offscreen->embedder, device, &tmpx, &tmpy, &tmpmask);
      from_embedder (window,
		     tmpx, tmpy,
		     &dtmpx, &dtmpy);
      tmpx = floor (dtmpx + 0.5);
      tmpy = floor (dtmpy + 0.5);
    }

  if (x)
    *x = tmpx;
  if (y)
    *y = tmpy;
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
 * Returns: The offscreen surface, or %NULL if not offscreen
 */
cairo_surface_t *
gdk_offscreen_window_get_surface (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkOffscreenWindow *offscreen;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (!GDK_IS_OFFSCREEN_WINDOW (private->impl))
    return NULL;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);

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
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkOffscreenWindow *offscreen;
  gint dx, dy, dw, dh;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  if (private->destroyed)
    return;

  dx = x - private->x;
  dy = y - private->y;
  dw = width - private->width;
  dh = height - private->height;

  private->x = x;
  private->y = y;

  if (private->width != width ||
      private->height != height)
    {
      private->width = width;
      private->height = height;

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

  if (GDK_WINDOW_IS_MAPPED (private))
    {
      // TODO: Only invalidate new area, i.e. for larger windows
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
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkOffscreenWindow *offscreen;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);

  if (!with_move)
    {
      x = private->x;
      y = private->y;
    }

  if (width < 0)
    width = private->width;

  if (height < 0)
    height = private->height;

  gdk_offscreen_window_move_resize_internal (window, x, y,
					     width, height,
					     TRUE);
}

static void
gdk_offscreen_window_show (GdkWindow *window,
			   gboolean already_mapped)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkRectangle area = { 0, 0, private->width, private->height };

  gdk_window_invalidate_rect (window, &area, FALSE);
}


static void
gdk_offscreen_window_hide (GdkWindow *window)
{
  GdkWindowObject *private;
  GdkOffscreenWindow *offscreen;
  GdkDisplay *display;

  g_return_if_fail (window != NULL);

  private = (GdkWindowObject*) window;
  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);

  /* May need to break grabs on children */
  display = gdk_window_get_display (window);

  /* TODO: This needs updating to the new grab world */
#if 0
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
				   gint      *height,
				   gint      *depth)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (x)
	*x = private->x;
      if (y)
	*y = private->y;
      if (width)
	*width = private->width;
      if (height)
	*height = private->height;
      if (depth)
	*depth = private->depth;
    }
}

static gboolean
gdk_offscreen_window_queue_antiexpose (GdkWindow *window,
				       cairo_region_t *area)
{
  return FALSE;
}

static void
gdk_offscreen_window_translate (GdkWindow      *window,
                                cairo_region_t *area,
                                gint            dx,
                                gint            dy)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (((GdkWindowObject *) window)->impl);

  if (offscreen->surface)
    {
      cairo_t *cr;

      cr = cairo_create (offscreen->surface);

      area = cairo_region_copy (area);

      gdk_cairo_region (cr, area);
      cairo_clip (cr);

      /* NB: This is a self-copy and Cairo doesn't support that yet.
       * So we do a litle trick.
       */
      cairo_push_group (cr);

      cairo_set_source_surface (cr, offscreen->surface, dx, dy);
      cairo_paint (cr);

      cairo_pop_group_to_source (cr);
      cairo_paint (cr);

      cairo_destroy (cr);
    }

  _gdk_window_add_damage (window, area);
}

static cairo_surface_t *
gdk_offscreen_window_resize_cairo_surface (GdkWindow       *window,
                                           cairo_surface_t *surface,
                                           gint             width,
                                           gint             height)
{
  /* No-op.  The surface gets resized in
   * gdk_offscreen_window_move_resize_internal().
   */
  return surface;
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
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkOffscreenWindow *offscreen;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (!GDK_IS_OFFSCREEN_WINDOW (private->impl))
    return;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);

  if (embedder)
    {
      g_object_ref (embedder);
      GDK_WINDOW_OBJECT (embedder)->num_offscreen_children++;
    }

  if (offscreen->embedder)
    {
      g_object_unref (offscreen->embedder);
      GDK_WINDOW_OBJECT (offscreen->embedder)->num_offscreen_children--;
    }

  offscreen->embedder = embedder;
}

/**
 * gdk_offscreen_window_get_embedder:
 * @window: a #GdkWindow
 *
 * Gets the window that @window is embedded in.
 *
 * Returns: the embedding #GdkWindow, or %NULL if @window is not an
 *     embedded offscreen window
 *
 * Since: 2.18
 */
GdkWindow *
gdk_offscreen_window_get_embedder (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkOffscreenWindow *offscreen;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (!GDK_IS_OFFSCREEN_WINDOW (private->impl))
    return NULL;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);

  return offscreen->embedder;
}

static void
gdk_offscreen_window_class_init (GdkOffscreenWindowClass *klass)
{
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_offscreen_window_finalize;

  drawable_class->ref_cairo_surface = gdk_offscreen_window_ref_cairo_surface;
}

static void
gdk_offscreen_window_impl_iface_init (GdkWindowImplIface *iface)
{
  iface->show = gdk_offscreen_window_show;
  iface->hide = gdk_offscreen_window_hide;
  iface->withdraw = gdk_offscreen_window_withdraw;
  iface->raise = gdk_offscreen_window_raise;
  iface->lower = gdk_offscreen_window_lower;
  iface->move_resize = gdk_offscreen_window_move_resize;
  iface->set_background = gdk_offscreen_window_set_background;
  iface->get_events = gdk_offscreen_window_get_events;
  iface->set_events = gdk_offscreen_window_set_events;
  iface->reparent = gdk_offscreen_window_reparent;
  iface->get_geometry = gdk_offscreen_window_get_geometry;
  iface->shape_combine_region = gdk_offscreen_window_shape_combine_region;
  iface->input_shape_combine_region = gdk_offscreen_window_input_shape_combine_region;
  iface->set_static_gravities = gdk_offscreen_window_set_static_gravities;
  iface->queue_antiexpose = gdk_offscreen_window_queue_antiexpose;
  iface->translate = gdk_offscreen_window_translate;
  iface->get_root_coords = gdk_offscreen_window_get_root_coords;
  iface->get_device_state = gdk_offscreen_window_get_device_state;
  iface->destroy = gdk_offscreen_window_destroy;
  iface->resize_cairo_surface = gdk_offscreen_window_resize_cairo_surface;
}
