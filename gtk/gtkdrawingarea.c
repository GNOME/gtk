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

#include "gtkdrawingarea.h"


static void gtk_drawing_area_class_init    (GtkDrawingAreaClass *klass);
static void gtk_drawing_area_init          (GtkDrawingArea      *darea);
static void gtk_drawing_area_realize       (GtkWidget           *widget);
static void gtk_drawing_area_size_allocate (GtkWidget           *widget,
					    GtkAllocation       *allocation);
static void gtk_drawing_area_send_configure (GtkDrawingArea     *darea);


GtkType
gtk_drawing_area_get_type (void)
{
  static GtkType drawing_area_type = 0;

  if (!drawing_area_type)
    {
      static const GtkTypeInfo drawing_area_info =
      {
	"GtkDrawingArea",
	sizeof (GtkDrawingArea),
	sizeof (GtkDrawingAreaClass),
	(GtkClassInitFunc) gtk_drawing_area_class_init,
	(GtkObjectInitFunc) gtk_drawing_area_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      drawing_area_type = gtk_type_unique (GTK_TYPE_WIDGET, &drawing_area_info);
    }

  return drawing_area_type;
}

static void
gtk_drawing_area_class_init (GtkDrawingAreaClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = GTK_WIDGET_CLASS (class);

  widget_class->realize = gtk_drawing_area_realize;
  widget_class->size_allocate = gtk_drawing_area_size_allocate;
}

static void
gtk_drawing_area_init (GtkDrawingArea *darea)
{
  darea->draw_data = NULL;
}


GtkWidget*
gtk_drawing_area_new (void)
{
  return GTK_WIDGET (gtk_type_new (gtk_drawing_area_get_type ()));
}

void
gtk_drawing_area_size (GtkDrawingArea *darea,
		       gint            width,
		       gint            height)
{
  g_return_if_fail (GTK_IS_DRAWING_AREA (darea));

  GTK_WIDGET (darea)->requisition.width = width;
  GTK_WIDGET (darea)->requisition.height = height;

  gtk_widget_queue_resize (GTK_WIDGET (darea));
}

static void
gtk_drawing_area_realize (GtkWidget *widget)
{
  GtkDrawingArea *darea;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_DRAWING_AREA (widget));

  darea = GTK_DRAWING_AREA (widget);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, darea);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  gtk_drawing_area_send_configure (GTK_DRAWING_AREA (widget));
}

static void
gtk_drawing_area_size_allocate (GtkWidget     *widget,
				GtkAllocation *allocation)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_DRAWING_AREA (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  /* FIXME, TODO-1.3: back out the MAX() statements */
  widget->allocation.width = MAX (1, widget->allocation.width);
  widget->allocation.height = MAX (1, widget->allocation.height);

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      gtk_drawing_area_send_configure (GTK_DRAWING_AREA (widget));
    }
}

static void
gtk_drawing_area_send_configure (GtkDrawingArea *darea)
{
  GtkWidget *widget;
  GdkEventConfigure event;

  widget = GTK_WIDGET (darea);

  event.type = GDK_CONFIGURE;
  event.window = widget->window;
  event.send_event = TRUE;
  event.x = widget->allocation.x;
  event.y = widget->allocation.y;
  event.width = widget->allocation.width;
  event.height = widget->allocation.height;
  
  gtk_widget_event (widget, (GdkEvent*) &event);
}
