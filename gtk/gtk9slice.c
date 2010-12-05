/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#include "config.h"
#include "gtk9slice.h"

enum {
  BORDER_LEFT,
  BORDER_MIDDLE,
  BORDER_RIGHT,
  BORDER_LAST,
  BORDER_TOP = BORDER_LEFT,
  BORDER_BOTTOM = BORDER_RIGHT
};

enum {
  SIDE_TOP,
  SIDE_RIGHT,
  SIDE_BOTTOM,
  SIDE_LEFT
};

struct Gtk9Slice
{
  cairo_surface_t *surfaces[BORDER_LAST][BORDER_LAST];
  GtkSliceSideModifier modifiers[4];
  gdouble distances[4];
  gint ref_count;
};

G_DEFINE_BOXED_TYPE (Gtk9Slice, _gtk_9slice,
                     _gtk_9slice_ref, _gtk_9slice_unref)


Gtk9Slice *
_gtk_9slice_new (GdkPixbuf            *pixbuf,
                 gdouble               distance_top,
                 gdouble               distance_bottom,
                 gdouble               distance_left,
                 gdouble               distance_right,
                 GtkSliceSideModifier  horizontal_modifier,
                 GtkSliceSideModifier  vertical_modifier)
{
  Gtk9Slice *slice;
  cairo_surface_t *surface;
  gint width, height;
  cairo_t *cr;

  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  slice = g_slice_new0 (Gtk9Slice);
  slice->ref_count = 1;

  slice->distances[SIDE_TOP] = distance_top;
  slice->distances[SIDE_BOTTOM] = distance_bottom;
  slice->distances[SIDE_LEFT] = distance_left;
  slice->distances[SIDE_RIGHT] = distance_right;

  slice->modifiers[SIDE_TOP] = slice->modifiers[SIDE_BOTTOM] = horizontal_modifier;
  slice->modifiers[SIDE_LEFT] = slice->modifiers[SIDE_RIGHT] = vertical_modifier;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  /* Get an image surface from the pixbuf */
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (surface);

  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);

  cairo_destroy (cr);

  /* Top /left corner */
  slice->surfaces[BORDER_LEFT][BORDER_TOP] = cairo_surface_create_similar (surface, CAIRO_CONTENT_COLOR_ALPHA,
                                                                           distance_left, distance_top);
  cr = cairo_create (slice->surfaces[BORDER_LEFT][BORDER_TOP]);

  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);

  cairo_destroy (cr);

  /* Top/right corner */
  slice->surfaces[BORDER_RIGHT][BORDER_TOP] = cairo_surface_create_similar (surface, CAIRO_CONTENT_COLOR_ALPHA,
                                                                            distance_right, distance_top);
  cr = cairo_create (slice->surfaces[BORDER_RIGHT][BORDER_TOP]);

  cairo_set_source_surface (cr, surface, - width + distance_right, 0);
  cairo_paint (cr);

  cairo_destroy (cr);

  /* Bottom/left corner */
  slice->surfaces[BORDER_LEFT][BORDER_BOTTOM] = cairo_surface_create_similar (surface, CAIRO_CONTENT_COLOR_ALPHA,
                                                                              distance_left, distance_bottom);
  cr = cairo_create (slice->surfaces[BORDER_LEFT][BORDER_BOTTOM]);

  cairo_set_source_surface (cr, surface, 0, - height + distance_bottom);
  cairo_paint (cr);

  cairo_destroy (cr);

  /* Bottom/right corner */
  slice->surfaces[BORDER_RIGHT][BORDER_BOTTOM] = cairo_surface_create_similar (surface, CAIRO_CONTENT_COLOR_ALPHA,
                                                                               distance_right, distance_bottom);
  cr = cairo_create (slice->surfaces[BORDER_RIGHT][BORDER_BOTTOM]);

  cairo_set_source_surface (cr, surface, - width + distance_right, - height + distance_bottom);
  cairo_paint (cr);

  cairo_destroy (cr);

  /* Top side */
  slice->surfaces[BORDER_MIDDLE][BORDER_TOP] = cairo_surface_create_similar (surface,
                                                                             CAIRO_CONTENT_COLOR_ALPHA,
                                                                             width - distance_left - distance_right,
                                                                             distance_top);
  cr = cairo_create (slice->surfaces[BORDER_MIDDLE][BORDER_TOP]);
  cairo_set_source_surface (cr, surface, - distance_left, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  /* Bottom side */
  slice->surfaces[BORDER_MIDDLE][BORDER_BOTTOM] = cairo_surface_create_similar (surface,
                                                                                CAIRO_CONTENT_COLOR_ALPHA,
                                                                                width - distance_left - distance_right,
                                                                                distance_bottom);
  cr = cairo_create (slice->surfaces[BORDER_MIDDLE][BORDER_BOTTOM]);
  cairo_set_source_surface (cr, surface, - distance_left, - height + distance_bottom);
  cairo_paint (cr);
  cairo_destroy (cr);

  /* Left side */
  slice->surfaces[BORDER_LEFT][BORDER_MIDDLE] = cairo_surface_create_similar (surface,
                                                                              CAIRO_CONTENT_COLOR_ALPHA,
                                                                              distance_left,
                                                                              height - distance_top - distance_bottom);
  cr = cairo_create (slice->surfaces[BORDER_LEFT][BORDER_MIDDLE]);
  cairo_set_source_surface (cr, surface, 0, - distance_top);
  cairo_paint (cr);
  cairo_destroy (cr);

  /* Right side */
  slice->surfaces[BORDER_RIGHT][BORDER_MIDDLE] = cairo_surface_create_similar (surface,
                                                                               CAIRO_CONTENT_COLOR_ALPHA,
                                                                               distance_right,
                                                                               height - distance_top - distance_bottom);
  cr = cairo_create (slice->surfaces[BORDER_RIGHT][BORDER_MIDDLE]);
  cairo_set_source_surface (cr, surface, - width + distance_right, - distance_top);
  cairo_paint (cr);
  cairo_destroy (cr);

  /* Center */
  slice->surfaces[BORDER_MIDDLE][BORDER_MIDDLE] = cairo_surface_create_similar (surface,
                                                                                CAIRO_CONTENT_COLOR_ALPHA,
                                                                                width - distance_left - distance_right,
                                                                                height - distance_top - distance_bottom);
  cr = cairo_create (slice->surfaces[BORDER_MIDDLE][BORDER_MIDDLE]);
  cairo_set_source_surface (cr, surface, - distance_left, - distance_top);
  cairo_paint (cr);
  cairo_destroy (cr);

  cairo_surface_destroy (surface);

  return slice;
}

