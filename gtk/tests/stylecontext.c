#include <gtk/gtk.h>

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
    "E, F /* comment here */ {}",
    "E,/* comment here */ F {}",
    "E1.e1_2 #T3_4 {}",
    "E:first-child {}",
    "E:last-child {}",
    "E:nth-child(first) {}",
    "E:nth-child(last) {}",
    "E:nth-child(even) {}",
    "E:nth-child(odd) {}",
    "E:sorted {}",
    "E:focused tab {}",
     NULL
  };

  error = NULL;
  for (i = 0; valid[i]; i++)
    {
      provider = gtk_css_provider_new ();
      res = gtk_css_provider_load_from_data (provider, valid[i], -1, &error);
      if (error)
        g_print ("parsing '%s': got unexpected error: %s\n", valid[i], error->message);
      g_assert_no_error (error);
      g_assert (res);

      g_object_unref (provider);
   }
}

static void
test_path (void)
{
  GtkWidgetPath *path;
  GtkWidgetPath *path2;
  gint pos;
  GtkRegionFlags flags;

  path = gtk_widget_path_new ();
  g_assert_cmpint (gtk_widget_path_length (path), ==, 0);

  pos = gtk_widget_path_append_type (path, GTK_TYPE_WINDOW);
  g_assert_cmpint (pos, ==, 0);
  g_assert_cmpint (gtk_widget_path_length (path), ==, 1);
  g_assert (gtk_widget_path_iter_get_object_type (path, 0) == GTK_TYPE_WINDOW);
  g_assert (gtk_widget_path_is_type (path, GTK_TYPE_WIDGET));
  g_assert (gtk_widget_path_iter_get_name (path, 0) == NULL);

  pos = gtk_widget_path_append_type (path, GTK_TYPE_WIDGET);
  g_assert_cmpint (pos, ==, 1);
  g_assert_cmpint (gtk_widget_path_length (path), ==, 2);
  gtk_widget_path_iter_set_object_type (path, pos, GTK_TYPE_BUTTON);
  g_assert (gtk_widget_path_is_type (path, GTK_TYPE_BUTTON));
  g_assert (gtk_widget_path_has_parent (path, GTK_TYPE_WIDGET));
  g_assert (gtk_widget_path_has_parent (path, GTK_TYPE_WINDOW));
  g_assert (!gtk_widget_path_has_parent (path, GTK_TYPE_DIALOG));
  g_assert (gtk_widget_path_iter_get_name (path, 1) == NULL);

  gtk_widget_path_iter_set_name (path, 1, "name");
  g_assert (gtk_widget_path_iter_has_name (path, 1, "name"));

  gtk_widget_path_iter_add_class (path, 1, "class1");
  gtk_widget_path_iter_add_class (path, 1, "class2");
  g_assert (gtk_widget_path_iter_has_class (path, 1, "class1"));
  g_assert (gtk_widget_path_iter_has_class (path, 1, "class2"));
  g_assert (!gtk_widget_path_iter_has_class (path, 1, "class3"));

  path2 = gtk_widget_path_copy (path);
  g_assert (gtk_widget_path_iter_has_class (path2, 1, "class1"));
  g_assert (gtk_widget_path_iter_has_class (path2, 1, "class2"));
  g_assert (!gtk_widget_path_iter_has_class (path2, 1, "class3"));
  gtk_widget_path_free (path2);

  gtk_widget_path_iter_remove_class (path, 1, "class2");
  g_assert (gtk_widget_path_iter_has_class (path, 1, "class1"));
  g_assert (!gtk_widget_path_iter_has_class (path, 1, "class2"));
  gtk_widget_path_iter_clear_classes (path, 1);
  g_assert (!gtk_widget_path_iter_has_class (path, 1, "class1"));

  gtk_widget_path_iter_add_region (path, 1, "tab", 0);
  gtk_widget_path_iter_add_region (path, 1, "title", GTK_REGION_EVEN | GTK_REGION_FIRST);

  g_assert (gtk_widget_path_iter_has_region (path, 1, "tab", &flags) &&
            flags == 0);
  g_assert (gtk_widget_path_iter_has_region (path, 1, "title", &flags) &&
            flags == (GTK_REGION_EVEN | GTK_REGION_FIRST));
  g_assert (!gtk_widget_path_iter_has_region (path, 1, "extension", NULL));

  path2 = gtk_widget_path_copy (path);
  g_assert (gtk_widget_path_iter_has_region (path2, 1, "tab", &flags) &&
            flags == 0);
  g_assert (gtk_widget_path_iter_has_region (path2, 1, "title", &flags) &&
            flags == (GTK_REGION_EVEN | GTK_REGION_FIRST));
  g_assert (!gtk_widget_path_iter_has_region (path2, 1, "extension", NULL));
  gtk_widget_path_free (path2);

  gtk_widget_path_free (path);
}

