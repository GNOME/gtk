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
#include <unistd.h>

#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <glib-unix.h>

#include "gdkwayland.h"
#include "gdkprivate-wayland.h"
#include "gdkdisplay-wayland.h"
#include "gdkcontentformatsprivate.h"
#include "gdkdndprivate.h"
#include "gdkproperty.h"

#include <string.h>

typedef struct _SelectionData SelectionData;
typedef struct _DataOfferData DataOfferData;

struct _DataOfferData
{
  GDestroyNotify destroy_notify;
  gpointer offer_data;
  GdkContentFormats *targets;
};

struct _SelectionData
{
  DataOfferData *offer;
};

struct _GdkWaylandSelection
{
  /* Destination-side data */
  SelectionData selection;
  GHashTable *offers; /* Currently alive offers, Hashtable of wl_data_offer->DataOfferData */

  struct wl_data_source *dnd_source; /* Owned by the GdkDragContext */
};

static DataOfferData *
data_offer_data_new (gpointer       offer,
                     GDestroyNotify destroy_notify)
{
  DataOfferData *info;

  info = g_slice_new0 (DataOfferData);
  info->offer_data = offer;
  info->destroy_notify = destroy_notify;
  info->targets = gdk_content_formats_new (NULL, 0);

  return info;
}

static void
data_offer_data_free (DataOfferData *info)
{
  info->destroy_notify (info->offer_data);
  gdk_content_formats_unref (info->targets);
  g_slice_free (DataOfferData, info);
}

GdkWaylandSelection *
gdk_wayland_selection_new (void)
{
  GdkWaylandSelection *selection;

  selection = g_new0 (GdkWaylandSelection, 1);

  selection->offers =
    g_hash_table_new_full (NULL, NULL, NULL,
                           (GDestroyNotify) data_offer_data_free);
  return selection;
}

void
gdk_wayland_selection_free (GdkWaylandSelection *selection)
{
  g_hash_table_destroy (selection->offers);

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
  GdkContentFormatsBuilder *builder;
  DataOfferData *info;

  info = g_hash_table_lookup (selection->offers, wl_data_offer);

  if (!info || gdk_content_formats_contain_mime_type (info->targets, type))
    return;

  GDK_NOTE (EVENTS,
            g_message ("data offer offer, offer %p, type = %s", wl_data_offer, type));

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_formats (builder, info->targets);
  gdk_content_formats_builder_add_mime_type (builder, type);
  gdk_content_formats_unref (info->targets);
  info->targets = gdk_content_formats_builder_free (builder);
}

static inline GdkDragAction
_wl_to_gdk_actions (uint32_t dnd_actions)
{
  GdkDragAction actions = 0;

  if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
    actions |= GDK_ACTION_COPY;
  if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
    actions |= GDK_ACTION_MOVE;
  if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)
    actions |= GDK_ACTION_ASK;

  return actions;
}

static void
data_offer_source_actions (void                 *data,
                           struct wl_data_offer *wl_data_offer,
                           uint32_t              source_actions)
{
  GdkDragContext *drop_context;
  GdkDisplay *display;
  GdkDevice *device;
  GdkSeat *seat;

  display = gdk_display_get_default ();
  seat = gdk_display_get_default_seat (display);
  device = gdk_seat_get_pointer (seat);
  drop_context = gdk_wayland_device_get_drop_context (device);

  drop_context->actions = _wl_to_gdk_actions (source_actions);

  GDK_DISPLAY_NOTE (display, EVENTS,
            g_message ("data offer source actions, offer %p, actions %d", wl_data_offer, source_actions));

  if (gdk_drag_context_get_dest_window (drop_context))
    _gdk_wayland_drag_context_emit_event (drop_context, GDK_DRAG_MOTION,
                                          GDK_CURRENT_TIME);
}

static void
data_offer_action (void                 *data,
                   struct wl_data_offer *wl_data_offer,
                   uint32_t              action)
{
  GdkDragContext *drop_context;
  GdkDisplay *display;
  GdkDevice *device;
  GdkSeat *seat;

  display = gdk_display_get_default ();
  seat = gdk_display_get_default_seat (display);
  device = gdk_seat_get_pointer (seat);
  drop_context = gdk_wayland_device_get_drop_context (device);

  drop_context->action = _wl_to_gdk_actions (action);

  if (gdk_drag_context_get_dest_window (drop_context))
    _gdk_wayland_drag_context_emit_event (drop_context, GDK_DRAG_MOTION,
                                          GDK_CURRENT_TIME);
}

static const struct wl_data_offer_listener data_offer_listener = {
  data_offer_offer,
  data_offer_source_actions,
  data_offer_action
};

