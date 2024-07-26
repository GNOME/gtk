/* GTK - The GIMP Toolkit
 * Copyright (C) 2016 Benjamin Otte <otte@gnome.org>
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

#include "gtkcssnumbervalueprivate.h"

#include "gtkcsscalcvalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssdimensionvalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkprivate.h"

#include <fenv.h>

#define RAD_TO_DEG(x) ((x) * 180.0 / G_PI)
#define DEG_TO_RAD(x) ((x) * G_PI / 180.0)

static GtkCssValue * gtk_css_calc_value_alloc           (guint        n_terms);
static GtkCssValue * gtk_css_calc_value_new_sum         (GtkCssValue *a,
                                                         GtkCssValue *b);
static GtkCssValue * gtk_css_round_value_new            (guint        mode,
                                                         GtkCssValue *a,
                                                         GtkCssValue *b);
static GtkCssValue * gtk_css_clamp_value_new            (GtkCssValue *min,
                                                         GtkCssValue *center,
                                                         GtkCssValue *max);

static double _round (guint mode, double a, double b);
static double _mod (double a, double b);
static double _rem (double a, double b);
static double _sign (double a);

typedef enum {
  TYPE_CALC,
  TYPE_DIMENSION,
  TYPE_MIN,
  TYPE_MAX,
  TYPE_CLAMP,
  TYPE_ROUND,
  TYPE_MOD,
  TYPE_REM,
  TYPE_PRODUCT,
  TYPE_ABS,
  TYPE_SIGN,
  TYPE_SIN,
  TYPE_COS,
  TYPE_TAN,
  TYPE_ASIN,
  TYPE_ACOS,
  TYPE_ATAN,
  TYPE_ATAN2,
  TYPE_POW,
  TYPE_SQRT,
  TYPE_EXP,
  TYPE_LOG,
  TYPE_HYPOT,
  TYPE_COLOR_COORD,
} NumberValueType;

static const char *function_name[] = {
    "calc",
    "", /* TYPE_DIMENSION */
    "min",
    "max",
    "clamp",
    "round",
    "mod",
    "rem",
    "", /* TYPE_PRODUCT */
    "abs",
    "sign",
    "sin",
    "cos",
    "tan",
    "asin",
    "acos",
    "atan",
    "atan2",
    "pow",
    "sqrt",
    "exp",
    "log",
    "hypot",
  };

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  guint type : 16;
  union {
    struct {
      GtkCssUnit unit;
      double value;
    } dimension;
    struct {
      guint mode;
      guint n_terms;
      GtkCssValue *terms[1];
    } calc;
    struct {
      GtkCssValue *color;
      GtkCssColorSpace color_space;
      guint coord            : 16;
      guint legacy_rgb_scale :  1;
    } color_coord;
  };
};

static GtkCssValue *
gtk_css_calc_value_new (guint         type,
                        guint         mode,
                        GtkCssValue **values,
                        guint         n_values)
{
  GtkCssValue *result;
  gboolean computed;
  gboolean contains_current_color;

  if (n_values == 1 &&
      (type == TYPE_CALC || type == TYPE_PRODUCT ||
       type == TYPE_MIN || type == TYPE_MAX))
    {
      return values[0];
    }

  result = gtk_css_calc_value_alloc (n_values);
  result->type = type;
  result->calc.mode = mode;

  memcpy (result->calc.terms, values, n_values * sizeof (GtkCssValue *));

  computed = TRUE;
  contains_current_color = FALSE;
  for (int i = 0; i < n_values; i++)
    {
      computed &= gtk_css_value_is_computed (values[i]);
      contains_current_color |= gtk_css_value_contains_current_color (values[i]);
    }

  result->is_computed = computed;
  result->contains_current_color = contains_current_color;

  return result;
}

static void
gtk_css_value_number_free (GtkCssValue *number)
{
  if (number->type == TYPE_COLOR_COORD)
    {
      gtk_css_value_unref (number->color_coord.color);
    }
  else if (number->type != TYPE_DIMENSION)
    {
      for (guint i = 0; i < number->calc.n_terms; i++)
        {
          if (number->calc.terms[i])
            gtk_css_value_unref (number->calc.terms[i]);
        }
    }

  g_free (number);
}

static double
get_dpi (GtkCssStyle *style)
{
  return gtk_css_number_value_get (style->core->dpi, 96);
}

static double
get_base_font_size_px (guint             property_id,
                       GtkStyleProvider *provider,
                       GtkCssStyle      *style,
                       GtkCssStyle      *parent_style)
{
  if (property_id == GTK_CSS_PROPERTY_FONT_SIZE)
    {
      if (parent_style)
        return gtk_css_number_value_get (parent_style->core->font_size, 100);
      else
        return gtk_css_font_size_get_default_px (provider, style);
    }

  return gtk_css_number_value_get (style->core->font_size, 100);
}

/* Canonical units that can be used before compute time
 *
 * Our compatibility is a bit stricter than CSS, since we
 * have a dpi property, so PX and the dpi-dependent units
 * can't be unified before compute time.
 */
static GtkCssUnit
canonical_unit (GtkCssUnit unit)
{
  switch (unit)
    {
    case GTK_CSS_NUMBER:  return GTK_CSS_NUMBER;
    case GTK_CSS_PERCENT: return GTK_CSS_PERCENT;
    case GTK_CSS_PX:      return GTK_CSS_PX;
    case GTK_CSS_EM:      return GTK_CSS_EM;
    case GTK_CSS_EX:      return GTK_CSS_EM;
    case GTK_CSS_REM:     return GTK_CSS_REM;
    case GTK_CSS_PT:      return GTK_CSS_MM;
    case GTK_CSS_PC:      return GTK_CSS_MM;
    case GTK_CSS_IN:      return GTK_CSS_MM;
    case GTK_CSS_CM:      return GTK_CSS_MM;
    case GTK_CSS_MM:      return GTK_CSS_MM;
    case GTK_CSS_RAD:     return GTK_CSS_DEG;
    case GTK_CSS_DEG:     return GTK_CSS_DEG;
    case GTK_CSS_GRAD:    return GTK_CSS_DEG;
    case GTK_CSS_TURN:    return GTK_CSS_DEG;
    case GTK_CSS_S:       return GTK_CSS_S;
    case GTK_CSS_MS:      return GTK_CSS_S;
    default:              g_assert_not_reached ();
    }
}

static inline gboolean
unit_is_compute_time (GtkCssUnit unit)
{
  return gtk_css_unit_get_dimension (unit) == GTK_CSS_DIMENSION_LENGTH &&
         unit != GTK_CSS_PX;
}

static inline gboolean
value_is_compute_time (GtkCssValue *value)
{
  if (value->type == TYPE_DIMENSION)
    return unit_is_compute_time (value->dimension.unit);

  return FALSE;
}

/* Units are compatible if they have the same compute time
 * dependency. This still allows some operations to applied
 * early.
 */
static gboolean
units_compatible (GtkCssValue *v1,
                  GtkCssValue *v2)
{
  if ((v1 && v1->type != TYPE_DIMENSION) || (v2 && v2->type != TYPE_DIMENSION))
    return FALSE;

  if (v1 && v2)
    return canonical_unit (v1->dimension.unit) == canonical_unit (v2->dimension.unit);

  return TRUE;
}

/* Assumes that value is a dimension value and
 * unit is canonical and compatible its unit.
 */
