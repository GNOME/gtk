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
#import "GdkMacosCairoView.h"
#import "GdkMacosWindow.h"

#include "gdkmacosdisplay-private.h"
#include "gdkmacossurface-private.h"
#include "gdkmacostoplevelsurface-private.h"

#include "gdksurfaceprivate.h"

@implementation GdkMacosWindow

-(void)invalidateStacking
{
  if (gdkSurface != NULL)
    {
      GdkDisplay *display = gdk_surface_get_display (GDK_SURFACE (gdkSurface));

      if (GDK_IS_MACOS_DISPLAY (display))
        _gdk_macos_display_stacking_changed (GDK_MACOS_DISPLAY (display));
    }
}

-(void)windowWillClose:(NSNotification*)notification
{
  /* Clears the delegate when window is going to be closed; since EL
   * Capitan it is possible that the methods of delegate would get called
   * after the window has been closed.
   */
  [self setDelegate:nil];
}

-(BOOL)windowShouldClose:(id)sender
{
  GdkDisplay *display;
  GdkEvent *event;

  display = gdk_surface_get_display (GDK_SURFACE (self->gdkSurface));
  event = gdk_delete_event_new (GDK_SURFACE (self->gdkSurface));

  _gdk_event_queue_append (display, event);

  return NO;
}

-(void)windowWillMiniaturize:(NSNotification *)aNotification
{
  if (GDK_IS_MACOS_TOPLEVEL_SURFACE (gdkSurface))
    _gdk_macos_toplevel_surface_detach_from_parent (GDK_MACOS_TOPLEVEL_SURFACE (gdkSurface));
}

-(void)windowDidMiniaturize:(NSNotification *)aNotification
{
  gdk_synthesize_surface_state (GDK_SURFACE (gdkSurface), 0, GDK_SURFACE_STATE_MINIMIZED);

  [self invalidateStacking];
}

-(void)windowDidDeminiaturize:(NSNotification *)aNotification
{
  if (GDK_IS_MACOS_TOPLEVEL_SURFACE (gdkSurface))
    _gdk_macos_toplevel_surface_attach_to_parent (GDK_MACOS_TOPLEVEL_SURFACE (gdkSurface));

  gdk_synthesize_surface_state (GDK_SURFACE (gdkSurface), GDK_SURFACE_STATE_MINIMIZED, 0);

  [self invalidateStacking];
}

-(void)windowDidBecomeKey:(NSNotification *)aNotification
{
  _gdk_macos_surface_set_is_key (gdkSurface, TRUE);
}

-(void)windowDidResignKey:(NSNotification *)aNotification
{
  _gdk_macos_surface_set_is_key (gdkSurface, FALSE);
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

  [self invalidateStacking];
}

-(void)windowDidResignMain:(NSNotification *)aNotification
{
  [self invalidateStacking];
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
  switch ((int)[event type])
    {
    case NSEventTypeLeftMouseUp:
    {
      GdkDisplay *display = gdk_surface_get_display (GDK_SURFACE (gdkSurface));
      double time = ((double)[event timestamp]) * 1000.0;

      _gdk_macos_display_break_all_grabs (GDK_MACOS_DISPLAY (display), time);

      inManualMove = NO;
      inManualResize = NO;
      inMove = NO;

      break;
    }

    case NSEventTypeLeftMouseDragged:
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
                                   trackingNumber: [view trackingRect]
                                         userData: nil];

          [NSApp postEvent:event atStart:NO];
        }
    }
}

-(void)windowDidMove:(NSNotification *)aNotification
{
  GdkSurface *surface = GDK_SURFACE (gdkSurface);
  GdkDisplay *display = gdk_surface_get_display (surface);
  gboolean maximized = !!(surface->state & GDK_SURFACE_STATE_MAXIMIZED);
  GdkEvent *event;

  /* In case the window is changed when maximized remove the maximized state */
  if (maximized && !inMaximizeTransition && !NSEqualRects (lastMaximizedFrame, [self frame]))
    gdk_synthesize_surface_state (surface, GDK_SURFACE_STATE_MAXIMIZED, 0);

  _gdk_macos_surface_update_position (gdkSurface);

  event = gdk_configure_event_new (surface, surface->width, surface->height);
  _gdk_event_queue_append (GDK_DISPLAY (display), event);

  [self checkSendEnterNotify];
}

