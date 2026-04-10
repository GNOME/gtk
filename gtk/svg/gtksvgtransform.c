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

#include "gtksvgtransformprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgnumberprivate.h"
#include "gtksvgstringutilsprivate.h"
#include "gtksvgutilsprivate.h"

#include <tgmath.h>

typedef struct
{
  TransformType type;
  union {
    struct {
      double x, y;
    } translate, scale;
    struct {
      double angle;
      double x, y;
    } rotate;
    struct {
      double angle;
    } skew_x;
    struct {
      double angle;
    } skew_y;
    union {
      struct {
        double xx, yx, xy, yy, dx, dy;
      };
      double m[6];
    } matrix;
    struct {
      double x, y, z;
    } translate3d, scale3d;
    struct {
      double x, y;
    } skew;
    struct {
      double x, y, z;
      double angle;
    } rotate3d;
    struct {
      double depth;
    } perspective;
    struct {
      graphene_matrix_t m;
    } matrix3d;
  };
} PrimitiveTransform;

typedef struct
{
  SvgValue base;
  unsigned int n_transforms;
  PrimitiveTransform transforms[1];
} SvgTransform;

static unsigned int
svg_transform_size (unsigned int n)
{
  return sizeof (SvgTransform) + (n - 1) * sizeof (PrimitiveTransform);
}

static gboolean
primitive_transform_equal (const PrimitiveTransform *t0,
                           const PrimitiveTransform *t1)
{
  if (t0->type != t1->type)
    return FALSE;

  switch (t0->type)
    {
    case TRANSFORM_NONE:
      return TRUE;
    case TRANSFORM_TRANSLATE:
      return t0->translate.x == t1->translate.x &&
             t0->translate.y == t1->translate.y;
    case TRANSFORM_SCALE:
      return t0->scale.x == t1->scale.x &&
             t0->scale.y == t1->scale.y;
    case TRANSFORM_ROTATE:
      return t0->rotate.angle == t1->rotate.angle &&
             t0->rotate.x == t1->rotate.x &&
             t0->rotate.y == t1->rotate.y;
    case TRANSFORM_SKEW_X:
      return t0->skew_x.angle == t1->skew_x.angle;
    case TRANSFORM_SKEW_Y:
      return t0->skew_y.angle == t1->skew_y.angle;
    case TRANSFORM_MATRIX:
      return t0->matrix.xx == t1->matrix.xx && t0->matrix.yx == t1->matrix.yx &&
             t0->matrix.xy == t1->matrix.xy && t0->matrix.yy == t1->matrix.yy &&
             t0->matrix.dx == t1->matrix.dx && t0->matrix.dy == t1->matrix.dy;
    case TRANSFORM_TRANSLATE_3D:
      return t0->translate3d.x == t1->translate3d.x &&
             t0->translate3d.y == t1->translate3d.y &&
             t0->translate3d.z == t1->translate3d.z;
    case TRANSFORM_SCALE_3D:
      return t0->scale3d.x == t1->scale3d.x &&
             t0->scale3d.y == t1->scale3d.y &&
             t0->scale3d.z == t1->scale3d.z;
    case TRANSFORM_ROTATE_3D:
      return t0->rotate3d.angle == t1->rotate3d.angle &&
             t0->rotate3d.x == t1->rotate3d.x &&
             t0->rotate3d.y == t1->rotate3d.y &&
             t0->rotate3d.z == t1->rotate3d.z;
    case TRANSFORM_SKEW:
      return t0->skew.x == t1->skew.x &&
             t0->skew.y == t1->skew.y;
    case TRANSFORM_PERSPECTIVE:
      return t0->perspective.depth == t1->perspective.depth;
    case TRANSFORM_MATRIX_3D:
      return graphene_matrix_equal (&t0->matrix3d.m, &t1->matrix3d.m);
    default:
      g_assert_not_reached ();
    }
}

static gboolean
svg_transform_equal (const SvgValue *value0,
                     const SvgValue *value1)
{
  const SvgTransform *t0 = (const SvgTransform *) value0;
  const SvgTransform *t1 = (const SvgTransform *) value1;

  if (t0->n_transforms != t1->n_transforms)
    return FALSE;

  for (unsigned int i = 0; i < t1->n_transforms; i++)
    {
      if (!primitive_transform_equal (&t0->transforms[i], &t1->transforms[i]))
        return FALSE;
    }

  return TRUE;
}

static SvgValue *svg_transform_interpolate (const SvgValue    *v0,
                                            const SvgValue    *v1,
                                            SvgComputeContext *context,
                                            double             t);
static SvgValue *svg_transform_accumulate  (const SvgValue    *v0,
                                            const SvgValue    *v1,
                                            SvgComputeContext *context,
                                            int                n);
static void      svg_transform_print       (const SvgValue    *v,
                                            GString           *string);
static double    svg_transform_distance    (const SvgValue    *v0,
                                            const SvgValue    *v1);


static const SvgValueClass SVG_TRANSFORM_CLASS = {
  "SvgTransform",
  svg_value_default_free,
  svg_transform_equal,
  svg_transform_interpolate,
  svg_transform_accumulate,
  svg_transform_print,
  svg_transform_distance,
  svg_value_default_resolve,
};

static SvgTransform *
svg_transform_alloc (unsigned int n)
{
  SvgTransform *t;

  t = (SvgTransform *) svg_value_alloc (&SVG_TRANSFORM_CLASS, svg_transform_size (n));
  t->n_transforms = n;

  return t;
}

SvgValue *
svg_transform_new_none (void)
{
  static SvgTransform none = { { &SVG_TRANSFORM_CLASS, 0 }, 1, { { TRANSFORM_NONE, } }};
  return (SvgValue *) &none;
}

SvgValue *
svg_transform_new_translate (double x, double y)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_TRANSLATE;
  tf->transforms[0].translate.x = x;
  tf->transforms[0].translate.y = y;
  return (SvgValue *) tf;
}

SvgValue *
svg_transform_new_scale (double x, double y)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_SCALE;
  tf->transforms[0].scale.x = x;
  tf->transforms[0].scale.y = y;
  return (SvgValue *) tf;
}

