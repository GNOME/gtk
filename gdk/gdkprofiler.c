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
static guint fps_counter = 0;

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

  filename = g_strdup_printf ("gtk.%d.syscap", getpid ());
  writer = sp_capture_writer_new (filename, 16*1024);
  g_free (filename);

  fps_counter = (guint) sp_capture_writer_request_counter (writer, 1);
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

guint
gdk_profiler_define_counter (const char *name,
                             const char *description)
{
  SpCaptureCounter counter;

  if (!writer)
    return 0;

  counter.id = (guint) sp_capture_writer_request_counter (writer, 1);
  counter.type = SP_CAPTURE_COUNTER_DOUBLE;
  counter.value.vdbl = 0;
  g_strlcpy (counter.category, "gtk", sizeof counter.category);
  g_strlcpy (counter.name, name, sizeof counter.name);
  g_strlcpy (counter.description, description, sizeof counter.name);

  sp_capture_writer_define_counters (writer,
                                     SP_CAPTURE_CURRENT_TIME,
                                     -1,
                                     getpid (),
                                     &counter,
                                     1);

  return counter.id;
}

void
gdk_profiler_set_counter (guint  id,
                          gint64 time,
                          double val)
{
  SpCaptureCounterValue value;

  if (!running)
    return;

  value.vdbl = val;
  sp_capture_writer_set_counters (writer,
                                  time,
                                  -1, getpid (),
                                  &id, &value, 1);
}