-(void)windowDidResize:(NSNotification *)aNotification
{
  NSRect content_rect;
  GdkSurface *surface;
  GdkDisplay *display;
  GdkEvent *event;
  gboolean maximized;

  surface = GDK_SURFACE (self->gdkSurface);
  display = gdk_surface_get_display (surface);

  content_rect = [self contentRectForFrameRect:[self frame]];
  maximized = (surface->state & GDK_SURFACE_STATE_MAXIMIZED) != 0;

  /* see same in windowDidMove */
  if (maximized && !inMaximizeTransition && !NSEqualRects (lastMaximizedFrame, [self frame]))
    gdk_synthesize_surface_state (surface, GDK_SURFACE_STATE_MAXIMIZED, 0);

  surface->width = content_rect.size.width;
  surface->height = content_rect.size.height;

  /* Certain resize operations (e.g. going fullscreen), also move the
   * origin of the window.
   */
  _gdk_macos_surface_update_position (GDK_MACOS_SURFACE (surface));

  [[self contentView] setFrame:NSMakeRect (0, 0, surface->width, surface->height)];

  _gdk_surface_update_size (surface);

  /* Synthesize a configure event */
  event = gdk_configure_event_new (surface,
                                   content_rect.size.width,
                                   content_rect.size.height);
  _gdk_event_queue_append (display, event);

  [self checkSendEnterNotify];
}

-(id)initWithContentRect:(NSRect)contentRect
               styleMask:(NSWindowStyleMask)styleMask
                 backing:(NSBackingStoreType)backingType
                   defer:(BOOL)flag
                  screen:(NSScreen *)screen
{
  GdkMacosCairoView *view;

  self = [super initWithContentRect:contentRect
	                  styleMask:styleMask
	                    backing:backingType
	                      defer:flag
                       screen:screen];

  [self setAcceptsMouseMovedEvents:YES];
  [self setDelegate:(id<NSWindowDelegate>)self];
  [self setReleasedWhenClosed:YES];

  view = [[GdkMacosCairoView alloc] initWithFrame:contentRect];
  [self setContentView:view];
  [view release];

  return self;
}

-(BOOL)canBecomeMainWindow
{
  return GDK_IS_TOPLEVEL (self->gdkSurface);
}

-(BOOL)canBecomeKeyWindow
{
  return GDK_IS_TOPLEVEL (gdkSurface);
}

-(void)showAndMakeKey:(BOOL)makeKey
{
  inShowOrHide = YES;

  if (makeKey)
    [self makeKeyAndOrderFront:nil];
  else
    [self orderFront:nil];

  inShowOrHide = NO;

  [self checkSendEnterNotify];

  [(GdkMacosBaseView *)[self contentView] updateTrackingRect];
}

-(void)hide
{
  inShowOrHide = YES;
  [self orderOut:nil];
  inShowOrHide = NO;

  initialPositionKnown = NO;

  [self invalidateStacking];
}

-(BOOL)trackManualMove
{
  NSRect screenFrame = [[NSScreen mainScreen] visibleFrame];
  NSRect windowFrame = [self frame];
  NSPoint currentLocation;
  NSPoint newOrigin;
  int shadow_top = 0;

  if (!inManualMove)
    return NO;

  currentLocation = [self convertPointToScreen:[self mouseLocationOutsideOfEventStream]];
  newOrigin.x = currentLocation.x - initialMoveLocation.x;
  newOrigin.y = currentLocation.y - initialMoveLocation.y;

  _gdk_macos_surface_get_shadow (gdkSurface, &shadow_top, NULL, NULL, NULL);

  /* Clamp vertical position to below the menu bar. */
  if (newOrigin.y + windowFrame.size.height - shadow_top > screenFrame.origin.y + screenFrame.size.height)
    newOrigin.y = screenFrame.origin.y + screenFrame.size.height - windowFrame.size.height + shadow_top;

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

  initialMoveLocation = [self convertPointToScreen:[self mouseLocationOutsideOfEventStream]];
  initialMoveLocation.x -= frame.origin.x;
  initialMoveLocation.y -= frame.origin.y;
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

  mouse_location = [self convertPointToScreen:[self mouseLocationOutsideOfEventStream]];
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
  initialResizeLocation = [self convertPointToScreen:[self mouseLocationOutsideOfEventStream]];
}

