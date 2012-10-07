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

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#ifndef __GDK_FRAME_CLOCK_H__
#define __GDK_FRAME_CLOCK_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GdkFrameClock                GdkFrameClock;
typedef struct _GdkFrameClockInterface       GdkFrameClockInterface;
typedef struct _GdkFrameClockTarget          GdkFrameClockTarget;
typedef struct _GdkFrameClockTargetInterface GdkFrameClockTargetInterface;

#define GDK_TYPE_FRAME_CLOCK_TARGET             (gdk_frame_clock_target_get_type ())
#define GDK_FRAME_CLOCK_TARGET(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_FRAME_CLOCK_TARGET, GdkFrameClockTarget))
#define GDK_IS_FRAME_CLOCK_TARGET(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_FRAME_CLOCK_TARGET))
#define GDK_FRAME_CLOCK_TARGET_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GDK_TYPE_FRAME_CLOCK_TARGET, GdkFrameClockTargetInterface))

struct _GdkFrameClockTargetInterface
{
  GTypeInterface base_iface;

  void (*set_clock) (GdkFrameClockTarget *target,
                     GdkFrameClock       *clock);
};

GType gdk_frame_clock_target_get_type (void) G_GNUC_CONST;

void gdk_frame_clock_target_set_clock (GdkFrameClockTarget *target,
                                       GdkFrameClock       *clock);

#define GDK_TYPE_FRAME_CLOCK             (gdk_frame_clock_get_type ())
#define GDK_FRAME_CLOCK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_FRAME_CLOCK, GdkFrameClock))
#define GDK_IS_FRAME_CLOCK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_FRAME_CLOCK))
#define GDK_FRAME_CLOCK_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GDK_TYPE_FRAME_CLOCK, GdkFrameClockInterface))

typedef enum {
  GDK_FRAME_CLOCK_PHASE_NONE          = 0,
  GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS  = 1 << 0,
  GDK_FRAME_CLOCK_PHASE_BEFORE_PAINT  = 1 << 1,
  GDK_FRAME_CLOCK_PHASE_UPDATE        = 1 << 2,
  GDK_FRAME_CLOCK_PHASE_LAYOUT        = 1 << 3,
  GDK_FRAME_CLOCK_PHASE_PAINT         = 1 << 4,
  GDK_FRAME_CLOCK_PHASE_RESUME_EVENTS = 1 << 5,
  GDK_FRAME_CLOCK_PHASE_AFTER_PAINT   = 1 << 6
} GdkFrameClockPhase;

struct _GdkFrameClockInterface
{
  GTypeInterface		   base_iface;

  guint64  (* get_frame_time)      (GdkFrameClock *clock);

  void               (* request_phase) (GdkFrameClock      *clock,
                                        GdkFrameClockPhase  phase);
  GdkFrameClockPhase (* get_requested) (GdkFrameClock      *clock);

  void     (* freeze)              (GdkFrameClock *clock);
  void     (* thaw)                (GdkFrameClock *clock);

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

GType    gdk_frame_clock_get_type             (void) G_GNUC_CONST;

guint64  gdk_frame_clock_get_frame_time      (GdkFrameClock *clock);

void               gdk_frame_clock_request_phase (GdkFrameClock      *clock,
                                                  GdkFrameClockPhase  phase);
GdkFrameClockPhase gdk_frame_clock_get_requested (GdkFrameClock      *clock);

void     gdk_frame_clock_freeze              (GdkFrameClock *clock);
void     gdk_frame_clock_thaw                (GdkFrameClock *clock);

/* Convenience API */
void  gdk_frame_clock_get_frame_time_val (GdkFrameClock  *clock,
                                          GTimeVal       *timeval);

/* Signal emitters (used in frame clock implementations) */
void     gdk_frame_clock_frame_requested     (GdkFrameClock *clock);

G_END_DECLS

#endif /* __GDK_FRAME_CLOCK_H__ */
