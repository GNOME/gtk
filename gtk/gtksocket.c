/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* By Owen Taylor <otaylor@gtk.org>              98/4/4 */

#include "gdk/gdkx.h"
#include "gdk/gdkkeysyms.h"
#include "gtkwindow.h"
#include "gtksignal.h"
#include "gtksocket.h"
#include "gtkdnd.h"

/* Forward declararations */

static void gtk_socket_class_init               (GtkSocketClass    *klass);
static void gtk_socket_init                     (GtkSocket         *socket);
static void gtk_socket_realize                  (GtkWidget        *widget);
static void gtk_socket_unrealize                (GtkWidget        *widget);
static void gtk_socket_size_request             (GtkWidget      *widget,
					       GtkRequisition *requisition);
static void gtk_socket_size_allocate            (GtkWidget     *widget,
					       GtkAllocation *allocation);
static gint gtk_socket_focus_in_event           (GtkWidget *widget, 
						 GdkEventFocus *event);
static void gtk_socket_claim_focus              (GtkSocket *socket);
static gint gtk_socket_focus_out_event          (GtkWidget *widget, 
						 GdkEventFocus *event);
static void gtk_socket_send_configure_event     (GtkSocket *socket);
static gint gtk_socket_focus                    (GtkContainer *container, 
						 GtkDirectionType direction);
static GdkFilterReturn gtk_socket_filter_func   (GdkXEvent *gdk_xevent, 
						 GdkEvent *event, 
						 gpointer data);

#ifdef DEBUG_PLUGSOCKET
#define DPRINTF(arg) g_print arg
#else
#define DPRINTF(arg)
#endif

/* From Tk */
#define EMBEDDED_APP_WANTS_FOCUS NotifyNormal+20

/* Local data */

static GtkWidgetClass *parent_class = NULL;

guint
gtk_socket_get_type ()
{
  static guint socket_type = 0;

  if (!socket_type)
    {
      static const GtkTypeInfo socket_info =
      {
	"GtkSocket",
	sizeof (GtkSocket),
	sizeof (GtkSocketClass),
	(GtkClassInitFunc) gtk_socket_class_init,
	(GtkObjectInitFunc) gtk_socket_init,
	(GtkArgSetFunc) NULL,
	(GtkArgGetFunc) NULL
      };

      socket_type = gtk_type_unique (gtk_container_get_type (), &socket_info);
    }

  return socket_type;
}

static void
gtk_socket_class_init (GtkSocketClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = gtk_type_class (gtk_widget_get_type ());

  widget_class->realize = gtk_socket_realize;
  widget_class->unrealize = gtk_socket_unrealize;
  widget_class->size_request = gtk_socket_size_request;
  widget_class->size_allocate = gtk_socket_size_allocate;
  widget_class->focus_in_event = gtk_socket_focus_in_event;
  widget_class->focus_out_event = gtk_socket_focus_out_event;

  container_class->focus = gtk_socket_focus;
}

static void
gtk_socket_init (GtkSocket *socket)
{
  socket->request_width = 0;
  socket->request_height = 0;
  socket->current_width = 0;
  socket->current_height = 0;
  
  socket->plug_window = NULL;
  socket->same_app = FALSE;
  socket->focus_in = FALSE;
  socket->have_size = FALSE;
  socket->need_map = FALSE;
}

GtkWidget*
gtk_socket_new ()
{
  GtkSocket *socket;

  socket = gtk_type_new (gtk_socket_get_type ());

  return GTK_WIDGET (socket);
}

void           
gtk_socket_steal (GtkSocket *socket, guint32 id)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (socket);
  
  socket->plug_window = gdk_window_lookup (id);

  if (socket->plug_window && socket->plug_window->user_data)
    {
      /*
	GtkWidget *child_widget;

	child_widget = GTK_WIDGET (socket->plug_window->user_data);
      */

      g_warning("Stealing from same app not yet implemented");
      
      socket->same_app = TRUE;
    }
  else
    {
      socket->plug_window = gdk_window_foreign_new (id);
      socket->same_app = FALSE;
      socket->have_size = FALSE;

      XSelectInput (GDK_DISPLAY (),
		    GDK_WINDOW_XWINDOW(socket->plug_window),
		    StructureNotifyMask | PropertyChangeMask);

      gtk_widget_queue_resize (widget);
    }

  gdk_window_hide (socket->plug_window);
  gdk_window_reparent (socket->plug_window, widget->window, 0, 0);
  socket->need_map = TRUE;
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
    }

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

  if (!socket->have_size && socket->plug_window)
    {
      XSizeHints hints;
      long supplied;
      
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
    }

  requisition->width = socket->request_width;
  requisition->height = socket->request_height;
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

      if (socket->plug_window)
	{
	  if (!socket->need_map &&
	      (allocation->width == socket->current_width) &&
	      (allocation->height == socket->current_height))
	    {
	      gtk_socket_send_configure_event (socket);
	      DPRINTF(( "No change: %d %d\n",
		      allocation->width, allocation->height));
	    }
	  else
	    {
	      gdk_window_move_resize (socket->plug_window,
				      0, 0,
				      allocation->width, allocation->height);
	      DPRINTF(("configuring: %d %d\n",
		      allocation->width, allocation->height));
	      socket->current_width = allocation->width;
	      socket->current_height = allocation->height;
	    }

	  if (socket->need_map)
	    {
	      gdk_window_show (socket->plug_window);
	      socket->need_map = FALSE;
	    }

	}
    }
}

