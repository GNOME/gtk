/* GSK - The GTK Scene Kit
 *
 * Copyright 2016  Endless
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

#include "config.h"

#include "gsktextnodeprivate.h"

#include "gskrendernodeprivate.h"
#include "gskrectprivate.h"
#include "gskrenderreplay.h"
#include "gskprivate.h"

#include "gdk/gdkcairoprivate.h"
#include "gdk/gdkcolorprivate.h"

/**
 * GskTextNode:
 *
 * A render node drawing a set of glyphs.
 */
struct _GskTextNode
{
  GskRenderNode render_node;

  PangoFontMap *fontmap;
  PangoFont *font;
  gboolean has_color_glyphs;
  cairo_hint_style_t hint_style;

  GdkColor color;
  graphene_point_t offset;

  guint num_glyphs;
  PangoGlyphInfo *glyphs;
};

static void
gsk_text_node_finalize (GskRenderNode *node)
{
  GskTextNode *self = (GskTextNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_TEXT_NODE));

  g_object_unref (self->font);
  g_object_unref (self->fontmap);
  g_free (self->glyphs);
  gdk_color_finish (&self->color);

  parent_class->finalize (node);
}

static void
gsk_text_node_draw (GskRenderNode *node,
                    cairo_t       *cr,
                    GskCairoData  *data)
{
  GskTextNode *self = (GskTextNode *) node;
  PangoGlyphString glyphs;

  glyphs.num_glyphs = self->num_glyphs;
  glyphs.glyphs = self->glyphs;
  glyphs.log_clusters = NULL;

  cairo_save (cr);

  if (!gdk_color_state_equal (data->ccs, GDK_COLOR_STATE_SRGB) &&
      self->has_color_glyphs)
    {
      g_warning ("whoopsie, color glyphs and we're not in sRGB");
    }
  else
    {
      gdk_cairo_set_source_color (cr, data->ccs, &self->color);
      cairo_translate (cr, self->offset.x, self->offset.y);
      pango_cairo_show_glyph_string (cr, self->font, &glyphs);
    }

  cairo_restore (cr);
}

static void
gsk_text_node_diff (GskRenderNode *node1,
                    GskRenderNode *node2,
                    GskDiffData   *data)
{
  GskTextNode *self1 = (GskTextNode *) node1;
  GskTextNode *self2 = (GskTextNode *) node2;

  if (self1->font == self2->font &&
      gdk_color_equal (&self1->color, &self2->color) &&
      graphene_point_equal (&self1->offset, &self2->offset) &&
      self1->num_glyphs == self2->num_glyphs)
    {
      guint i;

      for (i = 0; i < self1->num_glyphs; i++)
        {
          PangoGlyphInfo *info1 = &self1->glyphs[i];
          PangoGlyphInfo *info2 = &self2->glyphs[i];

          if (info1->glyph == info2->glyph &&
              info1->geometry.width == info2->geometry.width &&
              info1->geometry.x_offset == info2->geometry.x_offset &&
              info1->geometry.y_offset == info2->geometry.y_offset &&
              info1->attr.is_cluster_start == info2->attr.is_cluster_start &&
              info1->attr.is_color == info2->attr.is_color)
            continue;

          gsk_render_node_diff_impossible (node1, node2, data);
          return;
        }

      return;
    }

  gsk_render_node_diff_impossible (node1, node2, data);
}

static GskRenderNode *
gsk_text_node_replay (GskRenderNode   *node,
                      GskRenderReplay *replay)
{
  GskTextNode *self = (GskTextNode *) node;
  GskRenderNode *result;
  PangoFont *font;

  font = gsk_render_replay_filter_font (replay, self->font);
  if (font == self->font)
    {
      g_object_unref (font);
      return gsk_render_node_ref (node);
    }

  result = gsk_text_node_new2 (font, 
                               &(PangoGlyphString) {
                                 self->num_glyphs,
                                 self->glyphs,
                                 NULL,
                               },
                               &self->color,
                               &self->offset);

  g_object_unref (font);

  return result;
}

static void
gsk_text_node_class_init (gpointer g_class,
                          gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_TEXT_NODE;

  node_class->finalize = gsk_text_node_finalize;
  node_class->draw = gsk_text_node_draw;
  node_class->diff = gsk_text_node_diff;
  node_class->replay = gsk_text_node_replay;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskTextNode, gsk_text_node)

static inline float
pango_units_to_float (int i)
{
  return (float) i / PANGO_SCALE;
}

/**
 * gsk_text_node_new:
 * @font: the `PangoFont` containing the glyphs
 * @glyphs: the `PangoGlyphString` to render
 * @color: the foreground color to render with
 * @offset: offset of the baseline
 *
 * Creates a render node that renders the given glyphs.
 *
 * Note that @color may not be used if the font contains
 * color glyphs.
 *
 * Returns: (nullable) (transfer full) (type GskTextNode): a new `GskRenderNode`
 */
GskRenderNode *
gsk_text_node_new (PangoFont              *font,
                   PangoGlyphString       *glyphs,
                   const GdkRGBA          *color,
                   const graphene_point_t *offset)
{
  GdkColor color2;
  GskRenderNode *node;

  gdk_color_init_from_rgba (&color2, color);
  node = gsk_text_node_new2 (font, glyphs, &color2, offset);
  gdk_color_finish (&color2);

  return node;
}

/*< private >
 * gsk_text_node_new2:
 * @font: the `PangoFont` containing the glyphs
 * @glyphs: the `PangoGlyphString` to render
 * @color: the foreground color to render with
 * @offset: offset of the baseline
 *
 * Creates a render node that renders the given glyphs.
 *
 * Note that @color may not be used if the font contains
 * color glyphs.
 *
 * Returns: (nullable) (transfer full) (type GskTextNode): a new `GskRenderNode`
 */
