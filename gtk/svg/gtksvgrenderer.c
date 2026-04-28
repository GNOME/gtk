/*
 * Copyright © 2025 Red Hat, Inc
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtksvgrendererprivate.h"

#include "gtksvgprivate.h"

#include "gtkenums.h"
#include "gtksymbolicpaintable.h"
#include "gtksnapshotprivate.h"
#include "gsk/gskarithmeticnodeprivate.h"
#include "gsk/gskblendnodeprivate.h"
#include "gsk/gskcolormatrixnodeprivate.h"
#include "gsk/gskcolornodeprivate.h"
#include "gsk/gskcomponenttransfernodeprivate.h"
#include "gsk/gskdisplacementnodeprivate.h"
#include "gsk/gskisolationnodeprivate.h"
#include "gsk/gskrepeatnodeprivate.h"
#include "gsk/gskpathprivate.h"
#include "gsk/gskcontourprivate.h"
#include "gsk/gskrectprivate.h"
#include "gsk/gskroundedrectprivate.h"
#include "gtksvgutilsprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgkeywordprivate.h"
#include "gtksvgnumberprivate.h"
#include "gtksvgnumbersprivate.h"
#include "gtksvgstringprivate.h"
#include "gtksvgstringlistprivate.h"
#include "gtksvgenumprivate.h"
#include "gtksvgfilterrefprivate.h"
#include "gtksvgtransformprivate.h"
#include "gtksvgcolorprivate.h"
#include "gtksvgpaintprivate.h"
#include "gtksvgfilterfunctionsprivate.h"
#include "gtksvgdasharrayprivate.h"
#include "gtksvgpathprivate.h"
#include "gtksvgpathdataprivate.h"
#include "gtksvgclipprivate.h"
#include "gtksvgmaskprivate.h"
#include "gtksvgviewboxprivate.h"
#include "gtksvgcontentfitprivate.h"
#include "gtksvgorientprivate.h"
#include "gtksvglanguageprivate.h"
#include "gtksvghrefprivate.h"
#include "gtksvgtextdecorationprivate.h"
#include "gtksvgpathutilsprivate.h"
#include "gtksvgcolorutilsprivate.h"
#include "gtksvgpangoutilsprivate.h"
#include "gtksvglocationprivate.h"
#include "gtksvgerrorprivate.h"
#include "gtksvgelementtypeprivate.h"
#include "gtksvgpropertyprivate.h"
#include "gtksvgfiltertypeprivate.h"
#include "gtksvgfilterprivate.h"
#include "gtksvgcolorstopprivate.h"
#include "gtksvgelementinternal.h"
#include "gtksvgparserprivate.h"

#include <tgmath.h>
#include <stdint.h>

#include "gdk/gdkrgbaprivate.h"

typedef enum
{
  CLIPPING,
  MASKING,
  RENDERING,
  MARKERS,
} RenderOp;

typedef struct
{
  GtkSvg *svg;
  double width;
  double height;
  const graphene_rect_t *viewport;
  GSList *viewport_stack;
  GtkSnapshot *snapshot;
  GSList *transforms;
  int64_t current_time;
  const GdkRGBA *colors;
  size_t n_colors;
  double weight;
  RenderOp op;
  GSList *op_stack;
  gboolean op_changed;
  int depth;
  uint64_t instance_count;
  GSList *ctx_shape_stack;
  struct {
    gboolean picking;
    graphene_point_t p;
    GSList *points;
    gboolean done;
    SvgElement *clipped;
    SvgElement *picked;
  } picking;
} PaintContext;


static void
compute_viewport_transform (gboolean               none,
                            Align                  align_x,
                            Align                  align_y,
                            MeetOrSlice            meet,
                            const graphene_rect_t *vb,
                            double e_x,          double e_y,
                            double e_width,      double e_height,
                            double *scale_x,     double *scale_y,
                            double *translate_x, double *translate_y)
{
  double sx, sy, tx, ty;

  sx = e_width / vb->size.width;
  sy = e_height / vb->size.height;

  if (!none && meet == MEET)
    sx = sy = MIN (sx, sy);
  else if (!none && meet == SLICE)
    sx = sy = MAX (sx, sy);

  tx = e_x - vb->origin.x * sx;
  ty = e_y - vb->origin.y * sy;

  if (align_x == ALIGN_MID)
    tx += (e_width - vb->size.width * sx) / 2;
  else if (align_x == ALIGN_MAX)
    tx += (e_width - vb->size.width * sx);

  if (align_y == ALIGN_MID)
    ty += (e_height - vb->size.height * sy) / 2;
  else if (align_y == ALIGN_MAX)
    ty += (e_height - vb->size.height * sy);

  *scale_x = sx;
  *scale_y = sy;
  *translate_x = tx;
  *translate_y = ty;
}

static GskPorterDuff
svg_composite_operator_to_gsk (CompositeOperator op)
{
  switch (op)
    {
    case COMPOSITE_OPERATOR_OVER: return GSK_PORTER_DUFF_SOURCE_OVER_DEST;
    case COMPOSITE_OPERATOR_IN: return GSK_PORTER_DUFF_SOURCE_IN_DEST;
    case COMPOSITE_OPERATOR_OUT: return GSK_PORTER_DUFF_SOURCE_OUT_DEST;
    case COMPOSITE_OPERATOR_ATOP: return GSK_PORTER_DUFF_SOURCE_ATOP_DEST;
    case COMPOSITE_OPERATOR_XOR: return GSK_PORTER_DUFF_XOR;
    case COMPOSITE_OPERATOR_LIGHTER: return GSK_PORTER_DUFF_SOURCE; /* Not used */
    case COMPOSITE_OPERATOR_ARITHMETIC: return GSK_PORTER_DUFF_SOURCE; /* Not used */
    default:
      g_assert_not_reached ();
    }
}

static gboolean
svg_writing_mode_is_vertical (WritingMode mode)
{
  const gboolean is_vertical[] = {
    FALSE, TRUE, TRUE,
    FALSE, FALSE, FALSE, FALSE, TRUE, TRUE
  };

  return is_vertical[mode];
}

static PangoFontMap *
get_fontmap (GtkSvg *svg)
{
  if (svg->fontmap)
    return svg->fontmap;
  else
    return pango_cairo_font_map_get_default ();
}

/* {{{ Gradients */

static void
project_point_onto_line (const graphene_point_t *a,
                         const graphene_point_t *b,
                         const graphene_point_t *point,
                         graphene_point_t       *p)
{
  graphene_vec2_t n;
  graphene_vec2_t ap;
  float t;

  if (graphene_point_equal (a, b))
    {
      *p = *a;
      return;
    }

  *p = *point;

  graphene_vec2_init (&n, b->x - a->x, b->y - a->y);
  graphene_vec2_init (&ap, point->x - a->x, point->y - a->y);
  t = graphene_vec2_dot (&n, &ap) / graphene_vec2_dot (&n, &n);
  graphene_point_interpolate (a, b, t, p);
}

static void
transform_gradient_line (GskTransform *transform,
                         const graphene_point_t *start,
                         const graphene_point_t *end,
                         graphene_point_t *start_out,
                         graphene_point_t *end_out)
{
  graphene_point_t s, e, t, e2;

  if (gsk_transform_get_category (transform) == GSK_TRANSFORM_CATEGORY_IDENTITY)
    {
      *start_out = *start;
      *end_out = *end;
      return;
    }

  graphene_point_init (&t, start->x + (end->y - start->y),
                           start->y - (end->x - start->x));

  gsk_transform_transform_point (transform, start, &s);
  gsk_transform_transform_point (transform, end, &e);
  gsk_transform_transform_point (transform, &t, &t);

  /* Now s-t is the normal of the gradient we want to draw
   * To unskew the gradient, shift the start point so s-e is
   * perpendicular to s-t again
   */

  project_point_onto_line (&s, &t, &e, &e2);

  *start_out = e2;
  *end_out = e;
}

/* }}} */
/* {{{ Rendernode utilities */

static GskRenderNode *
skip_debug_node (GskRenderNode *node)
{
  if (gsk_render_node_get_node_type (node) == GSK_DEBUG_NODE)
    return gsk_debug_node_get_child (node);
  else
    return node;
}

static GskRenderNode *
apply_transform (GskRenderNode *node,
                 GskTransform  *transform)
{
  if (transform != NULL)
    {
      node = skip_debug_node (node);

      if (gsk_render_node_get_node_type (node) == GSK_TRANSFORM_NODE)
        {
          GskTransform *tx = gsk_transform_node_get_transform (node);
          GskRenderNode *child = gsk_transform_node_get_child (node);
          GskRenderNode *transformed;

          tx = gsk_transform_transform (gsk_transform_ref (tx), transform);
          transformed = gsk_transform_node_new (child, tx);
          gsk_transform_unref (tx);

          return transformed;
        }
      else
        return gsk_transform_node_new (node, transform);
    }
  else
    return gsk_render_node_ref (node);
}

/* }}} */
/* {{{ Text */

static char *
text_chomp (const char *in,
            gboolean   *lastwasspace)
{
  size_t len = strlen (in);
  GString *ret = g_string_sized_new (len);

  for (size_t i = 0; i < len; i++)
    {
      if (in[i] == '\n')
        continue;
      if (in[i] == '\t' || in[i] == ' ')
        {
          if (*lastwasspace)
            continue;
          g_string_append_c (ret, ' ');
          *lastwasspace = TRUE;
          continue;
        }
      g_string_append_c (ret, in[i]);
      *lastwasspace = FALSE;
    }

  return g_string_free_and_steal (ret);
}

/* }}} */
/* {{{ Rendering */

/* Rendering works by walking the shape tree from the top.
 * For each shape, we set up a 'compositing group' before
 * rendering the content of the shape. Establishing a group
 * involves handling transforms, opacity, filters, blending,
 * masking and clipping.
 *
 * This is the core of the rendering machinery:
 *
 * push_group (shape, context);
 * paint_shape (shape, context);
 * pop_group (shape, context);
 *
 * This process is highly recursive. For example, obtaining
 * the clip path or mask may require rendering a different
 * part of the shape tree (in a special mode).
 */
/* Our paint machinery can be used in different modes - for
 * creating clip paths, masks, for rendering markers, and
 * regular rendering. Since these operation are mutually
 * recursive, we maintain a stack of modes, aka RenderOps.
 *
 * The op_changed flag is used to treat the topmost shape
 * in the new mode differently. This is relevant since
 * markers, clip paths, etc can be nested, and just because
 * we see a marker while in MARKERS mode does not mean
 * that we should render it.
 */
static void
push_op (PaintContext *context,
         RenderOp      op)
{
  context->op_stack = g_slist_prepend (context->op_stack, GUINT_TO_POINTER (context->op));
  context->op = op;
  context->op_changed = TRUE;
}

static void
pop_op (PaintContext *context)
{
  GSList *tos = context->op_stack;

  g_assert (tos != NULL);

  context->op = GPOINTER_TO_UINT (tos->data);
  context->op_stack = context->op_stack->next;
  g_slist_free_1 (tos);
}

/* In certain contexts (use, and markers), paint can be
 * context-fill or context-stroke. Again, this can be
 * recursive, so we have a stack of context shapes.
 */
static void
push_ctx_shape (PaintContext *context,
                SvgElement   *shape)
{
  context->ctx_shape_stack = g_slist_prepend (context->ctx_shape_stack, shape);
}

static void
pop_ctx_shape (PaintContext *context)
{
  GSList *tos = context->ctx_shape_stack;

  g_assert (tos != NULL);
  context->ctx_shape_stack = context->ctx_shape_stack->next;
  g_slist_free_1 (tos);
}

/* Viewports are used to resolve percentages in lengths.
 * Nested svg elements create new viewports, and we have
 * a stack of them.
 */
static void
push_viewport (PaintContext          *context,
               const graphene_rect_t *viewport)
{
  context->viewport_stack = g_slist_prepend (context->viewport_stack,
                                             (gpointer) context->viewport);
  context->viewport = viewport;
}

static const graphene_rect_t *
pop_viewport (PaintContext *context)
{
  GSList *tos = context->viewport_stack;
  const graphene_rect_t *viewport = context->viewport;

  g_assert (tos != NULL);

  context->viewport = (const graphene_rect_t *) tos->data;
  context->viewport_stack = tos->next;
  g_slist_free_1 (tos);

  return viewport;
}

static void
push_transform (PaintContext *context,
                GskTransform *transform)
{
  if (context->picking.picking)
    {
      GskTransform *t;

      graphene_point_t *p = g_new (graphene_point_t, 1);
      graphene_point_init_from_point (p, &context->picking.p);
      context->picking.points = g_slist_prepend (context->picking.points, p);

      t = gsk_transform_invert (gsk_transform_ref (transform));
      gsk_transform_transform_point (t, p, &context->picking.p);
      gsk_transform_unref (t);
    }

  context->transforms = g_slist_prepend (context->transforms,
                                         gsk_transform_ref (transform));
}

static void
pop_transform (PaintContext *context)
{
  GSList *tos = context->transforms;

  g_assert (tos != NULL);

  context->transforms = tos->next;
  gsk_transform_unref ((GskTransform *) tos->data);
  g_slist_free_1 (tos);

  if (context->picking.picking)
    {
      tos = context->picking.points;
      context->picking.points = tos->next;

      context->picking.p = *(graphene_point_t *) tos->data;
      g_free (tos->data);
      g_slist_free_1 (tos);
    }
}

static GskTransform *
context_get_host_transform (PaintContext *context)
{
  GskTransform *transform = NULL;

  for (GSList *l = context->transforms; l; l = l->next)
    {
      GskTransform *t = l->data;

      t = gsk_transform_invert (gsk_transform_ref (t));
      t = gsk_transform_transform (t, transform);
      gsk_transform_unref (transform);
      transform = t;
    }

  return transform;
}

static void push_group   (SvgElement   *shape,
                          PaintContext *context);
static void pop_group    (SvgElement   *shape,
                          PaintContext *context);
static void paint_shape  (SvgElement   *shape,
                          PaintContext *context);
static void fill_shape   (SvgElement   *shape,
                          GskPath      *path,
                          SvgValue     *paint,
                          double        opacity,
                          PaintContext *context);
static void render_shape (SvgElement   *shape,
                          PaintContext *context);

/* {{{ Filters */

static GskRenderNode *
apply_alpha_only (GskRenderNode *child)
{
  GskComponentTransfer *zero;
  GskComponentTransfer *identity;
  GskRenderNode *node;

  zero = gsk_component_transfer_new_linear (0, 0);
  identity = gsk_component_transfer_new_identity ();
  node = gsk_component_transfer_node_new (child, zero, zero, zero, identity);
  gsk_component_transfer_free (identity);
  gsk_component_transfer_free (zero);

  gsk_render_node_unref (child);

  return node;
}

static GskRenderNode *
empty_node (void)
{
  return gsk_container_node_new (NULL, 0);
}

static GskRenderNode *
error_node (const graphene_rect_t *bounds)
{
  GdkRGBA pink = { 255 / 255., 105 / 255., 180 / 255., 1.0 };
  return gsk_color_node_new (&pink, bounds);
}

/* We need to store the filter subregion for each result node,
 * for the sole reason that feTile needs it to size its tile.
 */
typedef struct
{
  int ref_count;
  GskRenderNode *node;
  graphene_rect_t bounds;
} FilterResult;

static FilterResult *
filter_result_new (GskRenderNode         *node,
                   const graphene_rect_t *bounds)
{
  FilterResult *res = g_new0 (FilterResult, 1);
  res->ref_count = 1;
  res->node = gsk_render_node_ref (node);
  if (bounds)
    res->bounds = *bounds;
  else
    gsk_render_node_get_bounds (node, &res->bounds);

  return res;
}

static FilterResult *
filter_result_ref (FilterResult *res)
{
  res->ref_count++;
  return res;
}

static void
filter_result_unref (FilterResult *res)
{
  res->ref_count--;
  if (res->ref_count == 0)
    {
      gsk_render_node_unref (res->node);
      g_free (res);
    }
}

static FilterResult *
get_input_for_ref (SvgValue              *in,
                   const graphene_rect_t *subregion,
                   SvgElement            *shape,
                   PaintContext          *context,
                   GskRenderNode         *source,
                   GHashTable            *results)
{
  GskRenderNode *node;
  FilterResult *res;

  switch (svg_filter_ref_get_type (in))
    {
    case FILTER_REF_DEFAULT_SOURCE:
      return filter_result_ref (g_hash_table_lookup (results, ""));
    case FILTER_REF_SOURCE_GRAPHIC:
      return filter_result_new (source, NULL);
    case FILTER_REF_SOURCE_ALPHA:
      res = filter_result_new (source, NULL);
      res->node = apply_alpha_only (res->node);
      return res;
    case FILTER_REF_BACKGROUND_IMAGE:
      node = gsk_paste_node_new (subregion, 0);
      res = filter_result_new (node, subregion);
      gsk_render_node_unref (node);
      return res;
    case FILTER_REF_BACKGROUND_ALPHA:
      node = gsk_paste_node_new (subregion, 0);
      res = filter_result_new (node, subregion);
      gsk_render_node_unref (node);
      res->node = apply_alpha_only (res->node);
      return res;
    case FILTER_REF_BY_NAME:
      res = g_hash_table_lookup (results, svg_filter_ref_get_ref (in));
      if (res)
        return filter_result_ref (res);
      else
        return filter_result_ref (g_hash_table_lookup (results, ""));
    case FILTER_REF_FILL_PAINT:
    case FILTER_REF_STROKE_PAINT:
      {
        GskPath *path;
        GskPathBuilder *builder;
        SvgValue *paint;
        double opacity;

        if (svg_filter_ref_get_type (in))
          {
            paint = svg_element_get_current_value (shape, SVG_PROPERTY_FILL);
            opacity = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_FILL_OPACITY), 1);
          }
        else
          {
            paint = svg_element_get_current_value (shape, SVG_PROPERTY_STROKE);
            opacity = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_STROKE_OPACITY), 1);
          }

        builder = gsk_path_builder_new ();
        gsk_path_builder_add_rect (builder, subregion);
        path = gsk_path_builder_free_to_path (builder);

        gtk_snapshot_push_collect (context->snapshot);
        fill_shape (shape, path, paint, opacity, context);
        node = gtk_snapshot_pop_collect (context->snapshot);

        if (!node)
          node = empty_node ();

        res = filter_result_new (node, subregion);

        gsk_render_node_unref (node);
        gsk_path_unref (path);
        return res;
      }
    default:
      g_assert_not_reached ();
    }

  return NULL;
}

static void
determine_filter_subregion_from_refs (SvgValue              **refs,
                                      unsigned int            n_refs,
                                      gboolean                is_first,
                                      const graphene_rect_t  *filter_region,
                                      GHashTable             *results,
                                      graphene_rect_t        *subregion)
{
  for (unsigned int i = 0; i < n_refs; i++)
    {
      SvgFilterRefType type = svg_filter_ref_get_type (refs[i]);

      if (type == FILTER_REF_SOURCE_GRAPHIC || type == FILTER_REF_SOURCE_ALPHA ||
          type == FILTER_REF_BACKGROUND_IMAGE || type == FILTER_REF_BACKGROUND_ALPHA ||
          type == FILTER_REF_FILL_PAINT || type == FILTER_REF_STROKE_PAINT ||
          (type == FILTER_REF_DEFAULT_SOURCE && is_first))
        {
          if (i == 0)
            graphene_rect_init_from_rect (subregion, filter_region);
          else
            graphene_rect_union (filter_region, subregion, subregion);
        }
      else
        {
          FilterResult *res;

          if (type == FILTER_REF_DEFAULT_SOURCE)
            res = g_hash_table_lookup (results, "");
          else if (type == FILTER_REF_BY_NAME)
            res = g_hash_table_lookup (results, svg_filter_ref_get_ref (refs[i]));
          else
            g_assert_not_reached ();

          if (res)
            {
              if (i == 0)
                graphene_rect_init_from_rect (subregion, &res->bounds);
              else
                graphene_rect_union (&res->bounds, subregion, subregion);
            }
          else
            {
              if (i == 0)
                graphene_rect_init_from_rect (subregion, filter_region);
              else
                graphene_rect_union (filter_region, subregion, subregion);
            }
        }
    }
}

