/* gdkwindow-quartz.c
 *
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2005 Imendio AB
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

#include <config.h>

#include "gdk.h"
#include "gdkprivate-quartz.h"

static gpointer parent_class;

NSView *
gdk_quartz_window_get_nsview (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  return ((GdkWindowImplQuartz *)private->impl)->view;
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

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_window_impl_quartz_class_init (GdkWindowImplQuartzClass *klass)
{
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_window_impl_quartz_finalize;

  drawable_class->get_size = gdk_window_impl_quartz_get_size;

  /* Visible and clip regions are the same */
  drawable_class->get_clip_region = gdk_window_impl_quartz_get_visible_region;
  drawable_class->get_visible_region = gdk_window_impl_quartz_get_visible_region;
}


static void
gdk_window_impl_quartz_init (GdkWindowImplQuartz *impl)
{
  impl->width = 1;
  impl->height = 1;
}

static void
gdk_window_impl_quartz_begin_paint_region (GdkPaintable *paintable,
					   GdkRegion    *region)
{
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (paintable);
  CGContextRef context = _gdk_quartz_drawable_get_context (GDK_DRAWABLE (impl), FALSE);
  int i, n_rects;
  GdkRectangle *rects;

  if (impl->begin_paint_count == 0)
    impl->paint_clip_region = gdk_region_copy (region);
  else
    gdk_region_union (impl->paint_clip_region, region);

  impl->begin_paint_count ++;

  gdk_region_get_rectangles (region, &rects, &n_rects);
  for (i = 0; i < n_rects; i++) 
    {
      _gdk_quartz_set_context_fill_color_from_pixel (context, gdk_drawable_get_colormap (GDK_DRAWABLE_IMPL_QUARTZ (impl)->wrapper),
						     GDK_WINDOW_OBJECT (GDK_DRAWABLE_IMPL_QUARTZ (impl)->wrapper)->bg_color.pixel);
      
      CGContextFillRect (context, CGRectMake (rects[i].x, rects[i].y, rects[i].width, rects[i].height));
      
    }
  g_free (rects);

  _gdk_quartz_drawable_release_context (GDK_DRAWABLE (impl), context);
}

static void
gdk_window_impl_quartz_end_paint (GdkPaintable *paintable)
{
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (paintable);

  impl->begin_paint_count --;

  if (impl->begin_paint_count == 0)
    {
      gdk_region_destroy (impl->paint_clip_region);
      impl->paint_clip_region = NULL;
    }
}

static void
gdk_window_impl_quartz_invalidate_maybe_recurse (GdkPaintable *paintable,
						 GdkRegion    *region,
						 gboolean    (*child_func) (GdkWindow *, gpointer),
						 gpointer      user_data)
{
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (paintable);
  int i, n_rects;
  GdkRectangle *rects;

  gdk_region_get_rectangles (region, &rects, &n_rects);

  for (i = 0; i < n_rects; i++) 
    {
      [impl->view setNeedsDisplayInRect:NSMakeRect (rects[i].x, rects[i].y,
						    rects[i].width, rects[i].height)];
    }

  g_free (rects);

  /* FIXME: Check if we need to traverse the children */
}

