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
#include "gtkpaned.h"


static void gtk_paned_class_init (GtkPanedClass    *klass);
static void gtk_paned_init       (GtkPaned         *paned);
static void gtk_paned_realize    (GtkWidget *widget);
static void gtk_paned_map        (GtkWidget      *widget);
static void gtk_paned_unmap      (GtkWidget      *widget);
static void gtk_paned_unrealize  (GtkWidget *widget);
static gint gtk_paned_expose     (GtkWidget      *widget,
			        GdkEventExpose *event);
static void gtk_paned_add        (GtkContainer   *container,
			        GtkWidget      *widget);
static void gtk_paned_remove     (GtkContainer   *container,
			        GtkWidget      *widget);
static void gtk_paned_foreach    (GtkContainer   *container,
			        GtkCallback     callback,
			        gpointer        callback_data);


static GtkContainerClass *parent_class = NULL;


guint
gtk_paned_get_type ()
{
  static guint paned_type = 0;

  if (!paned_type)
    {
      GtkTypeInfo paned_info =
      {
	"GtkPaned",
	sizeof (GtkPaned),
	sizeof (GtkPanedClass),
	(GtkClassInitFunc) gtk_paned_class_init,
	(GtkObjectInitFunc) gtk_paned_init,
	(GtkArgSetFunc) NULL,
	(GtkArgGetFunc) NULL,
      };

      paned_type = gtk_type_unique (gtk_container_get_type (), &paned_info);
    }

  return paned_type;
}

static void
gtk_paned_class_init (GtkPanedClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = gtk_type_class (gtk_container_get_type ());

  widget_class->realize = gtk_paned_realize;
  widget_class->map = gtk_paned_map;
  widget_class->unmap = gtk_paned_unmap;
  widget_class->unrealize = gtk_paned_unrealize;
  widget_class->expose_event = gtk_paned_expose;

  container_class->add = gtk_paned_add;
  container_class->remove = gtk_paned_remove;
  container_class->foreach = gtk_paned_foreach;
}

static void
gtk_paned_init (GtkPaned *paned)
{
  GTK_WIDGET_SET_FLAGS (paned, GTK_NO_WINDOW);

  paned->child1 = NULL;
  paned->child2 = NULL;
  paned->handle = NULL;
  paned->xor_gc = NULL;

  paned->handle_size = 10;
  paned->gutter_size = 6;
  paned->position_set = FALSE;
  paned->in_drag = FALSE;

  paned->handle_xpos = -1;
  paned->handle_ypos = -1;
}


