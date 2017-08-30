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

#include "gskpango.h"

#include <math.h>

#include <pango/pango.h>
#include <cairo/cairo.h>

typedef struct _PangoCairoFontHexBoxInfo             PangoCairoFontHexBoxInfo;

struct _PangoCairoFontHexBoxInfo
{
  PangoCairoFont *font;
  int rows;
  double digit_width;
  double digit_height;
  double pad_x;
  double pad_y;
  double line_width;
  double box_descent;
  double box_height;
};

#define GSK_PANGO_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_PANGO_RENDERER, GskPangoRendererClass))
#define GSK_IS_PANGO_RENDERER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_PANGO_RENDERER))
#define GSK_PANGO_RENDERER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_PANGO_RENDERER, GskPangoRendererClass))

struct _GskPangoRenderer
{
  PangoRenderer parent_instance;

  cairo_t *cr;
  gboolean do_path;
  gboolean has_show_text_glyphs;
  double x_offset, y_offset;

  /* house-keeping options */
  gboolean is_cached_renderer;
  gboolean cr_had_current_point;
};

struct _GskPangoRendererClass
{
  PangoRendererClass parent_class;
};

G_DEFINE_TYPE (GskPangoRenderer, gsk_pango_renderer, PANGO_TYPE_RENDERER)

static void
set_color (GskPangoRenderer *crenderer,
           PangoRenderPart     part)
{
  PangoColor *color = pango_renderer_get_color ((PangoRenderer *) (crenderer), part);
  guint16 a = pango_renderer_get_alpha ((PangoRenderer *) (crenderer), part);
  gdouble red, green, blue, alpha;

  if (!a && !color)
    return;

  if (color)
    {
      red = color->red / 65535.;
      green = color->green / 65535.;
      blue = color->blue / 65535.;
      alpha = 1.;
    }
  else
    {
      cairo_pattern_t *pattern = cairo_get_source (crenderer->cr);

      if (pattern && cairo_pattern_get_type (pattern) == CAIRO_PATTERN_TYPE_SOLID)
        cairo_pattern_get_rgba (pattern, &red, &green, &blue, &alpha);
      else
        {
          red = 0.;
          green = 0.;
          blue = 0.;
          alpha = 1.;
        }
    }

  if (a)
    alpha = a / 65535.;

  cairo_set_source_rgba (crenderer->cr, red, green, blue, alpha);
}

#if 0
/* note: modifies crenderer->cr without doing cairo_save/restore() */
static void
gsk_pango_renderer_draw_frame (GskPangoRenderer *crenderer,
                               double              x,
                               double              y,
                               double              width,
                               double              height,
                               double              line_width,
                               gboolean            invalid)
{
  cairo_t *cr = crenderer->cr;

  if (crenderer->do_path)
    {
      double d2 = line_width * .5, d = line_width;

      /* we draw an outer box in one winding direction and an inner one in the
       * opposite direction.  This works for both cairo windings rules.
       *
       * what we really want is cairo_stroke_to_path(), but that's not
       * implemented in cairo yet.
       */

      /* outer */
      cairo_rectangle (cr, x-d2, y-d2, width+d, height+d);

      /* inner */
      if (invalid)
        {
          /* delicacies of computing the joint... this is REALLY slow */

          double alpha, tan_alpha2, cos_alpha;
          double sx, sy;

          alpha = atan2 (height, width);

          tan_alpha2 = tan (alpha * .5);
          if (tan_alpha2 < 1e-5 || (sx = d2 / tan_alpha2, 2. * sx > width - d))
            sx = (width - d) * .5;

          cos_alpha = cos (alpha);
          if (cos_alpha < 1e-5 || (sy = d2 / cos_alpha, 2. * sy > height - d))
            sy = (height - d) * .5;

          /* top triangle */
          cairo_new_sub_path (cr);
          cairo_line_to (cr, x+width-sx, y+d2);
          cairo_line_to (cr, x+sx, y+d2);
          cairo_line_to (cr, x+.5*width, y+.5*height-sy);
          cairo_close_path (cr);

          /* bottom triangle */
          cairo_new_sub_path (cr);
          cairo_line_to (cr, x+width-sx, y+height-d2);
          cairo_line_to (cr, x+.5*width, y+.5*height+sy);
          cairo_line_to (cr, x+sx, y+height-d2);
          cairo_close_path (cr);


          alpha = G_PI_2 - alpha;
          tan_alpha2 = tan (alpha * .5);
          if (tan_alpha2 < 1e-5 || (sy = d2 / tan_alpha2, 2. * sy > height - d))
            sy = (height - d) * .5;

          cos_alpha = cos (alpha);
          if (cos_alpha < 1e-5 || (sx = d2 / cos_alpha, 2. * sx > width - d))
            sx = (width - d) * .5;

          /* left triangle */
          cairo_new_sub_path (cr);
          cairo_line_to (cr, x+d2, y+sy);
          cairo_line_to (cr, x+d2, y+height-sy);
          cairo_line_to (cr, x+.5*width-sx, y+.5*height);
          cairo_close_path (cr);

          /* right triangle */
          cairo_new_sub_path (cr);
          cairo_line_to (cr, x+width-d2, y+sy);
          cairo_line_to (cr, x+.5*width+sx, y+.5*height);
          cairo_line_to (cr, x+width-d2, y+height-sy);
          cairo_close_path (cr);
        }
      else
        cairo_rectangle (cr, x+width-d2, y+d2, - (width-d), height-d);
    }
  else
    {
      cairo_rectangle (cr, x, y, width, height);

      if (invalid)
        {
          /* draw an X */

          cairo_new_sub_path (cr);
          cairo_move_to (cr, x, y);
          cairo_rel_line_to (cr, width, height);

          cairo_new_sub_path (cr);
          cairo_move_to (cr, x + width, y);
          cairo_rel_line_to (cr, -width, height);

          cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
        }

      cairo_set_line_width (cr, line_width);
      cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER);
      cairo_set_miter_limit (cr, 2.);
      cairo_stroke (cr);
    }
}