SvgValue *
svg_transform_new_rotate (double angle, double x, double y)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_ROTATE;
  tf->transforms[0].rotate.angle = angle;
  tf->transforms[0].rotate.x = x;
  tf->transforms[0].rotate.y = y;
  return (SvgValue *) tf;
}

SvgValue *
svg_transform_new_skew_x (double angle)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_SKEW_X;
  tf->transforms[0].skew_x.angle = angle;
  return (SvgValue *) tf;
}

SvgValue *
svg_transform_new_skew_y (double angle)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_SKEW_Y;
  tf->transforms[0].skew_y.angle = angle;
  return (SvgValue *) tf;
}

SvgValue *
svg_transform_new_matrix (double params[6])
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_MATRIX;
  memcpy (tf->transforms[0].matrix.m, params, sizeof (double) * 6);
  return (SvgValue *) tf;
}

SvgValue *
svg_transform_new_rotate_and_shift (double            angle,
                                    graphene_point_t *orig,
                                    graphene_point_t *final)
{
  SvgTransform *tf = svg_transform_alloc (3);
  tf->transforms[0].type = TRANSFORM_TRANSLATE;
  tf->transforms[0].translate.x = final->x;
  tf->transforms[0].translate.y = final->y;
  tf->transforms[1].type = TRANSFORM_ROTATE;
  tf->transforms[1].rotate.angle = angle;
  tf->transforms[1].rotate.x = 0;
  tf->transforms[1].rotate.y = 0;
  tf->transforms[2].type = TRANSFORM_TRANSLATE;
  tf->transforms[2].translate.x = - orig->x;
  tf->transforms[2].translate.y = - orig->y;
  return (SvgValue *) tf;
}

SvgValue *
svg_transform_new_translate_3d (double x, double y, double z)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_TRANSLATE_3D;
  tf->transforms[0].translate3d.x = x;
  tf->transforms[0].translate3d.y = y;
  tf->transforms[0].translate3d.z = z;
  return (SvgValue *) tf;
}

SvgValue *
svg_transform_new_scale_3d (double x, double y, double z)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_SCALE_3D;
  tf->transforms[0].scale3d.x = x;
  tf->transforms[0].scale3d.y = y;
  tf->transforms[0].scale3d.z = z;
  return (SvgValue *) tf;
}

SvgValue *
svg_transform_new_rotate_3d (double angle,
                             double x, double y, double z)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_ROTATE_3D;
  tf->transforms[0].rotate3d.angle = angle;
  tf->transforms[0].rotate3d.x = x;
  tf->transforms[0].rotate3d.y = y;
  tf->transforms[0].rotate3d.z = z;
  return (SvgValue *) tf;
}

SvgValue *
svg_transform_new_skew (double x, double y)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_SKEW;
  tf->transforms[0].skew.x = x;
  tf->transforms[0].skew.y = y;
  return (SvgValue *) tf;
}

SvgValue *
svg_transform_new_perspective (double depth)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_PERSPECTIVE;
  tf->transforms[0].perspective.depth = depth;
  return (SvgValue *) tf;
}

SvgValue *
svg_transform_new_matrix_3d (const graphene_matrix_t *m)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_MATRIX_3D;
  graphene_matrix_init_from_matrix (&tf->transforms[0].matrix3d.m, m);
  return (SvgValue *) tf;
}

unsigned int
css_parser_parse_number (GtkCssParser *parser,
                         unsigned int  n,
                         gpointer      data)
{
  ParserNumber *val = data;
  double d;

  if (!gtk_css_parser_consume_number (parser, &d))
    return 0;

  val[n].value = d;
  val[n].unit = SVG_UNIT_NUMBER;

  return 1;
}

unsigned int
css_parser_parse_number_length (GtkCssParser *parser,
                                unsigned int  n,
                                gpointer      data)
{
  ParserNumber *val = data;
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (parser);
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_NUMBER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_NUMBER))
    {
      val[n].value = ((const GtkCssNumberToken *) token)->number;
      val[n].unit = SVG_UNIT_NUMBER;
      gtk_css_parser_consume_token (parser);
      return 1;
    }

  if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_DIMENSION) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_DIMENSION))
    {
      for (unsigned int i = 0; i < SVG_UNIT_TURN; i++)
        {
          if (svg_unit_dimension (i) == SVG_DIMENSION_LENGTH &&
              strcmp (svg_unit_name (i), ((const GtkCssDimensionToken *) token)->dimension) == 0)
            {
              val[n].value = ((const GtkCssDimensionToken *) token)->value;
              val[n].unit = i;
              gtk_css_parser_consume_token (parser);
              return TRUE;
            }
        }
    }

  return 0;
}

unsigned int
css_parser_parse_number_angle (GtkCssParser *parser,
                               unsigned int  n,
                               gpointer      data)
{
  ParserNumber *val = data;
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (parser);
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_NUMBER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_NUMBER))
    {
      val[n].value = ((const GtkCssNumberToken *) token)->number;
      val[n].unit = SVG_UNIT_NUMBER;
      gtk_css_parser_consume_token (parser);
      return 1;
    }

  if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_DIMENSION) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_DIMENSION))
    {
      for (unsigned int i = 0; i <= SVG_UNIT_TURN; i++)
        {
          if (svg_unit_dimension (i) == SVG_DIMENSION_ANGLE &&
              strcmp (svg_unit_name (i), ((const GtkCssDimensionToken *) token)->dimension) == 0)
            {
              val[n].value = ((const GtkCssDimensionToken *) token)->value;
              val[n].unit = i;
              gtk_css_parser_consume_token (parser);
              return TRUE;
            }
        }
    }

  return 0;
}

unsigned int
css_parser_parse_number_percentage (GtkCssParser *parser,
                                    unsigned int  n,
                                    gpointer      data)
{
  ParserNumber *val = data;
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (parser);
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_NUMBER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_NUMBER))
    {
      val[n].value = ((const GtkCssNumberToken *) token)->number;
      val[n].unit = SVG_UNIT_NUMBER;
      gtk_css_parser_consume_token (parser);
      return 1;
    }

  if (gtk_css_token_is (token, GTK_CSS_TOKEN_PERCENTAGE))
    {
      val[n].value = ((const GtkCssNumberToken *) token)->number;
      val[n].unit = SVG_UNIT_PERCENTAGE;
      gtk_css_parser_consume_token (parser);
      return 1;
    }

  return 0;
}

