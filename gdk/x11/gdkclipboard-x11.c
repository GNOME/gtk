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

#include "gdkintl.h"
#include "gdkprivate-x11.h"
#include "gdkselectioninputstream-x11.h"
#include "gdktextlistconverter-x11.h"

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

static GInputStream * 
text_list_convert (GdkX11Clipboard *cb,
                   GInputStream    *stream,
                   const char      *encoding,
                   int              format)
{
  GInputStream *converter_stream;
  GConverter *converter;

  converter = gdk_x11_text_list_converter_to_utf8_new (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)),
                                                       encoding,
                                                       format);
  converter_stream = g_converter_input_stream_new (stream, converter);

  g_object_unref (converter);
  g_object_unref (stream);

  return converter_stream;
}

static GInputStream * 
no_convert (GdkX11Clipboard *cb,
            GInputStream    *stream,
            const char      *encoding,
            int              format)
{
  return stream;
}

static const struct {
  const char *x_target;
  const char *mime_type;
  GInputStream * (* convert) (GdkX11Clipboard *, GInputStream *, const char *, int);
} special_targets[] = {
  { "UTF8_STRING", "text/plain;charset=utf-8",   no_convert },
  { "COMPOUND_TEXT", "text/plain;charset=utf-8", text_list_convert },
  { "TEXT", "text/plain;charset=utf-8",          text_list_convert },
  { "STRING", "text/plain;charset=utf-8",        text_list_convert }
};

static void
print_atoms (GdkX11Clipboard *cb,
             const char      *prefix,
             const Atom      *atoms,
             gsize            n_atoms)
{
  GDK_NOTE(CLIPBOARD,
           GdkDisplay *display = gdk_clipboard_get_display (GDK_CLIPBOARD (cb));
           gsize i;
            
           g_printerr ("%s: %s [ ", cb->selection, prefix);
           for (i = 0; i < n_atoms; i++)
             {
               g_printerr ("%s%s", i > 0 ? ", " : "", gdk_x11_get_xatom_name_for_display (display , atoms[i]));
             }
           g_printerr (" ]\n");
          ); 
}

static GSList *
gdk_x11_clipboard_formats_to_targets (GdkContentFormats *formats)
{
  GSList *targets;
  const char * const *mime_types;
  gsize i, j, n_mime_types;

  targets = NULL;
  mime_types = gdk_content_formats_get_mime_types (formats, &n_mime_types);

  for (i = 0; i < n_mime_types; i++)
    {
      for (j = 0; j < G_N_ELEMENTS (special_targets); j++)
        {
          if (g_str_equal (mime_types[i], special_targets[j].mime_type))
            targets = g_slist_prepend (targets, (gpointer) g_intern_string (special_targets[i].x_target));
        }
      targets = g_slist_prepend (targets, (gpointer) mime_types[i]);
    }

  return g_slist_reverse (targets);
}

static GdkContentFormats *
gdk_x11_clipboard_formats_from_atoms (GdkDisplay *display,
                                      const Atom *atoms,
                                      gsize       n_atoms)
{
  GdkContentFormatsBuilder *builder;
  gsize i, j;

  builder = gdk_content_formats_builder_new ();
  for (i = 0; i < n_atoms; i++)
    {
      const char *name;

      name = gdk_x11_get_xatom_name_for_display (display , atoms[i]);
      if (strchr (name, '/'))
        {
          gdk_content_formats_builder_add_mime_type (builder, name);
          continue;
        }

      for (j = 0; j < G_N_ELEMENTS (special_targets); j++)
        {
          if (g_str_equal (name, special_targets[j].x_target))
            {
              gdk_content_formats_builder_add_mime_type (builder, special_targets[j].mime_type);
              break;
            }
        }
    }

  return gdk_content_formats_builder_free (builder);
}