static gboolean
determine_filter_subregion (SvgFilter             *f,
                            SvgElement            *filter,
                            unsigned int           idx,
                            const graphene_rect_t *bounds,
                            const graphene_rect_t *viewport,
                            const graphene_rect_t *filter_region,
                            GHashTable            *results,
                            graphene_rect_t       *subregion)
{
  SvgFilterType type = svg_filter_get_type (f);
  gboolean x_set, y_set, w_set, h_set;

  if (type == SVG_FILTER_MERGE_NODE ||
      type == SVG_FILTER_FUNC_R ||
      type == SVG_FILTER_FUNC_G ||
      type == SVG_FILTER_FUNC_B ||
      type == SVG_FILTER_FUNC_A)
    {
      g_error ("Can't get subregion for %s\n", svg_filter_type_get_name (type));
      return FALSE;
    }

  x_set = svg_filter_is_specified (f, SVG_PROPERTY_FE_X);
  y_set = svg_filter_is_specified (f, SVG_PROPERTY_FE_Y);
  w_set = svg_filter_is_specified (f, SVG_PROPERTY_FE_WIDTH);
  h_set = svg_filter_is_specified (f, SVG_PROPERTY_FE_HEIGHT);

  if (x_set || y_set || w_set || h_set)
    {
      graphene_rect_init_from_rect (subregion, filter_region);

      if (x_set)
        {
          SvgValue *n = svg_filter_get_current_value (f, SVG_PROPERTY_FE_X);
          if (svg_enum_get (filter->current[SVG_PROPERTY_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
            subregion->origin.x = bounds->origin.x + svg_number_get (n, 1) * bounds->size.width;
          else
            subregion->origin.x = viewport->origin.x + svg_number_get (n, viewport->size.width);
        }

      if (y_set)
        {
          SvgValue *n = svg_filter_get_current_value (f, SVG_PROPERTY_FE_Y);
          if (svg_enum_get (filter->current[SVG_PROPERTY_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
            subregion->origin.y = bounds->origin.y + svg_number_get (n, 1) * bounds->size.height;
          else
            subregion->origin.y = viewport->origin.y + svg_number_get (n, viewport->size.height);
        }

      if (w_set)
        {
          SvgValue *n = svg_filter_get_current_value (f, SVG_PROPERTY_FE_WIDTH);
          if (svg_enum_get (filter->current[SVG_PROPERTY_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
            subregion->size.width = svg_number_get (n, 1) * bounds->size.width;
          else
            subregion->size.width = svg_number_get (n, viewport->size.width);
        }

      if (h_set)
        {
          SvgValue *n = svg_filter_get_current_value (f, SVG_PROPERTY_FE_HEIGHT);
          if (svg_enum_get (filter->current[SVG_PROPERTY_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
            subregion->size.height = svg_number_get (n, 1) * bounds->size.height;
          else
            subregion->size.height = svg_number_get (n, viewport->size.height);
        }
    }
  else
    {
      gboolean is_first = idx == 0;

      switch (type)
        {
        case SVG_FILTER_FLOOD:
        case SVG_FILTER_IMAGE:
          /* zero inputs */
          graphene_rect_init_from_rect (subregion, filter_region);
          break;

        case SVG_FILTER_TILE:
        case SVG_FILTER_MERGE_NODE:
        case SVG_FILTER_FUNC_R:
        case SVG_FILTER_FUNC_G:
        case SVG_FILTER_FUNC_B:
        case SVG_FILTER_FUNC_A:
          /* special case */
          graphene_rect_init_from_rect (subregion, filter_region);
          break;

        case SVG_FILTER_BLUR:
        case SVG_FILTER_COLOR_MATRIX:
        case SVG_FILTER_COMPONENT_TRANSFER:
        case SVG_FILTER_DROPSHADOW:
        case SVG_FILTER_OFFSET:
          {
            SvgValue *ref = svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN);

            determine_filter_subregion_from_refs (&ref, 1, is_first, filter_region, results, subregion);
          }
          break;

        case SVG_FILTER_BLEND:
        case SVG_FILTER_COMPOSITE:
        case SVG_FILTER_DISPLACEMENT:
          {
            SvgValue *refs[2];

            refs[0] = svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN);
            refs[1] = svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN2);

            determine_filter_subregion_from_refs (refs, 2, is_first, filter_region, results, subregion);
          }
          break;

        case SVG_FILTER_MERGE:
          {
            SvgValue **refs;
            unsigned int n_refs;

            refs = g_newa (SvgValue *, filter->filters->len);
            n_refs = 0;
            for (idx++; idx < filter->filters->len; idx++)
              {
                SvgFilter *ff = g_ptr_array_index (filter->filters, idx);

                if (svg_filter_get_type (ff) != SVG_FILTER_MERGE_NODE)
                  break;

                refs[n_refs] = svg_filter_get_current_value (ff, SVG_PROPERTY_FE_IN);
                n_refs++;
              }

            determine_filter_subregion_from_refs (refs, n_refs, is_first, filter_region, results, subregion);
          }
          break;

        default:
          g_assert_not_reached ();
        }
    }

  return gsk_rect_intersection (filter_region, subregion, subregion);
}

static GskRenderNode *
apply_filter_tree (SvgElement    *shape,
                   SvgElement    *filter,
                   PaintContext  *context,
                   GskRenderNode *source)
{
  graphene_rect_t filter_region;
  GHashTable *results;
  graphene_rect_t bounds, rect;
  GdkColorState *color_state;

  if (filter->filters->len == 0)
    return empty_node ();

  if (!svg_element_get_current_bounds (shape, context->viewport, context->svg, &bounds))
    return gsk_render_node_ref (source);

  if (svg_enum_get (filter->current[SVG_PROPERTY_BOUND_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
    {
      if (bounds.size.width == 0 || bounds.size.height == 0)
        return empty_node ();

      filter_region.origin.x = bounds.origin.x + svg_number_get (filter->current[SVG_PROPERTY_X], 1) * bounds.size.width;
      filter_region.origin.y = bounds.origin.y + svg_number_get (filter->current[SVG_PROPERTY_Y], 1) * bounds.size.height;
      filter_region.size.width = svg_number_get (filter->current[SVG_PROPERTY_WIDTH], 1) * bounds.size.width;
      filter_region.size.height = svg_number_get (filter->current[SVG_PROPERTY_HEIGHT], 1) * bounds.size.height;
    }
  else
    {
      filter_region.origin.x = svg_number_get (filter->current[SVG_PROPERTY_X], context->viewport->size.width);
      filter_region.origin.y = svg_number_get (filter->current[SVG_PROPERTY_Y], context->viewport->size.height);
      filter_region.size.width = svg_number_get (filter->current[SVG_PROPERTY_WIDTH], context->viewport->size.width);
      filter_region.size.height = svg_number_get (filter->current[SVG_PROPERTY_HEIGHT], context->viewport->size.height);
    }

  results = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) filter_result_unref);

  if (!gsk_rect_intersection (&filter_region, &source->bounds, &rect))
    return empty_node ();

  if (!gsk_rect_equal (&filter_region, &rect))
    {
      GskRenderNode *pad, *padded;

      pad = gsk_color_node_new (&GDK_RGBA_TRANSPARENT, &filter_region);
      padded = gsk_container_node_new ((GskRenderNode*[]) { pad, source }, 2);
      g_hash_table_insert (results, (gpointer) "", filter_result_new (padded, NULL));
      gsk_render_node_unref (padded);
      gsk_render_node_unref (pad);
    }
  else
    g_hash_table_insert (results, (gpointer) "", filter_result_new (source, NULL));

  if (svg_enum_get (filter->current[SVG_PROPERTY_COLOR_INTERPOLATION_FILTERS]) == COLOR_INTERPOLATION_LINEAR)
    color_state = GDK_COLOR_STATE_SRGB_LINEAR;
  else
    color_state = GDK_COLOR_STATE_SRGB;

  for (unsigned int i = 0; i < filter->filters->len; i++)
    {
      SvgFilter *f = g_ptr_array_index (filter->filters, i);
      graphene_rect_t subregion;
      GskRenderNode *result = NULL;

      if (!determine_filter_subregion (f, filter, i, &bounds, context->viewport, &filter_region, results, &subregion))
        {
          graphene_rect_init (&subregion, 0, 0, 0, 0);
          result = gsk_container_node_new (NULL, 0);

          /* Skip dependent filters */
          if (svg_filter_get_type (f) == SVG_FILTER_MERGE)
            {
              for (i++; i < filter->filters->len; i++)
                {
                  SvgFilter *ff = g_ptr_array_index (filter->filters, i);
                  if (svg_filter_get_type (ff) != SVG_FILTER_MERGE_NODE)
                    {
                      i--;
                      break;
                    }
                }
            }
          else if (svg_filter_get_type (f) == SVG_FILTER_COMPONENT_TRANSFER)
            {
              for (i++; i < filter->filters->len; i++)
                {
                  SvgFilter *ff = g_ptr_array_index (filter->filters, i);
                  SvgFilterType t = svg_filter_get_type (ff);
                  if (t != SVG_FILTER_FUNC_R &&
                      t != SVG_FILTER_FUNC_G &&
                      t != SVG_FILTER_FUNC_B &&
                      t != SVG_FILTER_FUNC_A)
                    {
                      i--;
                      break;
                    }
                }
            }

          goto got_result;
        }

      switch (svg_filter_get_type (f))
        {
        case SVG_FILTER_BLUR:
          {
            FilterResult *in;
            double std_dev;
            EdgeMode edge_mode;
            GskRenderNode *child;
            SvgValue *num;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);

            num = svg_filter_get_current_value (f, SVG_PROPERTY_FE_STD_DEV);
            if (svg_numbers_get_length (num) == 2 &&
                svg_numbers_get (num, 0, 1) != svg_numbers_get (num, 1, 1))
              gtk_svg_rendering_error (context->svg,
                                       "Separate x/y values for stdDeviation not supported");
            std_dev = svg_numbers_get (num, 0, 1);

            edge_mode = svg_enum_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_BLUR_EDGE_MODE));

            switch (edge_mode)
              {
             case EDGE_MODE_DUPLICATE:
               child = gsk_repeat_node_new2 (&filter_region, in->node, &in->bounds, GSK_REPEAT_PAD);
               break;
             case EDGE_MODE_WRAP:
               child = gsk_repeat_node_new2 (&filter_region, in->node, &in->bounds, GSK_REPEAT_REPEAT);
               break;
             case EDGE_MODE_NONE:
               child = gsk_render_node_ref (in->node);
               break;
             default:
               g_assert_not_reached ();
             }
             if (std_dev < 0)
               {
                 gtk_svg_rendering_error (context->svg, "stdDeviation < 0");
                 result = gsk_render_node_ref (child);
               }
             else
               {
                 result = gsk_blur_node_new (child, 2 * std_dev);
               }
             gsk_render_node_unref (child);
             filter_result_unref (in);
          }
          break;

        case SVG_FILTER_FLOOD:
          {
            SvgValue *color = svg_filter_get_current_value (f, SVG_PROPERTY_FE_COLOR);
            SvgValue *alpha = svg_filter_get_current_value (f, SVG_PROPERTY_FE_OPACITY);
            GdkColor c;

            gdk_color_init_copy (&c, svg_color_get_color (color));
            c.alpha *= svg_number_get (alpha, 1);
            result = gsk_color_node_new2 (&c, &subregion);
            gdk_color_finish (&c);
          }
          break;

        case SVG_FILTER_MERGE:
          {
            GskRenderNode **children;
            unsigned int n_children;

            children = g_new0 (GskRenderNode *, filter->filters->len);
            n_children = 0;
            for (i++; i < filter->filters->len; i++)
              {
                SvgFilter *ff = g_ptr_array_index (filter->filters, i);
                FilterResult *in;

                if (svg_filter_get_type (ff) != SVG_FILTER_MERGE_NODE)
                  {
                    i--;
                    break;
                  }

                in = get_input_for_ref (svg_filter_get_current_value (ff, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);

                children[n_children] = gsk_render_node_ref (in->node);
                n_children++;
                filter_result_unref (in);
              }
            result = gsk_container_node_new (children, n_children);
            for (unsigned int j = 0; j < n_children; j++)
              gsk_render_node_unref (children[j]);
            g_free (children);
          }
          break;

        case SVG_FILTER_MERGE_NODE:
          break;

        case SVG_FILTER_BLEND:
          {
            FilterResult *in, *in2;
            GskBlendMode blend_mode;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);
            in2 = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN2), &subregion, shape, context, source, results);
            blend_mode = svg_enum_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_BLEND_MODE));

            result = gsk_blend_node_new2 (in2->node, in->node, color_state, blend_mode);

            filter_result_unref (in);
            filter_result_unref (in2);
          }
          break;

        case SVG_FILTER_OFFSET:
          {
            FilterResult *in;
            double dx, dy;
            GskTransform *transform;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);

            if (svg_enum_get (filter->current[SVG_PROPERTY_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
              {
                dx = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DX), 1) * bounds.size.width;
                dy = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DY), 1) * bounds.size.height;
              }
            else
              {
                dx = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DX), 1);
                dy = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DY), 1);
              }

            transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (dx, dy));
            result = apply_transform (in->node, transform);
            filter_result_unref (in);
            gsk_transform_unref (transform);
          }
          break;

        case SVG_FILTER_COMPOSITE:
          {
            FilterResult *in, *in2;
            CompositeOperator svg_op;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);
            in2 = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN2), &subregion, shape, context, source, results);
            svg_op = svg_enum_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_COMPOSITE_OPERATOR));

            if (svg_op == COMPOSITE_OPERATOR_LIGHTER)
              {
                gtk_svg_rendering_error (context->svg,
                                         "lighter composite operator not supported");
                result = gsk_container_node_new ((GskRenderNode*[]) { in2->node, in->node }, 2);
              }
            else if (svg_op == COMPOSITE_OPERATOR_ARITHMETIC)
              {
                float k1, k2, k3, k4;

                k1 = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_COMPOSITE_K1), 1);
                k2 = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_COMPOSITE_K2), 1);
                k3 = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_COMPOSITE_K3), 1);
                k4 = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_COMPOSITE_K4), 1);

                result = gsk_arithmetic_node_new (&subregion, in->node, in2->node, color_state, k1, k2, k3, k4);
              }
            else if (svg_op == COMPOSITE_OPERATOR_OVER)
              {
                result = gsk_container_node_new ((GskRenderNode*[]) { in2->node, in->node }, 2);
              }
            else if (svg_op == COMPOSITE_OPERATOR_IN)
              {
                result = gsk_mask_node_new (in->node, in2->node, GSK_MASK_MODE_ALPHA);
              }
            else if (svg_op == COMPOSITE_OPERATOR_OUT)
              {
                result = gsk_mask_node_new (in->node, in2->node, GSK_MASK_MODE_INVERTED_ALPHA);
              }
            else
              {
                graphene_rect_t mask_bounds;
                GskRenderNode *comp, *mask, *container;
                GskPorterDuff op;
                GskIsolation features;

                op = svg_composite_operator_to_gsk (svg_op);

                graphene_rect_union (&in->bounds, &in2->bounds, &mask_bounds);
                mask = gsk_color_node_new (&(GdkRGBA) { 0, 0, 0, 1 }, &mask_bounds);

                comp = gsk_composite_node_new (in->node, mask, op);
                container = gsk_container_node_new ((GskRenderNode*[]) { in2->node, comp }, 2);
                features = gsk_isolation_features_simplify_for_node (GSK_ISOLATION_ALL, container);
                if (features)
                  {
                    result = gsk_isolation_node_new (container, features);
                    gsk_render_node_unref (container);
                  }
                else
                  result = container;

                gsk_render_node_unref (comp);
                gsk_render_node_unref (mask);
              }

            filter_result_unref (in);
            filter_result_unref (in2);
          }
          break;

        case SVG_FILTER_TILE:
          {
            FilterResult *in;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);

            result = gsk_repeat_node_new (&subregion, in->node, &in->bounds);

            filter_result_unref (in);
          }
          break;

        case SVG_FILTER_COMPONENT_TRANSFER:
          {
            GskComponentTransfer *r = gsk_component_transfer_new_identity ();
            GskComponentTransfer *g = gsk_component_transfer_new_identity ();
            GskComponentTransfer *b = gsk_component_transfer_new_identity ();
            GskComponentTransfer *a = gsk_component_transfer_new_identity ();
            FilterResult *in;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);

            for (i++; i < filter->filters->len; i++)
              {
                SvgFilter *ff = g_ptr_array_index (filter->filters, i);
                SvgFilterType t = svg_filter_get_type (ff);
                if (t == SVG_FILTER_FUNC_R)
                  {
                    gsk_component_transfer_free (r);
                    r = svg_filter_get_component_transfer (ff);
                  }
                else if (t == SVG_FILTER_FUNC_G)
                  {
                    gsk_component_transfer_free (g);
                    g = svg_filter_get_component_transfer (ff);
                  }
                else if (t == SVG_FILTER_FUNC_B)
                  {
                    gsk_component_transfer_free (b);
                    b = svg_filter_get_component_transfer (ff);
                  }
                else if (t == SVG_FILTER_FUNC_A)
                  {
                    gsk_component_transfer_free (a);
                    a = svg_filter_get_component_transfer (ff);
                  }
                else
                  {
                    i--;
                    break;
                  }
              }

            result = gsk_component_transfer_node_new2 (in->node, color_state, r, g, b, a);

            gsk_component_transfer_free (r);
            gsk_component_transfer_free (g);
            gsk_component_transfer_free (b);
            gsk_component_transfer_free (a);
            filter_result_unref (in);
          }
          break;

        case SVG_FILTER_FUNC_R:
        case SVG_FILTER_FUNC_G:
        case SVG_FILTER_FUNC_B:
        case SVG_FILTER_FUNC_A:
          break;

        case SVG_FILTER_COLOR_MATRIX:
          {
            FilterResult *in;
            graphene_matrix_t matrix;
            graphene_vec4_t offset;
            GskRenderNode *node;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);
            color_matrix_type_get_color_matrix (svg_enum_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_COLOR_MATRIX_TYPE)),
                                                svg_filter_get_current_value (f, SVG_PROPERTY_FE_COLOR_MATRIX_VALUES),
                                                &matrix, &offset);

            node = skip_debug_node (in->node);
            if (gsk_render_node_get_node_type (node) == GSK_COLOR_NODE)
              {
                const GdkColor *color = gsk_color_node_get_gdk_color (node);
                GdkColor new_color;

                color_apply_color_matrix (color, color_state, &matrix, &offset,  &new_color);
                result = gsk_color_node_new2 (&new_color, &node->bounds);
                gdk_color_finish (&new_color);
              }
            else
              result = gsk_color_matrix_node_new2 (&in->node->bounds, in->node, color_state, &matrix, &offset);

            filter_result_unref (in);
          }
          break;

        case SVG_FILTER_DROPSHADOW:
          {
            FilterResult *in;
            double std_dev;
            double dx, dy;
            SvgValue *color;
            SvgValue *alpha;
            GskShadowEntry shadow;
            SvgValue *num;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);
            num = svg_filter_get_current_value (f, SVG_PROPERTY_FE_STD_DEV);
            if (svg_numbers_get_length (num) == 2 &&
                svg_numbers_get (num, 0, 1) != svg_numbers_get (num, 1, 1))
              gtk_svg_rendering_error (context->svg,
                                       "Separate x/y values for stdDeviation not supported");
            std_dev = svg_numbers_get (num, 0, 1);

            if (svg_enum_get (filter->current[SVG_PROPERTY_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
              {
                dx = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DX), 1) * bounds.size.width;
                dy = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DY), 1) * bounds.size.height;
              }
            else
              {
                dx = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DX), 1);
                dy = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DY), 1);
              }

            color = svg_filter_get_current_value (f, SVG_PROPERTY_FE_COLOR);
            alpha = svg_filter_get_current_value (f, SVG_PROPERTY_FE_OPACITY);

            gdk_color_init_copy (&shadow.color, svg_color_get_color (color));
            shadow.color.alpha *= svg_number_get (alpha, 1);
            shadow.offset.x = dx;
            shadow.offset.y = dy;
            shadow.radius = 2 * std_dev;

            result = gsk_shadow_node_new2 (in->node, &shadow, 1);

            gdk_color_finish (&shadow.color);

            filter_result_unref (in);
          }
          break;

        case SVG_FILTER_IMAGE:
          {
            SvgValue *href = svg_filter_get_current_value (f, SVG_PROPERTY_FE_IMAGE_HREF);
            SvgValue *cf = svg_filter_get_current_value (f, SVG_PROPERTY_FE_IMAGE_CONTENT_FIT);
            GskTransform *transform;
            GskRenderNode *node;

            if (svg_href_get_kind (href) == HREF_NONE)
              {
                result = empty_node ();
              }
            else if (svg_href_get_texture (href) != NULL)
              {
                graphene_rect_t vb;
                double sx, sy, tx, ty;

                graphene_rect_init (&vb,
                                    0, 0,
                                    gdk_texture_get_width (svg_href_get_texture (href)),
                                    gdk_texture_get_height (svg_href_get_texture (href)));

                compute_viewport_transform (svg_content_fit_is_none (cf),
                                            svg_content_fit_get_align_x (cf),
                                            svg_content_fit_get_align_y (cf),
                                            svg_content_fit_get_meet (cf),
                                            &vb,
                                            subregion.origin.x, subregion.origin.y,
                                            subregion.size.width, subregion.size.height,
                                            &sx, &sy, &tx, &ty);

                transform = gsk_transform_scale (gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (tx, ty)), sx, sy);

                node = gsk_texture_node_new (svg_href_get_texture (href), &vb);
                result = apply_transform (node, transform);
                gsk_render_node_unref (node);
                gsk_transform_unref (transform);
              }
            else if (svg_href_get_shape (href) != NULL)
              {
                gtk_snapshot_push_collect (context->snapshot);
                render_shape (svg_href_get_shape (href), context);
                node = gtk_snapshot_pop_collect (context->snapshot);
                if (node)
                  {
                    transform = gsk_transform_translate (NULL, &subregion.origin);
                    result = apply_transform (node, transform);
                    gsk_render_node_unref (node);
                    gsk_transform_unref (transform);
                  }
                else
                  {
                    gtk_svg_rendering_error (context->svg, "No content for <feImage>");
                    result = error_node (&subregion);
                  }
              }
            else
              {
                gtk_svg_rendering_error (context->svg, "No content for <feImage>");
                result = error_node (&subregion);
              }
          }
          break;

        case SVG_FILTER_DISPLACEMENT:
          {
            FilterResult *in, *in2;
            GdkColorChannel channels[2];
            graphene_size_t max, scale;
            graphene_point_t offset;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);
            in2 = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN2), &subregion, shape, context, source, results);
            channels[0] = svg_enum_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DISPLACEMENT_X));
            channels[1] = svg_enum_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DISPLACEMENT_Y));
            scale.width = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DISPLACEMENT_SCALE), 1);
            scale.height = scale.width;

            max.width = scale.width / 2;
            max.height = scale.height / 2;
            offset.x = offset.y = 0.5;

            result = gsk_displacement_node_new (&subregion,
                                                in->node, in2->node,
                                                channels,
                                                &max, &scale, &offset);

            filter_result_unref (in);
            filter_result_unref (in2);
          }
          break;

        default:
          g_assert_not_reached ();
        }

