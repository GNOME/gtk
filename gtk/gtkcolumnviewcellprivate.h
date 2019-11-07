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

#ifndef __GTK_COLUMN_VIEW_CELL_PRIVATE_H__
#define __GTK_COLUMN_VIEW_CELL_PRIVATE_H__

#include "gtkcolumnviewcolumn.h"

G_BEGIN_DECLS

#define GTK_TYPE_COLUMN_VIEW_CELL         (gtk_column_view_cell_get_type ())
#define GTK_COLUMN_VIEW_CELL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_COLUMN_VIEW_CELL, GtkColumnViewCell))
#define GTK_COLUMN_VIEW_CELL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_COLUMN_VIEW_CELL, GtkColumnViewCellClass))
#define GTK_IS_COLUMN_VIEW_CELL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_COLUMN_VIEW_CELL))
#define GTK_IS_COLUMN_VIEW_CELL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_COLUMN_VIEW_CELL))
#define GTK_COLUMN_VIEW_CELL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_COLUMN_VIEW_CELL, GtkColumnViewCellClass))

typedef struct _GtkColumnViewCell GtkColumnViewCell;
typedef struct _GtkColumnViewCellClass GtkColumnViewCellClass;

GType                   gtk_column_view_cell_get_type           (void) G_GNUC_CONST;

GtkWidget *             gtk_column_view_cell_new                (GtkColumnViewColumn    *column);

void                    gtk_column_view_cell_remove             (GtkColumnViewCell      *self);

GtkColumnViewCell *     gtk_column_view_cell_get_next           (GtkColumnViewCell      *self);
GtkColumnViewCell *     gtk_column_view_cell_get_prev           (GtkColumnViewCell      *self);
GtkColumnViewColumn *   gtk_column_view_cell_get_column         (GtkColumnViewCell      *self);

G_END_DECLS

#endif  /* __GTK_COLUMN_VIEW_CELL_PRIVATE_H__ */
