#include <gtk/gtk.h>

static void
assert_section_is_not_null (GtkCssStyleSheet *stylesheet,
                            GtkCssSection    *section,
                            const GError     *error,
                            gpointer          unused)
{
  g_assert (section != NULL);
}

static void
test_section_in_load_from_data (void)
{
  GtkCssStyleSheet *stylesheet;

  stylesheet = gtk_css_style_sheet_new ();
  g_signal_connect (stylesheet, "parsing-error",
                    G_CALLBACK (assert_section_is_not_null), NULL);
  gtk_css_style_sheet_load_from_data (stylesheet, "random garbage goes here", -1);
  g_object_unref (stylesheet);
}

static void
test_section_load_nonexisting_file (void)
{
  GtkCssStyleSheet *stylesheet;

  stylesheet = gtk_css_style_sheet_new ();
  g_signal_connect (stylesheet, "parsing-error",
                    G_CALLBACK (assert_section_is_not_null), NULL);
  gtk_css_style_sheet_load_from_path (stylesheet, "this/path/does/absolutely/not/exist.css");
  g_object_unref (stylesheet);
}

int
main (int argc, char *argv[])
{
  gtk_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/cssstylesheet/section-in-load-from-data", test_section_in_load_from_data);
  g_test_add_func ("/cssstylesheet/load-nonexisting-file", test_section_load_nonexisting_file);

  return g_test_run ();
}
