/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#include "gtkrender.h"

#include <math.h>

#include "gtkcsscornervalueprivate.h"
#include "gtkcssimagevalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcssshadowvalueprivate.h"
#include "gtkcsstransformvalueprivate.h"
#include "gtkrendericonprivate.h"
#include "gtkrenderborderprivate.h"
#include "gtkrenderbackgroundprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtksettings.h"

#include "gsk/gskroundedrectprivate.h"
#include <gdk/gdktextureprivate.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void
gtk_do_render_icon (GtkStyleContext        *context,
                    cairo_t                *cr,
                    double                  x,
                    double                  y,
                    double                  width,
                    double                  height)
{
  GtkSnapshot *snapshot;
  GskRenderNode *node;

  snapshot = gtk_snapshot_new ();
  gtk_css_style_snapshot_icon (gtk_style_context_lookup_style (context), snapshot, width, height);
  node = gtk_snapshot_free_to_node (snapshot);
  if (node == NULL)
    return;

  cairo_save (cr);
  cairo_translate (cr, x, y);
  gsk_render_node_draw (node, cr);
  cairo_restore (cr);

  gsk_render_node_unref (node);
}

/**
 * gtk_render_check:
 * @context: a `GtkStyleContext`
 * @cr: a `cairo_t`
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a checkmark (as in a `GtkCheckButton`).
 *
 * The %GTK_STATE_FLAG_CHECKED state determines whether the check is
 * on or off, and %GTK_STATE_FLAG_INCONSISTENT determines whether it
 * should be marked as undefined.
 *
 * Typical checkmark rendering:
 *
 * ![](checks.png)
 *
 * Deprecated: 4.10
 **/
void
gtk_render_check (GtkStyleContext *context,
                  cairo_t         *cr,
                  double           x,
                  double           y,
                  double           width,
                  double           height)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  gtk_do_render_icon (context, cr, x, y, width, height);
}

/**
 * gtk_render_option:
 * @context: a `GtkStyleContext`
 * @cr: a `cairo_t`
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders an option mark (as in a radio button), the %GTK_STATE_FLAG_CHECKED
 * state will determine whether the option is on or off, and
 * %GTK_STATE_FLAG_INCONSISTENT whether it should be marked as undefined.
 *
 * Typical option mark rendering:
 *
 * ![](options.png)
 *
 * Deprecated: 4.10
 **/
void
gtk_render_option (GtkStyleContext *context,
                   cairo_t         *cr,
                   double           x,
                   double           y,
                   double           width,
                   double           height)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  gtk_do_render_icon (context, cr, x, y, width, height);
}

/**
 * gtk_render_arrow:
 * @context: a `GtkStyleContext`
 * @cr: a `cairo_t`
 * @angle: arrow angle from 0 to 2 * %G_PI, being 0 the arrow pointing to the north
 * @x: X origin of the render area
 * @y: Y origin of the render area
 * @size: square side for render area
 *
 * Renders an arrow pointing to @angle.
 *
 * Typical arrow rendering at 0, 1⁄2 π;, π; and 3⁄2 π:
 *
 * ![](arrows.png)
 *
 * Deprecated: 4.10
 **/
void
gtk_render_arrow (GtkStyleContext *context,
                  cairo_t         *cr,
                  double           angle,
                  double           x,
                  double           y,
                  double           size)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (size <= 0)
    return;

  gtk_do_render_icon (context, cr, x, y, size, size);
}

/**
 * gtk_render_background:
 * @context: a `GtkStyleContext`
 * @cr: a `cairo_t`
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders the background of an element.
 *
 * Typical background rendering, showing the effect of
 * `background-image`, `border-width` and `border-radius`:
 *
 * ![](background.png)
 *
 * Deprecated: 4.10
 **/
