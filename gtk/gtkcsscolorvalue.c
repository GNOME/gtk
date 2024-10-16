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

#include "gtkcsscolorvalueprivate.h"

#include "gtkcssstylepropertyprivate.h"
#include "gtkprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkstyleproviderprivate.h"

#include "gdk/gdkhslaprivate.h"
#include "gdk/gdkrgbaprivate.h"
#include "gtkcsscolorprivate.h"
#include "gtkcolorutilsprivate.h"


static GtkCssValue * gtk_css_color_value_new_mix   (GtkCssValue *color1,
                                                    GtkCssValue *color2,
                                                    double       factor);
static GtkCssValue * gtk_css_color_value_new_alpha (GtkCssValue *color,
                                                    double       factor);
static GtkCssValue * gtk_css_color_value_new_shade (GtkCssValue *color,
                                                    double       factor);
static GtkCssValue * gtk_css_color_value_new_color_from_rgba (const GdkRGBA *rgba);
static GtkCssValue * gtk_css_color_value_new_color_mix (GtkCssColorSpace        color_space,
                                                        GtkCssHueInterpolation  hue_interpolation,
                                                        GtkCssValue            *color1,
                                                        GtkCssValue            *color2,
                                                        float                   percentage1,
                                                        float                   percentage2);
static GtkCssValue * gtk_css_color_value_new_relative  (GtkCssValue            *origin,
                                                        GtkCssColorSpace        color_space,
                                                        gboolean                legacy_srgb,
                                                        GtkCssValue             *values[4]);
static GtkCssValue * gtk_css_color_value_resolve  (GtkCssValue          *color,
                                                   GtkCssComputeContext *context,
                                                   GtkCssValue          *current);

typedef enum {
  COLOR_TYPE_COLOR,
  COLOR_TYPE_RELATIVE,
  COLOR_TYPE_NAME,
  COLOR_TYPE_COLOR_MIX,
  COLOR_TYPE_SHADE,
  COLOR_TYPE_ALPHA,
  COLOR_TYPE_MIX,
  COLOR_TYPE_CURRENT_COLOR,
} ColorType;

struct _GtkCssValue
{
  GTK_CSS_VALUE_BASE
  guint serialize_as_rgb : 1;
  guint type : 16;
  GdkRGBA rgba;

  union
  {
    char *name;
    GtkCssColor color;

    struct
    {
      GtkCssColorSpace color_space;
      GtkCssHueInterpolation hue_interpolation;
      GtkCssValue *color1;
      GtkCssValue *color2;
      float percentage1;
      float percentage2;
    } color_mix;

    struct
    {
      GtkCssValue *color;
      double factor;
    } shade, alpha;

    struct
    {
      GtkCssValue *color1;
      GtkCssValue *color2;
      double factor;
    } mix;

    struct
    {
      GtkCssValue *origin;
      GtkCssColorSpace color_space;
      gboolean legacy_srgb;
      GtkCssValue *values[1];
    } relative;
  };
};

/* {{{ GtkCssValue vfuncs */

static void
gtk_css_value_color_free (GtkCssValue *color)
{
  switch (color->type)
    {
    case COLOR_TYPE_NAME:
      g_free (color->name);
      break;

    case COLOR_TYPE_COLOR_MIX:
      gtk_css_value_unref (color->color_mix.color1);
      gtk_css_value_unref (color->color_mix.color2);
      break;

    case COLOR_TYPE_SHADE:
      gtk_css_value_unref (color->shade.color);
      break;

    case COLOR_TYPE_ALPHA:
      gtk_css_value_unref (color->alpha.color);
      break;

    case COLOR_TYPE_MIX:
      gtk_css_value_unref (color->mix.color1);
      gtk_css_value_unref (color->mix.color2);
      break;

    case COLOR_TYPE_RELATIVE:
      gtk_css_value_unref (color->relative.origin);
      for (guint i = 0; i < 4; i++)
        {
          if (color->relative.values[i])
            gtk_css_value_unref (color->relative.values[i]);
        }

      break;

    case COLOR_TYPE_COLOR:
    case COLOR_TYPE_CURRENT_COLOR:
    default:
      break;
    }

  g_free (color);
}

static GtkCssValue *
gtk_css_value_color_get_fallback (guint                 property_id,
                                  GtkCssComputeContext *context)
{
  switch (property_id)
    {
      case GTK_CSS_PROPERTY_BACKGROUND_IMAGE:
      case GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE:
      case GTK_CSS_PROPERTY_TEXT_SHADOW:
      case GTK_CSS_PROPERTY_ICON_SHADOW:
      case GTK_CSS_PROPERTY_BOX_SHADOW:
        return gtk_css_color_value_new_transparent ();

      case GTK_CSS_PROPERTY_COLOR:
      case GTK_CSS_PROPERTY_BACKGROUND_COLOR:
      case GTK_CSS_PROPERTY_BORDER_TOP_COLOR:
      case GTK_CSS_PROPERTY_BORDER_RIGHT_COLOR:
      case GTK_CSS_PROPERTY_BORDER_BOTTOM_COLOR:
      case GTK_CSS_PROPERTY_BORDER_LEFT_COLOR:
      case GTK_CSS_PROPERTY_OUTLINE_COLOR:
      case GTK_CSS_PROPERTY_CARET_COLOR:
      case GTK_CSS_PROPERTY_SECONDARY_CARET_COLOR:
        return gtk_css_value_compute (_gtk_css_style_property_get_initial_value (_gtk_css_style_property_lookup_by_id (property_id)),
                                       property_id,
                                       context);

      case GTK_CSS_PROPERTY_ICON_PALETTE:
        return gtk_css_value_ref (context->style->core->color);

      default:
        if (property_id < GTK_CSS_PROPERTY_N_PROPERTIES)
          g_warning ("No fallback color defined for property '%s'",
                     _gtk_style_property_get_name (GTK_STYLE_PROPERTY (_gtk_css_style_property_lookup_by_id (property_id))));
        return gtk_css_color_value_new_transparent ();
    }
}

static GtkCssValue *
gtk_css_value_color_compute (GtkCssValue          *value,
                             guint                 property_id,
                             GtkCssComputeContext *context)
{
  GtkCssValue *computed;

  computed = gtk_css_color_value_resolve (value, context, NULL);
  if (computed == NULL)
    return gtk_css_value_color_get_fallback (property_id, context);

  return computed;
}

static GtkCssValue *
gtk_css_value_color_resolve (GtkCssValue          *value,
                             GtkCssComputeContext *context,
                             GtkCssValue          *current)
{
  return gtk_css_color_value_resolve (value, context, current);
}

