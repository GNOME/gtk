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

#ifndef __GTK_NAMED_PICTURE_H__
#define __GTK_NAMED_PICTURE_H__

#include <gtk/gtkiconthemepicture.h>


G_BEGIN_DECLS

#define GTK_TYPE_NAMED_PICTURE              (gtk_named_picture_get_type ())
#define GTK_NAMED_PICTURE(o)                (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_NAMED_PICTURE, GtkNamedPicture))
#define GTK_NAMED_PICTURE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_NAMED_PICTURE, GtkNamedPictureClass))
#define GTK_IS_NAMED_PICTURE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GTK_TYPE_NAMED_PICTURE))
#define GTK_IS_NAMED_PICTURE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_NAMED_PICTURE))
#define GTK_NAMED_PICTURE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_NAMED_PICTURE, GtkNamedPictureClass))

typedef struct _GtkNamedPicture        GtkNamedPicture;
typedef struct _GtkNamedPicturePrivate GtkNamedPicturePrivate;
typedef struct _GtkNamedPictureClass   GtkNamedPictureClass;

struct _GtkNamedPicture {
  GtkIconThemePicture      parent_picture;

  GtkNamedPicturePrivate *  priv;
};

struct _GtkNamedPictureClass {
  GtkIconThemePictureClass parent_class;
};

GType           gtk_named_picture_get_type      (void) G_GNUC_CONST;

GdkPicture *    gtk_named_picture_new           (const char *      name,
                                                 GtkIconSize       size);

const char *    gtk_named_picture_get_name      (GtkNamedPicture * picture);
void            gtk_named_picture_set_name      (GtkNamedPicture * picture,
                                                 const char *      name);

G_END_DECLS

#endif /* __GTK_NAMED_PICTURE_H__ */