void
gtk_render_background (GtkStyleContext *context,
                       cairo_t         *cr,
                       double           x,
                       double           y,
                       double           width,
                       double           height)
{
  GtkSnapshot *snapshot;
  GskRenderNode *node;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  snapshot = gtk_snapshot_new ();
  gtk_snapshot_render_background (snapshot, context, x, y, width, height);
  node = gtk_snapshot_free_to_node (snapshot);
  if (node == NULL)
    return;

  cairo_save (cr);
  gsk_render_node_draw (node, cr);
  cairo_restore (cr);

  gsk_render_node_unref (node);
}

/**
 * gtk_render_frame:
 * @context: a `GtkStyleContext`
 * @cr: a `cairo_t`
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a frame around the rectangle defined by @x, @y, @width, @height.
 *
 * Examples of frame rendering, showing the effect of `border-image`,
 * `border-color`, `border-width`, `border-radius` and junctions:
 *
 * ![](frames.png)
 *
 * Deprecated: 4.10
 **/
void
gtk_render_frame (GtkStyleContext *context,
                  cairo_t         *cr,
                  double           x,
                  double           y,
                  double           width,
                  double           height)
{
  GtkSnapshot *snapshot;
  GskRenderNode *node;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  snapshot = gtk_snapshot_new ();
  gtk_snapshot_render_frame (snapshot, context, x, y, width, height);
  node = gtk_snapshot_free_to_node (snapshot);
  if (node == NULL)
    return;

  cairo_save (cr);
  gsk_render_node_draw (node, cr);
  cairo_restore (cr);

  gsk_render_node_unref (node);
}

/**
 * gtk_render_expander:
 * @context: a `GtkStyleContext`
 * @cr: a `cairo_t`
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders an expander (as used in `GtkTreeView` and `GtkExpander`) in the area
 * defined by @x, @y, @width, @height. The state %GTK_STATE_FLAG_CHECKED
 * determines whether the expander is collapsed or expanded.
 *
 * Typical expander rendering:
 *
 * ![](expanders.png)
 *
 * Deprecated: 4.10
 **/
void
gtk_render_expander (GtkStyleContext *context,
                     cairo_t         *cr,
                     double           x,
                     double           y,
                     double           width,
                     double           height)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  gtk_do_render_icon (context, cr, x, y, width, height);
}

/**
 * gtk_render_focus:
 * @context: a `GtkStyleContext`
 * @cr: a `cairo_t`
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a focus indicator on the rectangle determined by @x, @y, @width, @height.
 *
 * Typical focus rendering:
 *
 * ![](focus.png)
 *
 * Deprecated: 4.10
 **/
void
gtk_render_focus (GtkStyleContext *context,
                  cairo_t         *cr,
                  double           x,
                  double           y,
                  double           width,
                  double           height)
{
  GtkSnapshot *snapshot;
  GskRenderNode *node;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  snapshot = gtk_snapshot_new ();
  gtk_snapshot_render_frame (snapshot, context, x, y, width, height);
  node = gtk_snapshot_free_to_node (snapshot);
  if (node == NULL)
    return;

  cairo_save (cr);
  gsk_render_node_draw (node, cr);
  cairo_restore (cr);

  gsk_render_node_unref (node);
}

/**
 * gtk_render_layout:
 * @context: a `GtkStyleContext`
 * @cr: a `cairo_t`
 * @x: X origin
 * @y: Y origin
 * @layout: the `PangoLayout` to render
 *
 * Renders @layout on the coordinates @x, @y
 *
 * Deprecated: 4.10
 **/
void
gtk_render_layout (GtkStyleContext *context,
                   cairo_t         *cr,
                   double           x,
                   double           y,
                   PangoLayout     *layout)
{
  GtkSnapshot *snapshot;
  GskRenderNode *node;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (PANGO_IS_LAYOUT (layout));
  g_return_if_fail (cr != NULL);

  snapshot = gtk_snapshot_new ();
  gtk_snapshot_render_layout (snapshot, context, x, y, layout); 
  node = gtk_snapshot_free_to_node (snapshot);
  if (node == NULL)
    return;

  cairo_save (cr);
  gsk_render_node_draw (node, cr);
  cairo_restore (cr);

  gsk_render_node_unref (node);
}

