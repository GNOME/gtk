/* gdksurface-quartz.c
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gdk/gdk.h>
#include <gdk/gdkdeviceprivate.h>
#include <gdk/gdkdisplayprivate.h>

#include "gdksurfaceimpl.h"
#include "gdkprivate-quartz.h"
#include "gdkglcontext-quartz.h"
#include "gdkquartzscreen.h"
#include "gdkquartzcursor.h"

#include <Carbon/Carbon.h>
#include <AvailabilityMacros.h>

#include <sys/time.h>
#include <cairo-quartz.h>

static gpointer parent_class;
static gpointer root_window_parent_class;

static GSList   *update_nswindows;
static gboolean  in_process_all_updates = FALSE;

static GSList *main_window_stack;

void _gdk_quartz_surface_flush (GdkSurfaceImplQuartz *surface_impl);

typedef struct
{
  gint            x, y;
  gint            width, height;
  GdkWMDecoration decor;
} FullscreenSavedGeometry;


#ifndef AVAILABLE_MAC_OS_X_VERSION_10_7_AND_LATER
static FullscreenSavedGeometry *get_fullscreen_geometry (GdkSurface *window);
#endif

#define FULLSCREEN_DATA "fullscreen-data"

static void update_toplevel_order (void);
static void clear_toplevel_order  (void);

#define SURFACE_IS_TOPLEVEL(window)      TRUE

/*
 * GdkQuartzSurface
 */

struct _GdkQuartzSurface
{
  GdkSurface parent;
};

struct _GdkQuartzSurfaceClass
{
  GdkSurfaceClass parent_class;
};

G_DEFINE_TYPE (GdkQuartzSurface, gdk_quartz_surface, GDK_TYPE_SURFACE);

static void
gdk_quartz_surface_class_init (GdkQuartzSurfaceClass *quartz_surface_class)
{
}

static void
gdk_quartz_surface_init (GdkQuartzSurface *quartz_surface)
{
}


/*
 * GdkQuartzSurfaceImpl
 */

NSView *
gdk_quartz_surface_get_nsview (GdkSurface *window)
{
  if (GDK_SURFACE_DESTROYED (window))
    return NULL;

  return ((GdkSurfaceImplQuartz *)window->impl)->view;
}

NSWindow *
gdk_quartz_surface_get_nswindow (GdkSurface *window)
{
  if (GDK_SURFACE_DESTROYED (window))
    return NULL;

  return ((GdkSurfaceImplQuartz *)window->impl)->toplevel;
}

static CGContextRef
gdk_surface_impl_quartz_get_context (GdkSurfaceImplQuartz *surface_impl,
				    gboolean             antialias)
{
  CGContextRef cg_context;
  CGSize scale;

  if (GDK_SURFACE_DESTROYED (surface_impl->wrapper))
    return NULL;

  /* Lock focus when not called as part of a drawRect call. This
   * is needed when called from outside "real" expose events, for
   * example for synthesized expose events when realizing windows
   * and for widgets that send fake expose events like the arrow
   * buttons in spinbuttons or the position marker in rulers.
   */
  if (surface_impl->in_paint_rect_count == 0)
    {
      if (![surface_impl->view lockFocusIfCanDraw])
        return NULL;
    }
  if (gdk_quartz_osx_version () < GDK_OSX_YOSEMITE)
    cg_context = [[NSGraphicsContext currentContext] graphicsPort];
  else
    cg_context = [[NSGraphicsContext currentContext] CGContext];
  if (!cg_context)
    return NULL;
  CGContextSaveGState (cg_context);
  CGContextSetAllowsAntialiasing (cg_context, antialias);

  /* Undo the default scaling transform, since we apply our own
   * in gdk_quartz_ref_cairo_surface () */
  scale = CGContextConvertSizeToDeviceSpace (cg_context,
                                             CGSizeMake (1.0, 1.0));
  CGContextScaleCTM (cg_context, 1.0 / scale.width, 1.0 / scale.height);

  return cg_context;
}

static void
gdk_surface_impl_quartz_release_context (GdkSurfaceImplQuartz *surface_impl,
                                        CGContextRef         cg_context)
{
  CGContextRestoreGState (cg_context);
  CGContextSetAllowsAntialiasing (cg_context, TRUE);

  /* See comment in gdk_quartz_surface_get_context(). */
  if (surface_impl->in_paint_rect_count == 0)
    {
      _gdk_quartz_surface_flush (surface_impl);
      [surface_impl->view unlockFocus];
    }
}

static void
check_grab_destroy (GdkSurface *window)
{
  GList *devices = NULL, *l;
  GdkDisplay *display = gdk_surface_get_display (window);
  GdkSeat *seat;

  seat = gdk_display_get_default_seat (display);

  devices = g_list_prepend (devices, gdk_seat_get_keyboard (seat));
  devices = g_list_prepend (devices, gdk_seat_get_pointer (seat));

  for (l = devices; l; l = l->next)
    {
      GdkDeviceGrabInfo *grab;

      grab = _gdk_display_get_last_device_grab (display, l->data);
      if (grab && grab->native_surface == window)
        {
          /* Serials are always 0 in quartz, but for clarity: */
          grab->serial_end = grab->serial_start;
          grab->implicit_ungrab = TRUE;
        }
    }

  g_list_free (devices);
}

