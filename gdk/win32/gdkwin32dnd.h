/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2010 Red Hat, Inc.
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

#ifndef __GDK_WIN32_DND_H__
#define __GDK_WIN32_DND_H__

#if !defined (__GDKWIN32_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdkwin32.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GDK_TYPE_WIN32_DRAG_CONTEXT              (gdk_win32_drag_context_get_type ())
#define GDK_WIN32_DRAG_CONTEXT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WIN32_DRAG_CONTEXT, GdkWin32DragContext))
#define GDK_WIN32_DRAG_CONTEXT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WIN32_DRAG_CONTEXT, GdkWin32DragContextClass))
#define GDK_IS_WIN32_DRAG_CONTEXT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WIN32_DRAG_CONTEXT))
#define GDK_IS_WIN32_DRAG_CONTEXT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WIN32_DRAG_CONTEXT))
#define GDK_WIN32_DRAG_CONTEXT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WIN32_DRAG_CONTEXT, GdkWin32DragContextClass))

#ifdef GDK_COMPILATION
typedef struct _GdkWin32DragContext GdkWin32DragContext;
#else
typedef GdkDragContext GdkWin32DragContext;
#endif
typedef struct _GdkWin32DragContextClass GdkWin32DragContextClass;

GType    gdk_win32_drag_context_get_type (void);

G_END_DECLS

#endif /* __GDK_WIN32_DRAG_CONTEXT_H__ */