static gboolean
gtk_css_value_color_equal (const GtkCssValue *value1,
                           const GtkCssValue *value2)
{
  if (value1->type != value2->type)
    return FALSE;

  switch (value1->type)
    {
    case COLOR_TYPE_COLOR:
      return gtk_css_color_equal (&value1->color, &value2->color);

    case COLOR_TYPE_RELATIVE:
      return value1->relative.color_space == value2->relative.color_space &&
             value1->relative.legacy_srgb == value2->relative.legacy_srgb &&
             gtk_css_value_equal0 (value1->relative.values[0], value2->relative.values[0]) &&
             gtk_css_value_equal0 (value1->relative.values[1], value2->relative.values[1]) &&
             gtk_css_value_equal0 (value1->relative.values[2], value2->relative.values[2]) &&
             gtk_css_value_equal0 (value1->relative.values[3], value2->relative.values[3]);

    case COLOR_TYPE_NAME:
      return g_str_equal (value1->name, value2->name);

    case COLOR_TYPE_COLOR_MIX:
      return value1->color_mix.color_space == value2->color_mix.color_space &&
             value1->color_mix.hue_interpolation == value2->color_mix.hue_interpolation &&
             value1->color_mix.percentage1 == value2->color_mix.percentage1 &&
             value1->color_mix.percentage2 == value2->color_mix.percentage2 &&
             gtk_css_value_equal (value1->color_mix.color1,
                                  value2->color_mix.color1) &&
             gtk_css_value_equal (value1->color_mix.color2,
                                  value2->color_mix.color2);

    case COLOR_TYPE_SHADE:
      return value1->shade.factor == value2->shade.factor &&
             gtk_css_value_equal (value1->shade.color,
                                  value2->shade.color);

    case COLOR_TYPE_ALPHA:
      return value1->alpha.factor == value2->alpha.factor &&
             gtk_css_value_equal (value1->alpha.color,
                                  value2->alpha.color);

    case COLOR_TYPE_MIX:
      return value1->mix.factor == value2->mix.factor &&
             gtk_css_value_equal (value1->mix.color1,
                                  value2->mix.color1) &&
             gtk_css_value_equal (value1->mix.color2,
                                  value2->mix.color2);

    case COLOR_TYPE_CURRENT_COLOR:
      return TRUE;

    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static void
normalize_color_mix_percentages (float  p1,
                                 float  p2,
                                 float *out_p1,
                                 float *out_p2,
                                 float *alpha_multiplier)
{
  *alpha_multiplier = 1;

  if (p1 < -0.5 && p2 < -0.5)
    {
      if (out_p1)
        *out_p1 = 50;
      if (out_p2)
        *out_p2 = 50;
      return;
    }

  if (p1 < -0.5)
    {
      if (out_p1)
        *out_p1 = 100 - p2;
      if (out_p2)
        *out_p2 = p2;
      return;
    }

  if (p2 < -0.5)
    {
      if (out_p1)
        *out_p1 = p1;
      if (out_p2)
        *out_p2 = 100 - p1;
      return;
    }

  if (out_p1)
    *out_p1 = p1 / (p1 + p2) * 100;
  if (out_p2)
    *out_p2 = p2 / (p1 + p2) * 100;
  if (alpha_multiplier)
    *alpha_multiplier = MIN (1, (p1 + p2) * 0.01);
}

static GtkCssValue *
gtk_css_value_color_transition (GtkCssValue *start,
                                GtkCssValue *end,
                                guint        property_id,
                                double       progress)
{
  return gtk_css_color_value_new_mix (start, end, progress);
}

static void
gtk_css_value_color_print (const GtkCssValue *value,
                           GString           *string)
{
  char buffer[G_ASCII_DTOSTR_BUF_SIZE];

  switch (value->type)
    {
    case COLOR_TYPE_COLOR:
      gtk_css_color_print (&value->color, value->serialize_as_rgb, string);
      break;

    case COLOR_TYPE_RELATIVE:
      {
        switch (value->relative.color_space)
          {
          case GTK_CSS_COLOR_SPACE_SRGB:
            g_string_append (string, "color(from ");
            gtk_css_value_print (value->relative.origin, string);
            g_string_append (string, " srgb");
            break;

          case GTK_CSS_COLOR_SPACE_SRGB_LINEAR:
            g_string_append (string, "color(from ");
            gtk_css_value_print (value->relative.origin, string);
            g_string_append (string, " srgb-linear");
            break;

          case GTK_CSS_COLOR_SPACE_DISPLAY_P3:
            g_string_append (string, "color(from ");
            gtk_css_value_print (value->relative.origin, string);
            g_string_append (string, " display-p3");
            break;

          case GTK_CSS_COLOR_SPACE_XYZ:
            g_string_append (string, "color(from ");
            gtk_css_value_print (value->relative.origin, string);
            g_string_append (string, " xyz");
            break;

          case GTK_CSS_COLOR_SPACE_REC2020:
            g_string_append (string, "color(from ");
            gtk_css_value_print (value->relative.origin, string);
            g_string_append (string, " rec2020");
            break;

          case GTK_CSS_COLOR_SPACE_REC2100_PQ:
            g_string_append (string, "color(from ");
            gtk_css_value_print (value->relative.origin, string);
            g_string_append (string, " rec2100-pq");
            break;

          case GTK_CSS_COLOR_SPACE_HSL:
            g_string_append (string, "hsl(from ");
            gtk_css_value_print (value->relative.origin, string);
            break;

          case GTK_CSS_COLOR_SPACE_HWB:
            g_string_append (string, "hwb(from ");
            gtk_css_value_print (value->relative.origin, string);
            break;

          case GTK_CSS_COLOR_SPACE_OKLAB:
            g_string_append (string, "oklab(from ");
            gtk_css_value_print (value->relative.origin, string);
            break;

          case GTK_CSS_COLOR_SPACE_OKLCH:
            g_string_append (string, "oklch(from ");
            gtk_css_value_print (value->relative.origin, string);
            break;

          default:
            g_assert_not_reached ();
          }

        for (guint i = 0; i < 3; i++)
          {
            g_string_append_c (string, ' ');

            if (value->relative.values[i])
              {
                if (value->relative.legacy_srgb &&
                    gtk_css_number_value_get_dimension (value->relative.values[i]) == GTK_CSS_DIMENSION_NUMBER &&
                    !gtk_css_value_contains_current_color (value->relative.values[i]))
                  {
                    GtkCssValue *v;

                    v = gtk_css_number_value_multiply (value->relative.values[i], 1.0 / 255.0);
                    gtk_css_value_print (v, string);
                    gtk_css_value_unref (v);
                  }
                else
                  {
                    gtk_css_value_print (value->relative.values[i], string);
                  }
              }
            else
              g_string_append (string, "none");
          }

        if (value->relative.values[3] == NULL ||
            !gtk_css_value_is_computed (value->relative.values[3]) ||
            gtk_css_value_contains_current_color (value->relative.values[3]) ||
            gtk_css_number_value_get_canonical (value->relative.values[3], 1) < 0.999)
          {
            g_string_append (string, " / ");
            if (value->relative.values[3])
              gtk_css_value_print (value->relative.values[3], string);
            else
              g_string_append (string, "none");
          }

        g_string_append_c (string, ')');
      }
      break;

    case COLOR_TYPE_NAME:
      g_string_append (string, "@");
      g_string_append (string, value->name);
      break;

    case COLOR_TYPE_COLOR_MIX:
      {
        char percent[G_ASCII_DTOSTR_BUF_SIZE];
        float p1 = value->color_mix.percentage1;
        float p2 = value->color_mix.percentage2;
        gboolean p1_present = p1 > -0.5;
        gboolean p2_present = p2 > -0.5;
        gboolean defaults = (!p1_present || G_APPROX_VALUE (p1, 50, FLT_EPSILON)) &&
                            (!p2_present || G_APPROX_VALUE (p2, 50, FLT_EPSILON));

        if (!p1_present && p2_present && !defaults)
          {
            p1 = 100 - p2;
            p1_present = TRUE;
            p2_present = FALSE;
          }

        g_string_append (string, "color-mix(");
        gtk_css_color_interpolation_method_print (value->color_mix.color_space,
                                                  value->color_mix.hue_interpolation,
                                                  string);

        g_string_append (string, ", ");

        gtk_css_value_print (value->color_mix.color1, string);

        if (p1_present && !defaults)
          {
            g_ascii_dtostr (percent, sizeof (percent), p1);
            g_string_append_c (string, ' ');
            g_string_append (string, percent);
            g_string_append_c (string, '%');
          }

        g_string_append (string, ", ");

        gtk_css_value_print (value->color_mix.color2, string);

        if (p2_present && !defaults)
          {
            g_ascii_dtostr (percent, sizeof (percent), p2);
            g_string_append_c (string, ' ');
            g_string_append (string, percent);
            g_string_append_c (string, '%');
          }

        g_string_append (string, ")");
      }
      break;

    case COLOR_TYPE_SHADE:
      g_string_append (string, "shade(");
      gtk_css_value_print (value->shade.color, string);
      g_string_append (string, ", ");
      g_ascii_dtostr (buffer, sizeof (buffer), value->shade.factor);
      g_string_append (string, buffer);
      g_string_append_c (string, ')');
      break;

    case COLOR_TYPE_ALPHA:
      g_string_append (string, "alpha(");
      gtk_css_value_print (value->alpha.color, string);
      g_string_append (string, ", ");
      g_ascii_dtostr (buffer, sizeof (buffer), value->alpha.factor);
      g_string_append (string, buffer);
      g_string_append_c (string, ')');
      break;

    case COLOR_TYPE_MIX:
      g_string_append (string, "mix(");
      gtk_css_value_print (value->mix.color1, string);
      g_string_append (string, ", ");
      gtk_css_value_print (value->mix.color2, string);
      g_string_append (string, ", ");
      g_ascii_dtostr (buffer, sizeof (buffer), value->mix.factor);
      g_string_append (string, buffer);
      g_string_append_c (string, ')');
      break;

    case COLOR_TYPE_CURRENT_COLOR:
      g_string_append (string, "currentcolor");
      break;

    default:
      g_assert_not_reached ();
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_COLOR = {
  "GtkCssColorValue",
  gtk_css_value_color_free,
  gtk_css_value_color_compute,
  gtk_css_value_color_resolve,
  gtk_css_value_color_equal,
  gtk_css_value_color_transition,
  NULL,
  NULL,
  gtk_css_value_color_print
};

/* }}} */
/* {{{ Resolving colors */

static void
apply_alpha (const GdkRGBA *in,
             GdkRGBA       *out,
             double         factor)
{
  *out = *in;

  out->alpha = CLAMP (in->alpha * factor, 0, 1);
}

static void
apply_shade (const GdkRGBA *in,
             GdkRGBA       *out,
             double         factor)
{
  GdkHSLA hsla;

  _gdk_hsla_init_from_rgba (&hsla, in);
  _gdk_hsla_shade (&hsla, &hsla, factor);
  _gdk_rgba_init_from_hsla (out, &hsla);
}

static inline double
transition (double start,
            double end,
            double progress)
{
  return start + (end - start) * progress;
}

static void
apply_mix (const GdkRGBA *in1,
           const GdkRGBA *in2,
           GdkRGBA       *out,
           double         factor)
{
  out->alpha = CLAMP (transition (in1->alpha, in2->alpha, factor), 0, 1);

  if (out->alpha <= 0.0)
    {
      out->red = out->green = out->blue = 0.0;
    }
  else
    {
      out->red   = CLAMP (transition (in1->red * in1->alpha, in2->red * in2->alpha, factor), 0,  1) / out->alpha;
      out->green = CLAMP (transition (in1->green * in1->alpha, in2->green * in2->alpha, factor), 0,  1) / out->alpha;
      out->blue  = CLAMP (transition (in1->blue * in1->alpha, in2->blue * in2->alpha, factor), 0,  1) / out->alpha;
    }
}

static GtkCssValue *
apply_color_mix (GtkCssColorSpace        in,
                 GtkCssHueInterpolation  interp,
                 const GtkCssValue      *value1,
                 const GtkCssValue      *value2,
                 float                   percentage1,
                 float                   percentage2)
{
  float p2, alpha_multiplier;
  GtkCssColor output;
  gboolean missing[4];
  int i;

  normalize_color_mix_percentages (percentage1, percentage2,
                                   NULL, &p2, &alpha_multiplier);

  gtk_css_color_interpolate (&value1->color, &value2->color,
                             p2 * 0.01, in, interp, &output);

  output.values[3] *= alpha_multiplier;

  for (i = 0; i < 4; i++)
    missing[i] = gtk_css_color_component_missing (&output, i);

  return gtk_css_color_value_new_color (output.color_space,
                                        FALSE,
                                        output.values,
                                        missing);
}

static GtkCssValue *
resolve_relative (GtkCssValue      *values[4],
                  GtkCssColorSpace  color_space,
                  gboolean          legacy_rgb_scale)
{
  float v[4];
  gboolean m[4];

  for (guint i = 0; i < 4; i++)
    {
      if (values[i])
        {
          float lower, upper;

          gtk_css_color_space_get_coord_range (color_space, legacy_rgb_scale,
                                               i, &lower, &upper);

          m[i] = FALSE;
          v[i] = gtk_css_number_value_get_canonical (values[i], upper - lower);

          if (gtk_css_number_value_has_percent (values[i]))
            v[i] += lower;
        }
      else
        {
          m[i] = TRUE;
          v[i] = 0;
        }
    }

  if (color_space == GTK_CSS_COLOR_SPACE_SRGB &&
      legacy_rgb_scale)
    {
      v[0] /= 255.;
      v[1] /= 255.;
      v[2] /= 255.;
    }

  v[3] = CLAMP (v[3], 0, 1); /* clamp alpha */

  return gtk_css_color_value_new_color (color_space, FALSE, v, m);
}

static GtkCssValue *
gtk_css_color_value_do_resolve (GtkCssValue          *color,
                                GtkCssComputeContext *context,
                                GtkCssValue          *current,
                                GSList               *cycle_list)
{
  GtkStyleProvider *provider = context->provider;
  GtkCssValue *value;

  gtk_internal_return_val_if_fail (color != NULL, NULL);

  switch (color->type)
    {
    case COLOR_TYPE_COLOR:
      value = gtk_css_value_ref (color);
      break;

    case COLOR_TYPE_RELATIVE:
      {
        GtkCssValue *origin;
        GtkCssValue *vals[4];

        origin = gtk_css_color_value_do_resolve (color->relative.origin, context, current, cycle_list);
        if (origin == NULL)
          return NULL;

        for (guint i = 0; i < 4; i++)
          {
            if (color->relative.values[i])
              {
                if (current)
                  vals[i] = gtk_css_value_resolve (color->relative.values[i], context, current);
                else
                  vals[i] = gtk_css_value_compute (color->relative.values[i], GTK_CSS_PROPERTY_COLOR, context);
              }
            else
              vals[i] = NULL;
          }

        value = gtk_css_color_value_new_relative (origin,
                                                  color->relative.color_space,
                                                  color->relative.legacy_srgb,
                                                  vals);

        for (guint i = 0; i < 4; i++)
          {
            if (vals[i])
              gtk_css_value_unref (vals[i]);
          }

        gtk_css_value_unref (origin);
      }
      break;

    case COLOR_TYPE_NAME:
      {
        GtkCssValue *named;
        GSList cycle = { color, cycle_list };

        g_assert (provider != NULL);

        /* If color exists in cycle_list, we're currently resolving it.
         * So we've detected a cycle.
         */
        if (g_slist_find (cycle_list, color))
          return NULL;

        named = gtk_style_provider_get_color (provider, color->name);
        if (named == NULL)
          return NULL;

        value = gtk_css_color_value_do_resolve (named, context, current, &cycle);
      }
      break;

    case COLOR_TYPE_COLOR_MIX:
      {
        GtkCssValue *val1, *val2;

        val1 = gtk_css_color_value_do_resolve (color->color_mix.color1, context, current, cycle_list);
        if (val1 == NULL)
          return NULL;

        val2 = gtk_css_color_value_do_resolve (color->color_mix.color2, context, current, cycle_list);
        if (val2 == NULL)
          {
            gtk_css_value_unref (val1);
            return NULL;
          }

        value = gtk_css_color_value_new_color_mix (color->color_mix.color_space,
                                                   color->color_mix.hue_interpolation,
                                                   val1, val2,
                                                   color->color_mix.percentage1,
                                                   color->color_mix.percentage2);

        gtk_css_value_unref (val1);
        gtk_css_value_unref (val2);
      }
      break;

    case COLOR_TYPE_SHADE:
      {
        GtkCssValue *val;

        val = gtk_css_color_value_do_resolve (color->shade.color, context, current, cycle_list);
        if (val == NULL)
          return NULL;
        value = gtk_css_color_value_new_shade (val, color->shade.factor);
        gtk_css_value_unref (val);
      }
      break;

    case COLOR_TYPE_ALPHA:
      {
        GtkCssValue *val;

        val = gtk_css_color_value_do_resolve (color->alpha.color, context, current, cycle_list);
        if (val == NULL)
          return NULL;

        value = gtk_css_color_value_new_alpha (val, color->alpha.factor);
        gtk_css_value_unref (val);
      }
      break;

    case COLOR_TYPE_MIX:
      {
        GtkCssValue *val1, *val2;

        val1 = gtk_css_color_value_do_resolve (color->mix.color1, context, current, cycle_list);
        if (val1 == NULL)
          return NULL;

        val2 = gtk_css_color_value_do_resolve (color->mix.color2, context, current, cycle_list);
        if (val2 == NULL)
          {
            gtk_css_value_unref (val1);
            return NULL;
          }

        value = gtk_css_color_value_new_mix (val1, val2, color->mix.factor);
        gtk_css_value_unref (val1);
        gtk_css_value_unref (val2);
      }
      break;

    case COLOR_TYPE_CURRENT_COLOR:
      if (current)
        value = gtk_css_value_ref (current);
      else
        value = gtk_css_value_ref (color);
      break;

    default:
      g_assert_not_reached ();
    }

  return value;
}

/* gtk_css_color_value_resolve() can be called in two ways:
 * - at compute time, passing current == NULL, to make
 *   currentcolor compute to itself
 * - at use time, passing the appropriate value for current,
 *   to fully resolve color values that contain currentcolor
 *   references
 */
static GtkCssValue *
gtk_css_color_value_resolve (GtkCssValue          *color,
                             GtkCssComputeContext *context,
                             GtkCssValue          *current)
{
  gtk_internal_return_val_if_fail (context != NULL, NULL);

  return gtk_css_color_value_do_resolve (color, context, current, NULL);
}

/* }}} */
/* {{{ Constructors */

static GtkCssValue transparent_black_singleton = { .class = &GTK_CSS_VALUE_COLOR,
                                                   .ref_count = 1,
                                                   .is_computed = TRUE,
                                                   .contains_variables = FALSE,
                                                   .contains_current_color = FALSE,
                                                   .serialize_as_rgb = TRUE,
                                                   .type = COLOR_TYPE_COLOR,
                                                   .rgba = {0, 0, 0, 0},
                                                   .color = { GTK_CSS_COLOR_SPACE_SRGB, {0, 0, 0, 0}, 0 } };
static GtkCssValue white_singleton             = { .class = &GTK_CSS_VALUE_COLOR,
                                                   .ref_count = 1,
                                                   .is_computed = TRUE,
                                                   .contains_variables = FALSE,
                                                   .contains_current_color = FALSE,
                                                   .serialize_as_rgb = TRUE,
                                                   .type = COLOR_TYPE_COLOR,
                                                   .rgba = {1, 1, 1, 1},
                                                   .color = { GTK_CSS_COLOR_SPACE_SRGB, {1, 1, 1, 1}, 0 } };

static GtkCssValue current_color_singleton     = { .class = &GTK_CSS_VALUE_COLOR,
                                                   .ref_count = 1,
                                                   .is_computed = TRUE,
                                                   .contains_variables = FALSE,
                                                   .contains_current_color = TRUE,
                                                   .serialize_as_rgb = FALSE,
                                                   .type = COLOR_TYPE_CURRENT_COLOR,
                                                   .rgba = {0, 0, 0, 0 },
                                                   .color = { GTK_CSS_COLOR_SPACE_SRGB, {0, 0, 0, 0}, 0 } };

GtkCssValue *
gtk_css_color_value_new_transparent (void)
{
  return gtk_css_value_ref (&transparent_black_singleton);
}

GtkCssValue *
gtk_css_color_value_new_white (void)
{
  return gtk_css_value_ref (&white_singleton);
}

GtkCssValue *
gtk_css_color_value_new_current_color (void)
{
  return gtk_css_value_ref (&current_color_singleton);
}

GtkCssValue *
gtk_css_color_value_new_color (GtkCssColorSpace color_space,
                               gboolean         serialize_as_rgb,
                               const float      values[4],
                               gboolean         missing[4])
{
  GtkCssValue *value;
  GtkCssColor tmp;

  value = gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->color.color_space = color_space;
  value->is_computed = TRUE;
  value->serialize_as_rgb = serialize_as_rgb;
  value->type = COLOR_TYPE_COLOR;
  gtk_css_color_init_with_missing (&value->color, color_space, values, missing);

  gtk_css_color_convert (&value->color, GTK_CSS_COLOR_SPACE_SRGB, &tmp);

  value->rgba.red = tmp.values[0];
  value->rgba.green = tmp.values[1];
  value->rgba.blue = tmp.values[2];
  value->rgba.alpha = tmp.values[3];

  return value;
}

static GtkCssValue *
gtk_css_color_value_new_color_from_rgba (const GdkRGBA *rgba)
{
  GtkCssValue *value;

  g_return_val_if_fail (rgba != NULL, NULL);

  if (gdk_rgba_equal (rgba, &white_singleton.rgba))
    return gtk_css_value_ref (&white_singleton);

  if (gdk_rgba_equal (rgba, &transparent_black_singleton.rgba))
    return gtk_css_value_ref (&transparent_black_singleton);

  value = gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->color.color_space = GTK_CSS_COLOR_SPACE_SRGB;
  value->is_computed = TRUE;
  value->serialize_as_rgb = TRUE;
  value->type = COLOR_TYPE_COLOR;
  gtk_css_color_init (&value->color,
                      GTK_CSS_COLOR_SPACE_SRGB,
                      (float[4]) { rgba->red, rgba->green, rgba->blue, rgba->alpha });

  value->rgba = *rgba;

  return value;
}

static GtkCssValue *
gtk_css_color_value_new_relative (GtkCssValue      *origin,
                                  GtkCssColorSpace  color_space,
                                  gboolean          legacy_srgb,
                                  GtkCssValue      *values[4])
{
  GtkCssValue *value;
  gboolean computed = TRUE;
  gboolean resolved = TRUE;

  if (!gtk_css_value_is_computed (origin))
    computed = FALSE;

  if (gtk_css_value_contains_current_color (origin))
    resolved = FALSE;

  if (computed || resolved)
    {
      for (guint i = 0; i < 4; i++)
        {
          if (values[i])
            {
              if (!gtk_css_value_is_computed (values[i]) &&
                  !gtk_css_number_value_has_percent (values[i]))
                computed = FALSE;
              if (gtk_css_value_contains_current_color (values[i]))
                resolved = FALSE;
            }
        }
    }

  if (computed && resolved)
    return resolve_relative (values, color_space, legacy_srgb);

  value = gtk_css_value_alloc (&GTK_CSS_VALUE_COLOR,
                               sizeof (GtkCssValue) + sizeof (GtkCssValue *) * 3);
  value->relative.color_space = color_space;
  value->is_computed = computed;
  value->contains_current_color = !resolved;
  value->type = COLOR_TYPE_RELATIVE;

  value->relative.origin = gtk_css_value_ref (origin);
  value->relative.legacy_srgb = legacy_srgb;

  for (guint i = 0; i < 4; i++)
    {
      if (values[i])
        value->relative.values[i] = gtk_css_value_ref (values[i]);
    }

  return value;
}

GtkCssValue *
gtk_css_color_value_new_name (const char *name)
{
  GtkCssValue *value;

  gtk_internal_return_val_if_fail (name != NULL, NULL);

  value = gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->type = COLOR_TYPE_NAME;
  value->name = g_strdup (name);

  return value;
}

static GtkCssValue *
gtk_css_color_value_new_shade (GtkCssValue *color,
                               double       factor)
{
  GtkCssValue *value;

  gtk_internal_return_val_if_fail (color->class == &GTK_CSS_VALUE_COLOR, NULL);

  if (color->is_computed && !color->contains_current_color)
    {
      GdkRGBA c;

      apply_shade (&color->rgba, &c, factor);

      return gtk_css_color_value_new_color_from_rgba (&c);
    }

  value = gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->type = COLOR_TYPE_SHADE;
  value->is_computed = color->is_computed;
  value->contains_current_color = color->contains_current_color;
  value->shade.color = gtk_css_value_ref (color);
  value->shade.factor = factor;

  return value;
}

static GtkCssValue *
gtk_css_color_value_new_alpha (GtkCssValue *color,
                               double       factor)
{
  GtkCssValue *value;

  gtk_internal_return_val_if_fail (color->class == &GTK_CSS_VALUE_COLOR, NULL);

  if (color->is_computed && !color->contains_current_color)
    {
      GdkRGBA c;

      apply_alpha (&color->rgba, &c, factor);

      return gtk_css_color_value_new_color_from_rgba (&c);
    }

  value = gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->type = COLOR_TYPE_ALPHA;
  value->is_computed = color->is_computed;
  value->contains_current_color = color->contains_current_color;
  value->alpha.color = gtk_css_value_ref (color);
  value->alpha.factor = factor;

  return value;
}

static GtkCssValue *
gtk_css_color_value_new_color_mix (GtkCssColorSpace        color_space,
                                   GtkCssHueInterpolation  hue_interpolation,
                                   GtkCssValue            *color1,
                                   GtkCssValue            *color2,
                                   float                   percentage1,
                                   float                   percentage2)
{
  GtkCssValue *value;

  gtk_internal_return_val_if_fail (color1->class == &GTK_CSS_VALUE_COLOR, NULL);
  gtk_internal_return_val_if_fail (color2->class == &GTK_CSS_VALUE_COLOR, NULL);

  if (percentage1 > -0.5 && percentage2 > -0.5 && fabs (percentage1 + percentage2) < 0.001)
    return NULL;

  if (color1->is_computed && color2->is_computed &&
      !color1->contains_current_color && !color2->contains_current_color)
    {
      return apply_color_mix (color_space, hue_interpolation,
                              color1, color2,
                              percentage1, percentage2);
    }

  value = gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->is_computed = color1->is_computed &&
                       color2->is_computed;
  value->contains_current_color = color1->contains_current_color ||
                                  color2->contains_current_color;
  value->type = COLOR_TYPE_COLOR_MIX;
  value->color_mix.color_space = color_space;
  value->color_mix.hue_interpolation = hue_interpolation;
  value->color_mix.color1 = gtk_css_value_ref (color1);
  value->color_mix.color2 = gtk_css_value_ref (color2);
  value->color_mix.percentage1 = percentage1;
  value->color_mix.percentage2 = percentage2;

  return value;
}

static GtkCssValue *
gtk_css_color_value_new_mix (GtkCssValue *color1,
                             GtkCssValue *color2,
                             double       factor)
{
  GtkCssValue *value;

  gtk_internal_return_val_if_fail (color1->class == &GTK_CSS_VALUE_COLOR, NULL);
  gtk_internal_return_val_if_fail (color2->class == &GTK_CSS_VALUE_COLOR, NULL);

  if (color1->is_computed && color2->is_computed &&
      !color1->contains_current_color && !color2->contains_current_color)
    {
      GdkRGBA result;

      apply_mix (&color1->rgba, &color2->rgba, &result, factor);

      return gtk_css_color_value_new_color_from_rgba (&result);

    }

  value = gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->type = COLOR_TYPE_MIX;
  value->is_computed = color1->is_computed &&
                       color2->is_computed;
  value->contains_current_color = color1->contains_current_color ||
                                  color2->contains_current_color;
  value->mix.color1 = gtk_css_value_ref (color1);
  value->mix.color2 = gtk_css_value_ref (color2);
  value->mix.factor = factor;

  return value;
}

/* }}} */
/* {{{ Parsing */

gboolean
gtk_css_color_value_can_parse (GtkCssParser *parser)
{
  /* This is way too generous, but meh... */
  return gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT)
      || gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_AT_KEYWORD)
      || gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_HASH_ID)
      || gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_HASH_UNRESTRICTED)
      || gtk_css_parser_has_function (parser, "lighter")
      || gtk_css_parser_has_function (parser, "darker")
      || gtk_css_parser_has_function (parser, "shade")
      || gtk_css_parser_has_function (parser, "alpha")
      || gtk_css_parser_has_function (parser, "mix")
      || gtk_css_parser_has_function (parser, "hsl")
      || gtk_css_parser_has_function (parser, "hsla")
      || gtk_css_parser_has_function (parser, "rgb")
      || gtk_css_parser_has_function (parser, "rgba")
      || gtk_css_parser_has_function (parser, "hwb")
      || gtk_css_parser_has_function (parser, "oklab")
      || gtk_css_parser_has_function (parser, "oklch")
      || gtk_css_parser_has_function (parser, "color")
      || gtk_css_parser_has_function (parser, "color-mix");
}

