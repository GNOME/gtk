/* gtkaccessiblepropertysetprivate.h: Accessible property set
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

typedef struct _GtkAccessiblePropertySet   GtkAccessiblePropertySet;

GtkAccessiblePropertySet *      gtk_accessible_property_set_new         (void);
GtkAccessiblePropertySet *      gtk_accessible_property_set_ref         (GtkAccessiblePropertySet *self);
void                            gtk_accessible_property_set_unref       (GtkAccessiblePropertySet *self);

void                            gtk_accessible_property_set_add         (GtkAccessiblePropertySet *self,
                                                                         GtkAccessibleProperty     property,
                                                                         GtkAccessibleValue       *value);
void                            gtk_accessible_property_set_remove      (GtkAccessiblePropertySet *self,
                                                                         GtkAccessibleProperty     property);
gboolean                        gtk_accessible_property_set_contains    (GtkAccessiblePropertySet *self,
                                                                         GtkAccessibleProperty     property);
GtkAccessibleValue *            gtk_accessible_property_set_get_value   (GtkAccessiblePropertySet *self,
                                                                         GtkAccessibleProperty     property);

void                            gtk_accessible_property_set_print       (GtkAccessiblePropertySet *self,
                                                                         gboolean                  only_set,
                                                                         GString                  *string);
char *                          gtk_accessible_property_set_to_string   (GtkAccessiblePropertySet *self);

G_END_DECLS
