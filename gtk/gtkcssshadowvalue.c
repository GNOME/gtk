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

#include "gtkcairoblurprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkrenderprivate.h"
#include "gtkpango.h"

#include "fallback-c89.c"
#include <float.h>

#define CORNER_MASK_CACHE_MAX_SIZE 2000U

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
gtk_css_value_shadow_compute (GtkCssValue             *shadow,
                              guint                    property_id,
                              GtkStyleProviderPrivate *provider,
                              GtkCssStyle             *style,
                              GtkCssStyle             *parent_style)
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
                                                       GTK_CSS_PARSE_LENGTH
                                                       | GTK_CSS_NUMBER_AS_PIXELS);
        if (values[HOFFSET] == NULL)
          goto fail;

        values[VOFFSET] = _gtk_css_number_value_parse (parser,
                                                       GTK_CSS_PARSE_LENGTH
                                                       | GTK_CSS_NUMBER_AS_PIXELS);
        if (values[VOFFSET] == NULL)
          goto fail;

        if (gtk_css_number_value_can_parse (parser))
          {
            values[RADIUS] = _gtk_css_number_value_parse (parser,
                                                          GTK_CSS_PARSE_LENGTH
                                                          | GTK_CSS_POSITIVE_ONLY
                                                          | GTK_CSS_NUMBER_AS_PIXELS);
            if (values[RADIUS] == NULL)
              goto fail;
          }
        else
          values[RADIUS] = _gtk_css_number_value_new (0.0, GTK_CSS_PX);

        if (box_shadow_mode && gtk_css_number_value_can_parse (parser))
          {
            values[SPREAD] = _gtk_css_number_value_parse (parser,
                                                          GTK_CSS_PARSE_LENGTH
                                                          | GTK_CSS_NUMBER_AS_PIXELS);
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
                                    GtkBlurFlags       blur_flags)
{
  cairo_rectangle_int_t clip_rect;
  cairo_surface_t *surface;
  cairo_t *blur_cr;
  gdouble radius, clip_radius;
  gdouble x_scale, y_scale;
  gboolean blur_x = (blur_flags & GTK_BLUR_X) != 0;
  gboolean blur_y = (blur_flags & GTK_BLUR_Y) != 0;

  if (!needs_blur (shadow))
    return cr;

  gdk_cairo_get_clip_rectangle (cr, &clip_rect);

  radius = _gtk_css_number_value_get (shadow->radius, 0);
  clip_radius = _gtk_cairo_blur_compute_pixels (radius);

  x_scale = y_scale = 1;
  cairo_surface_get_device_scale (cairo_get_target (cr), &x_scale, &y_scale);

  if (blur_flags & GTK_BLUR_REPEAT)
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

void
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
                                     GtkBlurFlags       blur_flags)
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

  _gtk_cairo_blur_surface (surface, x_scale * radius, blur_flags);

  gdk_cairo_set_source_rgba (original_cr, _gtk_css_rgba_value_get_rgba (shadow->color));
  if (blur_flags & GTK_BLUR_REPEAT)
    mask_surface_repeat (original_cr, surface);
  else
    cairo_mask_surface (original_cr, surface, 0, 0);

  cairo_destroy (cr);

  cairo_surface_destroy (surface);

  return original_cr;
}

static const cairo_user_data_key_t radius_key;
static const cairo_user_data_key_t layout_serial_key;

G_DEFINE_QUARK (GtkCssShadowValue pango_cached_blurred_surface, pango_cached_blurred_surface)

static cairo_surface_t *
get_cached_pango_surface (PangoLayout       *layout,
                          const GtkCssValue *shadow)
{
  cairo_surface_t *cached_surface = g_object_get_qdata (G_OBJECT (layout), pango_cached_blurred_surface_quark ());
  guint cached_radius, cached_serial;
  guint radius, serial;

  if (!cached_surface)
    return NULL;

  radius = _gtk_css_number_value_get (shadow->radius, 0);
  cached_radius = GPOINTER_TO_UINT (cairo_surface_get_user_data (cached_surface, &radius_key));
  if (radius != cached_radius)
    return NULL;

  serial = pango_layout_get_serial (layout);
  cached_serial = GPOINTER_TO_UINT (cairo_surface_get_user_data (cached_surface, &layout_serial_key));
  if (serial != cached_serial)
    return NULL;

  return cached_surface;
}

