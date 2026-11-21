/*
 * Copyright © 2025 Red Hat, Inc
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtksvgtextprivate.h"

#include "gtksvgprivate.h"

#include "gtkenums.h"
#include "gtksvgutilsprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgnumberprivate.h"
#include "gtksvgnumbersprivate.h"
#include "gtksvgstringlistprivate.h"
#include "gtksvgenumprivate.h"
#include "gtksvglanguageprivate.h"
#include "gtksvgtextdecorationprivate.h"
#include "gtksvgelementtypeprivate.h"
#include "gtksvgelementinternal.h"

#include <hb-ot.h>

/* {{{ Whitespace handling */

static inline gboolean
preserve_newline (WhiteSpace space)
{
  switch (space)
    {
    case WHITE_SPACE_NORMAL:
    case WHITE_SPACE_NOWRAP:
    case WHITE_SPACE_COLLAPSE:
    case WHITE_SPACE_PRESERVE_SPACES:
      return FALSE;
    case WHITE_SPACE_PRE:
    case WHITE_SPACE_PRE_WRAP:
    case WHITE_SPACE_BREAK_SPACES:
    case WHITE_SPACE_PRE_LINE:
    case WHITE_SPACE_PRESERVE:
    case WHITE_SPACE_PRESERVE_BREAKS:
      return TRUE;
    default:
      g_assert_not_reached ();
    }
}

static inline gboolean
preserve_space (WhiteSpace space)
{
  switch (space)
    {
    case WHITE_SPACE_NORMAL:
    case WHITE_SPACE_NOWRAP:
    case WHITE_SPACE_PRE_LINE:
    case WHITE_SPACE_COLLAPSE:
    case WHITE_SPACE_PRESERVE_BREAKS:
      return FALSE;
    case WHITE_SPACE_PRE:
    case WHITE_SPACE_PRE_WRAP:
    case WHITE_SPACE_BREAK_SPACES:
    case WHITE_SPACE_PRESERVE:
    case WHITE_SPACE_PRESERVE_SPACES:
      return TRUE;
    default:
      g_assert_not_reached ();
    }
}

static inline gboolean
preserve_tab (WhiteSpace space)
{
  switch (space)
    {
    case WHITE_SPACE_NORMAL:
    case WHITE_SPACE_PRE_LINE:
    case WHITE_SPACE_COLLAPSE:
    case WHITE_SPACE_PRESERVE_BREAKS:
    case WHITE_SPACE_PRESERVE_SPACES:
    case WHITE_SPACE_NOWRAP:
      return FALSE;
    case WHITE_SPACE_PRE:
    case WHITE_SPACE_PRE_WRAP:
    case WHITE_SPACE_PRESERVE:
    case WHITE_SPACE_BREAK_SPACES:
      return TRUE;
    default:
      g_assert_not_reached ();
    }
}

typedef struct
{
  SvgElement *shape;
  WritingMode wmode;
  WhiteSpace space;
  gboolean initial;
  gboolean trailing;
  double x0, y0;
  double x, y;
  double baseline_shift;
  SvgElement *chunk_shape;
  unsigned int chunk_node;
  unsigned int chunk_layout;
} TextContext;

static char *
text_chomp (const char  *in,
            TextContext *context)
{
  size_t len = strlen (in);
  GString *str = g_string_sized_new (len);

  for (size_t i = 0; i < len; i++)
    {
      context->initial = context->initial && str->len == 0;

      if (in[i] == '\r')
        {
          if (in[i+1] == '\n')
            i++;
        }

      if (in[i] == '\r' || in[i] == '\n')
        {
          if (preserve_newline (context->space))
            {
              g_string_append_c (str, '\n');
              context->trailing = TRUE;
            }
          else if (preserve_space (context->space))
            {
              g_string_append_c (str, ' ');
              context->trailing = TRUE;
            }
          else if (!context->trailing && !context->initial)
            {
              g_string_append_c (str, ' ');
              context->trailing = TRUE;
            }
        }
      else if (in[i] == '\t')
        {
          if (preserve_tab (context->space))
            {
              g_string_append_c (str, '\t');
              context->trailing = TRUE;
            }
          else if (preserve_space (context->space))
            {
              g_string_append_c (str, ' ');
              context->trailing = TRUE;
            }
          else if (!context->trailing && !context->initial)
            {
              g_string_append_c (str, ' ');
              context->trailing = TRUE;
            }
        }
      else if (in[i] == ' ')
        {
          if (preserve_space (context->space))
            {
              g_string_append_c (str, ' ');
              context->trailing = TRUE;
            }
          else if (!context->trailing && !context->initial)
            {
              g_string_append_c (str, ' ');
              context->trailing = TRUE;
            }
        }
      else
        {
          g_string_append_c (str, in[i]);
          context->trailing = FALSE;
        }
    }

  return g_string_free_and_steal (str);
}

