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

#include "gtkmain.h"
#include "gtkplug.h"

#include "gdk/gdkkeysyms.h"
#include "x11/gdkx.h"

#include "xembed.h"

static void            gtk_plug_class_init            (GtkPlugClass     *klass);
static void            gtk_plug_init                  (GtkPlug          *plug);
static void            gtk_plug_realize               (GtkWidget        *widget);
static void            gtk_plug_unrealize             (GtkWidget        *widget);
static gboolean        gtk_plug_key_press_event       (GtkWidget        *widget,
						       GdkEventKey      *event);
static void            gtk_plug_forward_key_press     (GtkPlug          *plug,
						       GdkEventKey      *event);
static void            gtk_plug_set_focus             (GtkWindow        *window,
						       GtkWidget        *focus);
static gboolean        gtk_plug_focus                 (GtkContainer     *container,
						       GtkDirectionType  direction);
static void            gtk_plug_accel_entries_changed (GtkWindow        *window);
static GdkFilterReturn gtk_plug_filter_func           (GdkXEvent        *gdk_xevent,
						       GdkEvent         *event,
						       gpointer          data);

static void gtk_plug_free_grabbed_keys (GHashTable *key_table);
static void handle_modality_off        (GtkPlug    *plug);
static void send_xembed_message        (GtkPlug    *plug,
					glong       message,
					glong       detail,
					glong       data1,
					glong       data2,
					guint32     time);

/* From Tk */
#define EMBEDDED_APP_WANTS_FOCUS NotifyNormal+20
  
static GtkWindowClass *parent_class = NULL;

GtkType
gtk_plug_get_type ()
{
  static GtkType plug_type = 0;

  if (!plug_type)
    {
      static const GTypeInfo plug_info =
      {
	sizeof (GtkPlugClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
	(GClassInitFunc) gtk_plug_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data */
	sizeof (GtkPlug),
	16,             /* n_preallocs */
	(GInstanceInitFunc) gtk_plug_init,
      };

      plug_type = g_type_register_static (GTK_TYPE_WINDOW, "GtkPlug", &plug_info, 0);
    }

  return plug_type;
}

static void
gtk_plug_class_init (GtkPlugClass *class)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *)class;
  GtkContainerClass *container_class = (GtkContainerClass *)class;
  GtkWindowClass *window_class = (GtkWindowClass *)class;

  parent_class = gtk_type_class (GTK_TYPE_WINDOW);

  widget_class->realize = gtk_plug_realize;
  widget_class->unrealize = gtk_plug_unrealize;
  widget_class->key_press_event = gtk_plug_key_press_event;

  container_class->focus = gtk_plug_focus;

  window_class->set_focus = gtk_plug_set_focus;
#if 0  
  window_class->accel_entries_changed = gtk_plug_accel_entries_changed;
#endif
}

static void
gtk_plug_init (GtkPlug *plug)
{
  GtkWindow *window;

  window = GTK_WINDOW (plug);

  window->type = GTK_WINDOW_TOPLEVEL;
  window->auto_shrink = TRUE;

#if 0  
  gtk_window_set_grab_group (window, window);
#endif  
}

void
gtk_plug_construct (GtkPlug *plug, GdkNativeWindow socket_id)
{
  if (socket_id)
    {
      plug->socket_window = gdk_window_lookup (socket_id);
      plug->same_app = TRUE;

      if (plug->socket_window == NULL)
	{
	  plug->socket_window = gdk_window_foreign_new (socket_id);
	  plug->same_app = FALSE;
	}
    }
}

GtkWidget*
gtk_plug_new (GdkNativeWindow socket_id)
{
  GtkPlug *plug;

  plug = GTK_PLUG (gtk_type_new (GTK_TYPE_PLUG));
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

#if 0  
  if (plug->modality_window)
    handle_modality_off (plug);
#endif  
  
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
  
  GDK_WINDOW_TYPE (widget->window) = GDK_WINDOW_TOPLEVEL;
  gdk_window_set_user_data (widget->window, window);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  gdk_window_add_filter (widget->window, gtk_plug_filter_func, widget);
}