/**
 * gtk_render_line:
 * @context: a `GtkStyleContext`
 * @cr: a `cairo_t`
 * @x0: X coordinate for the origin of the line
 * @y0: Y coordinate for the origin of the line
 * @x1: X coordinate for the end of the line
 * @y1: Y coordinate for the end of the line
 *
 * Renders a line from (x0, y0) to (x1, y1).
 *
 * Deprecated: 4.10
 **/
void
gtk_render_line (GtkStyleContext *context,
                 cairo_t         *cr,
                 double           x0,
                 double           y0,
                 double           x1,
                 double           y1)
{
  const GdkRGBA *color;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  cairo_save (cr);

  color = gtk_css_color_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR));

  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
  cairo_set_line_width (cr, 1);

  cairo_move_to (cr, x0 + 0.5, y0 + 0.5);
  cairo_line_to (cr, x1 + 0.5, y1 + 0.5);

  gdk_cairo_set_source_rgba (cr, color);
  cairo_stroke (cr);

  cairo_restore (cr);
}

/**
 * gtk_render_handle:
 * @context: a `GtkStyleContext`
 * @cr: a `cairo_t`
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a handle (as in `GtkPaned` and `GtkWindow`’s resize grip),
 * in the rectangle determined by @x, @y, @width, @height.
 *
 * Handles rendered for the paned and grip classes:
 *
 * ![](handles.png)
 *
 * Deprecated: 4.10
 **/
void
gtk_render_handle (GtkStyleContext *context,
                   cairo_t         *cr,
                   double           x,
                   double           y,
                   double           width,
                   double           height)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  gtk_render_background (context, cr, x, y, width, height);
  gtk_render_frame (context, cr, x, y, width, height);

  gtk_do_render_icon (context, cr, x, y, width, height);
}

/**
 * gtk_render_activity:
 * @context: a `GtkStyleContext`
 * @cr: a `cairo_t`
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders an activity indicator (such as in `GtkSpinner`).
 * The state %GTK_STATE_FLAG_CHECKED determines whether there is
 * activity going on.
 *
 * Deprecated: 4.10
 **/
void
gtk_render_activity (GtkStyleContext *context,
                     cairo_t         *cr,
                     double           x,
                     double           y,
                     double           width,
                     double           height)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  gtk_do_render_icon (context, cr, x, y, width, height);
}

/**
 * gtk_render_icon:
 * @context: a `GtkStyleContext`
 * @cr: a `cairo_t`
 * @texture: a `GdkTexture` containing the icon to draw
 * @x: X position for the @texture
 * @y: Y position for the @texture
 *
 * Renders the icon in @texture at the specified @x and @y coordinates.
 *
 * This function will render the icon in @texture at exactly its size,
 * regardless of scaling factors, which may not be appropriate when
 * drawing on displays with high pixel densities.
 *
 * Deprecated: 4.10
 **/
void
gtk_render_icon (GtkStyleContext *context,
                 cairo_t         *cr,
                 GdkTexture      *texture,
                 double           x,
                 double           y)
{
  GtkSnapshot *snapshot;
  GskRenderNode *node;

  snapshot = gtk_snapshot_new ();
  gtk_css_style_snapshot_icon_paintable (gtk_style_context_lookup_style (context),
                                         snapshot,
                                         GDK_PAINTABLE (texture),
                                         gdk_texture_get_width (texture),
                                         gdk_texture_get_height (texture));
  node = gtk_snapshot_free_to_node (snapshot);
  if (node == NULL)
    return;

  cairo_save (cr);
  cairo_translate (cr, x, y);
  gsk_render_node_draw (node, cr);
  cairo_restore (cr);
}


