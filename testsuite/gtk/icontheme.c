#include <gtk/gtk.h>

#include <string.h>

#define SCALABLE_IMAGE_SIZE (128)

static GtkIconTheme *
get_test_icontheme (gboolean force_reload)
{
  static GtkIconTheme *icon_theme = NULL;
  const char *current_dir;

  if (force_reload)
    g_clear_object (&icon_theme);

  if (icon_theme)
    return icon_theme;

  icon_theme = gtk_icon_theme_new ();
  gtk_icon_theme_set_custom_theme (icon_theme, "icons");
  current_dir = g_test_get_dir (G_TEST_DIST);
  gtk_icon_theme_set_search_path (icon_theme, &current_dir, 1);

  return icon_theme;
}

static char *
lookup_flags_to_string (GtkIconLookupFlags flags)
{
  GValue flags_value = { 0, }, string_value = { 0, };
  char *result;

  g_value_init (&flags_value, GTK_TYPE_ICON_LOOKUP_FLAGS);
  g_value_init (&string_value, G_TYPE_STRING);

  g_value_set_flags (&flags_value, flags);
  if (!g_value_transform (&flags_value, &string_value))
    {
      g_assert_not_reached ();
    }

  result = g_value_dup_string (&string_value);

  g_value_unset (&flags_value);
  g_value_unset (&string_value);

  return result;
}

static void
assert_icon_lookup_size (const char         *icon_name,
                         gint                size,
                         GtkIconLookupFlags  flags,
                         const char         *filename,
                         gint                pixbuf_size)
{
  GtkIconInfo *info;

  info = gtk_icon_theme_lookup_icon (get_test_icontheme (FALSE), icon_name, size, flags);
  if (info == NULL)
    {
      g_error ("Could not look up an icon for \"%s\" with flags %s at size %d",
               icon_name, lookup_flags_to_string (flags), size);
      return;
    }

  if (filename)
    {
      if (!g_str_has_suffix (gtk_icon_info_get_filename (info), filename))
        {
          g_error ("Icon for \"%s\" with flags %s at size %d should be \"...%s\" but is \"...%s\"",
                   icon_name, lookup_flags_to_string (flags), size,
                   filename, gtk_icon_info_get_filename (info) + strlen (g_get_current_dir ()));
          return;
        }
    }
  else
    {
      g_assert (gtk_icon_info_get_filename (info) == NULL);
    }

  if (pixbuf_size > 0)
    {
      GdkPixbuf *pixbuf;
      GError *error = NULL;

      pixbuf = gtk_icon_info_load_icon (info, &error);
      g_assert_no_error (error);
      g_assert_cmpint (gdk_pixbuf_get_width (pixbuf), ==, pixbuf_size);
      g_object_unref (pixbuf);
    }

  g_object_unref (info);
}

static void
assert_icon_lookup (const char         *icon_name,
                    gint                size,
                    GtkIconLookupFlags  flags,
                    const char         *filename)
{
  assert_icon_lookup_size (icon_name, size, flags, filename, -1);
}

static void
assert_icon_lookup_fails (const char         *icon_name,
                          gint                size,
                          GtkIconLookupFlags  flags)
{
  GtkIconInfo *info;

  info = gtk_icon_theme_lookup_icon (get_test_icontheme (FALSE), icon_name, size, flags);

  if (info != NULL)
    {
      g_error ("Should not find an icon for \"%s\" with flags %s at size %d, but found \"%s\"",
               icon_name, lookup_flags_to_string (flags), size, gtk_icon_info_get_filename (info) + strlen (g_get_current_dir ()));
      g_object_unref (info);
      return;
    }
}

static GList *lookups = NULL;

static void
print_func (const gchar *string)
{
  if (g_str_has_prefix (string, "\tlookup name: "))
    {
      gchar *s;
      s = g_strchomp (g_strdup (string + strlen ("\tlookup name: ")));
      lookups = g_list_append (lookups, s);
    }
}

