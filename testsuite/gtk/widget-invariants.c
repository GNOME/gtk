#include <gtk/gtk.h>

typedef struct {
  int k;
  int p;
} ExampleData;

static gboolean example_data_free_called = FALSE;

static void
example_data_free (ExampleData *data)
{
  g_free (data);
  example_data_free_called = TRUE;
}

static void
parent_tag (void)
{
  GtkWidget *child;
  GtkWidget *parent;
  ExampleData *data;

  /* _{get,set}_parent_tag should ONLY be used by the parent widget,
   * but for the purposes of testing it, we break that rule here.
   */

  child = gtk_button_new ();
  g_object_ref_sink (child);

  parent = gtk_button_new ();
  g_object_ref_sink (parent);
  g_assert_null (gtk_widget_get_parent_tag (child));

  data = g_new0 (ExampleData, 1);
  data->k = 10;
  data->p = 40;

  gtk_widget_set_parent (child, parent);
  gtk_widget_set_parent_tag (child, data, (GDestroyNotify)example_data_free);
  g_assert (gtk_widget_get_parent_tag (child) == data);

  gtk_widget_unparent (child);
  g_assert_null (gtk_widget_get_parent_tag (child));
  g_assert (example_data_free_called);


  g_object_unref (child);
  g_object_unref (parent);
}

int main (int argc, char **argv)
{
  gtk_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/invariants/parent-tag", parent_tag);

  return g_test_run ();
}
