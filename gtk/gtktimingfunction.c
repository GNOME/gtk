/* gtktimingfunction.c: Timing functions
 *
 * Copyright 2019  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:gtktimingfunction
 * @Title: GtkTimingFunction
 * @Short_description: Easing functions
 *
 * Easing functions represent a curve that control the rate at which
 * an *input value* changes, by taking that input value and producing
 * a corresponding transformed *output progress value*.
 *
 * Easing functions can be used to change the speed of an animation.
 *
 * The *input value* is a real number in the range (-∞, ∞); typically,
 * though, the values are going to be in the [0.0, 1.0] range.
 *
 * The *output progress value* is a real number in the range (-∞, ∞).
 *
 * GTK provides three types of timing functions:
 *
 *  - linear, where the output progress value is set to the input value
 *  - cubic-bezier, where the output progress value is computed along
 *    a Bézier curve with two control points P1 and P2, and whose end
 *    points P0 and P3 are fixed at (0, 0) and (1, 1), respectively
 *  - steps, where the output progress value is computed on a series
 *    of evenly distributed number of steps
 *
 * Additionally, GTK provides convenience easing functions based on
 * the cubic-bezier and steps types, with pre-defined values.
 */

#include "config.h"

#include "gtktimingfunctionprivate.h"

#include <math.h>

static inline gboolean
approx_value (double a,
              double b,
              double epsilon)
{
  double abs_a = fabs (a);
  double abs_b = fabs (b);
  double diff = fabs (a - b);
  double largest = abs_a > abs_b ? abs_a : abs_b;

  if (diff <= epsilon)
    return TRUE;

  return diff <= largest * epsilon;
}

typedef struct {
  double ax, bx, cx;
  double ay, by, cy;
} CubicBezier;

/* Initialize the coefficients */
static inline void
cubic_bezier_init (CubicBezier *b,
                   double       x_1,
                   double       y_1,
                   double       x_2,
                   double       y_2)
{
  b->cx = 3.0 * x_1;
  b->bx = 3.0 * (x_2 - x_1) - b->cx;
  b->ax = 1.0 - b->cx - b->bx;

  b->cy = 3.0 * y_1;
  b->by = 3.0 * (y_2 - y_1) - b->cy;
  b->ay = 1.0 - b->cy - b->by;
}

static inline double
cubic_bezier_sample_curve_x (const CubicBezier *b,
                             double             t)
{
  return ((b->ax * t + b->bx) * t + b->cx) * t;
}

static inline double
cubic_bezier_sample_curve_y (const CubicBezier *b,
                             double             t)
{
  return ((b->ay * t + b->by) * t + b->cy) * t;
}

static inline double
cubic_bezier_sample_curve_derive_x (const CubicBezier *b,
                                    double             t)
{
  return (3.0 * b->ax * t + 2.0 * b->bx) * t + b->cx;
}

static inline double
cubic_bezier_solve_for_x (const CubicBezier *b,
                          double             x,
                          double             epsilon)
{
  double t0, t1, t2;
  double x2;
  double d2;

  t2 = x;
  for (int i = 0; i < 8; i++)
    {
      x2 = cubic_bezier_sample_curve_x (b, t2);
      if (approx_value (x2, x, epsilon))
        return t2;

      d2 = cubic_bezier_sample_curve_derive_x (b, t2);
      if (approx_value (d2, 0.0, 1e-6))
        break;

      t2 = t2 - (x2 - x) / d2;
    }

  t0 = 0.0;
  t1 = 1.0;
  t2 = x;

  if (t2 < t0)
    return t0;
  if (t2 > t1)
    return t1;

  while (t0 < t1)
    {
      x2 = cubic_bezier_sample_curve_x (b, t2);
      if (approx_value (x2, x, epsilon))
        return t2;

      if (x > x2)
        t0 = t2;
      else
        t1 = t2;

      t2 = (t1 - t0) * 0.5 + t0;
    }

  return t2;
}

static inline double
cubic_bezier_solve (const CubicBezier *b,
                    double             x,
                    double             epsilon)
{
  return cubic_bezier_sample_curve_y (b, cubic_bezier_solve_for_x (b, x, epsilon));
}