static TextLayout *
find_last_layout (SvgElement *shape)
{
  for (unsigned int i = shape->text->len; i > 0; i--)
    {
      TextNode *node = &g_array_index (shape->text, TextNode, i - 1);
      if (node->type == TEXT_NODE_SHAPE)
        {
          TextLayout *layout = find_last_layout (node->shape.shape);
          if (layout)
            return layout;
        }
      else
        {
          if (node->characters.layouts->len > 0)
            return &g_array_index (node->characters.layouts, TextLayout, node->characters.layouts->len - 1);
        }
    }

  return NULL;
}

static void
strip_trailing_space (SvgElement  *shape,
                      TextContext *context)
{
  TextLayout *layout;
  const char *text;
  size_t len;

  /* Remove the leftover final space. Note that we rely on text_chomp
   * only considering single-byte whitespace
   */
  layout = find_last_layout (shape);
  if (layout == NULL)
    return;

  text = pango_layout_get_text (layout->layout);
  len = strlen (text);

  if (len == 0)
    return;

  if ((text[len - 1] == ' ' && !preserve_space (context->space)) ||
      (text[len - 1] == '\n' && !preserve_newline (context->space)) ||
      (text[len - 1] == '\t' && !preserve_tab (context->space)))
    pango_layout_set_text (layout->layout, text, len - 1);
}

/* }}} */
/* {{{ Helpers */

static PangoFontMap *
get_fontmap (GtkSvg *svg)
{
  if (svg->fontmap)
    return svg->fontmap;
  else
    return pango_cairo_font_map_get_default ();
}

static gboolean
svg_writing_mode_is_vertical (WritingMode mode)
{
  const gboolean is_vertical[] = {
    FALSE, TRUE, TRUE,
    FALSE, FALSE, FALSE, FALSE, TRUE, TRUE
  };

  return is_vertical[mode];
}

static PangoGravity
writing_mode_to_gravity (WritingMode wmode)
{
  switch (wmode)
    {
    case WRITING_MODE_HORIZONTAL_TB:
      return PANGO_GRAVITY_SOUTH;
    case WRITING_MODE_VERTICAL_LR:
      // See note about having vertical-lr rotate -90° instead, below
    case WRITING_MODE_VERTICAL_RL:
      return PANGO_GRAVITY_EAST;
    case WRITING_MODE_LEGACY_LR:
    case WRITING_MODE_LEGACY_LR_TB:
      return PANGO_GRAVITY_SOUTH;
    case WRITING_MODE_LEGACY_RL:
    case WRITING_MODE_LEGACY_RL_TB:
      return PANGO_GRAVITY_SOUTH;
    case WRITING_MODE_LEGACY_TB:
    case WRITING_MODE_LEGACY_TB_RL:
      return PANGO_GRAVITY_EAST;
    default:
      g_assert_not_reached ();
    }
}

static PangoDirection
apply_writing_mode_to_direction (WritingMode    wmode,
                                 PangoDirection dir)
{
  switch (wmode)
    {
    case WRITING_MODE_HORIZONTAL_TB:
      return dir;
    case WRITING_MODE_VERTICAL_LR:
    case WRITING_MODE_VERTICAL_RL:
      return dir;
    case WRITING_MODE_LEGACY_LR:
    case WRITING_MODE_LEGACY_LR_TB:
      return PANGO_DIRECTION_LTR;
    case WRITING_MODE_LEGACY_RL:
    case WRITING_MODE_LEGACY_RL_TB:
      return PANGO_DIRECTION_RTL;
    case WRITING_MODE_LEGACY_TB:
    case WRITING_MODE_LEGACY_TB_RL:
      return PANGO_DIRECTION_LTR;
    default:
      g_assert_not_reached ();
    }
}
#define LRE 0x202a
#define RLE 0x202b
#define PDF 0x202c
#define LRO 0x202d
#define RLO 0x202e
#define LRI 0x2066
#define RLI 0x2067
#define FSI 0x2068
#define PDI 0x2069

static char *
apply_bidi_to_text (UnicodeBidi     bidi,
                    PangoDirection  dir,
                    const char     *text,
                    size_t          len)
{
  GString *s;

  if (bidi == UNICODE_BIDI_NORMAL)
    return NULL;

  s = g_string_new ("");

  switch (bidi)
    {
    case UNICODE_BIDI_EMBED:
      g_string_append_unichar (s, dir == PANGO_DIRECTION_LTR ? LRE : RLE);
      g_string_append_len (s, text, len);
      g_string_append_unichar (s, PDF);
      break;
    case UNICODE_BIDI_ISOLATE:
      g_string_append_unichar (s, dir == PANGO_DIRECTION_LTR ? LRI : RLI);
      g_string_append_len (s, text, len);
      g_string_append_unichar (s, PDI);
      break;
    case UNICODE_BIDI_OVERRIDE:
      g_string_append_unichar (s, dir == PANGO_DIRECTION_LTR ? LRO : RLO);
      g_string_append_len (s, text, len);
      g_string_append_unichar (s, PDF);
      break;
    case UNICODE_BIDI_ISOLATE_OVERRIDE:
      g_string_append_unichar (s, FSI);
      g_string_append_unichar (s, dir == PANGO_DIRECTION_LTR ? LRO : RLO);
      g_string_append_len (s, text, len);
      g_string_append_unichar (s, PDF);
      g_string_append_unichar (s, PDI);
      break;
    case UNICODE_BIDI_PLAINTEXT:
      g_string_append_unichar (s, FSI);
      g_string_append_len (s, text, len);
      g_string_append_unichar (s, PDI);
      break;
    case UNICODE_BIDI_NORMAL:
      default:
      g_assert_not_reached ();
    }

  return g_string_free_and_steal (s);
}

