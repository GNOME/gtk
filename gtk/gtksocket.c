/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* By Owen Taylor <otaylor@gtk.org>              98/4/4 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gdk/gdkkeysyms.h"
#include "gtkmain.h"
#include "gtkwindow.h"
#include "gtkplug.h"
#include "gtksignal.h"
#include "gtksocket.h"
#include "gtkdnd.h"

#include "x11/gdkx.h"

#include "xembed.h"

/* Forward declararations */

static void     gtk_socket_class_init           (GtkSocketClass   *klass);
static void     gtk_socket_init                 (GtkSocket        *socket);
static void     gtk_socket_realize              (GtkWidget        *widget);
static void     gtk_socket_unrealize            (GtkWidget        *widget);
static void     gtk_socket_size_request         (GtkWidget        *widget,
						 GtkRequisition   *requisition);
static void     gtk_socket_size_allocate        (GtkWidget        *widget,
						 GtkAllocation    *allocation);
static void     gtk_socket_hierarchy_changed    (GtkWidget        *widget,
						 GtkWidget        *old_toplevel);
static void     gtk_socket_grab_notify          (GtkWidget        *widget,
						 gboolean          was_grabbed);
static gboolean gtk_socket_key_press_event      (GtkWidget        *widget,
						 GdkEventKey      *event);
static gboolean gtk_socket_focus_in_event       (GtkWidget        *widget,
						 GdkEventFocus    *event);
static void     gtk_socket_claim_focus          (GtkSocket        *socket);
static gboolean gtk_socket_focus_out_event      (GtkWidget        *widget,
						 GdkEventFocus    *event);
static void     gtk_socket_send_configure_event (GtkSocket        *socket);
static gboolean gtk_socket_focus                (GtkWidget        *widget,
						 GtkDirectionType  direction);
static void     gtk_socket_remove               (GtkContainer     *container,
						 GtkWidget        *widget);
static void     gtk_socket_forall               (GtkContainer     *container,
						 gboolean          include_internals,
						 GtkCallback       callback,
						 gpointer          callback_data);


static void            gtk_socket_add_window  (GtkSocket       *socket,
					       GdkNativeWindow  xid,
					       gboolean         need_reparent);
static GdkFilterReturn gtk_socket_filter_func (GdkXEvent       *gdk_xevent,
					       GdkEvent        *event,
					       gpointer         data);

static void     send_xembed_message (GtkSocket     *socket,
				     glong          message,
				     glong          detail,
				     glong          data1,
				     glong          data2,
				     guint32        time);
static gboolean xembed_get_info     (GdkWindow     *gdk_window,
				     unsigned long *version,
				     unsigned long *flags);

/* From Tk */
#define EMBEDDED_APP_WANTS_FOCUS NotifyNormal+20

/* Local data */

static GtkWidgetClass *parent_class = NULL;

GtkType
gtk_socket_get_type (void)
{
  static GtkType socket_type = 0;

  if (!socket_type)
    {
      static const GTypeInfo socket_info =
      {
	sizeof (GtkSocketClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
	(GClassInitFunc) gtk_socket_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data */
	sizeof (GtkSocket),
	16,             /* n_preallocs */
	(GInstanceInitFunc) gtk_socket_init,
      };

      socket_type = g_type_register_static (GTK_TYPE_CONTAINER, "GtkSocket", &socket_info, 0);
    }

  return socket_type;
}

static void
gtk_socket_class_init (GtkSocketClass *class)
{
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = gtk_type_class (GTK_TYPE_CONTAINER);

  widget_class->realize = gtk_socket_realize;
  widget_class->unrealize = gtk_socket_unrealize;
  widget_class->size_request = gtk_socket_size_request;
  widget_class->size_allocate = gtk_socket_size_allocate;
  widget_class->hierarchy_changed = gtk_socket_hierarchy_changed;
  widget_class->grab_notify = gtk_socket_grab_notify;
  widget_class->key_press_event = gtk_socket_key_press_event;
  widget_class->focus_in_event = gtk_socket_focus_in_event;
  widget_class->focus_out_event = gtk_socket_focus_out_event;
  widget_class->focus = gtk_socket_focus;
  
  container_class->remove = gtk_socket_remove;
  container_class->forall = gtk_socket_forall;
}

static void
gtk_socket_init (GtkSocket *socket)
{
  socket->request_width = 0;
  socket->request_height = 0;
  socket->current_width = 0;
  socket->current_height = 0;
  
  socket->plug_window = NULL;
  socket->plug_widget = NULL;
  socket->focus_in = FALSE;
  socket->have_size = FALSE;
  socket->need_map = FALSE;
}

GtkWidget*
gtk_socket_new (void)
{
  GtkSocket *socket;

  socket = g_object_new (GTK_TYPE_SOCKET, NULL);

  return GTK_WIDGET (socket);
}

void           
gtk_socket_steal (GtkSocket *socket, GdkNativeWindow id)
{
  gtk_socket_add_window (socket, id, TRUE);
}

static void
gtk_socket_realize (GtkWidget *widget)
{
  GtkSocket *socket;
  GdkWindowAttr attributes;
  gint attributes_mask;
  XWindowAttributes xattrs;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SOCKET (widget));

  socket = GTK_SOCKET (widget);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = GDK_FOCUS_CHANGE_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), 
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, socket);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  XGetWindowAttributes (GDK_DISPLAY (),
			GDK_WINDOW_XWINDOW (widget->window),
			&xattrs);

  XSelectInput (GDK_DISPLAY (),
		GDK_WINDOW_XWINDOW(widget->window), 
		xattrs.your_event_mask | 
		SubstructureNotifyMask | SubstructureRedirectMask);

  gdk_window_add_filter (widget->window, gtk_socket_filter_func, widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  /* We sync here so that we make sure that if the XID for
   * our window is passed to another application, SubstructureRedirectMask
   * will be set by the time the other app creates its window.
   */
  gdk_flush();
}

