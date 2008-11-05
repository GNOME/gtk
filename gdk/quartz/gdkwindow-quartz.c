/* gdkwindow-quartz.c
 *
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2005-2007 Imendio AB
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

#include "config.h"
#include <Carbon/Carbon.h>

#include "gdk.h"
#include "gdkwindowimpl.h"
#include "gdkprivate-quartz.h"

static gpointer parent_class;

static GSList *update_windows;
static guint   update_idle;

static GSList *main_window_stack;

#define FULLSCREEN_DATA "fullscreen-data"

typedef struct
{
  gint            x, y;
  gint            width, height;
  GdkWMDecoration decor;
} FullscreenSavedGeometry;


static void update_toplevel_order (void);
static void clear_toplevel_order  (void);

static FullscreenSavedGeometry *get_fullscreen_geometry (GdkWindow *window);

#define WINDOW_IS_TOPLEVEL(window)		   \
  (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD && \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN)

static void gdk_window_impl_iface_init (GdkWindowImplIface *iface);

NSView *
gdk_quartz_window_get_nsview (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  return ((GdkWindowImplQuartz *)private->impl)->view;
}

NSWindow *
gdk_quartz_window_get_nswindow (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  return ((GdkWindowImplQuartz *)private->impl)->toplevel;
}

static void
gdk_window_impl_quartz_get_size (GdkDrawable *drawable,
				 gint        *width,
				 gint        *height)
{
  g_return_if_fail (GDK_IS_WINDOW_IMPL_QUARTZ (drawable));

  if (width)
    *width = GDK_WINDOW_IMPL_QUARTZ (drawable)->width;
  if (height)
    *height = GDK_WINDOW_IMPL_QUARTZ (drawable)->height;
}

static CGContextRef
gdk_window_impl_quartz_get_context (GdkDrawable *drawable,
				    gboolean     antialias)
{
  GdkDrawableImplQuartz *drawable_impl = GDK_DRAWABLE_IMPL_QUARTZ (drawable);
  GdkWindowImplQuartz *window_impl = GDK_WINDOW_IMPL_QUARTZ (drawable);
  CGContextRef cg_context;

  if (GDK_WINDOW_DESTROYED (drawable_impl->wrapper))
    return NULL;

  /* Lock focus when not called as part of a drawRect call. This
   * is needed when called from outside "real" expose events, for
   * example for synthesized expose events when realizing windows
   * and for widgets that send fake expose events like the arrow
   * buttons in spinbuttons.
   */
  if (window_impl->in_paint_rect_count == 0)
    {
      if (![window_impl->view lockFocusIfCanDraw])
        return NULL;
    }

  cg_context = [[NSGraphicsContext currentContext] graphicsPort];
  CGContextSaveGState (cg_context);
  CGContextSetAllowsAntialiasing (cg_context, antialias);

  /* We'll emulate the clipping caused by double buffering here */
  if (window_impl->begin_paint_count != 0)
    {
      CGRect rect;
      CGRect *cg_rects;
      GdkRectangle *rects;
      gint n_rects, i;

      gdk_region_get_rectangles (window_impl->paint_clip_region,
                                 &rects, &n_rects);

      if (n_rects == 1)
        cg_rects = &rect;
      else
        cg_rects = g_new (CGRect, n_rects);

      for (i = 0; i < n_rects; i++)
        {
          cg_rects[i].origin.x = rects[i].x;
          cg_rects[i].origin.y = rects[i].y;
          cg_rects[i].size.width = rects[i].width;
          cg_rects[i].size.height = rects[i].height;
        }

      CGContextClipToRects (cg_context, cg_rects, n_rects);

      g_free (rects);
      if (cg_rects != &rect)
        g_free (cg_rects);
    }

  return cg_context;
}

static GdkRegion*
gdk_window_impl_quartz_get_visible_region (GdkDrawable *drawable)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (GDK_DRAWABLE_IMPL_QUARTZ (drawable)->wrapper);
  GdkRectangle rect;
  GdkWindowImplQuartz *impl;
  GList *windows = NULL, *l;

  /* FIXME: The clip rectangle should really be cached
   * and recalculated when the window rectangle changes.
   */
  while (private)
    {
      windows = g_list_prepend (windows, private);

      if (private->parent == GDK_WINDOW_OBJECT (_gdk_root))
	break;

      private = private->parent;
    }

  /* Get rectangle for toplevel window */
  private = GDK_WINDOW_OBJECT (windows->data);
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  
  rect.x = 0;
  rect.y = 0;
  rect.width = impl->width;
  rect.height = impl->height;

  /* Skip toplevel window since we have its rect */
  for (l = windows->next; l; l = l->next)
    {
      private = GDK_WINDOW_OBJECT (l->data);
      impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
      GdkRectangle tmp_rect;

      tmp_rect.x = -MIN (0, private->x - rect.x);
      tmp_rect.y = -MIN (0, private->y - rect.y);
      tmp_rect.width = MIN (rect.width, impl->width + private->x - rect.x) - MAX (0, private->x - rect.x);
      tmp_rect.height = MIN (rect.height, impl->height + private->y - rect.y) - MAX (0, private->y - rect.y);

      rect = tmp_rect;
    }
  
  g_list_free (windows);
  
  return gdk_region_rectangle (&rect);
}

static void
gdk_window_impl_quartz_finalize (GObject *object)
{
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (object);

  if (impl->nscursor)
    [impl->nscursor release];

  if (impl->paint_clip_region)
    gdk_region_destroy (impl->paint_clip_region);

  if (impl->transient_for)
    g_object_unref (impl->transient_for);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_window_impl_quartz_class_init (GdkWindowImplQuartzClass *klass)
{
  GdkDrawableImplQuartzClass *drawable_quartz_class = GDK_DRAWABLE_IMPL_QUARTZ_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_window_impl_quartz_finalize;

  drawable_class->get_size = gdk_window_impl_quartz_get_size;

  drawable_quartz_class->get_context = gdk_window_impl_quartz_get_context;

  /* Visible and clip regions are the same */
  drawable_class->get_clip_region = gdk_window_impl_quartz_get_visible_region;
  drawable_class->get_visible_region = gdk_window_impl_quartz_get_visible_region;
}

static void
gdk_window_impl_quartz_init (GdkWindowImplQuartz *impl)
{
  impl->width = 1;
  impl->height = 1;
  impl->type_hint = GDK_WINDOW_TYPE_HINT_NORMAL;
}

static void
gdk_window_impl_quartz_begin_paint_region (GdkPaintable    *paintable,
					   const GdkRegion *region)
{
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (paintable);
  GdkDrawableImplQuartz *drawable_impl;
  int n_rects;
  GdkRectangle *rects;
  GdkPixmap *bg_pixmap;
  GdkWindow *window;

  drawable_impl = GDK_DRAWABLE_IMPL_QUARTZ (impl);
  bg_pixmap = GDK_WINDOW_OBJECT (drawable_impl->wrapper)->bg_pixmap;

  if (impl->begin_paint_count == 0)
    impl->paint_clip_region = gdk_region_copy (region);
  else
    gdk_region_union (impl->paint_clip_region, region);

  impl->begin_paint_count++;

  if (bg_pixmap == GDK_NO_BG)
    return;

  gdk_region_get_rectangles (region, &rects, &n_rects);

  if (bg_pixmap == NULL)
    {
      CGContextRef cg_context;
      gfloat r, g, b, a;
      gint i;

      cg_context = gdk_quartz_drawable_get_context (GDK_DRAWABLE (impl), FALSE);
      _gdk_quartz_colormap_get_rgba_from_pixel (gdk_drawable_get_colormap (drawable_impl->wrapper),
				      GDK_WINDOW_OBJECT (drawable_impl->wrapper)->bg_color.pixel,
				      &r, &g, &b, &a);
      CGContextSetRGBFillColor (cg_context, r, g, b, a);
 
      for (i = 0; i < n_rects; i++)
        {
          CGContextFillRect (cg_context,
                             CGRectMake (rects[i].x, rects[i].y,
                                         rects[i].width, rects[i].height));
        }

      gdk_quartz_drawable_release_context (GDK_DRAWABLE (impl), cg_context);
    }
  else
    {
      int x, y;
      int x_offset, y_offset;
      int width, height;
      GdkGC *gc;

      x_offset = y_offset = 0;

      window = GDK_WINDOW (drawable_impl->wrapper);
      while (window && bg_pixmap == GDK_PARENT_RELATIVE_BG)
        {
          /* If this window should have the same background as the parent,
           * fetch the parent. (And if the same goes for the parent, fetch
           * the grandparent, etc.)
           */
          x_offset += ((GdkWindowObject *) window)->x;
          y_offset += ((GdkWindowObject *) window)->y;
          window = GDK_WINDOW (((GdkWindowObject *) window)->parent);
          bg_pixmap = ((GdkWindowObject *) window)->bg_pixmap;
        }

      if (bg_pixmap == NULL || bg_pixmap == GDK_NO_BG || bg_pixmap == GDK_PARENT_RELATIVE_BG)
        {
          /* Parent relative background but the parent doesn't have a
           * pixmap.
           */ 
          g_free (rects);
          return;
        }

      /* Note: There should be a CG API to draw tiled images, we might
       * want to look into that for this. 
       */
      gc = gdk_gc_new (GDK_DRAWABLE (impl));

      gdk_drawable_get_size (GDK_DRAWABLE (bg_pixmap), &width, &height);

      x = -x_offset;
      while (x < (rects[0].x + rects[0].width))
        {
          if (x + width >= rects[0].x)
	    {
              y = -y_offset;
              while (y < (rects[0].y + rects[0].height))
                {
                  if (y + height >= rects[0].y)
                    gdk_draw_drawable (GDK_DRAWABLE (impl), gc, bg_pixmap, 0, 0, x, y, width, height);
		  
                  y += height;
                }
            }
          x += width;
        }

      g_object_unref (gc);
    }

  g_free (rects);
}

static void
gdk_window_impl_quartz_end_paint (GdkPaintable *paintable)
{
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (paintable);

  impl->begin_paint_count--;

  if (impl->begin_paint_count == 0)
    {
      gdk_region_destroy (impl->paint_clip_region);
      impl->paint_clip_region = NULL;
    }
}

static void
gdk_window_quartz_process_updates_internal (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *) window;
  GdkWindowImplQuartz *impl = (GdkWindowImplQuartz *) private->impl;
      
  if (private->update_area)
    {
      int i, n_rects;
      GdkRectangle *rects;

      gdk_region_get_rectangles (private->update_area, &rects, &n_rects);

      gdk_region_destroy (private->update_area);
      private->update_area = NULL;

      for (i = 0; i < n_rects; i++) 
	{
	  [impl->view setNeedsDisplayInRect:NSMakeRect (rects[i].x, rects[i].y,
							rects[i].width, rects[i].height)];
	}

      [impl->view displayIfNeeded];

      g_free (rects);
    }
}

