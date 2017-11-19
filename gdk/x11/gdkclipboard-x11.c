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
#include "gdkclipboard-x11.h"
#include "gdkselectioninputstream-x11.h"
#include "gdkprivate-x11.h"

#include <string.h>
#include <X11/Xatom.h>

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#define IDLE_ABORT_TIME 30 /* seconds */

typedef struct _GdkX11ClipboardClass GdkX11ClipboardClass;

typedef struct _RetrievalInfo RetrievalInfo;

struct _GdkX11Clipboard
{
  GdkClipboard parent;

  char       *selection;
  Atom        xselection;
  guint32     timestamp;
};

struct _GdkX11ClipboardClass
{
  GdkClipboardClass parent_class;
};

G_DEFINE_TYPE (GdkX11Clipboard, gdk_x11_clipboard, GDK_TYPE_CLIPBOARD)

#define SELECTION_MAX_SIZE(display)                                     \
  MIN(262144,                                                           \
      XExtendedMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) == 0     \
       ? XMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) - 100         \
       : XExtendedMaxRequestSize (GDK_DISPLAY_XDISPLAY (display)) - 100)

static void
gdk_x11_clipboard_request_targets_finish (GObject      *source_object,
                                          GAsyncResult *res,
                                          gpointer      user_data)
{
  GInputStream *stream = G_INPUT_STREAM (source_object);
  GdkX11Clipboard *cb = user_data;
  GdkDisplay *display;
  GdkContentFormats *formats;
  GdkContentFormatsBuilder *builder;
  GBytes *bytes;
  GError *error = NULL;
  const Atom *atoms;
  guint i, n_atoms;

  bytes = g_input_stream_read_bytes_finish (stream, res, &error);
  if (bytes == NULL || g_bytes_get_size (bytes) == 0)
    {
      if (bytes)
        g_bytes_unref (bytes);
      g_object_unref (stream);
      g_object_unref (cb);
      return;
    }

  display = gdk_clipboard_get_display (GDK_CLIPBOARD (cb));

  atoms = g_bytes_get_data (bytes, NULL);
  n_atoms = g_bytes_get_size (bytes) / sizeof (Atom);
  builder = gdk_content_formats_builder_new ();
  for (i = 0; i < n_atoms; i++)
    {
      gdk_content_formats_builder_add_mime_type (builder, gdk_x11_get_xatom_name_for_display (display , atoms[i]));
    }
  gdk_content_formats_builder_add_formats (builder, gdk_clipboard_get_formats (GDK_CLIPBOARD (cb)));
  formats = gdk_content_formats_builder_free (builder);
  GDK_NOTE(CLIPBOARD, char *s = gdk_content_formats_to_string (formats); g_printerr ("%s: got formats: %s\n", cb->selection, s); g_free (s));

  /* union with previously loaded formats */
  gdk_clipboard_claim_remote (GDK_CLIPBOARD (cb), formats);
  gdk_content_formats_unref (formats);

  g_input_stream_read_bytes_async (stream,
                                   SELECTION_MAX_SIZE (display),
                                   G_PRIORITY_DEFAULT,
                                   NULL,
                                   gdk_x11_clipboard_request_targets_finish,
                                   cb);
}

static void
gdk_x11_clipboard_request_targets (GdkX11Clipboard *cb)
{
  GInputStream *stream;
  GdkDisplay *display;
  
  display = gdk_clipboard_get_display (GDK_CLIPBOARD (cb));

  stream = gdk_x11_selection_input_stream_new (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)),
                                               cb->selection,
                                               "TARGETS",
                                               cb->timestamp);

  g_input_stream_read_bytes_async (stream,
                                   SELECTION_MAX_SIZE (display),
                                   G_PRIORITY_DEFAULT,
                                   NULL,
                                   gdk_x11_clipboard_request_targets_finish,
                                   g_object_ref (cb));
}

static GdkFilterReturn
gdk_x11_clipboard_filter_event (GdkXEvent *xev,
                                GdkEvent  *gdkevent,
                                gpointer   data)
{
  GdkX11Clipboard *cb = GDK_X11_CLIPBOARD (data);
  GdkDisplay *display;
  XEvent *xevent = xev;
  Window xwindow;

  display = gdk_clipboard_get_display (GDK_CLIPBOARD (cb));
  xwindow = GDK_X11_DISPLAY (display)->leader_window;

  if (xevent->xany.window != xwindow)
    return GDK_FILTER_CONTINUE;

  switch (xevent->type)
  {
    default:
#ifdef HAVE_XFIXES
      if (xevent->type - GDK_X11_DISPLAY (display)->xfixes_event_base == XFixesSelectionNotify)
	{
	  XFixesSelectionNotifyEvent *sn = (XFixesSelectionNotifyEvent *) xevent;

          if (sn->selection == cb->xselection)
            {
              GdkContentFormats *empty;
              
              GDK_NOTE(CLIPBOARD, g_printerr ("%s: got FixesSelectionNotify\n", cb->selection));
              empty = gdk_content_formats_new (NULL, 0);
              gdk_clipboard_claim_remote (GDK_CLIPBOARD (cb), empty);
              gdk_content_formats_unref (empty);
              cb->timestamp = sn->selection_timestamp;
              gdk_x11_clipboard_request_targets (cb);
            }
        }
#endif
      return GDK_FILTER_CONTINUE;
  }
}

static void
gdk_x11_clipboard_finalize (GObject *object)
{
  GdkX11Clipboard *cb = GDK_X11_CLIPBOARD (object);

  gdk_window_remove_filter (NULL, gdk_x11_clipboard_filter_event, cb);
  g_free (cb->selection);

  G_OBJECT_CLASS (gdk_x11_clipboard_parent_class)->finalize (object);
}

static void
gdk_x11_clipboard_class_init (GdkX11ClipboardClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  //GdkClipboardClass *clipboard_class = GDK_CLIPBOARD_CLASS (class);

  object_class->finalize = gdk_x11_clipboard_finalize;
}

static void
gdk_x11_clipboard_init (GdkX11Clipboard *cb)
{
  cb->timestamp = CurrentTime;
}

GdkClipboard *
gdk_x11_clipboard_new (GdkDisplay  *display,
                       const gchar *selection)
{
  GdkX11Clipboard *cb;

  cb = g_object_new (GDK_TYPE_X11_CLIPBOARD,
                     "display", display,
                     NULL);

  cb->selection = g_strdup (selection);
  cb->xselection = gdk_x11_get_xatom_by_name_for_display (display, selection);

  gdk_display_request_selection_notification (display, gdk_atom_intern (selection, FALSE));
  gdk_window_add_filter (NULL, gdk_x11_clipboard_filter_event, cb);
  gdk_x11_clipboard_request_targets (cb);

  return GDK_CLIPBOARD (cb);
}