static void
gtk_socket_unrealize (GtkWidget *widget)
{
  GtkSocket *socket;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SOCKET (widget));

  socket = GTK_SOCKET (widget);

  if (socket->plug_window)
    {
      GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (socket));
      if (toplevel && GTK_IS_WINDOW (toplevel))
	gtk_window_remove_embedded_xid (GTK_WINDOW (toplevel), 
					GDK_WINDOW_XWINDOW (socket->plug_window));

      socket->plug_window = NULL;
    }

#if 0  
  if (socket->grabbed_keys)
    {
      g_hash_table_foreach (socket->grabbed_keys, (GHFunc)g_free, NULL);
      g_hash_table_destroy (socket->grabbed_keys);
      socket->grabbed_keys = NULL;
    }
#endif
  
  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}
  
static void 
gtk_socket_size_request (GtkWidget      *widget,
			 GtkRequisition *requisition)
{
  GtkSocket *socket;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SOCKET (widget));
  g_return_if_fail (requisition != NULL);
  
  socket = GTK_SOCKET (widget);

  if (socket->plug_widget)
    {
      gtk_widget_size_request (socket->plug_widget, requisition);
    }
  else
    {
      if (socket->is_mapped && !socket->have_size && socket->plug_window)
	{
	  XSizeHints hints;
	  long supplied;
	  
	  gdk_error_trap_push ();
	  
	  if (XGetWMNormalHints (GDK_DISPLAY(),
				 GDK_WINDOW_XWINDOW (socket->plug_window),
				 &hints, &supplied))
	    {
	      /* This is obsolete, according the X docs, but many programs
	       * still use it */
	      if (hints.flags & (PSize | USSize))
		{
		  socket->request_width = hints.width;
		  socket->request_height = hints.height;
		}
	      else if (hints.flags & PMinSize)
		{
		  socket->request_width = hints.min_width;
		  socket->request_height = hints.min_height;
		}
	      else if (hints.flags & PBaseSize)
		{
		  socket->request_width = hints.base_width;
		  socket->request_height = hints.base_height;
		}
	    }
	  socket->have_size = TRUE;	/* don't check again? */
	  
	  gdk_error_trap_pop ();
	}

      if (socket->is_mapped && socket->have_size)
	{
	  requisition->width = MAX (socket->request_width, 1);
	  requisition->height = MAX (socket->request_height, 1);
	}
      else
	{
	  requisition->width = 1;
	  requisition->height = 1;
	}
    }
}

static void
gtk_socket_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkSocket *socket;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SOCKET (widget));
  g_return_if_fail (allocation != NULL);

  socket = GTK_SOCKET (widget);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      if (socket->plug_widget)
	{
	  GtkAllocation child_allocation;

	  child_allocation.x = 0;
	  child_allocation.y = 0;
	  child_allocation.width = allocation->width;
	  child_allocation.height = allocation->height;

	  gtk_widget_size_allocate (socket->plug_widget, &child_allocation);
	}
      else if (socket->plug_window)
	{
	  gdk_error_trap_push ();
	  
	  if (!socket->need_map &&
	      (allocation->width == socket->current_width) &&
	      (allocation->height == socket->current_height))
	    {
	      gtk_socket_send_configure_event (socket);
	      GTK_NOTE(PLUGSOCKET, 
		       g_message ("GtkSocket - allocated no change: %d %d",
				  allocation->width, allocation->height));
	    }
	  else
	    {
	      gdk_window_move_resize (socket->plug_window,
				      0, 0,
				      allocation->width, allocation->height);
	      GTK_NOTE(PLUGSOCKET,
		       g_message ("GtkSocket - allocated: %d %d",
				  allocation->width, allocation->height));
	      socket->current_width = allocation->width;
	      socket->current_height = allocation->height;
	    }

	  if (socket->need_map)
	    {
	      gdk_window_show (socket->plug_window);
	      socket->need_map = FALSE;
	    }

	  gdk_flush ();
	  gdk_error_trap_pop ();
	}
    }
}

