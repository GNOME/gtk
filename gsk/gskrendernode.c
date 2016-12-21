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

/**
 * SECTION:GskRenderNode
 * @Title: GskRenderNode
 * @Short_description: Simple scene graph element
 *
 * #GskRenderNode is the basic block in a scene graph to be
 * rendered using #GskRenderer.
 *
 * Each node has a parent, except the top-level node; each node may have
 * children nodes.
 *
 * Each node has an associated drawing surface, which has the size of
 * the rectangle set using gsk_render_node_set_bounds(). Nodes have an
 * associated transformation matrix, which is used to position and
 * transform the node on the scene graph; additionally, they also have
 * a child transformation matrix, which will be applied to each child.
 *
 * Render nodes are meant to be transient; once they have been associated
 * to a #GskRenderer it's safe to release any reference you have on them.
 * All #GskRenderNodes are immutable, you can only specify their properties
 * during construction.
 */

#include "config.h"

#include "gskrendernodeprivate.h"

#include "gskdebugprivate.h"
#include "gskrendererprivate.h"
#include "gsktexture.h"

#include <graphene-gobject.h>

#include <math.h>

#include <gobject/gvaluecollector.h>

#define ALLOCATE_CHUNK_SIZE (16*1024 - 2*sizeof(gsize))

#define ALIGN(size, base)       ((base) * (gsize) (((size) + (base) - 1) / (base)))

struct _GskRenderTree
{
  GObject parent_instance;

  guint8 *chunk;
  gsize chunk_offset;
  int chunk_count;

  GPtrArray *destroy_list;
};

G_DEFINE_TYPE (GskRenderTree, gsk_render_tree, G_TYPE_OBJECT)
G_DEFINE_QUARK (gsk-serialization-error-quark, gsk_serialization_error)

static void
gsk_render_tree_finalize (GObject *gobject)
{
  GskRenderTree *self = GSK_RENDER_TREE (gobject);
  int i;

  /* We free in reverse order, because the notify may touch
     something allocated before it */
  for (i = self->destroy_list->len - 2; i >= 0 ; i -= 2)
    {
      GDestroyNotify notify = g_ptr_array_index (self->destroy_list, i);
      gpointer data = g_ptr_array_index (self->destroy_list, i + 1);
      notify (data);
    }

  g_ptr_array_free (self->destroy_list, TRUE);

  G_OBJECT_CLASS (gsk_render_tree_parent_class)->finalize (gobject);
}

static void
gsk_render_tree_class_init (GskRenderTreeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gsk_render_tree_finalize;
}

static void
gsk_render_tree_init (GskRenderTree *self)
{
  self->destroy_list = g_ptr_array_new ();
}

void
gsk_render_tree_add_cleanup (GskRenderTree  *self,
                             GDestroyNotify notify,
                             gpointer data)
{
  g_ptr_array_add (self->destroy_list, notify);
  g_ptr_array_add (self->destroy_list, data);
}

/* Note: Align-size must be a power of two */
gpointer
gsk_render_tree_allocate (GskRenderTree *self, gsize n_bytes, gsize align_size)
{
  gpointer data = NULL;

  if (n_bytes >= ALLOCATE_CHUNK_SIZE / 4)
    {
      data = g_malloc0 (n_bytes);
      gsk_render_tree_add_cleanup (self, g_free, data);
    }
  else
    {
      self->chunk_offset = (self->chunk_offset + align_size - 1) & ~(align_size - 1);
      if (self->chunk == NULL || (ALLOCATE_CHUNK_SIZE - self->chunk_offset) < n_bytes)
        {
          self->chunk = g_malloc (ALLOCATE_CHUNK_SIZE);
          gsk_render_tree_add_cleanup (self, g_free, self->chunk);
          self->chunk_offset = 0;
        }

      data = self->chunk + self->chunk_offset;
      self->chunk_offset += n_bytes;
      memset (data, 0, n_bytes);
    }

  return data;
}

GskRenderNode *
gsk_render_tree_ref_foreign (GskRenderTree  *tree, GskRenderNode *node)
{
  if (node->tree != tree)
    gsk_render_tree_add_cleanup (tree, (GDestroyNotify)gsk_render_node_unref, gsk_render_node_ref (node));

  return node;
}


GskRenderTree *
gsk_render_tree_new ()
{
  return g_object_new (GSK_TYPE_RENDER_TREE,
                       NULL);
}

/**
 * GskRenderNode: (ref-func gsk_render_node_ref) (unref-func gsk_render_node_unref)
 *
 * The `GskRenderNode` structure contains only private data.
 *
 * Since: 3.90
 */

G_DEFINE_BOXED_TYPE (GskRenderNode, gsk_render_node,
                     gsk_render_node_ref,
                     gsk_render_node_unref)

/*< private >
 * gsk_render_tree_new_node:
 * @node_class: class structure for this node
 *
 * Returns: (transfer full): the newly created #GskRenderNode
 */
