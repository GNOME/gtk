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

#include <string.h>
#include "gtkframe.h"
#include "gtklabel.h"

enum {
  ARG_0,
  ARG_LABEL,
  ARG_LABEL_XALIGN,
  ARG_LABEL_YALIGN,
  ARG_SHADOW
};


static void gtk_frame_class_init    (GtkFrameClass  *klass);
static void gtk_frame_init          (GtkFrame       *frame);
static void gtk_frame_set_arg       (GtkObject      *object,
				     GtkArg         *arg,
				     guint           arg_id);
static void gtk_frame_get_arg       (GtkObject      *object,
				     GtkArg         *arg,
				     guint           arg_id);
static void gtk_frame_paint         (GtkWidget      *widget,
				     GdkRectangle   *area);
static void gtk_frame_draw          (GtkWidget      *widget,
				     GdkRectangle   *area);
static gint gtk_frame_expose        (GtkWidget      *widget,
				     GdkEventExpose *event);
static void gtk_frame_size_request  (GtkWidget      *widget,
				     GtkRequisition *requisition);
static void gtk_frame_size_allocate (GtkWidget      *widget,
				     GtkAllocation  *allocation);
static void gtk_frame_map           (GtkWidget      *widget);
static void gtk_frame_unmap         (GtkWidget      *widget);
static void gtk_frame_remove        (GtkContainer   *container,
				     GtkWidget      *child);
static void gtk_frame_forall        (GtkContainer   *container,
				     gboolean	     include_internals,
			             GtkCallback     callback,
			             gpointer        callback_data);

static void gtk_frame_compute_child_allocation      (GtkFrame      *frame,
						     GtkAllocation *child_allocation);
static void gtk_frame_real_compute_child_allocation (GtkFrame      *frame,
						     GtkAllocation *child_allocation);

static GtkBinClass *parent_class = NULL;


/* Here until I convince timj about memory management behavior
 */
gchar *    gtk_frame_get_label        (GtkFrame      *frame);
gchar *    gtk_label_get_text (GtkLabel         *label);


GtkType
gtk_frame_get_type (void)
{
  static GtkType frame_type = 0;

  if (!frame_type)
    {
      static const GtkTypeInfo frame_info =
      {
	"GtkFrame",
	sizeof (GtkFrame),
	sizeof (GtkFrameClass),
	(GtkClassInitFunc) gtk_frame_class_init,
	(GtkObjectInitFunc) gtk_frame_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      frame_type = gtk_type_unique (gtk_bin_get_type (), &frame_info);
    }

  return frame_type;
}

static void
gtk_frame_class_init (GtkFrameClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = GTK_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);
  container_class = GTK_CONTAINER_CLASS (class);

  parent_class = gtk_type_class (gtk_bin_get_type ());

  gtk_object_add_arg_type ("GtkFrame::label", GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_LABEL);
  gtk_object_add_arg_type ("GtkFrame::label_xalign", GTK_TYPE_FLOAT, GTK_ARG_READWRITE, ARG_LABEL_XALIGN);
  gtk_object_add_arg_type ("GtkFrame::label_yalign", GTK_TYPE_FLOAT, GTK_ARG_READWRITE, ARG_LABEL_YALIGN);
  gtk_object_add_arg_type ("GtkFrame::shadow", GTK_TYPE_SHADOW_TYPE, GTK_ARG_READWRITE, ARG_SHADOW);

  object_class->set_arg = gtk_frame_set_arg;
  object_class->get_arg = gtk_frame_get_arg;

  widget_class->draw = gtk_frame_draw;
  widget_class->expose_event = gtk_frame_expose;
  widget_class->size_request = gtk_frame_size_request;
  widget_class->size_allocate = gtk_frame_size_allocate;
  widget_class->map = gtk_frame_map;
  widget_class->unmap = gtk_frame_unmap;

  container_class->remove = gtk_frame_remove;
  container_class->forall = gtk_frame_forall;

  class->compute_child_allocation = gtk_frame_real_compute_child_allocation;
}

static void
gtk_frame_init (GtkFrame *frame)
{
  frame->label_widget = NULL;
  frame->shadow_type = GTK_SHADOW_ETCHED_IN;
  frame->label_xalign = 0.0;
  frame->label_yalign = 0.5;
}