static SelectionData *
selection_lookup_offer_by_atom (GdkWaylandSelection *selection)
{
  return &selection->selection;
}

void
gdk_wayland_selection_ensure_offer (GdkDisplay           *display,
                                    struct wl_data_offer *wl_offer)
{
  GdkWaylandSelection *selection = gdk_wayland_display_get_selection (display);
  DataOfferData *info;

  info = g_hash_table_lookup (selection->offers, wl_offer);

  if (!info)
    {
      info = data_offer_data_new (wl_offer,
                                  (GDestroyNotify) wl_data_offer_destroy);
      g_hash_table_insert (selection->offers, wl_offer, info);
      wl_data_offer_add_listener (wl_offer,
                                  &data_offer_listener,
                                  selection);
    }
}

GdkContentFormats *
gdk_wayland_selection_steal_offer (GdkDisplay *display,
                                   gpointer    wl_offer)
{
  GdkWaylandSelection *selection = gdk_wayland_display_get_selection (display);
  GdkContentFormats *formats;
  DataOfferData *info;

  info = g_hash_table_lookup (selection->offers, wl_offer);
  if (info == NULL)
    return NULL;

  g_hash_table_steal (selection->offers, wl_offer);
  formats = info->targets;
  g_slice_free (DataOfferData, info);

  return formats;
}

void
gdk_wayland_selection_set_offer (GdkDisplay *display,
                                 gpointer    wl_offer)
{
  GdkWaylandSelection *selection = gdk_wayland_display_get_selection (display);
  struct wl_data_offer *prev_offer;
  SelectionData *selection_data;
  DataOfferData *info;

  info = g_hash_table_lookup (selection->offers, wl_offer);

  prev_offer = gdk_wayland_selection_get_offer (display);

  if (prev_offer)
    g_hash_table_remove (selection->offers, prev_offer);

  selection_data = selection_lookup_offer_by_atom (selection);

  if (selection_data)
    {
      selection_data->offer = info;
    }
}

gpointer
gdk_wayland_selection_get_offer (GdkDisplay *display)
{
  GdkWaylandSelection *selection = gdk_wayland_display_get_selection (display);
  const SelectionData *data;

  data = selection_lookup_offer_by_atom (selection);

  if (data && data->offer)
    return data->offer->offer_data;

  return NULL;
}

GdkContentFormats *
gdk_wayland_selection_get_targets (GdkDisplay *display)
{
  GdkWaylandSelection *selection = gdk_wayland_display_get_selection (display);
  const SelectionData *data;

  data = selection_lookup_offer_by_atom (selection);

  if (data && data->offer)
    return data->offer->targets;

  return NULL;
}

static void
data_source_target (void                  *data,
                    struct wl_data_source *source,
                    const char            *mime_type)
{
  GDK_NOTE (EVENTS,
            g_message ("data source target, source = %p, mime_type = %s",
                       source, mime_type));
}

static void
gdk_wayland_drag_context_write_done (GObject      *context,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GError *error = NULL;

  if (!gdk_drag_context_write_finish (GDK_DRAG_CONTEXT (context), result, &error))
    {
      GDK_DISPLAY_NOTE (gdk_drag_context_get_display (GDK_DRAG_CONTEXT (context)), DND, g_message ("%p: failed to write stream: %s", context, error->message));
      g_error_free (error);
    }
}

static void
data_source_send (void                  *data,
                  struct wl_data_source *source,
                  const char            *mime_type,
                  int32_t                fd)
{
  GdkDragContext *context;
  GOutputStream *stream;

  context = gdk_wayland_drag_context_lookup_by_data_source (source);
  if (!context)
    return;

  GDK_DISPLAY_NOTE (gdk_drag_context_get_display (context), DND, g_message ("%p: data source send request for %s on fd %d\n",
                             source, mime_type, fd));

  //mime_type = gdk_intern_mime_type (mime_type);
  mime_type = g_intern_string (mime_type);
  stream = g_unix_output_stream_new (fd, TRUE);

  gdk_drag_context_write_async (context,
                                mime_type,
                                stream,
                                G_PRIORITY_DEFAULT,
                                NULL,
                                gdk_wayland_drag_context_write_done,
                                context);
  g_object_unref (stream);
}

static void
data_source_cancelled (void                  *data,
                       struct wl_data_source *source)
{
  GdkWaylandSelection *wayland_selection = data;
  GdkDragContext *context;
  GdkDisplay *display;

  display = gdk_display_get_default ();

  GDK_DISPLAY_NOTE (display, EVENTS,
            g_message ("data source cancelled, source = %p", source));

  if (source != wayland_selection->dnd_source)
    return;

  context = gdk_wayland_drag_context_lookup_by_data_source (source);

