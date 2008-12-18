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
#include "gdkwindow.h"
#include "gdkwindowimpl.h"
#include "gdkinternals.h"
#include "gdk.h"		/* For gdk_rectangle_union() */
#include "gdkpixmap.h"
#include "gdkdrawable.h"
#include "gdkscreen.h"
#include "gdkalias.h"

#undef DEBUG_WINDOW_PRINTING

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"           /* For workaround */
#endif

#include "math.h"

/* Not all GdkWindows have a corresponding native window.
 * Instead some draw into the nearest parent that has whatss
 * called an "impl", i.e. the implementation window.
 * For toplevel window system windows the impl is always native
 * window, but child windows can also have native windows as
 * this is sometimes necessary. Furthermore, offscreen windows
 * (type GDK_WINDOW_OFFSCREEN) have an impl of type
 * GdkOffscreenWindow rather than a backend implementation native
 * window. Such windows draw into an offscreen pixmap instead
 * of a window and collect damage that lets you paint it where
 * you want.
 *
 * All GdkWindow track their position, size, clip region and
 * absolute position in the impl window. For child window with
 * native windows the clip region is set on the native window
 * as a window shape to make it clip against other non-native windows.
 */  

#define USE_BACKING_STORE	/* Appears to work on Win32, too, now. */

struct _GdkWindowPaint
{
  GdkRegion *region;
  GdkPixmap *pixmap;
  gint x_offset;
  gint y_offset;
  cairo_surface_t *surface;
  guint uses_implicit : 1;
  guint32 region_tag;
};

typedef struct {
  GdkRegion *region; /* The destination region */
  int dx, dy;
} GdkWindowRegionMove;


/* Global info */

static GdkGC *gdk_window_create_gc      (GdkDrawable     *drawable,
                                         GdkGCValues     *values,
                                         GdkGCValuesMask  mask);
static void   gdk_window_draw_rectangle (GdkDrawable     *drawable,
                                         GdkGC           *gc,
                                         gboolean         filled,
                                         gint             x,
                                         gint             y,
                                         gint             width,
                                         gint             height);
static void   gdk_window_draw_arc       (GdkDrawable     *drawable,
                                         GdkGC           *gc,
                                         gboolean         filled,
                                         gint             x,
                                         gint             y,
                                         gint             width,
                                         gint             height,
                                         gint             angle1,
                                         gint             angle2);
static void   gdk_window_draw_polygon   (GdkDrawable     *drawable,
                                         GdkGC           *gc,
                                         gboolean         filled,
                                         GdkPoint        *points,
                                         gint             npoints);
static void   gdk_window_draw_text      (GdkDrawable     *drawable,
                                         GdkFont         *font,
                                         GdkGC           *gc,
                                         gint             x,
                                         gint             y,
                                         const gchar     *text,
                                         gint             text_length);
static void   gdk_window_draw_text_wc   (GdkDrawable     *drawable,
                                         GdkFont         *font,
                                         GdkGC           *gc,
                                         gint             x,
                                         gint             y,
                                         const GdkWChar  *text,
                                         gint             text_length);
static void   gdk_window_draw_drawable  (GdkDrawable     *drawable,
                                         GdkGC           *gc,
                                         GdkPixmap       *src,
                                         gint             xsrc,
                                         gint             ysrc,
                                         gint             xdest,
                                         gint             ydest,
                                         gint             width,
                                         gint             height);
static void   gdk_window_draw_points    (GdkDrawable     *drawable,
                                         GdkGC           *gc,
                                         GdkPoint        *points,
                                         gint             npoints);
static void   gdk_window_draw_segments  (GdkDrawable     *drawable,
                                         GdkGC           *gc,
                                         GdkSegment      *segs,
                                         gint             nsegs);
static void   gdk_window_draw_lines     (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 GdkPoint        *points,
					 gint             npoints);

static void gdk_window_draw_glyphs             (GdkDrawable      *drawable,
						GdkGC            *gc,
						PangoFont        *font,
						gint              x,
						gint              y,
						PangoGlyphString *glyphs);
static void gdk_window_draw_glyphs_transformed (GdkDrawable      *drawable,
						GdkGC            *gc,
						PangoMatrix      *matrix,
						PangoFont        *font,
						gint              x,
						gint              y,
						PangoGlyphString *glyphs);

static void   gdk_window_draw_image     (GdkDrawable     *drawable,
                                         GdkGC           *gc,
                                         GdkImage        *image,
                                         gint             xsrc,
                                         gint             ysrc,
                                         gint             xdest,
                                         gint             ydest,
                                         gint             width,
                                         gint             height);

static void gdk_window_draw_pixbuf (GdkDrawable     *drawable,
				    GdkGC           *gc,
				    GdkPixbuf       *pixbuf,
				    gint             src_x,
				    gint             src_y,
				    gint             dest_x,
				    gint             dest_y,
				    gint             width,
				    gint             height,
				    GdkRgbDither     dither,
				    gint             x_dither,
				    gint             y_dither);

static void gdk_window_draw_trapezoids (GdkDrawable   *drawable,
					GdkGC	      *gc,
					GdkTrapezoid  *trapezoids,
					gint           n_trapezoids);

static GdkImage* gdk_window_copy_to_image (GdkDrawable *drawable,
					   GdkImage    *image,
					   gint         src_x,
					   gint         src_y,
					   gint         dest_x,
					   gint         dest_y,
					   gint         width,
					   gint         height);

static cairo_surface_t *gdk_window_ref_cairo_surface (GdkDrawable *drawable);
static cairo_surface_t *gdk_window_create_cairo_surface (GdkDrawable *drawable,
							 int width,
							 int height);
static void             gdk_window_set_cairo_clip    (GdkDrawable *drawable,
						      cairo_t *cr);

static void   gdk_window_real_get_size  (GdkDrawable     *drawable,
                                         gint            *width,
                                         gint            *height);

static GdkVisual*   gdk_window_real_get_visual   (GdkDrawable *drawable);
static gint         gdk_window_real_get_depth    (GdkDrawable *drawable);
static GdkScreen*   gdk_window_real_get_screen   (GdkDrawable *drawable);
static void         gdk_window_real_set_colormap (GdkDrawable *drawable,
						  GdkColormap *cmap);
static GdkColormap* gdk_window_real_get_colormap (GdkDrawable *drawable);

static GdkDrawable* gdk_window_get_source_drawable    (GdkDrawable *drawable);
static GdkDrawable* gdk_window_get_composite_drawable (GdkDrawable *drawable,
						       gint         x,
						       gint         y,
						       gint         width,
						       gint         height,
						       gint        *composite_x_offset,
						       gint        *composite_y_offset);
static GdkRegion*   gdk_window_get_clip_region        (GdkDrawable *drawable);
static GdkRegion*   gdk_window_get_visible_region     (GdkDrawable *drawable);

static void gdk_window_free_paint_stack (GdkWindow *window);

static void gdk_window_init       (GdkWindowObject      *window);
static void gdk_window_class_init (GdkWindowObjectClass *klass);
static void gdk_window_finalize   (GObject              *object);
static void gdk_window_clear_backing_rect (GdkWindow *window,
					   gint       x,
					   gint       y,
					   gint       width,
					   gint       height);
static void gdk_window_redirect_free      (GdkWindowRedirect *redirect);
static void apply_redirect_to_children    (GdkWindowObject   *private,
					   GdkWindowRedirect *redirect);
static void remove_redirect_from_children (GdkWindowObject   *private,
					   GdkWindowRedirect *redirect);

static void recompute_visible_regions (GdkWindowObject *private,
				       gboolean recalculate_siblings,
				       gboolean recalculate_children);
static void gdk_window_flush          (GdkWindow *window);
static void do_move_region_bits_on_impl (GdkWindowObject *private,
					 GdkDrawable *dest,
					 int dest_off_x, int dest_off_y,
					 GdkRegion *region, /* In impl window coords */
					 int dx, int dy);
static void gdk_window_invalidate_in_parent (GdkWindowObject *private);
  
static gpointer parent_class = NULL;

static const cairo_user_data_key_t gdk_window_cairo_key;

static guint32
new_region_tag (void)
{
  static guint32 tag = 0;

  return ++tag;
}

GType
gdk_window_object_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    object_type = g_type_register_static_simple (GDK_TYPE_DRAWABLE,
						 "GdkWindow",
						 sizeof (GdkWindowObjectClass),
						 (GClassInitFunc) gdk_window_class_init,
						 sizeof (GdkWindowObject),
						 (GInstanceInitFunc) gdk_window_init,
						 0);
  
  return object_type;
}

GType
_gdk_paintable_get_type (void)
{
  static GType paintable_type = 0;

  if (!paintable_type)
    {
      const GTypeInfo paintable_info =
      {
	sizeof (GdkPaintableIface),  /* class_size */
	NULL,                        /* base_init */
	NULL,                        /* base_finalize */
      };

      paintable_type = g_type_register_static (G_TYPE_INTERFACE,
					       g_intern_static_string ("GdkPaintable"),
					       &paintable_info, 0);

      g_type_interface_add_prerequisite (paintable_type, G_TYPE_OBJECT);
    }

  return paintable_type;
}

static void
gdk_window_init (GdkWindowObject *window)
{
  /* 0-initialization is good for all other fields. */

  window->window_type = GDK_WINDOW_CHILD;

  window->state = GDK_WINDOW_STATE_WITHDRAWN;
  window->width = 1;
  window->height = 1;
  window->toplevel_window_type = -1;
}

static GQuark quark_pointer_window = 0;

static void
gdk_window_class_init (GdkWindowObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_window_finalize;

  drawable_class->create_gc = gdk_window_create_gc;
  drawable_class->draw_rectangle = gdk_window_draw_rectangle;
  drawable_class->draw_arc = gdk_window_draw_arc;
  drawable_class->draw_polygon = gdk_window_draw_polygon;
  drawable_class->draw_text = gdk_window_draw_text;
  drawable_class->draw_text_wc = gdk_window_draw_text_wc;
  drawable_class->draw_drawable = gdk_window_draw_drawable;
  drawable_class->draw_points = gdk_window_draw_points;
  drawable_class->draw_segments = gdk_window_draw_segments;
  drawable_class->draw_lines = gdk_window_draw_lines;
  drawable_class->draw_glyphs = gdk_window_draw_glyphs;
  drawable_class->draw_glyphs_transformed = gdk_window_draw_glyphs_transformed;
  drawable_class->draw_image = gdk_window_draw_image;
  drawable_class->draw_pixbuf = gdk_window_draw_pixbuf;
  drawable_class->draw_trapezoids = gdk_window_draw_trapezoids;
  drawable_class->get_depth = gdk_window_real_get_depth;
  drawable_class->get_screen = gdk_window_real_get_screen;
  drawable_class->get_size = gdk_window_real_get_size;
  drawable_class->set_colormap = gdk_window_real_set_colormap;
  drawable_class->get_colormap = gdk_window_real_get_colormap;
  drawable_class->get_visual = gdk_window_real_get_visual;
  drawable_class->_copy_to_image = gdk_window_copy_to_image;
  drawable_class->ref_cairo_surface = gdk_window_ref_cairo_surface;
  drawable_class->create_cairo_surface = gdk_window_create_cairo_surface;
  drawable_class->set_cairo_clip = gdk_window_set_cairo_clip;
  drawable_class->get_clip_region = gdk_window_get_clip_region;
  drawable_class->get_visible_region = gdk_window_get_visible_region;
  drawable_class->get_composite_drawable = gdk_window_get_composite_drawable;
  drawable_class->get_source_drawable = gdk_window_get_source_drawable;

  quark_pointer_window = g_quark_from_static_string ("gtk-pointer-window");
}

