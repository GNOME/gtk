/*
 * Copyright © 2021 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include <glib/gi18n.h>

#include "gdkmacospasteboard-private.h"
#include "gdkmacosutils-private.h"

enum {
  TYPE_STRING,
  TYPE_PBOARD,
  TYPE_URL,
  TYPE_FILE_URL,
  TYPE_COLOR,
  TYPE_TIFF,
  TYPE_PNG,
  TYPE_LAST
};

#define PTYPE(k) (get_pasteboard_type(TYPE_##k))

static NSPasteboardType pasteboard_types[TYPE_LAST];

static NSPasteboardType
get_pasteboard_type (int type)
{
  static gsize initialized = FALSE;

  g_assert (type >= 0);
  g_assert (type < TYPE_LAST);

  if (g_once_init_enter (&initialized))
    {
      pasteboard_types[TYPE_PNG] = NSPasteboardTypePNG;
      pasteboard_types[TYPE_STRING] = NSPasteboardTypeString;
      pasteboard_types[TYPE_TIFF] = NSPasteboardTypeTIFF;
      pasteboard_types[TYPE_COLOR] = NSPasteboardTypeColor;

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      pasteboard_types[TYPE_PBOARD] = NSStringPboardType;
      G_GNUC_END_IGNORE_DEPRECATIONS

#ifdef AVAILABLE_MAC_OS_X_VERSION_10_13_AND_LATER
      pasteboard_types[TYPE_URL] = NSPasteboardTypeURL;
      pasteboard_types[TYPE_FILE_URL] = NSPasteboardTypeFileURL;
#else
      pasteboard_types[TYPE_URL] = [[NSString alloc] initWithUTF8String:"public.url"];
      pasteboard_types[TYPE_FILE_URL] = [[NSString alloc] initWithUTF8String:"public.file-url"];
#endif

      g_once_init_leave (&initialized, TRUE);
    }

  return pasteboard_types[type];
}

const char *
_gdk_macos_pasteboard_from_ns_type (NSPasteboardType type)
{
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if ([type isEqualToString:PTYPE(STRING)] ||
      [type isEqualToString:PTYPE(PBOARD)])
    return g_intern_string ("text/plain;charset=utf-8");
  else if ([type isEqualToString:PTYPE(URL)] ||
           [type isEqualToString:PTYPE(FILE_URL)])
    return g_intern_string ("text/uri-list");
  else if ([type isEqualToString:PTYPE(COLOR)])
    return g_intern_string ("application/x-color");
  else if ([type isEqualToString:PTYPE(TIFF)])
    return g_intern_string ("image/tiff");
  else if ([type isEqualToString:PTYPE(PNG)])
    return g_intern_string ("image/png");

  G_GNUC_END_IGNORE_DEPRECATIONS;

  return NULL;
}

NSPasteboardType
_gdk_macos_pasteboard_to_ns_type (const char       *mime_type,
                                  NSPasteboardType *alternate)
{
  if (alternate)
    *alternate = NULL;

  if (g_strcmp0 (mime_type, "text/plain;charset=utf-8") == 0)
    {
      return PTYPE(STRING);
    }
  else if (g_strcmp0 (mime_type, "text/uri-list") == 0)
    {
      if (alternate)
        *alternate = PTYPE(URL);
      return PTYPE(FILE_URL);
    }
  else if (g_strcmp0 (mime_type, "application/x-color") == 0)
    {
      return PTYPE(COLOR);
    }
  else if (g_strcmp0 (mime_type, "image/tiff") == 0)
    {
      return PTYPE(TIFF);
    }
  else if (g_strcmp0 (mime_type, "image/png") == 0)
    {
      return PTYPE(PNG);
    }

  return nil;
}

static void
populate_content_formats (GdkContentFormatsBuilder *builder,
                          NSPasteboardType          type)
{
  const char *mime_type;

  g_assert (builder != NULL);
  g_assert (type != NULL);

  if ((mime_type = _gdk_macos_pasteboard_from_ns_type (type)))
    gdk_content_formats_builder_add_mime_type (builder, mime_type);
}

static GdkContentFormats *
load_offer_formats (NSPasteboard *pasteboard)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  GdkContentFormatsBuilder *builder;
  GdkContentFormats *formats;

  builder = gdk_content_formats_builder_new ();
  for (NSPasteboardType type in [pasteboard types])
    populate_content_formats (builder, type);
  formats = gdk_content_formats_builder_free_to_formats (builder);

  GDK_END_MACOS_ALLOC_POOL;

  return g_steal_pointer (&formats);
}

GdkContentFormats *
_gdk_macos_pasteboard_load_formats (NSPasteboard *pasteboard)
{
  return load_offer_formats (pasteboard);
}

void
_gdk_macos_pasteboard_send_content (NSPasteboard                   *pasteboard,
                                    GdkContentProvider             *content,
                                    GdkMacosPasteboardDataProvider *dataProvider)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  NSPasteboardItem *item;

  g_return_if_fail (pasteboard != NULL);
  g_return_if_fail (GDK_IS_CONTENT_PROVIDER (content));

  item = [[NSPasteboardItem alloc] init];
  [item setDataProvider:dataProvider forTypes:[dataProvider types]];

  [pasteboard clearContents];
  if ([pasteboard writeObjects:[NSArray arrayWithObject:item]] == NO)
    g_warning ("Failed to write object to pasteboard");

  GDK_END_MACOS_ALLOC_POOL;
}

@implementation GdkMacosPasteboardDataProvider

-(id)initPasteboard:(NSPasteboard *)pasteBoard mimetypes:(const char * const *)mime_types;
{
  [super init];

  self->mimeTypes = g_strdupv ((char **)mime_types);
  self->pasteboard = [pasteBoard retain];

  return self;
}

-(void)dealloc
{
  g_cancellable_cancel (self->cancellable);

  if (self->pasteboard)
    {
      [self->pasteboard release];
      self->pasteboard = nil;
    }

  g_clear_pointer (&self->mimeTypes, g_strfreev);
  g_clear_object (&self->cancellable);

  [super dealloc];
}

-(void)pasteboardFinishedWithDataProvider:(NSPasteboard *)pasteboard
{
}

-(void)pasteboard:(NSPasteboard *)pasteboard item:(NSPasteboardItem *)item provideDataForType:(NSPasteboardType)type
{
}

-(NSArray<NSPasteboardType> *)types
{
  NSMutableArray *ret = [[NSMutableArray alloc] init];

  for (guint i = 0; self->mimeTypes[i]; i++)
    {
      const char *mime_type = self->mimeTypes[i];
      NSPasteboardType type;
      NSPasteboardType alternate = nil;

      if ((type = _gdk_macos_pasteboard_to_ns_type (mime_type, &alternate)))
        {
          [ret addObject:type];
          if (alternate)
            [ret addObject:alternate];
        }
    }

  return g_steal_pointer (&ret);
}

@end

static GInputStream *
create_stream_from_nsdata (NSData *data)
{
  const guint8 *bytes = [data bytes];
  gsize len = [data length];

  return g_memory_input_stream_new_from_data (g_memdup2 (bytes, len), len, g_free);
}

void
_gdk_macos_pasteboard_read_async (GObject             *object,
                                  NSPasteboard        *pasteboard,
                                  GdkContentFormats   *formats,
                                  int                  io_priority,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  GdkContentFormats *offer_formats = NULL;
  const char *mime_type;
  GInputStream *stream = NULL;
  GTask *task = NULL;

  g_assert (G_IS_OBJECT (object));
  g_assert (pasteboard != NULL);
  g_assert (formats != NULL);

  task = g_task_new (object, cancellable, callback, user_data);
  g_task_set_source_tag (task, _gdk_macos_pasteboard_read_async);
  g_task_set_priority (task, io_priority);

  offer_formats = load_offer_formats (pasteboard);
  mime_type = gdk_content_formats_match_mime_type (formats, offer_formats);

  if (mime_type == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "%s",
                               _("No compatible transfer format found"));
      goto cleanup;
    }

  if (strcmp (mime_type, "text/plain;charset=utf-8") == 0)
    {
      NSString *nsstr = [pasteboard stringForType:NSPasteboardTypeString];

      if (nsstr != NULL)
        {
          const char *str = [nsstr UTF8String];
          stream = g_memory_input_stream_new_from_data (g_strdup (str),
                                                        strlen (str) + 1,
                                                        g_free);
        }
    }
  else if (strcmp (mime_type, "text/uri-list") == 0)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

      if ([[pasteboard types] containsObject:PTYPE(FILE_URL)])
        {
          GString *str = g_string_new (NULL);
          NSArray *files = [pasteboard propertyListForType:NSFilenamesPboardType];
          gsize n_files = [files count];
          char *data;
          guint len;

          for (gsize i = 0; i < n_files; ++i)
            {
              NSString* uriString = [files objectAtIndex:i];
              uriString = [@"file://" stringByAppendingString:uriString];
              uriString = [uriString stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];

              g_string_append_printf (str,
                                      "%s\r\n",
                                      [uriString cStringUsingEncoding:NSUTF8StringEncoding]);
            }

          len = str->len;
          data = g_string_free (str, FALSE);
          stream = g_memory_input_stream_new_from_data (data, len, g_free);
        }

      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
  else if (strcmp (mime_type, "application/x-color") == 0)
    {
      NSColorSpace *colorspace;
      NSColor *nscolor;
      guint16 color[4];

      colorspace = [NSColorSpace genericRGBColorSpace];
      nscolor = [[NSColor colorFromPasteboard:pasteboard]
                    colorUsingColorSpace:colorspace];

      color[0] = 0xffff * [nscolor redComponent];
      color[1] = 0xffff * [nscolor greenComponent];
      color[2] = 0xffff * [nscolor blueComponent];
      color[3] = 0xffff * [nscolor alphaComponent];

      stream = g_memory_input_stream_new_from_data (g_memdup2 (&color, sizeof color),
                                                    sizeof color,
                                                    g_free);
    }
  else if (strcmp (mime_type, "image/tiff") == 0)
    {
      NSData *data = [pasteboard dataForType:PTYPE(TIFF)];
      stream = create_stream_from_nsdata (data);
    }
  else if (strcmp (mime_type, "image/png") == 0)
    {
      NSData *data = [pasteboard dataForType:PTYPE(PNG)];
      stream = create_stream_from_nsdata (data);
    }

  if (stream != NULL)
    {
      g_task_set_task_data (task, g_strdup (mime_type), g_free);
      g_task_return_pointer (task, g_steal_pointer (&stream), g_object_unref);
    }
  else
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("Failed to decode contents with mime-type of '%s'"),
                               mime_type);
    }

cleanup:
  g_clear_object (&task);
  g_clear_pointer (&offer_formats, gdk_content_formats_unref);

  GDK_END_MACOS_ALLOC_POOL;
}

GInputStream *
_gdk_macos_pasteboard_read_finish (GObject       *object,
                                   GAsyncResult  *result,
                                   const char   **out_mime_type,
                                   GError       **error)
{
  GTask *task = (GTask *)result;

  g_assert (G_IS_OBJECT (object));
  g_assert (G_IS_TASK (task));

  if (out_mime_type != NULL)
    *out_mime_type = g_strdup (g_task_get_task_data (task));

  return g_task_propagate_pointer (task, error);
}

void
_gdk_macos_pasteboard_register_drag_types (NSWindow *window)
{
  [window registerForDraggedTypes:[NSArray arrayWithObjects:PTYPE(STRING),
                                                            PTYPE(PBOARD),
                                                            PTYPE(URL),
                                                            PTYPE(FILE_URL),
                                                            PTYPE(COLOR),
                                                            PTYPE(TIFF),
                                                            PTYPE(PNG),
                                                            nil]];
}
