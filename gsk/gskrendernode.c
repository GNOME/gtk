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
 * GskRenderNode: (ref-func gsk_render_node_ref) (unref-func gsk_render_node_unref) (set-value-func gsk_value_set_render_node) (get-value-func gsk_value_get_render_node)
 *
 * The basic block in a scene graph to be rendered using [class@Gsk.Renderer].
 *
 * Each node has a parent, except the top-level node; each node may have
 * children nodes.
 *
 * Each node has an associated drawing surface, which has the size of
 * the rectangle set when creating it.
 *
 * Render nodes are meant to be transient; once they have been associated
 * to a [class@Gsk.Renderer] it's safe to release any reference you have on
 * them. All [class@Gsk.RenderNode]s are immutable, you can only specify their
 * properties during construction.
 */

#include "config.h"

#include "gskrendernodeprivate.h"

#include "gskdebugprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeparserprivate.h"

#include "gdk/gdkcairoprivate.h"
#include "gdk/gdkcolorstateprivate.h"

#include <graphene-gobject.h>

#include <math.h>

#include <gobject/gvaluecollector.h>

/**
 * gsk_serialization_error_quark:
 *
 * Registers an error quark for [class@Gsk.RenderNode] errors.
 *
 * Returns: the error quark
 **/
G_DEFINE_QUARK (gsk-serialization-error-quark, gsk_serialization_error)


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

static char *
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

static gboolean
gsk_render_node_real_can_diff (const GskRenderNode *node1,
                               const GskRenderNode *node2)
{
  return TRUE;
}

static void
gsk_render_node_real_diff (GskRenderNode  *node1,
                           GskRenderNode  *node2,
                           GskDiffData    *data)
{
  gsk_render_node_diff_impossible (node1, node2, data);
}

static gboolean
gsk_render_node_real_get_opaque_rect (GskRenderNode   *node,
                                      graphene_rect_t *out_opaque)
{
  return FALSE;
}

static void
gsk_render_node_class_init (GskRenderNodeClass *klass)
{
  klass->node_type = GSK_NOT_A_RENDER_NODE;
  klass->finalize = gsk_render_node_finalize;
  klass->can_diff = gsk_render_node_real_can_diff;
  klass->diff = gsk_render_node_real_diff;
  klass->get_opaque_rect = gsk_render_node_real_get_opaque_rect;
}

static void
gsk_render_node_init (GskRenderNode *self)
{
  g_atomic_ref_count_init (&self->ref_count);
  self->preferred_depth = GDK_N_DEPTHS; /* illegal value */
}

GType
gsk_render_node_get_type (void)
{
  static gsize render_node_type__volatile;

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

/*< private >
 * gsk_render_node_type_register_static:
 * @node_name: the name of the node
 * @node_info: type information of the node
 *
 * Registers a new `GskRenderNode` type for the given @node_name using
 * the type information in @node_info.
 *
 * Returns: the newly registered GType
 */
GType
gsk_render_node_type_register_static (const char     *node_name,
                                      gsize           instance_size,
                                      GClassInitFunc  class_init)
{
  GTypeInfo info;

  info.class_size = sizeof (GskRenderNodeClass);
  info.base_init = NULL;
  info.base_finalize = NULL;
  info.class_init = class_init;
  info.class_finalize = NULL;
  info.instance_size = instance_size;
  info.n_preallocs = 0;
  info.instance_init = NULL;
  info.value_table = NULL;

  return g_type_register_static (GSK_TYPE_RENDER_NODE, node_name, &info, 0);
}

/*< private >
 * gsk_render_node_alloc:
 * @node_type: the `GskRenderNode` type to instantiate
 *
 * Instantiates a new `GskRenderNode` for the given @node_type.
 *
 * Returns: (transfer full) (type GskRenderNode): the newly created `GskRenderNode`
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
 * @node: a render node
 *
 * Acquires a reference on the given `GskRenderNode`.
 *
 * Returns: (transfer full): the render node with an additional reference
 */
GskRenderNode *
(gsk_render_node_ref) (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  return _gsk_render_node_ref (node);
}

void
_gsk_render_node_unref (GskRenderNode *node)
{
  if G_UNLIKELY (g_atomic_ref_count_dec (&node->ref_count))
    GSK_RENDER_NODE_GET_CLASS (node)->finalize (node);
}


/**
 * gsk_render_node_unref:
 * @node: (transfer full): a render node
 *
 * Releases a reference on the given `GskRenderNode`.
 *
 * If the reference was the last, the resources associated to the @node are
 * freed.
 */
void
(gsk_render_node_unref) (GskRenderNode *node)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));

  _gsk_render_node_unref (node);
}