static void
gdk_window_finalize (GObject *object)
{
  GdkWindow *window = GDK_WINDOW (object);
  GdkWindowObject *obj = (GdkWindowObject *) object;
  
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

  if (obj->impl)
    {
      g_object_unref (obj->impl);
      obj->impl = NULL;
    }

  if (obj->shape)
    gdk_region_destroy (obj->shape);

  if (obj->input_shape)
    gdk_region_destroy (obj->input_shape);

  if (obj->cursor)
    gdk_cursor_unref (obj->cursor);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gdk_window_is_offscreen (GdkWindowObject *window)
{
  return GDK_WINDOW_TYPE (window) == GDK_WINDOW_OFFSCREEN;
}

static GdkWindowObject *
gdk_window_get_impl_window (GdkWindowObject *window)
{
  while (window->parent != NULL && window->parent->impl == window->impl)
    window = window->parent;

  return window;
}

GdkWindow *
_gdk_window_get_impl_window (GdkWindow *window)
{
  return (GdkWindow *)gdk_window_get_impl_window ((GdkWindowObject *)window);
}

static gboolean
gdk_window_has_impl (GdkWindowObject *window)
{
  return window->parent == NULL || window->parent->impl != window->impl;
}

gboolean
_gdk_window_has_impl (GdkWindow *window)
{
  return gdk_window_has_impl ((GdkWindowObject *)window);
}

static gboolean
gdk_window_has_no_impl (GdkWindowObject *window)
{
  return window->parent->impl == window->impl;
}

static void
remove_child_area (GdkWindowObject *private,
		   GdkWindowObject *until,
		   gboolean for_input,
		   GdkRegion *region)
{
  GdkWindowObject *child;
  GdkRegion *child_region;
  GdkRectangle r;
  GList *l;
  GdkRegion *shape;
  
  for (l = private->children; l; l = l->next)
    {
      child = l->data;

      if (child == until)
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

      child_region = gdk_region_rectangle (&r);
      
      if (child->shape)
	gdk_region_intersect (child_region, child->shape);
      else if (private->window_type == GDK_WINDOW_FOREIGN)
	{
	  shape = _gdk_windowing_window_get_shape ((GdkWindow *)child);
	  if (shape)
	    {
	      gdk_region_intersect (child_region, shape);
	      gdk_region_destroy (shape);
	    }
	}

      if (for_input)
	{
	  if (child->input_shape)
	    gdk_region_intersect (child_region, child->input_shape);
	  else if (private->window_type == GDK_WINDOW_FOREIGN)
	    {
	      shape = _gdk_windowing_window_get_input_shape ((GdkWindow *)child);
	      if (shape)
		{
		  gdk_region_intersect (child_region, shape);
		  gdk_region_destroy (shape);
		}
	    }
	}
      
      gdk_region_subtract (region, child_region);
      gdk_region_destroy (child_region);

    }
}

static void
recompute_visible_regions_internal (GdkWindowObject *private,
				    gboolean recalculate_clip,
				    gboolean recalculate_siblings,
				    gboolean recalculate_children)
{
  GdkRectangle r;
  GList *l;
  GdkWindowObject *child;
  GdkRegion *new_clip, *old_clip_region_with_children;
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
   * window size
   * siblings in parents above window
   */
  clip_region_changed = FALSE;
  if (recalculate_clip)
    {
      /* Calculate visible region (sans children) in parent window coords */
      r.x = private->x;
      r.y = private->y;
      r.width = private->width;
      r.height = private->height;
      new_clip = gdk_region_rectangle (&r);

      if (private->parent != NULL && GDK_WINDOW_TYPE (private->parent) != GDK_WINDOW_ROOT)
	{
	  gdk_region_intersect (new_clip, private->parent->clip_region);
	  
	  /* Remove all overlapping children from parent */
	  remove_child_area (private->parent, private, FALSE, new_clip);
	}
      
      /* Convert from parent coords to window coords */
      gdk_region_offset (new_clip, -private->x, -private->y);

      if (private->shape)
	gdk_region_intersect (new_clip, private->shape);

      if (private->clip_region == NULL ||
	  !gdk_region_equal (private->clip_region, new_clip))
	clip_region_changed = TRUE;
	
      if (private->clip_region)
	gdk_region_destroy (private->clip_region);
      private->clip_region = new_clip;

      old_clip_region_with_children = private->clip_region_with_children;
      private->clip_region_with_children = gdk_region_copy (private->clip_region);
      remove_child_area (private, NULL, FALSE, private->clip_region_with_children);

      if (clip_region_changed ||
	  !gdk_region_equal (private->clip_region_with_children, old_clip_region_with_children))
	  private->clip_tag = new_region_tag ();

      if (old_clip_region_with_children)
	gdk_region_destroy (old_clip_region_with_children);
    }

  /* Update all children, recursively. */
  if (abs_pos_changed || clip_region_changed || recalculate_children)
    {
      for (l = private->children; l; l = l->next)
	{
	  child = l->data;
	  /* Only recalculate clip if the the clip region changed, otherwise
	   * there is no way the child clip region could change (its has not e.g. moved)
	   * Except if recalculate_children is set to force child updates
	   */
	  recompute_visible_regions_internal (child, recalculate_clip && (clip_region_changed || recalculate_children), FALSE, FALSE);
	}
    }

  if (clip_region_changed &&
      gdk_window_has_impl (private) &&
      /* Not for offscreens */
      private->window_type != GDK_WINDOW_OFFSCREEN &&
      /* or for non-shaped toplevels */
      (private->shaped ||
       (private->parent != NULL &&
	GDK_WINDOW_TYPE (private->parent) != GDK_WINDOW_ROOT)) &&
      /* or for foreign windows */
      GDK_WINDOW_TYPE (private) != GDK_WINDOW_FOREIGN
      )
    {
      GDK_WINDOW_IMPL_GET_IFACE (private->impl)->shape_combine_region ((GdkWindow *)private, private->clip_region, 0, 0);
    }

  if (recalculate_siblings &&
      private->parent != NULL &&
      GDK_WINDOW_TYPE (private->parent) != GDK_WINDOW_ROOT)
    {
      /* If we moved a child window in parent or changed the stacking order, then we
       * need to recompute the visible area of all the other children in the parent
       */
      for (l = private->parent->children; l; l = l->next)
	{
	  child = l->data;

	  if (child != private)
	    recompute_visible_regions_internal (child, TRUE, FALSE, FALSE);
	}

      /* We also need to recompute the _with_children clip for the parent */
      recompute_visible_regions_internal (private->parent, TRUE, FALSE, FALSE);
    }

  if (private->cairo_surface)
    {
      int width, height;

      /* It would be nice if we had some cairo support here so we
	 could set the clip rect on the cairo surface */
      width = private->abs_x + private->width;
      height = private->abs_y + private->height;

      _gdk_windowing_set_cairo_surface_size (private->cairo_surface,
					     width, height);
      cairo_surface_set_device_offset (private->cairo_surface, 
				       private->abs_x,
				       private->abs_y);
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
 * Unless the window didn't change stacking order or size/pos, pass in TRUE
 * for recalculate_siblings. (Mostly used internally for the recursion)
 * 
 * If a child window was removed (and you can't use that child for
 * recompute_visible_regions), pass in TRUE for recalculate_children on the parent
 */
static void
recompute_visible_regions (GdkWindowObject *private,
			   gboolean recalculate_siblings,
			   gboolean recalculate_children)
{
  recompute_visible_regions_internal (private,
				      TRUE,
				      recalculate_siblings,
				      recalculate_children);
}

void
_gdk_window_update_size (GdkWindow *window)
{
  recompute_visible_regions ((GdkWindowObject *)window, TRUE, FALSE);
}

static GdkEventMask
get_native_event_mask (GdkWindowObject *private)
{
  if (GDK_WINDOW_TYPE (private->parent) == GDK_WINDOW_ROOT)
    return
      GDK_EXPOSURE_MASK |
      GDK_POINTER_MOTION_MASK |
      GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
      GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
      GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
      GDK_FOCUS_CHANGE_MASK | GDK_STRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK |
      GDK_PROXIMITY_IN_MASK | GDK_PROXIMITY_OUT_MASK | GDK_SCROLL_MASK;
  else
    return GDK_EXPOSURE_MASK;
}


/**
 * gdk_window_new:
 * @parent: a #GdkWindow, or %NULL to create the window as a child of
 *   the default root window for the default display.
 * @attributes: attributes of the new window
 * @attributes_mask: mask indicating which fields in @attributes are valid
 * 
 * Creates a new #GdkWindow using the attributes from
 * @attributes. See #GdkWindowAttr and #GdkWindowAttributesType for
 * more details.  Note: to use this on displays other than the default
 * display, @parent must be specified.
 * 
 * Return value: the new #GdkWindow
 **/
GdkWindow*
gdk_window_new (GdkWindow     *parent,
		GdkWindowAttr *attributes,
		gint           attributes_mask)
{
  GdkWindow *window;
  GdkWindowObject *private;
  GdkScreen *screen;
  GdkVisual *visual;
  int x, y;
  gboolean native;
  GdkEventMask event_mask;
  GdkWindow *real_parent;
  
  g_return_val_if_fail (attributes != NULL, NULL);
  
  if (!parent)
    {
      GDK_NOTE (MULTIHEAD,
		g_warning ("gdk_window_new(): no parent specified reverting to parent = default root window"));
      
      screen = gdk_screen_get_default ();
      parent = gdk_screen_get_root_window (screen);
    }
  else
    screen = gdk_drawable_get_screen (parent);

  g_return_val_if_fail (GDK_IS_WINDOW (parent), NULL);
  
  if (GDK_WINDOW_DESTROYED (parent))
    return NULL;

  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *) window;

  /* Windows with a foreign parent are treated as if they are children
   * of the root window, except for actual creation.
   */
  real_parent = parent;
  if (GDK_WINDOW_TYPE (parent) == GDK_WINDOW_FOREIGN)
    parent = gdk_screen_get_root_window (screen);

  private->parent = (GdkWindowObject *)parent;

  private->accept_focus = TRUE;
  private->focus_on_map = TRUE;

  if (attributes_mask & GDK_WA_X)
    x = attributes->x;
  else
    x = 0;
  
  if (attributes_mask & GDK_WA_Y)
    y = attributes->y;
  else
    y = 0;
  
  private->x = x;
  private->y = y;
  private->width = (attributes->width > 1) ? (attributes->width) : (1);
  private->height = (attributes->height > 1) ? (attributes->height) : (1);

#ifdef GDK_WINDOWING_X11
  /* Work around a bug where Xorg refuses to map toplevel InputOnly windows 
   * from an untrusted client: http://bugs.freedesktop.org/show_bug.cgi?id=6988
   */
  if (attributes->wclass == GDK_INPUT_ONLY &&
      GDK_WINDOW_TYPE (private->parent) == GDK_WINDOW_ROOT &&
      !G_LIKELY (GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (parent))->trusted_client))
    {
      g_warning ("Coercing GDK_INPUT_ONLY toplevel window to GDK_INPUT_OUTPUT to work around bug in Xorg server");
      attributes->wclass = GDK_INPUT_OUTPUT;
    }
#endif
  
  if (attributes->wclass == GDK_INPUT_ONLY)
    {
      /* Backwards compatiblity - we've always ignored
       * attributes->window_type for input-only windows
       * before
       */
      if (GDK_WINDOW_TYPE (parent) == GDK_WINDOW_ROOT)
	private->window_type = GDK_WINDOW_TEMP;
      else
	private->window_type = GDK_WINDOW_CHILD;
    }
  else
    private->window_type = attributes->window_type;

  /* Sanity checks */
  switch (private->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
      if (GDK_WINDOW_TYPE (parent) != GDK_WINDOW_ROOT)
	g_warning (G_STRLOC "Toplevel windows must be created as children of\n"
		   "of a window of type GDK_WINDOW_ROOT or GDK_WINDOW_FOREIGN");
    case GDK_WINDOW_CHILD:
    case GDK_WINDOW_OFFSCREEN:
      break;
    default:
      g_warning (G_STRLOC "cannot make windows of type %d", private->window_type);
      return NULL;
    }
  
  if (attributes_mask & GDK_WA_VISUAL)
    visual = attributes->visual;
  else
    visual = gdk_screen_get_system_visual (screen);

  private->event_mask = attributes->event_mask;

  if (attributes->wclass == GDK_INPUT_OUTPUT)
    {
      private->input_only = FALSE;
      private->depth = visual->depth;
  
      private->bg_color.pixel = 0; // TODO: BlackPixel (xdisplay, screen_x11->screen_num);
      private->bg_color.red = private->bg_color.green = private->bg_color.blue = 0;

      private->bg_pixmap = NULL;
    }
  else
    {
      private->depth = 0;
      private->input_only = TRUE;
    }

  if (private->parent)
    private->parent->children = g_list_prepend (private->parent->children, window);

  native = FALSE; /* Default */
  if (GDK_WINDOW_TYPE (private->parent) == GDK_WINDOW_ROOT)
    native = TRUE; /* Always use native windows for toplevels */
  else if (!private->input_only &&
	   ((attributes_mask & GDK_WA_COLORMAP &&
	     attributes->colormap != gdk_drawable_get_colormap ((GdkDrawable *)private->parent)) ||
	    (attributes_mask & GDK_WA_VISUAL &&
	     attributes->visual != gdk_drawable_get_visual ((GdkDrawable *)private->parent))))
    native = TRUE; /* InputOutput window with different colormap or visual than parent, needs native window */

  if (private->window_type == GDK_WINDOW_OFFSCREEN)
    {
      _gdk_offscreen_window_new (window, screen, visual, attributes, attributes_mask);
    }
  else if (native)
    {
      event_mask = get_native_event_mask (private);
      
      /* Create the impl */
      _gdk_window_impl_new (window, real_parent, screen, visual, event_mask, attributes, attributes_mask);
    }
  else
    {
      private->impl = private->parent->impl;
    }

  recompute_visible_regions (private, TRUE, FALSE);
  
  if (GDK_WINDOW_TYPE (private->parent) != GDK_WINDOW_ROOT)
    {
      /* Inherit redirection from parent */
      private->redirect = private->parent->redirect;
    }
    
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
change_impl (GdkWindowObject *private,
	     GdkDrawable *new)
{
  GList *l;
  GdkWindowObject *child;
  gboolean show;
  GdkDrawable *old_impl;

  old_impl = private->impl;
  private->impl = new;
  
  for (l = private->children; l != NULL; l = l->next)
    {
      child = l->data;

      if (child->impl == old_impl)
	change_impl (child, new);
      else
	{
	  show = GDK_WINDOW_IMPL_GET_IFACE (private->impl)->reparent ((GdkWindow *)child,
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
  GdkWindowObject *private;
  GdkWindowObject *new_parent_private;
  GdkWindowObject *old_parent;
  gboolean show, was_toplevel, was_mapped;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (new_parent == NULL || GDK_IS_WINDOW (new_parent));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_ROOT);

  if (GDK_WINDOW_DESTROYED (window) ||
      (new_parent && GDK_WINDOW_DESTROYED (new_parent)))
    return;

  if (!new_parent)
    new_parent = gdk_screen_get_root_window (GDK_WINDOW_SCREEN (window));
  
  private = (GdkWindowObject *) window;
  new_parent_private = (GdkWindowObject *)new_parent;

  /* No input-output children of input-only windows */
  if (new_parent_private->input_only && !private->input_only)
    return;

  /* Don't create loops in hierarchy */
  if (is_parent_of (window, new_parent))
    return;
  
  if (private->cairo_surface)
    {
      /* This might be wrong in the new parent, e.g. for non-native surfaces.
	 To make sure we're ok, just wipe it. */
      cairo_surface_finish (private->cairo_surface);
      cairo_surface_set_user_data (private->cairo_surface, &gdk_window_cairo_key,
				   NULL, NULL);
    }
  
  old_parent = private->parent;

  /* Break up redirection if inherited */
  if (private->redirect && private->redirect->redirected != private)
    {
      remove_redirect_from_children (private, private->redirect);
      private->redirect = NULL;
    }

  was_toplevel = private->parent == NULL;
  was_mapped = GDK_WINDOW_IS_MAPPED (window);
  show = FALSE;

  /* Reparenting to toplevel. Ensure we have a native window so this can work */
  if (new_parent_private->window_type == GDK_WINDOW_ROOT ||
      new_parent_private->window_type == GDK_WINDOW_FOREIGN)
    gdk_window_set_has_native (window, TRUE);
  
  if (gdk_window_has_impl (private))
    {
      /* Native window */
      show = GDK_WINDOW_IMPL_GET_IFACE (private->impl)->reparent (window, new_parent, x, y);
    }
  else
    {
      /* This shouldn't happen, as we created a native in this case, check anyway to see if that ever fails */
      g_assert (new_parent_private->window_type != GDK_WINDOW_ROOT &&
		new_parent_private->window_type != GDK_WINDOW_FOREIGN);

      show = was_mapped;
      gdk_window_hide (window);

      change_impl (private, new_parent_private->impl);
    }

  /* From here on, we treat parents of type GDK_WINDOW_FOREIGN like
   * the root window
   */
  if (GDK_WINDOW_TYPE (new_parent) == GDK_WINDOW_FOREIGN)
    {
      new_parent = gdk_screen_get_root_window (GDK_WINDOW_SCREEN (window));
      new_parent_private = (GdkWindowObject *)new_parent;
    }

  if (private->parent)
    private->parent->children = g_list_remove (private->parent->children, window);
  
  private->parent = new_parent_private;
  private->x = x;
  private->y = y;

  new_parent_private->children = g_list_prepend (new_parent_private->children, window);
  
  /* Switch the window type as appropriate */

  switch (GDK_WINDOW_TYPE (new_parent))
    {
    case GDK_WINDOW_ROOT:
    case GDK_WINDOW_FOREIGN:
      if (private->toplevel_window_type != -1)
	GDK_WINDOW_TYPE (window) = private->toplevel_window_type;
      else if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD)
	GDK_WINDOW_TYPE (window) = GDK_WINDOW_TOPLEVEL;
      break;
    case GDK_WINDOW_OFFSCREEN:
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_CHILD:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
      if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD && \
	  GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN)
	{
	  /* Save the original window type so we can restore it if the
	   * window is reparented back to be a toplevel
	   */
	  private->toplevel_window_type = GDK_WINDOW_TYPE (window);
	  GDK_WINDOW_TYPE (window) = GDK_WINDOW_CHILD;
	}
    }

  /* We might have changed window type for a native windows, so we
     need to change the event mask too. */
  if (gdk_window_has_impl (private))
    GDK_WINDOW_IMPL_GET_IFACE (private->impl)->set_events (window, get_native_event_mask (private));
  
  /* Inherit parent redirect if we don't have our own */
  if (private->parent && private->redirect == NULL)
    {
      private->redirect = private->parent->redirect;
      apply_redirect_to_children (private, private->redirect);
    }

  recompute_visible_regions (private, TRUE, FALSE);
  if (old_parent && GDK_WINDOW_TYPE (old_parent) != GDK_WINDOW_ROOT)
    recompute_visible_regions (old_parent, FALSE, TRUE);
  
  if (show)
    gdk_window_show_unraised (window);
  else
    _gdk_syntesize_crossing_events_for_geometry_change (window);
}

/**
 * gdk_window_set_has_native:
 * @window: a #GdkWindow
 * @has_native: whethe the window should have a native window
 *
 * Tries to create or remove a window-system native window for this
 * GdkWindow. This may fail in some situations. For instance:
 *
 * Toplevel and foreign windows must have a native window.
 * Offscreen window and children of them can never have native windows.
 * Some backends may not support native child windows.
 * 
 **/
void
gdk_window_set_has_native (GdkWindow *window, gboolean has_native)
{
  GdkWindowObject *private;
  GdkWindowObject *impl_window;
  GdkDrawable *new_impl, *old_impl;
  GdkScreen *screen;
  GdkVisual *visual;
  GdkWindowAttr attributes;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_ROOT ||
      GDK_WINDOW_DESTROYED (window))
    return;

  private = (GdkWindowObject *) window;
  
  if (has_native)
    {
      /* Create native window */

      if (gdk_window_has_impl (private))
	/* Already has an impl, either native (ok) or
	   offscreen (not supported). Bail. */
	return; 

      impl_window = gdk_window_get_impl_window (private);
      if (impl_window->window_type == GDK_WINDOW_OFFSCREEN)
	return; /* native in offscreens not supported */

      screen = gdk_drawable_get_screen (window);
      visual = gdk_drawable_get_visual (window);

      attributes.colormap = gdk_drawable_get_colormap (window);
      
      old_impl = private->impl;
      _gdk_window_impl_new (window, (GdkWindow *)private->parent, screen, visual,
			    get_native_event_mask (private), &attributes, GDK_WA_COLORMAP);
      new_impl = private->impl;
      
      private->impl = old_impl;
      change_impl (private, new_impl);

      GDK_WINDOW_IMPL_GET_IFACE (private->impl)->input_shape_combine_region ((GdkWindow *)private, private->input_shape, 0, 0);
    }
  else
    {
      /* Remove native window */

      if (!gdk_window_has_impl (private))
	return;  /* Not native, can't remove */

      if (private->window_type == GDK_WINDOW_OFFSCREEN)
	return; /* Not native, can't remove */
      
      if (private->parent == NULL ||
	  GDK_WINDOW_TYPE (private->parent) == GDK_WINDOW_ROOT)
	return; /* toplevel, must be native */

      g_warning ("Tried to turn native window to client side window, this is not supported yet.");

      /* TODO: remove native */
    }
}

static void
window_remove_filters (GdkWindow *window)
{
  GdkWindowObject *obj = (GdkWindowObject*) window;

  if (obj->filters)
    {
      GList *tmp_list;
      
      for (tmp_list = obj->filters; tmp_list; tmp_list = tmp_list->next)
	g_free (tmp_list->data);
    
      g_list_free (obj->filters);
      obj->filters = NULL;
    }
}

/**
 * _gdk_window_destroy_hierarchy:
 * @window: a #GdkWindow
 * @recursing: If TRUE, then this is being called because a parent
 *            was destroyed. This generally means that the call to the 
 *            windowing system to destroy the window can be omitted, since
 *            it will be destroyed as a result of the parent being destroyed.
 *            Unless @foreign_destroy.           
 * @foreign_destroy: If TRUE, the window or a parent was destroyed by some 
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
			       gboolean   foreign_destroy)
{
  GdkWindowObject *private;
  GdkWindowObject *temp_private;
  GdkWindow *temp_window;
  GdkScreen *screen;
  GdkDisplay *display;
  GList *children;
  GList *tmp;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject*) window;
  
  if (GDK_WINDOW_DESTROYED (window))
    return;
    
  display = gdk_drawable_get_display (GDK_DRAWABLE (window));
  screen = gdk_drawable_get_screen (GDK_DRAWABLE (window));
  temp_window = g_object_get_qdata (G_OBJECT (screen), quark_pointer_window);
  if (temp_window == window)
    g_object_set_qdata (G_OBJECT (screen), quark_pointer_window, NULL);


  switch (GDK_WINDOW_TYPE (window))
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
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
    case GDK_WINDOW_FOREIGN:
    case GDK_WINDOW_OFFSCREEN:
      if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_FOREIGN && !foreign_destroy)
	{
	  /* Logically, it probably makes more sense to send
	   * a "destroy yourself" message to the foreign window
	   * whether or not it's in our hierarchy; but for historical
	   * reasons, we only send "destroy yourself" messages to
	   * foreign windows in our hierarchy.
	   */
	  if (private->parent)
	    _gdk_windowing_window_destroy_foreign (window);

	  /* Also for historical reasons, we remove any filters
	   * on a foreign window when it or a parent is destroyed;
	   * this likely causes problems if two separate portions
	   * of code are maintaining filter lists on a foreign window.
	   */
	  window_remove_filters (window);
	}
      else
	{
	  if (private->parent)
	    {
	      GdkWindowObject *parent_private = (GdkWindowObject *)private->parent;

	      if (parent_private->children)
		parent_private->children = g_list_remove (parent_private->children, window);

	      if (!recursing &&
		  GDK_WINDOW_IS_MAPPED (window))
		{
		  recompute_visible_regions (private, TRUE, FALSE);
		  gdk_window_invalidate_in_parent (private);
		}
	    }

	  gdk_window_free_paint_stack (window);
	  
	  if (private->bg_pixmap &&
              private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
              private->bg_pixmap != GDK_NO_BG)
	    {
	      g_object_unref (private->bg_pixmap);
	      private->bg_pixmap = NULL;
	    }
	  
	  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_FOREIGN)
	    g_assert (private->children == NULL);
	  else
	    {
	      children = tmp = private->children;
	      private->children = NULL;
	      
	      while (tmp)
		{
		  temp_window = tmp->data;
		  tmp = tmp->next;
		  
		  temp_private = (GdkWindowObject*) temp_window;
		  if (temp_private)
		    _gdk_window_destroy_hierarchy (temp_window,
                                                   TRUE, foreign_destroy);
		}
	      
	      g_list_free (children);
	    }

	  _gdk_window_clear_update_area (window); 
	  
	  if (private->cairo_surface)
	    {
	      cairo_surface_finish (private->cairo_surface);
	      cairo_surface_set_user_data (private->cairo_surface, &gdk_window_cairo_key,
					   NULL, NULL);
	    }

	  if (gdk_window_has_impl (private))
	    {
	      GDK_WINDOW_IMPL_GET_IFACE (private->impl)->destroy (window, recursing, foreign_destroy);
	    }
	  else
	    {
	      /* hide to make sure we repaint and break grabs */
	      gdk_window_hide (window);
	      /* NULL out impl so we don't double free it on finalize */
	      private->impl = NULL;
	    }

	  private->state |= GDK_WINDOW_STATE_WITHDRAWN;
	  private->parent = NULL;
	  private->destroyed = TRUE;

	  window_remove_filters (window);

          gdk_drawable_set_colormap (GDK_DRAWABLE (window), NULL);

	  /* If we own the redirect, free it */
	  if (private->redirect && private->redirect->redirected == private)
	    gdk_window_redirect_free (private->redirect);

	  private->redirect = NULL;

	  if (display->pointer_info.toplevel_under_pointer == window)
	    {
	      g_object_unref (display->pointer_info.toplevel_under_pointer);
	      display->pointer_info.toplevel_under_pointer = NULL;
	    }
	}
      break;
    }
}

/**
 * _gdk_window_destroy:
 * @window: a #GdkWindow
 * @foreign_destroy: If TRUE, the window or a parent was destroyed by some
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
  _gdk_window_destroy_hierarchy (window, FALSE, foreign_destroy);
}

/**
 * gdk_window_destroy:
 * @window: a #GdkWindow
 *
 * Destroys the window system resources associated with @window and decrements @window's
 * reference count. The window system resources for all children of @window are also
 * destroyed, but the children's reference counts are not decremented.
 *
 * Note that a window will not be destroyed automatically when its reference count
 * reaches zero. You must call this function yourself before that happens.
 *
 **/
void
gdk_window_destroy (GdkWindow *window)
{
  _gdk_window_destroy_hierarchy (window, FALSE, FALSE);
  g_object_unref (window);
}

/**
 * gdk_window_set_user_data:
 * @window: a #GdkWindow
 * @user_data: user data
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

  ((GdkWindowObject*)window)->user_data = user_data;
}

/**
 * gdk_window_get_user_data:
 * @window: a #GdkWindow
 * @data: return location for user data
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

  *data = ((GdkWindowObject*)window)->user_data;
}

/**
 * gdk_window_get_window_type:
 * @window: a #GdkWindow
 * 
 * Gets the type of the window. See #GdkWindowType.
 * 
 * Return value: type of window
 **/
GdkWindowType
gdk_window_get_window_type (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), (GdkWindowType) -1);
  
  return GDK_WINDOW_TYPE (window);
}

/**
 * gdk_window_get_position:
 * @window: a #GdkWindow
 * @x: X coordinate of window
 * @y: Y coordinate of window
 *
 * Obtains the position of the window as reported in the
 * most-recently-processed #GdkEventConfigure. Contrast with
 * gdk_window_get_geometry() which queries the X server for the
 * current window position, regardless of which events have been
 * received or processed.
 *
 * The position coordinates are relative to the window's parent window.
 * 
 **/
void
gdk_window_get_position (GdkWindow *window,
			 gint      *x,
			 gint      *y)
{
  GdkWindowObject *obj;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  obj = (GdkWindowObject*) window;
  
  if (x)
    *x = obj->x;
  if (y)
    *y = obj->y;
}

/**
 * gdk_window_get_parent:
 * @window: a #GdkWindow
 * 
 * Obtains the parent of @window, as known to GDK. Does not query the
 * X server; thus this returns the parent as passed to gdk_window_new(),
 * not the actual parent. This should never matter unless you're using
 * Xlib calls mixed with GDK calls on the X11 platform. It may also
 * matter for toplevel windows, because the window manager may choose
 * to reparent them.
 * 
 * Return value: parent of @window
 **/
GdkWindow*
gdk_window_get_parent (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
  
  return (GdkWindow*) ((GdkWindowObject*) window)->parent;
}

/**
 * gdk_window_get_toplevel:
 * @window: a #GdkWindow
 * 
 * Gets the toplevel window that's an ancestor of @window.
 * 
 * Return value: the toplevel window containing @window
 **/
GdkWindow*
gdk_window_get_toplevel (GdkWindow *window)
{
  GdkWindowObject *obj;
  
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  obj = (GdkWindowObject *)window;
  while (GDK_WINDOW_TYPE (obj) == GDK_WINDOW_CHILD)
    obj = (GdkWindowObject *)obj->parent;
  
  return GDK_WINDOW (obj);
}

/**
 * gdk_window_get_children:
 * @window: a #GdkWindow
 * 
 * Gets the list of children of @window known to GDK.
 * This function only returns children created via GDK,
 * so for example it's useless when used with the root window;
 * it only returns windows an application created itself.
 *
 * The returned list must be freed, but the elements in the
 * list need not be.
 * 
 * Return value: list of child windows inside @window
 **/
GList*
gdk_window_get_children (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  return g_list_copy (GDK_WINDOW_OBJECT (window)->children);
}

/**
 * gdk_window_peek_children:
 * @window: a #GdkWindow
 * 
 * Like gdk_window_get_children(), but does not copy the list of
 * children, so the list does not need to be freed.
 * 
 * Return value: a reference to the list of child windows in @window
 **/
GList *
gdk_window_peek_children (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  return GDK_WINDOW_OBJECT (window)->children;
}

/**
 * gdk_window_add_filter:
 * @window: a #GdkWindow
 * @function: filter callback
 * @data: data to pass to filter callback
 *
 * Adds an event filter to @window, allowing you to intercept events
 * before they reach GDK. This is a low-level operation and makes it
 * easy to break GDK and/or GTK+, so you have to know what you're
 * doing. Pass %NULL for @window to get all events for all windows,
 * instead of events for a specific window.
 *
 * See gdk_display_add_client_message_filter() if you are interested
 * in X ClientMessage events.
 **/
void          
gdk_window_add_filter (GdkWindow     *window,
		       GdkFilterFunc  function,
		       gpointer       data)
{
  GdkWindowObject *private;
  GList *tmp_list;
  GdkEventFilter *filter;
  
  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));

  private = (GdkWindowObject*) window;
  if (private && GDK_WINDOW_DESTROYED (window))
    return;

  /* Filters are for the native events on the native window, so
     ensure there is a native window. */
  if (window)
    gdk_window_set_has_native (window, TRUE);
  
  if (private)
    tmp_list = private->filters;
  else
    tmp_list = _gdk_default_filters;
  
  while (tmp_list)
    {
      filter = (GdkEventFilter *)tmp_list->data;
      if ((filter->function == function) && (filter->data == data))
	return;
      tmp_list = tmp_list->next;
    }
  
  filter = g_new (GdkEventFilter, 1);
  filter->function = function;
  filter->data = data;
  
  if (private)
    private->filters = g_list_append (private->filters, filter);
  else
    _gdk_default_filters = g_list_append (_gdk_default_filters, filter);
}

/**
 * gdk_window_remove_filter:
 * @window: a #GdkWindow
 * @function: previously-added filter function
 * @data: user data for previously-added filter function
 *
 * Remove a filter previously added with gdk_window_add_filter().
 * 
 **/
void
gdk_window_remove_filter (GdkWindow     *window,
			  GdkFilterFunc  function,
			  gpointer       data)
{
  GdkWindowObject *private;
  GList *tmp_list, *node;
  GdkEventFilter *filter;
  
  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));

  private = (GdkWindowObject*) window;
  
  if (private)
    tmp_list = private->filters;
  else
    tmp_list = _gdk_default_filters;
  
  while (tmp_list)
    {
      filter = (GdkEventFilter *)tmp_list->data;
      node = tmp_list;
      tmp_list = tmp_list->next;
      
      if ((filter->function == function) && (filter->data == data))
	{
	  if (private)
	    private->filters = g_list_remove_link (private->filters, node);
	  else
	    _gdk_default_filters = g_list_remove_link (_gdk_default_filters, node);
	  g_list_free_1 (node);
	  g_free (filter);
	  
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
 * Return value: list of toplevel windows, free with g_list_free()
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

  tmp_list = ((GdkWindowObject *)root_window)->children;
  while (tmp_list)
    {
      if (GDK_WINDOW_TYPE (tmp_list->data) != GDK_WINDOW_FOREIGN)
	new_list = g_list_prepend (new_list, tmp_list->data);
      tmp_list = tmp_list->next;
    }
  
  return new_list;
}

/**
 * gdk_window_get_toplevels:
 * 
 * Obtains a list of all toplevel windows known to GDK on the default
 * screen (see gdk_screen_get_toplevel_windows()).
 * A toplevel window is a child of the root window (see
 * gdk_get_default_root_window()).
 *
 * The returned list should be freed with g_list_free(), but
 * its elements need not be freed.
 * 
 * Return value: list of toplevel windows, free with g_list_free()
 *
 * Deprecated: 2.16: Use gdk_screen_get_toplevel_windows() instead.
 **/
GList *
gdk_window_get_toplevels (void)
{
  return gdk_screen_get_toplevel_windows (gdk_screen_get_default ());
}

/**
 * gdk_window_is_visible:
 * @window: a #GdkWindow
 * 
 * Checks whether the window has been mapped (with gdk_window_show() or
 * gdk_window_show_unraised()).
 * 
 * Return value: %TRUE if the window is mapped
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
 * Return value: %TRUE if the window is viewable
 **/
gboolean 
gdk_window_is_viewable (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkScreen *screen;
  GdkWindow *root_window;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  screen = gdk_drawable_get_screen (window);
  root_window = gdk_screen_get_root_window (screen);
  
  while (private && 
	 (private != (GdkWindowObject *)root_window) &&
	 (GDK_WINDOW_TYPE (private) != GDK_WINDOW_FOREIGN))
    {
      if (GDK_WINDOW_DESTROYED (private) || !GDK_WINDOW_IS_MAPPED (private))
	return FALSE;
      
      private = (GdkWindowObject *)private->parent;
    }
  
  return TRUE;
}

/**
 * gdk_window_get_state:
 * @window: a #GdkWindow
 * 
 * Gets the bitwise OR of the currently active window state flags,
 * from the #GdkWindowState enumeration.
 * 
 * Return value: window state bitfield
 **/
GdkWindowState
gdk_window_get_state (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);
  
  return private->state;
}


/* This creates an empty "implicit" paint region for the impl window.
 * By itself this does nothing, but real paints to this window
 * or children of it can use this pixmap as backing to avoid allocating
 * multiple pixmaps for subwindow rendering. When doing so they
 * add to the region of the implicit paint region, which will be
 * pushed to the window when the implicit paint region is ended.
 * Such paints should not copy anything to the window on paint end, but
 * should rely on the implicit paint end.
 * The implicit paint will be automatically ended if someone draws
 * directly to the window or a child window.
 */