static void
gdk_window_impl_quartz_process_updates (GdkPaintable *paintable,
					gboolean      update_children)
{
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (paintable);

  [impl->view display];

  /* FIXME: Check if display actually updates the children too */
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
      static const GTypeInfo object_info =
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
      
      static const GInterfaceInfo paintable_info = 
      {
	(GInterfaceInitFunc) gdk_window_impl_quartz_paintable_init,
	NULL,
	NULL
      };

      object_type = g_type_register_static (GDK_TYPE_DRAWABLE_IMPL_QUARTZ,
                                            "GdkWindowImplQuartz",
                                            &object_info, 0);
      g_type_add_interface_static (object_type,
				   GDK_TYPE_PAINTABLE,
				   &paintable_info);
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

gint 
_gdk_quartz_get_inverted_screen_y (gint y)
{
  NSRect rect = [[NSScreen mainScreen] frame];

  return rect.size.height - y;
}

GdkWindow *
gdk_window_new (GdkWindow     *parent,
		GdkWindowAttr *attributes,
		gint           attributes_mask)
{
  GdkWindow *window;
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;
  GdkDrawableImplQuartz *draw_impl;
  GdkVisual *visual;

  GDK_QUARTZ_ALLOC_POOL;

  if (parent && GDK_WINDOW_DESTROYED (parent))
    {
      GDK_QUARTZ_RELEASE_POOL;
      return NULL;
    }

  if (!parent)
    parent = _gdk_root;

  window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)window;
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  draw_impl = GDK_DRAWABLE_IMPL_QUARTZ (private->impl);
  draw_impl->wrapper = GDK_DRAWABLE (window);

  private->parent = (GdkWindowObject *)parent;

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

  if (private->parent)
    private->parent->children = g_list_prepend (private->parent->children, window);

  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));

  switch (attributes->window_type) 
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
      {
	NSRect content_rect = NSMakeRect (private->x, 
					  _gdk_quartz_get_inverted_screen_y (private->y) - impl->height,
					  impl->width, impl->height);
	const char *title;
	int style_mask;

	switch (attributes->window_type) {
	case GDK_WINDOW_TEMP:
	  style_mask = NSBorderlessWindowMask;
	  break;
	default:
	  style_mask = NSTitledWindowMask|NSClosableWindowMask|NSMiniaturizableWindowMask|NSResizableWindowMask;
	} 

	impl->toplevel = [[GdkQuartzWindow alloc] initWithContentRect:content_rect 
			                                    styleMask:style_mask
			                                      backing:NSBackingStoreBuffered defer:NO];
	if (attributes_mask & GDK_WA_TITLE)
	  title = attributes->title;
	else
	  title = get_default_title ();

	if (attributes->window_type == GDK_WINDOW_TEMP)
	  [impl->toplevel setLevel:NSPopUpMenuWindowLevel];

	gdk_window_set_title (window, title);

	  
	if (draw_impl->colormap == gdk_screen_get_rgba_colormap (_gdk_screen))
	  {
	    [impl->toplevel setOpaque:NO];
	    [impl->toplevel setBackgroundColor:[NSColor clearColor]];
	  }

	impl->view = [[GdkQuartzView alloc] initWithFrame:content_rect];
	[impl->view setGdkWindow:window];
	[impl->toplevel setContentView:impl->view];

	/* Add a tracking rect */
	impl->tracking_rect = [impl->view addTrackingRect:NSMakeRect(0, 0, impl->width, impl->height) 
			                            owner:impl->view
			                         userData:nil
			                     assumeInside:NO];
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

  return window;
}

void
_gdk_windowing_window_init (void)
{
  GdkWindowObject *private;

  g_assert (_gdk_root == NULL);

  _gdk_root = g_object_new (GDK_TYPE_WINDOW, NULL);

  private = (GdkWindowObject *)_gdk_root;

  private->state = 0; /* We don't want GDK_WINDOW_STATE_WITHDRAWN here */
  private->window_type = GDK_WINDOW_ROOT;
  private->depth = 24;
}

void
_gdk_windowing_window_destroy (GdkWindow *window,
			       gboolean   recursing,
			       gboolean   foreign_destroy)
{
  if (!recursing && ! foreign_destroy)
    {
      GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);

      if (window == _gdk_quartz_get_mouse_window ()) 
	{
	  _gdk_quartz_update_mouse_window (_gdk_root);
	}

      if (impl->toplevel)
	[impl->toplevel close];
      else if (impl->view)
	[impl->view release];
      
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

static void
show_window_internal (GdkWindow *window, gboolean raise)
{
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;

  private = (GdkWindowObject *)window;

  if (private->destroyed)
    return;

  GDK_QUARTZ_ALLOC_POOL;

  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  /* FIXME: We need to raise the window (move it to the top in the list) */

  if (impl->toplevel)
    {
      [impl->toplevel orderFront:nil];
      [impl->view setNeedsDisplay:YES];
    }
  else
    {
      [impl->view setHidden:NO];
      [impl->view setNeedsDisplay:YES];
    }

  if (all_parents_shown (private->parent))
    _gdk_quartz_send_map_events (window);

  gdk_synthesize_window_state (window, GDK_WINDOW_STATE_WITHDRAWN, 0);

  GDK_QUARTZ_RELEASE_POOL;
}

void
gdk_window_show_unraised (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  show_window_internal (window, FALSE);
}

void
gdk_window_show (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  show_window_internal (window, TRUE);
}

void
gdk_window_hide (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_synthesize_window_state (window,
				 0,
				 GDK_WINDOW_STATE_WITHDRAWN);
  
  _gdk_window_clear_update_area (window);

  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if (impl->toplevel) 
    {
      [impl->toplevel orderOut:nil];
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
gdk_window_withdraw (GdkWindow *window)
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

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if (x != -1)
    private->x = x;

  if (y != -1)
    private->y = y;

  if (width != -1)
    impl->width = width;

  if (height != -1)
    impl->height = height;

  GDK_QUARTZ_ALLOC_POOL;

  if (impl->toplevel)
    {
      NSRect content_rect = NSMakeRect (private->x, 
					_gdk_quartz_get_inverted_screen_y (private->y) ,
					impl->width, impl->height);
      NSRect frame_rect = [impl->toplevel frameRectForContentRect:content_rect];
      
      frame_rect.origin.y -= frame_rect.size.height;

      [impl->toplevel setFrame:frame_rect display:YES];
    }
  else 
    {
      if (!private->input_only)
	{
	  [impl->view setFrame:NSMakeRect (private->x, private->y, 
					   impl->width, impl->height)];

	  /* FIXME: Maybe we should use setNeedsDisplayInRect instead */
	  [impl->view setNeedsDisplay:YES];
	}
    }

  GDK_QUARTZ_RELEASE_POOL;
}

void
gdk_window_move (GdkWindow *window,
		 gint       x,
		 gint       y)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  move_resize_window_internal (window, x, y, -1, -1);
}

void
gdk_window_resize (GdkWindow *window,
		   gint       width,
		   gint       height)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  move_resize_window_internal (window, -1, -1, width, height);
}

