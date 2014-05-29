/*
 * Copyright (C) 2014 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_CLIPBOARD_PRIVATE_H__
#define __GDK_CLIPBOARD_PRIVATE_H__

#include <gdk/gdkclipboard.h>

G_BEGIN_DECLS

#define GDK_CLIPBOARD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_CLIPBOARD, GdkClipboardClass))
#define GDK_IS_CLIPBOARD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_CLIPBOARD))
#define GDK_CLIPBOARD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_CLIPBOARD, GdkClipboardClass))

typedef struct _GdkClipboardClass GdkClipboardClass;

struct _GdkClipboard
{
  GObject parent;
};

struct _GdkClipboardClass
{
  GObjectClass parent_class;

  /* signals */
  void          (* changed)          (GdkClipboard           *clipboard);

  /* vfuncs */
  void           (* get_text_async)   (GdkClipboard          *clipboard, 
                                       GCancellable          *cancellable,
                                       GAsyncReadyCallback    callback,
                                       gpointer               user_data);
  gchar *        (* get_text_finish)  (GdkClipboard          *clipboard,
                                       GAsyncResult          *res,
                                       GError               **error);
  void           (* set_text)         (GdkClipboard          *clipboard,
                                       const gchar           *text);

  void           (* get_image_async)  (GdkClipboard          *clipboard, 
                                       GCancellable          *cancellable,
                                       GAsyncReadyCallback    callback,
                                       gpointer               user_data);
  GdkPixbuf *    (* get_image_finish) (GdkClipboard          *clipboard,
                                       GAsyncResult          *res,
                                       GError               **error);
  void           (* set_image)        (GdkClipboard          *clipboard,
                                       GdkPixbuf             *pixbuf);

  void           (* get_data_async)   (GdkClipboard          *clipboard, 
                                       const gchar           *content_type,
                                       GCancellable          *cancellable,
                                       GAsyncReadyCallback    callback,
                                       gpointer               user_data);
  GInputStream * (* get_data_finish)  (GdkClipboard          *clipboard,
                                       GAsyncResult          *res,
                                       GError               **error);
  void           (* set_data)         (GdkClipboard          *clipboard,
                                       const gchar          **content_types,
                                       GdkClipboardProvider   provider,
                                       gpointer               data,
                                       GDestroyNotify         destroy);

  void           (* clear)            (GdkClipboard          *clipboard);
};


typedef enum {
  NO_CONTENT    = 0,
  OTHER_CONTENT = 1 << 0,
  TEXT_CONTENT  = 1 << 1,
  IMAGE_CONTENT = 1 << 2
} GdkClipboardContent;

void
_gdk_clipboard_set_available_content (GdkClipboard         *clipboard,
                                      GdkClipboardContent   content,
                                      const gchar         **content_types);

GdkClipboardContent
_gdk_clipboard_get_available_content (GdkClipboard        *clipboard);

G_END_DECLS

#endif /* __GDK_CLIPBOARD_PRIVATE_H__ */
