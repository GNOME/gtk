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

#include "gtksvgclipprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgpathdataprivate.h"

#define FILL_RULE_OMITTED 0xffff

typedef struct
{
  SvgValue base;
  ClipKind kind;

  union {
    struct {
      SvgPathData *pdata;
      GskPath *path;
      unsigned int fill_rule;
    } path;
    struct {
      char *ref;
      Shape *shape;
    } ref;
  };
} SvgClip;

static void
svg_clip_free (SvgValue *value)
{
  SvgClip *clip = (SvgClip *) value;

  if (clip->kind == CLIP_PATH)
    {
      gsk_path_unref (clip->path.path);
      svg_path_data_free (clip->path.pdata);
    }
  else if (clip->kind == CLIP_URL)
    {
      g_free (clip->ref.ref);
    }

  g_free (value);
}

static gboolean
svg_clip_equal (const SvgValue *value0,
                const SvgValue *value1)
{
  const SvgClip *c0 = (const SvgClip *) value0;
  const SvgClip *c1 = (const SvgClip *) value1;

  if (c0->kind != c1->kind)
    return FALSE;

  switch (c0->kind)
    {
    case CLIP_NONE:
      return TRUE;
    case CLIP_PATH:
      return c0->path.fill_rule == c1->path.fill_rule &&
             gsk_path_equal (c0->path.path, c1->path.path);
    case CLIP_URL:
      return c0->ref.shape == c1->ref.shape;
    default:
      g_assert_not_reached ();
    }
}

static SvgValue * svg_clip_interpolate (const SvgValue    *value0,
                                        const SvgValue    *value1,
                                        SvgComputeContext *context,
                                        double             t);

static SvgValue *
svg_clip_accumulate (const SvgValue    *value0,
                     const SvgValue    *value1,
                     SvgComputeContext *context,
                     int                n)
{
  return NULL;
}

static void
svg_clip_print (const SvgValue *value,
                GString        *string)
{
  const SvgClip *c = (const SvgClip *) value;
  const char *rule[] = {
    [GSK_FILL_RULE_WINDING] = "nonzero",
    [GSK_FILL_RULE_EVEN_ODD] = "evenodd"
  };

  switch (c->kind)
    {
    case CLIP_NONE:
      g_string_append (string, "none");
      break;
    case CLIP_PATH:
      g_string_append (string, "path(");
      if (c->path.fill_rule != FILL_RULE_OMITTED)
        {
          g_string_append (string, rule[c->path.fill_rule]);
          g_string_append (string, ", ");
        }
      g_string_append (string, "\"");
      svg_path_data_print (c->path.pdata, string);
      g_string_append (string, "\")");
      break;
    case CLIP_URL:
      g_string_append_printf (string, "url(%s)", c->ref.ref);
      break;
    default:
      g_assert_not_reached ();
    }
}

