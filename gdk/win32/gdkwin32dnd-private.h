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
  GdkDragContext drag;
  GdkDragProtocol protocol;
  GdkSurface *ipc_window;
  GdkSurface *drag_surface;
  GdkCursor *cursor;
  GdkSeat *grab_seat;
  GdkDragAction current_action;

  GdkWin32DragContextUtilityData util_data;

  guint scale;          /* Temporarily caches the HiDPI scale */
  gint hot_x;           /* Hotspot offset from the top-left of the drag-window, scaled (can be added to GDK space coordinates) */
  gint hot_y;
  gint start_x;         /* Coordinates of the drag start, in GDK space */
  gint start_y;

  guint drag_status : 4;             /* Current status of drag */
  guint drop_failed : 1;             /* Whether the drop was unsuccessful */
  guint handle_events : 1;           /* Whether handle_event() should do anything */
};

struct _GdkWin32DragContextClass
{
  GdkDragContextClass parent_class;
};


gpointer _gdk_win32_dnd_thread_main                      (gpointer         data);

GdkDragContext *_gdk_win32_find_drag_for_dest_surface    (GdkSurface      *dest_surface);

void     _gdk_win32_drag_send_local_status_event         (GdkDragContext  *drag,
                                                          GdkDragAction    action);
void     _gdk_win32_local_send_enter                     (GdkDragContext  *drag,
                                                          GdkSurface      *dest_surface,
                                                          guint32          time);

GdkDragContext *_gdk_win32_drag_context_find             (GdkSurface      *source,
                                                          GdkSurface      *dest);
GdkDrop        *_gdk_win32_get_drop_for_dest_surface     (GdkSurface      *dest);

void     _gdk_win32_drag_do_leave                        (GdkDragContext  *context,
                                                          guint32          time);

gboolean _gdk_win32_local_drop_target_will_emit_motion (GdkDrop *drop,
                                                        gint     x_root,
                                                        gint     y_root,
                                                        DWORD    grfKeyState);

void     _gdk_win32_local_drop_target_dragenter (GdkDragContext *drag,
                                                 GdkSurface     *dest_surface,
                                                 gint            x_root,
                                                 gint            y_root,
                                                 DWORD           grfKeyState,
                                                 guint32         time_,
                                                 GdkDragAction  *actions);
void     _gdk_win32_local_drop_target_dragover  (GdkDrop        *drop,
                                                 GdkDragContext *drag,
                                                 gint            x_root,
                                                 gint            y_root,
                                                 DWORD           grfKeyState,
                                                 guint32         time_,
                                                 GdkDragAction  *actions);
void     _gdk_win32_local_drop_target_dragleave (GdkDrop *drop,
                                                 guint32  time_);
void     _gdk_win32_local_drop_target_drop      (GdkDrop        *drop,
                                                 GdkDragContext *drag,
                                                 guint32         time_,
                                                 GdkDragAction  *actions);

void     _gdk_win32_local_drag_give_feedback    (GdkDragContext *drag,
                                                 GdkDragAction   actions);
void     _gdk_win32_local_drag_context_drop_response (GdkDragContext *drag,
                                                      GdkDragAction   action);


G_END_DECLS

#endif /* __GDK_WIN32_DND_PRIVATE_H__ */