static void
render_border (cairo_t              *cr,
               cairo_surface_t      *surface,
               guint                 side,
               gdouble               x,
               gdouble               y,
               gdouble               width,
               gdouble               height,
               GtkSliceSideModifier  modifier)
{
  cairo_pattern_t *pattern;
  cairo_matrix_t matrix;

  cairo_save (cr);

  cairo_rectangle (cr, x, y, width, height);
  cairo_clip (cr);

  pattern = cairo_pattern_create_for_surface (surface);

  if (modifier == GTK_SLICE_REPEAT)
    {
      cairo_matrix_init_translate (&matrix, - x, - y);
      cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);

      cairo_pattern_set_matrix (pattern, &matrix);
      cairo_set_source (cr, pattern);

      cairo_rectangle (cr, x, y, width, height);
      cairo_fill (cr);
    }
  else
    {
      /* Use nearest filter so borders aren't blurred */
      cairo_pattern_set_filter (pattern, CAIRO_FILTER_NEAREST);

      if (side == SIDE_TOP || side == SIDE_BOTTOM)
        {
          gint w = cairo_image_surface_get_width (surface);

          cairo_translate (cr, x, y);
          cairo_scale (cr, width / w, 1.0);
        }
      else
        {
          gint h = cairo_image_surface_get_height (surface);

          cairo_translate (cr, x, y);
          cairo_scale (cr, 1.0, height / h);
        }

      cairo_set_source (cr, pattern);
      cairo_rectangle (cr, x, y, width, height);
      cairo_paint (cr);
    }

  cairo_restore (cr);

  cairo_pattern_destroy (pattern);
}

static void
render_corner (cairo_t         *cr,
               cairo_surface_t *surface,
               gdouble          x,
               gdouble          y,
               gdouble          width,
               gdouble          height)
{
  cairo_save (cr);

  cairo_rectangle (cr, x, y, width, height);
  cairo_clip (cr);

  cairo_set_source_surface (cr, surface, x, y);
  cairo_rectangle (cr, x, y, width, height);
  cairo_fill (cr);

  cairo_restore (cr);
}

