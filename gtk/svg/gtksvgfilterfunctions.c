/*
 * Copyright © 2025 Red Hat, Inc
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtksvgfilterfunctionsprivate.h"

#include "gtksvgvalueprivate.h"
#include "gtksvgnumberprivate.h"
#include "gtksvgnumbersprivate.h"
#include "gtksvgcolorprivate.h"
#include "gtksvgtransformprivate.h"
#include "gdk/gdkcolorprivate.h"
#include "gdk/gdkrgbaprivate.h"
#include "gtksvgutilsprivate.h"
#include "gtksvgfiltertypeprivate.h"
#include "gtksvgfilterprivate.h"
#include "gtksvgelementinternal.h"

#define DEG_TO_RAD(x) ((x) * G_PI / 180)

#ifndef G_DISABLE_ASSERT
static gboolean
filter_kind_is_simple (FilterKind kind)
{
  return kind != FILTER_REF && kind != FILTER_DROPSHADOW;
}

static gboolean
filter_kind_is_matrix (FilterKind kind)
{
  switch (kind)
    {
    case FILTER_NONE:
    case FILTER_BLUR:
    case FILTER_OPACITY:
      return FALSE;
    case FILTER_BRIGHTNESS:
    case FILTER_CONTRAST:
    case FILTER_GRAYSCALE:
    case FILTER_HUE_ROTATE:
    case FILTER_INVERT:
    case FILTER_SATURATE:
    case FILTER_SEPIA:
      return TRUE;
    case FILTER_REF:
    case FILTER_DROPSHADOW:
    default:
      return FALSE;
    }
}
#endif

typedef unsigned int (* ArgParseFunc) (GtkCssParser *, unsigned int, gpointer);

enum
{
  CLAMPED  = 1 << 0,
  POSITIVE = 1 << 1,
};

static struct {
  FilterKind kind;
  const char *name;
  double default_value;
  unsigned int flags;
  ArgParseFunc parse_arg;
} filter_func_desc[] = {
  { FILTER_NONE,        "none", 0, 0,                     NULL, },
  { FILTER_BLUR,        "blur", 1, POSITIVE,              css_parser_parse_number_length, },
  { FILTER_BRIGHTNESS,  "brightness", 1, POSITIVE,        css_parser_parse_number_percentage, },
  { FILTER_CONTRAST,    "contrast", 1, POSITIVE,          css_parser_parse_number_percentage, },
  { FILTER_GRAYSCALE,   "grayscale", 1, CLAMPED|POSITIVE, css_parser_parse_number_percentage, },
  { FILTER_HUE_ROTATE,  "hue-rotate", 0, 0,               css_parser_parse_number_angle, },
  { FILTER_INVERT,      "invert", 1, CLAMPED|POSITIVE,    css_parser_parse_number_percentage, },
  { FILTER_OPACITY,     "opacity", 1, CLAMPED|POSITIVE,   css_parser_parse_number_percentage, },
  { FILTER_SATURATE,    "saturate", 1, POSITIVE,          css_parser_parse_number_percentage, },
  { FILTER_SEPIA,       "sepia", 1, CLAMPED|POSITIVE,     css_parser_parse_number_percentage, },
  /* FILTER_REF and FILTER_DROPSHADOW are handled separately */
};

typedef struct
{
  FilterKind kind;
  union {
    SvgValue *simple;
    struct {
      char *ref;
      SvgElement *shape;
    } ref;
    struct {
      SvgValue *color;
      SvgValue *dx;
      SvgValue *dy;
      SvgValue *std_dev;
    } dropshadow;
  };
} FilterFunction;

typedef struct {
  SvgValue base;
  unsigned int n_functions;
  FilterFunction functions[1];
} SvgFilterFunctions;

static unsigned int
svg_filter_functions_size (unsigned int n)
{
  return sizeof (SvgFilterFunctions) + (n - 1) * sizeof (FilterFunction);
}

