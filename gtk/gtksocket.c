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

#include <config.h>
#include <string.h>

#include "gdk/gdkkeysyms.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkwindow.h"
#include "gtkplug.h"
#include "gtkprivate.h"
#include "gtksocket.h"
#include "gtkdnd.h"

#include "x11/gdkx.h"

#include "gtkxembed.h"
#include "gtkalias.h"

typedef struct _GtkSocketPrivate GtkSocketPrivate;

struct _GtkSocketPrivate
{
  gint resize_count;
};

/* Forward declararations */

static void     gtk_socket_class_init           (GtkSocketClass   *klass);
static void     gtk_socket_init                 (GtkSocket        *socket);
static void     gtk_socket_finalize             (GObject          *object);
static void     gtk_socket_notify               (GObject          *object,
						 GParamSpec       *pspec);
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
static gboolean gtk_socket_key_event            (GtkWidget        *widget,
						 GdkEventKey      *event);
static void     gtk_socket_claim_focus          (GtkSocket        *socket,
						 gboolean          send_event);
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

static gboolean xembed_get_info     (GdkWindow     *gdk_window,
				     unsigned long *version,
				     unsigned long *flags);

/* From Tk */
#define EMBEDDED_APP_WANTS_FOCUS NotifyNormal+20

/* Local data */

enum {
  PLUG_ADDED,
  PLUG_REMOVED,
  LAST_SIGNAL
}; 

static guint socket_signals[LAST_SIGNAL] = { 0 };

static GtkWidgetClass *parent_class = NULL;

static GtkSocketPrivate *
gtk_socket_get_private (GtkSocket *socket)
{
  GtkSocketPrivate *private;
  static GQuark private_quark = 0;

  if (!private_quark)
    private_quark = g_quark_from_static_string ("gtk-socket-private");

  private = g_object_get_qdata (G_OBJECT (socket), private_quark);

  if (!private)
    {
      private = g_new0 (GtkSocketPrivate, 1);
      private->resize_count = 0;
      
      g_object_set_qdata_full (G_OBJECT (socket), private_quark,
			       private, (GDestroyNotify) g_free);
    }

  return private;
}

GType
gtk_socket_get_type (void)
{
  static GType socket_type = 0;

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

      socket_type = g_type_register_static (GTK_TYPE_CONTAINER, "GtkSocket",
					    &socket_info, 0);
    }

  return socket_type;
}

static void
gtk_socket_finalize (GObject *object)
{
  GtkSocket *socket = GTK_SOCKET (object);
  
  g_object_unref (socket->accel_group);
  socket->accel_group = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_socket_class_init (GtkSocketClass *class)
{
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = g_type_class_peek_parent (class);

  gobject_class->finalize = gtk_socket_finalize;
  gobject_class->notify = gtk_socket_notify;

  widget_class->realize = gtk_socket_realize;
  widget_class->unrealize = gtk_socket_unrealize;
  widget_class->size_request = gtk_socket_size_request;
  widget_class->size_allocate = gtk_socket_size_allocate;
  widget_class->hierarchy_changed = gtk_socket_hierarchy_changed;
  widget_class->grab_notify = gtk_socket_grab_notify;
  widget_class->key_press_event = gtk_socket_key_event;
  widget_class->key_release_event = gtk_socket_key_event;
  widget_class->focus = gtk_socket_focus;

  /* We don't want to show_all/hide_all the in-process
   * plug, if any.
   */
  widget_class->show_all = gtk_widget_show;
  widget_class->hide_all = gtk_widget_hide;
  
  container_class->remove = gtk_socket_remove;
  container_class->forall = gtk_socket_forall;

  socket_signals[PLUG_ADDED] =
    g_signal_new ("plug_added",
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkSocketClass, plug_added),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  socket_signals[PLUG_REMOVED] =
    g_signal_new ("plug_removed",
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkSocketClass, plug_removed),
                  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);
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
  socket->active = FALSE;

  socket->accel_group = gtk_accel_group_new ();
  g_object_set_data (G_OBJECT (socket->accel_group), "gtk-socket", socket);
}

