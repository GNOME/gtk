/* gdkutils-quartz.c
 *
 * Copyright (C) 2005 Imendio AB
 * Copyright (C) 2010  Kristian Rietveld  <kris@gtk.org>
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

#include <AppKit/AppKit.h>

#include <gdkquartzutils.h>
#include "gdkprivate-quartz.h"

NSImage *
gdk_quartz_pixbuf_to_ns_image_libgtk_only (GdkPixbuf *pixbuf)
{
  NSBitmapImageRep  *bitmap_rep;
  NSImage           *image;
  gboolean           has_alpha;
  
  has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
  
  /* Create a bitmap image rep */
  bitmap_rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL 
                                         pixelsWide:gdk_pixbuf_get_width (pixbuf)
					 pixelsHigh:gdk_pixbuf_get_height (pixbuf)
					 bitsPerSample:8 samplesPerPixel:has_alpha ? 4 : 3
					 hasAlpha:has_alpha isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
					 bytesPerRow:0 bitsPerPixel:0];
	
  {
    /* Add pixel data to bitmap rep */
    guchar *src, *dst;
    int src_stride, dst_stride;
    int x, y;
		
    src_stride = gdk_pixbuf_get_rowstride (pixbuf);
    dst_stride = [bitmap_rep bytesPerRow];
		
    for (y = 0; y < gdk_pixbuf_get_height (pixbuf); y++) 
      {
	src = gdk_pixbuf_get_pixels (pixbuf) + y * src_stride;
	dst = [bitmap_rep bitmapData] + y * dst_stride;
	
	for (x = 0; x < gdk_pixbuf_get_width (pixbuf); x++)
	  {
	    if (has_alpha)
	      {
		guchar red, green, blue, alpha;
		
		red = *src++;
		green = *src++;
		blue = *src++;
		alpha = *src++;
		
		*dst++ = (red * alpha) / 255;
		*dst++ = (green * alpha) / 255;
		*dst++ = (blue * alpha) / 255;
		*dst++ = alpha;
	      }
	    else
	     {
	       *dst++ = *src++;
	       *dst++ = *src++;
	       *dst++ = *src++;
	     }
	  }
      }	
  }
	
  image = [[NSImage alloc] init];
  [image addRepresentation:bitmap_rep];
  [bitmap_rep release];
  [image autorelease];
	
  return image;
}

NSEvent *
gdk_quartz_event_get_nsevent (GdkEvent *event)
{
  /* FIXME: If the event here is unallocated, we crash. */
  return ((GdkEventPrivate *) event)->windowing_data;
}
