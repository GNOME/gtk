/* GDK - The GIMP Drawing Kit
 *
 * gdkeventhistoryprivate.h: input event history
 *
 * Copyright © 2019 Yariv Barkan
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

#ifndef __GDK_EVENT_HISTORY_PRIVATE__
#define __GDK_EVENT_HISTORY_PRIVATE__

#include <glib.h>

#include "gdkevents.h"

G_BEGIN_DECLS

/*
 * History of a single event sequence
 */

typedef struct _GdkEventHistory GdkEventHistory;

GdkEventHistory * gdk_event_history_new (void);

void gdk_event_history_free (GdkEventHistory *history);

void gdk_event_history_push (GdkEventHistory *history,
                             GdkEvent        *event);

guint gdk_event_history_length (GdkEventHistory *history);

void gdk_event_history_reset (GdkEventHistory *history);

GdkEvent * gdk_event_history_interpolate_event (GdkEventHistory *history,
                                                gint64           frame_time);

void gdk_event_history_set_start_event (GdkEventHistory *history,
                                        GdkEvent        *event);

GdkEvent * gdk_event_history_get_start_event (GdkEventHistory *history);

void gdk_event_history_set_stop_event (GdkEventHistory *history,
                                       GdkEvent        *event);

GdkEvent * gdk_event_history_get_stop_event (GdkEventHistory *history);

guint32 gdk_event_history_newest_event_time (GdkEventHistory *history);

guint32 gdk_event_history_average_event_interval (GdkEventHistory *history);

gboolean gdk_event_history_all_existing_events_emitted (GdkEventHistory *history,
                                                        gint64           interpolation_point);

G_END_DECLS

#endif /* __GDK_EVENT_HISTORY_PRIVATE__ */
