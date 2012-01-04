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
  cairo_save (cr);

  _gtk_rounded_box_path (&bg->clip_box, cr);
  cairo_clip (cr);

  gdk_cairo_set_source_rgba (cr, &bg->bg_color);
  cairo_paint (cr);

  if (bg->image)
    {
      GtkCssBackgroundRepeat repeat;
      double image_width, image_height;

      gtk_theming_engine_get (bg->engine, bg->flags,
                              "background-repeat", &repeat,
                              NULL);

      _gtk_css_image_get_concrete_size (bg->image,
                                        0, 0, /* XXX: needs background-size support */
                                        bg->image_rect.width, bg->image_rect.height,
                                        &image_width, &image_height);

      cairo_translate (cr, bg->image_rect.x, bg->image_rect.y);
      /* XXX: repeat flags */
      _gtk_css_image_draw (bg->image, cr, image_width, image_height);
    }

  cairo_restore (cr);
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

  bg->image = g_value_get_object (_gtk_theming_engine_peek_property (bg->engine, "background-image"));
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

  bg->image = NULL;
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
  _gtk_theming_background_paint (bg, cr);
  _gtk_theming_background_apply_shadow (bg, cr);

  cairo_restore (cr);
}

gboolean
_gtk_theming_background_has_background_image (GtkThemingBackground *bg)
{
  return (bg->image != NULL) ? TRUE : FALSE;
}
