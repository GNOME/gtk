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

#include "gtkmenu.h"
#include "gtksignal.h"
#include "gtktearoffmenuitem.h"

#define ARROW_SIZE 10
#define TEAR_LENGTH 5
#define BORDER_SPACING  3

static void gtk_tearoff_menu_item_class_init (GtkTearoffMenuItemClass *klass);
static void gtk_tearoff_menu_item_init       (GtkTearoffMenuItem      *tearoff_menu_item);
static void gtk_tearoff_menu_item_size_request (GtkWidget             *widget,
				                GtkRequisition        *requisition);
static void gtk_tearoff_menu_item_draw       (GtkWidget             *widget,
					      GdkRectangle          *area);
static gint gtk_tearoff_menu_item_expose     (GtkWidget             *widget,
					      GdkEventExpose        *event);
static void gtk_tearoff_menu_item_activate   (GtkMenuItem           *menu_item);
static gint gtk_tearoff_menu_item_delete_cb  (GtkMenuItem           *menu_item,
					      GdkEventAny           *event);

GtkType
gtk_tearoff_menu_item_get_type (void)
{
  static GtkType tearoff_menu_item_type = 0;

  if (!tearoff_menu_item_type)
    {
      static const GtkTypeInfo tearoff_menu_item_info =
      {
        "GtkTearoffMenuItem",
        sizeof (GtkTearoffMenuItem),
        sizeof (GtkTearoffMenuItemClass),
        (GtkClassInitFunc) gtk_tearoff_menu_item_class_init,
        (GtkObjectInitFunc) gtk_tearoff_menu_item_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      tearoff_menu_item_type = gtk_type_unique (gtk_menu_item_get_type (), &tearoff_menu_item_info);
    }

  return tearoff_menu_item_type;
}

GtkWidget*
gtk_tearoff_menu_item_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_tearoff_menu_item_get_type ()));
}

static void
gtk_tearoff_menu_item_class_init (GtkTearoffMenuItemClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkMenuItemClass *menu_item_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  menu_item_class = (GtkMenuItemClass*) klass;

  widget_class->draw = gtk_tearoff_menu_item_draw;
  widget_class->expose_event = gtk_tearoff_menu_item_expose;
  widget_class->size_request = gtk_tearoff_menu_item_size_request;

  menu_item_class->activate = gtk_tearoff_menu_item_activate;
}

static void
gtk_tearoff_menu_item_init (GtkTearoffMenuItem *tearoff_menu_item)
{
  tearoff_menu_item->torn_off = FALSE;
}

static void
gtk_tearoff_menu_item_size_request (GtkWidget      *widget,
				    GtkRequisition *requisition)
{
  GtkTearoffMenuItem *tearoff;

  tearoff = GTK_TEAROFF_MENU_ITEM (widget);
  
  requisition->width = (GTK_CONTAINER (widget)->border_width +
			widget->style->klass->xthickness +
			BORDER_SPACING) * 2;
  requisition->height = (GTK_CONTAINER (widget)->border_width +
			 widget->style->klass->ythickness) * 2;

  if (tearoff->torn_off)
    {
      requisition->height += ARROW_SIZE;
    }
  else
    {
      requisition->height += widget->style->klass->ythickness;
    }
}

static void
gtk_tearoff_menu_item_paint (GtkWidget   *widget,
			     GdkRectangle *area)
{
  GtkMenuItem *menu_item;
  GtkTearoffMenuItem *tearoff_item;
  GtkShadowType shadow_type;
  gint width, height;
  gint x, y;
  gint right_max;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TEAROFF_MENU_ITEM (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      menu_item = GTK_MENU_ITEM (widget);
      tearoff_item = GTK_TEAROFF_MENU_ITEM (widget);

      x = GTK_CONTAINER (menu_item)->border_width;
      y = GTK_CONTAINER (menu_item)->border_width;
      width = widget->allocation.width - x * 2;
      height = widget->allocation.height - y * 2;
      right_max = x + width;

      if (widget->state == GTK_STATE_PRELIGHT)
	gtk_paint_box (widget->style,
		       widget->window,
		       GTK_STATE_PRELIGHT,
		       GTK_SHADOW_OUT,
		       area, widget, "menuitem",
		       x, y, width, height);
       else
	 gdk_window_clear_area (widget->window, area->x, area->y, area->width, area->height);

      if (tearoff_item->torn_off)
	{
	  gint arrow_x;

	  if (widget->state == GTK_STATE_PRELIGHT)
	    shadow_type = GTK_SHADOW_IN;
	  else
	    shadow_type = GTK_SHADOW_OUT;

	  if (menu_item->toggle_size > ARROW_SIZE)
	    {
	      arrow_x = x + (menu_item->toggle_size - ARROW_SIZE)/2;
	      x += menu_item->toggle_size + BORDER_SPACING;
	    }
	  else
	    {
	      arrow_x = ARROW_SIZE / 2;
	      x += 2 * ARROW_SIZE;
	    }
	  
	  gtk_draw_arrow (widget->style, widget->window,
			  widget->state, shadow_type, GTK_ARROW_LEFT, FALSE,
			  arrow_x, y + height / 2 - 5, 
			  ARROW_SIZE, ARROW_SIZE);
	}

      while (x < right_max)
	{
	  gtk_draw_hline (widget->style, widget->window, GTK_STATE_NORMAL,
			  x, MIN (x+TEAR_LENGTH, right_max),
			  y + (height - widget->style->klass->ythickness)/2);
	  x += 2 * TEAR_LENGTH;
	}
    }
}

static void
gtk_tearoff_menu_item_draw (GtkWidget    *widget,
			  GdkRectangle *area)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_TEAROFF_MENU_ITEM (widget));
  g_return_if_fail (area != NULL);

  gtk_tearoff_menu_item_paint (widget, area);
}

static gint
gtk_tearoff_menu_item_expose (GtkWidget      *widget,
			    GdkEventExpose *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TEAROFF_MENU_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  gtk_tearoff_menu_item_paint (widget, &event->area);

  return FALSE;
}

static gint
gtk_tearoff_menu_item_delete_cb (GtkMenuItem *menu_item, GdkEventAny *event)
{
  gtk_tearoff_menu_item_activate (menu_item);
  return TRUE;
}

static void
gtk_tearoff_menu_item_activate (GtkMenuItem *menu_item)
{
  GtkTearoffMenuItem *tearoff_menu_item;

  g_return_if_fail (menu_item != NULL);
  g_return_if_fail (GTK_IS_TEAROFF_MENU_ITEM (menu_item));

  tearoff_menu_item = GTK_TEAROFF_MENU_ITEM (menu_item);
  tearoff_menu_item->torn_off = !tearoff_menu_item->torn_off;

  if (GTK_IS_MENU (GTK_WIDGET (menu_item)->parent))
    {
      GtkMenu *menu = GTK_MENU (GTK_WIDGET (menu_item)->parent);
      gboolean need_connect;
      
      	need_connect = (tearoff_menu_item->torn_off && !menu->tearoff_window);

	gtk_menu_set_tearoff_state (GTK_MENU (GTK_WIDGET (menu_item)->parent),
				    tearoff_menu_item->torn_off);

	if (need_connect)
	  gtk_signal_connect_object (GTK_OBJECT (menu->tearoff_window),  
				     "delete_event",
				     GTK_SIGNAL_FUNC (gtk_tearoff_menu_item_delete_cb),
				     GTK_OBJECT (menu_item));
    }
  
  gtk_widget_queue_resize (GTK_WIDGET (menu_item));
}

