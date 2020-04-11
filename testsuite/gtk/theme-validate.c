#include <gtk/gtk.h>

typedef struct _Theme Theme;

struct _Theme
{
  const char *name;
};

static void
theme_parsing_error (GtkCssProvider *provider,
                     GtkCssSection  *section,
                     const GError   *error,
                     gpointer        unused)
{
  char *s = gtk_css_section_to_string (section);

  g_test_message ("Theme parsing error: %s: %s",
                  s,
                  error->message);

  g_free (s);

  g_test_fail ();
}

static void
test_theme (gconstpointer data)
{
  const Theme *theme = data;
  GtkCssProvider *provider;

  provider = gtk_css_provider_new ();
  g_signal_connect (provider, "parsing-error",
                    G_CALLBACK (theme_parsing_error), NULL);
  gtk_css_provider_load_named (provider, theme->name);
  g_object_unref (provider);
}

int
main (int argc, char *argv[])
{
  const Theme themes[] = {
    { "Adwaita" },
    { "Adwaita-dark" },
    { "HighContrast" },
    { "HighContrastInverse" }
  };
  guint i;

  gtk_init ();
  g_test_init (&argc, &argv, NULL);

  for (i = 0; i < G_N_ELEMENTS (themes); i++)
    {
      char *testname;

      testname = g_strdup_printf ("/theme-validate/%s", themes[i].name);

      g_test_add_data_func (testname, &themes[i], test_theme);

      g_free (testname);
    }

  return g_test_run ();
}
