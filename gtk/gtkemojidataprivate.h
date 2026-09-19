/* gtkemoji-data.c
 *
 * Copyright 2026 Christian Hergert
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

GBytes   *gtk_emoji_data_load           (void);
char     *gtk_emoji_data_dup_text       (GVariant    *record,
                                         gunichar     modifier);
gboolean  gtk_emoji_data_has_variations (GVariant    *record);
gboolean  gtk_emoji_data_matches        (GVariant    *record,
                                         const char **term_tokens);

G_END_DECLS
