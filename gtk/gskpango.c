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
#include "gtktextlayoutprivate.h"
#include "gtktextviewprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkcsscolorvalueprivate.h"

#include <math.h>

#include <pango2/pango.h>
#include <cairo.h>

G_DEFINE_TYPE (GskPangoRenderer, gsk_pango_renderer, PANGO2_TYPE_RENDERER)

void
gsk_pango_renderer_set_state (GskPangoRenderer      *crenderer,
                              GskPangoRendererState  state)
{
  g_return_if_fail (GSK_IS_PANGO_RENDERER (crenderer));

  crenderer->state = state;
}

static void
get_color (GskPangoRenderer *crenderer,
           Pango2RenderPart   part,
           GdkRGBA          *rgba)
{
  const Pango2Color *color = pango2_renderer_get_color ((Pango2Renderer *) (crenderer), part);

  if (color)
    {
      rgba->red = color->red / 65535.;
      rgba->green = color->green / 65535.;
      rgba->blue = color->blue / 65535.;
      rgba->alpha = color->alpha / 65535.;
    }
  else
    {
      *rgba = *crenderer->fg_color;
    }
}

static void
set_color (GskPangoRenderer *crenderer,
           Pango2RenderPart   part,
           cairo_t          *cr)
{
  GdkRGBA rgba = { 1, 192./255., 203./255., 1 };

  get_color (crenderer, part, &rgba);
  gdk_cairo_set_source_rgba (cr, &rgba);
}

static void
gsk_pango_renderer_draw_run (Pango2Renderer *renderer,
                             const char    *text,
                             Pango2Run      *run,
                             int            x,
                             int            y)
{
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);
  GdkRGBA color = { 0, 0, 0, 1 };
  Pango2Item *item = pango2_run_get_item (run);
  Pango2GlyphString *glyphs = pango2_run_get_glyphs (run);

  get_color (crenderer, PANGO2_RENDER_PART_FOREGROUND, &color);

  gtk_snapshot_append_text (crenderer->snapshot,
                            pango2_analysis_get_font (pango2_item_get_analysis (item)),
                            crenderer->palette,
                            glyphs,
                            &color,
                            (float) x / PANGO2_SCALE,
                            (float) y / PANGO2_SCALE);
}

static void
gsk_pango_renderer_draw_rectangle (Pango2Renderer     *renderer,
                                   Pango2RenderPart    part,
                                   int                x,
                                   int                y,
                                   int                width,
                                   int                height)
{
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);
  GdkRGBA color = { 0, 0, 0, 1 };

  get_color (crenderer, part, &color);

  gtk_snapshot_append_color (crenderer->snapshot,
                             &color,
                             &GRAPHENE_RECT_INIT ((double)x / PANGO2_SCALE,
                                                  (double)y / PANGO2_SCALE,
                                                  (double)width / PANGO2_SCALE,
                                                  (double)height / PANGO2_SCALE));
}

static void
gsk_pango_renderer_draw_trapezoid (Pango2Renderer   *renderer,
                                   Pango2RenderPart  part,
                                   double           y1_,
                                   double           x11,
                                   double           x21,
                                   double           y2,
                                   double           x12,
                                   double           x22)
{
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);
  Pango2Lines *lines;
  Pango2Rectangle ink_rect;
  cairo_t *cr;
  double x, y;

  lines = pango2_renderer_get_lines (renderer);
  if (!lines)
    return;

  pango2_lines_get_extents (lines, &ink_rect, NULL);
  pango2_extents_to_pixels (&ink_rect, NULL);
  cr = gtk_snapshot_append_cairo (crenderer->snapshot,
                                  &GRAPHENE_RECT_INIT (ink_rect.x, ink_rect.y,
                                                       ink_rect.width, ink_rect.height));
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

#if 0
static void
gsk_pango_renderer_draw_error_underline (Pango2Renderer *renderer,
                                         int            x,
                                         int            y,
                                         int            width,
                                         int            height)
{
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);
  double xx, yy, ww, hh;
  GdkRGBA rgba;
  GskRoundedRect dot;

  xx = (double)x / PANGO2_SCALE;
  yy = (double)y / PANGO2_SCALE;
  ww = (double)width / PANGO2_SCALE;
  hh = (double)height / PANGO2_SCALE;

  get_color (crenderer, PANGO2_RENDER_PART_UNDERLINE, &rgba);

  gtk_snapshot_push_repeat (crenderer->snapshot,
                            &GRAPHENE_RECT_INIT (xx, yy, ww, hh),
                            NULL);

  gsk_rounded_rect_init_from_rect (&dot,
                                   &GRAPHENE_RECT_INIT (xx, yy, hh, hh),
                                   hh / 2);

  gtk_snapshot_push_rounded_clip (crenderer->snapshot, &dot);
  gtk_snapshot_append_color (crenderer->snapshot, &rgba, &dot.bounds);
  gtk_snapshot_pop (crenderer->snapshot);
  gtk_snapshot_append_color (crenderer->snapshot,
                             &(GdkRGBA) { 0.f, 0.f, 0.f, 0.f },
                             &GRAPHENE_RECT_INIT (xx, yy, 1.5 * hh, hh));

  gtk_snapshot_pop (crenderer->snapshot);
}
#endif