typedef struct
{
  GtkCssColorSpace color_space;
  GtkCssHueInterpolation hue_interpolation;
  GtkCssValue *color1;
  GtkCssValue *color2;
  float percentage1;
  float percentage2;
} ColorMixData;

static gboolean
parse_color_mix_component (GtkCssParser  *parser,
                           GtkCssValue  **value,
                           float         *percentage)
{
  *percentage = -1;

  if (gtk_css_color_value_can_parse (parser))
    {
      *value = gtk_css_color_value_parse (parser);
      if (*value == NULL)
        return FALSE;
    }

  if (gtk_css_number_value_can_parse (parser))
    {
      GtkCssValue *val;
      val = gtk_css_number_value_parse (parser, GTK_CSS_PARSE_PERCENT | GTK_CSS_POSITIVE_ONLY);
      if (val == NULL)
        return FALSE;

      *percentage = gtk_css_number_value_get_canonical (val, 100);

      if (*percentage > 100)
        {
          gtk_css_parser_error_syntax (parser, "Values above 100%% are not allowed");
          gtk_css_value_unref (val);
          return FALSE;
        }

      gtk_css_value_unref (val);
    }

  if (*value == NULL)
    {
      *value = gtk_css_color_value_parse (parser);
      if (*value == NULL)
        return FALSE;
    }

  return TRUE;
}

