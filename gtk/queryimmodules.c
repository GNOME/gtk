/* GTK+
 * querymodules.c:
 *
 * Copyright (C) 2000-2010 Red Hat Software
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

#include <glib.h>
#include <glib/gprintf.h>

#include <errno.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef USE_LA_MODULES
#define SOEXT ".la"
#else
#define SOEXT ("." G_MODULE_SUFFIX)
#endif

#include "gtk/gtkimcontextinfo.h"
#include "gtk/gtkversion.h"
#include "gtk/gtkutilsprivate.h"

#include "gtk/deprecated/gtkrc.h"

static void
escape_string (GString *contents, const char *str)
{
  while (TRUE)
    {
      char c = *str++;

      switch (c)
        {
        case '\0':
          goto done;
        case '\n':
          g_string_append (contents, "\\n");
          break;
        case '\"':
          g_string_append (contents, "\\\"");
          break;
#ifdef G_OS_WIN32
                /* Replace backslashes in path with forward slashes, so that
                 * it reads in without problems.
                 */
        case '\\':
          g_string_append (contents, "/");
          break;
#endif
        default:
          g_string_append_c (contents, c);
        }
    }

 done:;
}

static void
print_escaped (GString *contents, const char *str)
{
  g_string_append_c (contents, '"');
  escape_string (contents, str);
  g_string_append_c (contents, '"');
  g_string_append_c (contents, ' ');
}

static gboolean
query_module (const char *dir, const char *name, GString *contents)
{
  void          (*list)   (const GtkIMContextInfo ***contexts,
                           guint                    *n_contexts);

  gpointer list_ptr;
  gpointer init_ptr;
  gpointer exit_ptr;
  gpointer create_ptr;

  GModule *module;
  gchar *path;
  gboolean error = FALSE;

  if (g_path_is_absolute (name))
    path = g_strdup (name);
  else
    path = g_build_filename (dir, name, NULL);

  module = g_module_open (path, 0);

  if (!module)
    {
      g_fprintf (stderr, "Cannot load module %s: %s\n", path, g_module_error());
      error = TRUE;
    }

  if (module &&
      g_module_symbol (module, "im_module_list", &list_ptr) &&
      g_module_symbol (module, "im_module_init", &init_ptr) &&
      g_module_symbol (module, "im_module_exit", &exit_ptr) &&
      g_module_symbol (module, "im_module_create", &create_ptr))
    {
      const GtkIMContextInfo **contexts;
      guint n_contexts;
      int i;

      list = list_ptr;

      print_escaped (contents, path);
      g_string_append_c (contents, '\n');

      (*list) (&contexts, &n_contexts);

      for (i = 0; i < n_contexts; i++)
        {
          print_escaped (contents, contexts[i]->context_id);
          print_escaped (contents, contexts[i]->context_name);
          print_escaped (contents, contexts[i]->domain);
          print_escaped (contents, contexts[i]->domain_dirname);
          print_escaped (contents, contexts[i]->default_locales);
          g_string_append_c (contents, '\n');
        }
      g_string_append_c (contents, '\n');
    }
  else
    {
      g_fprintf (stderr, "%s does not export GTK+ IM module API: %s\n", path,
                 g_module_error ());
      error = TRUE;
    }

  g_free (path);
  if (module)
    g_module_close (module);

  return error;
}

int main (int argc, char **argv)
{
  char *cwd;
  int i;
  char *path;
  gboolean error = FALSE;
  gchar *cache_file = NULL;
  gint first_file = 1;
  GString *contents;

  if (argc > 1 && strcmp (argv[1], "--update-cache") == 0)
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      cache_file = gtk_rc_get_im_module_file ();
G_GNUC_END_IGNORE_DEPRECATIONS
      first_file = 2;
    }

  contents = g_string_new ("");
  g_string_append_printf (contents,
                          "# GTK+ Input Method Modules file\n"
                          "# Automatically generated file, do not edit\n"
                          "# Created by %s from gtk+-%d.%d.%d\n"
                          "#\n",
                          argv[0],
                          GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);

  if (argc == first_file)  /* No file arguments given */
    {
      char **dirs;
      GHashTable *dirs_done;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      path = gtk_rc_get_im_module_path ();
G_GNUC_END_IGNORE_DEPRECATIONS

      g_string_append_printf (contents, "# ModulesPath = %s\n#\n", path);

      dirs = gtk_split_file_list (path);
      dirs_done = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

      for (i = 0; dirs[i]; i++)
        if (!g_hash_table_lookup (dirs_done, dirs[i]))
          {
            GDir *dir = g_dir_open (dirs[i], 0, NULL);
            if (dir)
              {
                const char *dent;
                GList *list = NULL, *iterator = NULL;

                while ((dent = g_dir_read_name (dir)))
                  list = g_list_prepend (list, g_strdup (dent));

                list = g_list_sort (list, (GCompareFunc) strcmp);
                for (iterator = list; iterator; iterator = iterator->next)
                  {
                    if (g_str_has_suffix (iterator->data, SOEXT))
                      error |= query_module (dirs[i], iterator->data, contents);
                  }

                g_list_free_full (list, g_free);
                g_dir_close (dir);
              }

            g_hash_table_insert (dirs_done, dirs[i], GUINT_TO_POINTER (TRUE));
          }

      g_hash_table_destroy (dirs_done);
    }
  else
    {
      cwd = g_get_current_dir ();

      for (i = first_file; i < argc; i++)
        error |= query_module (cwd, argv[i], contents);

      g_free (cwd);
    }

  if (!error)
    {
      if (cache_file)
        {
          GError *err;

          err = NULL;
          if (!g_file_set_contents (cache_file, contents->str, -1, &err))
            {
                g_fprintf (stderr, "%s\n", err->message);
                error = 1;
            }
        }
      else
        g_print ("%s\n", contents->str);
    }

  return error ? 1 : 0;
}
