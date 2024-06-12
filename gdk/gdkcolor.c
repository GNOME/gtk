/* GDK - The GIMP Drawing Kit
 *
 * Copyright (C) 2021 Benjamin Otte
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

#include "gdkcolorprivate.h"
#include "gdkcolorstateprivate.h"
#include "gdkrgbaprivate.h"

#include <lcms2.h>

/**
 * GdkColor:
 * @color_state: the color state to interpret the values in
 * @values: the 3 coordinates that define the color, followed
 *   by the alpha value
 *
 * A `GdkColor` represents a color.
 *
 * The color state defines the meaning and range of the values.
 * E.g., the srgb color state has r, g, b components representing
 * red, green and blue with a range of [0,1], while the oklch color
 * state has l, c, h components representing luminosity, chromaticity
 * and hue, with l ranging from 0 to 1 and c from 0 to about 0.4, while
 * h is interpreted as angle in degrees.
 *
 * value[3] is always the alpha value with a range of [0,1].
 *
 * Note that `GdkColor` is mainly intended for on-stack use, and does
 * not take a reference to the @color_state, unless you use gdk_color_copy().
 */

/* {{{ Boxed type */

G_DEFINE_BOXED_TYPE (GdkColor, gdk_color,
                     gdk_color_copy, gdk_color_free)

/**
 * gdk_color_copy:
 * @self: a `GdkColor`
 *
 * Makes a copy of a `GdkColor`.
 *
 * The result must be freed through [method@Gdk.Color.free].
 *
 * Returns: A newly allocated `GdkColor`, with the same contents as @self
 *
 * Since: 4.16
 */
GdkColor *
gdk_color_copy (const GdkColor *self)
{
  GdkColor *copy = g_new0 (GdkColor, 1);
  g_set_object (&copy->color_state, self->color_state);
  memcpy (copy->values, self->values, sizeof (float) * 4);
  return copy;
}

/**
 * gdk_color_free:
 * @self: a `GdkColor`
 *
 * Frees a `Gdkcolor`.
 *
 * Since: 4.16
 */
void
gdk_color_free (GdkColor *self)
{
  g_clear_pointer (&self->color_state, gdk_color_state_unref);
  g_free (self);
}

/* }}} */
/* {{{ Public API */

void
(gdk_color_init) (GdkColor      *self,
                  GdkColorState *color_state,
                  const float    components[4])
{
  _gdk_color_init (self, color_state, components);
}

void
(gdk_color_init_from_rgba) (GdkColor       *self,
                            const GdkRGBA  *rgba)
{
  _gdk_color_init_from_rgba (self, rgba);
}

GdkColorState *
(gdk_color_get_color_state) (const GdkColor *self)
{
  return _gdk_color_get_color_state (self);
}

const float *
(gdk_color_get_components) (const GdkColor *self)
{
  return _gdk_color_get_components (self);
}

gboolean
(gdk_color_equal) (const GdkColor *self,
                   const GdkColor *other)
{
  return _gdk_color_equal (self, other);
}

gboolean
(gdk_color_is_black) (const GdkColor *self)
{
  return _gdk_color_is_black (self);
}

gboolean
(gdk_color_is_clear) (const GdkColor *self)
{
  return _gdk_color_is_clear (self);
}

gboolean
(gdk_color_is_opaque) (const GdkColor *self)
{
  return _gdk_color_is_opaque (self);
}

/* }}} */

/**
 * gdk_color_convert:
 * @self: the `GdkColor` to store the result in
 * @color_state: the target color start
 * @other: the `GdkColor` to convert
 *
 * Converts a given `GdkColor` to another color state.
 *
 * After the conversion, @self will represent the same
 * color as @other in @color_state, to the degree possible.
 *
 * Different color states have different gamuts of colors
 * they can represent, and converting a color to a color
 * state with a smaller gamut may yield an 'out of gamut'
 * result.
 *
 * Since: 4.16
 */
void
(gdk_color_convert) (GdkColor        *self,
                     GdkColorState   *color_state,
                     const GdkColor  *other)
{
  GdkColorStateTransform tf;

  self->color_state = color_state;

  gdk_color_state_transform_init (&tf, other->color_state, color_state, TRUE);
  gdk_color_state_transform (&tf, other->values, self->values, 1);
  gdk_color_state_transform_finish (&tf);
}

/**
 * gdk_color_convert_rgba:
 * @self: the `GdkColor` to store the result in
 * @color_state: the target color state
 * @rgba: the `GdkRGBA` to convert
 *
 * Converts a given `GdkRGBA` to the target @color_state.
 *
 * Since: 4.16
 */
