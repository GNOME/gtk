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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <string.h>
#include <limits.h>
#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"
#include "gdk/gdkx.h"
#include "gtkprivate.h"
#include "gtkrc.h"
#include "gtksignal.h"
#include "gtkwindow.h"
#include "gtkbindings.h"
#include "gtkmain.h"

enum {
  SET_FOCUS,
  LAST_SIGNAL
};
enum {
  ARG_0,
  ARG_TYPE,
  ARG_TITLE,
  ARG_AUTO_SHRINK,
  ARG_ALLOW_SHRINK,
  ARG_ALLOW_GROW,
  ARG_WIN_POS
};

static void gtk_window_class_init         (GtkWindowClass    *klass);
static void gtk_window_init               (GtkWindow         *window);
static void gtk_window_set_arg            (GtkObject         *object,
					   GtkArg            *arg,
					   guint	      arg_id);
static void gtk_window_get_arg            (GtkObject         *object,
					   GtkArg            *arg,
					   guint	      arg_id);
static void gtk_window_shutdown           (GtkObject         *object);
static void gtk_window_destroy            (GtkObject         *object);
static void gtk_window_finalize           (GtkObject         *object);
static void gtk_window_show               (GtkWidget         *widget);
static void gtk_window_hide               (GtkWidget         *widget);
static void gtk_window_map                (GtkWidget         *widget);
static void gtk_window_unmap              (GtkWidget         *widget);
static void gtk_window_realize            (GtkWidget         *widget);
static void gtk_window_size_request       (GtkWidget         *widget,
					   GtkRequisition    *requisition);
static void gtk_window_size_allocate      (GtkWidget         *widget,
					   GtkAllocation     *allocation);
static gint gtk_window_expose_event       (GtkWidget         *widget,
					   GdkEventExpose    *event);
static gint gtk_window_configure_event    (GtkWidget         *widget,
					   GdkEventConfigure *event);
static gint gtk_window_key_press_event    (GtkWidget         *widget,
					   GdkEventKey       *event);
static gint gtk_window_key_release_event  (GtkWidget         *widget,
					   GdkEventKey       *event);
static gint gtk_window_enter_notify_event (GtkWidget         *widget,
					   GdkEventCrossing  *event);
static gint gtk_window_leave_notify_event (GtkWidget         *widget,
					   GdkEventCrossing  *event);
static gint gtk_window_focus_in_event     (GtkWidget         *widget,
					   GdkEventFocus     *event);
static gint gtk_window_focus_out_event    (GtkWidget         *widget,
					   GdkEventFocus     *event);
static gint gtk_window_client_event	  (GtkWidget	     *widget,
					   GdkEventClient    *event);
static void gtk_window_check_resize       (GtkContainer      *container);
static void gtk_real_window_set_focus     (GtkWindow         *window,
					   GtkWidget         *focus);
static void gtk_window_move_resize        (GtkWindow         *window);
static void gtk_window_set_hints          (GtkWidget         *widget,
					   GtkRequisition    *requisition);

static void gtk_window_read_rcfiles       (GtkWidget         *widget,
					   GdkEventClient    *event);
static void gtk_window_draw               (GtkWidget         *widget,
				           GdkRectangle      *area);
static gint gtk_window_expose             (GtkWidget         *widget,
				           GdkEventExpose    *event);
static void gtk_window_style_set          (GtkWidget         *widget,
				           GtkStyle          *previous_style);

static GtkBinClass *parent_class = NULL;
static guint window_signals[LAST_SIGNAL] = { 0 };


