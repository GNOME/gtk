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

#include "config.h"

#include "gtksequencetrackerprivate.h"

#include <math.h>

#define GTK_SEQUENCE_TRACKER_HISTORY_SIZE 8

typedef struct _GtkSequenceTrackerClass GtkSequenceTrackerClass;
struct _GtkSequenceTrackerClass {
  gboolean      (* update)              (GtkSequenceTracker *tracker,
                                         GdkEvent           *event,
                                         double             *x,
                                         double             *y);
};

struct _GtkSequenceTracker {
  GtkSequenceTrackerClass *klass;

  struct {
    guint                  time;
    GtkMovementDirection   dir;
    double                 x;           /* x position in pixels - will be compared to start_x */
    double                 y;           /* y position in pixels - will be compared to start_y */
  }                        history[GTK_SEQUENCE_TRACKER_HISTORY_SIZE];
  guint                    history_index; /* current item */

  GdkEventSequence        *sequence;    /* sequence we're tracking */
  double                   start_x;     /* NOT screen location, but in device coordinates */
  double                   start_y;     /* NOT screen location, but in device coordinates */
};

/* MOUSE */

/* FIXME */

/* TOUCHSCREEN */

/* FIXME */

/* TOUCHPAD */

static gboolean
gtk_sequence_tracker_touchpad_update (GtkSequenceTracker *tracker,
                                      GdkEvent           *event,
                                      double             *x,
                                      double             *y)
{
  GdkDevice *device = gdk_event_get_device (event);

  switch (event->type)
  {
    case GDK_TOUCH_BEGIN:
      tracker->sequence = gdk_event_get_event_sequence (event);
      /* fall through */
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
      if (tracker->sequence != gdk_event_get_event_sequence (event))
        return FALSE;

      if (gdk_device_get_axis (device,
                               event->touch.axes,
                               GDK_AXIS_X_RELATIVE,
                               x))
        {
          *x *= gdk_screen_get_width (gdk_display_get_default_screen (gdk_device_get_display (device)));
        }
      else
        {
          g_warning ("Could not query X value");
          *x = 0;
        }

      if (gdk_device_get_axis (device,
                               event->touch.axes,
                               GDK_AXIS_Y_RELATIVE,
                               y))
        {
          *y *= gdk_screen_get_height (gdk_display_get_default_screen (gdk_device_get_display (device)));
        }
      else
        {
          g_warning ("Could not query Y value");
          *y = 0;
        }

      return TRUE;
    default:
      return FALSE;
  }
}

GtkSequenceTrackerClass GTK_SEQUENCE_TRACKER_CLASS_TOUCHPAD = {
  gtk_sequence_tracker_touchpad_update
};

/* API */

GtkSequenceTracker *
_gtk_sequence_tracker_new (GdkEvent *event)
{
  GtkSequenceTrackerClass *klass;
  GtkSequenceTracker *tracker;
  guint i, time;

  switch (event->type)
    {
      case GDK_TOUCH_BEGIN:
        if (gdk_device_get_source (gdk_event_get_source_device (event)) == GDK_SOURCE_TOUCHPAD)
          klass = &GTK_SEQUENCE_TRACKER_CLASS_TOUCHPAD;
        else
          return NULL;
        break;
      default:
        return NULL;
    }

  tracker = g_slice_new0 (GtkSequenceTracker);
  tracker->klass = klass;

  if (!tracker->klass->update (tracker, event, &tracker->start_x, &tracker->start_y))
    {
      g_assert_not_reached ();
      return NULL;
    }
  time = gdk_event_get_time (event);

  for (i = 0; i < GTK_SEQUENCE_TRACKER_HISTORY_SIZE; i++)
    {
      tracker->history[i].time = time;
      tracker->history[i].x = tracker->start_x;
      tracker->history[i].y = tracker->start_y;
      tracker->history[i].dir = GTK_DIR_ANY;
    }
  tracker->history_index = 0;

  return tracker;
}

void
_gtk_sequence_tracker_free (GtkSequenceTracker *tracker)
{
  g_return_if_fail (tracker != NULL);

  g_slice_free (GtkSequenceTracker, tracker);
}