static void
gdk_window_quartz_process_all_updates (void)
{
  GSList *old_update_windows = update_windows;
  GSList *tmp_list = update_windows;
  GSList *nswindows;

  update_idle = 0;
  update_windows = NULL;
  nswindows = NULL;

  g_slist_foreach (old_update_windows, (GFunc) g_object_ref, NULL);
  
  GDK_QUARTZ_ALLOC_POOL;

  while (tmp_list)
    {
      GdkWindow *window = tmp_list->data;
      GdkWindow *toplevel;

      /* Only flush each toplevel at most once. */
      toplevel = gdk_window_get_toplevel (window);
      if (toplevel)
        {
          GdkWindowObject *private;
          GdkWindowImplQuartz *impl;
          NSWindow *nswindow;

          private = (GdkWindowObject *) toplevel;
          impl = (GdkWindowImplQuartz *) private->impl;
          nswindow = impl->toplevel;

          if (nswindow && ![nswindow isFlushWindowDisabled]) 
            {
              [nswindow disableFlushWindow];
              nswindows = g_slist_prepend (nswindows, nswindow);
            }
        }

      gdk_window_quartz_process_updates_internal (tmp_list->data);

      g_object_unref (tmp_list->data);
      tmp_list = tmp_list->next;
    }

  tmp_list = nswindows;
  while (tmp_list) 
    {
      NSWindow *nswindow = tmp_list->data;

      [nswindow enableFlushWindow];
      [nswindow flushWindow];

      tmp_list = tmp_list->next;
    }
		    
  GDK_QUARTZ_RELEASE_POOL;

  g_slist_free (old_update_windows);
  g_slist_free (nswindows);
}

static gboolean
gdk_window_quartz_update_idle (gpointer data)
{
  gdk_window_quartz_process_all_updates ();

  return FALSE;
}

static void
gdk_window_impl_quartz_invalidate_maybe_recurse (GdkPaintable    *paintable,
						 const GdkRegion *region,
						 gboolean        (*child_func) (GdkWindow *, gpointer),
						 gpointer         user_data)
{
  GdkWindowImplQuartz *window_impl = GDK_WINDOW_IMPL_QUARTZ (paintable);
  GdkDrawableImplQuartz *drawable_impl = (GdkDrawableImplQuartz *) window_impl;
  GdkWindow *window = (GdkWindow *) drawable_impl->wrapper;
  GdkWindowObject *private = (GdkWindowObject *) window;
  GdkRegion *visible_region;

  visible_region = gdk_drawable_get_visible_region (window);
  gdk_region_intersect (visible_region, region);

  if (private->update_area)
    {
      gdk_region_union (private->update_area, visible_region);
      gdk_region_destroy (visible_region);
    }
  else
    {
      /* FIXME: When the update_window/update_area handling is abstracted in
       * some way, we can remove this check. Currently it might be cleared
       * in the generic code without us knowing, see bug #530801.
       */
      if (!g_slist_find (update_windows, window))
        update_windows = g_slist_prepend (update_windows, window);
      private->update_area = visible_region;

      if (update_idle == 0)
        update_idle = gdk_threads_add_idle_full (GDK_PRIORITY_REDRAW,
                                                 gdk_window_quartz_update_idle, NULL, NULL);
    }
}

static void
gdk_window_impl_quartz_process_updates (GdkPaintable *paintable,
					gboolean      update_children)
{
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (paintable);
  GdkDrawableImplQuartz *drawable_impl = (GdkDrawableImplQuartz *) impl;
  GdkWindowObject *private = (GdkWindowObject *) drawable_impl->wrapper;

  if (private->update_area)
    {
      GDK_QUARTZ_ALLOC_POOL;
      gdk_window_quartz_process_updates_internal ((GdkWindow *) private);
      update_windows = g_slist_remove (update_windows, private);
      GDK_QUARTZ_RELEASE_POOL;
    }
}

static void
gdk_window_impl_quartz_paintable_init (GdkPaintableIface *iface)
{
  iface->begin_paint_region = gdk_window_impl_quartz_begin_paint_region;
  iface->end_paint = gdk_window_impl_quartz_end_paint;

  iface->invalidate_maybe_recurse = gdk_window_impl_quartz_invalidate_maybe_recurse;
  iface->process_updates = gdk_window_impl_quartz_process_updates;
}

GType
_gdk_window_impl_quartz_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
	{
	  sizeof (GdkWindowImplQuartzClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  (GClassInitFunc) gdk_window_impl_quartz_class_init,
	  NULL,           /* class_finalize */
	  NULL,           /* class_data */
	  sizeof (GdkWindowImplQuartz),
	  0,              /* n_preallocs */
	  (GInstanceInitFunc) gdk_window_impl_quartz_init,
	};

      const GInterfaceInfo paintable_info = 
	{
	  (GInterfaceInitFunc) gdk_window_impl_quartz_paintable_init,
	  NULL,
	  NULL
	};

      const GInterfaceInfo window_impl_info = 
	{
	  (GInterfaceInitFunc) gdk_window_impl_iface_init,
	  NULL,
	  NULL
	};

      object_type = g_type_register_static (GDK_TYPE_DRAWABLE_IMPL_QUARTZ,
                                            "GdkWindowImplQuartz",
                                            &object_info, 0);
      g_type_add_interface_static (object_type,
				   GDK_TYPE_PAINTABLE,
				   &paintable_info);
      g_type_add_interface_static (object_type,
				   GDK_TYPE_WINDOW_IMPL,
				   &window_impl_info);
    }

  return object_type;
}

GType
_gdk_window_impl_get_type (void)
{
  return _gdk_window_impl_quartz_get_type ();
}

static const gchar *
get_default_title (void)
{
  const char *title;

  title = g_get_application_name ();
  if (!title)
    title = g_get_prgname ();

  return title;
}

static void
get_ancestor_coordinates_from_child (GdkWindow *child_window,
				     gint       child_x,
				     gint       child_y,
				     GdkWindow *ancestor_window, 
				     gint      *ancestor_x, 
				     gint      *ancestor_y)
{
  GdkWindowObject *child_private = GDK_WINDOW_OBJECT (child_window);
  GdkWindowObject *ancestor_private = GDK_WINDOW_OBJECT (ancestor_window);

  while (child_private != ancestor_private)
    {
      child_x += child_private->x;
      child_y += child_private->y;

      child_private = child_private->parent;
    }

  *ancestor_x = child_x;
  *ancestor_y = child_y;
}

void
_gdk_quartz_window_debug_highlight (GdkWindow *window, gint number)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  gint x, y;
  GdkWindow *toplevel;
  gint tx, ty;
  static NSWindow *debug_window[10];
  static NSRect old_rect[10];
  NSRect rect;
  NSColor *color;

  g_return_if_fail (number >= 0 && number <= 9);

  if (window == _gdk_root)
    return;

  if (window == NULL)
    {
      if (debug_window[number])
        [debug_window[number] close];
      debug_window[number] = NULL;

      return;
    }

  toplevel = gdk_window_get_toplevel (window);
  get_ancestor_coordinates_from_child (window, 0, 0, toplevel, &x, &y);

  gdk_window_get_origin (toplevel, &tx, &ty);
  x += tx;
  y += ty;

  rect = NSMakeRect (x,
                     _gdk_quartz_window_get_inverted_screen_y (y + impl->height),
                     impl->width, impl->height);

  if (debug_window[number] && NSEqualRects (rect, old_rect[number]))
    return;

  old_rect[number] = rect;

  if (debug_window[number])
    [debug_window[number] close];

  debug_window[number] = [[NSWindow alloc] initWithContentRect:rect
                                                     styleMask:NSBorderlessWindowMask
			                               backing:NSBackingStoreBuffered
			                                 defer:NO];

  switch (number)
    {
    case 0:
      color = [NSColor redColor];
      break;
    case 1:
      color = [NSColor blueColor];
      break;
    case 2:
      color = [NSColor greenColor];
      break;
    case 3:
      color = [NSColor yellowColor];
      break;
    case 4:
      color = [NSColor brownColor];
      break;
    case 5:
      color = [NSColor purpleColor];
      break;
    default:
      color = [NSColor blackColor];
      break;
    }

  [debug_window[number] setBackgroundColor:color];
  [debug_window[number] setAlphaValue:0.4];
  [debug_window[number] setOpaque:NO];
  [debug_window[number] setReleasedWhenClosed:YES];
  [debug_window[number] setIgnoresMouseEvents:YES];
  [debug_window[number] setLevel:NSFloatingWindowLevel];

  [debug_window[number] orderFront:nil];
}

gboolean
_gdk_quartz_window_is_ancestor (GdkWindow *ancestor,
                                GdkWindow *window)
{
  if (ancestor == NULL || window == NULL)
    return FALSE;

  return (gdk_window_get_parent (window) == ancestor ||
          _gdk_quartz_window_is_ancestor (ancestor, 
                                          gdk_window_get_parent (window)));
}

/* FIXME: It would be nice to have one function that takes an NSPoint
 * and flips the coords for any window.
 */
gint 
_gdk_quartz_window_get_inverted_screen_y (gint y)
{
  NSRect rect = [[NSScreen mainScreen] frame];

  return rect.size.height - y;
}