#define SVG_DEFAULT_DPI 96.0

static PangoFontDescription *
svg_element_get_font_description (SvgElement *self)
{
  PangoFontDescription *desc;
  SvgValue *font_family;

  desc = pango_font_description_new ();

  font_family = svg_element_get_current_value (self, SVG_PROPERTY_FONT_FAMILY);
  if (svg_string_list_get_length (font_family) > 1)
    {
      GString *s = g_string_new ("");

      for (unsigned int i = 0; i < svg_string_list_get_length (font_family); i++)
        {
          if (i > 0)
            g_string_append_c (s, ',');
          g_string_append (s, svg_string_list_get (font_family, i));
        }
      pango_font_description_set_family (desc, s->str);
      g_string_free (s, TRUE);
    }
  else if (svg_string_list_get_length (font_family) == 1)
    {
      pango_font_description_set_family (desc, svg_string_list_get (font_family, 0));
    }
  else
    {
      pango_font_description_set_family (desc, "serif");
    }

  pango_font_description_set_style (desc, svg_enum_get (svg_element_get_current_value (self, SVG_PROPERTY_FONT_STYLE)));
  pango_font_description_set_variant (desc, svg_enum_get (svg_element_get_current_value (self, SVG_PROPERTY_FONT_VARIANT)));
  pango_font_description_set_weight (desc, (unsigned int) svg_number_get (svg_element_get_current_value (self, SVG_PROPERTY_FONT_WEIGHT), 1000.));
  pango_font_description_set_width (desc, (unsigned int) svg_number_get (svg_element_get_current_value (self, SVG_PROPERTY_FONT_WIDTH), 1000.));

  pango_font_description_set_size (desc,
                                   svg_number_get (svg_element_get_current_value (self, SVG_PROPERTY_FONT_SIZE), 1.) *
                                   PANGO_SCALE / SVG_DEFAULT_DPI * 72);

  return desc;
}

static PangoAttrList *
svg_element_get_text_attrs (SvgElement *self)
{
  PangoAttrList *attrs;
  PangoAttribute *attr;
  TextDecoration decoration;

  attrs = pango_attr_list_new ();

  attr = pango_attr_letter_spacing_new (svg_number_get (svg_element_get_current_value (self, SVG_PROPERTY_LETTER_SPACING), 1.) * PANGO_SCALE);
  attr->start_index = 0;
  attr->end_index = G_MAXINT;
  pango_attr_list_insert (attrs, attr);

  decoration = svg_text_decoration_get (svg_element_get_current_value (self, SVG_PROPERTY_TEXT_DECORATION));
  if (decoration & TEXT_DECORATION_UNDERLINE)
    {
      attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
      attr->start_index = 0;
      attr->end_index = -1;
      pango_attr_list_insert (attrs, attr);
    }
  if (decoration & TEXT_DECORATION_OVERLINE)
    {
      attr = pango_attr_overline_new (PANGO_OVERLINE_SINGLE);
      attr->start_index = 0;
      attr->end_index = -1;
      pango_attr_list_insert (attrs, attr);
    }
  if (decoration & TEXT_DECORATION_LINE_THROUGH)
    {
      attr = pango_attr_strikethrough_new (TRUE);
      attr->start_index = 0;
      attr->end_index = -1;
      pango_attr_list_insert (attrs, attr);
    }

  return attrs;
}

