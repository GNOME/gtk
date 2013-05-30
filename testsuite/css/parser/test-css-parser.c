/*
 * Copyright (C) 2011 Red Hat Inc.
 *
 * Author:
 *      Benjamin Otte <otte@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

static char *
test_get_reference_file (const char *css_file)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (css_file, ".css"))
    g_string_append_len (file, css_file, strlen (css_file) - 4);
  else
    g_string_append (file, css_file);
  
  g_string_append (file, ".ref.css");

  if (!g_file_test (file->str, G_FILE_TEST_EXISTS))
    {
      g_string_free (file, TRUE);
      return g_strdup (css_file);
    }

  return g_string_free (file, FALSE);
}

static char *
test_get_errors_file (const char *css_file)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (css_file, ".css"))
    g_string_append_len (file, css_file, strlen (css_file) - 4);
  else
    g_string_append (file, css_file);
  
  g_string_append (file, ".errors");

  if (!g_file_test (file->str, G_FILE_TEST_EXISTS))
    {
      g_string_free (file, TRUE);
      return NULL;
    }

  return g_string_free (file, FALSE);
}

static char *
diff_with_file (const char  *file1,
                char        *text,
                gssize       len,
                GError     **error)
{
  const char *command[] = { "diff", "-u", file1, NULL, NULL };
  char *diff, *tmpfile;
  int fd;

  diff = NULL;

  if (len < 0)
    len = strlen (text);
  
  /* write the text buffer to a temporary file */
  fd = g_file_open_tmp (NULL, &tmpfile, error);
  if (fd < 0)
    return NULL;

  if (write (fd, text, len) != (int) len)
    {
      close (fd);
      g_set_error (error,
                   G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   "Could not write data to temporary file '%s'", tmpfile);
      goto done;
    }
  close (fd);
  command[3] = tmpfile;

  /* run diff command */
  g_spawn_sync (NULL, 
                (char **) command,
                NULL,
                G_SPAWN_SEARCH_PATH,
                NULL, NULL,
	        &diff,
                NULL, NULL,
                error);

done:
  g_unlink (tmpfile);
  g_free (tmpfile);

  return diff;
}

static void
append_error_value (GString *string,
                    GType    enum_type,
                    guint    value)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  enum_class = g_type_class_ref (enum_type);
  enum_value = g_enum_get_value (enum_class, value);

  g_string_append (string, enum_value->value_name);

  g_type_class_unref (enum_class);
}

static void
parsing_error_cb (GtkCssProvider *provider,
                  GtkCssSection  *section,
                  const GError   *error,
                  GString        *errors)
{
  char *basename;

  basename = g_file_get_basename (gtk_css_section_get_file (section));
  g_string_append_printf (errors,
                          "%s:%u: error: ",
                          basename, gtk_css_section_get_end_line (section) + 1);
  g_free (basename);
                          
  if (error->domain == GTK_CSS_PROVIDER_ERROR)
      append_error_value (errors, GTK_TYPE_CSS_PROVIDER_ERROR, error->code);
  else
    g_string_append_printf (errors, 
                            "%s %u\n",
                            g_quark_to_string (error->domain),
                            error->code);

  g_string_append_c (errors, '\n');
}

static void
test_css_file (GFile *file)
{
  GtkCssProvider *provider;
  char *css, *diff;
  char *css_file, *reference_file, *errors_file;
  GString *errors;
  GError *error = NULL;

  css_file = g_file_get_path (file);
  errors = g_string_new ("");

  provider = gtk_css_provider_new ();
  g_signal_connect (provider, 
                    "parsing-error",
                    G_CALLBACK (parsing_error_cb),
                    errors);
  gtk_css_provider_load_from_path (provider,
                                   css_file,
                                   NULL);

  css = gtk_css_provider_to_string (provider);

  reference_file = test_get_reference_file (css_file);

  diff = diff_with_file (reference_file, css, -1, &error);
  g_assert_no_error (error);

  if (diff && diff[0])
    {
      g_test_message ("Resulting CSS doesn't match reference:\n%s", diff);
      g_test_fail ();
    }

  g_free (css);
  g_free (reference_file);

  errors_file = test_get_errors_file (css_file);

  if (errors_file)
    {
      diff = diff_with_file (errors_file, errors->str, errors->len, &error);
      g_assert_no_error (error);

      if (diff && diff[0])
        {
          g_test_message ("Errors don't match expected errors:\n%s", diff);
          g_test_fail ();
        }
    }
  else if (errors->str[0])
    {
      g_test_message ("Unexpected errors:\n%s", errors->str);
      g_test_fail ();
    }

  g_free (errors_file);
  g_string_free (errors, TRUE);

  g_free (diff);
  g_free (css_file);
}

