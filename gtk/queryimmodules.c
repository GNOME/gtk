/* GTK+
 * querymodules.c:
 *
 * Copyright (C) 2000 Red Hat Software
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib.h>
#include <gmodule.h>

#include <errno.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>

#ifdef G_OS_WIN32
#define SOEXT ".dll"
#else
#define SOEXT ".so"
#endif

#include <pango/pango-utils.h>
#include "gtk/gtkrc.h"
#include "gtk/gtkimmodule.h"

void
print_escaped (const char *str)
{
  char *tmp = g_strescape (str, NULL);
  printf ("\"%s\" ", tmp);
  g_free (tmp);
}

gboolean
query_module (const char *dir, const char *name)
{
  void          (*list)   (const GtkIMContextInfo ***contexts,
 		           guint                    *n_contexts);
  void          (*init)   (GTypeModule              *type_module);
  void          (*exit)   (void);
  GtkIMContext *(*create) (const gchar             *context_id);

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
      fprintf(stderr, "Cannot load module %s: %s\n", path, g_module_error());
      error = TRUE;
    }
	  
  if (module &&
      g_module_symbol (module, "im_module_list", (gpointer)&list) &&
      g_module_symbol (module, "im_module_init", (gpointer)&init) &&
      g_module_symbol (module, "im_module_exit", (gpointer)&exit) &&
      g_module_symbol (module, "im_module_create", (gpointer)&create))
    {
      const GtkIMContextInfo **contexts;
      guint n_contexts;
      int i;

      print_escaped (path);
      fputs ("\n", stdout);

      (*list) (&contexts, &n_contexts);

      for (i=0; i<n_contexts; i++)
	{
	  print_escaped (contexts[i]->context_id);
	  print_escaped (contexts[i]->context_name);
	  print_escaped (contexts[i]->domain);
	  print_escaped (contexts[i]->domain_dirname);
	  print_escaped (contexts[i]->default_locales);
	  fputs ("\n", stdout);
	}
      fputs ("\n", stdout);
    }
  else
    {
      fprintf (stderr, "%s does not export GTK+ IM module API: %s\n", path,
	       g_module_error());
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

  printf ("# GTK+ Input Method Modules file\n"
	  "# Automatically generated file, do not edit\n"
	  "#\n");

  if (argc == 1)		/* No arguments given */
    {
      char **dirs;
      int i;

      path = gtk_rc_get_im_module_path ();

      printf ("# ModulesPath = %s\n#\n", path);

      dirs = pango_split_file_list (path);

      for (i=0; dirs[i]; i++)
	{
	  GDir *dir = g_dir_open (dirs[i], 0, NULL);
	  if (dir)
	    {
	      char *dent;

	      while ((dent = g_dir_read_name (dir)))
		{
		  int len = strlen (dent);
		  if (len > 3 && strcmp (dent + len - strlen (SOEXT), SOEXT) == 0)
		    error |= query_module (dirs[i], dent);
		}
	      
	      g_dir_close (dir);
	    }
	}
    }
  else
    {
      cwd = g_get_current_dir ();
      
      for (i=1; i<argc; i++)
	error |= query_module (cwd, argv[i]);

      g_free (cwd);
    }
  
  return error ? 1 : 0;
}
