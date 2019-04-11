#include <gtk/gtk.h>

typedef struct {
  GtkStyleContext *context;
  GtkCssProvider  *blue_provider;
  GtkCssProvider  *red_provider;
  GtkCssProvider  *green_provider;
} PrioritiesFixture;

static void
test_parse_selectors (void)
{
  GtkCssProvider *provider;
  gint i;
  const gchar *valid[] = {
    "* {}",
    "E {}",
    "E F {}",
    "E > F {}",
    "E + F {}",
    "E#id {}",
    "#id {}",
    "tab:first-child {}",
    "tab:last-child {}",
    "tab:first-child {}",
    "tab:last-child {}",
    "tab:nth-child(even) {}",
    "tab:nth-child(odd) {}",
    ".some-class {}",
    ".some-class.another-class {}",
    ".some-class .another-class {}",
    "E * {}",
    "E .class {}",
    "E > .foo {}",
    "E > #id {}",
    "E:active {}",
    "E:hover {}",
    "E:selected {}",
    "E:disabled {}",
    "E:indeterminate {}",
    "E:focus {}",
    "E:active:hover {}",
    "* > .notebook tab:first-child .label:focus {}",
    "E, F {}",
    "E, F /* comment here */ {}",
    "E,/* comment here */ F {}",
    "E1.e1_2 #T3_4 {}",
    "E:first-child {}",
    "E:last-child {}",
    "E:first-child {}",
    "E:last-child {}",
    "E:nth-child(even) {}",
    "E:nth-child(odd) {}",
    "E:focus tab {}",
     NULL
  };

  for (i = 0; valid[i]; i++)
    {
      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_data (provider, valid[i], -1);

      g_object_unref (provider);
   }
}

static void
test_path (void)
{
  GtkWidgetPath *path;
  GtkWidgetPath *path2;
  gint pos;

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


  gtk_widget_path_free (path);
}