static gboolean
gdk_window_begin_implicit_paint (GdkWindow *window, GdkRectangle *rect)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowPaint *paint;
  GdkRectangle r, clipbox;
  GList *l;
  GdkWindowRegionMove *move;

  g_assert (gdk_window_has_impl (private));

  if (GDK_IS_PAINTABLE (private->impl))
    return FALSE; /* Implementation does double buffering */
  
  if (private->paint_stack != NULL ||
      private->implicit_paint != NULL)
    return FALSE; /* Don't stack implicit paints */

  r = *rect;
  for (l = private->outstanding_moves; l != NULL; l = l->next)
    {
      move = l->data;

      gdk_region_get_clipbox (move->region, &clipbox);
      gdk_rectangle_union (&r, &clipbox, &r);
    }
  
  paint = g_new (GdkWindowPaint, 1);
  paint->region = gdk_region_new (); /* Empty */
  paint->x_offset = r.x;
  paint->y_offset = r.y;
  paint->uses_implicit = FALSE;
  paint->surface = NULL;
  paint->pixmap =
    gdk_pixmap_new (window,
		    MAX (r.width, 1), MAX (r.height, 1), -1);
  
  _gdk_pixmap_set_as_backing (paint->pixmap,
			      window, r.x, r.y);

  for (l = private->outstanding_moves; l != NULL; l = l->next)
    {
      move = l->data;

      gdk_region_union (paint->region, move->region);
      g_object_ref (paint->pixmap);
      do_move_region_bits_on_impl (private,
				   paint->pixmap,
				   paint->x_offset, paint->y_offset,
				   move->region, /* In impl window coords */
				   move->dx, move->dy);
      gdk_region_destroy (move->region);
      g_slice_free (GdkWindowRegionMove, move);
    }
  private->outstanding_moves = NULL;

  private->implicit_paint = paint;
  
  return TRUE;
}

/* Ensure that all content related to this (sub)window is pushed to the
   native region */
static void
gdk_window_flush_implicit_paint (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *impl_window;
  GdkWindowPaint *paint;
  GdkRegion *region;
  GdkGC *tmp_gc;

  /* Ensure that there is no explicit paint region. */
  g_assert (private->paint_stack == NULL);
  
  impl_window = gdk_window_get_impl_window (private);
  if (impl_window->implicit_paint == NULL)
    return;

  paint = impl_window->implicit_paint;
  region = gdk_region_copy (private->clip_region_with_children);
  gdk_region_offset (region, private->abs_x, private->abs_y);
  gdk_region_intersect (region, paint->region);
  
  if (!gdk_region_empty (region))
    {
      /* Some regions are valid, push these to window now */
      tmp_gc = _gdk_drawable_get_scratch_gc ((GdkDrawable *)window, FALSE);
      _gdk_gc_set_clip_region_internal (tmp_gc, region, TRUE);
      gdk_draw_drawable (private->impl, tmp_gc, paint->pixmap,
			 0, 0, paint->x_offset, paint->y_offset, -1, -1);
      /* Reset clip region of the cached GdkGC */
      gdk_gc_set_clip_region (tmp_gc, NULL);

      /* Remove flushed region from the implicit paint */
      gdk_region_subtract (paint->region, region);
    }
  else
    gdk_region_destroy (region);
}

/* Ends an implicit paint, paired with gdk_window_begin_implicit_paint returning TRUE */
static void
gdk_window_end_implicit_paint (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowPaint *paint;
  GdkGC *tmp_gc;

  g_assert (gdk_window_has_impl (private));

  g_assert (private->implicit_paint != NULL);

  paint = private->implicit_paint;

  private->implicit_paint = NULL;
  
  if (!gdk_region_empty (paint->region))
    {
      /* Some regions are valid, push these to window now */
      tmp_gc = _gdk_drawable_get_scratch_gc ((GdkDrawable *)window, FALSE);
      _gdk_gc_set_clip_region_internal (tmp_gc, paint->region, TRUE);
      gdk_draw_drawable (private->impl, tmp_gc, paint->pixmap,
			 0, 0, paint->x_offset, paint->y_offset, -1, -1);
      /* Reset clip region of the cached GdkGC */
      gdk_gc_set_clip_region (tmp_gc, NULL);
    }
  
  g_object_unref (paint->pixmap);
  g_free (paint);
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
  GdkRegion *region;

  g_return_if_fail (GDK_IS_WINDOW (window));

  region = gdk_region_rectangle (rectangle);
  gdk_window_begin_paint_region (window, region);
  gdk_region_destroy (region);
}

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif

/**
 * gdk_window_begin_paint_region:
 * @window: a #GdkWindow
 * @region: region you intend to draw to
 *
 * Indicates that you are beginning the process of redrawing @region.
 * A backing store (offscreen buffer) large enough to contain @region
 * will be created. The backing store will be initialized with the
 * background color or background pixmap for @window. Then, all
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
 * emissions of the expose_event signal. That is, if you're writing an
 * expose event handler, you can assume that the exposed area in
 * #GdkEventExpose has already been cleared to the window background,
 * is already set as the clip region, and already has a backing store.
 * Therefore in most cases, application code need not call
 * gdk_window_begin_paint_region(). (You can disable the automatic
 * calls around expose events on a widget-by-widget basis by calling
 * gtk_widget_set_double_buffered().)
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
                               const GdkRegion *region)
{
#ifdef USE_BACKING_STORE
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkRectangle clip_box;
  GdkWindowPaint *paint, *implicit_paint;
  GdkWindowObject *impl_window;
  GSList *list;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_IS_PAINTABLE (private->impl)) 
    {
      GdkPaintableIface *iface = GDK_PAINTABLE_GET_IFACE (private->impl);

      if (iface->begin_paint_region)
        iface->begin_paint_region ((GdkPaintable*)private->impl, region);
      
      return;
    }

  gdk_region_get_clipbox (region, &clip_box);

  impl_window = gdk_window_get_impl_window (private);
  implicit_paint = impl_window->implicit_paint;

  paint = g_new (GdkWindowPaint, 1);
  paint->region = gdk_region_copy (region);
  paint->region_tag = new_region_tag ();
      
  if (implicit_paint)
    {
      int width, height;
      
      paint->uses_implicit = TRUE;
      paint->pixmap = g_object_ref (implicit_paint->pixmap);
      paint->x_offset = -private->abs_x + implicit_paint->x_offset;
      paint->y_offset = -private->abs_y + implicit_paint->y_offset;
      gdk_region_intersect (paint->region, private->clip_region_with_children);
      
      /* It would be nice if we had some cairo support here so we
	 could set the clip rect on the cairo surface */
      width = private->abs_x + private->width;
      height = private->abs_y + private->height;
      
      paint->surface = _gdk_drawable_create_cairo_surface (paint->pixmap, width, height);

      /* Mark the region as valid on the implicit paint */
      gdk_region_offset (paint->region, private->abs_x, private->abs_y); 
      gdk_region_union (implicit_paint->region, paint->region);
      gdk_region_offset (paint->region, -private->abs_x, -private->abs_y); 
    }
  else
    {
      paint->uses_implicit = FALSE;
      paint->x_offset = clip_box.x;
      paint->y_offset = clip_box.y;
      paint->pixmap =
	gdk_pixmap_new (window,
			MAX (clip_box.width, 1), MAX (clip_box.height, 1), -1);
      paint->surface = _gdk_drawable_ref_cairo_surface (paint->pixmap);
    }
 
  if (paint->surface)
    cairo_surface_set_device_offset (paint->surface,
				     -paint->x_offset, -paint->y_offset);
  
  for (list = private->paint_stack; list != NULL; list = list->next)
    {
      GdkWindowPaint *tmp_paint = list->data;

      gdk_region_subtract (tmp_paint->region, paint->region);
    }
  
  private->paint_stack = g_slist_prepend (private->paint_stack, paint);

  if (!gdk_region_empty (paint->region))
    {
      gdk_window_clear_backing_rect (window,
				     clip_box.x, clip_box.y,
				     clip_box.width, clip_box.height);
    }

#endif /* USE_BACKING_STORE */
}

static void
setup_redirect_clip (GdkWindow      *window,
		     GdkGC          *gc,
		     int            *x_offset_out,
		     int            *y_offset_out)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkRegion *visible_region;
  GdkRectangle dest_rect;
  GdkRegion *tmpreg;
  GdkWindow *toplevel;
  int x_offset, y_offset;

  toplevel = GDK_WINDOW (private->redirect->redirected);
  
  /* Get the clip region for gc clip rect + window hierarchy in
     window relative coords */
  visible_region =
    _gdk_window_calculate_full_clip_region (window, toplevel,
					    TRUE,
					    &x_offset, 
					    &y_offset);

  /* Compensate for the source pos/size */
  x_offset -= private->redirect->src_x;
  y_offset -= private->redirect->src_y;
  dest_rect.x = -x_offset;
  dest_rect.y = -y_offset;
  dest_rect.width = private->redirect->width;
  dest_rect.height = private->redirect->height;
  tmpreg = gdk_region_rectangle (&dest_rect);
  gdk_region_intersect (visible_region, tmpreg);
  gdk_region_destroy (tmpreg);

  /* Compensate for the dest pos */
  x_offset += private->redirect->dest_x;
  y_offset += private->redirect->dest_y;

  gdk_gc_set_clip_region (gc, visible_region); /* This resets clip origin! */

  /* offset clip and tiles from window coords to pixmaps coords */
  gdk_gc_offset (gc, -x_offset, -y_offset);

  gdk_region_destroy (visible_region);

  *x_offset_out = x_offset;
  *y_offset_out = y_offset;
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
#ifdef USE_BACKING_STORE
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *composited;
  GdkWindowPaint *paint;
  GdkGC *tmp_gc;
  GdkRectangle clip_box;
  gint x_offset, y_offset;
  GdkRegion *full_clip;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_IS_PAINTABLE (private->impl))
    {
      GdkPaintableIface *iface = GDK_PAINTABLE_GET_IFACE (private->impl);

      if (iface->end_paint)
        iface->end_paint ((GdkPaintable*)private->impl);
      return;
    }

  if (private->paint_stack == NULL)
    {
      g_warning (G_STRLOC": no preceding call to gdk_window_begin_paint_region(), see documentation");
      return;
    }

  paint = private->paint_stack->data;

  private->paint_stack = g_slist_delete_link (private->paint_stack, 
					      private->paint_stack);

  gdk_region_get_clipbox (paint->region, &clip_box);

  tmp_gc = _gdk_drawable_get_scratch_gc (window, FALSE);

  x_offset = -private->abs_x;
  y_offset = -private->abs_y;

  if (!paint->uses_implicit)
    {
      full_clip = gdk_region_copy (private->clip_region_with_children);
      gdk_region_intersect (full_clip, paint->region);
      _gdk_gc_set_clip_region_internal (tmp_gc, full_clip, TRUE); /* Takes ownership of full_clip */
      gdk_gc_set_clip_origin (tmp_gc, - x_offset, - y_offset);
      gdk_draw_drawable (private->impl, tmp_gc, paint->pixmap,
			 clip_box.x - paint->x_offset,
			 clip_box.y - paint->y_offset,
			 clip_box.x - x_offset, clip_box.y - y_offset,
			 clip_box.width, clip_box.height);
    }
  
  if (private->redirect)
    {
      int x_offset, y_offset;

      /* TODO: Should also use paint->region for clipping */
      setup_redirect_clip (window, tmp_gc, &x_offset, &y_offset);
      gdk_draw_drawable (private->redirect->pixmap, tmp_gc, paint->pixmap,
			 clip_box.x - paint->x_offset,
			 clip_box.y - paint->y_offset,
			 clip_box.x + x_offset,
			 clip_box.y + y_offset,
			 clip_box.width, clip_box.height);
    }
  
  /* Reset clip region of the cached GdkGC */
  gdk_gc_set_clip_region (tmp_gc, NULL);

  cairo_surface_destroy (paint->surface);
  g_object_unref (paint->pixmap);
  gdk_region_destroy (paint->region);
  g_free (paint);

  /* find a composited window in our hierarchy to signal its
   * parent to redraw, calculating the clip box as we go...
   *
   * stop if parent becomes NULL since then we'd have nowhere
   * to draw (ie: 'composited' will always be non-NULL here).
   */
  for (composited = private;
       composited->parent;
       composited = composited->parent)
    {
      int width, height;

      gdk_drawable_get_size (GDK_DRAWABLE (composited->parent),
			     &width, &height);

      clip_box.x += composited->x;
      clip_box.y += composited->y;
      clip_box.width = MIN (clip_box.width, width - clip_box.x);
      clip_box.height = MIN (clip_box.height, height - clip_box.y);

      if (composited->composited)
	{
	  gdk_window_invalidate_rect (GDK_WINDOW (composited->parent),
				      &clip_box, FALSE);
	  break;
	}
    }
#endif /* USE_BACKING_STORE */
}

static void
gdk_window_free_paint_stack (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  
  if (private->paint_stack)
    {
      GSList *tmp_list = private->paint_stack;

      while (tmp_list)
	{
	  GdkWindowPaint *paint = tmp_list->data;

	  if (tmp_list == private->paint_stack)
	    g_object_unref (paint->pixmap);
		  
	  gdk_region_destroy (paint->region);
	  g_free (paint);

	  tmp_list = tmp_list->next;
	}

      g_slist_free (private->paint_stack);
      private->paint_stack = NULL;
    }
}

static void
do_move_region_bits_on_impl (GdkWindowObject *private,
			     GdkDrawable *dest,
			     int dest_off_x, int dest_off_y,
			     GdkRegion *region, /* In impl window coords */
			     int dx, int dy)
{
  GdkGC *tmp_gc;
  GdkRectangle copy_rect;
  
  gdk_region_get_clipbox (region, &copy_rect);
  gdk_region_offset (region, -dest_off_x, -dest_off_y);
  tmp_gc = _gdk_drawable_get_scratch_gc ((GdkWindow *)private, TRUE);
  gdk_gc_set_clip_region (tmp_gc, region);
  gdk_draw_drawable (dest,
		     tmp_gc,
		     private->impl,
		     copy_rect.x-dx, copy_rect.y-dy,
		     copy_rect.x - dest_off_x, copy_rect.y - dest_off_y,
		     copy_rect.width, copy_rect.height);
  gdk_gc_set_clip_region (tmp_gc, NULL);
}

static void
append_move_region (GdkWindowObject *impl_window,
		    GdkRegion *region,
		    int dx, int dy)
{
  GList *moves_to_add, *l, *s;
  GdkRegion *intersection;
  GdkWindowRegionMove *move, *new_move, *existing_move;
  
  move = g_slice_new (GdkWindowRegionMove);
  move->region  = region;
  move->dx = dx;
  move->dy = dy;

  moves_to_add = g_list_prepend (NULL, move);

  for (l = impl_window->outstanding_moves; l != NULL; l = l->next)
    {
      existing_move = l->data;
      
      for (s = moves_to_add; s != NULL; s = s->next)
	{
	  move = s->data;

	  intersection = gdk_region_copy (move->region);
	  gdk_region_offset (intersection, -move->dx, -move->dy);
	  gdk_region_intersect (intersection, existing_move->region);
	  gdk_region_offset (intersection, move->dx, move->dy);

	  if (!gdk_region_empty (intersection))
	    {
	      
	      new_move = g_slice_new (GdkWindowRegionMove);
	      new_move->region  = intersection;
	      new_move->dx = move->dx + existing_move->dx;
	      new_move->dy = move->dy + existing_move->dy;
	      moves_to_add = g_list_prepend (moves_to_add, new_move);

	      gdk_region_subtract (move->region, intersection);
	      gdk_region_subtract (existing_move->region, intersection);
	    }
	  else
	    gdk_region_destroy (intersection);
	}
    }

  impl_window->outstanding_moves = g_list_concat (impl_window->outstanding_moves,
						  moves_to_add);
  
}

/* Moves bits and update area by dx/dy in impl window
 * Takes ownership of region.
 */
static void
move_region_on_impl (GdkWindowObject *private,
		     GdkRegion *region, /* In impl window coords */
		     int dx, int dy)
{
  GdkWindowObject *impl_window;
  gboolean free_region;

  if (dx == 0 && dy == 0)
    {
      gdk_region_destroy (region);
      return;
    }
  
  free_region = TRUE;
  impl_window = gdk_window_get_impl_window (private);

  if (1) /* Enable flicker free handling of moves. */
    {
      free_region = FALSE;

      append_move_region (impl_window, region, dx, dy);
    }
  else
    do_move_region_bits_on_impl (private,
				 private->impl, 0,0,
				 region, dx, dy);
    
  /* Move any old invalid regions in the copy source area by dx/dy */
  if (impl_window->update_area)
    {
      GdkRegion *update_area;
      
      update_area = gdk_region_copy (region);
      /* Convert from target to source */
      gdk_region_offset (update_area, -dx, -dy);
      gdk_region_intersect (update_area, impl_window->update_area);
      gdk_region_subtract (impl_window->update_area, update_area);
      /* Convert back */
      gdk_region_offset (update_area, dx, dy);
      gdk_region_union (impl_window->update_area, update_area);
      gdk_region_destroy (update_area);
    }

  if (free_region)
    gdk_region_destroy (region);
}

/* Flushes all outstanding changes to the window, call this
 * before drawing directly to the window (i.e. outside a begin/end_paint pair).
 */
static void
gdk_window_flush (GdkWindow *window)
{
  GdkWindowObject *private;
  GdkWindowObject *impl_window;
  GList *l;
  GdkWindowRegionMove *move;

  private = (GdkWindowObject *) window;
  
  impl_window = gdk_window_get_impl_window (private);
  
  for (l = impl_window->outstanding_moves; l != NULL; l = l->next)
    {
      move = l->data;

      do_move_region_bits_on_impl (private,
				   private->impl, 0, 0,
				   move->region, move->dx, move->dy);

      gdk_region_destroy (move->region);
      g_slice_free (GdkWindowRegionMove, move);
    }
  
  g_list_free (impl_window->outstanding_moves);
  impl_window->outstanding_moves = NULL;
  
  gdk_window_flush_implicit_paint (window);
}

static void
gdk_window_get_offsets (GdkWindow *window,
			gint      *x_offset,
			gint      *y_offset)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      *x_offset = paint->x_offset;
      *y_offset = paint->y_offset;
    }
  else
    {
      *x_offset = -private->abs_x;
      *y_offset = -private->abs_y;
    }
}

/**
 * gdk_window_get_internal_paint_info:
 * @window: a #GdkWindow
 * @real_drawable: location to store the drawable to which drawing should be 
 *            done.
 * @x_offset: location to store the X offset between coordinates in @window,
 *            and the underlying window system primitive coordinates for 
 *            *@real_drawable.
 * @y_offset: location to store the Y offset between coordinates in @window,
 *            and the underlying window system primitive coordinates for
 *            *@real_drawable.
 * 
 * If you bypass the GDK layer and use windowing system primitives to
 * draw directly onto a #GdkWindow, then you need to deal with two
 * details: there may be an offset between GDK coordinates and windowing
 * system coordinates, and GDK may have redirected drawing to a offscreen
 * pixmap as the result of a gdk_window_begin_paint_region() calls.
 * This function allows retrieving the information you need to compensate
 * for these effects.
 *
 * This function exposes details of the GDK implementation, and is thus
 * likely to change in future releases of GDK.
 **/
void
gdk_window_get_internal_paint_info (GdkWindow    *window,
				    GdkDrawable **real_drawable,
				    gint         *x_offset,
				    gint         *y_offset)
{
  gint x_off, y_off;
  
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *)window;

  if (real_drawable)
    {
      if (private->paint_stack)
	{
	  GdkWindowPaint *paint = private->paint_stack->data;
	  *real_drawable = paint->pixmap;
	}
      else
	*real_drawable = window;
    }

  gdk_window_get_offsets (window, &x_off, &y_off);

  if (x_offset)
    *x_offset = x_off;
  if (y_offset)
    *y_offset = y_off;
}

static void
setup_clip_for_draw (GdkDrawable *drawable,
		     GdkGC *gc,
		     int old_clip_x, int old_clip_y)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  GdkRegion *clip;

  if (private->window_type == GDK_WINDOW_ROOT)
    return;
  
  if (_gdk_gc_get_subwindow (gc) == GDK_CLIP_BY_CHILDREN)
    clip = private->clip_region_with_children;
  else
    clip = private->clip_region;
    
  _gdk_gc_add_drawable_clip (gc,
			     private->clip_tag,
			     clip,
			     /* If there was a clip origin set appart from the
			      * window offset, need to take that into consideration */
			     -old_clip_x, -old_clip_y);
}

static void
setup_clip_for_paint (GdkDrawable *drawable,
		      GdkWindowPaint *paint,
		      GdkGC *gc,
		      int old_clip_x, int old_clip_y)
{
  _gdk_gc_add_drawable_clip (gc,
			     paint->region_tag,
			     /* This includes the window clip */
			     paint->region,
			     /* If there was a clip origin set appart from the
			      * window offset, need to take that into consideration */
			     -old_clip_x, -old_clip_y);
}


#define OFFSET_GC(gc)                                         \
    gint x_offset, y_offset; 				      \
    gint old_clip_x = gc->clip_x_origin;    \
    gint old_clip_y = gc->clip_y_origin;    \
    gint old_ts_x = gc->ts_x_origin;        \
    gint old_ts_y = gc->ts_y_origin;        \
    gdk_window_get_offsets (drawable, &x_offset, &y_offset);  \
    if (x_offset != 0 || y_offset != 0)             	      \
      {                                                       \
        gdk_gc_set_clip_origin (gc, old_clip_x - x_offset,    \
	  	                old_clip_y - y_offset);       \
        gdk_gc_set_ts_origin (gc, old_ts_x - x_offset,        \
	  	              old_ts_y - y_offset);           \
      }

#define RESTORE_GC(gc)                                      \
    if (x_offset != 0 || y_offset != 0)       		    \
     {                                                      \
       gdk_gc_set_clip_origin (gc, old_clip_x, old_clip_y); \
       gdk_gc_set_ts_origin (gc, old_ts_x, old_ts_y);       \
     }

#define SETUP_PAINT_GC_CLIP(gc)                             \
      if (paint->uses_implicit)        			    \
	setup_clip_for_paint (drawable, paint, gc, old_clip_x,	\
                              old_clip_y);

#define RESTORE_PAINT_GC_CLIP(gc) 


#define SETUP_DIRECT_GC_CLIP(gc)                            \
      gdk_window_flush ((GdkWindow *)drawable);\
      setup_clip_for_draw (drawable, gc, old_clip_x, old_clip_y);

#define RESTORE_DIRECT_GC_CLIP(gc)

static GdkGC *
gdk_window_create_gc (GdkDrawable     *drawable,
                      GdkGCValues     *values,
                      GdkGCValuesMask  mask)
{
  g_return_val_if_fail (GDK_IS_WINDOW (drawable), NULL);
  
  if (GDK_WINDOW_DESTROYED (drawable))
    return NULL;

  return gdk_gc_new_with_values (((GdkWindowObject *) drawable)->impl,
                                 values, mask);
}

/* After having set up the drawable clip rect on a GC we need
 * to make sure that if we draw to a pixmap we draw to the impl,
 * otherwise the pixmap code will reset the drawable clip.
 */
static GdkDrawable *
pixmap_impl (GdkPixmap *pixmap)
{
  return ((GdkPixmapObject *)pixmap)->impl;
}

static void
gdk_window_draw_rectangle (GdkDrawable *drawable,
			   GdkGC       *gc,
			   gboolean     filled,
			   gint         x,
			   gint         y,
			   gint         width,
			   gint         height)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      SETUP_PAINT_GC_CLIP (gc);
      gdk_draw_rectangle (pixmap_impl (paint->pixmap), gc, filled,
                          x - x_offset, y - y_offset, width, height);
      RESTORE_PAINT_GC_CLIP (gc);
    }
  else
    {
      SETUP_DIRECT_GC_CLIP(gc);
      gdk_draw_rectangle (private->impl, gc, filled,
			  x - x_offset, y - y_offset, width, height);
      RESTORE_DIRECT_GC_CLIP(gc);
    }

  RESTORE_GC (gc);
}

