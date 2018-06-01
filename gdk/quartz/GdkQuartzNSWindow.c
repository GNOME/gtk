/* GdkQuartzSurface.m
 *
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
#import "GdkQuartzNSWindow.h"
#include "gdkquartzsurface.h"
#include "gdkdnd-quartz.h"
#include "gdkprivate-quartz.h"

@implementation GdkQuartzNSWindow

- (void)windowWillClose:(NSNotification*)notification
{
  // Clears the delegate when window is going to be closed; since EL
  // Capitan it is possible that the methods of delegate would get
  // called after the window has been closed.
  [self setDelegate:nil];
}

-(BOOL)windowShouldClose:(id)sender
{
  GdkSurface *window = [[self contentView] gdkSurface];
  GdkEvent *event;

  event = gdk_event_new (GDK_DELETE);

  event->any.surface = g_object_ref (window);
  event->any.send_event = FALSE;

  _gdk_event_queue_append (gdk_display_get_default (), event);

  return NO;
}

-(void)windowWillMiniaturize:(NSNotification *)aNotification
{
  GdkSurface *window = [[self contentView] gdkSurface];

  _gdk_quartz_surface_detach_from_parent (window);
}

-(void)windowDidMiniaturize:(NSNotification *)aNotification
{
  GdkSurface *window = [[self contentView] gdkSurface];

  gdk_synthesize_surface_state (window, 0, 
			       GDK_SURFACE_STATE_ICONIFIED);
}

-(void)windowDidDeminiaturize:(NSNotification *)aNotification
{
  GdkSurface *window = [[self contentView] gdkSurface];

  _gdk_quartz_surface_attach_to_parent (window);

  gdk_synthesize_surface_state (window, GDK_SURFACE_STATE_ICONIFIED, 0);
}

-(void)windowDidBecomeKey:(NSNotification *)aNotification
{
  GdkSurface *window = [[self contentView] gdkSurface];

  gdk_synthesize_surface_state (window, 0, GDK_SURFACE_STATE_FOCUSED);
  _gdk_quartz_events_update_focus_window (window, TRUE);
}

-(void)windowDidResignKey:(NSNotification *)aNotification
{
  GdkSurface *window = [[self contentView] gdkSurface];

  _gdk_quartz_events_update_focus_window (window, FALSE);
  gdk_synthesize_surface_state (window, GDK_SURFACE_STATE_FOCUSED, 0);
}

-(void)windowDidBecomeMain:(NSNotification *)aNotification
{
  GdkSurface *window = [[self contentView] gdkSurface];

  if (![self isVisible])
    {
      /* Note: This is a hack needed because for unknown reasons, hidden
       * windows get shown when clicking the dock icon when the application
       * is not already active.
       */
      [self orderOut:nil];
      return;
    }

  _gdk_quartz_surface_did_become_main (window);
}

-(void)windowDidResignMain:(NSNotification *)aNotification
{
  GdkSurface *window;

  window = [[self contentView] gdkSurface];
  _gdk_quartz_surface_did_resign_main (window);
}

/* Used in combination with NSLeftMouseUp in sendEvent to keep track
 * of when the window is being moved with the mouse.
 */
-(void)windowWillMove:(NSNotification *)aNotification
{
  inMove = YES;
}

-(void)sendEvent:(NSEvent *)event
{
  switch ([event type])
    {
    case NSLeftMouseUp:
    {
      double time = ((double)[event timestamp]) * 1000.0;

      _gdk_quartz_events_break_all_grabs (time);
      inManualMove = NO;
      inManualResize = NO;
      inMove = NO;
      break;
    }

    case NSLeftMouseDragged:
      if ([self trackManualMove] || [self trackManualResize])
        return;
      break;

    default:
      break;
    }

  [super sendEvent:event];
}

-(BOOL)isInMove
{
  return inMove;
}

-(void)checkSendEnterNotify
{
  GdkSurface *window = [[self contentView] gdkSurface];
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  /* When a new window has been created, and the mouse
   * is in the window area, we will not receive an NSMouseEntered
   * event.  Therefore, we synthesize an enter notify event manually.
   */
  if (!initialPositionKnown)
    {
      initialPositionKnown = YES;

      if (NSPointInRect ([NSEvent mouseLocation], [self frame]))
        {
          NSEvent *event;

          event = [NSEvent enterExitEventWithType: NSMouseEntered
                                         location: [self mouseLocationOutsideOfEventStream]
                                    modifierFlags: 0
                                        timestamp: [[NSApp currentEvent] timestamp]
                                     windowNumber: [impl->toplevel windowNumber]
                                          context: NULL
                                      eventNumber: 0
                                   trackingNumber: [impl->view trackingRect]
                                         userData: nil];

          [NSApp postEvent:event atStart:NO];
        }
    }
}

