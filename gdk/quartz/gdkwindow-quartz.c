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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gdk/gdk.h>
#include <gdk/gdkdeviceprivate.h>
#include <gdk/gdkdisplayprivate.h>
#include <gdk/gdkframeclockprivate.h>

#include "gdkwindowimpl.h"
#include "gdkwindow-quartz.h"
#include "gdkprivate-quartz.h"
#include "gdkglcontext-quartz.h"
#include "gdkquartzglcontext.h"
#include "gdkquartzscreen.h"
#include "gdkquartzcursor.h"
#include "gdkquartz-cocoa-access.h"
#include "gdkinternal-quartz.h"

#include <Carbon/Carbon.h>
#include <AvailabilityMacros.h>

#include <sys/time.h>
#include <cairo-quartz.h>

static gpointer parent_class;
static gpointer root_window_parent_class;

static GSList   *update_nswindows;
static gboolean  in_process_all_updates = FALSE;

static GSList *main_window_stack;

typedef struct
{
  gint            x, y;
  gint            width, height;
  GdkWMDecoration decor;
} FullscreenSavedGeometry;

#if MAC_OS_X_VERSION_MIN_REQUIRED < 101200
typedef enum
{
 GDK_QUARTZ_BORDERLESS_WINDOW = NSBorderlessWindowMask,
 GDK_QUARTZ_CLOSABLE_WINDOW = NSClosableWindowMask,
#if MAC_OS_X_VERSION_MIN_REQUIRED > 1060
 /* Added in 10.7. Apple's docs are wrong to say it's from earlier. */
 GDK_QUARTZ_FULLSCREEN_WINDOW = NSFullScreenWindowMask,
#endif
 GDK_QUARTZ_MINIATURIZABLE_WINDOW = NSMiniaturizableWindowMask,
 GDK_QUARTZ_RESIZABLE_WINDOW = NSResizableWindowMask,
 GDK_QUARTZ_TITLED_WINDOW = NSTitledWindowMask,
} GdkQuartzWindowMask;
#else
typedef enum
{
 GDK_QUARTZ_BORDERLESS_WINDOW = NSWindowStyleMaskBorderless,
 GDK_QUARTZ_CLOSABLE_WINDOW = NSWindowStyleMaskClosable,
 GDK_QUARTZ_FULLSCREEN_WINDOW = NSWindowStyleMaskFullScreen,
 GDK_QUARTZ_MINIATURIZABLE_WINDOW = NSWindowStyleMaskMiniaturizable,
 GDK_QUARTZ_RESIZABLE_WINDOW = NSWindowStyleMaskResizable,
 GDK_QUARTZ_TITLED_WINDOW = NSWindowStyleMaskTitled,
} GdkQuartzWindowMask;
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
static FullscreenSavedGeometry *get_fullscreen_geometry (GdkWindow *window);
#endif

#define FULLSCREEN_DATA "fullscreen-data"

static void update_toplevel_order (void);
static void clear_toplevel_order  (void);

#define WINDOW_IS_TOPLEVEL(window)		     \
  (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&   \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN && \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_OFFSCREEN)

/*
 * GdkQuartzWindow
 */

struct _GdkQuartzWindow
{
  GdkWindow parent;
};

struct _GdkQuartzWindowClass
{
  GdkWindowClass parent_class;
};

G_DEFINE_TYPE (GdkQuartzWindow, gdk_quartz_window, GDK_TYPE_WINDOW);

static void
gdk_quartz_window_class_init (GdkQuartzWindowClass *quartz_window_class)
{
}

static void
gdk_quartz_window_init (GdkQuartzWindow *quartz_window)
{
}


/*
 * GdkQuartzWindowImpl
 */
static inline GdkWindowImplQuartz*
gdk_window_get_quartz_impl (GdkWindow* window)
{
  GdkWindow *embedder;

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  embedder = gdk_offscreen_window_get_embedder (window);

  while (embedder)
    {
      window = embedder;
      embedder = gdk_offscreen_window_get_embedder (window);
    }

  g_return_val_if_fail (GDK_IS_WINDOW_IMPL_QUARTZ (window->impl), NULL);

  return GDK_WINDOW_IMPL_QUARTZ (window->impl);
}

NSView *
gdk_quartz_window_get_nsview (GdkWindow *window)
{
  GdkWindowImplQuartz *impl = gdk_window_get_quartz_impl (window);

  return impl ? impl->view : NULL;
}

NSWindow *
gdk_quartz_window_get_nswindow (GdkWindow *window)
{
  GdkWindowImplQuartz *impl = gdk_window_get_quartz_impl (window);

  return impl ? impl->toplevel : NULL;
}

static CGContextRef
gdk_window_impl_quartz_get_context (GdkWindowImplQuartz *window_impl,
				    gboolean             antialias)
{
  CGContextRef cg_context = NULL;

  if (GDK_WINDOW_DESTROYED (window_impl->wrapper))
    return NULL;

  /* Lock focus when not called as part of a drawRect call. This
   * is needed when called from outside "real" expose events, for
   * example for synthesized expose events when realizing windows
   * and for widgets that send fake expose events like the arrow
   * buttons in spinbuttons or the position marker in rulers.
   */
#if MAC_OS_X_VERSION_MAX_ALLOWED < 101400
  if (gdk_quartz_osx_version() < GDK_OSX_MOJAVE &&
      window_impl->in_paint_rect_count == 0)
    {
      /* The NSView focus-locking API set was deprecated in MacOS 10.14 and
       * has a significant cost in MacOS 11 - every lock/unlock seems to
       * trigger a drawRect: call for the entire window.  To return the
       * lost performance, do not use the locking API in MacOS 11+
       */
         if (![window_impl->view lockFocusIfCanDraw])
            return NULL;
    }
#endif
#if MAC_OS_X_VERSION_MIN_REQUIRED < 101000
  if (gdk_quartz_osx_version () < GDK_OSX_YOSEMITE)
    cg_context = [[NSGraphicsContext currentContext] graphicsPort];
  else
#endif
    cg_context = [[NSGraphicsContext currentContext] CGContext];

  if (!cg_context)
    return NULL;
  CGContextSaveGState (cg_context);
  CGContextSetAllowsAntialiasing (cg_context, antialias);

  return cg_context;
}

static void
gdk_window_impl_quartz_release_context (GdkWindowImplQuartz *window_impl,
                                        CGContextRef         cg_context)
{
  if (cg_context)
    {
      CGContextRestoreGState (cg_context);
      CGContextSetAllowsAntialiasing (cg_context, TRUE);
    }

  /* See comment in gdk_quartz_window_get_context(). */
#if MAC_OS_X_VERSION_MIN_REQUIRED < 101400
  if (gdk_quartz_osx_version() < GDK_OSX_MOJAVE &&
      window_impl->in_paint_rect_count == 0)
    {
      [window_impl->toplevel flushWindow];
      [window_impl->view unlockFocus];
    }
#endif
}