static double
get_converted_value (GtkCssValue *value,
                     GtkCssUnit   unit)
{
  double v;

  if (value->type != TYPE_DIMENSION)
    return NAN;

  v = gtk_css_number_value_get (value, 100);

  if (unit == value->dimension.unit)
    {
      return v;
    }
  else if (unit == GTK_CSS_MM)
    {
      switch ((int) value->dimension.unit)
        {
        case GTK_CSS_PT: return v * 0.35277778;
        case GTK_CSS_PC: return v * 4.2333333;
        case GTK_CSS_IN: return v * 25.4;
        case GTK_CSS_CM: return v * 10;
        default: return NAN;
        }
    }
  else if (unit == GTK_CSS_EM)
    {
      switch ((int) value->dimension.unit)
        {
        case GTK_CSS_EX: return v * 0.5;
        default: return NAN;
        }
    }
  else if (unit == GTK_CSS_DEG)
    {
      switch ((int) value->dimension.unit)
        {
        case GTK_CSS_RAD: return v * 180.0 / G_PI;
        case GTK_CSS_GRAD: return v * 360.0 / 400.0;
        case GTK_CSS_TURN: return v * 360.0;
        default: return NAN;
        }
    }
  else if (unit == GTK_CSS_S)
    {
      switch ((int) value->dimension.unit)
        {
        case GTK_CSS_MS: return v / 1000.0;
        default: return NAN;
        }
    }
  else
    {
      return NAN;
    }
}

static GtkCssValue *
gtk_css_value_number_compute (GtkCssValue          *number,
                              guint                 property_id,
                              GtkCssComputeContext *context)
{
  GtkStyleProvider *provider = context->provider;
  GtkCssStyle *style = context->style;
  GtkCssStyle *parent_style = context->parent_style;

  if (number->type == TYPE_COLOR_COORD)
    {
      GtkCssValue *color, *result;;

      color = gtk_css_value_compute (number->color_coord.color, property_id, context);
      result = gtk_css_number_value_new_color_component (color,
                                                         number->color_coord.color_space,
                                                         number->color_coord.legacy_rgb_scale,
                                                         number->color_coord.coord);
      gtk_css_value_unref (color);
      return result;
    }
  else if (number->type != TYPE_DIMENSION)
    {
      const guint n_terms = number->calc.n_terms;
      GtkCssValue *result;
      GtkCssValue **new_values;

      new_values = g_alloca (sizeof (GtkCssValue *) * n_terms);

      for (gsize i = 0; i < n_terms; i++)
        new_values[i] = gtk_css_value_compute (number->calc.terms[i], property_id, context);

      result = gtk_css_math_value_new (number->type, number->calc.mode, new_values, n_terms);
      result->is_computed = TRUE;

      return result;
    }
  else
    {
      double value = number->dimension.value;

      switch (number->dimension.unit)
        {
        case GTK_CSS_PERCENT:
          /* percentages for font sizes are computed, other percentages aren't */
          if (property_id == GTK_CSS_PROPERTY_FONT_SIZE)
            {
              return gtk_css_dimension_value_new (value / 100.0 *
                                                  get_base_font_size_px (property_id, provider, style, parent_style),
                                                  GTK_CSS_PX);
            }
          G_GNUC_FALLTHROUGH;
        case GTK_CSS_NUMBER:
        case GTK_CSS_PX:
        case GTK_CSS_DEG:
        case GTK_CSS_S:
          return gtk_css_dimension_value_new (value, number->dimension.unit);
        case GTK_CSS_PT:
          return gtk_css_dimension_value_new (value * get_dpi (style) / 72.0, GTK_CSS_PX);
        case GTK_CSS_PC:
          return gtk_css_dimension_value_new (value * get_dpi (style) / 72.0 * 12.0, GTK_CSS_PX);
        case GTK_CSS_IN:
          return gtk_css_dimension_value_new (value * get_dpi (style), GTK_CSS_PX);
        case GTK_CSS_CM:
          return gtk_css_dimension_value_new (value * get_dpi (style) * 0.39370078740157477, GTK_CSS_PX);
        case GTK_CSS_MM:
          return gtk_css_dimension_value_new (value * get_dpi (style) * 0.039370078740157477, GTK_CSS_PX);
        case GTK_CSS_EM:
          return gtk_css_dimension_value_new (value *
                                              get_base_font_size_px (property_id, provider, style, parent_style),
                                              GTK_CSS_PX);
        case GTK_CSS_EX:
          /* for now we pretend ex is half of em */
          return gtk_css_dimension_value_new (value * 0.5 *
                                              get_base_font_size_px (property_id, provider, style, parent_style),
                                              GTK_CSS_PX);
        case GTK_CSS_REM:
          return gtk_css_dimension_value_new (value *
                                              gtk_css_font_size_get_default_px (provider, style),
                                              GTK_CSS_PX);
        case GTK_CSS_RAD:
          return gtk_css_dimension_value_new (value * 360.0 / (2 * G_PI), GTK_CSS_DEG);
        case GTK_CSS_GRAD:
          return gtk_css_dimension_value_new (value * 360.0 / 400.0, GTK_CSS_DEG);
        case GTK_CSS_TURN:
          return gtk_css_dimension_value_new (value * 360.0, GTK_CSS_DEG);
        case GTK_CSS_MS:
          return gtk_css_dimension_value_new (value / 1000.0, GTK_CSS_S);
        default:
          g_assert_not_reached();
        }
    }
}

static gboolean
gtk_css_value_number_equal (const GtkCssValue *val1,
                            const GtkCssValue *val2)
{
  if (val1->type != val2->type)
    return FALSE;

  if (val1->type == TYPE_DIMENSION)
    {
      return val1->dimension.unit == val2->dimension.unit &&
             val1->dimension.value == val2->dimension.value;
    }
  else
    {
      if (val1->calc.n_terms != val2->calc.n_terms)
        return FALSE;

      for (guint i = 0; i < val1->calc.n_terms; i++)
        {
          if (!gtk_css_value_equal (val1->calc.terms[i], val2->calc.terms[i]))
            return FALSE;
        }
    }

  return TRUE;
}