static void
gdk_x11_clipboard_request_targets_finish (GObject      *source_object,
                                          GAsyncResult *res,
                                          gpointer      user_data)
{
  GInputStream *stream = G_INPUT_STREAM (source_object);
  GdkX11Clipboard *cb = user_data;
  GdkDisplay *display;
  GdkContentFormats *formats;
  GBytes *bytes;
  GError *error = NULL;

  bytes = g_input_stream_read_bytes_finish (stream, res, &error);
  if (bytes == NULL)
    {
      g_error_free (error);
      g_object_unref (stream);
      g_object_unref (cb);
      return;
    }
  else if (g_bytes_get_size (bytes) == 0)
    {
      g_bytes_unref (bytes);
      g_object_unref (stream);
      g_object_unref (cb);
      return;
    }

  print_atoms (cb,
               "received targets",
               g_bytes_get_data (bytes, NULL),
               g_bytes_get_size (bytes) / sizeof (Atom));

  display = gdk_clipboard_get_display (GDK_CLIPBOARD (cb));
  formats = gdk_x11_clipboard_formats_from_atoms (display,
                                                  g_bytes_get_data (bytes, NULL),
                                                  g_bytes_get_size (bytes) / sizeof (Atom));
  GDK_NOTE(CLIPBOARD, char *s = gdk_content_formats_to_string (formats); g_printerr ("%s: got formats: %s\n", cb->selection, s); g_free (s));

  /* union with previously loaded formats */
  gdk_clipboard_claim_remote (GDK_CLIPBOARD (cb), formats);
  gdk_content_formats_unref (formats);

  g_input_stream_read_bytes_async (stream,
                                   gdk_x11_display_get_max_request_size (display),
                                   G_PRIORITY_DEFAULT,
                                   NULL,
                                   gdk_x11_clipboard_request_targets_finish,
                                   cb);
}

static void
gdk_x11_clipboard_request_targets_got_stream (GObject      *source,
                                              GAsyncResult *result,
                                              gpointer      data)
{
  GdkX11Clipboard *cb = data;
  GInputStream *stream;
  GdkDisplay *display;
  GError *error = NULL;
  const char *type;
  int format;

  stream = gdk_x11_selection_input_stream_new_finish (result, &type, &format, &error);
  if (stream == NULL)
    {
      g_object_unref (cb);
      return;
    }
  else if (!g_str_equal (type, "ATOM") || format != 32)
    {
      g_input_stream_close (stream, NULL, NULL);
      g_object_unref (stream);
      g_object_unref (cb);
      return;
    }

  display = gdk_clipboard_get_display (GDK_CLIPBOARD (cb));

  g_input_stream_read_bytes_async (stream,
                                   gdk_x11_display_get_max_request_size (display),
                                   G_PRIORITY_DEFAULT,
                                   NULL,
                                   gdk_x11_clipboard_request_targets_finish,
                                   cb);
}

