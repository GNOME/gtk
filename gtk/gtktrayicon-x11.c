/* gtktrayicon.c
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
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
 * This is an implementation of the freedesktop.org "system tray" spec,
 * http://www.freedesktop.org/wiki/Standards/systemtray-spec
 */

#include <config.h>
#include <string.h>
#include <libintl.h>

#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktrayicon.h"

#include "gtkalias.h"

#include "x11/gdkx.h"
#include <X11/Xatom.h>

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#define SYSTEM_TRAY_ORIENTATION_HORZ 0
#define SYSTEM_TRAY_ORIENTATION_VERT 1

enum {
  PROP_0,
  PROP_ORIENTATION
};

struct _GtkTrayIconPrivate
{
  guint stamp;
  
  Atom selection_atom;
  Atom manager_atom;
  Atom system_tray_opcode_atom;
  Atom orientation_atom;
  Window manager_window;

  GtkOrientation orientation;
};
         
static void gtk_tray_icon_get_property  (GObject     *object,
				 	 guint        prop_id,
					 GValue      *value,
					 GParamSpec  *pspec);

static void     gtk_tray_icon_realize   (GtkWidget   *widget);
static void     gtk_tray_icon_unrealize (GtkWidget   *widget);
static gboolean gtk_tray_icon_delete    (GtkWidget   *widget,
					 GdkEventAny *event);
static gboolean gtk_tray_icon_expose    (GtkWidget      *widget, 
					 GdkEventExpose *event);

static void gtk_tray_icon_update_manager_window    (GtkTrayIcon *icon,
						    gboolean     dock_if_realized);
static void gtk_tray_icon_manager_window_destroyed (GtkTrayIcon *icon);

G_DEFINE_TYPE (GtkTrayIcon, gtk_tray_icon, GTK_TYPE_PLUG)

static void
gtk_tray_icon_class_init (GtkTrayIconClass *class)
{
  GObjectClass *gobject_class = (GObjectClass *)class;
  GtkWidgetClass *widget_class = (GtkWidgetClass *)class;

  gobject_class->get_property = gtk_tray_icon_get_property;

  widget_class->realize   = gtk_tray_icon_realize;
  widget_class->unrealize = gtk_tray_icon_unrealize;
  widget_class->delete_event = gtk_tray_icon_delete;
  widget_class->expose_event = gtk_tray_icon_expose;

  g_object_class_install_property (gobject_class,
				   PROP_ORIENTATION,
				   g_param_spec_enum ("orientation",
						      P_("Orientation"),
						      P_("The orientation of the tray"),
						      GTK_TYPE_ORIENTATION,
						      GTK_ORIENTATION_HORIZONTAL,
						      GTK_PARAM_READABLE));

  g_type_class_add_private (class, sizeof (GtkTrayIconPrivate));
}

static void
gtk_tray_icon_init (GtkTrayIcon *icon)
{
  icon->priv = G_TYPE_INSTANCE_GET_PRIVATE (icon, GTK_TYPE_TRAY_ICON,
					    GtkTrayIconPrivate);
  
  icon->priv->stamp = 1;
  icon->priv->orientation = GTK_ORIENTATION_HORIZONTAL;

  gtk_widget_set_app_paintable (GTK_WIDGET (icon), TRUE);
  gtk_widget_set_double_buffered (GTK_WIDGET (icon), FALSE);
  gtk_widget_add_events (GTK_WIDGET (icon), GDK_PROPERTY_CHANGE_MASK);
}

