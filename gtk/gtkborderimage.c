/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *          Cosimo Cecchi <cosimoc@gnome.org>
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

#include <config.h>
#include <cairo-gobject.h>

#include <math.h>

#include "gtkborderimageprivate.h"

static GtkBorderImageRepeat *
_gtk_border_image_repeat_copy (const GtkBorderImageRepeat *repeat)
{
  GtkBorderImageRepeat *retval;

  retval = g_slice_new0 (GtkBorderImageRepeat);
  *retval = *repeat;

  return retval;
}

static void
_gtk_border_image_repeat_free (GtkBorderImageRepeat *repeat)
{
  g_slice_free (GtkBorderImageRepeat, repeat);
}

G_DEFINE_BOXED_TYPE (GtkBorderImageRepeat, _gtk_border_image_repeat,
                     _gtk_border_image_repeat_copy, _gtk_border_image_repeat_free)

G_DEFINE_BOXED_TYPE (GtkBorderImage, _gtk_border_image,
                     _gtk_border_image_ref, _gtk_border_image_unref)

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

struct _GtkBorderImage {
  cairo_pattern_t *source;
  GtkGradient *source_gradient;

  GtkBorder slice;
  GtkBorderImageRepeat repeat;

  gint ref_count;
  gboolean resolved;

  cairo_surface_t *surfaces[BORDER_LAST][BORDER_LAST];
};

static void
_gtk_border_image_ensure_slices (GtkBorderImage *image,
                                 gdouble         width,
                                 gdouble         height)
{
  cairo_surface_t *surface;
  cairo_t *cr;

  if (image->surfaces[0][0] != NULL)
    return;

  if (cairo_pattern_get_type (image->source) != CAIRO_PATTERN_TYPE_SURFACE)
    {
      cairo_matrix_t matrix;

      cairo_matrix_init_scale (&matrix, 1 / width, 1 / height);
      cairo_pattern_set_matrix (image->source, &matrix);

      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
      cr = cairo_create (surface);
      cairo_set_source (cr, image->source);
      cairo_paint (cr);

      cairo_destroy (cr);
    }
  else
    {
      cairo_pattern_get_surface (image->source, &surface);
      cairo_surface_reference (surface);

      width = cairo_image_surface_get_width (surface);
      height = cairo_image_surface_get_height (surface);
    }

  /* Top /left corner */
  image->surfaces[BORDER_LEFT][BORDER_TOP] =
    cairo_surface_create_similar (surface, CAIRO_CONTENT_COLOR_ALPHA,
                                  image->slice.left, image->slice.top);
  cr = cairo_create (image->surfaces[BORDER_LEFT][BORDER_TOP]);
  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);

  cairo_destroy (cr);

  /* Top/right corner */
  image->surfaces[BORDER_RIGHT][BORDER_TOP] =
    cairo_surface_create_similar (surface, CAIRO_CONTENT_COLOR_ALPHA,
                                  image->slice.right, image->slice.top);
  cr = cairo_create (image->surfaces[BORDER_RIGHT][BORDER_TOP]);
  cairo_set_source_surface (cr, surface,
                            - width + image->slice.right,
                            0);
  cairo_paint (cr);
  cairo_destroy (cr);

  /* Bottom/left corner */
  image->surfaces[BORDER_LEFT][BORDER_BOTTOM] =
    cairo_surface_create_similar (surface, CAIRO_CONTENT_COLOR_ALPHA,
                                  image->slice.left, image->slice.bottom);
  cr = cairo_create (image->surfaces[BORDER_LEFT][BORDER_BOTTOM]);
  cairo_set_source_surface (cr, surface,
                            0,
                            - height + image->slice.bottom);
  cairo_paint (cr);
  cairo_destroy (cr);

  /* Bottom/right corner */
  image->surfaces[BORDER_RIGHT][BORDER_BOTTOM] =
    cairo_surface_create_similar (surface, CAIRO_CONTENT_COLOR_ALPHA,
                                  image->slice.right, image->slice.bottom);
  cr = cairo_create (image->surfaces[BORDER_RIGHT][BORDER_BOTTOM]);
  cairo_set_source_surface (cr, surface,
                            - width + image->slice.right,
                            - height + image->slice.bottom);
  cairo_paint (cr);
  cairo_destroy (cr);

  if ((image->slice.left + image->slice.right) < width)
    {
      /* Top side */
      image->surfaces[BORDER_MIDDLE][BORDER_TOP] =
        cairo_surface_create_similar (surface,
                                      CAIRO_CONTENT_COLOR_ALPHA,
                                      width - image->slice.left - image->slice.right,
                                      image->slice.top);
      cr = cairo_create (image->surfaces[BORDER_MIDDLE][BORDER_TOP]);
      cairo_set_source_surface (cr, surface,
                                - image->slice.left,
                                0);
      cairo_paint (cr);
      cairo_destroy (cr);

      /* Bottom side */
      image->surfaces[BORDER_MIDDLE][BORDER_BOTTOM] =
        cairo_surface_create_similar (surface,
                                      CAIRO_CONTENT_COLOR_ALPHA,
                                      width - image->slice.left - image->slice.right,
                                      image->slice.bottom);
      cr = cairo_create (image->surfaces[BORDER_MIDDLE][BORDER_BOTTOM]);
      cairo_set_source_surface (cr, surface,
                                - image->slice.left,
                                - height + image->slice.bottom);
      cairo_paint (cr);
      cairo_destroy (cr);
    }

  if ((image->slice.top + image->slice.bottom) < height)
    {
      /* Left side */
      image->surfaces[BORDER_LEFT][BORDER_MIDDLE] =
        cairo_surface_create_similar (surface,
                                      CAIRO_CONTENT_COLOR_ALPHA,
                                      image->slice.left,
                                      height - image->slice.top - image->slice.bottom);
      cr = cairo_create (image->surfaces[BORDER_LEFT][BORDER_MIDDLE]);
      cairo_set_source_surface (cr, surface,
                                0,
                                - image->slice.top);
      cairo_paint (cr);
      cairo_destroy (cr);

      /* Right side */
      image->surfaces[BORDER_RIGHT][BORDER_MIDDLE] =
        cairo_surface_create_similar (surface,
                                      CAIRO_CONTENT_COLOR_ALPHA,
                                      image->slice.right,
                                      height - image->slice.top - image->slice.bottom);
      cr = cairo_create (image->surfaces[BORDER_RIGHT][BORDER_MIDDLE]);
      cairo_set_source_surface (cr, surface,
                                - width + image->slice.right,
                                - image->slice.top);
      cairo_paint (cr);
      cairo_destroy (cr);
    }

  cairo_surface_destroy (surface);
}

