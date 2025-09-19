/*
 * Copyright © 2025 Red Hat, Inc
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


#define RANGE_EDITOR_TYPE (range_editor_get_type ())
G_DECLARE_FINAL_TYPE (RangeEditor, range_editor, RANGE, EDITOR, GtkWidget)


RangeEditor * range_editor_new (void);

void          range_editor_get_limits (RangeEditor *self,
                                       float       *lower,
                                       float       *middle,
                                       float       *upper);
void          range_editor_get_values (RangeEditor *self,
                                       float       *value1,
                                       float       *value2);
void          range_editor_configure  (RangeEditor *self,
                                       float        lower,
                                       float        middle,
                                       float        upper,
                                       float        value1,
                                       float        value2);