void
gdk_window_move_resize (GdkWindow *window,
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

void
gdk_window_reparent (GdkWindow *window,
		     GdkWindow *new_parent,
		     gint       x,
		     gint       y)
{
  g_warning ("gdk_window_reparent: %p %p (%d, %d)", 
	     window, new_parent, x, y);

  /* FIXME: Implement */
}

void
_gdk_windowing_window_clear_area (GdkWindow *window,
				  gint       x,
				  gint       y,
				  gint       width,
				  gint       height)
{
  /* FIXME: Implement */
}

void
_gdk_windowing_window_clear_area_e (GdkWindow *window,
				    gint       x,
				    gint       y,
				    gint       width,
				    gint       height)
{
  /* FIXME: Implement */
}

void
gdk_window_raise (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  /* FIXME: Implement */
}

void
gdk_window_lower (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  /* FIXME: Implement */
}

void
gdk_window_set_background (GdkWindow      *window,
			   const GdkColor *color)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

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

void
gdk_window_set_back_pixmap (GdkWindow *window,
			    GdkPixmap *pixmap,
			    gboolean   parent_relative)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (pixmap == NULL || !parent_relative);
  g_return_if_fail (pixmap == NULL || gdk_drawable_get_depth (window) == gdk_drawable_get_depth (pixmap));

  /* FIXME: Implement */
}

void
gdk_window_set_cursor (GdkWindow *window,
		       GdkCursor *cursor)
{
  GdkWindowImplQuartz *impl;
  GdkCursorPrivate *cursor_private;
  NSCursor *nscursor;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);
  cursor_private = (GdkCursorPrivate *)cursor;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (!cursor)
    nscursor = NULL;
  else 
    nscursor = [cursor_private->nscursor retain];

  if (impl->nscursor)
    [impl->nscursor release];

  impl->nscursor = nscursor;

  _gdk_quartz_update_cursor (_gdk_quartz_get_mouse_window ());
}

void
gdk_window_get_geometry (GdkWindow *window,
			 gint      *x,
			 gint      *y,
			 gint      *width,
			 gint      *height,
			 gint      *depth)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

gboolean
gdk_window_get_origin (GdkWindow *window,
		       gint      *x,
		       gint      *y)
{
  GdkWindowObject *private;
  int tmp_x = 0, tmp_y = 0;
  GdkWindow *toplevel;
  NSRect content_rect;
  GdkWindowImplQuartz *impl;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (GDK_WINDOW_DESTROYED (window)) 
    {
      if (x) 
	*x = 0;
      if (y) 
	*y = 0;
      
      return FALSE;
    }
  
  private = GDK_WINDOW_OBJECT (window);

  toplevel = gdk_window_get_toplevel (window);
  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (toplevel)->impl);

  content_rect = [impl->toplevel contentRectForFrameRect:[impl->toplevel frame]];

  tmp_x = content_rect.origin.x;
  tmp_y = _gdk_quartz_get_inverted_screen_y (content_rect.origin.y + content_rect.size.height);

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

GdkWindow *
_gdk_windowing_window_get_pointer (GdkDisplay      *display,
				   GdkWindow       *window,
				   gint            *x,
				   gint            *y,
				   GdkModifierType *mask)
{
  GdkWindow *toplevel = gdk_window_get_toplevel (window);
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (toplevel)->impl);
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  NSWindow *nswindow = impl->toplevel;
  NSPoint point = [nswindow mouseLocationOutsideOfEventStream];
  int x_tmp, y_tmp;

  /* FIXME: Might need to special-case window being the root window. */
  
  /* First flip the y coordinate */
  x_tmp = point.x;
  y_tmp = impl->height - point.y;
      
  while (private != GDK_WINDOW_OBJECT (toplevel)) {
    x_tmp -= private->x;
    y_tmp -= private->y;
    
    private = private->parent;
  }

  if (x)
    *x = x_tmp;
  if (y)
    *y = y_tmp;

  /* FIXME: Implement return value */
  return NULL;
}

