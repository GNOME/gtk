/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
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

#include "gtkcssparserprivate.h"

void
_gtk_css_parser_error (GtkCssParser *parser,
                       const char   *format,
                       ...)
{
  GtkCssLocation location;
  va_list args;
  GError *error;

  gtk_css_parser_get_location (parser, &location);
  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PARSER_ERROR,
                              GTK_CSS_PARSER_ERROR_FAILED,
                              format, args);
  gtk_css_parser_emit_error (parser, &location, &location, error);
  g_error_free (error);
  va_end (args);
}

void
_gtk_css_print_string (GString    *str,
                       const char *string)
{
  gsize len;

  g_return_if_fail (str != NULL);
  g_return_if_fail (string != NULL);

  g_string_append_c (str, '"');

  do {
    len = strcspn (string, "\\\"\n\r\f");
    g_string_append_len (str, string, len);
    string += len;
    switch (*string)
      {
      case '\0':
        goto out;
      case '\n':
        g_string_append (str, "\\A ");
        break;
      case '\r':
        g_string_append (str, "\\D ");
        break;
      case '\f':
        g_string_append (str, "\\C ");
        break;
      case '\"':
        g_string_append (str, "\\\"");
        break;
      case '\\':
        g_string_append (str, "\\\\");
        break;
      default:
        g_assert_not_reached ();
        break;
      }
    string++;
  } while (*string);

out:
  g_string_append_c (str, '"');
}

