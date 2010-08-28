/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.
 */

/*
 * GTK+ DirectFB backend
 * Copyright (C) 2001-2002  convergence integrated media GmbH
 * Copyright (C) 2002-2004  convergence GmbH
 * Written by Denis Oliver Kropp <dok@convergence.de> and
 *            Sven Neumann <sven@convergence.de>
 */

#include "config.h"
#include "gdk.h"
#include "gdkdirectfb.h"
#include "gdkprivate-directfb.h"

#include "gdkdnd.h"
#include "gdkproperty.h"
#include "gdkalias.h"

typedef struct _GdkDragContextPrivate GdkDragContextPrivate;

typedef enum
{
  GDK_DRAG_STATUS_DRAG,
  GDK_DRAG_STATUS_MOTION_WAIT,
  GDK_DRAG_STATUS_ACTION_WAIT,
  GDK_DRAG_STATUS_DROP
} GtkDragStatus;

/* Structure that holds information about a drag in progress.
 * this is used on both source and destination sides.
 */
struct _GdkDragContextPrivate
{
  GdkAtom local_selection;

  guint16 last_x;		/* Coordinates from last event */
  guint16 last_y;
  guint   drag_status : 4;	/* current status of drag      */
};

/* Drag Contexts */

static GList          *contexts          = NULL;
static GdkDragContext *current_dest_drag = NULL;


#define GDK_DRAG_CONTEXT_PRIVATE_DATA(ctx) ((GdkDragContextPrivate *) GDK_DRAG_CONTEXT (ctx)->windowing_data)

static void gdk_drag_context_finalize (GObject *object);

G_DEFINE_TYPE (GdkDragContext, gdk_drag_context, G_TYPE_OBJECT)

static void
gdk_drag_context_init (GdkDragContext *dragcontext)
{
  GdkDragContextPrivate *private;

  private = G_TYPE_INSTANCE_GET_PRIVATE (dragcontext,
                                         GDK_TYPE_DRAG_CONTEXT,
                                         GdkDragContextPrivate);

  dragcontext->windowing_data = private;

  contexts = g_list_prepend (contexts, dragcontext);
}

static void
gdk_drag_context_class_init (GdkDragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_drag_context_finalize;

  g_type_class_add_private (object_class, sizeof (GdkDragContextPrivate));
}

static void
gdk_drag_context_finalize (GObject *object)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (object);

  g_list_free (context->targets);

  if (context->source_window)
    g_object_unref (context->source_window);

  if (context->dest_window)
    g_object_unref (context->dest_window);

  contexts = g_list_remove (contexts, context);

  G_OBJECT_CLASS (gdk_drag_context_parent_class)->finalize (object);
}

GdkDragContext *
gdk_drag_context_new (void)
{
  return g_object_new (gdk_drag_context_get_type (), NULL);
}

void
gdk_drag_context_ref (GdkDragContext *context)
{
  g_object_ref (context);
}

void
gdk_drag_context_unref (GdkDragContext *context)
{
  g_object_unref (context);
}

static GdkDragContext *
gdk_drag_context_find (gboolean     is_source,
		       GdkWindow   *source,
		       GdkWindow   *dest)
{
  GdkDragContext        *context;
  GdkDragContextPrivate *private;
  GList                 *list;

  for (list = contexts; list; list = list->next)
    {
      context = (GdkDragContext *) list->data;
      private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);

      if ((!context->is_source == !is_source) &&
	  ((source == NULL) ||
           (context->source_window && (context->source_window == source))) &&
	  ((dest == NULL) ||
           (context->dest_window && (context->dest_window == dest))))
	  return context;
    }

  return NULL;
}


/************************** Public API ***********************/

void
_gdk_dnd_init (void)
{
}

/* Source side */

static void
local_send_leave (GdkDragContext  *context,
		  guint32          time)
{
  GdkEvent event;

  if ((current_dest_drag != NULL) &&
      (current_dest_drag->protocol == GDK_DRAG_PROTO_LOCAL) &&
      (current_dest_drag->source_window == context->source_window))
    {
      event.dnd.type       = GDK_DRAG_LEAVE;
      event.dnd.window     = context->dest_window;
      /* Pass ownership of context to the event */
      event.dnd.context    = current_dest_drag;
      event.dnd.send_event = FALSE;
      event.dnd.time       = time;

      current_dest_drag = NULL;

      gdk_event_put (&event);
    }
}

