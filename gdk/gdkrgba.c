/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"
#include "gdkrgba.h"
#include <string.h>
#include <errno.h>
#include <math.h>

#include "fallback-c89.c"

/**
 * SECTION:rgba_colors
 * @Short_description: RGBA colors
 * @Title: RGBA Colors
 *
 * The #GdkRGBA struct is a convenient way to pass rgba colors around.
 * It's based on cairo's way to deal with colors and mirrors its behavior.
 * All values are in the range from 0.0 to 1.0 inclusive. So the color
 * (0.0, 0.0, 0.0, 0.0) represents transparent black and
 * (1.0, 1.0, 1.0, 1.0) is opaque white. Other values will be clamped
 * to this range when drawing.
 */

G_DEFINE_BOXED_TYPE (GdkRGBA, gdk_rgba,
                     gdk_rgba_copy, gdk_rgba_free)

/**
 * GdkRGBA:
 * @red: The intensity of the red channel from 0.0 to 1.0 inclusive
 * @green: The intensity of the green channel from 0.0 to 1.0 inclusive
 * @blue: The intensity of the blue channel from 0.0 to 1.0 inclusive
 * @alpha: The opacity of the color from 0.0 for completely translucent to
 *   1.0 for opaque
 *
 * The GdkRGBA structure is used to represent a (possibly translucent)
 * color, in a way that is compatible with cairos notion of color.
 */

/**
 * gdk_rgba_copy:
 * @rgba: a #GdkRGBA
 *
 * Makes a copy of a #GdkRGBA structure.
 *
 * The result must be freed through gdk_rgba_free().
 *
 * Returns: A newly allocated #GdkRGBA, with the same contents as @rgba
 *
 * Since: 3.0
 */
GdkRGBA *
gdk_rgba_copy (const GdkRGBA *rgba)
{
  return g_slice_dup (GdkRGBA, rgba);
}

/**
 * gdk_rgba_free:
 * @rgba: a #GdkRGBA
 *
 * Frees a #GdkRGBA struct created with gdk_rgba_copy()
 *
 * Since: 3.0
 */
void
gdk_rgba_free (GdkRGBA *rgba)
{
  g_slice_free (GdkRGBA, rgba);
}

#define SKIP_WHITESPACES(s) while (*(s) == ' ') (s)++;

/* Parses a single color component from a rgb() or rgba() specification
 * according to CSS3 rules. Compared to exact CSS3 parsing we are liberal
 * in what we accept as follows:
 *
 *  - For non-percentage values, we accept floats in the range 0-255
 *    not just [0-9]+ integers
 *  - For percentage values we accept any float, not just
 *     [ 0-9]+ | [0-9]* '.' [0-9]+
 *  - We accept mixed percentages and non-percentages in a single
 *    rgb() or rgba() specification.
 */
static gboolean
parse_rgb_value (const gchar  *str,
                 gchar       **endp,
                 gdouble      *number)
{
  const char *p;

  *number = g_ascii_strtod (str, endp);
  if (errno == ERANGE || *endp == str ||
      isinf (*number) || isnan (*number))
    return FALSE;

  p = *endp;

  SKIP_WHITESPACES (p);

  if (*p == '%')
    {
      *endp = (char *)(p + 1);
      *number = CLAMP(*number / 100., 0., 1.);
    }
  else
    {
      *number = CLAMP(*number / 255., 0., 1.);
    }

  return TRUE;
}

/**
 * gdk_rgba_parse:
 * @rgba: the #GdkRGBA struct to fill in
 * @spec: the string specifying the color
 *
 * Parses a textual representation of a color, filling in
 * the <structfield>red</structfield>, <structfield>green</structfield>,
 * <structfield>blue</structfield> and <structfield>alpha</structfield>
 * fields of the @rgba struct.
 *
 * The string can be either one of:
 * <itemizedlist>
 * <listitem>
 * A standard name (Taken from the X11 rgb.txt file).
 * </listitem>
 * <listitem>
 * A hex value in the form '&num;rgb' '&num;rrggbb' '&num;rrrgggbbb'
 * or '&num;rrrrggggbbbb'
 * </listitem>
 * <listitem>
 * A RGB color in the form 'rgb(r,g,b)' (In this case the color will
 * have full opacity)
 * </listitem>
 * <listitem>
 * A RGBA color in the form 'rgba(r,g,b,a)'
 * </listitem>
 * </itemizedlist>
 *
 * Where 'r', 'g', 'b' and 'a' are respectively the red, green, blue and
 * alpha color values. In the last two cases, r g and b are either integers
 * in the range 0 to 255 or precentage values in the range 0% to 100%, and
 * a is a floating point value in the range 0 to 1.
 *
 * Returns: %TRUE if the parsing succeeded
 *
 * Since: 3.0
 */
