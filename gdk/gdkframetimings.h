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

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#ifndef __GDK_FRAME_TIMINGS_H__
#define __GDK_FRAME_TIMINGS_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GdkFrameTimings GdkFrameTimings;

GType            gdk_frame_timings_get_type (void) G_GNUC_CONST;

GdkFrameTimings *gdk_frame_timings_new (gint64 frame_counter);

GdkFrameTimings *gdk_frame_timings_ref   (GdkFrameTimings *timings);
void             gdk_frame_timings_unref (GdkFrameTimings *timings);

gint64           gdk_frame_timings_get_frame_counter     (GdkFrameTimings *timings);

guint64          gdk_frame_timings_get_cookie            (GdkFrameTimings *timings);
void             gdk_frame_timings_set_cookie            (GdkFrameTimings *timings,
                                                          guint64          cookie);

gboolean         gdk_frame_timings_get_complete          (GdkFrameTimings *timings);
void             gdk_frame_timings_set_complete          (GdkFrameTimings *timings,
                                                          gboolean         complete);

gboolean         gdk_frame_timings_get_slept_before      (GdkFrameTimings *timings);
void             gdk_frame_timings_set_slept_before      (GdkFrameTimings *timings,
                                                          gboolean         slept_before);

gint64           gdk_frame_timings_get_frame_time        (GdkFrameTimings *timings);
void             gdk_frame_timings_set_frame_time        (GdkFrameTimings *timings,
                                                          gint64           frame_time);
gint64           gdk_frame_timings_get_drawn_time        (GdkFrameTimings *timings);
void             gdk_frame_timings_set_drawn_time        (GdkFrameTimings *timings,
                                                          gint64           frame_time);
gint64           gdk_frame_timings_get_presentation_time (GdkFrameTimings *timings);
void             gdk_frame_timings_set_presentation_time (GdkFrameTimings *timings,
                                                          gint64           presentation_time);
gint64           gdk_frame_timings_get_refresh_interval  (GdkFrameTimings *timings);
void             gdk_frame_timings_set_refresh_interval  (GdkFrameTimings *timings,
                                                          gint64           refresh_interval);

gint64           gdk_frame_timings_get_predicted_presentation_time (GdkFrameTimings *timings);
void             gdk_frame_timings_set_predicted_presentation_time (GdkFrameTimings *timings,
                                                                    gint64           predicted_presentation_time);

G_END_DECLS

#endif /* __GDK_FRAME_TIMINGS_H__ */