static void
gdk_window_draw_arc (GdkDrawable *drawable,
		     GdkGC       *gc,
		     gboolean     filled,
		     gint         x,
		     gint         y,
		     gint         width,
		     gint         height,
		     gint         angle1,
		     gint         angle2)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      SETUP_PAINT_GC_CLIP (gc);
      gdk_draw_arc (pixmap_impl (paint->pixmap), gc, filled,
		    x - x_offset, y - y_offset,
		    width, height, angle1, angle2);
      RESTORE_PAINT_GC_CLIP (gc);
    }
  else
    {
      SETUP_DIRECT_GC_CLIP(gc);
      gdk_draw_arc (private->impl, gc, filled,
		    x - x_offset, y - y_offset,
		    width, height, angle1, angle2);
      RESTORE_DIRECT_GC_CLIP(gc);
    }
  RESTORE_GC (gc);
}

static void
gdk_window_draw_polygon (GdkDrawable *drawable,
			 GdkGC       *gc,
			 gboolean     filled,
			 GdkPoint    *points,
			 gint         npoints)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  GdkPoint *new_points;
  
  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (x_offset != 0 || y_offset != 0)
    {
      int i;
      
      new_points = g_new (GdkPoint, npoints);
      for (i=0; i<npoints; i++)
	{
	  new_points[i].x = points[i].x - x_offset;
	  new_points[i].y = points[i].y - y_offset;
	}
    }
  else
    new_points = points;

  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      SETUP_PAINT_GC_CLIP (gc);
      gdk_draw_polygon (pixmap_impl (paint->pixmap), gc, filled, new_points, npoints);
      RESTORE_PAINT_GC_CLIP (gc);
    }
  else
    {
      SETUP_DIRECT_GC_CLIP(gc);
      gdk_draw_polygon (private->impl, gc, filled, new_points, npoints);
      RESTORE_DIRECT_GC_CLIP(gc);
    }
  
  if (new_points != points)
    g_free (new_points);

  RESTORE_GC (gc);
}

static void
gdk_window_draw_text (GdkDrawable *drawable,
		      GdkFont     *font,
		      GdkGC       *gc,
		      gint         x,
		      gint         y,
		      const gchar *text,
		      gint         text_length)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      SETUP_PAINT_GC_CLIP (gc);
      gdk_draw_text (pixmap_impl (paint->pixmap), font, gc, 
		     x - x_offset, y - y_offset, text, text_length);

      RESTORE_PAINT_GC_CLIP (gc);
    }
  else
    {
      SETUP_DIRECT_GC_CLIP(gc);
      gdk_draw_text (private->impl, font, gc,
		     x - x_offset, y - y_offset, text, text_length);
      RESTORE_DIRECT_GC_CLIP(gc);
    }

  RESTORE_GC (gc);
}

static void
gdk_window_draw_text_wc (GdkDrawable    *drawable,
			 GdkFont        *font,
			 GdkGC          *gc,
			 gint            x,
			 gint            y,
			 const GdkWChar *text,
			 gint            text_length)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      SETUP_PAINT_GC_CLIP (gc);
      gdk_draw_text_wc (pixmap_impl (paint->pixmap), font, gc, 
			x - x_offset, y - y_offset, text, text_length);
      RESTORE_PAINT_GC_CLIP (gc);
    }
  else
    {
      SETUP_DIRECT_GC_CLIP(gc);
      gdk_draw_text_wc (private->impl, font, gc,
			x - x_offset, y - y_offset, text, text_length);
      RESTORE_DIRECT_GC_CLIP(gc);
    }
  
  RESTORE_GC (gc);
}

static GdkDrawable *
gdk_window_get_source_drawable (GdkDrawable *drawable)
{
  GdkWindow *window = GDK_WINDOW (drawable);
  GdkWindowObject *private;

  private = (GdkWindowObject *) window;
  if (GDK_DRAWABLE_GET_CLASS (private->impl)->get_source_drawable)
    return GDK_DRAWABLE_GET_CLASS (private->impl)->get_source_drawable (private->impl);
  
  return drawable;
}

static GdkDrawable *
gdk_window_get_composite_drawable (GdkDrawable *drawable,
                                   gint         x,
                                   gint         y,
                                   gint         width,
                                   gint         height,
                                   gint        *composite_x_offset,
                                   gint        *composite_y_offset)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  GSList *list;
  GdkPixmap *tmp_pixmap;
  GdkRectangle rect;
  GdkGC *tmp_gc;
  gboolean overlap_buffer;
  GdkDrawable *source;
  GdkWindowObject *impl_window;
  GdkWindowPaint *implicit_paint;

  *composite_x_offset = -private->abs_x;
  *composite_y_offset = -private->abs_y;

  if ((GDK_IS_WINDOW (drawable) && GDK_WINDOW_DESTROYED (drawable)))
    return g_object_ref (_gdk_drawable_get_source_drawable (drawable));

  /* See if any buffered part is overlapping the part we want
   * to get
   */
  rect.x = x;
  rect.y = y;
  rect.width = width;
  rect.height = height;

  overlap_buffer = FALSE;

  for (list = private->paint_stack; list != NULL; list = list->next)
    {
      GdkWindowPaint *paint = list->data;
      GdkOverlapType overlap;
      
      overlap = gdk_region_rect_in (paint->region, &rect);
      
      if (overlap == GDK_OVERLAP_RECTANGLE_IN)
	{
	  *composite_x_offset = paint->x_offset;
	  *composite_y_offset = paint->y_offset;
	  
	  return g_object_ref (paint->pixmap);
	}
      else if (overlap == GDK_OVERLAP_RECTANGLE_PART)
	{
	  overlap_buffer = TRUE;
	  break;
	}
    }

  impl_window = gdk_window_get_impl_window (private);
  implicit_paint = impl_window->implicit_paint;
  if (implicit_paint)
    {
      GdkOverlapType overlap;

      rect.x += private->abs_x;
      rect.y += private->abs_y;
      
      overlap = gdk_region_rect_in (implicit_paint->region, &rect);
      if (overlap == GDK_OVERLAP_RECTANGLE_IN)
	{
	  *composite_x_offset = -private->abs_x + implicit_paint->x_offset;
	  *composite_y_offset = -private->abs_y + implicit_paint->y_offset;
	  
	  return g_object_ref (implicit_paint->pixmap);
	}
      else if (overlap == GDK_OVERLAP_RECTANGLE_PART)
	overlap_buffer = TRUE;
    }

  if (!overlap_buffer)
    return g_object_ref (_gdk_drawable_get_source_drawable (drawable));
  
  tmp_pixmap = gdk_pixmap_new (drawable, width, height, -1);
  tmp_gc = _gdk_drawable_get_scratch_gc (tmp_pixmap, FALSE);

  source = _gdk_drawable_get_source_drawable (drawable);
  
  /* Copy the current window contents */
  gdk_draw_drawable (tmp_pixmap,
                     tmp_gc,
                     GDK_WINDOW_OBJECT (source)->impl,
                     x - *composite_x_offset,
                     y - *composite_y_offset,
                     0, 0,
                     width, height);

  /* paint the backing stores */
  if (implicit_paint)
    {
      GdkWindowPaint *paint = list->data;
      
      gdk_gc_set_clip_region (tmp_gc, paint->region);
      gdk_gc_set_clip_origin (tmp_gc, -x  - paint->x_offset, -y  - paint->y_offset);
      
      gdk_draw_drawable (tmp_pixmap, tmp_gc, paint->pixmap,
			 x - paint->x_offset,
			 y - paint->y_offset,
			 0, 0, width, height);
    }
  
  for (list = private->paint_stack; list != NULL; list = list->next)
    {
      GdkWindowPaint *paint = list->data;

      if (paint->uses_implicit)
	continue; /* We already copied this above */
      
      gdk_gc_set_clip_region (tmp_gc, paint->region);
      gdk_gc_set_clip_origin (tmp_gc, -x, -y);
      
      gdk_draw_drawable (tmp_pixmap, tmp_gc, paint->pixmap,
			 x - paint->x_offset,
			 y - paint->y_offset,
			 0, 0, width, height);
    }
  
  /* Reset clip region of the cached GdkGC */
  gdk_gc_set_clip_region (tmp_gc, NULL);

  /* Set these to location of tmp_pixmap within the window */
  *composite_x_offset = x;
  *composite_y_offset = y;

  return tmp_pixmap;
}

static GdkRegion*
gdk_window_get_clip_region (GdkDrawable *drawable)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  GdkRegion *result;

  result = gdk_region_copy (private->clip_region);

  if (private->paint_stack)
    {
      GdkRegion *paint_region = gdk_region_new ();
      GSList *tmp_list = private->paint_stack;

      while (tmp_list)
	{
	  GdkWindowPaint *paint = tmp_list->data;
	  
	  gdk_region_union (paint_region, paint->region);

          tmp_list = tmp_list->next;
	}

      gdk_region_intersect (result, paint_region);
      gdk_region_destroy (paint_region);
    }

  return result;
}

static GdkRegion*
gdk_window_get_visible_region (GdkDrawable *drawable)
{
  GdkWindowObject *private = (GdkWindowObject*) drawable;

  return gdk_region_copy (private->clip_region);
}

static void
gdk_window_draw_drawable (GdkDrawable *drawable,
			  GdkGC       *gc,
			  GdkPixmap   *src,
			  gint         xsrc,
			  gint         ysrc,
			  gint         xdest,
			  gint         ydest,
			  gint         width,
			  gint         height)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  OFFSET_GC (gc);
  
  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  /* If we have a backing pixmap draw to that */
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      SETUP_PAINT_GC_CLIP (gc);
      gdk_draw_drawable (pixmap_impl (paint->pixmap), gc,
                         src, xsrc, ysrc,
			 xdest - x_offset, ydest - y_offset, width, height);

      RESTORE_PAINT_GC_CLIP (gc);
    }
  else
    {
      SETUP_DIRECT_GC_CLIP(gc);
      gdk_draw_drawable (private->impl, gc,
			 src, xsrc, ysrc,
			 xdest - x_offset, ydest - y_offset,
			 width, height);
      RESTORE_DIRECT_GC_CLIP(gc);
    }

  RESTORE_GC (gc);
}

static void
gdk_window_draw_points (GdkDrawable *drawable,
			GdkGC       *gc,
			GdkPoint    *points,
			gint         npoints)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  GdkPoint *new_points;
  
  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (x_offset != 0 || y_offset != 0)
    {
      gint i;

      new_points = g_new (GdkPoint, npoints);
      for (i=0; i<npoints; i++)
	{
	  new_points[i].x = points[i].x - x_offset;
	  new_points[i].y = points[i].y - y_offset;
	}
    }
  else
    new_points = points;

  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      SETUP_PAINT_GC_CLIP (gc);
      gdk_draw_points (pixmap_impl (paint->pixmap), gc, new_points, npoints);
      RESTORE_PAINT_GC_CLIP (gc);
    }
  else
    {
      SETUP_DIRECT_GC_CLIP(gc);
      gdk_draw_points (private->impl, gc, new_points, npoints);
      RESTORE_DIRECT_GC_CLIP(gc);
    }

  if (new_points != points)
    g_free (new_points);

  RESTORE_GC (gc);
}

static void
gdk_window_draw_segments (GdkDrawable *drawable,
			  GdkGC       *gc,
			  GdkSegment  *segs,
			  gint         nsegs)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  GdkSegment *new_segs;

  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (x_offset != 0 || y_offset != 0)
    {
      gint i;

      new_segs = g_new (GdkSegment, nsegs);
      for (i=0; i<nsegs; i++)
	{
	  new_segs[i].x1 = segs[i].x1 - x_offset;
	  new_segs[i].y1 = segs[i].y1 - y_offset;
	  new_segs[i].x2 = segs[i].x2 - x_offset;
	  new_segs[i].y2 = segs[i].y2 - y_offset;
	}
    }
  else
    new_segs = segs;

  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      SETUP_PAINT_GC_CLIP (gc);
      gdk_draw_segments (pixmap_impl (paint->pixmap), gc, new_segs, nsegs);
      RESTORE_PAINT_GC_CLIP (gc);
    }
  else
    {
      SETUP_DIRECT_GC_CLIP(gc);
      gdk_draw_segments (private->impl, gc, new_segs, nsegs);
      RESTORE_DIRECT_GC_CLIP(gc);
    }
  
  if (new_segs != segs)
    g_free (new_segs);

  RESTORE_GC (gc);
}

static void
gdk_window_draw_lines (GdkDrawable *drawable,
		       GdkGC       *gc,
		       GdkPoint    *points,
		       gint         npoints)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  GdkPoint *new_points;

  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (x_offset != 0 || y_offset != 0)
    {
      gint i;

      new_points = g_new (GdkPoint, npoints);
      for (i=0; i<npoints; i++)
	{
	  new_points[i].x = points[i].x - x_offset;
	  new_points[i].y = points[i].y - y_offset;
	}
    }
  else
    new_points = points;

  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      SETUP_PAINT_GC_CLIP (gc);
      gdk_draw_lines (pixmap_impl (paint->pixmap), gc, new_points, npoints);
      RESTORE_PAINT_GC_CLIP (gc);
    }
  else
    {
      SETUP_DIRECT_GC_CLIP(gc);
      gdk_draw_lines (private->impl, gc, new_points, npoints);
      RESTORE_DIRECT_GC_CLIP(gc);
    }

  if (new_points != points)
    g_free (new_points);

  RESTORE_GC (gc);
}

static void
gdk_window_draw_glyphs (GdkDrawable      *drawable,
			GdkGC            *gc,
			PangoFont        *font,
			gint              x,
			gint              y,
			PangoGlyphString *glyphs)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;

  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;

      SETUP_PAINT_GC_CLIP (gc);
      gdk_draw_glyphs (pixmap_impl (paint->pixmap), gc, font, x - x_offset, y - y_offset, glyphs);
      RESTORE_PAINT_GC_CLIP (gc);
    }
  else
    {
      SETUP_DIRECT_GC_CLIP(gc);
      gdk_draw_glyphs (private->impl, gc, font,
		       x - x_offset, y - y_offset, glyphs);
      RESTORE_DIRECT_GC_CLIP(gc);
    }

  RESTORE_GC (gc);
}

static void
gdk_window_draw_glyphs_transformed (GdkDrawable      *drawable,
				    GdkGC            *gc,
				    PangoMatrix      *matrix,
				    PangoFont        *font,
				    gint              x,
				    gint              y,
				    PangoGlyphString *glyphs)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  PangoMatrix tmp_matrix;

  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  if (x_offset != 0 || y_offset != 0)
    {
      if (matrix)
	{
	  tmp_matrix = *matrix;
	  tmp_matrix.x0 -= x_offset;
	  tmp_matrix.y0 -= y_offset;
	  matrix = &tmp_matrix;
	}
      else if (GDK_PANGO_UNITS_OVERFLOWS (x_offset, y_offset))
	{
	  PangoMatrix identity = PANGO_MATRIX_INIT;
	  
	  tmp_matrix = identity;
	  tmp_matrix.x0 -= x_offset;
	  tmp_matrix.y0 -= y_offset;
	  matrix = &tmp_matrix;
	}
      else
	{
	  x -= x_offset * PANGO_SCALE;
	  y -= y_offset * PANGO_SCALE;
	}
    }
  
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;

      SETUP_PAINT_GC_CLIP (gc);
      gdk_draw_glyphs_transformed (pixmap_impl (paint->pixmap), gc, matrix, font, x, y, glyphs);
      RESTORE_PAINT_GC_CLIP (gc);
    }
  else
    {
      SETUP_DIRECT_GC_CLIP(gc);
      gdk_draw_glyphs_transformed (private->impl, gc, matrix, font, x, y, glyphs);
      RESTORE_DIRECT_GC_CLIP(gc);
    }

  RESTORE_GC (gc);
}

typedef struct {
  cairo_t *cr; /* if non-null, it means use this cairo context */
  GdkGC *gc;   /* if non-null, it means use this GC instead */
} BackingRectMethod;

static void
setup_backing_rect_method (BackingRectMethod *method, GdkWindow *window, GdkWindowPaint *paint, int x_offset_cairo, int y_offset_cairo)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  if (private->bg_pixmap == GDK_PARENT_RELATIVE_BG && private->parent)
    {
      GdkWindowPaint tmp_paint;

      tmp_paint = *paint;
      tmp_paint.x_offset += private->x;
      tmp_paint.y_offset += private->y;

      x_offset_cairo += private->x;
      y_offset_cairo += private->y;

      setup_backing_rect_method (method, GDK_WINDOW (private->parent), &tmp_paint, x_offset_cairo, y_offset_cairo);
    }
  else if (private->bg_pixmap &&
	   private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
	   private->bg_pixmap != GDK_NO_BG)
    {
/* This is a workaround for https://bugs.freedesktop.org/show_bug.cgi?id=4320.
 * In it, using a pixmap as a repeating pattern in Cairo, and painting it to a
 * pixmap destination surface, can be very slow (on the order of seconds for a
 * whole-screen copy).  The workaround is to use pretty much the same code that
 * we used in GTK+ 2.6 (pre-Cairo), which clears the double-buffer pixmap with
 * a tiled GC XFillRectangle().
 */

/* Actually computing this flag is left as an exercise for the reader */
#if defined (G_OS_UNIX)
#  define GDK_CAIRO_REPEAT_IS_FAST 0
#else
#  define GDK_CAIRO_REPEAT_IS_FAST 1
#endif

#if GDK_CAIRO_REPEAT_IS_FAST
      cairo_surface_t *surface = _gdk_drawable_ref_cairo_surface (private->bg_pixmap);
      cairo_pattern_t *pattern = cairo_pattern_create_for_surface (surface);
      cairo_surface_destroy (surface);

      if (x_offset_cairo != 0 || y_offset_cairo != 0)
	{
	  cairo_matrix_t matrix;
	  cairo_matrix_init_translate (&matrix, x_offset_cairo, y_offset_cairo);
	  cairo_pattern_set_matrix (pattern, &matrix);
	}

      cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);

      method->cr = cairo_create (paint->surface);
      method->gc = NULL;

      cairo_set_source (method->cr, pattern);
      cairo_pattern_destroy (pattern);
#else
      guint gc_mask;
      GdkGCValues gc_values;

      gc_values.fill = GDK_TILED;
      gc_values.tile = private->bg_pixmap;
      gc_values.ts_x_origin = -x_offset_cairo;
      gc_values.ts_y_origin = -y_offset_cairo;

      gc_mask = GDK_GC_FILL | GDK_GC_TILE | GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN;

      method->gc = gdk_gc_new_with_values (paint->pixmap, &gc_values, gc_mask);
#endif
    }
  else
    {
      method->cr = cairo_create (paint->surface);

      gdk_cairo_set_source_color (method->cr, &private->bg_color);
    }
}

static void
gdk_window_clear_backing_rect (GdkWindow *window,
			       gint       x,
			       gint       y,
			       gint       width,
			       gint       height)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowPaint *paint = private->paint_stack->data;
  BackingRectMethod method;
#if 0
  GTimer *timer;
  double elapsed;
#endif

  if (GDK_WINDOW_DESTROYED (window))
    return;

#if 0
  timer = g_timer_new ();
#endif

  method.cr = NULL;
  method.gc = NULL;
  setup_backing_rect_method (&method, window, paint, 0, 0);

  if (method.cr)
    {
      g_assert (method.gc == NULL);

      cairo_rectangle (method.cr, x, y, width, height);
      cairo_clip (method.cr);

      gdk_cairo_region (method.cr, paint->region);
      cairo_fill (method.cr);

      cairo_destroy (method.cr);
#if 0
      elapsed = g_timer_elapsed (timer, NULL);
      g_print ("Draw the background with Cairo: %fs\n", elapsed);
#endif
    }
  else
    {
      g_assert (method.gc != NULL);

      gdk_gc_set_clip_region (method.gc, paint->region);
      gdk_draw_rectangle (window, method.gc, TRUE, x, y, width, height);
      g_object_unref (method.gc);

#if 0
      elapsed = g_timer_elapsed (timer, NULL);
      g_print ("Draw the background with GDK: %fs\n", elapsed);
#endif
    }

#if 0
  g_timer_destroy (timer);
#endif
}

static void
gdk_window_clear_backing_rect_redirect (GdkWindow *window,
					gint       x,
					gint       y,
					gint       width,
					gint       height)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowRedirect *redirect = private->redirect;
  GdkRegion *clip_region;
  gint x_offset, y_offset;
  BackingRectMethod method;
  GdkWindowPaint paint;
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  clip_region = _gdk_window_calculate_full_clip_region (window,
							GDK_WINDOW (redirect->redirected),
							TRUE,
							&x_offset, &y_offset);
  
  paint.x_offset = x_offset;
  paint.y_offset = y_offset;
  paint.pixmap = redirect->pixmap;
  paint.surface = _gdk_drawable_ref_cairo_surface (redirect->pixmap);
  
  method.cr = NULL;
  method.gc = NULL;
  setup_backing_rect_method (&method, window, &paint, 0, 0);

  if (method.cr)
    {
      g_assert (method.gc == NULL);

      cairo_rectangle (method.cr, x, y, width, height);
      cairo_clip (method.cr);

      gdk_cairo_region (method.cr, clip_region);
      cairo_fill (method.cr);

      cairo_destroy (method.cr);
    }
  else
    {
      g_assert (method.gc != NULL);

      gdk_gc_set_clip_region (method.gc, clip_region);
      gdk_draw_rectangle (window, method.gc, TRUE, x, y, width, height);
      g_object_unref (method.gc);

    }

  gdk_region_destroy (clip_region);
  cairo_surface_destroy (paint.surface);
}

static void
gdk_window_clear_backing_rect_direct (GdkWindow *window,
				      gint       x,
				      gint       y,
				      gint       width,
				      gint       height)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  BackingRectMethod method;
  GdkWindowPaint paint;
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  paint.x_offset = 0;
  paint.y_offset = 0;
  paint.pixmap = window;
  paint.surface = _gdk_drawable_ref_cairo_surface (window);
  
  method.cr = NULL;
  method.gc = NULL;
  setup_backing_rect_method (&method, window, &paint, 0, 0);

  if (method.cr)
    {
      g_assert (method.gc == NULL);

      gdk_cairo_region (method.cr, private->clip_region_with_children);
      cairo_clip (method.cr);
      
      cairo_rectangle (method.cr, x, y, width, height);
      cairo_fill (method.cr);

      cairo_destroy (method.cr);
    }
  else
    {
      g_assert (method.gc != NULL);

      gdk_gc_set_clip_region (method.gc, private->clip_region_with_children);
      gdk_draw_rectangle (window, method.gc, TRUE, x, y, width, height);
      g_object_unref (method.gc);

    }

  cairo_surface_destroy (paint.surface);
}


/**
 * gdk_window_clear:
 * @window: a #GdkWindow
 * 
 * Clears an entire @window to the background color or background pixmap.
 **/
void
gdk_window_clear (GdkWindow *window)
{
  gint width, height;

  g_return_if_fail (GDK_IS_WINDOW (window));

  gdk_drawable_get_size (GDK_DRAWABLE (window), &width, &height);
  
  gdk_window_clear_area (window, 0, 0,
                         width, height);
}

static void
gdk_window_clear_area_internal (GdkWindow *window,
				gint       x,
				gint       y,
				gint       width,
				gint       height,
				gboolean   send_expose)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  /* This is what XClearArea does, and e.g. GtkCList uses it,
     so we need to duplicate that */
  if (width == 0)
    width = private->width - x;
  if (height == 0)
    height = private->height - y;
  
  if (private->paint_stack)
    gdk_window_clear_backing_rect (window, x, y, width, height);
  else
    {
      if (private->redirect)
	gdk_window_clear_backing_rect_redirect (window, x, y, width, height);
      
      gdk_window_clear_backing_rect_direct (window, x, y, width, height);
      if (send_expose)
	{
	  GdkRectangle rect;

	  rect.x = x;
	  rect.y = x;
	  rect.width = width;
	  rect.height = height;
	  
	  gdk_window_invalidate_rect (window, &rect, FALSE);
	}
    }
}


/**
 * gdk_window_clear_area:
 * @window: a #GdkWindow
 * @x: x coordinate of rectangle to clear
 * @y: y coordinate of rectangle to clear
 * @width: width of rectangle to clear
 * @height: height of rectangle to clear
 *
 * Clears an area of @window to the background color or background pixmap.
 * 
 **/
