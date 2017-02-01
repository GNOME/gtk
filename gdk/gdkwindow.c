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

#include <cairo-gobject.h>

#include "gdkwindow.h"

#include "gdkrectangle.h"
#include "gdkinternals.h"
#include "gdkintl.h"
#include "gdkscreenprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkdeviceprivate.h"
#include "gdkvisualprivate.h"
#include "gdkmarshalers.h"
#include "gdkframeclockidle.h"
#include "gdkwindowimpl.h"
#include "gdkglcontextprivate.h"
#include "gdkdrawingcontextprivate.h"
#include "gdk-private.h"

#include <math.h>

#include <epoxy/gl.h>

/* for the use of round() */
#include "fallback-c89.c"

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#endif

#undef DEBUG_WINDOW_PRINTING


/**
 * SECTION:windows
 * @Short_description: Onscreen display areas in the target window system
 * @Title: Windows
 *
 * A #GdkWindow is a (usually) rectangular region on the screen.
 * It’s a low-level object, used to implement high-level objects such as
 * #GtkWidget and #GtkWindow on the GTK+ level. A #GtkWindow is a toplevel
 * window, the thing a user might think of as a “window” with a titlebar
 * and so on; a #GtkWindow may contain many #GdkWindows. For example,
 * each #GtkButton has a #GdkWindow associated with it.
 */


/* Historically a GdkWindow always matches a platform native window,
 * be it a toplevel window or a child window. In this setup the
 * GdkWindow (and other GdkDrawables) were platform independent classes,
 * and the actual platform specific implementation was in a delegate
 * object available as “impl” in the window object.
 *
 * With the addition of client side windows this changes a bit. The
 * application-visible GdkWindow object behaves as it did before, but
 * such windows now don't a corresponding native window. Instead subwindows
 * windows are “client side”, i.e. emulated by the gdk code such
 * that clipping, drawing, moving, events etc work as expected.
 *
 * GdkWindows have a pointer to the “impl window” they are in, i.e.
 * the topmost GdkWindow which have the same “impl” value. This is stored
 * in impl_window, which is different from the window itself only for client
 * side windows.
 * All GdkWindows (native or not) track the position of the window in the parent
 * (x, y), the size of the window (width, height), the position of the window
 * with respect to the impl window (abs_x, abs_y). We also track the clip
 * region of the window wrt parent windows, in window-relative coordinates (clip_region).
 */

/* This adds a local value to the GdkVisibilityState enum */
#define GDK_VISIBILITY_NOT_VIEWABLE 3

enum {
  MOVED_TO_RECT,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_CURSOR,
  PROP_DISPLAY,
  LAST_PROP
};

/* Global info */

static void gdk_window_finalize   (GObject              *object);

static void gdk_window_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec);
static void gdk_window_get_property (GObject      *object,
                                     guint         prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec);

static void gdk_window_clear_backing_region (GdkWindow *window);

static void recompute_visible_regions   (GdkWindow *private,
					 gboolean recalculate_children);
static void gdk_window_invalidate_in_parent (GdkWindow *private);
static void update_cursor               (GdkDisplay *display,
                                         GdkDevice  *device);
static void impl_window_add_update_area (GdkWindow *impl_window,
					 cairo_region_t *region);
static void gdk_window_invalidate_region_full (GdkWindow       *window,
					       const cairo_region_t *region,
					       gboolean         invalidate_children);
static void gdk_window_invalidate_rect_full (GdkWindow          *window,
					     const GdkRectangle *rect,
					     gboolean            invalidate_children);
static cairo_surface_t *gdk_window_ref_impl_surface (GdkWindow *window);

static void gdk_window_set_frame_clock (GdkWindow      *window,
                                        GdkFrameClock  *clock);


static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *properties[LAST_PROP] = { NULL, };

G_DEFINE_ABSTRACT_TYPE (GdkWindow, gdk_window, G_TYPE_OBJECT)

#ifdef DEBUG_WINDOW_PRINTING
char *
print_region (cairo_region_t *region)
{
  GString *s = g_string_new ("{");
  if (cairo_region_is_empty (region))
    {
      g_string_append (s, "empty");
    }
  else
    {
      int num = cairo_region_num_rectangles (region);
      cairo_rectangle_int_t r;

      if (num == 1)
	{
	  cairo_region_get_rectangle (region, 0, &r);
	  g_string_append_printf (s, "%dx%d @%d,%d", r.width, r.height, r.x, r.y);
	}
      else
	{
	  int i;
	  cairo_region_get_extents (region, &r);
	  g_string_append_printf (s, "extent: %dx%d @%d,%d, details: ", r.width, r.height, r.x, r.y);
	  for (i = 0; i < num; i++)
	    {
              cairo_region_get_rectangle (region, i, &r);
	      g_string_append_printf (s, "[%dx%d @%d,%d]", r.width, r.height, r.x, r.y);
	      if (i != num -1)
		g_string_append (s, ", ");
	    }
	}
    }
  g_string_append (s, "}");
  return g_string_free (s, FALSE);
}
#endif

static GList *
list_insert_link_before (GList *list,
                         GList *sibling,
                         GList *link)
{
  if (list == NULL || sibling == list)
    {
      link->prev = NULL;
      link->next = list;
      if (list)
        list->prev = link;
      return link;
    }
  else if (sibling == NULL)
    {
      GList *last = g_list_last (list);

      last->next = link;
      link->prev = last;
      link->next = NULL;

      return list;
    }
  else
    {
      link->next = sibling;
      link->prev = sibling->prev;
      sibling->prev = link;

      if (link->prev)
        link->prev->next = link;

      return list;
    }
}

static void
gdk_window_init (GdkWindow *window)
{
  /* 0-initialization is good for all other fields. */

  window->window_type = GDK_WINDOW_CHILD;

  window->state = GDK_WINDOW_STATE_WITHDRAWN;
  window->fullscreen_mode = GDK_FULLSCREEN_ON_CURRENT_MONITOR;
  window->width = 1;
  window->height = 1;
  window->toplevel_window_type = -1;
  /* starts hidden */
  window->effective_visibility = GDK_VISIBILITY_NOT_VIEWABLE;
  window->visibility = GDK_VISIBILITY_FULLY_OBSCURED;
  /* Default to unobscured since some backends don't send visibility events */
  window->native_visibility = GDK_VISIBILITY_UNOBSCURED;
  window->children_list_node.data = window;

  window->device_cursor = g_hash_table_new_full (NULL, NULL,
                                                 NULL, g_object_unref);
}

static GQuark quark_pointer_window = 0;

static void
gdk_window_class_init (GdkWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_window_finalize;
  object_class->set_property = gdk_window_set_property;
  object_class->get_property = gdk_window_get_property;

  quark_pointer_window = g_quark_from_static_string ("gtk-pointer-window");


  /* Properties */

  /**
   * GdkWindow:cursor:
   *
   * The mouse pointer for a #GdkWindow. See gdk_window_set_cursor() and
   * gdk_window_get_cursor() for details.
   *
   * Since: 2.18
   */
  properties[PROP_CURSOR] =
      g_param_spec_object ("cursor",
                           P_("Cursor"),
                           P_("Cursor"),
                           GDK_TYPE_CURSOR,
                           G_PARAM_READWRITE);

  /**
   * GdkWindow:display:
   *
   * The #GdkDisplay connection of the window. See gdk_window_get_display()
   * for details.
   *
   * Since: 3.90
   */
  properties[PROP_DISPLAY] =
      g_param_spec_object ("display",
                           P_("Display"),
                           P_("Display"),
                           GDK_TYPE_DISPLAY,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  /**
   * GdkWindow::moved-to-rect:
   * @window: the #GdkWindow that moved
   * @flipped_rect: (nullable): the position of @window after any possible
   *                flipping or %NULL if the backend can't obtain it
   * @final_rect: (nullable): the final position of @window or %NULL if the
   *              backend can't obtain it
   * @flipped_x: %TRUE if the anchors were flipped horizontally
   * @flipped_y: %TRUE if the anchors were flipped vertically
   *
   * Emitted when the position of @window is finalized after being moved to a
   * destination rectangle.
   *
   * @window might be flipped over the destination rectangle in order to keep
   * it on-screen, in which case @flipped_x and @flipped_y will be set to %TRUE
   * accordingly.
   *
   * @flipped_rect is the ideal position of @window after any possible
   * flipping, but before any possible sliding. @final_rect is @flipped_rect,
   * but possibly translated in the case that flipping is still ineffective in
   * keeping @window on-screen.
   *
   * Since: 3.22
   * Stability: Private
   */
  signals[MOVED_TO_RECT] =
    g_signal_new (g_intern_static_string ("moved-to-rect"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL,
                  NULL,
                  _gdk_marshal_VOID__POINTER_POINTER_BOOLEAN_BOOLEAN,
                  G_TYPE_NONE,
                  4,
                  G_TYPE_POINTER,
                  G_TYPE_POINTER,
                  G_TYPE_BOOLEAN,
                  G_TYPE_BOOLEAN);
}

static void
seat_removed_cb (GdkDisplay *display,
                 GdkSeat    *seat,
                 GdkWindow  *window)
{
  GdkDevice *device = gdk_seat_get_pointer (seat);

  window->devices_inside = g_list_remove (window->devices_inside, device);
  g_hash_table_remove (window->device_cursor, device);

  if (window->device_events)
    g_hash_table_remove (window->device_events, device);
}

static void
gdk_window_finalize (GObject *object)
{
  GdkWindow *window = GDK_WINDOW (object);

  g_signal_handlers_disconnect_by_func (gdk_window_get_display (window),
                                        seat_removed_cb, window);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN)
	{
	  g_warning ("losing last reference to undestroyed window");
	  _gdk_window_destroy (window, FALSE);
	}
      else
	/* We use TRUE here, to keep us from actually calling
	 * XDestroyWindow() on the window
	 */
	_gdk_window_destroy (window, TRUE);
    }

  if (window->impl)
    {
      g_object_unref (window->impl);
      window->impl = NULL;
    }

  if (window->impl_window != window)
    {
      g_object_unref (window->impl_window);
      window->impl_window = NULL;
    }

  if (window->shape)
    cairo_region_destroy (window->shape);

  if (window->input_shape)
    cairo_region_destroy (window->input_shape);

  if (window->cursor)
    g_object_unref (window->cursor);

  if (window->device_cursor)
    g_hash_table_destroy (window->device_cursor);

  if (window->device_events)
    g_hash_table_destroy (window->device_events);

  if (window->source_event_masks)
    g_hash_table_destroy (window->source_event_masks);

  if (window->devices_inside)
    g_list_free (window->devices_inside);

  g_clear_object (&window->display);

  G_OBJECT_CLASS (gdk_window_parent_class)->finalize (object);
}

static void
gdk_window_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GdkWindow *window = GDK_WINDOW (object);

  switch (prop_id)
    {
    case PROP_CURSOR:
      gdk_window_set_cursor (window, g_value_get_object (value));
      break;

    case PROP_DISPLAY:
      window->display = g_value_dup_object (value);
      g_assert (window->display != NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_window_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GdkWindow *window = GDK_WINDOW (object);

  switch (prop_id)
    {
    case PROP_CURSOR:
      g_value_set_object (value, gdk_window_get_cursor (window));
      break;

    case PROP_DISPLAY:
      g_value_set_object (value, window->display);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
gdk_window_is_subsurface (GdkWindow *window)
{
   return window->window_type == GDK_WINDOW_SUBSURFACE;
}

static GdkWindow *
gdk_window_get_impl_window (GdkWindow *window)
{
  return window->impl_window;
}

GdkWindow *
_gdk_window_get_impl_window (GdkWindow *window)
{
  return gdk_window_get_impl_window (window);
}

static gboolean
gdk_window_has_impl (GdkWindow *window)
{
  return window->impl_window == window;
}

static gboolean
gdk_window_is_toplevel (GdkWindow *window)
{
  return
    window->parent == NULL ||
    window->parent->window_type == GDK_WINDOW_ROOT;
}

gboolean
_gdk_window_has_impl (GdkWindow *window)
{
  return gdk_window_has_impl (window);
}

static gboolean
gdk_window_has_no_impl (GdkWindow *window)
{
  return window->impl_window != window;
}

static void
remove_sibling_overlapped_area (GdkWindow *window,
				cairo_region_t *region)
{
  GdkWindow *parent;
  GdkWindow *sibling;
  cairo_region_t *child_region;
  GdkRectangle r;
  GList *l;

  parent = window->parent;

  if (gdk_window_is_toplevel (window))
    return;

  /* Convert from from window coords to parent coords */
  cairo_region_translate (region, window->x, window->y);

  for (l = parent->children; l; l = l->next)
    {
      sibling = l->data;

      if (sibling == window)
	break;

      if (!GDK_WINDOW_IS_MAPPED (sibling) || sibling->input_only)
	continue;

      r.x = sibling->x;
      r.y = sibling->y;
      r.width = sibling->width;
      r.height = sibling->height;

      child_region = cairo_region_create_rectangle (&r);

      if (sibling->shape)
	{
	  /* Adjust shape region to parent window coords */
	  cairo_region_translate (sibling->shape, sibling->x, sibling->y);
	  cairo_region_intersect (child_region, sibling->shape);
	  cairo_region_translate (sibling->shape, -sibling->x, -sibling->y);
	}

      cairo_region_subtract (region, child_region);
      cairo_region_destroy (child_region);
    }

  remove_sibling_overlapped_area (parent, region);

  /* Convert back to window coords */
  cairo_region_translate (region, -window->x, -window->y);
}

static void
remove_child_area (GdkWindow *window,
		   gboolean for_input,
		   cairo_region_t *region)
{
  GdkWindow *child;
  cairo_region_t *child_region;
  GdkRectangle r;
  GList *l;

  for (l = window->children; l; l = l->next)
    {
      child = l->data;

      /* If region is empty already, no need to do
	 anything potentially costly */
      if (cairo_region_is_empty (region))
	break;

      if (!GDK_WINDOW_IS_MAPPED (child) || child->input_only)
	continue;

      r.x = child->x;
      r.y = child->y;
      r.width = child->width;
      r.height = child->height;

      /* Bail early if child totally outside region */
      if (cairo_region_contains_rectangle (region, &r) == CAIRO_REGION_OVERLAP_OUT)
	continue;

      child_region = cairo_region_create_rectangle (&r);

      if (child->shape)
	{
	  /* Adjust shape region to parent window coords */
	  cairo_region_translate (child->shape, child->x, child->y);
	  cairo_region_intersect (child_region, child->shape);
	  cairo_region_translate (child->shape, -child->x, -child->y);
	}

      if (for_input)
	{
	  if (child->input_shape)
	    cairo_region_intersect (child_region, child->input_shape);
	}

      cairo_region_subtract (region, child_region);
      cairo_region_destroy (child_region);
    }
}

static GdkVisibilityState
effective_visibility (GdkWindow *window)
{
  GdkVisibilityState native;

  if (!gdk_window_is_viewable (window))
    return GDK_VISIBILITY_NOT_VIEWABLE;

  native = window->impl_window->native_visibility;

  if (native == GDK_VISIBILITY_FULLY_OBSCURED ||
      window->visibility == GDK_VISIBILITY_FULLY_OBSCURED)
    return GDK_VISIBILITY_FULLY_OBSCURED;
  else if (native == GDK_VISIBILITY_UNOBSCURED)
    return window->visibility;
  else /* native PARTIAL, private partial or unobscured  */
    return GDK_VISIBILITY_PARTIAL;
}

static void
gdk_window_update_visibility (GdkWindow *window)
{
  GdkVisibilityState new_visibility;
  GdkEvent *event;

  new_visibility = effective_visibility (window);

  if (new_visibility != window->effective_visibility)
    {
      window->effective_visibility = new_visibility;

      if (new_visibility != GDK_VISIBILITY_NOT_VIEWABLE &&
	  window->event_mask & GDK_VISIBILITY_NOTIFY_MASK)
	{
	  event = _gdk_make_event (window, GDK_VISIBILITY_NOTIFY,
				   NULL, FALSE);
	  event->visibility.state = new_visibility;
	}
    }
}

static void
gdk_window_update_visibility_recursively (GdkWindow *window,
					  GdkWindow *only_for_impl)
{
  GdkWindow *child;
  GList *l;

  gdk_window_update_visibility (window);
  for (l = window->children; l != NULL; l = l->next)
    {
      child = l->data;
      if ((only_for_impl == NULL) ||
	  (only_for_impl == child->impl_window))
	gdk_window_update_visibility_recursively (child, only_for_impl);
    }
}

static gboolean
should_apply_clip_as_shape (GdkWindow *window)
{
  return
    gdk_window_has_impl (window) &&
    /* Not for non-shaped toplevels */
    (window->shape != NULL || window->applied_shape) &&
    /* or for foreign windows */
    window->window_type != GDK_WINDOW_FOREIGN &&
    /* or for the root window */
    window->window_type != GDK_WINDOW_ROOT;
}

static void
apply_shape (GdkWindow *window,
	     cairo_region_t *region)
{
  GdkWindowImplClass *impl_class;

  /* We trash whether we applied a shape so that
     we can avoid unsetting it many times, which
     could happen in e.g. apply_clip_as_shape as
     windows get resized */
  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
  if (region)
    impl_class->shape_combine_region (window,
				      region, 0, 0);
  else if (window->applied_shape)
    impl_class->shape_combine_region (window,
				      NULL, 0, 0);

  window->applied_shape = region != NULL;
}

static gboolean
region_rect_equal (const cairo_region_t *region,
                   const GdkRectangle *rect)
{
    GdkRectangle extents;

    if (cairo_region_num_rectangles (region) != 1)
        return FALSE;

    cairo_region_get_extents (region, &extents);

    return extents.x == rect->x &&
        extents.y == rect->y &&
        extents.width == rect->width &&
        extents.height == rect->height;
}

static void
apply_clip_as_shape (GdkWindow *window)
{
  GdkRectangle r;
  cairo_region_t *region;

  r.x = r.y = 0;
  r.width = window->width;
  r.height = window->height;

  region = cairo_region_copy (window->clip_region);
  remove_sibling_overlapped_area (window, region);

  /* We only apply the clip region if would differ
     from the actual clip region implied by the size
     of the window. This is to avoid unneccessarily
     adding meaningless shapes to all native subwindows */
  if (!region_rect_equal (region, &r))
    apply_shape (window, region);
  else
    apply_shape (window, NULL);

  cairo_region_destroy (region);
}

static void
recompute_visible_regions_internal (GdkWindow *private,
				    gboolean   recalculate_clip,
				    gboolean   recalculate_children)
{
  GdkRectangle r;
  GList *l;
  GdkWindow *child;
  cairo_region_t *new_clip;
  gboolean clip_region_changed;
  gboolean abs_pos_changed;
  int old_abs_x, old_abs_y;

  old_abs_x = private->abs_x;
  old_abs_y = private->abs_y;

  /* Update absolute position */
  if ((gdk_window_has_impl (private) &&
       private->window_type != GDK_WINDOW_SUBSURFACE) ||
      (gdk_window_is_toplevel (private) &&
       private->window_type == GDK_WINDOW_SUBSURFACE))
    {
      /* Native windows and toplevel subsurfaces start here */
      private->abs_x = 0;
      private->abs_y = 0;
    }
  else
    {
      private->abs_x = private->parent->abs_x + private->x;
      private->abs_y = private->parent->abs_y + private->y;
    }

  abs_pos_changed =
    private->abs_x != old_abs_x ||
    private->abs_y != old_abs_y;

  /* Update clip region based on:
   * parent clip
   * window size/position
   */
  clip_region_changed = FALSE;
  if (recalculate_clip)
    {
      if (private->viewable)
	{
	  /* Calculate visible region (sans children) in parent window coords */
	  r.x = private->x;
	  r.y = private->y;
	  r.width = private->width;
	  r.height = private->height;
	  new_clip = cairo_region_create_rectangle (&r);

	  if (!gdk_window_is_toplevel (private))
	    cairo_region_intersect (new_clip, private->parent->clip_region);

	  /* Convert from parent coords to window coords */
	  cairo_region_translate (new_clip, -private->x, -private->y);

	  if (should_apply_clip_as_shape (private) && private->shape)
	    cairo_region_intersect (new_clip, private->shape);
	}
      else
	  new_clip = cairo_region_create ();

      if (private->clip_region == NULL ||
	  !cairo_region_equal (private->clip_region, new_clip))
	clip_region_changed = TRUE;

      if (private->clip_region)
	cairo_region_destroy (private->clip_region);
      private->clip_region = new_clip;
    }

  if (clip_region_changed)
    {
      GdkVisibilityState visibility;
      gboolean fully_visible;

      if (cairo_region_is_empty (private->clip_region))
	visibility = GDK_VISIBILITY_FULLY_OBSCURED;
      else
        {
          if (private->shape)
            {
	      fully_visible = cairo_region_equal (private->clip_region,
	                                        private->shape);
            }
          else
            {
	      r.x = 0;
	      r.y = 0;
	      r.width = private->width;
	      r.height = private->height;
	      fully_visible = region_rect_equal (private->clip_region, &r);
	    }

	  if (fully_visible)
	    visibility = GDK_VISIBILITY_UNOBSCURED;
	  else
	    visibility = GDK_VISIBILITY_PARTIAL;
	}

      if (private->visibility != visibility)
	{
	  private->visibility = visibility;
	  gdk_window_update_visibility (private);
	}
    }

  /* Update all children, recursively (except for root, where children are not exact). */
  if ((abs_pos_changed || clip_region_changed || recalculate_children) &&
      private->window_type != GDK_WINDOW_ROOT)
    {
      for (l = private->children; l; l = l->next)
	{
	  child = l->data;
	  /* Only recalculate clip if the the clip region changed, otherwise
	   * there is no way the child clip region could change (its has not e.g. moved)
	   * Except if recalculate_children is set to force child updates
	   */
	  recompute_visible_regions_internal (child,
					      recalculate_clip && (clip_region_changed || recalculate_children),
					      FALSE);
	}
    }
}

/* Call this when private has changed in one or more of these ways:
 *  size changed
 *  window moved
 *  new window added
 *  stacking order of window changed
 *  child deleted
 *
 * It will recalculate abs_x/y and the clip regions
 *
 * Unless the window didn’t change stacking order or size/pos, pass in TRUE
 * for recalculate_siblings. (Mostly used internally for the recursion)
 *
 * If a child window was removed (and you can’t use that child for
 * recompute_visible_regions), pass in TRUE for recalculate_children on the parent
 */
static void
recompute_visible_regions (GdkWindow *private,
			   gboolean recalculate_children)
{
  GdkWindow *toplevel;

  toplevel = gdk_window_get_toplevel (private);
  toplevel->geometry_dirty = TRUE;

  recompute_visible_regions_internal (private,
				      TRUE,
				      recalculate_children);
}

static void
gdk_window_clear_old_updated_area (GdkWindow *window)
{
  int i;

  for (i = 0; i < 2; i++)
    {
      if (window->old_updated_area[i])
        {
          cairo_region_destroy (window->old_updated_area[i]);
          window->old_updated_area[i] = NULL;
        }
    }
}

static void
gdk_window_append_old_updated_area (GdkWindow *window,
                                    cairo_region_t *region)
{
  if (window->old_updated_area[1])
    cairo_region_destroy (window->old_updated_area[1]);
  window->old_updated_area[1] = window->old_updated_area[0];
  window->old_updated_area[0] = cairo_region_reference (region);
}

void
_gdk_window_update_size (GdkWindow *window)
{
  gdk_window_clear_old_updated_area (window);
  recompute_visible_regions (window, FALSE);
}

static GdkEventMask
get_native_device_event_mask (GdkWindow *private,
                              GdkDevice *device)
{
  GdkEventMask event_mask;

  if (device)
    event_mask = GPOINTER_TO_INT (g_hash_table_lookup (private->device_events, device));
  else
    event_mask = private->event_mask;

  if (private->window_type == GDK_WINDOW_ROOT ||
      private->window_type == GDK_WINDOW_FOREIGN)
    return event_mask;
  else
    {
      GdkEventMask mask;

      /* Do whatever the app asks to, since the app
       * may be asking for weird things for native windows,
       * but don't use motion hints as that may affect non-native
       * child windows that don't want it. Also, we need to
       * set all the app-specified masks since they will be picked
       * up by any implicit grabs (i.e. if they were not set as
       * native we would not get the events we need). */
      mask = private->event_mask & ~GDK_POINTER_MOTION_HINT_MASK;

      /* We need thse for all native windows so we can
	 emulate events on children: */
      mask |=
	GDK_EXPOSURE_MASK |
	GDK_VISIBILITY_NOTIFY_MASK |
	GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
        GDK_TOUCH_MASK |
        GDK_POINTER_MOTION_MASK |
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
        GDK_SCROLL_MASK;

      return mask;
    }
}

static GdkEventMask
get_native_event_mask (GdkWindow *private)
{
  return get_native_device_event_mask (private, NULL);
}

GdkWindow*
gdk_window_new (GdkWindow     *parent,
		GdkWindowAttr *attributes)
{
  GdkWindow *window;
  GdkScreen *screen;
  GdkDisplay *display;
  gboolean native;
  GdkEventMask event_mask;

  g_return_val_if_fail (attributes != NULL, NULL);

  if (!parent)
    {
      screen = gdk_screen_get_default ();
      parent = gdk_screen_get_root_window (screen);
    }
  else
    screen = gdk_window_get_screen (parent);

  g_return_val_if_fail (GDK_IS_WINDOW (parent), NULL);

  if (GDK_WINDOW_DESTROYED (parent))
    {
      g_warning ("gdk_window_new(): parent is destroyed");
      return NULL;
    }

  display = gdk_screen_get_display (screen);

  window = _gdk_display_create_window (display);

  window->parent = parent;

  window->accept_focus = TRUE;
  window->focus_on_map = TRUE;
  window->event_compression = TRUE;

  window->x = attributes->x;
  window->y = attributes->y;
  window->width = (attributes->width > 1) ? (attributes->width) : (1);
  window->height = (attributes->height > 1) ? (attributes->height) : (1);
  window->alpha = 255;

  if (attributes->wclass == GDK_INPUT_ONLY)
    {
      /* Backwards compatiblity - we've always ignored
       * attributes->window_type for input-only windows
       * before
       */
      if (GDK_WINDOW_TYPE (parent) == GDK_WINDOW_ROOT)
	window->window_type = GDK_WINDOW_TEMP;
      else
	window->window_type = GDK_WINDOW_CHILD;
    }
  else
    window->window_type = attributes->window_type;

  /* Sanity checks */
  switch (window->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_TEMP:
      if (GDK_WINDOW_TYPE (parent) != GDK_WINDOW_ROOT)
	g_warning (G_STRLOC "Toplevel windows must be created as children of\n"
		   "a window of type GDK_WINDOW_ROOT");
      break;
    case GDK_WINDOW_SUBSURFACE:
#ifdef GDK_WINDOWING_WAYLAND
      if (!GDK_IS_WAYLAND_DISPLAY (display))
        {
          g_warning (G_STRLOC "Subsurface windows can only be used on Wayland");
          return NULL;
        }
#endif
      break;
    case GDK_WINDOW_CHILD:
      if (GDK_WINDOW_TYPE (parent) == GDK_WINDOW_ROOT ||
          GDK_WINDOW_TYPE (parent) == GDK_WINDOW_FOREIGN)
        {
          g_warning (G_STRLOC "Child windows must not be created as children of\n"
                     "a window of type GDK_WINDOW_ROOT or GDK_WINDOW_FOREIGN");
          return NULL;
        }
      break;
    default:
      g_warning (G_STRLOC "cannot make windows of type %d", window->window_type);
      return NULL;
    }

  window->event_mask = attributes->event_mask;

  if (attributes->wclass == GDK_INPUT_OUTPUT)
    {
      window->input_only = FALSE;
    }
  else
    {
      window->input_only = TRUE;
    }

  window->parent->children = g_list_concat (&window->children_list_node, window->parent->children);

  if (window->parent->window_type == GDK_WINDOW_ROOT)
    {
      GdkFrameClock *frame_clock = g_object_new (GDK_TYPE_FRAME_CLOCK_IDLE, NULL);
      gdk_window_set_frame_clock (window, frame_clock);
      g_object_unref (frame_clock);
    }

  native = FALSE;
  if (window->parent->window_type == GDK_WINDOW_ROOT)
    native = TRUE; /* Always use native windows for toplevels */

#ifdef GDK_WINDOWING_WAYLAND
  if (window->window_type == GDK_WINDOW_SUBSURFACE)
    native = TRUE; /* Always use native windows for subsurfaces as well */
#endif

  if (native)
    {
      event_mask = get_native_event_mask (window);

      /* Create the impl */
      _gdk_display_create_window_impl (display, window, parent, screen, event_mask, attributes);
      window->impl_window = window;
    }
  else
    {
      window->impl_window = g_object_ref (window->parent->impl_window);
      window->impl = g_object_ref (window->impl_window->impl);
    }

  recompute_visible_regions (window, FALSE);

  g_signal_connect (gdk_window_get_display (parent), "seat-removed",
                    G_CALLBACK (seat_removed_cb), window);

  if ((_gdk_gl_flags & (GDK_GL_ALWAYS | GDK_GL_DISABLE)) == GDK_GL_ALWAYS)
    {
      GError *error = NULL;

      if (gdk_window_get_paint_gl_context (window, &error) == NULL)
        {
          g_warning ("Unable to force GL enabled: %s", error->message);
          g_error_free (error);
        }
    }

  return window;
}

/**
 * gdk_window_new_toplevel: (constructor)
 * @display: the display to create the window on
 * @event_mask: event mask (see gdk_window_set_events())
 * @width: width of new window
 * @height: height of new window
 *
 * Creates a new toplevel window. The window will be managed by the window
 * manager.
 *
 * Returns: (transfer full): the new #GdkWindow
 *
 * Since: 3.90
 **/
GdkWindow *
gdk_window_new_toplevel (GdkDisplay *display,
                         gint        event_mask,
                         gint        width,
                         gint        height)
{
  GdkWindowAttr attr;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  attr.event_mask = event_mask;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.x = 0;
  attr.y = 0;
  attr.width = width;
  attr.height = height;
  attr.window_type = GDK_WINDOW_TOPLEVEL;

  return gdk_window_new (gdk_screen_get_root_window (gdk_display_get_default_screen (display)),
                         &attr);
}

/**
 * gdk_window_new_popup: (constructor)
 * @display: the display to create the window on
 * @event_mask: event mask (see gdk_window_set_events())
 * @position: position of the window on screen
 *
 * Creates a new toplevel popup window. The window will bypass window
 * management.
 *
 * Returns: (transfer full): the new #GdkWindow
 *
 * Since: 3.90
 **/
GdkWindow *
gdk_window_new_popup (GdkDisplay         *display,
                      gint                event_mask,
                      const GdkRectangle *position)
{
  GdkWindowAttr attr;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (position != NULL, NULL);

  attr.event_mask = event_mask;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.x = position->x;
  attr.y = position->y;
  attr.width = position->width;
  attr.height = position->height;
  attr.window_type = GDK_WINDOW_TEMP;

  return gdk_window_new (gdk_screen_get_root_window (gdk_display_get_default_screen (display)),
                         &attr);
}

/**
 * gdk_window_new_temp: (constructor)
 * @display: the display to create the window on
 *
 * Creates a new toplevel temporary window. The window will be
 * situated off-screen and not handle output.
 *
 * You most likely do not want to use this function.
 *
 * Returns: (transfer full): the new #GdkWindow
 *
 * Since: 3.90
 **/
GdkWindow *
gdk_window_new_temp (GdkDisplay *display)
{
  GdkWindowAttr attr;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  attr.event_mask = 0;
  attr.wclass = GDK_INPUT_ONLY;
  attr.x = -100;
  attr.y = -100;
  attr.width = 10;
  attr.height = 10;
  attr.window_type = GDK_WINDOW_TEMP;

  return gdk_window_new (gdk_screen_get_root_window (gdk_display_get_default_screen (display)),
                         &attr);
}

/**
 * gdk_window_new_child: (constructor)
 * @parent: the parent window
 * @event_mask: event mask (see gdk_window_set_events())
 * @position: placement of the window inside @parent
 *
 * Creates a new client-side child window.
 *
 * Returns: (transfer full): the new #GdkWindow
 *
 * Since: 3.90
 **/
GdkWindow *
gdk_window_new_child (GdkWindow          *parent,
                      gint                event_mask,
                      const GdkRectangle *position)
{
  GdkWindowAttr attr;

  g_return_val_if_fail (GDK_IS_WINDOW (parent), NULL);

  attr.event_mask = event_mask;
  attr.wclass = GDK_INPUT_OUTPUT;
  attr.x = position->x;
  attr.y = position->y;
  attr.width = position->width;
  attr.height = position->height;
  attr.window_type = GDK_WINDOW_CHILD;

  return gdk_window_new (parent, &attr);
}

/**
 * gdk_window_new_input: (constructor)
 * @parent: the parent window
 * @event_mask: event mask (see gdk_window_set_events())
 * @position: placement of the window inside @parent
 *
 * Creates a new client-side input-only window.
 *
 * Returns: (transfer full): the new #GdkWindow
 *
 * Since: 3.90
 **/
GdkWindow *
gdk_window_new_input (GdkWindow          *parent,
                      gint                event_mask,
                      const GdkRectangle *position)
{
  GdkWindowAttr attr;

  g_return_val_if_fail (GDK_IS_WINDOW (parent), NULL);

  attr.event_mask = event_mask;
  attr.wclass = GDK_INPUT_ONLY;
  attr.x = position->x;
  attr.y = position->y;
  attr.width = position->width;
  attr.height = position->height;
  attr.window_type = GDK_WINDOW_CHILD;

  return gdk_window_new (parent, &attr);
}

/**
 * _gdk_event_filter_unref:
 * @window: (allow-none): A #GdkWindow, or %NULL to be the global window
 * @filter: A window filter
 *
 * Release a reference to @filter.  Note this function may
 * mutate the list storage, so you need to handle this
 * if iterating over a list of filters.
 */
void
_gdk_event_filter_unref (GdkWindow       *window,
			 GdkEventFilter  *filter)
{
  GList **filters;
  GList *tmp_list;

  if (window == NULL)
    filters = &_gdk_default_filters;
  else
    filters = &window->filters;

  tmp_list = *filters;
  while (tmp_list)
    {
      GdkEventFilter *iter_filter = tmp_list->data;
      GList *node;

      node = tmp_list;
      tmp_list = tmp_list->next;

      if (iter_filter != filter)
	continue;

      g_assert (iter_filter->ref_count > 0);

      filter->ref_count--;
      if (filter->ref_count != 0)
	continue;

      *filters = g_list_remove_link (*filters, node);
      g_free (filter);
      g_list_free_1 (node);
    }
}

static void
window_remove_filters (GdkWindow *window)
{
  while (window->filters)
    _gdk_event_filter_unref (window, window->filters->data);
}

static void
update_pointer_info_foreach (GdkDisplay           *display,
                             GdkDevice            *device,
                             GdkPointerWindowInfo *pointer_info,
                             gpointer              user_data)
{
  GdkWindow *window = user_data;

  if (pointer_info->toplevel_under_pointer == window)
    {
      g_object_unref (pointer_info->toplevel_under_pointer);
      pointer_info->toplevel_under_pointer = NULL;
    }
}

static void
window_remove_from_pointer_info (GdkWindow  *window,
                                 GdkDisplay *display)
{
  _gdk_display_pointer_info_foreach (display,
                                     update_pointer_info_foreach,
                                     window);
}

static void
gdk_window_free_current_paint (GdkWindow *window)
{
  cairo_surface_destroy (window->current_paint.surface);
  window->current_paint.surface = NULL;

  cairo_region_destroy (window->current_paint.region);
  window->current_paint.region = NULL;

  window->current_paint.surface_needs_composite = FALSE;
}

/**
 * _gdk_window_destroy_hierarchy:
 * @window: a #GdkWindow
 * @recursing: If %TRUE, then this is being called because a parent
 *            was destroyed.
 * @recursing_native: If %TRUE, then this is being called because a native parent
 *            was destroyed. This generally means that the call to the
 *            windowing system to destroy the window can be omitted, since
 *            it will be destroyed as a result of the parent being destroyed.
 *            Unless @foreign_destroy.
 * @foreign_destroy: If %TRUE, the window or a parent was destroyed by some
 *            external agency. The window has already been destroyed and no
 *            windowing system calls should be made. (This may never happen
 *            for some windowing systems.)
 *
 * Internal function to destroy a window. Like gdk_window_destroy(),
 * but does not drop the reference count created by gdk_window_new().
 **/
static void
_gdk_window_destroy_hierarchy (GdkWindow *window,
			       gboolean   recursing,
			       gboolean   recursing_native,
			       gboolean   foreign_destroy)
{
  GdkWindowImplClass *impl_class;
  GdkWindow *temp_window;
  GdkScreen *screen;
  GdkDisplay *display;
  GList *tmp;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  display = gdk_window_get_display (window);
  screen = gdk_window_get_screen (window);
  temp_window = g_object_get_qdata (G_OBJECT (screen), quark_pointer_window);
  if (temp_window == window)
    g_object_set_qdata (G_OBJECT (screen), quark_pointer_window, NULL);

  switch (window->window_type)
    {
    case GDK_WINDOW_ROOT:
      if (!screen->closed)
	{
	  g_error ("attempted to destroy root window");
	  break;
	}
      /* else fall thru */
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_CHILD:
    case GDK_WINDOW_TEMP:
    case GDK_WINDOW_FOREIGN:
    case GDK_WINDOW_SUBSURFACE:
      if (window->window_type == GDK_WINDOW_FOREIGN && !foreign_destroy)
	{
	  /* For historical reasons, we remove any filters
	   * on a foreign window when it or a parent is destroyed;
	   * this likely causes problems if two separate portions
	   * of code are maintaining filter lists on a foreign window.
	   */
	  window_remove_filters (window);
	}
      else
	{
	  if (window->parent)
	    {
	      if (window->parent->children)
                window->parent->children = g_list_remove_link (window->parent->children, &window->children_list_node);

	      if (!recursing &&
		  GDK_WINDOW_IS_MAPPED (window))
		{
		  recompute_visible_regions (window, FALSE);
		  gdk_window_invalidate_in_parent (window);
		}
	    }

          if (window->gl_paint_context)
            {
              /* Make sure to destroy if current */
              g_object_run_dispose (G_OBJECT (window->gl_paint_context));
              g_object_unref (window->gl_paint_context);
              window->gl_paint_context = NULL;
            }

          if (window->frame_clock)
            {
              g_object_run_dispose (G_OBJECT (window->frame_clock));
              gdk_window_set_frame_clock (window, NULL);
            }

          gdk_window_free_current_paint (window);

	  if (window->window_type == GDK_WINDOW_FOREIGN)
	    g_assert (window->children == NULL);
	  else
	    {
	      tmp = window->children;
	      window->children = NULL;
	      /* No need to free children list, its all made up of in-struct nodes */

	      while (tmp)
		{
		  temp_window = tmp->data;
		  tmp = tmp->next;

		  if (temp_window)
		    _gdk_window_destroy_hierarchy (temp_window,
						   TRUE,
						   recursing_native || gdk_window_has_impl (window),
						   foreign_destroy);
		}
	    }

	  _gdk_window_clear_update_area (window);

	  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

	  if (gdk_window_has_impl (window))
	    impl_class->destroy (window, recursing_native, foreign_destroy);
	  else
	    {
	      /* hide to make sure we repaint and break grabs */
	      gdk_window_hide (window);
	    }

	  window->state |= GDK_WINDOW_STATE_WITHDRAWN;
	  window->parent = NULL;
	  window->destroyed = TRUE;

	  window_remove_filters (window);

	  window_remove_from_pointer_info (window, display);

	  if (window->clip_region)
	    {
	      cairo_region_destroy (window->clip_region);
	      window->clip_region = NULL;
	    }
	}
      break;
    }
}

/**
 * _gdk_window_destroy:
 * @window: a #GdkWindow
 * @foreign_destroy: If %TRUE, the window or a parent was destroyed by some
 *            external agency. The window has already been destroyed and no
 *            windowing system calls should be made. (This may never happen
 *            for some windowing systems.)
 *
 * Internal function to destroy a window. Like gdk_window_destroy(),
 * but does not drop the reference count created by gdk_window_new().
 **/
void
_gdk_window_destroy (GdkWindow *window,
		     gboolean   foreign_destroy)
{
  _gdk_window_destroy_hierarchy (window, FALSE, FALSE, foreign_destroy);
}

/**
 * gdk_window_destroy:
 * @window: a #GdkWindow
 *
 * Destroys the window system resources associated with @window and decrements @window's
 * reference count. The window system resources for all children of @window are also
 * destroyed, but the children’s reference counts are not decremented.
 *
 * Note that a window will not be destroyed automatically when its reference count
 * reaches zero. You must call this function yourself before that happens.
 *
 **/
void
gdk_window_destroy (GdkWindow *window)
{
  _gdk_window_destroy_hierarchy (window, FALSE, FALSE, FALSE);
  g_object_unref (window);
}

/**
 * gdk_window_set_user_data:
 * @window: a #GdkWindow
 * @user_data: (allow-none) (type GObject.Object): user data
 *
 * For most purposes this function is deprecated in favor of
 * g_object_set_data(). However, for historical reasons GTK+ stores
 * the #GtkWidget that owns a #GdkWindow as user data on the
 * #GdkWindow. So, custom widget implementations should use
 * this function for that. If GTK+ receives an event for a #GdkWindow,
 * and the user data for the window is non-%NULL, GTK+ will assume the
 * user data is a #GtkWidget, and forward the event to that widget.
 *
 **/
void
gdk_window_set_user_data (GdkWindow *window,
			  gpointer   user_data)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  window->user_data = user_data;
}

/**
 * gdk_window_get_user_data:
 * @window: a #GdkWindow
 * @data: (out): return location for user data
 *
 * Retrieves the user data for @window, which is normally the widget
 * that @window belongs to. See gdk_window_set_user_data().
 *
 **/
void
gdk_window_get_user_data (GdkWindow *window,
			  gpointer  *data)
{
  *data = window->user_data;
}

/**
 * gdk_window_get_window_type:
 * @window: a #GdkWindow
 *
 * Gets the type of the window. See #GdkWindowType.
 *
 * Returns: type of window
 **/
GdkWindowType
gdk_window_get_window_type (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), (GdkWindowType) -1);

  return GDK_WINDOW_TYPE (window);
}

