/* GTK - The GIMP Toolkit
 * Copyright (C) 2016 Benjamin Otte <otte@gnome.org>
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

#include "gtksnapshot.h"
#include "gtksnapshotprivate.h"

#include "gtkcssrgbavalueprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkrenderbackgroundprivate.h"
#include "gtkrenderborderprivate.h"
#include "gtkrendericonprivate.h"
#include "gtkstylecontextprivate.h"

#include "gsk/gskrendernodeprivate.h"


/**
 * SECTION:gtksnapshot
 * @Short_description: Auxiliary object for snapshots
 * @Title: GtkSnapshot
 *
 * GtkSnapshot is an auxiliary object that assists in creating #GskRenderNodes
 * in the #GtkWidget::snapshot vfunc. It functions in a similar way to
 * a cairo context, and maintains a stack of render nodes and their associated
 * transformations.
 *
 * The node at the top of the stack is the the one that gtk_snapshot_append_node()
 * operates on. Use the gtk_snapshot_push_() and gtk_snapshot_pop() functions to
 * change the current node.
 *
 * The only way to obtain a #GtkSnapshot object is as an argument to
 * the #GtkWidget::snapshot vfunc.
 */

static GtkSnapshotState *
gtk_snapshot_state_new (GtkSnapshotState *parent,
                        GskRenderNode    *node)
{
  GtkSnapshotState *state;

  state = g_slice_new0 (GtkSnapshotState);

  state->node = node;
  state->parent = parent;
  graphene_matrix_init_identity (&state->transform);

  return state;
}

static void
gtk_snapshot_state_free (GtkSnapshotState *state)
{
  g_slice_free (GtkSnapshotState, state);
}

static void
gtk_snapshot_state_set_transform (GtkSnapshotState        *state,
                                  const graphene_matrix_t *transform)
{
  graphene_matrix_init_from_matrix (&state->transform, transform);

  state->world_is_valid = FALSE;
}

static const graphene_matrix_t *
gtk_snapshot_state_get_world_transform (GtkSnapshotState *state)
{
  if (!state->world_is_valid)
    {
      if (state->parent)
        graphene_matrix_multiply (gtk_snapshot_state_get_world_transform (state->parent),
                                  &state->transform,
                                  &state->world_transform);
      else
        graphene_matrix_init_from_matrix (&state->world_transform, &state->transform);

      state->world_is_valid = TRUE;
    }

  return &state->world_transform;
}

void
gtk_snapshot_init (GtkSnapshot          *snapshot,
                   GskRenderer          *renderer,
                   const cairo_region_t *clip,
                   const char           *name,
                   ...)
{
  cairo_rectangle_int_t extents;

  cairo_region_get_extents (clip, &extents);

  snapshot->state = NULL;
  snapshot->renderer = renderer;
  snapshot->clip_region = clip;
  snapshot->root = gsk_container_node_new ();

  if (name)
    {
      va_list args;
      char *str;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);

      gsk_render_node_set_name (snapshot->root, str);

      g_free (str);
    }

  snapshot->state = gtk_snapshot_state_new (NULL, snapshot->root);
}

GskRenderNode *
gtk_snapshot_finish (GtkSnapshot *snapshot)
{
  gtk_snapshot_pop (snapshot);

  if (snapshot->state != NULL)
    {
      g_warning ("Too many gtk_snapshot_push() calls.");
    }

  return snapshot->root;
}

/**
 * gtk_snapshot_push_node:
 * @snapshot: a #GtkSnapshot
 * @node: the render node to push
 *
 * Appends @node to the current render node of @snapshot,
 * and makes @node the new current render node.
 *
 * Since: 3.90
 */
void
gtk_snapshot_push_node (GtkSnapshot   *snapshot,
                        GskRenderNode *node)
{
  gtk_snapshot_append_node (snapshot, node);

  snapshot->state = gtk_snapshot_state_new (snapshot->state, node);
}

