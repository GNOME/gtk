/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkAspectFrame: Ensure that the child window has a specified aspect ratio
 *    or, if obey_child, has the same aspect ratio as its requested size
 *
 *     Copyright Owen Taylor                          4/9/97
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

#include "gtkaspectframe.h"

enum {
  ARG_0,
  ARG_XALIGN,
  ARG_YALIGN,
  ARG_RATIO,
  ARG_OBEY_CHILD
};

static void gtk_aspect_frame_class_init    (GtkAspectFrameClass *klass);
static void gtk_aspect_frame_init          (GtkAspectFrame *aspect_frame);
static void gtk_aspect_frame_set_arg       (GtkObject      *object,
					    GtkArg         *arg,
					    guint           arg_id);
static void gtk_aspect_frame_get_arg       (GtkObject      *object,
					    GtkArg         *arg,
					    guint           arg_id);
static void gtk_aspect_frame_draw          (GtkWidget      *widget,
					    GdkRectangle   *area);
static void gtk_aspect_frame_paint         (GtkWidget      *widget,
					    GdkRectangle   *area);
static gint gtk_aspect_frame_expose        (GtkWidget      *widget,
					    GdkEventExpose *event);
static void gtk_aspect_frame_size_allocate (GtkWidget         *widget,
					    GtkAllocation     *allocation);

#define MAX_RATIO 10000.0
#define MIN_RATIO 0.0001

GtkType
gtk_aspect_frame_get_type (void)
{
  static GtkType aspect_frame_type = 0;
  
  if (!aspect_frame_type)
    {
      static const GtkTypeInfo aspect_frame_info =
      {
	"GtkAspectFrame",
	sizeof (GtkAspectFrame),
	sizeof (GtkAspectFrameClass),
	(GtkClassInitFunc) gtk_aspect_frame_class_init,
	(GtkObjectInitFunc) gtk_aspect_frame_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };
      
      aspect_frame_type = gtk_type_unique (GTK_TYPE_FRAME, &aspect_frame_info);
    }
  
  return aspect_frame_type;
}

static void
gtk_aspect_frame_class_init (GtkAspectFrameClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  
  object_class = GTK_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);
  
  object_class->set_arg = gtk_aspect_frame_set_arg;
  object_class->get_arg = gtk_aspect_frame_get_arg;

  widget_class->draw = gtk_aspect_frame_draw;
  widget_class->expose_event = gtk_aspect_frame_expose;
  widget_class->size_allocate = gtk_aspect_frame_size_allocate;

  gtk_object_add_arg_type ("GtkAspectFrame::xalign", GTK_TYPE_FLOAT,
			   GTK_ARG_READWRITE, ARG_XALIGN);
  gtk_object_add_arg_type ("GtkAspectFrame::yalign", GTK_TYPE_FLOAT,
			   GTK_ARG_READWRITE, ARG_YALIGN);
  gtk_object_add_arg_type ("GtkAspectFrame::ratio", GTK_TYPE_FLOAT,
			   GTK_ARG_READWRITE, ARG_RATIO);
  gtk_object_add_arg_type ("GtkAspectFrame::obey_child", GTK_TYPE_BOOL,
			   GTK_ARG_READWRITE, ARG_OBEY_CHILD);  
}

static void
gtk_aspect_frame_init (GtkAspectFrame *aspect_frame)
{
  aspect_frame->xalign = 0.5;
  aspect_frame->yalign = 0.5;
  aspect_frame->ratio = 1.0;
  aspect_frame->obey_child = TRUE;
  aspect_frame->center_allocation.x = -1;
  aspect_frame->center_allocation.y = -1;
  aspect_frame->center_allocation.width = 1;
  aspect_frame->center_allocation.height = 1;
}

static void
gtk_aspect_frame_set_arg (GtkObject *object,
			  GtkArg    *arg,
			  guint      arg_id)
{
  GtkAspectFrame *aspect_frame = GTK_ASPECT_FRAME (object);
  
  switch (arg_id)
    {
    case ARG_XALIGN:
      gtk_aspect_frame_set (aspect_frame,
			    GTK_VALUE_FLOAT (*arg),
			    aspect_frame->yalign,
			    aspect_frame->ratio,
			    aspect_frame->obey_child);
      break;
    case ARG_YALIGN:
      gtk_aspect_frame_set (aspect_frame,
			    aspect_frame->xalign,
			    GTK_VALUE_FLOAT (*arg),
			    aspect_frame->ratio,
			    aspect_frame->obey_child);
      break;
    case ARG_RATIO:
      gtk_aspect_frame_set (aspect_frame,
			    aspect_frame->xalign,
			    aspect_frame->yalign,
			    GTK_VALUE_FLOAT (*arg),
			    aspect_frame->obey_child);
      break;
    case ARG_OBEY_CHILD:
      gtk_aspect_frame_set (aspect_frame,
			    aspect_frame->xalign,
			    aspect_frame->yalign,
			    aspect_frame->ratio,
			    GTK_VALUE_BOOL (*arg));
      break;
    }
}