static void
add_test_for_file (GFile *file)
{
  g_test_add_vtable (g_file_get_path (file),
                     0,
                     g_object_ref (file),
                     NULL,
                     (GTestFixtureFunc) test_css_file,
                     (GTestFixtureFunc) g_object_unref);
}

static int
compare_files (gconstpointer a, gconstpointer b)
{
  GFile *file1 = G_FILE (a);
  GFile *file2 = G_FILE (b);
  char *path1, *path2;
  int result;

  path1 = g_file_get_path (file1);
  path2 = g_file_get_path (file2);

  result = strcmp (path1, path2);

  g_free (path1);
  g_free (path2);

  return result;
}

static void
add_tests_for_files_in_directory (GFile *dir)
{
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GList *files;
  GError *error = NULL;

  enumerator = g_file_enumerate_children (dir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &error);
  g_assert_no_error (error);
  files = NULL;

  while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)))
    {
      const char *filename;

      filename = g_file_info_get_name (info);

      if (!g_str_has_suffix (filename, ".css") ||
          g_str_has_suffix (filename, ".out.css") ||
          g_str_has_suffix (filename, ".ref.css"))
        {
          g_object_unref (info);
          continue;
        }

      files = g_list_prepend (files, g_file_get_child (dir, filename));

      g_object_unref (info);
    }
  
  g_assert_no_error (error);
  g_object_unref (enumerator);

  files = g_list_sort (files, compare_files);
  g_list_foreach (files, (GFunc) add_test_for_file, NULL);
  g_list_free_full (files, g_object_unref);
}

static gboolean
parse_uint8 (const char *string,
             GValue *value,
             GError **error)
{
  g_value_set_uchar (value, 42);
  return TRUE;
}

int
main (int argc, char **argv)
{
  gtk_test_init (&argc, &argv);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  /* Add a bunch of properties so we can test that we parse them properly */
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boolean ("boolean-property",
                                                                "boolean property",
                                                                "test boolean properties",
                                                                TRUE,
                                                                G_PARAM_READABLE));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("int-property",
                                                            "int property",
                                                            "test int properties",
                                                            G_MININT, G_MAXINT, 0,
                                                            G_PARAM_READABLE));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_uint ("uint-property",
                                                             "uint property",
                                                             "test uint properties",
                                                             0, G_MAXUINT, 0,
                                                             G_PARAM_READABLE));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_string ("string-property",
                                                               "string property",
                                                               "test string properties",
                                                               NULL,
                                                               G_PARAM_READABLE));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("rgba-property",
                                                              "rgba property",
                                                              "test rgba properties",
                                                              GDK_TYPE_RGBA,
                                                              G_PARAM_READABLE));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("color-property",
                                                              "color property",
                                                              "test color properties",
                                                              GDK_TYPE_COLOR,
                                                              G_PARAM_READABLE));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("border-property",
                                                              "border property",
                                                              "test border properties",
                                                              GTK_TYPE_BORDER,
                                                              G_PARAM_READABLE));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("font-property",
                                                              "font property",
                                                              "test font properties",
                                                              PANGO_TYPE_FONT_DESCRIPTION,
                                                              G_PARAM_READABLE));
#if 0
  /* not public API, use transition instead */
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("animation-property",
                                                              "animation property",
                                                              "test animation properties",
                                                              GTK_TYPE_ANIMATION_DESCRIPTION,
                                                              G_PARAM_READABLE));
#endif
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_object ("engine-property",
                                                               "engine property",
                                                               "test theming engine properties",
                                                               GTK_TYPE_THEMING_ENGINE,
                                                               G_PARAM_READABLE));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_enum ("enum-property",
                                                             "enum property",
                                                             "test enum properties",
                                                             GTK_TYPE_SHADOW_TYPE,
                                                             0,
                                                             G_PARAM_READABLE));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_flags ("flags-property",
                                                              "flags property",
                                                              "test flags properties",
                                                              GTK_TYPE_STATE_FLAGS,
                                                              GTK_STATE_FLAG_NORMAL,
                                                              G_PARAM_READABLE));
  gtk_style_properties_register_property (parse_uint8,
                                          g_param_spec_uchar ("uint8-property",
                                                              "uint8 property",
                                                              "test uint8 properties",
                                                              0, G_MAXUINT8, 0,
                                                              G_PARAM_READABLE));
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (argc < 2)
    {
      const char *basedir;
      GFile *dir;

      basedir = g_test_get_dir (G_TEST_DIST);
      dir = g_file_new_for_path (basedir);
      add_tests_for_files_in_directory (dir);

      g_object_unref (dir);
    }
  else
    {
      guint i;

      for (i = 1; i < argc; i++)
        {
          GFile *file = g_file_new_for_commandline_arg (argv[i]);

          add_test_for_file (file);

          g_object_unref (file);
        }
    }

  return g_test_run ();
}

