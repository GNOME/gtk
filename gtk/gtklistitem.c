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
#include "gtklabel.h"
#include "gtklistitem.h"
#include "gtklist.h"

static void gtk_list_item_class_init    (GtkListItemClass *klass);
static void gtk_list_item_init          (GtkListItem      *list_item);
static void gtk_list_item_realize       (GtkWidget        *widget);
static void gtk_list_item_size_request  (GtkWidget        *widget,
					 GtkRequisition   *requisition);
static void gtk_list_item_size_allocate (GtkWidget        *widget,
					 GtkAllocation    *allocation);
static void gtk_list_item_draw          (GtkWidget        *widget,
					 GdkRectangle     *area);
static void gtk_list_item_draw_focus    (GtkWidget        *widget);
static gint gtk_list_item_button_press  (GtkWidget        *widget,
					 GdkEventButton   *event);
static gint gtk_list_item_expose        (GtkWidget        *widget,
					 GdkEventExpose   *event);
static gint gtk_list_item_focus_in      (GtkWidget        *widget,
					 GdkEventFocus    *event);
static gint gtk_list_item_focus_out     (GtkWidget        *widget,
					 GdkEventFocus    *event);
static void gtk_real_list_item_select   (GtkItem          *item);
static void gtk_real_list_item_deselect (GtkItem          *item);
static void gtk_real_list_item_toggle   (GtkItem          *item);


static GtkItemClass *parent_class = NULL;


guint
gtk_list_item_get_type ()
{
  static guint list_item_type = 0;

  if (!list_item_type)
    {
      GtkTypeInfo list_item_info =
      {
	"GtkListItem",
	sizeof (GtkListItem),
	sizeof (GtkListItemClass),
	(GtkClassInitFunc) gtk_list_item_class_init,
	(GtkObjectInitFunc) gtk_list_item_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      list_item_type = gtk_type_unique (gtk_item_get_type (), &list_item_info);
    }

  return list_item_type;
}

static void
gtk_list_item_class_init (GtkListItemClass *class)
{
  GtkWidgetClass *widget_class;
  GtkItemClass *item_class;

  widget_class = (GtkWidgetClass*) class;
  item_class = (GtkItemClass*) class;

  parent_class = gtk_type_class (gtk_item_get_type ());

  widget_class->realize = gtk_list_item_realize;
  widget_class->size_request = gtk_list_item_size_request;
  widget_class->size_allocate = gtk_list_item_size_allocate;
  widget_class->draw = gtk_list_item_draw;
  widget_class->draw_focus = gtk_list_item_draw_focus;
  widget_class->button_press_event = gtk_list_item_button_press;
  widget_class->expose_event = gtk_list_item_expose;
  widget_class->focus_in_event = gtk_list_item_focus_in;
  widget_class->focus_out_event = gtk_list_item_focus_out;

  item_class->select = gtk_real_list_item_select;
  item_class->deselect = gtk_real_list_item_deselect;
  item_class->toggle = gtk_real_list_item_toggle;
}

static void
gtk_list_item_init (GtkListItem *list_item)
{
  GTK_WIDGET_SET_FLAGS (list_item, GTK_CAN_FOCUS);
}

GtkWidget*
gtk_list_item_new ()
{
  return GTK_WIDGET (gtk_type_new (gtk_list_item_get_type ()));
}

GtkWidget*
gtk_list_item_new_with_label (const gchar *label)
{
  GtkWidget *list_item;
  GtkWidget *label_widget;

  list_item = gtk_list_item_new ();
  label_widget = gtk_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (label_widget), 0.0, 0.5);

  gtk_container_add (GTK_CONTAINER (list_item), label_widget);
  gtk_widget_show (label_widget);

  return list_item;
}

void
gtk_list_item_select (GtkListItem *list_item)
{
  gtk_item_select (GTK_ITEM (list_item));
}

void
gtk_list_item_deselect (GtkListItem *list_item)
{
  gtk_item_deselect (GTK_ITEM (list_item));
}


static void
gtk_list_item_realize (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LIST_ITEM (widget));

  if (GTK_WIDGET_CLASS (parent_class)->realize)
    (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

  gdk_window_set_background (widget->window, 
			     &widget->style->base[GTK_STATE_NORMAL]);
}

static void
gtk_list_item_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkBin *bin;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LIST_ITEM (widget));
  g_return_if_fail (requisition != NULL);

  bin = GTK_BIN (widget);

  requisition->width = (GTK_CONTAINER (widget)->border_width +
			widget->style->klass->xthickness) * 2;
  requisition->height = GTK_CONTAINER (widget)->border_width * 2;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gtk_widget_size_request (bin->child, &bin->child->requisition);

      requisition->width += bin->child->requisition.width;
      requisition->height += bin->child->requisition.height;
    }
}

