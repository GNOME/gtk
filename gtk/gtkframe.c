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

#include <string.h>
#include "gtkframe.h"

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
static void gtk_frame_finalize      (GtkObject      *object);
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
static void gtk_frame_style_set     (GtkWidget      *widget,
				     GtkStyle	    *previous_style);


static GtkBinClass *parent_class = NULL;


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

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = gtk_type_class (gtk_bin_get_type ());

  gtk_object_add_arg_type ("GtkFrame::label", GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_LABEL);
  gtk_object_add_arg_type ("GtkFrame::label_xalign", GTK_TYPE_FLOAT, GTK_ARG_READWRITE, ARG_LABEL_XALIGN);
  gtk_object_add_arg_type ("GtkFrame::label_yalign", GTK_TYPE_FLOAT, GTK_ARG_READWRITE, ARG_LABEL_YALIGN);
  gtk_object_add_arg_type ("GtkFrame::shadow", GTK_TYPE_SHADOW_TYPE, GTK_ARG_READWRITE, ARG_SHADOW);

  object_class->set_arg = gtk_frame_set_arg;
  object_class->get_arg = gtk_frame_get_arg;
  object_class->finalize = gtk_frame_finalize;

  widget_class->draw = gtk_frame_draw;
  widget_class->expose_event = gtk_frame_expose;
  widget_class->size_request = gtk_frame_size_request;
  widget_class->size_allocate = gtk_frame_size_allocate;
  widget_class->style_set = gtk_frame_style_set;
}

