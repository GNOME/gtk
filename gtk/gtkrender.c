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

#include "gtkcssenginevalueprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkthemingengine.h"
#include "gtkthemingengineprivate.h"

/**
 * gtk_render_check:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a checkmark (as in a #GtkCheckButton).
 *
 * The %GTK_STATE_FLAG_ACTIVE state determines whether the check is
 * on or off, and %GTK_STATE_FLAG_INCONSISTENT determines whether it
 * should be marked as undefined.
 *
 * Typical checkmark rendering:
 *
 * ![](checks.png)
 *
 * Since: 3.0
 **/
void
gtk_render_check (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          x,
                  gdouble          y,
                  gdouble          width,
                  gdouble          height)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);
  cairo_new_path (cr);


  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_check (engine, cr,
                              x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_option:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders an option mark (as in a #GtkRadioButton), the %GTK_STATE_FLAG_ACTIVE
 * state will determine whether the option is on or off, and
 * %GTK_STATE_FLAG_INCONSISTENT whether it should be marked as undefined.
 *
 * Typical option mark rendering:
 *
 * ![](options.png)
 *
 * Since: 3.0
 **/
void
gtk_render_option (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   gdouble          width,
                   gdouble          height)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);
  cairo_new_path (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_option (engine, cr,
                               x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_arrow:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @angle: arrow angle from 0 to 2 * %G_PI, being 0 the arrow pointing to the north
 * @x: X origin of the render area
 * @y: Y origin of the render area
 * @size: square side for render area
 *
 * Renders an arrow pointing to @angle.
 *
 * Typical arrow rendering at 0, 1&solidus;2 &pi;, &pi; and 3&solidus;2 &pi;:
 *
 * ![](arrows.png)
 *
 * Since: 3.0
 **/
void
gtk_render_arrow (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          angle,
                  gdouble          x,
                  gdouble          y,
                  gdouble          size)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (size <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);
  cairo_new_path (cr);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_ARROW);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_arrow (engine, cr,
                              angle, x, y, size);

  gtk_style_context_restore (context);
  cairo_restore (cr);
}

/**
 * gtk_render_background:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
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
 * Since: 3.0.
 **/
void
gtk_render_background (GtkStyleContext *context,
                       cairo_t         *cr,
                       gdouble          x,
                       gdouble          y,
                       gdouble          width,
                       gdouble          height)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);
  cairo_new_path (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_background (engine, cr, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_frame:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
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
 * Since: 3.0
 **/
void
gtk_render_frame (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          x,
                  gdouble          y,
                  gdouble          width,
                  gdouble          height)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);
  cairo_new_path (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_frame (engine, cr, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_expander:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders an expander (as used in #GtkTreeView and #GtkExpander) in the area
 * defined by @x, @y, @width, @height. The state %GTK_STATE_FLAG_ACTIVE
 * determines whether the expander is collapsed or expanded.
 *
 * Typical expander rendering:
 *
 * ![](expanders.png)
 *
 * Since: 3.0
 **/
void
gtk_render_expander (GtkStyleContext *context,
                     cairo_t         *cr,
                     gdouble          x,
                     gdouble          y,
                     gdouble          width,
                     gdouble          height)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);
  cairo_new_path (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_expander (engine, cr, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_focus:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
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
 * Since: 3.0
 **/
void
gtk_render_focus (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          x,
                  gdouble          y,
                  gdouble          width,
                  gdouble          height)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);
  cairo_new_path (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_focus (engine, cr, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_layout:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin
 * @y: Y origin
 * @layout: the #PangoLayout to render
 *
 * Renders @layout on the coordinates @x, @y
 *
 * Since: 3.0
 **/
void
gtk_render_layout (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   PangoLayout     *layout)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (PANGO_IS_LAYOUT (layout));
  g_return_if_fail (cr != NULL);

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);
  cairo_new_path (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_layout (engine, cr, x, y, layout);

  cairo_restore (cr);
}

/**
 * gtk_render_line:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x0: X coordinate for the origin of the line
 * @y0: Y coordinate for the origin of the line
 * @x1: X coordinate for the end of the line
 * @y1: Y coordinate for the end of the line
 *
 * Renders a line from (x0, y0) to (x1, y1).
 *
 * Since: 3.0
 **/
void
gtk_render_line (GtkStyleContext *context,
                 cairo_t         *cr,
                 gdouble          x0,
                 gdouble          y0,
                 gdouble          x1,
                 gdouble          y1)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);
  cairo_new_path (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_line (engine, cr, x0, y0, x1, y1);

  cairo_restore (cr);
}

/**
 * gtk_render_slider:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 * @orientation: orientation of the slider
 *
 * Renders a slider (as in #GtkScale) in the rectangle defined by @x, @y,
 * @width, @height. @orientation defines whether the slider is vertical
 * or horizontal.
 *
 * Typical slider rendering:
 *
 * ![](sliders.png)
 *
 * Since: 3.0
 **/
void
gtk_render_slider (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   gdouble          width,
                   gdouble          height,
                   GtkOrientation   orientation)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);
  cairo_new_path (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_slider (engine, cr, x, y, width, height, orientation);

  cairo_restore (cr);
}

/**
 * gtk_render_frame_gap:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 * @gap_side: side where the gap is
 * @xy0_gap: initial coordinate (X or Y depending on @gap_side) for the gap
 * @xy1_gap: end coordinate (X or Y depending on @gap_side) for the gap
 *
 * Renders a frame around the rectangle defined by (@x, @y, @width, @height),
 * leaving a gap on one side. @xy0_gap and @xy1_gap will mean X coordinates
 * for %GTK_POS_TOP and %GTK_POS_BOTTOM gap sides, and Y coordinates for
 * %GTK_POS_LEFT and %GTK_POS_RIGHT.
 *
 * Typical rendering of a frame with a gap:
 *
 * ![](frame-gap.png)
 *
 * Since: 3.0
 **/
void
gtk_render_frame_gap (GtkStyleContext *context,
                      cairo_t         *cr,
                      gdouble          x,
                      gdouble          y,
                      gdouble          width,
                      gdouble          height,
                      GtkPositionType  gap_side,
                      gdouble          xy0_gap,
                      gdouble          xy1_gap)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (xy0_gap <= xy1_gap);
  g_return_if_fail (xy0_gap >= 0);

  if (width <= 0 || height <= 0)
    return;

  if (gap_side == GTK_POS_LEFT ||
      gap_side == GTK_POS_RIGHT)
    g_return_if_fail (xy1_gap <= height);
  else
    g_return_if_fail (xy1_gap <= width);

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);
  cairo_new_path (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_frame_gap (engine, cr,
                                  x, y, width, height, gap_side,
                                  xy0_gap, xy1_gap);

  cairo_restore (cr);
}

/**
 * gtk_render_extension:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 * @gap_side: side where the gap is
 *
 * Renders a extension (as in a #GtkNotebook tab) in the rectangle
 * defined by @x, @y, @width, @height. The side where the extension
 * connects to is defined by @gap_side.
 *
 * Typical extension rendering:
 *
 * ![](extensions.png)
 *
 * Since: 3.0
 **/
void
gtk_render_extension (GtkStyleContext *context,
                      cairo_t         *cr,
                      gdouble          x,
                      gdouble          y,
                      gdouble          width,
                      gdouble          height,
                      GtkPositionType  gap_side)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);
  cairo_new_path (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_extension (engine, cr, x, y, width, height, gap_side);

  cairo_restore (cr);
}

/**
 * gtk_render_handle:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a handle (as in #GtkHandleBox, #GtkPaned and
 * #GtkWindow’s resize grip), in the rectangle
 * determined by @x, @y, @width, @height.
 *
 * Handles rendered for the paned and grip classes:
 *
 * ![](handles.png)
 *
 * Since: 3.0
 **/
void
gtk_render_handle (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   gdouble          width,
                   gdouble          height)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);
  cairo_new_path (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_handle (engine, cr, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_activity:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders an activity indicator (such as in #GtkSpinner).
 * The state %GTK_STATE_FLAG_ACTIVE determines whether there is
 * activity going on.
 *
 * Since: 3.0
 **/
void
gtk_render_activity (GtkStyleContext *context,
                     cairo_t         *cr,
                     gdouble          x,
                     gdouble          y,
                     gdouble          width,
                     gdouble          height)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);
  cairo_new_path (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_activity (engine, cr, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_icon_pixbuf:
 * @context: a #GtkStyleContext
 * @source: the #GtkIconSource specifying the icon to render
 * @size: (type int): the size to render the icon at. A size of (GtkIconSize) -1
 *        means render at the size of the source and don’t scale.
 *
 * Renders the icon specified by @source at the given @size, returning the result
 * in a pixbuf.
 *
 * Returns: (transfer full): a newly-created #GdkPixbuf containing the rendered icon
 *
 * Since: 3.0
 *
 * Deprecated: 3.10: Use gtk_icon_theme_load_icon() instead.
 **/
GdkPixbuf *
gtk_render_icon_pixbuf (GtkStyleContext     *context,
                        const GtkIconSource *source,
                        GtkIconSize          size)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);
  g_return_val_if_fail (size > GTK_ICON_SIZE_INVALID || size == -1, NULL);
  g_return_val_if_fail (source != NULL, NULL);

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  _gtk_theming_engine_set_context (engine, context);
  return engine_class->render_icon_pixbuf (engine, source, size);
}

/**
 * gtk_render_icon:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @pixbuf: a #GdkPixbuf containing the icon to draw
 * @x: X position for the @pixbuf
 * @y: Y position for the @pixbuf
 *
 * Renders the icon in @pixbuf at the specified @x and @y coordinates.
 *
 * Since: 3.2
 **/
void
gtk_render_icon (GtkStyleContext *context,
                 cairo_t         *cr,
                 GdkPixbuf       *pixbuf,
                 gdouble          x,
                 gdouble          y)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);
  cairo_new_path (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_icon (engine, cr, pixbuf, x, y);

  cairo_restore (cr);
}

/**
 * gtk_render_icon_surface:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @surface: a #cairo_surface_t containing the icon to draw
 * @x: X position for the @icon
 * @y: Y position for the @incon
 *
 * Renders the icon in @surface at the specified @x and @y coordinates.
 *
 * Since: 3.10
 **/
void
gtk_render_icon_surface (GtkStyleContext *context,
			 cairo_t         *cr,
			 cairo_surface_t *surface,
			 gdouble          x,
			 gdouble          y)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);
  cairo_new_path (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_icon_surface (engine, cr, surface, x, y);

  cairo_restore (cr);
}

