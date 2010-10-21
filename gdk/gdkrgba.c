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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

/**
 * SECTION:rgba_colors
 * @Short_description: RGBA colors
 * @Title: RGBA Colors
 */

G_DEFINE_BOXED_TYPE (GdkRGBA, gdk_rgba,
                     gdk_rgba_copy, gdk_rgba_free)

/**
 * gdk_rgba_copy:
 * @rgba: a #GdkRGBA
 *
 * Makes a copy of a #GdkRGBA structure, the result must be freed
 * through gdk_rgba_free().
 *
 * Returns: A newly allocated #GdkRGBA
 **/
GdkRGBA *
gdk_rgba_copy (GdkRGBA *rgba)
{
  GdkRGBA *copy;

  copy = g_slice_new (GdkRGBA);
  copy->red = rgba->red;
  copy->green = rgba->green;
  copy->blue = rgba->blue;
  copy->alpha = rgba->alpha;

  return copy;
}

/**
 * gdk_rgba_free:
 * @rgba: a #GdkRGBA
 *
 * Frees a #GdkRGBA struct created with gdk_rgba_copy()
 **/
void
gdk_rgba_free (GdkRGBA *rgba)
{
  g_slice_free (GdkRGBA, rgba);
}

/**
 * gdk_rgba_parse:
 * @spec: the string specifying the color
 * @rgba: the #GdkRGBA struct to fill in
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
 * A hex value in the form '#rgb' '#rrggbb' '#rrrgggbbb' or '#rrrrggggbbbb'
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
 * alpha color values, parsed in the last 2 cases as double numbers in
 * the range [0..1], any other value out of that range will be clamped.
 *
 * Returns: %TRUE if the parsing succeeded
 **/
gboolean
gdk_rgba_parse (const gchar *spec,
                GdkRGBA     *rgba)
{
  gboolean has_alpha;
  gdouble r, g, b, a;
  gchar *str = (gchar *) spec;

#define SKIP_WHITESPACES(s) while (*(s) == ' ') (s)++;

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
       * map and '#' prefixed colors */
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
  r = g_ascii_strtod (str, &str);
  SKIP_WHITESPACES (str);

  if (*str != ',')
    return FALSE;

  str++;

  /* Parse green */
  SKIP_WHITESPACES (str);
  g = g_ascii_strtod (str, &str);
  SKIP_WHITESPACES (str);

  if (*str != ',')
    return FALSE;

  str++;

  /* Parse blue */
  SKIP_WHITESPACES (str);
  b = g_ascii_strtod (str, &str);
  SKIP_WHITESPACES (str);

  if (has_alpha)
    {
      if (*str != ',')
        return FALSE;

      str++;

      SKIP_WHITESPACES (str);
      a = g_ascii_strtod (str, &str);
      SKIP_WHITESPACES (str);
    }

  if (*str != ')')
    return FALSE;

#undef SKIP_WHITESPACES

  if (rgba)
    {
      rgba->red = CLAMP (r, 0, 1);
      rgba->green = CLAMP (g, 0, 1);
      rgba->blue = CLAMP (b, 0, 1);
      rgba->alpha = CLAMP (a, 0, 1);
    }

  return TRUE;
}

/**
 * gdk_rgba_hash:
 * @p: a #GdkRGBA pointer.
 *
 * A hash function suitable for using for a hash
 * table that stores #GdkRGBA<!-- -->s.
 *
 * Return value: The hash function applied to @p
 **/
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
 * @p1: a #GdkRGBA pointer.
 * @p2: another #GdkRGBA pointer.
 *
 * Compares two RGBA colors.
 *
 * Return value: %TRUE if the two colors compare equal
 **/
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
 * <literal>rgba (r, g, b, a)</literal>, where 'r', 'g',
 * 'b' and 'a' represent the red, green, blue and alpha
 * values respectively.
 *
 * Returns: A newly allocated text string
 **/
gchar *
gdk_rgba_to_string (GdkRGBA *rgba)
{
  return g_strdup_printf ("rgba(%f,%f,%f,%f)",
                          CLAMP (rgba->red, 0, 1),
                          CLAMP (rgba->green, 0, 1),
                          CLAMP (rgba->blue, 0, 1),
                          CLAMP (rgba->alpha, 0, 1));
}