static void
local_send_enter (GdkDragContext *context,
		  guint32         time)
{
  GdkDragContextPrivate *private;
  GdkDragContext        *new_context;
  GdkEvent event;

  private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);

  if (!private->local_selection)
    private->local_selection = gdk_atom_intern ("LocalDndSelection", FALSE);

  if (current_dest_drag != NULL)
    {
      g_object_unref (current_dest_drag);
      current_dest_drag = NULL;
    }

  new_context = gdk_drag_context_new ();
  new_context->protocol  = GDK_DRAG_PROTO_LOCAL;
  new_context->is_source = FALSE;

  new_context->source_window = g_object_ref (context->source_window);

  new_context->dest_window   = g_object_ref (context->dest_window);

  new_context->targets = g_list_copy (context->targets);

  gdk_window_set_events (new_context->source_window,
			 gdk_window_get_events (new_context->source_window) |
			 GDK_PROPERTY_CHANGE_MASK);
  new_context->actions = context->actions;

  event.dnd.type       = GDK_DRAG_ENTER;
  event.dnd.window     = context->dest_window;
  event.dnd.send_event = FALSE;
  event.dnd.context    = new_context;
  event.dnd.time       = time;

  current_dest_drag = new_context;

  (GDK_DRAG_CONTEXT_PRIVATE_DATA (new_context))->local_selection =
    private->local_selection;

  gdk_event_put (&event);
}

static void
local_send_motion (GdkDragContext  *context,
		    gint            x_root,
		    gint            y_root,
		    GdkDragAction   action,
		    guint32         time)
{
  GdkEvent event;

  if ((current_dest_drag != NULL) &&
      (current_dest_drag->protocol == GDK_DRAG_PROTO_LOCAL) &&
      (current_dest_drag->source_window == context->source_window))
    {
      event.dnd.type       = GDK_DRAG_MOTION;
      event.dnd.window     = current_dest_drag->dest_window;
      event.dnd.send_event = FALSE;
      event.dnd.context    = current_dest_drag;
      event.dnd.time       = time;
      event.dnd.x_root     = x_root;
      event.dnd.y_root     = y_root;

      current_dest_drag->suggested_action = action;
      current_dest_drag->actions          = action;

      (GDK_DRAG_CONTEXT_PRIVATE_DATA (current_dest_drag))->last_x = x_root;
      (GDK_DRAG_CONTEXT_PRIVATE_DATA (current_dest_drag))->last_y = y_root;

      GDK_DRAG_CONTEXT_PRIVATE_DATA (context)->drag_status = GDK_DRAG_STATUS_MOTION_WAIT;

      gdk_event_put (&event);
    }
}

static void
local_send_drop (GdkDragContext *context,
                 guint32         time)
{
  GdkEvent event;

  if ((current_dest_drag != NULL) &&
      (current_dest_drag->protocol == GDK_DRAG_PROTO_LOCAL) &&
      (current_dest_drag->source_window == context->source_window))
    {
      GdkDragContextPrivate *private;
      private = GDK_DRAG_CONTEXT_PRIVATE_DATA (current_dest_drag);

      event.dnd.type       = GDK_DROP_START;
      event.dnd.window     = current_dest_drag->dest_window;
      event.dnd.send_event = FALSE;
      event.dnd.context    = current_dest_drag;
      event.dnd.time       = time;
      event.dnd.x_root     = private->last_x;
      event.dnd.y_root     = private->last_y;

      gdk_event_put (&event);
    }
}

