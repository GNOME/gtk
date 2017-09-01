/*
 * Copyright (C) 2017 Red Hat, Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gsk/gsk.h"
#include "gskpango.h"

#include <math.h>

#include <pango/pango.h>
#include <cairo/cairo.h>

#define GSK_PANGO_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_PANGO_RENDERER, GskPangoRendererClass))
#define GSK_IS_PANGO_RENDERER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_PANGO_RENDERER))
#define GSK_PANGO_RENDERER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_PANGO_RENDERER, GskPangoRendererClass))

/*
 * This is a PangoRenderer implementation that translates all the draw calls to
 * gsk render nodes, using the GtkSnapshot helper class. Glyphs are translated
 * to text nodes, all other draw calls fall back to cairo nodes.
 */

struct _GskPangoRenderer
{
  PangoRenderer parent_instance;

  GtkSnapshot *snapshot;
  GdkRGBA fg_color;
  graphene_rect_t bounds;

  /* house-keeping options */
  gboolean is_cached_renderer;
};

struct _GskPangoRendererClass
{
  PangoRendererClass parent_class;
};

G_DEFINE_TYPE (GskPangoRenderer, gsk_pango_renderer, PANGO_TYPE_RENDERER)

static void
get_color (GskPangoRenderer *crenderer,
           PangoRenderPart   part,
           GdkRGBA          *rgba)
{
  PangoColor *color = pango_renderer_get_color ((PangoRenderer *) (crenderer), part);
  guint16 a = pango_renderer_get_alpha ((PangoRenderer *) (crenderer), part);
  gdouble red, green, blue, alpha;

  red = crenderer->fg_color.red;
  green = crenderer->fg_color.green;
  blue = crenderer->fg_color.blue;
  alpha = crenderer->fg_color.alpha;

  if (color)
    {
      red = color->red / 65535.;
      green = color->green / 65535.;
      blue = color->blue / 65535.;
      alpha = 1.;
    }

  if (a)
    alpha = a / 65535.;

  rgba->red = red;
  rgba->green = green;
  rgba->blue = blue;
  rgba->alpha = alpha;
}

static void
set_color (GskPangoRenderer *crenderer,
           PangoRenderPart   part,
           cairo_t          *cr)
{
  GdkRGBA rgba = { 0, 0, 0, 1 };

  get_color (crenderer, part, &rgba);
  gdk_cairo_set_source_rgba (cr, &rgba);
}

static void
gsk_pango_renderer_show_text_glyphs (PangoRenderer        *renderer,
                                     const char           *text,
                                     int                   text_len,
                                     PangoGlyphString     *glyphs,
                                     cairo_text_cluster_t *clusters,
                                     int                   num_clusters,
                                     gboolean              backward,
                                     PangoFont            *font,
                                     int                   x,
                                     int                   y)
{
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);
  double base_x = (double)x / PANGO_SCALE;
  double base_y = (double)y / PANGO_SCALE;
  int x_offset, y_offset;
  GskRenderNode *node;
  char name[64];

  gtk_snapshot_get_offset (crenderer->snapshot, &x_offset, &y_offset);

  gtk_snapshot_offset (crenderer->snapshot, base_x, base_y);

  node = gsk_text_node_new (font, glyphs, &crenderer->fg_color, x_offset, y_offset, base_x, base_y);
  snprintf (name, sizeof (name), "Glyphs<%d>", glyphs->num_glyphs);
  gsk_render_node_set_name (node, name);
  gtk_snapshot_append_node (crenderer->snapshot, node);
  gsk_render_node_unref (node);

  gtk_snapshot_offset (crenderer->snapshot, -base_x, -base_y);
}

static void
gsk_pango_renderer_draw_glyphs (PangoRenderer    *renderer,
                                PangoFont        *font,
                                PangoGlyphString *glyphs,
                                int               x,
                                int               y)
{
  gsk_pango_renderer_show_text_glyphs (renderer, NULL, 0, glyphs, NULL, 0, FALSE, font, x, y);
}

static void
gsk_pango_renderer_draw_glyph_item (PangoRenderer  *renderer,
                                    const char     *text,
                                    PangoGlyphItem *glyph_item,
                                    int             x,
                                    int             y)
{
  PangoFont *font = glyph_item->item->analysis.font;
  PangoGlyphString *glyphs = glyph_item->glyphs;

  gsk_pango_renderer_show_text_glyphs (renderer, NULL, 0, glyphs, NULL, 0, FALSE, font, x, y);
}