static void
gsk_pango_renderer_draw_box_glyph (GskPangoRenderer *crenderer,
                                   PangoGlyphInfo     *gi,
                                   double              cx,
                                   double              cy,
                                   gboolean            invalid)
{
  cairo_save (crenderer->cr);

  gsk_pango_renderer_draw_frame (crenderer,
                                    cx + 1.5,
                                    cy + 1.5 - PANGO_UNKNOWN_GLYPH_HEIGHT,
                                    (double)gi->geometry.width / PANGO_SCALE - 3.0,
                                    PANGO_UNKNOWN_GLYPH_HEIGHT - 3.0,
                                    1.0,
                                    invalid);

  cairo_restore (crenderer->cr);
}
#endif

static gboolean
_pango_cairo_font_install (PangoFont *font,
                           cairo_t   *cr)
{
  cairo_scaled_font_t *scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *)font);

  if (G_UNLIKELY (scaled_font == NULL || cairo_scaled_font_status (scaled_font) != CAIRO_STATUS_SUCCESS))
    return FALSE;

  cairo_set_scaled_font (cr, scaled_font);

  return TRUE;
}

static void
gsk_pango_renderer_draw_unknown_glyph (GskPangoRenderer *crenderer,
                                       PangoFont          *font,
                                       PangoGlyphInfo     *gi,
                                       double              cx,
                                       double              cy)
{
#if 0
  char buf[7];
  double x0, y0;
  int row, col;
  int rows, cols;
  double width, lsb;
  char hexbox_string[2] = {0, 0};
  PangoCairoFontHexBoxInfo *hbi;
  gunichar ch;
  gboolean invalid_input;

  cairo_save (crenderer->cr);

  ch = gi->glyph & ~PANGO_GLYPH_UNKNOWN_FLAG;
  invalid_input = G_UNLIKELY (gi->glyph == PANGO_GLYPH_INVALID_INPUT || ch > 0x10FFFF);

  hbi = pango_cairo_font_get_hex_box_info ((PangoCairoFont *)font);
  if (!hbi || !_pango_cairo_font_install ((PangoFont *)(hbi->font), crenderer->cr))
    {
      gsk_pango_renderer_draw_box_glyph (crenderer, gi, cx, cy, invalid_input);
      goto done;
    }

  rows = hbi->rows;
  if (G_UNLIKELY (invalid_input))
    {
      cols = 1;
    }
  else
    {
      cols = (ch > 0xffff ? 6 : 4) / rows;
      g_snprintf (buf, sizeof(buf), (ch > 0xffff) ? "%06X" : "%04X", ch);
    }

  width = (3 * hbi->pad_x + cols * (hbi->digit_width + hbi->pad_x));
  lsb = ((double)gi->geometry.width / PANGO_SCALE - width) * .5;
  lsb = floor (lsb / hbi->pad_x) * hbi->pad_x;

  gsk_pango_renderer_draw_frame (crenderer,
                                 cx + lsb + .5 * hbi->pad_x,
                                 cy + hbi->box_descent - hbi->box_height + hbi->pad_y * 0.5,
                                 width - hbi->pad_x,
                                 (hbi->box_height - hbi->pad_y),
                                 hbi->line_width,
                                 invalid_input);

  if (invalid_input)
    goto done;

  x0 = cx + lsb + hbi->pad_x * 2;
  y0 = cy + hbi->box_descent - hbi->pad_y * 2;

  for (row = 0; row < rows; row++)
    {
      double y = y0 - (rows - 1 - row) * (hbi->digit_height + hbi->pad_y);
      for (col = 0; col < cols; col++)
        {
          double x = x0 + col * (hbi->digit_width + hbi->pad_x);

          cairo_move_to (crenderer->cr, x, y);

          hexbox_string[0] = buf[row * cols + col];

          if (crenderer->do_path)
              cairo_text_path (crenderer->cr, hexbox_string);
          else
              cairo_show_text (crenderer->cr, hexbox_string);
        }
    }

done:
  cairo_restore (crenderer->cr);
#endif
}