static void
gtk_css_value_number_print (const GtkCssValue *value,
                            GString           *string)
{
  const char *round_modes[] = { "nearest", "up", "down", "to-zero" };

  switch (value->type)
    {
    case TYPE_DIMENSION:
      {
        const char *names[] = {
          /* [GTK_CSS_NUMBER] = */ "",
          /* [GTK_CSS_PERCENT] = */ "%",
          /* [GTK_CSS_PX] = */ "px",
          /* [GTK_CSS_PT] = */ "pt",
          /* [GTK_CSS_EM] = */ "em",
          /* [GTK_CSS_EX] = */ "ex",
          /* [GTK_CSS_REM] = */ "rem",
          /* [GTK_CSS_PC] = */ "pc",
          /* [GTK_CSS_IN] = */ "in",
          /* [GTK_CSS_CM] = */ "cm",
          /* [GTK_CSS_MM] = */ "mm",
          /* [GTK_CSS_RAD] = */ "rad",
          /* [GTK_CSS_DEG] = */ "deg",
          /* [GTK_CSS_GRAD] = */ "grad",
          /* [GTK_CSS_TURN] = */ "turn",
          /* [GTK_CSS_S] = */ "s",
          /* [GTK_CSS_MS] = */ "ms",
        };
        char buf[G_ASCII_DTOSTR_BUF_SIZE];

        if (isinf (value->dimension.value))
          {
            if (value->dimension.value > 0)
              g_string_append (string, "infinite");
            else
              g_string_append (string, "-infinite");
          }
        else if (isnan (value->dimension.value))
          g_string_append (string, "NaN");
        else
          {
            g_ascii_dtostr (buf, sizeof (buf), value->dimension.value);
            g_string_append (string, buf);
            if (value->dimension.value != 0.0)
              g_string_append (string, names[value->dimension.unit]);
          }

        return;
      }

    case TYPE_CLAMP:
      {
        GtkCssValue *min = value->calc.terms[0];
        GtkCssValue *center = value->calc.terms[1];
        GtkCssValue *max = value->calc.terms[2];

        g_string_append (string, function_name[value->type]);
        g_string_append_c (string, '(');
        if (min != NULL)
          gtk_css_value_print (min, string);
        else
          g_string_append (string, "none");
        g_string_append (string, ", ");
        gtk_css_value_print (center, string);
        g_string_append (string, ", ");
        if (max != NULL)
          gtk_css_value_print (max, string);
        else
          g_string_append (string, "none");
        g_string_append_c (string, ')');
      }
      break;

    case TYPE_ROUND:
      g_string_append (string, function_name[value->type]);
      g_string_append_c (string, '(');
      g_string_append (string, round_modes[value->calc.mode]);
      g_string_append (string, ", ");
      gtk_css_value_print (value->calc.terms[0], string);
      if (value->calc.n_terms > 1)
        {
          g_string_append (string, ", ");
          gtk_css_value_print (value->calc.terms[1], string);
        }
      g_string_append_c (string, ')');
      break;

    case TYPE_COLOR_COORD:
      g_string_append (string, gtk_css_color_space_get_coord_name (value->color_coord.color_space, value->color_coord.coord));
      break;

    default:
      {
        const char *sep = value->type == TYPE_CALC ? " + " : (value->type == TYPE_PRODUCT ? " * " : ", ");

        g_string_append (string, function_name[value->type]);
        g_string_append_c (string, '(');
        gtk_css_value_print (value->calc.terms[0], string);
        for (guint i = 1; i < value->calc.n_terms; i++)
          {
            g_string_append (string, sep);
            gtk_css_value_print (value->calc.terms[i], string);
          }
        g_string_append_c (string, ')');
      }
      break;
    }
}

static GtkCssValue *
gtk_css_value_number_transition (GtkCssValue *start,
                                 GtkCssValue *end,
                                 guint        property_id,
                                 double       progress)
{
  GtkCssValue *result, *mul_start, *mul_end;

  if (start == end)
    return gtk_css_value_ref (start);

  if (G_LIKELY (start->type == TYPE_DIMENSION && end->type == TYPE_DIMENSION))
    {
      if (start->dimension.unit == end->dimension.unit)
        {
          const double start_value = start->dimension.value;
          const double end_value = end->dimension.value;
          return gtk_css_dimension_value_new (start_value + (end_value - start_value) * progress,
                                              start->dimension.unit);
        }
    }

  mul_start = gtk_css_number_value_multiply (start, 1 - progress);
  mul_end = gtk_css_number_value_multiply (end, progress);

  result = gtk_css_number_value_add (mul_start, mul_end);

  gtk_css_value_unref (mul_start);
  gtk_css_value_unref (mul_end);

  return result;
}

static GtkCssValue *
gtk_css_value_number_resolve (GtkCssValue          *number,
                              GtkCssComputeContext *context,
                              GtkCssValue          *current)
{
  if (number->type == TYPE_COLOR_COORD)
    {
      GtkCssValue *color, *result;

      color = gtk_css_value_resolve (number->color_coord.color, context, current);
      result = gtk_css_number_value_new_color_component (color,
                                                         number->color_coord.color_space,
                                                         number->color_coord.legacy_rgb_scale,
                                                         number->color_coord.coord);
      gtk_css_value_unref (color);
      return result;
    }
  else if (number->type != TYPE_DIMENSION)
    {
      const guint n_terms = number->calc.n_terms;
      GtkCssValue *result;
      gboolean changed = FALSE;
      GtkCssValue **new_values;

      new_values = g_alloca (sizeof (GtkCssValue *) * n_terms);

      for (gsize i = 0; i < n_terms; i++)
        {
          GtkCssValue *resolved;

          resolved = gtk_css_value_resolve (number->calc.terms[i], context, current);
          changed |= resolved != number->calc.terms[i];
          new_values[i] = resolved ;
        }

      if (changed)
        {
          result = gtk_css_math_value_new (number->type, number->calc.mode, new_values, n_terms);
        }
      else
        {
          for (gsize i = 0; i < n_terms; i++)
            gtk_css_value_unref (new_values[i]);

          result = gtk_css_value_ref (number);
        }

      return result;
    }
  else
    {
      return gtk_css_value_ref (number);
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_NUMBER = {
  "GtkCssNumberValue",
  gtk_css_value_number_free,
  gtk_css_value_number_compute,
  gtk_css_value_number_resolve,
  gtk_css_value_number_equal,
  gtk_css_value_number_transition,
  NULL,
  NULL,
  gtk_css_value_number_print
};

static gsize
gtk_css_value_calc_get_size (gsize n_terms)
{
  g_assert (n_terms > 0);

  return sizeof (GtkCssValue) + sizeof (GtkCssValue *) * (n_terms - 1);
}

static GtkCssValue *
gtk_css_calc_value_alloc (guint n_terms)
{
  GtkCssValue *result;

  result = gtk_css_value_alloc (&GTK_CSS_VALUE_NUMBER,
                                gtk_css_value_calc_get_size (n_terms));
  result->calc.n_terms = n_terms;

  return result;
}

GtkCssValue *
gtk_css_dimension_value_new (double     value,
                             GtkCssUnit unit)
{
  static GtkCssValue number_singletons[] = {
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_NUMBER, 0 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_NUMBER, 1 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_NUMBER, 96 }} }, /* DPI default */
  };
  static GtkCssValue px_singletons[] = {
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_PX, 0 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_PX, 1 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_PX, 2 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_PX, 3 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_PX, 4 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_PX, 5 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_PX, 6 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_PX, 7 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_PX, 8 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_PX, 16 }} }, /* Icon size default */
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_PX, 32 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_PX, 64 }} },
  };
  static GtkCssValue percent_singletons[] = {
    { &GTK_CSS_VALUE_NUMBER, 1, 0, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_PERCENT, 0 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 0, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_PERCENT, 50 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 0, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_PERCENT, 100 }} },
  };
  static GtkCssValue second_singletons[] = {
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_S, 0 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_S, 1 }} },
  };
  static GtkCssValue deg_singletons[] = {
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_DEG, 0 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_DEG, 90 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_DEG, 180 }} },
    { &GTK_CSS_VALUE_NUMBER, 1, 1, 0, 0, TYPE_DIMENSION, {{ GTK_CSS_DEG, 270 }} },
  };
  GtkCssValue *result;

  switch ((guint)unit)
    {
    case GTK_CSS_NUMBER:
      if (value == 0 || value == 1)
        return gtk_css_value_ref (&number_singletons[(int) value]);

      if (value == 96)
        return gtk_css_value_ref (&number_singletons[2]);

      break;

    case GTK_CSS_PX:
      if (value == 0 || value == 1 || value == 2 || value == 3 ||
          value == 4 || value == 5 || value == 6 || value == 7 ||
          value == 8)
        return gtk_css_value_ref (&px_singletons[(int) value]);
      if (value == 16)
        return gtk_css_value_ref (&px_singletons[9]);
      if (value == 32)
        return gtk_css_value_ref (&px_singletons[10]);
      if (value == 64)
        return gtk_css_value_ref (&px_singletons[11]);

      break;

    case GTK_CSS_PERCENT:
      if (value == 0)
        return gtk_css_value_ref (&percent_singletons[0]);
      if (value == 50)
        return gtk_css_value_ref (&percent_singletons[1]);
      if (value == 100)
        return gtk_css_value_ref (&percent_singletons[2]);

      break;

    case GTK_CSS_S:
      if (value == 0 || value == 1)
        return gtk_css_value_ref (&second_singletons[(int)value]);

      break;

    case GTK_CSS_DEG:
      if (value == 0)
        return gtk_css_value_ref (&deg_singletons[0]);
      if (value == 90)
        return gtk_css_value_ref (&deg_singletons[1]);
      if (value == 180)
        return gtk_css_value_ref (&deg_singletons[2]);
      if (value == 270)
        return gtk_css_value_ref (&deg_singletons[3]);

      break;

    default:
      ;
    }

  result = gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_NUMBER);
  result->type = TYPE_DIMENSION;
  result->dimension.unit = unit;
  result->dimension.value = value;
  result->is_computed = value == 0 ||
                        unit == GTK_CSS_NUMBER ||
                        unit == GTK_CSS_PX ||
                        unit == GTK_CSS_DEG ||
                        unit == GTK_CSS_S;
  return result;
}