#if 0

typedef struct
{
  guint			 accelerator_key;
  GdkModifierType	 accelerator_mods;
} GrabbedKey;

static guint
grabbed_key_hash (gconstpointer a)
{
  const GrabbedKey *key = a;
  guint h;
  
  h = key->accelerator_key << 16;
  h ^= key->accelerator_key >> 16;
  h ^= key->accelerator_mods;

  return h;
}

static gboolean
grabbed_key_equal (gconstpointer a, gconstpointer b)
{
  const GrabbedKey *keya = a;
  const GrabbedKey *keyb = b;

  return (keya->accelerator_key == keyb->accelerator_key &&
	  keya->accelerator_mods == keyb->accelerator_mods);
}

static void
add_grabbed_key (GtkSocket      *socket,
		 guint           hardware_keycode,
		 GdkModifierType mods)
{
  GrabbedKey key;
  GrabbedKey *new_key;
  GrabbedKey *found_key;

  if (socket->grabbed_keys)
    {
      key.accelerator_key = hardware_keycode;
      key.accelerator_mods = mods;

      found_key = g_hash_table_lookup (socket->grabbed_keys, &key);

      if (found_key)
	{
	  g_warning ("GtkSocket: request to add already present grabbed key %u,%#x\n",
		     hardware_keycode, mods);
	  return;
	}
    }
  
  if (!socket->grabbed_keys)
    socket->grabbed_keys = g_hash_table_new (grabbed_key_hash, grabbed_key_equal);

  new_key = g_new (GrabbedKey, 1);
  
  new_key->accelerator_key = hardware_keycode;
  new_key->accelerator_mods = mods;

  g_hash_table_insert (socket->grabbed_keys, new_key, new_key);
}

static void
remove_grabbed_key (GtkSocket      *socket,
		    guint           hardware_keycode,
		    GdkModifierType mods)
{
  GrabbedKey key;
  GrabbedKey *found_key = NULL;

  if (socket->grabbed_keys)
    {
      key.accelerator_key = hardware_keycode;
      key.accelerator_mods = mods;

      found_key = g_hash_table_lookup (socket->grabbed_keys, &key);
    }

  if (found_key)
    {
      g_hash_table_remove (socket->grabbed_keys, &key);
      g_free (found_key);
    }
  else
    g_warning ("GtkSocket: request to remove non-present grabbed key %u,%#x\n",
	       hardware_keycode, mods);
}

static gboolean
toplevel_key_press_handler (GtkWidget   *toplevel,
			    GdkEventKey *event,
			    GtkSocket   *socket)
{
  GrabbedKey search_key;

  search_key.accelerator_key = event->hardware_keycode;
  search_key.accelerator_mods = event->state;

  if (socket->grabbed_keys &&
      g_hash_table_lookup (socket->grabbed_keys, &search_key))
    {
      gtk_socket_key_press_event (GTK_WIDGET (socket), event);
      gtk_signal_emit_stop_by_name (GTK_OBJECT (toplevel), "key_press_event");

      return TRUE;
    }
  else
    return FALSE;
}

#endif

static void
toplevel_focus_in_handler (GtkWidget     *toplevel,
			   GdkEventFocus *event,
			   GtkSocket     *socket)
{
  /* It appears spurious focus in events can occur when
   *  the window is hidden. So we'll just check to see if
   *  the window is visible before actually handling the
   *  event. (Comment from gtkwindow.c)
   */
  if (GTK_WIDGET_VISIBLE (toplevel))
    send_xembed_message (socket, XEMBED_WINDOW_ACTIVATE, 0, 0, 0,
			 gtk_get_current_event_time ()); /* Will be GDK_CURRENT_TIME */
}

static void
toplevel_focus_out_handler (GtkWidget     *toplevel,
			    GdkEventFocus *event,
			    GtkSocket     *socket)
{
  send_xembed_message (socket, XEMBED_WINDOW_DEACTIVATE, 0, 0, 0,
		       gtk_get_current_event_time ()); /* Will be GDK_CURRENT_TIME */
}

static void
gtk_socket_hierarchy_changed (GtkWidget *widget,
			      GtkWidget *old_toplevel)
{
  GtkSocket *socket = GTK_SOCKET (widget);
  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);

  if (toplevel && !GTK_IS_WINDOW (toplevel))
    toplevel = NULL;

  if (toplevel != socket->toplevel)
    {
      if (socket->toplevel)
	{
#if 0
	  gtk_signal_disconnect_by_func (GTK_OBJECT (socket->toplevel), GTK_SIGNAL_FUNC (toplevel_key_press_handler), socket);
#endif	  
	  gtk_signal_disconnect_by_func (GTK_OBJECT (socket->toplevel), GTK_SIGNAL_FUNC (toplevel_focus_in_handler), socket);
	  gtk_signal_disconnect_by_func (GTK_OBJECT (socket->toplevel), GTK_SIGNAL_FUNC (toplevel_focus_out_handler), socket);
	}

      socket->toplevel = toplevel;

      if (toplevel)
	{
#if 0
	  gtk_signal_connect (GTK_OBJECT (socket->toplevel), "key_press_event",
			      GTK_SIGNAL_FUNC (toplevel_key_press_handler), socket);
#endif
	  gtk_signal_connect (GTK_OBJECT (socket->toplevel), "focus_in_event",
			      GTK_SIGNAL_FUNC (toplevel_focus_in_handler), socket);
	  gtk_signal_connect (GTK_OBJECT (socket->toplevel), "focus_out_event",
			      GTK_SIGNAL_FUNC (toplevel_focus_out_handler), socket);
	}
    }
}

