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

#ifndef __GDK_CLIPBOARD_X11_H__
#define __GDK_CLIPBOARD_X11_H__

#include "gdk/gdkclipboard.h"

#include <X11/Xlib.h>


G_BEGIN_DECLS

#define GDK_TYPE_CLIPBOARD_X11    (gdk_clipboard_x11_get_type ())
#define GDK_CLIPBOARD_X11(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_CLIPBOARD_X11, GdkClipboardX11))
#define GDK_IS_CLIPBOARD_X11(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_CLIPBOARD_X11))

typedef struct _GdkClipboardX11 GdkClipboardX11;

GType            gdk_clipboard_x11_get_type (void) G_GNUC_CONST;
GdkClipboardX11 *gdk_clipboard_x11_new      (GdkDisplay  *display,
                                             const gchar *selection);

gboolean gdk_clipboard_x11_handle_selection_clear        (GdkClipboardX11        *cb,
                                                          XSelectionClearEvent   *event);
gboolean gdk_clipboard_x11_handle_selection_request      (GdkClipboardX11        *cb,
                                                          XSelectionRequestEvent *event);
gboolean gdk_clipboard_x11_handle_selection_notify       (GdkClipboardX11        *cb,
                                                          XSelectionEvent        *event);
gboolean gdk_clipboard_x11_handle_selection_owner_change (GdkClipboardX11        *cb,
                                                          XEvent                 *event);

G_END_DECLS

#endif /* __GDK_CLIPBOARD_X11_H__ */