static void
assert_lookup_order (const char         *icon_name,
                     gint                size,
                     GtkIconLookupFlags  flags,
                     const char         *first,
                     ...)
{
  guint debug_flags;
  GPrintFunc old_print_func;
  va_list args;
  const gchar *s;
  GtkIconInfo *info;
  GList *l;

  debug_flags = gtk_get_debug_flags ();
  gtk_set_debug_flags (debug_flags | GTK_DEBUG_ICONTHEME);
  old_print_func = g_set_print_handler (print_func);

  g_assert (lookups == NULL);
  
  info = gtk_icon_theme_lookup_icon (get_test_icontheme (FALSE), icon_name, size, flags);
  if (info)
    g_object_unref (info);
  
  va_start (args, first);
  s = first;
  l = lookups;
  while (s != NULL)
    {
      g_assert (l != NULL);
      g_assert_cmpstr (s, ==, l->data);
      s = va_arg (args, gchar*);
      l = l->next;
    }
  g_assert (l == NULL);
  va_end (args);

  g_list_free_full (lookups, g_free);
  lookups = NULL;

  g_set_print_handler (old_print_func);
  gtk_set_debug_flags (debug_flags);
}

static void
test_basics (void)
{
  /* just a basic boring lookup so we know everything works */
  assert_icon_lookup ("simple", 16, 0, "/icons/16x16/simple.png");

  /* The first time an icon is looked up that doesn't exist, GTK spews a 
   * warning.
   * We make that happen right here, so we can get rid of the warning 
   * and do failing lookups in other tests.
   */
  g_test_expect_message ("Gtk", G_LOG_LEVEL_WARNING, "Could not find the icon*");
  assert_icon_lookup_fails ("this-icon-totally-does-not-exist", 16, 0);
  g_test_assert_expected_messages ();
}

static void
test_lookup_order (void)
{
  assert_lookup_order ("foo-bar-baz", 16, GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                       "foo-bar-baz",
                       "foo-bar",
                       "foo",
                       NULL);
  assert_lookup_order ("foo-bar-baz", 16, GTK_ICON_LOOKUP_GENERIC_FALLBACK|GTK_ICON_LOOKUP_DIR_RTL,
                       "foo-bar-baz-rtl",
                       "foo-bar-baz",
                       "foo-bar-rtl",
                       "foo-bar",
                       "foo-rtl",
                       "foo",
                       NULL);
  assert_lookup_order ("foo-bar-baz", 16, GTK_ICON_LOOKUP_DIR_RTL,
                       "foo-bar-baz-rtl",
                       "foo-bar-baz",
                       NULL);
  assert_lookup_order ("foo-bar-baz-symbolic", 16, GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                       "foo-bar-baz-symbolic",
                       "foo-bar-symbolic",
                       "foo-symbolic",
                       "foo-bar-baz",
                       "foo-bar",
                       "foo",
                       NULL);

  assert_lookup_order ("bla-bla", 16, GTK_ICON_LOOKUP_GENERIC_FALLBACK|GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                       "bla-bla-symbolic",
                       "bla-symbolic",
                       "bla-bla",
                       "bla",
                       NULL);
  assert_lookup_order ("bla-bla-symbolic", 16, GTK_ICON_LOOKUP_GENERIC_FALLBACK|GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                       "bla-bla-symbolic",
                       "bla-symbolic",
                       "bla-bla-symbolic", /* awkward */
                       "bla-symbolic", /* awkward */
                       "bla-bla",
                       "bla",
                       NULL);

  assert_lookup_order ("bar-baz", 16, GTK_ICON_LOOKUP_FORCE_SYMBOLIC|GTK_ICON_LOOKUP_GENERIC_FALLBACK|GTK_ICON_LOOKUP_DIR_RTL,
                       "bar-baz-symbolic-rtl",
                       "bar-baz-symbolic",
                       "bar-symbolic-rtl",
                       "bar-symbolic",
                       "bar-baz-rtl",
                       "bar-baz",
                       "bar-rtl",
                       "bar",
                       NULL);
  assert_lookup_order ("bar-baz-symbolic", 16, GTK_ICON_LOOKUP_FORCE_SYMBOLIC|GTK_ICON_LOOKUP_GENERIC_FALLBACK|GTK_ICON_LOOKUP_DIR_RTL,
                       "bar-baz-symbolic-rtl",
                       "bar-baz-symbolic",
                       "bar-symbolic-rtl",
                       "bar-symbolic",
                       "bar-baz-symbolic-rtl", /* awkward */
                       "bar-baz-symbolic", /* awkward */
                       "bar-symbolic-rtl", /* awkward */
                       "bar-symbolic", /* awkward */
                       "bar-baz-rtl",
                       "bar-baz",
                       "bar-rtl",
                       "bar",
                       NULL);

  assert_lookup_order ("bar-baz", 16, GTK_ICON_LOOKUP_FORCE_SYMBOLIC|GTK_ICON_LOOKUP_GENERIC_FALLBACK|GTK_ICON_LOOKUP_DIR_LTR,
                       "bar-baz-symbolic-ltr",
                       "bar-baz-symbolic",
                       "bar-symbolic-ltr",
                       "bar-symbolic",
                       "bar-baz-ltr",
                       "bar-baz",
                       "bar-ltr",
                       "bar",
                       NULL);
  assert_lookup_order ("bar-baz-symbolic", 16, GTK_ICON_LOOKUP_FORCE_SYMBOLIC|GTK_ICON_LOOKUP_GENERIC_FALLBACK|GTK_ICON_LOOKUP_DIR_LTR,
                       "bar-baz-symbolic-ltr",
                       "bar-baz-symbolic",
                       "bar-symbolic-ltr",
                       "bar-symbolic",
                       "bar-baz-symbolic-ltr", /* awkward */
                       "bar-baz-symbolic", /* awkward */
                       "bar-symbolic-ltr", /* awkward */
                       "bar-symbolic", /* awkward */
                       "bar-baz-ltr",
                       "bar-baz",
                       "bar-ltr",
                       "bar",
                       NULL);
}

