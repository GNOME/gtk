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

  _gdk_quartz_update_focus_window (window, TRUE);
}

-(void)windowDidResignKey:(NSNotification *)aNotification
{
  GdkWindow *window = [[self contentView] gdkWindow];

  _gdk_quartz_update_focus_window (window, FALSE);
}

-(void)windowDidMove:(NSNotification *)aNotification
{
  NSRect content_rect = [self contentRectForFrameRect:[self frame]];
  GdkWindow *window = [[self contentView] gdkWindow];
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  GdkEvent *event;

  private->x = content_rect.origin.x;
  private->y = _gdk_quartz_get_inverted_screen_y (content_rect.origin.y) - impl->height;

  /* Synthesize a configure event */
  event = gdk_event_new (GDK_CONFIGURE);
  event->configure.window = g_object_ref (window);
  event->configure.x = private->x;
  event->configure.y = private->y;
  event->configure.width = impl->width;
  event->configure.height = impl->height;

  _gdk_event_queue_append (gdk_display_get_default (), event);
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

-(BOOL)canBecomeMainWindow
{
  GdkWindow *window = [[self contentView] gdkWindow];
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  switch (impl->type_hint)
    {
    case GDK_WINDOW_TYPE_HINT_NORMAL:
    case GDK_WINDOW_TYPE_HINT_DIALOG:
      return YES;
      
    case GDK_WINDOW_TYPE_HINT_MENU:
    case GDK_WINDOW_TYPE_HINT_TOOLBAR:
    case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
    case GDK_WINDOW_TYPE_HINT_UTILITY:
    case GDK_WINDOW_TYPE_HINT_DOCK:
    case GDK_WINDOW_TYPE_HINT_DESKTOP:
    case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU:
    case GDK_WINDOW_TYPE_HINT_POPUP_MENU:
    case GDK_WINDOW_TYPE_HINT_TOOLTIP:
    case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
    case GDK_WINDOW_TYPE_HINT_COMBO:
    case GDK_WINDOW_TYPE_HINT_DND:
      return NO;
    }
  
  return YES;
}

-(BOOL)canBecomeKeyWindow
{
  GdkWindow *window = [[self contentView] gdkWindow];
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  /* FIXME: Is this right? If so, the switch shouldn't be needed. Need
   * this + some tweaking to the event/grab code to get menus
   * working...
   */
  /*if (private->window_type == GDK_WINDOW_TEMP)
    return NO;
  */

  switch (impl->type_hint)
    {
    case GDK_WINDOW_TYPE_HINT_NORMAL:
    case GDK_WINDOW_TYPE_HINT_DIALOG:
    case GDK_WINDOW_TYPE_HINT_MENU:
    case GDK_WINDOW_TYPE_HINT_TOOLBAR:
    case GDK_WINDOW_TYPE_HINT_UTILITY:
    case GDK_WINDOW_TYPE_HINT_DOCK:
    case GDK_WINDOW_TYPE_HINT_DESKTOP:
    case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU:
    case GDK_WINDOW_TYPE_HINT_POPUP_MENU:
    case GDK_WINDOW_TYPE_HINT_COMBO:
      return YES;
      
    case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
    case GDK_WINDOW_TYPE_HINT_TOOLTIP:
    case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
    case GDK_WINDOW_TYPE_HINT_DND:
      return NO;
    }
  
  return YES;
}

static GdkDragContext *current_context = NULL;

static GdkDragAction
drag_operation_to_drag_action (NSDragOperation operation)
{
  GdkDragAction result = 0;

  if (operation & NSDragOperationGeneric)
    result |= GDK_ACTION_COPY;

  return result;
}

static NSDragOperation
drag_action_to_drag_operation (GdkDragAction action)
{
  NSDragOperation result = 0;

  if (action & GDK_ACTION_COPY)
    result |= NSDragOperationCopy;

  return result;
}

static void
update_context_from_dragging_info (id <NSDraggingInfo> sender)
{
  g_assert (current_context != NULL);

  GDK_DRAG_CONTEXT_PRIVATE (current_context)->dragging_info = sender;
  current_context->suggested_action = drag_operation_to_drag_action ([sender draggingSourceOperationMask]);
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
  GdkEvent event;

  if (current_context)
    g_object_unref (current_context);
  
  current_context = gdk_drag_context_new ();
  update_context_from_dragging_info (sender);

  event.dnd.type = GDK_DRAG_ENTER;
  event.dnd.window = g_object_ref ([[self contentView] gdkWindow]);
  event.dnd.send_event = FALSE;
  event.dnd.context = current_context;
  event.dnd.time = GDK_CURRENT_TIME;

  (*_gdk_event_func) (&event, _gdk_event_data);

  return NSDragOperationNone;
}

- (void)draggingEnded:(id <NSDraggingInfo>)sender
{
  g_object_unref (current_context);
  current_context = NULL;
}

- (void)draggingExited:(id <NSDraggingInfo>)sender
{
  GdkEvent event;
  
  event.dnd.type = GDK_DRAG_LEAVE;
  event.dnd.window = g_object_ref ([[self contentView] gdkWindow]);
  event.dnd.send_event = FALSE;
  event.dnd.context = current_context;
  event.dnd.time = GDK_CURRENT_TIME;

  (*_gdk_event_func) (&event, _gdk_event_data);
  
  g_object_unref (current_context);
  current_context = NULL;
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender
{
  NSPoint point = [sender draggingLocation];
  NSPoint screen_point = [self convertBaseToScreen:point];
  GdkEvent event;

  update_context_from_dragging_info (sender);

  event.dnd.type = GDK_DRAG_MOTION;
  event.dnd.window = g_object_ref ([[self contentView] gdkWindow]);
  event.dnd.send_event = FALSE;
  event.dnd.context = current_context;
  event.dnd.time = GDK_CURRENT_TIME;
  event.dnd.x_root = screen_point.x;
  event.dnd.y_root = _gdk_quartz_get_inverted_screen_y (screen_point.y);

  (*_gdk_event_func) (&event, _gdk_event_data);

  g_object_unref (event.dnd.window);

  return drag_action_to_drag_operation (current_context->action);
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
  NSPoint point = [sender draggingLocation];
  NSPoint screen_point = [self convertBaseToScreen:point];
  GdkEvent event;

  update_context_from_dragging_info (sender);

  event.dnd.type = GDK_DROP_START;
  event.dnd.window = g_object_ref ([[self contentView] gdkWindow]);
  event.dnd.send_event = FALSE;
  event.dnd.context = current_context;
  event.dnd.time = GDK_CURRENT_TIME;
  event.dnd.x_root = screen_point.x;
  event.dnd.y_root = _gdk_quartz_get_inverted_screen_y (screen_point.y);

  (*_gdk_event_func) (&event, _gdk_event_data);

  g_object_unref (event.dnd.window);

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
  GdkEvent event;

  g_assert (_gdk_quartz_drag_source_context != NULL);

  event.dnd.type = GDK_DROP_FINISHED;
  event.dnd.window = g_object_ref ([[self contentView] gdkWindow]);
  event.dnd.send_event = FALSE;
  event.dnd.context = _gdk_quartz_drag_source_context;

  (*_gdk_event_func) (&event, _gdk_event_data);

  g_object_unref (event.dnd.window);

  g_object_unref (_gdk_quartz_drag_source_context);
  _gdk_quartz_drag_source_context = NULL;
}

@end
