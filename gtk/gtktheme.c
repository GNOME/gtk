/* GTK - The GIMP Toolkit
 * Copyright (C) 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtktheme.h"
#include "gtkcssproviderprivate.h"

static gboolean
theme_exists (const char *theme)
{
  char *path;

  path = _gtk_css_find_theme (theme);
  if (path)
    {
      g_free (path);
      return TRUE;
    }

  return FALSE;
}

/**
 * gtk_theme_get_dark_variant:
 * @theme: a theme name
 *
 * Returns the name of the dark variant of the
 * given theme, if such a variant is available.
 * Otherwise, @theme is returned.
 *
 * Returns: (transfer full): name of the dark variant
 *     of @theme. Free with g_free()
 */
char *
gtk_theme_get_dark_variant (const char *theme)
{
  if (g_str_equal (theme, "HighContrast"))
    return g_strdup ("HighContrastInverse");

  if (g_str_equal (theme, "Adwaita"))
    return g_strdup ("Adwaita-dark");

  if (!g_str_has_suffix (theme, "-dark"))
    {
      char *dark = g_strconcat (theme, "-dark", NULL);
      if (theme_exists (dark))
        return dark;

      g_free (dark);
    }

  return g_strdup (theme);
}

/**
 * gtk_theme_get_light_variant:
 * @theme: a theme name
 *
 * Returns the name of the light variant of the
 * given theme, if such a variant is available.
 * Otherwise, @theme is returned.
 *
 * Returns: (transfer full): name of the light variant
 *     of @theme. Free with g_free()
 */
char *
gtk_theme_get_light_variant (const char *theme)
{
  if (g_str_equal (theme, "HighContrastInverse"))
    return g_strdup ("HighContrast");

  if (g_str_equal (theme, "Adwaita-dark"))
    return g_strdup ("Adwaita");

  if (g_str_has_suffix (theme, "-dark"))
    {
      char *light = g_strndup (theme, strlen (theme) - strlen ("-dark"));
      if (theme_exists (light))
        return light;

      g_free (light);
    }

  return g_strdup (theme);
}

static void
fill_gtk (const gchar *path,
          GHashTable  *t)
{
  const gchar *dir_entry;
  GDir *dir = g_dir_open (path, 0, NULL);

  if (!dir)
    return;

  while ((dir_entry = g_dir_read_name (dir)))
    {
      gchar *filename = g_build_filename (path, dir_entry, "gtk-4.0", "gtk.css", NULL);

      if (g_file_test (filename, G_FILE_TEST_IS_REGULAR) &&
          !g_hash_table_contains (t, dir_entry))
        g_hash_table_add (t, g_strdup (dir_entry));

      g_free (filename);
    }

  g_dir_close (dir);
}

/**
 * gtk_theme_get_available_themes:
 *
 * Returns the list of available themes.
 *
 * Returns: (transfer full): an array of theme names
 */
char **
gtk_theme_get_available_themes (void)
{
  GHashTable *t;
  GHashTableIter iter;
  gchar *theme, *path;
  gchar **builtin_themes;
  guint i;
  const gchar * const *dirs;
  char **keys;
  char **result;

  t = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  /* Builtin themes */
  builtin_themes = g_resources_enumerate_children ("/org/gtk/libgtk/theme", 0, NULL);
  for (i = 0; builtin_themes[i] != NULL; i++)
    {
      if (g_str_has_suffix (builtin_themes[i], "/"))
        g_hash_table_add (t, g_strndup (builtin_themes[i], strlen (builtin_themes[i]) - 1));
    }
  g_strfreev (builtin_themes);

  path = _gtk_get_theme_dir ();
  fill_gtk (path, t);
  g_free (path);

  path = g_build_filename (g_get_user_data_dir (), "themes", NULL);
  fill_gtk (path, t);
  g_free (path);

  path = g_build_filename (g_get_home_dir (), ".themes", NULL);
  fill_gtk (path, t);
  g_free (path);

  dirs = g_get_system_data_dirs ();
  for (i = 0; dirs[i]; i++)
    {
      path = g_build_filename (dirs[i], "themes", NULL);
      fill_gtk (path, t);
      g_free (path);
    }

  keys = (char **)g_hash_table_get_keys_as_array (t, NULL);
  result = g_strdupv (keys);
  g_free (keys);
  g_hash_table_destroy (t);

  return result;
}