static void
gtk_frame_init (GtkFrame *frame)
{
  frame->label = NULL;
  frame->shadow_type = GTK_SHADOW_ETCHED_IN;
  frame->label_width = 0;
  frame->label_height = 0;
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
      GTK_VALUE_STRING (*arg) = g_strdup (frame->label);
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
gtk_frame_style_set (GtkWidget      *widget,
		     GtkStyle	    *previous_style)
{
  GtkFrame *frame;

  frame = GTK_FRAME (widget);

  if (frame->label)
    {
      frame->label_width = gdk_string_measure (GTK_WIDGET (frame)->style->font, frame->label) + 7;
      frame->label_height = (GTK_WIDGET (frame)->style->font->ascent +
			     GTK_WIDGET (frame)->style->font->descent + 1);
    }

  if (GTK_WIDGET_CLASS (parent_class)->style_set)
    GTK_WIDGET_CLASS (parent_class)->style_set (widget, previous_style);
}

void
gtk_frame_set_label (GtkFrame *frame,
		     const gchar *label)
{
  g_return_if_fail (frame != NULL);
  g_return_if_fail (GTK_IS_FRAME (frame));

  if ((label && frame->label && (strcmp (frame->label, label) == 0)) ||
      (!label && !frame->label))
    return;

  if (frame->label)
    g_free (frame->label);
  frame->label = NULL;

  if (label)
    {
      frame->label = g_strdup (label);
      frame->label_width = gdk_string_measure (GTK_WIDGET (frame)->style->font, frame->label) + 7;
      frame->label_height = (GTK_WIDGET (frame)->style->font->ascent +
			     GTK_WIDGET (frame)->style->font->descent + 1);
    }
  else
    {
      frame->label_width = 0;
      frame->label_height = 0;
    }

  if (GTK_WIDGET_DRAWABLE (frame))
    {
      GtkWidget *widget;

      /* clear the old label area
      */
      widget = GTK_WIDGET (frame);
      gtk_widget_queue_clear_area (widget,
				   widget->allocation.x + GTK_CONTAINER (frame)->border_width,
				   widget->allocation.y + GTK_CONTAINER (frame)->border_width,
				   widget->allocation.width - GTK_CONTAINER (frame)->border_width,
				   widget->allocation.y + frame->label_height);

    }
  
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

      if (GTK_WIDGET_DRAWABLE (frame))
	{
          GtkWidget *widget;

          /* clear the old label area
          */
          widget = GTK_WIDGET (frame);
          gtk_widget_queue_clear_area (widget,
				       widget->allocation.x + GTK_CONTAINER (frame)->border_width,
				       widget->allocation.y + GTK_CONTAINER (frame)->border_width,
				       widget->allocation.width - GTK_CONTAINER (frame)->border_width,
				       widget->allocation.y + frame->label_height);

	}
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
gtk_frame_finalize (GtkObject *object)
{
  GtkFrame *frame;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_FRAME (object));

  frame = GTK_FRAME (object);

  if (frame->label)
    g_free (frame->label);

  (* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
gtk_frame_paint (GtkWidget    *widget,
		 GdkRectangle *area)
{
  GtkFrame *frame;
  gint height_extra;
  gint label_area_width;
  gint x, y, x2, y2;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_FRAME (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      frame = GTK_FRAME (widget);

      height_extra = frame->label_height - widget->style->klass->xthickness;
      height_extra = MAX (height_extra, 0);

      x = GTK_CONTAINER (frame)->border_width;
      y = GTK_CONTAINER (frame)->border_width;

      if (frame->label)
	{
	   label_area_width = (widget->allocation.width -
			       GTK_CONTAINER (frame)->border_width * 2 -
			       widget->style->klass->xthickness * 2);
	   
	   x2 = ((label_area_width - frame->label_width) * frame->label_xalign +
		GTK_CONTAINER (frame)->border_width + widget->style->klass->xthickness);
	   y2 = (GTK_CONTAINER (frame)->border_width + widget->style->font->ascent);

	   gtk_paint_shadow_gap (widget->style, widget->window,
				 GTK_STATE_NORMAL, frame->shadow_type,
				 area, widget, "frame",
				 widget->allocation.x + x,
				 widget->allocation.y + y + height_extra / 2,
				 widget->allocation.width - x * 2,
				 widget->allocation.height - y * 2 - height_extra / 2,
				 GTK_POS_TOP, 
				 x2 + 2 - x, frame->label_width - 4);
	   
	   gtk_paint_string (widget->style, widget->window, GTK_WIDGET_STATE (widget),
			     area, widget, "frame",
			     widget->allocation.x + x2 + 3,
			     widget->allocation.y + y2,
			     frame->label);
	}
       else
	 gtk_paint_shadow (widget->style, widget->window,
			   GTK_STATE_NORMAL, frame->shadow_type,
			   area, widget, "frame",
			   widget->allocation.x + x,
			   widget->allocation.y + y + height_extra / 2,
			   widget->allocation.width - x * 2,
			   widget->allocation.height - y * 2 - height_extra / 2);
    }
}

static void
gtk_frame_draw (GtkWidget    *widget,
		GdkRectangle *area)
{
  GtkBin *bin;
  GdkRectangle child_area;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_FRAME (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);

      gtk_frame_paint (widget, area);

      if (bin->child && gtk_widget_intersect (bin->child, area, &child_area))
	gtk_widget_draw (bin->child, &child_area);
    }
}

static gint
gtk_frame_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkBin *bin;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_FRAME (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      bin = GTK_BIN (widget);

      gtk_frame_paint (widget, &event->area);

      child_event = *event;
      if (bin->child &&
	  GTK_WIDGET_NO_WINDOW (bin->child) &&
	  gtk_widget_intersect (bin->child, &event->area, &child_event.area))
	gtk_widget_event (bin->child, (GdkEvent*) &child_event);
    }

  return FALSE;
}

static void
gtk_frame_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  GtkFrame *frame;
  GtkBin *bin;
  gint tmp_height;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_FRAME (widget));
  g_return_if_fail (requisition != NULL);

  frame = GTK_FRAME (widget);
  bin = GTK_BIN (widget);

  requisition->width = (GTK_CONTAINER (widget)->border_width +
			GTK_WIDGET (widget)->style->klass->xthickness) * 2;

  tmp_height = frame->label_height - GTK_WIDGET (widget)->style->klass->ythickness;
  tmp_height = MAX (tmp_height, 0);

  requisition->height = tmp_height + (GTK_CONTAINER (widget)->border_width +
				      GTK_WIDGET (widget)->style->klass->ythickness) * 2;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      GtkRequisition child_requisition;
      
      gtk_widget_size_request (bin->child, &child_requisition);

      requisition->width += MAX (child_requisition.width, frame->label_width);
      requisition->height += child_requisition.height;
    }
  else
    {
      requisition->width += frame->label_width;
    }
}

static void
gtk_frame_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkFrame *frame;
  GtkBin *bin;
  GtkAllocation child_allocation;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_FRAME (widget));
  g_return_if_fail (allocation != NULL);

  frame = GTK_FRAME (widget);
  bin = GTK_BIN (widget);

  if (GTK_WIDGET_MAPPED (widget) &&
      ((widget->allocation.x != allocation->x) ||
       (widget->allocation.y != allocation->y) ||
       (widget->allocation.width != allocation->width) ||
       (widget->allocation.height != allocation->height)) &&
      (widget->allocation.width != 0) &&
      (widget->allocation.height != 0))
     gtk_widget_queue_clear (widget);

  widget->allocation = *allocation;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      child_allocation.x = (GTK_CONTAINER (frame)->border_width +
			    GTK_WIDGET (frame)->style->klass->xthickness);
      child_allocation.width = MAX(1, (gint)allocation->width - child_allocation.x * 2);

      child_allocation.y = (GTK_CONTAINER (frame)->border_width +
			    MAX (frame->label_height, GTK_WIDGET (frame)->style->klass->ythickness));
      child_allocation.height = MAX (1, ((gint)allocation->height - child_allocation.y -
					 (gint)GTK_CONTAINER (frame)->border_width -
					 (gint)GTK_WIDGET (frame)->style->klass->ythickness));

      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}
