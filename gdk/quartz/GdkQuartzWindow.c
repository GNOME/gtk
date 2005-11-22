/* GdkQuartzWindow.m
 *
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


#import "GdkQuartzWindow.h"
#include "gdkwindow-quartz.h"
#include "gdkprivate-quartz.h"

@implementation GdkQuartzWindow

-(BOOL)windowShouldClose:(id)sender
{
  GdkWindow *window = [[self contentView] gdkWindow];
  GdkEvent *event;

  event = gdk_event_new (GDK_DELETE);

  event->any.window = g_object_ref (window);
  event->any.send_event = FALSE;

  _gdk_event_queue_append (gdk_display_get_default (), event);

  return NO;
}

-(void)windowDidMiniaturize:(NSNotification *)aNotification
{
  GdkWindow *window = [[self contentView] gdkWindow];

  gdk_synthesize_window_state (window, 0, 
			       GDK_WINDOW_STATE_ICONIFIED);
}

-(void)windowDidDeminiaturize:(NSNotification *)aNotification
{
  GdkWindow *window = [[self contentView] gdkWindow];

  gdk_synthesize_window_state (window, GDK_WINDOW_STATE_ICONIFIED, 0);
}

-(void)windowDidBecomeKey:(NSNotification *)aNotification
{
  GdkWindow *window = [[self contentView] gdkWindow];

  _gdk_quartz_update_focus_window (window);
}

-(void)windowDidResize:(NSNotification *)aNotification
{
  NSRect content_rect = [self contentRectForFrameRect:[self frame]];
  GdkWindow *window = [[self contentView] gdkWindow];
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  GdkEvent *event;

  impl->width = content_rect.size.width;
  impl->height = content_rect.size.height;

  /* Synthesize a configure event */

  event = gdk_event_new (GDK_CONFIGURE);
  event->configure.window = g_object_ref (window);
  event->configure.x = private->x;
  event->configure.y = private->y;
  event->configure.width = impl->width;
  event->configure.height = impl->height;

  _gdk_event_queue_append (gdk_display_get_default (), event);

  /* Update tracking rectangle */
  [[self contentView] removeTrackingRect:impl->tracking_rect];
  impl->tracking_rect = [impl->view addTrackingRect:NSMakeRect(0, 0, impl->width, impl->height) 
			                      owner:impl->view
			                   userData:nil
			               assumeInside:NO];
}

-(id)initWithContentRect:(NSRect)contentRect styleMask:(unsigned int)styleMask backing:(NSBackingStoreType)backingType defer:(BOOL)flag
{
  self = [super initWithContentRect:contentRect
	                  styleMask:styleMask
	                    backing:backingType
	                      defer:flag];


  /* A possible modification here would be to only accept mouse moved events
   * if any of the child GdkWindows are interested in mouse moved events.
   */
  [self setAcceptsMouseMovedEvents:YES];

  [self setDelegate:self];
  [self setReleasedWhenClosed:YES];

  return self;
}

@end
