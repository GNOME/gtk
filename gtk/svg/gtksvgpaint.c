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

#include "gtksvgpaintprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgutilsprivate.h"
#include "gtksvgcolorprivate.h"
#include "gtksvgelementinternal.h"
#include "gdk/gdkrgbaprivate.h"

gboolean
paint_is_server (PaintKind kind)
{
  return kind == PAINT_SERVER ||
         kind == PAINT_SERVER_WITH_FALLBACK ||
         kind == PAINT_SERVER_WITH_CURRENT_COLOR;
}

typedef struct
{
  SvgValue base;
  PaintKind kind;
  union {
    GtkSymbolicColor symbolic;
    GdkColor color;
    struct {
      char *ref;
      SvgElement *shape;
      GdkColor fallback;
    } server;
  };
} SvgPaint;

static void
svg_paint_free (SvgValue *value)
{
  SvgPaint *paint = (SvgPaint *) value;

  if (paint->kind == PAINT_COLOR)
    {
      gdk_color_finish (&paint->color);
    }
  else if (paint_is_server (paint->kind))
    {
      g_free (paint->server.ref);
      gdk_color_finish (&paint->server.fallback);
    }

  g_free (value);
}

static gboolean
svg_paint_equal (const SvgValue *value0,
                 const SvgValue *value1)
{
  const SvgPaint *paint0 = (const SvgPaint *) value0;
  const SvgPaint *paint1 = (const SvgPaint *) value1;

  if (paint0->kind != paint1->kind)
    return FALSE;

  switch (paint0->kind)
    {
    case PAINT_NONE:
    case PAINT_CONTEXT_FILL:
    case PAINT_CONTEXT_STROKE:
    case PAINT_CURRENT_COLOR:
      return TRUE;
    case PAINT_SYMBOLIC:
      return paint0->symbolic == paint1->symbolic;
    case PAINT_COLOR:
      return gdk_color_equal (&paint0->color, &paint1->color);
    case PAINT_SERVER:
    case PAINT_SERVER_WITH_CURRENT_COLOR:
      return paint0->server.shape == paint1->server.shape &&
             g_strcmp0 (paint0->server.ref, paint1->server.ref) == 0;
    case PAINT_SERVER_WITH_FALLBACK:
      return paint0->server.shape == paint1->server.shape &&
             g_strcmp0 (paint0->server.ref, paint1->server.ref) == 0 &&
             gdk_color_equal (&paint0->server.fallback, &paint1->server.fallback);
    default:
      g_assert_not_reached ();
    }
}

static SvgValue *svg_paint_interpolate (const SvgValue    *v0,
                                        const SvgValue    *v1,
                                        SvgComputeContext *context,
                                        double             t);
static SvgValue *svg_paint_accumulate  (const SvgValue    *v0,
                                        const SvgValue    *v1,
                                        SvgComputeContext *context,
                                        int                n);
static void      svg_paint_print       (const SvgValue    *v0,
                                        GString           *string);
static double    svg_paint_distance    (const SvgValue    *v0,
                                        const SvgValue    *v1);
static SvgValue * svg_paint_resolve    (const SvgValue    *value,
                                        SvgProperty        attr,
                                        unsigned int       idx,
                                        SvgElement        *shape,
                                        SvgComputeContext *context);

static const SvgValueClass SVG_PAINT_CLASS = {
  "SvgPaint",
  svg_paint_free,
  svg_paint_equal,
  svg_paint_interpolate,
  svg_paint_accumulate,
  svg_paint_print,
  svg_paint_distance,
  svg_paint_resolve,
};

static const char *symbolic_colors[] = {
  [GTK_SYMBOLIC_COLOR_FOREGROUND] = "foreground",
  [GTK_SYMBOLIC_COLOR_ERROR] = "error",
  [GTK_SYMBOLIC_COLOR_WARNING] = "warning",
  [GTK_SYMBOLIC_COLOR_SUCCESS] = "success",
  [GTK_SYMBOLIC_COLOR_ACCENT] = "accent",
};