static float
svg_element_get_baseline_shift (SvgElement   *shape,
                                PangoContext *context)
{
  PangoFontDescription *desc;
  PangoFontMetrics *metrics;
  SvgValue *bshift;
  float baseline_shift;

  bshift = svg_element_get_current_value (shape, SVG_PROPERTY_BASELINE_SHIFT);

  desc = svg_element_get_font_description (shape);
  metrics = pango_context_get_metrics (context, desc, pango_context_get_language (context));

  if (svg_value_is_number (bshift))
    {
      baseline_shift = - svg_number_get (bshift, pango_font_metrics_get_height (metrics) / (double) PANGO_SCALE);
    }
  else
    {
      PangoFont *font = pango_context_load_font (context, desc);
      hb_font_t *hb_font = pango_font_get_hb_font (font);
      int ofs = 0;

      switch (svg_enum_get (bshift))
        {
        case BASELINE_SHIFT_SUB:
          hb_ot_metrics_get_position (hb_font, HB_OT_METRICS_TAG_SUBSCRIPT_EM_Y_OFFSET, &ofs);
          baseline_shift = ofs / (double) PANGO_SCALE;
          break;
        case BASELINE_SHIFT_SUPER:
          hb_ot_metrics_get_position (hb_font, HB_OT_METRICS_TAG_SUPERSCRIPT_EM_Y_OFFSET, &ofs);
          baseline_shift = - ofs / (double) PANGO_SCALE;
          break;
        case BASELINE_SHIFT_TOP:
          baseline_shift = pango_font_metrics_get_height (metrics) / (double) PANGO_SCALE;
          break;
        case BASELINE_SHIFT_CENTER:
          baseline_shift = 0.5 * pango_font_metrics_get_height (metrics) / (double) PANGO_SCALE;
          break;
        case BASELINE_SHIFT_BOTTOM:
          baseline_shift = 0;
          break;
        default:
          g_assert_not_reached ();
        }

    g_object_unref (font);
  }

  pango_font_metrics_unref (metrics);
  pango_font_description_free (desc);

  return baseline_shift;
}

static float
svg_element_get_line_height (SvgElement   *shape,
                             PangoContext *context)
{
  PangoFontDescription *desc;
  PangoFontMetrics *metrics;
  float line_height;

  desc = svg_element_get_font_description (shape);
  metrics = pango_context_get_metrics (context, desc, pango_context_get_language (context));

  line_height = pango_font_metrics_get_height (metrics) / (double) PANGO_SCALE;

  pango_font_metrics_unref (metrics);
  pango_font_description_free (desc);

  return line_height;
}

/* }}} */
/* {{{ Text */

static PangoContext *
svg_element_get_pango_context (SvgElement   *self,
                               PangoFontMap *fontmap,
                               WritingMode   wmode)
{
  PangoContext *context;
  UnicodeBidi bidi;
  PangoDirection direction, dir;
  PangoGravity gravity;

  context = pango_font_map_create_context (fontmap);

  pango_context_set_language (context, svg_language_get (svg_element_get_current_value (self, SVG_PROPERTY_LANG), 0));

  bidi = svg_enum_get (svg_element_get_current_value (self, SVG_PROPERTY_UNICODE_BIDI));
  dir = svg_enum_get (svg_element_get_current_value (self, SVG_PROPERTY_DIRECTION));

  gravity = writing_mode_to_gravity (wmode);
  direction = apply_writing_mode_to_direction (wmode, dir);

  pango_context_set_base_gravity (context, gravity);

  if (bidi == UNICODE_BIDI_OVERRIDE || bidi == UNICODE_BIDI_EMBED)
    pango_context_set_base_dir (context, direction);
  else if (dir != PANGO_DIRECTION_LTR)
    pango_context_set_base_dir (context, dir);
  else
    pango_context_set_base_dir (context, direction);

  return context;
}