static void
gtk_socket_grab_notify (GtkWidget *widget,
			gboolean   was_grabbed)
{
  send_xembed_message (GTK_SOCKET (widget),
		       was_grabbed ? XEMBED_MODALITY_OFF : XEMBED_MODALITY_ON,
		       0, 0, 0, gtk_get_current_event_time ());
}

static gboolean
gtk_socket_key_press_event (GtkWidget   *widget,
			    GdkEventKey *event)
{
  GtkSocket *socket = GTK_SOCKET (widget);
  
  if (socket->plug_window)
    {
      XEvent xevent;
      
      xevent.xkey.type = KeyPress;
      xevent.xkey.display = GDK_WINDOW_XDISPLAY (event->window);
      xevent.xkey.window = GDK_WINDOW_XWINDOW (socket->plug_window);
      xevent.xkey.root = GDK_ROOT_WINDOW ();
      xevent.xkey.time = event->time;
      /* FIXME, the following might cause problems for non-GTK apps */
      xevent.xkey.x = 0;
      xevent.xkey.y = 0;
      xevent.xkey.x_root = 0;
      xevent.xkey.y_root = 0;
      xevent.xkey.state = event->state;
      xevent.xkey.keycode = event->hardware_keycode;
      xevent.xkey.same_screen = TRUE; /* FIXME ? */
      
      gdk_error_trap_push ();
      XSendEvent (gdk_display,
		  GDK_WINDOW_XWINDOW (socket->plug_window),
		  False, NoEventMask, &xevent);
      gdk_flush ();
      gdk_error_trap_pop ();
      
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_socket_focus_in_event (GtkWidget *widget, GdkEventFocus *event)
{
  GtkSocket *socket = GTK_SOCKET (widget);

  if (!GTK_WIDGET_HAS_FOCUS (widget))
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
  
      if (socket->plug_window)
	{
          send_xembed_message (socket, XEMBED_FOCUS_IN, XEMBED_FOCUS_CURRENT, 0, 0,
			       gtk_get_current_event_time ());
	}
    }
  
  return TRUE;
}

static gboolean
gtk_socket_focus_out_event (GtkWidget *widget, GdkEventFocus *event)
{
  GtkSocket *socket = GTK_SOCKET (widget);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);

#if 0
  GtkWidget *toplevel;
  toplevel = gtk_widget_get_toplevel (widget);
  
  if (toplevel && GTK_IS_WINDOW (toplevel))
    {
      XSetInputFocus (GDK_DISPLAY (),
		      GDK_WINDOW_XWINDOW (toplevel->window),
		      RevertToParent, CurrentTime); /* FIXME? */
    }

#endif      

  if (socket->plug_window)
    {
      send_xembed_message (socket, XEMBED_FOCUS_OUT, 0, 0, 0,
			   gtk_get_current_event_time ());
    }

  socket->focus_in = FALSE;
  
  return TRUE;
}

static void
gtk_socket_claim_focus (GtkSocket *socket)
{
      
  socket->focus_in = TRUE;
  
  /* Oh, the trickery... */
  
  GTK_WIDGET_SET_FLAGS (socket, GTK_CAN_FOCUS);
  gtk_widget_grab_focus (GTK_WIDGET (socket));
  GTK_WIDGET_UNSET_FLAGS (socket, GTK_CAN_FOCUS);
  
  /* FIXME: we might grab the focus even if we don't have
   * it as an app... (and see _focus_in ()) */
  if (socket->plug_window)
    {
#if 0      
      gdk_error_trap_push ();
      XSetInputFocus (GDK_DISPLAY (),
		      GDK_WINDOW_XWINDOW (socket->plug_window),
		      RevertToParent, GDK_CURRENT_TIME);
      gdk_flush ();
      gdk_error_trap_pop ();
#endif
    }
}

