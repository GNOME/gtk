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
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>
#include "gdk.h"          /* For gdk_flush() */
#include "gdkdnd.h"
#include "gdkproperty.h"
#include "gdkinternals.h"
#include "gdkprivate-fb.h"

typedef struct _GdkDragContextPrivate GdkDragContextPrivate;

typedef enum {
  GDK_DRAG_STATUS_DRAG,
  GDK_DRAG_STATUS_MOTION_WAIT,
  GDK_DRAG_STATUS_ACTION_WAIT,
  GDK_DRAG_STATUS_DROP
} GtkDragStatus;

/* Structure that holds information about a drag in progress.
 * this is used on both source and destination sides.
 */
struct _GdkDragContextPrivate {
  GdkAtom local_selection;
  
  guint16 last_x;		/* Coordinates from last event */
  guint16 last_y;
  
  guint drag_status : 4;	/* current status of drag */
};

/* Drag Contexts */

static GList *contexts;
static gpointer parent_class = NULL;

#define GDK_DRAG_CONTEXT_PRIVATE_DATA(ctx) ((GdkDragContextPrivate *) GDK_DRAG_CONTEXT (ctx)->windowing_data)

GdkDragContext *current_dest_drag = NULL;

static void
gdk_drag_context_init (GdkDragContext *dragcontext)
{
  dragcontext->windowing_data = g_new0 (GdkDragContextPrivate, 1);

  contexts = g_list_prepend (contexts, dragcontext);
}

static void
gdk_drag_context_finalize (GObject *object)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (object);
  GdkDragContextPrivate *private = GDK_DRAG_CONTEXT_PRIVATE_DATA (object);
  
  g_list_free (context->targets);

  if (context->source_window)
    gdk_window_unref (context->source_window);
  
  if (context->dest_window)
    gdk_window_unref (context->dest_window);

  
  if (private)
    {
      g_free (private);
      context->windowing_data = NULL;
    }
  
  contexts = g_list_remove (contexts, context);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_drag_context_class_init (GdkDragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_drag_context_finalize;
}


GType
gdk_drag_context_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkDragContextClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_drag_context_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkDragContext),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_drag_context_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkDragContext",
                                            &object_info,
					    0);
    }
  
  return object_type;
}

GdkDragContext *
gdk_drag_context_new        (void)
{
  return (GdkDragContext *)g_object_new (gdk_drag_context_get_type (), NULL);
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
  GList *tmp_list = contexts;
  
  GdkDragContext *context;
  GdkDragContextPrivate *private;

  while (tmp_list)
    {
      context = (GdkDragContext *)tmp_list->data;
      private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);

      if ((!context->is_source == !is_source) &&
	  ((source == NULL) || (context->source_window && (context->source_window == source))) &&
	  ((dest == NULL) || (context->dest_window && (context->dest_window == dest))))
	  return context;
      
      tmp_list = tmp_list->next;
    }
  
  return NULL;
}


/*************************************************************
 ************************** Public API ***********************
 *************************************************************/

void
_gdk_dnd_init (void)
{
}

/* Source side */

static void
local_send_leave (GdkDragContext  *context,
		  guint32          time)
{
  GdkEvent tmp_event;
  
  if ((current_dest_drag != NULL) &&
      (current_dest_drag->protocol == GDK_DRAG_PROTO_LOCAL) &&
      (current_dest_drag->source_window == context->source_window))
    {
      tmp_event.dnd.type = GDK_DRAG_LEAVE;
      tmp_event.dnd.window = context->dest_window;
      /* Pass ownership of context to the event */
      tmp_event.dnd.context = current_dest_drag;
      tmp_event.dnd.send_event = FALSE;
      tmp_event.dnd.time = GDK_CURRENT_TIME; /* FIXME? */

      current_dest_drag = NULL;
      
      gdk_event_put (&tmp_event);
    }
  
}

