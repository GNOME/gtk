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

#ifndef __GTK_STOCK_PICTURE_H__
#define __GTK_STOCK_PICTURE_H__

#include <gtk/gtk.h>


G_BEGIN_DECLS

#define GTK_TYPE_STOCK_PICTURE              (gtk_stock_picture_get_type ())
#define GTK_STOCK_PICTURE(o)                (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_STOCK_PICTURE, GtkStockPicture))
#define GTK_STOCK_PICTURE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_STOCK_PICTURE, GtkStockPictureClass))
#define GTK_IS_STOCK_PICTURE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GTK_TYPE_STOCK_PICTURE))
#define GTK_IS_STOCK_PICTURE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_STOCK_PICTURE))
#define GTK_STOCK_PICTURE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_STOCK_PICTURE, GtkStockPictureClass))

typedef struct _GtkStockPicture        GtkStockPicture;
typedef struct _GtkStockPicturePrivate GtkStockPicturePrivate;
typedef struct _GtkStockPictureClass   GtkStockPictureClass;

struct _GtkStockPicture {
  GdkPicture              parent_picture;

  GtkStockPicturePrivate *priv;
};

struct _GtkStockPictureClass {
  GdkPictureClass         parent_class;
};

GType           gtk_stock_picture_get_type      (void) G_GNUC_CONST;

GdkPicture *    gtk_stock_picture_new           (const char *     stock_id,
                                                 GtkIconSize      size);

GtkIconSize     gtk_stock_picture_get_size      (GtkStockPicture * picture);
void            gtk_stock_picture_set_size      (GtkStockPicture * picture,
                                                 GtkIconSize       size);
const char *    gtk_stock_picture_get_stock_id  (GtkStockPicture * picture);
void            gtk_stock_picture_set_stock_id  (GtkStockPicture * picture,
                                                 const char *      stock_id);

G_END_DECLS

#endif /* __GTK_STOCK_PICTURE_H__ */
