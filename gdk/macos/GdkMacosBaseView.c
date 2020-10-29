/* GdkMacosBaseView.c
 *
 * Copyright 2005-2007 Imendio AB
 * Copyright 2011 Hiroyuki Yamamoto
 * Copyright 2020 Red Hat, Inc.
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

#import "GdkMacosBaseView.h"
#import "GdkMacosWindow.h"

#include "gdkinternals.h"

#include "gdkmacosdisplay-private.h"
#include "gdkmacossurface-private.h"

@implementation GdkMacosBaseView

-(id)initWithFrame:(NSRect)frameRect
{
  if ((self = [super initWithFrame: frameRect]))
    {
      NSRect rect = NSMakeRect (0, 0, 0, 0);
      NSTrackingAreaOptions options;

      markedRange = NSMakeRange (NSNotFound, 0);
      selectedRange = NSMakeRange (0, 0);
      [self setValue: @(YES) forKey: @"postsFrameChangedNotifications"];

      options = (NSTrackingMouseEnteredAndExited |
                 NSTrackingMouseMoved |
                 NSTrackingInVisibleRect |
                 NSTrackingActiveAlways);
      trackingArea = [[NSTrackingArea alloc] initWithRect:rect
                                                  options:options
                                                    owner:(id)self
                                                 userInfo:nil];
      [self addTrackingArea:trackingArea];
    }

  return self;
}

-(void)setOpaqueRegion:(cairo_region_t *)region
{
  /* Do nothing */
}

-(BOOL)acceptsFirstMouse
{
  return YES;
}

-(BOOL)mouseDownCanMoveWindow
{
  return NO;
}

-(BOOL)acceptsFirstResponder
{
  GDK_NOTE (EVENTS, g_message ("acceptsFirstResponder"));
  return YES;
}

-(BOOL)becomeFirstResponder
{
  GDK_NOTE (EVENTS, g_message ("becomeFirstResponder"));
  return YES;
}

-(BOOL)resignFirstResponder
{
  GDK_NOTE (EVENTS, g_message ("resignFirstResponder"));
  return YES;
}

-(void)setNeedsInvalidateShadow: (BOOL)invalidate
{
  needsInvalidateShadow = invalidate;
}

-(NSTrackingArea *)trackingArea
{
  return trackingArea;
}

-(GdkMacosSurface *)gdkSurface
{
  return [(GdkMacosWindow *)[self window] gdkSurface];
}

-(GdkMacosDisplay *)gdkDisplay
{
  GdkMacosSurface *surface = [self gdkSurface];
  GdkDisplay *display = gdk_surface_get_display (GDK_SURFACE (surface));

  return GDK_MACOS_DISPLAY (display);
}

-(void)keyDown:(NSEvent *)theEvent
{
  /* NOTE: When user press Cmd+A, interpretKeyEvents: will call noop:
   * method. When user press and hold A to show the accented char window,
   * it consumed repeating key down events for key 'A' do NOT call
   * any other method. We use this behavior to determine if this key
   * down event is filtered by interpretKeyEvents.
   */
  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_FILTERED));

  GDK_NOTE (EVENTS, g_message ("keyDown"));
  [self interpretKeyEvents: [NSArray arrayWithObject: theEvent]];
}

-(void)flagsChanged: (NSEvent *)theEvent
{
}

-(NSUInteger)characterIndexForPoint:(NSPoint)aPoint
{
  GDK_NOTE (EVENTS, g_message ("characterIndexForPoint"));
  return 0;
}

-(NSRect)firstRectForCharacterRange:(NSRange)aRange actualRange: (NSRangePointer)actualRange
{
  GdkRectangle *rect;

  GDK_NOTE (EVENTS, g_message ("firstRectForCharacterRange"));

  if ((rect = g_object_get_data (G_OBJECT ([self gdkSurface]), GIC_CURSOR_RECT)))
    {
      GdkMacosDisplay *display = [self gdkDisplay];
      int ns_x, ns_y;

      _gdk_macos_display_to_display_coords (display,
                                            rect->x, rect->y + rect->height,
                                            &ns_x, &ns_y);

      return NSMakeRect (ns_x, ns_y, rect->width, rect->height);
    }

  return NSMakeRect (0, 0, 0, 0);
}