static PangoLayout *
text_create_layout (SvgElement       *self,
                    PangoContext     *context,
                    const char       *text,
                    int               len,
                    WritingMode       wmode,
                    graphene_point_t *origin,
                    graphene_rect_t  *bounds,
                    gboolean         *is_vertical,
                    double           *rotation)
{
  UnicodeBidi bidi;
  PangoDirection direction, dir;
  PangoFontDescription *desc;
  PangoLayout *layout;
  PangoFontMetrics *metrics;
  int ascent, descent;
  PangoAttrList *attrs;
  int w, h;
  PangoLayoutIter *iter;
  double offset;
  DominantBaseline baseline;
  char *text_with_bidi = NULL;

  bidi = svg_enum_get (svg_element_get_current_value (self, SVG_PROPERTY_UNICODE_BIDI));
  dir = svg_enum_get (svg_element_get_current_value (self, SVG_PROPERTY_DIRECTION));

  text_with_bidi = apply_bidi_to_text (bidi, dir, text, len);

  direction = apply_writing_mode_to_direction (wmode, dir);

  desc = svg_element_get_font_description (self);
  attrs = svg_element_get_text_attrs (self);

  layout = pango_layout_new (context);

  if (text_with_bidi)
    pango_layout_set_text (layout, text_with_bidi, -1);
  else
    pango_layout_set_text (layout, text, len);
  pango_layout_set_font_description (layout, desc);
  pango_layout_set_attributes (layout, attrs);
  pango_layout_set_auto_dir (layout, FALSE);
  pango_layout_set_alignment (layout, (direction == PANGO_DIRECTION_LTR) ? PANGO_ALIGN_LEFT : PANGO_ALIGN_RIGHT);

  metrics = pango_context_get_metrics (context, desc, pango_context_get_language (context));

  g_free (text_with_bidi);
  pango_font_description_free (desc);
  pango_attr_list_unref (attrs);

  pango_layout_get_size (layout, &w, &h);
  iter = pango_layout_get_iter (layout);
  offset = pango_layout_iter_get_baseline (iter) / (double)PANGO_SCALE;
  pango_layout_iter_free (iter);

  ascent = pango_font_metrics_get_ascent (metrics);
  descent = pango_font_metrics_get_descent (metrics);

  baseline = svg_enum_get (self->current[SVG_PROPERTY_DOMINANT_BASELINE]);
  switch (baseline)
    {
    case DOMINANT_BASELINE_AUTO:
    case DOMINANT_BASELINE_ALPHABETIC:
      offset += 0.;
      break;
    case DOMINANT_BASELINE_HANGING:
      offset -= (ascent - descent) / (double)PANGO_SCALE;
      break;
    case DOMINANT_BASELINE_MIDDLE:
      // Approximate meanline using strikethrough position and thickness
      // https://mail.gnome.org/archives/gtk-i18n-list/2012-December/msg00046.html
      offset -= (pango_font_metrics_get_strikethrough_position (metrics) +
                 pango_font_metrics_get_strikethrough_thickness (metrics) / 2
                ) / (double)PANGO_SCALE;
      break;
    case DOMINANT_BASELINE_CENTRAL:
      offset = .5 * (ascent + descent) / (double)PANGO_SCALE;
      break;
    case DOMINANT_BASELINE_TEXT_BEFORE_EDGE:
    case DOMINANT_BASELINE_TEXT_BOTTOM:
      //offset -= ascent / (double)PANGO_SCALE;
      // Bit of a klutch, but leads to better results
      offset -= (2.*ascent - h) / (double)PANGO_SCALE;
      break;
    case DOMINANT_BASELINE_TEXT_AFTER_EDGE:
    case DOMINANT_BASELINE_TEXT_TOP:
      offset += descent / (double)PANGO_SCALE;
      break;
    case DOMINANT_BASELINE_IDEOGRAPHIC:
      // Approx
      offset += descent / (double)PANGO_SCALE;
      break;
    case DOMINANT_BASELINE_MATHEMATICAL:
      // Approx
      offset += .5 * (ascent + descent) / (double)PANGO_SCALE;
      break;
    default:
      g_assert_not_reached ();
    }

  switch (wmode)
    {
    case WRITING_MODE_HORIZONTAL_TB:
    case WRITING_MODE_LEGACY_LR:
    case WRITING_MODE_LEGACY_LR_TB:
    case WRITING_MODE_LEGACY_RL:
    case WRITING_MODE_LEGACY_RL_TB:
      *origin = GRAPHENE_POINT_INIT (0., -offset);
      *bounds = GRAPHENE_RECT_INIT (0., -offset, w / (double)PANGO_SCALE, h / (double)PANGO_SCALE);
      *is_vertical = FALSE;
      *rotation = 0.;
      break;
    case WRITING_MODE_VERTICAL_LR:
      /* Firefox uses 90° here (like for the other vertical modes), while rsvg uses -90°
       * I came to the conclusion that implementing -90° properly (rsvg doesn't) doesn't
       * quite work with the current text handling architecture, so we'll just do what the
       * web browsers do.
       */
    case WRITING_MODE_VERTICAL_RL:
    case WRITING_MODE_LEGACY_TB:
    case WRITING_MODE_LEGACY_TB_RL:
      *origin = GRAPHENE_POINT_INIT (offset, 0.);
      *bounds = GRAPHENE_RECT_INIT (offset - h / (double)PANGO_SCALE, 0., h / (double)PANGO_SCALE, w / (double)PANGO_SCALE);
      *is_vertical = TRUE;
      *rotation = 90.;
      break;
    default:
      g_assert_not_reached ();
    }

  pango_font_metrics_unref (metrics);

  return layout;
}

typedef struct TextOffset TextOffset;
struct TextOffset
{
  SvgElement *elt;
  unsigned int idx;
  TextOffset *next;
};

static void
advance_text_offset (TextOffset   *offsets,
                     unsigned int  inc)
{
  while (offsets)
    {
      offsets->idx += inc;
      offsets = offsets->next;
    }
}

typedef struct
{
  gboolean has_x, has_y, has_dx, has_dy;
  double x, y, dx, dy;
} TextPosition;

