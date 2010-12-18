/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include "gtkdrawingarea.h"
#include "gtkintl.h"


static void gtk_drawing_area_realize       (GtkWidget           *widget);
static void gtk_drawing_area_size_allocate (GtkWidget           *widget,
					    GtkAllocation       *allocation);
static void gtk_drawing_area_send_configure (GtkDrawingArea     *darea);

G_DEFINE_TYPE (GtkDrawingArea, gtk_drawing_area, GTK_TYPE_WIDGET)

static void
gtk_drawing_area_class_init (GtkDrawingAreaClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  widget_class->realize = gtk_drawing_area_realize;
  widget_class->size_allocate = gtk_drawing_area_size_allocate;
}

static void
gtk_drawing_area_init (GtkDrawingArea *darea)
{
}


GtkWidget*
gtk_drawing_area_new (void)
{
  return g_object_new (GTK_TYPE_DRAWING_AREA, NULL);
}

static void
gtk_drawing_area_realize (GtkWidget *widget)
{
  GtkDrawingArea *darea = GTK_DRAWING_AREA (widget);
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  if (!gtk_widget_get_has_window (widget))
    {
      GTK_WIDGET_CLASS (gtk_drawing_area_parent_class)->realize (widget);
    }
  else
    {
      gtk_widget_set_realized (widget, TRUE);

      gtk_widget_get_allocation (widget, &allocation);

      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.x = allocation.x;
      attributes.y = allocation.y;
      attributes.width = allocation.width;
      attributes.height = allocation.height;
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;

      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

      window = gdk_window_new (gtk_widget_get_parent_window (widget),
                               &attributes, attributes_mask);
      gdk_window_set_user_data (window, darea);
      gtk_widget_set_window (widget, window);

      gtk_widget_style_attach (widget);
      gtk_style_set_background (gtk_widget_get_style (widget), window, GTK_STATE_NORMAL);
    }

  gtk_drawing_area_send_configure (GTK_DRAWING_AREA (widget));
}

static void
gtk_drawing_area_size_allocate (GtkWidget     *widget,
				GtkAllocation *allocation)
{
  g_return_if_fail (GTK_IS_DRAWING_AREA (widget));
  g_return_if_fail (allocation != NULL);

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    {
      if (gtk_widget_get_has_window (widget))
        gdk_window_move_resize (gtk_widget_get_window (widget),
                                allocation->x, allocation->y,
                                allocation->width, allocation->height);

      gtk_drawing_area_send_configure (GTK_DRAWING_AREA (widget));
    }
}

static void
gtk_drawing_area_send_configure (GtkDrawingArea *darea)
{
  GtkAllocation allocation;
  GtkWidget *widget;
  GdkEvent *event = gdk_event_new (GDK_CONFIGURE);

  widget = GTK_WIDGET (darea);
  gtk_widget_get_allocation (widget, &allocation);

  event->configure.window = g_object_ref (gtk_widget_get_window (widget));
  event->configure.send_event = TRUE;
  event->configure.x = allocation.x;
  event->configure.y = allocation.y;
  event->configure.width = allocation.width;
  event->configure.height = allocation.height;
  
  gtk_widget_event (widget, event);
  gdk_event_free (event);
}
