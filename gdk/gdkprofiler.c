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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "gdkversionmacros.h"
#include "gdkprofiler.h"
#include "gdkprofilerprivate.h"
#include "gdkframeclockprivate.h"

#ifndef G_OS_WIN32

#include "capture/sp-capture-writer.h"

static SpCaptureWriter *writer = NULL;
static gboolean running = FALSE;

static void
profiler_stop (void)
{
  if (writer)
    sp_capture_writer_unref (writer);
}

void
gdk_profiler_start (int fd)
{
  if (writer)
    return;

  sp_clock_init ();

  if (fd == -1)
    {
      gchar *filename;

      filename = g_strdup_printf ("gtk.%d.syscap", getpid ());
      g_print ("Writing profiling data to %s\n", filename);
      writer = sp_capture_writer_new (filename, 16*1024);
      g_free (filename);
    }
  else if (fd > 2)
    writer = sp_capture_writer_new_from_fd (fd, 16*1024);

  if (writer)
    running = TRUE;

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

static guint
define_counter (const char *name,
                const char *description,
                int         type)
{
  SpCaptureCounter counter;

  if (!writer)
    return 0;

  counter.id = (guint) sp_capture_writer_request_counter (writer, 1);
  counter.type = type;
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

guint
gdk_profiler_define_counter (const char *name,
                             const char *description)
{
  return define_counter (name, description, SP_CAPTURE_COUNTER_DOUBLE);
}

guint
gdk_profiler_define_int_counter (const char *name,
                                 const char *description)
{
  return define_counter (name, description, SP_CAPTURE_COUNTER_INT64);
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

void
gdk_profiler_set_int_counter (guint  id,
                              gint64 time,
                              gint64 val)
{
  SpCaptureCounterValue value;

  if (!running)
    return;

  value.v64 = val;
  sp_capture_writer_set_counters (writer,
                                  time,
                                  -1, getpid (),
                                  &id, &value, 1);
}

#else

void
gdk_profiler_start (int fd)
{
}

void
gdk_profiler_stop (void)
{
}

gboolean
gdk_profiler_is_running (void)
{
  return FALSE;
}

void
gdk_profiler_add_mark (gint64      start,
                       guint64     duration,
                       const char *name,
                       const char *message)
{
}

guint
gdk_profiler_define_counter (const char *name,
                             const char *description)
{
 return 0;
}

void
gdk_profiler_set_counter (guint  id,
                          gint64 time,
                          double value)
{
}

guint
gdk_profiler_define_int_counter (const char *name,
                                 const char *description)
{
  return 0;
}

void
gdk_profiler_set_int_counter (guint  id,
                              gint64 time,
                              gint64 value)
{
}

#endif /* G_OS_WIN32 */

/**
 * gdk_profiler_set_mark:
 * @duration: the duration of the mark, or 0
 * @name: the name of the mark (up to 40 characters)
 * @message: (optional): the message of the mark
 *
 * Insert a mark into the profiling data if we
 * are currently profiling.
 * This information will show up in tools like sysprof
 * or GNOME Builder when viewing the profiling data.
 * It can be used to mark interesting regions in the
 * captured data.
 *
 * If the duration is non-zero, the mark applies to
 * the timespan from @duration microseconds in the
 * past to the current time. To mark just a point in
 * time, pass 0 as duration.
 *
 * @name should be a short string, and @message is optional.
 */
void
gdk_profiler_set_mark (guint64     duration,
                       const char *name,
                       const char *message)
{
  guint64 start;

  start = g_get_monotonic_time () - duration;
  gdk_profiler_add_mark (start, duration, name, message);
}