  if (context)
    gdk_drag_context_cancel (context, GDK_DRAG_CANCEL_ERROR);

  gdk_wayland_selection_unset_data_source (display);
}

static void
data_source_dnd_drop_performed (void                  *data,
                                struct wl_data_source *source)
{
  GdkDragContext *context;

  context = gdk_wayland_drag_context_lookup_by_data_source (source);

  if (!context)
    return;

  g_signal_emit_by_name (context, "drop-performed", GDK_CURRENT_TIME);
}

static void
data_source_dnd_finished (void                  *data,
                          struct wl_data_source *source)
{
  GdkDragContext *context;

  context = gdk_wayland_drag_context_lookup_by_data_source (source);

  if (!context)
    return;

  g_signal_emit_by_name (context, "dnd-finished");
}

static void
data_source_action (void                  *data,
                    struct wl_data_source *source,
                    uint32_t               action)
{
  GdkDragContext *context;

  context = gdk_wayland_drag_context_lookup_by_data_source (source);
  if (!context)
    return;

  GDK_DISPLAY_NOTE (gdk_drag_context_get_display (context), EVENTS,
            g_message ("data source action, source = %p action=%x",
                       source, action));

  context->action = _wl_to_gdk_actions (action);
  g_signal_emit_by_name (context, "action-changed", context->action);
}

static const struct wl_data_source_listener data_source_listener = {
  data_source_target,
  data_source_send,
  data_source_cancelled,
  data_source_dnd_drop_performed,
  data_source_dnd_finished,
  data_source_action,
};

struct wl_data_source *
gdk_wayland_selection_get_data_source (GdkWindow *owner)
{
  GdkDisplay *display = gdk_window_get_display (owner);
  GdkWaylandSelection *wayland_selection = gdk_wayland_display_get_selection (display);
  gpointer source = NULL;
  GdkWaylandDisplay *display_wayland;

  if (wayland_selection->dnd_source)
    return wayland_selection->dnd_source;

  display_wayland = GDK_WAYLAND_DISPLAY (gdk_window_get_display (owner));

  source = wl_data_device_manager_create_data_source (display_wayland->data_device_manager);
  wl_data_source_add_listener (source,
                               &data_source_listener,
                               wayland_selection);

  wayland_selection->dnd_source = source;

  return source;
}

void
gdk_wayland_selection_unset_data_source (GdkDisplay *display)
{
  GdkWaylandSelection *wayland_selection = gdk_wayland_display_get_selection (display);

  wayland_selection->dnd_source = NULL;
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

/* This function has been copied straight from the x11 backend */
static gchar *
sanitize_utf8 (const gchar *src,
               gboolean return_latin1)
{
  gint len = strlen (src);
  GString *result = g_string_sized_new (len);
  const gchar *p = src;

  while (*p)
    {
      if (*p == '\r')
        {
          p++;
          if (*p == '\n')
            p++;

          g_string_append_c (result, '\n');
        }
      else
        {
          gunichar ch = g_utf8_get_char (p);

          if (!((ch < 0x20 && ch != '\t' && ch != '\n') || (ch >= 0x7f && ch < 0xa0)))
            {
              if (return_latin1)
                {
                  if (ch <= 0xff)
                    g_string_append_c (result, ch);
                  else
                    g_string_append_printf (result,
                                            ch < 0x10000 ? "\\u%04x" : "\\U%08x",
                                            ch);
                }
              else
                {
                  char buf[7];
                  gint buflen;

                  buflen = g_unichar_to_utf8 (ch, buf);
                  g_string_append_len (result, buf, buflen);
                }
            }

          p = g_utf8_next_char (p);
        }
    }

  return g_string_free (result, FALSE);
}

gchar *
_gdk_wayland_display_utf8_to_string_target (GdkDisplay  *display,
                                            const gchar *str)
{
  /* This is mainly needed when interfacing with old clients through
   * Xwayland, the STRING target could be used, and passed as-is
   * by the compositor.
   *
   * There's already some handling of this atom (aka "mimetype" in
   * this backend) in common code, so we end up in this vfunc.
   */
  return sanitize_utf8 (str, TRUE);
}

gboolean
gdk_wayland_selection_set_current_offer_actions (GdkDisplay *display,
                                                 uint32_t    action)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  struct wl_data_offer *offer;
  uint32_t all_actions = 0;

  offer = gdk_wayland_selection_get_offer (display);

  if (!offer)
    return FALSE;

  if (action != 0)
    all_actions = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
      WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE |
      WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;

  if (display_wayland->data_device_manager_version >=
      WL_DATA_OFFER_SET_ACTIONS_SINCE_VERSION)
    wl_data_offer_set_actions (offer, all_actions, action);
  return TRUE;
}
