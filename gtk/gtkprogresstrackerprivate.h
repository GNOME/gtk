/*
 * Copyright © 2016 Endless Mobile Inc.
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
 *
 * Authors: Matthew Watson <mattdangerw@gmail.com>
 */

#ifndef __GTK_PROGRESS_TRACKER_PRIVATE_H__
#define __GTK_PROGRESS_TRACKER_PRIVATE_H__

#include <glib-object.h>
#include "gtkcsseasevalueprivate.h"

G_BEGIN_DECLS

typedef enum {
  GTK_PROGRESS_STATE_BEFORE,
  GTK_PROGRESS_STATE_DURING,
  GTK_PROGRESS_STATE_AFTER,
} GtkProgressState;

typedef struct _GtkProgressTracker GtkProgressTracker;

struct _GtkProgressTracker
{
  gboolean is_running;
  guint64 last_frame_time;
  guint64 duration;
  gdouble iteration;
  gdouble iteration_count;
};

void                 gtk_progress_tracker_init_copy           (GtkProgressTracker *source,
                                                               GtkProgressTracker *dest);

void                 gtk_progress_tracker_start               (GtkProgressTracker *tracker,
                                                               guint64 duration,
                                                               gint64 delay,
                                                               gdouble iteration_count);

void                 gtk_progress_tracker_finish              (GtkProgressTracker *tracker);

void                 gtk_progress_tracker_advance_frame       (GtkProgressTracker *tracker,
                                                               guint64 frame_time);

void                 gtk_progress_tracker_skip_frame          (GtkProgressTracker *tracker,
                                                               guint64 frame_time);

GtkProgressState     gtk_progress_tracker_get_state           (GtkProgressTracker *tracker);

gdouble              gtk_progress_tracker_get_iteration       (GtkProgressTracker *tracker);

guint64              gtk_progress_tracker_get_iteration_cycle (GtkProgressTracker *tracker);

gdouble              gtk_progress_tracker_get_progress        (GtkProgressTracker *tracker,
                                                               gboolean reverse);

gdouble              gtk_progress_tracker_get_ease_out_cubic  (GtkProgressTracker *tracker,
                                                               gboolean reverse);

G_END_DECLS

#endif /* __GTK_PROGRESS_TRACKER_PRIVATE_H__ */