/*
 * gtk_css_number_value_get_calc_term_order:
 * @value: Value to compute order for
 *
 * Determines the position of @value when printed as part of a calc()
 * expression. Values with lower numbers are printed first. Note that
 * these numbers are arbitrary, so when adding new types of values to
 * print, feel free to change them in implementations so that they
 * match.
 *
 * Returns: Magic value determining placement when printing calc()
 *   expression.
 */
static int
gtk_css_number_value_get_calc_term_order (const GtkCssValue *value)
{
  if (G_LIKELY (value->type == TYPE_DIMENSION))
    {
      /* note: the order is alphabetic */
      static const guint order_per_unit[] = {
        /* [GTK_CSS_NUMBER] = */ 0,
        /* [GTK_CSS_PERCENT] = */ 16,
        /* [GTK_CSS_PX] = */ 11,
        /* [GTK_CSS_PT] = */ 10,
        /* [GTK_CSS_EM] = */ 3,
        /* [GTK_CSS_EX] = */ 4,
        /* [GTK_CSS_REM] = */ 13,
        /* [GTK_CSS_PC] = */ 9,
        /* [GTK_CSS_IN] = */ 6,
        /* [GTK_CSS_CM] = */ 1,
        /* [GTK_CSS_MM] = */ 7,
        /* [GTK_CSS_RAD] = */ 12,
        /* [GTK_CSS_DEG] = */ 2,
        /* [GTK_CSS_GRAD] = */ 5,
        /* [GTK_CSS_TURN] = */ 15,
        /* [GTK_CSS_S] = */ 14,
        /* [GTK_CSS_MS] = */ 8
      };
      return 1000 + order_per_unit[value->dimension.unit];
    }

  /* This should never be needed because calc() can't contain calc(),
   * but eh...
   */
  return 0;
}

static void
gtk_css_calc_array_add (GPtrArray *array, GtkCssValue *value)
{
  gsize i;
  int calc_term_order;

  calc_term_order = gtk_css_number_value_get_calc_term_order (value);

  for (i = 0; i < array->len; i++)
    {
      GtkCssValue *sum = gtk_css_number_value_try_add (g_ptr_array_index (array, i), value);

      if (sum)
        {
          g_ptr_array_index (array, i) = sum;
          gtk_css_value_unref (value);
          return;
        }
      else if (gtk_css_number_value_get_calc_term_order (g_ptr_array_index (array, i)) > calc_term_order)
        {
          g_ptr_array_insert (array, i, value);
          return;
        }
    }

  g_ptr_array_add (array, value);
}

static GtkCssValue *
gtk_css_calc_value_new_sum (GtkCssValue *value1,
                            GtkCssValue *value2)
{
  GPtrArray *array;
  GtkCssValue *result;
  gsize i;

  array = g_ptr_array_new ();

  if (value1->class == &GTK_CSS_VALUE_NUMBER &&
      value1->type == TYPE_CALC)
    {
      for (i = 0; i < value1->calc.n_terms; i++)
        {
          gtk_css_calc_array_add (array, gtk_css_value_ref (value1->calc.terms[i]));
        }
    }
  else
    {
      gtk_css_calc_array_add (array, gtk_css_value_ref (value1));
    }

  if (value2->class == &GTK_CSS_VALUE_NUMBER &&
      value2->type == TYPE_CALC)
    {
      for (i = 0; i < value2->calc.n_terms; i++)
        {
          gtk_css_calc_array_add (array, gtk_css_value_ref (value2->calc.terms[i]));
        }
    }
  else
    {
      gtk_css_calc_array_add (array, gtk_css_value_ref (value2));
    }

  result = gtk_css_math_value_new (TYPE_CALC, 0, (GtkCssValue **)array->pdata, array->len);
  g_ptr_array_free (array, TRUE);

  return result;
}

GtkCssDimension
gtk_css_number_value_get_dimension (const GtkCssValue *value)
{
  switch ((NumberValueType) value->type)
    {
    case TYPE_DIMENSION:
      return gtk_css_unit_get_dimension (value->dimension.unit);

    case TYPE_CALC:
    case TYPE_MIN:
    case TYPE_MAX:
    case TYPE_HYPOT:
    case TYPE_ABS:
    case TYPE_ROUND:
    case TYPE_MOD:
    case TYPE_REM:
    case TYPE_CLAMP:
      {
        GtkCssDimension dimension = GTK_CSS_DIMENSION_PERCENTAGE;

        for (guint i = 0; i < value->calc.n_terms && dimension == GTK_CSS_DIMENSION_PERCENTAGE; i++)
          {
            dimension = gtk_css_number_value_get_dimension (value->calc.terms[i]);
            if (dimension != GTK_CSS_DIMENSION_PERCENTAGE)
              break;
          }
        return dimension;
      }

    case TYPE_PRODUCT:
      if (gtk_css_number_value_get_dimension (value->calc.terms[0]) != GTK_CSS_DIMENSION_NUMBER)
        return gtk_css_number_value_get_dimension (value->calc.terms[0]);
      else
        return gtk_css_number_value_get_dimension (value->calc.terms[1]);

    case TYPE_SIGN:
    case TYPE_SIN:
    case TYPE_COS:
    case TYPE_TAN:
    case TYPE_EXP:
    case TYPE_SQRT:
    case TYPE_POW:
    case TYPE_LOG:
    case TYPE_COLOR_COORD:
      return GTK_CSS_DIMENSION_NUMBER;

    case TYPE_ASIN:
    case TYPE_ACOS:
    case TYPE_ATAN:
    case TYPE_ATAN2:
      return GTK_CSS_DIMENSION_ANGLE;

    default:
      g_assert_not_reached ();
    }
}

gboolean
gtk_css_number_value_has_percent (const GtkCssValue *value)
{
  if (value->type == TYPE_COLOR_COORD)
    {
      return FALSE;
    }
  else if (value->type == TYPE_DIMENSION)
    {
      return gtk_css_unit_get_dimension (value->dimension.unit) == GTK_CSS_DIMENSION_PERCENTAGE;
    }
  else
    {
      for (guint i = 0; i < value->calc.n_terms; i++)
        {
          if (gtk_css_number_value_has_percent (value->calc.terms[i]))
            return TRUE;
        }

      return FALSE;
    }
}

