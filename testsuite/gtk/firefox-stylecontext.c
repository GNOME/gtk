#include <gtk/gtk.h>

static void
test_init_of_theme (void)
{
  GtkStyleContext *context;
  GtkCssProvider *provider;
  GtkWidgetPath *path;
  GdkRGBA before, after;
  char *css;

  /* Test that a style context actually uses the theme loaded for the 
   * screen it is using. If no screen is set, it's the default one.
   */
  context = gtk_style_context_new ();
  path = gtk_widget_path_new ();

  /* Set a path that will have a color set.
   * (This could actually fail if style classes change, so if this test
   *  fails, make sure to have this path represent something sane.)
   */
  gtk_widget_path_append_type (path, GTK_TYPE_WINDOW);
  gtk_widget_path_iter_add_class (path, -1, GTK_STYLE_CLASS_BACKGROUND);
  gtk_style_context_set_path (context, path);
  gtk_widget_path_free (path);

  /* Get the color. This should be initialized by the theme and not be
   * the default. */
  gtk_style_context_get_color (context, 0, &before);

  /* Add a style that sets a different color for this widget.
   * This style has a higher priority than fallback, but a lower 
   * priority than the theme. */
  css = g_strdup_printf (".background { color: %s; }",
                         before.alpha < 0.5 ? "black" : "transparent");
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1, NULL);
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_FALLBACK + 1);
  g_object_unref (provider);

  /* Get the color again. */
  gtk_style_context_get_color (context, 0, &after);

  /* Because the style we added does not influence the color,
   * the before and after colors should be identical. */
  g_assert (gdk_rgba_equal (&before, &after));

  g_object_unref (context);
}

int
main (int argc, char *argv[])
{
  /* If gdk_init() is called before gtk_init() the GTK code takes
   * a different path (why?)
   */
  gdk_init (NULL, NULL);
  gtk_init (NULL, NULL);
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/style/init_of_theme", test_init_of_theme);

  return g_test_run ();
}
