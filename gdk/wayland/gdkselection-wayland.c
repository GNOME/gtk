/*
 * Copyright © 2010 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <fcntl.h>

#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <glib-unix.h>

#include "gdkwayland.h"
#include "gdkprivate-wayland.h"
#include "gdkdisplay-wayland.h"
#include "gdkdndprivate.h"
#include "gdkselection.h"
#include "gdkproperty.h"
#include "gdkprivate.h"

#include <string.h>

typedef struct _SelectionBuffer SelectionBuffer;
typedef struct _StoredSelection StoredSelection;
typedef struct _AsyncWriteData AsyncWriteData;

struct _SelectionBuffer
{
  GInputStream *stream;
  GCancellable *cancellable;
  GByteArray *data;
  GList *requestors;
  GdkAtom selection;
  GdkAtom target;
  gint ref_count;
};

struct _StoredSelection
{
  GdkWindow *source;
  GCancellable *cancellable;
  guchar *data;
  gsize data_len;
  GdkAtom type;
  gint fd;
};

struct _DataSourceData
{
  GdkWindow *window;
  GdkAtom selection;
};

struct _AsyncWriteData
{
  GOutputStream *stream;
  GdkWaylandSelection *selection;
  gsize index;
};

enum {
  ATOM_CLIPBOARD,
  ATOM_DND
};

static GdkAtom atoms[2] = { 0 };

struct _GdkWaylandSelection
{
  /* Destination-side data */
  struct wl_data_offer *offer;
  GdkAtom source_requested_target;

  GHashTable *selection_buffers; /* Hashtable of target_atom->SelectionBuffer */
  GList *targets; /* List of GdkAtom */

  /* Source-side data */
  StoredSelection stored_selection;

  struct wl_data_source *clipboard_source;
  GdkWindow *clipboard_owner;

  struct wl_data_source *dnd_source; /* Owned by the GdkDragContext */
  GdkWindow *dnd_owner;
};

static void selection_buffer_read (SelectionBuffer *buffer);
static void async_write_data_write (AsyncWriteData *write_data);

static void
selection_buffer_notify (SelectionBuffer *buffer)
{
  GdkEvent *event;
  GList *l;

  for (l = buffer->requestors; l; l = l->next)
    {
      event = gdk_event_new (GDK_SELECTION_NOTIFY);
      event->selection.window = g_object_ref (l->data);
      event->selection.send_event = FALSE;
      event->selection.selection = buffer->selection;
      event->selection.target = buffer->target;
      event->selection.property = gdk_atom_intern_static_string ("GDK_SELECTION");
      event->selection.time = GDK_CURRENT_TIME;
      event->selection.requestor = g_object_ref (l->data);

      gdk_event_put (event);
      gdk_event_free (event);
    }
}

static SelectionBuffer *
selection_buffer_new (GInputStream *stream,
                      GdkAtom       selection,
                      GdkAtom       target)
{
  SelectionBuffer *buffer;

  buffer = g_new0 (SelectionBuffer, 1);
  buffer->stream = (stream) ? g_object_ref (stream) : NULL;
  buffer->cancellable = g_cancellable_new ();
  buffer->data = g_byte_array_new ();
  buffer->selection = selection;
  buffer->target = target;
  buffer->ref_count = 1;

  if (stream)
    selection_buffer_read (buffer);

  return buffer;
}

static SelectionBuffer *
selection_buffer_ref (SelectionBuffer *buffer)
{
  buffer->ref_count++;
  return buffer;
}

static void
selection_buffer_unref (SelectionBuffer *buffer_data)
{
  buffer_data->ref_count--;

  if (buffer_data->ref_count != 0)
    return;

  if (buffer_data->cancellable)
    g_object_unref (buffer_data->cancellable);

  if (buffer_data->stream)
    g_object_unref (buffer_data->stream);

  if (buffer_data->data)
    g_byte_array_unref (buffer_data->data);

  g_free (buffer_data);
}

static void
selection_buffer_append_data (SelectionBuffer *buffer,
                              gconstpointer    data,
                              gsize            len)
{
  g_byte_array_append (buffer->data, data, len);
}