GtkType
gtk_window_get_type (void)
{
  static GtkType window_type = 0;

  if (!window_type)
    {
      GtkTypeInfo window_info =
      {
	"GtkWindow",
	sizeof (GtkWindow),
	sizeof (GtkWindowClass),
	(GtkClassInitFunc) gtk_window_class_init,
	(GtkObjectInitFunc) gtk_window_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      window_type = gtk_type_unique (gtk_bin_get_type (), &window_info);
    }

  return window_type;
}

static void
gtk_window_class_init (GtkWindowClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;

  parent_class = gtk_type_class (gtk_bin_get_type ());

  gtk_object_add_arg_type ("GtkWindow::type", GTK_TYPE_WINDOW_TYPE, GTK_ARG_READWRITE, ARG_TYPE);
  gtk_object_add_arg_type ("GtkWindow::title", GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_TITLE);
  gtk_object_add_arg_type ("GtkWindow::auto_shrink", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_AUTO_SHRINK);
  gtk_object_add_arg_type ("GtkWindow::allow_shrink", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_ALLOW_SHRINK);
  gtk_object_add_arg_type ("GtkWindow::allow_grow", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_ALLOW_GROW);
  gtk_object_add_arg_type ("GtkWindow::window_position", GTK_TYPE_WINDOW_POSITION, GTK_ARG_READWRITE, ARG_WIN_POS);

  window_signals[SET_FOCUS] =
    gtk_signal_new ("set_focus",
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkWindowClass, set_focus),
                    gtk_marshal_NONE__POINTER,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_POINTER);

  gtk_object_class_add_signals (object_class, window_signals, LAST_SIGNAL);

  object_class->set_arg = gtk_window_set_arg;
  object_class->get_arg = gtk_window_get_arg;
  object_class->shutdown = gtk_window_shutdown;
  object_class->destroy = gtk_window_destroy;
  object_class->finalize = gtk_window_finalize;

  widget_class->show = gtk_window_show;
  widget_class->hide = gtk_window_hide;
  widget_class->map = gtk_window_map;
  widget_class->unmap = gtk_window_unmap;
  widget_class->realize = gtk_window_realize;
  widget_class->size_request = gtk_window_size_request;
  widget_class->size_allocate = gtk_window_size_allocate;
  widget_class->expose_event = gtk_window_expose_event;
  widget_class->configure_event = gtk_window_configure_event;
  widget_class->key_press_event = gtk_window_key_press_event;
  widget_class->key_release_event = gtk_window_key_release_event;
  widget_class->enter_notify_event = gtk_window_enter_notify_event;
  widget_class->leave_notify_event = gtk_window_leave_notify_event;
  widget_class->focus_in_event = gtk_window_focus_in_event;
  widget_class->focus_out_event = gtk_window_focus_out_event;
  widget_class->client_event = gtk_window_client_event;
  widget_class->style_set = gtk_window_style_set;

  widget_class->draw = gtk_window_draw;
  widget_class->expose_event = gtk_window_expose;
   
  container_class->check_resize = gtk_window_check_resize;

  klass->set_focus = gtk_real_window_set_focus;
}

static void
gtk_window_init (GtkWindow *window)
{
  GTK_WIDGET_UNSET_FLAGS (window, GTK_NO_WINDOW);
  GTK_WIDGET_SET_FLAGS (window, GTK_TOPLEVEL);

  gtk_container_set_resize_mode (GTK_CONTAINER (window), GTK_RESIZE_QUEUE);

  window->title = NULL;
  window->wmclass_name = g_strdup (g_get_prgname ());
  window->wmclass_class = g_strdup (gdk_progclass);
  window->type = GTK_WINDOW_TOPLEVEL;
  window->focus_widget = NULL;
  window->default_widget = NULL;
  window->resize_count = 0;
  window->allow_shrink = FALSE;
  window->allow_grow = TRUE;
  window->auto_shrink = FALSE;
  window->handling_resize = FALSE;
  window->position = GTK_WIN_POS_NONE;
  window->use_uposition = TRUE;
  window->modal = FALSE;
  
  gtk_container_register_toplevel (GTK_CONTAINER (window));
}

