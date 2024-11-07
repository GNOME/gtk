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

#include "gdkdragprivate.h"
#include "gdkmacospasteboard-private.h"
#include "gdkmacosutils-private.h"
#include "gdkdebugprivate.h"

const char *
_gdk_macos_pasteboard_from_ns_type (NSPasteboardType type)
{
  gchar *mime_type;

  if ([type isEqualToString:NSPasteboardTypeURL] ||
      [type isEqualToString:NSPasteboardTypeFileURL])
    {
      GDK_DEBUG (DND, "From UTI %s to mime-type text/uri-list", [type UTF8String]);
      return g_intern_string ("text/uri-list");
    }
  else if ([type isEqualToString:NSPasteboardTypeColor])
    {
      GDK_DEBUG (DND, "From UTI %s to mime-type application/x-color", [type UTF8String]);
      return g_intern_string ("application/x-color");
    }
  else if ((mime_type = g_content_type_get_mime_type ([type UTF8String])))
    {
      const gchar *intern = g_intern_string (mime_type);
      GDK_DEBUG (DND, "From UTI %s to mime-type %s", [type UTF8String], mime_type);
      g_free (mime_type);

      return intern;
    }

  return NULL;
}

NSPasteboardType
_gdk_macos_pasteboard_to_ns_type (const char       *mime_type,
                                  NSPasteboardType *alternate)
{
  gchar *uti_str;

  if (alternate)
    *alternate = NULL;

  if (g_strcmp0 (mime_type, "text/uri-list") == 0)
    {
      if (alternate)
        *alternate = NSPasteboardTypeURL;
      GDK_DEBUG (DND, "From mime-type %s to UTI public.file-url and public.url", mime_type);
      return [NSPasteboardTypeFileURL retain];
    }
  else if (g_strcmp0 (mime_type, "application/x-color") == 0)
    {
      GDK_DEBUG (DND, "From mime-type %s to UTI %s", mime_type, [NSPasteboardTypeColor UTF8String]);
      return [NSPasteboardTypeColor retain];
    }
  else if ((uti_str = g_content_type_from_mime_type (mime_type)))
    {
      NSString *uti = [[NSString alloc] initWithUTF8String:uti_str];
      GDK_DEBUG (DND, "From mime-type %s to UTI %s", mime_type, uti_str);
      g_free (uti_str);

      return uti;
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
  NSString *uti;

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

      if ([[pasteboard types] containsObject:NSPasteboardTypeFileURL])
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
  else if ((uti = _gdk_macos_pasteboard_to_ns_type (mime_type, NULL)))
    {
      NSData *data = [pasteboard dataForType:uti];

      if (data != NULL)
        {
          const guint8 *bytes = [data bytes];
          gsize len = [data length];

          stream = g_memory_input_stream_new_from_data (g_memdup2 (bytes, len), len, g_free);
        }

      [uti release];
    }
  else
    {
      GDK_DEBUG (DND, "No stream for mime-type %s", mime_type);
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
  // TODO: how can GTK tell us what drag types expected?
  // Now the app will accept everything.
  [window registerForDraggedTypes:[NSArray arrayWithObjects:@"public.item",
                                                            @"org.gtk.internal",
                                                            nil]];
}

@implementation GdkMacosPasteboardItemDataProvider

-(id)initForClipboard:(GdkClipboard*)clipboard withContentProvider:(GdkContentProvider*)contentProvider
{
  [super init];
  g_set_object (&self->_clipboard, clipboard);
  g_set_object (&self->_contentProvider, contentProvider);
  return self;
}

-(id)initForDrag:(GdkDrag*)drag withContentProvider:(GdkContentProvider*)contentProvider
{
  [super init];
  g_set_object (&self->_drag, drag);
  g_set_object (&self->_contentProvider, contentProvider);
  return self;
}

-(void)dealloc
{
  g_clear_object (&self->_contentProvider);
  g_clear_object (&self->_clipboard);
  g_clear_object (&self->_drag);
  [super dealloc];
}

-(NSArray<NSPasteboardType> *)types
{
  NSMutableArray *ret = [[NSMutableArray alloc] init];
  GdkContentFormats *serializable;
  const char * const *mime_types;
  gsize n_mime_types;

  serializable = gdk_content_provider_ref_storable_formats (self->_contentProvider);
  serializable = gdk_content_formats_union_serialize_mime_types (serializable);
  mime_types = gdk_content_formats_get_mime_types (serializable, &n_mime_types);

  for (gsize i = 0; i < n_mime_types; i++)
    {
      const char *mime_type = mime_types[i];
      NSPasteboardType type;
      NSPasteboardType alternate = nil;

      if ((type = _gdk_macos_pasteboard_to_ns_type (mime_type, &alternate)))
        {
          [ret addObject:type];
          [type release];
          if (alternate)
            [ret addObject:alternate];
        }
    }

  gdk_content_formats_unref (serializable);

  /* Support internal, GType-based DnD. */
  if ([ret count] == 0)
    {
      GdkContentFormats *formats;
      gsize n_gtypes;

      formats = gdk_content_provider_ref_formats (self->_contentProvider);
      gdk_content_formats_get_gtypes (formats, &n_gtypes);

      if (n_gtypes)
        [ret addObject:@"org.gtk.internal"];

      gdk_content_formats_unref (formats);
    }

  return g_steal_pointer (&ret);
}

typedef struct
{
  GMemoryOutputStream *stream;
  NSPasteboardItem    *item;
  NSPasteboardType     type;
  GMainContext        *main_context;
  guint                done : 1;
} WriteRequest;

static void
write_request_free (WriteRequest *wr)
{
  g_clear_pointer (&wr->main_context, g_main_context_unref);
  g_clear_object (&wr->stream);
  [wr->item release];
  g_free (wr);
}

static void
on_data_ready_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  WriteRequest *wr = user_data;
  GError *error = NULL;
  NSData *data = nil;
  gboolean ret;

  g_assert (G_IS_OBJECT (object));
  g_assert (GDK_IS_CLIPBOARD (object) || GDK_IS_DRAG (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (wr != NULL);
  g_assert (G_IS_MEMORY_OUTPUT_STREAM (wr->stream));
  g_assert ([wr->item isKindOfClass:[NSPasteboardItem class]]);

  if (GDK_IS_CLIPBOARD (object))
    ret = gdk_clipboard_write_finish (GDK_CLIPBOARD (object), result, &error);
  else if (GDK_IS_DRAG (object))
    ret = gdk_drag_write_finish (GDK_DRAG (object), result, &error);
  else
    g_return_if_reached ();

  if (ret)
    {
      gsize size;

      g_output_stream_close (G_OUTPUT_STREAM (wr->stream), NULL, NULL);

      size = g_memory_output_stream_get_data_size (wr->stream);

      if (size == 8 && [wr->type isEqualToString:NSPasteboardTypeColor])
        {
          guint16 *color;
          NSError *nserror = nil;

          color = (guint16 *)g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (wr->stream));
          NSColor *nscolor = [[NSColor colorWithRed:color[0] / 65535.0
                                              green:color[1] / 65535.0
                                               blue:color[2] / 65535.0
                                              alpha:color[3] / 65535.0]
                              colorUsingColorSpace:[NSColorSpace genericRGBColorSpace]];
          data = [NSKeyedArchiver archivedDataWithRootObject:nscolor requiringSecureCoding:YES error: &nserror];
          if (error)
            g_warning ("Encoding color failed");
        }
      else
        {
          gpointer bytes = g_memory_output_stream_steal_data (wr->stream);
          data = [[NSData alloc] initWithBytesNoCopy:bytes
                                              length:size
                                        deallocator:^(void *alloc, NSUInteger length) { g_free (alloc); }];
        }
    }
  else
    {
      g_warning ("Failed to serialize pasteboard contents: %s",
                 error->message);
      g_clear_error (&error);
    }

  [wr->item setData:data forType:wr->type];

  wr->done = TRUE;

  GDK_END_MACOS_ALLOC_POOL;
}

-(void)pasteboard:(NSPasteboard *)pasteboard item:(NSPasteboardItem *)item provideDataForType:(NSPasteboardType)type
{
  const char *mime_type = _gdk_macos_pasteboard_from_ns_type (type);
  GMainContext *main_context = g_main_context_default ();
  WriteRequest *wr;

  if (self->_contentProvider == NULL || mime_type == NULL)
    {
      [item setData:[NSData data] forType:type];
      return;
    }

  wr = g_new0 (WriteRequest, 1);
  wr->item = [item retain];
  wr->stream = G_MEMORY_OUTPUT_STREAM (g_memory_output_stream_new_resizable ());
  wr->type = type;
  wr->main_context = g_main_context_ref (main_context);
  wr->done = FALSE;

  if (GDK_IS_CLIPBOARD (self->_clipboard))
    gdk_clipboard_write_async (self->_clipboard,
                               mime_type,
                               G_OUTPUT_STREAM (wr->stream),
                               G_PRIORITY_DEFAULT,
                               NULL,
                               on_data_ready_cb,
                               wr);
  else if (GDK_IS_DRAG (self->_drag))
    gdk_drag_write_async (self->_drag,
                          mime_type,
                          G_OUTPUT_STREAM (wr->stream),
                          G_PRIORITY_DEFAULT,
                          NULL,
                          on_data_ready_cb,
                          wr);
  else
    g_return_if_reached ();

  /* We're forced to provide data synchronously via this API
   * so we must block on the main loop. Using another main loop
   * than the default tends to get us locked up here, so that is
   * what we'll do for now.
   */
  while (!wr->done)
    g_main_context_iteration (wr->main_context, TRUE);

  write_request_free (wr);
}

-(void)pasteboardFinishedWithDataProvider:(NSPasteboard *)pasteboard
{
  g_clear_object (&self->_clipboard);
  g_clear_object (&self->_drag);
  g_clear_object (&self->_contentProvider);
}

@end

@implementation GdkMacosPasteboardItem

-(id)initForClipboard:(GdkClipboard*)clipboard withContentProvider:(GdkContentProvider*)contentProvider
{
  GdkMacosPasteboardItemDataProvider *dataProvider;

  dataProvider = [[GdkMacosPasteboardItemDataProvider alloc] initForClipboard:clipboard withContentProvider:contentProvider];

  [super init];
  g_set_object (&self->_clipboard, clipboard);
  g_set_object (&self->_contentProvider, contentProvider);
  [self setDataProvider:dataProvider forTypes:[dataProvider types]];

  [dataProvider release];

  return self;
}

-(id)initForDrag:(GdkDrag*)drag withContentProvider:(GdkContentProvider*)contentProvider
{
  GdkMacosPasteboardItemDataProvider *dataProvider;

  dataProvider = [[GdkMacosPasteboardItemDataProvider alloc] initForDrag:drag withContentProvider:contentProvider];

  [super init];
  g_set_object (&self->_drag, drag);
  g_set_object (&self->_contentProvider, contentProvider);
  [self setDataProvider:dataProvider forTypes:[dataProvider types]];

  [dataProvider release];

  return self;
}

-(void)dealloc
{
  g_clear_object (&self->_contentProvider);
  g_clear_object (&self->_clipboard);
  g_clear_object (&self->_drag);
  [super dealloc];
}

-(NSRect)draggingFrame
{
  return self->_draggingFrame;
}

-(void)setDraggingFrame:(NSRect)draggingFrame;
{
  self->_draggingFrame = draggingFrame;
}

-(id)item
{
  return self;
}

-(NSArray* (^) (void))imageComponentsProvider
{
  return nil;
}

@end