static void
selection_buffer_cancel_and_unref (SelectionBuffer *buffer_data)
{
  if (buffer_data->cancellable)
    g_cancellable_cancel (buffer_data->cancellable);

  selection_buffer_unref (buffer_data);
}

static void
selection_buffer_add_requestor (SelectionBuffer *buffer,
                                GdkWindow       *requestor)
{
  if (!g_list_find (buffer->requestors, requestor))
    buffer->requestors = g_list_prepend (buffer->requestors,
                                         g_object_ref (requestor));
}

static gboolean
selection_buffer_remove_requestor (SelectionBuffer *buffer,
                                   GdkWindow       *requestor)
{
  GList *link = g_list_find (buffer->requestors, requestor);

  if (!link)
    return FALSE;

  g_object_unref (link->data);
  buffer->requestors = g_list_delete_link (buffer->requestors, link);
  return TRUE;
}

static void
selection_buffer_read_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  SelectionBuffer *buffer = user_data;
  GError *error = NULL;
  GBytes *bytes;

  bytes = g_input_stream_read_bytes_finish (buffer->stream, result, &error);

  if (bytes && g_bytes_get_size (bytes) > 0)
    {
      selection_buffer_append_data (buffer,
                                    g_bytes_get_data (bytes, NULL),
                                    g_bytes_get_size (bytes));
      selection_buffer_read (buffer);
      g_bytes_unref (bytes);
    }
  else
    {
      if (error)
        {
          g_warning (G_STRLOC ": error reading selection buffer: %s\n", error->message);
          g_error_free (error);
        }
      else
        selection_buffer_notify (buffer);

      g_input_stream_close (buffer->stream, NULL, NULL);
      g_clear_object (&buffer->stream);
      g_clear_object (&buffer->cancellable);

      if (bytes)
        g_bytes_unref (bytes);
    }

  selection_buffer_unref (buffer);
}

static void
selection_buffer_read (SelectionBuffer *buffer)
{
  selection_buffer_ref (buffer);
  g_input_stream_read_bytes_async (buffer->stream, 1000, G_PRIORITY_DEFAULT,
                                   buffer->cancellable, selection_buffer_read_cb,
                                   buffer);
}

GdkWaylandSelection *
gdk_wayland_selection_new (void)
{
  GdkWaylandSelection *selection;

  /* init atoms */
  atoms[ATOM_CLIPBOARD] = gdk_atom_intern_static_string ("CLIPBOARD");
  atoms[ATOM_DND] = gdk_atom_intern_static_string ("GdkWaylandSelection");

  selection = g_new0 (GdkWaylandSelection, 1);
  selection->selection_buffers =
      g_hash_table_new_full (NULL, NULL, NULL,
                             (GDestroyNotify) selection_buffer_cancel_and_unref);
  return selection;
}

void
gdk_wayland_selection_free (GdkWaylandSelection *selection)
{
  g_hash_table_destroy (selection->selection_buffers);

  if (selection->targets)
    g_list_free (selection->targets);

  g_free (selection->stored_selection.data);

  if (selection->stored_selection.cancellable)
    {
      g_cancellable_cancel (selection->stored_selection.cancellable);
      g_object_unref (selection->stored_selection.cancellable);
    }

  if (selection->stored_selection.fd > 0)
    close (selection->stored_selection.fd);

  if (selection->offer)
    wl_data_offer_destroy (selection->offer);
  if (selection->clipboard_source)
    wl_data_source_destroy (selection->clipboard_source);
  if (selection->dnd_source)
    wl_data_source_destroy (selection->dnd_source);

  g_free (selection);
}

static void
data_offer_offer (void                 *data,
                  struct wl_data_offer *wl_data_offer,
                  const char           *type)
{
  GdkWaylandSelection *selection = data;
  GdkAtom atom = gdk_atom_intern (type, FALSE);

  if (g_list_find (selection->targets, atom))
    return;

  selection->targets = g_list_prepend (selection->targets, atom);
}

static const struct wl_data_offer_listener data_offer_listener = {
  data_offer_offer,
};

