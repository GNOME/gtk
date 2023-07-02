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
do_pathop (const char *op, int *argc, const char ***argv)
{
  GError *error = NULL;
  const char *fill = "winding";
  char **args = NULL;
  GOptionContext *context;
  GOptionEntry entries[] = {
    { "fill-rule", 0, 0, G_OPTION_ARG_STRING, &fill, N_("Fill rule"), N_("RULE") },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &args, NULL, N_("PATH…") },
    { NULL, },
  };
  GskPath *path1, *path2, *result;
  GskFillRule fill_rule;
  char *prgname;
  char *summary;

  prgname = g_strconcat ("gtk4-path-tool ", op, NULL);
  summary = g_strdup_printf (_("Apply the %s path operation."), op);
  g_set_prgname (prgname);

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, summary);

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

  path1 = get_path (args[0]);
  if (args[1] != NULL)
    path2 = get_path (args[1]);
  else
    path2 = NULL;

  fill_rule = get_enum_value (GSK_TYPE_FILL_RULE, _("fill rule"), fill);

  if (strcmp (op, "simplify") == 0)
    result = gsk_path_simplify (path1, fill_rule);
  else if (strcmp (op, "union") == 0)
    result = gsk_path_union (path1, path2, fill_rule);
  else if (strcmp (op, "intersection") == 0)
    result = gsk_path_intersection (path1, path2, fill_rule);
  else if (strcmp (op, "difference") == 0)
    result = gsk_path_difference (path1, path2, fill_rule);
  else if (strcmp (op, "symmetric-difference") == 0)
    result = gsk_path_symmetric_difference (path1, path2, fill_rule);
  else
    {
      char *msg = g_strdup_printf (_("'%s' is not a supported operation."), op);
      g_printerr ("%s\n", msg);
      exit (1);
    }

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
