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
#include "gtkdrawingarea.h"


static void gtk_drawing_area_class_init    (GtkDrawingAreaClass *klass);
static void gtk_drawing_area_init          (GtkDrawingArea      *darea);
static void gtk_drawing_area_realize       (GtkWidget           *widget);
static void gtk_drawing_area_size_allocate (GtkWidget           *widget,
					    GtkAllocation       *allocation);


guint
gtk_drawing_area_get_type ()
{
  static guint drawing_area_type = 0;

  if (!drawing_area_type)
    {
      GtkTypeInfo drawing_area_info =
      {
	"GtkDrawingArea",
	sizeof (GtkDrawingArea),
	sizeof (GtkDrawingAreaClass),
	(GtkClassInitFunc) gtk_drawing_area_class_init,
	(GtkObjectInitFunc) gtk_drawing_area_init,
	(GtkArgFunc) NULL,
      };

      drawing_area_type = gtk_type_unique (gtk_widget_get_type (), &drawing_area_info);
    }

  return drawing_area_type;
}

static void
gtk_drawing_area_class_init (GtkDrawingAreaClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->realize = gtk_drawing_area_realize;
  widget_class->size_allocate = gtk_drawing_area_size_allocate;
}

static void
gtk_drawing_area_init (GtkDrawingArea *darea)
{
  GTK_WIDGET_SET_FLAGS (darea, GTK_BASIC);
}


GtkWidget*
gtk_drawing_area_new ()
{
  return GTK_WIDGET (gtk_type_new (gtk_drawing_area_get_type ()));
}

void
gtk_drawing_area_size (GtkDrawingArea *darea,
		       gint            width,
		       gint            height)
{
  g_return_if_fail (darea != NULL);
  g_return_if_fail (GTK_IS_DRAWING_AREA (darea));

  GTK_WIDGET (darea)->requisition.width = width;
  GTK_WIDGET (darea)->requisition.height = height;
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
  attributes.event_mask = gtk_widget_get_events (widget);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, darea);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
gtk_drawing_area_size_allocate (GtkWidget     *widget,
				GtkAllocation *allocation)
{
  GdkEventConfigure event;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_DRAWING_AREA (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      event.type = GDK_CONFIGURE;
      event.window = widget->window;
      event.x = allocation->x;
      event.y = allocation->y;
      event.width = allocation->width;
      event.height = allocation->height;

      gtk_widget_event (widget, (GdkEvent*) &event);
    }
}
