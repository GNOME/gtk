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
#include "gtkframe.h"


static void gtk_frame_class_init    (GtkFrameClass  *klass);
static void gtk_frame_init          (GtkFrame       *frame);
static void gtk_frame_destroy       (GtkObject      *object);
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


static GtkBinClass *parent_class = NULL;


guint
gtk_frame_get_type ()
{
  static guint frame_type = 0;

  if (!frame_type)
    {
      GtkTypeInfo frame_info =
      {
	"GtkFrame",
	sizeof (GtkFrame),
	sizeof (GtkFrameClass),
	(GtkClassInitFunc) gtk_frame_class_init,
	(GtkObjectInitFunc) gtk_frame_init,
	(GtkArgFunc) NULL,
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

  object_class->destroy = gtk_frame_destroy;

  widget_class->draw = gtk_frame_draw;
  widget_class->expose_event = gtk_frame_expose;
  widget_class->size_request = gtk_frame_size_request;
  widget_class->size_allocate = gtk_frame_size_allocate;
}

static void
gtk_frame_init (GtkFrame *frame)
{
  GTK_WIDGET_SET_FLAGS (frame, GTK_BASIC);

  frame->label = NULL;
  frame->shadow_type = GTK_SHADOW_ETCHED_IN;
  frame->label_width = 0;
  frame->label_height = 0;
  frame->label_xalign = 0.0;
  frame->label_yalign = 0.5;
}

GtkWidget*
gtk_frame_new (const gchar *label)
{
  GtkFrame *frame;

  frame = gtk_type_new (gtk_frame_get_type ());

  gtk_frame_set_label (frame, label);

  return GTK_WIDGET (frame);
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
      gdk_window_clear_area (widget->window,
                             widget->allocation.x + GTK_CONTAINER (frame)->border_width,
                             widget->allocation.y + GTK_CONTAINER (frame)->border_width,
                             widget->allocation.width - GTK_CONTAINER (frame)->border_width,
                             widget->allocation.y + frame->label_height);

      gtk_widget_queue_resize (GTK_WIDGET (frame));
    }
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

      if (GTK_WIDGET_VISIBLE (frame))
	{
          GtkWidget *widget;

          /* clear the old label area
          */
          widget = GTK_WIDGET (frame);
          gdk_window_clear_area (widget->window,
                                 widget->allocation.x + GTK_CONTAINER (frame)->border_width,
                                 widget->allocation.y + GTK_CONTAINER (frame)->border_width,
                                 widget->allocation.width - GTK_CONTAINER (frame)->border_width,
                                 widget->allocation.y + frame->label_height);

	  gtk_widget_size_allocate (GTK_WIDGET (frame), &(GTK_WIDGET (frame)->allocation));
	  gtk_widget_queue_draw (GTK_WIDGET (frame));
	}
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

      if (GTK_WIDGET_MAPPED (frame))
	{
	  gdk_window_clear_area (GTK_WIDGET (frame)->window,
				 GTK_WIDGET (frame)->allocation.x,
				 GTK_WIDGET (frame)->allocation.y,
				 GTK_WIDGET (frame)->allocation.width,
				 GTK_WIDGET (frame)->allocation.height);
	  gtk_widget_size_allocate (GTK_WIDGET (frame), &(GTK_WIDGET (frame)->allocation));
	  gtk_widget_queue_draw (GTK_WIDGET (frame));
	}
    }
}


static void
gtk_frame_destroy (GtkObject *object)
{
  GtkFrame *frame;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_FRAME (object));

  frame = GTK_FRAME (object);

  if (frame->label)
    g_free (frame->label);

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_frame_paint (GtkWidget    *widget,
		 GdkRectangle *area)
{
  GtkFrame *frame;
  GtkStateType state;
  gint height_extra;
  gint label_area_width;
  gint x, y;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_FRAME (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      frame = GTK_FRAME (widget);

      state = widget->state;
      if (!GTK_WIDGET_IS_SENSITIVE (widget))
        state = GTK_STATE_INSENSITIVE;

      height_extra = frame->label_height - widget->style->klass->xthickness;
      height_extra = MAX (height_extra, 0);

      x = GTK_CONTAINER (frame)->border_width;
      y = GTK_CONTAINER (frame)->border_width;

      gtk_draw_shadow (widget->style, widget->window,
		       GTK_STATE_NORMAL, frame->shadow_type,
		       widget->allocation.x + x,
		       widget->allocation.y + y + height_extra / 2,
		       widget->allocation.width - x * 2,
		       widget->allocation.height - y * 2 - height_extra / 2);

      if (frame->label)
	{
	  label_area_width = (widget->allocation.width -
			      GTK_CONTAINER (frame)->border_width * 2 -
			      widget->style->klass->xthickness * 2);

	  x = ((label_area_width - frame->label_width) * frame->label_xalign +
	       GTK_CONTAINER (frame)->border_width + widget->style->klass->xthickness);
	  y = (GTK_CONTAINER (frame)->border_width + widget->style->font->ascent);

	  gdk_window_clear_area (widget->window,
				 widget->allocation.x + x + 2,
				 widget->allocation.y + GTK_CONTAINER (frame)->border_width,
				 frame->label_width - 4,
				 frame->label_height);
	  gtk_draw_string (widget->style, widget->window, state,
			   widget->allocation.x + x + 3,
			   widget->allocation.y + y,
			   frame->label);
	}
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
      gtk_widget_size_request (bin->child, &bin->child->requisition);

      requisition->width += MAX (bin->child->requisition.width, frame->label_width);
      requisition->height += bin->child->requisition.height;
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
    gdk_window_clear_area (widget->window,
			   widget->allocation.x,
			   widget->allocation.y,
			   widget->allocation.width,
			   widget->allocation.height);

  widget->allocation = *allocation;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      child_allocation.x = (GTK_CONTAINER (frame)->border_width +
			    GTK_WIDGET (frame)->style->klass->xthickness);
      child_allocation.width = MAX(0, allocation->width - child_allocation.x * 2);

      child_allocation.y = (GTK_CONTAINER (frame)->border_width +
			    MAX (frame->label_height, GTK_WIDGET (frame)->style->klass->ythickness));
      child_allocation.height = MAX (0, (allocation->height - child_allocation.y -
					 GTK_CONTAINER (frame)->border_width -
					 GTK_WIDGET (frame)->style->klass->ythickness));

      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}
