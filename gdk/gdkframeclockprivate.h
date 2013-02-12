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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2010.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/* Uninstalled header, internal to GDK */

#ifndef __GDK_FRAME_CLOCK_PRIVATE_H__
#define __GDK_FRAME_CLOCK_PRIVATE_H__

#include <gdk/gdkframeclock.h>

G_BEGIN_DECLS

typedef struct _GdkFrameClockIdlePrivate       GdkFrameClockIdlePrivate;

struct _GdkFrameClock
{
  GObject parent_instance;

  /*< private >*/
  GdkFrameClockPrivate *priv;
};

struct _GdkFrameClockClass
{
  GObjectClass parent_class;

  guint64  (* get_frame_time)            (GdkFrameClock *clock);

  void               (* request_phase) (GdkFrameClock      *clock,
                                        GdkFrameClockPhase  phase);
  GdkFrameClockPhase (* get_requested) (GdkFrameClock      *clock);

  void     (* freeze) (GdkFrameClock *clock);
  void     (* thaw)   (GdkFrameClock *clock);

  /* signals */
  /* void (* frame_requested)    (GdkFrameClock *clock); */
  /* void (* flush_events)       (GdkFrameClock *clock); */
  /* void (* before_paint)       (GdkFrameClock *clock); */
  /* void (* update)             (GdkFrameClock *clock); */
  /* void (* layout)             (GdkFrameClock *clock); */
  /* void (* paint)              (GdkFrameClock *clock); */
  /* void (* after_paint)        (GdkFrameClock *clock); */
  /* void (* resume_events)      (GdkFrameClock *clock); */
};

void _gdk_frame_clock_begin_frame         (GdkFrameClock   *clock);
void _gdk_frame_clock_debug_print_timings (GdkFrameClock   *clock,
                                           GdkFrameTimings *timings);

G_END_DECLS

#endif /* __GDK_FRAME_CLOCK_PRIVATE_H__ */
