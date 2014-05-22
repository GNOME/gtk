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

#include "gdkcolor.h"

#include "gdkscreen.h"
#include "gdkinternals.h"

#include <time.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * SECTION:colors
 * @Short_description: Manipulation of colors
 * @Title: Colors
 *
 * A #GdkColor represents a color.
 *
 * When working with cairo, it is often more convenient
 * to use a #GdkRGBA instead, and #GdkColor has been
 * deprecated in favor of #GdkRGBA.
 */


/**
 * gdk_color_copy:
 * @color: a #GdkColor
 *
 * Makes a copy of a #GdkColor.
 *
 * The result must be freed using gdk_color_free().
 *
 * Returns: a copy of @color
 *
 * Deprecated: 3.14: Use #GdkRGBA
 */
GdkColor*
gdk_color_copy (const GdkColor *color)
{
  GdkColor *new_color;

  g_return_val_if_fail (color != NULL, NULL);

  new_color = g_slice_new (GdkColor);
  *new_color = *color;
  return new_color;
}

/**
 * gdk_color_free:
 * @color: a #GdkColor
 *
 * Frees a #GdkColor created with gdk_color_copy().
 *
 * Deprecated: 3.14: Use #GdkRGBA
 */
void
gdk_color_free (GdkColor *color)
{
  g_return_if_fail (color != NULL);

  g_slice_free (GdkColor, color);
}

/**
 * gdk_color_hash:
 * @color: a #GdkColor
 *
 * A hash function suitable for using for a hash
 * table that stores #GdkColors.
 *
 * Returns: The hash function applied to @color
 *
 * Deprecated: 3.14: Use #GdkRGBA
 */
guint
gdk_color_hash (const GdkColor *color)
{
  return ((color->red) +
          (color->green << 11) +
          (color->blue << 22) +
          (color->blue >> 6));
}

/**
 * gdk_color_equal:
 * @colora: a #GdkColor
 * @colorb: another #GdkColor
 *
 * Compares two colors.
 *
 * Returns: %TRUE if the two colors compare equal
 *
 * Deprecated: 3.14: Use #GdkRGBA
 */
gboolean
gdk_color_equal (const GdkColor *colora,
                 const GdkColor *colorb)
{
  g_return_val_if_fail (colora != NULL, FALSE);
  g_return_val_if_fail (colorb != NULL, FALSE);

  return ((colora->red == colorb->red) &&
          (colora->green == colorb->green) &&
          (colora->blue == colorb->blue));
}

G_DEFINE_BOXED_TYPE (GdkColor, gdk_color,
                     gdk_color_copy,
                     gdk_color_free)

/**
 * gdk_color_parse:
 * @spec: the string specifying the color
 * @color: (out): the #GdkColor to fill in
 *
 * Parses a textual specification of a color and fill in the
 * @red, @green, and @blue fields of a #GdkColor.
 *
 * The string can either one of a large set of standard names
 * (taken from the X11 `rgb.txt` file), or it can be a hexadecimal
 * value in the form “\#rgb” “\#rrggbb”, “\#rrrgggbbb” or
 * “\#rrrrggggbbbb” where “r”, “g” and “b” are hex digits of
 * the red, green, and blue components of the color, respectively.
 * (White in the four forms is “\#fff”, “\#ffffff”, “\#fffffffff”
 * and “\#ffffffffffff”).
 *
 * Returns: %TRUE if the parsing succeeded
 *
 * Deprecated: 3.14: Use #GdkRGBA
 */
gboolean
gdk_color_parse (const gchar *spec,
                 GdkColor    *color)
{
  PangoColor pango_color;

  if (pango_color_parse (&pango_color, spec))
    {
      color->red = pango_color.red;
      color->green = pango_color.green;
      color->blue = pango_color.blue;

      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gdk_color_to_string:
 * @color: a #GdkColor
 *
 * Returns a textual specification of @color in the hexadecimal
 * form “\#rrrrggggbbbb” where “r”, “g” and “b” are hex digits
 * representing the red, green and blue components respectively.
 *
 * The returned string can be parsed by gdk_color_parse().
 *
 * Returns: a newly-allocated text string
 *
 * Since: 2.12
 *
 * Deprecated: 3.14: Use #GdkRGBA
 */
gchar *
gdk_color_to_string (const GdkColor *color)
{
  PangoColor pango_color;

  g_return_val_if_fail (color != NULL, NULL);

  pango_color.red = color->red;
  pango_color.green = color->green;
  pango_color.blue = color->blue;

  return pango_color_to_string (&pango_color);
}
