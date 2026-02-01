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

/* nothing here is evil, so you can just skip to the comment that starts the evil */
#include "gsk/gskcopypasteutilsprivate.h"

#include "gsk/gskcopypasteutils.c"

/* this comment starts the evil */

GskRenderNode *
filter_copypaste (GskRenderNode  *node,
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
      g_printerr ("copypaste: %s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  if (argc != 1)
    {
      g_printerr ("copypaste: Unexpected arguments\n");
      exit (1);
    }

  return gsk_render_node_replace_copy_paste (node);
}
