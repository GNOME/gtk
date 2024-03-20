/*
 * Copyright (C) 2023 GNOME Foundation Inc.
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
 * Authors: Alice Mikhaylenko <alicem@gnome.org>
 */

#pragma once

#include <gtk/css/gtkcss.h>
#include "gtkcssvalueprivate.h"
#include "gtkstylepropertyprivate.h"
#include "css/gtkcssvariablevalueprivate.h"

G_BEGIN_DECLS

GtkCssValue *_gtk_css_reference_value_new             (GtkStyleProperty    *property,
                                                       GtkCssVariableValue *value,
                                                       GFile               *file);
void         _gtk_css_reference_value_set_subproperty (GtkCssValue         *value,
                                                       guint                property);

G_END_DECLS
