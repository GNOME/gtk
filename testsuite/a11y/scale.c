#include <gtk/gtk.h>

static void
scale_role (void)
{
  GtkWidget *widget = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, NULL);
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_role (widget, GTK_ACCESSIBLE_ROLE_SLIDER);

  g_object_unref (widget);
}

static void
scale_state (void)
{
  GtkWidget *widget = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, NULL);
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_state (widget, GTK_ACCESSIBLE_STATE_DISABLED, FALSE);

  gtk_widget_set_sensitive (widget, FALSE);

  gtk_test_accessible_assert_state (widget, GTK_ACCESSIBLE_STATE_DISABLED, TRUE);

  g_object_unref (widget);
}

static void
scale_properties (void)
{
  GtkAdjustment *adj = gtk_adjustment_new (0.0, 0.0, 100.0, 1.0, 10.0, 10.0);
  GtkWidget *widget = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adj);
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_ORIENTATION, GTK_ORIENTATION_HORIZONTAL);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, 90.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, 0.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, 0.);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (widget), GTK_ORIENTATION_VERTICAL);
  gtk_adjustment_set_value (adj, 50.);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_ORIENTATION, GTK_ORIENTATION_VERTICAL);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, 90.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, 0.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, 50.);

  gtk_range_set_fill_level (GTK_RANGE (widget), 25.);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_ORIENTATION, GTK_ORIENTATION_VERTICAL);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, 25.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, 0.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, 25.);

  gtk_range_set_restrict_to_fill_level (GTK_RANGE (widget), FALSE);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_ORIENTATION, GTK_ORIENTATION_VERTICAL);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, 90.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, 0.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, 25.);

  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/scale/role", scale_role);
  g_test_add_func ("/a11y/scale/state", scale_state);
  g_test_add_func ("/a11y/scale/properties", scale_properties);

  return g_test_run ();
}