GtkCssValue *
gtk_css_number_value_multiply (GtkCssValue *value,
                               double       factor)
{
  if (factor == 1)
    return gtk_css_value_ref (value);

  switch (value->type)
    {
    case TYPE_DIMENSION:
      return gtk_css_dimension_value_new (value->dimension.value * factor,
                                          value->dimension.unit);

    case TYPE_MIN:
    case TYPE_MAX:
    case TYPE_MOD:
    case TYPE_REM:
      {
        GtkCssValue **values;
        guint type = value->type;

        values = g_new (GtkCssValue *, value->calc.n_terms);
        for (guint i = 0; i < value->calc.n_terms; i++)
          values[i] = gtk_css_number_value_multiply (value->calc.terms[i], factor);

        if (factor < 0)
          {
            if (type == TYPE_MIN)
              type = TYPE_MAX;
            else if (type == TYPE_MAX)
              type = TYPE_MIN;
          }

        return gtk_css_math_value_new (type, 0, values, value->calc.n_terms);
      }

    case TYPE_CALC:
      {
        GtkCssValue *result = gtk_css_calc_value_alloc (value->calc.n_terms);

        result->type = value->type;
        result->calc.mode = value->calc.mode;
        for (guint i = 0; i < value->calc.n_terms; i++)
          result->calc.terms[i] = gtk_css_number_value_multiply (value->calc.terms[i], factor);
        return result;
      }

    case TYPE_PRODUCT:
      {
        GtkCssValue *result = gtk_css_calc_value_alloc (value->calc.n_terms);
        gboolean found = FALSE;

        result->type = value->type;
        result->calc.mode = value->calc.mode;
        for (guint i = 0; i < value->calc.n_terms; i++)
          {
            if (!found &&
                value->calc.terms[i]->type == TYPE_DIMENSION &&
                value->calc.terms[i]->dimension.unit == GTK_CSS_NUMBER)
              {
                result->calc.terms[i] = gtk_css_number_value_multiply (value->calc.terms[i], factor);
                found = TRUE;
              }
            else
              {
                result->calc.terms[i] = gtk_css_value_ref (value->calc.terms[i]);
              }
          }

        if (found)
          return result;

        gtk_css_value_unref (result);
      }
      break;

    case TYPE_ROUND:
      {
        GtkCssValue *a = gtk_css_number_value_multiply (value->calc.terms[0], factor);
        GtkCssValue *b = value->calc.n_terms > 0
                           ? gtk_css_number_value_multiply (value->calc.terms[1], factor)
                           : gtk_css_number_value_new (factor, GTK_CSS_NUMBER);

        return gtk_css_round_value_new (value->calc.mode, a, b);
      }

    case TYPE_CLAMP:
      {
        GtkCssValue *min = value->calc.terms[0];
        GtkCssValue *center = value->calc.terms[1];
        GtkCssValue *max = value->calc.terms[2];

        if (min)
          min = gtk_css_number_value_multiply (min, factor);
        center = gtk_css_number_value_multiply (center, factor);
        if (max)
          max = gtk_css_number_value_multiply (max, factor);

        if (factor < 0)
          {
            GtkCssValue *tmp = min;
            min = max;
            max = tmp;
          }

        return gtk_css_clamp_value_new (min, center, max);
      }

    default:
      break;
    }

  return gtk_css_math_value_new (TYPE_PRODUCT, 0,
                                 (GtkCssValue *[]) {
                                    gtk_css_value_ref (value),
                                    gtk_css_number_value_new (factor, GTK_CSS_NUMBER)
                                 }, 2);
}

GtkCssValue *
gtk_css_number_value_add (GtkCssValue *value1,
                          GtkCssValue *value2)
{
  GtkCssValue *sum;

  sum = gtk_css_number_value_try_add (value1, value2);
  if (sum == NULL)
    sum = gtk_css_calc_value_new_sum (value1, value2);

  return sum;
}

GtkCssValue *
gtk_css_number_value_try_add (GtkCssValue *value1,
                              GtkCssValue *value2)
{
  if (G_UNLIKELY (value1->type != value2->type))
    return NULL;

  if (G_LIKELY (value1->type == TYPE_DIMENSION))
    {
      GtkCssUnit unit = canonical_unit (value1->dimension.unit);
      double v1, v2;

      if (unit != canonical_unit (value2->dimension.unit))
        return NULL;

      if (value1->dimension.value == 0)
        return gtk_css_value_ref (value2);

      if (value2->dimension.value == 0)
        return gtk_css_value_ref (value1);

      v1 = get_converted_value (value1, unit);
      v2 = get_converted_value (value2, unit);

      return gtk_css_dimension_value_new (v1 + v2, unit);
    }

  return NULL;
}

GtkCssValue *
gtk_css_number_value_new (double     value,
                          GtkCssUnit unit)
{
  return gtk_css_dimension_value_new (value, unit);
}

static GtkCssValue *
gtk_css_clamp_value_new (GtkCssValue *min,
                         GtkCssValue *center,
                         GtkCssValue *max)
{
  GtkCssValue *values[] = { min, center, max };
  GtkCssUnit unit;
  double min_, center_, max_, v;

  if (min == NULL && max == NULL)
    return center;

  if (!units_compatible (center, min) || !units_compatible (center, max))
    return gtk_css_calc_value_new (TYPE_CLAMP, 0, values, 3);

  unit = canonical_unit (center->dimension.unit);
  min_ = min ? get_converted_value (min, unit) : -INFINITY;
  center_ = get_converted_value (center, unit);
  max_ = max ? get_converted_value (max, unit) : INFINITY;

  v = CLAMP (center_, min_, max_);

  g_clear_pointer (&min, gtk_css_value_unref);
  g_clear_pointer (&center, gtk_css_value_unref);
  g_clear_pointer (&max, gtk_css_value_unref);

  return gtk_css_dimension_value_new (v, unit);
}

static GtkCssValue *
gtk_css_round_value_new (guint        mode,
                         GtkCssValue *a,
                         GtkCssValue *b)
{
  GtkCssValue *values[2] = { a, b };
  GtkCssUnit unit;
  double a_, b_, v;

  if (!units_compatible (a, b))
    return gtk_css_calc_value_new (TYPE_ROUND, mode, values, b != NULL ? 2 : 1);

  unit = canonical_unit (a->dimension.unit);
  a_ = get_converted_value (a, unit);
  b_ = b ? get_converted_value (b, unit) : 1;

  v = _round (mode, a_, b_);

  gtk_css_value_unref (a);
  gtk_css_value_unref (b);

  return gtk_css_dimension_value_new (v, unit);
}

static GtkCssValue *
gtk_css_minmax_value_new (guint         type,
                          GtkCssValue **values,
                          guint         n_values)
{
  GtkCssValue **vals;
  guint n_vals;

  if (n_values == 1)
    return values[0];

  vals = g_newa (GtkCssValue *, n_values);
  memset (vals, 0, sizeof (GtkCssValue *) * n_values);

  n_vals = 0;

  for (guint i = 0; i < n_values; i++)
    {
      GtkCssValue *value = values[i];

      if (value->type == TYPE_DIMENSION)
        {
          GtkCssUnit unit;
          double v;

          unit = canonical_unit (value->dimension.unit);
          v = get_converted_value (value, unit);

          for (guint j = 0; j < n_vals; j++)
            {
              if ((vals[j]->type == TYPE_DIMENSION) &&
                  (canonical_unit (vals[j]->dimension.unit) == unit))
                {
                  double v1 = get_converted_value (vals[j], unit);

                  if ((type == TYPE_MIN && v < v1) ||
                      (type == TYPE_MAX && v > v1))
                    {
                      gtk_css_value_unref (vals[j]);
                      vals[j] = g_steal_pointer (&value);
                    }
                  else
                    {
                      g_clear_pointer (&value, gtk_css_value_unref);
                    }

                  break;
                }
            }
        }

      if (value)
        {
          vals[n_vals] = value;
          n_vals++;
        }
    }

  return gtk_css_calc_value_new (type, 0, vals, n_vals);
}