static GdkWindow *
find_child_window_helper (GdkWindow *window,
			  gint       x,
			  gint       y,
			  gint       x_offset,
			  gint       y_offset)
{
  GdkWindowImplQuartz *impl;
  GList *l;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);

  if (window == _gdk_root)
    update_toplevel_order ();

  for (l = impl->sorted_children; l; l = l->next)
    {
      GdkWindowObject *child_private = l->data;
      GdkWindowImplQuartz *child_impl = GDK_WINDOW_IMPL_QUARTZ (child_private->impl);
      int temp_x, temp_y;

      if (!GDK_WINDOW_IS_MAPPED (child_private))
	continue;

      temp_x = x_offset + child_private->x;
      temp_y = y_offset + child_private->y;

      /* Special-case the root window. We have to include the title
       * bar in the checks, otherwise the window below the title bar
       * will be found i.e. events punch through. (If we can find a
       * better way to deal with the events in gdkevents-quartz, this
       * might not be needed.)
       */
      if (window == _gdk_root)
        {
          NSRect frame = NSMakeRect (0, 0, 100, 100);
          NSRect content;
          int mask;
          int titlebar_height;

          mask = [child_impl->toplevel styleMask];

          /* Get the title bar height. */
          content = [NSWindow contentRectForFrameRect:frame
                                            styleMask:mask];
          titlebar_height = frame.size.height - content.size.height;

          if (titlebar_height > 0 &&
              x >= temp_x && y >= temp_y - titlebar_height &&
              x < temp_x + child_impl->width && y < temp_y)
            {
              /* The root means "unknown" i.e. a window not managed by
               * GDK.
               */
              return (GdkWindow *)_gdk_root;
            }
        }

      if (x >= temp_x && y >= temp_y &&
	  x < temp_x + child_impl->width && y < temp_y + child_impl->height)
	{
	  /* Look for child windows. */
	  return find_child_window_helper (l->data,
					   x, y,
					   temp_x, temp_y);
	}
    }
  
  return window;
}

/* Given a GdkWindow and coordinates relative to it, returns the
 * innermost subwindow that contains the point. If the coordinates are
 * outside the passed in window, NULL is returned.
 */
GdkWindow *
_gdk_quartz_window_find_child (GdkWindow *window,
			       gint       x,
			       gint       y)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if (x >= 0 && y >= 0 && x < impl->width && y < impl->height)
    return find_child_window_helper (window, x, y, 0, 0);

  return NULL;
}

void
_gdk_quartz_window_did_become_main (GdkWindow *window)
{
  main_window_stack = g_slist_remove (main_window_stack, window);

  if (GDK_WINDOW_OBJECT (window)->window_type != GDK_WINDOW_TEMP)
    main_window_stack = g_slist_prepend (main_window_stack, window);

  clear_toplevel_order ();
}

void
_gdk_quartz_window_did_resign_main (GdkWindow *window)
{
  GdkWindow *new_window = NULL;

  if (main_window_stack)
    new_window = main_window_stack->data;
  else
    {
      GList *toplevels;

      toplevels = gdk_window_get_toplevels ();
      if (toplevels)
        new_window = toplevels->data;
      g_list_free (toplevels);
    }

  if (new_window &&
      new_window != window &&
      GDK_WINDOW_IS_MAPPED (new_window) &&
      GDK_WINDOW_OBJECT (new_window)->window_type != GDK_WINDOW_TEMP)
    {
      GdkWindowObject *private = (GdkWindowObject *) new_window;
      GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

      [impl->toplevel makeKeyAndOrderFront:impl->toplevel];
    }

  clear_toplevel_order ();
}

GdkWindow *
_gdk_window_new (GdkWindow     *parent,
                 GdkWindowAttr *attributes,
                 gint           attributes_mask)
{
  GdkWindow *window;
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;
  GdkDrawableImplQuartz *draw_impl;
  GdkVisual *visual;
  GdkWindowImplQuartz *parent_impl;

  if (parent && GDK_WINDOW_DESTROYED (parent))
    return NULL;

  GDK_QUARTZ_ALLOC_POOL;

  if (!parent)
    parent = _gdk_root;

  window = g_object_new (GDK_TYPE_WINDOW, NULL);

  private = (GdkWindowObject *)window;
  private->impl = g_object_new (_gdk_window_impl_get_type (), NULL);

  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_QUARTZ (private->impl);
  draw_impl->wrapper = GDK_DRAWABLE (window);

  private->parent = (GdkWindowObject *)parent;
  parent_impl = GDK_WINDOW_IMPL_QUARTZ (private->parent->impl);

  private->accept_focus = TRUE;
  private->focus_on_map = TRUE;

  if (attributes_mask & GDK_WA_X)
    private->x = attributes->x;
  else
    private->x = 0;
  
  if (attributes_mask & GDK_WA_Y)
    private->y = attributes->y;
  else if (attributes_mask & GDK_WA_X)
    private->y = 100;
  else
    private->y = 0;

  private->event_mask = attributes->event_mask;

  impl->width = attributes->width > 1 ? attributes->width : 1;
  impl->height = attributes->height > 1 ? attributes->height : 1;

  if (attributes_mask & GDK_WA_VISUAL)
    visual = attributes->visual;
  else
    visual = gdk_screen_get_system_visual (_gdk_screen);

  if (attributes->wclass == GDK_INPUT_ONLY)
    {
      /* Backwards compatiblity - we've always ignored
       * attributes->window_type for input-only windows
       * before
       */
      if (parent == _gdk_root)
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
	{
	  g_warning (G_STRLOC "Toplevel windows must be created as children of\n"
		     "of a window of type GDK_WINDOW_ROOT or GDK_WINDOW_FOREIGN");
	}
    case GDK_WINDOW_CHILD:
      break;
    default:
      g_warning (G_STRLOC "cannot make windows of type %d", private->window_type);
      GDK_QUARTZ_RELEASE_POOL;
      return NULL;
    }

  if (attributes->wclass == GDK_INPUT_OUTPUT)
    {
      private->input_only = FALSE;
      private->depth = visual->depth;

      if (attributes_mask & GDK_WA_COLORMAP)
	{
	  draw_impl->colormap = attributes->colormap;
	  g_object_ref (attributes->colormap);
	}
      else
	{
	  if (visual == gdk_screen_get_system_visual (_gdk_screen))
	    {
	      draw_impl->colormap = gdk_screen_get_system_colormap (_gdk_screen);
	      g_object_ref (draw_impl->colormap);
	    }
	  else if (visual == gdk_screen_get_rgba_visual (_gdk_screen))
	    {
	      draw_impl->colormap = gdk_screen_get_rgba_colormap (_gdk_screen);
	      g_object_ref (draw_impl->colormap);
	    }
	  else
	    {
	      draw_impl->colormap = gdk_colormap_new (visual, FALSE);
	    }
	}

      private->bg_color.pixel = 0;
      private->bg_color.red = private->bg_color.green = private->bg_color.blue = 0;
    }
  else
    {
      private->depth = 0;
      private->input_only = TRUE;
      draw_impl->colormap = gdk_screen_get_system_colormap (_gdk_screen);
      g_object_ref (draw_impl->colormap);
    }

  private->parent->children = g_list_prepend (private->parent->children, window);

  /* Maintain the z-ordered list of children. */
  if (parent != _gdk_root)
    parent_impl->sorted_children = g_list_prepend (parent_impl->sorted_children, window);
  else
    clear_toplevel_order ();

  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));

  switch (attributes->window_type) 
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
      {
        NSRect content_rect;
        int style_mask;
        const char *title;

        /* Big hack: We start out outside the screen and move the
         * window in before showing it. This makes the initial
         * MouseEntered event work if the window ends up right under
         * the mouse pointer, bad quartz.
         */
        content_rect = NSMakeRect (-500 - impl->width, -500 - impl->height,
                                   impl->width, impl->height);

        if (attributes->window_type == GDK_WINDOW_TEMP ||
            attributes->type_hint == GDK_WINDOW_TYPE_HINT_SPLASHSCREEN)
          {
            style_mask = NSBorderlessWindowMask;
          }
        else
          {
            style_mask = (NSTitledWindowMask |
                          NSClosableWindowMask |
                          NSMiniaturizableWindowMask |
                          NSResizableWindowMask);
          }

	impl->toplevel = [[GdkQuartzWindow alloc] initWithContentRect:content_rect 
			                                    styleMask:style_mask
			                                      backing:NSBackingStoreBuffered
			                                        defer:NO];

	if (attributes_mask & GDK_WA_TITLE)
	  title = attributes->title;
	else
	  title = get_default_title ();

	gdk_window_set_title (window, title);
  
	if (draw_impl->colormap == gdk_screen_get_rgba_colormap (_gdk_screen))
	  {
	    [impl->toplevel setOpaque:NO];
	    [impl->toplevel setBackgroundColor:[NSColor clearColor]];
	  }

	impl->view = [[GdkQuartzView alloc] initWithFrame:content_rect];
	[impl->view setGdkWindow:window];
	[impl->toplevel setContentView:impl->view];
      }
      break;

    case GDK_WINDOW_CHILD:
      {
	GdkWindowImplQuartz *parent_impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (parent)->impl);

	if (attributes->wclass == GDK_INPUT_OUTPUT)
	  {
	    NSRect frame_rect = NSMakeRect (private->x, private->y, impl->width, impl->height);
	
	    impl->view = [[GdkQuartzView alloc] initWithFrame:frame_rect];
	    
	    [impl->view setGdkWindow:window];

	    /* GdkWindows should be hidden by default */
	    [impl->view setHidden:YES];
	    [parent_impl->view addSubview:impl->view];
	  }
      }
      break;

    default:
      g_assert_not_reached ();
    }

  GDK_QUARTZ_RELEASE_POOL;

  if (attributes_mask & GDK_WA_TYPE_HINT)
    gdk_window_set_type_hint (window, attributes->type_hint);

  return window;
}