G_DEFINE_BOXED_TYPE (GtkTimingFunction, gtk_timing_function,
                     gtk_timing_function_ref,
                     gtk_timing_function_unref)

/* The classes of timing functions; cubic-bezier and steps have
 * special cases with known parameters
 */
typedef enum {
  GTK_TIMING_FUNCTION_LINEAR,
  GTK_TIMING_FUNCTION_CUBIC_BEZIER,
  GTK_TIMING_FUNCTION_STEPS
} GtkTimingFunctionType;

typedef struct _GtkTimingFunctionClass  GtkTimingFunctionClass;

typedef struct _GtkCubicBezierFunction  GtkCubicBezierFunction;
typedef struct _GtkStepsFunction        GtkStepsFunction;

struct _GtkTimingFunction
{
  const GtkTimingFunctionClass *function_class;

  GtkTimingFunctionType function_type;
};

struct _GtkTimingFunctionClass
{
  gsize instance_size;

  const char *type_name;

  double (* transform_time) (GtkTimingFunction *self,
                             double             elapsed_time,
                             double             duration);

  void (* print) (GtkTimingFunction *self,
                  GString *buffer);

  gboolean (* equal) (GtkTimingFunction *a,
                      GtkTimingFunction *b);
};

static gpointer
gtk_timing_function_alloc (const GtkTimingFunctionClass *function_class,
                           GtkTimingFunctionType         function_type)
{
  GtkTimingFunction *res;

  g_return_val_if_fail (function_class != NULL, NULL);

  res = g_atomic_rc_box_alloc (function_class->instance_size);
  res->function_class = function_class;
  res->function_type = function_type;

  return res;
}

/**
 * gtk_timing_function_ref:
 * @self: a #GtkTimingFunction
 *
 * Acquires a reference on @self.
 *
 * Returns: (transfer none): the timing function, with its
 *   reference count increased
 */
GtkTimingFunction *
gtk_timing_function_ref (GtkTimingFunction *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return g_atomic_rc_box_acquire (self);
}

/**
 * gtk_timing_function_unref:
 * @self: a #GtkTimingFunction
 *
 * Releases a reference on @self.
 *
 * If the reference released was the last one held by the #GtkTimingFunction,
 * the resources allocated by @self are freed.
 */
void
gtk_timing_function_unref (GtkTimingFunction *self)
{
  g_return_if_fail (self != NULL);

  g_atomic_rc_box_release (self);
}

/**
 * gtk_timing_function_to_string:
 * @self: a #GtkTimingFunction
 *
 * Serializes the given timing function into a string that is
 * suitable for parsing with gtk_timing_function_parse().
 *
 * Returns: (transfer full): the newly created string
 */
char *
gtk_timing_function_to_string (GtkTimingFunction *self)
{
  GString *str;

  str = g_string_new (NULL);
  gtk_timing_function_print (self, str);

  return g_string_free (str, FALSE);
}

/**
 * gtk_timing_function_print:
 * @self: a #GtkTimingFunction
 * @buffer: a #GString
 *
 * Serializes the given timing function into @buffer.
 */
void
gtk_timing_function_print (GtkTimingFunction *self,
                           GString           *buffer)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (buffer != NULL);

  self->function_class->print (self, buffer);
}

gboolean
gtk_timing_function_equal (GtkTimingFunction *a,
                           GtkTimingFunction *b)
{
  if (a == b)
    return TRUE;

  if (a == NULL || b == NULL)
    return FALSE;

  if (a->function_type != b->function_type)
    return FALSE;

  return a->function_class->equal (a, b);
}

/**
 * gtk_timing_function_transform_time:
 * @self: a #GtkTimingFunction
 * @elapsed_time: the elapsed time of the animation
 * @duration: the total duration of the animation
 *
 * Transforms the elapsed time of an animation according to the
 * timing function.
 *
 * Returns: the tranformed time
 */
double
gtk_timing_function_transform_time (GtkTimingFunction *self,
                                    double             elapsed_time,
                                    double             duration)
{
  g_return_val_if_fail (self != NULL, 0.0);

  return self->function_class->transform_time (self, elapsed_time, duration);
}

