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

#ifndef __GDK_PICTURE_H__
#define __GDK_PICTURE_H__

#include <glib-object.h>
#include <cairo.h>


G_BEGIN_DECLS

#define GDK_TYPE_PICTURE              (gdk_picture_get_type ())
#define GDK_PICTURE(o)                (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_PICTURE, GdkPicture))
#define GDK_PICTURE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_PICTURE, GdkPictureClass))
#define GDK_IS_PICTURE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_PICTURE))
#define GDK_IS_PICTURE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_PICTURE))
#define GDK_PICTURE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_PICTURE, GdkPictureClass))

typedef struct _GdkPicture        GdkPicture;
typedef struct _GdkPicturePrivate GdkPicturePrivate;
typedef struct _GdkPictureClass   GdkPictureClass;

struct _GdkPicture {
  GObject            parent_object;

  GdkPicturePrivate *priv;
};

struct _GdkPictureClass {
  GObjectClass       parent_class;

  /* need to implement one of these */
  cairo_surface_t *     (* ref_surface)                 (GdkPicture           *picture);
  void                  (* draw)                        (GdkPicture           *picture,
                                                         cairo_t              *cr);
};

GType                   gdk_picture_get_type            (void);

int                     gdk_picture_get_width           (GdkPicture           *picture);
int                     gdk_picture_get_height          (GdkPicture           *picture);

cairo_surface_t *       gdk_picture_ref_surface         (GdkPicture           *picture);
void                    gdk_picture_draw                (GdkPicture           *picture,
                                                         cairo_t              *cr);

/* for implementing subclasses only */
void                    gdk_picture_resized             (GdkPicture           *picture,
                                                         int                   new_width,
                                                         int                   new_height);
void                    gdk_picture_changed             (GdkPicture           *picture);
void                    gdk_picture_changed_rect        (GdkPicture           *picture,
                                                         const cairo_rectangle_int_t *rect);
void                    gdk_picture_changed_region      (GdkPicture           *picture,
                                                         const cairo_region_t *region);

G_END_DECLS

#endif /* __GDK_PICTURE_H__ */
