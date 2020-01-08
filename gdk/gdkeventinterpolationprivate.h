/* GDK - The GIMP Drawing Kit
 *
 * gdkeventinterpolationprivate.h: input event interpolation
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

#ifndef __GDK_EVENT_INTERPOLATION_PRIVATE__
#define __GDK_EVENT_INTERPOLATION_PRIVATE__

#include <glib.h>

#include "gdkevents.h"

G_BEGIN_DECLS

/*
 * Relative events interpolaiton
 */

typedef struct _GdkEventInterpolation GdkEventInterpolation;

GdkEventInterpolation * gdk_event_interpolation_new (void);

void gdk_event_interpolation_free (GdkEventInterpolation  *interpolator);

void gdk_event_interpolation_history_push (GdkEventInterpolation  *interpolator,
                                           GdkEvent               *event);

guint gdk_event_interpolation_history_length (GdkEventInterpolation  *interpolator);

void gdk_event_interpolation_history_reset (GdkEventInterpolation  *interpolator);

GdkEvent * gdk_event_interpolation_interpolate_event (GdkEventInterpolation  *interpolator,
                                                      gint64                  frame_time);

void gdk_event_interpolation_set_start_event (GdkEventInterpolation  *interpolator,
                                              GdkEvent               *event);

GdkEvent * gdk_event_interpolation_get_start_event (GdkEventInterpolation  *interpolator);

void gdk_event_interpolation_set_stop_event (GdkEventInterpolation  *interpolator,
                                             GdkEvent               *event);

GdkEvent * gdk_event_interpolation_get_stop_event (GdkEventInterpolation  *interpolator);

guint32 gdk_event_interpolation_newest_event_time (GdkEventInterpolation  *interpolator);

guint32 gdk_event_interpolation_average_event_interval (GdkEventInterpolation  *interpolator);

gboolean gdk_event_interpolation_all_existing_events_emitted (GdkEventInterpolation *interpolator,
                                                              gint64                 interpolation_point);
G_END_DECLS

#endif /* __GDK_EVENT_INTERPOLATION_PRIVATE__ */