GskRenderNode *
gsk_render_tree_new_node (GskRenderTree *self, const GskRenderNodeClass *node_class, gsize extra_size)
{
  GskRenderNode *node;

  g_return_val_if_fail (node_class != NULL, NULL);
  g_return_val_if_fail (node_class->node_type != GSK_NOT_A_RENDER_NODE, NULL);

  node = gsk_render_tree_allocate (self, node_class->struct_size + extra_size, 2*sizeof(gsize));

  node->node_class = node_class;
  node->tree = self;
  node->min_filter = GSK_SCALING_FILTER_NEAREST;
  node->mag_filter = GSK_SCALING_FILTER_NEAREST;

  return node;
}

GskRenderTree *
gsk_render_node_get_tree (GskRenderNode *self)
{
  return self->tree;
}

/**
 * gsk_render_node_ref:
 * @node: a #GskRenderNode
 *
 * Acquires a reference on the given #GskRenderNode.
 * All nodes are owned by the tree they are part of, so normally you don't
 * need to ref individual nodes.
 *
 * Note: This keeps the whole tree alive for the lifetime of the node.
 *
 * Returns: (transfer none): the #GskRenderNode with an additional reference
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_render_node_ref (GskRenderNode *node)
{
  GskRenderTree *tree;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  tree = gsk_render_node_get_tree (node);
  g_object_ref (tree);

  return node;
}

/**
 * gsk_render_node_unref:
 * @node: a #GskRenderNode
 *
 * Releases a reference on the given #GskRenderNode.
 *
 * If the reference was the last, the resources associated to the @node are
 * freed.
 *
 * Since: 3.90
 */
void
gsk_render_node_unref (GskRenderNode *node)
{
  GskRenderTree *tree;

  g_return_if_fail (GSK_IS_RENDER_NODE (node));

  tree = gsk_render_node_get_tree (node);
  g_object_unref (tree);
}

/**
 * gsk_render_node_get_node_type:
 * @node: a #GskRenderNode
 *
 * Returns the type of the @node.
 *
 * Returns: the type of the #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNodeType
gsk_render_node_get_node_type (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), GSK_NOT_A_RENDER_NODE);

  return node->node_class->node_type;
}

/**
 * gsk_render_node_get_bounds:
 * @node: a #GskRenderNode
 * @bounds: (out caller-allocates): return location for the boundaries
 *
 * Retrieves the boundaries of the @node. The node will not draw outside
 * of its boundaries.
 *
 * Since: 3.90
 */
void
gsk_render_node_get_bounds (GskRenderNode   *node,
                            graphene_rect_t *bounds)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));
  g_return_if_fail (bounds != NULL);

  graphene_rect_init_from_rect (bounds, &node->bounds);
}

void
gsk_render_node_set_scaling_filters (GskRenderNode    *node,
                                     GskScalingFilter  min_filter,
                                     GskScalingFilter  mag_filter)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));

  if (node->min_filter != min_filter)
    node->min_filter = min_filter;
  if (node->mag_filter != mag_filter)
    node->mag_filter = mag_filter;
}

/**
 * gsk_render_node_set_name:
 * @node: a #GskRenderNode
 * @name: (nullable): a name for the node
 *
 * Sets the name of the node.
 *
 * A name is generally useful for debugging purposes.
 *
 * Since: 3.90
 */
void
gsk_render_node_set_name (GskRenderNode *node,
                          const char    *name)
{
  GskRenderTree *tree;
  char *new_name;

  g_return_if_fail (GSK_IS_RENDER_NODE (node));

  tree = gsk_render_node_get_tree (node);

  new_name = gsk_render_tree_allocate (tree, strlen (name) + 1, 1);
  strcpy (new_name, name);
  node->name = new_name;
}

/**
 * gsk_render_node_get_name:
 * @node: a #GskRenderNode
 *
 * Retrieves the name previously set via gsk_render_node_set_name().
 * If no name has been set, %NULL is returned.
 *
 * Returns: (nullable): The name previously set via
 *     gsk_render_node_set_name() or %NULL
 *
 * Since: 3.90
 **/
const char *
gsk_render_node_get_name (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  return node->name;
}

/**
 * gsk_render_node_draw:
 * @node: a #GskRenderNode
 * @cr: cairo context to draw to
 *
 * Draw the contents of @node to the given cairo context.
 *
 * Typically, you'll use this function to implement fallback rendering
 * of #GskRenderNodes on an intermediate Cairo context, instead of using
 * the drawing context associated to a #GdkWindow's rendering buffer.
 *
 * For advanced nodes that cannot be supported using Cairo, in particular
 * for nodes doing 3D operations, this function may fail.
 **/
