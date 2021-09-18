/*  Copyright 2015 Red Hat, Inc.
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include "gtkbuilderprivate.h"
#include "gtk-builder-tool.h"


static void
compile_file (const char *input,
              const char *output)
{
  GtkBuilder *builder;
  char *text;
  gsize len;
  GError *error = NULL;
  GBytes *bytes;

  if (!g_file_get_contents (input, &text, &len, &error))
    {
      g_printerr ("%s\n", error->message);
      exit (1);
    }

  builder = gtk_builder_new ();
  bytes = _gtk_buildable_parser_precompile (text, len, &error);
  if (!bytes)
    {
      g_printerr ("%s\n", error->message);
      exit (1);
    }

  if (!g_file_set_contents (output,
                            g_bytes_get_data (bytes, NULL),
                            g_bytes_get_size (bytes),
                            &error))
    {
      g_printerr ("%s\n", error->message);
      exit (1);
    }

  g_bytes_unref (bytes);
  g_object_unref (builder);
  g_free (text);
}

void
do_compile (int          *argc,
            const char ***argv)
{
  GOptionContext *context;
  char **filenames = NULL;
  const GOptionEntry entries[] = {
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, NULL },
    { NULL, }
  };
  GError *error = NULL;

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

  if (g_strv_length (filenames) != 2)
    {
      g_printerr ("Need to specify an output file\n");
      exit (1);
    }

  compile_file (filenames[0], filenames[1]);

  g_strfreev (filenames);
}
