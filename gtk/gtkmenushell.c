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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gdk/gdkkeysyms.h"
#include "gtkbindings.h"
#include "gtkmain.h"
#include "gtkmenuitem.h"
#include "gtktearoffmenuitem.h" /* FIXME */
#include "gtkmenushell.h"
#include "gtksignal.h"


#define MENU_SHELL_TIMEOUT   500
#define MENU_SHELL_CLASS(w)  GTK_MENU_SHELL_CLASS (GTK_OBJECT (w)->klass)


enum {
  DEACTIVATE,
  SELECTION_DONE,
  MOVE_CURRENT,
  ACTIVATE_CURRENT,
  CANCEL,
  LAST_SIGNAL
};

typedef void (*GtkMenuShellSignal1) (GtkObject           *object,
				     GtkMenuDirectionType arg1,
				     gpointer             data);
typedef void (*GtkMenuShellSignal2) (GtkObject *object,
				     gboolean   arg1,
				     gpointer   data);

/* Terminology:
 * 
 * A menu item can be "selected", this means that it is displayed
 * in the prelight state, and if it has a submenu, that submenu
 * will be popped up. 
 * 
 * A menu is "active" when it is visible onscreen and the user
 * is selecting from it. A menubar is not active until the user
 * clicks on one of its menuitems. When a menu is active,
 * passing the mouse over a submenu will pop it up.
 *
 * menu_shell->active_menu_item, is however, not an "active"
 * menu item (there is no such thing) but rather, the selected
 * menu item in that MenuShell, if there is one.
 *
 * There is also is a concept of the current menu and a current
 * menu item. The current menu item is the selected menu item
 * that is furthest down in the heirarchy. (Every active menu_shell
 * does not necessarily contain a selected menu item, but if
 * it does, then menu_shell->parent_menu_shell must also contain
 * a selected menu item. The current menu is the menu that 
 * contains the current menu_item. It will always have a GTK
 * grab and receive all key presses.
 *
 *
 * Action signals:
 *
 *  ::move_current (GtkMenuDirection *dir)
 *     Moves the current menu item in direction 'dir':
 *
 *       GTK_MENU_DIR_PARENT: To the parent menu shell
 *       GTK_MENU_DIR_CHILD: To the child menu shell (if this item has
 *          a submenu.
 *       GTK_MENU_DIR_NEXT/PREV: To the next or previous item
 *          in this menu.
 * 
 *     As a a bit of a hack to get movement between menus and
 *     menubars working, if submenu_placement is different for
 *     the menu and its MenuShell then the following apply:
 * 
 *       - For 'parent' the current menu is not just moved to
 *         the parent, but moved to the previous entry in the parent
 *       - For 'child', if there is no child, then current is
 *         moved to the next item in the parent.
 *
 * 
 *  ::activate_current (GBoolean *force_hide)
 *     Activate the current item. If 'force_hide' is true, hide
 *     the current menu item always. Otherwise, only hide
 *     it if menu_item->klass->hide_on_activate is true.
 *
 *  ::cancel ()
 *     Cancels the current selection
 */

static void gtk_menu_shell_class_init        (GtkMenuShellClass *klass);
static void gtk_menu_shell_init              (GtkMenuShell      *menu_shell);
static void gtk_menu_shell_map               (GtkWidget         *widget);
static void gtk_menu_shell_realize           (GtkWidget         *widget);
static gint gtk_menu_shell_button_press      (GtkWidget         *widget,
					      GdkEventButton    *event);
static gint gtk_menu_shell_button_release    (GtkWidget         *widget,
					      GdkEventButton    *event);
static gint gtk_menu_shell_key_press         (GtkWidget	        *widget,
					      GdkEventKey       *event);
static gint gtk_menu_shell_enter_notify      (GtkWidget         *widget,
					      GdkEventCrossing  *event);
static gint gtk_menu_shell_leave_notify      (GtkWidget         *widget,
					      GdkEventCrossing  *event);
static void gtk_menu_shell_add               (GtkContainer      *container,
					      GtkWidget         *widget);
static void gtk_menu_shell_remove            (GtkContainer      *container,
					      GtkWidget         *widget);
static void gtk_menu_shell_forall            (GtkContainer      *container,
					      gboolean		 include_internals,
					      GtkCallback        callback,
					      gpointer           callback_data);
static void gtk_real_menu_shell_deactivate   (GtkMenuShell      *menu_shell);
static gint gtk_menu_shell_is_item           (GtkMenuShell      *menu_shell,
					      GtkWidget         *child);
