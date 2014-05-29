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

#ifndef __GDK_CLIPBOARD_FALLBACK_H__
#define __GDK_CLIPBOARD_FALLBACK_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkclipboard.h>


G_BEGIN_DECLS

#define GDK_TYPE_CLIPBOARD_FALLBACK    (gdk_clipboard_fallback_get_type ())
#define GDK_CLIPBOARD_FALLBACK(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_CLIPBOARD_FALLBACK, GdkClipboardFallback))
#define GDK_IS_CLIPBOARD_FALLBACK(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_CLIPBOARD_FALLBACK))

typedef struct _GdkClipboardFallback GdkClipboardFallback;

GDK_AVAILABLE_IN_3_14
GType         gdk_clipboard_fallback_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_14
GdkClipboard *gdk_clipboard_fallback_new      (void);

G_END_DECLS

#endif /* __GDK_CLIPBOARD_FALLBACK_H__ */
