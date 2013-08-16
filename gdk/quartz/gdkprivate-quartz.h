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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_PRIVATE_QUARTZ_H__
#define __GDK_PRIVATE_QUARTZ_H__

#define GDK_QUARTZ_ALLOC_POOL NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init]
#define GDK_QUARTZ_RELEASE_POOL [pool release]

#include <gdk/gdkprivate.h>
#include <gdk/quartz/gdkquartz.h>
#include <gdk/quartz/gdkdevicemanager-core-quartz.h>
#include <gdk/quartz/gdkdnd-quartz.h>
#include <gdk/quartz/gdkscreen-quartz.h>
#include <gdk/quartz/gdkwindow-quartz.h>

#include <gdk/gdk.h>

#include "gdkinternals.h"

#include "config.h"

extern GdkDisplay *_gdk_display;
extern GdkScreen *_gdk_screen;
extern GdkWindow *_gdk_root;

extern GdkDragContext *_gdk_quartz_drag_source_context;

#define GDK_WINDOW_IS_QUARTZ(win)        (GDK_IS_WINDOW_IMPL_QUARTZ (((GdkWindow *)win)->impl))

/* Initialization */
void _gdk_quartz_window_init_windowing      (GdkDisplay *display,
                                             GdkScreen  *screen);
void _gdk_quartz_events_init                (void);
void _gdk_quartz_event_loop_init            (void);

/* Cursor */
NSCursor   *_gdk_quartz_cursor_get_ns_cursor        (GdkCursor *cursor);

/* Events */
typedef enum {
  GDK_QUARTZ_EVENT_SUBTYPE_EVENTLOOP
} GdkQuartzEventSubType;

void         _gdk_quartz_events_update_focus_window    (GdkWindow *new_window,
                                                        gboolean   got_focus);
void         _gdk_quartz_events_send_map_event         (GdkWindow *window);

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
void         _gdk_quartz_synthesize_null_key_event (GdkWindow *window);

/* Drag and Drop */
void        _gdk_quartz_window_register_dnd      (GdkWindow   *window);
GdkDragContext * _gdk_quartz_window_drag_begin   (GdkWindow   *window,
                                                  GdkDevice   *device,
                                                  GList       *targets);

/* Display */

GdkDisplay *    _gdk_quartz_display_open (const gchar *name);

/* Display methods - events */
void     _gdk_quartz_display_queue_events (GdkDisplay *display);
gboolean _gdk_quartz_display_has_pending  (GdkDisplay *display);

void       _gdk_quartz_display_event_data_copy (GdkDisplay     *display,
                                                const GdkEvent *src,
                                                GdkEvent       *dst);
void       _gdk_quartz_display_event_data_free (GdkDisplay     *display,
                                                GdkEvent       *event);

/* Display methods - cursor */
GdkCursor *_gdk_quartz_display_get_cursor_for_type     (GdkDisplay      *display,
                                                        GdkCursorType    type);
GdkCursor *_gdk_quartz_display_get_cursor_for_name     (GdkDisplay      *display,
                                                        const gchar     *name);
GdkCursor *_gdk_quartz_display_get_cursor_for_surface  (GdkDisplay      *display,
                                                        cairo_surface_t *surface,
                                                        gdouble          x,
                                                        gdouble          y);
gboolean   _gdk_quartz_display_supports_cursor_alpha   (GdkDisplay    *display);
gboolean   _gdk_quartz_display_supports_cursor_color   (GdkDisplay    *display);
void       _gdk_quartz_display_get_default_cursor_size (GdkDisplay *display,
                                                        guint      *width,
                                                        guint      *height);
void       _gdk_quartz_display_get_maximal_cursor_size (GdkDisplay *display,
                                                        guint      *width,
                                                        guint      *height);

/* Display methods - window */
void       _gdk_quartz_display_before_process_all_updates (GdkDisplay *display);
void       _gdk_quartz_display_after_process_all_updates  (GdkDisplay *display);
void       _gdk_quartz_display_create_window_impl (GdkDisplay    *display,
                                                   GdkWindow     *window,
                                                   GdkWindow     *real_parent,
                                                   GdkScreen     *screen,
                                                   GdkEventMask   event_mask,
                                                   GdkWindowAttr *attributes,
                                                   gint           attributes_mask);

/* Display methods - keymap */
GdkKeymap * _gdk_quartz_display_get_keymap (GdkDisplay *display);

/* Display methods - selection */
gboolean    _gdk_quartz_display_set_selection_owner (GdkDisplay *display,
                                                     GdkWindow  *owner,
                                                     GdkAtom     selection,
                                                     guint32     time,
                                                     gboolean    send_event);
