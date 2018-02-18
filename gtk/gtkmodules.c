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

#include "gtkmodules.h"
#include "gtksettings.h"
#include "gtkdebug.h"
#include "gtkprivate.h"
#include "gtkmodulesprivate.h"
#include "gtkintl.h"
#include "gtkutilsprivate.h"

#include <gmodule.h>

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

  result = gtk_split_file_list (module_path);
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

/* Like g_module_path, but use .la as the suffix
 */
static gchar*
module_build_la_path (const gchar *directory,
		      const gchar *module_name)
{
  gchar *filename;
  gchar *result;
	
  if (strncmp (module_name, "lib", 3) == 0)
    filename = (gchar *)module_name;
  else
    filename =  g_strconcat ("lib", module_name, ".la", NULL);

  if (directory && *directory)
    result = g_build_filename (directory, filename, NULL);
  else
    result = g_strdup (filename);

  if (filename != module_name)
    g_free (filename);

  return result;
}

/**
 * _gtk_find_module:
 * @name: the name of the module
 * @type: the type of the module, for instance 'modules', 'engines', immodules'
 * 
 * Looks for a dynamically module named @name of type @type in the standard GTK+
 *  module search path.
 * 
 * Returns: the pathname to the found module, or %NULL if it wasn’t found.
 *  Free with g_free().
 **/
gchar *
_gtk_find_module (const gchar *name,
		  const gchar *type)
{
  gchar **paths;
  gchar **path;
  gchar *module_name = NULL;

  if (g_path_is_absolute (name))
    return g_strdup (name);

  paths = _gtk_get_module_path (type);
  for (path = paths; *path; path++)
    {
      gchar *tmp_name;

      tmp_name = g_module_build_path (*path, name);
      if (g_file_test (tmp_name, G_FILE_TEST_EXISTS))
	{
	  module_name = tmp_name;
	  goto found;
	}
      g_free(tmp_name);

      tmp_name = module_build_la_path (*path, name);
      if (g_file_test (tmp_name, G_FILE_TEST_EXISTS))
	{
	  module_name = tmp_name;
	  goto found;
	}
      g_free(tmp_name);
    }

 found:
  g_strfreev (paths);
  return module_name;
}

/* Return TRUE if module_to_check causes version conflicts.
 * If module_to_check is NULL, check the main module.
 */
gboolean
_gtk_module_has_mixed_deps (GModule *module_to_check)
{
  GModule *module;
  gpointer func;
  gboolean result;

  if (!module_to_check)
    module = g_module_open (NULL, 0);
  else
    module = module_to_check;

  if (g_module_symbol (module, "gtk_progress_get_type", &func))
    result = TRUE;
  else if (g_module_symbol (module, "gtk_misc_get_type", &func))
    result = TRUE;
  else
    result = FALSE;

  if (!module_to_check)
    g_module_close (module);

  return result;
}