static void
gtk_window_set_arg (GtkObject  *object,
		    GtkArg     *arg,
		    guint	arg_id)
{
  GtkWindow  *window;

  window = GTK_WINDOW (object);

  switch (arg_id)
    {
    case ARG_TYPE:
      window->type = GTK_VALUE_ENUM (*arg);
      break;
    case ARG_TITLE:
      gtk_window_set_title (window, GTK_VALUE_STRING (*arg));
      break;
    case ARG_AUTO_SHRINK:
      window->auto_shrink = (GTK_VALUE_BOOL (*arg) != FALSE);
      gtk_window_set_hints (GTK_WIDGET (window), &GTK_WIDGET (window)->requisition);
      break;
    case ARG_ALLOW_SHRINK:
      window->allow_shrink = (GTK_VALUE_BOOL (*arg) != FALSE);
      gtk_window_set_hints (GTK_WIDGET (window), &GTK_WIDGET (window)->requisition);
      break;
    case ARG_ALLOW_GROW:
      window->allow_grow = (GTK_VALUE_BOOL (*arg) != FALSE);
      gtk_window_set_hints (GTK_WIDGET (window), &GTK_WIDGET (window)->requisition);
      break;
    case ARG_WIN_POS:
      gtk_window_position (window, GTK_VALUE_ENUM (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_window_get_arg (GtkObject  *object,
		    GtkArg     *arg,
		    guint	arg_id)
{
  GtkWindow  *window;

  window = GTK_WINDOW (object);

  switch (arg_id)
    {
    case ARG_TYPE:
      GTK_VALUE_ENUM (*arg) = window->type;
      break;
    case ARG_TITLE:
      GTK_VALUE_STRING (*arg) = g_strdup (window->title);
      break;
    case ARG_AUTO_SHRINK:
      GTK_VALUE_BOOL (*arg) = window->auto_shrink;
      break;
    case ARG_ALLOW_SHRINK:
      GTK_VALUE_BOOL (*arg) = window->allow_shrink;
      break;
    case ARG_ALLOW_GROW:
      GTK_VALUE_BOOL (*arg) = window->allow_grow;
      break;
    case ARG_WIN_POS:
      GTK_VALUE_ENUM (*arg) = window->position;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

GtkWidget*
gtk_window_new (GtkWindowType type)
{
  GtkWindow *window;

  window = gtk_type_new (gtk_window_get_type ());

  window->type = type;

  return GTK_WIDGET (window);
}

void
gtk_window_set_title (GtkWindow   *window,
		      const gchar *title)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (window->title)
    g_free (window->title);
  window->title = g_strdup (title);

  if (GTK_WIDGET_REALIZED (window))
    gdk_window_set_title (GTK_WIDGET (window)->window, window->title);
}

void
gtk_window_set_wmclass (GtkWindow *window,
			const gchar *wmclass_name,
			const gchar *wmclass_class)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  g_free (window->wmclass_name);
  window->wmclass_name = g_strdup (wmclass_name);

  g_free (window->wmclass_class);
  window->wmclass_class = g_strdup (wmclass_class);

  if (GTK_WIDGET_REALIZED (window))
    g_warning ("shouldn't set wmclass after window is realized!\n");
}

void
gtk_window_set_focus (GtkWindow *window,
		      GtkWidget *focus)
{
  gtk_signal_emit (GTK_OBJECT (window), window_signals[SET_FOCUS], focus);
}

void
gtk_window_set_default (GtkWindow *window,
			GtkWidget *default_widget)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (default_widget)
    g_return_if_fail (GTK_WIDGET_CAN_DEFAULT (default_widget));

  if (window->default_widget != default_widget)
    {
      if (window->default_widget)
	{
	  GTK_WIDGET_UNSET_FLAGS (window->default_widget, GTK_HAS_DEFAULT);
	  gtk_widget_draw_default (window->default_widget);
	}

      window->default_widget = default_widget;

      if (window->default_widget)
	{
	  GTK_WIDGET_SET_FLAGS (window->default_widget, GTK_HAS_DEFAULT);
	  gtk_widget_draw_default (window->default_widget);
	}
    }
}

void
gtk_window_set_policy (GtkWindow *window,
		       gint       allow_shrink,
		       gint       allow_grow,
		       gint       auto_shrink)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  window->allow_shrink = (allow_shrink != FALSE);
  window->allow_grow = (allow_grow != FALSE);
  window->auto_shrink = (auto_shrink != FALSE);

  gtk_window_set_hints (GTK_WIDGET (window), &GTK_WIDGET (window)->requisition);
}

void
gtk_window_add_accel_group (GtkWindow        *window,
			    GtkAccelGroup    *accel_group)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (accel_group != NULL);

  gtk_accel_group_attach (accel_group, GTK_OBJECT (window));
}

void
gtk_window_remove_accel_group (GtkWindow       *window,
			       GtkAccelGroup   *accel_group)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (accel_group != NULL);

  gtk_accel_group_detach (accel_group, GTK_OBJECT (window));
}

