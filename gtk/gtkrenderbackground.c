/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
 * Copyright (C) 2011 Red Hat, Inc.
 * 
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *          Cosimo Cecchi <cosimoc@gnome.org>
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

#include "gtkrenderbackgroundprivate.h"

#include "gtkcssarrayvalueprivate.h"
#include "gtkcssbgsizevalueprivate.h"
#include "gtkcsscornervalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssimagevalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkcsspositionvalueprivate.h"
#include "gtkcssrepeatvalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcsstypesprivate.h"

#include <math.h>

#include <gdk/gdk.h>

#include "gsk/gskroundedrectprivate.h"

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

typedef struct _GtkThemingBackground GtkThemingBackground;

#define N_BOXES (3)

struct _GtkThemingBackground {
  GtkCssStyle *style;

  GskRoundedRect boxes[N_BOXES];
};

static void
_gtk_theming_background_paint_color (GtkThemingBackground *bg,
                                     cairo_t              *cr,
                                     const GdkRGBA        *bg_color,
                                     GtkCssValue          *background_image)
{
  gint n_values = _gtk_css_array_value_get_n_values (background_image);
  GtkCssArea clip = _gtk_css_area_value_get 
    (_gtk_css_array_value_get_nth 
     (gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BACKGROUND_CLIP), 
      n_values - 1));

  gsk_rounded_rect_path (&bg->boxes[clip], cr);
  gdk_cairo_set_source_rgba (cr, bg_color);
  cairo_fill (cr);
}

static void
gtk_theming_background_snapshot_color (GtkThemingBackground *bg,
                                       GtkSnapshot          *snapshot,
                                       const GdkRGBA        *bg_color,
                                       GtkCssValue          *background_image)
{
  gint n_values = _gtk_css_array_value_get_n_values (background_image);
  GtkCssArea clip = _gtk_css_area_value_get 
    (_gtk_css_array_value_get_nth 
     (gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BACKGROUND_CLIP), 
      n_values - 1));

  if (gdk_rgba_is_clear (bg_color))
    return;

  if (gsk_rounded_rect_is_rectilinear (&bg->boxes[clip]))
    {
      gtk_snapshot_append_color (snapshot,
                                 bg_color,
                                 &bg->boxes[clip].bounds,
                                 "BackgroundColor");
    }
  else
    {
      gtk_snapshot_push_rounded_clip (snapshot,
                                      &bg->boxes[clip],
                                      "BackgroundColorClip");
      gtk_snapshot_append_color (snapshot,
                                 bg_color,
                                 &bg->boxes[clip].bounds,
                                 "BackgroundColor");
      gtk_snapshot_pop (snapshot);
    }
}

static gboolean
_gtk_theming_background_needs_push_group (GtkCssStyle *style)
{
  GtkCssValue *blend_modes;
  gint i;

  blend_modes = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BACKGROUND_BLEND_MODE);

  /*
   * If we have any blend mode different than NORMAL, we'll need to
   * push a group in order to correctly apply the blend modes.
   */
  for (i = _gtk_css_array_value_get_n_values (blend_modes); i > 0; i--)
    {
      GskBlendMode blend_mode;

      blend_mode = _gtk_css_blend_mode_value_get (_gtk_css_array_value_get_nth (blend_modes, i - 1));

      if (blend_mode != GSK_BLEND_MODE_DEFAULT)
        return TRUE;
    }

  return FALSE;
}