got_result:
     if (result)
       {
         GskRenderNode *clipped;
         FilterResult *res;
         const char *id;

         if (graphene_rect_contains_rect (&subregion, &result->bounds))
           clipped = gsk_render_node_ref (result);
         else
           clipped = gsk_clip_node_new (result, &subregion);
         res = filter_result_new (clipped, &subregion);
         gsk_render_node_unref (clipped);

         g_hash_table_replace (results, (gpointer) "", filter_result_ref (res));

         id = svg_string_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_RESULT));
         if (id && *id)
           {
             if (g_hash_table_lookup (results, id))
               gtk_svg_rendering_error (context->svg,
                                        "Duplicate filter result ID %s", id);
             else
               g_hash_table_insert (results, (gpointer) id, filter_result_ref (res));
           }

         filter_result_unref (res);
         gsk_render_node_unref (result);
       }
    }

  {
    FilterResult *out;
    GskRenderNode *result;

    out = g_hash_table_lookup (results, "");
    result = gsk_render_node_ref (out->node);

    g_hash_table_unref (results);

    return result;
  }
}

static GskRenderNode *
apply_filter_functions (SvgValue      *filter,
                        PaintContext  *context,
                        SvgElement    *shape,
                        GskRenderNode *source)
{
  GskRenderNode *result = gsk_render_node_ref (source);

  for (unsigned int i = 0; i < svg_filter_functions_get_length (filter); i++)
    {
      GskRenderNode *child = result;

      switch (svg_filter_functions_get_kind (filter, i))
        {
        case FILTER_NONE:
          result = gsk_render_node_ref (child);
          break;
        case FILTER_BLUR:
          result = gsk_blur_node_new (child, 2 * svg_filter_functions_get_simple (filter, i));
          break;
        case FILTER_OPACITY:
          result = gsk_opacity_node_new (child, svg_filter_functions_get_simple (filter, i));
          break;
        case FILTER_BRIGHTNESS:
        case FILTER_CONTRAST:
        case FILTER_GRAYSCALE:
        case FILTER_HUE_ROTATE:
        case FILTER_INVERT:
        case FILTER_SATURATE:
        case FILTER_SEPIA:
          {
            graphene_matrix_t matrix;
            graphene_vec4_t offset;

            svg_filter_functions_get_color_matrix (filter, i, &matrix, &offset);
            result = gsk_color_matrix_node_new (child, &matrix, &offset);
          }
          break;
        case FILTER_DROPSHADOW:
          {
            GskShadowEntry shadow;
            double x, y, std_dev;

            svg_filter_functions_get_dropshadow (filter, i, &shadow.color, &x, &y, &std_dev);
            shadow.offset.x = x;
            shadow.offset.y = y;
            shadow.radius = 2 * std_dev;

            result = gsk_shadow_node_new2 (child, &shadow, 1);

            gdk_color_finish (&shadow.color);
          }
          break;
        case FILTER_REF:
          if (svg_filter_functions_get_shape (filter, i) == NULL)
            {
              gsk_render_node_unref (child);
              return gsk_render_node_ref (source);
            }

          result = apply_filter_tree (shape, svg_filter_functions_get_shape (filter, i), context, child);
          break;
        default:
          g_assert_not_reached ();
        }
      gsk_render_node_unref (child);
    }

  return result;
}

/* }}} */
/* {{{ Groups */

static gboolean
needs_copy (SvgElement    *shape,
            PaintContext  *context,
            const char   **reason)
{
  SvgValue *filter = svg_element_get_current_value (shape, SVG_PROPERTY_FILTER);
  SvgValue *blend = svg_element_get_current_value (shape, SVG_PROPERTY_BLEND_MODE);

  if (context->op == CLIPPING)
    return FALSE;

  if (svg_filter_functions_need_backdrop (filter))
    {
      if (reason) *reason = "filter";
      return TRUE;
    }

  if (svg_enum_get (blend) != GSK_BLEND_MODE_DEFAULT)
    {
      if (reason) *reason = "blending";
      return TRUE;
    }

  return FALSE;
}

static gboolean
needs_isolation (SvgElement    *shape,
                 PaintContext  *context,
                 const char   **reason)
{
  SvgValue *isolation = svg_element_get_current_value (shape, SVG_PROPERTY_ISOLATION);
  SvgValue *opacity = svg_element_get_current_value (shape, SVG_PROPERTY_OPACITY);
  SvgValue *blend = svg_element_get_current_value (shape, SVG_PROPERTY_BLEND_MODE);
  SvgValue *filter = svg_element_get_current_value (shape, SVG_PROPERTY_FILTER);
  SvgValue *tf = svg_element_get_current_value (shape, SVG_PROPERTY_TRANSFORM);

  if (context->op == CLIPPING)
    return FALSE;

  if (svg_element_get_type (shape) == SVG_ELEMENT_SVG && svg_element_get_parent (shape) == NULL)
    {
      if (reason) *reason = "toplevel <svg>";
      return TRUE;
    }

  if (context->op == MASKING && context->op_changed && svg_element_get_type (shape) == SVG_ELEMENT_MASK)
    {
      if (reason) *reason = "<mask>";
      return TRUE;
    }

  if (svg_enum_get (isolation) == ISOLATION_ISOLATE)
    {
      if (reason) *reason = "isolate attribute";
      return TRUE;
    }

  if (svg_number_get (opacity, 1) != 1)
    {
      if (reason) *reason = "opacity";
      return TRUE;
    }

  if (svg_enum_get (blend) != GSK_BLEND_MODE_DEFAULT)
    {
      if (reason) *reason = "blending";
      return TRUE;
    }

  if (!svg_filter_functions_is_none (filter))
    {
      if (reason) *reason = "filter";
      return TRUE;
    }

  if (!svg_transform_is_none (tf))
    {
      GskTransform *transform = svg_transform_get_gsk (tf);

      if (gsk_transform_get_category (transform) <= GSK_TRANSFORM_CATEGORY_3D)
        {
          if (reason) *reason = "3D transform";
          gsk_transform_unref (transform);
          return TRUE;
        }

      gsk_transform_unref (transform);
    }

  return FALSE;
}

static gboolean
shape_is_use_target (SvgElement *shape)
{
  return shape->parent != NULL &&
         svg_element_get_type (shape->parent) == SVG_ELEMENT_USE;
}

static void
push_group (SvgElement   *shape,
            PaintContext *context)
{
  SvgValue *filter = svg_element_get_current_value (shape, SVG_PROPERTY_FILTER);
  SvgValue *opacity = svg_element_get_current_value (shape, SVG_PROPERTY_OPACITY);
  SvgValue *clip = svg_element_get_current_value (shape, SVG_PROPERTY_CLIP_PATH);
  SvgValue *mask = svg_element_get_current_value (shape, SVG_PROPERTY_MASK);
  SvgValue *tf = svg_element_get_current_value (shape, SVG_PROPERTY_TRANSFORM);
  SvgValue *blend = svg_element_get_current_value (shape, SVG_PROPERTY_BLEND_MODE);
  SvgValue *fill_rule = svg_element_get_current_value (shape, SVG_PROPERTY_FILL_RULE);

#ifdef DEBUG
  if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
    {
      GtkSvgLocation loc;

      svg_element_get_origin (shape, &loc);
      if (svg_element_get_id (shape))
        gtk_snapshot_push_debug (context->snapshot, "Group for <%s id='%s'> at line %" G_GSIZE_FORMAT, svg_element_type_get_name (svg_element_get_type (shape)), svg_element_get_id (shape), loc.lines);
      else
        gtk_snapshot_push_debug (context->snapshot, "Group for <%s> at line %" G_GSIZE_FORMAT, svg_element_type_get_name (svg_element_get_type (shape)), loc.lines);
    }
#endif

  if (svg_element_get_type (shape) == SVG_ELEMENT_SVG || svg_element_get_type (shape) == SVG_ELEMENT_SYMBOL)
    {
      SvgValue *cf = svg_element_get_current_value (shape, SVG_PROPERTY_CONTENT_FIT);
      SvgValue *overflow = svg_element_get_current_value (shape, SVG_PROPERTY_OVERFLOW);
      double x, y, width, height;
      double sx, sy, tx, ty;
      graphene_rect_t view_box;
      graphene_rect_t *viewport;
      double w, h;

      if (svg_element_get_parent (shape) == NULL)
        {
          x = 0;
          y = 0;
          width = context->svg->current_width;
          height = context->svg->current_height;
          w = context->svg->width;
          h = context->svg->height;
        }
      else
        {
          g_assert (context->viewport != NULL);

          x = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_X), context->viewport->size.width);
          y = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_Y), context->viewport->size.height);

          if (shape_is_use_target (shape))
            {
              SvgElement *use = shape->parent;
              if (svg_element_is_specified (use, SVG_PROPERTY_WIDTH))
                width = svg_number_get (use->current[SVG_PROPERTY_WIDTH], context->viewport->size.width);
              else
                width = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_WIDTH), context->viewport->size.width);
              if (svg_element_is_specified (use, SVG_PROPERTY_HEIGHT))
                height = svg_number_get (use->current[SVG_PROPERTY_HEIGHT], context->viewport->size.height);
              else
                height = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_HEIGHT), context->viewport->size.height);
            }
          else
            {
              width = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_WIDTH), context->viewport->size.width);
              height = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_HEIGHT), context->viewport->size.height);
            }

          w = width;
          h = height;
        }

      if (svg_enum_get (overflow) == OVERFLOW_HIDDEN)
        {
          if (context->picking.picking)
            {
              if (!gsk_rect_contains_point (&GRAPHENE_RECT_INIT (x, y, width, height), &context->picking.p))
                {
                  if (!context->picking.done)
                    context->picking.done = TRUE;
                }
            }

          gtk_snapshot_push_clip (context->snapshot, &GRAPHENE_RECT_INIT (x, y, width, height));
        }

      if (!svg_view_box_get (svg_element_get_current_value (shape, SVG_PROPERTY_VIEW_BOX), &view_box))
        graphene_rect_init (&view_box, 0, 0, w, h);

      viewport = g_new (graphene_rect_t, 1);
      graphene_rect_init_from_rect (viewport, &view_box);

      compute_viewport_transform (svg_content_fit_is_none (cf),
                                  svg_content_fit_get_align_x (cf),
                                  svg_content_fit_get_align_y (cf),
                                  svg_content_fit_get_meet (cf),
                                  &view_box,
                                  x, y, width, height,
                                  &sx, &sy, &tx, &ty);

      push_viewport (context, viewport);

      GskTransform *transform = NULL;
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (tx, ty));
      transform = gsk_transform_scale (transform, sx, sy);
      gtk_snapshot_save (context->snapshot);
      gtk_snapshot_transform (context->snapshot, transform);
      push_transform (context, transform);
      gsk_transform_unref (transform);
    }

  if (svg_element_get_type (shape) != SVG_ELEMENT_CLIP_PATH && !svg_transform_is_none (tf))
    {
      GskTransform *transform = svg_transform_get_gsk (tf);

      if (svg_element_is_specified (shape, SVG_PROPERTY_TRANSFORM_ORIGIN))
        {
          SvgValue *tfo = svg_element_get_current_value (shape, SVG_PROPERTY_TRANSFORM_ORIGIN);
          SvgValue *tfb = svg_element_get_current_value (shape, SVG_PROPERTY_TRANSFORM_BOX);
          graphene_rect_t bounds;
          double x, y;

          switch (svg_enum_get (tfb))
            {
            case TRANSFORM_BOX_CONTENT_BOX:
            case TRANSFORM_BOX_FILL_BOX:
              svg_element_get_current_bounds (shape, context->viewport, context->svg, &bounds);
              break;
            case TRANSFORM_BOX_BORDER_BOX:
            case TRANSFORM_BOX_STROKE_BOX:
              svg_element_get_current_stroke_bounds (shape, context->viewport, context->svg, &bounds);
              break;
            case TRANSFORM_BOX_VIEW_BOX:
              graphene_rect_init_from_rect (&bounds, context->viewport);
              break;
            default:
              g_assert_not_reached ();
            }

          x = svg_numbers_get (tfo, 0, bounds.size.width);
          y = svg_numbers_get (tfo, 1, bounds.size.height);

          transform = gsk_transform_translate (
                          gsk_transform_transform (
                              gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (x, y)),
                              transform),
                          &GRAPHENE_POINT_INIT (-x, -y));
        }

      push_transform (context, transform);
      gtk_snapshot_save (context->snapshot);
      gtk_snapshot_transform (context->snapshot, transform);
      gsk_transform_unref (transform);
    }

  if (svg_element_get_type (shape) == SVG_ELEMENT_USE)
    {
      double x, y;

      x = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_X), context->viewport->size.width);
      y = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_Y), context->viewport->size.height);
      GskTransform *transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (x, y));
      push_transform (context, transform);
      gtk_snapshot_save (context->snapshot);
      gtk_snapshot_transform (context->snapshot, transform);
      gsk_transform_unref (transform);
    }

  if (context->op != CLIPPING)
    {
      const char *reason;

      if (needs_copy (shape, context, &reason))
        {
#ifdef DEBUG
          if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
            gtk_snapshot_push_debug (context->snapshot, "copy for %s", reason);
#endif
          gtk_snapshot_push_copy (context->snapshot);
        }

      if (needs_isolation (shape, context, &reason))
        {
#ifdef DEBUG
          if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
            gtk_snapshot_push_debug (context->snapshot, "isolate for %s", reason);
#endif
          gtk_snapshot_push_isolation (context->snapshot, GSK_ISOLATION_BACKGROUND);
        }

      if (svg_enum_get (blend) != GSK_BLEND_MODE_DEFAULT)
        {
          graphene_rect_t bounds;

          svg_element_get_current_bounds (shape, context->viewport, context->svg, &bounds);

          gtk_snapshot_push_blend (context->snapshot, svg_enum_get (blend));
          gtk_snapshot_append_paste (context->snapshot, &bounds, 0);
          gtk_snapshot_pop (context->snapshot);
        }
    }

  if (svg_clip_get_kind (clip) == CLIP_PATH ||
      (svg_clip_get_kind (clip) == CLIP_URL && svg_clip_get_shape (clip) != NULL))
    {
      /* Clip mask - see language in the spec about 'raw geometry' */
      if (svg_clip_get_kind (clip) == CLIP_PATH)
        {
          GskFillRule rule;

          switch (svg_clip_get_fill_rule (clip))
            {
            case GSK_FILL_RULE_WINDING:
            case GSK_FILL_RULE_EVEN_ODD:
              rule = svg_clip_get_fill_rule (clip);
              break;
            default:
              rule = svg_enum_get (fill_rule);
              break;
            }

          if (context->picking.picking)
            {
              if (!gsk_path_in_fill (svg_clip_get_path (clip), &context->picking.p, rule))
                {
                  if (!context->picking.done)
                    context->picking.clipped = shape;
                }
            }

#ifdef DEBUG
          if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
            gtk_snapshot_push_debug (context->snapshot, "fill or clip for clip");
#endif
          svg_snapshot_push_fill (context->snapshot, svg_clip_get_path (clip), rule);
        }
      else
        {
          SvgElement *clip_shape = svg_clip_get_shape (clip);
          SvgValue *ctf = svg_element_get_current_value (clip_shape, SVG_PROPERTY_TRANSFORM);
          SvgElement *child = NULL;

          /* In the general case, we collect the clip geometry in a mask.
           * We special-case a single shape in the <clipPath> without
           * transforms and translate them to a clip or a fill.
           */
          if (clip_shape->shapes->len > 0)
            child = g_ptr_array_index (clip_shape->shapes, 0);

          if (svg_transform_is_none (ctf) &&
              svg_enum_get (svg_element_get_current_value (clip_shape, SVG_PROPERTY_CONTENT_UNITS)) == COORD_UNITS_USER_SPACE_ON_USE &&
              clip_shape->shapes->len == 1 &&
              (child->type == SVG_ELEMENT_PATH || child->type == SVG_ELEMENT_RECT || child->type == SVG_ELEMENT_CIRCLE) &&
              svg_enum_get (svg_element_get_current_value (child, SVG_PROPERTY_VISIBILITY)) != VISIBILITY_HIDDEN &&
              svg_enum_get (svg_element_get_current_value (child, SVG_PROPERTY_DISPLAY)) != DISPLAY_NONE &&
              svg_clip_get_kind (svg_element_get_current_value (child, SVG_PROPERTY_CLIP_PATH)) == CLIP_NONE &&
              svg_transform_is_none (svg_element_get_current_value (child, SVG_PROPERTY_TRANSFORM)))
            {
              GskPath *path;

              path = svg_element_get_current_path (child, context->viewport);

              if (context->picking.picking)
                {
                  if (!gsk_path_in_fill (path, &context->picking.p, GSK_FILL_RULE_WINDING))
                    {
                      if (!context->picking.done)
                        context->picking.clipped = shape;
                    }
                }

#ifdef DEBUG
              if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
                gtk_snapshot_push_debug (context->snapshot, "fill or clip for clip");
#endif
              svg_snapshot_push_fill (context->snapshot, path, GSK_FILL_RULE_WINDING);

              gsk_path_unref (path);
            }
          else
            {
              push_op (context, CLIPPING);
#ifdef DEBUG
              if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
                gtk_snapshot_push_debug (context->snapshot, "mask for clip");
#endif
              gtk_snapshot_push_mask (context->snapshot, GSK_MASK_MODE_ALPHA);

              if (!svg_transform_is_none (ctf))
                {
                  GskTransform *transform = svg_transform_get_gsk (ctf);

                  if (svg_element_is_specified (clip_shape, SVG_PROPERTY_TRANSFORM_ORIGIN))
                    {
                      SvgValue *tfo = svg_element_get_current_value (clip_shape, SVG_PROPERTY_TRANSFORM_ORIGIN);
                      SvgValue *tfb = svg_element_get_current_value (clip_shape, SVG_PROPERTY_TRANSFORM_BOX);
                      graphene_rect_t bounds;
                      double x, y;

                      switch (svg_enum_get (tfb))
                        {
                        case TRANSFORM_BOX_CONTENT_BOX:
                        case TRANSFORM_BOX_FILL_BOX:
                          svg_element_get_current_bounds (shape, context->viewport, context->svg, &bounds);
                          break;
                        case TRANSFORM_BOX_BORDER_BOX:
                        case TRANSFORM_BOX_STROKE_BOX:
                          svg_element_get_current_stroke_bounds (shape, context->viewport, context->svg, &bounds);
                          break;
                        case TRANSFORM_BOX_VIEW_BOX:
                          graphene_rect_init_from_rect (&bounds, context->viewport);
                          break;
                        default:
                          g_assert_not_reached ();
                        }

                      x = svg_numbers_get (tfo, 0, bounds.size.width);
                      y = svg_numbers_get (tfo, 1, bounds.size.width);

                      transform = gsk_transform_translate (
                                      gsk_transform_transform (
                                          gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (x, y)),
                                          transform),
                                      &GRAPHENE_POINT_INIT (-x, -y));
                    }

                  push_transform (context, transform);
                  gtk_snapshot_save (context->snapshot);
                  gtk_snapshot_transform (context->snapshot, transform);
                  gsk_transform_unref (transform);
                }

              if (svg_enum_get (svg_element_get_current_value (clip_shape, SVG_PROPERTY_CONTENT_UNITS)) == COORD_UNITS_OBJECT_BOUNDING_BOX)
                {
                  graphene_rect_t bounds;

                  GskTransform *transform = NULL;

                  gtk_snapshot_save (context->snapshot);

                  if (svg_element_get_current_bounds (shape, context->viewport, context->svg, &bounds))
                    {
                      transform = gsk_transform_translate (NULL, &bounds.origin);
                      transform = gsk_transform_scale (transform, bounds.size.width, bounds.size.height);
                      gtk_snapshot_transform (context->snapshot, transform);
                    }
                  push_transform (context, transform);
                  gsk_transform_unref (transform);
                }

              render_shape (clip_shape, context);

              if (svg_enum_get (svg_element_get_current_value (clip_shape, SVG_PROPERTY_CONTENT_UNITS)) == COORD_UNITS_OBJECT_BOUNDING_BOX)
                {
                  pop_transform (context);
                  gtk_snapshot_restore (context->snapshot);
                }

              if (!svg_transform_is_none (ctf))
                {
                  pop_transform (context);
                  gtk_snapshot_restore (context->snapshot);
                }

              gtk_snapshot_pop (context->snapshot); /* mask */

              pop_op (context);

              if (context->picking.picking)
                {
                  if (context->picking.done && context->picking.picked != NULL)
                    context->picking.picked = NULL;
                  else
                    context->picking.clipped = shape;
                  context->picking.done = FALSE;
                }
            }
        }
    }

  if (svg_mask_get_kind (mask) != MASK_NONE &&
      svg_mask_get_shape (mask) != NULL &&
      context->op != CLIPPING)
    {
      SvgElement *mask_shape = svg_mask_get_shape (mask);
      gboolean has_clip = FALSE;

      push_op (context, MASKING);

#ifdef DEBUG
      if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
        gtk_snapshot_push_debug (context->snapshot, "mask for masking");
#endif
      gtk_snapshot_push_mask (context->snapshot, svg_enum_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_MASK_TYPE)));

      if (svg_element_is_specified (mask_shape, SVG_PROPERTY_X) ||
          svg_element_is_specified (mask_shape, SVG_PROPERTY_Y) ||
          svg_element_is_specified (mask_shape, SVG_PROPERTY_WIDTH) ||
          svg_element_is_specified (mask_shape, SVG_PROPERTY_HEIGHT))
        {
           graphene_rect_t mask_clip;

          if (svg_enum_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_BOUND_UNITS)) == COORD_UNITS_OBJECT_BOUNDING_BOX)
            {
              graphene_rect_t bounds;

              if (svg_element_get_current_bounds (shape, context->viewport, context->svg, &bounds))
                {
                  mask_clip.origin.x = bounds.origin.x + svg_number_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_X), 1) * bounds.size.width;
                  mask_clip.origin.y = bounds.origin.y + svg_number_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_Y), 1) * bounds.size.height;
                  mask_clip.size.width = svg_number_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_WIDTH), 1) * bounds.size.width;
                  mask_clip.size.height = svg_number_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_HEIGHT), 1) * bounds.size.height;
                  has_clip = TRUE;
                }
            }
          else
            {
              mask_clip.origin.x = svg_number_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_X), context->viewport->size.width);
              mask_clip.origin.y = svg_number_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_Y), context->viewport->size.height);
              mask_clip.size.width = svg_number_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_WIDTH), context->viewport->size.width);
              mask_clip.size.height = svg_number_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_HEIGHT), context->viewport->size.height);
              has_clip = TRUE;
            }

          if (has_clip)
            gtk_snapshot_push_clip (context->snapshot, &mask_clip);
        }

      if (svg_enum_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_CONTENT_UNITS)) == COORD_UNITS_OBJECT_BOUNDING_BOX)
        {
          graphene_rect_t bounds;
          GskTransform *transform = NULL;

          gtk_snapshot_save (context->snapshot);

          if (svg_element_get_current_bounds (shape, context->viewport, context->svg, &bounds))
            {
              transform = gsk_transform_translate (NULL, &bounds.origin);
              transform = gsk_transform_scale (transform, bounds.size.width, bounds.size.height);
              gtk_snapshot_transform (context->snapshot, transform);
            }

          push_transform (context, transform);
          gsk_transform_unref (transform);
        }

      render_shape (mask_shape, context);

      if (svg_enum_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_CONTENT_UNITS)) == COORD_UNITS_OBJECT_BOUNDING_BOX)
        {
          pop_transform (context);
          gtk_snapshot_restore (context->snapshot);
        }

      if (has_clip)
        gtk_snapshot_pop (context->snapshot);

      gtk_snapshot_pop (context->snapshot);

      pop_op (context);
    }

  if (!context->picking.picking &&
      context->op != CLIPPING &&
      svg_element_get_type (shape) != SVG_ELEMENT_MASK)
    {
      if (svg_number_get (opacity, 1) != 1)
        gtk_snapshot_push_opacity (context->snapshot, svg_number_get (opacity, 1));

      if (!svg_filter_functions_is_none (filter))
        gtk_snapshot_push_collect (context->snapshot);
    }
}