static void
svg_filter_functions_free (SvgValue *value)
{
  SvgFilterFunctions *f = (SvgFilterFunctions *) value;
  for (unsigned int i = 0; i < f->n_functions; i++)
    {
      FilterFunction *ff = &f->functions[i];

      if (ff->kind == FILTER_REF)
        {
          g_free (ff->ref.ref);
        }
      else if (ff->kind == FILTER_DROPSHADOW)
        {
          svg_value_unref (ff->dropshadow.color);
          svg_value_unref (ff->dropshadow.dx);
          svg_value_unref (ff->dropshadow.dy);
          svg_value_unref (ff->dropshadow.std_dev);
        }
      else
        svg_value_unref (ff->simple);
    }

  g_free (value);
}

static gboolean
svg_filter_functions_equal (const SvgValue *value0,
                            const SvgValue *value1)
{
  const SvgFilterFunctions *f0 = (const SvgFilterFunctions *) value0;
  const SvgFilterFunctions *f1 = (const SvgFilterFunctions *) value1;

  if (f0->n_functions != f1->n_functions)
    return FALSE;

  for (unsigned int i = 0; i < f0->n_functions; i++)
    {
      const FilterFunction *ff0 = &f0->functions[i];
      const FilterFunction *ff1 = &f1->functions[i];

      if (ff0->kind != ff1->kind)
        return FALSE;

      if (ff0->kind == FILTER_NONE)
        continue;
      else if (ff0->kind == FILTER_REF)
        {
          if (ff0->ref.shape != ff1->ref.shape ||
              strcmp (ff0->ref.ref, ff1->ref.ref) != 0)
            return FALSE;
        }
      else if (ff0->kind == FILTER_DROPSHADOW)
        {
          if (!svg_value_equal (ff0->dropshadow.color, ff1->dropshadow.color) ||
              !svg_value_equal (ff0->dropshadow.dx, ff1->dropshadow.dx) ||
              !svg_value_equal (ff0->dropshadow.dy, ff1->dropshadow.dy) ||
              !svg_value_equal (ff0->dropshadow.std_dev, ff1->dropshadow.std_dev))
            return FALSE;
        }
      else
        {
          if (!svg_value_equal (ff0->simple, ff1->simple))
            return FALSE;
        }
    }

  return TRUE;
}

static SvgValue *svg_filter_functions_interpolate (const SvgValue    *v0,
                                                   const SvgValue    *v1,
                                                   SvgComputeContext *context,
                                                   double             t);
static SvgValue *svg_filter_functions_accumulate  (const SvgValue    *v0,
                                                   const SvgValue    *v1,
                                                   SvgComputeContext *context,
                                                   int                n);
static void      svg_filter_functions_print       (const SvgValue    *v0,
                                                   GString           *string);

static SvgValue * svg_filter_functions_resolve    (const SvgValue    *value,
                                                   SvgProperty        attr,
                                                   unsigned int       idx,
                                                   SvgElement        *shape,
                                                   SvgComputeContext *context);

static const SvgValueClass SVG_FILTER_FUNCTIONS_CLASS = {
  "SvgFilter",
  svg_filter_functions_free,
  svg_filter_functions_equal,
  svg_filter_functions_interpolate,
  svg_filter_functions_accumulate,
  svg_filter_functions_print,
  svg_value_default_distance,
  svg_filter_functions_resolve,
};

static SvgFilterFunctions *
svg_filter_functions_alloc (unsigned int n)
{
  SvgFilterFunctions *f;

  f = (SvgFilterFunctions *) svg_value_alloc (&SVG_FILTER_FUNCTIONS_CLASS, svg_filter_functions_size (n));
  f->n_functions = n;

  return f;
}

SvgValue *
svg_filter_functions_new_none (void)
{
  static SvgFilterFunctions none = { { &SVG_FILTER_FUNCTIONS_CLASS, 0 }, 1, { { FILTER_NONE, } } };
  return (SvgValue *) &none;
}

