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

#include "gtkalias.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkplug.h"
#include "gtkprivate.h"

#include "gdk/gdkkeysyms.h"
#include "x11/gdkx.h"

#include "gtkxembed.h"

static void            gtk_plug_class_init            (GtkPlugClass     *klass);
static void            gtk_plug_init                  (GtkPlug          *plug);
static void            gtk_plug_finalize              (GObject          *object);
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
static gboolean        gtk_plug_focus_event           (GtkWidget        *widget,
						       GdkEventFocus    *event);
static void            gtk_plug_set_focus             (GtkWindow        *window,
						       GtkWidget        *focus);
static gboolean        gtk_plug_focus                 (GtkWidget        *widget,
						       GtkDirectionType  direction);
static void            gtk_plug_check_resize          (GtkContainer     *container);
static void            gtk_plug_keys_changed          (GtkWindow        *window);
static GdkFilterReturn gtk_plug_filter_func           (GdkXEvent        *gdk_xevent,
						       GdkEvent         *event,
						       gpointer          data);

static void handle_modality_off        (GtkPlug       *plug);
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

GType
gtk_plug_get_type (void)
{
  static GType plug_type = 0;

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

      plug_type = g_type_register_static (GTK_TYPE_WINDOW, "GtkPlug",
					  &plug_info, 0);
    }

  return plug_type;
}

