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
#include "gtksignal.h"
#include "gtkhandlebox.h"
#include <gdk/gdkx.h>

#define DRAG_HANDLE_SIZE 10

static void gtk_handle_box_class_init    (GtkHandleBoxClass *klass);
static void gtk_handle_box_init          (GtkHandleBox      *handle_box);
static void gtk_handle_box_realize       (GtkWidget        *widget);
static void gtk_handle_box_size_request  (GtkWidget *widget,
					  GtkRequisition   *requisition);
static void gtk_handle_box_size_allocate (GtkWidget *widget,
					  GtkAllocation *allocation);
static void gtk_handle_box_paint         (GtkWidget *widget,
					  GdkRectangle *area);
static void gtk_handle_box_draw          (GtkWidget *widget,
					  GdkRectangle *area);
static gint gtk_handle_box_expose        (GtkWidget *widget,
					  GdkEventExpose *event);
static gint gtk_handle_box_button_changed(GtkWidget *widget,
					  GdkEventButton *event);
static gint gtk_handle_box_motion        (GtkWidget *widget,
					  GdkEventMotion *event);


guint
gtk_handle_box_get_type ()
{
  static guint handle_box_type = 0;

  if (!handle_box_type)
    {
      GtkTypeInfo handle_box_info =
      {
	"GtkHandleBox",
	sizeof (GtkHandleBox),
	sizeof (GtkHandleBoxClass),
	(GtkClassInitFunc) gtk_handle_box_class_init,
	(GtkObjectInitFunc) gtk_handle_box_init,
	(GtkArgFunc) NULL,
      };

      handle_box_type = gtk_type_unique (gtk_event_box_get_type (), &handle_box_info);
    }

  return handle_box_type;
}

static void
gtk_handle_box_class_init (GtkHandleBoxClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;
  widget_class->realize = gtk_handle_box_realize;
  widget_class->size_request = gtk_handle_box_size_request;
  widget_class->size_allocate = gtk_handle_box_size_allocate;
  widget_class->draw = gtk_handle_box_draw;
  widget_class->expose_event = gtk_handle_box_expose;
  widget_class->button_press_event = gtk_handle_box_button_changed;
  widget_class->button_release_event = gtk_handle_box_button_changed;
  widget_class->motion_notify_event = gtk_handle_box_motion;
}

static void
gtk_handle_box_init (GtkHandleBox *handle_box)
{
  GTK_WIDGET_UNSET_FLAGS (handle_box, GTK_NO_WINDOW);
  GTK_WIDGET_SET_FLAGS (handle_box, GTK_BASIC);
  handle_box->is_being_dragged = FALSE;
  handle_box->is_onroot = FALSE;
  handle_box->real_parent = NULL;
}

GtkWidget*
gtk_handle_box_new ()
{
  return GTK_WIDGET ( gtk_type_new (gtk_handle_box_get_type ()));
}

static void
gtk_handle_box_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget)
			| GDK_BUTTON_MOTION_MASK
			| GDK_BUTTON_PRESS_MASK
			| GDK_BUTTON_RELEASE_MASK
			| GDK_EXPOSURE_MASK
			| GDK_ENTER_NOTIFY_MASK
			| GDK_LEAVE_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
gtk_handle_box_size_request (GtkWidget      *widget,
			     GtkRequisition *requisition)
{
  GtkBin *bin;
  GtkHandleBox *hb;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));
  g_return_if_fail (requisition != NULL);

  bin = GTK_BIN (widget);
  hb = GTK_HANDLE_BOX(widget);

  requisition->width = DRAG_HANDLE_SIZE + GTK_CONTAINER(widget)->border_width * 2;
  requisition->height = DRAG_HANDLE_SIZE + GTK_CONTAINER(widget)->border_width * 2;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gtk_widget_size_request (bin->child, &bin->child->requisition);

      requisition->width += bin->child->requisition.width;
      if(bin->child->requisition.height > requisition->height)
	requisition->height = bin->child->requisition.height;
    }

  hb->real_requisition = *requisition;
  if(hb->is_onroot)
    requisition->height = 3;
}

static void
gtk_handle_box_size_allocate (GtkWidget     *widget,
			      GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkAllocation child_allocation;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  bin = GTK_BIN (widget);

  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width = allocation->width - DRAG_HANDLE_SIZE - GTK_CONTAINER(widget)->border_width * 2;
  child_allocation.height = allocation->height - GTK_CONTAINER(widget)->border_width * 2;

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x + GTK_CONTAINER(widget)->border_width,
			      allocation->y + GTK_CONTAINER(widget)->border_width,
			      child_allocation.width,
			      child_allocation.height);
    }
  
  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static void gtk_handle_box_paint(GtkWidget *widget,
				 GdkRectangle *area)
{
  GtkHandleBox *hb;
  gint startx, endx, x;
  gint line_y2;

  hb = GTK_HANDLE_BOX(widget);

  startx = 1; endx = DRAG_HANDLE_SIZE;
  if(area->x > startx)
    startx = area->x;
  if((area->x + area->width) < endx)
    endx = area->x + area->width;
  line_y2 = area->y + area->height;

  for(x = startx; x < DRAG_HANDLE_SIZE; x += 3)
    gtk_draw_vline(widget->style,
		   widget->window,
		   GTK_WIDGET_STATE(widget),
		   area->y, line_y2,
		   x);

  gtk_draw_shadow(widget->style,
		  widget->window,
		  GTK_WIDGET_STATE(widget),
		  GTK_SHADOW_OUT,
		  0, 0,
		  widget->allocation.width - 1,
		  widget->allocation.height);
  if(hb->is_onroot)
    gtk_draw_hline(widget->style,
		   widget->parent->window,
		   GTK_WIDGET_STATE(widget),
		   widget->allocation.x,
		   widget->allocation.width - widget->allocation.x,
		   widget->allocation.y);
}

