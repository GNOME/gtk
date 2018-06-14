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

#include "gdkdndprivate.h"

#include "gdkinternals.h"
#include "gdkproperty.h"
#include "gdkprivate-wayland.h"
#include "gdkcontentformats.h"
#include "gdkdisplay-wayland.h"
#include "gdkintl.h"
#include "gdkseat-wayland.h"

#include "gdkdeviceprivate.h"

#include <glib-unix.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <string.h>

#define GDK_TYPE_WAYLAND_DRAG_CONTEXT              (gdk_wayland_drag_context_get_type ())
#define GDK_WAYLAND_DRAG_CONTEXT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_DRAG_CONTEXT, GdkWaylandDragContext))
#define GDK_WAYLAND_DRAG_CONTEXT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WAYLAND_DRAG_CONTEXT, GdkWaylandDragContextClass))
#define GDK_IS_WAYLAND_DRAG_CONTEXT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_DRAG_CONTEXT))
#define GDK_IS_WAYLAND_DRAG_CONTEXT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WAYLAND_DRAG_CONTEXT))
#define GDK_WAYLAND_DRAG_CONTEXT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WAYLAND_DRAG_CONTEXT, GdkWaylandDragContextClass))

typedef struct _GdkWaylandDragContext GdkWaylandDragContext;
typedef struct _GdkWaylandDragContextClass GdkWaylandDragContextClass;

struct _GdkWaylandDragContext
{
  GdkDragContext context;
  GdkSurface *dnd_surface;
  struct wl_surface *dnd_wl_surface;
  struct wl_data_source *data_source;
  struct wl_data_offer *offer;
  GdkDragAction selected_action;
  uint32_t serial;
  gint hot_x;
  gint hot_y;
};

struct _GdkWaylandDragContextClass
{
  GdkDragContextClass parent_class;
};

static GList *contexts;

GType gdk_wayland_drag_context_get_type (void);

G_DEFINE_TYPE (GdkWaylandDragContext, gdk_wayland_drag_context, GDK_TYPE_DRAG_CONTEXT)

static void
gdk_wayland_drag_context_finalize (GObject *object)
{
  GdkWaylandDragContext *wayland_context = GDK_WAYLAND_DRAG_CONTEXT (object);
  GdkDragContext *context = GDK_DRAG_CONTEXT (object);
  GdkSurface *dnd_surface;

  contexts = g_list_remove (contexts, context);

  if (context->is_source)
    {
      gdk_drag_context_set_cursor (context, NULL);
    }

  g_clear_pointer (&wayland_context->data_source, (GDestroyNotify) wl_data_source_destroy);
  g_clear_pointer (&wayland_context->offer, (GDestroyNotify) wl_data_offer_destroy);

  dnd_surface = wayland_context->dnd_surface;

  G_OBJECT_CLASS (gdk_wayland_drag_context_parent_class)->finalize (object);

  if (dnd_surface)
    gdk_surface_destroy (dnd_surface);
}

static inline uint32_t
gdk_to_wl_actions (GdkDragAction action)
{
  uint32_t dnd_actions = 0;

  if (action & (GDK_ACTION_COPY | GDK_ACTION_LINK))
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
  if (action & GDK_ACTION_MOVE)
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
  if (action & GDK_ACTION_ASK)
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;

  return dnd_actions;
}

static void
gdk_wayland_drag_context_drag_abort (GdkDragContext *context,
				     guint32         time)
{
}

static void
gdk_wayland_drag_context_drag_drop (GdkDragContext *context,
				    guint32         time)
{
}

/* Destination side */

static void
gdk_wayland_drop_context_set_status (GdkWaylandDragContext *context_wayland,
                                     gboolean               accepted)
{
  if (accepted)
    {
      const char *const *mimetypes;
      gsize i, n_mimetypes;
      
      mimetypes = gdk_content_formats_get_mime_types (gdk_drop_get_formats (GDK_DROP (context_wayland)), &n_mimetypes);
      for (i = 0; i < n_mimetypes; i++)
        {
          if (mimetypes[i] != g_intern_static_string ("DELETE"))
            break;
        }

      if (i < n_mimetypes)
        {
          wl_data_offer_accept (context_wayland->offer, context_wayland->serial, mimetypes[i]);
          return;
        }
    }

  wl_data_offer_accept (context_wayland->offer, context_wayland->serial, NULL);
}

static void
gdk_wayland_drag_context_commit_status (GdkWaylandDragContext *wayland_context)
{
  GdkDisplay *display;
  uint32_t dnd_actions;

  display = gdk_drop_get_display (GDK_DROP (wayland_context));

  dnd_actions = gdk_to_wl_actions (wayland_context->selected_action);

  if (GDK_WAYLAND_DISPLAY (display)->data_device_manager_version >=
      WL_DATA_OFFER_SET_ACTIONS_SINCE_VERSION)
    {
      if (gdk_drag_action_is_unique (wayland_context->selected_action))
        {
          wl_data_offer_set_actions (wayland_context->offer, dnd_actions, dnd_actions);
        }
      else
        {
          wl_data_offer_set_actions (wayland_context->offer,
                                     dnd_actions | WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK,
                                     WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK);
        }
    }

  gdk_wayland_drop_context_set_status (wayland_context, wayland_context->selected_action != 0);
}

