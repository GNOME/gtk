/*
 * Copyright (c) 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __GTK_HEADER_BAR_PRIVATE_H__
#define __GTK_HEADER_BAR_PRIVATE_H__

G_BEGIN_DECLS

gboolean     _gtk_header_bar_shows_app_menu        (GtkHeaderBar *bar);
void         _gtk_header_bar_update_window_buttons (GtkHeaderBar *bar);
gboolean     _gtk_header_bar_update_window_icon    (GtkHeaderBar *bar,
                                                    GtkWindow    *window);

G_END_DECLS

#endif /* __GTK_HEADER_BAR_PRIVATE_H__ */