static void
gsk_pango_renderer_draw_rectangle (PangoRenderer     *renderer,
                                   PangoRenderPart    part,
                                   int                x,
                                   int                y,
                                   int                width,
                                   int                height)
{
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);
  GdkRGBA rgba;
  graphene_rect_t bounds;

  get_color (crenderer, part, &rgba);

  graphene_rect_init (&bounds,
                      (double)x / PANGO_SCALE, (double)y / PANGO_SCALE,
                      (double)width / PANGO_SCALE, (double)height / PANGO_SCALE);

  gtk_snapshot_append_color (crenderer->snapshot, &rgba, &bounds, "DrawRectangle");
}

static void
gsk_pango_renderer_draw_trapezoid (PangoRenderer   *renderer,
                                   PangoRenderPart  part,
                                   double           y1_,
                                   double           x11,
                                   double           x21,
                                   double           y2,
                                   double           x12,
                                   double           x22)
{
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);
  cairo_t *cr;
  gdouble x, y;

  cr = gtk_snapshot_append_cairo (crenderer->snapshot, &crenderer->bounds, "DrawTrapezoid");

  set_color (crenderer, part, cr);

  x = y = 0;
  cairo_user_to_device_distance (cr, &x, &y);
  cairo_identity_matrix (cr);
  cairo_translate (cr, x, y);

  cairo_move_to (cr, x11, y1_);
  cairo_line_to (cr, x21, y1_);
  cairo_line_to (cr, x22, y2);
  cairo_line_to (cr, x12, y2);
  cairo_close_path (cr);

  cairo_fill (cr);

  cairo_destroy (cr);
}

/* Draws an error underline that looks like one of:
 *              H       E                H
 *     /\      /\      /\        /\      /\               -
 *   A/  \    /  \    /  \     A/  \    /  \              |
 *    \   \  /    \  /   /D     \   \  /    \             |
 *     \   \/  C   \/   /        \   \/   C  \            | height = HEIGHT_SQUARES * square
 *      \      /\  F   /          \  F   /\   \           |
 *       \    /  \    /            \    /  \   \G         |
 *        \  /    \  /              \  /    \  /          |
 *         \/      \/                \/      \/           -
 *         B                         B
 *         |---|
 *       unit_width = (HEIGHT_SQUARES - 1) * square
 *
 * The x, y, width, height passed in give the desired bounding box;
 * x/width are adjusted to make the underline a integer number of units
 * wide.
 */
#define HEIGHT_SQUARES 2.5

static void
draw_error_underline (cairo_t *cr,
                      double   x,
                      double   y,
                      double   width,
                      double   height)
{
  double square = height / HEIGHT_SQUARES;
  double unit_width = (HEIGHT_SQUARES - 1) * square;
  double double_width = 2 * unit_width;
  int width_units = (width + unit_width / 2) / unit_width;
  double y_top, y_bottom;
  double x_left, x_middle, x_right;
  int i;

  x += (width - width_units * unit_width) / 2;

  y_top = y;
  y_bottom = y + height;

  /* Bottom of squiggle */
  x_middle = x + unit_width;
  x_right  = x + double_width;
  cairo_move_to (cr, x - square / 2, y_top + square / 2); /* A */
  for (i = 0; i < width_units-2; i += 2)
    {
      cairo_line_to (cr, x_middle, y_bottom); /* B */
      cairo_line_to (cr, x_right, y_top + square); /* C */

      x_middle += double_width;
      x_right  += double_width;
    }
  cairo_line_to (cr, x_middle, y_bottom); /* B */

  if (i + 1 == width_units)
    cairo_line_to (cr, x_middle + square / 2, y_bottom - square / 2); /* G */
  else if (i + 2 == width_units) {
    cairo_line_to (cr, x_right + square / 2, y_top + square / 2); /* D */
    cairo_line_to (cr, x_right, y_top); /* E */
  }

  /* Top of squiggle */
  x_left = x_middle - unit_width;
  for (; i >= 0; i -= 2)
    {
      cairo_line_to (cr, x_middle, y_bottom - square); /* F */
      cairo_line_to (cr, x_left, y_top);   /* H */

      x_left   -= double_width;
      x_middle -= double_width;
    }
}