static void
gdk_wayland_drag_context_status (GdkDrop       *drop,
				 GdkDragAction  action)
{
  GdkWaylandDragContext *wayland_context;

  wayland_context = GDK_WAYLAND_DRAG_CONTEXT (drop);
  wayland_context->selected_action = action;

  gdk_wayland_drag_context_commit_status (wayland_context);
}

static void
gdk_wayland_drag_context_finish (GdkDrop       *drop,
                                 GdkDragAction  action)
{
  GdkWaylandDragContext *wayland_context = GDK_WAYLAND_DRAG_CONTEXT (drop);
  GdkDisplay *display = gdk_drop_get_display (drop);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  wayland_context->selected_action = action;

  if (action)
    {
      gdk_wayland_drag_context_commit_status (wayland_context);

      if (display_wayland->data_device_manager_version >=
          WL_DATA_OFFER_FINISH_SINCE_VERSION)
        wl_data_offer_finish (wayland_context->offer);
    }
}

static void
gdk_wayland_drag_context_read_async (GdkDrop             *drop,
                                     GdkContentFormats   *formats,
                                     int                  io_priority,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  GdkWaylandDragContext *wayland_context = GDK_WAYLAND_DRAG_CONTEXT (drop);
  GdkDisplay *display;
  GInputStream *stream;
  const char *mime_type;
  int pipe_fd[2];
  GError *error = NULL;
  GTask *task;

  display = gdk_drop_get_display (drop),
  task = g_task_new (drop, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_wayland_drag_context_read_async);

  GDK_DISPLAY_NOTE (display, DND, char *s = gdk_content_formats_to_string (formats);
                 g_message ("%p: read for %s", drop, s);
                 g_free (s); );
  mime_type = gdk_content_formats_match_mime_type (formats,
                                                   gdk_drop_get_formats (drop));
  if (mime_type == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("No compatible transfer format found"));
      return;
    }

  g_task_set_task_data (task, (gpointer) mime_type, NULL);

  if (!g_unix_open_pipe (pipe_fd, FD_CLOEXEC, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  wl_data_offer_receive (wayland_context->offer, mime_type, pipe_fd[1]);
  stream = g_unix_input_stream_new (pipe_fd[0], TRUE);
  close (pipe_fd[1]);
  g_task_return_pointer (task, stream, g_object_unref);
}

static GInputStream *
gdk_wayland_drag_context_read_finish (GdkDrop         *drop,
                                      const char     **out_mime_type,
                                      GAsyncResult    *result,
                                      GError         **error)
{
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (result, G_OBJECT (drop)), NULL);
  task = G_TASK (result);
  g_return_val_if_fail (g_task_get_source_tag (task) == gdk_wayland_drag_context_read_async, NULL);

  if (out_mime_type)
    *out_mime_type = g_task_get_task_data (task);

  return g_task_propagate_pointer (task, error);
}

static void
gdk_wayland_drag_context_init (GdkWaylandDragContext *context_wayland)
{
  GdkDragContext *context;

  context = GDK_DRAG_CONTEXT (context_wayland);
  contexts = g_list_prepend (contexts, context);

  context->action = GDK_ACTION_COPY;
}

static GdkSurface *
gdk_wayland_drag_context_get_drag_surface (GdkDragContext *context)
{
  return GDK_WAYLAND_DRAG_CONTEXT (context)->dnd_surface;
}

static void
gdk_wayland_drag_context_set_hotspot (GdkDragContext *context,
                                      gint            hot_x,
                                      gint            hot_y)
{
  GdkWaylandDragContext *context_wayland = GDK_WAYLAND_DRAG_CONTEXT (context);
  gint prev_hot_x = context_wayland->hot_x;
  gint prev_hot_y = context_wayland->hot_y;
  const GdkRectangle damage_rect = { .width = 1, .height = 1 };

  context_wayland->hot_x = hot_x;
  context_wayland->hot_y = hot_y;

  if (prev_hot_x == hot_x && prev_hot_y == hot_y)
    return;

  _gdk_wayland_surface_offset_next_wl_buffer (context_wayland->dnd_surface,
                                             -hot_x, -hot_y);
  gdk_surface_invalidate_rect (context_wayland->dnd_surface, &damage_rect);
}

static void
gdk_wayland_drag_context_set_cursor (GdkDragContext *context,
                                     GdkCursor      *cursor)
{
  GdkDevice *device = gdk_drag_context_get_device (context);

  gdk_wayland_seat_set_global_cursor (gdk_device_get_seat (device), cursor);
}

static void
gdk_wayland_drag_context_action_changed (GdkDragContext *context,
                                         GdkDragAction   action)
{
  GdkCursor *cursor;

  cursor = gdk_drag_get_cursor (context, action);
  gdk_drag_context_set_cursor (context, cursor);
}