static void
gdk_window_impl_quartz_finalize (GObject *object)
{
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (object);
  GdkDisplay *display = gdk_window_get_display (impl->wrapper);
  GdkSeat *seat = gdk_display_get_default_seat (display);

  gdk_seat_ungrab (seat);

  if (impl->transient_for)
    g_object_unref (impl->transient_for);

  if (impl->view)
    {
      [[NSNotificationCenter defaultCenter] removeObserver: impl->toplevel
                                        name: @"NSViewFrameDidChangeNotification"
                                      object: impl->view];
      [impl->view release];
    }

  impl->view = NULL;
  impl->transient_for = NULL;
  impl->toplevel = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static cairo_user_data_key_t gdk_quartz_cairo_key;

typedef struct {
  GdkWindowImplQuartz  *window_impl;
  CGContextRef  cg_context;
} GdkQuartzCairoSurfaceData;

static void
gdk_quartz_cairo_surface_destroy (void *data)
{
  GdkQuartzCairoSurfaceData *surface_data = data;
  cairo_surface_t *surface = surface_data->window_impl->cairo_surface;

  if (!cairo_surface_get_reference_count (surface))
  surface_data->window_impl->cairo_surface = NULL;

  g_free (surface_data);
}

static cairo_surface_t *
gdk_quartz_create_cairo_surface (GdkWindowImplQuartz *impl,
				 int                  width,
				 int                  height)
{
  GdkQuartzCairoSurfaceData *surface_data;
  cairo_surface_t *surface;

  surface_data = g_new (GdkQuartzCairoSurfaceData, 1);
  surface_data->window_impl = impl;
  surface_data->cg_context = NULL;

  [impl->view createBackingStoreWithWidth: width andHeight: height];
  surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
  cairo_surface_set_user_data (surface, &gdk_quartz_cairo_key,
                               surface_data,
                               gdk_quartz_cairo_surface_destroy);

  return surface;
}

static cairo_surface_t *
gdk_quartz_ref_cairo_surface (GdkWindow *window)
{
  GdkWindowImplQuartz *impl = gdk_window_get_quartz_impl (window);

  if (GDK_WINDOW_DESTROYED (window) || !impl)
    return NULL;

  if (!impl->cairo_surface)
    {
      gint width = gdk_window_get_width (impl->wrapper);
      gint height = gdk_window_get_height (impl->wrapper);
      gint scale = gdk_window_get_scale_factor (impl->wrapper);
      gint scaled_width = width * scale;
      const gint align = GDK_WINDOW_QUARTZ_ALIGNMENT;

      if (scaled_width % align)
          scaled_width += align - scaled_width % align; // Surface widths must be 4-pixel aligned

      impl->cairo_surface = gdk_quartz_create_cairo_surface (impl,
                                                             scaled_width,
                                                             height * scale);
      cairo_surface_set_device_scale (impl->cairo_surface, scale, scale);
      cairo_surface_reference (impl->cairo_surface); // The caller will destroy the returned one.
    }
  else
    {
      cairo_surface_reference (impl->cairo_surface);
    }

  return impl->cairo_surface;
}

void
_gdk_quartz_unref_cairo_surface (GdkWindow *window)
{
  GdkWindowImplQuartz *impl = gdk_window_get_quartz_impl (window);
  if (GDK_WINDOW_DESTROYED (window) || !impl)
    return;

  if (impl->cairo_surface)
    {
      cairo_surface_destroy (impl->cairo_surface);
      if (impl->cairo_surface &&
          !cairo_surface_get_reference_count (impl->cairo_surface))
          impl->cairo_surface = NULL;
    }
}

static void
gdk_window_impl_quartz_init (GdkWindowImplQuartz *impl)
{
  impl->type_hint = GDK_WINDOW_TYPE_HINT_NORMAL;
  impl->view      = NULL;
  impl->toplevel  = NULL;
  impl->transient_for = NULL;
}

static gboolean
gdk_window_impl_quartz_begin_paint (GdkWindow *window)
{
     gdk_quartz_ref_cairo_surface (window); //unreffed in GdkQuartzView::updateLayer
  return FALSE;
}

static void
gdk_quartz_window_set_needs_display_in_region (GdkWindow    *window,
                                               cairo_region_t    *region)
{
  GdkWindowImplQuartz *impl;
  int i, n_rects;

  impl = gdk_window_get_quartz_impl (window);

  if (!impl)
    return;

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
_gdk_quartz_window_process_updates_recurse (GdkWindow *window,
                                            cairo_region_t *region)
{
  /* Make sure to only flush each toplevel at most once if we're called
   * from process_all_updates.
   */
  if (in_process_all_updates)
    {
      GdkWindow *toplevel= gdk_window_get_effective_toplevel (window);
      GdkWindowImplQuartz *toplevel_impl =
            gdk_window_get_quartz_impl (toplevel);

      if (toplevel && WINDOW_IS_TOPLEVEL (toplevel) && toplevel_impl)
        {
          NSWindow *nswindow = toplevel_impl->toplevel;
#if MAC_OS_X_VERSION_MIN_REQUIRED < 101400
          /* In theory, we could skip the flush disabling, since we only
           * have one NSView.
           */
          if (gdk_quartz_osx_version() < GDK_OSX_MOJAVE &&
               nswindow && ![nswindow isFlushWindowDisabled])
            {
              [nswindow retain];
              [nswindow disableFlushWindow];
            }
#endif
          update_nswindows = g_slist_prepend (update_nswindows, nswindow);
        }
    }

  if (WINDOW_IS_TOPLEVEL (window))
    gdk_quartz_window_set_needs_display_in_region (window, region);
  else
    _gdk_window_process_updates_recurse (window, region);

  /* NOTE: I'm not sure if we should displayIfNeeded here. It slows down a
   * lot (since it triggers the beam syncing) and things seem to work
   * without it.
   */
}

void
_gdk_quartz_display_before_process_all_updates (GdkDisplay *display)
{
  in_process_all_updates = TRUE;

  if (gdk_quartz_osx_version () >= GDK_OSX_EL_CAPITAN)
    {
      [NSAnimationContext endGrouping];
    }
#if MAC_OS_X_VERSION_MIN_REQUIRED < 101100
  else
    {
      NSDisableScreenUpdates ();
    }
#endif
}

void
_gdk_quartz_display_after_process_all_updates (GdkDisplay *display)
{
  GSList *tmp_list = update_nswindows;

  update_nswindows = NULL;

  while (tmp_list)
    {
      NSWindow *nswindow = tmp_list->data;

      [[nswindow contentView] displayIfNeeded];

#if MAC_OS_X_VERSION_MIN_REQUIRED < 101400
      if(gdk_quartz_osx_version() < GDK_OSX_BIGSUR)
        {
          [nswindow enableFlushWindow];
          [nswindow flushWindow];
        }
#endif
      [nswindow release];

      tmp_list = g_slist_remove_link (tmp_list, tmp_list);
    }

  in_process_all_updates = FALSE;

  if (gdk_quartz_osx_version() >= GDK_OSX_EL_CAPITAN)
    {
      [NSAnimationContext beginGrouping];
    }
#if MAC_OS_X_VERSION_MIN_REQUIRED < 101100
  else
    {
      NSEnableScreenUpdates ();
    }
#endif
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
_gdk_quartz_window_debug_highlight (GdkWindow *window, gint number)
{
  gint x, y;
  gint gx, gy;
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

  _gdk_quartz_window_gdk_xy_to_xy (x, y + window->height,
                                   &gx, &gy);

  rect = NSMakeRect (gx, gy, window->width, window->height);

  if (debug_window[number] && NSEqualRects (rect, old_rect[number]))
    return;

  old_rect[number] = rect;

  if (debug_window[number])
    [debug_window[number] close];

  debug_window[number] = [[NSWindow alloc] initWithContentRect:rect
#if MAC_OS_X_VERSION_MIN_REQUIRED < 101200
                                                     styleMask:(NSUInteger)GDK_QUARTZ_BORDERLESS_WINDOW
#else
                                                     styleMask:(NSWindowStyleMask)GDK_QUARTZ_BORDERLESS_WINDOW
#endif
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


/* See notes on top of gdkscreen-quartz.c */
void
_gdk_quartz_window_gdk_xy_to_xy (gint  gdk_x,
                                 gint  gdk_y,
                                 gint *ns_x,
                                 gint *ns_y)
{
  GdkQuartzScreen *screen_quartz = GDK_QUARTZ_SCREEN (_gdk_screen);

  if (ns_y)
    *ns_y = screen_quartz->orig_y - gdk_y;

  if (ns_x)
    *ns_x = gdk_x + screen_quartz->orig_x;
}

void
_gdk_quartz_window_xy_to_gdk_xy (gint  ns_x,
                                 gint  ns_y,
                                 gint *gdk_x,
                                 gint *gdk_y)
{
  GdkQuartzScreen *screen_quartz = GDK_QUARTZ_SCREEN (_gdk_screen);

  if (gdk_y)
    *gdk_y = screen_quartz->orig_y - ns_y;

  if (gdk_x)
    *gdk_x = ns_x - screen_quartz->orig_x;
}

void
_gdk_quartz_window_nspoint_to_gdk_xy (NSPoint  point,
                                      gint    *x,
                                      gint    *y)
{
  _gdk_quartz_window_xy_to_gdk_xy (point.x, point.y,
                                   x, y);
}

static GdkWindow *
find_child_window_helper (GdkWindow *window,
			  gint       x,
			  gint       y,
			  gint       x_offset,
			  gint       y_offset,
                          gboolean   get_toplevel)
{
  GdkWindowImplQuartz *impl;
  GList *l;

  impl = gdk_window_get_quartz_impl (window);

  if (window == _gdk_root)
    update_toplevel_order ();

  g_return_val_if_fail (impl, NULL);

  for (l = impl->sorted_children; l; l = l->next)
    {
      GdkWindow *child = l->data;
      GdkWindowImplQuartz *child_impl = gdk_window_get_quartz_impl (child);
      int temp_x, temp_y;

      if (!GDK_WINDOW_IS_MAPPED (child))
	continue;

      temp_x = x_offset + child->x;
      temp_y = y_offset + child->y;

      /* Special-case the root window. We have to include the title
       * bar in the checks, otherwise the window below the title bar
       * will be found i.e. events punch through. (If we can find a
       * better way to deal with the events in gdkevents-quartz, this
       * might not be needed.)
       */
      if (child_impl && window == _gdk_root)
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
              return (GdkWindow *)_gdk_root;
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

/* Given a GdkWindow and coordinates relative to it, returns the
 * innermost subwindow that contains the point. If the coordinates are
 * outside the passed in window, NULL is returned.
 */
GdkWindow *
_gdk_quartz_window_find_child (GdkWindow *window,
			       gint       x,
			       gint       y,
                               gboolean   get_toplevel)
{
  if (x >= 0 && y >= 0 && x < window->width && y < window->height)
    return find_child_window_helper (window, x, y, 0, 0, get_toplevel);

  return NULL;
}

/* Raises a transient window.
 */
static void
raise_transient (GdkWindowImplQuartz *impl)
{
  /* In quartz the transient-for behavior is implemented by
   * attaching the transient-for GdkNSWindows to the parent's
   * GdkNSWindow. Stacking is managed by Quartz and the order
   * is that of the parent's childWindows array. The only way
   * to change that order is to remove the child from the
   * parent and then add it back in.
   */
  GdkWindowImplQuartz *parent_impl =
        gdk_window_get_quartz_impl (impl->transient_for);
  if (parent_impl)
    {
      [parent_impl->toplevel removeChildWindow:impl->toplevel];
      [parent_impl->toplevel addChildWindow:impl->toplevel
                                    ordered:NSWindowAbove];
    }
}

void
_gdk_quartz_window_did_become_main (GdkWindow *window)
{
  GdkWindowImplQuartz *impl = gdk_window_get_quartz_impl (window);

  main_window_stack = g_slist_remove (main_window_stack, window);

  if (window->window_type != GDK_WINDOW_TEMP)
    main_window_stack = g_slist_prepend (main_window_stack, window);

  if (impl &&
      impl->transient_for && !GDK_WINDOW_DESTROYED (impl->transient_for))
    raise_transient (impl);

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

      toplevels = gdk_screen_get_toplevel_windows (gdk_screen_get_default ());
      if (toplevels)
        new_window = toplevels->data;
      g_list_free (toplevels);
    }

  if (new_window &&
      new_window != window &&
      GDK_WINDOW_IS_MAPPED (new_window) &&
      WINDOW_IS_TOPLEVEL (new_window))
    {
      GdkWindowImplQuartz *impl = gdk_window_get_quartz_impl (new_window);

      if (impl)
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

static void
on_frame_clock_before_paint (GdkFrameClock *frame_clock,
                             GdkWindow     *window)
{
}

static void
on_frame_clock_after_paint (GdkFrameClock *frame_clock,
                            GdkWindow     *window)
{
  GdkWindowImplQuartz *impl = gdk_window_get_quartz_impl (window);
  GdkDisplay *display = gdk_window_get_display (window);
  GdkFrameTimings *timings;

  timings = gdk_frame_clock_get_current_timings (frame_clock);
  if (impl && timings != NULL)
    impl->pending_frame_counter = timings->frame_counter;

  _gdk_quartz_display_add_frame_callback (display, window);

  _gdk_frame_clock_freeze (frame_clock);
}

void
_gdk_quartz_display_create_window_impl (GdkDisplay    *display,
                                        GdkWindow     *window,
                                        GdkWindow     *real_parent,
                                        GdkScreen     *screen,
                                        GdkEventMask   event_mask,
                                        GdkWindowAttr *attributes,
                                        gint           attributes_mask)
{
  GdkWindowImplQuartz *impl;
  GdkWindowImplQuartz *parent_impl;
  GdkWindowTypeHint    type_hint = GDK_WINDOW_TYPE_HINT_NORMAL;
  GdkFrameClock *frame_clock;

  GDK_QUARTZ_ALLOC_POOL;

  impl = g_object_new (GDK_TYPE_WINDOW_IMPL_QUARTZ, NULL);
  window->impl = GDK_WINDOW_IMPL (impl);
  impl->wrapper = window;

  parent_impl = gdk_window_get_quartz_impl (window->parent);

  switch (window->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_TEMP:
      if (GDK_WINDOW_TYPE (window->parent) != GDK_WINDOW_ROOT)
	{
	  /* The common code warns for this case */
          parent_impl = gdk_window_get_quartz_impl (_gdk_root);
	}
    }

  /* Maintain the z-ordered list of children. */
  if (parent_impl && window->parent != _gdk_root)
    parent_impl->sorted_children = g_list_prepend (parent_impl->sorted_children, window);
  else
    clear_toplevel_order ();

  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));

  if (attributes_mask & GDK_WA_TYPE_HINT)
    {
      type_hint = attributes->type_hint;
      gdk_window_set_type_hint (window, type_hint);
    }

  switch (window->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_TEMP:
      {
        NSScreen *screen;
        NSRect screen_rect;
        NSRect content_rect;
        NSUInteger style_mask;
        int nx, ny;
        const char *title;
        const gint scale = gdk_window_get_scale_factor (window);
        const guint align = GDK_WINDOW_QUARTZ_ALIGNMENT / scale;

        /* initWithContentRect will place on the mainScreen by default.
         * We want to select the screen to place on ourselves.  We need
         * to find the screen the window will be on and correct the
         * content_rect coordinates to be relative to that screen.
         */
        _gdk_quartz_window_gdk_xy_to_xy (window->x, window->y, &nx, &ny);

        screen = get_nsscreen_for_point (nx, ny);
        screen_rect = [screen frame];
        nx -= screen_rect.origin.x;
        ny -= screen_rect.origin.y;

        if (window->width % align)
          window->width += align - window->width % align;

        content_rect = NSMakeRect (nx, ny - window->height,
                                   window->width,
                                   window->height);

        if (window->window_type == GDK_WINDOW_TEMP ||
            type_hint == GDK_WINDOW_TYPE_HINT_SPLASHSCREEN)
          {
            style_mask = GDK_QUARTZ_BORDERLESS_WINDOW;
          }
        else
          {
            style_mask = (GDK_QUARTZ_TITLED_WINDOW |
                          GDK_QUARTZ_CLOSABLE_WINDOW |
                          GDK_QUARTZ_MINIATURIZABLE_WINDOW |
                          GDK_QUARTZ_RESIZABLE_WINDOW);
          }

	impl->toplevel = [[GdkQuartzNSWindow alloc] initWithContentRect:content_rect 
			                                      styleMask:style_mask
			                                        backing:NSBackingStoreBuffered
			                                          defer:NO
                                                                  screen:screen];

        if (type_hint != GDK_WINDOW_TYPE_HINT_NORMAL)
          impl->toplevel.excludedFromWindowsMenu = true;

	if (attributes_mask & GDK_WA_TITLE)
	  title = attributes->title;
	else
	  title = get_default_title ();

	gdk_window_set_title (window, title);
  
	if (gdk_window_get_visual (window) == gdk_screen_get_rgba_visual (_gdk_screen))
	  {
	    [impl->toplevel setOpaque:NO];
	    [impl->toplevel setBackgroundColor:[NSColor clearColor]];
	  }

        content_rect.origin.x = 0;
        content_rect.origin.y = 0;

	impl->view = [[GdkQuartzView alloc] initWithFrame:content_rect];
	[impl->view setGdkWindow:window];
	[impl->toplevel setContentView:impl->view];
        [[NSNotificationCenter defaultCenter] addObserver: impl->toplevel
                                      selector: @selector (windowDidResize:)
                                      name: @"NSViewFrameDidChangeNotification"
                                      object: impl->view];
      }
      break;

    case GDK_WINDOW_CHILD:
      {
	GdkWindowImplQuartz *parent_impl = gdk_window_get_quartz_impl (window->parent);

	if (parent_impl && !window->input_only)
	  {
	    NSRect frame_rect = NSMakeRect (window->x + window->parent->abs_x,
                                            window->y + window->parent->abs_y,
                                            window->width,
                                            window->height);
	
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

  frame_clock = gdk_window_get_frame_clock (window);

  g_signal_connect (frame_clock, "before-paint",
                    G_CALLBACK (on_frame_clock_before_paint), window);
  g_signal_connect (frame_clock, "after-paint",
                    G_CALLBACK (on_frame_clock_after_paint), window);
}

void
_gdk_quartz_window_update_position (GdkWindow *window)
{
  NSRect frame_rect;
  NSRect content_rect;
  GdkWindowImplQuartz *impl = gdk_window_get_quartz_impl (window);

  if (!impl)
    return;

  GDK_QUARTZ_ALLOC_POOL;

  frame_rect = [impl->toplevel frame];
  content_rect = [impl->toplevel contentRectForFrameRect:frame_rect];

  _gdk_quartz_window_xy_to_gdk_xy (content_rect.origin.x,
                                   content_rect.origin.y + content_rect.size.height,
                                   &window->x, &window->y);

  GDK_QUARTZ_RELEASE_POOL;
}

void
_gdk_quartz_window_init_windowing (GdkDisplay *display,
                                   GdkScreen  *screen)
{
  GdkWindowImplQuartz *impl;

  g_assert (_gdk_root == NULL);

  _gdk_root = _gdk_display_create_window (display);

  _gdk_root->impl = g_object_new (_gdk_root_window_impl_quartz_get_type (), NULL);
  _gdk_root->impl_window = _gdk_root;
  _gdk_root->visual = gdk_screen_get_system_visual (screen);

  impl = gdk_window_get_quartz_impl (_gdk_root);
  /* If impl is somehow not a GdkWindowImplQuartz we're screwed, so crash. */
  if (!impl)
    {
      g_error ("Initializing quartz windowing produced the wrong kind of impl!");
      g_abort ();
    }

  _gdk_quartz_screen_update_window_sizes (screen);

  _gdk_root->state = 0; /* We don't want GDK_WINDOW_STATE_WITHDRAWN here */
  _gdk_root->window_type = GDK_WINDOW_ROOT;
  _gdk_root->depth = 24;
  _gdk_root->viewable = TRUE;

  impl->wrapper = _gdk_root;
}

static void
gdk_quartz_window_destroy (GdkWindow *window,
                           gboolean   recursing,
                           gboolean   foreign_destroy)
{
  GdkWindowImplQuartz *impl;
  GdkWindow *parent;
  GdkDisplay *display;

  impl = gdk_window_get_quartz_impl (window);

  if (!impl)
    return;

  display = gdk_window_get_display (window);

  _gdk_quartz_display_remove_frame_callback (display, window);

  main_window_stack = g_slist_remove (main_window_stack, window);

  g_list_free (impl->sorted_children);
  impl->sorted_children = NULL;

  parent = window->parent;
  if (parent)
    {
      GdkWindowImplQuartz *parent_impl = gdk_window_get_quartz_impl (parent);

      if (parent_impl)
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

      impl->view = NULL;
      impl->toplevel = NULL;

      GDK_QUARTZ_RELEASE_POOL;
    }
}

static void
gdk_quartz_window_destroy_foreign (GdkWindow *window)
{
  /* Foreign windows aren't supported in OSX. */
}

/* FIXME: This might be possible to simplify with client-side windows. Also
 * note that already_mapped is not used yet, see the x11 backend.
*/
static void
gdk_window_quartz_show (GdkWindow *window, gboolean already_mapped)
{
  GdkWindowImplQuartz *impl = gdk_window_get_quartz_impl (window);
  gboolean focus_on_map;

  if (!impl)
    return;

  GDK_QUARTZ_ALLOC_POOL;

  if (!GDK_WINDOW_IS_MAPPED (window))
    focus_on_map = window->focus_on_map;
  else
    focus_on_map = TRUE;

  if (WINDOW_IS_TOPLEVEL (window) && impl->toplevel)
    {
      gboolean make_key;

      make_key = (window->accept_focus && focus_on_map &&
                  window->window_type != GDK_WINDOW_TEMP);

      [(GdkQuartzNSWindow*)impl->toplevel showAndMakeKey:make_key];
      clear_toplevel_order ();

      _gdk_quartz_events_send_map_event (window);
    }
  else
    {
      [impl->view setHidden:NO];
    }

  [impl->view setNeedsDisplay:YES];

  gdk_synthesize_window_state (window, GDK_WINDOW_STATE_WITHDRAWN, 0);

  if (window->state & GDK_WINDOW_STATE_MAXIMIZED)
    gdk_window_maximize (window);

  if (window->state & GDK_WINDOW_STATE_ICONIFIED)
    gdk_window_iconify (window);

  if (impl->transient_for && !GDK_WINDOW_DESTROYED (impl->transient_for))
    _gdk_quartz_window_attach_to_parent (window);

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

  impl = gdk_window_get_quartz_impl (window);

  g_return_if_fail (impl && impl->toplevel != NULL);

  if (impl->transient_for && !GDK_WINDOW_DESTROYED (impl->transient_for))
    {
      GdkWindowImplQuartz *parent_impl;

      parent_impl = gdk_window_get_quartz_impl (impl->transient_for);
      if (parent_impl)
        {
          [parent_impl->toplevel removeChildWindow:impl->toplevel];
          clear_toplevel_order ();
        }
    }
}

/* Re-sets the parent window, if the window is a transient. */
void
_gdk_quartz_window_attach_to_parent (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = gdk_window_get_quartz_impl (window);

  g_return_if_fail (impl && impl->toplevel != NULL);

  if (impl->transient_for && !GDK_WINDOW_DESTROYED (impl->transient_for))
    {
      GdkWindowImplQuartz *parent_impl;

      parent_impl = gdk_window_get_quartz_impl (impl->transient_for);
      if (parent_impl)
        {
          [parent_impl->toplevel addChildWindow:impl->toplevel ordered:NSWindowAbove];
          clear_toplevel_order ();
        }
    }
}

void
gdk_window_quartz_hide (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;
  GdkDisplay *display = gdk_window_get_display (window);
  GdkSeat *seat = gdk_display_get_default_seat (display);
  gdk_seat_ungrab (seat);

  /* Make sure we're not stuck in fullscreen mode. */
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
  if (get_fullscreen_geometry (window))
    SetSystemUIMode (kUIModeNormal, 0);
#endif

  _gdk_window_clear_update_area (window);

  impl = gdk_window_get_quartz_impl (window);
  if (!impl)
    return;

  if (WINDOW_IS_TOPLEVEL (window))
    {
     /* Update main window. */
      main_window_stack = g_slist_remove (main_window_stack, window);
      if ([NSApp mainWindow] == impl->toplevel)
        _gdk_quartz_window_did_resign_main (window);

      if (impl->transient_for)
        _gdk_quartz_window_detach_from_parent (window);

      [(GdkQuartzNSWindow*)impl->toplevel hide];
    }
  else if (impl->view)
    {
      [impl->view setHidden:YES];
    }
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
  GdkWindowImplQuartz *impl;
  GdkRectangle old_visible;
  GdkRectangle new_visible;
  GdkRectangle scroll_rect;
  cairo_region_t *old_region;
  cairo_region_t *expose_region;
  NSSize delta;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = gdk_window_get_quartz_impl (window);

  if (!impl)
    return;

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
    {
      window->width = width;
    }

  if (height != -1)
    {
      window->height = height;
    }

  GDK_QUARTZ_ALLOC_POOL;

  if (impl->toplevel)
    {
      NSRect content_rect;
      NSRect frame_rect;
      gint gx, gy;

      _gdk_quartz_window_gdk_xy_to_xy (window->x, window->y + window->height,
                                       &gx, &gy);

      content_rect = NSMakeRect (gx, gy, window->width, window->height);

      frame_rect = [impl->toplevel frameRectForContentRect:content_rect];
      [impl->toplevel setFrame:frame_rect display:YES];
      impl->cairo_surface = gdk_quartz_ref_cairo_surface (window);
      cairo_surface_destroy (impl->cairo_surface); // Remove the extra reference
    }
  else 
    {
      if (!window->input_only)
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

              gdk_quartz_window_set_needs_display_in_region (window, expose_region);
            }
          else
            {
              [impl->view setFrame:nsrect];
              [impl->view setNeedsDisplay:YES];
            }

          cairo_region_destroy (expose_region);
          cairo_region_destroy (old_region);
        }
    }

  if (window->gl_paint_context != NULL)
    [GDK_QUARTZ_GL_CONTEXT (window->gl_paint_context)->gl_context update];

  GDK_QUARTZ_RELEASE_POOL;
}

static inline void
window_quartz_move (GdkWindow *window,
                    gint       x,
                    gint       y)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->state & GDK_WINDOW_STATE_FULLSCREEN)
    return;

  move_resize_window_internal (window, x, y, -1, -1);
}

static inline void
window_quartz_resize (GdkWindow *window,
                      gint       width,
                      gint       height)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (window->state & GDK_WINDOW_STATE_FULLSCREEN)
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

/* FIXME: This might need fixing (reparenting didn't work before client-side
 * windows either).
 */
static gboolean
gdk_window_quartz_reparent (GdkWindow *window,
                            GdkWindow *new_parent,
                            gint       x,
                            gint       y)
{
  GdkWindow *old_parent;
  GdkWindowImplQuartz *impl, *old_parent_impl, *new_parent_impl;
  NSView *view, *new_parent_view;

  if (new_parent == _gdk_root)
    {
      /* Could be added, just needs implementing. */
      g_warning ("Reparenting to root window is not supported yet in the Mac OS X backend");
      return FALSE;
    }

  impl = gdk_window_get_quartz_impl (window);
  if (!impl)
    return FALSE;
  view = impl->view;

  new_parent_impl = gdk_window_get_quartz_impl (new_parent);
  if (!new_parent_impl)
    return FALSE;

  new_parent_view = new_parent_impl->view;

  old_parent = window->parent;
  old_parent_impl = gdk_window_get_quartz_impl (old_parent);
  if (!old_parent_impl)
    return FALSE;

  [view retain];

  [view removeFromSuperview];
  [new_parent_view addSubview:view];

  [view release];

  window->parent = new_parent;

  if (old_parent)
    {
      old_parent_impl->sorted_children = g_list_remove (old_parent_impl->sorted_children, window);
    }

  new_parent_impl->sorted_children = g_list_prepend (new_parent_impl->sorted_children, window);

  return FALSE;
}

/* Get the toplevel ordering from NSApp and update our own list. We do
 * this on demand since the NSApp’s list is not up to date directly
 * after we get windowDidBecomeMain.
 */
static void
update_toplevel_order (void)
{
  GdkWindowImplQuartz *root_impl;
  NSEnumerator *enumerator;
  id nswindow;
  GList *toplevels = NULL;

  root_impl = gdk_window_get_quartz_impl (_gdk_root);

  if (!root_impl || root_impl->sorted_children)
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
  GdkWindowImplQuartz *root_impl;

  root_impl = gdk_window_get_quartz_impl (_gdk_root);
  if (root_impl)
    {
      g_list_free (root_impl->sorted_children);
      root_impl->sorted_children = NULL;
    }
}

static void
gdk_window_quartz_raise (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (WINDOW_IS_TOPLEVEL (window))
    {
      GdkWindowImplQuartz *impl;

      impl = gdk_window_get_quartz_impl (window);
      if (!impl)
        return;
      if (impl->transient_for && !GDK_WINDOW_DESTROYED (impl->transient_for))
        raise_transient (impl);
      else
        [impl->toplevel orderFront:impl->toplevel];

      clear_toplevel_order ();
    }
  else
    {
      GdkWindow *parent = window->parent;

      if (parent)
        {
          GdkWindowImplQuartz *impl;

          impl = gdk_window_get_quartz_impl (parent);
          if (impl)
            {
              impl->sorted_children = g_list_remove (impl->sorted_children, window);
              impl->sorted_children = g_list_prepend (impl->sorted_children, window);
            }
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

      impl = gdk_window_get_quartz_impl (window);
      if (impl)
        {
          [impl->toplevel orderBack:impl->toplevel];

          clear_toplevel_order ();
        }
    }
  else
    {
      GdkWindow *parent = window->parent;

      if (parent)
        {
          GdkWindowImplQuartz *impl;

          impl = gdk_window_get_quartz_impl (parent);
          if (impl)
            {
              impl->sorted_children = g_list_remove (impl->sorted_children, window);
              impl->sorted_children = g_list_append (impl->sorted_children, window);
            }
        }
    }
}

static void
gdk_window_quartz_restack_toplevel (GdkWindow *window,
				    GdkWindow *sibling,
				    gboolean   above)
{
  GdkWindowImplQuartz *impl;
  gint sibling_num = -1;

  impl = gdk_window_get_quartz_impl (sibling);
  if (impl)
    sibling_num = [impl->toplevel windowNumber];

  impl = gdk_window_get_quartz_impl (window);
  if (!(impl && sibling_num >= 0))
    return;
  if (above)
    [impl->toplevel orderWindow:NSWindowAbove relativeTo:sibling_num];
  else
    [impl->toplevel orderWindow:NSWindowBelow relativeTo:sibling_num];
}

static void
gdk_window_quartz_set_background (GdkWindow       *window,
                                  cairo_pattern_t *pattern)
{
  /* FIXME: We could theoretically set the background color for toplevels
   * here. (Currently we draw the background before emitting expose events)
   */
}

static void
gdk_window_quartz_set_device_cursor (GdkWindow *window,
                                     GdkDevice *device,
                                     GdkCursor *cursor)
{
  NSCursor *nscursor;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  nscursor = _gdk_quartz_cursor_get_ns_cursor (cursor);

  [nscursor set];
}

static void
gdk_window_quartz_get_geometry (GdkWindow *window,
                                gint      *x,
                                gint      *y,
                                gint      *width,
                                gint      *height)
{
  GdkWindowImplQuartz *impl;
  NSRect ns_rect;

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = gdk_window_get_quartz_impl (window);
  if (!impl)
    return;
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
      if ([impl->toplevel styleMask] == GDK_QUARTZ_BORDERLESS_WINDOW)
        {
          _gdk_quartz_window_xy_to_gdk_xy (ns_rect.origin.x,
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
gdk_window_quartz_get_root_coords (GdkWindow *window,
                                   gint       x,
                                   gint       y,
                                   gint      *root_x,
                                   gint      *root_y)
{
  int tmp_x = 0, tmp_y = 0;
  GdkWindow *toplevel;
  NSRect content_rect;
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window)) 
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
  
  toplevel = gdk_window_get_toplevel (window);
  impl = gdk_window_get_quartz_impl (toplevel);

  if (!impl)
    return;

  content_rect = [impl->toplevel contentRectForFrameRect:[impl->toplevel frame]];

  _gdk_quartz_window_xy_to_gdk_xy (content_rect.origin.x,
                                   content_rect.origin.y + content_rect.size.height,
                                   &tmp_x, &tmp_y);

  tmp_x += x;
  tmp_y += y;

  while (window != toplevel)
    {
      if (_gdk_window_has_impl ((GdkWindow *)window))
        {
          tmp_x += window->x;
          tmp_y += window->y;
        }

      window = window->parent;
    }

  if (root_x)
    *root_x = tmp_x;
  if (root_y)
    *root_y = tmp_y;
}

/* Returns coordinates relative to the passed in window. */
static GdkWindow *
gdk_window_quartz_get_device_state_helper (GdkWindow       *window,
                                           GdkDevice       *device,
                                           gdouble         *x,
                                           gdouble         *y,
                                           GdkModifierType *mask)
{
  NSPoint point;
  gint x_tmp, y_tmp;
  GdkWindow *toplevel;
  GdkWindow *found_window;

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), NULL);

  if (GDK_WINDOW_DESTROYED (window))
    {
      *x = 0;
      *y = 0;
      *mask = 0;
      return NULL;
    }
  
  toplevel = gdk_window_get_toplevel (window);

  *mask = _gdk_quartz_events_get_current_keyboard_modifiers () |
      _gdk_quartz_events_get_current_mouse_modifiers ();

  /* Get the y coordinate, needs to be flipped. */
  if (window == _gdk_root)
    {
      point = [NSEvent mouseLocation];
      _gdk_quartz_window_nspoint_to_gdk_xy (point, &x_tmp, &y_tmp);
    }
  else
    {
      GdkWindowImplQuartz *impl;
      NSWindow *nswindow;

      impl = gdk_window_get_quartz_impl (toplevel);
      nswindow = impl ? impl->toplevel : NULL;

      point = [nswindow mouseLocationOutsideOfEventStream];

      x_tmp = point.x;
      y_tmp = toplevel->height - point.y;

      window = (GdkWindow *)toplevel;
    }

  found_window = _gdk_quartz_window_find_child (window, x_tmp, y_tmp,
                                                FALSE);

  /* We never return the root window. */
  if (found_window == _gdk_root)
    found_window = NULL;

  *x = x_tmp;
  *y = y_tmp;

  return found_window;
}

static gboolean
gdk_window_quartz_get_device_state (GdkWindow       *window,
                                    GdkDevice       *device,
                                    gdouble          *x,
                                    gdouble          *y,
                                    GdkModifierType *mask)
{
  return gdk_window_quartz_get_device_state_helper (window,
                                                    device,
                                                    x, y, mask) != NULL;
}

static GdkEventMask
gdk_window_quartz_get_events (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return 0;
  else
    return window->event_mask;
}

static void
gdk_window_quartz_set_events (GdkWindow       *window,
                              GdkEventMask     event_mask)
{
  /* The mask is set in the common code. */
}

static void
gdk_quartz_window_set_urgency_hint (GdkWindow *window,
                                    gboolean   urgent)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  /* FIXME: Implement */
}

static void
gdk_quartz_window_set_geometry_hints (GdkWindow         *window,
                                      const GdkGeometry *geometry,
                                      GdkWindowHints     geom_mask)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (geometry != NULL);

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;
  
  impl = gdk_window_get_quartz_impl (window);
  if (!(impl && impl->toplevel))
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
gdk_quartz_window_set_title (GdkWindow   *window,
                             const gchar *title)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (title != NULL);

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = gdk_window_get_quartz_impl (window);

  if (impl && impl->toplevel)
    {
      GDK_QUARTZ_ALLOC_POOL;
      [impl->toplevel setTitle:[NSString stringWithUTF8String:title]];
      GDK_QUARTZ_RELEASE_POOL;
    }
}

static void
gdk_quartz_window_set_role (GdkWindow   *window,
                            const gchar *role)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      WINDOW_IS_TOPLEVEL (window))
    return;

  /* FIXME: Implement */
}

static void
gdk_quartz_window_set_startup_id (GdkWindow   *window,
                                  const gchar *startup_id)
{
  /* FIXME: Implement? */
}

static void
gdk_quartz_window_set_transient_for (GdkWindow *window,
                                     GdkWindow *parent)
{
  GdkWindowImplQuartz *window_impl;
  GdkWindowImplQuartz *parent_impl;

  if (GDK_WINDOW_DESTROYED (window)  || GDK_WINDOW_DESTROYED (parent) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  window_impl = gdk_window_get_quartz_impl (window);
  if (!(window_impl && window_impl->toplevel))
    return;

  GDK_QUARTZ_ALLOC_POOL;

  if (window_impl->transient_for)
    {
      _gdk_quartz_window_detach_from_parent (window);

      g_object_unref (window_impl->transient_for);
      window_impl->transient_for = NULL;
    }

  parent_impl = gdk_window_get_quartz_impl (parent);
  if (parent_impl && parent_impl->toplevel)
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
          if (!(window->state & GDK_WINDOW_STATE_WITHDRAWN))
            _gdk_quartz_window_attach_to_parent (window);
        }
    }
  
  GDK_QUARTZ_RELEASE_POOL;
}