static gboolean
parse_transform_function (GtkCssParser *self,
                          unsigned int  min_args,
                          unsigned int  max_args,
                          double       *values)
{
  const GtkCssToken *token;
  gboolean result = FALSE;
  char func[64];
  unsigned int arg;
  ParserNumber *num;

  num = g_newa (ParserNumber, max_args);

  token = gtk_css_parser_get_token (self);
  g_return_val_if_fail (gtk_css_token_is (token, GTK_CSS_TOKEN_FUNCTION), FALSE);

  g_strlcpy (func, gtk_css_token_get_string (token), sizeof (func));
  gtk_css_parser_start_block (self);

  arg = 0;
  while (TRUE)
    {
      unsigned int parse_args;

      if (arg >= max_args)
        {
          gtk_css_parser_error_syntax (self, "Expected ')' at end of %s()", func);
          break;
        }

      parse_args = css_parser_parse_number (self, arg, num);
      if (parse_args == 0)
        break;
      values[arg] = num[arg].value;
      arg += parse_args;
      token = gtk_css_parser_get_token (self);
      if (gtk_css_token_is (token, GTK_CSS_TOKEN_EOF))
        {
          if (arg < min_args)
            {
              gtk_css_parser_error_syntax (self, "%s() requires at least %u arguments", func, min_args);
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
          if (arg >= max_args)
            {
              gtk_css_parser_error_syntax (self, "Expected ')' at end of %s()", func);
              break;
            }

          gtk_css_parser_consume_token (self);
          continue;
        }
      else if (!gtk_css_parser_has_number (self))
        {
          gtk_css_parser_error_syntax (self, "Unexpected data at end of %s()", func);
          break;
        }
    }

  gtk_css_parser_end_block (self);

  return result;
}

SvgValue *
svg_transform_parse_css (GtkCssParser *parser)
{
  SvgTransform *tf;
  GArray *array;

  if (gtk_css_parser_try_ident (parser, "none"))
    return svg_transform_new_none ();

  array = g_array_new (FALSE, FALSE, sizeof (PrimitiveTransform));

    while (TRUE)
    {
      PrimitiveTransform transform;

      memset (&transform, 0, sizeof (PrimitiveTransform));

      if (gtk_css_parser_has_function (parser, "rotate"))
        {
           double values[3] = { 0, 0, 0 };

          if (!parse_transform_function (parser, 1, 3, values))
            goto fail;

          transform.type = TRANSFORM_ROTATE;
          transform.rotate.angle = values[0];
          transform.rotate.x = values[1];
          transform.rotate.y = values[2];
        }
      else if (gtk_css_parser_has_function (parser, "scale"))
        {
          double values[2] = { 0, 0 };

          if (!parse_transform_function (parser, 1, 2, values))
            goto fail;

          transform.type = TRANSFORM_SCALE;
          transform.scale.x = values[0];
          if (values[1])
            transform.scale.y = values[1];
          else
            transform.scale.y = transform.scale.x;
        }
      else if (gtk_css_parser_has_function (parser, "translate"))
        {
          double values[2] = { 0, 0 };

          if (!parse_transform_function (parser, 1, 2, values))
            goto fail;

          transform.type = TRANSFORM_TRANSLATE;
          transform.translate.x = values[0];
          if (values[1])
            transform.translate.y = values[1];
          else
            transform.translate.y = 0;
        }
      else if (gtk_css_parser_has_function (parser, "skewX"))
        {
          double values[1];

          if (!parse_transform_function (parser, 1, 1, values))
            goto fail;

          transform.type = TRANSFORM_SKEW_X;
          transform.skew_x.angle = values[0];
        }
      else if (gtk_css_parser_has_function (parser, "skewY"))
        {
          double values[1];

          if (!parse_transform_function (parser, 1, 1, values))
            goto fail;

          transform.type = TRANSFORM_SKEW_Y;
          transform.skew_y.angle = values[0];
        }
      else if (gtk_css_parser_has_function (parser, "matrix"))
        {
          double values[6];

          if (!parse_transform_function (parser, 6, 6, values))
            goto fail;

          transform.type = TRANSFORM_MATRIX;
          memcpy (transform.matrix.m, values, sizeof (double) * 6);
        }
      else if (gtk_css_parser_has_function (parser, "translate3d"))
        {
          double values[3];

          if (!parse_transform_function (parser, 3, 3, values))
            goto fail;

          transform.type = TRANSFORM_TRANSLATE_3D;
          transform.translate3d.x = values[0];
          transform.translate3d.y = values[1];
          transform.translate3d.z = values[2];
        }
      else if (gtk_css_parser_has_function (parser, "scale3d"))
        {
          double values[3];

          if (!parse_transform_function (parser, 3, 3, values))
            goto fail;

          transform.type = TRANSFORM_SCALE_3D;
          transform.scale3d.x = values[0];
          transform.scale3d.y = values[1];
          transform.scale3d.z = values[2];
        }
      else if (gtk_css_parser_has_function (parser, "rotate3d"))
        {
          double values[4];

          if (!parse_transform_function (parser, 4, 4, values))
            goto fail;

          transform.type = TRANSFORM_ROTATE_3D;
          transform.rotate3d.x = values[0];
          transform.rotate3d.y = values[1];
          transform.rotate3d.z = values[2];
          transform.rotate3d.angle = values[3];
        }
      else if (gtk_css_parser_has_function (parser, "skew"))
        {
          double values[2];

          if (!parse_transform_function (parser, 2, 2, values))
            goto fail;

          transform.type = TRANSFORM_SKEW;
          transform.skew.x = values[0];
          transform.skew.y = values[1];
        }
      else if (gtk_css_parser_has_function (parser, "perspective"))
        {
          double values[1];

          if (!parse_transform_function (parser, 1, 1, values))
            goto fail;

          transform.type = TRANSFORM_PERSPECTIVE;
          transform.perspective.depth = values[0];
        }
      else if (gtk_css_parser_has_function (parser, "matrix3d"))
        {
          double values[16];
          float floats[16];

          if (!parse_transform_function (parser, 16, 16, values))
            goto fail;

          transform.type = TRANSFORM_MATRIX_3D;
          for (unsigned int i = 0; i < 16; i++)
            floats[i] = values[i];
          graphene_matrix_init_from_float (&transform.matrix3d.m, floats);
        }
      else
        {
          break;
        }

      g_array_append_val (array, transform);

      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COMMA))
        gtk_css_parser_skip (parser);
    }

  if (array->len == 0)
    goto fail;

  tf = svg_transform_alloc (array->len);
  memcpy (tf->transforms, array->data, sizeof (PrimitiveTransform) * array->len);

  g_array_free (array, TRUE);

  return (SvgValue *) tf;