static void
gdk_drag_do_leave (GdkDragContext *context,
                   guint32         time)
{
  if (context->dest_window)
    {
      switch (context->protocol)
	{
	case GDK_DRAG_PROTO_LOCAL:
	  local_send_leave (context, time);
	  break;

	default:
	  break;
	}

      g_object_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

GdkDragContext *
gdk_drag_begin (GdkWindow *window,
		GList     *targets)
{
  GList          *list;
  GdkDragContext *new_context;

  g_return_val_if_fail (window != NULL, NULL);

  g_object_ref (window);

  new_context = gdk_drag_context_new ();
  new_context->is_source     = TRUE;
  new_context->source_window = window;
  new_context->targets       = NULL;
  new_context->actions       = 0;

  for (list = targets; list; list = list->next)
    new_context->targets = g_list_append (new_context->targets, list->data);

  return new_context;
}

guint32
gdk_drag_get_protocol_for_display(GdkDisplay *display, guint32          xid,
                                   GdkDragProtocol *protocol)
{
  GdkWindow *window;

  window = gdk_window_lookup ((GdkNativeWindow) xid);

  if (window &&
      GPOINTER_TO_INT (gdk_drawable_get_data (window, "gdk-dnd-registered")))
    {
      *protocol = GDK_DRAG_PROTO_LOCAL;
      return xid;
    }

  *protocol = GDK_DRAG_PROTO_NONE;
  return 0;
}

void
gdk_drag_find_window_for_screen (GdkDragContext   *context,
                                 GdkWindow        *drag_window,
                                 GdkScreen        *screen,
                                 gint              x_root,
                                 gint              y_root,
                                 GdkWindow       **dest_window,
                                 GdkDragProtocol  *protocol)
{
  GdkWindow *dest;

  g_return_if_fail (context != NULL);

  dest = gdk_window_get_pointer (NULL, &x_root, &y_root, NULL);

  if (context->dest_window != dest)
    {
      guint32 recipient;

      /* Check if new destination accepts drags, and which protocol */
      if ((recipient = gdk_drag_get_protocol (GDK_WINDOW_DFB_ID (dest),
                                              protocol)))
	{
	  *dest_window = gdk_window_lookup ((GdkNativeWindow) recipient);
	  if (dest_window)
            g_object_ref (*dest_window);
	}
      else
	*dest_window = NULL;
    }
  else
    {
      *dest_window = context->dest_window;
      if (*dest_window)
	g_object_ref (*dest_window);

      *protocol = context->protocol;
    }
}

gboolean
gdk_drag_motion (GdkDragContext  *context,
		 GdkWindow       *dest_window,
		 GdkDragProtocol  protocol,
		 gint             x_root,
		 gint             y_root,
		 GdkDragAction    suggested_action,
		 GdkDragAction    possible_actions,
		 guint32          time)
{
  GdkDragContextPrivate *private;

  g_return_val_if_fail (context != NULL, FALSE);

  private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);

  if (context->dest_window != dest_window)
    {
      GdkEvent  event;

      /* Send a leave to the last destination */
      gdk_drag_do_leave (context, time);
      private->drag_status = GDK_DRAG_STATUS_DRAG;

      /* Check if new destination accepts drags, and which protocol */
      if (dest_window)
	{
	  context->dest_window = g_object_ref (dest_window);
	  context->protocol = protocol;

	  switch (protocol)
	    {
	    case GDK_DRAG_PROTO_LOCAL:
	      local_send_enter (context, time);
	      break;

	    default:
	      break;
	    }
	  context->suggested_action = suggested_action;
	}
      else
	{
	  context->dest_window = NULL;
	  context->action = 0;
	}

      /* Push a status event, to let the client know that
       * the drag changed
       */

      event.dnd.type       = GDK_DRAG_STATUS;
      event.dnd.window     = context->source_window;
      /* We use this to signal a synthetic status. Perhaps
       * we should use an extra field...
       */
      event.dnd.send_event = TRUE;
      event.dnd.context    = context;
      event.dnd.time       = time;

      gdk_event_put (&event);
    }
  else
    {
      context->suggested_action = suggested_action;
    }

  /* Send a drag-motion event */

  private->last_x = x_root;
  private->last_y = y_root;

  if (context->dest_window)
    {
      if (private->drag_status == GDK_DRAG_STATUS_DRAG)
	{
	  switch (context->protocol)
	    {
	    case GDK_DRAG_PROTO_LOCAL:
	      local_send_motion (context,
                                 x_root, y_root, suggested_action, time);
	      break;

	    case GDK_DRAG_PROTO_NONE:
	      g_warning ("GDK_DRAG_PROTO_NONE is not valid in gdk_drag_motion()");
	      break;
	    default:
	      break;
	    }
	}
      else
	return TRUE;
    }

  return FALSE;
}

void
gdk_drag_drop (GdkDragContext *context,
	       guint32         time)
{
  g_return_if_fail (context != NULL);

  if (context->dest_window)
    {
      switch (context->protocol)
	{
	case GDK_DRAG_PROTO_LOCAL:
	  local_send_drop (context, time);
	  break;
	case GDK_DRAG_PROTO_NONE:
	  g_warning ("GDK_DRAG_PROTO_NONE is not valid in gdk_drag_drop()");
	  break;
	default:
	  break;
	}
    }
}

void
gdk_drag_abort (GdkDragContext *context,
		guint32         time)
{
  g_return_if_fail (context != NULL);

  gdk_drag_do_leave (context, time);
}

/* Destination side */

void
gdk_drag_status (GdkDragContext   *context,
		 GdkDragAction     action,
		 guint32           time)
{
  GdkDragContextPrivate *private;
  GdkDragContext        *src_context;
  GdkEvent event;

  g_return_if_fail (context != NULL);

  private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);

  src_context = gdk_drag_context_find (TRUE,
				       context->source_window,
				       context->dest_window);

  if (src_context)
    {
      GdkDragContextPrivate *private;

      private = GDK_DRAG_CONTEXT_PRIVATE_DATA (src_context);

      if (private->drag_status == GDK_DRAG_STATUS_MOTION_WAIT)
	private->drag_status = GDK_DRAG_STATUS_DRAG;

      event.dnd.type       = GDK_DRAG_STATUS;
      event.dnd.window     = src_context->source_window;
      event.dnd.send_event = FALSE;
      event.dnd.context    = src_context;
      event.dnd.time       = time;

      src_context->action = action;

      gdk_event_put (&event);
    }
}

