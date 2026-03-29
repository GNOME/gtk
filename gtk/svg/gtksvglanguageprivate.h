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

#include <glib.h>
#include "gtksvgtypesprivate.h"
#include "gtksvgenumsprivate.h"
#include <pango/pango.h>

G_BEGIN_DECLS

SvgValue *     svg_language_new_list    (unsigned int    len,
                                         PangoLanguage **langs);
SvgValue *     svg_language_new         (PangoLanguage  *language);
SvgValue *     svg_language_new_default (void);

unsigned int   svg_language_get_length  (const SvgValue *value);
PangoLanguage *svg_language_get         (const SvgValue *value,
                                         unsigned int    pos);

G_END_DECLS