static void
gdk_window_quartz_shape_combine_region (GdkWindow       *window,
                                        const cairo_region_t *shape,
                                        gint             x,
                                        gint             y)
{
  /* FIXME: Implement */
}

static void
gdk_window_quartz_input_shape_combine_region (GdkWindow       *window,
                                              const cairo_region_t *shape_region,
                                              gint             offset_x,
                                              gint             offset_y)
{
  /* FIXME: Implement */
}

static void
gdk_quartz_window_set_override_redirect (GdkWindow *window,
                                         gboolean override_redirect)
{
  /* FIXME: Implement */
}

static void
gdk_quartz_window_set_accept_focus (GdkWindow *window,
                                    gboolean accept_focus)
{
  window->accept_focus = accept_focus != FALSE;
}

static void
gdk_quartz_window_set_focus_on_map (GdkWindow *window,
                                    gboolean focus_on_map)
{
  window->focus_on_map = focus_on_map != FALSE;
}

static void
gdk_quartz_window_set_icon_name (GdkWindow   *window,
                                 const gchar *name)
{
  /* FIXME: Implement */
}

static void
gdk_quartz_window_focus (GdkWindow *window,
                         guint32    timestamp)
{
  GdkWindowImplQuartz *impl;

  impl = gdk_window_get_quartz_impl (window);

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window) ||
      !impl)
    return;

  if (window->accept_focus && window->window_type != GDK_WINDOW_TEMP)
    {
      GDK_QUARTZ_ALLOC_POOL;
      [impl->toplevel makeKeyAndOrderFront:impl->toplevel];
      clear_toplevel_order ();
      GDK_QUARTZ_RELEASE_POOL;
    }
}