static GtkMovementDirection
gtk_sequence_tracker_compute_direction (double dx,
                                        double dy)
{
  double r;
  int i1, i2;

  if (fabs (dx) < 2 && fabs (dy) < 2)
    {
      GtkMovementDirection dir = GTK_DIR_ANY;

      if (dx <= 1)
        dir &= GTK_DIR_SOUTH | GTK_DIR_SOUTH_WEST | GTK_DIR_WEST | GTK_DIR_NORTH_WEST | GTK_DIR_NORTH;
      else if (dx >= 1)
        dir &= GTK_DIR_SOUTH | GTK_DIR_SOUTH_EAST | GTK_DIR_EAST | GTK_DIR_NORTH_EAST | GTK_DIR_NORTH;

      if (dy <= 1)
        dir &= GTK_DIR_WEST | GTK_DIR_NORTH_WEST | GTK_DIR_NORTH | GTK_DIR_NORTH_EAST | GTK_DIR_EAST;
      else
        dir &= GTK_DIR_WEST | GTK_DIR_SOUTH_WEST | GTK_DIR_SOUTH | GTK_DIR_SOUTH_EAST | GTK_DIR_EAST;
      
      return dir;
  }
  
  r = atan2(dy, dx);

  /* Add 360° to avoid r become negative since C has no well-defined
   * modulo for such cases. */
  r += 2 * G_PI;
  
  /* Divide by 45° to get the octant number,  e.g.
   *          0 <= r <= 1 is [0-45]°
   *          1 <= r <= 2 is [45-90]°
   *          etc. */
  r /= G_PI / 4;

  /* This intends to flag 2 directions (45 degrees),
   * except on very well-aligned coordinates. */
  i1 = (int) (r + 0.1) % 8;
  i2 = (int) (r + 0.9) % 8;
  if (i1 < 0 || i1 > 7 || i2 < 0 || i2 > 7)
    return GTK_DIR_ANY;
  
  return (1 << i1 | 1 << i2);
}

gboolean
_gtk_sequence_tracker_update (GtkSequenceTracker *tracker,
                              GdkEvent           *event)
{
  double x, y, dx, dy;

  g_return_val_if_fail (tracker != NULL, FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (!tracker->klass->update (tracker, event, &x, &y))
    return FALSE;

  dx = x - tracker->history[tracker->history_index].x;
  dy = y - tracker->history[tracker->history_index].y;
  
  tracker->history_index = (tracker->history_index + 1) % GTK_SEQUENCE_TRACKER_HISTORY_SIZE;
  tracker->history[tracker->history_index].time = gdk_event_get_time (event);
  tracker->history[tracker->history_index].x = x;
  tracker->history[tracker->history_index].y = y;
  tracker->history[tracker->history_index].dir = gtk_sequence_tracker_compute_direction (dx, dy);

  return TRUE;
}

double
_gtk_sequence_tracker_get_x_offset (GtkSequenceTracker *tracker)
{
  g_return_val_if_fail (tracker != NULL, 0);

  return tracker->history[tracker->history_index].x
    - tracker->start_x;
}

double
_gtk_sequence_tracker_get_y_offset (GtkSequenceTracker *tracker)
{
  g_return_val_if_fail (tracker != NULL, 0);

  return tracker->history[tracker->history_index].y
    - tracker->start_y;
}

GtkMovementDirection
_gtk_sequence_tracker_get_direction (GtkSequenceTracker *tracker)
{
  g_return_val_if_fail (tracker != NULL, GTK_DIR_ANY);

  return tracker->history[tracker->history_index].dir;
}

void
_gtk_sequence_tracker_compute_distance (GtkSequenceTracker *from,
                                        GtkSequenceTracker *to,
                                        double             *x,
                                        double             *y)
{
  g_return_if_fail (from != NULL);
  g_return_if_fail (to != NULL);
  g_return_if_fail (from->klass == to->klass);
  /* XXX: compare devices here? */

  if (x)
    *x = from->history[from->history_index].x - to->history[to->history_index].x;
  if (y)
    *y = from->history[from->history_index].y - to->history[to->history_index].y;
}

