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
#include "gtkmarshalers.h"
#include "gtkplug.h"
#include "gtkprivate.h"

#include "gdk/gdkkeysyms.h"
#include "x11/gdkx.h"

#include "xembed.h"

static void            gtk_plug_class_init            (GtkPlugClass     *klass);
static void            gtk_plug_init                  (GtkPlug          *plug);
static void            gtk_plug_realize               (GtkWidget        *widget);
static void            gtk_plug_unrealize             (GtkWidget        *widget);
static void            gtk_plug_show                  (GtkWidget        *widget);
static void            gtk_plug_hide                  (GtkWidget        *widget);
static void            gtk_plug_map                   (GtkWidget        *widget);
static void            gtk_plug_unmap                 (GtkWidget        *widget);
static void            gtk_plug_size_allocate         (GtkWidget        *widget,
						       GtkAllocation    *allocation);
static gboolean        gtk_plug_key_press_event       (GtkWidget        *widget,
						       GdkEventKey      *event);
static void            gtk_plug_forward_key_press     (GtkPlug          *plug,
						       GdkEventKey      *event);
static void            gtk_plug_set_focus             (GtkWindow        *window,
						       GtkWidget        *focus);
static gboolean        gtk_plug_focus                 (GtkWidget        *widget,
						       GtkDirectionType  direction);
static void            gtk_plug_check_resize          (GtkContainer     *container);
#if 0
static void            gtk_plug_accel_entries_changed (GtkWindow        *window);
#endif
static GdkFilterReturn gtk_plug_filter_func           (GdkXEvent        *gdk_xevent,
						       GdkEvent         *event,
						       gpointer          data);

#if 0
static void gtk_plug_free_grabbed_keys (GHashTable    *key_table);
#endif
static void handle_modality_off        (GtkPlug       *plug);
static void send_xembed_message        (GtkPlug       *plug,
					glong          message,
					glong          detail,
					glong          data1,
					glong          data2,
					guint32        time);
static void xembed_set_info            (GdkWindow     *window,
					unsigned long  flags);

/* From Tk */
#define EMBEDDED_APP_WANTS_FOCUS NotifyNormal+20
  
static GtkWindowClass *parent_class = NULL;
static GtkBinClass *bin_class = NULL;

enum {
  EMBEDDED,
  LAST_SIGNAL
}; 

static guint plug_signals[LAST_SIGNAL] = { 0 };

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
  GtkWindowClass *window_class = (GtkWindowClass *)class;
  GtkContainerClass *container_class = (GtkContainerClass *)class;

  parent_class = gtk_type_class (GTK_TYPE_WINDOW);
  bin_class = gtk_type_class (GTK_TYPE_BIN);

  widget_class->realize = gtk_plug_realize;
  widget_class->unrealize = gtk_plug_unrealize;
  widget_class->key_press_event = gtk_plug_key_press_event;

  widget_class->show = gtk_plug_show;
  widget_class->hide = gtk_plug_hide;
  widget_class->map = gtk_plug_map;
  widget_class->unmap = gtk_plug_unmap;
  widget_class->size_allocate = gtk_plug_size_allocate;

  widget_class->focus = gtk_plug_focus;

  container_class->check_resize = gtk_plug_check_resize;

  window_class->set_focus = gtk_plug_set_focus;
#if 0  
  window_class->accel_entries_changed = gtk_plug_accel_entries_changed;
#endif

  plug_signals[EMBEDDED] =
    g_signal_new ("embedded",
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPlugClass, embedded),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  GTK_TYPE_NONE, 0);
}

static void
gtk_plug_init (GtkPlug *plug)
{
  GtkWindow *window;

  window = GTK_WINDOW (plug);

  window->type = GTK_WINDOW_TOPLEVEL;
}