static gint
window_type_hint_to_level (GdkWindowTypeHint hint)
{
  /*  the order in this switch statement corresponds to the actual
   *  stacking order: the first group is top, the last group is bottom
   */
  switch (hint)
    {
    case GDK_WINDOW_TYPE_HINT_POPUP_MENU:
    case GDK_WINDOW_TYPE_HINT_COMBO:
    case GDK_WINDOW_TYPE_HINT_DND:
    case GDK_WINDOW_TYPE_HINT_TOOLTIP:
      return NSPopUpMenuWindowLevel;

    case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
    case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
      return NSStatusWindowLevel;

    case GDK_WINDOW_TYPE_HINT_MENU: /* Torn-off menu */
    case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU: /* Menu from menubar */
      return NSTornOffMenuWindowLevel;

    case GDK_WINDOW_TYPE_HINT_DOCK:
      return NSFloatingWindowLevel; /* NSDockWindowLevel is deprecated, and not replaced */

    case GDK_WINDOW_TYPE_HINT_UTILITY:
    case GDK_WINDOW_TYPE_HINT_DIALOG:  /* Dialog window */
    case GDK_WINDOW_TYPE_HINT_NORMAL:  /* Normal toplevel window */
    case GDK_WINDOW_TYPE_HINT_TOOLBAR: /* Window used to implement toolbars */
      return NSNormalWindowLevel;

    case GDK_WINDOW_TYPE_HINT_DESKTOP:
      return kCGDesktopWindowLevelKey; /* doesn't map to any real Cocoa model */

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

static gboolean
window_type_hint_to_hides_on_deactivate (GdkWindowTypeHint hint)
{
  switch (hint)
    {
    case GDK_WINDOW_TYPE_HINT_UTILITY:
    case GDK_WINDOW_TYPE_HINT_MENU: /* Torn-off menu */
    case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
    case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
    case GDK_WINDOW_TYPE_HINT_TOOLTIP:
      return TRUE;

    default:
      break;
    }

  return FALSE;
}

static void
_gdk_quartz_window_update_has_shadow (GdkWindowImplQuartz *impl)
{
    gboolean has_shadow;

    /* In case there is any shadow set we have to turn off the
     * NSWindow setHasShadow as the system drawn ones wont match our
     * window boundary anymore */
    has_shadow = (window_type_hint_to_shadow (impl->type_hint) && !impl->shadow_max);

    [impl->toplevel setHasShadow: has_shadow];
}

static void
_gdk_quartz_window_set_collection_behavior (NSWindow *nswindow,
                                            GdkWindowTypeHint hint)
{
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
#define GDK_QUARTZ_ALLOWS_TILING NSWindowCollectionBehaviorFullScreenAllowsTiling
#define GDK_QUARTZ_DISALLOWS_TILING NSWindowCollectionBehaviorFullScreenDisallowsTiling
#else
#define GDK_QUARTZ_ALLOWS_TILING 1 << 11
#define GDK_QUARTZ_DISALLOWS_TILING 1 << 12
#endif
  if (gdk_quartz_osx_version() >= GDK_OSX_LION)
    {
      /* Fullscreen Collection Behavior */
      NSWindowCollectionBehavior behavior = [nswindow collectionBehavior];
      switch (hint)
        {
        case GDK_WINDOW_TYPE_HINT_NORMAL:
        case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
          behavior &= ~(NSWindowCollectionBehaviorFullScreenAuxiliary &
                        GDK_QUARTZ_DISALLOWS_TILING);
          behavior |= (NSWindowCollectionBehaviorFullScreenPrimary |
                       GDK_QUARTZ_ALLOWS_TILING);

          break;
        default:
          behavior &= ~(NSWindowCollectionBehaviorFullScreenPrimary &
                        GDK_QUARTZ_ALLOWS_TILING);
          behavior |= (NSWindowCollectionBehaviorFullScreenAuxiliary |
                       GDK_QUARTZ_DISALLOWS_TILING);
          break;
        }
      [nswindow setCollectionBehavior:behavior];
    }
#undef GDK_QUARTZ_ALLOWS_TILING
#undef GDK_QUARTZ_DISALLOWS_TILING
#endif
}

static void
gdk_quartz_window_set_type_hint (GdkWindow        *window,
                                 GdkWindowTypeHint hint)
{
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = gdk_window_get_quartz_impl (window);

  if (!impl)
    return;

  if (hint == GDK_WINDOW_TYPE_HINT_NORMAL &&
      window->window_type == GDK_WINDOW_TEMP)
    hint = GDK_WINDOW_TYPE_HINT_POPUP_MENU;

  impl->type_hint = hint;

  /* Match the documentation, only do something if we're not mapped yet. */
  if (GDK_WINDOW_IS_MAPPED (window))
    return;

  _gdk_quartz_window_update_has_shadow (impl);
  if (impl->toplevel)
    _gdk_quartz_window_set_collection_behavior (impl->toplevel, hint);
  [impl->toplevel setLevel: window_type_hint_to_level (hint)];
  [impl->toplevel setHidesOnDeactivate: window_type_hint_to_hides_on_deactivate (hint)];
}

static GdkWindowTypeHint
gdk_quartz_window_get_type_hint (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return GDK_WINDOW_TYPE_HINT_NORMAL;
  
  return gdk_window_get_quartz_impl (window)->type_hint;
}

static void
gdk_quartz_window_set_modal_hint (GdkWindow *window,
                                  gboolean   modal)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  /* FIXME: Implement */
}

static void
gdk_quartz_window_set_skip_taskbar_hint (GdkWindow *window,
                                         gboolean   skips_taskbar)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  /* FIXME: Implement */
}