static guint
parse_color_mix (GtkCssParser *parser,
                 guint         arg,
                 gpointer      data_)
{
  ColorMixData *data = data_;

  switch (arg)
  {
    case 0:
      if (!gtk_css_color_interpolation_method_parse (parser, &data->color_space, &data->hue_interpolation))
        return 0;
      return 1;

    case 1:
      if (!parse_color_mix_component (parser, &data->color1, &data->percentage1))
        return 0;
      return 1;

    case 2:
      if (!parse_color_mix_component (parser, &data->color2, &data->percentage2))
        return 0;
      return 1;

    default:
      g_assert_not_reached ();
  }
}

typedef struct
{
  GtkCssValue *color;
  GtkCssValue *color2;
  double       value;
} ColorFunctionData;

static guint
parse_legacy_mix (GtkCssParser *parser,
                  guint         arg,
                  gpointer      data_)
{
  ColorFunctionData *data = data_;

  switch (arg)
  {
    case 0:
      data->color = gtk_css_color_value_parse (parser);
      if (data->color == NULL)
        return 0;
      return 1;

    case 1:
      data->color2 = gtk_css_color_value_parse (parser);
      if (data->color2 == NULL)
        return 0;
      return 1;

    case 2:
      if (!gtk_css_parser_consume_number (parser, &data->value))
        return 0;
      return 1;

    default:
      g_return_val_if_reached (0);
  }
}