static void
gtk_plug_class_init (GtkPlugClass *class)
{
  GObjectClass *gobject_class = (GObjectClass *)class;
  GtkWidgetClass *widget_class = (GtkWidgetClass *)class;
  GtkWindowClass *window_class = (GtkWindowClass *)class;
  GtkContainerClass *container_class = (GtkContainerClass *)class;

  parent_class = g_type_class_peek_parent (class);
  bin_class = g_type_class_peek (GTK_TYPE_BIN);

  gobject_class->finalize = gtk_plug_finalize;
  
  widget_class->realize = gtk_plug_realize;
  widget_class->unrealize = gtk_plug_unrealize;
  widget_class->key_press_event = gtk_plug_key_press_event;
  widget_class->focus_in_event = gtk_plug_focus_event;
  widget_class->focus_out_event = gtk_plug_focus_event;

  widget_class->show = gtk_plug_show;
  widget_class->hide = gtk_plug_hide;
  widget_class->map = gtk_plug_map;
  widget_class->unmap = gtk_plug_unmap;
  widget_class->size_allocate = gtk_plug_size_allocate;

  widget_class->focus = gtk_plug_focus;

  container_class->check_resize = gtk_plug_check_resize;

  window_class->set_focus = gtk_plug_set_focus;
  window_class->keys_changed = gtk_plug_keys_changed;

  plug_signals[EMBEDDED] =
    g_signal_new ("embedded",
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPlugClass, embedded),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
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
      if (GTK_WINDOW (plug)->focus_widget)
	gtk_window_set_focus (GTK_WINDOW (plug), NULL);
      if (GTK_WINDOW (plug)->default_widget)
	gtk_window_set_default (GTK_WINDOW (plug), NULL);
	  
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
 * @socket_: a #GtkSocket
 * 
 * Adds a plug to a socket within the same application.
 **/
void
_gtk_plug_add_to_socket (GtkPlug   *plug,
			 GtkSocket *socket)
{
  GtkWidget *widget;
  gint w, h;
  
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
    {
      gdk_drawable_get_size (GDK_DRAWABLE (widget->window), &w, &h);
      gdk_window_reparent (widget->window, plug->socket_window, -w, -h);
    }

  gtk_widget_set_parent (widget, GTK_WIDGET (socket));

  g_signal_emit_by_name (socket, "plug_added", 0);
}

static void
send_delete_event (GtkWidget *widget)
{
  GdkEvent *event = gdk_event_new (GDK_DELETE);
  
  event->any.window = g_object_ref (widget->window);
  event->any.send_event = FALSE;

  gtk_widget_ref (widget);
  
  if (!gtk_widget_event (widget, event))
    gtk_widget_destroy (widget);
  
  gtk_widget_unref (widget);

  gdk_event_free (event);
}

/**
 * _gtk_plug_remove_from_socket:
 * @plug: a #GtkPlug
 * @socket_: a #GtkSocket
 * 
 * Removes a plug from a socket within the same application.
 **/
void
_gtk_plug_remove_from_socket (GtkPlug   *plug,
			      GtkSocket *socket)
{
  GtkWidget *widget;
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
  gdk_window_reparent (widget->window,
		       gtk_widget_get_root_window (widget),
		       0, 0);

  GTK_PRIVATE_SET_FLAG (plug, GTK_IN_REPARENT);
  gtk_widget_unparent (GTK_WIDGET (plug));
  GTK_PRIVATE_UNSET_FLAG (plug, GTK_IN_REPARENT);
  
  socket->plug_widget = NULL;
  if (socket->plug_window != NULL)
    {
      g_object_unref (socket->plug_window);
      socket->plug_window = NULL;
    }
  
  socket->same_app = FALSE;

  plug->same_app = FALSE;
  plug->socket_window = NULL;

  gtk_plug_set_is_child (plug, FALSE);
		    
  g_signal_emit_by_name (socket, "plug_removed", &result);
  if (!result)
    gtk_widget_destroy (GTK_WIDGET (socket));

  send_delete_event (widget);

  g_object_unref (plug);

  if (widget_was_visible && GTK_WIDGET_VISIBLE (socket))
    gtk_widget_queue_resize (GTK_WIDGET (socket));

  g_object_unref (socket);
}

/**
 * gtk_plug_construct:
 * @plug: a #GtkPlug.
 * @socket_id: the XID of the socket's window.
 *
 * Finish the initialization of @plug for a given #GtkSocket identified by
 * @socket_id. This function will generally only be used by classes deriving from #GtkPlug.
 **/
void
gtk_plug_construct (GtkPlug         *plug,
		    GdkNativeWindow  socket_id)
{
  gtk_plug_construct_for_display (plug, gdk_display_get_default (), socket_id);
}

/**
 * gtk_plug_construct_for_display:
 * @plug: a #GtkPlug.
 * @display: the #GdkDisplay associated with @socket_id's 
 *	     #GtkSocket.
 * @socket_id: the XID of the socket's window.
 *
 * Finish the initialization of @plug for a given #GtkSocket identified by
 * @socket_id which is currently displayed on @display.
 * This function will generally only be used by classes deriving from #GtkPlug.
 *
 * Since: 2.2
 **/
void
gtk_plug_construct_for_display (GtkPlug         *plug,
				GdkDisplay	*display,
				GdkNativeWindow  socket_id)
{
  if (socket_id)
    {
      gpointer user_data = NULL;

      plug->socket_window = gdk_window_lookup_for_display (display, socket_id);
      
      if (plug->socket_window)
	gdk_window_get_user_data (plug->socket_window, &user_data);
      else
	plug->socket_window = gdk_window_foreign_new_for_display (display, socket_id);
	  
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
	g_signal_emit (plug, plug_signals[EMBEDDED], 0);
    }
}

/**
 * gtk_plug_new:
 * @socket_id:  the window ID of the socket, or 0.
 * 
 * Creates a new plug widget inside the #GtkSocket identified
 * by @socket_id. If @socket_id is 0, the plug is left "unplugged" and
 * can later be plugged into a #GtkSocket by  gtk_socket_add_id().
 * 
 * Return value: the new #GtkPlug widget.
 **/
GtkWidget*
gtk_plug_new (GdkNativeWindow socket_id)
{
  return gtk_plug_new_for_display (gdk_display_get_default (), socket_id);
}

/**
 * gtk_plug_new_for_display:
 * @display : the #GdkDisplay on which @socket_id is displayed
 * @socket_id: the XID of the socket's window.
 * 
 * Create a new plug widget inside the #GtkSocket identified by socket_id.
 *
 * Return value: the new #GtkPlug widget.
 *
 * Since: 2.2
 */
GtkWidget*
gtk_plug_new_for_display (GdkDisplay	  *display,
			  GdkNativeWindow  socket_id)
{
  GtkPlug *plug;

  plug = g_object_new (GTK_TYPE_PLUG, NULL);
  gtk_plug_construct_for_display (plug, display, socket_id);
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
gtk_plug_finalize (GObject *object)
{
  GtkPlug *plug = GTK_PLUG (object);

  if (plug->grabbed_keys)
    {
      g_hash_table_destroy (plug->grabbed_keys);
      plug->grabbed_keys = NULL;
    }
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
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
      g_object_unref (plug->socket_window);
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
			    GDK_STRUCTURE_MASK);

  attributes_mask = GDK_WA_VISUAL | GDK_WA_COLORMAP;
  attributes_mask |= (window->title ? GDK_WA_TITLE : 0);
  attributes_mask |= (window->wmclass_name ? GDK_WA_WMCLASS : 0);

  if (GTK_WIDGET_TOPLEVEL (widget))
    {
      attributes.window_type = GDK_WINDOW_TOPLEVEL;

      gdk_error_trap_push ();
      if (plug->socket_window)
	widget->window = gdk_window_new (plug->socket_window, 
					 &attributes, attributes_mask);
      else /* If it's a passive plug, we use the root window */
	widget->window = gdk_window_new (gtk_widget_get_root_window (widget),
					 &attributes, attributes_mask);

      gdk_display_sync (gtk_widget_get_display (widget));
      if (gdk_error_trap_pop ()) /* Uh-oh */
	{
	  gdk_error_trap_push ();
	  gdk_window_destroy (widget->window);
	  gdk_flush ();
	  gdk_error_trap_pop ();
	  widget->window = gdk_window_new (gtk_widget_get_root_window (widget),
					   &attributes, attributes_mask);
	}
      
      gdk_window_add_filter (widget->window, gtk_plug_filter_func, widget);

      plug->modality_group = gtk_window_group_new ();
      gtk_window_group_add_window (plug->modality_group, window);
      
      xembed_set_info (widget->window, 0);
    }
  else
    widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), 
				     &attributes, attributes_mask);      
  
  gdk_window_set_user_data (widget->window, window);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  gdk_window_enable_synchronized_configure (widget->window);
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
  if (GTK_WIDGET_TOPLEVEL (widget))
    return GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);
  else
    return FALSE;
}

