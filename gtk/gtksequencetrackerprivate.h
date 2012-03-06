/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Benjamin Otte <otte@gnome.org>
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

#ifndef __GTK_SEQUENCE_TRACKER_PRIVATE_H__
#define __GTK_SEQUENCE_TRACKER_PRIVATE_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef enum {
  GTK_DIR_EAST       = (1 << 0),
  GTK_DIR_SOUTH_EAST = (1 << 1),
  GTK_DIR_SOUTH      = (1 << 2),
  GTK_DIR_SOUTH_WEST = (1 << 3),
  GTK_DIR_WEST       = (1 << 4),
  GTK_DIR_NORTH_WEST = (1 << 5),
  GTK_DIR_NORTH      = (1 << 6),
  GTK_DIR_NORTH_EAST = (1 << 7),
  GTK_DIR_ANY        = (1 << 8) - 1,
} GtkMovementDirection;

typedef struct _GtkSequenceTracker GtkSequenceTracker;

GtkSequenceTracker *  _gtk_sequence_tracker_new                 (GdkEvent           *event);
void                  _gtk_sequence_tracker_free                (GtkSequenceTracker *tracker);

gboolean              _gtk_sequence_tracker_update              (GtkSequenceTracker *tracker,
                                                                 GdkEvent           *event);

double                _gtk_sequence_tracker_get_x_offset        (GtkSequenceTracker *tracker);
double                _gtk_sequence_tracker_get_y_offset        (GtkSequenceTracker *tracker);

GtkMovementDirection  _gtk_sequence_tracker_get_direction       (GtkSequenceTracker *tracker);

void                  _gtk_sequence_tracker_compute_distance    (GtkSequenceTracker *from,
                                                                 GtkSequenceTracker *to,
                                                                 double             *x,
                                                                 double             *y);

G_END_DECLS

#endif /* __GTK_SEQUENCE_TRACKER_PRIVATE_H__ */
