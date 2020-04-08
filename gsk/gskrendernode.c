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
 * the rectangle set using gsk_render_node_set_bounds().
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
#include "gskrendernodeparserprivate.h"

#include <graphene-gobject.h>

#include <math.h>

#include <gobject/gvaluecollector.h>

G_DEFINE_QUARK (gsk-serialization-error-quark, gsk_serialization_error)

#define GSK_RENDER_NODE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_RENDER_NODE, GskRenderNodeClass))

/**
 * GskRenderNode: (ref-func gsk_render_node_ref) (unref-func gsk_render_node_unref)
 *
 * A node in the render tree.
 */

static void
value_render_node_init (GValue *value)
{
  value->data[0].v_pointer = NULL;
}
 
static void
value_render_node_free_value (GValue *value)
{
  if (value->data[0].v_pointer != NULL)
    gsk_render_node_unref (value->data[0].v_pointer);
}
 
static void
value_render_node_copy_value (const GValue *src,
                              GValue       *dst)
{
  if (src->data[0].v_pointer != NULL)
    dst->data[0].v_pointer = gsk_render_node_ref (src->data[0].v_pointer);
  else
    dst->data[0].v_pointer = NULL;
}
 
static gpointer
value_render_node_peek_pointer (const GValue *value)
{
  return value->data[0].v_pointer;
}

static char *
value_render_node_collect_value (GValue      *value,
                                 guint        n_collect_values,
                                 GTypeCValue *collect_values,
                                 guint        collect_flags)
{
  GskRenderNode *node = collect_values[0].v_pointer;

  if (node == NULL)
    {
      value->data[0].v_pointer = NULL;
      return NULL;
    }

  if (node->parent_instance.g_class == NULL)
    return g_strconcat ("invalid unclassed GskRenderNode pointer for "
                        "value type '",
                        G_VALUE_TYPE_NAME (value),
                        "'",
                        NULL);

  value->data[0].v_pointer = gsk_render_node_ref (node);

  return NULL;
}

static gchar *
value_render_node_lcopy_value (const GValue *value,
                               guint         n_collect_values,
                               GTypeCValue  *collect_values,
                               guint         collect_flags)
{
  GskRenderNode **node_p = collect_values[0].v_pointer;

  if (node_p == NULL)
    return g_strconcat ("value location for '",
                        G_VALUE_TYPE_NAME (value),
                        "' passed as NULL",
                        NULL);

  if (value->data[0].v_pointer == NULL)
    *node_p = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    *node_p = value->data[0].v_pointer;
  else
    *node_p = gsk_render_node_ref (value->data[0].v_pointer);

  return NULL;
}

static void
gsk_render_node_finalize (GskRenderNode *self)
{
  g_type_free_instance ((GTypeInstance *) self);
}

static void
gsk_render_node_real_draw (GskRenderNode *node,
                           cairo_t       *cr)
{
}

static gboolean
gsk_render_node_real_can_diff (const GskRenderNode *node1,
                               const GskRenderNode *node2)
{
  return FALSE;
}

static void
gsk_render_node_real_diff (GskRenderNode  *node1,
                           GskRenderNode  *node2,
                           cairo_region_t *region)
{
}

static void
gsk_render_node_class_init (GskRenderNodeClass *klass)
{
  klass->node_type = GSK_NOT_A_RENDER_NODE;
  klass->finalize = gsk_render_node_finalize;
  klass->draw = gsk_render_node_real_draw;
  klass->can_diff = gsk_render_node_real_can_diff;
  klass->diff = gsk_render_node_real_diff;
}

static void
gsk_render_node_init (GskRenderNode *self)
{
  g_atomic_ref_count_init (&self->ref_count);
}

