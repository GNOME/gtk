/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * Insensitive pixmap building code by Eckehard Berns from GNOME Stock
 * Copyright (C) 1997, 1998 Free Software Foundation
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

#include "gtkcontainer.h"
#include "gtkpixmap.h"


static void gtk_pixmap_class_init (GtkPixmapClass  *klass);
static void gtk_pixmap_init       (GtkPixmap       *pixmap);
static gint gtk_pixmap_expose     (GtkWidget       *widget,
				   GdkEventExpose  *event);
static void gtk_pixmap_finalize   (GtkObject       *object);
static void build_insensitive_pixmap (GtkPixmap *gtkpixmap);

static GtkWidgetClass *parent_class;

GtkType
gtk_pixmap_get_type (void)
{
  static GtkType pixmap_type = 0;

  if (!pixmap_type)
    {
      static const GtkTypeInfo pixmap_info =
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
  
  pixmap->build_insensitive = TRUE;
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
      if (pixmap->pixmap_insensitive)
	gdk_pixmap_unref (pixmap->pixmap_insensitive);
      pixmap->pixmap = val;
      pixmap->pixmap_insensitive = NULL;
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

      if (GTK_WIDGET_STATE (widget) == GTK_STATE_INSENSITIVE
          && pixmap->build_insensitive)
        {
	  if (!pixmap->pixmap_insensitive)
	    build_insensitive_pixmap (pixmap);
          gdk_draw_pixmap (widget->window,
	   	           widget->style->black_gc,
		           pixmap->pixmap_insensitive,
		           0, 0, x, y, -1, -1);
        }
      else
	{
          gdk_draw_pixmap (widget->window,
	   	           widget->style->black_gc,
		           pixmap->pixmap,
		           0, 0, x, y, -1, -1);
	}

      if (pixmap->mask)
	{
	  gdk_gc_set_clip_mask (widget->style->black_gc, NULL);
	  gdk_gc_set_clip_origin (widget->style->black_gc, 0, 0);
	}
    }
  return FALSE;
}

void
gtk_pixmap_set_build_insensitive (GtkPixmap *pixmap, guint build)
{
  g_return_if_fail (pixmap != NULL);
  g_return_if_fail (GTK_IS_PIXMAP (pixmap));

  pixmap->build_insensitive = build;

  if (GTK_WIDGET_VISIBLE (pixmap))
    {
      gtk_widget_queue_clear (GTK_WIDGET (pixmap));
    }
}

static void
build_insensitive_pixmap(GtkPixmap *gtkpixmap)
{
  GdkGC *gc;
  GdkPixmap *pixmap = gtkpixmap->pixmap;
  GdkPixmap *insensitive;
  gint w, h, x, y;
  GdkGCValues vals;
  GdkVisual *visual;
  GdkImage *image;
  GdkColorContext *cc;
  GdkColor color;
  GdkColormap *cmap;
  gint32 red, green, blue;
  GtkStyle *style;
  GtkWidget *window;
  GdkColor c;
  int failed;

  window = GTK_WIDGET (gtkpixmap);

  g_return_if_fail(window != NULL);

  gdk_window_get_size(pixmap, &w, &h);
  image = gdk_image_get(pixmap, 0, 0, w, h);
  insensitive = gdk_pixmap_new(GTK_WIDGET (gtkpixmap)->window, w, h, -1);
  gc = gdk_gc_new (pixmap);

  visual = gtk_widget_get_visual(GTK_WIDGET(gtkpixmap));
  cmap = gtk_widget_get_colormap(GTK_WIDGET(gtkpixmap));
  cc = gdk_color_context_new(visual, cmap);

  if ((cc->mode != GDK_CC_MODE_TRUE) && (cc->mode != GDK_CC_MODE_MY_GRAY)) 
    {
      gdk_draw_image(insensitive, gc, image, 0, 0, 0, 0, w, h);

      style = gtk_widget_get_style(window);
      color = style->bg[0];
      gdk_gc_set_foreground (gc, &color);
      for (y = 0; y < h; y++) 
        {
          for (x = y % 2; x < w; x += 2) 
	    {
              gdk_draw_point(insensitive, gc, x, y);
            }
        }
    }
  else
    {
      gdk_gc_get_values(gc, &vals);
      style = gtk_widget_get_style(window);

      color = style->bg[0];
      red = color.red;
      green = color.green;
      blue = color.blue;

      for (y = 0; y < h; y++) 
	{
	  for (x = 0; x < w; x++) 
	    {
	      c.pixel = gdk_image_get_pixel(image, x, y);
	      gdk_color_context_query_color(cc, &c);
	      c.red = (((gint32)c.red - red) >> 1) + red;
	      c.green = (((gint32)c.green - green) >> 1) + green;
	      c.blue = (((gint32)c.blue - blue) >> 1) + blue;
	      c.pixel = gdk_color_context_get_pixel(cc, c.red, c.green, c.blue,
						    &failed);
	      gdk_image_put_pixel(image, x, y, c.pixel);
	    }
	}

      for (y = 0; y < h; y++) 
	{
	  for (x = y % 2; x < w; x += 2) 
	    {
	      c.pixel = gdk_image_get_pixel(image, x, y);
	      gdk_color_context_query_color(cc, &c);
	      c.red = (((gint32)c.red - red) >> 1) + red;
	      c.green = (((gint32)c.green - green) >> 1) + green;
	      c.blue = (((gint32)c.blue - blue) >> 1) + blue;
	      c.pixel = gdk_color_context_get_pixel(cc, c.red, c.green, c.blue,
						    &failed);
	      gdk_image_put_pixel(image, x, y, c.pixel);
	    }
	}

      gdk_draw_image(insensitive, gc, image, 0, 0, 0, 0, w, h);
    }

  gtkpixmap->pixmap_insensitive = insensitive;

  gdk_image_destroy(image);
  gdk_color_context_free(cc);
  gdk_gc_destroy(gc);
}


