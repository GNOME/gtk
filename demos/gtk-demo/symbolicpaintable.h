/*
 * Copyright (c) 2025 RedHat, Inc
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
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SYMBOLIC_TYPE_PAINTABLE (symbolic_paintable_get_type ())

G_DECLARE_FINAL_TYPE (SymbolicPaintable, symbolic_paintable, SYMBOLIC, PAINTABLE, GObject)

SymbolicPaintable * symbolic_paintable_new   (GFile *file);

G_END_DECLS
