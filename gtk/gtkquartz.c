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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkquartz.h"
#include "gtkselectionprivate.h"
#include <gdk/quartz/gdkquartz-gtk-only.h>


static gboolean
_cairo_surface_extents (cairo_surface_t *surface,
                            GdkRectangle *extents)
{
  double x1, x2, y1, y2;
  cairo_t *cr;

  g_return_val_if_fail (surface != NULL, FALSE);
  g_return_val_if_fail (extents != NULL, FALSE);

  cr = cairo_create (surface);
  cairo_clip_extents (cr, &x1, &y1, &x2, &y2);

  x1 = floor (x1);
  y1 = floor (y1);
  x2 = ceil (x2);
  y2 = ceil (y2);
  x2 -= x1;
  y2 -= y1;

  if (x1 < G_MININT || x1 > G_MAXINT ||
      y1 < G_MININT || y1 > G_MAXINT ||
      x2 > G_MAXINT || y2 > G_MAXINT)
    {
      extents->x = extents->y = extents->width = extents->height = 0;
      return FALSE;
    }

  extents->x = x1;
  extents->y = y1;
  extents->width = x2;
  extents->height = y2;

  return TRUE;
}

static void
_data_provider_release_cairo_surface (void* info, const void* data, size_t size)
{
  cairo_surface_destroy ((cairo_surface_t *)info);
}

/* Returns a new NSImage or %NULL in case of an error.
 * The device scale factor will be transfered to the NSImage (hidpi)
 */
