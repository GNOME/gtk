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

static GskRenderNode *
filter_save (GskRenderNode *node,
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

  if (filenames == NULL || g_strv_length (filenames) != 1)
    {
      g_printerr ("Expected a filename\n");
      exit (1);
    }

  if (!gsk_render_node_write_to_file (node, filenames[0], &error))
    {
      g_printerr (_("Failed to save file: %s\n"), error->message);
      exit (1);
    }

  return NULL;
}


typedef struct _Filter Filter;

struct _Filter
{
  const char *name;
  const char *description;
  gboolean suppress_printing;
  GskRenderNode * (* run) (GskRenderNode *node,
                           int            argc,
                           const char   **argv);
};

static const Filter filters[] = {
  {
    .name = "copypaste",
    .description = "Replace copy/paste nodes with copies of nodes",
    .run = filter_copypaste,
  },
  {
    .name = "save",
    .description = "Save current node to file",
    .suppress_printing = TRUE,
    .run = filter_save,
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

void
do_filter (int          *argc_,
           const char ***argv_)
{
  GOptionContext *context;
  char **filenames = NULL;
  gboolean list = FALSE;
  const GOptionEntry entries[] = {
    { "list", 0, 0, G_OPTION_ARG_NONE, &list, N_("list all filters and exit"), NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE…") },
    { NULL, }
  };
  const char **argv;
  char **argv_part;
  gsize argc, argc_part;
  GskRenderNode *node;
  GError *error = NULL;
  const Filter *filter = NULL;

  g_set_prgname ("gtk4-rendernode-tool filter");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Filter a node file and print the result."));

  argv = *argv_;
  argc = *argc_;
  argv_part = filter_argv (argv, argc);
  argc_part = g_strv_length (argv_part);

  if (!g_option_context_parse (context, (int[1]) { argc_part }, &argv_part, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (list)
    {
      list_filters ();
      exit (0);
    }

  if (filenames == NULL)
    {
      g_printerr (_("No .node file specified\n"));
      exit (1);
    }

  if (g_strv_length (filenames) != 1)
    {
      g_printerr (_("Need a single .node file\n"));
      exit (1);
    }

  node = load_node_file (filenames[0]);

  while (TRUE)
    {
      g_strfreev (argv_part);
      argv += argc_part;
      argc -= argc_part;

      if (argc == 0)
        {
          if (filter == NULL || !filter->suppress_printing)
            {
              GBytes *bytes = gsk_render_node_serialize (node);
              g_print ("%s", (char *) g_bytes_get_data (bytes, NULL));
              g_bytes_unref (bytes);
            }
          break;
        }

      filter = filter_find (argv[0]);
      if (filter == NULL)
        {
          g_printerr (_("Argument \"%s\" is not a known filter\n"), argv[0]);
          exit (1);
        }

      argv_part = filter_argv (argv, argc);
      argc_part = g_strv_length (argv_part);

      node = filter->run (node, argc_part, (const char **) argv_part);
  }

  g_strfreev (filenames);
  g_clear_pointer (&node, gsk_render_node_unref);
}