static void
gtk_paned_realize (GtkWidget *widget)
{
  GtkPaned *paned;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PANED (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  paned = GTK_PANED (widget);

  attributes.x = paned->handle_xpos;
  attributes.y = paned->handle_ypos;
  attributes.width = paned->handle_size;
  attributes.height = paned->handle_size;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.cursor = paned->cursor = gdk_cursor_new (GDK_CROSS);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_POINTER_MOTION_MASK |
			    GDK_POINTER_MOTION_HINT_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP |
    GDK_WA_CURSOR;

  paned->handle = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (paned->handle, widget);
  gdk_window_show (paned->handle);
  gdk_window_raise (paned->handle);

  widget->window = gtk_widget_get_parent_window (widget);
  gdk_window_ref (widget->window);
  widget->style = gtk_style_attach (widget->style, widget->window);

  gtk_style_set_background (widget->style, paned->handle, GTK_STATE_NORMAL);
}

static void
gtk_paned_map (GtkWidget *widget)
{
  GtkPaned *paned;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PANED (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
  paned = GTK_PANED (widget);

  gdk_window_show (paned->handle);
  gtk_widget_queue_draw (widget);

  if (paned->child1 &&
      GTK_WIDGET_VISIBLE (paned->child1) &&
      !GTK_WIDGET_MAPPED (paned->child1))
    gtk_widget_map (paned->child1);
  if (paned->child2 &&
      GTK_WIDGET_VISIBLE (paned->child2) &&
      !GTK_WIDGET_MAPPED (paned->child2))
    gtk_widget_map (paned->child2);
}

static void
gtk_paned_unmap (GtkWidget *widget)
{
  GtkPaned *paned;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PANED (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  paned = GTK_PANED (widget);

  gdk_window_clear_area (widget->window,
			 widget->allocation.x,
			 widget->allocation.y,
			 widget->allocation.width,
			 widget->allocation.height);
  gdk_window_hide (paned->handle);

  if (paned->child1 &&
      GTK_WIDGET_VISIBLE (paned->child1) &&
      GTK_WIDGET_MAPPED (paned->child1))
    gtk_widget_unmap (paned->child1);
  if (paned->child2 &&
      GTK_WIDGET_VISIBLE (paned->child2) &&
      GTK_WIDGET_MAPPED (paned->child2))
    gtk_widget_unmap (paned->child2);
}

static void
gtk_paned_unrealize (GtkWidget *widget)
{
  GtkPaned *paned;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PANED (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_REALIZED);
  paned = GTK_PANED (widget);

  gtk_style_detach (widget->style);

  if (paned->xor_gc)
    gdk_gc_destroy (paned->xor_gc);

  if (paned->handle)
    {
      gdk_window_set_user_data (paned->handle, NULL);
      gdk_window_destroy (paned->handle);
      paned->handle = NULL;
      gdk_cursor_destroy (paned->cursor);
      paned->cursor = NULL;
    }

  if (widget->window)
    {
      gdk_window_unref (widget->window);
      widget->window = NULL;
    }
}

static gint
gtk_paned_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkPaned *paned;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_PANED (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      paned = GTK_PANED (widget);

      /* An expose event for the handle */
      if (event->window == paned->handle)
	{
	  gdk_window_set_background (paned->handle,
				     &widget->style->bg[widget->state]);
	  gdk_window_clear (paned->handle);
	  gtk_draw_shadow (widget->style, paned->handle,
			   GTK_WIDGET_STATE(widget),
			   GTK_SHADOW_OUT, 0, 0,
			   paned->handle_size, paned->handle_size);
	}
      else
	{
	  child_event = *event;
	  if (paned->child1 &&
	      GTK_WIDGET_NO_WINDOW (paned->child1) &&
	      gtk_widget_intersect (paned->child1, &event->area, &child_event.area))
	    gtk_widget_event (paned->child1, (GdkEvent*) &child_event);

	  if (paned->child2 &&
	      GTK_WIDGET_NO_WINDOW (paned->child2) &&
	      gtk_widget_intersect (paned->child2, &event->area, &child_event.area))
	    gtk_widget_event (paned->child2, (GdkEvent*) &child_event);

	  /* redraw the groove if necessary */
	  child_event.area = paned->groove_rectangle;
	  if (gtk_widget_intersect (widget, &event->area, &child_event.area))
	    gtk_widget_draw (widget, &child_event.area);
	}
    }
  return FALSE;
}

void
gtk_paned_add1 (GtkPaned     *paned,
		GtkWidget    *widget)
{
  g_return_if_fail (widget != NULL);

  if (!paned->child1)
    {
      gtk_widget_set_parent (widget, GTK_WIDGET (paned));

      if (GTK_WIDGET_VISIBLE (widget->parent))
	{
	  if (GTK_WIDGET_REALIZED (widget->parent) &&
	      !GTK_WIDGET_REALIZED (widget))
	    gtk_widget_realize (widget);
	  
	  if (GTK_WIDGET_MAPPED (widget->parent) &&
	      !GTK_WIDGET_MAPPED (widget))
	    gtk_widget_map (widget);
	}

      paned->child1 = widget;

      if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_VISIBLE (paned))
        gtk_widget_queue_resize (widget);
    }
}

void
gtk_paned_add2 (GtkPaned  *paned,
		GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);

  if (!paned->child2)
    {
      gtk_widget_set_parent (widget, GTK_WIDGET (paned));

      if (GTK_WIDGET_VISIBLE (widget->parent))
	{
	  if (GTK_WIDGET_REALIZED (widget->parent) &&
	      !GTK_WIDGET_REALIZED (widget))
	    gtk_widget_realize (widget);
	  
	  if (GTK_WIDGET_MAPPED (widget->parent) &&
	      !GTK_WIDGET_MAPPED (widget))
	    gtk_widget_map (widget);
	}

      paned->child2 = widget;

      if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_VISIBLE (paned))
        gtk_widget_queue_resize (widget);
    }
}

static void
gtk_paned_add (GtkContainer *container,
	       GtkWidget    *widget)
{
  GtkPaned *paned;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_PANED (container));
  g_return_if_fail (widget != NULL);

  paned = GTK_PANED (container);

  if (!paned->child1)
    gtk_paned_add1 (GTK_PANED (container),widget);
  else if (!paned->child2)
    gtk_paned_add2 (GTK_PANED (container),widget);
}

static void
gtk_paned_remove (GtkContainer *container,
		  GtkWidget    *widget)
{
  GtkPaned *paned;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_PANED (container));
  g_return_if_fail (widget != NULL);

  paned = GTK_PANED (container);

  if (paned->child1 == widget)
    {
      gtk_widget_unparent (widget);

      paned->child1 = NULL;

      if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_VISIBLE (container))
        gtk_widget_queue_resize (GTK_WIDGET (container));
    }
  else if (paned->child2 == widget)
    {
      gtk_widget_unparent (widget);

      paned->child2 = NULL;

      if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_VISIBLE (container))
        gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

static void
gtk_paned_foreach (GtkContainer *container,
		 GtkCallback   callback,
		 gpointer      callback_data)
{
  GtkPaned *paned;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_PANED (container));
  g_return_if_fail (callback != NULL);

  paned = GTK_PANED (container);

  if (paned->child1)
    (* callback) (paned->child1, callback_data);
  if (paned->child2)
    (* callback) (paned->child2, callback_data);
}

void
gtk_paned_handle_size (GtkPaned *paned, guint16 size)
{
  gint x,y;

  if (paned->handle)
    {
      gdk_window_get_geometry (paned->handle, &x, &y, NULL, NULL, NULL);
      gdk_window_move_resize (paned->handle,
                              x + paned->handle_size / 2 - size / 2,
                              y + paned->handle_size / 2 - size / 2,
                              size, size);
    }
  
  paned->handle_size = size;
}

void
gtk_paned_gutter_size (GtkPaned *paned, guint16 size)
{
  paned->gutter_size = size;

  if (GTK_WIDGET_VISIBLE (GTK_WIDGET (paned)))
    gtk_widget_queue_resize (GTK_WIDGET (paned));
}
