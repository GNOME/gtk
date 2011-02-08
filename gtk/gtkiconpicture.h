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

#ifndef __GTK_ICON_PICTURE_H__
#define __GTK_ICON_PICTURE_H__

#include <gtk/gtk.h>


G_BEGIN_DECLS

#define GTK_TYPE_ICON_PICTURE              (gtk_icon_picture_get_type ())
#define GTK_ICON_PICTURE(o)                (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_ICON_PICTURE, GtkIconPicture))
#define GTK_ICON_PICTURE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_ICON_PICTURE, GtkIconPictureClass))
#define GTK_IS_ICON_PICTURE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GTK_TYPE_ICON_PICTURE))
#define GTK_IS_ICON_PICTURE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ICON_PICTURE))
#define GTK_ICON_PICTURE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ICON_PICTURE, GtkIconPictureClass))

typedef struct _GtkIconPicture        GtkIconPicture;
typedef struct _GtkIconPicturePrivate GtkIconPicturePrivate;
typedef struct _GtkIconPictureClass   GtkIconPictureClass;

struct _GtkIconPicture {
  GdkPicture             parent_picture;

  GtkIconPicturePrivate *priv;
};

struct _GtkIconPictureClass {
  GdkPictureClass       parent_class;
};

GType           gtk_icon_picture_get_type       (void) G_GNUC_CONST;

GdkPicture *    gtk_icon_picture_new            (const char *     icon_name,
                                                 GtkIconSize      size);

GtkIconSize     gtk_icon_picture_get_size       (GtkIconPicture * picture);
void            gtk_icon_picture_set_size       (GtkIconPicture * picture,
                                                 GtkIconSize      size);
const char *    gtk_icon_picture_get_name       (GtkIconPicture * picture);
void            gtk_icon_picture_set_name       (GtkIconPicture * picture,
                                                 const char *     name);

G_END_DECLS

#endif /* __GTK_ICON_PICTURE_H__ */
