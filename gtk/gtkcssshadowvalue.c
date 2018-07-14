/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
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

#include "gtkcssshadowvalueprivate.h"

#include "gtkcsscolorvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtksnapshotprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkpango.h"

#include "gsk/gskcairoblurprivate.h"
#include "gsk/gskroundedrectprivate.h"

#include <math.h>

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  guint inset :1;

  GtkCssValue *hoffset;
  GtkCssValue *voffset;
  GtkCssValue *radius;
  GtkCssValue *spread;

  GtkCssValue *color;
};

static GtkCssValue *    gtk_css_shadow_value_new (GtkCssValue *hoffset,
                                                  GtkCssValue *voffset,
                                                  GtkCssValue *radius,
                                                  GtkCssValue *spread,
                                                  gboolean     inset,
                                                  GtkCssValue *color);

static void
gtk_css_value_shadow_free (GtkCssValue *shadow)
{
  _gtk_css_value_unref (shadow->hoffset);
  _gtk_css_value_unref (shadow->voffset);
  _gtk_css_value_unref (shadow->radius);
  _gtk_css_value_unref (shadow->spread);
  _gtk_css_value_unref (shadow->color);

  g_slice_free (GtkCssValue, shadow);
}

static GtkCssValue *
gtk_css_value_shadow_compute (GtkCssValue      *shadow,
                              guint             property_id,
                              GtkStyleProvider *provider,
                              GtkCssStyle      *style,
                              GtkCssStyle      *parent_style)
{
  GtkCssValue *hoffset, *voffset, *radius, *spread, *color;

  hoffset = _gtk_css_value_compute (shadow->hoffset, property_id, provider, style, parent_style);
  voffset = _gtk_css_value_compute (shadow->voffset, property_id, provider, style, parent_style);
  radius = _gtk_css_value_compute (shadow->radius, property_id, provider, style, parent_style);
  spread = _gtk_css_value_compute (shadow->spread, property_id, provider, style, parent_style),
  color = _gtk_css_value_compute (shadow->color, property_id, provider, style, parent_style);

  if (hoffset == shadow->hoffset &&
      voffset == shadow->voffset &&
      radius == shadow->radius &&
      spread == shadow->spread &&
      color == shadow->color)
    {
      _gtk_css_value_unref (hoffset);
      _gtk_css_value_unref (voffset);
      _gtk_css_value_unref (radius);
      _gtk_css_value_unref (spread);
      _gtk_css_value_unref (color);

      return _gtk_css_value_ref (shadow);
    }

  return gtk_css_shadow_value_new (hoffset, voffset, radius, spread, shadow->inset, color);
}

static gboolean
gtk_css_value_shadow_equal (const GtkCssValue *shadow1,
                            const GtkCssValue *shadow2)
{
  return shadow1->inset == shadow2->inset
      && _gtk_css_value_equal (shadow1->hoffset, shadow2->hoffset)
      && _gtk_css_value_equal (shadow1->voffset, shadow2->voffset)
      && _gtk_css_value_equal (shadow1->radius, shadow2->radius)
      && _gtk_css_value_equal (shadow1->spread, shadow2->spread)
      && _gtk_css_value_equal (shadow1->color, shadow2->color);
}

static GtkCssValue *
gtk_css_value_shadow_transition (GtkCssValue *start,
                                 GtkCssValue *end,
                                 guint        property_id,
                                 double       progress)
{
  if (start->inset != end->inset)
    return NULL;

  return gtk_css_shadow_value_new (_gtk_css_value_transition (start->hoffset, end->hoffset, property_id, progress),
                                   _gtk_css_value_transition (start->voffset, end->voffset, property_id, progress),
                                   _gtk_css_value_transition (start->radius, end->radius, property_id, progress),
                                   _gtk_css_value_transition (start->spread, end->spread, property_id, progress),
                                   start->inset,
                                   _gtk_css_value_transition (start->color, end->color, property_id, progress));
}

static void
gtk_css_value_shadow_print (const GtkCssValue *shadow,
                            GString           *string)
{
  _gtk_css_value_print (shadow->hoffset, string);
  g_string_append_c (string, ' ');
  _gtk_css_value_print (shadow->voffset, string);
  g_string_append_c (string, ' ');
  if (_gtk_css_number_value_get (shadow->radius, 100) != 0 ||
      _gtk_css_number_value_get (shadow->spread, 100) != 0)
    {
      _gtk_css_value_print (shadow->radius, string);
      g_string_append_c (string, ' ');
    }

  if (_gtk_css_number_value_get (shadow->spread, 100) != 0)
    {
      _gtk_css_value_print (shadow->spread, string);
      g_string_append_c (string, ' ');
    }

  _gtk_css_value_print (shadow->color, string);

  if (shadow->inset)
    g_string_append (string, " inset");

}

