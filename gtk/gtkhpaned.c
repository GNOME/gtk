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

#include "gtkhpaned.h"
#include "gtkmain.h"
#include "gtksignal.h"

static void gtk_hpaned_class_init       (GtkHPanedClass *klass);
static void gtk_hpaned_init             (GtkHPaned      *hpaned);
static void gtk_hpaned_size_request     (GtkWidget      *widget,
					 GtkRequisition *requisition);
static void gtk_hpaned_size_allocate    (GtkWidget          *widget,
					 GtkAllocation      *allocation);
static void gtk_hpaned_draw             (GtkWidget    *widget,
					 GdkRectangle *area);
static void gtk_hpaned_xor_line         (GtkPaned *paned);
static gint gtk_hpaned_button_press     (GtkWidget *widget,
					 GdkEventButton *event);
static gint gtk_hpaned_button_release   (GtkWidget *widget,
					 GdkEventButton *event);
static gint gtk_hpaned_motion           (GtkWidget *widget,
					 GdkEventMotion *event);

guint
gtk_hpaned_get_type (void)
{
  static guint hpaned_type = 0;

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

      hpaned_type = gtk_type_unique (gtk_paned_get_type (), &hpaned_info);
    }

  return hpaned_type;
}

static void
gtk_hpaned_class_init (GtkHPanedClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->size_request = gtk_hpaned_size_request;
  widget_class->size_allocate = gtk_hpaned_size_allocate;
  widget_class->draw = gtk_hpaned_draw;
  widget_class->button_press_event = gtk_hpaned_button_press;
  widget_class->button_release_event = gtk_hpaned_button_release;
  widget_class->motion_notify_event = gtk_hpaned_motion;
}

static void
gtk_hpaned_init (GtkHPaned *hpaned)
{
}

GtkWidget*
gtk_hpaned_new (void)
{
  GtkHPaned *hpaned;

  hpaned = gtk_type_new (gtk_hpaned_get_type ());

  return GTK_WIDGET (hpaned);
}

static void
gtk_hpaned_size_request (GtkWidget      *widget,
			 GtkRequisition *requisition)
{
  GtkPaned *paned;
  GtkRequisition child_requisition;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HPANED (widget));
  g_return_if_fail (requisition != NULL);

  paned = GTK_PANED (widget);
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

  requisition->width += GTK_CONTAINER (paned)->border_width * 2 + _gtk_paned_get_gutter_size (paned);
  requisition->height += GTK_CONTAINER (paned)->border_width * 2;
}