static gboolean
gtk_plug_focus_event (GtkWidget      *widget,
		      GdkEventFocus  *event)
{
  /* We eat focus-in events and focus-out events, since they
   * can be generated by something like a keyboard grab on
   * a child of the plug.
   */
  return FALSE;
}

static void
gtk_plug_set_focus (GtkWindow *window,
		    GtkWidget *focus)
{
  GtkPlug *plug = GTK_PLUG (window);

  GTK_WINDOW_CLASS (parent_class)->set_focus (window, focus);
  
  /* Ask for focus from embedder
   */

  if (focus && !window->has_toplevel_focus)
    {
      _gtk_xembed_send_message (plug->socket_window,
				XEMBED_REQUEST_FOCUS, 0, 0, 0);
    }
}

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
add_grabbed_key (gpointer key, gpointer val, gpointer data)
{
  GrabbedKey *grabbed_key = key;
  GtkPlug *plug = data;

  if (!plug->grabbed_keys ||
      !g_hash_table_lookup (plug->grabbed_keys, grabbed_key))
    {
      _gtk_xembed_send_message (plug->socket_window, XEMBED_GTK_GRAB_KEY, 0, 
				grabbed_key->accelerator_key, grabbed_key->accelerator_mods);
    }
}

static void
add_grabbed_key_always (gpointer key, gpointer val, gpointer data)
{
  GrabbedKey *grabbed_key = key;
  GtkPlug *plug = data;

  _gtk_xembed_send_message (plug->socket_window, XEMBED_GTK_GRAB_KEY, 0, 
			    grabbed_key->accelerator_key, grabbed_key->accelerator_mods);
}