static void
get_text_position (TextOffset            *offsets,
                   TextPosition          *pos,
                   const graphene_rect_t *viewport)
{
  memset (pos, 0, sizeof (TextPosition));

  while (offsets)
    {
      if (!pos->has_x)
        {
          SvgValue *v = svg_element_get_current_value (offsets->elt, SVG_PROPERTY_TEXT_X);
          if (offsets->idx < svg_numbers_get_length (v))
            {
              pos->x = svg_numbers_get (v, offsets->idx, viewport->size.width);
              pos->has_x = TRUE;
            }
        }
      if (!pos->has_y)
        {
          SvgValue *v = svg_element_get_current_value (offsets->elt, SVG_PROPERTY_TEXT_Y);
          if (offsets->idx < svg_numbers_get_length (v))
            {
              pos->y = svg_numbers_get (v, offsets->idx, viewport->size.height);
              pos->has_y = TRUE;
            }
        }
      if (!pos->has_dx)
        {
          SvgValue *v = svg_element_get_current_value (offsets->elt, SVG_PROPERTY_TEXT_DX);
          if (offsets->idx < svg_numbers_get_length (v))
            {
              pos->dx = svg_numbers_get (v, offsets->idx, viewport->size.width);
              pos->has_dx = TRUE;
            }
        }
      if (!pos->has_dy)
        {
          SvgValue *v = svg_element_get_current_value (offsets->elt, SVG_PROPERTY_TEXT_DY);
          if (offsets->idx < svg_numbers_get_length (v))
            {
              pos->dy = svg_numbers_get (v, offsets->idx, viewport->size.height);
              pos->has_dy = TRUE;
            }
        }

      if (pos->has_x && pos->has_y && pos->has_dx && pos->has_dy)
        break;

      offsets = offsets->next;
    }
}

static gboolean
starts_layout (TextOffset *offsets)
{
  while (offsets)
    {
      if (offsets->idx < svg_numbers_get_length (svg_element_get_current_value (offsets->elt, SVG_PROPERTY_TEXT_X)))
        return TRUE;
      if (offsets->idx < svg_numbers_get_length (svg_element_get_current_value (offsets->elt, SVG_PROPERTY_TEXT_Y)))
        return TRUE;
      if (offsets->idx < svg_numbers_get_length (svg_element_get_current_value (offsets->elt, SVG_PROPERTY_TEXT_DX)))
        return TRUE;
      if (offsets->idx < svg_numbers_get_length (svg_element_get_current_value (offsets->elt, SVG_PROPERTY_TEXT_DY)))
        return TRUE;

      offsets = offsets->next;
    }

  return FALSE;
}

static void
apply_pos_to_chunk (TextContext *ctx,
                    SvgElement  *shape,
                    TextLayout  *chunk,
                    gboolean    *in_chunk,
                    double       dx,
                    double       dy)
{
  for (unsigned int i = 0; i < shape->text->len; i++)
    {
      TextNode *node = &g_array_index (shape->text, TextNode, i);

      if (node->type == TEXT_NODE_SHAPE)
        {
          apply_pos_to_chunk (ctx, node->shape.shape, chunk, in_chunk, dx, dy);
        }
      else
        {
          for (unsigned int j = 0; j < node->characters.layouts->len; j++)
            {
              TextLayout *layout = &g_array_index (node->characters.layouts, TextLayout, j);

              if (layout == chunk)
                *in_chunk = TRUE;

              if (*in_chunk)
                {
                  layout->x += dx;
                  layout->y += dy;
                  layout->x0 += dx;
                  layout->y0 += dy;
                }
            }
        }
    }
}

static void
end_chunk (TextContext *ctx)
{
  double dx = 0;
  double dy = 0;
  gboolean in_chunk;
  TextNode *node;
  TextLayout *chunk;

  /* when we get here, text_ctx->chunk is the layout starting the chunk to end,
   * and text_ctx->x/y are the end position
   */

  if (ctx->chunk_shape == NULL)
    return;

  g_assert (ctx->chunk_node < ctx->chunk_shape->text->len);
  node = &g_array_index (ctx->chunk_shape->text, TextNode, ctx->chunk_node);
  g_assert (node->type == TEXT_NODE_CHARACTERS);
  g_assert (ctx->chunk_layout < node->characters.layouts->len);
  chunk = &g_array_index (node->characters.layouts, TextLayout, ctx->chunk_layout);

  switch (chunk->anchor)
    {
    case TEXT_ANCHOR_START:
      return;
    case TEXT_ANCHOR_MIDDLE:
      if (svg_writing_mode_is_vertical (ctx->wmode))
        dy = - (ctx->y - chunk->y0) / 2;
      else
        dx = - (ctx->x - chunk->x0) / 2;
      break;
    case TEXT_ANCHOR_END:
      if (svg_writing_mode_is_vertical (ctx->wmode))
        dy = - (ctx->y - chunk->y0);
      else
        dx = - (ctx->x - chunk->x0);
      break;
    default:
      g_assert_not_reached ();
    }

  in_chunk = FALSE;
  apply_pos_to_chunk (ctx, ctx->shape, chunk, &in_chunk, dx, dy);
  ctx->chunk_shape = NULL;
}