void
gdk_window_clear_area (GdkWindow *window,
		       gint       x,
		       gint       y,
		       gint       width,
		       gint       height)
{
  gdk_window_clear_area_internal (window,
				  x, y,
				  width, height,
				  FALSE);
}

/**
 * gdk_window_clear_area_e:
 * @window: a #GdkWindow
 * @x: x coordinate of rectangle to clear
 * @y: y coordinate of rectangle to clear
 * @width: width of rectangle to clear
 * @height: height of rectangle to clear
 *
 * Like gdk_window_clear_area(), but also generates an expose event for
 * the cleared area.
 *
 * This function has a stupid name because it dates back to the mists
 * time, pre-GDK-1.0.
 * 
 **/
void
gdk_window_clear_area_e (GdkWindow *window,
		         gint       x,
		         gint       y,
		         gint       width,
		         gint       height)
{
  gdk_window_clear_area_internal (window,
				  x, y,
				  width, height,
				  TRUE);
}

static void
gdk_window_draw_image (GdkDrawable *drawable,
                       GdkGC       *gc,
                       GdkImage    *image,
                       gint         xsrc,
                       gint         ysrc,
                       gint         xdest,
                       gint         ydest,
                       gint         width,
                       gint         height)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;

  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      SETUP_PAINT_GC_CLIP (gc);
      gdk_draw_image (pixmap_impl (paint->pixmap), gc, image, xsrc, ysrc,
                      xdest - x_offset, ydest - y_offset,
                      width, height);

      RESTORE_PAINT_GC_CLIP (gc);
    }
  else
    {
      SETUP_DIRECT_GC_CLIP(gc);
      gdk_draw_image (private->impl, gc, image, xsrc, ysrc,
		      xdest - x_offset, ydest - y_offset,
		      width, height);
      RESTORE_DIRECT_GC_CLIP(gc);
    }

  RESTORE_GC (gc);
}

static void
gdk_window_draw_pixbuf (GdkDrawable     *drawable,
			GdkGC           *gc,
			GdkPixbuf       *pixbuf,
			gint             src_x,
			gint             src_y,
			gint             dest_x,
			gint             dest_y,
			gint             width,
			gint             height,
			GdkRgbDither     dither,
			gint             x_dither,
			gint             y_dither)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (gc)
    {
      OFFSET_GC (gc);
  
      if (private->paint_stack)
	{
	  GdkWindowPaint *paint = private->paint_stack->data;
	  SETUP_PAINT_GC_CLIP (gc);
	  gdk_draw_pixbuf (pixmap_impl (paint->pixmap), gc, pixbuf, src_x, src_y,
			   dest_x - x_offset, dest_y - y_offset,
			   width, height,
			   dither, x_dither - x_offset, y_dither - y_offset);
	  RESTORE_PAINT_GC_CLIP (gc);
	}
      else
	{
	  SETUP_DIRECT_GC_CLIP(gc);
	  gdk_draw_pixbuf (private->impl, gc, pixbuf, src_x, src_y,
			   dest_x - x_offset, dest_y - y_offset,
			   width, height,
			   dither, x_dither, y_dither);
	  RESTORE_DIRECT_GC_CLIP(gc);
	}
      
      RESTORE_GC (gc);
    }
  else
    {
      gint x_offset, y_offset;
      gdk_window_get_offsets (drawable, &x_offset, &y_offset);
      
      if (private->paint_stack)
	{
	  GdkWindowPaint *paint = private->paint_stack->data;
	  /* TODO: Do paint clipping here... */
	  gdk_draw_pixbuf (paint->pixmap, gc, pixbuf, src_x, src_y,
			   dest_x - x_offset, dest_y - y_offset,
			   width, height,
			    dither, x_dither - x_offset, y_dither - y_offset);
	}
      else
	{
	  /* TODO: No GC passed in, but still want clipping here... */
	  gdk_draw_pixbuf (private->impl, gc, pixbuf, src_x, src_y,
			   dest_x - x_offset, dest_y - y_offset,
			   width, height,
			   dither, x_dither, y_dither);
	}
    }
}

static void
gdk_window_draw_trapezoids (GdkDrawable   *drawable,
			    GdkGC	  *gc,
			    GdkTrapezoid  *trapezoids,
			    gint           n_trapezoids)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  GdkTrapezoid *new_trapezoids = NULL;

  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (x_offset != 0 || y_offset != 0)
    {
      gint i;

      new_trapezoids = g_new (GdkTrapezoid, n_trapezoids);
      for (i=0; i < n_trapezoids; i++)
	{
	  new_trapezoids[i].y1 = trapezoids[i].y1 - y_offset;
	  new_trapezoids[i].x11 = trapezoids[i].x11 - x_offset;
	  new_trapezoids[i].x21 = trapezoids[i].x21 - x_offset;
	  new_trapezoids[i].y2 = trapezoids[i].y2 - y_offset;
	  new_trapezoids[i].x12 = trapezoids[i].x12 - x_offset;
	  new_trapezoids[i].x22 = trapezoids[i].x22 - x_offset;
	}

      trapezoids = new_trapezoids;
    }

  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      SETUP_PAINT_GC_CLIP (gc);
      gdk_draw_trapezoids (pixmap_impl (paint->pixmap), gc, trapezoids, n_trapezoids);
      RESTORE_PAINT_GC_CLIP (gc);
    }
  else
    {
      SETUP_DIRECT_GC_CLIP(gc);
      gdk_draw_trapezoids (private->impl, gc, trapezoids, n_trapezoids);
      RESTORE_DIRECT_GC_CLIP(gc);
    }
  
  g_free (new_trapezoids);

  RESTORE_GC (gc);
}

static void
gdk_window_real_get_size (GdkDrawable *drawable,
                          gint *width,
                          gint *height)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;

  if (width)
    *width = private->width;
  if (height)
    *height = private->height;
}

static GdkVisual*
gdk_window_real_get_visual (GdkDrawable *drawable)
{
  GdkColormap *colormap;

  g_return_val_if_fail (GDK_IS_WINDOW (drawable), NULL);

  colormap = gdk_drawable_get_colormap (drawable);
  return colormap ? gdk_colormap_get_visual (colormap) : NULL;
}

static gint
gdk_window_real_get_depth (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_WINDOW (drawable), 0);

  return ((GdkWindowObject *)GDK_WINDOW (drawable))->depth;
}

static GdkScreen*
gdk_window_real_get_screen (GdkDrawable *drawable)
{
  return gdk_drawable_get_screen (GDK_WINDOW_OBJECT (drawable)->impl);
}

static void
gdk_window_real_set_colormap (GdkDrawable *drawable,
                              GdkColormap *cmap)
{
  GdkWindowObject *private;
  
  g_return_if_fail (GDK_IS_WINDOW (drawable));  

  if (GDK_WINDOW_DESTROYED (drawable))
    return;

  private = (GdkWindowObject *)drawable;

  /* different colormap than parent, requires native window */
  if (!private->input_only &&
      cmap != gdk_drawable_get_colormap ((GdkDrawable *)(private->parent)))
    gdk_window_set_has_native ((GdkWindow *)drawable, TRUE);
  
  gdk_drawable_set_colormap (private->impl, cmap);
}

static GdkColormap*
gdk_window_real_get_colormap (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_WINDOW (drawable), NULL);

  if (GDK_WINDOW_DESTROYED (drawable))
    return NULL;
  
  return gdk_drawable_get_colormap (((GdkWindowObject*)drawable)->impl);
}
                      
static GdkImage*
gdk_window_copy_to_image (GdkDrawable     *drawable,
			  GdkImage        *image,
			  gint             src_x,
			  gint             src_y,
			  gint             dest_x,
			  gint             dest_y,
			  gint             width,
			  gint             height)
{
  GdkWindowObject *private = (GdkWindowObject *) drawable;
  gint x_offset, y_offset;
  
  g_return_val_if_fail (GDK_IS_WINDOW (drawable), NULL);
  
  if (GDK_WINDOW_DESTROYED (drawable))
    return NULL;

  /* If we're here, a composite image was not necessary, so
   * we can ignore the paint stack.
   */

  /* TODO: Is this right? */
  x_offset = 0;
  y_offset = 0; 
  
  return gdk_drawable_copy_to_image (private->impl,
				     image,
				     src_x - x_offset,
				     src_y - y_offset,
				     dest_x, dest_y,
				     width, height);
}

static void
gdk_window_cairo_surface_destroy (void *data)
{
  GdkWindowObject *private = (GdkWindowObject*) data;

  private->cairo_surface = NULL;
}

static cairo_surface_t *
gdk_window_create_cairo_surface (GdkDrawable *drawable,
				 int width,
				 int height)
{
  return _gdk_windowing_create_cairo_surface (GDK_WINDOW_OBJECT(drawable)->impl,
					      width, height);
}


static cairo_surface_t *
gdk_window_ref_cairo_surface (GdkDrawable *drawable)
{
  GdkWindowObject *private = (GdkWindowObject*) drawable;
  cairo_surface_t *surface;

  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;

      surface = paint->surface;
      cairo_surface_reference (surface);
    }
  else
    {
      
      /* This will be drawing directly to the window, so flush implicit paint */
      gdk_window_flush ((GdkWindow *)drawable);
      
      if (!private->cairo_surface)
	{
	  int width, height;
	  GdkDrawable *source;

	  /* It would be nice if we had some cairo support here so we
	     could set the clip rect on the cairo surface */
	  width = private->abs_x + private->width;
	  height = private->abs_y + private->height;

	  source = _gdk_drawable_get_source_drawable (drawable);

	  /* TODO: Avoid the typecheck crap by adding virtual call */
	  private->cairo_surface = _gdk_drawable_create_cairo_surface (source, width, height);
	  
	  if (private->cairo_surface)
	    {
	      cairo_surface_set_device_offset (private->cairo_surface, 
					       private->abs_x,
					       private->abs_y);
	      
	      cairo_surface_set_user_data (private->cairo_surface, &gdk_window_cairo_key,
					   drawable, gdk_window_cairo_surface_destroy);
	    }
	}
      else
	cairo_surface_reference (private->cairo_surface);
      
      surface = private->cairo_surface;
    }

  return surface;
}

static void
gdk_window_set_cairo_clip (GdkDrawable *drawable,
			   cairo_t *cr)
{
  GdkWindowObject *private = (GdkWindowObject*) drawable;

  if (!private->paint_stack)
    {
      cairo_save (cr);
      cairo_identity_matrix (cr);

      cairo_reset_clip (cr);
      
      cairo_new_path (cr);
      gdk_cairo_region (cr, private->clip_region);
      
      cairo_restore (cr);
      cairo_clip (cr);
    }
  else
    {
      GdkWindowPaint *paint = private->paint_stack->data;

      /* Only needs to clip to region if piggybacking
	 on an implicit paint pixmap */
      if (paint->uses_implicit)
	{
	  cairo_save (cr);
	  cairo_identity_matrix (cr);
	  
	  cairo_reset_clip (cr);
	  
	  cairo_new_path (cr);
	  gdk_cairo_region (cr, paint->region);
	  cairo_restore (cr);
	  
	  cairo_clip (cr);
	}
    }
}

/* Code for dirty-region queueing
 */
static GSList *update_windows = NULL;
static guint update_idle = 0;
static gboolean debug_updates = FALSE;

static inline gboolean
gdk_window_is_ancestor (GdkWindow *window,
                        GdkWindow *ancestor)
{
  while (window)
    {
      GdkWindow *parent = (GdkWindow*) ((GdkWindowObject*) window)->parent;

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

  for (tmp = update_windows; tmp; tmp = tmp->next)
    {
      GdkWindowObject *parent = GDK_WINDOW_OBJECT (window)->parent;

      /*  check if tmp is an ancestor of "window"; if it is, set a
       *  flag indicating that all following windows are either
       *  children of "window" or from a differen hierarchy
       */
      if (!has_ancestor_in_list && gdk_window_is_ancestor (window, tmp->data))
        has_ancestor_in_list = TRUE;

      /* insert in reverse stacking order when adding around siblings,
       * so processing updates properly paints over lower stacked windows
       */
      if (parent == GDK_WINDOW_OBJECT (tmp->data)->parent)
        {
          gint index = g_list_index (parent->children, window);
          for (; tmp && parent == GDK_WINDOW_OBJECT (tmp->data)->parent; tmp = tmp->next)
            {
              gint sibling_index = g_list_index (parent->children, tmp->data);
              if (index > sibling_index)
                break;
              prev = tmp;
            }
          /* here, tmp got advanced past all lower stacked siblings */
          tmp = g_slist_prepend (tmp, window);
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
          tmp = g_slist_prepend (tmp, window);

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
          tmp = g_slist_append (tmp, window);
          return;
        }

      prev = tmp;
    }

  /*  if all above checks failed ("window" is from a different
   *  hierarchy than what is already in the list) or the list is
   *  empty, prepend
   */
  update_windows = g_slist_prepend (update_windows, window);
}

static void
gdk_window_remove_update_window (GdkWindow *window)
{
  update_windows = g_slist_remove (update_windows, window);
}

static gboolean
gdk_window_update_idle (gpointer data)
{
  gdk_window_process_all_updates ();
  
  return FALSE;
}

static gboolean
gdk_window_is_toplevel_frozen (GdkWindow *window)
{
  GdkWindowObject *toplevel;

  toplevel = (GdkWindowObject *)gdk_window_get_toplevel (window);

  return toplevel->update_and_descendants_freeze_count > 0;
}

static void
gdk_window_schedule_update (GdkWindow *window)
{
  if (window &&
      (GDK_WINDOW_OBJECT (window)->update_freeze_count ||
       gdk_window_is_toplevel_frozen (window)))
    return;

  if (!update_idle)
    {
      update_idle = gdk_threads_add_idle_full (GDK_PRIORITY_REDRAW,
				     gdk_window_update_idle, NULL, NULL);
    }
}

static void
gdk_window_process_updates_recurse (GdkWindow *window,
				    GdkRegion *expose_region)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *child;
  GdkRegion *child_region;
  GdkRectangle r;
  GList *l;

  if (gdk_region_empty (expose_region))
    return;
  
  /* Iterate over children, starting at topmost */
  for (l = private->children; l != NULL; l = l->next)
    {
      child = l->data;
      
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
      
      child_region = gdk_region_rectangle (&r);
      if (child->shape)
	gdk_region_intersect (child_region, child->shape);
	
      if (child->impl == private->impl) 
	{
	  /* Client side child, expose */
	  gdk_region_intersect (child_region, expose_region);
	  gdk_region_subtract (expose_region, child_region);
	  gdk_region_offset (child_region, -child->x, -child->y);
	  gdk_window_process_updates_recurse ((GdkWindow *)child, child_region);
	}
      else 
	{
	  /* Native child, just remove area from expose region */
	  gdk_region_subtract (expose_region, child_region);
	}
      gdk_region_destroy (child_region);
    }

  if (private->event_mask & GDK_EXPOSURE_MASK)
    {
      GdkEvent event;
      
      event.expose.type = GDK_EXPOSE;
      event.expose.window = g_object_ref (window);
      event.expose.send_event = FALSE;
      event.expose.count = 0;
      event.expose.region = expose_region;
      gdk_region_get_clipbox (expose_region, &event.expose.area);
      
      (*_gdk_event_func) (&event, _gdk_event_data);
      
      g_object_unref (window);
    }
}

static void
gdk_window_process_updates_internal (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  gboolean save_region = FALSE;
  GdkRectangle clip_box;

  /* If an update got queued during update processing, we can get a
   * window in the update queue that has an empty update_area.
   * just ignore it.
   */
  if (private->update_area)
    {
      GdkRegion *update_area = private->update_area;
      private->update_area = NULL;
      
      if (_gdk_event_func && gdk_window_is_viewable (window))
	{
	  GdkRectangle window_rect;
	  GdkRegion *expose_region;
	  GdkRegion *window_region;
	  gboolean end_implicit;

          if (debug_updates)
            {
              /* Make sure we see the red invalid area before redrawing. */
              gdk_display_sync (gdk_drawable_get_display (window));
              g_usleep (70000);
            }

	  save_region = GDK_WINDOW_IMPL_GET_IFACE (private->impl)->queue_antiexpose (window, update_area);
	  if (save_region)
	    expose_region = gdk_region_copy (update_area);
	  else
	    expose_region = update_area;
	  
	  window_rect.x = 0;
	  window_rect.y = 0;
	  window_rect.width = private->width;
	  window_rect.height = private->height;

	  window_region = gdk_region_rectangle (&window_rect);
	  gdk_region_intersect (expose_region,
				window_region);
	  gdk_region_destroy (window_region);

	  gdk_region_get_clipbox (expose_region, &clip_box);
	  end_implicit = gdk_window_begin_implicit_paint (window, &clip_box);
	  gdk_window_process_updates_recurse (window, expose_region);
	  if (end_implicit)
	    gdk_window_end_implicit_paint (window);
	  
	  if (expose_region != update_area)
	    gdk_region_destroy (expose_region);
	}
      if (!save_region)
	gdk_region_destroy (update_area);
    }
  
  if (private->outstanding_moves)
    {
      /* Flush any outstanding moves, may happen if we moved a window but got
	 no actual invalid area */
      gdk_window_flush (window);
    }
}

static void
flush_all_displays (void)
{
  GSList *displays = gdk_display_manager_list_displays (gdk_display_manager_get ());
  GSList *tmp_list;

  for (tmp_list = displays; tmp_list; tmp_list = tmp_list->next)
    gdk_display_flush (tmp_list->data);

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

  if (update_idle)
    g_source_remove (update_idle);
  
  update_windows = NULL;
  update_idle = 0;

  g_slist_foreach (old_update_windows, (GFunc)g_object_ref, NULL);
  
  while (tmp_list)
    {
      GdkWindowObject *private = (GdkWindowObject *)tmp_list->data;
      
      if (!GDK_WINDOW_DESTROYED (tmp_list->data))
        {
	  if (private->update_freeze_count ||
	      gdk_window_is_toplevel_frozen (tmp_list->data))
	    gdk_window_add_update_window ((GdkWindow *) private);
	  else
	    gdk_window_process_updates_internal (tmp_list->data);
	}

      g_object_unref (tmp_list->data);
      tmp_list = tmp_list->next;
    }

  g_slist_free (old_update_windows);

  flush_all_displays ();
}

/**
 * gdk_window_process_updates:
 * @window: a #GdkWindow
 * @update_children: whether to also process updates for child windows
 *
 * Sends one or more expose events to @window. The areas in each 
 * expose event will cover the entire update area for the window (see
 * gdk_window_invalidate_region() for details). Normally GDK calls
 * gdk_window_process_all_updates() on your behalf, so there's no
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
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *impl_window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_IS_PAINTABLE (private->impl))
    {
      GdkPaintableIface *iface = GDK_PAINTABLE_GET_IFACE (private->impl);

      if (iface->process_updates)
        iface->process_updates ((GdkPaintable*)private->impl, update_children);

      return;
    }

  impl_window = gdk_window_get_impl_window (private);
  if ((impl_window->update_area ||
       impl_window->outstanding_moves) &&
      !impl_window->update_freeze_count &&
      !gdk_window_is_toplevel_frozen (window))
    {      
      gdk_window_process_updates_internal ((GdkWindow *)impl_window);
      gdk_window_remove_update_window ((GdkWindow *)impl_window);
    }

  if (update_children)
    {
      /* process updates in reverse stacking order so composition or
       * painting over achieves the desired effect for offscreen windows
       */
      GList *node;
      for (node = g_list_last (private->children); node; node = node->prev)
        gdk_window_process_updates (node->data, TRUE);
    }
}

/**
 * gdk_window_invalidate_rect:
 * @window: a #GdkWindow
 * @rect: rectangle to invalidate or %NULL to invalidate the whole
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
  GdkRectangle window_rect;
  GdkRegion *region;
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  if (private->input_only || !GDK_WINDOW_IS_MAPPED (window))
    return;

  if (!rect)
    {
      window_rect.x = 0;
      window_rect.y = 0;
      gdk_drawable_get_size (GDK_DRAWABLE (window),
                             &window_rect.width,
                             &window_rect.height);
      rect = &window_rect;
    }

  region = gdk_region_rectangle (rect);
  gdk_window_invalidate_region (window, region, invalidate_children);
  gdk_region_destroy (region);
}

static void
draw_ugly_color (GdkWindow       *window,
                 const GdkRegion *region)
{
  /* Draw ugly color all over the newly-invalid region */
  GdkColor ugly_color = { 0, 50000, 10000, 10000 };
  GdkGC *ugly_gc;
  GdkRectangle clipbox;
    
  ugly_gc = gdk_gc_new (window);
  gdk_gc_set_rgb_fg_color (ugly_gc, &ugly_color);
  gdk_gc_set_clip_region (ugly_gc, region);

  gdk_region_get_clipbox (region, &clipbox);
  
  gdk_draw_rectangle (window,
		      ugly_gc,
		      TRUE,
		      clipbox.x, clipbox.y,
		      clipbox.width, clipbox.height);
  
  g_object_unref (ugly_gc);
}

/**
 * gdk_window_invalidate_maybe_recurse:
 * @window: a #GdkWindow
 * @region: a #GdkRegion
 * @child_func: function to use to decide if to recurse to a child,
 *              %NULL means never recurse.
 * @user_data: data passed to @child_func
 *
 * Adds @region to the update area for @window. The update area is the
 * region that needs to be redrawn, or "dirty region." The call
 * gdk_window_process_updates() sends one or more expose events to the
 * window, which together cover the entire update area. An
 * application would normally redraw the contents of @window in
 * response to those expose events.
 *
 * GDK will call gdk_window_process_all_updates() on your behalf
 * whenever your program returns to the main loop and becomes idle, so
 * normally there's no need to do that manually, you just need to
 * invalidate regions that you know should be redrawn.
 *
 * The @child_func parameter controls whether the region of
 * each child window that intersects @region will also be invalidated.
 * Only children for which @child_func returns TRUE will have the area
 * invalidated.
 **/
void
gdk_window_invalidate_maybe_recurse (GdkWindow       *window,
                                     const GdkRegion *region,
                                     gboolean       (*child_func) (GdkWindow *,
                                                                   gpointer),
				     gpointer   user_data)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *impl_window;
  GdkRegion *visible_region;
  GList *tmp_list;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  if (private->input_only || !GDK_WINDOW_IS_MAPPED (window))
    return;

  if (GDK_IS_PAINTABLE (private->impl))
    {
      GdkPaintableIface *iface = GDK_PAINTABLE_GET_IFACE (private->impl);

      if (iface->invalidate_maybe_recurse)
        iface->invalidate_maybe_recurse ((GdkPaintable*)private->impl, 
                                         region, child_func, user_data);
      return;
    }

  visible_region = gdk_drawable_get_visible_region (window);
  gdk_region_intersect (visible_region, region);

  tmp_list = private->children;
  while (tmp_list)
    {
      GdkWindowObject *child = tmp_list->data;
      
      if (!child->input_only)
	{
	  GdkRegion *child_region;
	  GdkRectangle child_rect;

	  child_rect.x = child->x;
	  child_rect.y = child->y;
	  child_rect.width = child->width;
	  child_rect.height = child->height;
	  child_region = gdk_region_rectangle (&child_rect);
	  
	  /* remove child area from the invalid area of the parent */
	  if (GDK_WINDOW_IS_MAPPED (child) && !child->shaped &&
	      !child->composited &&
              !gdk_window_is_offscreen (child))
	    gdk_region_subtract (visible_region, child_region);
	  
	  if (child_func && (*child_func) ((GdkWindow *)child, user_data))
	    {
              GdkRegion *tmp = gdk_region_copy (region);

	      gdk_region_offset (tmp, - child_rect.x, - child_rect.y);
	      gdk_region_offset (child_region, - child_rect.x, - child_rect.y);
	      gdk_region_intersect (child_region, tmp);
	      
	      gdk_window_invalidate_maybe_recurse ((GdkWindow *)child,
						   child_region, child_func, user_data);
	      
	      gdk_region_destroy (tmp);
	    }

	  gdk_region_destroy (child_region);
	}

      tmp_list = tmp_list->next;
    }

  impl_window = gdk_window_get_impl_window (private);
  
  if (!gdk_region_empty (visible_region)  ||
      /* Even if we're not exposing anything, make sure we process
	 idles for windows with outstanding moves */
      (impl_window->outstanding_moves != NULL &&
       impl_window->update_area == NULL))
    {
      if (debug_updates)
        draw_ugly_color (window, region);
      
      /* Convert to impl coords */
      gdk_region_offset (visible_region, private->abs_x, private->abs_y);
      if (impl_window->update_area)
	{
	  gdk_region_union (impl_window->update_area, visible_region);
	}
      else
	{
	  gdk_window_add_update_window ((GdkWindow *)impl_window);
	  impl_window->update_area = gdk_region_copy (visible_region);
	  
	  gdk_window_schedule_update ((GdkWindow *)impl_window);
	}
    }
  
  gdk_region_destroy (visible_region);
}

