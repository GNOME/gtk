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
#include "gtkcssshadowsvalueprivate.h"
#include "gtkcsspositionvalueprivate.h"
#include "gtkcssrepeatvalueprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkthemingengineprivate.h"

#include <math.h>

#include <gdk/gdk.h>

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

static const GtkRoundedBox *
gtk_theming_background_get_box (GtkThemingBackground *bg,
                                GtkCssArea            area)
{
  switch (area)
    {
    case GTK_CSS_AREA_BORDER_BOX:
      return &bg->border_box;
    case GTK_CSS_AREA_PADDING_BOX:
      return &bg->padding_box;
    case GTK_CSS_AREA_CONTENT_BOX:
      return &bg->content_box;
    default:
      g_return_val_if_reached (&bg->border_box);
  }
}

static void
_gtk_theming_background_paint_color (GtkThemingBackground *bg,
                                     cairo_t              *cr,
                                     GtkCssValue          *background_image)
{
  gint n_values = _gtk_css_array_value_get_n_values (background_image);
  GtkCssArea clip = _gtk_css_area_value_get 
    (_gtk_css_array_value_get_nth 
     (_gtk_style_context_peek_property (bg->context, GTK_CSS_PROPERTY_BACKGROUND_CLIP), 
      n_values - 1));

  cairo_save (cr);
  _gtk_rounded_box_path (gtk_theming_background_get_box (bg, clip), cr);
  cairo_clip (cr);

  gdk_cairo_set_source_rgba (cr, &bg->bg_color);
  cairo_paint (cr);

  cairo_restore (cr);
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

  pos = _gtk_css_array_value_get_nth (_gtk_style_context_peek_property (bg->context, GTK_CSS_PROPERTY_BACKGROUND_POSITION), idx);
  repeat = _gtk_css_array_value_get_nth (_gtk_style_context_peek_property (bg->context, GTK_CSS_PROPERTY_BACKGROUND_REPEAT), idx);
  hrepeat = _gtk_css_background_repeat_value_get_x (repeat);
  vrepeat = _gtk_css_background_repeat_value_get_y (repeat);
  image = _gtk_css_image_value_get_image (
              _gtk_css_array_value_get_nth (
                  _gtk_style_context_peek_property (bg->context, GTK_CSS_PROPERTY_BACKGROUND_IMAGE),
                  idx));
  origin = gtk_theming_background_get_box (
               bg,
               _gtk_css_area_value_get (
                   _gtk_css_array_value_get_nth (
                       _gtk_style_context_peek_property (bg->context, GTK_CSS_PROPERTY_BACKGROUND_ORIGIN),
                       idx)));
  width = origin->box.width;
  height = origin->box.height;

  if (image == NULL || width <= 0 || height <= 0)
    return;

  _gtk_css_bg_size_value_compute_size (_gtk_css_array_value_get_nth (_gtk_style_context_peek_property (bg->context, GTK_CSS_PROPERTY_BACKGROUND_SIZE), idx),
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
      gtk_theming_background_get_box (
          bg,
          _gtk_css_area_value_get (
              _gtk_css_array_value_get_nth (
                  _gtk_style_context_peek_property (bg->context, GTK_CSS_PROPERTY_BACKGROUND_CLIP),
                  idx))),
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
_gtk_theming_background_apply_shadow (GtkThemingBackground *bg,
                                      cairo_t              *cr,
                                      gboolean              inset)
{
  _gtk_css_shadows_value_paint_box (_gtk_style_context_peek_property (bg->context, GTK_CSS_PROPERTY_BOX_SHADOW),
                                    cr,
                                    &bg->padding_box,
                                    inset);
}

static void
_gtk_theming_background_init_context (GtkThemingBackground *bg)
{
  GtkStateFlags flags = gtk_style_context_get_state (bg->context);
  GtkBorder border, padding;

  gtk_style_context_get_border (bg->context, flags, &border);
  gtk_style_context_get_padding (bg->context, flags, &padding);
  gtk_style_context_get_background_color (bg->context, flags, &bg->bg_color);

  /* In the CSS box model, by default the background positioning area is
   * the padding-box, i.e. all the border-box minus the borders themselves,
   * which determines also its default size, see
   * http://dev.w3.org/csswg/css3-background/#background-origin
   *
   * In the future we might want to support different origins or clips, but
   * right now we just shrink to the default.
   */
  _gtk_rounded_box_init_rect (&bg->border_box, 0, 0, bg->paint_area.width, bg->paint_area.height);
  _gtk_rounded_box_apply_border_radius_for_context (&bg->border_box, bg->context, bg->junction);

  bg->padding_box = bg->border_box;
  _gtk_rounded_box_shrink (&bg->padding_box,
			   border.top, border.right,
			   border.bottom, border.left);

  bg->content_box = bg->padding_box;
  _gtk_rounded_box_shrink (&bg->content_box,
			   padding.top, padding.right,
			   padding.bottom, padding.left);
}

void
_gtk_theming_background_init (GtkThemingBackground *bg,
                              GtkThemingEngine     *engine,
                              gdouble               x,
                              gdouble               y,
                              gdouble               width,
                              gdouble               height,
                              GtkJunctionSides      junction)
{
  GtkStyleContext *context;

  g_assert (bg != NULL);

  context = _gtk_theming_engine_get_context (engine);
  _gtk_theming_background_init_from_context (bg, context,
                                             x, y, width, height,
                                             junction);
}

void
_gtk_theming_background_init_from_context (GtkThemingBackground *bg,
                                           GtkStyleContext      *context,
                                           gdouble               x,
                                           gdouble               y,
                                           gdouble               width,
                                           gdouble               height,
                                           GtkJunctionSides      junction)
{
  g_assert (bg != NULL);

  bg->context = context;

  bg->paint_area.x = x;
  bg->paint_area.y = y;
  bg->paint_area.width = width;
  bg->paint_area.height = height;

  bg->junction = junction;

  _gtk_theming_background_init_context (bg);
}

void
_gtk_theming_background_render (GtkThemingBackground *bg,
                                cairo_t              *cr)
{
  gint idx;
  GtkCssValue *background_image;

  background_image = _gtk_style_context_peek_property (bg->context, GTK_CSS_PROPERTY_BACKGROUND_IMAGE);

  cairo_save (cr);
  cairo_translate (cr, bg->paint_area.x, bg->paint_area.y);

  _gtk_theming_background_apply_shadow (bg, cr, FALSE); /* Outset shadow */

  _gtk_theming_background_paint_color (bg, cr, background_image);

  for (idx = _gtk_css_array_value_get_n_values (background_image) - 1; idx >= 0; idx--)
    {
      _gtk_theming_background_paint_layer (bg, idx, cr);
    }

  _gtk_theming_background_apply_shadow (bg, cr, TRUE);  /* Inset shadow */

  cairo_restore (cr);
}

gboolean
_gtk_theming_background_has_background_image (GtkThemingBackground *bg)
{
  GtkCssImage *image;
  GtkCssValue *value = _gtk_style_context_peek_property (bg->context, GTK_CSS_PROPERTY_BACKGROUND_IMAGE);

  if (_gtk_css_array_value_get_n_values (value) == 0)
    return FALSE;

  image = _gtk_css_image_value_get_image (_gtk_css_array_value_get_nth (value, 0));
  return (image != NULL);
}