#ifndef STACK_BUFFER_SIZE
#define STACK_BUFFER_SIZE (512 * sizeof (int))
#endif

#define STACK_ARRAY_LENGTH(T) (STACK_BUFFER_SIZE / sizeof(T))

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

  int i, count;
  int x_position = 0;
  cairo_glyph_t *cairo_glyphs;
  cairo_glyph_t stack_glyphs[STACK_ARRAY_LENGTH (cairo_glyph_t)];
  double base_x = crenderer->x_offset + (double)x / PANGO_SCALE;
  double base_y = crenderer->y_offset + (double)y / PANGO_SCALE;

  cairo_save (crenderer->cr);
  if (!crenderer->do_path)
    set_color (crenderer, PANGO_RENDER_PART_FOREGROUND);

  if (!_pango_cairo_font_install (font, crenderer->cr))
    {
      for (i = 0; i < glyphs->num_glyphs; i++)
        {
          PangoGlyphInfo *gi = &glyphs->glyphs[i];

          if (gi->glyph != PANGO_GLYPH_EMPTY)
            {
              double cx = base_x + (double)(x_position + gi->geometry.x_offset) / PANGO_SCALE;
              double cy = gi->geometry.y_offset == 0 ?
                          base_y :
                          base_y + (double)(gi->geometry.y_offset) / PANGO_SCALE;

              gsk_pango_renderer_draw_unknown_glyph (crenderer, font, gi, cx, cy);
            }
          x_position += gi->geometry.width;
        }

      goto done;
    }

  if (glyphs->num_glyphs > (int) G_N_ELEMENTS (stack_glyphs))
    cairo_glyphs = g_new (cairo_glyph_t, glyphs->num_glyphs);
  else
    cairo_glyphs = stack_glyphs;

  count = 0;
  for (i = 0; i < glyphs->num_glyphs; i++)
    {
      PangoGlyphInfo *gi = &glyphs->glyphs[i];

      if (gi->glyph != PANGO_GLYPH_EMPTY)
        {
          double cx = base_x + (double)(x_position + gi->geometry.x_offset) / PANGO_SCALE;
          double cy = gi->geometry.y_offset == 0 ?
                      base_y :
                      base_y + (double)(gi->geometry.y_offset) / PANGO_SCALE;

          if (gi->glyph & PANGO_GLYPH_UNKNOWN_FLAG)
            gsk_pango_renderer_draw_unknown_glyph (crenderer, font, gi, cx, cy);
          else
            {
              cairo_glyphs[count].index = gi->glyph;
              cairo_glyphs[count].x = cx;
              cairo_glyphs[count].y = cy;
              count++;
            }
        }
      x_position += gi->geometry.width;
    }

  if (G_UNLIKELY (crenderer->do_path))
    cairo_glyph_path (crenderer->cr, cairo_glyphs, count);
  else
    if (G_UNLIKELY (clusters))
      cairo_show_text_glyphs (crenderer->cr,
                              text, text_len,
                              cairo_glyphs, count,
                              clusters, num_clusters,
                              backward ? CAIRO_TEXT_CLUSTER_FLAG_BACKWARD : 0);
    else
      cairo_show_glyphs (crenderer->cr, cairo_glyphs, count);

  if (cairo_glyphs != stack_glyphs)
    g_free (cairo_glyphs);