fail:
  g_array_free (array, TRUE);
  gtk_css_parser_error_syntax (parser, "Expected a transform");
  return NULL;
}

SvgValue *
svg_transform_parse (const char *value)
{
  GtkCssParser *parser = parser_new_for_string (value);
  SvgValue *tf;

  tf = svg_transform_parse_css (parser);

  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    g_clear_pointer (&tf, svg_value_unref);

  gtk_css_parser_unref (parser);

  return tf;
}

SvgValue *
primitive_transform_parse (TransformType  type,
                           const char    *string)
{
  GtkCssParser *parser = parser_new_for_string (string);
  double angle, x, y;
  SvgUnit u;
  SvgValue *value = NULL;

  gtk_css_parser_skip_whitespace (parser);
  switch ((unsigned int) type)
    {
    case TRANSFORM_TRANSLATE:
      if (svg_number_parse2 (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER, &x, &u))
        {
          skip_whitespace_and_optional_comma (parser);
          if (svg_number_parse2 (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER, &y, &u))
            value = svg_transform_new_translate (x, y);
          else
            value = svg_transform_new_translate (x, 0);
        }
      break;

    case TRANSFORM_SCALE:
      if (svg_number_parse2 (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER, &x, &u))
        {
          skip_whitespace_and_optional_comma (parser);
          if (svg_number_parse2 (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER, &y, &u))
            value = svg_transform_new_scale (x, y);
          else
            value = svg_transform_new_scale (x, x);
        }
      break;

    case TRANSFORM_ROTATE:
      if (svg_number_parse2 (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER, &angle, &u))
        {
          skip_whitespace_and_optional_comma (parser);
          if (svg_number_parse2 (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER, &x, &u))
            {
              skip_whitespace_and_optional_comma (parser);
              if (svg_number_parse2 (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER, &y, &u))
                value = svg_transform_new_rotate (angle, x, y);
              else
                value = svg_transform_new_rotate (angle, x, 0);
            }
          else
            value = svg_transform_new_rotate (angle, 0, 0);
        }
      break;

    case TRANSFORM_SKEW_X:
      if (svg_number_parse2 (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER, &angle, &u))
        value = svg_transform_new_skew_x (angle);
      break;

    case TRANSFORM_SKEW_Y:
      if (svg_number_parse2 (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER, &angle, &u))
        value = svg_transform_new_skew_y (angle);
      break;

    default:
      g_assert_not_reached ();
    }

  gtk_css_parser_unref (parser);
  return value;
}

void
svg_primitive_transform_print (const SvgValue *value,
                               GString        *s)
{
  const SvgTransform *tf = (const SvgTransform *) value;

  g_assert (tf->n_transforms == 1);

  switch (tf->transforms[0].type)
    {
    case TRANSFORM_TRANSLATE:
      string_append_double (s, "", tf->transforms[0].translate.x);
      string_append_double (s, " ", tf->transforms[0].translate.y);
      break;

    case TRANSFORM_SCALE:
      string_append_double (s, "", tf->transforms[0].scale.x);
      string_append_double (s, " ", tf->transforms[0].scale.y);
      break;

    case TRANSFORM_ROTATE:
      string_append_double (s, "", tf->transforms[0].rotate.angle);
      string_append_double (s, " ", tf->transforms[0].rotate.x);
      string_append_double (s, " ", tf->transforms[0].rotate.y);
      break;

    case TRANSFORM_SKEW_X:
      string_append_double (s, "", tf->transforms[0].skew_x.angle);
      break;

    case TRANSFORM_SKEW_Y:
      string_append_double (s, "", tf->transforms[0].skew_y.angle);
      break;

    case TRANSFORM_MATRIX:
    case TRANSFORM_TRANSLATE_3D:
    case TRANSFORM_SCALE_3D:
    case TRANSFORM_ROTATE_3D:
    case TRANSFORM_SKEW:
    case TRANSFORM_PERSPECTIVE:
    case TRANSFORM_MATRIX_3D:
    case TRANSFORM_NONE:
    default:
      g_assert_not_reached ();
    }
}

