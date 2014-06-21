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

#include <math.h>

/* for the use of round() */
#include "fallback-c89.c"

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
 *
 * # Composited Windows # {#COMPOSITED-WINDOWS}
 *
 * Normally, the windowing system takes care of rendering the contents
 * of a child window onto its parent window. This mechanism can be
 * intercepted by calling gdk_window_set_composited() on the child
 * window. For a “composited” window it is the
 * responsibility of the application to render the window contents at
 * the right spot.
 *
 * # Offscreen Windows # {#OFFSCREEN-WINDOWS}
 *
 * Offscreen windows are more general than composited windows, since
 * they allow not only to modify the rendering of the child window onto
 * its parent, but also to apply coordinate transformations.
 *
 * To integrate an offscreen window into a window hierarchy, one has
 * to call gdk_offscreen_window_set_embedder() and handle a number of
 * signals. The #GdkWindow::pick-embedded-child signal on the embedder
 * window is used to select an offscreen child at given coordinates,
 * and the #GdkWindow::to-embedder and #GdkWindow::from-embedder signals
 * on the offscreen window are used to translate coordinates between
 * the embedder and the offscreen window.
 *
 * For rendering an offscreen window onto its embedder, the contents
 * of the offscreen window are available as a surface, via
 * gdk_offscreen_window_get_surface().
 */


/* Historically a GdkWindow always matches a platform native window,
 * be it a toplevel window or a child window. In this setup the
 * GdkWindow (and other GdkDrawables) were platform independent classes,
 * and the actual platform specific implementation was in a delegate
 * object available as “impl” in the window object.
 *
 * With the addition of client side windows and offscreen windows this
 * changes a bit. The application-visible GdkWindow object behaves as
 * it did before, but not all such windows now have a corresponding native
 * window. Instead windows that are “client side” are emulated by the gdk
 * code such that clipping, drawing, moving, events etc work as expected.
 *
 * For GdkWindows that have a native window the “impl” object is the
 * same as before. However, for all client side windows the impl object
 * is shared with its parent (i.e. all client windows descendants of one
 * native window has the same impl.
 *
 * Additionally there is a new type of platform independent impl object,
 * GdkOffscreenWindow. All windows of type GDK_WINDOW_OFFSCREEN get an impl
 * of this type (while their children are generally GDK_WINDOW_CHILD virtual
 * windows). Such windows work by allocating a #cairo_surface_t as the backing
 * store for drawing operations, which is resized with the window.
 *
 * GdkWindows have a pointer to the “impl window” they are in, i.e.
 * the topmost GdkWindow which have the same “impl” value. This is stored
 * in impl_window, which is different from the window itself only for client
 * side windows.
 * All GdkWindows (native or not) track the position of the window in the parent
 * (x, y), the size of the window (width, height), the position of the window
 * with respect to the impl window (abs_x, abs_y). We also track the clip
 * region of the window wrt parent windows, in window-relative coordinates (clip_region).
 *
 * All toplevel windows are native windows, but also child windows can be
 * native (although not children of offscreens). We always listen to
 * a basic set of events (see get_native_event_mask) for these windows
 * so that we can emulate events for any client side children.
 *
 * For native windows we apply the calculated clip region as a window shape
 * so that eg. client side siblings that overlap the native child properly
 * draws over the native child window.
 */

/* This adds a local value to the GdkVisibilityState enum */
#define GDK_VISIBILITY_NOT_VIEWABLE 3

enum {
  PICK_EMBEDDED_CHILD, /* only called if children are embedded */
  TO_EMBEDDER,
  FROM_EMBEDDER,
  CREATE_SURFACE,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_CURSOR
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

static void gdk_window_clear_backing_region (GdkWindow *window,
					     cairo_region_t *region);

static void recompute_visible_regions   (GdkWindow *private,
					 gboolean recalculate_children);
static void gdk_window_invalidate_in_parent (GdkWindow *private);
static void move_native_children        (GdkWindow *private);
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

static gpointer parent_class = NULL;

static const cairo_user_data_key_t gdk_window_cairo_key;

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

  window->device_cursor = g_hash_table_new_full (NULL, NULL,
                                                 NULL, g_object_unref);
}

/* Stop and return on the first non-NULL parent */
static gboolean
accumulate_get_window (GSignalInvocationHint *ihint,
		       GValue		       *return_accu,
		       const GValue	       *handler_return,
		       gpointer               data)
{
  g_value_copy (handler_return, return_accu);
  /* Continue while returning NULL */
  return g_value_get_object (handler_return) == NULL;
}

static gboolean
create_surface_accumulator (GSignalInvocationHint *ihint,
                            GValue                *return_accu,
                            const GValue          *handler_return,
                            gpointer               data)
{
  g_value_copy (handler_return, return_accu);

  /* Stop on the first non-NULL return value */
  return g_value_get_boxed (handler_return) == NULL;
}

static GQuark quark_pointer_window = 0;

static void
gdk_window_class_init (GdkWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_window_finalize;
  object_class->set_property = gdk_window_set_property;
  object_class->get_property = gdk_window_get_property;

  klass->create_surface = _gdk_offscreen_window_create_surface;

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
  g_object_class_install_property (object_class,
                                   PROP_CURSOR,
                                   g_param_spec_object ("cursor",
                                                        P_("Cursor"),
                                                        P_("Cursor"),
                                                        GDK_TYPE_CURSOR,
                                                        G_PARAM_READWRITE));

  /**
   * GdkWindow::pick-embedded-child:
   * @window: the window on which the signal is emitted
   * @x: x coordinate in the window
   * @y: y coordinate in the window
   *
   * The ::pick-embedded-child signal is emitted to find an embedded
   * child at the given position.
   *
   * Returns: (nullable) (transfer none): the #GdkWindow of the
   *     embedded child at @x, @y, or %NULL
   *
   * Since: 2.18
   */
  signals[PICK_EMBEDDED_CHILD] =
    g_signal_new (g_intern_static_string ("pick-embedded-child"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkWindowClass, pick_embedded_child),
		  accumulate_get_window, NULL,
		  _gdk_marshal_OBJECT__DOUBLE_DOUBLE,
		  GDK_TYPE_WINDOW,
		  2,
		  G_TYPE_DOUBLE,
		  G_TYPE_DOUBLE);

  /**
   * GdkWindow::to-embedder:
   * @window: the offscreen window on which the signal is emitted
   * @offscreen_x: x coordinate in the offscreen window
   * @offscreen_y: y coordinate in the offscreen window
   * @embedder_x: (out) (type double): return location for the x
   *     coordinate in the embedder window
   * @embedder_y: (out) (type double): return location for the y
   *     coordinate in the embedder window
   *
   * The ::to-embedder signal is emitted to translate coordinates
   * in an offscreen window to its embedder.
   *
   * See also #GdkWindow::from-embedder.
   *
   * Since: 2.18
   */
  signals[TO_EMBEDDER] =
    g_signal_new (g_intern_static_string ("to-embedder"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkWindowClass, to_embedder),
		  NULL, NULL,
		  _gdk_marshal_VOID__DOUBLE_DOUBLE_POINTER_POINTER,
		  G_TYPE_NONE,
		  4,
		  G_TYPE_DOUBLE,
		  G_TYPE_DOUBLE,
		  G_TYPE_POINTER,
		  G_TYPE_POINTER);

  /**
   * GdkWindow::from-embedder:
   * @window: the offscreen window on which the signal is emitted
   * @embedder_x: x coordinate in the embedder window
   * @embedder_y: y coordinate in the embedder window
   * @offscreen_x: (out) (type double): return location for the x
   *     coordinate in the offscreen window
   * @offscreen_y: (out) (type double): return location for the y
   *     coordinate in the offscreen window
   *
   * The ::from-embedder signal is emitted to translate coordinates
   * in the embedder of an offscreen window to the offscreen window.
   *
   * See also #GdkWindow::to-embedder.
   *
   * Since: 2.18
   */
  signals[FROM_EMBEDDER] =
    g_signal_new (g_intern_static_string ("from-embedder"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkWindowClass, from_embedder),
		  NULL, NULL,
		  _gdk_marshal_VOID__DOUBLE_DOUBLE_POINTER_POINTER,
		  G_TYPE_NONE,
		  4,
		  G_TYPE_DOUBLE,
		  G_TYPE_DOUBLE,
		  G_TYPE_POINTER,
		  G_TYPE_POINTER);

  /**
   * GdkWindow::create-surface:
   * @window: the offscreen window on which the signal is emitted
   * @width: the width of the offscreen surface to create
   * @height: the height of the offscreen surface to create
   *
   * The ::create-surface signal is emitted when an offscreen window
   * needs its surface (re)created, which happens either when the the
   * window is first drawn to, or when the window is being
   * resized. The first signal handler that returns a non-%NULL
   * surface will stop any further signal emission, and its surface
   * will be used.
   *
   * Note that it is not possible to access the window's previous
   * surface from within any callback of this signal. Calling
   * gdk_offscreen_window_get_surface() will lead to a crash.
   *
   * Returns: the newly created #cairo_surface_t for the offscreen window
   *
   * Since: 3.0
   */
  signals[CREATE_SURFACE] =
    g_signal_new (g_intern_static_string ("create-surface"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkWindowClass, create_surface),
                  create_surface_accumulator, NULL,
                  _gdk_marshal_BOXED__INT_INT,
                  CAIRO_GOBJECT_TYPE_SURFACE,
                  2,
                  G_TYPE_INT,
                  G_TYPE_INT);
}

static void
device_removed_cb (GdkDeviceManager *device_manager,
                   GdkDevice        *device,
                   GdkWindow        *window)
{
  window->devices_inside = g_list_remove (window->devices_inside, device);
  g_hash_table_remove (window->device_cursor, device);

  if (window->device_events)
    g_hash_table_remove (window->device_events, device);
}

