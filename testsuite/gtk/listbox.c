#include <gtk/gtk.h>

static gint
sort_list (GtkListBoxRow *row1,
           GtkListBoxRow *row2,
           gpointer       data)
{
  GtkWidget *label1, *label2;
  gint n1, n2;
  gint *count = data;

  (*count)++;

  label1 = gtk_bin_get_child (GTK_BIN (row1));
  n1 = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (label1), "data"));

  label2 = gtk_bin_get_child (GTK_BIN (row2));
  n2 = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (label2), "data"));

  return (n1 - n2);
}

static void
check_sorted (GtkListBox *list)
{
  GList *children;
  GtkWidget *row, *label;
  gint n1, n2;
  GList *l;

  n2 = n1 = 0;
  children = gtk_container_get_children (GTK_CONTAINER (list));
  for (l = children; l; l = l->next)
    {
      row = l->data;
      n1 = n2;
      label = gtk_bin_get_child (GTK_BIN (row));
      n2 = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (label), "data"));
      g_assert_cmpint (n1, <=, n2);
    }
  g_list_free (children);
}

static void
test_sort (void)
{
  GtkListBox *list;
  GtkListBoxRow *row;
  GtkWidget *label;
  gint i, r;
  gchar *s;
  gint count;

  list = GTK_LIST_BOX (gtk_list_box_new ());
  g_object_ref_sink (list);
  gtk_widget_show (GTK_WIDGET (list));

  for (i = 0; i < 100; i++)
    {
      r = g_random_int_range (0, 1000);
      s = g_strdup_printf ("%d: %d", i, r);
      label = gtk_label_new (s);
      g_object_set_data (G_OBJECT (label), "data", GINT_TO_POINTER (r));
      g_free (s);
      gtk_container_add (GTK_CONTAINER (list), label);
    }

  count = 0;
  gtk_list_box_set_sort_func (list, sort_list, &count, NULL);
  g_assert_cmpint (count, >, 0);

  check_sorted (list);

  count = 0;
  gtk_list_box_invalidate_sort (list);
  g_assert_cmpint (count, >, 0);

  count = 0;
  row = gtk_list_box_get_row_at_index (list, 0);
  gtk_list_box_row_changed (row);
  g_assert_cmpint (count, >, 0);

  g_object_unref (list);
}

static GtkListBoxRow *callback_row;

static void
on_row_selected (GtkListBox    *list_box,
                 GtkListBoxRow *row,
                 gpointer       data)
{
  gint *i = data;

  (*i)++;

  callback_row = row;
}

static void
test_selection (void)
{
  GtkListBox *list;
  GtkListBoxRow *row, *row2;
  GtkWidget *label;
  gint i;
  gchar *s;
  gint count;
  gint index;

  list = GTK_LIST_BOX (gtk_list_box_new ());
  g_object_ref_sink (list);
  gtk_widget_show (GTK_WIDGET (list));

  g_assert_cmpint (gtk_list_box_get_selection_mode (list), ==, GTK_SELECTION_SINGLE);
  g_assert (gtk_list_box_get_selected_row (list) == NULL);

  for (i = 0; i < 100; i++)
    {
      s = g_strdup_printf ("%d", i);
      label = gtk_label_new (s);
      g_object_set_data (G_OBJECT (label), "data", GINT_TO_POINTER (i));
      g_free (s);
      gtk_container_add (GTK_CONTAINER (list), label);
    }

  count = 0;
  g_signal_connect (list, "row-selected",
                    G_CALLBACK (on_row_selected),
                    &count);

  row = gtk_list_box_get_row_at_index (list, 20);
  g_assert (!gtk_list_box_row_is_selected (row));
  gtk_list_box_select_row (list, row);
  g_assert (gtk_list_box_row_is_selected (row));
  g_assert (callback_row == row);
  g_assert_cmpint (count, ==, 1);
  row2 = gtk_list_box_get_selected_row (list);
  g_assert (row2 == row);

  gtk_list_box_set_selection_mode (list, GTK_SELECTION_BROWSE);
  gtk_container_remove (GTK_CONTAINER (list), GTK_WIDGET (row));
  g_assert (callback_row == NULL);
  g_assert_cmpint (count, ==, 2);
  row2 = gtk_list_box_get_selected_row (list);
  g_assert (row2 == NULL);

  row = gtk_list_box_get_row_at_index (list, 20);
  gtk_list_box_select_row (list, row);
  g_assert (gtk_list_box_row_is_selected (row));
  g_assert (callback_row == row);
  g_assert_cmpint (count, ==, 3);

  gtk_list_box_set_selection_mode (list, GTK_SELECTION_NONE);
  g_assert (!gtk_list_box_row_is_selected (row));
  g_assert (callback_row == NULL);
  g_assert_cmpint (count, ==, 4);
  row2 = gtk_list_box_get_selected_row (list);
  g_assert (row2 == NULL);

  row = gtk_list_box_get_row_at_index (list, 20);
  index = gtk_list_box_row_get_index (row);
  g_assert_cmpint (index, ==, 20);

  row = GTK_LIST_BOX_ROW (gtk_list_box_row_new ());
  g_object_ref_sink (row);
  index = gtk_list_box_row_get_index (row);
  g_assert_cmpint (index, ==, -1);
  g_object_unref (row);

  g_object_unref (list);
}