static void
svg_transform_print (const SvgValue *value,
                     GString        *s)
{
  const SvgTransform *tf = (const SvgTransform *) value;

  for (unsigned int i = 0; i < tf->n_transforms; i++)
    {
      const PrimitiveTransform *t = &tf->transforms[i];

      if (i > 0)
        g_string_append_c (s, ' ');

      switch (t->type)
        {
        case TRANSFORM_NONE:
          g_string_append (s, "none");
          break;
        case TRANSFORM_TRANSLATE:
          string_append_double (s, "translate(", t->translate.x);
          string_append_double (s, ", ", t->translate.y);
          g_string_append (s, ")");
          break;
        case TRANSFORM_SCALE:
          string_append_double (s, "scale(", t->scale.x);
          string_append_double (s, ", ", t->scale.y);
          g_string_append (s, ")");
          break;
        case TRANSFORM_ROTATE:
          string_append_double (s, "rotate(", t->rotate.angle);
          string_append_double (s, ", ", t->rotate.x);
          string_append_double (s, ", ", t->rotate.y);
          g_string_append (s, ")");
          break;
        case TRANSFORM_SKEW_X:
          string_append_double (s, "skewX(",  t->skew_x.angle);
          g_string_append (s, ")");
          break;
        case TRANSFORM_SKEW_Y:
          string_append_double (s, "skewY(", t->skew_y.angle);
          g_string_append (s, ")");
          break;
        case TRANSFORM_MATRIX:
          string_append_double (s, "matrix(", t->matrix.xx);
          string_append_double (s, ", ", t->matrix.yx);
          string_append_double (s, ", ", t->matrix.xy);
          string_append_double (s, ", ", t->matrix.yy);
          string_append_double (s, ", ", t->matrix.dx);
          string_append_double (s, ", ", t->matrix.dy);
          g_string_append (s, ")");
          break;
        case TRANSFORM_TRANSLATE_3D:
          string_append_double (s, "translate3d(", t->translate3d.x);
          string_append_double (s, ", ", t->translate3d.y);
          string_append_double (s, ", ", t->translate3d.z);
          g_string_append (s, ")");
          break;
        case TRANSFORM_SCALE_3D:
          string_append_double (s, "scale3d(", t->scale3d.x);
          string_append_double (s, ", ", t->scale3d.y);
          string_append_double (s, ", ", t->scale3d.z);
          g_string_append (s, ")");
          break;
        case TRANSFORM_ROTATE_3D:
          string_append_double (s, "rotate3d(", t->rotate3d.x);
          string_append_double (s, ", ", t->rotate3d.y);
          string_append_double (s, ", ", t->rotate3d.z);
          string_append_double (s, ", ", t->rotate3d.angle);
          g_string_append (s, ")");
          break;
        case TRANSFORM_PERSPECTIVE:
          string_append_double (s, "perspective(", t->perspective.depth);
          g_string_append (s, ")");
          break;
        case TRANSFORM_SKEW:
          string_append_double (s, "skew(", t->skew.x);
          string_append_double (s, ", ", t->skew.y);
          g_string_append (s, ")");
          break;
        case TRANSFORM_MATRIX_3D:
          {
            float v[16];
            graphene_matrix_to_float (&t->matrix3d.m, v);
            string_append_double (s, "matrix(", v[0]);
            for (unsigned int j = 1; j < 16; j++)
              {
                if (j % 4 == 0)
                  string_append_double (s, ",\n       ", v[j]);
                else
                  string_append_double (s, ", ", v[j]);
              }
            g_string_append (s, ")");
          }
          break;
        default:
          g_assert_not_reached ();
        }
    }
}

static void
interpolate_matrices (double       t,
                      const double m0[6],
                      const double m1[6],
                      double       m[6])
{
  graphene_matrix_t mat0, mat1, res;

  graphene_matrix_init_from_2d (&mat0, m0[0], m0[1], m0[2], m0[3], m0[4], m0[5]);
  graphene_matrix_init_from_2d (&mat1, m1[0], m1[1], m1[2], m1[3], m1[4], m1[5]);
  graphene_matrix_interpolate (&mat0, &mat1, t, &res);
  graphene_matrix_to_2d (&res, &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]);
}

static GskTransform *
primitive_transform_apply (const PrimitiveTransform *transform,
                           GskTransform             *next)
{
  switch (transform->type)
    {
    case TRANSFORM_NONE:
      return next;

    case TRANSFORM_TRANSLATE:
      return gsk_transform_translate (next,
                                      &GRAPHENE_POINT_INIT (
                                        transform->translate.x,
                                        transform->translate.y
                                      ));

    case TRANSFORM_SCALE:
      return gsk_transform_scale (next,
                                  transform->scale.x,
                                  transform->scale.y);

    case TRANSFORM_ROTATE:
      return gsk_transform_translate (
                  gsk_transform_rotate (
                      gsk_transform_translate (next,
                                               &GRAPHENE_POINT_INIT (
                                                 transform->rotate.x,
                                                 transform->rotate.y
                                               )),
                      transform->rotate.angle),
                  &GRAPHENE_POINT_INIT (
                    -transform->rotate.x,
                    -transform->rotate.y
                  ));

    case TRANSFORM_SKEW_X:
      return gsk_transform_skew (next, transform->skew_x.angle, 0);

    case TRANSFORM_SKEW_Y:
      return gsk_transform_skew (next, 0, transform->skew_y.angle);

    case TRANSFORM_MATRIX:
      return gsk_transform_matrix_2d (next,
                                      transform->matrix.xx, transform->matrix.yx,
                                      transform->matrix.xy, transform->matrix.yy,
                                      transform->matrix.dx, transform->matrix.dy);

    case TRANSFORM_TRANSLATE_3D:
      return gsk_transform_translate_3d (next,
                                         &GRAPHENE_POINT3D_INIT (
                                           transform->translate3d.x,
                                           transform->translate3d.y,
                                           transform->translate3d.z));
    case TRANSFORM_SCALE_3D:
      return gsk_transform_scale_3d (next,
                                     transform->scale3d.x,
                                     transform->scale3d.y,
                                     transform->scale3d.z);

    case TRANSFORM_ROTATE_3D:
      {
        graphene_vec3_t v3;
        return gsk_transform_rotate_3d (next,
                                        transform->rotate3d.angle,
                                        graphene_vec3_init (&v3,
                                          transform->rotate3d.x,
                                          transform->rotate3d.y,
                                          transform->rotate3d.z));
      }

    case TRANSFORM_SKEW:
      return gsk_transform_skew (next, transform->skew.x, transform->skew.y);

    case TRANSFORM_PERSPECTIVE:
      return gsk_transform_perspective (next, transform->perspective.depth);

    case TRANSFORM_MATRIX_3D:
      return gsk_transform_matrix (next, &transform->matrix3d.m);

    default:
      g_assert_not_reached ();
    }
}