NSImage *
_gtk_quartz_create_image_from_surface (cairo_surface_t *surface)
{
  CGColorSpaceRef colorspace;
  CGDataProviderRef data_provider;
  CGImageRef image;
  void *data;
  NSImage *nsimage;
  double sx, sy;
  cairo_t *cr;
  cairo_surface_t *img_surface;
  cairo_rectangle_int_t extents;
  int width, height, rowstride;

  if (!_cairo_surface_extents (surface, &extents))
    return NULL;

  cairo_surface_get_device_scale (surface, &sx, &sy);
  width = extents.width * sx;
  height = extents.height * sy;

  img_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (img_surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_scale (cr, sx, sy);
  cairo_set_source_surface (cr, surface, -extents.x, -extents.y);
  cairo_paint (cr);
  cairo_destroy (cr);

  cairo_surface_flush (img_surface);
  rowstride = cairo_image_surface_get_stride (img_surface);
  data = cairo_image_surface_get_data (img_surface);

  colorspace = CGColorSpaceCreateDeviceRGB ();
  /* Note: the release callback will only be called after NSImage below dies */
  data_provider = CGDataProviderCreateWithData (surface, data, height * rowstride,
                                                _data_provider_release_cairo_surface);

  image = CGImageCreate (width, height, 8,
                         32, rowstride,
                         colorspace,
                         /* XXX: kCGBitmapByteOrderDefault gives wrong colors..?? */
                         kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst,
                         data_provider, NULL, FALSE,
                         kCGRenderingIntentDefault);
  CGDataProviderRelease (data_provider);
  CGColorSpaceRelease (colorspace);

  nsimage = [[NSImage alloc] initWithCGImage:image size:NSMakeSize (extents.width, extents.height)];
  CGImageRelease (image);

  return nsimage;
}

NSSet *
_gtk_quartz_target_list_to_pasteboard_types (GtkTargetList *target_list)
{
  NSMutableSet *set = [[NSMutableSet alloc] init];
  GList *list;

  for (list = target_list->list; list; list = list->next)
    {
      GtkTargetPair *pair = list->data;
      g_return_val_if_fail (pair->flags < 16, NULL);
      [set addObject:gdk_quartz_atom_to_pasteboard_type_libgtk_only (pair->target)];
    }

  return set;
}

NSSet *
_gtk_quartz_target_entries_to_pasteboard_types (const GtkTargetEntry *targets,
						guint                 n_targets)
{
  NSMutableSet *set = [[NSMutableSet alloc] init];
  int i;

  for (i = 0; i < n_targets; i++)
    {
      [set addObject:gdk_quartz_target_to_pasteboard_type_libgtk_only (targets[i].target)];
    }

  return set;
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
      GdkAtom atom = gdk_quartz_pasteboard_type_to_atom_libgtk_only ([array objectAtIndex:i]);

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

  selection_data = g_slice_new0 (GtkSelectionData);
  selection_data->selection = selection;
  selection_data->target = target;
  if (!selection_data->display)
    selection_data->display = gdk_display_get_default ();
  if (target == gdk_atom_intern_static_string ("UTF8_STRING"))
    {
      NSString *s = [pasteboard stringForType:GDK_QUARTZ_STRING_PBOARD_TYPE];

      if (s)
	{
          const char *utf8_string = [s UTF8String];

          gtk_selection_data_set (selection_data,
                                  target, 8,
                                  (guchar *)utf8_string, strlen (utf8_string));
	}
    }
  else if (target == gdk_atom_intern_static_string ("application/x-color"))
    {
      NSColor *nscolor = [[NSColor colorFromPasteboard:pasteboard]
                          colorUsingColorSpaceName:NSDeviceRGBColorSpace];
      
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
      if ([[pasteboard types] containsObject:NSFilenamesPboardType])
        {
           gchar **uris;
           NSArray *files = [pasteboard propertyListForType:NSFilenamesPboardType];
           int n_files = [files count];
           int i;

           selection_data->target = gdk_atom_intern_static_string ("text/uri-list");

           uris = (gchar **) g_malloc (sizeof (gchar*) * (n_files + 1));
           for (i = 0; i < n_files; ++i)
             {
               NSString* uriString = [files objectAtIndex:i];
               uriString = [@"file://" stringByAppendingString:uriString];
               uriString = [uriString stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
               uris[i] = (gchar *) [uriString cStringUsingEncoding:NSUTF8StringEncoding];
             }
           uris[i] = NULL;

           gtk_selection_data_set_uris (selection_data, uris);
           g_free (uris);
         }
      else if ([[pasteboard types] containsObject:GDK_QUARTZ_URL_PBOARD_TYPE] ||
               [[pasteboard types] containsObject:GDK_QUARTZ_FILE_PBOARD_TYPE])
        {
          gchar *uris[2];
          NSURL *url = [NSURL URLFromPasteboard:pasteboard];

          selection_data->target = gdk_atom_intern_static_string ("text/uri-list");

          uris[0] = (gchar *) [[url description] UTF8String];

          uris[1] = NULL;
          gtk_selection_data_set_uris (selection_data, uris);
        }
    }
  else
    {
      NSData *data;
      gchar *name;

      name = gdk_atom_name (target);

      if (strcmp (name, "image/tiff") == 0)
	data = [pasteboard dataForType:GDK_QUARTZ_TIFF_PBOARD_TYPE];
      else
	data = [pasteboard dataForType:[NSString stringWithUTF8String:name]];

      g_free (name);

      if (data)
	{
	  gtk_selection_data_set (selection_data,
                                  target, 8,
                                  [data bytes], [data length]);
	}
    }

  return selection_data;
}

void
_gtk_quartz_set_selection_data_for_pasteboard (NSPasteboard     *pasteboard,
					       GtkSelectionData *selection_data)
{
  NSString *type;
  GdkDisplay *display;
  gint format;
  const guchar *data;
  NSUInteger length;

  display = gtk_selection_data_get_display (selection_data);
  format = gtk_selection_data_get_format (selection_data);
  data = gtk_selection_data_get_data (selection_data);
  length = gtk_selection_data_get_length (selection_data);

  type = gdk_quartz_atom_to_pasteboard_type_libgtk_only (gtk_selection_data_get_target (selection_data));

  if ([type isEqualTo:GDK_QUARTZ_STRING_PBOARD_TYPE])
    [pasteboard setString:[NSString stringWithUTF8String:(const char *)data]
                  forType:type];
  else if ([type isEqualTo:GDK_QUARTZ_COLOR_PBOARD_TYPE])
    {
      guint16 *color = (guint16 *)data;
      float red, green, blue, alpha;
      NSColor *nscolor;

      red   = (float)color[0] / 0xffff;
      green = (float)color[1] / 0xffff;
      blue  = (float)color[2] / 0xffff;
      alpha = (float)color[3] / 0xffff;

      nscolor = [NSColor colorWithDeviceRed:red green:green blue:blue alpha:alpha];
      [nscolor writeToPasteboard:pasteboard];
    }
  else if ([type isEqualTo:GDK_QUARTZ_URL_PBOARD_TYPE])
    {
      gchar **uris;

      uris = gtk_selection_data_get_uris (selection_data);
      if (uris != NULL)
        {
          NSURL *url;

          url = [NSURL URLWithString:[NSString stringWithUTF8String:uris[0]]];
          [url writeToPasteboard:pasteboard];
        }
      g_strfreev (uris);
    }
  else
    [pasteboard setData:[NSData dataWithBytesNoCopy:(void *)data
                                             length:length
                                       freeWhenDone:NO]
                                            forType:type];
}

#ifdef QUARTZ_RELOCATION

/* Bundle-based functions for various directories. These almost work
 * even when the application isn’t in a bundle, becuase mainBundle
 * paths point to the bin directory in that case. It’s a simple matter
 * to test for that and remove the last element.
 */

static const gchar *
get_bundle_path (void)
{
  static gchar *path = NULL;

  if (path == NULL)
    {
      NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
      gchar *resource_path = g_strdup ([[[NSBundle mainBundle] resourcePath] UTF8String]);
      gchar *base;
      [pool drain];

      base = g_path_get_basename (resource_path);
      if (strcmp (base, "bin") == 0)
	path = g_path_get_dirname (resource_path);
      else
	path = strdup (resource_path);

      g_free (resource_path);
      g_free (base);
    }

  return path;
}

const gchar *
_gtk_get_datadir (void)
{
  static gchar *path = NULL;

  if (path == NULL)
    path = g_build_filename (get_bundle_path (), "share", NULL);

  return path;
}

const gchar *
_gtk_get_libdir (void)
{
  static gchar *path = NULL;

  if (path == NULL)
    path = g_build_filename (get_bundle_path (), "lib", NULL);

  return path;
}

const gchar *
_gtk_get_localedir (void)
{
  static gchar *path = NULL;

  if (path == NULL)
    path = g_build_filename (get_bundle_path (), "share", "locale", NULL);

  return path;
}

const gchar *
_gtk_get_sysconfdir (void)
{
  static gchar *path = NULL;

  if (path == NULL)
    path = g_build_filename (get_bundle_path (), "etc", NULL);

  return path;
}

const gchar *
_gtk_get_data_prefix (void)
{
  return get_bundle_path ();
}

#endif /* QUARTZ_RELOCATION */
