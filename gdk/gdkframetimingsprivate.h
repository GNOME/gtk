/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2010.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#pragma once

#include "gdkframetimings.h"

G_BEGIN_DECLS

typedef enum {
  GDK_FRAME_STAGE_NONE,
  GDK_FRAME_STAGE_FLUSH_EVENTS,
  GDK_FRAME_STAGE_BEFORE_PAINT,
  GDK_FRAME_STAGE_UPDATE,
  GDK_FRAME_STAGE_LAYOUT,
  GDK_FRAME_STAGE_PAINT,
  GDK_FRAME_STAGE_AFTER_PAINT,
  GDK_FRAME_STAGE_RESUME_EVENTS,

  GDK_FRAME_N_STAGES
} GdkFrameStage;

struct _GdkFrameTimings
{
  /*< private >*/
  guint ref_count;

  gint64 frame_counter;
  guint64 cookie;
  gint64 frame_time;
  gint64 drawn_time;
  gint64 presentation_time;
  gint64 refresh_interval;
  gint64 predicted_presentation_time;

  uint64_t stage_end_time[GDK_FRAME_N_STAGES];

  gint64 layout_start_time;
  gint64 paint_start_time;
  gint64 frame_end_time;

  GdkFrameResult result;
};

GdkFrameTimings *_gdk_frame_timings_new   (gint64           frame_counter);
gboolean         _gdk_frame_timings_steal (GdkFrameTimings *timings,
                                           gint64           frame_counter);

G_END_DECLS

