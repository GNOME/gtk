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

#include "gtksignal.h"
#include "gtkeventbox.h"


static void gtk_event_box_class_init               (GtkEventBoxClass *klass);
static void gtk_event_box_init                     (GtkEventBox      *event_box);
static void gtk_event_box_realize                  (GtkWidget        *widget);
static void gtk_event_box_size_request             (GtkWidget        *widget,
						    GtkRequisition   *requisition);
static void gtk_event_box_size_allocate            (GtkWidget        *widget,
						    GtkAllocation    *allocation);
static void gtk_event_box_paint                    (GtkWidget         *widget,
						    GdkRectangle      *area);
static void gtk_event_box_draw                     (GtkWidget         *widget,
						   GdkRectangle       *area);
static gint gtk_event_box_expose                   (GtkWidget         *widget,
						   GdkEventExpose     *event);


GtkType
gtk_event_box_get_type (void)
{
  static GtkType event_box_type = 0;

  if (!event_box_type)
    {
      static const GtkTypeInfo event_box_info =
      {
	"GtkEventBox",
	sizeof (GtkEventBox),
	sizeof (GtkEventBoxClass),
	(GtkClassInitFunc) gtk_event_box_class_init,
	(GtkObjectInitFunc) gtk_event_box_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      event_box_type = gtk_type_unique (gtk_bin_get_type (), &event_box_info);
    }

  return event_box_type;
}

static void
gtk_event_box_class_init (GtkEventBoxClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->realize = gtk_event_box_realize;
  widget_class->size_request = gtk_event_box_size_request;
  widget_class->size_allocate = gtk_event_box_size_allocate;
  widget_class->draw = gtk_event_box_draw;
  widget_class->expose_event = gtk_event_box_expose;
}

static void
gtk_event_box_init (GtkEventBox *event_box)
{
  GTK_WIDGET_UNSET_FLAGS (event_box, GTK_NO_WINDOW);
}

GtkWidget*
gtk_event_box_new (void)
{
  return GTK_WIDGET ( gtk_type_new (gtk_event_box_get_type ()));
}

static void
gtk_event_box_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint border_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_EVENT_BOX (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  border_width = GTK_CONTAINER (widget)->border_width;
  
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - 2*border_width;
  attributes.height = widget->allocation.height - 2*border_width;
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

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
gtk_event_box_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  GtkBin *bin;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_EVENT_BOX (widget));
  g_return_if_fail (requisition != NULL);

  bin = GTK_BIN (widget);

  requisition->width = GTK_CONTAINER (widget)->border_width * 2;
  requisition->height = GTK_CONTAINER (widget)->border_width * 2;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      GtkRequisition child_requisition;
      
      gtk_widget_size_request (bin->child, &child_requisition);

      requisition->width += child_requisition.width;
      requisition->height += child_requisition.height;
    }
}

static void
gtk_event_box_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkAllocation child_allocation;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_EVENT_BOX (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  bin = GTK_BIN (widget);

  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width = MAX (allocation->width - GTK_CONTAINER (widget)->border_width * 2, 0);
  child_allocation.height = MAX (allocation->height - GTK_CONTAINER (widget)->border_width * 2, 0);

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x + GTK_CONTAINER (widget)->border_width,
			      allocation->y + GTK_CONTAINER (widget)->border_width,
			      child_allocation.width,
			      child_allocation.height);
    }
  
  if (bin->child)
    {
      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static void
gtk_event_box_paint (GtkWidget    *widget,
		     GdkRectangle *area)
{
  gtk_paint_flat_box (widget->style, widget->window,
		      widget->state, GTK_SHADOW_NONE,
		      area, widget, "eventbox",
		      0, 0, -1, -1);
}

static void
gtk_event_box_draw (GtkWidget    *widget,
		    GdkRectangle *area)
{
  GtkBin *bin;
  GdkRectangle tmp_area;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_EVENT_BOX (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);
      tmp_area = *area;
      tmp_area.x -= GTK_CONTAINER (widget)->border_width;
      tmp_area.y -= GTK_CONTAINER (widget)->border_width;

      gtk_event_box_paint (widget, &tmp_area);
      
      if (bin->child)
	{
	  if (gtk_widget_intersect (bin->child, &tmp_area, &child_area))
	    gtk_widget_draw (bin->child, &child_area);
	}
    }
}

static gint
gtk_event_box_expose (GtkWidget      *widget,
		     GdkEventExpose *event)
{
  GtkBin *bin;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_EVENT_BOX (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);

      gtk_event_box_paint (widget, &event->area);
      
      child_event = *event;
      if (bin->child &&
	  GTK_WIDGET_NO_WINDOW (bin->child) &&
	  gtk_widget_intersect (bin->child, &event->area, &child_event.area))
	gtk_widget_event (bin->child, (GdkEvent*) &child_event);
    }

  return FALSE;
}

