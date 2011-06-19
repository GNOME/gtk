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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#define DEPTH_INCREMENT 2

static char *
get_test_file (const char *test_file,
               const char *extension,
               gboolean    must_exist)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (test_file, ".ui"))
    g_string_append_len (file, test_file, strlen (test_file) - strlen (".ui"));
  else
    g_string_append (file, test_file);
  
  g_string_append (file, extension);

  if (must_exist &&
      !g_file_test (file->str, G_FILE_TEST_EXISTS))
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

static const char *
get_name (AtkObject *accessible)
{
  char *name;

  name = g_object_get_data (G_OBJECT (accessible), "gtk-accessibility-dump-name");
  if (name)
    return name;

  if (GTK_IS_ACCESSIBLE (accessible))
    {
      GtkWidget *widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
      
      name = g_strdup (gtk_buildable_get_name (GTK_BUILDABLE (widget)));
    }
  else
    name = NULL;

  if (name == NULL)
    {
      /* XXX: Generate a unique, repeatable name */
      g_assert_not_reached ();
    }

  g_object_set_data_full (G_OBJECT (accessible), "gtk-accessibility-dump-name", name, g_free);
  return name;
}

static void
dump_relation (GString     *string,
               guint        depth,
               AtkRelation *relation)
{
  GPtrArray *targets;
  const char *name;
  guint i;

  targets = atk_relation_get_target (relation);
  if (targets == NULL || targets->len == 0)
    return;

  name = atk_relation_type_get_name (atk_relation_get_relation_type (relation));
  g_string_append_printf (string, "%*s%s: %s\n", depth, "", name, get_name (g_ptr_array_index (targets, 0)));
  depth += strlen (name) + 2;

  for (i = 1; i < targets->len; i++)
    {
      g_string_append_printf (string, "%*s%s\n", depth, "", get_name (g_ptr_array_index (targets, i)));
    }
}

static void
dump_relation_set (GString        *string,
                   guint           depth,
                   AtkRelationSet *set)
{
  guint i;

  if (set == NULL)
    return;

  for (i = 0; i < atk_relation_set_get_n_relations (set); i++)
    {
      AtkRelation *relation;
      
      relation = atk_relation_set_get_relation (set, i);

      dump_relation (string, depth, relation);
    }

  g_object_unref (set);
}

static void
dump_state_set (GString     *string,
                guint        depth,
                AtkStateSet *set)
{
  guint i;

  if (set == NULL)
    return;

  if (!atk_state_set_is_empty (set))
    {
      g_string_append_printf (string, "%*sstate:", depth, "");
      for (i = 0; i < ATK_STATE_LAST_DEFINED; i++)
        {
          if (atk_state_set_contains_state (set, i))
            g_string_append_printf (string, " %s", atk_state_type_get_name (i));
        }
      g_string_append_c (string, '\n');
    }

  g_object_unref (set);
}

static void
dump_attribute (GString      *string,
                guint         depth,
                AtkAttribute *attribute)
{
  g_string_append_printf (string, "%*s%s: %s\n", depth, "", attribute->name, attribute->value);
}

static void
dump_attribute_set (GString         *string,
                    guint            depth,
                    AtkAttributeSet *set)
{
  GSList *l;
  AtkAttribute *attribute;

  for (l = set; l; l = l->next)
    {
      attribute = l->data;

      dump_attribute (string, depth, attribute);
    }
}

