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

void
gdk_profiler_start (void)
{
  char *filename;

  if (writer)
    return;

  sp_clock_init ();

  filename = g_strdup_printf ("gtk.trace.%d", getpid ());
  writer = sp_capture_writer_new (filename, 16*1024);
  g_free (filename);

  atexit (gdk_profiler_stop);
}

void
gdk_profiler_stop (void)
{
  sp_capture_writer_unref (writer);
  writer = NULL;
}

gboolean
gdk_profiler_is_running (void)
{
  return writer != NULL;
}

static void
add_mark (SpCaptureWriter *writer,
          gint64           start,
          guint64          duration,
          const char      *name,
          const char      *message)
{
  sp_capture_writer_add_mark (writer, start, 0, getpid (), duration, "gtk", name, message);
}

void
gdk_profiler_add_frame (GdkFrameTimings *timings)
{
  if (!writer)
    return;

  add_mark (writer, timings->frame_time * 1000, (timings->frame_end_time - timings->frame_time) * 1000, "frame", "");
  if (timings->layout_start_time != 0)
    add_mark (writer, timings->layout_start_time * 1000, (timings->paint_start_time - timings->layout_start_time) * 1000, "layout", "");

  if (timings->paint_start_time != 0)
    add_mark (writer, timings->paint_start_time * 1000, (timings->frame_end_time - timings->paint_start_time) * 1000, "paint", "");

}
