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

#include "gtkcontainer.h"
#include "gtkimage.h"
#include "gtkiconfactory.h"

static void gtk_image_class_init   (GtkImageClass  *klass);
static void gtk_image_init         (GtkImage       *image);
static gint gtk_image_expose       (GtkWidget      *widget,
                                    GdkEventExpose *event);
static void gtk_image_size_request (GtkWidget      *widget,
                                    GtkRequisition *requisition);
static void gtk_image_clear        (GtkImage       *image);
static void gtk_image_update_size  (GtkImage       *image,
                                    gint            image_width,
                                    gint            image_height);

static gpointer parent_class;

GtkType
gtk_image_get_type (void)
{
  static GtkType image_type = 0;

  if (!image_type)
    {
      static const GtkTypeInfo image_info =
      {
	"GtkImage",
	sizeof (GtkImage),
	sizeof (GtkImageClass),
	(GtkClassInitFunc) gtk_image_class_init,
	(GtkObjectInitFunc) gtk_image_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      image_type = gtk_type_unique (GTK_TYPE_MISC, &image_info);
    }

  return image_type;
}

static void
gtk_image_class_init (GtkImageClass *class)
{
  GtkWidgetClass *widget_class;

  parent_class = g_type_class_peek_parent (class);
  
  widget_class = (GtkWidgetClass*) class;

  widget_class->expose_event = gtk_image_expose;
  widget_class->size_request = gtk_image_size_request;
}

static void
gtk_image_init (GtkImage *image)
{
  GTK_WIDGET_SET_FLAGS (image, GTK_NO_WINDOW);

  image->storage_type = GTK_IMAGE_EMPTY;
}

GtkWidget*
gtk_image_new_from_pixmap (GdkPixmap *pixmap,
                           GdkBitmap *mask)
{
  GtkImage *image;

  image = gtk_type_new (GTK_TYPE_IMAGE);

  gtk_image_set_from_pixmap (image, pixmap, mask);

  return GTK_WIDGET (image);
}

GtkWidget*
gtk_image_new_from_image  (GdkImage  *gdk_image,
                           GdkBitmap *mask)
{
  GtkImage *image;

  image = gtk_type_new (GTK_TYPE_IMAGE);

  gtk_image_set_from_image (image, gdk_image, mask);

  return GTK_WIDGET (image);
}

GtkWidget*
gtk_image_new_from_file   (const gchar *filename)
{
  GtkImage *image;

  image = gtk_type_new (GTK_TYPE_IMAGE);

  gtk_image_set_from_file (image, filename);

  return GTK_WIDGET (image);
}

GtkWidget*
gtk_image_new_from_pixbuf (GdkPixbuf *pixbuf)
{
  GtkImage *image;

  image = gtk_type_new (GTK_TYPE_IMAGE);

  gtk_image_set_from_pixbuf (image, pixbuf);

  return GTK_WIDGET (image);  
}

GtkWidget*
gtk_image_new_from_stock (const gchar    *stock_id,
                          const gchar    *size)
{
  GtkImage *image;

  image = gtk_type_new (GTK_TYPE_IMAGE);

  gtk_image_set_from_stock (image, stock_id, size);

  return GTK_WIDGET (image);
}

GtkWidget*
gtk_image_new_from_icon_set (GtkIconSet     *icon_set,
                             const gchar    *size)
{
  GtkImage *image;

  image = gtk_type_new (GTK_TYPE_IMAGE);

  gtk_image_set_from_icon_set (image, icon_set, size);

  return GTK_WIDGET (image);
}

void
gtk_image_set_from_pixmap (GtkImage  *image,
                           GdkPixmap *pixmap,
                           GdkBitmap *mask)
{
  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (pixmap == NULL ||
                    GDK_IS_PIXMAP (pixmap));
  g_return_if_fail (mask == NULL ||
                    GDK_IS_PIXMAP (mask));
  
  if (pixmap)
    g_object_ref (G_OBJECT (pixmap));

  if (mask)
    g_object_ref (G_OBJECT (mask));

  gtk_image_clear (image);

  if (pixmap)
    {
      int width;
      int height;
      
      image->storage_type = GTK_IMAGE_PIXMAP;

      image->data.pixmap.pixmap = pixmap;
      image->data.pixmap.mask = mask;

      gdk_drawable_get_size (GDK_DRAWABLE (pixmap), &width, &height);

      gtk_image_update_size (image, width, height);
    }
  else
    {
      /* Clean up the mask if pixmap was NULL */
      if (mask)
        g_object_unref (G_OBJECT (mask));
    }
}

void
gtk_image_set_from_image  (GtkImage  *image,
                           GdkImage  *gdk_image,
                           GdkBitmap *mask)
{
  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (gdk_image == NULL ||
                    GDK_IS_IMAGE (gdk_image));
  g_return_if_fail (mask == NULL ||
                    GDK_IS_PIXMAP (mask));

  
  if (gdk_image)
    g_object_ref (G_OBJECT (gdk_image));

  if (mask)
    g_object_ref (G_OBJECT (mask));

  gtk_image_clear (image);

  if (gdk_image)
    {
      image->storage_type = GTK_IMAGE_IMAGE;

      image->data.image.image = gdk_image;
      image->data.image.mask = mask;

      gtk_image_update_size (image, gdk_image->width, gdk_image->height);
    }
  else
    {
      /* Clean up the mask if gdk_image was NULL */
      if (mask)
        g_object_unref (G_OBJECT (mask));
    }
}

void
gtk_image_set_from_file   (GtkImage    *image,
                           const gchar *filename)
{
  GdkPixbuf *pixbuf;
  
  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (filename != NULL);
  
  gtk_image_clear (image);

  if (filename == NULL)
    return;
  
  pixbuf = gdk_pixbuf_new_from_file (filename);

  if (pixbuf == NULL)
    return;

  gtk_image_set_from_pixbuf (image, pixbuf);

  g_object_unref (G_OBJECT (pixbuf));
}

void
gtk_image_set_from_pixbuf (GtkImage  *image,
                           GdkPixbuf *pixbuf)
{
  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (pixbuf == NULL ||
                    GDK_IS_PIXBUF (pixbuf));
  
  if (pixbuf)
    g_object_ref (G_OBJECT (pixbuf));

  gtk_image_clear (image);

  if (pixbuf != NULL)
    {
      image->storage_type = GTK_IMAGE_PIXBUF;

      image->data.pixbuf.pixbuf = pixbuf;

      gtk_image_update_size (image,
                             gdk_pixbuf_get_width (pixbuf),
                             gdk_pixbuf_get_height (pixbuf));
    }
}

void
gtk_image_set_from_stock  (GtkImage       *image,
                           const gchar    *stock_id,
                           const gchar    *size)
{
  g_return_if_fail (GTK_IS_IMAGE (image));
  
  gtk_image_clear (image);

  if (stock_id)
    {      
      image->storage_type = GTK_IMAGE_STOCK;
      
      image->data.stock.stock_id = g_strdup (stock_id);
      image->data.stock.size = g_strdup (size);

      /* Size is demand-computed in size request method
       * if we're a stock image, since changing the
       * style impacts the size request
       */
    }
}

void
gtk_image_set_from_icon_set  (GtkImage       *image,
                              GtkIconSet     *icon_set,
                              const gchar    *size)
{
  g_return_if_fail (GTK_IS_IMAGE (image));

  if (icon_set)
    gtk_icon_set_ref (icon_set);
  
  gtk_image_clear (image);

  if (icon_set)
    {      
      image->storage_type = GTK_IMAGE_ICON_SET;
      
      image->data.icon_set.icon_set = icon_set;
      image->data.icon_set.size = g_strdup (size);

      /* Size is demand-computed in size request method
       * if we're an icon set
       */
    }
}

GtkImageType
gtk_image_get_storage_type (GtkImage *image)
{
  g_return_val_if_fail (GTK_IS_IMAGE (image), GTK_IMAGE_EMPTY);

  return image->storage_type;
}

void
gtk_image_get_pixmap (GtkImage   *image,
                      GdkPixmap **pixmap,
                      GdkBitmap **mask)
{
  g_return_if_fail (GTK_IS_IMAGE (image)); 
  g_return_if_fail (image->storage_type == GTK_IMAGE_PIXMAP ||
                    image->storage_type == GTK_IMAGE_EMPTY);
  
  if (pixmap)
    *pixmap = image->data.pixmap.pixmap;
  
  if (mask)
    *mask = image->data.pixmap.mask;
}

void
gtk_image_get_image  (GtkImage   *image,
                      GdkImage  **gdk_image,
                      GdkBitmap **mask)
{
  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (image->storage_type == GTK_IMAGE_IMAGE ||
                    image->storage_type == GTK_IMAGE_EMPTY);

  if (gdk_image)
    *gdk_image = image->data.image.image;
  
  if (mask)
    *mask = image->data.image.mask;
}

GdkPixbuf*
gtk_image_get_pixbuf (GtkImage *image)
{
  g_return_val_if_fail (GTK_IS_IMAGE (image), NULL);
  g_return_val_if_fail (image->storage_type == GTK_IMAGE_PIXBUF ||
                        image->storage_type == GTK_IMAGE_EMPTY, NULL);

  if (image->storage_type == GTK_IMAGE_EMPTY)
    image->data.pixbuf.pixbuf = NULL;
  
  return image->data.pixbuf.pixbuf;
}

void
gtk_image_get_stock  (GtkImage        *image,
                      gchar          **stock_id,
                      gchar          **size)
{
  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (image->storage_type == GTK_IMAGE_STOCK ||
                    image->storage_type == GTK_IMAGE_EMPTY);

  if (image->storage_type == GTK_IMAGE_EMPTY)
    image->data.stock.stock_id = NULL;
  
  if (stock_id)
    *stock_id = g_strdup (image->data.stock.stock_id);

  if (size)
    *size = image->data.stock.size;
}

void
gtk_image_get_icon_set  (GtkImage        *image,
                         GtkIconSet     **icon_set,
                         gchar          **size)
{
  g_return_if_fail (GTK_IS_IMAGE (image));
  g_return_if_fail (image->storage_type == GTK_IMAGE_ICON_SET ||
                    image->storage_type == GTK_IMAGE_EMPTY);
      
  if (icon_set)    
    *icon_set = image->data.icon_set.icon_set;

  if (size)
    *size = g_strdup (image->data.icon_set.size);
}

GtkWidget*
gtk_image_new (GdkImage  *val,
	       GdkBitmap *mask)
{
  GtkImage *image;

  g_return_val_if_fail (val != NULL, NULL);

  image = gtk_type_new (GTK_TYPE_IMAGE);

  gtk_image_set (image, val, mask);

  return GTK_WIDGET (image);
}

void
gtk_image_set (GtkImage  *image,
	       GdkImage  *val,
	       GdkBitmap *mask)
{
  g_return_if_fail (GTK_IS_IMAGE (image));

  gtk_image_set_from_image (image, val, mask);
}

void
gtk_image_get (GtkImage   *image,
	       GdkImage  **val,
	       GdkBitmap **mask)
{
  g_return_if_fail (GTK_IS_IMAGE (image));

  gtk_image_get_image (image, val, mask);
}


static gint
gtk_image_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_IMAGE (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget) &&
      GTK_IMAGE (widget)->storage_type != GTK_IMAGE_EMPTY)
    {
      GtkImage *image;
      GtkMisc *misc;
      GdkRectangle area, image_bound, intersection;
      gint x, y;
      GdkBitmap *mask = NULL;
      GdkPixbuf *stock_pixbuf = NULL;
      
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

      image_bound.x = x;
      image_bound.y = y;      

      switch (image->storage_type)
        {
        case GTK_IMAGE_PIXMAP:
          mask = image->data.pixmap.mask;
          gdk_drawable_get_size (image->data.pixmap.pixmap,
                                 &image_bound.width,
                                 &image_bound.height);
          break;

        case GTK_IMAGE_IMAGE:
          mask = image->data.image.mask;
          image_bound.width = image->data.image.image->width;
          image_bound.height = image->data.image.image->height;
          break;

        case GTK_IMAGE_PIXBUF:
          image_bound.width = gdk_pixbuf_get_width (image->data.pixbuf.pixbuf);
          image_bound.height = gdk_pixbuf_get_height (image->data.pixbuf.pixbuf);
          break;

        case GTK_IMAGE_STOCK:
          stock_pixbuf = gtk_widget_render_stock_icon (widget,
                                                       image->data.stock.stock_id,
                                                       image->data.stock.size,
                                                       NULL);

          if (stock_pixbuf)
            {
              image_bound.width = gdk_pixbuf_get_width (stock_pixbuf);
              image_bound.height = gdk_pixbuf_get_height (stock_pixbuf);
            }
          break;

        case GTK_IMAGE_ICON_SET:
          stock_pixbuf =
            gtk_icon_set_render_icon (image->data.icon_set.icon_set,
                                      widget->style,
                                      gtk_widget_get_direction (widget),
                                      GTK_WIDGET_STATE (widget),
                                      image->data.icon_set.size,
                                      widget,
                                      NULL);

          if (stock_pixbuf)
            {
              image_bound.width = gdk_pixbuf_get_width (stock_pixbuf);
              image_bound.height = gdk_pixbuf_get_height (stock_pixbuf);
            }
          break;
          
        default:
          break;
        }

      if (mask)
	{
	  gdk_gc_set_clip_mask (widget->style->black_gc, mask);
	  gdk_gc_set_clip_origin (widget->style->black_gc, x, y);
	}

      area = event->area;
      
      if (gdk_rectangle_intersect (&image_bound, &area, &intersection))
        {

          switch (image->storage_type)
            {
            case GTK_IMAGE_PIXMAP:
              gdk_draw_drawable (widget->window,
                                 widget->style->black_gc,
                                 image->data.pixmap.pixmap,
                                 image_bound.x - x, image_bound.y - y,
                                 image_bound.x, image_bound.y,
                                 image_bound.width, image_bound.height);
              break;
              
            case GTK_IMAGE_IMAGE:
              gdk_draw_image (widget->window,
                              widget->style->black_gc,
                              image->data.image.image,
                              image_bound.x - x, image_bound.y - y,
                              image_bound.x, image_bound.y,
                              image_bound.width, image_bound.height);
              break;

            case GTK_IMAGE_PIXBUF:
              gdk_pixbuf_render_to_drawable_alpha (image->data.pixbuf.pixbuf,
                                                   widget->window,
                                                   image_bound.x - x, image_bound.y - y,
                                                   image_bound.x, image_bound.y,
                                                   image_bound.width, image_bound.height,
                                                   GDK_PIXBUF_ALPHA_FULL,
                                                   128,
                                                   GDK_RGB_DITHER_NORMAL,
                                                   0, 0);
              break;

            case GTK_IMAGE_STOCK: /* fall thru */
            case GTK_IMAGE_ICON_SET:
              if (stock_pixbuf)
                {
                  gdk_pixbuf_render_to_drawable_alpha (stock_pixbuf,
                                                       widget->window,
                                                       image_bound.x - x, image_bound.y - y,
                                                       image_bound.x, image_bound.y,
                                                       image_bound.width, image_bound.height,
                                                       GDK_PIXBUF_ALPHA_FULL,
                                                       128,
                                                       GDK_RGB_DITHER_NORMAL,
                                                       0, 0);
                  
                  g_object_unref (G_OBJECT (stock_pixbuf));
                }
              break;

            default:
              break;
            }
        } /* if rectangle intersects */      
      if (mask)
        {
          gdk_gc_set_clip_mask (widget->style->black_gc, NULL);
          gdk_gc_set_clip_origin (widget->style->black_gc, 0, 0);
        }
    } /* if widget is drawable */

  return FALSE;
}

