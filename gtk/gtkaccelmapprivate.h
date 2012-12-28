/* GTK - The GIMP Toolkit
 * Copyright (C) 1998, 2001 Tim Janik
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

#ifndef __GTK_ACCEL_MAP_PRIVATE_H__
#define __GTK_ACCEL_MAP_PRIVATE_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkaccelmap.h>

G_BEGIN_DECLS

void            _gtk_accel_map_init         (void);

void            _gtk_accel_map_add_group    (const gchar   *accel_path,
                                             GtkAccelGroup *accel_group);
void            _gtk_accel_map_remove_group (const gchar   *accel_path,
                                             GtkAccelGroup *accel_group);
gboolean        _gtk_accel_path_is_valid    (const gchar   *accel_path);

gchar         * _gtk_accel_path_for_action  (const gchar   *action_name,
                                             GVariant      *parameter);

G_END_DECLS

#endif /* __GTK_ACCEL_MAP_PRIVATE_H__ */