static void
gtk_frame_set_arg (GtkObject      *object,
		   GtkArg         *arg,
		   guint           arg_id)
{
  GtkFrame *frame;

  frame = GTK_FRAME (object);

  switch (arg_id)
    {
    case ARG_LABEL:
      gtk_frame_set_label (frame, GTK_VALUE_STRING (*arg));
      break;
    case ARG_LABEL_XALIGN:
      gtk_frame_set_label_align (frame, GTK_VALUE_FLOAT (*arg), frame->label_yalign);
      break;
    case ARG_LABEL_YALIGN:
      gtk_frame_set_label_align (frame, frame->label_xalign, GTK_VALUE_FLOAT (*arg));
      break;
    case ARG_SHADOW:
      gtk_frame_set_shadow_type (frame, GTK_VALUE_ENUM (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_frame_get_arg (GtkObject      *object,
		   GtkArg         *arg,
		   guint           arg_id)
{
  GtkFrame *frame;

  frame = GTK_FRAME (object);

  switch (arg_id)
    {
    case ARG_LABEL:
      GTK_VALUE_STRING (*arg) = gtk_frame_get_label (frame);
      break;
    case ARG_LABEL_XALIGN:
      GTK_VALUE_FLOAT (*arg) = frame->label_xalign;
      break;
    case ARG_LABEL_YALIGN:
      GTK_VALUE_FLOAT (*arg) = frame->label_yalign;
      break;
    case ARG_SHADOW:
      GTK_VALUE_ENUM (*arg) = frame->shadow_type;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

GtkWidget*
gtk_frame_new (const gchar *label)
{
  GtkFrame *frame;

  frame = gtk_type_new (gtk_frame_get_type ());

  gtk_frame_set_label (frame, label);

  return GTK_WIDGET (frame);
}

static void
gtk_frame_remove (GtkContainer *container,
		  GtkWidget    *child)
{
  GtkFrame *frame = GTK_FRAME (container);

  if (frame->label_widget == child)
    gtk_frame_set_label_widget (frame, NULL);
  else
    GTK_CONTAINER_CLASS (parent_class)->remove (container, child);
}

static void
gtk_frame_forall (GtkContainer *container,
		  gboolean      include_internals,
		  GtkCallback   callback,
		  gpointer      callback_data)
{
  GtkBin *bin = GTK_BIN (container);
  GtkFrame *frame = GTK_FRAME (container);

  if (bin->child)
    (* callback) (bin->child, callback_data);

  if (frame->label_widget)
    (* callback) (frame->label_widget, callback_data);
}

void
gtk_frame_set_label (GtkFrame *frame,
		     const gchar *label)
{
  g_return_if_fail (frame != NULL);
  g_return_if_fail (GTK_IS_FRAME (frame));

  if (!label)
    {
      gtk_frame_set_label_widget (frame, NULL);
    }
  else
    {
      GtkWidget *child = gtk_label_new (label);
      gtk_widget_show (child);

      gtk_frame_set_label_widget (frame, child);
    }
}

/**
 * gtk_frame_get_label:
 * @frame: a #GtkFrame
 * 
 * If the frame's label widget is a #GtkLabel, return the
 * text in the label widget. (The frame will have a #GtkLabel
 * for the label widget if a non-%NULL argument was passed
 * to gtk_frame_new().)
 * 
 * Return value: the text in the label, or %NULL if there
 *               was no label widget or the lable widget was not
 *               a #GtkLabel. This value must be freed with g_free().
 **/
gchar *
gtk_frame_get_label (GtkFrame *frame)
{
  g_return_val_if_fail (frame != NULL, NULL);
  g_return_val_if_fail (GTK_IS_FRAME (frame), NULL);

  if (frame->label_widget && GTK_IS_LABEL (frame->label_widget))
    return gtk_label_get_text (GTK_LABEL (frame->label_widget));
  else
    return NULL;
}

/**
 * gtk_frame_set_label_widget:
 * @frame: a #GtkFrame
 * @label_widget: the new label widget
 * 
 * Set the label widget for the frame. This is the widget that
 * will appear embedded in the top edge of the frame as a
 * title.
 **/
void
gtk_frame_set_label_widget (GtkFrame  *frame,
			    GtkWidget *label_widget)
{
  gboolean need_resize = FALSE;
  
  g_return_if_fail (frame != NULL);
  g_return_if_fail (GTK_IS_FRAME (frame));
  g_return_if_fail (label_widget == NULL || GTK_IS_WIDGET (label_widget));
  g_return_if_fail (label_widget == NULL || label_widget->parent == NULL);
  
  if (frame->label_widget == label_widget)
    return;
  
  if (frame->label_widget)
    {
      need_resize = GTK_WIDGET_VISIBLE (frame->label_widget);
      gtk_widget_unparent (frame->label_widget);
    }

  frame->label_widget = label_widget;
    
  if (label_widget)
    {
      frame->label_widget = label_widget;
      gtk_widget_set_parent (label_widget, GTK_WIDGET (frame));
      need_resize |= GTK_WIDGET_VISIBLE (label_widget);
    }

  if (GTK_WIDGET_VISIBLE (frame) && need_resize)
    gtk_widget_queue_resize (GTK_WIDGET (frame));
}

void
gtk_frame_set_label_align (GtkFrame *frame,
			   gfloat    xalign,
			   gfloat    yalign)
{
  g_return_if_fail (frame != NULL);
  g_return_if_fail (GTK_IS_FRAME (frame));

  xalign = CLAMP (xalign, 0.0, 1.0);
  yalign = CLAMP (yalign, 0.0, 1.0);

  if ((xalign != frame->label_xalign) || (yalign != frame->label_yalign))
    {
      frame->label_xalign = xalign;
      frame->label_yalign = yalign;

      gtk_widget_queue_resize (GTK_WIDGET (frame));
    }
}

void
gtk_frame_set_shadow_type (GtkFrame      *frame,
			   GtkShadowType  type)
{
  g_return_if_fail (frame != NULL);
  g_return_if_fail (GTK_IS_FRAME (frame));

  if ((GtkShadowType) frame->shadow_type != type)
    {
      frame->shadow_type = type;

      if (GTK_WIDGET_DRAWABLE (frame))
	{
	  gtk_widget_queue_clear (GTK_WIDGET (frame));
	}
      gtk_widget_queue_resize (GTK_WIDGET (frame));
    }
}

static void
gtk_frame_paint (GtkWidget    *widget,
		 GdkRectangle *area)
{
  GtkFrame *frame;
  gint x, y, width, height;

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      frame = GTK_FRAME (widget);

      x = frame->child_allocation.x - widget->style->xthickness;
      y = frame->child_allocation.y - widget->style->ythickness;
      width = frame->child_allocation.width + 2 * widget->style->xthickness;
      height =  frame->child_allocation.height + 2 * widget->style->ythickness;

      if (frame->label_widget)
	{
	  GtkRequisition child_requisition;
	  gfloat xalign;
	  gint height_extra;
	  gint x2;

	  gtk_widget_get_child_requisition (frame->label_widget, &child_requisition);

	  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	    xalign = frame->label_xalign;
	  else
	    xalign = 1 - frame->label_xalign;

	  height_extra = MAX (0, child_requisition.height - widget->style->xthickness);
	  y -= height_extra * (1 - frame->label_yalign);
	  height += height_extra * (1 - frame->label_yalign);
	  
	  x2 = 2 + (frame->child_allocation.width - child_requisition.width) * xalign;

	  gtk_paint_shadow_gap (widget->style, widget->window,
				GTK_STATE_NORMAL, frame->shadow_type,
				area, widget, "frame",
				x, y, width, height,
				GTK_POS_TOP, 
				x2, child_requisition.width - 4);
	}
       else
	 gtk_paint_shadow (widget->style, widget->window,
			   GTK_STATE_NORMAL, frame->shadow_type,
			   area, widget, "frame",
			   x, y, width, height);
    }
}

static void
gtk_frame_draw (GtkWidget    *widget,
		GdkRectangle *area)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkFrame *frame = GTK_FRAME (widget);
  GdkRectangle child_area;

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_frame_paint (widget, area);

      if (bin->child && gtk_widget_intersect (bin->child, area, &child_area))
	gtk_widget_draw (bin->child, &child_area);

      if (frame->label_widget && gtk_widget_intersect (frame->label_widget, area, &child_area))
	gtk_widget_draw (frame->label_widget, &child_area);
    }
}

static gboolean
gtk_frame_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkFrame *frame = GTK_FRAME (widget);
  GdkEventExpose child_event;

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_frame_paint (widget, &event->area);

      if (bin->child && GTK_WIDGET_NO_WINDOW (bin->child))
	{
	  child_event = *event;
	  if (gtk_widget_intersect (bin->child, &event->area, &child_event.area))
	    gtk_widget_event (bin->child, (GdkEvent*) &child_event);
	}
      
      if (frame->label_widget && GTK_WIDGET_NO_WINDOW (frame->label_widget))
	{
	  child_event = *event;
	  if (gtk_widget_intersect (frame->label_widget, &event->area, &child_event.area))
	    gtk_widget_event (frame->label_widget, (GdkEvent*) &child_event);
	}
    }

  return FALSE;
}

static void
gtk_frame_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  GtkFrame *frame = GTK_FRAME (widget);
  GtkBin *bin = GTK_BIN (widget);
  GtkRequisition child_requisition;
  
  if (frame->label_widget && GTK_WIDGET_VISIBLE (frame->label_widget))
    {
      gtk_widget_size_request (frame->label_widget, &child_requisition);

      requisition->width = child_requisition.width;
      requisition->height =
	MAX (0, child_requisition.height - GTK_WIDGET (widget)->style->xthickness);
    }
  else
    {
      requisition->width = 0;
      requisition->height = 0;
    }
  
  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gtk_widget_size_request (bin->child, &child_requisition);

      requisition->width = MAX (requisition->width, child_requisition.width);
      requisition->height += child_requisition.height;
    }

  requisition->width += (GTK_CONTAINER (widget)->border_width +
			 GTK_WIDGET (widget)->style->xthickness) * 2;
  requisition->height += (GTK_CONTAINER (widget)->border_width +
			  GTK_WIDGET (widget)->style->ythickness) * 2;
}