static void
collect_bounds (SvgElement *shape)
{
  shape->valid_bounds = FALSE;

  for (unsigned int i = 0; i < shape->text->len; i++)
    {
      TextNode *node = &g_array_index (shape->text, TextNode, i);

      if (node->type == TEXT_NODE_SHAPE)
        {
          collect_bounds (node->shape.shape);

          if (node->shape.shape->valid_bounds)
            {
              if (!shape->valid_bounds)
                shape->bounds = node->shape.shape->bounds;
              else
                graphene_rect_union (&shape->bounds, &node->shape.shape->bounds, &shape->bounds);
              shape->valid_bounds = TRUE;
            }
        }
      else
        {
          for (unsigned int j = 0; j < node->characters.layouts->len; j++)
            {
              TextLayout *layout = &g_array_index (node->characters.layouts, TextLayout, j);

               graphene_rect_offset (&layout->bounds, layout->x0, layout->y0);

              if (!shape->valid_bounds)
                shape->bounds = layout->bounds;
              else
                graphene_rect_union (&shape->bounds, &layout->bounds, &shape->bounds);
              shape->valid_bounds = TRUE;
            }
        }
    }
}

static void
do_generate_layouts (SvgElement            *self,
                     PangoContext          *context,
                     TextContext           *text_ctx,
                     TextOffset            *offsets,
                     const graphene_rect_t *viewport)
{
  WhiteSpace space = text_ctx->space;
  double baseline_shift = text_ctx->baseline_shift;
  TextAnchor anchor;
  float line_height;
  TextOffset offset;
  gboolean is_vertical;

  g_assert (svg_element_holds_text_content (self));

  if (svg_enum_get (svg_element_get_current_value (self, SVG_PROPERTY_DISPLAY)) == DISPLAY_NONE)
    return;

  offset.elt = self;
  offset.idx = 0;
  offset.next = offsets;

  is_vertical = svg_writing_mode_is_vertical (text_ctx->wmode);
  line_height = svg_element_get_line_height (self, context);
  anchor = svg_enum_get (svg_element_get_current_value (self, SVG_PROPERTY_TEXT_ANCHOR));

  text_ctx->baseline_shift += svg_element_get_baseline_shift (self, context);
  text_ctx->space = svg_enum_get (svg_element_get_current_value (self, SVG_PROPERTY_SPACE));

  for (unsigned int i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);
      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          do_generate_layouts (node->shape.shape, context, text_ctx, &offset, viewport);
          break;
        case TEXT_NODE_CHARACTERS:
          {
            graphene_point_t origin;
            char *text;
            GStrv lines;
            const char *ch;
            TextLayout layout0 = { NULL, };
            int attrs_len;
            PangoLogAttr *attrs;
            unsigned int text_idx;
            PangoLanguage *lang;

            g_assert (node->characters.layouts == NULL);
            node->characters.layouts = array_new_with_clear_func (sizeof (TextLayout), (GDestroyNotify) text_layout_clear);

            lang = svg_language_get (svg_element_get_current_value (self, SVG_PROPERTY_LANG), 0);
            text = text_chomp (node->characters.text, text_ctx);
            lines = g_strsplit (text, "\n", 0);
            g_free (text);

            for (unsigned int l = 0; lines[l]; l++)
              {
                if (l > 0)
                  {
                    end_chunk (text_ctx);
                    if (is_vertical)
                      {
                        text_ctx->x += line_height;
                        text_ctx->y = text_ctx->y0;
                      }
                    else
                      {
                        text_ctx->x = text_ctx->x0;
                        text_ctx->y += line_height;
                      }
                    advance_text_offset (&offset, 1); /* newline is an addressable char */
                  }

                text = lines[l];

                attrs_len = g_utf8_strlen (text, -1) + 1;
                attrs = g_newa (PangoLogAttr, attrs_len);
                pango_get_log_attrs (text, strlen (text), -1, lang, attrs, attrs_len);

                ch = text;
                text_idx = 0;
                do
                  {
                    const char *the_text;
                    TextPosition pos = { 0, };
                    size_t len;

                    the_text = ch;
                    get_text_position (&offset, &pos, viewport);

                    if (pos.has_x || pos.has_y)
                      end_chunk (text_ctx);

                    if (pos.has_x)
                      {
                        text_ctx->x0 = pos.x;
                        text_ctx->x = pos.x;
                      }
                    if (pos.has_dx)
                      {
                        text_ctx->x0 += pos.dx;
                        text_ctx->x += pos.dx;
                      }
                    if (pos.has_y)
                      {
                        text_ctx->y0 = pos.y;
                        text_ctx->y = pos.y;
                      }
                    if (pos.has_dy)
                      {
                        text_ctx->y0 += pos.dy;
                        text_ctx->y += pos.dy;
                      }

                    if (*ch == '\0')
                      break;

                    advance_text_offset (&offset, 1);
                    if (starts_layout (&offset))
                      {
                        do
                          {
                            ch = g_utf8_next_char (ch);
                            text_idx++;
                          }
                        while (!attrs[text_idx].is_cursor_position);

                        len = ch - the_text;
                      }
                    else
                      {
                        advance_text_offset (&offset, g_utf8_strlen (ch, -1) - 1);
                        len = strlen (the_text);
                        ch = &the_text[len];
                      }

                    layout0.layout = text_create_layout (self, context, the_text, len, text_ctx->wmode, &origin, &layout0.bounds, &is_vertical, &layout0.r);
                    layout0.x0 = text_ctx->x;
                    layout0.y0 = text_ctx->y;
                    layout0.x = text_ctx->x + origin.x;
                    layout0.y = text_ctx->y + origin.y + text_ctx->baseline_shift;
                    layout0.anchor = anchor;
                    g_array_append_val (node->characters.layouts, layout0);

                    if (is_vertical)
                      text_ctx->y += layout0.bounds.size.height;
                    else
                      text_ctx->x += layout0.bounds.size.width;

                    if (text_ctx->chunk_shape == NULL || pos.has_x || pos.has_y)
                      {
                        text_ctx->chunk_shape = self;
                        text_ctx->chunk_node = i;
                        text_ctx->chunk_layout = node->characters.layouts->len - 1;
                      }
                  }
                while (*ch);
              }

            g_strfreev (lines);
          }
          break;
        default:
          g_assert_not_reached ();
        }
    }

  text_ctx->baseline_shift = baseline_shift;
  text_ctx->space = space;
}