static void
gdk_wayland_drag_context_drop_performed (GdkDragContext *context,
                                         guint32         time_)
{
  gdk_drag_context_set_cursor (context, NULL);
}

static void
gdk_wayland_drag_context_cancel (GdkDragContext      *context,
                                 GdkDragCancelReason  reason)
{
  gdk_drag_context_set_cursor (context, NULL);
}

static void
gdk_wayland_drag_context_drop_done (GdkDragContext *context,
                                    gboolean        success)
{
  GdkWaylandDragContext *context_wayland = GDK_WAYLAND_DRAG_CONTEXT (context);

  if (success)
    {
      if (context_wayland->dnd_surface)
        gdk_surface_hide (context_wayland->dnd_surface);
    }
}

static void
gdk_wayland_drag_context_class_init (GdkWaylandDragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDropClass *drop_class = GDK_DROP_CLASS (klass);
  GdkDragContextClass *context_class = GDK_DRAG_CONTEXT_CLASS (klass);

  object_class->finalize = gdk_wayland_drag_context_finalize;

  drop_class->status = gdk_wayland_drag_context_status;
  drop_class->finish = gdk_wayland_drag_context_finish;
  drop_class->read_async = gdk_wayland_drag_context_read_async;
  drop_class->read_finish = gdk_wayland_drag_context_read_finish;

  context_class->drag_abort = gdk_wayland_drag_context_drag_abort;
  context_class->drag_drop = gdk_wayland_drag_context_drag_drop;
  context_class->get_drag_surface = gdk_wayland_drag_context_get_drag_surface;
  context_class->set_hotspot = gdk_wayland_drag_context_set_hotspot;
  context_class->drop_done = gdk_wayland_drag_context_drop_done;
  context_class->set_cursor = gdk_wayland_drag_context_set_cursor;
  context_class->action_changed = gdk_wayland_drag_context_action_changed;
  context_class->drop_performed = gdk_wayland_drag_context_drop_performed;
  context_class->cancel = gdk_wayland_drag_context_cancel;
}

void
_gdk_wayland_surface_register_dnd (GdkSurface *surface)
{
}

static GdkSurface *
create_dnd_surface (GdkDisplay *display)
{
  GdkSurface *surface;

  surface = gdk_surface_new_popup (display, &(GdkRectangle) { 0, 0, 100, 100 });

  gdk_surface_set_type_hint (surface, GDK_SURFACE_TYPE_HINT_DND);
  
  return surface;
}

GdkDragContext *
_gdk_wayland_surface_drag_begin (GdkSurface          *surface,
				GdkDevice          *device,
	                        GdkContentProvider *content,
                                GdkDragAction       actions,
                                gint                dx,
                                gint                dy)
{
  GdkWaylandDragContext *context_wayland;
  GdkDragContext *context;
  GdkWaylandDisplay *display_wayland;
  const char *const *mimetypes;
  gsize i, n_mimetypes;

  display_wayland = GDK_WAYLAND_DISPLAY (gdk_device_get_display (device));

  context_wayland = g_object_new (GDK_TYPE_WAYLAND_DRAG_CONTEXT,
                                  "device", device,
                                  "content", content,
                                  NULL);
  context = GDK_DRAG_CONTEXT (context_wayland);
  context->source_surface = g_object_ref (surface);
  context->is_source = TRUE;

  context_wayland->dnd_surface = create_dnd_surface (gdk_surface_get_display (surface));
  context_wayland->dnd_wl_surface = gdk_wayland_surface_get_wl_surface (context_wayland->dnd_surface);
  context_wayland->data_source = gdk_wayland_selection_get_data_source (surface);

  mimetypes = gdk_content_formats_get_mime_types (gdk_drag_context_get_formats (context), &n_mimetypes);
  for (i = 0; i < n_mimetypes; i++)
    {
      wl_data_source_offer (context_wayland->data_source, mimetypes[i]);
    }

  if (display_wayland->data_device_manager_version >=
      WL_DATA_SOURCE_SET_ACTIONS_SINCE_VERSION)
    {
      wl_data_source_set_actions (context_wayland->data_source,
                                  gdk_to_wl_actions (actions));
    }

  wl_data_device_start_drag (gdk_wayland_device_get_data_device (device),
                             context_wayland->data_source,
                             gdk_wayland_surface_get_wl_surface (surface),
			     context_wayland->dnd_wl_surface,
                             _gdk_wayland_display_get_serial (display_wayland));

  gdk_seat_ungrab (gdk_device_get_seat (device));

  return context;
}

void
_gdk_wayland_drag_context_set_source_surface (GdkDragContext *context,
                                             GdkSurface      *surface)
{
  if (context->source_surface)
    g_object_unref (context->source_surface);

  context->source_surface = surface ? g_object_ref (surface) : NULL;
}

GdkDragContext *
gdk_wayland_drag_context_lookup_by_data_source (struct wl_data_source *source)
{
  GList *l;

  for (l = contexts; l; l = l->next)
    {
      GdkWaylandDragContext *wayland_context = l->data;

      if (wayland_context->data_source == source)
        return l->data;
    }

  return NULL;
}

