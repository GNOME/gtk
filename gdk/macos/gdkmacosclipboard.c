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
#include "gdkmacospasteboard-private.h"
#include "gdkmacosutils-private.h"
#include "gdkprivate.h"

struct _GdkMacosClipboard
{
  GdkClipboard  parent_instance;
  NSPasteboard *pasteboard;
  NSInteger     last_change_count;
};

typedef struct
{
  GMemoryOutputStream *stream;
  NSPasteboardItem    *item;
  NSPasteboardType     type;
  GMainContext        *main_context;
  guint                done : 1;
} WriteRequest;

G_DEFINE_TYPE (GdkMacosClipboard, _gdk_macos_clipboard, GDK_TYPE_CLIPBOARD)

static void
write_request_free (WriteRequest *wr)
{
  g_clear_pointer (&wr->main_context, g_main_context_unref);
  g_clear_object (&wr->stream);
  [wr->item release];
  g_slice_free (WriteRequest, wr);
}

static void
_gdk_macos_clipboard_load_contents (GdkMacosClipboard *self)
{
  GdkContentFormats *formats;
  NSInteger change_count;

  g_assert (GDK_IS_MACOS_CLIPBOARD (self));

  change_count = [self->pasteboard changeCount];

  formats = _gdk_macos_pasteboard_load_formats (self->pasteboard);
  gdk_clipboard_claim_remote (GDK_CLIPBOARD (self), formats);
  gdk_content_formats_unref (formats);

  self->last_change_count = change_count;
}

static void
_gdk_macos_clipboard_read_async (GdkClipboard        *clipboard,
                                 GdkContentFormats   *formats,
                                 int                  io_priority,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  _gdk_macos_pasteboard_read_async (G_OBJECT (clipboard),
                                    GDK_MACOS_CLIPBOARD (clipboard)->pasteboard,
                                    formats,
                                    io_priority,
                                    cancellable,
                                    callback,
                                    user_data);
}

static GInputStream *
_gdk_macos_clipboard_read_finish (GdkClipboard  *clipboard,
                                  GAsyncResult  *result,
                                  const char   **out_mime_type,
                                  GError       **error)
{
  return _gdk_macos_pasteboard_read_finish (G_OBJECT (clipboard), result, out_mime_type, error);
}

static void
_gdk_macos_clipboard_send_to_pasteboard (GdkMacosClipboard  *self,
                                         GdkContentProvider *content)
{
  GdkMacosClipboardDataProvider *dataProvider;
  GdkContentFormats *serializable;
  const char * const *mime_types;
  gsize n_mime_types;

  g_assert (GDK_IS_MACOS_CLIPBOARD (self));
  g_assert (GDK_IS_CONTENT_PROVIDER (content));

  serializable = gdk_content_provider_ref_storable_formats (content);
  serializable = gdk_content_formats_union_serialize_mime_types (serializable);
  mime_types = gdk_content_formats_get_mime_types (serializable, &n_mime_types);

  dataProvider = [[GdkMacosClipboardDataProvider alloc] initClipboard:self mimetypes:mime_types];
  _gdk_macos_pasteboard_send_content (self->pasteboard, content, dataProvider);
  [dataProvider release];

  self->last_change_count = [self->pasteboard changeCount];

  g_clear_pointer (&serializable, gdk_content_formats_unref);
}

static gboolean
_gdk_macos_clipboard_claim (GdkClipboard       *clipboard,
                            GdkContentFormats  *formats,
                            gboolean            local,
                            GdkContentProvider *provider)
{
  GdkMacosClipboard *self = (GdkMacosClipboard *)clipboard;
  gboolean ret;

  g_assert (GDK_IS_CLIPBOARD (clipboard));
  g_assert (formats != NULL);
  g_assert (!provider || GDK_IS_CONTENT_PROVIDER (provider));

  ret = GDK_CLIPBOARD_CLASS (_gdk_macos_clipboard_parent_class)->claim (clipboard, formats, local, provider);

  if (local)
    _gdk_macos_clipboard_send_to_pasteboard (self, provider);

  return ret;
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

@implementation GdkMacosClipboardDataProvider

-(id)initClipboard:(GdkMacosClipboard *)gdkClipboard mimetypes:(const char * const *)mime_types;
{
  [super initPasteboard:gdkClipboard->pasteboard mimetypes:mime_types];

  self->clipboard = g_object_ref (GDK_CLIPBOARD (gdkClipboard));

  return self;
}

-(void)dealloc
{
  g_clear_object (&self->clipboard);

  [super dealloc];
}

-(void)pasteboardFinishedWithDataProvider:(NSPasteboard *)pasteboard
{
  g_clear_object (&self->clipboard);
}

static void
on_data_ready_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  GdkClipboard *clipboard = (GdkClipboard *)object;
  WriteRequest *wr = user_data;
  GError *error = NULL;
  NSData *data = nil;

  g_assert (GDK_IS_CLIPBOARD (clipboard));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (wr != NULL);
  g_assert (G_IS_MEMORY_OUTPUT_STREAM (wr->stream));
  g_assert ([wr->item isKindOfClass:[NSPasteboardItem class]]);

  if (gdk_clipboard_write_finish (clipboard, result, &error))
    {
      gsize size;
      gpointer bytes;

      g_output_stream_close (G_OUTPUT_STREAM (wr->stream), NULL, NULL);

      size = g_memory_output_stream_get_data_size (wr->stream);
      bytes = g_memory_output_stream_steal_data (wr->stream);
      data = [[NSData alloc] initWithBytesNoCopy:bytes
                                          length:size
                                     deallocator:^(void *alloc, NSUInteger length) { g_free (alloc); }];
    }
  else
    {
      g_warning ("Failed to serialize clipboard contents: %s",
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

  if (self->clipboard == NULL || mime_type == NULL)
    {
      [item setData:[NSData data] forType:type];
      return;
    }

  wr = g_slice_new0 (WriteRequest);
  wr->item = [item retain];
  wr->stream = G_MEMORY_OUTPUT_STREAM (g_memory_output_stream_new_resizable ());
  wr->type = type;
  wr->main_context = g_main_context_ref (main_context);
  wr->done = FALSE;

  gdk_clipboard_write_async (self->clipboard,
                             mime_type,
                             G_OUTPUT_STREAM (wr->stream),
                             G_PRIORITY_DEFAULT,
                             self->cancellable,
                             on_data_ready_cb,
                             wr);

  /* We're forced to provide data synchronously via this API
   * so we must block on the main loop. Using another main loop
   * than the default tends to get us locked up here, so that is
   * what we'll do for now.
   */
  while (!wr->done)
    g_main_context_iteration (wr->main_context, TRUE);

  write_request_free (wr);
}

@end
