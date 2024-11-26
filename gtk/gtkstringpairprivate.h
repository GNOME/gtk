/*
 * Copyright (C) 2006 Alexander Larsson  <alexl@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>

#define GTK_TYPE_STRING_PAIR (gtk_string_pair_get_type ())

G_DECLARE_FINAL_TYPE (GtkStringPair, gtk_string_pair, GTK, STRING_PAIR, GObject)

GtkStringPair * gtk_string_pair_new        (const char    *id,
                                            const char    *string);

const char *    gtk_string_pair_get_id     (GtkStringPair *pair);

const char *    gtk_string_pair_get_string (GtkStringPair *pair);


