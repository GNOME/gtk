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
#include "gsk/gskrendernodeprivate.h"
#include "gskpango.h"
#include "gtksnapshotprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtktextlayoutprivate.h"
#include "gtktextdisplayprivate.h"
#include "gtktextviewprivate.h"
#include "gtkwidgetprivate.h"

#include <math.h>

#include <pango/pango.h>
#include <cairo.h>

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

  GtkWidget *widget;
  GtkSnapshot *snapshot;
  GdkRGBA fg_color;
  graphene_rect_t bounds;

  GdkRGBA *error_color;	/* Error underline color for this widget */

  int state;

  /* house-keeping options */
  gboolean is_cached_renderer;
};

struct _GskPangoRendererClass
{
  PangoRendererClass parent_class;
};

enum {
  NORMAL,
  SELECTED,
  CURSOR
};

G_DEFINE_TYPE (GskPangoRenderer, gsk_pango_renderer, PANGO_TYPE_RENDERER)

static void
gsk_pango_renderer_set_state (GskPangoRenderer *crenderer,
                              int               state)
{
  crenderer->state = state;
}

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
  GdkRGBA color;

  get_color (crenderer, PANGO_RENDER_PART_FOREGROUND, &color);

  gtk_snapshot_append_text (crenderer->snapshot,
                            font,
                            glyphs,
                            &color,
                            (float) x / PANGO_SCALE,
                            (float) y / PANGO_SCALE);
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

  gtk_snapshot_append_color (crenderer->snapshot, &rgba, &bounds);
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

  cr = gtk_snapshot_append_cairo (crenderer->snapshot, &crenderer->bounds);

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

  cr = gtk_snapshot_append_cairo (crenderer->snapshot, &crenderer->bounds);

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

  cr = gtk_snapshot_append_cairo (crenderer->snapshot, &crenderer->bounds);

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
text_renderer_set_rgba (GskPangoRenderer *crenderer,
                        PangoRenderPart   part,
                        const GdkRGBA    *rgba)
{
  PangoRenderer *renderer = PANGO_RENDERER (crenderer);
  PangoColor color = { 0, };
  guint16 alpha;

  if (rgba)
    {
      color.red = (guint16)(rgba->red * 65535);
      color.green = (guint16)(rgba->green * 65535);
      color.blue = (guint16)(rgba->blue * 65535);
      alpha = (guint16)(rgba->alpha * 65535);
      pango_renderer_set_color (renderer, part, &color);
      pango_renderer_set_alpha (renderer, part, alpha);
    }
  else
    {
      pango_renderer_set_color (renderer, part, NULL);
      pango_renderer_set_alpha (renderer, part, 0);
    }
}

static GtkTextAppearance *
get_item_appearance (PangoItem *item)
{
  GSList *tmp_list = item->analysis.extra_attrs;

  while (tmp_list)
    {
      PangoAttribute *attr = tmp_list->data;

      if (attr->klass->type == gtk_text_attr_appearance_type)
	return &((GtkTextAttrAppearance *)attr)->appearance;

      tmp_list = tmp_list->next;
    }

  return NULL;
}

