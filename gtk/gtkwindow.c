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
#include <string.h>
#include <limits.h>
#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"
#include "gdk/gdkx.h"
#include "gtkprivate.h"
#include "gtksignal.h"
#include "gtkwindow.h"

enum {
  MOVE_RESIZE,
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

typedef gint (*GtkWindowSignal1) (GtkObject *object,
				  gpointer   arg1,
				  gpointer   arg2,
				  gint       arg3,
				  gint       arg4,
				  gpointer   data);
typedef void (*GtkWindowSignal2) (GtkObject *object,
				  gpointer   arg1,
				  gpointer   data);

static void gtk_window_marshal_signal_1 (GtkObject      *object,
					 GtkSignalFunc   func,
					 gpointer        func_data,
					 GtkArg         *args);
static void gtk_window_marshal_signal_2 (GtkObject      *object,
					 GtkSignalFunc   func,
					 gpointer        func_data,
					 GtkArg         *args);
static void gtk_window_class_init         (GtkWindowClass    *klass);
static void gtk_window_init               (GtkWindow         *window);
static void gtk_window_set_arg            (GtkWindow         *window,
					   GtkArg            *arg,
					   guint	      arg_id);
static void gtk_window_get_arg            (GtkWindow         *window,
					   GtkArg            *arg,
					   guint	      arg_id);
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
static gint gtk_window_need_resize        (GtkContainer      *container);
static gint gtk_real_window_move_resize   (GtkWindow         *window,
					   gint              *x,
					   gint              *y,
					   gint               width,
					   gint               height);
static void gtk_real_window_set_focus     (GtkWindow         *window,
					   GtkWidget         *focus);
static gint gtk_window_move_resize        (GtkWidget         *widget);
static void gtk_window_set_hints          (GtkWidget         *widget,
					   GtkRequisition    *requisition);
static gint gtk_window_check_accelerator  (GtkWindow         *window,
					   gint               key,
					   guint              mods);


static GtkBinClass *parent_class = NULL;
static gint window_signals[LAST_SIGNAL] = { 0 };


guint
gtk_window_get_type ()
{
  static guint window_type = 0;

  if (!window_type)
    {
      GtkTypeInfo window_info =
      {
	"GtkWindow",
	sizeof (GtkWindow),
	sizeof (GtkWindowClass),
	(GtkClassInitFunc) gtk_window_class_init,
	(GtkObjectInitFunc) gtk_window_init,
	(GtkArgSetFunc) gtk_window_set_arg,
	(GtkArgGetFunc) gtk_window_get_arg,
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

  gtk_object_add_arg_type ("GtkWindow::type", GTK_TYPE_WINDOW_TYPE, ARG_TYPE);
  gtk_object_add_arg_type ("GtkWindow::title", GTK_TYPE_STRING, ARG_TITLE);
  gtk_object_add_arg_type ("GtkWindow::auto_shrink", GTK_TYPE_BOOL, ARG_AUTO_SHRINK);
  gtk_object_add_arg_type ("GtkWindow::allow_shrink", GTK_TYPE_BOOL, ARG_ALLOW_SHRINK);
  gtk_object_add_arg_type ("GtkWindow::allow_grow", GTK_TYPE_BOOL, ARG_ALLOW_GROW);
  gtk_object_add_arg_type ("GtkWindow::window_position", GTK_TYPE_ENUM, ARG_WIN_POS);

  window_signals[MOVE_RESIZE] =
    gtk_signal_new ("move_resize",
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkWindowClass, move_resize),
                    gtk_window_marshal_signal_1,
		    GTK_TYPE_BOOL, 4,
                    GTK_TYPE_POINTER, GTK_TYPE_POINTER,
                    GTK_TYPE_INT, GTK_TYPE_INT);

  window_signals[SET_FOCUS] =
    gtk_signal_new ("set_focus",
                    GTK_RUN_LAST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkWindowClass, set_focus),
                    gtk_window_marshal_signal_2,
		    GTK_TYPE_NONE, 1,
                    GTK_TYPE_POINTER);

  gtk_object_class_add_signals (object_class, window_signals, LAST_SIGNAL);

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

  container_class->need_resize = gtk_window_need_resize;

  klass->move_resize = gtk_real_window_move_resize;
  klass->set_focus = gtk_real_window_set_focus;
}

static void
gtk_window_init (GtkWindow *window)
{
  GTK_WIDGET_UNSET_FLAGS (window, GTK_NO_WINDOW);
  GTK_WIDGET_SET_FLAGS (window, GTK_TOPLEVEL);

  window->title = NULL;
  window->wmclass_name = NULL;
  window->wmclass_class = NULL;
  window->type = GTK_WINDOW_TOPLEVEL;
  window->accelerator_tables = NULL;
  window->focus_widget = NULL;
  window->default_widget = NULL;
  window->resize_count = 0;
  window->need_resize = FALSE;
  window->allow_shrink = FALSE;
  window->allow_grow = TRUE;
  window->auto_shrink = FALSE;
  window->handling_resize = FALSE;
  window->position = GTK_WIN_POS_NONE;
  window->use_uposition = TRUE;
  
  gtk_container_register_toplevel (GTK_CONTAINER (window));
}

static void
gtk_window_set_arg (GtkWindow  *window,
		    GtkArg     *arg,
		    guint	arg_id)
{
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
      break;
    case ARG_ALLOW_SHRINK:
      window->allow_shrink = (GTK_VALUE_BOOL (*arg) != FALSE);
      break;
    case ARG_ALLOW_GROW:
      window->allow_grow = (GTK_VALUE_BOOL (*arg) != FALSE);
      break;
    case ARG_WIN_POS:
      gtk_window_position (window, GTK_VALUE_ENUM (*arg));
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_window_get_arg (GtkWindow  *window,
		    GtkArg     *arg,
		    guint	arg_id)
{
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
			gchar     *wmclass_name,
			gchar     *wmclass_class)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (window->wmclass_name)
    g_free (window->wmclass_name);
  window->wmclass_name = g_strdup (wmclass_name);

  if (window->wmclass_class)
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
			GtkWidget *defaultw)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (GTK_WIDGET_CAN_DEFAULT (defaultw));

