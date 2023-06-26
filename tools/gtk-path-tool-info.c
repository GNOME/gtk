/*  Copyright 2023 Red Hat, Inc.
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

#include "config.h"

#include <gtk/gtk.h>
#include "gtk-path-tool.h"

#include <glib/gi18n-lib.h>

void
do_info (int *argc, const char ***argv)
{
  GError *error = NULL;
  char **args = NULL;
  GOptionContext *context;
  GOptionEntry entries[] = {
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &args, NULL, N_("PATH") },
    { NULL, },
  };
  GskPath *path;
  GskPathMeasure *measure;
  graphene_rect_t bounds;

  g_set_prgname ("gtk4-path-tool info");

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Print information about a path."));

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (args == NULL)
    {
      g_printerr ("%s\n", _("No paths given."));
      exit (1);
    }

  path = get_path (args[0]);
  measure = gsk_path_measure_new (path);

  if (gsk_path_is_empty (path))
    g_print ("%s\n", _("Path is empty."));
  else
    {
      if (gsk_path_measure_is_closed (measure))
        g_print ("%s\n", _("Path is closed"));

      if (gsk_path_is_convex (path))
        g_print ("%s\n", _("Path is convex"));

      g_print ("%s %g\n", _("Path length"), gsk_path_measure_get_length (measure));

      if (gsk_path_get_bounds (path, &bounds))
        g_print ("%s: %g %g %g %g\n", _("Bounds"),
                 bounds.origin.x, bounds.origin.y,
                 bounds.size.width, bounds.size.height);
    }
}