static GtkWidget *gtk_menu_shell_get_item    (GtkMenuShell      *menu_shell,
					      GdkEvent          *event);
static GtkType    gtk_menu_shell_child_type  (GtkContainer      *container);
static void gtk_menu_shell_select_submenu_first (GtkMenuShell   *menu_shell); 

static void gtk_real_menu_shell_move_current (GtkMenuShell      *menu_shell,
					      GtkMenuDirectionType direction);
static void gtk_real_menu_shell_activate_current (GtkMenuShell      *menu_shell,
						  gboolean           force_hide);
static void gtk_real_menu_shell_cancel           (GtkMenuShell      *menu_shell);

static GtkContainerClass *parent_class = NULL;
static guint menu_shell_signals[LAST_SIGNAL] = { 0 };


GtkType
gtk_menu_shell_get_type (void)
{
  static GtkType menu_shell_type = 0;

  if (!menu_shell_type)
    {
      static const GtkTypeInfo menu_shell_info =
      {
	"GtkMenuShell",
	sizeof (GtkMenuShell),
	sizeof (GtkMenuShellClass),
	(GtkClassInitFunc) gtk_menu_shell_class_init,
	(GtkObjectInitFunc) gtk_menu_shell_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
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

  GtkBindingSet *binding_set;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;

  parent_class = gtk_type_class (gtk_container_get_type ());

  menu_shell_signals[DEACTIVATE] =
    gtk_signal_new ("deactivate",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkMenuShellClass, deactivate),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  menu_shell_signals[SELECTION_DONE] =
    gtk_signal_new ("selection-done",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkMenuShellClass, selection_done),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  menu_shell_signals[MOVE_CURRENT] =
    gtk_signal_new ("move_current",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkMenuShellClass, move_current),
		    gtk_marshal_NONE__ENUM,
		    GTK_TYPE_NONE, 1, 
		    GTK_TYPE_MENU_DIRECTION_TYPE);
  menu_shell_signals[ACTIVATE_CURRENT] =
    gtk_signal_new ("activate_current",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkMenuShellClass, activate_current),
		    gtk_marshal_NONE__BOOL,
		    GTK_TYPE_NONE, 1, 
		    GTK_TYPE_BOOL);
  menu_shell_signals[CANCEL] =
    gtk_signal_new ("cancel",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkMenuShellClass, cancel),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  
  gtk_object_class_add_signals (object_class, menu_shell_signals, LAST_SIGNAL);

  widget_class->map = gtk_menu_shell_map;
  widget_class->realize = gtk_menu_shell_realize;
  widget_class->button_press_event = gtk_menu_shell_button_press;
  widget_class->button_release_event = gtk_menu_shell_button_release;
  widget_class->key_press_event = gtk_menu_shell_key_press;
  widget_class->enter_notify_event = gtk_menu_shell_enter_notify;
  widget_class->leave_notify_event = gtk_menu_shell_leave_notify;

  container_class->add = gtk_menu_shell_add;
  container_class->remove = gtk_menu_shell_remove;
  container_class->forall = gtk_menu_shell_forall;
  container_class->child_type = gtk_menu_shell_child_type;

  klass->submenu_placement = GTK_TOP_BOTTOM;
  klass->deactivate = gtk_real_menu_shell_deactivate;
  klass->selection_done = NULL;
  klass->move_current = gtk_real_menu_shell_move_current;
  klass->activate_current = gtk_real_menu_shell_activate_current;
  klass->cancel = gtk_real_menu_shell_cancel;

  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Escape, 0,
				"cancel", 0);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Return, 0,
				"activate_current", 1,
				GTK_TYPE_BOOL,
				TRUE);
  gtk_binding_entry_add_signal (binding_set,
				GDK_space, 0,
				"activate_current", 1,
				GTK_TYPE_BOOL,
				FALSE);
}