/**
 * gdk_window_get_screen:
 * @window: a #GdkWindow
 * 
 * Gets the #GdkScreen associated with a #GdkWindow.
 * 
 * Returns: (transfer none): the #GdkScreen associated with @window
 *
 * Since: 2.24
 **/
GdkScreen*
gdk_window_get_screen (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  return gdk_display_get_default_screen (window->display);
}

/**
 * gdk_window_get_display:
 * @window: a #GdkWindow
 * 
 * Gets the #GdkDisplay associated with a #GdkWindow.
 * 
 * Returns: (transfer none): the #GdkDisplay associated with @window
 *
 * Since: 2.24
 **/
GdkDisplay *
gdk_window_get_display (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  return window->display;
}
/**
 * gdk_window_is_destroyed:
 * @window: a #GdkWindow
 *
 * Check to see if a window is destroyed..
 *
 * Returns: %TRUE if the window is destroyed
 *
 * Since: 2.18
 **/
gboolean
gdk_window_is_destroyed (GdkWindow *window)
{
  return GDK_WINDOW_DESTROYED (window);
}

/**
 * gdk_window_has_native:
 * @window: a #GdkWindow
 *
 * Checks whether the window has a native window or not.
 *
 * Returns: %TRUE if the @window has a native window, %FALSE otherwise.
 *
 * Since: 2.22
 */
gboolean
gdk_window_has_native (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  return window->parent == NULL || window->parent->impl != window->impl;
}

/**
 * gdk_window_get_position:
 * @window: a #GdkWindow
 * @x: (out) (allow-none): X coordinate of window
 * @y: (out) (allow-none): Y coordinate of window
 *
 * Obtains the position of the window as reported in the
 * most-recently-processed #GdkEventConfigure. Contrast with
 * gdk_window_get_geometry() which queries the X server for the
 * current window position, regardless of which events have been
 * received or processed.
 *
 * The position coordinates are relative to the window’s parent window.
 *
 **/
void
gdk_window_get_position (GdkWindow *window,
			 gint      *x,
			 gint      *y)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (x)
    *x = window->x;
  if (y)
    *y = window->y;
}

/**
 * gdk_window_get_parent:
 * @window: a #GdkWindow
 *
 * Obtains the parent of @window, as known to GDK. Does not query the
 * X server; thus this returns the parent as passed to gdk_window_new(),
 * not the actual parent. This should never matter unless you’re using
 * Xlib calls mixed with GDK calls on the X11 platform. It may also
 * matter for toplevel windows, because the window manager may choose
 * to reparent them.
 *
 * Returns: (transfer none): parent of @window
 **/
GdkWindow*
gdk_window_get_parent (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (gdk_window_is_subsurface (window))
    return window->transient_for;
  else
    return window->parent;
}

/**
 * gdk_window_get_toplevel:
 * @window: a #GdkWindow
 *
 * Gets the toplevel window that’s an ancestor of @window.
 *
 * Any window type but %GDK_WINDOW_CHILD is considered a
 * toplevel window, as is a %GDK_WINDOW_CHILD window that
 * has a root window as parent.
 *
 * Returns: (transfer none): the toplevel window containing @window
 **/
GdkWindow *
gdk_window_get_toplevel (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  while (window->window_type == GDK_WINDOW_CHILD ||
         window->window_type == GDK_WINDOW_SUBSURFACE)
    {
      if (gdk_window_is_toplevel (window))
	break;
      window = window->parent;
    }

  return window;
}

/**
 * gdk_window_get_children:
 * @window: a #GdkWindow
 *
 * Gets the list of children of @window known to GDK.
 * This function only returns children created via GDK,
 * so for example it’s useless when used with the root window;
 * it only returns windows an application created itself.
 *
 * The returned list must be freed, but the elements in the
 * list need not be.
 *
 * Returns: (transfer container) (element-type GdkWindow):
 *     list of child windows inside @window
 **/
GList*
gdk_window_get_children (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  return g_list_copy (window->children);
}

/**
 * gdk_window_peek_children:
 * @window: a #GdkWindow
 *
 * Like gdk_window_get_children(), but does not copy the list of
 * children, so the list does not need to be freed.
 *
 * Returns: (transfer none) (element-type GdkWindow):
 *     a reference to the list of child windows in @window
 **/
GList *
gdk_window_peek_children (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  return window->children;
}


/**
 * gdk_window_get_children_with_user_data:
 * @window: a #GdkWindow
 * @user_data: user data to look for
 *
 * Gets the list of children of @window known to GDK with a
 * particular @user_data set on it.
 *
 * The returned list must be freed, but the elements in the
 * list need not be.
 *
 * The list is returned in (relative) stacking order, i.e. the
 * lowest window is first.
 *
 * Returns: (transfer container) (element-type GdkWindow):
 *     list of child windows inside @window
 *
 * Since: 3.10
 **/
GList *
gdk_window_get_children_with_user_data (GdkWindow *window,
                                        gpointer   user_data)
{
  GdkWindow *child;
  GList *res, *l;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  res = NULL;
  for (l = window->children; l != NULL; l = l->next)
    {
      child = l->data;

      if (child->user_data == user_data)
	res = g_list_prepend (res, child);
    }

  return res;
}


/**
 * gdk_window_add_filter: (skip)
 * @window: (allow-none): a #GdkWindow
 * @function: filter callback
 * @data: data to pass to filter callback
 *
 * Adds an event filter to @window, allowing you to intercept events
 * before they reach GDK. This is a low-level operation and makes it
 * easy to break GDK and/or GTK+, so you have to know what you're
 * doing. Pass %NULL for @window to get all events for all windows,
 * instead of events for a specific window.
 *
 * If you are interested in X GenericEvents, bear in mind that
 * XGetEventData() has been already called on the event, and
 * XFreeEventData() must not be called within @function.
 **/
void
gdk_window_add_filter (GdkWindow     *window,
		       GdkFilterFunc  function,
		       gpointer       data)
{
  GList *tmp_list;
  GdkEventFilter *filter;

  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));

  if (window && GDK_WINDOW_DESTROYED (window))
    return;

  /* Filters are for the native events on the native window, so
     ensure there is a native window. */
  if (window && !gdk_window_has_impl (window))
    {
      g_warning ("Filters only work on toplevel windows");
      return;
    }

  if (window)
    tmp_list = window->filters;
  else
    tmp_list = _gdk_default_filters;

  while (tmp_list)
    {
      filter = (GdkEventFilter *)tmp_list->data;
      if ((filter->function == function) && (filter->data == data))
        {
          filter->ref_count++;
          return;
        }
      tmp_list = tmp_list->next;
    }

  filter = g_new (GdkEventFilter, 1);
  filter->function = function;
  filter->data = data;
  filter->ref_count = 1;
  filter->flags = 0;

  if (window)
    window->filters = g_list_append (window->filters, filter);
  else
    _gdk_default_filters = g_list_append (_gdk_default_filters, filter);
}

/**
 * gdk_window_remove_filter: (skip)
 * @window: a #GdkWindow
 * @function: previously-added filter function
 * @data: user data for previously-added filter function
 *
 * Remove a filter previously added with gdk_window_add_filter().
 */
void
gdk_window_remove_filter (GdkWindow     *window,
                          GdkFilterFunc  function,
                          gpointer       data)
{
  GList *tmp_list;
  GdkEventFilter *filter;

  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));

  if (window)
    tmp_list = window->filters;
  else
    tmp_list = _gdk_default_filters;

  while (tmp_list)
    {
      filter = (GdkEventFilter *)tmp_list->data;
      tmp_list = tmp_list->next;

      if ((filter->function == function) && (filter->data == data))
        {
          filter->flags |= GDK_EVENT_FILTER_REMOVED;

          _gdk_event_filter_unref (window, filter);

          return;
        }
    }
}

/**
 * gdk_screen_get_toplevel_windows:
 * @screen: The #GdkScreen where the toplevels are located.
 *
 * Obtains a list of all toplevel windows known to GDK on the screen @screen.
 * A toplevel window is a child of the root window (see
 * gdk_get_default_root_window()).
 *
 * The returned list should be freed with g_list_free(), but
 * its elements need not be freed.
 *
 * Returns: (transfer container) (element-type GdkWindow):
 *     list of toplevel windows, free with g_list_free()
 *
 * Since: 2.2
 **/
GList *
gdk_screen_get_toplevel_windows (GdkScreen *screen)
{
  GdkWindow * root_window;
  GList *new_list = NULL;
  GList *tmp_list;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  root_window = gdk_screen_get_root_window (screen);

  tmp_list = root_window->children;
  while (tmp_list)
    {
      GdkWindow *w = tmp_list->data;

      if (w->window_type != GDK_WINDOW_FOREIGN)
	new_list = g_list_prepend (new_list, w);
      tmp_list = tmp_list->next;
    }

  return new_list;
}

/**
 * gdk_window_is_visible:
 * @window: a #GdkWindow
 *
 * Checks whether the window has been mapped (with gdk_window_show() or
 * gdk_window_show_unraised()).
 *
 * Returns: %TRUE if the window is mapped
 **/
gboolean
gdk_window_is_visible (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  return GDK_WINDOW_IS_MAPPED (window);
}

/**
 * gdk_window_is_viewable:
 * @window: a #GdkWindow
 *
 * Check if the window and all ancestors of the window are
 * mapped. (This is not necessarily "viewable" in the X sense, since
 * we only check as far as we have GDK window parents, not to the root
 * window.)
 *
 * Returns: %TRUE if the window is viewable
 **/
gboolean
gdk_window_is_viewable (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (window->destroyed)
    return FALSE;

  return window->viewable;
}

/**
 * gdk_window_get_state:
 * @window: a #GdkWindow
 *
 * Gets the bitwise OR of the currently active window state flags,
 * from the #GdkWindowState enumeration.
 *
 * Returns: window state bitfield
 **/
GdkWindowState
gdk_window_get_state (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  return window->state;
}

static cairo_content_t
gdk_window_get_content (GdkWindow *window)
{
  cairo_surface_t *surface;
  cairo_content_t content;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  surface = gdk_window_ref_impl_surface (window);
  content = cairo_surface_get_content (surface);
  cairo_surface_destroy (surface);

  return content;
}

static cairo_surface_t *
gdk_window_ref_impl_surface (GdkWindow *window)
{
  return GDK_WINDOW_IMPL_GET_CLASS (window->impl)->ref_cairo_surface (gdk_window_get_impl_window (window));
}

GdkGLContext *
gdk_window_get_paint_gl_context (GdkWindow  *window,
                                 GError    **error)
{
  GError *internal_error = NULL;

  if (_gdk_gl_flags & GDK_GL_DISABLE)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("GL support disabled via GDK_DEBUG"));
      return NULL;
    }

  if (window->impl_window->gl_paint_context == NULL)
    {
      GdkWindowImplClass *impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

      if (impl_class->create_gl_context == NULL)
        {
          g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                               _("The current backend does not support OpenGL"));
          return NULL;
        }

      window->impl_window->gl_paint_context =
        impl_class->create_gl_context (window->impl_window,
                                       TRUE,
                                       NULL,
                                       &internal_error);
    }

  if (internal_error != NULL)
    {
      g_propagate_error (error, internal_error);
      g_clear_object (&(window->impl_window->gl_paint_context));
      return NULL;
    }

  gdk_gl_context_realize (window->impl_window->gl_paint_context, &internal_error);
  if (internal_error != NULL)
    {
      g_propagate_error (error, internal_error);
      g_clear_object (&(window->impl_window->gl_paint_context));
      return NULL;
    }

  return window->impl_window->gl_paint_context;
}

/**
 * gdk_window_create_gl_context:
 * @window: a #GdkWindow
 * @error: return location for an error
 *
 * Creates a new #GdkGLContext matching the
 * framebuffer format to the visual of the #GdkWindow. The context
 * is disconnected from any particular window or surface.
 *
 * If the creation of the #GdkGLContext failed, @error will be set.
 *
 * Before using the returned #GdkGLContext, you will need to
 * call gdk_gl_context_make_current() or gdk_gl_context_realize().
 *
 * Returns: (transfer full): the newly created #GdkGLContext, or
 * %NULL on error
 *
 * Since: 3.16
 **/
GdkGLContext *
gdk_window_create_gl_context (GdkWindow    *window,
                              GError      **error)
{
  GdkGLContext *paint_context;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  paint_context = gdk_window_get_paint_gl_context (window, error);
  if (paint_context == NULL)
    return NULL;

  return GDK_WINDOW_IMPL_GET_CLASS (window->impl)->create_gl_context (window->impl_window,
								      FALSE,
                                                                      paint_context,
                                                                      error);
}

/**
 * gdk_window_create_vulkan_context:
 * @window: a #GdkWindow
 * @error: return location for an error
 *
 * Creates a new #GdkVulkanContext for rendering on @window.
 *
 * If the creation of the #GdkVulkanContext failed, @error will be set.
 *
 * Returns: (transfer full): the newly created #GdkVulkanContext, or
 * %NULL on error
 *
 * Since: 3.90
 **/
GdkVulkanContext *
gdk_window_create_vulkan_context (GdkWindow  *window,
                                  GError    **error)
{
  GdkDisplay *display;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (_gdk_vulkan_flags & GDK_VULKAN_DISABLE)
    {
      g_set_error_literal (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE,
                           _("Vulkan support disabled via GDK_DEBUG"));
      return NULL;
    }

  display = gdk_window_get_display (window);

  if (GDK_DISPLAY_GET_CLASS (display)->vk_extension_name == NULL)
    {
      g_set_error (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_UNSUPPORTED,
                   "The %s backend has no Vulkan support.", G_OBJECT_TYPE_NAME (display));
      return FALSE;
    }

  return g_initable_new (GDK_DISPLAY_GET_CLASS (display)->vk_context_type,
                         NULL,
                         error,
                         "window", window,
                         NULL);
}

static void
gdk_window_begin_paint_internal (GdkWindow            *window,
			         const cairo_region_t *region)
{
  GdkRectangle clip_box;
  GdkWindowImplClass *impl_class;
  double sx, sy;
  gboolean needs_surface;
  cairo_content_t surface_content;

  if (window->current_paint.surface != NULL)
    {
      g_warning ("A paint operation on the window is alredy in progress. "
                 "This is not allowed.");
      return;
    }

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

  needs_surface = TRUE;
  if (impl_class->begin_paint)
    needs_surface = impl_class->begin_paint (window);

  window->current_paint.region = cairo_region_copy (region);
  cairo_region_intersect (window->current_paint.region, window->clip_region);
  cairo_region_get_extents (window->current_paint.region, &clip_box);

  surface_content = gdk_window_get_content (window);

  if (needs_surface)
    {
      window->current_paint.surface = gdk_window_create_similar_surface (window,
                                                                         surface_content,
                                                                         MAX (clip_box.width, 1),
                                                                         MAX (clip_box.height, 1));
      sx = sy = 1;
      cairo_surface_get_device_scale (window->current_paint.surface, &sx, &sy);
      cairo_surface_set_device_offset (window->current_paint.surface, -clip_box.x*sx, -clip_box.y*sy);
      gdk_cairo_surface_mark_as_direct (window->current_paint.surface, window);

      window->current_paint.surface_needs_composite = TRUE;
    }
  else
    {
      window->current_paint.surface = gdk_window_ref_impl_surface (window);
      window->current_paint.surface_needs_composite = FALSE;
    }

  if (!cairo_region_is_empty (window->current_paint.region))
    gdk_window_clear_backing_region (window);
}

static void
gdk_window_end_paint_internal (GdkWindow *window)
{
  GdkWindowImplClass *impl_class;
  cairo_t *cr;

  if (window->current_paint.surface == NULL)
    {
      g_warning (G_STRLOC": no preceding call to gdk_window_begin_draw_frame(), see documentation");
      return;
    }

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

  if (impl_class->end_paint)
    impl_class->end_paint (window);

  if (window->current_paint.surface_needs_composite)
    {
      cairo_surface_t *surface;

      surface = gdk_window_ref_impl_surface (window);
      cr = cairo_create (surface);

      cairo_set_source_surface (cr, window->current_paint.surface, 0, 0);
      gdk_cairo_region (cr, window->current_paint.region);
      cairo_clip (cr);

      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_paint (cr);

      cairo_destroy (cr);

      cairo_surface_flush (surface);
      cairo_surface_destroy (surface);
    }

  gdk_window_free_current_paint (window);
}

/**
 * gdk_window_begin_draw_frame:
 * @window: a #GdkWindow
 * @context: (allow-none): the context used to draw the frame
 * @region: a Cairo region
 *
 * Indicates that you are beginning the process of redrawing @region
 * on @window, and provides you with a #GdkDrawingContext.
 *
 * If @window is a top level #GdkWindow, backed by a native window
 * implementation, a backing store (offscreen buffer) large enough to
 * contain @region will be created. The backing store will be initialized
 * with the background color or background surface for @window. Then, all
 * drawing operations performed on @window will be diverted to the
 * backing store. When you call gdk_window_end_frame(), the contents of
 * the backing store will be copied to @window, making it visible
 * on screen. Only the part of @window contained in @region will be
 * modified; that is, drawing operations are clipped to @region.
 *
 * The net result of all this is to remove flicker, because the user
 * sees the finished product appear all at once when you call
 * gdk_window_end_draw_frame(). If you draw to @window directly without
 * calling gdk_window_begin_draw_frame(), the user may see flicker
 * as individual drawing operations are performed in sequence.
 *
 * When using GTK+, the widget system automatically places calls to
 * gdk_window_begin_draw_frame() and gdk_window_end_draw_frame() around
 * emissions of the `GtkWidget::draw` signal. That is, if you’re
 * drawing the contents of the widget yourself, you can assume that the
 * widget has a cleared background, is already set as the clip region,
 * and already has a backing store. Therefore in most cases, application
 * code in GTK does not need to call gdk_window_begin_draw_frame()
 * explicitly.
 *
 * Returns: (transfer none): a #GdkDrawingContext context that should be
 *   used to draw the contents of the window; the returned context is owned
 *   by GDK.
 *
 * Since: 3.22
 */
GdkDrawingContext *
gdk_window_begin_draw_frame (GdkWindow            *window,
                             GdkDrawContext       *draw_context,
                             const cairo_region_t *region)
{
  GdkDrawingContext *context;
  cairo_region_t *real_region;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (gdk_window_has_native (window), NULL);
  g_return_val_if_fail (gdk_window_is_toplevel (window), NULL);
  g_return_val_if_fail (region != NULL, NULL);
  if (draw_context != NULL)
    {
      g_return_val_if_fail (GDK_IS_DRAW_CONTEXT (draw_context), NULL);
      g_return_val_if_fail (gdk_draw_context_get_window (draw_context) == window, NULL);
    }

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  if (window->drawing_context != NULL)
    {
      g_critical ("The window %p already has a drawing context. You cannot "
                  "call gdk_window_begin_draw_frame() without calling "
                  "gdk_window_end_draw_frame() first.", window);
      return NULL;
    }

  real_region = cairo_region_copy (region);

  if (draw_context)
    gdk_draw_context_begin_frame (draw_context, real_region);
  else
    gdk_window_begin_paint_internal (window, real_region);

  context = g_object_new (GDK_TYPE_DRAWING_CONTEXT,
                          "window", window,
                          "paint-context", draw_context,
                          "clip", real_region,
                          NULL);

  /* Do not take a reference, to avoid creating cycles */
  window->drawing_context = context;

  cairo_region_destroy (real_region);

  return context;
}

/**
 * gdk_window_end_draw_frame:
 * @window: a #GdkWindow
 * @context: the #GdkDrawingContext created by gdk_window_begin_draw_frame()
 *
 * Indicates that the drawing of the contents of @window started with
 * gdk_window_begin_frame() has been completed.
 *
 * This function will take care of destroying the #GdkDrawingContext.
 *
 * It is an error to call this function without a matching
 * gdk_window_begin_frame() first.
 *
 * Since: 3.22
 */
void
gdk_window_end_draw_frame (GdkWindow         *window,
                           GdkDrawingContext *context)
{
  GdkDrawContext *paint_context;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_DRAWING_CONTEXT (context));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (window->drawing_context == NULL)
    {
      g_critical ("The window %p has no drawing context. You must call "
                  "gdk_window_begin_draw_frame() before calling "
                  "gdk_window_end_draw_frame().", window);
      return;
    }
  g_return_if_fail (window->drawing_context == context);

  paint_context = gdk_drawing_context_get_paint_context (context);
  if (paint_context)
    {
      cairo_region_t *clip = gdk_drawing_context_get_clip (context);

      gdk_draw_context_end_frame (paint_context,
                                  clip,
                                  window->active_update_area);

      cairo_region_destroy (clip);
    }
  else
    {
      gdk_window_end_paint_internal (window);
    }

  window->drawing_context = NULL;

  g_object_unref (context);
}

/*< private >
 * gdk_window_get_current_paint_region:
 * @window: a #GdkWindow
 *
 * Retrieves a copy of the current paint region.
 *
 * Returns: (transfer full): a Cairo region
 */
cairo_region_t *
gdk_window_get_current_paint_region (GdkWindow *window)
{
  cairo_region_t *region;

  if (window->impl_window->current_paint.region != NULL)
    {
      region = cairo_region_copy (window->impl_window->current_paint.region);
      cairo_region_translate (region, -window->abs_x, -window->abs_y);
    }
  else
    {
      region = cairo_region_copy (window->clip_region);
    }

  return region;
}

/*< private >
 * gdk_window_get_drawing_context:
 * @window: a #GdkWindow
 *
 * Retrieves the #GdkDrawingContext associated to @window by
 * gdk_window_begin_draw_frame().
 *
 * Returns: (transfer none) (nullable): a #GdkDrawingContext, if any is set
 */
GdkDrawingContext *
gdk_window_get_drawing_context (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  return window->drawing_context;
}

/**
 * gdk_window_get_clip_region:
 * @window: a #GdkWindow
 * 
 * Computes the region of a window that potentially can be written
 * to by drawing primitives. This region may not take into account
 * other factors such as if the window is obscured by other windows,
 * but no area outside of this region will be affected by drawing
 * primitives.
 * 
 * Returns: a #cairo_region_t. This must be freed with cairo_region_destroy()
 *          when you are done.
 **/
cairo_region_t*
gdk_window_get_clip_region (GdkWindow *window)
{
  cairo_region_t *result;

  g_return_val_if_fail (GDK_WINDOW (window), NULL);

  result = cairo_region_copy (window->clip_region);

  if (window->current_paint.region != NULL)
    cairo_region_intersect (result, window->current_paint.region);

  return result;
}