GtkBorderImage *
_gtk_border_image_new (cairo_pattern_t      *pattern,
                       GtkBorder            *slice,
                       GtkBorderImageRepeat *repeat)
{
  GtkBorderImage *image;

  image = g_slice_new0 (GtkBorderImage);
  image->ref_count = 1;

  if (pattern != NULL)
    image->source = cairo_pattern_reference (pattern);

  if (slice != NULL)
    image->slice = *slice;

  if (repeat != NULL)
    image->repeat = *repeat;

  image->resolved = TRUE;

  return image;
}

GtkBorderImage *
_gtk_border_image_new_for_gradient (GtkGradient          *gradient,
                                    GtkBorder            *slice,
                                    GtkBorderImageRepeat *repeat)
{
  GtkBorderImage *image;

  image = g_slice_new0 (GtkBorderImage);
  image->ref_count = 1;

  if (gradient != NULL)
    image->source_gradient = gtk_gradient_ref (gradient);

  if (slice != NULL)
    image->slice = *slice;

  if (repeat != NULL)
    image->repeat = *repeat;

  image->resolved = FALSE;

  return image;  
}

gboolean
_gtk_border_image_get_resolved (GtkBorderImage *image)
{
  return image->resolved;
}

GtkBorderImage *
_gtk_border_image_resolve (GtkBorderImage     *image,
                           GtkStyleProperties *props)
{
  GtkBorderImage *resolved_image;
  cairo_pattern_t *pattern;

  if (image->resolved)
    return _gtk_border_image_ref (image);

  if (!gtk_gradient_resolve (image->source_gradient, props, &pattern))
    return NULL;

  resolved_image = _gtk_border_image_new (pattern, &image->slice, &image->repeat);

  return resolved_image;
}

