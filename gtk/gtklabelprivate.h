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
gint _gtk_label_get_cursor_position (GtkLabel *label);

gint         _gtk_label_get_n_links     (GtkLabel *label);
gint         _gtk_label_get_link_at     (GtkLabel *label,
                                         gint      pos);
void         _gtk_label_activate_link   (GtkLabel *label, 
                                         gint      idx);
const gchar *_gtk_label_get_link_uri    (GtkLabel *label,
                                         gint      idx);
void         _gtk_label_get_link_extent (GtkLabel *label,
                                         gint      idx,
                                         gint     *start,
                                         gint     *end);
gboolean     _gtk_label_get_link_visited (GtkLabel *label,
                                          gint      idx);
gboolean     _gtk_label_get_link_focused (GtkLabel *label,
                                          gint      idx);
                             
G_END_DECLS

#endif /* __GTK_LABEL_PRIVATE_H__ */
