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
#include "gtkdrawwindow.h"

static void gtk_draw_window_class_init (GtkDrawWindowClass *klass);
static void gtk_draw_window_init       (GtkDrawWindow      *draw_window);
static void gtk_draw_window_draw       (GtkWidget          *widget,
					GdkRectangle       *area);
static gint gtk_draw_window_expose     (GtkWidget          *widget,
					GdkEventExpose     *event);


GtkType
gtk_draw_window_get_type (void)
{
  static GtkType draw_window_type = 0;

  if (!draw_window_type)
    {
      static const GtkTypeInfo draw_window_info =
      {
	"GtkDrawWindow",
	sizeof (GtkDrawWindow),
	sizeof (GtkDrawWindowClass),
	(GtkClassInitFunc) gtk_draw_window_class_init,
	(GtkObjectInitFunc) gtk_draw_window_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      draw_window_type = gtk_type_unique (GTK_TYPE_WINDOW, &draw_window_info);
    }

  return draw_window_type;
}

static void
gtk_draw_window_class_init (GtkDrawWindowClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->draw = gtk_draw_window_draw;
  widget_class->expose_event = gtk_draw_window_expose;
}

static void
gtk_draw_window_init (GtkDrawWindow *draw_window)
{
}

static gint
gtk_draw_window_expose (GtkWidget      *widget,
			GdkEventExpose *event)
{
  GtkBin *bin;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_BIN (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);

      child_event = *event;
      if (bin->child &&
	  GTK_WIDGET_NO_WINDOW (bin->child) &&
	  gtk_widget_intersect (bin->child, &event->area, &child_event.area))
	gtk_widget_event (bin->child, (GdkEvent*) &child_event);
    }

  return FALSE;
}

static void
gtk_draw_window_draw (GtkWidget    *widget,
		      GdkRectangle *area)
{
  GtkBin *bin;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_BIN (widget));

  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget))
    {
      bin = GTK_BIN (widget);

      if (bin->child &&
	  gtk_widget_intersect (bin->child, area, &child_area))
        gtk_widget_draw (bin->child, &child_area);
    }
}

GtkWidget*
gtk_draw_window_new (GtkWindowType type)
{
  GtkWindow *window;

  window = gtk_type_new (GTK_TYPE_DRAW_WINDOW);

  window->type = type;

  return GTK_WIDGET (window);
}