static void
gtk_theming_background_paint_layer (GtkThemingBackground *bg,
                                    guint                 idx,
                                    cairo_t              *cr)
{
  GtkCssRepeatStyle hrepeat, vrepeat;
  const GtkCssValue *pos, *repeat;
  GtkCssImage *image;
  GskBlendMode blend_mode;
  const GskRoundedRect *origin;
  double image_width, image_height;
  double width, height;

  pos = _gtk_css_array_value_get_nth (gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BACKGROUND_POSITION), idx);
  repeat = _gtk_css_array_value_get_nth (gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BACKGROUND_REPEAT), idx);
  hrepeat = _gtk_css_background_repeat_value_get_x (repeat);
  vrepeat = _gtk_css_background_repeat_value_get_y (repeat);
  image = _gtk_css_image_value_get_image (
              _gtk_css_array_value_get_nth (
                  gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BACKGROUND_IMAGE),
                  idx));
  blend_mode = _gtk_css_blend_mode_value_get (
                   _gtk_css_array_value_get_nth (
                       gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BACKGROUND_BLEND_MODE), idx));

  origin = &bg->boxes[
               _gtk_css_area_value_get (
                   _gtk_css_array_value_get_nth (
                       gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BACKGROUND_ORIGIN),
                       idx))];
  width = origin->bounds.size.width;
  height = origin->bounds.size.height;

  if (image == NULL || width <= 0 || height <= 0)
    return;

  _gtk_css_bg_size_value_compute_size (_gtk_css_array_value_get_nth (gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BACKGROUND_SIZE), idx),
                                       image,
                                       width,
                                       height,
                                       &image_width,
                                       &image_height);

  if (image_width <= 0 || image_height <= 0)
    return;

  /* optimization */
  if (image_width == width)
    hrepeat = GTK_CSS_REPEAT_STYLE_NO_REPEAT;
  if (image_height == height)
    vrepeat = GTK_CSS_REPEAT_STYLE_NO_REPEAT;


  cairo_save (cr);

  gsk_rounded_rect_path (
      &bg->boxes[
          _gtk_css_area_value_get (
              _gtk_css_array_value_get_nth (
                  gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BACKGROUND_CLIP),
                  idx))],
      cr);
  cairo_clip (cr);


  cairo_translate (cr, origin->bounds.origin.x, origin->bounds.origin.y);

  /*
   * Apply the blend mode, if any.
   */
  if (G_UNLIKELY (_gtk_css_blend_mode_get_operator (blend_mode) != cairo_get_operator (cr)))
    cairo_set_operator (cr, _gtk_css_blend_mode_get_operator (blend_mode));


  if (hrepeat == GTK_CSS_REPEAT_STYLE_NO_REPEAT && vrepeat == GTK_CSS_REPEAT_STYLE_NO_REPEAT)
    {
      cairo_translate (cr,
                       _gtk_css_position_value_get_x (pos, width - image_width),
                       _gtk_css_position_value_get_y (pos, height - image_height));
      /* shortcut for normal case */
      _gtk_css_image_draw (image, cr, image_width, image_height);
    }
  else
    {
      int surface_width, surface_height;
      cairo_rectangle_t fill_rect;
      cairo_surface_t *surface;
      cairo_t *cr2;

      /* If ‘background-repeat’ is ‘round’ for one (or both) dimensions,
       * there is a second step. The UA must scale the image in that
       * dimension (or both dimensions) so that it fits a whole number of
       * times in the background positioning area. In the case of the width
       * (height is analogous):
       *
       * If X ≠ 0 is the width of the image after step one and W is the width
       * of the background positioning area, then the rounded width
       * X' = W / round(W / X) where round() is a function that returns the
       * nearest natural number (integer greater than zero). 
       *
       * If ‘background-repeat’ is ‘round’ for one dimension only and if
       * ‘background-size’ is ‘auto’ for the other dimension, then there is
       * a third step: that other dimension is scaled so that the original
       * aspect ratio is restored. 
       */
      if (hrepeat == GTK_CSS_REPEAT_STYLE_ROUND)
        {
          double n = round (width / image_width);

          n = MAX (1, n);

          if (vrepeat != GTK_CSS_REPEAT_STYLE_ROUND
              /* && vsize == auto (it is by default) */)
            image_height *= width / (image_width * n);
          image_width = width / n;
        }
      if (vrepeat == GTK_CSS_REPEAT_STYLE_ROUND)
        {
          double n = round (height / image_height);

          n = MAX (1, n);

          if (hrepeat != GTK_CSS_REPEAT_STYLE_ROUND
              /* && hsize == auto (it is by default) */)
            image_width *= height / (image_height * n);
          image_height = height / n;
        }

      /* if hrepeat or vrepeat is 'space', we create a somewhat larger surface
       * to store the extra space. */
      if (hrepeat == GTK_CSS_REPEAT_STYLE_SPACE)
        {
          double n = floor (width / image_width);
          surface_width = n ? round (width / n) : 0;
        }
      else
        surface_width = round (image_width);

      if (vrepeat == GTK_CSS_REPEAT_STYLE_SPACE)
        {
          double n = floor (height / image_height);
          surface_height = n ? round (height / n) : 0;
        }
      else
        surface_height = round (image_height);

      surface = cairo_surface_create_similar (cairo_get_target (cr),
                                              CAIRO_CONTENT_COLOR_ALPHA,
                                              surface_width, surface_height);
      cr2 = cairo_create (surface);
      cairo_translate (cr2,
                       0.5 * (surface_width - image_width),
                       0.5 * (surface_height - image_height));
      _gtk_css_image_draw (image, cr2, image_width, image_height);
      cairo_destroy (cr2);

      cairo_set_source_surface (cr, surface,
                                _gtk_css_position_value_get_x (pos, width - image_width),
                                _gtk_css_position_value_get_y (pos, height - image_height));
      cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_REPEAT);
      cairo_surface_destroy (surface);

      if (hrepeat == GTK_CSS_REPEAT_STYLE_NO_REPEAT)
        {
          fill_rect.x = _gtk_css_position_value_get_x (pos, width - image_width);
          fill_rect.width = image_width;
        }
      else
        {
          fill_rect.x = 0;
          fill_rect.width = width;
        }

      if (vrepeat == GTK_CSS_REPEAT_STYLE_NO_REPEAT)
        {
          fill_rect.y = _gtk_css_position_value_get_y (pos, height - image_height);
          fill_rect.height = image_height;
        }
      else
        {
          fill_rect.y = 0;
          fill_rect.height = height;
        }

      cairo_rectangle (cr, fill_rect.x, fill_rect.y,
                       fill_rect.width, fill_rect.height);
      cairo_fill (cr);
    }

  /*
   * Since this cairo_t can be shared with other widgets,
   * we must reset the operator after all the backgrounds
   * are properly rendered.
   */
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  cairo_restore (cr);
}

