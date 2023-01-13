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

#include "config.h"

#include <gtk/gtk.h>

#include "gskinternaltests.h"

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  add_diff_tests ();
  add_half_float_tests ();
  add_rounded_rect_tests ();
  add_misc_tests ();

  return g_test_run ();
}
