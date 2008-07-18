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
#include <glib/gprintf.h>
#include <gmodule.h>

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

#include "gtk/gtkrc.h"
#include "gtk/gtkimmodule.h"
#include "gtk/gtkversion.h"

static char *
escape_string (const char *str)
{
  GString *result = g_string_new (NULL);

  while (TRUE)
    {
      char c = *str++;
      
      switch (c)
	{
	case '\0':
	  goto done;
	case '\n':
	  g_string_append (result, "\\n");
	  break;
	case '\"':
	  g_string_append (result, "\\\"");
	  break;
#ifdef G_OS_WIN32
		/* Replace backslashes in path with forward slashes, so that
		 * it reads in without problems.
		 */
	case '\\':
	  g_string_append (result, "/");
	  break;
#endif	
	default:
	  g_string_append_c (result, c);
	}
    }

 done:
  return g_string_free (result, FALSE);
}

static void
print_escaped (const char *str)
{
  char *tmp = escape_string (str);
  g_printf ("\"%s\" ", tmp);
  g_free (tmp);
}

static gboolean
query_module (const char *dir, const char *name)
{
  void          (*list)   (const GtkIMContextInfo ***contexts,
 		           guint                    *n_contexts);
  void          (*init)   (GTypeModule              *type_module);
  void          (*exit)   (void);
  GtkIMContext *(*create) (const gchar             *context_id);

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
      init = init_ptr;
      exit = exit_ptr;
      create = create_ptr;

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

  g_printf ("# GTK+ Input Method Modules file\n"
	    "# Automatically generated file, do not edit\n"
	    "# Created by %s from gtk+-%d.%d.%d\n"
	    "#\n",
	    argv[0],
	    GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);


  if (argc == 1)		/* No arguments given */
    {
      char **dirs;
      int i;
      GHashTable *dirs_done;

      path = gtk_rc_get_im_module_path ();

      g_printf ("# ModulesPath = %s\n#\n", path);

      dirs = pango_split_file_list (path);
      dirs_done = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

      for (i=0; dirs[i]; i++)
        if (!g_hash_table_lookup (dirs_done, dirs[i]))
          {
	    GDir *dir = g_dir_open (dirs[i], 0, NULL);
	    if (dir)
	      {
	        const char *dent;

	        while ((dent = g_dir_read_name (dir)))
	          {
	            if (g_str_has_suffix (dent, SOEXT))
	              error |= query_module (dirs[i], dent);
	          }
	        
	        g_dir_close (dir);
	      }

            g_hash_table_insert (dirs_done, dirs[i], GUINT_TO_POINTER (TRUE));
          }

      g_hash_table_destroy (dirs_done);
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
