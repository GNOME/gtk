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
#include "gtkmain.h"
#include "gtkmenuitem.h"
#include "gtkmenushell.h"
#include "gtksignal.h"


#define MENU_SHELL_TIMEOUT   500
#define MENU_SHELL_CLASS(w)  GTK_MENU_SHELL_CLASS (GTK_OBJECT (w)->klass)


enum {
  DEACTIVATE,
  LAST_SIGNAL
};


static void gtk_menu_shell_class_init        (GtkMenuShellClass *klass);
static void gtk_menu_shell_init              (GtkMenuShell      *menu_shell);
static void gtk_menu_shell_map               (GtkWidget         *widget);
static void gtk_menu_shell_realize           (GtkWidget         *widget);
static gint gtk_menu_shell_button_press      (GtkWidget         *widget,
					      GdkEventButton    *event);
static gint gtk_menu_shell_button_release    (GtkWidget         *widget,
					      GdkEventButton    *event);
static gint gtk_menu_shell_enter_notify      (GtkWidget         *widget,
					      GdkEventCrossing  *event);
static gint gtk_menu_shell_leave_notify      (GtkWidget         *widget,
					      GdkEventCrossing  *event);
static void gtk_menu_shell_add               (GtkContainer      *container,
					      GtkWidget         *widget);
static void gtk_menu_shell_remove            (GtkContainer      *container,
					      GtkWidget         *widget);
static void gtk_menu_shell_foreach           (GtkContainer      *container,
					      GtkCallback        callback,
					      gpointer           callback_data);
static void gtk_real_menu_shell_deactivate   (GtkMenuShell      *menu_shell);
static gint gtk_menu_shell_is_item           (GtkMenuShell      *menu_shell,
					      GtkWidget         *child);


static GtkContainerClass *parent_class = NULL;
static guint menu_shell_signals[LAST_SIGNAL] = { 0 };


guint
gtk_menu_shell_get_type ()
{
  static guint menu_shell_type = 0;

  if (!menu_shell_type)
    {
      GtkTypeInfo menu_shell_info =
      {
	"GtkMenuShell",
	sizeof (GtkMenuShell),
	sizeof (GtkMenuShellClass),
	(GtkClassInitFunc) gtk_menu_shell_class_init,
	(GtkObjectInitFunc) gtk_menu_shell_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      menu_shell_type = gtk_type_unique (gtk_container_get_type (), &menu_shell_info);
    }

  return menu_shell_type;
}

static void
gtk_menu_shell_class_init (GtkMenuShellClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;

  parent_class = gtk_type_class (gtk_container_get_type ());

  menu_shell_signals[DEACTIVATE] =
    gtk_signal_new ("deactivate",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkMenuShellClass, deactivate),
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, menu_shell_signals, LAST_SIGNAL);

  widget_class->map = gtk_menu_shell_map;
  widget_class->realize = gtk_menu_shell_realize;
  widget_class->button_press_event = gtk_menu_shell_button_press;
  widget_class->button_release_event = gtk_menu_shell_button_release;
  widget_class->enter_notify_event = gtk_menu_shell_enter_notify;
  widget_class->leave_notify_event = gtk_menu_shell_leave_notify;

  container_class->add = gtk_menu_shell_add;
  container_class->remove = gtk_menu_shell_remove;
  container_class->foreach = gtk_menu_shell_foreach;

  klass->submenu_placement = GTK_TOP_BOTTOM;
  klass->deactivate = gtk_real_menu_shell_deactivate;
}

static void
gtk_menu_shell_init (GtkMenuShell *menu_shell)
{
  menu_shell->children = NULL;
  menu_shell->active_menu_item = NULL;
  menu_shell->parent_menu_shell = NULL;
  menu_shell->active = FALSE;
  menu_shell->have_grab = FALSE;
  menu_shell->have_xgrab = FALSE;
  menu_shell->ignore_leave = FALSE;
  menu_shell->button = 0;
  menu_shell->menu_flag = 0;
  menu_shell->activate_time = 0;
}