/**
 * gtk_socket_new:
 * 
 * Create a new empty #GtkSocket.
 * 
 * Return value:  the new #GtkSocket.
 **/
GtkWidget*
gtk_socket_new (void)
{
  GtkSocket *socket;

  socket = g_object_new (GTK_TYPE_SOCKET, NULL);

  return GTK_WIDGET (socket);
}

/**
 * gtk_socket_steal:
 * @socket_: a #GtkSocket
 * @wid: the window ID of an existing toplevel window.
 * 
 * Reparents a pre-existing toplevel window into a #GtkSocket. This is
 * meant to embed clients that do not know about embedding into a
 * #GtkSocket, however doing so is inherently unreliable, and using
 * this function is not recommended.
 *
 * The #GtkSocket must have already be added into a toplevel window
 *  before you can make this call.
 **/
void           
gtk_socket_steal (GtkSocket *socket, GdkNativeWindow wid)
{
  g_return_if_fail (GTK_IS_SOCKET (socket));
  g_return_if_fail (GTK_WIDGET_ANCHORED (socket));

  if (!GTK_WIDGET_REALIZED (socket))
    gtk_widget_realize (GTK_WIDGET (socket));

  gtk_socket_add_window (socket, wid, TRUE);
}

/**
 * gtk_socket_add_id:
 * @socket_: a #GtkSocket
 * @window_id: the window ID of a client participating in the XEMBED protocol.
 *
 * Adds an XEMBED client, such as a #GtkPlug, to the #GtkSocket.  The
 * client may be in the same process or in a different process. 
 * 
 * To embed a #GtkPlug in a #GtkSocket, you can either create the
 * #GtkPlug with <literal>gtk_plug_new (0)</literal>, call 
 * gtk_plug_get_id() to get the window ID of the plug, and then pass that to the
 * gtk_socket_add_id(), or you can call gtk_socket_get_id() to get the
 * window ID for the socket, and call gtk_plug_new() passing in that
 * ID.
 *
 * The #GtkSocket must have already be added into a toplevel window
 *  before you can make this call.
 **/
void           
gtk_socket_add_id (GtkSocket *socket, GdkNativeWindow window_id)
{
  g_return_if_fail (GTK_IS_SOCKET (socket));
  g_return_if_fail (GTK_WIDGET_ANCHORED (socket));

  if (!GTK_WIDGET_REALIZED (socket))
    gtk_widget_realize (GTK_WIDGET (socket));

  gtk_socket_add_window (socket, window_id, TRUE);
}

/**
 * gtk_socket_get_id:
 * @socket_: a #GtkSocket.
 * 
 * Gets the window ID of a #GtkSocket widget, which can then
 * be used to create a client embedded inside the socket, for
 * instance with gtk_plug_new(). 
 *
 * The #GtkSocket must have already be added into a toplevel window 
 * before you can make this call.
 * 
 * Return value: the window ID for the socket
 **/
GdkNativeWindow
gtk_socket_get_id (GtkSocket *socket)
{
  g_return_val_if_fail (GTK_IS_SOCKET (socket), 0);
  g_return_val_if_fail (GTK_WIDGET_ANCHORED (socket), 0);

  if (!GTK_WIDGET_REALIZED (socket))
    gtk_widget_realize (GTK_WIDGET (socket));

  return GDK_WINDOW_XWINDOW (GTK_WIDGET (socket)->window);
}

