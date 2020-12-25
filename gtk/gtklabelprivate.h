/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#ifndef __GTK_LABEL_PRIVATE_H__
#define __GTK_LABEL_PRIVATE_H__


#include <gtk/gtklabel.h>


G_BEGIN_DECLS

void _gtk_label_mnemonics_visible_apply_recursively (GtkWidget *widget,
                                                     gboolean   mnemonics_visible);
int _gtk_label_get_cursor_position (GtkLabel *label);
int _gtk_label_get_selection_bound (GtkLabel *label);

int          _gtk_label_get_n_links     (GtkLabel *label);
int          _gtk_label_get_link_at     (GtkLabel *label,
                                         int       pos);
void         _gtk_label_activate_link   (GtkLabel *label,
                                         int       idx);
const char *_gtk_label_get_link_uri    (GtkLabel *label,
                                         int       idx);
void         _gtk_label_get_link_extent (GtkLabel *label,
                                         int       idx,
                                         int      *start,
                                         int      *end);
gboolean     _gtk_label_get_link_visited (GtkLabel *label,
                                          int       idx);
gboolean     _gtk_label_get_link_focused (GtkLabel *label,
                                          int       idx);

G_END_DECLS

#endif /* __GTK_LABEL_PRIVATE_H__ */