/**
 * gtk_snapshot_render_background:
 * @snapshot: a `GtkSnapshot`
 * @context: the style context that defines the background
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Creates a render node for the CSS background according to @context,
 * and appends it to the current node of @snapshot, without changing
 * the current node.
 *
 * Deprecated: 4.10
 */
void
gtk_snapshot_render_background (GtkSnapshot     *snapshot,
                                GtkStyleContext *context,
                                double           x,
                                double           y,
                                double           width,
                                double           height)
{
  GtkCssBoxes boxes;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_css_boxes_init_border_box (&boxes,
                                 gtk_style_context_lookup_style (context),
                                 x, y, width, height);
  gtk_css_style_snapshot_background (&boxes, snapshot);
}

/**
 * gtk_snapshot_render_frame:
 * @snapshot: a `GtkSnapshot`
 * @context: the style context that defines the frame
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Creates a render node for the CSS border according to @context,
 * and appends it to the current node of @snapshot, without changing
 * the current node.
 *
 * Deprecated: 4.10
 */
void
gtk_snapshot_render_frame (GtkSnapshot     *snapshot,
                           GtkStyleContext *context,
                           double           x,
                           double           y,
                           double           width,
                           double           height)
{
  GtkCssBoxes boxes;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_css_boxes_init_border_box (&boxes,
                                 gtk_style_context_lookup_style (context),
                                 x, y, width, height);
  gtk_css_style_snapshot_border (&boxes, snapshot);
}

/**
 * gtk_snapshot_render_focus:
 * @snapshot: a `GtkSnapshot`
 * @context: the style context that defines the focus ring
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Creates a render node for the focus outline according to @context,
 * and appends it to the current node of @snapshot, without changing
 * the current node.
 *
 * Deprecated: 4.10
 */
void
gtk_snapshot_render_focus (GtkSnapshot     *snapshot,
                           GtkStyleContext *context,
                           double           x,
                           double           y,
                           double           width,
                           double           height)
{
  GtkCssBoxes boxes;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_css_boxes_init_border_box (&boxes,
                                 gtk_style_context_lookup_style (context),
                                 x, y, width, height);
  gtk_css_style_snapshot_outline (&boxes, snapshot);
}

/**
 * gtk_snapshot_render_layout:
 * @snapshot: a `GtkSnapshot`
 * @context: the style context that defines the text
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @layout: the `PangoLayout` to render
 *
 * Creates a render node for rendering @layout according to the style
 * information in @context, and appends it to the current node of @snapshot,
 * without changing the current node.
 *
 * Deprecated: 4.10
 */
void
gtk_snapshot_render_layout (GtkSnapshot     *snapshot,
                            GtkStyleContext *context,
                            double           x,
                            double           y,
                            PangoLayout     *layout)
{
  const bool needs_translate = (x != 0 || y != 0);
  const GdkRGBA *fg_color;
  GtkCssValue *shadows_value;
  gboolean has_shadow;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (PANGO_IS_LAYOUT (layout));

  if (needs_translate)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
    }

  fg_color = gtk_css_color_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR));

  shadows_value = _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_TEXT_SHADOW);
  has_shadow = gtk_css_shadow_value_push_snapshot (shadows_value, snapshot);

  gtk_snapshot_append_layout (snapshot, layout, fg_color);

  if (has_shadow)
    gtk_snapshot_pop (snapshot);

  if (needs_translate)
    gtk_snapshot_restore (snapshot);
}