void
_gdk_windowing_window_init (void)
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;
  GdkDrawableImplQuartz *drawable_impl;
  NSRect rect;

  g_assert (_gdk_root == NULL);

  _gdk_root = g_object_new (GDK_TYPE_WINDOW, NULL);

  private = (GdkWindowObject *)_gdk_root;
  private->impl = g_object_new (_gdk_window_impl_get_type (), NULL);

  /* Note: This needs to be reworked for multi-screen support. */
  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (_gdk_root)->impl);
  rect = [[NSScreen mainScreen] frame];
  impl->width = rect.size.width;
  impl->height = rect.size.height;

  private->state = 0; /* We don't want GDK_WINDOW_STATE_WITHDRAWN here */
  private->window_type = GDK_WINDOW_ROOT;
  private->depth = 24;

  drawable_impl = GDK_DRAWABLE_IMPL_QUARTZ (private->impl);
  
  drawable_impl->wrapper = GDK_DRAWABLE (private);
  drawable_impl->colormap = gdk_screen_get_system_colormap (_gdk_screen);
  g_object_ref (drawable_impl->colormap);
}

void
_gdk_windowing_window_destroy (GdkWindow *window,
			       gboolean   recursing,
			       gboolean   foreign_destroy)
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;
  GdkWindowObject *parent;
  GdkWindow *mouse_window;

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  update_windows = g_slist_remove (update_windows, window);
  main_window_stack = g_slist_remove (main_window_stack, window);

  g_list_free (impl->sorted_children);
  impl->sorted_children = NULL;

  parent = private->parent;
  if (parent)
    {
      GdkWindowImplQuartz *parent_impl = GDK_WINDOW_IMPL_QUARTZ (parent->impl);

      parent_impl->sorted_children = g_list_remove (parent_impl->sorted_children, window);
    }

  /* If the destroyed window was targeted for a pointer or keyboard
   * grab, release the grab.
   */
  if (window == _gdk_quartz_pointer_grab_window)
    gdk_pointer_ungrab (0);

  if (window == _gdk_quartz_keyboard_grab_window)
    gdk_keyboard_ungrab (0);

  _gdk_quartz_drawable_finish (GDK_DRAWABLE (impl));

  mouse_window = _gdk_quartz_events_get_mouse_window (FALSE);
  if (window == mouse_window ||
      _gdk_quartz_window_is_ancestor (window, mouse_window))
    _gdk_quartz_events_update_mouse_window (_gdk_root);

  if (!recursing && !foreign_destroy)
    {
      GDK_QUARTZ_ALLOC_POOL;

      if (impl->toplevel)
	[impl->toplevel close];
      else if (impl->view)
	[impl->view removeFromSuperview];

      GDK_QUARTZ_RELEASE_POOL;
    }
}

void
_gdk_windowing_window_destroy_foreign (GdkWindow *window)
{
  /* Foreign windows aren't supported in OSX. */
}

static gboolean
all_parents_shown (GdkWindowObject *private)
{
  while (GDK_WINDOW_IS_MAPPED (private))
    {
      if (private->parent)
	private = (GdkWindowObject *)private->parent;
      else
	return TRUE;
    }

  return FALSE;
}

/* Note: the raise argument is not really used, it doesn't seem
 * possible to show a window without raising it?
 */
static void
gdk_window_quartz_show (GdkWindow *window,
                        gboolean   raise)
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;
  gboolean focus_on_map;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_QUARTZ_ALLOC_POOL;

  private = (GdkWindowObject *)window;
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if (!GDK_WINDOW_IS_MAPPED (window))
    focus_on_map = private->focus_on_map;
  else
    focus_on_map = TRUE;

  if (impl->toplevel)
    {
      gboolean make_key;

      /* Move the window into place, to guarantee that we get the
       * initial MouseEntered event.
       */
      make_key = (private->accept_focus && focus_on_map && raise && 
                  private->window_type != GDK_WINDOW_TEMP);

      [(GdkQuartzWindow*)impl->toplevel showAndMakeKey:make_key];
      clear_toplevel_order ();
    }
  else
    {
      [impl->view setHidden:NO];
    }

  [impl->view setNeedsDisplay:YES];

  if (all_parents_shown (private->parent))
    _gdk_quartz_events_send_map_events (window);

  gdk_synthesize_window_state (window, GDK_WINDOW_STATE_WITHDRAWN, 0);

  if (private->state & GDK_WINDOW_STATE_MAXIMIZED)
    gdk_window_maximize (window);

  if (private->state & GDK_WINDOW_STATE_ICONIFIED)
    gdk_window_iconify (window);

  if (impl->transient_for && !GDK_WINDOW_DESTROYED (impl->transient_for))
    _gdk_quartz_window_attach_to_parent (window);

  /* Create a crossing event for windows that pop up under the mouse. Part
   * of the workarounds for problems with the tracking rect API.
   */
  if (impl->toplevel)
    _gdk_quartz_events_trigger_crossing_events (TRUE);

  GDK_QUARTZ_RELEASE_POOL;
}

/* Temporarily unsets the parent window, if the window is a
 * transient. 
 */
void
_gdk_quartz_window_detach_from_parent (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);
  
  g_return_if_fail (impl->toplevel != NULL);

  if (impl->transient_for && !GDK_WINDOW_DESTROYED (impl->transient_for))
    {
      GdkWindowImplQuartz *parent_impl;

      parent_impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (impl->transient_for)->impl);
      [parent_impl->toplevel removeChildWindow:impl->toplevel];
      clear_toplevel_order ();
    }
}

/* Re-sets the parent window, if the window is a transient. */
void
_gdk_quartz_window_attach_to_parent (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);
  
  g_return_if_fail (impl->toplevel != NULL);

  if (impl->transient_for && !GDK_WINDOW_DESTROYED (impl->transient_for))
    {
      GdkWindowImplQuartz *parent_impl;

      parent_impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (impl->transient_for)->impl);
      [parent_impl->toplevel addChildWindow:impl->toplevel ordered:NSWindowAbove];
      clear_toplevel_order ();
    }
}

void
gdk_window_quartz_hide (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl;
  GdkWindow *mouse_window;

  /* Make sure we're not stuck in fullscreen mode. */
  if (get_fullscreen_geometry (window))
    SetSystemUIMode (kUIModeNormal, 0);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  mouse_window = _gdk_quartz_events_get_mouse_window (FALSE);
  if (window == mouse_window || 
      _gdk_quartz_window_is_ancestor (window, mouse_window))
    _gdk_quartz_events_update_mouse_window (_gdk_root);

  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_synthesize_window_state (window,
				 0,
				 GDK_WINDOW_STATE_WITHDRAWN);

  _gdk_window_clear_update_area (window);

  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if (impl->toplevel) 
    {
     /* Update main window. */
      main_window_stack = g_slist_remove (main_window_stack, window);
      if ([NSApp mainWindow] == impl->toplevel)
        _gdk_quartz_window_did_resign_main (window);

      if (impl->transient_for)
        _gdk_quartz_window_detach_from_parent (window);

      [(GdkQuartzWindow*)impl->toplevel hide];
    }
  else if (impl->view)
    {
      [impl->view setHidden:YES];
    }

  if (window == _gdk_quartz_pointer_grab_window)
    gdk_pointer_ungrab (0);

  if (window == _gdk_quartz_keyboard_grab_window)
    gdk_keyboard_ungrab (0);
}

void
gdk_window_quartz_withdraw (GdkWindow *window)
{
  gdk_window_hide (window);
}

static void
move_resize_window_internal (GdkWindow *window,
			     gint       x,
			     gint       y,
			     gint       width,
			     gint       height)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl;
  GdkRectangle old_visible;
  GdkRectangle new_visible;
  GdkRectangle scroll_rect;
  GdkRegion *old_region;
  GdkRegion *expose_region;
  NSSize delta;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if ((x == -1 || (x == private->x)) &&
      (y == -1 || (y == private->y)) &&
      (width == -1 || (width == impl->width)) &&
      (height == -1 || (height == impl->height)))
    {
      return;
    }

  if (!impl->toplevel)
    {
      /* The previously visible area of this window in a coordinate
       * system rooted at the origin of this window.
       */
      old_visible.x = -private->x;
      old_visible.y = -private->y;

      gdk_window_get_size (GDK_DRAWABLE (private->parent), 
                           &old_visible.width, 
                           &old_visible.height);
    }

  if (x != -1)
    {
      delta.width = x - private->x;
      private->x = x;
    }
  else
    {
      delta.width = 0;
    }

  if (y != -1)
    {
      delta.height = y - private->y;
      private->y = y;
    }
  else
    {
      delta.height = 0;
    }

  if (width != -1)
    impl->width = width;

  if (height != -1)
    impl->height = height;

  GDK_QUARTZ_ALLOC_POOL;

  if (impl->toplevel)
    {
      NSRect content_rect;
      NSRect frame_rect;

      /* We don't update the NSWindow while unmapped, since we move windows
       * off-screen when hiding in order for MouseEntered to be triggered
       * reliably when showing windows and they appear under the mouse.
       */
      if (GDK_WINDOW_IS_MAPPED (window))
        {
          content_rect =  NSMakeRect (private->x,
                                      _gdk_quartz_window_get_inverted_screen_y (private->y + impl->height),
                                      impl->width, impl->height);

          frame_rect = [impl->toplevel frameRectForContentRect:content_rect];
          [impl->toplevel setFrame:frame_rect display:YES];
        }
    }
  else 
    {
      if (!private->input_only)
        {
          NSRect nsrect;

          nsrect = NSMakeRect (private->x, private->y, impl->width, impl->height);

          /* The newly visible area of this window in a coordinate
           * system rooted at the origin of this window.
           */
          new_visible.x = -private->x;
          new_visible.y = -private->y;
          new_visible.width = old_visible.width;   /* parent has not changed size */
          new_visible.height = old_visible.height; /* parent has not changed size */

          expose_region = gdk_region_rectangle (&new_visible);
          old_region = gdk_region_rectangle (&old_visible);
          gdk_region_subtract (expose_region, old_region);

          /* Determine what (if any) part of the previously visible
           * part of the window can be copied without a redraw
           */
          scroll_rect = old_visible;
          scroll_rect.x -= delta.width;
          scroll_rect.y -= delta.height;
          gdk_rectangle_intersect (&scroll_rect, &old_visible, &scroll_rect);

          if (!gdk_region_empty (expose_region))
            {
              GdkRectangle* rects;
              gint n_rects;
              gint n;

              if (scroll_rect.width != 0 && scroll_rect.height != 0)
                {
                  [impl->view scrollRect:NSMakeRect (scroll_rect.x,
                                                     scroll_rect.y,
                                                     scroll_rect.width,
                                                     scroll_rect.height)
			              by:delta];
                }

              [impl->view setFrame:nsrect];

              gdk_region_get_rectangles (expose_region, &rects, &n_rects);

              for (n = 0; n < n_rects; ++n)
                {
                  [impl->view setNeedsDisplayInRect:NSMakeRect (rects[n].x,
                                                                rects[n].y,
                                                                rects[n].width,
                                                                rects[n].height)];
                }

              g_free (rects);
            }
          else
            {
              [impl->view setFrame:nsrect];
              [impl->view setNeedsDisplay:YES];
            }

          gdk_region_destroy (expose_region);
          gdk_region_destroy (old_region);
        }
    }

  GDK_QUARTZ_RELEASE_POOL;
}

