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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gtkcsstypesprivate.h"
#include "gtkthemingbackgroundprivate.h"
#include "gtkthemingengineprivate.h"

#include <gdk/gdk.h>

static void
_gtk_theming_background_apply_window_background (GtkThemingBackground *bg,
                                                 cairo_t              *cr)
{
  if (gtk_theming_engine_has_class (bg->engine, "background"))
    {
      cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0); /* transparent */
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_paint (cr);
    }
}

static void
_gtk_theming_background_apply_running_transformation (GtkThemingBackground *bg,
                                                      cairo_t              *cr)
{
  gboolean running;
  gdouble progress;
  cairo_pattern_t *other_pattern;
  GtkStateFlags other_flags;
  GdkRGBA other_bg;
  cairo_pattern_t *new_pattern = NULL;

  running = gtk_theming_engine_state_is_running (bg->engine, GTK_STATE_PRELIGHT, &progress);

  if (!running)
    return;

  if (bg->flags & GTK_STATE_FLAG_PRELIGHT)
    {
      other_flags = bg->flags & ~(GTK_STATE_FLAG_PRELIGHT);
      progress = 1 - progress;
    }
  else
    other_flags = bg->flags | GTK_STATE_FLAG_PRELIGHT;

  gtk_theming_engine_get_background_color (bg->engine, other_flags, &other_bg);
  _gtk_theming_engine_get (bg->engine, other_flags, &bg->prop_context,
                           "background-image", &other_pattern,
                           NULL);

  if (bg->pattern && other_pattern)
    {
      cairo_pattern_type_t type, other_type;
      gint n0, n1;

      cairo_pattern_get_color_stop_count (bg->pattern, &n0);
      cairo_pattern_get_color_stop_count (other_pattern, &n1);
      type = cairo_pattern_get_type (bg->pattern);
      other_type = cairo_pattern_get_type (other_pattern);

      if (type == other_type && n0 == n1)
        {
          gdouble offset0, red0, green0, blue0, alpha0;
          gdouble offset1, red1, green1, blue1, alpha1;
          gdouble x00, x01, y00, y01, x10, x11, y10, y11;
          gdouble r00, r01, r10, r11;
          guint i;

          if (type == CAIRO_PATTERN_TYPE_LINEAR)
            {
              cairo_pattern_get_linear_points (bg->pattern, &x00, &y00, &x01, &y01);
              cairo_pattern_get_linear_points (other_pattern, &x10, &y10, &x11, &y11);

              new_pattern = cairo_pattern_create_linear (x00 + (x10 - x00) * progress,
                                                         y00 + (y10 - y00) * progress,
                                                         x01 + (x11 - x01) * progress,
                                                         y01 + (y11 - y01) * progress);
            }
          else
            {
              cairo_pattern_get_radial_circles (bg->pattern, &x00, &y00, &r00, &x01, &y01, &r01);
              cairo_pattern_get_radial_circles (other_pattern, &x10, &y10, &r10, &x11, &y11, &r11);

              new_pattern = cairo_pattern_create_radial (x00 + (x10 - x00) * progress,
                                                         y00 + (y10 - y00) * progress,
                                                         r00 + (r10 - r00) * progress,
                                                         x01 + (x11 - x01) * progress,
                                                         y01 + (y11 - y01) * progress,
                                                         r01 + (r11 - r01) * progress);
            }

          cairo_pattern_set_filter (new_pattern, CAIRO_FILTER_FAST);
          i = 0;

          /* Blend both gradients into one */
          while (i < n0 && i < n1)
            {
              cairo_pattern_get_color_stop_rgba (bg->pattern, i,
                                                 &offset0,
                                                 &red0, &green0, &blue0,
                                                 &alpha0);
              cairo_pattern_get_color_stop_rgba (other_pattern, i,
                                                 &offset1,
                                                 &red1, &green1, &blue1,
                                                 &alpha1);

              cairo_pattern_add_color_stop_rgba (new_pattern,
                                                 offset0 + ((offset1 - offset0) * progress),
                                                 red0 + ((red1 - red0) * progress),
                                                 green0 + ((green1 - green0) * progress),
                                                 blue0 + ((blue1 - blue0) * progress),
                                                 alpha0 + ((alpha1 - alpha0) * progress));
              i++;
            }
        }
      else
        {
          cairo_save (cr);

          cairo_rectangle (cr, 0, 0, bg->paint_area.width, bg->paint_area.height);
          cairo_clip (cr);

          cairo_push_group (cr);

          cairo_scale (cr, bg->paint_area.width, bg->paint_area.height);
          cairo_set_source (cr, other_pattern);
          cairo_paint_with_alpha (cr, progress);
          cairo_set_source (cr, bg->pattern);
          cairo_paint_with_alpha (cr, 1.0 - progress);

          new_pattern = cairo_pop_group (cr);

          cairo_restore (cr);
        }
    }
  else if (bg->pattern || other_pattern)
    {
      cairo_pattern_t *p;
      const GdkRGBA *c;
      gdouble x0, y0, x1, y1, r0, r1;
      gint n, i;

      /* Blend a pattern with a color */
      if (bg->pattern)
        {
          p = bg->pattern;
          c = &other_bg;
          progress = 1 - progress;
        }
      else
        {
          p = other_pattern;
          c = &bg->bg_color;
        }

      if (cairo_pattern_get_type (p) == CAIRO_PATTERN_TYPE_LINEAR)
        {
          cairo_pattern_get_linear_points (p, &x0, &y0, &x1, &y1);
          new_pattern = cairo_pattern_create_linear (x0, y0, x1, y1);
        }
      else
        {
          cairo_pattern_get_radial_circles (p, &x0, &y0, &r0, &x1, &y1, &r1);
          new_pattern = cairo_pattern_create_radial (x0, y0, r0, x1, y1, r1);
        }

      cairo_pattern_get_color_stop_count (p, &n);

      for (i = 0; i < n; i++)
        {
          gdouble red1, green1, blue1, alpha1;
          gdouble offset;

          cairo_pattern_get_color_stop_rgba (p, i,
                                             &offset,
                                             &red1, &green1, &blue1,
                                             &alpha1);
          cairo_pattern_add_color_stop_rgba (new_pattern, offset,
                                             c->red + ((red1 - c->red) * progress),
                                             c->green + ((green1 - c->green) * progress),
                                             c->blue + ((blue1 - c->blue) * progress),
                                             c->alpha + ((alpha1 - c->alpha) * progress));
        }
    }
  else
    {
      /* Merge just colors */
      new_pattern = cairo_pattern_create_rgba (CLAMP (bg->bg_color.red + ((other_bg.red - bg->bg_color.red) * progress), 0, 1),
                                               CLAMP (bg->bg_color.green + ((other_bg.green - bg->bg_color.green) * progress), 0, 1),
                                               CLAMP (bg->bg_color.blue + ((other_bg.blue - bg->bg_color.blue) * progress), 0, 1),
                                               CLAMP (bg->bg_color.alpha + ((other_bg.alpha - bg->bg_color.alpha) * progress), 0, 1));
    }

  if (new_pattern)
    {
      /* Replace pattern to use */
      cairo_pattern_destroy (bg->pattern);
      bg->pattern = new_pattern;
    }

  if (other_pattern)
    cairo_pattern_destroy (other_pattern);
}