static void
dump_accessible (AtkObject     *accessible,
                 guint          depth,
                 GString       *string)
{
  guint i;

  g_string_append_printf (string, "%*s%s\n", depth, "", get_name (accessible));
  depth += DEPTH_INCREMENT;

  g_string_append_printf (string, "%*s\"%s\"\n", depth, "", atk_role_get_name (atk_object_get_role (accessible)));
  if (atk_object_get_parent (accessible))
    g_string_append_printf (string, "%*sparent: %s\n", depth, "", get_name (atk_object_get_parent (accessible)));
  if (atk_object_get_index_in_parent (accessible) != -1)
    g_string_append_printf (string, "%*sindex: %d\n", depth, "", atk_object_get_index_in_parent (accessible));
  if (atk_object_get_name (accessible))
    g_string_append_printf (string, "%*sname: %s\n", depth, "", atk_object_get_name (accessible));
  if (atk_object_get_description (accessible))
    g_string_append_printf (string, "%*sdescription: %s\n", depth, "", atk_object_get_description (accessible));
  dump_relation_set (string, depth, atk_object_ref_relation_set (accessible));
  dump_state_set (string, depth, atk_object_ref_state_set (accessible));
  dump_attribute_set (string, depth, atk_object_get_attributes (accessible));

  for (i = 0; i < atk_object_get_n_accessible_children (accessible); i++)
    {
      AtkObject *child = atk_object_ref_accessible_child (accessible, i);
      dump_accessible (child, depth, string);
      g_object_unref (child);
    }
}

static GtkWidget *
builder_get_toplevel (GtkBuilder *builder)
{
  GSList *list, *walk;
  GtkWidget *window = NULL;

  list = gtk_builder_get_objects (builder);
  for (walk = list; walk; walk = walk->next)
    {
      if (GTK_IS_WINDOW (walk->data) &&
          gtk_widget_get_parent (walk->data) == NULL)
        {
          window = walk->data;
          break;
        }
    }
  
  g_slist_free (list);

  return window;
}

static void
dump_ui_file (const char *ui_file,
              GString *string)
{
  GtkWidget *window;
  GtkBuilder *builder;
  GError *error = NULL;

  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, ui_file, &error);
  g_assert_no_error (error);
  window = builder_get_toplevel (builder);
  g_object_unref (builder);
  g_assert (window);

  gtk_widget_show (window);

  dump_accessible (gtk_widget_get_accessible (window), 0, string);
  gtk_widget_destroy (window);
}

static void
dump_to_stdout (GFile *file)
{
  char *ui_file;
  GString *dump;

  ui_file = g_file_get_path (file);
  dump = g_string_new ("");

  dump_ui_file (ui_file, dump);
  g_print ("%s", dump->str);

  g_string_free (dump, TRUE);
  g_free (ui_file);
}

static void
test_ui_file (GFile *file)
{
  char *diff;
  char *ui_file, *a11y_file;
  GString *dump;
  GError *error = NULL;

  ui_file = g_file_get_path (file);
  a11y_file = get_test_file (ui_file, ".txt", TRUE);
  dump = g_string_new ("");

  dump_ui_file (ui_file, dump);

  if (a11y_file)
    {
      diff = diff_with_file (a11y_file, dump->str, dump->len, &error);
      g_assert_no_error (error);

      if (diff && diff[0])
        {
          g_test_message ("Contents don't match expected contents:\n%s", diff);
          g_test_fail ();
        }
    }
  else if (dump->str[0])
    {
      g_test_message ("Expected a reference file:\n%s", dump->str);
      g_test_fail ();
    }

  g_string_free (dump, TRUE);
  g_free (diff);
  g_free (a11y_file);
  g_free (ui_file);
}

static void
add_test_for_file (GFile *file)
{
  g_test_add_vtable (g_file_get_path (file),
                     0,
                     g_object_ref (file),
                     NULL,
                     (GTestFixtureFunc) test_ui_file,
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

      if (!g_str_has_suffix (filename, ".ui"))
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

int
main (int argc, char **argv)
{
  gtk_test_init (&argc, &argv);

  if (argc < 2)
    {
      const char *basedir;
      GFile *dir;

      if (g_getenv ("srcdir"))
        basedir = g_getenv ("srcdir");
      else
        basedir = ".";
        
      dir = g_file_new_for_path (basedir);
      
      add_tests_for_files_in_directory (dir);

      g_object_unref (dir);
    }
  else if (argc == 3 && strcmp (argv[1], "--generate") == 0)
    {
      GFile *file = g_file_new_for_commandline_arg (argv[2]);

      dump_to_stdout (file);

      g_object_unref (file);

      return 0;
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

