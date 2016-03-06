/* GtkCssRbTree tests.
 *
 * Copyright (C) 2016, Red Hat, Inc.
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

#include "../../gtk/gtkcssrbtreeprivate.h"

#include <string.h>

typedef struct _Element Element;
struct _Element {
  char *value;
};

static void
clear_element (gpointer data)
{
  Element *e = data;

  g_free (e->value);
};

static void
augment (GtkCssRbTree *tree,
         gpointer      aug_data,
         gpointer      data,
         gpointer      ldata,
         gpointer      rdata)
{
  Element *aug = aug_data;
  Element *e = data;
  GString *str = g_string_new (NULL);

  g_free (aug->value);

  if (ldata)
    {
      Element *l = gtk_css_rb_tree_get_augment (tree, ldata);

      g_string_append (str, l->value);
      g_string_append_c (str, ' ');
    }

  g_string_append (str, e->value);

  if (rdata)
    {
      Element *r = gtk_css_rb_tree_get_augment (tree, rdata);

      g_string_append_c (str, ' ');
      g_string_append (str, r->value);
    }

  aug->value = g_string_free (str, FALSE);
}

static GtkCssRbTree *
create_tree (void)
{
  return gtk_css_rb_tree_new (Element,
                              Element,
                              augment,
                              clear_element,
                              clear_element);
}

static void
check_tree (GtkCssRbTree  *tree,
            char         **elements)
{
  Element *e;

  for (e = gtk_css_rb_tree_get_first (tree);
       e;
       e = gtk_css_rb_tree_get_next (tree, e))
    {
      g_assert_cmpstr (e->value, ==, *elements);
      elements++;
    }

  g_assert (*elements == NULL);
}

static void
check_augment (GtkCssRbTree *tree,
               char         *expected)
{
  Element *e, *aug;

  e = gtk_css_rb_tree_get_root (tree);
  aug = gtk_css_rb_tree_get_augment (tree, e);

  g_assert_cmpstr (aug->value, ==, expected);
}

static char *tests[] = {
  "3 20 100",
  "1",
  "1 2",
  "1 2 3",
  "1 2 3 4",
  "1 2 3 4 5",
  "1 2 3 4 5",
  "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 "
    "26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 "
    "51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73 74 75 "
    "76 77 78 79 80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95 96 97 98 99 100"
};

static void
test_insert_after (void)
{
  GtkCssRbTree *tree;
  Element *e;
  guint t, i;

  for (t = 0; t < G_N_ELEMENTS (tests); t++)
    {
      gchar **elements = g_strsplit (tests[t], " ", -1);

      tree = create_tree ();
      e = NULL;

      for (i = 0; i < g_strv_length (elements); i++)
        {
          e = gtk_css_rb_tree_insert_after (tree, e);
          e->value = g_strdup (elements[i]);
        }

      check_tree (tree, elements);
      check_augment (tree, tests[t]);

      gtk_css_rb_tree_unref (tree);
      g_strfreev (elements);
    }
}

static void
test_insert_before (void)
{
  GtkCssRbTree *tree;
  Element *e;
  guint t, i;

  for (t = 0; t < G_N_ELEMENTS (tests); t++)
    {
      gchar **elements = g_strsplit (tests[t], " ", -1);

      tree = create_tree ();
      e = NULL;

      for (i = g_strv_length (elements); i > 0; i--)
        {
          e = gtk_css_rb_tree_insert_before (tree, e);
          e->value = g_strdup (elements[i - 1]);
        }

      check_tree (tree, elements);
      check_augment (tree, tests[t]);

      gtk_css_rb_tree_unref (tree);
      g_strfreev (elements);
    }
}

static int
compare_number_strings (GtkCssRbTree *tree,
                        gpointer      elem,
                        gpointer      data)
{
  Element *e = elem;
  int len_diff;

  len_diff = strlen (e->value) - strlen (data);
  if (len_diff != 0)
    return len_diff;

  return strcmp (e->value, data);
}
               
static void
test_find (void)
{
  GtkCssRbTree *tree;
  Element *e, *before, *after;
  guint t, i;

  for (t = 0; t < G_N_ELEMENTS (tests); t++)
    {
      gchar **elements = g_strsplit (tests[t], " ", -1);

      tree = create_tree ();
      e = NULL;

      for (i = 0; i < g_strv_length (elements); i++)
        {
          e = gtk_css_rb_tree_insert_after (tree, e);
          e->value = g_strdup (elements[i]);
        }

      for (i = 0; i < g_strv_length (elements); i++)
        {
          e = gtk_css_rb_tree_find (tree,
                                    (gpointer *) &before,
                                    (gpointer *) &after,
                                    compare_number_strings,
                                    elements[i]);

          g_assert_cmpstr (e->value, ==, elements[i]);
          if (i == 0)
            g_assert (before == NULL);
          else
            g_assert_cmpstr (before->value, ==, elements[i - 1]);
          if (elements[i + 1] == NULL)
            g_assert (after == NULL);
          else
            g_assert_cmpstr (after->value, ==, elements[i + 1]);
        }

      gtk_css_rb_tree_unref (tree);
      g_strfreev (elements);
    }
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");
  g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=%s");

  g_test_add_func ("/rbtree/insert_after", test_insert_after);
  g_test_add_func ("/rbtree/insert_before", test_insert_before);
  g_test_add_func ("/rbtree/find", test_find);

  return g_test_run ();
}