static void
_gtk_theming_background_apply_origin (GtkThemingBackground *bg)
{
  GtkCssArea origin;
  cairo_rectangle_t image_rect;

  gtk_theming_engine_get (bg->engine, bg->flags,
			  "background-origin", &origin,
			  NULL);

  /* The default size of the background image depends on the
     background-origin value as this affects the top left
     and the bottom right corners. */
  switch (origin) {
  case GTK_CSS_AREA_BORDER_BOX:
    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.width = bg->paint_area.width;
    image_rect.height = bg->paint_area.height;
    break;
  case GTK_CSS_AREA_CONTENT_BOX:
    image_rect.x = bg->border.left + bg->padding.left;
    image_rect.y = bg->border.top + bg->padding.top;
    image_rect.width = bg->paint_area.width - bg->border.left - bg->border.right - bg->padding.left - bg->padding.right;
    image_rect.height = bg->paint_area.height - bg->border.top - bg->border.bottom - bg->padding.top - bg->padding.bottom;
    break;
  case GTK_CSS_AREA_PADDING_BOX:
  default:
    image_rect.x = bg->border.left;
    image_rect.y = bg->border.top;
    image_rect.width = bg->paint_area.width - bg->border.left - bg->border.right;
    image_rect.height = bg->paint_area.height - bg->border.top - bg->border.bottom;
    break;
  }

  bg->image_rect = image_rect;
  bg->prop_context.width = image_rect.width;
  bg->prop_context.height = image_rect.height;
}

static void
_gtk_theming_background_apply_clip (GtkThemingBackground *bg)
{
  GtkCssArea clip;

  gtk_theming_engine_get (bg->engine, bg->flags,
			  "background-clip", &clip,
			  NULL);

  if (clip == GTK_CSS_AREA_PADDING_BOX)
    {
      _gtk_rounded_box_shrink (&bg->clip_box,
			       bg->border.top, bg->border.right,
			       bg->border.bottom, bg->border.left);
    }
  else if (clip == GTK_CSS_AREA_CONTENT_BOX)
    {
      _gtk_rounded_box_shrink (&bg->clip_box,
			       bg->border.top + bg->padding.top,
			       bg->border.right + bg->padding.right,
			       bg->border.bottom + bg->padding.bottom,
			       bg->border.left + bg->padding.left);
    }
}

