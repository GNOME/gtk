/*
 * Copyright © 2020 Benjamin Otte
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
 */

#ifndef __GTK_SORTER_PRIVATE_H__
#define __GTK_SORTER_PRIVATE_H__

#include <gtk/gtksorter.h>

#include "gtk/gtksortkeysprivate.h"

GtkSortKeys *           gtk_sorter_get_keys                     (GtkSorter              *self);

void                    gtk_sorter_changed_with_keys            (GtkSorter              *self,
                                                                 GtkSorterChange         change,
                                                                 GtkSortKeys            *keys);


#endif /* __GTK_SORTER_PRIVATE_H__ */