static const char *symbolic_fallbacks[] = {
  [GTK_SYMBOLIC_COLOR_FOREGROUND] = "rgb(0,0,0)",
  [GTK_SYMBOLIC_COLOR_ERROR] = "rgb(204,0,0)",
  [GTK_SYMBOLIC_COLOR_WARNING] = "rgb(245,121,0)",
  [GTK_SYMBOLIC_COLOR_SUCCESS] = "rgb(51,209,122)",
  [GTK_SYMBOLIC_COLOR_ACCENT] = "rgb(0,34,255)",
};

gboolean
parse_symbolic_color (const char       *value,
                      GtkSymbolicColor *symbolic)
{
  unsigned int u = 0;

  if (!parse_enum (value, symbolic_colors, G_N_ELEMENTS (symbolic_colors), &u))
    return FALSE;

  *symbolic = u;
  return TRUE;
}

SvgValue *
svg_paint_new_simple (PaintKind kind)
{
  static SvgPaint paint_values[] = {
    { { &SVG_PAINT_CLASS, 0 }, .kind = PAINT_NONE,           .color = { .color_state = GDK_COLOR_STATE_SRGB, .r = 0, .g = 0, .b = 0, .a = 0 } },
    { { &SVG_PAINT_CLASS, 0 }, .kind = PAINT_CONTEXT_FILL,   .color = { .color_state = GDK_COLOR_STATE_SRGB, .r = 0, .g = 0, .b = 0, .a = 0 } },
    { { &SVG_PAINT_CLASS, 0 }, .kind = PAINT_CONTEXT_STROKE, .color = { .color_state = GDK_COLOR_STATE_SRGB, .r = 0, .g = 0, .b = 0, .a = 0 } },
    { { &SVG_PAINT_CLASS, 0 }, .kind = PAINT_CURRENT_COLOR,  .color = { .color_state = GDK_COLOR_STATE_SRGB, .r = 0, .g = 0, .b = 0, .a = 0 } },
  };

  g_assert (kind < G_N_ELEMENTS (paint_values));

  return (SvgValue *) &paint_values[kind];
}

SvgValue *
svg_paint_new_none (void)
{
  return svg_paint_new_simple (PAINT_NONE);
}

SvgValue *
svg_paint_new_symbolic (GtkSymbolicColor symbolic)
{
  static SvgPaint sym[] = {
    { { &SVG_PAINT_CLASS, 0 }, .kind = PAINT_SYMBOLIC, .symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND },
    { { &SVG_PAINT_CLASS, 0 }, .kind = PAINT_SYMBOLIC, .symbolic = GTK_SYMBOLIC_COLOR_ERROR },
    { { &SVG_PAINT_CLASS, 0 }, .kind = PAINT_SYMBOLIC, .symbolic = GTK_SYMBOLIC_COLOR_WARNING },
    { { &SVG_PAINT_CLASS, 0 }, .kind = PAINT_SYMBOLIC, .symbolic = GTK_SYMBOLIC_COLOR_SUCCESS },
    { { &SVG_PAINT_CLASS, 0 }, .kind = PAINT_SYMBOLIC, .symbolic = GTK_SYMBOLIC_COLOR_ACCENT },
  };

  return (SvgValue *) &sym[symbolic];
}

static SvgPaint default_color[] = {
  { { &SVG_PAINT_CLASS, 0 }, .kind = PAINT_COLOR, .color = { .color_state = GDK_COLOR_STATE_SRGB, .r = 0, .g = 0, .b = 0, .a = 1 } },
  { { &SVG_PAINT_CLASS, 0 }, .kind = PAINT_COLOR, .color = { .color_state = GDK_COLOR_STATE_SRGB, .r = 0, .g = 0, .b = 0, .a = 0 } },
};

SvgValue *
svg_paint_new_color (const GdkColor *color)
{
  SvgValue *value;

  for (unsigned int i = 0; i < G_N_ELEMENTS (default_color); i++)
    {
      if (gdk_color_equal (color, &default_color[i].color))
        return (SvgValue *) &default_color[i];
    }

  value = svg_value_alloc (&SVG_PAINT_CLASS, sizeof (SvgPaint));
  ((SvgPaint *) value)->kind = PAINT_COLOR;
  gdk_color_init_copy (&((SvgPaint *) value)->color, color);

  return value;
}

