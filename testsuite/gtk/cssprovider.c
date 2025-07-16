#include <gtk/gtk.h>

static void
assert_section_is_not_null (GtkCssProvider *provider,
                            GtkCssSection  *section,
                            const GError   *error,
                            gpointer        unused)
{
  g_assert_nonnull (section);
}

static void
test_section_in_load_from_data (void)
{
  GtkCssProvider *provider;

  provider = gtk_css_provider_new ();
  g_signal_connect (provider, "parsing-error",
                    G_CALLBACK (assert_section_is_not_null), NULL);
  gtk_css_provider_load_from_string (provider, "random garbage goes here");
  g_object_unref (provider);
}

static void
test_section_load_nonexisting_file (void)
{
  GtkCssProvider *provider;

  provider = gtk_css_provider_new ();
  g_signal_connect (provider, "parsing-error",
                    G_CALLBACK (assert_section_is_not_null), NULL);
  gtk_css_provider_load_from_path (provider, "this/path/does/absolutely/not/exist.css");
  g_object_unref (provider);
}

static void
test_load_with_media_query (void)
{
  GtkCssProvider *provider;
  char *rendered_css;
  provider = gtk_css_provider_new ();

  g_object_set (provider,
                "prefers-color-scheme", GTK_INTERFACE_COLOR_SCHEME_LIGHT,
                NULL);

  gtk_css_provider_load_from_string (provider,
    "@media (prefers-color-scheme: light) { include-me { color: blue; } }"
    "@media (prefers-color-scheme: dark) { skip-me { color: blue; } }");
  rendered_css = gtk_css_provider_to_string (provider);
  g_object_unref (provider);

  g_assert_nonnull (strstr (rendered_css, "include-me"));
  g_assert_null (strstr (rendered_css, "skip-me"));
  g_free (rendered_css);
}

static void
assert_media_query_parse_warning (GtkCssProvider *provider,
                                  GtkCssSection  *section,
                                  const GError   *error,
                                  gpointer        unused)
{
  g_assert_error (error, GTK_CSS_PARSER_WARNING, GTK_CSS_PARSER_WARNING_SYNTAX);
  g_assert_cmpstr (error->message, ==, "Undefined @media feature 'not-a-feature'");
}

static void
test_load_with_undefined_media_query (void)
{
  GtkCssProvider *provider;
  char *rendered_css;
  provider = gtk_css_provider_new ();
  g_signal_connect (provider, "parsing-error",
                    G_CALLBACK (assert_media_query_parse_warning), NULL);

  gtk_css_provider_load_from_string (provider,
    "@media (not-a-feature: other-value) { skip-me { color: blue; } }");
  rendered_css = gtk_css_provider_to_string (provider);
  g_object_unref (provider);

  g_assert_cmpstr (rendered_css, ==, "");
  g_free (rendered_css);
}

static void
test_load_with_and_media_query (void)
{
  GtkCssProvider *provider;
  char *rendered_css;
  provider = gtk_css_provider_new ();

  g_object_set (provider,
                "prefers-color-scheme", GTK_INTERFACE_COLOR_SCHEME_LIGHT,
                "prefers-contrast", GTK_INTERFACE_CONTRAST_MORE,
                NULL);

  gtk_css_provider_load_from_string (provider,
    "@media (prefers-color-scheme: light) and (prefers-contrast: more) { style { color: blue; } }");
  rendered_css = gtk_css_provider_to_string (provider);
  g_object_unref (provider);

  g_assert_nonnull (strstr (rendered_css, "style"));
  g_free (rendered_css);
}

static void
test_load_with_negating_media_query (void)
{
  GtkCssProvider *provider;
  char *rendered_css;
  provider = gtk_css_provider_new ();

  g_object_set (provider,
                "prefers-color-scheme", GTK_INTERFACE_COLOR_SCHEME_LIGHT,
                NULL);

  gtk_css_provider_load_from_string (provider,
    "@media not (prefers-color-scheme: dark) { style { color: blue; } }");
  rendered_css = gtk_css_provider_to_string (provider);
  g_object_unref (provider);

  g_assert_nonnull (strstr (rendered_css, "style"));
  g_free (rendered_css);
}

static void
test_update_media_features_after_style_sheet_is_loaded (void)
{
  GtkCssProvider *provider;
  char *rendered_css;
  provider = gtk_css_provider_new ();


  g_object_set (provider,
                "prefers-color-scheme", GTK_INTERFACE_COLOR_SCHEME_LIGHT,
                NULL);

  gtk_css_provider_load_from_string (provider,
    "@media (prefers-color-scheme: light) { one-style { color: blue; } }"
    "@media (prefers-color-scheme: dark) { two-style { color: blue; } }"
  );

  g_object_set (provider,
                "prefers-color-scheme", GTK_INTERFACE_COLOR_SCHEME_DARK,
                NULL);

  rendered_css = gtk_css_provider_to_string (provider);
  g_object_unref (provider);

  g_assert_null (strstr (rendered_css, "one-style"));
  g_assert_nonnull (strstr (rendered_css, "two-style"));
  g_free (rendered_css);
}

int
main (int argc, char *argv[])
{
  gtk_init ();
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/cssprovider/section-in-load-from-data", test_section_in_load_from_data);
  g_test_add_func ("/cssprovider/load-nonexisting-file", test_section_load_nonexisting_file);
  g_test_add_func ("/cssprovider/load-with-media-query", test_load_with_media_query);
  g_test_add_func ("/cssprovider/load-with-undefined-media-query", test_load_with_undefined_media_query);
  g_test_add_func ("/cssprovider/load-with-negating-media-query", test_load_with_negating_media_query);
  g_test_add_func ("/cssprovider/load-with-and-media-query", test_load_with_and_media_query);
  g_test_add_func ("/cssprovider/update-media-features-after-style-sheet-is-loaded", test_update_media_features_after_style_sheet_is_loaded);

  return g_test_run ();
}