static void
pop_group (SvgElement   *shape,
           PaintContext *context)
{
  SvgValue *filter = svg_element_get_current_value (shape, SVG_PROPERTY_FILTER);
  SvgValue *opacity = svg_element_get_current_value (shape, SVG_PROPERTY_OPACITY);
  SvgValue *clip = svg_element_get_current_value (shape, SVG_PROPERTY_CLIP_PATH);
  SvgValue *mask = svg_element_get_current_value (shape, SVG_PROPERTY_MASK);
  SvgValue *tf = svg_element_get_current_value (shape, SVG_PROPERTY_TRANSFORM);
  SvgValue *blend = svg_element_get_current_value (shape, SVG_PROPERTY_BLEND_MODE);

  if (!context->picking.picking &&
      context->op != CLIPPING &&
      svg_element_get_type (shape) != SVG_ELEMENT_MASK)
    {
      if (!svg_filter_functions_is_none (filter))
        {
          GskRenderNode *node, *node2;

          node = gtk_snapshot_pop_collect (context->snapshot);

          if (!node)
            node = empty_node ();

          node2 = apply_filter_functions (filter, context, shape, node);

          gtk_snapshot_append_node (context->snapshot, node2);
          gsk_render_node_unref (node2);
          gsk_render_node_unref (node);
        }

      if (svg_number_get (opacity, 1) != 1)
        gtk_snapshot_pop (context->snapshot);
    }

  if (svg_mask_get_kind (mask) != MASK_NONE &&
      svg_mask_get_shape (mask) != NULL &&
      context->op != CLIPPING)
    {
      gtk_snapshot_pop (context->snapshot);
#ifdef DEBUG
      if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
        gtk_snapshot_pop (context->snapshot);
#endif
    }

  if (svg_clip_get_kind (clip) == CLIP_PATH ||
      (svg_clip_get_kind (clip) == CLIP_URL && svg_clip_get_shape (clip) != NULL))
    {
      gtk_snapshot_pop (context->snapshot);
#ifdef DEBUG
      if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
        gtk_snapshot_pop (context->snapshot);
#endif
    }

  if (context->op != CLIPPING)
    {
      if (svg_enum_get (blend) != GSK_BLEND_MODE_DEFAULT)
        {
          gtk_snapshot_pop (context->snapshot);
        }

      if (needs_isolation (shape, context, NULL))
        {
          gtk_snapshot_pop (context->snapshot);
#ifdef DEBUG
          if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
            gtk_snapshot_pop (context->snapshot);
#endif
        }

      if (needs_copy (shape, context, NULL))
        {
          gtk_snapshot_pop (context->snapshot);
#ifdef DEBUG
          if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
            gtk_snapshot_pop (context->snapshot);
#endif
        }
    }

  if (svg_element_get_type (shape) == SVG_ELEMENT_USE)
    {
      pop_transform (context);
      gtk_snapshot_restore (context->snapshot);
    }

  if (svg_element_get_type (shape) != SVG_ELEMENT_CLIP_PATH && !svg_transform_is_none (tf))
    {
      pop_transform (context);
      gtk_snapshot_restore (context->snapshot);
    }

  if (svg_element_get_type (shape) == SVG_ELEMENT_SVG || svg_element_get_type (shape) == SVG_ELEMENT_SYMBOL)
    {
      SvgValue *overflow = svg_element_get_current_value (shape, SVG_PROPERTY_OVERFLOW);

      g_free ((gpointer) pop_viewport (context));

      pop_transform (context);
      gtk_snapshot_restore (context->snapshot);

      if (svg_enum_get (overflow) == OVERFLOW_HIDDEN)
        gtk_snapshot_pop (context->snapshot);
   }

#ifdef DEBUG
  if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
    gtk_snapshot_pop (context->snapshot);
#endif
}

/* }}} */
/* {{{ Paint servers */

static gboolean
template_type_compatible (SvgElementType type1,
                          SvgElementType type2)
{
  return type1 == type2 ||
         (svg_element_type_is_gradient (type1) && svg_element_type_is_gradient (type2));
}

/* Only return explicitly set values from a template.
 * If we don't have one, we use the initial value for
 * the original paint server.
 */
static SvgValue *
paint_server_get_template_value (SvgElement   *shape,
                                 SvgProperty   attr,
                                 PaintContext *context)
{
  if (!svg_element_is_specified (shape, attr))
    {
      SvgValue *href = svg_element_get_current_value (shape, SVG_PROPERTY_HREF);
      const char *ref = svg_href_get_id (href);

      if (context->depth > NESTING_LIMIT)
        {
          gtk_svg_rendering_error (context->svg,
                                   "excessive rendering depth (> %d) while resolving href %s, aborting",
                                   NESTING_LIMIT,
                                   ref);
          goto fail;
        }

      if (svg_href_get_shape (href))
        {
          SvgElement *template = svg_href_get_shape (href);
          if (template_type_compatible (template->type, svg_element_get_type (shape)))
            {
              SvgValue *ret;

              context->depth++;
              ret = paint_server_get_template_value (template, attr, context);
              context->depth--;

              return ret;
            }

          gtk_svg_invalid_reference (context->svg,
                                     "<%s> can not use a <%s> as template (while resolving href %s)",
                                     svg_element_type_get_name (svg_element_get_type (shape)),
                                     svg_element_type_get_name (template->type),
                                     ref);
        }

      return NULL;
    }

fail:
  return svg_element_get_current_value (shape, attr);
}

static SvgValue *
paint_server_get_current_value (SvgElement   *shape,
                                SvgProperty   attr,
                                PaintContext *context)
{
  SvgValue *value = NULL;

  if (svg_element_is_specified (shape, attr))
    return svg_element_get_current_value (shape, attr);

  value = paint_server_get_template_value (shape, attr, context);
  if (value)
    return value;

  return svg_element_get_current_value (shape, attr);
}

static GPtrArray *
gradient_get_color_stops (SvgElement   *shape,
                          PaintContext *context)
{
  if (shape->color_stops->len == 0)
    {
      SvgValue *href = svg_element_get_current_value (shape, SVG_PROPERTY_HREF);
      const char *ref = svg_href_get_id (href);

      if (context->depth > NESTING_LIMIT)
        {
          gtk_svg_rendering_error (context->svg,
                                   "excessive rendering depth (> %d) while resolving href %s, aborting",
                                   NESTING_LIMIT,
                                   ref);
          goto fail;
        }

      if (svg_href_get_shape (href))
        {
          SvgElement *template = svg_href_get_shape (href);
          if (template_type_compatible (template->type, svg_element_get_type (shape)))
            {
              GPtrArray *ret;
              context->depth++;
              ret = gradient_get_color_stops (template, context);
              context->depth--;
              return ret;
            }

          gtk_svg_invalid_reference (context->svg,
                                     "<%s> can not use a <%s> as template (while collecting color stops)",
                                     svg_element_type_get_name (svg_element_get_type (shape)),
                                     svg_element_type_get_name (template->type));
        }
    }

fail:
  return shape->color_stops;
}

static GskGradient *
gradient_get_gsk_gradient (SvgElement   *gradient,
                           PaintContext *context)
{
  GskGradient *g;
  GPtrArray *color_stops;
  double offset;
  SvgValue *spread_method;
  SvgValue *color_interpolation;

  color_stops = gradient_get_color_stops (gradient, context);

  g = gsk_gradient_new ();
  offset = 0;
  for (unsigned int i = 0; i < color_stops->len; i++)
    {
      SvgColorStop *stop = g_ptr_array_index (color_stops, i);
      SvgValue *stop_color = svg_color_stop_get_current_value (stop, SVG_PROPERTY_STOP_COLOR);
      GdkColor color;

      offset = MAX (svg_number_get (svg_color_stop_get_current_value (stop, SVG_PROPERTY_STOP_OFFSET), 1), offset);
      gdk_color_init_copy (&color, svg_color_get_color (stop_color));
      color.alpha *= svg_number_get (svg_color_stop_get_current_value (stop, SVG_PROPERTY_STOP_OPACITY), 1);
      gsk_gradient_add_stop (g, offset, 0.5, &color);
      gdk_color_finish (&color);
    }

  spread_method = paint_server_get_current_value (gradient, SVG_PROPERTY_SPREAD_METHOD, context);
  gsk_gradient_set_repeat (g, svg_enum_get (spread_method));
  gsk_gradient_set_premultiplied (g, FALSE);

  color_interpolation = paint_server_get_current_value (gradient, SVG_PROPERTY_COLOR_INTERPOLATION, context);
  if (svg_enum_get (color_interpolation) == COLOR_INTERPOLATION_LINEAR)
    gsk_gradient_set_interpolation (g, GDK_COLOR_STATE_SRGB_LINEAR);
  else
    gsk_gradient_set_interpolation (g, GDK_COLOR_STATE_SRGB);

  return g;
}

static GPtrArray *
pattern_get_shapes (SvgElement   *shape,
                    PaintContext *context)
{
  if (shape->shapes->len == 0)
    {
      SvgValue *href = svg_element_get_current_value (shape, SVG_PROPERTY_HREF);
      const char *ref = svg_href_get_id (href);

      if (context->depth > NESTING_LIMIT)
        {
          gtk_svg_rendering_error (context->svg,
                                   "excessive rendering depth (> %d) while resolving href %s, aborting",
                                   NESTING_LIMIT,
                                   ref);

          goto fail;
        }

      if (svg_href_get_shape (href))
        {
          SvgElement *template = svg_href_get_shape (href);
          if (template_type_compatible (template->type, svg_element_get_type (shape)))
            {
              GPtrArray *ret;
              context->depth++;
              ret = pattern_get_shapes (template, context);
              context->depth--;
              return ret;
            }

          gtk_svg_invalid_reference (context->svg,
                                     "<%s> can not use a <%s> as template (while collecting pattern content)",
                                     svg_element_type_get_name (svg_element_get_type (shape)),
                                     svg_element_type_get_name (template->type));
        }
    }

fail:
  return shape->shapes;
}

static gboolean
paint_server_is_skipped (SvgElement            *server,
                         const graphene_rect_t *bounds,
                         PaintContext          *context)
{
  SvgValue *units;

  if (server->type == SVG_ELEMENT_PATTERN)
    {
      SvgValue *width = paint_server_get_current_value (server, SVG_PROPERTY_WIDTH, context);
      SvgValue *height = paint_server_get_current_value (server, SVG_PROPERTY_HEIGHT, context);

      if (svg_number_get (width, 1) == 0 || svg_number_get (height, 1) == 0)
        return TRUE;

      units = paint_server_get_current_value (server, SVG_PROPERTY_BOUND_UNITS, context);
    }
  else
    units = paint_server_get_current_value (server, SVG_PROPERTY_CONTENT_UNITS, context);

  if (svg_enum_get (units) == COORD_UNITS_OBJECT_BOUNDING_BOX)
    return bounds->size.width == 0 || bounds->size.height == 0;
  else
    return FALSE;
}

static void
paint_linear_gradient (SvgElement            *gradient,
                       const graphene_rect_t *bounds,
                       const graphene_rect_t *paint_bounds,
                       PaintContext          *context)
{
  graphene_point_t start, end;
  GskTransform *transform, *gradient_transform;
  GskGradient *g;
  SvgValue *x1 = paint_server_get_current_value (gradient, SVG_PROPERTY_X1, context);
  SvgValue *y1 = paint_server_get_current_value (gradient, SVG_PROPERTY_Y1, context);
  SvgValue *x2 = paint_server_get_current_value (gradient, SVG_PROPERTY_X2, context);
  SvgValue *y2 = paint_server_get_current_value (gradient, SVG_PROPERTY_Y2, context);
  SvgValue *tf = paint_server_get_current_value (gradient, SVG_PROPERTY_TRANSFORM, context);
  SvgValue *units = paint_server_get_current_value (gradient, SVG_PROPERTY_CONTENT_UNITS, context);

  g = gradient_get_gsk_gradient (gradient, context);

  transform = NULL;
  if (svg_enum_get (units) == COORD_UNITS_OBJECT_BOUNDING_BOX)
    {
      transform = gsk_transform_translate (transform, &bounds->origin);
      transform = gsk_transform_scale (transform, bounds->size.width, bounds->size.height);
      graphene_point_init (&start,
                           svg_number_get (x1, 1),
                           svg_number_get (y1, 1));
      graphene_point_init (&end,
                           svg_number_get (x2, 1),
                           svg_number_get (y2, 1));
    }
  else
    {
      graphene_point_init (&start,
                           svg_number_get (x1, context->viewport->size.width),
                           svg_number_get (y1, context->viewport->size.height));
      graphene_point_init (&end,
                           svg_number_get (x2, context->viewport->size.width),
                           svg_number_get (y2, context->viewport->size.height));
    }

  gradient_transform = svg_transform_get_gsk (tf);
  transform = gsk_transform_transform (transform, gradient_transform);
  gsk_transform_unref (gradient_transform);
  transform_gradient_line (transform, &start, &end, &start, &end);
  gsk_transform_unref (transform);

  gtk_snapshot_add_linear_gradient (context->snapshot, paint_bounds, &start, &end, g);

  gsk_gradient_free (g);
}