void
gtk_menu_shell_append (GtkMenuShell *menu_shell,
		       GtkWidget    *child)
{
  gtk_menu_shell_insert (menu_shell, child, -1);
}

void
gtk_menu_shell_prepend (GtkMenuShell *menu_shell,
			GtkWidget    *child)
{
  gtk_menu_shell_insert (menu_shell, child, 0);
}

void
gtk_menu_shell_insert (GtkMenuShell *menu_shell,
		       GtkWidget    *child,
		       gint          position)
{
  GList *tmp_list;
  GList *new_list;
  gint nchildren;

  g_return_if_fail (menu_shell != NULL);
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (child));

  gtk_widget_set_parent (child, GTK_WIDGET (menu_shell));

  if (GTK_WIDGET_VISIBLE (child->parent))
    {
      if (GTK_WIDGET_REALIZED (child->parent) &&
	  !GTK_WIDGET_REALIZED (child))
	gtk_widget_realize (child);

      if (GTK_WIDGET_MAPPED (child->parent) &&
	  !GTK_WIDGET_MAPPED (child))
	gtk_widget_map (child);
    }

  nchildren = g_list_length (menu_shell->children);
  if ((position < 0) || (position > nchildren))
    position = nchildren;

  if (position == nchildren)
    {
      menu_shell->children = g_list_append (menu_shell->children, child);
    }
  else
    {
      tmp_list = g_list_nth (menu_shell->children, position);
      new_list = g_list_alloc ();
      new_list->data = child;

      if (tmp_list->prev)
	tmp_list->prev->next = new_list;
      new_list->next = tmp_list;
      new_list->prev = tmp_list->prev;
      tmp_list->prev = new_list;

      if (tmp_list == menu_shell->children)
	menu_shell->children = new_list;
    }

  if (GTK_WIDGET_VISIBLE (menu_shell))
    gtk_widget_queue_resize (GTK_WIDGET (menu_shell));
}

void
gtk_menu_shell_deactivate (GtkMenuShell *menu_shell)
{
  g_return_if_fail (menu_shell != NULL);
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

  gtk_signal_emit (GTK_OBJECT (menu_shell), menu_shell_signals[DEACTIVATE]);
}

static void
gtk_menu_shell_map (GtkWidget *widget)
{
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_SHELL (widget));

  menu_shell = GTK_MENU_SHELL (widget);
  GTK_WIDGET_SET_FLAGS (menu_shell, GTK_MAPPED);
  gdk_window_show (widget->window);

  children = menu_shell->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child) && !GTK_WIDGET_MAPPED (child))
	gtk_widget_map (child);
    }
}

static void
gtk_menu_shell_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_SHELL (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static gint
gtk_menu_shell_button_press (GtkWidget      *widget,
			     GdkEventButton *event)
{
  GtkMenuShell *menu_shell;
  GtkWidget *menu_item;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU_SHELL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->type != GDK_BUTTON_PRESS)
    return FALSE;

  menu_shell = GTK_MENU_SHELL (widget);

  if (menu_shell->parent_menu_shell)
    {
      gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent*) event);
    }
  else if (!menu_shell->active || !menu_shell->button)
    {
      if (!menu_shell->active)
	{
	  gtk_grab_add (GTK_WIDGET (widget));
	  GTK_WIDGET_SET_FLAGS (widget, GTK_EXCLUSIVE_GRAB);
	  menu_shell->have_grab = TRUE;
	  menu_shell->active = TRUE;
	}
      menu_shell->button = event->button;
      
      menu_item = gtk_get_event_widget ((GdkEvent*) event);

      if (!GTK_WIDGET_IS_SENSITIVE (menu_item))
	return TRUE;

      if (menu_item && GTK_IS_MENU_ITEM (menu_item) &&
	  gtk_menu_shell_is_item (menu_shell, menu_item))
	{
	  if ((menu_item->parent == widget) &&
	      (menu_item != menu_shell->active_menu_item))
	    {
	      if (menu_shell->active_menu_item)
		gtk_menu_item_deselect (GTK_MENU_ITEM (menu_shell->active_menu_item));
	      
	      menu_shell->active_menu_item = menu_item;
	      gtk_menu_item_set_placement (GTK_MENU_ITEM (menu_shell->active_menu_item),
					   MENU_SHELL_CLASS (menu_shell)->submenu_placement);
	      gtk_menu_item_select (GTK_MENU_ITEM (menu_shell->active_menu_item));
	    }
	}
    }
  else
    {
      widget = gtk_get_event_widget ((GdkEvent*) event);
      if (widget == GTK_WIDGET (menu_shell))
	gtk_menu_shell_deactivate (menu_shell);
    }

  return TRUE;
}