-(NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
  return NSDragOperationNone;
}

-(void)draggingEnded:(id <NSDraggingInfo>)sender
{
}

-(void)draggingExited:(id <NSDraggingInfo>)sender
{
}

-(NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender
{
  return NSDragOperationNone;
}

-(BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
  return YES;
}

-(BOOL)wantsPeriodicDraggingUpdates
{
  return NO;
}

-(void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
}

-(void)setStyleMask:(NSWindowStyleMask)styleMask
{
  gboolean was_fullscreen;
  gboolean is_fullscreen;
  gboolean was_opaque;
  gboolean is_opaque;

  was_fullscreen = (([self styleMask] & NSWindowStyleMaskFullScreen) != 0);
  was_opaque = (([self styleMask] & NSWindowStyleMaskTitled) != 0);

  [super setStyleMask:styleMask];

  is_fullscreen = (([self styleMask] & NSWindowStyleMaskFullScreen) != 0);
  is_opaque = (([self styleMask] & NSWindowStyleMaskTitled) != 0);

  if (was_fullscreen != is_fullscreen)
    _gdk_macos_surface_update_fullscreen_state (self->gdkSurface);

  if (was_opaque != is_opaque)
    {
      [self setOpaque:is_opaque];

      if (!is_opaque)
        [self setBackgroundColor:[NSColor clearColor]];
    }
}

-(NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen *)screen
{
  GdkMacosSurface *surface = self->gdkSurface;
  NSRect rect;
  gint shadow_top;

  /* Allow the window to move up "shadow_top" more than normally allowed
   * by the default impl. This makes it possible to move windows with
   * client side shadow right up to the screen's menu bar. */
  _gdk_macos_surface_get_shadow (surface, &shadow_top, NULL, NULL, NULL);
  rect = [super constrainFrameRect:frameRect toScreen:screen];
  if (frameRect.origin.y > rect.origin.y)
    rect.origin.y = MIN (frameRect.origin.y, rect.origin.y + shadow_top);

  return rect;
}

-(NSRect)windowWillUseStandardFrame:(NSWindow *)nsWindow
                       defaultFrame:(NSRect)newFrame
{
  NSRect screenFrame = [[self screen] visibleFrame];
  GdkMacosSurface *surface = self->gdkSurface;
  gboolean maximized = GDK_SURFACE (surface)->state & GDK_SURFACE_STATE_MAXIMIZED;

  if (!maximized)
    return screenFrame;
  else
    return lastUnmaximizedFrame;
}

-(BOOL)windowShouldZoom:(NSWindow *)nsWindow
                toFrame:(NSRect)newFrame
{
  GdkMacosSurface *surface = self->gdkSurface;
  GdkSurfaceState state = GDK_SURFACE (surface)->state;

  if (state & GDK_SURFACE_STATE_MAXIMIZED)
    {
      lastMaximizedFrame = newFrame;
      gdk_surface_set_state (GDK_SURFACE (surface), state & ~GDK_SURFACE_STATE_MAXIMIZED);
    }
  else
    {
      lastUnmaximizedFrame = [nsWindow frame];
      gdk_surface_set_state (GDK_SURFACE (surface), state & GDK_SURFACE_STATE_MAXIMIZED);
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

-(void)windowDidEnterFullScreen:(NSNotification *)aNotification
{
  [self invalidateStacking];
}

-(void)windowWillExitFullScreen:(NSNotification *)aNotification
{
  [self setFrame:lastUnfullscreenFrame display:YES];
}

-(void)windowDidExitFullScreen:(NSNotification *)aNotification
{
  [self invalidateStacking];
}

-(void)setGdkSurface:(GdkMacosSurface *)surface
{
  self->gdkSurface = surface;
}

-(void)setDecorated:(BOOL)decorated
{
  NSWindowStyleMask style_mask = [self styleMask];

  [self setHasShadow:decorated];

  if (decorated)
    style_mask |= NSWindowStyleMaskTitled;
  else
    style_mask &= ~NSWindowStyleMaskTitled;

  [self setStyleMask:style_mask];
}

-(GdkMacosSurface *)getGdkSurface
{
  return self->gdkSurface;
}

@end
