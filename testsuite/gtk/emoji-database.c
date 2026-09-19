/*
 * emoji-database.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gtk/gtk.h>

#include "gtkemojiitemprivate.h"
#include "gtkemojidataprivate.h"

static void
test_database (void)
{
  GtkEmojiDatabase *database = gtk_emoji_database_new ();
  GtkEmojiItem *first = NULL;
  GtkEmojiItem *again = NULL;
  GVariant *record = NULL;
  char *text = NULL;
  char *variant_text = NULL;
  guint n_items = g_list_model_get_n_items (G_LIST_MODEL (database));
  guint group_offset;
  guint group_size;

  g_assert_cmpuint (n_items, >, 1900);
  g_assert_cmpuint (gtk_emoji_database_get_n_live_items (database), ==, 0);
  g_assert_cmpuint (gtk_emoji_database_get_item_calls (database), ==, 0);
  g_assert_cmpuint (g_list_model_get_item_type (G_LIST_MODEL (database)), ==, GTK_TYPE_EMOJI_ITEM);
  g_assert_null (g_list_model_get_item (G_LIST_MODEL (database), n_items));

  first = g_list_model_get_item (G_LIST_MODEL (database), 0);
  again = g_list_model_get_item (G_LIST_MODEL (database), 0);
  g_assert_true (first == again);
  g_assert_cmpuint (gtk_emoji_item_get_id (first), ==, 0);
  g_assert_cmpuint (gtk_emoji_database_get_n_live_items (database), ==, 1);

  record = gtk_emoji_database_dup_record (database, 0);
  g_assert_nonnull (record);
  text = gtk_emoji_database_dup_text (database, 0, 0);
  g_assert_cmpstr (text, ==, "😀");
  gtk_emoji_database_get_group_range (database, 1, &group_offset, &group_size);
  g_assert_cmpuint (group_size, >, 0);
  variant_text = gtk_emoji_database_dup_text (database, group_offset, 0x1f3fb);
  g_assert_cmpstr (variant_text, ==, "👋🏻");

  g_clear_object (&again);
  g_clear_object (&first);
  g_assert_cmpuint (gtk_emoji_database_get_n_live_items (database), ==, 0);
  first = g_list_model_get_item (G_LIST_MODEL (database), 0);
  g_assert_nonnull (first);
  g_assert_cmpuint (gtk_emoji_database_get_n_live_items (database), ==, 1);
  g_clear_object (&database);
  g_assert_nonnull (gtk_emoji_item_get_database (first));
  g_object_unref (first);
  g_variant_unref (record);
  g_free (text);
  g_free (variant_text);
}

static void
test_database_disposed_before_items (void)
{
  GtkEmojiDatabase *database = gtk_emoji_database_new ();
  GtkEmojiItem *first = g_list_model_get_item (G_LIST_MODEL (database), 0);
  GtkEmojiItem *second = g_list_model_get_item (G_LIST_MODEL (database), 1);
  GVariant *record = NULL;

  g_assert_cmpuint (gtk_emoji_database_get_n_live_items (database), ==, 2);
  g_object_run_dispose (G_OBJECT (database));
  g_assert_cmpuint (gtk_emoji_database_get_n_live_items (database), ==, 0);
  g_assert_null (g_list_model_get_item (G_LIST_MODEL (database), 0));

  /* The item owns the serialized resource even after the registry is gone. */
  record = gtk_emoji_database_dup_record (gtk_emoji_item_get_database (first), 0);
  g_assert_nonnull (record);
  g_clear_object (&first);
  g_clear_object (&second);
  g_variant_unref (record);
  g_object_unref (database);
}

