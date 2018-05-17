/* GDK - The GIMP Drawing Kit
 *
 * gdkprofiler.c: A simple profiler
 *
 * Copyright © 2018 Matthias Clasen
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

#include "config.h"

#include <sys/types.h>
#include <unistd.h>

#include "capture/sp-capture-writer.h"
#include "gdkprofiler.h"
#include "gdkframeclockprivate.h"

static SpCaptureWriter *writer = NULL;
static gboolean running = FALSE;
static int fps_counter = 0;

static void
profiler_stop (void)
{
  if (writer)
    sp_capture_writer_unref (writer);
}

void
gdk_profiler_start (void)
{
  char *filename;
  SpCaptureCounter counter;

  running = TRUE;

  if (writer)
    return;

  sp_clock_init ();

  filename = g_strdup_printf ("gtk.trace.%d", getpid ());
  writer = sp_capture_writer_new (filename, 16*1024);
  g_free (filename);

  fps_counter = sp_capture_writer_request_counter (writer, 1);
  counter.id = fps_counter;
  counter.type = SP_CAPTURE_COUNTER_DOUBLE;
  counter.value.vdbl = 0;
  g_strlcpy (counter.category, "gtk", sizeof counter.category);
  g_strlcpy (counter.name, "fps", sizeof counter.name);
  g_strlcpy (counter.description, "Frames per second", sizeof counter.name);

  sp_capture_writer_define_counters (writer,
                                     SP_CAPTURE_CURRENT_TIME,
                                     -1,
                                     getpid (),
                                     &counter,
                                     1);

  atexit (profiler_stop);
}

void
gdk_profiler_stop (void)
{
  running = FALSE;
}

gboolean
gdk_profiler_is_running (void)
{
  return running;
}

void
gdk_profiler_add_mark (gint64      start,
                       guint64     duration,
                       const char *name,
                       const char *message)
{
  if (!running)
    return;

  sp_capture_writer_add_mark (writer,
                              start,
                              -1, getpid (),
                              duration,
                              "gtk", name, message);
}

static gint64
guess_refresh_interval (GdkFrameClock *frame_clock)
{
  gint64 interval;
  gint64 i;

  interval = G_MAXINT64;

  for (i = gdk_frame_clock_get_history_start (frame_clock);
       i < gdk_frame_clock_get_frame_counter (frame_clock);
       i++)
    {
      GdkFrameTimings *t, *before;
      gint64 ts, before_ts;

      t = gdk_frame_clock_get_timings (frame_clock, i);
      before = gdk_frame_clock_get_timings (frame_clock, i - 1);
      if (t == NULL || before == NULL)
        continue;

      ts = gdk_frame_timings_get_frame_time (t);
      before_ts = gdk_frame_timings_get_frame_time (before);
      if (ts == 0 || before_ts == 0)
        continue;

      interval = MIN (interval, ts - before_ts);
    }

  if (interval == G_MAXINT64)
    return 0;

  return interval;
}

static double
frame_clock_get_fps (GdkFrameClock *frame_clock)
{
  GdkFrameTimings *start, *end;
  gint64 start_counter, end_counter;
  gint64 start_timestamp, end_timestamp;
  gint64 interval;

  start_counter = gdk_frame_clock_get_history_start (frame_clock);
  end_counter = gdk_frame_clock_get_frame_counter (frame_clock);
  start = gdk_frame_clock_get_timings (frame_clock, start_counter);
  for (end = gdk_frame_clock_get_timings (frame_clock, end_counter);
       end_counter > start_counter && end != NULL && !gdk_frame_timings_get_complete (end);
       end = gdk_frame_clock_get_timings (frame_clock, end_counter))
    end_counter--;
  if (end_counter - start_counter < 4)
    return 0.0;

  start_timestamp = gdk_frame_timings_get_presentation_time (start);
  end_timestamp = gdk_frame_timings_get_presentation_time (end);
  if (start_timestamp == 0 || end_timestamp == 0)
    {
      start_timestamp = gdk_frame_timings_get_frame_time (start);
      end_timestamp = gdk_frame_timings_get_frame_time (end);
    }

  interval = gdk_frame_timings_get_refresh_interval (end);
  if (interval == 0)
    {
      interval = guess_refresh_interval (frame_clock);
      if (interval == 0)
        return 0.0;
    }

  return ((double) end_counter - start_counter) * G_USEC_PER_SEC / (end_timestamp - start_timestamp);
}

void
gdk_profiler_add_frame (GdkFrameClock   *clock,
                        GdkFrameTimings *timings)
{
  SpCaptureCounterValue value;

  if (!running)
    return;

  sp_capture_writer_add_mark (writer,
                              timings->frame_time * 1000,
                              -1, getpid (),
                              (timings->frame_end_time - timings->frame_time) * 1000,
                              "gtk", "frame", "");
  if (timings->layout_start_time != 0)
    sp_capture_writer_add_mark (writer,
                                timings->layout_start_time * 1000,
                                -1, getpid (),
                                (timings->paint_start_time - timings->layout_start_time) * 1000,
                                "gtk", "layout", "");

  if (timings->paint_start_time != 0)
    sp_capture_writer_add_mark (writer,
                                timings->paint_start_time * 1000,
                                -1, getpid (),
                                (timings->frame_end_time - timings->paint_start_time) * 1000,
                                "gtk", "paint", "");

  value.vdbl = frame_clock_get_fps (clock);
  sp_capture_writer_set_counters (writer,
                                  timings->frame_end_time,
                                  -1, getpid (),
                                  &fps_counter, &value, 1);
}
