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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gdk/gdkx.h"
#include "gdk/gdkkeysyms.h"
#include "gtkplug.h"

static void gtk_plug_class_init (GtkPlugClass *klass);
static void gtk_plug_init       (GtkPlug      *plug);

static void gtk_plug_realize    (GtkWidget *widget);
static void gtk_plug_unrealize	(GtkWidget *widget);
static gint gtk_plug_key_press_event (GtkWidget   *widget,
				      GdkEventKey *event);
static void gtk_plug_forward_key_press (GtkPlug *plug, GdkEventKey *event);
static gint gtk_plug_focus_in_event (GtkWidget     *widget, GdkEventFocus *event);
static gint gtk_plug_focus_out_event (GtkWidget     *widget, GdkEventFocus *event);
static void gtk_plug_set_focus       (GtkWindow         *window,
				      GtkWidget         *focus);

/* From Tk */
#define EMBEDDED_APP_WANTS_FOCUS NotifyNormal+20

static GtkWindowClass *parent_class = NULL;

guint
gtk_plug_get_type ()
{
  static guint plug_type = 0;

  if (!plug_type)
    {
      static const GtkTypeInfo plug_info =
      {
	"GtkPlug",
	sizeof (GtkPlug),
	sizeof (GtkPlugClass),
	(GtkClassInitFunc) gtk_plug_class_init,
	(GtkObjectInitFunc) gtk_plug_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      plug_type = gtk_type_unique (gtk_window_get_type (), &plug_info);
    }

  return plug_type;
}

static void
gtk_plug_class_init (GtkPlugClass *class)
{
  GtkWidgetClass *widget_class;
  GtkWindowClass *window_class;

  widget_class = (GtkWidgetClass *)class;
  window_class = (GtkWindowClass *)class;

  parent_class = gtk_type_class (gtk_window_get_type ());

  widget_class->realize = gtk_plug_realize;
  widget_class->unrealize = gtk_plug_unrealize;
  widget_class->key_press_event = gtk_plug_key_press_event;
  widget_class->focus_in_event = gtk_plug_focus_in_event;
  widget_class->focus_out_event = gtk_plug_focus_out_event;

  window_class->set_focus = gtk_plug_set_focus;
}

static void
gtk_plug_init (GtkPlug *plug)
{
  GtkWindow *window;

  window = GTK_WINDOW (plug);

  window->type = GTK_WINDOW_TOPLEVEL;
  window->auto_shrink = TRUE;
}

void
gtk_plug_construct (GtkPlug *plug, guint32 socket_id)
{
  plug->socket_window = gdk_window_lookup (socket_id);
  plug->same_app = TRUE;

  if (plug->socket_window == NULL)
    {
      plug->socket_window = gdk_window_foreign_new (socket_id);
      plug->same_app = FALSE;
    }
}

GtkWidget*
gtk_plug_new (guint32 socket_id)
{
  GtkPlug *plug;

  plug = GTK_PLUG (gtk_type_new (gtk_plug_get_type ()));
  gtk_plug_construct (plug, socket_id);
  return GTK_WIDGET (plug);
}

static void
gtk_plug_unrealize (GtkWidget *widget)
{
  GtkPlug *plug;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PLUG (widget));

  plug = GTK_PLUG (widget);

  if (plug->socket_window != NULL)
    {
      gdk_window_set_user_data (plug->socket_window, NULL);
      gdk_window_unref (plug->socket_window);
      plug->socket_window = NULL;
    }

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_plug_realize (GtkWidget *widget)
{
  GtkWindow *window;
  GtkPlug *plug;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PLUG (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  window = GTK_WINDOW (widget);
  plug = GTK_PLUG (widget);

  attributes.window_type = GDK_WINDOW_CHILD;	/* XXX GDK_WINDOW_PLUG ? */
  attributes.title = window->title;
  attributes.wmclass_name = window->wmclass_name;
  attributes.wmclass_class = window->wmclass_class;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;

  /* this isn't right - we should match our parent's visual/colormap.
   * though that will require handling "foreign" colormaps */
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_KEY_PRESS_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK |
			    GDK_FOCUS_CHANGE_MASK |
			    GDK_STRUCTURE_MASK);

  attributes_mask = GDK_WA_VISUAL | GDK_WA_COLORMAP;
  attributes_mask |= (window->title ? GDK_WA_TITLE : 0);
  attributes_mask |= (window->wmclass_name ? GDK_WA_WMCLASS : 0);

  gdk_error_trap_push ();
  widget->window = gdk_window_new (plug->socket_window, 
				   &attributes, attributes_mask);
  gdk_flush ();
  if (gdk_error_trap_pop ()) /* Uh-oh */
    {
      gdk_error_trap_push ();
      gdk_window_destroy (widget->window);
      gdk_flush ();
      gdk_error_trap_pop ();
      widget->window = gdk_window_new (NULL, &attributes, attributes_mask);
    }
  
  ((GdkWindowPrivate *)widget->window)->window_type = GDK_WINDOW_TOPLEVEL;
  gdk_window_set_user_data (widget->window, window);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static gint
gtk_plug_key_press_event (GtkWidget   *widget,
			  GdkEventKey *event)
{
  GtkWindow *window;
  GtkPlug *plug;
  GtkDirectionType direction = 0;
  gint return_val;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_PLUG (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  window = GTK_WINDOW (widget);
  plug = GTK_PLUG (widget);

  if (!GTK_WIDGET_HAS_FOCUS(widget))
    {
      gtk_plug_forward_key_press (plug, event);
      return TRUE;
    }

  return_val = FALSE;
  if (window->focus_widget)
    return_val = gtk_widget_event (window->focus_widget, (GdkEvent*) event);

#if 0
  if (!return_val && gtk_window_check_accelerator (window, event->keyval, event->state))
    return_val = TRUE;
#endif
  
  if (!return_val)
    {
      switch (event->keyval)
	{
	case GDK_space:
	  if (window->focus_widget)
	    {
	      gtk_widget_activate (window->focus_widget);
	      return_val = TRUE;
	    }
	  break;
	case GDK_Return:
	case GDK_KP_Enter:
	  if (window->default_widget &&
	      (!window->focus_widget || 
	       !GTK_WIDGET_RECEIVES_DEFAULT (window->focus_widget)))
	    {
	      gtk_widget_activate (window->default_widget);
	      return_val = TRUE;
	    }
          else if (window->focus_widget)
	    {
	      gtk_widget_activate (window->focus_widget);
	      return_val = TRUE;
	    }
	  break;
	case GDK_Up:
	case GDK_Down:
	case GDK_Left:
	case GDK_Right:
	case GDK_Tab:
	  switch (event->keyval)
	    {
	    case GDK_Up:
	      direction = GTK_DIR_UP;
	      break;
	    case GDK_Down:
	      direction = GTK_DIR_DOWN;
	      break;
	    case GDK_Left:
	      direction = GTK_DIR_LEFT;
	      break;
	    case GDK_Right:
	      direction = GTK_DIR_RIGHT;
	      break;
	    case GDK_Tab:
	      if (event->state & GDK_SHIFT_MASK)
		direction = GTK_DIR_TAB_BACKWARD;
	      else
		direction = GTK_DIR_TAB_FORWARD;
              break;
            default :
              direction = GTK_DIR_UP; /* never reached, but makes compiler happy */
	    }

	  gtk_container_focus (GTK_CONTAINER (widget), direction);

	  if (!GTK_CONTAINER (window)->focus_child)
	    {
	      gtk_window_set_focus (GTK_WINDOW (widget), NULL);

	      gdk_error_trap_push ();
	      XSetInputFocus (GDK_DISPLAY (),
			      GDK_WINDOW_XWINDOW (plug->socket_window),
			      RevertToParent, event->time);
	      gdk_flush ();
	      gdk_error_trap_pop ();

	      gtk_plug_forward_key_press (plug, event);
	    }

	  return_val = TRUE;

	  break;
	}
    }

  return return_val;
}

static void
gtk_plug_forward_key_press (GtkPlug *plug, GdkEventKey *event)
{
  XEvent xevent;
  
  xevent.xkey.type = KeyPress;
  xevent.xkey.display = GDK_WINDOW_XDISPLAY (GTK_WIDGET(plug)->window);
  xevent.xkey.window = GDK_WINDOW_XWINDOW (plug->socket_window);
  xevent.xkey.root = GDK_ROOT_WINDOW (); /* FIXME */
  xevent.xkey.time = event->time;
  /* FIXME, the following might cause big problems for
   * non-GTK apps */
  xevent.xkey.x = 0;
  xevent.xkey.y = 0;
  xevent.xkey.x_root = 0;
  xevent.xkey.y_root = 0;
  xevent.xkey.state = event->state;
  xevent.xkey.keycode =  XKeysymToKeycode(GDK_DISPLAY(), 
					  event->keyval);
  xevent.xkey.same_screen = TRUE; /* FIXME ? */

  gdk_error_trap_push ();
  XSendEvent (gdk_display,
	      GDK_WINDOW_XWINDOW (plug->socket_window),
	      False, NoEventMask, &xevent);
  gdk_flush ();
  gdk_error_trap_pop ();
}

/* Copied from Window, Ughh */

static gint
gtk_plug_focus_in_event (GtkWidget     *widget,
			 GdkEventFocus *event)
{
  GtkWindow *window;
  GdkEventFocus fevent;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_PLUG (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  /* It appears spurious focus in events can occur when
   *  the window is hidden. So we'll just check to see if
   *  the window is visible before actually handling the
   *  event
   */
  if (GTK_WIDGET_VISIBLE (widget))
    {
      GTK_OBJECT_SET_FLAGS (widget, GTK_HAS_FOCUS);

      window = GTK_WINDOW (widget);
      if (window->focus_widget && !GTK_WIDGET_HAS_FOCUS (window->focus_widget))
	{
	  fevent.type = GDK_FOCUS_CHANGE;
	  fevent.window = window->focus_widget->window;
	  fevent.in = TRUE;

	  gtk_widget_event (window->focus_widget, (GdkEvent*) &fevent);
	}
    }

  return FALSE;
}

static gint
gtk_plug_focus_out_event (GtkWidget     *widget,
			  GdkEventFocus *event)
{
  GtkWindow *window;
  GdkEventFocus fevent;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_PLUG (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_OBJECT_UNSET_FLAGS (widget, GTK_HAS_FOCUS);

  window = GTK_WINDOW (widget);

  if (window->focus_widget && GTK_WIDGET_HAS_FOCUS (window->focus_widget))
    {
      fevent.type = GDK_FOCUS_CHANGE;
      fevent.window = window->focus_widget->window;
      fevent.in = FALSE;

      gtk_widget_event (window->focus_widget, (GdkEvent*) &fevent);
    }

  return FALSE;
}

static void
gtk_plug_set_focus (GtkWindow *window,
		    GtkWidget *focus)
{
  GtkPlug *plug;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_PLUG (window));

  plug = GTK_PLUG (window);

  GTK_WINDOW_CLASS (parent_class)->set_focus (window, focus);
  
  if (focus &&
      GTK_WIDGET_CAN_FOCUS (focus) &&
      !GTK_WIDGET_HAS_FOCUS(window))
    {
      XEvent xevent;

      xevent.xfocus.type = FocusIn;
      xevent.xfocus.display = GDK_WINDOW_XDISPLAY (GTK_WIDGET(plug)->window);
      xevent.xfocus.window = GDK_WINDOW_XWINDOW (plug->socket_window);
      xevent.xfocus.mode = EMBEDDED_APP_WANTS_FOCUS;
      xevent.xfocus.detail = FALSE; /* Don't force */

      gdk_error_trap_push ();
      XSendEvent (gdk_display,
		  GDK_WINDOW_XWINDOW (plug->socket_window),
		  False, NoEventMask, &xevent);
      gdk_flush ();
      gdk_error_trap_pop ();
    }
}
