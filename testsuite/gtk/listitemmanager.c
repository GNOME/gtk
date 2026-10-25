/*
 * Copyright © 2023 Benjamin Otte
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
#include "gtk/gtklistitemmanagerprivate.h"
#include "gtk/gtklistbaseprivate.h"

static GListModel *
create_source_model (guint min_size, guint max_size)
{
  GtkStringList *list;
  guint i, size;

  size = g_test_rand_int_range (min_size, max_size + 1);
  list = gtk_string_list_new (NULL);

  for (i = 0; i < size; i++)
    gtk_string_list_append (list, g_test_rand_bit () ? "A" : "B");

  return G_LIST_MODEL (list);
}

void
print_list_item_manager_tiles (GtkListItemManager *items)
{
  GString *string;
  GtkListTile *tile;

  string = g_string_new ("");

  for (tile = gtk_list_item_manager_get_first (items);
       tile != NULL;
       tile = gtk_rb_tree_node_get_next (tile))
    {
      switch (tile->type)
        {
          case GTK_LIST_TILE_ITEM:
            if (tile->widget)
              g_string_append_c (string, 'W');
            else if (tile->n_items == 1)
              g_string_append_c (string, 'x');
            else
              g_string_append_printf (string, "%u,", tile->n_items);
            break;
          case GTK_LIST_TILE_HEADER:
            g_string_append_c (string, '[');
            break;
          case GTK_LIST_TILE_UNMATCHED_HEADER:
            g_string_append_c (string, '(');
            break;
          case GTK_LIST_TILE_FOOTER:
            g_string_append_c (string, ']');
            break;
          case GTK_LIST_TILE_UNMATCHED_FOOTER:
            g_string_append_c (string, ')');
            break;

          case GTK_LIST_TILE_REMOVED:
            g_string_append_c (string, '.');
            break;
          default:
            g_assert_not_reached ();
            break;
        }
    }

  g_print ("%s\n", string->str);

  g_string_free (string, TRUE);
}

static gsize
widget_count_children (GtkWidget *widget)
{
  GtkWidget *child;
  gsize n_children = 0;

  for (child = gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child))
    n_children++;

  return n_children;
}

static void
check_list_item_manager (GtkListItemManager  *items,
                         GtkWidget           *widget,
                         GtkListItemTracker **trackers,
                         gsize                n_trackers)
{
  GListModel *model = G_LIST_MODEL (gtk_list_item_manager_get_model (items));
  GtkListTile *tile;
  guint n_items, n_tile_widgets;
  guint i;
  gboolean has_sections;
  enum {
    NO_SECTION,
    MATCHED_SECTION,
    UNMATCHED_SECTION
  } section_state = NO_SECTION;
  gboolean after_items = FALSE;

  has_sections = gtk_list_item_manager_get_has_sections (items);

  n_items = 0;
  n_tile_widgets = 0;

  for (tile = gtk_list_item_manager_get_first (items);
       tile != NULL;
       tile = gtk_rb_tree_node_get_next (tile))
    {
      switch (tile->type)
        {
          case GTK_LIST_TILE_HEADER:
            g_assert_cmpint (section_state, ==, NO_SECTION);
            g_assert_cmpint (tile->n_items, ==, 0);
            g_assert_true (has_sections);
            g_assert_true (tile->widget);
            section_state = MATCHED_SECTION;
            after_items = FALSE;
            break;

          case GTK_LIST_TILE_UNMATCHED_HEADER:
            g_assert_cmpint (section_state, ==, NO_SECTION);
            g_assert_cmpint (tile->n_items, ==, 0);
            g_assert_null (tile->widget);
            section_state = UNMATCHED_SECTION;
            after_items = FALSE;
            break;

          case GTK_LIST_TILE_FOOTER:
            g_assert_cmpint (section_state, ==, MATCHED_SECTION);
            g_assert_cmpint (tile->n_items, ==, 0);
            g_assert_true (has_sections);
            g_assert_null (tile->widget);
            section_state = NO_SECTION;
            after_items = FALSE;
            break;

          case GTK_LIST_TILE_UNMATCHED_FOOTER:
            g_assert_cmpint (section_state, ==, UNMATCHED_SECTION);
            g_assert_cmpint (tile->n_items, ==, 0);
            g_assert_null (tile->widget);
            section_state = NO_SECTION;
            after_items = FALSE;
            break;

          case GTK_LIST_TILE_ITEM:
            g_assert_cmpint (section_state, !=, NO_SECTION);
            if (tile->widget)
              {
                GObject *item = g_list_model_get_item (model, n_items);
                if (has_sections)
                  g_assert_cmpint (section_state, ==, MATCHED_SECTION);
                else
                  g_assert_cmpint (section_state, ==, UNMATCHED_SECTION);
                g_assert_cmphex (GPOINTER_TO_SIZE (item), ==, GPOINTER_TO_SIZE (gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (tile->widget))));
                g_object_unref (item);
                g_assert_cmpint (n_items, ==, gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (tile->widget)));
                g_assert_cmpint (tile->n_items, ==, 1);
                after_items = FALSE;
              }
            else
              {
                after_items = TRUE;
              }
            if (tile->n_items)
              n_items += tile->n_items;
            break;

          case GTK_LIST_TILE_REMOVED:
            g_assert_cmpint (tile->n_items, ==, 0);
            g_assert_null (tile->widget);
            break;

          default:
            g_assert_not_reached ();
            break;
        }

      if (tile->widget)
        n_tile_widgets++;
    }

  g_assert_cmpint (section_state, ==, NO_SECTION);
  g_assert_cmpint (n_items, ==, g_list_model_get_n_items (model));
  g_assert_cmpint (n_tile_widgets, ==, widget_count_children (widget));

  for (i = 0; i < n_trackers; i++)
    {
      guint pos, offset;

      pos = gtk_list_item_tracker_get_position (items, trackers[i]);
      if (pos == GTK_INVALID_LIST_POSITION)
        continue;
      tile = gtk_list_item_manager_get_nth (items, pos, &offset);
      g_assert_cmpint (tile->n_items, ==, 1);
      g_assert_cmpint (offset, ==, 0);
      g_assert_true (tile->widget);
    }

  gtk_list_item_manager_gc_tiles (items);

  n_items = 0;
  n_tile_widgets = 0;

  for (tile = gtk_list_item_manager_get_first (items);
       tile != NULL;
       tile = gtk_rb_tree_node_get_next (tile))
    {
      switch (tile->type)
        {
          case GTK_LIST_TILE_HEADER:
            g_assert_cmpint (section_state, ==, NO_SECTION);
            g_assert_cmpint (tile->n_items, ==, 0);
            g_assert_true (has_sections);
            g_assert_true (tile->widget);
            section_state = MATCHED_SECTION;
            after_items = FALSE;
            break;

          case GTK_LIST_TILE_UNMATCHED_HEADER:
            g_assert_cmpint (section_state, ==, NO_SECTION);
            g_assert_cmpint (tile->n_items, ==, 0);
            g_assert_false (tile->widget);
            section_state = UNMATCHED_SECTION;
            after_items = FALSE;
            break;

          case GTK_LIST_TILE_FOOTER:
            g_assert_cmpint (section_state, ==, MATCHED_SECTION);
            g_assert_cmpint (tile->n_items, ==, 0);
            g_assert_true (has_sections);
            g_assert_false (tile->widget);
            section_state = NO_SECTION;
            after_items = FALSE;
            break;

          case GTK_LIST_TILE_UNMATCHED_FOOTER:
            g_assert_cmpint (section_state, ==, UNMATCHED_SECTION);
            g_assert_cmpint (tile->n_items, ==, 0);
            g_assert_false (tile->widget);
            section_state = NO_SECTION;
            after_items = FALSE;
            break;

          case GTK_LIST_TILE_ITEM:
            g_assert_cmpint (section_state, !=, NO_SECTION);
            if (tile->widget)
              {
                g_assert_cmpint (tile->n_items, ==, 1);
                after_items = FALSE;
              }
            else
              {
                g_assert_false (after_items);
                after_items = TRUE;
              }
            if (tile->n_items)
              n_items += tile->n_items;
            break;

          case GTK_LIST_TILE_REMOVED:
          default:
            g_assert_not_reached ();
            break;
        }

      if (tile->widget)
        n_tile_widgets++;
    }

  g_assert_cmpint (section_state, ==, NO_SECTION);
  g_assert_cmpint (n_items, ==, g_list_model_get_n_items (model));
  g_assert_cmpint (n_tile_widgets, ==, widget_count_children (widget));

  for (i = 0; i < n_trackers; i++)
    {
      guint pos, offset;

      pos = gtk_list_item_tracker_get_position (items, trackers[i]);
      if (pos == GTK_INVALID_LIST_POSITION)
        continue;
      tile = gtk_list_item_manager_get_nth (items, pos, &offset);
      g_assert_cmpint (tile->n_items, ==, 1);
      g_assert_cmpint (offset, ==, 0);
      g_assert_true (tile->widget);
    }
}

static GtkListTile *
split_simple (GtkWidget   *widget,
              GtkListTile *tile,
              guint        n_items)
{
  GtkListItemManager *items = g_object_get_data (G_OBJECT (widget), "the-items");

  return gtk_list_tile_split (items, tile, n_items);
}

static void
prepare_simple (GtkWidget   *widget,
                GtkListTile *tile,
                guint        n_items)
{
}

static GtkListItemBase *
create_simple_item (GtkWidget *widget)
{
  return g_object_new (GTK_TYPE_LIST_ITEM_BASE, NULL);
}

static GtkListHeaderBase *
create_simple_header (GtkWidget *widget)
{
  return g_object_new (GTK_TYPE_LIST_HEADER_BASE, NULL);
}

static void
test_create (void)
{
  GtkListItemManager *items;
  GtkWidget *widget;

  widget = gtk_window_new ();
  items = gtk_list_item_manager_new (widget,
                                     split_simple,
                                     create_simple_item,
                                     prepare_simple,
                                     create_simple_header);
  g_object_set_data_full (G_OBJECT (widget), "the-items", items, g_object_unref);

  gtk_window_destroy (GTK_WINDOW (widget));
}

static void
test_create_with_items (void)
{
  GListModel *source;
  GtkNoSelection *selection;
  GtkListItemManager *items;
  GtkWidget *widget;

  widget = gtk_window_new ();
  items = gtk_list_item_manager_new (widget,
                                     split_simple,
                                     create_simple_item,
                                     prepare_simple,
                                     create_simple_header);
  g_object_set_data_full (G_OBJECT (widget), "the-items", items, g_object_unref);

  source = create_source_model (1, 50);
  selection = gtk_no_selection_new (G_LIST_MODEL (source));
  gtk_list_item_manager_set_model (items, GTK_SELECTION_MODEL (selection));
  check_list_item_manager (items, widget, NULL, 0);
  gtk_list_item_manager_set_model (items, GTK_SELECTION_MODEL (selection));
  check_list_item_manager (items, widget, NULL, 0);

  g_object_unref (selection);
  gtk_window_destroy (GTK_WINDOW (widget));
}

static void
test_tracker_changes (void)
{
  const char *strings[] = { "0", "1", "2", NULL };
  GtkListItemTracker *tracker;
  GtkNoSelection *selection;
  GtkListItemManager *items;
  GtkWidget *widget;

  widget = gtk_window_new ();
  items = gtk_list_item_manager_new (widget,
                                     split_simple,
                                     create_simple_item,
                                     prepare_simple,
                                     create_simple_header);
  g_object_set_data_full (G_OBJECT (widget), "the-items", items, g_object_unref);
  tracker = gtk_list_item_tracker_new (items);

  g_assert_true (gtk_list_item_tracker_set_position (items, tracker, 1, 2, 3));
  g_assert_false (gtk_list_item_tracker_set_position (items, tracker, 2, 2, 3));

  selection = gtk_no_selection_new (G_LIST_MODEL (gtk_string_list_new (strings)));
  gtk_list_item_manager_set_model (items, GTK_SELECTION_MODEL (selection));

  g_assert_true (gtk_list_item_tracker_set_position (items, tracker, 1, 2, 3));
  g_assert_false (gtk_list_item_tracker_set_position (items, tracker, 1, 2, 3));
  g_assert_true (gtk_list_item_tracker_set_position (items, tracker, 1, 3, 3));
  g_assert_true (gtk_list_item_tracker_set_position (items, tracker, 1, 3, 2));
  g_assert_true (gtk_list_item_tracker_set_position (items, tracker, 99, 3, 2));
  g_assert_false (gtk_list_item_tracker_set_position (items, tracker, 3, 3, 2));

  gtk_list_item_tracker_free (items, tracker);
  g_object_unref (selection);
  gtk_window_destroy (GTK_WINDOW (widget));
}

static void
test_unmapped_changes (gconstpointer data)
{
  guint flags = GPOINTER_TO_UINT (data);
  GtkListItemTracker *tracker = NULL;
  GtkStringList *source;
  GtkNoSelection *selection;
  GtkListItemManager *items;
  GtkWidget *widget;
  guint i;

  widget = gtk_window_new ();
  items = gtk_list_item_manager_new (widget,
                                     split_simple,
                                     create_simple_item,
                                     prepare_simple,
                                     create_simple_header);
  g_object_set_data_full (G_OBJECT (widget), "the-items", items, g_object_unref);

  source = gtk_string_list_new (NULL);
  for (i = 0; i < 64; i++)
    gtk_string_list_append (source, "A");
  selection = gtk_no_selection_new (G_LIST_MODEL (source));
  gtk_list_item_manager_set_model (items, GTK_SELECTION_MODEL (selection));
  gtk_list_item_manager_set_has_sections (items, (flags & 2) != 0);
  if (flags & 1)
    {
      tracker = gtk_list_item_tracker_new (items);
      gtk_list_item_tracker_set_position (items, tracker, 32, 2, 2);
    }

  g_assert_false (gtk_widget_get_mapped (widget));

  for (i = 0; i < 1000; i++)
    {
      const char *replacement[] = { "B", NULL };
      GtkListTile *tile;
      guint n_tiles = 0;

      if (i % 3 == 0)
        gtk_string_list_splice (source, i % 63, 1, NULL);
      else if (i % 3 == 1)
        gtk_string_list_splice (source, i % 63, 0, replacement);
      else
        gtk_string_list_splice (source, i % 63, 1, replacement);

      /* Check before check_list_item_manager(), which collects tiles itself.
       * The model stays bounded while items churn.
       */
      for (tile = gtk_list_item_manager_get_first (items);
           tile != NULL;
           tile = gtk_rb_tree_node_get_next (tile))
        {
          g_assert_cmpint (tile->type, !=, GTK_LIST_TILE_REMOVED);
          n_tiles++;
        }
      /* Up to five tracked items, two untracked spans, and two boundaries. */
      g_assert_cmpuint (n_tiles, <=, tracker ? 9 : 3);
      check_list_item_manager (items, widget, tracker ? &tracker : NULL, tracker ? 1 : 0);
    }

  if (tracker)
    gtk_list_item_tracker_free (items, tracker);
  g_object_unref (selection);
  gtk_window_destroy (GTK_WINDOW (widget));
}

