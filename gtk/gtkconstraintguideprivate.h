/* gtkconstraintguideprivate.h: Constraint between two widgets
 * Copyright 2019 Red Hat, Inc.
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
 * Author: Matthias Clasen
 */

#pragma once

#include "gtkconstraintguide.h"
#include "gtkconstraintlayout.h"
#include "gtkconstrainttypesprivate.h"

G_BEGIN_DECLS

typedef enum {
  GUIDE_MIN_WIDTH,
  GUIDE_MIN_HEIGHT,
  GUIDE_NAT_WIDTH,
  GUIDE_NAT_HEIGHT,
  GUIDE_MAX_WIDTH,
  GUIDE_MAX_HEIGHT,
  LAST_GUIDE_VALUE
} GuideValue;

struct _GtkConstraintGuide
{
  GObject parent_instance;

  int values[LAST_GUIDE_VALUE];

  GtkConstraintLayout *layout;

  /* HashTable<static string, Variable>; a hash table of variables,
   * one for each attribute; we use these to query and suggest the
   * values for the solver. The string is static and does not need
   * to be freed.
   */
  GHashTable *bound_attributes;

  GtkConstraintRef *constraints[LAST_GUIDE_VALUE];
};

void gtk_constraint_guide_update (GtkConstraintGuide *guide,
                                  GuideValue          index);
void gtk_constraint_guide_detach (GtkConstraintGuide *guide);


G_END_DECLS