static void
draw_insertion_cursor (GtkStyleContext *context,
                       cairo_t         *cr,
                       double           x,
                       double           y,
                       double           width,
                       double           height,
                       double           aspect_ratio,
                       gboolean         is_primary,
                       PangoDirection   direction,
                       gboolean         draw_arrow)
{
  GdkRGBA primary_color;
  GdkRGBA secondary_color;
  int stem_width;
  double angle;
  double dx, dy;
  double xx1, yy1, xx2, yy2;

  cairo_save (cr);
  cairo_new_path (cr);

  _gtk_style_context_get_cursor_color (context, &primary_color, &secondary_color);
  gdk_cairo_set_source_rgba (cr, is_primary ? &primary_color : &secondary_color);

  stem_width = height * aspect_ratio + 1;

  yy1 = y;
  yy2 = y + height;

  if (width < 0)
    {
      xx1 = x;
      xx2 = x - width;
    }
  else
    {
      xx1 = x + width;
      xx2 = x;
    }

  angle = atan2 (height, width);

  dx = (stem_width/2.0) * cos (M_PI/2 - angle);
  dy = (stem_width/2.0) * sin (M_PI/2 - angle);

  if (draw_arrow)
    {
      if (direction == PANGO_DIRECTION_RTL)
        {
          double x0, y0, x1, y1, x2, y2;

          x0 = xx2 - dx + 2 * dy;
          y0 = yy2 - dy - 2 * dx;

          x1 = x0 + 4 * dy;
          y1 = y0 - 4 * dx;
          x2 = x0 + 2 * dy - 3 * dx;
          y2 = y0 - 2 * dx - 3 * dy;

          cairo_move_to (cr, xx1 + dx, yy1 + dy);
          cairo_line_to (cr, xx2 + dx, yy2 + dy);
          cairo_line_to (cr, x2, y2);
          cairo_line_to (cr, x1, y1);
          cairo_line_to (cr, xx1 - dx, yy1 - dy);
        }
      else if (direction == PANGO_DIRECTION_LTR)
        {
          double x0, y0, x1, y1, x2, y2;

          x0 = xx2 + dx + 2 * dy;
          y0 = yy2 + dy - 2 * dx;

          x1 = x0 + 4 * dy;
          y1 = y0 - 4 * dx;
          x2 = x0 + 2 * dy + 3 * dx;
          y2 = y0 - 2 * dx + 3 * dy;

          cairo_move_to (cr, xx1 - dx, yy1 - dy);
          cairo_line_to (cr, xx2 - dx, yy2 - dy);
          cairo_line_to (cr, x2, y2);
          cairo_line_to (cr, x1, y1);
          cairo_line_to (cr, xx1 + dx, yy1 + dy);
        }
      else
        g_assert_not_reached();
    }
  else
    {
      cairo_move_to (cr, xx1 + dx, yy1 + dy);
      cairo_line_to (cr, xx2 + dx, yy2 + dy);
      cairo_line_to (cr, xx2 - dx, yy2 - dy);
      cairo_line_to (cr, xx1 - dx, yy1 - dy);
    }

  cairo_fill (cr);

  cairo_restore (cr);
}

static void
get_insertion_cursor_bounds (double           width,
                             double           height,
                             double           aspect_ratio,
                             PangoDirection   direction,
                             gboolean         draw_arrow,
                             graphene_rect_t *bounds)
{
  int stem_width;

  if (width < 0)
    width = - width;

  stem_width = height * aspect_ratio + 1;

  graphene_rect_init (bounds,
                      - 2 * stem_width, - stem_width,
                      width + 4 * stem_width, height + 2 * stem_width);
}

static void
snapshot_insertion_cursor (GtkSnapshot     *snapshot,
                           GtkStyleContext *context,
                           double           width,
                           double           height,
                           double           aspect_ratio,
                           gboolean         is_primary,
                           PangoDirection   direction,
                           gboolean         draw_arrow)
{
  if (width != 0 || draw_arrow)
    {
      cairo_t *cr;
      graphene_rect_t bounds;

      get_insertion_cursor_bounds (width, height, aspect_ratio, direction, draw_arrow, &bounds);
      cr = gtk_snapshot_append_cairo (snapshot, &bounds);

      draw_insertion_cursor (context, cr, 0, 0, width, height, aspect_ratio, is_primary, direction, draw_arrow);

      cairo_destroy (cr);
    }
  else
    {
      GdkRGBA primary_color;
      GdkRGBA secondary_color;
      int stem_width;
      int offset;

      _gtk_style_context_get_cursor_color (context, &primary_color, &secondary_color);

      stem_width = height * aspect_ratio + 1;

      /* put (stem_width % 2) on the proper side of the cursor */
      if (direction == PANGO_DIRECTION_LTR)
        offset = stem_width / 2;
      else
        offset = stem_width - stem_width / 2;

      gtk_snapshot_append_color (snapshot,
                                 is_primary ? &primary_color : &secondary_color,
                                 &GRAPHENE_RECT_INIT (- offset, 0, stem_width, height));
    }
}