void
gdk_wayland_selection_set_offer (GdkDisplay           *display,
                                 struct wl_data_offer *wl_offer)
{
  GdkWaylandSelection *selection = gdk_wayland_display_get_selection (display);

  if (selection->offer == wl_offer)
    return;

  if (selection->offer)
    wl_data_offer_destroy (selection->offer);

  selection->offer = wl_offer;

  if (wl_offer)
    wl_data_offer_add_listener (wl_offer,
                                &data_offer_listener,
                                selection);

  /* Clear all buffers */
  g_hash_table_remove_all (selection->selection_buffers);
  g_list_free (selection->targets);
  selection->targets = NULL;
}

struct wl_data_offer *
gdk_wayland_selection_get_offer (GdkDisplay *display)
{
  GdkWaylandSelection *selection = gdk_wayland_display_get_selection (display);

  return selection->offer;
}

GList *
gdk_wayland_selection_get_targets (GdkDisplay *display)
{
  GdkWaylandSelection *selection = gdk_wayland_display_get_selection (display);

  return selection->targets;
}

static void
gdk_wayland_selection_emit_request (GdkWindow *window,
                                    GdkAtom    selection,
                                    GdkAtom    target)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_SELECTION_REQUEST);
  event->selection.window = g_object_ref (window);
  event->selection.send_event = FALSE;
  event->selection.selection = selection;
  event->selection.target = target;
  event->selection.property = gdk_atom_intern_static_string ("GDK_SELECTION");
  event->selection.time = GDK_CURRENT_TIME;
  event->selection.requestor = g_object_ref (window);

  gdk_event_put (event);
  gdk_event_free (event);
}

static AsyncWriteData *
async_write_data_new (GdkWaylandSelection *selection)
{
  AsyncWriteData *write_data;

  write_data = g_slice_new0 (AsyncWriteData);
  write_data->selection = selection;
  write_data->stream =
    g_unix_output_stream_new (selection->stored_selection.fd, TRUE);

  return write_data;
}

static void
async_write_data_free (AsyncWriteData *write_data)
{
  g_object_unref (write_data->stream);
  g_slice_free (AsyncWriteData, write_data);
}

static void
async_write_data_cb (GObject      *object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  AsyncWriteData *write_data = user_data;
  GError *error = NULL;
  gsize bytes_written;

  bytes_written = g_output_stream_write_finish (G_OUTPUT_STREAM (object),
                                                res, &error);
  if (error)
    {
      g_warning ("Error writing selection data: %s", error->message);
      g_error_free (error);

      async_write_data_free (write_data);
      return;
    }

  write_data->index += bytes_written;

  if (write_data->index <
      write_data->selection->stored_selection.data_len)
    {
      /* Write the next chunk */
      async_write_data_write (write_data);
    }
  else
    async_write_data_free (write_data);
}

static void
async_write_data_write (AsyncWriteData *write_data)
{
  GdkWaylandSelection *selection = write_data->selection;
  gsize buf_len;
  guchar *buf;

  buf = selection->stored_selection.data;
  buf_len = selection->stored_selection.data_len;

  g_output_stream_write_async (write_data->stream,
                               &buf[write_data->index],
                               buf_len - write_data->index,
                               G_PRIORITY_DEFAULT,
                               selection->stored_selection.cancellable,
                               async_write_data_cb,
                               write_data);
}

static gboolean
gdk_wayland_selection_check_write (GdkWaylandSelection *selection)
{
  AsyncWriteData *write_data;

  if (selection->stored_selection.fd < 0 ||
      selection->stored_selection.data_len == 0)
    return FALSE;

  write_data = async_write_data_new (selection);
  async_write_data_write (write_data);
  selection->stored_selection.fd = -1;

  return TRUE;
}

