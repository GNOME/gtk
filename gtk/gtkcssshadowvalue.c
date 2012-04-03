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

#include "gtkcssrgbavalueprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtksymboliccolorprivate.h"
#include "gtkthemingengineprivate.h"
#include "gtkpango.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  guint inset :1;

  gint16 hoffset;
  gint16 voffset;
  gint16 radius;
  gint16 spread;

  GtkCssValue *color;
};

static void
gtk_css_value_shadow_free (GtkCssValue *shadow)
{
  _gtk_css_value_unref (shadow->color);

  g_slice_free (GtkCssValue, shadow);
}

static gboolean
gtk_css_value_shadow_equal (const GtkCssValue *shadow1,
                            const GtkCssValue *shadow2)
{
  /* FIXME */
  return shadow1 == shadow2;
}

static GtkCssValue *
gtk_css_value_shadow_transition (GtkCssValue *start,
                                 GtkCssValue *end,
                                 double       progress)
{
  return NULL;
}

static void
gtk_css_value_shadow_print (const GtkCssValue *shadow,
                            GString           *string)
{
  if (shadow->inset)
    g_string_append (string, "inset ");

  g_string_append_printf (string, "%d %d ",
                          (gint) shadow->hoffset,
                          (gint) shadow->voffset);

  if (shadow->radius != 0)
    g_string_append_printf (string, "%d ", (gint) shadow->radius);

  if (shadow->spread != 0)
    g_string_append_printf (string, "%d ", (gint) shadow->spread);

  _gtk_css_value_print (shadow->color, string);
}

static const GtkCssValueClass GTK_CSS_VALUE_SHADOW = {
  gtk_css_value_shadow_free,
  gtk_css_value_shadow_equal,
  gtk_css_value_shadow_transition,
  gtk_css_value_shadow_print
};

static GtkCssValue *
gtk_css_shadow_value_new (gdouble hoffset,
                          gdouble voffset,
                          gdouble radius,
                          gdouble spread,
                          gboolean inset,
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
_gtk_css_shadow_value_parse (GtkCssParser *parser)
{
  gboolean have_inset, have_color, have_lengths;
  gdouble hoffset, voffset, blur, spread;
  GtkSymbolicColor *color;
  guint i;

  have_inset = have_lengths = have_color = FALSE;

  for (i = 0; i < 3; i++)
    {
      if (!have_inset && 
          _gtk_css_parser_try (parser, "inset", TRUE))
        {
          have_inset = TRUE;
          continue;
        }
        
      if (!have_lengths &&
          _gtk_css_parser_try_double (parser, &hoffset))
        {
          have_lengths = TRUE;

          if (!_gtk_css_parser_try_double (parser, &voffset))
            {
              _gtk_css_parser_error (parser, "Horizontal and vertical offsets are required");
              if (have_color)
                gtk_symbolic_color_unref (color);
              return NULL;
            }

          if (!_gtk_css_parser_try_double (parser, &blur))
            blur = 0;

          if (!_gtk_css_parser_try_double (parser, &spread))
            spread = 0;

          continue;
        }

      if (!have_color)
        {
          have_color = TRUE;

          /* XXX: the color is optional and UA-defined if it's missing,
           * but it doesn't really make sense for us...
           */
          color = _gtk_css_parser_read_symbolic_color (parser);

          if (color == NULL)
            return NULL;
        }
    }

  if (!have_color || !have_lengths)
    {
      _gtk_css_parser_error (parser, "Must specify at least color and offsets");
      if (have_color)
        gtk_symbolic_color_unref (color);
      return NULL;
    }

  return gtk_css_shadow_value_new (hoffset, voffset,
                                   blur, spread, have_inset,
                                   _gtk_css_value_new_take_symbolic_color (color));
}

GtkCssValue *
_gtk_css_shadow_value_compute (GtkCssValue     *shadow,
                               GtkStyleContext *context)
{
  GtkCssValue *color;

  color = _gtk_css_rgba_value_compute_from_symbolic (shadow->color,
                                                     _gtk_css_value_new_take_symbolic_color (
                                                       gtk_symbolic_color_ref (
                                                         _gtk_symbolic_color_get_current_color ())),
                                                     context,
                                                     FALSE);

  return gtk_css_shadow_value_new (shadow->hoffset, shadow->voffset,
                                   shadow->radius, shadow->spread, shadow->inset,
                                   color);
}

void
_gtk_css_shadow_value_paint_layout (const GtkCssValue *shadow,
                                    cairo_t           *cr,
                                    PangoLayout       *layout)
{
  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  if (!cairo_has_current_point (cr))
    cairo_move_to (cr, 0, 0);

  cairo_save (cr);

  cairo_rel_move_to (cr, shadow->hoffset, shadow->voffset);
  gdk_cairo_set_source_rgba (cr, _gtk_css_rgba_value_get_rgba (shadow->color));
  _gtk_pango_fill_layout (cr, layout);

  cairo_rel_move_to (cr, -shadow->hoffset, -shadow->voffset);
  cairo_restore (cr);
}

void
_gtk_css_shadow_value_paint_icon (const GtkCssValue *shadow,
			          cairo_t           *cr)
{
  cairo_pattern_t *pattern;

  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  cairo_save (cr);
  pattern = cairo_pattern_reference (cairo_get_source (cr));
  gdk_cairo_set_source_rgba (cr, _gtk_css_rgba_value_get_rgba (shadow->color));

  cairo_translate (cr, shadow->hoffset, shadow->voffset);
  cairo_mask (cr, pattern);

  cairo_restore (cr);
  cairo_pattern_destroy (pattern);
}

void
_gtk_css_shadow_value_paint_spinner (const GtkCssValue *shadow,
                                     cairo_t           *cr,
                                     gdouble            radius,
                                     gdouble            progress)
{
  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  cairo_save (cr);

  cairo_translate (cr, shadow->hoffset, shadow->voffset);
  _gtk_theming_engine_paint_spinner (cr,
                                     radius, progress,
                                     _gtk_css_rgba_value_get_rgba (shadow->color));

  cairo_restore (cr);
}

void
_gtk_css_shadow_value_paint_box (const GtkCssValue   *shadow,
                                 cairo_t             *cr,
                                 const GtkRoundedBox *padding_box)
{
  GtkRoundedBox box;

  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  cairo_save (cr);
  cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);

  _gtk_rounded_box_path (padding_box, cr);
  cairo_clip (cr);

  box = *padding_box;
  _gtk_rounded_box_move (&box, shadow->hoffset, shadow->voffset);
  _gtk_rounded_box_shrink (&box,
                           shadow->spread, shadow->spread,
                           shadow->spread, shadow->spread);

  _gtk_rounded_box_path (&box, cr);
  _gtk_rounded_box_clip_path (padding_box, cr);

  gdk_cairo_set_source_rgba (cr, _gtk_css_rgba_value_get_rgba (shadow->color));
  cairo_fill (cr);

  cairo_restore (cr);
}