static void
gtk_tray_icon_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  GtkTrayIcon *icon = GTK_TRAY_ICON (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, icon->priv->orientation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
gtk_tray_icon_expose (GtkWidget      *widget, 
		      GdkEventExpose *event)
{
  gdk_window_clear_area (widget->window, event->area.x, event->area.y,
			 event->area.width, event->area.height);

  if (GTK_WIDGET_CLASS (gtk_tray_icon_parent_class)->expose_event)  
    return GTK_WIDGET_CLASS (gtk_tray_icon_parent_class)->expose_event (widget, event);

  return FALSE;
}

static void
gtk_tray_icon_get_orientation_property (GtkTrayIcon *icon)
{
  Display *xdisplay;
  Atom type;
  int format;
  union {
	gulong *prop;
	guchar *prop_ch;
  } prop = { NULL };
  gulong nitems;
  gulong bytes_after;
  int error, result;

  g_assert (icon->priv->manager_window != None);
  
  xdisplay = GDK_DISPLAY_XDISPLAY (gtk_widget_get_display (GTK_WIDGET (icon)));

  gdk_error_trap_push ();
  type = None;
  result = XGetWindowProperty (xdisplay,
			       icon->priv->manager_window,
			       icon->priv->orientation_atom,
			       0, G_MAXLONG, FALSE,
			       XA_CARDINAL,
			       &type, &format, &nitems,
			       &bytes_after, &(prop.prop_ch));
  error = gdk_error_trap_pop ();

  if (error || result != Success)
    return;

  if (type == XA_CARDINAL)
    {
      GtkOrientation orientation;

      orientation = (prop.prop [0] == SYSTEM_TRAY_ORIENTATION_HORZ) ?
					GTK_ORIENTATION_HORIZONTAL :
					GTK_ORIENTATION_VERTICAL;

      if (icon->priv->orientation != orientation)
	{
	  icon->priv->orientation = orientation;

	  g_object_notify (G_OBJECT (icon), "orientation");
	}
    }

  if (prop.prop)
    XFree (prop.prop);
}

static GdkFilterReturn
gtk_tray_icon_manager_filter (GdkXEvent *xevent, 
			      GdkEvent  *event, 
			      gpointer   user_data)
{
  GtkTrayIcon *icon = user_data;
  XEvent *xev = (XEvent *)xevent;

  if (xev->xany.type == ClientMessage &&
      xev->xclient.message_type == icon->priv->manager_atom &&
      xev->xclient.data.l[1] == icon->priv->selection_atom)
    {
      gtk_tray_icon_update_manager_window (icon, TRUE);
    }
  else if (xev->xany.window == icon->priv->manager_window)
    {
      if (xev->xany.type == PropertyNotify &&
	  xev->xproperty.atom == icon->priv->orientation_atom)
	{
	  gtk_tray_icon_get_orientation_property (icon);
	}
      if (xev->xany.type == DestroyNotify)
	{
	  gtk_tray_icon_manager_window_destroyed (icon);
	}
    }
  
  return GDK_FILTER_CONTINUE;
}

static void
gtk_tray_icon_unrealize (GtkWidget *widget)
{
  GtkTrayIcon *icon = GTK_TRAY_ICON (widget);
  GdkWindow *root_window;

  if (icon->priv->manager_window != None)
    {
      GdkWindow *gdkwin;

      gdkwin = gdk_window_lookup_for_display (gtk_widget_get_display (widget),
                                              icon->priv->manager_window);
      
      gdk_window_remove_filter (gdkwin, gtk_tray_icon_manager_filter, icon);
    }

  root_window = gdk_screen_get_root_window (gtk_widget_get_screen (widget));

  gdk_window_remove_filter (root_window, gtk_tray_icon_manager_filter, icon);

  if (GTK_WIDGET_CLASS (gtk_tray_icon_parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (gtk_tray_icon_parent_class)->unrealize) (widget);
}

static void
gtk_tray_icon_send_manager_message (GtkTrayIcon *icon,
				    long         message,
				    Window       window,
				    long         data1,
				    long         data2,
				    long         data3)
{
  XClientMessageEvent ev;
  Display *display;
  
  memset (&ev, 0, sizeof (ev));
  ev.type = ClientMessage;
  ev.window = window;
  ev.message_type = icon->priv->system_tray_opcode_atom;
  ev.format = 32;
  ev.data.l[0] = gdk_x11_get_server_time (GTK_WIDGET (icon)->window);
  ev.data.l[1] = message;
  ev.data.l[2] = data1;
  ev.data.l[3] = data2;
  ev.data.l[4] = data3;

  display = GDK_DISPLAY_XDISPLAY (gtk_widget_get_display (GTK_WIDGET (icon)));
  
  gdk_error_trap_push ();
  XSendEvent (display,
	      icon->priv->manager_window, False, NoEventMask, (XEvent *)&ev);
  XSync (display, False);
  gdk_error_trap_pop ();
}

static void
gtk_tray_icon_send_dock_request (GtkTrayIcon *icon)
{
  gtk_tray_icon_send_manager_message (icon,
				      SYSTEM_TRAY_REQUEST_DOCK,
				      icon->priv->manager_window,
				      gtk_plug_get_id (GTK_PLUG (icon)),
				      0, 0);
}

static void
gtk_tray_icon_update_manager_window (GtkTrayIcon *icon,
				     gboolean     dock_if_realized)
{
  Display *xdisplay;
  
  if (icon->priv->manager_window != None)
    return;

  xdisplay = GDK_DISPLAY_XDISPLAY (gtk_widget_get_display (GTK_WIDGET (icon)));
  
  XGrabServer (xdisplay);
  
  icon->priv->manager_window = XGetSelectionOwner (xdisplay,
						   icon->priv->selection_atom);

  if (icon->priv->manager_window != None)
    XSelectInput (xdisplay,
		  icon->priv->manager_window, StructureNotifyMask|PropertyChangeMask);

  XUngrabServer (xdisplay);
  XFlush (xdisplay);
  
  if (icon->priv->manager_window != None)
    {
      GdkWindow *gdkwin;

      gdkwin = gdk_window_lookup_for_display (gtk_widget_get_display (GTK_WIDGET (icon)),
					      icon->priv->manager_window);
      
      gdk_window_add_filter (gdkwin, gtk_tray_icon_manager_filter, icon);

      if (dock_if_realized && GTK_WIDGET_REALIZED (icon))
	gtk_tray_icon_send_dock_request (icon);

      gtk_tray_icon_get_orientation_property (icon);
    }
}

static void
gtk_tray_icon_manager_window_destroyed (GtkTrayIcon *icon)
{
  GdkWindow *gdkwin;
  
  g_return_if_fail (icon->priv->manager_window != None);

  gdkwin = gdk_window_lookup_for_display (gtk_widget_get_display (GTK_WIDGET (icon)),
					  icon->priv->manager_window);
      
  gdk_window_remove_filter (gdkwin, gtk_tray_icon_manager_filter, icon);

  icon->priv->manager_window = None;

  gtk_tray_icon_update_manager_window (icon, TRUE);
}

static gboolean 
gtk_tray_icon_delete (GtkWidget   *widget,
		      GdkEventAny *event)
{
  GtkTrayIcon *icon = GTK_TRAY_ICON (widget);
  GdkWindow *gdkwin;

  if (icon->priv->manager_window != None)
    {  
      gdkwin = gdk_window_lookup_for_display (gtk_widget_get_display (GTK_WIDGET (icon)),
					      icon->priv->manager_window);
      
      gdk_window_remove_filter (gdkwin, gtk_tray_icon_manager_filter, icon);
      
      icon->priv->manager_window = None;
    }

  gtk_tray_icon_update_manager_window (icon, TRUE);  

  return TRUE;
}

static void
gtk_tray_icon_realize (GtkWidget *widget)
{
  GtkTrayIcon *icon = GTK_TRAY_ICON (widget);
  GdkScreen *screen;
  GdkDisplay *display;
  Display *xdisplay;
  char buffer[256];
  GdkWindow *root_window;

  if (GTK_WIDGET_CLASS (gtk_tray_icon_parent_class)->realize)
    GTK_WIDGET_CLASS (gtk_tray_icon_parent_class)->realize (widget);

  screen = gtk_widget_get_screen (widget);
  display = gdk_screen_get_display (screen);
  xdisplay = gdk_x11_display_get_xdisplay (display);

  /* Now see if there's a manager window around */
  g_snprintf (buffer, sizeof (buffer),
	      "_NET_SYSTEM_TRAY_S%d",
	      gdk_screen_get_number (screen));

  icon->priv->selection_atom = XInternAtom (xdisplay, buffer, False);
  
  icon->priv->manager_atom = XInternAtom (xdisplay, "MANAGER", False);
  
  icon->priv->system_tray_opcode_atom = XInternAtom (xdisplay,
						     "_NET_SYSTEM_TRAY_OPCODE",
						     False);

  icon->priv->orientation_atom = XInternAtom (xdisplay,
					      "_NET_SYSTEM_TRAY_ORIENTATION",
					      False);

  gtk_tray_icon_update_manager_window (icon, FALSE);
  gtk_tray_icon_send_dock_request (icon);

  root_window = gdk_screen_get_root_window (screen);
  
  /* Add a root window filter so that we get changes on MANAGER */
  gdk_window_add_filter (root_window,
			 gtk_tray_icon_manager_filter, icon);
}

guint
_gtk_tray_icon_send_message (GtkTrayIcon *icon,
			     gint         timeout,
			     const gchar *message,
			     gint         len)
{
  guint stamp;
  
  g_return_val_if_fail (GTK_IS_TRAY_ICON (icon), 0);
  g_return_val_if_fail (timeout >= 0, 0);
  g_return_val_if_fail (message != NULL, 0);
		     
  if (icon->priv->manager_window == None)
    return 0;

  if (len < 0)
    len = strlen (message);

  stamp = icon->priv->stamp++;
  
  /* Get ready to send the message */
  gtk_tray_icon_send_manager_message (icon, SYSTEM_TRAY_BEGIN_MESSAGE,
				      (Window)gtk_plug_get_id (GTK_PLUG (icon)),
				      timeout, len, stamp);

  /* Now to send the actual message */
  gdk_error_trap_push ();
  while (len > 0)
    {
      XClientMessageEvent ev;
      Display *xdisplay;

      xdisplay = GDK_DISPLAY_XDISPLAY (gtk_widget_get_display (GTK_WIDGET (icon)));
      
      memset (&ev, 0, sizeof (ev));
      ev.type = ClientMessage;
      ev.window = (Window)gtk_plug_get_id (GTK_PLUG (icon));
      ev.format = 8;
      ev.message_type = XInternAtom (xdisplay,
				     "_NET_SYSTEM_TRAY_MESSAGE_DATA", False);
      if (len > 20)
	{
	  memcpy (&ev.data, message, 20);
	  len -= 20;
	  message += 20;
	}
      else
	{
	  memcpy (&ev.data, message, len);
	  len = 0;
	}

      XSendEvent (xdisplay,
		  icon->priv->manager_window, False, 
		  StructureNotifyMask, (XEvent *)&ev);
      XSync (xdisplay, False);
    }

  gdk_error_trap_pop ();

  return stamp;
}

void
_gtk_tray_icon_cancel_message (GtkTrayIcon *icon,
			       guint        id)
{
  g_return_if_fail (GTK_IS_TRAY_ICON (icon));
  g_return_if_fail (id > 0);
  
  gtk_tray_icon_send_manager_message (icon, SYSTEM_TRAY_CANCEL_MESSAGE,
				      (Window)gtk_plug_get_id (GTK_PLUG (icon)),
				      id, 0, 0);
}

GtkTrayIcon *
_gtk_tray_icon_new_for_screen (GdkScreen  *screen, 
			       const gchar *name)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return g_object_new (GTK_TYPE_TRAY_ICON, 
		       "screen", screen, 
		       "title", name, 
		       NULL);
}

GtkTrayIcon*
_gtk_tray_icon_new (const gchar *name)
{
  return g_object_new (GTK_TYPE_TRAY_ICON, 
		       "title", name, 
		       NULL);
}

GtkOrientation
_gtk_tray_icon_get_orientation (GtkTrayIcon *icon)
{
  g_return_val_if_fail (GTK_IS_TRAY_ICON (icon), GTK_ORIENTATION_HORIZONTAL);

  return icon->priv->orientation;
}


#define __GTK_TRAY_ICON_X11_C__
#include "gtkaliasdef.c"