/**
 * gdk_window_get_visible_region:
 * @window: a #GdkWindow
 * 
 * Computes the region of the @window that is potentially visible.
 * This does not necessarily take into account if the window is
 * obscured by other windows, but no area outside of this region
 * is visible.
 * 
 * Returns: a #cairo_region_t. This must be freed with cairo_region_destroy()
 *          when you are done.
 **/
cairo_region_t *
gdk_window_get_visible_region (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  return cairo_region_copy (window->clip_region);
}

static void
gdk_window_clear_backing_region (GdkWindow *window)
{
  cairo_t *cr;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  cr = cairo_create (window->current_paint.surface);

  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  gdk_cairo_region (cr, window->current_paint.region);
  cairo_fill (cr);

  cairo_destroy (cr);
}

/* This returns either the current working surface on the paint stack
 * or the actual impl surface of the window. This should not be used
 * from very many places: be careful! */
static cairo_surface_t *
ref_window_surface (GdkWindow *window)
{
  if (window->impl_window->current_paint.surface)
    return cairo_surface_reference (window->impl_window->current_paint.surface);
  else
    return gdk_window_ref_impl_surface (window);
}

/* This is used in places like gdk_cairo_set_source_window and
 * other places to take "screenshots" of windows. Thus, we allow
 * it to be used outside of a begin_paint / end_paint. */
cairo_surface_t *
_gdk_window_ref_cairo_surface (GdkWindow *window)
{
  cairo_surface_t *surface;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  surface = ref_window_surface (window);

  if (gdk_window_has_impl (window))
    {
      return surface;
    }
  else
    {
      cairo_surface_t *subsurface;
      subsurface = cairo_surface_create_for_rectangle (surface,
                                                       window->abs_x,
                                                       window->abs_y,
                                                       window->width,
                                                       window->height);
      cairo_surface_destroy (surface);
      return subsurface;
    }
}

/* Code for dirty-region queueing
 */
static GSList *update_windows = NULL;

static inline gboolean
gdk_window_is_ancestor (GdkWindow *window,
			GdkWindow *ancestor)
{
  while (window)
    {
      GdkWindow *parent = window->parent;

      if (parent == ancestor)
	return TRUE;

      window = parent;
    }

  return FALSE;
}

static void
gdk_window_add_update_window (GdkWindow *window)
{
  GSList *tmp;
  GSList *prev = NULL;
  gboolean has_ancestor_in_list = FALSE;

  /*  Check whether "window" is already in "update_windows" list.
   *  It could be added during execution of gtk_widget_destroy() when
   *  setting focus widget to NULL and redrawing old focus widget.
   *  See bug 711552.
   */
  tmp = g_slist_find (update_windows, window);
  if (tmp != NULL)
    return;

  for (tmp = update_windows; tmp; tmp = tmp->next)
    {
      GdkWindow *parent = window->parent;

      /*  check if tmp is an ancestor of "window"; if it is, set a
       *  flag indicating that all following windows are either
       *  children of "window" or from a differen hierarchy
       */
      if (!has_ancestor_in_list && gdk_window_is_ancestor (window, tmp->data))
	has_ancestor_in_list = TRUE;

      /* insert in reverse stacking order when adding around siblings,
       * so processing updates properly paints over lower stacked windows
       */
      if (parent == GDK_WINDOW (tmp->data)->parent)
	{
	  gint index = g_list_index (parent->children, window);
	  for (; tmp && parent == GDK_WINDOW (tmp->data)->parent; tmp = tmp->next)
	    {
	      gint sibling_index = g_list_index (parent->children, tmp->data);
	      if (index > sibling_index)
		break;
	      prev = tmp;
	    }
	  /* here, tmp got advanced past all lower stacked siblings */
	  tmp = g_slist_prepend (tmp, g_object_ref (window));
	  if (prev)
	    prev->next = tmp;
	  else
	    update_windows = tmp;
	  return;
	}

      /*  if "window" has an ancestor in the list and tmp is one of
       *  "window's" children, insert "window" before tmp
       */
      if (has_ancestor_in_list && gdk_window_is_ancestor (tmp->data, window))
	{
	  tmp = g_slist_prepend (tmp, g_object_ref (window));

	  if (prev)
	    prev->next = tmp;
	  else
	    update_windows = tmp;
	  return;
	}

      /*  if we're at the end of the list and had an ancestor it it,
       *  append to the list
       */
      if (! tmp->next && has_ancestor_in_list)
	{
	  tmp = g_slist_append (tmp, g_object_ref (window));
	  return;
	}

      prev = tmp;
    }

  /*  if all above checks failed ("window" is from a different
   *  hierarchy than what is already in the list) or the list is
   *  empty, prepend
   */
  update_windows = g_slist_prepend (update_windows, g_object_ref (window));
}

static void
gdk_window_remove_update_window (GdkWindow *window)
{
  GSList *link;

  link = g_slist_find (update_windows, window);
  if (link != NULL)
    {
      update_windows = g_slist_delete_link (update_windows, link);
      g_object_unref (window);
    }
}

static gboolean
gdk_window_is_toplevel_frozen (GdkWindow *window)
{
  GdkWindow *toplevel;

  toplevel = gdk_window_get_toplevel (window);

  return toplevel->update_and_descendants_freeze_count > 0;
}

static void
gdk_window_schedule_update (GdkWindow *window)
{
  GdkFrameClock *frame_clock;

  if (window &&
      (window->update_freeze_count ||
       gdk_window_is_toplevel_frozen (window)))
    return;

  /* If there's no frame clock (a foreign window), then the invalid
   * region will just stick around unless gdk_window_process_updates()
   * is called. */
  frame_clock = gdk_window_get_frame_clock (window);
  if (frame_clock)
    gdk_frame_clock_request_phase (gdk_window_get_frame_clock (window),
                                   GDK_FRAME_CLOCK_PHASE_PAINT);
}

void
_gdk_window_process_updates_recurse (GdkWindow *window,
                                     cairo_region_t *expose_region)
{
  cairo_region_t *clipped_expose_region;
  GdkEvent event;

  if (window->destroyed)
    return;

  clipped_expose_region = cairo_region_copy (expose_region);

  cairo_region_intersect (clipped_expose_region, window->clip_region);

  if (cairo_region_is_empty (clipped_expose_region))
    goto out;

  /* Paint the window before the children, clipped to the window region */

  event.expose.type = GDK_EXPOSE;
  event.expose.window = window; /* we already hold a ref */
  event.expose.send_event = FALSE;
  event.expose.count = 0;
  event.expose.region = clipped_expose_region;
  cairo_region_get_extents (clipped_expose_region, &event.expose.area);

  _gdk_event_emit (&event);

 out:
  cairo_region_destroy (clipped_expose_region);
}


static void
gdk_window_update_native_shapes (GdkWindow *window)
{
  if (should_apply_clip_as_shape (window))
    apply_clip_as_shape (window);
}

/* Process and remove any invalid area on the native window by creating
 * expose events for the window and all non-native descendants.
 */
static void
gdk_window_process_updates_internal (GdkWindow *window)
{
  GdkWindowImplClass *impl_class;
  GdkWindow *toplevel;

  toplevel = gdk_window_get_toplevel (window);
  if (toplevel->geometry_dirty)
    {
      gdk_window_update_native_shapes (toplevel);
      toplevel->geometry_dirty = FALSE;
    }

  /* Ensure the window lives while updating it */
  g_object_ref (window);

  window->in_update = TRUE;

  /* If an update got queued during update processing, we can get a
   * window in the update queue that has an empty update_area.
   * just ignore it.
   */
  if (window->update_area)
    {
      g_assert (window->active_update_area == NULL); /* No reentrancy */

      window->active_update_area = window->update_area;
      window->update_area = NULL;

      if (gdk_window_is_viewable (window))
	{
	  cairo_region_t *expose_region;

	  expose_region = cairo_region_copy (window->active_update_area);

	  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

	  /* Clip to part visible in impl window */
	  cairo_region_intersect (expose_region, window->clip_region);

          if (impl_class->queue_antiexpose)
            impl_class->queue_antiexpose (window, expose_region);

          impl_class->process_updates_recurse (window, expose_region);

          gdk_window_append_old_updated_area (window, window->active_update_area);

          cairo_region_destroy (expose_region);
        }

      cairo_region_destroy (window->active_update_area);
      window->active_update_area = NULL;
    }

  window->in_update = FALSE;

  g_object_unref (window);
}

static void
gdk_window_paint_on_clock (GdkFrameClock *clock,
			   void          *data)
{
  GdkWindow *window;

  window = GDK_WINDOW (data);

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (window->impl_window == window);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  g_object_ref (window);

  if (window->update_area &&
      !window->update_freeze_count &&
      !gdk_window_is_toplevel_frozen (window) &&

      /* Don't recurse into process_updates_internal, we'll
       * do the update later when idle instead. */
      !window->in_update)
    {
      gdk_window_process_updates_internal (window);
      gdk_window_remove_update_window (window);
    }

  g_object_unref (window);
}

static void
gdk_window_invalidate_rect_full (GdkWindow          *window,
				  const GdkRectangle *rect,
				  gboolean            invalidate_children)
{
  GdkRectangle window_rect;
  cairo_region_t *region;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (window->input_only || !window->viewable)
    return;

  if (!rect)
    {
      window_rect.x = 0;
      window_rect.y = 0;
      window_rect.width = window->width;
      window_rect.height = window->height;
      rect = &window_rect;
    }

  region = cairo_region_create_rectangle (rect);
  gdk_window_invalidate_region_full (window, region, invalidate_children);
  cairo_region_destroy (region);
}

/**
 * gdk_window_invalidate_rect:
 * @window: a #GdkWindow
 * @rect: (allow-none): rectangle to invalidate or %NULL to invalidate the whole
 *      window
 * @invalidate_children: whether to also invalidate child windows
 *
 * A convenience wrapper around gdk_window_invalidate_region() which
 * invalidates a rectangular region. See
 * gdk_window_invalidate_region() for details.
 **/
void
gdk_window_invalidate_rect (GdkWindow          *window,
			    const GdkRectangle *rect,
			    gboolean            invalidate_children)
{
  gdk_window_invalidate_rect_full (window, rect, invalidate_children);
}

static void
impl_window_add_update_area (GdkWindow *impl_window,
			     cairo_region_t *region)
{
  if (impl_window->update_area)
    cairo_region_union (impl_window->update_area, region);
  else
    {
      gdk_window_add_update_window (impl_window);
      impl_window->update_area = cairo_region_copy (region);
      gdk_window_schedule_update (impl_window);
    }
}

static void
gdk_window_invalidate_maybe_recurse_full (GdkWindow            *window,
					  const cairo_region_t *region,
                                          GdkWindowChildFunc    child_func,
					  gpointer              user_data)
{
  cairo_region_t *visible_region;
  cairo_rectangle_int_t r;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (window->input_only ||
      !window->viewable ||
      cairo_region_is_empty (region) ||
      window->window_type == GDK_WINDOW_ROOT)
    return;

  r.x = 0;
  r.y = 0;

  visible_region = cairo_region_copy (region);

  while (window != NULL && 
	 !cairo_region_is_empty (visible_region))
    {
      r.width = window->width;
      r.height = window->height;
      cairo_region_intersect_rectangle (visible_region, &r);

      if (gdk_window_has_impl (window))
	{
	  impl_window_add_update_area (window, visible_region);
	  break;
	}
      else
	{
	  cairo_region_translate (visible_region,
				  window->x, window->y);
	  window = window->parent;
	}
    }

  cairo_region_destroy (visible_region);
}

/**
 * gdk_window_invalidate_maybe_recurse:
 * @window: a #GdkWindow
 * @region: a #cairo_region_t
 * @child_func: (scope call) (allow-none): function to use to decide if to
 *     recurse to a child, %NULL means never recurse.
 * @user_data: data passed to @child_func
 *
 * Adds @region to the update area for @window. The update area is the
 * region that needs to be redrawn, or “dirty region.”
 *
 * GDK will process all updates whenever the frame clock schedules a redraw,
 * so there’s no need to do forces redraws manually, you just need to
 * invalidate regions that you know should be redrawn.
 *
 * The @child_func parameter controls whether the region of
 * each child window that intersects @region will also be invalidated.
 * Only children for which @child_func returns #TRUE will have the area
 * invalidated.
 **/
void
gdk_window_invalidate_maybe_recurse (GdkWindow            *window,
				     const cairo_region_t *region,
                                     GdkWindowChildFunc    child_func,
				     gpointer              user_data)
{
  gdk_window_invalidate_maybe_recurse_full (window, region,
					    child_func, user_data);
}

static gboolean
true_predicate (GdkWindow *window,
		gpointer   user_data)
{
  return TRUE;
}

static void
gdk_window_invalidate_region_full (GdkWindow       *window,
				    const cairo_region_t *region,
				    gboolean         invalidate_children)
{
  gdk_window_invalidate_maybe_recurse_full (window, region,
					    invalidate_children ?
					    true_predicate : (gboolean (*) (GdkWindow *, gpointer))NULL,
				       NULL);
}

/**
 * gdk_window_invalidate_region:
 * @window: a #GdkWindow
 * @region: a #cairo_region_t
 * @invalidate_children: %TRUE to also invalidate child windows
 *
 * Adds @region to the update area for @window. The update area is the
 * region that needs to be redrawn, or “dirty region.”
 *
 * GDK will process all updates whenever the frame clock schedules a redraw,
 * so there’s no need to do forces redraws manually, you just need to
 * invalidate regions that you know should be redrawn.
 *
 * The @invalidate_children parameter controls whether the region of
 * each child window that intersects @region will also be invalidated.
 * If %FALSE, then the update area for child windows will remain
 * unaffected. See gdk_window_invalidate_maybe_recurse if you need
 * fine grained control over which children are invalidated.
 **/
void
gdk_window_invalidate_region (GdkWindow       *window,
			      const cairo_region_t *region,
			      gboolean         invalidate_children)
{
  gdk_window_invalidate_maybe_recurse (window, region,
				       invalidate_children ?
					 true_predicate : (gboolean (*) (GdkWindow *, gpointer))NULL,
				       NULL);
}

/**
 * _gdk_window_invalidate_for_expose:
 * @window: a #GdkWindow
 * @region: a #cairo_region_t
 *
 * Adds @region to the update area for @window.
 *
 * GDK will process all updates whenever the frame clock schedules a redraw,
 * so there’s no need to do forces redraws manually, you just need to
 * invalidate regions that you know should be redrawn.
 *
 * This version of invalidation is used when you recieve expose events
 * from the native window system. It exposes the native window, plus
 * any non-native child windows.
 **/
void
_gdk_window_invalidate_for_expose (GdkWindow       *window,
				   cairo_region_t       *region)
{
  gdk_window_invalidate_maybe_recurse_full (window, region,
					    (gboolean (*) (GdkWindow *, gpointer))gdk_window_has_no_impl,
					    NULL);
}


/**
 * gdk_window_get_update_area:
 * @window: a #GdkWindow
 *
 * Transfers ownership of the update area from @window to the caller
 * of the function. That is, after calling this function, @window will
 * no longer have an invalid/dirty region; the update area is removed
 * from @window and handed to you. If a window has no update area,
 * gdk_window_get_update_area() returns %NULL. You are responsible for
 * calling cairo_region_destroy() on the returned region if it’s non-%NULL.
 *
 * Returns: the update area for @window
 **/
cairo_region_t *
gdk_window_get_update_area (GdkWindow *window)
{
  GdkWindow *impl_window;
  cairo_region_t *tmp_region, *to_remove;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  impl_window = gdk_window_get_impl_window (window);

  if (impl_window->update_area)
    {
      tmp_region = cairo_region_copy (window->clip_region);
      /* Convert to impl coords */
      cairo_region_translate (tmp_region, window->abs_x, window->abs_y);
      cairo_region_intersect (tmp_region, impl_window->update_area);

      if (cairo_region_is_empty (tmp_region))
	{
	  cairo_region_destroy (tmp_region);
	  return NULL;
	}
      else
	{
	  /* Convert from impl coords */
	  cairo_region_translate (tmp_region, -window->abs_x, -window->abs_y);

	  /* Don't remove any update area that is overlapped by sibling windows
	     or child windows as these really need to be repainted independently of this window. */
	  to_remove = cairo_region_copy (tmp_region);

	  remove_child_area (window, FALSE, to_remove);
	  remove_sibling_overlapped_area (window, to_remove);

	  /* Remove from update_area */
	  cairo_region_translate (to_remove, window->abs_x, window->abs_y);
	  cairo_region_subtract (impl_window->update_area, to_remove);

	  cairo_region_destroy (to_remove);

	  if (cairo_region_is_empty (impl_window->update_area))
	    {
	      cairo_region_destroy (impl_window->update_area);
	      impl_window->update_area = NULL;

	      gdk_window_remove_update_window ((GdkWindow *)impl_window);
	    }

	  return tmp_region;
	}
    }
  else
    return NULL;
}

/**
 * _gdk_window_clear_update_area:
 * @window: a #GdkWindow.
 *
 * Internal function to clear the update area for a window. This
 * is called when the window is hidden or destroyed.
 **/
void
_gdk_window_clear_update_area (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->update_area)
    {
      gdk_window_remove_update_window (window);

      cairo_region_destroy (window->update_area);
      window->update_area = NULL;
    }
}

/**
 * gdk_window_freeze_updates:
 * @window: a #GdkWindow
 *
 * Temporarily freezes a window such that it won’t receive expose
 * events.  The window will begin receiving expose events again when
 * gdk_window_thaw_updates() is called. If gdk_window_freeze_updates()
 * has been called more than once, gdk_window_thaw_updates() must be called
 * an equal number of times to begin processing exposes.
 **/
void
gdk_window_freeze_updates (GdkWindow *window)
{
  GdkWindow *impl_window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl_window = gdk_window_get_impl_window (window);
  impl_window->update_freeze_count++;
}

/**
 * gdk_window_thaw_updates:
 * @window: a #GdkWindow
 *
 * Thaws a window frozen with gdk_window_freeze_updates().
 **/
void
gdk_window_thaw_updates (GdkWindow *window)
{
  GdkWindow *impl_window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl_window = gdk_window_get_impl_window (window);

  g_return_if_fail (impl_window->update_freeze_count > 0);

  if (--impl_window->update_freeze_count == 0)
    gdk_window_schedule_update (GDK_WINDOW (impl_window));
}

void
gdk_window_freeze_toplevel_updates (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (window->window_type != GDK_WINDOW_CHILD);

  window->update_and_descendants_freeze_count++;
  _gdk_frame_clock_freeze (gdk_window_get_frame_clock (window));
}

void
gdk_window_thaw_toplevel_updates (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (window->window_type != GDK_WINDOW_CHILD);
  g_return_if_fail (window->update_and_descendants_freeze_count > 0);

  window->update_and_descendants_freeze_count--;
  _gdk_frame_clock_thaw (gdk_window_get_frame_clock (window));

  gdk_window_schedule_update (window);
}

/**
 * gdk_window_constrain_size:
 * @geometry: a #GdkGeometry structure
 * @flags: a mask indicating what portions of @geometry are set
 * @width: desired width of window
 * @height: desired height of the window
 * @new_width: (out): location to store resulting width
 * @new_height: (out): location to store resulting height
 *
 * Constrains a desired width and height according to a
 * set of geometry hints (such as minimum and maximum size).
 */
void
gdk_window_constrain_size (GdkGeometry    *geometry,
			   GdkWindowHints  flags,
			   gint            width,
			   gint            height,
			   gint           *new_width,
			   gint           *new_height)
{
  /* This routine is partially borrowed from fvwm.
   *
   * Copyright 1993, Robert Nation
   *     You may use this code for any purpose, as long as the original
   *     copyright remains in the source code and all documentation
   *
   * which in turn borrows parts of the algorithm from uwm
   */
  gint min_width = 0;
  gint min_height = 0;
  gint base_width = 0;
  gint base_height = 0;
  gint xinc = 1;
  gint yinc = 1;
  gint max_width = G_MAXINT;
  gint max_height = G_MAXINT;

#define FLOOR(value, base)	( ((gint) ((value) / (base))) * (base) )

  if ((flags & GDK_HINT_BASE_SIZE) && (flags & GDK_HINT_MIN_SIZE))
    {
      base_width = geometry->base_width;
      base_height = geometry->base_height;
      min_width = geometry->min_width;
      min_height = geometry->min_height;
    }
  else if (flags & GDK_HINT_BASE_SIZE)
    {
      base_width = geometry->base_width;
      base_height = geometry->base_height;
      min_width = geometry->base_width;
      min_height = geometry->base_height;
    }
  else if (flags & GDK_HINT_MIN_SIZE)
    {
      base_width = geometry->min_width;
      base_height = geometry->min_height;
      min_width = geometry->min_width;
      min_height = geometry->min_height;
    }

  if (flags & GDK_HINT_MAX_SIZE)
    {
      max_width = geometry->max_width ;
      max_height = geometry->max_height;
    }

  if (flags & GDK_HINT_RESIZE_INC)
    {
      xinc = MAX (xinc, geometry->width_inc);
      yinc = MAX (yinc, geometry->height_inc);
    }

  /* clamp width and height to min and max values
   */
  width = CLAMP (width, min_width, max_width);
  height = CLAMP (height, min_height, max_height);

  /* shrink to base + N * inc
   */
  width = base_width + FLOOR (width - base_width, xinc);
  height = base_height + FLOOR (height - base_height, yinc);

  /* constrain aspect ratio, according to:
   *
   *                width
   * min_aspect <= -------- <= max_aspect
   *                height
   */

  if (flags & GDK_HINT_ASPECT &&
      geometry->min_aspect > 0 &&
      geometry->max_aspect > 0)
    {
      gint delta;

      if (geometry->min_aspect * height > width)
	{
	  delta = FLOOR (height - width / geometry->min_aspect, yinc);
	  if (height - delta >= min_height)
	    height -= delta;
	  else
	    {
	      delta = FLOOR (height * geometry->min_aspect - width, xinc);
	      if (width + delta <= max_width)
		width += delta;
	    }
	}

      if (geometry->max_aspect * height < width)
	{
	  delta = FLOOR (width - height * geometry->max_aspect, xinc);
	  if (width - delta >= min_width)
	    width -= delta;
	  else
	    {
	      delta = FLOOR (width / geometry->max_aspect - height, yinc);
	      if (height + delta <= max_height)
		height += delta;
	    }
	}
    }

#undef FLOOR

  *new_width = width;
  *new_height = height;
}

/**
 * gdk_window_get_device_position_double:
 * @window: a #GdkWindow.
 * @device: pointer #GdkDevice to query to.
 * @x: (out) (allow-none): return location for the X coordinate of @device, or %NULL.
 * @y: (out) (allow-none): return location for the Y coordinate of @device, or %NULL.
 * @mask: (out) (allow-none): return location for the modifier mask, or %NULL.
 *
 * Obtains the current device position in doubles and modifier state.
 * The position is given in coordinates relative to the upper left
 * corner of @window.
 *
 * Returns: (nullable) (transfer none): The window underneath @device
 * (as with gdk_device_get_window_at_position()), or %NULL if the
 * window is not known to GDK.
 *
 * Since: 3.10
 **/
GdkWindow *
gdk_window_get_device_position_double (GdkWindow       *window,
                                       GdkDevice       *device,
                                       double          *x,
                                       double          *y,
                                       GdkModifierType *mask)
{
  gdouble tmp_x, tmp_y;
  GdkModifierType tmp_mask;
  gboolean normal_child;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);
  g_return_val_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD, NULL);

  tmp_x = tmp_y = 0;
  tmp_mask = 0;
  normal_child = GDK_WINDOW_IMPL_GET_CLASS (window->impl)->get_device_state (window,
                                                                             device,
                                                                             &tmp_x, &tmp_y,
                                                                             &tmp_mask);
  /* We got the coords on the impl, convert to the window */
  tmp_x -= window->abs_x;
  tmp_y -= window->abs_y;

  if (x)
    *x = tmp_x;
  if (y)
    *y = tmp_y;
  if (mask)
    *mask = tmp_mask;

  _gdk_display_enable_motion_hints (gdk_window_get_display (window), device);

  if (normal_child)
    return _gdk_window_find_child_at (window, tmp_x, tmp_y);
  return NULL;
}

/**
 * gdk_window_get_device_position:
 * @window: a #GdkWindow.
 * @device: pointer #GdkDevice to query to.
 * @x: (out) (allow-none): return location for the X coordinate of @device, or %NULL.
 * @y: (out) (allow-none): return location for the Y coordinate of @device, or %NULL.
 * @mask: (out) (allow-none): return location for the modifier mask, or %NULL.
 *
 * Obtains the current device position and modifier state.
 * The position is given in coordinates relative to the upper left
 * corner of @window.
 *
 * Use gdk_window_get_device_position_double() if you need subpixel precision.
 *
 * Returns: (nullable) (transfer none): The window underneath @device
 * (as with gdk_device_get_window_at_position()), or %NULL if the
 * window is not known to GDK.
 *
 * Since: 3.0
 **/
GdkWindow *
gdk_window_get_device_position (GdkWindow       *window,
                                GdkDevice       *device,
                                gint            *x,
                                gint            *y,
                                GdkModifierType *mask)
{
  gdouble tmp_x, tmp_y;

  window = gdk_window_get_device_position_double (window, device,
                                                  &tmp_x, &tmp_y, mask);
  if (x)
    *x = round (tmp_x);
  if (y)
    *y = round (tmp_y);

  return window;
}

/**
 * gdk_get_default_root_window:
 *
 * Obtains the root window (parent all other windows are inside)
 * for the default display and screen.
 *
 * Returns: (transfer none): the default root window
 **/
GdkWindow *
gdk_get_default_root_window (void)
{
  return gdk_screen_get_root_window (gdk_screen_get_default ());
}


static gboolean
gdk_window_raise_internal (GdkWindow *window)
{
  GdkWindow *parent = window->parent;
  GdkWindowImplClass *impl_class;
  gboolean did_raise = FALSE;

  if (parent && parent->children->data != window)
    {
      parent->children = g_list_remove_link (parent->children, &window->children_list_node);
      parent->children = g_list_concat (&window->children_list_node, parent->children);
      did_raise = TRUE;
    }

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

  /* Just do native raise for toplevels */
  if (gdk_window_has_impl (window))
      impl_class->raise (window);

  return did_raise;
}

/* Returns TRUE If the native window was mapped or unmapped */
static gboolean
set_viewable (GdkWindow *w,
	      gboolean val)
{
  GdkWindow *child;
  GList *l;

  if (w->viewable == val)
    return FALSE;

  w->viewable = val;

  if (val)
    recompute_visible_regions (w, FALSE);

  for (l = w->children; l != NULL; l = l->next)
    {
      child = l->data;

      if (GDK_WINDOW_IS_MAPPED (child))
	set_viewable (child, val);
    }

  return FALSE;
}

/* Returns TRUE If the native window was mapped or unmapped */
gboolean
_gdk_window_update_viewable (GdkWindow *window)
{
  gboolean viewable;

  if (window->window_type == GDK_WINDOW_FOREIGN ||
      window->window_type == GDK_WINDOW_ROOT)
    viewable = TRUE;
  else if (gdk_window_is_toplevel (window) ||
	   window->parent->viewable)
    viewable = GDK_WINDOW_IS_MAPPED (window);
  else
    viewable = FALSE;

  return set_viewable (window, viewable);
}

static void
gdk_window_show_internal (GdkWindow *window, gboolean raise)
{
  GdkWindowImplClass *impl_class;
  gboolean was_mapped, was_viewable;
  gboolean did_show, did_raise = FALSE;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->destroyed)
    return;

  was_mapped = GDK_WINDOW_IS_MAPPED (window);
  was_viewable = window->viewable;

  if (raise)
    {
      /* Keep children in (reverse) stacking order */
      did_raise = gdk_window_raise_internal (window);
    }

  if (gdk_window_has_impl (window))
    {
      if (!was_mapped)
	gdk_synthesize_window_state (window,
				     GDK_WINDOW_STATE_WITHDRAWN,
				     GDK_WINDOW_STATE_FOCUSED);
    }
  else
    {
      window->state = 0;
    }

  did_show = _gdk_window_update_viewable (window);

  /* If it was already viewable the backend show op won't be called, call it
     again to ensure things happen right if the mapped tracking was not right
     for e.g. a foreign window.
     Dunno if this is strictly needed but its what happened pre-csw.
     Also show if not done by gdk_window_update_viewable. */
  if (gdk_window_has_impl (window) && (was_viewable || !did_show))
    {
      impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
      impl_class->show (window, !did_show ? was_mapped : TRUE);
    }

  if (!was_mapped && !gdk_window_has_impl (window))
    {
      if (window->event_mask & GDK_STRUCTURE_MASK)
	_gdk_make_event (window, GDK_MAP, NULL, FALSE);

      if (window->parent && window->parent->event_mask & GDK_SUBSTRUCTURE_MASK)
	_gdk_make_event (window, GDK_MAP, NULL, FALSE);
    }

  if (!was_mapped || did_raise)
    {
      recompute_visible_regions (window, FALSE);

      /* If any decendants became visible we need to send visibility notify */
      gdk_window_update_visibility_recursively (window, NULL);

      if (gdk_window_is_viewable (window))
	{
	  _gdk_synthesize_crossing_events_for_geometry_change (window);
	  gdk_window_invalidate_rect_full (window, NULL, TRUE);
	}
    }
}

/**
 * gdk_window_show_unraised:
 * @window: a #GdkWindow
 *
 * Shows a #GdkWindow onscreen, but does not modify its stacking
 * order. In contrast, gdk_window_show() will raise the window
 * to the top of the window stack.
 *
 * On the X11 platform, in Xlib terms, this function calls
 * XMapWindow() (it also updates some internal GDK state, which means
 * that you can’t really use XMapWindow() directly on a GDK window).
 */
void
gdk_window_show_unraised (GdkWindow *window)
{
  gdk_window_show_internal (window, FALSE);
}

/**
 * gdk_window_raise:
 * @window: a #GdkWindow
 *
 * Raises @window to the top of the Z-order (stacking order), so that
 * other windows with the same parent window appear below @window.
 * This is true whether or not the windows are visible.
 *
 * If @window is a toplevel, the window manager may choose to deny the
 * request to move the window in the Z-order, gdk_window_raise() only
 * requests the restack, does not guarantee it.
 */
void
gdk_window_raise (GdkWindow *window)
{
  gboolean did_raise;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->destroyed)
    return;

  /* Keep children in (reverse) stacking order */
  did_raise = gdk_window_raise_internal (window);

  if (did_raise &&
      !gdk_window_is_toplevel (window) &&
      gdk_window_is_viewable (window) &&
      !window->input_only)
    gdk_window_invalidate_region_full (window, window->clip_region, TRUE);
}

static void
gdk_window_lower_internal (GdkWindow *window)
{
  GdkWindow *parent = window->parent;
  GdkWindowImplClass *impl_class;

  if (parent)
    {
      parent->children = g_list_remove_link (parent->children, &window->children_list_node);
      parent->children = g_list_concat (parent->children, &window->children_list_node);
    }

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

  /* Just do native lower for toplevels */
  if (gdk_window_has_impl (window))
    impl_class->lower (window);
}

static void
gdk_window_invalidate_in_parent (GdkWindow *private)
{
  GdkRectangle r, child;

  if (gdk_window_is_toplevel (private))
    return;

  /* get the visible rectangle of the parent */
  r.x = r.y = 0;
  r.width = private->parent->width;
  r.height = private->parent->height;

  child.x = private->x;
  child.y = private->y;
  child.width = private->width;
  child.height = private->height;
  gdk_rectangle_intersect (&r, &child, &r);

  gdk_window_invalidate_rect_full (private->parent, &r, TRUE);
}


/**
 * gdk_window_lower:
 * @window: a #GdkWindow
 *
 * Lowers @window to the bottom of the Z-order (stacking order), so that
 * other windows with the same parent window appear above @window.
 * This is true whether or not the other windows are visible.
 *
 * If @window is a toplevel, the window manager may choose to deny the
 * request to move the window in the Z-order, gdk_window_lower() only
 * requests the restack, does not guarantee it.
 *
 * Note that gdk_window_show() raises the window again, so don’t call this
 * function before gdk_window_show(). (Try gdk_window_show_unraised().)
 */
