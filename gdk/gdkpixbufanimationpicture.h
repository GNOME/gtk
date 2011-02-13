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

#ifndef __GDK_PIXBUF_ANIMATION_PICTURE_H__
#define __GDK_PIXBUF_ANIMATION_PICTURE_H__

#include <gdk/gdkpicture.h>

#include <gdk-pixbuf/gdk-pixbuf.h>


G_BEGIN_DECLS

#define GDK_TYPE_PIXBUF_ANIMATION_PICTURE              (gdk_pixbuf_animation_picture_get_type ())
#define GDK_PIXBUF_ANIMATION_PICTURE(o)                (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_PIXBUF_ANIMATION_PICTURE, GdkPixbufAnimationPicture))
#define GDK_PIXBUF_ANIMATION_PICTURE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_PIXBUF_ANIMATION_PICTURE, GdkPixbufAnimationPictureClass))
#define GDK_IS_PIXBUF_ANIMATION_PICTURE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_PIXBUF_ANIMATION_PICTURE))
#define GDK_IS_PIXBUF_ANIMATION_PICTURE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_PIXBUF_ANIMATION_PICTURE))
#define GDK_PIXBUF_ANIMATION_PICTURE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_PIXBUF_ANIMATION_PICTURE, GdkPixbufAnimationPictureClass))

typedef struct _GdkPixbufAnimationPicture        GdkPixbufAnimationPicture;
typedef struct _GdkPixbufAnimationPicturePrivate GdkPixbufAnimationPicturePrivate;
typedef struct _GdkPixbufAnimationPictureClass   GdkPixbufAnimationPictureClass;

struct _GdkPixbufAnimationPicture {
  GdkPicture               parent_picture;

  GdkPixbufAnimationPicturePrivate *priv;
};

struct _GdkPixbufAnimationPictureClass {
  GdkPictureClass       parent_class;
};

GType           gdk_pixbuf_animation_picture_get_type       (void);

GdkPicture *    gdk_pixbuf_animation_picture_new            (GdkPixbufAnimation        *animation);

void            gdk_pixbuf_animation_picture_set_animation  (GdkPixbufAnimationPicture *picture,
                                                             GdkPixbufAnimation *       animation);
GdkPixbufAnimation *
                gdk_pixbuf_animation_picture_get_animation  (GdkPixbufAnimationPicture *picture);

G_END_DECLS

#endif /* __GDK_PIXBUF_ANIMATION_PICTURE_H__ */