static void
paint_radial_gradient (SvgElement            *gradient,
                       const graphene_rect_t *bounds,
                       const graphene_rect_t *paint_bounds,
                       PaintContext          *context)
{
  graphene_point_t start_center;
  graphene_point_t end_center;
  double start_radius, end_radius;
  GskTransform *gradient_transform;
  graphene_rect_t gradient_bounds;
  GskGradient *g;
  SvgValue *fx = paint_server_get_current_value (gradient, SVG_PROPERTY_FX, context);
  SvgValue *fy = paint_server_get_current_value (gradient, SVG_PROPERTY_FY, context);
  SvgValue *fr = paint_server_get_current_value (gradient, SVG_PROPERTY_FR, context);
  SvgValue *cx = paint_server_get_current_value (gradient, SVG_PROPERTY_CX, context);
  SvgValue *cy = paint_server_get_current_value (gradient, SVG_PROPERTY_CY, context);
  SvgValue *r = paint_server_get_current_value (gradient, SVG_PROPERTY_R, context);
  SvgValue *tf = paint_server_get_current_value (gradient, SVG_PROPERTY_TRANSFORM, context);
  SvgValue *units = paint_server_get_current_value (gradient, SVG_PROPERTY_CONTENT_UNITS, context);

  g = gradient_get_gsk_gradient (gradient, context);

  gtk_snapshot_save (context->snapshot);

  if (svg_enum_get (units) == COORD_UNITS_OBJECT_BOUNDING_BOX)
    {
      GskTransform *transform = NULL;

      graphene_point_init (&start_center,
                           svg_number_get (fx, 1),
                           svg_number_get (fy, 1));
      start_radius = svg_number_get (fr, 1);

      graphene_point_init (&end_center,
                           svg_number_get (cx, 1),
                           svg_number_get (cy, 1));
      end_radius = svg_number_get (r, 1);

      transform = gsk_transform_translate (transform, &bounds->origin);
      transform = gsk_transform_scale (transform, bounds->size.width, bounds->size.height);
      gtk_snapshot_transform (context->snapshot, transform);
      push_transform (context, transform);

      transform = gsk_transform_invert (transform);
      gsk_transform_transform_bounds (transform, paint_bounds, &gradient_bounds);
      gsk_transform_unref (transform);
    }
  else
    {
      graphene_point_init (&start_center,
                           svg_number_get (fx, context->viewport->size.width),
                           svg_number_get (fy, context->viewport->size.height));
      start_radius = svg_number_get (fr, normalized_diagonal (context->viewport));

      graphene_point_init (&end_center,
                           svg_number_get (cx, context->viewport->size.width),
                           svg_number_get (cy, context->viewport->size.height));
      end_radius = svg_number_get (r, normalized_diagonal (context->viewport));
      graphene_rect_init_from_rect (&gradient_bounds, paint_bounds);
      push_transform (context, NULL);
    }

  gradient_transform = svg_transform_get_gsk (tf);

  if (svg_element_is_specified (gradient, SVG_PROPERTY_TRANSFORM_ORIGIN))
    {
      SvgValue *tfo = gradient->current[SVG_PROPERTY_TRANSFORM_ORIGIN];
      double x, y;

      if (svg_numbers_get_unit (tfo, 0) == SVG_UNIT_PERCENTAGE)
        x = bounds->origin.x + svg_numbers_get (tfo, 0, bounds->size.width);
      else
        x = svg_numbers_get (tfo, 0, 1);

      if (svg_numbers_get_unit (tfo, 1) == SVG_UNIT_PERCENTAGE)
        y = bounds->origin.y + svg_numbers_get (tfo, 1, bounds->size.width);
      else
        y = svg_numbers_get (tfo, 1, 1);

      gradient_transform = gsk_transform_translate (
                      gsk_transform_transform (
                          gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (x, y)),
                          gradient_transform),
                      &GRAPHENE_POINT_INIT (-x, -y));
    }

  gtk_snapshot_transform (context->snapshot, gradient_transform);
  push_transform (context, gradient_transform);

  if (gradient_transform)
    {
      gradient_transform = gsk_transform_invert (gradient_transform);
      if (gradient_transform)
        {
          gsk_transform_transform_bounds (gradient_transform, &gradient_bounds, &gradient_bounds);
          gsk_transform_unref (gradient_transform);
        }
      else
        graphene_rect_init (&gradient_bounds, 0, 0, 0, 0);
    }

  /* If the gradient transform is singular, we might end up with
   * nans or infs in the bounds :(
   */
  if (!isnormal (gradient_bounds.size.width) ||
      !isnormal (gradient_bounds.size.height))
    {
      GskRenderNode *node = gsk_container_node_new (NULL, 0);
      gtk_snapshot_append_node (context->snapshot, node);
      gsk_render_node_unref (node);
    }
  else
    {
      gtk_snapshot_add_radial_gradient (context->snapshot,
                                        &gradient_bounds,
                                        &start_center, start_radius,
                                        &end_center, end_radius,
                                        1,
                                        g);
    }

  gsk_gradient_free (g);

  pop_transform (context);
  pop_transform (context);
  gtk_snapshot_restore (context->snapshot);
}

static void
paint_pattern (SvgElement            *pattern,
               const graphene_rect_t *bounds,
               const graphene_rect_t *paint_bounds,
               PaintContext          *context)
{
  graphene_rect_t pattern_bounds, child_bounds;
  double tx, ty;
  double sx, sy;
  GskTransform *transform;
  double dx, dy;
  SvgValue *bound_units = paint_server_get_current_value (pattern, SVG_PROPERTY_BOUND_UNITS, context);
  SvgValue *content_units = paint_server_get_current_value (pattern, SVG_PROPERTY_CONTENT_UNITS, context);
  SvgValue *x = paint_server_get_current_value (pattern, SVG_PROPERTY_X, context);
  SvgValue *y = paint_server_get_current_value (pattern, SVG_PROPERTY_Y, context);
  SvgValue *width = paint_server_get_current_value (pattern, SVG_PROPERTY_WIDTH, context);
  SvgValue *height = paint_server_get_current_value (pattern, SVG_PROPERTY_HEIGHT, context);
  SvgValue *tf = paint_server_get_current_value (pattern, SVG_PROPERTY_TRANSFORM, context);
  SvgValue *vb = paint_server_get_current_value (pattern, SVG_PROPERTY_VIEW_BOX, context);
  SvgValue *cf = paint_server_get_current_value (pattern, SVG_PROPERTY_CONTENT_FIT, context);
  GPtrArray *shapes;
  graphene_rect_t view_box;

  if (svg_enum_get (bound_units) == COORD_UNITS_OBJECT_BOUNDING_BOX)
    {
      dx = bounds->origin.x + svg_number_get (x, 1) * bounds->size.width;
      dy = bounds->origin.y + svg_number_get (y, 1) * bounds->size.height;
      child_bounds.origin.x = 0;
      child_bounds.origin.y = 0;
      child_bounds.size.width = svg_number_get (width, 1) * bounds->size.width;
      child_bounds.size.height = svg_number_get (height, 1) * bounds->size.height;
    }
  else
    {
      dx = svg_number_get (x, context->viewport->size.width);
      dy = svg_number_get (y, context->viewport->size.height);
      child_bounds.origin.x = 0;
      child_bounds.origin.y = 0;
      child_bounds.size.width = svg_number_get (width, context->viewport->size.width);
      child_bounds.size.height = svg_number_get (height, context->viewport->size.height);
    }

  if (svg_view_box_get (vb, &view_box))
    {
      compute_viewport_transform (svg_content_fit_is_none (cf),
                                  svg_content_fit_get_align_x (cf),
                                  svg_content_fit_get_align_y (cf),
                                  svg_content_fit_get_meet (cf),
                                  &view_box,
                                  child_bounds.origin.x, child_bounds.origin.y,
                                  child_bounds.size.width, child_bounds.size.height,
                                  &sx, &sy, &tx, &ty);
    }
  else
    {
      if (svg_enum_get (content_units) == COORD_UNITS_OBJECT_BOUNDING_BOX)
        {
          tx = 0;
          ty = 0;
          sx = bounds->size.width;
          sy = bounds->size.height;
        }
      else
        {
          sx = sy = 1;
          tx = ty = 0;
        }
    }

  child_bounds.origin.x += dx;
  child_bounds.origin.y += dy;

  tx += dx;
  ty += dy;

  gtk_snapshot_save (context->snapshot);

  transform = svg_transform_get_gsk (tf);

  if (svg_element_is_specified (pattern, SVG_PROPERTY_TRANSFORM_ORIGIN))
    {
      SvgValue *tfo = pattern->current[SVG_PROPERTY_TRANSFORM_ORIGIN];
      double xx, yy;

      xx = svg_numbers_get (tfo, 0, child_bounds.size.width);
      yy = svg_numbers_get (tfo, 1, child_bounds.size.width);

      transform = gsk_transform_translate (
                      gsk_transform_transform (
                          gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (xx, yy)),
                          transform),
                      &GRAPHENE_POINT_INIT (-xx, -yy));
     }

  gtk_snapshot_transform (context->snapshot, transform);
  push_transform (context, transform);

  transform = gsk_transform_invert (transform);
  gsk_transform_transform_bounds (transform, paint_bounds, &pattern_bounds);
  gsk_transform_unref (transform);

  /* If the gradient transform is singular, we might end up with
   * nans or infs in the bounds :(
   */
  if (!isfinite (pattern_bounds.size.width) || pattern_bounds.size.width == 0 ||
      !isfinite (pattern_bounds.size.height) || pattern_bounds.size.height == 0)
    {
      GskRenderNode *node = gsk_container_node_new (NULL, 0);
      gtk_snapshot_append_node (context->snapshot, node);
      gsk_render_node_unref (node);
    }
  else
    {
      gtk_snapshot_push_repeat (context->snapshot, &pattern_bounds, &child_bounds);

      gtk_snapshot_save (context->snapshot);
      transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (tx, ty));
      transform = gsk_transform_scale (transform, sx, sy);
      gtk_snapshot_transform (context->snapshot, transform);
      push_transform (context, transform);
      gsk_transform_unref (transform);

      shapes = pattern_get_shapes (pattern, context);
      for (int i = 0; i < shapes->len; i++)
        {
          SvgElement *s = g_ptr_array_index (shapes, i);
          render_shape (s, context);
        }

      pop_transform (context);
      gtk_snapshot_restore (context->snapshot);

      gtk_snapshot_pop (context->snapshot);
    }

  pop_transform (context);
  gtk_snapshot_restore (context->snapshot);
}

static void
paint_server (SvgValue              *paint,
              const graphene_rect_t *bounds,
              const graphene_rect_t *paint_bounds,
              PaintContext          *context)
{
  SvgElement *server = svg_paint_get_server_shape (paint);

  if (server == NULL ||
      paint_server_is_skipped (server, bounds, context))
    {
      gtk_snapshot_add_color (context->snapshot,
                              svg_paint_get_server_fallback (paint),
                              paint_bounds);
    }
  else if (server->type == SVG_ELEMENT_LINEAR_GRADIENT ||
           server->type == SVG_ELEMENT_RADIAL_GRADIENT)
    {
      GPtrArray *color_stops;

      color_stops = gradient_get_color_stops (server, context);

      if (color_stops->len == 0)
        return;

      if (color_stops->len == 1)
        {
          SvgColorStop *stop = g_ptr_array_index (color_stops, 0);
          SvgValue *stop_color = svg_color_stop_get_current_value (stop, SVG_PROPERTY_STOP_COLOR);
          GdkColor c;

          gdk_color_init_copy (&c, svg_color_get_color (stop_color));
          c.alpha *= svg_number_get (svg_color_stop_get_current_value (stop, SVG_PROPERTY_STOP_OPACITY), 1);
          gtk_snapshot_add_color (context->snapshot, &c, paint_bounds);
          gdk_color_finish (&c);
          return;
        }

      if (server->type == SVG_ELEMENT_LINEAR_GRADIENT)
        paint_linear_gradient (server, bounds, paint_bounds, context);
      else
        paint_radial_gradient (server, bounds, paint_bounds, context);
    }
  else if (server->type == SVG_ELEMENT_PATTERN)
    {
      paint_pattern (server, bounds, paint_bounds, context);
    }
}

/* }}} */
/* {{{ Shapes */

static GskStroke *
shape_create_stroke (SvgElement   *shape,
                     PaintContext *context)
{
  GskStroke *stroke;
  SvgValue *da;

  stroke = svg_element_create_basic_stroke (shape, context->viewport, context->svg->features & GTK_SVG_EXTENSIONS, context->weight);

  da = svg_element_get_current_value (shape, SVG_PROPERTY_STROKE_DASHARRAY);
  if (svg_dash_array_get_kind (da) != DASH_ARRAY_NONE)
    {
      unsigned int len;
      double path_length;
      double length;
      double offset;
      GskPathMeasure *measure;
      gboolean invalid = FALSE;
      float *vals;

      if (svg_element_type_is_text (svg_element_get_type (shape)))
        {
          gtk_svg_rendering_error (context->svg,
                                   "Dashing of stroked text is not supported");
          return stroke;
        }

      measure = svg_element_get_current_measure (shape, context->viewport);
      length = gsk_path_measure_get_length (measure);
      gsk_path_measure_unref (measure);

      path_length = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_PATH_LENGTH), 1);
      if (path_length < 0)
        path_length = length;

      offset = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_STROKE_DASHOFFSET), normalized_diagonal (context->viewport));

      len = svg_dash_array_get_length (da);
      vals = g_newa (float, len);

      if (path_length > 0)
        {
          for (unsigned int i = 0; i < len; i++)
            {
              g_assert (svg_dash_array_get_unit (da, i) != SVG_UNIT_PERCENTAGE);
              vals[i] = svg_dash_array_get (da, i, 1) / path_length * length;
              if (vals[i] < 0)
                invalid = TRUE;
            }

          offset = offset / path_length * length;
        }
      else
        {
          for (unsigned int i = 0; i < len; i++)
            {
              g_assert (svg_dash_array_get_unit (da, i) != SVG_UNIT_PERCENTAGE);
              vals[i] = svg_dash_array_get (da, i, 1);
              if (vals[i] < 0)
                invalid = TRUE;
            }
        }

      if (!invalid)
        {
          gsk_stroke_set_dash (stroke, vals, len);
          gsk_stroke_set_dash_offset (stroke, offset);
        }
    }

  return stroke;
}

static SvgValue *
get_context_paint (SvgValue *paint,
                   GSList   *ctx_stack)
{
  switch (svg_paint_get_kind (paint))
    {
    case PAINT_NONE:
    case PAINT_CURRENT_COLOR:
      break;
    case PAINT_COLOR:
    case PAINT_SERVER:
    case PAINT_SERVER_WITH_FALLBACK:
    case PAINT_SERVER_WITH_CURRENT_COLOR:
    case PAINT_SYMBOLIC:
      return paint;
    case PAINT_CONTEXT_FILL:
    case PAINT_CONTEXT_STROKE:
      if (ctx_stack)
        {
          SvgElement *shape = ctx_stack->data;

          if (svg_paint_get_kind (paint) == PAINT_CONTEXT_FILL)
            paint = svg_element_get_current_value (shape, SVG_PROPERTY_FILL);
          else
            paint = svg_element_get_current_value (shape, SVG_PROPERTY_STROKE);

          return get_context_paint (paint, ctx_stack->next);
        }
      break;
    default:
      g_assert_not_reached ();
    }

  paint = svg_paint_new_none ();
  svg_value_unref (paint);

  return paint;
}

static void
fill_shape (SvgElement   *shape,
            GskPath      *path,
            SvgValue     *paint,
            double        opacity,
            PaintContext *context)
{
  graphene_rect_t bounds;
  GskFillRule fill_rule;

  paint = get_context_paint (paint, context->ctx_shape_stack);
  if (svg_paint_get_kind (paint) == PAINT_NONE)
    return;

  if (!gsk_path_get_bounds (path, &bounds))
    return;

  fill_rule = svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_FILL_RULE));

  switch (svg_paint_get_kind (paint))
    {
    case PAINT_NONE:
      break;
    case PAINT_COLOR:
      {
        GdkColor color;

        gdk_color_init_copy (&color, svg_paint_get_color (paint));
        color.alpha *= opacity;
        svg_snapshot_push_fill (context->snapshot, path, fill_rule);
        gtk_snapshot_add_color (context->snapshot, &color, &bounds);
        gtk_snapshot_pop (context->snapshot);
        gdk_color_finish (&color);
      }
      break;
    case PAINT_SERVER:
    case PAINT_SERVER_WITH_FALLBACK:
    case PAINT_SERVER_WITH_CURRENT_COLOR:
      {
        if (opacity < 1)
          gtk_snapshot_push_opacity (context->snapshot, opacity);

        svg_snapshot_push_fill (context->snapshot, path, fill_rule);
        paint_server (paint, &bounds, &bounds, context);
        gtk_snapshot_pop (context->snapshot);

        if (opacity < 1)
          gtk_snapshot_pop (context->snapshot);
      }
      break;
    case PAINT_CURRENT_COLOR:
    case PAINT_CONTEXT_FILL:
    case PAINT_CONTEXT_STROKE:
    case PAINT_SYMBOLIC:
    default:
      g_assert_not_reached ();
    }
}

static void
stroke_shape (SvgElement   *shape,
              GskPath      *path,
              PaintContext *context)
{
  SvgValue *paint;
  GskStroke *stroke;
  graphene_rect_t bounds;
  graphene_rect_t paint_bounds;
  double opacity;
  VectorEffect effect;
  GskRenderNode *child;
  GskRenderNode *node;

  paint = svg_element_get_current_value (shape, SVG_PROPERTY_STROKE);
  paint = get_context_paint (paint, context->ctx_shape_stack);
  if (svg_paint_get_kind (paint) == PAINT_NONE)
    return;

  if (!gsk_path_get_bounds (path, &bounds))
    return;

  stroke = shape_create_stroke (shape, context);
  if (!gsk_path_get_stroke_bounds (path, stroke, &paint_bounds))
    return;

  opacity = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_STROKE_OPACITY), 1);
  effect = svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_VECTOR_EFFECT));

  switch (svg_paint_get_kind (paint))
    {
    case PAINT_COLOR:
      {
        GdkColor color;
        gdk_color_init_copy (&color, svg_paint_get_color (paint));
        color.alpha *= opacity;
        opacity = 1;
        child = gsk_color_node_new2 (&color, &paint_bounds);
        gdk_color_finish (&color);
      }
      break;
    case PAINT_SERVER:
    case PAINT_SERVER_WITH_FALLBACK:
    case PAINT_SERVER_WITH_CURRENT_COLOR:
      gtk_snapshot_push_collect (context->snapshot);
      paint_server (paint, &bounds, &paint_bounds, context);
      child = gtk_snapshot_pop_collect (context->snapshot);

      if (!child)
        child = empty_node ();
      break;
    case PAINT_NONE:
    case PAINT_CURRENT_COLOR:
    case PAINT_CONTEXT_FILL:
    case PAINT_CONTEXT_STROKE:
    case PAINT_SYMBOLIC:
    default:
      g_assert_not_reached ();
    }

  if (effect == VECTOR_EFFECT_NON_SCALING_STROKE)
    {
      GskTransform *host_transform = NULL;
      GskTransform *user_transform = NULL;
      GskPath *transformed_path = NULL;
      GskRenderNode *transformed_child;
      GskRenderNode *stroke_node;

      host_transform = context_get_host_transform (context);
      user_transform = gsk_transform_invert (gsk_transform_ref (host_transform));
      transformed_path = svg_transform_path (user_transform, path);

      transformed_child = gsk_transform_node_new (child, user_transform);
      stroke_node = gsk_stroke_node_new (transformed_child, transformed_path, stroke);

      node = gsk_transform_node_new (stroke_node, host_transform);

      gsk_render_node_unref (stroke_node);
      gsk_render_node_unref (transformed_child);

      gsk_path_unref (transformed_path);
      gsk_transform_unref (user_transform);
      gsk_transform_unref (host_transform);
    }
  else
    {
      node = gsk_stroke_node_new (child, path, stroke);
    }

  gsk_render_node_unref (child);

  if (opacity < 1)
    {
      GskRenderNode *node2 = gsk_opacity_node_new (node, opacity);
      gsk_render_node_unref (node);
      node = node2;
    }

  gtk_snapshot_append_node (context->snapshot, node);
  gsk_render_node_unref (node);

  gsk_stroke_free (stroke);
}