static int
compare_groups (gconstpointer a,
                gconstpointer b,
                gpointer      data)
{
  guint divisor = GPOINTER_TO_UINT (data);
  guint x = g_ascii_strtoull (gtk_string_object_get_string (GTK_STRING_OBJECT ((gpointer) a)), NULL, 10) / divisor;
  guint y = g_ascii_strtoull (gtk_string_object_get_string (GTK_STRING_OBJECT ((gpointer) b)), NULL, 10) / divisor;

  return (int) x - (int) y;
}

static void
count_items_changes (GListModel *model,
                     guint       position,
                     guint       removed,
                     guint       added,
                     gpointer    data)
{
  (*(guint *) data)++;
}

static void
test_unmapped_sections (void)
{
  GtkListItemTracker *tracker;
  GtkListItemManager *items;
  GtkStringList *source;
  GtkSortListModel *sorted;
  GtkNoSelection *selection;
  GtkSorter *groups[2];
  GtkWidget *widget;
  guint i, items_changes = 0;

  widget = gtk_window_new ();
  items = gtk_list_item_manager_new (widget,
                                     split_simple,
                                     create_simple_item,
                                     prepare_simple,
                                     create_simple_header);
  g_object_set_data_full (G_OBJECT (widget), "the-items", items, g_object_unref);

  source = gtk_string_list_new (NULL);
  for (i = 0; i < 64; i++)
    {
      char *text = g_strdup_printf ("%02u", i);
      gtk_string_list_append (source, text);
      g_free (text);
    }
  sorted = gtk_sort_list_model_new (G_LIST_MODEL (source),
                                    GTK_SORTER (gtk_string_sorter_new (gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string"))));
  groups[0] = GTK_SORTER (gtk_custom_sorter_new (compare_groups, GUINT_TO_POINTER (2), NULL));
  groups[1] = GTK_SORTER (gtk_custom_sorter_new (compare_groups, GUINT_TO_POINTER (4), NULL));
  gtk_sort_list_model_set_section_sorter (sorted, groups[0]);
  selection = gtk_no_selection_new (G_LIST_MODEL (sorted));
  gtk_list_item_manager_set_model (items, GTK_SELECTION_MODEL (selection));
  gtk_list_item_manager_set_has_sections (items, TRUE);
  tracker = gtk_list_item_tracker_new (items);
  gtk_list_item_tracker_set_position (items, tracker, 32, 64, 64);

  g_assert_false (gtk_widget_get_mapped (widget));
  g_signal_connect (sorted, "items-changed", G_CALLBACK (count_items_changes), &items_changes);

  for (i = 0; i < 1000; i++)
    {
      GtkListTile *tile;
      guint n_tiles = 0;

      /* Changing group sizes preserves item order, emitting only sections-changed. */
      gtk_sort_list_model_set_section_sorter (sorted, groups[(i + 1) % 2]);
      g_assert_cmpuint (items_changes, ==, 0);

      for (tile = gtk_list_item_manager_get_first (items);
           tile != NULL;
           tile = gtk_rb_tree_node_get_next (tile))
        {
          g_assert_cmpint (tile->type, !=, GTK_LIST_TILE_REMOVED);
          n_tiles++;
        }
      /* 64 tracked items and at most 32 pairs of section boundaries. */
      g_assert_cmpuint (n_tiles, <=, 128);
      check_list_item_manager (items, widget, &tracker, 1);
    }

  gtk_list_item_tracker_free (items, tracker);
  g_object_unref (selection);
  g_object_unref (groups[0]);
  g_object_unref (groups[1]);
  gtk_window_destroy (GTK_WINDOW (widget));
}

enum {
  LIFECYCLE_SETUP = 1,
  LIFECYCLE_BOUND,
};

static void
lifecycle_setup (GtkSignalListItemFactory *factory,
                 GObject                  *object,
                 GHashTable               *states)
{
  g_assert_false (g_hash_table_contains (states, object));
  g_hash_table_insert (states, object, GINT_TO_POINTER (LIFECYCLE_SETUP));
}

static void
lifecycle_bind (GtkSignalListItemFactory *factory,
                GObject                  *object,
                GHashTable               *states)
{
  g_assert_cmpint (GPOINTER_TO_INT (g_hash_table_lookup (states, object)), ==, LIFECYCLE_SETUP);
  g_hash_table_insert (states, object, GINT_TO_POINTER (LIFECYCLE_BOUND));
}

static void
lifecycle_unbind (GtkSignalListItemFactory *factory,
                  GObject                  *object,
                  GHashTable               *states)
{
  g_assert_cmpint (GPOINTER_TO_INT (g_hash_table_lookup (states, object)), ==, LIFECYCLE_BOUND);
  g_hash_table_insert (states, object, GINT_TO_POINTER (LIFECYCLE_SETUP));
}

static void
lifecycle_teardown (GtkSignalListItemFactory *factory,
                    GObject                  *object,
                    GHashTable               *states)
{
  g_assert_cmpint (GPOINTER_TO_INT (g_hash_table_lookup (states, object)), ==, LIFECYCLE_SETUP);
  g_hash_table_remove (states, object);
}

static GtkListItemFactory *
lifecycle_factory_new (GHashTable *states)
{
  GtkListItemFactory *factory;

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (lifecycle_setup), states);
  g_signal_connect (factory, "bind", G_CALLBACK (lifecycle_bind), states);
  g_signal_connect (factory, "unbind", G_CALLBACK (lifecycle_unbind), states);
  g_signal_connect (factory, "teardown", G_CALLBACK (lifecycle_teardown), states);

  return factory;
}

static guint
count_bound (GHashTable *states)
{
  GHashTableIter iter;
  gpointer state;
  guint n_bound = 0;

  g_hash_table_iter_init (&iter, states);
  while (g_hash_table_iter_next (&iter, NULL, &state))
    {
      if (GPOINTER_TO_INT (state) == LIFECYCLE_BOUND)
        n_bound++;
    }

  return n_bound;
}

static GtkWidget *
find_child (GtkWidget  *widget,
            const char *css_name)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (g_str_equal (gtk_widget_get_css_name (child), css_name))
        return child;
    }

  return NULL;
}