static void
gdk_surface_impl_quartz_finalize (GObject *object)
{
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (object);

  check_grab_destroy (GDK_SURFACE_IMPL_QUARTZ (object)->wrapper);

  if (impl->transient_for)
    g_object_unref (impl->transient_for);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* Help preventing "beam sync penalty" where CG makes all graphics code
 * block until the next vsync if we try to flush (including call display on
 * a view) too often. We do this by limiting the manual flushing done
 * outside of expose calls to less than some frequency when measured over
 * the last 4 flushes. This is a bit arbitray, but seems to make it possible
 * for some quick manual flushes (such as gtkruler or gimp’s marching ants)
 * without hitting the max flush frequency.
 *
 * If drawable NULL, no flushing is done, only registering that a flush was
 * done externally.
 */
void
_gdk_quartz_surface_flush (GdkSurfaceImplQuartz *surface_impl)
{
  static struct timeval prev_tv;
  static gint intervals[4];
  static gint index;
  struct timeval tv;
  gint ms;

  gettimeofday (&tv, NULL);
  ms = (tv.tv_sec - prev_tv.tv_sec) * 1000 + (tv.tv_usec - prev_tv.tv_usec) / 1000;
  intervals[index++ % 4] = ms;

  if (surface_impl)
    {
      ms = intervals[0] + intervals[1] + intervals[2] + intervals[3];

      /* ~25Hz on average. */
      if (ms > 4*40)
        {
          if (surface_impl)
            [surface_impl->toplevel flushWindow];

          prev_tv = tv;
        }
    }
  else
    prev_tv = tv;
}

static cairo_user_data_key_t gdk_quartz_cairo_key;

typedef struct {
  GdkSurfaceImplQuartz  *surface_impl;
  CGContextRef  cg_context;
} GdkQuartzCairoSurfaceData;

static void
gdk_quartz_cairo_surface_destroy (void *data)
{
  GdkQuartzCairoSurfaceData *surface_data = data;

  surface_data->surface_impl->cairo_surface = NULL;

  gdk_quartz_surface_release_context (surface_data->surface_impl,
                                     surface_data->cg_context);

  g_free (surface_data);
}

static cairo_surface_t *
gdk_quartz_create_cairo_surface (GdkSurfaceImplQuartz *impl,
				 int                  width,
				 int                  height)
{
  CGContextRef cg_context;
  GdkQuartzCairoSurfaceData *surface_data;
  cairo_surface_t *surface;

  cg_context = gdk_quartz_surface_get_context (impl, TRUE);

  surface_data = g_new (GdkQuartzCairoSurfaceData, 1);
  surface_data->surface_impl = impl;
  surface_data->cg_context = cg_context;

  if (cg_context)
    surface = cairo_quartz_surface_create_for_cg_context (cg_context,
                                                          width, height);
  else
    surface = cairo_quartz_surface_create(CAIRO_FORMAT_ARGB32, width, height);

  cairo_surface_set_user_data (surface, &gdk_quartz_cairo_key,
                               surface_data,
                               gdk_quartz_cairo_surface_destroy);

  return surface;
}

static cairo_surface_t *
gdk_quartz_ref_cairo_surface (GdkSurface *window)
{
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (GDK_SURFACE_DESTROYED (window))
    return NULL;

  if (!impl->cairo_surface)
    {
      gint scale = gdk_surface_get_scale_factor (impl->wrapper);

      impl->cairo_surface = 
          gdk_quartz_create_cairo_surface (impl,
                                           gdk_surface_get_width (impl->wrapper) * scale,
                                           gdk_surface_get_height (impl->wrapper) * scale);

      cairo_surface_set_device_scale (impl->cairo_surface, scale, scale);
    }
  else
    cairo_surface_reference (impl->cairo_surface);

  return impl->cairo_surface;
}

static void
gdk_surface_impl_quartz_init (GdkSurfaceImplQuartz *impl)
{
  impl->type_hint = GDK_SURFACE_TYPE_HINT_NORMAL;
}

static gboolean
gdk_surface_impl_quartz_begin_paint (GdkSurface *window)
{
  return FALSE;
}

static void
gdk_quartz_surface_set_needs_display_in_region (GdkSurface    *window,
                                               cairo_region_t    *region)
{
  GdkSurfaceImplQuartz *impl;
  int i, n_rects;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (!impl->needs_display_region)
    impl->needs_display_region = cairo_region_create ();

  cairo_region_union (impl->needs_display_region, region);

  n_rects = cairo_region_num_rectangles (region);
  for (i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t rect;
      cairo_region_get_rectangle (region, i, &rect);
      [impl->view setNeedsDisplayInRect:NSMakeRect (rect.x, rect.y,
                                                    rect.width, rect.height)];
    }
}

void
_gdk_quartz_surface_process_updates_recurse (GdkSurface *window,
                                            cairo_region_t *region)
{
  /* Make sure to only flush each toplevel at most once if we're called
   * from process_all_updates.
   */
  if (in_process_all_updates)
    {
      GdkSurface *toplevel;

      toplevel = gdk_surface_get_toplevel (window);
      if (toplevel && SURFACE_IS_TOPLEVEL (toplevel))
        {
          GdkSurfaceImplQuartz *toplevel_impl;
          NSWindow *nswindow;

          toplevel_impl = (GdkSurfaceImplQuartz *)toplevel->impl;
          nswindow = toplevel_impl->toplevel;

          /* In theory, we could skip the flush disabling, since we only
           * have one NSView.
           */
          if (nswindow && ![nswindow isFlushWindowDisabled]) 
            {
              [nswindow retain];
              [nswindow disableFlushWindow];
              update_nswindows = g_slist_prepend (update_nswindows, nswindow);
            }
        }
    }

  if (SURFACE_IS_TOPLEVEL (window))
    gdk_quartz_surface_set_needs_display_in_region (window, region);
  else
    _gdk_surface_process_updates_recurse (window, region);

  /* NOTE: I'm not sure if we should displayIfNeeded here. It slows down a
   * lot (since it triggers the beam syncing) and things seem to work
   * without it.
   */
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
get_ancestor_coordinates_from_child (GdkSurface *child_window,
				     gint       child_x,
				     gint       child_y,
				     GdkSurface *ancestor_window, 
				     gint      *ancestor_x, 
				     gint      *ancestor_y)
{
  while (child_window != ancestor_window)
    {
      child_x += child_window->x;
      child_y += child_window->y;

      child_window = child_window->parent;
    }

  *ancestor_x = child_x;
  *ancestor_y = child_y;
}

void
_gdk_quartz_surface_debug_highlight (GdkSurface *window, gint number)
{
  gint x, y;
  gint gx, gy;
  GdkSurface *toplevel;
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

  toplevel = gdk_surface_get_toplevel (window);
  get_ancestor_coordinates_from_child (window, 0, 0, toplevel, &x, &y);

  gdk_surface_get_origin (toplevel, &tx, &ty);
  x += tx;
  y += ty;

  _gdk_quartz_surface_gdk_xy_to_xy (x, y + window->height,
                                   &gx, &gy);

  rect = NSMakeRect (gx, gy, window->width, window->height);

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
_gdk_quartz_surface_is_ancestor (GdkSurface *ancestor,
                                GdkSurface *window)
{
  if (ancestor == NULL || window == NULL)
    return FALSE;

  return (gdk_surface_get_parent (window) == ancestor ||
          _gdk_quartz_surface_is_ancestor (ancestor, 
                                          gdk_surface_get_parent (window)));
}


/* See notes on top of gdkscreen-quartz.c */
void
_gdk_quartz_surface_gdk_xy_to_xy (gint  gdk_x,
                                 gint  gdk_y,
                                 gint *ns_x,
                                 gint *ns_y)
{
  GdkQuartzScreen *screen_quartz = GDK_QUARTZ_SCREEN (_gdk_screen);

  if (ns_y)
    *ns_y = screen_quartz->height - gdk_y + screen_quartz->min_y;

  if (ns_x)
    *ns_x = gdk_x + screen_quartz->min_x;
}

void
_gdk_quartz_surface_xy_to_gdk_xy (gint  ns_x,
                                 gint  ns_y,
                                 gint *gdk_x,
                                 gint *gdk_y)
{
  GdkQuartzScreen *screen_quartz = GDK_QUARTZ_SCREEN (_gdk_screen);

  if (gdk_y)
    *gdk_y = screen_quartz->height - ns_y + screen_quartz->min_y;

  if (gdk_x)
    *gdk_x = ns_x - screen_quartz->min_x;
}

void
_gdk_quartz_surface_nspoint_to_gdk_xy (NSPoint  point,
                                      gint    *x,
                                      gint    *y)
{
  _gdk_quartz_surface_xy_to_gdk_xy (point.x, point.y,
                                   x, y);
}

static GdkSurface *
find_child_window_helper (GdkSurface *window,
			  gint       x,
			  gint       y,
			  gint       x_offset,
			  gint       y_offset,
                          gboolean   get_toplevel)
{
  GdkSurfaceImplQuartz *impl;
  GList *l;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (window == _gdk_root)
    update_toplevel_order ();

  for (l = impl->sorted_children; l; l = l->next)
    {
      GdkSurface *child = l->data;
      GdkSurfaceImplQuartz *child_impl = GDK_SURFACE_IMPL_QUARTZ (child->impl);
      int temp_x, temp_y;

      if (!GDK_SURFACE_IS_MAPPED (child))
	continue;

      temp_x = x_offset + child->x;
      temp_y = y_offset + child->y;

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
          NSUInteger mask;
          int titlebar_height;

          mask = [child_impl->toplevel styleMask];

          /* Get the title bar height. */
          content = [NSWindow contentRectForFrameRect:frame
                                            styleMask:mask];
          titlebar_height = frame.size.height - content.size.height;

          if (titlebar_height > 0 &&
              x >= temp_x && y >= temp_y - titlebar_height &&
              x < temp_x + child->width && y < temp_y)
            {
              /* The root means "unknown" i.e. a window not managed by
               * GDK.
               */
              return (GdkSurface *)_gdk_root;
            }
        }

      if ((!get_toplevel || (get_toplevel && window == _gdk_root)) &&
          x >= temp_x && y >= temp_y &&
	  x < temp_x + child->width && y < temp_y + child->height)
	{
	  /* Look for child windows. */
	  return find_child_window_helper (l->data,
					   x, y,
					   temp_x, temp_y,
                                           get_toplevel);
	}
    }
  
  return window;
}

/* Given a GdkSurface and coordinates relative to it, returns the
 * innermost subwindow that contains the point. If the coordinates are
 * outside the passed in window, NULL is returned.
 */
GdkSurface *
_gdk_quartz_surface_find_child (GdkSurface *window,
			       gint       x,
			       gint       y,
                               gboolean   get_toplevel)
{
  if (x >= 0 && y >= 0 && x < window->width && y < window->height)
    return find_child_window_helper (window, x, y, 0, 0, get_toplevel);

  return NULL;
}


void
_gdk_quartz_surface_did_become_main (GdkSurface *window)
{
  main_window_stack = g_slist_remove (main_window_stack, window);

  if (window->surface_type != GDK_SURFACE_TEMP)
    main_window_stack = g_slist_prepend (main_window_stack, window);

  clear_toplevel_order ();
}

void
_gdk_quartz_surface_did_resign_main (GdkSurface *window)
{
  GdkSurface *new_window = NULL;

  if (main_window_stack)
    new_window = main_window_stack->data;
  else
    {
      GList *toplevels;

      toplevels = get_toplevels ();
      if (toplevels)
        new_window = toplevels->data;
    }

  if (new_window &&
      new_window != window &&
      GDK_SURFACE_IS_MAPPED (new_window) &&
      SURFACE_IS_TOPLEVEL (new_window))
    {
      GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (new_window->impl);

      [impl->toplevel makeKeyAndOrderFront:impl->toplevel];
    }

  clear_toplevel_order ();
}

static NSScreen *
get_nsscreen_for_point (gint x, gint y)
{
  int i;
  NSArray *screens;
  NSScreen *screen = NULL;

  GDK_QUARTZ_ALLOC_POOL;

  screens = [NSScreen screens];

  for (i = 0; i < [screens count]; i++)
    {
      NSRect rect = [[screens objectAtIndex:i] frame];

      if (x >= rect.origin.x && x <= rect.origin.x + rect.size.width &&
          y >= rect.origin.y && y <= rect.origin.y + rect.size.height)
        {
          screen = [screens objectAtIndex:i];
          break;
        }
    }

  GDK_QUARTZ_RELEASE_POOL;

  return screen;
}

void
_gdk_quartz_display_create_surface_impl (GdkDisplay    *display,
                                        GdkSurface     *window,
                                        GdkSurface     *real_parent)
{
  GdkSurfaceImplQuartz *impl;
  GdkSurfaceImplQuartz *parent_impl;

  GDK_QUARTZ_ALLOC_POOL;

  impl = g_object_new (GDK_TYPE_SURFACE_IMPL_QUARTZ, NULL);
  window->impl = GDK_SURFACE_IMPL (impl);
  impl->wrapper = window;

  parent_impl = GDK_SURFACE_IMPL_QUARTZ (window->parent->impl);

  switch (window->surface_type)
    {
    case GDK_SURFACE_TOPLEVEL:
    case GDK_SURFACE_TEMP:
      if (GDK_SURFACE_TYPE (window->parent) != GDK_SURFACE_ROOT)
	{
	  /* The common code warns for this case */
          parent_impl = GDK_SURFACE_IMPL_QUARTZ (_gdk_root->impl);
	}
    }

  /* Maintain the z-ordered list of children. */
  if (window->parent != _gdk_root)
    parent_impl->sorted_children = g_list_prepend (parent_impl->sorted_children, window);
  else
    clear_toplevel_order ();

  impl->view = NULL;

  switch (window->surface_type)
    {
    case GDK_SURFACE_TOPLEVEL:
    case GDK_SURFACE_TEMP:
      {
        NSScreen *screen;
        NSRect screen_rect;
        NSRect content_rect;
        NSUInteger style_mask;
        int nx, ny;

        /* initWithContentRect will place on the mainScreen by default.
         * We want to select the screen to place on ourselves.  We need
         * to find the screen the window will be on and correct the
         * content_rect coordinates to be relative to that screen.
         */
        _gdk_quartz_surface_gdk_xy_to_xy (window->x, window->y, &nx, &ny);

        screen = get_nsscreen_for_point (nx, ny);
        screen_rect = [screen frame];
        nx -= screen_rect.origin.x;
        ny -= screen_rect.origin.y;

        content_rect = NSMakeRect (nx, ny - window->height,
                                   window->width,
                                   window->height);

        if (window->surface_type == GDK_SURFACE_TEMP)
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

	impl->toplevel = [[GdkQuartzNSWindow alloc] initWithContentRect:content_rect 
			                                      styleMask:style_mask
			                                        backing:NSBackingStoreBuffered
			                                          defer:NO
                                                                  screen:screen];

	gdk_surface_set_title (window, get_default_title ());
  
        [impl->toplevel setOpaque:NO];
        [impl->toplevel setBackgroundColor:[NSColor clearColor]];

        content_rect.origin.x = 0;
        content_rect.origin.y = 0;

	impl->view = [[GdkQuartzView alloc] initWithFrame:content_rect];
	[impl->view setGdkSurface:window];
	[impl->toplevel setContentView:impl->view];
	[impl->view release];
      }
      break;

    default:
      g_assert_not_reached ();
    }

  GDK_QUARTZ_RELEASE_POOL;
}

void
_gdk_quartz_surface_update_position (GdkSurface *window)
{
  NSRect frame_rect;
  NSRect content_rect;
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  GDK_QUARTZ_ALLOC_POOL;

  frame_rect = [impl->toplevel frame];
  content_rect = [impl->toplevel contentRectForFrameRect:frame_rect];

  _gdk_quartz_surface_xy_to_gdk_xy (content_rect.origin.x,
                                   content_rect.origin.y + content_rect.size.height,
                                   &window->x, &window->y);


  GDK_QUARTZ_RELEASE_POOL;
}

void
_gdk_quartz_surface_init_windowing (GdkDisplay *display)
{
  GdkSurfaceImplQuartz *impl;

  g_assert (_gdk_root == NULL);

  _gdk_root = _gdk_display_create_window (display);

  _gdk_root->impl = g_object_new (_gdk_root_surface_impl_quartz_get_type (), NULL);
  _gdk_root->impl_surface = _gdk_root;

  impl = GDK_SURFACE_IMPL_QUARTZ (_gdk_root->impl);

  _gdk_quartz_screen_update_window_sizes (screen);

  _gdk_root->state = 0; /* We don't want GDK_SURFACE_STATE_WITHDRAWN here */
  _gdk_root->surface_type = GDK_SURFACE_ROOT;
  _gdk_root->viewable = TRUE;

  impl->wrapper = _gdk_root;
}

static void
gdk_quartz_surface_destroy (GdkSurface *window,
                           gboolean   recursing,
                           gboolean   foreign_destroy)
{
  GdkSurfaceImplQuartz *impl;
  GdkSurface *parent;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  main_window_stack = g_slist_remove (main_window_stack, window);

  g_list_free (impl->sorted_children);
  impl->sorted_children = NULL;

  parent = window->parent;
  if (parent)
    {
      GdkSurfaceImplQuartz *parent_impl = GDK_SURFACE_IMPL_QUARTZ (parent->impl);

      parent_impl->sorted_children = g_list_remove (parent_impl->sorted_children, window);
    }

  if (impl->cairo_surface)
    {
      cairo_surface_finish (impl->cairo_surface);
      cairo_surface_set_user_data (impl->cairo_surface, &gdk_quartz_cairo_key,
				   NULL, NULL);
      impl->cairo_surface = NULL;
    }

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

/* FIXME: This might be possible to simplify with client-side windows. Also
 * note that already_mapped is not used yet, see the x11 backend.
*/
static void
gdk_surface_quartz_show (GdkSurface *window, gboolean already_mapped)
{
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
  gboolean focus_on_map;

  GDK_QUARTZ_ALLOC_POOL;

  if (!GDK_SURFACE_IS_MAPPED (window))
    focus_on_map = window->focus_on_map;
  else
    focus_on_map = TRUE;

  if (SURFACE_IS_TOPLEVEL (window) && impl->toplevel)
    {
      gboolean make_key;

      make_key = (window->accept_focus && focus_on_map &&
                  window->surface_type != GDK_SURFACE_TEMP);

      [(GdkQuartzNSWindow*)impl->toplevel showAndMakeKey:make_key];
      clear_toplevel_order ();

      _gdk_quartz_events_send_map_event (window);
    }
  else
    {
      [impl->view setHidden:NO];
    }

  [impl->view setNeedsDisplay:YES];

  gdk_synthesize_surface_state (window, GDK_SURFACE_STATE_WITHDRAWN, 0);

  if (window->state & GDK_SURFACE_STATE_MAXIMIZED)
    gdk_surface_maximize (window);

  if (window->state & GDK_SURFACE_STATE_ICONIFIED)
    gdk_surface_iconify (window);

  if (impl->transient_for && !GDK_SURFACE_DESTROYED (impl->transient_for))
    _gdk_quartz_surface_attach_to_parent (window);

  GDK_QUARTZ_RELEASE_POOL;
}

/* Temporarily unsets the parent window, if the window is a
 * transient. 
 */
void
_gdk_quartz_surface_detach_from_parent (GdkSurface *window)
{
  GdkSurfaceImplQuartz *impl;

  g_return_if_fail (GDK_IS_SURFACE (window));

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
  
  g_return_if_fail (impl->toplevel != NULL);

  if (impl->transient_for && !GDK_SURFACE_DESTROYED (impl->transient_for))
    {
      GdkSurfaceImplQuartz *parent_impl;

      parent_impl = GDK_SURFACE_IMPL_QUARTZ (impl->transient_for->impl);
      [parent_impl->toplevel removeChildWindow:impl->toplevel];
      clear_toplevel_order ();
    }
}

/* Re-sets the parent window, if the window is a transient. */
void
_gdk_quartz_surface_attach_to_parent (GdkSurface *window)
{
  GdkSurfaceImplQuartz *impl;

  g_return_if_fail (GDK_IS_SURFACE (window));

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
  
  g_return_if_fail (impl->toplevel != NULL);

  if (impl->transient_for && !GDK_SURFACE_DESTROYED (impl->transient_for))
    {
      GdkSurfaceImplQuartz *parent_impl;

      parent_impl = GDK_SURFACE_IMPL_QUARTZ (impl->transient_for->impl);
      [parent_impl->toplevel addChildWindow:impl->toplevel ordered:NSWindowAbove];
      clear_toplevel_order ();
    }
}

void
gdk_surface_quartz_hide (GdkSurface *window)
{
  GdkSurfaceImplQuartz *impl;

  /* Make sure we're not stuck in fullscreen mode. */
#ifndef AVAILABLE_MAC_OS_X_VERSION_10_7_AND_LATER
  if (get_fullscreen_geometry (window))
    SetSystemUIMode (kUIModeNormal, 0);
#endif

  _gdk_surface_clear_update_area (window);

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (SURFACE_IS_TOPLEVEL (window)) 
    {
     /* Update main window. */
      main_window_stack = g_slist_remove (main_window_stack, window);
      if ([NSApp mainWindow] == impl->toplevel)
        _gdk_quartz_surface_did_resign_main (window);

      if (impl->transient_for)
        _gdk_quartz_surface_detach_from_parent (window);

      [(GdkQuartzNSWindow*)impl->toplevel hide];
    }
  else if (impl->view)
    {
      [impl->view setHidden:YES];
    }
}

void
gdk_surface_quartz_withdraw (GdkSurface *window)
{
  gdk_surface_hide (window);
}

static void
move_resize_window_internal (GdkSurface *window,
			     gint       x,
			     gint       y,
			     gint       width,
			     gint       height)
{
  GdkSurfaceImplQuartz *impl;
  GdkRectangle old_visible;
  GdkRectangle new_visible;
  GdkRectangle scroll_rect;
  cairo_region_t *old_region;
  cairo_region_t *expose_region;
  NSSize delta;

  if (GDK_SURFACE_DESTROYED (window))
    return;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if ((x == -1 || (x == window->x)) &&
      (y == -1 || (y == window->y)) &&
      (width == -1 || (width == window->width)) &&
      (height == -1 || (height == window->height)))
    {
      return;
    }

  if (!impl->toplevel)
    {
      /* The previously visible area of this window in a coordinate
       * system rooted at the origin of this window.
       */
      old_visible.x = -window->x;
      old_visible.y = -window->y;

      old_visible.width = window->width;
      old_visible.height = window->height;
    }

  if (x != -1)
    {
      delta.width = x - window->x;
      window->x = x;
    }
  else
    {
      delta.width = 0;
    }

  if (y != -1)
    {
      delta.height = y - window->y;
      window->y = y;
    }
  else
    {
      delta.height = 0;
    }

  if (width != -1)
    window->width = width;

  if (height != -1)
    window->height = height;

  GDK_QUARTZ_ALLOC_POOL;

  if (impl->toplevel)
    {
      NSRect content_rect;
      NSRect frame_rect;
      gint gx, gy;

      _gdk_quartz_surface_gdk_xy_to_xy (window->x, window->y + window->height,
                                       &gx, &gy);

      content_rect = NSMakeRect (gx, gy, window->width, window->height);

      frame_rect = [impl->toplevel frameRectForContentRect:content_rect];
      [impl->toplevel setFrame:frame_rect display:YES];
    }
  else 
    {
      NSRect nsrect;

      nsrect = NSMakeRect (window->x, window->y, window->width, window->height);

      /* The newly visible area of this window in a coordinate
       * system rooted at the origin of this window.
       */
      new_visible.x = -window->x;
      new_visible.y = -window->y;
      new_visible.width = old_visible.width;   /* parent has not changed size */
      new_visible.height = old_visible.height; /* parent has not changed size */

      expose_region = cairo_region_create_rectangle (&new_visible);
      old_region = cairo_region_create_rectangle (&old_visible);
      cairo_region_subtract (expose_region, old_region);

      /* Determine what (if any) part of the previously visible
       * part of the window can be copied without a redraw
       */
      scroll_rect = old_visible;
      scroll_rect.x -= delta.width;
      scroll_rect.y -= delta.height;
      gdk_rectangle_intersect (&scroll_rect, &old_visible, &scroll_rect);

      if (!cairo_region_is_empty (expose_region))
        {
          if (scroll_rect.width != 0 && scroll_rect.height != 0)
            {
              [impl->view scrollRect:NSMakeRect (scroll_rect.x,
                                                 scroll_rect.y,
                                                 scroll_rect.width,
                                                 scroll_rect.height)
			              by:delta];
            }

          [impl->view setFrame:nsrect];

          gdk_quartz_surface_set_needs_display_in_region (window, expose_region);
        }
      else
        {
          [impl->view setFrame:nsrect];
          [impl->view setNeedsDisplay:YES];
        }

      cairo_region_destroy (expose_region);
      cairo_region_destroy (old_region);
    }

  GDK_QUARTZ_RELEASE_POOL;
}

static inline void
window_quartz_move (GdkSurface *window,
                    gint       x,
                    gint       y)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (window->state & GDK_SURFACE_STATE_FULLSCREEN)
    return;

  move_resize_window_internal (window, x, y, -1, -1);
}

static inline void
window_quartz_resize (GdkSurface *window,
                      gint       width,
                      gint       height)
{
  g_return_if_fail (GDK_IS_SURFACE (window));

  if (window->state & GDK_SURFACE_STATE_FULLSCREEN)
    return;

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  move_resize_window_internal (window, -1, -1, width, height);
}

static inline void
window_quartz_move_resize (GdkSurface *window,
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
gdk_surface_quartz_move_resize (GdkSurface *window,
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

static void
gdk_surface_quartz_toplevel_resize (GdkSurface *surface,
                                    gint        width,
                                    gint        height)
{
  window_quartz_resize (window, width, height);
}

/* Get the toplevel ordering from NSApp and update our own list. We do
 * this on demand since the NSApp’s list is not up to date directly
 * after we get windowDidBecomeMain.
 */
static void
update_toplevel_order (void)
{
  GdkSurfaceImplQuartz *root_impl;
  NSEnumerator *enumerator;
  id nswindow;
  GList *toplevels = NULL;

  root_impl = GDK_SURFACE_IMPL_QUARTZ (_gdk_root->impl);

  if (root_impl->sorted_children)
    return;

  GDK_QUARTZ_ALLOC_POOL;

  enumerator = [[NSApp orderedWindows] objectEnumerator];
  while ((nswindow = [enumerator nextObject]))
    {
      GdkSurface *window;

      if (![[nswindow contentView] isKindOfClass:[GdkQuartzView class]])
        continue;

      window = [(GdkQuartzView *)[nswindow contentView] gdkSurface];
      toplevels = g_list_prepend (toplevels, window);
    }

  GDK_QUARTZ_RELEASE_POOL;

  root_impl->sorted_children = g_list_reverse (toplevels);
}

static void
clear_toplevel_order (void)
{
  GdkSurfaceImplQuartz *root_impl;

  root_impl = GDK_SURFACE_IMPL_QUARTZ (_gdk_root->impl);

  g_list_free (root_impl->sorted_children);
  root_impl->sorted_children = NULL;
}

static void
gdk_surface_quartz_raise (GdkSurface *window)
{
  if (GDK_SURFACE_DESTROYED (window))
    return;

  if (SURFACE_IS_TOPLEVEL (window))
    {
      GdkSurfaceImplQuartz *impl;

      impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
      [impl->toplevel orderFront:impl->toplevel];

      clear_toplevel_order ();
    }
  else
    {
      GdkSurface *parent = window->parent;

      if (parent)
        {
          GdkSurfaceImplQuartz *impl;

          impl = (GdkSurfaceImplQuartz *)parent->impl;

          impl->sorted_children = g_list_remove (impl->sorted_children, window);
          impl->sorted_children = g_list_prepend (impl->sorted_children, window);
        }
    }
}

static void
gdk_surface_quartz_lower (GdkSurface *window)
{
  if (GDK_SURFACE_DESTROYED (window))
    return;

  if (SURFACE_IS_TOPLEVEL (window))
    {
      GdkSurfaceImplQuartz *impl;

      impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
      [impl->toplevel orderBack:impl->toplevel];

      clear_toplevel_order ();
    }
  else
    {
      GdkSurface *parent = window->parent;

      if (parent)
        {
          GdkSurfaceImplQuartz *impl;

          impl = (GdkSurfaceImplQuartz *)parent->impl;

          impl->sorted_children = g_list_remove (impl->sorted_children, window);
          impl->sorted_children = g_list_append (impl->sorted_children, window);
        }
    }
}

static void
gdk_surface_quartz_restack_toplevel (GdkSurface *window,
				    GdkSurface *sibling,
				    gboolean   above)
{
  GdkSurfaceImplQuartz *impl;
  gint sibling_num;

  impl = GDK_SURFACE_IMPL_QUARTZ (sibling->impl);
  sibling_num = [impl->toplevel windowNumber];

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (above)
    [impl->toplevel orderWindow:NSWindowAbove relativeTo:sibling_num];
  else
    [impl->toplevel orderWindow:NSWindowBelow relativeTo:sibling_num];
}

static void
gdk_surface_quartz_get_geometry (GdkSurface *window,
                                gint      *x,
                                gint      *y,
                                gint      *width,
                                gint      *height)
{
  GdkSurfaceImplQuartz *impl;
  NSRect ns_rect;

  if (GDK_SURFACE_DESTROYED (window))
    return;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
  if (window == _gdk_root)
    {
      if (x) 
        *x = 0;
      if (y) 
        *y = 0;

      if (width) 
        *width = window->width;
      if (height)
        *height = window->height;
    }
  else if (SURFACE_IS_TOPLEVEL (window))
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
          _gdk_quartz_surface_xy_to_gdk_xy (ns_rect.origin.x,
                                           ns_rect.origin.y + ns_rect.size.height,
                                           x, y);
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
}

static void
gdk_surface_quartz_get_root_coords (GdkSurface *window,
                                   gint       x,
                                   gint       y,
                                   gint      *root_x,
                                   gint      *root_y)
{
  int tmp_x = 0, tmp_y = 0;
  GdkSurface *toplevel;
  NSRect content_rect;
  GdkSurfaceImplQuartz *impl;

  if (GDK_SURFACE_DESTROYED (window)) 
    {
      if (root_x)
	*root_x = 0;
      if (root_y)
	*root_y = 0;
      
      return;
    }

  if (window == _gdk_root)
    {
      if (root_x)
        *root_x = x;
      if (root_y)
        *root_y = y;

      return;
    }
  
  toplevel = gdk_surface_get_toplevel (window);
  impl = GDK_SURFACE_IMPL_QUARTZ (toplevel->impl);

  content_rect = [impl->toplevel contentRectForFrameRect:[impl->toplevel frame]];

  _gdk_quartz_surface_xy_to_gdk_xy (content_rect.origin.x,
                                   content_rect.origin.y + content_rect.size.height,
                                   &tmp_x, &tmp_y);

  tmp_x += x;
  tmp_y += y;

  while (window != toplevel)
    {
      tmp_x += window->x;
      tmp_y += window->y;

      window = window->parent;
    }

  if (root_x)
    *root_x = tmp_x;
  if (root_y)
    *root_y = tmp_y;
}

/* Returns coordinates relative to the passed in window. */
static GdkSurface *
gdk_surface_quartz_get_device_state_helper (GdkSurface       *window,
                                           GdkDevice       *device,
                                           gdouble         *x,
                                           gdouble         *y,
                                           GdkModifierType *mask)
{
  NSPoint point;
  gint x_tmp, y_tmp;
  GdkSurface *toplevel;
  GdkSurface *found_window;

  g_return_val_if_fail (window == NULL || GDK_IS_SURFACE (window), NULL);

  if (GDK_SURFACE_DESTROYED (window))
    {
      *x = 0;
      *y = 0;
      *mask = 0;
      return NULL;
    }
  
  toplevel = gdk_surface_get_toplevel (window);

  *mask = _gdk_quartz_events_get_current_keyboard_modifiers () |
      _gdk_quartz_events_get_current_mouse_modifiers ();

  /* Get the y coordinate, needs to be flipped. */
  if (window == _gdk_root)
    {
      point = [NSEvent mouseLocation];
      _gdk_quartz_surface_nspoint_to_gdk_xy (point, &x_tmp, &y_tmp);
    }
  else
    {
      GdkSurfaceImplQuartz *impl;
      NSWindow *nswindow;

      impl = GDK_SURFACE_IMPL_QUARTZ (toplevel->impl);
      nswindow = impl->toplevel;

      point = [nswindow mouseLocationOutsideOfEventStream];

      x_tmp = point.x;
      y_tmp = toplevel->height - point.y;

      window = (GdkSurface *)toplevel;
    }

  found_window = _gdk_quartz_surface_find_child (window, x_tmp, y_tmp,
                                                FALSE);

  /* We never return the root window. */
  if (found_window == _gdk_root)
    found_window = NULL;

  *x = x_tmp;
  *y = y_tmp;

  return found_window;
}

static gboolean
gdk_surface_quartz_get_device_state (GdkSurface       *window,
                                    GdkDevice       *device,
                                    gdouble          *x,
                                    gdouble          *y,
                                    GdkModifierType *mask)
{
  return gdk_surface_quartz_get_device_state_helper (window,
                                                    device,
                                                    x, y, mask) != NULL;
}

static void
gdk_quartz_surface_set_geometry_hints (GdkSurface         *window,
                                      const GdkGeometry *geometry,
                                      GdkSurfaceHints     geom_mask)
{
  GdkSurfaceImplQuartz *impl;

  g_return_if_fail (geometry != NULL);

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;
  
  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
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
      NSSize size;

      if (geometry->min_aspect != geometry->max_aspect)
        {
          g_warning ("Only equal minimum and maximum aspect ratios are supported on Mac OS. Using minimum aspect ratio...");
        }

      size.width = geometry->min_aspect;
      size.height = 1.0;

      [impl->toplevel setContentAspectRatio:size];
    }

  if (geom_mask & GDK_HINT_WIN_GRAVITY)
    {
      /* FIXME: Implement */
    }
}

static void
gdk_quartz_surface_set_title (GdkSurface   *window,
                             const gchar *title)
{
  GdkSurfaceImplQuartz *impl;

  g_return_if_fail (title != NULL);

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (impl->toplevel)
    {
      GDK_QUARTZ_ALLOC_POOL;
      [impl->toplevel setTitle:[NSString stringWithUTF8String:title]];
      GDK_QUARTZ_RELEASE_POOL;
    }
}

static void
gdk_quartz_surface_set_startup_id (GdkSurface   *window,
                                  const gchar *startup_id)
{
  /* FIXME: Implement? */
}

static void
gdk_quartz_surface_set_transient_for (GdkSurface *window,
                                     GdkSurface *parent)
{
  GdkSurfaceImplQuartz *surface_impl;
  GdkSurfaceImplQuartz *parent_impl;

  if (GDK_SURFACE_DESTROYED (window)  || GDK_SURFACE_DESTROYED (parent) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  surface_impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
  if (!surface_impl->toplevel)
    return;

  GDK_QUARTZ_ALLOC_POOL;

  if (surface_impl->transient_for)
    {
      _gdk_quartz_surface_detach_from_parent (window);

      g_object_unref (surface_impl->transient_for);
      surface_impl->transient_for = NULL;
    }

  parent_impl = GDK_SURFACE_IMPL_QUARTZ (parent->impl);
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
      if (gdk_surface_get_type_hint (window) != GDK_SURFACE_TYPE_HINT_TOOLTIP)
        {
          surface_impl->transient_for = g_object_ref (parent);

          /* We only add the window if it is shown, otherwise it will
           * be shown unconditionally here. If it is not shown, the
           * window will be added in show() instead.
           */
          if (!(window->state & GDK_SURFACE_STATE_WITHDRAWN))
            _gdk_quartz_surface_attach_to_parent (window);
        }
    }
  
  GDK_QUARTZ_RELEASE_POOL;
}

static void
gdk_surface_quartz_input_shape_combine_region (GdkSurface       *window,
                                              const cairo_region_t *shape_region,
                                              gint             offset_x,
                                              gint             offset_y)
{
  /* FIXME: Implement */
}

static void
gdk_quartz_surface_set_accept_focus (GdkSurface *window,
                                    gboolean accept_focus)
{
  window->accept_focus = accept_focus != FALSE;
}

static void
gdk_quartz_surface_set_focus_on_map (GdkSurface *window,
                                    gboolean focus_on_map)
{
  window->focus_on_map = focus_on_map != FALSE;
}

static void
gdk_quartz_surface_set_icon_name (GdkSurface   *window,
                                 const gchar *name)
{
  /* FIXME: Implement */
}

static void
gdk_quartz_surface_focus (GdkSurface *window,
                         guint32    timestamp)
{
  GdkSurfaceImplQuartz *impl;
	
  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  if (window->accept_focus && window->surface_type != GDK_SURFACE_TEMP)
    {
      GDK_QUARTZ_ALLOC_POOL;
      [impl->toplevel makeKeyAndOrderFront:impl->toplevel];
      clear_toplevel_order ();
      GDK_QUARTZ_RELEASE_POOL;
    }
}

static gint
surface_type_hint_to_level (GdkSurfaceTypeHint hint)
{
  /*  the order in this switch statement corresponds to the actual
   *  stacking order: the first group is top, the last group is bottom
   */
  switch (hint)
    {
    case GDK_SURFACE_TYPE_HINT_POPUP_MENU:
    case GDK_SURFACE_TYPE_HINT_COMBO:
    case GDK_SURFACE_TYPE_HINT_DND:
    case GDK_SURFACE_TYPE_HINT_TOOLTIP:
      return NSPopUpMenuWindowLevel;

    case GDK_SURFACE_TYPE_HINT_NOTIFICATION:
    case GDK_SURFACE_TYPE_HINT_SPLASHSCREEN:
      return NSStatusWindowLevel;

    case GDK_SURFACE_TYPE_HINT_MENU: /* Torn-off menu */
    case GDK_SURFACE_TYPE_HINT_DROPDOWN_MENU: /* Menu from menubar */
      return NSTornOffMenuWindowLevel;

    case GDK_SURFACE_TYPE_HINT_DOCK:
      return NSFloatingWindowLevel; /* NSDockWindowLevel is deprecated, and not replaced */

    case GDK_SURFACE_TYPE_HINT_UTILITY:
    case GDK_SURFACE_TYPE_HINT_DIALOG:  /* Dialog window */
    case GDK_SURFACE_TYPE_HINT_NORMAL:  /* Normal toplevel window */
    case GDK_SURFACE_TYPE_HINT_TOOLBAR: /* Window used to implement toolbars */
      return NSNormalWindowLevel;

    case GDK_SURFACE_TYPE_HINT_DESKTOP:
      return kCGDesktopWindowLevelKey; /* doesn't map to any real Cocoa model */

    default:
      break;
    }

  return NSNormalWindowLevel;
}

static gboolean
surface_type_hint_to_shadow (GdkSurfaceTypeHint hint)
{
  switch (hint)
    {
    case GDK_SURFACE_TYPE_HINT_NORMAL:  /* Normal toplevel window */
    case GDK_SURFACE_TYPE_HINT_DIALOG:  /* Dialog window */
    case GDK_SURFACE_TYPE_HINT_DOCK:
    case GDK_SURFACE_TYPE_HINT_UTILITY:
    case GDK_SURFACE_TYPE_HINT_MENU: /* Torn-off menu */
    case GDK_SURFACE_TYPE_HINT_DROPDOWN_MENU: /* Menu from menubar */
    case GDK_SURFACE_TYPE_HINT_SPLASHSCREEN:
    case GDK_SURFACE_TYPE_HINT_POPUP_MENU:
    case GDK_SURFACE_TYPE_HINT_COMBO:
    case GDK_SURFACE_TYPE_HINT_NOTIFICATION:
    case GDK_SURFACE_TYPE_HINT_TOOLTIP:
      return TRUE;

    case GDK_SURFACE_TYPE_HINT_TOOLBAR: /* Window used to implement toolbars */
    case GDK_SURFACE_TYPE_HINT_DESKTOP: /* N/A */
    case GDK_SURFACE_TYPE_HINT_DND:
      break;

    default:
      break;
    }

  return FALSE;
}

static gboolean
surface_type_hint_to_hides_on_deactivate (GdkSurfaceTypeHint hint)
{
  switch (hint)
    {
    case GDK_SURFACE_TYPE_HINT_UTILITY:
    case GDK_SURFACE_TYPE_HINT_MENU: /* Torn-off menu */
    case GDK_SURFACE_TYPE_HINT_SPLASHSCREEN:
    case GDK_SURFACE_TYPE_HINT_NOTIFICATION:
    case GDK_SURFACE_TYPE_HINT_TOOLTIP:
      return TRUE;

    default:
      break;
    }

  return FALSE;
}

static void
_gdk_quartz_surface_update_has_shadow (GdkSurfaceImplQuartz *impl)
{
    gboolean has_shadow;

    /* In case there is any shadow set we have to turn off the
     * NSWindow setHasShadow as the system drawn ones wont match our
     * window boundary anymore */
    has_shadow = (surface_type_hint_to_shadow (impl->type_hint) && !impl->shadow_max);

    [impl->toplevel setHasShadow: has_shadow];
}

static void
gdk_quartz_surface_set_type_hint (GdkSurface        *window,
                                 GdkSurfaceTypeHint hint)
{
  GdkSurfaceImplQuartz *impl;

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  impl->type_hint = hint;

  /* Match the documentation, only do something if we're not mapped yet. */
  if (GDK_SURFACE_IS_MAPPED (window))
    return;

  _gdk_quartz_surface_update_has_shadow (impl);
  [impl->toplevel setLevel: surface_type_hint_to_level (hint)];
  [impl->toplevel setHidesOnDeactivate: surface_type_hint_to_hides_on_deactivate (hint)];
}

static GdkSurfaceTypeHint
gdk_quartz_surface_get_type_hint (GdkSurface *window)
{
  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return GDK_SURFACE_TYPE_HINT_NORMAL;
  
  return GDK_SURFACE_IMPL_QUARTZ (window->impl)->type_hint;
}

static void
gdk_quartz_surface_set_modal_hint (GdkSurface *window,
                                  gboolean   modal)
{
  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  /* FIXME: Implement */
}

static void
gdk_quartz_surface_begin_resize_drag (GdkSurface     *window,
                                     GdkSurfaceEdge  edge,
                                     GdkDevice     *device,
                                     gint           button,
                                     gint           root_x,
                                     gint           root_y,
                                     guint32        timestamp)
{
  GdkSurfaceImplQuartz *impl;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window))
    return;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (!impl->toplevel)
    {
      g_warning ("Can't call gdk_surface_begin_resize_drag on non-toplevel window");
      return;
    }

  [(GdkQuartzNSWindow *)impl->toplevel beginManualResize:edge];
}

static void
gdk_quartz_surface_begin_move_drag (GdkSurface *window,
                                   GdkDevice *device,
                                   gint       button,
                                   gint       root_x,
                                   gint       root_y,
                                   guint32    timestamp)
{
  GdkSurfaceImplQuartz *impl;

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (!impl->toplevel)
    {
      g_warning ("Can't call gdk_surface_begin_move_drag on non-toplevel window");
      return;
    }

  [(GdkQuartzNSWindow *)impl->toplevel beginManualMove];
}

static void
gdk_quartz_surface_set_icon_list (GdkSurface *window,
                                 GList     *surfaces)
{
  /* FIXME: Implement */
}

static void
gdk_quartz_surface_get_frame_extents (GdkSurface    *window,
                                     GdkRectangle *rect)
{
  GdkSurface *toplevel;
  GdkSurfaceImplQuartz *impl;
  NSRect ns_rect;

  g_return_if_fail (rect != NULL);


  rect->x = 0;
  rect->y = 0;
  rect->width = 1;
  rect->height = 1;
  
  toplevel = gdk_surface_get_toplevel (window);
  impl = GDK_SURFACE_IMPL_QUARTZ (toplevel->impl);

  ns_rect = [impl->toplevel frame];

  _gdk_quartz_surface_xy_to_gdk_xy (ns_rect.origin.x,
                                   ns_rect.origin.y + ns_rect.size.height,
                                   &rect->x, &rect->y);

  rect->width = ns_rect.size.width;
  rect->height = ns_rect.size.height;
}

/* Fake protocol to make gcc think that it's OK to call setStyleMask
   even if it isn't. We check to make sure before actually calling
   it. */

@protocol CanSetStyleMask
- (void)setStyleMask:(int)mask;
@end

static void
gdk_quartz_surface_set_decorations (GdkSurface       *window,
			    GdkWMDecoration  decorations)
{
  GdkSurfaceImplQuartz *impl;
  NSUInteger old_mask, new_mask;
  NSView *old_view;

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (decorations == 0 || GDK_SURFACE_TYPE (window) == GDK_SURFACE_TEMP ||
      impl->type_hint == GDK_SURFACE_TYPE_HINT_SPLASHSCREEN )
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

  if (old_mask != new_mask)
    {
      NSRect rect;

      old_view = [[impl->toplevel contentView] retain];

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

      /* Note, before OS 10.6 there doesn't seem to be a way to change this
       * without recreating the toplevel. From 10.6 onward, a simple call to
       * setStyleMask takes care of most of this, except for ensuring that the
       * title is set.
       */
      if ([impl->toplevel respondsToSelector:@selector(setStyleMask:)])
        {
          NSString *title = [impl->toplevel title];

          [(id<CanSetStyleMask>)impl->toplevel setStyleMask:new_mask];

          /* It appears that unsetting and then resetting NSTitledWindowMask
           * does not reset the title in the title bar as might be expected.
           *
           * In theory we only need to set this if new_mask includes
           * NSTitledWindowMask. This behaved extremely oddly when
           * conditionalized upon that and since it has no side effects (i.e.
           * if NSTitledWindowMask is not requested, the title will not be
           * displayed) just do it unconditionally. We also must null check
           * 'title' before setting it to avoid crashing.
           */
          if (title)
            [impl->toplevel setTitle:title];
        }
      else
        {
          NSString *title = [impl->toplevel title];
          NSColor *bg = [impl->toplevel backgroundColor];
          NSScreen *screen = [impl->toplevel screen];

          /* Make sure the old window is closed, recall that releasedWhenClosed
           * is set on GdkQuartzSurfaces.
           */
          [impl->toplevel close];

          impl->toplevel = [[GdkQuartzNSWindow alloc] initWithContentRect:rect
                                                                styleMask:new_mask
                                                                  backing:NSBackingStoreBuffered
                                                                    defer:NO
                                                                   screen:screen];
          _gdk_quartz_surface_update_has_shadow (impl);

          [impl->toplevel setLevel: surface_type_hint_to_level (impl->type_hint)];
          if (title)
            [impl->toplevel setTitle:title];
          [impl->toplevel setBackgroundColor:bg];
          [impl->toplevel setHidesOnDeactivate: surface_type_hint_to_hides_on_deactivate (impl->type_hint)];
          [impl->toplevel setContentView:old_view];
        }

      if (new_mask == NSBorderlessWindowMask)
        {
          [impl->toplevel setContentSize:rect.size];
          [impl->toplevel setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
        }
      else
        [impl->toplevel setFrame:rect display:YES];

      /* Invalidate the window shadow for non-opaque views that have shadow
       * enabled, to get the shadow shape updated.
       */
      if (![old_view isOpaque] && [impl->toplevel hasShadow])
        [(GdkQuartzView*)old_view setNeedsInvalidateShadow:YES];

      [old_view release];
    }

  GDK_QUARTZ_RELEASE_POOL;
}

static gboolean
gdk_quartz_surface_get_decorations (GdkSurface       *window,
                                   GdkWMDecoration *decorations)
{
  GdkSurfaceImplQuartz *impl;

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return FALSE;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

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

static void
gdk_quartz_surface_set_functions (GdkSurface    *window,
                                 GdkWMFunction functions)
{
  GdkSurfaceImplQuartz *impl;
  gboolean min, max, close;

  g_return_if_fail (GDK_IS_SURFACE (window));

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (functions & GDK_FUNC_ALL)
    {
      min = !(functions & GDK_FUNC_MINIMIZE);
      max = !(functions & GDK_FUNC_MAXIMIZE);
      close = !(functions & GDK_FUNC_CLOSE);
    }
  else
    {
      min = (functions & GDK_FUNC_MINIMIZE);
      max = (functions & GDK_FUNC_MAXIMIZE);
      close = (functions & GDK_FUNC_CLOSE);
    }

  if (impl->toplevel)
    {
      NSUInteger mask = [impl->toplevel styleMask];

      if (min)
        mask = mask | NSMiniaturizableWindowMask;
      else
        mask = mask & ~NSMiniaturizableWindowMask;

      if (max)
        mask = mask | NSResizableWindowMask;
      else
        mask = mask & ~NSResizableWindowMask;

      if (close)
        mask = mask | NSClosableWindowMask;
      else
        mask = mask & ~NSClosableWindowMask;

      [impl->toplevel setStyleMask:mask];
    }
}

static void
gdk_quartz_surface_stick (GdkSurface *window)
{
  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;
}

static void
gdk_quartz_surface_unstick (GdkSurface *window)
{
  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;
}

static void
gdk_quartz_surface_maximize (GdkSurface *window)
{
  GdkSurfaceImplQuartz *impl;
  gboolean maximized;

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
  maximized = gdk_surface_get_state (window) & GDK_SURFACE_STATE_MAXIMIZED;

  if (GDK_SURFACE_IS_MAPPED (window))
    {
      GDK_QUARTZ_ALLOC_POOL;

      if (impl->toplevel && !maximized)
        [impl->toplevel zoom:nil];

      GDK_QUARTZ_RELEASE_POOL;
    }
}

static void
gdk_quartz_surface_unmaximize (GdkSurface *window)
{
  GdkSurfaceImplQuartz *impl;
  gboolean maximized;

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
  maximized = gdk_surface_get_state (window) & GDK_SURFACE_STATE_MAXIMIZED;

  if (GDK_SURFACE_IS_MAPPED (window))
    {
      GDK_QUARTZ_ALLOC_POOL;

      if (impl->toplevel && maximized)
        [impl->toplevel zoom:nil];

      GDK_QUARTZ_RELEASE_POOL;
    }
}

static void
gdk_quartz_surface_iconify (GdkSurface *window)
{
  GdkSurfaceImplQuartz *impl;

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (GDK_SURFACE_IS_MAPPED (window))
    {
      GDK_QUARTZ_ALLOC_POOL;

      if (impl->toplevel)
	[impl->toplevel miniaturize:nil];

      GDK_QUARTZ_RELEASE_POOL;
    }
  else
    {
      gdk_synthesize_surface_state (window,
				   0,
				   GDK_SURFACE_STATE_ICONIFIED);
    }
}

static void
gdk_quartz_surface_deiconify (GdkSurface *window)
{
  GdkSurfaceImplQuartz *impl;

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (GDK_SURFACE_IS_MAPPED (window))
    {
      GDK_QUARTZ_ALLOC_POOL;

      if (impl->toplevel)
	[impl->toplevel deminiaturize:nil];

      GDK_QUARTZ_RELEASE_POOL;
    }
  else
    {
      gdk_synthesize_surface_state (window,
				   GDK_SURFACE_STATE_ICONIFIED,
				   0);
    }
}

#ifdef AVAILABLE_MAC_OS_X_VERSION_10_7_AND_LATER

static gboolean
window_is_fullscreen (GdkSurface *window)
{
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  return ([impl->toplevel styleMask] & NSFullScreenWindowMask) != 0;
}

static void
gdk_quartz_surface_fullscreen (GdkSurface *window)
{
  GdkSurfaceImplQuartz *impl;

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (!window_is_fullscreen (window))
    [impl->toplevel toggleFullScreen:nil];
}

static void
gdk_quartz_surface_unfullscreen (GdkSurface *window)
{
  GdkSurfaceImplQuartz *impl;

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (window_is_fullscreen (window))
    [impl->toplevel toggleFullScreen:nil];
}

void
_gdk_quartz_surface_update_fullscreen_state (GdkSurface *window)
{
  gboolean is_fullscreen;
  gboolean was_fullscreen;

  is_fullscreen = window_is_fullscreen (window);
  was_fullscreen = (gdk_surface_get_state (window) & GDK_SURFACE_STATE_FULLSCREEN) != 0;

  if (is_fullscreen != was_fullscreen)
    {
      if (is_fullscreen)
        gdk_synthesize_surface_state (window, 0, GDK_SURFACE_STATE_FULLSCREEN);
      else
        gdk_synthesize_surface_state (window, GDK_SURFACE_STATE_FULLSCREEN, 0);
    }
}

#else

static FullscreenSavedGeometry *
get_fullscreen_geometry (GdkSurface *window)
{
  return g_object_get_data (G_OBJECT (window), FULLSCREEN_DATA);
}

static void
gdk_quartz_surface_fullscreen (GdkSurface *window)
{
  FullscreenSavedGeometry *geometry;
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
  NSRect frame;

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  geometry = get_fullscreen_geometry (window);
  if (!geometry)
    {
      geometry = g_new (FullscreenSavedGeometry, 1);

      geometry->x = window->x;
      geometry->y = window->y;
      geometry->width = window->width;
      geometry->height = window->height;

      if (!gdk_surface_get_decorations (window, &geometry->decor))
        geometry->decor = GDK_DECOR_ALL;

      g_object_set_data_full (G_OBJECT (window),
                              FULLSCREEN_DATA, geometry, 
                              g_free);

      gdk_surface_set_decorations (window, 0);

      frame = [[impl->toplevel screen] frame];
      move_resize_window_internal (window,
                                   0, 0, 
                                   frame.size.width, frame.size.height);
      [impl->toplevel setContentSize:frame.size];
      [impl->toplevel makeKeyAndOrderFront:impl->toplevel];

      clear_toplevel_order ();
    }

  SetSystemUIMode (kUIModeAllHidden, kUIOptionAutoShowMenuBar);

  gdk_synthesize_surface_state (window, 0, GDK_SURFACE_STATE_FULLSCREEN);
}

static void
gdk_quartz_surface_unfullscreen (GdkSurface *window)
{
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
  FullscreenSavedGeometry *geometry;

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  geometry = get_fullscreen_geometry (window);
  if (geometry)
    {
      SetSystemUIMode (kUIModeNormal, 0);

      move_resize_window_internal (window,
                                   geometry->x,
                                   geometry->y,
                                   geometry->width,
                                   geometry->height);
      
      gdk_surface_set_decorations (window, geometry->decor);

      g_object_set_data (G_OBJECT (window), FULLSCREEN_DATA, NULL);

      [impl->toplevel makeKeyAndOrderFront:impl->toplevel];
      clear_toplevel_order ();

      gdk_synthesize_surface_state (window, GDK_SURFACE_STATE_FULLSCREEN, 0);
    }
}

#endif

static void
gdk_quartz_surface_set_keep_above (GdkSurface *window,
                                  gboolean   setting)
{
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
  gint level;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  level = surface_type_hint_to_level (gdk_surface_get_type_hint (window));
  
  /* Adjust normal window level by one if necessary. */
  [impl->toplevel setLevel: level + (setting ? 1 : 0)];
}

static void
gdk_quartz_surface_set_keep_below (GdkSurface *window,
                                  gboolean   setting)
{
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
  gint level;

  g_return_if_fail (GDK_IS_SURFACE (window));

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;
  
  level = surface_type_hint_to_level (gdk_surface_get_type_hint (window));
  
  /* Adjust normal window level by one if necessary. */
  [impl->toplevel setLevel: level - (setting ? 1 : 0)];
}

static void
gdk_quartz_surface_destroy_notify (GdkSurface *window)
{
  check_grab_destroy (window);
}

static void
gdk_quartz_surface_set_opacity (GdkSurface *window,
                               gdouble    opacity)
{
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  g_return_if_fail (GDK_IS_SURFACE (window));
  g_return_if_fail (SURFACE_IS_TOPLEVEL (window));

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  if (opacity < 0)
    opacity = 0;
  else if (opacity > 1)
    opacity = 1;

  [impl->toplevel setAlphaValue: opacity];
}

static void
gdk_quartz_surface_set_shadow_width (GdkSurface *window,
                                    gint       left,
                                    gint       right,
                                    gint       top,
                                    gint       bottom)
{
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  g_return_if_fail (GDK_IS_SURFACE (window));
  g_return_if_fail (SURFACE_IS_TOPLEVEL (window));

  if (GDK_SURFACE_DESTROYED (window) ||
      !SURFACE_IS_TOPLEVEL (window))
    return;

  impl->shadow_top = top;
  impl->shadow_max = MAX (MAX (left, right), MAX (top, bottom));
  _gdk_quartz_surface_update_has_shadow (impl);
}

/* Protocol to build cleanly for OSX < 10.7 */
@protocol ScaleFactor
- (CGFloat) backingScaleFactor;
@end

static gint
gdk_quartz_surface_get_scale_factor (GdkSurface *window)
{
  GdkSurfaceImplQuartz *impl;

  if (GDK_SURFACE_DESTROYED (window))
    return 1;

  impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (impl->toplevel != NULL && gdk_quartz_osx_version() >= GDK_OSX_LION)
    return [(id <ScaleFactor>) impl->toplevel backingScaleFactor];

  return 1;
}

static void
gdk_surface_impl_quartz_class_init (GdkSurfaceImplQuartzClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceImplClass *impl_class = GDK_SURFACE_IMPL_CLASS (klass);
  GdkSurfaceImplQuartzClass *impl_quartz_class = GDK_SURFACE_IMPL_QUARTZ_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_surface_impl_quartz_finalize;

  impl_class->ref_cairo_surface = gdk_quartz_ref_cairo_surface;
  impl_class->show = gdk_surface_quartz_show;
  impl_class->hide = gdk_surface_quartz_hide;
  impl_class->withdraw = gdk_surface_quartz_withdraw;
  impl_class->raise = gdk_surface_quartz_raise;
  impl_class->lower = gdk_surface_quartz_lower;
  impl_class->restack_toplevel = gdk_surface_quartz_restack_toplevel;
  impl_class->move_resize = gdk_surface_quartz_move_resize;
  impl_class->toplevel_resize = gdk_surface_quartz_toplevel_resize;
  impl_class->get_geometry = gdk_surface_quartz_get_geometry;
  impl_class->get_root_coords = gdk_surface_quartz_get_root_coords;
  impl_class->get_device_state = gdk_surface_quartz_get_device_state;
  impl_class->input_shape_combine_region = gdk_surface_quartz_input_shape_combine_region;
  impl_class->destroy = gdk_quartz_surface_destroy;
  impl_class->begin_paint = gdk_surface_impl_quartz_begin_paint;
  impl_class->get_scale_factor = gdk_quartz_surface_get_scale_factor;

  impl_class->focus = gdk_quartz_surface_focus;
  impl_class->set_type_hint = gdk_quartz_surface_set_type_hint;
  impl_class->get_type_hint = gdk_quartz_surface_get_type_hint;
  impl_class->set_modal_hint = gdk_quartz_surface_set_modal_hint;
  impl_class->set_geometry_hints = gdk_quartz_surface_set_geometry_hints;
  impl_class->set_title = gdk_quartz_surface_set_title;
  impl_class->set_startup_id = gdk_quartz_surface_set_startup_id;
  impl_class->set_transient_for = gdk_quartz_surface_set_transient_for;
  impl_class->get_frame_extents = gdk_quartz_surface_get_frame_extents;
  impl_class->set_accept_focus = gdk_quartz_surface_set_accept_focus;
  impl_class->set_focus_on_map = gdk_quartz_surface_set_focus_on_map;
  impl_class->set_icon_list = gdk_quartz_surface_set_icon_list;
  impl_class->set_icon_name = gdk_quartz_surface_set_icon_name;
  impl_class->iconify = gdk_quartz_surface_iconify;
  impl_class->deiconify = gdk_quartz_surface_deiconify;
  impl_class->stick = gdk_quartz_surface_stick;
  impl_class->unstick = gdk_quartz_surface_unstick;
  impl_class->maximize = gdk_quartz_surface_maximize;
  impl_class->unmaximize = gdk_quartz_surface_unmaximize;
  impl_class->fullscreen = gdk_quartz_surface_fullscreen;
  impl_class->unfullscreen = gdk_quartz_surface_unfullscreen;
  impl_class->set_keep_above = gdk_quartz_surface_set_keep_above;
  impl_class->set_keep_below = gdk_quartz_surface_set_keep_below;
  impl_class->set_decorations = gdk_quartz_surface_set_decorations;
  impl_class->get_decorations = gdk_quartz_surface_get_decorations;
  impl_class->set_functions = gdk_quartz_surface_set_functions;
  impl_class->set_functions = gdk_quartz_surface_set_functions;
  impl_class->begin_resize_drag = gdk_quartz_surface_begin_resize_drag;
  impl_class->begin_move_drag = gdk_quartz_surface_begin_move_drag;
  impl_class->set_opacity = gdk_quartz_surface_set_opacity;
  impl_class->set_shadow_width = gdk_quartz_surface_set_shadow_width;
  impl_class->destroy_notify = gdk_quartz_surface_destroy_notify;
  impl_class->register_dnd = _gdk_quartz_surface_register_dnd;
  impl_class->drag_begin = _gdk_quartz_surface_drag_begin;
  impl_class->process_updates_recurse = _gdk_quartz_surface_process_updates_recurse;

  impl_class->create_gl_context = gdk_quartz_surface_create_gl_context;

  impl_quartz_class->get_context = gdk_surface_impl_quartz_get_context;
  impl_quartz_class->release_context = gdk_surface_impl_quartz_release_context;
}

GType
_gdk_surface_impl_quartz_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
	{
	  sizeof (GdkSurfaceImplQuartzClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  (GClassInitFunc) gdk_surface_impl_quartz_class_init,
	  NULL,           /* class_finalize */
	  NULL,           /* class_data */
	  sizeof (GdkSurfaceImplQuartz),
	  0,              /* n_preallocs */
	  (GInstanceInitFunc) gdk_surface_impl_quartz_init,
	};

      object_type = g_type_register_static (GDK_TYPE_SURFACE_IMPL,
                                            "GdkSurfaceImplQuartz",
                                            &object_info, 0);
    }

  return object_type;
}

CGContextRef
gdk_quartz_surface_get_context (GdkSurfaceImplQuartz  *window,
                               gboolean             antialias)
{
  if (!GDK_SURFACE_IMPL_QUARTZ_GET_CLASS (window)->get_context)
    {
      g_warning ("%s doesn't implement GdkSurfaceImplQuartzClass::get_context()",
                 G_OBJECT_TYPE_NAME (window));
      return NULL;
    }

  return GDK_SURFACE_IMPL_QUARTZ_GET_CLASS (window)->get_context (window, antialias);
}

void
gdk_quartz_surface_release_context (GdkSurfaceImplQuartz  *window,
                                   CGContextRef          cg_context)
{
  if (!GDK_SURFACE_IMPL_QUARTZ_GET_CLASS (window)->release_context)
    {
      g_warning ("%s doesn't implement GdkSurfaceImplQuartzClass::release_context()",
                 G_OBJECT_TYPE_NAME (window));
      return;
    }

  GDK_SURFACE_IMPL_QUARTZ_GET_CLASS (window)->release_context (window, cg_context);
}



static CGContextRef
gdk_root_surface_impl_quartz_get_context (GdkSurfaceImplQuartz *window,
                                         gboolean             antialias)
{
  CGColorSpaceRef colorspace;
  CGContextRef cg_context;
  GdkSurfaceImplQuartz *surface_impl = GDK_SURFACE_IMPL_QUARTZ (window);

  if (GDK_SURFACE_DESTROYED (surface_impl->wrapper))
    return NULL;

  /* We do not have the notion of a root window on OS X.  We fake this
   * by creating a 1x1 bitmap and return a context to that.
   */
  colorspace = CGColorSpaceCreateWithName (kCGColorSpaceGenericRGB);
  cg_context = CGBitmapContextCreate (NULL,
                                      1, 1, 8, 4, colorspace,
                                      kCGImageAlphaPremultipliedLast);
  CGColorSpaceRelease (colorspace);

  return cg_context;
}

static void
gdk_root_surface_impl_quartz_release_context (GdkSurfaceImplQuartz *window,
                                             CGContextRef         cg_context)
{
  CGContextRelease (cg_context);
}

static void
gdk_root_surface_impl_quartz_class_init (GdkRootWindowImplQuartzClass *klass)
{
  GdkSurfaceImplQuartzClass *window_quartz_class = GDK_SURFACE_IMPL_QUARTZ_CLASS (klass);

  root_window_parent_class = g_type_class_peek_parent (klass);

  window_quartz_class->get_context = gdk_root_surface_impl_quartz_get_context;
  window_quartz_class->release_context = gdk_root_surface_impl_quartz_release_context;
}

static void
gdk_root_surface_impl_quartz_init (GdkRootWindowImplQuartz *impl)
{
}

GType
_gdk_root_surface_impl_quartz_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
        {
          sizeof (GdkRootWindowImplQuartzClass),
          (GBaseInitFunc) NULL,
          (GBaseFinalizeFunc) NULL,
          (GClassInitFunc) gdk_root_surface_impl_quartz_class_init,
          NULL,           /* class_finalize */
          NULL,           /* class_data */
          sizeof (GdkRootWindowImplQuartz),
          0,              /* n_preallocs */
          (GInstanceInitFunc) gdk_root_surface_impl_quartz_init,
        };

      object_type = g_type_register_static (GDK_TYPE_SURFACE_IMPL_QUARTZ,
                                            "GdkRootWindowQuartz",
                                            &object_info, 0);
    }

  return object_type;
}

GList *
get_toplevels (void)
{
  update_toplevel_order ();
  return GDK_SURFACE_IMPL_QUARTZ (_gdk_root->impl)->sorted_children;
}
