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
#include "gtk-rendernode-tool.h"

static GskRenderNode *
strip_node (GskRenderReplay *replay,
            GskRenderNode   *node,
            gpointer         user_data)
{
  switch ((guint) gsk_render_node_get_node_type (node))
  {
    case GSK_DEBUG_NODE:
      return gsk_render_replay_filter_node (replay, gsk_debug_node_get_child (node));

    default:
      return gsk_render_replay_default (replay, node);
  }
}

GskRenderNode *
filter_strip (GskRenderNode  *node,
              int             argc,
              const char    **argv)
{
  const GOptionEntry entries[] = {
    { NULL, }
  };
  GOptionContext *context;
  GskRenderReplay *replay;
  GskRenderNode *result;
  GError *error = NULL;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Strip debug nodes"));

  if (!g_option_context_parse (context, &argc, (char ***) &argv, &error))
    {
      g_printerr ("strip: %s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  if (argc != 1)
    {
      g_printerr ("strip: Unexpected arguments\n");
      exit (1);
    }

  replay = gsk_render_replay_new ();
  gsk_render_replay_set_node_filter (replay,
                                     strip_node,
                                     NULL,
                                     NULL);
  result = gsk_render_replay_filter_node (replay, node);
  gsk_render_replay_free (replay);

  gsk_render_node_unref (node);

  return result;
}