static gboolean
gtk_plug_key_press_event (GtkWidget   *widget,
			  GdkEventKey *event)
{
  if (!GTK_WINDOW (widget)->has_focus)
    {
      gtk_plug_forward_key_press (GTK_PLUG (widget), event);
      return TRUE;
    }
  else
    return GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);
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

static void
gtk_plug_set_focus (GtkWindow *window,
		    GtkWidget *focus)
{
  GtkPlug *plug = GTK_PLUG (window);

  GTK_WINDOW_CLASS (parent_class)->set_focus (window, focus);
  
  /* Ask for focus from embedder
   */

  if (focus && !window->has_focus)
    {
#if 0      
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
#endif

      send_xembed_message (plug, XEMBED_REQUEST_FOCUS, 0, 0, 0,
			   gtk_get_current_event_time ());
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
add_grabbed_keys (gpointer key, gpointer val, gpointer data)
{
  GrabbedKey *grabbed_key = key;
  GtkPlug *plug = data;

  if (!plug->grabbed_keys ||
      !g_hash_table_lookup (plug->grabbed_keys, grabbed_key))
    {
      send_xembed_message (plug, XEMBED_GRAB_KEY, 0, 
			   grabbed_key->accelerator_key, grabbed_key->accelerator_mods,
			   gtk_get_current_event_time ());
    }
}

static void
remove_grabbed_keys (gpointer key, gpointer val, gpointer data)
{
  GrabbedKey *grabbed_key = key;
  GtkPlug *plug = data;

  if (!plug->grabbed_keys ||
      !g_hash_table_lookup (plug->grabbed_keys, grabbed_key))
    {
      send_xembed_message (plug, XEMBED_UNGRAB_KEY, 0, 
			   grabbed_key->accelerator_key, grabbed_key->accelerator_mods,
			   gtk_get_current_event_time ());
    }
}

static void
gtk_plug_free_grabbed_keys (GHashTable *key_table)
{
  g_hash_table_foreach (key_table, (GHFunc)g_free, NULL);
  g_hash_table_destroy (key_table);
}

static void
gtk_plug_accel_entries_changed (GtkWindow *window)
{
  GHashTable *new_grabbed_keys, *old_grabbed_keys;
  GSList *accel_groups, *tmp_list;
  GtkPlug *plug = GTK_PLUG (window);

  new_grabbed_keys = g_hash_table_new (grabbed_key_hash, grabbed_key_equal);

  accel_groups = gtk_accel_groups_from_object (GTK_OBJECT (window));
  
  tmp_list = accel_groups;

  while (tmp_list)
    {
      GtkAccelGroup *accel_group = tmp_list->data;
      gint i, n_entries;
      GtkAccelEntry *entries;

      gtk_accel_group_get_entries (accel_group, &entries, &n_entries);

      for (i = 0; i < n_entries; i++)
	{
	  GdkKeymapKey *keys;
	  gint n_keys;
	  
	  if (gdk_keymap_get_entries_for_keyval (NULL, entries[i].accelerator_key, &keys, &n_keys))
	    {
	      GrabbedKey *key = g_new (GrabbedKey, 1);
	      
	      key->accelerator_key = keys[0].keycode;
	      key->accelerator_mods = entries[i].accelerator_mods;
	      
	      g_hash_table_insert (new_grabbed_keys, key, key);

	      g_free (keys);
	    }
	}
      
      tmp_list = tmp_list->next;
    }

  g_hash_table_foreach (new_grabbed_keys, add_grabbed_keys, plug);

  old_grabbed_keys = plug->grabbed_keys;
  plug->grabbed_keys = new_grabbed_keys;

  if (old_grabbed_keys)
    {
      g_hash_table_foreach (old_grabbed_keys, remove_grabbed_keys, plug);
      gtk_plug_free_grabbed_keys (old_grabbed_keys);
    }

}
#endif

static gboolean
gtk_plug_focus (GtkContainer     *container,
		GtkDirectionType  direction)
{
  GtkBin *bin = GTK_BIN (container);
  GtkPlug *plug = GTK_PLUG (container);
  GtkWindow *window = GTK_WINDOW (container);
  GtkWidget *old_focus_child = container->focus_child;
  GtkWidget *parent;
  
  /* We override GtkWindow's behavior, since we don't want wrapping here.
   */
  if (old_focus_child)
    {
      if (GTK_IS_CONTAINER (old_focus_child) &&
	  GTK_WIDGET_DRAWABLE (old_focus_child) &&
	  GTK_WIDGET_IS_SENSITIVE (old_focus_child) &&
	  gtk_container_focus (GTK_CONTAINER (old_focus_child), direction))
	return TRUE;

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

	  if (!GTK_CONTAINER (window)->focus_child)
	    {
	      gint message = -1;

	      switch (direction)
		{
		case GTK_DIR_UP:
		case GTK_DIR_LEFT:
		case GTK_DIR_TAB_BACKWARD:
		  message = XEMBED_FOCUS_PREV;
		  break;
		case GTK_DIR_DOWN:
		case GTK_DIR_RIGHT:
		case GTK_DIR_TAB_FORWARD:
		  message = XEMBED_FOCUS_NEXT;
		  break;
		}
	      
	      send_xembed_message (plug, message, 0, 0, 0,
				   gtk_get_current_event_time ());
	      
#if 0	      
	      gtk_window_set_focus (GTK_WINDOW (widget), NULL);

	      gdk_error_trap_push ();
	      XSetInputFocus (GDK_DISPLAY (),
			      GDK_WINDOW_XWINDOW (plug->socket_window),
			      RevertToParent, event->time);
	      gdk_flush ();
	      gdk_error_trap_pop ();

	      gtk_plug_forward_key_press (plug, event);
#endif	      
	    }
	}

      return FALSE;
    }
  else
    {
      /* Try to focus the first widget in the window */
      if (GTK_WIDGET_DRAWABLE (bin->child) &&
	  GTK_WIDGET_IS_SENSITIVE (bin->child))
	{
	  if (GTK_IS_CONTAINER (bin->child))
	    {
	      if (gtk_container_focus (GTK_CONTAINER (bin->child), direction))
		return TRUE;
	    }
	  else if (GTK_WIDGET_CAN_FOCUS (bin->child))
	    {
	      gtk_widget_grab_focus (bin->child);
	      return TRUE;
	    }
	}
    }

  return FALSE;
}

static void
send_xembed_message (GtkPlug *plug,
		     glong      message,
		     glong      detail,
		     glong      data1,
		     glong      data2,
		     guint32    time)
{
  if (plug->socket_window)
    {
      XEvent xevent;

      xevent.xclient.window = GDK_WINDOW_XWINDOW (plug->socket_window);
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
		  GDK_WINDOW_XWINDOW (plug->socket_window),
		  False, NoEventMask, &xevent);
      gdk_flush ();
      gdk_error_trap_pop ();
    }
}