void
gtk_window_position (GtkWindow         *window,
		     GtkWindowPosition  position)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  window->position = position;
}

gint
gtk_window_activate_focus (GtkWindow      *window)
{
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  if (window->focus_widget)
    {
      gtk_widget_activate (window->focus_widget);
      return TRUE;
    }

  return FALSE;
}

gint
gtk_window_activate_default (GtkWindow      *window)
{
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

  if (window->default_widget)
    {
      gtk_widget_activate (window->default_widget);
      return TRUE;
    }

  return FALSE;
}

void
gtk_window_set_modal (GtkWindow *window, gboolean modal)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  /* If the widget was showed already, adjust it's grab state */
  if (GTK_WIDGET_VISIBLE(GTK_WIDGET(window)))
    {
      if (window->modal && !modal)
	gtk_grab_remove (GTK_WIDGET(window));
      else if (!window->modal && modal)
	gtk_grab_add (GTK_WIDGET(window));
    }
  
  window->modal = modal;
}

static void
gtk_window_shutdown (GtkObject *object)
{
  GtkWindow *window;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_WINDOW (object));

  window = GTK_WINDOW (object);

  gtk_window_set_focus (window, NULL);
  gtk_window_set_default (window, NULL);

  GTK_OBJECT_CLASS (parent_class)->shutdown (object);
}

static void
gtk_window_destroy (GtkObject *object)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_WINDOW (object));

  gtk_container_unregister_toplevel (GTK_CONTAINER (object));

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtk_window_finalize (GtkObject *object)
{
  GtkWindow *window;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_WINDOW (object));

  window = GTK_WINDOW (object);
  g_free (window->title);
  g_free (window->wmclass_name);
  g_free (window->wmclass_class);

  GTK_OBJECT_CLASS(parent_class)->finalize (object);
}

static void
gtk_window_show (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_VISIBLE);
  gtk_container_check_resize (GTK_CONTAINER (widget));
  gtk_widget_map (widget);

  if (GTK_WINDOW(widget)->modal)
      gtk_grab_add(widget);
      
}

static void
gtk_window_hide (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_VISIBLE);
  gtk_widget_unmap (widget);

  if (GTK_WINDOW(widget)->modal)
      gtk_grab_remove(widget);

}

static void
gtk_window_map (GtkWidget *widget)
{
  GtkWindow *window;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

  window = GTK_WINDOW (widget);

  if (window->bin.child &&
      GTK_WIDGET_VISIBLE (window->bin.child) &&
      !GTK_WIDGET_MAPPED (window->bin.child))
    gtk_widget_map (window->bin.child);

  gtk_window_set_hints (widget, &widget->requisition);
  gdk_window_show (widget->window);
}

static void
gtk_window_unmap (GtkWidget *widget)
{
  GtkWindow *window;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  gdk_window_hide (widget->window);

  window = GTK_WINDOW (widget);
  window->use_uposition = TRUE;
}

static void
gtk_window_realize (GtkWidget *widget)
{
  GtkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));
  
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  window = GTK_WINDOW (widget);
  
  switch (window->type)
    {
    case GTK_WINDOW_TOPLEVEL:
      attributes.window_type = GDK_WINDOW_TOPLEVEL;
      break;
    case GTK_WINDOW_DIALOG:
      attributes.window_type = GDK_WINDOW_DIALOG;
      break;
    case GTK_WINDOW_POPUP:
      attributes.window_type = GDK_WINDOW_TEMP;
      break;
    }
   
  attributes.title = window->title;
  attributes.wmclass_name = window->wmclass_name;
  attributes.wmclass_class = window->wmclass_class;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
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
   
  widget->window = gdk_window_new (NULL, &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, window);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  gtk_window_paint (widget, NULL);
}

