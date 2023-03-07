/*
 * Copyright © 2023 Matthias Clasen
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#pragma once

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GTK_TYPE_THROBBER (gtk_throbber_get_type ())

G_DECLARE_FINAL_TYPE (GtkThrobber, gtk_throbber, GTK, THROBBER, GObject)

GdkPaintable *  gtk_throbber_new (GdkFrameClock *clock,
                                  const char    *resource_path,
                                  float          min,
                                  float          max,
                                  GdkDrag       *drag);

G_END_DECLS
