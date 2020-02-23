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

#include "gdkdisplayprivate.h"
#include "gdkkeys.h"
#include "gdksurface.h"
#include "gdkinternals.h"

G_BEGIN_DECLS


struct _GdkQuartzDisplay
{
  GdkDisplay parent_instance;
  GPtrArray *monitors;
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
void       _gdk_quartz_display_create_surface_impl (GdkDisplay    *display,
                                                   GdkSurface     *window,
                                                   GdkSurface     *real_parent,
                                                   GdkSurfaceAttr *attributes);

/* Display methods - keymap */
GdkKeymap * _gdk_quartz_display_get_keymap (GdkDisplay *display);


G_END_DECLS

#endif  /* __GDK_QUARTZ_DISPLAY__ */