/**
 * gtk_snapshot_push:
 * @snapshot: a #GtkSnapshot
 * @bounds: the bounds for the new node
 * @name: (transfer none): a printf() style format string for the name for the new node
 * @...: arguments to insert into the format string
 *
 * Creates a new render node, appends it to the current render
 * node of @snapshot, and makes it the new current render node.
 *
 * Since: 3.90
 */
void
gtk_snapshot_push (GtkSnapshot           *snapshot,
                   const char            *name,
                   ...)
{
  GskRenderNode *node;

  node = gsk_container_node_new ();

  if (name)
    {
      va_list args;
      char *str;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);

      gsk_render_node_set_name (node, str);

      g_free (str);
    }

  gtk_snapshot_push_node (snapshot, node);
  gsk_render_node_unref (node);
}

/**
 * gtk_snapshot_pop:
 * @snapshot: a #GtkSnapshot
 *
 * Removes the top element from the stack of render nodes,
 * making the node underneath the current node again.
 *
 * Since: 3.90
 */
void
gtk_snapshot_pop (GtkSnapshot *snapshot)
{
  GtkSnapshotState *state;

  if (snapshot->state == NULL)
    {
      g_warning ("Too many gtk_snapshot_pop() calls.");
      return;
    }

  state = snapshot->state;
  snapshot->state = state->parent;

  gtk_snapshot_state_free (state);
}

/**
 * gtk_snapshot_get_renderer:
 * @snapshot: a #GtkSnapshot
 *
 * Obtains the #GskRenderer that this snapshot will be
 * rendered with.
 *
 * Returns: (transfer none): the #GskRenderer
 *
 * Since: 3.90
 */
GskRenderer *
gtk_snapshot_get_renderer (const GtkSnapshot *snapshot)
{
  return snapshot->renderer;
}

/**
 * gtk_snapshot_set_transform:
 * @snapshot: a #GtkSnapshot
 * @transform: a transformation matrix
 *
 * Replaces the current transformation with the given @transform.
 *
 * Since: 3.90
 */
void
gtk_snapshot_set_transform (GtkSnapshot             *snapshot,
                            const graphene_matrix_t *transform)
{
  g_return_if_fail (snapshot->state != NULL);

  gtk_snapshot_state_set_transform (snapshot->state, transform);
}

/**
 * gtk_snapshot_transform:
 * @snapshot: a #GtkSnapshot
 * @transform: a transformation matrix
 *
 * Appends @transform to the current transformation.
 *
 * Since: 3.90
 */
void
gtk_snapshot_transform (GtkSnapshot             *snapshot,
                        const graphene_matrix_t *transform)
{
  graphene_matrix_t result;

  g_return_if_fail (snapshot->state != NULL);

  graphene_matrix_multiply (transform, &snapshot->state->transform, &result);
  gtk_snapshot_state_set_transform (snapshot->state, &result);
}

/**
 * gtk_snapshot_translate_2d:
 * @snapshot: a $GtkSnapshot
 * @x: horizontal translation
 * @y: vertical translation
 *
 * Appends a translation by (@x, @y) to the current transformation.
 *
 * Since: 3.90
 */
void
gtk_snapshot_translate_2d (GtkSnapshot *snapshot,
                           int          x,
                           int          y)
{
  graphene_matrix_t transform;
  graphene_point3d_t point;

  graphene_point3d_init (&point, x, y, 0);
  graphene_matrix_init_translate (&transform, &point);
  gtk_snapshot_transform (snapshot, &transform);
}

/**
 * gtk_snapshot_append_node:
 * @snapshot: a #GtkSnapshot
 * @node: a #GskRenderNode
 *
 * Appends @node to the current render node of @snapshot,
 * without changing the current node. If @snapshot does
 * not have a current node yet, @node will become the
 * initial node.
 */
