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

typedef struct
{
  gsize start;
  gsize end;

  gsize cur;
} CutData;

static gboolean
node_is_leaf (GskRenderNode *node)
{
  gsize n_children;

  gsk_render_node_get_children (node, &n_children);

  return n_children == 0;
}

static GskRenderNode *
cut_node (GskRenderReplay *replay,
          GskRenderNode   *node,
          gpointer         user_data)
{
  CutData *data = user_data;
  gboolean skip;

  if (!node_is_leaf (node))
    return gsk_render_replay_default (replay, node);

  skip = data->cur < data->start ||
         data->cur >= data->end;
  data->cur++;

  if (skip)
    return NULL;
  else
    return gsk_render_replay_default (replay, node);
}

GskRenderNode *
filter_cut (GskRenderNode  *node,
            int             argc,
            const char    **argv)
{
  CutData data = {
    .end = G_MAXSIZE,
  };
  const GOptionEntry entries[] = {
    { NULL, }
  };
  GOptionContext *context;
  GskRenderReplay *replay;
  GskRenderNode *result;
  GError *error = NULL;
  char *s;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Cut out a part of the node"));

  if (!g_option_context_parse (context, &argc, (char ***) &argv, &error))
    {
      g_printerr ("cut: %s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  switch (argc)
  {
    case 3:
      data.end = strtoul (argv[2], &s, 0);
      if (s == argv[2] || *s != 0)
        {
          g_printerr ("cut: Invalid value for end: %s\n", argv[2]);
          exit (1);
        }
      G_GNUC_FALLTHROUGH;

    case 2:
      data.start = strtoul (argv[1], &s, 0);
      if (s == argv[1] || *s != 0)
        {
          g_printerr ("cut: Invalid value for end: %s\n", argv[2]);
          exit (1);
        }
      break;

    default:
      g_printerr ("Usage: cut START [END]\n");
      exit (1);
    }

  replay = gsk_render_replay_new ();
  gsk_render_replay_set_node_filter (replay,
                                     cut_node,
                                     &data,
                                     NULL);
  result = gsk_render_replay_filter_node (replay, node);
  gsk_render_replay_free (replay);

  gsk_render_node_unref (node);

  return result;
}