void
gdk_color_convert_rgba (GdkColor        *self,
                        GdkColorState   *color_state,
                        const GdkRGBA   *rgba)
{
  GdkColor tmp = { (GdkColorState *) GINT_TO_POINTER (GDK_COLOR_STATE_SRGB),
                   { rgba->red, rgba->green, rgba->blue, rgba->alpha } };

  gdk_color_convert (self, color_state, &tmp);
}

/**
 * gdk_color_mix:
 * @self: the `GdkColor` to store the result in
 * @color_start: the target color state
 * @src1: the first color
 * @src2: the second color
 * @progress: the relative amount of @src2, from 0 to 1
 *
 * Mix two colors.
 *
 * This operation first converts @src1 and @src2 to the
 * target @color_state, and then interpolates between them
 * with a position given by @progress.
 *
 * Since: 4.16
 */
void
gdk_color_mix (GdkColor        *self,
               GdkColorState   *color_state,
               const GdkColor  *src1,
               const GdkColor  *src2,
               double           progress)
{
  if (src1->color_state != color_state)
    {
      GdkColor tmp;
      gdk_color_convert (&tmp, color_state, src1);
      gdk_color_mix (self, color_state, &tmp, src2, progress);
    }
  else if (src2->color_state != color_state)
    {
      GdkColor tmp;
      gdk_color_convert (&tmp, color_state, src2);
      gdk_color_mix (self, color_state, src1, &tmp, progress);
    }
  else
    {
      gsize i;
      const float *s1, *s2;
      float *d;

      self->color_state = color_state;
      self->values[3] = src1->values[3] * (1.0 - progress) + src2->values[3] * progress;

      d = (float *) gdk_color_get_components (self);
      s1 = gdk_color_get_components (src1);
      s2 = gdk_color_get_components (src2);

      if (self->values[3] == 0)
        {
          for (i = 0; i < 3; i++)
            d[i] = s1[i] * (1.0 - progress) + s2[i] * progress;
        }
      else
        {
          for (i = 0; i < 3; i++)
            d[i] = (s1[i] * src1->values[3] * (1.0 - progress) + s2[i] * src2->values[3] * progress) / self->values[3];
        }
    }
}

/*< private >
 * gdk_color_parser_parse:
 * @parser: the parser
 * @color: location to store the parsed color
 *
 * Parses a string representation of colors that
 * is inspired by CSS.
 *
 * Returns: `TRUE` if parsing succeeded
 *
 * Since: 4.16
 */