static void
gtk_socket_realize (GtkWidget *widget)
{
  GtkSocket *socket;
  GdkWindowAttr attributes;
  gint attributes_mask;
  XWindowAttributes xattrs;

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

  XGetWindowAttributes (GDK_WINDOW_XDISPLAY (widget->window),
			GDK_WINDOW_XWINDOW (widget->window),
			&xattrs);

  XSelectInput (GDK_WINDOW_XDISPLAY (widget->window),
		GDK_WINDOW_XWINDOW (widget->window), 
		xattrs.your_event_mask | 
		SubstructureNotifyMask | SubstructureRedirectMask);

  gdk_window_add_filter (widget->window, gtk_socket_filter_func, widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  /* We sync here so that we make sure that if the XID for
   * our window is passed to another application, SubstructureRedirectMask
   * will be set by the time the other app creates its window.
   */
  gdk_display_sync (gtk_widget_get_display (widget));
}

static void
gtk_socket_end_embedding (GtkSocket *socket)
{
  GtkSocketPrivate *private = gtk_socket_get_private (socket);
  GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (socket));
  gint i;
  
  if (toplevel && GTK_IS_WINDOW (toplevel))
    gtk_window_remove_embedded_xid (GTK_WINDOW (toplevel), 
				    GDK_WINDOW_XWINDOW (socket->plug_window));

  g_object_unref (socket->plug_window);
  socket->plug_window = NULL;
  socket->current_width = 0;
  socket->current_height = 0;
  private->resize_count = 0;

  /* Remove from end to avoid indexes shifting. This is evil */
  for (i = socket->accel_group->n_accels - 1; i >= 0; i--)
    {
      GtkAccelGroupEntry *accel_entry = &socket->accel_group->priv_accels[i];
      gtk_accel_group_disconnect (socket->accel_group, accel_entry->closure);
    }
}

static void
gtk_socket_unrealize (GtkWidget *widget)
{
  GtkSocket *socket = GTK_SOCKET (widget);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_REALIZED);

  if (socket->plug_widget)
    {
      _gtk_plug_remove_from_socket (GTK_PLUG (socket->plug_widget), socket);
    }
  else if (socket->plug_window)
    {
      gtk_socket_end_embedding (socket);
    }

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}
  
static void 
gtk_socket_size_request (GtkWidget      *widget,
			 GtkRequisition *requisition)
{
  GtkSocket *socket = GTK_SOCKET (widget);

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

	  socket->request_width = 1;
	  socket->request_height = 1;
	  
	  if (XGetWMNormalHints (GDK_WINDOW_XDISPLAY (socket->plug_window),
				 GDK_WINDOW_XWINDOW (socket->plug_window),
				 &hints, &supplied))
	    {
	      if (hints.flags & PMinSize)
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
	  socket->have_size = TRUE;
	  
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
	  GtkSocketPrivate *private = gtk_socket_get_private (socket);
	  
	  gdk_error_trap_push ();
	  
	  if (allocation->width != socket->current_width ||
	      allocation->height != socket->current_height)
	    {
	      gdk_window_move_resize (socket->plug_window,
				      0, 0,
				      allocation->width, allocation->height);
	      if (private->resize_count)
		private->resize_count--;
	      
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

	  while (private->resize_count)
 	    {
 	      gtk_socket_send_configure_event (socket);
 	      private->resize_count--;
 	      GTK_NOTE(PLUGSOCKET,
 		       g_message ("GtkSocket - sending synthetic configure: %d %d",
 				  allocation->width, allocation->height));
 	    }
	  
	  gdk_display_sync (gtk_widget_get_display (widget));
	  gdk_error_trap_pop ();
	}
    }
}

typedef struct
{
  guint			 accel_key;
  GdkModifierType	 accel_mods;
} GrabbedKey;