static void
on_selected_rows_changed (GtkListBox *box, gpointer data)
{
  gint *i = data;

  (*i)++;
}

static void
test_multi_selection (void)
{
  GtkListBox *list;
  GList *l;
  GtkListBoxRow *row, *row2;
  GtkWidget *label;
  gint i;
  gchar *s;
  gint count;

  list = GTK_LIST_BOX (gtk_list_box_new ());
  g_object_ref_sink (list);
  gtk_widget_show (GTK_WIDGET (list));

  g_assert_cmpint (gtk_list_box_get_selection_mode (list), ==, GTK_SELECTION_SINGLE);
  g_assert (gtk_list_box_get_selected_rows (list) == NULL);

  gtk_list_box_set_selection_mode (list, GTK_SELECTION_MULTIPLE);

  for (i = 0; i < 100; i++)
    {
      s = g_strdup_printf ("%d", i);
      label = gtk_label_new (s);
      g_object_set_data (G_OBJECT (label), "data", GINT_TO_POINTER (i));
      g_free (s);
      gtk_container_add (GTK_CONTAINER (list), label);
    }

  count = 0;
  g_signal_connect (list, "selected-rows-changed",
                    G_CALLBACK (on_selected_rows_changed),
                    &count);

  row = gtk_list_box_get_row_at_index (list, 20);

  gtk_list_box_select_all (list);
  g_assert_cmpint (count, ==, 1);
  l = gtk_list_box_get_selected_rows (list);
  g_assert_cmpint (g_list_length (l), ==, 100);
  g_list_free (l);
  g_assert (gtk_list_box_row_is_selected (row));

  gtk_list_box_unselect_all (list);
  g_assert_cmpint (count, ==, 2);
  l = gtk_list_box_get_selected_rows (list);
  g_assert (l == NULL);
  g_assert (!gtk_list_box_row_is_selected (row));

  gtk_list_box_select_row (list, row);
  g_assert (gtk_list_box_row_is_selected (row));
  g_assert_cmpint (count, ==, 3);
  l = gtk_list_box_get_selected_rows (list);
  g_assert_cmpint (g_list_length (l), ==, 1);
  g_assert (l->data == row);
  g_list_free (l);

  row2 = gtk_list_box_get_row_at_index (list, 40);
  g_assert (!gtk_list_box_row_is_selected (row2));
  gtk_list_box_select_row (list, row2);
  g_assert (gtk_list_box_row_is_selected (row2));
  g_assert_cmpint (count, ==, 4);
  l = gtk_list_box_get_selected_rows (list);
  g_assert_cmpint (g_list_length (l), ==, 2);
  g_assert (l->data == row);
  g_assert (l->next->data == row2);
  g_list_free (l);

  gtk_list_box_unselect_row (list, row);
  g_assert (!gtk_list_box_row_is_selected (row));
  g_assert_cmpint (count, ==, 5);
  l = gtk_list_box_get_selected_rows (list);
  g_assert_cmpint (g_list_length (l), ==, 1);
  g_assert (l->data == row2);
  g_list_free (l);

  g_object_unref (list);
}

static gboolean
filter_func (GtkListBoxRow *row,
             gpointer       data)
{
  gint *count = data;
  GtkWidget *child;
  gint i;

  (*count)++;

  child = gtk_bin_get_child (GTK_BIN (row));
  i = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (child), "data"));

  return (i % 2) == 0;
}