static void
gdk_x11_clipboard_request_targets (GdkX11Clipboard *cb)
{
  gdk_x11_selection_input_stream_new_async (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)),
                                            cb->selection,
                                            "TARGETS",
                                            cb->timestamp,
                                            G_PRIORITY_DEFAULT,
                                            NULL,
                                            gdk_x11_clipboard_request_targets_got_stream,
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
gdk_x11_clipboard_read_got_stream (GObject      *source,
                                   GAsyncResult *res,
                                   gpointer      data)
{
  GTask *task = data;
  GError *error = NULL;
  GInputStream *stream;
  const char *type;
  int format;
  
  stream = gdk_x11_selection_input_stream_new_finish (res, &type, &format, &error);
  if (stream == NULL)
    {
      GSList *targets, *next;
      
      targets = g_task_get_task_data (task);
      next = targets->next;
      if (next)
        {
          GdkX11Clipboard *cb = GDK_X11_CLIPBOARD (g_task_get_source_object (task));

          GDK_NOTE(CLIPBOARD, g_printerr ("%s: reading %s failed, trying %s next\n",
                                          cb->selection, (char *) targets->data, (char *) next->data));
          targets->next = NULL;
          g_task_set_task_data (task, next, (GDestroyNotify) g_slist_free);
          gdk_x11_selection_input_stream_new_async (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)),
                                                    cb->selection,
                                                    next->data,
                                                    cb->timestamp,
                                                    g_task_get_priority (task),
                                                    g_task_get_cancellable (task),
                                                    gdk_x11_clipboard_read_got_stream,
                                                    task);
          g_error_free (error);
          return;
        }

      g_task_return_error (task, error);
    }
  else
    {
      GdkX11Clipboard *cb = GDK_X11_CLIPBOARD (g_task_get_source_object (task));
      const char *mime_type = ((GSList *) g_task_get_task_data (task))->data;
      gsize i;

      for (i = 0; i < G_N_ELEMENTS (special_targets); i++)
        {
          if (g_str_equal (mime_type, special_targets[i].x_target))
            {
              GDK_NOTE(CLIPBOARD, g_printerr ("%s: reading with converter from %s to %s\n",
                                              cb->selection, mime_type, special_targets[i].mime_type));
              mime_type = g_intern_string (special_targets[i].mime_type);
              g_task_set_task_data (task, g_slist_prepend (NULL, (gpointer) mime_type), (GDestroyNotify) g_slist_free);
              stream = special_targets[i].convert (cb, stream, type, format);
              break;
            }
        }

      GDK_NOTE(CLIPBOARD, g_printerr ("%s: reading clipboard as %s now\n",
                                      cb->selection, mime_type));
      g_task_return_pointer (task, stream, g_object_unref);
    }

  g_object_unref (task);
}

static void
gdk_x11_clipboard_read_async (GdkClipboard        *clipboard,
                              GdkContentFormats   *formats,
                              int                  io_priority,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  GdkX11Clipboard *cb = GDK_X11_CLIPBOARD (clipboard);
  GSList *targets;
  GTask *task;

  task = g_task_new (clipboard, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_x11_clipboard_read_async);

  targets = gdk_x11_clipboard_formats_to_targets (formats);
  g_task_set_task_data (task, targets, (GDestroyNotify) g_slist_free);
  if (targets == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("No compatible transfer format found"));
      return;
    }

  GDK_NOTE(CLIPBOARD, g_printerr ("%s: new read for %s (%u other options)\n",
                                  cb->selection, (char *) targets->data, g_slist_length (targets->next)));
  gdk_x11_selection_input_stream_new_async (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)),
                                            cb->selection,
                                            targets->data,
                                            cb->timestamp,
                                            io_priority,
                                            cancellable,
                                            gdk_x11_clipboard_read_got_stream,
                                            task);
}

static GInputStream *
gdk_x11_clipboard_read_finish (GdkClipboard  *clipboard,
                               const char   **out_mime_type,
                               GAsyncResult  *result,
                               GError       **error)
{
  GInputStream *stream;
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (result, G_OBJECT (clipboard)), NULL);
  task = G_TASK (result);
  g_return_val_if_fail (g_task_get_source_tag (task) == gdk_x11_clipboard_read_async, NULL);

  stream = g_task_propagate_pointer (task, error);

  if (stream)
    {
      if (out_mime_type)
        {
          GSList *targets;

          targets = g_task_get_task_data (task);
          *out_mime_type = targets->data;
        }
      g_object_ref (stream);
    }
  else
    {
      if (out_mime_type)
        *out_mime_type = NULL;
    }

  return stream;
}

static void
gdk_x11_clipboard_class_init (GdkX11ClipboardClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkClipboardClass *clipboard_class = GDK_CLIPBOARD_CLASS (class);

  object_class->finalize = gdk_x11_clipboard_finalize;

  clipboard_class->read_async = gdk_x11_clipboard_read_async;
  clipboard_class->read_finish = gdk_x11_clipboard_read_finish;
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

