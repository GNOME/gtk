/* GSK - The GTK Scene Kit
 *
 * Copyright 2016  Endless
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

#include "gskglshadernode.h"

#include "gskcontainernodeprivate.h"
#include "gskrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrenderreplay.h"

#include <gdk/gdkcairoprivate.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GskGLShaderNode:
 *
 * A render node using a GL shader when drawing its children nodes.
 */
struct _GskGLShaderNode
{
  GskRenderNode render_node;

  GskGLShader *shader;
  GBytes *args;
  GskRenderNode **children;
  guint n_children;
};

static void
gsk_gl_shader_node_finalize (GskRenderNode *node)
{
  GskGLShaderNode *self = (GskGLShaderNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_GL_SHADER_NODE));

  for (guint i = 0; i < self->n_children; i++)
    gsk_render_node_unref (self->children[i]);
  g_free (self->children);

  g_bytes_unref (self->args);

  g_object_unref (self->shader);

  parent_class->finalize (node);
}

static void
gsk_gl_shader_node_draw (GskRenderNode *node,
                         cairo_t       *cr,
                         GdkColorState *ccs)
{
  GdkRGBA pink = { 255 / 255., 105 / 255., 180 / 255., 1.0 };

  gdk_cairo_set_source_rgba_ccs (cr, ccs, &pink);
  gdk_cairo_rect (cr, &node->bounds);
  cairo_fill (cr);
}

static void
gsk_gl_shader_node_diff (GskRenderNode *node1,
                         GskRenderNode *node2,
                         GskDiffData   *data)
{
  GskGLShaderNode *self1 = (GskGLShaderNode *) node1;
  GskGLShaderNode *self2 = (GskGLShaderNode *) node2;

  if (gsk_rect_equal (&node1->bounds, &node2->bounds) &&
      self1->shader == self2->shader &&
      g_bytes_compare (self1->args, self2->args) == 0 &&
      self1->n_children == self2->n_children)
    {
      cairo_region_t *child_region = cairo_region_create();
      for (guint i = 0; i < self1->n_children; i++)
        gsk_render_node_diff (self1->children[i], self2->children[i], &(GskDiffData) { child_region, data->copies, data->surface });
      if (!cairo_region_is_empty (child_region))
        gsk_render_node_diff_impossible (node1, node2, data);
      cairo_region_destroy (child_region);
    }
  else
    {
      gsk_render_node_diff_impossible (node1, node2, data);
    }
}

static GskRenderNode **
gsk_gl_shader_node_get_children (GskRenderNode *node,
                                 gsize         *n_children)
{
  GskGLShaderNode *self = (GskGLShaderNode *) node;

  *n_children = self->n_children;

  return self->children;
}

static GskRenderNode *
gsk_gl_shader_node_replay (GskRenderNode   *node,
                           GskRenderReplay *replay)
{
  GskGLShaderNode *self = (GskGLShaderNode *) node;
  GskRenderNode *result;
  GskRenderNode **children;
  gboolean changed, all_empty;
  guint i;

  children = g_new (GskRenderNode *, self->n_children);
  changed = FALSE;
  all_empty = TRUE;

  for (i = 0; i < self->n_children; i++)
    {
      children[i] = gsk_render_replay_filter_node (replay, self->children[i]);

      if (children[i] == NULL)
        children[i] = gsk_container_node_new (NULL, 0);
      else
        all_empty = FALSE;

      if (children[i] != self->children[i])
        changed = TRUE;
    }

  if (!changed)
    result = gsk_render_node_ref (node);
  else if (all_empty)
    result = NULL;
  else
    result = gsk_gl_shader_node_new (self->shader,
                                     &node->bounds,
                                     self->args,
                                     children,
                                     self->n_children);

  for (i = 0; i < self->n_children; i++)
    gsk_render_node_unref (children[i]);
  g_free (children);

  return result;
}

static void
gsk_gl_shader_node_class_init (gpointer g_class,
                               gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_GL_SHADER_NODE;

  node_class->finalize = gsk_gl_shader_node_finalize;
  node_class->draw = gsk_gl_shader_node_draw;
  node_class->diff = gsk_gl_shader_node_diff;
  node_class->get_children = gsk_gl_shader_node_get_children;
  node_class->replay = gsk_gl_shader_node_replay;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskGLShaderNode, gsk_gl_shader_node)

