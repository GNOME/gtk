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

/**
 * Absolute events interpolaiton
 */

typedef struct _GdkAbsoluteEventInterpolation GdkAbsoluteEventInterpolation;

GdkAbsoluteEventInterpolation * gdk_absolute_event_interpolation_new (void);

void gdk_absolute_event_interpolation_free (GdkAbsoluteEventInterpolation  *interpolator);

void gdk_absolute_event_interpolation_history_push (GdkAbsoluteEventInterpolation  *interpolator,
                                                    GdkEvent                       *new_item);

guint gdk_absolute_event_interpolation_history_length (GdkAbsoluteEventInterpolation  *interpolator);

void gdk_absolute_event_interpolation_history_reset (GdkAbsoluteEventInterpolation  *interpolator);

GdkEvent * gdk_absolute_event_interpolation_interpolate_event (GdkAbsoluteEventInterpolation  *interpolator,
                                                               gint64                          frame_time);

guint32 gdk_absolute_event_interpolation_offset_from_latest (GdkAbsoluteEventInterpolation  *interpolator,
                                                             gint64                          frame_time);

/**
 * Relative events interpolaiton
 */

typedef struct _GdkRelativeEventInterpolation GdkRelativeEventInterpolation;

GdkRelativeEventInterpolation * gdk_relative_event_interpolation_new (void);

void gdk_relative_event_interpolation_free (GdkRelativeEventInterpolation  *interpolator);

void gdk_relative_event_interpolation_history_push (GdkRelativeEventInterpolation  *interpolator,
                                                    GdkEvent                       *event);

guint gdk_relative_event_interpolation_history_length (GdkRelativeEventInterpolation  *interpolator);

void gdk_relative_event_interpolation_history_reset (GdkRelativeEventInterpolation  *interpolator);

GdkEvent * gdk_relative_event_interpolation_interpolate_event (GdkRelativeEventInterpolation  *interpolator,
                                                               gint64                          frame_time);

guint32 gdk_relative_event_interpolation_offset_from_latest (GdkRelativeEventInterpolation  *interpolator,
                                                             gint64                          frame_time);

G_END_DECLS

#endif /* __GDK_EVENT_INTERPOLATION_PRIVATE__ */
