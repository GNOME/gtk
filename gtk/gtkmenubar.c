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
#include "gtkmenubar.h"
#include "gtkmenuitem.h"

enum {
  ARG_0,
  ARG_SHADOW
};


#define BORDER_SPACING  0
#define CHILD_SPACING   3


static void gtk_menu_bar_class_init    (GtkMenuBarClass *klass);
static void gtk_menu_bar_init          (GtkMenuBar      *menu_bar);
static void gtk_menu_bar_set_arg       (GtkObject      *object,
					GtkArg         *arg,
					guint           arg_id);
static void gtk_menu_bar_get_arg       (GtkObject      *object,
					GtkArg         *arg,
					guint           arg_id);
static void gtk_menu_bar_size_request  (GtkWidget       *widget,
					GtkRequisition  *requisition);
static void gtk_menu_bar_size_allocate (GtkWidget       *widget,
					GtkAllocation   *allocation);
static void gtk_menu_bar_paint         (GtkWidget       *widget,
					GdkRectangle    *area);
static void gtk_menu_bar_draw          (GtkWidget       *widget,
					GdkRectangle    *area);
static gint gtk_menu_bar_expose        (GtkWidget       *widget,
					GdkEventExpose  *event);


GtkType
gtk_menu_bar_get_type (void)
{
  static GtkType menu_bar_type = 0;

  if (!menu_bar_type)
    {
      static const GtkTypeInfo menu_bar_info =
      {
	"GtkMenuBar",
	sizeof (GtkMenuBar),
	sizeof (GtkMenuBarClass),
	(GtkClassInitFunc) gtk_menu_bar_class_init,
	(GtkObjectInitFunc) gtk_menu_bar_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      menu_bar_type = gtk_type_unique (gtk_menu_shell_get_type (), &menu_bar_info);
    }

  return menu_bar_type;
}

static void
gtk_menu_bar_class_init (GtkMenuBarClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkMenuShellClass *menu_shell_class;

  GtkBindingSet *binding_set;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  menu_shell_class = (GtkMenuShellClass*) class;

  gtk_object_add_arg_type ("GtkMenuBar::shadow", GTK_TYPE_SHADOW_TYPE, GTK_ARG_READWRITE, ARG_SHADOW);

  object_class->set_arg = gtk_menu_bar_set_arg;
  object_class->get_arg = gtk_menu_bar_get_arg;

  widget_class->draw = gtk_menu_bar_draw;
  widget_class->size_request = gtk_menu_bar_size_request;
  widget_class->size_allocate = gtk_menu_bar_size_allocate;
  widget_class->expose_event = gtk_menu_bar_expose;

  menu_shell_class->submenu_placement = GTK_TOP_BOTTOM;

  binding_set = gtk_binding_set_by_class (class);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Left, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PREV);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Right, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_NEXT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Up, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_PARENT);
  gtk_binding_entry_add_signal (binding_set,
				GDK_Down, 0,
				"move_current", 1,
				GTK_TYPE_MENU_DIRECTION_TYPE,
				GTK_MENU_DIR_CHILD);
}

static void
gtk_menu_bar_init (GtkMenuBar *menu_bar)
{
  menu_bar->shadow_type = GTK_SHADOW_OUT;
}

static void
gtk_menu_bar_set_arg (GtkObject      *object,
		      GtkArg         *arg,
		      guint           arg_id)
{
  GtkMenuBar *menu_bar;

  menu_bar = GTK_MENU_BAR (object);

  switch (arg_id)
    {
    case ARG_SHADOW:
      gtk_menu_bar_set_shadow_type (menu_bar, GTK_VALUE_ENUM (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_menu_bar_get_arg (GtkObject      *object,
		      GtkArg         *arg,
		      guint           arg_id)
{
  GtkMenuBar *menu_bar;

  menu_bar = GTK_MENU_BAR (object);

  switch (arg_id)
    {
    case ARG_SHADOW:
      GTK_VALUE_ENUM (*arg) = menu_bar->shadow_type;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}
 
GtkWidget*
gtk_menu_bar_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_menu_bar_get_type ()));
}

void
gtk_menu_bar_append (GtkMenuBar *menu_bar,
		     GtkWidget  *child)
{
  gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), child);
}

void
gtk_menu_bar_prepend (GtkMenuBar *menu_bar,
		      GtkWidget  *child)
{
  gtk_menu_shell_prepend (GTK_MENU_SHELL (menu_bar), child);
}

void
gtk_menu_bar_insert (GtkMenuBar *menu_bar,
		     GtkWidget  *child,
		     gint        position)
{
  gtk_menu_shell_insert (GTK_MENU_SHELL (menu_bar), child, position);
}


