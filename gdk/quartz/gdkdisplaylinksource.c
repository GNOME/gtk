/* gdkdisplaylinksource.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
 *
 * Authors:
 *   Christian Hergert <christian@hergert.me>
 */

#include "config.h"

#include <mach/mach_time.h>

#include "gdkinternal-quartz.h"
#include "gdkdisplaylinksource.h"

static gint64 host_to_frame_clock_time (gint64 host_time);

static gboolean
gdk_display_link_source_prepare (GSource *source,
                                 gint    *timeout_)
{
  GdkDisplayLinkSource *impl = (GdkDisplayLinkSource *)source;
  gint64 now;

  now = g_source_get_time (source);

  if (now < impl->presentation_time)
    *timeout_ = (impl->presentation_time - now) / 1000L;
  else
    *timeout_ = -1;

  return impl->needs_dispatch;
}

static gboolean
gdk_display_link_source_check (GSource *source)
{
  GdkDisplayLinkSource *impl = (GdkDisplayLinkSource *)source;
  return impl->needs_dispatch;
}

static gboolean
gdk_display_link_source_dispatch (GSource     *source,
                                  GSourceFunc  callback,
                                  gpointer     user_data)
{
  GdkDisplayLinkSource *impl = (GdkDisplayLinkSource *)source;
  gboolean ret = G_SOURCE_CONTINUE;

  impl->needs_dispatch = FALSE;

  if (callback != NULL)
    ret = callback (user_data);

  return ret;
}

static void
gdk_display_link_source_finalize (GSource *source)
{
  GdkDisplayLinkSource *impl = (GdkDisplayLinkSource *)source;

  CVDisplayLinkStop (impl->display_link);
  CVDisplayLinkRelease (impl->display_link);
}

static GSourceFuncs gdk_display_link_source_funcs = {
  gdk_display_link_source_prepare,
  gdk_display_link_source_check,
  gdk_display_link_source_dispatch,
  gdk_display_link_source_finalize
};

void
gdk_display_link_source_pause (GdkDisplayLinkSource *source)
{
  CVDisplayLinkStop (source->display_link);
}

void
gdk_display_link_source_unpause (GdkDisplayLinkSource *source)
{
  CVDisplayLinkStart (source->display_link);
}

static CVReturn
gdk_display_link_source_frame_cb (CVDisplayLinkRef   display_link,
                                  const CVTimeStamp *inNow,
                                  const CVTimeStamp *inOutputTime,
                                  CVOptionFlags      flagsIn,
                                  CVOptionFlags     *flagsOut,
                                  void              *user_data)
{
  GdkDisplayLinkSource *impl = user_data;
  gint64 presentation_time;
  gboolean needs_wakeup;

  needs_wakeup = !g_atomic_int_get (&impl->needs_dispatch);

  presentation_time = host_to_frame_clock_time (inOutputTime->hostTime);

  impl->presentation_time = presentation_time;
  impl->needs_dispatch = TRUE;

  if (needs_wakeup)
     {
      NSEvent *event;

      /* Post a message so we'll break out of the message loop.
       *
       * We don't use g_main_context_wakeup() here because that
       * would result in sending a message to the pipe(2) fd in
       * the select thread which would then send this message as
       * well. Lots of extra work.
       */
      event = [NSEvent otherEventWithType: NSEventTypeApplicationDefined
                                 location: NSZeroPoint
                            modifierFlags: 0
                                timestamp: 0
                             windowNumber: 0
                                  context: nil
                                  subtype: GDK_QUARTZ_EVENT_SUBTYPE_EVENTLOOP
                                    data1: 0
                                    data2: 0];

      [NSApp postEvent:event atStart:YES];
     }

  return kCVReturnSuccess;
}

/**
 * gdk_display_link_source_new:
 *
 * Creates a new #GSource that will activate the dispatch function upon
 * notification from a CVDisplayLink that a new frame should be drawn.
 *
 * Effort is made to keep the transition from the high-priority
 * CVDisplayLink thread into this GSource lightweight. However, this is
 * somewhat non-ideal since the best case would be to do the drawing
 * from the high-priority thread.
 *
 * Returns: (transfer full): A newly created #GSource.
 */
GSource *
gdk_display_link_source_new (void)
{
  GdkDisplayLinkSource *impl;
  GSource *source;
  CVReturn ret;
  double period;

  source = g_source_new (&gdk_display_link_source_funcs, sizeof *impl);
  impl = (GdkDisplayLinkSource *)source;

  /*
   * Create our link based on currently connected displays.
   * If there are multiple displays, this will be something that tries
   * to work for all of them. In the future, we may want to explore multiple
   * links based on the connected displays.
   */
  ret = CVDisplayLinkCreateWithActiveCGDisplays (&impl->display_link);
  if (ret != kCVReturnSuccess)
    {
      g_warning ("Failed to initialize CVDisplayLink!");
      return source;
    }

  /*
   * Determine our nominal period between frames.
   */
  period = CVDisplayLinkGetActualOutputVideoRefreshPeriod (impl->display_link);
  if (period == 0.0)
    period = 1.0 / 60.0;
  impl->refresh_interval = period * 1000000L;

  /*
   * Wire up our callback to be executed within the high-priority thread.
   */
  CVDisplayLinkSetOutputCallback (impl->display_link,
                                  gdk_display_link_source_frame_cb,
                                  source);

  g_source_set_name (source, "[gdk] quartz frame clock");

  return source;
}

static gint64
host_to_frame_clock_time (gint64 host_time)
{
  static mach_timebase_info_data_t timebase_info;

  /*
   * NOTE:
   *
   * This code is taken from GLib to match g_get_monotonic_time().
   */
  if (G_UNLIKELY (timebase_info.denom == 0))
    {
      /* This is a fraction that we must use to scale
       * mach_absolute_time() by in order to reach nanoseconds.
       *
       * We've only ever observed this to be 1/1, but maybe it could be
       * 1000/1 if mach time is microseconds already, or 1/1000 if
       * picoseconds.  Try to deal nicely with that.
       */
      mach_timebase_info (&timebase_info);

      /* We actually want microseconds... */
      if (timebase_info.numer % 1000 == 0)
        timebase_info.numer /= 1000;
      else
        timebase_info.denom *= 1000;

      /* We want to make the numer 1 to avoid having to multiply... */
      if (timebase_info.denom % timebase_info.numer == 0)
        {
          timebase_info.denom /= timebase_info.numer;
          timebase_info.numer = 1;
        }
      else
        {
          /* We could just multiply by timebase_info.numer below, but why
           * bother for a case that may never actually exist...
           *
           * Plus -- performing the multiplication would risk integer
           * overflow.  If we ever actually end up in this situation, we
           * should more carefully evaluate the correct course of action.
           */
          mach_timebase_info (&timebase_info); /* Get a fresh copy for a better message */
          g_error ("Got weird mach timebase info of %d/%d.  Please file a bug against GLib.",
                   timebase_info.numer, timebase_info.denom);
        }
    }

  return host_time / timebase_info.denom;
}
