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
#include "gtkbin.h"


static void gtk_bin_class_init (GtkBinClass    *klass);
static void gtk_bin_init       (GtkBin         *bin);
static void gtk_bin_map        (GtkWidget      *widget);
static void gtk_bin_unmap      (GtkWidget      *widget);
static void gtk_bin_draw       (GtkWidget      *widget,
			        GdkRectangle   *area);
static gint gtk_bin_expose     (GtkWidget      *widget,
			        GdkEventExpose *event);
static void gtk_bin_add        (GtkContainer   *container,
			        GtkWidget      *widget);
static void gtk_bin_remove     (GtkContainer   *container,
			        GtkWidget      *widget);
static void gtk_bin_foreach    (GtkContainer   *container,
			        GtkCallback     callback,
			        gpointer        callback_data);
static GtkType gtk_bin_child_type (GtkContainer*container);


static GtkContainerClass *parent_class = NULL;


guint
gtk_bin_get_type (void)
{
  static guint bin_type = 0;

  if (!bin_type)
    {
      GtkTypeInfo bin_info =
      {
	"GtkBin",
	sizeof (GtkBin),
	sizeof (GtkBinClass),
	(GtkClassInitFunc) gtk_bin_class_init,
	(GtkObjectInitFunc) gtk_bin_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      bin_type = gtk_type_unique (gtk_container_get_type (), &bin_info);
    }

  return bin_type;
}

static void
gtk_bin_class_init (GtkBinClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = gtk_type_class (gtk_container_get_type ());

  widget_class->map = gtk_bin_map;
  widget_class->unmap = gtk_bin_unmap;
  widget_class->draw = gtk_bin_draw;
  widget_class->expose_event = gtk_bin_expose;

  container_class->add = gtk_bin_add;
  container_class->remove = gtk_bin_remove;
  container_class->foreach = gtk_bin_foreach;
  container_class->child_type = gtk_bin_child_type;
}

static void
gtk_bin_init (GtkBin *bin)
{
  GTK_WIDGET_SET_FLAGS (bin, GTK_NO_WINDOW);

  bin->child = NULL;
}


static GtkType
gtk_bin_child_type (GtkContainer *container)
{
  if (!GTK_BIN (container)->child)
    return GTK_TYPE_WIDGET;
  else
    return GTK_TYPE_NONE;
}

static void
gtk_bin_map (GtkWidget *widget)
{
  GtkBin *bin;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_BIN (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
  bin = GTK_BIN (widget);

  if (!GTK_WIDGET_NO_WINDOW (widget))
    gdk_window_show (widget->window);
  else
    gtk_widget_queue_draw (widget);

  if (bin->child &&
      GTK_WIDGET_VISIBLE (bin->child) &&
      !GTK_WIDGET_MAPPED (bin->child))
    gtk_widget_map (bin->child);
}

static void
gtk_bin_unmap (GtkWidget *widget)
{
  GtkBin *bin;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_BIN (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  bin = GTK_BIN (widget);

  if (GTK_WIDGET_NO_WINDOW (widget))
    gdk_window_clear_area (widget->window,
			   widget->allocation.x,
			   widget->allocation.y,
			   widget->allocation.width,
			   widget->allocation.height);
  else
    gdk_window_hide (widget->window);

  if (bin->child &&
      GTK_WIDGET_VISIBLE (bin->child) &&
      GTK_WIDGET_MAPPED (bin->child))
    gtk_widget_unmap (bin->child);
}

static void
gtk_bin_draw (GtkWidget    *widget,
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

static gint
gtk_bin_expose (GtkWidget      *widget,
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
gtk_bin_add (GtkContainer *container,
	     GtkWidget    *widget)
{
  GtkBin *bin;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_BIN (container));
  g_return_if_fail (widget != NULL);

  bin = GTK_BIN (container);
  g_return_if_fail (bin->child == NULL);

  gtk_widget_set_parent (widget, GTK_WIDGET (container));
  
  if (GTK_WIDGET_VISIBLE (widget->parent))
    {
      if (GTK_WIDGET_REALIZED (widget->parent) &&
	  !GTK_WIDGET_REALIZED (widget))
	gtk_widget_realize (widget);
      
      if (GTK_WIDGET_MAPPED (widget->parent) &&
	  !GTK_WIDGET_MAPPED (widget))
	gtk_widget_map (widget);
    }
  
  bin->child = widget;
  
  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_VISIBLE (container))
    gtk_widget_queue_resize (widget);
}

static void
gtk_bin_remove (GtkContainer *container,
		GtkWidget    *widget)
{
  GtkBin *bin;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_BIN (container));
  g_return_if_fail (widget != NULL);

  bin = GTK_BIN (container);

  if (bin->child == widget)
    {
      gboolean widget_was_visible = GTK_WIDGET_VISIBLE (widget);

      gtk_widget_unparent (widget);
      bin->child = NULL;

      /* queue resize regardless of GTK_WIDGET_VISIBLE (container),
       * since that's what is needed by toplevels, which derive from GtkBin.
       */
      if (widget_was_visible)
        gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

static void
gtk_bin_foreach (GtkContainer *container,
		 GtkCallback   callback,
		 gpointer      callback_data)
{
  GtkBin *bin;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_BIN (container));
  g_return_if_fail (callback != NULL);

  bin = GTK_BIN (container);

  if (bin->child)
    (* callback) (bin->child, callback_data);
}
