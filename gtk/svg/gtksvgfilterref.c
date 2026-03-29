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

#include "gtksvgfilterrefprivate.h"
#include "gtksvgvalueprivate.h"

typedef struct
{
  SvgValue base;
  SvgFilterRefType type;
  const char *ref;
} SvgFilterRef;

static void
svg_filter_ref_free (SvgValue *value)
{
  SvgFilterRef *f = (SvgFilterRef *) value;

  if (f->type == FILTER_REF_BY_NAME)
    g_free ((gpointer) f->ref);

  g_free (f);
}

static gboolean
svg_filter_ref_equal (const SvgValue *value0,
                      const SvgValue *value1)
{
  const SvgFilterRef *f0 = (SvgFilterRef *) value0;
  const SvgFilterRef *f1 = (SvgFilterRef *) value1;

  if (f0->type != f1->type)
    return FALSE;

  if (f0->type == FILTER_REF_BY_NAME)
    return strcmp (f0->ref, f1->ref) == 0;

  return TRUE;
}

static SvgValue *
svg_filter_ref_interpolate (const SvgValue    *value0,
                            const SvgValue    *value1,
                            SvgComputeContext *context,
                            double             t)
{
  const SvgFilterRef *f0 = (const SvgFilterRef *) value0;
  const SvgFilterRef *f1 = (const SvgFilterRef *) value1;

  if (f0->type != f1->type)
    return NULL;

  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_filter_ref_accumulate (const SvgValue    *value0,
                           const SvgValue    *value1,
                           SvgComputeContext *context,
                           int                n)
{
  return NULL;
}

static void
svg_filter_ref_print (const SvgValue *value,
                      GString        *string)
{
  const SvgFilterRef *f = (const SvgFilterRef *) value;

  if (f->type != FILTER_REF_DEFAULT_SOURCE)
    g_string_append (string, f->ref);
}

static const SvgValueClass SVG_FILTER_REF_CLASS = {
  "SvgFilterRef",
  svg_filter_ref_free,
  svg_filter_ref_equal,
  svg_filter_ref_interpolate,
  svg_filter_ref_accumulate,
  svg_filter_ref_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

static SvgFilterRef filter_ref_values[] = {
     { { &SVG_FILTER_REF_CLASS, 0 }, .type = FILTER_REF_DEFAULT_SOURCE, .ref = NULL, },
     { { &SVG_FILTER_REF_CLASS, 0 }, .type = FILTER_REF_SOURCE_GRAPHIC, .ref = "SourceGraphic", },
     { { &SVG_FILTER_REF_CLASS, 0 }, .type = FILTER_REF_SOURCE_ALPHA, .ref = "SourceAlpha", },
     { { &SVG_FILTER_REF_CLASS, 0 }, .type = FILTER_REF_BACKGROUND_IMAGE, .ref = "BackgroundImage", },
     { { &SVG_FILTER_REF_CLASS, 0 }, .type = FILTER_REF_BACKGROUND_ALPHA, .ref = "BackgroundAlpha", },
     { { &SVG_FILTER_REF_CLASS, 0 }, .type = FILTER_REF_FILL_PAINT, .ref = "FillPaint", },
     { { &SVG_FILTER_REF_CLASS, 0 }, .type = FILTER_REF_STROKE_PAINT, .ref = "StrokePaint", },
  };

SvgValue *
svg_filter_ref_new (SvgFilterRefType type)
{
  g_assert (type < FILTER_REF_BY_NAME);

  return svg_value_ref ((SvgValue *) &filter_ref_values[type]);
}

SvgValue *
svg_filter_ref_new_ref (const char *ref)
{
  SvgFilterRef *f = (SvgFilterRef *) svg_value_alloc (&SVG_FILTER_REF_CLASS,
                                                      sizeof (SvgFilterRef));

  f->type = FILTER_REF_BY_NAME;
  f->ref = g_strdup (ref);

  return (SvgValue *) f;
}

SvgValue *
svg_filter_ref_parse (GtkCssParser *parser)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (filter_ref_values); i++)
    {
      if (filter_ref_values[i].ref &&
          gtk_css_parser_try_ident (parser, filter_ref_values[i].ref))
        return svg_value_ref ((SvgValue *) &filter_ref_values[i]);
    }

  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
    {
      const GtkCssToken *token = gtk_css_parser_get_token (parser);
      SvgValue *value;

      value = svg_filter_ref_new_ref (gtk_css_token_get_string (token));
      gtk_css_parser_skip (parser);

      return value;
    }

  gtk_css_parser_error_syntax (parser, "Expected a filter primitive ref value");
  return NULL;
}

SvgFilterRefType
svg_filter_ref_get_type (const SvgValue *value)
{
  const SvgFilterRef *f = (const SvgFilterRef *) value;

  g_assert (value->class == &SVG_FILTER_REF_CLASS);

  return f->type;
}

const char *
svg_filter_ref_get_ref (const SvgValue *value)
{
  const SvgFilterRef *f = (const SvgFilterRef *) value;

  g_assert (value->class == &SVG_FILTER_REF_CLASS);

  return f->ref;
}
