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
#include "gtk-tool-utils.h"
#include "rntcontext.h"

static GskRenderNode *
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
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  if (filenames == NULL)
    {
      g_printerr ("Expected a filename\n");
      exit (1);
    }

  if (!gsk_render_node_write_to_file (node, filenames[0], &error))
    {
      g_printerr (_("Failed to save file: %s\n"), error->message);
      exit (1);
    }
  for (i = 1; i < g_strv_length (filenames); i++)
    {
      GskRenderNode *tmp = rnt_context_get_node (ctx, i - 1);
      if (tmp == NULL)
        {
          g_printerr (_("%u filenames given but only %zu nodes available\n"),
                      g_strv_length (filenames), i);
          break;
        }
      if (!gsk_render_node_write_to_file (tmp, filenames[i], &error))
        {
          g_printerr (_("Failed to save file: %s\n"), error->message);
          exit (1);
        }
    }

  return node;
}

static GskRenderNode *
filter_print (GskRenderNode *node,
              RntContext    *ctx,
              int            argc,
              const char   **argv)
{
  GBytes *bytes;

  if (argc != 1)
    {
      g_printerr ("print: Unexpected arguments.\n");
      exit (1);
    }

  bytes = gsk_render_node_serialize (node);
  g_print ("%s", (const char *) g_bytes_get_data (bytes, NULL));
  g_bytes_unref (bytes);

  return node;
}

typedef struct _Filter Filter;

struct _Filter
{
  const char *name;
  const char *description;
  GskRenderNode * (* run) (GskRenderNode *node,
                           RntContext    *ctx,
                           int            argc,
                           const char   **argv);
};

static const Filter filters[] = {
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

static const Filter *
filter_find (const char *name)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (filters); i++)
    {
      if (g_str_equal (name, filters[i].name))
        return &filters[i];
    }

  return NULL;
}

static char **
filter_argv (const char **argv,
             gsize        argc)
{
  GStrvBuilder *builder;
  gsize i;

  g_assert (argc > 0);

  builder = g_strv_builder_new ();
  g_strv_builder_add (builder, argv[0]);

  for (i = 1; i < argc; i++)
    {
      if (filter_find (argv[i]))
        break;

      g_strv_builder_add (builder, argv[i]);
    }

  return g_strv_builder_end (builder);
}

static void
list_filters (void)
{
  gsize i;
  int name_len;

  name_len = 0;
  for (i = 0; i < G_N_ELEMENTS (filters); i++)
    {
      name_len = MAX (name_len, strlen (filters[i].name));
    }

  for (i = 0; i < G_N_ELEMENTS (filters); i++)
    {
      g_print ("%*s\t%s\n", name_len, filters[i].name, filters[i].description);
    }
}

static GskRenderNode *
run_filters (GskRenderNode  *node,
             RntContext     *ctx,
             int             argc,
             const char    **argv)
{
  while (argc > 0)
    {
      const Filter *filter;
      char **argv_part;
      gsize argc_part;

      filter = filter_find (argv[0]);
      if (filter == NULL)
        {
          g_printerr (_("Argument \"%s\" is not a known filter\n"), argv[0]);
          exit (1);
        }

      argv_part = filter_argv (argv, argc);
      argc_part = g_strv_length (argv_part);

      node = filter->run (node, ctx, argc_part, (const char **) argv_part);

      g_strfreev (argv_part);
      argv += argc_part;
      argc -= argc_part;
    }

  return node;
}

static GLogWriterOutput
log_writer_func (GLogLevelFlags   level,
                 const GLogField *fields,
                 gsize            n_fields,
                 gpointer         user_data)
{
  gsize i;
  const char *domain = NULL;
  const char *message = NULL;

  for (i = 0; i < n_fields; i++)
    {
      if (g_strcmp0 (fields[i].key, "GLIB_DOMAIN") == 0)
        domain = fields[i].value;
      else if (g_strcmp0 (fields[i].key, "MESSAGE") == 0)
        message = fields[i].value;
    }

  if (message != NULL && !g_log_writer_default_would_drop (level, domain))
    {
      const char *prefix;
      switch (level & G_LOG_LEVEL_MASK)
        {
        case G_LOG_LEVEL_ERROR:
          prefix = "ERROR";
          break;
        case G_LOG_LEVEL_CRITICAL:
          prefix = "CRITICAL";
          break;
        case G_LOG_LEVEL_WARNING:
          prefix = "WARNING";
          break;
        default:
          prefix = "INFO";
          break;
        }
      g_printerr ("%s-%s: %s\n", domain, prefix, message);
    }

  return G_LOG_WRITER_HANDLED;
}

int
main (int argc, const char *argv[])
{
  gtk_init_check ();
  GOptionContext *context;
  char **filenames = NULL;
  gboolean list = FALSE;
  const GOptionEntry entries[] = {
    { "list", 0, 0, G_OPTION_ARG_NONE, &list, N_("list all filters and exit"), NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE…") },
    { NULL, }
  };
  char **argv_part;
  gsize argc_part;
  GskRenderNode *node;
  GError *error = NULL;
  RntContext *ctx;
  int result;

  g_set_prgname ("gtk-rendernode-tool");

  g_log_set_writer_func (log_writer_func, NULL, NULL);

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Filter a node file and print the result."));

  gtk_init_check ();

  argv_part = filter_argv (argv, argc);
  argc_part = g_strv_length (argv_part);

  if (!g_option_context_parse (context, (int[1]) { argc_part }, &argv_part, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }
  g_strfreev (argv_part);
  argv += argc_part;
  argc -= argc_part;

  g_option_context_free (context);

  if (list)
    {
      list_filters ();
      exit (0);
    }

  ctx = rnt_context_new ();

  if (filenames == NULL || filenames[0] == NULL)
    {
      node = gsk_render_node_ref (rnt_context_get_null_node (ctx));
    }
  else
    {
      if (g_strv_length (filenames) > 1)
        {
          gsize i;

          for (i = g_strv_length (filenames) - 1; i > 0; i--)
            {
              node = load_node_file (filenames[i]);
              rnt_context_push_node (ctx, node);
            }
        }

      node = load_node_file (filenames[0]);
    }

  node = run_filters (node, ctx, argc, argv);

  gtk_tool_run ();

  result = rnt_context_get_failed (ctx) ? EXIT_FAILURE : EXIT_SUCCESS;

  rnt_context_free (ctx);
  g_strfreev (filenames);
  g_clear_pointer (&node, gsk_render_node_unref);

  return result;
}