GtkBorderImage *
_gtk_border_image_ref (GtkBorderImage *image)
{
  g_return_val_if_fail (image != NULL, NULL);

  image->ref_count++;

  return image;
}

void
_gtk_border_image_unref (GtkBorderImage *image)
{
  g_return_if_fail (image != NULL);

  image->ref_count--;

  if (image->ref_count == 0)
    {
      if (image->source != NULL)
        cairo_pattern_destroy (image->source);

      if (image->source_gradient != NULL)
        gtk_gradient_unref (image->source_gradient);

      g_slice_free (GtkBorderImage, image);
    }
}

GParameter *
_gtk_border_image_unpack (const GValue *value,
                          guint        *n_params)
{
  GParameter *parameter = g_new0 (GParameter, 3);
  GtkBorderImage *image = g_value_get_boxed (value);

  parameter[0].name = "border-image-source";
  g_value_init (&parameter[0].value, CAIRO_GOBJECT_TYPE_PATTERN);
  g_value_set_boxed (&parameter[0].value, image->source);

  parameter[1].name = "border-image-slice";
  g_value_init (&parameter[1].value, GTK_TYPE_BORDER);
  g_value_set_boxed (&parameter[1].value, &image->slice);

  parameter[2].name = "border-image-repeat";
  g_value_init (&parameter[2].value, GTK_TYPE_BORDER_IMAGE_REPEAT);
  g_value_set_boxed (&parameter[2].value, &image->repeat);

  *n_params = 3;
  return parameter;
}

void
_gtk_border_image_pack (GValue             *value,
                        GtkStyleProperties *props,
                        GtkStateFlags       state)
{
  GtkBorderImage *image;
  cairo_pattern_t *source;
  GtkBorder *slice;
  GtkBorderImageRepeat *repeat;

  gtk_style_properties_get (props, state,
                            "border-image-source", &source,
                            "border-image-slice", &slice,
                            "border-image-repeat", &repeat,
                            NULL);

  if (source == NULL)
    {
      g_value_take_boxed (value, NULL);
    }
  else
    {
      image = _gtk_border_image_new (source, slice, repeat);
      g_value_take_boxed (value, image);

      cairo_pattern_destroy (source);
    }

  if (slice != NULL)
    gtk_border_free (slice);

  if (repeat != NULL)
    _gtk_border_image_repeat_free (repeat);
}

static void
render_corner (cairo_t         *cr,
               cairo_surface_t *surface,
               gdouble          x,
               gdouble          y,
               gdouble          width,
               gdouble          height)
{
  gint image_width, image_height;

  if (width == 0 || height == 0)
    return;

  cairo_save (cr);

  image_width = cairo_image_surface_get_width (surface);
  image_height = cairo_image_surface_get_height (surface);

  cairo_translate (cr, x, y);
  cairo_scale (cr,
               width / image_width,
               height / image_height);
  cairo_set_source_surface (cr, surface, 0, 0);

  /* use the nearest filter for scaling, to avoid color blending */
  cairo_pattern_set_filter (cairo_get_source (cr), CAIRO_FILTER_NEAREST);

  cairo_paint (cr);

  cairo_restore (cr);
}

