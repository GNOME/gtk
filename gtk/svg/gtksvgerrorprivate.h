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

#include "gtksvg.h"
#include <glib.h>

G_BEGIN_DECLS

void gtk_svg_error_set_element   (GError               *error,
                                  const char           *element);

void gtk_svg_error_set_attribute (GError               *error,
                                  const char           *attribute);

void gtk_svg_error_set_location  (GError               *error,
                                  const GtkSvgLocation *start,
                                  const GtkSvgLocation *end);

G_END_DECLS
