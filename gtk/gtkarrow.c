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
#include "gtkarrow.h"


#define MIN_ARROW_SIZE  11


static void gtk_arrow_class_init (GtkArrowClass  *klass);
static void gtk_arrow_init       (GtkArrow       *arrow);
static gint gtk_arrow_expose     (GtkWidget      *widget,
				  GdkEventExpose *event);


guint
gtk_arrow_get_type ()
{
  static guint arrow_type = 0;

  if (!arrow_type)
    {
      GtkTypeInfo arrow_info =
      {
	"GtkArrow",
	sizeof (GtkArrow),
	sizeof (GtkArrowClass),
	(GtkClassInitFunc) gtk_arrow_class_init,
	(GtkObjectInitFunc) gtk_arrow_init,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
      };

      arrow_type = gtk_type_unique (gtk_misc_get_type (), &arrow_info);
    }

  return arrow_type;
}

static void
gtk_arrow_class_init (GtkArrowClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->expose_event = gtk_arrow_expose;
}

static void
gtk_arrow_init (GtkArrow *arrow)
{
  GTK_WIDGET_SET_FLAGS (arrow, GTK_NO_WINDOW);

  arrow->arrow_type = GTK_ARROW_RIGHT;
  arrow->shadow_type = GTK_SHADOW_OUT;
}

GtkWidget*
gtk_arrow_new (GtkArrowType  arrow_type,
	       GtkShadowType shadow_type)
{
  GtkArrow *arrow;

  arrow = gtk_type_new (gtk_arrow_get_type ());

  GTK_WIDGET (arrow)->requisition.width = MIN_ARROW_SIZE + GTK_MISC (arrow)->xpad * 2;
  GTK_WIDGET (arrow)->requisition.height = MIN_ARROW_SIZE + GTK_MISC (arrow)->ypad * 2;

  arrow->arrow_type = arrow_type;
  arrow->shadow_type = shadow_type;

  return GTK_WIDGET (arrow);
}

void
gtk_arrow_set (GtkArrow      *arrow,
	       GtkArrowType   arrow_type,
	       GtkShadowType  shadow_type)
{
  g_return_if_fail (arrow != NULL);
  g_return_if_fail (GTK_IS_ARROW (arrow));

  if (((GtkArrowType) arrow->arrow_type != arrow_type) ||
      ((GtkShadowType) arrow->shadow_type != shadow_type))
    {
      arrow->arrow_type = arrow_type;
      arrow->shadow_type = shadow_type;

      if (GTK_WIDGET_DRAWABLE (arrow))
	{
	  gdk_window_clear_area (GTK_WIDGET (arrow)->window,
				 GTK_WIDGET (arrow)->allocation.x + 1,
				 GTK_WIDGET (arrow)->allocation.y + 1,
				 GTK_WIDGET (arrow)->allocation.width - 2,
				 GTK_WIDGET (arrow)->allocation.height - 2);
	  gtk_widget_queue_draw (GTK_WIDGET (arrow));
	}
    }
}


static gint
gtk_arrow_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkArrow *arrow;
  GtkMisc *misc;
  GtkShadowType shadow_type;
  gint width, height;
  gint x, y;
  gint extent;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ARROW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      arrow = GTK_ARROW (widget);
      misc = GTK_MISC (widget);

      width = widget->allocation.width - misc->xpad * 2;
      height = widget->allocation.height - misc->ypad * 2;
      extent = MIN (width, height);

      x = ((widget->allocation.x + misc->xpad) * (1.0 - misc->xalign) +
	   (widget->allocation.x + widget->allocation.width - extent - misc->ypad) * misc->xalign);
      y = ((widget->allocation.y + misc->ypad) * (1.0 - misc->yalign) +
	   (widget->allocation.y + widget->allocation.height - extent - misc->ypad) * misc->yalign);

      shadow_type = arrow->shadow_type;

      if (widget->state == GTK_STATE_ACTIVE)
	{
          if (shadow_type == GTK_SHADOW_IN)
            shadow_type = GTK_SHADOW_OUT;
          else if (shadow_type == GTK_SHADOW_OUT)
            shadow_type = GTK_SHADOW_IN;
          else if (shadow_type == GTK_SHADOW_ETCHED_IN)
            shadow_type = GTK_SHADOW_ETCHED_OUT;
          else if (shadow_type == GTK_SHADOW_ETCHED_OUT)
            shadow_type = GTK_SHADOW_ETCHED_IN;
	}

      gtk_draw_arrow (widget->style, widget->window,
		      widget->state, shadow_type, arrow->arrow_type, TRUE,
		      x, y, extent, extent);
    }

  return FALSE;
}