static void
gtk_aspect_frame_get_arg (GtkObject *object,
			  GtkArg    *arg,
			  guint      arg_id)
{
  GtkAspectFrame *aspect_frame = GTK_ASPECT_FRAME (object);
  
  switch (arg_id)
    {
    case ARG_XALIGN:
      GTK_VALUE_FLOAT (*arg) = aspect_frame->xalign;
      break;
    case ARG_YALIGN:
      GTK_VALUE_FLOAT (*arg) = aspect_frame->yalign;
      break;
    case ARG_RATIO:
      GTK_VALUE_FLOAT (*arg) = aspect_frame->ratio;
      break;
    case ARG_OBEY_CHILD:
      GTK_VALUE_BOOL (*arg) = aspect_frame->obey_child;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

GtkWidget*
gtk_aspect_frame_new (const gchar *label,
		      gfloat       xalign,
		      gfloat       yalign,
		      gfloat       ratio,
		      gboolean     obey_child)
{
  GtkAspectFrame *aspect_frame;

  aspect_frame = gtk_type_new (gtk_aspect_frame_get_type ());

  aspect_frame->xalign = CLAMP (xalign, 0.0, 1.0);
  aspect_frame->yalign = CLAMP (yalign, 0.0, 1.0);
  aspect_frame->ratio = CLAMP (ratio, MIN_RATIO, MAX_RATIO);
  aspect_frame->obey_child = obey_child != FALSE;

  gtk_frame_set_label (GTK_FRAME(aspect_frame), label);

  return GTK_WIDGET (aspect_frame);
}

void
gtk_aspect_frame_set (GtkAspectFrame *aspect_frame,
		      gfloat          xalign,
		      gfloat          yalign,
		      gfloat          ratio,
		      gboolean        obey_child)
{
  g_return_if_fail (aspect_frame != NULL);
  g_return_if_fail (GTK_IS_ASPECT_FRAME (aspect_frame));
  
  xalign = CLAMP (xalign, 0.0, 1.0);
  yalign = CLAMP (yalign, 0.0, 1.0);
  ratio = CLAMP (ratio, MIN_RATIO, MAX_RATIO);
  obey_child = obey_child != FALSE;
  
  if ((aspect_frame->xalign != xalign) ||
      (aspect_frame->yalign != yalign) ||
      (aspect_frame->ratio != ratio) ||
      (aspect_frame->obey_child != obey_child))
    {
      GtkWidget *widget = GTK_WIDGET(aspect_frame);
      
      aspect_frame->xalign = xalign;
      aspect_frame->yalign = yalign;
      aspect_frame->ratio = ratio;
      aspect_frame->obey_child = obey_child;
      
      if (GTK_WIDGET_DRAWABLE(widget))
	gtk_widget_queue_clear (widget);
      
      gtk_widget_queue_resize (widget);
    }
}

static void
gtk_aspect_frame_paint (GtkWidget    *widget,
			GdkRectangle *area)
{
  GtkFrame *frame;
  gint height_extra;
  gint label_area_width;
  gint x, y, x2, y2;
  GtkAllocation *allocation;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ASPECT_FRAME (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      frame = GTK_FRAME (widget);
      allocation = &GTK_ASPECT_FRAME(widget)->center_allocation;

      height_extra = frame->label_height - widget->style->klass->xthickness;
      height_extra = MAX (height_extra, 0);

      x = GTK_CONTAINER (frame)->border_width;
      y = GTK_CONTAINER (frame)->border_width;

      if (frame->label)
	{
	  label_area_width = (allocation->width +
			      GTK_CONTAINER (frame)->border_width * 2 -
			      widget->style->klass->xthickness * 2);

	  x2 = ((label_area_width - frame->label_width) * frame->label_xalign +
		GTK_CONTAINER (frame)->border_width + widget->style->klass->xthickness);
	  y2 = (GTK_CONTAINER (frame)->border_width + widget->style->font->ascent);
	  
	  gtk_paint_shadow_gap (widget->style, widget->window,
				GTK_STATE_NORMAL, frame->shadow_type,
				area, widget, "frame",
				allocation->x + x,
				allocation->y + y + height_extra / 2,
				allocation->width - x * 2,
				allocation->height - y * 2 - height_extra / 2,
				GTK_POS_TOP, 
				x2 + 2 - x, frame->label_width - 4);
	  
	  gtk_paint_string (widget->style, widget->window, GTK_WIDGET_STATE (widget),
			    area, widget, "frame",
			    allocation->x + x2 + 3,
			    allocation->y + y2,
			    frame->label);
	}
      else
	gtk_paint_shadow (widget->style, widget->window,
			  GTK_STATE_NORMAL, frame->shadow_type,
			  area, widget, "frame",
			  allocation->x + x,
			  allocation->y + y + height_extra / 2,
			  allocation->width - x * 2,
			  allocation->height - y * 2 - height_extra / 2);
    }
}

/* the only modification to the next two routines is to call
   gtk_aspect_frame_paint instead of gtk_frame_paint */

static void
gtk_aspect_frame_draw (GtkWidget    *widget,
		       GdkRectangle *area)
{
  GtkBin *bin;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ASPECT_FRAME (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);

      gtk_aspect_frame_paint (widget, area);

      if (bin->child && gtk_widget_intersect (bin->child, area, &child_area))
	gtk_widget_draw (bin->child, &child_area);
    }
}

static gint
gtk_aspect_frame_expose (GtkWidget      *widget,
			 GdkEventExpose *event)
{
  GtkBin *bin;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ASPECT_FRAME (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);

      gtk_aspect_frame_paint (widget, &event->area);

      child_event = *event;
      if (bin->child &&
	  GTK_WIDGET_NO_WINDOW (bin->child) &&
	  gtk_widget_intersect (bin->child, &event->area, &child_event.area))
	gtk_widget_event (bin->child, (GdkEvent*) &child_event);
    }

  return FALSE;
}

