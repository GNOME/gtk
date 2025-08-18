/* Filterlistmodel tests.
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

static GQuark number_quark;
static GQuark changes_quark;

static guint
get (GListModel *model,
     guint       position)
{
  GObject *object = g_list_model_get_item (model, position);
  guint number;
  g_assert_nonnull (object);
  number = GPOINTER_TO_UINT (g_object_get_qdata (object, number_quark));
  g_object_unref (object);
  return number;
}

static char *
model_to_string (GListModel *model)
{
  GString *string = g_string_new (NULL);
  guint i;

  for (i = 0; i < g_list_model_get_n_items (model); i++)
    {
      if (i > 0)
        g_string_append (string, " ");
      g_string_append_printf (string, "%u", get (model, i));
    }

  return g_string_free (string, FALSE);
}

static GListStore *
new_store (guint start,
           guint end,
           guint step);

static void
add (GListStore *store,
     guint       number)
{
  GObject *object;

  /* 0 cannot be differentiated from NULL, so don't use it */
  g_assert_cmpint (number, !=, 0);

  object = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_set_qdata (object, number_quark, GUINT_TO_POINTER (number));
  g_list_store_append (store, object);
  g_object_unref (object);
}

#define assert_model(model, expected) G_STMT_START{ \
  char *s = model_to_string (G_LIST_MODEL (model)); \
  if (!g_str_equal (s, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " == " #expected, s, "==", expected); \
  g_free (s); \
}G_STMT_END

#define assert_changes(model, expected) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), changes_quark); \
  if (!g_str_equal (changes->str, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " == " #expected, changes->str, "==", expected); \
  g_string_set_size (changes, 0); \
}G_STMT_END

#define ignore_changes(model) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), changes_quark); \
  g_string_set_size (changes, 0); \
}G_STMT_END

static GListStore *
new_empty_store (void)
{
  return g_list_store_new (G_TYPE_OBJECT);
}

static GListStore *
new_store (guint start,
           guint end,
           guint step)
{
  GListStore *store = new_empty_store ();
  guint i;

  for (i = start; i <= end; i += step)
    add (store, i);

  return store;
}

static void
items_changed (GListModel *model,
               guint       position,
               guint       removed,
               guint       added,
               GString    *changes)
{
  g_assert_true (removed != 0 || added != 0);

  if (changes->len)
    g_string_append (changes, ", ");

  if (removed == 1 && added == 0)
    {
      g_string_append_printf (changes, "-%u", position);
    }
  else if (removed == 0 && added == 1)
    {
      g_string_append_printf (changes, "+%u", position);
    }
  else
    {
      g_string_append_printf (changes, "%u", position);
      if (removed > 0)
        g_string_append_printf (changes, "-%u", removed);
      if (added > 0)
        g_string_append_printf (changes, "+%u", added);
    }
}

static void
notify_n_items (GObject    *object,
                GParamSpec *pspec,
                GString    *changes)
{
  g_string_append_c (changes, '*');
}

static void
free_changes (gpointer data)
{
  GString *changes = data;

  /* all changes must have been checked via assert_changes() before */
  g_assert_cmpstr (changes->str, ==, "");

  g_string_free (changes, TRUE);
}

static GtkFilterListModel *
new_model (guint               size,
           GtkCustomFilterFunc filter_func,
           gpointer            data)
{
  GtkFilterListModel *result;
  GtkFilter *filter;
  GString *changes;

  if (filter_func)
    filter = GTK_FILTER (gtk_custom_filter_new (filter_func, data, NULL));
  else
    filter = NULL;
  result = gtk_filter_list_model_new (g_object_ref (G_LIST_MODEL (new_store (1, size, 1))), filter);
  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT(result), changes_quark, changes, free_changes);
  g_signal_connect (result, "items-changed", G_CALLBACK (items_changed), changes);
  g_signal_connect (result, "notify::n-items", G_CALLBACK (notify_n_items), changes);

  return result;
}