static GtkType
gtk_menu_shell_child_type (GtkContainer     *container)
{
  return GTK_TYPE_MENU_ITEM;
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
  g_return_if_fail (menu_shell != NULL);
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (child));

  menu_shell->children = g_list_insert (menu_shell->children, child, position);

  gtk_widget_set_parent (child, GTK_WIDGET (menu_shell));

  if (GTK_WIDGET_REALIZED (child->parent))
    gtk_widget_realize (child);

  if (GTK_WIDGET_VISIBLE (child->parent) && GTK_WIDGET_VISIBLE (child))
    {
      if (GTK_WIDGET_MAPPED (child->parent))
	gtk_widget_map (child);

      gtk_widget_queue_resize (child);
    }
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

  children = menu_shell->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child) && !GTK_WIDGET_MAPPED (child))
	gtk_widget_map (child);
    }

  gdk_window_show (widget->window);
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
			    GDK_KEY_PRESS_MASK |
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
	  menu_shell->have_grab = TRUE;
	  menu_shell->active = TRUE;
	}
      menu_shell->button = event->button;

      menu_item = gtk_menu_shell_get_item (menu_shell, (GdkEvent *)event);

      if (menu_item &&
	  GTK_WIDGET_IS_SENSITIVE (menu_item))
	{
	  if ((menu_item->parent == widget) &&
	      (menu_item != menu_shell->active_menu_item))
	    gtk_menu_shell_select_item (menu_shell, menu_item);
	}
    }
  else
    {
      widget = gtk_get_event_widget ((GdkEvent*) event);
      if (widget == GTK_WIDGET (menu_shell))
	{
	  gtk_menu_shell_deactivate (menu_shell);
	  gtk_signal_emit (GTK_OBJECT (menu_shell), menu_shell_signals[SELECTION_DONE]);
	}
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
      menu_item = gtk_menu_shell_get_item (menu_shell, (GdkEvent*) event);

      deactivate = TRUE;

      if ((event->time - menu_shell->activate_time) > MENU_SHELL_TIMEOUT)
	{
	  if (menu_item && (menu_shell->active_menu_item == menu_item) &&
	      GTK_WIDGET_IS_SENSITIVE (menu_item))
	    {
	      if (GTK_MENU_ITEM (menu_item)->submenu == NULL)
		{
		  gtk_menu_shell_activate_item (menu_shell, menu_item, TRUE);
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
	{
	  /* We only ever want to prevent deactivation on the first
           * press/release. Setting the time to zero is a bit of a
	   * hack, since we could be being triggered in the first
	   * few fractions of a second after a server time wraparound.
	   * the chances of that happening are ~1/10^6, without
	   * serious harm if we lose.
	   */
	  menu_shell->activate_time = 0;
	  deactivate = FALSE;
	}
      
      /* If the button click was very fast, or we ended up on a submenu,
       * leave the menu up
       */
      if (!deactivate || 
	  (menu_item && (menu_shell->active_menu_item == menu_item)))
	{
	  deactivate = FALSE;
	  menu_shell->ignore_leave = TRUE;
	}
      else
	deactivate = TRUE;

      if (deactivate)
	{
	  gtk_menu_shell_deactivate (menu_shell);
	  gtk_signal_emit (GTK_OBJECT (menu_shell), menu_shell_signals[SELECTION_DONE]);
	}
    }

  return TRUE;
}

static gint
gtk_menu_shell_key_press (GtkWidget	*widget,
			  GdkEventKey *event)
{
  GtkMenuShell *menu_shell;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU_SHELL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
      
  menu_shell = GTK_MENU_SHELL (widget);

  if (!menu_shell->active_menu_item && menu_shell->parent_menu_shell)
    return gtk_widget_event (menu_shell->parent_menu_shell, (GdkEvent *)event);
  
  if (gtk_bindings_activate (GTK_OBJECT (widget),
			     event->keyval,
			     event->state))
    return TRUE;

  if (gtk_accel_groups_activate (GTK_OBJECT (widget), event->keyval, event->state))
    return TRUE;

  return FALSE;
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
	  if (menu_shell->ignore_enter)
	    return TRUE;
	  
	  if ((event->detail != GDK_NOTIFY_INFERIOR) &&
	      (GTK_WIDGET_STATE (menu_item) != GTK_STATE_PRELIGHT))
	    {
	      gtk_menu_shell_select_item (menu_shell, menu_item);
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

      if (menu_shell->ignore_leave)
	{
	  menu_shell->ignore_leave = FALSE;
	  return TRUE;
	}

      if (!GTK_WIDGET_IS_SENSITIVE (menu_item))
	return TRUE;

      if ((menu_shell->active_menu_item == event_widget) &&
	  (menu_item->submenu == NULL))
	{
	  if ((event->detail != GDK_NOTIFY_INFERIOR) &&
	      (GTK_WIDGET_STATE (menu_item) != GTK_STATE_NORMAL))
	    {
	      gtk_menu_shell_deselect (menu_shell);
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
  
  if (widget == menu_shell->active_menu_item)
    {
      gtk_item_deselect (GTK_ITEM (menu_shell->active_menu_item));
      menu_shell->active_menu_item = NULL;
    }

  gtk_widget_unparent (widget);
  
  /* queue resize regardless of GTK_WIDGET_VISIBLE (container),
   * since that's what is needed by toplevels.
   */
  if (was_visible)
    gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
gtk_menu_shell_forall (GtkContainer *container,
		       gboolean      include_internals,
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
	}
      if (menu_shell->have_xgrab)
	{
	  menu_shell->have_xgrab = FALSE;
	  gdk_pointer_ungrab (GDK_CURRENT_TIME);
	  gdk_keyboard_ungrab (GDK_CURRENT_TIME);
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

static GtkWidget*
gtk_menu_shell_get_item (GtkMenuShell *menu_shell,
			 GdkEvent     *event)
{
  GtkWidget *menu_item;

  menu_item = gtk_get_event_widget ((GdkEvent*) event);
  
  while (menu_item && !GTK_IS_MENU_ITEM (menu_item))
    menu_item = menu_item->parent;

  if (menu_item && gtk_menu_shell_is_item (menu_shell, menu_item))
    return menu_item;
  else
    return NULL;
}

/* Handlers for action signals */

void
gtk_menu_shell_select_item (GtkMenuShell *menu_shell,
			    GtkWidget    *menu_item)
{
  g_return_if_fail (menu_shell != NULL);
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (menu_item != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  gtk_menu_shell_deselect (menu_shell);
  
  menu_shell->active_menu_item = menu_item;
  gtk_menu_item_set_placement (GTK_MENU_ITEM (menu_shell->active_menu_item),
			       MENU_SHELL_CLASS (menu_shell)->submenu_placement);
  gtk_menu_item_select (GTK_MENU_ITEM (menu_shell->active_menu_item));

  /* This allows the bizarre radio buttons-with-submenus-display-history
   * behavior
   */
  if (GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu)
    gtk_widget_activate (menu_shell->active_menu_item);
}

void
gtk_menu_shell_deselect (GtkMenuShell *menu_shell)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

  if (menu_shell->active_menu_item)
    {
      gtk_menu_item_deselect (GTK_MENU_ITEM (menu_shell->active_menu_item));
      menu_shell->active_menu_item = NULL;
    }
}

void
gtk_menu_shell_activate_item (GtkMenuShell      *menu_shell,
			      GtkWidget         *menu_item,
			      gboolean           force_deactivate)
{
  GSList *slist, *shells = NULL;
  gboolean deactivate = force_deactivate;

  g_return_if_fail (menu_shell != NULL);
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  g_return_if_fail (menu_item != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  if (!deactivate)
    deactivate = GTK_MENU_ITEM_CLASS (GTK_OBJECT (menu_item)->klass)->hide_on_activate;

  gtk_widget_ref (GTK_WIDGET (menu_shell));

  if (deactivate)
    {
      GtkMenuShell *parent_menu_shell = menu_shell;

      do
	{
	  gtk_widget_ref (GTK_WIDGET (parent_menu_shell));
	  shells = g_slist_prepend (shells, parent_menu_shell);
	  parent_menu_shell = (GtkMenuShell*) parent_menu_shell->parent_menu_shell;
	}
      while (parent_menu_shell);
      shells = g_slist_reverse (shells);

      gtk_menu_shell_deactivate (menu_shell);
  
      /* flush the x-queue, so any grabs are removed and
       * the menu is actually taken down
       */
      gdk_flush ();
    }

  gtk_widget_activate (menu_item);

  for (slist = shells; slist; slist = slist->next)
    {
      gtk_signal_emit (slist->data, menu_shell_signals[SELECTION_DONE]);
      gtk_widget_unref (slist->data);
    }
  g_slist_free (shells);

  gtk_widget_unref (GTK_WIDGET (menu_shell));
}

/* Distance should be +/- 1 */
static void
gtk_menu_shell_move_selected (GtkMenuShell  *menu_shell, 
			      gint           distance)
{
  if (menu_shell->active_menu_item)
    {
      GList *node = g_list_find (menu_shell->children,
				 menu_shell->active_menu_item);
      GList *start_node = node;
      
      if (distance > 0)
	{
	  node = node->next;
	  while (node != start_node && 
		 (!node ||
		  !GTK_WIDGET_IS_SENSITIVE (node->data) ||
		  !GTK_WIDGET_VISIBLE (node->data) ))
	    {
	      if (!node)
		node = menu_shell->children;
	      else
		node = node->next;
	    }
	}
      else
	{
	  node = node->prev;
	  while (node != start_node &&
		 (!node ||
		  !GTK_WIDGET_IS_SENSITIVE (node->data) ||
		  !GTK_WIDGET_VISIBLE (node->data) ))
	    {
	      if (!node)
		node = g_list_last (menu_shell->children);
	      else
		node = node->prev;
	    }
	}
      
      if (node)
	gtk_menu_shell_select_item (menu_shell, node->data);
    }
}

static void
gtk_menu_shell_select_submenu_first (GtkMenuShell     *menu_shell)
{
  GtkMenuItem *menu_item;

  menu_item = GTK_MENU_ITEM (menu_shell->active_menu_item); 
  
  if (menu_item->submenu)
    {
      GtkMenuShell *submenu = GTK_MENU_SHELL (menu_item->submenu); 
      if (submenu->children)
	gtk_menu_shell_select_item (submenu, submenu->children->data); 
    }
}

static void
gtk_real_menu_shell_move_current (GtkMenuShell      *menu_shell,
				  GtkMenuDirectionType direction)
{
  GtkMenuShell *parent_menu_shell = NULL;
  gboolean had_selection;

  had_selection = menu_shell->active_menu_item != NULL;

  if (menu_shell->parent_menu_shell)
    parent_menu_shell = GTK_MENU_SHELL (menu_shell->parent_menu_shell);
  
  switch (direction)
    {
    case GTK_MENU_DIR_PARENT:
      if (parent_menu_shell)
	{
	  if (GTK_MENU_SHELL_CLASS (GTK_OBJECT (parent_menu_shell)->klass)->submenu_placement == 
		       GTK_MENU_SHELL_CLASS (GTK_OBJECT (menu_shell)->klass)->submenu_placement)
	    gtk_menu_shell_deselect (menu_shell);
	  else 
	    {
	      gtk_menu_shell_move_selected (parent_menu_shell, -1);
	      gtk_menu_shell_select_submenu_first (parent_menu_shell); 
	    }
	}
      break;
      
    case GTK_MENU_DIR_CHILD:
      if (menu_shell->active_menu_item &&
	  GTK_BIN (menu_shell->active_menu_item)->child &&
	  GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu)
	{
	  menu_shell = GTK_MENU_SHELL (GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu);
	  if (menu_shell->children)
	    gtk_menu_shell_select_item (menu_shell, menu_shell->children->data);
	}
      else
	{
	  /* Try to find a menu running the opposite direction */
	  while (parent_menu_shell && 
		 (GTK_MENU_SHELL_CLASS (GTK_OBJECT (parent_menu_shell)->klass)->submenu_placement ==
		  GTK_MENU_SHELL_CLASS (GTK_OBJECT (menu_shell)->klass)->submenu_placement))
	    parent_menu_shell = GTK_MENU_SHELL (parent_menu_shell->parent_menu_shell);
	  
	  if (parent_menu_shell)
	    {
	      gtk_menu_shell_move_selected (parent_menu_shell, 1);
	      gtk_menu_shell_select_submenu_first (parent_menu_shell);
	    }
	}
      break;
      
    case GTK_MENU_DIR_PREV:
      gtk_menu_shell_move_selected (menu_shell, -1);
      if (!had_selection &&
	  !menu_shell->active_menu_item &&
	  menu_shell->children)
	gtk_menu_shell_select_item (menu_shell, g_list_last (menu_shell->children)->data);
      break;
    case GTK_MENU_DIR_NEXT:
      gtk_menu_shell_move_selected (menu_shell, 1);
      if (!had_selection &&
	  !menu_shell->active_menu_item &&
	  menu_shell->children)
	gtk_menu_shell_select_item (menu_shell, menu_shell->children->data);
      break;
    }
  
}

static void
gtk_real_menu_shell_activate_current (GtkMenuShell      *menu_shell,
				      gboolean           force_hide)
{
  if (menu_shell->active_menu_item &&
      GTK_WIDGET_IS_SENSITIVE (menu_shell->active_menu_item) &&
      GTK_MENU_ITEM (menu_shell->active_menu_item)->submenu == NULL)
    {
      gtk_menu_shell_activate_item (menu_shell,
				    menu_shell->active_menu_item,
				    force_hide);
    }
}

static void
gtk_real_menu_shell_cancel (GtkMenuShell      *menu_shell)
{
  /* Unset the active menu item so gtk_menu_popdown() doesn't see it.
   */
  gtk_menu_shell_deselect (menu_shell);
  
  gtk_menu_shell_deactivate (menu_shell);
  gtk_signal_emit (GTK_OBJECT (menu_shell), menu_shell_signals[SELECTION_DONE]);
}