static gboolean
activate_key (GtkAccelGroup  *accel_group,
	      GObject        *acceleratable,
	      guint           accel_key,
	      GdkModifierType accel_mods,
	      GrabbedKey     *grabbed_key)
{
  XEvent xevent;
  GdkEvent *gdk_event = gtk_get_current_event ();
  
  GtkSocket *socket = g_object_get_data (G_OBJECT (accel_group), "gtk-socket");
  GdkScreen *screen = gdk_drawable_get_screen (socket->plug_window);
  gboolean retval = FALSE;

  if (gdk_event && gdk_event->type == GDK_KEY_PRESS && socket->plug_window)
    {
      xevent.xkey.type = KeyPress;
      xevent.xkey.window = GDK_WINDOW_XWINDOW (socket->plug_window);
      xevent.xkey.root = GDK_WINDOW_XWINDOW (gdk_screen_get_root_window (screen));
      xevent.xkey.subwindow = None;
      xevent.xkey.time = gdk_event->key.time;
      xevent.xkey.x = 0;
      xevent.xkey.y = 0;
      xevent.xkey.x_root = 0;
      xevent.xkey.y_root = 0;
      xevent.xkey.state = gdk_event->key.state;
      xevent.xkey.keycode = gdk_event->key.hardware_keycode;
      xevent.xkey.same_screen = True;

      gdk_error_trap_push ();
      XSendEvent (GDK_WINDOW_XDISPLAY (socket->plug_window),
		  GDK_WINDOW_XWINDOW (socket->plug_window),
		  False, KeyPressMask, &xevent);
      gdk_display_sync (gdk_screen_get_display (screen));
      gdk_error_trap_pop ();

      retval = TRUE;
    }

  if (gdk_event)
    gdk_event_free (gdk_event);

  return retval;
}

static gboolean
find_accel_key (GtkAccelKey *key,
		GClosure    *closure,
		gpointer     data)
{
  GrabbedKey *grabbed_key = data;
  
  return (key->accel_key == grabbed_key->accel_key &&
	  key->accel_mods == grabbed_key->accel_mods);
}

static void
add_grabbed_key (GtkSocket       *socket,
		 guint            keyval,
		 GdkModifierType  modifiers)
{
  GClosure *closure;
  GrabbedKey *grabbed_key;

  grabbed_key = g_new (GrabbedKey, 1);
  
  grabbed_key->accel_key = keyval;
  grabbed_key->accel_mods = modifiers;

  if (gtk_accel_group_find (socket->accel_group,
			    find_accel_key,
			    &grabbed_key))
    {
      g_warning ("GtkSocket: request to add already present grabbed key %u,%#x\n",
		 keyval, modifiers);
      g_free (grabbed_key);
      return;
    }

  closure = g_cclosure_new (G_CALLBACK (activate_key), grabbed_key, (GClosureNotify)g_free);

  gtk_accel_group_connect (socket->accel_group, keyval, modifiers, GTK_ACCEL_LOCKED,
			   closure);
}

static void
remove_grabbed_key (GtkSocket      *socket,
		    guint           keyval,
		    GdkModifierType modifiers)
{
  gint i;

  for (i = 0; i < socket->accel_group->n_accels; i++)
    {
      GtkAccelGroupEntry *accel_entry = &socket->accel_group->priv_accels[i];
      if (accel_entry->key.accel_key == keyval &&
	  accel_entry->key.accel_mods == modifiers)
	{
	  gtk_accel_group_disconnect (socket->accel_group,
				      accel_entry->closure);
	  return;
	}
    }

  g_warning ("GtkSocket: request to remove non-present grabbed key %u,%#x\n",
	     keyval, modifiers);
}

static void
socket_update_focus_in (GtkSocket *socket)
{
  gboolean focus_in = FALSE;

  if (socket->plug_window)
    {
      GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (socket));
      if (GTK_WIDGET_TOPLEVEL (toplevel) &&
	  GTK_WINDOW (toplevel)->has_toplevel_focus &&
	  gtk_widget_is_focus (GTK_WIDGET (socket)))
	focus_in = TRUE;
    }

  if (focus_in != socket->focus_in)
    {
      socket->focus_in = focus_in;

      if (focus_in)
	_gtk_xembed_send_focus_message (socket->plug_window,
					XEMBED_FOCUS_IN, XEMBED_FOCUS_CURRENT);
      else
	_gtk_xembed_send_message (socket->plug_window,
				  XEMBED_FOCUS_OUT, 0, 0, 0);
    }
}

