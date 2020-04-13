/* GTK+ - accessibility implementations
 * Copyright (C) 2002, 2004  Anders Carlsson <andersca@gnu.org>
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

#ifndef __GTK_LABEL_ACCESSIBLE_PRIVATE_H__
#define __GTK_LABEL_ACCESSIBLE_PRIVATE_H__

#include <gtk/a11y/gtklabelaccessible.h>

G_BEGIN_DECLS

void _gtk_label_accessible_text_deleted  (GtkLabel *label);
void _gtk_label_accessible_text_inserted (GtkLabel *label);
void _gtk_label_accessible_update_links  (GtkLabel *label);
void _gtk_label_accessible_focus_link_changed (GtkLabel *label);
void _gtk_label_accessible_selection_bound_changed (GtkLabel *label);
void _gtk_label_accessible_cursor_position_changed (GtkLabel *label);

G_END_DECLS

#endif /* __GTK_LABEL_ACCESSIBLE_PRIVATE_H__ */
