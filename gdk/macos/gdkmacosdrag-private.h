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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GDK_MACOS_DRAG_PRIVATE_H__
#define __GDK_MACOS_DRAG_PRIVATE_H__

#include "gdkdragprivate.h"

#include "gdkmacosdragsurface-private.h"

G_BEGIN_DECLS

#define GDK_TYPE_MACOS_DRAG            (gdk_macos_drag_get_type ())
#define GDK_MACOS_DRAG(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_MACOS_DRAG, GdkMacosDrag))
#define GDK_MACOS_DRAG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_MACOS_DRAG, GdkMacosDragClass))
#define GDK_IS_MACOS_DRAG(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_MACOS_DRAG))
#define GDK_IS_MACOS_DRAG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_MACOS_DRAG))
#define GDK_MACOS_DRAG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_MACOS_DRAG, GdkMacosDragClass))

typedef struct _GdkMacosDrag GdkMacosDrag;
typedef struct _GdkMacosDragClass GdkMacosDragClass;

struct _GdkMacosDrag
{
  GdkDrag parent_instance;

  GdkMacosDragSurface *drag_surface;
  GdkSeat *drag_seat;
  GdkCursor *cursor;

  int hot_x;
  int hot_y;

  int last_x;
  int last_y;

  int start_x;
  int start_y;

  guint did_update : 1;
  guint cancelled : 1;
};

struct _GdkMacosDragClass
{
  GdkDragClass parent_class;
};

GType    gdk_macos_drag_get_type (void) G_GNUC_CONST;
gboolean _gdk_macos_drag_begin   (GdkMacosDrag *self);

G_END_DECLS

#endif /* __GDK_MACOS_DRAG_PRIVATE_H__ */