static void
gsk_pango_renderer_prepare_run (PangoRenderer  *renderer,
                                PangoLayoutRun *run)
{
  GtkStyleContext *context;
  GskPangoRenderer *crenderer = GSK_PANGO_RENDERER (renderer);
  GdkRGBA *bg_rgba = NULL;
  GdkRGBA *fg_rgba = NULL;
  GtkTextAppearance *appearance;

  PANGO_RENDERER_CLASS (gsk_pango_renderer_parent_class)->prepare_run (renderer, run);

  appearance = get_item_appearance (run->item);

  if (appearance == NULL)
    return;

  context = gtk_widget_get_style_context (crenderer->widget);

  if (appearance->draw_bg && crenderer->state == NORMAL)
    bg_rgba = appearance->bg_rgba;
  else
    bg_rgba = NULL;

  text_renderer_set_rgba (crenderer, PANGO_RENDER_PART_BACKGROUND, bg_rgba);

  if (crenderer->state == SELECTED)
    {
      GtkCssNode *selection_node;

      selection_node = gtk_text_view_get_selection_node ((GtkTextView *)crenderer->widget);
      gtk_style_context_save_to_node (context, selection_node);

      gtk_style_context_get (context,
                             "color", &fg_rgba,
                             NULL);

      gtk_style_context_restore (context);
    }
  else if (crenderer->state == CURSOR && gtk_widget_has_focus (crenderer->widget))
    {
      gtk_style_context_get (context,
                             "background-color", &fg_rgba,
                              NULL);
    }
  else
    fg_rgba = appearance->fg_rgba;

  text_renderer_set_rgba (crenderer, PANGO_RENDER_PART_FOREGROUND, fg_rgba);

  if (appearance->strikethrough_rgba)
    text_renderer_set_rgba (crenderer, PANGO_RENDER_PART_STRIKETHROUGH, appearance->strikethrough_rgba);
  else
    text_renderer_set_rgba (crenderer, PANGO_RENDER_PART_STRIKETHROUGH, fg_rgba);

  if (appearance->underline_rgba)
    text_renderer_set_rgba (crenderer, PANGO_RENDER_PART_UNDERLINE, appearance->underline_rgba);
  else if (appearance->underline == PANGO_UNDERLINE_ERROR)
    {
      if (!crenderer->error_color)
        {
          static const GdkRGBA red = { 1, 0, 0, 1 };
          crenderer->error_color = gdk_rgba_copy (&red);
        }

      text_renderer_set_rgba (crenderer, PANGO_RENDER_PART_UNDERLINE, crenderer->error_color);
    }
  else
    text_renderer_set_rgba (crenderer, PANGO_RENDER_PART_UNDERLINE, fg_rgba);

  if (fg_rgba != appearance->fg_rgba)
    gdk_rgba_free (fg_rgba);
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
  renderer_class->prepare_run = gsk_pango_renderer_prepare_run;
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
      renderer->widget = NULL;
      renderer->snapshot = NULL;

      if (renderer->error_color)
        {
          gdk_rgba_free (renderer->error_color);
          renderer->error_color = NULL;
        }

      G_UNLOCK (cached_renderer);
    }
  else
    g_object_unref (renderer);
}

/**
 * gtk_snapshot_append_layout:
 * @snapshot: a #GtkSnapshot
 * @layout: the #PangoLayout to render
 * @color: the foreground color to render the layout in
 *
 * Creates render nodes for rendering @layout in the given foregound @color
 * and appends them to the current node of @snapshot without changing the
 * current node.
 **/
void
gtk_snapshot_append_layout (GtkSnapshot   *snapshot,
                            PangoLayout   *layout,
                            const GdkRGBA *color)
{
  GskPangoRenderer *crenderer;
  PangoRectangle ink_rect;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (PANGO_IS_LAYOUT (layout));

  crenderer = acquire_renderer ();

  crenderer->snapshot = snapshot;
  crenderer->fg_color = *color;

  pango_layout_get_pixel_extents (layout, &ink_rect, NULL);
  graphene_rect_init (&crenderer->bounds, ink_rect.x, ink_rect.y, ink_rect.width, ink_rect.height);

  pango_renderer_draw_layout (PANGO_RENDERER (crenderer), layout, 0, 0);

  release_renderer (crenderer);
}