static gboolean
gtk_socket_focus (GtkWidget *widget, GtkDirectionType direction)
{
  GtkSocket *socket;
  gint detail = -1;

  g_return_val_if_fail (GTK_IS_SOCKET (widget), FALSE);
  
  socket = GTK_SOCKET (widget);

  if (socket->plug_widget)
    return gtk_widget_child_focus (socket->plug_widget, direction);

  if (!GTK_WIDGET_HAS_FOCUS (widget))
    {
      switch (direction)
	{
	case GTK_DIR_UP:
	case GTK_DIR_LEFT:
	case GTK_DIR_TAB_BACKWARD:
	  detail = XEMBED_FOCUS_LAST;
	  break;
	case GTK_DIR_DOWN:
	case GTK_DIR_RIGHT:
	case GTK_DIR_TAB_FORWARD:
	  detail = XEMBED_FOCUS_FIRST;
	  break;
	}
      
      send_xembed_message (socket, XEMBED_FOCUS_IN, detail, 0, 0,
			   gtk_get_current_event_time ());

      GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
      gtk_widget_grab_focus (widget);
 
      return TRUE;
    }
  else
    return FALSE;

#if 0
  if (!socket->focus_in && socket->plug_window)
    {
      XEvent xevent;

      gtk_socket_claim_focus (socket);
      
      xevent.xkey.type = KeyPress;
      xevent.xkey.display = GDK_DISPLAY ();
      xevent.xkey.window = GDK_WINDOW_XWINDOW (socket->plug_window);
      xevent.xkey.root = GDK_ROOT_WINDOW (); /* FIXME */
      xevent.xkey.time = GDK_CURRENT_TIME; /* FIXME */
      /* FIXME, the following might cause big problems for
       * non-GTK apps */
      xevent.xkey.x = 0;
      xevent.xkey.y = 0;
      xevent.xkey.x_root = 0;
      xevent.xkey.y_root = 0;
      xevent.xkey.state = 0;
      xevent.xkey.same_screen = TRUE; /* FIXME ? */

      switch (direction)
	{
	case GTK_DIR_UP:
	  xevent.xkey.keycode =  XKeysymToKeycode(GDK_DISPLAY(), GDK_Up);
	  break;
	case GTK_DIR_DOWN:
	  xevent.xkey.keycode =  XKeysymToKeycode(GDK_DISPLAY(), GDK_Down);
	  break;
	case GTK_DIR_LEFT:
	  xevent.xkey.keycode =  XKeysymToKeycode(GDK_DISPLAY(), GDK_Left);
	  break;
	case GTK_DIR_RIGHT:
	  xevent.xkey.keycode =  XKeysymToKeycode(GDK_DISPLAY(), GDK_Right);
	  break;
	case GTK_DIR_TAB_FORWARD:
	  xevent.xkey.keycode =  XKeysymToKeycode(GDK_DISPLAY(), GDK_Tab);
	  break;
	case GTK_DIR_TAB_BACKWARD:
	  xevent.xkey.keycode =  XKeysymToKeycode(GDK_DISPLAY(), GDK_Tab);
	  xevent.xkey.state = ShiftMask;
	  break;
	}


      gdk_error_trap_push ();
      XSendEvent (gdk_display,
		  GDK_WINDOW_XWINDOW (socket->plug_window),
		  False, NoEventMask, &xevent);
      gdk_flush();
      gdk_error_trap_pop ();
      
      return TRUE;
    }
  else
    {
      return FALSE;
    }
#endif  
}

