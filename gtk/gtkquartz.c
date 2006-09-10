/* gtkquartz.c: Utility functions used by the Quartz port
 *
 * Copyright (C) 2006 Imendio AB
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

#include "gtkquartz.h"
#include "gtkalias.h"

NSImage *
_gtk_quartz_create_image_from_pixbuf (GdkPixbuf *pixbuf)
{
  CGColorSpaceRef colorspace;
  CGDataProviderRef data_provider;
  CGContextRef context;
  CGImageRef image;
  void *data;
  int rowstride, pixbuf_width, pixbuf_height;
  gboolean has_alpha;
  NSImage *nsimage;

  pixbuf_width = gdk_pixbuf_get_width (pixbuf);
  pixbuf_height = gdk_pixbuf_get_height (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

  data = gdk_pixbuf_get_pixels (pixbuf);

  colorspace = CGColorSpaceCreateDeviceRGB ();
  data_provider = CGDataProviderCreateWithData (NULL, data, pixbuf_height * rowstride, NULL);

  image = CGImageCreate (pixbuf_width, pixbuf_height, 8,
			 has_alpha ? 32 : 24, rowstride, 
			 colorspace, 
			 has_alpha ? kCGImageAlphaLast : 0,
			 data_provider, NULL, FALSE, 
			 kCGRenderingIntentDefault);

  CGDataProviderRelease (data_provider);
  CGColorSpaceRelease (colorspace);

  nsimage = [[NSImage alloc] initWithSize:NSMakeSize (pixbuf_width, pixbuf_height)];
  [nsimage lockFocus];

  context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
  CGContextDrawImage (context, CGRectMake (0, 0, pixbuf_width, pixbuf_height), image);
 
  [nsimage unlockFocus];

  CGImageRelease (image);

  return nsimage;
}

static NSString *
target_to_pasteboard_type (const char *target)
{
  if (strcmp (target, "UTF8_STRING") == 0)
    return NSStringPboardType;
  else if (strcmp (target, "image/tiff") == 0)
    return NSTIFFPboardType;
  else if (strcmp (target, "application/x-color") == 0)
    return NSColorPboardType;
  else if (strcmp (target, "text/uri-list") == 0)
    return NSURLPboardType;
  else
    return [NSString stringWithUTF8String:target];
}

NSArray *
_gtk_quartz_target_list_to_pasteboard_types (GtkTargetList *target_list)
{
  NSMutableSet *set = [[NSMutableSet alloc] init];
  GList *list;

  for (list = target_list->list; list; list = list->next)
    {
      GtkTargetPair *pair = list->data;
      gchar *target = gdk_atom_name (pair->target);
      [set addObject:target_to_pasteboard_type (target)];
      g_free (target);
    }

  return [set allObjects];
}


NSArray *
_gtk_quartz_target_entries_to_pasteboard_types (const GtkTargetEntry *targets,
						guint                 n_targets)
{
  NSMutableSet *set = [[NSMutableSet alloc] init];
  int i;

  for (i = 0; i < n_targets; i++)
    {
      [set addObject:target_to_pasteboard_type (targets[i].target)];
    }

  return [set allObjects];
}

GdkAtom 
_gtk_quartz_pasteboard_type_to_atom (NSString *type)
{
  if ([type isEqualToString:NSStringPboardType])
    return gdk_atom_intern_static_string ("UTF8_STRING");
  else if ([type isEqualToString:NSTIFFPboardType])
    return gdk_atom_intern_static_string ("image/tiff");
  else if ([type isEqualToString:NSColorPboardType])
    return gdk_atom_intern_static_string ("application/x-color");
  else if ([type isEqualToString:NSURLPboardType])
    return gdk_atom_intern_static_string ("text/uri-list");
  else
    return gdk_atom_intern ([type UTF8String], FALSE);  
}

GList *
_gtk_quartz_pasteboard_types_to_atom_list (NSArray *array)
{
  GList *result = NULL;
  int i;
  int count;

  count = [array count];

  for (i = 0; i < count; i++) 
    {
      GdkAtom atom = _gtk_quartz_pasteboard_type_to_atom ([array objectAtIndex:i]);

      result = g_list_prepend (result, GDK_ATOM_TO_POINTER (atom));
    }

  return result;
}

GtkSelectionData *
_gtk_quartz_get_selection_data_from_pasteboard (NSPasteboard *pasteboard,
						GdkAtom       target,
						GdkAtom       selection)
{
  GtkSelectionData *selection_data = NULL;

  selection_data = g_new0 (GtkSelectionData, 1);
  selection_data->selection = selection;
  selection_data->target = target;

  if (target == gdk_atom_intern_static_string ("UTF8_STRING"))
    {
      NSString *s = [pasteboard stringForType:NSStringPboardType];
      int len = [s length];

      if (s)
	{
	  selection_data->type = target;
	  selection_data->format = 8;
	  selection_data->length = len;
	  selection_data->data = g_memdup ([s UTF8String], len + 1);
	}
    }
  else if (target == gdk_atom_intern_static_string ("application/x-color"))
    {
      NSColor *nscolor = [[NSColor colorFromPasteboard:pasteboard] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
      
      guint16 color[4];
      
      selection_data->target = target;

      color[0] = 0xffff * [nscolor redComponent];
      color[1] = 0xffff * [nscolor greenComponent];
      color[2] = 0xffff * [nscolor blueComponent];
      color[3] = 0xffff * [nscolor alphaComponent];

      gtk_selection_data_set (selection_data, target, 16, (guchar *)color, 8);
    }
  else if (target == gdk_atom_intern_static_string ("text/uri-list"))
    {
      gchar *uris[2];
      NSURL *url = [NSURL URLFromPasteboard:pasteboard];

      selection_data->target = gdk_atom_intern_static_string ("text/uri-list");
      
      uris[0] = [[url description] UTF8String];
      uris[1] = NULL;
      gtk_selection_data_set_uris (selection_data, uris);
    }
  else
    {
      NSData *data;
      gchar *name;

      name = gdk_atom_name (target);

      if (strcmp (name, "image/tiff") == 0)
	data = [pasteboard dataForType:NSTIFFPboardType];
      else
	data = [pasteboard dataForType:[NSString stringWithUTF8String:name]];

      g_free (name);

      if (data)
	{
	  selection_data->type = target;
	  selection_data->format = 8;
	  selection_data->length = [data length];
	  selection_data->data = g_malloc (selection_data->length + 1);
	  selection_data->data[selection_data->length] = '\0';
	  memcpy(selection_data->data, [data bytes], selection_data->length);
	}
    }

  return selection_data;
}

void
_gtk_quartz_set_selection_data_for_pasteboard (NSPasteboard *pasteboard,
					       GtkSelectionData *selection_data)
{
  NSString *type;
  NSData *data;
  gchar *target = gdk_atom_name (selection_data->target);

  type = target_to_pasteboard_type (target);
  g_free (target);
  
  if ([type isEqualTo:NSStringPboardType]) 
    [pasteboard setString:[NSString stringWithUTF8String:(const char *)selection_data->data]
                  forType:type];
  else if ([type isEqualTo:NSColorPboardType])
    {
      guint16 *color = (guint16 *)selection_data->data;
      float red, green, blue, alpha;

      red   = (float)color[0] / 0xffff;
      green = (float)color[1] / 0xffff;
      blue  = (float)color[2] / 0xffff;
      alpha = (float)color[3] / 0xffff;
      
      NSColor *nscolor = [NSColor colorWithDeviceRed:red green:green blue:blue alpha:alpha];

      [nscolor writeToPasteboard:pasteboard];
    }
  else if ([type isEqualTo:NSURLPboardType])
    {
      gchar **list;
      gchar **result = NULL;
      NSURL *url;

      int count = gdk_text_property_to_utf8_list_for_display (selection_data->display,
							      gdk_atom_intern_static_string ("UTF8_STRING"),
							      selection_data->format,
							      selection_data->data,
							      selection_data->length,
							      &list);

      if (count > 0)
	result = g_uri_list_extract_uris (list[0]);
      g_strfreev (list);

      url = [NSURL URLWithString:[NSString stringWithUTF8String:result[0]]];
      [url writeToPasteboard:pasteboard];

      g_strfreev (result);

    }
  else
    [pasteboard setData:[NSData dataWithBytesNoCopy:selection_data->data
	    	                             length:selection_data->length
			               freeWhenDone:NO]
                forType:type];
}


