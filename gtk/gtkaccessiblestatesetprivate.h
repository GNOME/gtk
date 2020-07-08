/* gtkaccessiblestatesetprivate.h: Accessible state container
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gtkaccessibleprivate.h"
#include "gtkaccessiblevalueprivate.h"

G_BEGIN_DECLS

typedef struct _GtkAccessibleStateSet   GtkAccessibleStateSet;

GtkAccessibleStateSet * gtk_accessible_state_set_new            (void);
GtkAccessibleStateSet * gtk_accessible_state_set_ref            (GtkAccessibleStateSet *self);
void                    gtk_accessible_state_set_unref          (GtkAccessibleStateSet *self);

void                    gtk_accessible_state_set_add            (GtkAccessibleStateSet *self,
                                                                 GtkAccessibleState     state,
                                                                 GtkAccessibleValue    *value);
void                    gtk_accessible_state_set_remove         (GtkAccessibleStateSet *self,
                                                                 GtkAccessibleState     state);
gboolean                gtk_accessible_state_set_contains       (GtkAccessibleStateSet *self,
                                                                 GtkAccessibleState     state);
GtkAccessibleValue *    gtk_accessible_state_set_get_value      (GtkAccessibleStateSet *self,
                                                                 GtkAccessibleState     state);

void                    gtk_accessible_state_set_print          (GtkAccessibleStateSet *self,
                                                                 gboolean               only_set,
                                                                 GString               *string);
char *                  gtk_accessible_state_set_to_string      (GtkAccessibleStateSet *self);

G_END_DECLS