static cairo_surface_t *
create_spaced_surface (cairo_surface_t *tile,
                       gdouble          width,
                       gdouble          height,
                       GtkOrientation   orientation)
{
  gint n_repeats, idx;
  gint tile_width, tile_height;
  gdouble avail_space, step;
  cairo_surface_t *retval;
  cairo_t *cr;

  tile_width = cairo_image_surface_get_width (tile);
  tile_height = cairo_image_surface_get_height (tile);

  n_repeats = (orientation == GTK_ORIENTATION_HORIZONTAL) ?
    (gint) floor (width / tile_width) :
    (gint) floor (height / tile_height);

  avail_space = (orientation == GTK_ORIENTATION_HORIZONTAL) ?
    width - (n_repeats * tile_width) :
    height - (n_repeats * tile_height);
  step = avail_space / (n_repeats + 2);

  retval = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                       width, height);
  cr = cairo_create (retval);
  idx = 0;

  while (idx < n_repeats)
    {
      cairo_save (cr);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        cairo_set_source_surface (cr, tile,
                                  ((idx + 1) * step) + (idx * tile_width), 0);
      else
        cairo_set_source_surface (cr, tile,
                                  0, ((idx + 1 ) * step) + (idx * tile_height));

      cairo_paint (cr);
      cairo_restore (cr);

      idx++;
    }

  cairo_destroy (cr);

  return retval;
}

static void
render_border (cairo_t              *cr,
               cairo_surface_t      *surface,
               gdouble               total_width,
               gdouble               total_height,
               guint                 side,
               GtkBorder            *border_area,
               GtkBorderImageRepeat *repeat)
{
  gdouble target_x, target_y;
  gdouble target_width, target_height;
  gint original_width, original_height;
  GdkRectangle image_area;
  cairo_pattern_t *pattern;
  gboolean repeat_pattern;

  if (surface == NULL)
    return;

  cairo_surface_reference (surface);
  repeat_pattern = FALSE;

  original_width = cairo_image_surface_get_width (surface);
  original_height = cairo_image_surface_get_height (surface);

  if (side == SIDE_TOP || side == SIDE_BOTTOM)
    {
      target_height = (side == SIDE_TOP) ? (border_area->top) : (border_area->bottom);
      target_width = original_width * (target_height / original_height);
    }
  else
    {
      target_width = (side == SIDE_LEFT) ? (border_area->left) : (border_area->right);
      target_height = original_height * (target_width / original_width);
    }

  if (side == SIDE_TOP || side == SIDE_BOTTOM)
    {
      image_area.x = border_area->left;
      image_area.y = (side == SIDE_TOP) ? 0 : (total_height - border_area->bottom);
      image_area.width = total_width - border_area->left - border_area->right;
      image_area.height = (side == SIDE_TOP) ? border_area->top : border_area->bottom;

      target_x = border_area->left;
      target_y = (side == SIDE_TOP) ? 0 : (total_height - border_area->bottom);

      if (repeat->vrepeat == GTK_REPEAT_STYLE_NONE)
        {
          target_width = image_area.width;
        }
      else if (repeat->vrepeat == GTK_REPEAT_STYLE_REPEAT)
        {
          repeat_pattern = TRUE;

          target_x = border_area->left + (total_width - border_area->left - border_area->right) / 2;
          target_y = ((side == SIDE_TOP) ? 0 : (total_height - border_area->bottom)) / 2;
        }
      else if (repeat->vrepeat == GTK_REPEAT_STYLE_ROUND)
        {
          gint n_repeats;

          repeat_pattern = TRUE;

          n_repeats = (gint) floor (image_area.width / original_width);
          target_width = image_area.width / n_repeats;
        }
      else if (repeat->vrepeat == GTK_REPEAT_STYLE_SPACE)
        {
          cairo_surface_t *spaced_surface;

          spaced_surface = create_spaced_surface (surface,
                                                  image_area.width, original_height,
                                                  GTK_ORIENTATION_HORIZONTAL);
          cairo_surface_destroy (surface);
          surface = spaced_surface;

          /* short-circuit hscaling */
          target_width = original_width = cairo_image_surface_get_width (surface);
        }
    }
  else
    {
      image_area.x = (side == SIDE_LEFT) ? 0 : (total_width - border_area->right);
      image_area.y = border_area->top;
      image_area.width = (side == SIDE_LEFT) ? border_area->left : border_area->right;
      image_area.height = total_height - border_area->top - border_area->bottom;

      target_x = (side == SIDE_LEFT) ? 0 : (total_width - border_area->right);
      target_y = border_area->top;

      if (repeat->hrepeat == GTK_REPEAT_STYLE_NONE)
        {
          target_height = total_height - border_area->top - border_area->bottom;
        }
      else if (repeat->hrepeat == GTK_REPEAT_STYLE_REPEAT)
        {
          repeat_pattern = TRUE;

          target_height = total_height - border_area->top - border_area->bottom;
          target_x = (side == SIDE_LEFT) ? 0 : (total_width - border_area->right) / 2;
          target_y = border_area->top + (total_height - border_area->top - border_area->bottom) / 2;
        }
      else if (repeat->hrepeat == GTK_REPEAT_STYLE_ROUND)
        {
          gint n_repeats;

          repeat_pattern = TRUE;

          n_repeats = (gint) floor (image_area.height / original_height);
          target_height = image_area.height / n_repeats;
        }
      else if (repeat->hrepeat == GTK_REPEAT_STYLE_SPACE)
        {
          cairo_surface_t *spaced_surface;

          spaced_surface = create_spaced_surface (surface,
                                                  original_width, image_area.height,
                                                  GTK_ORIENTATION_VERTICAL);
          cairo_surface_destroy (surface);
          surface = spaced_surface;

          /* short-circuit vscaling */
          target_height = original_height = cairo_image_surface_get_height (surface);
        }
    }

  if (target_width == 0 || target_height == 0)
    return;

  cairo_save (cr);

  pattern = cairo_pattern_create_for_surface (surface);

  gdk_cairo_rectangle (cr, &image_area);
  cairo_clip (cr);

  cairo_translate (cr,
                   target_x, target_y);

  /* use the nearest filter for scaling, to avoid color blending */
  cairo_pattern_set_filter (pattern, CAIRO_FILTER_NEAREST);
  
  if (repeat_pattern)
    cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);

  cairo_scale (cr,
               target_width / original_width,
               target_height / original_height);

  cairo_set_source (cr, pattern);
  cairo_paint (cr);

  cairo_restore (cr);

  cairo_pattern_destroy (pattern);
}