static void
test_match (void)
{
  GtkStyleContext *context;
  GtkWidgetPath *path;
  GtkCssProvider *provider;
  GError *error;
  const gchar *data;
  GdkRGBA color;
  GdkRGBA expected;

  error = NULL;
  provider = gtk_css_provider_new ();

  gdk_rgba_parse (&expected, "#fff");

  context = gtk_style_context_new ();

  path = gtk_widget_path_new ();
  gtk_widget_path_append_type (path, GTK_TYPE_WINDOW);
  gtk_widget_path_append_type (path, GTK_TYPE_BOX);
  gtk_widget_path_append_type (path, GTK_TYPE_BUTTON);
  gtk_widget_path_iter_set_name (path, 0, "mywindow");
  gtk_widget_path_iter_add_class (path, 2, "button");
  gtk_style_context_set_path (context, path);
  gtk_widget_path_free (path);

  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_USER);

  data = "* { color: #fff }";
  gtk_css_provider_load_from_data (provider, data, -1, &error);
  g_assert_no_error (error);
  gtk_style_context_invalidate (context);
  gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00 }\n"
         "GtkButton { color: #fff }";
  gtk_css_provider_load_from_data (provider, data, -1, &error);
  g_assert_no_error (error);
  gtk_style_context_invalidate (context);
  gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00 }\n"
         "GtkButton { color: #fff }\n"
         "GtkWindow > GtkButton { color: #000 }";
  gtk_css_provider_load_from_data (provider, data, -1, &error);
  g_assert_no_error (error);
  gtk_style_context_invalidate (context);
  gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00 }\n"
         ".button { color: #fff }";
  gtk_css_provider_load_from_data (provider, data, -1, &error);
  g_assert_no_error (error);
  gtk_style_context_invalidate (context);
  gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00 }\n"
         "GtkButton { color: #000 }\n"
         ".button { color: #fff }";
  gtk_css_provider_load_from_data (provider, data, -1, &error);
  g_assert_no_error (error);
  gtk_style_context_invalidate (context);
  gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00 }\n"
         "GtkButton { color: #000 }\n"
         "GtkWindow GtkButton { color: #fff }";
  gtk_css_provider_load_from_data (provider, data, -1, &error);
  g_assert_no_error (error);
  gtk_style_context_invalidate (context);
  gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00 }\n"
         ".button { color: #000 }\n"
         "GtkWindow .button { color: #fff }";
  gtk_css_provider_load_from_data (provider, data, -1, &error);
  g_assert_no_error (error);
  gtk_style_context_invalidate (context);
  gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00 }\n"
         "* .button { color: #000 }\n"
         "#mywindow .button { color: #fff }";
  gtk_css_provider_load_from_data (provider, data, -1, &error);
  g_assert_no_error (error);
  gtk_style_context_invalidate (context);
  gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00 }\n"
         "GtkWindow .button { color: #000 }\n"
         "GtkWindow#mywindow .button { color: #fff }";
  gtk_css_provider_load_from_data (provider, data, -1, &error);
  g_assert_no_error (error);
  gtk_style_context_invalidate (context);
  gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00 }\n"
         "GtkWindow .button { color: #000 }\n"
         "GObject .button { color: #fff }";
  gtk_css_provider_load_from_data (provider, data, -1, &error);
  g_assert_no_error (error);
  gtk_style_context_invalidate (context);
  gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  g_object_unref (provider);
  g_object_unref (context);
}

static void
test_style_property (void)
{
  GtkStyleContext *context;
  GtkWidgetPath *path;
  GtkCssProvider *provider;
  GError *error;
  const gchar *data;
  gint x;
  GdkRGBA color;
  GdkRGBA expected;

  error = NULL;
  provider = gtk_css_provider_new ();

  context = gtk_style_context_new ();

  path = gtk_widget_path_new ();
  gtk_widget_path_append_type (path, GTK_TYPE_WINDOW);
  gtk_widget_path_append_type (path, GTK_TYPE_BOX);
  gtk_widget_path_append_type (path, GTK_TYPE_BUTTON);
  gtk_style_context_set_path (context, path);
  gtk_widget_path_free (path);
  gtk_style_context_set_state (context, GTK_STATE_FLAG_PRELIGHT);

  /* Since we set the prelight state on the context, we expect
   * only the third selector to match, even though the second one
   * has higher specificity, and the fourth one comes later.
   *
   * In particular, we want to verify that widget style properties and
   * CSS properties follow the same matching rules, ie we expect
   * color to be #003 and child-displacement-x to be 3.
   */
  data = "GtkButton:insensitive { color: #001; -GtkButton-child-displacement-x: 1 }\n"
         "GtkBox GtkButton:selected { color: #002; -GtkButton-child-displacement-x: 2 }\n"
         "GtkButton:prelight { color: #003; -GtkButton-child-displacement-x: 3 }\n"
         "GtkButton:focused { color: #004; -GtkButton-child-displacement-x: 4 }\n";
  gtk_css_provider_load_from_data (provider, data, -1, &error);
  g_assert_no_error (error);
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_USER);

  gtk_style_context_invalidate (context);

  gtk_style_context_get_color (context, GTK_STATE_FLAG_PRELIGHT, &color);
  gdk_rgba_parse (&expected, "#003");
  g_assert (gdk_rgba_equal (&color, &expected));

  gtk_style_context_get_style (context, "child-displacement-x", &x, NULL);

  g_assert_cmpint (x, ==, 3);

  g_object_unref (provider);
  g_object_unref (context);
}

static void
test_basic_properties (void)
{
  GtkStyleContext *context;
  GtkWidgetPath *path;
  GdkRGBA *color;
  GdkRGBA *bg_color;
  PangoFontDescription *font;

  context = gtk_style_context_new ();
  path = gtk_widget_path_new ();
  gtk_style_context_set_path (context, path);
  gtk_widget_path_free (path);

  gtk_style_context_get (context, 0,
                         "color", &color,
                         "background-color", &bg_color,
                         "font", &font,
                         NULL);
  g_assert (color != NULL);
  g_assert (bg_color != NULL);
  g_assert (font != NULL);

  gdk_rgba_free (color);
  gdk_rgba_free (bg_color);
  pango_font_description_free (font);

  g_object_unref (context);
}

int
main (int argc, char *argv[])
{
  gtk_init (NULL, NULL);
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/style/parse/selectors", test_parse_selectors);
  g_test_add_func ("/style/path", test_path);
  g_test_add_func ("/style/match", test_match);
  g_test_add_func ("/style/style-property", test_style_property);
  g_test_add_func ("/style/basic", test_basic_properties);

  return g_test_run ();
}
