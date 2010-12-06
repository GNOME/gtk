/* gdkwindow-quartz.c
 *
 * Copyright (C) 2005-2007 Imendio AB
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDK_PRIVATE_QUARTZ_H__
#define __GDK_PRIVATE_QUARTZ_H__

#define GDK_QUARTZ_ALLOC_POOL NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init]
#define GDK_QUARTZ_RELEASE_POOL [pool release]

#include <gdk/gdkprivate.h>
#include <gdk/quartz/gdkwindow-quartz.h>
#include <gdk/quartz/gdkquartz.h>

#include <gdk/gdk.h>

#include "gdkinternals.h"

#include "config.h"

#define GDK_DRAG_CONTEXT_PRIVATE(context) ((GdkDragContextPrivate *) GDK_DRAG_CONTEXT (context)->windowing_data)

typedef struct _GdkCursorPrivate GdkCursorPrivate;
typedef struct _GdkDragContextPrivate GdkDragContextPrivate;

struct _GdkVisualClass
{
  GObjectClass parent_class;
};

struct _GdkCursorPrivate
{
  GdkCursor cursor;
  NSCursor *nscursor;
};

struct _GdkDragContextPrivate
{
  id <NSDraggingInfo> dragging_info;
  GdkDevice *device;
};

extern GdkDisplay *_gdk_display;
extern GdkScreen *_gdk_screen;
extern GdkWindow *_gdk_root;

extern GdkDragContext *_gdk_quartz_drag_source_context;

#define GDK_WINDOW_IS_QUARTZ(win)        (GDK_IS_WINDOW_IMPL_QUARTZ (((GdkWindow *)win)->impl))

/* Initialization */
void _gdk_windowing_update_window_sizes     (GdkScreen *screen);
void _gdk_windowing_window_init             (void);
void _gdk_events_init                       (void);
void _gdk_visual_init                       (void);
void _gdk_input_init                        (void);
void _gdk_quartz_event_loop_init            (void);

/* GC */
typedef enum {
  GDK_QUARTZ_CONTEXT_STROKE = 1 << 0,
  GDK_QUARTZ_CONTEXT_FILL   = 1 << 1,
  GDK_QUARTZ_CONTEXT_TEXT   = 1 << 2
} GdkQuartzContextValuesMask;

/* Window */
gboolean    _gdk_quartz_window_is_ancestor          (GdkWindow *ancestor,
                                                     GdkWindow *window);
void       _gdk_quartz_window_gdk_xy_to_xy          (gint       gdk_x,
                                                     gint       gdk_y,
                                                     gint      *ns_x,
                                                     gint      *ns_y);
void       _gdk_quartz_window_xy_to_gdk_xy          (gint       ns_x,
                                                     gint       ns_y,
                                                     gint      *gdk_x,
                                                     gint      *gdk_y);
void       _gdk_quartz_window_nspoint_to_gdk_xy     (NSPoint    point,
                                                     gint      *x,
                                                     gint      *y);
GdkWindow *_gdk_quartz_window_find_child            (GdkWindow *window,
						     gint       x,
						     gint       y,
                                                     gboolean   get_toplevel);
void       _gdk_quartz_window_attach_to_parent      (GdkWindow *window);
void       _gdk_quartz_window_detach_from_parent    (GdkWindow *window);
void       _gdk_quartz_window_did_become_main       (GdkWindow *window);
void       _gdk_quartz_window_did_resign_main       (GdkWindow *window);
void       _gdk_quartz_window_debug_highlight       (GdkWindow *window,
                                                     gint       number);

void       _gdk_quartz_window_set_needs_display_in_region (GdkWindow    *window,
                                                           cairo_region_t    *region);

void       _gdk_quartz_window_update_position           (GdkWindow    *window);

/* Events */
typedef enum {
  GDK_QUARTZ_EVENT_SUBTYPE_EVENTLOOP
} GdkQuartzEventSubType;

void         _gdk_quartz_events_update_focus_window    (GdkWindow *new_window,
                                                        gboolean   got_focus);
void         _gdk_quartz_events_send_map_event         (GdkWindow *window);
GdkEventMask _gdk_quartz_events_get_current_event_mask (void);

void         _gdk_quartz_events_send_enter_notify_event (GdkWindow *window);

/* Event loop */
gboolean   _gdk_quartz_event_loop_check_pending (void);
NSEvent *  _gdk_quartz_event_loop_get_pending   (void);
void       _gdk_quartz_event_loop_release_event (NSEvent *event);

/* Keys */
GdkEventType _gdk_quartz_keys_event_type  (NSEvent   *event);
gboolean     _gdk_quartz_keys_is_modifier (guint      keycode);

/* Geometry */
void        _gdk_quartz_window_scroll      (GdkWindow       *window,
                                            gint             dx,
                                            gint             dy);
void        _gdk_quartz_window_translate   (GdkWindow       *window,
                                            cairo_region_t  *area,
                                            gint             dx,
                                            gint             dy);
gboolean    _gdk_quartz_window_queue_antiexpose  (GdkWindow *window,
                                                  cairo_region_t *area);

#endif /* __GDK_PRIVATE_QUARTZ_H__ */
