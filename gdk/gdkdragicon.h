/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __GDK_DRAG_ICON_H__
#define __GDK_DRAG_ICON_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdksurface.h>

G_BEGIN_DECLS

#define GDK_TYPE_DRAG_ICON               (gdk_drag_icon_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (GdkDragIcon, gdk_drag_icon, GDK, DRAG_ICON, GObject)

GDK_AVAILABLE_IN_ALL
gboolean        gdk_drag_icon_present           (GdkDragIcon    *drag_icon,
                                                 int             width,
                                                 int             height);

G_END_DECLS

#endif /* __GDK_DRAG_ICON_H__ */