static void
gtk_plug_set_is_child (GtkPlug  *plug,
		       gboolean  is_child)
{
  g_assert (!GTK_WIDGET (plug)->parent);
      
  if (is_child)
    {
      if (plug->modality_window)
	handle_modality_off (plug);

      if (plug->modality_group)
	{
	  gtk_window_group_remove_window (plug->modality_group, GTK_WINDOW (plug));
	  g_object_unref (plug->modality_group);
	  plug->modality_group = NULL;
	}
      
      /* As a toplevel, the MAPPED flag doesn't correspond
       * to whether the widget->window is mapped; we unmap
       * here, but don't bother remapping -- we will get mapped
       * by gtk_widget_set_parent ().
       */
      if (GTK_WIDGET_MAPPED (plug))
	gtk_widget_unmap (GTK_WIDGET (plug));
      
      GTK_WIDGET_UNSET_FLAGS (plug, GTK_TOPLEVEL);
      gtk_container_set_resize_mode (GTK_CONTAINER (plug), GTK_RESIZE_PARENT);

      _gtk_widget_propagate_hierarchy_changed (GTK_WIDGET (plug), GTK_WIDGET (plug));
    }
  else
    {
      plug->modality_group = gtk_window_group_new ();
      gtk_window_group_add_window (plug->modality_group, GTK_WINDOW (plug));
      
      GTK_WIDGET_SET_FLAGS (plug, GTK_TOPLEVEL);
      gtk_container_set_resize_mode (GTK_CONTAINER (plug), GTK_RESIZE_QUEUE);

      _gtk_widget_propagate_hierarchy_changed (GTK_WIDGET (plug), NULL);
    }
}

/**
 * _gtk_plug_add_to_socket:
 * @plug: a #GtkPlug
 * @socket: a #GtkSocket
 * 
 * Add a plug to a socket within the same application.
 **/
void
_gtk_plug_add_to_socket (GtkPlug   *plug,
			 GtkSocket *socket)
{
  GtkWidget *widget;
  
  g_return_if_fail (GTK_IS_PLUG (plug));
  g_return_if_fail (GTK_IS_SOCKET (socket));
  g_return_if_fail (GTK_WIDGET_REALIZED (socket));

  widget = GTK_WIDGET (plug);

  gtk_plug_set_is_child (plug, TRUE);
  plug->same_app = TRUE;
  socket->same_app = TRUE;
  socket->plug_widget = widget;

  plug->socket_window = GTK_WIDGET (socket)->window;

  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_reparent (widget->window, plug->socket_window, 0, 0);

  gtk_widget_set_parent (widget, GTK_WIDGET (socket));

  g_signal_emit_by_name (G_OBJECT (socket), "plug_added", 0);
}

/**
 * _gtk_plug_remove_from_socket:
 * @plug: a #GtkPlug
 * @socket: a #GtkSocket
 * 
 * Remove a plug from a socket within the same application.
 **/
void
_gtk_plug_remove_from_socket (GtkPlug   *plug,
			      GtkSocket *socket)
{
  GtkWidget *widget;
  GdkEvent event;
  gboolean result;
  gboolean widget_was_visible;

  g_return_if_fail (GTK_IS_PLUG (plug));
  g_return_if_fail (GTK_IS_SOCKET (socket));
  g_return_if_fail (GTK_WIDGET_REALIZED (plug));

  widget = GTK_WIDGET (plug);

  g_object_ref (plug);
  g_object_ref (socket);

  widget_was_visible = GTK_WIDGET_VISIBLE (plug);
  
  gdk_window_hide (widget->window);
  gdk_window_reparent (widget->window, GDK_ROOT_PARENT (), 0, 0);

  GTK_PRIVATE_SET_FLAG (plug, GTK_IN_REPARENT);
  gtk_widget_unparent (GTK_WIDGET (plug));
  GTK_PRIVATE_UNSET_FLAG (plug, GTK_IN_REPARENT);
  
  socket->plug_widget = NULL;
  socket->plug_window = NULL;
  socket->same_app = FALSE;

  plug->same_app = FALSE;
  plug->socket_window = NULL;

  gtk_plug_set_is_child (plug, FALSE);
		    
  g_signal_emit_by_name (G_OBJECT (socket), "plug_removed", &result);
  if (!result)
    gtk_widget_destroy (GTK_WIDGET (socket));

  event.any.type = GDK_DELETE;
  event.any.window = g_object_ref (widget->window);
  event.any.send_event = FALSE;
  
  if (!gtk_widget_event (widget, &event))
    gtk_widget_destroy (widget);
  
  g_object_unref (event.any.window);
  g_object_unref (plug);

  if (widget_was_visible && GTK_WIDGET_VISIBLE (socket))
    gtk_widget_queue_resize (GTK_WIDGET (socket));

  g_object_unref (socket);
}

