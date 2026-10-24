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

GskRenderNode *
filter_print (GskRenderNode *node,
              RntContext    *ctx,
              int            argc,
              const char   **argv)
{
  GBytes *bytes;

  if (argc != 1)
    {
      rnt_context_error (ctx, "print: Unexpected arguments.\n");
      return NULL;
    }

  bytes = gsk_render_node_serialize (node);
  rnt_context_print (ctx, "%s", (const char *) g_bytes_get_data (bytes, NULL));
  g_bytes_unref (bytes);

  return gsk_render_node_ref (node);
}
