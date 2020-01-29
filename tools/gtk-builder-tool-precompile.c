/*  Copyright 2020 Red Hat, Inc.
 *
 * GTK+ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK+; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#define GTK_COMPILATION
#include "../gtk/gtkbuilderprecompile.c"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "gtk-builder-tool.h"

static void
precompile_file (const char *filename)
{
  char *data;
  gsize len;
  GError *error = NULL;
  char *outfile;
  GBytes *bytes;

  if (!g_file_get_contents (filename, &data, &len, &error))
    {
      g_warning ("Failed to load '%s': %s", filename, error->message);
      exit (1);
    }

  bytes = _gtk_buildable_parser_precompile (data, len, &error);
  if (bytes == NULL)
    {
      g_warning ("Failed to precompile '%s': %s", filename, error->message);
      exit (1);
    }

  outfile = g_strconcat (filename, ".precompiled", NULL);
  if (!g_file_set_contents (outfile, g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), &error))
  if (bytes == NULL)
    {
      g_warning ("Failed to write precompiled data to '%s': %s", outfile, error->message);
      exit (1);
    }

  g_print ("Wrote %ld bytes to %s.\n", g_bytes_get_size (bytes), outfile);

  g_free (outfile);
  g_bytes_unref (bytes);
}

void
do_precompile (int          *argc,
               const char ***argv)
{
  GOptionContext *context;
  char **filenames = NULL;
  const GOptionEntry entries[] = {
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, NULL },
    { NULL, }
  };
  GError *error = NULL;
  int i;

  context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (filenames == NULL)
    {
      g_printerr ("No .ui file specified\n");
      exit (1);
    }

  for (i = 0; filenames[i]; i++)
    precompile_file (filenames[i]);

  g_strfreev (filenames);
}
