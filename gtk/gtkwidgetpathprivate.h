/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __GTK_WIDGET_PATH_PRIVATE_H__
#define __GTK_WIDGET_PATH_PRIVATE_H__

#include <gtk/gtkwidgetpath.h>

G_BEGIN_DECLS

void gtk_widget_path_iter_add_qclass (GtkWidgetPath *path,
                                      gint           pos,
                                      GQuark         qname);

G_END_DECLS

#endif /* __GTK_WIDGET_PATH_PRIVATE_H__ */