GdkWindow * _gdk_quartz_display_get_selection_owner (GdkDisplay *display,
                                                     GdkAtom     selection);
gint        _gdk_quartz_display_get_selection_property (GdkDisplay     *display,
                                                        GdkWindow      *requestor,
                                                        guchar        **data,
                                                        GdkAtom        *ret_type,
                                                        gint           *ret_format);
void        _gdk_quartz_display_convert_selection      (GdkDisplay     *display,
                                                        GdkWindow      *requestor,
                                                        GdkAtom         selection,
                                                        GdkAtom         target,
                                                        guint32         time);
gint        _gdk_quartz_display_text_property_to_utf8_list (GdkDisplay     *display,
                                                            GdkAtom         encoding,
                                                            gint            format,
                                                            const guchar   *text,
                                                            gint            length,
                                                            gchar        ***list);
gchar *     _gdk_quartz_display_utf8_to_string_target      (GdkDisplay     *displayt,
                                                            const gchar    *str);

/* Screen */
GdkScreen  *_gdk_quartz_screen_new                      (void);
void        _gdk_quartz_screen_update_window_sizes      (GdkScreen *screen);

/* Screen methods - visual */
GdkVisual *   _gdk_quartz_screen_get_rgba_visual            (GdkScreen      *screen);
GdkVisual *   _gdk_quartz_screen_get_system_visual          (GdkScreen      *screen);
gint          _gdk_quartz_screen_visual_get_best_depth      (GdkScreen      *screen);
GdkVisualType _gdk_quartz_screen_visual_get_best_type       (GdkScreen      *screen);
GdkVisual *   _gdk_quartz_screen_get_system_visual          (GdkScreen      *screen);
GdkVisual*    _gdk_quartz_screen_visual_get_best            (GdkScreen      *screen);
GdkVisual*    _gdk_quartz_screen_visual_get_best_with_depth (GdkScreen      *screen,
                                                             gint            depth);
GdkVisual*    _gdk_quartz_screen_visual_get_best_with_type  (GdkScreen      *screen,
                                                             GdkVisualType   visual_type);
GdkVisual*    _gdk_quartz_screen_visual_get_best_with_both  (GdkScreen      *screen,
                                                             gint            depth,
                                                             GdkVisualType   visual_type);
void          _gdk_quartz_screen_query_depths               (GdkScreen      *screen,
                                                             gint          **depths,
                                                             gint           *count);
void          _gdk_quartz_screen_query_visual_types         (GdkScreen      *screen,
                                                             GdkVisualType **visual_types,
                                                             gint           *count);
void          _gdk_quartz_screen_init_visuals               (GdkScreen      *screen);
GList *       _gdk_quartz_screen_list_visuals               (GdkScreen      *screen);

/* Screen methods - events */
void        _gdk_quartz_screen_broadcast_client_message (GdkScreen   *screen,
                                                         GdkEvent    *event);
gboolean    _gdk_quartz_screen_get_setting              (GdkScreen   *screen,
                                                         const gchar *name,
                                                         GValue      *value);


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

void       _gdk_quartz_window_update_position           (GdkWindow    *window);


/* Window methods - testing */
void     _gdk_quartz_window_sync_rendering    (GdkWindow       *window);
gboolean _gdk_quartz_window_simulate_key      (GdkWindow       *window,
                                               gint             x,
                                               gint             y,
                                               guint            keyval,
                                               GdkModifierType  modifiers,
                                               GdkEventType     key_pressrelease);
gboolean _gdk_quartz_window_simulate_button   (GdkWindow       *window,
                                               gint             x,
                                               gint             y,
                                               guint            button,
                                               GdkModifierType  modifiers,
                                               GdkEventType     button_pressrelease);

/* Window methods - property */
gboolean _gdk_quartz_window_get_property      (GdkWindow    *window,
                                               GdkAtom       property,
                                               GdkAtom       type,
                                               gulong        offset,
                                               gulong        length,
                                               gint          pdelete,
                                               GdkAtom      *actual_property_type,
                                               gint         *actual_format_type,
                                               gint         *actual_length,
                                               guchar      **data);
void     _gdk_quartz_window_change_property   (GdkWindow    *window,
                                               GdkAtom       property,
                                               GdkAtom       type,
                                               gint          format,
                                               GdkPropMode   mode,
                                               const guchar *data,
                                               gint          nelements);
void     _gdk_quartz_window_delete_property   (GdkWindow    *window,
                                               GdkAtom       property);


#endif /* __GDK_PRIVATE_QUARTZ_H__ */
