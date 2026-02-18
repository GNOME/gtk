/*  Copyright 2023 Red Hat, Inc.
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
#include "gtk-rendernode-tool.h"
#include "gtk-tool-utils.h"

#define N_NODE_TYPES (GSK_DISPLACEMENT_NODE + 1)

typedef struct {
  unsigned int counts[N_NODE_TYPES];
  guint max_depth;
  guint cur_depth;
} NodeCount;

static void
count_nodes (GskRenderNode *node,
             NodeCount     *count)
{
  GskRenderNode **children;
  gsize i, n_children;

  g_assert (gsk_render_node_get_node_type (node) < N_NODE_TYPES);

  count->counts[gsk_render_node_get_node_type (node)] += 1;
  count->cur_depth++;
  count->max_depth = MAX (count->cur_depth, count->max_depth);
  children = gsk_render_node_get_children (node, &n_children);
  for (i = 0; i < n_children; i++)
    count_nodes (children[i], count);
  count->cur_depth--;
}

static void
file_info (const char *filename)
{
  GskRenderNode *node;
  NodeCount count = { { 0, } };
  unsigned int total = 0;
  unsigned int namelen = 0;
  graphene_rect_t bounds, opaque;

  node = load_node_file (filename);

  count_nodes (node, &count);

  for (unsigned int i = 0; i < G_N_ELEMENTS (count.counts); i++)
    {
      total += count.counts[i];
      if (count.counts[i] > 0)
        namelen = MAX (namelen, strlen (get_node_name (i)));
    }

  namelen = MAX (namelen, strlen (_("Number of nodes:")));

  g_print ("%*s %u\n", namelen, _("Number of nodes:"), total);

  int digits = 0;
  while (pow (10, digits) < total)
    digits++;

  for (unsigned int i = 0; i < G_N_ELEMENTS (count.counts); i++)
    {
      if (count.counts[i] > 0)
        g_print ("%*s: %*u\n", namelen - 1, get_node_name (i), digits, count.counts[i]);
    }

  g_print ("%s %u\n", _("Depth:"), count.max_depth);

  gsk_render_node_get_bounds (node, &bounds);
  g_print ("%s %g x %g\n", _("Bounds:"), bounds.size.width, bounds.size.height);
  g_print ("%s %g %g\n", _("Origin:"), bounds.origin.x, bounds.origin.y);
  if (gsk_render_node_get_opaque_rect (node, &opaque))
    {
      g_print ("%s %g %g, %g x %g (%.0f%%)\n",
               _("Opaque part:"),
               opaque.origin.x, opaque.origin.y,
               opaque.size.width, opaque.size.height,
               100 * (opaque.size.width * opaque.size.height) / (bounds.size.width * bounds.size.height));
    }
  else
    g_print ("%s none\n", _("Opaque part:"));

  gsk_render_node_unref (node);
}

void
do_info (int          *argc,
         const char ***argv)
{
  GOptionContext *context;
  char **filenames = NULL;
  const GOptionEntry entries[] = {
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE") },
    { NULL, }
  };
  GError *error = NULL;

  g_set_prgname ("gtk4-rendernode-tool info");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Provide information about the render node."));

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (filenames == NULL)
    {
      g_printerr (_("No .node file specified\n"));
      exit (1);
    }

  if (g_strv_length (filenames) > 1)
    {
      g_printerr (_("Can only accept a single .node file\n"));
      exit (1);
    }

  file_info (filenames[0]);

  g_strfreev (filenames);
}