static void
socket_update_active (GtkSocket *socket)
{
  gboolean active = FALSE;

  if (socket->plug_window)
    {
      GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (socket));
      if (GTK_WIDGET_TOPLEVEL (toplevel) &&
	  GTK_WINDOW (toplevel)->is_active)
	active = TRUE;
    }

  if (active != socket->active)
    {
      socket->active = active;

      _gtk_xembed_send_message (socket->plug_window,
				active ? XEMBED_WINDOW_ACTIVATE : XEMBED_WINDOW_DEACTIVATE,
				0, 0, 0);
    }
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
	  gtk_window_remove_accel_group (GTK_WINDOW (socket->toplevel), socket->accel_group);
	  g_signal_handlers_disconnect_by_func (socket->toplevel,
						socket_update_focus_in,
						socket);
	  g_signal_handlers_disconnect_by_func (socket->toplevel,
						socket_update_active,
						socket);
	}

      socket->toplevel = toplevel;

      if (toplevel)
	{
	  gtk_window_add_accel_group (GTK_WINDOW (socket->toplevel), socket->accel_group);
	  g_signal_connect_swapped (socket->toplevel, "notify::has-toplevel-focus",
				    G_CALLBACK (socket_update_focus_in), socket);
	  g_signal_connect_swapped (socket->toplevel, "notify::is-active",
				    G_CALLBACK (socket_update_active), socket);
	}

      socket_update_focus_in (socket);
      socket_update_active (socket);
    }
}

static void
gtk_socket_grab_notify (GtkWidget *widget,
			gboolean   was_grabbed)
{
  GtkSocket *socket = GTK_SOCKET (widget);

  if (!socket->same_app)
    _gtk_xembed_send_message (GTK_SOCKET (widget)->plug_window,
			      was_grabbed ? XEMBED_MODALITY_OFF : XEMBED_MODALITY_ON,
			      0, 0, 0);
}

