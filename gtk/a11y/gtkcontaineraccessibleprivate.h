/* GTK+ - accessibility implementations
 * Copyright (C) 2014  Benjamin Otte <otte@gnome.org>
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

#ifndef __GTK_CONTAINER_ACCESSIBLE_PRIVATE_H__
#define __GTK_CONTAINER_ACCESSIBLE_PRIVATE_H__

#include <gtk/a11y/gtkcontaineraccessible.h>

G_BEGIN_DECLS

void            _gtk_container_accessible_add_child     (GtkContainerAccessible *accessible,
                                                         AtkObject              *child,
                                                         gint                    index);
void            _gtk_container_accessible_remove_child  (GtkContainerAccessible *accessible,
                                                         AtkObject              *child,
                                                         gint                    index);
void            _gtk_container_accessible_add           (GtkWidget              *parent,
                                                         GtkWidget              *child);
void            _gtk_container_accessible_remove        (GtkWidget              *parent,
                                                         GtkWidget              *child);

G_END_DECLS

#endif /* __GTK_CONTAINER_ACCESSIBLE_PRIVATE_H__ */
