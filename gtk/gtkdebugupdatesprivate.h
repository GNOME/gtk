/* GTK - The GIMP Toolkit
 * Copyright (C) 2016 Benjamin Otte <otte@gnome.org>
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

#ifndef __GTK_DEBUG_UPDATES_PRIVATE_H__
#define __GTK_DEBUG_UPDATES_PRIVATE_H__

#include "gtkwidget.h"

G_BEGIN_DECLS


gboolean        gtk_debug_updates_get_enabled                   (void);
void            gtk_debug_updates_set_enabled                   (gboolean                enabled);
gboolean        gtk_debug_updates_get_enabled_for_display       (GdkDisplay             *display);
void            gtk_debug_updates_set_enabled_for_display       (GdkDisplay             *display,
                                                                 gboolean                enabled);

void            gtk_debug_updates_add                           (GtkWidget              *widget,
                                                                 const cairo_region_t   *region);
void            gtk_debug_updates_snapshot                      (GtkWidget              *widget,
                                                                 GtkSnapshot            *snapshot);


G_END_DECLS

#endif /* __GTK_DEBUG_UPDATES_PRIVATE_H__ */
