/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2017 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Benjamin Otte <otte@gnome.org>
 *         Christian Kellner <gicmo@gnome.org> 
 */

#include "config.h"

#include "gdkselectioninputstream-x11.h"

#include "gdkdisplay-x11.h"
#include "gdkintl.h"
#include "gdkx11display.h"
#include "gdkx11property.h"
#include "gdkx11window.h"

typedef struct GdkX11SelectionInputStreamPrivate  GdkX11SelectionInputStreamPrivate;

struct GdkX11SelectionInputStreamPrivate {
  GdkDisplay *display;
  GQueue chunks;
  char *selection;
  Atom xselection;
  char *target;
  Atom xtarget;
  char *property;
  Atom xproperty;

  GTask *pending_task;
  guchar *pending_data;
  gsize pending_size;

  guint complete : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (GdkX11SelectionInputStream, gdk_x11_selection_input_stream, G_TYPE_INPUT_STREAM);

static GdkFilterReturn
gdk_x11_selection_input_stream_filter_event (GdkXEvent *xevent,
                                             GdkEvent  *gdkevent,
                                             gpointer   data);

static gboolean
gdk_x11_selection_input_stream_has_data (GdkX11SelectionInputStream *stream)
{
  GdkX11SelectionInputStreamPrivate *priv = gdk_x11_selection_input_stream_get_instance_private (stream);

  return !g_queue_is_empty (&priv->chunks) || priv->complete;
}

static gssize
gdk_x11_selection_input_stream_fill_buffer (GdkX11SelectionInputStream *stream,
                                            guchar                     *buffer,
                                            gsize                       count)
{
  GdkX11SelectionInputStreamPrivate *priv = gdk_x11_selection_input_stream_get_instance_private (stream);
  gssize result = 0;

  while (!g_queue_is_empty (&priv->chunks) && count > 0)
  {
    GBytes *bytes = g_queue_pop_head (&priv->chunks);
    gsize size = g_bytes_get_size (bytes);

    if (g_bytes_get_size (bytes) > count)
      {
        GBytes *subbytes;
        if (buffer)
          memcpy (buffer, g_bytes_get_data (bytes, NULL), count);
        subbytes = g_bytes_new_from_bytes (bytes, count, size - count);
        g_queue_push_head (&priv->chunks, subbytes);
        size = count;
      }
    else
      {
        if (buffer)
          memcpy (buffer, g_bytes_get_data (bytes, NULL), size);
      }

    g_bytes_unref (bytes);
    result += size;
    if (buffer)
      buffer += size;
    count -= size;
  }

  return result;
}

static void
gdk_x11_selection_input_stream_flush (GdkX11SelectionInputStream *stream)
{
  GdkX11SelectionInputStreamPrivate *priv = gdk_x11_selection_input_stream_get_instance_private (stream);
  gssize written;

  if (!gdk_x11_selection_input_stream_has_data (stream))
    return;

  if (priv->pending_task == NULL)
    return;

  written = gdk_x11_selection_input_stream_fill_buffer (stream, priv->pending_data, priv->pending_size);
  g_task_return_int (priv->pending_task, written);

  priv->pending_task = NULL;
  priv->pending_data = NULL;
  priv->pending_size = 0;
}

static void
gdk_x11_selection_input_stream_complete (GdkX11SelectionInputStream *stream)
{
  GdkX11SelectionInputStreamPrivate *priv = gdk_x11_selection_input_stream_get_instance_private (stream);

  if (priv->complete)
    return;

  priv->complete = TRUE;

  gdk_x11_selection_input_stream_flush (stream);

  GDK_X11_DISPLAY (priv->display)->input_streams = g_slist_remove (GDK_X11_DISPLAY (priv->display)->input_streams, stream);
  gdk_window_remove_filter (NULL, gdk_x11_selection_input_stream_filter_event, stream);

  g_object_unref (stream);
}

static gssize
gdk_x11_selection_input_stream_read (GInputStream  *stream,
                                     void          *buffer,
                                     gsize          count,
                                     GCancellable  *cancellable,
                                     GError       **error)
{
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                       _("X11 Selections cannot be read synchronously"));

  return -1;
}