static guint
gtk_timing_function_parse_float (GtkCssParser *parser,
                                 guint         n,
                                 gpointer      data)
{
  double *p = data;
  double d;

  if (!gtk_css_parser_consume_number (parser, &d))
    return 0;

  /* css-easing-1. §2.2:
   *
   * cubic-bezier(<number>, <number>, <number>, <number>)
   *   Specifies a cubic Bézier easing function. The four numbers specify
   *   points P1 and P2 of the curve as (x1, y1, x2, y2). Both x values
   *   must be in the range [0, 1] or the definition is invalid.
   */
  if (n == 0 || n == 2)
    {
      if (d < 0.0 || d > 1.0)
        {
          gtk_css_parser_error_value (parser, "value %g out of range. Must be between 0.0 and 1.0", d);
          return 0;
        }
    }

  p[n] = d;
  return 1;
}

static guint
gtk_timing_function_parse_steps (GtkCssParser *parser,
                                 guint         n,
                                 gpointer      data)
{
  int *s = data;

  switch (n)
    {
    case 0:
      {
        int d;

        if (!gtk_css_parser_consume_integer (parser, &d))
          return 0;

        if (d < 1)
          {
            gtk_css_parser_error_value (parser, "Number of steps must be a positive integer");
            return 0;
          }

        s[n] = d;
        s[1] = GTK_STEP_POSITION_END;
      }
      return 1;

    case 1:
      {
        const struct {
          const char *ident;
          GtkStepPosition id;
        } positions[] = {
          { "jump-start", GTK_STEP_POSITION_JUMP_START },
          { "jump-end",   GTK_STEP_POSITION_JUMP_END },
          { "jump-none",  GTK_STEP_POSITION_JUMP_NONE },
          { "jump-both",  GTK_STEP_POSITION_JUMP_BOTH },
          { "start",      GTK_STEP_POSITION_START },
          { "end",        GTK_STEP_POSITION_END },
        };
        gboolean found = FALSE;

        char *str = gtk_css_parser_consume_ident (parser);
        if (str == NULL)
          return 0;

        for (int i = 0; i < G_N_ELEMENTS (positions); i++)
          {
            if (strcmp (str, positions[i].ident) == 0)
              {
                s[n] = positions[i].id;
                found = TRUE;
                break;
              }
          }

        if (!found)
          {
            gtk_css_parser_error_syntax (parser, "Invalid position %s; allowed values are: jump-start, jump-end, jump-none, jump-both, start, end", str);
            return 0;
          }
      }
      return 1;

    default:
      g_assert_not_reached ();
      return 0;
    }
}

/*< private >
 * gtk_timing_function_parser_parse:
 * @parser: a #GtkCssParser
 * @out_function: (out) (callee-allocates) (transfer-full) (nullable): the
 *   #GtkTimingFunction
 *
 * Parses the current CSS stream, and creates a new #GtkTimingFunction
 * if one is defined.
 *
 * Returns: %TRUE if the timing function was parsed successfully
 */
gboolean
gtk_timing_function_parser_parse (GtkCssParser       *parser,
                                  GtkTimingFunction **out_function)
{
  static const struct {
    const char *name;
    gboolean is_function;
    gboolean is_bezier;
    gpointer func;
  } tokens[] = {
    { "linear",       FALSE, FALSE, gtk_linear },
    { "ease",         FALSE, TRUE,  gtk_ease },
    { "ease-in",      FALSE, TRUE,  gtk_ease_in },
    { "ease-out",     FALSE, TRUE,  gtk_ease_out },
    { "ease-in-out",  FALSE, TRUE,  gtk_ease_in_out },
    { "step-start",   FALSE, FALSE, gtk_step_start },
    { "step-end",     FALSE, FALSE, gtk_step_end },
    { "cubic-bezier", TRUE,  TRUE,  gtk_cubic_bezier },
    { "steps",        TRUE,  FALSE, gtk_steps },
  };

  const GtkCssToken *token;
  GtkTimingFunction *tm = NULL;

  typedef GtkTimingFunction *(* tm_new) (void);

  token = gtk_css_parser_get_token (parser);

  for (int i = 0; i < G_N_ELEMENTS (tokens); i++)
    {
      if (!tokens[i].is_function)
        {
          if (gtk_css_token_is_ident (token, tokens[i].name))
            {
              tm_new ctor = (tm_new) tokens[i].func;

              gtk_css_parser_consume_token (parser);
              tm = ctor ();
              break;
            }
        }
      else
        {
          if (gtk_css_token_is_function (token, tokens[i].name))
            {
              if (tokens[i].is_bezier)
                {
                  double p[4];

                  if (!gtk_css_parser_consume_function (parser, 4, 4, gtk_timing_function_parse_float, p))
                    goto fail;

                  tm = gtk_cubic_bezier (p[0], p[1], p[2], p[3]);
                  break;
                }
              else
                {
                  int s[2];

                  if (!gtk_css_parser_consume_function (parser, 1, 2, gtk_timing_function_parse_steps, s))
                    goto fail;

                  tm = gtk_steps (s[0], s[1]);
                  break;
                }
            }
        }
    }

  if (tm != NULL)
    {
      if (out_function != NULL)
        *out_function = tm;

      return TRUE;
    }

fail:
  if (out_function != NULL)
    *out_function = NULL;

  return FALSE;
}