typedef enum {
  VERTEX_START,
  VERTEX_MID,
  VERTEX_END,
} VertexKind;

/* }}} */
/* {{{ Markers */

static void
paint_marker (SvgElement         *shape,
              GskPath            *path,
              PaintContext       *context,
              const GskPathPoint *point,
              VertexKind          kind)
{
  SvgProperty attrs[] = { SVG_PROPERTY_MARKER_START, SVG_PROPERTY_MARKER_MID, SVG_PROPERTY_MARKER_END };
  SvgValue *href;
  SvgValue *orient;
  SvgValue *units;
  SvgElement *marker;
  double scale;
  graphene_point_t vertex;
  double angle;
  double x, y, width, height;
  SvgValue *vb;
  SvgValue *cf;
  SvgValue *overflow;
  double sx, sy, tx, ty;
  graphene_rect_t view_box;
  SvgValue *v;
  GskTransform *transform = NULL;

  gsk_path_point_get_position (point, path, &vertex);

  href = svg_element_get_current_value (shape, attrs[kind]);
  if (svg_href_get_kind (href) == HREF_NONE)
    return;

  marker = svg_href_get_shape (href);
  if (!marker)
    return;

  orient = marker->current[SVG_PROPERTY_MARKER_ORIENT];
  units = marker->current[SVG_PROPERTY_MARKER_UNITS];

  if (svg_enum_get (units) == MARKER_UNITS_STROKE_WIDTH)
    scale = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_STROKE_WIDTH), 1);
  else
    scale = 1;

  if (svg_orient_get_kind (orient) == ORIENT_AUTO)
    {
      if (kind == VERTEX_START)
        {
          angle = gsk_path_point_get_rotation (point, path, GSK_PATH_TO_END);
          if (svg_orient_get_start_reverse (orient))
            angle += 180;
        }
      else if (kind == VERTEX_END)
        {
          angle = gsk_path_point_get_rotation (point, path, GSK_PATH_FROM_START);
        }
      else
        {
          angle = (gsk_path_point_get_rotation (point, path, GSK_PATH_TO_END) +
                   gsk_path_point_get_rotation (point, path, GSK_PATH_FROM_START)) / 2;
        }
    }
  else
    {
      angle = svg_orient_get_angle (orient);
    }

  vb = marker->current[SVG_PROPERTY_VIEW_BOX];
  cf = marker->current[SVG_PROPERTY_CONTENT_FIT];
  overflow = marker->current[SVG_PROPERTY_OVERFLOW];

  width = svg_number_get (marker->current[SVG_PROPERTY_WIDTH], context->viewport->size.width);
  height = svg_number_get (marker->current[SVG_PROPERTY_HEIGHT], context->viewport->size.height);

  if (width == 0 || height == 0)
    return;

  v = marker->current[SVG_PROPERTY_REF_X];
  if (svg_number_get_unit (v) == SVG_UNIT_PERCENTAGE)
    x = svg_number_get (v, width);
  else if (svg_view_box_get (vb, &view_box))
    x = svg_number_get (v, 100) / view_box.size.width * width;
  else
    x = svg_number_get (v, 100);

  v = marker->current[SVG_PROPERTY_REF_Y];
  if (svg_number_get_unit (v) == SVG_UNIT_PERCENTAGE)
    y = svg_number_get (v, height);
  else if (svg_view_box_get (vb, &view_box))
    y = svg_number_get (v, 100) / view_box.size.height * height;
  else
    y = svg_number_get (v, 100);

  width *= scale;
  height *= scale;
  x *= scale;
  y *= scale;

  if (!svg_view_box_get (vb, &view_box))
    graphene_rect_init (&view_box, 0, 0, width, height);

  compute_viewport_transform (svg_content_fit_is_none (cf),
                              svg_content_fit_get_align_x (cf),
                              svg_content_fit_get_align_y (cf),
                              svg_content_fit_get_meet (cf),
                              &view_box,
                              0, 0, width, height,
                              &sx, &sy, &tx, &ty);

  gtk_snapshot_save (context->snapshot);

  transform = gsk_transform_translate (NULL, &vertex);
  transform = gsk_transform_rotate (transform, angle);
  transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (-x, -y));
  transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (tx, ty));
  transform = gsk_transform_scale (transform, sx, sy);

  gtk_snapshot_transform (context->snapshot, transform);
  push_transform (context, transform);
  gsk_transform_unref (transform);

  if (svg_enum_get (overflow) == OVERFLOW_HIDDEN)
    gtk_snapshot_push_clip (context->snapshot, &GRAPHENE_RECT_INIT (0, 0, view_box.size.width, view_box.size.height));

  render_shape (marker, context);

  if (svg_enum_get (overflow) == OVERFLOW_HIDDEN)
    gtk_snapshot_pop (context->snapshot);

  pop_transform (context);
  gtk_snapshot_restore (context->snapshot);
}

static void
paint_markers (SvgElement   *shape,
               GskPath      *path,
               PaintContext *context)
{
  GskPathPoint point, point1;

  if (gsk_path_is_empty (path))
    return;

  if (svg_href_get_kind (svg_element_get_current_value (shape, SVG_PROPERTY_MARKER_START)) == HREF_NONE &&
      svg_href_get_kind (svg_element_get_current_value (shape, SVG_PROPERTY_MARKER_MID)) == HREF_NONE &&
      svg_href_get_kind (svg_element_get_current_value (shape, SVG_PROPERTY_MARKER_END)) == HREF_NONE)
    return;

  if (!gsk_path_get_start_point (path, &point))
    return;

  push_ctx_shape (context, shape);

  push_op (context, MARKERS);
  paint_marker (shape, path, context, &point, VERTEX_START);

  if (gsk_path_get_next (path, &point))
    {
      while (1)
        {
          point1 = point;
          if (!gsk_path_get_next (path, &point1))
            {
              break;
            }

          paint_marker (shape, path, context, &point, VERTEX_MID);
          point = point1;
        }
    }

  paint_marker (shape, path, context, &point, VERTEX_END);

  pop_op (context);

  pop_ctx_shape (context);
}

/* }}} */
/* {{{ Text */

#define SVG_DEFAULT_DPI 96.0
static PangoLayout *
text_create_layout (SvgElement       *self,
                    PangoFontMap     *fontmap,
                    const char       *text,
                    graphene_point_t *origin,
                    graphene_rect_t  *bounds,
                    gboolean         *is_vertical,
                    double           *rotation)
{
  PangoContext *context;
  UnicodeBidi uni;
  PangoDirection direction;
  WritingMode wmode;
  PangoGravity gravity;
  PangoFontDescription *font_desc;
  PangoLayout *layout;
  PangoFontMetrics *metrics;
  int ascent, descent;
  PangoAttrList *attr_list;
  PangoAttribute *attr;
  TextDecoration decoration;
  int w,h;
  PangoLayoutIter *iter;
  double offset;
  SvgValue *font_family;
  DominantBaseline baseline;

  context = pango_font_map_create_context (fontmap);
  pango_context_set_language (context, svg_language_get (self->current[SVG_PROPERTY_LANG], 0));

  uni = svg_enum_get (self->current[SVG_PROPERTY_UNICODE_BIDI]);
  direction = svg_enum_get (self->current[SVG_PROPERTY_DIRECTION]);
  wmode = svg_enum_get (self->current[SVG_PROPERTY_WRITING_MODE]);
  switch (wmode)
    {
    case WRITING_MODE_HORIZONTAL_TB:
      gravity = PANGO_GRAVITY_SOUTH;
      break;
    case WRITING_MODE_VERTICAL_LR:
      // See note about having vertical-lr rotate -90° instead, below
    case WRITING_MODE_VERTICAL_RL:
      gravity = PANGO_GRAVITY_EAST;
      break;
    case WRITING_MODE_LEGACY_LR:
    case WRITING_MODE_LEGACY_LR_TB:
      direction = PANGO_DIRECTION_LTR;
      gravity = PANGO_GRAVITY_SOUTH;
      break;
    case WRITING_MODE_LEGACY_RL:
    case WRITING_MODE_LEGACY_RL_TB:
      direction = PANGO_DIRECTION_RTL;
      gravity = PANGO_GRAVITY_SOUTH;
      break;
    case WRITING_MODE_LEGACY_TB:
    case WRITING_MODE_LEGACY_TB_RL:
      direction = PANGO_DIRECTION_LTR;
      gravity = PANGO_GRAVITY_EAST;
      break;
    default:
      g_assert_not_reached ();
    }
  pango_context_set_base_gravity (context, gravity);

  if (uni == UNICODE_BIDI_OVERRIDE || uni == UNICODE_BIDI_EMBED)
    pango_context_set_base_dir (context, direction);

  font_desc = pango_font_description_copy (pango_context_get_font_description (context));

  font_family = self->current[SVG_PROPERTY_FONT_FAMILY];
  if (svg_string_list_get_length (font_family) > 0)
    {
      GString *s = g_string_new ("");

      for (unsigned int i = 0; i < svg_string_list_get_length (font_family); i++)
        {
          if (i > 0)
            g_string_append_c (s, ',');
          g_string_append (s, svg_string_list_get (font_family, i));
        }
      pango_font_description_set_family (font_desc, s->str);
      g_string_free (s, TRUE);
    }

  pango_font_description_set_style (font_desc, svg_enum_get (self->current[SVG_PROPERTY_FONT_STYLE]));
  pango_font_description_set_variant (font_desc, svg_enum_get (self->current[SVG_PROPERTY_FONT_VARIANT]));
  pango_font_description_set_weight (font_desc, svg_number_get (self->current[SVG_PROPERTY_FONT_WEIGHT], 1000.));
  pango_font_description_set_stretch (font_desc, svg_enum_get (self->current[SVG_PROPERTY_FONT_STRETCH]));

  pango_font_description_set_size (font_desc,
                                   svg_number_get (self->current[SVG_PROPERTY_FONT_SIZE], 1.) *
                                   PANGO_SCALE / SVG_DEFAULT_DPI * 72);

  layout = pango_layout_new (context);
  pango_layout_set_font_description (layout, font_desc);

  metrics = pango_context_get_metrics (context, font_desc, pango_context_get_language (context));
  ascent = pango_font_metrics_get_ascent (metrics);
  descent = pango_font_metrics_get_descent (metrics);

  pango_font_description_free (font_desc);

  attr_list = pango_attr_list_new ();

  attr = pango_attr_letter_spacing_new (svg_number_get (self->current[SVG_PROPERTY_LETTER_SPACING], 1.) * PANGO_SCALE);
  attr->start_index = 0;
  attr->end_index = G_MAXINT;
  pango_attr_list_insert (attr_list, attr);

  decoration = svg_text_decoration_get (self->current[SVG_PROPERTY_TEXT_DECORATION]);
  if (text)
    {
      if (decoration & TEXT_DECORATION_UNDERLINE)
        {
          attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
          attr->start_index = 0;
          attr->end_index = -1;
          pango_attr_list_insert (attr_list, attr);
        }
      if (decoration & TEXT_DECORATION_OVERLINE)
        {
          attr = pango_attr_overline_new (PANGO_OVERLINE_SINGLE);
          attr->start_index = 0;
          attr->end_index = -1;
          pango_attr_list_insert (attr_list, attr);
        }
      if (decoration & TEXT_DECORATION_LINE_TROUGH)
        {
          attr = pango_attr_strikethrough_new (TRUE);
          attr->start_index = 0;
          attr->end_index = -1;
          pango_attr_list_insert (attr_list, attr);
        }
    }

  pango_layout_set_attributes (layout, attr_list);
  pango_attr_list_unref (attr_list);

  pango_layout_set_text (layout, text, -1);

  pango_layout_set_alignment (layout, (direction == PANGO_DIRECTION_LTR) ? PANGO_ALIGN_LEFT : PANGO_ALIGN_RIGHT);

  pango_layout_get_size (layout, &w, &h);
  iter = pango_layout_get_iter (layout);
  offset = pango_layout_iter_get_baseline (iter) / (double)PANGO_SCALE;
  pango_layout_iter_free (iter);

  baseline = svg_enum_get (self->current[SVG_PROPERTY_DOMINANT_BASELINE]);
  switch (baseline)
    {
    case DOMINANT_BASELINE_AUTO:
    case DOMINANT_BASELINE_ALPHABETIC:
      offset += 0.;
      break;
    case DOMINANT_BASELINE_HANGING:
      offset -= (ascent - descent) / (double)PANGO_SCALE;
      break;
    case DOMINANT_BASELINE_MIDDLE:
      // Approximate meanline using strikethrough position and thickness
      // https://mail.gnome.org/archives/gtk-i18n-list/2012-December/msg00046.html
      offset -= (pango_font_metrics_get_strikethrough_position (metrics) +
                 pango_font_metrics_get_strikethrough_thickness (metrics) / 2
                ) / (double)PANGO_SCALE;
      break;
    case DOMINANT_BASELINE_CENTRAL:
      offset = .5 * (ascent + descent) / (double)PANGO_SCALE;
      break;
    case DOMINANT_BASELINE_TEXT_BEFORE_EDGE:
    case DOMINANT_BASELINE_TEXT_BOTTOM:
      //offset -= ascent / (double)PANGO_SCALE;
      // Bit of a klutch, but leads to better results
      offset -= (2.*ascent - h) / (double)PANGO_SCALE;
      break;
    case DOMINANT_BASELINE_TEXT_AFTER_EDGE:
    case DOMINANT_BASELINE_TEXT_TOP:
      offset += descent / (double)PANGO_SCALE;
      break;
    case DOMINANT_BASELINE_IDEOGRAPHIC:
      // Approx
      offset += descent / (double)PANGO_SCALE;
      break;
    case DOMINANT_BASELINE_MATHEMATICAL:
      // Approx
      offset += .5 * (ascent + descent) / (double)PANGO_SCALE;
      break;
    default:
      g_assert_not_reached ();
    }
  pango_font_metrics_unref (metrics);

  switch (wmode)
    {
    case WRITING_MODE_HORIZONTAL_TB:
    case WRITING_MODE_LEGACY_LR:
    case WRITING_MODE_LEGACY_LR_TB:
    case WRITING_MODE_LEGACY_RL:
    case WRITING_MODE_LEGACY_RL_TB:
      *origin = GRAPHENE_POINT_INIT (0., -offset);
      *bounds = GRAPHENE_RECT_INIT (0., -offset, w / (double)PANGO_SCALE, h / (double)PANGO_SCALE);
      *is_vertical = FALSE;
      *rotation = 0.;
      break;
    case WRITING_MODE_VERTICAL_LR:
      /* Firefox uses 90° here (like for the other vertical modes), while rsvg uses -90°
       * I came to the conclusion that implementing -90° properly (rsvg doesn't) doesn't
       * quite work with the current text handling architecture, so we'll just do what the
       * web browsers do.
       */
    case WRITING_MODE_VERTICAL_RL:
    case WRITING_MODE_LEGACY_TB:
    case WRITING_MODE_LEGACY_TB_RL:
      *origin = GRAPHENE_POINT_INIT (offset, 0.);
      *bounds = GRAPHENE_RECT_INIT (offset - h / (double)PANGO_SCALE, 0., h / (double)PANGO_SCALE, w / (double)PANGO_SCALE);
      *is_vertical = TRUE;
      *rotation = 90.;
      break;
    default:
      g_assert_not_reached ();
    }

  g_object_unref (context);
  return layout;
}

static gboolean
generate_layouts (SvgElement      *self,
                  PangoFontMap    *fontmap,
                  double          *x,
                  double          *y,
                  gboolean        *lastwasspace,
                  graphene_rect_t *bounds)
{
  double xs = 0.;
  double ys = 0.;
  gboolean lwss = TRUE;
  double dx, dy;

  g_assert (svg_element_type_is_text (self->type));

  if (svg_enum_get (self->current[SVG_PROPERTY_DISPLAY]) == DISPLAY_NONE)
    return FALSE;

  if (!x)
    x = &xs;
  if (!y)
    y = &ys;
  if (!lastwasspace)
    lastwasspace = &lwss;

  if (svg_element_is_specified (self, SVG_PROPERTY_X))
    *x = svg_number_get (self->current[SVG_PROPERTY_X], 1);
  if (svg_element_is_specified (self, SVG_PROPERTY_Y))
    *y = svg_number_get (self->current[SVG_PROPERTY_Y], 1);

  dx = svg_number_get (self->current[SVG_PROPERTY_DX], 1);
  dy = svg_number_get (self->current[SVG_PROPERTY_DY], 1);
  *x += dx;
  *y += dy;

  gboolean set_bounds = FALSE;
#define ADD_BBOX(box) do {                          \
  if (!set_bounds)                                  \
    {                                               \
      graphene_rect_init_from_rect (bounds, (box)); \
      set_bounds = TRUE;                            \
    }                                               \
  else                                              \
    {                                               \
      graphene_rect_t res;                          \
      graphene_rect_union (bounds, (box), &res);    \
      graphene_rect_init_from_rect (bounds, &res);  \
    }                                               \
} while(0);

  for (unsigned int i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);
      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          node->shape.has_bounds = generate_layouts (node->shape.shape, fontmap, x, y, lastwasspace, &node->shape.bounds);
          if (node->shape.has_bounds)
            ADD_BBOX (&node->shape.bounds)
          break;
        case TEXT_NODE_CHARACTERS:
          {
            graphene_point_t origin;
            graphene_rect_t cbounds;
            gboolean is_vertical;
            char *text = text_chomp (node->characters.text, lastwasspace);
            node->characters.layout = text_create_layout (self, fontmap, text, &origin, &cbounds, &is_vertical, &node->characters.r);
            g_free (text);

            node->characters.x = *x + origin.x;
            node->characters.y = *y + origin.y;

            graphene_rect_offset (&cbounds, *x, *y);
            ADD_BBOX (&cbounds)

            if (is_vertical)
              *y += cbounds.size.height;
            else
              *x += cbounds.size.width;
          }
          break;
        default:
          g_assert_not_reached ();
        }
    }

#undef ADD_BBOX
  return set_bounds;
}

static void
clear_layouts (SvgElement *self)
{
  for (unsigned int i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);
      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          clear_layouts (node->shape.shape);
          break;
        case TEXT_NODE_CHARACTERS:
          g_clear_object (&node->characters.layout);
          break;
        default:
          g_assert_not_reached ();
        }
    }
}

