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
#ifndef __GTK_PIXMAP_H__
#define __GTK_PIXMAP_H__


#include <gdk/gdk.h>
#include <gtk/gtkmisc.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_PIXMAP(obj)          GTK_CHECK_CAST (obj, gtk_pixmap_get_type (), GtkPixmap)
#define GTK_PIXMAP_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_pixmap_get_type (), GtkPixmapClass)
#define GTK_IS_PIXMAP(obj)       GTK_CHECK_TYPE (obj, gtk_pixmap_get_type ())


typedef struct _GtkPixmap       GtkPixmap;
typedef struct _GtkPixmapClass  GtkPixmapClass;

struct _GtkPixmap
{
  GtkMisc misc;

  GdkPixmap *pixmap;
  GdkBitmap *mask;
};

struct _GtkPixmapClass
{
  GtkMiscClass parent_class;
};


guint      gtk_pixmap_get_type   (void);
GtkWidget* gtk_pixmap_new        (GdkPixmap  *pixmap,
				  GdkBitmap  *mask);
void       gtk_pixmap_set        (GtkPixmap  *pixmap,
				  GdkPixmap  *val,
				  GdkBitmap  *mask);
void       gtk_pixmap_get        (GtkPixmap  *pixmap,
				  GdkPixmap **val,
				  GdkBitmap **mask);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_PIXMAP_H__ */