static gboolean
gdk_x11_selection_input_stream_close (GInputStream  *stream,
                                      GCancellable  *cancellable,
                                      GError       **error)
{
  return TRUE;
}

static void
gdk_x11_selection_input_stream_read_async (GInputStream        *input_stream,
                                           void                *buffer,
                                           gsize                count,
                                           int                  io_priority,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  GdkX11SelectionInputStream *stream = GDK_X11_SELECTION_INPUT_STREAM (input_stream);
  GdkX11SelectionInputStreamPrivate *priv = gdk_x11_selection_input_stream_get_instance_private (stream);
  GTask *task;
  
  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, gdk_x11_selection_input_stream_read_async);
  g_task_set_priority (task, io_priority);

  if (gdk_x11_selection_input_stream_has_data (stream))
    {
      gssize size;

      size = gdk_x11_selection_input_stream_fill_buffer (stream, buffer, count);
      g_task_return_int (task, size);
    }
  else
    {
      priv->pending_data = buffer;
      priv->pending_size = count;
      priv->pending_task = task;
    }
}

static gssize
gdk_x11_selection_input_stream_read_finish (GInputStream  *stream,
                                            GAsyncResult  *result,
                                            GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), -1);

  return g_task_propagate_int (G_TASK (result), error);
}

static void
gdk_x11_selection_input_stream_close_async (GInputStream        *stream,
                                            int                  io_priority,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  GTask *task;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, gdk_x11_selection_input_stream_close_async);
  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

static gboolean
gdk_x11_selection_input_stream_close_finish (GInputStream  *stream,
                                             GAsyncResult  *result,
                                             GError       **error)
{
  return TRUE;
}

static void
gdk_x11_selection_input_stream_finalize (GObject *object)
{
  GdkX11SelectionInputStream *stream = GDK_X11_SELECTION_INPUT_STREAM (object);
  GdkX11SelectionInputStreamPrivate *priv = gdk_x11_selection_input_stream_get_instance_private (stream);

  gdk_x11_selection_input_stream_complete (stream);

  g_queue_foreach (&priv->chunks, (GFunc) g_bytes_unref, NULL);
  g_queue_clear (&priv->chunks);

  g_free (priv->selection);
  g_free (priv->target);
  g_free (priv->property);

  G_OBJECT_CLASS (gdk_x11_selection_input_stream_parent_class)->finalize (object);
}

static void
gdk_x11_selection_input_stream_class_init (GdkX11SelectionInputStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GInputStreamClass *istream_class = G_INPUT_STREAM_CLASS (klass);

  object_class->finalize = gdk_x11_selection_input_stream_finalize;
  
  istream_class->read_fn = gdk_x11_selection_input_stream_read;
  istream_class->close_fn = gdk_x11_selection_input_stream_close;

  istream_class->read_async = gdk_x11_selection_input_stream_read_async;
  istream_class->read_finish = gdk_x11_selection_input_stream_read_finish;
  istream_class->close_async = gdk_x11_selection_input_stream_close_async;
  istream_class->close_finish = gdk_x11_selection_input_stream_close_finish;
}

static void
gdk_x11_selection_input_stream_init (GdkX11SelectionInputStream *stream)
{
}

static void
XFree_without_return_value (gpointer data)
{
  XFree (data);
}

static GBytes *
get_selection_property (Display  *display,
                        Window    owner,
                        Atom      property,
                        Atom     *ret_type,
                        gint     *ret_format)
{
  gulong nitems;
  gulong nbytes;
  Atom prop_type;
  gint prop_format;
  guchar *data = NULL;

  if (XGetWindowProperty (display, owner, property,
                          0, 0x1FFFFFFF, False,
                          AnyPropertyType, &prop_type, &prop_format,
                          &nitems, &nbytes, &data) != Success)
    goto err;

  if (prop_type != None)
    {
      gsize length;

      switch (prop_format)
        {
        case 8:
          length = nitems;
          break;
        case 16:
          length = sizeof (short) * nitems;
          break;
        case 32:
          length = sizeof (long) * nitems;
          break;
        default:
          g_warning ("Unknown XGetWindowProperty() format %u", prop_format);
          goto err;
        }

      *ret_type = prop_type;
      *ret_format = prop_format;

      return g_bytes_new_with_free_func (data,
                                         length,
                                         XFree_without_return_value,
                                         data);
    }

err:
  if (data)
    XFree (data);

  *ret_type = None;
  *ret_format = 0;

  return NULL;
}


