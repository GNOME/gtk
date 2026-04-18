/*
 * Copyright (c) 2026 Kristjan ESPERANTO
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/*
 * GdkAndroidChoreographerSource — vsync-aligned GSource for the Android backend
 *
 * Design (analogous to gdkdisplaylinksource.c on macOS):
 *
 *   Android delivers vsync callbacks via AChoreographer. The GTK main loop
 *   runs on a separate pthread.  We bridge the two worlds with an atomic
 *   pending flag plus g_main_context_wakeup().
 *
 *     Choreographer callback: stores presentation_time, marks pending and
 *                             wakes the target main context
 *     GTK thread (GLib):      prepare()/check() observe pending, dispatch()
 *                             clears it and invokes the GSourceFunc callback
 *
 *   Only one pending notification is allowed at a time (needs_dispatch flag),
 *   matching the macOS implementation's approach.
 *
 *   One source is created per GdkAndroidDisplay (not per surface), mirroring
 *   the per-monitor CVDisplayLink model used by the macOS backend.
 *
 * The callback re-posts itself while unpaused.
 */

#include "config.h"

#include "gdkandroidchoreographersource-private.h"

#include "gdkandroidutils-private.h"

#include <android/choreographer.h>
#include <stdint.h>

struct _GdkAndroidChoreographerSource
{
  GSource       source;

  gint          needs_dispatch;    /* atomic; set on UI thread, cleared on GTK thread */
  gint          paused;            /* atomic; read on UI thread, written on GTK thread */
  gint64        presentation_time; /* µs, CLOCK_MONOTONIC; naturally atomic on 64-bit */

  AChoreographer *choreographer;
  GMainContext *context;
};

static void
gdk_android_choreographer_source_on_frame (int64_t frame_time_nanos,
                                           void *data)
{
  GdkAndroidChoreographerSource *self = data;

  if (!self || g_atomic_int_get (&self->paused))
    return;

  self->presentation_time = frame_time_nanos / 1000;  /* ns -> us */

  if (g_atomic_int_compare_and_exchange (&self->needs_dispatch, FALSE, TRUE))
    g_main_context_wakeup (self->context);

  AChoreographer_postFrameCallback64 (self->choreographer,
                                      gdk_android_choreographer_source_on_frame,
                                      self);
}

static gboolean
gdk_android_choreographer_source_prepare (GSource *source,
                                          int     *timeout_out)
{
  GdkAndroidChoreographerSource *self = (GdkAndroidChoreographerSource *) source;

  *timeout_out = -1;
  return g_atomic_int_get (&self->needs_dispatch);
}

static gboolean
gdk_android_choreographer_source_check (GSource *source)
{
  GdkAndroidChoreographerSource *self = (GdkAndroidChoreographerSource *) source;

  return g_atomic_int_get (&self->needs_dispatch);
}

static gboolean
gdk_android_choreographer_source_dispatch (GSource     *source,
                                           GSourceFunc  callback,
                                           gpointer     user_data)
{
  GdkAndroidChoreographerSource *self = (GdkAndroidChoreographerSource *) source;

  g_atomic_int_set (&self->needs_dispatch, FALSE);

  if (g_atomic_int_get (&self->paused))
    return G_SOURCE_CONTINUE;

  if (callback != NULL)
    return callback (user_data);

  return G_SOURCE_CONTINUE;
}

static void
gdk_android_choreographer_source_finalize (GSource *source)
{
  GdkAndroidChoreographerSource *self = (GdkAndroidChoreographerSource *) source;

  g_atomic_int_set (&self->paused, TRUE);
}

static GSourceFuncs gdk_android_choreographer_source_funcs = {
  gdk_android_choreographer_source_prepare,
  gdk_android_choreographer_source_check,
  gdk_android_choreographer_source_dispatch,
  gdk_android_choreographer_source_finalize,
};

GSource *
gdk_android_choreographer_source_new (GMainContext *context)
{
  GdkAndroidChoreographerSource *self;
  GSource *source;

  g_return_val_if_fail (context != NULL, NULL);

  source = g_source_new (&gdk_android_choreographer_source_funcs, sizeof *self);
  self = (GdkAndroidChoreographerSource *) source;
  self->context = context;
  g_atomic_int_set (&self->paused, TRUE);

  self->choreographer = AChoreographer_getInstance ();
  if (self->choreographer == NULL)
    {
      g_warning ("GdkAndroidChoreographerSource: AChoreographer_getInstance() failed");
      g_source_unref (source);
      return NULL;
    }

  g_source_set_name (source, "GdkAndroidChoreographerSource");

  return source;
}

void
gdk_android_choreographer_source_pause (GdkAndroidChoreographerSource *self)
{
  g_return_if_fail (self != NULL);

  if (g_atomic_int_get (&self->paused))
    return;

  g_atomic_int_set (&self->paused, TRUE);
}

void
gdk_android_choreographer_source_unpause (GdkAndroidChoreographerSource *self)
{
  g_return_if_fail (self != NULL);

  if (!g_atomic_int_get (&self->paused))
    return;

  g_atomic_int_set (&self->paused, FALSE);

  AChoreographer_postFrameCallback64 (self->choreographer,
                                      gdk_android_choreographer_source_on_frame,
                                      self);
}

gint64
gdk_android_choreographer_source_get_presentation_time (GdkAndroidChoreographerSource *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->presentation_time;
}
