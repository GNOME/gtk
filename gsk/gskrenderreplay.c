/* GSK - The GTK Scene Kit
 *
 * Copyright 2025  Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gskrenderreplay.h"

#include "gskrendernodeprivate.h"

/**
 * GskRenderReplay: (free-func gsk_render_replay_free)
 *
 * A facility to replay a [class@Gsk.RenderNode] and its children, potentially
 * modifying them.
 *
 * This is a utility tool to walk a rendernode tree. The most powerful way
 * is to provide a function via [method@Gsk.RenderReplay.set_node_filter]
 * to filter each individual node and then run
 * [method@Gsk.RenderReplay.filter_node] on the nodes you want to filter.
 *
 * An easier method exists to just walk the node tree and extract information
 * without any modifications. If you want to do that, the functions
 * [method@Gsk.RenderReplay.set_node_foreach] exists. You can also call
 * [method@Gsk.RenderReplay.foreach_node] to run that function. Note that
 * the previously mentioned complex functionality will still be invoked if you
 * have set up a function for it, but its result will not be returned.
 *
 * Here is an example that combines both approaches to print the whole tree:
 *
 * ```c
 * #include <gtk/gtk.h>
 * 
 * static GskRenderNode *
 * print_nodes (GskRenderReplay *replay,
 *              GskRenderNode   *node,
 *              gpointer         user_data)
 * {
 *   int *depth = user_data;
 *   GskRenderNode *result;
 * 
 *   g_print ("%*s%s\n", 2 * *depth, "", g_type_name_from_instance ((GTypeInstance *) node));
 *   
 *   *depth += 1;
 *   result = gsk_render_replay_default (replay, node);
 *   *depth -= 1;
 * 
 *   return result;
 * }
 * 
 * int
 * main (int argc, char *argv[])
 * {
 *   GFile *file;
 *   GBytes *bytes;
 *   GskRenderNode *node;
 *   GskRenderReplay *replay;
 *   int depth = 0;
 * 
 *   gtk_init ();
 * 
 *   if (argc < 2)
 *     {
 *       g_print ("usage: %s NODEFILE\n", argv[0]);
 *       return 0;
 *     }
 * 
 *   file = g_file_new_for_commandline_arg (argv[1]);
 *   bytes = g_file_load_bytes (file, NULL, NULL, NULL);
 *   g_object_unref (file);
 *   if (bytes == NULL)
 *     return 1;
 * 
 *   node = gsk_render_node_deserialize (bytes, NULL, NULL);
 *   g_bytes_unref (bytes);
 *   if (node == NULL)
 *     return 1;
 * 
 *   replay = gsk_render_replay_new ();
 *   gsk_render_replay_set_node_filter (replay, print_nodes, &depth, NULL);
 *   gsk_render_node_foreach_node (replay, node);
 *   gsk_render_node_unref (node);
 * 
 *   return 0;
 * }
 * ```
 *
 * Since: 4.22
 */

struct _GskRenderReplay
{
  GskRenderReplayNodeFilter node_filter;
  gpointer node_filter_data;
  GDestroyNotify node_filter_destroy;

  GskRenderReplayNodeForeach node_foreach;
  gpointer node_foreach_data;
  GDestroyNotify node_foreach_destroy;

  GskRenderReplayTextureFilter texture_filter;
  gpointer texture_filter_data;
  GDestroyNotify texture_filter_destroy;

  GskRenderReplayFontFilter font_filter;
  gpointer font_filter_data;
  GDestroyNotify font_filter_destroy;
};

/**
 * gsk_render_replay_new:
 *
 * Creates a new replay object to replay nodes.
 *
 * Returns: A new replay object to replay nodes
 *
 * Since: 4.22
 **/
GskRenderReplay *
gsk_render_replay_new (void)
{
  GskRenderReplay *self;

  self = g_new0 (GskRenderReplay, 1);

  return self;
}

/**
 * gsk_render_replay_free:
 * @self: the replay object to free
 *
 * Frees a `GskRenderReplay`.
 *
 * Since: 4.22
 **/
void
gsk_render_replay_free (GskRenderReplay *self)
{
  g_return_if_fail (self != NULL);

  gsk_render_replay_set_node_foreach (self, NULL, NULL, NULL);
  gsk_render_replay_set_node_filter (self, NULL, NULL, NULL);
  gsk_render_replay_set_texture_filter (self, NULL, NULL, NULL);
  gsk_render_replay_set_font_filter (self, NULL, NULL, NULL);

  g_free (self);
}

