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

#include "gtkhpaned.h"

static void     gtk_hpaned_class_init     (GtkHPanedClass *klass);
static void     gtk_hpaned_init           (GtkHPaned      *hpaned);
static void     gtk_hpaned_size_request   (GtkWidget      *widget,
					   GtkRequisition *requisition);
static void     gtk_hpaned_size_allocate  (GtkWidget      *widget,
					   GtkAllocation  *allocation);
static void     gtk_hpaned_xor_line       (GtkPaned       *paned);
static gboolean gtk_hpaned_button_press   (GtkWidget      *widget,
					   GdkEventButton *event);
static gboolean gtk_hpaned_button_release (GtkWidget      *widget,
					   GdkEventButton *event);
static gboolean gtk_hpaned_motion         (GtkWidget      *widget,
					   GdkEventMotion *event);

static gpointer parent_class;

GtkType
gtk_hpaned_get_type (void)
{
  static GtkType hpaned_type = 0;

  if (!hpaned_type)
    {
      static const GtkTypeInfo hpaned_info =
      {
	"GtkHPaned",
	sizeof (GtkHPaned),
	sizeof (GtkHPanedClass),
	(GtkClassInitFunc) gtk_hpaned_class_init,
	(GtkObjectInitFunc) gtk_hpaned_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      hpaned_type = gtk_type_unique (GTK_TYPE_PANED, &hpaned_info);
    }

  return hpaned_type;
}

static void
gtk_hpaned_class_init (GtkHPanedClass *class)
{
  GtkWidgetClass *widget_class;

  parent_class = gtk_type_class (GTK_TYPE_PANED);
  
  widget_class = (GtkWidgetClass *) class;

  widget_class->size_request = gtk_hpaned_size_request;
  widget_class->size_allocate = gtk_hpaned_size_allocate;
  widget_class->button_press_event = gtk_hpaned_button_press;
  widget_class->button_release_event = gtk_hpaned_button_release;
  widget_class->motion_notify_event = gtk_hpaned_motion;
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

  hpaned = gtk_type_new (GTK_TYPE_HPANED);

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
      
      gtk_widget_style_get (widget, "handle_size", &handle_size, NULL);
      requisition->width += handle_size;
    }
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
      
      gtk_widget_style_get (widget, "handle_size", &handle_size, NULL);

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
      
      if (GTK_WIDGET_REALIZED (widget))
	{
	  gdk_window_show (paned->handle);
	  gdk_window_move_resize (paned->handle,
				  paned->handle_pos.x,
				  paned->handle_pos.y,
				  handle_size,
				  paned->handle_pos.height);
	}
      
      child1_allocation.height = child2_allocation.height = MAX (1, (gint) allocation->height - border_width * 2);
      child1_allocation.width = paned->child1_size;
      child1_allocation.x = widget->allocation.x + border_width;
      child1_allocation.y = child2_allocation.y = widget->allocation.y + border_width;
      
      child2_allocation.x = child1_allocation.x + child1_allocation.width +  paned->handle_pos.width;
      child2_allocation.width = MAX (1, (gint) allocation->width - child2_allocation.x - border_width);
      
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

static void
gtk_hpaned_xor_line (GtkPaned *paned)
{
  GtkWidget *widget;
  GdkGCValues values;
  guint16 xpos;
  gint handle_size;

  widget = GTK_WIDGET (paned);

  gtk_widget_style_get (widget, "handle_size", &handle_size, NULL);

  if (!paned->xor_gc)
    {
      values.function = GDK_INVERT;
      values.subwindow_mode = GDK_INCLUDE_INFERIORS;
      paned->xor_gc = gdk_gc_new_with_values (widget->window,
					      &values,
					      GDK_GC_FUNCTION | GDK_GC_SUBWINDOW);
    }

  gdk_gc_set_line_attributes (paned->xor_gc, 2, GDK_LINE_SOLID,
			      GDK_CAP_NOT_LAST, GDK_JOIN_BEVEL);

  xpos = widget->allocation.x + paned->child1_size
    + GTK_CONTAINER (paned)->border_width + handle_size / 2;

  gdk_draw_line (widget->window, paned->xor_gc,
		 xpos,
		 widget->allocation.y,
		 xpos,
		 widget->allocation.y + widget->allocation.height - 1);
}

static gboolean
gtk_hpaned_button_press (GtkWidget      *widget,
			 GdkEventButton *event)
{
  GtkPaned *paned = GTK_PANED (widget);
  gint handle_size;

  gtk_widget_style_get (widget, "handle_size", &handle_size, NULL);

  if (!paned->in_drag &&
      event->window == paned->handle && event->button == 1)
    {
      paned->in_drag = TRUE;
      /* We need a server grab here, not gtk_grab_add(), since
       * we don't want to pass events on to the widget's children */
      gdk_pointer_grab(paned->handle, FALSE,
		       GDK_POINTER_MOTION_HINT_MASK
		       | GDK_BUTTON1_MOTION_MASK
		       | GDK_BUTTON_RELEASE_MASK,
		       NULL, NULL, event->time);
      paned->child1_size += event->x - handle_size / 2;
      paned->child1_size = CLAMP (paned->child1_size, 0,
				  widget->allocation.width
				  - handle_size
				  - 2 * GTK_CONTAINER (paned)->border_width);
      gtk_hpaned_xor_line (paned);

      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_hpaned_button_release (GtkWidget      *widget,
			   GdkEventButton *event)
{
  GtkPaned *paned = GTK_PANED (widget);
  GObject *object = G_OBJECT (widget);

  if (paned->in_drag && (event->button == 1))
    {
      gtk_hpaned_xor_line (paned);
      paned->in_drag = FALSE;
      paned->position_set = TRUE;
      gdk_pointer_ungrab (event->time);
      gtk_widget_queue_resize (GTK_WIDGET (paned));
      g_object_freeze_notify (object);
      g_object_notify (object, "position");
      g_object_notify (object, "position_set");
      g_object_thaw_notify (object);
  
      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_hpaned_motion (GtkWidget      *widget,
		   GdkEventMotion *event)
{
  GtkPaned *paned = GTK_PANED (widget);
  gint x;
  gint handle_size;

  gtk_widget_style_get (widget, "handle_size", &handle_size, NULL);

  gtk_widget_get_pointer (widget, &x, NULL);

  if (paned->in_drag)
    {
      gint size = x - GTK_CONTAINER (paned)->border_width - handle_size / 2;

      gtk_hpaned_xor_line (paned);
      paned->child1_size = CLAMP (size, paned->min_position, paned->max_position);
      gtk_hpaned_xor_line (paned);
    }

  return TRUE;
}
