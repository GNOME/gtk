/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2011 Javier Jardón
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
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __GTK_CONTAINER_PRIVATE_H__
#define __GTK_CONTAINER_PRIVATE_H__

#include "gtkcontainer.h"

G_BEGIN_DECLS


GList *  _gtk_container_get_all_children       (GtkContainer *container);
void     _gtk_container_queue_resize           (GtkContainer *container);
void     _gtk_container_queue_restyle          (GtkContainer *container);
void     _gtk_container_resize_invalidate      (GtkContainer *container);
void     _gtk_container_clear_resize_widgets   (GtkContainer *container);
gchar*   _gtk_container_child_composite_name   (GtkContainer *container,
                                                GtkWidget    *child);
void     _gtk_container_dequeue_resize_handler (GtkContainer *container);
GList *  _gtk_container_focus_sort             (GtkContainer     *container,
                                                GList            *children,
                                                GtkDirectionType  direction,
                                                GtkWidget        *old_focus);
gboolean _gtk_container_get_reallocate_redraws (GtkContainer *container);

void      _gtk_container_stop_idle_sizer        (GtkContainer *container);
void      _gtk_container_maybe_start_idle_sizer (GtkContainer *container);
gboolean  _gtk_container_get_border_width_set   (GtkContainer *container);
void      _gtk_container_set_border_width_set   (GtkContainer *container,
                                                 gboolean      border_width_set);

G_END_DECLS

#endif /* __GTK_CONTAINER_PRIVATE_H__ */
