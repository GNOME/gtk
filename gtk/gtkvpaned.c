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
#include "gtkvpaned.h"
#include "gtkintl.h"
#include "gtkalias.h"

static void     gtk_vpaned_size_request   (GtkWidget      *widget,
					   GtkRequisition *requisition);
static void     gtk_vpaned_size_allocate  (GtkWidget      *widget,
					   GtkAllocation  *allocation);

G_DEFINE_TYPE (GtkVPaned, gtk_vpaned, GTK_TYPE_PANED)

static void
gtk_vpaned_class_init (GtkVPanedClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass *) class;

  widget_class->size_request = gtk_vpaned_size_request;
  widget_class->size_allocate = gtk_vpaned_size_allocate;
}

static void
gtk_vpaned_init (GtkVPaned *vpaned)
{
  GtkPaned *paned;

  g_return_if_fail (GTK_IS_PANED (vpaned));

  paned = GTK_PANED (vpaned);

  paned->cursor_type = GDK_SB_V_DOUBLE_ARROW;
  paned->orientation = GTK_ORIENTATION_HORIZONTAL;
}

GtkWidget *
gtk_vpaned_new (void)
{
  GtkVPaned *vpaned;

  vpaned = g_object_new (GTK_TYPE_VPANED, NULL);

  return GTK_WIDGET (vpaned);
}

static void
gtk_vpaned_size_request (GtkWidget      *widget,
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

      requisition->width = MAX (requisition->width, child_requisition.width);
      requisition->height += child_requisition.height;
    }

  requisition->height += GTK_CONTAINER (paned)->border_width * 2;
  requisition->width += GTK_CONTAINER (paned)->border_width * 2;
  
  if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1) &&
      paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
    {
      gint handle_size;

      gtk_widget_style_get (widget, "handle-size", &handle_size, NULL);
      requisition->height += handle_size;
    }
}

static void
gtk_vpaned_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkPaned *paned = GTK_PANED (widget);
  gint border_width = GTK_CONTAINER (paned)->border_width;

  widget->allocation = *allocation;

  if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1) &&
      paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
    {
      GtkRequisition child1_requisition;
      GtkRequisition child2_requisition;
      GtkAllocation child1_allocation;
      GtkAllocation child2_allocation;
      gint handle_size;
      
      gtk_widget_style_get (widget, "handle-size", &handle_size, NULL);

      gtk_widget_get_child_requisition (paned->child1, &child1_requisition);
      gtk_widget_get_child_requisition (paned->child2, &child2_requisition);
    
      gtk_paned_compute_position (paned,
				  MAX (1, widget->allocation.height
				       - handle_size
				       - 2 * border_width),
				  child1_requisition.height,
				  child2_requisition.height);

      paned->handle_pos.x = widget->allocation.x + border_width;
      paned->handle_pos.y = widget->allocation.y + paned->child1_size + border_width;
      paned->handle_pos.width = MAX (1, (gint) widget->allocation.width - 2 * border_width);
      paned->handle_pos.height = handle_size;
      
      if (GTK_WIDGET_REALIZED (widget))
	{
	  if (GTK_WIDGET_MAPPED (widget))
	    gdk_window_show (paned->handle);
	  gdk_window_move_resize (paned->handle,
				  paned->handle_pos.x,
				  paned->handle_pos.y,
				  paned->handle_pos.width,
				  handle_size);
	}

      child1_allocation.width = child2_allocation.width = MAX (1, (gint) allocation->width - border_width * 2);
      child1_allocation.height = MAX (1, paned->child1_size);
      child1_allocation.x = child2_allocation.x = widget->allocation.x + border_width;
      child1_allocation.y = widget->allocation.y + border_width;
      
      child2_allocation.y = child1_allocation.y + paned->child1_size + paned->handle_pos.height;
      child2_allocation.height = MAX (1, widget->allocation.y + widget->allocation.height - child2_allocation.y - border_width);
      
      /* Now allocate the childen, making sure, when resizing not to
       * overlap the windows
       */
      if (GTK_WIDGET_MAPPED (widget) &&
	  paned->child1->allocation.height < child1_allocation.height)
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

#define __GTK_VPANED_C__
#include "gtkaliasdef.c"