SvgValue *
svg_paint_new_rgba (const GdkRGBA *rgba)
{
  GdkColor c;
  SvgValue *ret;

  gdk_color_init_from_rgba (&c, rgba);
  ret = svg_paint_new_color (&c);
  gdk_color_finish (&c);

  return ret;
}

SvgValue *
svg_paint_new_black (void)
{
  return (SvgValue *) &default_color[0];
}

SvgValue *
svg_paint_new_transparent (void)
{
  return (SvgValue *) &default_color[1];
}

SvgValue *
svg_paint_new_server (const char *ref)
{
  SvgPaint *paint = (SvgPaint *) svg_value_alloc (&SVG_PAINT_CLASS,
                                                  sizeof (SvgPaint));
  paint->kind = PAINT_SERVER;

  paint->server.fallback = GDK_COLOR_SRGB (0, 0, 0, 0);
  paint->server.shape = NULL;
  paint->server.ref = g_strdup (ref);

  return (SvgValue *) paint;
}

SvgValue *
svg_paint_new_server_with_fallback (const char     *ref,
                                    const GdkColor *fallback)
{
  SvgPaint *paint = (SvgPaint *) svg_value_alloc (&SVG_PAINT_CLASS,
                                                  sizeof (SvgPaint));
  paint->kind = PAINT_SERVER_WITH_FALLBACK;
  gdk_color_init_copy (&paint->server.fallback, fallback);
  paint->server.shape = NULL;
  paint->server.ref = g_strdup (ref);

  return (SvgValue *) paint;
}

SvgValue *
svg_paint_new_server_with_current_color (const char *ref)
{
  SvgPaint *paint = (SvgPaint *) svg_value_alloc (&SVG_PAINT_CLASS,
                                                  sizeof (SvgPaint));
  paint->kind = PAINT_SERVER_WITH_CURRENT_COLOR;
  paint->server.fallback = GDK_COLOR_SRGB (0, 0, 0, 1);
  paint->server.shape = NULL;
  paint->server.ref = g_strdup (ref);

  return (SvgValue *) paint;
}

SvgValue *
svg_paint_parse (GtkCssParser *parser)
{
  GdkRGBA color;

  if (gtk_css_parser_try_ident (parser, "none"))
    return svg_paint_new_simple (PAINT_NONE);

  for (GtkSymbolicColor sym = GTK_SYMBOLIC_COLOR_FOREGROUND;
       sym <= GTK_SYMBOLIC_COLOR_ACCENT;
       sym++)
    {
      if (gtk_css_parser_try_ident (parser, symbolic_system_color (sym)))
        return svg_paint_new_symbolic (sym);
    }

  if (gtk_css_parser_try_ident (parser, "context-fill"))
    return svg_paint_new_simple (PAINT_CONTEXT_FILL);

  if (gtk_css_parser_try_ident (parser, "context-stroke"))
    return svg_paint_new_simple (PAINT_CONTEXT_STROKE);

  if (gtk_css_parser_try_ident (parser, "currentColor"))
    return svg_paint_new_simple (PAINT_CURRENT_COLOR);

  if (gtk_css_parser_has_url (parser))
    {
      char *url;
      SvgValue *paint = NULL;

      url = gtk_css_parser_consume_url (parser);
      if (url)
        {
          const char *ref;
          GdkRGBA fallback = GDK_RGBA_TRANSPARENT;

          if (url[0] == '#')
            ref = url + 1;
          else
            ref = url;

          gtk_css_parser_skip_whitespace (parser);
          if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
            {
              paint = svg_paint_new_server (ref);
            }
          else if (gtk_css_parser_try_ident (parser, "currentColor"))
            {
              paint = svg_paint_new_server_with_current_color (ref);
            }
          else if (gtk_css_parser_try_ident (parser, "none") ||
                   gdk_rgba_parser_parse (parser, &fallback))
            {
              GdkColor c;
              gdk_color_init_from_rgba (&c, &fallback);
              paint = svg_paint_new_server_with_fallback (ref, &c);
              gdk_color_finish (&c);
            }

          g_free (url);
        }

      /* gdk_rgba_parser_parse already throws an error */
      return paint;
    }

  if (gdk_rgba_parser_parse (parser, &color))
    return svg_paint_new_rgba (&color);

  /* gdk_rgba_parser_parse already throws an error */
  return NULL;
}