static void
test_match (void)
{
  GtkStyleContext *context;
  GtkWidgetPath *path;
  GtkCssProvider *provider;
  const gchar *data;
  GdkRGBA color;
  GdkRGBA expected;

  provider = gtk_css_provider_new ();

  gdk_rgba_parse (&expected, "#fff");

  context = gtk_style_context_new ();

  path = gtk_widget_path_new ();
  gtk_widget_path_append_type (path, GTK_TYPE_WINDOW);
  gtk_widget_path_append_type (path, GTK_TYPE_BOX);
  gtk_widget_path_append_type (path, GTK_TYPE_BUTTON);
  gtk_widget_path_iter_set_object_name (path, 0, "window");
  gtk_widget_path_iter_set_name (path, 0, "mywindow");
  gtk_widget_path_iter_set_object_name (path, 2, "button");
  gtk_widget_path_iter_add_class (path, 2, "button");
  gtk_widget_path_iter_set_state (path, 0, GTK_STATE_FLAG_ACTIVE);
  gtk_style_context_set_path (context, path);
  gtk_widget_path_free (path);

  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_USER);

  data = "* { color: #fff; }";
  gtk_css_provider_load_from_data (provider, data, -1);
  gtk_style_context_get_color (context, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00; }\n"
         "button { color: #fff; }";
  gtk_css_provider_load_from_data (provider, data, -1);
  gtk_style_context_get_color (context, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00; }\n"
         "button { color: #fff; }\n"
         "window > button { color: #000; }";
  gtk_css_provider_load_from_data (provider, data, -1);
  gtk_style_context_get_color (context, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00; }\n"
         ".button { color: #fff; }";
  gtk_css_provider_load_from_data (provider, data, -1);
  gtk_style_context_get_color (context, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00; }\n"
         "button { color: #000; }\n"
         ".button { color: #fff; }";
  gtk_css_provider_load_from_data (provider, data, -1);
  gtk_style_context_get_color (context, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00; }\n"
         "button { color: #000; }\n"
         "window button { color: #fff; }";
  gtk_css_provider_load_from_data (provider, data, -1);
  gtk_style_context_get_color (context, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00; }\n"
         ".button { color: #000; }\n"
         "window .button { color: #fff; }";
  gtk_css_provider_load_from_data (provider, data, -1);
  gtk_style_context_get_color (context, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00; }\n"
         "* .button { color: #000; }\n"
         "#mywindow .button { color: #fff; }";
  gtk_css_provider_load_from_data (provider, data, -1);
  gtk_style_context_get_color (context, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00; }\n"
         "window .button { color: #000; }\n"
         "window#mywindow .button { color: #fff; }";
  gtk_css_provider_load_from_data (provider, data, -1);
  gtk_style_context_get_color (context, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00; }\n"
         "window .button { color: #000; }\n"
         "window button.button { color: #fff; }";
  gtk_css_provider_load_from_data (provider, data, -1);
  gtk_style_context_get_color (context, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

  data = "* { color: #f00; }\n"
         "window:backdrop .button { color: #000; }\n"
         "window .button { color: #111; }\n"
         "window:active .button { color: #fff; }";
  gtk_css_provider_load_from_data (provider, data, -1);
  gtk_style_context_get_color (context, &color);
  g_assert (gdk_rgba_equal (&color, &expected));

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

  gtk_style_context_get (context,
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

void
test_widget_path_parent (void)
{
  GtkStyleContext *parent, *context;

  parent = gtk_style_context_new ();
  context = gtk_style_context_new ();

  gtk_style_context_set_parent (context, parent);

  g_object_unref (parent);
  g_object_unref (context);
}

static void
test_style_classes (void)
{
  GtkStyleContext *context;
  GList *classes;

  context = gtk_style_context_new ();

  classes = gtk_style_context_list_classes (context);
  g_assert_null (classes);

  gtk_style_context_add_class (context, "A");

  classes = gtk_style_context_list_classes (context);
  g_assert (classes);
  g_assert_null (classes->next);
  g_assert_cmpstr (classes->data, ==, "A");
  g_list_free (classes);

  gtk_style_context_add_class (context, "B");

  classes = gtk_style_context_list_classes (context);
  g_assert (classes);
  g_assert_cmpstr (classes->data, ==, "A");
  g_assert (classes->next);
  g_assert_cmpstr (classes->next->data, ==, "B");
  g_assert_null (classes->next->next);
  g_list_free (classes);

  gtk_style_context_remove_class (context, "A");

  classes = gtk_style_context_list_classes (context);
  g_assert (classes);
  g_assert_null (classes->next);
  g_assert_cmpstr (classes->data, ==, "B");
  g_list_free (classes);

  g_object_unref (context);
}

static void
test_style_priorities_setup (PrioritiesFixture *f,
                             gconstpointer      unused)
{
  f->blue_provider = gtk_css_provider_new ();
  f->red_provider = gtk_css_provider_new ();
  f->green_provider = gtk_css_provider_new ();
  f->context = gtk_style_context_new ();
  GtkWidgetPath *path = gtk_widget_path_new ();

  gtk_css_provider_load_from_data (f->blue_provider, "* { color: blue; }", -1);
  gtk_css_provider_load_from_data (f->red_provider, "* { color: red; }", -1);
  gtk_css_provider_load_from_data (f->green_provider, "* { color: green; }", -1);

  gtk_widget_path_append_type (path, GTK_TYPE_WINDOW);
  gtk_style_context_set_path (f->context, path);

  gtk_widget_path_free (path);
}

static void
test_style_priorities_teardown (PrioritiesFixture *f,
                                gconstpointer      unused)
{
  g_object_unref (f->blue_provider);
  g_object_unref (f->red_provider);
  g_object_unref (f->green_provider);
  g_object_unref (f->context);
}

static void
test_style_priorities_equal (PrioritiesFixture *f,
                             gconstpointer      unused)
{
  GdkRGBA color, ref_color;

  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (f->blue_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
  gtk_style_context_add_provider (f->context, GTK_STYLE_PROVIDER (f->red_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_USER);

  /* When style providers are added to the display as well as the style context
  the one specific to the style context should take priority */
  gdk_rgba_parse (&ref_color, "red");
  gtk_style_context_get_color (f->context, &color);

  g_assert_true (gdk_rgba_equal (&ref_color, &color));
}

static void
test_style_priorities_display_only (PrioritiesFixture *f,
                                    gconstpointer      unused)
{
  GdkRGBA color, ref_color;

  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (f->blue_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);

  gdk_rgba_parse (&ref_color, "blue");
  gtk_style_context_get_color (f->context, &color);

  g_assert_true (gdk_rgba_equal (&ref_color, &color));
}

static void
test_style_priorities_context_only (PrioritiesFixture *f,
                                    gconstpointer      unused)
{
  GdkRGBA color, ref_color;

  gtk_style_context_add_provider (f->context, GTK_STYLE_PROVIDER (f->red_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_USER);

  gdk_rgba_parse (&ref_color, "red");
  gtk_style_context_get_color (f->context, &color);

  g_assert_true (gdk_rgba_equal (&ref_color, &color));
}

static void
test_style_priorities_display_higher (PrioritiesFixture *f,
                                      gconstpointer      unused)
{
  GdkRGBA color, ref_color;

  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (f->blue_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER + 1);
  gtk_style_context_add_provider (f->context, GTK_STYLE_PROVIDER (f->red_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_USER);

  gdk_rgba_parse (&ref_color, "blue");
  gtk_style_context_get_color (f->context, &color);

  g_assert_true (gdk_rgba_equal (&ref_color, &color));
}

static void
test_style_priorities_context_higher (PrioritiesFixture *f,
                                      gconstpointer      unused)
{
  GdkRGBA color, ref_color;

  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (f->blue_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
  gtk_style_context_add_provider (f->context, GTK_STYLE_PROVIDER (f->red_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_USER + 1);

  gdk_rgba_parse (&ref_color, "red");
  gtk_style_context_get_color (f->context, &color);

  g_assert_true (gdk_rgba_equal (&ref_color, &color));
}

static void
test_style_priorities_two_display (PrioritiesFixture *f,
                                   gconstpointer      unused)
{
  GdkRGBA color, ref_color;

  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (f->blue_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (f->red_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER + 1);

  gdk_rgba_parse (&ref_color, "red");
  gtk_style_context_get_color (f->context, &color);

  g_assert_true (gdk_rgba_equal (&ref_color, &color));
}

static void
test_style_priorities_two_context (PrioritiesFixture *f,
                                   gconstpointer      unused)
{
  GdkRGBA color, ref_color;

  gtk_style_context_add_provider (f->context, GTK_STYLE_PROVIDER (f->blue_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_USER);
  gtk_style_context_add_provider (f->context, GTK_STYLE_PROVIDER (f->red_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_USER + 1);

  gdk_rgba_parse (&ref_color, "red");
  gtk_style_context_get_color (f->context, &color);

  g_assert_true (gdk_rgba_equal (&ref_color, &color));
}

static void
test_style_priorities_three_display_higher (PrioritiesFixture *f,
                                            gconstpointer      unused)
{
  GdkRGBA color, ref_color;

  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (f->blue_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (f->green_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER + 1);
  gtk_style_context_add_provider (f->context, GTK_STYLE_PROVIDER (f->red_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_USER);

  gdk_rgba_parse (&ref_color, "green");
  gtk_style_context_get_color (f->context, &color);

  g_assert_true (gdk_rgba_equal (&ref_color, &color));
}

static void
test_style_priorities_three_context_higher (PrioritiesFixture *f,
                                            gconstpointer      unused)
{
  GdkRGBA color, ref_color;

  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (f->blue_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
  gtk_style_context_add_provider (f->context, GTK_STYLE_PROVIDER (f->red_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_USER);
  gtk_style_context_add_provider (f->context, GTK_STYLE_PROVIDER (f->green_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_USER + 1);

  gdk_rgba_parse (&ref_color, "green");
  gtk_style_context_get_color (f->context, &color);

  g_assert_true (gdk_rgba_equal (&ref_color, &color));
}

int
main (int argc, char *argv[])
{
  gtk_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/style/parse/selectors", test_parse_selectors);
  g_test_add_func ("/style/path", test_path);
  g_test_add_func ("/style/match", test_match);
  g_test_add_func ("/style/basic", test_basic_properties);
  g_test_add_func ("/style/widget-path-parent", test_widget_path_parent);
  g_test_add_func ("/style/classes", test_style_classes);

#define ADD_PRIORITIES_TEST(path, func) \
  g_test_add ("/style/priorities/" path, PrioritiesFixture, NULL, test_style_priorities_setup, \
              (func), test_style_priorities_teardown)

  ADD_PRIORITIES_TEST ("equal", test_style_priorities_equal);
  ADD_PRIORITIES_TEST ("display-only", test_style_priorities_display_only);
  ADD_PRIORITIES_TEST ("context-only", test_style_priorities_context_only);
  ADD_PRIORITIES_TEST ("display-higher", test_style_priorities_display_higher);
  ADD_PRIORITIES_TEST ("context-higher", test_style_priorities_context_higher);
  ADD_PRIORITIES_TEST ("two-display", test_style_priorities_two_display);
  ADD_PRIORITIES_TEST ("two-context", test_style_priorities_two_context);
  ADD_PRIORITIES_TEST ("three-display-higher", test_style_priorities_three_display_higher);
  ADD_PRIORITIES_TEST ("three-context-higher", test_style_priorities_three_context_higher);

#undef ADD_PRIORITIES_TEST
  return g_test_run ();
}
