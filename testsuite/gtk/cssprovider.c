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

  gtk_css_provider_update_discrete_media_features (provider, 1, (const char *[]) { "prefers-color-scheme" }, (const char *[]) { "light" });
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

  gtk_css_provider_update_discrete_media_features (provider, 2,
    (const char *[]) { "prefers-color-scheme", "prefers-contrast" },
    (const char *[]) { "light", "more" });
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

  gtk_css_provider_update_discrete_media_features (provider, 1, (const char *[]) { "prefers-color-scheme" }, (const char *[]) { "light" });

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


  gtk_css_provider_update_discrete_media_features (provider, 1, (const char *[]) { "prefers-color-scheme" }, (const char *[]) { "light" });

  gtk_css_provider_load_from_string (provider,
    "@media (prefers-color-scheme: light) { one-style { color: blue; } }"
    "@media (prefers-color-scheme: dark) { two-style { color: blue; } }"
  );

  gtk_css_provider_update_discrete_media_features (provider, 1, (const char *[]) { "prefers-color-scheme" }, (const char *[]) { "dark" });

  rendered_css = gtk_css_provider_to_string (provider);
  g_object_unref (provider);

  g_assert_null (strstr (rendered_css, "one-style"));
  g_assert_nonnull (strstr (rendered_css, "two-style"));
  g_free (rendered_css);
}

static void
real_test_gdk_display_media_feature (const char* display_name)
{
  GtkCssProvider *provider = gtk_css_provider_new ();
  char *rendered_css;
  char style_name[32];

  gtk_css_provider_update_discrete_media_features (provider, 1, (const char *[]) { "--gdk-display" }, (const char *[]) { display_name });

  gtk_css_provider_load_from_string (provider,
    "@media (--gdk-display: wayland) { wayland-style { color: blue; } }"
    "@media (--gdk-display: x11) { x11-style { color: blue; } }"
    "@media (--gdk-display: windows) { windows-style { color: blue; } }"
    "@media (--gdk-display: macos) { macos-style { color: blue; } }"
    "@media (--gdk-display: android) { android-style { color: blue; } }"
    "@media (--gdk-display: broadway) { broadway-style { color: blue; } }"
  );
  rendered_css = gtk_css_provider_to_string (provider);
  g_object_unref (provider);

  g_snprintf (style_name, sizeof (style_name), "%s-style", display_name);

  g_assert_nonnull (strstr (rendered_css, style_name));
  g_free (rendered_css);
}

static void
test_gdk_display_media_feature_wayland (void)
{
  real_test_gdk_display_media_feature ("wayland");
}

static void
test_gdk_display_media_feature_x11 (void)
{
  real_test_gdk_display_media_feature ("x11");
}

static void
test_gdk_display_media_feature_windows (void)
{
  real_test_gdk_display_media_feature ("windows");
}

static void
test_gdk_display_media_feature_macos (void)
{
  real_test_gdk_display_media_feature ("macos");
}

static void
test_gdk_display_media_feature_android (void)
{
  real_test_gdk_display_media_feature ("android");
}

static void
test_gdk_display_media_feature_broadway (void)
{
  real_test_gdk_display_media_feature ("broadway");
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
  g_test_add_func ("/cssprovider/--gdk-display-media-feature/wayland", test_gdk_display_media_feature_wayland);
  g_test_add_func ("/cssprovider/--gdk-display-media-feature/x11", test_gdk_display_media_feature_x11);
  g_test_add_func ("/cssprovider/--gdk-display-media-feature/windows", test_gdk_display_media_feature_windows);
  g_test_add_func ("/cssprovider/--gdk-display-media-feature/macos", test_gdk_display_media_feature_macos);
  g_test_add_func ("/cssprovider/--gdk-display-media-feature/android", test_gdk_display_media_feature_android);
  g_test_add_func ("/cssprovider/--gdk-display-media-feature/broadway", test_gdk_display_media_feature_broadway);

  return g_test_run ();
}