static gint
gtk_menu_shell_button_release (GtkWidget      *widget,
			       GdkEventButton *event)
{
  GtkMenuShell *menu_shell;
  GtkWidget *menu_item;
  gint deactivate;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU_SHELL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  menu_shell = GTK_MENU_SHELL (widget);
  if (menu_shell->active)
    {
      if (menu_shell->button && (event->button != menu_shell->button))
	{
	  menu_shell->button = 0;
	  if (menu_shell->parent_menu_shell)
	    gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent*) event);
	  return TRUE;
	}
      
      menu_shell->button = 0;
      menu_item = gtk_get_event_widget ((GdkEvent*) event);
      
      if (!GTK_WIDGET_IS_SENSITIVE (menu_item))
	return TRUE;

      deactivate = TRUE;

      if ((event->time - menu_shell->activate_time) > MENU_SHELL_TIMEOUT)
	{
	  if (menu_shell->active_menu_item == menu_item)
	    {
	      if (GTK_MENU_ITEM (menu_item)->submenu == NULL)
		{
		  gtk_menu_shell_deactivate (menu_shell);
		  gtk_widget_activate (menu_item);
		  return TRUE;
		}
	    }
	  else if (menu_shell->parent_menu_shell)
	    {
	      menu_shell->active = TRUE;
	      gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent*) event);
	      return TRUE;
	    }
	}
      else
	deactivate = FALSE;

      if ((!deactivate || (menu_shell->active_menu_item == menu_item)) &&
	  (gdk_pointer_grab (widget->window, TRUE,
			     GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			     GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK,
			     NULL, NULL, event->time) == 0))
	{
	  deactivate = FALSE;
	  menu_shell->have_xgrab = TRUE;
	  menu_shell->ignore_leave = TRUE;
	}
      else
	deactivate = TRUE;

      if (deactivate)
	gtk_menu_shell_deactivate (menu_shell);
    }

  return TRUE;
}

static gint
gtk_menu_shell_enter_notify (GtkWidget        *widget,
			     GdkEventCrossing *event)
{
  GtkMenuShell *menu_shell;
  GtkWidget *menu_item;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU_SHELL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  menu_shell = GTK_MENU_SHELL (widget);
  if (menu_shell->active)
    {
      menu_item = gtk_get_event_widget ((GdkEvent*) event);

      if (!menu_item || !GTK_WIDGET_IS_SENSITIVE (menu_item))
	return TRUE;

      if ((menu_item->parent == widget) &&
	  (menu_shell->active_menu_item != menu_item) &&
	  GTK_IS_MENU_ITEM (menu_item))
	{
	  if ((event->detail != GDK_NOTIFY_INFERIOR) &&
	      (GTK_WIDGET_STATE (menu_item) != GTK_STATE_PRELIGHT))
	    {
	      if (menu_shell->active_menu_item)
		gtk_menu_item_deselect (GTK_MENU_ITEM (menu_shell->active_menu_item));

	      menu_shell->active_menu_item = menu_item;
	      gtk_menu_item_set_placement (GTK_MENU_ITEM (menu_shell->active_menu_item),
					   MENU_SHELL_CLASS (menu_shell)->submenu_placement);
	      gtk_menu_item_select (GTK_MENU_ITEM (menu_shell->active_menu_item));
	      if (GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu)
		gtk_widget_activate (menu_shell->active_menu_item);
	    }
	}
      else if (menu_shell->parent_menu_shell)
	{
	  gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent*) event);
	}
    }

  return TRUE;
}