/**
 * gsk_render_node_get_node_type:
 * @node: a render node
 *
 * Returns the type of the render node.
 *
 * Returns: the type of @node
 */
GskRenderNodeType
(gsk_render_node_get_node_type) (const GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), GSK_NOT_A_RENDER_NODE);

  return GSK_RENDER_NODE_GET_CLASS (node)->node_type;
}

/**
 * gsk_render_node_get_bounds:
 * @node: a render node
 * @bounds: (out caller-allocates): return location for the boundaries
 *
 * Retrieves the boundaries of the @node.
 *
 * The node will not draw outside of its boundaries.
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
gsk_render_node_draw_ccs (GskRenderNode *node,
                          cairo_t       *cr,
                          GdkColorState *ccs)
{
  /* Check that the calling function did pass a correct color state */
  g_assert (ccs == gdk_color_state_get_rendering_color_state (ccs));

  cairo_save (cr);

  GSK_RENDER_NODE_GET_CLASS (node)->draw (node, cr, ccs);

  if (GSK_DEBUG_CHECK (GEOMETRY))
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
      g_warning ("drawing failure for render node %s: %s",
                 g_type_name_from_instance ((GTypeInstance *) node),
                 cairo_status_to_string (cairo_status (cr)));
    }
}

void
gsk_render_node_draw_with_color_state (GskRenderNode *node,
                                       cairo_t       *cr,
                                       GdkColorState *color_state)
{
  GdkColorState *ccs;

  ccs = gdk_color_state_get_rendering_color_state (color_state);

  if (gdk_color_state_equal (color_state, ccs))
    {
      gsk_render_node_draw_ccs (node, cr, ccs);
    }
  else
    {
      cairo_save (cr);
      gdk_cairo_rect (cr, &node->bounds);
      cairo_clip (cr);
      cairo_push_group (cr);

      gsk_render_node_draw_ccs (node, cr, ccs);
      gdk_cairo_surface_convert_color_state (cairo_get_group_target (cr),
                                             ccs,
                                             color_state);
      cairo_pop_group_to_source (cr);
      cairo_paint (cr);
      cairo_restore (cr);
    }
}

/**
 * gsk_render_node_draw:
 * @node: a render node
 * @cr: cairo context to draw to
 *
 * Draws the contents of a render node on a cairo context.
 *
 * Typically, you'll use this function to implement fallback rendering
 * of render nodes on an intermediate Cairo context, instead of using
 * the drawing context associated to a [class@Gdk.Surface]'s rendering buffer.
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

  gsk_render_node_draw_with_color_state (node, cr, GDK_COLOR_STATE_SRGB);
}

/*
 * gsk_render_node_draw_fallback:
 * @node: a render node
 * @cr: cairo context to draw to
 *
 * Like [method@Gsk.RenderNode.draw], but will overlay an error pattern
 * if `GSK_DEBUG=cairo` is enabled.
 *
 * This has two purposes:
 * 1. It allows detecting fallbacks in GPU renderers
 * 2. Application code can use it to detect where it is using Cairo drawing
 *
 * So use this function whenever either of those cases should be detected.
 */
void
gsk_render_node_draw_fallback (GskRenderNode *node,
                               cairo_t       *cr)
{
  gsk_render_node_draw (node, cr);

  if (GSK_DEBUG_CHECK (CAIRO))
    {
      /* pink, black
       * black, pink
       */
      static const guint32 fallback_pixels[] = { 0xFFFF00CC, 0xFF000000,
                                                 0xFF000000, 0xFFFF00CC };
      static const guint32 cairo_pixels[] = { 0xFF9900FF, 0xFF000000,
                                              0xFF000000, 0xFF9900FF };
      const guint32 *pixels;
      cairo_surface_t *surface;

      cairo_save (cr);
      if (GSK_RENDER_NODE_TYPE (node) == GSK_CAIRO_NODE)
        pixels = cairo_pixels;
      else
        pixels = fallback_pixels;
      surface = cairo_image_surface_create_for_data ((guchar *) pixels,
                                                     CAIRO_FORMAT_ARGB32,
                                                     2, 2,
                                                     2 * 4);
      cairo_scale (cr, 10, 10);
      cairo_set_source_surface (cr, surface, 0, 0);
      cairo_pattern_set_filter (cairo_get_source (cr), CAIRO_FILTER_NEAREST);
      cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_REPEAT);
      cairo_paint_with_alpha (cr, 0.6);
      cairo_surface_destroy (surface);
      cairo_restore (cr);
    }
}