static void
gtk_socket_remove (GtkContainer *container,
		   GtkWidget    *child)
{
  GtkSocket *socket = GTK_SOCKET (container);
  gboolean widget_was_visible;

  g_return_if_fail (child == socket->plug_widget);

  widget_was_visible = GTK_WIDGET_VISIBLE (child);
  
  gtk_widget_unparent (child);
  socket->plug_widget = NULL;
  
  /* queue resize regardless of GTK_WIDGET_VISIBLE (container),
   * since that's what is needed by toplevels, which derive from GtkBin.
   */
  if (widget_was_visible)
    gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
gtk_socket_forall (GtkContainer *container,
		   gboolean      include_internals,
		   GtkCallback   callback,
		   gpointer      callback_data)
{
  GtkSocket *socket = GTK_SOCKET (container);

  if (socket->plug_widget)
    (* callback) (socket->plug_widget, callback_data);
}

static void
gtk_socket_send_configure_event (GtkSocket *socket)
{
  XEvent event;

  g_return_if_fail (socket->plug_window != NULL);

  event.xconfigure.type = ConfigureNotify;
  event.xconfigure.display = gdk_display;

  event.xconfigure.event = GDK_WINDOW_XWINDOW (socket->plug_window);
  event.xconfigure.window = GDK_WINDOW_XWINDOW (socket->plug_window);

  event.xconfigure.x = 0;
  event.xconfigure.y = 0;
  event.xconfigure.width = GTK_WIDGET(socket)->allocation.width;
  event.xconfigure.height = GTK_WIDGET(socket)->allocation.height;

  event.xconfigure.border_width = 0;
  event.xconfigure.above = None;
  event.xconfigure.override_redirect = False;

  gdk_error_trap_push ();
  XSendEvent (gdk_display,
	      GDK_WINDOW_XWINDOW (socket->plug_window),
	      False, NoEventMask, &event);
  gdk_flush ();
  gdk_error_trap_pop ();
}

static void
gtk_socket_add_window (GtkSocket        *socket,
		       GdkNativeWindow   xid,
		       gboolean          need_reparent)
{

  GtkWidget *widget = GTK_WIDGET (socket);
  gpointer user_data = NULL;
  
  socket->plug_window = gdk_window_lookup (xid);

  if (socket->plug_window)
    {
      g_object_ref (socket->plug_window);
      gdk_window_get_user_data (socket->plug_window, &user_data);
    }

  if (user_data)		/* A widget's window in this process */
    {
      GtkWidget *child_widget = user_data;

      if (!GTK_IS_PLUG (child_widget))
	{
	  g_warning (G_STRLOC "Can't add non-GtkPlug to GtkSocket");
	  socket->plug_window = NULL;
	  gdk_error_trap_pop ();
	  
	  return;
	}

      _gtk_plug_add_to_socket (GTK_PLUG (child_widget), socket);
    }
  else				/* A foreign window */
    {
      GtkWidget *toplevel;
      GdkDragProtocol protocol;
      unsigned long version;
      unsigned long flags;

      gdk_error_trap_push ();

      if (!socket->plug_window)
	{  
	  socket->plug_window = gdk_window_foreign_new (xid);
	  if (!socket->plug_window) /* was deleted before we could get it */
	    {
	      gdk_error_trap_pop ();
	      return;
	    }
	}
	
      XSelectInput (GDK_DISPLAY (),
		    GDK_WINDOW_XWINDOW(socket->plug_window),
		    StructureNotifyMask | PropertyChangeMask);
      
      if (gdk_error_trap_pop ())
	{
	  gdk_window_unref (socket->plug_window);
	  socket->plug_window = NULL;
	  return;
	}
      
      /* OK, we now will reliably get destroy notification on socket->plug_window */

      gdk_error_trap_push ();

      if (need_reparent)
	{
	  gdk_window_hide (socket->plug_window); /* Shouldn't actually be necessary for XEMBED, but just in case */
	  gdk_window_reparent (socket->plug_window, widget->window, 0, 0);
	}

      socket->have_size = FALSE;

      socket->xembed_version = -1;
      if (xembed_get_info (socket->plug_window, &version, &flags))
	{
	  socket->xembed_version = version;
	  socket->is_mapped = (flags & XEMBED_MAPPED) != 0;
	}
      else
	{
	  /* FIXME, we should probably actually check the state before we started */
	  
	  socket->is_mapped = need_reparent ? TRUE : FALSE;
	}
      
      socket->need_map = socket->is_mapped;

      if (gdk_drag_get_protocol (xid, &protocol))
	gtk_drag_dest_set_proxy (GTK_WIDGET (socket), socket->plug_window, 
				 protocol, TRUE);
      
      gdk_flush ();
      gdk_error_trap_pop ();

      gdk_window_add_filter (socket->plug_window, 
			     gtk_socket_filter_func, socket);

      /* Add a pointer to the socket on our toplevel window */

      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (socket));
      if (toplevel && GTK_IS_WINDOW (toplevel))
	gtk_window_add_embedded_xid (GTK_WINDOW (toplevel), xid);

      gtk_widget_queue_resize (GTK_WIDGET (socket));
    }
}


static void
send_xembed_message (GtkSocket *socket,
		     glong      message,
		     glong      detail,
		     glong      data1,
		     glong      data2,
		     guint32    time)
{
  GTK_NOTE(PLUGSOCKET,
	   g_message ("GtkSocket: Sending XEMBED message of type %d", message));
  
  if (socket->plug_window)
    {
      XEvent xevent;

      xevent.xclient.window = GDK_WINDOW_XWINDOW (socket->plug_window);
      xevent.xclient.type = ClientMessage;
      xevent.xclient.message_type = gdk_atom_intern ("_XEMBED", FALSE);
      xevent.xclient.format = 32;
      xevent.xclient.data.l[0] = time;
      xevent.xclient.data.l[1] = message;
      xevent.xclient.data.l[2] = detail;
      xevent.xclient.data.l[3] = data1;
      xevent.xclient.data.l[4] = data2;

      gdk_error_trap_push ();
      XSendEvent (gdk_display,
		  GDK_WINDOW_XWINDOW (socket->plug_window),
		  False, NoEventMask, &xevent);
      gdk_flush ();
      gdk_error_trap_pop ();
    }
}