static void
gtk_image_clear (GtkImage *image)
{
  switch (image->storage_type)
    {
    case GTK_IMAGE_PIXMAP:

      if (image->data.pixmap.pixmap)
        g_object_unref (G_OBJECT (image->data.pixmap.pixmap));

      if (image->data.pixmap.mask)
        g_object_unref (G_OBJECT (image->data.pixmap.mask));

      image->data.pixmap.pixmap = NULL;
      image->data.pixmap.mask = NULL;

      break;

    case GTK_IMAGE_IMAGE:

      if (image->data.image.image)
        g_object_unref (G_OBJECT (image->data.image.image));

      if (image->data.image.mask)
        g_object_unref (G_OBJECT (image->data.image.mask));

      image->data.image.image = NULL;
      image->data.image.mask = NULL;

      break;

    case GTK_IMAGE_PIXBUF:

      if (image->data.pixbuf.pixbuf)
        g_object_unref (G_OBJECT (image->data.pixbuf.pixbuf));

      image->data.pixbuf.pixbuf = NULL;

      break;

    case GTK_IMAGE_STOCK:

      g_free (image->data.stock.size);
      g_free (image->data.stock.stock_id);

      image->data.stock.stock_id = NULL;
      image->data.stock.size = NULL;
      
      break;

    case GTK_IMAGE_ICON_SET:
      if (image->data.icon_set.icon_set)
        gtk_icon_set_unref (image->data.icon_set.icon_set);

      g_free (image->data.icon_set.size);

      image->data.icon_set.size = NULL;
      image->data.icon_set.icon_set = NULL;
      
      break;
      
    case GTK_IMAGE_EMPTY:
    default:
      break;
      
    }

  image->storage_type = GTK_IMAGE_EMPTY;

  GTK_WIDGET (image)->requisition.width = 0;
  GTK_WIDGET (image)->requisition.height = 0;
  
  if (GTK_WIDGET_VISIBLE (image))
    gtk_widget_queue_resize (GTK_WIDGET (image));
}

