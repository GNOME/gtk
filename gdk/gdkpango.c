/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc. 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkpango.h"

#include "gdkscreen.h"
#include "gdkintl.h"

#include <math.h>
#include <pango/pangocairo.h>


/**
 * SECTION:pango_interaction
 * @Short_description: Using Pango in GDK
 * @Title: Pango Interaction
 *
 * Pango is the text layout system used by GDK and GTK+. The functions
 * and types in this section are used to obtain clip regions for
 * #PangoLayouts, and to get #PangoContexts that can be used with
 * GDK.
 *
 * Creating a #PangoLayout object is the first step in rendering text,
 * and requires getting a handle to a #PangoContext. For GTK+ programs,
 * you’ll usually want to use gtk_widget_get_pango_context(), or
 * gtk_widget_create_pango_layout(), rather than using the lowlevel
 * gdk_pango_context_get_for_screen(). Once you have a #PangoLayout, you
 * can set the text and attributes of it with Pango functions like
 * pango_layout_set_text() and get its size with pango_layout_get_size().
 * (Note that Pango uses a fixed point system internally, so converting
 * between Pango units and pixels using [PANGO_SCALE][PANGO-SCALE-CAPS]
 * or the PANGO_PIXELS() macro.)
 *
 * Rendering a Pango layout is done most simply with pango_cairo_show_layout();
 * you can also draw pieces of the layout with pango_cairo_show_layout_line().
 *
 * ## Draw transformed text with Pango and cairo ## {#rotated-example}
 *
 * |[<!-- language="C" -->
 * #define RADIUS 100
 * #define N_WORDS 10
 * #define FONT "Sans Bold 18"
 *
 * PangoContext *context;
 * PangoLayout *layout;
 * PangoFontDescription *desc;
 *
 * double radius;
 * int width, height;
 * int i;
 *
 * // Set up a transformation matrix so that the user space coordinates for
 * // where we are drawing are [-RADIUS, RADIUS], [-RADIUS, RADIUS]
 * // We first center, then change the scale
 *
 * width = gdk_window_get_width (window);
 * height = gdk_window_get_height (window);
 * radius = MIN (width, height) / 2.;
 *
 * cairo_translate (cr,
 *                  radius + (width - 2 * radius) / 2,
 *                  radius + (height - 2 * radius) / 2);
 *                  cairo_scale (cr, radius / RADIUS, radius / RADIUS);
 *
 * // Create a PangoLayout, set the font and text
 * context = gdk_pango_context_get_for_screen (screen);
 * layout = pango_layout_new (context);
 * pango_layout_set_text (layout, "Text", -1);
 * desc = pango_font_description_from_string (FONT);
 * pango_layout_set_font_description (layout, desc);
 * pango_font_description_free (desc);
 *
 * // Draw the layout N_WORDS times in a circle
 * for (i = 0; i < N_WORDS; i++)
 *   {
 *     double red, green, blue;
 *     double angle = 2 * G_PI * i / n_words;
 *
 *     cairo_save (cr);
 *
 *     // Gradient from red at angle == 60 to blue at angle == 300
 *     red = (1 + cos (angle - 60)) / 2;
 *     green = 0;
 *     blue = 1 - red;
 *
 *     cairo_set_source_rgb (cr, red, green, blue);
 *     cairo_rotate (cr, angle);
 *
 *     // Inform Pango to re-layout the text with the new transformation matrix
 *     pango_cairo_update_layout (cr, layout);
 *
 *     pango_layout_get_size (layout, &width, &height);
 *
 *     cairo_move_to (cr, - width / 2 / PANGO_SCALE, - DEFAULT_TEXT_RADIUS);
 *     pango_cairo_show_layout (cr, layout);
 *
 *     cairo_restore (cr);
 *   }
 *
 * g_object_unref (layout);
 * g_object_unref (context);
 * ]|
 *
 * ## Output of the [example][rotated-example] above.
 *
 * ![](rotated-text.png)
 */

/* Get a clip region to draw only part of a layout. index_ranges
 * contains alternating range starts/stops. The region is the
 * region which contains the given ranges, i.e. if you draw with the
 * region as clip, only the given ranges are drawn.
 */