void
gtk_plug_construct (GtkPlug         *plug,
		    GdkNativeWindow  socket_id)
{
  if (socket_id)
    {
      gpointer user_data = NULL;

      plug->socket_window = gdk_window_lookup (socket_id);

      if (plug->socket_window)
	gdk_window_get_user_data (plug->socket_window, &user_data);
      else
	plug->socket_window = gdk_window_foreign_new (socket_id);

      if (user_data)
	{
	  if (GTK_IS_SOCKET (user_data))
	    _gtk_plug_add_to_socket (plug, user_data);
	  else
	    {
	      g_warning (G_STRLOC "Can't create GtkPlug as child of non-GtkSocket");
	      plug->socket_window = NULL;
	    }
	}

      if (plug->socket_window)
	g_signal_emit (G_OBJECT (plug), plug_signals[EMBEDDED], 0);
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

/**
 * gtk_plug_get_id:
 * @plug: a #GtkPlug.
 * 
 * Gets the window ID of a #GtkPlug widget, which can then
 * be used to embed this window inside another window, for
 * instance with gtk_socket_add_id().
 * 
 * Return value: the window ID for the plug
 **/
GdkNativeWindow
gtk_plug_get_id (GtkPlug *plug)
{
  g_return_val_if_fail (GTK_IS_PLUG (plug), 0);

  if (!GTK_WIDGET_REALIZED (plug))
    gtk_widget_realize (GTK_WIDGET (plug));

  return GDK_WINDOW_XWINDOW (GTK_WIDGET (plug)->window);
}

static void
gtk_plug_unrealize (GtkWidget *widget)
{
  GtkPlug *plug;

  g_return_if_fail (GTK_IS_PLUG (widget));

  plug = GTK_PLUG (widget);

  if (plug->socket_window != NULL)
    {
      gdk_window_set_user_data (plug->socket_window, NULL);
      gdk_window_unref (plug->socket_window);
      plug->socket_window = NULL;
    }

  if (!plug->same_app)
    {
      if (plug->modality_window)
	handle_modality_off (plug);

      gtk_window_group_remove_window (plug->modality_group, GTK_WINDOW (plug));
      g_object_unref (plug->modality_group);
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
			    GDK_KEY_RELEASE_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK |
			    GDK_FOCUS_CHANGE_MASK |
			    GDK_STRUCTURE_MASK);

  attributes_mask = GDK_WA_VISUAL | GDK_WA_COLORMAP;
  attributes_mask |= (window->title ? GDK_WA_TITLE : 0);
  attributes_mask |= (window->wmclass_name ? GDK_WA_WMCLASS : 0);

  if (GTK_WIDGET_TOPLEVEL (widget))
    {
      attributes.window_type = GDK_WINDOW_TOPLEVEL;

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
      
      gdk_window_add_filter (widget->window, gtk_plug_filter_func, widget);

      plug->modality_group = gtk_window_group_new ();
      gtk_window_group_add_window (plug->modality_group, window);
      
      xembed_set_info (widget->window, 0);
    }
  else
    widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);      
  
  gdk_window_set_user_data (widget->window, window);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
gtk_plug_show (GtkWidget *widget)
{
  if (GTK_WIDGET_TOPLEVEL (widget))
    GTK_WIDGET_CLASS (parent_class)->show (widget);
  else
    GTK_WIDGET_CLASS (bin_class)->show (widget);
}

static void
gtk_plug_hide (GtkWidget *widget)
{
  if (GTK_WIDGET_TOPLEVEL (widget))
    GTK_WIDGET_CLASS (parent_class)->hide (widget);
  else
    GTK_WIDGET_CLASS (bin_class)->hide (widget);
}

/* From gdkinternals.h */
void gdk_synthesize_window_state (GdkWindow     *window,
                                  GdkWindowState unset_flags,
                                  GdkWindowState set_flags);

static void
gtk_plug_map (GtkWidget *widget)
{
  if (GTK_WIDGET_TOPLEVEL (widget))
    {
      GtkBin *bin = GTK_BIN (widget);
      
      GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

      if (bin->child &&
	  GTK_WIDGET_VISIBLE (bin->child) &&
	  !GTK_WIDGET_MAPPED (bin->child))
	gtk_widget_map (bin->child);

      xembed_set_info (widget->window, XEMBED_MAPPED);
      
      gdk_synthesize_window_state (widget->window,
				   GDK_WINDOW_STATE_WITHDRAWN,
				   0);
    }
  else
    GTK_WIDGET_CLASS (bin_class)->map (widget);
}

static void
gtk_plug_unmap (GtkWidget *widget)
{
  if (GTK_WIDGET_TOPLEVEL (widget))
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

      gdk_window_hide (widget->window);
      xembed_set_info (widget->window, 0);
      
      gdk_synthesize_window_state (widget->window,
				   0,
				   GDK_WINDOW_STATE_WITHDRAWN);
    }
  else
    GTK_WIDGET_CLASS (bin_class)->unmap (widget);
}

static void
gtk_plug_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  if (GTK_WIDGET_TOPLEVEL (widget))
    GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
  else
    {
      GtkBin *bin = GTK_BIN (widget);

      widget->allocation = *allocation;

      if (GTK_WIDGET_REALIZED (widget))
	gdk_window_move_resize (widget->window,
				allocation->x, allocation->y,
				allocation->width, allocation->height);

      if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
	{
	  GtkAllocation child_allocation;
	  
	  child_allocation.x = child_allocation.y = GTK_CONTAINER (widget)->border_width;
	  child_allocation.width =
	    MAX (1, (gint)allocation->width - child_allocation.x * 2);
	  child_allocation.height =
	    MAX (1, (gint)allocation->height - child_allocation.y * 2);
	  
	  gtk_widget_size_allocate (bin->child, &child_allocation);
	}
      
    }
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
  XSendEvent (GDK_DISPLAY (),
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
      XSendEvent (GDK_DISPLAY (),
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

  accel_groups = gtk_accel_groups_from_object (G_OBJECT (window));
  
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
gtk_plug_focus (GtkWidget        *widget,
		GtkDirectionType  direction)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkPlug *plug = GTK_PLUG (widget);
  GtkWindow *window = GTK_WINDOW (widget);
  GtkContainer *container = GTK_CONTAINER (widget);
  GtkWidget *old_focus_child = container->focus_child;
  GtkWidget *parent;
  
  /* We override GtkWindow's behavior, since we don't want wrapping here.
   */
  if (old_focus_child)
    {
      if (gtk_widget_child_focus (old_focus_child, direction))
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
      
      if (gtk_widget_child_focus (bin->child, direction))
        return TRUE;
    }

  return FALSE;
}

static void
gtk_plug_check_resize (GtkContainer *container)
{
  if (GTK_WIDGET_TOPLEVEL (container))
    GTK_CONTAINER_CLASS (parent_class)->check_resize (container);
  else
    GTK_CONTAINER_CLASS (bin_class)->check_resize (container);
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
      xevent.xclient.message_type = gdk_x11_get_xatom_by_name ("_XEMBED");
      xevent.xclient.format = 32;
      xevent.xclient.data.l[0] = time;
      xevent.xclient.data.l[1] = message;
      xevent.xclient.data.l[2] = detail;
      xevent.xclient.data.l[3] = data1;
      xevent.xclient.data.l[4] = data2;

      gdk_error_trap_push ();
      XSendEvent (GDK_DISPLAY (),
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

  gtk_widget_child_focus (GTK_WIDGET (plug), direction);
}

static void
handle_modality_on (GtkPlug *plug)
{
  if (!plug->modality_window)
    {
      plug->modality_window = gtk_window_new (GTK_WINDOW_POPUP);
      gtk_window_group_add_window (plug->modality_group, GTK_WINDOW (plug->modality_window));
      gtk_grab_add (plug->modality_window);
    }
}

static void
handle_modality_off (GtkPlug *plug)
{
  if (plug->modality_window)
    {
      gtk_widget_destroy (plug->modality_window);
      plug->modality_window = NULL;
    }
}

static void
xembed_set_info (GdkWindow     *gdk_window,
		 unsigned long  flags)
{
  Display *display = GDK_WINDOW_XDISPLAY (gdk_window);
  Window window = GDK_WINDOW_XWINDOW (gdk_window);
  unsigned long buffer[2];
  
  Atom xembed_info_atom = gdk_x11_get_xatom_by_name ("_XEMBED_INFO");

  buffer[1] = 0;		/* Protocol version */
  buffer[1] = flags;

  XChangeProperty (display, window,
		   xembed_info_atom, xembed_info_atom, 32,
		   PropModeReplace,
		   (unsigned char *)buffer, 2);
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
      if (xevent->xclient.message_type == gdk_x11_get_xatom_by_name ("_XEMBED"))
	{
	  handle_xembed_message (plug,
				 xevent->xclient.data.l[1],
				 xevent->xclient.data.l[2],
				 xevent->xclient.data.l[3],
				 xevent->xclient.data.l[4],
				 xevent->xclient.data.l[0]);
				 

	  return GDK_FILTER_REMOVE;
	}
      else if (xevent->xclient.message_type == gdk_x11_get_xatom_by_name ("WM_DELETE_WINDOW"))
	{
	  /* We filter these out because we take being reparented back to the
	   * root window as the reliable end of the embedding protocol
	   */

	  return GDK_FILTER_REMOVE;
	}
      break;
    case ReparentNotify:
      {
	XReparentEvent *xre = &xevent->xreparent;
	gboolean was_embedded = plug->socket_window != NULL;

	return_val = GDK_FILTER_REMOVE;
	
	g_object_ref (plug);
	
	if (was_embedded)
	  {
	    /* End of embedding protocol for previous socket */
	    
	    /* FIXME: race if we remove from another socket and
	     * then add to a local window before we get notification
	     * Probably need check in _gtk_plug_add_to_socket
	     */
	    
	    if (xre->parent != GDK_WINDOW_XWINDOW (plug->socket_window))
	      {
		GtkWidget *widget = GTK_WIDGET (plug);

		gdk_window_set_user_data (plug->socket_window, NULL);
		gdk_window_unref (plug->socket_window);
		plug->socket_window = NULL;

		/* Emit a delete window, as if the user attempted
		 * to close the toplevel. Simple as to how we
		 * handle WM_DELETE_WINDOW, if it isn't handled
		 * we destroy the widget. BUt only do this if
		 * we are being reparented to the root window.
		 * Moving from one embedder to another should
		 * be invisible to the app.
		 */

		if (xre->parent == GDK_ROOT_WINDOW())
		  {
		    GdkEvent event;
		    
		    event.any.type = GDK_DELETE;
		    event.any.window = g_object_ref (widget->window);
		    event.any.send_event = FALSE;
		    
		    if (!gtk_widget_event (widget, &event))
		      gtk_widget_destroy (widget);
		    
		    g_object_unref (event.any.window);
		  }
	      }
	    else
	      break;
	  }

	if (xre->parent != GDK_ROOT_WINDOW ())
	  {
	    /* Start of embedding protocol */

	    plug->socket_window = gdk_window_lookup (xre->parent);
	    if (plug->socket_window)
	      {
		gpointer user_data = NULL;
		gdk_window_get_user_data (plug->socket_window, &user_data);

		if (user_data)
		  {
		    g_warning (G_STRLOC "Plug reparented unexpectedly into window in the same process");
		    plug->socket_window = NULL;
		    break;
		  }

		g_object_ref (plug->socket_window);
	      }
	    else
	      {
		plug->socket_window = gdk_window_foreign_new (xre->parent);
		if (!plug->socket_window) /* Already gone */
		  break;
	      }

	    /* FIXME: Add grabbed keys here */

	    if (!was_embedded)
	      g_signal_emit (G_OBJECT (plug), plug_signals[EMBEDDED], 0);
	  }
	
	g_object_unref (plug);
	
	break;
      }
    }

  return GDK_FILTER_CONTINUE;
}
