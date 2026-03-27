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

#include "gtksvgcontentfitprivate.h"
#include "gtksvgvalueprivate.h"

typedef struct
{
  SvgValue base;
  gboolean is_none;
  Align align_x;
  Align align_y;
  MeetOrSlice meet;
} SvgContentFit;

static gboolean
svg_content_fit_equal (const SvgValue *value0,
                       const SvgValue *value1)
{
  const SvgContentFit *v0 = (const SvgContentFit *) value0;
  const SvgContentFit *v1 = (const SvgContentFit *) value1;

  if (v0->is_none || v1->is_none)
    return v0->is_none == v1->is_none;

  return v0->align_x == v1->align_x &&
         v0->align_y == v1->align_y &&
         v0->meet == v1->meet;
}

static SvgValue *
svg_content_fit_interpolate (const SvgValue    *value0,
                             const SvgValue    *value1,
                             SvgComputeContext *context,
                             double             t)
{
  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_content_fit_accumulate (const SvgValue    *value0,
                            const SvgValue    *value1,
                            SvgComputeContext *context,
                            int                n)
{
  return NULL;
}

static void
svg_content_fit_print (const SvgValue *value,
                       GString        *string)
{
  const SvgContentFit *v = (const SvgContentFit *) value;

  if (v->is_none)
    {
      g_string_append (string, "none");
    }
  else
    {
      const char *align[] = { "Min", "Mid", "Max" };

      g_string_append_c (string, 'x');
      g_string_append (string, align[v->align_x]);
      g_string_append_c (string, 'Y');
      g_string_append (string, align[v->align_y]);
    }

  if (v->meet != MEET)
    {
      const char *meet[] = { "meet", "slice" };

      g_string_append_c (string, ' ');
      g_string_append (string, meet[v->meet]);
    }
}

static const SvgValueClass SVG_CONTENT_FIT_CLASS = {
  "SvgContentFit",
  svg_value_default_free,
  svg_content_fit_equal,
  svg_content_fit_interpolate,
  svg_content_fit_accumulate,
  svg_content_fit_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

SvgValue *
svg_content_fit_new_none (void)
{
  static SvgContentFit none = { { &SVG_CONTENT_FIT_CLASS, 0 }, 1, 0, 0, 0 };

  return (SvgValue *) &none;
}

SvgValue *
svg_content_fit_new (Align       align_x,
                     Align       align_y,
                     MeetOrSlice meet)
{
  static SvgContentFit def = { { &SVG_CONTENT_FIT_CLASS, 0 }, 0, ALIGN_MID, ALIGN_MID, MEET };
  SvgContentFit *v;

  if (align_x == ALIGN_MID && align_y == ALIGN_MID && meet == MEET)
    return svg_value_ref ((SvgValue *) &def);

  v = (SvgContentFit *) svg_value_alloc (&SVG_CONTENT_FIT_CLASS, sizeof (SvgContentFit));

  v->is_none = FALSE;
  v->align_x = align_x;
  v->align_y = align_y;
  v->meet = meet;

  return (SvgValue *) v;
}

SvgValue *
svg_content_fit_parse (GtkCssParser *parser)
{
  Align align_x;
  Align align_y;
  MeetOrSlice meet;

  if (gtk_css_parser_try_ident (parser, "none"))
    return svg_content_fit_new_none ();

  if (gtk_css_parser_try_ident (parser, "xMinYMin"))
    {
      align_x = ALIGN_MIN;
      align_y = ALIGN_MIN;
    }
  else if (gtk_css_parser_try_ident (parser, "xMinYMid"))
    {
      align_x = ALIGN_MIN;
      align_y = ALIGN_MID;
    }
  else if (gtk_css_parser_try_ident (parser, "xMinYMax"))
    {
      align_x = ALIGN_MIN;
      align_y = ALIGN_MAX;
    }
  else if (gtk_css_parser_try_ident (parser, "xMidYMin"))
    {
      align_x = ALIGN_MID;
      align_y = ALIGN_MIN;
    }
  else if (gtk_css_parser_try_ident (parser, "xMidYMid"))
    {
      align_x = ALIGN_MID;
      align_y = ALIGN_MID;
    }
  else if (gtk_css_parser_try_ident (parser, "xMidYMax"))
    {
      align_x = ALIGN_MID;
      align_y = ALIGN_MAX;
    }
  else if (gtk_css_parser_try_ident (parser, "xMaxYMin"))
    {
      align_x = ALIGN_MAX;
      align_y = ALIGN_MIN;
    }
  else if (gtk_css_parser_try_ident (parser, "xMaxYMid"))
    {
      align_x = ALIGN_MAX;
      align_y = ALIGN_MID;
    }
  else if (gtk_css_parser_try_ident (parser, "xMaxYMax"))
    {
      align_x = ALIGN_MAX;
      align_y = ALIGN_MAX;
    }
  else
    return NULL;

  gtk_css_parser_skip_whitespace (parser);
  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      if (gtk_css_parser_try_ident (parser, "meet"))
        meet = MEET;
      else if (gtk_css_parser_try_ident (parser, "slice"))
        meet = SLICE;
      else
        return NULL;
    }
  else
    {
      meet = MEET;
    }

  return svg_content_fit_new (align_x, align_y, meet);
}

gboolean
svg_content_fit_is_none (const SvgValue *value)
{
  const SvgContentFit *cf = (const SvgContentFit *) value;

  g_assert (value->class == &SVG_CONTENT_FIT_CLASS);

  return cf->is_none;
}

Align
svg_content_fit_get_align_x (const SvgValue *value)
{
  const SvgContentFit *cf = (const SvgContentFit *) value;

  g_assert (value->class == &SVG_CONTENT_FIT_CLASS);

  return cf->align_x;
}

Align
svg_content_fit_get_align_y (const SvgValue *value)
{
  const SvgContentFit *cf = (const SvgContentFit *) value;

  g_assert (value->class == &SVG_CONTENT_FIT_CLASS);

  return cf->align_y;
}

MeetOrSlice
svg_content_fit_get_meet (const SvgValue *value)
{
  const SvgContentFit *cf = (const SvgContentFit *) value;

  g_assert (value->class == &SVG_CONTENT_FIT_CLASS);

  return cf->meet;
}