void
gdk_window_lower (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->destroyed)
    return;

  /* Keep children in (reverse) stacking order */
  gdk_window_lower_internal (window);

  _gdk_synthesize_crossing_events_for_geometry_change (window);
  gdk_window_invalidate_in_parent (window);
}

/**
 * gdk_window_restack:
 * @window: a #GdkWindow
 * @sibling: (allow-none): a #GdkWindow that is a sibling of @window, or %NULL
 * @above: a boolean
 *
 * Changes the position of  @window in the Z-order (stacking order), so that
 * it is above @sibling (if @above is %TRUE) or below @sibling (if @above is
 * %FALSE).
 *
 * If @sibling is %NULL, then this either raises (if @above is %TRUE) or
 * lowers the window.
 *
 * If @window is a toplevel, the window manager may choose to deny the
 * request to move the window in the Z-order, gdk_window_restack() only
 * requests the restack, does not guarantee it.
 *
 * Since: 2.18
 */
void
gdk_window_restack (GdkWindow     *window,
		    GdkWindow     *sibling,
		    gboolean       above)
{
  GdkWindowImplClass *impl_class;
  GdkWindow *parent;
  GList *sibling_link;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (sibling == NULL || GDK_IS_WINDOW (sibling));

  if (window->destroyed)
    return;

  if (sibling == NULL)
    {
      if (above)
	gdk_window_raise (window);
      else
	gdk_window_lower (window);
      return;
    }

  if (gdk_window_is_toplevel (window))
    {
      g_return_if_fail (gdk_window_is_toplevel (sibling));
      impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
      impl_class->restack_toplevel (window, sibling, above);
      return;
    }

  parent = window->parent;
  if (parent)
    {
      sibling_link = g_list_find (parent->children, sibling);
      g_return_if_fail (sibling_link != NULL);
      if (sibling_link == NULL)
	return;

      parent->children = g_list_remove_link (parent->children, &window->children_list_node);
      if (above)
	parent->children = list_insert_link_before (parent->children,
                                                    sibling_link,
                                                    &window->children_list_node);
      else
	parent->children = list_insert_link_before (parent->children,
                                                    sibling_link->next,
                                                    &window->children_list_node);
    }

  _gdk_synthesize_crossing_events_for_geometry_change (window);
  gdk_window_invalidate_in_parent (window);
}


/**
 * gdk_window_show:
 * @window: a #GdkWindow
 *
 * Like gdk_window_show_unraised(), but also raises the window to the
 * top of the window stack (moves the window to the front of the
 * Z-order).
 *
 * This function maps a window so it’s visible onscreen. Its opposite
 * is gdk_window_hide().
 *
 * When implementing a #GtkWidget, you should call this function on the widget's
 * #GdkWindow as part of the “map” method.
 */
void
gdk_window_show (GdkWindow *window)
{
  gdk_window_show_internal (window, TRUE);
}

/**
 * gdk_window_hide:
 * @window: a #GdkWindow
 *
 * For toplevel windows, withdraws them, so they will no longer be
 * known to the window manager; for all windows, unmaps them, so
 * they won’t be displayed. Normally done automatically as
 * part of gtk_widget_hide().
 */
void
gdk_window_hide (GdkWindow *window)
{
  GdkWindowImplClass *impl_class;
  gboolean was_mapped, did_hide;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->destroyed)
    return;

  was_mapped = GDK_WINDOW_IS_MAPPED (window);

  if (gdk_window_has_impl (window))
    {

      if (GDK_WINDOW_IS_MAPPED (window))
	gdk_synthesize_window_state (window,
				     0,
				     GDK_WINDOW_STATE_WITHDRAWN);
    }
  else if (was_mapped)
    {
      GdkDisplay *display;
      GdkSeat *seat;
      GList *devices, *d;

      /* May need to break grabs on children */
      display = gdk_window_get_display (window);
      seat = gdk_display_get_default_seat (display);

      devices = gdk_seat_get_slaves (seat, GDK_SEAT_CAPABILITY_ALL);
      devices = g_list_prepend (devices, gdk_seat_get_keyboard (seat));
      devices = g_list_prepend (devices, gdk_seat_get_pointer (seat));

      for (d = devices; d; d = d->next)
        {
          GdkDevice *device = d->data;

          if (_gdk_display_end_device_grab (display,
                                            device,
                                            _gdk_display_get_next_serial (display),
                                            window,
                                            TRUE))
            {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
              gdk_device_ungrab (device, GDK_CURRENT_TIME);
G_GNUC_END_IGNORE_DEPRECATIONS
            }
        }

      window->state = GDK_WINDOW_STATE_WITHDRAWN;
      g_list_free (devices);
    }

  did_hide = _gdk_window_update_viewable (window);

  /* Hide foreign window as those are not handled by update_viewable. */
  if (gdk_window_has_impl (window) && (!did_hide))
    {
      impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
      impl_class->hide (window);
    }

  gdk_window_clear_old_updated_area (window);
  recompute_visible_regions (window, FALSE);

  /* all decendants became non-visible, we need to send visibility notify */
  gdk_window_update_visibility_recursively (window, NULL);

  if (was_mapped && !gdk_window_has_impl (window))
    {
      if (window->event_mask & GDK_STRUCTURE_MASK)
	_gdk_make_event (window, GDK_UNMAP, NULL, FALSE);

      if (window->parent && window->parent->event_mask & GDK_SUBSTRUCTURE_MASK)
	_gdk_make_event (window, GDK_UNMAP, NULL, FALSE);

      _gdk_synthesize_crossing_events_for_geometry_change (window->parent);
    }

  /* Invalidate the rect */
  if (was_mapped)
    gdk_window_invalidate_in_parent (window);
}

/**
 * gdk_window_withdraw:
 * @window: a toplevel #GdkWindow
 *
 * Withdraws a window (unmaps it and asks the window manager to forget about it).
 * This function is not really useful as gdk_window_hide() automatically
 * withdraws toplevel windows before hiding them.
 **/
void
gdk_window_withdraw (GdkWindow *window)
{
  GdkWindowImplClass *impl_class;
  gboolean was_mapped;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->destroyed)
    return;

  was_mapped = GDK_WINDOW_IS_MAPPED (window);

  if (gdk_window_has_impl (window))
    {
      impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
      impl_class->withdraw (window);

      if (was_mapped)
	{
	  if (window->event_mask & GDK_STRUCTURE_MASK)
	    _gdk_make_event (window, GDK_UNMAP, NULL, FALSE);

	  if (window->parent && window->parent->event_mask & GDK_SUBSTRUCTURE_MASK)
	    _gdk_make_event (window, GDK_UNMAP, NULL, FALSE);

	  _gdk_synthesize_crossing_events_for_geometry_change (window->parent);
	}

      recompute_visible_regions (window, FALSE);
      gdk_window_clear_old_updated_area (window);
    }
}

/**
 * gdk_window_set_events:
 * @window: a #GdkWindow
 * @event_mask: event mask for @window
 *
 * The event mask for a window determines which events will be reported
 * for that window from all master input devices. For example, an event mask
 * including #GDK_BUTTON_PRESS_MASK means the window should report button
 * press events. The event mask is the bitwise OR of values from the
 * #GdkEventMask enumeration.
 *
 * See the [input handling overview][event-masks] for details.
 **/
void
gdk_window_set_events (GdkWindow       *window,
		       GdkEventMask     event_mask)
{
  GdkWindowImplClass *impl_class;
  GdkDisplay *display;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->destroyed)
    return;

  /* If motion hint is disabled, enable motion events again */
  display = gdk_window_get_display (window);
  if ((window->event_mask & GDK_POINTER_MOTION_HINT_MASK) &&
      !(event_mask & GDK_POINTER_MOTION_HINT_MASK))
    {
      GList *devices = window->devices_inside;

      while (devices)
        {
          _gdk_display_enable_motion_hints (display, (GdkDevice *) devices->data);
          devices = devices->next;
        }
    }

  window->event_mask = event_mask;

  if (gdk_window_has_impl (window))
    {
      impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
      impl_class->set_events (window,
			      get_native_event_mask (window));
    }

}

/**
 * gdk_window_get_events:
 * @window: a #GdkWindow
 *
 * Gets the event mask for @window for all master input devices. See
 * gdk_window_set_events().
 *
 * Returns: event mask for @window
 **/
GdkEventMask
gdk_window_get_events (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (window->destroyed)
    return 0;

  return window->event_mask;
}

/**
 * gdk_window_set_device_events:
 * @window: a #GdkWindow
 * @device: #GdkDevice to enable events for.
 * @event_mask: event mask for @window
 *
 * Sets the event mask for a given device (Normally a floating device, not
 * attached to any visible pointer) to @window. For example, an event mask
 * including #GDK_BUTTON_PRESS_MASK means the window should report button
 * press events. The event mask is the bitwise OR of values from the
 * #GdkEventMask enumeration.
 *
 * See the [input handling overview][event-masks] for details.
 *
 * Since: 3.0
 **/
void
gdk_window_set_device_events (GdkWindow    *window,
                              GdkDevice    *device,
                              GdkEventMask  event_mask)
{
  GdkEventMask device_mask;
  GdkDisplay *display;
  GdkWindow *native;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_DEVICE (device));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* If motion hint is disabled, enable motion events again */
  display = gdk_window_get_display (window);
  if ((window->event_mask & GDK_POINTER_MOTION_HINT_MASK) &&
      !(event_mask & GDK_POINTER_MOTION_HINT_MASK))
    _gdk_display_enable_motion_hints (display, device);

  if (G_UNLIKELY (!window->device_events))
    window->device_events = g_hash_table_new (NULL, NULL);

  if (event_mask == 0)
    {
      /* FIXME: unsetting events on a master device
       * would restore window->event_mask
       */
      g_hash_table_remove (window->device_events, device);
    }
  else
    g_hash_table_insert (window->device_events, device,
                         GINT_TO_POINTER (event_mask));

  native = gdk_window_get_toplevel (window);

  device_mask = get_native_device_event_mask (window, device);
  GDK_DEVICE_GET_CLASS (device)->select_window_events (device, native, device_mask);
}

/**
 * gdk_window_get_device_events:
 * @window: a #GdkWindow.
 * @device: a #GdkDevice.
 *
 * Returns the event mask for @window corresponding to an specific device.
 *
 * Returns: device event mask for @window
 *
 * Since: 3.0
 **/
GdkEventMask
gdk_window_get_device_events (GdkWindow *window,
                              GdkDevice *device)
{
  GdkEventMask mask;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  if (GDK_WINDOW_DESTROYED (window))
    return 0;

  if (!window->device_events)
    return 0;

  mask = GPOINTER_TO_INT (g_hash_table_lookup (window->device_events, device));

  /* FIXME: device could be controlled by window->event_mask */

  return mask;
}

static void
gdk_window_move_resize_toplevel (GdkWindow *window,
                                 gboolean   with_move,
                                 gint       x,
                                 gint       y,
                                 gint       width,
                                 gint       height)
{
  cairo_region_t *old_region, *new_region;
  GdkWindowImplClass *impl_class;
  gboolean expose;
  gboolean is_resize;

  expose = FALSE;
  old_region = NULL;

  is_resize = (width != -1) || (height != -1);

  if (gdk_window_is_viewable (window) &&
      !window->input_only)
    {
      expose = TRUE;
      old_region = cairo_region_copy (window->clip_region);
    }

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
  impl_class->move_resize (window, with_move, x, y, width, height);

  /* Avoid recomputing for pure toplevel moves, for performance reasons */
  if (is_resize)
    recompute_visible_regions (window, FALSE);

  if (expose)
    {
      new_region = cairo_region_copy (window->clip_region);

      /* This is the newly exposed area (due to any resize),
       * X will expose it, but lets do that without the roundtrip
       */
      cairo_region_subtract (new_region, old_region);
      gdk_window_invalidate_region_full (window, new_region, TRUE);

      cairo_region_destroy (old_region);
      cairo_region_destroy (new_region);
    }

  _gdk_synthesize_crossing_events_for_geometry_change (window);
}


static void
gdk_window_move_resize_internal (GdkWindow *window,
				 gboolean   with_move,
				 gint       x,
				 gint       y,
				 gint       width,
				 gint       height)
{
  cairo_region_t *old_region, *new_region;
  gboolean expose;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->destroyed)
    return;

  if (gdk_window_is_toplevel (window))
    {
      gdk_window_move_resize_toplevel (window, with_move, x, y, width, height);
      return;
    }

  if (width == 0)
    width = 1;
  if (height == 0)
    height = 1;

  /* Bail early if no change */
  if (window->width == width &&
      window->height == height &&
      (!with_move ||
       (window->x == x &&
	window->y == y)))
    return;

  /* Handle child windows */

  expose = FALSE;
  old_region = NULL;

  if (gdk_window_is_viewable (window) &&
      !window->input_only)
    {
      GdkRectangle r;

      expose = TRUE;

      r.x = window->x;
      r.y = window->y;
      r.width = window->width;
      r.height = window->height;

      old_region = cairo_region_create_rectangle (&r);
    }

  /* Set the new position and size */
  if (with_move)
    {
      window->x = x;
      window->y = y;
    }
  if (!(width < 0 && height < 0))
    {
      window->width = width;
      window->height = height;
    }

  recompute_visible_regions (window, FALSE);

  if (expose)
    {
      GdkRectangle r;

      r.x = window->x;
      r.y = window->y;
      r.width = window->width;
      r.height = window->height;

      new_region = cairo_region_create_rectangle (&r);

      cairo_region_union (new_region, old_region);

      gdk_window_invalidate_region_full (window->parent, new_region, TRUE);

      cairo_region_destroy (old_region);
      cairo_region_destroy (new_region);
    }

  _gdk_synthesize_crossing_events_for_geometry_change (window);
}



/**
 * gdk_window_move:
 * @window: a #GdkWindow
 * @x: X coordinate relative to window’s parent
 * @y: Y coordinate relative to window’s parent
 *
 * Repositions a window relative to its parent window.
 * For toplevel windows, window managers may ignore or modify the move;
 * you should probably use gtk_window_move() on a #GtkWindow widget
 * anyway, instead of using GDK functions. For child windows,
 * the move will reliably succeed.
 *
 * If you’re also planning to resize the window, use gdk_window_move_resize()
 * to both move and resize simultaneously, for a nicer visual effect.
 **/
void
gdk_window_move (GdkWindow *window,
		 gint       x,
		 gint       y)
{
  gdk_window_move_resize_internal (window, TRUE, x, y, -1, -1);
}

/**
 * gdk_window_resize:
 * @window: a #GdkWindow
 * @width: new width of the window
 * @height: new height of the window
 *
 * Resizes @window; for toplevel windows, asks the window manager to resize
 * the window. The window manager may not allow the resize. When using GTK+,
 * use gtk_window_resize() instead of this low-level GDK function.
 *
 * Windows may not be resized below 1x1.
 *
 * If you’re also planning to move the window, use gdk_window_move_resize()
 * to both move and resize simultaneously, for a nicer visual effect.
 **/
void
gdk_window_resize (GdkWindow *window,
		   gint       width,
		   gint       height)
{
  gdk_window_move_resize_internal (window, FALSE, 0, 0, width, height);
}


/**
 * gdk_window_move_resize:
 * @window: a #GdkWindow
 * @x: new X position relative to window’s parent
 * @y: new Y position relative to window’s parent
 * @width: new width
 * @height: new height
 *
 * Equivalent to calling gdk_window_move() and gdk_window_resize(),
 * except that both operations are performed at once, avoiding strange
 * visual effects. (i.e. the user may be able to see the window first
 * move, then resize, if you don’t use gdk_window_move_resize().)
 **/
void
gdk_window_move_resize (GdkWindow *window,
			gint       x,
			gint       y,
			gint       width,
			gint       height)
{
  gdk_window_move_resize_internal (window, TRUE, x, y, width, height);
}

/**
 * gdk_window_move_to_rect:
 * @window: the #GdkWindow to move
 * @rect: (not nullable): the destination #GdkRectangle to align @window with
 * @rect_anchor: the point on @rect to align with @window's anchor point
 * @window_anchor: the point on @window to align with @rect's anchor point
 * @anchor_hints: positioning hints to use when limited on space
 * @rect_anchor_dx: horizontal offset to shift @window, i.e. @rect's anchor
 *                  point
 * @rect_anchor_dy: vertical offset to shift @window, i.e. @rect's anchor point
 *
 * Moves @window to @rect, aligning their anchor points.
 *
 * @rect is relative to the top-left corner of the window that @window is
 * transient for. @rect_anchor and @window_anchor determine anchor points on
 * @rect and @window to pin together. @rect's anchor point can optionally be
 * offset by @rect_anchor_dx and @rect_anchor_dy, which is equivalent to
 * offsetting the position of @window.
 *
 * @anchor_hints determines how @window will be moved if the anchor points cause
 * it to move off-screen. For example, %GDK_ANCHOR_FLIP_X will replace
 * %GDK_GRAVITY_NORTH_WEST with %GDK_GRAVITY_NORTH_EAST and vice versa if
 * @window extends beyond the left or right edges of the monitor.
 *
 * Connect to the #GdkWindow::moved-to-rect signal to find out how it was
 * actually positioned.
 *
 * Since: 3.22
 * Stability: Private
 */
void
gdk_window_move_to_rect (GdkWindow          *window,
                         const GdkRectangle *rect,
                         GdkGravity          rect_anchor,
                         GdkGravity          window_anchor,
                         GdkAnchorHints      anchor_hints,
                         gint                rect_anchor_dx,
                         gint                rect_anchor_dy)
{
  GdkWindowImplClass *impl_class;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (window->transient_for);
  g_return_if_fail (rect);

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
  impl_class->move_to_rect (window,
                            rect,
                            rect_anchor,
                            window_anchor,
                            anchor_hints,
                            rect_anchor_dx,
                            rect_anchor_dy);
}

/**
 * gdk_window_scroll:
 * @window: a #GdkWindow
 * @dx: Amount to scroll in the X direction
 * @dy: Amount to scroll in the Y direction
 *
 * Scroll the contents of @window, both pixels and children, by the
 * given amount. @window itself does not move. Portions of the window
 * that the scroll operation brings in from offscreen areas are
 * invalidated. The invalidated region may be bigger than what would
 * strictly be necessary.
 *
 * For X11, a minimum area will be invalidated if the window has no
 * subwindows, or if the edges of the window’s parent do not extend
 * beyond the edges of the window. In other cases, a multi-step process
 * is used to scroll the window which may produce temporary visual
 * artifacts and unnecessary invalidations.
 **/
void
gdk_window_scroll (GdkWindow *window,
		   gint       dx,
		   gint       dy)
{
  GList *tmp_list;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (dx == 0 && dy == 0)
    return;

  if (window->destroyed)
    return;

  /* First move all child windows, without causing invalidation */

  tmp_list = window->children;
  while (tmp_list)
    {
      GdkWindow *child = GDK_WINDOW (tmp_list->data);

      /* Just update the positions, the bits will move with the copy */
      child->x += dx;
      child->y += dy;

      tmp_list = tmp_list->next;
    }

  recompute_visible_regions (window, TRUE);

  gdk_window_invalidate_rect_full (window, NULL, TRUE);

  _gdk_synthesize_crossing_events_for_geometry_change (window);
}

/**
 * gdk_window_move_region:
 * @window: a #GdkWindow
 * @region: The #cairo_region_t to move
 * @dx: Amount to move in the X direction
 * @dy: Amount to move in the Y direction
 *
 * Move the part of @window indicated by @region by @dy pixels in the Y
 * direction and @dx pixels in the X direction. The portions of @region
 * that not covered by the new position of @region are invalidated.
 *
 * Child windows are not moved.
 *
 * Since: 2.8
 */
void
gdk_window_move_region (GdkWindow            *window,
                        const cairo_region_t *region,
                        gint                  dx,
                        gint                  dy)
{
  cairo_region_t *expose_area;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (region != NULL);

  if (dx == 0 && dy == 0)
    return;

  if (window->destroyed)
    return;

  expose_area = cairo_region_copy (region);
  cairo_region_translate (expose_area, dx, dy);
  cairo_region_union (expose_area, region);

  gdk_window_invalidate_region_full (window, expose_area, FALSE);
  cairo_region_destroy (expose_area);
}

static void
gdk_window_set_cursor_internal (GdkWindow *window,
                                GdkDevice *device,
                                GdkCursor *cursor)
{
  if (GDK_WINDOW_DESTROYED (window))
    return;

  g_assert (gdk_window_get_display (window) == gdk_device_get_display (device));
  g_assert (!cursor || gdk_window_get_display (window) == gdk_cursor_get_display (cursor));

  if (window->window_type == GDK_WINDOW_ROOT ||
      window->window_type == GDK_WINDOW_FOREIGN)
    GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_device_cursor (window, device, cursor);
  else
    {
      GdkPointerWindowInfo *pointer_info;
      GdkDisplay *display;

      display = gdk_window_get_display (window);
      pointer_info = _gdk_display_get_pointer_info (display, device);

      if (_gdk_window_event_parent_of (window, pointer_info->window_under_pointer))
        update_cursor (display, device);
    }
}

/**
 * gdk_window_get_cursor:
 * @window: a #GdkWindow
 *
 * Retrieves a #GdkCursor pointer for the cursor currently set on the
 * specified #GdkWindow, or %NULL.  If the return value is %NULL then
 * there is no custom cursor set on the specified window, and it is
 * using the cursor for its parent window.
 *
 * Returns: (nullable) (transfer none): a #GdkCursor, or %NULL. The
 *   returned object is owned by the #GdkWindow and should not be
 *   unreferenced directly. Use gdk_window_set_cursor() to unset the
 *   cursor of the window
 *
 * Since: 2.18
 */
GdkCursor *
gdk_window_get_cursor (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  return window->cursor;
}

/**
 * gdk_window_set_cursor:
 * @window: a #GdkWindow
 * @cursor: (allow-none): a cursor
 *
 * Sets the default mouse pointer for a #GdkWindow.
 *
 * Note that @cursor must be for the same display as @window.
 *
 * Use gdk_cursor_new_for_display() or gdk_cursor_new_from_pixbuf() to
 * create the cursor. To make the cursor invisible, use %GDK_BLANK_CURSOR.
 * Passing %NULL for the @cursor argument to gdk_window_set_cursor() means
 * that @window will use the cursor of its parent window. Most windows
 * should use this default.
 */
void
gdk_window_set_cursor (GdkWindow *window,
		       GdkCursor *cursor)
{
  GdkDisplay *display;

  g_return_if_fail (GDK_IS_WINDOW (window));

  display = gdk_window_get_display (window);

  if (window->cursor)
    {
      g_object_unref (window->cursor);
      window->cursor = NULL;
    }

  if (!GDK_WINDOW_DESTROYED (window))
    {
      GdkDevice *device;
      GList *seats, *s;

      if (cursor)
	window->cursor = g_object_ref (cursor);

      seats = gdk_display_list_seats (display);

      for (s = seats; s; s = s->next)
        {
          device = gdk_seat_get_pointer (s->data);
          gdk_window_set_cursor_internal (window, device, window->cursor);
        }

      g_list_free (seats);
      g_object_notify_by_pspec (G_OBJECT (window), properties[PROP_CURSOR]);
    }
}

/**
 * gdk_window_get_device_cursor:
 * @window: a #GdkWindow.
 * @device: a master, pointer #GdkDevice.
 *
 * Retrieves a #GdkCursor pointer for the @device currently set on the
 * specified #GdkWindow, or %NULL.  If the return value is %NULL then
 * there is no custom cursor set on the specified window, and it is
 * using the cursor for its parent window.
 *
 * Returns: (nullable) (transfer none): a #GdkCursor, or %NULL. The
 *   returned object is owned by the #GdkWindow and should not be
 *   unreferenced directly. Use gdk_window_set_cursor() to unset the
 *   cursor of the window
 *
 * Since: 3.0
 **/
GdkCursor *
gdk_window_get_device_cursor (GdkWindow *window,
                              GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);
  g_return_val_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD, NULL);
  g_return_val_if_fail (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_MASTER, NULL);

  return g_hash_table_lookup (window->device_cursor, device);
}

/**
 * gdk_window_set_device_cursor:
 * @window: a #GdkWindow
 * @device: a master, pointer #GdkDevice
 * @cursor: a #GdkCursor
 *
 * Sets a specific #GdkCursor for a given device when it gets inside @window.
 * Use gdk_cursor_new_for_display() or gdk_cursor_new_from_pixbuf() to create
 * the cursor. To make the cursor invisible, use %GDK_BLANK_CURSOR. Passing
 * %NULL for the @cursor argument to gdk_window_set_cursor() means that
 * @window will use the cursor of its parent window. Most windows should
 * use this default.
 *
 * Since: 3.0
 **/
void
gdk_window_set_device_cursor (GdkWindow *window,
                              GdkDevice *device,
                              GdkCursor *cursor)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD);
  g_return_if_fail (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_MASTER);

  if (!cursor)
    g_hash_table_remove (window->device_cursor, device);
  else
    g_hash_table_replace (window->device_cursor, device, g_object_ref (cursor));

  gdk_window_set_cursor_internal (window, device, cursor);
}

/**
 * gdk_window_get_geometry:
 * @window: a #GdkWindow
 * @x: (out) (allow-none): return location for X coordinate of window (relative to its parent)
 * @y: (out) (allow-none): return location for Y coordinate of window (relative to its parent)
 * @width: (out) (allow-none): return location for width of window
 * @height: (out) (allow-none): return location for height of window
 *
 * Any of the return location arguments to this function may be %NULL,
 * if you aren’t interested in getting the value of that field.
 *
 * The X and Y coordinates returned are relative to the parent window
 * of @window, which for toplevels usually means relative to the
 * window decorations (titlebar, etc.) rather than relative to the
 * root window (screen-size background window).
 *
 * On the X11 platform, the geometry is obtained from the X server,
 * so reflects the latest position of @window; this may be out-of-sync
 * with the position of @window delivered in the most-recently-processed
 * #GdkEventConfigure. gdk_window_get_position() in contrast gets the
 * position from the most recent configure event.
 *
 * Note: If @window is not a toplevel, it is much better
 * to call gdk_window_get_position(), gdk_window_get_width() and
 * gdk_window_get_height() instead, because it avoids the roundtrip to
 * the X server and because these functions support the full 32-bit
 * coordinate space, whereas gdk_window_get_geometry() is restricted to
 * the 16-bit coordinates of X11.
 */
void
gdk_window_get_geometry (GdkWindow *window,
			 gint      *x,
			 gint      *y,
			 gint      *width,
			 gint      *height)
{
  GdkWindow *parent;
  GdkWindowImplClass *impl_class;

  if (!window)
    window = gdk_screen_get_root_window ((gdk_screen_get_default ()));

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (gdk_window_has_impl (window))
	{
	  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
	  impl_class->get_geometry (window, x, y,
				    width, height);
	  /* This reports the position wrt to the native parent, we need to convert
	     it to be relative to the client side parent */
	  parent = window->parent;
	  if (parent && !gdk_window_has_impl (parent))
	    {
	      if (x)
		*x -= parent->abs_x;
	      if (y)
		*y -= parent->abs_y;
	    }
	}
      else
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
}

/**
 * gdk_window_get_width:
 * @window: a #GdkWindow
 *
 * Returns the width of the given @window.
 *
 * On the X11 platform the returned size is the size reported in the
 * most-recently-processed configure event, rather than the current
 * size on the X server.
 *
 * Returns: The width of @window
 *
 * Since: 2.24
 */
int
gdk_window_get_width (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  return window->width;
}

/**
 * gdk_window_get_height:
 * @window: a #GdkWindow
 *
 * Returns the height of the given @window.
 *
 * On the X11 platform the returned size is the size reported in the
 * most-recently-processed configure event, rather than the current
 * size on the X server.
 *
 * Returns: The height of @window
 *
 * Since: 2.24
 */
int
gdk_window_get_height (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  return window->height;
}

/**
 * gdk_window_get_origin:
 * @window: a #GdkWindow
 * @x: (out) (allow-none): return location for X coordinate
 * @y: (out) (allow-none): return location for Y coordinate
 *
 * Obtains the position of a window in root window coordinates.
 * (Compare with gdk_window_get_position() and
 * gdk_window_get_geometry() which return the position of a window
 * relative to its parent window.)
 *
 * Returns: not meaningful, ignore
 */
gint
gdk_window_get_origin (GdkWindow *window,
		       gint      *x,
		       gint      *y)
{
  gint dummy_x, dummy_y;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  gdk_window_get_root_coords (window,
                              0, 0,
                              x ? x : &dummy_x,
                              y ? y : &dummy_y);

  return TRUE;
}

/**
 * gdk_window_get_root_coords:
 * @window: a #GdkWindow
 * @x: X coordinate in window
 * @y: Y coordinate in window
 * @root_x: (out): return location for X coordinate
 * @root_y: (out): return location for Y coordinate
 *
 * Obtains the position of a window position in root
 * window coordinates. This is similar to
 * gdk_window_get_origin() but allows you to pass
 * in any position in the window, not just the origin.
 *
 * Since: 2.18
 */
void
gdk_window_get_root_coords (GdkWindow *window,
			    gint       x,
			    gint       y,
			    gint      *root_x,
			    gint      *root_y)
{
  GdkWindowImplClass *impl_class;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    {
      *root_x = 0;
      *root_y = 0;
      return;
    }
  
  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
  impl_class->get_root_coords (window->impl_window,
			       x + window->abs_x,
			       y + window->abs_y,
			       root_x, root_y);
}

/**
 * gdk_window_coords_to_parent:
 * @window: a child window
 * @x: X coordinate in child’s coordinate system
 * @y: Y coordinate in child’s coordinate system
 * @parent_x: (out) (allow-none): return location for X coordinate
 * in parent’s coordinate system, or %NULL
 * @parent_y: (out) (allow-none): return location for Y coordinate
 * in parent’s coordinate system, or %NULL
 *
 * Transforms window coordinates from a child window to its parent
 * window. Calling this function is equivalent to adding the return
 * values of gdk_window_get_position() to the child coordinates.
 *
 * See also: gdk_window_coords_from_parent()
 *
 * Since: 2.22
 **/
void
gdk_window_coords_to_parent (GdkWindow *window,
                             gdouble    x,
                             gdouble    y,
                             gdouble   *parent_x,
                             gdouble   *parent_y)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (parent_x)
    *parent_x = x + window->x;

  if (parent_y)
    *parent_y = y + window->y;
}

/**
 * gdk_window_coords_from_parent:
 * @window: a child window
 * @parent_x: X coordinate in parent’s coordinate system
 * @parent_y: Y coordinate in parent’s coordinate system
 * @x: (out) (allow-none): return location for X coordinate in child’s coordinate system
 * @y: (out) (allow-none): return location for Y coordinate in child’s coordinate system
 *
 * Transforms window coordinates from a parent window to a child
 * window.
 *
 * Calling this function is equivalent to subtracting the return
 * values of gdk_window_get_position() from the parent coordinates.
 *
 * See also: gdk_window_coords_to_parent()
 *
 * Since: 2.22
 **/
void
gdk_window_coords_from_parent (GdkWindow *window,
                               gdouble    parent_x,
                               gdouble    parent_y,
                               gdouble   *x,
                               gdouble   *y)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (x)
    *x = parent_x - window->x;

  if (y)
    *y = parent_y - window->y;
}

/**
 * gdk_window_shape_combine_region:
 * @window: a #GdkWindow
 * @shape_region: (allow-none): region of window to be non-transparent
 * @offset_x: X position of @shape_region in @window coordinates
 * @offset_y: Y position of @shape_region in @window coordinates
 *
 * Makes pixels in @window outside @shape_region be transparent,
 * so that the window may be nonrectangular.
 *
 * If @shape_region is %NULL, the shape will be unset, so the whole
 * window will be opaque again. @offset_x and @offset_y are ignored
 * if @shape_region is %NULL.
 *
 * On the X11 platform, this uses an X server extension which is
 * widely available on most common platforms, but not available on
 * very old X servers, and occasionally the implementation will be
 * buggy. On servers without the shape extension, this function
 * will do nothing.
 *
 * This function works on both toplevel and child windows.
 */