static gboolean
is_smaller_than (gpointer item,
                 gpointer data)
{
  return g_object_get_qdata (item, number_quark) < data;
}

static gboolean
is_larger_than (gpointer item,
                gpointer data)
{
  return g_object_get_qdata (item, number_quark) > data;
}

static gboolean
is_near (gpointer item,
         gpointer data)
{
  return ABS (GPOINTER_TO_INT (g_object_get_qdata (item, number_quark)) - GPOINTER_TO_INT (data)) <= 2;
}

static gboolean
is_not_near (gpointer item,
             gpointer data)
{
  return ABS (GPOINTER_TO_INT (g_object_get_qdata (item, number_quark)) - GPOINTER_TO_INT (data)) > 2;
}

static void
test_create (void)
{
  GtkFilterListModel *filter;

  filter = new_model (10, NULL, NULL);
  assert_model (filter, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (filter, "");

  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (filter)) == G_TYPE_OBJECT);
  g_assert_false (gtk_filter_list_model_get_incremental (filter));
  g_assert_null (gtk_filter_list_model_get_filter (filter));

  gtk_filter_list_model_set_model (GTK_FILTER_LIST_MODEL (filter), NULL);
  assert_model (filter, "");
  assert_changes (filter, "0-10*");

  g_object_unref (filter);

  filter = new_model (10, is_smaller_than, GUINT_TO_POINTER (20));
  assert_model (filter, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (filter, "");
  g_object_unref (filter);

  filter = new_model (10, is_smaller_than, GUINT_TO_POINTER (7));
  assert_model (filter, "1 2 3 4 5 6");
  assert_changes (filter, "");
  g_object_unref (filter);

  filter = new_model (10, is_smaller_than, GUINT_TO_POINTER (0));
  assert_model (filter, "");
  assert_changes (filter, "");
  g_object_unref (filter);
}

