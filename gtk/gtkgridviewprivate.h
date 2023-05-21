/*
 * Copyright © 2023 Red Hat, Inc.
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


#pragma once

#include "gtk/gtktypes.h"
#include "gtk/gtkenums.h"
#include "gtk/gtkgridview.h"
#include "gtk/gtklistitemmanagerprivate.h"

G_BEGIN_DECLS

void         gtk_grid_view_get_section_for_position (GtkListItemManager *items,
                                                     unsigned int        position,
                                                     unsigned int       *start,
                                                     unsigned int       *end);

unsigned int gtk_grid_view_get_column_for_position (GtkListItemManager *items,
                                                    unsigned int        n_columns,
                                                    unsigned int        position);

gboolean     gtk_grid_view_is_multirow_tile        (GtkListItemManager *items,
                                                    unsigned int        n_columns,
                                                    GtkListTile        *tile);

void         gtk_grid_view_split_tiles_by_columns  (GtkListItemManager *items,
                                                    guint               n_columns);

G_END_DECLS

