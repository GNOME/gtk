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

static char *
text_chomp (const char *in,
            WhiteSpace  space,
            gboolean   *initial,
            gboolean   *lastwasspace)
{
  size_t len = strlen (in);
  GString *str = g_string_sized_new (len);

  for (size_t i = 0; i < len; i++)
    {
      *initial = *initial && str->len == 0;

      if (in[i] == '\r')
        {
          if (in[i+1] == '\n')
            i++;
        }

      if (in[i] == '\r' || in[i] == '\n')
        {
          if (preserve_newline (space))
            {
              g_string_append_c (str, '\n');
              *lastwasspace = TRUE;
            }
          else if (!*lastwasspace && !*initial)
            {
              g_string_append_c (str, ' ');
              *lastwasspace = TRUE;
            }
          continue;
        }

      if (in[i] == '\t' || in[i] == ' ')
        {
          if (preserve_space (space))
            {
              g_string_append_c (str, ' ');
              *lastwasspace = TRUE;
            }
          else if (!*lastwasspace && !*initial)
            {
              g_string_append_c (str, ' ');
              *lastwasspace = TRUE;
            }
          continue;
        }

      g_string_append_c (str, in[i]);
      *lastwasspace = FALSE;
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
strip_trailing_space (SvgElement *shape)
{
  TextLayout *layout;
  const char *text;

  /* Remove the leftover final space. Note that we rely on text_chomp
   * only considering single-byte whitespace
   */
  layout = find_last_layout (shape);
  text = pango_layout_get_text (layout->layout);
  pango_layout_set_text (layout->layout, text, strlen (text) - 1);
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
writing_mode_to_direction (WritingMode    wmode,
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

/* }}} */
/* {{{ Text */

static PangoLayout *
text_create_layout (SvgElement       *self,
                    PangoFontMap     *fontmap,
                    const char       *text,
                    int               len,
                    WritingMode       wmode,
                    graphene_point_t *origin,
                    graphene_rect_t  *bounds,
                    gboolean         *is_vertical,
                    double           *rotation)
{
  PangoContext *context;
  UnicodeBidi bidi;
  PangoDirection direction, dir;
  PangoGravity gravity;
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

  context = pango_font_map_create_context (fontmap);
  pango_context_set_language (context, svg_language_get (svg_element_get_current_value (self, SVG_PROPERTY_LANG), 0));

  bidi = svg_enum_get (svg_element_get_current_value (self, SVG_PROPERTY_UNICODE_BIDI));
  dir = svg_enum_get (svg_element_get_current_value (self, SVG_PROPERTY_DIRECTION));

  text_with_bidi = apply_bidi_to_text (bidi, dir, text, len);

  gravity = writing_mode_to_gravity (wmode);
  direction = writing_mode_to_direction (wmode, dir);

  pango_context_set_base_gravity (context, gravity);

  if (bidi == UNICODE_BIDI_OVERRIDE || bidi == UNICODE_BIDI_EMBED)
    pango_context_set_base_dir (context, direction);
  else if (dir != PANGO_DIRECTION_LTR)
    pango_context_set_base_dir (context, dir);
  else
    pango_context_set_base_dir (context, direction);

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
  g_object_unref (context);

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
has_text_position (TextOffset *offsets)
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

static gboolean
do_generate_layouts (SvgElement             *self,
                     PangoFontMap           *fontmap,
                     WhiteSpace              space,
                     WritingMode             wmode,
                     double                 *x,
                     double                 *y,
                     gboolean               *initial,
                     gboolean               *lastwasspace,
                     TextOffset             *offsets,
                     const graphene_rect_t  *viewport,
                     graphene_rect_t        *bounds)
{
  double dx = 0;
  double dy = 0;
  gboolean set_bounds = FALSE;
  SvgValue *v;
  double baseline_shift = 0;
  TextOffset offset;

  g_assert (svg_element_type_is_text (self->type));

  if (svg_enum_get (svg_element_get_current_value (self, SVG_PROPERTY_DISPLAY)) == DISPLAY_NONE)
    return FALSE;

  offset.elt = self;
  offset.idx = 0;
  offset.next = offsets;

  v = svg_element_get_current_value (self, SVG_PROPERTY_TEXT_X);
  if (svg_numbers_get_length (v) > 0)
    *x = svg_numbers_get (v, 0, viewport->size.width);

  v = svg_element_get_current_value (self, SVG_PROPERTY_TEXT_Y);
  if (svg_numbers_get_length (v) > 0)
    *y = svg_numbers_get (v, 0, viewport->size.height);

  v = svg_element_get_current_value (self, SVG_PROPERTY_TEXT_DX);
  if (svg_numbers_get_length (v) > 0)
    dx = svg_numbers_get (v, 0, viewport->size.width);

  v = svg_element_get_current_value (self, SVG_PROPERTY_TEXT_DY);
  if (svg_numbers_get_length (v) > 0)
    dy = svg_numbers_get (v, 0, viewport->size.height);

  *x += dx;
  *y += dy;

#define ADD_BBOX(box) do {                          \
  if (!set_bounds)                                  \
    {                                               \
      graphene_rect_init_from_rect (bounds, (box)); \
      set_bounds = TRUE;                            \
    }                                               \
  else                                              \
    {                                               \
      graphene_rect_t res;                          \
      graphene_rect_union (bounds, (box), &res);    \
      graphene_rect_init_from_rect (bounds, &res);  \
    }                                               \
} while(0);

  for (unsigned int i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);
      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          {
            WhiteSpace space2;
            double y2 = *y + baseline_shift;

            space2 = svg_enum_get (svg_element_get_current_value (node->shape.shape, SVG_PROPERTY_SPACE));
            node->shape.has_bounds = do_generate_layouts (node->shape.shape, fontmap, space2, wmode, x, &y2, initial, lastwasspace, &offset, viewport, &node->shape.bounds);
            *y = y2 - baseline_shift;
            if (node->shape.has_bounds)
              {
                graphene_rect_init_from_rect (&node->shape.shape->bounds, &node->shape.bounds);
                node->shape.shape->valid_bounds = TRUE;

                ADD_BBOX (&node->shape.bounds)
              }
          }
          break;
        case TEXT_NODE_CHARACTERS:
          {
            graphene_point_t origin;
            graphene_rect_t cbounds;
            gboolean is_vertical;
            char *text;
            const char *ch;
            TextLayout layout0 = { NULL, };
            TextLayout *layout = NULL;
            int attrs_len;
            PangoLogAttr *attrs;
            unsigned int text_idx;
            PangoLanguage *lang;

            text = text_chomp (node->characters.text, space, initial, lastwasspace);

            g_assert (node->characters.layouts == NULL);
            node->characters.layouts = array_new_with_clear_func (sizeof (TextLayout), (GDestroyNotify) text_layout_clear);

            lang = svg_language_get (svg_element_get_current_value (self, SVG_PROPERTY_LANG), 0);
            attrs_len = g_utf8_strlen (text, -1) + 1;
            attrs = g_newa (PangoLogAttr, attrs_len);
            pango_get_log_attrs (text, strlen (text), -1, lang, attrs, attrs_len);

            ch = text;
            text_idx = 0;
            while (*ch)
              {
                TextPosition pos = { 0, };
                const char *the_text;
                size_t len;

                the_text = ch;
                get_text_position (&offset, &pos, viewport);
                advance_text_offset (&offset, 1);
                if (has_text_position (&offset))
                  {
                    do {
                      ch = g_utf8_next_char (ch);
                      text_idx++;
                    } while (!attrs[text_idx].is_cursor_position);

                    len = ch - the_text;
                  }
                else
                  {
                    advance_text_offset (&offset, g_utf8_strlen (ch, -1) - 1);
                    len = strlen (the_text);
                    ch = &the_text[len];
                  }

                g_array_append_val (node->characters.layouts, layout0);
                layout = &g_array_index (node->characters.layouts, TextLayout, node->characters.layouts->len - 1);
                layout->layout = text_create_layout (self, fontmap, the_text, len, wmode, &origin, &cbounds, &is_vertical, &layout->r);

                baseline_shift = svg_element_get_baseline_shift (self, pango_layout_get_context (layout->layout));

                if (pos.has_x)
                  *x = pos.x;
                if (pos.has_dx)
                  *x += pos.dx;
                if (pos.has_y)
                  *y = pos.y;
                if (pos.has_dy)
                  *y += pos.dy;

                layout->x = *x + origin.x;
                layout->y = *y + origin.y + baseline_shift;

                graphene_rect_offset (&cbounds, *x, *y);

                ADD_BBOX (&cbounds)

                if (is_vertical)
                  *y += cbounds.size.height;
                else
                  *x += cbounds.size.width;
              }

            g_free (text);
          }
          break;
        default:
          g_assert_not_reached ();
        }
    }

#undef ADD_BBOX
  return set_bounds;
}

#if 0
static void
print_layouts (SvgElement *self,
               gboolean    before)
{
  for (unsigned int i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);
      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          g_print ("<tspan>\n");
          print_layouts (node->shape.shape, before);
          g_print ("</tspan>\n");
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
}
#endif

gboolean
generate_layouts (SvgElement            *shape,
                  GtkSvg                *svg,
                  const graphene_rect_t *viewport,
                  graphene_rect_t       *bounds,
                  graphene_point_t      *offset)
{
  PangoFontMap *fontmap = get_fontmap (svg);
  gboolean retval;
  gboolean initial = TRUE;
  gboolean lastwasspace = FALSE;
  double x = 0;
  double y = 0;
  WhiteSpace space;
  WritingMode wmode;
  TextAnchor anchor;
  float dx, dy;

#if 0
  g_print ("---\nBEFORE\n");
  print_layouts (self, TRUE);
#endif

  for (unsigned int i = 0; i < shape->text->len; i++)
    {
      TextNode *node = &g_array_index (shape->text, TextNode, i);
      if (node->type == TEXT_NODE_CHARACTERS)
        g_assert (node->characters.layouts == NULL);
    }

  space = svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_SPACE));
  wmode = svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_WRITING_MODE));
  anchor = svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_TEXT_ANCHOR));

  retval = do_generate_layouts (shape, fontmap, space, wmode, &x, &y, &initial, &lastwasspace, NULL, viewport, bounds);

  if (lastwasspace)
    strip_trailing_space (shape);

#if 0
  g_print ("---\nAFTER\n");
  print_layouts (self, FALSE);
#endif

  if (!retval)
    return FALSE;

  dx = dy = 0;
  switch (anchor)
    {
    case TEXT_ANCHOR_START:
      break;
    case TEXT_ANCHOR_MIDDLE:
      if (svg_writing_mode_is_vertical (wmode))
        dy = - bounds->size.height / 2;
      else
        dx = - bounds->size.width / 2;
      break;
    case TEXT_ANCHOR_END:
      if (svg_writing_mode_is_vertical (wmode))
        dy = - bounds->size.height;
      else
        dx = - bounds->size.width;
      break;
    default:
      g_assert_not_reached ();
    }

  graphene_rect_init (&shape->bounds,
                      bounds->origin.x + dx, bounds->origin.y + dy,
                      bounds->size.width, bounds->size.height);
  shape->valid_bounds = TRUE;
  offset->x = dx;
  offset->y = dy;

  return retval;
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