static void
fill_text (SvgElement            *self,
           PaintContext          *context,
           SvgValue              *paint,
           const graphene_rect_t *bounds)
{
  g_assert (svg_element_type_is_text (self->type));

  for (unsigned int i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);

      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          {
            SvgValue *cpaint = paint;
            const graphene_rect_t *cbounds = bounds;

            if (svg_enum_get (svg_element_get_current_value (node->shape.shape, SVG_PROPERTY_DISPLAY)) == DISPLAY_NONE)
              continue;

            if (svg_enum_get (svg_element_get_current_value (node->shape.shape, SVG_PROPERTY_VISIBILITY)) == VISIBILITY_HIDDEN)
              continue;

            if (svg_element_is_specified (node->shape.shape, SVG_PROPERTY_FILL))
              {
                cpaint = svg_element_get_current_value (node->shape.shape, SVG_PROPERTY_FILL);
                if (node->shape.has_bounds)
                  cbounds = &node->shape.bounds;
              }
            fill_text (node->shape.shape, context, cpaint, cbounds);
          }
          break;
        case TEXT_NODE_CHARACTERS:
          {
            double opacity;

            if (svg_paint_get_kind (paint) == PAINT_NONE)
              goto skip;

#if 0
            GskRoundedRect brd;
            gsk_rounded_rect_init_from_rect (&brd, bounds, 0.f);
            GdkRGBA red = (GdkRGBA){ .red = 1., .green = 0., .blue = 0., .alpha = 1. };
            gtk_snapshot_append_border (context->snapshot, &brd, (const float[4]){ 3.f, 3.f, 3.f, 3.f }, (const GdkRGBA[4]){ red, red, red, red });
#endif

            gtk_snapshot_save (context->snapshot);

            opacity = svg_number_get (self->current[SVG_PROPERTY_FILL_OPACITY], 1);
            if (svg_paint_get_kind (paint) == PAINT_COLOR)
              {
                GdkColor color;

                gdk_color_init_copy (&color, svg_paint_get_color (paint));
                color.alpha *= opacity;
                gtk_snapshot_translate (context->snapshot, &GRAPHENE_POINT_INIT (node->characters.x, node->characters.y));
                gtk_snapshot_rotate (context->snapshot, node->characters.r);
                gtk_snapshot_add_layout (context->snapshot, node->characters.layout, &color);
              }
            else if (paint_is_server (svg_paint_get_kind (paint)))
              {
                if (opacity < 1)
                  gtk_snapshot_push_opacity (context->snapshot, opacity);

                gtk_snapshot_push_mask (context->snapshot, GSK_MASK_MODE_ALPHA);
                gtk_snapshot_translate (context->snapshot, &GRAPHENE_POINT_INIT (node->characters.x, node->characters.y));
                gtk_snapshot_rotate (context->snapshot, node->characters.r);
                gtk_snapshot_append_layout (context->snapshot, node->characters.layout, &GDK_RGBA_BLACK);
                gtk_snapshot_pop (context->snapshot);
                paint_server (paint, bounds, bounds, context);
                gtk_snapshot_pop (context->snapshot);

                if (opacity < 1)
                  gtk_snapshot_pop (context->snapshot);
              }

            gtk_snapshot_restore (context->snapshot);

skip: ;
          }
          break;
        default:
          g_assert_not_reached ();
        }
    }
}

static void
stroke_text (SvgElement            *self,
             PaintContext          *context,
             SvgValue              *paint,
             GskStroke             *stroke,
             const graphene_rect_t *bounds)
{
  g_assert (svg_element_type_is_text (self->type));

  for (unsigned int i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);

      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          {
            SvgValue *cpaint = paint;
            const graphene_rect_t *cbounds = bounds;

            if (svg_enum_get (svg_element_get_current_value (node->shape.shape, SVG_PROPERTY_DISPLAY)) == DISPLAY_NONE)
              continue;

            if (svg_enum_get (svg_element_get_current_value (node->shape.shape, SVG_PROPERTY_VISIBILITY)) == VISIBILITY_HIDDEN)
              continue;

            if (svg_element_is_specified (node->shape.shape, SVG_PROPERTY_STROKE))
              {
                cpaint = svg_element_get_current_value (node->shape.shape, SVG_PROPERTY_STROKE);
                if (node->shape.has_bounds)
                  cbounds = &node->shape.bounds;
              }
            stroke_text (node->shape.shape, context, cpaint, stroke, cbounds);
          }
          break;
        case TEXT_NODE_CHARACTERS:
          {
            GskPath *path;
            double opacity;

            if (svg_paint_get_kind (paint) == PAINT_NONE)
              goto skip;

            gtk_snapshot_save (context->snapshot);
            opacity = svg_number_get (self->current[SVG_PROPERTY_STROKE_OPACITY], 1);
            if (svg_paint_get_kind (paint) == PAINT_COLOR)
              {
                GdkColor color;

                gdk_color_init_copy (&color, svg_paint_get_color (paint));
                color.alpha *= opacity;

                gtk_snapshot_push_mask (context->snapshot, GSK_MASK_MODE_ALPHA);
                gtk_snapshot_translate (context->snapshot, &GRAPHENE_POINT_INIT (node->characters.x, node->characters.y));
                gtk_snapshot_rotate (context->snapshot, node->characters.r);
                path = svg_pango_layout_to_path (node->characters.layout);

                gtk_snapshot_append_stroke (context->snapshot, path, stroke, &GDK_RGBA_BLACK);
                gtk_snapshot_pop (context->snapshot);
                gtk_snapshot_add_color (context->snapshot, &color, bounds);
                gtk_snapshot_pop (context->snapshot);
                gsk_path_unref (path);
                gdk_color_finish (&color);
              }
            else if (paint_is_server (svg_paint_get_kind (paint)))
              {
                if (opacity < 1)
                  gtk_snapshot_push_opacity (context->snapshot, opacity);

                gtk_snapshot_push_mask (context->snapshot, GSK_MASK_MODE_ALPHA);
                gtk_snapshot_translate (context->snapshot, &GRAPHENE_POINT_INIT (node->characters.x, node->characters.y));
                gtk_snapshot_rotate (context->snapshot, node->characters.r);
                path = svg_pango_layout_to_path (node->characters.layout);
                gtk_snapshot_append_stroke (context->snapshot, path, stroke, &GDK_RGBA_BLACK);
                gtk_snapshot_pop (context->snapshot);
                paint_server (paint, bounds, bounds, context);
                gtk_snapshot_pop (context->snapshot);
                gsk_path_unref (path);

                if (opacity < 1)
                  gtk_snapshot_pop (context->snapshot);
              }
            gtk_snapshot_restore (context->snapshot);

skip: ;
          }
          break;
        default:
          g_assert_not_reached ();
        }
    }
}

static gboolean
point_in_pango_rect (PangoRectangle         *rect,
                     const graphene_point_t *p)
{
  pango_extents_to_pixels (rect, NULL);
  return rect->x <= p->x && p->x <= rect->x + rect->width &&
         rect->y <= p->y && p->y <= rect->y + rect->height;
}

static gboolean
point_in_layout (PangoLayout            *layout,
                 const graphene_point_t *p)
{
  PangoLayoutIter *iter;
  PangoRectangle rect;

  iter = pango_layout_get_iter (layout);
  do {
    pango_layout_iter_get_line_extents (iter, &rect, NULL);
    if (point_in_pango_rect (&rect, p))
      {
        do {
          pango_layout_iter_get_char_extents (iter, &rect);
          if (point_in_pango_rect (&rect, p))
            {
              pango_layout_iter_free (iter);
              return TRUE;
            }
        } while (pango_layout_iter_next_char (iter));
      }
  } while (pango_layout_iter_next_line (iter));

  pango_layout_iter_free (iter);

  return FALSE;
}

static void
pick_text (SvgElement   *self,
           PaintContext *context)
{
  g_assert (svg_element_type_is_text (self->type));

  for (unsigned int i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);

      if (context->picking.done)
        break;

      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          {
            if (svg_enum_get (svg_element_get_current_value (node->shape.shape, SVG_PROPERTY_DISPLAY)) == DISPLAY_NONE)
              continue;

            pick_text (node->shape.shape, context);
          }
          break;
        case TEXT_NODE_CHARACTERS:
          {
            GskTransform *transform;
            transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (node->characters.x, node->characters.y));
            push_transform (context, transform);
            gsk_transform_unref (transform);
            transform = gsk_transform_rotate (NULL, node->characters.r);
            push_transform (context, transform);
            gsk_transform_unref (transform);
            if (point_in_layout (node->characters.layout, &context->picking.p))
              {
                context->picking.picked = self;
                context->picking.done = TRUE;
              }
            pop_transform (context);
            pop_transform (context);
          }
          break;
        default:
          g_assert_not_reached ();
        }
    }
}

/* }}} */
/* {{{ Images */

static void
render_image (SvgElement   *shape,
              PaintContext *context)
{
  SvgValue *href = svg_element_get_current_value (shape, SVG_PROPERTY_HREF);
  SvgValue *cf = svg_element_get_current_value (shape, SVG_PROPERTY_CONTENT_FIT);
  SvgValue *overflow = svg_element_get_current_value (shape, SVG_PROPERTY_OVERFLOW);
  graphene_rect_t vb;
  double sx, sy, tx, ty;
  double x, y, width, height;
  GdkTexture *texture;

  if (context->picking.picking && context->picking.done)
    return;

  if (svg_href_get_texture (href) == NULL)
    {
      gtk_svg_rendering_error (context->svg,
                               "No content for <image>");
      return;
    }

  texture = svg_href_get_texture (href);
  graphene_rect_init (&vb,
                      0, 0,
                      gdk_texture_get_width (texture),
                      gdk_texture_get_height (texture));

  x = context->viewport->origin.x + svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_X), context->viewport->size.width);
  y = context->viewport->origin.y + svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_Y), context->viewport->size.height);
  width = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_WIDTH), context->viewport->size.width);
  height = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_HEIGHT), context->viewport->size.height);

  compute_viewport_transform (svg_content_fit_is_none (cf),
                              svg_content_fit_get_align_x (cf),
                              svg_content_fit_get_align_y (cf),
                              svg_content_fit_get_meet (cf),
                              &vb,
                              x, y, width, height,
                              &sx, &sy, &tx, &ty);

  if (svg_enum_get (overflow) == OVERFLOW_HIDDEN)
    gtk_snapshot_push_clip (context->snapshot, &GRAPHENE_RECT_INIT (x, y, width, height));

  GskTransform *transform = NULL;
  gtk_snapshot_save (context->snapshot);

  transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (tx, ty));
  transform = gsk_transform_scale (transform, sx, sy);

  gtk_snapshot_transform (context->snapshot, transform);
  push_transform (context, transform);
  gsk_transform_unref (transform);

  if (context->picking.picking)
    {
      switch (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_POINTER_EVENTS)))
        {
        case POINTER_EVENTS_AUTO:
        case POINTER_EVENTS_VISIBLE_PAINTED:
        case POINTER_EVENTS_VISIBLE_FILL:
        case POINTER_EVENTS_VISIBLE_STROKE:
        case POINTER_EVENTS_VISIBLE:
          if (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_VISIBILITY)) == VISIBILITY_HIDDEN)
            break;
          G_GNUC_FALLTHROUGH;

        case POINTER_EVENTS_BOUNDING_BOX:
        case POINTER_EVENTS_PAINTED:
        case POINTER_EVENTS_FILL:
        case POINTER_EVENTS_STROKE:
        case POINTER_EVENTS_ALL:
          if (gsk_rect_contains_point (&vb, &context->picking.p))
            {
              context->picking.picked = shape;
              context->picking.done = TRUE;
            }
          break;

        case POINTER_EVENTS_NONE:
          break;
        default:
          g_assert_not_reached ();
        }

    }
  else
    gtk_snapshot_append_texture (context->snapshot, texture, &vb);

  pop_transform (context);
  gtk_snapshot_restore (context->snapshot);

  if (svg_enum_get (overflow) == OVERFLOW_HIDDEN)
    gtk_snapshot_pop (context->snapshot);
}

/* }}} */

static gboolean
shape_is_degenerate (SvgElement *shape)
{
  if (svg_element_get_type (shape) == SVG_ELEMENT_RECT)
    return svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_WIDTH), 1) <= 0 ||
           svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_HEIGHT), 1) <= 0;
  else if (svg_element_get_type (shape) == SVG_ELEMENT_CIRCLE)
    return svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_R), 1) <= 0;
  else if (svg_element_get_type (shape) == SVG_ELEMENT_ELLIPSE)
    return (!svg_value_is_auto (svg_element_get_current_value (shape, SVG_PROPERTY_RX)) &&
            svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_RX), 1) <= 0) ||
           (!svg_value_is_auto (svg_element_get_current_value (shape, SVG_PROPERTY_RY)) &&
            svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_RY), 1) <= 0);
  else
    return FALSE;
}

static void
recompute_current_values (SvgElement   *shape,
                          SvgElement   *parent,
                          PaintContext *context)
{
  SvgComputeContext ctx;

  ctx.svg = context->svg;
  ctx.viewport = context->viewport;
  ctx.parent = parent;
  ctx.current_time = context->current_time;
  ctx.colors = context->colors;
  ctx.n_colors = context->n_colors;

  compute_current_values_for_shape (shape, &ctx);
}

static void
paint_shape (SvgElement   *shape,
             PaintContext *context)
{
  GskPath *path;

  if (context->picking.picking &&
      (context->picking.done ||
       context->picking.clipped == shape))
    return;

  if (svg_element_get_type (shape) == SVG_ELEMENT_USE)
    {
      if (shape->shapes->len > 0)
        {
          SvgElement *use_shape = g_ptr_array_index (shape->shapes, 0);

          push_ctx_shape (context, shape);
          render_shape (use_shape, context);
          pop_ctx_shape (context);
        }

      return;
    }

  if (svg_element_get_type (shape) == SVG_ELEMENT_TEXT)
    {
      TextAnchor anchor;
      WritingMode wmode;
      graphene_rect_t bounds;
      float dx, dy;
      GskTransform *transform = NULL;

      if (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_DISPLAY)) == DISPLAY_NONE)
        return;

      if (!context->picking.picking)
        {
          if (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_VISIBILITY)) == VISIBILITY_HIDDEN)
            return;
        }

      anchor = svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_TEXT_ANCHOR));
      wmode = svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_WRITING_MODE));
      if (!generate_layouts (shape, get_fontmap (context->svg), NULL, NULL, NULL, &bounds))
        return;

      dx = dy = 0;
      switch (anchor)
        {
        case TEXT_ANCHOR_START:
          break;
        case TEXT_ANCHOR_MIDDLE:
          if (svg_writing_mode_is_vertical (wmode))
            dy = - bounds.size.height / 2;
          else
            dx = - bounds.size.width / 2;
          break;
        case TEXT_ANCHOR_END:
          if (svg_writing_mode_is_vertical (wmode))
            dy = - bounds.size.height;
          else
            dx = - bounds.size.width;
          break;
        default:
          g_assert_not_reached ();
        }

      graphene_rect_init (&shape->bounds,
                          bounds.origin.x + dx, bounds.origin.y + dy,
                          bounds.size.width, bounds.size.height);
      shape->valid_bounds = TRUE;

      if (context->picking.picking &&
          svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_POINTER_EVENTS)) == POINTER_EVENTS_BOUNDING_BOX)
        {
          if (gsk_rect_contains_point (&shape->bounds, &context->picking.p))
            {
              context->picking.picked = shape;
              context->picking.done = TRUE;
            }
          return;
        }

      gtk_snapshot_save (context->snapshot);
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (dx, dy));

      gtk_snapshot_transform (context->snapshot, transform);
      push_transform (context, transform);
      gsk_transform_unref (transform);

      if (context->picking.picking)
        {
          switch (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_POINTER_EVENTS)))
            {
            case POINTER_EVENTS_NONE:
              break;

            case POINTER_EVENTS_AUTO:
            case POINTER_EVENTS_VISIBLE_PAINTED:
              if (svg_paint_get_kind (svg_element_get_current_value (shape, SVG_PROPERTY_FILL)) == PAINT_NONE &&
                  svg_paint_get_kind (svg_element_get_current_value (shape, SVG_PROPERTY_STROKE)) == PAINT_NONE)
                break;
              G_GNUC_FALLTHROUGH;

            case POINTER_EVENTS_VISIBLE_FILL:
            case POINTER_EVENTS_VISIBLE_STROKE:
            case POINTER_EVENTS_VISIBLE:
              if (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_VISIBILITY)) == VISIBILITY_HIDDEN)
                break;
              G_GNUC_FALLTHROUGH;

            case POINTER_EVENTS_FILL:
            case POINTER_EVENTS_STROKE:
            case POINTER_EVENTS_ALL:
              pick_text (shape, context);
              break;

            case POINTER_EVENTS_PAINTED:
              if (svg_paint_get_kind (svg_element_get_current_value (shape, SVG_PROPERTY_FILL)) == PAINT_NONE &&
                  svg_paint_get_kind (svg_element_get_current_value (shape, SVG_PROPERTY_STROKE)) == PAINT_NONE)
                break;
              pick_text (shape, context);
              break;

            case POINTER_EVENTS_BOUNDING_BOX: /* handled earlier */
            default:
              g_assert_not_reached ();
            }
        }
      else if (context->op == CLIPPING)
        {
          SvgValue *paint = svg_paint_new_black ();
          fill_text (shape, context, paint, &bounds);
          svg_value_unref (paint);
        }
      else
        {
          GskStroke *stroke = shape_create_stroke (shape, context);

          switch (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_PAINT_ORDER)))
            {
            case PAINT_ORDER_FILL_STROKE_MARKERS:
            case PAINT_ORDER_FILL_MARKERS_STROKE:
            case PAINT_ORDER_MARKERS_FILL_STROKE:
              fill_text (shape, context, svg_element_get_current_value (shape, SVG_PROPERTY_FILL), &bounds);
              break;
            case PAINT_ORDER_STROKE_FILL_MARKERS:
            case PAINT_ORDER_STROKE_MARKERS_FILL:
            case PAINT_ORDER_MARKERS_STROKE_FILL:
              stroke_text (shape, context, svg_element_get_current_value (shape, SVG_PROPERTY_STROKE), stroke, &bounds);
              break;
            default:
              g_assert_not_reached ();
            }

          switch (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_PAINT_ORDER)))
            {
            case PAINT_ORDER_FILL_STROKE_MARKERS:
            case PAINT_ORDER_FILL_MARKERS_STROKE:
            case PAINT_ORDER_MARKERS_FILL_STROKE:
              stroke_text (shape, context, svg_element_get_current_value (shape, SVG_PROPERTY_STROKE), stroke, &bounds);
              break;
            case PAINT_ORDER_STROKE_FILL_MARKERS:
            case PAINT_ORDER_STROKE_MARKERS_FILL:
            case PAINT_ORDER_MARKERS_STROKE_FILL:
              fill_text (shape, context, svg_element_get_current_value (shape, SVG_PROPERTY_FILL), &bounds);
              break;
            default:
              g_assert_not_reached ();
            }

          gsk_stroke_free (stroke);
        }

      clear_layouts (shape);

      pop_transform (context);
      gtk_snapshot_restore (context->snapshot);

      return;
    }

  if (svg_element_get_type (shape) == SVG_ELEMENT_IMAGE)
    {
      render_image (shape, context);
      return;
    }

  if (shape->shapes)
    {
      if (context->picking.picking)
        {
          for (int i = 0; i < shape->shapes->len; i++)
            {
              SvgElement *s = g_ptr_array_index (shape->shapes, shape->shapes->len - 1 - i);

              if (context->picking.done)
                break;

              render_shape (s, context);

              if (svg_element_get_type (shape) == SVG_ELEMENT_SWITCH &&
                  !svg_element_conditionally_excluded (s, context->svg))
                break;
            }
        }
      else
        {
          for (int i = 0; i < shape->shapes->len; i++)
            {
              SvgElement *s = g_ptr_array_index (shape->shapes, i);

              render_shape (s, context);

              if (svg_element_get_type (shape) == SVG_ELEMENT_SWITCH &&
                  !svg_element_conditionally_excluded (s, context->svg))
                break;
            }
        }

      return;
    }

  if (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_VISIBILITY)) == VISIBILITY_HIDDEN)
    return;

  if (shape_is_degenerate (shape))
    return;

  if (context->picking.picking)
    {
      if (svg_element_contains (shape, context->viewport, context->svg, &context->picking.p))
        {
          if (!context->picking.done)
            {
              context->picking.picked = shape;
              context->picking.done = TRUE;
            }
        }

      return;
    }

  /* Below is where we render *actual* content (i.e. graphical
   * shapes that have paths). This involves filling, stroking
   * and placing markers, in the order determined by paint-order.
   * Unless we are clipping, in which case we just fill to
   * create a 1-bit mask of the 'raw geometry'.
   */
  path = svg_element_get_current_path (shape, context->viewport);

  if (context->op == CLIPPING)
    {
      graphene_rect_t bounds;

      if (gsk_path_get_bounds (path, &bounds))
        {
          GdkColor color = GDK_COLOR_SRGB (0, 0, 0, 1);
          GskFillRule clip_rule;

          clip_rule = svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_CLIP_RULE));
          gtk_snapshot_push_fill (context->snapshot, path, clip_rule);
          gtk_snapshot_add_color (context->snapshot, &color, &bounds);
          gtk_snapshot_pop (context->snapshot);
        }
    }
  else
    {
      SvgValue *paint = svg_element_get_current_value (shape, SVG_PROPERTY_FILL);
      double opacity = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_FILL_OPACITY), 1);

      switch (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_PAINT_ORDER)))
        {
        case PAINT_ORDER_FILL_STROKE_MARKERS:
        case PAINT_ORDER_FILL_MARKERS_STROKE:
          fill_shape (shape, path, paint, opacity, context);
          break;
        case PAINT_ORDER_STROKE_FILL_MARKERS:
        case PAINT_ORDER_STROKE_MARKERS_FILL:
          stroke_shape (shape, path, context);
          break;
        case PAINT_ORDER_MARKERS_FILL_STROKE:
        case PAINT_ORDER_MARKERS_STROKE_FILL:
          paint_markers (shape, path, context);
          break;
        default:
          g_assert_not_reached ();
        }

      switch (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_PAINT_ORDER)))
        {
        case PAINT_ORDER_MARKERS_FILL_STROKE:
        case PAINT_ORDER_STROKE_FILL_MARKERS:
          fill_shape (shape, path, paint, opacity, context);
          break;
        case PAINT_ORDER_FILL_STROKE_MARKERS:
        case PAINT_ORDER_MARKERS_STROKE_FILL:
          stroke_shape (shape, path, context);
          break;
        case PAINT_ORDER_FILL_MARKERS_STROKE:
        case PAINT_ORDER_STROKE_MARKERS_FILL:
          paint_markers (shape, path, context);
          break;
        default:
          g_assert_not_reached ();
        }

      switch (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_PAINT_ORDER)))
        {
        case PAINT_ORDER_MARKERS_STROKE_FILL:
        case PAINT_ORDER_STROKE_MARKERS_FILL:
          fill_shape (shape, path, paint, opacity, context);
          break;
        case PAINT_ORDER_MARKERS_FILL_STROKE:
        case PAINT_ORDER_FILL_MARKERS_STROKE:
          stroke_shape (shape, path, context);
          break;
        case PAINT_ORDER_STROKE_FILL_MARKERS:
        case PAINT_ORDER_FILL_STROKE_MARKERS:
          paint_markers (shape, path, context);
          break;
        default:
          g_assert_not_reached ();
        }
    }

  gsk_path_unref (path);
}