static guint
parse_color_number (GtkCssParser *parser,
                    guint         arg,
                    gpointer      data_)
{
  ColorFunctionData *data = data_;

  switch (arg)
  {
    case 0:
      data->color = gtk_css_color_value_parse (parser);
      if (data->color == NULL)
        return 0;
      return 1;

    case 1:
      if (!gtk_css_parser_consume_number (parser, &data->value))
        return 0;
      return 1;

    default:
      g_return_val_if_reached (0);
  }
}

typedef enum
{
  COLOR_SYNTAX_DETECTING,
  COLOR_SYNTAX_MODERN,
  COLOR_SYNTAX_LEGACY,
} ColorSyntax;

typedef struct
{
  ColorSyntax syntax;
  gboolean serialize_as_rgb;
  GtkCssValue *values[4];
  float v[4];
  gboolean alpha_specified;
  gboolean use_percentages;
  GtkCssNumberParseContext ctx;
} ParseData;

static gboolean
parse_rgb_channel_value (GtkCssParser *parser,
                         ParseData    *data,
                         guint         idx)
{
  gboolean has_percentage =
    gtk_css_token_is (gtk_css_parser_get_token (parser), GTK_CSS_TOKEN_PERCENTAGE);

  switch (data->syntax)
  {
    case COLOR_SYNTAX_DETECTING:
      data->use_percentages = has_percentage;
      break;

    case COLOR_SYNTAX_LEGACY:
      if (data->use_percentages != has_percentage)
        {
          gtk_css_parser_error_syntax (parser, "Legacy color syntax doesn't allow mixing numbers and percentages");
          return FALSE;
        }

      break;

    case COLOR_SYNTAX_MODERN:
      break;

    default:
      g_assert_not_reached ();
  }

  if (data->syntax != COLOR_SYNTAX_LEGACY &&
      gtk_css_parser_try_ident (parser, "none"))
    {
      data->syntax = COLOR_SYNTAX_MODERN;
      data->values[idx] = NULL;
      data->v[idx] = 0;
    }
  else
    {
      data->values[idx] = gtk_css_number_value_parse_with_context (parser, GTK_CSS_PARSE_NUMBER | GTK_CSS_PARSE_PERCENT, &data->ctx);

      if (data->values[idx] == NULL)
        return FALSE;

      data->v[idx] = gtk_css_number_value_get_canonical (data->values[idx], 255);
      data->v[idx] = CLAMP (data->v[idx], 0.0, 255.0) / 255.0;
    }

  return TRUE;
}

