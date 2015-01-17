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

#include "gtkthemingbackgroundprivate.h"

#include "gtkcssarrayvalueprivate.h"
#include "gtkcssbgsizevalueprivate.h"
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

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

typedef struct _GtkThemingBackground GtkThemingBackground;

#define N_BOXES (3)

struct _GtkThemingBackground {
  GtkCssStyle *style;

  GtkRoundedBox boxes[N_BOXES];
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

  _gtk_rounded_box_path (&bg->boxes[clip], cr);
  gdk_cairo_set_source_rgba (cr, bg_color);
  cairo_fill (cr);
}

static void
_gtk_theming_background_paint_layer (GtkThemingBackground *bg,
                                     guint                 idx,
                                     cairo_t              *cr)
{
  GtkCssRepeatStyle hrepeat, vrepeat;
  const GtkCssValue *pos, *repeat;
  GtkCssImage *image;
  const GtkRoundedBox *origin;
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
  width = origin->box.width;
  height = origin->box.height;

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

  _gtk_rounded_box_path (
      &bg->boxes[
          _gtk_css_area_value_get (
              _gtk_css_array_value_get_nth (
                  gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BACKGROUND_CLIP),
                  idx))],
      cr);
  cairo_clip (cr);


  cairo_translate (cr, origin->box.x, origin->box.y);

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


  cairo_restore (cr);
}

static void
_gtk_theming_background_init_style (GtkThemingBackground *bg,
                                    double                width,
                                    double                height,
                                    GtkJunctionSides      junction)
{
  GtkBorder border, padding;

  border.top = _gtk_css_number_value_get (gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BORDER_TOP_WIDTH), 100);
  border.right = _gtk_css_number_value_get (gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH), 100);
  border.bottom = _gtk_css_number_value_get (gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH), 100);
  border.left = _gtk_css_number_value_get (gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH), 100);
  padding.top = _gtk_css_number_value_get (gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_PADDING_TOP), 100);
  padding.right = _gtk_css_number_value_get (gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_PADDING_RIGHT), 100);
  padding.bottom = _gtk_css_number_value_get (gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_PADDING_BOTTOM), 100);
  padding.left = _gtk_css_number_value_get (gtk_css_style_get_value (bg->style, GTK_CSS_PROPERTY_PADDING_LEFT), 100);

  /* In the CSS box model, by default the background positioning area is
   * the padding-box, i.e. all the border-box minus the borders themselves,
   * which determines also its default size, see
   * http://dev.w3.org/csswg/css3-background/#background-origin
   *
   * In the future we might want to support different origins or clips, but
   * right now we just shrink to the default.
   */
  _gtk_rounded_box_init_rect (&bg->boxes[GTK_CSS_AREA_BORDER_BOX], 0, 0, width, height);
  _gtk_rounded_box_apply_border_radius_for_style (&bg->boxes[GTK_CSS_AREA_BORDER_BOX], bg->style, junction);

  bg->boxes[GTK_CSS_AREA_PADDING_BOX] = bg->boxes[GTK_CSS_AREA_BORDER_BOX];
  _gtk_rounded_box_shrink (&bg->boxes[GTK_CSS_AREA_PADDING_BOX],
			   border.top, border.right,
			   border.bottom, border.left);

  bg->boxes[GTK_CSS_AREA_CONTENT_BOX] = bg->boxes[GTK_CSS_AREA_PADDING_BOX];
  _gtk_rounded_box_shrink (&bg->boxes[GTK_CSS_AREA_CONTENT_BOX],
			   padding.top, padding.right,
			   padding.bottom, padding.left);
}

void
gtk_css_style_render_background (GtkCssStyle      *style,
                                 cairo_t          *cr,
                                 gdouble           x,
                                 gdouble           y,
                                 gdouble           width,
                                 gdouble           height,
                                 GtkJunctionSides  junction)
{
  GtkThemingBackground bg;
  gint idx;
  GtkCssValue *background_image;
  GtkCssValue *box_shadow;
  const GdkRGBA *bg_color;

  background_image = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BACKGROUND_IMAGE);
  bg_color = _gtk_css_rgba_value_get_rgba (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BACKGROUND_COLOR));
  box_shadow = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BOX_SHADOW);

  /* This is the common default case of no background */
  if (gtk_rgba_is_clear (bg_color) &&
      _gtk_css_array_value_get_n_values (background_image) == 1 &&
      _gtk_css_image_value_get_image (_gtk_css_array_value_get_nth (background_image, 0)) == NULL &&
      _gtk_css_shadows_value_is_none (box_shadow))
    return;

  bg.style = style;
  _gtk_theming_background_init_style (&bg, width, height, junction);

  cairo_save (cr);
  cairo_translate (cr, x, y);

  /* Outset shadows */
  _gtk_css_shadows_value_paint_box (box_shadow,
                                    cr,
                                    &bg.boxes[GTK_CSS_AREA_BORDER_BOX],
                                    FALSE);

  _gtk_theming_background_paint_color (&bg, cr, bg_color, background_image);

  for (idx = _gtk_css_array_value_get_n_values (background_image) - 1; idx >= 0; idx--)
    {
      _gtk_theming_background_paint_layer (&bg, idx, cr);
    }

  /* Inset shadows */
  _gtk_css_shadows_value_paint_box (box_shadow,
                                    cr,
                                    &bg.boxes[GTK_CSS_AREA_PADDING_BOX],
                                    TRUE);

  cairo_restore (cr);
}
