#include <gtk/gtk.h>

static void
label_role (void)
{
  GtkWidget *label = gtk_label_new ("a");

  g_object_ref_sink (label);

  gtk_test_accessible_assert_role (GTK_ACCESSIBLE (label), GTK_ACCESSIBLE_ROLE_LABEL);

  g_object_unref (label);
}

static void
label_relations (void)
{
  GtkWidget *label = gtk_label_new ("a");
  GtkWidget *label2 = gtk_label_new ("b");
  GtkWidget *entry = gtk_entry_new ();

  g_object_ref_sink (label);
  g_object_ref_sink (label2);
  g_object_ref_sink (entry);

  gtk_test_accessible_assert_relation (GTK_ACCESSIBLE (entry), GTK_ACCESSIBLE_RELATION_LABELLED_BY, NULL);

  gtk_widget_add_mnemonic_label (entry, label);

  gtk_test_accessible_assert_relation (GTK_ACCESSIBLE (entry), GTK_ACCESSIBLE_RELATION_LABELLED_BY, label, NULL);

  gtk_widget_add_mnemonic_label (entry, label2);

  gtk_test_accessible_assert_relation (GTK_ACCESSIBLE (entry), GTK_ACCESSIBLE_RELATION_LABELLED_BY, label, label2, NULL);

  g_object_unref (entry);
  g_object_unref (label);
  g_object_unref (label2);
}

static void
label_properties (void)
{
  GtkWidget *label = gtk_label_new ("a");

  g_object_ref_sink (label);

  gtk_test_accessible_assert_property (label, GTK_ACCESSIBLE_PROPERTY_LABEL, "a");

  gtk_label_set_label (GTK_LABEL (label), "b");

  gtk_test_accessible_assert_property (label, GTK_ACCESSIBLE_PROPERTY_LABEL, "b");

  g_object_unref (label);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/label/role", label_role);
  g_test_add_func ("/a11y/label/relations", label_relations);
  g_test_add_func ("/a11y/label/properties", label_properties);

  return g_test_run ();
}
