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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.Free
 */

/* By Owen Taylor <otaylor@gtk.org>              98/4/4 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gtksocketprivate.h"

#include <string.h>

#include "gtkmarshalers.h"
#include "gtksizerequest.h"
#include "gtkplug.h"
#include "gtkprivate.h"
#include "gtkdnd.h"
#include "gtkdebug.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkwidgetprivate.h"

#include <gdk/gdkx.h>
#include <gdk/gdkprivate.h>

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#include "gtkxembed.h"


/**
 * SECTION:gtksocket
 * @Short_description: Container for widgets from other processes
 * @Title: GtkSocket
 * @include: gtk/gtkx.h
 * @See_also: #GtkPlug, <ulink url="http://www.freedesktop.org/Standards/xembed-spec">XEmbed</ulink>
 *
 * Together with #GtkPlug, #GtkSocket provides the ability to embed
 * widgets from one process into another process in a fashion that
 * is transparent to the user. One process creates a #GtkSocket widget
 * and passes that widget's window ID to the other process, which then
 * creates a #GtkPlug with that window ID. Any widgets contained in the
 * #GtkPlug then will appear inside the first application's window.
 *
 * The socket's window ID is obtained by using gtk_socket_get_id().
 * Before using this function, the socket must have been realized,
 * and for hence, have been added to its parent.
 *
 * <example>
 * <title>Obtaining the window ID of a socket.</title>
 * <programlisting>
 * GtkWidget *socket = gtk_socket_new (<!-- -->);
 * gtk_widget_show (socket);
 * gtk_container_add (GTK_CONTAINER (parent), socket);
 *
 * /&ast; The following call is only necessary if one of
 *  * the ancestors of the socket is not yet visible.
 *  &ast;/
 * gtk_widget_realize (socket);
 * g_print ("The ID of the sockets window is %#x\n",
 *          gtk_socket_get_id (socket));
 * </programlisting>
 * </example>
 *
 * Note that if you pass the window ID of the socket to another
 * process that will create a plug in the socket, you must make
 * sure that the socket widget is not destroyed until that plug
 * is created. Violating this rule will cause unpredictable
 * consequences, the most likely consequence being that the plug
 * will appear as a separate toplevel window. You can check if
 * the plug has been created by using gtk_socket_get_plug_window().
 * If it returns a non-%NULL value, then the plug has been
 * successfully created inside of the socket.
 *
 * When GTK+ is notified that the embedded window has been destroyed,
 * then it will destroy the socket as well. You should always,
 * therefore, be prepared for your sockets to be destroyed at any
 * time when the main event loop is running. To prevent this from
 * happening, you can connect to the #GtkSocket::plug-removed signal.
 *
 * The communication between a #GtkSocket and a #GtkPlug follows the
 * <ulink url="http://www.freedesktop.org/Standards/xembed-spec">XEmbed</ulink>
 * protocol. This protocol has also been implemented in other toolkits,
 * e.g. <application>Qt</application>, allowing the same level of
 * integration when embedding a <application>Qt</application> widget
 * in GTK or vice versa.
 *
 * <note>
 * The #GtkPlug and #GtkSocket widgets are only available when GTK+
 * is compiled for the X11 platform and %GDK_WINDOWING_X11 is defined.
 * They can only be used on a #GdkX11Display. To use #GtkPlug and
 * #GtkSocket, you need to include the <filename>gtk/gtkx.h</filename>
 * header.
 * </note>
 */

/* Forward declararations */

static void     gtk_socket_finalize             (GObject          *object);
static void     gtk_socket_notify               (GObject          *object,
						 GParamSpec       *pspec);
static void     gtk_socket_realize              (GtkWidget        *widget);
static void     gtk_socket_unrealize            (GtkWidget        *widget);
static void     gtk_socket_get_preferred_width  (GtkWidget        *widget,
                                                 gint             *minimum,
                                                 gint             *natural);
static void     gtk_socket_get_preferred_height (GtkWidget        *widget,
                                                 gint             *minimum,
                                                 gint             *natural);
static void     gtk_socket_size_allocate        (GtkWidget        *widget,
						 GtkAllocation    *allocation);
static void     gtk_socket_hierarchy_changed    (GtkWidget        *widget,
						 GtkWidget        *old_toplevel);
static void     gtk_socket_grab_notify          (GtkWidget        *widget,
						 gboolean          was_grabbed);
static gboolean gtk_socket_key_event            (GtkWidget        *widget,
						 GdkEventKey      *event);
static gboolean gtk_socket_focus                (GtkWidget        *widget,
						 GtkDirectionType  direction);
static void     gtk_socket_remove               (GtkContainer     *container,
						 GtkWidget        *widget);
static void     gtk_socket_forall               (GtkContainer     *container,
						 gboolean          include_internals,
						 GtkCallback       callback,
						 gpointer          callback_data);
static void     gtk_socket_add_window           (GtkSocket        *socket,
                                                 Window            xid,
                                                 gboolean          need_reparent);
static GdkFilterReturn gtk_socket_filter_func   (GdkXEvent        *gdk_xevent,
                                                 GdkEvent         *event,
                                                 gpointer          data);

static gboolean xembed_get_info                 (GdkWindow        *gdk_window,
                                                 unsigned long    *version,
                                                 unsigned long    *flags);

