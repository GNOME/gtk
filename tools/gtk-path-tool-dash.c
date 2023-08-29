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

static gboolean
collect_path (GskPathOperation        op,
              const graphene_point_t *pts,
              gsize                   n_pts,
              float                   weight,
              gpointer                user_data)
{
  GskPathBuilder *builder = user_data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      gsk_path_builder_move_to (builder, pts[0].x, pts[0].y);
      break;

    case GSK_PATH_CLOSE:
      gsk_path_builder_close (builder);
      break;

    case GSK_PATH_LINE:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      break;

    case GSK_PATH_QUAD:
      gsk_path_builder_quad_to (builder, pts[1].x, pts[1].y,
                                         pts[2].x, pts[2].y);
      break;

    case GSK_PATH_CUBIC:
      gsk_path_builder_cubic_to (builder, pts[1].x, pts[1].y,
                                          pts[2].x, pts[2].y,
                                          pts[3].x, pts[3].y);
      break;

    case GSK_PATH_CONIC:
      gsk_path_builder_conic_to (builder, pts[1].x, pts[1].y,
                                          pts[2].x, pts[2].y,
                                          weight);
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

void
do_dash (int *argc, const char ***argv)
{
  GError *error = NULL;
  char **args = NULL;
  const char *dashes = NULL;
  double dash_offset = 0;
  GOptionContext *context;
  GOptionEntry entries[] = {
    { "dashes", 0, 0, G_OPTION_ARG_STRING, &dashes, N_("Dash pattern (comma-separated numbers)"), N_("VALUE") },
    { "dash-offset", 0, 0, G_OPTION_ARG_DOUBLE, &dash_offset, N_("Dash offset (number)"), N_("VALUE") },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &args, NULL, N_("PATH") },
    { NULL, },
  };
  GskPath *path, *result;
  GskPathBuilder *builder;
  GskStroke *stroke;

  g_set_prgname ("gtk4-path-tool dash");

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Dash a path."));

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

  stroke = gsk_stroke_new (1);

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

  builder = gsk_path_builder_new ();

  gsk_path_dash (path, stroke, collect_path, builder);

  result = gsk_path_builder_free_to_path (builder);

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