static gint
gtk_socket_focus_in_event (GtkWidget *widget, GdkEventFocus *event)
{
  GtkSocket *socket;
  g_return_val_if_fail (GTK_IS_SOCKET (widget), FALSE);
  socket = GTK_SOCKET (widget);

  DPRINTF (( "Got focus\n"));

  if (socket->focus_in && socket->plug_window)
    XSetInputFocus (GDK_DISPLAY (),
		    GDK_WINDOW_XWINDOW (socket->plug_window),
		    RevertToParent, GDK_CURRENT_TIME);
  
  return TRUE;
}

static gint
gtk_socket_focus_out_event (GtkWidget *widget, GdkEventFocus *event)
{
  GtkWidget *toplevel;
  GtkSocket *socket;

  g_return_val_if_fail (GTK_IS_SOCKET (widget), FALSE);
  socket = GTK_SOCKET (widget);

  toplevel = gtk_widget_get_ancestor (widget, gtk_window_get_type());
  
  if (toplevel)
    {
      XSetInputFocus (GDK_DISPLAY (),
		      GDK_WINDOW_XWINDOW (toplevel->window),
		      RevertToParent, CurrentTime); /* FIXME? */
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
    XSetInputFocus (GDK_DISPLAY (),
		    GDK_WINDOW_XWINDOW (socket->plug_window),
		    RevertToParent, GDK_CURRENT_TIME);
}

static gint 
gtk_socket_focus (GtkContainer *container, GtkDirectionType direction)
{
  GtkSocket *socket;

  g_return_val_if_fail (GTK_IS_SOCKET (container), FALSE);
  
  socket = GTK_SOCKET (container);

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

      
      XSendEvent (gdk_display,
		  GDK_WINDOW_XWINDOW (socket->plug_window),
		  False, NoEventMask, &xevent);
      return TRUE;
    }
  else
    {
      return FALSE;
    }
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
  
  XSendEvent (gdk_display,
	      GDK_WINDOW_XWINDOW (socket->plug_window),
	      False, NoEventMask, &event);
}

static void
gtk_socket_add_window (GtkSocket *socket, guint32 xid)
{
  socket->plug_window = gdk_window_lookup (xid);
  socket->same_app = TRUE;

  if (!socket->plug_window)
    {
      GtkWidget *toplevel;
      GdkDragProtocol protocol;

      socket->plug_window = gdk_window_foreign_new (xid);
      socket->same_app = FALSE;
      
      XSelectInput (GDK_DISPLAY (),
		    GDK_WINDOW_XWINDOW(socket->plug_window),
		    StructureNotifyMask | PropertyChangeMask);

      if (gdk_drag_get_protocol (xid, &protocol))
	gtk_drag_dest_set_proxy (GTK_WIDGET (socket), socket->plug_window, 
				 protocol, TRUE);

      gdk_window_add_filter (socket->plug_window, 
			     gtk_socket_filter_func, socket);

      /* Add a pointer to the socket on our toplevel window */

      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (socket));
      if (toplevel && GTK_IS_WINDOW (toplevel))
	{
	  gtk_window_add_embedded_xid (GTK_WINDOW (toplevel), xid);
	}
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

  switch (xevent->type)
    {
    case CreateNotify:
      {
	XCreateWindowEvent *xcwe = &xevent->xcreatewindow;

	if (!socket->plug_window)
	  {
	    g_print("Here!\n");

	    gtk_socket_add_window (socket, xcwe->window);
	    
	    gdk_window_move_resize(socket->plug_window,
				   0, 0,
				   widget->allocation.width, 
				   widget->allocation.height);
	
	    socket->request_width = xcwe->width;
	    socket->request_height = xcwe->height;
	    socket->have_size = TRUE;
	    
	    DPRINTF(("Window created with size: %d %d\n",
		     socket->request_width,
		     socket->request_height));
	    
	    gtk_widget_queue_resize (widget);
	  }
	
	return_val = GDK_FILTER_REMOVE;
	
	break;
      }

    case ConfigureRequest:
      {
	XConfigureRequestEvent *xcre = &xevent->xconfigurerequest;
	
	if (!socket->plug_window)
	  gtk_socket_add_window (socket, xcre->window);
	
	if (xcre->window == GDK_WINDOW_XWINDOW (socket->plug_window))
	  {
	    if (xcre->value_mask & (CWWidth | CWHeight))
	      {
		socket->request_width = xcre->width;
		socket->request_height = xcre->height;
		socket->have_size = TRUE;
		
		DPRINTF(("Configure request: %d %d\n",
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

	if (socket->plug_window &&
	    (xdwe->window == GDK_WINDOW_XWINDOW (socket->plug_window)))
	  {
	    GtkWidget *toplevel;
	    
	    DPRINTF(("Destroy Notify\n"));
	    
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
	  toplevel = gtk_widget_get_ancestor (widget, gtk_window_get_type());
	  
	  if (toplevel)
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
	gtk_socket_add_window (socket, xevent->xmaprequest.window);
	
      if (xevent->xmaprequest.window ==
	  GDK_WINDOW_XWINDOW (socket->plug_window))
	{
	  DPRINTF(("Map Request\n"));
	    
	  gdk_window_show (socket->plug_window);

	  return_val = GDK_FILTER_REMOVE;
	}
      break;
    case PropertyNotify:
      if (xevent->xproperty.window ==
	  GDK_WINDOW_XWINDOW (socket->plug_window))
	{
	  GdkDragProtocol protocol;

	  if ((xevent->xproperty.atom == gdk_atom_intern ("XdndAware", FALSE)) ||
	      (xevent->xproperty.atom == gdk_atom_intern ("_MOTIF_DRAG_RECEIVER_INFO", FALSE)))
	    {
	      if (gdk_drag_get_protocol (xevent->xproperty.window, &protocol))
		gtk_drag_dest_set_proxy (GTK_WIDGET (socket),
					 socket->plug_window,
					 protocol, TRUE);
	    }
	  return_val = GDK_FILTER_REMOVE;
	}
    }

  return return_val;
}
