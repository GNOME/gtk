/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2012 Red Hat, Inc.
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

#include "gdkframeclockprivate.h"

G_DEFINE_BOXED_TYPE (GdkFrameTimings, gdk_frame_timings,
                     gdk_frame_timings_ref,
                     gdk_frame_timings_unref)

GdkFrameTimings *
_gdk_frame_timings_new (gint64 frame_counter)
{
  GdkFrameTimings *timings;

  timings = g_slice_new0 (GdkFrameTimings);
  timings->ref_count = 1;
  timings->frame_counter = frame_counter;

  return timings;
}

GdkFrameTimings *
gdk_frame_timings_ref (GdkFrameTimings *timings)
{
  g_return_val_if_fail (timings != NULL, NULL);

  timings->ref_count++;

  return timings;
}

void
gdk_frame_timings_unref (GdkFrameTimings *timings)
{
  g_return_if_fail (timings != NULL);
  g_return_if_fail (timings->ref_count > 0);

  timings->ref_count--;
  if (timings->ref_count == 0)
    {
      g_slice_free (GdkFrameTimings, timings);
    }
}

gint64
gdk_frame_timings_get_frame_counter (GdkFrameTimings *timings)
{
  return timings->frame_counter;
}

gboolean
gdk_frame_timings_get_complete (GdkFrameTimings *timings)
{
  g_return_val_if_fail (timings != NULL, FALSE);

  return timings->complete;
}

gint64
gdk_frame_timings_get_frame_time (GdkFrameTimings *timings)
{
  g_return_val_if_fail (timings != NULL, 0);

  return timings->frame_time;
}

gint64
gdk_frame_timings_get_presentation_time (GdkFrameTimings *timings)
{
  g_return_val_if_fail (timings != NULL, 0);

  return timings->presentation_time;
}

gint64
gdk_frame_timings_get_predicted_presentation_time (GdkFrameTimings *timings)
{
  g_return_val_if_fail (timings != NULL, 0);

  return timings->predicted_presentation_time;
}

gint64
gdk_frame_timings_get_refresh_interval (GdkFrameTimings *timings)
{
  g_return_val_if_fail (timings != NULL, 0);

  return timings->refresh_interval;
}