static void
test_lifecycle (void)
{
  const char *strings[] = { "a", "a", "b", "c", "c", NULL };
  const char *replacement[] = { "b", "d", NULL };
  GtkListItemFactory *header_factory;
  GtkStringList *source;
  GtkSortListModel *sorted;
  GtkSorter *sections;
  GtkWidget *window, *view, *row, *header;
  GHashTable *states;

  states = g_hash_table_new (NULL, NULL);
  source = gtk_string_list_new (strings);
  sorted = gtk_sort_list_model_new (G_LIST_MODEL (source), NULL);
  sections = GTK_SORTER (gtk_string_sorter_new (gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string")));
  gtk_sort_list_model_set_section_sorter (sorted, sections);
  view = gtk_list_view_new (GTK_SELECTION_MODEL (gtk_no_selection_new (G_LIST_MODEL (sorted))),
                            lifecycle_factory_new (states));
  header_factory = lifecycle_factory_new (states);
  gtk_list_view_set_header_factory (GTK_LIST_VIEW (view), header_factory);
  window = gtk_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), view);
  g_assert_cmpuint (count_bound (states), >, 0);

  gtk_string_list_splice (source, 1, 2, replacement);

  /* Something else holding a released row or header must not keep it bound. */
  row = g_object_ref (find_child (view, "row"));
  header = g_object_ref (find_child (view, "header"));
  gtk_list_view_set_model (GTK_LIST_VIEW (view), NULL);
  g_assert_cmpuint (count_bound (states), ==, 0);

  g_object_unref (row);
  g_object_unref (header);
  gtk_window_destroy (GTK_WINDOW (window));
  g_assert_cmpuint (g_hash_table_size (states), ==, 0);

  g_object_unref (header_factory);
  g_object_unref (sections);
  g_hash_table_unref (states);
}

