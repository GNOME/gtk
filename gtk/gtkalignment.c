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
#include "gtkalignment.h"


static void gtk_alignment_class_init    (GtkAlignmentClass *klass);
static void gtk_alignment_init          (GtkAlignment      *alignment);
static void gtk_alignment_size_request  (GtkWidget         *widget,
					 GtkRequisition    *requisition);
static void gtk_alignment_size_allocate (GtkWidget         *widget,
					 GtkAllocation     *allocation);


guint
gtk_alignment_get_type ()
{
  static guint alignment_type = 0;

  if (!alignment_type)
    {
      GtkTypeInfo alignment_info =
      {
	"GtkAlignment",
	sizeof (GtkAlignment),
	sizeof (GtkAlignmentClass),
	(GtkClassInitFunc) gtk_alignment_class_init,
	(GtkObjectInitFunc) gtk_alignment_init,
	(GtkArgFunc) NULL,
      };

      alignment_type = gtk_type_unique (gtk_bin_get_type (), &alignment_info);
    }

  return alignment_type;
}

static void
gtk_alignment_class_init (GtkAlignmentClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->size_request = gtk_alignment_size_request;
  widget_class->size_allocate = gtk_alignment_size_allocate;
}

static void
gtk_alignment_init (GtkAlignment *alignment)
{
  GTK_WIDGET_SET_FLAGS (alignment, GTK_NO_WINDOW | GTK_BASIC);

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
      gtk_widget_size_request (bin->child, &bin->child->requisition);

      requisition->width += bin->child->requisition.width;
      requisition->height += bin->child->requisition.height;
    }
}

static void
gtk_alignment_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GtkAlignment *alignment;
  GtkBin *bin;
  GtkAllocation child_allocation;
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
      x = GTK_CONTAINER (alignment)->border_width;
      y = GTK_CONTAINER (alignment)->border_width;
      width = allocation->width - 2 * x;
      height = allocation->height - 2 * y;

      if (width > bin->child->requisition.width)
	child_allocation.width = (bin->child->requisition.width *
				  (1.0 - alignment->xscale) +
				  width * alignment->xscale);
      else
	child_allocation.width = width;

      if (height > bin->child->requisition.height)
	child_allocation.height = (bin->child->requisition.height *
				  (1.0 - alignment->yscale) +
				  height * alignment->yscale);
      else
	child_allocation.height = height;

      child_allocation.x = alignment->xalign * (width - child_allocation.width) + allocation->x + x;
      child_allocation.y = alignment->yalign * (height - child_allocation.height) + allocation->y + y;

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}
