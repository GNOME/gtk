/* GdkQuartzView.m
 *
 * Copyright (C) 2005-2007 Imendio AB
 * Copyright (C) 2011 Hiroyuki Yamamoto
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

#include <AvailabilityMacros.h>
#include "config.h"
#import "GdkQuartzView.h"
#include "gdkquartzwindow.h"
#include "gdkprivate-quartz.h"
#include "gdkquartz.h"
#include "gdkinternal-quartz.h"
#include <cairo/cairo-quartz.h>
#import <AppKit/AppKit.h>
#import <IOSurface/IOSurface.h>

@implementation GdkQuartzView



-(id)initWithFrame: (NSRect)frameRect
{
  if ((self = [super initWithFrame: frameRect]))
    {
      pb_props = @{
        (id)kCVPixelBufferIOSurfaceCoreAnimationCompatibilityKey: @1,
        (id)kCVPixelBufferBytesPerRowAlignmentKey: @64,
      };
      [pb_props retain];
      cfpb_props  = (__bridge CFDictionaryRef)pb_props;

      markedRange = NSMakeRange (NSNotFound, 0);
      selectedRange = NSMakeRange (0, 0);
    }

  [self setValue: @(YES) forKey: @"postsFrameChangedNotifications"];

  return self;
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

-(void) keyDown: (NSEvent *) theEvent
{
  /* NOTE: When user press Cmd+A, interpretKeyEvents: will call noop:
     method. When user press and hold A to show the accented char window,
     it consumed repeating key down events for key 'A' do NOT call
     any other method. We use this behavior to determine if this key
     down event is filtered by interpretKeyEvents.
  */

  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_FILTERED));

  GDK_NOTE (EVENTS, g_message ("keyDown"));
  [self interpretKeyEvents: [NSArray arrayWithObject: theEvent]];
}

-(void)flagsChanged: (NSEvent *) theEvent
{
}

-(NSUInteger)characterIndexForPoint: (NSPoint)aPoint
{
  GDK_NOTE (EVENTS, g_message ("characterIndexForPoint"));
  return 0;
}

-(NSRect)firstRectForCharacterRange: (NSRange)aRange actualRange: (NSRangePointer)actualRange
{
  GDK_NOTE (EVENTS, g_message ("firstRectForCharacterRange"));
  gint ns_x, ns_y;
  GdkRectangle *rect;

  rect = g_object_get_data (G_OBJECT (gdk_window), GIC_CURSOR_RECT);
  if (rect)
    {
      _gdk_quartz_window_gdk_xy_to_xy (rect->x, rect->y + rect->height,
				       &ns_x, &ns_y);

      return NSMakeRect (ns_x, ns_y, rect->width, rect->height);
    }
  else
    {
      return NSMakeRect (0, 0, 0, 0);
    }
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

  g_object_set_data_full (G_OBJECT (gdk_window), TIC_MARKED_TEXT, NULL, g_free);
}

-(void)setMarkedText: (id)aString selectedRange: (NSRange)newSelection replacementRange: (NSRange)replacementRange
{
  GDK_NOTE (EVENTS, g_message ("setMarkedText"));
  const char *str;

  if (replacementRange.location == NSNotFound)
    {
      markedRange = NSMakeRange (newSelection.location, [aString length]);
      selectedRange = NSMakeRange (newSelection.location, newSelection.length);
    }
  else {
      markedRange = NSMakeRange (replacementRange.location, [aString length]);
      selectedRange = NSMakeRange (replacementRange.location + newSelection.location, newSelection.length);
    }

  if ([aString isKindOfClass: [NSAttributedString class]])
    {
      str = [[aString string] UTF8String];
    }
  else {
      str = [aString UTF8String];
    }

  g_object_set_data_full (G_OBJECT (gdk_window), TIC_MARKED_TEXT, g_strdup (str), g_free);
  g_object_set_data (G_OBJECT (gdk_window), TIC_SELECTED_POS,
		     GUINT_TO_POINTER (selectedRange.location));
  g_object_set_data (G_OBJECT (gdk_window), TIC_SELECTED_LEN,
		     GUINT_TO_POINTER (selectedRange.length));

  GDK_NOTE (EVENTS, g_message ("setMarkedText: set %s (%p, nsview %p): %s",
			       TIC_MARKED_TEXT, gdk_window, self,
			       str ? str : "(empty)"));

  /* handle text input changes by mouse events */
  if (!GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (gdk_window),
                                            TIC_IN_KEY_DOWN)))
    {
      _gdk_quartz_synthesize_null_key_event(gdk_window);
    }
}