-(void)windowDidMove:(NSNotification *)aNotification
{
  GdkSurface *window = [[self contentView] gdkSurface];
  GdkEvent *event;

  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
  gboolean maximized = gdk_surface_get_state (window) & GDK_SURFACE_STATE_MAXIMIZED;

  /* In case the window is changed when maximized remove the maximized state */
  if (maximized && !inMaximizeTransition && !NSEqualRects (lastMaximizedFrame, [self frame]))
    {
      gdk_synthesize_surface_state (window,
                                   GDK_SURFACE_STATE_MAXIMIZED,
                                   0);
    }

  _gdk_quartz_surface_update_position (window);

  /* Synthesize a configure event */
  event = gdk_event_new (GDK_CONFIGURE);
  event->configure.window = g_object_ref (window);
  event->configure.x = window->x;
  event->configure.y = window->y;
  event->configure.width = window->width;
  event->configure.height = window->height;

  _gdk_event_queue_append (gdk_display_get_default (), event);

  [self checkSendEnterNotify];
}

-(void)windowDidResize:(NSNotification *)aNotification
{
  NSRect content_rect = [self contentRectForFrameRect:[self frame]];
  GdkSurface *window = [[self contentView] gdkSurface];
  GdkEvent *event;
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
  gboolean maximized = gdk_surface_get_state (window) & GDK_SURFACE_STATE_MAXIMIZED;

  /* see same in windowDidMove */
  if (maximized && !inMaximizeTransition && !NSEqualRects (lastMaximizedFrame, [self frame]))
    {
      gdk_synthesize_surface_state (window,
                                   GDK_SURFACE_STATE_MAXIMIZED,
                                   0);
    }

  window->width = content_rect.size.width;
  window->height = content_rect.size.height;

  /* Certain resize operations (e.g. going fullscreen), also move the
   * origin of the window.
   */
  _gdk_quartz_surface_update_position (window);

  [[self contentView] setFrame:NSMakeRect (0, 0, window->width, window->height)];

  _gdk_surface_update_size (window);

  /* Synthesize a configure event */
  event = gdk_event_new (GDK_CONFIGURE);
  event->configure.window = g_object_ref (window);
  event->configure.x = window->x;
  event->configure.y = window->y;
  event->configure.width = window->width;
  event->configure.height = window->height;

  _gdk_event_queue_append (gdk_display_get_default (), event);

  [self checkSendEnterNotify];
}