static inline void
window_quartz_move (GdkWindow *window,
                    gint       x,
                    gint       y)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (((GdkWindowObject *)window)->state & GDK_WINDOW_STATE_FULLSCREEN)
    return;

  move_resize_window_internal (window, x, y, -1, -1);
}

static inline void
window_quartz_resize (GdkWindow *window,
                      gint       width,
                      gint       height)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (((GdkWindowObject *)window)->state & GDK_WINDOW_STATE_FULLSCREEN)
    return;

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  move_resize_window_internal (window, -1, -1, width, height);
}

static inline void
window_quartz_move_resize (GdkWindow *window,
                           gint       x,
                           gint       y,
                           gint       width,
                           gint       height)
{
  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  move_resize_window_internal (window, x, y, width, height);
}

static void
gdk_window_quartz_move_resize (GdkWindow *window,
                               gboolean   with_move,
                               gint       x,
                               gint       y,
                               gint       width,
                               gint       height)
{
  if (with_move && (width < 0 && height < 0))
    window_quartz_move (window, x, y);
  else
    {
      if (with_move)
        window_quartz_move_resize (window, x, y, width, height);
      else
        window_quartz_resize (window, width, height);
    }
}

static gboolean
gdk_window_quartz_reparent (GdkWindow *window,
                            GdkWindow *new_parent,
                            gint       x,
                            gint       y)
{
  GdkWindowObject *private, *old_parent_private, *new_parent_private;
  GdkWindowImplQuartz *impl, *old_parent_impl, *new_parent_impl;
  NSView *view, *new_parent_view;

  if (!new_parent || new_parent == _gdk_root)
    {
      /* Could be added, just needs implementing. */
      g_warning ("Reparenting to root window is not supported yet in the Mac OS X backend");
      return FALSE;
    }

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  view = impl->view;

  new_parent_private = GDK_WINDOW_OBJECT (new_parent);
  new_parent_impl = GDK_WINDOW_IMPL_QUARTZ (new_parent_private->impl);
  new_parent_view = new_parent_impl->view;

  old_parent_private = GDK_WINDOW_OBJECT (private->parent);
  old_parent_impl = GDK_WINDOW_IMPL_QUARTZ (old_parent_private->impl);

  [view retain];

  [view removeFromSuperview];
  [new_parent_view addSubview:view];

  [view release];

  private->x = x;
  private->y = y;
  private->parent = (GdkWindowObject *)new_parent;

  if (old_parent_private)
    {
      old_parent_private->children = g_list_remove (old_parent_private->children, window);
      old_parent_impl->sorted_children = g_list_remove (old_parent_impl->sorted_children, window);
    }

  new_parent_private->children = g_list_prepend (new_parent_private->children, window);
  new_parent_impl->sorted_children = g_list_prepend (new_parent_impl->sorted_children, window);

  return TRUE;
}

static void
gdk_window_quartz_clear_area (GdkWindow *window,
                              gint       x,
                              gint       y,
                              gint       width,
                              gint       height,
                              gboolean   send_expose)
{
  /* FIXME: Implement */
}

/* Get the toplevel ordering from NSApp and update our own list. We do
 * this on demand since the NSApp's list is not up to date directly
 * after we get windowDidBecomeMain.
 */
static void
update_toplevel_order (void)
{
  GdkWindowObject *root;
  GdkWindowImplQuartz *root_impl;
  NSEnumerator *enumerator;
  id nswindow;
  GList *toplevels = NULL;

  root = GDK_WINDOW_OBJECT (_gdk_root);
  root_impl = GDK_WINDOW_IMPL_QUARTZ (root->impl);

  if (root_impl->sorted_children)
    return;

  GDK_QUARTZ_ALLOC_POOL;

  enumerator = [[NSApp orderedWindows] objectEnumerator];
  while ((nswindow = [enumerator nextObject]))
    {
      GdkWindow *window;

      if (![[nswindow contentView] isKindOfClass:[GdkQuartzView class]])
        continue;

      window = [(GdkQuartzView *)[nswindow contentView] gdkWindow];
      toplevels = g_list_prepend (toplevels, window);
    }

  GDK_QUARTZ_RELEASE_POOL;

  root_impl->sorted_children = g_list_reverse (toplevels);
}

static void
clear_toplevel_order (void)
{
  GdkWindowObject *root;
  GdkWindowImplQuartz *root_impl;

  root = GDK_WINDOW_OBJECT (_gdk_root);
  root_impl = GDK_WINDOW_IMPL_QUARTZ (root->impl);

  g_list_free (root_impl->sorted_children);
  root_impl->sorted_children = NULL;
}

static void
gdk_window_quartz_raise (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (WINDOW_IS_TOPLEVEL (window))
    {
      GdkWindowImplQuartz *impl;

      impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);
      [impl->toplevel orderFront:impl->toplevel];

      clear_toplevel_order ();
    }
  else
    {
      GdkWindowObject *parent = GDK_WINDOW_OBJECT (window)->parent;

      if (parent)
        {
          GdkWindowImplQuartz *impl;

          impl = (GdkWindowImplQuartz *)parent->impl;

          impl->sorted_children = g_list_remove (impl->sorted_children, window);
          impl->sorted_children = g_list_prepend (impl->sorted_children, window);
        }
    }
}

static void
gdk_window_quartz_lower (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (WINDOW_IS_TOPLEVEL (window))
    {
      GdkWindowImplQuartz *impl;

      impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);
      [impl->toplevel orderBack:impl->toplevel];

      clear_toplevel_order ();
    }
  else
    {
      GdkWindowObject *parent = GDK_WINDOW_OBJECT (window)->parent;

      if (parent)
        {
          GdkWindowImplQuartz *impl;

          impl = (GdkWindowImplQuartz *)parent->impl;

          impl->sorted_children = g_list_remove (impl->sorted_children, window);
          impl->sorted_children = g_list_append (impl->sorted_children, window);
        }
    }
}

static void
gdk_window_quartz_set_background (GdkWindow      *window,
                                  const GdkColor *color)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  private->bg_color = *color;

  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    g_object_unref (private->bg_pixmap);
  
  private->bg_pixmap = NULL;
}

static void
gdk_window_quartz_set_back_pixmap (GdkWindow *window,
                                   GdkPixmap *pixmap,
                                   gboolean   parent_relative)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    g_object_unref (private->bg_pixmap);

  if (parent_relative)
    {
      private->bg_pixmap = GDK_PARENT_RELATIVE_BG;
      GDK_NOTE (MISC, g_print (G_STRLOC ": setting background pixmap to parent_relative\n"));
    }
  else
    {
      if (pixmap)
	{
	  g_object_ref (pixmap);
	  private->bg_pixmap = pixmap;
	}
      else
	{
	  private->bg_pixmap = GDK_NO_BG;
	}
    }
}

static void
gdk_window_quartz_set_cursor (GdkWindow *window,
                              GdkCursor *cursor)
{
  GdkWindowImplQuartz *impl;
  GdkCursorPrivate *cursor_private;
  NSCursor *nscursor;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);
  cursor_private = (GdkCursorPrivate *)cursor;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_QUARTZ_ALLOC_POOL;

  if (!cursor)
    nscursor = NULL;
  else 
    nscursor = [cursor_private->nscursor retain];

  if (impl->nscursor)
    [impl->nscursor release];

  impl->nscursor = nscursor;

  GDK_QUARTZ_RELEASE_POOL;

  _gdk_quartz_events_update_cursor (_gdk_quartz_events_get_mouse_window (TRUE));
}

static void
gdk_window_quartz_get_geometry (GdkWindow *window,
                                gint      *x,
                                gint      *y,
                                gint      *width,
                                gint      *height,
                                gint      *depth)
{
  GdkWindowImplQuartz *impl;
  NSRect ns_rect;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);
  if (window == _gdk_root)
    {
      if (x) 
        *x = 0;
      if (y) 
        *y = 0;

      if (width) 
        *width = impl->width;
      if (height)
        *height = impl->height;
    }
  else if (WINDOW_IS_TOPLEVEL (window))
    {
      ns_rect = [impl->toplevel contentRectForFrameRect:[impl->toplevel frame]];

      /* This doesn't work exactly as in X. There doesn't seem to be a
       * way to get the coords relative to the parent window (usually
       * the window frame), but that seems useless except for
       * borderless windows where it's relative to the root window. So
       * we return (0, 0) (should be something like (0, 22)) for
       * windows with borders and the root relative coordinates
       * otherwise.
       */
      if ([impl->toplevel styleMask] == NSBorderlessWindowMask)
        {
          if (x)
            *x = ns_rect.origin.x;
          if (y)
            *y = _gdk_quartz_window_get_inverted_screen_y (ns_rect.origin.y + ns_rect.size.height);
        }
      else 
        {
          if (x)
            *x = 0;
          if (y)
            *y = 0;
        }

      if (width)
        *width = ns_rect.size.width;
      if (height)
        *height = ns_rect.size.height;
    }
  else
    {
      ns_rect = [impl->view frame];
      
      if (x)
        *x = ns_rect.origin.x;
      if (y)
        *y = ns_rect.origin.y;
      if (width)
        *width  = ns_rect.size.width;
      if (height)
        *height = ns_rect.size.height;
    }
    
  if (depth)
      *depth = gdk_drawable_get_depth (window);
}

