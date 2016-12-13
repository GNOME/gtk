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
 * operates on. Use the gtk_snapshot_push() and gtk_snapshot_pop() functions to
 * change the current node.
 *
 * The only way to obtain a #GtkSnapshot object is as an argument to
 * the #GtkWidget::snapshot vfunc.
 */

static GtkSnapshotState *
gtk_snapshot_state_new (GtkSnapshotState *parent,
                        char             *name,
                        cairo_region_t   *clip,
                        double            translate_x,
                        double            translate_y)
{
  GtkSnapshotState *state;

  state = g_slice_new0 (GtkSnapshotState);

  state->nodes = g_ptr_array_new_with_free_func ((GDestroyNotify) gsk_render_node_unref);

  state->parent = parent;
  state->name = name;
  state->translate_x = translate_x;
  state->translate_y = translate_y;
  if (clip)
    state->clip_region = cairo_region_reference (clip);

  return state;
}

static void
gtk_snapshot_state_free (GtkSnapshotState *state)
{
  g_ptr_array_unref (state->nodes);

  if (state->clip_region)
    cairo_region_destroy (state->clip_region);

  g_free (state->name);

  g_slice_free (GtkSnapshotState, state);
}

void
gtk_snapshot_init (GtkSnapshot          *snapshot,
                   GskRenderer          *renderer,
                   const cairo_region_t *clip,
                   const char           *name,
                   ...)
{
  char *str;

  snapshot->state = NULL;
  snapshot->renderer = renderer;

  if (name)
    {
      va_list args;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);
    }
  else
    str = NULL;

  snapshot->state = gtk_snapshot_state_new (NULL,
                                            str,
                                            (cairo_region_t *) clip,
                                            0, 0);
}

GskRenderNode *
gtk_snapshot_finish (GtkSnapshot *snapshot)
{
  GskRenderNode *result;
  
  result = gtk_snapshot_pop (snapshot);

  if (snapshot->state != NULL)
    {
      g_warning ("Too many gtk_snapshot_push() calls.");
    }

  return result;
}

/**
 * gtk_snapshot_push:
 * @snapshot: a #GtkSnapshot
 * @keep_coordinates: If %TRUE, the current offset and clip will be kept.
 *     Otherwise, the clip will be unset and the offset will be reset to
 *     (0, 0).
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
                   gboolean               keep_coordinates,
                   const char            *name,
                   ...)
{
  char *str;

  if (name)
    {
      va_list args;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);
    }
  else
    str = NULL;

  if (keep_coordinates)
    {
      snapshot->state = gtk_snapshot_state_new (snapshot->state,
                                                str,
                                                snapshot->state->clip_region,
                                                snapshot->state->translate_x,
                                                snapshot->state->translate_y);
    }
  else
    {
      snapshot->state = gtk_snapshot_state_new (snapshot->state,
                                                str,
                                                NULL,
                                                0, 0);
    }
}

/**
 * gtk_snapshot_pop:
 * @snapshot: a #GtkSnapshot
 *
 * Removes the top element from the stack of render nodes,
 * making the node underneath the current node again.
 *
 * Returns: (transfer full) (allow none): A #GskRenderNode for
 *     the contents that were rendered to @snapshot since
 *     the corresponding gtk_snapshot_push() call
 *
 * Since: 3.90
 */
GskRenderNode *
gtk_snapshot_pop (GtkSnapshot *snapshot)
{
  GtkSnapshotState *state;
  GskRenderNode *node;

  if (snapshot->state == NULL)
    {
      g_warning ("Too many gtk_snapshot_pop() calls.");
      return NULL;
    }

  state = snapshot->state;
  snapshot->state = state->parent;

  if (state->nodes->len == 0)
    {
      node = NULL;
    }
  else if (state->nodes->len == 1)
    {
      node = gsk_render_node_ref (g_ptr_array_index (state->nodes, 0));
    }
  else
    {
      node = gsk_container_node_new ((GskRenderNode **) state->nodes->pdata,
                                     state->nodes->len);
      gsk_render_node_set_name (node, state->name);
    }

  gtk_snapshot_state_free (state);

  return node;
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
  snapshot->state->translate_x += x;
  snapshot->state->translate_y += y;
}