static void
gdk_quartz_window_set_skip_pager_hint (GdkWindow *window,
                                       gboolean   skips_pager)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  /* FIXME: Implement */
}

static void
gdk_quartz_window_begin_resize_drag (GdkWindow     *window,
                                     GdkWindowEdge  edge,
                                     GdkDevice     *device,
                                     gint           button,
                                     gint           root_x,
                                     gint           root_y,
                                     guint32        timestamp)
{
  GdkWindowImplQuartz *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = gdk_window_get_quartz_impl (window);

  if (!(impl && impl->toplevel))
    {
      g_warning ("Can't call gdk_window_begin_resize_drag on non-quartz or non-toplevel window");
      return;
    }

  [(GdkQuartzNSWindow *)impl->toplevel beginManualResize:edge];
}

static void
gdk_quartz_window_begin_move_drag (GdkWindow *window,
                                   GdkDevice *device,
                                   gint       button,
                                   gint       root_x,
                                   gint       root_y,
                                   guint32    timestamp)
{
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = gdk_window_get_quartz_impl (window);

  if (!(impl && impl->toplevel))
    {
      g_warning ("Can't call gdk_window_begin_move_drag on non-quartz or non-toplevel window");
      return;
    }

  [(GdkQuartzNSWindow *)impl->toplevel beginManualMove];
}

