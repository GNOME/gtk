/*
 * Copyright © 2020 Benjamin Otte
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

#include <gtk/gtk.h>

#define GTK_VECTOR_NO_UNDEF

#include "../../gtk/gtkvectorimpl.c"

static void
gtk_vector(test_simple) (void)
{
  GtkVector v;
  gsize i;

  gtk_vector(init) (&v);

  for (i = 0; i < 1000; i++)
    {
      g_assert_cmpint (gtk_vector(get_size) (&v), ==, i);
      gtk_vector(append) (&v, i);
    }
  g_assert_cmpint (gtk_vector(get_size) (&v), ==, i);

  for (i = 0; i < 1000; i++)
    {
      g_assert_cmpint (gtk_vector(get) (&v, i), ==, i);
    }

  gtk_vector(clear) (&v);
}

#undef _T_
#undef GtkVector
#undef gtk_vector_paste_more
#undef gtk_vector_paste
#undef gtk_vector

#undef GTK_VECTOR_PREALLOC
#undef GTK_VECTOR_ELEMENT_TYPE
#undef GTK_VECTOR_NAME
#undef GTK_VECTOR_TYPE_NAME

