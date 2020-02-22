/* gdksurface-quartz.c
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_PRIVATE_QUARTZ_H__
#define __GDK_PRIVATE_QUARTZ_H__

#define GDK_QUARTZ_ALLOC_POOL NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init]
#define GDK_QUARTZ_RELEASE_POOL [pool release]

#include <gdk/quartz/gdkquartz.h>
#include <gdk/quartz/gdkdevicemanager-core-quartz.h>
#include <gdk/quartz/gdkdnd-quartz.h>
#include <gdk/quartz/gdkscreen-quartz.h>
#include <gdk/quartz/gdksurface-quartz.h>

#include <gdk/gdk.h>

#include "gdkinternals.h"

#include "config.h"

extern GdkDisplay *_gdk_display;
extern GdkQuartzScreen *_gdk_screen;
extern GdkSurface *_gdk_root;
extern GdkDeviceManager *_gdk_device_manager;

extern GdkDragContext *_gdk_quartz_drag_source_context;

#define GDK_SURFACE_IS_QUARTZ(win)        (GDK_IS_SURFACE_IMPL_QUARTZ (((GdkSurface *)win)->impl))

/* Initialization */
void _gdk_quartz_surface_init_windowing      (GdkDisplay *display);
void _gdk_quartz_events_init                (void);
void _gdk_quartz_event_loop_init            (void);

/* Cursor */
NSCursor   *_gdk_quartz_cursor_get_ns_cursor        (GdkCursor *cursor);

/* Events */
typedef enum {
  GDK_QUARTZ_EVENT_SUBTYPE_EVENTLOOP
} GdkQuartzEventSubType;

void         _gdk_quartz_events_update_focus_window    (GdkSurface *new_window,
                                                        gboolean   got_focus);
void         _gdk_quartz_events_send_map_event         (GdkSurface *window);

GdkModifierType _gdk_quartz_events_get_current_keyboard_modifiers (void);
GdkModifierType _gdk_quartz_events_get_current_mouse_modifiers    (void);

void         _gdk_quartz_events_break_all_grabs         (guint32    time);

/* Event loop */
gboolean   _gdk_quartz_event_loop_check_pending (void);
NSEvent *  _gdk_quartz_event_loop_get_pending   (void);
void       _gdk_quartz_event_loop_release_event (NSEvent *event);

/* Keys */
GdkEventType _gdk_quartz_keys_event_type  (NSEvent   *event);
gboolean     _gdk_quartz_keys_is_modifier (guint      keycode);
void         _gdk_quartz_synthesize_null_key_event (GdkSurface *window);

/* Drag and Drop */
GdkDragContext * _gdk_quartz_surface_drag_begin   (GdkSurface   *window,
                                                   GdkDevice   *device,
                                                   GList       *targets,
                                                   gint         x_root,
                                                   gint         y_root);

/* Display */

GdkDisplay *    _gdk_quartz_display_open (const gchar *name);


/* Screen */
GdkQuartzScreen  *_gdk_quartz_screen_new                      (void);
void        _gdk_quartz_screen_update_window_sizes      (GdkQuartzScreen *screen);

/* Screen methods - events */
gboolean    _gdk_quartz_get_setting                 (const gchar *name,
                                                     GValue      *value);


/* Window */
gboolean    _gdk_quartz_surface_is_ancestor         (GdkSurface *ancestor,
                                                     GdkSurface *window);
void       _gdk_quartz_surface_gdk_xy_to_xy         (gint       gdk_x,
                                                     gint       gdk_y,
                                                     gint      *ns_x,
                                                     gint      *ns_y);
void       _gdk_quartz_surface_xy_to_gdk_xy         (gint       ns_x,
                                                     gint       ns_y,
                                                     gint      *gdk_x,
                                                     gint      *gdk_y);
void       _gdk_quartz_surface_nspoint_to_gdk_xy    (NSPoint    point,
                                                     gint      *x,
                                                     gint      *y);
GdkSurface *_gdk_quartz_surface_find_child          (GdkSurface *window,
                                                     gint       x,
                                                     gint       y,
                                                     gboolean   get_toplevel);
void       _gdk_quartz_surface_attach_to_parent     (GdkSurface *window);
void       _gdk_quartz_surface_detach_from_parent   (GdkSurface *window);
void       _gdk_quartz_surface_did_become_main      (GdkSurface *window);
void       _gdk_quartz_surface_did_resign_main      (GdkSurface *window);
void       _gdk_quartz_surface_debug_highlight      (GdkSurface *window,
                                                     gint       number);

void       _gdk_quartz_surface_update_position           (GdkSurface    *window);
void       _gdk_quartz_surface_update_fullscreen_state   (GdkSurface    *window);

/* Window methods - property */
gboolean _gdk_quartz_surface_get_property     (GdkSurface    *window,
                                               GdkAtom       property,
                                               GdkAtom       type,
                                               gulong        offset,
                                               gulong        length,
                                               gint          pdelete,
                                               GdkAtom      *actual_property_type,
                                               gint         *actual_format_type,
                                               gint         *actual_length,
                                               guchar      **data);
void     _gdk_quartz_surface_change_property  (GdkSurface    *window,
                                               GdkAtom       property,
                                               GdkAtom       type,
                                               gint          format,
                                               GdkPropMode   mode,
                                               const guchar *data,
                                               gint          nelements);
void     _gdk_quartz_surface_delete_property  (GdkSurface    *window,
                                               GdkAtom       property);


#endif /* __GDK_PRIVATE_QUARTZ_H__ */
