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
#include "gtk-tool-utils.h"

#include "rntcontext.h"
#include "rntfilter.h"

static void
list_filters (void)
{
  gsize i;
  int name_len;

  name_len = 0;
  for (i = 0; i < rnt_filter_n_filters (); i++)
    {
      const RntFilter *filter = rnt_filter_get (i);

      name_len = MAX (name_len, strlen (rnt_filter_get_name (filter)));
    }

  for (i = 0; i < rnt_filter_n_filters (); i++)
    {
      const RntFilter *filter = rnt_filter_get (i);

      g_print ("%*s\t%s\n",
               name_len,
               rnt_filter_get_name (filter),
               rnt_filter_get_description (filter));
    }
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

  ctx = rnt_context_new (NULL, NULL, NULL, NULL);

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_set_main_group (context, rnt_context_create_option_group (ctx));
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Filter a node file and print the result."));

  gtk_init_check ();

  argv_part = rnt_filter_argv (argc, argv);
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

  node = rnt_context_run (ctx, node, argc, argv);

  gtk_tool_run ();

  result = rnt_context_get_failed (ctx) ? EXIT_FAILURE : EXIT_SUCCESS;

  rnt_context_free (ctx);
  g_strfreev (filenames);
  g_clear_pointer (&node, gsk_render_node_unref);

  return result;
}