static void
render_para (GskPangoRenderer   *crenderer,
             int                 offset_y,
             GtkTextLineDisplay *line_display,
             int                 selection_start_index,
             int                 selection_end_index)
{
  GtkStyleContext *context;
  PangoLayout *layout = line_display->layout;
  int byte_offset = 0;
  PangoLayoutIter *iter;
  int screen_width;
  GdkRGBA *selection;
  gboolean first = TRUE;
  GtkCssNode *selection_node;
  graphene_point_t point = { 0, offset_y };

  iter = pango_layout_get_iter (layout);
  screen_width = line_display->total_width;

  context = gtk_widget_get_style_context (crenderer->widget);
  selection_node = gtk_text_view_get_selection_node ((GtkTextView*)crenderer->widget);
  gtk_style_context_save_to_node (context, selection_node);

  gtk_style_context_get (context, "background-color", &selection, NULL);

  gtk_style_context_restore (context);

  gtk_snapshot_save (crenderer->snapshot);
  gtk_snapshot_translate (crenderer->snapshot, &point);

  do
    {
      PangoLayoutLine *line = pango_layout_iter_get_line_readonly (iter);
      int selection_y, selection_height;
      int first_y, last_y;
      PangoRectangle line_rect;
      int baseline;
      gboolean at_last_line;
      
      pango_layout_iter_get_line_extents (iter, NULL, &line_rect);
      baseline = pango_layout_iter_get_baseline (iter);
      pango_layout_iter_get_line_yrange (iter, &first_y, &last_y);
      
      /* Adjust for margins */

      line_rect.x += line_display->x_offset * PANGO_SCALE;
      line_rect.y += line_display->top_margin * PANGO_SCALE;
      baseline += line_display->top_margin * PANGO_SCALE;

      /* Selection is the height of the line, plus top/bottom
       * margin if we're the first/last line
       */
      selection_y = PANGO_PIXELS (first_y) + line_display->top_margin;
      selection_height = PANGO_PIXELS (last_y) - PANGO_PIXELS (first_y);

      if (first)
        {
          selection_y -= line_display->top_margin;
          selection_height += line_display->top_margin;
        }

      at_last_line = pango_layout_iter_at_last_line (iter);
      if (at_last_line)
        selection_height += line_display->bottom_margin;
      
      first = FALSE;

      if (selection_start_index < byte_offset &&
          selection_end_index > line->length + byte_offset) /* All selected */
        {
          graphene_rect_t bounds = {
            .origin.x = line_display->left_margin,
            .origin.y = selection_y,
            .size.width = screen_width,
            .size.height = selection_height,
          };

          gtk_snapshot_append_color (crenderer->snapshot, selection, &bounds);

#if 0
          gtk_snapshot_render_background (crenderer->snapshot, context,
                                          line_display->left_margin, selection_y,
                                          screen_width, selection_height);
#endif

          gsk_pango_renderer_set_state (crenderer, SELECTED);
	  pango_renderer_draw_layout_line (PANGO_RENDERER (crenderer),
                                           line, 
                                           line_rect.x,
                                           baseline);

#if 0

          cairo_t *cr = crenderer->cr;

          cairo_save (cr);
          gdk_cairo_set_source_rgba (cr, selection);
          cairo_rectangle (cr, 
                           line_display->left_margin, selection_y,
                           screen_width, selection_height);
          cairo_fill (cr);
          cairo_restore(cr);
#endif
        }
      else
        {
          if (line_display->pg_bg_rgba)
            {
              graphene_rect_t bounds = {
                .origin.x = line_display->left_margin,
                .origin.y = selection_y,
                .size.width = screen_width,
                .size.height = selection_height,
              };

              gtk_snapshot_append_color (crenderer->snapshot,
                                         line_display->pg_bg_rgba,
                                         &bounds);

#if 0

              cairo_t *cr = crenderer->cr;

              cairo_save (cr);
 
	      gdk_cairo_set_source_rgba (crenderer->cr, line_display->pg_bg_rgba);
              cairo_rectangle (cr, 
                               line_display->left_margin, selection_y,
                               screen_width, selection_height);
              cairo_fill (cr);

              cairo_restore (cr);
#endif
            }
        
	  gsk_pango_renderer_set_state (crenderer, NORMAL);
	  pango_renderer_draw_layout_line (PANGO_RENDERER (crenderer),
                                           line, 
                                           line_rect.x,
                                           baseline);

	  /* Check if some part of the line is selected; the newline
	   * that is after line->length for the last line of the
	   * paragraph counts as part of the line for this
	   */
          if ((selection_start_index < byte_offset + line->length ||
	       (selection_start_index == byte_offset + line->length && pango_layout_iter_at_last_line (iter))) &&
	      selection_end_index > byte_offset)
            {
              gint *ranges;
              gint n_ranges, i;

              pango_layout_line_get_x_ranges (line, selection_start_index, selection_end_index, &ranges, &n_ranges);

              gsk_pango_renderer_set_state (crenderer, SELECTED);

              for (i=0; i < n_ranges; i++)
                {
                  graphene_rect_t rect;
                  gint x = line_display->x_offset;
                  gint y = selection_y;
                  gint height = selection_height;

                  rect.origin.x = x + PANGO_PIXELS (ranges[2*i]);
                  rect.origin.y = y;
                  rect.size.width = PANGO_PIXELS (ranges[2*i + 1]) - PANGO_PIXELS (ranges[2*i]);
                  rect.size.height = height;

                  gtk_snapshot_append_color (crenderer->snapshot, selection, &rect);

                  gtk_snapshot_push_clip (crenderer->snapshot, &rect);
                  pango_renderer_draw_layout_line (PANGO_RENDERER (crenderer),
                                                   line, 
                                                   line_rect.x,
                                                   baseline);
                  gtk_snapshot_pop (crenderer->snapshot);
                }

              g_free (ranges);

#if 0

              cairo_t *cr = crenderer->cr;
              cairo_region_t *clip_region = get_selected_clip (crenderer, layout, line,
                                                               line_display->x_offset,
                                                               selection_y,
                                                               selection_height,
                                                               selection_start_index, selection_end_index);

              cairo_save (cr);
              gdk_cairo_region (cr, clip_region);
              cairo_clip (cr);
              cairo_region_destroy (clip_region);

              gdk_cairo_set_source_rgba (cr, selection);
              cairo_rectangle (cr,
                               PANGO_PIXELS (line_rect.x),
                               selection_y,
                               PANGO_PIXELS (line_rect.width),
                               selection_height);
              cairo_fill (cr);
#endif

#if 0
              cairo_restore (cr);
#endif

              /* Paint in the ends of the line */
              if (line_rect.x > line_display->left_margin * PANGO_SCALE &&
                  ((line_display->direction == GTK_TEXT_DIR_LTR && selection_start_index < byte_offset) ||
                   (line_display->direction == GTK_TEXT_DIR_RTL && selection_end_index > byte_offset + line->length)))
                {
                  graphene_rect_t bounds = {
                    .origin.x = line_display->left_margin,
                    .origin.y = selection_y,
                    .size.width = PANGO_PIXELS (line_rect.x) - line_display->left_margin,
                    .size.height = selection_height,
                  };

                  gtk_snapshot_append_color (crenderer->snapshot, selection, &bounds);

#if 0
                  cairo_save (cr);

                  gdk_cairo_set_source_rgba (cr, selection);
                  cairo_rectangle (cr,
                                   line_display->left_margin,
                                   selection_y,
                                   PANGO_PIXELS (line_rect.x) - line_display->left_margin,
                                   selection_height);
                  cairo_fill (cr);

                  cairo_restore (cr);
#endif
                }

              if (line_rect.x + line_rect.width <
                  (screen_width + line_display->left_margin) * PANGO_SCALE &&
                  ((line_display->direction == GTK_TEXT_DIR_LTR && selection_end_index > byte_offset + line->length) ||
                   (line_display->direction == GTK_TEXT_DIR_RTL && selection_start_index < byte_offset)))
                {
                  int nonlayout_width;

                  nonlayout_width =
                    line_display->left_margin + screen_width -
                    PANGO_PIXELS (line_rect.x) - PANGO_PIXELS (line_rect.width);

                  {
                    graphene_rect_t bounds = {
                      .origin.x = PANGO_PIXELS (line_rect.x) + PANGO_PIXELS (line_rect.width),
                      .origin.y = selection_y,
                      .size.width = nonlayout_width,
                      .size.height = selection_height,
                    };

                    gtk_snapshot_append_color (crenderer->snapshot, selection, &bounds);
                  }

#if 0
                  cairo_save (cr);

                  gdk_cairo_set_source_rgba (cr, selection);
                  cairo_rectangle (cr,
                                   PANGO_PIXELS (line_rect.x) + PANGO_PIXELS (line_rect.width),
                                   selection_y,
                                   nonlayout_width,
                                   selection_height);
                  cairo_fill (cr);

                  cairo_restore (cr);
#endif
                }
            }
	  else if (line_display->has_block_cursor &&
		   gtk_widget_has_focus (crenderer->widget) &&
		   byte_offset <= line_display->insert_index &&
		   (line_display->insert_index < byte_offset + line->length ||
		    (at_last_line && line_display->insert_index == byte_offset + line->length)))
	    {
#if 0
	      GdkRectangle cursor_rect;
              GdkRGBA cursor_color;
              cairo_t *cr = crenderer->cr;

              /* we draw text using base color on filled cursor rectangle of cursor color
               * (normally white on black) */
              _gtk_style_context_get_cursor_color (context, &cursor_color, NULL);

	      cursor_rect.x = line_display->x_offset + line_display->block_cursor.x;
	      cursor_rect.y = line_display->block_cursor.y + line_display->top_margin;
	      cursor_rect.width = line_display->block_cursor.width;
	      cursor_rect.height = line_display->block_cursor.height;

              cairo_save (cr);

              gdk_cairo_rectangle (cr, &cursor_rect);
              cairo_clip (cr);

              gdk_cairo_set_source_rgba (cr, &cursor_color);
              cairo_paint (cr);

              /* draw text under the cursor if any */
              if (!line_display->cursor_at_line_end)
                {
                  GdkRGBA *color;

                  gtk_style_context_get (context, "background-color", &color, NULL);

                  gdk_cairo_set_source_rgba (cr, color);

		  gsk_pango_renderer_set_state (crenderer, CURSOR);

		  pango_renderer_draw_layout_line (PANGO_RENDERER (crenderer),
                                                   line,
                                                   line_rect.x,
                                                   baseline);
                  gdk_rgba_free (color);
                }

              cairo_restore (cr);
#endif
	    }
        }

      byte_offset += line->length;
    }
  while (pango_layout_iter_next_line (iter));

  gtk_snapshot_restore (crenderer->snapshot);

  gdk_rgba_free (selection);
  pango_layout_iter_free (iter);
}

