#include <gtk/gtk.h>

typedef struct
{
  const char *name;
  GtkTextDirection direction;
  int content_width;
  double value;
  double lower;
  double upper;
  int expected_value;
} AllocationCase;

static const AllocationCase allocation_cases[] = {
  { "fits", GTK_TEXT_DIR_LTR, 600, 980, 0, 3000, 0 },
  { "fits", GTK_TEXT_DIR_RTL, 600, 980, 0, 3000, 0 },
  { "overflows", GTK_TEXT_DIR_LTR, 1200, 980, 0, 3000, 400 },
  { "overflows", GTK_TEXT_DIR_RTL, 1200, 980, 0, 3000, 400 },
  { "unchanged", GTK_TEXT_DIR_LTR, 1200, 250, 0, 3000, 250 },
  { "unchanged", GTK_TEXT_DIR_RTL, 1200, 250, 0, 3000, 250 },
  { "fractional", GTK_TEXT_DIR_LTR, 1200, 250.75, 0, 3000, 250 },
  { "fractional", GTK_TEXT_DIR_RTL, 1200, 250.75, 0, 3000, 250 },
  { "negative", GTK_TEXT_DIR_LTR, 1200, -40, -100, 3000, 0 },
  { "negative", GTK_TEXT_DIR_RTL, 1200, -40, -100, 3000, 0 },
  { "large", GTK_TEXT_DIR_LTR, 1200, (double) G_MAXINT + 1000, 0, (double) G_MAXINT + 5000, 400 },
  { "large", GTK_TEXT_DIR_RTL, 1200, (double) G_MAXINT + 1000, 0, (double) G_MAXINT + 5000, 400 },
};

static GtkWidget *
create_view (GtkTextDirection direction,
             int              content_width)
{
  GtkWidget *view = gtk_column_view_new (NULL);

  g_object_ref_sink (view);
  gtk_widget_set_direction (view, direction);

  for (guint i = 0; i < 2; i++)
    {
      GtkColumnViewColumn *column = gtk_column_view_column_new ("Column", NULL);

      gtk_column_view_column_set_fixed_width (column, content_width / 2);
      gtk_column_view_append_column (GTK_COLUMN_VIEW (view), column);
      g_object_unref (column);
    }

  return view;
}

static void
allocate_and_check (GtkWidget *view,
                    int        width,
                    int        content_width,
                    int        value)
{
  GtkAdjustment *adjustment;
  GtkWidget *child;
  int minimum, x, n_children = 0;

  gtk_widget_measure (view, GTK_ORIENTATION_HORIZONTAL, -1,
                      &minimum, NULL, NULL, NULL);
  gtk_widget_measure (view, GTK_ORIENTATION_VERTICAL, MAX (width, minimum),
                      NULL, NULL, NULL, NULL);
  gtk_widget_allocate (view, width, 200, -1, NULL);

  adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (view));
  g_assert_cmpfloat (gtk_adjustment_get_lower (adjustment), ==, 0);
  g_assert_cmpfloat (gtk_adjustment_get_upper (adjustment), ==, MAX (width, content_width));
  g_assert_cmpfloat (gtk_adjustment_get_page_size (adjustment), ==, width);
  g_assert_cmpfloat (gtk_adjustment_get_value (adjustment), ==, value);

  x = gtk_widget_get_direction (view) == GTK_TEXT_DIR_RTL
      ? width - content_width + value : -value;

  for (child = gtk_widget_get_first_child (view);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      graphene_rect_t bounds;

      if (!GTK_IS_LIST_VIEW (child) &&
          g_strcmp0 (gtk_widget_get_css_name (child), "header") != 0)
        continue;

      g_assert_true (gtk_widget_compute_bounds (child, view, &bounds));
      g_assert_cmpfloat (bounds.size.width, ==, content_width);
      g_assert_cmpfloat (bounds.origin.x, ==, x);
      n_children++;
    }

  g_assert_cmpint (n_children, ==, 2);
}

