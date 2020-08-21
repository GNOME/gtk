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

#if !defined (__GDKWIN32_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdkwin32.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _GdkWin32DragUtilityData GdkWin32DragUtilityData;

/* This structure is deliberately designed to be
 * copiable with memcpy(), i.e. no pointers inside.
 */
struct _GdkWin32DragUtilityData
{
  int              last_x;         /* Coordinates from last event, in GDK space */
  int              last_y;
  DWORD            last_key_state; /* Key state from last event */
  GdkWin32DndState state;
};

struct _GdkWin32Drag
{
  GdkDrag drag;

  /* The drag protocol being used */
  GdkDragProtocol  protocol;

  /* The surface used for grabs.
   * Usually the same as GdkDrag->source_surface
   */
  GdkSurface      *grab_surface;

  /* The window currently under cursor.
   * Will be INVALID_HANDLE_VALUE (not NULL!) if it is not known
   */
  HWND             dest_window;

  /* The surface that we move along with the cursor to indicate
   * what exactly is being dragged.
   */
  GdkSurface      *drag_surface;
  GdkCursor       *cursor;
  GdkSeat         *grab_seat;

  /* Used to keep track of current drag action, and
   * emit an "action-changed" signal whenever it changes.
   * In the end the actual action is decided on the basis of
   * the last information we receive from the drop site, this
   * field is not used for that purpose.
   */
  GdkDragAction    current_action;

  GdkWin32DragUtilityData util_data;

  guint scale;             /* Temporarily caches the HiDPI scale */
  int   hot_x;             /* Hotspot offset from the top-left of the drag-window, scaled (can be added to GDK space coordinates) */
  int   hot_y;
  int   start_x;           /* Coordinates of the drag start, in GDK space */
  int   start_y;

  guint drag_status : 4;   /* Current status of drag */
  guint drop_failed : 1;   /* Whether the drop was unsuccessful */
  guint handle_events : 1; /* Whether handle_event() should do anything */
};

struct _GdkWin32DragClass
{
  GdkDragClass parent_class;
};


gpointer _gdk_win32_dnd_thread_main                      (gpointer         data);

GdkDrag *_gdk_win32_find_drag_for_dest_window     (HWND             dest_window);
GdkDrop  *_gdk_win32_get_drop_for_dest_surface     (GdkSurface      *dest);

gboolean _gdk_win32_local_drop_target_will_emit_motion   (GdkDrop *drop,
                                                          int      x_root,
                                                          int      y_root,
                                                          DWORD    grfKeyState);

void     _gdk_win32_local_drop_target_dragenter          (GdkDrag        *drag,
                                                          GdkSurface     *dest_surface,
                                                          int             x_root,
                                                          int             y_root,
                                                          DWORD           grfKeyState,
                                                          guint32         time_,
                                                          GdkDragAction  *actions);
void     _gdk_win32_local_drop_target_dragover           (GdkDrop        *drop,
                                                          GdkDrag        *drag,
                                                          int             x_root,
                                                          int             y_root,
                                                          DWORD           grfKeyState,
                                                          guint32         time_,
                                                          GdkDragAction  *actions);
void     _gdk_win32_local_drop_target_dragleave          (GdkDrop        *drop,
                                                          guint32         time_);
void     _gdk_win32_local_drop_target_drop               (GdkDrop        *drop,
                                                          GdkDrag        *drag,
                                                          guint32         time_,
                                                          GdkDragAction  *actions);

void     _gdk_win32_local_drag_give_feedback             (GdkDrag        *drag,
                                                          GdkDragAction   actions);
void     _gdk_win32_local_drag_drop_response             (GdkDrag        *drag,
                                                          GdkDragAction   action);


G_END_DECLS

#endif /* __GDK_WIN32_DND_PRIVATE_H__ */