void
gdk_wayland_selection_store (GdkWindow    *window,
                             GdkAtom       type,
                             GdkPropMode   mode,
                             const guchar *data,
                             gint          len)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkWaylandSelection *selection = gdk_wayland_display_get_selection (display);
  GArray *array;

  array = g_array_new (TRUE, FALSE, sizeof (guchar));
  g_array_append_vals (array, data, len);

  if (selection->stored_selection.data)
    {
      if (mode != GDK_PROP_MODE_REPLACE &&
          type != selection->stored_selection.type)
        {
          g_warning (G_STRLOC ": Attempted to append/prepend selection data with "
                     "type %s into the current selection with type %s",
                     gdk_atom_name (type),
                     gdk_atom_name (selection->stored_selection.type));
          return;
        }

      /* In these cases we also replace the stored data, so we
       * apply the inverse operation into the just given data.
       */
      if (mode == GDK_PROP_MODE_APPEND)
        g_array_prepend_vals (array, selection->stored_selection.data,
                              selection->stored_selection.data_len - 1);
      else if (mode == GDK_PROP_MODE_PREPEND)
        g_array_append_vals (array, selection->stored_selection.data,
                             selection->stored_selection.data_len - 1);

      g_free (selection->stored_selection.data);
    }

  if (selection->stored_selection.cancellable)
    {
      g_cancellable_cancel (selection->stored_selection.cancellable);
      g_object_unref (selection->stored_selection.cancellable);
    }

  selection->stored_selection.source = window;
  selection->stored_selection.data_len = array->len;
  selection->stored_selection.data = (guchar *) g_array_free (array, FALSE);
  selection->stored_selection.type = type;
  selection->stored_selection.cancellable = g_cancellable_new ();

  gdk_wayland_selection_check_write (selection);
}

static SelectionBuffer *
gdk_wayland_selection_lookup_requestor_buffer (GdkWindow *requestor)
{
  GdkDisplay *display = gdk_window_get_display (requestor);
  GdkWaylandSelection *selection = gdk_wayland_display_get_selection (display);
  SelectionBuffer *buffer_data;
  GHashTableIter iter;

  g_hash_table_iter_init (&iter, selection->selection_buffers);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &buffer_data))
    {
      if (g_list_find (buffer_data->requestors, requestor))
        return buffer_data;
    }

  return NULL;
}

static gboolean
gdk_wayland_selection_request_target (GdkWaylandSelection *wayland_selection,
                                      GdkWindow           *window,
                                      GdkAtom              target,
                                      gint                 fd)
{
  GdkAtom selection;

  if (wayland_selection->clipboard_owner == window)
    selection = atoms[ATOM_CLIPBOARD];
  else if (wayland_selection->dnd_owner == window)
    selection = atoms[ATOM_DND];
  else
    return FALSE;

  if (wayland_selection->stored_selection.fd == fd &&
      wayland_selection->source_requested_target == target)
    return FALSE;

  wayland_selection->stored_selection.fd = fd;

  wayland_selection->source_requested_target = target;

  if (window && target != GDK_NONE)
    {
      gdk_wayland_selection_emit_request (window, selection, target);
      return TRUE;
    }

  return FALSE;
}

static void
data_source_target (void                  *data,
                    struct wl_data_source *source,
                    const char            *mime_type)
{
  GdkWaylandSelection *wayland_selection = data;
  GdkDragContext *context = NULL;
  GdkWindow *window;

  g_debug (G_STRLOC ": %s source = %p, mime_type = %s",
           G_STRFUNC, source, mime_type);

  context = gdk_wayland_drag_context_lookup_by_data_source (source);

  if (!mime_type)
    {
      if (context)
        {
          gdk_wayland_drag_context_set_action (context, 0);
          _gdk_wayland_drag_context_emit_event (context, GDK_DRAG_STATUS,
                                                GDK_CURRENT_TIME);
        }
      return;
    }

  if (source == wayland_selection->dnd_source)
    {
      window = wayland_selection->dnd_owner;
      gdk_wayland_drag_context_set_action (context, GDK_ACTION_COPY);
      _gdk_wayland_drag_context_emit_event (context, GDK_DRAG_STATUS,
                                            GDK_CURRENT_TIME);
    }
  else if (source == wayland_selection->clipboard_source)
    window = wayland_selection->clipboard_owner;
  else
    return;

  gdk_wayland_selection_request_target (wayland_selection, window,
                                        gdk_atom_intern (mime_type, FALSE),
                                        -1);
}

