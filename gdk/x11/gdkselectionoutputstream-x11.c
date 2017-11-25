/* GIO - GLib Output, Output and Streaming Library
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

#include "gdkselectionoutputstream-x11.h"

#include "gdkdisplay-x11.h"
#include "gdkintl.h"
#include "gdkx11display.h"
#include "gdkx11property.h"
#include "gdkx11window.h"

typedef struct GdkX11SelectionOutputStreamPrivate  GdkX11SelectionOutputStreamPrivate;

struct GdkX11SelectionOutputStreamPrivate {
  GdkDisplay *display;
  Window xwindow;
  char *selection;
  Atom xselection;
  char *target;
  Atom xtarget;
  char *property;
  Atom xproperty;
  const char *type;
  Atom xtype;
  int format;
  gulong timestamp;

  GMutex mutex;
  GCond cond;
  GByteArray *data;
  guint flush_requested : 1;

  GTask *pending_task;

  guint started : 1;
  guint incr : 1;
  guint delete_pending : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (GdkX11SelectionOutputStream, gdk_x11_selection_output_stream, G_TYPE_OUTPUT_STREAM);

static GdkFilterReturn
gdk_x11_selection_output_stream_filter_event (GdkXEvent *xevent,
                                              GdkEvent  *gdkevent,
                                              gpointer   data);

static gboolean
gdk_x11_selection_output_stream_can_flush (GdkX11SelectionOutputStream *stream)
{
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);

  if (priv->delete_pending)
    return FALSE;

  return TRUE;
}

static gboolean
gdk_x11_selection_output_stream_needs_flush_unlocked (GdkX11SelectionOutputStream *stream)
{
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);

  if (priv->data->len == 0)
    return FALSE;

  if (g_output_stream_is_closing (G_OUTPUT_STREAM (stream)))
    return TRUE;

  if (priv->flush_requested)
    return TRUE;

  return priv->data->len >= gdk_x11_display_get_max_request_size (priv->display);
}

static gboolean
gdk_x11_selection_output_stream_needs_flush (GdkX11SelectionOutputStream *stream)
{
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);

  gboolean result;

  g_mutex_lock (&priv->mutex);

  result = gdk_x11_selection_output_stream_needs_flush_unlocked (stream);

  g_mutex_unlock (&priv->mutex);

  return result;
}

static gsize
get_element_size (int format)
{
  switch (format)
    {
    case 8:
      return 1;

    case 16:
      return sizeof (short);

    case 32:
      return sizeof (long);

    default:
      g_warning ("Unknown format %u", format);
      return 1;
    }
}

static void
gdk_x11_selection_output_stream_perform_flush (GdkX11SelectionOutputStream *stream)
{
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);
  Display *xdisplay;
  gsize element_size, n_elements;
  int error;

  g_assert (!priv->delete_pending);

  xdisplay = gdk_x11_display_get_xdisplay (priv->display);

  /* We operate on a foreign window, better guard against catastrophe */
  gdk_x11_display_error_trap_push (priv->display);

  g_mutex_lock (&priv->mutex);

  element_size = get_element_size (priv->format);
  n_elements = priv->data->len / element_size;

  if (!priv->started && !g_output_stream_is_closing (G_OUTPUT_STREAM (stream)))
    {
      XWindowAttributes attrs;

      priv->incr = TRUE;
      GDK_NOTE(SELECTION, g_printerr ("%s:%s: initiating INCR transfer\n",
                                      priv->selection, priv->target));

      XGetWindowAttributes (xdisplay,
			    priv->xwindow,
			    &attrs);
      if (!(attrs.your_event_mask & PropertyChangeMask))
        {
          XSelectInput (xdisplay, priv->xwindow, attrs.your_event_mask | PropertyChangeMask);
        }

      XChangeProperty (GDK_DISPLAY_XDISPLAY (priv->display),
                       priv->xwindow,
                       priv->xproperty, 
                       gdk_x11_get_xatom_by_name_for_display (priv->display, "INCR"),
                       32,
                       PropModeReplace,
                       (guchar *) &(long) { n_elements },
                       1);
    }
  else
    {
      XChangeProperty (GDK_DISPLAY_XDISPLAY (priv->display),
                       priv->xwindow,
                       priv->xproperty, 
                       priv->xtype,
                       priv->format,
                       PropModeReplace,
                       priv->data->data,
                       n_elements);
      GDK_NOTE(SELECTION, g_printerr ("%s:%s: wrote %zu/%u bytes\n",
                                      priv->selection, priv->target, n_elements * element_size, priv->data->len));
      g_byte_array_remove_range (priv->data, 0, n_elements * element_size);
      if (priv->data->len < element_size)
        priv->flush_requested = FALSE;
    }

  if (!priv->started)
    {
      XSelectionEvent xevent;

      xevent.type = SelectionNotify;
      xevent.serial = 0;
      xevent.send_event = True;
      xevent.requestor = priv->xwindow;
      xevent.selection = priv->xselection;
      xevent.target = priv->xtarget;
      xevent.property = priv->xproperty;
      xevent.time = priv->timestamp;

      if (XSendEvent (xdisplay, priv->xwindow, False, NoEventMask, (XEvent*) & xevent) == 0)
        {
          GDK_NOTE(SELECTION, g_printerr ("%s:%s: failed to XSendEvent()\n",
                                          priv->selection, priv->target));
          g_warning ("failed to XSendEvent()");
        }
      XSync (xdisplay, False);

      GDK_NOTE(SELECTION, g_printerr ("%s:%s: sent SelectionNotify for %s on %s\n",
                                      priv->selection, priv->target, priv->target, priv->property));
      priv->started = TRUE;
    }

  priv->delete_pending = TRUE;
  g_cond_broadcast (&priv->cond);
  g_mutex_unlock (&priv->mutex);

  /* XXX: handle failure here and report EPIPE for future operations on the stream? */
  error = gdk_x11_display_error_trap_pop (priv->display);
  if (error != Success)
    {
      GDK_NOTE(SELECTION, g_printerr ("%s:%s: X error during write: %d\n",
                                      priv->selection, priv->target, error));
    }

  if (priv->pending_task)
    {
      g_task_return_int (priv->pending_task, GPOINTER_TO_SIZE (g_task_get_task_data (priv->pending_task)));
      g_object_unref (priv->pending_task);
      priv->pending_task = NULL;
    }
}