static void
test_item_disposed_before_database (void)
{
  GtkEmojiDatabase *database = gtk_emoji_database_new ();
  GtkEmojiItem *first = g_list_model_get_item (G_LIST_MODEL (database), 0);
  GtkEmojiItem *second = g_list_model_get_item (G_LIST_MODEL (database), 1);
  gpointer first_weak = first;
  gpointer second_weak = second;

  g_object_add_weak_pointer (G_OBJECT (first), &first_weak);
  g_object_add_weak_pointer (G_OBJECT (second), &second_weak);
  g_assert_cmpuint (gtk_emoji_database_get_n_live_items (database), ==, 2);

  g_object_run_dispose (G_OBJECT (first));
  g_assert_cmpuint (gtk_emoji_database_get_n_live_items (database), ==, 1);
  g_assert_null (gtk_emoji_item_get_database (first));
  g_object_unref (first);
  g_assert_null (first_weak);

  g_object_unref (second);
  g_assert_null (second_weak);
  g_assert_cmpuint (gtk_emoji_database_get_n_live_items (database), ==, 0);
  first = g_list_model_get_item (G_LIST_MODEL (database), 0);
  g_assert_nonnull (first);
  g_object_unref (first);
  g_object_unref (database);
}

static void
test_owner_teardown (void)
{
  GtkEmojiDatabase *database = gtk_emoji_database_new ();
  GtkEmojiItem *item = g_list_model_get_item (G_LIST_MODEL (database), 0);
  gpointer database_weak = database;
  GVariant *record = NULL;

  g_object_add_weak_pointer (G_OBJECT (database), &database_weak);
  g_object_unref (database);
  g_assert_nonnull (database_weak);
  record = gtk_emoji_database_dup_record (gtk_emoji_item_get_database (item), 0);
  g_assert_nonnull (record);
  g_object_unref (item);
  g_assert_null (database_weak);
  g_variant_unref (record);
}

static void
test_group_index (void)
{
  GtkEmojiDatabase *database = gtk_emoji_database_new ();
  guint n_items = g_list_model_get_n_items (G_LIST_MODEL (database));
  guint total = 0;

  for (guint group = 0; group <= 10; group++)
    {
      guint offset;
      guint size;

      gtk_emoji_database_get_group_range (database, group, &offset, &size);
      for (guint id = offset; id < offset + size; id++)
        {
          GVariant *record = gtk_emoji_database_dup_record (database, id);
          guint actual_group;

          g_variant_get_child (record, 5, "u", &actual_group);
          g_assert_cmpuint (MIN (actual_group, 10), ==, group);
          g_variant_unref (record);
        }
      total += size;
    }

  g_assert_cmpuint (total, ==, n_items);
  g_assert_cmpuint (gtk_emoji_database_get_n_live_items (database), ==, 0);
  g_object_unref (database);
}

static gboolean
exclude_emoji_id (gpointer item,
                  gpointer data)
{
  return gtk_emoji_item_get_id (item) != *(guint *) data;
}