static GtkCssValue *
gtk_css_hypot_value_new (GtkCssValue **values,
                         guint         n_values)
{
  GtkCssUnit unit;
  double acc;
  double v;

  for (guint i = 0; i < n_values; i++)
    {
      if (value_is_compute_time (values[i]))
        return gtk_css_calc_value_new (TYPE_HYPOT, 0, values, n_values);
    }

  unit = canonical_unit (values[0]->dimension.unit);
  acc = 0;

  for (guint i = 0; i < n_values; i++)
    {
      double a = get_converted_value (values[i], unit);
      acc += a * a;
    }

  v = sqrt (acc);

  for (guint i = 0; i < n_values; i++)
    gtk_css_value_unref (values[i]);

  return gtk_css_dimension_value_new (v, unit);
}

static GtkCssValue *
gtk_css_arg1_value_new (guint        type,
                        GtkCssValue *value)
{
  GtkCssUnit unit;
  double a;
  double v;

  if (value_is_compute_time (value))
    return gtk_css_calc_value_new (type, 0, &value, 1);

  a = get_converted_value (value, canonical_unit (value->dimension.unit));

  if (type == TYPE_SIN || type == TYPE_COS || type == TYPE_TAN)
    {
      if (gtk_css_unit_get_dimension (value->dimension.unit) == GTK_CSS_DIMENSION_ANGLE)
        a = DEG_TO_RAD (a);
    }

  switch (type)
    {
    case TYPE_SIN: v = sin (a); break;
    case TYPE_COS: v = cos (a); break;
    case TYPE_TAN: v = tan (a); break;
    case TYPE_ASIN: v = asin (a); break;
    case TYPE_ACOS: v = acos (a); break;
    case TYPE_ATAN: v = atan (a); break;
    case TYPE_SQRT: v = sqrt (a); break;
    case TYPE_EXP: v = exp (a); break;
    case TYPE_ABS: v = fabs (a); break;
    case TYPE_SIGN: v = _sign (a); break;
    default: g_assert_not_reached ();
    }

  if (type == TYPE_ASIN || type == TYPE_ACOS || type == TYPE_ATAN)
    {
      unit = GTK_CSS_DEG;
      v = RAD_TO_DEG (v);
    }
  else if (type == TYPE_ABS)
    {
      unit = value->dimension.unit;
    }
  else
    {
      unit = GTK_CSS_NUMBER;
    }

  gtk_css_value_unref (value);

  return gtk_css_dimension_value_new (v, unit);
}

static GtkCssValue *
gtk_css_arg2_value_new (guint        type,
                        GtkCssValue *value1,
                        GtkCssValue *value2)
{
  GtkCssValue *values[2] = { value1, value2 };
  GtkCssUnit unit;
  double a, b = 1;
  double v;

  if (value_is_compute_time (value1) ||
      (value2 && value_is_compute_time (value2)))
    return gtk_css_calc_value_new (type, 0, values, 2);

  a = get_converted_value (value1, canonical_unit (value1->dimension.unit));
  if (value2)
    b = get_converted_value (value2, canonical_unit (value2->dimension.unit));

  switch (type)
    {
    case TYPE_MOD: v = _mod (a, b); break;
    case TYPE_REM: v = _rem (a, b); break;
    case TYPE_ATAN2: v = atan2 (a, b); break;
    case TYPE_POW: v = pow (a, b); break;
    case TYPE_LOG: v = value2 ? log (a) / log (b) : log (a); break;
    default: g_assert_not_reached ();
    }

  if (type == TYPE_ATAN2)
    {
      unit = GTK_CSS_DEG;
      v = RAD_TO_DEG (v);
    }
  else
    {
      unit = GTK_CSS_NUMBER;
    }

  gtk_css_value_unref (value1);
  gtk_css_value_unref (value2);

  return gtk_css_dimension_value_new (v, unit);
}

/* This function is called at parsing time, so units are not
 * canonical, and length values can't necessarily be unified.
 */
GtkCssValue *
gtk_css_math_value_new (guint         type,
                        guint         mode,
                        GtkCssValue **values,
                        guint         n_values)
{
  switch ((NumberValueType) type)
    {
    case TYPE_DIMENSION:
    case TYPE_COLOR_COORD:
      g_assert_not_reached ();

    case TYPE_ROUND:
      return gtk_css_round_value_new (mode, values[0], values[1]);

    case TYPE_CLAMP:
      return gtk_css_clamp_value_new (values[0], values[1], values[2]);

    case TYPE_HYPOT:
      return gtk_css_hypot_value_new (values, n_values);

    case TYPE_MIN:
    case TYPE_MAX:
      return gtk_css_minmax_value_new (type, values, n_values);

    case TYPE_SIN:
    case TYPE_COS:
    case TYPE_TAN:
    case TYPE_ASIN:
    case TYPE_ACOS:
    case TYPE_ATAN:
    case TYPE_SQRT:
    case TYPE_EXP:
    case TYPE_ABS:
    case TYPE_SIGN:
      return gtk_css_arg1_value_new (type, values[0]);

    case TYPE_MOD:
    case TYPE_REM:
    case TYPE_ATAN2:
    case TYPE_POW:
    case TYPE_LOG:
      return gtk_css_arg2_value_new (type, values[0], values[1]);

    case TYPE_PRODUCT:
    case TYPE_CALC:
    default:
      return gtk_css_calc_value_new (type, mode, values, n_values);
    }
}

gboolean
gtk_css_number_value_can_parse (GtkCssParser *parser)
{
  const GtkCssToken *token = gtk_css_parser_get_token (parser);

  switch ((int) token->type)
    {
    case GTK_CSS_TOKEN_SIGNED_NUMBER:
    case GTK_CSS_TOKEN_SIGNLESS_NUMBER:
    case GTK_CSS_TOKEN_SIGNED_INTEGER:
    case GTK_CSS_TOKEN_SIGNLESS_INTEGER:
    case GTK_CSS_TOKEN_PERCENTAGE:
    case GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION:
    case GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION:
    case GTK_CSS_TOKEN_SIGNED_DIMENSION:
    case GTK_CSS_TOKEN_SIGNLESS_DIMENSION:
      return TRUE;

    case GTK_CSS_TOKEN_FUNCTION:
      {
        const char *name = gtk_css_token_get_string (token);

        for (guint i = 0; i < G_N_ELEMENTS (function_name); i++)
          {
            if (g_ascii_strcasecmp (function_name[i], name) == 0)
              return TRUE;
          }
      }
      break;

    default:
      break;
    }

  return FALSE;
}

GtkCssValue *
gtk_css_number_value_parse (GtkCssParser           *parser,
                            GtkCssNumberParseFlags  flags)
{
  GtkCssNumberParseContext ctx = { NULL, 0, FALSE };

  return gtk_css_number_value_parse_with_context (parser, flags, &ctx);
}