static cairo_surface_t *
make_blurred_pango_surface (cairo_t           *existing_cr,
                            PangoLayout       *layout,
                            const GtkCssValue *shadow)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  gdouble radius, clip_radius;
  gdouble x_scale, y_scale;
  PangoRectangle ink_rect;

  radius = _gtk_css_number_value_get (shadow->radius, 0);

  pango_layout_get_pixel_extents (layout, &ink_rect, NULL);
  clip_radius = _gtk_cairo_blur_compute_pixels (radius);
  x_scale = y_scale = 1;
  cairo_surface_get_device_scale (cairo_get_target (existing_cr), &x_scale, &y_scale);

  surface = cairo_surface_create_similar_image (cairo_get_target (existing_cr),
                                                CAIRO_FORMAT_A8,
                                                x_scale * (ink_rect.width + 2 * clip_radius),
                                                y_scale * (ink_rect.height + 2 * clip_radius));
  cairo_surface_set_device_scale (surface, x_scale, y_scale);
  cairo_surface_set_device_offset (surface,
                                   x_scale * (-ink_rect.x + clip_radius),
                                   y_scale * (-ink_rect.y + clip_radius));
  cr = cairo_create (surface);
  cairo_move_to (cr, 0, 0);
  _gtk_pango_fill_layout (cr, layout);
  _gtk_cairo_blur_surface (surface, radius * x_scale, GTK_BLUR_X | GTK_BLUR_Y);

  cairo_destroy (cr);

  return surface;
}

static cairo_surface_t *
get_blurred_pango_surface (cairo_t           *cr,
                           PangoLayout       *layout,
                           const GtkCssValue *shadow)
{
  cairo_surface_t *surface;
  guint radius, serial;

  surface = get_cached_pango_surface (layout, shadow);
  if (!surface)
    {
      surface = make_blurred_pango_surface (cr, layout, shadow);

      /* Cache the surface on the PangoLayout */
      radius = _gtk_css_number_value_get (shadow->radius, 0);
      cairo_surface_set_user_data (surface, &radius_key, GUINT_TO_POINTER (radius), NULL);

      serial = pango_layout_get_serial (layout);
      cairo_surface_set_user_data (surface, &layout_serial_key, GUINT_TO_POINTER (serial), NULL);

      g_object_set_qdata_full (G_OBJECT (layout), pango_cached_blurred_surface_quark (),
                               surface, (GDestroyNotify) cairo_surface_destroy);
    }

  return surface;
}

void
_gtk_css_shadow_value_paint_layout (const GtkCssValue *shadow,
                                    cairo_t           *cr,
                                    PangoLayout       *layout)
{
  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  /* We don't need to draw invisible shadows */
  if (gtk_rgba_is_clear (_gtk_css_rgba_value_get_rgba (shadow->color)))
    return;

  if (!cairo_has_current_point (cr))
    cairo_move_to (cr, 0, 0);

  cairo_save (cr);

  if (needs_blur (shadow))
    {
      cairo_surface_t *blurred_surface = get_blurred_pango_surface (cr, layout, shadow);
      double x, y;
      cairo_get_current_point (cr, &x, &y);
      cairo_translate (cr, x, y);
      cairo_translate (cr,
                       _gtk_css_number_value_get (shadow->hoffset, 0),
                       _gtk_css_number_value_get (shadow->voffset, 0));

      gdk_cairo_set_source_rgba (cr, _gtk_css_rgba_value_get_rgba (shadow->color));
      cairo_mask_surface (cr, blurred_surface, 0, 0);
    }
  else
    {
      /* The no blur case -- just paint directly. */
      cairo_rel_move_to (cr,
                         _gtk_css_number_value_get (shadow->hoffset, 0),
                         _gtk_css_number_value_get (shadow->voffset, 0));
      gdk_cairo_set_source_rgba (cr, _gtk_css_rgba_value_get_rgba (shadow->color));
      _gtk_pango_fill_layout (cr, layout);
      cairo_rel_move_to (cr,
                         - _gtk_css_number_value_get (shadow->hoffset, 0),
                         - _gtk_css_number_value_get (shadow->voffset, 0));
    }

  cairo_restore (cr);
}

