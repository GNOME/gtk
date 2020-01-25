/* GTK - The GIMP Toolkit
 * Copyright 1998-2002 Tim Janik, Red Hat, Inc., and others.
 * Copyright (C) 2003 Alex Graveley
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

#include <string.h>

#include "gtksettings.h"
#include "gtkdebug.h"
#include "gtkprivate.h"
#include "gtkmodulesprivate.h"
#include "gtkintl.h"

#include <gmodule.h>

static char *
gtk_trim_string (const char *str)
{
  int len;

  g_return_val_if_fail (str != NULL, NULL);

  while (*str && g_ascii_isspace (*str))
    str++;

  len = strlen (str);
  while (len > 0 && g_ascii_isspace (str[len - 1]))
    len--;

  return g_strndup (str, len);
}

static char **
split_file_list (const char *str)
{
  int i = 0;
  int j;
  char **files;

  files = g_strsplit (str, G_SEARCHPATH_SEPARATOR_S, -1);

  while (files[i])
    {
      char *file = gtk_trim_string (files[i]);

      /* If the resulting file is empty, skip it */
      if (file[0] == '\0')
        {
          g_free (file);
          g_free (files[i]);

          for (j = i + 1; files[j]; j++)
            files[j - 1] = files[j];

          files[j - 1] = NULL;

          continue;
        }

#ifndef G_OS_WIN32
      /* '~' is a quite normal and common character in file names on
       * Windows, especially in the 8.3 versions of long file names, which
       * still occur now and then. Also, few Windows user are aware of the
       * Unix shell convention that '~' stands for the home directory,
       * even if they happen to have a home directory.
       */
      if (file[0] == '~' && file[1] == G_DIR_SEPARATOR)
        {
          char *tmp = g_strconcat (g_get_home_dir(), file + 1, NULL);
          g_free (file);
          file = tmp;
        }
      else if (file[0] == '~' && file[1] == '\0')
        {
          g_free (file);
          file = g_strdup (g_get_home_dir ());
        }
#endif

      g_free (files[i]);
      files[i] = file;

      i++;
    }

  return files;
}

static gchar **
get_module_path (void)
{
  const gchar *module_path_env;
  const gchar *exe_prefix;
  gchar *module_path;
  gchar *default_dir;
  static gchar **result = NULL;

  if (result)
    return result;

  module_path_env = g_getenv ("GTK_PATH");
  exe_prefix = g_getenv ("GTK_EXE_PREFIX");

  if (exe_prefix)
    default_dir = g_build_filename (exe_prefix, "lib", "gtk-4.0", NULL);
  else
    default_dir = g_build_filename (_gtk_get_libdir (), "gtk-4.0", NULL);

  if (module_path_env)
    module_path = g_build_path (G_SEARCHPATH_SEPARATOR_S,
				module_path_env, default_dir, NULL);
  else
    module_path = g_build_path (G_SEARCHPATH_SEPARATOR_S,
				default_dir, NULL);

  g_free (default_dir);

  result = split_file_list (module_path);
  g_free (module_path);

  return result;
}

/**
 * _gtk_get_module_path:
 * @type: the type of the module, for instance 'modules', 'engines', immodules'
 * 
 * Determines the search path for a particular type of module.
 * 
 * Returns: the search path for the module type. Free with g_strfreev().
 **/
gchar **
_gtk_get_module_path (const gchar *type)
{
  gchar **paths = get_module_path();
  gchar **path;
  gchar **result;
  gint count = 0;

  for (path = paths; *path; path++)
    count++;

  result = g_new (gchar *, count * 4 + 1);

  count = 0;
  for (path = get_module_path (); *path; path++)
    {
      gint use_version, use_host;
      
      for (use_version = TRUE; use_version >= FALSE; use_version--)
	for (use_host = TRUE; use_host >= FALSE; use_host--)
	  {
	    gchar *tmp_dir;
	    
	    if (use_version && use_host)
	      tmp_dir = g_build_filename (*path, GTK_BINARY_VERSION, GTK_HOST, type, NULL);
	    else if (use_version)
	      tmp_dir = g_build_filename (*path, GTK_BINARY_VERSION, type, NULL);
	    else if (use_host)
	      tmp_dir = g_build_filename (*path, GTK_HOST, type, NULL);
	    else
	      tmp_dir = g_build_filename (*path, type, NULL);

	    result[count++] = tmp_dir;
	  }
    }

  result[count++] = NULL;

  return result;
}