  if (window->default_widget != defaultw)
    {
      if (window->default_widget)
	{
	  GTK_WIDGET_UNSET_FLAGS (window->default_widget, GTK_HAS_DEFAULT);
	  gtk_widget_draw_default (window->default_widget);
	}

      window->default_widget = defaultw;

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
}

void
gtk_window_add_accelerator_table (GtkWindow           *window,
				  GtkAcceleratorTable *table)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  gtk_accelerator_table_ref (table);
  window->accelerator_tables = g_list_prepend (window->accelerator_tables,
					       table);
}

void
gtk_window_remove_accelerator_table (GtkWindow           *window,
				     GtkAcceleratorTable *table)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GTK_IS_WINDOW (window));

  window->accelerator_tables = g_list_remove (window->accelerator_tables,
					      table);
  gtk_accelerator_table_unref (table);
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

static void
gtk_window_marshal_signal_1 (GtkObject      *object,
			     GtkSignalFunc   func,
			     gpointer        func_data,
			     GtkArg         *args)
{
  GtkWindowSignal1 rfunc;
  gint *return_val;

  rfunc = (GtkWindowSignal1) func;
  return_val = GTK_RETLOC_BOOL (args[4]);

  *return_val = (* rfunc) (object,
			   GTK_VALUE_POINTER (args[0]),
			   GTK_VALUE_POINTER (args[1]),
			   GTK_VALUE_INT (args[2]),
			   GTK_VALUE_INT (args[3]),
			   func_data);
}

static void
gtk_window_marshal_signal_2 (GtkObject      *object,
			     GtkSignalFunc   func,
			     gpointer        func_data,
			     GtkArg         *args)
{
  GtkWindowSignal2 rfunc;

  rfunc = (GtkWindowSignal2) func;

  (* rfunc) (object, GTK_VALUE_POINTER (args[0]), func_data);
}

static void
gtk_window_destroy (GtkObject *object)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_WINDOW (object));

  gtk_container_unregister_toplevel (GTK_CONTAINER (object));

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_window_finalize (GtkObject *object)
{
  GtkWindow *window;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_WINDOW (object));

  window = GTK_WINDOW (object);
  g_free (window->title);

  GTK_OBJECT_CLASS(parent_class)->finalize (object);
}

static void
gtk_window_show (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_VISIBLE);
  gtk_container_need_resize (GTK_CONTAINER (widget));
  gtk_widget_map (widget);
}

static void
gtk_window_hide (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_VISIBLE);
  gtk_widget_unmap (widget);
}