static gint
gtk_menu_shell_leave_notify (GtkWidget        *widget,
			     GdkEventCrossing *event)
{
  GtkMenuShell *menu_shell;
  GtkMenuItem *menu_item;
  GtkWidget *event_widget;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU_SHELL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_VISIBLE (widget))
    {
      menu_shell = GTK_MENU_SHELL (widget);
      event_widget = gtk_get_event_widget ((GdkEvent*) event);

      if (!event_widget || !GTK_IS_MENU_ITEM (event_widget))
	return TRUE;

      menu_item = GTK_MENU_ITEM (event_widget);

      if (!GTK_WIDGET_IS_SENSITIVE (menu_item))
	return TRUE;

      if (menu_shell->ignore_leave)
	{
	  menu_shell->ignore_leave = FALSE;
	  return TRUE;
	}

      if ((menu_shell->active_menu_item == event_widget) &&
	  (menu_item->submenu == NULL))
	{
	  if ((event->detail != GDK_NOTIFY_INFERIOR) &&
	      (GTK_WIDGET_STATE (menu_item) != GTK_STATE_NORMAL))
	    {
	      gtk_menu_item_deselect (GTK_MENU_ITEM (menu_shell->active_menu_item));
	      menu_shell->active_menu_item = NULL;
	    }
	}
      else if (menu_shell->parent_menu_shell)
	{
	  gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent*) event);
	}
    }

  return TRUE;
}

static void
gtk_menu_shell_add (GtkContainer *container,
		    GtkWidget    *widget)
{
  gtk_menu_shell_append (GTK_MENU_SHELL (container), widget);
}

static void
gtk_menu_shell_remove (GtkContainer *container,
		       GtkWidget    *widget)
{
  GtkMenuShell *menu_shell;
  gint was_visible;
  
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_MENU_SHELL (container));
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  
  was_visible = GTK_WIDGET_VISIBLE (widget);
  menu_shell = GTK_MENU_SHELL (container);
  menu_shell->children = g_list_remove (menu_shell->children, widget);
  
  gtk_widget_unparent (widget);
  
  if (was_visible && GTK_WIDGET_VISIBLE (container))
    gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
gtk_menu_shell_foreach (GtkContainer *container,
			GtkCallback   callback,
			gpointer      callback_data)
{
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GList *children;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_MENU_SHELL (container));
  g_return_if_fail (callback != NULL);

  menu_shell = GTK_MENU_SHELL (container);

  children = menu_shell->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      (* callback) (child, callback_data);
    }
}


static void
gtk_real_menu_shell_deactivate (GtkMenuShell *menu_shell)
{
  g_return_if_fail (menu_shell != NULL);
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

  if (menu_shell->active)
    {
      menu_shell->button = 0;
      menu_shell->active = FALSE;

      if (menu_shell->active_menu_item)
	{
	  gtk_menu_item_deselect (GTK_MENU_ITEM (menu_shell->active_menu_item));
	  menu_shell->active_menu_item = NULL;
	}

      if (menu_shell->have_grab)
	{
	  menu_shell->have_grab = FALSE;
	  gtk_grab_remove (GTK_WIDGET (menu_shell));
	  GTK_WIDGET_UNSET_FLAGS (menu_shell, GTK_EXCLUSIVE_GRAB);
	}
      if (menu_shell->have_xgrab)
	{
	  menu_shell->have_xgrab = FALSE;
	  gdk_pointer_ungrab (GDK_CURRENT_TIME);
	}
    }
}

static gint
gtk_menu_shell_is_item (GtkMenuShell *menu_shell,
			GtkWidget    *child)
{
  GtkWidget *parent;

  g_return_val_if_fail (menu_shell != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU_SHELL (menu_shell), FALSE);
  g_return_val_if_fail (child != NULL, FALSE);

  parent = child->parent;
  while (parent && GTK_IS_MENU_SHELL (parent))
    {
      if (parent == (GtkWidget*) menu_shell)
	return TRUE;
      parent = GTK_MENU_SHELL (parent)->parent_menu_shell;
    }

  return FALSE;
}