static gboolean
true_predicate (GdkWindow *window,
		gpointer   user_data)
{
  return TRUE;
}

/**
 * gdk_window_invalidate_region:
 * @window: a #GdkWindow
 * @region: a #GdkRegion
 * @invalidate_children: %TRUE to also invalidate child windows 
 *
 * Adds @region to the update area for @window. The update area is the
 * region that needs to be redrawn, or "dirty region." The call
 * gdk_window_process_updates() sends one or more expose events to the
 * window, which together cover the entire update area. An
 * application would normally redraw the contents of @window in
 * response to those expose events.
 *
 * GDK will call gdk_window_process_all_updates() on your behalf
 * whenever your program returns to the main loop and becomes idle, so
 * normally there's no need to do that manually, you just need to
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
			      const GdkRegion *region,
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
 * @region: a #GdkRegion
 *
 * Adds @region to the update area for @window. The update area is the
 * region that needs to be redrawn, or "dirty region." The call
 * gdk_window_process_updates() sends one or more expose events to the
 * window, which together cover the entire update area. An
 * application would normally redraw the contents of @window in
 * response to those expose events.
 *
 * GDK will call gdk_window_process_all_updates() on your behalf
 * whenever your program returns to the main loop and becomes idle, so
 * normally there's no need to do that manually, you just need to
 * invalidate regions that you know should be redrawn.
 *
 * This version of invalidation is used when you recieve expose events
 * from the native window system. It exposes the native window, plus
 * any non-native child windows (but not native child windows, as those would
 * have gotten their own expose events). 
 **/
void
_gdk_window_invalidate_for_expose (GdkWindow       *window,
				   const GdkRegion *region)
{
  gdk_window_invalidate_maybe_recurse (window, region,
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
 * calling gdk_region_destroy() on the returned region if it's non-%NULL.
 * 
 * Return value: the update area for @window
 **/
GdkRegion *
gdk_window_get_update_area (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *impl_window;
  GdkRegion *tmp_region;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  impl_window = gdk_window_get_impl_window (private);
  
  if (impl_window->update_area)
    {
      tmp_region = gdk_region_copy (private->clip_region_with_children);
      /* Convert to impl coords */
      gdk_region_offset (tmp_region, private->abs_x, private->abs_y);
      gdk_region_intersect (tmp_region, impl_window->update_area);

      if (gdk_region_empty (tmp_region))
	{
	  gdk_region_destroy (tmp_region);
	  return NULL;
	}
      else
	{
	  gdk_region_subtract (impl_window->update_area, tmp_region);

	  if (gdk_region_empty (impl_window->update_area) &&
	      impl_window->outstanding_moves == NULL)
	    {
	      gdk_region_destroy (impl_window->update_area);
	      impl_window->update_area = NULL;
	  
	      gdk_window_remove_update_window ((GdkWindow *)impl_window);
	    }
	  
	  /* Convert from impl coords */
	  gdk_region_offset (tmp_region, -private->abs_x, -private->abs_y);
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
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (private->update_area)
    {
      gdk_window_remove_update_window (window);

      gdk_region_destroy (private->update_area);
      private->update_area = NULL;
    }
}

/**
 * gdk_window_freeze_updates:
 * @window: a #GdkWindow
 * 
 * Temporarily freezes a window such that it won't receive expose
 * events.  The window will begin receiving expose events again when
 * gdk_window_thaw_updates() is called. If gdk_window_freeze_updates()
 * has been called more than once, gdk_window_thaw_updates() must be called
 * an equal number of times to begin processing exposes.
 **/
void
gdk_window_freeze_updates (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *impl_window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl_window = gdk_window_get_impl_window (private);
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
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *impl_window;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (private->update_freeze_count > 0);

  impl_window = gdk_window_get_impl_window (private);
  
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
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (private->window_type != GDK_WINDOW_CHILD);

  private->update_and_descendants_freeze_count++;
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
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (private->window_type != GDK_WINDOW_CHILD);
  g_return_if_fail (private->update_and_descendants_freeze_count > 0);

  private->update_and_descendants_freeze_count--;

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
 * see exactly what's being redrawn when, in what order.
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
 * @new_width: location to store resulting width
 * @new_height: location to store resulting height
 * 
 * Constrains a desired width and height according to a 
 * set of geometry hints (such as minimum and maximum size).
 */
void
gdk_window_constrain_size (GdkGeometry *geometry,
			   guint        flags,
			   gint         width,
			   gint         height,
			   gint        *new_width,
			   gint        *new_height)
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
 * @x: return location for X coordinate of pointer or %NULL to not
 *      return the X coordinate
 * @y: return location for Y coordinate of pointer or %NULL to not
 *      return the Y coordinate
 * @mask: return location for modifier mask or %NULL to not return the
 *      modifier mask
 *
 * Obtains the current pointer position and modifier state.
 * The position is given in coordinates relative to the upper left 
 * corner of @window.
 * 
 * Return value: the window containing the pointer (as with
 * gdk_window_at_pointer()), or %NULL if the window containing the
 * pointer isn't known to GDK
 **/
GdkWindow*
gdk_window_get_pointer (GdkWindow	  *window,
			gint		  *x,
			gint		  *y,
			GdkModifierType   *mask)
{
  GdkDisplay *display;
  gint tmp_x, tmp_y;
  GdkModifierType tmp_mask;
  GdkWindow *child;
  
  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), NULL);

  if (window)
    {
      display = gdk_drawable_get_display (window);
    }
  else
    {
      GdkScreen *screen = gdk_screen_get_default ();

      display = gdk_screen_get_display (screen);
      window = gdk_screen_get_root_window (screen);
      
      GDK_NOTE (MULTIHEAD,
		g_message ("Passing NULL for window to gdk_window_get_pointer()\n"
			   "is not multihead safe"));
    }

  child = display->pointer_hooks->window_get_pointer (display, window, &tmp_x, &tmp_y, &tmp_mask);

  if (x)
    *x = tmp_x;
  if (y)
    *y = tmp_y;
  if (mask)
    *mask = tmp_mask;

  return child;
}

/**
 * gdk_window_at_pointer:
 * @win_x: return location for origin of the window under the pointer
 * @win_y: return location for origin of the window under the pointer
 * 
 * Obtains the window underneath the mouse pointer, returning the
 * location of that window in @win_x, @win_y. Returns %NULL if the
 * window under the mouse pointer is not known to GDK (if the window
 * belongs to another application and a #GdkWindow hasn't been created
 * for it with gdk_window_foreign_new())
 *
 * NOTE: For multihead-aware widgets or applications use
 * gdk_display_get_window_at_pointer() instead.
 * 
 * Return value: window under the mouse pointer
 **/
GdkWindow*
gdk_window_at_pointer (gint *win_x,
		       gint *win_y)
{
  return gdk_display_get_window_at_pointer (gdk_display_get_default (), win_x, win_y);
}

/**
 * gdk_get_default_root_window:
 * 
 * Obtains the root window (parent all other windows are inside)
 * for the default display and screen.
 * 
 * Return value: the default root window
 **/
GdkWindow *
gdk_get_default_root_window (void)
{
  return gdk_screen_get_root_window (gdk_screen_get_default ());
}

/**
 * gdk_window_foreign_new:
 * @anid: a native window handle.
 * 
 * Wraps a native window for the default display in a #GdkWindow.
 * This may fail if the window has been destroyed.
 *
 * For example in the X backend, a native window handle is an Xlib
 * <type>XID</type>.
 * 
 * Return value: the newly-created #GdkWindow wrapper for the 
 *    native window or %NULL if the window has been destroyed.
 **/
GdkWindow *
gdk_window_foreign_new (GdkNativeWindow anid)
{
  return gdk_window_foreign_new_for_display (gdk_display_get_default (), anid);
}

static inline void
gdk_window_raise_internal (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *parent = private->parent;

  if (parent)
    {
      parent->children = g_list_remove (parent->children, window);
      parent->children = g_list_prepend (parent->children, window);
    }
}

static void
show_all_visible_impls (GdkWindowObject *private)
{
  GdkWindowObject *child;
  GList *l;
  
  for (l = private->children; l != NULL; l = l->next)
    {
      child = l->data;
      if (GDK_WINDOW_IS_MAPPED (child))
	show_all_visible_impls (child);
    }
  
  if (gdk_window_has_impl (private))
    GDK_WINDOW_IMPL_GET_IFACE (private->impl)->show ((GdkWindow *)private);
}

static void
gdk_window_show_internal (GdkWindow *window, gboolean raise)
{
  GdkWindowObject *private;
  gboolean was_mapped;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;
  if (private->destroyed)
    return;

  was_mapped = GDK_WINDOW_IS_MAPPED (window);
  
  if (raise)
    {
      /* Keep children in (reverse) stacking order */
      gdk_window_raise_internal (window);
      
      if (gdk_window_has_impl (private))
	GDK_WINDOW_IMPL_GET_IFACE (private->impl)->raise (window);
    }

  if (gdk_window_has_impl (private))
    {
      if (!was_mapped)
	gdk_synthesize_window_state (window,
				     GDK_WINDOW_STATE_WITHDRAWN,
				     0);
    }
  else
    {
      private->state = 0;
    }

  if (gdk_window_is_viewable (window))
    show_all_visible_impls (private);
  
  if (!was_mapped)
    {
      if (private->event_mask & GDK_STRUCTURE_MASK)
	_gdk_make_event (GDK_WINDOW (private), GDK_MAP, NULL, FALSE);
      
      if (private->parent && private->parent->event_mask & GDK_SUBSTRUCTURE_MASK)
	_gdk_make_event (GDK_WINDOW (private), GDK_MAP, NULL, FALSE);
    }
  
  if (!was_mapped || raise)
    {
      recompute_visible_regions (private, TRUE, FALSE);
      
      if (gdk_window_is_viewable (window))
	{
	  _gdk_syntesize_crossing_events_for_geometry_change (window);
	  gdk_window_invalidate_rect (window, NULL, TRUE);
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
 * that you can't really use XMapWindow() directly on a GDK window).
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
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;
  if (private->destroyed)
    return;

  /* Keep children in (reverse) stacking order */
  gdk_window_raise_internal (window);

  if (gdk_window_has_impl (private))
    GDK_WINDOW_IMPL_GET_IFACE (private->impl)->raise (window);

  recompute_visible_regions (private, TRUE, FALSE);

  gdk_window_invalidate_rect (window, NULL, TRUE);
}

static void
gdk_window_lower_internal (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *parent = private->parent;

  if (parent)
    {
      parent->children = g_list_remove (parent->children, window);
      parent->children = g_list_append (parent->children, window);
    }
}

static void
gdk_window_invalidate_in_parent (GdkWindowObject *private)
{
  GdkRectangle r, child;

  if (private->parent == NULL ||
      GDK_WINDOW_TYPE (private->parent) == GDK_WINDOW_ROOT)
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

  gdk_window_invalidate_rect (GDK_WINDOW (private->parent), &r, TRUE);
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
 * Note that gdk_window_show() raises the window again, so don't call this
 * function before gdk_window_show(). (Try gdk_window_show_unraised().)
 */
void
gdk_window_lower (GdkWindow *window)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;
  if (private->destroyed)
    return;

  /* Keep children in (reverse) stacking order */
  gdk_window_lower_internal (window);

  recompute_visible_regions (private, TRUE, FALSE);
  
  if (gdk_window_has_impl (private))
    GDK_WINDOW_IMPL_GET_IFACE (private->impl)->lower (window);

  _gdk_syntesize_crossing_events_for_geometry_change (window);
  gdk_window_invalidate_in_parent (private);
}

/**
 * gdk_window_show:
 * @window: a #GdkWindow
 *
 * Like gdk_window_show_unraised(), but also raises the window to the
 * top of the window stack (moves the window to the front of the
 * Z-order).
 *
 * This function maps a window so it's visible onscreen. Its opposite
 * is gdk_window_hide().
 *
 * When implementing a #GtkWidget, you should call this function on the widget's
 * #GdkWindow as part of the "map" method.
 */
void
gdk_window_show (GdkWindow *window)
{
  gdk_window_show_internal (window, TRUE);
}

static void
hide_all_visible_impls (GdkWindowObject *private)
{
  GdkWindowObject *child;
  GList *l;
  
  for (l = private->children; l != NULL; l = l->next)
    {
      child = l->data;
      
      if (GDK_WINDOW_IS_MAPPED (child))
	hide_all_visible_impls (child);
    }
  
  if (gdk_window_has_impl (private))
    GDK_WINDOW_IMPL_GET_IFACE (private->impl)->hide ((GdkWindow *)private);
}


/**
 * gdk_window_hide:
 * @window: a #GdkWindow
 *
 * For toplevel windows, withdraws them, so they will no longer be
 * known to the window manager; for all windows, unmaps them, so
 * they won't be displayed. Normally done automatically as
 * part of gtk_widget_hide().
 */
void
gdk_window_hide (GdkWindow *window)
{
  GdkWindowObject *private;
  gboolean was_mapped, was_viewable;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;
  if (private->destroyed)
    return;

  was_mapped = GDK_WINDOW_IS_MAPPED (private);
  was_viewable = gdk_window_is_viewable (window);
  
  if (gdk_window_has_impl (private))
    {

      if (GDK_WINDOW_IS_MAPPED (window))
        gdk_synthesize_window_state (window,
                                     0,
                                     GDK_WINDOW_STATE_WITHDRAWN);
    }
  else if (was_mapped)
    {
      GdkDisplay *display;

      /* May need to break grabs on children */
      display = gdk_drawable_get_display (window);

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

      private->state = GDK_WINDOW_STATE_WITHDRAWN;
    }

  if (was_viewable)
    hide_all_visible_impls (private);
  
  recompute_visible_regions (private, TRUE, FALSE);
  
  if (was_mapped)
    {
      if (private->event_mask & GDK_STRUCTURE_MASK)
	_gdk_make_event (GDK_WINDOW (private), GDK_UNMAP, NULL, FALSE);
      
      if (private->parent && private->parent->event_mask & GDK_SUBSTRUCTURE_MASK)
	_gdk_make_event (GDK_WINDOW (private), GDK_UNMAP, NULL, FALSE);
      
      _gdk_syntesize_crossing_events_for_geometry_change (GDK_WINDOW (private->parent));
    }
  
  /* Invalidate the rect */
  gdk_window_invalidate_in_parent (private);
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
  GdkWindowObject *private;
  gboolean was_mapped;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;
  if (private->destroyed)
    return;

  was_mapped = GDK_WINDOW_IS_MAPPED (private);
  
  if (gdk_window_has_impl (private))
    {
      GDK_WINDOW_IMPL_GET_IFACE (private->impl)->withdraw (window);
      
      if (was_mapped)
	{
	  if (private->event_mask & GDK_STRUCTURE_MASK)
	    _gdk_make_event (GDK_WINDOW (private), GDK_UNMAP, NULL, FALSE);
	  
	  if (private->parent && private->parent->event_mask & GDK_SUBSTRUCTURE_MASK)
	    _gdk_make_event (GDK_WINDOW (private), GDK_UNMAP, NULL, FALSE);
	  
	  _gdk_syntesize_crossing_events_for_geometry_change (GDK_WINDOW (private->parent));
	}
      
      recompute_visible_regions (private, TRUE, FALSE);
    }
}

/**
 * gdk_window_set_events:
 * @window: a #GdkWindow
 * @event_mask: event mask for @window
 *
 * The event mask for a window determines which events will be reported
 * for that window. For example, an event mask including #GDK_BUTTON_PRESS_MASK
 * means the window should report button press events. The event mask
 * is the bitwise OR of values from the #GdkEventMask enumeration.
 **/
void
gdk_window_set_events (GdkWindow       *window,
		       GdkEventMask     event_mask)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;
  if (private->destroyed)
    return;

  private->event_mask = event_mask;
}

/**
 * gdk_window_get_events:
 * @window: a #GdkWindow
 *
 * Gets the event mask for @window. See gdk_window_set_events().
 *
 * Return value: event mask for @window
 **/
GdkEventMask
gdk_window_get_events (GdkWindow *window)
{
  GdkWindowObject *private;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  private = (GdkWindowObject *) window;
  if (private->destroyed)
    return 0;

  return private->event_mask;
}

static void
move_native_children (GdkWindowObject *private)
{
  GList *l;
  GdkWindowObject *child;
  
  for (l = private->children; l; l = l->next)
    {
      child = l->data;
      
      if (child->impl != private->impl)
	GDK_WINDOW_IMPL_GET_IFACE (child->impl)->move_resize ((GdkWindow *)child, TRUE, child->x, child->y, child->width, child->height);
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
  GdkWindowObject *private;
  GdkRegion *old_region, *new_region, *copy_area;
  GdkWindowObject *impl_window;
  gboolean expose;
  int old_x, old_y, old_abs_x, old_abs_y;
  int dx, dy;
  gboolean do_move_native_children;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;
  if (private->destroyed)
    return;

  do_move_native_children = FALSE;
  expose = FALSE;
  old_region = NULL;

  impl_window = gdk_window_get_impl_window (private);

  old_x = private->x;
  old_y = private->y;
  
  if (gdk_window_has_impl (private))
    {
      if (GDK_WINDOW_IS_MAPPED (window))
	{
	  expose = !private->input_only;
	  old_region = gdk_region_copy (private->clip_region);
	  /* Adjust region to parent window coords */
	  gdk_region_offset (old_region, private->x, private->y);
	}
      
      GDK_WINDOW_IMPL_GET_IFACE (private->impl)->move_resize (window, with_move, x, y, width, height);
    }
  else
    {
      if (GDK_WINDOW_IS_MAPPED (window))
	{
	  expose = !private->input_only;
	  old_region = gdk_region_copy (private->clip_region);
	  /* Adjust region to parent window coords */
	  gdk_region_offset (old_region, private->x, private->y);
	}

      if (with_move)
	{
	  private->x = x;
	  private->y = y;
	}
      if (!(width < 0 && height < 0))
	{
	  if (width < 1)
	    width = 1;
	  private->width = width;
	  if (height < 1)
	    height = 1;
	  private->height = height;
	}

      do_move_native_children = TRUE;
    }

  dx = private->x - old_x;
  dy = private->y - old_y;

  old_abs_x = private->abs_x;
  old_abs_y = private->abs_y;
  
  recompute_visible_regions (private, TRUE, FALSE);

  if (do_move_native_children &&
      (old_abs_x != private->abs_x ||
       old_abs_y != private->abs_y))
      move_native_children (private);
  
  if (expose)
    {
      new_region = gdk_region_copy (private->clip_region);
      /* Adjust region to parent window coords */
      gdk_region_offset (new_region, private->x, private->y);

      /* copy_area:
       * Part of the data at the new location can be copied from the
       * old location, this area is the intersection of the old region
       * moved as the copy will move it and then intersected with
       * the new region.
       *
       * new_region:
       * Everything in the old and new regions that is not copied must be
       * invalidated (including children) as this is newly exposed 
       */
      copy_area = gdk_region_copy (new_region);
	  
      gdk_region_union (new_region, old_region);
      
      gdk_region_offset (old_region, dx, dy);
      gdk_region_intersect (copy_area, old_region);
      
      gdk_region_subtract (new_region, copy_area);

      /* Convert old region to impl coords */
      gdk_region_offset (old_region, -dx + private->abs_x - private->x, -dy + private->abs_y - private->y);

      /* The old_region area is moved and we queue translations for all expose events
	 to it that will be sent before the copy operation */
      GDK_WINDOW_IMPL_GET_IFACE (impl_window->impl)->queue_translation ((GdkWindow *)impl_window,
									old_region, dx, dy);
      
      /* convert from parent coords to impl */
      gdk_region_offset (copy_area, private->abs_x - private->x, private->abs_y - private->y);

      move_region_on_impl (private, copy_area, dx, dy); /* Takes ownership of copy_area */

      /* Invalidate affected part in the parent window
       *  (no higher window should be affected)
       * We also invalidate any children in that area, which could include
       * this window if it still overlaps that area.
       */
      gdk_window_invalidate_region (GDK_WINDOW (private->parent), new_region, TRUE);

      gdk_region_destroy (old_region);
      gdk_region_destroy (new_region);
    }

  _gdk_syntesize_crossing_events_for_geometry_change (window);
}



/**
 * gdk_window_move:
 * @window: a #GdkWindow
 * @x: X coordinate relative to window's parent
 * @y: Y coordinate relative to window's parent
 *
 * Repositions a window relative to its parent window.
 * For toplevel windows, window managers may ignore or modify the move;
 * you should probably use gtk_window_move() on a #GtkWindow widget
 * anyway, instead of using GDK functions. For child windows,
 * the move will reliably succeed.
 *
 * If you're also planning to resize the window, use gdk_window_move_resize()
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
 * If you're also planning to move the window, use gdk_window_move_resize()
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
 * @x: new X position relative to window's parent
 * @y: new Y position relative to window's parent
 * @width: new width
 * @height: new height
 *
 * Equivalent to calling gdk_window_move() and gdk_window_resize(),
 * except that both operations are performed at once, avoiding strange
 * visual effects. (i.e. the user may be able to see the window first
 * move, then resize, if you don't use gdk_window_move_resize().)
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
 * subwindows, or if the edges of the window's parent do not extend
 * beyond the edges of the window. In other cases, a multi-step process
 * is used to scroll the window which may produce temporary visual
 * artifacts and unnecessary invalidations.
 **/
void
gdk_window_scroll (GdkWindow *window,
		   gint       dx,
		   gint       dy)
{
  GdkWindowObject *private = (GdkWindowObject *) window;
  GdkWindowObject *impl_window;
  GdkRegion *source_area, *copy_area, *noncopy_area;
  GList *tmp_list;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (dx == 0 && dy == 0)
    return;

  if (private->destroyed)
    return;

  /* First move all child windows, without causing invalidation */
  
  tmp_list = private->children;
  while (tmp_list)
    {
      GdkWindow *child = GDK_WINDOW (tmp_list->data);
      GdkWindowObject *child_obj = GDK_WINDOW_OBJECT (child);
      
      /* Just update the positions, the bits will move with the copy */
      child_obj->x += dx;
      child_obj->y += dy;
      
      tmp_list = tmp_list->next;
    }

  recompute_visible_regions (private, FALSE, TRUE);

  move_native_children (private);
  
  /* Then copy the actual bits of the window w/ child windows */
  
  impl_window = gdk_window_get_impl_window (private);

  /* Calculate the area that can be gotten by copying the old area */
  copy_area = gdk_region_copy (private->clip_region);
  gdk_region_offset (copy_area, dx, dy);
  gdk_region_intersect (copy_area, private->clip_region);

  /* And the rest need to be invalidated */
  noncopy_area = gdk_region_copy (private->clip_region);
  gdk_region_subtract (noncopy_area, copy_area);

  /* Get window clip and convert to real window coords, this
     area is moved and we queue translations for all expose events
     to it that will be sent before the copy operation */

  source_area = gdk_region_copy (private->clip_region);
  /* convert from window coords to real parent */
  gdk_region_offset (source_area, private->abs_x, private->abs_y);
  GDK_WINDOW_IMPL_GET_IFACE (impl_window->impl)->queue_translation ((GdkWindow *)impl_window,
								    source_area, dx, dy);
  gdk_region_destroy (source_area);
  
  /* convert from window coords to impl */
  gdk_region_offset (copy_area, private->abs_x, private->abs_y);

  move_region_on_impl (private, copy_area, dx, dy); /* Takes ownership of copy_area */
   
  /* Invalidate not copied regions */
  gdk_window_invalidate_region (window, noncopy_area, TRUE);

  gdk_region_destroy (noncopy_area);
  
  _gdk_syntesize_crossing_events_for_geometry_change (window);
}

/**
 * gdk_window_move_region:
 * @window: a #GdkWindow
 * @region: The #GdkRegion to move
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
			const GdkRegion *region,
			gint             dx,
			gint             dy)
{
  GdkWindowObject *private = (GdkWindowObject *) window;
  GdkWindowObject *impl_window;
  GdkRegion *source_area;
  GdkRegion *nocopy_area;
  GdkRegion *copy_area;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (region != NULL);

  if (dx == 0 && dy == 0)
    return;

  if (private->destroyed)
    return;

  impl_window = gdk_window_get_impl_window (private);
  
  /* compute source regions */
  source_area = gdk_region_copy (region);
  gdk_region_intersect (source_area, private->clip_region_with_children);

  /* compute destination regions */
  copy_area = gdk_region_copy (source_area);
  gdk_region_offset (copy_area, dx, dy);
  gdk_region_intersect (copy_area, private->clip_region_with_children);

  /* invalidate parts of the region not covered by the copy */
  nocopy_area = gdk_region_copy (region);
  gdk_region_offset (nocopy_area, dx, dy);
  gdk_region_subtract (nocopy_area, copy_area);
  gdk_window_invalidate_region (window, nocopy_area, FALSE);
  gdk_region_destroy (nocopy_area);

  gdk_region_offset (source_area, private->abs_x, private->abs_y);
  GDK_WINDOW_IMPL_GET_IFACE (impl_window->impl)->queue_translation ((GdkWindow *)impl_window,
								    source_area, dx, dy);
  
  /* convert from window coords to impl */
  gdk_region_offset (copy_area, private->abs_x, private->abs_y);
  
  move_region_on_impl (private, copy_area, dx, dy); /* Takes ownership of copy_area */

  gdk_region_destroy (source_area);
}

/**
 * gdk_window_set_background:
 * @window: a #GdkWindow
 * @color: an allocated #GdkColor
 *
 * Sets the background color of @window. (However, when using GTK+,
 * set the background of a widget with gtk_widget_modify_bg() - if
 * you're an application - or gtk_style_set_background() - if you're
 * implementing a custom widget.)
 *
 * The @color must be allocated; gdk_rgb_find_color() is the best way
 * to allocate a color.
 *
 * See also gdk_window_set_back_pixmap().
 */
void
gdk_window_set_background (GdkWindow      *window,
			   const GdkColor *color)
{
  GdkWindowObject *private;
  GdkColormap *colormap = gdk_drawable_get_colormap (window);

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;
  
  private->bg_color = *color;
  gdk_colormap_query_color (colormap, private->bg_color.pixel, &private->bg_color);

  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    g_object_unref (private->bg_pixmap);
  
  private->bg_pixmap = NULL;

  if (!GDK_WINDOW_DESTROYED (window) && gdk_window_has_impl (private))
    GDK_WINDOW_IMPL_GET_IFACE (private->impl)->set_background (window, &private->bg_color);
}

/**
 * gdk_window_set_back_pixmap:
 * @window: a #GdkWindow
 * @pixmap: a #GdkPixmap, or %NULL
 * @parent_relative: whether the tiling origin is at the origin of
 *   @window's parent
 *
 * Sets the background pixmap of @window. May also be used to set a
 * background of "None" on @window, by setting a background pixmap
 * of %NULL.
 *
 * A background pixmap will be tiled, positioning the first tile at
 * the origin of @window, or if @parent_relative is %TRUE, the tiling
 * will be done based on the origin of the parent window (useful to
 * align tiles in a parent with tiles in a child).
 *
 * A background pixmap of %NULL means that the window will have no
 * background.  A window with no background will never have its
 * background filled by the windowing system, instead the window will
 * contain whatever pixels were already in the corresponding area of
 * the display.
 *
 * The windowing system will normally fill a window with its background
 * when the window is obscured then exposed, and when you call
 * gdk_window_clear().
 */
void
gdk_window_set_back_pixmap (GdkWindow *window,
			    GdkPixmap *pixmap,
			    gboolean   parent_relative)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (pixmap == NULL || !parent_relative);
  g_return_if_fail (pixmap == NULL || gdk_drawable_get_depth (window) == gdk_drawable_get_depth (pixmap));

  private = (GdkWindowObject *) window;

  if (pixmap && !gdk_drawable_get_colormap (pixmap))
    {
      g_warning ("gdk_window_set_back_pixmap(): pixmap must have a colormap");
      return;
    }
  
  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    g_object_unref (private->bg_pixmap);

  if (parent_relative)
    private->bg_pixmap = GDK_PARENT_RELATIVE_BG;
  else if (pixmap)
    private->bg_pixmap = g_object_ref (pixmap);
  else
    private->bg_pixmap = GDK_NO_BG;
  
  if (!GDK_WINDOW_DESTROYED (window) && gdk_window_has_impl (private))
    GDK_WINDOW_IMPL_GET_IFACE (private->impl)->set_back_pixmap (window, private->bg_pixmap);
}

static void
update_cursor (GdkDisplay *display)
{
  GdkWindowObject *pointer_window, *cursor_window;
  
  pointer_window = (GdkWindowObject *)display->pointer_info.window_under_pointer;
  
  cursor_window = pointer_window;
  while (cursor_window->cursor == NULL &&
	 cursor_window->parent != NULL &&
	 cursor_window->parent->window_type != GDK_WINDOW_ROOT)
    cursor_window = cursor_window->parent;

  if (display->pointer_grab.window != NULL &&
      !is_parent_of (display->pointer_grab.window, (GdkWindow *)cursor_window))
    cursor_window = (GdkWindowObject *)display->pointer_grab.window;
  
  GDK_WINDOW_IMPL_GET_IFACE (pointer_window->impl)->set_cursor ((GdkWindow *)pointer_window,
								cursor_window->cursor);
}

/**
 * gdk_window_set_cursor:
 * @window: a #GdkWindow
 * @cursor: a cursor
 *
 * Sets the mouse pointer for a #GdkWindow. Use gdk_cursor_new_for_display() 
 * or gdk_cursor_new_from_pixmap() to create the cursor. To make the cursor 
 * invisible, use %GDK_BLANK_CURSOR. Passing %NULL for the @cursor argument 
 * to gdk_window_set_cursor() means that @window will use the cursor of its 
 * parent window. Most windows should use this default.
 */
void
gdk_window_set_cursor (GdkWindow *window,
		       GdkCursor *cursor)
{
  GdkWindowObject *private;
  GdkDisplay *display;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;
  display = gdk_drawable_get_display (window);

  if (private->cursor)
    {
      gdk_cursor_unref (private->cursor);
      private->cursor = NULL;
    }

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (cursor)
	private->cursor = gdk_cursor_ref (cursor);

      if (is_parent_of (window, display->pointer_info.window_under_pointer))
	update_cursor (display);
    }
}

/**
 * gdk_window_get_geometry:
 * @window: a #GdkWindow
 * @x: return location for X coordinate of window (relative to its parent)
 * @y: return location for Y coordinate of window (relative to its parent)
 * @width: return location for width of window
 * @height: return location for height of window
 * @depth: return location for bit depth of window
 *
 * Any of the return location arguments to this function may be %NULL,
 * if you aren't interested in getting the value of that field.
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
 * <note>
 * If @window is not a toplevel, it is <emphasis>much</emphasis> better
 * to call gdk_window_get_position() and gdk_drawable_get_size() instead,
 * because it avoids the roundtrip to the X server and because
 * gdk_drawable_get_size() supports the full 32-bit coordinate space,
 * whereas gdk_window_get_geometry() is restricted to the 16-bit
 * coordinates of X11.
 *</note>
 **/
void
gdk_window_get_geometry (GdkWindow *window,
			 gint      *x,
			 gint      *y,
			 gint      *width,
			 gint      *height,
			 gint      *depth)
{
  GdkWindowObject *private;

  if (!window)
    {
      GDK_NOTE (MULTIHEAD,
		g_message ("gdk_window_get_geometry(): Window needs "
                           "to be non-NULL to be multi head safe"));
      window = gdk_screen_get_root_window ((gdk_screen_get_default ()));
    }

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (gdk_window_has_impl (private))
	GDK_WINDOW_IMPL_GET_IFACE (private->impl)->get_geometry (window, x, y,
								 width, height,
								 depth);
      else
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
}

/**
 * gdk_window_get_origin:
 * @window: a #GdkWindow
 * @x: return location for X coordinate
 * @y: return location for Y coordinate
 *
 * Obtains the position of a window in root window coordinates.
 * (Compare with gdk_window_get_position() and
 * gdk_window_get_geometry() which return the position of a window
 * relative to its parent window.)
 *
 * Return value: not meaningful, ignore
 */
gint
gdk_window_get_origin (GdkWindow *window,
		       gint      *x,
		       gint      *y)
{
  GdkWindowObject *private;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  private = (GdkWindowObject *) window;
  
  GDK_WINDOW_IMPL_GET_IFACE (private->impl)->get_origin (window, x, y);

  if (x)
    *x += private->abs_x;
  if (y)
    *y += private->abs_y;
  
  return TRUE;
}


/**
 * gdk_window_get_deskrelative_origin:
 * @window: a toplevel #GdkWindow
 * @x: return location for X coordinate
 * @y: return location for Y coordinate
 * 
 * This gets the origin of a #GdkWindow relative to
 * an Enlightenment-window-manager desktop. As long as you don't
 * assume that the user's desktop/workspace covers the entire
 * root window (i.e. you don't assume that the desktop begins
 * at root window coordinate 0,0) this function is not necessary.
 * It's deprecated for that reason.
 * 
 * Return value: not meaningful
 **/
gboolean
gdk_window_get_deskrelative_origin (GdkWindow *window,
				    gint      *x,
				    gint      *y)
{
  GdkWindowObject *private;
  gboolean return_val = FALSE;
  gint tx = 0;
  gint ty = 0;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  private = (GdkWindowObject *) window;
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      return_val = GDK_WINDOW_IMPL_GET_IFACE (private->impl)->get_deskrelative_origin (window, &tx, &ty);
      
      if (x)
	*x = tx + private->abs_x;
      if (y)
	*y = ty + private->abs_y;
    }
  
  return return_val;
}

/**
 * gdk_window_shape_combine_mask:
 * @window: a #GdkWindow
 * @mask: shape mask
 * @x: X position of shape mask with respect to @window
 * @y: Y position of shape mask with respect to @window
 *
 * Applies a shape mask to @window. Pixels in @window corresponding to
 * set bits in the @mask will be visible; pixels in @window
 * corresponding to unset bits in the @mask will be transparent. This
 * gives a non-rectangular window.
 *
 * If @mask is %NULL, the shape mask will be unset, and the @x/@y
 * parameters are not used.
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
gdk_window_shape_combine_mask (GdkWindow *window,
			       GdkBitmap *mask,
			       gint       x,
                               gint       y)
{
  GdkWindowObject *private;
  GdkRegion *region;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;

  if (mask)
    region = _gdk_windowing_get_shape_for_mask (mask);
  else
    region = NULL;

  gdk_window_shape_combine_region (window,
				   region,
				   x, y);

  gdk_region_destroy (region);
}

/**
 * gdk_window_shape_combine_region:
 * @window: a #GdkWindow
 * @shape_region: region of window to be non-transparent
 * @offset_x: X position of @shape_region in @window coordinates
 * @offset_y: Y position of @shape_region in @window coordinates
 *
 * Makes pixels in @window outside @shape_region be transparent,
 * so that the window may be nonrectangular. See also
 * gdk_window_shape_combine_mask() to use a bitmap as the mask.
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
                                 const GdkRegion *shape_region,
                                 gint             offset_x,
                                 gint             offset_y)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  private->shaped = (shape_region != NULL);

  if (private->shape)
    gdk_region_destroy (private->shape);

  if (shape_region)
    {
      private->shape = gdk_region_copy (shape_region);
      gdk_region_offset (private->shape, offset_x, offset_y);      
    }
  else
    private->shape = NULL;
  
  recompute_visible_regions (private, TRUE, FALSE);
}

static void
do_child_shapes (GdkWindow *window,
		 gboolean merge)
{
  GdkWindowObject *private;
  GdkRectangle r;
  GdkRegion *region;

  private = (GdkWindowObject *) window;
  
  r.x = 0;
  r.y = 0;
  r.width = private->width;
  r.height = private->height;
  
  region = gdk_region_rectangle (&r);
  remove_child_area (private, NULL, FALSE, region);

  if (merge && private->shape)
    gdk_region_subtract (region, private->shape);
  
  gdk_window_shape_combine_region (window, region, 0, 0);
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
 * for @window. See gdk_window_shape_combine_mask().
 *
 * This function is distinct from gdk_window_set_child_shapes()
 * because it includes @window's shape mask in the set of shapes to
 * be merged.
 */
void
gdk_window_merge_child_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  do_child_shapes (window, TRUE);
}