static SvgValue *
svg_filter_functions_resolve (const SvgValue    *value,
                              SvgProperty        attr,
                              unsigned int       idx,
                              SvgElement        *shape,
                              SvgComputeContext *context)
{
  const SvgFilterFunctions *filter = (const SvgFilterFunctions *) value;
  SvgFilterFunctions *result;

  if (filter->n_functions == 1 && filter->functions[0].kind == FILTER_NONE)
    return svg_value_ref ((SvgValue *) value);

  result = svg_filter_functions_alloc (filter->n_functions);

  for (unsigned int i = 0; i < filter->n_functions; i++)
    {
      const FilterFunction *ff = &filter->functions[i];
      FilterFunction *r = &result->functions[i];

      r->kind = ff->kind;

      if (ff->kind == FILTER_REF)
        {
          r->ref.ref = g_strdup (ff->ref.ref);
          r->ref.shape = ff->ref.shape;
        }
      else if (ff->kind == FILTER_DROPSHADOW)
        {
          r->dropshadow.color = svg_value_resolve (ff->dropshadow.color, attr, idx, shape, context);
          r->dropshadow.dx = svg_value_resolve (ff->dropshadow.dx, attr, idx, shape, context);
          r->dropshadow.dy = svg_value_resolve (ff->dropshadow.dy, attr, idx, shape, context);
          r->dropshadow.std_dev = svg_value_resolve (ff->dropshadow.std_dev, attr, idx, shape, context);
        }
      else
        {
          SvgValue *v = svg_value_resolve (ff->simple, attr, idx, shape, context);

          if (filter_func_desc[ff->kind].flags & CLAMPED)
            r->simple = svg_number_new_full (svg_number_get_unit (v),
                                             CLAMP (svg_number_get (v, 100), 0, 1));
          else
            r->simple = svg_value_ref (v);
          svg_value_unref (v);
        }
    }

  return (SvgValue *) result;
}

static unsigned int
parse_drop_shadow_arg (GtkCssParser *parser,
                       unsigned int  n,
                       gpointer      data)
{
  SvgValue **vals = data;
  ParserNumber num;
  unsigned int i;

  if (vals[0] == NULL)
    {
      GdkRGBA color;

      if (gtk_css_parser_try_ident (parser, "currentColor"))
        {
          vals[0] = svg_color_new_current ();
          return 1;
        }
      else if (gdk_rgba_parser_parse (parser, &color))
        {
          vals[0] = svg_color_new_rgba (&color);
          return 1;
        }
    }

  if (!css_parser_parse_number_length (parser, 0, &num))
    return 0;

  for (i = 1; vals[i]; i++)
    ;

  g_assert (i < 4);

  vals[i] = svg_number_new_full (num.unit, num.value);
  return 1;
}

