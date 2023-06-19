#include <gtk/gtk.h>
#include "gtk/gtkatcontextprivate.h"
#include "gtk/gtkwidgetprivate.h"

static void
test_name_content (void)
{
  GtkWidget *label1, *label2, *box, *button;
  char *name;

  label1 = gtk_label_new ("a");
  label2 = gtk_label_new ("b");
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  button = gtk_button_new ();

  gtk_box_append (GTK_BOX (box), label1);
  gtk_box_append (GTK_BOX (box), label2);
  gtk_button_set_child (GTK_BUTTON (button), box);
  g_object_ref_sink (button);

  /* gtk_at_context_get_name only works on realized contexts */
  gtk_widget_realize_at_context (label1);
  gtk_widget_realize_at_context (label2);
  gtk_widget_realize_at_context (box);
  gtk_widget_realize_at_context (button);

  name = gtk_at_context_get_name (gtk_accessible_get_at_context (GTK_ACCESSIBLE (label1)));
  g_assert_cmpstr (name, ==, "a");
  g_free (name);

  /* this is because generic doesn't allow naming */
  name = gtk_at_context_get_name (gtk_accessible_get_at_context (GTK_ACCESSIBLE (box)));
  g_assert_cmpstr (name, ==, "");
  g_free (name);

  name = gtk_at_context_get_name (gtk_accessible_get_at_context (GTK_ACCESSIBLE (button)));
  g_assert_cmpstr (name, ==, "a b");
  g_free (name);

  gtk_widget_set_visible (label2, FALSE);

  name = gtk_at_context_get_name (gtk_accessible_get_at_context (GTK_ACCESSIBLE (button)));
  g_assert_cmpstr (name, ==, "a");
  g_free (name);

  g_object_unref (button);
}

static void
test_name_tooltip (void)
{
  GtkWidget *image = gtk_image_new ();
  char *name;

  g_object_ref_sink (image);
  gtk_widget_realize_at_context (image);

  gtk_widget_set_tooltip_text (image, "tooltip");

  name = gtk_at_context_get_name (gtk_accessible_get_at_context (GTK_ACCESSIBLE (image)));
  g_assert_cmpstr (name, ==, "tooltip");
  g_free (name);

  g_object_unref (image);
}

static void
test_name_label (void)
{
  GtkWidget *image = gtk_image_new ();
  char *name;
  char *desc;

  g_object_ref_sink (image);
  gtk_widget_realize_at_context (image);

  gtk_widget_set_tooltip_text (image, "tooltip");

  gtk_accessible_update_property (GTK_ACCESSIBLE (image),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, "label",
                                  -1);

  name = gtk_at_context_get_name (gtk_accessible_get_at_context (GTK_ACCESSIBLE (image)));
  desc = gtk_at_context_get_description (gtk_accessible_get_at_context (GTK_ACCESSIBLE (image)));

  g_assert_cmpstr (name, ==, "label");
  g_assert_cmpstr (desc, ==, "tooltip");

  g_free (name);
  g_free (desc);

  g_object_unref (image);
}

static void
test_name_prohibited (void)
{
  GtkWidget *widget;
  char *name;
  char *desc;

  widget = g_object_new (GTK_TYPE_BUTTON,
                         "accessible-role", GTK_ACCESSIBLE_ROLE_TIME,
                         "label", "too late",
                         NULL);

  g_object_ref_sink (widget);
  gtk_widget_realize_at_context (widget);

  name = gtk_at_context_get_name (gtk_accessible_get_at_context (GTK_ACCESSIBLE (widget)));
  desc = gtk_at_context_get_description (gtk_accessible_get_at_context (GTK_ACCESSIBLE (widget)));

  g_assert_cmpstr (name, ==, "");
  g_assert_cmpstr (desc, ==, "");

  g_free (name);
  g_free (desc);

  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/name/content", test_name_content);
  g_test_add_func ("/a11y/name/tooltip", test_name_tooltip);
  g_test_add_func ("/a11y/name/label", test_name_label);
  g_test_add_func ("/a11y/name/prohibited", test_name_prohibited);

  return g_test_run ();
}
