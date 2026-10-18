/*  Copyright 2024 Red Hat, Inc.
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
 * Author: Benjamin Otte
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include "gtk-rendernode-tool.h"
#include "rntcontext.h"

static void
rect_snap_grow (graphene_rect_t *rect)
{
  float x1, y1;

  x1 = rect->origin.x + rect->size.width;
  y1 = rect->origin.y + rect->size.height;
  *rect = GRAPHENE_RECT_INIT (floorf (rect->origin.x),
                              floorf (rect->origin.y),
                              ceilf (x1) - floorf (rect->origin.x),
                              ceilf (y1) - floorf (rect->origin.y));
}

static GskRenderNode *
merge_horizontal (GskRenderNode *node,
                  RntContext    *ctx,
                  gboolean       accurate)
{
  float offset = 0;
  GPtrArray *array;
  GskRenderNode *result;
  gsize i;

  array = g_ptr_array_new ();
  g_ptr_array_set_free_func (array, (GDestroyNotify) gsk_render_node_unref);
  for (i = 0; node != NULL; i++)
    {
      graphene_rect_t bounds;
      gsk_render_node_get_bounds (node, &bounds);
      if (!accurate)
        rect_snap_grow (&bounds);
      if (bounds.origin.x != offset || bounds.origin.y != 0)
        {
          GskTransform *transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (offset - bounds.origin.x, - bounds.origin.y));
          g_ptr_array_add (array, gsk_transform_node_new (node, transform));
          gsk_transform_unref (transform);
        }
      else
        {
          g_ptr_array_add (array, gsk_render_node_ref (node));
        }

      offset += bounds.size.width;
      node = rnt_context_get_node (ctx, i);
    }

  result = gsk_container_node_new ((GskRenderNode **) array->pdata, array->len);
  g_ptr_array_free (array, TRUE);

  return result;
}

GskRenderNode *
filter_merge (GskRenderNode  *node,
              RntContext     *ctx,
              int             argc,
              const char    **argv)
{
  GOptionContext *context;
  gboolean accurate = FALSE;
  gboolean keep = FALSE;
  const GOptionEntry entries[] = {
    { "accurate", 0, 0, G_OPTION_ARG_NONE, &accurate, N_("Use float bounds, not integers"), NULL },
    { "keep", 0, 0, G_OPTION_ARG_NONE, &keep, N_("Keep nodes in stack"), NULL },
    { NULL, }
  };
  GError *error = NULL;
  GskRenderNode *result;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Merge the stack of nodes into one"));

  if (!g_option_context_parse (context, &argc, (char ***) &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  result = merge_horizontal (node, ctx, accurate);

  if (keep)
    {
      rnt_context_push_node (ctx, node);
    }
  else
    {
      gsk_render_node_unref (node);
      rnt_context_clear_nodes (ctx);
    }

  return result;
}