static gboolean
gdk_x11_selection_output_stream_invoke_flush (gpointer data)
{
  GdkX11SelectionOutputStream *stream = GDK_X11_SELECTION_OUTPUT_STREAM (data);

  if (gdk_x11_selection_output_stream_needs_flush (stream) &
      gdk_x11_selection_output_stream_can_flush (stream))
    gdk_x11_selection_output_stream_perform_flush (stream);

  return G_SOURCE_REMOVE;
}

static gssize
gdk_x11_selection_output_stream_write (GOutputStream  *output_stream,
                                       const void     *buffer,
                                       gsize           count,
                                       GCancellable   *cancellable,
                                       GError        **error)
{
  GdkX11SelectionOutputStream *stream = GDK_X11_SELECTION_OUTPUT_STREAM (output_stream);
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);

  g_mutex_lock (&priv->mutex);
  g_byte_array_append (priv->data, buffer, count);
  GDK_NOTE(SELECTION, g_printerr ("%s:%s: wrote %zu bytes, %u total now\n",
                                  priv->selection, priv->target, count, priv->data->len));
  g_mutex_unlock (&priv->mutex);

  g_main_context_invoke (NULL, gdk_x11_selection_output_stream_invoke_flush, stream);

  g_mutex_lock (&priv->mutex);
  if (gdk_x11_selection_output_stream_needs_flush_unlocked (stream))
    g_cond_wait (&priv->cond, &priv->mutex);
  g_mutex_unlock (&priv->mutex);

  return count;
}

static void
gdk_x11_selection_output_stream_write_async (GOutputStream        *output_stream,
                                             const void           *buffer,
                                             gsize                 count,
                                             int                   io_priority,
                                             GCancellable         *cancellable,
                                             GAsyncReadyCallback   callback,
                                             gpointer              user_data)
{
  GdkX11SelectionOutputStream *stream = GDK_X11_SELECTION_OUTPUT_STREAM (output_stream);
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);
  GTask *task;
  
  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, gdk_x11_selection_output_stream_write_async);
  g_task_set_priority (task, io_priority);

  g_mutex_lock (&priv->mutex);
  g_byte_array_append (priv->data, buffer, count);
  GDK_NOTE(SELECTION, g_printerr ("%s:%s: async wrote %zu bytes, %u total now\n",
                                  priv->selection, priv->target, count, priv->data->len));
  g_mutex_unlock (&priv->mutex);

  if (!gdk_x11_selection_output_stream_needs_flush (stream))
    {
      g_task_return_int (task, count);
      g_object_unref (task);
      return;
    }
  else if (!gdk_x11_selection_output_stream_can_flush (stream))
    {
      g_assert (priv->pending_task == NULL);
      priv->pending_task = task;
      g_task_set_task_data (task, GSIZE_TO_POINTER (count), NULL);
      return;
    }
  else
    {
      gdk_x11_selection_output_stream_perform_flush (stream);
      g_task_return_int (task, count);
      g_object_unref (task);
      return;
    }
}