static cairo_region_t*
layout_iter_get_line_clip_region (PangoLayoutIter *iter,
				  gint             x_origin,
				  gint             y_origin,
				  const gint      *index_ranges,
				  gint             n_ranges)
{
  PangoLayoutLine *line;
  cairo_region_t *clip_region;
  PangoRectangle logical_rect;
  gint baseline;
  gint i;

  line = pango_layout_iter_get_line_readonly (iter);

  clip_region = cairo_region_create ();

  pango_layout_iter_get_line_extents (iter, NULL, &logical_rect);
  baseline = pango_layout_iter_get_baseline (iter);

  i = 0;
  while (i < n_ranges)
    {  
      gint *pixel_ranges = NULL;
      gint n_pixel_ranges = 0;
      gint j;

      /* Note that get_x_ranges returns layout coordinates
       */
      if (index_ranges[i*2+1] >= line->start_index &&
	  index_ranges[i*2] < line->start_index + line->length)
	pango_layout_line_get_x_ranges (line,
					index_ranges[i*2],
					index_ranges[i*2+1],
					&pixel_ranges, &n_pixel_ranges);
  
      for (j = 0; j < n_pixel_ranges; j++)
        {
          GdkRectangle rect;
	  int x_off, y_off;
          
          x_off = PANGO_PIXELS (pixel_ranges[2*j] - logical_rect.x);
	  y_off = PANGO_PIXELS (baseline - logical_rect.y);

          rect.x = x_origin + x_off;
          rect.y = y_origin - y_off;
          rect.width = PANGO_PIXELS (pixel_ranges[2*j + 1] - logical_rect.x) - x_off;
          rect.height = PANGO_PIXELS (baseline - logical_rect.y + logical_rect.height) - y_off;

          cairo_region_union_rectangle (clip_region, &rect);
        }

      g_free (pixel_ranges);
      ++i;
    }
  return clip_region;
}

/**
 * gdk_pango_layout_line_get_clip_region: (skip)
 * @line: a #PangoLayoutLine 
 * @x_origin: X pixel where you intend to draw the layout line with this clip
 * @y_origin: baseline pixel where you intend to draw the layout line with this clip
 * @index_ranges: (array): array of byte indexes into the layout,
 *     where even members of array are start indexes and odd elements
 *     are end indexes
 * @n_ranges: number of ranges in @index_ranges, i.e. half the size of @index_ranges
 * 
 * Obtains a clip region which contains the areas where the given
 * ranges of text would be drawn. @x_origin and @y_origin are the top left
 * position of the layout. @index_ranges
 * should contain ranges of bytes in the layout’s text. The clip
 * region will include space to the left or right of the line (to the
 * layout bounding box) if you have indexes above or below the indexes
 * contained inside the line. This is to draw the selection all the way
 * to the side of the layout. However, the clip region is in line coordinates,
 * not layout coordinates.
 *
 * Note that the regions returned correspond to logical extents of the text
 * ranges, not ink extents. So the drawn line may in fact touch areas out of
 * the clip region.  The clip region is mainly useful for highlightling parts
 * of text, such as when text is selected.
 * 
 * Returns: a clip region containing the given ranges
 **/
cairo_region_t*
gdk_pango_layout_line_get_clip_region (PangoLayoutLine *line,
                                       gint             x_origin,
                                       gint             y_origin,
                                       const gint      *index_ranges,
                                       gint             n_ranges)
{
  cairo_region_t *clip_region;
  PangoLayoutIter *iter;
  
  g_return_val_if_fail (line != NULL, NULL);
  g_return_val_if_fail (index_ranges != NULL, NULL);
  
  iter = pango_layout_get_iter (line->layout);
  while (pango_layout_iter_get_line_readonly (iter) != line)
    pango_layout_iter_next_line (iter);
  
  clip_region = layout_iter_get_line_clip_region(iter, x_origin, y_origin, index_ranges, n_ranges);

  pango_layout_iter_free (iter);

  return clip_region;
}

