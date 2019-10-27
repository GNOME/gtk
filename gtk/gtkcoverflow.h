/*
 * Copyright © 2018 Benjamin Otte
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GTK_COVER_FLOW_H__
#define __GTK_COVER_FLOW_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtklistbase.h>

G_BEGIN_DECLS

#define GTK_TYPE_COVER_FLOW         (gtk_cover_flow_get_type ())
#define GTK_COVER_FLOW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_COVER_FLOW, GtkCoverFlow))
#define GTK_COVER_FLOW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_COVER_FLOW, GtkCoverFlowClass))
#define GTK_IS_COVER_FLOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_COVER_FLOW))
#define GTK_IS_COVER_FLOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_COVER_FLOW))
#define GTK_COVER_FLOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_COVER_FLOW, GtkCoverFlowClass))

/**
 * GtkCoverFlow:
 *
 * GtkCoverFlow is the simple list implementation for GTK's list widgets.
 */
typedef struct _GtkCoverFlow GtkCoverFlow;
typedef struct _GtkCoverFlowClass GtkCoverFlowClass;

GDK_AVAILABLE_IN_ALL
GType           gtk_cover_flow_get_type                          (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_cover_flow_new                               (void);
GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_cover_flow_new_with_factory                  (GtkListItemFactory     *factory);

GDK_AVAILABLE_IN_ALL
GListModel *    gtk_cover_flow_get_model                         (GtkCoverFlow            *self);
GDK_AVAILABLE_IN_ALL
void            gtk_cover_flow_set_model                         (GtkCoverFlow            *self,
                                                                 GListModel             *model);
GDK_AVAILABLE_IN_ALL
void            gtk_cover_flow_set_factory                       (GtkCoverFlow            *self,
                                                                 GtkListItemFactory     *factory);
GDK_AVAILABLE_IN_ALL
GtkListItemFactory *
                gtk_cover_flow_get_factory                       (GtkCoverFlow            *self);

G_END_DECLS

#endif  /* __GTK_COVER_FLOW_H__ */