static gssize
gdk_x11_selection_output_stream_write_finish (GOutputStream  *stream,
                                              GAsyncResult   *result,
                                              GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), -1);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_x11_selection_output_stream_write_async, -1);

  return g_task_propagate_int (G_TASK (result), error);
}

static gboolean
gdk_x11_selection_output_request_flush (GdkX11SelectionOutputStream *stream)
{
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);
  gboolean needs_flush;

  g_mutex_lock (&priv->mutex);

  if (priv->data->len >= get_element_size (priv->format))
    priv->flush_requested = TRUE;

  needs_flush = gdk_x11_selection_output_stream_needs_flush_unlocked (stream);
  g_mutex_unlock (&priv->mutex);

  GDK_NOTE(SELECTION, g_printerr ("%s:%s: requested flush%s\n",
                                  priv->selection, priv->target, needs_flush ? "" : ", but not needed"));
  return needs_flush;
}

static gboolean
gdk_x11_selection_output_stream_flush (GOutputStream  *output_stream,
                                       GCancellable   *cancellable,
                                       GError        **error)
{
  GdkX11SelectionOutputStream *stream = GDK_X11_SELECTION_OUTPUT_STREAM (output_stream);
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);

  if (!gdk_x11_selection_output_request_flush (stream))
    return TRUE;

  g_main_context_invoke (NULL, gdk_x11_selection_output_stream_invoke_flush, stream);

  g_mutex_lock (&priv->mutex);
  if (gdk_x11_selection_output_stream_needs_flush_unlocked (stream))
    g_cond_wait (&priv->cond, &priv->mutex);
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

static void
gdk_x11_selection_output_stream_flush_async (GOutputStream       *output_stream,
                                             int                  io_priority,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  GdkX11SelectionOutputStream *stream = GDK_X11_SELECTION_OUTPUT_STREAM (output_stream);
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);
  GTask *task;
  
  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, gdk_x11_selection_output_stream_flush_async);
  g_task_set_priority (task, io_priority);

  if (!gdk_x11_selection_output_stream_can_flush (stream))
    {
      if (gdk_x11_selection_output_request_flush (stream))
        {
          g_assert (priv->pending_task == NULL);
          priv->pending_task = task;
          return;
        }
      else
        {
          g_task_return_boolean (task, TRUE);
          g_object_unref (task);
          return;
        }
    }

  gdk_x11_selection_output_stream_perform_flush (stream);
  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
  return;
}

static gboolean
gdk_x11_selection_output_stream_flush_finish (GOutputStream  *stream,
                                              GAsyncResult   *result,
                                              GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result, gdk_x11_selection_output_stream_flush_async), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static gboolean
gdk_x11_selection_output_stream_invoke_close (gpointer stream)
{
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);

  GDK_X11_DISPLAY (priv->display)->streams = g_slist_remove (GDK_X11_DISPLAY (priv->display)->streams, stream);
  gdk_window_remove_filter (NULL, gdk_x11_selection_output_stream_filter_event, stream);

  return G_SOURCE_REMOVE;
}

static gboolean
gdk_x11_selection_output_stream_close (GOutputStream  *stream,
                                       GCancellable   *cancellable,
                                       GError        **error)
{
  g_main_context_invoke (NULL, gdk_x11_selection_output_stream_invoke_close, stream);

  return TRUE;
}

static void
gdk_x11_selection_output_stream_close_async (GOutputStream       *stream,
                                             int                  io_priority,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  GTask *task;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, gdk_x11_selection_output_stream_close_async);
  g_task_set_priority (task, io_priority);

  gdk_x11_selection_output_stream_invoke_close (stream);
  g_task_return_boolean (task, TRUE);

  g_object_unref (task);
}

