#include <gtk/gtk.h>

static void
label_relations (void)
{
  GtkWidget *label = gtk_label_new ("a");
  GtkWidget *label2 = gtk_label_new ("b");
  GtkWidget *entry = gtk_entry_new ();
  GList *list;

  g_object_ref_sink (label);
  g_object_ref_sink (label2);
  g_object_ref_sink (entry);

  gtk_test_accessible_assert_relation (GTK_ACCESSIBLE (entry), GTK_ACCESSIBLE_RELATION_LABELLED_BY, NULL);

  gtk_widget_add_mnemonic_label (entry, label);

  list = g_list_append (NULL, label);
  gtk_test_accessible_assert_relation (GTK_ACCESSIBLE (entry), GTK_ACCESSIBLE_RELATION_LABELLED_BY, list);
  g_list_free (list);

  gtk_widget_add_mnemonic_label (entry, label2);

  list = g_list_append (NULL, label);
  list = g_list_append (list, label2);
  gtk_test_accessible_assert_relation (GTK_ACCESSIBLE (entry), GTK_ACCESSIBLE_RELATION_LABELLED_BY, list);
  g_list_free (list);

  g_object_unref (entry);
  g_object_unref (label);
  g_object_unref (label2);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/label/relations", label_relations);

  return g_test_run ();
}