static void
gtk_window_map (GtkWidget *widget)
{
  GtkWindow *window;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WINDOW (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

  gtk_window_move_resize (widget);
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
	window->need_resize = TRUE;
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

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  window = GTK_WINDOW (widget);
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

  window->resize_count -= 1;
  if (window->resize_count == 0)
    {
      if ((event->width != widget->requisition.width) ||
	  (event->height != widget->requisition.height))
	{
	  window->resize_count += 1;
	  gdk_window_resize (widget->window,
			     widget->requisition.width,
			     widget->requisition.height);
	}
    }
  else if (window->resize_count < 0)
    {
      window->resize_count = 0;
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
  gint return_val;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  window = GTK_WINDOW (widget);

  return_val = FALSE;
  if (window->focus_widget)
    return_val = gtk_widget_event (window->focus_widget, (GdkEvent*) event);

  if (!return_val && gtk_window_check_accelerator (window, event->keyval, event->state))
    return_val = TRUE;

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
	  if (window->default_widget)
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
	    gtk_window_set_focus (GTK_WINDOW (widget), NULL);
	  else
	    return_val = TRUE;
	  break;
	}
    }

  return return_val;
}

static gint
gtk_window_key_release_event (GtkWidget   *widget,
			      GdkEventKey *event)
{
  GtkWindow *window;
  gint return_val;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  window = GTK_WINDOW (widget);
  return_val = FALSE;
  if (window->focus_widget)
    return_val = gtk_widget_event (window->focus_widget, (GdkEvent*) event);

  return return_val;
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

static void
gtk_window_style_set_event (GtkWidget *widget,
			    GdkEventClient *event)
{
  GdkAtom atom_default_colors;
  GtkStyle *style_newdefault;
  GdkAtom realtype;
  gint retfmt, retlen;
  GdkColor *data, *stylecolors;
  int i = 0;
  GdkColormap *widget_cmap;
  
  atom_default_colors = gdk_atom_intern("_GTK_DEFAULT_COLORS", FALSE);
  
  if(gdk_property_get (GDK_ROOT_PARENT(),
		       atom_default_colors,
		       gdk_atom_intern("STRING", FALSE),
		       0,
		       sizeof(GdkColor) * GTK_STYLE_NUM_STYLECOLORS(),
		       FALSE,
		       &realtype,
		       &retfmt,
		       &retlen,
		       (guchar **)&data) != TRUE) {
    g_warning("gdk_property_get() failed in _GTK_STYLE_CHANGED handler\n");
    return;
  }
  if(retfmt != sizeof(gushort)*8) {
    g_warning("retfmt (%d) != sizeof(gushort)*8 (%d)\n", retfmt,
	sizeof(gushort)*8);
    return;
  }
  /* We have the color data, now let's interpret it */
  style_newdefault = gtk_widget_get_default_style();
  gtk_style_ref(style_newdefault);
  stylecolors = (GdkColor *) style_newdefault;

  widget_cmap = gtk_widget_get_colormap(widget);
  for(i = 0; i < GTK_STYLE_NUM_STYLECOLORS(); i++) {
    stylecolors[i] = data[i];
    gdk_color_alloc(widget_cmap, &stylecolors[i]);
  }

  gtk_widget_set_default_style(style_newdefault);
  gtk_style_unref(style_newdefault);

  /* Now we need to redraw everything */
  gtk_widget_draw(widget, NULL);
  gtk_widget_draw_children(widget);
}

static gint
gtk_window_client_event (GtkWidget	*widget,
			 GdkEventClient	*event)
{
  GdkAtom atom_styleset;
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  atom_styleset = gdk_atom_intern("_GTK_STYLE_CHANGED", FALSE);

  if(event->message_type == atom_styleset) {
    gtk_window_style_set_event(widget, event);    
  }
  return FALSE;
}

static gint
gtk_window_need_resize (GtkContainer *container)
{
  GtkWindow *window;
  gint return_val;

  g_return_val_if_fail (container != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (container), FALSE);

  return_val = FALSE;

  window = GTK_WINDOW (container);
  if (window->handling_resize)
    return return_val;

  if (GTK_WIDGET_VISIBLE (container))
    {
      window->need_resize = TRUE;
      return_val = gtk_window_move_resize (GTK_WIDGET (window));
      window->need_resize = FALSE;
    }

  return return_val;
}

static gint
gtk_real_window_move_resize (GtkWindow *window,
			     gint      *x,
			     gint      *y,
			     gint       width,
			     gint       height)
{
  GtkWidget *widget;
  GtkWidget *resize_container, *child;
  GSList *resize_containers;
  GSList *tmp_list;
  gint return_val;

  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail ((x != NULL) || (y != NULL), FALSE);

  return_val = FALSE;

  widget = GTK_WIDGET (window);

  if ((*x != -1) && (*y != -1))
    gdk_window_move (widget->window, *x, *y);

  if ((widget->requisition.width == 0) ||
      (widget->requisition.height == 0))
    {
      widget->requisition.width = 200;
      widget->requisition.height = 200;
    }

  gdk_window_get_geometry (widget->window, NULL, NULL, &width, &height, NULL);

  resize_containers = NULL;

  if ((window->auto_shrink &&
       ((width != widget->requisition.width) ||
	(height != widget->requisition.height))) ||
      (width < widget->requisition.width) ||
      (height < widget->requisition.height))
    {
      if (window->resize_count == 0)
	{
	  window->resize_count += 1;
	  gdk_window_resize (widget->window,
			     widget->requisition.width,
			     widget->requisition.height);
	}
    }
  else
    {
      /* The window hasn't changed size but one of its children
       *  queued a resize request. Which means that the allocation
       *  is not sufficient for the requisition of some child.
       *  We've already performed a size request at this point,
       *  so we simply need to run through the list of resize
       *  widgets and reallocate their sizes appropriately. We
       *  make the optimization of not performing reallocation
       *  for a widget who also has a parent in the resize widgets
       *  list. GTK_RESIZE_NEEDED is used for flagging those
       *  parents inside this function.
       */
      GSList *resize_widgets, *node;

      resize_widgets = GTK_CONTAINER (window)->resize_widgets;
      GTK_CONTAINER (window)->resize_widgets = NULL;

      for (node = resize_widgets; node; node = node->next)
	{
	  child = (GtkWidget *)node->data;

	  GTK_PRIVATE_UNSET_FLAG (child, GTK_RESIZE_NEEDED);

	  widget = child->parent;
	  while (widget &&
		 ((widget->allocation.width < widget->requisition.width) ||
		  (widget->allocation.height < widget->requisition.height)))
	    widget = widget->parent;

	  if (widget)
	    GTK_PRIVATE_SET_FLAG (widget, GTK_RESIZE_NEEDED);
	}

      for (node = resize_widgets; node; node = node->next)
	{
	  child = (GtkWidget *)node->data;

	  resize_container = child->parent;
	  while (resize_container &&
		 !GTK_WIDGET_RESIZE_NEEDED (resize_container))
	    resize_container = resize_container->parent;

	  if (resize_container)
	    widget = resize_container->parent;
	  else
	    widget = NULL;

	  while (widget)
	    {
	      if (GTK_WIDGET_RESIZE_NEEDED (widget))
		{
		  GTK_PRIVATE_UNSET_FLAG (resize_container, GTK_RESIZE_NEEDED);
		  resize_container = widget;
		}
	      widget = widget->parent;
	    }

	  if (resize_container &&
	      !g_slist_find (resize_containers, resize_container))
	    resize_containers = g_slist_prepend (resize_containers,
						 resize_container);
	}

      g_slist_free (resize_widgets);

      tmp_list = resize_containers;
      while (tmp_list)
       {
         widget = tmp_list->data;
         tmp_list = tmp_list->next;

	 GTK_PRIVATE_UNSET_FLAG (widget, GTK_RESIZE_NEEDED);
         gtk_widget_size_allocate (widget, &widget->allocation);
         gtk_widget_queue_draw (widget);
       }

      g_slist_free (resize_containers);
    }

  return return_val;
}

static gint
gtk_window_move_resize (GtkWidget *widget)
{
  GtkWindow *window;
  gint x, y;
  gint width, height;
  gint screen_width;
  gint screen_height;
  gint return_val;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);

  return_val = FALSE;

  if (GTK_WIDGET_REALIZED (widget))
    {
      window = GTK_WINDOW (widget);

      /* Remember old size, to know if we have to reset hints */
      width = widget->requisition.width;
      height = widget->requisition.height;
      gtk_widget_size_request (widget, &widget->requisition);

      if (GTK_WIDGET_MAPPED (widget) &&
	  (width != widget->requisition.width ||
	   height != widget->requisition.height))
	gtk_window_set_hints (widget, &widget->requisition);

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

      gtk_signal_emit (GTK_OBJECT (widget), window_signals[MOVE_RESIZE],
                       &x, &y, width, height, &return_val);
    }

  return return_val;
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

static gint
gtk_window_check_accelerator (GtkWindow *window,
			      gint       key,
			      guint      mods)
{
  GtkAcceleratorTable *table;
  GList *tmp;

  if ((key >= 0x20) && (key <= 0xFF))
    {
      tmp = window->accelerator_tables;
      while (tmp)
	{
	  table = tmp->data;
	  tmp = tmp->next;

	  if (gtk_accelerator_table_check (table, key, mods))
	    return TRUE;
	}

      if (gtk_accelerator_table_check (NULL, key, mods))
	return TRUE;
    }

  return FALSE;
}