/**
 * gtk_timing_function_parse:
 * @string: a string
 * @out_func: (out) (transfer full): a #GtkTimingFunction
 *
 * Parses the contents of @string and creates a new #GtkTimingFunction
 * from them.
 *
 * Returns: %TRUE if the string was successfully parsed, and %FALSE
 *   otherwise
 */
gboolean
gtk_timing_function_parse (const char         *string,
                           GtkTimingFunction **out_func)
{
  GtkCssParser *parser;
  GBytes *bytes;
  gboolean result;

  g_return_val_if_fail (string != NULL, FALSE);
  g_return_val_if_fail (out_func != NULL, FALSE);

  bytes = g_bytes_new_static (string, strlen (string));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL, NULL);

  result = gtk_timing_function_parser_parse (parser, out_func);

  if (result && !gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      g_clear_pointer (out_func, gtk_timing_function_unref);
      result = FALSE;
    }

  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);

  return result;
}

/* linear */
static double
gtk_linear_function_transform_time (GtkTimingFunction *tm,
                                    double             elapsed_time,
                                    double             duration)
{
  return elapsed_time / duration;
}

static void
gtk_linear_function_print (GtkTimingFunction *tm,
                           GString           *buffer)
{
  g_string_append (buffer, "linear");
}

static gboolean
gtk_linear_function_equal (GtkTimingFunction *a,
                           GtkTimingFunction *b)
{
  return TRUE;
}

static const GtkTimingFunctionClass GTK_LINEAR_FUNCTION_CLASS =
{
  sizeof (GtkTimingFunction),
  "GtkLinearFunction",
  gtk_linear_function_transform_time,
  gtk_linear_function_print,
  gtk_linear_function_equal,
};

/**
 * gtk_linear:
 *
 * Creates a new #GtkTimingFunction with a linear progress.
 *
 * Returns: (transfer full): the newly created #GtkTimingFunction
 */
GtkTimingFunction *
gtk_linear (void)
{
  return gtk_timing_function_alloc (&GTK_LINEAR_FUNCTION_CLASS,
                                    GTK_TIMING_FUNCTION_LINEAR);
}

/* cubic-bezier */
struct _GtkCubicBezierFunction
{
  GtkTimingFunction parent;

  double x_1, y_1;
  double x_2, y_2;
};

static double
gtk_cubic_bezier_function_transform_time (GtkTimingFunction *tm,
                                          double             elapsed_time,
                                          double             duration)
{
  GtkCubicBezierFunction *self = (GtkCubicBezierFunction *) tm;
  double progress = elapsed_time / duration;
  double epsilon;
  CubicBezier b;

  /* We need to scale the epsilon to ensure that we get an accurate
   * result with long durations
   */
  epsilon = 1.0 / (1000.0 * duration);

  cubic_bezier_init (&b, self->x_1, self->y_1, self->x_2, self->y_2);

  return cubic_bezier_solve (&b, progress, epsilon);
}

