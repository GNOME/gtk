/*
 * gdkdisplay-quartz.h
 *
 * Copyright 2017 Tom Schoonjans 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_QUARTZ_DISPLAY__
#define __GDK_QUARTZ_DISPLAY__

#include <AppKit/AppKit.h>

#include "gdkdisplayprivate.h"
#include "gdkkeys.h"
#include "gdkwindow.h"
#include "gdkinternals.h"
#include "gdkmain.h"

G_BEGIN_DECLS


struct _GdkQuartzDisplay
{
  GdkDisplay parent_instance;
  NSRect geometry; /* In AppKit coordinates. */
  NSSize size; /* Aggregate size of displays in millimeters. */
  GPtrArray *monitors;
  /* This structure is not allocated. It points to an embedded
   * GList in the GdkWindow. */
  GSList   *windows_awaiting_frame;
  GSource *frame_source;
};

struct _GdkQuartzDisplayClass
{
  GdkDisplayClass parent_class;
};

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
G_END_DECLS

#endif  /* __GDK_QUARTZ_DISPLAY__ */