static const GtkCssValueClass GTK_CSS_VALUE_SHADOW = {
  gtk_css_value_shadow_free,
  gtk_css_value_shadow_compute,
  gtk_css_value_shadow_equal,
  gtk_css_value_shadow_transition,
  NULL,
  NULL,
  gtk_css_value_shadow_print
};

static GtkCssValue *
gtk_css_shadow_value_new (GtkCssValue *hoffset,
                          GtkCssValue *voffset,
                          GtkCssValue *radius,
                          GtkCssValue *spread,
                          gboolean     inset,
                          GtkCssValue *color)
{
  GtkCssValue *retval;

  retval = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_SHADOW);

  retval->hoffset = hoffset;
  retval->voffset = voffset;
  retval->radius = radius;
  retval->spread = spread;
  retval->inset = inset;
  retval->color = color;

  return retval;
}

GtkCssValue *
_gtk_css_shadow_value_new_for_transition (GtkCssValue *target)
{
  GdkRGBA transparent = { 0, 0, 0, 0 };

  g_return_val_if_fail (target->class == &GTK_CSS_VALUE_SHADOW, NULL);

  return gtk_css_shadow_value_new (_gtk_css_number_value_new (0, GTK_CSS_PX),
                                   _gtk_css_number_value_new (0, GTK_CSS_PX),
                                   _gtk_css_number_value_new (0, GTK_CSS_PX),
                                   _gtk_css_number_value_new (0, GTK_CSS_PX),
                                   target->inset,
                                   _gtk_css_rgba_value_new_from_rgba (&transparent));
}

static gboolean
value_is_done_parsing (GtkCssParser *parser)
{
  return _gtk_css_parser_is_eof (parser) ||
         _gtk_css_parser_begins_with (parser, ',') ||
         _gtk_css_parser_begins_with (parser, ';') ||
         _gtk_css_parser_begins_with (parser, '}');
}

GtkCssValue *
_gtk_css_shadow_value_parse (GtkCssParser *parser,
                             gboolean      box_shadow_mode)
{
  enum {
    HOFFSET,
    VOFFSET,
    RADIUS,
    SPREAD,
    COLOR,
    N_VALUES
  };
  GtkCssValue *values[N_VALUES] = { NULL, };
  gboolean inset;
  guint i;

  if (box_shadow_mode)
    inset = _gtk_css_parser_try (parser, "inset", TRUE);
  else
    inset = FALSE;

  do
  {
    if (values[HOFFSET] == NULL &&
        gtk_css_number_value_can_parse (parser))
      {
        values[HOFFSET] = _gtk_css_number_value_parse (parser,
                                                       GTK_CSS_PARSE_LENGTH);
        if (values[HOFFSET] == NULL)
          goto fail;

        values[VOFFSET] = _gtk_css_number_value_parse (parser,
                                                       GTK_CSS_PARSE_LENGTH);
        if (values[VOFFSET] == NULL)
          goto fail;

        if (gtk_css_number_value_can_parse (parser))
          {
            values[RADIUS] = _gtk_css_number_value_parse (parser,
                                                          GTK_CSS_PARSE_LENGTH
                                                          | GTK_CSS_POSITIVE_ONLY);
            if (values[RADIUS] == NULL)
              goto fail;
          }
        else
          values[RADIUS] = _gtk_css_number_value_new (0.0, GTK_CSS_PX);

        if (box_shadow_mode && gtk_css_number_value_can_parse (parser))
          {
            values[SPREAD] = _gtk_css_number_value_parse (parser,
                                                          GTK_CSS_PARSE_LENGTH);
            if (values[SPREAD] == NULL)
              goto fail;
          }
        else
          values[SPREAD] = _gtk_css_number_value_new (0.0, GTK_CSS_PX);
      }
    else if (!inset && box_shadow_mode && _gtk_css_parser_try (parser, "inset", TRUE))
      {
        if (values[HOFFSET] == NULL)
          goto fail;
        inset = TRUE;
        break;
      }
    else if (values[COLOR] == NULL)
      {
        values[COLOR] = _gtk_css_color_value_parse (parser);

        if (values[COLOR] == NULL)
          goto fail;
      }
    else
      {
        /* We parsed everything and there's still stuff left?
         * Pretend we didn't notice and let the normal code produce
         * a 'junk at end of value' error */
        goto fail;
      }
  }
  while (values[HOFFSET] == NULL || !value_is_done_parsing (parser));

  if (values[COLOR] == NULL)
    values[COLOR] = _gtk_css_color_value_new_current_color ();

  return gtk_css_shadow_value_new (values[HOFFSET], values[VOFFSET],
                                   values[RADIUS], values[SPREAD],
                                   inset, values[COLOR]);

fail:
  for (i = 0; i < N_VALUES; i++)
    {
      if (values[i])
        _gtk_css_value_unref (values[i]);
    }

  return NULL;
}