SvgValue *
svg_paint_parse_gpa (const char *string)
{
  GtkCssParser *parser = parser_new_for_string (string);
  GtkSymbolicColor symbolic;
  GdkRGBA rgba;
  SvgValue *value;

  gtk_css_parser_skip_whitespace (parser);
  if (gtk_css_parser_try_ident (parser, "none"))
    value = svg_paint_new_none ();
  else if (gtk_css_parser_try_ident (parser, "context-fill"))
    value = svg_paint_new_simple (PAINT_CONTEXT_FILL);
  else if (gtk_css_parser_try_ident (parser, "context-stroke"))
    value = svg_paint_new_simple (PAINT_CONTEXT_STROKE);
  else if (gtk_css_parser_try_ident (parser, "currentColor"))
    value = svg_paint_new_simple (PAINT_CURRENT_COLOR);
  else if (parser_try_enum (parser,
                            symbolic_colors, G_N_ELEMENTS (symbolic_colors),
                            (unsigned int *) &symbolic))
    value = svg_paint_new_symbolic (symbolic);
  else if (gdk_rgba_parser_parse (parser, &rgba))
    value = svg_paint_new_rgba (&rgba);
  else
    value = NULL;

  gtk_css_parser_skip_whitespace (parser);
  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    g_clear_pointer (&value, svg_value_unref);
  gtk_css_parser_unref (parser);

  return value;
}

static void
svg_paint_print (const SvgValue *value,
                 GString        *s)
{
  const SvgPaint *paint = (const SvgPaint *) value;

  switch (paint->kind)
    {
    case PAINT_NONE:
      g_string_append (s, "none");
      break;

    case PAINT_CONTEXT_FILL:
      g_string_append (s, "context-fill");
      break;

    case PAINT_CONTEXT_STROKE:
      g_string_append (s, "context-stroke");
      break;

    case PAINT_CURRENT_COLOR:
      g_string_append (s, "currentColor");
      break;

    case PAINT_COLOR:
      {
        GdkColor c;
        /* Don't use gdk_color_print here until we parse
         * modern css color syntax.
         */
        gdk_color_convert (&c, GDK_COLOR_STATE_SRGB, &paint->color);
        gdk_rgba_print ((const GdkRGBA *) &c.values, s);
        gdk_color_finish (&c);
      }
      break;

    case PAINT_SYMBOLIC:
      g_string_append_printf (s, "url(#gpa:%s) %s",
                              symbolic_colors[paint->symbolic],
                              symbolic_fallbacks[paint->symbolic]);
      break;

    case PAINT_SERVER:
      {
        GtkSymbolicColor symbolic;

        g_string_append_printf (s, "url(#%s)", paint->server.ref);
        if (g_str_has_prefix (paint->server.ref, "gpa:") &&
            parse_symbolic_color (paint->server.ref + strlen ("gpa:"), &symbolic))
          {
            g_string_append_c (s, ' ');
            g_string_append (s, symbolic_fallbacks[symbolic]);
          }
        }
      break;

    case PAINT_SERVER_WITH_FALLBACK:
      {
        GdkColor c;

        g_string_append_printf (s, "url(#%s) ", paint->server.ref);

        /* Don't use gdk_color_print here until we parse
         * modern css color syntax.
         */
        gdk_color_convert (&c, GDK_COLOR_STATE_SRGB, &paint->server.fallback);
        gdk_rgba_print ((const GdkRGBA *) &c.values, s);
        gdk_color_finish (&c);
      }
      break;

    case PAINT_SERVER_WITH_CURRENT_COLOR:
      g_string_append_printf (s, "url(#%s) currentColor", paint->server.ref);
      break;

    default:
      g_assert_not_reached ();
    }
}