static SvgValue *
svg_transform_interpolate (const SvgValue    *value0,
                           const SvgValue    *value1,
                           SvgComputeContext *context,
                           double             t)
{
  static const PrimitiveTransform identity[] = {
    { .type = TRANSFORM_TRANSLATE, .translate = { 0, 0 } },
    { .type = TRANSFORM_SCALE, .scale = { 1, 1 } },
    { .type = TRANSFORM_ROTATE, .rotate = { 0, 0, 0 } },
    { .type = TRANSFORM_SKEW_X, .skew_x = { 0 } },
    { .type = TRANSFORM_SKEW_Y, .skew_y = { 0 } },
    { .type = TRANSFORM_MATRIX, .matrix = { .m = { 1, 0, 0, 1, 0, 0 } } },
    { .type = TRANSFORM_TRANSLATE_3D, .translate3d = { 0, 0, 0 } },
    { .type = TRANSFORM_SCALE_3D, .scale3d = { 1, 1, 1 } },
    { .type = TRANSFORM_ROTATE_3D, .rotate3d = { 0, 1, 0, 0 } },
    { .type = TRANSFORM_SKEW, .skew = { 0, 0 } },
  };
  static PrimitiveTransform id_mat3 = { 0, };
  const SvgTransform *tf0 = (const SvgTransform *) value0;
  const SvgTransform *tf1 = (const SvgTransform *) value1;
  SvgTransform *tf;

  if (id_mat3.type == 0)
    {
      id_mat3.type = TRANSFORM_MATRIX_3D;
      graphene_matrix_init_identity (&id_mat3.matrix3d.m);
    }

  tf = svg_transform_alloc (MAX (tf0->n_transforms, tf1->n_transforms));

  for (unsigned int i = 0; i < tf->n_transforms; i++)
    {
      const PrimitiveTransform *p0;
      const PrimitiveTransform *p1;
      PrimitiveTransform *p = &tf->transforms[i];

      if (i < tf0->n_transforms)
        p0 = &tf0->transforms[i];
      else if (tf1->transforms[i].type == TRANSFORM_PERSPECTIVE ||
               tf1->transforms[i].type == TRANSFORM_MATRIX_3D)
        p0 = &id_mat3;
      else
        p0 = &identity[tf1->transforms[i].type];

      if (i < tf1->n_transforms)
        p1 = &tf1->transforms[i];
      else if (tf0->transforms[i].type == TRANSFORM_PERSPECTIVE ||
               tf0->transforms[i].type == TRANSFORM_MATRIX_3D)
        p1 = &id_mat3;
      else
        p1 = &identity[tf0->transforms[i].type];

      if (p0->type != p1->type)
        {
          GskTransform *transform = NULL;
          graphene_matrix_t mat1, mat2, res;

          transform = NULL;
          for (unsigned int j = i; j < tf0->n_transforms; j++)
            transform = primitive_transform_apply (&tf0->transforms[j], transform);
          gsk_transform_to_matrix (transform, &mat1);
          gsk_transform_unref (transform);

          transform = NULL;
          for (unsigned int j = i; j < tf1->n_transforms; j++)
            transform = primitive_transform_apply (&tf1->transforms[j], transform);
          gsk_transform_to_matrix (transform, &mat2);
          gsk_transform_unref (transform);

          graphene_matrix_interpolate (&mat1, &mat2, t, &res);

          p->type = TRANSFORM_MATRIX;
          graphene_matrix_to_2d (&res,
                                 &p->matrix.m[0], &p->matrix.m[1],
                                 &p->matrix.m[2], &p->matrix.m[3],
                                 &p->matrix.m[4], &p->matrix.m[5]);
          tf->n_transforms = i + 1;
          break;
        }

      p->type = p0->type;
      switch (p0->type)
        {
        case TRANSFORM_NONE:
          break;
        case TRANSFORM_TRANSLATE:
          p->translate.x = lerp (t, p0->translate.x, p1->translate.x);
          p->translate.y = lerp (t, p0->translate.y, p1->translate.y);
          break;
        case TRANSFORM_SCALE:
          p->scale.x = lerp (t, p0->scale.x, p1->scale.x);
          p->scale.y = lerp (t, p0->scale.y, p1->scale.y);
          break;
        case TRANSFORM_ROTATE:
          p->rotate.angle = lerp (t, p0->rotate.angle, p1->rotate.angle);
          p->rotate.x = lerp (t, p0->rotate.x, p1->rotate.x);
          p->rotate.y = lerp (t, p0->rotate.y, p1->rotate.y);
          break;
        case TRANSFORM_SKEW_X:
          p->skew_x.angle = lerp (t, p0->skew_x.angle, p1->skew_x.angle);
          break;
        case TRANSFORM_SKEW_Y:
          p->skew_y.angle = lerp (t, p0->skew_y.angle, p1->skew_y.angle);
          break;
        case TRANSFORM_MATRIX:
          interpolate_matrices (t, p0->matrix.m, p1->matrix.m, p->matrix.m);
          break;
        case TRANSFORM_TRANSLATE_3D:
          p->translate3d.x = lerp (t, p0->translate3d.x, p1->translate3d.x);
          p->translate3d.y = lerp (t, p0->translate3d.y, p1->translate3d.y);
          p->translate3d.z = lerp (t, p0->translate3d.z, p1->translate3d.z);
          break;
        case TRANSFORM_SCALE_3D:
          p->scale3d.x = lerp (t, p0->scale3d.x, p1->scale3d.x);
          p->scale3d.y = lerp (t, p0->scale3d.y, p1->scale3d.y);
          p->scale3d.z = lerp (t, p0->scale3d.z, p1->scale3d.z);
          break;
        case TRANSFORM_ROTATE_3D:
          p->rotate3d.angle = lerp (t, p0->rotate3d.angle, p1->rotate3d.angle);
          p->rotate3d.x = lerp (t, p0->rotate3d.x, p1->rotate3d.x);
          p->rotate3d.y = lerp (t, p0->rotate3d.y, p1->rotate3d.y);
          p->rotate3d.z = lerp (t, p0->rotate3d.z, p1->rotate3d.z);
          break;
        case TRANSFORM_SKEW:
          p->skew.x = lerp (t, p0->skew.x, p1->skew.x);
          p->skew.y = lerp (t, p0->skew.y, p1->skew.y);
          break;
        case TRANSFORM_PERSPECTIVE:
          p->perspective.depth = lerp (t, p0->perspective.depth, p1->perspective.depth);
          break;
        case TRANSFORM_MATRIX_3D:
          graphene_matrix_interpolate (&p0->matrix3d.m, &p1->matrix3d.m, t, &p->matrix3d.m);
          break;
        default:
          g_assert_not_reached ();
        }
    }

  return (SvgValue *) tf;
}