static gboolean
parse_alpha_value (GtkCssParser *parser,
                   ParseData    *data,
                   guint         idx)
{
  GtkCssNumberParseFlags flags = GTK_CSS_PARSE_NUMBER;

  if (data->syntax == COLOR_SYNTAX_MODERN)
    flags |= GTK_CSS_PARSE_PERCENT;

  if (data->syntax != COLOR_SYNTAX_LEGACY &&
      gtk_css_parser_try_ident (parser, "none"))
    {
      data->syntax = COLOR_SYNTAX_MODERN;
      data->values[idx] = NULL;
      data->v[idx] = 0;
    }
  else
    {
      data->values[idx] = gtk_css_number_value_parse_with_context (parser, flags, &data->ctx);
      if (data->values[idx] == NULL)
        return FALSE;

      data->v[idx] = gtk_css_number_value_get_canonical (data->values[idx], 1);
      data->v[idx] = CLAMP (data->v[idx], 0.0, 1.0);
    }

  data->alpha_specified = TRUE;

  return TRUE;
}

static gboolean
parse_hsl_channel_value (GtkCssParser *parser,
                         ParseData    *data,
                         guint         idx)
{
  if (data->syntax != COLOR_SYNTAX_LEGACY &&
      gtk_css_parser_try_ident (parser, "none"))
    {
      data->syntax = COLOR_SYNTAX_MODERN;
      data->values[idx] = NULL;
      data->v[idx] = 0;
    }
  else
    {
      GtkCssNumberParseFlags flags = GTK_CSS_PARSE_PERCENT;

      if (data->syntax == COLOR_SYNTAX_MODERN)
        flags |= GTK_CSS_PARSE_NUMBER;

      data->values[idx] = gtk_css_number_value_parse_with_context (parser, flags, &data->ctx);
      if (data->values[idx] == NULL)
        return FALSE;

      data->v[idx] = gtk_css_number_value_get_canonical (data->values[idx], 100);
      data->v[idx] = CLAMP (data->v[idx], 0.0, 100.0);
    }

  return TRUE;
}

static gboolean
parse_hwb_channel_value (GtkCssParser *parser,
                         ParseData    *data,
                         guint         idx)
{
  if (data->syntax != COLOR_SYNTAX_LEGACY &&
      gtk_css_parser_try_ident (parser, "none"))
    {
      data->syntax = COLOR_SYNTAX_MODERN;
      data->values[idx] = NULL;
      data->v[idx] = 0;
    }
  else
    {
      GtkCssNumberParseFlags flags = GTK_CSS_PARSE_PERCENT | GTK_CSS_PARSE_NUMBER;

      data->values[idx] = gtk_css_number_value_parse_with_context (parser, flags, &data->ctx);
      if (data->values[idx] == NULL)
        return FALSE;

      data->v[idx] = gtk_css_number_value_get_canonical (data->values[idx], 100);
      data->v[idx] = CLAMP (data->v[idx], 0.0, 100.0);
    }

  return TRUE;
}

static gboolean
parse_hue_value (GtkCssParser *parser,
                 ParseData    *data,
                 guint         idx)
{
  if (data->syntax != COLOR_SYNTAX_LEGACY &&
      gtk_css_parser_try_ident (parser, "none"))
    {
      data->syntax = COLOR_SYNTAX_MODERN;
      data->values[idx] = NULL;
      data->v[idx] = 0;
    }
  else
    {
      data->values[idx] = gtk_css_number_value_parse_with_context (parser, GTK_CSS_PARSE_NUMBER | GTK_CSS_PARSE_ANGLE, &data->ctx);
      if (data->values[idx] == NULL)
        return FALSE;

      data->v[idx] = gtk_css_number_value_get_canonical (data->values[idx], 360);
      data->v[idx] = fmod (data->v[idx], 360);
      if (data->v[idx] < 0)
        data->v[idx] += 360;
    }

  return TRUE;
}

static gboolean
parse_ok_L_value (GtkCssParser *parser,
                  ParseData    *data,
                  guint         idx)
{
  if (gtk_css_parser_try_ident (parser, "none"))
    {
      data->values[idx] = NULL;
      data->v[idx] = 0;
    }
  else
    {
      GtkCssNumberParseFlags flags = GTK_CSS_PARSE_PERCENT | GTK_CSS_PARSE_NUMBER;

      data->values[idx] = gtk_css_number_value_parse_with_context (parser, flags, &data->ctx);
      if (data->values[idx] == NULL)
        return FALSE;

      data->v[idx] = gtk_css_number_value_get_canonical (data->values[idx], 1);
      data->v[idx] = CLAMP (data->v[idx], 0.0, 1.0);
    }

  return TRUE;
}

static gboolean
parse_ok_C_value (GtkCssParser *parser,
                  ParseData    *data,
                  guint         idx)
{
  if (gtk_css_parser_try_ident (parser, "none"))
    {
      data->values[idx] = NULL;
      data->v[idx] = 0;
    }
  else
    {
      GtkCssNumberParseFlags flags = GTK_CSS_PARSE_PERCENT | GTK_CSS_PARSE_NUMBER;

      data->values[idx] = gtk_css_number_value_parse_with_context (parser, flags, &data->ctx);
      if (data->values[idx] == NULL)
        return FALSE;

      data->v[idx] = gtk_css_number_value_get_canonical (data->values[idx], 0.4);
      data->v[idx] = MAX (data->v[idx], 0.0);
    }

  return TRUE;
}

static gboolean
parse_ok_ab_value (GtkCssParser *parser,
                   ParseData    *data,
                   guint         idx)
{
  if (gtk_css_parser_try_ident (parser, "none"))
    {
      data->values[idx] = NULL;
      data->v[idx] = 0;
    }
  else
    {
      GtkCssNumberParseFlags flags = GTK_CSS_PARSE_PERCENT | GTK_CSS_PARSE_NUMBER;

      data->values[idx] = gtk_css_number_value_parse_with_context (parser, flags, &data->ctx);
      if (data->values[idx] == NULL)
        return FALSE;

      data->v[idx] = gtk_css_number_value_get_canonical (data->values[idx], 0.8);

      /* gtk_css_number_value_get_canonical() doesn't let us specify what 0% is */
      if (gtk_css_number_value_has_percent (data->values[idx]))
        data->v[idx] -= 0.4;
    }

  return TRUE;
}

static guint
parse_rgba_color_channel (GtkCssParser *parser,
                          ParseData    *data,
                          guint         arg)
{
  switch (arg)
    {
    case 0:
    case 1:
    case 2:
      return parse_rgb_channel_value (parser, data, arg);

    case 3:
      return parse_alpha_value (parser, data, arg);

    default:
      g_assert_not_reached ();
    }
}

static guint
parse_hsla_color_channel (GtkCssParser *parser,
                          ParseData    *data,
                          guint         arg)
{
  switch (arg)
    {
    case 0:
      return parse_hue_value (parser, data, arg);

    case 1:
    case 2:
      return parse_hsl_channel_value (parser, data, arg);

    case 3:
      return parse_alpha_value (parser, data, arg);

    default:
      g_assert_not_reached ();
    }
}

static guint
parse_hwb_color_channel (GtkCssParser *parser,
                         ParseData    *data,
                         guint         arg)
{
  switch (arg)
    {
    case 0:
      return parse_hue_value (parser, data, arg);

    case 1:
    case 2:
      return parse_hwb_channel_value (parser, data, arg);

    case 3:
      return parse_alpha_value (parser, data, arg);

    default:
      g_assert_not_reached ();
    }
}

static guint
parse_oklab_color_channel (GtkCssParser *parser,
                           ParseData    *data,
                           guint         arg)
{
  switch (arg)
    {
    case 0:
      return parse_ok_L_value (parser, data, arg);

    case 1:
    case 2:
      return parse_ok_ab_value (parser, data, arg);

    case 3:
      return parse_alpha_value (parser, data, arg);

    default:
      g_assert_not_reached ();
    }
}

static guint
parse_oklch_color_channel (GtkCssParser *parser,
                           ParseData    *data,
                           guint         arg)
{
  switch (arg)
    {
    case 0:
      return parse_ok_L_value (parser, data, arg);

    case 1:
      return parse_ok_C_value (parser, data, arg);

    case 2:
      return parse_hue_value (parser, data, arg);

    case 3:
      return parse_alpha_value (parser, data, arg);

    default:
      g_assert_not_reached ();
    }
}