static gboolean
gtk_socket_key_event (GtkWidget   *widget,
                      GdkEventKey *event)
{
  GtkSocket *socket = GTK_SOCKET (widget);
  
  if (GTK_WIDGET_HAS_FOCUS (socket) && socket->plug_window && !socket->plug_widget)
    {
      GdkScreen *screen = gdk_drawable_get_screen (socket->plug_window);
      XEvent xevent;
      
      xevent.xkey.type = (event->type == GDK_KEY_PRESS) ? KeyPress : KeyRelease;
      xevent.xkey.window = GDK_WINDOW_XWINDOW (socket->plug_window);
      xevent.xkey.root = GDK_WINDOW_XWINDOW (gdk_screen_get_root_window (screen));
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
      XSendEvent (GDK_WINDOW_XDISPLAY (socket->plug_window),
		  GDK_WINDOW_XWINDOW (socket->plug_window),
		  False, NoEventMask, &xevent);
      gdk_display_sync (gtk_widget_get_display (widget));
      gdk_error_trap_pop ();
      
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_socket_notify (GObject    *object,
		   GParamSpec *pspec)
{
  if (!strcmp (pspec->name, "is_focus"))
    return;

  socket_update_focus_in (GTK_SOCKET (object));
}

static void
gtk_socket_claim_focus (GtkSocket *socket,
			gboolean   send_event)
{
  if (!send_event)
    socket->focus_in = TRUE;	/* Otherwise, our notify handler will send FOCUS_IN  */
      
  /* Oh, the trickery... */
  
  GTK_WIDGET_SET_FLAGS (socket, GTK_CAN_FOCUS);
  gtk_widget_grab_focus (GTK_WIDGET (socket));
  GTK_WIDGET_UNSET_FLAGS (socket, GTK_CAN_FOCUS);
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

  if (!gtk_widget_is_focus (widget))
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
      
      _gtk_xembed_send_focus_message (socket->plug_window,
				      XEMBED_FOCUS_IN, detail);

      gtk_socket_claim_focus (socket, FALSE);
 
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_socket_remove (GtkContainer *container,
		   GtkWidget    *child)
{
  GtkSocket *socket = GTK_SOCKET (container);

  g_return_if_fail (child == socket->plug_widget);

  _gtk_plug_remove_from_socket (GTK_PLUG (socket->plug_widget), socket);
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
  gint x, y;

  g_return_if_fail (socket->plug_window != NULL);

  event.xconfigure.type = ConfigureNotify;

  event.xconfigure.event = GDK_WINDOW_XWINDOW (socket->plug_window);
  event.xconfigure.window = GDK_WINDOW_XWINDOW (socket->plug_window);

  /* The ICCCM says that synthetic events should have root relative
   * coordinates. We still aren't really ICCCM compliant, since
   * we don't send events when the real toplevel is moved.
   */
  gdk_error_trap_push ();
  gdk_window_get_origin (socket->plug_window, &x, &y);
  gdk_error_trap_pop ();
			 
  event.xconfigure.x = x;
  event.xconfigure.y = y;
  event.xconfigure.width = GTK_WIDGET(socket)->allocation.width;
  event.xconfigure.height = GTK_WIDGET(socket)->allocation.height;

  event.xconfigure.border_width = 0;
  event.xconfigure.above = None;
  event.xconfigure.override_redirect = False;

  gdk_error_trap_push ();
  XSendEvent (GDK_WINDOW_XDISPLAY (socket->plug_window),
	      GDK_WINDOW_XWINDOW (socket->plug_window),
	      False, NoEventMask, &event);
  gdk_display_sync (gtk_widget_get_display (GTK_WIDGET (socket)));
  gdk_error_trap_pop ();
}

static void
gtk_socket_add_window (GtkSocket        *socket,
		       GdkNativeWindow   xid,
		       gboolean          need_reparent)
{
  GtkWidget *widget = GTK_WIDGET (socket);
  GdkDisplay *display = gtk_widget_get_display (widget);
  gpointer user_data = NULL;
  
  socket->plug_window = gdk_window_lookup_for_display (display, xid);

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
	  socket->plug_window = gdk_window_foreign_new_for_display (display, xid);
	  if (!socket->plug_window) /* was deleted before we could get it */
	    {
	      gdk_error_trap_pop ();
	      return;
	    }
	}
	
      XSelectInput (GDK_DISPLAY_XDISPLAY (display),
		    GDK_WINDOW_XWINDOW (socket->plug_window),
		    StructureNotifyMask | PropertyChangeMask);
      
      if (gdk_error_trap_pop ())
	{
	  g_object_unref (socket->plug_window);
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
	  socket->xembed_version = MIN (GTK_XEMBED_PROTOCOL_VERSION, version);
	  socket->is_mapped = (flags & XEMBED_MAPPED) != 0;
	}
      else
	{
	  /* FIXME, we should probably actually check the state before we started */
	  
	  socket->is_mapped = TRUE;
	}
      
      socket->need_map = socket->is_mapped;

      if (gdk_drag_get_protocol_for_display (display, xid, &protocol))
	gtk_drag_dest_set_proxy (GTK_WIDGET (socket), socket->plug_window, 
				 protocol, TRUE);

      gdk_display_sync (display);
      gdk_error_trap_pop ();

      gdk_window_add_filter (socket->plug_window, 
			     gtk_socket_filter_func, socket);

      /* Add a pointer to the socket on our toplevel window */

      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (socket));
      if (toplevel && GTK_IS_WINDOW (toplevel))
	gtk_window_add_embedded_xid (GTK_WINDOW (toplevel), xid);

      _gtk_xembed_send_message (socket->plug_window,
				XEMBED_EMBEDDED_NOTIFY, 0,
				GDK_WINDOW_XWINDOW (widget->window),
				socket->xembed_version);
      socket_update_active (socket);
      socket_update_focus_in (socket);

      gtk_widget_queue_resize (GTK_WIDGET (socket));
    }

  if (socket->plug_window)
    g_signal_emit (socket, socket_signals[PLUG_ADDED], 0);
}