static void
gtk_image_size_request (GtkWidget      *widget,
                        GtkRequisition *requisition)
{
  GtkImage *image;
  GdkPixbuf *pixbuf = NULL;
  
  image = GTK_IMAGE (widget);

  switch (image->storage_type)
    {
    case GTK_IMAGE_STOCK:
      pixbuf = gtk_widget_render_stock_icon (GTK_WIDGET (image),
                                             image->data.stock.stock_id,
                                             image->data.stock.size,
                                             NULL);
      break;

    case GTK_IMAGE_ICON_SET:
      pixbuf = gtk_icon_set_render_icon (image->data.icon_set.icon_set,
                                         widget->style,
                                         gtk_widget_get_direction (widget),
                                         GTK_WIDGET_STATE (widget),
                                         image->data.icon_set.size,
                                         widget,
                                         NULL);
      break;
      
    default:
      break;
    }

  if (pixbuf)
    {
      gtk_image_update_size (image,
                             gdk_pixbuf_get_width (pixbuf),
                             gdk_pixbuf_get_height (pixbuf));
      g_object_unref (G_OBJECT (pixbuf));
    }

  /* Chain up to default that simply reads current requisition */
  GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);
}

static void
gtk_image_update_size (GtkImage *image,
                       gint      image_width,
                       gint      image_height)
{
  GTK_WIDGET (image)->requisition.width = image_width + GTK_MISC (image)->xpad * 2;
  GTK_WIDGET (image)->requisition.height = image_height + GTK_MISC (image)->ypad * 2;
}


