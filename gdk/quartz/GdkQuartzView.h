/* GdkQuartzView.h
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#import <AppKit/AppKit.h>
#import <CoreVideo/CoreVideo.h>
#include "gdk/gdk.h"

/* Text Input Client */
#define TIC_MARKED_TEXT "tic-marked-text"
#define TIC_SELECTED_POS  "tic-selected-pos"
#define TIC_SELECTED_LEN  "tic-selected-len"
#define TIC_INSERT_TEXT "tic-insert-text"
#define TIC_INSERT_TEXT_REPLACE_LEN "tic-insert-text-replace-len"
#define TIC_IN_KEY_DOWN "tic-in-key-down"

/* GtkIMContext */
#define GIC_CURSOR_RECT  "gic-cursor-rect"
#define GIC_FILTER_KEY   "gic-filter-key"
#define GIC_FILTER_PASSTHRU	0
#define GIC_FILTER_FILTERED	1

#if MAC_OS_X_VERSION_MIN_REQUIRED < 101400
@interface GdkQuartzView : NSView <NSTextInputClient>
#else
@interface GdkQuartzView : NSView <NSTextInputClient, NSViewLayerContentScaleDelegate>
#endif
{
  GdkWindow *gdk_window;
  NSTrackingRectTag trackingRect;
  BOOL needsInvalidateShadow;
  NSRange markedRange;
  NSRange selectedRange;
  CVPixelBufferRef pixels;
  NSDictionary *pb_props;
  CFDictionaryRef cfpb_props;
}

- (void)setGdkWindow: (GdkWindow *)window;
- (GdkWindow *)gdkWindow;
- (NSTrackingRectTag)trackingRect;
- (void)setNeedsInvalidateShadow: (BOOL)invalidate;
- (void)createBackingStoreWithWidth: (CGFloat) width andHeight: (CGFloat) height;

@end
