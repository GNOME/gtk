/* GDK - The GIMP Drawing Kit
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

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#ifndef __GDK_PIXBUF_PICTURE_H__
#define __GDK_PIXBUF_PICTURE_H__

#include <gdk/gdkpicture.h>

#include <gdk-pixbuf/gdk-pixbuf.h>


G_BEGIN_DECLS

#define GDK_TYPE_PIXBUF_PICTURE              (gdk_pixbuf_picture_get_type ())
#define GDK_PIXBUF_PICTURE(o)                (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_PIXBUF_PICTURE, GdkPixbufPicture))
#define GDK_PIXBUF_PICTURE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_PIXBUF_PICTURE, GdkPixbufPictureClass))
#define GDK_IS_PIXBUF_PICTURE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_PIXBUF_PICTURE))
#define GDK_IS_PIXBUF_PICTURE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_PIXBUF_PICTURE))
#define GDK_PIXBUF_PICTURE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_PIXBUF_PICTURE, GdkPixbufPictureClass))

typedef struct _GdkPixbufPicture        GdkPixbufPicture;
typedef struct _GdkPixbufPicturePrivate GdkPixbufPicturePrivate;
typedef struct _GdkPixbufPictureClass   GdkPixbufPictureClass;

struct _GdkPixbufPicture {
  GdkPicture               parent_picture;

  GdkPixbufPicturePrivate *priv;
};

struct _GdkPixbufPictureClass {
  GdkPictureClass       parent_class;
};

GType           gdk_pixbuf_picture_get_type            (void);

GdkPicture *    gdk_pixbuf_picture_new                 (GdkPixbuf        *pixbuf);

void            gdk_pixbuf_picture_set_pixbuf          (GdkPixbufPicture *picture,
                                                        GdkPixbuf *       pixbuf);
GdkPixbuf *     gdk_pixbuf_picture_get_pixbuf          (GdkPixbufPicture *picture);
void            gdk_pixbuf_picture_set_keep_pixbuf     (GdkPixbufPicture *picture,
                                                        gboolean          keep_pixbuf);
gboolean        gdk_pixbuf_picture_get_keep_pixbuf     (GdkPixbufPicture *picture);
void            gdk_pixbuf_picture_set_keep_surface    (GdkPixbufPicture *picture,
                                                        gboolean          keep_surface);
gboolean        gdk_pixbuf_picture_get_keep_surface    (GdkPixbufPicture *picture);

G_END_DECLS

#endif /* __GDK_PIXBUF_PICTURE_H__ */
