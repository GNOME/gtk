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
#include "gtkmenubar.h"
#include "gtkmenuitem.h"


#define BORDER_SPACING  2
#define CHILD_SPACING   3


static void gtk_menu_bar_class_init    (GtkMenuBarClass *klass);
static void gtk_menu_bar_init          (GtkMenuBar      *menu_bar);
static void gtk_menu_bar_size_request  (GtkWidget       *widget,
					GtkRequisition  *requisition);
static void gtk_menu_bar_size_allocate (GtkWidget       *widget,
					GtkAllocation   *allocation);
static void gtk_menu_bar_paint         (GtkWidget       *widget);
static void gtk_menu_bar_draw          (GtkWidget       *widget,
					GdkRectangle    *area);
static gint gtk_menu_bar_expose        (GtkWidget       *widget,
					GdkEventExpose  *event);


guint
gtk_menu_bar_get_type ()
{
  static guint menu_bar_type = 0;

  if (!menu_bar_type)
    {
      GtkTypeInfo menu_bar_info =
      {
	"GtkMenuBar",
	sizeof (GtkMenuBar),
	sizeof (GtkMenuBarClass),
	(GtkClassInitFunc) gtk_menu_bar_class_init,
	(GtkObjectInitFunc) gtk_menu_bar_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      menu_bar_type = gtk_type_unique (gtk_menu_shell_get_type (), &menu_bar_info);
    }

  return menu_bar_type;
}

static void
gtk_menu_bar_class_init (GtkMenuBarClass *class)
{
  GtkWidgetClass *widget_class;
  GtkMenuShellClass *menu_shell_class;

  widget_class = (GtkWidgetClass*) class;
  menu_shell_class = (GtkMenuShellClass*) class;

  widget_class->draw = gtk_menu_bar_draw;
  widget_class->size_request = gtk_menu_bar_size_request;
  widget_class->size_allocate = gtk_menu_bar_size_allocate;
  widget_class->expose_event = gtk_menu_bar_expose;

  menu_shell_class->submenu_placement = GTK_TOP_BOTTOM;
}

static void
gtk_menu_bar_init (GtkMenuBar *menu_bar)
{
}

GtkWidget*
gtk_menu_bar_new ()
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
	      gtk_widget_size_request (child, &child->requisition);

	      requisition->width += child->requisition.width;
	      requisition->height = MAX (requisition->height, child->requisition.height);
	      /* Support for the right justified help menu */
	      if ( (children == NULL) && (GTK_IS_MENU_ITEM(child))
		   && (GTK_MENU_ITEM(child)->right_justify))
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
      child_allocation.height = MAX (0, allocation->height - child_allocation.y * 2);

      children = menu_shell->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  /* Support for the right justified help menu */
	  if ( (children == NULL) && (GTK_IS_MENU_ITEM(child))
	      && (GTK_MENU_ITEM(child)->right_justify)) 
	    {
	      child_allocation.x = allocation->width -
		  child->requisition.width - CHILD_SPACING - offset;
	    }
	  if (GTK_WIDGET_VISIBLE (child))
	    {
	      child_allocation.width = child->requisition.width;

	      gtk_widget_size_allocate (child, &child_allocation);

	      child_allocation.x += child_allocation.width + CHILD_SPACING * 2;
	    }
	}
    }
}

static void
gtk_menu_bar_paint (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MENU_BAR (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_draw_shadow (widget->style,
		       widget->window,
		       GTK_STATE_NORMAL,
		       GTK_SHADOW_OUT,
		       0, 0,
		       widget->allocation.width,
		       widget->allocation.height);
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
      gtk_menu_bar_paint (widget);

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
  GtkWidget *child;
  GdkEventExpose child_event;
  GList *children;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU_BAR (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_menu_bar_paint (widget);

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
