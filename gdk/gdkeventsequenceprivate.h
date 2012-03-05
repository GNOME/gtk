/* gdkeventsequence.h - tracking sequences of events
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.▸ See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#ifndef __GDK_EVENT_SEQUENCE_H__
#define __GDK_EVENT_SEQUENCE_H__

#include <gdk/gdktypes.h>

struct _GdkEventSequence {
  GdkDevice *device;
  guint sequence_id;
  guint ref_count;
  double *axes;
};

GdkEventSequence *      gdk_event_sequence_new          (GdkDevice              *device,
                                                         guint                   sequence_id);
GdkEventSequence *      gdk_event_sequence_lookup       (GdkDevice              *device,
                                                         guint                   sequence_id);

GdkEventSequence *      gdk_event_sequence_ref          (GdkEventSequence       *sequence);
void                    gdk_event_sequence_unref        (GdkEventSequence       *sequence);

#endif  /* __GDK_EVENT_SEQUENCE_H__ */