gboolean
gdk_rgba_parse (GdkRGBA     *rgba,
                const gchar *spec)
{
  gboolean has_alpha;
  gdouble r, g, b, a;
  gchar *str = (gchar *) spec;
  gchar *p;

  if (strncmp (str, "rgba", 4) == 0)
    {
      has_alpha = TRUE;
      str += 4;
    }
  else if (strncmp (str, "rgb", 3) == 0)
    {
      has_alpha = FALSE;
      a = 1;
      str += 3;
    }
  else
    {
      PangoColor pango_color;

      /* Resort on PangoColor for rgb.txt color
       * map and '#' prefixed colors
       */
      if (pango_color_parse (&pango_color, str))
        {
          if (rgba)
            {
              rgba->red = pango_color.red / 65535.;
              rgba->green = pango_color.green / 65535.;
              rgba->blue = pango_color.blue / 65535.;
              rgba->alpha = 1;
            }

          return TRUE;
        }
      else
        return FALSE;
    }

  SKIP_WHITESPACES (str);

  if (*str != '(')
    return FALSE;

  str++;

  /* Parse red */
  SKIP_WHITESPACES (str);
  if (!parse_rgb_value (str, &str, &r))
    return FALSE;
  SKIP_WHITESPACES (str);

  if (*str != ',')
    return FALSE;

  str++;

  /* Parse green */
  SKIP_WHITESPACES (str);
  if (!parse_rgb_value (str, &str, &g))
    return FALSE;
  SKIP_WHITESPACES (str);

  if (*str != ',')
    return FALSE;

  str++;

  /* Parse blue */
  SKIP_WHITESPACES (str);
  if (!parse_rgb_value (str, &str, &b))
    return FALSE;
  SKIP_WHITESPACES (str);

  if (has_alpha)
    {
      if (*str != ',')
        return FALSE;

      str++;

      SKIP_WHITESPACES (str);
      a = g_ascii_strtod (str, &p);
      if (errno == ERANGE || p == str ||
          isinf (a) || isnan (a))
        return FALSE;
      str = p;
      SKIP_WHITESPACES (str);
    }

  if (*str != ')')
    return FALSE;

  str++;

  SKIP_WHITESPACES (str);

  if (*str != '\0')
    return FALSE;

  if (rgba)
    {
      rgba->red = CLAMP (r, 0, 1);
      rgba->green = CLAMP (g, 0, 1);
      rgba->blue = CLAMP (b, 0, 1);
      rgba->alpha = CLAMP (a, 0, 1);
    }

  return TRUE;
}

#undef SKIP_WHITESPACES

/**
 * gdk_rgba_hash:
 * @p: (type GdkRGBA): a #GdkRGBA pointer
 *
 * A hash function suitable for using for a hash
 * table that stores #GdkRGBAs.
 *
 * Return value: The hash value for @p
 *
 * Since: 3.0
 */
guint
gdk_rgba_hash (gconstpointer p)
{
  const GdkRGBA *rgba = p;

  return ((guint) (rgba->red * 65535) +
          ((guint) (rgba->green * 65535) << 11) +
          ((guint) (rgba->blue * 65535) << 22) +
          ((guint) (rgba->alpha * 65535) >> 6));
}

/**
 * gdk_rgba_equal:
 * @p1: (type GdkRGBA): a #GdkRGBA pointer
 * @p2: (type GdkRGBA): another #GdkRGBA pointer
 *
 * Compares two RGBA colors.
 *
 * Return value: %TRUE if the two colors compare equal
 *
 * Since: 3.0
 */
gboolean
gdk_rgba_equal (gconstpointer p1,
                gconstpointer p2)
{
  const GdkRGBA *rgba1, *rgba2;

  rgba1 = p1;
  rgba2 = p2;

  if (rgba1->red == rgba2->red &&
      rgba1->green == rgba2->green &&
      rgba1->blue == rgba2->blue &&
      rgba1->alpha == rgba2->alpha)
    return TRUE;

  return FALSE;
}

/**
 * gdk_rgba_to_string:
 * @rgba: a #GdkRGBA
 *
 * Returns a textual specification of @rgba in the form
 * <literal>rgb (r, g, b)</literal> or
 * <literal>rgba (r, g, b, a)</literal>,
 * where 'r', 'g', 'b' and 'a' represent the red, green,
 * blue and alpha values respectively. r, g, and b are
 * represented as integers in the range 0 to 255, and a
 * is represented as floating point value in the range 0 to 1.
 *
 * These string forms are string forms those supported by
 * the CSS3 colors module, and can be parsed by gdk_rgba_parse().
 *
 * Note that this string representation may lose some
 * precision, since r, g and b are represented as 8-bit
 * integers. If this is a concern, you should use a
 * different representation.
 *
 * Returns: A newly allocated text string
 *
 * Since: 3.0
 */
gchar *
gdk_rgba_to_string (const GdkRGBA *rgba)
{
  if (rgba->alpha > 0.999)
    {
      return g_strdup_printf ("rgb(%d,%d,%d)",
                              (int)(0.5 + CLAMP (rgba->red, 0., 1.) * 255.),
                              (int)(0.5 + CLAMP (rgba->green, 0., 1.) * 255.),
                              (int)(0.5 + CLAMP (rgba->blue, 0., 1.) * 255.));
    }
  else
    {
      gchar alpha[G_ASCII_DTOSTR_BUF_SIZE];

      g_ascii_dtostr (alpha, G_ASCII_DTOSTR_BUF_SIZE, CLAMP (rgba->alpha, 0, 1));

      return g_strdup_printf ("rgba(%d,%d,%d,%s)",
                              (int)(0.5 + CLAMP (rgba->red, 0., 1.) * 255.),
                              (int)(0.5 + CLAMP (rgba->green, 0., 1.) * 255.),
                              (int)(0.5 + CLAMP (rgba->blue, 0., 1.) * 255.),
                              alpha);
    }
}