static void
local_send_enter (GdkDragContext  *context,
		  guint32          time)
{
  GdkEvent tmp_event;
  GdkDragContextPrivate *private;
  GdkDragContext *new_context;

  private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);
  
  if (!private->local_selection)
    private->local_selection = gdk_atom_intern ("LocalDndSelection", FALSE);

  if (current_dest_drag != NULL)
    {
      gdk_drag_context_unref (current_dest_drag);
      current_dest_drag = NULL;
    }

  new_context = gdk_drag_context_new ();
  new_context->protocol = GDK_DRAG_PROTO_LOCAL;
  new_context->is_source = FALSE;

  new_context->source_window = context->source_window;
  gdk_window_ref (new_context->source_window);
  new_context->dest_window = context->dest_window;
  gdk_window_ref (new_context->dest_window);

  new_context->targets = g_list_copy (context->targets);

  gdk_window_set_events (new_context->source_window,
			 gdk_window_get_events (new_context->source_window) |
			 GDK_PROPERTY_CHANGE_MASK);
 new_context->actions = context->actions;

  tmp_event.dnd.type = GDK_DRAG_ENTER;
  tmp_event.dnd.window = context->dest_window;
  tmp_event.dnd.send_event = FALSE;
  tmp_event.dnd.context = new_context;
  gdk_drag_context_ref (new_context);

  tmp_event.dnd.time = GDK_CURRENT_TIME; /* FIXME? */
  
  current_dest_drag = new_context;
  
  (GDK_DRAG_CONTEXT_PRIVATE_DATA (new_context))->local_selection = 
    private->local_selection;

  gdk_event_put (&tmp_event);
}

static void
local_send_motion (GdkDragContext  *context,
		    gint            x_root, 
		    gint            y_root,
		    GdkDragAction   action,
		    guint32         time)
{
  GdkEvent tmp_event;
  
  if ((current_dest_drag != NULL) &&
      (current_dest_drag->protocol == GDK_DRAG_PROTO_LOCAL) &&
      (current_dest_drag->source_window == context->source_window))
    {
      tmp_event.dnd.type = GDK_DRAG_MOTION;
      tmp_event.dnd.window = current_dest_drag->dest_window;
      tmp_event.dnd.send_event = FALSE;
      tmp_event.dnd.context = current_dest_drag;
      gdk_drag_context_ref (current_dest_drag);

      tmp_event.dnd.time = time;

      current_dest_drag->suggested_action = action;
      current_dest_drag->actions = current_dest_drag->suggested_action;

      tmp_event.dnd.x_root = x_root;
      tmp_event.dnd.y_root = y_root;

      (GDK_DRAG_CONTEXT_PRIVATE_DATA (current_dest_drag))->last_x = x_root;
      (GDK_DRAG_CONTEXT_PRIVATE_DATA (current_dest_drag))->last_y = y_root;

      GDK_DRAG_CONTEXT_PRIVATE_DATA (context)->drag_status = GDK_DRAG_STATUS_MOTION_WAIT;
      
      gdk_event_put (&tmp_event);
    }
}

static void
local_send_drop (GdkDragContext *context, guint32 time)
{
  GdkEvent tmp_event;
  
  if ((current_dest_drag != NULL) &&
      (current_dest_drag->protocol == GDK_DRAG_PROTO_LOCAL) &&
      (current_dest_drag->source_window == context->source_window))
    {
      GdkDragContextPrivate *private;
      private = GDK_DRAG_CONTEXT_PRIVATE_DATA (current_dest_drag);

      tmp_event.dnd.type = GDK_DROP_START;
      tmp_event.dnd.window = current_dest_drag->dest_window;
      tmp_event.dnd.send_event = FALSE;

      tmp_event.dnd.context = current_dest_drag;
      gdk_drag_context_ref (current_dest_drag);

      tmp_event.dnd.time = GDK_CURRENT_TIME;
      
      tmp_event.dnd.x_root = private->last_x;
      tmp_event.dnd.y_root = private->last_y;
      
      gdk_event_put (&tmp_event);
    }

}