-(id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask backing:(NSBackingStoreType)backingType defer:(BOOL)flag screen:(NSScreen *)screen
{
  self = [super initWithContentRect:contentRect
	                  styleMask:styleMask
	                    backing:backingType
	                      defer:flag
                             screen:screen];

  [self setAcceptsMouseMovedEvents:YES];
  [self setDelegate:self];
  [self setReleasedWhenClosed:YES];

  return self;
}

-(BOOL)canBecomeMainWindow
{
  GdkSurface *window = [[self contentView] gdkSurface];
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  switch (impl->type_hint)
    {
    case GDK_SURFACE_TYPE_HINT_NORMAL:
    case GDK_SURFACE_TYPE_HINT_DIALOG:
      return YES;
      
    case GDK_SURFACE_TYPE_HINT_MENU:
    case GDK_SURFACE_TYPE_HINT_TOOLBAR:
    case GDK_SURFACE_TYPE_HINT_SPLASHSCREEN:
    case GDK_SURFACE_TYPE_HINT_UTILITY:
    case GDK_SURFACE_TYPE_HINT_DOCK:
    case GDK_SURFACE_TYPE_HINT_DESKTOP:
    case GDK_SURFACE_TYPE_HINT_DROPDOWN_MENU:
    case GDK_SURFACE_TYPE_HINT_POPUP_MENU:
    case GDK_SURFACE_TYPE_HINT_TOOLTIP:
    case GDK_SURFACE_TYPE_HINT_NOTIFICATION:
    case GDK_SURFACE_TYPE_HINT_COMBO:
    case GDK_SURFACE_TYPE_HINT_DND:
      return NO;
    }
  
  return YES;
}

-(BOOL)canBecomeKeyWindow
{
  GdkSurface *window = [[self contentView] gdkSurface];
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (!window->accept_focus)
    return NO;

  /* Popup windows should not be able to get focused in the window
   * manager sense, it's only handled through grabs.
   */
  if (window->surface_type == GDK_SURFACE_TEMP)
    return NO;

  switch (impl->type_hint)
    {
    case GDK_SURFACE_TYPE_HINT_NORMAL:
    case GDK_SURFACE_TYPE_HINT_DIALOG:
    case GDK_SURFACE_TYPE_HINT_MENU:
    case GDK_SURFACE_TYPE_HINT_TOOLBAR:
    case GDK_SURFACE_TYPE_HINT_UTILITY:
    case GDK_SURFACE_TYPE_HINT_DOCK:
    case GDK_SURFACE_TYPE_HINT_DESKTOP:
    case GDK_SURFACE_TYPE_HINT_DROPDOWN_MENU:
    case GDK_SURFACE_TYPE_HINT_POPUP_MENU:
    case GDK_SURFACE_TYPE_HINT_COMBO:
      return YES;
      
    case GDK_SURFACE_TYPE_HINT_SPLASHSCREEN:
    case GDK_SURFACE_TYPE_HINT_TOOLTIP:
    case GDK_SURFACE_TYPE_HINT_NOTIFICATION:
    case GDK_SURFACE_TYPE_HINT_DND:
      return NO;
    }
  
  return YES;
}

- (void)showAndMakeKey:(BOOL)makeKey
{
  GdkSurface *window = [[self contentView] gdkSurface];
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  inShowOrHide = YES;

  if (makeKey)
    [impl->toplevel makeKeyAndOrderFront:impl->toplevel];
  else
    [impl->toplevel orderFront:nil];

  inShowOrHide = NO;

  [self checkSendEnterNotify];
}

- (void)hide
{
  GdkSurface *window = [[self contentView] gdkSurface];
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  inShowOrHide = YES;
  [impl->toplevel orderOut:nil];
  inShowOrHide = NO;

  initialPositionKnown = NO;
}

- (BOOL)trackManualMove
{
  GdkSurface *window = [[self contentView] gdkSurface];
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
  NSPoint currentLocation;
  NSPoint newOrigin;
  NSRect screenFrame = [[NSScreen mainScreen] visibleFrame];
  NSRect windowFrame = [self frame];

  if (!inManualMove)
    return NO;

  currentLocation = [self convertBaseToScreen:[self mouseLocationOutsideOfEventStream]];
  newOrigin.x = currentLocation.x - initialMoveLocation.x;
  newOrigin.y = currentLocation.y - initialMoveLocation.y;

  /* Clamp vertical position to below the menu bar. */
  if (newOrigin.y + windowFrame.size.height - impl->shadow_top > screenFrame.origin.y + screenFrame.size.height)
    newOrigin.y = screenFrame.origin.y + screenFrame.size.height - windowFrame.size.height + impl->shadow_top;

  [self setFrameOrigin:newOrigin];

  return YES;
}

/* Used by gdkevents-quartz.c to decide if our sendEvent() handler above
 * will see the event or if it will be subjected to standard processing
 * by GDK.
*/
-(BOOL)isInManualResizeOrMove
{
  return inManualResize || inManualMove;
}

-(void)beginManualMove
{
  NSRect frame = [self frame];

  if (inMove || inManualMove || inManualResize)
    return;

  inManualMove = YES;

  initialMoveLocation = [self convertBaseToScreen:[self mouseLocationOutsideOfEventStream]];
  initialMoveLocation.x -= frame.origin.x;
  initialMoveLocation.y -= frame.origin.y;
}

- (BOOL)trackManualResize
{
  NSPoint mouse_location;
  NSRect new_frame;
  float mdx, mdy, dw, dh, dx, dy;
  NSSize min_size;

  if (!inManualResize || inTrackManualResize)
    return NO;

  inTrackManualResize = YES;

  mouse_location = [self convertBaseToScreen:[self mouseLocationOutsideOfEventStream]];
  mdx = initialResizeLocation.x - mouse_location.x;
  mdy = initialResizeLocation.y - mouse_location.y;

  /* Set how a mouse location delta translates to changes in width,
   * height and position.
   */
  dw = dh = dx = dy = 0.0;
  if (resizeEdge == GDK_SURFACE_EDGE_EAST ||
      resizeEdge == GDK_SURFACE_EDGE_NORTH_EAST ||
      resizeEdge == GDK_SURFACE_EDGE_SOUTH_EAST)
    {
      dw = -1.0;
    }
  if (resizeEdge == GDK_SURFACE_EDGE_NORTH ||
      resizeEdge == GDK_SURFACE_EDGE_NORTH_WEST ||
      resizeEdge == GDK_SURFACE_EDGE_NORTH_EAST)
    {
      dh = -1.0;
    }
  if (resizeEdge == GDK_SURFACE_EDGE_SOUTH ||
      resizeEdge == GDK_SURFACE_EDGE_SOUTH_WEST ||
      resizeEdge == GDK_SURFACE_EDGE_SOUTH_EAST)
    {
      dh = 1.0;
      dy = -1.0;
    }
  if (resizeEdge == GDK_SURFACE_EDGE_WEST ||
      resizeEdge == GDK_SURFACE_EDGE_NORTH_WEST ||
      resizeEdge == GDK_SURFACE_EDGE_SOUTH_WEST)
    {
      dw = 1.0;
      dx = -1.0;
    }

  /* Apply changes to the frame captured when we started resizing */
  new_frame = initialResizeFrame;
  new_frame.origin.x += mdx * dx;
  new_frame.origin.y += mdy * dy;
  new_frame.size.width += mdx * dw;
  new_frame.size.height += mdy * dh;

  /* In case the resulting window would be too small reduce the
   * change to both size and position.
   */
  min_size = [self contentMinSize];

  if (new_frame.size.width < min_size.width)
    {
      if (dx)
        new_frame.origin.x -= min_size.width - new_frame.size.width;
      new_frame.size.width = min_size.width;
    }

  if (new_frame.size.height < min_size.height)
    {
      if (dy)
        new_frame.origin.y -= min_size.height - new_frame.size.height;
      new_frame.size.height = min_size.height;
    }

  /* We could also apply aspect ratio:
     new_frame.size.height = new_frame.size.width / [self aspectRatio].width * [self aspectRatio].height;
  */

  [self setFrame:new_frame display:YES];

  /* Let the resizing be handled by GTK+. */
  if (g_main_context_pending (NULL))
    g_main_context_iteration (NULL, FALSE);

  inTrackManualResize = NO;

  return YES;
}

-(void)beginManualResize:(GdkSurfaceEdge)edge
{
  if (inMove || inManualMove || inManualResize)
    return;

  inManualResize = YES;
  resizeEdge = edge;

  initialResizeFrame = [self frame];
  initialResizeLocation = [self convertBaseToScreen:[self mouseLocationOutsideOfEventStream]];
}



static GdkDragContext *current_context = NULL;

static GdkDragAction
drag_operation_to_drag_action (NSDragOperation operation)
{
  GdkDragAction result = 0;

  /* GDK and Quartz drag operations do not map 1:1.
   * This mapping represents about the best that we
   * can come up.
   */

  if (operation & NSDragOperationGeneric)
    result |= GDK_ACTION_MOVE;
  if (operation & NSDragOperationCopy)
    result |= GDK_ACTION_COPY;
  if (operation & NSDragOperationMove)
    result |= GDK_ACTION_MOVE;
  if (operation & NSDragOperationLink)
    result |= GDK_ACTION_LINK;

  return result;
}

static NSDragOperation
drag_action_to_drag_operation (GdkDragAction action)
{
  NSDragOperation result = 0;

  if (action & GDK_ACTION_COPY)
    result |= NSDragOperationCopy;
  if (action & GDK_ACTION_LINK)
    result |= NSDragOperationLink;
  if (action & GDK_ACTION_MOVE)
    result |= NSDragOperationMove;

  return result;
}

static void
update_context_from_dragging_info (id <NSDraggingInfo> sender)
{
  g_assert (current_context != NULL);

  GDK_QUARTZ_DRAG_CONTEXT (current_context)->dragging_info = sender;
  current_context->suggested_action = drag_operation_to_drag_action ([sender draggingSourceOperationMask]);
  current_context->actions = current_context->suggested_action;
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
  GdkEvent *event;
  GdkSurface *window;

  if (current_context)
    g_object_unref (current_context);

  current_context = g_object_new (GDK_TYPE_QUARTZ_DRAG_CONTEXT,
                                  "device", gdk_seat_get_pointer (gdk_display_get_default_seat (current_context->display)),
                                  NULL);
  update_context_from_dragging_info (sender);

  window = [[self contentView] gdkSurface];

  event = gdk_event_new (GDK_DRAG_ENTER);
  event->dnd.window = g_object_ref (window);
  event->dnd.send_event = FALSE;
  event->dnd.context = g_object_ref (current_context);
  event->dnd.time = GDK_CURRENT_TIME;

  gdk_event_set_device (event, gdk_drag_context_get_device (current_context));
  gdk_event_set_seat (event, gdk_device_get_seat (gdk_drag_context_get_device (current_context)));

  _gdk_event_emit (event);

  g_object_unref (event);

  return NSDragOperationNone;
}

- (void)draggingEnded:(id <NSDraggingInfo>)sender
{
  /* leave a note for the source about what action was taken */
  if (_gdk_quartz_drag_source_context && current_context)
   _gdk_quartz_drag_source_context->action = current_context->action;

  if (current_context)
    g_object_unref (current_context);
  current_context = NULL;
}

- (void)draggingExited:(id <NSDraggingInfo>)sender
{
  GdkEvent *event;
  
  event = gdk_event_new (GDK_DRAG_LEAVE);
  event->dnd.window = g_object_ref ([[self contentView] gdkSurface]);
  event->dnd.send_event = FALSE;
  event->dnd.context = g_object_ref (current_context);
  event->dnd.time = GDK_CURRENT_TIME;

  gdk_event_set_device (event, gdk_drag_context_get_device (current_context));
  gdk_event_set_seat (event, gdk_device_get_seat (gdk_drag_context_get_device (current_context)));

  _gdk_event_emit (event);

  g_object_unref (event);
  
  g_object_unref (current_context);
  current_context = NULL;
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender
{
  NSPoint point = [sender draggingLocation];
  NSPoint screen_point = [self convertBaseToScreen:point];
  GdkEvent *event;
  int gx, gy;

  update_context_from_dragging_info (sender);
  _gdk_quartz_surface_nspoint_to_gdk_xy (screen_point, &gx, &gy);

  event = gdk_event_new (GDK_DRAG_MOTION);
  event->dnd.window = g_object_ref ([[self contentView] gdkSurface]);
  event->dnd.send_event = FALSE;
  event->dnd.context = g_object_ref (current_context);
  event->dnd.time = GDK_CURRENT_TIME;
  event->dnd.x_root = gx;
  event->dnd.y_root = gy;

  gdk_event_set_device (event, gdk_drag_context_get_device (current_context));
  gdk_event_set_seat (event, gdk_device_get_seat (gdk_drag_context_get_device (current_context)));

  _gdk_event_emit (event);

  g_object_unref (event);

  return drag_action_to_drag_operation (current_context->action);
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
  NSPoint point = [sender draggingLocation];
  NSPoint screen_point = [self convertBaseToScreen:point];
  GdkEvent *event;
  int gy, gx;

  update_context_from_dragging_info (sender);
  _gdk_quartz_surface_nspoint_to_gdk_xy (screen_point, &gx, &gy);

  event = gdk_event_new (GDK_DROP_START);
  event->dnd.window = g_object_ref ([[self contentView] gdkSurface]);
  event->dnd.send_event = FALSE;
  event->dnd.context = g_object_ref (current_context);
  event->dnd.time = GDK_CURRENT_TIME;
  event->dnd.x_root = gx;
  event->dnd.y_root = gy;

  gdk_event_set_device (event, gdk_drag_context_get_device (current_context));
  gdk_event_set_seat (event, gdk_device_get_seat (gdk_drag_context_get_device (current_context)));

  _gdk_event_emit (event);

  g_object_unref (event);

  g_object_unref (current_context);
  current_context = NULL;

  return YES;
}

- (BOOL)wantsPeriodicDraggingUpdates
{
  return NO;
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
  GdkEvent *event;
  GdkDisplay *display;
  GdkDevice *device;

  g_assert (_gdk_quartz_drag_source_context != NULL);

  event = gdk_event_new (GDK_DROP_FINISHED);
  event->dnd.window = g_object_ref ([[self contentView] gdkSurface]);
  event->dnd.send_event = FALSE;
  event->dnd.context = g_object_ref (_gdk_quartz_drag_source_context);

  display = gdk_surface_get_display (event->dnd.window);

  if (display)
    {
      GList* windows, *list;
      gint gx, gy;

      event->dnd.context->dest_surface = NULL;

      windows = get_toplevels ();
      _gdk_quartz_surface_nspoint_to_gdk_xy (aPoint, &gx, &gy);

      for (list = windows; list; list = list->next)
        {
          GdkSurface* win = (GdkSurface*) list->data;
          gint wx, wy;
          gint ww, wh;

          gdk_surface_get_root_origin (win, &wx, &wy);
          ww = gdk_surface_get_width (win);
          wh = gdk_surface_get_height (win);

          if (gx > wx && gy > wy && gx <= wx + ww && gy <= wy + wh)
            event->dnd.context->dest_surface = win;
        }
    }

  device = gdk_drag_context_get_device (_gdk_quartz_drag_source_context);
  gdk_event_set_device (event, device);
  gdk_event_set_seat (event, gdk_device_get_seat (device));

  _gdk_event_emit (event);

  g_object_unref (event);

  g_object_unref (_gdk_quartz_drag_source_context);
  _gdk_quartz_drag_source_context = NULL;
}

#ifdef AVAILABLE_MAC_OS_X_VERSION_10_7_AND_LATER

- (void)setStyleMask:(NSUInteger)styleMask
{
  gboolean was_fullscreen;
  gboolean is_fullscreen;

  was_fullscreen = (([self styleMask] & NSFullScreenWindowMask) != 0);

  [super setStyleMask:styleMask];

  is_fullscreen = (([self styleMask] & NSFullScreenWindowMask) != 0);

  if (was_fullscreen != is_fullscreen)
    _gdk_quartz_surface_update_fullscreen_state ([[self contentView] gdkSurface]);
}

#endif

- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen *)screen
{
  NSRect rect;
  GdkSurface *window = [[self contentView] gdkSurface];
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  /* Allow the window to move up "shadow_top" more than normally allowed
   * by the default impl. This makes it possible to move windows with
   * client side shadow right up to the screen's menu bar. */
  rect = [super constrainFrameRect:frameRect toScreen:screen];
  if (frameRect.origin.y > rect.origin.y)
    rect.origin.y = MIN (frameRect.origin.y, rect.origin.y + impl->shadow_top);

  return rect;
}

- (NSRect)windowWillUseStandardFrame:(NSWindow *)nsWindow
                        defaultFrame:(NSRect)newFrame
{
  NSRect screenFrame = [[self screen] visibleFrame];
  GdkSurface *window = [[self contentView] gdkSurface];
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
  gboolean maximized = gdk_surface_get_state (window) & GDK_SURFACE_STATE_MAXIMIZED;

  if (!maximized)
    return screenFrame;
  else
    return lastUnmaximizedFrame;
}

- (BOOL)windowShouldZoom:(NSWindow *)nsWindow
                 toFrame:(NSRect)newFrame
{

  GdkSurface *window = [[self contentView] gdkSurface];
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);
  gboolean maximized = gdk_surface_get_state (window) & GDK_SURFACE_STATE_MAXIMIZED;

  if (maximized)
    {
      lastMaximizedFrame = newFrame;
      gdk_synthesize_surface_state (window,
                                   GDK_SURFACE_STATE_MAXIMIZED,
                                   0);
    }
  else
    {
      lastUnmaximizedFrame = [nsWindow frame];
      gdk_synthesize_surface_state (window,
                                   0,
                                   GDK_SURFACE_STATE_MAXIMIZED);
    }

  inMaximizeTransition = YES;
  return YES;
}

-(void)windowDidEndLiveResize:(NSNotification *)aNotification
{
  inMaximizeTransition = NO;
}

-(NSSize)window:(NSWindow *)window willUseFullScreenContentSize:(NSSize)proposedSize
{
  return [[window screen] frame].size;
}

-(void)windowWillEnterFullScreen:(NSNotification *)aNotification
{
  lastUnfullscreenFrame = [self frame];
}

-(void)windowWillExitFullScreen:(NSNotification *)aNotification
{
  [self setFrame:lastUnfullscreenFrame display:YES];
}

@end
