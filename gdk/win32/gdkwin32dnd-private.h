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

#ifndef __GDK_WIN32_DND_PRIVATE_H__
#define __GDK_WIN32_DND_PRIVATE_H__

#if !defined (__GDKWIN32_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdkwin32.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _GdkWin32DragContextUtilityData GdkWin32DragContextUtilityData;

struct _GdkWin32DragContextUtilityData
{
  gint             last_x;         /* Coordinates from last event, in GDK space */
  gint             last_y;
  DWORD            last_key_state; /* Key state from last event */
  GdkWin32DndState state;
};

struct _GdkWin32DragContext
{
  GdkDragContext context;
  GdkSurface *ipc_window;
  GdkSurface *drag_surface;
  GdkCursor *cursor;
  GdkSeat *grab_seat;
  GdkDragAction actions;
  GdkDragAction current_action;

  GdkWin32DragContextUtilityData util_data;

  guint scale;          /* Temporarily caches the HiDPI scale */
  gint hot_x;           /* Hotspot offset from the top-left of the drag-window, scaled (can be added to GDK space coordinates) */
  gint hot_y;
  gint start_x;         /* Coordinates of the drag start, in GDK space */
  gint start_y;

  guint drag_status : 4;             /* Current status of drag */
  guint drop_failed : 1;             /* Whether the drop was unsuccessful */
};

struct _GdkWin32DragContextClass
{
  GdkDragContextClass parent_class;
};

struct _GdkWin32DropContext
{
  GdkDragContext context;
  GdkDragAction actions;
  GdkDragAction current_action;

  guint scale;          /* Temporarily caches the HiDPI scale */
  gint             last_x;         /* Coordinates from last event, in GDK space */
  gint             last_y;
  DWORD            last_key_state; /* Key state from last event */

  /* Just like context->formats, but an array, and with format IDs
   * stored inside.
   */
  GArray *droptarget_w32format_contentformat_map;

  GdkWin32DragContext *local_source_context;

  guint drag_status : 4;             /* Current status of drag */
  guint drop_failed : 1;             /* Whether the drop was unsuccessful */
};

struct _GdkWin32DropContextClass
{
  GdkDragContextClass parent_class;
};

gpointer _gdk_win32_dnd_thread_main (gpointer data);

GdkDragContext *_gdk_win32_find_source_context_for_dest_surface  (GdkSurface      *dest_surface);

void            _gdk_win32_drag_context_send_local_status_event (GdkDragContext *src_context,
                                                                 GdkDragAction   action);
void            _gdk_win32_local_send_enter                     (GdkDragContext *context,
                                                                 guint32         time);

GdkDragContext *_gdk_win32_drag_context_find                    (GdkSurface      *source,
                                                                 GdkSurface      *dest);
GdkDragContext *_gdk_win32_drop_context_find                    (GdkSurface      *source,
                                                                 GdkSurface      *dest);


void            _gdk_win32_drag_do_leave                        (GdkDragContext *context,
                                                                 guint32         time);


G_END_DECLS

#endif /* __GDK_WIN32_DND_PRIVATE_H__ */