/**
 * gsk_gl_shader_node_new:
 * @shader: the `GskGLShader`
 * @bounds: the rectangle to render the shader into
 * @args: Arguments for the uniforms
 * @children: (nullable) (array length=n_children): array of child nodes,
 *   these will be rendered to textures and used as input.
 * @n_children: Length of @children (currently the GL backend supports
 *   up to 4 children)
 *
 * Creates a `GskRenderNode` that will render the given @shader into the
 * area given by @bounds.
 *
 * The @args is a block of data to use for uniform input, as per types and
 * offsets defined by the @shader. Normally this is generated by
 * [method@Gsk.GLShader.format_args] or [struct@Gsk.ShaderArgsBuilder].
 *
 * See [class@Gsk.GLShader] for details about how the shader should be written.
 *
 * All the children will be rendered into textures (if they aren't already
 * `GskTextureNodes`, which will be used directly). These textures will be
 * sent as input to the shader.
 *
 * If the renderer doesn't support GL shaders, or if there is any problem
 * when compiling the shader, then the node will draw pink. You should use
 * [method@Gsk.GLShader.compile] to ensure the @shader will work for the
 * renderer before using it.
 *
 * Returns: (transfer full) (type GskGLShaderNode): A new `GskRenderNode`
 *
 * Deprecated: 4.16: GTK's new Vulkan-focused rendering
 *   does not support this feature. Use [GtkGLArea](../gtk4/class.GLArea.html)
 *   for OpenGL rendering.
 */
GskRenderNode *
gsk_gl_shader_node_new (GskGLShader           *shader,
                        const graphene_rect_t *bounds,
                        GBytes                *args,
                        GskRenderNode        **children,
                        guint                  n_children)
{
  GskGLShaderNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_GL_SHADER (shader), NULL);
  g_return_val_if_fail (bounds != NULL, NULL);
  g_return_val_if_fail (args != NULL, NULL);
  g_return_val_if_fail (g_bytes_get_size (args) == gsk_gl_shader_get_args_size (shader), NULL);
  g_return_val_if_fail ((children == NULL && n_children == 0) ||
                        (n_children == gsk_gl_shader_get_n_textures (shader)), NULL);

  self = gsk_render_node_alloc (GSK_TYPE_GL_SHADER_NODE);
  node = (GskRenderNode *) self;
  node->preferred_depth = gdk_color_state_get_depth (GDK_COLOR_STATE_SRGB);

  gsk_rect_init_from_rect (&node->bounds, bounds);
  gsk_rect_normalize (&node->bounds);

  self->shader = g_object_ref (shader);

  self->args = g_bytes_ref (args);

  self->n_children = n_children;
  if (n_children > 0)
    {
      self->children = g_malloc_n (n_children, sizeof (GskRenderNode *));
      for (guint i = 0; i < n_children; i++)
        {
          self->children[i] = gsk_render_node_ref (children[i]);
          node->preferred_depth = gdk_memory_depth_merge (node->preferred_depth,
                                                          gsk_render_node_get_preferred_depth (children[i]));
          node->contains_subsurface_node |= gsk_render_node_contains_subsurface_node (children[i]);
          node->contains_paste_node |= gsk_render_node_contains_paste_node (children[i]);
        }
    }

  return node;
}

/**
 * gsk_gl_shader_node_get_n_children:
 * @node: (type GskGLShaderNode): a `GskRenderNode` for a gl shader
 *
 * Returns the number of children
 *
 * Returns: The number of children
 *
 * Deprecated: 4.16: GTK's new Vulkan-focused rendering
 *   does not support this feature. Use [GtkGLArea](../gtk4/class.GLArea.html)
 *   for OpenGL rendering.
 */
guint
gsk_gl_shader_node_get_n_children (const GskRenderNode *node)
{
  const GskGLShaderNode *self = (const GskGLShaderNode *) node;

  return self->n_children;
}

/**
 * gsk_gl_shader_node_get_child:
 * @node: (type GskGLShaderNode): a `GskRenderNode` for a gl shader
 * @idx: the position of the child to get
 *
 * Gets one of the children.
 *
 * Returns: (transfer none): the @idx'th child of @node
 *
 * Deprecated: 4.16: GTK's new Vulkan-focused rendering
 *   does not support this feature. Use [GtkGLArea](../gtk4/class.GLArea.html)
 *   for OpenGL rendering.
 */
GskRenderNode *
gsk_gl_shader_node_get_child (const GskRenderNode *node,
                              guint                idx)
{
  const GskGLShaderNode *self = (const GskGLShaderNode *) node;

  return self->children[idx];
}

/**
 * gsk_gl_shader_node_get_shader:
 * @node: (type GskGLShaderNode): a `GskRenderNode` for a gl shader
 *
 * Gets shader code for the node.
 *
 * Returns: (transfer none): the `GskGLShader` shader
 */
GskGLShader *
gsk_gl_shader_node_get_shader (const GskRenderNode *node)
{
  const GskGLShaderNode *self = (const GskGLShaderNode *) node;

  return self->shader;
}

/**
 * gsk_gl_shader_node_get_args:
 * @node: (type GskGLShaderNode): a `GskRenderNode` for a gl shader
 *
 * Gets args for the node.
 *
 * Returns: (transfer none): A `GBytes` with the uniform arguments
 *
 * Deprecated: 4.16: GTK's new Vulkan-focused rendering
 *   does not support this feature. Use [GtkGLArea](../gtk4/class.GLArea.html)
 *   for OpenGL rendering.
 */
GBytes *
gsk_gl_shader_node_get_args (const GskRenderNode *node)
{
  const GskGLShaderNode *self = (const GskGLShaderNode *) node;

  return self->args;
}
G_GNUC_END_IGNORE_DEPRECATIONS