GskRenderNode *
gsk_text_node_new2 (PangoFont              *font,
                    PangoGlyphString       *glyphs,
                    const GdkColor         *color,
                    const graphene_point_t *offset)
{
  GskTextNode *self;
  GskRenderNode *node;
  PangoRectangle ink_rect;
  PangoGlyphInfo *glyph_infos;
  int n;

  gsk_get_glyph_string_extents (glyphs, font, &ink_rect);

  /* Don't create nodes with empty bounds */
  if (ink_rect.width == 0 || ink_rect.height == 0)
    return NULL;

  self = gsk_render_node_alloc (GSK_TYPE_TEXT_NODE);
  node = (GskRenderNode *) self;
  node->preferred_depth = GDK_MEMORY_NONE;
  node->is_hdr = gdk_color_is_srgb (color);

  self->fontmap = g_object_ref (pango_font_get_font_map (font));
  self->font = g_object_ref (font);
  gdk_color_init_copy (&self->color, color);
  self->offset = *offset;
  self->has_color_glyphs = FALSE;
  self->hint_style = gsk_font_get_hint_style (font);

  glyph_infos = g_malloc_n (glyphs->num_glyphs, sizeof (PangoGlyphInfo));

  n = 0;
  for (int i = 0; i < glyphs->num_glyphs; i++)
    {
      /* skip empty glyphs */
      if (glyphs->glyphs[i].glyph == PANGO_GLYPH_EMPTY)
        continue;

      glyph_infos[n] = glyphs->glyphs[i];

      if (glyphs->glyphs[i].attr.is_color)
        self->has_color_glyphs = TRUE;

      n++;
    }

  self->glyphs = glyph_infos;
  self->num_glyphs = n;

  gsk_rect_init (&node->bounds,
                 offset->x + pango_units_to_float (ink_rect.x),
                 offset->y + pango_units_to_float (ink_rect.y),
                 pango_units_to_float (ink_rect.width),
                 pango_units_to_float (ink_rect.height));

  return node;
}


/**
 * gsk_text_node_get_color:
 * @node: (type GskTextNode): a text `GskRenderNode`
 *
 * Retrieves the color used by the text @node.
 *
 * The value returned by this function will not be correct
 * if the render node was created for a non-sRGB color.
 *
 * Returns: (transfer none): the text color
 */
const GdkRGBA *
gsk_text_node_get_color (const GskRenderNode *node)
{
  const GskTextNode *self = (const GskTextNode *) node;

  /* NOTE: This is only correct for nodes with sRGB colors */
  return (const GdkRGBA *) &self->color;
}

/*< private >
 * gsk_text_node_get_gdk_color:
 * @node: (type GskTextNode): a `GskRenderNode`
 *
 * Retrieves the color of the given @node.
 *
 * Returns: (transfer none): the color of the node
 */
const GdkColor *
gsk_text_node_get_gdk_color (const GskRenderNode *node)
{
  GskTextNode *self = (GskTextNode *) node;

  return &self->color;
}

/**
 * gsk_text_node_get_font:
 * @node: (type GskTextNode): The `GskRenderNode`
 *
 * Returns the font used by the text @node.
 *
 * Returns: (transfer none): the font
 */
PangoFont *
gsk_text_node_get_font (const GskRenderNode *node)
{
  const GskTextNode *self = (const GskTextNode *) node;

  return self->font;
}

cairo_hint_style_t
gsk_text_node_get_font_hint_style (const GskRenderNode *node)
{
  const GskTextNode *self = (const GskTextNode *) node;

  return self->hint_style;
}

/**
 * gsk_text_node_has_color_glyphs:
 * @node: (type GskTextNode): a text `GskRenderNode`
 *
 * Checks whether the text @node has color glyphs.
 *
 * Returns: %TRUE if the text node has color glyphs
 *
 * Since: 4.2
 */
gboolean
gsk_text_node_has_color_glyphs (const GskRenderNode *node)
{
  const GskTextNode *self = (const GskTextNode *) node;

  return self->has_color_glyphs;
}

/**
 * gsk_text_node_get_num_glyphs:
 * @node: (type GskTextNode): a text `GskRenderNode`
 *
 * Retrieves the number of glyphs in the text node.
 *
 * Returns: the number of glyphs
 */
guint
gsk_text_node_get_num_glyphs (const GskRenderNode *node)
{
  const GskTextNode *self = (const GskTextNode *) node;

  return self->num_glyphs;
}

/**
 * gsk_text_node_get_glyphs:
 * @node: (type GskTextNode): a text `GskRenderNode`
 * @n_glyphs: (out) (optional): the number of glyphs returned
 *
 * Retrieves the glyph information in the @node.
 *
 * Returns: (transfer none) (array length=n_glyphs): the glyph information
 */
const PangoGlyphInfo *
gsk_text_node_get_glyphs (const GskRenderNode *node,
                          guint               *n_glyphs)
{
  const GskTextNode *self = (const GskTextNode *) node;

  if (n_glyphs != NULL)
    *n_glyphs = self->num_glyphs;

  return self->glyphs;
}

/**
 * gsk_text_node_get_offset:
 * @node: (type GskTextNode): a text `GskRenderNode`
 *
 * Retrieves the offset applied to the text.
 *
 * Returns: (transfer none): a point with the horizontal and vertical offsets
 */
const graphene_point_t *
gsk_text_node_get_offset (const GskRenderNode *node)
{
  const GskTextNode *self = (const GskTextNode *) node;

  return &self->offset;
}