#if 0
static void
print_layouts (SvgElement *self,
               gboolean    before)
{
  g_print ("<%s>\n", svg_element_type_get_name (self->type));
  for (unsigned int i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);
      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          print_layouts (node->shape.shape, before);
          break;
        case TEXT_NODE_CHARACTERS:
          if (before)
            g_print ("|%s|\n", node->characters.text);
          else
            {
              for (unsigned int j = 0; j < node->characters.layouts->len; j++)
                {
                  TextLayout *layout = &g_array_index (node->characters.layouts, TextLayout, j);
                  g_print ("|%s|\n", pango_layout_get_text (layout->layout));
                }
            }
          break;
        default:
          g_assert_not_reached ();
        }
    }
  g_print ("</%s>\n", svg_element_type_get_name (self->type));
}
#endif

gboolean
generate_layouts (SvgElement            *shape,
                  GtkSvg                *svg,
                  const graphene_rect_t *viewport,
                  graphene_rect_t       *bounds,
                  graphene_point_t      *offset)
{
  PangoContext *context;
  WritingMode wmode;
  TextContext text_ctx = { 0, };

#if 0
  g_print ("---\nBEFORE\n");
  print_layouts (shape, TRUE);
#endif

  for (unsigned int i = 0; i < shape->text->len; i++)
    {
      TextNode *node = &g_array_index (shape->text, TextNode, i);
      if (node->type == TEXT_NODE_CHARACTERS)
        g_assert (node->characters.layouts == NULL);
    }

  wmode = svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_WRITING_MODE));

  text_ctx.shape = shape;
  text_ctx.wmode = wmode;
  text_ctx.space = WHITE_SPACE_NORMAL;
  text_ctx.initial = TRUE;
  text_ctx.trailing = FALSE;
  text_ctx.x = text_ctx.y = 0;
  text_ctx.x0 = text_ctx.y0 = 0;
  text_ctx.baseline_shift = 0;
  text_ctx.chunk_shape = NULL;

  context = svg_element_get_pango_context (shape, get_fontmap (svg), wmode);

  do_generate_layouts (shape, context, &text_ctx, NULL, viewport);

  strip_trailing_space (shape, &text_ctx);
  end_chunk (&text_ctx);

  collect_bounds (shape);

#if 0
  g_print ("---\nAFTER\n");
  print_layouts (shape, FALSE);
#endif

  *bounds = shape->bounds;
  offset->x = 0;
  offset->y = 0;

  g_object_unref (context);

  return TRUE;
}

void
clear_layouts (SvgElement *shape)
{
  for (unsigned int i = 0; i < shape->text->len; i++)
    {
      TextNode *node = &g_array_index (shape->text, TextNode, i);
      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          clear_layouts (node->shape.shape);
          break;
        case TEXT_NODE_CHARACTERS:
          g_clear_pointer (&node->characters.layouts, g_array_unref);
          break;
        default:
          g_assert_not_reached ();
        }
    }
}

void
ensure_valid_text_bounds (SvgElement            *shape,
                          GtkSvg                *svg,
                          const graphene_rect_t *viewport)
{
  if (!shape->valid_bounds)
    {
      graphene_rect_t bounds;
      graphene_point_t offset;

      if (generate_layouts (shape, svg, viewport, &bounds, &offset))
        {
          /* TODO: keep the layouts */
          clear_layouts (shape);
        }
    }
}

/* }}} */

/* vim:set foldmethod=marker: */
