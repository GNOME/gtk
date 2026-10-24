/*  Copyright 2026 Benjamin Otte
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
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "rntcontext.h"
#include "rntfilter.h"

GskRenderNode *
filter_save (GskRenderNode *node,
             RntContext    *ctx,
             int            argc,
             const char   **argv)
{
  char **filenames = NULL;
  const GOptionEntry entries[] = {
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE") },
    { NULL, }
  };
  GOptionContext *context;
  GError *error = NULL;
  gsize i;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Save the node to a file"));

  if (!g_option_context_parse (context, &argc, (char ***) &argv, &error))
    {
      rnt_context_error (ctx, "%s\n", error->message);
      g_error_free (error);
      return NULL;
    }

  if (filenames == NULL)
    {
      rnt_context_error (ctx, "Expected a filename\n");
      return NULL;
    }

  if (!gsk_render_node_write_to_file (node, filenames[0], &error))
    {
      rnt_context_error (ctx, _("Failed to save file: %s\n"), error->message);
      return NULL;
    }
  for (i = 1; i < g_strv_length (filenames); i++)
    {
      GskRenderNode *tmp = rnt_context_get_node (ctx, i - 1);
      if (tmp == NULL)
        {
          rnt_context_error (ctx, _("%u filenames given but only %zu nodes available\n"),
                             g_strv_length (filenames), i);
          return NULL;
        }
      if (!gsk_render_node_write_to_file (tmp, filenames[i], &error))
        {
          rnt_context_error (ctx, _("Failed to save file: %s\n"), error->message);
          return NULL;
        }
    }

  return gsk_render_node_ref (node);
}