/**
 * gdk_window_input_shape_combine_mask:
 * @window: a #GdkWindow
 * @mask: shape mask
 * @x: X position of shape mask with respect to @window
 * @y: Y position of shape mask with respect to @window
 * 
 * Like gdk_window_shape_combine_mask(), but the shape applies
 * only to event handling. Mouse events which happen while
 * the pointer position corresponds to an unset bit in the 
 * mask will be passed on the window below @window.
 *
 * An input shape is typically used with RGBA windows.
 * The alpha channel of the window defines which pixels are 
 * invisible and allows for nicely antialiased borders,
 * and the input shape controls where the window is
 * "clickable".
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
gdk_window_input_shape_combine_mask (GdkWindow *window,
				     GdkBitmap *mask,
				     gint       x,
				     gint       y)
{
  GdkWindowObject *private;
  GdkRegion *region;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;

  if (mask)
    region = _gdk_windowing_get_shape_for_mask (mask);
  else
    region = NULL;

  gdk_window_input_shape_combine_region (window,
					 region,
					 x, y);
  
  gdk_region_destroy (region);
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
 * "clickable".
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
				       const GdkRegion *shape_region,
				       gint             offset_x,
				       gint             offset_y)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (private->input_shape)
    gdk_region_destroy (private->input_shape);

  if (shape_region)
    {
      private->input_shape = gdk_region_copy (shape_region);
      gdk_region_offset (private->shape, offset_x, offset_y);      
    }
  else
    private->input_shape = NULL;

  if (gdk_window_has_impl (private))
    GDK_WINDOW_IMPL_GET_IFACE (private->impl)->input_shape_combine_region ((GdkWindow *)private, private->input_shape, 0, 0);

  /* Pointer may have e.g. moved outside window due to the input mask change */
  _gdk_syntesize_crossing_events_for_geometry_change (window);
}

static void
do_child_input_shapes (GdkWindow *window,
		       gboolean merge)
{
  GdkWindowObject *private;
  GdkRectangle r;
  GdkRegion *region;

  private = (GdkWindowObject *) window;
  
  r.x = 0;
  r.y = 0;
  r.width = private->width;
  r.height = private->height;
  
  region = gdk_region_rectangle (&r);
  remove_child_area (private, NULL, TRUE, region);

  if (merge && private->shape)
    gdk_region_subtract (region, private->shape);
  if (merge && private->input_shape)
    gdk_region_subtract (region, private->input_shape);
  
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
 * for @window. See gdk_window_input_shape_combine_mask().
 *
 * This function is distinct from gdk_window_set_child_input_shapes()
 * because it includes @window's input shape mask in the set of 
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
 * windowing system. Don't worry about it unless you have to.
 *
 * Return value: %TRUE if the server supports static gravity
 */
gboolean
gdk_window_set_static_gravities (GdkWindow *window,
				 gboolean   use_static)
{
  GdkWindowObject *private;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  private = (GdkWindowObject *) window;

  if (gdk_window_has_impl (private))
    return GDK_WINDOW_IMPL_GET_IFACE (private->impl)->set_static_gravities (window, use_static);
  
  return FALSE;
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
 * window. It is the responsibility of the parent's expose handler
 * to manually merge the off-screen content onto the screen in
 * whatever way it sees fit. See <xref linkend="composited-window-example"/>
 * for an example.
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
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkDisplay *display;

  g_return_if_fail (GDK_IS_WINDOW (window));

  composited = composited != FALSE;

  if (private->composited == composited)
    return;

  if (composited)
    gdk_window_set_has_native (window, TRUE);

  display = gdk_drawable_get_display (GDK_DRAWABLE (window));

  if (!gdk_display_supports_composite (display) && composited)
    {
      g_warning ("gdk_window_set_composited called but "
		 "compositing is not supported");
      return;
    }

  _gdk_windowing_window_set_composited (window, composited);

  recompute_visible_regions (private, TRUE, FALSE);

  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_window_invalidate_in_parent (private);
  
  private->composited = composited;
}


static void
remove_redirect_from_children (GdkWindowObject   *private,
                               GdkWindowRedirect *redirect)
{
  GList *l;
  GdkWindowObject *child;

  for (l = private->children; l != NULL; l = l->next)
    {
      child = l->data;

      /* Don't redirect this child if it already has another redirect */
      if (child->redirect == redirect)
	{
	  child->redirect = NULL;
	  remove_redirect_from_children (child, redirect);
	}
    }
}

/**
 * gdk_window_remove_redirection:
 * @window: a #GdkWindow
 *
 * Removes any active redirection started by
 * gdk_window_redirect_to_drawable().
 *
 * Since: 2.14
 **/
void
gdk_window_remove_redirection (GdkWindow *window)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *) window;

  if (private->redirect &&
      private->redirect->redirected == private)
    {
      remove_redirect_from_children (private, private->redirect);
      gdk_window_redirect_free (private->redirect);
      private->redirect = NULL;
    }
}

static void
apply_redirect_to_children (GdkWindowObject   *private,
                            GdkWindowRedirect *redirect)
{
  GList *l;
  GdkWindowObject *child;

  for (l = private->children; l != NULL; l = l->next)
    {
      child = l->data;

      /* Don't redirect this child if it already has another redirect */
      if (!child->redirect)
	{
	  child->redirect = redirect;
	  apply_redirect_to_children (child, redirect);
	}
    }
}

/**
 * gdk_window_redirect_to_drawable:
 * @window: a #GdkWindow
 * @drawable: a #GdkDrawable
 * @src_x: x position in @window
 * @src_y: y position in @window
 * @dest_x: x position in @drawable
 * @dest_y: y position in @drawable
 * @width: width of redirection
 * @height: height of redirection
 *
 * Redirects drawing into @window so that drawing to the
 * window in the rectangle specified by @src_x, @src_y,
 * @width and @height is also drawn into @drawable at
 * @dest_x, @dest_y.
 *
 * Only drawing between gdk_window_begin_paint_region() or
 * gdk_window_begin_paint_rect() and gdk_window_end_paint() is
 * redirected.
 *
 * Redirection is active until gdk_window_remove_redirection()
 * is called.
 *
 * Since: 2.14
 **/
void
gdk_window_redirect_to_drawable (GdkWindow   *window,
				 GdkDrawable *drawable,
				 gint         src_x,
                                 gint         src_y,
				 gint         dest_x,
                                 gint         dest_y,
				 gint         width,
                                 gint         height)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_ROOT);

  private = (GdkWindowObject *) window;

  if (private->redirect)
    gdk_window_remove_redirection (window);

  if (width == -1 || height == -1)
    {
      gint w, h;
      gdk_drawable_get_size (GDK_DRAWABLE (window), &w, &h);
      if (width == -1)
	width = w;
      if (height == -1)
	height = h;
    }
  
  private->redirect = g_new0 (GdkWindowRedirect, 1);
  private->redirect->redirected = private;
  private->redirect->pixmap = g_object_ref (drawable);
  private->redirect->src_x = src_x;
  private->redirect->src_y = src_y;
  private->redirect->dest_x = dest_x;
  private->redirect->dest_y = dest_y;
  private->redirect->width = width;
  private->redirect->height = height;

  apply_redirect_to_children (private, private->redirect);
}

static void
window_get_size_rectangle (GdkWindow    *window,
                           GdkRectangle *rect)
{
  GdkWindowObject *private = (GdkWindowObject *) window;
  
  rect->x = rect->y = 0;
  rect->width = private->width;
  rect->height = private->height;
}

/* Calculates the real clipping region for a window, in window coordinates,
 * taking into account other windows, gc clip region and gc clip mask.
 */
GdkRegion *
_gdk_window_calculate_full_clip_region (GdkWindow *window,
					GdkWindow *base_window,
					gboolean   do_children,
					gint      *base_x_offset,
					gint      *base_y_offset)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  GdkRectangle visible_rect;
  GdkRegion *real_clip_region, *tmpreg;
  gint x_offset, y_offset;
  GdkWindowObject *parentwin, *lastwin;

  if (base_x_offset)
    *base_x_offset = 0;
  if (base_y_offset)
    *base_y_offset = 0;
  
  if (!GDK_WINDOW_IS_MAPPED (window) || private->input_only)
    return gdk_region_new ();

  window_get_size_rectangle (window, &visible_rect);

  /* real_clip_region is in window coordinates */
  real_clip_region = gdk_region_rectangle (&visible_rect);

  x_offset = y_offset = 0;

  lastwin = private;
  if (do_children)
    parentwin = lastwin;
  else
    parentwin = lastwin->parent;
  
  /* Remove the areas of all overlapping windows above parentwin in the hiearachy */
  for (; parentwin != NULL &&
         (parentwin == private || lastwin != (GdkWindowObject*) base_window);
       lastwin = parentwin, parentwin = lastwin->parent)
    {
      GList *cur;
      GdkRectangle real_clip_rect;
      gboolean is_offscreen;

      if (parentwin != private)
	{
	  x_offset += GDK_WINDOW_OBJECT (lastwin)->x;
	  y_offset += GDK_WINDOW_OBJECT (lastwin)->y;
	}

      is_offscreen = gdk_window_is_offscreen (parentwin);

      /* children is ordered in reverse stack order */
      for (cur = parentwin->children;
           cur && cur->data != lastwin;
           cur = cur->next)
	{
	  GdkWindow *child = cur->data;
	  GdkWindowObject *child_private = (GdkWindowObject *)child;

	  if (!GDK_WINDOW_IS_MAPPED (child) || child_private->input_only)
	    continue;

          /* Ignore offscreen children, as they don't draw in their parent and
	   * don't take part in the clipping */
	  if (gdk_window_is_offscreen (child_private))
	    continue;

          window_get_size_rectangle (child, &visible_rect);

	  /* Convert rect to "window" coords */
	  visible_rect.x += child_private->x - x_offset;
	  visible_rect.y += child_private->y - y_offset;
	  
	  /* This shortcut is really necessary for performance when there are a lot of windows */
	  gdk_region_get_clipbox (real_clip_region, &real_clip_rect);
	  if (visible_rect.x >= real_clip_rect.x + real_clip_rect.width ||
	      visible_rect.x + visible_rect.width <= real_clip_rect.x ||
	      visible_rect.y >= real_clip_rect.y + real_clip_rect.height ||
	      visible_rect.y + visible_rect.height <= real_clip_rect.y)
	    continue;
	  
	  tmpreg = gdk_region_rectangle (&visible_rect);
	  gdk_region_subtract (real_clip_region, tmpreg);
	  gdk_region_destroy (tmpreg);
	}
      
      /* Clip to the parent */
      window_get_size_rectangle ((GdkWindow *)parentwin, &visible_rect);
      /* Convert rect to "window" coords */
      visible_rect.x += - x_offset;
      visible_rect.y += - y_offset;
      
      tmpreg = gdk_region_rectangle (&visible_rect);
      gdk_region_intersect (real_clip_region, tmpreg);
      gdk_region_destroy (tmpreg);
    }

  if (base_x_offset)
    *base_x_offset = x_offset;
  if (base_y_offset)
    *base_y_offset = y_offset;

  return real_clip_region;
}

