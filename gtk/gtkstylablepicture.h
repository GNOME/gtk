/* gtktreesortable.h
 * Copyright (C) 2001  Red Hat, Inc.
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

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_STYLABLE_PICTURE_H__
#define __GTK_STYLABLE_PICTURE_H__


#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>


G_BEGIN_DECLS

#define GTK_TYPE_STYLABLE_PICTURE            (gtk_stylable_picture_get_type ())
#define GTK_STYLABLE_PICTURE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_STYLABLE_PICTURE, GtkStylablePicture))
#define GTK_STYLABLE_PICTURE_CLASS(obj)      (G_TYPE_CHECK_CLASS_CAST ((obj), GTK_TYPE_STYLABLE_PICTURE, GtkStylablePictureInterface))
#define GTK_IS_STYLABLE_PICTURE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_STYLABLE_PICTURE))
#define GTK_STYLABLE_PICTURE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GTK_TYPE_STYLABLE_PICTURE, GtkStylablePictureInterface))

typedef struct _GtkStylablePicture          GtkStylablePicture; /* Dummy typedef */
typedef struct _GtkStylablePictureInterface GtkStylablePictureInterface;

struct _GtkStylablePictureInterface
{
  GTypeInterface g_iface;

  /* virtual table */
  GdkPicture * (* attach)            (GdkPicture      *picture,
                                      GtkWidget       *widget);
  GdkPicture * (* get_unstyled)      (GdkPicture      *picture);
};


GType           gtk_stylable_picture_get_type        (void) G_GNUC_CONST;

GdkPicture *    gtk_widget_style_picture             (GtkWidget  *widget,    
                                                      GdkPicture *picture);
GdkPicture *    gtk_picture_get_unstyled             (GdkPicture *styled);


G_END_DECLS

#endif /* __GTK_STYLABLE_PICTURE_H__ */