static gint
gdk_window_quartz_get_origin (GdkWindow *window,
                              gint      *x,
                              gint      *y)
{
  GdkWindowObject *private;
  int tmp_x = 0, tmp_y = 0;
  GdkWindow *toplevel;
  NSRect content_rect;
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window)) 
    {
      if (x) 
	*x = 0;
      if (y) 
	*y = 0;
      
      return 0;
    }

  if (window == _gdk_root)
    {
      *x = 0;
      *y = 0;
      return 1;
    }
  
  private = GDK_WINDOW_OBJECT (window);

  toplevel = gdk_window_get_toplevel (window);
  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (toplevel)->impl);

  content_rect = [impl->toplevel contentRectForFrameRect:[impl->toplevel frame]];

  tmp_x = content_rect.origin.x;
  tmp_y = _gdk_quartz_window_get_inverted_screen_y (content_rect.origin.y + content_rect.size.height);

  while (private != GDK_WINDOW_OBJECT (toplevel))
    {
      tmp_x += private->x;
      tmp_y += private->y;

      private = private->parent;
    }

  if (x)
    *x = tmp_x;
  if (y)
    *y = tmp_y;

  return TRUE;
}

gboolean
gdk_window_get_deskrelative_origin (GdkWindow *window,
				    gint      *x,
				    gint      *y)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  return gdk_window_get_origin (window, x, y);
}

void
gdk_window_get_root_origin (GdkWindow *window,
			    gint      *x,
			    gint      *y)
{
  GdkRectangle rect;

  g_return_if_fail (GDK_IS_WINDOW (window));

  rect.x = 0;
  rect.y = 0;
  
  gdk_window_get_frame_extents (window, &rect);

  if (x)
    *x = rect.x;

  if (y)
    *y = rect.y;
}

/* Returns coordinates relative to the root. */
void
_gdk_windowing_get_pointer (GdkDisplay       *display,
			    GdkScreen       **screen,
			    gint             *x,
			    gint             *y,
			    GdkModifierType  *mask)
{
  g_return_if_fail (display == _gdk_display);
  
  *screen = _gdk_screen;
  _gdk_windowing_window_get_pointer (_gdk_display, _gdk_root, x, y, mask);
}

/* Returns coordinates relative to the passed in window. */
GdkWindow *
_gdk_windowing_window_get_pointer (GdkDisplay      *display,
				   GdkWindow       *window,
				   gint            *x,
				   gint            *y,
				   GdkModifierType *mask)
{
  GdkWindowObject *toplevel;
  GdkWindowObject *private;
  NSPoint point;
  gint x_tmp, y_tmp;
  GdkWindow *found_window;

  if (GDK_WINDOW_DESTROYED (window))
    {
      *x = 0;
      *y = 0;
      *mask = 0;
      return NULL;
    }
  
  toplevel = GDK_WINDOW_OBJECT (gdk_window_get_toplevel (window));

  *mask = _gdk_quartz_events_get_current_event_mask ();

  /* Get the y coordinate, needs to be flipped. */
  if (window == _gdk_root)
    {
      point = [NSEvent mouseLocation];
      x_tmp = point.x;
      y_tmp = _gdk_quartz_window_get_inverted_screen_y (point.y);
    }
  else
    {
      GdkWindowImplQuartz *impl;
      NSWindow *nswindow;

      impl = GDK_WINDOW_IMPL_QUARTZ (toplevel->impl);
      nswindow = impl->toplevel;

      point = [nswindow mouseLocationOutsideOfEventStream];
      x_tmp = point.x;
      y_tmp = impl->height - point.y;
    }

  /* The coords are relative to the toplevel of the passed in window
   * at this point, make them relative to the passed in window:
   */
  private = GDK_WINDOW_OBJECT (window);
  while (private != toplevel)
    {
      x_tmp -= private->x;
      y_tmp -= private->y;

      private = private->parent;
    }

  found_window = _gdk_quartz_window_find_child (window, x_tmp, y_tmp);

  /* We never return the root window. */
  if (found_window == _gdk_root)
    found_window = NULL;

  *x = x_tmp;
  *y = y_tmp;

  return found_window;
}

void
gdk_display_warp_pointer (GdkDisplay *display,
			  GdkScreen  *screen,
			  gint        x,
			  gint        y)
{
  CGDisplayMoveCursorToPoint (CGMainDisplayID (), CGPointMake (x, y));
}

/* Returns coordinates relative to the found window. */
GdkWindow *
_gdk_windowing_window_at_pointer (GdkDisplay *display,
				  gint       *win_x,
				  gint       *win_y)
{
  GdkModifierType mask;
  GdkWindow *found_window;
  gint x, y;

  found_window = _gdk_windowing_window_get_pointer (display,
						    _gdk_root,
						    &x, &y,
						    &mask);
  if (found_window)
    {
      GdkWindowObject *private;

      /* The coordinates returned above are relative the root, we want
       * coordinates relative the window here. 
       */
      private = GDK_WINDOW_OBJECT (found_window);
      while (private != GDK_WINDOW_OBJECT (_gdk_root))
	{
	  x -= private->x;
	  y -= private->y;
	  
	  private = private->parent;
	}

      *win_x = x;
      *win_y = y;
    }
  else
    {
      /* Mimic the X backend here, -1,-1 for unknown windows. */
      *win_x = -1;
      *win_y = -1;
    }

  return found_window;
}

static GdkEventMask  
gdk_window_quartz_get_events (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return 0;
  else
    return GDK_WINDOW_OBJECT (window)->event_mask;
}

static void
gdk_window_quartz_set_events (GdkWindow       *window,
                              GdkEventMask     event_mask)
{
  if (!GDK_WINDOW_DESTROYED (window))
    {
      GDK_WINDOW_OBJECT (window)->event_mask = event_mask;
    }
}

void
gdk_window_set_urgency_hint (GdkWindow *window,
			     gboolean   urgent)
{
  /* FIXME: Implement */
}

void 
gdk_window_set_geometry_hints (GdkWindow         *window,
			       const GdkGeometry *geometry,
			       GdkWindowHints     geom_mask)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (geometry != NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  impl = GDK_WINDOW_IMPL_QUARTZ (((GdkWindowObject *) window)->impl);
  if (!impl->toplevel)
    return;

  if (geom_mask & GDK_HINT_POS)
    {
      /* FIXME: Implement */
    }

  if (geom_mask & GDK_HINT_USER_POS)
    {
      /* FIXME: Implement */
    }

  if (geom_mask & GDK_HINT_USER_SIZE)
    {
      /* FIXME: Implement */
    }
  
  if (geom_mask & GDK_HINT_MIN_SIZE)
    {
      NSSize size;

      size.width = geometry->min_width;
      size.height = geometry->min_height;

      [impl->toplevel setContentMinSize:size];
    }
  
  if (geom_mask & GDK_HINT_MAX_SIZE)
    {
      NSSize size;

      size.width = geometry->max_width;
      size.height = geometry->max_height;

      [impl->toplevel setContentMaxSize:size];
    }
  
  if (geom_mask & GDK_HINT_BASE_SIZE)
    {
      /* FIXME: Implement */
    }
  
  if (geom_mask & GDK_HINT_RESIZE_INC)
    {
      NSSize size;

      size.width = geometry->width_inc;
      size.height = geometry->height_inc;

      [impl->toplevel setContentResizeIncrements:size];
    }
  
  if (geom_mask & GDK_HINT_ASPECT)
    {
      /* FIXME: Implement */
    }

  if (geom_mask & GDK_HINT_WIN_GRAVITY)
    {
      /* FIXME: Implement */
    }
}

void
gdk_window_set_title (GdkWindow   *window,
		      const gchar *title)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (title != NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (((GdkWindowObject *)window)->impl);

  if (impl->toplevel)
    {
      GDK_QUARTZ_ALLOC_POOL;
      [impl->toplevel setTitle:[NSString stringWithUTF8String:title]];
      GDK_QUARTZ_RELEASE_POOL;
    }
}

void          
gdk_window_set_role (GdkWindow   *window,
		     const gchar *role)
{
  /* FIXME: Implement */
}

void
gdk_window_set_transient_for (GdkWindow *window, 
			      GdkWindow *parent)
{
  GdkWindowImplQuartz *window_impl;
  GdkWindowImplQuartz *parent_impl;

  if (GDK_WINDOW_DESTROYED (window) || GDK_WINDOW_DESTROYED (parent))
    return;

  window_impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);
  if (!window_impl->toplevel)
    return;

  GDK_QUARTZ_ALLOC_POOL;

  if (window_impl->transient_for)
    {
      _gdk_quartz_window_detach_from_parent (window);

      g_object_unref (window_impl->transient_for);
      window_impl->transient_for = NULL;
    }

  parent_impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (parent)->impl);
  if (parent_impl->toplevel)
    {
      /* We save the parent because it needs to be unset/reset when
       * hiding and showing the window. 
       */

      /* We don't set transients for tooltips, they are already
       * handled by the window level being the top one. If we do, then
       * the parent window will be brought to the top just because the
       * tooltip is, which is not what we want.
       */
      if (gdk_window_get_type_hint (window) != GDK_WINDOW_TYPE_HINT_TOOLTIP)
        {
          window_impl->transient_for = g_object_ref (parent);

          /* We only add the window if it is shown, otherwise it will
           * be shown unconditionally here. If it is not shown, the
           * window will be added in show() instead.
           */
          if (!(GDK_WINDOW_OBJECT (window)->state & GDK_WINDOW_STATE_WITHDRAWN))
            _gdk_quartz_window_attach_to_parent (window);
        }
    }
  
  GDK_QUARTZ_RELEASE_POOL;
}

