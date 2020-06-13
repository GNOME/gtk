/* GtkRBTree tests.
 *
 * Copyright (C) 2011, Red Hat, Inc.
 * Authors: Benjamin Otte <otte@gnome.org>
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

#define LARGE_VALUE (1000 * 1000)

static GtkBitset *
create_powers_of_10 (void)
{
  GtkBitset *set;
  guint i;

  set = gtk_bitset_new_empty ();
  for (i = 1; i <= LARGE_VALUE; i *= 10)
    gtk_bitset_add (set, i);

  return set;
}

static GtkBitset *
create_powers_of_10_ranges (void)
{
  GtkBitset *set;
  guint i, j;

  set = gtk_bitset_new_empty ();
  for (i = 1, j = 0; i <= LARGE_VALUE; i *= 10, j++)
    gtk_bitset_add_range (set, i - j, 2 * j);

  return set;
}

static GtkBitset *
create_large_range (void)
{
  GtkBitset *set;

  set = gtk_bitset_new_empty ();
  gtk_bitset_add_range (set, 0, LARGE_VALUE);

  return set;
}

static GtkBitset *
create_large_rectangle (void)
{
  GtkBitset *set;

  set = gtk_bitset_new_empty ();
  gtk_bitset_add_rectangle (set, 0, 900, 900, 1000);

  return set;
}

static struct {
  GtkBitset * (* create) (void);
  guint n_elements;
  guint minimum;
  guint maximum;
} bitsets[] = 
{
  { gtk_bitset_new_empty, 0, G_MAXUINT, 0 },
  { create_powers_of_10, 7, 1, LARGE_VALUE },
  { create_powers_of_10_ranges, 42, 9, LARGE_VALUE + 5, },
  { create_large_range, LARGE_VALUE, 0, LARGE_VALUE - 1 },
  { create_large_rectangle, 900 * 900, 0, 899899 }
};

/* UTILITIES */

/* TEST */

static void
test_is_empty (void)
{
  guint i;
  GtkBitset *set;

  for (i = 0; i < G_N_ELEMENTS (bitsets); i++)
    {
      set = bitsets[i].create();

      if (bitsets[i].n_elements == 0)
        g_assert_true (gtk_bitset_is_empty (set));
      else
        g_assert_false (gtk_bitset_is_empty (set));
      
      gtk_bitset_unref (set);
    }
}

static void
test_minimum (void)
{
  guint i;
  GtkBitset *set;
  GtkBitsetIter iter;
  gboolean success;
  guint value;

  for (i = 0; i < G_N_ELEMENTS (bitsets); i++)
    {
      set = bitsets[i].create();

      g_assert_cmpint (gtk_bitset_get_minimum (set), ==, bitsets[i].minimum);
      
      success = gtk_bitset_iter_init_first (&iter, set, &value);
      if (success)
        {
          g_assert_false (bitsets[i].n_elements == 0);
          g_assert_cmpint (value, ==, bitsets[i].minimum);
        }
      else
        {
          g_assert_true (bitsets[i].n_elements == 0);
          g_assert_cmpint (value, ==, 0);
        }
      g_assert_cmpint (gtk_bitset_iter_is_valid (&iter), ==, success);
      g_assert_cmpint (gtk_bitset_iter_get_value (&iter), ==, value);

      gtk_bitset_unref (set);
    }
}

static void
test_maximum (void)
{
  guint i;
  GtkBitset *set;
  GtkBitsetIter iter;
  gboolean success;
  guint value;

  for (i = 0; i < G_N_ELEMENTS (bitsets); i++)
    {
      set = bitsets[i].create();

      g_assert_cmpint (gtk_bitset_get_maximum (set), ==, bitsets[i].maximum);
      
      success = gtk_bitset_iter_init_last (&iter, set, &value);
      if (success)
        {
          g_assert_false (bitsets[i].n_elements == 0);
          g_assert_cmpint (value, ==, bitsets[i].maximum);
        }
      else
        {
          g_assert_true (bitsets[i].n_elements == 0);
          g_assert_cmpint (value, ==, 0);
        }
      g_assert_cmpint (gtk_bitset_iter_is_valid (&iter), ==, success);
      g_assert_cmpint (gtk_bitset_iter_get_value (&iter), ==, value);

      gtk_bitset_unref (set);
    }
}

static void
test_equals (void)
{
  guint i, j;
  GtkBitset *iset, *jset;

  for (i = 0; i < G_N_ELEMENTS (bitsets); i++)
    {
      iset = bitsets[i].create();

      g_assert_true (gtk_bitset_equals (iset, iset));

      for (j = 0; j < G_N_ELEMENTS (bitsets); j++)
        {
          jset = bitsets[j].create();

          if (i == j)
            g_assert_true (gtk_bitset_equals (iset, jset));
          else
            g_assert_false (gtk_bitset_equals (iset, jset));

          gtk_bitset_unref (jset);
        }
      
      gtk_bitset_unref (iset);
    }
}