void
_gtk_border_image_render (GtkBorderImage   *image,
                          GtkBorder        *border_width,
                          cairo_t          *cr,
                          gdouble           x,
                          gdouble           y,
                          gdouble           width,
                          gdouble           height)
{
  cairo_surface_t *surface;
  
  _gtk_border_image_ensure_slices (image, width, height);

  cairo_save (cr);
  cairo_translate (cr, x, y);

  /* Top side */
  surface = image->surfaces[BORDER_MIDDLE][BORDER_TOP];
  render_border (cr, surface,
                 width, height, SIDE_TOP,
                 border_width, &image->repeat);

  /* Bottom side */
  surface = image->surfaces[BORDER_MIDDLE][BORDER_BOTTOM];
  render_border (cr, surface,
                 width, height, SIDE_BOTTOM,
                 border_width, &image->repeat);

  /* Left side */
  surface = image->surfaces[BORDER_LEFT][BORDER_MIDDLE];
  render_border (cr, surface,
                 width, height, SIDE_LEFT,
                 border_width, &image->repeat);

  /* Right side */
  surface = image->surfaces[BORDER_RIGHT][BORDER_MIDDLE];
  render_border (cr, surface,
                 width, height, SIDE_RIGHT,
                 border_width, &image->repeat);

  /* Top/Left corner */
  surface = image->surfaces[BORDER_LEFT][BORDER_TOP];
  render_corner (cr, surface,
                 0, 0,
                 border_width->left, border_width->top);

  /* Top/right corner */
  surface = image->surfaces[BORDER_RIGHT][BORDER_TOP];
  render_corner (cr, surface,
                 width - border_width->right, 0,
                 border_width->right, border_width->top);

  /* Bottom/left corner */
  surface = image->surfaces[BORDER_LEFT][BORDER_BOTTOM];
  render_corner (cr, surface,
                 0, height - border_width->bottom,
                 border_width->left, border_width->bottom);

  /* Bottom/right corner */
  surface = image->surfaces[BORDER_RIGHT][BORDER_BOTTOM];
  render_corner (cr, surface,
                 width - border_width->right, height - border_width->bottom,
                 border_width->right, border_width->bottom);

  cairo_restore (cr);
}
