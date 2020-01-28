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

int
main (int argc, char *argv[])
{
  gtk_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/style/parse/selectors", test_parse_selectors);
  g_test_add_func ("/style/widget-path-parent", test_widget_path_parent);
  g_test_add_func ("/style/classes", test_style_classes);

  return g_test_run ();
}
