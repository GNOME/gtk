/* GSK - The GIMP Toolkit
 * Copyright (C) 2019 Benjamin Otte <otte@gnome.org>
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

#include "config.h"

#include "gtkcsslocationprivate.h"

/**
 * GtkCssLocation:
 * @bytes: number of bytes parsed since the beginning
 * @chars: number of characters parsed since the beginning
 * @lines: number of full lines that have been parsed
 *     If you want to display this as a line number, you
 *     need to add 1 to this.
 * @line_bytes: Number of bytes parsed since the last line break
 * @line_chars: Number of characters parsed since the last line
 *     break
 *
 * Represents a location in a file or other source of data parsed
 * by the CSS engine.
 *
 * The @bytes and @line_bytes offsets are meant to be used to
 * programmatically match data. The @lines and @line_chars offsets
 * can be used for printing the location in a file.
 *
 * Note that the @lines parameter starts from 0 and is increased
 * whenever a CSS line break is encountered. (CSS defines the C character
 * sequences "\r\n", "\r", "\n" and "\f" as newlines.)
 * If your document uses different rules for line breaking, you might want
 * run into problems here.
 */

void
gtk_css_location_init (GtkCssLocation *location)
{
  memset (location, 0, sizeof (GtkCssLocation));
}

void
gtk_css_location_advance (GtkCssLocation *location,
                          gsize           bytes,
                          gsize           chars)
{
  location->bytes += bytes;
  location->chars += chars;
  location->line_bytes += bytes;
  location->line_chars += chars;
}

void
gtk_css_location_advance_newline (GtkCssLocation *location,
                                  gboolean        is_windows)
{
  gtk_css_location_advance (location, is_windows ? 2 : 1, is_windows ? 2 : 1);

  location->lines++;
  location->line_bytes = 0;
  location->line_chars = 0;
}

