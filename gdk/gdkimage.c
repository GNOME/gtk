/* GDK - The GIMP Drawing Kit
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

#include <stdlib.h>
#include <sys/types.h>

#include "gdkimage.h"
#include "gdkprivate.h"

/**
 * gdk_image_ref:
 * @image: a #GdkImage
 * 
 * Deprecated function; use g_object_ref() instead.
 * 
 * Return value: the image
 **/
GdkImage *
gdk_image_ref (GdkImage *image)
{
  return (GdkImage *) g_object_ref (G_OBJECT (image));
}

/**
 * gdk_image_unref:
 * @image: a #GdkImage
 * 
 * Deprecated function; use g_object_unref() instead.
 * 
 **/
void
gdk_image_unref (GdkImage *image)
{
  g_return_if_fail (GDK_IS_IMAGE (image));

  g_object_unref (G_OBJECT (image));
}

/**
 * gdk_image_get:
 * @drawable: a #GdkDrawable
 * @x: x coordinate in @window
 * @y: y coordinate in @window
 * @width: width of area in @window
 * @height: height of area in @window
 * 
 * This is a deprecated wrapper for gdk_drawable_get_image();
 * gdk_drawable_get_image() should be used instead. Or even better: in
 * most cases gdk_pixbuf_get_from_drawable() is the most convenient
 * choice.
 * 
 * Return value: a new #GdkImage or %NULL
 **/
GdkImage*
gdk_image_get (GdkWindow *drawable,
	       gint       x,
	       gint       y,
	       gint       width,
	       gint       height)
{
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (x >= 0, NULL);
  g_return_val_if_fail (y >= 0, NULL);
  g_return_val_if_fail (width >= 0, NULL);
  g_return_val_if_fail (height >= 0, NULL);
  
  return gdk_drawable_get_image (drawable, y, width, height);
}

/**
 * gdk_image_set_colormap:
 * @image: a #GdkImage
 * @colormap: a #GdkColormap
 * 
 * Sets the colormap for the image to the given colormap.  Normally
 * there's no need to use this function, images are created with the
 * correct colormap if you get the image from a drawable. If you
 * create the image from scratch, use the colormap of the drawable you
 * intend to render the image to.
 **/
void
gdk_image_set_colormap (GdkImage       *image,
                        GdkColormap    *colormap)
{
  g_return_if_fail (GDK_IS_IMAGE (image));
  g_return_if_fail (GDK_IS_COLORMAP (colormap));

  if (image->colormap != colormap)
    {
      if (image->colormap)
	g_object_unref (G_OBJECT (image->colormap));

      image->colormap = colormap;
      g_object_ref (G_OBJECT (image->colormap));
    }
    
}

/**
 * gdk_image_get_colormap:
 * @image: a #GdkImage
 * 
 * Retrieves the colormap for a given image, if it exists.  An image
 * will have a colormap if the drawable from which it was created has
 * a colormap, or if a colormap was set explicitely with
 * gdk_image_set_colormap().
 * 
 * Return value: colormap for the image
 **/
GdkColormap *
gdk_image_get_colormap (GdkImage *image)
{
  g_return_val_if_fail (GDK_IS_IMAGE (image), NULL);

  return image->colormap;
}