void
_gtk_9slice_render (Gtk9Slice *slice,
                    cairo_t   *cr,
                    gdouble    x,
                    gdouble    y,
                    gdouble    width,
                    gdouble    height)
{
  int img_width, img_height;
  cairo_surface_t *surface;

  cairo_save (cr);

  /* Top side */
  surface = slice->surfaces[BORDER_MIDDLE][BORDER_TOP];
  img_height = cairo_image_surface_get_height (surface);

  render_border (cr, surface, SIDE_TOP,
                 x + slice->distances[SIDE_LEFT], y,
                 width - slice->distances[SIDE_LEFT] - slice->distances[SIDE_RIGHT],
                 (gdouble) img_height,
                 slice->modifiers[SIDE_TOP]);

  /* Bottom side */
  surface = slice->surfaces[BORDER_MIDDLE][BORDER_BOTTOM];
  img_height = cairo_image_surface_get_height (surface);

  render_border (cr, surface, SIDE_BOTTOM,
                 x + slice->distances[SIDE_LEFT], y + height - img_height,
                 width - slice->distances[SIDE_LEFT] - slice->distances[SIDE_RIGHT],
                 (gdouble) img_height,
                 slice->modifiers[SIDE_BOTTOM]);

  /* Left side */
  surface = slice->surfaces[BORDER_LEFT][BORDER_MIDDLE];
  img_width = cairo_image_surface_get_width (surface);

  render_border (cr, surface, SIDE_LEFT,
                 x, y + slice->distances[SIDE_TOP],
                 (gdouble) img_width,
                 height - slice->distances[SIDE_TOP] - slice->distances[SIDE_BOTTOM],
                 slice->modifiers[SIDE_LEFT]);

  /* Right side */
  surface = slice->surfaces[BORDER_RIGHT][BORDER_MIDDLE];
  img_width = cairo_image_surface_get_width (surface);

  render_border (cr, surface, SIDE_RIGHT,
                 x + width - img_width, y + slice->distances[SIDE_TOP],
                 (gdouble) img_width,
                 height - slice->distances[SIDE_TOP] - slice->distances[SIDE_BOTTOM],
                 slice->modifiers[SIDE_RIGHT]);

  /* Top/Left corner */
  surface = slice->surfaces[BORDER_LEFT][BORDER_TOP];
  img_width = cairo_image_surface_get_width (surface);
  img_height = cairo_image_surface_get_height (surface);
  render_corner (cr, surface, x, y, (gdouble) img_width, (gdouble) img_height);

  /* Top/right corner */
  surface = slice->surfaces[BORDER_RIGHT][BORDER_TOP];
  img_width = cairo_image_surface_get_width (surface);
  img_height = cairo_image_surface_get_height (surface);
  render_corner (cr, surface, x + width - img_width, y,
                 (gdouble) img_width, (gdouble) img_height);

  /* Bottom/left corner */
  surface = slice->surfaces[BORDER_LEFT][BORDER_BOTTOM];
  img_width = cairo_image_surface_get_width (surface);
  img_height = cairo_image_surface_get_height (surface);
  render_corner (cr, surface, x, y + height - img_height,
                 (gdouble) img_width, (gdouble) img_height);

  /* Bottom/right corner */
  surface = slice->surfaces[BORDER_RIGHT][BORDER_BOTTOM];
  img_width = cairo_image_surface_get_width (surface);
  img_height = cairo_image_surface_get_height (surface);
  render_corner (cr, surface, x + width - img_width, y + height - img_height,
                 (gdouble) img_width, (gdouble) img_height);

  cairo_restore (cr);
}

Gtk9Slice *
_gtk_9slice_ref (Gtk9Slice *slice)
{
  g_return_val_if_fail (slice != NULL, NULL);

  slice->ref_count++;
  return slice;
}

void
_gtk_9slice_unref (Gtk9Slice *slice)
{
  g_return_if_fail (slice != NULL);

  slice->ref_count--;

  if (slice->ref_count == 0)
    {
      gint i, j;

      for (i = 0; i < BORDER_LAST; i++)
        for (j = 0; j < BORDER_LAST; j++)
          cairo_surface_destroy (slice->surfaces[i][j]);

      g_slice_free (Gtk9Slice, slice);
    }
}