static void
gtk_window_size_request (GtkWidget      *widget,
			 GtkRequisition *requisition)
{
  GtkWindow *window;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));
  g_return_if_fail (requisition != NULL);

  window = GTK_WINDOW (widget);

  if (window->bin.child)
    {
      requisition->width = GTK_CONTAINER (window)->border_width * 2;
      requisition->height = GTK_CONTAINER (window)->border_width * 2;

      gtk_widget_size_request (window->bin.child, &window->bin.child->requisition);

      requisition->width += window->bin.child->requisition.width;
      requisition->height += window->bin.child->requisition.height;
    }
  else
    {
      if (!GTK_WIDGET_VISIBLE (window))
	GTK_CONTAINER (window)->need_resize = TRUE;
    }
}

static void
gtk_window_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkWindow *window;
  GtkAllocation child_allocation;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));
  g_return_if_fail (allocation != NULL);

  window = GTK_WINDOW (widget);
  widget->allocation = *allocation;

  if (window->bin.child && GTK_WIDGET_VISIBLE (window->bin.child))
    {
      child_allocation.x = GTK_CONTAINER (window)->border_width;
      child_allocation.y = GTK_CONTAINER (window)->border_width;
      child_allocation.width = allocation->width - child_allocation.x * 2;
      child_allocation.height = allocation->height - child_allocation.y * 2;

      gtk_widget_size_allocate (window->bin.child, &child_allocation);
    }
}

static gint
gtk_window_expose_event (GtkWidget      *widget,
			 GdkEventExpose *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    if (GTK_WIDGET_CLASS (parent_class)->expose_event)
      return (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
  
  return FALSE;
}

static gint
gtk_window_configure_event (GtkWidget         *widget,
			    GdkEventConfigure *event)
{
  GtkWindow *window;
  GtkAllocation allocation;
  gboolean need_expose = FALSE;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  window = GTK_WINDOW (widget);

  /* If the window was merely moved, do nothing */
  if ((widget->allocation.width == event->width) &&
      (widget->allocation.height == event->height))
    {
      if (window->resize_count == 0)      /* The window was merely moved */
	return FALSE;
      else
	{
	  /* We asked for a new size, which was rejected, so the
	   * WM sent us a synthetic configure event. We won't
	   * get the expose event we would normally get (since
	   * we have ForgetGravity), so we need to fake it.
	   */
	  need_expose = TRUE;
	}
    }
	
  
  window->handling_resize = TRUE;
  
  allocation.x = 0;
  allocation.y = 0;
  allocation.width = event->width;
  allocation.height = event->height;
  
  gtk_widget_size_allocate (widget, &allocation);
  
  if (window->bin.child &&
      GTK_WIDGET_VISIBLE (window->bin.child) &&
      !GTK_WIDGET_MAPPED (window->bin.child))
    gtk_widget_map (window->bin.child);
  
  if (window->resize_count > 0)
      window->resize_count -= 1;
  
  if (need_expose)
    {
      GdkEvent temp_event;
      temp_event.type = GDK_EXPOSE;
      temp_event.expose.window = widget->window;
      temp_event.expose.send_event = TRUE;
      temp_event.expose.area.x = 0;
      temp_event.expose.area.y = 0;
      temp_event.expose.area.width = event->width;
      temp_event.expose.area.height = event->height;
      temp_event.expose.count = 0;
      
      gtk_widget_event (widget, &temp_event);
    }

  window->handling_resize = FALSE;
  
  return FALSE;
}

static gint
gtk_window_key_press_event (GtkWidget   *widget,
			    GdkEventKey *event)
{
  GtkWindow *window;
  GtkDirectionType direction = 0;
  gboolean handled;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  window = GTK_WINDOW (widget);

  handled = FALSE;
  
  if (window->focus_widget)
    {
      handled = gtk_widget_event (window->focus_widget, (GdkEvent*) event);
    }
    
  if (!handled)
    handled = gtk_accel_groups_activate (GTK_OBJECT (window), event->keyval, event->state);

  if (!handled)
    {
      switch (event->keyval)
	{
	case GDK_space:
	  if (window->focus_widget)
	    {
	      gtk_widget_activate (window->focus_widget);
	      handled = TRUE;
	    }
	  break;
	case GDK_Return:
	case GDK_KP_Enter:
	  if (window->default_widget)
	    {
	      gtk_widget_activate (window->default_widget);
	      handled = TRUE;
	    }
          else if (window->focus_widget)
	    {
	      gtk_widget_activate (window->focus_widget);
	      handled = TRUE;
	    }
	  break;
	case GDK_Up:
	case GDK_Down:
	case GDK_Left:
	case GDK_Right:
	case GDK_Tab:
	case GDK_ISO_Left_Tab:
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
	    case GDK_ISO_Left_Tab:
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
	    gtk_window_set_focus (GTK_WINDOW (widget), NULL);
	  else
	    handled = TRUE;
	  break;
	}
    }

  if (!handled && GTK_WIDGET_CLASS (parent_class)->key_press_event)
    handled = GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event);

  return handled;
}