static void
gtk_hpaned_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkRequisition child1_requisition;
  GtkRequisition child2_requisition;
  GtkAllocation child1_allocation;
  GtkAllocation child2_allocation;

  gint border_width = GTK_CONTAINER (widget)->border_width;
  gint gutter_size = _gtk_paned_get_gutter_size (paned);
  gboolean handle_full_size = _gtk_paned_is_handle_full_size (paned);

  widget->allocation = *allocation;

  if (paned->child1)
    gtk_widget_get_child_requisition (paned->child1, &child1_requisition);
  else
    child1_requisition.width = 0;

  if (paned->child2)
    gtk_widget_get_child_requisition (paned->child2, &child2_requisition);
  else
    child2_requisition.width = 0;
    
  gtk_paned_compute_position (paned,
			      MAX (1, (gint) widget->allocation.width
				   - gutter_size
				   - 2 * border_width),
			      child1_requisition.width,
			      child2_requisition.width);
  
  child1_allocation.height = child2_allocation.height = MAX (1, (gint) allocation->height - border_width * 2);
  child1_allocation.width = paned->child1_size;
  child1_allocation.y = child2_allocation.y = border_width;
  child1_allocation.x = border_width;
      
  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x, allocation->y,
			    allocation->width, allocation->height);
  
  /* Move the handle before the children so we don't get extra expose events
   */
  if (handle_full_size)
    {
      GdkRectangle rect;

      _gtk_paned_get_handle_rect (paned, &rect);

      paned->handle_xpos = rect.x;
      paned->handle_ypos = rect.y;

      if (GTK_WIDGET_REALIZED (widget))
	gdk_window_move_resize (paned->handle,
				rect.x, rect.y, rect.width, rect.height);
    }
  else
    {
      GdkRectangle old_groove_rectangle;
      
      paned->handle_ypos = (gint) allocation->height - border_width - 2 * paned->handle_size;
      paned->handle_xpos = paned->child1_size + border_width + gutter_size / 2 - paned->handle_size / 2;
      
      if (GTK_WIDGET_REALIZED (widget))
	gdk_window_move (paned->handle, paned->handle_xpos, paned->handle_ypos);

      old_groove_rectangle = paned->groove_rectangle;
      
      paned->groove_rectangle.x = (child1_allocation.x +
				   child1_allocation.width + gutter_size / 2 - 1);
      paned->groove_rectangle.y = 0;
      paned->groove_rectangle.width = 2;
      paned->groove_rectangle.height = allocation->height;
      
      if (GTK_WIDGET_DRAWABLE (widget) &&
	  ((paned->groove_rectangle.x != old_groove_rectangle.x) ||
	   (paned->groove_rectangle.y != old_groove_rectangle.y) ||
	   (paned->groove_rectangle.width != old_groove_rectangle.width) ||
	   (paned->groove_rectangle.height != old_groove_rectangle.height)))
	{
	  gtk_widget_queue_clear_area (widget,
				       old_groove_rectangle.x,
				       old_groove_rectangle.y,
				       old_groove_rectangle.width,
				       old_groove_rectangle.height);
	  gtk_widget_queue_draw_area (widget,
				      paned->groove_rectangle.x,
				      paned->groove_rectangle.y,
				      paned->groove_rectangle.width,
				      paned->groove_rectangle.height);
	}
  
    }
      
  child2_allocation.x = child1_allocation.x + child1_allocation.width + gutter_size;
  child2_allocation.width = MAX (1, (gint) allocation->width -
				 child2_allocation.x - border_width);
  
  /* Now allocate the childen, making sure, when resizing not to
   * overlap the windows */
  if (GTK_WIDGET_MAPPED(widget) &&
      paned->child1 && GTK_WIDGET_VISIBLE (paned->child1) &&
      paned->child1->allocation.width < child1_allocation.width)
    {
      if (paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
	gtk_widget_size_allocate (paned->child2, &child2_allocation);
      gtk_widget_size_allocate (paned->child1, &child1_allocation);      
    }
  else
    {
      if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1))
	gtk_widget_size_allocate (paned->child1, &child1_allocation);
      if (paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
	gtk_widget_size_allocate (paned->child2, &child2_allocation);
    }
}

static void
gtk_hpaned_draw (GtkWidget    *widget,
		GdkRectangle *area)
{
  GtkPaned *paned = GTK_PANED (widget);
  GdkRectangle handle_area, child_area;
  guint16 border_width;
  gboolean handle_full_size = _gtk_paned_is_handle_full_size (paned);

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PANED (widget));

  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget))
    {
      gint width, height;
      
      border_width = GTK_CONTAINER (paned)->border_width;

      gdk_window_clear_area (widget->window,
			     area->x, area->y, area->width, area->height);

      /* Redraw the handle
       */
      gdk_window_get_size (paned->handle, &width, &height);
      
      handle_area.x = paned->handle_xpos;
      handle_area.y = paned->handle_ypos;
      handle_area.width = width;
      handle_area.height = height;

      if (gdk_rectangle_intersect (&handle_area, area, &child_area))
	{
	  child_area.x -= handle_area.x;
	  child_area.y -= handle_area.y;
	  gtk_paint_box (widget->style, paned->handle,
			 GTK_WIDGET_STATE(widget),
			 GTK_SHADOW_OUT, 
			 &child_area, widget, 
			 handle_full_size ? "hpaned" : "paned",
			 0, 0,
			 width, height);
	}

      /* Redraw the groove
       */
      if (!handle_full_size)
	gtk_paint_vline(widget->style, widget->window, GTK_STATE_NORMAL,
			area, widget, "hpaned", 
			0, widget->allocation.height - 1,
			border_width + paned->child1_size + _gtk_paned_get_gutter_size (paned) / 2 - 1);
      
      /* Redraw the children
       */
      if (paned->child1 &&
	  gtk_widget_intersect (paned->child1, area, &child_area))
        gtk_widget_draw (paned->child1, &child_area);
      if (paned->child2 &&
	  gtk_widget_intersect (paned->child2, area, &child_area))
        gtk_widget_draw (paned->child2, &child_area);

    }
}