static gboolean
xembed_get_info (GdkWindow     *window,
		 unsigned long *version,
		 unsigned long *flags)
{
  GdkDisplay *display = gdk_drawable_get_display (window);
  Atom xembed_info_atom = gdk_x11_get_xatom_by_name_for_display (display, "_XEMBED_INFO");
  Atom type;
  int format;
  unsigned long nitems, bytes_after;
  unsigned char *data;
  unsigned long *data_long;
  int status;
  
  gdk_error_trap_push();
  status = XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
			       GDK_WINDOW_XWINDOW (window),
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
advance_toplevel_focus (GtkSocket        *socket,
			GtkDirectionType  direction)
{
  GtkBin *bin;
  GtkWindow *window;
  GtkContainer *container;
  GtkWidget *toplevel;
  GtkWidget *old_focus_child;
  GtkWidget *parent;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (socket));
  if (!toplevel)
    return;

  if (!GTK_WIDGET_TOPLEVEL (toplevel) || GTK_IS_PLUG (toplevel))
    {
      gtk_widget_child_focus (toplevel,direction);
      return;
    }

  container = GTK_CONTAINER (toplevel);
  window = GTK_WINDOW (toplevel);
  bin = GTK_BIN (toplevel);

  /* This is a copy of gtk_window_focus(), modified so that we
   * can detect wrap-around.
   */
  old_focus_child = container->focus_child;
  
  if (old_focus_child)
    {
      if (gtk_widget_child_focus (old_focus_child, direction))
	return;

      /* We are allowed exactly one wrap-around per sequence of focus
       * events
       */
      if (_gtk_xembed_get_focus_wrapped ())
	return;
      else
	_gtk_xembed_set_focus_wrapped ();
    }

  if (window->focus_widget)
    {
      /* Wrapped off the end, clear the focus setting for the toplevel */
      parent = window->focus_widget->parent;
      while (parent)
	{
	  gtk_container_set_focus_child (GTK_CONTAINER (parent), NULL);
	  parent = GTK_WIDGET (parent)->parent;
	}
      
      gtk_window_set_focus (GTK_WINDOW (container), NULL);
    }

  /* Now try to focus the first widget in the window */
  if (bin->child)
    {
      if (gtk_widget_child_focus (bin->child, direction))
        return;
    }
}