/* From Tk */
#define EMBEDDED_APP_WANTS_FOCUS NotifyNormal+20


/* Local data */

typedef struct
{
  guint			 accel_key;
  GdkModifierType	 accel_mods;
} GrabbedKey;

enum {
  PLUG_ADDED,
  PLUG_REMOVED,
  LAST_SIGNAL
}; 

static guint socket_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkSocket, gtk_socket, GTK_TYPE_CONTAINER)

static void
gtk_socket_finalize (GObject *object)
{
  GtkSocket *socket = GTK_SOCKET (object);
  GtkSocketPrivate *priv = socket->priv;

  g_object_unref (priv->accel_group);

  G_OBJECT_CLASS (gtk_socket_parent_class)->finalize (object);
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

  gobject_class->finalize = gtk_socket_finalize;
  gobject_class->notify = gtk_socket_notify;

  widget_class->realize = gtk_socket_realize;
  widget_class->unrealize = gtk_socket_unrealize;
  widget_class->get_preferred_width = gtk_socket_get_preferred_width;
  widget_class->get_preferred_height = gtk_socket_get_preferred_height;
  widget_class->size_allocate = gtk_socket_size_allocate;
  widget_class->hierarchy_changed = gtk_socket_hierarchy_changed;
  widget_class->grab_notify = gtk_socket_grab_notify;
  widget_class->key_press_event = gtk_socket_key_event;
  widget_class->key_release_event = gtk_socket_key_event;
  widget_class->focus = gtk_socket_focus;

  /* We don't want to show_all the in-process plug, if any.
   */
  widget_class->show_all = gtk_widget_show;

  container_class->remove = gtk_socket_remove;
  container_class->forall = gtk_socket_forall;

  /**
   * GtkSocket::plug-added:
   * @socket_: the object which received the signal
   *
   * This signal is emitted when a client is successfully
   * added to the socket. 
   */
  socket_signals[PLUG_ADDED] =
    g_signal_new (I_("plug-added"),
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkSocketClass, plug_added),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkSocket::plug-removed:
   * @socket_: the object which received the signal
   *
   * This signal is emitted when a client is removed from the socket. 
   * The default action is to destroy the #GtkSocket widget, so if you 
   * want to reuse it you must add a signal handler that returns %TRUE. 
   *
   * Return value: %TRUE to stop other handlers from being invoked.
   */
  socket_signals[PLUG_REMOVED] =
    g_signal_new (I_("plug-removed"),
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
  GtkSocketPrivate *priv;

  priv = gtk_socket_get_instance_private (socket);
  socket->priv = priv;

  priv->request_width = 0;
  priv->request_height = 0;
  priv->current_width = 0;
  priv->current_height = 0;

  priv->plug_window = NULL;
  priv->plug_widget = NULL;
  priv->focus_in = FALSE;
  priv->have_size = FALSE;
  priv->need_map = FALSE;
  priv->active = FALSE;

  priv->accel_group = gtk_accel_group_new ();
  g_object_set_data (G_OBJECT (priv->accel_group), I_("gtk-socket"), socket);
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
 * gtk_socket_add_id:
 * @socket_: a #GtkSocket
 * @window: the Window of a client participating in the XEMBED protocol.
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
gtk_socket_add_id (GtkSocket      *socket,
		   Window          window)
{
  g_return_if_fail (GTK_IS_SOCKET (socket));
  g_return_if_fail (_gtk_widget_get_anchored (GTK_WIDGET (socket)));

  if (!gtk_widget_get_realized (GTK_WIDGET (socket)))
    gtk_widget_realize (GTK_WIDGET (socket));

  gtk_socket_add_window (socket, window, TRUE);
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
Window
gtk_socket_get_id (GtkSocket *socket)
{
  g_return_val_if_fail (GTK_IS_SOCKET (socket), 0);
  g_return_val_if_fail (_gtk_widget_get_anchored (GTK_WIDGET (socket)), 0);

  if (!gtk_widget_get_realized (GTK_WIDGET (socket)))
    gtk_widget_realize (GTK_WIDGET (socket));

  return GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (socket)));
}

/**
 * gtk_socket_get_plug_window:
 * @socket_: a #GtkSocket.
 *
 * Retrieves the window of the plug. Use this to check if the plug has
 * been created inside of the socket.
 *
 * Return value: (transfer none): the window of the plug if available, or %NULL
 *
 * Since:  2.14
 **/
GdkWindow*
gtk_socket_get_plug_window (GtkSocket *socket)
{
  g_return_val_if_fail (GTK_IS_SOCKET (socket), NULL);

  return socket->priv->plug_window;
}

static void
gtk_socket_realize (GtkWidget *widget)
{
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  XWindowAttributes xattrs;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = GDK_FOCUS_CHANGE_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  window = gdk_window_new (gtk_widget_get_parent_window (widget),
                           &attributes, attributes_mask);
  gtk_widget_set_window (widget, window);
  gtk_widget_register_window (widget, window);

  gtk_style_context_set_background (gtk_widget_get_style_context (widget),
                                    window);

  XGetWindowAttributes (GDK_WINDOW_XDISPLAY (window),
			GDK_WINDOW_XID (window),
			&xattrs);

  /* Sooooo, it turns out that mozilla, as per the gtk2xt code selects
     for input on the socket with a mask of 0x0fffff (for god knows why)
     which includes ButtonPressMask causing a BadAccess if someone else
     also selects for this. As per the client-side windows merge we always
     normally selects for button press so we can emulate it on client
     side children that selects for button press. However, we don't need
     this for GtkSocket, so we unselect it here, fixing the crashes in
     firefox. */
  XSelectInput (GDK_WINDOW_XDISPLAY (window),
		GDK_WINDOW_XID (window), 
		(xattrs.your_event_mask & ~ButtonPressMask) |
		SubstructureNotifyMask | SubstructureRedirectMask);

  gdk_window_add_filter (window,
			 gtk_socket_filter_func,
			 widget);

  /* We sync here so that we make sure that if the XID for
   * our window is passed to another application, SubstructureRedirectMask
   * will be set by the time the other app creates its window.
   */
  gdk_display_sync (gtk_widget_get_display (widget));
}

/**
 * gtk_socket_end_embedding:
 * @socket: a #GtkSocket
 *
 * Called to end the embedding of a plug in the socket.
 */
static void
gtk_socket_end_embedding (GtkSocket *socket)
{
  GtkSocketPrivate *private = socket->priv;

  g_object_unref (private->plug_window);
  private->plug_window = NULL;
  private->current_width = 0;
  private->current_height = 0;
  private->resize_count = 0;

  gtk_accel_group_disconnect (private->accel_group, NULL);
}

static void
gtk_socket_unrealize (GtkWidget *widget)
{
  GtkSocket *socket = GTK_SOCKET (widget);
  GtkSocketPrivate *private = socket->priv;

  gtk_widget_set_realized (widget, FALSE);

  if (private->plug_widget)
    {
      _gtk_plug_remove_from_socket (GTK_PLUG (private->plug_widget), socket);
    }
  else if (private->plug_window)
    {
      gtk_socket_end_embedding (socket);
    }

  GTK_WIDGET_CLASS (gtk_socket_parent_class)->unrealize (widget);
}

static void
gtk_socket_size_request (GtkSocket *socket)
{
  GtkSocketPrivate *private = socket->priv;
  XSizeHints hints;
  long supplied;
	  
  gdk_error_trap_push ();

  private->request_width = 1;
  private->request_height = 1;
	  
  if (XGetWMNormalHints (GDK_WINDOW_XDISPLAY (private->plug_window),
			 GDK_WINDOW_XID (private->plug_window),
			 &hints, &supplied))
    {
      if (hints.flags & PMinSize)
	{
	  private->request_width = MAX (hints.min_width, 1);
	  private->request_height = MAX (hints.min_height, 1);
	}
      else if (hints.flags & PBaseSize)
	{
	  private->request_width = MAX (hints.base_width, 1);
	  private->request_height = MAX (hints.base_height, 1);
	}
    }
  private->have_size = TRUE;
  
  gdk_error_trap_pop_ignored ();
}

static void
gtk_socket_get_preferred_width (GtkWidget *widget,
                                gint      *minimum,
                                gint      *natural)
{
  GtkSocket *socket = GTK_SOCKET (widget);
  GtkSocketPrivate *private = socket->priv;

  if (private->plug_widget)
    {
      gtk_widget_get_preferred_width (private->plug_widget, minimum, natural);
    }
  else
    {
      if (private->is_mapped && !private->have_size && private->plug_window)
        gtk_socket_size_request (socket);

      if (private->is_mapped && private->have_size)
        *minimum = *natural = MAX (private->request_width, 1);
      else
        *minimum = *natural = 1;
    }
}

static void
gtk_socket_get_preferred_height (GtkWidget *widget,
                                 gint      *minimum,
                                 gint      *natural)
{
  GtkSocket *socket = GTK_SOCKET (widget);
  GtkSocketPrivate *private = socket->priv;

  if (private->plug_widget)
    {
      gtk_widget_get_preferred_height (private->plug_widget, minimum, natural);
    }
  else
    {
      if (private->is_mapped && !private->have_size && private->plug_window)
        gtk_socket_size_request (socket);

      if (private->is_mapped && private->have_size)
        *minimum = *natural = MAX (private->request_height, 1);
      else
        *minimum = *natural = 1;
    }
}

static void
gtk_socket_send_configure_event (GtkSocket *socket)
{
  GtkAllocation allocation;
  XConfigureEvent xconfigure;
  gint x, y;

  g_return_if_fail (socket->priv->plug_window != NULL);

  memset (&xconfigure, 0, sizeof (xconfigure));
  xconfigure.type = ConfigureNotify;

  xconfigure.event = GDK_WINDOW_XID (socket->priv->plug_window);
  xconfigure.window = GDK_WINDOW_XID (socket->priv->plug_window);

  /* The ICCCM says that synthetic events should have root relative
   * coordinates. We still aren't really ICCCM compliant, since
   * we don't send events when the real toplevel is moved.
   */
  gdk_error_trap_push ();
  gdk_window_get_origin (socket->priv->plug_window, &x, &y);
  gdk_error_trap_pop_ignored ();

  gtk_widget_get_allocation (GTK_WIDGET(socket), &allocation);
  xconfigure.x = x;
  xconfigure.y = y;
  xconfigure.width = allocation.width;
  xconfigure.height = allocation.height;

  xconfigure.border_width = 0;
  xconfigure.above = None;
  xconfigure.override_redirect = False;

  gdk_error_trap_push ();
  XSendEvent (GDK_WINDOW_XDISPLAY (socket->priv->plug_window),
	      GDK_WINDOW_XID (socket->priv->plug_window),
	      False, NoEventMask, (XEvent *)&xconfigure);
  gdk_error_trap_pop_ignored ();
}

static void
gtk_socket_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkSocket *socket = GTK_SOCKET (widget);
  GtkSocketPrivate *private = socket->priv;

  gtk_widget_set_allocation (widget, allocation);
  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (gtk_widget_get_window (widget),
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      if (private->plug_widget)
	{
	  GtkAllocation child_allocation;

	  child_allocation.x = 0;
	  child_allocation.y = 0;
	  child_allocation.width = allocation->width;
	  child_allocation.height = allocation->height;

	  gtk_widget_size_allocate (private->plug_widget, &child_allocation);
	}
      else if (private->plug_window)
	{
	  gdk_error_trap_push ();

	  if (allocation->width != private->current_width ||
	      allocation->height != private->current_height)
	    {
	      gdk_window_move_resize (private->plug_window,
				      0, 0,
				      allocation->width, allocation->height);
	      if (private->resize_count)
		private->resize_count--;
	      
	      GTK_NOTE (PLUGSOCKET,
			g_message ("GtkSocket - allocated: %d %d",
				   allocation->width, allocation->height));
	      private->current_width = allocation->width;
	      private->current_height = allocation->height;
	    }

	  if (private->need_map)
	    {
	      gdk_window_show (private->plug_window);
	      private->need_map = FALSE;
	    }

	  while (private->resize_count)
 	    {
 	      gtk_socket_send_configure_event (socket);
 	      private->resize_count--;
 	      GTK_NOTE (PLUGSOCKET,
			g_message ("GtkSocket - sending synthetic configure: %d %d",
				   allocation->width, allocation->height));
 	    }

	  gdk_error_trap_pop_ignored ();
	}
    }
}