static void
test_empty_set_filter (void)
{
  GtkFilterListModel *filter;
  GtkFilter *custom;

  filter = new_model (10, NULL, NULL);
  custom = GTK_FILTER (gtk_custom_filter_new (is_smaller_than, GUINT_TO_POINTER (20), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (filter, "");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  custom = GTK_FILTER (gtk_custom_filter_new (is_smaller_than, GUINT_TO_POINTER (7), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "1 2 3 4 5 6");
  assert_changes (filter, "6-4*");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  custom = GTK_FILTER (gtk_custom_filter_new (is_smaller_than, GUINT_TO_POINTER (0), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "");
  assert_changes (filter, "0-10*");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  custom = GTK_FILTER (gtk_custom_filter_new (is_larger_than, GUINT_TO_POINTER (0), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (filter, "");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  custom = GTK_FILTER (gtk_custom_filter_new (is_larger_than, GUINT_TO_POINTER (3), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "4 5 6 7 8 9 10");
  assert_changes (filter, "0-3*");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  custom = GTK_FILTER (gtk_custom_filter_new (is_larger_than, GUINT_TO_POINTER (20), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "");
  assert_changes (filter, "0-10*");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  custom = GTK_FILTER (gtk_custom_filter_new (is_near, GUINT_TO_POINTER (5), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "3 4 5 6 7");
  assert_changes (filter, "0-10+5*");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  custom = GTK_FILTER (gtk_custom_filter_new (is_not_near, GUINT_TO_POINTER (5), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "1 2 8 9 10");
  assert_changes (filter, "2-5*");
  g_object_unref (filter);
}

static void
test_change_filter (void)
{
  GtkFilterListModel *filter;
  GtkFilter *custom;

  filter = new_model (10, is_not_near, GUINT_TO_POINTER (5));
  assert_model (filter, "1 2 8 9 10");
  assert_changes (filter, "");

  custom = GTK_FILTER (gtk_custom_filter_new (is_not_near, GUINT_TO_POINTER (6), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "1 2 3 9 10");
  assert_changes (filter, "2-1+1");

  custom = GTK_FILTER (gtk_custom_filter_new (is_not_near, GUINT_TO_POINTER (9), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "1 2 3 4 5 6");
  assert_changes (filter, "3-2+3*");

  custom = GTK_FILTER (gtk_custom_filter_new (is_smaller_than, GUINT_TO_POINTER (6), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "1 2 3 4 5");
  assert_changes (filter, "-5*");

  custom = GTK_FILTER (gtk_custom_filter_new (is_larger_than, GUINT_TO_POINTER (4), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "5 6 7 8 9 10");
  assert_changes (filter, "0-5+6*");

  custom = GTK_FILTER (gtk_custom_filter_new (is_not_near, GUINT_TO_POINTER (2), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "5 6 7 8 9 10");
  assert_changes (filter, "");

  custom = GTK_FILTER (gtk_custom_filter_new (is_not_near, GUINT_TO_POINTER (4), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "1 7 8 9 10");
  assert_changes (filter, "0-2+1*");

  g_object_unref (filter);
}

static void
test_incremental (void)
{
  GtkFilterListModel *filter;
  GtkFilter *custom;

  /* everything is filtered */
  filter = new_model (1000, is_larger_than, GUINT_TO_POINTER (10000));
  gtk_filter_list_model_set_incremental (filter, TRUE);
  assert_model (filter, "");
  assert_changes (filter, "");

  custom = GTK_FILTER (gtk_custom_filter_new (is_near, GUINT_TO_POINTER (512), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "");
  assert_changes (filter, "");

  while (g_main_context_pending (NULL))
    g_main_context_iteration (NULL, TRUE);
  assert_model (filter, "510 511 512 513 514");

  gtk_filter_list_model_set_incremental (filter, FALSE);
  assert_model (filter, "510 511 512 513 514");

  /* implementation detail */
  ignore_changes (filter);

  g_object_unref (filter);
}

static void
test_empty (void)
{
  GtkFilterListModel *filter;
  GListStore *store;
  GtkFilter *f;

  filter = gtk_filter_list_model_new (NULL, NULL);

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter)), ==, 0);
  g_assert_null (g_list_model_get_item (G_LIST_MODEL (filter), 11));

  store = g_list_store_new (G_TYPE_OBJECT);
  gtk_filter_list_model_set_model (filter, G_LIST_MODEL (store));
  g_object_unref (store);

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter)), ==, 0);
  g_assert_null (g_list_model_get_item (G_LIST_MODEL (filter), 11));

  f = GTK_FILTER (gtk_every_filter_new ());
  gtk_filter_list_model_set_filter (filter, f);
  g_object_unref (f);

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter)), ==, 0);
  g_assert_null (g_list_model_get_item (G_LIST_MODEL (filter), 11));

  g_object_unref (filter);
}

static void
test_add_remove_item (void)
{
  GtkFilterListModel *filter;
  GListStore *store;

  filter = new_model (10, is_smaller_than, GUINT_TO_POINTER (7));
  assert_model (filter, "1 2 3 4 5 6");
  assert_changes (filter, "");

  store = G_LIST_STORE (gtk_filter_list_model_get_model (filter));
  add (store, 9);
  assert_model (filter, "1 2 3 4 5 6");
  assert_changes (filter, "");

  add (store, 1);
  assert_model (filter, "1 2 3 4 5 6 1");
  assert_changes (filter, "+6*");

  g_list_store_remove (store, 10);
  assert_model (filter, "1 2 3 4 5 6 1");
  assert_changes (filter, "");

  g_list_store_remove (store, 10);
  assert_model (filter, "1 2 3 4 5 6");
  assert_changes (filter, "-6*");

  g_object_unref (filter);
}

static int
sort_func (gconstpointer p1,
           gconstpointer p2,
           gpointer      data)
{
  const char *s1 = gtk_string_object_get_string ((GtkStringObject *)p1);
  const char *s2 = gtk_string_object_get_string ((GtkStringObject *)p2);

  /* compare just the first byte */
  return (int)(s1[0]) - (int)(s2[0]);
}

static gboolean
filter_func (gpointer item,
             gpointer data)
{
  const char *s = gtk_string_object_get_string ((GtkStringObject *)item);

  return s[0] == s[1];
}

static void
sections_changed (GtkSectionModel *model,
                  unsigned int     start,
                  unsigned int     end,
                  gpointer         user_data)
{
  gboolean *got_it = user_data;

  *got_it = TRUE;
}

static void
test_sections (void)
{
  GtkStringList *list;
  const char *strings[] = {
    "aaa",
    "aab",
    "abc",
    "bbb",
    "bq1",
    "bq2",
    "cc",
    "cx",
    NULL
  };
  GtkSorter *sorter;
  GtkSortListModel *sorted;
  GtkSorter *section_sorter;
  guint s, e;
  GtkFilterListModel *filtered;
  GtkFilter *filter;
  gboolean got_it = FALSE;

  list = gtk_string_list_new (strings);
  sorter = GTK_SORTER (gtk_string_sorter_new (gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string")));
  sorted = gtk_sort_list_model_new (G_LIST_MODEL (list), sorter);
  section_sorter = GTK_SORTER (gtk_custom_sorter_new (sort_func, NULL, NULL));
  gtk_sort_list_model_set_section_sorter (GTK_SORT_LIST_MODEL (sorted), section_sorter);
  g_object_unref (section_sorter);

  gtk_section_model_get_section (GTK_SECTION_MODEL (sorted), 0, &s, &e);
  g_assert_cmpint (s, ==, 0);
  g_assert_cmpint (e, ==, 3);
  gtk_section_model_get_section (GTK_SECTION_MODEL (sorted), 3, &s, &e);
  g_assert_cmpint (s, ==, 3);
  g_assert_cmpint (e, ==, 6);
  gtk_section_model_get_section (GTK_SECTION_MODEL (sorted), 6, &s, &e);
  g_assert_cmpint (s, ==, 6);
  g_assert_cmpint (e, ==, 8);

  filtered = gtk_filter_list_model_new (NULL, NULL);
  gtk_section_model_get_section (GTK_SECTION_MODEL (filtered), 0, &s, &e);
  g_assert_cmpint (s, ==, 0);
  g_assert_cmpint (e, ==, G_MAXUINT);

  gtk_filter_list_model_set_model (filtered, G_LIST_MODEL (sorted));
  gtk_section_model_get_section (GTK_SECTION_MODEL (filtered), 0, &s, &e);
  g_assert_cmpint (s, ==, 0);
  g_assert_cmpint (e, ==, 3);

  filter = GTK_FILTER (gtk_custom_filter_new (filter_func, NULL, NULL));
  gtk_filter_list_model_set_filter (filtered, filter);
  g_object_unref (filter);

  gtk_section_model_get_section (GTK_SECTION_MODEL (filtered), 0, &s, &e);
  g_assert_cmpint (s, ==, 0);
  g_assert_cmpint (e, ==, 2);
  gtk_section_model_get_section (GTK_SECTION_MODEL (filtered), 2, &s, &e);
  g_assert_cmpint (s, ==, 2);
  g_assert_cmpint (e, ==, 3);
  gtk_section_model_get_section (GTK_SECTION_MODEL (filtered), 3, &s, &e);
  g_assert_cmpint (s, ==, 3);
  g_assert_cmpint (e, ==, 4);

  g_signal_connect (filtered, "sections-changed", G_CALLBACK (sections_changed), &got_it);
  gtk_sort_list_model_set_section_sorter (GTK_SORT_LIST_MODEL (sorted), NULL);
  g_assert_true (got_it);

  g_object_unref (filtered);
  g_object_unref (sorted);
}

struct _GtkMutableStringObject
{
  GObject parent;
  char *string;
};

enum
{
  PROP_STRING = 1,
};

#define GTK_TYPE_MUTABLE_STRING_OBJECT (gtk_mutable_string_object_get_type ())
G_DECLARE_FINAL_TYPE (GtkMutableStringObject, gtk_mutable_string_object, GTK, MUTABLE_STRING_OBJECT, GObject)

G_DEFINE_FINAL_TYPE (GtkMutableStringObject, gtk_mutable_string_object, G_TYPE_OBJECT)

static void
gtk_mutable_string_object_set_string (GtkMutableStringObject *self,
                                      const char             *string)
{
  g_assert (GTK_IS_MUTABLE_STRING_OBJECT (self));

  if (g_strcmp0 (self->string, string) == 0)
    return;

  g_clear_pointer (&self->string, g_free);
  self->string = g_strdup (string);

  g_object_notify (G_OBJECT (self), "string");
}

static void
gtk_mutable_string_object_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GtkMutableStringObject *self = GTK_MUTABLE_STRING_OBJECT (object);

  switch (prop_id)
    {
    case PROP_STRING:
      g_value_set_string (value, self->string);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_mutable_string_object_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GtkMutableStringObject *self = GTK_MUTABLE_STRING_OBJECT (object);

  switch (prop_id)
    {
    case PROP_STRING:
      gtk_mutable_string_object_set_string (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_mutable_string_object_class_init (GtkMutableStringObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_mutable_string_object_get_property;
  object_class->set_property = gtk_mutable_string_object_set_property;

  g_object_class_install_property (object_class,
                                   PROP_STRING,
                                   g_param_spec_string ("string", NULL, NULL, NULL,
                                                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
}

static void
gtk_mutable_string_object_init (GtkMutableStringObject *self)
{
}

static void
test_watch_items (void)
{
  GtkMutableStringObject *string_object;
  GtkFilterListModel *filter_model;
  GtkStringFilter *string_filter;
  GListStore *store;
  const char * const strings[] = {
    "aa",
    "ab",
    "ac",
    "ad",
    "ae",
  };

  string_filter = gtk_string_filter_new (gtk_property_expression_new (GTK_TYPE_STRING_OBJECT,
                                                                      NULL,
                                                                      "string"));
  gtk_string_filter_set_search (string_filter, "a");

  g_type_ensure (GTK_TYPE_MUTABLE_STRING_OBJECT);

  store = g_list_store_new (GTK_TYPE_MUTABLE_STRING_OBJECT);
  for (size_t i = 0; i < G_N_ELEMENTS (strings); i++)
    {
      gpointer item = g_object_new (GTK_TYPE_MUTABLE_STRING_OBJECT, "string", strings[i], NULL);
      g_list_store_append (store, item);
      g_clear_object (&item);
    }

  filter_model = gtk_filter_list_model_new (G_LIST_MODEL (store),
                                            GTK_FILTER (string_filter));
  gtk_filter_list_model_set_watch_items (filter_model, TRUE);

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 5);

  string_object = g_list_model_get_item (G_LIST_MODEL (store), 1);
  gtk_mutable_string_object_set_string (string_object, "bb");

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 4);

  gtk_mutable_string_object_set_string (string_object, "ab");
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 5);

  string_object = g_object_new (GTK_TYPE_MUTABLE_STRING_OBJECT, "string", "ff", NULL);
  g_list_store_append (store, string_object);

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 5);

  gtk_mutable_string_object_set_string (string_object, "af");
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 6);

  g_list_store_remove (store, 5);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 5);

  g_list_store_append (store, string_object);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 6);

  /* Stop watching, no changes should propagate */
  gtk_filter_list_model_set_watch_items (filter_model, FALSE);

  gtk_mutable_string_object_set_string (string_object, "ff");
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 6);

  /* Start watching again */
  gtk_filter_list_model_set_watch_items (filter_model, TRUE);

  gtk_mutable_string_object_set_string (string_object, "af");
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 6);

  gtk_mutable_string_object_set_string (string_object, "ff");
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 5);

  g_clear_object (&string_object);
  g_clear_object (&filter_model);
}

struct _GtkBoolObject
{
  GObject parent;
  gboolean value;
  gboolean value2;
};

enum
{
  PROP_VALUE = 1,
  PROP_VALUE2,
};

#define GTK_TYPE_BOOL_OBJECT (gtk_bool_object_get_type ())
G_DECLARE_FINAL_TYPE (GtkBoolObject, gtk_bool_object, GTK, BOOL_OBJECT, GObject)

G_DEFINE_FINAL_TYPE (GtkBoolObject, gtk_bool_object, G_TYPE_OBJECT)

static void
gtk_bool_object_set_values (GtkBoolObject *self,
                            gboolean       value,
                            gboolean       value2)
{
  g_assert (GTK_IS_BOOL_OBJECT (self));

  if (self->value == value && self->value2 == value2)
    return;

  if (self->value != value)
    {
      self->value = value;
      g_object_notify (G_OBJECT (self), "value");
    }

  if (self->value2 != value2)
    {
      self->value2 = value2;
      g_object_notify (G_OBJECT (self), "value2");
    }
}

static void
gtk_bool_object_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkBoolObject *self = GTK_BOOL_OBJECT (object);

  switch (prop_id)
    {
    case PROP_VALUE:
      g_value_set_boolean (value, self->value);
      break;

    case PROP_VALUE2:
      g_value_set_boolean (value, self->value2);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_bool_object_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkBoolObject *self = GTK_BOOL_OBJECT (object);

  switch (prop_id)
    {
    case PROP_VALUE:
      gtk_bool_object_set_values (self, g_value_get_boolean (value), self->value2);
      break;

    case PROP_VALUE2:
      gtk_bool_object_set_values (self, self->value, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_bool_object_class_init (GtkBoolObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_bool_object_get_property;
  object_class->set_property = gtk_bool_object_set_property;

  g_object_class_install_property (object_class,
                                   PROP_VALUE,
                                   g_param_spec_boolean ("value", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_VALUE2,
                                   g_param_spec_boolean ("value2", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
}

static void
gtk_bool_object_init (GtkBoolObject *self)
{
}

static void
test_watch_items_multifilter (void)
{
  GtkFilterListModel *filter_model;
  GtkEveryFilter *every_filter;
  GtkBoolObject *bool_object;
  GtkAnyFilter *any_filter;
  GListStore *store;

  store = g_list_store_new (GTK_TYPE_BOOL_OBJECT);

  bool_object = g_object_new (GTK_TYPE_BOOL_OBJECT, NULL);
  g_list_store_append (store, bool_object);

  any_filter = gtk_any_filter_new ();
  gtk_multi_filter_append (GTK_MULTI_FILTER (any_filter),
                           GTK_FILTER (gtk_bool_filter_new (gtk_property_expression_new (GTK_TYPE_BOOL_OBJECT, NULL, "value"))));
  gtk_multi_filter_append (GTK_MULTI_FILTER (any_filter),
                           GTK_FILTER (gtk_bool_filter_new (gtk_property_expression_new (GTK_TYPE_BOOL_OBJECT, NULL, "value2"))));

  filter_model = gtk_filter_list_model_new (G_LIST_MODEL (store), GTK_FILTER (any_filter));
  gtk_filter_list_model_set_watch_items (filter_model, TRUE);

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 0);

  gtk_bool_object_set_values (bool_object, FALSE, TRUE);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 1);

  gtk_bool_object_set_values (bool_object, TRUE, FALSE);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 1);

  gtk_bool_object_set_values (bool_object, TRUE, TRUE);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 1);

  gtk_bool_object_set_values (bool_object, FALSE, FALSE);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 0);

  every_filter = gtk_every_filter_new ();
  gtk_multi_filter_append (GTK_MULTI_FILTER (every_filter),
                           GTK_FILTER (gtk_bool_filter_new (gtk_property_expression_new (GTK_TYPE_BOOL_OBJECT, NULL, "value"))));
  gtk_multi_filter_append (GTK_MULTI_FILTER (every_filter),
                           GTK_FILTER (gtk_bool_filter_new (gtk_property_expression_new (GTK_TYPE_BOOL_OBJECT, NULL, "value2"))));

  gtk_filter_list_model_set_filter (filter_model, GTK_FILTER (every_filter));

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 0);

  gtk_bool_object_set_values (bool_object, FALSE, TRUE);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 0);

  gtk_bool_object_set_values (bool_object, TRUE, FALSE);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 0);

  gtk_bool_object_set_values (bool_object, TRUE, TRUE);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 1);

  gtk_bool_object_set_values (bool_object, FALSE, FALSE);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 0);

  g_clear_object (&every_filter);
  g_clear_object (&bool_object);
  g_clear_object (&filter_model);
}

static void
items_changed_cb (gboolean *items_changed_emitted)
{
  *items_changed_emitted = TRUE;
}

static void
test_watch_items_signaling (void)
{

  GtkMutableStringObject *string_object;
  GtkFilterListModel *filter_model;
  GtkStringFilter *string_filter;
  GListStore *store;
  const char * const strings[] = {
    "a",
  };
  gboolean items_changed_emitted = FALSE;

  string_filter = gtk_string_filter_new (gtk_property_expression_new (GTK_TYPE_STRING_OBJECT,
                                                                      NULL,
                                                                      "string"));
  gtk_string_filter_set_search (string_filter, "a");

  store = g_list_store_new (GTK_TYPE_MUTABLE_STRING_OBJECT);
  for (size_t i = 0; i < G_N_ELEMENTS (strings); i++)
    {
      gpointer item = g_object_new (GTK_TYPE_MUTABLE_STRING_OBJECT, "string", strings[i], NULL);
      g_list_store_append (store, item);
      g_clear_object (&item);
    }

  filter_model = gtk_filter_list_model_new (G_LIST_MODEL (store),
                                            GTK_FILTER (string_filter));
  gtk_filter_list_model_set_watch_items (filter_model, TRUE);
  g_signal_connect_swapped (filter_model, "items-changed", G_CALLBACK (items_changed_cb), &items_changed_emitted);

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 1);

  string_object = g_list_model_get_item (G_LIST_MODEL (store), 0);
  gtk_mutable_string_object_set_string (string_object, "b");

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 0);
  g_assert_true (items_changed_emitted);

  items_changed_emitted = FALSE;

  gtk_mutable_string_object_set_string (string_object, "a");
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter_model)), ==, 1);
  g_assert_true (items_changed_emitted);

  g_clear_object (&string_object);
  g_clear_object (&filter_model);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  number_quark = g_quark_from_static_string ("Hell and fire was spawned to be released.");
  changes_quark = g_quark_from_static_string ("What did I see? Can I believe what I saw?");

  g_test_add_func ("/filterlistmodel/create", test_create);
  g_test_add_func ("/filterlistmodel/empty_set_filter", test_empty_set_filter);
  g_test_add_func ("/filterlistmodel/change_filter", test_change_filter);
  g_test_add_func ("/filterlistmodel/incremental", test_incremental);
  g_test_add_func ("/filterlistmodel/empty", test_empty);
  g_test_add_func ("/filterlistmodel/add_remove_item", test_add_remove_item);
  g_test_add_func ("/filterlistmodel/sections", test_sections);
  g_test_add_func ("/filterlistmodel/watch-items", test_watch_items);
  g_test_add_func ("/filterlistmodel/watch-items-multifilter", test_watch_items_multifilter);
  g_test_add_func ("/filterlistmodel/watch-items-signaling", test_watch_items_signaling);

  return g_test_run ();
}
