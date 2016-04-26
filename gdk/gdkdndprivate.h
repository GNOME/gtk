/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2010, Red Hat, Inc
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

#ifndef __GDK_DND_PRIVATE_H__
#define __GDK_DND_PRIVATE_H__

#include "gdkdnd.h"

G_BEGIN_DECLS


#define GDK_DRAG_CONTEXT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DRAG_CONTEXT, GdkDragContextClass))
#define GDK_IS_DRAG_CONTEXT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DRAG_CONTEXT))
#define GDK_DRAG_CONTEXT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DRAG_CONTEXT, GdkDragContextClass))

typedef struct _GdkDragContextClass GdkDragContextClass;


struct _GdkDragContextClass {
  GObjectClass parent_class;

  GdkWindow * (*find_window)   (GdkDragContext  *context,
                                GdkWindow       *drag_window,
                                GdkScreen       *screen,
                                gint             x_root,
                                gint             y_root,
                                GdkDragProtocol *protocol);
  GdkAtom     (*get_selection) (GdkDragContext  *context);
  gboolean    (*drag_motion)   (GdkDragContext  *context,
                                GdkWindow       *dest_window,
                                GdkDragProtocol  protocol,
                                gint             root_x,
                                gint             root_y,
                                GdkDragAction    suggested_action,
                                GdkDragAction    possible_actions,
                                guint32          time_);
  void        (*drag_status)   (GdkDragContext  *context,
                                GdkDragAction    action,
                                guint32          time_);
  void        (*drag_abort)    (GdkDragContext  *context,
                                guint32          time_);
  void        (*drag_drop)     (GdkDragContext  *context,
                                guint32          time_);
  void        (*drop_reply)    (GdkDragContext  *context,
                                gboolean         accept,
                                guint32          time_);
  void        (*drop_finish)   (GdkDragContext  *context,
                                gboolean         success,
                                guint32          time_);
  gboolean    (*drop_status)   (GdkDragContext  *context);
  GdkWindow*  (*get_drag_window) (GdkDragContext *context);
  void        (*set_hotspot)   (GdkDragContext  *context,
                                gint             hot_x,
                                gint             hot_y);
  void        (*drop_done)     (GdkDragContext   *context,
                                gboolean          success);

  gboolean    (*manage_dnd)     (GdkDragContext  *context,
                                 GdkWindow       *ipc_window,
                                 GdkDragAction    actions);
  void        (*set_cursor)     (GdkDragContext  *context,
                                 GdkCursor       *cursor);
  void        (*cancel)         (GdkDragContext      *context,
                                 GdkDragCancelReason  reason);
  void        (*drop_performed) (GdkDragContext  *context,
                                 guint32          time);
  void        (*dnd_finished)   (GdkDragContext  *context);

  gboolean    (*handle_event)   (GdkDragContext  *context,
                                 const GdkEvent  *event);
  void        (*action_changed) (GdkDragContext  *context,
                                 GdkDragAction    action);

  void        (*commit_drag_status) (GdkDragContext  *context);
};

struct _GdkDragContext {
  GObject parent_instance;

  /*< private >*/
  GdkDragProtocol protocol;

  GdkDisplay *display;

  gboolean is_source;
  GdkWindow *source_window;
  GdkWindow *dest_window;
  GdkWindow *drag_window;

  GList *targets;
  GdkDragAction actions;
  GdkDragAction suggested_action;
  GdkDragAction action;

  guint32 start_time;

  GdkDevice *device;

  guint drop_done : 1; /* Whether gdk_drag_drop_done() was performed */
};

GList *  gdk_drag_context_list (void);

void     gdk_drag_context_set_cursor          (GdkDragContext *context,
                                               GdkCursor      *cursor);
void     gdk_drag_context_cancel              (GdkDragContext      *context,
                                               GdkDragCancelReason  reason);
gboolean gdk_drag_context_handle_source_event (GdkEvent *event);
gboolean gdk_drag_context_handle_dest_event   (GdkEvent *event);
GdkCursor * gdk_drag_get_cursor               (GdkDragContext *context,
                                               GdkDragAction   action);

G_END_DECLS

#endif