void
gdk_window_shape_combine_region (GdkWindow       *window,
				 const cairo_region_t *shape_region,
				 gint             offset_x,
				 gint             offset_y)
{
  cairo_region_t *old_region, *new_region, *diff;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (!window->shape && shape_region == NULL)
    return;

  window->shaped = (shape_region != NULL);

  if (window->shape)
    cairo_region_destroy (window->shape);

  old_region = NULL;
  if (GDK_WINDOW_IS_MAPPED (window))
    old_region = cairo_region_copy (window->clip_region);

  if (shape_region)
    {
      window->shape = cairo_region_copy (shape_region);
      cairo_region_translate (window->shape, offset_x, offset_y);
    }
  else
    window->shape = NULL;

  recompute_visible_regions (window, FALSE);

  if (old_region)
    {
      new_region = cairo_region_copy (window->clip_region);

      /* New area in the window, needs invalidation */
      diff = cairo_region_copy (new_region);
      cairo_region_subtract (diff, old_region);

      gdk_window_invalidate_region_full (window, diff, TRUE);

      cairo_region_destroy (diff);

      if (!gdk_window_is_toplevel (window))
	{
	  /* New area in the non-root parent window, needs invalidation */
	  diff = cairo_region_copy (old_region);
	  cairo_region_subtract (diff, new_region);

	  /* Adjust region to parent window coords */
	  cairo_region_translate (diff, window->x, window->y);

	  gdk_window_invalidate_region_full (window->parent, diff, TRUE);

	  cairo_region_destroy (diff);
	}

      cairo_region_destroy (new_region);
      cairo_region_destroy (old_region);
    }
}

static void
do_child_shapes (GdkWindow *window,
		 gboolean merge)
{
  GdkRectangle r;
  cairo_region_t *region;

  r.x = 0;
  r.y = 0;
  r.width = window->width;
  r.height = window->height;

  region = cairo_region_create_rectangle (&r);
  remove_child_area (window, FALSE, region);

  if (merge && window->shape)
    cairo_region_subtract (region, window->shape);

  cairo_region_xor_rectangle (region, &r);

  gdk_window_shape_combine_region (window, region, 0, 0);

  cairo_region_destroy (region);
}

/**
 * gdk_window_set_child_shapes:
 * @window: a #GdkWindow
 *
 * Sets the shape mask of @window to the union of shape masks
 * for all children of @window, ignoring the shape mask of @window
 * itself. Contrast with gdk_window_merge_child_shapes() which includes
 * the shape mask of @window in the masks to be merged.
 **/
void
gdk_window_set_child_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  do_child_shapes (window, FALSE);
}

/**
 * gdk_window_merge_child_shapes:
 * @window: a #GdkWindow
 *
 * Merges the shape masks for any child windows into the
 * shape mask for @window. i.e. the union of all masks
 * for @window and its children will become the new mask
 * for @window. See gdk_window_shape_combine_region().
 *
 * This function is distinct from gdk_window_set_child_shapes()
 * because it includes @window’s shape mask in the set of shapes to
 * be merged.
 */
void
gdk_window_merge_child_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  do_child_shapes (window, TRUE);
}

/**
 * gdk_window_input_shape_combine_region:
 * @window: a #GdkWindow
 * @shape_region: region of window to be non-transparent
 * @offset_x: X position of @shape_region in @window coordinates
 * @offset_y: Y position of @shape_region in @window coordinates
 *
 * Like gdk_window_shape_combine_region(), but the shape applies
 * only to event handling. Mouse events which happen while
 * the pointer position corresponds to an unset bit in the
 * mask will be passed on the window below @window.
 *
 * An input shape is typically used with RGBA windows.
 * The alpha channel of the window defines which pixels are
 * invisible and allows for nicely antialiased borders,
 * and the input shape controls where the window is
 * “clickable”.
 *
 * On the X11 platform, this requires version 1.1 of the
 * shape extension.
 *
 * On the Win32 platform, this functionality is not present and the
 * function does nothing.
 *
 * Since: 2.10
 */
void
gdk_window_input_shape_combine_region (GdkWindow       *window,
				       const cairo_region_t *shape_region,
				       gint             offset_x,
				       gint             offset_y)
{
  GdkWindowImplClass *impl_class;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (window->input_shape)
    cairo_region_destroy (window->input_shape);

  if (shape_region)
    {
      window->input_shape = cairo_region_copy (shape_region);
      cairo_region_translate (window->input_shape, offset_x, offset_y);
    }
  else
    window->input_shape = NULL;

  if (gdk_window_has_impl (window))
    {
      impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
      impl_class->input_shape_combine_region (window, window->input_shape, 0, 0);
    }

  /* Pointer may have e.g. moved outside window due to the input mask change */
  _gdk_synthesize_crossing_events_for_geometry_change (window);
}

static void
do_child_input_shapes (GdkWindow *window,
		       gboolean merge)
{
  GdkRectangle r;
  cairo_region_t *region;

  r.x = 0;
  r.y = 0;
  r.width = window->width;
  r.height = window->height;

  region = cairo_region_create_rectangle (&r);
  remove_child_area (window, TRUE, region);

  if (merge && window->shape)
    cairo_region_subtract (region, window->shape);
  if (merge && window->input_shape)
    cairo_region_subtract (region, window->input_shape);

  cairo_region_xor_rectangle (region, &r);

  gdk_window_input_shape_combine_region (window, region, 0, 0);
}


/**
 * gdk_window_set_child_input_shapes:
 * @window: a #GdkWindow
 *
 * Sets the input shape mask of @window to the union of input shape masks
 * for all children of @window, ignoring the input shape mask of @window
 * itself. Contrast with gdk_window_merge_child_input_shapes() which includes
 * the input shape mask of @window in the masks to be merged.
 *
 * Since: 2.10
 **/
void
gdk_window_set_child_input_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  do_child_input_shapes (window, FALSE);
}

/**
 * gdk_window_set_pass_through:
 * @window: a #GdkWindow
 * @pass_through: a boolean
 *
 * Sets whether input to the window is passed through to the window
 * below.
 *
 * The default value of this is %FALSE, which means that pointer
 * events that happen inside the window are send first to the window,
 * but if the event is not selected by the event mask then the event
 * is sent to the parent window, and so on up the hierarchy.
 *
 * If @pass_through is %TRUE then such pointer events happen as if the
 * window wasn't there at all, and thus will be sent first to any
 * windows below @window. This is useful if the window is used in a
 * transparent fashion. In the terminology of the web this would be called
 * "pointer-events: none".
 *
 * Note that a window with @pass_through %TRUE can still have a subwindow
 * without pass through, so you can get events on a subset of a window. And in
 * that cases you would get the in-between related events such as the pointer
 * enter/leave events on its way to the destination window.
 *
 * Since: 3.18
 **/
void
gdk_window_set_pass_through (GdkWindow *window,
                             gboolean   pass_through)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  window->pass_through = !!pass_through;

  /* Pointer may have e.g. moved outside window due to the input region change */
  _gdk_synthesize_crossing_events_for_geometry_change (window);
}

/**
 * gdk_window_get_pass_through:
 * @window: a #GdkWindow
 *
 * Returns whether input to the window is passed through to the window
 * below.
 *
 * See gdk_window_set_pass_through() for details
 *
 * Since: 3.18
 **/
gboolean
gdk_window_get_pass_through (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  return window->pass_through;
}

/**
 * gdk_window_merge_child_input_shapes:
 * @window: a #GdkWindow
 *
 * Merges the input shape masks for any child windows into the
 * input shape mask for @window. i.e. the union of all input masks
 * for @window and its children will become the new input mask
 * for @window. See gdk_window_input_shape_combine_region().
 *
 * This function is distinct from gdk_window_set_child_input_shapes()
 * because it includes @window’s input shape mask in the set of
 * shapes to be merged.
 *
 * Since: 2.10
 **/
void
gdk_window_merge_child_input_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  do_child_input_shapes (window, TRUE);
}


/**
 * gdk_window_get_modal_hint:
 * @window: A toplevel #GdkWindow.
 *
 * Determines whether or not the window manager is hinted that @window
 * has modal behaviour.
 *
 * Returns: whether or not the window has the modal hint set.
 *
 * Since: 2.22
 */
gboolean
gdk_window_get_modal_hint (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  return window->modal_hint;
}

/**
 * gdk_window_get_accept_focus:
 * @window: a toplevel #GdkWindow.
 *
 * Determines whether or not the desktop environment shuld be hinted that
 * the window does not want to receive input focus.
 *
 * Returns: whether or not the window should receive input focus.
 *
 * Since: 2.22
 */
gboolean
gdk_window_get_accept_focus (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  return window->accept_focus;
}

/**
 * gdk_window_get_focus_on_map:
 * @window: a toplevel #GdkWindow.
 *
 * Determines whether or not the desktop environment should be hinted that the
 * window does not want to receive input focus when it is mapped.
 *
 * Returns: whether or not the window wants to receive input focus when
 * it is mapped.
 *
 * Since: 2.22
 */
gboolean
gdk_window_get_focus_on_map (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  return window->focus_on_map;
}

/**
 * gdk_window_is_input_only:
 * @window: a toplevel #GdkWindow
 *
 * Determines whether or not the window is an input only window.
 *
 * Returns: %TRUE if @window is input only
 *
 * Since: 2.22
 */
gboolean
gdk_window_is_input_only (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  return window->input_only;
}

/**
 * gdk_window_is_shaped:
 * @window: a toplevel #GdkWindow
 *
 * Determines whether or not the window is shaped.
 *
 * Returns: %TRUE if @window is shaped
 *
 * Since: 2.22
 */
gboolean
gdk_window_is_shaped (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  return window->shaped;
}

/* Gets the toplevel for a window as used for events,
   i.e. including offscreen parents going up to the native
   toplevel */
static GdkWindow *
get_event_toplevel (GdkWindow *window)
{
  GdkWindow *parent;

  while ((parent = window->parent) != NULL &&
	 (parent->window_type != GDK_WINDOW_ROOT))
    window = parent;

  return window;
}

gboolean
_gdk_window_event_parent_of (GdkWindow *parent,
 	  	             GdkWindow *child)
{
  GdkWindow *w;

  w = child;
  while (w != NULL)
    {
      if (w == parent)
	return TRUE;

      w = w->parent;
    }

  return FALSE;
}

static void
update_cursor (GdkDisplay *display,
               GdkDevice  *device)
{
  GdkWindow *cursor_window, *parent, *toplevel;
  GdkWindow *pointer_window;
  GdkWindowImplClass *impl_class;
  GdkPointerWindowInfo *pointer_info;
  GdkDeviceGrabInfo *grab;
  GdkCursor *cursor;

  pointer_info = _gdk_display_get_pointer_info (display, device);
  pointer_window = pointer_info->window_under_pointer;

  /* We ignore the serials here and just pick the last grab
     we've sent, as that would shortly be used anyway. */
  grab = _gdk_display_get_last_device_grab (display, device);
  if (/* have grab */
      grab != NULL &&
      /* the pointer is not in a descendant of the grab window */
      !_gdk_window_event_parent_of (grab->window, pointer_window))
    {
      /* use the cursor from the grab window */
      cursor_window = grab->window;
    }
  else
    {
      /* otherwise use the cursor from the pointer window */
      cursor_window = pointer_window;
    }

  /* Find the first window with the cursor actually set, as
     the cursor is inherited from the parent */
  while (cursor_window->cursor == NULL &&
         !g_hash_table_contains (cursor_window->device_cursor, device) &&
	 (parent = cursor_window->parent) != NULL &&
	 parent->window_type != GDK_WINDOW_ROOT)
    cursor_window = parent;

  cursor = g_hash_table_lookup (cursor_window->device_cursor, device);

  if (!cursor)
    cursor = cursor_window->cursor;

  /* Set all cursors on toplevel, otherwise its tricky to keep track of
   * which native window has what cursor set. */
  toplevel = get_event_toplevel (pointer_window);
  impl_class = GDK_WINDOW_IMPL_GET_CLASS (toplevel->impl);
  impl_class->set_device_cursor (toplevel, device, cursor);
}

static gboolean
point_in_window (GdkWindow *window,
		 gdouble    x,
                 gdouble    y)
{
  return
    x >= 0 && x < window->width &&
    y >= 0 && y < window->height &&
    (window->shape == NULL ||
     cairo_region_contains_point (window->shape,
			  x, y)) &&
    (window->input_shape == NULL ||
     cairo_region_contains_point (window->input_shape,
			  x, y));
}

/* Same as point_in_window, except it also takes pass_through and its
   interaction with child windows into account */
static gboolean
point_in_input_window (GdkWindow *window,
		       gdouble    x,
		       gdouble    y,
		       GdkWindow **input_window,
		       gdouble   *input_window_x,
		       gdouble   *input_window_y)
{
  GdkWindow *sub;
  double child_x, child_y;
  GList *l;

  if (!point_in_window (window, x, y))
    return FALSE;

  if (!window->pass_through)
    {
      if (input_window)
	{
	  *input_window = window;
	  *input_window_x = x;
	  *input_window_y = y;
	}
      return TRUE;
    }

  /* For pass-through, must be over a child input window */

  /* Children is ordered in reverse stack order, i.e. first is topmost */
  for (l = window->children; l != NULL; l = l->next)
    {
      sub = l->data;

      if (!GDK_WINDOW_IS_MAPPED (sub))
	continue;

      gdk_window_coords_from_parent ((GdkWindow *)sub,
				     x, y,
				     &child_x, &child_y);
      if (point_in_input_window (sub, child_x, child_y,
				 input_window, input_window_x, input_window_y))
	{
	  if (input_window)
	    gdk_window_coords_to_parent (sub,
					 *input_window_x,
					 *input_window_y,
					 input_window_x,
					 input_window_y);
	  return TRUE;
	}
    }

  return FALSE;
}

static GdkWindow *
convert_native_coords_to_toplevel (GdkWindow *window,
				   gdouble    child_x,
                                   gdouble    child_y,
				   gdouble   *toplevel_x,
                                   gdouble   *toplevel_y)
{
  gdouble x, y;

  x = child_x;
  y = child_y;

  while (!gdk_window_is_toplevel (window))
    {
      x += window->x;
      y += window->y;
      window = window->parent;
    }

  *toplevel_x = x;
  *toplevel_y = y;

  return window;
}

static void
convert_toplevel_coords_to_window (GdkWindow *window,
				   gdouble    toplevel_x,
				   gdouble    toplevel_y,
				   gdouble   *window_x,
				   gdouble   *window_y)
{
  GdkWindow *parent;
  gdouble x, y;
  GList *children, *l;

  x = toplevel_x;
  y = toplevel_y;

  children = NULL;
  while ((parent = window->parent) != NULL &&
	 (parent->window_type != GDK_WINDOW_ROOT))
    {
      children = g_list_prepend (children, window);
      window = parent;
    }

  for (l = children; l != NULL; l = l->next)
    gdk_window_coords_from_parent (l->data, x, y, &x, &y);

  g_list_free (children);

  *window_x = x;
  *window_y = y;
}

GdkWindow *
_gdk_window_find_child_at (GdkWindow *window,
			   double     x,
                           double     y)
{
  GdkWindow *sub;
  double child_x, child_y;
  GList *l;

  if (point_in_window (window, x, y))
    {
      /* Children is ordered in reverse stack order, i.e. first is topmost */
      for (l = window->children; l != NULL; l = l->next)
	{
	  sub = l->data;

	  if (!GDK_WINDOW_IS_MAPPED (sub))
	    continue;

	  gdk_window_coords_from_parent ((GdkWindow *)sub,
                                         x, y,
                                         &child_x, &child_y);
	  if (point_in_input_window (sub, child_x, child_y,
				     NULL, NULL, NULL))
	    return (GdkWindow *)sub;
	}
    }

  return NULL;
}

GdkWindow *
_gdk_window_find_descendant_at (GdkWindow *window,
				gdouble    x,
                                gdouble    y,
				gdouble   *found_x,
				gdouble   *found_y)
{
  GdkWindow *sub, *input_window;
  gdouble child_x, child_y;
  GList *l;
  gboolean found;

  if (point_in_window (window, x, y))
    {
      do
	{
	  found = FALSE;
	  /* Children is ordered in reverse stack order, i.e. first is topmost */
	  for (l = window->children; l != NULL; l = l->next)
	    {
	      sub = l->data;

	      if (!GDK_WINDOW_IS_MAPPED (sub))
		continue;

	      gdk_window_coords_from_parent ((GdkWindow *)sub,
                                             x, y,
                                             &child_x, &child_y);
	      if (point_in_input_window (sub, child_x, child_y,
					 &input_window, &child_x, &child_y))
		{
		  x = child_x;
		  y = child_y;
		  window = input_window;
		  found = TRUE;
		  break;
		}
	    }
	}
      while (found);
    }
  else
    {
      /* Not in window at all */
      window = NULL;
    }

  if (found_x)
    *found_x = x;
  if (found_y)
    *found_y = y;

  return window;
}

/**
 * gdk_window_beep:
 * @window: a toplevel #GdkWindow
 *
 * Emits a short beep associated to @window in the appropriate
 * display, if supported. Otherwise, emits a short beep on
 * the display just as gdk_display_beep().
 *
 * Since: 2.12
 **/
void
gdk_window_beep (GdkWindow *window)
{
  GdkDisplay *display;
  GdkWindow *toplevel;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  toplevel = get_event_toplevel (window);
  display = gdk_window_get_display (window);

  if (toplevel)
    {
      if (GDK_WINDOW_IMPL_GET_CLASS (toplevel->impl)->beep (toplevel))
        return;
    }
  
  /* If windows fail to beep, we beep the display. */
  gdk_display_beep (display);
}

/**
 * gdk_window_set_support_multidevice:
 * @window: a #GdkWindow.
 * @support_multidevice: %TRUE to enable multidevice support in @window.
 *
 * This function will enable multidevice features in @window.
 *
 * Multidevice aware windows will need to handle properly multiple,
 * per device enter/leave events, device grabs and grab ownerships.
 *
 * Since: 3.0
 **/
void
gdk_window_set_support_multidevice (GdkWindow *window,
                                    gboolean   support_multidevice)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (window->support_multidevice == support_multidevice)
    return;

  window->support_multidevice = support_multidevice;

  /* FIXME: What to do if called when some pointers are inside the window ? */
}

/**
 * gdk_window_get_support_multidevice:
 * @window: a #GdkWindow.
 *
 * Returns %TRUE if the window is aware of the existence of multiple
 * devices.
 *
 * Returns: %TRUE if the window handles multidevice features.
 *
 * Since: 3.0
 **/
gboolean
gdk_window_get_support_multidevice (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;

  return window->support_multidevice;
}

static const guint type_masks[] = {
  GDK_SUBSTRUCTURE_MASK, /* GDK_DELETE                 = 0  */
  GDK_STRUCTURE_MASK, /* GDK_DESTROY                   = 1  */
  GDK_EXPOSURE_MASK, /* GDK_EXPOSE                     = 2  */
  GDK_POINTER_MOTION_MASK, /* GDK_MOTION_NOTIFY        = 3  */
  GDK_BUTTON_PRESS_MASK, /* GDK_BUTTON_PRESS           = 4  */
  GDK_BUTTON_PRESS_MASK, /* GDK_2BUTTON_PRESS          = 5  */
  GDK_BUTTON_PRESS_MASK, /* GDK_3BUTTON_PRESS          = 6  */
  GDK_BUTTON_RELEASE_MASK, /* GDK_BUTTON_RELEASE       = 7  */
  GDK_KEY_PRESS_MASK, /* GDK_KEY_PRESS                 = 8  */
  GDK_KEY_RELEASE_MASK, /* GDK_KEY_RELEASE             = 9  */
  GDK_ENTER_NOTIFY_MASK, /* GDK_ENTER_NOTIFY           = 10 */
  GDK_LEAVE_NOTIFY_MASK, /* GDK_LEAVE_NOTIFY           = 11 */
  GDK_FOCUS_CHANGE_MASK, /* GDK_FOCUS_CHANGE           = 12 */
  GDK_STRUCTURE_MASK, /* GDK_CONFIGURE                 = 13 */
  GDK_VISIBILITY_NOTIFY_MASK, /* GDK_MAP               = 14 */
  GDK_VISIBILITY_NOTIFY_MASK, /* GDK_UNMAP             = 15 */
  GDK_PROPERTY_CHANGE_MASK, /* GDK_PROPERTY_NOTIFY     = 16 */
  GDK_PROPERTY_CHANGE_MASK, /* GDK_SELECTION_CLEAR     = 17 */
  GDK_PROPERTY_CHANGE_MASK, /* GDK_SELECTION_REQUEST   = 18 */
  GDK_PROPERTY_CHANGE_MASK, /* GDK_SELECTION_NOTIFY    = 19 */
  GDK_PROXIMITY_IN_MASK, /* GDK_PROXIMITY_IN           = 20 */
  GDK_PROXIMITY_OUT_MASK, /* GDK_PROXIMITY_OUT         = 21 */
  GDK_ALL_EVENTS_MASK, /* GDK_DRAG_ENTER               = 22 */
  GDK_ALL_EVENTS_MASK, /* GDK_DRAG_LEAVE               = 23 */
  GDK_ALL_EVENTS_MASK, /* GDK_DRAG_MOTION              = 24 */
  GDK_ALL_EVENTS_MASK, /* GDK_DRAG_STATUS              = 25 */
  GDK_ALL_EVENTS_MASK, /* GDK_DROP_START               = 26 */
  GDK_ALL_EVENTS_MASK, /* GDK_DROP_FINISHED            = 27 */
  GDK_ALL_EVENTS_MASK, /* GDK_CLIENT_EVENT	       = 28 */
  GDK_VISIBILITY_NOTIFY_MASK, /* GDK_VISIBILITY_NOTIFY = 29 */
  GDK_EXPOSURE_MASK, /* GDK_NO_EXPOSE                  = 30 */
  GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK,/* GDK_SCROLL= 31 */
  0, /* GDK_WINDOW_STATE = 32 */
  0, /* GDK_SETTING = 33 */
  0, /* GDK_OWNER_CHANGE = 34 */
  0, /* GDK_GRAB_BROKEN = 35 */
  0, /* GDK_DAMAGE = 36 */
  GDK_TOUCH_MASK, /* GDK_TOUCH_BEGIN = 37 */
  GDK_TOUCH_MASK, /* GDK_TOUCH_UPDATE = 38 */
  GDK_TOUCH_MASK, /* GDK_TOUCH_END = 39 */
  GDK_TOUCH_MASK, /* GDK_TOUCH_CANCEL = 40 */
  GDK_TOUCHPAD_GESTURE_MASK, /* GDK_TOUCHPAD_SWIPE = 41 */
  GDK_TOUCHPAD_GESTURE_MASK, /* GDK_TOUCHPAD_PINCH = 42 */
  GDK_TABLET_PAD_MASK, /* GDK_PAD_BUTTON_PRESS  = 43 */
  GDK_TABLET_PAD_MASK, /* GDK_PAD_BUTTON_RELEASE = 44 */
  GDK_TABLET_PAD_MASK, /* GDK_PAD_RING = 45 */
  GDK_TABLET_PAD_MASK, /* GDK_PAD_STRIP = 46 */
  GDK_TABLET_PAD_MASK, /* GDK_PAD_GROUP_MODE = 47 */
};
G_STATIC_ASSERT (G_N_ELEMENTS (type_masks) == GDK_EVENT_LAST);

/* send motion events if the right buttons are down */
static guint
update_evmask_for_button_motion (guint           evmask,
				 GdkModifierType mask)
{
  if (evmask & GDK_BUTTON_MOTION_MASK &&
      mask & (GDK_BUTTON1_MASK |
	      GDK_BUTTON2_MASK |
	      GDK_BUTTON3_MASK |
	      GDK_BUTTON4_MASK |
	      GDK_BUTTON5_MASK))
    evmask |= GDK_POINTER_MOTION_MASK;

  if ((evmask & GDK_BUTTON1_MOTION_MASK && mask & GDK_BUTTON1_MASK) ||
      (evmask & GDK_BUTTON2_MOTION_MASK && mask & GDK_BUTTON2_MASK) ||
      (evmask & GDK_BUTTON3_MOTION_MASK && mask & GDK_BUTTON3_MASK))
    evmask |= GDK_POINTER_MOTION_MASK;

  return evmask;
}

static gboolean
is_button_type (GdkEventType type)
{
  return type == GDK_BUTTON_PRESS ||
	 type == GDK_2BUTTON_PRESS ||
	 type == GDK_3BUTTON_PRESS ||
	 type == GDK_BUTTON_RELEASE ||
         type == GDK_TOUCH_BEGIN ||
         type == GDK_TOUCH_END ||
         type == GDK_TOUCH_CANCEL ||
	 type == GDK_SCROLL;
}

static gboolean
is_gesture_type (GdkEventType type)
{
  return (type == GDK_TOUCHPAD_SWIPE ||
          type == GDK_TOUCHPAD_PINCH);
}

static gboolean
is_motion_type (GdkEventType type)
{
  return type == GDK_MOTION_NOTIFY ||
         type == GDK_TOUCH_UPDATE ||
	 type == GDK_ENTER_NOTIFY ||
	 type == GDK_LEAVE_NOTIFY;
}

static gboolean
is_touch_type (GdkEventType type)
{
  return type == GDK_TOUCH_BEGIN ||
         type == GDK_TOUCH_UPDATE ||
         type == GDK_TOUCH_END ||
         type == GDK_TOUCH_CANCEL;
}

static GdkWindow *
find_common_ancestor (GdkWindow *win1,
		      GdkWindow *win2)
{
  GdkWindow *tmp;
  GList *path1 = NULL, *path2 = NULL;
  GList *list1, *list2;

  tmp = win1;
  while (tmp != NULL && tmp->window_type != GDK_WINDOW_ROOT)
    {
      path1 = g_list_prepend (path1, tmp);
      tmp = tmp->parent;
    }

  tmp = win2;
  while (tmp != NULL && tmp->window_type != GDK_WINDOW_ROOT)
    {
      path2 = g_list_prepend (path2, tmp);
      tmp = tmp->parent;
    }

  list1 = path1;
  list2 = path2;
  tmp = NULL;
  while (list1 && list2 && (list1->data == list2->data))
    {
      tmp = list1->data;
      list1 = list1->next;
      list2 = list2->next;
    }
  g_list_free (path1);
  g_list_free (path2);

  return tmp;
}

GdkEvent *
_gdk_make_event (GdkWindow    *window,
		 GdkEventType  type,
		 GdkEvent     *event_in_queue,
		 gboolean      before_event)
{
  GdkEvent *event = gdk_event_new (type);
  guint32 the_time;
  GdkModifierType the_state;

  the_time = gdk_event_get_time (event_in_queue);
  gdk_event_get_state (event_in_queue, &the_state);

  event->any.window = g_object_ref (window);
  event->any.send_event = FALSE;
  if (event_in_queue && event_in_queue->any.send_event)
    event->any.send_event = TRUE;

  switch (type)
    {
    case GDK_MOTION_NOTIFY:
      event->motion.time = the_time;
      event->motion.axes = NULL;
      event->motion.state = the_state;
      break;

    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      event->button.time = the_time;
      event->button.axes = NULL;
      event->button.state = the_state;
      break;

    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      event->touch.time = the_time;
      event->touch.axes = NULL;
      event->touch.state = the_state;
      break;

    case GDK_SCROLL:
      event->scroll.time = the_time;
      event->scroll.state = the_state;
      break;

    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      event->key.time = the_time;
      event->key.state = the_state;
      break;

    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      event->crossing.time = the_time;
      event->crossing.state = the_state;
      break;

    case GDK_PROPERTY_NOTIFY:
      event->property.time = the_time;
      event->property.state = the_state;
      break;

    case GDK_SELECTION_CLEAR:
    case GDK_SELECTION_REQUEST:
    case GDK_SELECTION_NOTIFY:
      event->selection.time = the_time;
      break;

    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
      event->proximity.time = the_time;
      break;

    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
      event->dnd.time = the_time;
      break;

    case GDK_TOUCHPAD_SWIPE:
      event->touchpad_swipe.time = the_time;
      event->touchpad_swipe.state = the_state;
      break;

    case GDK_TOUCHPAD_PINCH:
      event->touchpad_pinch.time = the_time;
      event->touchpad_pinch.state = the_state;
      break;

    case GDK_FOCUS_CHANGE:
    case GDK_CONFIGURE:
    case GDK_MAP:
    case GDK_UNMAP:
    case GDK_CLIENT_EVENT:
    case GDK_VISIBILITY_NOTIFY:
    case GDK_DELETE:
    case GDK_DESTROY:
    case GDK_EXPOSE:
    default:
      break;
    }

  if (event_in_queue)
    {
    if (before_event)
      _gdk_event_queue_insert_before (gdk_window_get_display (window), event_in_queue, event);
    else
      _gdk_event_queue_insert_after (gdk_window_get_display (window), event_in_queue, event);
    }
  else
    _gdk_event_queue_append (gdk_window_get_display (window), event);

  return event;
}

static void
send_crossing_event (GdkDisplay                 *display,
		     GdkWindow                  *toplevel,
		     GdkWindow                  *window,
		     GdkEventType                type,
		     GdkCrossingMode             mode,
		     GdkNotifyType               notify_type,
		     GdkWindow                  *subwindow,
                     GdkDevice                  *device,
                     GdkDevice                  *source_device,
		     gdouble                     toplevel_x,
		     gdouble                     toplevel_y,
		     GdkModifierType             mask,
		     guint32                     time_,
		     GdkEvent                   *event_in_queue,
		     gulong                      serial)
{
  GdkEvent *event;
  guint32 window_event_mask, type_event_mask;
  GdkDeviceGrabInfo *grab;
  GdkTouchGrabInfo *touch_grab = NULL;
  GdkPointerWindowInfo *pointer_info;
  gboolean block_event = FALSE;
  GdkEventSequence *sequence;

  grab = _gdk_display_has_device_grab (display, device, serial);
  pointer_info = _gdk_display_get_pointer_info (display, device);

  sequence = gdk_event_get_event_sequence (event_in_queue);
  if (sequence)
    touch_grab = _gdk_display_has_touch_grab (display, device, sequence, serial);

  if (touch_grab)
    {
      if (window != touch_grab->window)
        return;

      window_event_mask = touch_grab->event_mask;
    }
  else if (grab != NULL &&
           !grab->owner_events)
    {
      /* !owner_event => only report events wrt grab window, ignore rest */
      if ((GdkWindow *)window != grab->window)
	return;
      window_event_mask = grab->event_mask;
    }
  else
    window_event_mask = window->event_mask;

  if (type == GDK_ENTER_NOTIFY &&
      (pointer_info->need_touch_press_enter ||
       (source_device &&
        gdk_device_get_source (source_device) == GDK_SOURCE_TOUCHSCREEN)) &&
      mode != GDK_CROSSING_TOUCH_BEGIN &&
      mode != GDK_CROSSING_TOUCH_END)
    {
      pointer_info->need_touch_press_enter = TRUE;
      block_event = TRUE;
    }
  else if (type == GDK_LEAVE_NOTIFY)
    {
      type_event_mask = GDK_LEAVE_NOTIFY_MASK;
      window->devices_inside = g_list_remove (window->devices_inside, device);

      if (!window->support_multidevice && window->devices_inside)
        {
          /* Block leave events unless it's the last pointer */
          block_event = TRUE;
        }
    }
  else
    {
      type_event_mask = GDK_ENTER_NOTIFY_MASK;

      if (!window->support_multidevice && window->devices_inside)
        {
          /* Only emit enter events for the first device */
          block_event = TRUE;
        }

      if (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_MASTER &&
          gdk_device_get_mode (device) != GDK_MODE_DISABLED &&
          !g_list_find (window->devices_inside, device))
        window->devices_inside = g_list_prepend (window->devices_inside, device);
    }

  if (block_event)
    return;

  if (window_event_mask & type_event_mask)
    {
      event = _gdk_make_event ((GdkWindow *)window, type, event_in_queue, TRUE);
      gdk_event_set_device (event, device);
      gdk_event_set_seat (event, gdk_device_get_seat (device));

      if (source_device)
        gdk_event_set_source_device (event, source_device);

      event->crossing.time = time_;
      event->crossing.subwindow = subwindow;
      if (subwindow)
	g_object_ref (subwindow);
      convert_toplevel_coords_to_window ((GdkWindow *)window,
					 toplevel_x, toplevel_y,
					 &event->crossing.x, &event->crossing.y);
      event->crossing.x_root = toplevel_x + toplevel->x;
      event->crossing.y_root = toplevel_y + toplevel->y;
      event->crossing.mode = mode;
      event->crossing.detail = notify_type;
      event->crossing.focus = FALSE;
      event->crossing.state = mask;
    }
}


/* The coordinates are in the toplevel window that src/dest are in.
 * src and dest are always (if != NULL) in the same toplevel, as
 * we get a leave-notify and set the window_under_pointer to null
 * before crossing to another toplevel.
 */