static void
gtk_theming_background_snapshot_layer (GtkThemingBackground *bg,
                                       guint                 idx,
                                       GtkSnapshot          *snapshot)
{
  GtkCssRepeatStyle hrepeat, vrepeat;
  const GtkCssValue *pos, *repeat;
  GtkCssImage *image;
  const GskRoundedRect *origin, *clip;
  double image_width, image_height;
  double width, height;

  pos = _gtk_css_array_value_get_nth (gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BACKGROUND_POSITION), idx);
  repeat = _gtk_css_array_value_get_nth (gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BACKGROUND_REPEAT), idx);
  hrepeat = _gtk_css_background_repeat_value_get_x (repeat);
  vrepeat = _gtk_css_background_repeat_value_get_y (repeat);
  image = _gtk_css_image_value_get_image (
              _gtk_css_array_value_get_nth (
                  gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BACKGROUND_IMAGE),
                  idx));

  origin = &bg->boxes[
               _gtk_css_area_value_get (
                   _gtk_css_array_value_get_nth (
                       gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BACKGROUND_ORIGIN),
                       idx))];
  clip = &bg->boxes[
              _gtk_css_area_value_get (
                                _gtk_css_array_value_get_nth (
                                                    gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BACKGROUND_CLIP),
                                                                      idx))];

  width = origin->bounds.size.width;
  height = origin->bounds.size.height;

  if (image == NULL || width <= 0 || height <= 0)
    return;

  _gtk_css_bg_size_value_compute_size (_gtk_css_array_value_get_nth (gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BACKGROUND_SIZE), idx),
                                       image,
                                       width,
                                       height,
                                       &image_width,
                                       &image_height);

  if (image_width <= 0 || image_height <= 0)
    return;

  /* optimization */
  if (image_width == width)
    hrepeat = GTK_CSS_REPEAT_STYLE_NO_REPEAT;
  if (image_height == height)
    vrepeat = GTK_CSS_REPEAT_STYLE_NO_REPEAT;

  gtk_snapshot_push_rounded_clip (snapshot, clip, "BackgroundLayerClip<%u>", idx);

  gtk_snapshot_offset (snapshot, origin->bounds.origin.x, origin->bounds.origin.y);

  if (hrepeat == GTK_CSS_REPEAT_STYLE_NO_REPEAT && vrepeat == GTK_CSS_REPEAT_STYLE_NO_REPEAT)
    {
      /* shortcut for normal case */
      double x, y;

      x = _gtk_css_position_value_get_x (pos, width - image_width);
      y = _gtk_css_position_value_get_y (pos, height - image_height);

      gtk_snapshot_offset (snapshot, x, y);

      gtk_css_image_snapshot (image, snapshot, image_width, image_height);

      gtk_snapshot_offset (snapshot, -x, -y);
    }
  else
    {
      float repeat_width, repeat_height;
      float position_x, position_y;
      graphene_rect_t fill_rect;

      /* If ‘background-repeat’ is ‘round’ for one (or both) dimensions,
       * there is a second step. The UA must scale the image in that
       * dimension (or both dimensions) so that it fits a whole number of
       * times in the background positioning area. In the case of the width
       * (height is analogous):
       *
       * If X ≠ 0 is the width of the image after step one and W is the width
       * of the background positioning area, then the rounded width
       * X' = W / round(W / X) where round() is a function that returns the
       * nearest natural number (integer greater than zero). 
       *
       * If ‘background-repeat’ is ‘round’ for one dimension only and if
       * ‘background-size’ is ‘auto’ for the other dimension, then there is
       * a third step: that other dimension is scaled so that the original
       * aspect ratio is restored. 
       */
      if (hrepeat == GTK_CSS_REPEAT_STYLE_ROUND)
        {
          double n = round (width / image_width);

          n = MAX (1, n);

          if (vrepeat != GTK_CSS_REPEAT_STYLE_ROUND
              /* && vsize == auto (it is by default) */)
            image_height *= width / (image_width * n);
          image_width = width / n;
        }
      if (vrepeat == GTK_CSS_REPEAT_STYLE_ROUND)
        {
          double n = round (height / image_height);

          n = MAX (1, n);

          if (hrepeat != GTK_CSS_REPEAT_STYLE_ROUND
              /* && hsize == auto (it is by default) */)
            image_width *= height / (image_height * n);
          image_height = height / n;
        }

      /* if hrepeat or vrepeat is 'space', we create a somewhat larger surface
       * to store the extra space. */
      if (hrepeat == GTK_CSS_REPEAT_STYLE_SPACE)
        {
          double n = floor (width / image_width);
          repeat_width = n ? round (width / n) : 0;
        }
      else
        repeat_width = round (image_width);

      if (vrepeat == GTK_CSS_REPEAT_STYLE_SPACE)
        {
          double n = floor (height / image_height);
          repeat_height = n ? round (height / n) : 0;
        }
      else
        repeat_height = round (image_height);

      if (hrepeat == GTK_CSS_REPEAT_STYLE_NO_REPEAT)
        {
          fill_rect.origin.x = _gtk_css_position_value_get_x (pos, width - image_width);
          fill_rect.size.width = image_width;
        }
      else
        {
          fill_rect.origin.x = 0;
          fill_rect.size.width = width;
        }

      if (vrepeat == GTK_CSS_REPEAT_STYLE_NO_REPEAT)
        {
          fill_rect.origin.y = _gtk_css_position_value_get_y (pos, height - image_height);
          fill_rect.size.height = image_height;
        }
      else
        {
          fill_rect.origin.y = 0;
          fill_rect.size.height = height;
        }

      position_x = _gtk_css_position_value_get_x (pos, width - image_width);
      position_y = _gtk_css_position_value_get_y (pos, height - image_height);

      gtk_snapshot_push_repeat (snapshot,
                                &fill_rect,
                                &GRAPHENE_RECT_INIT (
                                    position_x, position_y,
                                    repeat_width, repeat_height
                                ),
                                "BackgroundLayerRepeat<%u>", idx);
                                
      gtk_snapshot_offset (snapshot,
                           position_x + 0.5 * (repeat_width - image_width),
                           position_y + 0.5 * (repeat_height - image_height));
      gtk_css_image_snapshot (image, snapshot, image_width, image_height);

      gtk_snapshot_pop (snapshot);
    }

  gtk_snapshot_offset (snapshot, - origin->bounds.origin.x, - origin->bounds.origin.y);

  gtk_snapshot_pop (snapshot);
}