static void
remove_grabbed_key (gpointer key, gpointer val, gpointer data)
{
  GrabbedKey *grabbed_key = key;
  GtkPlug *plug = data;

  if (!plug->grabbed_keys ||
      !g_hash_table_lookup (plug->grabbed_keys, grabbed_key))
    {
      _gtk_xembed_send_message (plug->socket_window, XEMBED_GTK_UNGRAB_KEY, 0, 
				grabbed_key->accelerator_key, grabbed_key->accelerator_mods);
    }
}

static void
keys_foreach (GtkWindow      *window,
	      guint           keyval,
	      GdkModifierType modifiers,
	      gboolean        is_mnemonic,
	      gpointer        data)
{
  GHashTable *new_grabbed_keys = data;
  GrabbedKey *key = g_new (GrabbedKey, 1);

  key->accelerator_key = keyval;
  key->accelerator_mods = modifiers;
  
  g_hash_table_replace (new_grabbed_keys, key, key);
}

static void
gtk_plug_keys_changed (GtkWindow *window)
{
  GHashTable *new_grabbed_keys, *old_grabbed_keys;
  GtkPlug *plug = GTK_PLUG (window);

  new_grabbed_keys = g_hash_table_new_full (grabbed_key_hash, grabbed_key_equal, (GDestroyNotify)g_free, NULL);
  _gtk_window_keys_foreach (window, keys_foreach, new_grabbed_keys);

  if (plug->socket_window)
    g_hash_table_foreach (new_grabbed_keys, add_grabbed_key, plug);

  old_grabbed_keys = plug->grabbed_keys;
  plug->grabbed_keys = new_grabbed_keys;

  if (old_grabbed_keys)
    {
      if (plug->socket_window)
	g_hash_table_foreach (old_grabbed_keys, remove_grabbed_key, plug);
      g_hash_table_destroy (old_grabbed_keys);
    }
}

static void
focus_to_parent (GtkPlug          *plug,
		 GtkDirectionType  direction)
{
  XEmbedMessageType message = XEMBED_FOCUS_PREV; /* Quiet GCC */
  
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
  
  _gtk_xembed_send_focus_message (plug->socket_window, message, 0);
}

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
	}
    }
  else
    {
      /* Try to focus the first widget in the window */
      if (bin->child && gtk_widget_child_focus (bin->child, direction))
        return TRUE;
    }

  if (!GTK_CONTAINER (window)->focus_child)
    focus_to_parent (plug, direction);

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
      gtk_window_set_screen (GTK_WINDOW (plug->modality_window),
			     gtk_widget_get_screen (GTK_WIDGET (plug)));
      gtk_widget_realize (plug->modality_window);
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
xembed_set_info (GdkWindow     *window,
		 unsigned long  flags)
{
  GdkDisplay *display = gdk_drawable_get_display (window);
  unsigned long buffer[2];

  Atom xembed_info_atom = gdk_x11_get_xatom_by_name_for_display (display, "_XEMBED_INFO");

  buffer[0] = GTK_XEMBED_PROTOCOL_VERSION;
  buffer[1] = flags;

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
		   GDK_WINDOW_XWINDOW (window),
		   xembed_info_atom, xembed_info_atom, 32,
		   PropModeReplace,
		   (unsigned char *)buffer, 2);
}

