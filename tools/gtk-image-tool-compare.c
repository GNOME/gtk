/*  Copyright 2024 Red Hat, Inc.
 *
 * GTK is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GTK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include "gtk-image-tool.h"

#include "testsuite/reftests/reftest-compare.h"

void
do_compare (int          *argc,
            const char ***argv)
{
  GOptionContext *context;
  gboolean opt_quiet = FALSE;
  char **filenames = NULL;
  char *opt_filename = NULL;
  const GOptionEntry entries[] = {
    { "output", 'o', 0, G_OPTION_ARG_FILENAME, &opt_filename, N_("Output file"), N_("FILE") },
    { "quiet", 'q', 0, G_OPTION_ARG_NONE, &opt_quiet, "Don't talk", NULL },

    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE…") },
    { NULL, }
  };
  GdkTexture *texture[2];
  GdkTexture *diff;
  GError *error = NULL;

  g_set_prgname ("gtk4-image-tool compare");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Compare two images"));

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (filenames == NULL)
    {
      g_printerr (_("No image file specified\n"));
      exit (1);
    }

  if (g_strv_length (filenames) != 2)
    {
      g_printerr (_("Can only accept two image files\n"));
      exit (1);
    }

  for (int i = 0; i < 2; i++)
    {
      texture[i] = gdk_texture_new_from_filename (filenames[i], &error);
      if (texture[i] == NULL)
        {
          g_printerr (_("Failed to load %s: %s\n"), filenames[i], error->message);
          exit (1);
        }
    }

  diff = reftest_compare_textures (texture[0], texture[1]);

  if (opt_filename && diff)
    {
      if (!gdk_texture_save_to_png (diff, opt_filename))
        {
          g_printerr (_("Could not save diff image to %s\n"), opt_filename);
          exit (1);
        }
    }

  if (!opt_quiet)
    {
      if (diff)
        {
          if (opt_filename)
            g_print (_("Differences witten to %s.\n"), opt_filename);
          else
            g_print (_("The images are different.\n"));
        }
      else
        g_print (_("No differences.\n"));
    }

  if (diff)
    exit (1);

  g_strfreev (filenames);
  g_free (opt_filename);
  g_object_unref (texture[0]);
  g_object_unref (texture[1]);
}