static void
handle_xembed_message (GtkSocket        *socket,
		       XEmbedMessageType message,
		       glong             detail,
		       glong             data1,
		       glong             data2,
		       guint32           time)
{
  GTK_NOTE (PLUGSOCKET,
	    g_message ("GtkSocket: Message of type %d received", message));
  
  switch (message)
    {
    case XEMBED_EMBEDDED_NOTIFY:
    case XEMBED_WINDOW_ACTIVATE:
    case XEMBED_WINDOW_DEACTIVATE:
    case XEMBED_MODALITY_ON:
    case XEMBED_MODALITY_OFF:
    case XEMBED_FOCUS_IN:
    case XEMBED_FOCUS_OUT:
      g_warning ("GtkSocket: Invalid _XEMBED message of type %d received", message);
      break;
      
    case XEMBED_REQUEST_FOCUS:
      gtk_socket_claim_focus (socket, TRUE);
      break;

    case XEMBED_FOCUS_NEXT:
    case XEMBED_FOCUS_PREV:
      advance_toplevel_focus (socket,
			      (message == XEMBED_FOCUS_NEXT ?
			       GTK_DIR_TAB_FORWARD : GTK_DIR_TAB_BACKWARD));
      break;
      
    case XEMBED_GTK_GRAB_KEY:
      add_grabbed_key (socket, data1, data2);
      break; 
    case XEMBED_GTK_UNGRAB_KEY:
      remove_grabbed_key (socket, data1, data2);
      break;

    case XEMBED_GRAB_KEY:
    case XEMBED_UNGRAB_KEY:
      break;
      
    default:
      GTK_NOTE (PLUGSOCKET,
		g_message ("GtkSocket: Ignoring unknown _XEMBED message of type %d", message));
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
  GdkDisplay *display;
  XEvent *xevent;

  GdkFilterReturn return_val;
  
  socket = GTK_SOCKET (data);
  widget = GTK_WIDGET (socket);
  xevent = (XEvent *)gdk_xevent;
  display = gtk_widget_get_display (widget);

  return_val = GDK_FILTER_CONTINUE;

  if (socket->plug_widget)
    return return_val;

  switch (xevent->type)
    {
    case ClientMessage:
      if (xevent->xclient.message_type == gdk_x11_get_xatom_by_name_for_display (display, "_XEMBED"))
	{
	  _gtk_xembed_push_message (xevent);
	  handle_xembed_message (socket,
				 xevent->xclient.data.l[1],
				 xevent->xclient.data.l[2],
				 xevent->xclient.data.l[3],
				 xevent->xclient.data.l[4],
				 xevent->xclient.data.l[0]);
	  _gtk_xembed_pop_message ();
	  
	  return_val = GDK_FILTER_REMOVE;
	}
      break;

    case CreateNotify:
      {
	XCreateWindowEvent *xcwe = &xevent->xcreatewindow;

	if (!socket->plug_window)
	  {
	    gtk_socket_add_window (socket, xcwe->window, FALSE);

	    if (socket->plug_window)
	      {
		GTK_NOTE(PLUGSOCKET,
			 g_message ("GtkSocket - window created"));
	      }
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
	    GtkSocketPrivate *private = gtk_socket_get_private (socket);
	    
	    if (xcre->value_mask & (CWWidth | CWHeight))
	      {
		GTK_NOTE(PLUGSOCKET,
			 g_message ("GtkSocket - configure request: %d %d",
				    socket->request_width,
				    socket->request_height));

		private->resize_count++;
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

	/* Note that we get destroy notifies both from SubstructureNotify on
	 * our window and StructureNotify on socket->plug_window
	 */
	if (socket->plug_window && (xdwe->window == GDK_WINDOW_XWINDOW (socket->plug_window)))
	  {
	    gboolean result;
	    
	    GTK_NOTE(PLUGSOCKET,
		     g_message ("GtkSocket - destroy notify"));
	    
	    gdk_window_destroy_notify (socket->plug_window);
	    gtk_socket_end_embedding (socket);

	    g_object_ref (widget);
	    g_signal_emit (widget, socket_signals[PLUG_REMOVED], 0, &result);
	    if (!result)
	      gtk_widget_destroy (widget);
	    g_object_unref (widget);
	    
	    return_val = GDK_FILTER_REMOVE;
	  }
	break;
      }

    case FocusIn:
      if (xevent->xfocus.mode == EMBEDDED_APP_WANTS_FOCUS)
	{
	  gtk_socket_claim_focus (socket, TRUE);
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

	  if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "WM_NORMAL_HINTS"))
	    {
	      socket->have_size = FALSE;
	      gtk_widget_queue_resize (widget);
	      return_val = GDK_FILTER_REMOVE;
	    }
	  else if ((xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "XdndAware")) ||
	      (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "_MOTIF_DRAG_RECEIVER_INFO")))
	    {
	      gdk_error_trap_push ();
	      if (gdk_drag_get_protocol_for_display (display,
						     xevent->xproperty.window,
						     &protocol))
		gtk_drag_dest_set_proxy (GTK_WIDGET (socket),
					 socket->plug_window,
					 protocol, TRUE);

	      gdk_display_sync (display);
	      gdk_error_trap_pop ();
	      return_val = GDK_FILTER_REMOVE;
	    }
	  else if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "_XEMBED_INFO"))
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
	      return_val = GDK_FILTER_REMOVE;
	    }
	}
      break;
    case ReparentNotify:
      {
	XReparentEvent *xre = &xevent->xreparent;

	if (!socket->plug_window && xre->parent == GDK_WINDOW_XWINDOW (widget->window))
	  {
	    gtk_socket_add_window (socket, xre->window, FALSE);
	    
	    if (socket->plug_window)
	      {
		GTK_NOTE(PLUGSOCKET,
			 g_message ("GtkSocket - window reparented"));
	      }
	    
	    return_val = GDK_FILTER_REMOVE;
	  }
	
	break;
      }
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

#define __GTK_SOCKET_C__
#include "gtkaliasdef.c"