static void
check_filtered (GtkListBox *list)
{
  GList *children, *l;
  gint count;
  GtkWidget *row;

  count = 0;
  children = gtk_container_get_children (GTK_CONTAINER (list));
  for (l = children; l; l = l->next)
    {
      row = l->data;
      if (gtk_widget_get_child_visible (row))
        count++;
    }
  g_list_free (children);
  g_assert_cmpint (count, ==, 50);
}

static void
test_filter (void)
{
  GtkListBox *list;
  GtkListBoxRow *row;
  gint i;
  gchar *s;
  GtkWidget *label;
  gint count;

  list = GTK_LIST_BOX (gtk_list_box_new ());
  g_object_ref_sink (list);
  gtk_widget_show (GTK_WIDGET (list));

  g_assert_cmpint (gtk_list_box_get_selection_mode (list), ==, GTK_SELECTION_SINGLE);
  g_assert (gtk_list_box_get_selected_row (list) == NULL);

  for (i = 0; i < 100; i++)
    {
      s = g_strdup_printf ("%d", i);
      label = gtk_label_new (s);
      g_object_set_data (G_OBJECT (label), "data", GINT_TO_POINTER (i));
      g_free (s);
      gtk_container_add (GTK_CONTAINER (list), label);
    }

  count = 0;
  gtk_list_box_set_filter_func (list, filter_func, &count, NULL);
  g_assert_cmpint (count, >, 0);

  check_filtered (list);

  count = 0;
  gtk_list_box_invalidate_filter (list);
  g_assert_cmpint (count, >, 0);

  count = 0;
  row = gtk_list_box_get_row_at_index (list, 0);
  gtk_list_box_row_changed (row);
  g_assert_cmpint (count, >, 0);

  g_object_unref (list);
}

static void
header_func (GtkListBoxRow *row,
             GtkListBoxRow *before,
             gpointer       data)
{
  GtkWidget *child;
  gint i;
  gint *count = data;
  GtkWidget *header;
  gchar *s;

  (*count)++;

  child = gtk_bin_get_child (GTK_BIN (row));
  i = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (child), "data"));

  if (i % 2 == 0)
    {
      s = g_strdup_printf ("Header %d", i);
      header = gtk_label_new (s);
      g_free (s);
    }
  else
    header = NULL;

  gtk_list_box_row_set_header (row, header);
}

static void
check_headers (GtkListBox *list)
{
  GList *children, *l;
  gint count;
  GtkListBoxRow *row;

  count = 0;
  children = gtk_container_get_children (GTK_CONTAINER (list));
  for (l = children; l; l = l->next)
    {
      row = l->data;
      if (gtk_list_box_row_get_header (row) != NULL)
        count++;
    }
  g_list_free (children);
  g_assert_cmpint (count, ==, 50);
}

static void
test_header (void)
{
  GtkListBox *list;
  GtkListBoxRow *row;
  gint i;
  gchar *s;
  GtkWidget *label;
  gint count;

  list = GTK_LIST_BOX (gtk_list_box_new ());
  g_object_ref_sink (list);
  gtk_widget_show (GTK_WIDGET (list));

  g_assert_cmpint (gtk_list_box_get_selection_mode (list), ==, GTK_SELECTION_SINGLE);
  g_assert (gtk_list_box_get_selected_row (list) == NULL);

  for (i = 0; i < 100; i++)
    {
      s = g_strdup_printf ("%d", i);
      label = gtk_label_new (s);
      g_object_set_data (G_OBJECT (label), "data", GINT_TO_POINTER (i));
      g_free (s);
      gtk_container_add (GTK_CONTAINER (list), label);
    }

  count = 0;
  gtk_list_box_set_header_func (list, header_func, &count, NULL);
  g_assert_cmpint (count, >, 0);

  check_headers (list);

  count = 0;
  gtk_list_box_invalidate_headers (list);
  g_assert_cmpint (count, >, 0);

  count = 0;
  row = gtk_list_box_get_row_at_index (list, 0);
  gtk_list_box_row_changed (row);
  g_assert_cmpint (count, >, 0);

  g_object_unref (list);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/listbox/sort", test_sort);
  g_test_add_func ("/listbox/selection", test_selection);
  g_test_add_func ("/listbox/multi-selection", test_multi_selection);
  g_test_add_func ("/listbox/filter", test_filter);
  g_test_add_func ("/listbox/header", test_header);

  return g_test_run ();
}