static void
gsk_pango_renderer_draw_shape (Pango2Renderer  *renderer,
                               Pango2Rectangle *ink_rect,
                               Pango2Rectangle *logical_rect,
                               gpointer        data,
                               int             x,
                               int             y)
{
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);

  if (crenderer->shape_handler)
    {
      double shape_x = (double)x / PANGO2_SCALE;
      double shape_y = (double) (y + logical_rect->y) / PANGO2_SCALE;

      if (shape_x != 0 || shape_y != 0)
        {
          gtk_snapshot_save (crenderer->snapshot);
          gtk_snapshot_translate (crenderer->snapshot, &GRAPHENE_POINT_INIT (shape_x, shape_y));
        }

      crenderer->shape_handler (data,
                                crenderer->snapshot,
                                (double)logical_rect->width / PANGO2_SCALE,
                                (double)logical_rect->height / PANGO2_SCALE);
      if (shape_x != 0 || shape_y != 0)
        gtk_snapshot_restore (crenderer->snapshot);
    }
}

static void
text_renderer_set_rgba (GskPangoRenderer *crenderer,
                        Pango2RenderPart   part,
                        const GdkRGBA    *rgba)
{
  Pango2Renderer *renderer = PANGO2_RENDERER (crenderer);
  Pango2Color color = { 0, };

  if (rgba)
    {
      color.red = (guint16)(rgba->red * 65535);
      color.green = (guint16)(rgba->green * 65535);
      color.blue = (guint16)(rgba->blue * 65535);
      color.alpha = (guint16)(rgba->alpha * 65535);
      pango2_renderer_set_color (renderer, part, &color);
    }
  else
    {
      pango2_renderer_set_color (renderer, part, NULL);
    }
}

static GtkTextAppearance *
get_item_appearance (Pango2Item *item)
{
  GSList *tmp_list;

  tmp_list = pango2_analysis_get_extra_attributes (pango2_item_get_analysis (item));

  while (tmp_list)
    {
      Pango2Attribute *attr = tmp_list->data;

      if (pango2_attribute_type (attr) == gtk_text_attr_appearance_type)
        return (GtkTextAppearance *)pango2_attribute_get_pointer (attr);

      tmp_list = tmp_list->next;
    }

  return NULL;
}

static GQuark
find_palette (Pango2Context *context,
              Pango2Item    *item)
{
  GSList *l;

  for (l = pango2_analysis_get_extra_attributes (pango2_item_get_analysis (item)); l; l = l->next)
    {
      Pango2Attribute *attr = l->data;

      if (pango2_attribute_type (attr) == PANGO2_ATTR_PALETTE)
        return g_quark_from_string (pango2_attribute_get_string (attr));
    }

  return g_quark_from_string (pango2_context_get_palette (context));
}