static void
data_source_send (void                  *data,
                  struct wl_data_source *source,
                  const char            *mime_type,
                  int32_t                fd)
{
  GdkWaylandSelection *wayland_selection = data;
  GdkDragContext *context;
  GdkWindow *window;

  g_debug (G_STRLOC ": %s source = %p, mime_type = %s, fd = %d",
           G_STRFUNC, source, mime_type, fd);

  if (!mime_type)
    return;

  context = gdk_wayland_drag_context_lookup_by_data_source (source);

  if (source == wayland_selection->dnd_source)
    window = wayland_selection->dnd_owner;
  else if (source == wayland_selection->clipboard_source)
    window = wayland_selection->clipboard_owner;
  else
    return;

  if (!gdk_wayland_selection_request_target (wayland_selection, window,
                                             gdk_atom_intern (mime_type, FALSE),
                                             fd))
    gdk_wayland_selection_check_write (wayland_selection);

  if (context)
    {
      gdk_wayland_drag_context_undo_grab (context);
      _gdk_wayland_drag_context_emit_event (context, GDK_DROP_FINISHED,
                                            GDK_CURRENT_TIME);
    }

  wayland_selection->source_requested_target = GDK_NONE;
}

static void
data_source_cancelled (void                  *data,
                       struct wl_data_source *source)
{
  GdkWaylandSelection *wayland_selection = data;
  GdkDragContext *context;
  GdkDisplay *display;

  g_debug (G_STRLOC ": %s source = %p",
           G_STRFUNC, source);

  display = gdk_display_get_default ();

  if (source == wayland_selection->dnd_source)
    {
      gdk_wayland_selection_unset_data_source (display, atoms[ATOM_DND]);

      context = gdk_wayland_drag_context_lookup_by_data_source (source);

      if (context)
        gdk_wayland_drag_context_undo_grab (context);
    }
  else if (source == wayland_selection->clipboard_source)
    gdk_wayland_selection_unset_data_source (display, atoms[ATOM_CLIPBOARD]);
}

static const struct wl_data_source_listener data_source_listener = {
  data_source_target,
  data_source_send,
  data_source_cancelled
};

struct wl_data_source *
gdk_wayland_selection_get_data_source (GdkWindow *owner,
                                       GdkAtom    selection)
{
  GdkDisplay *display = gdk_window_get_display (owner);
  GdkWaylandSelection *wayland_selection = gdk_wayland_display_get_selection (display);
  struct wl_data_source *source = NULL;
  GdkWaylandDisplay *display_wayland;
  gboolean is_clipboard = FALSE;

  if (selection == atoms[ATOM_DND])
    {
      if (wayland_selection->dnd_source &&
          (!owner || owner == wayland_selection->dnd_owner))
        return wayland_selection->dnd_source;
    }
  else if (selection == atoms[ATOM_CLIPBOARD])
    {
      if (wayland_selection->clipboard_source &&
          (!owner || owner == wayland_selection->clipboard_owner))
        return wayland_selection->clipboard_source;

      if (wayland_selection->clipboard_source)
        {
          wl_data_source_destroy (wayland_selection->clipboard_source);
          wayland_selection->clipboard_source = NULL;
        }

      is_clipboard = TRUE;
    }
  else
    return NULL;

  if (!owner)
    return NULL;

  display_wayland = GDK_WAYLAND_DISPLAY (gdk_window_get_display (owner));

  source = wl_data_device_manager_create_data_source (display_wayland->data_device_manager);
  wl_data_source_add_listener (source,
                               &data_source_listener,
                               wayland_selection);

  if (is_clipboard)
    {
      wayland_selection->clipboard_source = source;
      wayland_selection->clipboard_owner = owner;
    }
  else
    {
      wayland_selection->dnd_source = source;
      wayland_selection->dnd_owner = owner;
    }

  return source;
}

