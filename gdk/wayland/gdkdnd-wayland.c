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

#include "gdkmain.h"
#include "gdkinternals.h"
#include "gdkproperty.h"
#include "gdkprivate-wayland.h"
#include "gdkdisplay-wayland.h"

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
  struct wl_data_offer *offer;
  uint32_t serial;
  gdouble x;
  gdouble y;
};

struct _GdkWaylandDragContextClass
{
  GdkDragContextClass parent_class;
};

static GList *contexts;

G_DEFINE_TYPE (GdkWaylandDragContext, gdk_wayland_drag_context, GDK_TYPE_DRAG_CONTEXT)

static void
gdk_wayland_drag_context_finalize (GObject *object)
{
  GdkWaylandDragContext *wayland_context = GDK_WAYLAND_DRAG_CONTEXT (object);
  GdkDragContext *context = GDK_DRAG_CONTEXT (object);

  contexts = g_list_remove (contexts, context);

  if (wayland_context->data_source)
    wl_data_source_destroy (wayland_context->data_source);

  if (wayland_context->dnd_window)
    gdk_window_destroy (wayland_context->dnd_window);

  G_OBJECT_CLASS (gdk_wayland_drag_context_parent_class)->finalize (object);
}

void
_gdk_wayland_drag_context_emit_event (GdkDragContext *context,
                                      GdkEventType    type,
                                      guint32         time_)
{
  GdkWindow *window;
  GdkEvent *event;

  switch (type)
    {
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
      break;
    default:
      return;
    }

  if (context->is_source)
    window = gdk_drag_context_get_source_window (context);
  else
    window = gdk_drag_context_get_dest_window (context);

  event = gdk_event_new (type);
  event->dnd.window = g_object_ref (window);
  event->dnd.context = g_object_ref (context);
  event->dnd.time = time_;
  event->dnd.x_root = GDK_WAYLAND_DRAG_CONTEXT (context)->x;
  event->dnd.y_root = GDK_WAYLAND_DRAG_CONTEXT (context)->y;
  gdk_event_set_device (event, gdk_drag_context_get_device (context));

  gdk_event_put (event);
  gdk_event_free (event);
}

static GdkWindow *
gdk_wayland_drag_context_find_window (GdkDragContext  *context,
				      GdkWindow       *drag_window,
				      GdkScreen       *screen,
				      gint             x_root,
				      gint             y_root,
				      GdkDragProtocol *protocol)
{
  GdkDevice *device;
  GdkWindow *window;

  device = gdk_drag_context_get_device (context);
  window = gdk_device_get_window_at_position (device, NULL, NULL);

  if (window)
    {
      window = gdk_window_get_toplevel (window);
      *protocol = GDK_DRAG_PROTO_WAYLAND;
      return g_object_ref (window);
    }

  return NULL;
}

void
gdk_wayland_drag_context_set_action (GdkDragContext *context,
                                     GdkDragAction   action)
{
  context->suggested_action = context->action = action;
}

