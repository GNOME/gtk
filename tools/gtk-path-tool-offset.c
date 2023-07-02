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
do_offset (int *argc, const char ***argv)
{
  GError *error = NULL;
  double distance = 0;
  const char *join = "miter";
  double miter_limit = 4;
  char **args = NULL;
  GOptionContext *context;
  GOptionEntry entries[] = {
    { "distance", 0, 0, G_OPTION_ARG_DOUBLE, &distance, N_("Offset to apply (positive or negative number)"), N_("VALUE") },
    { "line-join", 0, 0, G_OPTION_ARG_STRING, &join, N_("Line join (miter, miter-clip, round, bevel, arcs)"), N_("VALUE") },
    { "miter-limit", 0, 0, G_OPTION_ARG_DOUBLE, &miter_limit, N_("Miter limit (number"), N_("VALUE") },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &args, NULL, N_("PATH") },
    { NULL, },
  };
  GskPath *path, *result;
  GskLineJoin line_join;

  g_set_prgname ("gtk4-path-tool offset");

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Offset a path."));

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

  line_join = get_enum_value (GSK_TYPE_LINE_JOIN, _("line join"), join);

  result = gsk_path_offset (path, distance, line_join, miter_limit);

  if (result)
    {
      char *str = gsk_path_to_string (result);
      g_print ("%s\n", str);
      g_free (str);
    }
  else
    {
      g_printerr ("%s\n", _("That didn't work out."));
      exit (1);
    }
}