static void
gtk_theming_background_init (GtkThemingBackground *bg,
                             GtkCssStyle          *style,
                             double                width,
                             double                height)
{
  bg->style = style;

  gtk_rounded_boxes_init_for_style (&bg->boxes[GTK_CSS_AREA_BORDER_BOX],
                                    &bg->boxes[GTK_CSS_AREA_PADDING_BOX],
                                    &bg->boxes[GTK_CSS_AREA_CONTENT_BOX],
                                    style,
                                    0, 0, width, height);
}

void
gtk_css_style_render_background (GtkCssStyle      *style,
                                 cairo_t          *cr,
                                 gdouble           x,
                                 gdouble           y,
                                 gdouble           width,
                                 gdouble           height)
{
  GtkThemingBackground bg;
  gint idx;
  GtkCssValue *background_image;
  GtkCssValue *box_shadow;
  const GdkRGBA *bg_color;
  gboolean needs_push_group;
  gint number_of_layers;

  background_image = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BACKGROUND_IMAGE);
  bg_color = _gtk_css_rgba_value_get_rgba (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BACKGROUND_COLOR));
  box_shadow = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BOX_SHADOW);

  /* This is the common default case of no background */
  if (gdk_rgba_is_clear (bg_color) &&
      _gtk_css_array_value_get_n_values (background_image) == 1 &&
      _gtk_css_image_value_get_image (_gtk_css_array_value_get_nth (background_image, 0)) == NULL &&
      _gtk_css_shadows_value_is_none (box_shadow))
    return;

  gtk_theming_background_init (&bg, style, width, height);

  cairo_save (cr);
  cairo_translate (cr, x, y);

  /* Outset shadows */
  _gtk_css_shadows_value_paint_box (box_shadow,
                                    cr,
                                    &bg.boxes[GTK_CSS_AREA_BORDER_BOX],
                                    FALSE);

  /*
   * When we have a blend mode set for the background, we cannot blend the current
   * widget's drawing with whatever the content that the Cairo context may have.
   * Because of that, push the drawing to a new group before drawing the background
   * layers, and paint the resulting image back after.
   */
  needs_push_group = _gtk_theming_background_needs_push_group (style);

  if (needs_push_group)
    {
      cairo_save (cr);
      cairo_rectangle (cr, 0, 0, width, height);
      cairo_clip (cr);
      cairo_push_group (cr);
    }

  _gtk_theming_background_paint_color (&bg, cr, bg_color, background_image);

  number_of_layers = _gtk_css_array_value_get_n_values (background_image);

  for (idx = number_of_layers - 1; idx >= 0; idx--)
    {
      gtk_theming_background_paint_layer (&bg, idx, cr);
    }

  /* Paint back the resulting surface */
  if (needs_push_group)
    {
      cairo_pop_group_to_source (cr);
      cairo_paint (cr);
      cairo_restore (cr);
    }

  /* Inset shadows */
  _gtk_css_shadows_value_paint_box (box_shadow,
                                    cr,
                                    &bg.boxes[GTK_CSS_AREA_PADDING_BOX],
                                    TRUE);

  cairo_restore (cr);
}

