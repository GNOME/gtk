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

void                   gtk_constraint_guide_update        (GtkConstraintGuide     *guide);
void                   gtk_constraint_guide_detach        (GtkConstraintGuide     *guide);

GtkConstraintVariable *gtk_constraint_guide_get_attribute (GtkConstraintGuide      *guide,
                                                           GtkConstraintAttribute  attr);

GtkConstraintLayout   *gtk_constraint_guide_get_layout    (GtkConstraintGuide     *guide);
void                   gtk_constraint_guide_set_layout    (GtkConstraintGuide     *guide,
                                                           GtkConstraintLayout    *layout);

G_END_DECLS