static void
test_initial_allocation (gconstpointer data)
{
  const AllocationCase *test = data;
  GtkWidget *view = create_view (test->direction, test->content_width);
  GtkAdjustment *adjustment;

  adjustment = gtk_adjustment_new (test->value, test->lower, test->upper, 80, 720, 800);
  gtk_scrollable_set_hadjustment (GTK_SCROLLABLE (view), adjustment);
  g_assert_cmpfloat (gtk_adjustment_get_value (adjustment), ==, test->value);

  allocate_and_check (view, 800, test->content_width, test->expected_value);

  g_object_unref (view);
}

static void
test_column_changes (gconstpointer data)
{
  GtkWidget *view = create_view (GPOINTER_TO_INT (data), 1800);
  GtkAdjustment *adjustment;
  GtkColumnViewColumn *column;
  GListModel *columns = gtk_column_view_get_columns (GTK_COLUMN_VIEW (view));

  adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (view));
  gtk_adjustment_configure (adjustment, 980, 0, 1800, 80, 720, 800);
  allocate_and_check (view, 800, 1800, 980);

  for (guint i = 0; i < 2; i++)
    {
      column = g_list_model_get_item (columns, i);
      gtk_column_view_column_set_fixed_width (column, 600);
      g_object_unref (column);
    }

  allocate_and_check (view, 800, 1200, 400);

  column = g_list_model_get_item (columns, 1);
  gtk_column_view_column_set_visible (column, FALSE);
  g_object_unref (column);
  allocate_and_check (view, 800, 600, 0);

  g_object_unref (view);
}

static void
test_viewport_changes (gconstpointer data)
{
  GtkWidget *view = create_view (GPOINTER_TO_INT (data), 1200);
  GtkAdjustment *adjustment;

  adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (view));
  gtk_adjustment_configure (adjustment, 800, 0, 1200, 40, 360, 400);
  allocate_and_check (view, 400, 1200, 800);
  allocate_and_check (view, 800, 1200, 400);
  allocate_and_check (view, 1400, 1200, 0);

  g_object_unref (view);
}

static void
test_same_hadjustment (void)
{
  GtkWidget *view = create_view (GTK_TEXT_DIR_LTR, 1200);
  GtkAdjustment *adjustment;
  gpointer weak;

  adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (view));
  weak = adjustment;
  g_object_add_weak_pointer (G_OBJECT (adjustment), &weak);
  gtk_scrollable_set_hadjustment (GTK_SCROLLABLE (view), adjustment);

  g_object_unref (view);
  g_assert_null (weak);
}

int
main (int argc, char *argv[])
{
  GtkCssProvider *provider;
  int result;

  gtk_test_init (&argc, &argv, NULL);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (provider,
                                     "columnview, columnview * { margin: 0; padding: 0; border: 0; }");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  for (guint i = 0; i < G_N_ELEMENTS (allocation_cases); i++)
    {
      const AllocationCase *test = &allocation_cases[i];
      char *path = g_strdup_printf ("/columnview/initial-allocation/%s/%s",
                                    test->direction == GTK_TEXT_DIR_RTL ? "rtl" : "ltr",
                                    test->name);

      g_test_add_data_func (path, test, test_initial_allocation);
      g_free (path);
    }

  g_test_add_data_func ("/columnview/column-changes/ltr", GINT_TO_POINTER (GTK_TEXT_DIR_LTR), test_column_changes);
  g_test_add_data_func ("/columnview/column-changes/rtl", GINT_TO_POINTER (GTK_TEXT_DIR_RTL), test_column_changes);
  g_test_add_data_func ("/columnview/viewport-changes/ltr", GINT_TO_POINTER (GTK_TEXT_DIR_LTR), test_viewport_changes);
  g_test_add_data_func ("/columnview/viewport-changes/rtl", GINT_TO_POINTER (GTK_TEXT_DIR_RTL), test_viewport_changes);
  g_test_add_func ("/columnview/hadjustment/same", test_same_hadjustment);
  result = g_test_run ();

  gtk_style_context_remove_provider_for_display (gdk_display_get_default (), GTK_STYLE_PROVIDER (provider));
  g_object_unref (provider);

  return result;
}