static gboolean
xembed_get_info (GdkWindow     *gdk_window,
		 unsigned long *version,
		 unsigned long *flags)
{
  Display *display = GDK_WINDOW_XDISPLAY (gdk_window);
  Window window = GDK_WINDOW_XWINDOW (gdk_window);
  Atom xembed_info_atom = gdk_atom_intern ("_XEMBED_INFO", FALSE);
  Atom type;
  int format;
  unsigned long nitems, bytes_after;
  unsigned char *data;
  unsigned long *data_long;
  int status;
  
  gdk_error_trap_push();
  status = XGetWindowProperty (display, window,
			       xembed_info_atom,
			       0, 2, False,
			       xembed_info_atom, &type, &format,
			       &nitems, &bytes_after, &data);
  gdk_error_trap_pop();

  if (status != Success)
    return FALSE;		/* Window vanished? */

  if (type == None)		/* No info property */
    return FALSE;

  if (type != xembed_info_atom)
    {
      g_warning ("_XEMBED_INFO property has wrong type\n");
      return FALSE;
    }
  
  if (nitems < 2)
    {
      g_warning ("_XEMBED_INFO too short\n");
      XFree (data);
      return FALSE;
    }
  
  data_long = (unsigned long *)data;
  if (version)
    *version = data_long[0];
  if (flags)
    *flags = data_long[1] & XEMBED_MAPPED;
  
  XFree (data);
  return TRUE;
}

static void
handle_xembed_message (GtkSocket *socket,
		       glong      message,
		       glong      detail,
		       glong      data1,
		       glong      data2,
		       guint32    time)
{
  switch (message)
    {
    case XEMBED_EMBEDDED_NOTIFY:
    case XEMBED_WINDOW_ACTIVATE:
    case XEMBED_WINDOW_DEACTIVATE:
    case XEMBED_MODALITY_ON:
    case XEMBED_MODALITY_OFF:
    case XEMBED_FOCUS_IN:
    case XEMBED_FOCUS_OUT:
      g_warning ("GtkSocket: Invalid _XEMBED message of type %ld received", message);
      break;
      
    case XEMBED_REQUEST_FOCUS:
      gtk_socket_claim_focus (socket);
      break;

    case XEMBED_FOCUS_NEXT:
    case XEMBED_FOCUS_PREV:
      {
	GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (socket));
	if (toplevel)
	  {
	    gtk_widget_child_focus (toplevel,
                                    (message == XEMBED_FOCUS_NEXT ?
                                     GTK_DIR_TAB_FORWARD : GTK_DIR_TAB_BACKWARD));
	  }
	break;
      }
      
    case XEMBED_GRAB_KEY:
#if 0
      add_grabbed_key (socket, data1, data2);
#endif
      break; 
    case XEMBED_UNGRAB_KEY:
#if 0      
      remove_grabbed_key (socket, data1, data2);
#endif
      break;
      
    default:
      GTK_NOTE(PLUGSOCKET,
	       g_message ("GtkSocket: Ignoring unknown _XEMBED message of type %ld", message));
      break;
    }
}

static void
map_request (GtkSocket *socket)
{
  if (!socket->is_mapped)
    {
      socket->is_mapped = TRUE;
      socket->need_map = TRUE;

      gtk_widget_queue_resize (GTK_WIDGET (socket));
    }
}

static void
unmap_notify (GtkSocket *socket)
{
  if (socket->is_mapped)
    {
      socket->is_mapped = FALSE;
      gtk_widget_queue_resize (GTK_WIDGET (socket));
    }
}

