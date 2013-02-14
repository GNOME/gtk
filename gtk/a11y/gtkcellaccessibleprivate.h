/* GTK+ - accessibility implementations
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_CELL_ACCESSIBLE_PRIVATE_H__
#define __GTK_CELL_ACCESSIBLE_PRIVATE_H__

#include <gtk/a11y/gtkcellaccessible.h>

G_BEGIN_DECLS

void     _gtk_cell_accessible_state_changed (GtkCellAccessible *cell,
                                             GtkCellRendererState added,
                                             GtkCellRendererState removed);
void     _gtk_cell_accessible_update_cache  (GtkCellAccessible *cell);
void     _gtk_cell_accessible_initialize    (GtkCellAccessible *cell,
                                             GtkWidget         *widget,
                                             AtkObject         *parent);
gboolean _gtk_cell_accessible_add_state     (GtkCellAccessible *cell,
                                             AtkStateType       state_type,
                                             gboolean           emit_signal);
gboolean _gtk_cell_accessible_remove_state  (GtkCellAccessible *cell,
                                             AtkStateType       state_type,
                                             gboolean           emit_signal);

G_END_DECLS

#endif /* __GTK_CELL_ACCESSIBLE_PRIVATE_H__ */
