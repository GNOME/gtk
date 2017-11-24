/* GDK - The GIMP Drawing Kit
 *
 * Copyright (C) 2017 Benjamin Otte <otte@gnome.org>
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

GDK_AVAILABLE_IN_3_94
GType                   gdk_clipboard_get_type          (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_94
GdkDisplay *            gdk_clipboard_get_display       (GdkClipboard          *clipboard);
GDK_AVAILABLE_IN_3_94
GdkContentFormats *     gdk_clipboard_get_formats       (GdkClipboard          *clipboard);
GDK_AVAILABLE_IN_3_94
gboolean                gdk_clipboard_is_local          (GdkClipboard          *clipboard);
GDK_AVAILABLE_IN_3_94
GdkContentProvider *    gdk_clipboard_get_content       (GdkClipboard          *clipboard);

GDK_AVAILABLE_IN_3_94
void                    gdk_clipboard_read_async        (GdkClipboard          *clipboard,
                                                         const char           **mime_types,
                                                         int                    io_priority,
                                                         GCancellable          *cancellable,
                                                         GAsyncReadyCallback    callback,
                                                         gpointer               user_data);
GDK_AVAILABLE_IN_3_94
GInputStream *          gdk_clipboard_read_finish       (GdkClipboard          *clipboard,
                                                         const char           **out_mime_type,
                                                         GAsyncResult          *result,
                                                         GError               **error);
GDK_AVAILABLE_IN_3_94
void                    gdk_clipboard_read_value_async  (GdkClipboard          *clipboard,
                                                         GType                  type,
                                                         int                    io_priority,
                                                         GCancellable          *cancellable,
                                                         GAsyncReadyCallback    callback,
                                                         gpointer               user_data);
GDK_AVAILABLE_IN_3_94
const GValue *          gdk_clipboard_read_value_finish (GdkClipboard          *clipboard,
                                                         GAsyncResult          *res,
                                                         GError               **error);
GDK_AVAILABLE_IN_3_94
void                    gdk_clipboard_read_pixbuf_async (GdkClipboard          *clipboard,
                                                         GCancellable          *cancellable,
                                                         GAsyncReadyCallback    callback,
                                                         gpointer               user_data);
GDK_AVAILABLE_IN_3_94
GdkPixbuf *             gdk_clipboard_read_pixbuf_finish(GdkClipboard          *clipboard,
                                                         GAsyncResult          *res,
                                                         GError               **error);
GDK_AVAILABLE_IN_3_94
void                    gdk_clipboard_read_text_async   (GdkClipboard          *clipboard,
                                                         GCancellable          *cancellable,
                                                         GAsyncReadyCallback    callback,
                                                         gpointer               user_data);
GDK_AVAILABLE_IN_3_94
char *                  gdk_clipboard_read_text_finish  (GdkClipboard          *clipboard,
                                                         GAsyncResult          *res,
                                                         GError               **error);

GDK_AVAILABLE_IN_3_94
gboolean                gdk_clipboard_set_content       (GdkClipboard          *clipboard,
                                                         GdkContentProvider    *provider);
GDK_AVAILABLE_IN_3_94
void                    gdk_clipboard_set_text          (GdkClipboard          *clipboard,
                                                         const char            *text);
GDK_AVAILABLE_IN_3_94
void                    gdk_clipboard_set_pixbuf        (GdkClipboard          *clipboard,
                                                         GdkPixbuf             *pixbuf);

G_END_DECLS

#endif /* __GDK_CLIPBOARD_H__ */