static gboolean
needs_blur (const GtkCssValue *shadow)
{
  double radius = _gtk_css_number_value_get (shadow->radius, 0);

  /* The code doesn't actually do any blurring for radius 1, as it
   * ends up with box filter size 1 */
  if (radius <= 1.0)
    return FALSE;

  return TRUE;
}

static const cairo_user_data_key_t original_cr_key;

static cairo_t *
gtk_css_shadow_value_start_drawing (const GtkCssValue *shadow,
                                    cairo_t           *cr,
                                    GskBlurFlags       blur_flags)
{
  cairo_rectangle_int_t clip_rect;
  cairo_surface_t *surface;
  cairo_t *blur_cr;
  gdouble radius, clip_radius;
  gdouble x_scale, y_scale;
  gboolean blur_x = (blur_flags & GSK_BLUR_X) != 0;
  gboolean blur_y = (blur_flags & GSK_BLUR_Y) != 0;

  if (!needs_blur (shadow))
    return cr;

  gdk_cairo_get_clip_rectangle (cr, &clip_rect);

  radius = _gtk_css_number_value_get (shadow->radius, 0);
  clip_radius = gsk_cairo_blur_compute_pixels (radius);

  x_scale = y_scale = 1;
  cairo_surface_get_device_scale (cairo_get_target (cr), &x_scale, &y_scale);

  if (blur_flags & GSK_BLUR_REPEAT)
    {
      if (!blur_x)
        clip_rect.width = 1;
      if (!blur_y)
        clip_rect.height = 1;
    }

  /* Create a larger surface to center the blur. */
  surface = cairo_surface_create_similar_image (cairo_get_target (cr),
                                                CAIRO_FORMAT_A8,
                                                x_scale * (clip_rect.width + (blur_x ? 2 * clip_radius : 0)),
                                                y_scale * (clip_rect.height + (blur_y ? 2 * clip_radius : 0)));
  cairo_surface_set_device_scale (surface, x_scale, y_scale);
  cairo_surface_set_device_offset (surface,
                                    x_scale * ((blur_x ? clip_radius: 0) - clip_rect.x),
                                    y_scale * ((blur_y ? clip_radius: 0) - clip_rect.y));

  blur_cr = cairo_create (surface);
  cairo_set_user_data (blur_cr, &original_cr_key, cairo_reference (cr), (cairo_destroy_func_t) cairo_destroy);

  if (cairo_has_current_point (cr))
    {
      double x, y;

      cairo_get_current_point (cr, &x, &y);
      cairo_move_to (blur_cr, x, y);
    }

  return blur_cr;
}

static void
mask_surface_repeat (cairo_t         *cr,
                     cairo_surface_t *surface)
{
  cairo_pattern_t *pattern;

  pattern = cairo_pattern_create_for_surface (surface);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);

  cairo_mask (cr, pattern);

  cairo_pattern_destroy (pattern);
}

static cairo_t *
gtk_css_shadow_value_finish_drawing (const GtkCssValue *shadow,
                                     cairo_t           *cr,
                                     GskBlurFlags       blur_flags)
{
  gdouble radius;
  cairo_t *original_cr;
  cairo_surface_t *surface;
  gdouble x_scale;

  if (!needs_blur (shadow))
    return cr;

  original_cr = cairo_get_user_data (cr, &original_cr_key);

  /* Blur the surface. */
  surface = cairo_get_target (cr);
  radius = _gtk_css_number_value_get (shadow->radius, 0);

  x_scale = 1;
  cairo_surface_get_device_scale (cairo_get_target (cr), &x_scale, NULL);

  gsk_cairo_blur_surface (surface, x_scale * radius, blur_flags);

  gdk_cairo_set_source_rgba (original_cr, _gtk_css_rgba_value_get_rgba (shadow->color));
  if (blur_flags & GSK_BLUR_REPEAT)
    mask_surface_repeat (original_cr, surface);
  else
    cairo_mask_surface (original_cr, surface, 0, 0);

  cairo_destroy (cr);

  cairo_surface_destroy (surface);

  return original_cr;
}