void
_gtk_css_shadow_value_paint_icon (const GtkCssValue *shadow,
			          cairo_t           *cr)
{
  cairo_pattern_t *pattern;

  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  /* We don't need to draw invisible shadows */
  if (gtk_rgba_is_clear (_gtk_css_rgba_value_get_rgba (shadow->color)))
    return;

  cairo_save (cr);
  pattern = cairo_pattern_reference (cairo_get_source (cr));

  gdk_cairo_set_source_rgba (cr, _gtk_css_rgba_value_get_rgba (shadow->color));
  cr = gtk_css_shadow_value_start_drawing (shadow, cr, GTK_BLUR_X | GTK_BLUR_Y);

  cairo_translate (cr,
                   _gtk_css_number_value_get (shadow->hoffset, 0),
                   _gtk_css_number_value_get (shadow->voffset, 0));
  cairo_mask (cr, pattern);

  cr = gtk_css_shadow_value_finish_drawing (shadow, cr, GTK_BLUR_X | GTK_BLUR_Y);

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
_gtk_css_shadow_value_get_geometry (const GtkCssValue *shadow,
                                    gdouble           *hoffset,
                                    gdouble           *voffset,
                                    gdouble           *radius,
                                    gdouble           *spread)
{
  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  if (hoffset != NULL)
    *hoffset = _gtk_css_number_value_get (shadow->hoffset, 0);
  if (voffset != NULL)
    *voffset = _gtk_css_number_value_get (shadow->voffset, 0);

  if (radius != NULL)
    *radius = _gtk_css_number_value_get (shadow->radius, 0);
  if (spread != NULL)
    *spread = _gtk_css_number_value_get (shadow->spread, 0);
}

static gboolean
has_empty_clip (cairo_t *cr)
{
  double x1, y1, x2, y2;

  cairo_clip_extents (cr, &x1, &y1, &x2, &y2);
  return x1 == x2 && y1 == y2;
}

static void
draw_shadow (const GtkCssValue   *shadow,
	     cairo_t             *cr,
	     GtkRoundedBox       *box,
	     GtkRoundedBox       *clip_box,
	     GtkBlurFlags         blur_flags)
{
  cairo_t *shadow_cr;
  gboolean do_blur;

  if (has_empty_clip (cr))
    return;

  gdk_cairo_set_source_rgba (cr, _gtk_css_rgba_value_get_rgba (shadow->color));
  do_blur = (blur_flags & (GTK_BLUR_X | GTK_BLUR_Y)) != 0;
  if (do_blur)
    shadow_cr = gtk_css_shadow_value_start_drawing (shadow, cr, blur_flags);
  else
    shadow_cr = cr;

  cairo_set_fill_rule (shadow_cr, CAIRO_FILL_RULE_EVEN_ODD);
  _gtk_rounded_box_path (box, shadow_cr);
  if (shadow->inset)
    _gtk_rounded_box_clip_path (clip_box, shadow_cr);

  cairo_fill (shadow_cr);

  if (do_blur)
    gtk_css_shadow_value_finish_drawing (shadow, shadow_cr, blur_flags);
}

typedef struct {
  gint radius;
  /* rounded box corner */
  gint corner_horizontal;
  gint corner_vertical;
} CornerMask;

static guint
corner_mask_hash (CornerMask *mask)
{
  return ((guint)mask->radius) << 24 ^
    ((guint)mask->corner_horizontal) << 12 ^
    ((guint)mask->corner_vertical) << 0;
}

static gboolean
corner_mask_equal (CornerMask *mask1,
                   CornerMask *mask2)
{
  return
    mask1->radius == mask2->radius &&
    mask1->corner_horizontal == mask2->corner_horizontal &&
    mask1->corner_vertical == mask2->corner_vertical;
}

static gint
truncate_to_int (double val)
{
  if (isnan (val))
    return 0;
  if (val >= G_MAXINT)
    return G_MAXINT;
  if (val <= G_MININT)
    return G_MININT;
  return (gint) val;
}

static inline gint
round_to_int (double val)
{
  return truncate_to_int (val + (val > 0 ? 0.5 : -0.5));
}

static inline gint
quantize_to_int (double val)
{
  const double precision_factor = 10.0;
  return round_to_int (val * precision_factor);
}

static void
draw_shadow_corner (const GtkCssValue   *shadow,
                    cairo_t             *cr,
                    GtkRoundedBox       *box,
                    GtkRoundedBox       *clip_box,
                    GtkCssCorner         corner,
                    cairo_rectangle_int_t *drawn_rect)
{
  gdouble radius, clip_radius;
  int x1, x2, x3, y1, y2, y3, x, y;
  GtkRoundedBox corner_box;
  cairo_t *mask_cr;
  cairo_surface_t *mask;
  cairo_pattern_t *pattern;
  cairo_matrix_t matrix;
  double sx, sy;
  static GHashTable *corner_mask_cache = NULL;
  double max_other;
  CornerMask key;
  gboolean overlapped;

  radius = _gtk_css_number_value_get (shadow->radius, 0);
  clip_radius = _gtk_cairo_blur_compute_pixels (radius);

  overlapped = FALSE;
  if (corner == GTK_CSS_TOP_LEFT || corner == GTK_CSS_BOTTOM_LEFT)
    {
      x1 = floor (box->box.x - clip_radius);
      x2 = ceil (box->box.x + box->corner[corner].horizontal + clip_radius);
      x = x1;
      sx = 1;
      max_other = MAX(box->corner[GTK_CSS_TOP_RIGHT].horizontal, box->corner[GTK_CSS_BOTTOM_RIGHT].horizontal);
      x3 = floor (box->box.x + box->box.width - max_other - clip_radius);
      if (x2 > x3)
        overlapped = TRUE;
    }
  else
    {
      x1 = floor (box->box.x + box->box.width - box->corner[corner].horizontal - clip_radius);
      x2 = ceil (box->box.x + box->box.width + clip_radius);
      x = x2;
      sx = -1;
      max_other = MAX(box->corner[GTK_CSS_TOP_LEFT].horizontal, box->corner[GTK_CSS_BOTTOM_LEFT].horizontal);
      x3 = ceil (box->box.x + max_other + clip_radius);
      if (x3 > x1)
        overlapped = TRUE;
    }

  if (corner == GTK_CSS_TOP_LEFT || corner == GTK_CSS_TOP_RIGHT)
    {
      y1 = floor (box->box.y - clip_radius);
      y2 = ceil (box->box.y + box->corner[corner].vertical + clip_radius);
      y = y1;
      sy = 1;
      max_other = MAX(box->corner[GTK_CSS_BOTTOM_LEFT].vertical, box->corner[GTK_CSS_BOTTOM_RIGHT].vertical);
      y3 = floor (box->box.y + box->box.height - max_other - clip_radius);
      if (y2 > y3)
        overlapped = TRUE;
    }
  else
    {
      y1 = floor (box->box.y + box->box.height - box->corner[corner].vertical - clip_radius);
      y2 = ceil (box->box.y + box->box.height + clip_radius);
      y = y2;
      sy = -1;
      max_other = MAX(box->corner[GTK_CSS_TOP_LEFT].vertical, box->corner[GTK_CSS_TOP_RIGHT].vertical);
      y3 = ceil (box->box.y + max_other + clip_radius);
      if (y3 > y1)
        overlapped = TRUE;
    }

  drawn_rect->x = x1;
  drawn_rect->y = y1;
  drawn_rect->width = x2 - x1;
  drawn_rect->height = y2 - y1;

  cairo_rectangle (cr, x1, y1, x2 - x1, y2 - y1);
  cairo_clip (cr);

  if (shadow->inset || overlapped)
    {
      /* Fall back to generic path if inset or if the corner radius
         runs into each other */
      draw_shadow (shadow, cr, box, clip_box, GTK_BLUR_X | GTK_BLUR_Y);
      return;
    }

  if (has_empty_clip (cr))
    return;

  /* At this point we're drawing a blurred outset corner. The only
   * things that affect the output of the blurred mask in this case
   * is:
   *
   * What corner this is, which defines the orientation (sx,sy)
   * and position (x,y)
   *
   * The blur radius (which also defines the clip_radius)
   *
   * The horizontal and vertical corner radius
   *
   * We apply the first position and orientation when drawing the
   * mask, so we cache rendered masks based on the blur radius and the
   * corner radius.
   */
  if (corner_mask_cache == NULL)
    corner_mask_cache = g_hash_table_new_full ((GHashFunc)corner_mask_hash,
                                               (GEqualFunc)corner_mask_equal,
                                               g_free, (GDestroyNotify)cairo_surface_destroy);

  key.radius = quantize_to_int (radius);
  key.corner_horizontal = quantize_to_int (box->corner[corner].horizontal);
  key.corner_vertical = quantize_to_int (box->corner[corner].vertical);

  mask = g_hash_table_lookup (corner_mask_cache, &key);
  if (mask == NULL)
    {
      mask = cairo_surface_create_similar_image (cairo_get_target (cr), CAIRO_FORMAT_A8,
                                                 drawn_rect->width + clip_radius,
                                                 drawn_rect->height + clip_radius);
      mask_cr = cairo_create (mask);
      _gtk_rounded_box_init_rect (&corner_box, clip_radius, clip_radius, 2*drawn_rect->width, 2*drawn_rect->height);
      corner_box.corner[0] = box->corner[corner];
      _gtk_rounded_box_path (&corner_box, mask_cr);
      cairo_fill (mask_cr);
      _gtk_cairo_blur_surface (mask, radius, GTK_BLUR_X | GTK_BLUR_Y);
      cairo_destroy (mask_cr);

      if (g_hash_table_size (corner_mask_cache) >= CORNER_MASK_CACHE_MAX_SIZE)
        {
          GHashTableIter iter;
          guint i = 0;

          g_hash_table_iter_init (&iter, corner_mask_cache);
          while (g_hash_table_iter_next (&iter, NULL, NULL))
            if (i++ % 4 == 0)
              g_hash_table_iter_remove (&iter);
        }
      g_hash_table_insert (corner_mask_cache, g_memdup (&key, sizeof (key)), mask);
    }

  gdk_cairo_set_source_rgba (cr, _gtk_css_rgba_value_get_rgba (shadow->color));
  pattern = cairo_pattern_create_for_surface (mask);
  cairo_matrix_init_identity (&matrix);
  cairo_matrix_scale (&matrix, sx, sy);
  cairo_matrix_translate (&matrix, -x, -y);
  cairo_pattern_set_matrix (pattern, &matrix);
  cairo_mask (cr, pattern);
  cairo_pattern_destroy (pattern);
}

static void
draw_shadow_side (const GtkCssValue   *shadow,
                  cairo_t             *cr,
                  GtkRoundedBox       *box,
                  GtkRoundedBox       *clip_box,
                  GtkCssSide           side,
                  cairo_rectangle_int_t *drawn_rect)
{
  GtkBlurFlags blur_flags = GTK_BLUR_REPEAT;
  gdouble radius, clip_radius;
  int x1, x2, y1, y2;

  radius = _gtk_css_number_value_get (shadow->radius, 0);
  clip_radius = _gtk_cairo_blur_compute_pixels (radius);

  if (side == GTK_CSS_TOP || side == GTK_CSS_BOTTOM)
    {
      blur_flags |= GTK_BLUR_Y;
      x1 = floor (box->box.x - clip_radius);
      x2 = ceil (box->box.x + box->box.width + clip_radius);
    }
  else if (side == GTK_CSS_LEFT)
    {
      x1 = floor (box->box.x -clip_radius);
      x2 = ceil (box->box.x + clip_radius);
    }
  else
    {
      x1 = floor (box->box.x + box->box.width -clip_radius);
      x2 = ceil (box->box.x + box->box.width + clip_radius);
    }

  if (side == GTK_CSS_LEFT || side == GTK_CSS_RIGHT)
    {
      blur_flags |= GTK_BLUR_X;
      y1 = floor (box->box.y - clip_radius);
      y2 = ceil (box->box.y + box->box.height + clip_radius);
    }
  else if (side == GTK_CSS_TOP)
    {
      y1 = floor (box->box.y -clip_radius);
      y2 = ceil (box->box.y + clip_radius);
    }
  else
    {
      y1 = floor (box->box.y + box->box.height -clip_radius);
      y2 = ceil (box->box.y + box->box.height + clip_radius);
    }

  drawn_rect->x = x1;
  drawn_rect->y = y1;
  drawn_rect->width = x2 - x1;
  drawn_rect->height = y2 - y1;

  cairo_rectangle (cr, x1, y1, x2 - x1, y2 - y1);
  cairo_clip (cr);
  draw_shadow (shadow, cr, box, clip_box, blur_flags);
}

void
_gtk_css_shadow_value_paint_box (const GtkCssValue   *shadow,
                                 cairo_t             *cr,
                                 const GtkRoundedBox *padding_box)
{
  GtkRoundedBox box, clip_box;
  double spread, radius, clip_radius, x, y, outside;
  double x1c, y1c, x2c, y2c;

  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  /* We don't need to draw invisible shadows */
  if (gtk_rgba_is_clear (_gtk_css_rgba_value_get_rgba (shadow->color)))
    return;

  cairo_clip_extents (cr, &x1c, &y1c, &x2c, &y2c);
  if ((shadow->inset && !_gtk_rounded_box_intersects_rectangle (padding_box, x1c, y1c, x2c, y2c)) ||
      (!shadow->inset && _gtk_rounded_box_contains_rectangle (padding_box, x1c, y1c, x2c, y2c)))
    return;

  cairo_save (cr);

  spread = _gtk_css_number_value_get (shadow->spread, 0);
  radius = _gtk_css_number_value_get (shadow->radius, 0);
  clip_radius = _gtk_cairo_blur_compute_pixels (radius);
  x = _gtk_css_number_value_get (shadow->hoffset, 0);
  y = _gtk_css_number_value_get (shadow->voffset, 0);

  if (shadow->inset)
    {
      _gtk_rounded_box_path (padding_box, cr);
      cairo_clip (cr);
    }
  else
    {
      cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
      _gtk_rounded_box_path (padding_box, cr);
      outside = spread + clip_radius + MAX (fabs (x), fabs (y));
      clip_box = *padding_box;
      _gtk_rounded_box_grow (&clip_box, outside, outside, outside, outside);
      _gtk_rounded_box_clip_path (&clip_box, cr);

      cairo_clip (cr);
    }

  box = *padding_box;
  _gtk_rounded_box_move (&box, x, y);

  if (shadow->inset)
    _gtk_rounded_box_shrink (&box, spread, spread, spread, spread);
  else /* Outset */
    _gtk_rounded_box_grow (&box, spread, spread, spread, spread);

  clip_box = *padding_box;
  _gtk_rounded_box_shrink (&clip_box, -clip_radius, -clip_radius, -clip_radius, -clip_radius);

  if (!needs_blur (shadow))
    draw_shadow (shadow, cr, &box, &clip_box, GTK_BLUR_NONE);
  else
    {
      int i;
      cairo_region_t *remaining;
      cairo_rectangle_int_t r;

      /* For the blurred case we divide the rendering into 9 parts,
       * 4 of the corners, 4 for the horizonat/vertical lines and
       * one for the interior. We make the non-interior parts
       * large enought to fit the full radius of the blur, so that
       * the interior part can be drawn solidly.
       */

      if (shadow->inset)
	{
	  /* In the inset case we want to paint the whole clip-box.
	   * We could remove the part of "box" where the blur doesn't
	   * reach, but computing that is a bit tricky since the
	   * rounded corners are on the "inside" of it. */
	  r.x = floor (clip_box.box.x);
	  r.y = floor (clip_box.box.y);
	  r.width = ceil (clip_box.box.x + clip_box.box.width) - r.x;
	  r.height = ceil (clip_box.box.y + clip_box.box.height) - r.y;
	  remaining = cairo_region_create_rectangle (&r);
	}
      else
	{
	  /* In the outset case we want to paint the entire box, plus as far
	   * as the radius reaches from it */
	  r.x = floor (box.box.x - clip_radius);
	  r.y = floor (box.box.y - clip_radius);
	  r.width = ceil (box.box.x + box.box.width + clip_radius) - r.x;
	  r.height = ceil (box.box.y + box.box.height + clip_radius) - r.y;

	  remaining = cairo_region_create_rectangle (&r);
	}

      /* First do the corners of box */
      for (i = 0; i < 4; i++)
	{
	  cairo_save (cr);
          /* Always clip with remaining to ensure we never draw any area twice */
          gdk_cairo_region (cr, remaining);
          cairo_clip (cr);
	  draw_shadow_corner (shadow, cr, &box, &clip_box, i, &r);
	  cairo_restore (cr);

	  /* We drew the region, remove it from remaining */
	  cairo_region_subtract_rectangle (remaining, &r);
	}

      /* Then the sides */
      for (i = 0; i < 4; i++)
	{
	  cairo_save (cr);
          /* Always clip with remaining to ensure we never draw any area twice */
          gdk_cairo_region (cr, remaining);
          cairo_clip (cr);
	  draw_shadow_side (shadow, cr, &box, &clip_box, i, &r);
	  cairo_restore (cr);

	  /* We drew the region, remove it from remaining */
	  cairo_region_subtract_rectangle (remaining, &r);
	}

      /* Then the rest, which needs no blurring */

      cairo_save (cr);
      gdk_cairo_region (cr, remaining);
      cairo_clip (cr);
      draw_shadow (shadow, cr, &box, &clip_box, GTK_BLUR_NONE);
      cairo_restore (cr);

      cairo_region_destroy (remaining);
    }

  cairo_restore (cr);
}
