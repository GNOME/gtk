/* GDK - The GIMP Drawing Kit
 * gdkvisual.c
 *
 * Copyright 2001 Sun Microsystems Inc.
 *
 * Erwann Chenede <erwann.chenede@sun.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkvisualprivate.h"
#include "gdkscreenprivate.h"


/**
 * SECTION:visuals
 * @Short_description: Low-level display hardware information
 * @Title: Visuals
 *
 * A #GdkVisual describes a particular video hardware display format.
 * It includes information about the number of bits used for each color,
 * the way the bits are translated into an RGB value for display, and
 * the way the bits are stored in memory. For example, a piece of display
 * hardware might support 24-bit color, 16-bit color, or 8-bit color;
 * meaning 24/16/8-bit pixel sizes. For a given pixel size, pixels can
 * be in different formats; for example the “red” element of an RGB pixel
 * may be in the top 8 bits of the pixel, or may be in the lower 4 bits.
 *
 * There are several standard visuals. The visual returned by
 * gdk_screen_get_system_visual() is the system’s default visual, and
 * the visual returned by gdk_screen_get_rgba_visual() should be used for
 * creating windows with an alpha channel.
 *
 * A number of functions are provided for determining the “best” available
 * visual. For the purposes of making this determination, higher bit depths
 * are considered better, and for visuals of the same bit depth,
 * %GDK_VISUAL_PSEUDO_COLOR is preferred at 8bpp, otherwise, the visual
 * types are ranked in the order of(highest to lowest)
 * %GDK_VISUAL_DIRECT_COLOR, %GDK_VISUAL_TRUE_COLOR,
 * %GDK_VISUAL_PSEUDO_COLOR, %GDK_VISUAL_STATIC_COLOR,
 * %GDK_VISUAL_GRAYSCALE, then %GDK_VISUAL_STATIC_GRAY.
 */

G_DEFINE_TYPE (GdkVisual, gdk_visual, G_TYPE_OBJECT)

static void
gdk_visual_init (GdkVisual *visual)
{
}

static void
gdk_visual_finalize (GObject *object)
{
  G_OBJECT_CLASS (gdk_visual_parent_class)->finalize (object);
}

static void
gdk_visual_class_init (GdkVisualClass *visual_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (visual_class);

  object_class->finalize = gdk_visual_finalize;
}

/**
 * gdk_visual_get_visual_type:
 * @visual: A #GdkVisual.
 *
 * Returns the type of visual this is (PseudoColor, TrueColor, etc).
 *
 * Returns: A #GdkVisualType stating the type of @visual.
 *
 * Since: 2.22
 */
GdkVisualType
gdk_visual_get_visual_type (GdkVisual *visual)
{
  g_return_val_if_fail (GDK_IS_VISUAL (visual), 0);

  return visual->type;
}

/**
 * gdk_visual_get_depth:
 * @visual: A #GdkVisual.
 *
 * Returns the bit depth of this visual.
 *
 * Returns: The bit depth of this visual.
 *
 * Since: 2.22
 */
gint
gdk_visual_get_depth (GdkVisual *visual)
{
  g_return_val_if_fail (GDK_IS_VISUAL (visual), 0);

  return visual->depth;
}

static void
gdk_visual_get_pixel_details (GdkVisual *visual,
                              gulong     pixel_mask,
                              guint32   *mask,
                              gint      *shift,
                              gint      *precision)
{
  gulong m = 0;
  gint s = 0;
  gint p = 0;

  if (pixel_mask != 0)
    {
      m = pixel_mask;
      while (!(m & 0x1))
        {
          s++;
          m >>= 1;
        }

      while (m & 0x1)
        {
          p++;
          m >>= 1;
        }
    }

  if (mask)
    *mask = pixel_mask;

  if (shift)
    *shift = s;

  if (precision)
    *precision = p;
}

/**
 * gdk_visual_get_red_pixel_details:
 * @visual: A #GdkVisual
 * @mask: (out) (allow-none): A pointer to a #guint32 to be filled in, or %NULL
 * @shift: (out) (allow-none): A pointer to a #gint to be filled in, or %NULL
 * @precision: (out) (allow-none): A pointer to a #gint to be filled in, or %NULL
 *
 * Obtains values that are needed to calculate red pixel values in TrueColor
 * and DirectColor. The “mask” is the significant bits within the pixel.
 * The “shift” is the number of bits left we must shift a primary for it
 * to be in position (according to the "mask"). Finally, "precision" refers
 * to how much precision the pixel value contains for a particular primary.
 *
 * Since: 2.22
 */
void
gdk_visual_get_red_pixel_details (GdkVisual *visual,
                                  guint32   *mask,
                                  gint      *shift,
                                  gint      *precision)
{
  g_return_if_fail (GDK_IS_VISUAL (visual));

  gdk_visual_get_pixel_details (visual, visual->red_mask, mask, shift, precision);
}

/**
 * gdk_visual_get_green_pixel_details:
 * @visual: a #GdkVisual
 * @mask: (out) (allow-none): A pointer to a #guint32 to be filled in, or %NULL
 * @shift: (out) (allow-none): A pointer to a #gint to be filled in, or %NULL
 * @precision: (out) (allow-none): A pointer to a #gint to be filled in, or %NULL
 *
 * Obtains values that are needed to calculate green pixel values in TrueColor
 * and DirectColor. The “mask” is the significant bits within the pixel.
 * The “shift” is the number of bits left we must shift a primary for it
 * to be in position (according to the "mask"). Finally, "precision" refers
 * to how much precision the pixel value contains for a particular primary.
 *
 * Since: 2.22
 */
void
gdk_visual_get_green_pixel_details (GdkVisual *visual,
                                    guint32   *mask,
                                    gint      *shift,
                                    gint      *precision)
{
  g_return_if_fail (GDK_IS_VISUAL (visual));

  gdk_visual_get_pixel_details (visual, visual->green_mask, mask, shift, precision);
}

/**
 * gdk_visual_get_blue_pixel_details:
 * @visual: a #GdkVisual
 * @mask: (out) (allow-none): A pointer to a #guint32 to be filled in, or %NULL
 * @shift: (out) (allow-none): A pointer to a #gint to be filled in, or %NULL
 * @precision: (out) (allow-none): A pointer to a #gint to be filled in, or %NULL
 *
 * Obtains values that are needed to calculate blue pixel values in TrueColor
 * and DirectColor. The “mask” is the significant bits within the pixel.
 * The “shift” is the number of bits left we must shift a primary for it
 * to be in position (according to the "mask"). Finally, "precision" refers
 * to how much precision the pixel value contains for a particular primary.
 *
 * Since: 2.22
 */
void
gdk_visual_get_blue_pixel_details (GdkVisual *visual,
                                   guint32   *mask,
                                   gint      *shift,
                                   gint      *precision)
{
  g_return_if_fail (GDK_IS_VISUAL (visual));

  gdk_visual_get_pixel_details (visual, visual->blue_mask, mask, shift, precision);
}

/**
 * gdk_visual_get_screen:
 * @visual: a #GdkVisual
 *
 * Gets the screen to which this visual belongs
 *
 * Returns: (transfer none): the screen to which this visual belongs.
 *
 * Since: 2.2
 */
GdkScreen *
gdk_visual_get_screen (GdkVisual *visual)
{
  g_return_val_if_fail (GDK_IS_VISUAL (visual), NULL);

  return visual->screen;
}
