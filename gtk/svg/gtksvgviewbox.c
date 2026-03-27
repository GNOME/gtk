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

#include "gtksvgviewboxprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgstringutilsprivate.h"

typedef struct
{
  SvgValue base;
  gboolean unset;
  graphene_rect_t view_box;
} SvgViewBox;

static gboolean
svg_view_box_equal (const SvgValue *value0,
                    const SvgValue *value1)
{
  const SvgViewBox *v0 = (const SvgViewBox *) value0;
  const SvgViewBox *v1 = (const SvgViewBox *) value1;

  if (v0->unset != v1->unset)
    return FALSE;

  if (v0->unset)
    return TRUE;

  return graphene_rect_equal (&v0->view_box, &v1->view_box);
}

static SvgValue *
svg_view_box_interpolate (const SvgValue    *value0,
                          const SvgValue    *value1,
                          SvgComputeContext *context,
                          double             t)
{
  const SvgViewBox *v0 = (const SvgViewBox *) value0;
  const SvgViewBox *v1 = (const SvgViewBox *) value1;
  graphene_rect_t b;

  if (v0->unset || v1->unset)
    {
      if (t < 0.5)
        return svg_value_ref ((SvgValue *) value0);
      else
        return svg_value_ref ((SvgValue *) value1);
    }

  graphene_rect_interpolate (&v0->view_box, &v1->view_box, t, &b);

  return svg_view_box_new (&b);
}

static SvgValue *
svg_view_box_accumulate (const SvgValue    *value0,
                         const SvgValue    *value1,
                         SvgComputeContext *context,
                         int                n)
{
  return NULL;
}

static void
svg_view_box_print (const SvgValue *value,
                    GString        *string)
{
  const SvgViewBox *v = (const SvgViewBox *) value;

  if (v->unset)
    return;

  string_append_double (string, "", v->view_box.origin.x);
  string_append_double (string, " ", v->view_box.origin.y);
  string_append_double (string, " ", v->view_box.size.width);
  string_append_double (string, " ", v->view_box.size.height);
}

static const SvgValueClass SVG_VIEW_BOX_CLASS = {
  "SvgViewBox",
  svg_value_default_free,
  svg_view_box_equal,
  svg_view_box_interpolate,
  svg_view_box_accumulate,
  svg_view_box_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

SvgValue *
svg_view_box_new_unset (void)
{
  static SvgViewBox unset = { { &SVG_VIEW_BOX_CLASS, 0 }, TRUE, { { 0, 0 }, { 0, 0 } } };

  return (SvgValue *) &unset;
}

SvgValue *
svg_view_box_new (const graphene_rect_t *box)
{
  SvgViewBox *result;

  result = (SvgViewBox *) svg_value_alloc (&SVG_VIEW_BOX_CLASS, sizeof (SvgViewBox));
  result->unset = FALSE;
  graphene_rect_init_from_rect (&result->view_box, box);

  return (SvgValue *) result;
}

SvgValue *
svg_view_box_parse (GtkCssParser *parser)
{
  double v[4];
  unsigned int i;

  for (i = 0; i < 4; i++)
    {
      if (!gtk_css_parser_consume_number (parser, &v[i]))
        break;

      gtk_css_parser_skip_whitespace (parser);
      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COMMA))
        gtk_css_parser_consume_token (parser);
      gtk_css_parser_skip_whitespace (parser);
    }

  if (i < 4 || v[2] < 0 || v[3] < 0)
    return NULL;

  return (SvgValue *) svg_view_box_new (&GRAPHENE_RECT_INIT (v[0], v[1], v[2], v[3]));
}

gboolean
svg_view_box_get (const SvgValue  *value,
                  graphene_rect_t *vb)
{
  const SvgViewBox *viewbox = (const SvgViewBox *) value;

  g_assert (value->class == &SVG_VIEW_BOX_CLASS);

  graphene_rect_init_from_rect (vb, &viewbox->view_box);

  return !viewbox->unset;
}
