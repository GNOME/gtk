/*
 * Copyright © 2019 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __GTK_PROPERTY_SELECTION_H__
#define __GTK_PROPERTY_SELECTION_H__

#include <gtk/gtktypes.h>
#include <gtk/gtkselectionmodel.h>

G_BEGIN_DECLS

#define GTK_TYPE_PROPERTY_SELECTION (gtk_property_selection_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkPropertySelection, gtk_property_selection, GTK, PROPERTY_SELECTION, GObject)

GDK_AVAILABLE_IN_ALL
GListModel *    gtk_property_selection_new      (GListModel     *model,
                                                 const char     *property);


G_END_DECLS

#endif /* __GTK_PROPERTY_SELECTION_H__ */