-(NSArray *)validAttributesForMarkedText
{
  GDK_NOTE (EVENTS, g_message ("validAttributesForMarkedText"));
  return [NSArray arrayWithObjects: NSUnderlineStyleAttributeName, nil];
}

-(NSAttributedString *)attributedSubstringForProposedRange: (NSRange)aRange actualRange: (NSRangePointer)actualRange
{
  GDK_NOTE (EVENTS, g_message ("attributedSubstringForProposedRange"));
  return nil;
}

-(BOOL)hasMarkedText
{
  GDK_NOTE (EVENTS, g_message ("hasMarkedText"));
  return markedRange.location != NSNotFound && markedRange.length != 0;
}

-(NSRange)markedRange
{
  GDK_NOTE (EVENTS, g_message ("markedRange"));
  return markedRange;
}

-(NSRange)selectedRange
{
  GDK_NOTE (EVENTS, g_message ("selectedRange"));
  return selectedRange;
}

-(void)unmarkText
{
  GDK_NOTE (EVENTS, g_message ("unmarkText"));

  selectedRange = NSMakeRange (0, 0);
  markedRange = NSMakeRange (NSNotFound, 0);

  g_object_set_data_full (G_OBJECT ([self gdkSurface]), TIC_MARKED_TEXT, NULL, g_free);
}

-(void)setMarkedText:(id)aString selectedRange: (NSRange)newSelection replacementRange: (NSRange)replacementRange
{
  const char *str;

  GDK_NOTE (EVENTS, g_message ("setMarkedText"));

  if (replacementRange.location == NSNotFound)
    {
      markedRange = NSMakeRange (newSelection.location, [aString length]);
      selectedRange = NSMakeRange (newSelection.location, newSelection.length);
    }
  else
    {
      markedRange = NSMakeRange (replacementRange.location, [aString length]);
      selectedRange = NSMakeRange (replacementRange.location + newSelection.location, newSelection.length);
    }

  if ([aString isKindOfClass: [NSAttributedString class]])
    str = [[aString string] UTF8String];
  else
    str = [aString UTF8String];

  g_object_set_data_full (G_OBJECT ([self gdkSurface]), TIC_MARKED_TEXT, g_strdup (str), g_free);
  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     TIC_SELECTED_POS,
                     GUINT_TO_POINTER (selectedRange.location));
  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     TIC_SELECTED_LEN,
                     GUINT_TO_POINTER (selectedRange.length));

  GDK_NOTE (EVENTS, g_message ("setMarkedText: set %s (%p, nsview %p): %s",
                               TIC_MARKED_TEXT, [self gdkSurface], self,
                               str ? str : "(empty)"));

  /* handle text input changes by mouse events */
  if (!GPOINTER_TO_UINT (g_object_get_data (G_OBJECT ([self gdkSurface]), TIC_IN_KEY_DOWN)))
    _gdk_macos_surface_synthesize_null_key ([self gdkSurface]);
}

-(void)doCommandBySelector:(SEL)aSelector
{
  GDK_NOTE (EVENTS, g_message ("doCommandBySelector"));

  if ([self respondsToSelector: aSelector])
    [self performSelector: aSelector];
}

