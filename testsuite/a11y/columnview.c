#include <gtk/gtk.h>

#include "gtk/gtkactionmuxerprivate.h"
#include "gtk/gtkcolumnviewcolumnprivate.h"
#include "gtk/gtkwidgetprivate.h"

static gboolean
get_activate_enabled (GtkWidget *widget)
{
  GtkActionMuxer *muxer;
  gboolean enabled;

  muxer = _gtk_widget_get_action_muxer (widget, FALSE);
  g_assert_nonnull (muxer);
  g_assert_true (gtk_action_muxer_query_action (muxer, "activate",
                                                &enabled,
                                                NULL, NULL, NULL, NULL));

  return enabled;
}

static void
column_header (void)
{
  GtkWidget *view = gtk_column_view_new (NULL);
  GtkWidget *window = gtk_window_new ();
  GtkColumnViewColumn *column;
  GtkColumnViewColumn *primary;
  GtkExpression *expression;
  GtkSorter *sorter;
  GtkSorter *sort_model;
  GtkColumnViewSorter *view_sorter;
  GtkSortType sort_order;
  GtkWidget *header_row;
  GtkWidget *title;

  g_object_ref_sink (view);
  gtk_window_set_child (GTK_WINDOW (window), view);

  column = gtk_column_view_column_new ("Value", NULL);
  expression = gtk_property_expression_new (GTK_TYPE_ADJUSTMENT, NULL, "value");
  sorter = GTK_SORTER (gtk_numeric_sorter_new (expression));
  gtk_column_view_column_set_sorter (column, sorter);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (view), column);

  title = gtk_column_view_column_get_header (column);
  header_row = gtk_widget_get_parent (title);
  sort_model = gtk_column_view_get_sorter (GTK_COLUMN_VIEW (view));
  view_sorter = GTK_COLUMN_VIEW_SORTER (sort_model);

  gtk_test_accessible_assert_role (title, GTK_ACCESSIBLE_ROLE_COLUMN_HEADER);
  g_assert_false (gtk_widget_get_focusable (title));
  g_assert_false (gtk_widget_get_can_focus (header_row));
  g_assert_true (get_activate_enabled (title));

  primary = gtk_column_view_sorter_get_primary_sort_column (view_sorter);
  g_assert_null (primary);

  g_assert_true (gtk_widget_activate (title));
  primary = gtk_column_view_sorter_get_primary_sort_column (view_sorter);
  sort_order = gtk_column_view_sorter_get_primary_sort_order (view_sorter);
  g_assert_true (primary == column);
  g_assert_cmpint (sort_order, ==, GTK_SORT_ASCENDING);

  g_assert_true (gtk_widget_activate_action (title, "activate", NULL));
  sort_order = gtk_column_view_sorter_get_primary_sort_order (view_sorter);
  g_assert_cmpint (sort_order, ==, GTK_SORT_DESCENDING);

  gtk_column_view_column_set_sorter (column, NULL);
  primary = gtk_column_view_sorter_get_primary_sort_column (view_sorter);
  g_assert_null (primary);
  g_assert_false (get_activate_enabled (title));
  g_assert_true (gtk_widget_activate_action (title, "activate", NULL));
  primary = gtk_column_view_sorter_get_primary_sort_column (view_sorter);
  g_assert_null (primary);

  g_object_unref (column);
  g_object_unref (sorter);
  gtk_window_destroy (GTK_WINDOW (window));
  g_object_unref (view);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);
  g_test_add_func ("/a11y/columnview/header", column_header);
  return g_test_run ();
}