void
gdk_display_warp_pointer (GdkDisplay *display,
			  GdkScreen  *screen,
			  gint        x,
			  gint        y)
{
  CGDisplayMoveCursorToPoint (CGMainDisplayID (), CGPointMake (x, y));
}

GdkWindow *
_gdk_windowing_window_at_pointer (GdkDisplay *display,
				  gint       *win_x,
				  gint       *win_y)
{
  /* FIXME: Implement */
  return NULL;
}

GdkEventMask  
gdk_window_get_events (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (GDK_WINDOW_DESTROYED (window))
    return 0;
  else
    return GDK_WINDOW_OBJECT (window)->event_mask;
}

void          
gdk_window_set_events (GdkWindow       *window,
		       GdkEventMask     event_mask)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

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
gdk_window_set_geometry_hints (GdkWindow      *window,
			       GdkGeometry    *geometry,
			       GdkWindowHints  geom_mask)
{
  /* FIXME: Implement */
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
  /* FIXME: Implement */
}

void
gdk_window_shape_combine_region (GdkWindow *window,
                                 GdkRegion *shape,
                                 gint       x,
                                 gint       y)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

void
gdk_window_shape_combine_mask (GdkWindow *window,
			       GdkBitmap *mask,
			       gint x, gint y)
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
gdk_window_input_shape_combine_region (GdkWindow *window,
				       GdkRegion *shape_region,
				       gint       offset_x,
				       gint       offset_y)
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
  /* FIXME: Implement */
}

void
gdk_window_set_child_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

void
gdk_window_merge_child_shapes (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

void 
gdk_window_merge_child_input_shapes (GdkWindow *window)
{
  /* FIXME: Implement */
}

gboolean 
gdk_window_set_static_gravities (GdkWindow *window,
				 gboolean   use_static)
{
  /* FIXME: Implement */
  return FALSE;
}

void
gdk_window_set_focus_on_map (GdkWindow *window,
			     gboolean focus_on_map)
{
  /* FIXME: Implement */
}

void          
gdk_window_set_icon (GdkWindow *window, 
		     GdkWindow *icon_window,
		     GdkPixmap *pixmap,
		     GdkBitmap *mask)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

void          
gdk_window_set_icon_name (GdkWindow   *window, 
			  const gchar *name)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

void
gdk_window_focus (GdkWindow *window,
                  guint32    timestamp)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
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

void
gdk_window_set_type_hint (GdkWindow        *window,
			  GdkWindowTypeHint hint)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

GdkWindowTypeHint
gdk_window_get_type_hint (GdkWindow *window)
{
  /* FIXME: Implement */
  return GDK_WINDOW_TYPE_HINT_NORMAL;
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
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */  
}

void
gdk_window_begin_move_drag (GdkWindow *window,
                            gint       button,
                            gint       root_x,
                            gint       root_y,
                            guint32    timestamp)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
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
  rect->y = _gdk_quartz_get_inverted_screen_y (ns_rect.origin.y + ns_rect.size.height);
  rect->width = ns_rect.size.width;
  rect->height = ns_rect.size.height;
}

void
gdk_window_set_decorations (GdkWindow      *window,
			    GdkWMDecoration decorations)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

gboolean
gdk_window_get_decorations (GdkWindow       *window,
			    GdkWMDecoration *decorations)
{
  /* FIXME: Implement */
  return FALSE;
}

void
gdk_window_set_functions (GdkWindow    *window,
			  GdkWMFunction functions)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

void
_gdk_windowing_window_get_offsets (GdkWindow  *window,
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
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

void
gdk_window_unmaximize (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

void
gdk_window_iconify (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);
  if (!impl->toplevel)
    return;
  
  GDK_QUARTZ_ALLOC_POOL;
  [impl->toplevel miniaturize:nil];
  GDK_QUARTZ_RELEASE_POOL;
}

void
gdk_window_deiconify (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (window)->impl);
  if (!impl->toplevel)
    return;

  GDK_QUARTZ_ALLOC_POOL;
  [impl->toplevel deminiaturize:nil];
  GDK_QUARTZ_RELEASE_POOL;
}

void
gdk_window_fullscreen (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

void
gdk_window_unfullscreen (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

void
gdk_window_set_keep_above (GdkWindow *window, gboolean setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
}

void
gdk_window_set_keep_below (GdkWindow *window, gboolean setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* FIXME: Implement */
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
  /* FIXME: Implement. We should call this from -[GdkQuartzWindow dealloc] or
   * -[GdkQuartzView dealloc], although I suspect that currently they leak
   * anyway. */
}