static void
gdk_quartz_window_set_icon_list (GdkWindow *window,
                                 GList     *pixbufs)
{
  /* FIXME: Implement */
}

static void
gdk_quartz_window_get_frame_extents (GdkWindow    *window,
                                     GdkRectangle *rect)
{
  GdkWindow *toplevel;
  GdkWindowImplQuartz *impl;
  NSRect ns_rect;

  g_return_if_fail (rect != NULL);


  rect->x = 0;
  rect->y = 0;
  rect->width = 1;
  rect->height = 1;
  
  toplevel = gdk_window_get_effective_toplevel (window);
  impl = gdk_window_get_quartz_impl (toplevel);

  if (!impl)
    return;

  ns_rect = [impl->toplevel frame];

  _gdk_quartz_window_xy_to_gdk_xy (ns_rect.origin.x,
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
gdk_quartz_window_set_decorations (GdkWindow       *window,
			    GdkWMDecoration  decorations)
{
  GdkWindowImplQuartz *impl;
  NSUInteger old_mask, new_mask;
  NSView *old_view;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = gdk_window_get_quartz_impl (window);

  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TEMP ||
      (impl &&
       impl->type_hint == GDK_WINDOW_TYPE_HINT_SPLASHSCREEN ))
    {
      new_mask = GDK_QUARTZ_BORDERLESS_WINDOW;
    }
  else if (decorations == 0) {
      new_mask = GDK_QUARTZ_BORDERLESS_WINDOW | GDK_QUARTZ_MINIATURIZABLE_WINDOW;
    }
  else
    {
      /* FIXME: Honor other GDK_DECOR_* flags. */
      new_mask = (GDK_QUARTZ_TITLED_WINDOW | GDK_QUARTZ_CLOSABLE_WINDOW |
                  GDK_QUARTZ_MINIATURIZABLE_WINDOW |
                  GDK_QUARTZ_RESIZABLE_WINDOW);
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
      if (old_mask == GDK_QUARTZ_BORDERLESS_WINDOW &&
          new_mask != GDK_QUARTZ_BORDERLESS_WINDOW)
        {
          rect = [NSWindow frameRectForContentRect:rect styleMask:new_mask];

        }
      else if (old_mask != GDK_QUARTZ_BORDERLESS_WINDOW &&
               new_mask == GDK_QUARTZ_BORDERLESS_WINDOW)
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

          /* It appears that unsetting and then resetting
           * GDK_QUARTZ_TITLED_WINDOW does not reset the title in the
           * title bar as might be expected.
           *
           * In theory we only need to set this if new_mask includes
           * GDK_QUARTZ_TITLED_WINDOW. This behaved extremely oddly when
           * conditionalized upon that and since it has no side effects (i.e.
           * if GDK_QUARTZ_TITLED_WINDOW is not requested, the title will not be
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
           * is set on GdkQuartzWindows.
           */
          [impl->toplevel close];

          impl->toplevel = [[GdkQuartzNSWindow alloc] initWithContentRect:rect
                                                                styleMask:new_mask
                                                                  backing:NSBackingStoreBuffered
                                                                    defer:NO
                                                                   screen:screen];
          _gdk_quartz_window_update_has_shadow (impl);

          [impl->toplevel setLevel: window_type_hint_to_level (impl->type_hint)];
          if (title)
            [impl->toplevel setTitle:title];
          [impl->toplevel setBackgroundColor:bg];
          [impl->toplevel setHidesOnDeactivate: window_type_hint_to_hides_on_deactivate (impl->type_hint)];
          [impl->toplevel setContentView:old_view];
        }

      if (new_mask == GDK_QUARTZ_BORDERLESS_WINDOW)
        {
          [impl->toplevel setContentSize:rect.size];
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
gdk_quartz_window_get_decorations (GdkWindow       *window,
                                   GdkWMDecoration *decorations)
{
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return FALSE;

  impl = gdk_window_get_quartz_impl (window);

  if (decorations)
    {
      /* Borderless is 0, so we can't check it as a bit being set. */
      if (impl &&
          [impl->toplevel styleMask] == GDK_QUARTZ_BORDERLESS_WINDOW)
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
gdk_quartz_window_set_functions (GdkWindow    *window,
                                 GdkWMFunction functions)
{
  GdkWindowImplQuartz *impl;
  gboolean min, max, close;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = gdk_window_get_quartz_impl (window);

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

  if (impl && impl->toplevel)
    {
      NSUInteger mask = [impl->toplevel styleMask];

      if (min)
        mask = mask | GDK_QUARTZ_MINIATURIZABLE_WINDOW;
      else
        mask = mask & ~GDK_QUARTZ_MINIATURIZABLE_WINDOW;

      if (max)
        mask = mask | GDK_QUARTZ_RESIZABLE_WINDOW;
      else
        mask = mask & ~GDK_QUARTZ_RESIZABLE_WINDOW;

      if (close)
        mask = mask | GDK_QUARTZ_CLOSABLE_WINDOW;
      else
        mask = mask & ~GDK_QUARTZ_CLOSABLE_WINDOW;

      [impl->toplevel setStyleMask:mask];
    }
}

static void
gdk_quartz_window_stick (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;
}

static void
gdk_quartz_window_unstick (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;
}

static void
gdk_quartz_window_maximize (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;
  gboolean maximized;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = gdk_window_get_quartz_impl (window);
  maximized = gdk_window_get_state (window) & GDK_WINDOW_STATE_MAXIMIZED;

  if (GDK_WINDOW_IS_MAPPED (window) &&
      impl)
    {
      GDK_QUARTZ_ALLOC_POOL;

      if (impl->toplevel && !maximized)
        [impl->toplevel zoom:nil];

      GDK_QUARTZ_RELEASE_POOL;
    }
}

static void
gdk_quartz_window_unmaximize (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;
  gboolean maximized;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = gdk_window_get_quartz_impl (window);
  maximized = gdk_window_get_state (window) & GDK_WINDOW_STATE_MAXIMIZED;

  if (GDK_WINDOW_IS_MAPPED (window) &&
      impl)
    {
      GDK_QUARTZ_ALLOC_POOL;

      if (impl->toplevel && maximized)
        [impl->toplevel zoom:nil];

      GDK_QUARTZ_RELEASE_POOL;
    }
}

static void
gdk_quartz_window_iconify (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = gdk_window_get_quartz_impl (window);

  if (GDK_WINDOW_IS_MAPPED (window) &&
      impl)
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

static void
gdk_quartz_window_deiconify (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = gdk_window_get_quartz_impl (window);

  if (GDK_WINDOW_IS_MAPPED (window) &&
      impl)
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

static gboolean
window_is_fullscreen (GdkWindow *window)
{
  GdkWindowImplQuartz *impl = gdk_window_get_quartz_impl (window);

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
  if (impl &&
      gdk_quartz_osx_version() >= GDK_OSX_LION)
    return ([impl->toplevel styleMask] & GDK_QUARTZ_FULLSCREEN_WINDOW) != 0;
  else
#endif
    return g_object_get_data (G_OBJECT (window), FULLSCREEN_DATA) != NULL;
}

static void
gdk_quartz_window_fullscreen (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl = gdk_window_get_quartz_impl (window);

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
  if (gdk_quartz_osx_version() >= GDK_OSX_LION &&
      impl)
    {
      if (!window_is_fullscreen (window))
        [impl->toplevel toggleFullScreen:nil];
    }
  else
    {
#endif
      FullscreenSavedGeometry *geometry;
      GdkWindowImplQuartz *impl = gdk_window_get_quartz_impl (window);
      NSRect frame;

      if (GDK_WINDOW_DESTROYED (window) ||
          !WINDOW_IS_TOPLEVEL (window) ||
          !impl)
        return;

      geometry = get_fullscreen_geometry (window);
      if (!geometry)
        {
          geometry = g_new (FullscreenSavedGeometry, 1);

          geometry->x = window->x;
          geometry->y = window->y;
          geometry->width = window->width;
          geometry->height = window->height;

          if (!gdk_window_get_decorations (window, &geometry->decor))
            geometry->decor = GDK_DECOR_ALL;

          g_object_set_data_full (G_OBJECT (window),
                                  FULLSCREEN_DATA, geometry, 
                                  g_free);

          gdk_window_set_decorations (window, 0);

          frame = [[impl->toplevel screen] frame];
          move_resize_window_internal (window,
                                       0, 0, 
                                       frame.size.width, frame.size.height);
          [impl->toplevel setContentSize:frame.size];
          [impl->toplevel makeKeyAndOrderFront:impl->toplevel];

          clear_toplevel_order ();
        }

      SetSystemUIMode (kUIModeAllHidden, kUIOptionAutoShowMenuBar);

      gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_FULLSCREEN);
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
    }
#endif
}

static void
gdk_quartz_window_unfullscreen (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
  if (gdk_quartz_osx_version() >= GDK_OSX_LION)
    {
      impl = gdk_window_get_quartz_impl (window);

      if (window_is_fullscreen (window) &&
          impl)
        [impl->toplevel toggleFullScreen:nil];
    }
  else
    {
#endif
      GdkWindowImplQuartz *impl = gdk_window_get_quartz_impl (window);
      FullscreenSavedGeometry *geometry;

      if (GDK_WINDOW_DESTROYED (window) ||
          !WINDOW_IS_TOPLEVEL (window) ||
          !impl)
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

          gdk_window_set_decorations (window, geometry->decor);

          g_object_set_data (G_OBJECT (window), FULLSCREEN_DATA, NULL);

          [impl->toplevel makeKeyAndOrderFront:impl->toplevel];
          clear_toplevel_order ();

          gdk_synthesize_window_state (window, GDK_WINDOW_STATE_FULLSCREEN, 0);
        }

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
    }
#endif
}

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
static FullscreenSavedGeometry *
get_fullscreen_geometry (GdkWindow *window)
{
  return g_object_get_data (G_OBJECT (window), FULLSCREEN_DATA);
}
#endif

void
_gdk_quartz_window_update_fullscreen_state (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) || !WINDOW_IS_TOPLEVEL (window))
    return;

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
  if (gdk_quartz_osx_version() >= GDK_OSX_LION)
    {
      gboolean is_fullscreen = window_is_fullscreen (window);
      gboolean was_fullscreen = (gdk_window_get_state (window) &
                                 GDK_WINDOW_STATE_FULLSCREEN) != 0;

      if (is_fullscreen != was_fullscreen)
        {
          if (is_fullscreen)
            gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_FULLSCREEN);
          else
            gdk_synthesize_window_state (window, GDK_WINDOW_STATE_FULLSCREEN, 0);
        }
    }
#endif
}

static void
gdk_quartz_window_set_keep_above (GdkWindow *window,
                                  gboolean   setting)
{
  GdkWindowImplQuartz *impl = gdk_window_get_quartz_impl (window);
  gint level;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (impl);

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  level = window_type_hint_to_level (gdk_window_get_type_hint (window));
  
  /* Adjust normal window level by one if necessary. */
  [impl->toplevel setLevel: level + (setting ? 1 : 0)];
}

static void
gdk_quartz_window_set_keep_below (GdkWindow *window,
                                  gboolean   setting)
{
  GdkWindowImplQuartz *impl = gdk_window_get_quartz_impl (window);
  gint level;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (impl);

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;
  
  level = window_type_hint_to_level (gdk_window_get_type_hint (window));
  
  /* Adjust normal window level by one if necessary. */
  [impl->toplevel setLevel: level - (setting ? 1 : 0)];
}

/* X11 "feature" not useful in other backends. */
static GdkWindow *
gdk_quartz_window_get_group (GdkWindow *window)
{
    return NULL;
}

/* X11 "feature" not useful in other backends. */
static void
gdk_quartz_window_set_group (GdkWindow *window,
                             GdkWindow *leader)
{
}

static void
gdk_quartz_window_destroy_notify (GdkWindow *window)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkSeat *seat = gdk_display_get_default_seat (display);
  gdk_seat_ungrab (seat);
}

static void
gdk_quartz_window_set_opacity (GdkWindow *window,
                               gdouble    opacity)
{
  GdkWindowImplQuartz *impl = gdk_window_get_quartz_impl (window);

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (WINDOW_IS_TOPLEVEL (window));
  g_return_if_fail (impl);

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  if (opacity < 0)
    opacity = 0;
  else if (opacity > 1)
    opacity = 1;

  [impl->toplevel setAlphaValue: opacity];
}

static void
gdk_quartz_window_set_shadow_width (GdkWindow *window,
                                    gint       left,
                                    gint       right,
                                    gint       top,
                                    gint       bottom)
{
  GdkWindowImplQuartz *impl = gdk_window_get_quartz_impl (window);

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (WINDOW_IS_TOPLEVEL (window));
  g_return_if_fail (impl);

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  impl->shadow_top = top;
  impl->shadow_max = MAX (MAX (left, right), MAX (top, bottom));
  _gdk_quartz_window_update_has_shadow (impl);
}

static cairo_region_t *
gdk_quartz_window_get_shape (GdkWindow *window)
{
  /* FIXME: implement */
  return NULL;
}

static cairo_region_t *
gdk_quartz_window_get_input_shape (GdkWindow *window)
{
  /* FIXME: implement */
  return NULL;
}

/* Protocol to build cleanly for OSX < 10.7 */
@protocol ScaleFactor
- (CGFloat) backingScaleFactor;
@end

static gint
gdk_quartz_window_get_scale_factor (GdkWindow *window)
{
  GdkWindowImplQuartz *impl;

  if (GDK_WINDOW_DESTROYED (window))
    return 1;

  impl = gdk_window_get_quartz_impl (window);

  if (impl && impl->toplevel != NULL &&
      gdk_quartz_osx_version() >= GDK_OSX_LION)
    return [(id <ScaleFactor>) impl->toplevel backingScaleFactor];

  return 1;
}

static void
gdk_window_impl_quartz_class_init (GdkWindowImplQuartzClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkWindowImplClass *impl_class = GDK_WINDOW_IMPL_CLASS (klass);
  GdkWindowImplQuartzClass *impl_quartz_class = GDK_WINDOW_IMPL_QUARTZ_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_window_impl_quartz_finalize;

  impl_class->ref_cairo_surface = gdk_quartz_ref_cairo_surface;
  impl_class->show = gdk_window_quartz_show;
  impl_class->hide = gdk_window_quartz_hide;
  impl_class->withdraw = gdk_window_quartz_withdraw;
  impl_class->set_events = gdk_window_quartz_set_events;
  impl_class->get_events = gdk_window_quartz_get_events;
  impl_class->raise = gdk_window_quartz_raise;
  impl_class->lower = gdk_window_quartz_lower;
  impl_class->restack_toplevel = gdk_window_quartz_restack_toplevel;
  impl_class->move_resize = gdk_window_quartz_move_resize;
  impl_class->set_background = gdk_window_quartz_set_background;
  impl_class->reparent = gdk_window_quartz_reparent;
  impl_class->set_device_cursor = gdk_window_quartz_set_device_cursor;
  impl_class->get_geometry = gdk_window_quartz_get_geometry;
  impl_class->get_root_coords = gdk_window_quartz_get_root_coords;
  impl_class->get_device_state = gdk_window_quartz_get_device_state;
  impl_class->shape_combine_region = gdk_window_quartz_shape_combine_region;
  impl_class->input_shape_combine_region = gdk_window_quartz_input_shape_combine_region;
  impl_class->destroy = gdk_quartz_window_destroy;
  impl_class->destroy_foreign = gdk_quartz_window_destroy_foreign;
  impl_class->get_shape = gdk_quartz_window_get_shape;
  impl_class->get_input_shape = gdk_quartz_window_get_input_shape;
  impl_class->begin_paint = gdk_window_impl_quartz_begin_paint;
  impl_class->get_scale_factor = gdk_quartz_window_get_scale_factor;

  impl_class->focus = gdk_quartz_window_focus;
  impl_class->set_type_hint = gdk_quartz_window_set_type_hint;
  impl_class->get_type_hint = gdk_quartz_window_get_type_hint;
  impl_class->set_modal_hint = gdk_quartz_window_set_modal_hint;
  impl_class->set_skip_taskbar_hint = gdk_quartz_window_set_skip_taskbar_hint;
  impl_class->set_skip_pager_hint = gdk_quartz_window_set_skip_pager_hint;
  impl_class->set_urgency_hint = gdk_quartz_window_set_urgency_hint;
  impl_class->set_geometry_hints = gdk_quartz_window_set_geometry_hints;
  impl_class->set_title = gdk_quartz_window_set_title;
  impl_class->set_role = gdk_quartz_window_set_role;
  impl_class->set_startup_id = gdk_quartz_window_set_startup_id;
  impl_class->set_transient_for = gdk_quartz_window_set_transient_for;
  impl_class->get_frame_extents = gdk_quartz_window_get_frame_extents;
  impl_class->set_override_redirect = gdk_quartz_window_set_override_redirect;
  impl_class->set_accept_focus = gdk_quartz_window_set_accept_focus;
  impl_class->set_focus_on_map = gdk_quartz_window_set_focus_on_map;
  impl_class->set_icon_list = gdk_quartz_window_set_icon_list;
  impl_class->set_icon_name = gdk_quartz_window_set_icon_name;
  impl_class->iconify = gdk_quartz_window_iconify;
  impl_class->deiconify = gdk_quartz_window_deiconify;
  impl_class->stick = gdk_quartz_window_stick;
  impl_class->unstick = gdk_quartz_window_unstick;
  impl_class->maximize = gdk_quartz_window_maximize;
  impl_class->unmaximize = gdk_quartz_window_unmaximize;
  impl_class->fullscreen = gdk_quartz_window_fullscreen;
  impl_class->unfullscreen = gdk_quartz_window_unfullscreen;
  impl_class->set_keep_above = gdk_quartz_window_set_keep_above;
  impl_class->set_keep_below = gdk_quartz_window_set_keep_below;
  impl_class->get_group = gdk_quartz_window_get_group;
  impl_class->set_group = gdk_quartz_window_set_group;
  impl_class->set_decorations = gdk_quartz_window_set_decorations;
  impl_class->get_decorations = gdk_quartz_window_get_decorations;
  impl_class->set_functions = gdk_quartz_window_set_functions;
  impl_class->begin_resize_drag = gdk_quartz_window_begin_resize_drag;
  impl_class->begin_move_drag = gdk_quartz_window_begin_move_drag;
  impl_class->set_opacity = gdk_quartz_window_set_opacity;
  impl_class->set_shadow_width = gdk_quartz_window_set_shadow_width;
  impl_class->destroy_notify = gdk_quartz_window_destroy_notify;
  impl_class->register_dnd = _gdk_quartz_window_register_dnd;
  impl_class->drag_begin = _gdk_quartz_window_drag_begin;
  impl_class->process_updates_recurse = _gdk_quartz_window_process_updates_recurse;
  impl_class->sync_rendering = _gdk_quartz_window_sync_rendering;
  impl_class->simulate_key = _gdk_quartz_window_simulate_key;
  impl_class->simulate_button = _gdk_quartz_window_simulate_button;
  impl_class->get_property = _gdk_quartz_window_get_property;
  impl_class->change_property = _gdk_quartz_window_change_property;
  impl_class->delete_property = _gdk_quartz_window_delete_property;

  impl_class->create_gl_context = gdk_quartz_window_create_gl_context;
  impl_class->invalidate_for_new_frame = gdk_quartz_window_invalidate_for_new_frame;

  impl_quartz_class->get_context = gdk_window_impl_quartz_get_context;
  impl_quartz_class->release_context = gdk_window_impl_quartz_release_context;
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

      object_type = g_type_register_static (GDK_TYPE_WINDOW_IMPL,
                                            "GdkWindowImplQuartz",
                                            &object_info, 0);
    }

  return object_type;
}