static void
gtk_menu_bar_size_request (GtkWidget      *widget,
			   GtkRequisition *requisition)
{
  GtkMenuBar *menu_bar;
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GList *children;
  gint nchildren;
  GtkRequisition child_requisition;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_BAR (widget));
  g_return_if_fail (requisition != NULL);

  requisition->width = 0;
  requisition->height = 0;

  if (GTK_WIDGET_VISIBLE (widget))
    {
      menu_bar = GTK_MENU_BAR (widget);
      menu_shell = GTK_MENU_SHELL (widget);

      nchildren = 0;
      children = menu_shell->children;

      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (GTK_WIDGET_VISIBLE (child))
	    {
	      GTK_MENU_ITEM (child)->show_submenu_indicator = FALSE;
	      gtk_widget_size_request (child, &child_requisition);

	      requisition->width += child_requisition.width;
	      requisition->height = MAX (requisition->height, child_requisition.height);
	      /* Support for the right justified help menu */
	      if ((children == NULL) && GTK_IS_MENU_ITEM(child) &&
		  GTK_MENU_ITEM(child)->right_justify)
		{
		  requisition->width += CHILD_SPACING;
		}

	      nchildren += 1;
	    }
	}

      requisition->width += (GTK_CONTAINER (menu_bar)->border_width +
			     widget->style->klass->xthickness +
			     BORDER_SPACING) * 2;
      requisition->height += (GTK_CONTAINER (menu_bar)->border_width +
			      widget->style->klass->ythickness +
			      BORDER_SPACING) * 2;

      if (nchildren > 0)
	requisition->width += 2 * CHILD_SPACING * (nchildren - 1);
    }
}

static void
gtk_menu_bar_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  GtkMenuBar *menu_bar;
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GList *children;
  GtkAllocation child_allocation;
  GtkRequisition child_requisition;
  guint offset;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_BAR (widget));
  g_return_if_fail (allocation != NULL);

  menu_bar = GTK_MENU_BAR (widget);
  menu_shell = GTK_MENU_SHELL (widget);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x, allocation->y,
			    allocation->width, allocation->height);

  if (menu_shell->children)
    {
      child_allocation.x = (GTK_CONTAINER (menu_bar)->border_width +
			    widget->style->klass->xthickness +
			    BORDER_SPACING);
      offset = child_allocation.x; 	/* Window edge to menubar start */

      child_allocation.y = (GTK_CONTAINER (menu_bar)->border_width +
			    widget->style->klass->ythickness +
			    BORDER_SPACING);
      child_allocation.height = MAX (1, (gint)allocation->height - child_allocation.y * 2);

      children = menu_shell->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  gtk_widget_get_child_requisition (child, &child_requisition);
	  
	  /* Support for the right justified help menu */
	  if ( (children == NULL) && (GTK_IS_MENU_ITEM(child))
	      && (GTK_MENU_ITEM(child)->right_justify)) 
	    {
	      child_allocation.x = allocation->width -
		  child_requisition.width - CHILD_SPACING - offset;
	    }
	  if (GTK_WIDGET_VISIBLE (child))
	    {
	      child_allocation.width = child_requisition.width;

	      gtk_widget_size_allocate (child, &child_allocation);

	      child_allocation.x += child_allocation.width + CHILD_SPACING * 2;
	    }
	}
    }
}

void
gtk_menu_bar_set_shadow_type (GtkMenuBar    *menu_bar,
			      GtkShadowType  type)
{
  g_return_if_fail (menu_bar != NULL);
  g_return_if_fail (GTK_IS_MENU_BAR (menu_bar));

  if ((GtkShadowType) menu_bar->shadow_type != type)
    {
      menu_bar->shadow_type = type;

      if (GTK_WIDGET_DRAWABLE (menu_bar))
        {
          gtk_widget_queue_clear (GTK_WIDGET (menu_bar));
        }
      gtk_widget_queue_resize (GTK_WIDGET (menu_bar));
    }
}

static void
gtk_menu_bar_paint (GtkWidget *widget, GdkRectangle *area)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_BAR (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_paint_box (widget->style,
		     widget->window,
		     GTK_STATE_NORMAL,
		     GTK_MENU_BAR (widget)->shadow_type,
		     area, widget, "menubar",
		     0, 0,
		     -1,-1);
    }
}

static void
gtk_menu_bar_draw (GtkWidget    *widget,
		   GdkRectangle *area)
{
  GtkMenuShell *menu_shell;
  GtkWidget *child;
  GdkRectangle child_area;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_BAR (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_menu_bar_paint (widget, area);

      menu_shell = GTK_MENU_SHELL (widget);

      children = menu_shell->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (gtk_widget_intersect (child, area, &child_area))
	    gtk_widget_draw (child, &child_area);
	}
    }
}

static gint
gtk_menu_bar_expose (GtkWidget      *widget,
		     GdkEventExpose *event)
{
  GtkMenuShell *menu_shell;
  GdkEventExpose child_event;
  GList *children;
  GtkWidget *child;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU_BAR (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_menu_bar_paint (widget, &event->area);

      menu_shell = GTK_MENU_SHELL (widget);
      child_event = *event;

      children = menu_shell->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (GTK_WIDGET_NO_WINDOW (child) &&
	      gtk_widget_intersect (child, &event->area, &child_event.area))
	    gtk_widget_event (child, (GdkEvent*) &child_event);
	}
    }

  return FALSE;
}