SvgValue *
svg_filter_functions_parse_css (GtkCssParser *parser)
{
  SvgFilterFunctions *filter;
  GArray *array;

  if (gtk_css_parser_try_ident (parser, "none"))
    return svg_filter_functions_new_none ();

  array = g_array_new (FALSE, FALSE, sizeof (FilterFunction));

  while (TRUE)
    {
      FilterFunction function;
      unsigned int i;

      memset (&function, 0, sizeof (FilterFunction));

      if (gtk_css_parser_has_url (parser))
        {
          char *url = gtk_css_parser_consume_url (parser);
          if (url == NULL)
            goto fail;

          function.kind = FILTER_REF;
          if (url[0] == '#')
            function.ref.ref = g_strdup (url + 1);
          else
            function.ref.ref = g_strdup (url);
          g_free (url);
        }
      else if (gtk_css_parser_has_function (parser, "drop-shadow"))
        {
          SvgValue *values[4] = { NULL, };

          gtk_css_parser_start_block (parser);
          for (i = 0; i < 4; i++)
            {
              unsigned int parse_args = parse_drop_shadow_arg (parser, i, values);
              if (parse_args == 0)
                break;
            }

          const GtkCssToken *token = gtk_css_parser_get_token (parser);
          if (!gtk_css_token_is (token, GTK_CSS_TOKEN_EOF))
            {
              gtk_css_parser_error_syntax (parser, "Unexpected data at the end of drop-shadow()");
              g_clear_pointer (&values[0], svg_value_unref);
              g_clear_pointer (&values[1], svg_value_unref);
              g_clear_pointer (&values[2], svg_value_unref);
              g_clear_pointer (&values[3], svg_value_unref);

              gtk_css_parser_end_block (parser);

              goto fail;
            }

          gtk_css_parser_end_block (parser);

          if (!values[1] || !values[2])
            {
              gtk_css_parser_error_syntax (parser, "Failed to parse drop-shadow() arguments");
              g_clear_pointer (&values[0], svg_value_unref);
              g_clear_pointer (&values[1], svg_value_unref);
              g_clear_pointer (&values[2], svg_value_unref);
              g_clear_pointer (&values[3], svg_value_unref);

              goto fail;
            }

          if (values[0])
            function.dropshadow.color = values[0];
          else
            function.dropshadow.color = svg_color_new_current ();

          function.dropshadow.dx = values[1];
          function.dropshadow.dy = values[2];

          if (values[3])
            function.dropshadow.std_dev = values[3];
          else
            function.dropshadow.std_dev = svg_number_new (0);

          function.kind = FILTER_DROPSHADOW;
        }
      else
        {
          for (i = 1; i < G_N_ELEMENTS (filter_func_desc); i++)
            {
              if (gtk_css_parser_has_function (parser, filter_func_desc[i].name))
                {
                  ParserNumber n;

                  n.value = filter_func_desc[i].default_value;
                  n.unit = SVG_UNIT_NUMBER;

                  if (!gtk_css_parser_consume_function (parser, 0, 1, filter_func_desc[i].parse_arg, &n))
                    goto fail;

                  if ((filter_func_desc[i].flags & POSITIVE) && n.value < 0)
                    goto fail;

                  function.simple = svg_number_new_full (n.unit, n.value);
                  function.kind = filter_func_desc[i].kind;
                  break;
                }
            }
          if (i == G_N_ELEMENTS (filter_func_desc))
            break;
        }

      g_array_append_val (array, function);
    }

  if (array->len == 0)
    goto fail;

  filter = svg_filter_functions_alloc (array->len);
  memcpy (filter->functions, array->data, sizeof (FilterFunction) * array->len);

  g_array_free (array, TRUE);

  return (SvgValue *) filter;

fail:
  for (unsigned int i = 0; i < array->len; i++)
    {
      FilterFunction *ff = &g_array_index (array, FilterFunction, i);

      if (ff->kind == FILTER_REF)
        g_free (ff->ref.ref);
      else if (ff->kind == FILTER_DROPSHADOW)
        {
          g_clear_pointer (&ff->dropshadow.color, svg_value_unref);
          g_clear_pointer (&ff->dropshadow.dx, svg_value_unref);
          g_clear_pointer (&ff->dropshadow.dy, svg_value_unref);
          g_clear_pointer (&ff->dropshadow.std_dev, svg_value_unref);
        }
      else if (ff->kind != FILTER_NONE)
        g_clear_pointer (&ff->simple, svg_value_unref);
    }
  g_array_free (array, TRUE);
  return NULL;
}