void
gdk_wayland_selection_unset_data_source (GdkDisplay *display,
                                         GdkAtom     selection)
{
  GdkWaylandSelection *wayland_selection = gdk_wayland_display_get_selection (display);

  if (selection == atoms[ATOM_CLIPBOARD])
    {
      GdkDeviceManager *device_manager;
      GdkDevice *device;

      device_manager = gdk_display_get_device_manager (display);
      device = gdk_device_manager_get_client_pointer (device_manager);

      gdk_wayland_device_set_selection (device, NULL);
      wayland_selection->clipboard_owner = NULL;

      if (wayland_selection->clipboard_source)
        {
          wl_data_source_destroy (wayland_selection->clipboard_source);
          wayland_selection->clipboard_source = NULL;
        }
    }
  else if (selection == atoms[ATOM_DND])
    {
      wayland_selection->dnd_owner = NULL;
      wayland_selection->dnd_source = NULL;
    }
}

GdkWindow *
_gdk_wayland_display_get_selection_owner (GdkDisplay *display,
                                          GdkAtom     selection)
{
  GdkWaylandSelection *wayland_selection = gdk_wayland_display_get_selection (display);

  if (selection == atoms[ATOM_CLIPBOARD])
    return wayland_selection->clipboard_owner;
  else if (selection == atoms[ATOM_DND])
    return wayland_selection->dnd_owner;

  return NULL;
}

gboolean
_gdk_wayland_display_set_selection_owner (GdkDisplay *display,
                                          GdkWindow  *owner,
                                          GdkAtom     selection,
                                          guint32     time,
                                          gboolean    send_event)
{
  GdkWaylandSelection *wayland_selection = gdk_wayland_display_get_selection (display);

  if (selection == atoms[ATOM_CLIPBOARD])
    {
      wayland_selection->clipboard_owner = owner;
      return TRUE;
    }
  else if (selection == atoms[ATOM_DND])
    {
      wayland_selection->dnd_owner = owner;
      return TRUE;
    }

  return FALSE;
}

void
_gdk_wayland_display_send_selection_notify (GdkDisplay *dispay,
                                            GdkWindow  *requestor,
                                            GdkAtom     selection,
                                            GdkAtom     target,
                                            GdkAtom     property,
                                            guint32     time)
{
}

gint
_gdk_wayland_display_get_selection_property (GdkDisplay  *display,
                                             GdkWindow   *requestor,
                                             guchar     **data,
                                             GdkAtom     *ret_type,
                                             gint        *ret_format)
{
  SelectionBuffer *buffer_data;
  gsize len;

  buffer_data = gdk_wayland_selection_lookup_requestor_buffer (requestor);

  if (!buffer_data)
    return 0;

  selection_buffer_remove_requestor (buffer_data, requestor);
  len = buffer_data->data->len;

  if (data)
    {
      guchar *buffer;

      buffer = g_new0 (guchar, len + 1);
      memcpy (buffer, buffer_data->data->data, len);
      *data = buffer;
    }

  if (buffer_data->target == gdk_atom_intern_static_string ("TARGETS"))
    {
      if (ret_type)
        *ret_type = GDK_SELECTION_TYPE_ATOM;
      if (ret_format)
        *ret_format = 32;
    }
  else
    {
      if (ret_type)
        *ret_type = GDK_SELECTION_TYPE_STRING;
      if (ret_format)
        *ret_format = 8;
    }

  return len;
}