static void
test_union (void)
{
  guint i, j, k, min, max;
  GtkBitset *iset, *jset, *testset;

  for (i = 0; i < G_N_ELEMENTS (bitsets); i++)
    {
      iset = bitsets[i].create();

      g_assert_true (gtk_bitset_equals (iset, iset));

      for (j = 0; j < G_N_ELEMENTS (bitsets); j++)
        {
          jset = bitsets[j].create();

          testset = gtk_bitset_copy (iset);
          gtk_bitset_union (testset, jset);

          min = MIN (gtk_bitset_get_minimum (iset), gtk_bitset_get_minimum (jset));
          g_assert_cmpint (min, <=, gtk_bitset_get_minimum (testset));
          max = MAX (gtk_bitset_get_maximum (iset), gtk_bitset_get_maximum (jset));
          g_assert_cmpint (max, >=, gtk_bitset_get_maximum (testset));

          for (k = min; k <= max; k++)
            {
              g_assert_cmpint (gtk_bitset_contains (iset, k) || gtk_bitset_contains (jset, k), ==, gtk_bitset_contains (testset, k));
            }

          gtk_bitset_unref (testset);
          gtk_bitset_unref (jset);
        }
      
      gtk_bitset_unref (iset);
    }
}

static void
test_intersect (void)
{
  guint i, j, k, min, max;
  GtkBitset *iset, *jset, *testset;

  for (i = 0; i < G_N_ELEMENTS (bitsets); i++)
    {
      iset = bitsets[i].create();

      g_assert_true (gtk_bitset_equals (iset, iset));

      for (j = 0; j < G_N_ELEMENTS (bitsets); j++)
        {
          jset = bitsets[j].create();

          testset = gtk_bitset_copy (iset);
          gtk_bitset_intersect (testset, jset);

          min = MIN (gtk_bitset_get_minimum (iset), gtk_bitset_get_minimum (jset));
          g_assert_cmpint (min, <=, gtk_bitset_get_minimum (testset));
          max = MAX (gtk_bitset_get_maximum (iset), gtk_bitset_get_maximum (jset));
          g_assert_cmpint (max, >=, gtk_bitset_get_maximum (testset));

          for (k = min; k <= max; k++)
            {
              g_assert_cmpint (gtk_bitset_contains (iset, k) && gtk_bitset_contains (jset, k), ==, gtk_bitset_contains (testset, k));
            }

          gtk_bitset_unref (testset);
          gtk_bitset_unref (jset);
        }
      
      gtk_bitset_unref (iset);
    }
}

static void
test_difference (void)
{
  guint i, j, k, min, max;
  GtkBitset *iset, *jset, *testset;

  for (i = 0; i < G_N_ELEMENTS (bitsets); i++)
    {
      iset = bitsets[i].create();

      g_assert_true (gtk_bitset_equals (iset, iset));

      for (j = 0; j < G_N_ELEMENTS (bitsets); j++)
        {
          jset = bitsets[j].create();

          testset = gtk_bitset_copy (iset);
          gtk_bitset_difference (testset, jset);

          min = MIN (gtk_bitset_get_minimum (iset), gtk_bitset_get_minimum (jset));
          g_assert_cmpint (min, <=, gtk_bitset_get_minimum (testset));
          max = MAX (gtk_bitset_get_maximum (iset), gtk_bitset_get_maximum (jset));
          g_assert_cmpint (max, >=, gtk_bitset_get_maximum (testset));

          for (k = min; k <= max; k++)
            {
              g_assert_cmpint (gtk_bitset_contains (iset, k) ^ gtk_bitset_contains (jset, k), ==, gtk_bitset_contains (testset, k));
            }

          gtk_bitset_unref (testset);
          gtk_bitset_unref (jset);
        }
      
      gtk_bitset_unref (iset);
    }
}

static void
test_subtract (void)
{
  guint i, j, k, min, max;
  GtkBitset *iset, *jset, *testset;

  for (i = 0; i < G_N_ELEMENTS (bitsets); i++)
    {
      iset = bitsets[i].create();

      g_assert_true (gtk_bitset_equals (iset, iset));

      for (j = 0; j < G_N_ELEMENTS (bitsets); j++)
        {
          jset = bitsets[j].create();

          testset = gtk_bitset_copy (iset);
          gtk_bitset_subtract (testset, jset);

          min = MIN (gtk_bitset_get_minimum (iset), gtk_bitset_get_minimum (jset));
          g_assert_cmpint (min, <=, gtk_bitset_get_minimum (testset));
          max = MAX (gtk_bitset_get_maximum (iset), gtk_bitset_get_maximum (jset));
          g_assert_cmpint (max, >=, gtk_bitset_get_maximum (testset));

          for (k = min; k <= max; k++)
            {
              g_assert_cmpint (gtk_bitset_contains (iset, k) && !gtk_bitset_contains (jset, k), ==, gtk_bitset_contains (testset, k));
            }

          gtk_bitset_unref (testset);
          gtk_bitset_unref (jset);
        }
      
      gtk_bitset_unref (iset);
    }
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  g_test_add_func ("/bitset/is_empty", test_is_empty);
  g_test_add_func ("/bitset/minimum", test_minimum);
  g_test_add_func ("/bitset/maximum", test_maximum);
  g_test_add_func ("/bitset/equals", test_equals);
  g_test_add_func ("/bitset/union", test_union);
  g_test_add_func ("/bitset/intersect", test_intersect);
  g_test_add_func ("/bitset/difference", test_difference);
  g_test_add_func ("/bitset/subtract", test_subtract);

  return g_test_run ();
}
