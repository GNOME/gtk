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

#include "rntfilter.h"

#include <glib/gi18n-lib.h>

struct _RntFilter
{
  const char *name;
  const char *description;
  GskRenderNode * (* run) (GskRenderNode *node,
                           RntContext    *ctx,
                           int            argc,
                           const char   **argv);
};

static const RntFilter filters[] = {
  {
    .name = "background-blur",
    .description = "Show where background blur would take effect",
    .run = filter_background_blur,
  },
  {
    .name = "benchmark",
    .description = "Show where background blur would take effect",
    .run = filter_benchmark,
  },
  {
    .name = "compare",
    .description = "Compare rendering to .node or .png files.",
    .run = filter_compare,
  },
  {
    .name = "copypaste",
    .description = "Replace copy/paste nodes with copies of nodes",
    .run = filter_copypaste,
  },
  {
    .name = "cut",
    .description = "Cut out a part of the rendernode",
    .run = filter_cut,
  },
  {
    .name = "diff",
    .description = "Show where two nodes differ",
    .run = filter_diff,
  },
  {
    .name = "extract",
    .description = "Extract textures and glyphs",
    .run = filter_extract,
  },
  {
    .name = "info",
    .description = "Provide information about the render node.",
    .run = filter_info,
  },
  {
    .name = "load-icon",
    .description = "Load icons from filenames",
    .run = filter_load_icon,
  },
  {
    .name = "load-svg",
    .description = "Load from SVGs",
    .run = filter_load_icon,
  },
  {
    .name = "match",
    .description = "Match patterns in the render node.",
    .run = filter_match,
  },
  {
    .name = "merge",
    .description = "Merge the stack of nodes into one",
    .run = filter_merge,
  },
  {
    .name = "opaque",
    .description = "Show the opaque part",
    .run = filter_opaque,
  },
  {
    .name = "print",
    .description = "Print the node",
    .run = filter_print,
  },
  {
    .name = "render",
    .description = "Render to an image.",
    .run = filter_render,
  },
  {
    .name = "save",
    .description = "Save current node to file",
    .run = filter_save,
  },
  {
    .name = "show",
    .description = "Show the render node",
    .run = filter_show,
  },
  {
    .name = "simplify",
    .description = "Simplify the node by keeping it functionally identical",
    .run = filter_simplify,
  },
  {
    .name = "strip",
    .description = "Strip debug nodes (and others)",
    .run = filter_strip,
  },
  {
    .name = "texture",
    .description = "Convert textures",
    .run = filter_texture,
  },
};

gsize
rnt_filter_n_filters(void)
{
  return G_N_ELEMENTS (filters);
}

const RntFilter *
rnt_filter_get (gsize i)
{
  g_assert (i < G_N_ELEMENTS (filters));

  return &filters[i];
}

const RntFilter *
rnt_filter_find (const char *name)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (filters); i++)
    {
      if (g_str_equal (name, filters[i].name))
        return &filters[i];
    }

  return NULL;
}

const char *
rnt_filter_get_name (const RntFilter *filter)
{
  return filter->name;
}

const char *
rnt_filter_get_description (const RntFilter *filter)
{
  return filter->name;
}

GskRenderNode *
rnt_filter_run (const RntFilter  *filter,
                GskRenderNode    *node,
                RntContext       *ctx,
                int               argc,
                const char      **argv)
{
  return filter->run (node, ctx, argc, argv);
}

/**
 * rnt_filter_argv:
 * @argc: number of arguments
 * @argv: (array length=argc): the arguments
 *
 * Parses an argc/argv argument combo looking for
 * where the next filter begins.
 *
 * It collects all arguments for the current filter and then
 * return them.
 *
 * No check is done that `argv[0]` references a filter.
 *
 * Returns: (nullable): the argv ntil the enxt filter or
 *   NULL on error
 **/
char **
rnt_filter_argv (int          argc,
                 const char **argv)
{
  GStrvBuilder *builder;
  gsize i;

  if (argc == 0)
    return NULL;

  builder = g_strv_builder_new ();
  g_strv_builder_add (builder, argv[0]);

  for (i = 1; i < argc; i++)
    {
      if (rnt_filter_find (argv[i]))
        break;

      g_strv_builder_add (builder, argv[i]);
    }

  return g_strv_builder_end (builder);
}