static const SvgValueClass SVG_CLIP_CLASS = {
  "SvgClip",
  svg_clip_free,
  svg_clip_equal,
  svg_clip_interpolate,
  svg_clip_accumulate,
  svg_clip_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

SvgValue *
svg_clip_new_none (void)
{
  static SvgClip none = { { &SVG_CLIP_CLASS, 0 }, CLIP_NONE };
  return (SvgValue *) &none;
}

static SvgValue *
svg_clip_new_from_data (SvgPathData  *pdata,
                        unsigned int  fill_rule)
{
  SvgClip *result;

  if (pdata == NULL)
    return NULL;

  result = (SvgClip *) svg_value_alloc (&SVG_CLIP_CLASS, sizeof (SvgClip));
  result->kind = CLIP_PATH;
  result->path.pdata = pdata;
  result->path.path = svg_path_data_to_gsk (pdata);
  result->path.fill_rule = fill_rule;
  return (SvgValue *) result;
}

SvgValue *
svg_clip_new_path (const char   *string,
                   unsigned int  fill_rule)
{
  return svg_clip_new_from_data (svg_path_data_parse (string), fill_rule);
}

SvgValue *
svg_clip_new_url_take (const char *string)
{
  SvgClip *result;

  result = (SvgClip *) svg_value_alloc (&SVG_CLIP_CLASS, sizeof (SvgClip));
  result->kind = CLIP_URL;
  result->ref.ref = (char *) string;
  result->ref.shape = NULL;

  return (SvgValue *) result;
}

static SvgValue *
svg_clip_interpolate (const SvgValue    *value0,
                      const SvgValue    *value1,
                      SvgComputeContext *context,
                      double             t)
{
  const SvgClip *c0 = (const SvgClip *) value0;
  const SvgClip *c1 = (const SvgClip *) value1;

  if (c0->kind == c1->kind)
    {
      switch (c0->kind)
        {
        case CLIP_NONE:
          return svg_clip_new_none ();

        case CLIP_PATH:
          {
            SvgPathData *p;
            unsigned int fill_rule;

            p = svg_path_data_interpolate (c0->path.pdata,
                                           c1->path.pdata,
                                           t);

            if (t < 0.5)
              fill_rule = c0->path.fill_rule;
            else
              fill_rule = c1->path.fill_rule;

            if (p)
              return svg_clip_new_from_data (p, fill_rule);
          }
          break;

        case CLIP_URL:
          break;

        default:
          g_assert_not_reached ();
        }
    }

  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

typedef struct
{
  unsigned int fill_rule;
  char *string;
} ClipPathArgs;

static unsigned int
parse_clip_path_arg (GtkCssParser *parser,
                     unsigned int  n,
                     gpointer      data)
{
  ClipPathArgs *args = data;

  if (n == 0)
    {
      if (gtk_css_parser_try_ident (parser, "nonzero"))
        {
          args->fill_rule = GSK_FILL_RULE_WINDING;
          return 1;
        }
      else if (gtk_css_parser_try_ident (parser, "evenodd"))
        {
          args->fill_rule = GSK_FILL_RULE_EVEN_ODD;
          return 1;
        }
    }

  args->string = gtk_css_parser_consume_string (parser);
  if (!args->string)
    return 0;

  return 1;
}

SvgValue *
svg_clip_parse (GtkCssParser *parser)
{
  SvgValue *value = NULL;

  if (gtk_css_parser_try_ident (parser, "none"))
    return svg_clip_new_none ();

  if (gtk_css_parser_has_function (parser, "path"))
    {
      ClipPathArgs args = { .fill_rule = FILL_RULE_OMITTED, .string = NULL, };
      GtkCssLocation start;

      start = *gtk_css_parser_get_start_location (parser);
      if (gtk_css_parser_consume_function (parser, 1, 2, parse_clip_path_arg, &args))
        {
          SvgPathData *data = NULL;

          if (!svg_path_data_parse_full (args.string, &data))
            {
              GError *error;

              error = g_error_new_literal (GTK_CSS_PARSER_ERROR,
                                           GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                                           "Path data is invalid");

              gtk_css_parser_emit_error (parser,
                                         &start,
                                         gtk_css_parser_get_end_location (parser),
                                         error);
              g_error_free (error);
            }

          value = svg_clip_new_from_data (data, args.fill_rule);
          g_free (args.string);
        }
    }
  else
    {
      char *url = gtk_css_parser_consume_url (parser);
      if (url != NULL)
        value = svg_clip_new_url_take (url);
    }

  if (value == NULL)
    gtk_css_parser_error_syntax (parser, "Expected a path");

  return value;
}

ClipKind
svg_clip_get_kind (const SvgValue *value)
{
  const SvgClip *clip = (const SvgClip *) value;

  g_assert (value->class == &SVG_CLIP_CLASS);

  return clip->kind;
}

const char *
svg_clip_get_id (const SvgValue *value)
{
  const SvgClip *clip = (const SvgClip *) value;

  g_assert (value->class == &SVG_CLIP_CLASS);
  g_assert (clip->kind == CLIP_URL);

  if (clip->ref.ref[0] == '#')
    return clip->ref.ref + 1;
  else
    return clip->ref.ref;
}

Shape *
svg_clip_get_shape (const SvgValue *value)
{
  const SvgClip *clip = (const SvgClip *) value;

  g_assert (value->class == &SVG_CLIP_CLASS);
  g_assert (clip->kind == CLIP_URL);

  return clip->ref.shape;
}

GskFillRule
svg_clip_get_fill_rule (const SvgValue *value)
{
  const SvgClip *clip = (const SvgClip *) value;

  g_assert (value->class == &SVG_CLIP_CLASS);
  g_assert (clip->kind == CLIP_PATH);

  return clip->path.fill_rule;
}

GskPath *
svg_clip_get_path (const SvgValue *value)
{
  const SvgClip *clip = (const SvgClip *) value;

  g_assert (value->class == &SVG_CLIP_CLASS);
  g_assert (clip->kind == CLIP_PATH);

  return clip->path.path;
}

void
svg_clip_set_shape (SvgValue *value,
                    Shape    *shape)
{
  SvgClip *clip = (SvgClip *) value;

  g_assert (value->class == &SVG_CLIP_CLASS);
  g_assert (clip->kind == CLIP_URL);

  clip->ref.shape = shape;
}
