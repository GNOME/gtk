#include <gtk/gtk.h>

static void
textview_role (void)
{
  GtkWidget *widget = gtk_text_view_new ();
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_role (widget, GTK_ACCESSIBLE_ROLE_TEXT_BOX);

  g_object_unref (widget);
}

static void
textview_properties (void)
{
  GtkWidget *widget = gtk_text_view_new ();
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_MULTI_LINE, TRUE);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_READ_ONLY, FALSE);

  gtk_text_view_set_editable (GTK_TEXT_VIEW (widget), FALSE);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_READ_ONLY, TRUE);

  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/textview/role", textview_role);
  g_test_add_func ("/a11y/textview/properties", textview_properties);

  return g_test_run ();
}