void
gdk_drop_reply (GdkDragContext   *context,
		gboolean          ok,
		guint32           time)
{
  g_return_if_fail (context != NULL);
}

void
gdk_drop_finish (GdkDragContext   *context,
		 gboolean          success,
		 guint32           time)
{
  GdkDragContextPrivate *private;
  GdkDragContext        *src_context;
  GdkEvent event;

  g_return_if_fail (context != NULL);

  private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);

  src_context = gdk_drag_context_find (TRUE,
				       context->source_window,
				       context->dest_window);
  if (src_context)
    {
      g_object_ref (src_context);

      event.dnd.type       = GDK_DROP_FINISHED;
      event.dnd.window     = src_context->source_window;
      event.dnd.send_event = FALSE;
      event.dnd.context    = src_context;

      gdk_event_put (&event);
    }
}

gboolean
gdk_drag_drop_succeeded (GdkDragContext *context)
{
	g_warning("gdk_drag_drop_succeeded unimplemented \n");
	return TRUE;
}

void
gdk_window_register_dnd (GdkWindow      *window)
{
  g_return_if_fail (window != NULL);

  if (GPOINTER_TO_INT (gdk_drawable_get_data (window, "gdk-dnd-registered")))
    return;

  gdk_drawable_set_data (window, "gdk-dnd-registered",
                         GINT_TO_POINTER (TRUE), NULL);
}

/*************************************************************
 * gdk_drag_get_selection:
 *     Returns the selection atom for the current source window
 *   arguments:
 *
 *   results:
 *************************************************************/

GdkAtom
gdk_drag_get_selection (GdkDragContext *context)
{
  g_return_val_if_fail (context != NULL, GDK_NONE);

  if (context->protocol == GDK_DRAG_PROTO_LOCAL)
    return (GDK_DRAG_CONTEXT_PRIVATE_DATA (context))->local_selection;
  else
    return GDK_NONE;
}

#define __GDK_DND_X11_C__
#include "gdkaliasdef.c"
