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

#include "gtksvgcolorprivate.h"
#include "gtksvgvalueprivate.h"
#include "gdk/gdkrgbaprivate.h"
#include "gtksvgutilsprivate.h"

typedef struct
{
  SvgValue base;
  ColorKind kind;
  GtkSymbolicColor symbolic;
  GdkColor color;
} SvgColor;

static gboolean
svg_color_equal (const SvgValue *value0,
                 const SvgValue *value1)
{
  const SvgColor *c0 = (const SvgColor *) value0;
  const SvgColor *c1 = (const SvgColor *) value1;

  if (c0->kind != c1->kind)
    return FALSE;

  switch (c0->kind)
    {
    case COLOR_CURRENT:
      return TRUE;
    case COLOR_SYMBOLIC:
      return c0->symbolic == c1->symbolic;
    case COLOR_PLAIN:
      return gdk_color_equal (&c0->color, &c1->color);
    default:
      g_assert_not_reached ();
    }
}

static SvgValue *svg_color_interpolate (const    SvgValue *v0,
                                        const    SvgValue *v1,
                                        SvgComputeContext *context,
                                        double             t);
static SvgValue *svg_color_accumulate  (const SvgValue    *v0,
                                        const SvgValue    *v1,
                                        SvgComputeContext *context,
                                        int                n);
static void      svg_color_print       (const SvgValue    *v0,
                                        GString           *string);
static double    svg_color_distance    (const SvgValue    *v0,
                                        const SvgValue    *v1);
static SvgValue * svg_color_resolve    (const SvgValue    *value,
                                        ShapeAttr          attr,
                                        unsigned int       idx,
                                        Shape             *shape,
                                        SvgComputeContext *context);

static void
svg_color_free (SvgValue *value)
{
  SvgColor *color = (SvgColor *) value;
  if (color->kind == COLOR_PLAIN)
    gdk_color_finish (&color->color);
  g_free (color);
}

static const SvgValueClass SVG_COLOR_CLASS = {
  "SvgColor",
  svg_color_free,
  svg_color_equal,
  svg_color_interpolate,
  svg_color_accumulate,
  svg_color_print,
  svg_color_distance,
  svg_color_resolve,
};

static SvgColor static_color_values[] = {
  { { &SVG_COLOR_CLASS, 0 }, .kind = COLOR_SYMBOLIC, .symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND, },
  { { &SVG_COLOR_CLASS, 0 }, .kind = COLOR_SYMBOLIC, .symbolic = GTK_SYMBOLIC_COLOR_ERROR, },
  { { &SVG_COLOR_CLASS, 0 }, .kind = COLOR_SYMBOLIC, .symbolic = GTK_SYMBOLIC_COLOR_WARNING, },
  { { &SVG_COLOR_CLASS, 0 }, .kind = COLOR_SYMBOLIC, .symbolic = GTK_SYMBOLIC_COLOR_SUCCESS, },
  { { &SVG_COLOR_CLASS, 0 }, .kind = COLOR_SYMBOLIC, .symbolic = GTK_SYMBOLIC_COLOR_ACCENT, },
  { { &SVG_COLOR_CLASS, 0 }, .kind = COLOR_CURRENT, },
  { { &SVG_COLOR_CLASS, 0 }, .kind = COLOR_PLAIN, .color = { .color_state = GDK_COLOR_STATE_SRGB, .r = 0, .g = 0, .b = 0, .a = 1 } },
  { { &SVG_COLOR_CLASS, 0 }, .kind = COLOR_PLAIN, .color = { .color_state = GDK_COLOR_STATE_SRGB, .r = 0, .g = 0, .b = 0, .a = 0 } },
};

SvgValue *
svg_color_new_symbolic (GtkSymbolicColor symbolic)
{
  return (SvgValue *) &static_color_values[symbolic];
}

SvgValue *
svg_color_new_current (void)
{
  return (SvgValue *) &static_color_values[5];
}

SvgValue *
svg_color_new_black (void)
{
  return (SvgValue *) &static_color_values[6];
}

SvgValue *
svg_color_new_transparent (void)
{
  return (SvgValue *) &static_color_values[7];
}

SvgValue *
svg_color_new_color (const GdkColor *color)
{
  SvgColor *res;

  if (gdk_color_equal (&static_color_values[6].color, color))
    return (SvgValue *) &static_color_values[6];

  res = (SvgColor *) svg_value_alloc (&SVG_COLOR_CLASS, sizeof (SvgColor));
  res->kind = COLOR_PLAIN;
  gdk_color_init_copy (&res->color, color);

  return (SvgValue *) res;
}

SvgValue *
svg_color_new_rgba (const GdkRGBA *rgba)
{
  GdkColor color;
  SvgValue *result;

  gdk_color_init_from_rgba (&color, rgba);
  result = svg_color_new_color (&color);
  gdk_color_finish (&color);

  return result;
}

ColorKind
svg_color_get_kind (const SvgValue *value)
{
  const SvgColor *color = (const SvgColor *) value;

  g_assert (value->class == &SVG_COLOR_CLASS);

  return color->kind;
}

GtkSymbolicColor
svg_color_get_symbolic (const SvgValue *value)
{
  const SvgColor *color = (const SvgColor *) value;

  g_assert (value->class == &SVG_COLOR_CLASS);
  g_assert (color->kind == COLOR_SYMBOLIC);

  return color->symbolic;
}

