/* GTK - The GIMP Toolkit
 * gtksizegroup-private.h:
 * Copyright (C) 2000-2010 Red Hat Software
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

#ifndef __GTK_SIZE_GROUP_PRIVATE_H__
#define __GTK_SIZE_GROUP_PRIVATE_H__

#include <gtk/gtksizegroup.h>

G_BEGIN_DECLS

/*
 * GtkQueueResizeFlags:
 * @GTK_QUEUE_RESIZE_INVALIDATE_ONLY: invalidate all cached sizes
 *  as we would normally do when a widget is queued for resize,
 *  but don’t actually add the toplevel resize container to the
 *  resize queue. Useful if we want to change the size of a widget
 *  see how that would affect the overall layout, then restore
 *  the old size.
 *
 * Flags that affect the operation of queueing a widget for resize.
 */
typedef enum
{
  GTK_QUEUE_RESIZE_INVALIDATE_ONLY = 1 << 0
} GtkQueueResizeFlags;

GHashTable * _gtk_size_group_get_widget_peers (GtkWidget           *for_widget,
                                               GtkOrientation       orientation);
void _gtk_size_group_queue_resize             (GtkWidget           *widget,
                                               GtkQueueResizeFlags  flags);

G_END_DECLS

#endif /* __GTK_SIZE_GROUP_PRIVATE_H__ */