static void
gtk_frame_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkFrame *frame = GTK_FRAME (widget);
  GtkBin *bin = GTK_BIN (widget);
  GtkAllocation new_allocation;

  widget->allocation = *allocation;

  gtk_frame_compute_child_allocation (frame, &new_allocation);
  
  /* If the child allocation changed, that means that the frame is drawn
   * in a new place, so we must redraw the entire widget.
   */
  if (GTK_WIDGET_MAPPED (widget) &&
      (new_allocation.x != frame->child_allocation.x ||
       new_allocation.y != frame->child_allocation.y ||
       new_allocation.width != frame->child_allocation.width ||
       new_allocation.height != frame->child_allocation.height))
    gtk_widget_queue_clear (widget);
  
  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    gtk_widget_size_allocate (bin->child, &new_allocation);
  
  frame->child_allocation = new_allocation;
  
  if (frame->label_widget && GTK_WIDGET_VISIBLE (frame->label_widget))
    {
      GtkRequisition child_requisition;
      GtkAllocation child_allocation;
      gfloat xalign;

      gtk_widget_get_child_requisition (frame->label_widget, &child_requisition);

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	xalign = frame->label_xalign;
      else
	xalign = 1 - frame->label_xalign;
      
      child_allocation.x = frame->child_allocation.x +
	(frame->child_allocation.width - child_requisition.width) * xalign;
      child_allocation.width = child_requisition.width;

      child_allocation.y = frame->child_allocation.y - child_requisition.height;
      child_allocation.height = child_requisition.height;

      gtk_widget_size_allocate (frame->label_widget, &child_allocation);
    }
}