GtkCssValue *
gtk_css_number_value_parse_with_context (GtkCssParser             *parser,
                                         GtkCssNumberParseFlags    flags,
                                         GtkCssNumberParseContext *ctx)
{
  const GtkCssToken *token = gtk_css_parser_get_token (parser);

  if (gtk_css_token_is (token, GTK_CSS_TOKEN_FUNCTION))
    {
      const char *name = gtk_css_token_get_string (token);

      if (g_ascii_strcasecmp (name, "calc") == 0)
        return gtk_css_calc_value_parse (parser, flags, ctx);
      else if (g_ascii_strcasecmp (name, "min") == 0)
        return gtk_css_argn_value_parse (parser, flags, ctx, "min", TYPE_MIN);
      else if (g_ascii_strcasecmp (name, "max") == 0)
        return gtk_css_argn_value_parse (parser, flags, ctx, "max", TYPE_MAX);
      else if (g_ascii_strcasecmp (name, "hypot") == 0)
        return gtk_css_argn_value_parse (parser, flags, ctx, "hypot", TYPE_HYPOT);
      else if (g_ascii_strcasecmp (name, "clamp") == 0)
        return gtk_css_clamp_value_parse (parser, flags, ctx, TYPE_CLAMP);
      else if (g_ascii_strcasecmp (name, "round") == 0)
        return gtk_css_round_value_parse (parser, flags, ctx, TYPE_ROUND);
      else if (g_ascii_strcasecmp (name, "mod") == 0)
        return gtk_css_arg2_value_parse (parser, flags, ctx, 2, 2, "mod", TYPE_MOD);
      else if (g_ascii_strcasecmp (name, "rem") == 0)
        return gtk_css_arg2_value_parse (parser, flags, ctx, 2, 2, "rem", TYPE_REM);
      else if (g_ascii_strcasecmp (name, "abs") == 0)
        return gtk_css_arg2_value_parse (parser, flags, ctx, 1, 1, "abs", TYPE_ABS);
      else if ((flags & GTK_CSS_PARSE_NUMBER) && g_ascii_strcasecmp (name, "sign") == 0)
        return gtk_css_arg2_value_parse (parser, GTK_CSS_PARSE_NUMBER|GTK_CSS_PARSE_DIMENSION|GTK_CSS_PARSE_PERCENT, ctx, 1, 1, "sign", TYPE_SIGN);
      else if ((flags & GTK_CSS_PARSE_NUMBER) && g_ascii_strcasecmp (name, "sin") == 0)
        return gtk_css_arg2_value_parse (parser, GTK_CSS_PARSE_NUMBER|GTK_CSS_PARSE_ANGLE, ctx, 1, 1, "sin", TYPE_SIN);
      else if ((flags & GTK_CSS_PARSE_NUMBER) && g_ascii_strcasecmp (name, "cos") == 0)
        return gtk_css_arg2_value_parse (parser, GTK_CSS_PARSE_NUMBER|GTK_CSS_PARSE_ANGLE, ctx, 1, 1, "cos", TYPE_COS);
      else if ((flags & GTK_CSS_PARSE_NUMBER) && g_ascii_strcasecmp (name, "tan") == 0)
        return gtk_css_arg2_value_parse (parser, GTK_CSS_PARSE_NUMBER|GTK_CSS_PARSE_ANGLE, ctx, 1, 1, "tan", TYPE_TAN);
      else if ((flags & GTK_CSS_PARSE_ANGLE) && g_ascii_strcasecmp (name, "asin") == 0)
        return gtk_css_arg2_value_parse (parser, GTK_CSS_PARSE_NUMBER, ctx, 1, 1, "asin", TYPE_ASIN);
      else if ((flags & GTK_CSS_PARSE_ANGLE) && g_ascii_strcasecmp (name, "acos") == 0)
        return gtk_css_arg2_value_parse (parser, GTK_CSS_PARSE_NUMBER, ctx, 1, 1, "acos", TYPE_ACOS);
      else if ((flags & GTK_CSS_PARSE_ANGLE) && g_ascii_strcasecmp (name, "atan") == 0)
        return gtk_css_arg2_value_parse (parser, GTK_CSS_PARSE_NUMBER, ctx, 1, 1, "atan", TYPE_ATAN);
      else if ((flags & GTK_CSS_PARSE_ANGLE) && g_ascii_strcasecmp (name, "atan2") == 0)
        return gtk_css_arg2_value_parse (parser, GTK_CSS_PARSE_NUMBER|GTK_CSS_PARSE_DIMENSION|GTK_CSS_PARSE_PERCENT, ctx, 2, 2, "atan2", TYPE_ATAN2);
      else if ((flags & GTK_CSS_PARSE_NUMBER) && g_ascii_strcasecmp (name, "pow") == 0)
        return gtk_css_arg2_value_parse (parser, GTK_CSS_PARSE_NUMBER, ctx, 2, 2, "pow", TYPE_POW);
      else if ((flags & GTK_CSS_PARSE_NUMBER) && g_ascii_strcasecmp (name, "sqrt") == 0)
        return gtk_css_arg2_value_parse (parser, GTK_CSS_PARSE_NUMBER, ctx, 1, 1, "sqrt", TYPE_SQRT);
      else if ((flags & GTK_CSS_PARSE_NUMBER) && g_ascii_strcasecmp (name, "exp") == 0)
        return gtk_css_arg2_value_parse (parser, GTK_CSS_PARSE_NUMBER, ctx, 1, 1, "exp", TYPE_EXP);
      else if ((flags & GTK_CSS_PARSE_NUMBER) && g_ascii_strcasecmp (name, "log") == 0)
        return gtk_css_arg2_value_parse (parser, GTK_CSS_PARSE_NUMBER, ctx, 1, 2, "log", TYPE_LOG);
    }
  else if (gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
    {
      const char *name = gtk_css_token_get_string (token);
      struct {
        const char *name;
        double value;
      } constants[] = {
        { "e", G_E },
        { "pi", G_PI },
        { "infinity", INFINITY },
        { "-infinity", -INFINITY },
        { "NaN", NAN },
      };

      for (guint i = 0; i < G_N_ELEMENTS (constants); i++)
        {
          if (g_ascii_strcasecmp (name, constants[i].name) == 0)
            {
              gtk_css_parser_consume_token (parser);
              return gtk_css_number_value_new (constants[i].value, GTK_CSS_NUMBER);
            }
        }

      if (ctx->color)
        {
          for (guint i = 0; i < 4; i++)
            {
              if (g_ascii_strcasecmp (name, gtk_css_color_space_get_coord_name (ctx->color_space, i)) == 0)
                {
                  gtk_css_parser_consume_token (parser);
                  return gtk_css_number_value_new_color_component (ctx->color, ctx->color_space, ctx->legacy_rgb_scale, i);
                }
            }
        }
    }

  return gtk_css_dimension_value_parse (parser, flags);
}

/* This function is safe to call on computed values, since all
 * units are canonical and all lengths are in px at that time.
 */
double
gtk_css_number_value_get (const GtkCssValue *value,
                          double             one_hundred_percent)
{
  guint type = value->type;
  guint mode = value->calc.mode;
  GtkCssValue * const *terms = value->calc.terms;
  guint n_terms = value->calc.n_terms;

  switch ((NumberValueType) type)
    {
    case TYPE_DIMENSION:
      if (value->dimension.unit == GTK_CSS_PERCENT)
        return value->dimension.value * one_hundred_percent / 100;
      else
        return value->dimension.value;

    case TYPE_CALC:
      {
        double result = 0.0;

        for (guint i = 0; i < n_terms; i++)
          result += gtk_css_number_value_get (terms[i], one_hundred_percent);

        return result;
      }

    case TYPE_PRODUCT:
      {
        double result = 1.0;

        for (guint i = 0; i < n_terms; i++)
          result *= gtk_css_number_value_get (terms[i], one_hundred_percent);

        return result;
      }

    case TYPE_MIN:
      {
        double result = G_MAXDOUBLE;

        for (guint i = 0; i < n_terms; i++)
          result = MIN (result, gtk_css_number_value_get (terms[i], one_hundred_percent));

        return result;
      }

    case TYPE_MAX:
      {
        double result = -G_MAXDOUBLE;

        for (guint i = 0; i < n_terms; i++)
          result = MAX (result, gtk_css_number_value_get (terms[i], one_hundred_percent));

        return result;
      }

    case TYPE_CLAMP:
      {
        GtkCssValue *min = terms[0];
        GtkCssValue *center = terms[1];
        GtkCssValue *max = terms[2];
        double result;

        result = gtk_css_number_value_get (center, one_hundred_percent);

        if (max)
          result = MIN (result, gtk_css_number_value_get (max, one_hundred_percent));
        if (min)
          result = MAX (result, gtk_css_number_value_get (min, one_hundred_percent));

        return result;
      }

    case TYPE_ROUND:
      {
        double a = gtk_css_number_value_get (terms[0], one_hundred_percent);

        double b = terms[1] != NULL ? gtk_css_number_value_get (terms[1], one_hundred_percent) : 1;

        return _round (mode, a, b);
      }

    case TYPE_MOD:
      {
        double a = gtk_css_number_value_get (terms[0], one_hundred_percent);
        double b = gtk_css_number_value_get (terms[1], one_hundred_percent);

        return _mod (a, b);
      }

    case TYPE_REM:
      {
        double a = gtk_css_number_value_get (terms[0], one_hundred_percent);
        double b = gtk_css_number_value_get (terms[1], one_hundred_percent);

        return _rem (a, b);
      }

    case TYPE_ABS:
      {
        double a = gtk_css_number_value_get (terms[0], one_hundred_percent);

        return fabs (a);
      }

    case TYPE_SIGN:
      {
        double a = gtk_css_number_value_get (terms[0], one_hundred_percent);

        return _sign (a);
      }

    case TYPE_SIN:
      {
        double a = gtk_css_number_value_get (terms[0], one_hundred_percent);

        if (gtk_css_number_value_get_dimension (value) == GTK_CSS_DIMENSION_ANGLE)
          a = DEG_TO_RAD (a);

        return sin (a);
      }

    case TYPE_COS:
      {
        double a = gtk_css_number_value_get (terms[0], one_hundred_percent);

        if (gtk_css_number_value_get_dimension (value) == GTK_CSS_DIMENSION_ANGLE)
          a = DEG_TO_RAD (a);

        return cos (a);
      }

    case TYPE_TAN:
      {
        double a = gtk_css_number_value_get (terms[0], one_hundred_percent);

        if (gtk_css_number_value_get_dimension (value) == GTK_CSS_DIMENSION_ANGLE)
          a = DEG_TO_RAD (a);

        return tan (a);
      }

    case TYPE_ASIN:
      {
        double a = gtk_css_number_value_get (terms[0], one_hundred_percent);

        return RAD_TO_DEG (asin (a));
      }

    case TYPE_ACOS:
      {
        double a = gtk_css_number_value_get (terms[0], one_hundred_percent);

        return RAD_TO_DEG (acos (a));
      }

    case TYPE_ATAN:
      {
        double a = gtk_css_number_value_get (terms[0], one_hundred_percent);

        return RAD_TO_DEG (atan (a));
      }

    case TYPE_ATAN2:
      {
        double a = gtk_css_number_value_get (terms[0], one_hundred_percent);
        double b = gtk_css_number_value_get (terms[1], one_hundred_percent);

        return RAD_TO_DEG (atan2 (a, b));
      }

    case TYPE_POW:
      {
        double a = gtk_css_number_value_get (terms[0], one_hundred_percent);
        double b = gtk_css_number_value_get (terms[1], one_hundred_percent);

        return pow (a, b);
      }

    case TYPE_SQRT:
      {
        double a = gtk_css_number_value_get (terms[0], one_hundred_percent);

        return sqrt (a);
      }

    case TYPE_EXP:
      {
        double a = gtk_css_number_value_get (terms[0], one_hundred_percent);

        return exp (a);
      }

    case TYPE_LOG:
      if (n_terms > 1)
        {
          double a = gtk_css_number_value_get (terms[0], one_hundred_percent);
          double b = gtk_css_number_value_get (terms[1], one_hundred_percent);

          return log (a) / log (b);
        }
      else
        {
          double a = gtk_css_number_value_get (terms[0], one_hundred_percent);

          return log (a);
        }

    case TYPE_HYPOT:
      {
        double acc = 0;

        for (guint i = 0; i < n_terms; i++)
          {
            double a = gtk_css_number_value_get (terms[i], one_hundred_percent);

            acc += a * a;
          }

        return sqrt (acc);
      }

    case TYPE_COLOR_COORD:
      return gtk_css_color_value_get_coord (value->color_coord.color,
                                            value->color_coord.color_space,
                                            value->color_coord.legacy_rgb_scale,
                                            value->color_coord.coord);

    default:
      g_assert_not_reached ();
    }
}

double
gtk_css_number_value_get_canonical (GtkCssValue *number,
                                    double       one_hundred_percent)
{
  if (number->type == TYPE_DIMENSION && number->dimension.unit != GTK_CSS_PERCENT)
    return get_converted_value (number, canonical_unit (number->dimension.unit));

  return gtk_css_number_value_get (number, one_hundred_percent);
}

gboolean
gtk_css_dimension_value_is_zero (const GtkCssValue *value)
{
  g_assert (value != 0);
  g_assert (value->class == &GTK_CSS_VALUE_NUMBER);

  if (value->type != TYPE_DIMENSION)
    return FALSE;

  return value->dimension.value == 0;
}

static double
_round (guint mode, double a, double b)
{
  int old_mode;
  int modes[] = { FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO };
  double result;

  if (b == 0)
    return NAN;

  if (isinf (a))
    {
      if (isinf (b))
        return NAN;
      else
        return a;
    }

  if (isinf (b))
    {
      switch (mode)
        {
        case ROUND_NEAREST:
        case ROUND_TO_ZERO:
          return 0;
        case ROUND_UP:
          return a > 0 ? INFINITY : 0;
        case ROUND_DOWN:
          return a < 0 ? -INFINITY : 0;
        default:
          g_assert_not_reached ();
        }
    }

  old_mode = fegetround ();
  fesetround (modes[mode]);

  result = nearbyint (a/b) * b;

  fesetround (old_mode);

  return result;
}

static double
_mod (double a, double b)
{
  double z;

  if (b == 0 || isinf (a))
    return NAN;

  if (isinf (b) && (a < 0) != (b < 0))
    return NAN;

  z = fmod (a, b);
  if (z < 0)
    z += b;

  return z;
}

static double
_rem (double a, double b)
{
  if (b == 0 || isinf (a))
    return NAN;

  if (isinf (b))
    return a;

  return fmod (a, b);
}

static double
_sign (double a)
{
  if (a < 0)
    return -1;
  else if (a > 0)
    return 1;
  else
    return 0;
}

GtkCssValue *
gtk_css_number_value_new_color_component (GtkCssValue      *color,
                                          GtkCssColorSpace  color_space,
                                          gboolean          legacy_rgb_scale,
                                          guint             coord)
{
  if (gtk_css_value_is_computed (color) &&
      !gtk_css_value_contains_current_color (color))
    {
      float v;

      v = gtk_css_color_value_get_coord (color, color_space, legacy_rgb_scale, coord);

      return gtk_css_number_value_new (v, GTK_CSS_NUMBER);
    }
  else
    {
      GtkCssValue *result;

      result = gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_NUMBER);
      result->type = TYPE_COLOR_COORD;
      result->color_coord.color_space = color_space;
      result->color_coord.color = gtk_css_value_ref (color);
      result->color_coord.coord = coord;
      result->color_coord.legacy_rgb_scale = legacy_rgb_scale;
      result->is_computed = gtk_css_value_is_computed (color);
      result->contains_current_color = gtk_css_value_contains_current_color (color);

      return result;
    }
}