static void
gsk_pango_renderer_draw_error_underline (PangoRenderer *renderer,
                                         int            x,
                                         int            y,
                                         int            width,
                                         int            height)
{
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);
  cairo_t *cr;

  cr = gtk_snapshot_append_cairo (crenderer->snapshot, &crenderer->bounds, "DrawTrapezoid");

  set_color (crenderer, PANGO_RENDER_PART_UNDERLINE, cr);

  cairo_new_path (cr);

  draw_error_underline (cr,
                        (double)x / PANGO_SCALE, (double)y / PANGO_SCALE,
                        (double)width / PANGO_SCALE, (double)height / PANGO_SCALE);

  cairo_fill (cr);

  cairo_destroy (cr);
}

static void
gsk_pango_renderer_draw_shape (PangoRenderer  *renderer,
                               PangoAttrShape *attr,
                               int             x,
                               int             y)
{
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);
  cairo_t *cr;
  PangoLayout *layout;
  PangoCairoShapeRendererFunc shape_renderer;
  gpointer shape_renderer_data;
  double base_x = (double)x / PANGO_SCALE;
  double base_y = (double)y / PANGO_SCALE;

  cr = gtk_snapshot_append_cairo (crenderer->snapshot, &crenderer->bounds, "DrawShape");

  layout = pango_renderer_get_layout (renderer);
  if (!layout)
    return;

  shape_renderer = pango_cairo_context_get_shape_renderer (pango_layout_get_context (layout),
                                                           &shape_renderer_data);

  if (!shape_renderer)
    return;

  set_color (crenderer, PANGO_RENDER_PART_FOREGROUND, cr);

  cairo_move_to (cr, base_x, base_y);

  shape_renderer (cr, attr, FALSE, shape_renderer_data);

  cairo_destroy (cr);
}

static void
gsk_pango_renderer_init (GskPangoRenderer *renderer G_GNUC_UNUSED)
{
}

static void
gsk_pango_renderer_class_init (GskPangoRendererClass *klass)
{
  PangoRendererClass *renderer_class = PANGO_RENDERER_CLASS (klass);

  renderer_class->draw_glyphs = gsk_pango_renderer_draw_glyphs;
  renderer_class->draw_glyph_item = gsk_pango_renderer_draw_glyph_item;
  renderer_class->draw_rectangle = gsk_pango_renderer_draw_rectangle;
  renderer_class->draw_trapezoid = gsk_pango_renderer_draw_trapezoid;
  renderer_class->draw_error_underline = gsk_pango_renderer_draw_error_underline;
  renderer_class->draw_shape = gsk_pango_renderer_draw_shape;
}

static GskPangoRenderer *cached_renderer = NULL; /* MT-safe */
G_LOCK_DEFINE_STATIC (cached_renderer);

static GskPangoRenderer *
acquire_renderer (void)
{
  GskPangoRenderer *renderer;

  if (G_LIKELY (G_TRYLOCK (cached_renderer)))
    {
      if (G_UNLIKELY (!cached_renderer))
        {
          cached_renderer = g_object_new (GSK_TYPE_PANGO_RENDERER, NULL);
          cached_renderer->is_cached_renderer = TRUE;
        }

      renderer = cached_renderer;
    }
  else
    {
      renderer = g_object_new (GSK_TYPE_PANGO_RENDERER, NULL);
    }

  return renderer;
}

static void
release_renderer (GskPangoRenderer *renderer)
{
  if (G_LIKELY (renderer->is_cached_renderer))
    {
      renderer->snapshot = NULL;

      G_UNLOCK (cached_renderer);
    }
  else
    g_object_unref (renderer);
}

/* convenience wrappers using the default renderer */

void
gsk_pango_show_layout (GtkSnapshot   *snapshot,
                       const GdkRGBA *fg_color,
                       PangoLayout   *layout)
{
  GskPangoRenderer *crenderer;
  PangoRectangle ink_rect;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (PANGO_IS_LAYOUT (layout));

  crenderer = acquire_renderer ();

  crenderer->snapshot = snapshot;
  crenderer->fg_color = *fg_color;

  pango_layout_get_pixel_extents (layout, &ink_rect, NULL);
  graphene_rect_init (&crenderer->bounds, ink_rect.x, ink_rect.y, ink_rect.width, ink_rect.height);

  pango_renderer_draw_layout (PANGO_RENDERER (crenderer), layout, 0, 0);

  release_renderer (crenderer);
}