/*
 * gsk_render_node_can_diff:
 * @node1: a render node
 * @node2: the render onde to compare with
 *
 * Checks if two render nodes can be expected to be compared via
 * [method@Gsk.RenderNode.diff].
 *
 * The node diffing algorithm uses this function to match up similar
 * nodes to compare when trying to minimize the resulting region.
 *
 * Nodes of different type always return false here.
 *
 * Returns: true if @node1 and @node2 can be expected to be compared
 **/
gboolean
gsk_render_node_can_diff (const GskRenderNode *node1,
                          const GskRenderNode *node2)
{
  if (node1 == node2)
    return TRUE;

  if (gsk_render_node_get_node_type (node1) == gsk_render_node_get_node_type (node2))
    return GSK_RENDER_NODE_GET_CLASS (node1)->can_diff (node1, node2);

  if (gsk_render_node_get_node_type (node1) == GSK_CONTAINER_NODE ||
      gsk_render_node_get_node_type (node2) == GSK_CONTAINER_NODE)
    return TRUE;

  return FALSE;
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
                                 GskDiffData    *data)
{
  cairo_rectangle_int_t rect;

  rectangle_init_from_graphene (&rect, &node1->bounds);
  cairo_region_union_rectangle (data->region, &rect);
  rectangle_init_from_graphene (&rect, &node2->bounds);
  cairo_region_union_rectangle (data->region, &rect);
}

/**
 * gsk_render_node_diff:
 * @node1: a render node
 * @node2: the render node to compare with
 * @data: diff data to use
 *
 * Compares two nodes, trying to compute the minimal region of changes.
 *
 * In the worst case, this is the union of the bounds of @node1 and @node2.
 *
 * This function is used to compute the area that needs to be redrawn when
 * the previous contents were drawn by @node1 and the new contents should
 * correspond to @node2. As such, it is important that this comparison is
 * faster than the time it takes to actually do the redraw.
 *
 * Note that the passed in @region may already contain previous results from
 * previous node comparisons, so this function call will only add to it.
 */
void
gsk_render_node_diff (GskRenderNode  *node1,
                      GskRenderNode  *node2,
                      GskDiffData    *data)
{
  if (node1 == node2)
    return;

  if (gsk_render_node_get_node_type (node1) == gsk_render_node_get_node_type (node2))
    {
      GSK_RENDER_NODE_GET_CLASS (node1)->diff (node1, node2, data);
    }
  else if (gsk_render_node_get_node_type (node1) == GSK_CONTAINER_NODE)
    {
      gsk_container_node_diff_with (node1, node2, data);
    }
  else if (gsk_render_node_get_node_type (node2) == GSK_CONTAINER_NODE)
    {
      gsk_container_node_diff_with (node2, node1, data);
    }
  else
    {
      gsk_render_node_diff_impossible (node1, node2, data);
    }
}

/**
 * gsk_render_node_get_opaque_rect:
 * @self: a render node
 * @out_opaque: (out): return location for the opaque rect
 *
 * Gets an opaque rectangle inside the node that GTK can determine to
 * be fully opaque.
 *
 * There is no guarantee that this is indeed the largest opaque rectangle or
 * that regions outside the rectangle are not opaque. This function is a best
 * effort with that goal.
 *
 * The rectangle will be fully contained in the bounds of the node.
 *
 * Returns: true if part or all of the rendernode is opaque, false if no
 *   opaque region could be found.
 *
 * Since: 4.16
 **/
gboolean
gsk_render_node_get_opaque_rect (GskRenderNode   *self,
                                 graphene_rect_t *out_opaque)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (self), FALSE);
  g_return_val_if_fail (out_opaque != NULL, FALSE);

  if (self->fully_opaque)
    {
      *out_opaque = self->bounds;
      return TRUE;
    }

  return GSK_RENDER_NODE_GET_CLASS (self)->get_opaque_rect (self, out_opaque);
}