static void
gdk_window_finalize (GObject *object)
{
  GdkWindow *window = GDK_WINDOW (object);
  GdkDeviceManager *device_manager;

  device_manager = gdk_display_get_device_manager (gdk_window_get_display (window));
  g_signal_handlers_disconnect_by_func (device_manager, device_removed_cb, window);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN)
	{
	  g_warning ("losing last reference to undestroyed window\n");
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

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_window_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GdkWindow *window = (GdkWindow *)object;

  switch (prop_id)
    {
    case PROP_CURSOR:
      gdk_window_set_cursor (window, g_value_get_object (value));
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
  GdkWindow *window = (GdkWindow *) object;

  switch (prop_id)
    {
    case PROP_CURSOR:
      g_value_set_object (value, gdk_window_get_cursor (window));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
gdk_window_is_offscreen (GdkWindow *window)
{
  return window->window_type == GDK_WINDOW_OFFSCREEN;
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
  cairo_region_t *shape;

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

      if (!GDK_WINDOW_IS_MAPPED (sibling) || sibling->input_only || sibling->composited)
	continue;

      /* Ignore offscreen children, as they don't draw in their parent and
       * don't take part in the clipping */
      if (gdk_window_is_offscreen (sibling))
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
      else if (window->window_type == GDK_WINDOW_FOREIGN)
	{
	  shape = GDK_WINDOW_IMPL_GET_CLASS (sibling)->get_shape (sibling);
	  if (shape)
	    {
	      cairo_region_intersect (child_region, shape);
	      cairo_region_destroy (shape);
	    }
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
  cairo_region_t *shape;

  for (l = window->children; l; l = l->next)
    {
      child = l->data;

      /* If region is empty already, no need to do
	 anything potentially costly */
      if (cairo_region_is_empty (region))
	break;

      if (!GDK_WINDOW_IS_MAPPED (child) || child->input_only || child->composited)
	continue;

      /* Ignore offscreen children, as they don't draw in their parent and
       * don't take part in the clipping */
      if (gdk_window_is_offscreen (child))
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
      else if (window->window_type == GDK_WINDOW_FOREIGN)
	{
	  shape = GDK_WINDOW_IMPL_GET_CLASS (child)->get_shape (child);
	  if (shape)
	    {
	      cairo_region_intersect (child_region, shape);
	      cairo_region_destroy (shape);
	    }
	}

      if (for_input)
	{
	  if (child->input_shape)
	    cairo_region_intersect (child_region, child->input_shape);
	  else if (window->window_type == GDK_WINDOW_FOREIGN)
	    {
	      shape = GDK_WINDOW_IMPL_GET_CLASS (child)->get_input_shape (child);
	      if (shape)
		{
		  cairo_region_intersect (child_region, shape);
		  cairo_region_destroy (shape);
		}
	    }
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
    /* Not for offscreens */
    !gdk_window_is_offscreen (window) &&
    /* or for non-shaped toplevels */
    (!gdk_window_is_toplevel (window) ||
     window->shape != NULL || window->applied_shape) &&
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
  if (gdk_window_has_impl (private))
    {
      /* Native window starts here */
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

void
_gdk_window_update_size (GdkWindow *window)
{
  recompute_visible_regions (window, FALSE);
}

/* Find the native window that would be just above "child"
 * in the native stacking order if “child” was a native window
 * (it doesn’t have to be native). If there is no such native
 * window inside this native parent then NULL is returned.
 * If child is NULL, find lowest native window in parent.
 */
static GdkWindow *
find_native_sibling_above_helper (GdkWindow *parent,
				  GdkWindow *child)
{
  GdkWindow *w;
  GList *l;

  if (child)
    {
      l = g_list_find (parent->children, child);
      g_assert (l != NULL); /* Better be a child of its parent... */
      l = l->prev; /* Start looking at the one above the child */
    }
  else
    l = g_list_last (parent->children);

  for (; l != NULL; l = l->prev)
    {
      w = l->data;

      if (gdk_window_has_impl (w))
	return w;

      g_assert (parent != w);
      w = find_native_sibling_above_helper (w, NULL);
      if (w)
	return w;
    }

  return NULL;
}


static GdkWindow *
find_native_sibling_above (GdkWindow *parent,
			   GdkWindow *child)
{
  GdkWindow *w;

  w = find_native_sibling_above_helper (parent, child);
  if (w)
    return w;

  if (gdk_window_has_impl (parent))
    return NULL;
  else
    return find_native_sibling_above (parent->parent, parent);
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
	GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK;

      /* Additionally we select for pointer and button events
       * for toplevels as we need to get these to emulate
       * them for non-native subwindows. Even though we don't
       * select on them for all native windows we will get them
       * as the events are propagated out to the first window
       * that select for them.
       * Not selecting for button press on all windows is an
       * important thing, because in X only one client can do
       * so, and we don't want to unexpectedly prevent another
       * client from doing it.
       *
       * We also need to do the same if the app selects for button presses
       * because then we will get implicit grabs for this window, and the
       * event mask used for that grab is based on the rest of the mask
       * for the window, but we might need more events than this window
       * lists due to some non-native child window.
       */
      if (gdk_window_is_toplevel (private) ||
          mask & GDK_BUTTON_PRESS_MASK)
        mask |=
          GDK_TOUCH_MASK |
          GDK_POINTER_MOTION_MASK |
          GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
          GDK_SCROLL_MASK;

      return mask;
    }
}

static GdkEventMask
get_native_grab_event_mask (GdkEventMask grab_mask)
{
  /* Similar to the above but for pointer events only */
  return
    GDK_POINTER_MOTION_MASK |
    GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
    GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
    GDK_SCROLL_MASK |
    (grab_mask &
     ~GDK_POINTER_MOTION_HINT_MASK);
}

static GdkEventMask
get_native_event_mask (GdkWindow *private)
{
  return get_native_device_event_mask (private, NULL);
}

/* Puts the native window in the right order wrt the other native windows
 * in the hierarchy, given the position it has in the client side data.
 * This is useful if some operation changed the stacking order.
 * This calls assumes the native window is now topmost in its native parent.
 */
static void
sync_native_window_stack_position (GdkWindow *window)
{
  GdkWindow *above;
  GdkWindowImplClass *impl_class;
  GList listhead = {0};

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

  above = find_native_sibling_above (window->parent, window);
  if (above)
    {
      listhead.data = window;
      impl_class->restack_under (above, &listhead);
    }
}

/**
 * gdk_window_new: (constructor)
 * @parent: (allow-none): a #GdkWindow, or %NULL to create the window as a child of
 *   the default root window for the default display.
 * @attributes: attributes of the new window
 * @attributes_mask: (type GdkWindowAttributesType): mask indicating which
 *   fields in @attributes are valid
 *
 * Creates a new #GdkWindow using the attributes from
 * @attributes. See #GdkWindowAttr and #GdkWindowAttributesType for
 * more details.  Note: to use this on displays other than the default
 * display, @parent must be specified.
 *
 * Returns: (transfer full): the new #GdkWindow
 **/
GdkWindow*
gdk_window_new (GdkWindow     *parent,
		GdkWindowAttr *attributes,
		gint           attributes_mask)
{
  GdkWindow *window;
  GdkScreen *screen;
  GdkDisplay *display;
  int x, y;
  gboolean native;
  GdkEventMask event_mask;
  GdkWindow *real_parent;
  GdkDeviceManager *device_manager;

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
      g_warning ("gdk_window_new(): parent is destroyed\n");
      return NULL;
    }

  if (attributes_mask & GDK_WA_VISUAL)
    {
      g_return_val_if_fail (gdk_visual_get_screen (attributes->visual) == screen, NULL);
    }

  display = gdk_screen_get_display (screen);

  window = _gdk_display_create_window (display);

  /* Windows with a foreign parent are treated as if they are children
   * of the root window, except for actual creation.
   */
  real_parent = parent;
  if (GDK_WINDOW_TYPE (parent) == GDK_WINDOW_FOREIGN)
    parent = gdk_screen_get_root_window (screen);

  window->parent = parent;

  window->accept_focus = TRUE;
  window->focus_on_map = TRUE;
  window->event_compression = TRUE;

  if (attributes_mask & GDK_WA_X)
    x = attributes->x;
  else
    x = 0;

  if (attributes_mask & GDK_WA_Y)
    y = attributes->y;
  else
    y = 0;

  window->x = x;
  window->y = y;
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
    case GDK_WINDOW_OFFSCREEN:
      if (GDK_WINDOW_TYPE (parent) != GDK_WINDOW_ROOT)
	g_warning (G_STRLOC "Toplevel windows must be created as children of\n"
		   "of a window of type GDK_WINDOW_ROOT or GDK_WINDOW_FOREIGN");
    case GDK_WINDOW_CHILD:
      break;
      break;
    default:
      g_warning (G_STRLOC "cannot make windows of type %d", window->window_type);
      return NULL;
    }

  if (attributes_mask & GDK_WA_VISUAL)
    window->visual = attributes->visual;
  else
    window->visual = gdk_screen_get_system_visual (screen);

  window->event_mask = attributes->event_mask;

  if (attributes->wclass == GDK_INPUT_OUTPUT)
    {
      window->input_only = FALSE;
      window->depth = window->visual->depth;

      /* XXX: Cache this somehow? */
      window->background = cairo_pattern_create_rgba (0, 0, 0, 0);
    }
  else
    {
      window->depth = 0;
      window->input_only = TRUE;
    }

  if (window->parent)
    window->parent->children = g_list_prepend (window->parent->children, window);

  if (window->parent->window_type == GDK_WINDOW_ROOT)
    {
      GdkFrameClock *frame_clock = g_object_new (GDK_TYPE_FRAME_CLOCK_IDLE, NULL);
      gdk_window_set_frame_clock (window, frame_clock);
      g_object_unref (frame_clock);
    }

  native = FALSE;
  if (window->parent->window_type == GDK_WINDOW_ROOT)
    native = TRUE; /* Always use native windows for toplevels */

  if (gdk_window_is_offscreen (window))
    {
      _gdk_offscreen_window_new (window, attributes, attributes_mask);
      window->impl_window = window;
    }
  else if (native)
    {
      event_mask = get_native_event_mask (window);

      /* Create the impl */
      _gdk_display_create_window_impl (display, window, real_parent, screen, event_mask, attributes, attributes_mask);
      window->impl_window = window;

      if (parent)
        parent->impl_window->native_children = g_list_prepend (parent->impl_window->native_children, window);

      /* This will put the native window topmost in the native parent, which may
       * be wrong wrt other native windows in the non-native hierarchy, so restack */
      if (!_gdk_window_has_impl (real_parent))
	sync_native_window_stack_position (window);
    }
  else
    {
      window->impl_window = g_object_ref (window->parent->impl_window);
      window->impl = g_object_ref (window->impl_window->impl);
    }

  recompute_visible_regions (window, FALSE);

  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));

  device_manager = gdk_display_get_device_manager (gdk_window_get_display (parent));
  g_signal_connect (device_manager, "device-removed",
                    G_CALLBACK (device_removed_cb), window);

  return window;
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

static void
change_impl (GdkWindow *private,
	     GdkWindow *impl_window,
	     GdkWindowImpl *new)
{
  GList *l;
  GdkWindow *child;
  GdkWindowImpl *old_impl;
  GdkWindow *old_impl_window;

  old_impl = private->impl;
  old_impl_window = private->impl_window;
  if (private != impl_window)
    private->impl_window = g_object_ref (impl_window);
  else
    private->impl_window = private;
  private->impl = g_object_ref (new);
  if (old_impl_window != private)
    g_object_unref (old_impl_window);
  g_object_unref (old_impl);

  for (l = private->children; l != NULL; l = l->next)
    {
      child = l->data;

      if (child->impl == old_impl)
	change_impl (child, impl_window, new);
      else
        {
          /* The child is a native, update native_children */
          old_impl_window->native_children =
            g_list_remove (old_impl_window->native_children, child);
          impl_window->native_children =
            g_list_prepend (impl_window->native_children, child);
        }
    }
}

static void
reparent_to_impl (GdkWindow *private)
{
  GList *l;
  GdkWindow *child;
  gboolean show;
  GdkWindowImplClass *impl_class;

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (private->impl);

  /* Enumerate in reverse order so we get the right order for the native
     windows (first in childrens list is topmost, and reparent places on top) */
  for (l = g_list_last (private->children); l != NULL; l = l->prev)
    {
      child = l->data;

      if (child->impl == private->impl)
	reparent_to_impl (child);
      else
	{
	  show = impl_class->reparent ((GdkWindow *)child,
				       (GdkWindow *)private,
				       child->x, child->y);
	  if (show)
	    gdk_window_show_unraised ((GdkWindow *)child);
	}
    }
}


/**
 * gdk_window_reparent:
 * @window: a #GdkWindow
 * @new_parent: new parent to move @window into
 * @x: X location inside the new parent
 * @y: Y location inside the new parent
 *
 * Reparents @window into the given @new_parent. The window being
 * reparented will be unmapped as a side effect.
 *
 **/
void
gdk_window_reparent (GdkWindow *window,
		     GdkWindow *new_parent,
		     gint       x,
		     gint       y)
{
  GdkWindow *old_parent;
  GdkScreen *screen;
  gboolean show, was_mapped;
  gboolean do_reparent_to_impl;
  GdkEventMask old_native_event_mask;
  GdkWindowImplClass *impl_class;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (new_parent == NULL || GDK_IS_WINDOW (new_parent));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_ROOT);

  if (GDK_WINDOW_DESTROYED (window) ||
      (new_parent && GDK_WINDOW_DESTROYED (new_parent)))
    return;

  screen = gdk_window_get_screen (window);
  if (!new_parent)
    new_parent = gdk_screen_get_root_window (screen);

  /* No input-output children of input-only windows */
  if (new_parent->input_only && !window->input_only)
    return;

  /* Don't create loops in hierarchy */
  if (is_parent_of (window, new_parent))
    return;

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
  old_parent = window->parent;

  was_mapped = GDK_WINDOW_IS_MAPPED (window);

  /* Reparenting to toplevel. Ensure we have a native window so this can work */
  if (new_parent->window_type == GDK_WINDOW_ROOT ||
      new_parent->window_type == GDK_WINDOW_FOREIGN)
    gdk_window_ensure_native (window);

  old_native_event_mask = 0;
  do_reparent_to_impl = FALSE;
  if (gdk_window_has_impl (window))
    {
      old_native_event_mask = get_native_event_mask (window);
      /* Native window */
      show = impl_class->reparent (window, new_parent, x, y);
    }
  else
    {
      /* This shouldn't happen, as we created a native in this case, check anyway to see if that ever fails */
      g_assert (new_parent->window_type != GDK_WINDOW_ROOT &&
		new_parent->window_type != GDK_WINDOW_FOREIGN);

      show = was_mapped;
      gdk_window_hide (window);

      do_reparent_to_impl = TRUE;
      change_impl (window,
		   new_parent->impl_window,
		   new_parent->impl);
    }

  /* From here on, we treat parents of type GDK_WINDOW_FOREIGN like
   * the root window
   */
  if (GDK_WINDOW_TYPE (new_parent) == GDK_WINDOW_FOREIGN)
    {
      new_parent = gdk_screen_get_root_window (screen);
    }

  if (old_parent)
    {
      old_parent->children = g_list_remove (old_parent->children, window);

      if (gdk_window_has_impl (window))
        old_parent->impl_window->native_children =
          g_list_remove (old_parent->impl_window->native_children, window);
    }

  window->parent = new_parent;
  window->x = x;
  window->y = y;

  new_parent->children = g_list_prepend (new_parent->children, window);

  if (gdk_window_has_impl (window))
    new_parent->impl_window->native_children = g_list_prepend (new_parent->impl_window->native_children, window);

  /* Switch the window type as appropriate */

  switch (GDK_WINDOW_TYPE (new_parent))
    {
    case GDK_WINDOW_ROOT:
    case GDK_WINDOW_FOREIGN:
      if (window->toplevel_window_type != -1)
	GDK_WINDOW_TYPE (window) = window->toplevel_window_type;
      else if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD)
	GDK_WINDOW_TYPE (window) = GDK_WINDOW_TOPLEVEL;
      break;
    case GDK_WINDOW_OFFSCREEN:
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_CHILD:
    case GDK_WINDOW_TEMP:
      if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD && \
	  GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN)
	{
	  /* Save the original window type so we can restore it if the
	   * window is reparented back to be a toplevel
	   */
	  window->toplevel_window_type = GDK_WINDOW_TYPE (window);
	  GDK_WINDOW_TYPE (window) = GDK_WINDOW_CHILD;
	}
    }

  /* If we changed the window type, we might have to set or
   * unset the frame clock on the window
   */
  if (GDK_WINDOW_TYPE (new_parent) == GDK_WINDOW_ROOT &&
      GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN)
    {
      if (window->frame_clock == NULL)
        {
          GdkFrameClock *frame_clock = g_object_new (GDK_TYPE_FRAME_CLOCK_IDLE, NULL);
          gdk_window_set_frame_clock (window, frame_clock);
          g_object_unref (frame_clock);
        }
    }
  else
    {
      if (window->frame_clock != NULL)
        {
          g_object_run_dispose (G_OBJECT (window->frame_clock));
          gdk_window_set_frame_clock (window, NULL);
        }
    }

  /* We might have changed window type for a native windows, so we
     need to change the event mask too. */
  if (gdk_window_has_impl (window))
    {
      GdkEventMask native_event_mask = get_native_event_mask (window);

      if (native_event_mask != old_native_event_mask)
	impl_class->set_events (window,	native_event_mask);
    }

  _gdk_window_update_viewable (window);

  recompute_visible_regions (window, FALSE);

  if (do_reparent_to_impl)
    reparent_to_impl (window);
  else
    {
      /* The reparent will have put the native window topmost in the native parent,
       * which may be wrong wrt other native windows in the non-native hierarchy,
       * so restack */
      if (!gdk_window_has_impl (new_parent))
	sync_native_window_stack_position (window);
    }

  if (show)
    gdk_window_show_unraised (window);
  else
    _gdk_synthesize_crossing_events_for_geometry_change (window);
}

/**
 * gdk_window_ensure_native:
 * @window: a #GdkWindow
 *
 * Tries to ensure that there is a window-system native window for this
 * GdkWindow. This may fail in some situations, returning %FALSE.
 *
 * Offscreen window and children of them can never have native windows.
 *
 * Some backends may not support native child windows.
 *
 * Returns: %TRUE if the window has a native window, %FALSE otherwise
 *
 * Since: 2.18
 */
gboolean
gdk_window_ensure_native (GdkWindow *window)
{
  GdkWindow *impl_window;
  GdkWindowImpl *new_impl, *old_impl;
  GdkDisplay *display;
  GdkScreen *screen;
  GdkWindow *above, *parent;
  GList listhead;
  GdkWindowImplClass *impl_class;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_ROOT ||
      GDK_WINDOW_DESTROYED (window))
    return FALSE;

  impl_window = gdk_window_get_impl_window (window);

  if (gdk_window_is_offscreen (impl_window))
    return FALSE; /* native in offscreens not supported */

  if (impl_window == window)
    /* Already has an impl, and its not offscreen . */
    return TRUE;

  /* Need to create a native window */

  screen = gdk_window_get_screen (window);
  display = gdk_screen_get_display (screen);
  parent = window->parent;

  old_impl = window->impl;
  _gdk_display_create_window_impl (display,
                                   window, parent,
                                   screen,
                                   get_native_event_mask (window),
                                   NULL, 0);
  new_impl = window->impl;

  if (parent)
    parent->impl_window->native_children =
      g_list_prepend (parent->impl_window->native_children, window);

  window->impl = old_impl;
  change_impl (window, window, new_impl);

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

  /* Native window creation will put the native window topmost in the
   * native parent, which may be wrong wrt the position of the previous
   * non-native window wrt to the other non-native children, so correct this.
   */
  above = find_native_sibling_above (parent, window);
  if (above)
    {
      listhead.data = window;
      listhead.prev = NULL;
      listhead.next = NULL;
      impl_class->restack_under ((GdkWindow *)above, &listhead);
    }

  recompute_visible_regions (window, FALSE);

  reparent_to_impl (window);

  if (!window->input_only)
    impl_class->set_background (window, window->background);

  impl_class->input_shape_combine_region (window,
                                          window->input_shape,
                                          0, 0);

  if (gdk_window_is_viewable (window))
    impl_class->show (window, FALSE);

  gdk_window_invalidate_in_parent (window);

  return TRUE;
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
  GList *children;
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
    case GDK_WINDOW_OFFSCREEN:
      if (window->window_type == GDK_WINDOW_FOREIGN && !foreign_destroy)
	{
	  /* Logically, it probably makes more sense to send
	   * a "destroy yourself" message to the foreign window
	   * whether or not it's in our hierarchy; but for historical
	   * reasons, we only send "destroy yourself" messages to
	   * foreign windows in our hierarchy.
	   */
	  if (window->parent)
            {
              impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

              if (gdk_window_has_impl (window))
                impl_class->destroy_foreign (window);
            }

	  /* Also for historical reasons, we remove any filters
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
		window->parent->children = g_list_remove (window->parent->children, window);

              if (gdk_window_has_impl (window))
                window->parent->impl_window->native_children =
                  g_list_remove (window->parent->impl_window->native_children, window);

	      if (!recursing &&
		  GDK_WINDOW_IS_MAPPED (window))
		{
		  recompute_visible_regions (window, FALSE);
		  gdk_window_invalidate_in_parent (window);
		}
	    }

          if (window->frame_clock)
            {
              g_object_run_dispose (G_OBJECT (window->frame_clock));
              gdk_window_set_frame_clock (window, NULL);
            }

          gdk_window_free_current_paint (window);

          if (window->background)
            {
              cairo_pattern_destroy (window->background);
              window->background = NULL;
            }

	  if (window->window_type == GDK_WINDOW_FOREIGN)
	    g_assert (window->children == NULL);
	  else
	    {
	      children = tmp = window->children;
	      window->children = NULL;

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

	      g_list_free (children);

              if (gdk_window_has_impl (window))
                g_assert (window->native_children == NULL);
	    }

	  _gdk_window_clear_update_area (window);

	  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

	  if (gdk_window_has_impl (window))
	    impl_class->destroy (window, recursing_native,
				 foreign_destroy);
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
  g_return_if_fail (GDK_IS_WINDOW (window));

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
 * gdk_window_get_visual:
 * @window: a #GdkWindow
 * 
 * Gets the #GdkVisual describing the pixel format of @window.
 * 
 * Returns: (transfer none): a #GdkVisual
 *
 * Since: 2.24
 **/
GdkVisual*
gdk_window_get_visual (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
  
  return window->visual;
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

  return gdk_visual_get_screen (window->visual);
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

  return gdk_screen_get_display (gdk_visual_get_screen (window->visual));
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

static void
to_embedder (GdkWindow *window,
             gdouble    offscreen_x,
             gdouble    offscreen_y,
             gdouble   *embedder_x,
             gdouble   *embedder_y)
{
  g_signal_emit (window, signals[TO_EMBEDDER], 0,
                 offscreen_x, offscreen_y,
                 embedder_x, embedder_y);
}

static void
from_embedder (GdkWindow *window,
               gdouble    embedder_x,
               gdouble    embedder_y,
               gdouble   *offscreen_x,
               gdouble   *offscreen_y)
{
  g_signal_emit (window, signals[FROM_EMBEDDER], 0,
                 embedder_x, embedder_y,
                 offscreen_x, offscreen_y);
}

/**
 * gdk_window_has_native:
 * @window: a #GdkWindow
 *
 * Checks whether the window has a native window or not. Note that
 * you can use gdk_window_ensure_native() if a native window is needed.
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
 * Note that you should use gdk_window_get_effective_parent() when
 * writing generic code that walks up a window hierarchy, because
 * gdk_window_get_parent() will most likely not do what you expect if
 * there are offscreen windows in the hierarchy.
 *
 * Returns: (transfer none): parent of @window
 **/
GdkWindow*
gdk_window_get_parent (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  return window->parent;
}

/**
 * gdk_window_get_effective_parent:
 * @window: a #GdkWindow
 *
 * Obtains the parent of @window, as known to GDK. Works like
 * gdk_window_get_parent() for normal windows, but returns the
 * window’s embedder for offscreen windows.
 *
 * See also: gdk_offscreen_window_get_embedder()
 *
 * Returns: (transfer none): effective parent of @window
 *
 * Since: 2.22
 **/
GdkWindow *
gdk_window_get_effective_parent (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (gdk_window_is_offscreen (window))
    return gdk_offscreen_window_get_embedder (window);
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
 * Note that you should use gdk_window_get_effective_toplevel() when
 * you want to get to a window’s toplevel as seen on screen, because
 * gdk_window_get_toplevel() will most likely not do what you expect
 * if there are offscreen windows in the hierarchy.
 *
 * Returns: (transfer none): the toplevel window containing @window
 **/
GdkWindow *
gdk_window_get_toplevel (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  while (window->window_type == GDK_WINDOW_CHILD)
    {
      if (gdk_window_is_toplevel (window))
	break;
      window = window->parent;
    }

  return window;
}

/**
 * gdk_window_get_effective_toplevel:
 * @window: a #GdkWindow
 *
 * Gets the toplevel window that’s an ancestor of @window.
 *
 * Works like gdk_window_get_toplevel(), but treats an offscreen window's
 * embedder as its parent, using gdk_window_get_effective_parent().
 *
 * See also: gdk_offscreen_window_get_embedder()
 *
 * Returns: (transfer none): the effective toplevel window containing @window
 *
 * Since: 2.22
 **/
GdkWindow *
gdk_window_get_effective_toplevel (GdkWindow *window)
{
  GdkWindow *parent;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  while ((parent = gdk_window_get_effective_parent (window)) != NULL &&
	 (gdk_window_get_window_type (parent) != GDK_WINDOW_ROOT))
    window = parent;

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
  if (window)
    gdk_window_ensure_native (window);

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

/**
 * gdk_window_begin_paint_rect:
 * @window: a #GdkWindow
 * @rectangle: rectangle you intend to draw to
 *
 * A convenience wrapper around gdk_window_begin_paint_region() which
 * creates a rectangular region for you. See
 * gdk_window_begin_paint_region() for details.
 *
 **/
void
gdk_window_begin_paint_rect (GdkWindow          *window,
			     const GdkRectangle *rectangle)
{
  cairo_region_t *region;

  g_return_if_fail (GDK_IS_WINDOW (window));

  region = cairo_region_create_rectangle (rectangle);
  gdk_window_begin_paint_region (window, region);
  cairo_region_destroy (region);
}

/**
 * gdk_window_begin_paint_region:
 * @window: a #GdkWindow
 * @region: region you intend to draw to
 *
 * Indicates that you are beginning the process of redrawing @region.
 * A backing store (offscreen buffer) large enough to contain @region
 * will be created. The backing store will be initialized with the
 * background color or background surface for @window. Then, all
 * drawing operations performed on @window will be diverted to the
 * backing store.  When you call gdk_window_end_paint(), the backing
 * store will be copied to @window, making it visible onscreen. Only
 * the part of @window contained in @region will be modified; that is,
 * drawing operations are clipped to @region.
 *
 * The net result of all this is to remove flicker, because the user
 * sees the finished product appear all at once when you call
 * gdk_window_end_paint(). If you draw to @window directly without
 * calling gdk_window_begin_paint_region(), the user may see flicker
 * as individual drawing operations are performed in sequence.  The
 * clipping and background-initializing features of
 * gdk_window_begin_paint_region() are conveniences for the
 * programmer, so you can avoid doing that work yourself.
 *
 * When using GTK+, the widget system automatically places calls to
 * gdk_window_begin_paint_region() and gdk_window_end_paint() around
 * emissions of the expose_event signal. That is, if you’re writing an
 * expose event handler, you can assume that the exposed area in
 * #GdkEventExpose has already been cleared to the window background,
 * is already set as the clip region, and already has a backing store.
 * Therefore in most cases, application code need not call
 * gdk_window_begin_paint_region().
 *
 * If you call this function multiple times before calling the
 * matching gdk_window_end_paint(), the backing stores are pushed onto
 * a stack. gdk_window_end_paint() copies the topmost backing store
 * onscreen, subtracts the topmost region from all other regions in
 * the stack, and pops the stack. All drawing operations affect only
 * the topmost backing store in the stack. One matching call to
 * gdk_window_end_paint() is required for each call to
 * gdk_window_begin_paint_region().
 *
 **/
void
gdk_window_begin_paint_region (GdkWindow       *window,
			       const cairo_region_t *region)
{
  GdkRectangle clip_box;
  GdkWindowImplClass *impl_class;
  double sx, sy;
  gboolean needs_surface;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !gdk_window_has_impl (window))
    return;

  if (window->current_paint.surface != NULL)
    {
      g_warning ("gdk_window_begin_paint_region called while a paint was "
                 "alredy in progress. This is not allowed.");
      return;
    }

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

  needs_surface = TRUE;
  if (impl_class->begin_paint_region)
    needs_surface = impl_class->begin_paint_region (window, region);

  window->current_paint.region = cairo_region_copy (region);

  cairo_region_intersect (window->current_paint.region, window->clip_region);
  cairo_region_get_extents (window->current_paint.region, &clip_box);

  if (needs_surface)
    {
      window->current_paint.surface = gdk_window_create_similar_surface (window,
                                                                         gdk_window_get_content (window),
                                                                         MAX (clip_box.width, 1),
                                                                         MAX (clip_box.height, 1));
      sx = sy = 1;
#ifdef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
      cairo_surface_get_device_scale (window->current_paint.surface, &sx, &sy);
#endif
      cairo_surface_set_device_offset (window->current_paint.surface, -clip_box.x*sx, -clip_box.y*sy);

      window->current_paint.surface_needs_composite = TRUE;
    }
  else
    {
      window->current_paint.surface = gdk_window_ref_impl_surface (window);
      window->current_paint.surface_needs_composite = FALSE;
    }

  if (!cairo_region_is_empty (window->current_paint.region))
    gdk_window_clear_backing_region (window, window->current_paint.region);
}

/**
 * gdk_window_end_paint:
 * @window: a #GdkWindow
 *
 * Indicates that the backing store created by the most recent call to
 * gdk_window_begin_paint_region() should be copied onscreen and
 * deleted, leaving the next-most-recent backing store or no backing
 * store at all as the active paint region. See
 * gdk_window_begin_paint_region() for full details. It is an error to
 * call this function without a matching
 * gdk_window_begin_paint_region() first.
 *
 **/
void
gdk_window_end_paint (GdkWindow *window)
{
  GdkWindow *composited;
  GdkWindowImplClass *impl_class;
  GdkRectangle clip_box = { 0, };
  cairo_region_t *full_clip;
  cairo_t *cr;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !gdk_window_has_impl (window))
    return;

  if (window->current_paint.surface == NULL)
    {
      g_warning (G_STRLOC": no preceding call to gdk_window_begin_paint_region(), see documentation");
      return;
    }

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

  if (impl_class->end_paint)
    impl_class->end_paint (window);

  if (window->current_paint.surface_needs_composite)
    {
      cairo_surface_t *surface;

      cairo_region_get_extents (window->current_paint.region, &clip_box);
      full_clip = cairo_region_copy (window->clip_region);
      cairo_region_intersect (full_clip, window->current_paint.region);

      surface = gdk_window_ref_impl_surface (window);
      cr = cairo_create (surface);
      cairo_surface_destroy (surface);

      cairo_set_source_surface (cr, window->current_paint.surface, 0, 0);
      gdk_cairo_region (cr, full_clip);
      cairo_clip (cr);
      if (gdk_window_has_impl (window) ||
	  window->alpha == 255)
	{
	  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	  cairo_paint (cr);
	}
      else
	{
	  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	  cairo_paint_with_alpha (cr, window->alpha / 255.0);
	}

      cairo_destroy (cr);
      cairo_region_destroy (full_clip);
    }

  gdk_window_free_current_paint (window);

  /* find a composited window in our hierarchy to signal its
   * parent to redraw, calculating the clip box as we go...
   *
   * stop if parent becomes NULL since then we'd have nowhere
   * to draw (ie: 'composited' will always be non-NULL here).
   */
  for (composited = window;
       composited->parent;
       composited = composited->parent)
    {
      clip_box.x += composited->x;
      clip_box.y += composited->y;
      clip_box.width = MIN (clip_box.width, composited->parent->width - clip_box.x);
      clip_box.height = MIN (clip_box.height, composited->parent->height - clip_box.y);

      if (composited->composited)
	{
	  gdk_window_invalidate_rect (GDK_WINDOW (composited->parent),
				      &clip_box, FALSE);
	  break;
	}
    }
}

/**
 * gdk_window_flush:
 * @window: a #GdkWindow
 *
 * This function does nothing.
 *
 * Since: 2.18
 *
 * Deprecated: 3.14
 **/
void
gdk_window_flush (GdkWindow *window)
{
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
gdk_window_clear_backing_region (GdkWindow *window,
				 cairo_region_t *region)
{
  cairo_region_t *clip;
  GdkWindow *bg_window;
  cairo_pattern_t *pattern = NULL;
  int x_offset = 0, y_offset = 0;
  cairo_t *cr;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  cr = cairo_create (window->current_paint.surface);

  for (bg_window = window; bg_window; bg_window = bg_window->parent)
    {
      pattern = gdk_window_get_background_pattern (bg_window);
      if (pattern)
        break;

      x_offset += bg_window->x;
      y_offset += bg_window->y;
    }

  if (pattern)
    {
      cairo_translate (cr, -x_offset, -y_offset);
      cairo_set_source (cr, pattern);
      cairo_translate (cr, x_offset, y_offset);
    }
  else
    cairo_set_source_rgb (cr, 0, 0, 0);

  clip = cairo_region_copy (window->current_paint.region);
  cairo_region_intersect (clip, region);

  gdk_cairo_region (cr, clip);
  cairo_fill (cr);

  cairo_destroy (cr);

  cairo_region_destroy (clip);
}

/* This returns either the current working surface on the paint stack
 * or the actual impl surface of the window. This should not be used
 * from very many places: be careful! */
static cairo_surface_t *
get_window_surface (GdkWindow *window)
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

  surface = get_window_surface (window);

  return cairo_surface_create_for_rectangle (surface,
                                             window->abs_x,
                                             window->abs_y,
                                             window->width,
                                             window->height);
}

/**
 * gdk_cairo_create:
 * @window: a #GdkWindow
 * 
 * Creates a Cairo context for drawing to @window.
 *
 * Note that calling cairo_reset_clip() on the resulting #cairo_t will
 * produce undefined results, so avoid it at all costs.
 *
 * Returns: A newly created Cairo context. Free with
 *  cairo_destroy() when you are done drawing.
 * 
 * Since: 2.8
 **/
cairo_t *
gdk_cairo_create (GdkWindow *window)
{
  cairo_region_t *region;
  cairo_t *cr;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (window->impl_window->current_paint.surface == NULL)
    {
      cairo_surface_t *dummy_surface;
      cairo_t *cr;

      g_warning ("gdk_cairo_create called from outside a paint. Make sure to call "
                 "gdk_window_begin_paint_region before calling gdk_cairo_create!");

      /* Return a dummy surface to keep apps from crashing. */
      dummy_surface = cairo_image_surface_create (gdk_window_get_content (window), 0, 0);
      cr = cairo_create (dummy_surface);
      cairo_surface_destroy (dummy_surface);
      return cr;
    }

  cr = cairo_create (window->impl_window->current_paint.surface);
  region = cairo_region_copy (window->impl_window->current_paint.region);
  cairo_region_translate (region, -window->abs_x, -window->abs_y);
  gdk_cairo_region (cr, region);
  cairo_region_destroy (region);
  cairo_clip (cr);

  return cr;
}

/* Code for dirty-region queueing
 */
static GSList *update_windows = NULL;
static gboolean debug_updates = FALSE;

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

static void
_gdk_window_process_updates_recurse_helper (GdkWindow *window,
                                            cairo_region_t *expose_region,
                                            int dx, int dy)
{
  GdkWindow *child;
  cairo_region_t *clipped_expose_region;
  GdkRectangle clip_box;
  GList *l, *children;

  clipped_expose_region = cairo_region_copy (expose_region);
  cairo_region_translate (clipped_expose_region, dx, dy);
  cairo_region_intersect (clipped_expose_region, window->clip_region);

  if (cairo_region_is_empty (clipped_expose_region) || window->destroyed)
    goto out;

  if (gdk_window_is_offscreen (window->impl_window) &&
      gdk_window_has_impl (window))
    _gdk_window_add_damage ((GdkWindow *) window->impl_window, clipped_expose_region);

  if (window->alpha != 255 && !gdk_window_has_impl (window))
    {
      if (window->alpha == 0)
        goto out;

      cairo_region_get_extents (clipped_expose_region, &clip_box);
      clip_box.x += window->abs_x;
      clip_box.y += window->abs_y;
      /* TODO: How do we now do subwindow alphas */
    }

  /* Paint the window before the children, clipped to the window region */

  /* While gtk+ no longer handles exposes on anything but native
     window we still have to send them to all windows that have the
     event mask set for backwards compat. We also need to send
     it to all native windows, even if they don't specify the
     expose mask, because they may have non-native children that do. */
  if (gdk_window_has_impl (window) ||
      window->event_mask & GDK_EXPOSURE_MASK)
    {
      GdkEvent event;

      event.expose.type = GDK_EXPOSE;
      event.expose.window = g_object_ref (window);
      event.expose.send_event = FALSE;
      event.expose.count = 0;
      event.expose.region = clipped_expose_region;
      cairo_region_get_extents (clipped_expose_region, &event.expose.area);

      _gdk_event_emit (&event);

      g_object_unref (window);
    }

  /* Make this reentrancy safe for expose handlers freeing windows */
  children = g_list_copy (window->children);
  g_list_foreach (children, (GFunc)g_object_ref, NULL);

  /* Iterate over children, starting at bottommost */
  for (l = g_list_last (children); l != NULL; l = l->prev)
    {
      child = l->data;

      if (child->destroyed || !GDK_WINDOW_IS_MAPPED (child) || child->input_only || child->composited)
        continue;

      /* Ignore offscreen children, as they don't draw in their parent and
       * don't take part in the clipping */
      if (gdk_window_is_offscreen (child))
        continue;

      /* Client side child, expose */
      if (child->impl == window->impl)
        _gdk_window_process_updates_recurse_helper ((GdkWindow *)child, clipped_expose_region, -child->x, -child->y);
    }

  g_list_free_full (children, g_object_unref);

 out:
  cairo_region_destroy (clipped_expose_region);
}

void
_gdk_window_process_updates_recurse (GdkWindow *window,
                                     cairo_region_t *expose_region)
{
  _gdk_window_process_updates_recurse_helper (window, expose_region, 0, 0);
}


static void
gdk_window_update_native_shapes (GdkWindow *window)
{
  GdkWindow *child;
  GList *l;

  if (should_apply_clip_as_shape (window))
    apply_clip_as_shape (window);

  for (l = window->native_children; l != NULL; l = l->next)
    {
      child = l->data;

      gdk_window_update_native_shapes (child);
    }
}

/* Process and remove any invalid area on the native window by creating
 * expose events for the window and all non-native descendants.
 */
static void
gdk_window_process_updates_internal (GdkWindow *window)
{
  GdkWindowImplClass *impl_class;
  gboolean save_region = FALSE;
  GdkRectangle clip_box;
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
      cairo_region_t *update_area = window->update_area;
      window->update_area = NULL;

      if (gdk_window_is_viewable (window))
	{
	  cairo_region_t *expose_region;

	  /* Clip to part visible in impl window */
	  cairo_region_intersect (update_area, window->clip_region);

	  if (debug_updates)
	    {
	      /* Make sure we see the red invalid area before redrawing. */
	      gdk_display_sync (gdk_window_get_display (window));
	      g_usleep (70000);
	    }

	  cairo_region_get_extents (update_area, &clip_box);
	  expose_region = cairo_region_copy (update_area);
	  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
	  save_region = impl_class->queue_antiexpose (window, update_area);
          impl_class->process_updates_recurse (window, expose_region);
	  cairo_region_destroy (expose_region);
	}
      if (!save_region)
	cairo_region_destroy (update_area);
    }

  window->in_update = FALSE;

  g_object_unref (window);
}

static void
flush_all_displays (void)
{
  GSList *displays, *l;

  displays = gdk_display_manager_list_displays (gdk_display_manager_get ());
  for (l = displays; l; l = l->next)
    gdk_display_flush (l->data);

  g_slist_free (displays);
}

static void
before_process_all_updates (void)
{
  GSList *displays, *l;
  GdkDisplayClass *display_class;

  displays = gdk_display_manager_list_displays (gdk_display_manager_get ());
  display_class = GDK_DISPLAY_GET_CLASS (displays->data);
  for (l = displays; l; l = l->next)
    display_class->before_process_all_updates (l->data);

  g_slist_free (displays);
}

static void
after_process_all_updates (void)
{
  GSList *displays, *l;
  GdkDisplayClass *display_class;

  displays = gdk_display_manager_list_displays (gdk_display_manager_get ());
  display_class = GDK_DISPLAY_GET_CLASS (displays->data);
  for (l = displays; l; l = l->next)
    display_class->after_process_all_updates (l->data);

  g_slist_free (displays);
}

/* Currently it is not possible to override
 * gdk_window_process_all_updates in the same manner as
 * gdk_window_process_updates and gdk_window_invalidate_maybe_recurse
 * by implementing the GdkPaintable interface.  If in the future a
 * backend would need this, the right solution would be to add a
 * method to GdkDisplay that can be optionally
 * NULL. gdk_window_process_all_updates can then walk the list of open
 * displays and call the mehod.
 */

/**
 * gdk_window_process_all_updates:
 *
 * Calls gdk_window_process_updates() for all windows (see #GdkWindow)
 * in the application.
 *
 **/
void
gdk_window_process_all_updates (void)
{
  GSList *old_update_windows = update_windows;
  GSList *tmp_list = update_windows;
  static gboolean in_process_all_updates = FALSE;
  static gboolean got_recursive_update = FALSE;

  if (in_process_all_updates)
    {
      /* We can't do this now since that would recurse, so
	 delay it until after the recursion is done. */
      got_recursive_update = TRUE;
      return;
    }

  in_process_all_updates = TRUE;
  got_recursive_update = FALSE;

  update_windows = NULL;

  before_process_all_updates ();

  while (tmp_list)
    {
      GdkWindow *window = tmp_list->data;

      if (!GDK_WINDOW_DESTROYED (window))
	{
	  if (window->update_freeze_count ||
	      gdk_window_is_toplevel_frozen (window))
	    gdk_window_add_update_window (window);
	  else
	    gdk_window_process_updates_internal (window);
	}

      g_object_unref (window);
      tmp_list = tmp_list->next;
    }

  g_slist_free (old_update_windows);

  flush_all_displays ();

  after_process_all_updates ();

  in_process_all_updates = FALSE;

  /* If we ignored a recursive call, schedule a
     redraw now so that it eventually happens,
     otherwise we could miss an update if nothing
     else schedules an update. */
  if (got_recursive_update)
    gdk_window_schedule_update (NULL);
}


enum {
  PROCESS_UPDATES_NO_RECURSE,
  PROCESS_UPDATES_WITH_ALL_CHILDREN,
  PROCESS_UPDATES_WITH_SAME_CLOCK_CHILDREN
};

static GList *
find_impl_windows_to_update (GList      *list,
                             GdkWindow  *window,
                             gint        recurse_mode)
{
  GList *node;

  if (GDK_WINDOW_DESTROYED (window))
    return list;

  /* Recurse first, so that we process updates in reverse stacking
   * order so composition or painting over achieves the desired effect
   * for offscreen windows
   */
  if (recurse_mode != PROCESS_UPDATES_NO_RECURSE)
    {
      for (node = window->children; node; node = node->next)
        {
          GdkWindow *child = node->data;

          if (recurse_mode == PROCESS_UPDATES_WITH_ALL_CHILDREN ||
              (recurse_mode == PROCESS_UPDATES_WITH_SAME_CLOCK_CHILDREN &&
               child->frame_clock == NULL))
            {
              list = find_impl_windows_to_update (list, child, recurse_mode);
            }
        }
    }

  /* add reference count so the window cannot be deleted in a callback */
  if (window->impl_window == window)
    list = g_list_prepend (list, g_object_ref (window));

  return list;
}

static void
gdk_window_process_updates_with_mode (GdkWindow     *window,
                                      int            recurse_mode)
{
  GList *list = NULL;
  GList *node;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  list = find_impl_windows_to_update (list, window, recurse_mode);

  if (window->impl_window != window)
    list = g_list_prepend (list, g_object_ref (window->impl_window));

  for (node = list; node; node = node->next)
    {
      GdkWindow *impl_window = node->data;

      if (impl_window->update_area &&
          !impl_window->update_freeze_count &&
          !gdk_window_is_toplevel_frozen (impl_window) &&

          /* Don't recurse into process_updates_internal, we'll
           * do the update later when idle instead. */
          !impl_window->in_update)
        {
          gdk_window_process_updates_internal (impl_window);
          gdk_window_remove_update_window (impl_window);
        }
    }

  g_list_free_full (list, g_object_unref);
}

/**
 * gdk_window_process_updates:
 * @window: a #GdkWindow
 * @update_children: whether to also process updates for child windows
 *
 * Sends one or more expose events to @window. The areas in each
 * expose event will cover the entire update area for the window (see
 * gdk_window_invalidate_region() for details). Normally GDK calls
 * gdk_window_process_all_updates() on your behalf, so there’s no
 * need to call this function unless you want to force expose events
 * to be delivered immediately and synchronously (vs. the usual
 * case, where GDK delivers them in an idle handler). Occasionally
 * this is useful to produce nicer scrolling behavior, for example.
 *
 **/
void
gdk_window_process_updates (GdkWindow *window,
			    gboolean   update_children)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  return gdk_window_process_updates_with_mode (window,
                                               update_children ?
                                               PROCESS_UPDATES_WITH_ALL_CHILDREN :
                                               PROCESS_UPDATES_NO_RECURSE);
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

/**
 * gdk_window_set_invalidate_handler: (skip)
 * @window: a #GdkWindow
 * @handler: a #GdkWindowInvalidateHandlerFunc callback function
 *
 * Registers an invalidate handler for a specific window. This
 * will get called whenever a region in the window or its children
 * is invalidated.
 *
 * This can be used to record the invalidated region, which is
 * useful if you are keeping an offscreen copy of some region
 * and want to keep it up to date. You can also modify the
 * invalidated region in case you’re doing some effect where
 * e.g. a child widget appears in multiple places.
 *
 * Since: 3.10
 **/
void
gdk_window_set_invalidate_handler (GdkWindow                      *window,
				   GdkWindowInvalidateHandlerFunc  handler)
{
  window->invalidate_handler = handler;
}

static void
draw_ugly_color (GdkWindow       *window,
		 const cairo_region_t *region)
{
  cairo_t *cr;

  cr = gdk_cairo_create (window);
  /* Draw ugly color all over the newly-invalid region */
  cairo_set_source_rgb (cr, 50000/65535., 10000/65535., 10000/65535.);
  gdk_cairo_region (cr, region);
  cairo_fill (cr);

  cairo_destroy (cr);
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
					  gpointer              user_data);

static void
invalidate_impl_subwindows (GdkWindow            *window,
			    const cairo_region_t *region,
			    GdkWindowChildFunc    child_func,
			    gpointer              user_data,
			    int dx, int dy)
{
  GList *tmp_list;

  tmp_list = window->children;

  while (tmp_list)
    {
      GdkWindow *child = tmp_list->data;
      tmp_list = tmp_list->next;

      if (child->input_only ||
	  !window->viewable)
	continue;

      if (child_func && (*child_func) ((GdkWindow *)child, user_data))
	{
	  if (gdk_window_has_impl (child))
	    {
	      cairo_region_t *tmp = cairo_region_copy (region);
	      cairo_region_translate (tmp, -dx, -dy);
	      gdk_window_invalidate_maybe_recurse_full (child,
							tmp, child_func, user_data);
	      cairo_region_destroy (tmp);
	    }
	  else
	    {
	      dx += child->x;
	      dy += child->y;
	      invalidate_impl_subwindows (child,
					  region,
					  child_func, user_data,
					  dx, dy);
	      dx -= child->x;
	      dy -= child->y;
	    }

	}
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

  invalidate_impl_subwindows (window, region, child_func, user_data, 0, 0);

  if (debug_updates)
    draw_ugly_color (window, visible_region);

  while (window != NULL && 
	 !cairo_region_is_empty (visible_region))
    {
      if (window->invalidate_handler)
	window->invalidate_handler (window, visible_region);

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
 * region that needs to be redrawn, or “dirty region.” The call
 * gdk_window_process_updates() sends one or more expose events to the
 * window, which together cover the entire update area. An
 * application would normally redraw the contents of @window in
 * response to those expose events.
 *
 * GDK will call gdk_window_process_all_updates() on your behalf
 * whenever your program returns to the main loop and becomes idle, so
 * normally there’s no need to do that manually, you just need to
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
 * region that needs to be redrawn, or “dirty region.” The call
 * gdk_window_process_updates() sends one or more expose events to the
 * window, which together cover the entire update area. An
 * application would normally redraw the contents of @window in
 * response to those expose events.
 *
 * GDK will call gdk_window_process_all_updates() on your behalf
 * whenever your program returns to the main loop and becomes idle, so
 * normally there’s no need to do that manually, you just need to
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
 * Adds @region to the update area for @window. The update area is the
 * region that needs to be redrawn, or “dirty region.” The call
 * gdk_window_process_updates() sends one or more expose events to the
 * window, which together cover the entire update area. An
 * application would normally redraw the contents of @window in
 * response to those expose events.
 *
 * GDK will call gdk_window_process_all_updates() on your behalf
 * whenever your program returns to the main loop and becomes idle, so
 * normally there’s no need to do that manually, you just need to
 * invalidate regions that you know should be redrawn.
 *
 * This version of invalidation is used when you recieve expose events
 * from the native window system. It exposes the native window, plus
 * any non-native child windows (but not native child windows, as those would
 * have gotten their own expose events).
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

/**
 * gdk_window_freeze_toplevel_updates_libgtk_only:
 * @window: a #GdkWindow
 *
 * Temporarily freezes a window and all its descendants such that it won't
 * receive expose events.  The window will begin receiving expose events
 * again when gdk_window_thaw_toplevel_updates_libgtk_only() is called. If
 * gdk_window_freeze_toplevel_updates_libgtk_only()
 * has been called more than once,
 * gdk_window_thaw_toplevel_updates_libgtk_only() must be called
 * an equal number of times to begin processing exposes.
 *
 * This function is not part of the GDK public API and is only
 * for use by GTK+.
 **/
void
gdk_window_freeze_toplevel_updates_libgtk_only (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (window->window_type != GDK_WINDOW_CHILD);

  window->update_and_descendants_freeze_count++;
  _gdk_frame_clock_freeze (gdk_window_get_frame_clock (window));
}

/**
 * gdk_window_thaw_toplevel_updates_libgtk_only:
 * @window: a #GdkWindow
 *
 * Thaws a window frozen with
 * gdk_window_freeze_toplevel_updates_libgtk_only().
 *
 * This function is not part of the GDK public API and is only
 * for use by GTK+.
 **/
void
gdk_window_thaw_toplevel_updates_libgtk_only (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (window->window_type != GDK_WINDOW_CHILD);
  g_return_if_fail (window->update_and_descendants_freeze_count > 0);

  window->update_and_descendants_freeze_count--;
  _gdk_frame_clock_thaw (gdk_window_get_frame_clock (window));

  gdk_window_schedule_update (window);
}

/**
 * gdk_window_set_debug_updates:
 * @setting: %TRUE to turn on update debugging
 *
 * With update debugging enabled, calls to
 * gdk_window_invalidate_region() clear the invalidated region of the
 * screen to a noticeable color, and GDK pauses for a short time
 * before sending exposes to windows during
 * gdk_window_process_updates().  The net effect is that you can see
 * the invalid region for each window and watch redraws as they
 * occur. This allows you to diagnose inefficiencies in your application.
 *
 * In essence, because the GDK rendering model prevents all flicker,
 * if you are redrawing the same region 400 times you may never
 * notice, aside from noticing a speed problem. Enabling update
 * debugging causes GTK to flicker slowly and noticeably, so you can
 * see exactly what’s being redrawn when, in what order.
 *
 * The --gtk-debug=updates command line option passed to GTK+ programs
 * enables this debug option at application startup time. That's
 * usually more useful than calling gdk_window_set_debug_updates()
 * yourself, though you might want to use this function to enable
 * updates sometime after application startup time.
 *
 **/
void
gdk_window_set_debug_updates (gboolean setting)
{
  debug_updates = setting;
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
 * gdk_window_get_pointer:
 * @window: a #GdkWindow
 * @x: (out) (allow-none): return location for X coordinate of pointer or %NULL to not
 *      return the X coordinate
 * @y: (out) (allow-none):  return location for Y coordinate of pointer or %NULL to not
 *      return the Y coordinate
 * @mask: (out) (allow-none): return location for modifier mask or %NULL to not return the
 *      modifier mask
 *
 * Obtains the current pointer position and modifier state.
 * The position is given in coordinates relative to the upper left
 * corner of @window.
 *
 * Returns: (nullable) (transfer none): the window containing the
 * pointer (as with gdk_window_at_pointer()), or %NULL if the window
 * containing the pointer isn’t known to GDK
 *
 * Deprecated: 3.0: Use gdk_window_get_device_position() instead.
 **/
GdkWindow*
gdk_window_get_pointer (GdkWindow	  *window,
			gint		  *x,
			gint		  *y,
			GdkModifierType   *mask)
{
  GdkDisplay *display;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  display = gdk_window_get_display (window);

  return gdk_window_get_device_position (window, display->core_pointer, x, y, mask);
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

static void
get_all_native_children (GdkWindow *window,
			 GList **native)
{
  GdkWindow *child;
  GList *l;

  for (l = window->children; l != NULL; l = l->next)
    {
      child = l->data;

      if (gdk_window_has_impl (child))
	*native = g_list_prepend (*native, child);
      else
	get_all_native_children (child, native);
    }
}


static inline void
gdk_window_raise_internal (GdkWindow *window)
{
  GdkWindow *parent = window->parent;
  GdkWindow *above;
  GList *native_children;
  GList *l, listhead;
  GdkWindowImplClass *impl_class;

  if (parent)
    {
      parent->children = g_list_remove (parent->children, window);
      parent->children = g_list_prepend (parent->children, window);
    }

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
  /* Just do native raise for toplevels */
  if (gdk_window_is_toplevel (window) ||
      /* The restack_under codepath should work correctly even if the parent
	 is native, but it relies on the order of ->children to be correct,
	 and some apps like SWT reorder the x windows without gdks knowledge,
	 so we use raise directly in order to make these behave as before
	 when using native windows */
      (gdk_window_has_impl (window) && gdk_window_has_impl (parent)))
    {
      impl_class->raise (window);
    }
  else if (gdk_window_has_impl (window))
    {
      above = find_native_sibling_above (parent, window);
      if (above)
	{
	  listhead.data = window;
	  listhead.next = NULL;
	  listhead.prev = NULL;
	  impl_class->restack_under ((GdkWindow *)above,
				     &listhead);
	}
      else
	impl_class->raise (window);
    }
  else
    {
      native_children = NULL;
      get_all_native_children (window, &native_children);
      if (native_children != NULL)
	{
	  above = find_native_sibling_above (parent, window);

	  if (above)
	    impl_class->restack_under (above, native_children);
	  else
	    {
	      /* Right order, since native_children is bottom-topmost first */
	      for (l = native_children; l != NULL; l = l->next)
		impl_class->raise (l->data);
	    }

	  g_list_free (native_children);
	}

    }
}

/* Returns TRUE If the native window was mapped or unmapped */
static gboolean
set_viewable (GdkWindow *w,
	      gboolean val)
{
  GdkWindow *child;
  GdkWindowImplClass *impl_class;
  GList *l;

  if (w->viewable == val)
    return FALSE;

  w->viewable = val;

  if (val)
    recompute_visible_regions (w, FALSE);

  for (l = w->children; l != NULL; l = l->next)
    {
      child = l->data;

      if (GDK_WINDOW_IS_MAPPED (child) &&
	  child->window_type != GDK_WINDOW_FOREIGN)
	set_viewable (child, val);
    }

  if (gdk_window_has_impl (w)  &&
      w->window_type != GDK_WINDOW_FOREIGN &&
      !gdk_window_is_toplevel (w))
    {
      /* For most native windows we show/hide them not when they are
       * mapped/unmapped, because that may not produce the correct results.
       * For instance, if a native window have a non-native parent which is
       * hidden, but its native parent is viewable then showing the window
       * would make it viewable to X but its not viewable wrt the non-native
       * hierarchy. In order to handle this we track the gdk side viewability
       * and only map really viewable windows.
       *
       * There are two exceptions though:
       *
       * For foreign windows we don't want ever change the mapped state
       * except when explicitly done via gdk_window_show/hide, as this may
       * cause problems for client owning the foreign window when its window
       * is suddenly mapped or unmapped.
       *
       * For toplevel windows embedded in a foreign window (e.g. a plug)
       * we sometimes synthesize a map of a window, but the native
       * window is really shown by the embedder, so we don't want to
       * do the show ourselves. We can't really tell this case from the normal
       * toplevel show as such toplevels are seen by gdk as parents of the
       * root window, so we make an exception for all toplevels.
       */

      impl_class = GDK_WINDOW_IMPL_GET_CLASS (w->impl);
      if (val)
	impl_class->show ((GdkWindow *)w, FALSE);
      else
	impl_class->hide ((GdkWindow *)w);

      return TRUE;
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
  gboolean did_show;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->destroyed)
    return;

  was_mapped = GDK_WINDOW_IS_MAPPED (window);
  was_viewable = window->viewable;

  if (raise)
    /* Keep children in (reverse) stacking order */
    gdk_window_raise_internal (window);

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

  if (!was_mapped || raise)
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
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->destroyed)
    return;

  /* Keep children in (reverse) stacking order */
  gdk_window_raise_internal (window);

  if (!gdk_window_is_toplevel (window) &&
      gdk_window_is_viewable (window) &&
      !window->input_only)
    gdk_window_invalidate_region_full (window, window->clip_region, TRUE);
}

static void
gdk_window_lower_internal (GdkWindow *window)
{
  GdkWindow *parent = window->parent;
  GdkWindowImplClass *impl_class;
  GdkWindow *above;
  GList *native_children;
  GList *l, listhead;

  if (parent)
    {
      parent->children = g_list_remove (parent->children, window);
      parent->children = g_list_append (parent->children, window);
    }

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
  /* Just do native lower for toplevels */
  if (gdk_window_is_toplevel (window) ||
      /* The restack_under codepath should work correctly even if the parent
	 is native, but it relies on the order of ->children to be correct,
	 and some apps like SWT reorder the x windows without gdks knowledge,
	 so we use lower directly in order to make these behave as before
	 when using native windows */
      (gdk_window_has_impl (window) && gdk_window_has_impl (parent)))
    {
      impl_class->lower (window);
    }
  else if (gdk_window_has_impl (window))
    {
      above = find_native_sibling_above (parent, window);
      if (above)
	{
	  listhead.data = window;
	  listhead.next = NULL;
	  listhead.prev = NULL;
	  impl_class->restack_under ((GdkWindow *)above, &listhead);
	}
      else
	impl_class->raise (window);
    }
  else
    {
      native_children = NULL;
      get_all_native_children (window, &native_children);
      if (native_children != NULL)
	{
	  above = find_native_sibling_above (parent, window);

	  if (above)
	    impl_class->restack_under ((GdkWindow *)above,
				       native_children);
	  else
	    {
	      /* Right order, since native_children is bottom-topmost first */
	      for (l = native_children; l != NULL; l = l->next)
		impl_class->raise (l->data);
	    }

	  g_list_free (native_children);
	}

    }
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
  GdkWindow *above_native;
  GList *sibling_link;
  GList *native_children;
  GList *l, listhead;

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

      parent->children = g_list_remove (parent->children, window);
      if (above)
	parent->children = g_list_insert_before (parent->children,
						 sibling_link,
						 window);
      else
	parent->children = g_list_insert_before (parent->children,
						 sibling_link->next,
						 window);

      impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
      if (gdk_window_has_impl (window))
	{
	  above_native = find_native_sibling_above (parent, window);
	  if (above_native)
	    {
	      listhead.data = window;
	      listhead.next = NULL;
	      listhead.prev = NULL;
	      impl_class->restack_under (above_native, &listhead);
	    }
	  else
	    impl_class->raise (window);
	}
      else
	{
	  native_children = NULL;
	  get_all_native_children (window, &native_children);
	  if (native_children != NULL)
	    {
	      above_native = find_native_sibling_above (parent, window);
	      if (above_native)
		impl_class->restack_under (above_native,
					   native_children);
	      else
		{
		  /* Right order, since native_children is bottom-topmost first */
		  for (l = native_children; l != NULL; l = l->next)
		    impl_class->raise (l->data);
		}

	      g_list_free (native_children);
	    }
	}
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
      GdkDeviceManager *device_manager;
      GList *devices, *d;

      /* May need to break grabs on children */
      display = gdk_window_get_display (window);
      device_manager = gdk_display_get_device_manager (display);

      /* Get all devices */
      devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);
      devices = g_list_concat (devices, gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_SLAVE));
      devices = g_list_concat (devices, gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_FLOATING));

      for (d = devices; d; d = d->next)
        {
          GdkDevice *device = d->data;

          if (_gdk_display_end_device_grab (display,
                                            device,
                                            _gdk_display_get_next_serial (display),
                                            window,
                                            TRUE))
            gdk_device_ungrab (device, GDK_CURRENT_TIME);
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

  while (gdk_window_is_offscreen (native))
    {
      native = gdk_offscreen_window_get_embedder (native);

      if (native == NULL ||
	  (!_gdk_window_has_impl (native) &&
	   !gdk_window_is_viewable (native)))
	return;

      native = gdk_window_get_toplevel (native);
    }

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
move_native_children (GdkWindow *private)
{
  GList *l;
  GdkWindow *child;
  GdkWindowImplClass *impl_class;

  for (l = private->children; l; l = l->next)
    {
      child = l->data;

      if (child->impl != private->impl)
	{
	  impl_class = GDK_WINDOW_IMPL_GET_CLASS (child->impl);
	  impl_class->move_resize (child, TRUE,
				   child->x, child->y,
				   child->width, child->height);
	}
      else
	move_native_children  (child);
    }
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
  GdkWindowImplClass *impl_class;
  gboolean expose;
  int old_abs_x, old_abs_y;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->destroyed)
    return;

  if (gdk_window_is_toplevel (window))
    {
      gdk_window_move_resize_toplevel (window, with_move, x, y, width, height);
      return;
    }

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
      expose = TRUE;

      old_region = cairo_region_copy (window->clip_region);
      /* Adjust regions to parent window coords */
      cairo_region_translate (old_region, window->x, window->y);
    }

  /* Set the new position and size */
  if (with_move)
    {
      window->x = x;
      window->y = y;
    }
  if (!(width < 0 && height < 0))
    {
      if (width < 1)
	width = 1;
      window->width = width;
      if (height < 1)
	height = 1;
      window->height = height;
    }

  old_abs_x = window->abs_x;
  old_abs_y = window->abs_y;

  recompute_visible_regions (window, FALSE);

  if (gdk_window_has_impl (window))
    {
      impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

      /* Do the actual move after recomputing things, as this will have set the shape to
	 the now correct one, thus avoiding copying regions that should not be copied. */
      impl_class->move_resize (window, TRUE,
			       window->x, window->y,
			       window->width, window->height);
    }
  else if (old_abs_x != window->abs_x ||
	   old_abs_y != window->abs_y)
    move_native_children (window);

  if (expose)
    {
      new_region = cairo_region_copy (window->clip_region);
      /* Adjust region to parent window coords */
      cairo_region_translate (new_region, window->x, window->y);

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

  move_native_children (window);

  gdk_window_invalidate_region_full (window, window->clip_region, TRUE);

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
gdk_window_move_region (GdkWindow       *window,
			const cairo_region_t *region,
			gint             dx,
			gint             dy)
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

/**
 * gdk_window_set_background:
 * @window: a #GdkWindow
 * @color: a #GdkColor
 *
 * Sets the background color of @window. (However, when using GTK+,
 * set the background of a widget with gtk_widget_modify_bg() - if
 * you’re an application - or gtk_style_set_background() - if you're
 * implementing a custom widget.)
 *
 * See also gdk_window_set_background_pattern().
 *
 * Deprecated: 3.4: Use gdk_window_set_background_rgba() instead.
 */
void
gdk_window_set_background (GdkWindow      *window,
			   const GdkColor *color)
{
  cairo_pattern_t *pattern;

  g_return_if_fail (GDK_IS_WINDOW (window));

  pattern = cairo_pattern_create_rgb (color->red   / 65535.,
                                      color->green / 65535.,
                                      color->blue  / 65535.);

  gdk_window_set_background_pattern (window, pattern);

  cairo_pattern_destroy (pattern);
}

/**
 * gdk_window_set_background_rgba:
 * @window: a #GdkWindow
 * @rgba: a #GdkRGBA color
 *
 * Sets the background color of @window.
 *
 * See also gdk_window_set_background_pattern().
 **/
void
gdk_window_set_background_rgba (GdkWindow     *window,
                                const GdkRGBA *rgba)
{
  cairo_pattern_t *pattern;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (rgba != NULL);

  pattern = cairo_pattern_create_rgba (rgba->red, rgba->green,
                                       rgba->blue, rgba->alpha);

  gdk_window_set_background_pattern (window, pattern);

  cairo_pattern_destroy (pattern);
}


/**
 * gdk_window_set_background_pattern:
 * @window: a #GdkWindow
 * @pattern: (allow-none): a pattern to use, or %NULL
 *
 * Sets the background of @window.
 *
 * A background of %NULL means that the window will inherit its
 * background from its parent window.
 *
 * The windowing system will normally fill a window with its background
 * when the window is obscured then exposed.
 */
void
gdk_window_set_background_pattern (GdkWindow *window,
                                   cairo_pattern_t *pattern)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->input_only)
    return;

  if (pattern)
    cairo_pattern_reference (pattern);
  if (window->background)
    cairo_pattern_destroy (window->background);
  window->background = pattern;

  if (gdk_window_has_impl (window))
    {
      GdkWindowImplClass *impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
      impl_class->set_background (window, pattern);
    }
  else
    gdk_window_invalidate_rect_full (window, NULL, TRUE);
}

/**
 * gdk_window_get_background_pattern:
 * @window: a window
 *
 * Gets the pattern used to clear the background on @window. If @window
 * does not have its own background and reuses the parent's, %NULL is
 * returned and you’ll have to query it yourself.
 *
 * Returns: (nullable) (transfer none): The pattern to use for the
 * background or %NULL to use the parent’s background.
 *
 * Since: 2.22
 **/
cairo_pattern_t *
gdk_window_get_background_pattern (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  return window->background;
}

static void
gdk_window_set_cursor_internal (GdkWindow *window,
                                GdkDevice *device,
                                GdkCursor *cursor)
{
  if (GDK_WINDOW_DESTROYED (window))
    return;

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
 * Sets the default mouse pointer for a #GdkWindow. Use gdk_cursor_new_for_display()
 * or gdk_cursor_new_from_pixbuf() to create the cursor. To make the cursor
 * invisible, use %GDK_BLANK_CURSOR. Passing %NULL for the @cursor argument
 * to gdk_window_set_cursor() means that @window will use the cursor of its
 * parent window. Most windows should use this default.
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
      GdkDeviceManager *device_manager;
      GList *devices, *d;

      if (cursor)
	window->cursor = g_object_ref (cursor);

      device_manager = gdk_display_get_device_manager (display);
      devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);

      for (d = devices; d; d = d->next)
        {
          GdkDevice *device;

          device = d->data;

          if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
            continue;

          gdk_window_set_cursor_internal (window, device, window->cursor);
        }

      g_list_free (devices);
      g_object_notify (G_OBJECT (window), "cursor");
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
 * gdk_window_get_origin() but allows you go pass
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
 * window, where the parent window is the normal parent as returned by
 * gdk_window_get_parent() for normal windows, and the window's
 * embedder as returned by gdk_offscreen_window_get_embedder() for
 * offscreen windows.
 *
 * For normal windows, calling this function is equivalent to adding
 * the return values of gdk_window_get_position() to the child coordinates.
 * For offscreen windows however (which can be arbitrarily transformed),
 * this function calls the GdkWindow::to-embedder: signal to translate
 * the coordinates.
 *
 * You should always use this function when writing generic code that
 * walks up a window hierarchy.
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

  if (gdk_window_is_offscreen (window))
    {
      gdouble px, py;

      to_embedder (window, x, y, &px, &py);

      if (parent_x)
        *parent_x = px;

      if (parent_y)
        *parent_y = py;
    }
  else
    {
      if (parent_x)
        *parent_x = x + window->x;

      if (parent_y)
        *parent_y = y + window->y;
    }
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
 * window, where the parent window is the normal parent as returned by
 * gdk_window_get_parent() for normal windows, and the window's
 * embedder as returned by gdk_offscreen_window_get_embedder() for
 * offscreen windows.
 *
 * For normal windows, calling this function is equivalent to subtracting
 * the return values of gdk_window_get_position() from the parent coordinates.
 * For offscreen windows however (which can be arbitrarily transformed),
 * this function calls the GdkWindow::from-embedder: signal to translate
 * the coordinates.
 *
 * You should always use this function when writing generic code that
 * walks down a window hierarchy.
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

  if (gdk_window_is_offscreen (window))
    {
      gdouble cx, cy;

      from_embedder (window, parent_x, parent_y, &cx, &cy);

      if (x)
        *x = cx;

      if (y)
        *y = cy;
    }
  else
    {
      if (x)
        *x = parent_x - window->x;

      if (y)
        *y = parent_y - window->y;
    }
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
 * gdk_window_set_static_gravities:
 * @window: a #GdkWindow
 * @use_static: %TRUE to turn on static gravity
 *
 * Set the bit gravity of the given window to static, and flag it so
 * all children get static subwindow gravity. This is used if you are
 * implementing scary features that involve deep knowledge of the
 * windowing system. Don’t worry about it unless you have to.
 *
 * Returns: %TRUE if the server supports static gravity
 */
gboolean
gdk_window_set_static_gravities (GdkWindow *window,
				 gboolean   use_static)
{
  GdkWindowImplClass *impl_class;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (gdk_window_has_impl (window))
    {
      impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);
      return impl_class->set_static_gravities (window, use_static);
    }

  return FALSE;
}

/**
 * gdk_window_get_composited:
 * @window: a #GdkWindow
 *
 * Determines whether @window is composited.
 *
 * See gdk_window_set_composited().
 *
 * Returns: %TRUE if the window is composited.
 *
 * Since: 2.22
 **/
gboolean
gdk_window_get_composited (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  return window->composited;
}

/**
 * gdk_window_set_composited:
 * @window: a #GdkWindow
 * @composited: %TRUE to set the window as composited
 *
 * Sets a #GdkWindow as composited, or unsets it. Composited
 * windows do not automatically have their contents drawn to
 * the screen. Drawing is redirected to an offscreen buffer
 * and an expose event is emitted on the parent of the composited
 * window. It is the responsibility of the parent’s expose handler
 * to manually merge the off-screen content onto the screen in
 * whatever way it sees fit.
 *
 * It only makes sense for child windows to be composited; see
 * gdk_window_set_opacity() if you need translucent toplevel
 * windows.
 *
 * An additional effect of this call is that the area of this
 * window is no longer clipped from regions marked for
 * invalidation on its parent. Draws done on the parent
 * window are also no longer clipped by the child.
 *
 * This call is only supported on some systems (currently,
 * only X11 with new enough Xcomposite and Xdamage extensions).
 * You must call gdk_display_supports_composite() to check if
 * setting a window as composited is supported before
 * attempting to do so.
 *
 * Since: 2.12
 */
void
gdk_window_set_composited (GdkWindow *window,
			   gboolean   composited)
{
  GdkDisplay *display;
  GdkWindowImplClass *impl_class;

  g_return_if_fail (GDK_IS_WINDOW (window));

  composited = composited != FALSE;

  if (window->composited == composited)
    return;

  if (composited)
    gdk_window_ensure_native (window);

  display = gdk_window_get_display (window);

  impl_class = GDK_WINDOW_IMPL_GET_CLASS (window->impl);

  if (composited && (!gdk_display_supports_composite (display) || !impl_class->set_composited))
    {
      g_warning ("gdk_window_set_composited called but "
                 "compositing is not supported");
      return;
    }

  impl_class->set_composited (window, composited);

  recompute_visible_regions (window, FALSE);

  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_window_invalidate_in_parent (window);

  window->composited = composited;
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

void
_gdk_window_add_damage (GdkWindow *toplevel,
			cairo_region_t *damaged_region)
{
  GdkDisplay *display;
  GdkEvent event = { 0, };
  event.expose.type = GDK_DAMAGE;
  event.expose.window = toplevel;
  event.expose.send_event = FALSE;
  event.expose.region = damaged_region;
  cairo_region_get_extents (event.expose.region, &event.expose.area);
  display = gdk_window_get_display (event.expose.window);
  _gdk_event_queue_append (display, gdk_event_copy (&event));
}

/* Gets the toplevel for a window as used for events,
   i.e. including offscreen parents */
static GdkWindow *
get_event_parent (GdkWindow *window)
{
  if (gdk_window_is_offscreen (window))
    return gdk_offscreen_window_get_embedder ((GdkWindow *)window);
  else
    return window->parent;
}

/* Gets the toplevel for a window as used for events,
   i.e. including offscreen parents going up to the native
   toplevel */
static GdkWindow *
get_event_toplevel (GdkWindow *window)
{
  GdkWindow *parent;

  while ((parent = get_event_parent (window)) != NULL &&
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

      w = get_event_parent (w);
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
	 (parent = get_event_parent (cursor_window)) != NULL &&
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
  while ((parent = get_event_parent (window)) != NULL &&
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

static GdkWindow *
pick_embedded_child (GdkWindow *window,
		     gdouble    x,
                     gdouble    y)
{
  GdkWindow *res;

  res = NULL;
  g_signal_emit (window,
		 signals[PICK_EMBEDDED_CHILD], 0,
		 x, y, &res);

  return res;
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
	  if (point_in_window (sub, child_x, child_y))
	    return (GdkWindow *)sub;
	}

      if (window->num_offscreen_children > 0)
	{
	  sub = pick_embedded_child (window,
				     x, y);
	  if (sub)
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
  GdkWindow *sub;
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
	      if (point_in_window (sub, child_x, child_y))
		{
		  x = child_x;
		  y = child_y;
		  window = sub;
		  found = TRUE;
		  break;
		}
	    }
	  if (!found &&
	      window->num_offscreen_children > 0)
	    {
	      sub = pick_embedded_child (window,
					 x, y);
	      if (sub)
		{
		  found = TRUE;
		  window = sub;
		  from_embedder (sub, x, y, &x, &y);
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
  GDK_TOUCH_MASK /* GDK_TOUCH_CANCEL = 40 */
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
	 type == GDK_SCROLL;
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
      tmp = get_event_parent (tmp);
    }

  tmp = win2;
  while (tmp != NULL && tmp->window_type != GDK_WINDOW_ROOT)
    {
      path2 = g_list_prepend (path2, tmp);
      tmp = get_event_parent (tmp);
    }

  list1 = path1;
  list2 = path2;
  tmp = NULL;
  while (list1 && list2 && (list1->data == list2->data))
    {
      tmp = list1->data;
      list1 = g_list_next (list1);
      list2 = g_list_next (list2);
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
	  win = get_event_parent (a);
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
	      win = get_event_parent (win);
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
	  win = get_event_parent (b);
	  while (win != c && win->window_type != GDK_WINDOW_ROOT)
	    {
	      path = g_list_prepend (path, win);
	      win = get_event_parent (win);
	    }

	  if (non_linear)
	    notify_type = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	  else
	    notify_type = GDK_NOTIFY_VIRTUAL;

	  list = path;
	  while (list)
	    {
	      win = list->data;
	      list = g_list_next (list);
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
      pointer_window != grab->window)
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

/**
 * gdk_pointer_grab:
 * @window: the #GdkWindow which will own the grab (the grab window).
 * @owner_events: if %FALSE then all pointer events are reported with respect to
 *                @window and are only reported if selected by @event_mask. If %TRUE then pointer
 *                events for this application are reported as normal, but pointer events outside
 *                this application are reported with respect to @window and only if selected by
 *                @event_mask. In either mode, unreported events are discarded.
 * @event_mask: specifies the event mask, which is used in accordance with
 *              @owner_events. Note that only pointer events (i.e. button and motion events)
 *              may be selected.
 * @confine_to: (allow-none): If non-%NULL, the pointer will be confined to this
 *              window during the grab. If the pointer is outside @confine_to, it will
 *              automatically be moved to the closest edge of @confine_to and enter
 *              and leave events will be generated as necessary.
 * @cursor: (allow-none): the cursor to display while the grab is active. If this is %NULL then
 *          the normal cursors are used for @window and its descendants, and the cursor
 *          for @window is used for all other windows.
 * @time_: the timestamp of the event which led to this pointer grab. This usually
 *         comes from a #GdkEventButton struct, though %GDK_CURRENT_TIME can be used if
 *         the time isn’t known.
 *
 * Grabs the pointer (usually a mouse) so that all events are passed to this
 * application until the pointer is ungrabbed with gdk_pointer_ungrab(), or
 * the grab window becomes unviewable.
 * This overrides any previous pointer grab by this client.
 *
 * Pointer grabs are used for operations which need complete control over mouse
 * events, even if the mouse leaves the application.
 * For example in GTK+ it is used for Drag and Drop, for dragging the handle in
 * the #GtkHPaned and #GtkVPaned widgets.
 *
 * Note that if the event mask of an X window has selected both button press and
 * button release events, then a button press event will cause an automatic
 * pointer grab until the button is released.
 * X does this automatically since most applications expect to receive button
 * press and release events in pairs.
 * It is equivalent to a pointer grab on the window with @owner_events set to
 * %TRUE.
 *
 * If you set up anything at the time you take the grab that needs to be cleaned
 * up when the grab ends, you should handle the #GdkEventGrabBroken events that
 * are emitted when the grab ends unvoluntarily.
 *
 * Returns: %GDK_GRAB_SUCCESS if the grab was successful.
 *
 * Deprecated: 3.0: Use gdk_device_grab() instead.
 **/
GdkGrabStatus
gdk_pointer_grab (GdkWindow *	  window,
		  gboolean	  owner_events,
		  GdkEventMask	  event_mask,
		  GdkWindow *	  confine_to,
		  GdkCursor *	  cursor,
		  guint32	  time)
{
  GdkWindow *native;
  GdkDisplay *display;
  GdkDeviceManager *device_manager;
  GdkDevice *device;
  GdkGrabStatus res = 0;
  gulong serial;
  GList *devices, *dev;

  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  g_return_val_if_fail (confine_to == NULL || GDK_IS_WINDOW (confine_to), 0);

  /* We need a native window for confine to to work, ensure we have one */
  if (confine_to)
    {
      if (!gdk_window_ensure_native (confine_to))
	{
	  g_warning ("Can't confine to grabbed window, not native");
	  confine_to = NULL;
	}
    }

  /* Non-viewable client side window => fail */
  if (!_gdk_window_has_impl (window) &&
      !gdk_window_is_viewable (window))
    return GDK_GRAB_NOT_VIEWABLE;

  native = gdk_window_get_toplevel (window);
  while (gdk_window_is_offscreen (native))
    {
      native = gdk_offscreen_window_get_embedder (native);

      if (native == NULL ||
	  (!_gdk_window_has_impl (native) &&
	   !gdk_window_is_viewable (native)))
	return GDK_GRAB_NOT_VIEWABLE;

      native = gdk_window_get_toplevel (native);
    }

  display = gdk_window_get_display (window);

  serial = _gdk_display_get_next_serial (display);
  device_manager = gdk_display_get_device_manager (display);
  devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);

  /* FIXME: Should this be generic to all backends? */
  /* FIXME: What happens with extended devices? */
  for (dev = devices; dev; dev = dev->next)
    {
      device = dev->data;

      if (gdk_device_get_source (device) != GDK_SOURCE_MOUSE)
        continue;

      res = GDK_DEVICE_GET_CLASS (device)->grab (device,
                                                 native,
                                                 owner_events,
                                                 get_native_grab_event_mask (event_mask),
                                                 confine_to,
                                                 cursor,
                                                 time);

      if (res == GDK_GRAB_SUCCESS)
        _gdk_display_add_device_grab (display,
                                      device,
                                      window,
                                      native,
                                      GDK_OWNERSHIP_NONE,
                                      owner_events,
                                      event_mask,
                                      serial,
                                      time,
                                      FALSE);
    }

  /* FIXME: handle errors when grabbing */

  g_list_free (devices);

  return res;
}

/**
 * gdk_keyboard_grab:
 * @window: the #GdkWindow which will own the grab (the grab window).
 * @owner_events: if %FALSE then all keyboard events are reported with respect to
 *   @window. If %TRUE then keyboard events for this application are
 *   reported as normal, but keyboard events outside this application
 *   are reported with respect to @window. Both key press and key
 *   release events are always reported, independant of the event mask
 *   set by the application.
 * @time_: a timestamp from a #GdkEvent, or %GDK_CURRENT_TIME if no timestamp is
 *   available.
 *
 * Grabs the keyboard so that all events are passed to this
 * application until the keyboard is ungrabbed with gdk_keyboard_ungrab().
 * This overrides any previous keyboard grab by this client.
 *
 * If you set up anything at the time you take the grab that needs to be cleaned
 * up when the grab ends, you should handle the #GdkEventGrabBroken events that
 * are emitted when the grab ends unvoluntarily.
 *
 * Returns: %GDK_GRAB_SUCCESS if the grab was successful.
 *
 * Deprecated: 3.0: Use gdk_device_grab() instead.
 **/
GdkGrabStatus
gdk_keyboard_grab (GdkWindow *window,
		   gboolean   owner_events,
		   guint32    time)
{
  GdkWindow *native;
  GdkDisplay *display;
  GdkDeviceManager *device_manager;
  GdkDevice *device;
  GdkGrabStatus res = 0;
  gulong serial;
  GList *devices, *dev;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  /* Non-viewable client side window => fail */
  if (!_gdk_window_has_impl (window) &&
      !gdk_window_is_viewable (window))
    return GDK_GRAB_NOT_VIEWABLE;

  native = gdk_window_get_toplevel (window);

  while (gdk_window_is_offscreen (native))
    {
      native = gdk_offscreen_window_get_embedder (native);

      if (native == NULL ||
	  (!_gdk_window_has_impl (native) &&
	   !gdk_window_is_viewable (native)))
	return GDK_GRAB_NOT_VIEWABLE;

      native = gdk_window_get_toplevel (native);
    }

  display = gdk_window_get_display (window);
  serial = _gdk_display_get_next_serial (display);
  device_manager = gdk_display_get_device_manager (display);
  devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);

  /* FIXME: Should this be generic to all backends? */
  /* FIXME: What happens with extended devices? */
  for (dev = devices; dev; dev = dev->next)
    {
      device = dev->data;

      if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
        continue;

      res = GDK_DEVICE_GET_CLASS (device)->grab (device,
                                                 native,
                                                 owner_events,
                                                 GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK,
                                                 NULL,
                                                 NULL,
                                                 time);

      if (res == GDK_GRAB_SUCCESS)
        _gdk_display_add_device_grab (display,
                                      device,
                                      window,
                                      native,
                                      GDK_OWNERSHIP_NONE,
                                      owner_events, 0,
                                      serial,
                                      time,
                                      FALSE);
    }

  /* FIXME: handle errors when grabbing */

  g_list_free (devices);

  return res;
}

/**
 * gdk_window_geometry_changed:
 * @window: an embedded offscreen #GdkWindow
 *
 * This function informs GDK that the geometry of an embedded
 * offscreen window has changed. This is necessary for GDK to keep
 * track of which offscreen window the pointer is in.
 *
 * Since: 2.18
 */
void
gdk_window_geometry_changed (GdkWindow *window)
{
  _gdk_synthesize_crossing_events_for_geometry_change (window);
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

      pointer_window = get_event_parent (pointer_window);
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
       _gdk_event_get_pointer_emulated (source_event)))
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
       _gdk_event_get_pointer_emulated (source_event)) &&
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
                                    _gdk_event_get_pointer_emulated (source_event),
                                    serial);

      if (event_type == GDK_TOUCH_UPDATE)
        {
          if (_gdk_event_get_pointer_emulated (source_event))
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
        !_gdk_event_get_pointer_emulated (source_event))))
    {
      pointer_window =
	_gdk_window_find_descendant_at (toplevel_window,
					toplevel_x, toplevel_y,
					NULL, NULL);

      /* Find the event window, that gets the grab */
      w = pointer_window;
      while (w != NULL &&
	     (parent = get_event_parent (w)) != NULL &&
	     parent->window_type != GDK_WINDOW_ROOT)
	{
	  if (w->event_mask & GDK_BUTTON_PRESS_MASK &&
              (type == GDK_BUTTON_PRESS ||
               _gdk_event_get_pointer_emulated (source_event)))
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
                   _gdk_event_get_pointer_emulated (source_event))
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
                                _gdk_event_get_pointer_emulated (source_event),
                                serial);

  if (type == GDK_TOUCH_BEGIN || type == GDK_TOUCH_END)
    {
      if (_gdk_event_get_pointer_emulated (source_event))
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
        _gdk_event_get_pointer_emulated (source_event))) &&
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
             _gdk_event_get_pointer_emulated (source_event))))
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
                 _gdk_event_get_pointer_emulated (source_event))) &&
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

      if ((type == GDK_TOUCH_END &&
           _gdk_event_get_pointer_emulated (source_event)) &&
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
      gdk_event_set_source_device (event, source_device);
      return TRUE;

    default:
      return FALSE;
    }

  return TRUE; /* Always unlink original, we want to obey the emulated event mask */
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
    "offscreen"
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

  if (gdk_event_get_time (event) != GDK_CURRENT_TIME)
    display->last_event_time = gdk_event_get_time (event);

  device = gdk_event_get_device (event);
  source_device = gdk_event_get_source_device (event);

  if (device)
    {
      GdkInputMode mode;

      if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
        {
          pointer_info = _gdk_display_get_pointer_info (display, device);

          if (source_device != pointer_info->last_slave &&
              gdk_device_get_device_type (source_device) == GDK_DEVICE_TYPE_SLAVE)
            pointer_info->last_slave = source_device;
          else if (pointer_info->last_slave)
            source_device = pointer_info->last_slave;
        }

      g_object_get (device, "input-mode", &mode, NULL);
      _gdk_display_device_grab_update (display, device, source_device, serial);

      if (mode == GDK_MODE_DISABLED ||
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

  if (!(is_button_type (event->type) ||
        is_motion_type (event->type)) ||
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
      if (is_toplevel &&
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
  if (is_toplevel)
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
       _gdk_event_get_pointer_emulated (event)))
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

  if ((event->type == GDK_BUTTON_RELEASE ||
       event->type == GDK_TOUCH_END) &&
      !event->any.send_event)
    {
      GdkEventSequence *sequence;

      sequence = gdk_event_get_event_sequence (event);
      if (event->type == GDK_TOUCH_END && sequence)
        {
          _gdk_display_end_touch_grab (display, device, sequence);
        }

      if (event->type == GDK_BUTTON_RELEASE ||
          _gdk_event_get_pointer_emulated (event))
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
  cairo_surface_t *window_surface, *surface;
  double sx, sy;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  window_surface = gdk_window_ref_impl_surface (window);
  sx = sy = 1;
#ifdef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
  cairo_surface_get_device_scale (window_surface, &sx, &sy);
#endif

  switch (_gdk_rendering_mode)
  {
    case GDK_RENDERING_MODE_RECORDING:
      {
        cairo_rectangle_t rect = { 0, 0, width * sx, height *sy };
        surface = cairo_recording_surface_create (content, &rect);
#ifdef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
        cairo_surface_set_device_scale (surface, sx, sy);
#endif
      }
      break;
    case GDK_RENDERING_MODE_IMAGE:
      surface = cairo_image_surface_create (content == CAIRO_CONTENT_COLOR ? CAIRO_FORMAT_RGB24 :
                                            content == CAIRO_CONTENT_ALPHA ? CAIRO_FORMAT_A8 : CAIRO_FORMAT_ARGB32,
                                            width * sx, height * sy);
#ifdef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
      cairo_surface_set_device_scale (surface, sx, sy);
#endif
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

#ifdef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
  if (scale == 0)
    scale = gdk_window_get_scale_factor (window);

  cairo_surface_set_device_scale (surface, scale, scale);
#endif

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
 * of type %GDK_WINDOW_TEMP or windows where override redirect
 * has been turned on via gdk_window_set_override_redirect()
 * since these windows are not resizable by the user.
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
 * gdk_window_set_override_redirect:
 * @window: a toplevel #GdkWindow
 * @override_redirect: %TRUE if window should be override redirect
 *
 * An override redirect window is not under the control of the window manager.
 * This means it won’t have a titlebar, won’t be minimizable, etc. - it will
 * be entirely under the control of the application. The window manager
 * can’t see the override redirect window at all.
 *
 * Override redirect should only be used for short-lived temporary
 * windows, such as popup menus. #GtkMenu uses an override redirect
 * window in its implementation, for example.
 *
 **/
void
gdk_window_set_override_redirect (GdkWindow *window,
				  gboolean override_redirect)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->set_override_redirect (window, override_redirect);
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

  window->event_compression = event_compression;
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
 **/
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
  GdkDeviceManager *device_manager;
  GdkDevice *device;

  device_manager = gdk_display_get_device_manager (gdk_window_get_display (window));
  device = gdk_device_manager_get_client_pointer (device_manager);
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
  GdkDeviceManager *device_manager;
  GdkDevice *device;

  device_manager = gdk_display_get_device_manager (gdk_window_get_display (window));
  device = gdk_device_manager_get_client_pointer (device_manager);
  gdk_window_begin_move_drag_for_device (window, device, button, root_x, root_y, timestamp);
}

/**
 * gdk_window_enable_synchronized_configure:
 * @window: a toplevel #GdkWindow
 *
 * Does nothing, present only for compatiblity.
 *
 * Since: 2.6
 * Deprecated: 3.8: this function is no longer needed
 **/
void
gdk_window_enable_synchronized_configure (GdkWindow *window)
{
}

/**
 * gdk_window_configure_finished:
 * @window: a toplevel #GdkWindow
 *
 * Does nothing, present only for compatiblity.
 *
 * Since: 2.6
 * Deprecated: 3.8: this function is no longer needed
 **/
void
gdk_window_configure_finished (GdkWindow *window)
{
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
 * X screens with a compositing manager running.
 *
 * For child windows this function only works for non-native windows.
 *
 * For setting up per-pixel alpha topelevels, see gdk_screen_get_rgba_visual(),
 * and for non-toplevels, see gdk_window_set_composited().
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
  GdkDeviceManager *device_manager;
  GdkDevice *device;

  device_manager = gdk_display_get_device_manager (gdk_window_get_display (window));
  device = gdk_device_manager_get_client_pointer (device_manager);

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
gdk_drag_begin_for_device (GdkWindow     *window,
                           GdkDevice     *device,
                           GList         *targets)
{
  return GDK_WINDOW_IMPL_GET_CLASS (window->impl)->drag_begin (window, device, targets);
}

/**
 * gdk_test_render_sync:
 * @window: a mapped #GdkWindow
 *
 * Retrieves a pixel from @window to force the windowing
 * system to carry out any pending rendering commands.
 *
 * This function is intended to be used to synchronize with rendering
 * pipelines, to benchmark windowing system rendering operations.
 *
 * Since: 2.14
 **/
void
gdk_test_render_sync (GdkWindow *window)
{
  GDK_WINDOW_IMPL_GET_CLASS (window->impl)->sync_rendering (window);
}

/**
 * gdk_test_simulate_key:
 * @window: a #GdkWindow to simulate a key event for
 * @x:      x coordinate within @window for the key event
 * @y:      y coordinate within @window for the key event
 * @keyval: A GDK keyboard value
 * @modifiers: Keyboard modifiers the event is setup with
 * @key_pressrelease: either %GDK_KEY_PRESS or %GDK_KEY_RELEASE
 *
 * This function is intended to be used in GTK+ test programs.
 * If (@x,@y) are > (-1,-1), it will warp the mouse pointer to
 * the given (@x,@y) coordinates within @window and simulate a
 * key press or release event.
 *
 * When the mouse pointer is warped to the target location, use
 * of this function outside of test programs that run in their
 * own virtual windowing system (e.g. Xvfb) is not recommended.
 * If (@x,@y) are passed as (-1,-1), the mouse pointer will not
 * be warped and @window origin will be used as mouse pointer
 * location for the event.
 *
 * Also, gdk_test_simulate_key() is a fairly low level function,
 * for most testing purposes, gtk_test_widget_send_key() is the
 * right function to call which will generate a key press event
 * followed by its accompanying key release event.
 *
 * Returns: whether all actions necessary for a key event simulation
 *     were carried out successfully
 *
 * Since: 2.14
 */
gboolean
gdk_test_simulate_key (GdkWindow      *window,
                       gint            x,
                       gint            y,
                       guint           keyval,
                       GdkModifierType modifiers,
                       GdkEventType    key_pressrelease)
{
  return GDK_WINDOW_IMPL_GET_CLASS (window->impl)
    ->simulate_key (window, x, y, keyval, modifiers, key_pressrelease);
}

/**
 * gdk_test_simulate_button:
 * @window: a #GdkWindow to simulate a button event for
 * @x:      x coordinate within @window for the button event
 * @y:      y coordinate within @window for the button event
 * @button: Number of the pointer button for the event, usually 1, 2 or 3
 * @modifiers: Keyboard modifiers the event is setup with
 * @button_pressrelease: either %GDK_BUTTON_PRESS or %GDK_BUTTON_RELEASE
 *
 * This function is intended to be used in GTK+ test programs.
 * It will warp the mouse pointer to the given (@x,@y) coordinates
 * within @window and simulate a button press or release event.
 * Because the mouse pointer needs to be warped to the target
 * location, use of this function outside of test programs that
 * run in their own virtual windowing system (e.g. Xvfb) is not
 * recommended.
 *
* Also, gdk_test_simulate_button() is a fairly low level function,
 * for most testing purposes, gtk_test_widget_click() is the right
 * function to call which will generate a button press event followed
 * by its accompanying button release event.
 *
 * Returns: whether all actions necessary for a button event simulation
 *     were carried out successfully
 *
 * Since: 2.14
 */
gboolean
gdk_test_simulate_button (GdkWindow      *window,
                          gint            x,
                          gint            y,
                          guint           button, /*1..3*/
                          GdkModifierType modifiers,
                          GdkEventType    button_pressrelease)
{
  return GDK_WINDOW_IMPL_GET_CLASS (window->impl)
    ->simulate_button (window, x, y, button, modifiers, button_pressrelease);
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
}

static void
gdk_window_paint_on_clock (GdkFrameClock *clock,
			   void          *data)
{
  GdkWindow *window;

  window = GDK_WINDOW (data);

  /* Update window and any children on the same clock.
   */
  gdk_window_process_updates_with_mode (window, PROCESS_UPDATES_WITH_SAME_CLOCK_CHILDREN);
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

/**
 * gdk_window_set_opaque_region:
 * @window: a top-level or non-native #GdkWindow
 * @region: a region
 *
 * For optimizization purposes, compositing window managers may
 * like to not draw obscured regions of windows, or turn off blending
 * during for these regions. With RGB windows with no transparency,
 * this is just the shape of the window, but with ARGB32 windows, the
 * compositor does not know what regions of the window are transparent
 * or not.
 *
 * This function only works for toplevel windows.
 *
 * GTK+ will automatically update this property automatically if
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
    return impl_class->set_opaque_region (window, region);
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