static void
test_sectioned_browse_model (void)
{
  static const guint groups[] = { 0, 1, 3, 4, 5, 6, 7, 8, 9 };
  GtkEmojiDatabase *database = gtk_emoji_database_new ();
  GtkCustomFilter *filter = gtk_custom_filter_new (NULL, NULL, NULL);
  GListStore *models = g_list_store_new (G_TYPE_LIST_MODEL);
  GListStore *recent = g_list_store_new (GTK_TYPE_EMOJI_ITEM);
  GtkFlattenListModel *browse = NULL;
  GtkNoSelection *selection = NULL;
  GVariant *record = gtk_emoji_database_dup_record (database, 0);
  GtkEmojiItem *recent_item = gtk_emoji_item_new (record, 0);
  GtkFilterListModel *recent_section;
  guint excluded;
  guint position;

  g_list_store_append (recent, recent_item);
  recent_section = gtk_filter_list_model_new (g_object_ref (G_LIST_MODEL (recent)),
                                               g_object_ref (GTK_FILTER (filter)));
  g_list_store_append (models, recent_section);
  g_object_unref (recent_section);

  for (guint i = 0; i < G_N_ELEMENTS (groups); i++)
    {
      guint offset;
      guint size;
      GtkSliceListModel *slice;
      GtkFilterListModel *section;

      gtk_emoji_database_get_group_range (database, groups[i], &offset, &size);
      slice = gtk_slice_list_model_new (g_object_ref (G_LIST_MODEL (database)), offset, size);
      section = gtk_filter_list_model_new (G_LIST_MODEL (slice), g_object_ref (GTK_FILTER (filter)));
      g_list_store_append (models, section);
      g_object_unref (section);
    }

  browse = gtk_flatten_list_model_new (g_object_ref (G_LIST_MODEL (models)));
  selection = gtk_no_selection_new (g_object_ref (G_LIST_MODEL (browse)));
  g_assert_cmpuint (gtk_emoji_database_get_item_calls (database), ==, 0);

  for (guint pass = 0; pass < 2; pass++)
    {
      position = 0;

      for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (models)); i++)
        {
          GListModel *section = g_list_model_get_item (G_LIST_MODEL (models), i);
          guint start;
          guint end;
          guint size = g_list_model_get_n_items (section);

          if (size == 0)
            {
              g_object_unref (section);
              continue;
            }

          gtk_section_model_get_section (GTK_SECTION_MODEL (selection), position, &start, &end);
          g_assert_cmpuint (start, ==, position);
          g_assert_cmpuint (end, ==, position + size);
          g_assert_true (gtk_flatten_list_model_get_model_for_item (browse, position) == section);
          position += size;
          g_object_unref (section);
        }

      g_assert_cmpuint (position, ==, g_list_model_get_n_items (G_LIST_MODEL (selection)));

      if (pass == 0)
        {
          guint offset;
          guint size;
          guint components_offset;
          guint components_size;

          gtk_emoji_database_get_group_range (database, groups[0], &offset, &size);
          gtk_emoji_database_get_group_range (database, 2,
                                              &components_offset, &components_size);
          excluded = offset;
          gtk_custom_filter_set_filter_func (filter, exclude_emoji_id, &excluded, NULL);
          g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (selection)),
                            ==,
                            1 + g_list_model_get_n_items (G_LIST_MODEL (database)) - components_size - 1);
        }
    }

  g_assert_cmpuint (gtk_emoji_database_get_n_live_items (database), ==, 0);
  g_object_unref (selection);
  g_object_unref (browse);
  g_object_unref (models);
  g_object_unref (recent_item);
  g_variant_unref (record);
  g_object_unref (recent);
  g_object_unref (filter);
  g_object_unref (database);
}

typedef struct
{
  char **tokens;
} SearchData;

static gboolean
match_search_tokens (gpointer item,
                     gpointer data)
{
  SearchData *search = data;
  GVariant *record = gtk_emoji_item_dup_record (item);
  gboolean matches;

  matches = gtk_emoji_data_matches (record, (const char **) search->tokens);
  g_variant_unref (record);

  return matches;
}

static void
test_incremental_search (void)
{
  GtkEmojiDatabase *database = gtk_emoji_database_new ();
  GtkCustomFilter *filter = gtk_custom_filter_new (NULL, NULL, NULL);
  GtkFilterListModel *search = gtk_filter_list_model_new
    (g_object_ref (G_LIST_MODEL (database)), g_object_ref (GTK_FILTER (filter)));
  SearchData data = { 0 };
  guint n_items = g_list_model_get_n_items (G_LIST_MODEL (database));

  gtk_filter_list_model_set_incremental (search, TRUE);
  g_assert_cmpuint (gtk_emoji_database_get_item_calls (database), ==, 0);
  data.tokens = g_str_tokenize_and_fold ("heart", "en", NULL);
  gtk_custom_filter_set_filter_func (filter, match_search_tokens, &data, NULL);
  g_assert_cmpuint (gtk_filter_list_model_get_pending (search), ==, n_items);

  g_strfreev (data.tokens);
  data.tokens = g_str_tokenize_and_fold ("zzzzunlikely", "en", NULL);
  gtk_filter_changed (GTK_FILTER (filter), GTK_FILTER_CHANGE_DIFFERENT);
  for (guint i = 0; gtk_filter_list_model_get_pending (search) > 0 && i < 100; i++)
    g_main_context_iteration (NULL, TRUE);
  g_assert_cmpuint (gtk_filter_list_model_get_pending (search), ==, 0);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (search)), ==, 0);

  g_strfreev (data.tokens);
  data.tokens = g_str_tokenize_and_fold ("hand", "en", NULL);
  gtk_filter_changed (GTK_FILTER (filter), GTK_FILTER_CHANGE_DIFFERENT);
  for (guint i = 0; gtk_filter_list_model_get_pending (search) > 0 && i < 100; i++)
    g_main_context_iteration (NULL, TRUE);
  g_assert_cmpuint (gtk_filter_list_model_get_pending (search), ==, 0);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (search)), >, 0);

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (search)); i++)
    {
      GtkEmojiItem *item = g_list_model_get_item (G_LIST_MODEL (search), i);
      GVariant *item_record = gtk_emoji_item_dup_record (item);

      g_assert_true (gtk_emoji_data_matches (item_record, (const char **) data.tokens));
      g_variant_unref (item_record);
      g_object_unref (item);
    }

  gtk_custom_filter_set_filter_func (filter, NULL, NULL, NULL);
  g_strfreev (data.tokens);
  g_object_unref (search);
  g_object_unref (filter);
  g_object_unref (database);
}

