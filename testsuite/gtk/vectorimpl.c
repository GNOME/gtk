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
      g_assert_cmpint (gtk_vector(get_size) (&v), <=, gtk_vector(get_capacity) (&v));
      gtk_vector(append) (&v, i);
#ifdef GTK_VECTOR_NULL_TERMINATED
      g_assert_cmpint (*gtk_vector(index) (&v, gtk_vector(get_size) (&v)), ==, 0);
#endif
    }
  g_assert_cmpint (gtk_vector(get_size) (&v), ==, i);
  g_assert_cmpint (gtk_vector(get_size) (&v), <=, gtk_vector(get_capacity) (&v));

  for (i = 0; i < 1000; i++)
    {
      g_assert_cmpint (gtk_vector(get) (&v, i), ==, i);
    }

  gtk_vector(clear) (&v);
}

static void
gtk_vector(test_splice) (void)
{
  GtkVector v;
  gsize i, j, sum;
  gsize pos, add, remove;
  int additions[4] = { 0, 1, 2, 3 };

  gtk_vector(init) (&v);
  sum = 0;

  for (i = 0; i < 1000; i++)
    {
      gsize old_size = gtk_vector(get_size) (&v);

      pos = g_random_int_range (0, old_size + 1);
      g_assert (pos <= old_size);
      remove = g_random_int_range (0, 4);
      remove = MIN (remove, old_size - pos);
      add = g_random_int_range (0, 4);

      for (j = 0; j < remove; j++)
        sum -= gtk_vector(get) (&v, pos + j);
      for (j = 0; j < add; j++)
        sum += ++additions[j];

      gtk_vector(splice) (&v, pos, remove, additions, add);
      {
        gsize total = 0;
        for (j = 0; j < gtk_vector(get_size) (&v); j++)
          total += gtk_vector(get) (&v, j);
        g_assert_cmpint (total, ==, sum);
      }

      g_assert_cmpint (gtk_vector(get_size) (&v), ==, old_size + add - remove);
      g_assert_cmpint (gtk_vector(get_size) (&v), <=, gtk_vector(get_capacity) (&v));
#ifdef GTK_VECTOR_NULL_TERMINATED
      if (gtk_vector(get_size) (&v))
        g_assert_cmpint (*gtk_vector(index) (&v, gtk_vector(get_size) (&v)), ==, 0);
#endif
      for (j = 0; j < add; j++)
        g_assert_cmpint (gtk_vector(get) (&v, pos + j), ==, additions[j]);
    }

  for (i = 0; i < gtk_vector(get_size) (&v); i++)
    {
      sum -= gtk_vector(get) (&v, i);
    }
  g_assert_cmpint (sum, ==, 0);
}

#undef _T_
#undef GtkVector
#undef gtk_vector_paste_more
#undef gtk_vector_paste
#undef gtk_vector
#undef GTK_VECTOR_REAL_SIZE

#undef GTK_VECTOR_ELEMENT_TYPE
#undef GTK_VECTOR_NAME
#undef GTK_VECTOR_TYPE_NAME
#undef GTK_VECTOR_PREALLOC
#undef GTK_VECTOR_NULL_TERMINATED