static void
gtk_cubic_bezier_function_print (GtkTimingFunction *tm,
                                 GString           *buffer)
{
  GtkCubicBezierFunction *self = (GtkCubicBezierFunction *) tm;

  g_string_append_printf (buffer,
                          "cubic-bezier(%g,%g,%g,%g)",
                          self->x_1, self->y_1,
                          self->x_2, self->y_2);
}

static gboolean
gtk_cubic_bezier_function_equal (GtkTimingFunction *a,
                                 GtkTimingFunction *b)
{
  GtkCubicBezierFunction *cubic_a = (GtkCubicBezierFunction *) a;
  GtkCubicBezierFunction *cubic_b = (GtkCubicBezierFunction *) b;

  return approx_value (cubic_a->x_1, cubic_b->x_1, 0.0001) &&
         approx_value (cubic_a->y_1, cubic_b->y_1, 0.0001) &&
         approx_value (cubic_a->x_2, cubic_b->x_2, 0.0001) &&
         approx_value (cubic_a->y_2, cubic_b->y_2, 0.0001);
}

static const GtkTimingFunctionClass GTK_CUBIC_BEZIER_FUNCTION_CLASS =
{
  sizeof (GtkCubicBezierFunction),
  "GtkCubicBezierFunction",
  gtk_cubic_bezier_function_transform_time,
  gtk_cubic_bezier_function_print,
  gtk_cubic_bezier_function_equal,
};

/**
 * gtk_cubic_bezier:
 * @x_1: X coordinate of the first control point, between 0 and 1
 * @y_1: Y coordinate of the first control point
 * @x_2: X coordinate of the second control point, between 0 and 1
 * @y_2: Y coordinate of the second control point
 *
 * Creates a new #GtkTimingFunction with a cubic bezier progress.
 *
 * The cubic bezier has an initial point at (0, 0) and a final point
 * at (1, 1); the coordinates passed to the function are the two
 * control points.
 *
 * Returns: (transfer full): the newly created #GtkTimingFunction
 */
GtkTimingFunction *
gtk_cubic_bezier (double x_1,
                  double y_1,
                  double x_2,
                  double y_2)
{
  g_return_val_if_fail (x_1 >= 0.0 && x_1 <= 1.0, NULL);
  g_return_val_if_fail (x_2 >= 0.0 && x_2 <= 1.0, NULL);

  GtkCubicBezierFunction *self = gtk_timing_function_alloc (&GTK_CUBIC_BEZIER_FUNCTION_CLASS,
                                                            GTK_TIMING_FUNCTION_CUBIC_BEZIER);

  self->x_1 = x_1;
  self->y_1 = y_1;
  self->x_2 = x_2;
  self->y_2 = y_2;

  return &self->parent;
}

/* ease */
static void
gtk_ease_function_print (GtkTimingFunction *tm,
                         GString           *buffer)
{
  g_string_append (buffer, "ease");
}

static const GtkTimingFunctionClass GTK_EASE_FUNCTION_CLASS =
{
  sizeof (GtkCubicBezierFunction),
  "GtkEaseFunction",
  gtk_cubic_bezier_function_transform_time,
  gtk_ease_function_print,
  gtk_cubic_bezier_function_equal,
};

/**
 * gtk_ease:
 *
 * Creates a new #GtkTimingFunction with a cubic bezier progress.
 *
 * This function is the equivalent of calling gtk_cubic_bezier() with
 * control point coordinates (0.25, 0.1) and (0.25, 1.0).
 *
 * Returns: (transfer full): the newly created #GtkTimingFunction
 */
GtkTimingFunction *
gtk_ease (void)
{
  GtkCubicBezierFunction *self = gtk_timing_function_alloc (&GTK_EASE_FUNCTION_CLASS,
                                                            GTK_TIMING_FUNCTION_CUBIC_BEZIER);

  self->x_1 = 0.25;
  self->y_1 = 0.1;
  self->x_2 = 0.25;
  self->y_2 = 1.0;

  return &self->parent;
}

/* ease-in */
static void
gtk_ease_in_function_print (GtkTimingFunction *tm,
                            GString           *buffer)
{
  g_string_append (buffer, "ease-in");
}

