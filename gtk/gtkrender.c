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
#include "gtkstylecontextprivate.h"

#include "gsk/gskroundedrectprivate.h"
#include <gdk/gdktextureprivate.h>

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
 **/
void
gtk_render_line (GtkStyleContext *context,
                 cairo_t         *cr,
                 double           x0,
                 double           y0,
                 double           x1,
                 double           y1)
{
  GtkCssStyle *style;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  cairo_save (cr);

  style = gtk_style_context_lookup_style (context);

  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
  cairo_set_line_width (cr, 1);

  cairo_move_to (cr, x0 + 0.5, y0 + 0.5);
  cairo_line_to (cr, x1 + 0.5, y1 + 0.5);

  gdk_cairo_set_source_rgba (cr, &style->core->_color);
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