-(void)doCommandBySelector: (SEL)aSelector
{
     GDK_NOTE (EVENTS, g_message ("doCommandBySelector %s", [NSStringFromSelector (aSelector) UTF8String]));
  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
                     GUINT_TO_POINTER (GIC_FILTER_PASSTHRU));
}

-(void)insertText: (id)aString replacementRange: (NSRange)replacementRange
{
  GDK_NOTE (EVENTS, g_message ("insertText"));
  const char *str;
  NSString *string;

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
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
      NSInputManager *currentInputManager = [NSInputManager currentInputManager];
      [currentInputManager markedTextAbandoned:self];
#else
      [[NSTextInputContext currentInputContext] discardMarkedText];
#endif
    }
  else
   {
      str = [string UTF8String];
      selectedRange = NSMakeRange ([string length], 0);
   }

  if (replacementRange.length > 0)
    {
      g_object_set_data (G_OBJECT (gdk_window), TIC_INSERT_TEXT_REPLACE_LEN,
                         GINT_TO_POINTER (replacementRange.length));
    }

  g_object_set_data_full (G_OBJECT (gdk_window), TIC_INSERT_TEXT, g_strdup (str), g_free);
  GDK_NOTE (EVENTS, g_message ("insertText: set %s (%p, nsview %p): %s",
			     TIC_INSERT_TEXT, gdk_window, self,
			     str ? str : "(empty)"));

  g_object_set_data (G_OBJECT (gdk_window), GIC_FILTER_KEY,
		     GUINT_TO_POINTER (GIC_FILTER_FILTERED));

  /* handle text input changes by mouse events */
  if (!GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (gdk_window),
                                            TIC_IN_KEY_DOWN)))
    {
      _gdk_quartz_synthesize_null_key_event(gdk_window);
    }
}
/* --------------------------------------------------------------- */

-(void)dealloc
{
  if (trackingRect)
    {
      [self removeTrackingRect: trackingRect];
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 10500
      [(NSTrackingArea*)trackingRect release];
#endif
      trackingRect = 0;
    }

  if (pixels)
    {
      CVPixelBufferRelease (pixels);
    }

  [pb_props release];
  [super dealloc];
}

-(void)setGdkWindow: (GdkWindow *)window
{
  gdk_window = window;
}

-(GdkWindow *)gdkWindow
{
  return gdk_window;
}

-(NSTrackingRectTag)trackingRect
{
  return trackingRect;
}

-(BOOL)isFlipped
{
  return NO;
}

-(BOOL)isOpaque
{
  if (GDK_WINDOW_DESTROYED (gdk_window))
    return YES;

  /* A view is opaque if its GdkWindow doesn't have the RGBA visual */
  return gdk_window_get_visual (gdk_window) !=
    gdk_screen_get_rgba_visual (_gdk_screen);
}

- (void) viewWillDraw
{
  /* MacOS 11 (Big Sur) has added a new, dynamic "accent" as default.
   * This uses a 10-bit colorspace so every GIMP drawing operation
   * has the additional cost of an 8-bit (ARGB) to 10-bit conversion.
   * Let's disable this mode to regain the lost performance.
   */
  if(gdk_quartz_osx_version() >= GDK_OSX_BIGSUR)
  {
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    CALayer* layer = self.layer;
    layer.contentsFormat = kCAContentsFormatRGBA8Uint;
#endif
  }

  [super viewWillDraw];
}

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 10900
-(BOOL)wantsUpdateLayer
{
     return YES;
}
#endif

