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
#include "gtkcontainer.h"
#include "gtkpixmap.h"


static void gtk_pixmap_class_init (GtkPixmapClass  *klass);
static void gtk_pixmap_init       (GtkPixmap       *pixmap);
static gint gtk_pixmap_expose     (GtkWidget       *widget,
				   GdkEventExpose  *event);
static void gtk_pixmap_finalize   (GtkObject       *object);

static GtkWidgetClass *parent_class;

GtkType
gtk_pixmap_get_type (void)
{
  static GtkType pixmap_type = 0;

  if (!pixmap_type)
    {
      GtkTypeInfo pixmap_info =
      {
	"GtkPixmap",
	sizeof (GtkPixmap),
	sizeof (GtkPixmapClass),
	(GtkClassInitFunc) gtk_pixmap_class_init,
	(GtkObjectInitFunc) gtk_pixmap_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      pixmap_type = gtk_type_unique (GTK_TYPE_MISC, &pixmap_info);
    }

  return pixmap_type;
}

static void
gtk_pixmap_class_init (GtkPixmapClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  parent_class = gtk_type_class (gtk_widget_get_type ());

  object_class->finalize = gtk_pixmap_finalize;
  widget_class->expose_event = gtk_pixmap_expose;
}

static void
gtk_pixmap_init (GtkPixmap *pixmap)
{
  GTK_WIDGET_SET_FLAGS (pixmap, GTK_NO_WINDOW);

  pixmap->pixmap = NULL;
  pixmap->mask = NULL;
}

GtkWidget*
gtk_pixmap_new (GdkPixmap *val,
		GdkBitmap *mask)
{
  GtkPixmap *pixmap;
   
  g_return_val_if_fail (val != NULL, NULL);
  
  pixmap = gtk_type_new (gtk_pixmap_get_type ());
  
  gtk_pixmap_set (pixmap, val, mask);
  
  return GTK_WIDGET (pixmap);
}

static void
gtk_pixmap_finalize (GtkObject *object)
{
  gtk_pixmap_set (GTK_PIXMAP (object), NULL, NULL);
  (* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

void
gtk_pixmap_set (GtkPixmap *pixmap,
		GdkPixmap *val,
		GdkBitmap *mask)
{
  gint width;
  gint height;
  gint oldwidth;
  gint oldheight;

  g_return_if_fail (pixmap != NULL);
  g_return_if_fail (GTK_IS_PIXMAP (pixmap));

  if (pixmap->pixmap != val)
    {
      oldwidth = GTK_WIDGET (pixmap)->requisition.width;
      oldheight = GTK_WIDGET (pixmap)->requisition.height;
      if (pixmap->pixmap)
	gdk_pixmap_unref (pixmap->pixmap);
      pixmap->pixmap = val;
      if (pixmap->pixmap)
	{
	  gdk_pixmap_ref (pixmap->pixmap);
	  gdk_window_get_size (pixmap->pixmap, &width, &height);
	  GTK_WIDGET (pixmap)->requisition.width =
	    width + GTK_MISC (pixmap)->xpad * 2;
	  GTK_WIDGET (pixmap)->requisition.height =
	    height + GTK_MISC (pixmap)->ypad * 2;
	}
      else
	{
	  GTK_WIDGET (pixmap)->requisition.width = 0;
	  GTK_WIDGET (pixmap)->requisition.height = 0;
	}
      if (GTK_WIDGET_VISIBLE (pixmap))
	{
	  if ((GTK_WIDGET (pixmap)->requisition.width != oldwidth) ||
	      (GTK_WIDGET (pixmap)->requisition.height != oldheight))
	    gtk_widget_queue_resize (GTK_WIDGET (pixmap));
	  else
	    gtk_widget_queue_clear (GTK_WIDGET (pixmap));
	}
    }

  if (pixmap->mask != mask)
    {
      if (pixmap->mask)
	gdk_bitmap_unref (pixmap->mask);
      pixmap->mask = mask;
      if (pixmap->mask)
	gdk_bitmap_ref (pixmap->mask);
    }
}

void
gtk_pixmap_get (GtkPixmap  *pixmap,
		GdkPixmap **val,
		GdkBitmap **mask)
{
  g_return_if_fail (pixmap != NULL);
  g_return_if_fail (GTK_IS_PIXMAP (pixmap));

  if (val)
    *val = pixmap->pixmap;
  if (mask)
    *mask = pixmap->mask;
}


static gint
gtk_pixmap_expose (GtkWidget      *widget,
		   GdkEventExpose *event)
{
  GtkPixmap *pixmap;
  GtkMisc *misc;
  gint x, y;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_PIXMAP (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      pixmap = GTK_PIXMAP (widget);
      misc = GTK_MISC (widget);

      x = (widget->allocation.x * (1.0 - misc->xalign) +
	   (widget->allocation.x + widget->allocation.width
	    - (widget->requisition.width - misc->xpad * 2)) *
	   misc->xalign) + 0.5;
      y = (widget->allocation.y * (1.0 - misc->yalign) +
	   (widget->allocation.y + widget->allocation.height
	    - (widget->requisition.height - misc->ypad * 2)) *
	   misc->yalign) + 0.5;

      if (pixmap->mask)
	{
	  gdk_gc_set_clip_mask (widget->style->black_gc, pixmap->mask);
	  gdk_gc_set_clip_origin (widget->style->black_gc, x, y);
	}

      gdk_draw_pixmap (widget->window,
		       widget->style->black_gc,
		       pixmap->pixmap,
		       0, 0, x, y, -1, -1);

      if (pixmap->mask)
	{
	  gdk_gc_set_clip_mask (widget->style->black_gc, NULL);
	  gdk_gc_set_clip_origin (widget->style->black_gc, 0, 0);
	}
    }
  return FALSE;
}
