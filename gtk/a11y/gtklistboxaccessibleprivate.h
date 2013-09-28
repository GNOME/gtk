/*
 * Copyright (C) 2013 Red Hat, Inc.
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

#ifndef __GTK_LIST_BOX_ACCESSIBLE_PRIVATE_H__
#define __GTK_LIST_BOX_ACCESSIBLE_PRIVATE_H__

#include <gtk/a11y/gtklistboxaccessible.h>

G_BEGIN_DECLS

void _gtk_list_box_accessible_update_cursor   (GtkListBox *box, GtkListBoxRow *child);
void _gtk_list_box_accessible_selection_changed (GtkListBox *box);

G_END_DECLS

#endif /* __GTK_LIST_BOX_ACCESSIBLE_PRIVATE_H__ */