-(void)insertText:(id)aString replacementRange: (NSRange)replacementRange
{
  const char *str;
  NSString *string;

  GDK_NOTE (EVENTS, g_message ("insertText"));

  if ([self hasMarkedText])
    [self unmarkText];

  if ([aString isKindOfClass: [NSAttributedString class]])
    string = [aString string];
  else
    string = aString;

  NSCharacterSet *ctrlChars = [NSCharacterSet controlCharacterSet];
  NSCharacterSet *wsnlChars = [NSCharacterSet whitespaceAndNewlineCharacterSet];
  if ([string rangeOfCharacterFromSet:ctrlChars].length &&
      [string rangeOfCharacterFromSet:wsnlChars].length == 0)
    {
      /* discard invalid text input with Chinese input methods */
      str = "";
      [self unmarkText];
      [[NSTextInputContext currentInputContext] discardMarkedText];
    }
  else
   {
      str = [string UTF8String];
   }

  g_object_set_data_full (G_OBJECT ([self gdkSurface]), TIC_INSERT_TEXT, g_strdup (str), g_free);
  GDK_NOTE (EVENTS, g_message ("insertText: set %s (%p, nsview %p): %s",
                               TIC_INSERT_TEXT, [self gdkSurface], self,
                               str ? str : "(empty)"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_FILTERED));

  /* handle text input changes by mouse events */
  if (!GPOINTER_TO_UINT (g_object_get_data (G_OBJECT ([self gdkSurface]), TIC_IN_KEY_DOWN)))
    _gdk_macos_surface_synthesize_null_key ([self gdkSurface]);
}

-(void)deleteBackward:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("deleteBackward"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)deleteForward:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("deleteForward"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)deleteToBeginningOfLine:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("deleteToBeginningOfLine"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)deleteToEndOfLine:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("deleteToEndOfLine"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)deleteWordBackward:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("deleteWordBackward"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)deleteWordForward:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("deleteWordForward"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)insertBacktab:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("insertBacktab"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)insertNewline:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("insertNewline"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)insertTab:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("insertTab"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveBackward:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveBackward"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveBackwardAndModifySelection:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveBackwardAndModifySelection"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveDown:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveDown"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveDownAndModifySelection:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveDownAndModifySelection"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveForward:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveForward"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveForwardAndModifySelection:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveForwardAndModifySelection"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveLeft:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveLeft"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveLeftAndModifySelection:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveLeftAndModifySelection"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveRight:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveRight"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveRightAndModifySelection:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveRightAndModifySelection"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveToBeginningOfDocument:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveToBeginningOfDocument"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveToBeginningOfDocumentAndModifySelection:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveToBeginningOfDocumentAndModifySelection"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveToBeginningOfLine:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveToBeginningOfLine"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveToBeginningOfLineAndModifySelection:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveToBeginningOfLineAndModifySelection"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveToEndOfDocument:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveToEndOfDocument"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveToEndOfDocumentAndModifySelection:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveToEndOfDocumentAndModifySelection"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveToEndOfLine:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveToEndOfLine"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveToEndOfLineAndModifySelection:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveToEndOfLineAndModifySelection"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveUp:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveUp"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveUpAndModifySelection:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveUpAndModifySelection"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveWordBackward:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveWordBackward"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveWordBackwardAndModifySelection:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveWordBackwardAndModifySelection"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveWordForward:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveWordForward"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveWordForwardAndModifySelection:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveWordForwardAndModifySelection"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveWordLeft:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveWordLeft"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveWordLeftAndModifySelection:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveWordLeftAndModifySelection"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveWordRight:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveWordRight"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)moveWordRightAndModifySelection:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("moveWordRightAndModifySelection"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)pageDown:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("pageDown"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)pageDownAndModifySelection:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("pageDownAndModifySelection"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)pageUp:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("pageUp"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)pageUpAndModifySelection:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("pageUpAndModifySelection"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)selectAll:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("selectAll"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)selectLine:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("selectLine"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)selectWord:(id)sender
{
  GDK_NOTE (EVENTS, g_message ("selectWord"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)noop: (id)sender
{
  GDK_NOTE (EVENTS, g_message ("noop"));

  g_object_set_data (G_OBJECT ([self gdkSurface]),
                     GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

@end
