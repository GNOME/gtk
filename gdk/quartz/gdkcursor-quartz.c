/* gdkcursor-quartz.c
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

#include <config.h>

#include "gdkdisplay.h"
#include "gdkcursor.h"
#include "gdkprivate-quartz.h"

static GdkCursor *
gdk_quartz_cursor_new_from_nscursor (NSCursor *nscursor, GdkCursorType cursor_type)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;

  private = g_new (GdkCursorPrivate, 1);
  private->nscursor = nscursor;
  cursor = (GdkCursor *)private;
  cursor->type = cursor_type;
  cursor->ref_count = 1;

  return cursor;
}

GdkCursor*
gdk_cursor_new_for_display (GdkDisplay    *display,
			    GdkCursorType  cursor_type)
{
  NSCursor *nscursor;

  g_return_val_if_fail (display == gdk_display_get_default (), NULL);

  switch (cursor_type) 
    {
    case GDK_XTERM:
      nscursor = [NSCursor IBeamCursor];
      break;
    case GDK_SB_H_DOUBLE_ARROW:
      nscursor = [NSCursor resizeLeftRightCursor];
      break;
    case GDK_SB_V_DOUBLE_ARROW:
      nscursor = [NSCursor resizeUpDownCursor];
      break;
    case GDK_SB_UP_ARROW:
    case GDK_BASED_ARROW_UP:
    case GDK_BOTTOM_TEE:
    case GDK_TOP_SIDE:
      nscursor = [NSCursor resizeUpCursor];
      break;
    case GDK_SB_DOWN_ARROW:
    case GDK_BASED_ARROW_DOWN:
    case GDK_TOP_TEE:
    case GDK_BOTTOM_SIDE:
      nscursor = [NSCursor resizeDownCursor];
      break;
    case GDK_SB_LEFT_ARROW:
    case GDK_RIGHT_TEE:
    case GDK_LEFT_SIDE:
      nscursor = [NSCursor resizeLeftCursor];
      break;
    case GDK_SB_RIGHT_ARROW:
    case GDK_LEFT_TEE:
    case GDK_RIGHT_SIDE:
      nscursor = [NSCursor resizeRightCursor];
      break;
    case GDK_TCROSS:
    case GDK_CROSS:
    case GDK_CROSSHAIR:
    case GDK_DIAMOND_CROSS:
      nscursor = [NSCursor crosshairCursor];
    case GDK_HAND1:
    case GDK_HAND2:
      nscursor = [NSCursor pointingHandCursor];
      break;
    default:
      g_warning ("Unsupported cursor type %d, using default", cursor_type);
      nscursor = [NSCursor arrowCursor];
    }
  
  [nscursor retain];
 
  return gdk_quartz_cursor_new_from_nscursor (nscursor, cursor_type);
}

GdkCursor*
gdk_cursor_new_from_pixmap (GdkPixmap      *source,
			    GdkPixmap      *mask,
			    const GdkColor *fg,
			    const GdkColor *bg,
			    gint            x,
			    gint            y)
{
  NSAutoreleasePool *pool;
  NSBitmapImageRep *bitmap_rep;
  NSImage *image;
  NSCursor *nscursor;
  GdkCursor *cursor;
  gint width, height;
  gint tmp_x, tmp_y;
  guchar *dst_data, *mask_data, *src_data;
  guchar *mask_start, *src_start;
  int dst_stride;

  gdk_drawable_get_size (source, &width, &height);

  g_return_val_if_fail (GDK_IS_PIXMAP (source), NULL);
  g_return_val_if_fail (GDK_IS_PIXMAP (mask), NULL);
  g_return_val_if_fail (fg != NULL, NULL);
  g_return_val_if_fail (bg != NULL, NULL);

  pool = [[NSAutoreleasePool alloc] init];

  bitmap_rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
		pixelsWide:width pixelsHigh:height
		bitsPerSample:8 samplesPerPixel:4
		hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
		bytesPerRow:0 bitsPerPixel:0];

  dst_stride = [bitmap_rep bytesPerRow];
  mask_start = GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (mask)->impl)->data;
  src_start = GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (source)->impl)->data;

  for (tmp_y = 0; tmp_y < height; tmp_y++)
    {
      dst_data = [bitmap_rep bitmapData] + tmp_y * dst_stride;
      mask_data = mask_start + tmp_y * width;
      src_data = src_start + tmp_y * width;

      for (tmp_x = 0; tmp_x < width; tmp_x++)
	{
	  if (*mask_data++)
	    {
	      const GdkColor *color;

	      if (*src_data++)
		color = fg;
	      else
		color = bg;

	      *dst_data++ = (color->red >> 8) & 0xff;
	      *dst_data++ = (color->green >> 8) & 0xff;
	      *dst_data++ = (color->blue >> 8) & 0xff;
	      *dst_data++ = 0xff;

	    }
	  else
	    {
	      *dst_data++ = 0x00;
	      *dst_data++ = 0x00;
	      *dst_data++ = 0x00;
	      *dst_data++ = 0x00;

	      src_data++;
	    }
	}
    }
  image = [[NSImage alloc] init];
  [image addRepresentation:bitmap_rep];
  [bitmap_rep release];

  nscursor = [[NSCursor alloc] initWithImage:image hotSpot:NSMakePoint(x, y)];
  [image release];

  cursor = gdk_quartz_cursor_new_from_nscursor (nscursor, GDK_CURSOR_IS_PIXMAP);
  [pool release];

  return cursor;
}

GdkCursor *
gdk_cursor_new_from_pixbuf (GdkDisplay *display, 
			    GdkPixbuf  *pixbuf,
			    gint        x,
			    gint        y)
{
  NSAutoreleasePool *pool;
  NSBitmapImageRep *bitmap_rep;
  NSImage *image;
  NSCursor *nscursor;
  GdkCursor *cursor;
  gboolean has_alpha;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);
  g_return_val_if_fail (0 <= x && x < gdk_pixbuf_get_width (pixbuf), NULL);
  g_return_val_if_fail (0 <= y && y < gdk_pixbuf_get_height (pixbuf), NULL);

  pool = [[NSAutoreleasePool alloc] init];

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

  nscursor = [[NSCursor alloc] initWithImage:image hotSpot:NSMakePoint(x, y)];
  [image release];

  cursor = gdk_quartz_cursor_new_from_nscursor (nscursor, GDK_CURSOR_IS_PIXMAP);
  [pool release];

  return cursor;
}

GdkCursor*  
gdk_cursor_new_from_name (GdkDisplay  *display,  
			  const gchar *name)
{
  /* FIXME: Implement */
  return NULL;
}

void
_gdk_cursor_destroy (GdkCursor *cursor)
{
  GdkCursorPrivate *private;

  g_return_if_fail (cursor != NULL);
  g_return_if_fail (cursor->ref_count == 0);

  private = (GdkCursorPrivate *)cursor;
  [private->nscursor release];
  
  g_free (private);
}

gboolean 
gdk_display_supports_cursor_alpha (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return TRUE;
}

gboolean 
gdk_display_supports_cursor_color (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return TRUE;
}

guint     
gdk_display_get_default_cursor_size (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  /* Mac OS X doesn't have the notion of a default size */
  return 32;
}

void     
gdk_display_get_maximal_cursor_size (GdkDisplay *display,
				     guint       *width,
				     guint       *height)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  /* Cursor sizes in Mac OS X can be arbitrarily large */
  *width = 65536;
  *height = 65536;
}

GdkDisplay *
gdk_cursor_get_display (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, NULL);

  return gdk_display_get_default ();
}

GdkPixbuf *
gdk_cursor_get_image (GdkCursor *cursor)
{
  /* FIXME: Implement */
  return NULL;
}
