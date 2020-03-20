#include <gtk/gtk.h>

static void
test_type (gconstpointer data)
{
  GType t = *(GType *)data;
  GtkWidget *w;
  AtkObject *a;

  w = (GtkWidget *)g_object_new (t, NULL);
  if (g_type_is_a (t, G_TYPE_INITIALLY_UNOWNED))
    g_object_ref_sink (w);

  a = gtk_widget_get_accessible (w);

  g_assert (GTK_IS_ACCESSIBLE (a));
  g_assert (gtk_accessible_get_widget (GTK_ACCESSIBLE (a)) == w);

  g_object_unref (w);
}

int
main (int argc, char *argv[])
{
  const GType *tp;
  guint i, n;

  gtk_test_init (&argc, &argv, NULL);
  gtk_test_register_all_types ();

  tp = gtk_test_list_all_types (&n);

  for (i = 0; i < n; i++)
    {
      char *testname;

      if (!g_type_is_a (tp[i], GTK_TYPE_WIDGET) ||
          G_TYPE_IS_ABSTRACT (tp[i]) ||
          !G_TYPE_IS_INSTANTIATABLE (tp[i]))
        continue;

      testname = g_strdup_printf ("/Accessible/%s", g_type_name (tp[i]));
      g_test_add_data_func (testname, &tp[i], test_type);

      g_free (testname);
    }

  return g_test_run ();
}