static gboolean
gdk_x11_selection_output_stream_close_finish (GOutputStream  *stream,
                                              GAsyncResult   *result,
                                              GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result, gdk_x11_selection_output_stream_close_async), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gdk_x11_selection_output_stream_finalize (GObject *object)
{
  GdkX11SelectionOutputStream *stream = GDK_X11_SELECTION_OUTPUT_STREAM (object);
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);

  g_byte_array_unref (priv->data);
  g_cond_clear (&priv->cond);
  g_mutex_clear (&priv->mutex);

  g_free (priv->selection);
  g_free (priv->target);
  g_free (priv->property);

  G_OBJECT_CLASS (gdk_x11_selection_output_stream_parent_class)->finalize (object);
}

static void
gdk_x11_selection_output_stream_class_init (GdkX11SelectionOutputStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GOutputStreamClass *output_stream_class = G_OUTPUT_STREAM_CLASS (klass);

  object_class->finalize = gdk_x11_selection_output_stream_finalize;
  
  output_stream_class->write_fn = gdk_x11_selection_output_stream_write;
  output_stream_class->flush = gdk_x11_selection_output_stream_flush;
  output_stream_class->close_fn = gdk_x11_selection_output_stream_close;

  output_stream_class->write_async = gdk_x11_selection_output_stream_write_async;
  output_stream_class->write_finish = gdk_x11_selection_output_stream_write_finish;
  output_stream_class->flush_async = gdk_x11_selection_output_stream_flush_async;
  output_stream_class->flush_finish = gdk_x11_selection_output_stream_flush_finish;
  output_stream_class->close_async = gdk_x11_selection_output_stream_close_async;
  output_stream_class->close_finish = gdk_x11_selection_output_stream_close_finish;
}

static void
gdk_x11_selection_output_stream_init (GdkX11SelectionOutputStream *stream)
{
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);

  g_mutex_init (&priv->mutex);
  g_cond_init (&priv->cond);
  priv->data = g_byte_array_new ();
}

static GdkFilterReturn
gdk_x11_selection_output_stream_filter_event (GdkXEvent *xev,
                                             GdkEvent  *gdkevent,
                                             gpointer   data)
{
  GdkX11SelectionOutputStream *stream = GDK_X11_SELECTION_OUTPUT_STREAM (data);
  GdkX11SelectionOutputStreamPrivate *priv = gdk_x11_selection_output_stream_get_instance_private (stream);
  XEvent *xevent = xev;
  Display *xdisplay;

  xdisplay = gdk_x11_display_get_xdisplay (priv->display);

  if (xevent->xany.display != xdisplay ||
      xevent->xany.window != priv->xwindow)
    return GDK_FILTER_CONTINUE;

  switch (xevent->type)
  {
    case PropertyNotify:
      if (!priv->incr ||
          xevent->xproperty.atom != priv->xproperty ||
          xevent->xproperty.state != PropertyDelete)
        return GDK_FILTER_CONTINUE;

      GDK_NOTE(SELECTION, g_printerr ("%s:%s: got PropertyNotify Delete during INCR\n",
                                      priv->selection, priv->target));
      return GDK_FILTER_CONTINUE;

    default:
      return GDK_FILTER_CONTINUE;
    }
}

GOutputStream *
gdk_x11_selection_output_stream_new (GdkDisplay *display,
                                     Window      window,
                                     const char *selection,
                                     const char *target,
                                     const char *property,
                                     const char *type,
                                     int         format,
                                     gulong      timestamp)
{
  GdkX11SelectionOutputStream *stream;
  GdkX11SelectionOutputStreamPrivate *priv;

  stream = g_object_new (GDK_TYPE_X11_SELECTION_OUTPUT_STREAM, NULL);
  priv = gdk_x11_selection_output_stream_get_instance_private (stream);

  priv->display = display;
  GDK_X11_DISPLAY (display)->streams = g_slist_prepend (GDK_X11_DISPLAY (display)->streams, stream);
  priv->xwindow = window;
  priv->selection = g_strdup (selection);
  priv->xselection = gdk_x11_get_xatom_by_name_for_display (display, priv->selection);
  priv->target = g_strdup (target);
  priv->xtarget = gdk_x11_get_xatom_by_name_for_display (display, priv->target);
  priv->property = g_strdup (property);
  priv->xproperty = gdk_x11_get_xatom_by_name_for_display (display, priv->property);
  priv->type = g_strdup (type);
  priv->xtype = gdk_x11_get_xatom_by_name_for_display (display, priv->type);
  priv->format = format;
  priv->timestamp = timestamp;

  gdk_window_add_filter (NULL, gdk_x11_selection_output_stream_filter_event, stream);

  return G_OUTPUT_STREAM (stream);
}
