/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2017 Red Hat, Inc.
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

#include "gdkclipboardprivate.h"
#include "gdkclipboard-wayland.h"


typedef struct _GdkWaylandClipboardClass GdkWaylandClipboardClass;

struct _GdkWaylandClipboard
{
  GdkClipboard parent;
};

struct _GdkWaylandClipboardClass
{
  GdkClipboardClass parent_class;
};

G_DEFINE_TYPE (GdkWaylandClipboard, gdk_wayland_clipboard, GDK_TYPE_CLIPBOARD)

static void
gdk_wayland_clipboard_finalize (GObject *object)
{
  //GdkWaylandClipboard *cb = GDK_WAYLAND_CLIPBOARD (object);

  G_OBJECT_CLASS (gdk_wayland_clipboard_parent_class)->finalize (object);
}

static void
gdk_wayland_clipboard_class_init (GdkWaylandClipboardClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  //GdkClipboardClass *clipboard_class = GDK_CLIPBOARD_CLASS (class);

  object_class->finalize = gdk_wayland_clipboard_finalize;

#if 0
  clipboard_class->claim = gdk_wayland_clipboard_claim;
  clipboard_class->store_async = gdk_wayland_clipboard_store_async;
  clipboard_class->store_finish = gdk_wayland_clipboard_store_finish;
  clipboard_class->read_async = gdk_wayland_clipboard_read_async;
  clipboard_class->read_finish = gdk_wayland_clipboard_read_finish;
#endif
}

static void
gdk_wayland_clipboard_init (GdkWaylandClipboard *cb)
{
}

GdkClipboard *
gdk_wayland_clipboard_new (GdkDisplay *display)
{
  GdkWaylandClipboard *cb;

  cb = g_object_new (GDK_TYPE_WAYLAND_CLIPBOARD,
                     "display", display,
                     NULL);

  return GDK_CLIPBOARD (cb);
}
