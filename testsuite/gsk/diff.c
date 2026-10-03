/* GTK - The GIMP Toolkit
 * Copyright 2026 Christian Hergert
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

#undef G_DISABLE_ASSERT

#include "gsk/gskdiffprivate.h"

typedef struct
{
  GString *trace;
  gsize n_compares;
  gsize n_callbacks;
  gsize abort_after;
  gboolean reject_identical_compare;
} TestData;

static int
compare_element (gconstpointer elem1,
                 gconstpointer elem2,
                 gpointer      user_data)
{
  TestData *data = user_data;

  if (data->reject_identical_compare)
    g_assert (elem1 != elem2);

  data->n_compares++;

  return GPOINTER_TO_INT (elem1) != GPOINTER_TO_INT (elem2);
}

static int
compare_int_element (gconstpointer elem1,
                     gconstpointer elem2,
                     gpointer      user_data)
{
  TestData *data = user_data;

  data->n_compares++;

  return *(const int *) elem1 != *(const int *) elem2;
}

static GskDiffResult
finish_callback (TestData *data)
{
  data->n_callbacks++;

  if (data->abort_after != 0 && data->n_callbacks == data->abort_after)
    return GSK_DIFF_ABORTED;

  return GSK_DIFF_OK;
}

static GskDiffResult
keep_element (gconstpointer elem1,
              gconstpointer elem2,
              gpointer      user_data)
{
  TestData *data = user_data;

  g_string_append_printf (data->trace,
                          "K%d:%d;",
                          GPOINTER_TO_INT (elem1),
                          GPOINTER_TO_INT (elem2));

  return finish_callback (data);
}

static GskDiffResult
delete_element (gconstpointer elem,
                gsize         index,
                gpointer      user_data)
{
  TestData *data = user_data;

  g_string_append_printf (data->trace,
                          "D%d@%" G_GSIZE_FORMAT ";",
                          GPOINTER_TO_INT (elem),
                          index);

  return finish_callback (data);
}

static GskDiffResult
insert_element (gconstpointer elem,
                gsize         index,
                gpointer      user_data)
{
  TestData *data = user_data;

  g_string_append_printf (data->trace,
                          "I%d@%" G_GSIZE_FORMAT ";",
                          GPOINTER_TO_INT (elem),
                          index);

  return finish_callback (data);
}

static GskDiffResult
run_diff_full (gconstpointer   *elem1,
               gsize            n1,
               gconstpointer   *elem2,
               gsize            n2,
               GCompareDataFunc compare_func,
               gboolean         defer_callbacks,
               TestData        *data)
{
  GskDiffSettings *settings;
  GskDiffResult result;

  settings = gsk_diff_settings_new (compare_func,
                                    keep_element,
                                    delete_element,
                                    insert_element);
  gsk_diff_settings_set_allow_abort (settings, TRUE);
  gsk_diff_settings_set_defer_callbacks (settings, defer_callbacks);

  result = gsk_diff (elem1, n1, elem2, n2, settings, data);

  gsk_diff_settings_free (settings);

  return result;
}

static GskDiffResult
run_diff (gconstpointer *elem1,
          gsize          n1,
          gconstpointer *elem2,
          gsize          n2,
          gboolean       defer_callbacks,
          TestData      *data)
{
  return run_diff_full (elem1, n1, elem2, n2,
                        compare_element,
                        defer_callbacks,
                        data);
}

static void
assert_deferred_trace (gconstpointer *elem1,
                       gsize          n1,
                       gconstpointer *elem2,
                       gsize          n2,
                       gsize          abort_after)
{
  TestData direct = {
    .trace = g_string_new (NULL),
    .abort_after = abort_after,
  };
  TestData deferred = {
    .trace = g_string_new (NULL),
    .abort_after = abort_after,
  };
  GskDiffResult direct_result;
  GskDiffResult deferred_result;

  direct_result = run_diff (elem1, n1, elem2, n2, FALSE, &direct);
  deferred_result = run_diff (elem1, n1, elem2, n2, TRUE, &deferred);

  g_assert_cmpint (deferred_result, ==, direct_result);
  if (direct_result == GSK_DIFF_OK || abort_after != 0)
    g_assert_cmpstr (deferred.trace->str, ==, direct.trace->str);
  else
    g_assert_cmpuint (deferred.n_callbacks, ==, 0);

  g_string_free (direct.trace, TRUE);
  g_string_free (deferred.trace, TRUE);
}

static void
test_deferred_callbacks (void)
{
  gconstpointer elem1[] = {
    GINT_TO_POINTER (1), GINT_TO_POINTER (2), GINT_TO_POINTER (3),
    GINT_TO_POINTER (4), GINT_TO_POINTER (5), GINT_TO_POINTER (6),
    GINT_TO_POINTER (7), GINT_TO_POINTER (8), GINT_TO_POINTER (9),
    GINT_TO_POINTER (10), GINT_TO_POINTER (13), GINT_TO_POINTER (14),
  };
  gconstpointer elem2[] = {
    GINT_TO_POINTER (1), GINT_TO_POINTER (3), GINT_TO_POINTER (4),
    GINT_TO_POINTER (11), GINT_TO_POINTER (5), GINT_TO_POINTER (7),
    GINT_TO_POINTER (8), GINT_TO_POINTER (10), GINT_TO_POINTER (12),
    GINT_TO_POINTER (13), GINT_TO_POINTER (14),
  };

  assert_deferred_trace (elem1, G_N_ELEMENTS (elem1),
                         elem2, G_N_ELEMENTS (elem2),
                         0);
  assert_deferred_trace (elem1, G_N_ELEMENTS (elem1),
                         elem2, G_N_ELEMENTS (elem2),
                         4);
}

static void
test_direct_below_abort_limit (void)
{
  gconstpointer elem1[] = {
    GINT_TO_POINTER (1), GINT_TO_POINTER (2), GINT_TO_POINTER (3),
    GINT_TO_POINTER (4), GINT_TO_POINTER (5), GINT_TO_POINTER (6),
    GINT_TO_POINTER (7), GINT_TO_POINTER (8), GINT_TO_POINTER (9),
  };
  gconstpointer elem2[] = {
    GINT_TO_POINTER (1), GINT_TO_POINTER (12), GINT_TO_POINTER (13),
    GINT_TO_POINTER (14), GINT_TO_POINTER (15), GINT_TO_POINTER (16),
    GINT_TO_POINTER (17), GINT_TO_POINTER (18), GINT_TO_POINTER (19),
  };
  TestData data = {
    .trace = g_string_new (NULL),
    .abort_after = 1,
  };
  GskDiffResult result;

  result = run_diff (elem1, G_N_ELEMENTS (elem1),
                     elem2, G_N_ELEMENTS (elem2),
                     TRUE, &data);
  g_assert_cmpint (result, ==, GSK_DIFF_ABORTED);
  g_assert_cmpuint (data.n_callbacks, ==, 1);

  g_string_free (data.trace, TRUE);
}

static void
test_deferred_structural_abort (void)
{
  int values1[240];
  int values2[240];
  gconstpointer elem1[240];
  gconstpointer elem2[240];
  TestData direct = { .trace = g_string_new (NULL) };
  TestData deferred = { .trace = g_string_new (NULL) };
  GskDiffResult result;
  gsize i;

  for (i = 0; i < 40; i++)
    {
      values1[i] = i + 1;
      values2[i] = i + 1001;
    }

  for (; i < G_N_ELEMENTS (elem1); i++)
    values1[i] = values2[i] = i + 2001;

  for (i = 0; i < G_N_ELEMENTS (elem1); i++)
    {
      elem1[i] = &values1[i];
      elem2[i] = &values2[i];
    }

  result = run_diff_full (elem1, G_N_ELEMENTS (elem1),
                          elem2, G_N_ELEMENTS (elem2),
                          compare_int_element,
                          FALSE, &direct);
  g_assert_cmpint (result, ==, GSK_DIFF_ABORTED);
  g_assert_cmpuint (direct.n_callbacks, ==, 200);

  result = run_diff_full (elem1, G_N_ELEMENTS (elem1),
                          elem2, G_N_ELEMENTS (elem2),
                          compare_int_element,
                          TRUE, &deferred);
  g_assert_cmpint (result, ==, GSK_DIFF_ABORTED);
  g_assert_cmpuint (deferred.n_callbacks, ==, 0);

  g_string_free (direct.trace, TRUE);
  g_string_free (deferred.trace, TRUE);
}

static void
test_identical (void)
{
  gconstpointer elements[256];
  gconstpointer elem1[18];
  gconstpointer elem2[18];
  TestData data = {
    .trace = g_string_new (NULL),
    .reject_identical_compare = TRUE,
  };
  GskDiffResult result;
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (elements); i++)
    elements[i] = GINT_TO_POINTER (i + 1);

  result = run_diff (elements, G_N_ELEMENTS (elements),
                     elements, G_N_ELEMENTS (elements),
                     TRUE, &data);
  g_assert_cmpint (result, ==, GSK_DIFF_OK);
  g_assert_cmpuint (data.n_compares, ==, 0);
  g_assert_cmpuint (data.n_callbacks, ==, 0);

  data.n_compares = 0;
  elem1[0] = GINT_TO_POINTER (1000);
  elem2[0] = GINT_TO_POINTER (2000);
  for (i = 1; i < G_N_ELEMENTS (elem1) - 1; i++)
    elem1[i] = elem2[i] = GINT_TO_POINTER (i);
  elem1[G_N_ELEMENTS (elem1) - 1] = GINT_TO_POINTER (3000);
  elem2[G_N_ELEMENTS (elem2) - 1] = GINT_TO_POINTER (4000);

  result = run_diff (elem1, G_N_ELEMENTS (elem1),
                     elem2, G_N_ELEMENTS (elem2),
                     TRUE, &data);
  g_assert_cmpint (result, ==, GSK_DIFF_OK);
  g_assert_cmpuint (data.n_callbacks, ==, 4);

  g_string_free (data.trace, TRUE);
}

static void
test_pointer_distinct_equal (void)
{
  int values1[256];
  int values2[256];
  gconstpointer elem1[G_N_ELEMENTS (values1)];
  gconstpointer elem2[G_N_ELEMENTS (values2)];
  TestData data = { .trace = g_string_new (NULL) };
  GskDiffResult result;
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (values1); i++)
    {
      values1[i] = i;
      values2[i] = i;
      elem1[i] = &values1[i];
      elem2[i] = &values2[i];
    }

  result = run_diff_full (elem1, G_N_ELEMENTS (elem1),
                          elem2, G_N_ELEMENTS (elem2),
                          compare_int_element,
                          TRUE, &data);
  g_assert_cmpint (result, ==, GSK_DIFF_OK);
  g_assert_cmpuint (data.n_compares, ==, G_N_ELEMENTS (elem1));
  g_assert_cmpuint (data.n_callbacks, ==, G_N_ELEMENTS (elem1));

  g_string_free (data.trace, TRUE);
}

static void
test_trimmed_callback_indexes (void)
{
  gconstpointer elem1[] = {
    GINT_TO_POINTER (1), GINT_TO_POINTER (2), GINT_TO_POINTER (3),
  };
  gconstpointer elem2[] = {
    GINT_TO_POINTER (1), GINT_TO_POINTER (2), GINT_TO_POINTER (4),
  };
  TestData data = {
    .trace = g_string_new (NULL),
    .reject_identical_compare = TRUE,
  };
  GskDiffResult result;

  result = run_diff (elem1, G_N_ELEMENTS (elem1),
                     elem2, G_N_ELEMENTS (elem2),
                     TRUE, &data);
  g_assert_cmpint (result, ==, GSK_DIFF_OK);
  g_assert_cmpstr (data.trace->str, ==, "D3@2;I4@2;");

  g_string_free (data.trace, TRUE);
}

static void
test_recursive_identical_keeps (void)
{
  gconstpointer elem1[] = {
    GINT_TO_POINTER (1), GINT_TO_POINTER (10), GINT_TO_POINTER (2),
    GINT_TO_POINTER (20), GINT_TO_POINTER (3),
  };
  gconstpointer elem2[] = {
    GINT_TO_POINTER (1), GINT_TO_POINTER (11), GINT_TO_POINTER (2),
    GINT_TO_POINTER (21), GINT_TO_POINTER (3),
  };
  TestData data = {
    .trace = g_string_new (NULL),
    .reject_identical_compare = TRUE,
  };
  GskDiffResult result;

  result = run_diff (elem1, G_N_ELEMENTS (elem1),
                     elem2, G_N_ELEMENTS (elem2),
                     TRUE, &data);
  g_assert_cmpint (result, ==, GSK_DIFF_OK);
  g_assert_cmpuint (data.n_callbacks, ==, 4);
  g_assert_null (strstr (data.trace->str, "K"));

  g_string_free (data.trace, TRUE);
}

static void
test_boundary_snakes (void)
{
  gconstpointer elem1[64];
  gconstpointer elem2[65];
  TestData data = {
    .trace = g_string_new (NULL),
    .reject_identical_compare = TRUE,
  };
  GskDiffResult result;
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (elem1); i++)
    elem1[i] = elem2[i] = GSIZE_TO_POINTER (i + 1);
  elem2[G_N_ELEMENTS (elem1)] = GSIZE_TO_POINTER (1000);

  result = run_diff (elem1, G_N_ELEMENTS (elem1),
                     elem2, G_N_ELEMENTS (elem2),
                     TRUE, &data);
  g_assert_cmpint (result, ==, GSK_DIFF_OK);
  g_assert_cmpstr (data.trace->str, ==, "I1000@64;");
  g_assert_cmpuint (data.n_compares, ==, 0);

  g_string_truncate (data.trace, 0);
  data.n_callbacks = 0;
  elem2[0] = GSIZE_TO_POINTER (1000);
  for (i = 0; i < G_N_ELEMENTS (elem1); i++)
    elem2[i + 1] = elem1[i];

  result = run_diff (elem1, G_N_ELEMENTS (elem1),
                     elem2, G_N_ELEMENTS (elem2),
                     TRUE, &data);
  g_assert_cmpint (result, ==, GSK_DIFF_OK);
  g_assert_cmpstr (data.trace->str, ==, "I1000@0;");
  g_assert_cmpuint (data.n_compares, ==, 0);

  g_string_free (data.trace, TRUE);
}

static void
test_bounded_workspace_large_offset (void)
{
  gconstpointer *elem1;
  gconstpointer *elem2;
  TestData data = { .trace = g_string_new (NULL) };
  GskDiffResult result;
  gsize i;

  elem1 = g_new (gconstpointer, 4096);
  elem2 = g_new (gconstpointer, 4097);

  for (i = 0; i < 4096; i++)
    elem1[i] = GINT_TO_POINTER (i + 1);

  for (i = 0; i < 1000; i++)
    elem2[i] = elem1[i];
  elem2[1000] = GINT_TO_POINTER (5000);
  for (; i < 4096; i++)
    elem2[i + 1] = elem1[i];
  elem2[3001] = GINT_TO_POINTER (6000);

  result = run_diff (elem1, 4096, elem2, 4097, TRUE, &data);
  g_assert_cmpint (result, ==, GSK_DIFF_OK);

  g_free (elem1);
  g_free (elem2);
  g_string_free (data.trace, TRUE);
}

static void
test_bounded_workspace_budget_boundary (void)
{
  gconstpointer elem1[41];
  gconstpointer elem2[41];
  TestData data = { .trace = g_string_new (NULL) };
  GskDiffResult result;
  gsize i;

  for (i = 0; i < 20; i++)
    {
      elem1[2 * i] = elem2[2 * i] = GINT_TO_POINTER (i + 1);
      elem1[2 * i + 1] = GINT_TO_POINTER (i + 100);
      elem2[2 * i + 1] = GINT_TO_POINTER (i + 200);
    }
  elem1[40] = elem2[40] = GINT_TO_POINTER (21);

  result = run_diff (elem1, G_N_ELEMENTS (elem1),
                     elem2, G_N_ELEMENTS (elem2),
                     TRUE, &data);
  g_assert_cmpint (result, ==, GSK_DIFF_OK);
  g_assert_cmpuint (data.n_callbacks, ==, 40);

  g_string_free (data.trace, TRUE);
}

static void
test_randomized_deferred_trace (void)
{
  GRand *random;
  guint iteration;

  random = g_rand_new_with_seed (0x5EED);

  for (iteration = 0; iteration < 10000; iteration++)
    {
      gconstpointer elem1[64];
      gconstpointer elem2[64];
      gsize n1 = g_rand_int_range (random, 0, G_N_ELEMENTS (elem1) + 1);
      gsize n2 = g_rand_int_range (random, 0, G_N_ELEMENTS (elem2) + 1);
      gsize i;

      for (i = 0; i < n1; i++)
        elem1[i] = GINT_TO_POINTER (g_rand_int_range (random, 1, 17));
      for (i = 0; i < n2; i++)
        elem2[i] = GINT_TO_POINTER (g_rand_int_range (random, 1, 17));

      assert_deferred_trace (elem1, n1, elem2, n2, 0);
    }

  g_rand_free (random);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gsk/diff/deferred-callbacks", test_deferred_callbacks);
  g_test_add_func ("/gsk/diff/direct-below-abort-limit", test_direct_below_abort_limit);
  g_test_add_func ("/gsk/diff/deferred-structural-abort", test_deferred_structural_abort);
  g_test_add_func ("/gsk/diff/identical", test_identical);
  g_test_add_func ("/gsk/diff/pointer-distinct-equal", test_pointer_distinct_equal);
  g_test_add_func ("/gsk/diff/trimmed-callback-indexes", test_trimmed_callback_indexes);
  g_test_add_func ("/gsk/diff/recursive-identical-keeps", test_recursive_identical_keeps);
  g_test_add_func ("/gsk/diff/boundary-snakes", test_boundary_snakes);
  g_test_add_func ("/gsk/diff/bounded-workspace-large-offset", test_bounded_workspace_large_offset);
  g_test_add_func ("/gsk/diff/bounded-workspace-budget-boundary",
                   test_bounded_workspace_budget_boundary);
  g_test_add_func ("/gsk/diff/randomized-deferred-trace", test_randomized_deferred_trace);

  return g_test_run ();
}