static void
gsk_pango_renderer_prepare_run (Pango2Renderer *renderer,
                                Pango2Run      *run)
{
  GskPangoRenderer *crenderer = GSK_PANGO_RENDERER (renderer);
  const GdkRGBA *bg_rgba = NULL;
  const GdkRGBA *fg_rgba = NULL;
  GtkTextAppearance *appearance;

  PANGO2_RENDERER_CLASS (gsk_pango_renderer_parent_class)->prepare_run (renderer, run);

  crenderer->palette = find_palette (pango2_renderer_get_context (renderer), pango2_run_get_item (run));

  appearance = get_item_appearance (pango2_run_get_item (run));

  if (appearance == NULL)
    return;

  if (appearance->draw_bg && crenderer->state == GSK_PANGO_RENDERER_NORMAL)
    bg_rgba = appearance->bg_rgba;
  else
    bg_rgba = NULL;

  text_renderer_set_rgba (crenderer, PANGO2_RENDER_PART_BACKGROUND, bg_rgba);

  if (crenderer->state == GSK_PANGO_RENDERER_SELECTED &&
      GTK_IS_TEXT_VIEW (crenderer->widget))
    {
      GtkCssNode *node;
      GtkCssValue *value;

      node = gtk_text_view_get_selection_node ((GtkTextView *)crenderer->widget);
      value = gtk_css_node_get_style (node)->core->color;
      fg_rgba = gtk_css_color_value_get_rgba (value);
    }
  else if (crenderer->state == GSK_PANGO_RENDERER_CURSOR && gtk_widget_has_focus (crenderer->widget))
    {
      GtkCssNode *node;
      GtkCssValue *value;

      node = gtk_widget_get_css_node (crenderer->widget);
      value = gtk_css_node_get_style (node)->background->background_color;
      fg_rgba = gtk_css_color_value_get_rgba (value);
    }
  else
    fg_rgba = appearance->fg_rgba;

  text_renderer_set_rgba (crenderer, PANGO2_RENDER_PART_FOREGROUND, fg_rgba);

  if (appearance->strikethrough_rgba)
    text_renderer_set_rgba (crenderer, PANGO2_RENDER_PART_STRIKETHROUGH, appearance->strikethrough_rgba);
  else
    text_renderer_set_rgba (crenderer, PANGO2_RENDER_PART_STRIKETHROUGH, fg_rgba);

  if (appearance->underline_rgba)
    text_renderer_set_rgba (crenderer, PANGO2_RENDER_PART_UNDERLINE, appearance->underline_rgba);
  else if (appearance->underline == PANGO2_LINE_STYLE_DOTTED)
    {
      if (!crenderer->error_color)
        {
          static const GdkRGBA red = { 1, 0, 0, 1 };
          crenderer->error_color = gdk_rgba_copy (&red);
        }

      text_renderer_set_rgba (crenderer, PANGO2_RENDER_PART_UNDERLINE, crenderer->error_color);
    }
  else
    text_renderer_set_rgba (crenderer, PANGO2_RENDER_PART_UNDERLINE, fg_rgba);

  if (appearance->overline_rgba)
    text_renderer_set_rgba (crenderer, PANGO2_RENDER_PART_OVERLINE, appearance->overline_rgba);
  else
    text_renderer_set_rgba (crenderer, PANGO2_RENDER_PART_OVERLINE, fg_rgba);

}

static void
gsk_pango_renderer_init (GskPangoRenderer *renderer G_GNUC_UNUSED)
{
}

static void
gsk_pango_renderer_class_init (GskPangoRendererClass *klass)
{
  Pango2RendererClass *renderer_class = PANGO2_RENDERER_CLASS (klass);

  renderer_class->draw_run = gsk_pango_renderer_draw_run;
  renderer_class->draw_rectangle = gsk_pango_renderer_draw_rectangle;
  renderer_class->draw_trapezoid = gsk_pango_renderer_draw_trapezoid;
  //renderer_class->draw_error_underline = gsk_pango_renderer_draw_error_underline;
  renderer_class->draw_shape = gsk_pango_renderer_draw_shape;
  renderer_class->prepare_run = gsk_pango_renderer_prepare_run;
}

static GskPangoRenderer *cached_renderer = NULL; /* MT-safe */
G_LOCK_DEFINE_STATIC (cached_renderer);

GskPangoRenderer *
gsk_pango_renderer_acquire (void)
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

      /* Reset to standard state */
      renderer->state = GSK_PANGO_RENDERER_NORMAL;
    }
  else
    {
      renderer = g_object_new (GSK_TYPE_PANGO_RENDERER, NULL);
    }

  return renderer;
}

void
gsk_pango_renderer_release (GskPangoRenderer *renderer)
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
 * @snapshot: a `GtkSnapshot`
 * @layout: the `Pango2Layout` to render
 * @color: the foreground color to render the layout in
 *
 * Creates render nodes for rendering @layout in the given foregound @color
 * and appends them to the current node of @snapshot without changing the
 * current node.
 **/
void
gtk_snapshot_append_layout (GtkSnapshot   *snapshot,
                            Pango2Layout   *layout,
                            const GdkRGBA *color)
{
  GskPangoRenderer *crenderer;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (PANGO2_IS_LAYOUT (layout));

  crenderer = gsk_pango_renderer_acquire ();

  crenderer->snapshot = snapshot;
  crenderer->fg_color = color;

  pango2_renderer_draw_lines (PANGO2_RENDERER (crenderer), pango2_layout_get_lines (layout), 0, 0);

  gsk_pango_renderer_release (crenderer);
}

void
gtk_snapshot_append_lines (GtkSnapshot   *snapshot,
                            Pango2Lines   *lines,
                            const GdkRGBA *color)
{
  GskPangoRenderer *crenderer;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (PANGO2_IS_LINES (lines));

  crenderer = gsk_pango_renderer_acquire ();

  crenderer->snapshot = snapshot;
  crenderer->fg_color = color;

  pango2_renderer_draw_lines (PANGO2_RENDERER (crenderer), lines, 0, 0);

  gsk_pango_renderer_release (crenderer);
}
