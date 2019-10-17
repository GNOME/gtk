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
  g_return_val_if_fail (target->class == &GTK_CSS_VALUE_SHADOW, NULL);

  return gtk_css_shadow_value_new (_gtk_css_number_value_new (0, GTK_CSS_PX),
                                   _gtk_css_number_value_new (0, GTK_CSS_PX),
                                   _gtk_css_number_value_new (0, GTK_CSS_PX),
                                   _gtk_css_number_value_new (0, GTK_CSS_PX),
                                   target->inset,
                                   _gtk_css_rgba_value_new_transparent ());
}

enum {
  HOFFSET,
  VOFFSET,
  RADIUS,
  SPREAD,
  N_VALUES
};

static gboolean
has_inset (GtkCssParser *parser,
           gpointer      option_data,
           gpointer      box_shadow_mode)
{
  return box_shadow_mode && gtk_css_parser_has_ident (parser, "inset");
}

static gboolean
parse_inset (GtkCssParser *parser,
             gpointer      option_data,
             gpointer      box_shadow_mode)
{
  gboolean *inset = option_data;

  if (!gtk_css_parser_try_ident (parser, "inset"))
    {
      g_assert_not_reached ();
      return FALSE;
    }

  *inset = TRUE;

  return TRUE;
}

static gboolean
parse_lengths (GtkCssParser *parser,
               gpointer      option_data,
               gpointer      box_shadow_mode)
{
  GtkCssValue **values = option_data;

  values[HOFFSET] = _gtk_css_number_value_parse (parser,
                                                 GTK_CSS_PARSE_LENGTH);
  if (values[HOFFSET] == NULL)
    return FALSE;

  values[VOFFSET] = _gtk_css_number_value_parse (parser,
                                                 GTK_CSS_PARSE_LENGTH);
  if (values[VOFFSET] == NULL)
    return FALSE;

  if (gtk_css_number_value_can_parse (parser))
    {
      values[RADIUS] = _gtk_css_number_value_parse (parser,
                                                    GTK_CSS_PARSE_LENGTH
                                                    | GTK_CSS_POSITIVE_ONLY);
      if (values[RADIUS] == NULL)
        return FALSE;
    }
  else
    values[RADIUS] = _gtk_css_number_value_new (0.0, GTK_CSS_PX);

  if (box_shadow_mode && gtk_css_number_value_can_parse (parser))
    {
      values[SPREAD] = _gtk_css_number_value_parse (parser,
                                                    GTK_CSS_PARSE_LENGTH);
      if (values[SPREAD] == NULL)
        return FALSE;
    }
  else
    values[SPREAD] = _gtk_css_number_value_new (0.0, GTK_CSS_PX);

  return TRUE;
}

static gboolean
parse_color (GtkCssParser *parser,
             gpointer      option_data,
             gpointer      box_shadow_mode)
{
  GtkCssValue **color = option_data;
  
  *color = _gtk_css_color_value_parse (parser);
  if (*color == NULL)
    return FALSE;

  return TRUE;
}

GtkCssValue *
_gtk_css_shadow_value_parse (GtkCssParser *parser,
                             gboolean      box_shadow_mode)
{
  GtkCssValue *values[N_VALUES] = { NULL, };
  GtkCssValue *color = NULL;
  gboolean inset = FALSE;
  GtkCssParseOption options[] =
    {
      { (void *) gtk_css_number_value_can_parse, parse_lengths, values },
      { has_inset, parse_inset, &inset },
      { (void *) gtk_css_color_value_can_parse, parse_color, &color },
    };
  guint i;

  if (!gtk_css_parser_consume_any (parser, options, G_N_ELEMENTS (options), GUINT_TO_POINTER (box_shadow_mode)))
    goto fail;

  if (values[0] == NULL)
    {
      gtk_css_parser_error_syntax (parser, "Expected shadow value to contain a length");
      goto fail;
    }

  if (color == NULL)
    color = _gtk_css_color_value_new_current_color ();

  return gtk_css_shadow_value_new (values[HOFFSET], values[VOFFSET],
                                   values[RADIUS], values[SPREAD],
                                   inset, color);

fail:
  for (i = 0; i < N_VALUES; i++)
    {
      g_clear_pointer (&values[i], gtk_css_value_unref);
    }
  g_clear_pointer (&color, gtk_css_value_unref);

  return NULL;
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
  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  /* We don't need to draw invisible shadows */
  if (gdk_rgba_is_clear (_gtk_css_rgba_value_get_rgba (shadow->color)))
    return;

  gtk_snapshot_append_outset_shadow (snapshot,
                                     border_box,
                                     _gtk_css_rgba_value_get_rgba (shadow->color),
                                     _gtk_css_number_value_get (shadow->hoffset, 0),
                                     _gtk_css_number_value_get (shadow->voffset, 0),
                                     _gtk_css_number_value_get (shadow->spread, 0),
                                     _gtk_css_number_value_get (shadow->radius, 0));
}

void
gtk_css_shadow_value_snapshot_inset (const GtkCssValue   *shadow,
                                     GtkSnapshot         *snapshot,
                                     const GskRoundedRect*padding_box)
{
  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  /* We don't need to draw invisible shadows */
  if (gdk_rgba_is_clear (_gtk_css_rgba_value_get_rgba (shadow->color)))
    return;

  gtk_snapshot_append_inset_shadow (snapshot,
                                    padding_box, 
                                    _gtk_css_rgba_value_get_rgba (shadow->color),
                                    _gtk_css_number_value_get (shadow->hoffset, 0),
                                    _gtk_css_number_value_get (shadow->voffset, 0),
                                    _gtk_css_number_value_get (shadow->spread, 0),
                                    _gtk_css_number_value_get (shadow->radius, 0));
}

gboolean
gtk_css_shadow_value_is_clear (const GtkCssValue *shadow)
{
  return gdk_rgba_is_clear (_gtk_css_rgba_value_get_rgba (shadow->color));
}