#define N_TRACKERS 3
#define N_WIDGETS_PER_TRACKER 10
#define N_RUNS 500

static void
print_changes_cb (GListModel *model,
                  guint       position,
                  guint       removed,
                  guint       added,
                  gpointer    unused)
{
  if (!g_test_verbose ())
    return;

  if (removed == 0)
    g_test_message ("%u/%u: adding %u items", position, g_list_model_get_n_items (model), added);
  else if (added == 0)
    g_test_message ("%u/%u: removing %u items", position, g_list_model_get_n_items (model), removed);
  else
    g_test_message ("%u/%u: removing %u and adding %u items", position, g_list_model_get_n_items (model), removed, added);
}

static void
test_exhaustive (void)
{
  GtkListItemTracker *trackers[N_TRACKERS];
  GListStore *store;
  GtkFlattenListModel *flatten;
  GtkNoSelection *selection;
  GtkListItemManager *items;
  GtkWidget *widget;
  gsize i;

  widget = gtk_window_new ();
  items = gtk_list_item_manager_new (widget,
                                     split_simple,
                                     create_simple_item,
                                     prepare_simple,
                                     create_simple_header);
  for (i = 0; i < N_TRACKERS; i++)
    trackers[i] = gtk_list_item_tracker_new (items);

  g_object_set_data_full (G_OBJECT (widget), "the-items", items, g_object_unref);

  store = g_list_store_new (G_TYPE_OBJECT);
  flatten = gtk_flatten_list_model_new (G_LIST_MODEL (store));
  selection = gtk_no_selection_new (G_LIST_MODEL (flatten));
  g_signal_connect (selection, "items-changed", G_CALLBACK (print_changes_cb), NULL);
  gtk_list_item_manager_set_model (items, GTK_SELECTION_MODEL (selection));

  for (i = 0; i < N_RUNS; i++)
    {
      gboolean add = FALSE, remove = FALSE;
      guint position, n_items;

      switch (g_test_rand_int_range (0, 7))
      {
        case 0:
          if (g_test_verbose ())
            g_test_message ("GC and checking");
          check_list_item_manager (items, widget, trackers, N_TRACKERS);
          break;

        case 1:
          /* remove a model */
          remove = TRUE;
          break;

        case 2:
          /* add a model */
          add = TRUE;
          break;

        case 3:
          /* replace a model */
          remove = TRUE;
          add = TRUE;
          break;

        case 4:
          n_items = g_list_model_get_n_items (G_LIST_MODEL (selection));
          if (n_items > 0)
            {
              guint tracker_id = g_test_rand_int_range (0, N_TRACKERS);
              guint pos = g_test_rand_int_range (0, n_items);
              guint n_before = g_test_rand_int_range (0, N_WIDGETS_PER_TRACKER / 2);
              guint n_after = g_test_rand_int_range (0, N_WIDGETS_PER_TRACKER / 2);
              if (g_test_verbose ())
                g_test_message ("setting tracker %u to %u -%u + %u", tracker_id, pos, n_before, n_after);
              gtk_list_item_tracker_set_position (items,
                                                  trackers [tracker_id],
                                                  pos,
                                                  n_before, n_after);
            }
          break;

        case 5:
          {
            gboolean has_sections = g_test_rand_bit ();
            if (g_test_verbose ())
              g_test_message ("Setting has_sections to %s", has_sections ? "true" : "false");
            gtk_list_item_manager_set_has_sections (items, has_sections);
          }
          break;

        case 6:
          {
            n_items = g_list_model_get_n_items (G_LIST_MODEL (store));
            if (n_items > 0)
              {
                guint j = g_test_rand_int_range (0, n_items);
                GListModel *source = g_list_model_get_item (G_LIST_MODEL (store), j);
                guint source_size = g_list_model_get_n_items (G_LIST_MODEL (source));
                GStrvBuilder *builder = g_strv_builder_new ();
                guint inclusion_size = g_test_rand_int_range (1, 11);
                char **inclusion;

                for (j = 0; j < inclusion_size; j++)
                  g_strv_builder_add (builder, g_test_rand_bit () ? "A" : "B");
                inclusion = g_strv_builder_end (builder);
                g_strv_builder_unref (builder);

                j = g_test_rand_int_range (0, source_size + 1);
                gtk_string_list_splice (GTK_STRING_LIST (source), j, 0, (const char * const *) inclusion);
                g_strfreev (inclusion);

                if (g_test_verbose ())
                  g_test_message ("Adding %u items at position %u of a section which had %u items",
                                  inclusion_size, j, source_size);
              }
          }
          break;

        default:
          g_assert_not_reached ();
          break;
      }

      position = g_test_rand_int_range (0, g_list_model_get_n_items (G_LIST_MODEL (store)) + 1);
      if (g_list_model_get_n_items (G_LIST_MODEL (store)) == position)
        remove = FALSE;

      if (add)
        {
          /* We want at least one element, otherwise the filters will see no changes */
          GListModel *source = create_source_model (1, 50);
          g_list_store_splice (store,
                               position,
                               remove ? 1 : 0,
                               (gpointer *) &source, 1);
          g_object_unref (source);
        }
      else if (remove)
        {
          g_list_store_remove (store, position);
        }

      if (g_test_verbose ())
        print_list_item_manager_tiles (items);
    }

  check_list_item_manager (items, widget, trackers, N_TRACKERS);

  if (g_test_verbose ())
    g_test_message ("removing trackers");
  for (i = 0; i < N_TRACKERS; i++)
    gtk_list_item_tracker_free (items, trackers[i]);
  g_object_unref (selection);
  if (g_test_verbose ())
    print_list_item_manager_tiles (items);
  check_list_item_manager (items, widget, NULL, 0);

  gtk_window_destroy (GTK_WINDOW (widget));
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/listitemmanager/create", test_create);
  g_test_add_func ("/listitemmanager/create_with_items", test_create_with_items);
  g_test_add_data_func ("/listitemmanager/unmapped", GUINT_TO_POINTER (0), test_unmapped_changes);
  g_test_add_data_func ("/listitemmanager/unmapped/tracker", GUINT_TO_POINTER (1), test_unmapped_changes);
  g_test_add_data_func ("/listitemmanager/unmapped/sections", GUINT_TO_POINTER (2), test_unmapped_changes);
  g_test_add_data_func ("/listitemmanager/unmapped/sections-tracker", GUINT_TO_POINTER (3), test_unmapped_changes);
  g_test_add_func ("/listitemmanager/unmapped/section-changes", test_unmapped_sections);
  g_test_add_func ("/listitemmanager/tracker/changes", test_tracker_changes);
  g_test_add_func ("/listitemmanager/lifecycle", test_lifecycle);
  g_test_add_func ("/listitemmanager/exhaustive", test_exhaustive);

  return g_test_run ();
}
