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

#define GDK_TYPE_FRAME_CLOCK             (gdk_frame_clock_get_type ())
#define GDK_FRAME_CLOCK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_FRAME_CLOCK, GdkFrameClock))
#define GDK_IS_FRAME_CLOCK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_FRAME_CLOCK))
#define GDK_FRAME_CLOCK_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GDK_TYPE_FRAME_CLOCK, GdkFrameClockInterface))

typedef struct _GdkFrameClock          GdkFrameClock;
typedef struct _GdkFrameClockInterface GdkFrameClockInterface;

struct _GdkFrameClockInterface
{
  GTypeInterface		   base_iface;

  guint64  (* get_frame_time)      (GdkFrameClock *clock);
  void     (* request_frame)       (GdkFrameClock *clock);
  gboolean (* get_frame_requested) (GdkFrameClock *clock);

  /* signals */
  /* void (* frame_requested)    (GdkFrameClock *clock); */
  /* void (* before_paint)       (GdkFrameClock *clock); */
  /* void (* layout)             1(GdkFrameClock *clock); */
  /* void (* paint)              (GdkFrameClock *clock); */
  /* void (* after_paint)        (GdkFrameClock *clock); */
};

GType    gdk_frame_clock_get_type             (void) G_GNUC_CONST;

guint64  gdk_frame_clock_get_frame_time      (GdkFrameClock *clock);
void     gdk_frame_clock_request_frame       (GdkFrameClock *clock);
gboolean gdk_frame_clock_get_frame_requested (GdkFrameClock *clock);

/* Convenience API */
void  gdk_frame_clock_get_frame_time_val (GdkFrameClock  *clock,
                                          GTimeVal       *timeval);

/* Signal emitters (used in frame clock implementations) */
void     gdk_frame_clock_frame_requested     (GdkFrameClock *clock);
void     gdk_frame_clock_paint               (GdkFrameClock *clock);

G_END_DECLS

#endif /* __GDK_FRAME_CLOCK_H__ */