static void
gtk_socket_send_key_event (GtkSocket *socket,
			   GdkEvent  *gdk_event,
			   gboolean   mask_key_presses)
{
  XKeyEvent xkey;
  GdkScreen *screen = gdk_window_get_screen (socket->priv->plug_window);

  memset (&xkey, 0, sizeof (xkey));
  xkey.type = (gdk_event->type == GDK_KEY_PRESS) ? KeyPress : KeyRelease;
  xkey.window = GDK_WINDOW_XID (socket->priv->plug_window);
  xkey.root = GDK_WINDOW_XID (gdk_screen_get_root_window (screen));
  xkey.subwindow = None;
  xkey.time = gdk_event->key.time;
  xkey.x = 0;
  xkey.y = 0;
  xkey.x_root = 0;
  xkey.y_root = 0;
  xkey.state = gdk_event->key.state;
  xkey.keycode = gdk_event->key.hardware_keycode;
  xkey.same_screen = True;/* FIXME ? */

  gdk_error_trap_push ();
  XSendEvent (GDK_WINDOW_XDISPLAY (socket->priv->plug_window),
	      GDK_WINDOW_XID (socket->priv->plug_window),
	      False,
	      (mask_key_presses ? KeyPressMask : NoEventMask),
	      (XEvent *)&xkey);
  gdk_error_trap_pop_ignored ();
}