CGContextRef
gdk_quartz_window_get_context (GdkWindowImplQuartz  *window,
                               gboolean             antialias)
{
  if (!GDK_WINDOW_IMPL_QUARTZ_GET_CLASS (window)->get_context)
    {
      g_warning ("%s doesn't implement GdkWindowImplQuartzClass::get_context()",
                 G_OBJECT_TYPE_NAME (window));
      return NULL;
    }

  return GDK_WINDOW_IMPL_QUARTZ_GET_CLASS (window)->get_context (window, antialias);
}

void
gdk_quartz_window_release_context (GdkWindowImplQuartz  *window,
                                   CGContextRef          cg_context)
{
  if (!GDK_WINDOW_IMPL_QUARTZ_GET_CLASS (window)->release_context)
    {
      g_warning ("%s doesn't implement GdkWindowImplQuartzClass::release_context()",
                 G_OBJECT_TYPE_NAME (window));
      return;
    }

  g_return_if_fail (cg_context);
  GDK_WINDOW_IMPL_QUARTZ_GET_CLASS (window)->release_context (window, cg_context);
}

/* macOS doesn't define a root window, but Gdk needs one for two
 * purposes: To be a parent reference for some toplevels and to be a
 * fallback window when gdk_window_create_image_surface is called with
 * a NULL GdkWindow.
 *
 */

