#include <gtk/gtk.h>

static void
test_parse_empty (void)
{
  GtkCssProvider *provider;
  GError *error;
  gboolean res;

  provider = gtk_css_provider_new ();
  error = NULL;
  res = gtk_css_provider_load_from_data (provider, "", -1, &error);

  g_assert (res);
  g_assert_no_error (error);
  g_clear_error (&error);

  g_object_unref (provider);
}

static void
test_parse_at (void)
{
  GtkCssProvider *provider;
  GError *error;
  gboolean res;
  gint i;
  const gchar *valid[] = {
    "@import \"test.css\";",
    "@import 'test.css';",
    "@import url(\"test.css\");",
    "@import url('test.css');",
    "@import\nurl (\t\"test.css\" ) ;",
    NULL
  };

  const gchar *invalid[] = {
    "@import test.css ;",
    "@import url ( \"test.css\" xyz );",
    "@import url(\");",
    "@import url(');",
    "@import url(\"abc');",
    "@ import ;",
    NULL
  };

  error = NULL;
  for (i = 0; valid[i]; i++)
    {
      provider = gtk_css_provider_new ();
      res = gtk_css_provider_load_from_data (provider, valid[i], -1, &error);
      g_assert_no_error (error);
      g_assert (res);

      g_object_unref (provider);
   }

  for (i = 0; invalid[i]; i++)
    {
      provider = gtk_css_provider_new ();
      res = gtk_css_provider_load_from_data (provider, invalid[i], -1, &error);
      g_assert_error (error, GTK_CSS_PROVIDER_ERROR, GTK_CSS_PROVIDER_ERROR_FAILED);
      g_assert (!res);
      g_object_unref (provider);
      g_clear_error (&error);
   }
}

static void
test_parse_selectors (void)
{
  GtkCssProvider *provider;
  GError *error;
  gboolean res;
  gint i;
  const gchar *valid[] = {
    "* {}",
    "E {}",
    "E F {}",
    "E > F {}",
    "E#id {}",
    "#id {}",
    "tab:first-child {}",
    "tab:last-child {}",
    "tab:nth-child(first) {}",
    "tab:nth-child(last) {}",
    "tab:nth-child(even) {}",
    "tab:nth-child(odd) {}",
    "tab:sorted {}",
    ".some-class {}",
    ".some-class.another-class {}",
    ".some-class .another-class {}",
    "E * {}",
    "E .class {}",
    "E > .foo {}",
    "E > #id {}",
    "E:active {}",
    "E:prelight {}",
    "E:hover {}",
    "E:selected {}",
    "E:insensitive {}",
    "E:inconsistent {}",
    "E:focused {}",
    "E:active:prelight {}",
    "* > .notebook tab:first-child .label:focused {}",
    "E, F {}",
    NULL
  };

  const gchar *invalid[] = {
    /* nth-child and similar pseudo classes can only
     * be used with regions, not with types
     */
    "E:first-child {}",
    "E:last-child {}",
    "E:nth-child(first) {}",
    "E:nth-child(last) {}",
    "E:nth-child(even) {}",
    "E:nth-child(odd) {}",
    "E:sorted {}",
    /* widget state pseudo-classes can only be used for
     * the last element
     */
    "E:focused tab {}",
     NULL
  };

  error = NULL;
  for (i = 0; valid[i]; i++)
    {
      provider = gtk_css_provider_new ();
      res = gtk_css_provider_load_from_data (provider, valid[i], -1, &error);
      g_assert_no_error (error);
      g_assert (res);

      g_object_unref (provider);
   }

  for (i = 0; invalid[i]; i++)
    {
      provider = gtk_css_provider_new ();
      res = gtk_css_provider_load_from_data (provider, invalid[i], -1, &error);
      g_assert_error (error, GTK_CSS_PROVIDER_ERROR, GTK_CSS_PROVIDER_ERROR_FAILED);
      g_assert (!res);
      g_object_unref (provider);
      g_clear_error (&error);
   }
}

int
main (int argc, char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/style/parse/empty", test_parse_empty);
  g_test_add_func ("/style/parse/at", test_parse_at);
  g_test_add_func ("/style/parse/selectors", test_parse_selectors);

  return g_test_run ();
}
