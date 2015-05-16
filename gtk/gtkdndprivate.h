/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Red Hat, Inc.
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

#ifndef __GTK_DND_PRIVATE_H__
#define __GTK_DND_PRIVATE_H__


#include <gtk/gtkwidget.h>
#include <gtk/gtkselection.h>


G_BEGIN_DECLS

void _gtk_drag_source_handle_event (GtkWidget *widget,
				    GdkEvent  *event);
void _gtk_drag_dest_handle_event   (GtkWidget *toplevel,
				    GdkEvent  *event);

G_END_DECLS

#endif /* __GTK_DND_PRIVATE_H__ */
