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
#include <gtk/gtkdragdest.h>

#include "gtkimagedefinitionprivate.h"

typedef struct _GtkDragDestSite GtkDragDestSite;
struct _GtkDragDestSite
{
  GtkDestDefaults    flags;
  GtkTargetList     *target_list;
  GdkDragAction      actions;
  GdkWindow         *proxy_window;
  GdkDragProtocol    proxy_protocol;
  guint              do_proxy     : 1;
  guint              proxy_coords : 1;
  guint              have_drag    : 1;
  guint              track_motion : 1;
};

G_BEGIN_DECLS

GdkDragContext *        gtk_drag_begin_internal         (GtkWidget              *widget,
                                                         gboolean               *out_needs_icon,
                                                         GtkTargetList          *target_list,
                                                         GdkDragAction           actions,
                                                         gint                    button,
                                                         const GdkEvent         *event,
                                                         int                     x,
                                                         int                     y);
void                    gtk_drag_set_icon_definition    (GdkDragContext         *context,
                                                         GtkImageDefinition     *def,
                                                         gint                    hot_x,
                                                         gint                    hot_y);
void                    _gtk_drag_source_handle_event   (GtkWidget              *widget,
                                                         GdkEvent               *event);
void                    _gtk_drag_dest_handle_event     (GtkWidget              *toplevel,
				                         GdkEvent               *event);

G_END_DECLS

#endif /* __GTK_DND_PRIVATE_H__ */
