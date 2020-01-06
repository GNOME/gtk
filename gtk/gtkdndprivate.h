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

#include "gtkwidget.h"
#include "gtkdragdest.h"


G_BEGIN_DECLS

void                    _gtk_drag_dest_handle_event     (GtkWidget              *toplevel,
				                         GdkEvent               *event);

typedef struct _GtkDragDestInfo GtkDragDestInfo;

struct _GtkDragDestInfo
{
  GtkDropTarget     *dest;
  GdkDrop           *drop;                /* drop */
};

GtkDragDestInfo * gtk_drag_get_dest_info   (GdkDrop          *drop,
                                            gboolean          create);
void              gtk_drag_dest_set_target (GtkDragDestInfo  *info,
                                            GtkDropTarget    *dest);

G_END_DECLS

#endif /* __GTK_DND_PRIVATE_H__ */