static void
gtk_handle_box_draw (GtkWidget    *widget,
		   GdkRectangle *area)
{
  GtkBin *bin;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HANDLE_BOX (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);
      
      gtk_handle_box_paint(widget, area);
      if (bin->child)
	{
	  if (gtk_widget_intersect (bin->child, area, &child_area))
	    gtk_widget_draw (bin->child, &child_area);
	}
    }
}

static gint
gtk_handle_box_expose (GtkWidget      *widget,
		       GdkEventExpose *event)
{
  GtkBin *bin;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HANDLE_BOX (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);
      gtk_handle_box_paint(widget, &event->area);

      child_event = *event;
      if (bin->child &&
	  GTK_WIDGET_NO_WINDOW (bin->child) &&
	  gtk_widget_intersect (bin->child, &event->area, &child_event.area))
	gtk_widget_event (bin->child, (GdkEvent*) &child_event);
    }

  return FALSE;
}

static gint dragoff_x, dragoff_y;
static gint parentx, parenty;

static void
gtk_handle_box_get_parent_position(GtkWidget *widget,
				   gint *x, gint *y)
{
  gdk_window_get_origin(widget->parent->window, x, y);
  *x += widget->allocation.x;
  *y += widget->allocation.y;
  g_print("Position is %d x %d\n", *x, *y);
}

static gint
gtk_handle_box_button_changed(GtkWidget *widget,
			      GdkEventButton *event)
{
  GtkHandleBox *hb;
  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_HANDLE_BOX(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  hb = GTK_HANDLE_BOX(widget);
  if(event->button == 1)
    {
      if(event->type == GDK_BUTTON_PRESS
	 && event->x < DRAG_HANDLE_SIZE)
	{
	  dragoff_x = event->x;
	  dragoff_y = event->y;
	  gtk_handle_box_get_parent_position(widget,
					     &parentx,
					     &parenty);
	  hb->is_being_dragged = TRUE;
	  g_print("Grabbing\n");
	  gdk_pointer_grab(widget->window,
			   TRUE,
			   GDK_POINTER_MOTION_MASK
			   |GDK_BUTTON_RELEASE_MASK,
			   GDK_ROOT_PARENT(),
			   NULL, 
			   GDK_CURRENT_TIME);
	}
      else if(event->type == GDK_BUTTON_RELEASE)
	{
	  g_print("Ungrabbing\n");
	  gdk_pointer_ungrab(GDK_CURRENT_TIME);
	  hb->is_being_dragged = FALSE;
	}
    }
  return TRUE;
}

static void
gtk_handle_box_reparent      (GtkWidget *widget,
			      gboolean in_root)
{
  GtkHandleBox *hb;

  hb = GTK_HANDLE_BOX(widget);

  if(in_root)
    {
      GTK_HANDLE_BOX(widget)->is_onroot = TRUE;
      g_print("Reparenting to root\n");
      gdk_window_set_override_redirect(widget->window, TRUE);
      gdk_window_reparent(widget->window, GDK_ROOT_PARENT(),
			  parentx,
			  parenty);
      gdk_window_raise(widget->window);
      widget->requisition = hb->real_requisition;
      gtk_widget_queue_resize(widget->parent);
      gdk_pointer_ungrab(GDK_CURRENT_TIME);
      gdk_pointer_grab(widget->window,
		       TRUE,
		       GDK_POINTER_MOTION_MASK
		       |GDK_BUTTON_RELEASE_MASK,
		       GDK_ROOT_PARENT(),
		       NULL, 
		       GDK_CURRENT_TIME);
    }
  else
    {
      GTK_HANDLE_BOX(widget)->is_onroot = FALSE;
      g_print("Reparenting to parent %#x\n", widget->parent->window);
      gdk_window_reparent(widget->window, widget->parent->window,
			  widget->allocation.x, widget->allocation.y);
      widget->requisition.height = 3;
      gtk_widget_queue_resize(widget->parent);
    }
}

static gint
gtk_handle_box_motion        (GtkWidget *widget,
			      GdkEventMotion *event)
{
  GtkHandleBox *hb;
  gint newx, newy;

  g_return_val_if_fail(widget != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_HANDLE_BOX(widget), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);

  hb = GTK_HANDLE_BOX(widget);

  if(hb->is_being_dragged) {
    newx = event->x_root - dragoff_x;
    newy = event->y_root - dragoff_y;
    if(newx < 0) newx = 0;
    if(newy < 0) newy = 0;
    if(abs(parentx - newx) < 10
       && abs(parenty - newy) < 10)
      {
	if(hb->is_onroot == TRUE)
	  gtk_handle_box_reparent(widget, FALSE);
      }
    else
      {
	if(hb->is_onroot == FALSE)
	  gtk_handle_box_reparent(widget, TRUE);
	gdk_window_move(widget->window, newx, newy);
      }
  }
  return TRUE;
}