static gint
gtk_window_key_release_event (GtkWidget   *widget,
			      GdkEventKey *event)
{
  GtkWindow *window;
  gint handled;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  window = GTK_WINDOW (widget);
  handled = FALSE;
  if (window->focus_widget)
    {
      handled = gtk_widget_event (window->focus_widget, (GdkEvent*) event);
    }

  if (!handled && GTK_WIDGET_CLASS (parent_class)->key_release_event)
    handled = GTK_WIDGET_CLASS (parent_class)->key_release_event (widget, event);

  return handled;
}

static gint
gtk_window_enter_notify_event (GtkWidget        *widget,
			       GdkEventCrossing *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  return FALSE;
}

static gint
gtk_window_leave_notify_event (GtkWidget        *widget,
			       GdkEventCrossing *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  return FALSE;
}

static gint
gtk_window_focus_in_event (GtkWidget     *widget,
			   GdkEventFocus *event)
{
  GtkWindow *window;
  GdkEventFocus fevent;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  /* It appears spurious focus in events can occur when
   *  the window is hidden. So we'll just check to see if
   *  the window is visible before actually handling the
   *  event
   */
  if (GTK_WIDGET_VISIBLE (widget))
    {
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
gtk_window_focus_out_event (GtkWidget     *widget,
			    GdkEventFocus *event)
{
  GtkWindow *window;
  GdkEventFocus fevent;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

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

static GdkAtom atom_rcfiles = GDK_NONE;

static void
gtk_window_read_rcfiles (GtkWidget *widget,
			 GdkEventClient *event)
{
  GList *embedded_windows;

  embedded_windows = gtk_object_get_data (GTK_OBJECT (widget), "gtk-embedded");
  if (embedded_windows)
    {
      GdkEventClient sev;
      int i;
      
      for(i = 0; i < 5; i++)
	sev.data.l[i] = 0;
      sev.data_format = 32;
      sev.message_type = atom_rcfiles;
      
      while (embedded_windows)
	{
	  guint xid = GPOINTER_TO_UINT (embedded_windows->data);
	  gdk_event_send_client_message ((GdkEvent *) &sev, xid);
	  embedded_windows = embedded_windows->next;
	}
    }

  if (gtk_rc_reparse_all ())
    {
      /* If the above returned true, some of our RC files are out
       * of date, so we need to reset all our widgets. Our other
       * toplevel windows will also get the message, but by
       * then, the RC file will up to date, so we have to tell
       * them now.
       */
      GList *toplevels;
      
      toplevels = gtk_container_get_toplevels();
      while (toplevels)
	{
	  gtk_widget_reset_rc_styles (toplevels->data);
	  toplevels = toplevels->next;
	}
    }
}

static gint
gtk_window_client_event (GtkWidget	*widget,
			 GdkEventClient	*event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (!atom_rcfiles)
    atom_rcfiles = gdk_atom_intern("_GTK_READ_RCFILES", FALSE);

  if(event->message_type == atom_rcfiles) 
    gtk_window_read_rcfiles (widget, event);    

  return FALSE;
}

static void
gtk_window_check_resize (GtkContainer *container)
{
  GtkWindow *window;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_WINDOW (container));

  window = GTK_WINDOW (container);
  if (!window->handling_resize)
    {
      if (GTK_WIDGET_VISIBLE (container))
	gtk_window_move_resize (window);
      else
	GTK_CONTAINER (window)->need_resize = TRUE;
    }
}

/* FIXME: we leave container->resize_widgets set under some
   circumstances ? */
static void
gtk_window_move_resize (GtkWindow *window)
{
  GtkWidget    *widget;
  GtkContainer *container;
  gint x, y;
  gint width, height;
  gint screen_width;
  gint screen_height;
  gboolean needed_resize;
  gboolean size_changed;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  widget = GTK_WIDGET (window);
  container = GTK_CONTAINER (widget);

  /* Remember old size, to know if we have to reset hints */
  width = widget->requisition.width;
  height = widget->requisition.height;
  gtk_widget_size_request (widget, &widget->requisition);

  size_changed = ((width != widget->requisition.width) ||
		  (height != widget->requisition.height));
  
  if (size_changed)
    {
      gboolean saved_use_upos;

      saved_use_upos = window->use_uposition;
      gtk_window_set_hints (widget, &widget->requisition);
      window->use_uposition = saved_use_upos;
    }
  
  x = -1;
  y = -1;
  width = widget->requisition.width;
  height = widget->requisition.height;
  
  if (window->use_uposition)
    switch (window->position)
      {
      case GTK_WIN_POS_CENTER:
	x = (gdk_screen_width () - width) / 2;
	y = (gdk_screen_height () - height) / 2;
	gtk_widget_set_uposition (widget, x, y);
	break;
      case GTK_WIN_POS_MOUSE:
	gdk_window_get_pointer (NULL, &x, &y, NULL);
	
	x -= width / 2;
	y -= height / 2;
	
	screen_width = gdk_screen_width ();
	screen_height = gdk_screen_height ();
	
	if (x < 0)
	  x = 0;
	else if (x > (screen_width - width))
	  x = screen_width - width;
	
	if (y < 0)
	  y = 0;
	else if (y > (screen_height - height))
	  y = screen_height - height;
	
	gtk_widget_set_uposition (widget, x, y);
	break;
      }

  /* Now, do the resizing */

  needed_resize = container->need_resize;
  container->need_resize = FALSE;

  if ((widget->requisition.width == 0) ||
      (widget->requisition.height == 0))
    {
      widget->requisition.width = 200;
      widget->requisition.height = 200;
    }
  
  if (!GTK_WIDGET_REALIZED (window))
    {
      GtkAllocation allocation;

      allocation.x = 0;
      allocation.y = 0;
      allocation.width = widget->requisition.width;
      allocation.height = widget->requisition.height;
      
      gtk_widget_size_allocate (widget, &allocation);

      return;
    }
  
  gdk_window_get_geometry (widget->window, NULL, NULL, &width, &height, NULL);
  
  /* As an optimization, we don't try to get a new size from the
   * window manager if we asked for the same size last time and
   * didn't get it */

  if (size_changed && 
      (((window->auto_shrink &&
	((width != widget->requisition.width) ||
	 (height != widget->requisition.height)))) ||
       ((width < widget->requisition.width) ||
	(height < widget->requisition.height))))
    {
      window->resize_count += 1;
      if ((x != -1) && (y != -1))
	gdk_window_move_resize (widget->window, x, y,
				widget->requisition.width,
				widget->requisition.height);
      else
        gdk_window_resize (widget->window,
			   widget->requisition.width,
			   widget->requisition.height);
    }
  else if (needed_resize)
    {
      /* The windows contents changed size while it was not
       * visible, so reallocate everything, since we didn't
       * keep track of what changed
       */
      GtkAllocation allocation;
      
      allocation.x = 0;
      allocation.y = 0;
      allocation.width = widget->requisition.width;
      allocation.height = widget->requisition.height;
      
      gtk_widget_size_allocate (widget, &allocation);
    }
  else
    {
      if ((x != -1) && (y != -1))
	gdk_window_move (widget->window, x, y);
      
      gtk_container_resize_children (GTK_CONTAINER (window));
    }
}

static void
gtk_real_window_set_focus (GtkWindow *window,
			   GtkWidget *focus)
{
  GdkEventFocus event;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (focus && !GTK_WIDGET_CAN_FOCUS (focus))
    return;

  if (window->focus_widget != focus)
    {
      if (window->focus_widget)
	{
	  event.type = GDK_FOCUS_CHANGE;
	  event.window = window->focus_widget->window;
	  event.in = FALSE;

	  gtk_widget_event (window->focus_widget, (GdkEvent*) &event);
	}

      window->focus_widget = focus;

      if (window->focus_widget)
	{
	  event.type = GDK_FOCUS_CHANGE;
	  event.window = window->focus_widget->window;
	  event.in = TRUE;

	  gtk_widget_event (window->focus_widget, (GdkEvent*) &event);
	}
    }
}

static void
gtk_window_set_hints (GtkWidget      *widget,
		      GtkRequisition *requisition)
{
  GtkWindow *window;
  GtkWidgetAuxInfo *aux_info;
  gint flags;
  gint ux, uy;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));
  g_return_if_fail (requisition != NULL);

  if (GTK_WIDGET_REALIZED (widget))
    {
      window = GTK_WINDOW (widget);

      flags = 0;
      ux = 0;
      uy = 0;

      aux_info = gtk_object_get_data (GTK_OBJECT (widget), "gtk-aux-info");
      if (aux_info && (aux_info->x != -1) && (aux_info->y != -1))
	{
	  ux = aux_info->x;
	  uy = aux_info->y;
	  flags |= GDK_HINT_POS;
	}

      if (!window->allow_shrink)
	flags |= GDK_HINT_MIN_SIZE;
      if (!window->allow_grow)
	flags |= GDK_HINT_MAX_SIZE;

      gdk_window_set_hints (widget->window, ux, uy,
			    requisition->width, requisition->height,
			    requisition->width, requisition->height,
			    flags);

      if (window->use_uposition && (flags & GDK_HINT_POS))
	{
	  window->use_uposition = FALSE;
	  gdk_window_move (widget->window, ux, uy);
	}
    }
}