void
_gdk_synthesize_crossing_events (GdkDisplay                 *display,
				 GdkWindow                  *src,
				 GdkWindow                  *dest,
                                 GdkDevice                  *device,
                                 GdkDevice                  *source_device,
				 GdkCrossingMode             mode,
				 double                      toplevel_x,
				 double                      toplevel_y,
				 GdkModifierType             mask,
				 guint32                     time_,
				 GdkEvent                   *event_in_queue,
				 gulong                      serial,
				 gboolean                    non_linear)
{
  GdkWindow *c;
  GdkWindow *win, *last, *next;
  GList *path, *list;
  GdkWindow *a;
  GdkWindow *b;
  GdkWindow *toplevel;
  GdkNotifyType notify_type;

  /* TODO: Don't send events to toplevel, as we get those from the windowing system */

  a = (src && GDK_IS_WINDOW (src)) ? src : NULL;
  b = (dest && GDK_IS_WINDOW (dest)) ? dest : NULL;

  if (src == dest)
    return; /* No crossings generated between src and dest */

  if (gdk_device_get_device_type (device) != GDK_DEVICE_TYPE_MASTER)
    {
      if (a && gdk_window_get_device_events (src, device) == 0)
        a = NULL;

      if (b && gdk_window_get_device_events (dest, device) == 0)
        b = NULL;
    }

  if (!a && !b)
    return;

  c = find_common_ancestor (a, b);

  non_linear |= (c != a) && (c != b);

  if (a) /* There might not be a source (i.e. if no previous pointer_in_window) */
    {
      toplevel = gdk_window_get_toplevel (a);

      /* Traverse up from a to (excluding) c sending leave events */
      if (non_linear)
	notify_type = GDK_NOTIFY_NONLINEAR;
      else if (c == a)
	notify_type = GDK_NOTIFY_INFERIOR;
      else
	notify_type = GDK_NOTIFY_ANCESTOR;
      send_crossing_event (display, toplevel,
			   a, GDK_LEAVE_NOTIFY,
			   mode,
			   notify_type,
			   NULL, device, source_device,
			   toplevel_x, toplevel_y,
			   mask, time_,
			   event_in_queue,
			   serial);

      if (c != a)
	{
	  if (non_linear)
	    notify_type = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	  else
	    notify_type = GDK_NOTIFY_VIRTUAL;

	  last = a;
	  win = a->parent;
	  while (win != c && win->window_type != GDK_WINDOW_ROOT)
	    {
	      send_crossing_event (display, toplevel,
				   win, GDK_LEAVE_NOTIFY,
				   mode,
				   notify_type,
				   (GdkWindow *)last,
				   device, source_device,
				   toplevel_x, toplevel_y,
				   mask, time_,
				   event_in_queue,
				   serial);

	      last = win;
	      win = win->parent;
	    }
	}
    }

  if (b) /* Might not be a dest, e.g. if we're moving out of the window */
    {
      toplevel = gdk_window_get_toplevel ((GdkWindow *)b);

      /* Traverse down from c to b */
      if (c != b)
	{
	  path = NULL;
	  win = b->parent;
	  while (win != c && win->window_type != GDK_WINDOW_ROOT)
	    {
	      path = g_list_prepend (path, win);
	      win = win->parent;
	    }

	  if (non_linear)
	    notify_type = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	  else
	    notify_type = GDK_NOTIFY_VIRTUAL;

	  list = path;
	  while (list)
	    {
	      win = list->data;
	      list = list->next;
	      if (list)
		next = list->data;
	      else
		next = b;

	      send_crossing_event (display, toplevel,
				   win, GDK_ENTER_NOTIFY,
				   mode,
				   notify_type,
				   (GdkWindow *)next,
				   device, source_device,
				   toplevel_x, toplevel_y,
				   mask, time_,
				   event_in_queue,
				   serial);
	    }
	  g_list_free (path);
	}


      if (non_linear)
	notify_type = GDK_NOTIFY_NONLINEAR;
      else if (c == a)
	notify_type = GDK_NOTIFY_ANCESTOR;
      else
	notify_type = GDK_NOTIFY_INFERIOR;

      send_crossing_event (display, toplevel,
			   b, GDK_ENTER_NOTIFY,
			   mode,
			   notify_type,
			   NULL,
                           device, source_device,
			   toplevel_x, toplevel_y,
			   mask, time_,
			   event_in_queue,
			   serial);
    }
}

/* Returns the window inside the event window with the pointer in it
 * at the specified coordinates, or NULL if its not in any child of
 * the toplevel. It also takes into account !owner_events grabs.
 */
static GdkWindow *
get_pointer_window (GdkDisplay *display,
		    GdkWindow *event_window,
                    GdkDevice *device,
		    gdouble toplevel_x,
		    gdouble toplevel_y,
		    gulong serial)
{
  GdkWindow *pointer_window;
  GdkDeviceGrabInfo *grab;
  GdkPointerWindowInfo *pointer_info;

  pointer_info = _gdk_display_get_pointer_info (display, device);

  if (event_window == pointer_info->toplevel_under_pointer)
    pointer_window =
      _gdk_window_find_descendant_at (event_window,
				      toplevel_x, toplevel_y,
				      NULL, NULL);
  else
    pointer_window = NULL;

  grab = _gdk_display_has_device_grab (display, device, serial);
  if (grab != NULL &&
      !grab->owner_events &&
      pointer_window != grab->window &&
      !gdk_window_is_ancestor (pointer_window, grab->window))
    pointer_window = NULL;

  return pointer_window;
}

void
_gdk_display_set_window_under_pointer (GdkDisplay *display,
                                       GdkDevice  *device,
				       GdkWindow  *window)
{
  GdkPointerWindowInfo *device_info;

  device_info = _gdk_display_get_pointer_info (display, device);

  if (device_info->window_under_pointer)
    g_object_unref (device_info->window_under_pointer);
  device_info->window_under_pointer = window;

  if (window)
    {
      g_object_ref (window);
      update_cursor (display, device);
    }

  _gdk_display_enable_motion_hints (display, device);
}

static void
source_events_device_added (GdkDeviceManager *device_manager,
                            GdkDevice        *device,
                            gpointer          user_data)
{
  GdkWindow *window;
  GdkEventMask event_mask;
  GdkInputSource source;

  if (gdk_device_get_device_type (device) != GDK_DEVICE_TYPE_FLOATING)
    return;

  window = user_data;
  source = gdk_device_get_source (device);

  event_mask = GPOINTER_TO_INT (g_hash_table_lookup (window->source_event_masks,
                                                     GINT_TO_POINTER (source)));
  if (event_mask)
    gdk_window_set_device_events (window, device, event_mask);
}

static void
source_events_device_changed (GdkDeviceManager *device_manager,
                              GdkDevice        *device,
                              gpointer          user_data)
{
  GdkDeviceType type;
  GdkInputSource source;
  GdkEventMask event_mask;
  GdkWindow *window;

  window = user_data;
  type = gdk_device_get_device_type (device);
  source = gdk_device_get_source (device);

  event_mask = GPOINTER_TO_INT (g_hash_table_lookup (window->source_event_masks,
                                                     GINT_TO_POINTER (source)));

  if (!event_mask)
    return;

  if (type == GDK_DEVICE_TYPE_FLOATING)
    {
      /* The device was just floated, enable its event mask */
      gdk_window_set_device_events (window, device, event_mask);
    }
  else if (type == GDK_DEVICE_TYPE_SLAVE)
    gdk_window_set_device_events (window, device, 0);
}

/**
 * gdk_window_set_source_events:
 * @window: a #GdkWindow
 * @source: a #GdkInputSource to define the source class.
 * @event_mask: event mask for @window
 *
 * Sets the event mask for any floating device (i.e. not attached to any
 * visible pointer) that has the source defined as @source. This event
 * mask will be applied both to currently existing, newly added devices
 * after this call, and devices being attached/detached.
 *
 * Since: 3.0
 **/
void
gdk_window_set_source_events (GdkWindow      *window,
                              GdkInputSource  source,
                              GdkEventMask    event_mask)
{
  GdkDeviceManager *device_manager;
  GdkDisplay *display;
  GList *devices, *d;
  guint size;

  g_return_if_fail (GDK_IS_WINDOW (window));

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  display = gdk_window_get_display (window);
  device_manager = gdk_display_get_device_manager (display);

  devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_FLOATING);

  /* Set event mask for existing devices */
  for (d = devices; d; d = d->next)
    {
      GdkDevice *device = d->data;

      if (source == gdk_device_get_source (device))
        gdk_window_set_device_events (window, device, event_mask);
    }

  g_list_free (devices);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  /* Update accounting */
  if (G_UNLIKELY (!window->source_event_masks))
    window->source_event_masks = g_hash_table_new (NULL, NULL);

  if (event_mask)
    g_hash_table_insert (window->source_event_masks,
                         GUINT_TO_POINTER (source),
                         GUINT_TO_POINTER (event_mask));
  else
    g_hash_table_remove (window->source_event_masks,
                         GUINT_TO_POINTER (source));

  size = g_hash_table_size (window->source_event_masks);

  /* Update handler if needed */
  if (!window->device_added_handler_id && size > 0)
    {
      window->device_added_handler_id =
        g_signal_connect (device_manager, "device-added",
                          G_CALLBACK (source_events_device_added), window);
      window->device_changed_handler_id =
        g_signal_connect (device_manager, "device-changed",
                          G_CALLBACK (source_events_device_changed), window);
    }
  else if (window->device_added_handler_id && size == 0)
    g_signal_handler_disconnect (device_manager, window->device_added_handler_id);
}

/**
 * gdk_window_get_source_events:
 * @window: a #GdkWindow
 * @source: a #GdkInputSource to define the source class.
 *
 * Returns the event mask for @window corresponding to the device class specified
 * by @source.
 *
 * Returns: source event mask for @window
 **/
GdkEventMask
gdk_window_get_source_events (GdkWindow      *window,
                              GdkInputSource  source)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  return GPOINTER_TO_UINT (g_hash_table_lookup (window->source_event_masks,
                                                GUINT_TO_POINTER (source)));
}

static gboolean
do_synthesize_crossing_event (gpointer data)
{
  GdkDisplay *display;
  GdkWindow *changed_toplevel;
  GHashTableIter iter;
  gpointer key, value;
  gulong serial;

  changed_toplevel = data;

  changed_toplevel->synthesize_crossing_event_queued = FALSE;

  if (GDK_WINDOW_DESTROYED (changed_toplevel))
    return FALSE;

  display = gdk_window_get_display (changed_toplevel);
  serial = _gdk_display_get_next_serial (display);
  g_hash_table_iter_init (&iter, display->pointers_info);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GdkWindow *new_window_under_pointer;
      GdkPointerWindowInfo *pointer_info = value;
      GdkDevice *device = key;

      if (changed_toplevel == pointer_info->toplevel_under_pointer)
        {
          new_window_under_pointer =
            get_pointer_window (display, changed_toplevel,
                                device,
                                pointer_info->toplevel_x,
                                pointer_info->toplevel_y,
                                serial);
          if (new_window_under_pointer != pointer_info->window_under_pointer)
            {
              GdkDevice *source_device;

              if (pointer_info->last_slave)
                source_device = pointer_info->last_slave;
              else
                source_device = device;

              _gdk_synthesize_crossing_events (display,
                                               pointer_info->window_under_pointer,
                                               new_window_under_pointer,
                                               device, source_device,
                                               GDK_CROSSING_NORMAL,
                                               pointer_info->toplevel_x,
                                               pointer_info->toplevel_y,
                                               pointer_info->state,
                                               GDK_CURRENT_TIME,
                                               NULL,
                                               serial,
                                               FALSE);
              _gdk_display_set_window_under_pointer (display, device, new_window_under_pointer);
            }
        }
    }

  return FALSE;
}

void
_gdk_synthesize_crossing_events_for_geometry_change (GdkWindow *changed_window)
{
  GdkWindow *toplevel;

  toplevel = get_event_toplevel (changed_window);

  if (!toplevel->synthesize_crossing_event_queued)
    {
      guint id;

      toplevel->synthesize_crossing_event_queued = TRUE;

      id = gdk_threads_add_idle_full (GDK_PRIORITY_EVENTS - 1,
                                      do_synthesize_crossing_event,
                                      g_object_ref (toplevel),
                                      g_object_unref);
      g_source_set_name_by_id (id, "[gtk+] do_synthesize_crossing_event");
    }
}

/* Don't use for crossing events */
static GdkWindow *
get_event_window (GdkDisplay                 *display,
                  GdkDevice                  *device,
                  GdkEventSequence           *sequence,
                  GdkWindow                  *pointer_window,
                  GdkEventType                type,
                  GdkModifierType             mask,
                  guint                      *evmask_out,
                  gboolean                    pointer_emulated,
                  gulong                      serial)
{
  guint evmask, emulated_mask = 0;
  GdkWindow *grab_window;
  GdkDeviceGrabInfo *grab;
  GdkTouchGrabInfo *touch_grab;

  touch_grab = _gdk_display_has_touch_grab (display, device, sequence, serial);
  grab = _gdk_display_get_last_device_grab (display, device);

  /* Default value. */
  if (evmask_out)
    *evmask_out = 0;

  if (is_touch_type (type) && pointer_emulated)
    {
      switch (type)
        {
        case GDK_TOUCH_BEGIN:
          emulated_mask |= GDK_BUTTON_PRESS_MASK;
          break;
        case GDK_TOUCH_UPDATE:
          emulated_mask |= GDK_POINTER_MOTION_MASK;
          break;
        case GDK_TOUCH_END:
          emulated_mask |= GDK_BUTTON_RELEASE_MASK;
        default:
          break;
        }
    }

  if (touch_grab != NULL &&
      (!grab || grab->implicit || touch_grab->serial >= grab->serial_start))
    {
      evmask = touch_grab->event_mask;
      evmask = update_evmask_for_button_motion (evmask, mask);

      if (evmask & (type_masks[type] | emulated_mask))
        {
          if (evmask_out)
            *evmask_out = evmask;
          return touch_grab->window;
        }
      else
        return NULL;
    }

  if (grab != NULL && !grab->owner_events)
    {
      evmask = grab->event_mask;
      evmask = update_evmask_for_button_motion (evmask, mask);

      grab_window = grab->window;

      if (evmask & (type_masks[type] | emulated_mask))
	{
	  if (evmask_out)
	    *evmask_out = evmask;
	  return grab_window;
	}
      else
	return NULL;
    }

  while (pointer_window != NULL)
    {
      evmask = pointer_window->event_mask;
      evmask = update_evmask_for_button_motion (evmask, mask);

      if (evmask & (type_masks[type] | emulated_mask))
	{
	  if (evmask_out)
	    *evmask_out = evmask;
	  return pointer_window;
	}

      pointer_window = pointer_window->parent;
    }

  if (grab != NULL &&
      grab->owner_events)
    {
      evmask = grab->event_mask;
      evmask = update_evmask_for_button_motion (evmask, mask);

      if (evmask & (type_masks[type] | emulated_mask))
	{
	  if (evmask_out)
	    *evmask_out = evmask;
	  return grab->window;
	}
      else
	return NULL;
    }

  return NULL;
}

static gboolean
proxy_pointer_event (GdkDisplay                 *display,
		     GdkEvent                   *source_event,
		     gulong                      serial)
{
  GdkWindow *toplevel_window, *event_window;
  GdkWindow *pointer_window;
  GdkPointerWindowInfo *pointer_info;
  GdkDevice *device, *source_device;
  GdkEvent *event;
  guint state;
  gdouble toplevel_x, toplevel_y;
  guint32 time_;
  gboolean non_linear, need_synthetic_enter = FALSE;
  gint event_type;

  event_type = source_event->type;
  event_window = source_event->any.window;
  gdk_event_get_coords (source_event, &toplevel_x, &toplevel_y);
  gdk_event_get_state (source_event, &state);
  time_ = gdk_event_get_time (source_event);
  device = gdk_event_get_device (source_event);
  source_device = gdk_event_get_source_device (source_event);
  pointer_info = _gdk_display_get_pointer_info (display, device);
  toplevel_window = convert_native_coords_to_toplevel (event_window,
						       toplevel_x, toplevel_y,
						       &toplevel_x, &toplevel_y);

  non_linear = FALSE;
  if ((source_event->type == GDK_LEAVE_NOTIFY ||
       source_event->type == GDK_ENTER_NOTIFY) &&
      (source_event->crossing.detail == GDK_NOTIFY_NONLINEAR ||
       source_event->crossing.detail == GDK_NOTIFY_NONLINEAR_VIRTUAL))
    non_linear = TRUE;

  if (pointer_info->need_touch_press_enter &&
      gdk_device_get_source (pointer_info->last_slave) != GDK_SOURCE_TOUCHSCREEN &&
      (source_event->type != GDK_TOUCH_UPDATE ||
       gdk_event_get_pointer_emulated (source_event)))
    {
      pointer_info->need_touch_press_enter = FALSE;
      need_synthetic_enter = TRUE;
    }

  /* If we get crossing events with subwindow unexpectedly being NULL
     that means there is a native subwindow that gdk doesn't know about.
     We track these and forward them, with the correct virtual window
     events inbetween.
     This is important to get right, as metacity uses gdk for the frame
     windows, but gdk doesn't know about the client windows reparented
     into the frame. */
  if (((source_event->type == GDK_LEAVE_NOTIFY &&
	source_event->crossing.detail == GDK_NOTIFY_INFERIOR) ||
       (source_event->type == GDK_ENTER_NOTIFY &&
	(source_event->crossing.detail == GDK_NOTIFY_VIRTUAL ||
	 source_event->crossing.detail == GDK_NOTIFY_NONLINEAR_VIRTUAL))) &&
      source_event->crossing.subwindow == NULL)
    {
      /* Left for an unknown (to gdk) subwindow */

      /* Send leave events from window under pointer to event window
	 that will get the subwindow == NULL window */
      _gdk_synthesize_crossing_events (display,
				       pointer_info->window_under_pointer,
				       event_window,
                                       device, source_device,
				       source_event->crossing.mode,
				       toplevel_x, toplevel_y,
				       state, time_,
				       source_event,
				       serial,
				       non_linear);

      /* Send subwindow == NULL event */
      send_crossing_event (display,
			   toplevel_window,
			   event_window,
			   source_event->type,
			   source_event->crossing.mode,
			   source_event->crossing.detail,
			   NULL,
                           device, source_device,
			   toplevel_x, toplevel_y,
			   state, time_,
			   source_event,
			   serial);

      _gdk_display_set_window_under_pointer (display, device, NULL);
      return TRUE;
    }

  pointer_window = get_pointer_window (display, toplevel_window, device,
				       toplevel_x, toplevel_y, serial);

  if (((source_event->type == GDK_ENTER_NOTIFY &&
	source_event->crossing.detail == GDK_NOTIFY_INFERIOR) ||
       (source_event->type == GDK_LEAVE_NOTIFY &&
	(source_event->crossing.detail == GDK_NOTIFY_VIRTUAL ||
	 source_event->crossing.detail == GDK_NOTIFY_NONLINEAR_VIRTUAL))) &&
      source_event->crossing.subwindow == NULL)
    {
      /* Entered from an unknown (to gdk) subwindow */

      /* Send subwindow == NULL event */
      send_crossing_event (display,
			   toplevel_window,
			   event_window,
			   source_event->type,
			   source_event->crossing.mode,
			   source_event->crossing.detail,
			   NULL,
                           device, source_device,
			   toplevel_x, toplevel_y,
			   state, time_,
			   source_event,
			   serial);

      /* Send enter events from event window to pointer_window */
      _gdk_synthesize_crossing_events (display,
				       event_window,
				       pointer_window,
                                       device, source_device,
				       source_event->crossing.mode,
				       toplevel_x, toplevel_y,
				       state, time_,
				       source_event,
				       serial, non_linear);
      _gdk_display_set_window_under_pointer (display, device, pointer_window);
      return TRUE;
    }

  if ((source_event->type != GDK_TOUCH_UPDATE ||
       gdk_event_get_pointer_emulated (source_event)) &&
      pointer_info->window_under_pointer != pointer_window)
    {
      /* Either a toplevel crossing notify that ended up inside a child window,
	 or a motion notify that got into another child window  */

      /* Different than last time, send crossing events */
      _gdk_synthesize_crossing_events (display,
				       pointer_info->window_under_pointer,
				       pointer_window,
                                       device, source_device,
				       GDK_CROSSING_NORMAL,
				       toplevel_x, toplevel_y,
				       state, time_,
				       source_event,
				       serial, non_linear);
      _gdk_display_set_window_under_pointer (display, device, pointer_window);
    }
  else if (source_event->type == GDK_MOTION_NOTIFY ||
           source_event->type == GDK_TOUCH_UPDATE)
    {
      GdkWindow *event_win;
      guint evmask;
      gboolean is_hint;
      GdkEventSequence *sequence;

      sequence = gdk_event_get_event_sequence (source_event);

      event_win = get_event_window (display,
                                    device,
                                    sequence,
                                    pointer_window,
                                    source_event->type,
                                    state,
                                    &evmask,
                                    gdk_event_get_pointer_emulated (source_event),
                                    serial);

      if (event_type == GDK_TOUCH_UPDATE)
        {
          if (gdk_event_get_pointer_emulated (source_event))
            {
              /* Touch events emulating pointer events are transformed back
               * to pointer events if:
               * 1 - The event window doesn't select for touch events
               * 2 - There's no touch grab for this sequence, which means
               *     it was started as a pointer sequence, but a device
               *     grab added touch events afterwards, the sequence must
               *     not mutate in this case.
               */
              if ((evmask & GDK_TOUCH_MASK) == 0 ||
                  !_gdk_display_has_touch_grab (display, device, sequence, serial))
                event_type = GDK_MOTION_NOTIFY;
            }
          else if ((evmask & GDK_TOUCH_MASK) == 0)
            return TRUE;
        }

      if (is_touch_type (source_event->type) && !is_touch_type (event_type))
        state |= GDK_BUTTON1_MASK;

      if (event_win &&
          gdk_device_get_device_type (device) != GDK_DEVICE_TYPE_MASTER &&
          gdk_window_get_device_events (event_win, device) == 0)
        return TRUE;

      /* The last device to interact with the window was a touch device,
       * which synthesized a leave notify event, so synthesize another enter
       * notify to tell the pointer is on the window.
       */
      if (need_synthetic_enter)
        _gdk_synthesize_crossing_events (display,
                                         NULL, pointer_window,
                                         device, source_device,
                                         GDK_CROSSING_DEVICE_SWITCH,
                                         toplevel_x, toplevel_y,
                                         state, time_, NULL,
                                         serial, FALSE);

      is_hint = FALSE;

      if (event_win &&
          event_type == GDK_MOTION_NOTIFY &&
	  (evmask & GDK_POINTER_MOTION_HINT_MASK))
	{
          gulong *device_serial;

          device_serial = g_hash_table_lookup (display->motion_hint_info, device);

          if (!device_serial ||
              (*device_serial != 0 &&
               serial < *device_serial))
	    event_win = NULL; /* Ignore event */
	  else
	    {
	      is_hint = TRUE;
              *device_serial = G_MAXULONG;
	    }
	}

      if (!event_win)
        return TRUE;

      event = gdk_event_new (event_type);
      event->any.window = g_object_ref (event_win);
      event->any.send_event = source_event->any.send_event;

      gdk_event_set_device (event, gdk_event_get_device (source_event));
      gdk_event_set_source_device (event, source_device);
      gdk_event_set_seat (event, gdk_device_get_seat (device));
      gdk_event_set_device_tool (event, gdk_event_get_device_tool (source_event));

      if (event_type == GDK_TOUCH_UPDATE)
	{
	  event->touch.time = time_;
	  event->touch.state = state | GDK_BUTTON1_MASK;
	  event->touch.sequence = source_event->touch.sequence;
	  event->touch.emulating_pointer = source_event->touch.emulating_pointer;
	  convert_toplevel_coords_to_window (event_win,
					     toplevel_x, toplevel_y,
					     &event->touch.x, &event->touch.y);
	  gdk_event_get_root_coords (source_event,
				     &event->touch.x_root,
				     &event->touch.y_root);

	  event->touch.axes = g_memdup (source_event->touch.axes,
					sizeof (gdouble) * gdk_device_get_n_axes (source_event->touch.device));
	}
      else
	{
	  event->motion.time = time_;
	  event->motion.state = state;
	  event->motion.is_hint = is_hint;

	  convert_toplevel_coords_to_window (event_win,
					     toplevel_x, toplevel_y,
					     &event->motion.x, &event->motion.y);
	  gdk_event_get_root_coords (source_event,
				     &event->motion.x_root,
				     &event->motion.y_root);

	  if (is_touch_type (source_event->type))
	    event->motion.axes = g_memdup (source_event->touch.axes,
					   sizeof (gdouble) * gdk_device_get_n_axes (source_event->touch.device));
	  else
	    event->motion.axes = g_memdup (source_event->motion.axes,
					   sizeof (gdouble) * gdk_device_get_n_axes (source_event->motion.device));
	}

      /* Just insert the event */
      _gdk_event_queue_insert_after (gdk_window_get_display (event_win),
				     source_event, event);
    }

  /* unlink all move events from queue.
     We handle our own, including our emulated masks. */
  return TRUE;
}

#define GDK_ANY_BUTTON_MASK (GDK_BUTTON1_MASK | \
			     GDK_BUTTON2_MASK | \
			     GDK_BUTTON3_MASK | \
			     GDK_BUTTON4_MASK | \
			     GDK_BUTTON5_MASK)

static gboolean
proxy_button_event (GdkEvent *source_event,
		    gulong serial)
{
  GdkWindow *toplevel_window, *event_window;
  GdkWindow *event_win;
  GdkWindow *pointer_window;
  GdkWindow *parent;
  GdkEvent *event;
  GdkPointerWindowInfo *pointer_info;
  GdkDeviceGrabInfo *pointer_grab;
  guint state;
  guint32 time_;
  GdkEventType type;
  gdouble toplevel_x, toplevel_y;
  GdkDisplay *display;
  GdkWindow *w;
  GdkDevice *device, *source_device;
  GdkEventMask evmask;
  GdkEventSequence *sequence;

  type = source_event->any.type;
  event_window = source_event->any.window;
  gdk_event_get_coords (source_event, &toplevel_x, &toplevel_y);
  gdk_event_get_state (source_event, &state);
  time_ = gdk_event_get_time (source_event);
  device = gdk_event_get_device (source_event);
  source_device = gdk_event_get_source_device (source_event);
  display = gdk_window_get_display (source_event->any.window);
  toplevel_window = convert_native_coords_to_toplevel (event_window,
						       toplevel_x, toplevel_y,
						       &toplevel_x, &toplevel_y);

  sequence = gdk_event_get_event_sequence (source_event);

  pointer_info = _gdk_display_get_pointer_info (display, device);
  pointer_grab = _gdk_display_has_device_grab (display, device, serial);

  if ((type == GDK_BUTTON_PRESS ||
       type == GDK_TOUCH_BEGIN) &&
      !source_event->any.send_event &&
      (!pointer_grab ||
       (type == GDK_TOUCH_BEGIN && pointer_grab->implicit &&
        !gdk_event_get_pointer_emulated (source_event))))
    {
      pointer_window =
	_gdk_window_find_descendant_at (toplevel_window,
					toplevel_x, toplevel_y,
					NULL, NULL);

      /* Find the event window, that gets the grab */
      w = pointer_window;
      while (w != NULL &&
	     (parent = w->parent) != NULL &&
	     parent->window_type != GDK_WINDOW_ROOT)
	{
	  if (w->event_mask & GDK_BUTTON_PRESS_MASK &&
              (type == GDK_BUTTON_PRESS ||
               gdk_event_get_pointer_emulated (source_event)))
	    break;

          if (type == GDK_TOUCH_BEGIN &&
              w->event_mask & GDK_TOUCH_MASK)
            break;

	  w = parent;
	}
      pointer_window = w;

      if (pointer_window)
        {
          if (type == GDK_TOUCH_BEGIN &&
              pointer_window->event_mask & GDK_TOUCH_MASK)
            {
              _gdk_display_add_touch_grab (display, device, sequence,
                                           pointer_window, event_window,
                                           gdk_window_get_events (pointer_window),
                                           serial, time_);
            }
          else if (type == GDK_BUTTON_PRESS ||
                   gdk_event_get_pointer_emulated (source_event))
            {
              _gdk_display_add_device_grab  (display,
                                             device,
                                             pointer_window,
                                             event_window,
                                             GDK_OWNERSHIP_NONE,
                                             FALSE,
                                             gdk_window_get_events (pointer_window),
                                             serial,
                                             time_,
                                             TRUE);
              _gdk_display_device_grab_update (display, device,
                                               source_device, serial);
            }
        }
    }

  pointer_window = get_pointer_window (display, toplevel_window, device,
				       toplevel_x, toplevel_y,
				       serial);

  event_win = get_event_window (display,
                                device,
                                sequence,
                                pointer_window,
                                type, state,
                                &evmask,
                                gdk_event_get_pointer_emulated (source_event),
                                serial);

  if (type == GDK_TOUCH_BEGIN || type == GDK_TOUCH_END)
    {
      if (gdk_event_get_pointer_emulated (source_event))
        {
          if ((evmask & GDK_TOUCH_MASK) == 0 ||
              !_gdk_display_has_touch_grab (display, device, sequence, serial))
            {
              if (type == GDK_TOUCH_BEGIN)
                type = GDK_BUTTON_PRESS;
              else if (type == GDK_TOUCH_END)
                type = GDK_BUTTON_RELEASE;
            }
        }
      else if ((evmask & GDK_TOUCH_MASK) == 0)
        return TRUE;
    }

  if (source_event->type == GDK_TOUCH_END && !is_touch_type (type))
    state |= GDK_BUTTON1_MASK;

  if (event_win == NULL)
    return TRUE;

  if (gdk_device_get_device_type (device) != GDK_DEVICE_TYPE_MASTER &&
      gdk_window_get_device_events (event_win, device) == 0)
    return TRUE;

  if ((type == GDK_BUTTON_PRESS ||
       (type == GDK_TOUCH_BEGIN &&
        gdk_event_get_pointer_emulated (source_event))) &&
      pointer_info->need_touch_press_enter)
    {
      GdkCrossingMode mode;

      /* The last device to interact with the window was a touch device,
       * which synthesized a leave notify event, so synthesize another enter
       * notify to tell the pointer is on the window.
       */
      if (gdk_device_get_source (source_device) == GDK_SOURCE_TOUCHSCREEN)
        mode = GDK_CROSSING_TOUCH_BEGIN;
      else
        mode = GDK_CROSSING_DEVICE_SWITCH;

      pointer_info->need_touch_press_enter = FALSE;
      _gdk_synthesize_crossing_events (display,
                                       NULL,
                                       pointer_info->window_under_pointer,
                                       device, source_device, mode,
                                       toplevel_x, toplevel_y,
                                       state, time_, source_event,
                                       serial, FALSE);
    }
  else if (type == GDK_SCROLL &&
           (((evmask & GDK_SMOOTH_SCROLL_MASK) == 0 &&
             source_event->scroll.direction == GDK_SCROLL_SMOOTH) ||
            ((evmask & GDK_SMOOTH_SCROLL_MASK) != 0 &&
             source_event->scroll.direction != GDK_SCROLL_SMOOTH &&
             gdk_event_get_pointer_emulated (source_event))))
    return FALSE;

  event = _gdk_make_event (event_win, type, source_event, FALSE);

  switch (type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      event->button.button = source_event->button.button;
      convert_toplevel_coords_to_window (event_win,
					 toplevel_x, toplevel_y,
					 &event->button.x, &event->button.y);
      gdk_event_get_root_coords (source_event,
				 &event->button.x_root,
				 &event->button.y_root);
      gdk_event_set_device (event, gdk_event_get_device (source_event));
      gdk_event_set_source_device (event, source_device);
      gdk_event_set_seat (event, gdk_device_get_seat (device));
      gdk_event_set_device_tool (event, gdk_event_get_device_tool (source_event));

      if (is_touch_type (source_event->type))
        {
          if (type == GDK_BUTTON_RELEASE)
            event->button.state |= GDK_BUTTON1_MASK;
	  event->button.button = 1;
	  event->button.axes = g_memdup (source_event->touch.axes,
					 sizeof (gdouble) * gdk_device_get_n_axes (source_event->touch.device));
	}
      else
	{
	  event->button.button = source_event->button.button;
	  event->button.axes = g_memdup (source_event->button.axes,
					 sizeof (gdouble) * gdk_device_get_n_axes (source_event->button.device));
	}

      if (type == GDK_BUTTON_PRESS)
        _gdk_event_button_generate (display, event);
      else if ((type == GDK_BUTTON_RELEASE ||
                (type == GDK_TOUCH_END &&
                 gdk_event_get_pointer_emulated (source_event))) &&
               pointer_window == pointer_info->window_under_pointer &&
               gdk_device_get_source (source_device) == GDK_SOURCE_TOUCHSCREEN)
        {
          /* Synthesize a leave notify event
           * whenever a touch device is released
           */
          pointer_info->need_touch_press_enter = TRUE;
          _gdk_synthesize_crossing_events (display,
                                           pointer_window, NULL,
                                           device, source_device,
                                           GDK_CROSSING_TOUCH_END,
                                           toplevel_x, toplevel_y,
                                           state, time_, NULL,
                                           serial, FALSE);
        }
      return TRUE;

    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      convert_toplevel_coords_to_window (event_win,
                                         toplevel_x, toplevel_y,
                                         &event->button.x, &event->button.y);
      gdk_event_get_root_coords (source_event,
				 &event->touch.x_root,
				 &event->touch.y_root);
      event->touch.state = state;
      event->touch.device = source_event->touch.device;
      event->touch.axes = g_memdup (source_event->touch.axes,
                                     sizeof (gdouble) * gdk_device_get_n_axes (source_event->touch.device));
      event->touch.sequence = source_event->touch.sequence;
      event->touch.emulating_pointer = source_event->touch.emulating_pointer;

      gdk_event_set_source_device (event, source_device);

      if (((type == GDK_TOUCH_END || type == GDK_TOUCH_CANCEL) &&
           gdk_event_get_pointer_emulated (source_event)) &&
           pointer_window == pointer_info->window_under_pointer &&
           gdk_device_get_source (source_device) == GDK_SOURCE_TOUCHSCREEN)
        {
          /* Synthesize a leave notify event
           * whenever a touch device is released
           */
          pointer_info->need_touch_press_enter = TRUE;
          _gdk_synthesize_crossing_events (display,
                                           pointer_window, NULL,
                                           device, source_device,
                                           GDK_CROSSING_TOUCH_END,
                                           toplevel_x, toplevel_y,
                                           state, time_, NULL,
                                           serial, FALSE);
        }
      return TRUE;

    case GDK_SCROLL:
      event->scroll.direction = source_event->scroll.direction;
      convert_toplevel_coords_to_window (event_win,
					 toplevel_x, toplevel_y,
					 &event->scroll.x, &event->scroll.y);
      event->scroll.x_root = source_event->scroll.x_root;
      event->scroll.y_root = source_event->scroll.y_root;
      event->scroll.state = state;
      event->scroll.device = source_event->scroll.device;
      event->scroll.delta_x = source_event->scroll.delta_x;
      event->scroll.delta_y = source_event->scroll.delta_y;
      event->scroll.is_stop = source_event->scroll.is_stop;
      gdk_event_set_source_device (event, source_device);
      return TRUE;

    default:
      return FALSE;
    }

  return TRUE; /* Always unlink original, we want to obey the emulated event mask */
}

