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
#include "gtkbox.h"


static void gtk_box_class_init (GtkBoxClass    *klass);
static void gtk_box_init       (GtkBox         *box);
static void gtk_box_destroy    (GtkObject      *object);
static void gtk_box_map        (GtkWidget      *widget);
static void gtk_box_unmap      (GtkWidget      *widget);
static void gtk_box_draw       (GtkWidget      *widget,
			        GdkRectangle   *area);
static gint gtk_box_expose     (GtkWidget      *widget,
			        GdkEventExpose *event);
static void gtk_box_add        (GtkContainer   *container,
			        GtkWidget      *widget);
static void gtk_box_remove     (GtkContainer   *container,
			        GtkWidget      *widget);
static void gtk_box_foreach    (GtkContainer   *container,
			        GtkCallback     callback,
			        gpointer        callback_data);


static GtkContainerClass *parent_class = NULL;


guint
gtk_box_get_type ()
{
  static guint box_type = 0;

  if (!box_type)
    {
      GtkTypeInfo box_info =
      {
	"GtkBox",
	sizeof (GtkBox),
	sizeof (GtkBoxClass),
	(GtkClassInitFunc) gtk_box_class_init,
	(GtkObjectInitFunc) gtk_box_init,
	(GtkArgFunc) NULL,
      };

      box_type = gtk_type_unique (gtk_container_get_type (), &box_info);
    }

  return box_type;
}

static void
gtk_box_class_init (GtkBoxClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  parent_class = gtk_type_class (gtk_container_get_type ());

  object_class->destroy = gtk_box_destroy;

  widget_class->map = gtk_box_map;
  widget_class->unmap = gtk_box_unmap;
  widget_class->draw = gtk_box_draw;
  widget_class->expose_event = gtk_box_expose;

  container_class->add = gtk_box_add;
  container_class->remove = gtk_box_remove;
  container_class->foreach = gtk_box_foreach;
}

static void
gtk_box_init (GtkBox *box)
{
  GTK_WIDGET_SET_FLAGS (box, GTK_NO_WINDOW | GTK_BASIC);

  box->children = NULL;
  box->spacing = 0;
  box->homogeneous = FALSE;
}

void
gtk_box_pack_start (GtkBox    *box,
		    GtkWidget *child,
		    gint       expand,
		    gint       fill,
		    gint       padding)
{
  GtkBoxChild *child_info;

  g_return_if_fail (box != NULL);
  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (child != NULL);

  child_info = g_new (GtkBoxChild, 1);
  child_info->widget = child;
  child_info->padding = padding;
  child_info->expand = expand ? TRUE : FALSE;
  child_info->fill = fill ? TRUE : FALSE;
  child_info->pack = GTK_PACK_START;

  box->children = g_list_append (box->children, child_info);

  gtk_widget_set_parent (child, GTK_WIDGET (box));

  if (GTK_WIDGET_VISIBLE (GTK_WIDGET (box)))
    {
      if (GTK_WIDGET_REALIZED (GTK_WIDGET (box)) &&
	  !GTK_WIDGET_REALIZED (child))
	gtk_widget_realize (child);
      
      if (GTK_WIDGET_MAPPED (GTK_WIDGET (box)) &&
	  !GTK_WIDGET_MAPPED (child))
	gtk_widget_map (child);
    }

  if (GTK_WIDGET_VISIBLE (child) && GTK_WIDGET_VISIBLE (box))
    gtk_widget_queue_resize (child);
}

void
gtk_box_pack_end (GtkBox    *box,
		  GtkWidget *child,
		  gint       expand,
		  gint       fill,
		  gint       padding)
{
  GtkBoxChild *child_info;

  g_return_if_fail (box != NULL);
  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (child != NULL);

  child_info = g_new (GtkBoxChild, 1);
  child_info->widget = child;
  child_info->padding = padding;
  child_info->expand = expand ? TRUE : FALSE;
  child_info->fill = fill ? TRUE : FALSE;
  child_info->pack = GTK_PACK_END;

  box->children = g_list_append (box->children, child_info);

  gtk_widget_set_parent (child, GTK_WIDGET (box));

  if (GTK_WIDGET_VISIBLE (GTK_WIDGET (box)))
    {
      if (GTK_WIDGET_REALIZED (GTK_WIDGET (box)) &&
	  !GTK_WIDGET_REALIZED (child))
	gtk_widget_realize (child);
      
      if (GTK_WIDGET_MAPPED (GTK_WIDGET (box)) &&
	  !GTK_WIDGET_MAPPED (child))
	gtk_widget_map (child);
    }
  
  if (GTK_WIDGET_VISIBLE (child) && GTK_WIDGET_VISIBLE (box))
    gtk_widget_queue_resize (child);
}