static gboolean
activate_key (GtkAccelGroup  *accel_group,
	      GObject        *acceleratable,
	      guint           accel_key,
	      GdkModifierType accel_mods,
	      GrabbedKey     *grabbed_key)
{
  GdkEvent *gdk_event = gtk_get_current_event ();
  
  GtkSocket *socket = g_object_get_data (G_OBJECT (accel_group), "gtk-socket");
  gboolean retval = FALSE;

  if (gdk_event && gdk_event->type == GDK_KEY_PRESS && socket->priv->plug_window)
    {
      gtk_socket_send_key_event (socket, gdk_event, FALSE);
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

/**
 * gtk_socket_add_grabbed_key:
 * @socket: a #GtkSocket
 * @keyval: a key
 * @modifiers: modifiers for the key
 *
 * Called from the GtkSocket platform-specific backend when the
 * corresponding plug has told the socket to grab a key.
 */
static void
gtk_socket_add_grabbed_key (GtkSocket       *socket,
			    guint            keyval,
			    GdkModifierType  modifiers)
{
  GClosure *closure;
  GrabbedKey *grabbed_key;

  grabbed_key = g_new (GrabbedKey, 1);
  
  grabbed_key->accel_key = keyval;
  grabbed_key->accel_mods = modifiers;

  if (gtk_accel_group_find (socket->priv->accel_group,
			    find_accel_key,
			    &grabbed_key))
    {
      g_warning ("GtkSocket: request to add already present grabbed key %u,%#x\n",
		 keyval, modifiers);
      g_free (grabbed_key);
      return;
    }

  closure = g_cclosure_new (G_CALLBACK (activate_key), grabbed_key, (GClosureNotify)g_free);

  gtk_accel_group_connect (socket->priv->accel_group, keyval, modifiers, GTK_ACCEL_LOCKED,
			   closure);
}

/**
 * gtk_socket_remove_grabbed_key:
 * @socket: a #GtkSocket
 * @keyval: a key
 * @modifiers: modifiers for the key
 *
 * Called from the GtkSocket backend when the corresponding plug has
 * told the socket to remove a key grab.
 */
static void
gtk_socket_remove_grabbed_key (GtkSocket      *socket,
			       guint           keyval,
			       GdkModifierType modifiers)
{
  if (!gtk_accel_group_disconnect_key (socket->priv->accel_group, keyval, modifiers))
    g_warning ("GtkSocket: request to remove non-present grabbed key %u,%#x\n",
	       keyval, modifiers);
}

static void
socket_update_focus_in (GtkSocket *socket)
{
  GtkSocketPrivate *private = socket->priv;
  gboolean focus_in = FALSE;

  if (private->plug_window)
    {
      GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (socket));

      if (gtk_widget_is_toplevel (toplevel) &&
	  gtk_window_has_toplevel_focus (GTK_WINDOW (toplevel)) &&
	  gtk_widget_is_focus (GTK_WIDGET (socket)))
	focus_in = TRUE;
    }

  if (focus_in != private->focus_in)
    {
      private->focus_in = focus_in;

      if (focus_in)
        _gtk_xembed_send_focus_message (private->plug_window,
                                        XEMBED_FOCUS_IN, XEMBED_FOCUS_CURRENT);
      else
        _gtk_xembed_send_message (private->plug_window,
                                  XEMBED_FOCUS_OUT, 0, 0, 0);
    }
}

static void
socket_update_active (GtkSocket *socket)
{
  GtkSocketPrivate *private = socket->priv;
  gboolean active = FALSE;

  if (private->plug_window)
    {
      GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (socket));

      if (gtk_widget_is_toplevel (toplevel) &&
	  gtk_window_is_active  (GTK_WINDOW (toplevel)))
	active = TRUE;
    }

  if (active != private->active)
    {
      private->active = active;

      _gtk_xembed_send_message (private->plug_window,
                                active ? XEMBED_WINDOW_ACTIVATE : XEMBED_WINDOW_DEACTIVATE,
                                0, 0, 0);
    }
}