static void
gdk_drag_do_leave (GdkDragContext *context, guint32 time)
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

      gdk_window_unref (context->dest_window);
      context->dest_window = NULL;
    }
}

GdkDragContext * 
gdk_drag_begin (GdkWindow     *window,
		GList         *targets)
{
  GList *tmp_list;
  GdkDragContext *new_context;

  g_return_val_if_fail (window != NULL, NULL);

  new_context = gdk_drag_context_new ();
  new_context->is_source = TRUE;
  new_context->source_window = window;
  gdk_window_ref (window);

  tmp_list = g_list_last (targets);
  new_context->targets = NULL;
  while (tmp_list)
    {
      new_context->targets = g_list_prepend (new_context->targets,
					     tmp_list->data);
      tmp_list = tmp_list->prev;
    }

  new_context->actions = 0;

  return new_context;
}

guint32
gdk_drag_get_protocol_for_display (GdkDisplay      *display,
				   guint32          xid,
				   GdkDragProtocol *protocol)
{
  GdkWindow *window;

  window = gdk_window_lookup ((GdkNativeWindow) xid);

  if (GPOINTER_TO_INT (gdk_drawable_get_data (window, "gdk-dnd-registered")))
    {
      *protocol = GDK_DRAG_PROTO_LOCAL;
      return xid;
    }
  
  *protocol = GDK_DRAG_PROTO_NONE;
  return 0;
}

static GdkWindow *
get_toplevel_window_at (GdkWindow *ignore,
			gint       x_root,
			gint       y_root)
{

  GdkWindowObject *private;
  GdkWindowObject *sub;
  GdkWindowObject *child;
  GList *ltmp, *ltmp2;
  
  private = (GdkWindowObject *)_gdk_parent_root;

  for (ltmp = private->children; ltmp; ltmp = ltmp->next)
    {
      sub = ltmp->data;
	  
      if ((GDK_WINDOW (sub) != ignore) &&
	  (GDK_WINDOW_IS_MAPPED (sub)) &&
	  (x_root >= sub->x) &&
	  (x_root < sub->x + GDK_DRAWABLE_IMPL_FBDATA (sub)->width) &&
	  (y_root >= sub->y) &&
	  (y_root < sub->y + GDK_DRAWABLE_IMPL_FBDATA (sub)->height))
	{
	  if (g_object_get_data (G_OBJECT (sub), "gdk-window-child-handler"))
	    {
	      /* Managed window, check children */
	      for (ltmp2 = sub->children; ltmp2; ltmp2 = ltmp2->next)
		{
		  child = ltmp2->data;
		  
		  if ((GDK_WINDOW (child) != ignore) &&
		      (GDK_WINDOW_IS_MAPPED (child)) &&
		      (x_root >= sub->x + child->x) &&
		      (x_root < sub->x + child->x + GDK_DRAWABLE_IMPL_FBDATA (child)->width) &&
		      (y_root >= sub->y + child->y) &&
		      (y_root < sub->y + child->y + GDK_DRAWABLE_IMPL_FBDATA (child)->height))
		    return GDK_WINDOW (child);
		}
	    }
	  else
	    return GDK_WINDOW (sub);
	}
    }
  return NULL;
}


void
gdk_drag_find_window_for_screen (GdkDragContext  *context,
				 GdkWindow       *drag_window,
				 GdkScreen       *screen,
				 gint             x_root,
				 gint             y_root,
				 GdkWindow      **dest_window,
				 GdkDragProtocol *protocol)
{
  GdkWindow *dest;

  g_return_if_fail (context != NULL);

  dest = get_toplevel_window_at (drag_window, x_root, y_root);
  if (dest == NULL)
    dest = _gdk_parent_root;
  
  if (context->dest_window != dest)
    {
      guint32 recipient;

      /* Check if new destination accepts drags, and which protocol */
      if ((recipient = gdk_drag_get_protocol ((guint32)dest, protocol)))
	{
	  *dest_window = gdk_window_lookup ((GdkNativeWindow) recipient);
	  gdk_window_ref (*dest_window);
	}
      else
	*dest_window = NULL;
    }
  else
    {
      *dest_window = context->dest_window;
      if (*dest_window)
	gdk_window_ref (*dest_window);
      *protocol = context->protocol;
    }
  
}