void
gtk_box_pack_start_defaults (GtkBox    *box,
			     GtkWidget *child)
{
  g_return_if_fail (box != NULL);
  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (child != NULL);

  gtk_box_pack_start (box, child, TRUE, TRUE, 0);
}

void
gtk_box_pack_end_defaults (GtkBox    *box,
			   GtkWidget *child)
{
  g_return_if_fail (box != NULL);
  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (child != NULL);

  gtk_box_pack_end (box, child, TRUE, TRUE, 0);
}

void
gtk_box_set_homogeneous (GtkBox *box,
			 gint    homogeneous)
{
  g_return_if_fail (box != NULL);
  g_return_if_fail (GTK_IS_BOX (box));

  if ((homogeneous ? TRUE : FALSE) != box->homogeneous)
    {
      box->homogeneous = homogeneous ? TRUE : FALSE;
      gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}

void
gtk_box_set_spacing (GtkBox *box,
		     gint    spacing)
{
  g_return_if_fail (box != NULL);
  g_return_if_fail (GTK_IS_BOX (box));

  if (spacing != box->spacing)
    {
      box->spacing = spacing;
      gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}


static void
gtk_box_destroy (GtkObject *object)
{
  GtkBox *box;
  GtkBoxChild *child;
  GList *children;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_BOX (object));

  box = GTK_BOX (object);

  children = box->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      child->widget->parent = NULL;
      gtk_object_unref (GTK_OBJECT (child->widget));
      gtk_widget_destroy (child->widget);
      g_free (child);
    }

  g_list_free (box->children);

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_box_map (GtkWidget *widget)
{
  GtkBox *box;
  GtkBoxChild *child;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_BOX (widget));

  box = GTK_BOX (widget);
  GTK_WIDGET_SET_FLAGS (box, GTK_MAPPED);

  children = box->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget) &&
	  !GTK_WIDGET_MAPPED (child->widget))
	gtk_widget_map (child->widget);
    }
}

static void
gtk_box_unmap (GtkWidget *widget)
{
  GtkBox *box;
  GtkBoxChild *child;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_BOX (widget));

  box = GTK_BOX (widget);
  GTK_WIDGET_UNSET_FLAGS (box, GTK_MAPPED);

  children = box->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget) &&
	  GTK_WIDGET_MAPPED (child->widget))
	gtk_widget_unmap (child->widget);
    }
}

static void
gtk_box_draw (GtkWidget    *widget,
	      GdkRectangle *area)
{
  GtkBox *box;
  GtkBoxChild *child;
  GdkRectangle child_area;
  GList *children;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_BOX (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      box = GTK_BOX (widget);

      children = box->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (gtk_widget_intersect (child->widget, area, &child_area))
	    gtk_widget_draw (child->widget, &child_area);
	}
    }
}

static gint
gtk_box_expose (GtkWidget      *widget,
		GdkEventExpose *event)
{
  GtkBox *box;
  GtkBoxChild *child;
  GdkEventExpose child_event;
  GList *children;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_BOX (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      box = GTK_BOX (widget);

      child_event = *event;

      children = box->children;
      while (children)
	{
	  child = children->data;
	  children = children->next;

	  if (GTK_WIDGET_NO_WINDOW (child->widget) &&
	      gtk_widget_intersect (child->widget, &event->area, &child_event.area))
	    gtk_widget_event (child->widget, (GdkEvent*) &child_event);
	}
    }

  return FALSE;
}

static void
gtk_box_add (GtkContainer *container,
	     GtkWidget    *widget)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_BOX (container));
  g_return_if_fail (widget != NULL);

  gtk_box_pack_start_defaults (GTK_BOX (container), widget);
}

static void
gtk_box_remove (GtkContainer *container,
		GtkWidget    *widget)
{
  GtkBox *box;
  GtkBoxChild *child;
  GList *children;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_BOX (container));
  g_return_if_fail (widget != NULL);

  box = GTK_BOX (container);

  children = box->children;
  while (children)
    {
      child = children->data;

      if (child->widget == widget)
	{
	  gtk_widget_unparent (widget);

	  box->children = g_list_remove_link (box->children, children);
	  g_list_free (children);
	  g_free (child);

	  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_VISIBLE (container))
	    gtk_widget_queue_resize (GTK_WIDGET (container));

	  break;
	}

      children = children->next;
    }
}

static void
gtk_box_foreach (GtkContainer *container,
		 GtkCallback   callback,
		 gpointer      callback_data)
{
  GtkBox *box;
  GtkBoxChild *child;
  GList *children;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_BOX (container));
  g_return_if_fail (callback != NULL);

  box = GTK_BOX (container);

  children = box->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      if (child->pack == GTK_PACK_START)
	(* callback) (child->widget, callback_data);
    }

  children = g_list_last (box->children);
  while (children)
    {
      child = children->data;
      children = children->prev;

      if (child->pack == GTK_PACK_END)
	(* callback) (child->widget, callback_data);
    }
}