SvgValue *
svg_filter_functions_parse (const char *string)
{
  GtkCssParser *parser = parser_new_for_string (string);
  SvgValue *filter;

  filter = svg_filter_functions_parse_css (parser);

  if (filter && !gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    g_clear_pointer (&filter, svg_value_unref);
  gtk_css_parser_unref (parser);

  return filter;
}

static void
svg_filter_functions_print (const SvgValue *value,
                            GString        *s)
{
  const SvgFilterFunctions *filter = (const SvgFilterFunctions *) value;

  for (unsigned int i = 0; i < filter->n_functions; i++)
    {
      const FilterFunction *f = &filter->functions[i];

      if (i > 0)
        g_string_append_c (s, ' ');

      if (f->kind == FILTER_NONE)
        {
          g_string_append (s, "none");
        }
      else if (f->kind == FILTER_REF)
        {
          g_string_append_printf (s, "url(#%s)", f->ref.ref);
        }
      else if (f->kind == FILTER_DROPSHADOW)
        {
          g_string_append (s, "drop-shadow(");
          if (svg_color_get_kind (f->dropshadow.color) != COLOR_CURRENT)
            {
              svg_value_print (f->dropshadow.color, s);
              g_string_append (s, ", ");
            }
          svg_value_print (f->dropshadow.dx, s);
          g_string_append (s, ", ");
          svg_value_print (f->dropshadow.dy, s);
          g_string_append (s, ", ");
          svg_value_print (f->dropshadow.std_dev, s);
          g_string_append_c (s, ')');
        }
      else
        {
          g_string_append (s, filter_func_desc[f->kind].name);
          g_string_append_c (s, '(');
          svg_value_print (f->simple, s);
          g_string_append_c (s, ')');
        }
    }
}

static SvgValue *
svg_filter_functions_interpolate (const SvgValue    *value0,
                                  const SvgValue    *value1,
                                  SvgComputeContext *context,
                                  double             t)
{
  const SvgFilterFunctions *f0 = (const SvgFilterFunctions *) value0;
  const SvgFilterFunctions *f1 = (const SvgFilterFunctions *) value1;
  SvgFilterFunctions *result;

  if (f0->n_functions != f1->n_functions)
    return NULL;

  /* TODO: filter interpolation wording in the spec */
  for (unsigned int i = 0; i < f0->n_functions; i++)
    {
      const FilterFunction *ff0 = &f0->functions[i];
      const FilterFunction *ff1 = &f1->functions[i];

      if (ff0->kind != ff1->kind)
        return NULL;

      if (ff0->kind == FILTER_REF ||
          ff0->kind == FILTER_DROPSHADOW)
        continue;

      if (svg_number_get_unit (ff0->simple) != svg_number_get_unit (ff1->simple))
        return NULL;
    }

  result = svg_filter_functions_alloc (f0->n_functions);

  for (unsigned int i = 0; i < f0->n_functions; i++)
    {
      const FilterFunction *ff0 = &f0->functions[i];
      const FilterFunction *ff1 = &f1->functions[i];
      FilterFunction *r = &result->functions[i];

      r->kind = ff0->kind;

      if (ff0->kind == FILTER_NONE)
        ;
      else if (ff0->kind == FILTER_REF)
        {
          if (t < 0.5)
            {
              r->ref.ref = g_strdup (ff0->ref.ref);
              r->ref.shape = ff0->ref.shape;
            }
          else
            {
              r->ref.ref = g_strdup (ff1->ref.ref);
              r->ref.shape = ff1->ref.shape;
            }
        }
      else if (ff0->kind == FILTER_DROPSHADOW)
        {
          r->dropshadow.color = svg_value_interpolate (ff0->dropshadow.color, ff1->dropshadow.color, context, t);
          r->dropshadow.dx = svg_value_interpolate (ff0->dropshadow.dx, ff1->dropshadow.dx, context, t);
          r->dropshadow.dy = svg_value_interpolate (ff0->dropshadow.dy, ff1->dropshadow.dy, context, t);
          r->dropshadow.std_dev = svg_value_interpolate (ff0->dropshadow.std_dev, ff1->dropshadow.std_dev, context, t);
        }
      else
        {
          r->simple = svg_value_interpolate (ff0->simple, ff1->simple, context, t);
        }
    }

  return (SvgValue *) result;
}

static SvgValue *
svg_filter_functions_accumulate (const SvgValue    *value0,
                                 const SvgValue    *value1,
                                 SvgComputeContext *context,
                                 int                n)
{
  const SvgFilterFunctions *f0 = (const SvgFilterFunctions *) value0;
  const SvgFilterFunctions *f1 = (const SvgFilterFunctions *) value1;
  SvgFilterFunctions *result;

  result = svg_filter_functions_alloc (f0->n_functions + n * f1->n_functions);

  for (unsigned int i = 0; i < n; i++)
    memcpy (&result->functions[i * f1->n_functions],
            f1->functions,
            f1->n_functions * sizeof (FilterFunction));

  memcpy (&result->functions[n * f1->n_functions],
          f0->functions,
          f0->n_functions * sizeof (FilterFunction));

  for (unsigned int i = 0; i < result->n_functions; i++)
    {
      FilterFunction *r = &result->functions[i];

      if (r->kind == FILTER_NONE)
        ;
      else if (r->kind == FILTER_REF)
        {
          r->ref.ref = g_strdup (r->ref.ref);
        }
      else if (r->kind == FILTER_DROPSHADOW)
        {
          svg_value_ref (r->dropshadow.color);
          svg_value_ref (r->dropshadow.dx);
          svg_value_ref (r->dropshadow.dy);
          svg_value_ref (r->dropshadow.std_dev);
        }
      else
        svg_value_ref (r->simple);
    }

  return (SvgValue *) result;
}

#define R 0.2126
#define G 0.7152
#define B 0.0722

static gboolean
filter_function_get_color_matrix (FilterKind         kind,
                                  double             v,
                                  graphene_matrix_t *matrix,
                                  graphene_vec4_t   *offset)
{
  double c, s;

  switch (kind)
    {
    case FILTER_NONE:
    case FILTER_BLUR:
    case FILTER_REF:
    case FILTER_DROPSHADOW:
      return FALSE;
    case FILTER_BRIGHTNESS:
      graphene_matrix_init_scale (matrix, v, v, v);
      graphene_vec4_init (offset, 0, 0, 0, 0);
      return TRUE;
    case FILTER_CONTRAST:
      graphene_matrix_init_scale (matrix, v, v, v);
      graphene_vec4_init (offset, 0.5 - 0.5 * v, 0.5 - 0.5 * v, 0.5 - 0.5 * v, 0);
      return TRUE;
    case FILTER_GRAYSCALE:
      graphene_matrix_init_from_float (matrix, (float[16]) {
          1 - (1 - R) * v, R * v,               R * v,           0,
          G * v,           1.0 - (1.0 - G) * v, G * v,           0,
          B * v,           B * v,               1 - (1 - B) * v, 0,
          0,               0,                   0,               1  });
      graphene_vec4_init (offset, 0.0, 0.0, 0.0, 0.0);
      return TRUE;
    case FILTER_HUE_ROTATE:
      _sincos (DEG_TO_RAD (v), &s, &c);
      graphene_matrix_init_from_float (matrix, (float[16]) {
          0.213 + 0.787 * c - 0.213 * s,
          0.213 - 0.213 * c + 0.143 * s,
          0.213 - 0.213 * c - 0.787 * s,
          0,
          0.715 - 0.715 * c - 0.715 * s,
          0.715 + 0.285 * c + 0.140 * s,
          0.715 - 0.715 * c + 0.715 * s,
          0,
          0.072 - 0.072 * c + 0.928 * s,
          0.072 - 0.072 * c - 0.283 * s,
          0.072 + 0.928 * c + 0.072 * s,
          0,
          0, 0, 0, 1
          });
        graphene_vec4_init (offset, 0.0, 0.0, 0.0, 0.0);
      return TRUE;
    case FILTER_INVERT:
      graphene_matrix_init_scale (matrix, 1 - 2 * v, 1 - 2 * v, 1 - 2 * v);

      graphene_vec4_init (offset, v, v, v, 0);
      return TRUE;
    case FILTER_OPACITY:
      graphene_matrix_init_from_float (matrix, (float[16]) {
                                       1, 0, 0, 0,
                                       0, 1, 0, 0,
                                       0, 0, 1, 0,
                                       0, 0, 0, v });
      graphene_vec4_init (offset, 0.0, 0.0, 0.0, 0.0);
      return TRUE;
    case FILTER_SATURATE:
      graphene_matrix_init_from_float (matrix, (float[16]) {
          R + (1 - R) * v, R - R * v,       R - R * v,       0,
          G - G * v,       G + (1 - G) * v, G - G * v,       0,
          B - B * v,       B - B * v,       B + (1 - B) * v, 0,
          0,               0,               0,               1 });
      graphene_vec4_init (offset, 0, 0, 0, 0);
      return TRUE;
    case FILTER_SEPIA:
      graphene_matrix_init_from_float (matrix, (float[16]) {
          1 - 0.607 * v, 0.349 * v,     0.272 * v,     0,
          0.769 * v,     1 - 0.314 * v, 0.534 * v,     0,
          0.189 * v,     0.168 * v,     1 - 0.869 * v, 0,
          0,             0,             0,             1 });
      graphene_vec4_init (offset, 0.0, 0.0, 0.0, 0.0);
      return TRUE;
    default:
      g_assert_not_reached ();
    }
}

#undef R
#undef G
#undef B

void
color_matrix_type_get_color_matrix (ColorMatrixType    type,
                                    SvgValue          *values,
                                    graphene_matrix_t *matrix,
                                    graphene_vec4_t   *offset)
{
  switch (type)
    {
    case COLOR_MATRIX_TYPE_MATRIX:
      {
        float m[16];
        float o[4];

        for (unsigned int j = 0; j < 4; j++)
          o[j] = svg_numbers_get (values, 5* j + 4, 1);

        graphene_vec4_init_from_float (offset, o);

        for (unsigned int i = 0; i < 4; i++)
          for (unsigned int j = 0; j < 4; j++)
            m[4*i+j] = svg_numbers_get (values, 5 * j + i, 1);

        graphene_matrix_init_from_float (matrix, m);
      }
      break;
    case COLOR_MATRIX_TYPE_SATURATE:
      filter_function_get_color_matrix (FILTER_SATURATE, svg_numbers_get (values, 0, 1), matrix, offset);
      break;
    case COLOR_MATRIX_TYPE_HUE_ROTATE:
      filter_function_get_color_matrix (FILTER_HUE_ROTATE, svg_numbers_get (values, 0, 1), matrix, offset);
      break;
    case COLOR_MATRIX_TYPE_LUMINANCE_TO_ALPHA:
        graphene_vec4_init (offset, 0, 0, 0, 0);
        graphene_matrix_init_from_float (matrix, (float[16]) {
            0, 0, 0, 0.2126,
            0, 0, 0, 0.7152,
            0, 0, 0, 0.0722,
            0, 0, 0, 0
            });
      break;
    default:
      g_assert_not_reached ();
    }
}

void
svg_filter_functions_get_color_matrix (const SvgValue    *value,
                                       unsigned int       pos,
                                       graphene_matrix_t *matrix,
                                       graphene_vec4_t   *offset)
{
  const SvgFilterFunctions *f = (const SvgFilterFunctions *) value;

  g_assert (value->class == &SVG_FILTER_FUNCTIONS_CLASS);
  g_assert (pos < f->n_functions);
  g_assert (filter_kind_is_matrix (f->functions[pos].kind));

  filter_function_get_color_matrix (f->functions[pos].kind,
                                    svg_number_get (f->functions[pos].simple, 1),
                                    matrix,
                                    offset);
}

gboolean
svg_filter_functions_is_none (const SvgValue *value)
{
  const SvgFilterFunctions *f = (const SvgFilterFunctions *) value;

  g_assert (value->class == &SVG_FILTER_FUNCTIONS_CLASS);

  for (unsigned int i = 0; i < f->n_functions; i++)
    {
      if (f->functions[i].kind != FILTER_NONE)
        return FALSE;
    }

  return TRUE;
}

gboolean
svg_filter_functions_need_backdrop (const SvgValue *value)
{
  const SvgFilterFunctions *f = (const SvgFilterFunctions *) value;

  g_assert (value->class == &SVG_FILTER_FUNCTIONS_CLASS);

  for (unsigned int i = 0; i < f->n_functions; i++)
    {
      if (f->functions[i].kind != FILTER_REF)
        continue;

      if (f->functions[i].ref.shape == NULL)
        continue;

      if (filter_needs_backdrop (f->functions[i].ref.shape))
        return TRUE;
    }

  return FALSE;
}

unsigned int
svg_filter_functions_get_length (const SvgValue *value)
{
  const SvgFilterFunctions *f = (const SvgFilterFunctions *) value;

  g_assert (value->class == &SVG_FILTER_FUNCTIONS_CLASS);

  return f->n_functions;
}

FilterKind
svg_filter_functions_get_kind (const SvgValue *value,
                               unsigned int    pos)
{
  const SvgFilterFunctions *f = (const SvgFilterFunctions *) value;

  g_assert (value->class == &SVG_FILTER_FUNCTIONS_CLASS);
  g_assert (pos < f->n_functions);

  return f->functions[pos].kind;
}

const char *
svg_filter_functions_get_ref (const SvgValue *value,
                              unsigned int    pos)
{
  const SvgFilterFunctions *f = (const SvgFilterFunctions *) value;

  g_assert (value->class == &SVG_FILTER_FUNCTIONS_CLASS);
  g_assert (pos < f->n_functions);
  g_assert (f->functions[pos].kind == FILTER_REF);

  return f->functions[pos].ref.ref;
}

SvgElement *
svg_filter_functions_get_shape (const SvgValue *value,
                                unsigned int    pos)
{
  const SvgFilterFunctions *f = (const SvgFilterFunctions *) value;

  g_assert (value->class == &SVG_FILTER_FUNCTIONS_CLASS);
  g_assert (pos < f->n_functions);
  g_assert (f->functions[pos].kind == FILTER_REF);

  return f->functions[pos].ref.shape;
}

void
svg_filter_functions_set_shape (SvgValue     *value,
                                unsigned int  pos,
                                SvgElement   *shape)
{
  SvgFilterFunctions *f = (SvgFilterFunctions *) value;

  g_assert (value->class == &SVG_FILTER_FUNCTIONS_CLASS);
  g_assert (pos < f->n_functions);
  g_assert (f->functions[pos].kind == FILTER_REF);

  f->functions[pos].ref.shape = shape;
}

double
svg_filter_functions_get_simple (const SvgValue *value,
                                 unsigned int    pos)
{
  const SvgFilterFunctions *f = (const SvgFilterFunctions *) value;

  g_assert (value->class == &SVG_FILTER_FUNCTIONS_CLASS);
  g_assert (pos < f->n_functions);
  g_assert (filter_kind_is_simple (f->functions[pos].kind));

  return svg_number_get (f->functions[pos].simple, 1);
}

void
svg_filter_functions_get_dropshadow (const SvgValue *value,
                                     unsigned int    pos,
                                     GdkColor       *color,
                                     double         *dx,
                                     double         *dy,
                                     double         *std_dev)
{
  const SvgFilterFunctions *f = (const SvgFilterFunctions *) value;

  g_assert (value->class == &SVG_FILTER_FUNCTIONS_CLASS);
  g_assert (pos < f->n_functions);
  g_assert (f->functions[pos].kind == FILTER_DROPSHADOW);

  gdk_color_init_copy (color, svg_color_get_color (f->functions[pos].dropshadow.color));
  *dx = svg_number_get (f->functions[pos].dropshadow.dx, 1);
  *dy = svg_number_get (f->functions[pos].dropshadow.dy, 1);
  *std_dev = svg_number_get (f->functions[pos].dropshadow.std_dev, 1);
}