static CGContextRef gdk_root_window_impl_quartz_get_context (GdkWindowImplQuartz *window, gboolean antialias);
static void gdk_root_window_impl_quartz_release_context (GdkWindowImplQuartz *window, CGContextRef cg_context);

static void
gdk_root_window_impl_quartz_class_init (GdkRootWindowImplQuartzClass *klass)
{
  GdkWindowImplQuartzClass *window_quartz_class = GDK_WINDOW_IMPL_QUARTZ_CLASS (klass);

  root_window_parent_class = g_type_class_peek_parent (klass);

  window_quartz_class->get_context = gdk_root_window_impl_quartz_get_context;
  window_quartz_class->release_context = gdk_root_window_impl_quartz_release_context;
}

static void
gdk_root_window_impl_quartz_init (GdkRootWindowImplQuartz *impl)
{
  CGColorSpaceRef colorspace =  CGColorSpaceCreateDeviceRGB ();
  /* Alpha channel Info: Cairo, CGImage, and CVPixelBuffer all use
   * kCGImageAlphaPremultipliedFirst, CALayer.contents wants
   * kCGImageAlphaPremultipliedLast.
   */
  CGBitmapInfo info = (CGBitmapInfo)kCGImageAlphaPremultipliedLast;
  impl->cg_context = CGBitmapContextCreate (NULL, 1, 1, 8, 4,
                                            colorspace, info);
  CGColorSpaceRelease (colorspace);
  impl->cg_layers = NULL;
}

static void
gdk_root_window_impl_quartz_dispose (GdkRootWindowImplQuartz *impl)
{
  g_list_free_full (impl->cg_layers, (GDestroyNotify)CGLayerRelease);
}

GType
_gdk_root_window_impl_quartz_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
        {
          sizeof (GdkRootWindowImplQuartzClass),
          (GBaseInitFunc) NULL,
          (GBaseFinalizeFunc) NULL,
          (GClassInitFunc) gdk_root_window_impl_quartz_class_init,
          NULL,           /* class_finalize */
          NULL,           /* class_data */
          sizeof (GdkRootWindowImplQuartz),
          0,              /* n_preallocs */
          (GInstanceInitFunc) gdk_root_window_impl_quartz_init,
        };

      object_type = g_type_register_static (GDK_TYPE_WINDOW_IMPL_QUARTZ,
                                            "GdkRootWindowQuartz",
                                            &object_info, 0);
    }

  return object_type;
}


static CGContextRef
gdk_root_window_impl_quartz_get_context (GdkWindowImplQuartz *window_impl,
                                         gboolean             antialias)
{
   GdkRootWindowImplQuartz *impl = GDK_ROOT_WINDOW_IMPL_QUARTZ (window_impl);
   CGSize size;
   CGLayerRef layer;

  if (!window_impl || GDK_WINDOW_DESTROYED (window_impl->wrapper))
    return NULL;

  size.width = gdk_window_get_width (window_impl->wrapper);
  size.height = gdk_window_get_height (window_impl->wrapper);
  layer = CGLayerCreateWithContext(impl->cg_context, size, NULL);
  impl->cg_layers = g_list_prepend(impl->cg_layers, CGLayerRetain (layer));
  return CGContextRetain (CGLayerGetContext (layer));
}

static void
gdk_root_window_impl_quartz_release_context (GdkWindowImplQuartz *window,
                                             CGContextRef         cg_context)
{
  CGContextRelease (cg_context);
}
