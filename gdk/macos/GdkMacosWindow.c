/* GdkMacosWindow.m
 *
 * Copyright © 2020 Red Hat, Inc.
 * Copyright © 2005-2007 Imendio AB
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
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gdk/gdk.h>

#import "GdkMacosBaseView.h"
#import "GdkMacosView.h"
#import "GdkMacosWindow.h"

#include "gdkmacosdisplay-private.h"
#include "gdkmacosdrag-private.h"
#include "gdkmacosdrop-private.h"
#include "gdkmacoseventsource-private.h"
#include "gdkmacosmonitor-private.h"
#include "gdkmacospasteboard-private.h"
#include "gdkmacossurface-private.h"
#include "gdkmacospopupsurface-private.h"
#include "gdkmacostoplevelsurface-private.h"
#include "gdkmacosutils-private.h"

#include "gdkeventsprivate.h"
#include "gdkmonitorprivate.h"
#include "gdksurfaceprivate.h"

@implementation GdkMacosWindow

static Class _contentViewClass = nil;

+(void)setContentViewClass:(Class)newViewClass
{
    GDK_DEBUG (MISC, "Setting new content view class to %s", [[newViewClass description] UTF8String]);

    if (newViewClass == nil  || [newViewClass isSubclassOfClass:[GdkMacosView class]])
      _contentViewClass = newViewClass;
    else
      g_critical ("Assigned content view class %s is not a subclass of GdkMacosView", [[newViewClass description] UTF8String]);
}

+(Class)contentViewClass
{
    if (_contentViewClass != nil)
      return _contentViewClass;

    return [GdkMacosView class];
}

-(BOOL)windowShouldClose:(id)sender
{
  GdkDisplay *display;
  GdkEvent *event;
  GList *node;

  display = gdk_surface_get_display (GDK_SURFACE (gdk_surface));
  event = gdk_delete_event_new (GDK_SURFACE (gdk_surface));
  node = _gdk_event_queue_append (display, event);
  _gdk_windowing_got_event (display, node, event,
                            _gdk_display_get_next_serial (display));

  return NO;
}

-(void)windowWillMiniaturize:(NSNotification *)aNotification
{
  if (GDK_IS_MACOS_TOPLEVEL_SURFACE (gdk_surface))
    _gdk_macos_toplevel_surface_detach_from_parent (GDK_MACOS_TOPLEVEL_SURFACE (gdk_surface));
  else if (GDK_IS_MACOS_POPUP_SURFACE (gdk_surface))
    _gdk_macos_popup_surface_detach_from_parent (GDK_MACOS_POPUP_SURFACE (gdk_surface));
}

-(void)windowDidMiniaturize:(NSNotification *)aNotification
{
  gdk_synthesize_surface_state (GDK_SURFACE (gdk_surface),
                                0,
                                GDK_TOPLEVEL_STATE_MINIMIZED |
                                GDK_TOPLEVEL_STATE_SUSPENDED);
}

-(void)windowDidDeminiaturize:(NSNotification *)aNotification
{
  if (GDK_IS_MACOS_TOPLEVEL_SURFACE (gdk_surface))
    _gdk_macos_toplevel_surface_attach_to_parent (GDK_MACOS_TOPLEVEL_SURFACE (gdk_surface));
  else if (GDK_IS_MACOS_POPUP_SURFACE (gdk_surface))
    _gdk_macos_popup_surface_attach_to_parent (GDK_MACOS_POPUP_SURFACE (gdk_surface));

  gdk_synthesize_surface_state (GDK_SURFACE (gdk_surface),
                                GDK_TOPLEVEL_STATE_MINIMIZED |
                                GDK_TOPLEVEL_STATE_SUSPENDED,
                                0);
}

-(void)windowDidBecomeKey:(NSNotification *)aNotification
{
  gdk_synthesize_surface_state (GDK_SURFACE (gdk_surface), 0, GDK_TOPLEVEL_STATE_FOCUSED);
  _gdk_macos_display_surface_became_key ([self gdkDisplay], gdk_surface);
}

-(void)windowDidResignKey:(NSNotification *)aNotification
{
  gdk_synthesize_surface_state (GDK_SURFACE (gdk_surface), GDK_TOPLEVEL_STATE_FOCUSED, 0);
  _gdk_macos_display_surface_resigned_key ([self gdkDisplay], gdk_surface);
}

-(void)windowDidBecomeMain:(NSNotification *)aNotification
{
  if (![self isVisible])
    {
      /* Note: This is a hack needed because for unknown reasons, hidden
       * windows get shown when clicking the dock icon when the application
       * is not already active.
       */
      [self orderOut:nil];
      return;
    }

  _gdk_macos_display_surface_became_main ([self gdkDisplay], gdk_surface);
}