static const GtkTimingFunctionClass GTK_EASE_IN_FUNCTION_CLASS =
{
  sizeof (GtkCubicBezierFunction),
  "GtkEaseInFunction",
  gtk_cubic_bezier_function_transform_time,
  gtk_ease_in_function_print,
  gtk_cubic_bezier_function_equal,
};

/**
 * gtk_ease_in:
 *
 * Creates a new #GtkTimingFunction with a cubic bezier progress.
 *
 * This function is the equivalent of calling gtk_cubic_bezier() with
 * control point coordinates (0.42, 0.0) and (1.0, 1.0).
 *
 * Returns: (transfer full): the newly created #GtkTimingFunction
 */
GtkTimingFunction *
gtk_ease_in (void)
{
  GtkCubicBezierFunction *self = gtk_timing_function_alloc (&GTK_EASE_IN_FUNCTION_CLASS,
                                                            GTK_TIMING_FUNCTION_CUBIC_BEZIER);

  self->x_1 = 0.42;
  self->y_1 = 0.0;
  self->x_2 = 1.0;
  self->y_2 = 1.0;

  return &self->parent;
}

/* ease-out */
static void
gtk_ease_out_function_print (GtkTimingFunction *tm,
                             GString           *buffer)
{
  g_string_append (buffer, "ease-out");
}

static const GtkTimingFunctionClass GTK_EASE_OUT_FUNCTION_CLASS =
{
  sizeof (GtkCubicBezierFunction),
  "GtkEaseOutFunction",
  gtk_cubic_bezier_function_transform_time,
  gtk_ease_out_function_print,
  gtk_cubic_bezier_function_equal,
};

/**
 * gtk_ease_out:
 *
 * Creates a new #GtkTimingFunction with a cubic bezier progress.
 *
 * This function is the equivalent of calling gtk_cubic_bezier() with
 * control point coordinates (0.0, 0.0) and (0.58, 1.0).
 *
 * Returns: (transfer full): the newly created #GtkTimingFunction
 */
GtkTimingFunction *
gtk_ease_out (void)
{
  GtkCubicBezierFunction *self = gtk_timing_function_alloc (&GTK_EASE_OUT_FUNCTION_CLASS,
                                                            GTK_TIMING_FUNCTION_CUBIC_BEZIER);

  self->x_1 = 0.0;
  self->y_1 = 0.0;
  self->x_2 = 0.58;
  self->y_2 = 1.0;

  return &self->parent;
}

/* ease-in-out */
static void
gtk_ease_in_out_function_print (GtkTimingFunction *tm,
                                GString           *buffer)
{
  g_string_append (buffer, "ease-in-out");
}

static const GtkTimingFunctionClass GTK_EASE_IN_OUT_FUNCTION_CLASS =
{
  sizeof (GtkCubicBezierFunction),
  "GtkEaseInOutFunction",
  gtk_cubic_bezier_function_transform_time,
  gtk_ease_in_out_function_print,
  gtk_cubic_bezier_function_equal,
};

/**
 * gtk_ease_in_out:
 *
 * Creates a new #GtkTimingFunction with a cubic bezier progress.
 *
 * This function is the equivalent of calling gtk_cubic_bezier() with
 * control point coordinates (0.42, 0.0) and (0.58, 1.0).
 *
 * Returns: (transfer full): the newly created #GtkTimingFunction
 */
GtkTimingFunction *
gtk_ease_in_out (void)
{
  GtkCubicBezierFunction *self = gtk_timing_function_alloc (&GTK_EASE_IN_OUT_FUNCTION_CLASS,
                                                            GTK_TIMING_FUNCTION_CUBIC_BEZIER);

  self->x_1 = 0.42;
  self->y_1 = 0.0;
  self->x_2 = 0.58;
  self->y_2 = 1.0;

  return &self->parent;
}

/**
 * gtk_timing_function_get_control_points:
 * @self: a #GtkTimingFunction
 * @x_1: (out) (optional): the X coordinate of the first control point
 * @y_1: (out) (optional): the Y coordinate of the first control point
 * @x_2: (out) (optional): the X coordinate of the second control point
 * @y_2: (out) (optional): the Y coordinate of the second control point
 *
 * Retrieves the coordinates of the control points of a cubic Bézier
 * timing function.
 *
 * Returns: %TRUE if the timing function is a cubic Bézier, and
 *   %FALSE otherwise
 */