static SvgValue *
svg_transform_accumulate (const SvgValue    *value0,
                          const SvgValue    *value1,
                          SvgComputeContext *context,
                          int                n)
{
  const SvgTransform *tf0 = (const SvgTransform *) value0;
  const SvgTransform *tf1 = (const SvgTransform *) value1;
  SvgTransform *tf;
  int n0;

  if (tf1->n_transforms == 1 && tf1->transforms[0].type == TRANSFORM_NONE)
    return svg_value_ref ((SvgValue *) value0);

  if (tf0->n_transforms == 1 && tf0->transforms[0].type == TRANSFORM_NONE)
    {
      if (n == 1)
        return svg_value_ref ((SvgValue *) value1);

      n0 = 0;
    }
  else
    n0 = tf0->n_transforms;

  /* special-case this one */
  if (tf1->n_transforms == 1 &&
      tf1->transforms[0].type != TRANSFORM_MATRIX &&
      tf1->transforms[0].type != TRANSFORM_MATRIX_3D &&
      tf1->transforms[0].type != TRANSFORM_PERSPECTIVE)
    {
      PrimitiveTransform *p;

      tf = svg_transform_alloc (n0 + 1);
      p = &tf->transforms[0];
      memcpy (p, tf1->transforms, sizeof (PrimitiveTransform));

      switch (p->type)
        {
        case TRANSFORM_NONE:
          break;
        case TRANSFORM_TRANSLATE:
          p->translate.x *= n;
          p->translate.y *= n;
          break;
        case TRANSFORM_SCALE:
          p->scale.x = pow (p->scale.x, (double) n);
          p->scale.y = pow (p->scale.y, (double) n);
          break;
        case TRANSFORM_ROTATE:
          p->rotate.angle *= n;
          break;
        case TRANSFORM_SKEW_X:
          p->skew_x.angle *= n;
          break;
        case TRANSFORM_SKEW_Y:
          p->skew_y.angle *= n;
          break;
        case TRANSFORM_TRANSLATE_3D:
          p->translate3d.x *= n;
          p->translate3d.y *= n;
          p->translate3d.z *= n;
          break;
        case TRANSFORM_SCALE_3D:
          p->scale3d.x = pow (p->scale3d.x, (double) n);
          p->scale3d.y = pow (p->scale3d.y, (double) n);
          p->scale3d.z = pow (p->scale3d.z, (double) n);
          break;
        case TRANSFORM_ROTATE_3D:
          p->rotate3d.angle *= n;
          break;
        case TRANSFORM_SKEW:
          p->skew.x *= n; /* No idea if this is correct */
          p->skew.y *= n;
          break;
        case TRANSFORM_PERSPECTIVE:
        case TRANSFORM_MATRIX:
        case TRANSFORM_MATRIX_3D:
        default:
          g_assert_not_reached ();
        }

      if (n0 > 0)
        memcpy (tf->transforms + 1,
                tf0->transforms,
                n0 * sizeof (PrimitiveTransform));

    }
  else
    {
      /* For the general case, simply concatenate all the transforms */
      tf = svg_transform_alloc (n0 + n * tf1->n_transforms);
      for (unsigned int i = 0; i < n; i++)
        memcpy (&tf->transforms[i * tf1->n_transforms],
                tf1->transforms,
                tf1->n_transforms * sizeof (PrimitiveTransform));

      if (n0 > 0)
        memcpy (&tf->transforms[n * tf1->n_transforms],
                tf0->transforms,
                n0 * sizeof (PrimitiveTransform));
    }

  return (SvgValue *) tf;
}

GskTransform *
svg_transform_get_gsk (const SvgValue *value)
{
  const SvgTransform *tf = (const SvgTransform *) value;
  GskTransform *t = NULL;

  g_assert (value->class == &SVG_TRANSFORM_CLASS);

  if (!tf)
    return NULL;

  for (unsigned int i = 0; i < tf->n_transforms; i++)
    t = primitive_transform_apply (&tf->transforms[i], t);

  return t;
}

static double
hypot3 (double x, double y, double z)
{
  return sqrt (x * x + y * y + z * z);
}

static double
svg_transform_distance (const SvgValue *value0,
                        const SvgValue *value1)
{
  const SvgTransform *tf0 = (const SvgTransform *) value0;
  const SvgTransform *tf1 = (const SvgTransform *) value1;
  const PrimitiveTransform *p0 = &tf0->transforms[0];
  const PrimitiveTransform *p1 = &tf1->transforms[0];

  if (tf0->n_transforms > 1 || tf1->n_transforms > 1)
    {
      g_warning ("Can't determine distance between complex transforms");
      return 1;
    }

  if (p0->type != p1->type)
    {
      g_warning ("Can't determine distance between different "
                 "primitive transform types");
      return 1;
    }

  switch (p0->type)
    {
    case TRANSFORM_NONE:
      return 0;
    case TRANSFORM_TRANSLATE:
      return hypot (p0->translate.x - p1->translate.x,
                    p0->translate.y - p1->translate.y);
      break;
    case TRANSFORM_SCALE:
      return hypot (p0->scale.x - p1->scale.x,
                    p0->scale.y - p1->scale.y);
          break;
    case TRANSFORM_ROTATE:
      return hypot (p0->rotate.angle - p1->rotate.angle, 0);
    case TRANSFORM_SKEW_X:
    case TRANSFORM_SKEW_Y:
    case TRANSFORM_MATRIX:
    case TRANSFORM_MATRIX_3D:
    case TRANSFORM_SKEW:
    case TRANSFORM_PERSPECTIVE:
      g_warning ("Can't determine distance between these "
                 "primitive transforms");
      return 1;
    case TRANSFORM_TRANSLATE_3D:
      return hypot3 (p0->translate3d.x - p1->translate3d.x,
                     p0->translate3d.y - p1->translate3d.y,
                     p0->translate3d.z - p1->translate3d.z);
      break;
    case TRANSFORM_SCALE_3D:
      return hypot3 (p0->scale3d.x - p1->scale3d.x,
                     p0->scale3d.y - p1->scale3d.y,
                     p0->scale3d.z - p1->scale3d.z);
          break;
    case TRANSFORM_ROTATE_3D:
      if (p0->rotate3d.x == p0->rotate3d.x &&
          p0->rotate3d.y == p0->rotate3d.y &&
          p0->rotate3d.z == p0->rotate3d.z)
        {
          return hypot (p0->rotate3d.angle - p1->rotate3d.angle, 0);
        }
      else
        {
          g_warning ("Can't determine distance between these "
                     "primitive transforms");
          return 1;
        }
    default:
      g_assert_not_reached ();
    }
}

