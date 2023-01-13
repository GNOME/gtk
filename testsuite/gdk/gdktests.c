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
 */

#include <locale.h>

#include <gtk/gtk.h>

#include "gdktests.h"

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  /* display tests need this */
  if (!g_test_subprocess ())
    gtk_init ();

  add_array_tests ();
  add_cairo_tests ();
  add_content_formats_tests ();
  add_content_serializer_tests ();
  add_cursor_tests ();
  add_display_tests ();
  add_display_manager_tests ();
  add_encoding_tests ();
  add_glcontext_tests ();
  add_keysyms_tests ();
  add_memory_texture_tests ();
  add_pixbuf_tests ();
  add_rectangle_tests ();
  add_rgba_tests ();
  add_seat_tests ();
  add_texture_threads_tests ();

  return g_test_run ();
}
