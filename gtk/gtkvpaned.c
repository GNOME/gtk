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

#include "gtkvpaned.h"

static void     gtk_vpaned_class_init     (GtkVPanedClass *klass);
static void     gtk_vpaned_init           (GtkVPaned      *vpaned);
static void     gtk_vpaned_size_request   (GtkWidget      *widget,
					   GtkRequisition *requisition);
static void     gtk_vpaned_size_allocate  (GtkWidget      *widget,
					   GtkAllocation  *allocation);
static gint     gtk_vpaned_expose         (GtkWidget      *widget,
                                           GdkEventExpose *expose);
static void     gtk_vpaned_xor_line       (GtkPaned       *paned);
static gboolean gtk_vpaned_button_press   (GtkWidget      *widget,
					   GdkEventButton *event);
static gboolean gtk_vpaned_button_release (GtkWidget      *widget,
					   GdkEventButton *event);
static gboolean gtk_vpaned_motion         (GtkWidget      *widget,
					   GdkEventMotion *event);

static gpointer parent_class;

GtkType
gtk_vpaned_get_type (void)
{
  static GtkType vpaned_type = 0;

  if (!vpaned_type)
    {
      static const GtkTypeInfo vpaned_info =
      {
	"GtkVPaned",
	sizeof (GtkVPaned),
	sizeof (GtkVPanedClass),
	(GtkClassInitFunc) gtk_vpaned_class_init,
	(GtkObjectInitFunc) gtk_vpaned_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      vpaned_type = gtk_type_unique (GTK_TYPE_PANED, &vpaned_info);
    }

  return vpaned_type;
}

static void
gtk_vpaned_class_init (GtkVPanedClass *class)
{
  GtkWidgetClass *widget_class;

  parent_class = gtk_type_class (GTK_TYPE_PANED);
  
  widget_class = (GtkWidgetClass *) class;

  widget_class->size_request = gtk_vpaned_size_request;
  widget_class->size_allocate = gtk_vpaned_size_allocate;
  widget_class->expose_event = gtk_vpaned_expose;
  widget_class->button_press_event = gtk_vpaned_button_press;
  widget_class->button_release_event = gtk_vpaned_button_release;
  widget_class->motion_notify_event = gtk_vpaned_motion;
}

static void
gtk_vpaned_init (GtkVPaned *vpaned)
{
  GtkPaned *paned;

  g_return_if_fail (vpaned != NULL);
  g_return_if_fail (GTK_IS_PANED (vpaned));

  paned = GTK_PANED (vpaned);

  paned->cursor_type = GDK_SB_V_DOUBLE_ARROW;
}

GtkWidget *
gtk_vpaned_new (void)
{
  GtkVPaned *vpaned;

  vpaned = gtk_type_new (GTK_TYPE_VPANED);

  return GTK_WIDGET (vpaned);
}

static void
gtk_vpaned_size_request (GtkWidget      *widget,
			 GtkRequisition *requisition)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkRequisition child_requisition;
  gint handle_size;

  gtk_widget_style_get (widget, "handle_size", &handle_size, NULL);

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

  requisition->height += GTK_CONTAINER (paned)->border_width * 2 + handle_size;
  requisition->width += GTK_CONTAINER (paned)->border_width * 2;
}

static void
gtk_vpaned_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkPaned *paned = GTK_PANED (widget);
  GtkRequisition child1_requisition;
  GtkRequisition child2_requisition;
  GtkAllocation child1_allocation;
  GtkAllocation child2_allocation;
  gint border_width;
  gint handle_size;

  widget->allocation = *allocation;

  border_width = GTK_CONTAINER (widget)->border_width;
  gtk_widget_style_get (widget, "handle_size", &handle_size, NULL);

  if (paned->child1)
    gtk_widget_get_child_requisition (paned->child1, &child1_requisition);
  else
    child1_requisition.height = 0;

  if (paned->child2)
    gtk_widget_get_child_requisition (paned->child2, &child2_requisition);
  else
    child2_requisition.height = 0;
    
  gtk_paned_compute_position (paned,
			      MAX (1, widget->allocation.height
				   - handle_size
				   - 2 * border_width),
			      child1_requisition.height,
			      child2_requisition.height);

  /* Move the handle before the children so we don't get extra expose events */

  paned->handle_xpos = border_width;
  paned->handle_ypos = paned->child1_size + border_width;
  paned->handle_width = MAX (1, (gint) widget->allocation.width - 2 * border_width);
  paned->handle_height = handle_size;

  if (GTK_WIDGET_REALIZED(widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width,
			      allocation->height);

      gdk_window_move_resize (paned->handle,
			      paned->handle_xpos,
			      paned->handle_ypos,
			      paned->handle_width,
			      handle_size);
    }

  child1_allocation.width = child2_allocation.width = MAX (1, (gint) allocation->width - border_width * 2);
  child1_allocation.height = paned->child1_size;
  child1_allocation.x = child2_allocation.x = border_width;
  child1_allocation.y = border_width;

  child2_allocation.y = child1_allocation.y + child1_allocation.height + paned->handle_height;
  child2_allocation.height = MAX(1, (gint) allocation->height - child2_allocation.y - border_width);

  /* Now allocate the childen, making sure, when resizing not to
   * overlap the windows */
  if (GTK_WIDGET_MAPPED (widget) &&
      paned->child1 && GTK_WIDGET_VISIBLE (paned->child1) &&
      paned->child1->allocation.height < child1_allocation.height)
    {
      if (paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
	gtk_widget_size_allocate(paned->child2, &child2_allocation);
      gtk_widget_size_allocate(paned->child1, &child1_allocation);
    }
  else
    {
      if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1))
	gtk_widget_size_allocate(paned->child1, &child1_allocation);
      if (paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
	gtk_widget_size_allocate(paned->child2, &child2_allocation);
    }
}