static void
handle_xembed_message (GtkPlug           *plug,
		       XEmbedMessageType  message,
		       glong              detail,
		       glong              data1,
		       glong              data2,
		       guint32            time)
{
  GtkWindow *window = GTK_WINDOW (plug);

  GTK_NOTE (PLUGSOCKET,
	    g_message ("GtkPlug: Message of type %d received", message));
  
  switch (message)
    {
    case XEMBED_EMBEDDED_NOTIFY:
      break;
    case XEMBED_WINDOW_ACTIVATE:
      _gtk_window_set_is_active (window, TRUE);
      break;
    case XEMBED_WINDOW_DEACTIVATE:
      _gtk_window_set_is_active (window, FALSE);
      break;
      
    case XEMBED_MODALITY_ON:
      handle_modality_on (plug);
      break;
    case XEMBED_MODALITY_OFF:
      handle_modality_off (plug);
      break;

    case XEMBED_FOCUS_IN:
      _gtk_window_set_has_toplevel_focus (window, TRUE);
      switch (detail)
	{
	case XEMBED_FOCUS_FIRST:
	  focus_first_last (plug, GTK_DIR_TAB_FORWARD);
	  break;
	case XEMBED_FOCUS_LAST:
	  focus_first_last (plug, GTK_DIR_TAB_BACKWARD);
	  break;
	case XEMBED_FOCUS_CURRENT:
	  break;
	}
      break;

    case XEMBED_FOCUS_OUT:
      _gtk_window_set_has_toplevel_focus (window, FALSE);
      break;
      
    case XEMBED_GRAB_KEY:
    case XEMBED_UNGRAB_KEY:
    case XEMBED_GTK_GRAB_KEY:
    case XEMBED_GTK_UNGRAB_KEY:
    case XEMBED_REQUEST_FOCUS:
    case XEMBED_FOCUS_NEXT:
    case XEMBED_FOCUS_PREV:
      g_warning ("GtkPlug: Invalid _XEMBED message of type %d received", message);
      break;
      
    default:
      GTK_NOTE(PLUGSOCKET,
	       g_message ("GtkPlug: Ignoring unknown _XEMBED message of type %d", message));
      break;
    }
}

static GdkFilterReturn
gtk_plug_filter_func (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
  GdkScreen *screen = gdk_drawable_get_screen (event->any.window);
  GdkDisplay *display = gdk_screen_get_display (screen);
  GtkPlug *plug = GTK_PLUG (data);
  XEvent *xevent = (XEvent *)gdk_xevent;

  GdkFilterReturn return_val;
  
  return_val = GDK_FILTER_CONTINUE;

  switch (xevent->type)
    {
    case ClientMessage:
      if (xevent->xclient.message_type == gdk_x11_get_xatom_by_name_for_display (display, "_XEMBED"))
	{
	  _gtk_xembed_push_message (xevent);
	  handle_xembed_message (plug,
				 xevent->xclient.data.l[1],
				 xevent->xclient.data.l[2],
				 xevent->xclient.data.l[3],
				 xevent->xclient.data.l[4],
				 xevent->xclient.data.l[0]);
	  _gtk_xembed_pop_message ();
				 
	  return GDK_FILTER_REMOVE;
	}
      else if (xevent->xclient.message_type == gdk_x11_get_xatom_by_name_for_display (display, "WM_DELETE_WINDOW"))
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
		g_object_unref (plug->socket_window);
		plug->socket_window = NULL;

		/* Emit a delete window, as if the user attempted
		 * to close the toplevel. Simple as to how we
		 * handle WM_DELETE_WINDOW, if it isn't handled
		 * we destroy the widget. BUt only do this if
		 * we are being reparented to the root window.
		 * Moving from one embedder to another should
		 * be invisible to the app.
		 */

		if (xre->parent == GDK_WINDOW_XWINDOW (gdk_screen_get_root_window (screen)))
		  send_delete_event (widget);
	      }
	    else
	      goto done;
	  }

	if (xre->parent != GDK_WINDOW_XWINDOW (gdk_screen_get_root_window (screen)))
	  {
	    /* Start of embedding protocol */

	    plug->socket_window = gdk_window_lookup_for_display (display, xre->parent);
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
		plug->socket_window = gdk_window_foreign_new_for_display (display, xre->parent);
		if (!plug->socket_window) /* Already gone */
		  break;
	      }

	    if (plug->grabbed_keys)
	      g_hash_table_foreach (plug->grabbed_keys, add_grabbed_key_always, plug);

	    if (!was_embedded)
	      g_signal_emit (plug, plug_signals[EMBEDDED], 0);
	  }

      done:
	g_object_unref (plug);
	
	break;
      }
    }

  return GDK_FILTER_CONTINUE;
}