/**
 * gsk_render_node_write_to_file:
 * @node: a render node
 * @filename: (type filename): the file to save it to
 * @error: return location for an error
 *
 * This function is equivalent to calling [method@Gsk.RenderNode.serialize]
 * followed by [func@GLib.file_set_contents].
 *
 * See those two functions for details on the arguments.
 *
 * It is mostly intended for use inside a debugger to quickly dump a render
 * node to a file for later inspection.
 *
 * Returns: true if saving was successful
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
 * @error_func: (nullable) (scope call) (closure user_data): callback on parsing errors
 * @user_data: user_data for @error_func
 *
 * Loads data previously created via [method@Gsk.RenderNode.serialize].
 *
 * For a discussion of the supported format, see that function.
 *
 * Returns: (nullable) (transfer full): a new render node
 */
GskRenderNode *
gsk_render_node_deserialize (GBytes            *bytes,
                             GskParseErrorFunc  error_func,
                             gpointer           user_data)
{
  GskRenderNode *node = NULL;

  node = gsk_render_node_deserialize_from_bytes (bytes, error_func, user_data);

  return node;
}

/**
 * gsk_value_set_render_node:
 * @value: a [struct@GObject.Value] initialized with type `GSK_TYPE_RENDER_NODE`
 * @node: a render node
 *
 * Stores the given render node inside a `GValue`.
 *
 * The [struct@GObject.Value] will acquire a reference
 * to the render node.
 *
 * Since: 4.6
 */
void
gsk_value_set_render_node (GValue        *value,
                           GskRenderNode *node)
{
  GskRenderNode *old_node;

  g_return_if_fail (G_VALUE_HOLDS (value, GSK_TYPE_RENDER_NODE));

  old_node = value->data[0].v_pointer;

  if (node != NULL)
    {
      g_return_if_fail (GSK_IS_RENDER_NODE (node));

      value->data[0].v_pointer = gsk_render_node_ref (node);
    }
  else
    {
      value->data[0].v_pointer = NULL;
    }

  if (old_node != NULL)
    gsk_render_node_unref (old_node);
}

/**
 * gsk_value_take_render_node:
 * @value: a [struct@GObject.Value] initialized with type `GSK_TYPE_RENDER_NODE`
 * @node: (transfer full) (nullable): a render node
 *
 * Stores the given render node inside a `GValue`.
 *
 * This function transfers the ownership of the
 * render node to the `GValue`.
 *
 * Since: 4.6
 */
void
gsk_value_take_render_node (GValue        *value,
                            GskRenderNode *node)
{
  GskRenderNode *old_node;

  g_return_if_fail (G_VALUE_HOLDS (value, GSK_TYPE_RENDER_NODE));

  old_node = value->data[0].v_pointer;

  if (node != NULL)
    {
      g_return_if_fail (GSK_IS_RENDER_NODE (node));

      value->data[0].v_pointer = node;
    }
  else
    {
      value->data[0].v_pointer = NULL;
    }

  if (old_node != NULL)
    gsk_render_node_unref (old_node);
}

/**
 * gsk_value_get_render_node:
 * @value: a `GValue` initialized with type `GSK_TYPE_RENDER_NODE`
 *
 * Retrieves the render node stored inside a `GValue`.
 *
 * Returns: (transfer none) (nullable): the render node
 *
 * Since: 4.6
 */
GskRenderNode *
gsk_value_get_render_node (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS (value, GSK_TYPE_RENDER_NODE), NULL);

  return value->data[0].v_pointer;
}

/**
 * gsk_value_dup_render_node:
 * @value: a [struct@GObject.Value] initialized with type `GSK_TYPE_RENDER_NODE`
 *
 * Retrieves the render node stored inside a `GValue`,
 * and acquires a reference to it.
 *
 * Returns: (transfer full) (nullable): the render node
 *
 * Since: 4.6
 */
GskRenderNode *
gsk_value_dup_render_node (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS (value, GSK_TYPE_RENDER_NODE), NULL);

  if (value->data[0].v_pointer == NULL)
    return NULL;

  return gsk_render_node_ref (value->data[0].v_pointer);
}

GdkMemoryDepth
gsk_render_node_get_preferred_depth (const GskRenderNode *node)
{
  return node->preferred_depth;
}

gboolean
gsk_render_node_is_hdr (const GskRenderNode *node)
{
  return node->is_hdr;
}