static GtkWidget *
find_child_type (GtkWidget *widget,
                 GType      type)
{
  GtkWidget *child;
  GtkWidget *match;

  if (G_TYPE_CHECK_INSTANCE_TYPE (widget, type))
    return widget;

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      match = find_child_type (child, type);
      if (match != NULL)
        return match;
    }

  return NULL;
}

static void
test_chooser_search (void)
{
  GtkWidget *chooser = NULL;
  GtkWidget *entry;
  GtkWidget *grid;
  GtkWidget *stack;
  GtkNoSelection *selection;

  if (gdk_display_get_default () == NULL)
    {
      g_test_skip ("Requires a display");
      return;
    }

  chooser = g_object_ref_sink (gtk_emoji_chooser_new ());
  entry = find_child_type (chooser, GTK_TYPE_SEARCH_ENTRY);
  grid = find_child_type (chooser, GTK_TYPE_GRID_VIEW);
  stack = find_child_type (chooser, GTK_TYPE_STACK);
  g_assert_nonnull (entry);
  g_assert_nonnull (grid);
  g_assert_nonnull (stack);
  selection = GTK_NO_SELECTION (gtk_grid_view_get_model (GTK_GRID_VIEW (grid)));
  g_assert_true (GTK_IS_FLATTEN_LIST_MODEL (gtk_no_selection_get_model (selection)));

  gtk_editable_set_text (GTK_EDITABLE (entry), "zzzzunlikely");
  g_signal_emit_by_name (entry, "search-changed");
  g_assert_true (GTK_IS_FILTER_LIST_MODEL (gtk_no_selection_get_model (selection)));
  g_assert_cmpstr (gtk_stack_get_visible_child_name (GTK_STACK (stack)), ==, "grid");

  while (gtk_filter_list_model_get_pending
         (GTK_FILTER_LIST_MODEL (gtk_no_selection_get_model (selection))) > 0)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpstr (gtk_stack_get_visible_child_name (GTK_STACK (stack)), ==, "empty");
  gtk_editable_set_text (GTK_EDITABLE (entry), "");
  g_signal_emit_by_name (entry, "search-changed");
  g_assert_true (GTK_IS_FLATTEN_LIST_MODEL (gtk_no_selection_get_model (selection)));
  g_assert_cmpstr (gtk_stack_get_visible_child_name (GTK_STACK (stack)), ==, "grid");
  g_object_unref (chooser);
}

static GVariant *
make_record (gunichar code,
             gunichar placeholder,
             guint    length)
{
  GVariantBuilder codes;

  g_variant_builder_init (&codes, G_VARIANT_TYPE ("au"));
  for (guint i = 0; i < length; i++)
    g_variant_builder_add (&codes, "u", code);
  g_variant_builder_add (&codes, "u", placeholder);

  return g_variant_ref_sink (g_variant_new ("(@auss@as@asu)",
                             g_variant_builder_end (&codes), "Raised hand", "Main levée",
                             g_variant_new_strv ((const char *[]) { "palm", NULL }, -1),
                             g_variant_new_strv ((const char *[]) { "doigts", NULL }, -1), 1));
}

