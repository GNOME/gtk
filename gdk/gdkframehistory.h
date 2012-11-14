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

#ifndef __GDK_FRAME_HISTORY_H__
#define __GDK_FRAME_HISTORY_H__

#include <gdk/gdkframetimings.h>

G_BEGIN_DECLS

typedef struct _GdkFrameHistory      GdkFrameHistory;
typedef struct _GdkFrameHistoryClass GdkFrameHistoryClass;

#define GDK_TYPE_FRAME_HISTORY  (gdk_frame_history_get_type ())
#define GDK_FRAME_HISTORY(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_FRAME_HISTORY, GdkFrameHistory))
#define GDK_IS_FRAME_HISTORY(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_FRAME_HISTORY))

GType            gdk_frame_history_get_type (void) G_GNUC_CONST;

GdkFrameHistory *gdk_frame_history_new (void);

gint64           gdk_frame_history_get_frame_counter (GdkFrameHistory *history);
gint64           gdk_frame_history_get_start         (GdkFrameHistory *history);
void             gdk_frame_history_begin_frame       (GdkFrameHistory *history);
GdkFrameTimings *gdk_frame_history_get_timings       (GdkFrameHistory *history,
                                                      gint64           frame_counter);
GdkFrameTimings *gdk_frame_history_get_last_complete (GdkFrameHistory *history);

G_END_DECLS

#endif /* __GDK_FRAME_HISTORY_H__ */
