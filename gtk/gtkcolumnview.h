/*
 * Copyright © 2019 Benjamin Otte
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

#ifndef __GTK_COLUMN_VIEW_H__
#define __GTK_COLUMN_VIEW_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_COLUMN_VIEW         (gtk_column_view_get_type ())
#define GTK_COLUMN_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_COLUMN_VIEW, GtkColumnView))
#define GTK_COLUMN_VIEW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_COLUMN_VIEW, GtkColumnViewClass))
#define GTK_IS_COLUMN_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_COLUMN_VIEW))
#define GTK_IS_COLUMN_VIEW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_COLUMN_VIEW))
#define GTK_COLUMN_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_COLUMN_VIEW, GtkColumnViewClass))

/**
 * GtkColumnView:
 *
 * GtkColumnView is the simple list implementation for GTK's list widgets.
 */
typedef struct _GtkColumnView GtkColumnView;
typedef struct _GtkColumnViewClass GtkColumnViewClass;
/* forward declaration */
typedef struct _GtkColumnViewColumn GtkColumnViewColumn;

GDK_AVAILABLE_IN_ALL
GType           gtk_column_view_get_type                        (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_column_view_new                             (void);

GDK_AVAILABLE_IN_ALL
GListModel *    gtk_column_view_get_columns                     (GtkColumnView          *self);
GDK_AVAILABLE_IN_ALL
void            gtk_column_view_append_column                   (GtkColumnView          *self,
                                                                 GtkColumnViewColumn    *column);
GDK_AVAILABLE_IN_ALL
void            gtk_column_view_remove_column                   (GtkColumnView          *self,
                                                                 GtkColumnViewColumn    *column);

GDK_AVAILABLE_IN_ALL
GListModel *    gtk_column_view_get_model                       (GtkColumnView          *self);
GDK_AVAILABLE_IN_ALL
void            gtk_column_view_set_model                       (GtkColumnView          *self,
                                                                 GListModel             *model);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_column_view_get_show_separators             (GtkColumnView          *self);
GDK_AVAILABLE_IN_ALL
void            gtk_column_view_set_show_separators             (GtkColumnView          *self,
                                                                 gboolean                show_separators);

G_END_DECLS

#endif  /* __GTK_COLUMN_VIEW_H__ */
