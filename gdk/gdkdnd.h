/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GDK_DND_H__
#define __GDK_DND_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdktypes.h>
#include <gdk/gdkdevice.h>
#include <gdk/gdkevents.h>

G_BEGIN_DECLS

#define GDK_TYPE_DRAG_CONTEXT              (gdk_drag_context_get_type ())
#define GDK_DRAG_CONTEXT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DRAG_CONTEXT, GdkDragContext))
#define GDK_IS_DRAG_CONTEXT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DRAG_CONTEXT))

/**
 * GdkDragCancelReason:
 * @GDK_DRAG_CANCEL_NO_TARGET: There is no suitable drop target.
 * @GDK_DRAG_CANCEL_USER_CANCELLED: Drag cancelled by the user
 * @GDK_DRAG_CANCEL_ERROR: Unspecified error.
 *
 * Used in #GdkDragContext to the reason of a cancelled DND operation.
 *
 * Since: 3.20
 */
typedef enum {
  GDK_DRAG_CANCEL_NO_TARGET,
  GDK_DRAG_CANCEL_USER_CANCELLED,
  GDK_DRAG_CANCEL_ERROR
} GdkDragCancelReason;

GDK_AVAILABLE_IN_ALL
GType            gdk_drag_context_get_type             (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GdkDisplay *     gdk_drag_context_get_display          (GdkDragContext *context);
GDK_AVAILABLE_IN_ALL
GdkDevice *      gdk_drag_context_get_device           (GdkDragContext *context);

GDK_AVAILABLE_IN_ALL
GdkContentFormats *gdk_drag_context_get_formats        (GdkDragContext *context);
GDK_AVAILABLE_IN_ALL
GdkDragAction    gdk_drag_context_get_actions          (GdkDragContext *context);
GDK_AVAILABLE_IN_ALL
GdkDragAction    gdk_drag_context_get_suggested_action (GdkDragContext *context);
GDK_AVAILABLE_IN_ALL
GdkDragAction    gdk_drag_context_get_selected_action  (GdkDragContext *context);

GDK_AVAILABLE_IN_ALL
GdkDragAction     gdk_drag_action_is_unique            (GdkDragAction   action);

/* Source side */

GDK_AVAILABLE_IN_ALL
GdkDragContext *        gdk_drag_begin                  (GdkSurface              *surface,
                                                         GdkDevice              *device,
                                                         GdkContentProvider     *content,
                                                         GdkDragAction           actions,
                                                         gint                    dx,
                                                         gint                    dy);

GDK_AVAILABLE_IN_ALL
void            gdk_drag_drop_done   (GdkDragContext *context,
                                      gboolean        success);

GDK_AVAILABLE_IN_ALL
GdkSurface      *gdk_drag_context_get_drag_surface (GdkDragContext *context);

GDK_AVAILABLE_IN_ALL
void            gdk_drag_context_set_hotspot (GdkDragContext *context,
                                              gint            hot_x,
                                              gint            hot_y);

G_END_DECLS

#endif /* __GDK_DND_H__ */