void
_gdk_wayland_display_convert_selection (GdkDisplay *display,
                                        GdkWindow  *requestor,
                                        GdkAtom     selection,
                                        GdkAtom     target,
                                        guint32     time)
{
  GdkWaylandSelection *wayland_selection = gdk_wayland_display_get_selection (display);
  SelectionBuffer *buffer_data;

  if (!wayland_selection->offer)
    {
      GdkEvent *event;

      event = gdk_event_new (GDK_SELECTION_NOTIFY);
      event->selection.window = g_object_ref (requestor);
      event->selection.send_event = FALSE;
      event->selection.selection = selection;
      event->selection.target = target;
      event->selection.property = GDK_NONE;
      event->selection.time = GDK_CURRENT_TIME;
      event->selection.requestor = g_object_ref (requestor);

      gdk_event_put (event);
      gdk_event_free (event);
      return;
    }

  if (target != gdk_atom_intern_static_string ("TARGETS"))
    wl_data_offer_accept (wayland_selection->offer,
                          _gdk_wayland_display_get_serial (GDK_WAYLAND_DISPLAY (display)),
                          gdk_atom_name (target));

  buffer_data = g_hash_table_lookup (wayland_selection->selection_buffers,
                                     target);

  if (buffer_data)
    selection_buffer_add_requestor (buffer_data, requestor);
  else
    {
      GInputStream *stream = NULL;
      int pipe_fd[2], natoms = 0;
      GdkAtom *atoms = NULL;

      if (target == gdk_atom_intern_static_string ("TARGETS"))
        {
          gint i = 0;
          GList *l;

          natoms = g_list_length (wayland_selection->targets);
          atoms = g_new0 (GdkAtom, natoms);

          for (l = wayland_selection->targets; l; l = l->next)
            atoms[i++] = l->data;
        }
      else
        {
          g_unix_open_pipe (pipe_fd, FD_CLOEXEC, NULL);
          wl_data_offer_receive (wayland_selection->offer,
                                 gdk_atom_name (target),
                                 pipe_fd[1]);
          stream = g_unix_input_stream_new (pipe_fd[0], TRUE);
          close (pipe_fd[1]);
        }

      buffer_data = selection_buffer_new (stream, selection, target);
      selection_buffer_add_requestor (buffer_data, requestor);

      if (stream)
        g_object_unref (stream);

      if (atoms)
        {
          /* Store directly the local atoms */
          selection_buffer_append_data (buffer_data, atoms, natoms * sizeof (GdkAtom));
          g_free (atoms);
        }

      g_hash_table_insert (wayland_selection->selection_buffers,
                           GDK_ATOM_TO_POINTER (target),
                           buffer_data);
    }

  if (!buffer_data->stream)
    selection_buffer_notify (buffer_data);
}

gint
_gdk_wayland_display_text_property_to_utf8_list (GdkDisplay    *display,
                                                 GdkAtom        encoding,
                                                 gint           format,
                                                 const guchar  *text,
                                                 gint           length,
                                                 gchar       ***list)
{
  GPtrArray *array;
  const gchar *ptr;
  gsize chunk_len;
  gchar *copy;
  guint nitems;

  ptr = (const gchar *) text;
  array = g_ptr_array_new ();

  while (ptr < (const gchar *) &text[length])
    {
      chunk_len = strlen (ptr);

      if (g_utf8_validate (ptr, chunk_len, NULL))
        {
          copy = g_strndup (ptr, chunk_len);
          g_ptr_array_add (array, copy);
        }

      ptr = &ptr[chunk_len + 1];
    }

  nitems = array->len;
  g_ptr_array_add (array, NULL);

  if (list)
    *list = (gchar **) g_ptr_array_free (array, FALSE);
  else
    g_ptr_array_free (array, TRUE);

  return nitems;
}

gchar *
_gdk_wayland_display_utf8_to_string_target (GdkDisplay  *display,
                                            const gchar *str)
{
  return NULL;
}

void
gdk_wayland_selection_add_targets (GdkWindow *window,
                                   GdkAtom    selection,
                                   guint      ntargets,
                                   GdkAtom   *targets)
{
  struct wl_data_source *data_source;
  guint i;

  g_return_if_fail (GDK_IS_WINDOW (window));

  data_source = gdk_wayland_selection_get_data_source (window, selection);

  if (!data_source)
    return;

  for (i = 0; i < ntargets; i++)
    wl_data_source_offer (data_source, gdk_atom_name (targets[i]));

  if (selection == atoms[ATOM_CLIPBOARD])
    {
      GdkDeviceManager *device_manager;
      GdkDisplay *display;
      GdkDevice *device;

      display = gdk_window_get_display (window);
      device_manager = gdk_display_get_device_manager (display);
      device = gdk_device_manager_get_client_pointer (device_manager);
      gdk_wayland_device_set_selection (device, data_source);
    }
}

void
gdk_wayland_selection_clear_targets (GdkDisplay *display,
                                     GdkAtom     selection)
{
  gdk_wayland_selection_unset_data_source (display, selection);
}
