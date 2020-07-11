/*
 * Copyright © 2020 Benjamin Otte
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

#include <locale.h>

#include <gtk/gtk.h>

#include "gtk/gtktimsortprivate.h"

#define assert_sort_equal(a, b, size, n) \
  g_assert_cmpmem (a, sizeof (size) * n, b, sizeof (size) * n)

static int
compare_int (gconstpointer a,
             gconstpointer b,
             gpointer      unused)
{
  int ia = *(const int *) a;
  int ib = *(const int *) b;

  return ia < ib ? -1 : (ia > ib);
}

G_GNUC_UNUSED static void
dump (int *a,
      gsize n)
{
  gsize i;

  for (i = 0; i < n; i++)
    {
      if (i)
        g_print(", ");
      g_print ("%d", a[i]);
    }
  g_print ("\n");
}

static void
run_comparison (gpointer         a,
                gsize            n,
                gsize            element_size,
                GCompareDataFunc compare_func,
                gpointer         data)
{
  gint64 start, mid, end;
  gpointer b;

  b = g_memdup (a, element_size * n);

  start = g_get_monotonic_time ();
  gtk_tim_sort (a, n, element_size, compare_func, data);
  mid = g_get_monotonic_time ();
  g_qsort_with_data (b, n, element_size, compare_func, data);
  end = g_get_monotonic_time ();

  g_test_message ("%zu items in %ldus vs %ldus (%ld%%)", n, mid - start, end - mid, 100 * (mid - start) / MAX (1, end - mid));
  assert_sort_equal (a, b, int, n);

  g_free (b);
}

static void
test_integers (void)
{
  int *a;
  gsize i, n, run;

  a = g_new (int, 1000);

  for (run = 0; run < 10; run++)
    {
      n = g_test_rand_int_range (0, 1000);
      for (i = 0; i < n; i++)
        a[i] = g_test_rand_int ();

      run_comparison (a, n, sizeof (int), compare_int, NULL);
    }

  g_free (a);
}

static void
test_integers_runs (void)
{
  int *a;
  gsize i, j, n, run;

  a = g_new (int, 1000);

  for (run = 0; run < 10; run++)
    {
      n = g_test_rand_int_range (0, 1000);
      for (i = 0; i < n; i++)
        {
          a[i] = g_test_rand_int ();
          j = i + g_test_rand_int_range (0, 20);
          j = MIN (n, j);
          if (g_test_rand_bit ())
            {
              for (i++; i < j; i++)
                a[i] = a[i - 1] + 1;
            }
          else
            {
              for (i++; i < j; i++)
                a[i] = a[i - 1] - 1;
            }
        }

      run_comparison (a, n, sizeof (int), compare_int, NULL);
    }

  g_free (a);
}

static void
test_integers_huge (void)
{
  int *a;
  gsize i, n;

  n = g_test_rand_int_range (5 * 1000 * 1000, 10 * 1000 * 1000);

  a = g_new (int, n);
  for (i = 0; i < n; i++)
    a[i] = g_test_rand_int ();

  run_comparison (a, n, sizeof (int), compare_int, NULL);

  g_free (a);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  g_test_add_func ("/timsort/integers", test_integers);
  g_test_add_func ("/timsort/integers/runs", test_integers_runs);
  g_test_add_func ("/timsort/integers/huge", test_integers_huge);

  return g_test_run ();
}