static void
gdk_window_quartz_shape_combine_region (GdkWindow       *window,
                                        const GdkRegion *shape,
                                        gint             x,
                                        gint             y)
{
  /* FIXME: Implement */
}

static void
gdk_window_quartz_shape_combine_mask (GdkWindow *window,
                                      GdkBitmap *mask,
                                      gint       x,
                                      gint       y)
{
  /* FIXME: Implement */
}

void
gdk_window_input_shape_combine_mask (GdkWindow *window,
				     GdkBitmap *mask,
				     gint       x,
				     gint       y)
{
  /* FIXME: Implement */
}

void
gdk_window_input_shape_combine_region (GdkWindow       *window,
                                       const GdkRegion *shape_region,
                                       gint             offset_x,
                                       gint             offset_y)
{
  /* FIXME: Implement */
}

void 
gdk_window_set_child_input_shapes (GdkWindow *window)
{
  /* FIXME: IMplement */
}

void
gdk_window_set_override_redirect (GdkWindow *window,
				  gboolean override_redirect)
{
  /* FIXME: Implement */
}

void
gdk_window_set_accept_focus (GdkWindow *window,
			     gboolean accept_focus)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *)window;  

  private->accept_focus = accept_focus != FALSE;
}

static void
gdk_window_quartz_set_child_shapes (GdkWindow *window)
{
  /* FIXME: Implement */
}

static void
gdk_window_quartz_merge_child_shapes (GdkWindow *window)
{
  /* FIXME: Implement */
}

void 
gdk_window_merge_child_input_shapes (GdkWindow *window)
{
  /* FIXME: Implement */
}

static gboolean 
gdk_window_quartz_set_static_gravities (GdkWindow *window,
                                        gboolean   use_static)
{
  /* FIXME: Implement */
  return FALSE;
}

void
gdk_window_set_focus_on_map (GdkWindow *window,
			     gboolean focus_on_map)
{
  GdkWindowObject *private;

  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject *)window;  
  
  private->focus_on_map = focus_on_map != FALSE;
}

void          
gdk_window_set_icon (GdkWindow *window, 
		     GdkWindow *icon_window,
		     GdkPixmap *pixmap,
		     GdkBitmap *mask)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

void          
gdk_window_set_icon_name (GdkWindow   *window, 
			  const gchar *name)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

void
gdk_window_focus (GdkWindow *window,
                  guint32    timestamp)
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;
	
  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject*) window;
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if (impl->toplevel)
    {
      if (private->accept_focus && private->window_type != GDK_WINDOW_TEMP) 
        {
          GDK_QUARTZ_ALLOC_POOL;
          [impl->toplevel makeKeyAndOrderFront:impl->toplevel];
          clear_toplevel_order ();
          GDK_QUARTZ_RELEASE_POOL;
        }
    }
}

void
gdk_window_set_hints (GdkWindow *window,
		      gint       x,
		      gint       y,
		      gint       min_width,
		      gint       min_height,
		      gint       max_width,
		      gint       max_height,
		      gint       flags)
{
  /* FIXME: Implement */
}

static
gint window_type_hint_to_level (GdkWindowTypeHint hint)
{
  switch (hint)
    {
    case GDK_WINDOW_TYPE_HINT_DOCK:
    case GDK_WINDOW_TYPE_HINT_UTILITY:
      return NSFloatingWindowLevel;

    case GDK_WINDOW_TYPE_HINT_MENU: /* Torn-off menu */
    case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU: /* Menu from menubar */
      return NSTornOffMenuWindowLevel;

    case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
    case GDK_WINDOW_TYPE_HINT_TOOLTIP:
      return NSStatusWindowLevel;

    case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
    case GDK_WINDOW_TYPE_HINT_POPUP_MENU:
    case GDK_WINDOW_TYPE_HINT_COMBO:
    case GDK_WINDOW_TYPE_HINT_DND:
      return NSPopUpMenuWindowLevel;

    case GDK_WINDOW_TYPE_HINT_NORMAL:  /* Normal toplevel window */
    case GDK_WINDOW_TYPE_HINT_DIALOG:  /* Dialog window */
    case GDK_WINDOW_TYPE_HINT_TOOLBAR: /* Window used to implement toolbars */
    case GDK_WINDOW_TYPE_HINT_DESKTOP: /* N/A */
      break;

    default:
      break;
    }

  return NSNormalWindowLevel;
}

static gboolean 
window_type_hint_to_shadow (GdkWindowTypeHint hint)
{
  switch (hint)
    {
    case GDK_WINDOW_TYPE_HINT_NORMAL:  /* Normal toplevel window */
    case GDK_WINDOW_TYPE_HINT_DIALOG:  /* Dialog window */
    case GDK_WINDOW_TYPE_HINT_DOCK:
    case GDK_WINDOW_TYPE_HINT_UTILITY:
    case GDK_WINDOW_TYPE_HINT_MENU: /* Torn-off menu */
    case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU: /* Menu from menubar */
    case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
    case GDK_WINDOW_TYPE_HINT_POPUP_MENU:
    case GDK_WINDOW_TYPE_HINT_COMBO:
    case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
    case GDK_WINDOW_TYPE_HINT_TOOLTIP:
      return TRUE;

    case GDK_WINDOW_TYPE_HINT_TOOLBAR: /* Window used to implement toolbars */
    case GDK_WINDOW_TYPE_HINT_DESKTOP: /* N/A */
    case GDK_WINDOW_TYPE_HINT_DND:
      break;

    default:
      break;
    }

  return FALSE;
}


void
gdk_window_set_type_hint (GdkWindow        *window,
			  GdkWindowTypeHint hint)
{
  GdkWindowImplQuartz *impl;
  
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (((GdkWindowObject *) window)->impl);

  impl->type_hint = hint;

  /* Match the documentation, only do something if we're not mapped yet. */
  if (GDK_WINDOW_IS_MAPPED (window))
    return;

  [impl->toplevel setHasShadow: window_type_hint_to_shadow (hint)];
  [impl->toplevel setLevel: window_type_hint_to_level (hint)];
}

GdkWindowTypeHint
gdk_window_get_type_hint (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return GDK_WINDOW_TYPE_HINT_NORMAL;
  
  return GDK_WINDOW_IMPL_QUARTZ (((GdkWindowObject *) window)->impl)->type_hint;
}

void
gdk_window_set_modal_hint (GdkWindow *window,
			   gboolean   modal)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

void
gdk_window_set_skip_taskbar_hint (GdkWindow *window,
				  gboolean   skips_taskbar)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

void
gdk_window_set_skip_pager_hint (GdkWindow *window,
				gboolean   skips_pager)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

void
gdk_window_begin_resize_drag (GdkWindow     *window,
                              GdkWindowEdge  edge,
                              gint           button,
                              gint           root_x,
                              gint           root_y,
                              guint32        timestamp)
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (edge != GDK_WINDOW_EDGE_SOUTH_EAST)
    {
      g_warning ("Resizing is only implemented for GDK_WINDOW_EDGE_SOUTH_EAST on Mac OS");
      return;
    }

  if (GDK_WINDOW_DESTROYED (window))
    return;

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if (!impl->toplevel)
    {
      g_warning ("Can't call gdk_window_begin_resize_drag on non-toplevel window");
      return;
    }

  [(GdkQuartzWindow *)impl->toplevel beginManualResize];
}

void
gdk_window_begin_move_drag (GdkWindow *window,
                            gint       button,
                            gint       root_x,
                            gint       root_y,
                            guint32    timestamp)
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if (!impl->toplevel)
    {
      g_warning ("Can't call gdk_window_begin_move_drag on non-toplevel window");
      return;
    }

  [(GdkQuartzWindow *)impl->toplevel beginManualMove];
}

void
gdk_window_set_icon_list (GdkWindow *window,
			  GList     *pixbufs)
{
  /* FIXME: Implement */
}

void
gdk_window_get_frame_extents (GdkWindow    *window,
                              GdkRectangle *rect)
{
  GdkWindowObject *private;
  GdkWindow *toplevel;
  GdkWindowImplQuartz *impl;
  NSRect ns_rect;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (rect != NULL);

  private = GDK_WINDOW_OBJECT (window);

  rect->x = 0;
  rect->y = 0;
  rect->width = 1;
  rect->height = 1;
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  toplevel = gdk_window_get_toplevel (window);
  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (toplevel)->impl);

  ns_rect = [impl->toplevel frame];

  rect->x = ns_rect.origin.x;
  rect->y = _gdk_quartz_window_get_inverted_screen_y (ns_rect.origin.y + ns_rect.size.height);
  rect->width = ns_rect.size.width;
  rect->height = ns_rect.size.height;
}

