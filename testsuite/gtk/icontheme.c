#include <gtk/gtk.h>

#include <string.h>

static GtkIconTheme *
get_test_icontheme (void)
{
  static GtkIconTheme *icon_theme = NULL;
  const char *current_dir;

  if (icon_theme)
    return icon_theme;

  icon_theme = gtk_icon_theme_new ();
  gtk_icon_theme_set_custom_theme (icon_theme, "icons");
  current_dir = g_get_current_dir ();
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
assert_icon_lookup (const char         *icon_name,
                    gint                size,
                    GtkIconLookupFlags  flags,
                    const char         *filename)
{
  GtkIconInfo *info;

  info = gtk_icon_theme_lookup_icon (get_test_icontheme (), icon_name, size, flags);
  if (info == NULL)
    {
      g_error ("Could not look up an icon for \"%s\" with flags %s at size %d",
               icon_name, lookup_flags_to_string (flags), size);
      return;
    }

  if (!g_str_has_suffix (gtk_icon_info_get_filename (info), filename))
    {
      g_error ("Icon for \"%s\" with flags %s at size %d should be \"...%s\" but is \"...%s\"",
               icon_name, lookup_flags_to_string (flags), size,
               filename, gtk_icon_info_get_filename (info) + strlen (gtk_icon_info_get_filename (info)) - strlen (filename));
      return;
    }

  g_object_unref (info);
}

static void
test_basics (void)
{
  assert_icon_lookup ("simple", 16, 0, "/icons/16x16/simple.png");
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/icontheme/basics", test_basics);

  return g_test_run();
}