static void
_gtk_theming_background_paint (GtkThemingBackground *bg,
                               cairo_t              *cr)
{
  _gtk_rounded_box_path (&bg->clip_box, cr);
  gdk_cairo_set_source_rgba (cr, &bg->bg_color);

  if (bg->pattern)
    {
      GtkCssBackgroundRepeat *repeat;
      cairo_surface_t *surface;
      int scale_width, scale_height;

      gtk_theming_engine_get (bg->engine, bg->flags,
                              "background-repeat", &repeat,
                              NULL);

      if (cairo_pattern_get_surface (bg->pattern, &surface) != CAIRO_STATUS_SUCCESS)
        surface = NULL;

      if (surface && repeat &&
          repeat->repeat != GTK_CSS_BACKGROUND_REPEAT_STYLE_NONE)
        {
          scale_width = cairo_image_surface_get_width (surface);
          scale_height = cairo_image_surface_get_height (surface);
          if (repeat->repeat == GTK_CSS_BACKGROUND_REPEAT_STYLE_REPEAT)
            cairo_pattern_set_extend (bg->pattern, CAIRO_EXTEND_REPEAT);
          else if (repeat->repeat == GTK_CSS_BACKGROUND_REPEAT_STYLE_NO_REPEAT)
            cairo_pattern_set_extend (bg->pattern, CAIRO_EXTEND_NONE);
        }
      else
        {
          cairo_pattern_set_extend (bg->pattern, CAIRO_EXTEND_PAD);
          scale_width = bg->image_rect.width;
          scale_height = bg->image_rect.height;
        }

      if (scale_width && scale_height)
        {
          /* Fill background color first */
          cairo_fill_preserve (cr);

          cairo_translate (cr, bg->image_rect.x, bg->image_rect.y);
          cairo_scale (cr, scale_width, scale_height);
          cairo_set_source (cr, bg->pattern);
          cairo_scale (cr, 1.0 / scale_width, 1.0 / scale_height);
          cairo_translate (cr, -bg->image_rect.x, -bg->image_rect.y);

          g_free (repeat);

          cairo_pattern_destroy (bg->pattern);
          bg->pattern = NULL;
        }
    }

  cairo_fill (cr);
}

static void
_gtk_theming_background_apply_shadow (GtkThemingBackground *bg,
                                      cairo_t              *cr)
{
  GtkShadow *box_shadow;

  gtk_theming_engine_get (bg->engine, bg->flags,
			  "box-shadow", &box_shadow,
			  NULL);

  if (box_shadow != NULL)
    {
      _gtk_box_shadow_render (box_shadow, cr, &bg->padding_box);
      _gtk_shadow_unref (box_shadow);
    }
}

static void
_gtk_theming_background_init_engine (GtkThemingBackground *bg)
{
  bg->flags = gtk_theming_engine_get_state (bg->engine);

  gtk_theming_engine_get_border (bg->engine, bg->flags, &bg->border);
  gtk_theming_engine_get_padding (bg->engine, bg->flags, &bg->padding);
  gtk_theming_engine_get_background_color (bg->engine, bg->flags, &bg->bg_color);

  /* In the CSS box model, by default the background positioning area is
   * the padding-box, i.e. all the border-box minus the borders themselves,
   * which determines also its default size, see
   * http://dev.w3.org/csswg/css3-background/#background-origin
   *
   * In the future we might want to support different origins or clips, but
   * right now we just shrink to the default.
   */
  _gtk_rounded_box_init_rect (&bg->padding_box, 0, 0, bg->paint_area.width, bg->paint_area.height);
  _gtk_rounded_box_apply_border_radius (&bg->padding_box, bg->engine, bg->flags, bg->junction);

  bg->clip_box = bg->padding_box;
  _gtk_rounded_box_shrink (&bg->padding_box,
			   bg->border.top, bg->border.right,
			   bg->border.bottom, bg->border.left);

  _gtk_theming_background_apply_clip (bg);
  _gtk_theming_background_apply_origin (bg);

  _gtk_theming_engine_get (bg->engine, bg->flags, &bg->prop_context, 
                           "background-image", &bg->pattern,
                           NULL);
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
  g_assert (bg != NULL);

  bg->engine = engine;

  bg->paint_area.x = x;
  bg->paint_area.y = y;
  bg->paint_area.width = width;
  bg->paint_area.height = height;

  bg->pattern = NULL;
  bg->junction = junction;

  _gtk_theming_background_init_engine (bg);
}

void
_gtk_theming_background_render (GtkThemingBackground *bg,
                                cairo_t              *cr)
{
  cairo_save (cr);
  cairo_translate (cr, bg->paint_area.x, bg->paint_area.y);

  _gtk_theming_background_apply_window_background (bg, cr);
  _gtk_theming_background_apply_running_transformation (bg, cr);
  _gtk_theming_background_paint (bg, cr);
  _gtk_theming_background_apply_shadow (bg, cr);

  cairo_restore (cr);
}

gboolean
_gtk_theming_background_has_background_image (GtkThemingBackground *bg)
{
  return (bg->pattern != NULL) ? TRUE : FALSE;
}
