/* GDK - The GIMP Drawing Kit
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

#ifndef __GDK_CLIPBOARD_H__
#define __GDK_CLIPBOARD_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>
#include <gdk-pixbuf/gdk-pixbuf.h>


G_BEGIN_DECLS

#define GDK_TYPE_CLIPBOARD            (gdk_clipboard_get_type ())
#define GDK_CLIPBOARD(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_CLIPBOARD, GdkClipboard))
#define GDK_IS_CLIPBOARD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_CLIPBOARD))

typedef struct _GdkClipboard GdkClipboard;

typedef void (* GdkClipboardProvider) (GdkClipboard  *clipboard,
                                       const gchar   *content_type,
                                       GOutputStream *output,
                                       gpointer       user_data);

GDK_AVAILABLE_IN_3_14
GType          gdk_clipboard_get_type          (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_14
void           gdk_clipboard_get_text_async    (GdkClipboard         *clipboard,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
GDK_AVAILABLE_IN_3_14
gchar *        gdk_clipboard_get_text_finish   (GdkClipboard         *clipboard,
                                                GAsyncResult         *res,
                                                GError              **error);
GDK_AVAILABLE_IN_3_14
void           gdk_clipboard_set_text          (GdkClipboard         *clipboard,
                                                const gchar          *text);

GDK_AVAILABLE_IN_3_14
gboolean       gdk_clipboard_text_available   (GdkClipboard          *clipboard);

GDK_AVAILABLE_IN_3_14
void           gdk_clipboard_get_image_async  (GdkClipboard          *clipboard,
                                               GCancellable          *cancellable,
                                               GAsyncReadyCallback    callback,
                                               gpointer               user_data);
GDK_AVAILABLE_IN_3_14
GdkPixbuf *    gdk_clipboard_get_image_finish (GdkClipboard          *clipboard,
                                               GAsyncResult          *res,
                                               GError               **error);
GDK_AVAILABLE_IN_3_14
void           gdk_clipboard_set_image        (GdkClipboard          *clipboard,
                                               GdkPixbuf             *pixbuf);

GDK_AVAILABLE_IN_3_14
gboolean       gdk_clipboard_image_available  (GdkClipboard          *clipboard);

GDK_AVAILABLE_IN_3_14
void           gdk_clipboard_get_bytes_async  (GdkClipboard          *clipboard,
                                               const gchar           *content_type,
                                               GCancellable          *cancellable,
                                               GAsyncReadyCallback    callback,
                                               gpointer               user_data);
GDK_AVAILABLE_IN_3_14
GBytes *       gdk_clipboard_get_bytes_finish (GdkClipboard          *clipboard,
                                               GAsyncResult          *res,
                                               GError               **error);
GDK_AVAILABLE_IN_3_14
void           gdk_clipboard_set_bytes        (GdkClipboard          *clipboard,
                                               GBytes                *data,
                                               const gchar           *content_type);

GDK_AVAILABLE_IN_3_14
void           gdk_clipboard_get_data_async   (GdkClipboard          *clipboard,
                                               const gchar           *content_type,
                                               GCancellable          *cancellable,
                                               GAsyncReadyCallback    callback,
                                               gpointer               user_data);
GDK_AVAILABLE_IN_3_14
GInputStream * gdk_clipboard_get_data_finish  (GdkClipboard          *clipboard,
                                               GAsyncResult          *res,
                                               GError               **error);

GDK_AVAILABLE_IN_3_14
void           gdk_clipboard_set_data         (GdkClipboard          *clipboard,
                                               const gchar          **content_types,
                                               GdkClipboardProvider   callback,
                                               gpointer               user_data,
                                               GDestroyNotify         destroy);
                                          
GDK_AVAILABLE_IN_3_14
gboolean       gdk_clipboard_data_available   (GdkClipboard          *clipboard,
                                               const gchar           *content_type);

GDK_AVAILABLE_IN_3_14
void           gdk_clipboard_clear            (GdkClipboard          *clipboard);

GDK_AVAILABLE_IN_3_14
const gchar **gdk_clipboard_get_content_types (GdkClipboard          *clipboard);


G_END_DECLS

#endif /* __GDK_CLIPBOARD_H__ */