static gboolean
display_property_applies_to (SvgElement *shape)
{
  return svg_element_get_type (shape) != SVG_ELEMENT_MASK &&
         svg_element_get_type (shape) != SVG_ELEMENT_CLIP_PATH &&
         svg_element_get_type (shape) != SVG_ELEMENT_MARKER &&
         svg_element_get_type (shape) != SVG_ELEMENT_SYMBOL;
}

static void
render_shape (SvgElement   *shape,
              PaintContext *context)
{
  gboolean op_changed;

  if (svg_element_get_type (shape) == SVG_ELEMENT_DEFS ||
      svg_element_get_type (shape) == SVG_ELEMENT_LINEAR_GRADIENT ||
      svg_element_get_type (shape) == SVG_ELEMENT_RADIAL_GRADIENT)
    return;

  if (svg_element_type_never_rendered (svg_element_get_type (shape)))
    {
      if (!((svg_element_get_type (shape) == SVG_ELEMENT_SYMBOL && shape_is_use_target (shape)) ||
           (svg_element_get_type (shape) == SVG_ELEMENT_CLIP_PATH && context->op == CLIPPING && context->op_changed) ||
           (svg_element_get_type (shape) == SVG_ELEMENT_MASK && context->op == MASKING && context->op_changed) ||
           (svg_element_get_type (shape) == SVG_ELEMENT_MARKER && context->op == MARKERS && context->op_changed)))
        return;
    }

  if (display_property_applies_to (shape))
    {
      if (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_DISPLAY)) == DISPLAY_NONE)
        return;
    }

  if (svg_element_conditionally_excluded (shape, context->svg))
    return;

  if (context->instance_count++ > DRAWING_LIMIT)
    {
      gtk_svg_rendering_error (context->svg, "excessive instance count, aborting");
      return;
    }

  context->depth++;

  if (context->depth > NESTING_LIMIT)
    {
      gtk_svg_rendering_error (context->svg,
                               "excessive rendering depth (> %d), aborting",
                               NESTING_LIMIT);
      return;
    }

  push_group (shape, context);

  op_changed = context->op_changed;
  context->op_changed = FALSE;

  paint_shape (shape, context);

  context->op_changed = op_changed;

  pop_group (shape, context);

  context->depth--;
}

static SvgElement *
find_filter (SvgElement *shape,
             const char *filter_id)
{
  if (svg_element_get_type (shape) == SVG_ELEMENT_FILTER)
    {
      if (g_strcmp0 (svg_element_get_id (shape), filter_id) == 0)
        return shape;
      else
        return NULL;
    }

  if (svg_element_type_is_container (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          SvgElement *sh = g_ptr_array_index (shape->shapes, i);
          SvgElement *res;

          res = find_filter (sh, filter_id);
          if (res)
            return res;
        }
    }

  return NULL;
}

GskRenderNode *
gtk_svg_apply_filter (GtkSvg                *svg,
                      const char            *filter_id,
                      const graphene_rect_t *bounds,
                      GskRenderNode         *source)
{
  SvgElement *filter;
  SvgElement *shape;
  PaintContext paint_context;
  GskRenderNode *result;
  G_GNUC_UNUSED GskRenderNode *node;

  filter = find_filter (svg->content, filter_id);
  if (!filter)
    return gsk_render_node_ref (source);

  /* This is a bit iffy. We create an extra shape,
   * and treat it as if it was part of the svg.
   */

  shape = svg_element_new (NULL, SVG_ELEMENT_RECT);

  shape->valid_bounds = TRUE;
  shape->bounds = *bounds;

  paint_context.svg = svg;
  paint_context.viewport = bounds;
  paint_context.viewport_stack = NULL;
  paint_context.snapshot = gtk_snapshot_new ();
  paint_context.colors = NULL;
  paint_context.n_colors = 0;
  paint_context.weight = 400;
  paint_context.op = RENDERING;
  paint_context.op_stack = NULL;
  paint_context.ctx_shape_stack = NULL;
  paint_context.current_time = svg->current_time;
  paint_context.depth = 0;
  paint_context.transforms = NULL;
  paint_context.instance_count = 0;
  paint_context.picking.picking = FALSE;

  /* This is necessary so the filter has current values.
   * Also, any other part of the svg that the filter might
   * refer to.
   */
  recompute_current_values (svg->content, NULL, &paint_context);

  /* This is necessary, so the shape itself has current values */
  recompute_current_values (shape, NULL, &paint_context);

  result = apply_filter_tree (shape, filter, &paint_context, source);

  svg_element_free (shape);

  node = gtk_snapshot_free_to_node (paint_context.snapshot);
  g_assert (node == NULL);

  return result;
}

SvgElement *
gtk_svg_pick_element (GtkSvg                 *self,
                      const graphene_point_t *p)
{
  SvgComputeContext compute_context;
  PaintContext paint_context;
  graphene_rect_t viewport;
  GtkSnapshot *snapshot;
  GskRenderNode *node;

  if (self->width < 0 || self->height < 0)
    return NULL;

  viewport = GRAPHENE_RECT_INIT (0, 0, self->current_width, self->current_height);

  compute_context.svg = self;
  compute_context.viewport = &viewport;
  compute_context.colors = self->node_for.colors;
  compute_context.n_colors = self->node_for.n_colors;
  compute_context.current_time = self->current_time;
  compute_context.parent = NULL;
  compute_context.interpolation = GDK_COLOR_STATE_SRGB;

  compute_current_values_for_shape (self->content, &compute_context);

  snapshot = gtk_snapshot_new ();

  paint_context.svg = self;
  paint_context.viewport = &viewport;
  paint_context.viewport_stack = NULL;
  paint_context.snapshot = snapshot;
  paint_context.colors = self->node_for.colors;
  paint_context.n_colors = self->node_for.n_colors;
  paint_context.weight = self->node_for.weight;
  paint_context.op = RENDERING;
  paint_context.op_stack = NULL;
  paint_context.ctx_shape_stack = NULL;
  paint_context.current_time = self->current_time;
  paint_context.depth = 0;
  paint_context.transforms = NULL;
  paint_context.instance_count = 0;
  paint_context.picking.picking = TRUE;
  paint_context.picking.p = *p;
  paint_context.picking.points = NULL;
  paint_context.picking.done = FALSE;
  paint_context.picking.picked = NULL;

  if (self->overflow == GTK_OVERFLOW_HIDDEN)
    gtk_snapshot_push_clip (snapshot, &viewport);

  render_shape (self->content, &paint_context);

  if (self->overflow == GTK_OVERFLOW_HIDDEN)
    gtk_snapshot_pop (snapshot);

  node = gtk_snapshot_free_to_node (snapshot);
  g_clear_pointer (&node, gsk_render_node_unref);

  return paint_context.picking.picked;
}

static gboolean
can_reuse_node (GtkSvg        *self,
                double         width,
                double         height,
                const GdkRGBA *colors,
                size_t         n_colors,
                double         weight)
{
  if (self->node == NULL)
    return FALSE;

  if (self->style_changed)
    {
      dbg_print ("cache", "Can't reuse rendernode: %s", "style change");
      return FALSE;
    }

  if (self->view_changed)
    {
      dbg_print ("cache", "Can't reuse rendernode: %s", "view change");
      return FALSE;
    }

  if (self->state != self->node_for.state)
    {
      dbg_print ("cache", "Can't reuse rendernode: %s", "state change");
      return FALSE;
    }

  if (self->current_time != self->node_for.time)
    {
      dbg_print ("cache", "Can't reuse rendernode: %s", "current_time change");
      return FALSE;
    }

  if ((width != self->node_for.width || height != self->node_for.height))
    {
      dbg_print ("cache", "Can't reuse rendernode: %s", "size change");
      return FALSE;
    }

  if ((self->used & GTK_SVG_USES_STROKES) != 0 &&
      self->weight < 1 &&
      weight != self->node_for.weight)
    {
      dbg_print ("cache", "Can't reuse rendernode: %s", "stroke weight change");
      return FALSE;
    }

  if ((self->features & GTK_SVG_EXTENSIONS) != 0)
    {
      if ((self->used & GTK_SVG_USES_SYMBOLIC_FOREGROUND) != 0 &&
           self->node_for.n_colors > GTK_SYMBOLIC_COLOR_FOREGROUND &&
           n_colors > GTK_SYMBOLIC_COLOR_FOREGROUND &&
           !gdk_rgba_equal (&self->node_for.colors[GTK_SYMBOLIC_COLOR_FOREGROUND],
                            &colors[GTK_SYMBOLIC_COLOR_FOREGROUND]))
        {
          dbg_print ("cache", "Can't reuse rendernode: %s", "symbolic foreground change");
          return FALSE;
        }

      if ((self->used & GTK_SVG_USES_SYMBOLIC_ERROR) != 0 &&
          self->node_for.n_colors > GTK_SYMBOLIC_COLOR_ERROR &&
          n_colors > GTK_SYMBOLIC_COLOR_ERROR &&
          !gdk_rgba_equal (&self->node_for.colors[GTK_SYMBOLIC_COLOR_ERROR],
                          &colors[GTK_SYMBOLIC_COLOR_ERROR]))
        {
          dbg_print ("cache", "Can't reuse rendernode: %s", "symbolic error change");
          return FALSE;
        }

      if ((self->used & GTK_SVG_USES_SYMBOLIC_WARNING) != 0 &&
          self->node_for.n_colors > GTK_SYMBOLIC_COLOR_WARNING &&
          n_colors > GTK_SYMBOLIC_COLOR_WARNING &&
          !gdk_rgba_equal (&self->node_for.colors[GTK_SYMBOLIC_COLOR_WARNING],
                          &colors[GTK_SYMBOLIC_COLOR_WARNING]))
        {
          dbg_print ("cache", "Can't reuse rendernode: %s", "symbolic warning change");
          return FALSE;
        }

      if ((self->used & GTK_SVG_USES_SYMBOLIC_SUCCESS) != 0 &&
          self->node_for.n_colors > GTK_SYMBOLIC_COLOR_SUCCESS &&
          n_colors > GTK_SYMBOLIC_COLOR_SUCCESS &&
          !gdk_rgba_equal (&self->node_for.colors[GTK_SYMBOLIC_COLOR_SUCCESS],
                          &colors[GTK_SYMBOLIC_COLOR_SUCCESS]))
        {
          dbg_print ("cache", "Can't reuse rendernode: %s", "symbolic success change");
          return FALSE;
        }

      if ((self->used & GTK_SVG_USES_SYMBOLIC_ACCENT) != 0 &&
          self->node_for.n_colors > GTK_SYMBOLIC_COLOR_ACCENT &&
          n_colors > GTK_SYMBOLIC_COLOR_ACCENT &&
          !gdk_rgba_equal (&self->node_for.colors[GTK_SYMBOLIC_COLOR_ACCENT],
                          &colors[GTK_SYMBOLIC_COLOR_ACCENT]))
        {
          dbg_print ("cache", "Can't reuse rendernode: %s", "symbolic accent change");
          return FALSE;
        }
    }

  return TRUE;
}

/* Note that we are doing this in two passes:
 * 1. Update current values from animations
 * 2. Paint with the current values
 *
 * This is the easiest way to avoid complications due
 * to the fact that animations can have dependencies
 * on current values of other shapes (e.g animateMotion
 * -> path -> rect -> x, y, width, height -> viewport).
 *
 * To handle such dependencies correctly, we compute
 * an *update order* which may be different than the
 * paint order that is determined by the document
 * structure, and use that order in a separate pass
 * before we paint.
 *
 * Another complication is that hrefs can be animated,
 * so may have to regenerate use shadow trees mid-compute.
 */

void
gtk_svg_snapshot_full (GtkSvg        *self,
                       GtkSnapshot   *snapshot,
                       double         width,
                       double         height,
                       const GdkRGBA *colors,
                       size_t         n_colors,
                       double         weight)
{
  graphene_rect_t viewport = GRAPHENE_RECT_INIT (0, 0, width, height);
  const GdkRGBA *used_colors;
  GdkRGBA solid_colors[5];
  size_t n_used_colors;
  float used_opacity;

  if (self->width < 0 || self->height < 0)
    return;

  if (self->load_time == INDEFINITE)
    {
      int64_t current_time;

      /* If we get here and load_time is still INDEFINITE, we are
       * not rendering an animation properly, we just do a snapshot.
       *
       * But we still need to get initial animation state applied.
       */
      if (self->clock)
        current_time = gdk_frame_clock_get_frame_time (self->clock);
      else
        current_time = 0;

      gtk_svg_set_load_time (self, current_time);
    }

  if (!can_reuse_node (self, width, height, colors, n_colors, weight))
    {
      SvgComputeContext compute_context;
      PaintContext paint_context;

      g_clear_pointer (&self->node, gsk_render_node_unref);

      if (self->style_changed)
        {
          apply_styles_to_shape (self->content, self);
          self->style_changed = FALSE;
        }

      apply_view (self->content, self->view);

      /* Traditional symbolics often have overlapping shapes,
       * causing things to look wrong when using colors with
       * alpha. To work around that, we always draw them with
       * solid colors and apply foreground opacity globally.
       *
       * Non-symbolic icons are responsible for dealing with
       * overlaps themselves, using the full svg machinery.
       */
      if (self->gpa_version == 0 &&
          (self->features & GTK_SVG_TRADITIONAL_SYMBOLIC) != 0 &&
          colors[GTK_SYMBOLIC_COLOR_FOREGROUND].alpha < 1)
        {
          used_opacity = colors[GTK_SYMBOLIC_COLOR_FOREGROUND].alpha;
          n_used_colors = MIN (n_colors, 5);
          used_colors = solid_colors;
          for (unsigned int i = 0; i < n_used_colors; i++)
            {
              solid_colors[i] = colors[i];
              solid_colors[i].alpha = 1;
            }
        }
      else
        {
          used_opacity = 1;
          used_colors = colors;
          n_used_colors = n_colors;
        }

      self->current_width = width;
      self->current_height = height;

      compute_context.svg = self;
      compute_context.viewport = &viewport;
      compute_context.colors = used_colors;
      compute_context.n_colors = n_used_colors;
      compute_context.current_time = self->current_time;
      compute_context.parent = NULL;
      compute_context.interpolation = GDK_COLOR_STATE_SRGB;

      compute_current_values_for_shape (self->content, &compute_context);

      gtk_snapshot_push_collect (snapshot);

      if (self->gpa_version == 0 &&
          (self->features & GTK_SVG_TRADITIONAL_SYMBOLIC) != 0 &&
          used_opacity < 1)
        {
          gtk_snapshot_push_opacity (snapshot, used_opacity);
        }

      paint_context.svg = self;
      paint_context.viewport = &viewport;
      paint_context.viewport_stack = NULL;
      paint_context.snapshot = snapshot;
      paint_context.colors = used_colors;
      paint_context.n_colors = n_used_colors;
      paint_context.weight = self->weight >= 1 ? self->weight : weight;
      paint_context.op = RENDERING;
      paint_context.op_stack = NULL;
      paint_context.ctx_shape_stack = NULL;
      paint_context.current_time = self->current_time;
      paint_context.depth = 0;
      paint_context.transforms = NULL;
      paint_context.instance_count = 0;
      paint_context.picking.picking = FALSE;

      if (self->overflow == GTK_OVERFLOW_HIDDEN)
        gtk_snapshot_push_clip (snapshot,
                                &GRAPHENE_RECT_INIT (0, 0, width, height));

      render_shape (self->content, &paint_context);

      if (self->overflow == GTK_OVERFLOW_HIDDEN)
        gtk_snapshot_pop (snapshot);

      /* Sanity checks. */
      g_assert (paint_context.viewport_stack == NULL);
      g_assert (paint_context.op_stack == NULL);
      g_assert (paint_context.ctx_shape_stack == NULL);
      g_assert (paint_context.transforms == NULL);

      if (self->gpa_version == 0 &&
          (self->features & GTK_SVG_TRADITIONAL_SYMBOLIC) != 0 &&
          used_opacity < 1)
        {
          gtk_snapshot_pop (snapshot);
        }

      self->node = gtk_snapshot_pop_collect (snapshot);

      self->node_for.width = width;
      self->node_for.height = height;
      memcpy (self->node_for.colors, colors, n_colors * sizeof (GdkRGBA));
      self->node_for.n_colors = n_colors;
      self->node_for.weight = weight;
    }

  if (self->node)
    gtk_snapshot_append_node (snapshot, self->node);

  gtk_svg_advance_after_snapshot (self);
}

/* }}} */

/* vim:set foldmethod=marker: */