void
_gtk_css_shadow_value_paint_icon (const GtkCssValue *shadow,
			          cairo_t           *cr)
{
  cairo_pattern_t *pattern;

  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  /* We don't need to draw invisible shadows */
  if (gdk_rgba_is_clear (_gtk_css_rgba_value_get_rgba (shadow->color)))
    return;

  cairo_save (cr);
  pattern = cairo_pattern_reference (cairo_get_source (cr));

  gdk_cairo_set_source_rgba (cr, _gtk_css_rgba_value_get_rgba (shadow->color));
  cr = gtk_css_shadow_value_start_drawing (shadow, cr, GSK_BLUR_X | GSK_BLUR_Y);

  cairo_translate (cr,
                   _gtk_css_number_value_get (shadow->hoffset, 0),
                   _gtk_css_number_value_get (shadow->voffset, 0));
  cairo_mask (cr, pattern);

  cr = gtk_css_shadow_value_finish_drawing (shadow, cr, GSK_BLUR_X | GSK_BLUR_Y);

  cairo_restore (cr);
  cairo_pattern_destroy (pattern);
}

gboolean
_gtk_css_shadow_value_get_inset (const GtkCssValue *shadow)
{
  g_return_val_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW, FALSE);

  return shadow->inset;
}

void
gtk_css_shadow_value_get_extents (const GtkCssValue *shadow,
                                  GtkBorder         *border)
{
  gdouble hoffset, voffset, spread, radius, clip_radius;

  spread = _gtk_css_number_value_get (shadow->spread, 0);
  radius = _gtk_css_number_value_get (shadow->radius, 0);
  clip_radius = gsk_cairo_blur_compute_pixels (radius);
  hoffset = _gtk_css_number_value_get (shadow->hoffset, 0);
  voffset = _gtk_css_number_value_get (shadow->voffset, 0);

  border->top = MAX (0, ceil (clip_radius + spread - voffset));
  border->right = MAX (0, ceil (clip_radius + spread + hoffset));
  border->bottom = MAX (0, ceil (clip_radius + spread + voffset));
  border->left = MAX (0, ceil (clip_radius + spread - hoffset));
}

void
gtk_css_shadow_value_get_shadow (const GtkCssValue *value,
                                 GskShadow         *shadow)
{
  shadow->color = *_gtk_css_rgba_value_get_rgba (value->color);
  shadow->dx = _gtk_css_number_value_get (value->hoffset, 0);
  shadow->dy = _gtk_css_number_value_get (value->voffset, 0);
  shadow->radius = _gtk_css_number_value_get (value->radius, 0);
}

void
gtk_css_shadow_value_snapshot_outset (const GtkCssValue    *shadow,
                                      GtkSnapshot          *snapshot,
                                      const GskRoundedRect *border_box)
{
  GskRoundedRect outline;
  GskRenderNode *node;
  int off_x, off_y;

  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  /* We don't need to draw invisible shadows */
  if (gdk_rgba_is_clear (_gtk_css_rgba_value_get_rgba (shadow->color)))
    return;

  gtk_snapshot_get_offset (snapshot, &off_x, &off_y);
  gsk_rounded_rect_init_copy (&outline, border_box);
  gsk_rounded_rect_offset (&outline, off_x, off_y);

  node = gsk_outset_shadow_node_new (&outline, 
                                     _gtk_css_rgba_value_get_rgba (shadow->color),
                                     _gtk_css_number_value_get (shadow->hoffset, 0),
                                     _gtk_css_number_value_get (shadow->voffset, 0),
                                     _gtk_css_number_value_get (shadow->spread, 0),
                                     _gtk_css_number_value_get (shadow->radius, 0));
  gtk_snapshot_append_node_internal (snapshot, node);
  gsk_render_node_unref (node);
}

void
gtk_css_shadow_value_snapshot_inset (const GtkCssValue   *shadow,
                                     GtkSnapshot         *snapshot,
                                     const GskRoundedRect*padding_box)
{
  GskRoundedRect outline;
  GskRenderNode *node;
  int off_x, off_y;

  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  /* We don't need to draw invisible shadows */
  if (gdk_rgba_is_clear (_gtk_css_rgba_value_get_rgba (shadow->color)))
    return;

  gtk_snapshot_get_offset (snapshot, &off_x, &off_y);
  gsk_rounded_rect_init_copy (&outline, padding_box);
  gsk_rounded_rect_offset (&outline, off_x, off_y);

  node = gsk_inset_shadow_node_new (&outline, 
                                    _gtk_css_rgba_value_get_rgba (shadow->color),
                                    _gtk_css_number_value_get (shadow->hoffset, 0),
                                    _gtk_css_number_value_get (shadow->voffset, 0),
                                    _gtk_css_number_value_get (shadow->spread, 0),
                                    _gtk_css_number_value_get (shadow->radius, 0));
  gtk_snapshot_append_node_internal (snapshot, node);
  gsk_render_node_unref (node);
}

gboolean
gtk_css_shadow_value_is_clear (const GtkCssValue *shadow)
{
  return gdk_rgba_is_clear (_gtk_css_rgba_value_get_rgba (shadow->color));
}

