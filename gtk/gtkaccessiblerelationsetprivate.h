/* gtkaccessiblerelationsetprivate.h: Accessible relations set
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

typedef struct _GtkAccessibleRelationSet   GtkAccessibleRelationSet;

GtkAccessibleRelationSet *      gtk_accessible_relation_set_new         (void);
GtkAccessibleRelationSet *      gtk_accessible_relation_set_ref         (GtkAccessibleRelationSet *self);
void                            gtk_accessible_relation_set_unref       (GtkAccessibleRelationSet *self);

void                            gtk_accessible_relation_set_add         (GtkAccessibleRelationSet *self,
                                                                         GtkAccessibleRelation     state,
                                                                         GtkAccessibleValue       *value);
void                            gtk_accessible_relation_set_remove      (GtkAccessibleRelationSet *self,
                                                                         GtkAccessibleRelation     state);
gboolean                        gtk_accessible_relation_set_contains    (GtkAccessibleRelationSet *self,
                                                                         GtkAccessibleRelation     state);
GtkAccessibleValue *            gtk_accessible_relation_set_get_value   (GtkAccessibleRelationSet *self,
                                                                         GtkAccessibleRelation     state);

void                            gtk_accessible_relation_set_print       (GtkAccessibleRelationSet *self,
                                                                         gboolean                  only_set,
                                                                         GString                  *string);
char *                          gtk_accessible_relation_set_to_string   (GtkAccessibleRelationSet *self);

G_END_DECLS
