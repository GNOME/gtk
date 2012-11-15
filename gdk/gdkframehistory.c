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

#include "gdkframehistory.h"
#include "gdkinternals.h"

#define FRAME_HISTORY_MAX_LENGTH 16

struct _GdkFrameHistory
{
  GObject parent_instance;

  gint64 frame_counter;
  gint n_timings;
  gint current;
  GdkFrameTimings *timings[FRAME_HISTORY_MAX_LENGTH];
};

struct _GdkFrameHistoryClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GdkFrameHistory, gdk_frame_history, G_TYPE_OBJECT)

static void
gdk_frame_history_finalize (GObject *object)
{
  GdkFrameHistory *history = GDK_FRAME_HISTORY (object);
  int i;

  for (i = 0; i < FRAME_HISTORY_MAX_LENGTH; i++)
    if (history->timings[i] != 0)
      gdk_frame_timings_unref (history->timings[i]);

  G_OBJECT_CLASS (gdk_frame_history_parent_class)->finalize (object);
}

static void
gdk_frame_history_class_init (GdkFrameHistoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_frame_history_finalize;
}

static void
gdk_frame_history_init (GdkFrameHistory *history)
{
  history->frame_counter = -1;
  history->current = FRAME_HISTORY_MAX_LENGTH - 1;
}

GdkFrameHistory *
gdk_frame_history_new (void)
{
  return g_object_new (GDK_TYPE_FRAME_HISTORY, NULL);
}

gint64
gdk_frame_history_get_frame_counter (GdkFrameHistory *history)
{
  g_return_val_if_fail (GDK_IS_FRAME_HISTORY (history), 0);

  return history->frame_counter;
}

gint64
gdk_frame_history_get_start (GdkFrameHistory *history)
{
  g_return_val_if_fail (GDK_IS_FRAME_HISTORY (history), 0);

  return history->frame_counter + 1 - history->n_timings;
}

void
gdk_frame_history_begin_frame (GdkFrameHistory *history)
{
  g_return_if_fail (GDK_IS_FRAME_HISTORY (history));

  history->frame_counter++;
  history->current = (history->current + 1) % FRAME_HISTORY_MAX_LENGTH;

  if (history->n_timings < FRAME_HISTORY_MAX_LENGTH)
    history->n_timings++;
  else
    {
      gdk_frame_timings_unref(history->timings[history->current]);
    }

  history->timings[history->current] = gdk_frame_timings_new (history->frame_counter);
}

GdkFrameTimings *
gdk_frame_history_get_timings (GdkFrameHistory *history,
                               gint64           frame_counter)
{
  gint pos;

  g_return_val_if_fail (GDK_IS_FRAME_HISTORY (history), NULL);

  if (frame_counter > history->frame_counter)
    return NULL;

  if (frame_counter <= history->frame_counter - history->n_timings)
    return NULL;

  pos = (history->current - (history->frame_counter - frame_counter) + FRAME_HISTORY_MAX_LENGTH) % FRAME_HISTORY_MAX_LENGTH;

  return history->timings[pos];
}

GdkFrameTimings *
gdk_frame_history_get_last_complete (GdkFrameHistory *history)
{
  gint i;

  g_return_val_if_fail (GDK_IS_FRAME_HISTORY (history), NULL);

  for (i = 0; i < history->n_timings; i++)
    {
      gint pos = ((history->current - i) + FRAME_HISTORY_MAX_LENGTH) % FRAME_HISTORY_MAX_LENGTH;
      if (gdk_frame_timings_get_complete (history->timings[pos]))
        return history->timings[pos];
    }

  return NULL;
}

#ifdef G_ENABLE_DEBUG
void
_gdk_frame_history_debug_print (GdkFrameHistory *history,
                                GdkFrameTimings *timings)
{
  gint64 frame_counter = gdk_frame_timings_get_frame_counter (timings);
  gint64 layout_start_time = _gdk_frame_timings_get_layout_start_time (timings);
  gint64 paint_start_time = _gdk_frame_timings_get_paint_start_time (timings);
  gint64 frame_end_time = _gdk_frame_timings_get_frame_end_time (timings);
  gint64 frame_time = gdk_frame_timings_get_frame_time (timings);
  gint64 presentation_time = gdk_frame_timings_get_presentation_time (timings);
  gint64 predicted_presentation_time = gdk_frame_timings_get_predicted_presentation_time (timings);
  gint64 refresh_interval = gdk_frame_timings_get_refresh_interval (timings);
  gint64 previous_frame_time = 0;
  gboolean slept_before = gdk_frame_timings_get_slept_before (timings);
  GdkFrameTimings *previous_timings = gdk_frame_history_get_timings (history,
                                                                     frame_counter - 1);

  if (previous_timings != NULL)
    previous_frame_time = gdk_frame_timings_get_frame_time (previous_timings);

  g_print ("%5" G_GINT64_FORMAT ":", frame_counter);
  if (previous_frame_time != 0)
    {
      g_print (" interval=%-4.1f", (frame_time - previous_frame_time) / 1000.);
      g_print (slept_before ?  " (sleep)" : "        ");
    }
  if (layout_start_time != 0)
    g_print (" layout_start=%-4.1f", (layout_start_time - frame_time) / 1000.);
  if (paint_start_time != 0)
    g_print (" paint_start=%-4.1f", (paint_start_time - frame_time) / 1000.);
  if (frame_end_time != 0)
    g_print (" frame_end=%-4.1f", (frame_end_time - frame_time) / 1000.);
  if (presentation_time != 0)
    g_print (" present=%-4.1f", (presentation_time - frame_time) / 1000.);
  if (predicted_presentation_time != 0)
    g_print (" predicted=%-4.1f", (predicted_presentation_time - frame_time) / 1000.);
  if (refresh_interval != 0)
    g_print (" refresh_interval=%-4.1f", refresh_interval / 1000.);
  g_print ("\n");
}
#endif /* G_ENABLE_DEBUG */