static void
test_record_text (void)
{
  for (guint i = 0; i < 2; i++)
    {
      GVariant *record = make_record (0x270b, i ? 0x1f3fb : 0, 1);
      char *base = gtk_emoji_data_dup_text (record, 0);
      char *modified = gtk_emoji_data_dup_text (record, 0x1f3fb);
      GtkEmojiItem *item = gtk_emoji_item_new (record, 0x1f3fb);
      char *item_text = gtk_emoji_item_dup_text (item);

      g_assert_true (gtk_emoji_data_has_variations (record));
      g_assert_cmpstr (base, ==, i ? "✋" : "✋️");
      g_assert_cmpstr (modified, ==, "✋🏻");
      g_assert_cmpstr (item_text, ==, modified);
      g_assert_null (gtk_emoji_item_get_database (item));
      g_assert_cmpuint (gtk_emoji_item_get_id (item), ==, G_MAXUINT);
      g_free (item_text);
      g_object_unref (item);
      g_free (modified);
      g_free (base);
      g_variant_unref (record);
    }

  {
    GVariant *record = make_record (0x1f600, 0, 100);
    char *text = gtk_emoji_data_dup_text (record, 0);

    g_assert_cmpuint (strlen (text), ==, 403);
    g_assert_true (g_utf8_validate (text, -1, NULL));
    g_assert_true (gtk_emoji_data_matches (record, (const char *[]) { "rais", "hand", NULL }));
    g_assert_true (gtk_emoji_data_matches (record, (const char *[]) { "main", NULL }));
    g_assert_true (gtk_emoji_data_matches (record, (const char *[]) { "palm", NULL }));
    g_assert_false (gtk_emoji_data_matches (record, (const char *[]) { "hand", "missing", NULL }));
    g_free (text);
    g_variant_unref (record);
  }
}

static void
test_recent_records (void)
{
  GListStore *store = g_list_store_new (GTK_TYPE_EMOJI_ITEM);
  GListStore *restored = g_list_store_new (GTK_TYPE_EMOJI_ITEM);
  GVariant *saved = NULL;
  GVariant *saved_again = NULL;
  GtkEmojiItem *first = NULL;

  for (guint i = 0; i < 25; i++)
    {
      GVariant *record = make_record (0x1f600 + i, 0, 1);
      GtkEmojiItem *item = gtk_emoji_item_new (record, 0);

      gtk_emoji_recent_add (store, item);
      g_object_unref (item);
      g_variant_unref (record);
    }

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (store)), ==, 21);
  first = g_list_model_get_item (G_LIST_MODEL (store), 0);
  gtk_emoji_recent_add (store, first);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (store)), ==, 21);
  saved = g_variant_ref_sink (gtk_emoji_recent_serialize (store));
  gtk_emoji_recent_load (restored, saved);
  saved_again = g_variant_ref_sink (gtk_emoji_recent_serialize (restored));
  g_assert_true (g_variant_equal (saved, saved_again));

  {
    GVariant *record = gtk_emoji_item_dup_record (first);
    GtkEmojiItem *modified = gtk_emoji_item_new (record, 0x1f3fd);
    GtkEmojiItem *second = NULL;

    gtk_emoji_recent_add (restored, modified);
    second = g_list_model_get_item (G_LIST_MODEL (restored), 1);
    g_assert_cmpuint (gtk_emoji_item_get_modifier (second), ==, 0);
    g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (restored)), ==, 21);
    g_object_unref (second);
    g_object_unref (modified);
    g_variant_unref (record);
  }

  g_object_unref (first);
  g_variant_unref (saved_again);
  g_variant_unref (saved);
  g_object_unref (restored);
  g_object_unref (store);
}

int
main (int argc, char **argv)
{
  (g_test_init) (&argc, &argv, NULL);
  gtk_disable_setlocale ();
  gtk_init ();
  g_test_add_func ("/emoji/database", test_database);
  g_test_add_func ("/emoji/database/dispose-before-items", test_database_disposed_before_items);
  g_test_add_func ("/emoji/database/item-disposed-before-database", test_item_disposed_before_database);
  g_test_add_func ("/emoji/database/owner-teardown", test_owner_teardown);
  g_test_add_func ("/emoji/database/group-index", test_group_index);
  g_test_add_func ("/emoji/models/sectioned-browse", test_sectioned_browse_model);
  g_test_add_func ("/emoji/models/incremental-search", test_incremental_search);
  g_test_add_func ("/emoji/chooser/search", test_chooser_search);
  g_test_add_func ("/emoji/data/text", test_record_text);
  g_test_add_func ("/emoji/recents/records", test_recent_records);
  return g_test_run ();
}