/**
 * gtk_snapshot_get_offset:
 * @snapshot: a #GtkSnapshot
 * @x: (out allow-none): return location for x offset
 * @y: (out allow-none): return location for y offset
 *
 * Queries the offset managed by @snapshot. This offset is the
 * accumulated sum of calls to gtk_snapshot_translate_2d().
 *
 * Use this offset to determine how to offset nodes that you
 * manually add to the snapshot using
 * gtk_snapshot_append_node().
 *
 * Note that other functions that add nodes for you, such as
 * gtk_snapshot_append_cairo_node() will add this offset for
 * you.
 **/
void
gtk_snapshot_get_offset (GtkSnapshot *snapshot,
                         double      *x,
                         double      *y)
{
  if (x)
    *x = snapshot->state->translate_x;

  if (y)
    *y = snapshot->state->translate_y;
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
      g_ptr_array_add (snapshot->state->nodes, gsk_render_node_ref (node));
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
  graphene_rect_t real_bounds;
  cairo_t *cr;

  g_return_val_if_fail (snapshot != NULL, NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  graphene_rect_offset_r (bounds, snapshot->state->translate_x, snapshot->state->translate_y, &real_bounds);
  node = gsk_cairo_node_new (&real_bounds);

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

  cr = gsk_cairo_node_get_draw_context (node, snapshot->renderer);

  cairo_translate (cr, snapshot->state->translate_x, snapshot->state->translate_y);

  return cr;
}

/**
 * gtk_snapshot_append_texture_node:
 * @snapshot: a #GtkSnapshot
 * @texture: the #GskTexture to render
 * @bounds: the bounds for the new node
 * @name: (transfer none): a printf() style format string for the name for the new node
 * @...: arguments to insert into the format string
 *
 * Creates a new render node drawing the @texture into the given @bounds and appends it
 * to the current render node of @snapshot.
 **/
void
gtk_snapshot_append_texture_node (GtkSnapshot            *snapshot,
                                  GskTexture             *texture,
                                  const graphene_rect_t  *bounds,
                                  const char             *name,
                                  ...)
{
  GskRenderNode *node;
  graphene_rect_t real_bounds;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GSK_IS_TEXTURE (texture));
  g_return_if_fail (bounds != NULL);

  graphene_rect_offset_r (bounds, snapshot->state->translate_x, snapshot->state->translate_y, &real_bounds);
  node = gsk_texture_node_new (texture, &real_bounds);

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
}

/**
 * gtk_snapshot_append_color_node:
 * @snapshot: a #GtkSnapshot
 * @color: the #GdkRGBA to draw
 * @bounds: the bounds for the new node
 * @name: (transfer none): a printf() style format string for the name for the new node
 * @...: arguments to insert into the format string
 *
 * Creates a new render node drawing the @color into the given @bounds and appends it
 * to the current render node of @snapshot.
 *
 * You should try to avoid calling this function if @color is transparent.
 **/
void
gtk_snapshot_append_color_node (GtkSnapshot           *snapshot,
                                const GdkRGBA         *color,
                                const graphene_rect_t *bounds,
                                const char            *name,
                                ...)
{
  GskRenderNode *node;
  graphene_rect_t real_bounds;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (color != NULL);
  g_return_if_fail (bounds != NULL);

  graphene_rect_offset_r (bounds, snapshot->state->translate_x, snapshot->state->translate_y, &real_bounds);
  node = gsk_color_node_new (color, &real_bounds);

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
  graphene_rect_t offset_bounds;
  cairo_rectangle_int_t rect;

  if (snapshot->state->clip_region == NULL)
    return FALSE;

  graphene_rect_offset_r (bounds, snapshot->state->translate_x, snapshot->state->translate_y, &offset_bounds);
  rectangle_init_from_graphene (&rect, &offset_bounds);

  return cairo_region_contains_rectangle (snapshot->state->clip_region, &rect) == CAIRO_REGION_OVERLAP_OUT;
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

