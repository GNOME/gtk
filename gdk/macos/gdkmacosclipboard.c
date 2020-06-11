/*
 * Copyright © 2020 Red Hat, Inc.
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

#include "gdkmacosclipboard-private.h"
#include "gdkmacosutils-private.h"

struct _GdkMacosClipboard
{
  GdkClipboard            parent_instance;
  NSPasteboard           *pasteboard;
  GdkMacosClipboardOwner *owner;
  NSInteger               last_change_count;
};

G_DEFINE_TYPE (GdkMacosClipboard, _gdk_macos_clipboard, GDK_TYPE_CLIPBOARD)

static void
populate_content_formats (GdkContentFormatsBuilder *builder,
                          NSPasteboardType          type)
{
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if ([type isEqualToString:NSPasteboardTypeString] ||
      [type isEqualToString:NSStringPboardType])
    gdk_content_formats_builder_add_mime_type (builder, "text/plain;charset=utf-8");
  else if ([type isEqualToString:NSPasteboardTypeURL] ||
           [type isEqualToString:NSPasteboardTypeFileURL])
    gdk_content_formats_builder_add_mime_type (builder, "text/uri-list");
  else if ([type isEqualToString:NSPasteboardTypeColor])
    gdk_content_formats_builder_add_mime_type (builder, "application/x-color");
  else if ([type isEqualToString:NSPasteboardTypeTIFF])
    gdk_content_formats_builder_add_mime_type (builder, "image/tiff");
  else if ([type isEqualToString:NSPasteboardTypePNG])
    gdk_content_formats_builder_add_mime_type (builder, "image/png");

  G_GNUC_END_IGNORE_DEPRECATIONS;
}

static GdkContentFormats *
load_offer_formats (GdkMacosClipboard *self)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  GdkContentFormatsBuilder *builder;
  GdkContentFormats *formats;

  g_assert (GDK_IS_MACOS_CLIPBOARD (self));

  builder = gdk_content_formats_builder_new ();
  for (NSPasteboardType type in [self->pasteboard types])
    populate_content_formats (builder, type);
  formats = gdk_content_formats_builder_free_to_formats (builder);

  GDK_END_MACOS_ALLOC_POOL;

  return g_steal_pointer (&formats);
}

static void
_gdk_macos_clipboard_load_contents (GdkMacosClipboard *self)
{
  GdkContentFormats *formats;
  NSInteger change_count;

  g_assert (GDK_IS_MACOS_CLIPBOARD (self));

  change_count = [self->pasteboard changeCount];

  formats = load_offer_formats (self);
  gdk_clipboard_claim_remote (GDK_CLIPBOARD (self), formats);
  gdk_content_formats_unref (formats);

  self->last_change_count = change_count;
}

static gboolean
_gdk_macos_clipboard_claim (GdkClipboard       *clipboard,
                            GdkContentFormats  *formats,
                            gboolean            local,
                            GdkContentProvider *provider)
{
  g_assert (GDK_IS_CLIPBOARD (clipboard));
  g_assert (formats != NULL);
  g_assert (!provider || GDK_IS_CONTENT_PROVIDER (provider));

  return GDK_CLIPBOARD_CLASS (_gdk_macos_clipboard_parent_class)->claim (clipboard, formats, local, provider);
}

static GInputStream *
create_stream_from_nsdata (NSData *data)
{
  const guint8 *bytes = [data bytes];
  gsize len = [data length];

  return g_memory_input_stream_new_from_data (g_memdup (bytes, len), len, g_free);
}

static void
_gdk_macos_clipboard_read_async (GdkClipboard        *clipboard,
                                 GdkContentFormats   *formats,
                                 int                  io_priority,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  GdkMacosClipboard *self = (GdkMacosClipboard *)clipboard;
  GdkContentFormats *offer_formats = NULL;
  NSPasteboard *pasteboard;
  const gchar *mime_type;
  GInputStream *stream = NULL;
  GTask *task = NULL;

  g_assert (GDK_IS_MACOS_CLIPBOARD (self));
  g_assert (formats != NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, _gdk_macos_clipboard_read_async);
  g_task_set_priority (task, io_priority);

  pasteboard = [NSPasteboard generalPasteboard];
  offer_formats = load_offer_formats (GDK_MACOS_CLIPBOARD (clipboard));
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
                                                        strlen (str),
                                                        g_free);
        }
    }
  else if (strcmp (mime_type, "text/uri-list") == 0)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

      if ([[self->pasteboard types] containsObject:NSPasteboardTypeFileURL])
        {
          GString *str = g_string_new (NULL);
          NSArray *files = [pasteboard propertyListForType:NSFilenamesPboardType];
          gsize n_files = [files count];
          gchar *data;
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

      stream = g_memory_input_stream_new_from_data (g_memdup (&color, sizeof color),
                                                    sizeof color,
                                                    g_free);
    }
  else if (strcmp (mime_type, "image/tiff") == 0)
    {
      NSData *data = [pasteboard dataForType:NSPasteboardTypeTIFF];
      stream = create_stream_from_nsdata (data);
    }
  else if (strcmp (mime_type, "image/png") == 0)
    {
      NSData *data = [pasteboard dataForType:NSPasteboardTypePNG];
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

static GInputStream *
_gdk_macos_clipboard_read_finish (GdkClipboard  *clipboard,
                                  GAsyncResult  *result,
                                  const gchar  **out_mime_type,
                                  GError       **error)
{
  GTask *task = (GTask *)result;

  g_assert (GDK_IS_MACOS_CLIPBOARD (clipboard));
  g_assert (G_IS_TASK (task));

  if (out_mime_type != NULL)
    *out_mime_type = g_strdup (g_task_get_task_data (task));

  return g_task_propagate_pointer (task, error);
}

static void
_gdk_macos_clipboard_constructed (GObject *object)
{
  GdkMacosClipboard *self = (GdkMacosClipboard *)object;

  if (self->pasteboard == nil)
    self->pasteboard = [[NSPasteboard generalPasteboard] retain];

  G_OBJECT_CLASS (_gdk_macos_clipboard_parent_class)->constructed (object);
}

static void
_gdk_macos_clipboard_finalize (GObject *object)
{
  GdkMacosClipboard *self = (GdkMacosClipboard *)object;

  if (self->pasteboard != nil)
    {
      [self->pasteboard release];
      self->pasteboard = nil;
    }

  G_OBJECT_CLASS (_gdk_macos_clipboard_parent_class)->finalize (object);
}

static void
_gdk_macos_clipboard_class_init (GdkMacosClipboardClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkClipboardClass *clipboard_class = GDK_CLIPBOARD_CLASS (klass);

  object_class->constructed = _gdk_macos_clipboard_constructed;
  object_class->finalize = _gdk_macos_clipboard_finalize;

  clipboard_class->claim = _gdk_macos_clipboard_claim;
  clipboard_class->read_async = _gdk_macos_clipboard_read_async;
  clipboard_class->read_finish = _gdk_macos_clipboard_read_finish;
}

static void
_gdk_macos_clipboard_init (GdkMacosClipboard *self)
{
}

GdkClipboard *
_gdk_macos_clipboard_new (GdkMacosDisplay *display)
{
  GdkMacosClipboard *self;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);

  self = g_object_new (GDK_TYPE_MACOS_CLIPBOARD,
                       "display", display,
                       NULL);

  _gdk_macos_clipboard_load_contents (self);

  return GDK_CLIPBOARD (self);
}

void
_gdk_macos_clipboard_check_externally_modified (GdkMacosClipboard *self)
{
  g_return_if_fail (GDK_IS_MACOS_CLIPBOARD (self));

  if ([self->pasteboard changeCount] != self->last_change_count)
    _gdk_macos_clipboard_load_contents (self);
}

@implementation GdkMacosClipboardOwner

-(void)pasteboard:(NSPasteboard *)pb provideDataForType:(NSString *)type
{
  //[pb setData:data forType:type];
}

@end