/**
 * gsk_render_replay_set_node_filter:
 * @filter: (nullable) (scope notified) (closure user_data) (destroy user_destroy):
 *   the function to call to replay nodes
 * @user_data: user data to pass to @func
 * @user_destroy: destroy notify that will be called to release
 *   the user data parameter
 *
 * Sets the function to use as a node filter.
 *
 * This is the most complex function to use for replaying nodes.
 * It can either:
 *
 * * keep the node and just return it unchanged
 *
 * * create a replacement node and return that
 *
 * * discard the node by returning `NULL`
 *
 * * call [method@Gsk.RenderReplay.default] to have the default handler
 *   run for this node, which calls your function on its children
 *
 * Since: 4.22
 */
void
gsk_render_replay_set_node_filter (GskRenderReplay           *self,
                                   GskRenderReplayNodeFilter  filter,
                                   gpointer                   user_data,
                                   GDestroyNotify             user_destroy)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (filter || user_data == NULL);
  g_return_if_fail (user_data || !user_destroy);

  if (self->node_filter_destroy)
    self->node_filter_destroy (self->node_filter_data);

  self->node_filter = filter;
  self->node_filter_data = user_data;
  self->node_filter_destroy = user_destroy;
}

/**
 * gsk_render_replay_filter_node:
 * @self: the replay
 * @node: the node to replay
 *
 * Replays a node using the replay's filter function.
 *
 * After the replay the node may be unchanged, or it may be
 * removed, which will result in %NULL being returned.
 *
 * This function calls the registered callback in the following order:
 *
 * 1. If a foreach function is set, it is called first. If it returns 
 *    false, this function immediately exits and returns the passed
 *    in node.
 *
 * 2. If a node filter is set, it is called and its result is returned.
 *
 * 3. [method@Gsk.RenderReplay.default] is called and its result is
 *    returned.
 *
 * Returns: (transfer full) (nullable): The replayed node
 *
 * Since: 4.22
 **/
GskRenderNode *
gsk_render_replay_filter_node (GskRenderReplay *self,
                               GskRenderNode   *node)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  if (self->node_foreach &&
      !self->node_foreach (self, node, self->node_foreach_data))
    {
      return gsk_render_node_ref (node);
    }

  if (self->node_filter)
    return self->node_filter (self, node, self->node_filter_data);
  else
    return gsk_render_replay_default (self, node);
}

/**
 * gsk_render_replay_default:
 * @self: the replay
 * @node: the node to replay
 *
 * Replays the node using the default method.
 *
 * The default method calls [method@Gsk.RenderReplay.filter_node]
 * on all its child nodes and the filter functions for all its
 * properties. If none of them are changed, it returns the passed
 * in node. Otherwise it constructs a new node with the changed
 * children and properties.
 *
 * It may not be possible to construct a new node when any of the
 * callbacks return NULL. In that case, this function will return
 * NULL, too.
 *
 * Returns: (transfer full) (nullable): The replayed node 
 *
 * Since: 4.22
 **/
GskRenderNode *
gsk_render_replay_default (GskRenderReplay *self,
                           GskRenderNode   *node)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  return GSK_RENDER_NODE_GET_CLASS (node)->replay (node, self);
}

/**
 * gsk_render_replay_set_node_foreach:
 * @self: the replay
 * @foreach: (nullable) (scope notified) (closure user_data) (destroy user_destroy):
 *   the function to call for all nodes
 * @user_data: user data to pass to @func
 * @user_destroy: destroy notify that will be called to release
 *   the user data parameter
 *
 * Sets the function to call for every node.
 *
 * This function is called before the node filter, so if it returns
 * false, the node filter will never be called.
 *
 * Since: 4.22
 */
void
gsk_render_replay_set_node_foreach (GskRenderReplay            *self,
                                    GskRenderReplayNodeForeach  foreach,
                                    gpointer                    user_data,
                                    GDestroyNotify              user_destroy)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (foreach || user_data == NULL);
  g_return_if_fail (user_data || !user_destroy);

  if (self->node_foreach_destroy)
    self->node_foreach_destroy (self->node_foreach_data);

  self->node_foreach = foreach;
  self->node_foreach_data = user_data;
  self->node_foreach_destroy = user_destroy;
}