void
gtk_css_style_snapshot_background (GtkCssStyle      *style,
                                   GtkSnapshot      *snapshot,
                                   gdouble           width,
                                   gdouble           height)
{
  GtkThemingBackground bg;
  gint idx;
  GtkCssValue *background_image;
  GtkCssValue *box_shadow;
  GtkCssValue *blend_modes;
  GskBlendMode blend_mode;
  const GdkRGBA *bg_color;
  gint number_of_layers;

  background_image = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BACKGROUND_IMAGE);
  bg_color = _gtk_css_rgba_value_get_rgba (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BACKGROUND_COLOR));
  box_shadow = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BOX_SHADOW);

  /* This is the common default case of no background */
  if (gdk_rgba_is_clear (bg_color) &&
      _gtk_css_array_value_get_n_values (background_image) == 1 &&
      _gtk_css_image_value_get_image (_gtk_css_array_value_get_nth (background_image, 0)) == NULL &&
      _gtk_css_shadows_value_is_none (box_shadow))
    return;

  gtk_theming_background_init (&bg, style, width, height);

  gtk_css_shadows_value_snapshot_outset (box_shadow,
                                         snapshot,
                                         &bg.boxes[GTK_CSS_AREA_BORDER_BOX]);

  blend_modes = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BACKGROUND_BLEND_MODE);
  number_of_layers = _gtk_css_array_value_get_n_values (background_image);

  for (idx = number_of_layers - 1; idx >= 0; idx--)
    {
      blend_mode = _gtk_css_blend_mode_value_get (_gtk_css_array_value_get_nth (blend_modes, idx));

      if (blend_mode != GSK_BLEND_MODE_DEFAULT)
        gtk_snapshot_push_blend (snapshot, blend_mode, "Background<%u>Blend<%u>", idx, blend_mode);
    }

  gtk_theming_background_snapshot_color (&bg, snapshot, bg_color, background_image);

  for (idx = number_of_layers - 1; idx >= 0; idx--)
    {
      blend_mode = _gtk_css_blend_mode_value_get (_gtk_css_array_value_get_nth (blend_modes, idx));

      if (blend_mode == GSK_BLEND_MODE_DEFAULT)
        {
          gtk_theming_background_snapshot_layer (&bg, idx, snapshot);
        }
      else
        {
          gtk_snapshot_pop (snapshot);
          gtk_theming_background_snapshot_layer (&bg, idx, snapshot);
          gtk_snapshot_pop (snapshot);
        }
    }

  gtk_css_shadows_value_snapshot_inset (box_shadow,
                                        snapshot,
                                        &bg.boxes[GTK_CSS_AREA_PADDING_BOX]);
}