void
_gdk_window_add_damage (GdkWindow *toplevel,
			GdkRegion *damaged_region)
{
  GdkDisplay *display;
  GdkEvent event = { 0, };
  event.expose.type = GDK_DAMAGE;
  event.expose.window = toplevel;
  event.expose.send_event = FALSE;
  event.expose.region = damaged_region;
  gdk_region_get_clipbox (event.expose.region, &event.expose.area);
  display = gdk_drawable_get_display (event.expose.window);
  _gdk_event_queue_append (display, gdk_event_copy (&event));
}

static void
gdk_window_redirect_free (GdkWindowRedirect *redirect)
{
  g_object_unref (redirect->pixmap);
  g_free (redirect);
}

static void
convert_coords_to_child (GdkWindowObject *child,
			 double x, double y,
			 double *child_x, double *child_y)
{
  *child_x = x - child->x;
  *child_y = y - child->y;

  if (child->offscreen_hooks)
    child->offscreen_hooks->from_parent ((GdkWindow *)child, x, y, child_x, child_y);
}

static gboolean
point_in_window (GdkWindowObject *window,
		 double x, double y)
{
  return
    x >= 0 &&  x < window->width &&
    y >= 0 && y < window->height &&
    (window->shape == NULL ||
     gdk_region_point_in (window->shape,
			  x, y)) &&
    (window->input_shape == NULL ||
     gdk_region_point_in (window->input_shape,
			  x, y));
}

static void
convert_toplevel_coords_to_window (GdkWindow *window,
				   gdouble    toplevel_x,
				   gdouble    toplevel_y,
				   gdouble   *window_x,
				   gdouble   *window_y)
{
  GdkWindowObject *private;
  gdouble x, y;
  GList *children, *l;

  private = GDK_WINDOW_OBJECT (window);
  
  x = toplevel_x;
  y = toplevel_y;

  children = NULL;
  while (private->parent != NULL &&
	 (GDK_WINDOW_TYPE (private->parent) != GDK_WINDOW_ROOT))
    {
      children = g_list_prepend (children, private);
      private = private->parent;
    }

  for (l = children; l != NULL; l = l->next)
    convert_coords_to_child (l->data, x, y, &x, &y);
  
  g_list_free (children);
  
  *window_x = x;
  *window_y = y;
}

GdkWindow *
_gdk_window_find_descendant_at (GdkWindow *toplevel,
				double x, double y,
				double *found_x,
				double *found_y)
{
  GdkWindowObject *private, *sub;
  double child_x, child_y;
  GList *l;

  private = (GdkWindowObject *)toplevel;
  
  if (point_in_window (private, x, y))
    {
      do 
	{
	  /* Children is ordered in reverse stack order, i.e. first is topmost */
	  for (l = private->children; l != NULL; l = l->next)
	    {
	      sub = l->data;

              if (!GDK_WINDOW_IS_MAPPED (sub))
                continue;
	      
	      convert_coords_to_child (sub,
				       x, y,
				       &child_x, &child_y);
	      if (point_in_window (sub, child_x, child_y))
		{
		  x = child_x;
		  y = child_y;
		  private = sub;
		  break;
		}
	    }
	}
      while (l != NULL);
    }
  else
    {
      /* Not in window at all */
      private = NULL;
    }

  if (found_x)
    *found_x = x;
  if (found_y)
    *found_y = y;
  
  return (GdkWindow *)private;
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
  GDK_SCROLL_MASK | GDK_BUTTON_PRESS_MASK,/* GDK_SCROLL= 31 */
  0, /* GDK_WINDOW_STATE = 32 */
  0, /* GDK_SETTING = 33 */
  0, /* GDK_OWNER_CHANGE = 34 */
  0, /* GDK_GRAB_BROKEN = 35 */
  0, /* GDK_DAMAGE = 36 */
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
         type == GDK_SCROLL;
}

static gboolean
is_motion_type (GdkEventType type)
{
  return type == GDK_MOTION_NOTIFY ||
         type == GDK_ENTER_NOTIFY ||
         type == GDK_LEAVE_NOTIFY;
}

static GdkWindowObject *
find_common_ancestor (GdkWindowObject *win1,
		      GdkWindowObject *win2)
{
  GdkWindowObject *tmp;
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
      tmp = (GdkWindowObject *)list1->data;
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
    case GDK_NO_EXPOSE:
    case GDK_DELETE:
    case GDK_DESTROY:
    case GDK_EXPOSE:
    default:
      break;
    }

  if (event_in_queue)
    {
    if (before_event)
      _gdk_event_queue_insert_before (gdk_drawable_get_display (window), event_in_queue, event);
    else
      _gdk_event_queue_insert_after (gdk_drawable_get_display (window), event_in_queue, event);
    }
  else
    _gdk_event_queue_append (gdk_drawable_get_display (window), event);

  return event;
}

static void
send_crossing_event (GdkDisplay                 *display,
		     GdkWindowObject            *toplevel,
		     GdkWindowObject            *window,
		     GdkEventType                type,
		     GdkCrossingMode             mode,
		     GdkNotifyType               notify_type,
		     GdkWindow                  *subwindow,
		     gint                        toplevel_x,
		     gint                        toplevel_y,
		     GdkModifierType             mask,
		     guint32                     time_,
		     GdkEvent                   *event_in_queue)
{
  GdkEvent *event;
  guint32 event_mask;

  if (display->pointer_grab.window != NULL &&
      !display->pointer_grab.owner_events &&
      (GdkWindow *)window != display->pointer_grab.window)
    return;
  
  if (type == GDK_LEAVE_NOTIFY)
    event_mask = GDK_LEAVE_NOTIFY_MASK;
  else
    event_mask = GDK_ENTER_NOTIFY_MASK;

  if (window->event_mask & event_mask)
    {
      event = _gdk_make_event ((GdkWindow *)window, type, event_in_queue, TRUE);
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
_gdk_syntesize_crossing_events (GdkDisplay                 *display,
				GdkWindow                  *src,
				GdkWindow                  *dest,
				GdkCrossingMode             mode,
				gint                        toplevel_x,
				gint                        toplevel_y,
				GdkModifierType             mask,
				guint32                     time_,
				GdkEvent                   *event_in_queue)
{
  GdkWindowObject *c;
  GdkWindowObject *win, *last, *next;
  GList *path, *list;
  gboolean non_linear;
  GdkWindowObject *a;
  GdkWindowObject *b;
  GdkWindowObject *toplevel;
  GdkNotifyType notify_type;

  /* TODO: Don't send events to toplevel, as we get those from the windowing system */
  
  a = (GdkWindowObject *)src;
  b = (GdkWindowObject *)dest;
  if (a == b)
    return; /* No crossings generated between src and dest */

  c = find_common_ancestor (a, b);

  non_linear = (c != a) && (c != b);

  if (a) /* There might not be a source (i.e. if no previous pointer_in_window) */
    {
      toplevel = (GdkWindowObject *)gdk_window_get_toplevel ((GdkWindow *)a);
      
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
			   NULL,
			   toplevel_x, toplevel_y,
			   mask, time_,
			   event_in_queue);
     
      if (c != a)
	{
	  if (non_linear)
	    notify_type = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	  else
	    notify_type = GDK_NOTIFY_VIRTUAL;
	  
	  last = a;
	  win = a->parent;
	  while (win != c && GDK_WINDOW_TYPE (win) != GDK_WINDOW_ROOT)
	    {
	      send_crossing_event (display, toplevel,
				   win, GDK_LEAVE_NOTIFY,
				   mode,
				   notify_type,
				   (GdkWindow *)last,
				   toplevel_x, toplevel_y,
				   mask, time_,
				   event_in_queue);
	      
	      last = win;
	      win = win->parent;
	    }
	}
    }

  if (b) /* Might not be a dest, e.g. if we're moving out of the window */
    {
      toplevel = (GdkWindowObject *)gdk_window_get_toplevel ((GdkWindow *)b);
      
      /* Traverse down from c to b */
      if (c != b)
	{
	  path = NULL;
	  win = b->parent;
	  while (win != c && GDK_WINDOW_TYPE (win) != GDK_WINDOW_ROOT)
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
	      win = (GdkWindowObject *)list->data;
	      list = g_list_next (list);
	      if (list)
		next = (GdkWindowObject *)list->data;
	      else
		next = b;

	      send_crossing_event (display, toplevel,
				   win, GDK_ENTER_NOTIFY,
				   mode,
				   notify_type,
				   (GdkWindow *)next,
				   toplevel_x, toplevel_y,
				   mask, time_,
				   event_in_queue);
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
			   toplevel_x, toplevel_y,
			   mask, time_,
			   event_in_queue);
    }
}

static GdkWindow *
get_toplevel (GdkWindow *w)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (w);
  
  while (private->parent != NULL &&
	 (GDK_WINDOW_TYPE (private->parent) != GDK_WINDOW_ROOT))
    private = private->parent;

  return GDK_WINDOW (private);
}

/* Returns the window inside the event window with the pointer in it
 * at the specified coordinates, or NULL if its not in any child of
 * the toplevel. It also takes into account !owner_events grabs.
 */
static GdkWindow *
get_pointer_window (GdkDisplay *display,
		    GdkWindow *event_window,
		    gdouble toplevel_x,
		    gdouble toplevel_y)
{
  GdkWindow *pointer_window;

  if (event_window == display->pointer_info.toplevel_under_pointer)
    pointer_window =
      _gdk_window_find_descendant_at (event_window,
				      toplevel_x, toplevel_y,
				      NULL, NULL);
  else
    pointer_window = NULL;

  if (display->pointer_grab.window != NULL &&
      !display->pointer_grab.owner_events &&
      pointer_window != display->pointer_grab.window)
    pointer_window = NULL;
  
  return pointer_window;
}

void
_gdk_display_set_window_under_pointer (GdkDisplay *display,
				       GdkWindow *window)
{
  GdkWindowObject *private;

  private = (GdkWindowObject *)window;

  if (display->pointer_info.window_under_pointer)
    g_object_unref (display->pointer_info.window_under_pointer);
  display->pointer_info.window_under_pointer = window;
  if (window)
    g_object_ref (window);

  if (window)
    update_cursor (display);
}

void
_gdk_syntesize_crossing_events_for_geometry_change (GdkWindow *changed_window)
{
  GdkDisplay *display;
  GdkWindow *changed_toplevel;
  GdkWindow *new_window_under_pointer;

  changed_toplevel = get_toplevel (changed_window);
  
  display = gdk_drawable_get_display (changed_window);
  if (changed_toplevel == display->pointer_info.toplevel_under_pointer)
    {
      new_window_under_pointer =
	get_pointer_window (display, changed_toplevel,
			    display->pointer_info.toplevel_x,
			    display->pointer_info.toplevel_y);
      if (new_window_under_pointer !=
	  display->pointer_info.window_under_pointer)
	{
	  _gdk_syntesize_crossing_events (display,
					  display->pointer_info.window_under_pointer,
					  new_window_under_pointer,
					  GDK_CROSSING_NORMAL,
					  display->pointer_info.toplevel_x,
					  display->pointer_info.toplevel_y,
					  display->pointer_info.state,
					  GDK_CURRENT_TIME,
					  NULL);
	  _gdk_display_set_window_under_pointer (display, new_window_under_pointer);
	}
    }
}

/* Don't use for crossing events */
static GdkWindow *
get_event_window (GdkDisplay                 *display,
		  GdkWindow                  *pointer_window,
		  GdkEventType                type,
		  GdkModifierType             mask)
{
  guint evmask;
  GdkWindow *grab_window;
  GdkWindowObject *w;

  if ((display->pointer_grab.window != NULL && !display->pointer_grab.owner_events) ||
      (type == GDK_BUTTON_RELEASE && display->pointer_grab.grab_one_pointer_release_event))
    {
      evmask = display->pointer_grab.event_mask;
      evmask = update_evmask_for_button_motion (evmask, mask);

      if (type == GDK_BUTTON_RELEASE &&
	  display->pointer_grab.grab_one_pointer_release_event)
	{
	  grab_window = display->pointer_grab.grab_one_pointer_release_event;
	  display->pointer_grab.grab_one_pointer_release_event = NULL;
	}
      else
	grab_window = display->pointer_grab.window;

      if (evmask & type_masks[type])
	return grab_window;
      else
	return NULL;
    }

  w = (GdkWindowObject *)pointer_window;
  while (w != NULL)
    {
      evmask = w->event_mask;
      evmask = update_evmask_for_button_motion (evmask, mask);

      if (evmask & type_masks[type])
	return (GdkWindow *)w;

      w = w->parent;
    }

  if (display->pointer_grab.window != NULL &&
      display->pointer_grab.owner_events)
    {
      evmask = display->pointer_grab.event_mask;
      evmask = update_evmask_for_button_motion (evmask, mask);

      if (evmask & type_masks[type])
	return display->pointer_grab.window;
      else
	return NULL;
    }

  return NULL;
}

static gboolean
proxy_pointer_event (GdkDisplay                 *display,
		     GdkEvent                   *source_event)
{
  GdkWindow *toplevel_window;
  GdkWindow *pointer_window;
  GdkWindow *cursor_window;
  gboolean sent_motion;
  GdkEvent *event;
  guint state;
  gdouble toplevel_x, toplevel_y;
  guint32 time_;

  toplevel_window = source_event->any.window;
  gdk_event_get_coords (source_event, &toplevel_x, &toplevel_y);
  gdk_event_get_state (source_event, &state);
  time_ = gdk_event_get_time (source_event);

  pointer_window = get_pointer_window (display, toplevel_window, toplevel_x, toplevel_y);
  if (display->pointer_info.window_under_pointer != pointer_window)
    {
      /* Either a toplevel crossing notify that ended up inside a child window,
	 or a motion notify that got into another child window  */
      /* Different than last time, send crossing events */

      _gdk_syntesize_crossing_events (display,
				      display->pointer_info.window_under_pointer,
				      pointer_window,
				      GDK_CROSSING_NORMAL,
				      toplevel_x, toplevel_y,
				      state, time_,
				      source_event);

      _gdk_display_set_window_under_pointer (display, pointer_window);
    }
  else if (source_event->type == GDK_MOTION_NOTIFY)
    {
      GdkWindow *event_win;

      event_win = get_event_window (display,
				    pointer_window,
				    source_event->type,
				    state);
      
      if (event_win)
	{
	  sent_motion = TRUE;
	  event = _gdk_make_event (event_win, GDK_MOTION_NOTIFY, source_event, FALSE);
	  event->motion.time = time_;
	  convert_toplevel_coords_to_window (event_win,
					     toplevel_x, toplevel_y,
					     &event->motion.x, &event->motion.y);
	  event->motion.x_root = source_event->motion.x_root;
	  event->motion.y_root = source_event->motion.y_root;;
	  event->motion.state = state;
	  event->motion.is_hint = FALSE;
	  event->motion.device = NULL;
	  if (source_event && source_event->type == GDK_MOTION_NOTIFY)
	    event->motion.device = source_event->motion.device;
	}
    }

  /* TODO: set cursor from cursor_window, or grab cursor */
  cursor_window = pointer_window;
  if (display->pointer_grab.window &&
      (pointer_window == NULL ||
       !is_parent_of (display->pointer_grab.window, pointer_window)))
    cursor_window = display->pointer_grab.window;
  /* Actually, this should probably happen in synthesize crossing so it works with geometry changes */

  
  /* unlink all move events from queue.
     We handle our own, including our emulated masks. */
  return TRUE;
}

static gboolean
proxy_button_event (GdkEvent *source_event)
{
  GdkWindow *toplevel_window;
  GdkWindow *event_win;
  GdkWindow *pointer_window;
  GdkEvent *event;
  guint state;
  guint32 time_;
  GdkEventType type;
  gdouble toplevel_x, toplevel_y;
  GdkDisplay *display;
  GdkWindowObject *w;

  type = source_event->any.type;
  toplevel_window = source_event->any.window;
  gdk_event_get_coords (source_event, &toplevel_x, &toplevel_y);
  gdk_event_get_state (source_event, &state);
  time_ = gdk_event_get_time (source_event);
  display = gdk_drawable_get_display (source_event->any.window);

  if ((type == GDK_BUTTON_PRESS || type == GDK_SCROLL) &&
      display->pointer_grab.window == source_event->any.window &&
      display->pointer_grab.implicit &&
      !display->pointer_grab.converted_implicit)
    {
      pointer_window =
	_gdk_window_find_descendant_at (toplevel_window,
					toplevel_x, toplevel_y,
					NULL, NULL);

      /* Find the actual event window, its what gets the grab */
      w = (GdkWindowObject *)pointer_window;
      while (w != NULL && w->parent->window_type != GDK_WINDOW_ROOT)
	{
	  if (w->event_mask & GDK_BUTTON_PRESS_MASK)
	    break;
	  w = w->parent;
	}
      pointer_window = (GdkWindow *)w;
      
      if (pointer_window != NULL &&
	  pointer_window != source_event->any.window)
	_gdk_display_set_has_pointer_grab (display,
					   pointer_window,
					   display->pointer_grab.native_window,
					   display->pointer_grab.owner_events,
					   gdk_window_get_events (pointer_window),
					   display->pointer_grab.serial,
					   display->pointer_grab.time,
					   display->pointer_grab.implicit);
      display->pointer_grab.converted_implicit = TRUE;
    }

  pointer_window = get_pointer_window (display, toplevel_window, toplevel_x, toplevel_y);
  
  event_win = get_event_window (display,
				pointer_window,
				type,
				state);

  if (event_win == NULL)
    return TRUE;
  
  event = _gdk_make_event (event_win, type, source_event, FALSE);
  
  switch (type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      event->button.button = source_event->button.button;
      convert_toplevel_coords_to_window (event_win,
					 toplevel_x, toplevel_y,
					 &event->button.x, &event->button.y);
      event->button.x_root = source_event->button.x_root;
      event->button.y_root = source_event->button.y_root;
      event->button.state = state;
      event->button.device = source_event->button.device;
      
      if (type == GDK_BUTTON_PRESS)
	_gdk_event_button_generate (display, event);
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
      return TRUE;
      
    default:
      return FALSE;
    }

  return TRUE; /* Always unlink original, we want to obey the emulated event mask */
}

#ifdef DEBUG_WINDOW_PRINTING
static void
gdk_window_print (GdkWindowObject *window,
		  int indent)
{
  GdkRectangle r;
  
  g_print ("%*s%p: [%s] %d,%d %dx%d", indent, "", window,
	   window->user_data ? g_type_name_from_instance (window->user_data) : "no widget",
	   window->x, window->y,
	   window->width, window->height
	   );

  if (gdk_window_has_impl (window))
    {
      g_print (" impl(0x%lx)", gdk_x11_drawable_get_xid (GDK_DRAWABLE (window)));
    }
  
  if (window->input_only)
    g_print (" input-only");

  if (!gdk_window_is_visible ((GdkWindow *)window))
    g_print (" hidden");
    
  g_print (" abs[%d,%d]",
	   window->abs_x, window->abs_y);
  
  gdk_region_get_clipbox (window->clip_region, &r);
  if (gdk_region_empty (window->clip_region))
    g_print (" clipbox[empty]");
  else
    g_print (" clipbox[%d,%d %dx%d]", r.x, r.y, r.width, r.height);
  
  g_print ("\n");
}


static void
gdk_window_print_tree (GdkWindow *window,
		       int indent,
		       gboolean include_input_only)
{
  GdkWindowObject *private;
  GList *l;

  private = (GdkWindowObject *)window;

  if (private->input_only && !include_input_only)
    return;
  
  gdk_window_print (private, indent);

  for (l = private->children; l != NULL; l = l->next)
    gdk_window_print_tree (l->data, indent + 4, include_input_only);
}

#endif /* DEBUG_WINDOW_PRINTING */

void
_gdk_windowing_got_event (GdkDisplay *display,
			  GList      *event_link,
			  GdkEvent   *event)
{
  GdkWindow *event_window;
  GdkWindowObject *event_private;
  gdouble x, y;
  gboolean unlink_event;

  event_window = event->any.window;
  if (!event_window)
    return;
  
  event_private = GDK_WINDOW_OBJECT (event_window);

#ifdef DEBUG_WINDOW_PRINTING  
  if (event->type == GDK_KEY_PRESS &&
      (event->key.keyval == 0xa7 ||
       event->key.keyval == 0xbd))
    {
      gdk_window_print_tree (event_window, 0,
			     event->key.keyval == 0xbd);
    }
#endif
  
  if (!(is_button_type (event->type) ||
	is_motion_type (event->type)))
    return;

  if (GDK_WINDOW_TYPE (event_private->parent) != GDK_WINDOW_ROOT)
    {
      GEnumValue *event_type_value, *window_type_value;
      
      event_type_value = g_enum_get_value ((GEnumClass *) g_type_class_ref (GDK_TYPE_EVENT_TYPE),
					   event->type);
      window_type_value = g_enum_get_value ((GEnumClass *) g_type_class_ref (GDK_TYPE_WINDOW_TYPE),
					    event_private->window_type);
      
     /* We should only get these events on toplevel windows */
      g_warning ("got unexpected event of type %s on non-toplevel window (gtype %s, type %d)",
		 event_type_value->value_name,
		 window_type_value->value_name,
		 GDK_WINDOW_TYPE (event_window));
      return;
    }

  if ((event->type == GDK_ENTER_NOTIFY ||
       event->type == GDK_LEAVE_NOTIFY) &&
      (event->crossing.mode == GDK_CROSSING_GRAB ||
       event->crossing.mode == GDK_CROSSING_UNGRAB))
    {
      /* We synthesize all crossing events due to grabs are synthesized,
       * so we ignore the native ones. This is partly to get easier non-X
       * portability, and because of problems with race conditions due to
       * the cached state in the client and the real state in the xserver
       * when grabbing.
       */

      /* We ended up in this window after some (perhaps other clients)
	 grab, so update the toplevel_under_window state */
      if (event->type == GDK_ENTER_NOTIFY &&
	  event->crossing.mode == GDK_CROSSING_UNGRAB)
	{
	  if (display->pointer_info.toplevel_under_pointer)
	    g_object_unref (display->pointer_info.toplevel_under_pointer);
	  display->pointer_info.toplevel_under_pointer = g_object_ref (event_window);
	}
      
      unlink_event = TRUE;
      goto out;
    }
  
  /* Store last pointer window and position/state */
  if (event->type == GDK_ENTER_NOTIFY &&
      event->crossing.detail != GDK_NOTIFY_INFERIOR)
    {
      if (display->pointer_info.toplevel_under_pointer)
	g_object_unref (display->pointer_info.toplevel_under_pointer);
      display->pointer_info.toplevel_under_pointer = g_object_ref (event_window);
    }
  else if (event->type == GDK_LEAVE_NOTIFY &&
	   event->crossing.detail != GDK_NOTIFY_INFERIOR &&
	   display->pointer_info.toplevel_under_pointer == event_window)
    {
      if (display->pointer_info.toplevel_under_pointer)
	g_object_unref (display->pointer_info.toplevel_under_pointer);
      display->pointer_info.toplevel_under_pointer = NULL;
    }

  gdk_event_get_coords (event, &x, &y);
  display->pointer_info.toplevel_x = x;
  display->pointer_info.toplevel_y = y;
  gdk_event_get_state (event, &display->pointer_info.state);

  
  unlink_event = FALSE;
  if (is_motion_type (event->type))
    unlink_event = proxy_pointer_event (display,
                                        event);
  else if (is_button_type (event->type))
    unlink_event = proxy_button_event (event);

 out:
  if (unlink_event)
    {
      _gdk_event_queue_remove_link (display, event_link);
      g_list_free_1 (event_link);
      gdk_event_free (event);
    }
}

#define __GDK_WINDOW_C__
#include "gdkaliasdef.c"