static GdkFilterReturn
gtk_socket_filter_func (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
  GtkSocket *socket;
  GtkWidget *widget;
  XEvent *xevent;

  GdkFilterReturn return_val;
  
  socket = GTK_SOCKET (data);
  widget = GTK_WIDGET (socket);
  xevent = (XEvent *)gdk_xevent;

  return_val = GDK_FILTER_CONTINUE;

  if (socket->plug_widget)
    return return_val;

  switch (xevent->type)
    {
    case ClientMessage:
      if (xevent->xclient.message_type == gdk_atom_intern ("_XEMBED", FALSE))
	{
	  handle_xembed_message (socket,
				 xevent->xclient.data.l[1],
				 xevent->xclient.data.l[2],
				 xevent->xclient.data.l[3],
				 xevent->xclient.data.l[4],
				 xevent->xclient.data.l[0]);
	  
	  
	  return_val = GDK_FILTER_REMOVE;
	}
      break;

    case CreateNotify:
      {
	XCreateWindowEvent *xcwe = &xevent->xcreatewindow;

	if (!socket->plug_window)
	  gtk_socket_add_window (socket, xcwe->window, FALSE);

	if (socket->plug_window)
	  {
	    gdk_error_trap_push ();
	    gdk_window_move_resize(socket->plug_window,
				   0, 0,
				   widget->allocation.width, 
				   widget->allocation.height);
	    gdk_flush ();
	    gdk_error_trap_pop ();
	
	    socket->request_width = xcwe->width;
	    socket->request_height = xcwe->height;
	    socket->have_size = TRUE;

	    GTK_NOTE(PLUGSOCKET,
		     g_message ("GtkSocket - window created with size: %d %d",
				socket->request_width,
				socket->request_height));
	  }
	
	return_val = GDK_FILTER_REMOVE;
	
	break;
      }

    case ConfigureRequest:
      {
	XConfigureRequestEvent *xcre = &xevent->xconfigurerequest;
	
	if (!socket->plug_window)
	  gtk_socket_add_window (socket, xcre->window, FALSE);
	
	if (socket->plug_window)
	  {
	    if (xcre->value_mask & (CWWidth | CWHeight))
	      {
		socket->request_width = xcre->width;
		socket->request_height = xcre->height;
		socket->have_size = TRUE;
		
		GTK_NOTE(PLUGSOCKET,
			 g_message ("GtkSocket - configure request: %d %d",
				    socket->request_width,
				    socket->request_height));
		
		gtk_widget_queue_resize (widget);
	      }
	    else if (xcre->value_mask & (CWX | CWY))
	      {
		gtk_socket_send_configure_event (socket);
	      }
	    /* Ignore stacking requests. */
	    
	    return_val = GDK_FILTER_REMOVE;
	  }
	break;
      }

    case DestroyNotify:
      {
	XDestroyWindowEvent *xdwe = &xevent->xdestroywindow;

	if (socket->plug_window && (xdwe->window == GDK_WINDOW_XWINDOW (socket->plug_window)))
	  {
	    GtkWidget *toplevel;

	    GTK_NOTE(PLUGSOCKET,
		     g_message ("GtkSocket - destroy notify"));
	    
	    toplevel = gtk_widget_get_toplevel (GTK_WIDGET (socket));
	    if (toplevel && GTK_IS_WINDOW (toplevel))
	      gtk_window_remove_embedded_xid (GTK_WINDOW (toplevel), xdwe->window);
	    
	    gdk_window_destroy_notify (socket->plug_window);
	    gtk_widget_destroy (widget);

	    socket->plug_window = NULL;
	    
	    return_val = GDK_FILTER_REMOVE;
	  }
	break;
      }

    case FocusIn:
      if (xevent->xfocus.mode == EMBEDDED_APP_WANTS_FOCUS)
	{
	  gtk_socket_claim_focus (socket);
	}
      else if (xevent->xfocus.detail == NotifyInferior)
	{
#if 0
	  GtkWidget *toplevel;
	  toplevel = gtk_widget_get_toplevel (widget);
	  
	  if (toplevel && GTK_IS_WINDOW (topelevel))
	    {
	      XSetInputFocus (GDK_DISPLAY (),
			      GDK_WINDOW_XWINDOW (toplevel->window),
			      RevertToParent, CurrentTime); /* FIXME? */
	    }
#endif
	}
      return_val = GDK_FILTER_REMOVE;
      break;
    case FocusOut:
      return_val = GDK_FILTER_REMOVE;
      break;
    case MapRequest:
      if (!socket->plug_window)
	gtk_socket_add_window (socket, xevent->xmaprequest.window, FALSE);
	
      if (socket->plug_window)
	{
	  GTK_NOTE(PLUGSOCKET,
		   g_message ("GtkSocket - Map Request"));

	  map_request (socket);
	  return_val = GDK_FILTER_REMOVE;
	}
      break;
    case PropertyNotify:
      if (socket->plug_window &&
	  xevent->xproperty.window == GDK_WINDOW_XWINDOW (socket->plug_window))
	{
	  GdkDragProtocol protocol;

	  if ((xevent->xproperty.atom == gdk_atom_intern ("XdndAware", FALSE)) ||
	      (xevent->xproperty.atom == gdk_atom_intern ("_MOTIF_DRAG_RECEIVER_INFO", FALSE)))
	    {
	      gdk_error_trap_push ();
	      if (gdk_drag_get_protocol (xevent->xproperty.window, &protocol))
		gtk_drag_dest_set_proxy (GTK_WIDGET (socket),
					 socket->plug_window,
					 protocol, TRUE);
	      gdk_flush ();
	      gdk_error_trap_pop ();
	    }
	  else if (xevent->xproperty.atom == gdk_atom_intern ("_XEMBED_INFO", FALSE))
	    {
	      unsigned long flags;
	      
	      if (xembed_get_info (socket->plug_window, NULL, &flags))
		{
		  gboolean was_mapped = socket->is_mapped;
		  gboolean is_mapped = (flags & XEMBED_MAPPED) != 0;

		  if (was_mapped != is_mapped)
		    {
		      if (is_mapped)
			map_request (socket);
		      else
			{
			  gdk_error_trap_push ();
			  gdk_window_show (socket->plug_window);
			  gdk_flush ();
			  gdk_error_trap_pop ();
			  
			  unmap_notify (socket);
			}
		    }
		}
	    }
		   
	  return_val = GDK_FILTER_REMOVE;
	}
      break;
    case UnmapNotify:
      if (socket->plug_window &&
	  xevent->xunmap.window == GDK_WINDOW_XWINDOW (socket->plug_window))
	{
	  GTK_NOTE(PLUGSOCKET,
		   g_message ("GtkSocket - Unmap notify"));

	  unmap_notify (socket);
	  return_val = GDK_FILTER_REMOVE;
	}
      break;
      
    }
  
  return return_val;
}
