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
#include "rntcontext.h"

/* nothing here is evil, so you can just skip to the comment that starts the evil */
#include "gsk/gskcopypasteutilsprivate.h"

GskRenderNode *
filter_copypaste (GskRenderNode  *node,
                  RntContext     *ctx,
                  int             argc,
                  const char    **argv)
{
  const GOptionEntry entries[] = {
    { NULL, }
  };
  GOptionContext *context;
  GError *error = NULL;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Replace copy/paste nodes with copies of nodes"));

  if (!g_option_context_parse (context, &argc, (char ***) &argv, &error))
    {
      rnt_context_error (ctx, "%s\n", error->message);
      g_error_free (error);
      return NULL;
    }

  if (argc != 1)
    {
      rnt_context_error (ctx, "Unexpected arguments\n");
      return NULL;
    }

  return gsk_render_node_replace_copy_paste (gsk_render_node_ref (node));
}
