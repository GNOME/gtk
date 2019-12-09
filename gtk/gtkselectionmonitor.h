/*
 * Copyright © 2019 Matthias Clasen
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

#ifndef __GTK_SELECTION_MONITOR_H__
#define __GTK_SELECTION_MONITOR_H__

#include <gtk/gtktypes.h>
#include <gtk/gtkselectionmodel.h>

G_BEGIN_DECLS

#define GTK_TYPE_SELECTION_MONITOR (gtk_selection_monitor_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSelectionMonitor, gtk_selection_monitor, GTK, SELECTION_MONITOR, GObject)

GDK_AVAILABLE_IN_ALL
GtkSelectionMonitor *   gtk_selection_monitor_new       (GtkSelectionModel   *model);

GDK_AVAILABLE_IN_ALL
GtkSelectionModel *     gtk_selection_monitor_get_model (GtkSelectionMonitor *self);

G_END_DECLS

#endif /* __GTK_SELECTION_MONITOR_H__ */