static gboolean
gdk_wayland_drag_context_drag_motion (GdkDragContext *context,
				      GdkWindow      *dest_window,
				      GdkDragProtocol protocol,
				      gint            x_root,
				      gint            y_root,
				      GdkDragAction   suggested_action,
				      GdkDragAction   possible_actions,
				      guint32         time)
{
  if (context->dest_window != dest_window)
    {
      context->dest_window = dest_window ? g_object_ref (dest_window) : NULL;
      _gdk_wayland_drag_context_set_coords (context, x_root, y_root);
      _gdk_wayland_drag_context_emit_event (context, GDK_DRAG_STATUS, time);
    }

  gdk_wayland_drag_context_set_action (context, suggested_action);

  return context->dest_window != NULL;
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
gdk_wayland_drop_context_set_status (GdkDragContext *context,
                                     gboolean        accepted)
{
  GdkWaylandDragContext *context_wayland;
  struct wl_data_offer *wl_offer;

  context_wayland = GDK_WAYLAND_DRAG_CONTEXT (context);
  wl_offer = gdk_wayland_selection_get_offer ();

  if (!wl_offer)
    return;

  if (accepted)
    {
      GList *l;

      for (l = context->targets; l; l = l->next)
        {
          if (l->data != gdk_atom_intern_static_string ("DELETE"))
            break;
        }

      if (l)
        {
          wl_data_offer_accept (wl_offer, context_wayland->serial,
                                gdk_atom_name (l->data));
          return;
        }
    }

  wl_data_offer_accept (wl_offer, context_wayland->serial, NULL);
}

static void
gdk_wayland_drag_context_drag_status (GdkDragContext *context,
				      GdkDragAction   action,
				      guint32         time_)
{
  gdk_wayland_drop_context_set_status (context, action != 0);
}

static void
gdk_wayland_drag_context_drop_reply (GdkDragContext *context,
				     gboolean        accepted,
				     guint32         time_)
{
  gdk_wayland_drop_context_set_status (context, accepted);
}

static void
gdk_wayland_drag_context_drop_finish (GdkDragContext *context,
				      gboolean        success,
				      guint32         time)
{
}

static gboolean
gdk_wayland_drag_context_drop_status (GdkDragContext *context)
{
  return FALSE;
}

static GdkAtom
gdk_wayland_drag_context_get_selection (GdkDragContext *context)
{
  return gdk_atom_intern_static_string ("GdkWaylandSelection");
}

static void
gdk_wayland_drag_context_init (GdkWaylandDragContext *context_wayland)
{
  GdkDragContext *context;

  context = GDK_DRAG_CONTEXT (context_wayland);
  contexts = g_list_prepend (contexts, context);

  context->action = GDK_ACTION_COPY;
  context->suggested_action = GDK_ACTION_COPY;
  context->actions = GDK_ACTION_COPY | GDK_ACTION_MOVE;
}

static void
gdk_wayland_drag_context_class_init (GdkWaylandDragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDragContextClass *context_class = GDK_DRAG_CONTEXT_CLASS (klass);

  object_class->finalize = gdk_wayland_drag_context_finalize;

  context_class->find_window = gdk_wayland_drag_context_find_window;
  context_class->drag_status = gdk_wayland_drag_context_drag_status;
  context_class->drag_motion = gdk_wayland_drag_context_drag_motion;
  context_class->drag_abort = gdk_wayland_drag_context_drag_abort;
  context_class->drag_drop = gdk_wayland_drag_context_drag_drop;
  context_class->drop_reply = gdk_wayland_drag_context_drop_reply;
  context_class->drop_finish = gdk_wayland_drag_context_drop_finish;
  context_class->drop_status = gdk_wayland_drag_context_drop_status;
  context_class->get_selection = gdk_wayland_drag_context_get_selection;
}

GdkDragProtocol
_gdk_wayland_window_get_drag_protocol (GdkWindow *window, GdkWindow **target)
{
  return GDK_DRAG_PROTO_WAYLAND;
}

void
_gdk_wayland_window_register_dnd (GdkWindow *window)
{
}

GdkDragContext *
_gdk_wayland_window_drag_begin (GdkWindow *window,
				GdkDevice *device,
				GList     *targets)
{
  GdkDragContext *context;

  context = (GdkDragContext *) g_object_new (GDK_TYPE_WAYLAND_DRAG_CONTEXT, NULL);
  context->source_window = window;
  context->is_source = TRUE;

  gdk_drag_context_set_device (context, device);

  return context;
}

GdkDragContext *
_gdk_wayland_drop_context_new (GdkDevice             *device,
                               struct wl_data_device *data_device)
{
  GdkWaylandDragContext *context_wayland;
  GdkDragContext *context;

  context_wayland = g_object_new (GDK_TYPE_WAYLAND_DRAG_CONTEXT, NULL);
  context = GDK_DRAG_CONTEXT (context_wayland);
  context->is_source = FALSE;

  gdk_drag_context_set_device (context, device);

  return context;
}

void
gdk_wayland_drop_context_update_targets (GdkDragContext *context)
{
  g_list_free (context->targets);
  context->targets = g_list_copy (gdk_wayland_selection_get_targets ());
}

void
_gdk_wayland_drag_context_set_coords (GdkDragContext *context,
                                      gdouble         x,
                                      gdouble         y)
{
  GdkWaylandDragContext *context_wayland;

  context_wayland = GDK_WAYLAND_DRAG_CONTEXT (context);
  context_wayland->x = x;
  context_wayland->y = y;
}

void
_gdk_wayland_drag_context_set_source_window (GdkDragContext *context,
                                             GdkWindow      *window)
{
  if (context->source_window)
    g_object_unref (context->source_window);

  context->source_window = window ? g_object_ref (window) : NULL;
}

void
_gdk_wayland_drag_context_set_dest_window (GdkDragContext *context,
                                           GdkWindow      *dest_window,
                                           uint32_t        serial)
{
  context->dest_window = dest_window ? g_object_ref (dest_window) : NULL;
  GDK_WAYLAND_DRAG_CONTEXT (context)->serial = serial;
  gdk_wayland_drop_context_update_targets (context);
}