static gboolean
parse_color_channel_value (GtkCssParser *parser,
                           ParseData    *data,
                           guint         idx)
{
  if (gtk_css_parser_try_ident (parser, "none"))
    {
      data->values[idx] = NULL;
      data->v[idx] = 0;
    }
  else
    {
      GtkCssNumberParseFlags flags = GTK_CSS_PARSE_NUMBER | GTK_CSS_PARSE_PERCENT;

      data->values[idx] = gtk_css_number_value_parse_with_context (parser, flags, &data->ctx);
      if (data->values[idx] == NULL)
        return FALSE;

      data->v[idx] = gtk_css_number_value_get_canonical (data->values[idx], 1);
    }

  return TRUE;
}

static guint
parse_color_color_channel (GtkCssParser *parser,
                           ParseData    *data,
                           guint         arg)
{
  const GtkCssToken *token;

  switch (arg)
    {
    case 0:
      if (gtk_css_parser_try_ident (parser, "srgb"))
        {
          data->ctx.color_space = GTK_CSS_COLOR_SPACE_SRGB;
          return 1;
        }

      if (gtk_css_parser_try_ident (parser, "srgb-linear"))
        {
          data->ctx.color_space = GTK_CSS_COLOR_SPACE_SRGB_LINEAR;
          return 1;
        }

      if (gtk_css_parser_try_ident (parser, "display-p3"))
        {
          data->ctx.color_space = GTK_CSS_COLOR_SPACE_DISPLAY_P3;
          return 1;
        }

      if (gtk_css_parser_try_ident (parser, "xyz"))
        {
          data->ctx.color_space = GTK_CSS_COLOR_SPACE_XYZ;
          return 1;
        }

      if (gtk_css_parser_try_ident (parser, "rec2020"))
        {
          data->ctx.color_space = GTK_CSS_COLOR_SPACE_REC2020;
          return 1;
        }

      if (gtk_css_parser_try_ident (parser, "rec2100-pq"))
        {
          data->ctx.color_space = GTK_CSS_COLOR_SPACE_REC2100_PQ;
          return 1;
        }

      token = gtk_css_parser_get_token (parser);
      gtk_css_parser_error_syntax (parser,
                                   "Invalid color space in color(): %s",
                                   gtk_css_token_to_string (token));

      return 0;

    case 1:
    case 2:
    case 3:
      return parse_color_channel_value (parser, data, arg - 1);

    case 4:
      return parse_alpha_value (parser, data, 3);

    default:
      g_assert_not_reached ();
    }
}

static gboolean
parse_color_function (GtkCssParser     *self,
                      gboolean          parse_color_space,
                      gboolean          allow_alpha,
                      gboolean          require_alpha,
                      guint (* parse_func) (GtkCssParser *, ParseData *, guint),
                      ParseData        *data)
{
  const GtkCssToken *token;
  gboolean result = FALSE;
  char function_name[64];
  guint arg;
  guint min_args = 3;
  guint max_args = 4;

  if (parse_color_space)
    {
      min_args++;
      max_args++;
    }

  token = gtk_css_parser_get_token (self);
  g_return_val_if_fail (gtk_css_token_is (token, GTK_CSS_TOKEN_FUNCTION), FALSE);

  g_strlcpy (function_name, gtk_css_token_get_string (token), 64);
  gtk_css_parser_start_block (self);

  if (gtk_css_parser_try_ident (self, "from"))
    {
      data->ctx.color = gtk_css_color_value_parse (self);
      data->syntax = COLOR_SYNTAX_MODERN;
      data->serialize_as_rgb = FALSE;
    }

  arg = 0;
  while (TRUE)
    {
      guint parse_args = parse_func (self, data, arg);
      if (parse_args == 0)
        break;
      arg += parse_args;
      token = gtk_css_parser_get_token (self);

      if (data->syntax == COLOR_SYNTAX_DETECTING)
        {
          if (gtk_css_token_is (token, GTK_CSS_TOKEN_COMMA))
            {
              data->syntax = COLOR_SYNTAX_LEGACY;
              min_args = require_alpha ? 4 : 3;
              max_args = allow_alpha ? 4 : 3;
            }
          else
            {
              data->syntax = COLOR_SYNTAX_MODERN;
            }
        }

      if (gtk_css_token_is (token, GTK_CSS_TOKEN_EOF))
        {
          if (arg < min_args)
            {
              gtk_css_parser_error_syntax (self, "%s() requires at least %u arguments", function_name, min_args);
              break;
            }
          else
            {
              result = TRUE;
              break;
            }
        }
      else if (gtk_css_token_is (token, GTK_CSS_TOKEN_COMMA))
        {
          if (data->syntax == COLOR_SYNTAX_MODERN)
            {
              gtk_css_parser_error_syntax (self, "Commas aren't allowed in modern %s() syntax", function_name);
              break;
            }

          if (arg >= max_args)
            {
              gtk_css_parser_error_syntax (self, "Expected ')' at end of %s()", function_name);
              break;
            }

          gtk_css_parser_consume_token (self);
          continue;
        }
      else if (data->syntax == COLOR_SYNTAX_LEGACY)
        {
          gtk_css_parser_error_syntax (self, "Unexpected data at end of %s() argument", function_name);
          break;
        }
      else if (arg == min_args)
        {
          if (gtk_css_token_is_delim (token, '/'))
            {
              gtk_css_parser_consume_token (self);
              continue;
            }

          if (arg >= max_args)
            {
              gtk_css_parser_error_syntax (self, "Expected ')' at end of %s()", function_name);
              break;
            }

          gtk_css_parser_error_syntax (self, "Expected '/' or ')'");
          break;
        }
      else if (arg >= max_args)
        {
          gtk_css_parser_error_syntax (self, "Expected ')' at end of %s()", function_name);
          break;
        }
    }

  gtk_css_parser_end_block (self);

  return result;
}

static GtkCssValue *
gtk_css_color_value_new_from_parse_data (ParseData *data)
{
  if (!data->alpha_specified)
    data->v[3] = 1;

  if (data->ctx.color)
    {
      if (!data->alpha_specified)
        {
          if (data->ctx.color->type == COLOR_TYPE_COLOR)
            {
              data->v[3] = gtk_css_color_value_get_coord (data->ctx.color,
                                                          data->ctx.color_space,
                                                          data->ctx.legacy_rgb_scale,
                                                          3);
              data->values[3] = gtk_css_number_value_new (data->v[3], GTK_CSS_NUMBER);
            }
          else
            {
              data->values[3] = gtk_css_number_value_new_color_component (data->ctx.color,
                                                                          data->ctx.color_space,
                                                                          data->ctx.legacy_rgb_scale,
                                                                          3);
              if (gtk_css_value_is_computed (data->values[3]) &&
                  !gtk_css_value_contains_current_color (data->values[3]))
                data->v[3] = gtk_css_number_value_get_canonical (data->values[3], 1);
              else
                data->v[3] = 0;
            }
        }

      for (guint i = 0; i < 4; i++)
        {
          if (data->values[i] &&
              (!gtk_css_value_is_computed (data->values[i]) ||
                gtk_css_value_contains_current_color (data->values[i])))
            {
              return gtk_css_color_value_new_relative (data->ctx.color,
                                                       data->ctx.color_space,
                                                       data->ctx.legacy_rgb_scale,
                                                       data->values);
            }
        }
    }

  return gtk_css_color_value_new_color (data->ctx.color_space,
                                        data->serialize_as_rgb,
                                        data->v,
                                        (gboolean[4]) {
                                          data->values[0] == NULL && data->v[0] == 0,
                                          data->values[1] == NULL && data->v[1] == 0,
                                          data->values[2] == NULL && data->v[2] == 0,
                                          data->values[3] == NULL && data->v[3] == 0,
                                        });
}

static void
parse_data_clear (ParseData *data)
{
  if (data->ctx.color)
    gtk_css_value_unref (data->ctx.color);

  for (guint i = 0; i < 4; i++)
    {
      if (data->values[i])
        gtk_css_value_unref (data->values[i]);
    }
}

