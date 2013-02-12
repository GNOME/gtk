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

GdkFrameTimings *gdk_frame_timings_ref   (GdkFrameTimings *timings);
void             gdk_frame_timings_unref (GdkFrameTimings *timings);

gint64           gdk_frame_timings_get_frame_counter     (GdkFrameTimings *timings);
gboolean         gdk_frame_timings_get_complete          (GdkFrameTimings *timings);
gint64           gdk_frame_timings_get_frame_time        (GdkFrameTimings *timings);
gint64           gdk_frame_timings_get_presentation_time (GdkFrameTimings *timings);
gint64           gdk_frame_timings_get_refresh_interval  (GdkFrameTimings *timings);

gint64           gdk_frame_timings_get_predicted_presentation_time (GdkFrameTimings *timings);

G_END_DECLS

#endif /* __GDK_FRAME_TIMINGS_H__ */
