/*
 * Copyright © 2026 Red Hat, Inc
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#include <gtk/gtk.h>
#include "svg/gtksvgunitprivate.h"

G_DECLARE_FINAL_TYPE (NumberEditor, number_editor, NUMBER, EDITOR, GtkWidget)


NumberEditor *  number_editor_new (void);

double          number_editor_get_min   (NumberEditor *self);
void            number_editor_set_min   (NumberEditor *self,
                                         double        min);
double          number_editor_get_max   (NumberEditor *self);
void            number_editor_set_max   (NumberEditor *self,
                                         double        max);
double          number_editor_get_value (NumberEditor *self);
void            number_editor_set_value (NumberEditor *self,
                                         double        value);
SvgUnit         number_editor_get_unit  (NumberEditor *self);
void            number_editor_set_unit  (NumberEditor *self,
                                         SvgUnit       unit);

void            number_editor_get       (NumberEditor *self,
                                         double       *value,
                                         SvgUnit      *unit);
void            number_editor_set       (NumberEditor *self,
                                         double        value,
                                         SvgUnit       unit);