static void
test_generic_fallback (void)
{
  /* simple test for generic fallback */
  assert_icon_lookup ("simple-foo-bar",
                      16,
                      GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                      "/icons/16x16/simple.png");

  /* Check generic fallback also works for symbolics falling back to regular items */
  assert_icon_lookup ("simple-foo-bar-symbolic",
                      16,
                      GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                      "/icons/16x16/simple.png");

  /* Check we fall back to more generic symbolic icons before falling back to
   * non-symbolics */
  assert_icon_lookup ("everything-justregular-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                      "/icons/scalable/everything-symbolic.svg");
}

static void
test_force_symbolic (void)
{
  /* check forcing symbolic works */
  assert_icon_lookup ("everything",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      "/icons/scalable/everything-symbolic.svg");
  /* check forcing symbolic also works for symbolic icons (d'oh) */
  assert_icon_lookup ("everything-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      "/icons/scalable/everything-symbolic.svg");

  /* check all the combos for fallbacks on an icon that only exists as symbolic */
  assert_icon_lookup ("everything-justsymbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      "/icons/scalable/everything-justsymbolic-symbolic.svg");
  assert_icon_lookup ("everything-justsymbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_GENERIC_FALLBACK | GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      "/icons/scalable/everything-justsymbolic-symbolic.svg");
  assert_icon_lookup ("everything-justsymbolic-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      "/icons/scalable/everything-justsymbolic-symbolic.svg");
  assert_icon_lookup ("everything-justsymbolic-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_GENERIC_FALLBACK | GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      "/icons/scalable/everything-justsymbolic-symbolic.svg");

  /* check all the combos for fallbacks, this time for an icon that only exists as regular */
  assert_icon_lookup ("everything-justregular",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      "/icons/scalable/everything-justregular.svg");
  assert_icon_lookup ("everything-justregular",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_GENERIC_FALLBACK | GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      "/icons/scalable/everything-symbolic.svg");
  assert_icon_lookup_fails ("everything-justregular-symbolic",
                            SCALABLE_IMAGE_SIZE,
                            GTK_ICON_LOOKUP_FORCE_SYMBOLIC);
  assert_icon_lookup ("everything-justregular-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_GENERIC_FALLBACK | GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      "/icons/scalable/everything-symbolic.svg");
}

static void
test_force_regular (void)
{
  /* check forcing regular works (d'oh) */
  assert_icon_lookup ("everything",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_FORCE_REGULAR,
                      "/icons/scalable/everything.svg");
  /* check forcing regular also works for symbolic icons ) */
  assert_icon_lookup ("everything-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_FORCE_REGULAR,
                      "/icons/scalable/everything.svg");

  /* check all the combos for fallbacks on an icon that only exists as regular */
  assert_icon_lookup ("everything-justregular",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_FORCE_REGULAR,
                      "/icons/scalable/everything-justregular.svg");
  assert_icon_lookup ("everything-justregular",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_GENERIC_FALLBACK | GTK_ICON_LOOKUP_FORCE_REGULAR,
                      "/icons/scalable/everything-justregular.svg");
  assert_icon_lookup ("everything-justregular-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_FORCE_REGULAR,
                      "/icons/scalable/everything-justregular.svg");
  assert_icon_lookup ("everything-justregular-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_GENERIC_FALLBACK | GTK_ICON_LOOKUP_FORCE_REGULAR,
                      "/icons/scalable/everything-justregular.svg");

  /* check all the combos for fallbacks, this time for an icon that only exists as symbolic */
  assert_icon_lookup_fails ("everything-justsymbolic",
                            SCALABLE_IMAGE_SIZE,
                            GTK_ICON_LOOKUP_FORCE_REGULAR);
  assert_icon_lookup ("everything-justsymbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_GENERIC_FALLBACK | GTK_ICON_LOOKUP_FORCE_REGULAR,
                      "/icons/scalable/everything.svg");
  assert_icon_lookup ("everything-justsymbolic-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_FORCE_REGULAR,
                      "/icons/scalable/everything-justsymbolic-symbolic.svg");
  assert_icon_lookup ("everything-justsymbolic-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_GENERIC_FALLBACK | GTK_ICON_LOOKUP_FORCE_REGULAR,
                      "/icons/scalable/everything.svg");
}

static void
test_rtl (void)
{
  assert_icon_lookup ("everything",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_DIR_RTL,
                      "/icons/scalable/everything-rtl.svg");
  assert_icon_lookup ("everything-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_DIR_RTL,
                      "/icons/scalable/everything-symbolic-rtl.svg");

  assert_icon_lookup_fails ("everything-justrtl",
                            SCALABLE_IMAGE_SIZE,
                            0);
  assert_icon_lookup_fails ("everything-justrtl",
                            SCALABLE_IMAGE_SIZE,
                            GTK_ICON_LOOKUP_DIR_LTR);
  assert_icon_lookup ("everything-justrtl",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_DIR_RTL,
                      "/icons/scalable/everything-justrtl-rtl.svg");

  assert_icon_lookup ("everything-justrtl",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                      "/icons/scalable/everything.svg");
  assert_icon_lookup ("everything-justrtl",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_GENERIC_FALLBACK | GTK_ICON_LOOKUP_DIR_LTR,
                      "/icons/scalable/everything.svg");
  assert_icon_lookup ("everything-justrtl",
                      SCALABLE_IMAGE_SIZE,
                      GTK_ICON_LOOKUP_GENERIC_FALLBACK | GTK_ICON_LOOKUP_DIR_RTL,
                      "/icons/scalable/everything-justrtl-rtl.svg");
}

static void
test_symbolic_single_size (void)
{
  /* Check we properly load a symbolic icon from a sized directory */
  assert_icon_lookup ("only32-symbolic",
                      32,
                      0,
                      "/icons/32x32/only32-symbolic.svg");
  /* Check that we still properly load it even if a different size is requested */
  assert_icon_lookup ("only32-symbolic",
                      16,
                      0,
                      "/icons/32x32/only32-symbolic.svg");
  assert_icon_lookup ("only32-symbolic",
                      128,
                      0,
                      "/icons/32x32/only32-symbolic.svg");
}

static void
test_svg_size (void)
{
   /* To understand these results, keep in mind that we never allow upscaling,
   * and don't respect min/max size for scaling (though we do take it into
   * account for choosing).
   */
  /* Check we properly load a svg icon from a sized directory */
  assert_icon_lookup_size ("twosize-fixed", 48, 0, "/icons/32x32/twosize-fixed.svg", 32);
  assert_icon_lookup_size ("twosize-fixed", 32, 0, "/icons/32x32/twosize-fixed.svg", 32);
  assert_icon_lookup_size ("twosize-fixed", 20, 0, "/icons/32x32/twosize-fixed.svg", 32);
  assert_icon_lookup_size ("twosize-fixed", 16, 0, "/icons/16x16/twosize-fixed.svg", 16);

  /* Check that we still properly load it even if a different size is requested */
  assert_icon_lookup_size ("twosize", 64, 0, "/icons/32x32s/twosize.svg", 48);
  assert_icon_lookup_size ("twosize", 48, 0, "/icons/32x32s/twosize.svg", 48);
  assert_icon_lookup_size ("twosize", 32, 0, "/icons/32x32s/twosize.svg", 32);
  assert_icon_lookup_size ("twosize", 24, 0, "/icons/32x32s/twosize.svg", 24);
  assert_icon_lookup_size ("twosize", 16, 0, "/icons/16x16s/twosize.svg", 16);
  assert_icon_lookup_size ("twosize", 12, 0, "/icons/16x16s/twosize.svg", 12);
  assert_icon_lookup_size ("twosize",  8, 0, "/icons/16x16s/twosize.svg", 12);
}

static void
test_builtin (void)
{
  assert_icon_lookup_size ("gtk-color-picker", 16, GTK_ICON_LOOKUP_USE_BUILTIN, "/org/gtk/libgtk/icons/16x16/actions/gtk-color-picker.png", 16);
  assert_icon_lookup_size ("gtk-color-picker", 20, GTK_ICON_LOOKUP_USE_BUILTIN, "/org/gtk/libgtk/icons/24x24/actions/gtk-color-picker.png", 24);
  assert_icon_lookup_size ("gtk-color-picker", 24, GTK_ICON_LOOKUP_USE_BUILTIN, "/org/gtk/libgtk/icons/24x24/actions/gtk-color-picker.png", 24);
  assert_icon_lookup_size ("gtk-color-picker", 30, GTK_ICON_LOOKUP_USE_BUILTIN, "/org/gtk/libgtk/icons/24x24/actions/gtk-color-picker.png", 24);
}

static void
test_size (void)
{
  assert_icon_lookup_size ("size-test", 12, 0, "/icons/15/size-test.png", 15);
  assert_icon_lookup_size ("size-test", 13, 0, "/icons/15/size-test.png", 15);
  assert_icon_lookup_size ("size-test", 14, 0, "/icons/15/size-test.png", 15);
  assert_icon_lookup_size ("size-test", 15, 0, "/icons/15/size-test.png", 15);
  assert_icon_lookup_size ("size-test", 16, 0, "/icons/16-22/size-test.png", 19);
  assert_icon_lookup_size ("size-test", 17, 0, "/icons/16-22/size-test.png", 19);
  assert_icon_lookup_size ("size-test", 18, 0, "/icons/16-22/size-test.png", 19);
  assert_icon_lookup_size ("size-test", 19, 0, "/icons/16-22/size-test.png", 19);
  /* the next 3 are because we never scale up */
  assert_icon_lookup_size ("size-test", 20, 0, "/icons/25+/size-test.svg", 25);
  assert_icon_lookup_size ("size-test", 21, 0, "/icons/25+/size-test.svg", 25);
  assert_icon_lookup_size ("size-test", 22, 0, "/icons/25+/size-test.svg", 25);

  assert_icon_lookup_size ("size-test", 23, 0, "/icons/25+/size-test.svg", 25);
  assert_icon_lookup_size ("size-test", 23, 0, "/icons/25+/size-test.svg", 25);
  assert_icon_lookup_size ("size-test", 25, 0, "/icons/25+/size-test.svg", 25);
  assert_icon_lookup_size ("size-test", 28, 0, "/icons/25+/size-test.svg", 28);
  /* the next 2 are because we never scale up */
  assert_icon_lookup_size ("size-test", 31, 0, "/icons/35+/size-test.svg", 35);
  assert_icon_lookup_size ("size-test", 34, 0, "/icons/35+/size-test.svg", 35);

  assert_icon_lookup_size ("size-test", 37, 0, "/icons/35+/size-test.svg", 37);
  assert_icon_lookup_size ("size-test", 40, 0, "/icons/35+/size-test.svg", 40);
  assert_icon_lookup_size ("size-test", 45, 0, "/icons/35+/size-test.svg", 45);

  assert_icon_lookup_size ("size-test", 12, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/15/size-test.png", 12);
  assert_icon_lookup_size ("size-test", 13, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/15/size-test.png", 13);
  assert_icon_lookup_size ("size-test", 14, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/15/size-test.png", 14);
  assert_icon_lookup_size ("size-test", 15, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/15/size-test.png", 15);
  assert_icon_lookup_size ("size-test", 16, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/16-22/size-test.png", 16);
  assert_icon_lookup_size ("size-test", 17, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/16-22/size-test.png", 17);
  assert_icon_lookup_size ("size-test", 18, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/16-22/size-test.png", 18);
  assert_icon_lookup_size ("size-test", 19, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/16-22/size-test.png", 19);
  //assert_icon_lookup_size ("size-test", 20, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/16-22/size-test.png", 20);
  //assert_icon_lookup_size ("size-test", 21, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/16-22/size-test.png", 21);
  //assert_icon_lookup_size ("size-test", 22, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/16-22/size-test.png", 22);
  assert_icon_lookup_size ("size-test", 23, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/25+/size-test.svg", 23);
  assert_icon_lookup_size ("size-test", 24, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/25+/size-test.svg", 24);
  assert_icon_lookup_size ("size-test", 25, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/25+/size-test.svg", 25);
  assert_icon_lookup_size ("size-test", 28, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/25+/size-test.svg", 28);
  //assert_icon_lookup_size ("size-test", 31, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/25+/size-test.svg", 31);
  //assert_icon_lookup_size ("size-test", 34, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/25+/size-test.svg", 34);
  assert_icon_lookup_size ("size-test", 37, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/35+/size-test.svg", 37);
  assert_icon_lookup_size ("size-test", 40, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/35+/size-test.svg", 40);
  assert_icon_lookup_size ("size-test", 45, GTK_ICON_LOOKUP_FORCE_SIZE, "/icons/35+/size-test.svg", 45);
}

static void
test_list (void)
{
  GtkIconTheme *theme;
  GList *icons;

  theme = get_test_icontheme (TRUE);
  icons = gtk_icon_theme_list_icons (theme, NULL);

  g_assert (g_list_find_custom (icons, "size-test", (GCompareFunc)g_strcmp0));
  g_assert (g_list_find_custom (icons, "simple", (GCompareFunc)g_strcmp0));
  g_assert (g_list_find_custom (icons, "twosize-fixed", (GCompareFunc)g_strcmp0));
  g_assert (g_list_find_custom (icons, "twosize", (GCompareFunc)g_strcmp0));
  g_assert (g_list_find_custom (icons, "only32-symbolic", (GCompareFunc)g_strcmp0));
  g_assert (g_list_find_custom (icons, "everything", (GCompareFunc)g_strcmp0));
  g_assert (g_list_find_custom (icons, "everything-rtl", (GCompareFunc)g_strcmp0));
  g_assert (g_list_find_custom (icons, "everything-symbolic", (GCompareFunc)g_strcmp0));
  g_assert (g_list_find_custom (icons, "everything-justregular", (GCompareFunc)g_strcmp0));
  g_assert (g_list_find_custom (icons, "everything-justrtl-rtl", (GCompareFunc)g_strcmp0));
  g_assert (g_list_find_custom (icons, "everything-symbolic-rtl", (GCompareFunc)g_strcmp0));
  g_assert (g_list_find_custom (icons, "everything-justsymbolic-symbolic", (GCompareFunc)g_strcmp0));

  g_assert (gtk_icon_theme_has_icon (theme, "size-test"));
  g_assert (gtk_icon_theme_has_icon (theme, "simple"));
  g_assert (gtk_icon_theme_has_icon (theme, "twosize-fixed"));
  g_assert (gtk_icon_theme_has_icon (theme, "twosize"));
  g_assert (gtk_icon_theme_has_icon (theme, "only32-symbolic"));
  g_assert (gtk_icon_theme_has_icon (theme, "everything"));
  g_assert (gtk_icon_theme_has_icon (theme, "everything-rtl"));
  g_assert (gtk_icon_theme_has_icon (theme, "everything-symbolic"));
  g_assert (gtk_icon_theme_has_icon (theme, "everything-justregular"));
  g_assert (gtk_icon_theme_has_icon (theme, "everything-justrtl-rtl"));
  g_assert (gtk_icon_theme_has_icon (theme, "everything-symbolic-rtl"));
  g_assert (gtk_icon_theme_has_icon (theme, "everything-justsymbolic-symbolic"));

  g_list_free_full (icons, g_free);
}

static gint loaded;

static void
load_icon (GObject      *source,
           GAsyncResult *res,
           gpointer      data)
{
  GtkIconInfo *info = (GtkIconInfo *)source;
  GError *error = NULL;
  GdkPixbuf *pixbuf;

  pixbuf = gtk_icon_info_load_icon_finish (info, res, &error);
  g_assert (pixbuf != NULL);
  g_assert_no_error (error);
  g_object_unref (pixbuf);

  loaded++;
}

static void
load_symbolic (GObject      *source,
               GAsyncResult *res,
               gpointer      data)
{
  GtkIconInfo *info = (GtkIconInfo *)source;
  GError *error = NULL;
  gboolean symbolic;
  GdkPixbuf *pixbuf;

  pixbuf = gtk_icon_info_load_symbolic_finish (info, res, &symbolic, &error);
  g_assert (pixbuf != NULL);
  g_assert_no_error (error);
  g_object_unref (pixbuf);

  loaded++;
}

static gboolean
quit_loop (gpointer data)
{
  GMainLoop *loop = data;

  if (loaded == 2)
    {
      g_main_loop_quit (loop);
      return G_SOURCE_REMOVE;
    }
  return G_SOURCE_CONTINUE;
}

static void
test_async (void)
{
  GtkIconInfo *info1, *info2;
  GtkIconTheme *theme;
  GMainLoop *loop;
  GdkRGBA fg, red, green, blue;

  gdk_rgba_parse (&fg, "white");
  gdk_rgba_parse (&red, "red");
  gdk_rgba_parse (&green, "green");
  gdk_rgba_parse (&blue, "blue");

  loop = g_main_loop_new (NULL, FALSE);
  g_idle_add_full (G_PRIORITY_LOW, quit_loop, loop, NULL);

  theme = get_test_icontheme (TRUE);
  info1 = gtk_icon_theme_lookup_icon (theme, "twosize-fixed", 32, 0);
  info2 = gtk_icon_theme_lookup_icon (theme, "only32-symbolic", 32, 0);
  g_assert (info1);
  g_assert (info2);
  gtk_icon_info_load_icon_async (info1, NULL, load_icon, NULL);
  gtk_icon_info_load_symbolic_async (info2, &fg, &red, &green, &blue, NULL, load_symbolic, NULL);
  g_object_unref (info1);
  g_object_unref (info2);

  g_main_loop_run (loop);
  g_main_loop_unref (loop);  

  g_assert (loaded == 2);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/icontheme/basics", test_basics);
  g_test_add_func ("/icontheme/lookup-order", test_lookup_order);
  g_test_add_func ("/icontheme/generic-fallback", test_generic_fallback);
  g_test_add_func ("/icontheme/force-symbolic", test_force_symbolic);
  g_test_add_func ("/icontheme/force-regular", test_force_regular);
  g_test_add_func ("/icontheme/rtl", test_rtl);
  g_test_add_func ("/icontheme/symbolic-single-size", test_symbolic_single_size);
  g_test_add_func ("/icontheme/svg-size", test_svg_size);
  g_test_add_func ("/icontheme/size", test_size);
  g_test_add_func ("/icontheme/builtin", test_builtin);
  g_test_add_func ("/icontheme/list", test_list);
  g_test_add_func ("/icontheme/async", test_async);

  return g_test_run();
}