/**
 * gdk_pango_layout_get_clip_region: (skip)
 * @layout: a #PangoLayout 
 * @x_origin: X pixel where you intend to draw the layout with this clip
 * @y_origin: Y pixel where you intend to draw the layout with this clip
 * @index_ranges: array of byte indexes into the layout, where even members of array are start indexes and odd elements are end indexes
 * @n_ranges: number of ranges in @index_ranges, i.e. half the size of @index_ranges
 * 
 * Obtains a clip region which contains the areas where the given ranges
 * of text would be drawn. @x_origin and @y_origin are the top left point
 * to center the layout. @index_ranges should contain
 * ranges of bytes in the layout’s text.
 * 
 * Note that the regions returned correspond to logical extents of the text
 * ranges, not ink extents. So the drawn layout may in fact touch areas out of
 * the clip region.  The clip region is mainly useful for highlightling parts
 * of text, such as when text is selected.
 * 
 * Returns: a clip region containing the given ranges
 **/
cairo_region_t*
gdk_pango_layout_get_clip_region (PangoLayout *layout,
                                  gint         x_origin,
                                  gint         y_origin,
                                  const gint  *index_ranges,
                                  gint         n_ranges)
{
  PangoLayoutIter *iter;  
  cairo_region_t *clip_region;
  
  g_return_val_if_fail (PANGO_IS_LAYOUT (layout), NULL);
  g_return_val_if_fail (index_ranges != NULL, NULL);
  
  clip_region = cairo_region_create ();
  
  iter = pango_layout_get_iter (layout);
  
  do
    {
      PangoRectangle logical_rect;
      cairo_region_t *line_region;
      gint baseline;
      
      pango_layout_iter_get_line_extents (iter, NULL, &logical_rect);
      baseline = pango_layout_iter_get_baseline (iter);      

      line_region = layout_iter_get_line_clip_region(iter, 
						     x_origin + PANGO_PIXELS (logical_rect.x),
						     y_origin + PANGO_PIXELS (baseline),
						     index_ranges,
						     n_ranges);

      cairo_region_union (clip_region, line_region);
      cairo_region_destroy (line_region);
    }
  while (pango_layout_iter_next_line (iter));

  pango_layout_iter_free (iter);

  return clip_region;
}

/**
 * gdk_pango_context_get:
 * 
 * Creates a #PangoContext for the default GDK screen.
 *
 * The context must be freed when you’re finished with it.
 * 
 * When using GTK+, normally you should use gtk_widget_get_pango_context()
 * instead of this function, to get the appropriate context for
 * the widget you intend to render text onto.
 * 
 * The newly created context will have the default font options (see
 * #cairo_font_options_t) for the default screen; if these options
 * change it will not be updated. Using gtk_widget_get_pango_context()
 * is more convenient if you want to keep a context around and track
 * changes to the screen’s font rendering settings.
 *
 * Returns: (transfer full): a new #PangoContext for the default display
 **/
PangoContext *
gdk_pango_context_get (void)
{
  return gdk_pango_context_get_for_screen (gdk_screen_get_default ());
}

/**
 * gdk_pango_context_get_for_screen:
 * @screen: the #GdkScreen for which the context is to be created.
 * 
 * Creates a #PangoContext for @screen.
 *
 * The context must be freed when you’re finished with it.
 * 
 * When using GTK+, normally you should use gtk_widget_get_pango_context()
 * instead of this function, to get the appropriate context for
 * the widget you intend to render text onto.
 * 
 * The newly created context will have the default font options
 * (see #cairo_font_options_t) for the screen; if these options
 * change it will not be updated. Using gtk_widget_get_pango_context()
 * is more convenient if you want to keep a context around and track
 * changes to the screen’s font rendering settings.
 * 
 * Returns: (transfer full): a new #PangoContext for @screen
 *
 * Since: 2.2
 **/
PangoContext *
gdk_pango_context_get_for_screen (GdkScreen *screen)
{
  PangoFontMap *fontmap;
  PangoContext *context;
  const cairo_font_options_t *options;
  double dpi;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  fontmap = pango_cairo_font_map_get_default ();
  context = pango_font_map_create_context (fontmap);

  options = gdk_screen_get_font_options (screen);
  pango_cairo_context_set_font_options (context, options);

  dpi = gdk_screen_get_resolution (screen);
  pango_cairo_context_set_resolution (context, dpi);

  return context;
}