GType
gsk_render_node_get_type (void)
{
  static volatile gsize render_node_type__volatile;

  if (g_once_init_enter (&render_node_type__volatile))
    {
      static const GTypeFundamentalInfo finfo = {
        (G_TYPE_FLAG_CLASSED |
         G_TYPE_FLAG_INSTANTIATABLE |
         G_TYPE_FLAG_DERIVABLE |
         G_TYPE_FLAG_DEEP_DERIVABLE),
      };

      static const GTypeValueTable value_table = {
        value_render_node_init,
        value_render_node_free_value,
        value_render_node_copy_value,
        value_render_node_peek_pointer,
        "p",
        value_render_node_collect_value,
        "p",
        value_render_node_lcopy_value,
      };

      const GTypeInfo node_info = {
        /* Class */
        sizeof (GskRenderNodeClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gsk_render_node_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,

        /* Instance */
        sizeof (GskRenderNode),
        0,
        (GInstanceInitFunc) gsk_render_node_init,

        /* GValue */
        &value_table,
      };

      GType render_node_type =
        g_type_register_fundamental (g_type_fundamental_next (),
                                     g_intern_static_string ("GskRenderNode"),
                                     &node_info, &finfo,
                                     G_TYPE_FLAG_ABSTRACT);

      g_once_init_leave (&render_node_type__volatile, render_node_type);
    }

  return render_node_type__volatile;
}

typedef struct
{
  GskRenderNodeType node_type;

  void     (* finalize) (GskRenderNode        *node);
  void     (* draw)     (GskRenderNode        *node,
                         cairo_t              *cr);
  gboolean (* can_diff) (const GskRenderNode  *node1,
                         const GskRenderNode  *node2);
  void     (* diff)     (GskRenderNode        *node1,
                         GskRenderNode        *node2,
                         cairo_region_t       *region);
} RenderNodeClassData;

static void
gsk_render_node_generic_class_init (gpointer g_class,
                                    gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;
  RenderNodeClassData *node_data = class_data;

  /* Mandatory */
  node_class->node_type = node_data->node_type;

  /* Optional */
  if (node_data->finalize != NULL)
    node_class->finalize = node_data->finalize;
  if (node_data->can_diff != NULL)
    node_class->can_diff = node_data->can_diff;

  /* Mandatory */
  node_class->draw = node_data->draw;
  node_class->diff = node_data->diff;

  g_free (node_data);
}

static gboolean
gsk_render_node_can_diff_true (const GskRenderNode *node1,
                               const GskRenderNode *node2)
{
  return TRUE;
}

/*< private >
 * gsk_render_node_type_register_static:
 * @node_name: the name of the node
 * @node_info: type information of the node
 *
 * Registers a new #GskRenderNode type for the given @node_name using
 * the type information in @node_info.
 *
 * Returns: the newly registered GType
 */
GType
gsk_render_node_type_register_static (const char                  *node_name,
                                      const GskRenderNodeTypeInfo *node_info)
{
  GTypeInfo info;

  info.class_size = sizeof (GskRenderNodeClass);
  info.base_init = NULL;
  info.base_finalize = NULL;
  info.class_init = gsk_render_node_generic_class_init;
  info.class_finalize = NULL;

  /* Avoid having a class_init() and a class struct for every GskRenderNode,
   * by passing the various virtual functions and class data when initializing
   * the base class
   */
  info.class_data = g_new (RenderNodeClassData, 1);
  ((RenderNodeClassData *) info.class_data)->node_type = node_info->node_type;
  ((RenderNodeClassData *) info.class_data)->finalize = node_info->finalize;
  ((RenderNodeClassData *) info.class_data)->draw = node_info->draw;
  ((RenderNodeClassData *) info.class_data)->can_diff = node_info->can_diff != NULL
                                                      ? node_info->can_diff
                                                      : gsk_render_node_can_diff_true;
  ((RenderNodeClassData *) info.class_data)->diff = node_info->diff != NULL
                                                  ? node_info->diff
                                                  : gsk_render_node_diff_impossible;

  info.instance_size = node_info->instance_size;
  info.n_preallocs = 0;
  info.instance_init = (GInstanceInitFunc) node_info->instance_init;
  info.value_table = NULL;

  return g_type_register_static (GSK_TYPE_RENDER_NODE, node_name, &info, 0);
}

/*< private >
 * gsk_render_node_alloc:
 * @node_type: the #GskRenderNodeType to instantiate
 *
 * Instantiates a new #GskRenderNode for the given @node_type.
 *
 * Returns: (transfer full) (type GskRenderNode): the newly created #GskRenderNode
 */
gpointer
gsk_render_node_alloc (GskRenderNodeType node_type)
{
  g_return_val_if_fail (node_type > GSK_NOT_A_RENDER_NODE, NULL);
  g_return_val_if_fail (node_type < GSK_RENDER_NODE_TYPE_N_TYPES, NULL);

  g_assert (gsk_render_node_types[node_type] != G_TYPE_INVALID);
  return g_type_create_instance (gsk_render_node_types[node_type]);
}

/**
 * gsk_render_node_ref:
 * @node: a #GskRenderNode
 *
 * Acquires a reference on the given #GskRenderNode.
 *
 * Returns: (transfer full): the #GskRenderNode with an additional reference
 */
GskRenderNode *
gsk_render_node_ref (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  g_atomic_ref_count_inc (&node->ref_count);

  return node;
}

/**
 * gsk_render_node_unref:
 * @node: (transfer full): a #GskRenderNode
 *
 * Releases a reference on the given #GskRenderNode.
 *
 * If the reference was the last, the resources associated to the @node are
 * freed.
 */
void
gsk_render_node_unref (GskRenderNode *node)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));

  if (g_atomic_ref_count_dec (&node->ref_count))
    GSK_RENDER_NODE_GET_CLASS (node)->finalize (node);
}


/**
 * gsk_render_node_get_node_type:
 * @node: a #GskRenderNode
 *
 * Returns the type of the @node.
 *
 * Returns: the type of the #GskRenderNode
 */
GskRenderNodeType
gsk_render_node_get_node_type (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), GSK_NOT_A_RENDER_NODE);

  return GSK_RENDER_NODE_GET_CLASS (node)->node_type;
}

G_GNUC_PURE static inline
GskRenderNodeType
_gsk_render_node_get_node_type (const GskRenderNode *node)
{
  return GSK_RENDER_NODE_GET_CLASS (node)->node_type;
}

