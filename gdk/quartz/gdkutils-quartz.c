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

#include <gdk/gdk.h>
#include <gdkinternals.h>

#include "gdkquartz-gtk-only.h"
#include "gdkquartz-cocoa-access.h"
#include <gdkquartzutils.h>

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

/*
 * Code for key code conversion
 *
 * Copyright (C) 2009 Paul Davis
 */
gunichar
gdk_quartz_get_key_equivalent (guint key)
{
  if (key >= GDK_KEY_A && key <= GDK_KEY_Z)
    return key + (GDK_KEY_a - GDK_KEY_A);

  if (key >= GDK_KEY_space && key <= GDK_KEY_asciitilde)
    return key;

  switch (key)
    {
      case GDK_KEY_BackSpace:
        return NSBackspaceCharacter;
      case GDK_KEY_Delete:
        return NSDeleteFunctionKey;
      case GDK_KEY_Pause:
        return NSPauseFunctionKey;
      case GDK_KEY_Scroll_Lock:
        return NSScrollLockFunctionKey;
      case GDK_KEY_Sys_Req:
        return NSSysReqFunctionKey;
      case GDK_KEY_Home:
        return NSHomeFunctionKey;
      case GDK_KEY_Left:
      case GDK_KEY_leftarrow:
        return NSLeftArrowFunctionKey;
      case GDK_KEY_Up:
      case GDK_KEY_uparrow:
        return NSUpArrowFunctionKey;
      case GDK_KEY_Right:
      case GDK_KEY_rightarrow:
        return NSRightArrowFunctionKey;
      case GDK_KEY_Down:
      case GDK_KEY_downarrow:
        return NSDownArrowFunctionKey;
      case GDK_KEY_Page_Up:
        return NSPageUpFunctionKey;
      case GDK_KEY_Page_Down:
        return NSPageDownFunctionKey;
      case GDK_KEY_End:
        return NSEndFunctionKey;
      case GDK_KEY_Begin:
        return NSBeginFunctionKey;
      case GDK_KEY_Select:
        return NSSelectFunctionKey;
      case GDK_KEY_Print:
        return NSPrintFunctionKey;
      case GDK_KEY_Execute:
        return NSExecuteFunctionKey;
      case GDK_KEY_Insert:
        return NSInsertFunctionKey;
      case GDK_KEY_Undo:
        return NSUndoFunctionKey;
      case GDK_KEY_Redo:
        return NSRedoFunctionKey;
      case GDK_KEY_Menu:
        return NSMenuFunctionKey;
      case GDK_KEY_Find:
        return NSFindFunctionKey;
      case GDK_KEY_Help:
        return NSHelpFunctionKey;
      case GDK_KEY_Break:
        return NSBreakFunctionKey;
      case GDK_KEY_Mode_switch:
        return NSModeSwitchFunctionKey;
      case GDK_KEY_F1:
        return NSF1FunctionKey;
      case GDK_KEY_F2:
        return NSF2FunctionKey;
      case GDK_KEY_F3:
        return NSF3FunctionKey;
      case GDK_KEY_F4:
        return NSF4FunctionKey;
      case GDK_KEY_F5:
        return NSF5FunctionKey;
      case GDK_KEY_F6:
        return NSF6FunctionKey;
      case GDK_KEY_F7:
        return NSF7FunctionKey;
      case GDK_KEY_F8:
        return NSF8FunctionKey;
      case GDK_KEY_F9:
        return NSF9FunctionKey;
      case GDK_KEY_F10:
        return NSF10FunctionKey;
      case GDK_KEY_F11:
        return NSF11FunctionKey;
      case GDK_KEY_F12:
        return NSF12FunctionKey;
      case GDK_KEY_F13:
        return NSF13FunctionKey;
      case GDK_KEY_F14:
        return NSF14FunctionKey;
      case GDK_KEY_F15:
        return NSF15FunctionKey;
      case GDK_KEY_F16:
        return NSF16FunctionKey;
      case GDK_KEY_F17:
        return NSF17FunctionKey;
      case GDK_KEY_F18:
        return NSF18FunctionKey;
      case GDK_KEY_F19:
        return NSF19FunctionKey;
      case GDK_KEY_F20:
        return NSF20FunctionKey;
      case GDK_KEY_F21:
        return NSF21FunctionKey;
      case GDK_KEY_F22:
        return NSF22FunctionKey;
      case GDK_KEY_F23:
        return NSF23FunctionKey;
      case GDK_KEY_F24:
        return NSF24FunctionKey;
      case GDK_KEY_F25:
        return NSF25FunctionKey;
      case GDK_KEY_F26:
        return NSF26FunctionKey;
      case GDK_KEY_F27:
        return NSF27FunctionKey;
      case GDK_KEY_F28:
        return NSF28FunctionKey;
      case GDK_KEY_F29:
        return NSF29FunctionKey;
      case GDK_KEY_F30:
        return NSF30FunctionKey;
      case GDK_KEY_F31:
        return NSF31FunctionKey;
      case GDK_KEY_F32:
        return NSF32FunctionKey;
      case GDK_KEY_F33:
        return NSF33FunctionKey;
      case GDK_KEY_F34:
        return NSF34FunctionKey;
      case GDK_KEY_F35:
        return NSF35FunctionKey;
      default:
        break;
    }

  return '\0';
}