static void
gtk_list_item_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkAllocation child_allocation;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LIST_ITEM (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x, allocation->y,
			    allocation->width, allocation->height);

  bin = GTK_BIN (widget);

  if (bin->child)
    {
      child_allocation.x = (GTK_CONTAINER (widget)->border_width +
			    widget->style->klass->xthickness);
      child_allocation.y = GTK_CONTAINER (widget)->border_width;
      child_allocation.width = allocation->width - child_allocation.x * 2;
      child_allocation.height = allocation->height - child_allocation.y * 2;

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static void
gtk_list_item_draw (GtkWidget    *widget,
		    GdkRectangle *area)
{
  GtkBin *bin;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LIST_ITEM (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);

      if (!GTK_WIDGET_IS_SENSITIVE (widget))
	gtk_style_set_background (widget->style, widget->window, GTK_STATE_INSENSITIVE);
      else if (widget->state == GTK_STATE_NORMAL)
	gdk_window_set_background (widget->window, 
				   &widget->style->base[GTK_STATE_NORMAL]);
      else
	gtk_style_set_background (widget->style, widget->window, widget->state);

      gdk_window_clear_area (widget->window, area->x, area->y,
			     area->width, area->height);

      if (bin->child && gtk_widget_intersect (bin->child, area, &child_area))
	gtk_widget_draw (bin->child, &child_area);

      gtk_widget_draw_focus (widget);
    }
}

static void
gtk_list_item_draw_focus (GtkWidget *widget)
{
  GdkGC *gc;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LIST_ITEM (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      if (GTK_WIDGET_HAS_FOCUS (widget))
	gc = widget->style->black_gc;
      else if (!GTK_WIDGET_IS_SENSITIVE (widget))
	gc = widget->style->bg_gc[GTK_STATE_INSENSITIVE];
      else if (widget->state == GTK_STATE_NORMAL)
	gc = widget->style->base_gc[GTK_STATE_NORMAL];
      else
	gc = widget->style->bg_gc[widget->state];

      gdk_draw_rectangle (widget->window, gc, FALSE, 0, 0,
			  widget->allocation.width - 1,
			  widget->allocation.height - 1);
    }
}

static gint
gtk_list_item_button_press (GtkWidget      *widget,
			    GdkEventButton *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_LIST_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->type == GDK_BUTTON_PRESS)
    if (!GTK_WIDGET_HAS_FOCUS (widget))
      gtk_widget_grab_focus (widget);

  return FALSE;
}

static gint
gtk_list_item_expose (GtkWidget      *widget,
		      GdkEventExpose *event)
{
  GtkBin *bin;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_LIST_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);

      if (!GTK_WIDGET_IS_SENSITIVE (widget))
	gdk_window_set_background (widget->window, &widget->style->bg[GTK_STATE_INSENSITIVE]);
      else if (widget->state == GTK_STATE_NORMAL)
	gdk_window_set_background (widget->window, &widget->style->base[GTK_STATE_NORMAL]);
      else
	gdk_window_set_background (widget->window, &widget->style->bg[widget->state]);

      gdk_window_clear_area (widget->window, event->area.x, event->area.y,
			     event->area.width, event->area.height);

      if (bin->child)
	{
	  child_event = *event;

	  if (GTK_WIDGET_NO_WINDOW (bin->child) &&
	      gtk_widget_intersect (bin->child, &event->area, &child_event.area))
	    gtk_widget_event (bin->child, (GdkEvent*) &child_event);
	}

      gtk_widget_draw_focus (widget);
    }

  return FALSE;
}

static gint
gtk_list_item_focus_in (GtkWidget     *widget,
			GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_LIST_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);

  return FALSE;
}

static gint
gtk_list_item_focus_out (GtkWidget     *widget,
			 GdkEventFocus *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_LIST_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
  gtk_widget_draw_focus (widget);

  return FALSE;
}

static void
gtk_real_list_item_select (GtkItem *item)
{
  g_return_if_fail (item != NULL);
  g_return_if_fail (GTK_IS_LIST_ITEM (item));

  if (GTK_WIDGET (item)->state == GTK_STATE_SELECTED)
    return;

  gtk_widget_set_state (GTK_WIDGET (item), GTK_STATE_SELECTED);
  gtk_widget_queue_draw (GTK_WIDGET (item));
}

static void
gtk_real_list_item_deselect (GtkItem *item)
{
  g_return_if_fail (item != NULL);
  g_return_if_fail (GTK_IS_LIST_ITEM (item));

  if (GTK_WIDGET (item)->state == GTK_STATE_NORMAL)
    return;

  gtk_widget_set_state (GTK_WIDGET (item), GTK_STATE_NORMAL);
  gtk_widget_queue_draw (GTK_WIDGET (item));
}

static void
gtk_real_list_item_toggle (GtkItem *item)
{
  g_return_if_fail (item != NULL);
  g_return_if_fail (GTK_IS_LIST_ITEM (item));
  
  if (GTK_WIDGET (item)->parent && GTK_IS_LIST (GTK_WIDGET (item)->parent))
    gtk_list_select_child (GTK_LIST (GTK_WIDGET (item)->parent),
			   GTK_WIDGET (item));
  else
    {
      /* Should we really bother with this bit? A listitem not in a list?
       * -Johannes Keukelaar
       * yes, always be on the save side!
       * -timj
       */
      if (GTK_WIDGET (item)->state == GTK_STATE_SELECTED)
	gtk_widget_set_state (GTK_WIDGET (item), GTK_STATE_NORMAL);
      else
	gtk_widget_set_state (GTK_WIDGET (item), GTK_STATE_SELECTED);
      gtk_widget_queue_draw (GTK_WIDGET (item));
    }
}
