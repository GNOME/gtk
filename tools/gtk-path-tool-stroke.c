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
do_stroke (int *argc, const char ***argv)
{
  GError *error = NULL;
  double line_width = 1;
  const char *cap = "butt";
  const char *join = "miter";
  double miter_limit = 4;
  const char *dashes = NULL;
  double dash_offset = 0;
  char **args = NULL;
  GOptionContext *context;
  GOptionEntry entries[] = {
    { "line-width", 0, 0, G_OPTION_ARG_DOUBLE, &line_width, N_("Line width (number)"), N_("VALUE") },
    { "line-cap", 0, 0, G_OPTION_ARG_STRING, &cap, N_("Line cap (butt, round, square)"), N_("VALUE") },
    { "line-join", 0, 0, G_OPTION_ARG_STRING, &join, N_("Line join (miter, miter-clip, round, bevel, arcs)"), N_("VALUE") },
    { "miter-limit", 0, 0, G_OPTION_ARG_DOUBLE, &miter_limit, N_("Miter limit (number)"), N_("VALUE") },
    { "dashes", 0, 0, G_OPTION_ARG_STRING, &dashes, N_("Dash pattern (comma-separated numbers)"), N_("VALUE") },
    { "dash-offset", 0, 0, G_OPTION_ARG_DOUBLE, &dash_offset, N_("Dash offset (number)"), N_("VALUE") },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &args, NULL, N_("PATH") },
    { NULL, },
  };
  GskPath *path, *result;
  GskLineCap line_cap;
  GskLineJoin line_join;
  GskStroke *stroke;

  g_set_prgname ("gtk4-path-tool stroke");

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Stroke a path."));

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (args == NULL)
    {
      g_printerr (_("No paths given.\n"));
      exit (1);
    }

  path = get_path (args[0]);

  line_cap = get_enum_value (GSK_TYPE_LINE_CAP, _("line cap"), cap);
  line_join = get_enum_value (GSK_TYPE_LINE_JOIN, _("line join"), join);

  stroke = gsk_stroke_new (line_width);
  gsk_stroke_set_line_cap (stroke, line_cap);
  gsk_stroke_set_line_join (stroke, line_join);

  gsk_stroke_set_miter_limit (stroke, miter_limit);

  if (dashes != NULL)
    {
      GArray *d = g_array_new (FALSE, FALSE, sizeof (float));
      char **strings;

      strings = g_strsplit (dashes, ",", 0);

      for (unsigned int i = 0; strings[i]; i++)
        {
          char *end = NULL;
          float f;

          f = (float) g_ascii_strtod (strings[i], &end);

          if (*end != '\0')
            {
              char *msg = g_strdup_printf (_("Failed to parse '%s' as number"), strings[i]);
              g_printerr ("%s\n", msg);
              exit (1);
            }

          g_array_append_val (d, f);
        }

      g_strfreev (strings);

      gsk_stroke_set_dash (stroke, (const float *)d->data, d->len);

      g_array_unref (d);
    }

  gsk_stroke_set_dash_offset (stroke, dash_offset);

  result = gsk_path_stroke (path, stroke);
  gsk_stroke_free (stroke);

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
