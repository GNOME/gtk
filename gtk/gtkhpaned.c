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

#include <config.h>
#include "gtkhpaned.h"
#include "gtkintl.h"
#include "gtkalias.h"

static void     gtk_hpaned_size_request   (GtkWidget      *widget,
					   GtkRequisition *requisition);
static void     gtk_hpaned_size_allocate  (GtkWidget      *widget,
					   GtkAllocation  *allocation);

G_DEFINE_TYPE (GtkHPaned, gtk_hpaned, GTK_TYPE_PANED)

static void
gtk_hpaned_class_init (GtkHPanedClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass *) class;

  widget_class->size_request = gtk_hpaned_size_request;
  widget_class->size_allocate = gtk_hpaned_size_allocate;
}

static void
gtk_hpaned_init (GtkHPaned *hpaned)
{
  GtkPaned *paned;

  g_return_if_fail (GTK_IS_PANED (hpaned));

  paned = GTK_PANED (hpaned);
  
  paned->cursor_type = GDK_SB_H_DOUBLE_ARROW;
  paned->orientation = GTK_ORIENTATION_VERTICAL;
}

GtkWidget *
gtk_hpaned_new (void)
{
  GtkHPaned *hpaned;

  hpaned = g_object_new (GTK_TYPE_HPANED, NULL);

  return GTK_WIDGET (hpaned);
}

static void
gtk_hpaned_size_request (GtkWidget      *widget,
			 GtkRequisition *requisition)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkRequisition child_requisition;

  requisition->width = 0;
  requisition->height = 0;

  if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1))
    {
      gtk_widget_size_request (paned->child1, &child_requisition);

      requisition->height = child_requisition.height;
      requisition->width = child_requisition.width;
    }

  if (paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
    {
      gtk_widget_size_request (paned->child2, &child_requisition);

      requisition->height = MAX(requisition->height, child_requisition.height);
      requisition->width += child_requisition.width;
    }

  requisition->width += GTK_CONTAINER (paned)->border_width * 2;
  requisition->height += GTK_CONTAINER (paned)->border_width * 2;

  if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1) &&
      paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
    {
      gint handle_size;
      
      gtk_widget_style_get (widget, "handle-size", &handle_size, NULL);
      requisition->width += handle_size;
    }
}

static void
flip_child (GtkWidget *widget, GtkAllocation *child_pos)
{
  gint x = widget->allocation.x;
  gint width = widget->allocation.width;

  child_pos->x = 2 * x + width - child_pos->x - child_pos->width;
}

static void
gtk_hpaned_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkPaned *paned = GTK_PANED (widget);
  gint border_width = GTK_CONTAINER (paned)->border_width;

  widget->allocation = *allocation;

  if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1) &&
      paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
    {
      GtkAllocation child1_allocation;
      GtkAllocation child2_allocation;
      GtkRequisition child1_requisition;
      GtkRequisition child2_requisition;
      gint handle_size;
      
      gtk_widget_style_get (widget, "handle-size", &handle_size, NULL);

      gtk_widget_get_child_requisition (paned->child1, &child1_requisition);
      gtk_widget_get_child_requisition (paned->child2, &child2_requisition);
      
      gtk_paned_compute_position (paned,
				  MAX (1, widget->allocation.width
				       - handle_size
				       - 2 * border_width),
				  child1_requisition.width,
				  child2_requisition.width);
      
      paned->handle_pos.x = widget->allocation.x + paned->child1_size + border_width;
      paned->handle_pos.y = widget->allocation.y + border_width;
      paned->handle_pos.width = handle_size;
      paned->handle_pos.height = MAX (1, widget->allocation.height - 2 * border_width);

      child1_allocation.height = child2_allocation.height = MAX (1, (gint) allocation->height - border_width * 2);
      child1_allocation.width = MAX (1, paned->child1_size);
      child1_allocation.x = widget->allocation.x + border_width;
      child1_allocation.y = child2_allocation.y = widget->allocation.y + border_width;
      
      child2_allocation.x = child1_allocation.x + paned->child1_size + paned->handle_pos.width;
      child2_allocation.width = MAX (1, widget->allocation.x + widget->allocation.width - child2_allocation.x - border_width);

      if (gtk_widget_get_direction (GTK_WIDGET (widget)) == GTK_TEXT_DIR_RTL)
	{
	  flip_child (widget, &(child2_allocation));
	  flip_child (widget, &(child1_allocation));
	  flip_child (widget, &(paned->handle_pos));
	}
      
      if (GTK_WIDGET_REALIZED (widget))
	{
	  if (GTK_WIDGET_MAPPED (widget))
	    gdk_window_show (paned->handle);
	  gdk_window_move_resize (paned->handle,
				  paned->handle_pos.x,
				  paned->handle_pos.y,
				  handle_size,
				  paned->handle_pos.height);
	}
      
      /* Now allocate the childen, making sure, when resizing not to
       * overlap the windows
       */
      if (GTK_WIDGET_MAPPED (widget) &&
	  paned->child1->allocation.width < child1_allocation.width)
	{
	  gtk_widget_size_allocate (paned->child2, &child2_allocation);
	  gtk_widget_size_allocate (paned->child1, &child1_allocation);
	}
      else
	{
	  gtk_widget_size_allocate (paned->child1, &child1_allocation);
	  gtk_widget_size_allocate (paned->child2, &child2_allocation);
	}
    }
  else
    {
      GtkAllocation child_allocation;

      if (GTK_WIDGET_REALIZED (widget))      
	gdk_window_hide (paned->handle);

      if (paned->child1)
	gtk_widget_set_child_visible (paned->child1, TRUE);
      if (paned->child2)
	gtk_widget_set_child_visible (paned->child2, TRUE);

      child_allocation.x = widget->allocation.x + border_width;
      child_allocation.y = widget->allocation.y + border_width;
      child_allocation.width = MAX (1, allocation->width - 2 * border_width);
      child_allocation.height = MAX (1, allocation->height - 2 * border_width);
      
      if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1))
	gtk_widget_size_allocate (paned->child1, &child_allocation);
      else if (paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
	gtk_widget_size_allocate (paned->child2, &child_allocation);
    }
}

#define __GTK_HPANED_C__
#include "gtkaliasdef.c"
