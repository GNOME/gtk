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

#pragma once

#if !defined (__GSK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif

#include <gsk/gsktypes.h>

G_BEGIN_DECLS

/**
 * GskRenderReplayNodeFilter:
 * @replay: The replay object used to replay the node
 * @node: The node to replay
 * @user_data: The user data
 *
 * A function to replay a node.
 *
 * The node may be returned unmodified.
 *
 * The node may be discarded by returning %NULL.
 *
 * If you do not want to do any handling yourself, call
 * [method@Gsk.RenderReplay.default] to use the default handler
 * that calls your function on the children of the node.
 *
 * Returns: (transfer full) (nullable): The replayed node
 *
 * Since: 4.22
 */
typedef GskRenderNode * (* GskRenderReplayNodeFilter)           (GskRenderReplay                *replay,
                                                                 GskRenderNode                  *node,
                                                                 gpointer                        user_data);

/**
 * GskRenderReplayNodeForeach:
 * @replay: The replay object used to replay the node
 * @node: The node to replay
 * @user_data: The user data
 *
 * A function called for every node.
 *
 * If this function returns TRUE, the replay will continue and call
 * the filter function.
 *
 * If it returns FALSE, the filter function will not be called and
 * any child nodes will be skipped.
 *
 * Returns: TRUE to descend into this node's child nodes (if any)
 *   or FALSE to skip them
 *
 * Since: 4.22
 */
typedef gboolean        (* GskRenderReplayNodeForeach)          (GskRenderReplay                *replay,
                                                                 GskRenderNode                  *node,
                                                                 gpointer                        user_data);

/**
 * GskRenderReplayTextureFilter:
 * @replay: The replay object used to replay the node
 * @texture: The texture to filter
 * @user_data: The user data
 *
 * A function that filters textures.
 *
 * The function will be called by the default replay function for
 * all nodes with textures. They will then generate a node using the
 * returned texture.
 *
 * It is valid for the function to return the passed in texture if
 * the texture shuld not be modified.
 *
 * Returns: (transfer full): The filtered texture
 *
 * Since: 4.22
 */
typedef GdkTexture *   (* GskRenderReplayTextureFilter)         (GskRenderReplay                *replay,
                                                                 GdkTexture                     *texture,
                                                                 gpointer                        user_data);

/**
 * GskRenderReplayFontFilter:
 * @replay: The replay object used to replay the node
 * @font: The font to filter
 * @user_data: The user data
 *
 * A function that filters fonts.
 *
 * The function will be called by the default replay function for
 * all nodes with fonts. They will then generate a node using the
 * returned font.
 *
 * It is valid for the function to return the passed in font if
 * the font shuld not be modified.
 *
 * Returns: (transfer full): The filtered font
 *
 * Since: 4.22
 */
typedef PangoFont  *   (* GskRenderReplayFontFilter)            (GskRenderReplay                *replay,
                                                                 PangoFont                      *font,
                                                                 gpointer                        user_data);


GDK_AVAILABLE_IN_4_22
GType                   gsk_render_replay_get_type              (void);
GDK_AVAILABLE_IN_4_22
GskRenderReplay *       gsk_render_replay_new                   (void);
GDK_AVAILABLE_IN_4_22
void                    gsk_render_replay_free                  (GskRenderReplay                *self);

GDK_AVAILABLE_IN_4_22
void                    gsk_render_replay_set_node_filter       (GskRenderReplay                *self,
                                                                 GskRenderReplayNodeFilter       filter,
                                                                 gpointer                        user_data,
                                                                 GDestroyNotify                  user_destroy);
GDK_AVAILABLE_IN_4_22
GskRenderNode *         gsk_render_replay_filter_node           (GskRenderReplay                *self,
                                                                 GskRenderNode                  *node);
GDK_AVAILABLE_IN_4_22
GskRenderNode *         gsk_render_replay_default               (GskRenderReplay                *self,
                                                                 GskRenderNode                  *node);

GDK_AVAILABLE_IN_4_22
void                    gsk_render_replay_set_node_foreach      (GskRenderReplay                *self,
                                                                 GskRenderReplayNodeForeach      foreach,
                                                                 gpointer                        user_data,
                                                                 GDestroyNotify                  user_destroy);
GDK_AVAILABLE_IN_4_22
void                    gsk_render_replay_foreach_node          (GskRenderReplay                *self,
                                                                 GskRenderNode                  *node);

GDK_AVAILABLE_IN_4_22
void                    gsk_render_replay_set_texture_filter    (GskRenderReplay                *self,
                                                                 GskRenderReplayTextureFilter    filter,
                                                                 gpointer                        user_data,
                                                                 GDestroyNotify                  user_destroy);
GDK_AVAILABLE_IN_4_22
GdkTexture *            gsk_render_replay_filter_texture        (GskRenderReplay                *self,
                                                                 GdkTexture                     *texture);
GDK_AVAILABLE_IN_4_22
void                    gsk_render_replay_set_font_filter       (GskRenderReplay                *self,
                                                                 GskRenderReplayFontFilter       filter,
                                                                 gpointer                        user_data,
                                                                 GDestroyNotify                  user_destroy);
GDK_AVAILABLE_IN_4_22
PangoFont *             gsk_render_replay_filter_font           (GskRenderReplay                *self,
                                                                 PangoFont                      *font);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskRenderReplay, gsk_render_replay_free)

G_END_DECLS

