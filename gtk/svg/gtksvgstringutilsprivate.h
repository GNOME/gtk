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
#include <graphene.h>

G_BEGIN_DECLS

void string_append_double  (GString                *string,
                            const char             *prefix,
                            double                  value);

void string_append_point   (GString                *string,
                            const char             *prefix,
                            const graphene_point_t *point);

void string_append_base64  (GString                *string,
                            GBytes                 *bytes);

void string_append_escaped (GString                *string,
                            const char             *text);

void string_append_strv    (GString                *string,
                            GStrv                   strv);

void string_indent         (GString                *string,
                            int                     indent);

G_END_DECLS