static void
gtk_window_paint (GtkWidget     *widget,
		  GdkRectangle *area)
{
  gtk_paint_flat_box (widget->style, widget->window, GTK_STATE_NORMAL, 
		      GTK_SHADOW_NONE, area, widget, "base", 0, 0, -1, -1);
}

static gint
gtk_window_expose (GtkWidget      *widget,
		   GdkEventExpose *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  gtk_window_paint (widget, &event->area);
  
  if (GTK_WIDGET_CLASS (parent_class)->expose_event)
    return (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);

  return FALSE;
}

static void
gtk_window_draw (GtkWidget    *widget,
		 GdkRectangle *area)
{
  gtk_window_paint (widget, area);
  
  if (GTK_WIDGET_CLASS (parent_class)->draw)
    (* GTK_WIDGET_CLASS (parent_class)->draw) (widget, area);
}

static void
gtk_window_style_set (GtkWidget *widget,
		      GtkStyle  *previous_style)
{
   GdkRectangle area;
   
   if (GTK_WIDGET_REALIZED (widget) &&
       !GTK_WIDGET_NO_WINDOW (widget))
     {
	gtk_style_set_background (widget->style, widget->window, widget->state);

	area.x = 0;
	area.y = 0;
	area.width = widget->allocation.width;
	area.height = widget->allocation.height;
	gtk_window_draw(widget, &area);

	if (GTK_WIDGET_DRAWABLE (widget))
	  gdk_window_clear (widget->window);
     }
}