static void
gtk_socket_hierarchy_changed (GtkWidget *widget,
			      GtkWidget *old_toplevel)
{
  GtkSocket *socket = GTK_SOCKET (widget);
  GtkSocketPrivate *private = socket->priv;
  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);

  if (toplevel && !GTK_IS_WINDOW (toplevel))
    toplevel = NULL;

  if (toplevel != private->toplevel)
    {
      if (private->toplevel)
	{
	  gtk_window_remove_accel_group (GTK_WINDOW (private->toplevel), private->accel_group);
	  g_signal_handlers_disconnect_by_func (private->toplevel,
						socket_update_focus_in,
						socket);
	  g_signal_handlers_disconnect_by_func (private->toplevel,
						socket_update_active,
						socket);
	}

      private->toplevel = toplevel;

      if (toplevel)
	{
	  gtk_window_add_accel_group (GTK_WINDOW (private->toplevel), private->accel_group);
	  g_signal_connect_swapped (private->toplevel, "notify::has-toplevel-focus",
				    G_CALLBACK (socket_update_focus_in), socket);
	  g_signal_connect_swapped (private->toplevel, "notify::is-active",
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

  if (!socket->priv->same_app)
    _gtk_xembed_send_message (socket->priv->plug_window,
                              was_grabbed ? XEMBED_MODALITY_OFF : XEMBED_MODALITY_ON,
                              0, 0, 0);
}

static gboolean
gtk_socket_key_event (GtkWidget   *widget,
                      GdkEventKey *event)
{
  GtkSocket *socket = GTK_SOCKET (widget);
  GtkSocketPrivate *private = socket->priv;
  
  if (gtk_widget_has_focus (widget) && private->plug_window && !private->plug_widget)
    {
      gtk_socket_send_key_event (socket, (GdkEvent *) event, FALSE);

      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_socket_notify (GObject    *object,
		   GParamSpec *pspec)
{
  if (strcmp (pspec->name, "is-focus") == 0)
    socket_update_focus_in (GTK_SOCKET (object));

  if (G_OBJECT_CLASS (gtk_socket_parent_class)->notify)
    G_OBJECT_CLASS (gtk_socket_parent_class)->notify (object, pspec);
}

/**
 * gtk_socket_claim_focus:
 * @socket: a #GtkSocket
 * @send_event: huh?
 *
 * Claims focus for the socket. XXX send_event?
 */
static void
gtk_socket_claim_focus (GtkSocket *socket,
			gboolean   send_event)
{
  GtkWidget *widget = GTK_WIDGET (socket);
  GtkSocketPrivate *private = socket->priv;

  if (!send_event)
    private->focus_in = TRUE;	/* Otherwise, our notify handler will send FOCUS_IN  */
      
  /* Oh, the trickery... */
  
  gtk_widget_set_can_focus (widget, TRUE);
  gtk_widget_grab_focus (widget);
  gtk_widget_set_can_focus (widget, FALSE);
}

static gboolean
gtk_socket_focus (GtkWidget       *widget,
		  GtkDirectionType direction)
{
  GtkSocket *socket = GTK_SOCKET (widget);
  GtkSocketPrivate *private = socket->priv;

  if (private->plug_widget)
    return gtk_widget_child_focus (private->plug_widget, direction);

  if (!gtk_widget_is_focus (widget))
    {
      gint detail = -1;

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
      
      _gtk_xembed_send_focus_message (private->plug_window, XEMBED_FOCUS_IN, detail);
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
  GtkSocketPrivate *private = socket->priv;

  g_return_if_fail (child == private->plug_widget);

  _gtk_plug_remove_from_socket (GTK_PLUG (private->plug_widget), socket);
}

static void
gtk_socket_forall (GtkContainer *container,
		   gboolean      include_internals,
		   GtkCallback   callback,
		   gpointer      callback_data)
{
  GtkSocket *socket = GTK_SOCKET (container);
  GtkSocketPrivate *private = socket->priv;

  if (private->plug_widget)
    (* callback) (private->plug_widget, callback_data);
}

/**
 * gtk_socket_add_window:
 * @socket: a #GtkSocket
 * @xid: the native identifier for a window
 * @need_reparent: whether the socket's plug's window needs to be
 *                 reparented to the socket
 *
 * Adds a window to a GtkSocket.
 */
static void
gtk_socket_add_window (GtkSocket       *socket,
		       Window           xid,
		       gboolean         need_reparent)
{
  GtkWidget *widget = GTK_WIDGET (socket);
  GdkDisplay *display = gtk_widget_get_display (widget);
  gpointer user_data = NULL;
  GtkSocketPrivate *private = socket->priv;
  unsigned long version;
  unsigned long flags;

  if (GDK_IS_X11_DISPLAY (display))
    private->plug_window = gdk_x11_window_lookup_for_display (display, xid);
  else
    private->plug_window = NULL;

  if (private->plug_window)
    {
      g_object_ref (private->plug_window);
      gdk_window_get_user_data (private->plug_window, &user_data);
    }

  if (user_data) /* A widget's window in this process */
    {
      GtkWidget *child_widget = user_data;

      if (!GTK_IS_PLUG (child_widget))
        {
          g_warning (G_STRLOC ": Can't add non-GtkPlug to GtkSocket");
          private->plug_window = NULL;
          gdk_error_trap_pop_ignored ();

          return;
        }

      _gtk_plug_add_to_socket (GTK_PLUG (child_widget), socket);
    }
  else  /* A foreign window */
    {
      GdkDragProtocol protocol;

      gdk_error_trap_push ();

      if (!private->plug_window)
        {
          if (GDK_IS_X11_DISPLAY (display))
            private->plug_window = gdk_x11_window_foreign_new_for_display (display, xid);
          if (!private->plug_window) /* was deleted before we could get it */
            {
              gdk_error_trap_pop_ignored ();
              return;
            }
        }

      XSelectInput (GDK_DISPLAY_XDISPLAY (gtk_widget_get_display (GTK_WIDGET (socket))),
                    GDK_WINDOW_XID (private->plug_window),
                    StructureNotifyMask | PropertyChangeMask);

      if (gdk_error_trap_pop ())
	{
	  g_object_unref (private->plug_window);
	  private->plug_window = NULL;
	  return;
	}
      
      /* OK, we now will reliably get destroy notification on socket->plug_window */

      gdk_error_trap_push ();

      if (need_reparent)
	{
	  gdk_window_hide (private->plug_window); /* Shouldn't actually be necessary for XEMBED, but just in case */
	  gdk_window_reparent (private->plug_window,
                               gtk_widget_get_window (widget),
                               0, 0);
	}

      private->have_size = FALSE;

      private->xembed_version = -1;
      if (xembed_get_info (private->plug_window, &version, &flags))
        {
          private->xembed_version = MIN (GTK_XEMBED_PROTOCOL_VERSION, version);
          private->is_mapped = (flags & XEMBED_MAPPED) != 0;
        }
      else
        {
          /* FIXME, we should probably actually check the state before we started */
          private->is_mapped = TRUE;
        }

      private->need_map = private->is_mapped;

      protocol = gdk_window_get_drag_protocol (private->plug_window, NULL);
      if (protocol)
	gtk_drag_dest_set_proxy (GTK_WIDGET (socket), private->plug_window,
				 protocol, TRUE);

      gdk_error_trap_pop_ignored ();

      gdk_window_add_filter (private->plug_window,
			     gtk_socket_filter_func,
			     socket);

#ifdef HAVE_XFIXES
      gdk_error_trap_push ();
      XFixesChangeSaveSet (GDK_DISPLAY_XDISPLAY (gtk_widget_get_display (GTK_WIDGET (socket))),
                           GDK_WINDOW_XID (private->plug_window),
                           SetModeInsert, SaveSetRoot, SaveSetUnmap);
      gdk_error_trap_pop_ignored ();
#endif
      _gtk_xembed_send_message (private->plug_window,
                                XEMBED_EMBEDDED_NOTIFY, 0,
                                GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (socket))),
                                private->xembed_version);

      socket_update_active (socket);
      socket_update_focus_in (socket);

      gtk_widget_queue_resize (GTK_WIDGET (socket));
    }

  if (private->plug_window)
    g_signal_emit (socket, socket_signals[PLUG_ADDED], 0);
}

/**
 * gtk_socket_handle_map_request:
 * @socket: a #GtkSocket
 *
 * Called from the GtkSocket backend when the plug has been mapped.
 */
static void
gtk_socket_handle_map_request (GtkSocket *socket)
{
  GtkSocketPrivate *private = socket->priv;
  if (!private->is_mapped)
    {
      private->is_mapped = TRUE;
      private->need_map = TRUE;

      gtk_widget_queue_resize (GTK_WIDGET (socket));
    }
}

/**
 * gtk_socket_unmap_notify:
 * @socket: a #GtkSocket
 *
 * Called from the GtkSocket backend when the plug has been unmapped ???
 */
static void
gtk_socket_unmap_notify (GtkSocket *socket)
{
  GtkSocketPrivate *private = socket->priv;
  if (private->is_mapped)
    {
      private->is_mapped = FALSE;
      gtk_widget_queue_resize (GTK_WIDGET (socket));
    }
}

/**
 * gtk_socket_advance_toplevel_focus:
 * @socket: a #GtkSocket
 * @direction: a direction
 *
 * Called from the GtkSocket backend when the corresponding plug
 * has told the socket to move the focus.
 */
static void
gtk_socket_advance_toplevel_focus (GtkSocket        *socket,
				   GtkDirectionType  direction)
{
  GtkBin *bin;
  GtkWindow *window;
  GtkContainer *container;
  GtkWidget *child;
  GtkWidget *focus_widget;
  GtkWidget *toplevel;
  GtkWidget *old_focus_child;
  GtkWidget *parent;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (socket));
  if (!toplevel)
    return;

  if (!gtk_widget_is_toplevel (toplevel) || GTK_IS_PLUG (toplevel))
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
  old_focus_child = gtk_container_get_focus_child (container);
  
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

  focus_widget = gtk_window_get_focus (window);
  if (window)
    {
      /* Wrapped off the end, clear the focus setting for the toplevel */
      parent = gtk_widget_get_parent (focus_widget);
      while (parent)
	{
	  gtk_container_set_focus_child (GTK_CONTAINER (parent), NULL);
          parent = gtk_widget_get_parent (parent);
	}
      
      gtk_window_set_focus (GTK_WINDOW (container), NULL);
    }

  /* Now try to focus the first widget in the window */
  child = gtk_bin_get_child (bin);
  if (child)
    {
      if (gtk_widget_child_focus (child, direction))
        return;
    }
}

static gboolean
xembed_get_info (GdkWindow     *window,
		 unsigned long *version,
		 unsigned long *flags)
{
  GdkDisplay *display = gdk_window_get_display (window);
  Atom xembed_info_atom = gdk_x11_get_xatom_by_name_for_display (display, "_XEMBED_INFO");
  Atom type;
  int format;
  unsigned long nitems, bytes_after;
  unsigned char *data;
  unsigned long *data_long;
  int status;
  
  gdk_error_trap_push ();
  status = XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
			       GDK_WINDOW_XID (window),
			       xembed_info_atom,
			       0, 2, False,
			       xembed_info_atom, &type, &format,
			       &nitems, &bytes_after, &data);
  gdk_error_trap_pop_ignored ();

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
handle_xembed_message (GtkSocket        *socket,
		       XEmbedMessageType message,
		       glong             detail,
		       glong             data1,
		       glong             data2,
		       guint32           time)
{
  GTK_NOTE (PLUGSOCKET,
	    g_message ("GtkSocket: %s received", _gtk_xembed_message_name (message)));
  
  switch (message)
    {
    case XEMBED_EMBEDDED_NOTIFY:
    case XEMBED_WINDOW_ACTIVATE:
    case XEMBED_WINDOW_DEACTIVATE:
    case XEMBED_MODALITY_ON:
    case XEMBED_MODALITY_OFF:
    case XEMBED_FOCUS_IN:
    case XEMBED_FOCUS_OUT:
      g_warning ("GtkSocket: Invalid _XEMBED message %s received", _gtk_xembed_message_name (message));
      break;
      
    case XEMBED_REQUEST_FOCUS:
      gtk_socket_claim_focus (socket, TRUE);
      break;

    case XEMBED_FOCUS_NEXT:
    case XEMBED_FOCUS_PREV:
      gtk_socket_advance_toplevel_focus (socket,
					 (message == XEMBED_FOCUS_NEXT ?
					  GTK_DIR_TAB_FORWARD : GTK_DIR_TAB_BACKWARD));
      break;
      
    case XEMBED_GTK_GRAB_KEY:
      gtk_socket_add_grabbed_key (socket, data1, data2);
      break; 
    case XEMBED_GTK_UNGRAB_KEY:
      gtk_socket_remove_grabbed_key (socket, data1, data2);
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

static GdkFilterReturn
gtk_socket_filter_func (GdkXEvent *gdk_xevent,
			GdkEvent  *event,
			gpointer   data)
{
  GtkSocket *socket;
  GtkWidget *widget;
  GdkDisplay *display;
  XEvent *xevent;
  GtkSocketPrivate *private;

  GdkFilterReturn return_val;

  socket = GTK_SOCKET (data);
  private = socket->priv;

  return_val = GDK_FILTER_CONTINUE;

  if (private->plug_widget)
    return return_val;

  widget = GTK_WIDGET (socket);
  xevent = (XEvent *)gdk_xevent;
  display = gtk_widget_get_display (widget);

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

	if (!private->plug_window)
	  {
	    gtk_socket_add_window (socket, xcwe->window, FALSE);

	    if (private->plug_window)
	      {
		GTK_NOTE (PLUGSOCKET, g_message ("GtkSocket - window created"));
	      }
	  }
	
	return_val = GDK_FILTER_REMOVE;
	
	break;
      }

    case ConfigureRequest:
      {
	XConfigureRequestEvent *xcre = &xevent->xconfigurerequest;
	
	if (!private->plug_window)
	  gtk_socket_add_window (socket, xcre->window, FALSE);
	
	if (private->plug_window)
	  {
	    if (xcre->value_mask & (CWWidth | CWHeight))
	      {
		GTK_NOTE (PLUGSOCKET,
			  g_message ("GtkSocket - configure request: %d %d",
				     private->request_width,
				     private->request_height));

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
	if (private->plug_window && (xdwe->window == GDK_WINDOW_XID (private->plug_window)))
	  {
	    gboolean result;
	    
	    GTK_NOTE (PLUGSOCKET, g_message ("GtkSocket - destroy notify"));
	    
	    gdk_window_destroy_notify (private->plug_window);
	    gtk_socket_end_embedding (socket);

	    g_object_ref (widget);
	    g_signal_emit_by_name (widget, "plug-removed", &result);
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
      if (!private->plug_window)
	{
	  gtk_socket_add_window (socket, xevent->xmaprequest.window, FALSE);
	}
	
      if (private->plug_window)
	{
	  GTK_NOTE (PLUGSOCKET, g_message ("GtkSocket - Map Request"));

	  gtk_socket_handle_map_request (socket);
	  return_val = GDK_FILTER_REMOVE;
	}
      break;
    case PropertyNotify:
      if (private->plug_window &&
	  xevent->xproperty.window == GDK_WINDOW_XID (private->plug_window))
	{
	  GdkDragProtocol protocol;

	  if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "WM_NORMAL_HINTS"))
	    {
	      GTK_NOTE (PLUGSOCKET, g_message ("GtkSocket - received PropertyNotify for plug's WM_NORMAL_HINTS"));
	      private->have_size = FALSE;
	      gtk_widget_queue_resize (widget);
	      return_val = GDK_FILTER_REMOVE;
	    }
	  else if ((xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "XdndAware")) ||
	      (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "_MOTIF_DRAG_RECEIVER_INFO")))
	    {
	      gdk_error_trap_push ();
              protocol = gdk_window_get_drag_protocol (private->plug_window, NULL);
              if (protocol)
		gtk_drag_dest_set_proxy (GTK_WIDGET (socket),
					 private->plug_window,
					 protocol, TRUE);

	      gdk_error_trap_pop_ignored ();
	      return_val = GDK_FILTER_REMOVE;
	    }
	  else if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "_XEMBED_INFO"))
	    {
	      unsigned long flags;
	      
	      if (xembed_get_info (private->plug_window, NULL, &flags))
		{
		  gboolean was_mapped = private->is_mapped;
		  gboolean is_mapped = (flags & XEMBED_MAPPED) != 0;

		  if (was_mapped != is_mapped)
		    {
		      if (is_mapped)
			gtk_socket_handle_map_request (socket);
		      else
			{
			  gdk_error_trap_push ();
			  gdk_window_show (private->plug_window);
			  gdk_error_trap_pop_ignored ();
			  
			  gtk_socket_unmap_notify (socket);
			}
		    }
		}
	      return_val = GDK_FILTER_REMOVE;
	    }
	}
      break;
    case ReparentNotify:
      {
        GdkWindow *window;
	XReparentEvent *xre = &xevent->xreparent;

        window = gtk_widget_get_window (widget);

	GTK_NOTE (PLUGSOCKET, g_message ("GtkSocket - ReparentNotify received"));
	if (!private->plug_window &&
            xre->parent == GDK_WINDOW_XID (window))
	  {
	    gtk_socket_add_window (socket, xre->window, FALSE);
	    
	    if (private->plug_window)
	      {
		GTK_NOTE (PLUGSOCKET, g_message ("GtkSocket - window reparented"));
	      }
	    
	    return_val = GDK_FILTER_REMOVE;
	  }
        else
          {
            if (private->plug_window &&
                xre->window == GDK_WINDOW_XID (private->plug_window) &&
                xre->parent != GDK_WINDOW_XID (window))
              {
                gboolean result;

                gtk_socket_end_embedding (socket);

                g_object_ref (widget);
                g_signal_emit_by_name (widget, "plug-removed", &result);
                if (!result)
                  gtk_widget_destroy (widget);
                g_object_unref (widget);

                return_val = GDK_FILTER_REMOVE;
              }
          }

	break;
      }
    case UnmapNotify:
      if (private->plug_window &&
	  xevent->xunmap.window == GDK_WINDOW_XID (private->plug_window))
	{
	  GTK_NOTE (PLUGSOCKET, g_message ("GtkSocket - Unmap notify"));

	  gtk_socket_unmap_notify (socket);
	  return_val = GDK_FILTER_REMOVE;
	}
      break;
      
    }
  
  return return_val;
}
