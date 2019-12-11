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

#ifndef __GTK_COLUMN_VIEW_LAYOUT_PRIVATE_H__
#define __GTK_COLUMN_VIEW_LAYOUT_PRIVATE_H__

#include <gtk/gtkcolumnview.h>
#include <gtk/gtklayoutmanager.h>

G_BEGIN_DECLS

#define GTK_TYPE_COLUMN_VIEW_LAYOUT (gtk_column_view_layout_get_type ())

G_DECLARE_FINAL_TYPE (GtkColumnViewLayout, gtk_column_view_layout, GTK, COLUMN_VIEW_LAYOUT, GtkLayoutManager)

GtkLayoutManager *      gtk_column_view_layout_new              (GtkColumnView          *view);


#endif  /* __GTK_COLUMN_VIEW_LAYOUT_PRIVATE_H__ */