void
gtk_text_layout_snapshot2 (GtkTextLayout      *layout,
                           GtkWidget          *widget,
                           GtkSnapshot        *snapshot,
                           const GdkRectangle *clip)
{
  GskPangoRenderer *crenderer;
  GtkStyleContext *context;
  gint offset_y;
  GtkTextIter selection_start, selection_end;
  gboolean have_selection;
  GSList *line_list;
  GSList *tmp_list;
  GdkRGBA color;

  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));
  g_return_if_fail (layout->default_style != NULL);
  g_return_if_fail (layout->buffer != NULL);
  g_return_if_fail (snapshot != NULL);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_get_color (context, &color);

  line_list = gtk_text_layout_get_lines (layout, clip->y, clip->y + clip->height, &offset_y);

  if (line_list == NULL)
    return; /* nothing on the screen */

  crenderer = acquire_renderer ();

  crenderer->widget = widget;
  crenderer->snapshot = snapshot;
  crenderer->fg_color = color;

  gtk_text_layout_wrap_loop_start (layout);

  have_selection = gtk_text_buffer_get_selection_bounds (layout->buffer,
                                                         &selection_start,
                                                         &selection_end);

  tmp_list = line_list;
  while (tmp_list != NULL)
    {
      GtkTextLine *line = tmp_list->data;
      GtkTextLineDisplay *line_display;
      gint selection_start_index = -1;
      gint selection_end_index = -1;

      line_display = gtk_text_layout_get_line_display (layout, line, FALSE);

      if (line_display->height > 0)
        {
          g_assert (line_display->layout != NULL);

          if (have_selection)
            {
              GtkTextIter line_start, line_end;
              gint byte_count;

              gtk_text_layout_get_iter_at_line (layout, &line_start, line, 0);
              line_end = line_start;
              if (!gtk_text_iter_ends_line (&line_end))
                gtk_text_iter_forward_to_line_end (&line_end);
              byte_count = gtk_text_iter_get_visible_line_index (&line_end);

              if (gtk_text_iter_compare (&selection_start, &line_end) <= 0 &&
                  gtk_text_iter_compare (&selection_end, &line_start) >= 0)
                {
                  if (gtk_text_iter_compare (&selection_start, &line_start) >= 0)
                    selection_start_index = gtk_text_iter_get_visible_line_index (&selection_start);
                  else
                    selection_start_index = -1;

                  if (gtk_text_iter_compare (&selection_end, &line_end) <= 0)
                    selection_end_index = gtk_text_iter_get_visible_line_index (&selection_end);
                  else
                    selection_end_index = byte_count + 1; /* + 1 to flag past-the-end */
                }
            }

          render_para (crenderer, offset_y, line_display,
                       selection_start_index, selection_end_index);

          /* We paint the cursors last, because they overlap another chunk
           * and need to appear on top.
           */
#if 0
          if (line_display->cursors != NULL)
            {
              int i;

              for (i = 0; i < line_display->cursors->len; i++)
                {
                  int index;
                  PangoDirection dir;

                  index = g_array_index(line_display->cursors, int, i);
                  dir = (line_display->direction == GTK_TEXT_DIR_RTL) ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR;
                  gtk_render_insertion_cursor (context, cr,
                                               line_display->x_offset, line_display->top_margin,
                                               line_display->layout, index, dir);
                }
            }
#endif
        } /* line_display->height > 0 */

      offset_y += line_display->height;

      gtk_text_layout_free_line_display (layout, line_display);

      tmp_list = tmp_list->next;
    }

  gtk_text_layout_wrap_loop_end (layout);

  g_slist_free (line_list);

  release_renderer (crenderer);
}