static gboolean
proxy_gesture_event (GdkEvent *source_event,
                     gulong    serial)
{
  GdkWindow *toplevel_window, *pointer_window, *event_win;
  GdkDevice *device, *source_device;
  gdouble toplevel_x, toplevel_y;
  GdkDisplay *display;
  GdkEventMask evmask;
  GdkEventType evtype;
  GdkEvent *event;
  guint state;

  evtype = source_event->any.type;
  gdk_event_get_coords (source_event, &toplevel_x, &toplevel_y);
  gdk_event_get_state (source_event, &state);
  device = gdk_event_get_device (source_event);
  source_device = gdk_event_get_source_device (source_event);
  display = gdk_window_get_display (source_event->any.window);
  toplevel_window = convert_native_coords_to_toplevel (source_event->any.window,
						       toplevel_x, toplevel_y,
						       &toplevel_x, &toplevel_y);

  pointer_window = get_pointer_window (display, toplevel_window, device,
				       toplevel_x, toplevel_y,
				       serial);

  event_win = get_event_window (display, device, NULL,
                                pointer_window, evtype, state,
                                &evmask, FALSE, serial);
  if (!event_win)
    return TRUE;

  if ((evmask & GDK_TOUCHPAD_GESTURE_MASK) == 0)
    return TRUE;

  event = _gdk_make_event (event_win, evtype, source_event, FALSE);
  gdk_event_set_device (event, device);
  gdk_event_set_source_device (event, source_device);
  gdk_event_set_seat (event, gdk_device_get_seat (device));

  switch (evtype)
    {
    case GDK_TOUCHPAD_SWIPE:
      convert_toplevel_coords_to_window (event_win,
                                         toplevel_x, toplevel_y,
                                         &event->touchpad_swipe.x,
                                         &event->touchpad_swipe.y);
      gdk_event_get_root_coords (source_event,
				 &event->touchpad_swipe.x_root,
				 &event->touchpad_swipe.y_root);
      event->touchpad_swipe.dx = source_event->touchpad_swipe.dx;
      event->touchpad_swipe.dy = source_event->touchpad_swipe.dy;
      event->touchpad_swipe.n_fingers = source_event->touchpad_swipe.n_fingers;
      event->touchpad_swipe.phase = source_event->touchpad_swipe.phase;
      break;

    case GDK_TOUCHPAD_PINCH:
      convert_toplevel_coords_to_window (event_win,
                                         toplevel_x, toplevel_y,
                                         &event->touchpad_pinch.x,
                                         &event->touchpad_pinch.y);
      gdk_event_get_root_coords (source_event,
				 &event->touchpad_pinch.x_root,
				 &event->touchpad_pinch.y_root);
      event->touchpad_pinch.dx = source_event->touchpad_pinch.dx;
      event->touchpad_pinch.dy = source_event->touchpad_pinch.dy;
      event->touchpad_pinch.scale = source_event->touchpad_pinch.scale;
      event->touchpad_pinch.angle_delta = source_event->touchpad_pinch.angle_delta;
      event->touchpad_pinch.n_fingers = source_event->touchpad_pinch.n_fingers;
      event->touchpad_pinch.phase = source_event->touchpad_pinch.phase;
      break;

    default:
      break;
    }

  return TRUE;
}

#ifdef DEBUG_WINDOW_PRINTING

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif

static void
gdk_window_print (GdkWindow *window,
		  int indent)
{
  char *s;
  const char *window_types[] = {
    "root",
    "toplevel",
    "child",
    "dialog",
    "temp",
    "foreign",
    "subsurface"
  };

  g_print ("%*s%p: [%s] %d,%d %dx%d", indent, "", window,
	   window->user_data ? g_type_name_from_instance (window->user_data) : "no widget",
	   window->x, window->y,
	   window->width, window->height
	   );

  if (gdk_window_has_impl (window))
    {
#ifdef GDK_WINDOWING_X11
      g_print (" impl(0x%lx)", gdk_x11_window_get_xid (window));
#endif
    }

  if (window->window_type != GDK_WINDOW_CHILD)
    g_print (" %s", window_types[window->window_type]);

  if (window->input_only)
    g_print (" input-only");

  if (window->shaped)
    g_print (" shaped");

  if (!gdk_window_is_visible ((GdkWindow *)window))
    g_print (" hidden");

  g_print (" abs[%d,%d]",
	   window->abs_x, window->abs_y);

  if (window->alpha != 255)
    g_print (" alpha[%d]",
	   window->alpha);

  s = print_region (window->clip_region);
  g_print (" clipbox[%s]", s);

  g_print ("\n");
}


static void
gdk_window_print_tree (GdkWindow *window,
		       int indent,
		       gboolean include_input_only)
{
  GList *l;

  if (window->input_only && !include_input_only)
    return;

  gdk_window_print (window, indent);

  for (l = window->children; l != NULL; l = l->next)
    gdk_window_print_tree (l->data, indent + 4, include_input_only);
}

#endif /* DEBUG_WINDOW_PRINTING */

void
_gdk_windowing_got_event (GdkDisplay *display,
                          GList      *event_link,
                          GdkEvent   *event,
                          gulong      serial)
{
  GdkWindow *event_window;
  gdouble x, y;
  gboolean unlink_event = FALSE;
  GdkDeviceGrabInfo *button_release_grab;
  GdkPointerWindowInfo *pointer_info = NULL;
  GdkDevice *device, *source_device;
  gboolean is_toplevel;

  _gdk_display_update_last_event (display, event);

  device = gdk_event_get_device (event);
  source_device = gdk_event_get_source_device (event);

  if (device)
    {
      if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD &&
          gdk_device_get_source (device) != GDK_SOURCE_TABLET_PAD)
        {
          pointer_info = _gdk_display_get_pointer_info (display, device);

          if (source_device != pointer_info->last_slave &&
              gdk_device_get_device_type (source_device) == GDK_DEVICE_TYPE_SLAVE)
            pointer_info->last_slave = source_device;
          else if (pointer_info->last_slave)
            source_device = pointer_info->last_slave;
        }

      _gdk_display_device_grab_update (display, device, source_device, serial);

      if (gdk_device_get_input_mode (device) == GDK_MODE_DISABLED ||
          !_gdk_display_check_grab_ownership (display, device, serial))
        {
          /* Device events are blocked by another
           * device grab, or the device is disabled
           */
          unlink_event = TRUE;
          goto out;
        }
    }

  event_window = event->any.window;
  if (!event_window)
    goto out;

#ifdef DEBUG_WINDOW_PRINTING
  if (event->type == GDK_KEY_PRESS &&
      (event->key.keyval == 0xa7 ||
       event->key.keyval == 0xbd))
    {
      gdk_window_print_tree (event_window, 0, event->key.keyval == 0xbd);
    }
#endif

  if (event->type == GDK_VISIBILITY_NOTIFY)
    {
      event_window->native_visibility = event->visibility.state;
      gdk_window_update_visibility_recursively (event_window, event_window);
      goto out;
    }

  if (!(event->type == GDK_TOUCH_CANCEL ||
        is_button_type (event->type) ||
        is_motion_type (event->type) ||
        is_gesture_type (event->type)) ||
      event_window->window_type == GDK_WINDOW_ROOT)
    goto out;

  is_toplevel = gdk_window_is_toplevel (event_window);

  if ((event->type == GDK_ENTER_NOTIFY ||
       event->type == GDK_LEAVE_NOTIFY) &&
      (event->crossing.mode == GDK_CROSSING_GRAB ||
       event->crossing.mode == GDK_CROSSING_UNGRAB) &&
      (_gdk_display_has_device_grab (display, device, serial) ||
       event->crossing.detail == GDK_NOTIFY_INFERIOR))
    {
      /* We synthesize all crossing events due to grabs ourselves,
       * so we ignore the native ones caused by our native pointer_grab
       * calls. Otherwise we would proxy these crossing event and cause
       * multiple copies of crossing events for grabs.
       *
       * We do want to handle grabs from other clients though, as for
       * instance alt-tab in metacity causes grabs like these and
       * we want to handle those. Thus the has_pointer_grab check.
       *
       * Implicit grabs on child windows create some grabbing events
       * that are sent before the button press. This means we can't
       * detect these with the has_pointer_grab check (as the implicit
       * grab is only noticed when we get button press event), so we
       * detect these events by checking for INFERIOR enter or leave
       * events. These should never be a problem to filter out.
       */

      /* We ended up in this window after some (perhaps other clients)
       * grab, so update the toplevel_under_window state
       */
      if (pointer_info && is_toplevel &&
          event->type == GDK_ENTER_NOTIFY &&
          event->crossing.mode == GDK_CROSSING_UNGRAB)
        {
          if (pointer_info->toplevel_under_pointer)
            g_object_unref (pointer_info->toplevel_under_pointer);
          pointer_info->toplevel_under_pointer = g_object_ref (event_window);
        }

      unlink_event = TRUE;
      goto out;
    }

  /* Track toplevel_under_pointer */
  if (pointer_info && is_toplevel)
    {
      if (event->type == GDK_ENTER_NOTIFY &&
          event->crossing.detail != GDK_NOTIFY_INFERIOR)
        {
          if (pointer_info->toplevel_under_pointer)
            g_object_unref (pointer_info->toplevel_under_pointer);
          pointer_info->toplevel_under_pointer = g_object_ref (event_window);
        }
      else if (event->type == GDK_LEAVE_NOTIFY &&
               event->crossing.detail != GDK_NOTIFY_INFERIOR &&
               pointer_info->toplevel_under_pointer == event_window)
        {
          if (pointer_info->toplevel_under_pointer)
            g_object_unref (pointer_info->toplevel_under_pointer);
          pointer_info->toplevel_under_pointer = NULL;
        }
    }

  if (pointer_info &&
      (!is_touch_type (event->type) ||
       gdk_event_get_pointer_emulated (event)))
    {
      guint old_state, old_button;

      /* Store last pointer window and position/state */
      old_state = pointer_info->state;
      old_button = pointer_info->button;

      gdk_event_get_coords (event, &x, &y);
      convert_native_coords_to_toplevel (event_window, x, y,  &x, &y);
      pointer_info->toplevel_x = x;
      pointer_info->toplevel_y = y;
      gdk_event_get_state (event, &pointer_info->state);

      if (event->type == GDK_BUTTON_PRESS ||
          event->type == GDK_BUTTON_RELEASE)
        pointer_info->button = event->button.button;
      else if (event->type == GDK_TOUCH_BEGIN ||
               event->type == GDK_TOUCH_END)
        pointer_info->button = 1;

      if (device &&
          (pointer_info->state != old_state ||
           pointer_info->button != old_button))
        _gdk_display_enable_motion_hints (display, device);
    }

  if (is_motion_type (event->type))
    unlink_event = proxy_pointer_event (display, event, serial);
  else if (is_button_type (event->type))
    unlink_event = proxy_button_event (event, serial);
  else if (is_gesture_type (event->type))
    unlink_event = proxy_gesture_event (event, serial);

  if ((event->type == GDK_BUTTON_RELEASE ||
       event->type == GDK_TOUCH_CANCEL ||
       event->type == GDK_TOUCH_END) &&
      !event->any.send_event)
    {
      GdkEventSequence *sequence;

      sequence = gdk_event_get_event_sequence (event);
      if (sequence)
        {
          _gdk_display_end_touch_grab (display, device, sequence);
        }

      if (event->type == GDK_BUTTON_RELEASE ||
          gdk_event_get_pointer_emulated (event))
        {
          button_release_grab =
            _gdk_display_has_device_grab (display, device, serial);

          if (button_release_grab &&
              button_release_grab->implicit &&
              (event->button.state & GDK_ANY_BUTTON_MASK & ~(GDK_BUTTON1_MASK << (event->button.button - 1))) == 0)
            {
              button_release_grab->serial_end = serial;
              button_release_grab->implicit_ungrab = FALSE;
              _gdk_display_device_grab_update (display, device, source_device, serial);
            }
        }
    }

 out:
  if (unlink_event)
    {
      _gdk_event_queue_remove_link (display, event_link);
      g_list_free_1 (event_link);
      gdk_event_free (event);
    }

  /* This does two things - first it sees if there are motions at the
   * end of the queue that can be compressed. Second, if there is just
   * a single motion that won't be dispatched because it is a compression
   * candidate it queues up flushing the event queue.
   */
  _gdk_event_queue_handle_motion_compression (display);
}

/**
 * gdk_window_create_similar_surface:
 * @window: window to make new surface similar to
 * @content: the content for the new surface
 * @width: width of the new surface
 * @height: height of the new surface
 *
 * Create a new surface that is as compatible as possible with the
 * given @window. For example the new surface will have the same
 * fallback resolution and font options as @window. Generally, the new
 * surface will also use the same backend as @window, unless that is
 * not possible for some reason. The type of the returned surface may
 * be examined with cairo_surface_get_type().
 *
 * Initially the surface contents are all 0 (transparent if contents
 * have transparency, black otherwise.)
 *
 * Returns: a pointer to the newly allocated surface. The caller
 * owns the surface and should call cairo_surface_destroy() when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a “nil” surface if @other is already in an error state
 * or any other error occurs.
 *
 * Since: 2.22
 **/
cairo_surface_t *
gdk_window_create_similar_surface (GdkWindow *     window,
                                   cairo_content_t content,
                                   int             width,
                                   int             height)
{
  GdkDisplay *display;
  GdkRenderingMode rendering_mode;
  cairo_surface_t *window_surface, *surface;
  double sx, sy;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  window_surface = gdk_window_ref_impl_surface (window);
  sx = sy = 1;
  cairo_surface_get_device_scale (window_surface, &sx, &sy);

  display = gdk_window_get_display (window);
  rendering_mode = gdk_display_get_rendering_mode (display);

  switch (rendering_mode)
  {
    case GDK_RENDERING_MODE_RECORDING:
      {
        cairo_rectangle_t rect = { 0, 0, width * sx, height *sy };
        surface = cairo_recording_surface_create (content, &rect);
        cairo_surface_set_device_scale (surface, sx, sy);
      }
      break;
    case GDK_RENDERING_MODE_IMAGE:
      surface = cairo_image_surface_create (content == CAIRO_CONTENT_COLOR ? CAIRO_FORMAT_RGB24 :
                                            content == CAIRO_CONTENT_ALPHA ? CAIRO_FORMAT_A8 : CAIRO_FORMAT_ARGB32,
                                            width * sx, height * sy);
      cairo_surface_set_device_scale (surface, sx, sy);
      break;
    case GDK_RENDERING_MODE_SIMILAR:
    default:
      surface = cairo_surface_create_similar (window_surface,
                                              content,
                                              width, height);
      break;
  }

  cairo_surface_destroy (window_surface);

  return surface;
}


/**
 * gdk_window_create_similar_image_surface:
 * @window: (nullable): window to make new surface similar to, or
 *   %NULL if none
 * @format: (type int): the format for the new surface
 * @width: width of the new surface
 * @height: height of the new surface
 * @scale: the scale of the new surface, or 0 to use same as @window
 *
 * Create a new image surface that is efficient to draw on the
 * given @window.
 *
 * Initially the surface contents are all 0 (transparent if contents
 * have transparency, black otherwise.)
 *
 * The @width and @height of the new surface are not affected by
 * the scaling factor of the @window, or by the @scale argument; they
 * are the size of the surface in device pixels. If you wish to create
 * an image surface capable of holding the contents of @window you can
 * use:
 *
 * |[<!-- language="C" -->
 *   int scale = gdk_window_get_scale_factor (window);
 *   int width = gdk_window_get_width (window) * scale;
 *   int height = gdk_window_get_height (window) * scale;
 *
 *   // format is set elsewhere
 *   cairo_surface_t *surface =
 *     gdk_window_create_similar_image_surface (window,
 *                                              format,
 *                                              width, height,
 *                                              scale);
 * ]|
 *
 * Note that unlike cairo_surface_create_similar_image(), the new
 * surface's device scale is set to @scale, or to the scale factor of
 * @window if @scale is 0.
 *
 * Returns: a pointer to the newly allocated surface. The caller
 * owns the surface and should call cairo_surface_destroy() when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a “nil” surface if @other is already in an error state
 * or any other error occurs.
 *
 * Since: 3.10
 **/
cairo_surface_t *
gdk_window_create_similar_image_surface (GdkWindow *     window,
					 cairo_format_t  format,
					 int             width,
					 int             height,
					 int             scale)
{
  GdkWindowImplClass *impl_class;
  cairo_surface_t *window_surface, *surface;
  GdkDisplay *display;
  GdkScreen *screen;

  g_return_val_if_fail (window ==NULL || GDK_IS_WINDOW (window), NULL);

  if (window == NULL)
    {
      display = gdk_display_get_default ();
      screen = gdk_display_get_default_screen (display);
      window = gdk_screen_get_root_window (screen);
    }

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

  if (impl_class->create_similar_image_surface)
    surface = impl_class->create_similar_image_surface (window, format, width, height);
  else
    {
      window_surface = gdk_window_ref_impl_surface (window);
      surface =
        cairo_surface_create_similar_image (window_surface,
                                            format,
                                            width,
                                            height);
      cairo_surface_destroy (window_surface);
    }

  if (scale == 0)
    scale = gdk_window_get_scale_factor (window);

  cairo_surface_set_device_scale (surface, scale, scale);

  return surface;
}


/**
 * gdk_window_focus:
 * @window: a #GdkWindow
 * @timestamp: timestamp of the event triggering the window focus
 *
 * Sets keyboard focus to @window. In most cases, gtk_window_present()
 * should be used on a #GtkWindow, rather than calling this function.
 *
 **/
void
gdk_window_focus (GdkWindow *window,
                  guint32    timestamp)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->focus (window, timestamp);
}

/**
 * gdk_window_set_type_hint:
 * @window: A toplevel #GdkWindow
 * @hint: A hint of the function this window will have
 *
 * The application can use this call to provide a hint to the window
 * manager about the functionality of a window. The window manager
 * can use this information when determining the decoration and behaviour
 * of the window.
 *
 * The hint must be set before the window is mapped.
 **/
void
gdk_window_set_type_hint (GdkWindow        *window,
			  GdkWindowTypeHint hint)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_type_hint (window, hint);
}

/**
 * gdk_window_get_type_hint:
 * @window: A toplevel #GdkWindow
 *
 * This function returns the type hint set for a window.
 *
 * Returns: The type hint set for @window
 *
 * Since: 2.10
 **/
GdkWindowTypeHint
gdk_window_get_type_hint (GdkWindow *window)
{
  return GDK_WINDOW_IMPL_GET_CLASS (window->impl)->get_type_hint (window);
}

/**
 * gdk_window_set_modal_hint:
 * @window: A toplevel #GdkWindow
 * @modal: %TRUE if the window is modal, %FALSE otherwise.
 *
 * The application can use this hint to tell the window manager
 * that a certain window has modal behaviour. The window manager
 * can use this information to handle modal windows in a special
 * way.
 *
 * You should only use this on windows for which you have
 * previously called gdk_window_set_transient_for()
 **/
void
gdk_window_set_modal_hint (GdkWindow *window,
			   gboolean   modal)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_modal_hint (window, modal);
}

/**
 * gdk_window_set_skip_taskbar_hint:
 * @window: a toplevel #GdkWindow
 * @skips_taskbar: %TRUE to skip the taskbar
 *
 * Toggles whether a window should appear in a task list or window
 * list. If a window’s semantic type as specified with
 * gdk_window_set_type_hint() already fully describes the window, this
 * function should not be called in addition,
 * instead you should allow the window to be treated according to
 * standard policy for its semantic type.
 *
 * Since: 2.2
 **/
void
gdk_window_set_skip_taskbar_hint (GdkWindow *window,
                                  gboolean   skips_taskbar)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_skip_taskbar_hint (window, skips_taskbar);
}

/**
 * gdk_window_set_skip_pager_hint:
 * @window: a toplevel #GdkWindow
 * @skips_pager: %TRUE to skip the pager
 *
 * Toggles whether a window should appear in a pager (workspace
 * switcher, or other desktop utility program that displays a small
 * thumbnail representation of the windows on the desktop). If a
 * window’s semantic type as specified with gdk_window_set_type_hint()
 * already fully describes the window, this function should
 * not be called in addition, instead you should
 * allow the window to be treated according to standard policy for
 * its semantic type.
 *
 * Since: 2.2
 **/
void
gdk_window_set_skip_pager_hint (GdkWindow *window,
                                gboolean   skips_pager)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_skip_pager_hint (window, skips_pager);
}

/**
 * gdk_window_set_urgency_hint:
 * @window: a toplevel #GdkWindow
 * @urgent: %TRUE if the window is urgent
 *
 * Toggles whether a window needs the user's
 * urgent attention.
 *
 * Since: 2.8
 **/
void
gdk_window_set_urgency_hint (GdkWindow *window,
			     gboolean   urgent)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_urgency_hint (window, urgent);
}

/**
 * gdk_window_set_geometry_hints:
 * @window: a toplevel #GdkWindow
 * @geometry: geometry hints
 * @geom_mask: bitmask indicating fields of @geometry to pay attention to
 *
 * Sets the geometry hints for @window. Hints flagged in @geom_mask
 * are set, hints not flagged in @geom_mask are unset.
 * To unset all hints, use a @geom_mask of 0 and a @geometry of %NULL.
 *
 * This function provides hints to the windowing system about
 * acceptable sizes for a toplevel window. The purpose of
 * this is to constrain user resizing, but the windowing system
 * will typically  (but is not required to) also constrain the
 * current size of the window to the provided values and
 * constrain programatic resizing via gdk_window_resize() or
 * gdk_window_move_resize().
 *
 * Note that on X11, this effect has no effect on windows
 * of type %GDK_WINDOW_TEMP since these windows are not resizable
 * by the user.
 *
 * Since you can’t count on the windowing system doing the
 * constraints for programmatic resizes, you should generally
 * call gdk_window_constrain_size() yourself to determine
 * appropriate sizes.
 *
 **/
void
gdk_window_set_geometry_hints (GdkWindow         *window,
			       const GdkGeometry *geometry,
			       GdkWindowHints     geom_mask)
{
  g_return_if_fail (geometry != NULL || geom_mask == 0);

  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_geometry_hints (window, geometry, geom_mask);
}

/**
 * gdk_window_set_title:
 * @window: a toplevel #GdkWindow
 * @title: title of @window
 *
 * Sets the title of a toplevel window, to be displayed in the titlebar.
 * If you haven’t explicitly set the icon name for the window
 * (using gdk_window_set_icon_name()), the icon name will be set to
 * @title as well. @title must be in UTF-8 encoding (as with all
 * user-readable strings in GDK/GTK+). @title may not be %NULL.
 **/
void
gdk_window_set_title (GdkWindow   *window,
		      const gchar *title)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_title (window, title);
}

/**
 * gdk_window_set_role:
 * @window: a toplevel #GdkWindow
 * @role: a string indicating its role
 *
 * When using GTK+, typically you should use gtk_window_set_role() instead
 * of this low-level function.
 *
 * The window manager and session manager use a window’s role to
 * distinguish it from other kinds of window in the same application.
 * When an application is restarted after being saved in a previous
 * session, all windows with the same title and role are treated as
 * interchangeable.  So if you have two windows with the same title
 * that should be distinguished for session management purposes, you
 * should set the role on those windows. It doesn’t matter what string
 * you use for the role, as long as you have a different role for each
 * non-interchangeable kind of window.
 *
 **/
void
gdk_window_set_role (GdkWindow   *window,
		     const gchar *role)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_role (window, role);
}

/**
 * gdk_window_set_startup_id:
 * @window: a toplevel #GdkWindow
 * @startup_id: a string with startup-notification identifier
 *
 * When using GTK+, typically you should use gtk_window_set_startup_id()
 * instead of this low-level function.
 *
 * Since: 2.12
 *
 **/
void
gdk_window_set_startup_id (GdkWindow   *window,
			   const gchar *startup_id)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_startup_id (window, startup_id);
}

/**
 * gdk_window_set_transient_for:
 * @window: a toplevel #GdkWindow
 * @parent: another toplevel #GdkWindow
 *
 * Indicates to the window manager that @window is a transient dialog
 * associated with the application window @parent. This allows the
 * window manager to do things like center @window on @parent and
 * keep @window above @parent.
 *
 * See gtk_window_set_transient_for() if you’re using #GtkWindow or
 * #GtkDialog.
 **/
void
gdk_window_set_transient_for (GdkWindow *window,
			      GdkWindow *parent)
{
  window->transient_for = parent;

  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_transient_for (window, parent);
}

/**
 * gdk_window_get_root_origin:
 * @window: a toplevel #GdkWindow
 * @x: (out): return location for X position of window frame
 * @y: (out): return location for Y position of window frame
 *
 * Obtains the top-left corner of the window manager frame in root
 * window coordinates.
 *
 **/
void
gdk_window_get_root_origin (GdkWindow *window,
			    gint      *x,
			    gint      *y)
{
  GdkRectangle rect;

  gdk_window_get_frame_extents (window, &rect);

  if (x)
    *x = rect.x;

  if (y)
    *y = rect.y;
}

/**
 * gdk_window_get_frame_extents:
 * @window: a toplevel #GdkWindow
 * @rect: (out): rectangle to fill with bounding box of the window frame
 *
 * Obtains the bounding box of the window, including window manager
 * titlebar/borders if any. The frame position is given in root window
 * coordinates. To get the position of the window itself (rather than
 * the frame) in root window coordinates, use gdk_window_get_origin().
 *
 **/
void
gdk_window_get_frame_extents (GdkWindow    *window,
                              GdkRectangle *rect)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->get_frame_extents (window, rect);
}

/**
 * gdk_window_set_accept_focus:
 * @window: a toplevel #GdkWindow
 * @accept_focus: %TRUE if the window should receive input focus
 *
 * Setting @accept_focus to %FALSE hints the desktop environment that the
 * window doesn’t want to receive input focus.
 *
 * On X, it is the responsibility of the window manager to interpret this
 * hint. ICCCM-compliant window manager usually respect it.
 *
 * Since: 2.4
 **/
void
gdk_window_set_accept_focus (GdkWindow *window,
			     gboolean accept_focus)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_accept_focus (window, accept_focus);
}

/**
 * gdk_window_set_focus_on_map:
 * @window: a toplevel #GdkWindow
 * @focus_on_map: %TRUE if the window should receive input focus when mapped
 *
 * Setting @focus_on_map to %FALSE hints the desktop environment that the
 * window doesn’t want to receive input focus when it is mapped.
 * focus_on_map should be turned off for windows that aren’t triggered
 * interactively (such as popups from network activity).
 *
 * On X, it is the responsibility of the window manager to interpret
 * this hint. Window managers following the freedesktop.org window
 * manager extension specification should respect it.
 *
 * Since: 2.6
 **/
void
gdk_window_set_focus_on_map (GdkWindow *window,
			     gboolean focus_on_map)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_focus_on_map (window, focus_on_map);
}

/**
 * gdk_window_set_event_compression:
 * @window: a #GdkWindow
 * @event_compression: %TRUE if motion events should be compressed
 *
 * Determines whether or not extra unprocessed motion events in
 * the event queue can be discarded. If %TRUE only the most recent
 * event will be delivered.
 *
 * Some types of applications, e.g. paint programs, need to see all
 * motion events and will benefit from turning off event compression.
 *
 * By default, event compression is enabled.
 *
 * Since: 3.12
 **/
void
gdk_window_set_event_compression (GdkWindow *window,
                                  gboolean   event_compression)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  window->event_compression = !!event_compression;
}

/**
 * gdk_window_get_event_compression:
 * @window: a #GdkWindow
 *
 * Get the current event compression setting for this window.
 *
 * Returns: %TRUE if motion events will be compressed
 *
 * Since: 3.12
 **/
gboolean
gdk_window_get_event_compression (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), TRUE);

  return window->event_compression;
}

/**
 * gdk_window_set_icon_list:
 * @window: The #GdkWindow toplevel window to set the icon of.
 * @pixbufs: (transfer none) (element-type GdkPixbuf):
 *     A list of pixbufs, of different sizes.
 *
 * Sets a list of icons for the window. One of these will be used
 * to represent the window when it has been iconified. The icon is
 * usually shown in an icon box or some sort of task bar. Which icon
 * size is shown depends on the window manager. The window manager
 * can scale the icon  but setting several size icons can give better
 * image quality since the window manager may only need to scale the
 * icon by a small amount or not at all.
 *
 * Note that some platforms don't support window icons.
 */
void
gdk_window_set_icon_list (GdkWindow *window,
			  GList     *pixbufs)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_icon_list (window, pixbufs);
}

/**
 * gdk_window_set_icon_name:
 * @window: a toplevel #GdkWindow
 * @name: (allow-none): name of window while iconified (minimized)
 *
 * Windows may have a name used while minimized, distinct from the
 * name they display in their titlebar. Most of the time this is a bad
 * idea from a user interface standpoint. But you can set such a name
 * with this function, if you like.
 *
 * After calling this with a non-%NULL @name, calls to gdk_window_set_title()
 * will not update the icon title.
 *
 * Using %NULL for @name unsets the icon title; further calls to
 * gdk_window_set_title() will again update the icon title as well.
 *
 * Note that some platforms don't support window icons.
 **/
void
gdk_window_set_icon_name (GdkWindow   *window,
			  const gchar *name)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_icon_name (window, name);
}