static void
focus_first_last (GtkPlug          *plug,
		  GtkDirectionType  direction)
{
  GtkWindow *window = GTK_WINDOW (plug);
  GtkWidget *parent;
  
  if (window->focus_widget)
    {
      parent = window->focus_widget->parent;
      while (parent)
	{
	  gtk_container_set_focus_child (GTK_CONTAINER (parent), NULL);
	  parent = GTK_WIDGET (parent)->parent;
	}
      
      gtk_window_set_focus (GTK_WINDOW (plug), NULL);
    }

  gtk_container_focus (GTK_CONTAINER (plug), direction);
}

static void
handle_modality_on (GtkPlug *plug)
{
#if 0
  if (!plug->modality_window)
    {
      plug->modality_window = gtk_window_new (GTK_WINDOW_POPUP);
      gtk_window_set_grab_group (GTK_WINDOW (plug->modality_window), GTK_WINDOW (plug));
      gtk_grab_add (plug->modality_window);
    }
#endif  
}

static void
handle_modality_off (GtkPlug *plug)
{
#if 0  
  if (plug->modality_window)
    {
      gtk_grab_remove (plug->modality_window);
      gtk_widget_destroy (plug->modality_window);
      plug->modality_window = NULL;
    }
#endif  
}

static void
handle_xembed_message (GtkPlug   *plug,
		       glong      message,
		       glong      detail,
		       glong      data1,
		       glong      data2,
		       guint32    time)
{
  GTK_NOTE (PLUGSOCKET,
	    g_message ("Message of type %ld received", message));
  
  switch (message)
    {
    case XEMBED_EMBEDDED_NOTIFY:
      break;
    case XEMBED_WINDOW_ACTIVATE:
      GTK_NOTE(PLUGSOCKET,
	       g_message ("GtkPlug: ACTIVATE received"));
      break;
    case XEMBED_WINDOW_DEACTIVATE:
      GTK_NOTE(PLUGSOCKET,
	       g_message ("GtkPlug: DEACTIVATE received"));
      break;
      
    case XEMBED_MODALITY_ON:
      handle_modality_on (plug);
      break;
    case XEMBED_MODALITY_OFF:
      handle_modality_off (plug);
      break;

    case XEMBED_FOCUS_IN:
      switch (detail)
	{
	case XEMBED_FOCUS_FIRST:
	  focus_first_last (plug, GTK_DIR_TAB_FORWARD);
	  break;
	case XEMBED_FOCUS_LAST:
	  focus_first_last (plug, GTK_DIR_TAB_BACKWARD);
	  break;
	case XEMBED_FOCUS_CURRENT:
	  /* fall through */;
	}
      
    case XEMBED_FOCUS_OUT:
      {
	GdkEvent event;

	event.focus_change.type = GDK_FOCUS_CHANGE;
	event.focus_change.window = GTK_WIDGET (plug)->window;
	event.focus_change.send_event = TRUE;
	event.focus_change.in = (message == XEMBED_FOCUS_IN);

	gtk_widget_event (GTK_WIDGET (plug), &event);

	break;
      }
      
    case XEMBED_REQUEST_FOCUS:
    case XEMBED_FOCUS_NEXT:
    case XEMBED_FOCUS_PREV:
    case XEMBED_GRAB_KEY:
    case XEMBED_UNGRAB_KEY:
      g_warning ("GtkPlug: Invalid _XEMBED message of type %ld received", message);
      break;
      
    default:
      GTK_NOTE(PLUGSOCKET,
	       g_message ("GtkPlug: Ignoring unknown _XEMBED message of type %ld", message));
      break;
    }
}

static GdkFilterReturn
gtk_plug_filter_func (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
  GtkPlug *plug = GTK_PLUG (data);
  XEvent *xevent = (XEvent *)gdk_xevent;

  GdkFilterReturn return_val;
  
  return_val = GDK_FILTER_CONTINUE;

  switch (xevent->type)
    {
    case ClientMessage:
      if (xevent->xclient.message_type == gdk_atom_intern ("_XEMBED", FALSE))
	{
	  handle_xembed_message (plug,
				 xevent->xclient.data.l[1],
				 xevent->xclient.data.l[2],
				 xevent->xclient.data.l[3],
				 xevent->xclient.data.l[4],
				 xevent->xclient.data.l[0]);
				 

	  return GDK_FILTER_REMOVE;
	}
      break;
    }

  return GDK_FILTER_CONTINUE;
}