static void
gtk_frame_map (GtkWidget *widget)
{
  GtkFrame *frame = GTK_FRAME (widget);
  
  if (frame->label_widget &&
      GTK_WIDGET_VISIBLE (frame->label_widget) &&
      !GTK_WIDGET_MAPPED (frame->label_widget))
    gtk_widget_map (frame->label_widget);

  if (GTK_WIDGET_CLASS (parent_class)->map)
    (* GTK_WIDGET_CLASS (parent_class)->map) (widget);
}

static void
gtk_frame_unmap (GtkWidget *widget)
{
  GtkFrame *frame = GTK_FRAME (widget);

  if (GTK_WIDGET_CLASS (parent_class)->unmap)
    (* GTK_WIDGET_CLASS (parent_class)->unmap) (widget);

  if (frame->label_widget && GTK_WIDGET_MAPPED (frame->label_widget))
    gtk_widget_unmap (frame->label_widget);
}

static void
gtk_frame_compute_child_allocation (GtkFrame      *frame,
				    GtkAllocation *child_allocation)
{
  g_return_if_fail (frame != NULL);
  g_return_if_fail (GTK_IS_FRAME (frame));
  g_return_if_fail (child_allocation != NULL);

  GTK_FRAME_GET_CLASS (frame)->compute_child_allocation (frame, child_allocation);
}

static void
gtk_frame_real_compute_child_allocation (GtkFrame      *frame,
					 GtkAllocation *child_allocation)
{
  GtkWidget *widget = GTK_WIDGET (frame);
  GtkAllocation *allocation = &widget->allocation;
  GtkRequisition child_requisition;
  gint top_margin;

  if (frame->label_widget)
    {
      gtk_widget_get_child_requisition (frame->label_widget, &child_requisition);
      top_margin = MAX (child_requisition.height, widget->style->ythickness);
    }
  else
    top_margin = widget->style->ythickness;
  
  child_allocation->x = (GTK_CONTAINER (frame)->border_width +
			 widget->style->xthickness);
  child_allocation->width = MAX(1, (gint)allocation->width - child_allocation->x * 2);
  
  child_allocation->y = (GTK_CONTAINER (frame)->border_width + top_margin);
  child_allocation->height = MAX (1, ((gint)allocation->height - child_allocation->y -
				      (gint)GTK_CONTAINER (frame)->border_width -
				      (gint)widget->style->ythickness));
  
  child_allocation->x += allocation->x;
  child_allocation->y += allocation->y;
}