/**
 * gdk_window_iconify:
 * @window: a toplevel #GdkWindow
 *
 * Asks to iconify (minimize) @window. The window manager may choose
 * to ignore the request, but normally will honor it. Using
 * gtk_window_iconify() is preferred, if you have a #GtkWindow widget.
 *
 * This function only makes sense when @window is a toplevel window.
 *
 **/
void
gdk_window_iconify (GdkWindow *window)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->iconify (window);
}

/**
 * gdk_window_deiconify:
 * @window: a toplevel #GdkWindow
 *
 * Attempt to deiconify (unminimize) @window. On X11 the window manager may
 * choose to ignore the request to deiconify. When using GTK+,
 * use gtk_window_deiconify() instead of the #GdkWindow variant. Or better yet,
 * you probably want to use gtk_window_present(), which raises the window, focuses it,
 * unminimizes it, and puts it on the current desktop.
 *
 **/
void
gdk_window_deiconify (GdkWindow *window)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->deiconify (window);
}

/**
 * gdk_window_stick:
 * @window: a toplevel #GdkWindow
 *
 * “Pins” a window such that it’s on all workspaces and does not scroll
 * with viewports, for window managers that have scrollable viewports.
 * (When using #GtkWindow, gtk_window_stick() may be more useful.)
 *
 * On the X11 platform, this function depends on window manager
 * support, so may have no effect with many window managers. However,
 * GDK will do the best it can to convince the window manager to stick
 * the window. For window managers that don’t support this operation,
 * there’s nothing you can do to force it to happen.
 *
 **/
void
gdk_window_stick (GdkWindow *window)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->stick (window);
}

/**
 * gdk_window_unstick:
 * @window: a toplevel #GdkWindow
 *
 * Reverse operation for gdk_window_stick(); see gdk_window_stick(),
 * and gtk_window_unstick().
 *
 **/
void
gdk_window_unstick (GdkWindow *window)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->unstick (window);
}

/**
 * gdk_window_maximize:
 * @window: a toplevel #GdkWindow
 *
 * Maximizes the window. If the window was already maximized, then
 * this function does nothing.
 *
 * On X11, asks the window manager to maximize @window, if the window
 * manager supports this operation. Not all window managers support
 * this, and some deliberately ignore it or don’t have a concept of
 * “maximized”; so you can’t rely on the maximization actually
 * happening. But it will happen with most standard window managers,
 * and GDK makes a best effort to get it to happen.
 *
 * On Windows, reliably maximizes the window.
 *
 **/
void
gdk_window_maximize (GdkWindow *window)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->maximize (window);
}

/**
 * gdk_window_unmaximize:
 * @window: a toplevel #GdkWindow
 *
 * Unmaximizes the window. If the window wasn’t maximized, then this
 * function does nothing.
 *
 * On X11, asks the window manager to unmaximize @window, if the
 * window manager supports this operation. Not all window managers
 * support this, and some deliberately ignore it or don’t have a
 * concept of “maximized”; so you can’t rely on the unmaximization
 * actually happening. But it will happen with most standard window
 * managers, and GDK makes a best effort to get it to happen.
 *
 * On Windows, reliably unmaximizes the window.
 *
 **/
void
gdk_window_unmaximize (GdkWindow *window)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->unmaximize (window);
}

/**
 * gdk_window_fullscreen:
 * @window: a toplevel #GdkWindow
 *
 * Moves the window into fullscreen mode. This means the
 * window covers the entire screen and is above any panels
 * or task bars.
 *
 * If the window was already fullscreen, then this function does nothing.
 *
 * On X11, asks the window manager to put @window in a fullscreen
 * state, if the window manager supports this operation. Not all
 * window managers support this, and some deliberately ignore it or
 * don’t have a concept of “fullscreen”; so you can’t rely on the
 * fullscreenification actually happening. But it will happen with
 * most standard window managers, and GDK makes a best effort to get
 * it to happen.
 *
 * Since: 2.2
 **/
void
gdk_window_fullscreen (GdkWindow *window)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->fullscreen (window);
}

/**
 * gdk_window_fullscreen_on_monitor:
 * @window: a toplevel #GdkWindow
 * @monitor: Which monitor to display fullscreen on.
 *
 * Moves the window into fullscreen mode on the given monitor. This means
 * the window covers the entire screen and is above any panels or task bars.
 *
 * If the window was already fullscreen, then this function does nothing.
 * Since: UNRELEASED
 **/
void
gdk_window_fullscreen_on_monitor (GdkWindow      *window,
                                  gint            monitor)
{
  GdkDisplay *display = gdk_window_get_display (window);

  g_return_if_fail (monitor >= 0);
  g_return_if_fail (monitor < gdk_display_get_n_monitors (display));

  if (GDK_WINDOW_IMPL_GET_CLASS (window->impl)->fullscreen_on_monitor != NULL)
    GDK_WINDOW_IMPL_GET_CLASS (window->impl)->fullscreen_on_monitor (window, monitor);
  else
    GDK_WINDOW_IMPL_GET_CLASS (window->impl)->fullscreen (window);
}

/**
 * gdk_window_set_fullscreen_mode:
 * @window: a toplevel #GdkWindow
 * @mode: fullscreen mode
 *
 * Specifies whether the @window should span over all monitors (in a multi-head
 * setup) or only the current monitor when in fullscreen mode.
 *
 * The @mode argument is from the #GdkFullscreenMode enumeration.
 * If #GDK_FULLSCREEN_ON_ALL_MONITORS is specified, the fullscreen @window will
 * span over all monitors from the #GdkScreen.
 *
 * On X11, searches through the list of monitors from the #GdkScreen the ones
 * which delimit the 4 edges of the entire #GdkScreen and will ask the window
 * manager to span the @window over these monitors.
 *
 * If the XINERAMA extension is not available or not usable, this function
 * has no effect.
 *
 * Not all window managers support this, so you can’t rely on the fullscreen
 * window to span over the multiple monitors when #GDK_FULLSCREEN_ON_ALL_MONITORS
 * is specified.
 *
 * Since: 3.8
 **/
void
gdk_window_set_fullscreen_mode (GdkWindow        *window,
                                GdkFullscreenMode mode)
{
  GdkWindowImplClass *impl_class;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->fullscreen_mode != mode)
    {
      window->fullscreen_mode = mode;

      impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
      if (impl_class->apply_fullscreen_mode != NULL)
        impl_class->apply_fullscreen_mode (window);
    }
}

/**
 * gdk_window_get_fullscreen_mode:
 * @window: a toplevel #GdkWindow
 *
 * Obtains the #GdkFullscreenMode of the @window.
 *
 * Returns: The #GdkFullscreenMode applied to the window when fullscreen.
 *
 * Since: 3.8
 **/
GdkFullscreenMode
gdk_window_get_fullscreen_mode (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), GDK_FULLSCREEN_ON_CURRENT_MONITOR);

  return window->fullscreen_mode;
}

/**
 * gdk_window_unfullscreen:
 * @window: a toplevel #GdkWindow
 *
 * Moves the window out of fullscreen mode. If the window was not
 * fullscreen, does nothing.
 *
 * On X11, asks the window manager to move @window out of the fullscreen
 * state, if the window manager supports this operation. Not all
 * window managers support this, and some deliberately ignore it or
 * don’t have a concept of “fullscreen”; so you can’t rely on the
 * unfullscreenification actually happening. But it will happen with
 * most standard window managers, and GDK makes a best effort to get
 * it to happen.
 *
 * Since: 2.2
 **/
void
gdk_window_unfullscreen (GdkWindow *window)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->unfullscreen (window);
}

/**
 * gdk_window_set_keep_above:
 * @window: a toplevel #GdkWindow
 * @setting: whether to keep @window above other windows
 *
 * Set if @window must be kept above other windows. If the
 * window was already above, then this function does nothing.
 *
 * On X11, asks the window manager to keep @window above, if the window
 * manager supports this operation. Not all window managers support
 * this, and some deliberately ignore it or don’t have a concept of
 * “keep above”; so you can’t rely on the window being kept above.
 * But it will happen with most standard window managers,
 * and GDK makes a best effort to get it to happen.
 *
 * Since: 2.4
 **/
void
gdk_window_set_keep_above (GdkWindow *window,
                           gboolean   setting)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_keep_above (window, setting);
}

/**
 * gdk_window_set_keep_below:
 * @window: a toplevel #GdkWindow
 * @setting: whether to keep @window below other windows
 *
 * Set if @window must be kept below other windows. If the
 * window was already below, then this function does nothing.
 *
 * On X11, asks the window manager to keep @window below, if the window
 * manager supports this operation. Not all window managers support
 * this, and some deliberately ignore it or don’t have a concept of
 * “keep below”; so you can’t rely on the window being kept below.
 * But it will happen with most standard window managers,
 * and GDK makes a best effort to get it to happen.
 *
 * Since: 2.4
 **/
void
gdk_window_set_keep_below (GdkWindow *window, gboolean setting)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_keep_below (window, setting);
}

/**
 * gdk_window_get_group:
 * @window: a toplevel #GdkWindow
 *
 * Returns the group leader window for @window. See gdk_window_set_group().
 *
 * Returns: (transfer none): the group leader window for @window
 *
 * Since: 2.4
 **/
GdkWindow *
gdk_window_get_group (GdkWindow *window)
{
  return GDK_WINDOW_IMPL_GET_CLASS (window->impl)->get_group (window);
}

/**
 * gdk_window_set_group:
 * @window: a toplevel #GdkWindow
 * @leader: (allow-none): group leader window, or %NULL to restore the default group leader window
 *
 * Sets the group leader window for @window. By default,
 * GDK sets the group leader for all toplevel windows
 * to a global window implicitly created by GDK. With this function
 * you can override this default.
 *
 * The group leader window allows the window manager to distinguish
 * all windows that belong to a single application. It may for example
 * allow users to minimize/unminimize all windows belonging to an
 * application at once. You should only set a non-default group window
 * if your application pretends to be multiple applications.
 **/
void
gdk_window_set_group (GdkWindow *window,
		      GdkWindow *leader)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_group (window, leader);
}

/**
 * gdk_window_set_decorations:
 * @window: a toplevel #GdkWindow
 * @decorations: decoration hint mask
 *
 * “Decorations” are the features the window manager adds to a toplevel #GdkWindow.
 * This function sets the traditional Motif window manager hints that tell the
 * window manager which decorations you would like your window to have.
 * Usually you should use gtk_window_set_decorated() on a #GtkWindow instead of
 * using the GDK function directly.
 *
 * The @decorations argument is the logical OR of the fields in
 * the #GdkWMDecoration enumeration. If #GDK_DECOR_ALL is included in the
 * mask, the other bits indicate which decorations should be turned off.
 * If #GDK_DECOR_ALL is not included, then the other bits indicate
 * which decorations should be turned on.
 *
 * Most window managers honor a decorations hint of 0 to disable all decorations,
 * but very few honor all possible combinations of bits.
 *
 **/
void
gdk_window_set_decorations (GdkWindow      *window,
			    GdkWMDecoration decorations)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_decorations (window, decorations);
}

/**
 * gdk_window_get_decorations:
 * @window: The toplevel #GdkWindow to get the decorations from
 * @decorations: (out): The window decorations will be written here
 *
 * Returns the decorations set on the GdkWindow with
 * gdk_window_set_decorations().
 *
 * Returns: %TRUE if the window has decorations set, %FALSE otherwise.
 **/
gboolean
gdk_window_get_decorations(GdkWindow       *window,
			   GdkWMDecoration *decorations)
{
  return GDK_WINDOW_IMPL_GET_CLASS (window->impl)->get_decorations (window, decorations);
}

/**
 * gdk_window_set_functions:
 * @window: a toplevel #GdkWindow
 * @functions: bitmask of operations to allow on @window
 *
 * Sets hints about the window management functions to make available
 * via buttons on the window frame.
 *
 * On the X backend, this function sets the traditional Motif window
 * manager hint for this purpose. However, few window managers do
 * anything reliable or interesting with this hint. Many ignore it
 * entirely.
 *
 * The @functions argument is the logical OR of values from the
 * #GdkWMFunction enumeration. If the bitmask includes #GDK_FUNC_ALL,
 * then the other bits indicate which functions to disable; if
 * it doesn’t include #GDK_FUNC_ALL, it indicates which functions to
 * enable.
 *
 **/
void
gdk_window_set_functions (GdkWindow    *window,
			  GdkWMFunction functions)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_functions (window, functions);
}

/**
 * gdk_window_begin_resize_drag_for_device:
 * @window: a toplevel #GdkWindow
 * @edge: the edge or corner from which the drag is started
 * @device: the device used for the operation
 * @button: the button being used to drag, or 0 for a keyboard-initiated drag
 * @root_x: root window X coordinate of mouse click that began the drag
 * @root_y: root window Y coordinate of mouse click that began the drag
 * @timestamp: timestamp of mouse click that began the drag (use gdk_event_get_time())
 *
 * Begins a window resize operation (for a toplevel window).
 * You might use this function to implement a “window resize grip,” for
 * example; in fact #GtkStatusbar uses it. The function works best
 * with window managers that support the
 * [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec)
 * but has a fallback implementation for other window managers.
 *
 * Since: 3.4
 */
void
gdk_window_begin_resize_drag_for_device (GdkWindow     *window,
                                         GdkWindowEdge  edge,
                                         GdkDevice     *device,
                                         gint           button,
                                         gint           root_x,
                                         gint           root_y,
                                         guint32        timestamp)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->begin_resize_drag (window, edge, device, button, root_x, root_y, timestamp);
}

/**
 * gdk_window_begin_resize_drag:
 * @window: a toplevel #GdkWindow
 * @edge: the edge or corner from which the drag is started
 * @button: the button being used to drag, or 0 for a keyboard-initiated drag
 * @root_x: root window X coordinate of mouse click that began the drag
 * @root_y: root window Y coordinate of mouse click that began the drag
 * @timestamp: timestamp of mouse click that began the drag (use gdk_event_get_time())
 *
 * Begins a window resize operation (for a toplevel window).
 *
 * This function assumes that the drag is controlled by the
 * client pointer device, use gdk_window_begin_resize_drag_for_device()
 * to begin a drag with a different device.
 */
void
gdk_window_begin_resize_drag (GdkWindow     *window,
                              GdkWindowEdge  edge,
                              gint           button,
                              gint           root_x,
                              gint           root_y,
                              guint32        timestamp)
{
  GdkDisplay *display;
  GdkDevice *device;

  display = gdk_window_get_display (window);
  device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
  gdk_window_begin_resize_drag_for_device (window, edge,
                                           device, button, root_x, root_y, timestamp);
}

/**
 * gdk_window_begin_move_drag_for_device:
 * @window: a toplevel #GdkWindow
 * @device: the device used for the operation
 * @button: the button being used to drag, or 0 for a keyboard-initiated drag
 * @root_x: root window X coordinate of mouse click that began the drag
 * @root_y: root window Y coordinate of mouse click that began the drag
 * @timestamp: timestamp of mouse click that began the drag
 *
 * Begins a window move operation (for a toplevel window).
 * You might use this function to implement a “window move grip,” for
 * example. The function works best with window managers that support the
 * [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec)
 * but has a fallback implementation for other window managers.
 *
 * Since: 3.4
 */
void
gdk_window_begin_move_drag_for_device (GdkWindow *window,
                                       GdkDevice *device,
                                       gint       button,
                                       gint       root_x,
                                       gint       root_y,
                                       guint32    timestamp)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->begin_move_drag (window,
                                                             device, button, root_x, root_y, timestamp);
}

/**
 * gdk_window_begin_move_drag:
 * @window: a toplevel #GdkWindow
 * @button: the button being used to drag, or 0 for a keyboard-initiated drag
 * @root_x: root window X coordinate of mouse click that began the drag
 * @root_y: root window Y coordinate of mouse click that began the drag
 * @timestamp: timestamp of mouse click that began the drag
 *
 * Begins a window move operation (for a toplevel window).
 *
 * This function assumes that the drag is controlled by the
 * client pointer device, use gdk_window_begin_move_drag_for_device()
 * to begin a drag with a different device.
 */
void
gdk_window_begin_move_drag (GdkWindow *window,
                            gint       button,
                            gint       root_x,
                            gint       root_y,
                            guint32    timestamp)
{
  GdkDisplay *display;
  GdkDevice *device;

  display = gdk_window_get_display (window);
  device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
  gdk_window_begin_move_drag_for_device (window, device, button, root_x, root_y, timestamp);
}

/**
 * gdk_window_set_opacity:
 * @window: a top-level or non-native #GdkWindow
 * @opacity: opacity
 *
 * Set @window to render as partially transparent,
 * with opacity 0 being fully transparent and 1 fully opaque. (Values
 * of the opacity parameter are clamped to the [0,1] range.) 
 *
 * For toplevel windows this depends on support from the windowing system
 * that may not always be there. For instance, On X11, this works only on
 * X screens with a compositing manager running. On Wayland, there is no
 * per-window opacity value that the compositor would apply. Instead, use
 * `gdk_window_set_opaque_region (window, NULL)` to tell the compositor
 * that the entire window is (potentially) non-opaque, and draw your content
 * with alpha, or use gtk_widget_set_opacity() to set an overall opacity
 * for your widgets.
 *
 * Support for non-toplevel windows was added in 3.8.
 *
 * Since: 2.12
 */
void
gdk_window_set_opacity (GdkWindow *window,
			gdouble    opacity)
{
  if (opacity < 0)
    opacity = 0;
  else if (opacity > 1)
    opacity = 1;

  window->alpha = round (opacity * 255);

  if (window->destroyed)
    return;

  if (gdk_window_has_impl (window))
    GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_opacity (window, opacity);
  else
    {
      recompute_visible_regions (window, FALSE);
      gdk_window_invalidate_rect_full (window, NULL, TRUE);
    }
}

/* This function is called when the XWindow is really gone.
 */
void
gdk_window_destroy_notify (GdkWindow *window)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->destroy_notify (window);
}

/**
 * gdk_window_register_dnd:
 * @window: a #GdkWindow.
 *
 * Registers a window as a potential drop destination.
 */
void
gdk_window_register_dnd (GdkWindow *window)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->register_dnd (window);
}

/**
 * gdk_window_get_drag_protocol:
 * @window: the destination window
 * @target: (out) (allow-none) (transfer full): location of the window
 *    where the drop should happen. This may be @window or a proxy window,
 *    or %NULL if @window does not support Drag and Drop.
 *
 * Finds out the DND protocol supported by a window.
 *
 * Returns: the supported DND protocol.
 *
 * Since: 3.0
 */
GdkDragProtocol
gdk_window_get_drag_protocol (GdkWindow  *window,
                              GdkWindow **target)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), GDK_DRAG_PROTO_NONE);

  return GDK_WINDOW_IMPL_GET_CLASS (window->impl)->get_drag_protocol (window, target);
}

/**
 * gdk_drag_begin:
 * @window: the source window for this drag.
 * @targets: (transfer none) (element-type GdkAtom): the offered targets,
 *     as list of #GdkAtoms
 *
 * Starts a drag and creates a new drag context for it.
 * This function assumes that the drag is controlled by the
 * client pointer device, use gdk_drag_begin_for_device() to
 * begin a drag with a different device.
 *
 * This function is called by the drag source.
 *
 * Returns: (transfer full): a newly created #GdkDragContext
 */
GdkDragContext *
gdk_drag_begin (GdkWindow     *window,
                GList         *targets)
{
  GdkDisplay *display;
  GdkDevice *device;

  display = gdk_window_get_display (window);
  device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));

  return gdk_drag_begin_for_device (window, device, targets);
}

/**
 * gdk_drag_begin_for_device:
 * @window: the source window for this drag
 * @device: the device that controls this drag
 * @targets: (transfer none) (element-type GdkAtom): the offered targets,
 *     as list of #GdkAtoms
 *
 * Starts a drag and creates a new drag context for it.
 *
 * This function is called by the drag source.
 *
 * Returns: (transfer full): a newly created #GdkDragContext
 */
GdkDragContext *
gdk_drag_begin_for_device (GdkWindow *window,
                           GdkDevice *device,
                           GList     *targets)
{
  gint x, y;

  gdk_device_get_position (device, NULL, &x, &y);

  return gdk_drag_begin_from_point (window, device, targets, x, y);
}

/**
 * gdk_drag_begin_from_point:
 * @window: the source window for this drag
 * @device: the device that controls this drag
 * @targets: (transfer none) (element-type GdkAtom): the offered targets,
 *     as list of #GdkAtoms
 * @x_root: the x coordinate where the drag nominally started
 * @y_root: the y coordinate where the drag nominally started
 *
 * Starts a drag and creates a new drag context for it.
 *
 * This function is called by the drag source.
 *
 * Returns: (transfer full): a newly created #GdkDragContext
 *
 * Since: 3.20
 */
GdkDragContext *
gdk_drag_begin_from_point (GdkWindow *window,
                           GdkDevice *device,
                           GList     *targets,
                           gint       x_root,
                           gint       y_root)
{
  return GDK_WINDOW_IMPL_GET_CLASS (window->impl)->drag_begin (window, device, targets, x_root, y_root);
}

/**
 * gdk_property_get:
 * @window: a #GdkWindow
 * @property: the property to retrieve
 * @type: the desired property type, or %GDK_NONE, if any type of data
 *   is acceptable. If this does not match the actual
 *   type, then @actual_format and @actual_length will
 *   be filled in, a warning will be printed to stderr
 *   and no data will be returned.
 * @offset: the offset into the property at which to begin
 *   retrieving data, in 4 byte units.
 * @length: the length of the data to retrieve in bytes.  Data is
 *   considered to be retrieved in 4 byte chunks, so @length
 *   will be rounded up to the next highest 4 byte boundary
 *   (so be careful not to pass a value that might overflow
 *   when rounded up).
 * @pdelete: if %TRUE, delete the property after retrieving the
 *   data.
 * @actual_property_type: (out) (transfer none): location to store the
 *   actual type of the property.
 * @actual_format: (out): location to store the actual return format of the
 *   data; either 8, 16 or 32 bits.
 * @actual_length: location to store the length of the retrieved data, in
 *   bytes.  Data returned in the 32 bit format is stored
 *   in a long variable, so the actual number of 32 bit
 *   elements should be be calculated via
 *   @actual_length / sizeof(glong) to ensure portability to
 *   64 bit systems.
 * @data: (out) (array length=actual_length) (transfer full): location
 *   to store a pointer to the data. The retrieved data should be
 *   freed with g_free() when you are finished using it.
 *
 * Retrieves a portion of the contents of a property. If the
 * property does not exist, then the function returns %FALSE,
 * and %GDK_NONE will be stored in @actual_property_type.
 *
 * The XGetWindowProperty() function that gdk_property_get()
 * uses has a very confusing and complicated set of semantics.
 * Unfortunately, gdk_property_get() makes the situation
 * worse instead of better (the semantics should be considered
 * undefined), and also prints warnings to stderr in cases where it
 * should return a useful error to the program. You are advised to use
 * XGetWindowProperty() directly until a replacement function for
 * gdk_property_get() is provided.
 *
 * Returns: %TRUE if data was successfully received and stored
 *   in @data, otherwise %FALSE.
 */
gboolean
gdk_property_get (GdkWindow  *window,
                  GdkAtom     property,
                  GdkAtom     type,
                  gulong      offset,
                  gulong      length,
                  gint        pdelete,
                  GdkAtom    *actual_property_type,
                  gint       *actual_format_type,
                  gint       *actual_length,
                  guchar    **data)
{
  return GDK_WINDOW_IMPL_GET_CLASS (window->impl)
    ->get_property (window, property, type, offset, length, pdelete,
                    actual_property_type, actual_format_type,
                    actual_length, data);
}

/**
 * gdk_property_change: (skip)
 * @window: a #GdkWindow
 * @property: the property to change
 * @type: the new type for the property. If @mode is
 *   %GDK_PROP_MODE_PREPEND or %GDK_PROP_MODE_APPEND, then this
 *   must match the existing type or an error will occur.
 * @format: the new format for the property. If @mode is
 *   %GDK_PROP_MODE_PREPEND or %GDK_PROP_MODE_APPEND, then this
 *   must match the existing format or an error will occur.
 * @mode: a value describing how the new data is to be combined
 *   with the current data.
 * @data: the data (a `guchar *`
 *   `gushort *`, or `gulong *`,
 *   depending on @format), cast to a `guchar *`.
 * @nelements: the number of elements of size determined by the format,
 *   contained in @data.
 *
 * Changes the contents of a property on a window.
 */
void
gdk_property_change (GdkWindow    *window,
                     GdkAtom       property,
                     GdkAtom       type,
                     gint          format,
                     GdkPropMode   mode,
                     const guchar *data,
                     gint          nelements)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)
    ->change_property (window, property, type, format, mode, data, nelements);
}

/**
 * gdk_property_delete:
 * @window: a #GdkWindow
 * @property: the property to delete
 *
 * Deletes a property from a window.
 */
void
gdk_property_delete (GdkWindow *window,
                     GdkAtom    property)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->delete_property (window, property);
}

static void
gdk_window_flush_events (GdkFrameClock *clock,
                         void          *data)
{
  GdkWindow *window;
  GdkDisplay *display;

  window = GDK_WINDOW (data);

  display = gdk_window_get_display (window);
  _gdk_event_queue_flush (display);
  _gdk_display_pause_events (display);

  gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_RESUME_EVENTS);

  window->frame_clock_events_paused = TRUE;
}

static void
gdk_window_resume_events (GdkFrameClock *clock,
                          void          *data)
{
  GdkWindow *window;
  GdkDisplay *display;

  window = GDK_WINDOW (data);

  display = gdk_window_get_display (window);
  _gdk_display_unpause_events (display);

  window->frame_clock_events_paused = FALSE;
}

static void
gdk_window_set_frame_clock (GdkWindow     *window,
                            GdkFrameClock *clock)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (clock == NULL || GDK_IS_FRAME_CLOCK (clock));
  g_return_if_fail (clock == NULL || gdk_window_is_toplevel (window));

  if (clock == window->frame_clock)
    return;

  if (clock)
    {
      g_object_ref (clock);
      g_signal_connect (G_OBJECT (clock),
                        "flush-events",
                        G_CALLBACK (gdk_window_flush_events),
                        window);
      g_signal_connect (G_OBJECT (clock),
                        "paint",
                        G_CALLBACK (gdk_window_paint_on_clock),
                        window);
      g_signal_connect (G_OBJECT (clock),
                        "resume-events",
                        G_CALLBACK (gdk_window_resume_events),
                        window);
    }

  if (window->frame_clock)
    {
      if (window->frame_clock_events_paused)
        gdk_window_resume_events (window->frame_clock, G_OBJECT (window));

      g_signal_handlers_disconnect_by_func (G_OBJECT (window->frame_clock),
                                            G_CALLBACK (gdk_window_flush_events),
                                            window);
      g_signal_handlers_disconnect_by_func (G_OBJECT (window->frame_clock),
                                            G_CALLBACK (gdk_window_paint_on_clock),
                                            window);
      g_signal_handlers_disconnect_by_func (G_OBJECT (window->frame_clock),
                                            G_CALLBACK (gdk_window_resume_events),
                                            window);
      g_object_unref (window->frame_clock);
    }

  window->frame_clock = clock;
}

/**
 * gdk_window_get_frame_clock:
 * @window: window to get frame clock for
 *
 * Gets the frame clock for the window. The frame clock for a window
 * never changes unless the window is reparented to a new toplevel
 * window.
 *
 * Since: 3.8
 * Returns: (transfer none): the frame clock
 */
GdkFrameClock*
gdk_window_get_frame_clock (GdkWindow *window)
{
  GdkWindow *toplevel;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  toplevel = gdk_window_get_toplevel (window);

  return toplevel->frame_clock;
}

/**
 * gdk_window_get_scale_factor:
 * @window: window to get scale factor for
 *
 * Returns the internal scale factor that maps from window coordiantes
 * to the actual device pixels. On traditional systems this is 1, but
 * on very high density outputs this can be a higher value (often 2).
 *
 * A higher value means that drawing is automatically scaled up to
 * a higher resolution, so any code doing drawing will automatically look
 * nicer. However, if you are supplying pixel-based data the scale
 * value can be used to determine whether to use a pixel resource
 * with higher resolution data.
 *
 * The scale of a window may change during runtime, if this happens
 * a configure event will be sent to the toplevel window.
 *
 * Since: 3.10
 * Returns: the scale factor
 */
gint
gdk_window_get_scale_factor (GdkWindow *window)
{
  GdkWindowImplClass *impl_class;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 1);

  if (GDK_WINDOW_DESTROYED (window))
    return 1;

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

  if (impl_class->get_scale_factor)
    return impl_class->get_scale_factor (window);

  return 1;
}

/* Returns the *real* unscaled size, which may be a fractional size
   in window scale coordinates. We need this to properly handle GL
   coordinates which are y-flipped in the real coordinates. */
void
gdk_window_get_unscaled_size (GdkWindow *window,
                              int *unscaled_width,
                              int *unscaled_height)
{
  GdkWindowImplClass *impl_class;
  gint scale;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->impl_window == window)
    {
      impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

      if (impl_class->get_unscaled_size)
        {
          impl_class->get_unscaled_size (window, unscaled_width, unscaled_height);
          return;
        }
    }

  scale = gdk_window_get_scale_factor (window);

  if (unscaled_width)
    *unscaled_width = window->width * scale;

  if (unscaled_height)
    *unscaled_height = window->height * scale;
}


/**
 * gdk_window_set_opaque_region:
 * @window: a top-level or non-native #GdkWindow
 * @region: (allow-none):  a region, or %NULL
 *
 * For optimisation purposes, compositing window managers may
 * like to not draw obscured regions of windows, or turn off blending
 * during for these regions. With RGB windows with no transparency,
 * this is just the shape of the window, but with ARGB32 windows, the
 * compositor does not know what regions of the window are transparent
 * or not.
 *
 * This function only works for toplevel windows.
 *
 * GTK+ will update this property automatically if
 * the @window background is opaque, as we know where the opaque regions
 * are. If your window background is not opaque, please update this
 * property in your #GtkWidget::style-updated handler.
 *
 * Since: 3.10
 */
void
gdk_window_set_opaque_region (GdkWindow      *window,
                              cairo_region_t *region)
{
  GdkWindowImplClass *impl_class;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (!GDK_WINDOW_DESTROYED (window));

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

  if (impl_class->set_opaque_region)
    impl_class->set_opaque_region (window, region);
}

/**
 * gdk_window_set_shadow_width:
 * @window: a #GdkWindow
 * @left: The left extent
 * @right: The right extent
 * @top: The top extent
 * @bottom: The bottom extent
 *
 * Newer GTK+ windows using client-side decorations use extra geometry
 * around their frames for effects like shadows and invisible borders.
 * Window managers that want to maximize windows or snap to edges need
 * to know where the extents of the actual frame lie, so that users
 * don’t feel like windows are snapping against random invisible edges.
 *
 * Note that this property is automatically updated by GTK+, so this
 * function should only be used by applications which do not use GTK+
 * to create toplevel windows.
 *
 * Since: 3.12
 */
void
gdk_window_set_shadow_width (GdkWindow *window,
                             gint       left,
                             gint       right,
                             gint       top,
                             gint       bottom)
{
  GdkWindowImplClass *impl_class;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (!GDK_WINDOW_DESTROYED (window));
  g_return_if_fail (left >= 0 && right >= 0 && top >= 0 && bottom >= 0);

  window->shadow_top = top;
  window->shadow_left = left;
  window->shadow_right = right;
  window->shadow_bottom = bottom;

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

  if (impl_class->set_shadow_width)
    impl_class->set_shadow_width (window, left, right, top, bottom);
}

/**
 * gdk_window_show_window_menu:
 * @window: a #GdkWindow
 * @event: a #GdkEvent to show the menu for
 *
 * Asks the windowing system to show the window menu. The window menu
 * is the menu shown when right-clicking the titlebar on traditional
 * windows managed by the window manager. This is useful for windows
 * using client-side decorations, activating it with a right-click
 * on the window decorations.
 *
 * Returns: %TRUE if the window menu was shown and %FALSE otherwise.
 *
 * Since: 3.14
 */
gboolean
gdk_window_show_window_menu (GdkWindow *window,
                             GdkEvent  *event)
{
  GdkWindowImplClass *impl_class;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (!GDK_WINDOW_DESTROYED (window), FALSE);

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

  if (impl_class->show_window_menu)
    return impl_class->show_window_menu (window, event);
  else
    return FALSE;
}
