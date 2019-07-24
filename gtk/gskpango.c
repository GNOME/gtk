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
#include "gtktextviewprivate.h"

#include <math.h>

#include <pango/pango.h>
#include <cairo.h>

G_DEFINE_TYPE (GskPangoRenderer, gsk_pango_renderer, PANGO_TYPE_RENDERER)

void
gsk_pango_renderer_set_state (GskPangoRenderer      *crenderer,
                              GskPangoRendererState  state)
{
  g_return_if_fail (GSK_IS_PANGO_RENDERER (crenderer));

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

  get_color (crenderer, part, &rgba);
  gtk_snapshot_append_color (crenderer->snapshot,
                             &rgba,
                             &GRAPHENE_RECT_INIT ((double)x / PANGO_SCALE,
                                                  (double)y / PANGO_SCALE,
                                                  (double)width / PANGO_SCALE,
                                                  (double)height / PANGO_SCALE));
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

#define HEIGHT_RATIO (M_SQRT2/5.0)

static void
gsk_pango_renderer_draw_error_underline (PangoRenderer *renderer,
                                         int            x,
                                         int            y,
                                         int            width,
                                         int            height)
{
  GdkRGBA rgba;
  double xx, yy, ww, hh;
  double hs;
  double e, o;

  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);

  xx = (double)x / PANGO_SCALE;
  yy = (double)y / PANGO_SCALE;
  ww = (double)width / PANGO_SCALE;
  hh = (double)height / PANGO_SCALE;
  hs = hh / M_SQRT2;

  e = fmod (ww - 2 * hs * HEIGHT_RATIO, hs * (1 - HEIGHT_RATIO));

#if 0
  gdk_rgba_parse (&rgba, "yellow");
  gtk_snapshot_append_color (crenderer->snapshot, &rgba,
                             &GRAPHENE_RECT_INIT (xx, yy, ww, hh));
#endif


  get_color (crenderer, PANGO_RENDER_PART_UNDERLINE, &rgba);
  gtk_snapshot_save (crenderer->snapshot);
  gtk_snapshot_translate (crenderer->snapshot,
                          &GRAPHENE_POINT_INIT (xx, yy));

  gtk_snapshot_rotate (crenderer->snapshot, 45);
  gtk_snapshot_translate (crenderer->snapshot,
                          &GRAPHENE_POINT_INIT (e / 2 + hs * HEIGHT_RATIO,
                                                - hs * HEIGHT_RATIO));

  xx = yy = o = 0;
  while (1)
    {
      if (o + hs * (1 + HEIGHT_RATIO) >= ww)
        break;

      gtk_snapshot_append_color (crenderer->snapshot, &rgba,
                                 &GRAPHENE_RECT_INIT (xx, yy, hh, hh * HEIGHT_RATIO));

      xx += hh * (1 - HEIGHT_RATIO);
      yy -= hh * (1 - HEIGHT_RATIO);
      o += hs * (1 - HEIGHT_RATIO);

      if (o + hs * (1 + HEIGHT_RATIO) >= ww)
        break;

      gtk_snapshot_append_color (crenderer->snapshot, &rgba,
                                 &GRAPHENE_RECT_INIT (xx, yy, hh * HEIGHT_RATIO, hh));

      o += hs * (1 - HEIGHT_RATIO);
    }

  gtk_snapshot_restore (crenderer->snapshot);
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

  if (appearance->draw_bg && crenderer->state == GSK_PANGO_RENDERER_NORMAL)
    bg_rgba = appearance->bg_rgba;
  else
    bg_rgba = NULL;

  text_renderer_set_rgba (crenderer, PANGO_RENDER_PART_BACKGROUND, bg_rgba);

  if (crenderer->state == GSK_PANGO_RENDERER_SELECTED &&
      GTK_IS_TEXT_VIEW (crenderer->widget))
    {
      GtkCssNode *selection_node;

      selection_node = gtk_text_view_get_selection_node ((GtkTextView *)crenderer->widget);
      gtk_style_context_save_to_node (context, selection_node);

      gtk_style_context_get (context,
                             "color", &fg_rgba,
                             NULL);

      gtk_style_context_restore (context);
    }
  else if (crenderer->state == GSK_PANGO_RENDERER_CURSOR && gtk_widget_has_focus (crenderer->widget))
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

  crenderer = gsk_pango_renderer_acquire ();

  crenderer->snapshot = snapshot;
  crenderer->fg_color = *color;

  pango_layout_get_pixel_extents (layout, &ink_rect, NULL);
  graphene_rect_init (&crenderer->bounds, ink_rect.x, ink_rect.y, ink_rect.width, ink_rect.height);

  pango_renderer_draw_layout (PANGO_RENDERER (crenderer), layout, 0, 0);

  gsk_pango_renderer_release (crenderer);
}
