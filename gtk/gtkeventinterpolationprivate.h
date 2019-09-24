/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2019 Yariv Barkan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_EVENT_INTERPOLATION_H__
#define __GTK_EVENT_INTERPOLATION_H__

#include <glib.h>

G_BEGIN_DECLS

/**
 * Absolute events interpolaiton
 */

typedef struct _GtkAbsoluteEventInterpolation GtkAbsoluteEventInterpolation;

typedef struct _AbsoluteEventHistoryElement
{
  guint32 evtime;
  guint modifier_state;
  gdouble x;
  gdouble y;
} AbsoluteEventHistoryElement;

GtkAbsoluteEventInterpolation * gtk_absolute_event_interpolation_new (void);

void gtk_absolute_event_interpolation_free (GtkAbsoluteEventInterpolation  *interpolator);

void gtk_absolute_event_interpolation_history_push (GtkAbsoluteEventInterpolation  *interpolator,
                                                    guint32                         evtime,
                                                    guint                           modifier_state,
                                                    gdouble                         x,
                                                    gdouble                         y);

void gtk_absolute_event_interpolation_history_reset (GtkAbsoluteEventInterpolation  *interpolator);

void gtk_absolute_event_interpolation_interpolate_event (GtkAbsoluteEventInterpolation  *interpolator,
                                                         gint64                          frame_time,
                                                         AbsoluteEventHistoryElement    *interpolated_item);

guint32 gtk_absolute_event_interpolation_offset_from_latest (GtkAbsoluteEventInterpolation  *interpolator,
                                                            gint64                          frame_time);

/**
 * Relative events interpolaiton
 */

typedef struct _GtkRelativeEventInterpolation GtkRelativeEventInterpolation;

GtkRelativeEventInterpolation * gtk_relative_event_interpolation_new (void);

void gtk_relative_event_interpolation_free (GtkRelativeEventInterpolation  *interpolator);

void gtk_relative_event_interpolation_history_push (GtkRelativeEventInterpolation  *interpolator,
                                                    guint32                         evtime,
                                                    guint                           modifier_state,
                                                    gdouble                         delta_x,
                                                    gdouble                         delta_y);

void gtk_relative_event_interpolation_history_reset (GtkRelativeEventInterpolation  *interpolator);

void gtk_relative_event_interpolation_interpolate_event (GtkRelativeEventInterpolation  *interpolator,
                                                         gint64                          frame_time,
                                                         guint                          *modifier_state,
                                                         gdouble                        *delta_x,
                                                         gdouble                        *delta_y);

guint32 gtk_relative_event_interpolation_offset_from_latest (GtkRelativeEventInterpolation  *interpolator,
                                                            gint64                          frame_time);

G_END_DECLS

#endif /* __GTK_EVENT_INTERPOLATION_H__ */
