/*
 * Copyright © 2023 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#pragma once

#include <gtk/gtk.h>


G_BEGIN_DECLS

#define GTK_TYPE_LIST_MODEL_VALIDATOR (gtk_list_model_validator_get_type ())

G_DECLARE_FINAL_TYPE (GtkListModelValidator, gtk_list_model_validator, GTK, LIST_MODEL_VALIDATOR, GObject)

GtkListModelValidator * gtk_list_model_validator_new            (GListModel             *model);

void                    gtk_list_model_validator_set_model      (GtkListModelValidator  *self,
                                                                 GListModel             *model);
GListModel *            gtk_list_model_validator_get_model      (GtkListModelValidator  *self);

G_END_DECLS