static void
gtk_hpaned_xor_line (GtkPaned *paned)
{
  GtkWidget *widget;
  GdkGCValues values;
  guint16 xpos;

  widget = GTK_WIDGET(paned);

  if (!paned->xor_gc)
    {
      values.function = GDK_INVERT;
      values.subwindow_mode = GDK_INCLUDE_INFERIORS;
      paned->xor_gc = gdk_gc_new_with_values (widget->window,
					      &values,
					      GDK_GC_FUNCTION |
					      GDK_GC_SUBWINDOW);
    }

  xpos = paned->child1_size
    + GTK_CONTAINER (paned)->border_width + _gtk_paned_get_gutter_size (paned) / 2;

  gdk_draw_line (widget->window, paned->xor_gc,
		 xpos,
		 0,
		 xpos,
		 widget->allocation.height - 1);
}

static gint
gtk_hpaned_button_press (GtkWidget *widget, GdkEventButton *event)
{
  GtkPaned *paned = GTK_PANED (widget);
  gint gutter_size = _gtk_paned_get_gutter_size (paned);

  if (!paned->in_drag &&
      (event->window == paned->handle) && (event->button == 1))
    {
      paned->in_drag = TRUE;
      /* We need a server grab here, not gtk_grab_add(), since
       * we don't want to pass events on to the widget's children */
      gdk_pointer_grab (paned->handle, FALSE,
			GDK_POINTER_MOTION_HINT_MASK 
			| GDK_BUTTON1_MOTION_MASK 
			| GDK_BUTTON_RELEASE_MASK,
			NULL, NULL, event->time);
      paned->child1_size += event->x - paned->handle_size / 2;
      paned->child1_size = CLAMP (paned->child1_size, 0,
                                  widget->allocation.width - gutter_size
                                  - 2 * GTK_CONTAINER (paned)->border_width);
      gtk_hpaned_xor_line (paned);
    }

  return TRUE;
}

static gint
gtk_hpaned_button_release (GtkWidget *widget, GdkEventButton *event)
{
  GtkPaned *paned;

  g_return_val_if_fail (widget != NULL,FALSE);
  g_return_val_if_fail (GTK_IS_PANED (widget),FALSE);

  paned = GTK_PANED (widget);

  if (paned->in_drag && (event->button == 1))
    {
      gtk_hpaned_xor_line (paned);
      paned->in_drag = FALSE;
      paned->position_set = TRUE;
      gdk_pointer_ungrab (event->time);
      gtk_widget_queue_resize (GTK_WIDGET (paned));
    }

  return TRUE;
}

static gint
gtk_hpaned_motion (GtkWidget *widget, GdkEventMotion *event)
{
  GtkPaned *paned;
  gint x;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_PANED (widget), FALSE);

  paned = GTK_PANED (widget);

  if (event->is_hint || event->window != widget->window)
    gtk_widget_get_pointer(widget, &x, NULL);
  else
    x = event->x;

  if (paned->in_drag)
    {
      gint size = x - GTK_CONTAINER (paned)->border_width - _gtk_paned_get_gutter_size (paned) / 2;
      
      gtk_hpaned_xor_line (paned);
      paned->child1_size = CLAMP (size,
				  paned->min_position,
				  paned->max_position);
      gtk_hpaned_xor_line (paned);
    }

  return TRUE;
}
