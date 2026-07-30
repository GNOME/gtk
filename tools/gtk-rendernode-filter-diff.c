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
#include "overlayconfig.h"

#include "gsk/gskrendernodeprivate.h"

GskRenderNode *
filter_diff (GskRenderNode  *node,
             int             argc,
             const char    **argv)
{
  char **filenames = NULL;
  const GOptionEntry entries[] = {
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE") },
    { NULL, }
  };
  GOptionContext *context;
  OverlayConfig config;
  GError *error = NULL;
  cairo_region_t *region;
  GskRenderNode *reference, *result;

  overlay_config_init (&config);

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_set_main_group (context, overlay_config_create_option_group (&config));
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Show where two nodes differ"));

  if (!g_option_context_parse (context, &argc, (char ***) &argv, &error))
    {
      g_printerr ("diff: %s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  if (filenames == NULL || g_strv_length (filenames) != 1)
    {
      g_printerr ("diff: Need a single filename\n");
      exit (1);
    }

  reference = load_node_file (filenames[0]);
  region = cairo_region_create ();

  gsk_render_node_diff (node, reference, &(GskDiffData) { region, NULL, NULL });

  result = overlay_config_render_region (&config, node, region);
  gsk_render_node_unref (reference);
  gsk_render_node_unref (node);
  cairo_region_destroy (region);

  return result;
}