/**
 * gsk_render_replay_foreach_node:
 * @self: the replay
 * @node: the node to replay
 *
 * Calls the filter and foreach functions for each node.
 *
 * This function calls [method@Gsk.RenderReplay.filter_node] internally,
 * but discards the result assuming no changes were made.
 *
 * Since: 4.22
 **/
void
gsk_render_replay_foreach_node (GskRenderReplay *self,
                                GskRenderNode   *node)
{
  GskRenderNode *ignored;

  g_return_if_fail (self != NULL);
  g_return_if_fail (GSK_IS_RENDER_NODE (node));

  ignored = gsk_render_replay_filter_node (self, node);

  gsk_render_node_unref (ignored);
}

/**
 * gsk_render_replay_set_texture_filter:
 * @self: the replay
 * @filter: (nullable) (scope notified) (closure user_data) (destroy user_destroy):
 *   the texture filter function
 * @user_data: user data to pass to @filter
 * @user_destroy: destroy notify that will be called to release
 *   the user data parameter
 *
 * Sets a filter function to be called by [method@Gsk.RenderReplay.default]
 * for nodes that contain textures.
 *
 * You can call [method@GskRenderReplay.filter_texture] to filter
 * a texture yourself.
 *
 * Since: 4.22
 **/
void
gsk_render_replay_set_texture_filter (GskRenderReplay              *self,
                                      GskRenderReplayTextureFilter  filter,
                                      gpointer                      user_data,
                                      GDestroyNotify                user_destroy)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (filter || user_data == NULL);
  g_return_if_fail (user_data || !user_destroy);

  if (self->texture_filter_destroy)
    self->texture_filter_destroy (self->texture_filter_data);

  if (filter)
    self->texture_filter = filter;
  self->texture_filter_data = user_data;
  self->texture_filter_destroy = user_destroy;
}

/**
 * gsk_render_replay_filter_texture:
 * @self: the replay
 * @texture: The texture to filter
 *
 * Filters a texture using the current filter function.
 *
 * Returns: (transfer full): the filtered texture
 *
 * Since: 4.22
 **/
GdkTexture *
gsk_render_replay_filter_texture (GskRenderReplay *self,
                                  GdkTexture      *texture)
{
  GdkTexture *result;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), NULL);

  if (!self->texture_filter)
    return g_object_ref (texture);

  result = self->texture_filter (self, texture, self->texture_filter_data);
  g_assert (result != NULL);

  return result;
}

/**
 * gsk_render_replay_set_font_filter:
 * @self: the replay
 * @filter: (nullable) (scope notified) (closure user_data) (destroy user_destroy):
 *   the font filter function
 * @user_data: user data to pass to @filter
 * @user_destroy: destroy notify that will be called to release
 *   the user data parameter
 *
 * Sets a filter function to be called by [method@Gsk.RenderReplay.default]
 * for nodes that contain fonts.
 *
 * You can call [method@GskRenderReplay.filter_font] to filter
 * a font yourself.
 *
 * Since: 4.22
 **/
void
gsk_render_replay_set_font_filter (GskRenderReplay           *self,
                                   GskRenderReplayFontFilter  filter,
                                   gpointer                   user_data,
                                   GDestroyNotify             user_destroy)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (filter || user_data == NULL);
  g_return_if_fail (user_data || !user_destroy);

  if (self->font_filter_destroy)
    self->font_filter_destroy (self->font_filter_data);

  if (filter)
    self->font_filter = filter;
  self->font_filter_data = user_data;
  self->font_filter_destroy = user_destroy;
}

/**
 * gsk_render_replay_filter_font:
 * @self: the replay
 * @font: The font to filter
 *
 * Filters a font using the current filter function.
 *
 * Returns: (transfer full): the filtered font
 *
 * Since: 4.22
 **/
PangoFont *
gsk_render_replay_filter_font (GskRenderReplay *self,
                               PangoFont       *font)
{
  PangoFont *result;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (PANGO_IS_FONT (font), NULL);

  if (!self->font_filter)
    return g_object_ref (font);

  result = self->font_filter (self, font, self->font_filter_data);
  g_assert (result != NULL);

  return result;
}

G_DEFINE_POINTER_TYPE (GskRenderReplay, gsk_render_replay)
