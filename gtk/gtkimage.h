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

#ifndef __GTK_IMAGE_H__
#define __GTK_IMAGE_H__


#include <gdk/gdk.h>
#include <gtk/gtkmisc.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_IMAGE                  (gtk_image_get_type ())
#define GTK_IMAGE(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_IMAGE, GtkImage))
#define GTK_IMAGE_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_IMAGE, GtkImageClass))
#define GTK_IS_IMAGE(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_IMAGE))
#define GTK_IS_IMAGE_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_IMAGE))


typedef struct _GtkImage       GtkImage;
typedef struct _GtkImageClass  GtkImageClass;

struct _GtkImage
{
  GtkMisc misc;

  GdkImage *image;
  GdkBitmap *mask;
};

struct _GtkImageClass
{
  GtkMiscClass parent_class;
};


GtkType    gtk_image_get_type (void);
GtkWidget* gtk_image_new      (GdkImage   *val,
			       GdkBitmap  *mask);
void       gtk_image_set      (GtkImage   *image,
			       GdkImage   *val,
			       GdkBitmap  *mask);
void       gtk_image_get      (GtkImage   *image,
			       GdkImage  **val,
			       GdkBitmap **mask);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_IMAGE_H__ */