/**
 * gtk_snapshot_render_insertion_cursor:
 * @snapshot: snapshot to render to
 * @context: a `GtkStyleContext`
 * @x: X origin
 * @y: Y origin
 * @layout: the `PangoLayout` of the text
 * @index: the index in the `PangoLayout`
 * @direction: the `PangoDirection` of the text
 *
 * Draws a text caret using @snapshot at the specified index of @layout.
 *
 * Deprecated: 4.10
 */
void
gtk_snapshot_render_insertion_cursor (GtkSnapshot     *snapshot,
                                      GtkStyleContext *context,
                                      double           x,
                                      double           y,
                                      PangoLayout     *layout,
                                      int              index,
                                      PangoDirection   direction)
{
  GdkDisplay *display;
  gboolean split_cursor;
  double aspect_ratio;
  PangoRectangle strong_pos, weak_pos;
  PangoRectangle *cursor1, *cursor2;
  GdkSeat *seat;
  PangoDirection keyboard_direction;
  PangoDirection direction2;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (PANGO_IS_LAYOUT (layout));
  g_return_if_fail (index >= 0);

  display = gtk_style_context_get_display (context);

  g_object_get (gtk_settings_get_for_display (display),
                "gtk-split-cursor", &split_cursor,
                "gtk-cursor-aspect-ratio", &aspect_ratio,
                NULL);

  keyboard_direction = PANGO_DIRECTION_LTR;
  seat = gdk_display_get_default_seat (display);
  if (seat)
    {
      GdkDevice *keyboard = gdk_seat_get_keyboard (seat);

      if (keyboard)
        keyboard_direction = gdk_device_get_direction (keyboard);
    }

  pango_layout_get_caret_pos (layout, index, &strong_pos, &weak_pos);

  direction2 = PANGO_DIRECTION_NEUTRAL;

  if (split_cursor)
    {
      cursor1 = &strong_pos;

      if (strong_pos.x != weak_pos.x || strong_pos.y != weak_pos.y)
        {
          direction2 = (direction == PANGO_DIRECTION_LTR) ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR;
          cursor2 = &weak_pos;
        }
    }
  else
    {
      if (keyboard_direction == direction)
        cursor1 = &strong_pos;
      else
        cursor1 = &weak_pos;
    }

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x + PANGO_PIXELS (MIN (cursor1->x, cursor1->x + cursor1->width)), y + PANGO_PIXELS (cursor1->y)));
  snapshot_insertion_cursor (snapshot,
                             context,
                             PANGO_PIXELS (cursor1->width),
                             PANGO_PIXELS (cursor1->height),
                             aspect_ratio,
                             TRUE,
                             direction,
                             direction2 != PANGO_DIRECTION_NEUTRAL);
  gtk_snapshot_restore (snapshot);

  if (direction2 != PANGO_DIRECTION_NEUTRAL)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x + PANGO_PIXELS (MIN (cursor2->x, cursor2->x + cursor2->width)), y + PANGO_PIXELS (cursor2->y)));
      snapshot_insertion_cursor (snapshot,
                                 context,
                                 PANGO_PIXELS (cursor2->width),
                                 PANGO_PIXELS (cursor2->height),
                                 aspect_ratio,
                                 FALSE,
                                 direction2,
                                 TRUE);
      gtk_snapshot_restore (snapshot);
    }
}
