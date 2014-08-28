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
  GdkWindow *dnd_window;
  struct wl_surface *dnd_surface;
  struct wl_data_source *data_source;
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

  if (context->is_source &&
      gdk_selection_owner_get (gdk_drag_get_selection (context)) == context->source_window)
    gdk_wayland_selection_unset_data_source (gdk_drag_get_selection (context));

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
  if (gdk_selection_owner_get (gdk_drag_get_selection (context)))
    gdk_wayland_selection_unset_data_source (gdk_drag_get_selection (context));
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

static GdkWindow *
create_dnd_window (void)
{
  GdkWindowAttr attrs;
  GdkScreen *screen;
  guint mask;

  screen = gdk_display_get_default_screen (gdk_display_get_default ());

  attrs.x = attrs.y = 0;
  attrs.width = attrs.height = 100;
  attrs.wclass = GDK_INPUT_OUTPUT;
  attrs.window_type = GDK_WINDOW_TEMP;
  attrs.type_hint = GDK_WINDOW_TYPE_HINT_DND;
  attrs.visual = gdk_screen_get_system_visual (screen);

  mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_TYPE_HINT;

  return gdk_window_new (gdk_screen_get_root_window (screen), &attrs, mask);
}

GdkDragContext *
_gdk_wayland_window_drag_begin (GdkWindow *window,
				GdkDevice *device,
				GList     *targets)
{
  GdkWaylandDragContext *context_wayland;
  GdkWaylandDisplay *display_wayland;
  GdkDragContext *context;
  GdkWindow *toplevel;
  GList *l;

  toplevel = gdk_device_get_window_at_position (device, NULL, NULL);

  context_wayland = g_object_new (GDK_TYPE_WAYLAND_DRAG_CONTEXT, NULL);
  context = GDK_DRAG_CONTEXT (context_wayland);
  context->source_window = g_object_ref (window);
  context->is_source = TRUE;
  context->targets = g_list_copy (targets);

  gdk_drag_context_set_device (context, device);
  display_wayland = GDK_WAYLAND_DISPLAY (gdk_device_get_display (device));

  context_wayland->dnd_window = create_dnd_window ();
  context_wayland->dnd_surface = gdk_wayland_window_get_wl_surface (context_wayland->dnd_window);
  context_wayland->data_source =
    gdk_wayland_selection_get_data_source (window,
                                           gdk_wayland_drag_context_get_selection (context));

  for (l = context->targets; l; l = l->next)
    wl_data_source_offer (context_wayland->data_source, gdk_atom_name (l->data));

  wl_data_device_start_drag (gdk_wayland_device_get_data_device (device),
                             context_wayland->data_source,
                             gdk_wayland_window_get_wl_surface (toplevel),
			     context_wayland->dnd_surface,
                             _gdk_wayland_display_get_serial (display_wayland));

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

GdkDragContext *
gdk_wayland_drag_context_lookup_by_source_window (GdkWindow *window)
{
  GList *l;

  for (l = contexts; l; l = l->next)
    {
      if (window == gdk_drag_context_get_source_window (l->data))
        return l->data;
    }

  return NULL;
}

struct wl_data_source *
gdk_wayland_drag_context_get_data_source (GdkDragContext *context)
{
  return GDK_WAYLAND_DRAG_CONTEXT (context)->data_source;
}

void
gdk_wayland_drag_context_undo_grab (GdkDragContext *context)
{
  GdkEventSequence *sequence;
  GdkModifierType state;
  GdkDevice *device;
  GdkEvent *event;
  guint button;
  gdouble x, y;

  device = gdk_drag_context_get_device (context);
  _gdk_wayland_device_get_last_implicit_grab_serial (GDK_WAYLAND_DEVICE (device), &sequence);
  gdk_window_get_device_position_double (gdk_drag_context_get_source_window (context),
                                         device, &x, &y, &state);

  if (sequence)
    {
      event = gdk_event_new (GDK_TOUCH_END);
      event->touch.window = g_object_ref (gdk_drag_context_get_source_window (context));
      event->touch.send_event = TRUE;
      event->touch.sequence = sequence;
      event->touch.time = GDK_CURRENT_TIME;
      event->touch.x = event->touch.x_root = x;
      event->touch.y = event->touch.y_root = y;
    }
  else if (state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK))
    {
      if (state & GDK_BUTTON1_MASK)
        button = 1;
      else if (state & GDK_BUTTON2_MASK)
        button = 2;
      else if (state & GDK_BUTTON3_MASK)
        button = 3;
      else
        return;

      event = gdk_event_new (GDK_BUTTON_RELEASE);
      event->button.window = g_object_ref (gdk_drag_context_get_source_window (context));
      event->button.send_event = TRUE;
      event->button.button = button;
      event->button.time = GDK_CURRENT_TIME;
      event->button.x = event->button.x_root = x;
      event->button.y = event->button.x_root = y;
    }
  else
    return;

  gdk_event_set_device (event, device);
  gdk_event_set_source_device (event, device);

  _gdk_wayland_display_deliver_event (gdk_device_get_display (device), event);
}

GdkWindow *
gdk_wayland_drag_context_get_dnd_window (GdkDragContext *context)
{
  GdkWaylandDragContext *wayland_context;

  wayland_context = GDK_WAYLAND_DRAG_CONTEXT (context);
  return wayland_context->dnd_window;
}