const GdkColor *
svg_color_get_color (const SvgValue *value)
{
  const SvgColor *color = (const SvgColor *) value;

  g_assert (value->class == &SVG_COLOR_CLASS);
  g_assert (color->kind == COLOR_PLAIN);

  return &color->color;
}

static const char *symbolic_system_color_names[] = {
    [GTK_SYMBOLIC_COLOR_FOREGROUND] = "SymbolicForeground",
    [GTK_SYMBOLIC_COLOR_ERROR] = "SymbolicError",
    [GTK_SYMBOLIC_COLOR_WARNING] = "SymbolicWarning",
    [GTK_SYMBOLIC_COLOR_SUCCESS] = "SymbolicSuccess",
    [GTK_SYMBOLIC_COLOR_ACCENT] = "SymbolicAccent",
  };

const char *
symbolic_system_color (GtkSymbolicColor sym)
{ 
  return symbolic_system_color_names[sym];
};

SvgValue *
svg_color_parse (GtkCssParser *parser)
{
  GdkRGBA rgba;

  if (gtk_css_parser_try_ident (parser, "currentColor"))
    return svg_color_new_current ();

  for (unsigned int i = 0; i < G_N_ELEMENTS (symbolic_system_color_names); i++)
    if (gtk_css_parser_try_ident (parser, symbolic_system_color_names[i]))
      return svg_color_new_symbolic (i);

  if (gdk_rgba_parser_parse (parser, &rgba))
    return svg_color_new_rgba (&rgba);

  /* gdk_rgba_parser_parse already throws an error */
  return NULL;
}

static void
svg_color_print (const SvgValue *value,
                 GString        *s)
{
  const SvgColor *color = (const SvgColor *) value;

  switch (color->kind)
    {
    case COLOR_CURRENT:
      g_string_append (s, "currentColor");
      break;
    case COLOR_SYMBOLIC:
      g_string_append (s, symbolic_system_color_names[color->symbolic]);
      break;
    case COLOR_PLAIN:
      {
        GdkColor c;

        /* Don't use gdk_color_print here until we parse
         * modern css color syntax.
         */
        gdk_color_convert (&c, GDK_COLOR_STATE_SRGB, &color->color);
        gdk_rgba_print ((const GdkRGBA *) c.values, s);
        gdk_color_finish (&c);
      }
      break;
    default:
      g_assert_not_reached ();
    }
}

static SvgValue *
svg_color_resolve (const SvgValue    *value,
                   ShapeAttr          attr,
                   unsigned int       idx,
                   Shape             *shape,
                   SvgComputeContext *context)
{
  SvgColor *color = (SvgColor *) value;

  switch (color->kind)
    {
    case COLOR_CURRENT:
      if (idx > 0)
        return svg_value_ref (shape->current[SHAPE_ATTR_COLOR]);
      else if (context->parent)
        return svg_value_ref (context->parent->current[SHAPE_ATTR_COLOR]);
      else
        return svg_color_new_black ();
    case COLOR_SYMBOLIC:
      if ((context->svg->features & GTK_SVG_EXTENSIONS) != 0)
        {
          if (color->symbolic < context->n_colors)
            return svg_color_new_rgba (&context->colors[color->symbolic]);
          else if (GTK_SYMBOLIC_COLOR_FOREGROUND < context->n_colors)
            return svg_color_new_rgba (&context->colors[GTK_SYMBOLIC_COLOR_FOREGROUND]);
          else
            return svg_color_new_black ();
        }
      else
        return svg_color_new_transparent ();
    case COLOR_PLAIN:
      return svg_value_ref ((SvgValue *) value);
    default:
      g_assert_not_reached ();
    }
}

static SvgValue *
svg_color_interpolate (const SvgValue    *value0,
                       const SvgValue    *value1,
                       SvgComputeContext *context,
                       double             t)
{
  const SvgColor *c0 = (const SvgColor *) value0;
  const SvgColor *c1 = (const SvgColor *) value1;

  if (c0->kind == c1->kind &&
      c0->kind == COLOR_PLAIN)
    {
      GdkColor c;
      SvgValue *res;

      lerp_color (t, &c0->color, &c1->color, context->interpolation, &c);
      res = svg_color_new_color (&c);
      gdk_color_finish (&c);

      return res;
    }

  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_color_accumulate (const SvgValue    *value0,
                      const SvgValue    *value1,
                      SvgComputeContext *context,
                      int                n)
{
  const SvgColor *c0 = (const SvgColor *) value0;
  const SvgColor *c1 = (const SvgColor *) value1;

  if (c0->kind != c1->kind)
    return NULL;

  if (c0->kind == COLOR_PLAIN)
    {
      GdkColor c;
      SvgValue *res;

      accumulate_color (&c0->color, &c1->color, n, context->interpolation, &c);
      res = svg_color_new_color (&c);
      gdk_color_finish (&c);

      return res;
    }

  return svg_value_ref ((SvgValue *) value0);
}

static double
svg_color_distance (const SvgValue *v0,
                    const SvgValue *v1)
{
  const SvgColor *c0 = (const SvgColor *) v0;
  const SvgColor *c1 = (const SvgColor *) v1;

  if (c0->kind != c1->kind)
    {
      g_warning ("Can't measure distance between "
                 "different kinds of color");
      return 1;
    }

  if (c0->kind == COLOR_CURRENT)
    return 0;
  else if (c0->kind == COLOR_SYMBOLIC)
    return 1;
  else
    return color_distance (&c0->color, &c1->color);
}
