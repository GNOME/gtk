/* GTK - The GIMP Drawing Kit
 * Copyright (C) 2010 Benjamin Otte <otte@gnome.org>
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

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_STYLED_PICTURE_H__
#define __GTK_STYLED_PICTURE_H__

#include <gtk/gtk.h>


G_BEGIN_DECLS

#define GTK_TYPE_STYLED_PICTURE              (gtk_styled_picture_get_type ())
#define GTK_STYLED_PICTURE(o)                (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_STYLED_PICTURE, GtkStyledPicture))
#define GTK_STYLED_PICTURE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_STYLED_PICTURE, GtkStyledPictureClass))
#define GTK_IS_STYLED_PICTURE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GTK_TYPE_STYLED_PICTURE))
#define GTK_IS_STYLED_PICTURE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_STYLED_PICTURE))
#define GTK_STYLED_PICTURE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_STYLED_PICTURE, GtkStyledPictureClass))

typedef struct _GtkStyledPicture        GtkStyledPicture;
typedef struct _GtkStyledPicturePrivate GtkStyledPicturePrivate;
typedef struct _GtkStyledPictureClass   GtkStyledPictureClass;

struct _GtkStyledPicture {
  GdkPicture               parent_picture;

  GtkStyledPicturePrivate *priv;
};

struct _GtkStyledPictureClass {
  GdkPictureClass       parent_class;

  GdkPicture *          ( *update)                (GtkStyledPicture *picture);
};

GType           gtk_styled_picture_get_type       (void) G_GNUC_CONST;

GdkPicture *    gtk_styled_picture_new            (GdkPicture *unstyled,
                                                   GtkWidget  *widget);

void            gtk_styled_picture_update         (GtkStyledPicture *picture);
GtkWidget *     gtk_styled_picture_get_widget     (GtkStyledPicture *picture);

G_END_DECLS

#endif /* __GTK_STYLED_PICTURE_H__ */
