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

#include "gtkalignment.h"


enum {
  ARG_0,
  ARG_XALIGN,
  ARG_YALIGN,
  ARG_XSCALE,
  ARG_YSCALE
};


static void gtk_alignment_class_init    (GtkAlignmentClass *klass);
static void gtk_alignment_init          (GtkAlignment      *alignment);
static void gtk_alignment_size_request  (GtkWidget         *widget,
					 GtkRequisition    *requisition);
static void gtk_alignment_size_allocate (GtkWidget         *widget,
					 GtkAllocation     *allocation);
static void gtk_alignment_set_arg       (GtkObject         *object,
					 GtkArg            *arg,
					 guint              arg_id);
static void gtk_alignment_get_arg       (GtkObject         *object,
					 GtkArg            *arg,
					 guint              arg_id);



GtkType
gtk_alignment_get_type (void)
{
  static GtkType alignment_type = 0;

  if (!alignment_type)
    {
      static const GtkTypeInfo alignment_info =
      {
	"GtkAlignment",
	sizeof (GtkAlignment),
	sizeof (GtkAlignmentClass),
	(GtkClassInitFunc) gtk_alignment_class_init,
	(GtkObjectInitFunc) gtk_alignment_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      alignment_type = gtk_type_unique (GTK_TYPE_BIN, &alignment_info);
    }

  return alignment_type;
}

static void
gtk_alignment_class_init (GtkAlignmentClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  gtk_object_add_arg_type ("GtkAlignment::xalign", GTK_TYPE_FLOAT, GTK_ARG_READWRITE, ARG_XALIGN);
  gtk_object_add_arg_type ("GtkAlignment::yalign", GTK_TYPE_FLOAT, GTK_ARG_READWRITE, ARG_YALIGN);
  gtk_object_add_arg_type ("GtkAlignment::xscale", GTK_TYPE_FLOAT, GTK_ARG_READWRITE, ARG_XSCALE);
  gtk_object_add_arg_type ("GtkAlignment::yscale", GTK_TYPE_FLOAT, GTK_ARG_READWRITE, ARG_YSCALE);

  object_class->set_arg = gtk_alignment_set_arg;
  object_class->get_arg = gtk_alignment_get_arg;

  widget_class->size_request = gtk_alignment_size_request;
  widget_class->size_allocate = gtk_alignment_size_allocate;
}

static void
gtk_alignment_init (GtkAlignment *alignment)
{
  GTK_WIDGET_SET_FLAGS (alignment, GTK_NO_WINDOW);

  alignment->xalign = 0.5;
  alignment->yalign = 0.5;
  alignment->xscale = 1.0;
  alignment->yscale = 1.0;
}

GtkWidget*
gtk_alignment_new (gfloat xalign,
		   gfloat yalign,
		   gfloat xscale,
		   gfloat yscale)
{
  GtkAlignment *alignment;

  alignment = gtk_type_new (gtk_alignment_get_type ());

  alignment->xalign = CLAMP (xalign, 0.0, 1.0);
  alignment->yalign = CLAMP (yalign, 0.0, 1.0);
  alignment->xscale = CLAMP (xscale, 0.0, 1.0);
  alignment->yscale = CLAMP (yscale, 0.0, 1.0);

  return GTK_WIDGET (alignment);
}

static void
gtk_alignment_set_arg (GtkObject         *object,
		       GtkArg            *arg,
		       guint              arg_id)
{
  GtkAlignment *alignment;

  alignment = GTK_ALIGNMENT (object);

  switch (arg_id)
    {
    case ARG_XALIGN:
      gtk_alignment_set (alignment,
			 GTK_VALUE_FLOAT (*arg),
			 alignment->yalign,
			 alignment->xscale,
			 alignment->yscale);
      break;
    case ARG_YALIGN:
      gtk_alignment_set (alignment,
			 alignment->xalign,
			 GTK_VALUE_FLOAT (*arg),
			 alignment->xscale,
			 alignment->yscale);
      break;
    case ARG_XSCALE:
      gtk_alignment_set (alignment,
			 alignment->xalign,
			 alignment->yalign,
			 GTK_VALUE_FLOAT (*arg),
			 alignment->yscale);
      break;
    case ARG_YSCALE:
      gtk_alignment_set (alignment,
			 alignment->xalign,
			 alignment->yalign,
			 alignment->xscale,
			 GTK_VALUE_FLOAT (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_alignment_get_arg (GtkObject         *object,
		       GtkArg            *arg,
		       guint              arg_id)
{
  GtkAlignment *alignment;

  alignment = GTK_ALIGNMENT (object);

  switch (arg_id)
    {
    case ARG_XALIGN:
      GTK_VALUE_FLOAT (*arg) = alignment->xalign;
      break;
    case ARG_YALIGN:
      GTK_VALUE_FLOAT (*arg) = alignment->yalign;
      break;
    case ARG_XSCALE:
      GTK_VALUE_FLOAT (*arg) = alignment->xscale;
      break;
    case ARG_YSCALE:
      GTK_VALUE_FLOAT (*arg) = alignment->yscale;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

void
gtk_alignment_set (GtkAlignment *alignment,
		   gfloat        xalign,
		   gfloat        yalign,
		   gfloat        xscale,
		   gfloat        yscale)
{
  g_return_if_fail (alignment != NULL);
  g_return_if_fail (GTK_IS_ALIGNMENT (alignment));

  xalign = CLAMP (xalign, 0.0, 1.0);
  yalign = CLAMP (yalign, 0.0, 1.0);
  xscale = CLAMP (xscale, 0.0, 1.0);
  yscale = CLAMP (yscale, 0.0, 1.0);

  if ((alignment->xalign != xalign) ||
      (alignment->yalign != yalign) ||
      (alignment->xscale != xscale) ||
      (alignment->yscale != yscale))
    {
      alignment->xalign = xalign;
      alignment->yalign = yalign;
      alignment->xscale = xscale;
      alignment->yscale = yscale;

      gtk_widget_size_allocate (GTK_WIDGET (alignment), &(GTK_WIDGET (alignment)->allocation));
      gtk_widget_queue_draw (GTK_WIDGET (alignment));
    }
}


static void
gtk_alignment_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkAlignment *alignment;
  GtkBin *bin;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ALIGNMENT (widget));
  g_return_if_fail (requisition != NULL);

  alignment = GTK_ALIGNMENT (widget);
  bin = GTK_BIN (widget);

  requisition->width = GTK_CONTAINER (widget)->border_width * 2;
  requisition->height = GTK_CONTAINER (widget)->border_width * 2;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      GtkRequisition child_requisition;
      
      gtk_widget_size_request (bin->child, &child_requisition);

      requisition->width += child_requisition.width;
      requisition->height += child_requisition.height;
    }
}

static void
gtk_alignment_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GtkAlignment *alignment;
  GtkBin *bin;
  GtkAllocation child_allocation;
  GtkRequisition child_requisition;
  gint width, height;
  gint x, y;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ALIGNMENT (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  alignment = GTK_ALIGNMENT (widget);
  bin = GTK_BIN (widget);
  
  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gtk_widget_get_child_requisition (bin->child, &child_requisition);
      
      x = GTK_CONTAINER (alignment)->border_width;
      y = GTK_CONTAINER (alignment)->border_width;
      width = MAX (allocation->width - 2 * x, 0);
      height = MAX (allocation->height - 2 * y, 0);
      
      if (width > child_requisition.width)
	child_allocation.width = (child_requisition.width *
				  (1.0 - alignment->xscale) +
				  width * alignment->xscale);
      else
	child_allocation.width = width;
      
      if (height > child_requisition.height)
	child_allocation.height = (child_requisition.height *
				   (1.0 - alignment->yscale) +
				   height * alignment->yscale);
      else
	child_allocation.height = height;

      child_allocation.x = alignment->xalign * (width - child_allocation.width) + allocation->x + x;
      child_allocation.y = alignment->yalign * (height - child_allocation.height) + allocation->y + y;

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}