gboolean
gtk_timing_function_get_control_points (GtkTimingFunction *self,
                                        double            *x_1,
                                        double            *y_1,
                                        double            *x_2,
                                        double            *y_2)
{
  GtkCubicBezierFunction *cb = (GtkCubicBezierFunction *) self;

  g_return_val_if_fail (self != NULL, FALSE);

  if (self->function_type != GTK_TIMING_FUNCTION_CUBIC_BEZIER)
    return FALSE;

  if (x_1 != NULL)
    *x_1 = cb->x_1;
  if (y_1 != NULL)
    *y_1 = cb->y_1;
  if (x_2 != NULL)
    *x_2 = cb->x_2;
  if (y_2 != NULL)
    *y_2 = cb->y_2;

  return TRUE;
}

/* steps */
struct _GtkStepsFunction
{
  GtkTimingFunction parent;

  int n_steps;
  GtkStepPosition position;
};

static double
gtk_steps_function_transform_time (GtkTimingFunction *tm,
                                   double             elapsed,
                                   double             duration)
{
  GtkStepsFunction *self = (GtkStepsFunction *) tm;
  double progress = elapsed / duration;
  int cur_step = floor ((double) self->n_steps / progress);

  /* jump-start adds a step at 0.0 */
  if (self->position == GTK_STEP_POSITION_JUMP_START ||
      self->position == GTK_STEP_POSITION_JUMP_BOTH)
    cur_step += 1;

  /* jump-end adds a step at 1.0 */
  if (self->position == GTK_STEP_POSITION_JUMP_END ||
      self->position == GTK_STEP_POSITION_JUMP_BOTH)
    cur_step += 1;

  /* clamp the current step between 0 and n_steps */
  if (progress >= 0.0 && cur_step < 0)
    cur_step = 0;
  if (progress <= 1.0 && cur_step > self->n_steps)
    cur_step = self->n_steps;

  return (double) cur_step / (double) self->n_steps;
}

static const char *
position_to_string (GtkStepPosition position)
{
  switch (position)
    {
    case GTK_STEP_POSITION_JUMP_START:
      return "start";

    case GTK_STEP_POSITION_JUMP_END:
      return "end";

    case GTK_STEP_POSITION_JUMP_NONE:
      return "jump-none";

    case GTK_STEP_POSITION_JUMP_BOTH:
      return "jump-both";

    default:
      break;
    }

  return "end";
}

static void
gtk_steps_function_print (GtkTimingFunction *tm,
                          GString           *buffer)
{
  GtkStepsFunction *self = (GtkStepsFunction *) tm;

  if (self->position == GTK_STEP_POSITION_JUMP_END)
    g_string_append_printf (buffer, "steps(%d)", self->n_steps);
  else
    g_string_append_printf (buffer,
                            "steps(%d,%s)",
                            self->n_steps,
                            position_to_string (self->position));
}

static gboolean
gtk_steps_function_equal (GtkTimingFunction *a,
                          GtkTimingFunction *b)
{
  GtkStepsFunction *steps_a = (GtkStepsFunction *) a;
  GtkStepsFunction *steps_b = (GtkStepsFunction *) b;

  return steps_a->n_steps == steps_b->n_steps &&
         steps_a->position == steps_b->position;
}

static const GtkTimingFunctionClass GTK_STEPS_FUNCTION_CLASS =
{
  sizeof (GtkStepsFunction),
  "GtkStepsFunction",
  gtk_steps_function_transform_time,
  gtk_steps_function_print,
  gtk_steps_function_equal,
};

/**
 * gtk_steps:
 * @n_steps: the number of steps, as a positive integer
 * @position: the step position
 *
 * Creates a new #GtkTimingFunction that divides the input into
 * the given @n_steps number of intervals that are equal in length.
 *
 * The @position parameter defines the "step position":
 *
 *  - %GTK_STEP_POSITION_JUMP_START: the first rise occurs when the
 *    input progress is at 0.0
 *  - %GTK_STEP_POSITION_JUMP_END: the last rise occurs when the
 *    input progress is at 1.0
 *  - %GTK_STEP_POSITION_JUMP_NONE: all rises occur in the interval
 *    between 0.0 and 1.0
 *  - %GTK_STEP_POSITION_JUMP_BOTH: the first rise occurse when the
 *    input progress is at 0.0, and the last rise occurs when the
 *    input progress is at 1.0
 *
 * Returns: (transfer full): the newly created #GtkTimingFunction
 */