GtkCssValue *
gtk_css_color_value_parse (GtkCssParser *parser)
{
  GtkCssValue *value;
  GdkRGBA rgba;

  if (gtk_css_parser_try_ident (parser, "currentcolor"))
    {
      return gtk_css_color_value_new_current_color ();
    }
  else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_AT_KEYWORD))
    {
      const GtkCssToken *token = gtk_css_parser_get_token (parser);

      gtk_css_parser_warn_deprecated (parser, "@define-color and named colors are deprecated");

      value = gtk_css_color_value_new_name (gtk_css_token_get_string (token));
      gtk_css_parser_consume_token (parser);

      return value;
    }
  else if (gtk_css_parser_has_function (parser, "rgb") || gtk_css_parser_has_function (parser, "rgba"))
    {
      gboolean has_alpha;
      ParseData data = { COLOR_SYNTAX_DETECTING, TRUE, { NULL, }, { 0, }, FALSE, FALSE, { NULL, GTK_CSS_COLOR_SPACE_SRGB, TRUE } };

      has_alpha = gtk_css_parser_has_function (parser, "rgba");

      if (!parse_color_function (parser, FALSE, has_alpha, has_alpha, parse_rgba_color_channel, &data))
        return NULL;

      value = gtk_css_color_value_new_from_parse_data (&data);

      parse_data_clear (&data);

      return value;
    }
  else if (gtk_css_parser_has_function (parser, "hsl") || gtk_css_parser_has_function (parser, "hsla"))
    {
      ParseData data = { COLOR_SYNTAX_DETECTING, TRUE, { NULL, }, { 0, }, FALSE, FALSE, { NULL, GTK_CSS_COLOR_SPACE_HSL, FALSE } };

      if (!parse_color_function (parser, FALSE, TRUE, FALSE, parse_hsla_color_channel, &data))
        return NULL;

      value = gtk_css_color_value_new_from_parse_data (&data);

      parse_data_clear (&data);

      return value;
    }
  else if (gtk_css_parser_has_function (parser, "hwb"))
    {
      ParseData data = { COLOR_SYNTAX_MODERN, TRUE, { NULL, }, { 0, }, FALSE, FALSE, { NULL, GTK_CSS_COLOR_SPACE_HWB, FALSE } };

      if (!parse_color_function (parser, FALSE, TRUE, FALSE, parse_hwb_color_channel, &data))
        return NULL;

      value = gtk_css_color_value_new_from_parse_data (&data);

      parse_data_clear (&data);

      return value;
    }
  else if (gtk_css_parser_has_function (parser, "oklab"))
    {
      ParseData data = { COLOR_SYNTAX_MODERN, FALSE, { NULL, }, { 0, }, FALSE, FALSE, { NULL, GTK_CSS_COLOR_SPACE_OKLAB, FALSE } };

      if (!parse_color_function (parser, FALSE, TRUE, FALSE, parse_oklab_color_channel, &data))
        return NULL;

      value = gtk_css_color_value_new_from_parse_data (&data);

      parse_data_clear (&data);

      return value;
    }
  else if (gtk_css_parser_has_function (parser, "oklch"))
    {
      ParseData data = { COLOR_SYNTAX_MODERN, FALSE, { NULL, }, { 0, }, FALSE, FALSE, { NULL, GTK_CSS_COLOR_SPACE_OKLCH, FALSE } };

      if (!parse_color_function (parser, FALSE, TRUE, FALSE, parse_oklch_color_channel, &data))
        return NULL;

      value = gtk_css_color_value_new_from_parse_data (&data);

      parse_data_clear (&data);

      return value;
    }
  else if (gtk_css_parser_has_function (parser, "color"))
    {
      ParseData data = { COLOR_SYNTAX_MODERN, FALSE, { NULL, }, { 0, }, FALSE, FALSE, { NULL, 0, FALSE } };

      if (!parse_color_function (parser, TRUE, TRUE, FALSE, parse_color_color_channel, &data))
        return NULL;

      value = gtk_css_color_value_new_from_parse_data (&data);

      parse_data_clear (&data);

      return value;
    }
  else if (gtk_css_parser_has_function (parser, "color-mix"))
    {
      ColorMixData data;

      data.color1 = NULL;
      data.color2 = NULL;

      if (gtk_css_parser_consume_function (parser, 3, 3, parse_color_mix, &data))
        {
          value = gtk_css_color_value_new_color_mix (data.color_space,
                                                     data.hue_interpolation,
                                                     data.color1,
                                                     data.color2,
                                                     data.percentage1,
                                                     data.percentage2);
        }
      else
        {
          value = NULL;
        }

      g_clear_pointer (&data.color1, gtk_css_value_unref);
      g_clear_pointer (&data.color2, gtk_css_value_unref);
      return value;
    }
  else if (gtk_css_parser_has_function (parser, "lighter"))
    {
      ColorFunctionData data = { NULL, };

      gtk_css_parser_warn_deprecated (parser, "lighter() is deprecated");

      if (gtk_css_parser_consume_function (parser, 1, 1, parse_color_number, &data))
        value = gtk_css_color_value_new_shade (data.color, 1.3);
      else
        value = NULL;

      g_clear_pointer (&data.color, gtk_css_value_unref);
      return value;
    }
  else if (gtk_css_parser_has_function (parser, "darker"))
    {
      ColorFunctionData data = { NULL, };

      gtk_css_parser_warn_deprecated (parser, "darker() is deprecated");

      if (gtk_css_parser_consume_function (parser, 1, 1, parse_color_number, &data))
        value = gtk_css_color_value_new_shade (data.color, 0.7);
      else
        value = NULL;

      g_clear_pointer (&data.color, gtk_css_value_unref);
      return value;
    }
  else if (gtk_css_parser_has_function (parser, "shade"))
    {
      ColorFunctionData data = { NULL, };

      gtk_css_parser_warn_deprecated (parser, "shade() is deprecated");

      if (gtk_css_parser_consume_function (parser, 2, 2, parse_color_number, &data))
        value = gtk_css_color_value_new_shade (data.color, data.value);
      else
        value = NULL;

      g_clear_pointer (&data.color, gtk_css_value_unref);
      return value;
    }
  else if (gtk_css_parser_has_function (parser, "alpha"))
    {
      ColorFunctionData data = { NULL, };

      gtk_css_parser_warn_deprecated (parser, "alpha() is deprecated");

      if (gtk_css_parser_consume_function (parser, 2, 2, parse_color_number, &data))
        value = gtk_css_color_value_new_alpha (data.color, data.value);
      else
        value = NULL;

      g_clear_pointer (&data.color, gtk_css_value_unref);
      return value;
    }
  else if (gtk_css_parser_has_function (parser, "mix"))
    {
      ColorFunctionData data = { NULL, };

      gtk_css_parser_warn_deprecated (parser, "mix() is deprecated");

      if (gtk_css_parser_consume_function (parser, 3, 3, parse_legacy_mix, &data))
        value = gtk_css_color_value_new_mix (data.color, data.color2, data.value);
      else
        value = NULL;

      g_clear_pointer (&data.color, gtk_css_value_unref);
      g_clear_pointer (&data.color2, gtk_css_value_unref);
      return value;
    }

  if (gdk_rgba_parser_parse (parser, &rgba))
    return gtk_css_color_value_new_color_from_rgba (&rgba);
  else
    return NULL;
}

/* }}} */
/* {{{ Utilities */

const GdkRGBA *
gtk_css_color_value_get_rgba (const GtkCssValue *color)
{
  g_assert (color->class == &GTK_CSS_VALUE_COLOR);
  g_assert (color->type == COLOR_TYPE_COLOR);

  return &color->rgba;
}

const GtkCssColor *
gtk_css_color_value_get_color (const GtkCssValue *color)
{
  g_assert (color->class == &GTK_CSS_VALUE_COLOR);
  g_assert (color->type == COLOR_TYPE_COLOR);

  return &color->color;
}

float
gtk_css_color_value_get_coord (const GtkCssValue *color,
                               GtkCssColorSpace   color_space,
                               gboolean           legacy_rgb_scale,
                               guint              coord)
{
  GtkCssColor origin;

  g_assert (coord < 4);

  if (color->type != COLOR_TYPE_COLOR)
    return 0;

  if (color->type == COLOR_TYPE_COLOR)
    gtk_css_color_init_from_color (&origin, &color->color);
  else
    gtk_css_color_init (&origin, GTK_CSS_COLOR_SPACE_SRGB, (float *) &color->rgba);

  if (color_space != origin.color_space)
    {
      GtkCssColor tmp;
      gtk_css_color_convert (&origin, color_space, &tmp);
      gtk_css_color_init_from_color (&origin, &tmp);
    }

  /* Scale up r, g, b in legacy context */
  if (color_space == GTK_CSS_COLOR_SPACE_SRGB && legacy_rgb_scale && coord < 3)
    return origin.values[coord] * 255.;
  else
    return origin.values[coord];
}

/* }}} */

/* vim:set foldmethod=marker: */
