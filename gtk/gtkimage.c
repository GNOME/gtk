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
#include "gtkcontainer.h"
#include "gtkimage.h"


static void gtk_image_class_init (GtkImageClass  *klass);
static void gtk_image_init       (GtkImage       *image);
static gint gtk_image_expose     (GtkWidget      *widget,
				  GdkEventExpose *event);


guint
gtk_image_get_type ()
{
  static guint image_type = 0;

  if (!image_type)
    {
      GtkTypeInfo image_info =
      {
	"GtkImage",
	sizeof (GtkImage),
	sizeof (GtkImageClass),
	(GtkClassInitFunc) gtk_image_class_init,
	(GtkObjectInitFunc) gtk_image_init,
	(GtkArgFunc) NULL,
      };

      image_type = gtk_type_unique (gtk_misc_get_type (), &image_info);
    }

  return image_type;
}

static void
gtk_image_class_init (GtkImageClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  widget_class->expose_event = gtk_image_expose;
}

static void
gtk_image_init (GtkImage *image)
{
  GTK_WIDGET_SET_FLAGS (image, GTK_NO_WINDOW);

  image->image = NULL;
  image->mask = NULL;
}

GtkWidget*
gtk_image_new (GdkImage  *val,
	       GdkBitmap *mask)
{
  GtkImage *image;

  g_return_val_if_fail (val != NULL, NULL);

  image = gtk_type_new (gtk_image_get_type ());

  gtk_image_set (image, val, mask);

  return GTK_WIDGET (image);
}

void
gtk_image_set (GtkImage  *image,
	       GdkImage  *val,
	       GdkBitmap *mask)
{
  g_return_if_fail (image != NULL);
  g_return_if_fail (GTK_IS_IMAGE (image));

  image->image = val;
  image->mask = mask;

  if (image->image)
    {
      GTK_WIDGET (image)->requisition.width = image->image->width + GTK_MISC (image)->xpad * 2;
      GTK_WIDGET (image)->requisition.height = image->image->height + GTK_MISC (image)->ypad * 2;
    }
  else
    {
      GTK_WIDGET (image)->requisition.width = 0;
      GTK_WIDGET (image)->requisition.height = 0;
    }

  if (GTK_WIDGET_VISIBLE (image))
    gtk_widget_queue_resize (GTK_WIDGET (image));
}

void
gtk_image_get (GtkImage   *image,
	       GdkImage  **val,
	       GdkBitmap **mask)
{
  g_return_if_fail (image != NULL);
  g_return_if_fail (GTK_IS_IMAGE (image));

  if (val)
    *val = image->image;
  if (mask)
    *mask = image->mask;
}


static gint
gtk_image_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkImage *image;
  GtkMisc *misc;
  GdkRectangle area;
  gint x, y;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_IMAGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget))
    {
      image = GTK_IMAGE (widget);
      misc = GTK_MISC (widget);

      x = (widget->allocation.x * (1.0 - misc->xalign) +
	   (widget->allocation.x + widget->allocation.width
	    - (widget->requisition.width - misc->xpad * 2)) *
	   misc->xalign) + 0.5;
      y = (widget->allocation.y * (1.0 - misc->yalign) +
	   (widget->allocation.y + widget->allocation.height
	    - (widget->requisition.height - misc->ypad * 2)) *
	   misc->yalign) + 0.5;

      if (image->mask)
	{
	  gdk_gc_set_clip_mask (widget->style->black_gc, image->mask);
	  gdk_gc_set_clip_origin (widget->style->black_gc, x, y);
	}

      area = event->area;
      if ((area.x < 0) || (area.y < 0))
	{
	  area.x = area.y = 0;
	  area.width = image->image->width;
	  area.height = image->image->height;
	}

      gdk_draw_image (widget->window,
		      widget->style->black_gc,
		      image->image,
		      area.x, area.y, x+area.x, y+area.y,
		      area.width, area.height);

      if (image->mask)
	{
	  gdk_gc_set_clip_mask (widget->style->black_gc, NULL);
	  gdk_gc_set_clip_origin (widget->style->black_gc, 0, 0);
	}
    }

  return FALSE;
}