/**
 * gsk_render_node_get_bounds:
 * @node: a #GskRenderNode
 * @bounds: (out caller-allocates): return location for the boundaries
 *
 * Retrieves the boundaries of the @node. The node will not draw outside
 * of its boundaries.
 */
void
gsk_render_node_get_bounds (GskRenderNode   *node,
                            graphene_rect_t *bounds)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));
  g_return_if_fail (bounds != NULL);

  graphene_rect_init_from_rect (bounds, &node->bounds);
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
 * the drawing context associated to a #GdkSurface's rendering buffer.
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

  GSK_NOTE (CAIRO, g_message ("Rendering node %s[%p]",
                              g_type_name_from_instance ((GTypeInstance *) node),
                              node));

  GSK_RENDER_NODE_GET_CLASS (node)->draw (node, cr);

#ifdef G_ENABLE_DEBUG
  if (GSK_DEBUG_CHECK (GEOMETRY))
    {
      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
      cairo_rectangle (cr, node->bounds.origin.x - 1, node->bounds.origin.y - 1,
                       node->bounds.size.width + 2, node->bounds.size.height + 2);
      cairo_set_line_width (cr, 2);
      cairo_set_source_rgba (cr, 0, 0, 0, 0.5);
      cairo_stroke (cr);
    }
#endif

  cairo_restore (cr);

  if (cairo_status (cr))
    {
      g_warning ("drawing failure for render node %s: %s",
                 g_type_name_from_instance ((GTypeInstance *) node),
                 cairo_status_to_string (cairo_status (cr)));
    }
}

/*
 * gsk_render_node_can_diff:
 * @node1: a #GskRenderNode
 * @node2: the #GskRenderNode to compare with
 *
 * Checks if two render nodes can be expected to be compared via
 * gsk_render_node_diff(). The node diffing algorithm uses this function
 * to match up similar nodes to compare when trying to minimize the
 * resulting region.
 *
 * Nodes of different type always return %FALSE here.
 *
 * Returns: %TRUE if @node1 and @node2 can be expected to be compared
 **/
gboolean
gsk_render_node_can_diff (const GskRenderNode *node1,
                          const GskRenderNode *node2)
{
  if (node1 == node2)
    return TRUE;

  if (_gsk_render_node_get_node_type (node1) != _gsk_render_node_get_node_type (node2))
    return FALSE;

  return GSK_RENDER_NODE_GET_CLASS (node1)->can_diff (node1, node2);
}

static void
rectangle_init_from_graphene (cairo_rectangle_int_t *cairo,
                              const graphene_rect_t *graphene)
{
  cairo->x = floorf (graphene->origin.x);
  cairo->y = floorf (graphene->origin.y);
  cairo->width = ceilf (graphene->origin.x + graphene->size.width) - cairo->x;
  cairo->height = ceilf (graphene->origin.y + graphene->size.height) - cairo->y;
}

void
gsk_render_node_diff_impossible (GskRenderNode  *node1,
                                 GskRenderNode  *node2,
                                 cairo_region_t *region)
{
  cairo_rectangle_int_t rect;

  rectangle_init_from_graphene (&rect, &node1->bounds);
  cairo_region_union_rectangle (region, &rect);
  rectangle_init_from_graphene (&rect, &node2->bounds);
  cairo_region_union_rectangle (region, &rect);
}

/**
 * gsk_render_node_diff:
 * @node1: a #GskRenderNode
 * @node2: the #GskRenderNode to compare with
 * @region: a #cairo_region_t to add the differences to
 *
 * Compares @node1 and @node2 trying to compute the minimal region of changes.
 * In the worst case, this is the union of the bounds of @node1 and @node2.
 *
 * This function is used to compute the area that needs to be redrawn when
 * the previous contents were drawn by @node1 and the new contents should
 * correspond to @node2. As such, it is important that this comparison is
 * faster than the time it takes to actually do the redraw.
 *
 * Note that the passed in @region may already contain previous results from
 * previous node comparisons, so this function call will only add to it.
 **/
void
gsk_render_node_diff (GskRenderNode  *node1,
                      GskRenderNode  *node2,
                      cairo_region_t *region)
{
  if (node1 == node2)
    return;

  if (_gsk_render_node_get_node_type (node1) != _gsk_render_node_get_node_type (node2))
    return gsk_render_node_diff_impossible (node1, node2, region);

  return GSK_RENDER_NODE_GET_CLASS (node1)->diff (node1, node2, region);
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
 * @error_func: (nullable) (scope call): Callback on parsing errors or %NULL
 * @user_data: (closure error_func): user_data for @error_func
 *
 * Loads data previously created via gsk_render_node_serialize(). For a
 * discussion of the supported format, see that function.
 *
 * Returns: (nullable) (transfer full): a new #GskRenderNode or %NULL on
 *     error.
 **/
GskRenderNode *
gsk_render_node_deserialize (GBytes            *bytes,
                             GskParseErrorFunc  error_func,
                             gpointer           user_data)
{
  GskRenderNode *node = NULL;

  node = gsk_render_node_deserialize_from_bytes (bytes, error_func, user_data);

  return node;
}
