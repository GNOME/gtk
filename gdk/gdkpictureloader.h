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

#ifndef __GDK_PICTURE_LOADER_H__
#define __GDK_PICTURE_LOADER_H__

#include <gdk/gdkpicture.h>

#include <gio/gio.h>

G_BEGIN_DECLS

#define GDK_TYPE_PICTURE_LOADER              (gdk_picture_loader_get_type ())
#define GDK_PICTURE_LOADER(o)                (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_PICTURE_LOADER, GdkPictureLoader))
#define GDK_PICTURE_LOADER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_PICTURE_LOADER, GdkPictureLoaderClass))
#define GDK_IS_PICTURE_LOADER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_PICTURE_LOADER))
#define GDK_IS_PICTURE_LOADER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_PICTURE_LOADER))
#define GDK_PICTURE_LOADER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_PICTURE_LOADER, GdkPictureLoaderClass))

typedef struct _GdkPictureLoader        GdkPictureLoader;
typedef struct _GdkPictureLoaderPrivate GdkPictureLoaderPrivate;
typedef struct _GdkPictureLoaderClass   GdkPictureLoaderClass;

struct _GdkPictureLoader {
  GdkPicture               parent_picture;

  GdkPictureLoaderPrivate *priv;
};

struct _GdkPictureLoaderClass {
  GdkPictureClass          parent_class;
};

GType           gdk_picture_loader_get_type             (void);

GdkPicture *    gdk_picture_loader_new                  (void);

const GError *  gdk_picture_loader_get_error            (GdkPictureLoader *  loader);
GdkPicture *    gdk_picture_loader_get_picture          (GdkPictureLoader *  loader);

void            gdk_picture_loader_load_from_stream     (GdkPictureLoader *  loader,
                                                         GInputStream *      stream,
                                                         GCancellable *      cancellable,
                                                         GError **           error);
void            gdk_picture_loader_load_from_file       (GdkPictureLoader *  loader,
                                                         GFile *             file,
                                                         GCancellable *      cancellable,
                                                         GError **           error);
void            gdk_picture_loader_load_from_filename   (GdkPictureLoader *  loader,
                                                         const char *        filename,
                                                         GCancellable *      cancellable,
                                                         GError **           error);

void            gdk_picture_loader_load_from_stream_async(GdkPictureLoader * loader,
                                                         GInputStream *      stream,
                                                         int                 io_priority,
                                                         GCancellable *      cancellable,
                                                         GAsyncReadyCallback callback,
                                                         gpointer            user_data);
void            gdk_picture_loader_load_from_file_async (GdkPictureLoader *  loader,
                                                         GFile *             file,
                                                         int                 io_priority,
                                                         GCancellable *      cancellable,
                                                         GAsyncReadyCallback callback,
                                                         gpointer            user_data);
void            gdk_picture_loader_load_from_filename_async (GdkPictureLoader *loader,
                                                         const char *        filename,
                                                         int                 io_priority,
                                                         GCancellable *      cancellable,
                                                         GAsyncReadyCallback callback,
                                                         gpointer            user_data);


G_END_DECLS

#endif /* __GDK_PICTURE_LOADER_H__ */