static void
gtk_aspect_frame_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkFrame *frame;
  GtkAspectFrame *aspect_frame;
  GtkBin *bin;

  GtkAllocation child_allocation;
  gint x,y;
  gint width,height;
  gdouble ratio;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ASPECT_FRAME (widget));
  g_return_if_fail (allocation != NULL);

  aspect_frame = GTK_ASPECT_FRAME (widget);
  frame = GTK_FRAME (widget);
  bin = GTK_BIN (widget);

  if (GTK_WIDGET_DRAWABLE (widget) &&
      ((widget->allocation.x != allocation->x) ||
       (widget->allocation.y != allocation->y) ||
       (widget->allocation.width != allocation->width) ||
       (widget->allocation.height != allocation->height)) &&
      (widget->allocation.width != 0) &&
      (widget->allocation.height != 0))
    gdk_window_clear_area (widget->window,
			   widget->allocation.x,
			   widget->allocation.y,
			   widget->allocation.width,
			   widget->allocation.height);

  widget->allocation = *allocation;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      if (aspect_frame->obey_child)
	{
	  GtkRequisition child_requisition;

	  gtk_widget_get_child_requisition (bin->child, &child_requisition);
	  if (child_requisition.height != 0)
	    {
	      ratio = ((gdouble) child_requisition.width /
		       child_requisition.height);
	      if (ratio < MIN_RATIO)
		ratio = MIN_RATIO;
	    }
	  else if (child_requisition.width != 0)
	    ratio = MAX_RATIO;
	  else
	    ratio = 1.0;
	}
      else
	ratio = aspect_frame->ratio;
      
      x = (GTK_CONTAINER (frame)->border_width +
	   GTK_WIDGET (frame)->style->klass->xthickness);
      width = allocation->width - x * 2;
      
      y = (GTK_CONTAINER (frame)->border_width +
	   MAX (frame->label_height, GTK_WIDGET (frame)->style->klass->ythickness));
      height = (allocation->height - y -
		GTK_CONTAINER (frame)->border_width -
		GTK_WIDGET (frame)->style->klass->ythickness);
      
      /* make sure we don't allocate a negative width or height,
       * since that will be cast to a (very big) guint16 */
      width = MAX (1, width);
      height = MAX (1, height);
      
      if (ratio * height > width)
	{
	  child_allocation.width = width;
	  child_allocation.height = width/ratio + 0.5;
	}
      else
	{
	  child_allocation.width = ratio*height + 0.5;
	  child_allocation.height = height;
	}
      
      child_allocation.x = aspect_frame->xalign * (width - child_allocation.width) + allocation->x + x;
      child_allocation.y = aspect_frame->yalign * (height - child_allocation.height) + allocation->y + y;

      aspect_frame->center_allocation.width = child_allocation.width + 2*x;
      aspect_frame->center_allocation.x = child_allocation.x - x;
      aspect_frame->center_allocation.height = child_allocation.height + y +
				 GTK_CONTAINER (frame)->border_width +
				 GTK_WIDGET (frame)->style->klass->ythickness;
      aspect_frame->center_allocation.y = child_allocation.y - y;

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}
