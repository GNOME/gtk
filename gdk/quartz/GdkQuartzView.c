/* GdkQuartzView.m
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


#import "GdkQuartzView.h"
#include "gdkwindow-quartz.h"
#include "gdkprivate-quartz.h"

@implementation GdkQuartzView

-(void)setGdkWindow:(GdkWindow *)window
{
  gdk_window = window;
}

-(GdkWindow *)gdkWindow
{
  return gdk_window;
}

-(BOOL)isFlipped
{
  return YES;
}

-(BOOL)isOpaque
{
  /* A view is opaque if its GdkWindow doesn't have the RGBA colormap */
  return gdk_drawable_get_colormap (gdk_window) != gdk_screen_get_rgba_colormap (_gdk_screen);
}

-(void)drawRect:(NSRect)rect 
{
  NSRect bounds = [self bounds];
  GdkRectangle gdk_rect;

  GDK_QUARTZ_ALLOC_POOL;

  /* Draw background */
  if (GDK_WINDOW_OBJECT (gdk_window)->bg_pixmap == NULL)
    {
      CGContextRef context;

      context = [[NSGraphicsContext currentContext] graphicsPort];
      CGContextSaveGState (context);

      _gdk_quartz_set_context_fill_color_from_pixel (context, gdk_drawable_get_colormap (gdk_window),
						    GDK_WINDOW_OBJECT (gdk_window)->bg_color.pixel);

      CGContextFillRect (context, CGRectMake (bounds.origin.x, bounds.origin.y,
					     bounds.size.width, bounds.size.height));
      CGContextRestoreGState (context);
    }

  gdk_rect.x = bounds.origin.x;
  gdk_rect.y = bounds.origin.y;
  gdk_rect.width = bounds.size.width;
  gdk_rect.height = bounds.size.height;
  
  gdk_window_invalidate_rect (gdk_window, &gdk_rect, FALSE);
  gdk_window_process_updates (gdk_window, FALSE);

  GDK_QUARTZ_RELEASE_POOL;
}

@end