void
svg_paint_print_gpa (const SvgValue *value,
                     GString        *s)
{
  const SvgPaint *paint = (const SvgPaint *) value;

  switch (paint->kind)
    {
    case PAINT_CONTEXT_FILL:
      g_string_append (s, "context-fill");
      break;

    case PAINT_CONTEXT_STROKE:
      g_string_append (s, "context-stroke");
      break;

    case PAINT_CURRENT_COLOR:
      g_string_append (s, "currentColor");
      break;

    case PAINT_COLOR:
      {
        GdkColor c;
        /* Don't use gdk_color_print here until we parse
         * modern css color syntax.
         */
        gdk_color_convert (&c, GDK_COLOR_STATE_SRGB, &paint->color);
        gdk_rgba_print ((const GdkRGBA *) &c.values, s);
        gdk_color_finish (&c);
      }
      break;

    case PAINT_SYMBOLIC:
      g_string_append_printf (s, "%s", symbolic_colors[paint->symbolic]);
      break;

    case PAINT_NONE:
    case PAINT_SERVER:
    case PAINT_SERVER_WITH_FALLBACK:
    case PAINT_SERVER_WITH_CURRENT_COLOR:
    default:
      g_assert_not_reached ();
    }
}

gboolean
svg_value_is_paint (const SvgValue *value)
{
  return value->class == &SVG_PAINT_CLASS;
}

PaintKind
svg_paint_get_kind (const SvgValue *value)
{
  const SvgPaint *paint = (const SvgPaint *) value;

  g_assert (value->class == &SVG_PAINT_CLASS);

  return paint->kind;
}

gboolean
svg_paint_is_symbolic (const SvgValue   *value,
                       GtkSymbolicColor *symbolic)
{
  const SvgPaint *paint = (const SvgPaint *) value;

  g_assert (value->class == &SVG_PAINT_CLASS);

  if (paint->kind == PAINT_SYMBOLIC)
    {
      *symbolic = paint->symbolic;
      return TRUE;
    }
  else if (paint_is_server (paint->kind))
    {
      if (g_str_has_prefix (paint->server.ref, "gpa:") &&
          parse_symbolic_color (paint->server.ref + strlen ("gpa:"), symbolic))
        return TRUE;
    }

  return FALSE;
}

const GdkColor *
svg_paint_get_color (const SvgValue *value)
{
  const SvgPaint *paint = (const SvgPaint *) value;

  g_assert (value->class == &SVG_PAINT_CLASS);
  g_assert (paint->kind == PAINT_COLOR);

  return &paint->color;
}

GtkSymbolicColor
svg_paint_get_symbolic (const SvgValue *value)
{
  const SvgPaint *paint = (const SvgPaint *) value;

  g_assert (value->class == &SVG_PAINT_CLASS);
  g_assert (paint->kind == PAINT_SYMBOLIC);

  return paint->symbolic;
}

const char *
svg_paint_get_server_ref (const SvgValue *value)
{
  const SvgPaint *paint = (const SvgPaint *) value;

  g_assert (value->class == &SVG_PAINT_CLASS);
  g_assert (paint_is_server (paint->kind));

  return paint->server.ref;
}

SvgElement *
svg_paint_get_server_shape (const SvgValue *value)
{
  const SvgPaint *paint = (const SvgPaint *) value;

  g_assert (value->class == &SVG_PAINT_CLASS);
  g_assert (paint_is_server (paint->kind));

  return paint->server.shape;
}

const GdkColor *
svg_paint_get_server_fallback (const SvgValue *value)
{
  const SvgPaint *paint = (const SvgPaint *) value;

  g_assert (value->class == &SVG_PAINT_CLASS);
  g_assert (paint_is_server (paint->kind));

  return &paint->server.fallback;
}