void
gdk_window_set_decorations (GdkWindow       *window,
			    GdkWMDecoration  decorations)
{
  GdkWindowImplQuartz *impl;
  int old_mask, new_mask;
  NSView *old_view;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (WINDOW_IS_TOPLEVEL (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);

  if (decorations == 0 || GDK_WINDOW_TYPE (window) == GDK_WINDOW_TEMP ||
      impl->type_hint == GDK_WINDOW_TYPE_HINT_SPLASHSCREEN )
    {
      new_mask = NSBorderlessWindowMask;
    }
  else
    {
      /* FIXME: Honor other GDK_DECOR_* flags. */
      new_mask = (NSTitledWindowMask | NSClosableWindowMask |
                    NSMiniaturizableWindowMask | NSResizableWindowMask);
    }

  GDK_QUARTZ_ALLOC_POOL;

  old_mask = [impl->toplevel styleMask];

  /* Note, there doesn't seem to be a way to change this without
   * recreating the toplevel. There might be bad side-effects of doing
   * that, but it seems alright.
   */
  if (old_mask != new_mask)
    {
      NSRect rect;

      old_view = [impl->toplevel contentView];

      rect = [impl->toplevel frame];

      /* Properly update the size of the window when the titlebar is
       * added or removed.
       */
      if (old_mask == NSBorderlessWindowMask &&
          new_mask != NSBorderlessWindowMask)
        {
          rect = [NSWindow frameRectForContentRect:rect styleMask:new_mask];

        }
      else if (old_mask != NSBorderlessWindowMask &&
               new_mask == NSBorderlessWindowMask)
        {
          rect = [NSWindow contentRectForFrameRect:rect styleMask:old_mask];
        }

      impl->toplevel = [impl->toplevel initWithContentRect:rect
                                                 styleMask:new_mask
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];

      [impl->toplevel setHasShadow: window_type_hint_to_shadow (impl->type_hint)];
      [impl->toplevel setLevel: window_type_hint_to_level (impl->type_hint)];

      [impl->toplevel setContentView:old_view];
      [impl->toplevel setFrame:rect display:YES];

      /* Invalidate the window shadow for non-opaque views that have shadow
       * enabled, to get the shadow shape updated.
       */
      if (![old_view isOpaque] && [impl->toplevel hasShadow])
        [(GdkQuartzView*)old_view setNeedsInvalidateShadow:YES];
    }

  GDK_QUARTZ_RELEASE_POOL;
}

gboolean
gdk_window_get_decorations (GdkWindow       *window,
			    GdkWMDecoration *decorations)
{
  GdkWindowImplQuartz *impl;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (WINDOW_IS_TOPLEVEL (window), FALSE);

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);

  if (decorations)
    {
      /* Borderless is 0, so we can't check it as a bit being set. */
      if ([impl->toplevel styleMask] == NSBorderlessWindowMask)
        {
          *decorations = 0;
        }
      else
        {
          /* FIXME: Honor the other GDK_DECOR_* flags. */
          *decorations = GDK_DECOR_ALL;
        }
    }

  return TRUE;
}

void
gdk_window_set_functions (GdkWindow    *window,
			  GdkWMFunction functions)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

static void
gdk_window_quartz_get_offsets (GdkWindow  *window,
                               gint       *x_offset,
                               gint       *y_offset)
{
  *x_offset = *y_offset = 0;
}

gboolean
_gdk_windowing_window_queue_antiexpose (GdkWindow  *window,
					GdkRegion  *area)
{
  return FALSE;
}

void
gdk_window_stick (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
}

void
gdk_window_unstick (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
}

void
gdk_window_maximize (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      GDK_QUARTZ_ALLOC_POOL;

      if (impl->toplevel && ![impl->toplevel isZoomed])
	[impl->toplevel zoom:nil];

      GDK_QUARTZ_RELEASE_POOL;
    }
  else
    {
      gdk_synthesize_window_state (window,
				   0,
				   GDK_WINDOW_STATE_MAXIMIZED);
    }
}

void
gdk_window_unmaximize (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      GDK_QUARTZ_ALLOC_POOL;

      if (impl->toplevel && [impl->toplevel isZoomed])
	[impl->toplevel zoom:nil];

      GDK_QUARTZ_RELEASE_POOL;
    }
  else
    {
      gdk_synthesize_window_state (window,
				   GDK_WINDOW_STATE_MAXIMIZED,
				   0);
    }
}

void
gdk_window_iconify (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      GDK_QUARTZ_ALLOC_POOL;

      if (impl->toplevel)
	[impl->toplevel miniaturize:nil];

      GDK_QUARTZ_RELEASE_POOL;
    }
  else
    {
      gdk_synthesize_window_state (window,
				   0,
				   GDK_WINDOW_STATE_ICONIFIED);
    }
}

void
gdk_window_deiconify (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      GDK_QUARTZ_ALLOC_POOL;

      if (impl->toplevel)
	[impl->toplevel deminiaturize:nil];

      GDK_QUARTZ_RELEASE_POOL;
    }
  else
    {
      gdk_synthesize_window_state (window,
				   GDK_WINDOW_STATE_ICONIFIED,
				   0);
    }
}

static FullscreenSavedGeometry *
get_fullscreen_geometry (GdkWindow *window)
{
  return g_object_get_data (G_OBJECT (window), FULLSCREEN_DATA);
}

void
gdk_window_fullscreen (GdkWindow *window)
{
  FullscreenSavedGeometry *geometry;
  GdkWindowObject *private = (GdkWindowObject *) window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  NSRect frame;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (WINDOW_IS_TOPLEVEL (window));

  geometry = get_fullscreen_geometry (window);
  if (!geometry)
    {
      geometry = g_new (FullscreenSavedGeometry, 1);

      geometry->x = private->x;
      geometry->y = private->y;
      geometry->width = impl->width;
      geometry->height = impl->height;

      if (!gdk_window_get_decorations (window, &geometry->decor))
        geometry->decor = GDK_DECOR_ALL;

      g_object_set_data_full (G_OBJECT (window),
                              FULLSCREEN_DATA, geometry, 
                              g_free);

      gdk_window_set_decorations (window, 0);

      frame = [[NSScreen mainScreen] frame];
      move_resize_window_internal (window,
                                   0, 0, 
                                   frame.size.width, frame.size.height);
    }

  SetSystemUIMode (kUIModeAllHidden, kUIOptionAutoShowMenuBar);

  gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_FULLSCREEN);
}

void
gdk_window_unfullscreen (GdkWindow *window)
{
  FullscreenSavedGeometry *geometry;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (WINDOW_IS_TOPLEVEL (window));

  geometry = get_fullscreen_geometry (window);
  if (geometry)
    {
      SetSystemUIMode (kUIModeNormal, 0);

      move_resize_window_internal (window,
                                   geometry->x,
                                   geometry->y,
                                   geometry->width,
                                   geometry->height);
      
      gdk_window_set_decorations (window, geometry->decor);

      g_object_set_data (G_OBJECT (window), FULLSCREEN_DATA, NULL);

      gdk_synthesize_window_state (window, GDK_WINDOW_STATE_FULLSCREEN, 0);
    }
}

void
gdk_window_set_keep_above (GdkWindow *window, gboolean setting)
{
  GdkWindowObject *private = (GdkWindowObject *) window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  gint level;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (WINDOW_IS_TOPLEVEL (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  level = window_type_hint_to_level (gdk_window_get_type_hint (window));
  
  /* Adjust normal window level by one if necessary. */
  [impl->toplevel setLevel: level + (setting ? 1 : 0)];
}

void
gdk_window_set_keep_below (GdkWindow *window, gboolean setting)
{
  GdkWindowObject *private = (GdkWindowObject *) window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  gint level;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (WINDOW_IS_TOPLEVEL (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  level = window_type_hint_to_level (gdk_window_get_type_hint (window));
  
  /* Adjust normal window level by one if necessary. */
  [impl->toplevel setLevel: level - (setting ? 1 : 0)];
}

GdkWindow *
gdk_window_get_group (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD, NULL);

  /* FIXME: Implement */

  return NULL;
}

void          
gdk_window_set_group (GdkWindow *window, 
		      GdkWindow *leader)
{
  /* FIXME: Implement */	
}

GdkWindow*
gdk_window_foreign_new_for_display (GdkDisplay      *display,
				    GdkNativeWindow  anid)
{
  /* Foreign windows aren't supported in Mac OS X */
  return NULL;
}

GdkWindow*
gdk_window_lookup (GdkNativeWindow anid)
{
  /* Foreign windows aren't supported in Mac OS X */
  return NULL;
}

GdkWindow *
gdk_window_lookup_for_display (GdkDisplay *display, GdkNativeWindow anid)
{
  /* Foreign windows aren't supported in Mac OS X */
  return NULL;
}

void
gdk_window_enable_synchronized_configure (GdkWindow *window)
{
}

void
gdk_window_configure_finished (GdkWindow *window)
{
}

void
gdk_window_destroy_notify (GdkWindow *window)
{
}

void 
gdk_window_beep (GdkWindow *window)
{
  gdk_display_beep (_gdk_display);
}

void
gdk_window_set_opacity (GdkWindow *window,
			gdouble    opacity)
{
  GdkWindowObject *private = (GdkWindowObject *) window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (WINDOW_IS_TOPLEVEL (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (opacity < 0)
    opacity = 0;
  else if (opacity > 1)
    opacity = 1;

  [impl->toplevel setAlphaValue: opacity];
}

void
_gdk_windowing_window_set_composited (GdkWindow *window, gboolean composited)
{
}

static void
gdk_window_impl_iface_init (GdkWindowImplIface *iface)
{
  iface->show = gdk_window_quartz_show;
  iface->hide = gdk_window_quartz_hide;
  iface->withdraw = gdk_window_quartz_withdraw;
  iface->set_events = gdk_window_quartz_set_events;
  iface->get_events = gdk_window_quartz_get_events;
  iface->clear_area = gdk_window_quartz_clear_area;
  iface->raise = gdk_window_quartz_raise;
  iface->lower = gdk_window_quartz_lower;
  iface->move_resize = gdk_window_quartz_move_resize;
  iface->scroll = _gdk_quartz_window_scroll;
  iface->move_region = _gdk_quartz_window_move_region;
  iface->set_background = gdk_window_quartz_set_background;
  iface->set_back_pixmap = gdk_window_quartz_set_back_pixmap;
  iface->reparent = gdk_window_quartz_reparent;
  iface->set_cursor = gdk_window_quartz_set_cursor;
  iface->get_geometry = gdk_window_quartz_get_geometry;
  iface->get_origin = gdk_window_quartz_get_origin;
  iface->shape_combine_mask = gdk_window_quartz_shape_combine_mask;
  iface->shape_combine_region = gdk_window_quartz_shape_combine_region;
  iface->set_child_shapes = gdk_window_quartz_set_child_shapes;
  iface->merge_child_shapes = gdk_window_quartz_merge_child_shapes;
  iface->set_static_gravities = gdk_window_quartz_set_static_gravities;
  iface->get_offsets = gdk_window_quartz_get_offsets;
}