done:
  cairo_restore (crenderer->cr);
}

static void
gsk_pango_renderer_draw_glyphs (PangoRenderer     *renderer,
                                PangoFont         *font,
                                PangoGlyphString  *glyphs,
                                int                x,
                                int                y)
{
  gsk_pango_renderer_show_text_glyphs (renderer,
                                       NULL, 0,
                                       glyphs,
                                       NULL, 0,
                                       FALSE,
                                       font,
                                       x, y);
}

static void
gsk_pango_renderer_draw_glyph_item (PangoRenderer  *renderer,
                                    const char     *text,
                                    PangoGlyphItem *glyph_item,
                                    int             x,
                                    int             y)
{
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);
  PangoFont          *font      = glyph_item->item->analysis.font;
  PangoGlyphString   *glyphs    = glyph_item->glyphs;
  PangoItem          *item      = glyph_item->item;
  gboolean            backward  = (item->analysis.level & 1) != 0;

  PangoGlyphItemIter   iter;
  cairo_text_cluster_t *cairo_clusters;
  cairo_text_cluster_t stack_clusters[STACK_ARRAY_LENGTH (cairo_text_cluster_t)];
  int num_clusters;

  if (!crenderer->has_show_text_glyphs || crenderer->do_path)
    {
      gsk_pango_renderer_show_text_glyphs (renderer,
                                           NULL, 0,
                                           glyphs,
                                           NULL, 0,
                                           FALSE,
                                           font,
                                           x, y);
      return;
    }

  if (glyphs->num_glyphs > (int) G_N_ELEMENTS (stack_clusters))
    cairo_clusters = g_new (cairo_text_cluster_t, glyphs->num_glyphs);
  else
    cairo_clusters = stack_clusters;

  num_clusters = 0;
  if (pango_glyph_item_iter_init_start (&iter, glyph_item, text))
    {
      do {
        int num_bytes, num_glyphs, i;

        num_bytes  = iter.end_index - iter.start_index;
        num_glyphs = backward ? iter.start_glyph - iter.end_glyph : iter.end_glyph - iter.start_glyph;

        if (num_bytes < 1)
          g_warning ("gsk_pango_renderer_draw_glyph_item: bad cluster has num_bytes %d", num_bytes);
        if (num_glyphs < 1)
          g_warning ("gsk_pango_renderer_draw_glyph_item: bad cluster has num_glyphs %d", num_glyphs);

        /* Discount empty and unknown glyphs */
        for (i = MIN (iter.start_glyph, iter.end_glyph+1);
             i < MAX (iter.start_glyph+1, iter.end_glyph);
             i++)
          {
            PangoGlyphInfo *gi = &glyphs->glyphs[i];

            if (gi->glyph == PANGO_GLYPH_EMPTY ||
                gi->glyph & PANGO_GLYPH_UNKNOWN_FLAG)
              num_glyphs--;
          }

        cairo_clusters[num_clusters].num_bytes  = num_bytes;
        cairo_clusters[num_clusters].num_glyphs = num_glyphs;
        num_clusters++;
      } while (pango_glyph_item_iter_next_cluster (&iter));
    }

  gsk_pango_renderer_show_text_glyphs (renderer,
                                         text + item->offset, item->length,
                                         glyphs,
                                         cairo_clusters, num_clusters,
                                         backward,
                                         font,
                                         x, y);

  if (cairo_clusters != stack_clusters)
    g_free (cairo_clusters);
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

  if (!crenderer->do_path)
    {
      cairo_save (crenderer->cr);

      set_color (crenderer, part);
    }

  cairo_rectangle (crenderer->cr,
                   crenderer->x_offset + (double)x / PANGO_SCALE,
                   crenderer->y_offset + (double)y / PANGO_SCALE,
                   (double)width / PANGO_SCALE, (double)height / PANGO_SCALE);

  if (!crenderer->do_path)
    {
      cairo_fill (crenderer->cr);

      cairo_restore (crenderer->cr);
    }
}