gboolean        
gdk_drag_motion (GdkDragContext *context,
		 GdkWindow      *dest_window,
		 GdkDragProtocol protocol,
		 gint            x_root, 
		 gint            y_root,
		 GdkDragAction   suggested_action,
		 GdkDragAction   possible_actions,
		 guint32         time)
{
  GdkDragContextPrivate *private;

  g_return_val_if_fail (context != NULL, FALSE);

  private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);
  
  if (context->dest_window != dest_window)
    {
      GdkEvent temp_event;

      /* Send a leave to the last destination */
      gdk_drag_do_leave (context, time);
      private->drag_status = GDK_DRAG_STATUS_DRAG;

      /* Check if new destination accepts drags, and which protocol */
      if (dest_window)
	{
	  context->dest_window = dest_window;
	  gdk_window_ref (context->dest_window);
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

      temp_event.dnd.type = GDK_DRAG_STATUS;
      temp_event.dnd.window = context->source_window;
      /* We use this to signal a synthetic status. Perhaps
       * we should use an extra field...
       */
      temp_event.dnd.send_event = TRUE;

      temp_event.dnd.context = context;
      temp_event.dnd.time = time;

      gdk_event_put (&temp_event);
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
	      local_send_motion (context, x_root, y_root, suggested_action, time);
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
  GdkDragContext *src_context;
  GdkEvent tmp_event;

  g_return_if_fail (context != NULL);

  private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);

  src_context = gdk_drag_context_find (TRUE,
				       context->source_window,
				       context->dest_window);

  if (src_context)
    {
      GdkDragContextPrivate *private = GDK_DRAG_CONTEXT_PRIVATE_DATA (src_context);
      
      if (private->drag_status == GDK_DRAG_STATUS_MOTION_WAIT)
	private->drag_status = GDK_DRAG_STATUS_DRAG;

      tmp_event.dnd.type = GDK_DRAG_STATUS;
      tmp_event.dnd.window = context->source_window;
      tmp_event.dnd.send_event = FALSE;
      tmp_event.dnd.context = src_context;
      gdk_drag_context_ref (src_context);

      tmp_event.dnd.time = GDK_CURRENT_TIME; /* FIXME? */

      if (action == GDK_ACTION_DEFAULT)
	action = 0;
      
      src_context->action = action;
      
      gdk_event_put (&tmp_event);
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
  GdkDragContext *src_context;
  GdkEvent tmp_event;
	
  g_return_if_fail (context != NULL);

  private = GDK_DRAG_CONTEXT_PRIVATE_DATA (context);

  src_context = gdk_drag_context_find (TRUE,
				       context->source_window,
				       context->dest_window);
  if (src_context)
    {
      tmp_event.dnd.type = GDK_DROP_FINISHED;
      tmp_event.dnd.window = src_context->source_window;
      tmp_event.dnd.send_event = FALSE;
      tmp_event.dnd.context = src_context;
      gdk_drag_context_ref (src_context);

      gdk_event_put (&tmp_event);
    }
}


void            
gdk_window_register_dnd (GdkWindow      *window)
{
  g_return_if_fail (window != NULL);

  if (GPOINTER_TO_INT (gdk_drawable_get_data (window, "gdk-dnd-registered")))
    return;
  else
    gdk_drawable_set_data (window, "gdk-dnd-registered", GINT_TO_POINTER(TRUE), NULL);
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