void
gsk_render_node_draw (GskRenderNode *node,
                      cairo_t       *cr)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (cairo_status (cr) == CAIRO_STATUS_SUCCESS);

  cairo_save (cr);

  if (!GSK_RENDER_MODE_CHECK (GEOMETRY))
    {
      GSK_NOTE (CAIRO, g_print ("CLIP = { .x = %g, .y = %g, .width = %g, .height = %g }\n",
                                node->bounds.origin.x, node->bounds.origin.y,
                                node->bounds.size.width, node->bounds.size.height));

      cairo_rectangle (cr, node->bounds.origin.x, node->bounds.origin.y, node->bounds.size.width, node->bounds.size.height);
      cairo_clip (cr);
    }

  GSK_NOTE (CAIRO, g_print ("Rendering node %s[%p]\n",
                            node->name,
                            node));

  node->node_class->draw (node, cr);

  if (GSK_RENDER_MODE_CHECK (GEOMETRY))
    {
      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
      cairo_rectangle (cr, node->bounds.origin.x - 1, node->bounds.origin.y - 1,
                       node->bounds.size.width + 2, node->bounds.size.height + 2);
      cairo_set_line_width (cr, 2);
      cairo_set_source_rgba (cr, 0, 0, 0, 0.5);
      cairo_stroke (cr);
    }

  cairo_restore (cr);

  if (cairo_status (cr))
    {
      g_warning ("drawing failure for render node %s '%s': %s",
                 node->node_class->type_name,
                 gsk_render_node_get_name (node),
                 cairo_status_to_string (cairo_status (cr)));
    }
}

#define GSK_RENDER_NODE_SERIALIZATION_VERSION 0
#define GSK_RENDER_NODE_SERIALIZATION_ID "GskRenderNode"

/**
 * gsk_render_node_serialize:
 * @node: a #GskRenderNode
 *
 * Serializes the @node for later deserialization via
 * gsk_render_node_deserialize(). No guarantees are made about the format
 * used other than that the same version of GTK+ will be able to deserialize
 * the result of a call to gsk_render_node_serialize() and
 * gsk_render_node_deserialize() will correctly reject files it cannot open
 * that were created with previous versions of GTK+.
 *
 * The intended use of this functions is testing, benchmarking and debugging.
 * The format is not meant as a permanent storage format.
 *
 * Returns: a #GBytes representing the node.
 **/
GBytes *
gsk_render_node_serialize (GskRenderNode *node)
{
  GVariant *node_variant, *variant;
  GBytes *result;

  node_variant = gsk_render_node_serialize_node (node);

  variant = g_variant_new ("(suuv)",
                           GSK_RENDER_NODE_SERIALIZATION_ID,
                           (guint32) GSK_RENDER_NODE_SERIALIZATION_VERSION,
                           (guint32) gsk_render_node_get_node_type (node),
                           node_variant);

  result = g_variant_get_data_as_bytes (variant);
  g_variant_unref (variant);

  return result;
}

/**
 * gsk_render_node_write_to_file:
 * @node: a #GskRenderNode
 * @filename: the file to save it to.
 * @error: Return location for a potential error
 *
 * This function is equivalent to calling gsk_render_node_serialize()
 * followed by g_file_set_contents(). See those two functions for details
 * on the arguments.
 *
 * It is mostly intended for use inside a debugger to quickly dump a render
 * node to a file for later inspection.
 *
 * Returns: %TRUE if saving was successful
 **/
gboolean
gsk_render_node_write_to_file (GskRenderNode *node,
                               const char    *filename,
                               GError       **error)
{
  GBytes *bytes;
  gboolean result;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  bytes = gsk_render_node_serialize (node);
  result = g_file_set_contents (filename,
                                g_bytes_get_data (bytes, NULL),
                                g_bytes_get_size (bytes),
                                error);
  g_bytes_unref (bytes);

  return result;
}

/**
 * gsk_render_node_deserialize:
 * @bytes: the bytes containing the data
 * @error: (allow-none): location to store error or %NULL
 *
 * Loads data previously created via gsk_render_node_serialize(). For a
 * discussion of the supported format, see that function.
 *
 * Returns: (nullable) (transfer full): a new #GskRenderNode or %NULL on
 *     error.
 **/
GskRenderNode *
gsk_render_node_deserialize (GBytes  *bytes,
                             GError **error)
{
  char *id_string;
  guint32 version, node_type;
  GVariant *variant, *node_variant;
  GskRenderNode *node = NULL;
  GskRenderTree *tree;

  variant = g_variant_new_from_bytes (G_VARIANT_TYPE ("(suuv)"), bytes, FALSE);

  g_variant_get (variant, "(suuv)", &id_string, &version, &node_type, &node_variant);

  if (!g_str_equal (id_string, GSK_RENDER_NODE_SERIALIZATION_ID))
    {
      g_set_error (error, GSK_SERIALIZATION_ERROR, GSK_SERIALIZATION_UNSUPPORTED_FORMAT,
                   "Data not in GskRenderNode serialization format.");
      goto out;
    }

  if (version != GSK_RENDER_NODE_SERIALIZATION_VERSION)
    {
      g_set_error (error, GSK_SERIALIZATION_ERROR, GSK_SERIALIZATION_UNSUPPORTED_VERSION,
                   "Format version %u not supported.", version);
      goto out;
    }

  tree = gsk_render_tree_new ();
  node = gsk_render_node_deserialize_node (tree, node_type, node_variant, error);
  gsk_render_node_ref (node);
  g_object_unref (tree);

out:
  g_free (id_string);
  g_variant_unref (node_variant);
  g_variant_unref (variant);

  return node;
}