-(void)windowDidResignMain:(NSNotification *)aNotification
{
  _gdk_macos_display_surface_resigned_main ([self gdkDisplay], gdk_surface);
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
  NSEventType event_type = [event type];

  switch ((int)event_type)
    {
    case NSEventTypeLeftMouseUp: {
      if (inManualMove || inManualResize || inMove)
        _gdk_macos_display_send_event ([self gdkDisplay], event);

      inManualMove = NO;
      inManualResize = NO;
      inMove = NO;

      /* Reset gravity */
      [[[self contentView] layer] setContentsGravity:kCAGravityBottomLeft];

      break;
    }

    case NSEventTypeLeftMouseDragged:
      if (inManualMove || [self trackManualResize])
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

- (BOOL)inFullscreenTransition;
{
  return inFullscreenTransition;
}

-(void)checkSendEnterNotify
{
  /* When a new window has been created, and the mouse is in the window
   * area, we will not receive an NSEventTypeMouseEntered event.
   * Therefore, we synthesize an enter notify event manually.
   */
  if (!initialPositionKnown)
    {
      initialPositionKnown = YES;

      if (NSPointInRect ([NSEvent mouseLocation], [self frame]))
        {
          GdkMacosBaseView *view = (GdkMacosBaseView *)[self contentView];
          NSEvent *event;

          event = [NSEvent enterExitEventWithType: NSEventTypeMouseEntered
                                         location: [self mouseLocationOutsideOfEventStream]
                                    modifierFlags: 0
                                        timestamp: [[NSApp currentEvent] timestamp]
                                     windowNumber: [self windowNumber]
                                          context: NULL
                                      eventNumber: 0
                                   trackingNumber: (NSInteger)[view trackingArea]
                                         userData: nil];

          [NSApp postEvent:event atStart:NO];
        }
    }
}

-(id)initWithContentRect:(NSRect)contentRect
               styleMask:(NSWindowStyleMask)styleMask
                 backing:(NSBackingStoreType)backingType
                   defer:(BOOL)flag
                  screen:(NSScreen *)screen
{
  GdkMacosView *view;

  self = [super initWithContentRect:contentRect
	                        styleMask:styleMask
	                          backing:backingType
	                            defer:flag
                             screen:screen];

  [self setAcceptsMouseMovedEvents:YES];
  [self setDelegate:(id<NSWindowDelegate>)self];
  [self setReleasedWhenClosed:YES];
  [self setPreservesContentDuringLiveResize:NO];

  view = [[[GdkMacosWindow contentViewClass] alloc] initWithFrame:contentRect];
  [self setContentView:view];
  [view release];

  /* TODO: We might want to make this more extensible at some point */
  _gdk_macos_pasteboard_register_drag_types (self);

  return self;
}

-(BOOL)canBecomeMainWindow
{
  return GDK_IS_TOPLEVEL (gdk_surface);
}

-(BOOL)canBecomeKeyWindow
{
  return GDK_IS_TOPLEVEL (gdk_surface);
}

-(void)showAndMakeKey:(BOOL)makeKey
{
  inShowOrHide = YES;

  if (makeKey && [self canBecomeKeyWindow])
    [self makeKeyAndOrderFront:self];
  else
    [self orderFront:self];

  if (makeKey && [self canBecomeMainWindow])
    [self makeMainWindow];

  inShowOrHide = NO;

  [self checkSendEnterNotify];
}

-(void)hide
{
  BOOL wasKey = [self isKeyWindow];
  BOOL wasMain = [self isMainWindow];

  inShowOrHide = YES;
  [self orderOut:nil];
  inShowOrHide = NO;

  initialPositionKnown = NO;

  if (wasMain)
    [self windowDidResignMain:nil];

  if (wasKey)
    [self windowDidResignKey:nil];
}

-(void)windowDidMove:(NSNotification *)notification
{
  if ([self isZoomed])
    gdk_synthesize_surface_state (GDK_SURFACE (gdk_surface), 0, GDK_TOPLEVEL_STATE_MAXIMIZED);
  else
    gdk_synthesize_surface_state (GDK_SURFACE (gdk_surface), GDK_TOPLEVEL_STATE_MAXIMIZED, 0);

  _gdk_macos_surface_configure ([self gdkSurface]);
}

-(void)windowDidResize:(NSNotification *)notification
{
  [self windowDidMove: notification];

  /* If we're using server-side decorations, this notification is coming
   * in from a display-side change. We need to request a layout in
   * addition to the configure event.
   */
  if (GDK_IS_MACOS_TOPLEVEL_SURFACE (gdk_surface))
    gdk_surface_request_layout (GDK_SURFACE (gdk_surface));
}

/* Used by gdkmacosdisplay-translate.c to decide if our sendEvent() handler
 * above will see the event or if it will be subjected to standard processing
 * by GDK.
*/
-(BOOL)isInManualResizeOrMove
{
  return inManualResize || inManualMove;
}

-(void)beginManualMove
{
  NSEvent *event;

  if (inMove || inManualMove || inManualResize)
    return;

  event = _gdk_macos_display_get_last_nsevent ();

  if (event == NULL)
    return; 
  
  inManualMove = YES;

  /* The docs state it has to be a button press event,
   * but it works just as well with motion events.
   */
  [self performWindowDragWithEvent: event];
}

-(BOOL)trackManualResize
{
  NSPoint mouse_location;
  NSRect new_frame;
  float mdx, mdy, dw, dh, dx, dy;
  NSSize min_size;

  if (!inManualResize || inTrackManualResize)
    return NO;

  inTrackManualResize = YES;

  mouse_location = convert_nspoint_to_screen (self, [self mouseLocationOutsideOfEventStream]);
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

  _gdk_macos_surface_user_resize ([self gdkSurface], new_frame);

  inTrackManualResize = NO;

  return YES;
}

-(void)beginManualResize:(GdkSurfaceEdge)edge
{
  CALayerContentsGravity gravity = kCAGravityBottomLeft;

  if (inMove || inManualMove || inManualResize)
    return;

  inManualResize = YES;
  resizeEdge = edge;

  switch (edge)
    {
    default:
    case GDK_SURFACE_EDGE_NORTH:
      gravity = kCAGravityTopLeft;
      break;

    case GDK_SURFACE_EDGE_NORTH_WEST:
      gravity = kCAGravityTopRight;
      break;

    case GDK_SURFACE_EDGE_SOUTH_WEST:
    case GDK_SURFACE_EDGE_WEST:
      gravity = kCAGravityBottomRight;
      break;

    case GDK_SURFACE_EDGE_SOUTH:
    case GDK_SURFACE_EDGE_SOUTH_EAST:
      gravity = kCAGravityBottomLeft;
      break;

    case GDK_SURFACE_EDGE_EAST:
      gravity = kCAGravityBottomLeft;
      break;

    case GDK_SURFACE_EDGE_NORTH_EAST:
      gravity = kCAGravityTopLeft;
      break;
    }

  [[[self contentView] layer] setContentsGravity:gravity];

  initialResizeFrame = [self frame];
  initialResizeLocation = convert_nspoint_to_screen (self, [self mouseLocationOutsideOfEventStream]);
}

// NSDraggingDestination protocol

-(NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
  NSPoint location = [sender draggingLocation];
  NSDragOperation ret;
  GdkMacosDrop *drop;

  if (!(drop = _gdk_macos_drop_new ([self gdkSurface], sender)))
    return NSDragOperationNone;

  _gdk_macos_display_set_drop ([self gdkDisplay],
                               [sender draggingSequenceNumber],
                               GDK_DROP (drop));

  gdk_drop_emit_enter_event (GDK_DROP (drop),
                             TRUE,
                             location.x,
                             GDK_SURFACE (gdk_surface)->height - location.y,
                             GDK_CURRENT_TIME);

  ret = _gdk_macos_drop_operation (drop);

  g_object_unref (drop);

  return ret;
}

-(void)draggingEnded:(id <NSDraggingInfo>)sender
{
  _gdk_macos_display_set_drop ([self gdkDisplay], [sender draggingSequenceNumber], NULL);
}

-(void)draggingExited:(id <NSDraggingInfo>)sender
{
  NSInteger sequence_number = [sender draggingSequenceNumber];
  GdkDrop *drop = _gdk_macos_display_find_drop ([self gdkDisplay], sequence_number);

  if (drop != NULL)
    gdk_drop_emit_leave_event (drop, TRUE, GDK_CURRENT_TIME);

  _gdk_macos_display_set_drop ([self gdkDisplay], sequence_number, NULL);
}

-(NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender
{
  NSInteger sequence_number = [sender draggingSequenceNumber];
  GdkDisplay *display = gdk_surface_get_display (GDK_SURFACE (gdk_surface));
  GdkDrop *drop = _gdk_macos_display_find_drop (GDK_MACOS_DISPLAY (display), sequence_number);
  NSPoint location = [sender draggingLocation];

  if (drop == NULL)
    return NSDragOperationNone;

  _gdk_macos_drop_update_actions (GDK_MACOS_DROP (drop), sender);

  gdk_drop_emit_motion_event (drop,
                              TRUE,
                              location.x,
                              GDK_SURFACE (gdk_surface)->height - location.y,
                              GDK_CURRENT_TIME);

  return _gdk_macos_drop_operation (GDK_MACOS_DROP (drop));
}

-(BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
  NSInteger sequence_number = [sender draggingSequenceNumber];
  GdkDisplay *display = gdk_surface_get_display (GDK_SURFACE (gdk_surface));
  GdkDrop *drop = _gdk_macos_display_find_drop (GDK_MACOS_DISPLAY (display), sequence_number);
  NSPoint location = [sender draggingLocation];

  if (drop == NULL)
    return NO;

  gdk_drop_emit_drop_event (drop,
                            TRUE,
                            location.x,
                            GDK_SURFACE (gdk_surface)->height - location.y,
                            GDK_CURRENT_TIME);

  gdk_drop_emit_leave_event (drop, TRUE, GDK_CURRENT_TIME);

  return GDK_MACOS_DROP (drop)->finish_action != 0;
}

-(BOOL)wantsPeriodicDraggingUpdates
{
  return NO;
}

// NSDraggingSource protocol

- (NSDragOperation)draggingSession:(NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context
{
  NSInteger sequence_number = [session draggingSequenceNumber];
  GdkDisplay *display = gdk_surface_get_display (GDK_SURFACE (gdk_surface));
  GdkDrag *drag = _gdk_macos_display_find_drag (GDK_MACOS_DISPLAY (display), sequence_number);
  GdkModifierType state = _gdk_macos_display_get_current_keyboard_modifiers (GDK_MACOS_DISPLAY (display));

  _gdk_macos_drag_set_actions (GDK_MACOS_DRAG (drag), state);

  return _gdk_macos_drag_operation (GDK_MACOS_DRAG (drag));
}

- (void)draggingSession:(NSDraggingSession *)session willBeginAtPoint:(NSPoint)screenPoint
{
  NSInteger sequence_number = [session draggingSequenceNumber];
  GdkDisplay *display = gdk_surface_get_display (GDK_SURFACE (gdk_surface));
  GdkDrag *drag = _gdk_macos_display_find_drag (GDK_MACOS_DISPLAY (display), sequence_number);
  int x, y;

  _gdk_macos_display_from_display_coords (GDK_MACOS_DISPLAY (display), screenPoint.x, screenPoint.y, &x, &y);
  _gdk_macos_drag_set_start_position (GDK_MACOS_DRAG (drag), x, y);
  _gdk_macos_drag_surface_move (GDK_MACOS_DRAG (drag), x, y);
}

- (void)draggingSession:(NSDraggingSession *)session movedToPoint:(NSPoint)screenPoint
{
  NSInteger sequence_number = [session draggingSequenceNumber];
  GdkMacosDisplay *display = GDK_MACOS_DISPLAY (gdk_surface_get_display (GDK_SURFACE (gdk_surface)));
  GdkDrag *drag = _gdk_macos_display_find_drag (GDK_MACOS_DISPLAY (display), sequence_number);
  int x, y;

  _gdk_macos_display_send_event (display, [NSApp currentEvent]);

  _gdk_macos_display_from_display_coords (display, screenPoint.x, screenPoint.y, &x, &y);
  _gdk_macos_drag_surface_move (GDK_MACOS_DRAG (drag), x, y);
}

- (void)draggingSession:(NSDraggingSession *)session endedAtPoint:(NSPoint)screenPoint operation:(NSDragOperation)operation
{
  NSInteger sequence_number = [session draggingSequenceNumber];
  GdkMacosDisplay *display = GDK_MACOS_DISPLAY (gdk_surface_get_display (GDK_SURFACE (gdk_surface)));
  GdkDrag *drag = _gdk_macos_display_find_drag (display, sequence_number);

  _gdk_macos_display_send_event (display, [NSApp currentEvent]);
  gdk_drag_set_selected_action (drag, _gdk_macos_drag_ns_operation_to_action (operation));

  if (gdk_drag_get_selected_action (drag) != 0)
    g_signal_emit_by_name (drag, "drop-performed");
  else
    gdk_drag_cancel (drag, GDK_DRAG_CANCEL_NO_TARGET);

  _gdk_macos_display_set_drag (display, [session draggingSequenceNumber], NULL);
}

// end

-(void)setStyleMask:(NSWindowStyleMask)styleMask
{
  [super setStyleMask:styleMask];

  _gdk_macos_surface_update_fullscreen_state (gdk_surface);
}

-(NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen *)screen
{
  NSRect rect;

  rect = [super constrainFrameRect:frameRect toScreen:screen];
  if (frameRect.origin.y > rect.origin.y)
    rect.origin.y = MIN (frameRect.origin.y, rect.origin.y);

  return rect;
}

/* Implementing this method avoids new windows move around the screen. */
-(NSRect)windowWillUseStandardFrame:(NSWindow *)nsWindow
                       defaultFrame:(NSRect)newFrame
{
  return newFrame;
}

-(NSSize)window:(NSWindow *)window willUseFullScreenContentSize:(NSSize)proposedSize
{
  return [[window screen] frame].size;
}

-(void)windowWillEnterFullScreen:(NSNotification *)aNotification
{
  inFullscreenTransition = YES;
}

-(void)windowDidEnterFullScreen:(NSNotification *)aNotification
{
  inFullscreenTransition = NO;
  initialPositionKnown = NO;

  [self updateToolbarAppearence];

  [self checkSendEnterNotify];
}

-(void)windowWillExitFullScreen:(NSNotification *)aNotification
{
  inFullscreenTransition = YES;
}

-(void)windowDidExitFullScreen:(NSNotification *)aNotification
{
  inFullscreenTransition = NO;
  initialPositionKnown = NO;

  [self updateToolbarAppearence];

  [self checkSendEnterNotify];
}

-(void)windowDidFailToEnterFullScreen:(NSNotification *)aNotification
{
  inFullscreenTransition = NO;
}

-(void)windowDidFailToExitFullScreen:(NSNotification *)aNotification
{
  inFullscreenTransition = NO;
}

-(void)windowDidChangeScreen:(NSNotification *)aNotification
{
  _gdk_macos_surface_monitor_changed (gdk_surface);
}

/* We need this for the Vulkan renderer to update it's content scaling.
 */
-(void)windowDidChangeBackingProperties:(NSNotification *) notification
{
  _gdk_surface_update_size (GDK_SURFACE (gdk_surface));
}

-(void)setGdkSurface:(GdkMacosSurface *)surface
{
  self->gdk_surface = surface;
}

-(void)setDecorated:(BOOL)decorated
{
  NSWindowStyleMask style_mask = [self styleMask];

  if (decorated)
    {
      style_mask &= ~NSWindowStyleMaskFullSizeContentView;
    }
  else
    {
      style_mask |= NSWindowStyleMaskFullSizeContentView;
    }

  [self setStyleMask:style_mask];

  [self updateToolbarAppearence];
}

-(BOOL)showStandardWindowButtons;
{
  return showButtonCount > 0;
}

-(void)setShowStandardWindowButtons:(BOOL)show
{
  if (show)
    showButtonCount++;
  else if (showButtonCount > 0)
    showButtonCount--;
  else
    {
      g_warning ("Show standard window button count doesn't match hide count");
      return;
    }

  [self updateToolbarAppearence];
}

/* updateToolbarAppearence:
 * Update the toolbar appearence based on the following criteria:
 *
 * 1. The window is used Client Side Decorations (style mask is set)
 * 2. The window has native window buttons enabled
 * 3. The window is in fullscreen mode
 */
-(void)updateToolbarAppearence
{
  NSWindowStyleMask style_mask = [self styleMask];
  BOOL is_fullscreen = (style_mask & NSWindowStyleMaskFullScreen) != 0;
  BOOL is_csd = !is_fullscreen && (style_mask & NSWindowStyleMaskFullSizeContentView) != 0;
  BOOL hidden = is_csd && (showButtonCount == 0);

  /* Do not update toolbars when in fullscreen transition, as it can cause the app to crash.
   * It will be updated once the transition has finished.
   */
  if (inFullscreenTransition)
    return;

  /* By assigning a toolbar, the window controls are moved a bit more inwards,
   * In line with how toolbars look in macOS apps.
   * I haven't found a better way. Unfortunately we have to be careful not to
   * update the toolbar during a fullscreen transition.
   */
  if (is_csd && (showButtonCount > 0) && [self toolbar] == nil)
    {
      NSToolbar *toolbar = [[NSToolbar alloc] init];
      [self setToolbar:toolbar];
      [toolbar release];
    }
  else if (!is_csd && [self toolbar] != nil)
    [self setToolbar:nil];

  [self setTitleVisibility:is_csd ? NSWindowTitleHidden : NSWindowTitleVisible];
  [self setTitlebarAppearsTransparent:is_csd];

  [[self standardWindowButton:NSWindowCloseButton] setHidden:hidden];
  [[self standardWindowButton:NSWindowMiniaturizeButton] setHidden:hidden];
  [[self standardWindowButton:NSWindowZoomButton] setHidden:hidden];
}

-(GdkMacosSurface *)gdkSurface
{
  return self->gdk_surface;
}

-(GdkMacosDisplay *)gdkDisplay
{
  return GDK_MACOS_DISPLAY (GDK_SURFACE (self->gdk_surface)->display);
}

-(BOOL)movableByWindowBackground
{
  return NO;
}

-(void)swapBuffer:(GdkMacosBuffer *)buffer withDamage:(const cairo_region_t *)damage
{
  [(GdkMacosView *)[self contentView] swapBuffer:buffer withDamage:damage];
}

-(BOOL)needsMouseDownQuirk
{
  return GDK_IS_MACOS_TOPLEVEL_SURFACE (gdk_surface) &&
    !GDK_MACOS_TOPLEVEL_SURFACE (gdk_surface)->decorated;
}

@end
