/*
 * Copyright © 2025 Red Hat, Inc
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#include "gtk/css/gtkcssparserprivate.h"
#include "gtksvgtypesprivate.h"
#include "gtksvgenumsprivate.h"

G_BEGIN_DECLS

SvgValue *   svg_numbers_new                 (double       *values,
                                              unsigned int  n_values);

SvgValue *   svg_numbers_new_identity_matrix (void);

SvgValue *   svg_numbers_new_none            (void);

SvgValue *   svg_numbers_new1                (double value);

SvgValue *   svg_numbers_new2                (double v1, SvgUnit u1,
                                              double v2, SvgUnit u2);

SvgValue *   svg_numbers_new_00              (void);

SvgValue *   svg_numbers_parse               (GtkCssParser *parser);
SvgValue *   svg_numbers_parse2              (GtkCssParser *parser,
                                              unsigned int  flags);

unsigned int svg_numbers_get_length          (const SvgValue     *value);
SvgUnit      svg_numbers_get_unit            (const SvgValue     *value,
                                              unsigned int        pos);
double       svg_numbers_get                 (const SvgValue     *value,
                                              unsigned int        pos,
                                              double              one_hundred_percent);

SvgValue *   svg_numbers_drop_last           (const SvgValue *value);

G_END_DECLS