gboolean
gdk_color_parser_parse (GtkCssParser  *parser,
                        GdkColor      *color)
{
  const GtkCssToken *token;
  GdkRGBA rgba;

  if (gtk_css_parser_has_function (parser, "color"))
    {
      GdkColorState *color_state;
      float values[4];
      const char *coords;

      gtk_css_parser_start_block (parser);

      if (gtk_css_parser_try_ident (parser, "srgb"))
        {
          coords = "rgb";
          color_state = gdk_color_state_get_srgb ();
        }
      else if (gtk_css_parser_try_ident (parser, "srgb-linear"))
        {
          coords = "rgb";
          color_state = gdk_color_state_get_srgb_linear ();
        }
      else if (gtk_css_parser_try_ident (parser, "oklab"))
        {
          coords = "lab";
          color_state = gdk_color_state_get_oklab ();
        }
      else if (gtk_css_parser_try_ident (parser, "oklch"))
        {
          coords = "lch";
          color_state = gdk_color_state_get_oklch ();
        }
      else
        {
          gtk_css_parser_error_syntax (parser, "Expected a valid color state");
          gtk_css_parser_end_block (parser);
          return FALSE;
        }

      for (int i = 0; i < 3; i++)
        {
          token = gtk_css_parser_get_token (parser);

          if (gtk_css_token_is (token, GTK_CSS_TOKEN_PERCENTAGE))
            {
              float v = token->number.number;

              switch (coords[i])
                {
                case 'l':
                  values[i] = CLAMP (v / 100.0, 0, 1);
                  break;
                case 'a':
                case 'b':
                  values[i] = v * 0.4 / 100.0;
                  break;
                case 'c':
                  values[i] = MAX (0, v * 0.4 / 100.0);
                  break;
                case 'h':
                  gtk_css_parser_error_syntax (parser, "Can't use percentage for hue");
                  gtk_css_parser_end_block (parser);
                  return FALSE;
                default:
                  values[i] = CLAMP (v / 100.0, 0, 1);
                  break;
                }

              gtk_css_parser_consume_token (parser);
            }
          else if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_NUMBER) ||
                   gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_NUMBER) ||
                   gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER) ||
                   gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER))
            {
              float v = token->number.number;

              switch (coords[i])
                {
                case 'l':
                  values[i] = CLAMP (v, 0, 1);
                  break;
                case 'a':
                case 'b':
                case 'h':
                  values[i] = v;
                  break;
                case 'c':
                  values[i] = MAX (0, v);
                  break;
                default:
                  values[i] = CLAMP (v, 0, 1);
                  break;
                }

              gtk_css_parser_consume_token (parser);
            }
          else
            {
              gtk_css_parser_error_syntax (parser, "Expected a number or percentage");
              gtk_css_parser_end_block (parser);
              return FALSE;
            }
        }

      token = gtk_css_parser_get_token (parser);
      if (gtk_css_token_is (token, GTK_CSS_TOKEN_EOF))
        {
          values[3] = 1;
        }
      else if (gtk_css_token_is_delim (token, '/'))
        {
          gtk_css_parser_consume_token (parser);

          token = gtk_css_parser_get_token (parser);

          if (gtk_css_token_is (token, GTK_CSS_TOKEN_PERCENTAGE))
            {
              values[3] = CLAMP (token->number.number / 100.0, 0, 1);
              gtk_css_parser_consume_token (parser);
            }
          else if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_NUMBER) ||
                   gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_NUMBER) ||
                   gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER) ||
                   gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER))
            {
              values[3] = CLAMP (token->number.number, 0, 1);
              gtk_css_parser_consume_token (parser);
            }
          else
            {
              gtk_css_parser_error_syntax (parser, "Expected a number or percentage");
              gtk_css_parser_end_block (parser);
              return FALSE;
            }

          token = gtk_css_parser_get_token (parser);
          if (!gtk_css_token_is (token, GTK_CSS_TOKEN_EOF))
            {
              gtk_css_parser_error_syntax (parser, "Garbage at the end of the value");
              gtk_css_parser_end_block (parser);
              return FALSE;
            }

          gtk_css_parser_consume_token (parser);
        }
      else
        {
          gtk_css_parser_error_syntax (parser, "Expected '/'");
          gtk_css_parser_end_block (parser);
          return FALSE;
        }

      gtk_css_parser_end_block (parser);

      gdk_color_init (color, color_state, values);
      return TRUE;
    }
  else if (gdk_rgba_parser_parse (parser, &rgba))
    {
      gdk_color_init_from_rgba (color, &rgba);
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_color_print:
 * @self: a `GdkColor`
 * @string: the string to print on
 *
 * Appends a representation of @self to @string.
 *
 * The representation is inspired by CSS3 colors,
 * but not 100% identical, and looks like this:
 *
 *     color(NAME V1 V2 V3 / ALPHA)
 *
 * where `NAME` is the name of the color state,
 * `V1`, `V2`, `V3` and `ALPHA` are the components
 * of the color. Alpha may be omitted if it is 1.
 *
 * Since: 4.16
 */
void
gdk_color_print (const GdkColor *self,
                 GString        *string)
{
  char buffer[48];

  g_string_append (string, "color(");
  g_string_append (string, gdk_color_state_get_name (self->color_state));
  g_string_append_c (string, ' ');
  g_ascii_dtostr (buffer, sizeof (buffer), self->values[0]);
  g_string_append (string, buffer);
  g_string_append_c (string, ' ');
  g_ascii_dtostr (buffer, sizeof (buffer), self->values[1]);
  g_string_append (string, buffer);
  g_string_append_c (string, ' ');
  g_ascii_dtostr (buffer, sizeof (buffer), self->values[2]);
  g_string_append (string, buffer);
  if (self->values[3] < 0.999)
    {
      g_string_append (string, " / ");
      g_ascii_dtostr (buffer, sizeof (buffer), self->values[3]);
      g_string_append (string, buffer);
    }
  g_string_append_c (string, ')');
}

/**
 * gdk_color_to_string:
 * @self: a `GdkColor`
 *
 * Returns a string representation @self.
 *
 * See [method@Gdk.Color.print] for details about
 * the format.
 *
 * Returns: (transfer full): a newly allocated string
 *
 * Since: 4.16
 */
char *
gdk_color_to_string (const GdkColor *self)
{
  GString *string = g_string_new ("");
  gdk_color_print (self, string);
  return g_string_free (string, FALSE);
}

/* vim:set foldmethod=marker expandtab: */