-(BOOL)wantsLayer
{
     return YES;
}

static void
cairo_rect_from_nsrect (cairo_rectangle_int_t *rect, NSRect *nsrect)
{
  rect->x = (int)nsrect->origin.x;
  rect->y = (int)nsrect->origin.y;
  rect->width = (int)nsrect->size.width;
  rect->height = (int)nsrect->size.height;
}

static cairo_status_t
copy_rectangle_argb32 (cairo_surface_t *dest, cairo_surface_t *source,
                       cairo_region_t *region)
{
  cairo_surface_t *source_img, *dest_img;
  cairo_status_t status;
  cairo_format_t format;
  int height, width, stride;
  cairo_rectangle_int_t extents;

  cairo_region_get_extents (region, &extents);
  source_img = cairo_surface_map_to_image (source, &extents);
  status = cairo_surface_status (source_img);

  if (status)
    {
      g_warning ("Failed to map source image surface, %d %d %d %d on %d %d: %s\n",
                 extents.x, extents.y, extents.width, extents.height,
                 cairo_image_surface_get_width (source),
                 cairo_image_surface_get_height (source),
                 cairo_status_to_string (status));
       return status;
    }

  format = cairo_image_surface_get_format (source_img);
  dest_img = cairo_surface_map_to_image (dest, &extents);
  status = cairo_surface_status (dest_img);

  if (status)
    {
      g_warning ("Failed to map destination image surface, %d %d %d %d on %d %d: %s\n",
                 extents.x, extents.y, extents.width, extents.height,
                 cairo_image_surface_get_width (dest),
                 cairo_image_surface_get_height (dest),
                 cairo_status_to_string (status));
       goto CLEANUP;
    }

  width = cairo_image_surface_get_width (source_img);
  stride = cairo_format_stride_for_width (format, width);
  height = cairo_image_surface_get_height (source_img);
  memcpy (cairo_image_surface_get_data (dest_img),
          cairo_image_surface_get_data (source_img),
          stride * height);
  cairo_surface_unmap_image (dest, dest_img);

 CLEANUP:
  cairo_surface_unmap_image (source, source_img);
  return status;
}

-(void)updateLayer
{
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (gdk_window->impl);
  cairo_rectangle_int_t impl_rect = {0, 0, 0, 0};
  CGRect layer_bounds = [self.layer bounds];
  CGRect backing_bounds = [self convertRectToBacking: layer_bounds];
  cairo_rectangle_int_t bounds_rect;
  cairo_region_t *bounds_region;
  cairo_surface_t *cvpb_surface;

  if (GDK_WINDOW_DESTROYED (gdk_window))
    return;

  ++impl->in_paint_rect_count;

  if (impl->needs_display_region)
    {
      cairo_region_t *region = impl->needs_display_region;
      _gdk_window_process_updates_recurse (gdk_window, region);
      cairo_region_destroy (region);
      impl->needs_display_region = NULL;
    }
  else
    {
      cairo_rectangle_int_t bounds;
      cairo_region_t *region;

      cairo_rect_from_nsrect (&bounds, &layer_bounds);
      region = cairo_region_create_rectangle(&bounds);
      _gdk_window_process_updates_recurse (gdk_window, region);
      cairo_region_destroy (region);
    }

  if (!impl || !impl->cairo_surface)
    return;

  CVPixelBufferLockBaseAddress (pixels, 0);
  cvpb_surface =
    cairo_image_surface_create_for_data (CVPixelBufferGetBaseAddress (pixels),
                                         CAIRO_FORMAT_ARGB32,
                                         (int)CVPixelBufferGetWidth (pixels),
                                         (int)CVPixelBufferGetHeight (pixels),
                                         (int)CVPixelBufferGetBytesPerRow (pixels));


  cairo_rect_from_nsrect (&bounds_rect, &backing_bounds);
  bounds_region = cairo_region_create_rectangle (&bounds_rect);

  impl_rect.width = cairo_image_surface_get_width (impl->cairo_surface);
  impl_rect.height = cairo_image_surface_get_height (impl->cairo_surface);

  cairo_region_intersect_rectangle (bounds_region, &impl_rect);
  copy_rectangle_argb32 (cvpb_surface, impl->cairo_surface, bounds_region);

  cairo_surface_destroy (cvpb_surface);
  cairo_region_destroy (bounds_region);
  _gdk_quartz_unref_cairo_surface (gdk_window); // reffed in gdk_window_impl_quartz_begin_paint
  CVPixelBufferUnlockBaseAddress (pixels, 0);

  --impl->in_paint_rect_count;

  self.layer.contents = NULL;
  self.layer.contents = (id)CVPixelBufferGetIOSurface (pixels);
}