static GdkFilterReturn
gdk_x11_selection_input_stream_filter_event (GdkXEvent *xev,
                                             GdkEvent  *gdkevent,
                                             gpointer   data)
{
  GdkX11SelectionInputStream *stream = GDK_X11_SELECTION_INPUT_STREAM (data);
  GdkX11SelectionInputStreamPrivate *priv = gdk_x11_selection_input_stream_get_instance_private (stream);
  XEvent *xevent = xev;
  Display *xdisplay;
  Window xwindow;

#if 0
  GList *tmp_list;
  GtkRetrievalInfo *info = NULL;
  GdkWindow *window;
  GBytes *bytes;
  gint length;
  GdkAtom type;
  GdkAtom selection;
  GdkAtom property;
  gint	  format;
  guint32 time;
#endif

  xdisplay = gdk_x11_display_get_xdisplay (priv->display);
  xwindow = GDK_X11_DISPLAY (priv->display)->leader_window;

  if (xevent->xany.window != xwindow)
    return GDK_FILTER_CONTINUE;

  switch (xevent->type)
    {
    case SelectionNotify:
      {
        GBytes *bytes;
        Atom type;
        gint format;

        if (priv->xselection != xevent->xselection.selection ||
            priv->xtarget != xevent->xselection.target)
          return GDK_FILTER_CONTINUE;

        GDK_NOTE(SELECTION, g_print ("%s:%s: got SelectionNotify\n", priv->selection, priv->target));

        if (xevent->xselection.property != None)
          bytes = get_selection_property (xdisplay, xwindow, xevent->xselection.property, &type, &format);
        else
          bytes = NULL;

        if (bytes == NULL)
          { 
            gdk_x11_selection_input_stream_complete (stream);
          }
        else
          {
            g_queue_push_tail (&priv->chunks, bytes);

            if (type == gdk_x11_get_xatom_by_name_for_display (priv->display, "INCR"))
              {
                /* The remainder of the selection will come through PropertyNotify
                   events on xwindow */
                GDK_NOTE(SELECTION, g_print ("%s:%s: initiating INCR transfer\n", priv->selection, priv->target));
                gdk_x11_selection_input_stream_flush (stream);
              }
            else
              {
                gdk_x11_selection_input_stream_complete (stream);
              }

            XDeleteProperty (xdisplay, xwindow, xevent->xselection.property);
          }
        }
      return GDK_FILTER_REMOVE;

    default:
      return GDK_FILTER_CONTINUE;
    }
}

GInputStream *
gdk_x11_selection_input_stream_new (GdkDisplay *display,
                                    const char *selection,
                                    const char *target,
                                    guint32     timestamp)
{
  GdkX11SelectionInputStream *stream;
  GdkX11SelectionInputStreamPrivate *priv;
  
  stream = g_object_new (GDK_TYPE_X11_SELECTION_INPUT_STREAM, NULL);
  priv = gdk_x11_selection_input_stream_get_instance_private (stream);

  priv->display = display;
  GDK_X11_DISPLAY (display)->input_streams = g_slist_prepend (GDK_X11_DISPLAY (display)->input_streams, stream);
  priv->selection = g_strdup (selection);
  priv->xselection = gdk_x11_get_xatom_by_name_for_display (display, priv->selection);
  priv->target = g_strdup (target);
  priv->xtarget = gdk_x11_get_xatom_by_name_for_display (display, priv->target);
  priv->property = g_strdup_printf ("GDK_SELECTION_%p", stream); 
  priv->xproperty = gdk_x11_get_xatom_by_name_for_display (display, priv->property);

  gdk_window_add_filter (NULL, gdk_x11_selection_input_stream_filter_event, stream);

  XConvertSelection (GDK_DISPLAY_XDISPLAY (display),
                     priv->xselection,
                     priv->xtarget,
                     priv->xproperty,
                     GDK_X11_DISPLAY (display)->leader_window,
                     timestamp);

  return g_object_ref (stream);
}