TransformType
svg_transform_get_type (const SvgValue *value,
                        unsigned int    pos)
{
  const SvgTransform *tf = (const SvgTransform *) value;

  g_assert (value->class == &SVG_TRANSFORM_CLASS);
  g_assert (pos < tf->n_transforms);

  return tf->transforms[pos].type;
}

TransformType
svg_transform_get_primitive (const SvgValue *value,
                             unsigned int    pos,
                             double          params[6])
{
  const SvgTransform *tf = (const SvgTransform *) value;

  g_assert (value->class == &SVG_TRANSFORM_CLASS);
  g_assert (pos < tf->n_transforms);

  switch (tf->transforms[pos].type)
    {
    case TRANSFORM_NONE:
      break;
    case TRANSFORM_TRANSLATE:
      params[0] = tf->transforms[pos].translate.x;
      params[1] = tf->transforms[pos].translate.y;
      break;
    case TRANSFORM_SCALE:
      params[0] = tf->transforms[pos].scale.x;
      params[1] = tf->transforms[pos].scale.y;
      break;
    case TRANSFORM_ROTATE:
      params[0] = tf->transforms[pos].rotate.angle;
      params[1] = tf->transforms[pos].rotate.x;
      params[2] = tf->transforms[pos].rotate.y;
      break;
    case TRANSFORM_SKEW_X:
      params[0] = tf->transforms[pos].skew_x.angle;
      break;
    case TRANSFORM_SKEW_Y:
      params[0] = tf->transforms[pos].skew_y.angle;
      break;
    case TRANSFORM_MATRIX:
      memcpy (params, tf->transforms[pos].matrix.m, sizeof (double) * 6);
      break;
    case TRANSFORM_TRANSLATE_3D:
      params[0] = tf->transforms[pos].translate3d.x;
      params[1] = tf->transforms[pos].translate3d.y;
      params[2] = tf->transforms[pos].translate3d.z;
      break;
    case TRANSFORM_SCALE_3D:
      params[0] = tf->transforms[pos].scale3d.x;
      params[1] = tf->transforms[pos].scale3d.y;
      params[2] = tf->transforms[pos].scale3d.z;
      break;
    case TRANSFORM_ROTATE_3D:
      params[0] = tf->transforms[pos].rotate3d.angle;
      params[1] = tf->transforms[pos].rotate3d.x;
      params[2] = tf->transforms[pos].rotate3d.y;
      params[3] = tf->transforms[pos].rotate3d.z;
      break;
    case TRANSFORM_SKEW:
      params[0] = tf->transforms[pos].skew.x;
      params[1] = tf->transforms[pos].skew.y;
      break;
    case TRANSFORM_PERSPECTIVE:
      params[0] = tf->transforms[pos].perspective.depth;
      break;
    case TRANSFORM_MATRIX_3D:
    default:
      g_assert_not_reached ();
    }

  return tf->transforms[pos].type;
}

gboolean
svg_transform_is_none (const SvgValue *value)
{
  const SvgTransform *tf = (const SvgTransform *) value;

  g_assert (value->class == &SVG_TRANSFORM_CLASS);

  return tf->transforms[0].type == TRANSFORM_NONE;
}

gboolean
svg_value_is_transform (const SvgValue *value)
{
   return value->class == &SVG_TRANSFORM_CLASS;
}

unsigned int
svg_transform_get_length (const SvgValue *value)
{
  const SvgTransform *tf = (const SvgTransform *) value;

  g_assert (value->class == &SVG_TRANSFORM_CLASS);

  return tf->n_transforms;
}

SvgValue *
svg_transform_get_transform (const SvgValue *value,
                             unsigned int    pos)
{
  SvgTransform *tf = (SvgTransform *) value;

  g_assert (value->class == &SVG_TRANSFORM_CLASS);

  switch (tf->transforms[pos].type)
    {
    case TRANSFORM_NONE:
      return svg_transform_new_none ();
    case TRANSFORM_TRANSLATE:
      return svg_transform_new_translate (tf->transforms[pos].translate.x,
                                          tf->transforms[pos].translate.y);
    case TRANSFORM_SCALE:
      return svg_transform_new_scale (tf->transforms[pos].scale.x,
                                      tf->transforms[pos].scale.y);
    case TRANSFORM_ROTATE:
      return svg_transform_new_rotate (tf->transforms[pos].rotate.angle,
                                       tf->transforms[pos].rotate.x,
                                       tf->transforms[pos].rotate.y);
    case TRANSFORM_SKEW_X:
      return svg_transform_new_skew_x (tf->transforms[pos].skew_x.angle);
    case TRANSFORM_SKEW_Y:
      return svg_transform_new_skew_y (tf->transforms[pos].skew_y.angle);
    case TRANSFORM_MATRIX:
      return svg_transform_new_matrix (tf->transforms[pos].matrix.m);
    case TRANSFORM_TRANSLATE_3D:
      return svg_transform_new_translate_3d (tf->transforms[pos].translate3d.x,
                                             tf->transforms[pos].translate3d.y,
                                             tf->transforms[pos].translate3d.z);
    case TRANSFORM_SCALE_3D:
      return svg_transform_new_scale_3d (tf->transforms[pos].scale3d.x,
                                         tf->transforms[pos].scale3d.y,
                                         tf->transforms[pos].scale3d.z);
    case TRANSFORM_ROTATE_3D:
      return svg_transform_new_rotate_3d (tf->transforms[pos].rotate3d.angle,
                                          tf->transforms[pos].rotate3d.x,
                                          tf->transforms[pos].rotate3d.y,
                                          tf->transforms[pos].rotate3d.z);
    case TRANSFORM_SKEW:
      return svg_transform_new_skew (tf->transforms[pos].skew.x,
                                     tf->transforms[pos].skew.y);
    case TRANSFORM_PERSPECTIVE:
      return svg_transform_new_perspective (tf->transforms[pos].perspective.depth);
    case TRANSFORM_MATRIX_3D:
      return svg_transform_new_matrix_3d (&tf->transforms[pos].matrix3d.m);

    default:
      g_assert_not_reached ();
    }
}