static void
gsk_pango_renderer_draw_trapezoid (PangoRenderer     *renderer,
                                     PangoRenderPart    part,
                                     double             y1_,
                                     double             x11,
                                     double             x21,
                                     double             y2,
                                     double             x12,
                                     double             x22)
{
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);
  cairo_t *cr;
  double x, y;

  cr = crenderer->cr;

  cairo_save (cr);

  if (!crenderer->do_path)
    set_color (crenderer, part);

  x = crenderer->x_offset,
  y = crenderer->y_offset;
  cairo_user_to_device_distance (cr, &x, &y);
  cairo_identity_matrix (cr);
  cairo_translate (cr, x, y);

  cairo_move_to (cr, x11, y1_);
  cairo_line_to (cr, x21, y1_);
  cairo_line_to (cr, x22, y2);
  cairo_line_to (cr, x12, y2);
  cairo_close_path (cr);

  if (!crenderer->do_path)
    cairo_fill (cr);

  cairo_restore (cr);
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
  cairo_t *cr = crenderer->cr;

  if (!crenderer->do_path)
    {
      cairo_save (cr);

      set_color (crenderer, PANGO_RENDER_PART_UNDERLINE);

      cairo_new_path (cr);
    }

  draw_error_underline (cr,
                        crenderer->x_offset + (double)x / PANGO_SCALE,
                        crenderer->y_offset + (double)y / PANGO_SCALE,
                        (double)width / PANGO_SCALE, (double)height / PANGO_SCALE);

  if (!crenderer->do_path)
    {
      cairo_fill (cr);

      cairo_restore (cr);
    }
}

static void
gsk_pango_renderer_draw_shape (PangoRenderer  *renderer,
                                 PangoAttrShape *attr,
                                 int             x,
                                 int             y)
{
#if 0
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);
  cairo_t *cr = crenderer->cr;
  PangoLayout *layout;
  GskPangoShapeRendererFunc shape_renderer;
  gpointer                    shape_renderer_data;
  double base_x, base_y;

  layout = pango_renderer_get_layout (renderer);

  if (!layout)
        return;

  shape_renderer = gsk_pango_context_get_shape_renderer (pango_layout_get_context (layout),
                                                           &shape_renderer_data);

  if (!shape_renderer)
    return;

  base_x = crenderer->x_offset + (double)x / PANGO_SCALE;
  base_y = crenderer->y_offset + (double)y / PANGO_SCALE;

  cairo_save (cr);
  if (!crenderer->do_path)
    set_color (crenderer, PANGO_RENDER_PART_FOREGROUND);

  cairo_move_to (cr, base_x, base_y);

  shape_renderer (cr, attr, crenderer->do_path, shape_renderer_data);

  cairo_restore (cr);
#endif
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
      renderer->cr = NULL;
      renderer->do_path = FALSE;
      renderer->has_show_text_glyphs = FALSE;
      renderer->x_offset = 0.;
      renderer->y_offset = 0.;

      G_UNLOCK (cached_renderer);
    }
  else
    g_object_unref (renderer);
}

static void
save_current_point (GskPangoRenderer *renderer)
{
  renderer->cr_had_current_point = cairo_has_current_point (renderer->cr);
  cairo_get_current_point (renderer->cr, &renderer->x_offset, &renderer->y_offset);

  /* abuse save_current_point() to cache cairo_has_show_text_glyphs() result */
  renderer->has_show_text_glyphs = cairo_surface_has_show_text_glyphs (cairo_get_target (renderer->cr));
}

static void
restore_current_point (GskPangoRenderer *renderer)
{
  if (renderer->cr_had_current_point)
    /* XXX should do cairo_set_current_point() when we have that function */
    cairo_move_to (renderer->cr, renderer->x_offset, renderer->y_offset);
  else
    cairo_new_sub_path (renderer->cr);
}


/* convenience wrappers using the default renderer */

static void
gsk_pango_do_layout (cairo_t     *cr,
                     PangoLayout *layout,
                     gboolean     do_path)
{
  GskPangoRenderer *crenderer = acquire_renderer ();
  PangoRenderer *renderer = (PangoRenderer *) crenderer;

  crenderer->cr = cr;
  crenderer->do_path = do_path;
  save_current_point (crenderer);

  pango_renderer_draw_layout (renderer, layout, 0, 0);

  restore_current_point (crenderer);

  release_renderer (crenderer);
}

void
gsk_pango_show_layout (cairo_t     *cr,
                       PangoLayout *layout)
{
  g_return_if_fail (cr != NULL);
  g_return_if_fail (PANGO_IS_LAYOUT (layout));

  gsk_pango_do_layout (cr, layout, FALSE);
}