GtkTimingFunction *
gtk_steps (int             n_steps,
           GtkStepPosition position)
{
  GtkStepsFunction *self = gtk_timing_function_alloc (&GTK_STEPS_FUNCTION_CLASS,
                                                      GTK_TIMING_FUNCTION_STEPS);

  self->n_steps = MAX (1, n_steps);
  self->position = position;

  return &self->parent;
}

/* step-start */
static void
gtk_step_start_function_print (GtkTimingFunction *tm,
                               GString           *buffer)
{
  g_string_append (buffer, "step-start");
}

static const GtkTimingFunctionClass GTK_STEP_START_FUNCTION_CLASS =
{
  sizeof (GtkStepsFunction),
  "GtkStepStartFunction",
  gtk_steps_function_transform_time,
  gtk_step_start_function_print,
  gtk_steps_function_equal,
};

/**
 * gtk_step_start:
 *
 * Creates a new #GtkTimingFunction that divides the input into
 * a specified number of intervals that are equal in length.
 *
 * This function is the equivalent of calling
 *
 * |[
 *   gtk_steps (1, GTK_STEP_POSITION_START)
 * ]|
 *
 * Returns: (transfer full): the newly created #GtkTimingFunction
 */
GtkTimingFunction *
gtk_step_start (void)
{
  GtkStepsFunction *self = gtk_timing_function_alloc (&GTK_STEP_START_FUNCTION_CLASS,
                                                      GTK_TIMING_FUNCTION_STEPS);

  self->n_steps = 1;
  self->position = GTK_STEP_POSITION_JUMP_START;

  return &self->parent;
}

/* step-end */
static void
gtk_step_end_function_print (GtkTimingFunction *tm,
                             GString           *buffer)
{
  g_string_append (buffer, "step-end");
}

static const GtkTimingFunctionClass GTK_STEP_END_FUNCTION_CLASS =
{
  sizeof (GtkStepsFunction),
  "GtkStepEndFunction",
  gtk_steps_function_transform_time,
  gtk_step_end_function_print,
  gtk_steps_function_equal,
};

/**
 * gtk_step_end:
 *
 * Creates a new #GtkTimingFunction that divides the input into
 * a specified number of intervals that are equal in length.
 *
 * This function is the equivalent of calling
 *
 * |[
 *   gtk_steps (1, GTK_STEP_POSITION_END)
 * ]|
 *
 * Returns: (transfer full): the newly created #GtkTimingFunction
 */
GtkTimingFunction *
gtk_step_end (void)
{
  GtkStepsFunction *self = gtk_timing_function_alloc (&GTK_STEP_END_FUNCTION_CLASS,
                                                      GTK_TIMING_FUNCTION_STEPS);

  self->n_steps = 1;
  self->position = GTK_STEP_POSITION_JUMP_END;

  return &self->parent;
}

/**
 * gtk_timing_function_get_steps:
 * @self: a #GtkTimingFunction
 * @n_steps: (out) (optional): the number of steps
 * @position: (out) (optional): the step position
 *
 * Retrieves the number of steps and the step position of a step
 * timing function.
 *
 * Returns: %TRUE if the timing function is a step function, and
 *   %FALSE otherwise
 */
gboolean
gtk_timing_function_get_steps (GtkTimingFunction *self,
                               int               *n_steps,
                               GtkStepPosition   *position)
{
  GtkStepsFunction *s = (GtkStepsFunction *) self;

  g_return_val_if_fail (self != NULL, FALSE);

  if (self->function_type != GTK_TIMING_FUNCTION_STEPS)
    return FALSE;

  if (n_steps != NULL)
    *n_steps = s->n_steps;
  if (position != NULL)
    *position = s->position;

  return TRUE;
}
