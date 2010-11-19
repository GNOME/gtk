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
    "@define-color bg_color #f9a039;",
    "@define-color color @bg_color;",
    "@define-color color rgb(100, 99, 88);",
    "@define-color color rgba(50%, 50%, 50%, 0.5);",
    "@define-color color lighter(#f9a039);",
    "@define-color color darker ( @blue ) ;",
    "@define-color color shade(@blue, 1.3);",
    "@define-color color alpha(@blue, 1.3);",
    "@define-color color mix(@blue, @red, 0.2);",
    "@define-color color red;",
    "@define-color color mix(shade (#121212, 0.5), mix (rgb(10%,20%,100%), @blue,0.5), 0.2);",
    "@define-color blue @blue;",
    NULL
  };

  const gchar *invalid[] = {
    "@import test.css ;",
    "@import url ( \"test.css\" xyz );",
    "@import url(\");",
    "@import url(');",
    "@import url(\"abc');",
    "@ import ;",
    "@define_color blue  red;",
    "@define-color blue #12234;",
    "@define-color blue #12g234;",
    "@define-color blue @@;",
    "@define-color blue 5!#%4@DG$##x;",
    "@define-color color mix(@red, @blue, @green);",
    "@define-color color mix(@blue, 0.2, @red);",
    "@define-color color mix(0.2, @blue, @red);",
    "@define-color color mix(@blue, @red);",
    "@define-color color mix(@blue);",
    "@define-color color mix();",
    "@define-color color rgba(50%, 50%, 50%);",
    "@define-color color rgb(50%, a);",
    "@three-dee { some other crap };",
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
    "E, F /* comment here */ {}",
    "E,/* comment here */ F {}",
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

static void
test_parse_declarations (void)
{
  GtkCssProvider *provider;
  GError *error;
  gboolean res;
  gint i;
  const gchar *valid[] = {
    "* {}",
    "* { font: Sans 15 }",
    "* { font: Sans 15; }",
    "* { font: bold }",
    "* { color: red }",
    "* { /* just a comment */ }",
    "* { /* multi\nline\ncomment */ }",
    "* { font: /* comment here */ Sans 15 }",
    "* { color: red; background-color: shade (@bg_color, 0.5) }",
    "* { margin: 5 }",
    "* { margin: 5 10 }",
    "* { margin: 5 10 3 }",
    "* { margin: 5 10 3 5 }",
    "* { padding: 5 }",
    "* { padding: 5 10 }",
    "* { border-width: 5; border-radius: 10 }",
    "* { border-color: #ff00ff }",
    "* { engine: clearlooks }",
    "* { background-image: -gtk-gradient (linear,               \n"
    "                                    left top, right top,   \n"
    "                                    from (#fff), to (#000)) }",
    "* { background-image: -gtk-gradient (linear,               \n"
    "                                    0.0 0.5, 0.5 1.0,      \n"
    "                                    from (#fff),           \n"
    "                                    color-stop (0.5, #f00),\n"
    "                                    to (#000))              }",
    "* { background-image: -gtk-gradient (radial,               \n"
    "                                     center center, 0.2,   \n"
    "                                     center center, 0.8,   \n"
    "                                     color-stop (0.0,#fff),\n"
    "                                     color-stop (1.0,#000))}\n",
    "* { border-image: url (\"test.png\") 3 4 3 4 stretch       }",
    "* { border-image: url (\"test.png\") 3 4 3 4 repeat stretch}",
    "* { transition: 150ms ease-in-out                          }",
    "* { transition: 1s linear loop                             }",
    NULL
  };

  const gchar *invalid[] = {
    "* { color }",
    "* { color:green; color }",
    "* { color:red; color; color:green }",
    "* { color:green; color: }",
    "* { color:red; color:; color:green }",
    "* { color:green; color{;color:maroon} }",
    "* { color:red; color{;color:maroon}; color:green }",
    "* { content: 'Hello",
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
  g_test_add_func ("/style/parse/declarations", test_parse_declarations);

  return g_test_run ();
}
