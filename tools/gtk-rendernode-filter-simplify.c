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

static void
merge_containers_add_children (GPtrArray     *array,
                               GskRenderNode *node)
{
  GskRenderNode **children;
  gsize i, n_children;

  children = gsk_render_node_get_children (node, &n_children);

  for (i = 0; i < n_children; i++)
    {
      if (gsk_render_node_get_node_type (children[i]) == GSK_CONTAINER_NODE)
        merge_containers_add_children (array, children[i]);
      else
        g_ptr_array_add (array, children[i]);
    }
}

static GskRenderNode *
merge_containers (GskRenderNode *node)
{
  GskRenderNode **children;
  gsize i, n_children;
  GPtrArray *array;

  children = gsk_render_node_get_children (node, &n_children);
  array = NULL;

  for (i = 0; i < n_children; i++)
    {
      if (gsk_render_node_get_node_type (children[i]) == GSK_CONTAINER_NODE)
        break;
    }

  if (i == n_children)
    return gsk_render_node_ref (node);

  array = g_ptr_array_new ();
  merge_containers_add_children (array, node);
  node = gsk_container_node_new ((GskRenderNode **) array->pdata, array->len);
  g_ptr_array_unref (array);

  return node;
}

static GskRenderNode *
merge_transforms (GskRenderNode *node)
{
  GskRenderNode *child = gsk_transform_node_get_child (node);
  GskTransform *merged;
  GskRenderNode *result;

  if (gsk_render_node_get_node_type (child) != GSK_TRANSFORM_NODE)
    return gsk_render_node_ref (node);

  merged = gsk_transform_transform (gsk_transform_ref (gsk_transform_node_get_transform (node)),
                                    gsk_transform_node_get_transform (child));
  result = gsk_transform_node_new (gsk_transform_node_get_child (child), merged);

  gsk_transform_unref (merged);

  return result;
}

static GskRenderNode *
remove_containers (GskRenderNode *node)
{
  gsize n_children;

  n_children = gsk_container_node_get_n_children (node);

  if (n_children == 0)
    return NULL;
  else if (n_children == 1)
    return gsk_render_node_ref (gsk_container_node_get_child (node, 0));
  else
    return gsk_render_node_ref (node);
}

static GskRenderNode *
remove_transforms (GskRenderNode *node)
{
  if (gsk_transform_get_category (gsk_transform_node_get_transform (node)) == GSK_TRANSFORM_CATEGORY_IDENTITY)
    return gsk_render_node_ref (gsk_transform_node_get_child (node));
  else
    return gsk_render_node_ref (node);
}

static const struct 
{
  const char *name;
  GskRenderNodeType type;
  GskRenderNode * (* func) (GskRenderNode *);
} simplifications[] = {
  {
    .name = "merge-containers",
    .type = GSK_CONTAINER_NODE,
    .func = merge_containers,
  },
  {
    .name = "merge-transforms",
    .type = GSK_TRANSFORM_NODE,
    .func = merge_transforms,
  },
  {
    .name = "remove-containers",
    .type = GSK_CONTAINER_NODE,
    .func = remove_containers,
  },
  {
    .name = "remove-transforms",
    .type = GSK_TRANSFORM_NODE,
    .func = remove_transforms,
  },
};

static GskRenderNode *
simplify_node (GskRenderReplay *replay,
               GskRenderNode   *node,
               gpointer         user_data)
{
  node = gsk_render_replay_default (replay, node);

  while (node != NULL)
    {
      GskRenderNodeType node_type;
      GskRenderNode *simplified;
      gsize i;

      node_type = gsk_render_node_get_node_type (node);

      for (i = 0; i < G_N_ELEMENTS (simplifications); i++)
        {
          if (simplifications[i].type != 0 &&
              simplifications[i].type != node_type)
            continue;

          simplified = simplifications[i].func (node);
          if (simplified == node)
            {
              gsk_render_node_unref (simplified);
            }
          else
            {
              gsk_render_node_unref (node);
              node = simplified;
              break;
            }
        }

      if (i >= G_N_ELEMENTS (simplifications))
        break;
    }

  return node;
}

GskRenderNode *
filter_simplify (GskRenderNode  *node,
                 int             argc,
                 const char    **argv)
{
  const GOptionEntry entries[] = {
    { NULL, }
  };
  GOptionContext *context;
  GskRenderReplay *replay;
  GskRenderNode *result;
  GError *error = NULL;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Simplify node tree"));

  if (!g_option_context_parse (context, &argc, (char ***) &argv, &error))
    {
      g_printerr ("simplify: %s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  if (argc != 1)
    {
      g_printerr ("simplify: Unexpected arguments\n");
      exit (1);
    }

  replay = gsk_render_replay_new ();
  gsk_render_replay_set_node_filter (replay,
                                     simplify_node,
                                     NULL,
                                     NULL);
  result = gsk_render_replay_filter_node (replay, node);
  gsk_render_replay_free (replay);

  gsk_render_node_unref (node);

  return result;
}