static SvgValue *
svg_paint_resolve (const SvgValue    *value,
                   SvgProperty        attr,
                   unsigned int       idx,
                   SvgElement        *shape,
                   SvgComputeContext *context)
{
  const SvgPaint *paint = (const SvgPaint *) value;
  GtkSymbolicColor symbolic;

  g_assert (value->class == &SVG_PAINT_CLASS);

  if (paint->kind == PAINT_CURRENT_COLOR)
    {
      SvgValue *color = svg_element_get_current_value (shape, SVG_PROPERTY_COLOR);
      switch (svg_color_get_kind (color))
        {
        case COLOR_PLAIN:
          return svg_paint_new_color (svg_color_get_color (color));
        case COLOR_SYMBOLIC:
          return svg_paint_new_symbolic (svg_color_get_symbolic (color));
        case COLOR_CURRENT:
          return svg_paint_new_black ();
        default:
          g_assert_not_reached ();
        }
    }

  if ((context->svg->features & GTK_SVG_EXTENSIONS) != 0)
    {
      if (svg_paint_is_symbolic (value, &symbolic))
        {
          if (symbolic < context->n_colors)
            return svg_paint_new_rgba (&context->colors[symbolic]);
          else if (GTK_SYMBOLIC_COLOR_FOREGROUND < context->n_colors)
            return svg_paint_new_rgba (&context->colors[GTK_SYMBOLIC_COLOR_FOREGROUND]);
          else
            return svg_paint_new_black ();
        }
    }
  else
    {
      if (paint->kind == PAINT_SYMBOLIC)
        return svg_paint_new_transparent ();
    }

  if (paint->kind == PAINT_SERVER_WITH_CURRENT_COLOR)
    {
      const GdkColor *color = svg_color_get_color (svg_element_get_current_value (shape, SVG_PROPERTY_COLOR));
      return svg_paint_new_server_with_fallback (paint->server.ref, color);
    }

  return svg_value_ref ((SvgValue *) value);
}

static SvgValue *
svg_paint_interpolate (const SvgValue    *value0,
                       const SvgValue    *value1,
                       SvgComputeContext *context,
                       double             t)
{
  const SvgPaint *p0 = (const SvgPaint *) value0;
  const SvgPaint *p1 = (const SvgPaint *) value1;

  if (p0->kind == PAINT_COLOR && p1->kind == PAINT_COLOR)
    {
      GdkColor c;
      SvgValue *ret;

      lerp_color (t, &p0->color, &p1->color, context->interpolation, &c);
      ret = svg_paint_new_color (&c);
      gdk_color_finish (&c);

      return ret;
    }

  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_paint_accumulate (const SvgValue    *value0,
                      const SvgValue    *value1,
                      SvgComputeContext *context,
                      int                n)
{
  const SvgPaint *p0 = (const SvgPaint *) value0;
  const SvgPaint *p1 = (const SvgPaint *) value1;
  SvgPaint *paint;

  if (p0->kind != p1->kind)
    return NULL;

  if (p0->kind == PAINT_COLOR)
    {
      paint = (SvgPaint *) svg_value_alloc (&SVG_PAINT_CLASS, sizeof (SvgPaint));

      paint->kind = PAINT_COLOR;
      accumulate_color (&p0->color, &p1->color, n, context->interpolation, &paint->color);

      return (SvgValue *) paint;
    }

  return svg_value_ref ((SvgValue *) value0);
}

static double
svg_paint_distance (const SvgValue *v0,
                    const SvgValue *v1)
{
  const SvgPaint *p0 = (const SvgPaint *) v0;
  const SvgPaint *p1 = (const SvgPaint *) v1;

  if (p0->kind == p1->kind)
    {
      if (p0->kind == PAINT_COLOR)
        return color_distance (&p0->color, &p1->color);
      else if (p0->kind == PAINT_NONE ||
               p0->kind == PAINT_CONTEXT_FILL ||
               p0->kind == PAINT_CONTEXT_STROKE ||
               p0->kind == PAINT_CURRENT_COLOR)
        return 0;
    }

  g_warning ("Can't measure distance between these paint values");
  return 1;
}

void
svg_paint_set_server_shape (SvgValue   *value,
                            SvgElement *shape)
{
  SvgPaint *paint = (SvgPaint *) value;

  g_assert (value->class == &SVG_PAINT_CLASS);
  g_assert (paint_is_server (paint->kind));

  paint->server.shape = shape;
}