static gint
gtk_vpaned_expose (GtkWidget      *widget,
                   GdkEventExpose *event)
{
  GtkPaned *paned;
  guint16 border_width;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_PANED (widget), FALSE);

  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget))
    {
      paned = GTK_PANED (widget);
      border_width = GTK_CONTAINER (paned)->border_width;

      if (event->window == widget->window)
        {
          gdk_window_clear_area (widget->window,
                                 event->area.x, event->area.y,
                                 event->area.width,
                                 event->area.height);


          /* Chain up to draw children */
          GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
        }
      else if (event->window == paned->handle)
        {
	  gtk_paint_handle (widget->style,
			    paned->handle,
			    GTK_STATE_NORMAL,
			    GTK_SHADOW_NONE,
                            &event->area,
			    widget,
			    "paned",
			    0, 0, -1, -1,
			    GTK_ORIENTATION_HORIZONTAL);

	}
    }

  return FALSE;
}

static void
gtk_vpaned_xor_line (GtkPaned *paned)
{
  GtkWidget *widget;
  GdkGCValues values;
  guint16 ypos;
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

  ypos = paned->child1_size
    + GTK_CONTAINER (paned)->border_width + handle_size / 2;

  gdk_draw_line (widget->window, paned->xor_gc,
		 0,
		 ypos,
		 widget->allocation.width - 1,
		 ypos);
}

static gboolean
gtk_vpaned_button_press (GtkWidget      *widget,
			 GdkEventButton *event)
{
  GtkPaned *paned = GTK_PANED (widget);
  gint handle_size;

  gtk_widget_style_get (widget, "handle_size", &handle_size, NULL);

  if (!paned->in_drag &&
      (event->window == paned->handle) && (event->button == 1))
    {
      paned->in_drag = TRUE;
      /* We need a server grab here, not gtk_grab_add(), since
       * we don't want to pass events on to the widget's children */
      gdk_pointer_grab (paned->handle, FALSE,
			GDK_POINTER_MOTION_HINT_MASK
			| GDK_BUTTON1_MOTION_MASK
			| GDK_BUTTON_RELEASE_MASK, NULL, NULL,
			event->time);
      paned->child1_size += event->y - handle_size / 2;
      paned->child1_size = CLAMP (paned->child1_size, 0,
				  widget->allocation.height -
				  handle_size -
				  2 * GTK_CONTAINER (paned)->border_width);
      gtk_vpaned_xor_line(paned);

      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_vpaned_button_release (GtkWidget      *widget,
			   GdkEventButton *event)
{
  GtkPaned *paned = GTK_PANED (widget);
  GObject *object = G_OBJECT (widget);

  if (paned->in_drag && (event->button == 1))
    {
      gtk_vpaned_xor_line (paned);
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
gtk_vpaned_motion (GtkWidget      *widget,
		   GdkEventMotion *event)
{
  GtkPaned *paned = GTK_PANED (widget);
  gint y;
  gint handle_size;

  gtk_widget_style_get (widget, "handle_size", &handle_size, NULL);

  if (event->is_hint || event->window != widget->window)
    gtk_widget_get_pointer (widget, NULL, &y);
  else
    y = event->y;

  if (paned->in_drag)
    {
      gint size = y - GTK_CONTAINER(paned)->border_width - handle_size / 2;

      gtk_vpaned_xor_line (paned);
      paned->child1_size = CLAMP (size, paned->min_position, paned->max_position);
      gtk_vpaned_xor_line(paned);
    }

  return TRUE;
}