void
gtk_snapshot_append_node (GtkSnapshot   *snapshot,
                          GskRenderNode *node)
{
  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GSK_IS_RENDER_NODE (node));

  if (snapshot->state)
    {
      gsk_render_node_append_child (snapshot->state->node, node);
      gsk_render_node_set_transform (node, &snapshot->state->transform);
    }
  else
    {
      g_critical ("Tried appending a node to an already finished snapshot.");
    }
}

/**
 * gtk_snapshot_append_cairo_node:
 * @snapshot: a #GtkSnapshot
 * @bounds: the bounds for the new node
 * @name: (transfer none): a printf() style format string for the name for the new node
 * @...: arguments to insert into the format string
 *
 * Creates a new render node and appends it to the current render
 * node of @snapshot, without changing the current node.
 *
 * Returns: a cairo_t suitable for drawing the contents of the newly
 *     created render node
 *
 * Since: 3.90
 */
cairo_t *
gtk_snapshot_append_cairo_node (GtkSnapshot           *snapshot,
                                const graphene_rect_t *bounds,
                                const char            *name,
                                ...)
{
  GskRenderNode *node;

  g_return_val_if_fail (snapshot != NULL, NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  node = gsk_cairo_node_new (bounds);

  if (name)
    {
      va_list args;
      char *str;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);

      gsk_render_node_set_name (node, str);

      g_free (str);
    }

  gtk_snapshot_append_node (snapshot, node);
  gsk_render_node_unref (node);

  return gsk_cairo_node_get_draw_context (node, snapshot->renderer);
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

/**
 * gtk_snapshot_clips_rect:
 * @snapshot: a #GtkSnapshot
 * @bounds: a rectangle
 *
 * Tests whether the rectangle is entirely outside the clip region of @snapshot.
 *
 * Returns: %TRUE is @bounds is entirely outside the clip region
 *
 * Since: 3.90
 */
gboolean
gtk_snapshot_clips_rect (GtkSnapshot           *snapshot,
                         const graphene_rect_t *bounds)
{
  cairo_rectangle_int_t rect;

  if (snapshot->state)
    {
      const graphene_matrix_t *world;
      graphene_rect_t transformed;

      world = gtk_snapshot_state_get_world_transform (snapshot->state);

      graphene_matrix_transform_bounds (world, bounds, &transformed);
      rectangle_init_from_graphene (&rect, &transformed);
    }
  else
    {
      rectangle_init_from_graphene (&rect, bounds);
    }

  return cairo_region_contains_rectangle (snapshot->clip_region, &rect) == CAIRO_REGION_OVERLAP_OUT;
}

/**
 * gtk_snapshot_render_background:
 * @snapshot: a #GtkSnapshot
 * @context: the #GtkStyleContext to use
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Creates a render node for the CSS background according to @context,
 * and appends it to the current node of @snapshot, without changing
 * the current node.
 *
 * Since: 3.90
 */
void
gtk_snapshot_render_background (GtkSnapshot     *snapshot,
                                GtkStyleContext *context,
                                gdouble          x,
                                gdouble          y,
                                gdouble          width,
                                gdouble          height)
{
  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_snapshot_translate_2d (snapshot, x, y);
  gtk_css_style_snapshot_background (gtk_style_context_lookup_style (context),
                                     snapshot,
                                     width, height,
                                     gtk_style_context_get_junction_sides (context));
  gtk_snapshot_translate_2d (snapshot, -x, -y);
}

/**
 * gtk_snapshot_render_frame:
 * @snapshot: a #GtkSnapshot
 * @context: the #GtkStyleContext to use
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Creates a render node for the CSS border according to @context,
 * and appends it to the current node of @snapshot, without changing
 * the current node.
 *
 * Since: 3.90
 */
void
gtk_snapshot_render_frame (GtkSnapshot     *snapshot,
                           GtkStyleContext *context,
                           gdouble          x,
                           gdouble          y,
                           gdouble          width,
                           gdouble          height)
{
  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_snapshot_translate_2d (snapshot, x, y);
  gtk_css_style_snapshot_border (gtk_style_context_lookup_style (context),
                                 snapshot,
                                 width, height,
                                 gtk_style_context_get_junction_sides (context));
  gtk_snapshot_translate_2d (snapshot, -x, -y);
}

/**
 * gtk_snapshot_render_focus:
 * @snapshot: a #GtkSnapshot
 * @context: the #GtkStyleContext to use
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Creates a render node for the focus outline according to @context,
 * and appends it to the current node of @snapshot, without changing
 * the current node.
 *
 * Since: 3.90
 */
void
gtk_snapshot_render_focus (GtkSnapshot     *snapshot,
                           GtkStyleContext *context,
                           gdouble          x,
                           gdouble          y,
                           gdouble          width,
                           gdouble          height)
{
  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_snapshot_translate_2d (snapshot, x, y);
  gtk_css_style_snapshot_outline (gtk_style_context_lookup_style (context),
                                  snapshot,
                                  width, height);
  gtk_snapshot_translate_2d (snapshot, -x, -y);
}

/**
 * gtk_snapshot_render_layout:
 * @snapshot: a #GtkSnapshot
 * @context: the #GtkStyleContext to use
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @layout: the #PangoLayout to render
 *
 * Creates a render node for rendering @layout according to the style
 * information in @context, and appends it to the current node of @snapshot,
 * without changing the current node.
 *
 * Since: 3.90
 */
void
gtk_snapshot_render_layout (GtkSnapshot     *snapshot,
                            GtkStyleContext *context,
                            gdouble          x,
                            gdouble          y,
                            PangoLayout     *layout)
{
  const GdkRGBA *fg_color;
  graphene_rect_t bounds;
  GtkBorder shadow_extents;
  PangoRectangle ink_rect;
  GtkCssValue *shadow;
  cairo_t *cr;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (PANGO_IS_LAYOUT (layout));

  fg_color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR));
  shadow = _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_TEXT_SHADOW);
  pango_layout_get_pixel_extents (layout, &ink_rect, NULL);
  _gtk_css_shadows_value_get_extents (shadow, &shadow_extents);
  graphene_rect_init (&bounds,
                      ink_rect.x - shadow_extents.left,
                      ink_rect.y - shadow_extents.top,
                      ink_rect.width + shadow_extents.left + shadow_extents.right,
                      ink_rect.height + shadow_extents.top + shadow_extents.bottom);

  gtk_snapshot_translate_2d (snapshot, x, y);

  cr = gtk_snapshot_append_cairo_node (snapshot, &bounds, "Text<%dchars>", pango_layout_get_character_count (layout));

  _gtk_css_shadows_value_paint_layout (shadow, cr, layout);

  gdk_cairo_set_source_rgba (cr, fg_color);
  pango_cairo_show_layout (cr, layout);

  cairo_destroy (cr);
  gtk_snapshot_translate_2d (snapshot, -x, -y);
}

/**
 * gtk_snapshot_render_icon:
 * @snapshot: a #GtkSnapshot
 * @context: the #GtkStyleContext to use
 * @pixbuf: the #GdkPixbuf to render
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 *
 * Creates a render node for rendering @pixbuf according to the style
 * information in @context, and appends it to the current node of @snapshot,
 * without changing the current node.
 *
 * Since: 3.90
 */
void
gtk_snapshot_render_icon (GtkSnapshot     *snapshot,
                          GtkStyleContext *context,
                          GdkPixbuf       *pixbuf,
                          gdouble          x,
                          gdouble          y)
{
  GskTexture *texture;

  texture = gsk_texture_new_for_pixbuf (pixbuf);
  gtk_snapshot_translate_2d (snapshot, x, y);
  gtk_css_style_snapshot_icon_texture (gtk_style_context_lookup_style (context),
                                       snapshot,
                                       texture,
                                       1);
  gtk_snapshot_translate_2d (snapshot, -x, -y);
  gsk_texture_unref (texture);
}