-(void)setNeedsInvalidateShadow: (BOOL)invalidate
{
  needsInvalidateShadow = invalidate;
}

/* For information on setting up tracking rects properly, see here:
 * http://developer.apple.com/documentation/Cocoa/Conceptual/EventOverview/EventOverview.pdf
 */
-(void)updateTrackingRect
{
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (gdk_window->impl);
  NSRect rect;
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 10500
  NSTrackingArea *trackingArea;
#endif
  
  if (!impl || !impl->toplevel)
    return;

  if (trackingRect)
    {
      [self removeTrackingRect: trackingRect];
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 10500
      [(NSTrackingArea*)trackingRect release];
#endif
      trackingRect = 0;
    }

  if (!impl->toplevel)
    return;

  /* Note, if we want to set assumeInside we can use:
   * NSPointInRect ([[self window] convertScreenToBase:[NSEvent mouseLocation]], rect)
   */

  rect = [self bounds];
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 10500
  trackingArea = [[NSTrackingArea alloc] initWithRect: rect
                  options: NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingCursorUpdate | NSTrackingActiveInActiveApp | NSTrackingInVisibleRect | NSTrackingEnabledDuringMouseDrag
                  owner: self
                  userInfo: nil];
  [self addTrackingArea: trackingArea];
  trackingRect = (NSInteger)trackingArea;
#else
  trackingRect = [self addTrackingRect: rect
		  owner: self
		  userData: nil
		  assumeInside: NO];
#endif
}

-(void)viewDidMoveToWindow
{
  if (![self window]) /* We are destroyed already */
    return;

  [self updateTrackingRect];
}

-(void)viewWillMoveToWindow: (NSWindow *)newWindow
{
  if (newWindow == nil && trackingRect)
    {
      [self removeTrackingRect: trackingRect];
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 10500
      [(NSTrackingArea*)trackingRect release];
#endif
      trackingRect = 0;
    }
}

-(void)createBackingStoreWithWidth: (CGFloat) width andHeight: (CGFloat) height
{
  IOSurfaceRef surface;

  g_return_if_fail (width && height);

  CVPixelBufferRelease (pixels);
  CVPixelBufferCreate (NULL, width, height,
                       kCVPixelFormatType_32BGRA,
                       cfpb_props, &pixels);

  surface = CVPixelBufferGetIOSurface (pixels);
  IOSurfaceSetValue(surface, CFSTR("IOSurfaceColorSpace"),
                    kCGColorSpaceSRGB);
}

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 10700
-(BOOL)layer:(CALayer*) layer shouldInheritContentsScale: (CGFloat)scale fromWindow: (NSWindow *) window
{
  if (layer == self.layer && window == self.window)
    {
      _gdk_quartz_unref_cairo_surface (gdk_window);
      [self setNeedsDisplay: YES];
    }
  return YES;
}
#endif

-(void)setFrame: (NSRect)frame
{
  if (GDK_WINDOW_DESTROYED (gdk_window))
    return;

  _gdk_quartz_unref_cairo_surface (gdk_window);
  [super setFrame: frame];

  if ([self window])
    [self updateTrackingRect];
}

@end
