/*
 * Copyright © 2025 Red Hat, Inc
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtksvgprivate.h"

#include "gtkenums.h"
#include "gtkmain.h"
#include "gtksymbolicpaintable.h"
#include "gtktypebuiltins.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtk/css/gtkcssdataurlprivate.h"
#include "gtksnapshotprivate.h"
#include "gsk/gskarithmeticnodeprivate.h"
#include "gsk/gskblendnodeprivate.h"
#include "gsk/gskcolormatrixnodeprivate.h"
#include "gsk/gskcolornodeprivate.h"
#include "gsk/gskcomponenttransfernodeprivate.h"
#include "gsk/gskdisplacementnodeprivate.h"
#include "gsk/gskisolationnodeprivate.h"
#include "gsk/gskrepeatnodeprivate.h"
#include "gsk/gskpathprivate.h"
#include "gsk/gskcontourprivate.h"
#include "gsk/gskrectprivate.h"
#include "gsk/gskroundedrectprivate.h"
#include <glib/gstdio.h>

#include <tgmath.h>
#include <stdint.h>

#ifdef HAVE_PANGOFT
#include <pango/pangofc-fontmap.h>
#endif

#include "gdk/gdkrgbaprivate.h"

/**
 * GtkSvg:
 *
 * A paintable implementation that renders SVG, with animations.
 *
 * `GtkSvg` objects are created by parsing a subset of SVG,
 * including SVG animations.
 *
 * The `GtkSvg` fills or strokes paths with symbolic or fixed
 * colors. It can have multiple states, and paths can be included
 * in a subset of the states. The special 'empty' state is always
 * available. States can have animations, and the transition between
 * different states can also be animated.
 *
 * To find out what states a `GtkSvg` has, use [method@Gtk.Svg.get_n_states].
 * To set the current state, use [method@Gtk.Svg.set_state].
 *
 * To play the animations in an SVG file, use
 * [method@Gtk.Svg.set_frame_clock] to connect the paintable to a
 * frame clock, and then call [method@Gtk.Svg.play] to start animations.
 *
 *
 * ## Error handling
 *
 * Loading an SVG into `GtkSvg` will always produce a (possibly empty)
 * paintable. GTK will drop things that it can't handle and try to make
 * sense of the rest.
 *
 * To track errors during parsing or rendering, connect to the
 * [signal@Gtk.Svg::error] signal.
 *
 * For parsing errors in the `GTK_SVG_ERROR` domain, the functions
 * [func@Gtk.SvgError.get_start], [func@Gtk.SvgError.get_end],
 * [func@Gtk.SvgError.get_element] and [func@Gtk.SvgError.get_attribute]
 * can be used to obtain information about where the error occurred.
 *
 *
 * ## The supported subset of SVG
 *
 * The paintable supports much of SVG 2, with some exceptions.
 *
 * Among the graphical elements, `<textPath>` and `<foreignObject>`
 * are not supported.
 *
 * Among the structural elements, `<a>` and `<view>` are not supported.
 *
 * All filter functions are supported, plus a custom `alpha-level()`
 * function, which implements one particular case of feComponentTransfer.
 *
 * In the `<filter>` element, the following primitives are not
 * supported: feConvolveMatrix, feDiffuseLighting,
 * feMorphology, feSpecularLighting and feTurbulence.
 *
 * Support for the `mask` attribute is limited to just a url
 * referring to the `<mask>` element by ID.
 *
 * In animation elements, the parsing of `begin` and `end` attributes
 * is limited, and the `min` and `max` attributes are not supported.
 *
 * Lastly, there is only minimal CSS support (the style attribute,
 * but not `<style>`), and no interactivity.
 *
 *
 * ## SVG Extensions
 *
 * The paintable supports a number of [custom attributes](icon-format.html)
 * that offer a convenient way to define states, transitions and animations.
 * For example,
 *
 *     <circle cx='5' cy='5' r='5'
 *             gpa:states='0 1'
 *             gpa:animation-type='automatic'
 *             gpa:animation-direction='segment'
 *             gpa:animation-duration='600ms'/>
 *
 * defines the circle to be shown in states 0 and 1, and animates a segment
 * of the circle.
 *
 * <image src="svg-renderer1.svg">
 *
 * Note that the generated animations assume a `pathLengh` value of 1.
 * Setting `pathLength` in your SVG is therefore going to interfere with
 * generated animations.
 *
 * To connect general SVG animations to the states of the paintable,
 * use the custom `gpa:states(...)` condition in the `begin` and `end`
 * attributes of SVG animation elements. For example,
 *
 *     <animate href='path1'
 *              attributeName='fill'
 *              begin='gpa:states(0).begin'
 *              dur='300ms'
 *              fill='freeze'
 *              from='black'
 *              to='magenta'/>
 *
 * will make the fill color of path1 transition from black to
 * magenta when the renderer enters state 0.
 *
 * <image src="svg-renderer2.svg">
 *
 * The `gpa:states(...)` condition triggers for upcoming state changes
 * as well, to support fade-out transitions. For example,
 *
 *     <animate href='path1'
 *              attributeName='opacity'
 *              begin='gpa:states(0).end -300ms'
 *              dur='300ms'
 *              fill='freeze'
 *              from='1'
 *              to='0'/>
 *
 * will start a fade-out of path1 300ms before state 0 ends.
 *
 * In addition to the `gpa:fill` and `gpa:stroke` attributes, symbolic
 * colors can also be specified as a custom paint server reference,
 * like this: `url(gpa:#warning)`. This works in `fill` and `stroke`
 * attributes, but also when specifying colors in SVG animation
 * attributes like `to` or `values`.
 *
 * Note that the SVG syntax allows for a fallback RGB color to be
 * specified after the url, for compatibility with other SVG consumers:
 *
 *     fill='url(gpa:#warning) orange'
 *
 * In contrast to SVG 1.1 and 2.0, we allow the `transform` attribute
 * to be animated with `<animate>`.
 *
 *
 * Since: 4.22
 */

/* Max. nesting level of paint calls we allow */
#define NESTING_LIMIT 256

/* This is a mitigation for SVG files which create millions of elements
 * in an attempt to exhaust memory.  We don't allow loading more than
 * this number of elements during the initial streaming load process.
 */
#define LOADING_LIMIT 50000

/* This is a mitigation for the security-related bugs:
 * https://gitlab.gnome.org/GNOME/librsvg/issues/323
 * https://gitlab.gnome.org/GNOME/librsvg/issues/515
 *
 * Imagine the XML [billion laughs attack], but done in SVG's terms:
 *
 * - #323 above creates deeply nested groups of `<use>` elements.
 * The first one references the second one ten times, the second one
 * references the third one ten times, and so on.  In the file given,
 * this causes 10^17 objects to be rendered.  While this does not
 * exhaust memory, it would take a really long time.
 *
 * - #515 has deeply nested references of `<pattern>` elements.  Each
 * object inside each pattern has an attribute
 * fill="url(#next_pattern)", so the number of final rendered objects
 * grows exponentially.
 *
 * We deal with both cases by placing a limit on how many references
 * will be resolved during the SVG rendering process, that is,
 * how many `url(#foo)` will be resolved.
 *
 * [billion laughs attack]: https://bitbucket.org/tiran/defusedxml
 */
#define DRAWING_LIMIT 150000

#define DEFAULT_FONT_SIZE 13.333

#ifndef _MSC_VER
#define DEBUG
#endif /* _MSC_VER */

#define ALIGN_XY(x,y) ((x) | ((y) << 2))

#define ALIGN_GET_X(x) ((x) & 3)
#define ALIGN_GET_Y(x) ((x) >> 2)

#define DEG_TO_RAD(x) ((x) * G_PI / 180)

typedef struct _Animation Animation;

#define BIT(n) (G_GUINT64_CONSTANT (1) << (n))

static SvgValue *shape_attr_ref_initial_value (ShapeAttr  attr,
                                               ShapeType  type,
                                               gboolean   has_parent);

static unsigned int svg_enum_get (const SvgValue *value);

typedef enum
{
  COORD_UNITS_USER_SPACE_ON_USE,
  COORD_UNITS_OBJECT_BOUNDING_BOX,
} CoordUnits;

/* {{{ Some debug tools */

#ifdef DEBUG

static int64_t time_base;

static const char *
format_time_buf (char    *buf,
                 size_t   size,
                 int64_t  time)
{
  if (time == INDEFINITE)
    return "indefinite";
  else
    return g_ascii_formatd (buf, size, "%.3fs", (time - time_base) / (double) G_TIME_SPAN_SECOND);
}

static const char *
format_time (int64_t time)
{
  static char buf[64];
  return format_time_buf (buf, 64, time);
}

#if 0
static void
print_rect (const char *label,
            const graphene_rect_t *rect)
{
  g_print ("%s: %g %g %g %g\n",
           label,
           rect->origin.x, rect->origin.y,
           rect->size.width, rect->size.height);
}
#endif

#define dbg_print(cond, fmt,...) \
  G_STMT_START { \
    if (strstr (g_getenv ("SVG_DEBUG") ?:"", cond)) \
      { \
        char buf[64]; \
        g_print ("%s: ", format_time_buf (buf, 64, g_get_monotonic_time ())); \
        g_print (fmt __VA_OPT__(,) __VA_ARGS__); \
        g_print ("\n"); \
      } \
  } G_STMT_END

#else
#define dbg_print(cond,fmt,...)
#define print_rect (label, rect)
#endif

/* }}} */
/* {{{ Errors */

static void
gtk_svg_location_init (GtkSvgLocation      *location,
                       GMarkupParseContext *context)
{
  int lines, chars;
  g_markup_parse_context_get_position (context, &lines, &chars);
  location->lines = lines;
  location->line_chars = chars;
#if GLIB_CHECK_VERSION (2, 88, 0)
  location->bytes = g_markup_parse_context_get_offset (context);
#else
  location->bytes = 0;
#endif
}

typedef struct
{
  char *element;
  char *attribute;
  GtkSvgLocation start;
  GtkSvgLocation end;
} GtkSvgErrorPrivate;

static void
gtk_svg_error_private_init (GtkSvgErrorPrivate *priv)
{
}

static void
gtk_svg_error_private_copy (const GtkSvgErrorPrivate *src,
                            GtkSvgErrorPrivate       *dest)
{
  g_assert (dest != NULL);
  g_assert (src != NULL);
  dest->element = g_strdup (src->element);
  dest->attribute = g_strdup (src->attribute);
  dest->start = src->start;
  dest->end = src->end;
}

static void
gtk_svg_error_private_clear (GtkSvgErrorPrivate *priv)
{
  g_assert (priv != NULL);
  g_free (priv->element);
  g_free (priv->attribute);
}

G_DEFINE_EXTENDED_ERROR (GtkSvgError, gtk_svg_error);

static void
gtk_svg_error_set_element (GError     *error,
                           const char *element)
{
  GtkSvgErrorPrivate *priv = gtk_svg_error_get_private (error);
  g_assert (error->domain == GTK_SVG_ERROR);
  priv->element = g_strdup (element);
}

static void
gtk_svg_error_set_attribute (GError     *error,
                             const char *attribute)
{
  GtkSvgErrorPrivate *priv = gtk_svg_error_get_private (error);
  g_assert (error->domain == GTK_SVG_ERROR);
  priv->attribute = g_strdup (attribute);
}

static void
gtk_svg_error_set_location (GError               *error,
                            const GtkSvgLocation *start,
                            const GtkSvgLocation *end)
{
  GtkSvgErrorPrivate *priv = gtk_svg_error_get_private (error);
  g_assert (error->domain == GTK_SVG_ERROR);
  if (start)
    priv->start = *start;
  if (end)
    priv->end = *end;
}

static unsigned int error_signal;

static void
gtk_svg_emit_error (GtkSvg       *svg,
                    const GError *error)
{
  g_signal_emit (svg, error_signal, 0, error);
}

G_GNUC_PRINTF (5, 6)
static void
gtk_svg_invalid_element (GtkSvg               *self,
                         const char           *parent_element,
                         const GtkSvgLocation *start,
                         const GtkSvgLocation *end,
                         const char           *format,
                         ...)
{
  GError *error;
  va_list args;

  va_start (args, format);
  error = g_error_new_valist (GTK_SVG_ERROR,
                              GTK_SVG_ERROR_INVALID_ELEMENT,
                              format, args);
  va_end (args);

  gtk_svg_error_set_element (error, parent_element);
  gtk_svg_error_set_location (error, start, end);

  gtk_svg_emit_error (self, error);
  g_clear_error (&error);
}

G_GNUC_PRINTF (4, 5)
static void
gtk_svg_invalid_attribute (GtkSvg               *self,
                           GMarkupParseContext  *context,
                           const char           *attr_name,
                           const char           *format,
                           ...)
{
  GError *error;
  GtkSvgLocation location;

  if (format)
    {
      va_list args;
      va_start (args, format);
      error = g_error_new_valist (GTK_SVG_ERROR,
                                  GTK_SVG_ERROR_INVALID_ATTRIBUTE,
                                  format, args);
      va_end (args);
    }
  else
    {
      error = g_error_new (GTK_SVG_ERROR,
                           GTK_SVG_ERROR_INVALID_ATTRIBUTE,
                           "Invalid attribute: %s", attr_name);
    }

  gtk_svg_error_set_element (error, g_markup_parse_context_get_element (context));
  gtk_svg_error_set_attribute (error, attr_name);
  gtk_svg_location_init (&location, context);
  gtk_svg_error_set_location (error, &location, &location);

  gtk_svg_emit_error (self, error);
  g_error_free (error);
}

static void
gtk_svg_markup_error (GtkSvg              *self,
                      GMarkupParseContext *context,
                      const GError        *markup_error)
{
  GError *error;
  GtkSvgLocation start, end;

  error = g_error_new_literal (GTK_SVG_ERROR,
                               GTK_SVG_ERROR_INVALID_SYNTAX,
                               markup_error->message);

  gtk_svg_error_set_element (error, g_markup_parse_context_get_element (context));
  gtk_svg_location_init (&start, context);
  gtk_svg_location_init (&end, context);
  end.bytes += 1;
  end.line_chars += 1;
  gtk_svg_error_set_location (error, &start, &end);

  gtk_svg_emit_error (self, error);
  g_clear_error (&error);
}

G_GNUC_PRINTF (4, 5)
static void
gtk_svg_missing_attribute (GtkSvg               *self,
                           GMarkupParseContext  *context,
                           const char           *attr_name,
                           const char           *format,
                           ...)
{
  GError *error;
  GtkSvgLocation location;

  if (format)
    {
      va_list args;
      va_start (args, format);
      error = g_error_new_valist (GTK_SVG_ERROR,
                                  GTK_SVG_ERROR_MISSING_ATTRIBUTE,
                                  format, args);
      va_end (args);
    }
  else
    {
      error = g_error_new (GTK_SVG_ERROR,
                           GTK_SVG_ERROR_MISSING_ATTRIBUTE,
                           "Missing attribute: %s", attr_name);
    }

  gtk_svg_error_set_element (error, g_markup_parse_context_get_element (context));
  gtk_svg_error_set_attribute (error, attr_name);
  gtk_svg_location_init (&location, context);
  gtk_svg_error_set_location (error, &location, &location);

  gtk_svg_emit_error (self, error);
  g_error_free (error);
}

G_GNUC_PRINTF (2, 3)
static void
gtk_svg_invalid_reference (GtkSvg     *self,
                           const char *format,
                           ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_SVG_ERROR,
                              GTK_SVG_ERROR_INVALID_REFERENCE,
                              format, args);
  va_end (args);

  gtk_svg_emit_error (self, error);
  g_error_free (error);
}

static void
gtk_svg_check_unhandled_attributes (GtkSvg                *self,
                                    GMarkupParseContext   *context,
                                    const char           **attr_names,
                                    uint64_t               handled)
{
  unsigned int n = g_strv_length ((char **) attr_names);

  for (unsigned int i = 0; i < n; i++)
    {
      if ((handled & BIT (i)) == 0)
        gtk_svg_invalid_attribute (self, context, attr_names[i],
                                   "Unhandled attribute: %s", attr_names[i]);
    }
}

G_GNUC_PRINTF (2, 3)
static void
gtk_svg_update_error (GtkSvg     *self,
                      const char *format,
                      ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_SVG_ERROR,
                              GTK_SVG_ERROR_FAILED_UPDATE,
                              format, args);
  va_end (args);

  gtk_svg_emit_error (self, error);
  g_error_free (error);
}

G_GNUC_PRINTF (2, 3)
static void
gtk_svg_rendering_error (GtkSvg     *self,
                         const char *format,
                         ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_SVG_ERROR,
                              GTK_SVG_ERROR_FAILED_RENDERING,
                              format, args);
  va_end (args);

  gtk_svg_emit_error (self, error);
  g_error_free (error);
}

/* }}} */
/* {{{ Helpers */

static inline void
_sincos (double x, double *_sin, double *_cos)
{
#ifdef HAVE_SINCOS
  sincos (x, _sin, _cos);
#else
  *_sin = sin (x);
  *_cos = cos (x);
#endif
}

static unsigned int
gcd (unsigned int a,
     unsigned int b)
{
  if (b == 0)
    return a;

  return gcd (b, a % b);
}

static unsigned int
lcm (unsigned int a,
     unsigned int b)
{
  return (a * b) / gcd (a, b);
}

static double
normalized_diagonal (const graphene_rect_t *viewport)
{
  return hypot (viewport->size.width, viewport->size.height) / M_SQRT2;
}

static inline double
lerp (double t, double a, double b)
{
  return a + (b - a) * t;
}

static inline gboolean
lerp_bool (double t, gboolean b0, gboolean b1)
{
  return (t * b0 + (1 - t) * b1) != 0;
}

static inline void
lerp_point (double                  t,
            const graphene_point_t *p0,
            const graphene_point_t *p1,
            graphene_point_t       *p)
{
  p->x = lerp (t, p0->x, p1->x);
  p->y = lerp (t, p0->y, p1->y);
}

static inline void
lerp_color (double          t,
            const GdkColor *c0,
            const GdkColor *c1,
            GdkColorState  *interpolation,
            GdkColor       *c)
{
  GdkColor cc0, cc1;
  float values[4];

  gdk_color_convert (&cc0, interpolation, c0);
  gdk_color_convert (&cc1, interpolation, c1);

  for (unsigned int i = 0; i < 4; i++)
    values[i] = lerp (t, cc0.values[i], cc1.values[i]);

  gdk_color_finish (&cc0);
  gdk_color_finish (&cc1);

  gdk_color_init (c, interpolation, values);
}

static inline double
accumulate (double a, double b, int n)
{
  return a + b * n;
}

static inline void
accumulate_color (const GdkColor *c0,
                  const GdkColor *c1,
                  int             n,
                  GdkColorState  *interpolation,
                  GdkColor       *c)
{
  GdkColor cc0, cc1;
  float values[4];

  gdk_color_convert (&cc0, interpolation, c0);
  gdk_color_convert (&cc1, interpolation, c1);

  for (unsigned int i = 0; i < 4; i++)
    values[0] = accumulate (cc0.values[i], cc1.values[i], n);

  gdk_color_finish (&cc0);
  gdk_color_finish (&cc1);

  gdk_color_init (c, interpolation, values);
}

static inline double
color_distance (const GdkColor *c0,
                const GdkColor *c1)
{
  g_assert (gdk_color_state_equal (c0->color_state, c1->color_state));

  return sqrt ((c0->red - c1->red)     * (c0->red - c1->red) +
               (c0->green - c1->green) * (c0->green - c1->green) +
               (c0->blue - c1->blue)   * (c0->blue - c1->blue) +
               (c0->alpha - c1->alpha) * (c0->alpha - c1->alpha));
}

static float
ease (double *params,
      double  progress)
{
  static const double epsilon = 0.00001;
  double tmin, t, tmax;
  double x1, y1, x2, y2;

  x1 = params[0];
  y1 = params[1];
  x2 = params[2];
  y2 = params[3];

  if (progress <= 0)
    return 0;
  if (progress >= 1)
    return 1;

  tmin = 0.0;
  tmax = 1.0;
  t = progress;

  while (tmin < tmax)
    {
      double sample;

      sample = (((1 + 3 * x1 - 3 * x2) * t
                +    -6 * x1 + 3 * x2) * t
                +     3 * x1         ) * t;
      if (fabs (sample - progress) < epsilon)
        break;

      if (progress > sample)
        tmin = t;
      else
        tmax = t;
      t = (tmax + tmin) * .5;
    }

  return (((1 + 3 * y1 - 3 * y2) * t
          +    -6 * y1 + 3 * y2) * t
          +     3 * y1         ) * t;
}

static gboolean
has_ancestor (GMarkupParseContext *context,
              const char          *elt)
{
  const GSList *list;

  for (list = g_markup_parse_context_get_element_stack (context);
       list != NULL;
       list = list->next)
    {
      const char *name = list->data;
      if (strcmp (name, elt) == 0)
        return TRUE;
    }

  return FALSE;
}

static gboolean
check_ancestors (GMarkupParseContext *context,
                 ...)
{
  const GSList *list;
  va_list args;
  const char *name;

  list = g_markup_parse_context_get_element_stack (context);

  va_start (args, context);
  while ((name = va_arg (args, const char *)) != NULL)
    {
      list = list->next;

      if (list == NULL)
        return FALSE;

      if (strcmp (name, (const char *) list->data) != 0)
        return FALSE;
    }
  va_end (args);

  return TRUE;
}

static void
markup_filter_attributes (const char *element_name,
                          const char **attr_names,
                          const char **attr_values,
                          uint64_t    *handled,
                          const char  *name,
                          ...)
{
  va_list ap;

  va_start (ap, name);
  while (name)
    {
      const char **ptr;

      ptr = va_arg (ap, const char **);

      if (ptr)
        *ptr = NULL;
      for (unsigned int i = 0; attr_names[i]; i++)
        {
          if (g_str_has_suffix (name, "*") &&
              strncmp (attr_names[i], name, strlen (name) - 1) == 0)
            {
              g_assert (ptr == NULL);
              *handled |= G_GUINT64_CONSTANT(1) << i;
            }
          else if (strcmp (attr_names[i], name) == 0)
            {
              if (ptr)
                *ptr = attr_values[i];
              *handled |= G_GUINT64_CONSTANT(1) << i;
              break;
            }
        }

      name = va_arg (ap, const char *);
    }

  va_end (ap);
}

static void
string_append_double (GString    *s,
                      const char *prefix,
                      double      value)
{
  char buf[64];

  g_ascii_formatd (buf, sizeof (buf), "%g", value);
  g_string_append (s, prefix);
  g_string_append (s, buf);
}

static void
string_append_point (GString                *s,
                     const char             *prefix,
                     const graphene_point_t *p)
{
  string_append_double (s, prefix, p->x);
  string_append_double (s, " ", p->y);
}

/* Break str into tokens that are separated
 * by whitespace and the given separator.
 * If sep contains just a non-space byte,
 * the separator is mandatory. If it contains
 * a space as well, the separator is optional.
 * If a mandatory separators is missing, NULL
 * is returned.
 */
static char **
strsplit_set (const char *str,
              const char *sep)
{
  const char *p, *p0;
  GPtrArray *array = g_ptr_array_new ();

  p = str;
  while (*p)
    {
      while (*p == ' ')
        p++;

      if (!*p)
        break;

      if (array->len > 0)
        {
          if (strchr (sep, *p))
            {
              p++;

              if (!*p)
                break;

              while (*p == ' ')
                p++;

              if (!*p)
                break;
            }
          else if (!strchr (sep, ' '))
            {
              g_ptr_array_free (array, TRUE);
              return NULL;
            }
        }

      p0 = p;
      while (!strchr (sep, *p) && *p != ' ')
        p++;

      g_ptr_array_add (array, g_strndup (p0, p - p0));
    }

  g_ptr_array_add (array, NULL);

  return (char **) g_ptr_array_free (array, FALSE);
}

enum
{
  NUMBER     = 1 << 0,
  PERCENTAGE = 1 << 1,
  LENGTH     = 1 << 2,
  ANGLE      = 1 << 3,
};

static const char * unit_names[] = {
  [SVG_UNIT_NUMBER] = "",
  [SVG_UNIT_PERCENTAGE] = "%",
  [SVG_UNIT_PX] = "px",
  [SVG_UNIT_PT] = "pt",
  [SVG_UNIT_IN] = "in",
  [SVG_UNIT_CM] = "cm",
  [SVG_UNIT_MM] = "mm",
  [SVG_UNIT_VW] = "vw",
  [SVG_UNIT_VH] = "vh",
  [SVG_UNIT_VMIN] = "vmin",
  [SVG_UNIT_VMAX] = "vmax",
  [SVG_UNIT_EM] = "em",
  [SVG_UNIT_EX] = "ex",
  [SVG_UNIT_DEG] = "deg",
  [SVG_UNIT_RAD] = "rad",
  [SVG_UNIT_GRAD] = "grad",
  [SVG_UNIT_TURN] = "turn",
};

static gboolean
parse_numeric (const char   *value,
               double        min,
               double        max,
               unsigned int  flags,
               double       *f,
               SvgUnit      *unit)
{
  char *endp = NULL;

  *f = g_ascii_strtod (value, &endp);

  if (endp == value)
    return FALSE;

  if (endp && *endp != '\0')
    {
      unsigned int i;

      if (*endp == '%')
        {
          *unit = SVG_UNIT_PERCENTAGE;
          return (flags & PERCENTAGE) != 0;
        }

      for (i = 0; i < G_N_ELEMENTS (unit_names); i++)
        {
          if (strcmp (endp, unit_names[i]) == 0)
            {
              if ((flags & LENGTH) != 0 &&
                  FIRST_LENGTH_UNIT <= i && i <= LAST_LENGTH_UNIT)
                {
                  *unit = i;
                  break;
                }
              if ((flags & ANGLE) != 0 &&
                  FIRST_ANGLE_UNIT <= i && i <= LAST_ANGLE_UNIT)
                {
                  *unit = i;
                  break;
                }
            }
        }

      if (i == G_N_ELEMENTS (unit_names))
        return FALSE;

      if (*f < min || *f > max)
        return FALSE;

      return TRUE;
    }
  else
    {
      if (flags & NUMBER)
        *unit = SVG_UNIT_NUMBER;
      else
        return FALSE;

      if (*f < min || *f > max)
        return FALSE;

      return TRUE;
    }
}

static GArray *
parse_numbers2 (const char *value,
                const char *sep,
                double      min,
                double      max)
{
  GArray *array;
  GStrv strv;

  strv = strsplit_set (value, sep);

  array = g_array_new (FALSE, FALSE, sizeof (double));

  for (unsigned int i = 0; strv[i]; i++)
    {
      char *s = g_strstrip (strv[i]);
      double v;
      SvgUnit unit;

      if (*s == '\0' && strv[i] == NULL)
        break;

      if (!parse_numeric (s, min, max, NUMBER, &v, &unit))
        {
          g_array_unref (array);
          g_strfreev (strv);
          return NULL;
        }

      g_array_append_val (array, v);
    }

  g_strfreev (strv);
  return array;
}

static gboolean
parse_numbers (const char   *value,
               const char   *sep,
               double        min,
               double        max,
               double       *values,
               unsigned int  length,
               unsigned int *n_values)
{
  GStrv strv;

  strv = strsplit_set (value, sep);
  *n_values = g_strv_length (strv);

  for (unsigned int i = 0; strv[i]; i++)
    {
      char *s = g_strstrip (strv[i]);
      SvgUnit unit;

      if (*s == '\0' && strv[i] == NULL)
        {
          *n_values -= 1;
          break;
        }

      if (i == length)
        {
          g_strfreev (strv);
          return FALSE;
        }

      if (!parse_numeric (s, min, max, NUMBER, &values[i], &unit))
        {
          g_strfreev (strv);
          return FALSE;
        }
    }

  g_strfreev (strv);
  return TRUE;
}

static gboolean
parse_number (const char *value,
              double      min,
              double      max,
              double     *f)
{
  SvgUnit unit;

  return parse_numeric (value, min, max, NUMBER, f, &unit);
}

static gboolean
parse_duration (const char *value,
                int64_t    *f)
{
  double v;
  char *endp = NULL;

  v = g_ascii_strtod (value, &endp);

  if (endp == value)
    return FALSE;

  if (endp && *endp != '\0')
    {
      if (strcmp (endp, "ms") == 0)
        *f = (int64_t) round (v * G_TIME_SPAN_MILLISECOND);
      else if (strcmp (endp, "s") == 0)
        *f = (int64_t) round (v * G_TIME_SPAN_SECOND);
      else
        return FALSE;
    }
  else
    *f = (int64_t) round (v * G_TIME_SPAN_SECOND);

  return TRUE;
}

static void
skip_whitespace (const char **p)
{
  while (g_ascii_isspace (**p))
    (*p)++;
}

static gboolean
rgba_parse (GdkRGBA    *rgba,
            const char *value)
{
  char *copy, *p;
  gboolean ret;

  p = copy = g_strdup (value);
  g_strstrip (p);
  ret = gdk_rgba_parse (rgba, p);
  g_free (copy);

  return ret;
}

static gboolean
match_str_len (const char *value,
               const char *str,
               size_t      len)
{
  const char *p = value;

  skip_whitespace (&p);

  if (strncmp (value, str, len) != 0)
    return FALSE;

  p += len;
  skip_whitespace (&p);

  return *p == '\0';
}

#define match_str(value, str) match_str_len (value, str, strlen (str))

static gboolean
parse_enum (const char    *value,
            const char   **values,
            size_t         n_values,
            unsigned int  *result)
{
  for (unsigned int i = 0; i < n_values; i++)
    {
      if (values[i] && match_str (value, values[i]))
        {
          *result = i;
          return TRUE;
        }
    }
  return FALSE;
}

static void
path_builder_add_ellipse (GskPathBuilder *builder,
                          double cx, double cy,
                          double rx, double ry)
{
  gsk_path_builder_move_to  (builder, cx + rx, cy);
  gsk_path_builder_conic_to (builder, cx + rx, cy + ry,
                                      cx,      cy + ry, M_SQRT1_2);
  gsk_path_builder_conic_to (builder, cx - rx, cy + ry,
                                      cx - rx, cy,      M_SQRT1_2);
  gsk_path_builder_conic_to (builder, cx - rx, cy - ry,
                                      cx,      cy - ry, M_SQRT1_2);
  gsk_path_builder_conic_to (builder, cx + rx, cy - ry,
                                      cx + rx, cy,      M_SQRT1_2);
  gsk_path_builder_close    (builder);
}

static inline gboolean
g_strv_has (GStrv       strv,
            const char *s)
{
  return g_strv_contains ((const char * const *) strv, s);
}

static void
compute_viewport_transform (gboolean               none,
                            Align                  align_x,
                            Align                  align_y,
                            MeetOrSlice            meet,
                            const graphene_rect_t *vb,
                            double e_x,          double e_y,
                            double e_width,      double e_height,
                            double *scale_x,     double *scale_y,
                            double *translate_x, double *translate_y)
{
  double sx, sy, tx, ty;

  sx = e_width / vb->size.width;
  sy = e_height / vb->size.height;

  if (!none && meet == MEET)
    sx = sy = MIN (sx, sy);
  else if (!none && meet == SLICE)
    sx = sy = MAX (sx, sy);

  tx = e_x - vb->origin.x * sx;
  ty = e_y - vb->origin.y * sy;

  if (align_x == ALIGN_MID)
    tx += (e_width - vb->size.width * sx) / 2;
  else if (align_x == ALIGN_MAX)
    tx += (e_width - vb->size.width * sx);

  if (align_y == ALIGN_MID)
    ty += (e_height - vb->size.height * sy) / 2;
  else if (align_y == ALIGN_MAX)
    ty += (e_height - vb->size.height * sy);

  *scale_x = sx;
  *scale_y = sy;
  *translate_x = tx;
  *translate_y = ty;
}

/* }}} */
/* {{{ Image loading */

static GdkTexture *
load_texture (const char  *string,
              gboolean     allow_external,
              GError     **error)
{
  GdkTexture *texture = NULL;

  if (g_str_has_prefix (string, "data:"))
    {
      GBytes *bytes;

      bytes = gtk_css_data_url_parse (string, NULL, error);

      if (bytes == NULL)
        return NULL;

      texture = gdk_texture_new_from_bytes (bytes, error);

      g_bytes_unref (bytes);
    }
  else if (g_str_has_prefix (string, "resource:"))
    {
      texture = gdk_texture_new_from_resource (string + strlen ("resource:"));
    }
  else if (allow_external)
    {
      texture = gdk_texture_new_from_filename (string, error);
    }

  return texture;
}

static GdkTexture *
get_texture (GtkSvg      *svg,
             const char  *string,
             GError     **error)
{
  GdkTexture *texture;

  texture = g_hash_table_lookup (svg->images, string);
  if (!texture)
    {
      texture = load_texture (string,
                              (svg->features & GTK_SVG_EXTERNAL_RESOURCES) != 0,
                              error);
      if (texture)
        g_hash_table_insert (svg->images, g_strdup (string), texture);
    }

  return texture;
}

/* }}} */
/* {{{ Font handling */

static void
append_base64_with_linebreaks (GString *s,
                               GBytes  *bytes)
{
  const guchar *data;
  gsize len;
  gsize max;
  gsize before;
  char *out;
  int state = 0, outlen;
  int save = 0;

  data = g_bytes_get_data (bytes, &len);

  /* We can use a smaller limit here, since we know the saved state is 0,
     +1 is needed for trailing \0, also check for unlikely integer overflow */
  g_return_if_fail (len < ((G_MAXSIZE - 1 - s->len) / 4 - 1) * 3);

  /* The glib docs say:
   *
   * The output buffer must be large enough to fit all the data that will
   * be written to it. Due to the way base64 encodes you will need
   * at least: (@len / 3 + 1) * 4 + 4 bytes (+ 4 may be needed in case of
   * non-zero state). If you enable line-breaking you will need at least:
   * ((@len / 3 + 1) * 4 + 4) / 76 + 1 bytes of extra space.
   */
  max = (len / 3 + 1) * 4;
  max += ((len / 3 + 1) * 4 + 4) / 76 + 1;
  /* and the null byte */
  max += 1;

  before = s->len;
  g_string_set_size (s, s->len + max);
  out = s->str + before;

  outlen = g_base64_encode_step (data, len, TRUE, out, &state, &save);
  outlen += g_base64_encode_close (TRUE, out + outlen, &state, &save);
  out[outlen] = '\0';
  s->len = before + outlen;
}

static void
append_bytes_href (GString    *s,
                   GBytes     *bytes,
                   const char *mime_type)
{
  g_string_append_printf (s, "data:%s;base64,\\\n", mime_type ? mime_type : "");
  append_base64_with_linebreaks (s, bytes);
}

static void
delete_file (gpointer data)
{
  char *path = data;

  g_remove (path);
  g_free (path);
}

static void
ensure_fontmap (GtkSvg *svg)
{
  if (svg->fontmap)
    return;

  svg->fontmap = pango_cairo_font_map_new ();

  if ((svg->features & GTK_SVG_SYSTEM_RESOURCES) == 0)
    {
#ifdef HAVE_PANGOFT
      /* This isolates us from the system fonts */
      FcConfig *config;

      config = FcConfigCreate ();
      pango_fc_font_map_set_config (PANGO_FC_FONT_MAP (svg->fontmap), config);
      FcConfigDestroy (config);
#endif
    }

  svg->font_files = g_ptr_array_new_with_free_func (delete_file);
}

static PangoFontMap *
get_fontmap (GtkSvg *svg)
{
  if (svg->fontmap)
    return svg->fontmap;
  else
    return pango_cairo_font_map_get_default ();
}

static gboolean
add_font_from_file (GtkSvg      *svg,
                    const char  *path,
                    GError     **error)
{
  ensure_fontmap (svg);

  if (!pango_font_map_add_font_file (svg->fontmap, path, error))
    return FALSE;

  g_ptr_array_add (svg->font_files, g_strdup (path));
  return TRUE;
}

static gboolean
add_font_from_bytes (GtkSvg      *svg,
                     GBytes      *bytes,
                     GError     **error)
{
  GFile *file;
  GIOStream *iostream;
  GOutputStream *ostream;

  file = g_file_new_tmp ("gtk4-font-XXXXXX.ttf", (GFileIOStream **) &iostream, error);
  if (!file)
    return FALSE;

  ostream = g_io_stream_get_output_stream (iostream);
  if (g_output_stream_write_bytes (ostream, bytes, NULL, error) == -1)
    {
      g_object_unref (file);
      g_object_unref (iostream);

      return FALSE;
    }

  g_io_stream_close (iostream, NULL, NULL);
  g_object_unref (iostream);

  if (!add_font_from_file (svg, g_file_peek_path (file), error))
    {
      g_file_delete (file, NULL, NULL);
      g_object_unref (file);

      return FALSE;
    }

  g_object_unref (file);
  return TRUE;
}

static gboolean
add_font_from_url (GtkSvg              *svg,
                   GMarkupParseContext *context,
                   const char          *url)
{
  char *scheme;
  GBytes *bytes;
  char *mimetype = NULL;
  GError *error = NULL;

  scheme = g_uri_parse_scheme (url);
  if (!scheme || g_ascii_strcasecmp (scheme, "data") != 0)
    {
      char *start = g_utf8_make_valid (url, 20);
      gtk_svg_invalid_attribute (svg, context, "href",
                                 "Unsupported uri scheme for font: %s…", start);
      g_free (start);
      return FALSE;
    }

  g_free (scheme);

  bytes = gtk_css_data_url_parse (url, &mimetype, &error);
  if (!bytes)
    {
      gtk_svg_invalid_attribute (svg, context, "href",
                                 "Can't parse font data: %s", error->message);
      g_error_free (error);
      g_free (mimetype);
      return FALSE;
    }

  if (strcmp (mimetype, "font/ttf") != 0)
    {
      gtk_svg_invalid_attribute (svg, context, "href",
                                 "Unsupported mime type for font data: %s", mimetype);
      g_bytes_unref (bytes);
      g_free (mimetype);
      return FALSE;
    }

  g_free (mimetype);

  if (!add_font_from_bytes (svg, bytes, &error))
    {
      g_bytes_unref (bytes);
      gtk_svg_invalid_attribute (svg, context, "href",
                                 "Failed to add font: %s", error->message);
      g_error_free (error);
      return FALSE;
    }

  g_bytes_unref (bytes);
  return TRUE;
}

/* }}} */
/* {{{ Color utilities */

static void
apply_color_matrix (GdkColorState           *color_state,
                    const graphene_matrix_t *matrix,
                    const graphene_vec4_t   *offset,
                    const GdkColor          *color,
                    GdkColor                *new_color)
{
  GdkColor c;
  graphene_vec4_t p;
  float v[4];

  gdk_color_convert (&c, color_state, color);
  graphene_vec4_init (&p, c.r, c.g, c.b, c.a);
  graphene_matrix_transform_vec4 (matrix, &p, &p);
  graphene_vec4_add (&p, offset, &p);
  graphene_vec4_to_float (&p, v);
  gdk_color_init (new_color, color_state, v);
  gdk_color_finish (&c);
}

/* }}} */
/* {{{ Caching */

static void
gtk_svg_invalidate_contents (GtkSvg *self)
{
  g_clear_pointer (&self->node, gsk_render_node_unref);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

/* }}} */
/* {{{ Pango utilities */

static GskPath *
pango_layout_to_path (PangoLayout *layout)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_path_t *path;
  GskPathBuilder *builder;
  PangoRectangle rect;

  pango_layout_get_pixel_extents (layout, &rect, NULL);
  surface = cairo_recording_surface_create (CAIRO_CONTENT_COLOR_ALPHA,
                                            &(cairo_rectangle_t) { rect.x, rect.y, rect.width, rect.height });

  cr = cairo_create (surface);
  pango_cairo_layout_path (cr, layout);
  path = cairo_copy_path (cr);
  builder = gsk_path_builder_new ();
  gsk_path_builder_add_cairo_path (builder, path);
  cairo_path_destroy (path);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  return gsk_path_builder_free_to_path (builder);
}

/* }}} */
/* {{{ Gradients */

static void
project_point_onto_line (const graphene_point_t *a,
                         const graphene_point_t *b,
                         const graphene_point_t *point,
                         graphene_point_t       *p)
{
  graphene_vec2_t n;
  graphene_vec2_t ap;
  float t;

  if (graphene_point_equal (a, b))
    {
      *p = *a;
      return;
    }

  *p = *point;

  graphene_vec2_init (&n, b->x - a->x, b->y - a->y);
  graphene_vec2_init (&ap, point->x - a->x, point->y - a->y);
  t = graphene_vec2_dot (&n, &ap) / graphene_vec2_dot (&n, &n);
  graphene_point_interpolate (a, b, t, p);
}

static void
transform_gradient_line (GskTransform *transform,
                         const graphene_point_t *start,
                         const graphene_point_t *end,
                         graphene_point_t *start_out,
                         graphene_point_t *end_out)
{
  graphene_point_t s, e, t, e2;

  if (gsk_transform_get_category (transform) == GSK_TRANSFORM_CATEGORY_IDENTITY)
    {
      *start_out = *start;
      *end_out = *end;
      return;
    }

  graphene_point_init (&t, start->x + (end->y - start->y),
                           start->y - (end->x - start->x));

  gsk_transform_transform_point (transform, start, &s);
  gsk_transform_transform_point (transform, end, &e);
  gsk_transform_transform_point (transform, &t, &t);

  /* Now s-t is the normal of the gradient we want to draw
   * To unskew the gradient, shift the start point so s-e is
   * perpendicular to s-t again
   */

  project_point_onto_line (&s, &t, &e, &e2);

  *start_out = e2;
  *end_out = e;
}

/*  }}} */
/* {{{ Rendernode utilities */

static GskRenderNode *
skip_debug_node (GskRenderNode *node)
{
  if (gsk_render_node_get_node_type (node) == GSK_DEBUG_NODE)
    return gsk_debug_node_get_child (node);
  else
    return node;
}

static GskRenderNode *
apply_transform (GskRenderNode *node,
                 GskTransform  *transform)
{
  if (transform != NULL)
    {
      node = skip_debug_node (node);

      if (gsk_render_node_get_node_type (node) == GSK_TRANSFORM_NODE)
        {
          GskTransform *tx = gsk_transform_node_get_transform (node);
          GskRenderNode *child = gsk_transform_node_get_child (node);
          GskRenderNode *transformed;

          tx = gsk_transform_transform (gsk_transform_ref (tx), transform);
          transformed = gsk_transform_node_new (child, tx);
          gsk_transform_unref (tx);

          return transformed;
        }
      else
        return gsk_transform_node_new (node, transform);
    }
  else
    return gsk_render_node_ref (node);
}

/* }}} */
/* {{{ Text */

static char *
text_chomp (const char *in,
            gboolean   *lastwasspace)
{
  size_t len = strlen (in);
  GString *ret = g_string_sized_new (len);

  for (size_t i = 0; i < len; i++)
    {
      if (in[i] == '\n')
        continue;
      if (in[i] == '\t' || in[i] == ' ')
        {
          if (*lastwasspace)
            continue;
          g_string_append_c (ret, ' ');
          *lastwasspace = TRUE;
          continue;
        }
      g_string_append_c (ret, in[i]);
      *lastwasspace = FALSE;
    }

  return g_string_free_and_steal (ret);
}


static void
text_node_clear (TextNode *self)
{
  switch (self->type)
    {
    case TEXT_NODE_SHAPE:
      // shape node not owned
      break;
    case TEXT_NODE_CHARACTERS:
      g_assert (self->characters.layout == NULL);
      g_free (self->characters.text);
      break;
    default:
      g_assert_not_reached ();
    }
}

/* }}} */
/* {{{ Path transformation */

typedef struct
{
  GskPathBuilder *builder;
  GskTransform *transform;
} TransformData;

static gboolean
transform_op (GskPathOperation        op,
              const graphene_point_t *_pts,
              gsize                   n_pts,
              float                   weight,
              gpointer                user_data)
{
  TransformData *t = user_data;
  graphene_point_t pts[4];

  for (unsigned int i = 0; i < n_pts; i++)
    gsk_transform_transform_point (t->transform, &_pts[i], &pts[i]);

  gsk_path_builder_add_op (t->builder, op, pts, n_pts, weight);

  return TRUE;
}

static GskPath *
gsk_transform_transform_path (GskTransform *transform,
                              GskPath      *path)
{
  TransformData data;

  data.builder = gsk_path_builder_new ();
  data.transform = transform;

  gsk_path_foreach (path, (GskPathForeachFlags) -1, transform_op, &data);

  return gsk_path_builder_free_to_path (data.builder);
}

/* }}} */
/* {{{ Path interpolation */

#define SVG_PATH_ARC 22
typedef struct
{
  unsigned int op;
  union {
    struct {
      graphene_point_t pts[4];
    } seg;
    struct {
      float rx;
      float ry;
      float x_axis_rotation;
      gboolean large_arc;
      gboolean positive_sweep;
      float x;
      float y;
    } arc;
  };
} SvgPathOp;

#define GDK_ARRAY_NAME svg_path_ops
#define GDK_ARRAY_TYPE_NAME SvgPathOps
#define GDK_ARRAY_ELEMENT_TYPE SvgPathOp
#define GDK_ARRAY_PREALLOC 112
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

/* We want to choose the array preallocations above so that the
 * struct fits in 1 page
 */
G_STATIC_ASSERT (sizeof (SvgPathOps) < 4096);

typedef struct
{
  SvgPathOp *ops;
  size_t n_ops;
} SvgPathData;

static SvgPathData *
svg_path_data_new (size_t     n_ops,
                   SvgPathOp *ops)
{
  SvgPathData *p = g_new0 (SvgPathData, 1);
  p->n_ops = n_ops;
  p->ops = ops;
  return p;
}

static void
svg_path_data_free (SvgPathData *p)
{
  g_free (p->ops);
  g_free (p);
}

static inline size_t
points_per_op (GskPathOperation op)
{
  size_t n_pts[] = {
    [GSK_PATH_MOVE] = 1,
    [GSK_PATH_CLOSE] = 0,
    [GSK_PATH_LINE] = 2,
    [GSK_PATH_QUAD] = 3,
    [GSK_PATH_CUBIC] = 4,
    [GSK_PATH_CONIC] = 3
  };

  return n_pts[op];
}

static gboolean
add_op (GskPathOperation        op,
        const graphene_point_t *pts,
        size_t                  n_pts,
        float                   weight,
        gpointer                user_data)
{
  SvgPathOps *ops = user_data;
  gsize size;
  SvgPathOp *pop;

  if (op == GSK_PATH_CONIC)
    return FALSE;

  size = svg_path_ops_get_size (ops);
  svg_path_ops_set_size (ops, size + 1);
  pop = svg_path_ops_index (ops, size);

  pop->op = op;
  memset (pop->seg.pts, 0, sizeof (graphene_point_t) * 4);
  memcpy (pop->seg.pts, pts, sizeof (graphene_point_t) * n_pts);

  return TRUE;
}

static gboolean
add_arc (float    rx,
         float    ry,
         float    x_axis_rotation,
         gboolean large_arc,
         gboolean positive_sweep,
         float    x,
         float    y,
         gpointer user_data)
{
  SvgPathOps *ops = user_data;
  gsize size;
  SvgPathOp *pop;

  size = svg_path_ops_get_size (ops);
  svg_path_ops_set_size (ops, size + 1);
  pop = svg_path_ops_index (ops, size);

  pop->op = SVG_PATH_ARC;
  pop->arc.rx = rx;
  pop->arc.ry = ry;
  pop->arc.x_axis_rotation = x_axis_rotation;
  pop->arc.large_arc = large_arc;
  pop->arc.positive_sweep = positive_sweep;
  pop->arc.x = x;
  pop->arc.y = y;

  return TRUE;
}

static SvgPathData *
svg_path_data_parse (const char *string)
{
  GskPathParser parser = {
    add_op, add_arc, NULL, NULL, NULL,
  };
  SvgPathOps ops;
  size_t size;
  SvgPathOp *data;

  svg_path_ops_init (&ops);

  if (!gsk_path_parse_full (string, &parser, &ops))
    {
      /* TODO: emit an error */
    }

  size = svg_path_ops_get_size (&ops);
  data = svg_path_ops_steal (&ops);

  return svg_path_data_new (size, data);
}

static SvgPathData *
svg_path_data_collect (GskPath *path)
{
  SvgPathOps ops;
  size_t size;
  SvgPathOp *data;

  svg_path_ops_init (&ops);

  gsk_path_foreach (path,
                    GSK_PATH_FOREACH_ALLOW_QUAD |
                    GSK_PATH_FOREACH_ALLOW_CUBIC,
                    add_op,
                    &ops);

  size = svg_path_ops_get_size (&ops);
  data = svg_path_ops_steal (&ops);

  return svg_path_data_new (size, data);
}

static void
svg_path_data_print (SvgPathData *p,
                     GString     *s)
{
  for (unsigned int i = 0; i < p->n_ops; i++)
    {
      SvgPathOp *op = &p->ops[i];

      if (i > 0)
        g_string_append_c (s, ' ');

      switch (op->op)
        {
        case GSK_PATH_MOVE:
          string_append_point (s, "M ", &op->seg.pts[0]);
          break;
        case GSK_PATH_CLOSE:
          g_string_append_printf (s, "Z");
          break;
        case GSK_PATH_LINE:
          string_append_point (s, "L ", &op->seg.pts[1]);
          break;
        case GSK_PATH_QUAD:
          string_append_point (s, "Q ", &op->seg.pts[1]);
          string_append_point (s, ", ", &op->seg.pts[2]);
          break;
        case GSK_PATH_CUBIC:
          string_append_point (s, "C ", &op->seg.pts[1]);
          string_append_point (s, ", ", &op->seg.pts[2]);
          string_append_point (s, ", ", &op->seg.pts[3]);
          break;
        case SVG_PATH_ARC:
          string_append_double (s, "A ", op->arc.rx);
          string_append_double (s, " ", op->arc.ry);
          string_append_double (s, " ", op->arc.x_axis_rotation);
          g_string_append_printf (s, " %d %d", op->arc.large_arc, op->arc.positive_sweep);
          string_append_double (s, " ", op->arc.x);
          string_append_double (s, " ", op->arc.y);
          break;
        default:
          g_assert_not_reached ();
        }
    }
}

static SvgPathData *
svg_path_data_interpolate (SvgPathData *p0,
                           SvgPathData *p1,
                           double       t)
{
  SvgPathOp *ops;

  if (p0->n_ops != p1->n_ops)
    return NULL;

  ops = g_new0 (SvgPathOp, p0->n_ops);

  for (unsigned int i = 0; i < p0->n_ops; i++)
    {
      SvgPathOp *op0 = &p0->ops[i];
      SvgPathOp *op1 = &p1->ops[i];
      SvgPathOp *op = &ops[i];

      if (op0->op != op1->op)
        {
          g_free (ops);
          return NULL;
        }

      op->op = op0->op;

      switch (op->op)
        {
        case GSK_PATH_CUBIC:
          lerp_point (t, &op0->seg.pts[3], &op1->seg.pts[3], &op->seg.pts[3]);
          G_GNUC_FALLTHROUGH;
        case GSK_PATH_QUAD:
          lerp_point (t, &op0->seg.pts[2], &op1->seg.pts[2], &op->seg.pts[2]);
          G_GNUC_FALLTHROUGH;
        case GSK_PATH_LINE:
        case GSK_PATH_MOVE:
          lerp_point (t, &op0->seg.pts[1], &op1->seg.pts[1], &op->seg.pts[1]);
          G_GNUC_FALLTHROUGH;
        case GSK_PATH_CLOSE:
          lerp_point (t, &op0->seg.pts[0], &op1->seg.pts[0], &op->seg.pts[0]);
          break;
        case SVG_PATH_ARC:
          op->arc.rx = lerp (t, op0->arc.rx, op1->arc.rx);
          op->arc.ry = lerp (t, op0->arc.ry, op1->arc.ry);
          op->arc.x_axis_rotation = lerp (t, op0->arc.x_axis_rotation, op1->arc.x_axis_rotation);
          op->arc.large_arc = lerp_bool (t, op0->arc.large_arc, op1->arc.large_arc);
          op->arc.positive_sweep = lerp_bool (t, op0->arc.positive_sweep, op1->arc.positive_sweep);
          op->arc.x = lerp (t, op0->arc.x, op1->arc.x);
          op->arc.y = lerp (t, op0->arc.y, op1->arc.y);

          break;
        default:
          g_assert_not_reached ();
        }
    }

  return svg_path_data_new (p0->n_ops, ops);
}

static GskPath *
svg_path_data_to_gsk (SvgPathData *p)
{
  GskPathBuilder *builder;

  builder = gsk_path_builder_new ();

  for (unsigned int i = 0; i < p->n_ops; i++)
    {
      SvgPathOp *op = &p->ops[i];

      if (op->op == SVG_PATH_ARC)
        gsk_path_builder_svg_arc_to (builder,
                                     op->arc.rx, op->arc.ry,
                                     op->arc.x_axis_rotation,
                                     op->arc.large_arc,
                                     op->arc.positive_sweep,
                                     op->arc.x, op->arc.y);
      else
        gsk_path_builder_add_op (builder, op->op, op->seg.pts, points_per_op (op->op), 1);
    }

  return gsk_path_builder_free_to_path (builder);
}

/* }}} */
/* {{{ Path decomposition */

typedef enum
{
  PATH_EMPTY,
  PATH_RECT,
  PATH_ROUNDED_RECT,
  PATH_CIRCLE,
  PATH_GENERAL,
} PathClassification;

static gboolean
path_is_rect (GskPathOperation *ops,
              size_t            n_ops,
              graphene_point_t *points,
              size_t            n_points,
              graphene_rect_t  *rect)
{
  /* Look for the path produced by an axis-aligned rectangle: mlllz */

  if (n_ops != 5)
    return FALSE;

  if (ops[0] != GSK_PATH_MOVE ||
      ops[1] != GSK_PATH_LINE ||
      ops[2] != GSK_PATH_LINE ||
      ops[3] != GSK_PATH_LINE ||
      ops[4] != GSK_PATH_CLOSE)
    return FALSE;

  if (!((points[0].y == points[1].y &&
         points[1].x == points[2].x &&
         points[2].y == points[3].y &&
         points[3].x == points[0].x) ||
        (points[0].x == points[1].x &&
         points[1].y == points[2].y &&
         points[2].x == points[3].x &&
         points[3].y == points[0].y)))
    return FALSE;

  if (points[0].x == points[1].x)
    {
      rect->origin.x = MIN (points[0].x, points[2].x);
      rect->size.width = MAX (points[0].x, points[2].x) - rect->origin.x;
    }
  else
    {
      rect->origin.x = MIN (points[0].x, points[1].x);
      rect->size.width = MAX (points[0].x, points[1].x) - rect->origin.x;
    }
  if (points[0].y == points[1].y)
    {
      rect->origin.y = MIN (points[0].y, points[2].y);
      rect->size.height = MAX (points[0].y, points[2].y) - rect->origin.y;
    }
  else
    {
      rect->origin.y = MIN (points[0].y, points[1].y);
      rect->size.height = MAX (points[0].y, points[1].y) - rect->origin.y;
    }

  return TRUE;
}

#define in_order5(a, b, c, d, e) \
  ((a <= b && b <= c && c <= d && d <= e) || \
   (a >= b && b >= c && c >= d && d >= e))
#define in_order6(a, b, c, d, e, f) \
  ((a <= b && b <= c && c <= d && d <= e && e <= f) || \
   (a >= b && b >= c && c >= d && d >= e && e >= f))

#define equal3(a, b, c) (a == b && b == c)
#define equal4(a, b, c, d) (a == b && b == c && c == d)

static const double quarter_circle_d = (M_SQRT2 - 1) * 4 / 3;

static gboolean
path_is_circle (GskPathOperation *ops,
                size_t            n_ops,
                graphene_point_t *points,
                size_t            n_points,
                GskRoundedRect   *rect)
{
  /* Look for the path produced by the common way
   * to encode a circle: mccccz.
   *
   * See https://spencermortensen.com/articles/bezier-circle/
   *
   * There are of course many other ways to encode circles
   * that we don't find. Such is life.
   *
   * Harmlessly, we also accept a trailing m
   */
  double r;

  if (n_ops != 6)
    return FALSE;

  if (n_points != 14)
    return FALSE;

  if (ops[0] != GSK_PATH_MOVE ||
      ops[1] != GSK_PATH_CUBIC ||
      ops[2] != GSK_PATH_CUBIC ||
      ops[3] != GSK_PATH_CUBIC ||
      ops[4] != GSK_PATH_CUBIC ||
      ops[5] != GSK_PATH_CLOSE)
    return FALSE;

  if (!(points[0].x == points[12].x &&
        points[0].y == points[12].y))
    return FALSE;

  if (!(equal3 (points[11].x, points[0].x, points[1].x) &&
        equal3 (points[2].y, points[3].y, points[4].y) &&
        equal3 (points[5].x, points[6].x, points[7].x) &&
        equal3 (points[8].y, points[9].y, points[10].y)))
    return FALSE;

  if (!(points[11].y == points[7].y &&
        points[0].y == points[6].y &&
        points[1].y == points[5].y &&
        points[2].x == points[10].x &&
        points[3].x == points[9].x &&
        points[4].x == points[8].x))
    return FALSE;

  if (!in_order5 (points[10].y, points[11].y, points[0].y, points[1].y, points[2].y))
    return FALSE;

  if (!in_order5 (points[1].x, points[2].x, points[3].x, points[4].x, points[5].x))
    return FALSE;

  if (points[0].y - points[3].y != points[9].y - points[0].y)
    return FALSE;

  if (points[3].x - points[6].x != points[0].x - points[3].x)
    return FALSE;

  if (fabs (points[0].y - points[3].y) != fabs (points[0].x - points[3].x))
    return FALSE;

  r = fabs (points[0].y - points[3].y);

  if (points[0].y - points[1].y != points[11].y - points[12].y)
    return FALSE;

  if (points[2].x - points[3].x != points[3].x - points[4].x)
    return FALSE;

  if (!G_APPROX_VALUE (fabs (points[0].y - points[1].y), fabs (points[2].x - points[3].x), 0.01))
    return FALSE;

  if (!G_APPROX_VALUE (fabs (points[0].y - points[1].y), quarter_circle_d * r, 0.01))
    return FALSE;

  gsk_rounded_rect_init_uniform (rect,
                                 MIN (points[6].x, points[0].x),
                                 MIN (points[9].y, points[3].y),
                                 2 * r, 2 * r,
                                 r);

  return TRUE;
}

#define swap(a, b) { tmp = a; a = b; b = tmp; }

static gboolean
path_is_circle2 (GskPathOperation *ops,
                 size_t            n_ops,
                 graphene_point_t *points,
                 size_t            n_points,
                 GskRoundedRect   *rect)
{
  graphene_point_t pts[14];
  float tmp;

  if (n_ops != 6)
    return FALSE;

  if (n_points != 14)
    return FALSE;

  for (unsigned int i = 0; i < 14; i++)
    {
      pts[i].x = points[i].y;
      pts[i].y = points[i].x;
    }

  if (!path_is_circle (ops, n_ops, pts, n_points, rect))
    return FALSE;

  swap (rect->bounds.origin.x, rect->bounds.origin.y);
  swap (rect->bounds.size.width, rect->bounds.size.height);

  return TRUE;
}

static gboolean
rounded_rect_from_points2 (graphene_point_t *points,
                           GskRoundedRect   *rect)
{
  GskCorner c;

  /* points are assumed to be for an mlclclclcz contour */

  if (points[0].x != points[16].x ||
      points[0].y != points[16].y)
    return FALSE;

  if (!(equal4 (points[15].y, points[0].y, points[1].y, points[2].y) &&
        equal4 (points[3].x, points[4].x, points[5].x, points[6].x) &&
        equal4 (points[7].y, points[8].y, points[9].y, points[10].y) &&
        equal4 (points[11].x, points[12].x, points[13].x, points[14].x)))
    return FALSE;

  /* We match both cw and ccw */
  if (!in_order6 (points[14].x, points[15].x, points[0].x, points[1].x, points[2].x, points[3].x))
    return FALSE;

  if (!in_order6 (points[2].y, points[3].y, points[4].y, points[5].y, points[6].y, points[7].y))
    return FALSE;

  graphene_rect_init (&rect->bounds,
                      MIN (points[4].x, points[13].x),
                      MIN (points[8].y, points[1].y),
                      fabs (points[13].x - points[4].x),
                      fabs (points[8].y - points[1].y));

  if (!(G_APPROX_VALUE (points[2].x - points[1].x, quarter_circle_d * (points[4].x - points[1].x), 0.01) &&
        G_APPROX_VALUE (points[4].y - points[3].y, quarter_circle_d * (points[4].y - points[1].y), 0.01)))
    return FALSE;

  if (points[1].x < points[4].x)
    {
      if (points[1].y < points[4].y)
        c = GSK_CORNER_TOP_RIGHT;
      else
        c = GSK_CORNER_BOTTOM_RIGHT;
    }
  else
    {
      if (points[1].y < points[4].y)
        c = GSK_CORNER_TOP_LEFT;
      else
        c = GSK_CORNER_BOTTOM_LEFT;
    }

  rect->corner[c].width = fabs (points[4].x - points[1].x);
  rect->corner[c].height = fabs (points[4].y - points[1].y);

  if (!(G_APPROX_VALUE (points[7].x - points[8].x, quarter_circle_d * (points[5].x - points[8].x), 0.01) &&
        G_APPROX_VALUE (points[6].y - points[5].y, quarter_circle_d * (points[8].y - points[5].y), 0.01)))
    return FALSE;

  if (points[8].x < points[5].x)
    {
      if (points[5].y < points[8].y)
        c = GSK_CORNER_BOTTOM_RIGHT;
      else
        c = GSK_CORNER_TOP_RIGHT;
    }
  else
    {
      if (points[5].y < points[8].y)
        c = GSK_CORNER_BOTTOM_LEFT;
      else
        c = GSK_CORNER_TOP_LEFT;
    }

  rect->corner[c].width = fabs (points[5].x - points[8].x);
  rect->corner[c].height = fabs (points[8].y - points[5].y);

  if (!(G_APPROX_VALUE (points[9].x - points[10].x, quarter_circle_d * (points[9].x - points[12].x), 0.01) &&
        G_APPROX_VALUE (points[11].y - points[12].y, quarter_circle_d * (points[9].y - points[12].y), 0.01)))
    return FALSE;

  if (points[12].x < points[9].x)
    {
      if (points[12].y < points[9].y)
        c = GSK_CORNER_BOTTOM_LEFT;
      else
        c = GSK_CORNER_TOP_LEFT;
    }
  else
    {
      if (points[12].y < points[9].y)
        c = GSK_CORNER_BOTTOM_RIGHT;
      else
        c = GSK_CORNER_TOP_RIGHT;
    }

  rect->corner[c].width = fabs (points[9].x - points[12].x);
  rect->corner[c].height = fabs (points[9].y - points[12].y);

  if (!(G_APPROX_VALUE (points[16].x - points[15].x, quarter_circle_d * (points[16].x - points[13].x), 0.01) &&
        G_APPROX_VALUE (points[13].y - points[14].y, quarter_circle_d * (points[13].y - points[16].y), 0.01)))
    return FALSE;

  if (points[13].x < points[16].x)
    {
      if (points[16].y < points[13].y)
        c = GSK_CORNER_TOP_LEFT;
      else
        c = GSK_CORNER_BOTTOM_LEFT;
    }
  else
    {
      if (points[16].y < points[13].y)
        c = GSK_CORNER_TOP_RIGHT;
      else
        c = GSK_CORNER_BOTTOM_RIGHT;
    }

  rect->corner[c].width = fabs (points[16].x - points[13].x);
  rect->corner[c].height = fabs (points[13].y - points[16].y);

  return TRUE;
}

static gboolean
rounded_rect_from_points (graphene_point_t *points,
                          GskRoundedRect   *rect)
{
  if (rounded_rect_from_points2 (points, rect))
    {
      return TRUE;
    }
  else
    {
      graphene_point_t pts[18];

      for (unsigned int i = 0; i < 18; i++)
        {
          pts[i].x = points[i].y;
          pts[i].y = points[i].x;
        }

      if (rounded_rect_from_points2 (pts, rect))
        {
          float tmp;

          swap (rect->bounds.origin.x, rect->bounds.origin.y);
          swap (rect->bounds.size.width, rect->bounds.size.height);
          swap (rect->corner[0].width, rect->corner[0].height);
          swap (rect->corner[1].width, rect->corner[1].height);
          swap (rect->corner[2].width, rect->corner[2].height);
          swap (rect->corner[3].width, rect->corner[3].height);

          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
path_is_rounded_rect (GskPathOperation *ops,
                      size_t            n_ops,
                      graphene_point_t *points,
                      size_t            n_points,
                      GskRoundedRect   *rect)
{
  if (n_ops != 10)
    return FALSE;

  if (n_points != 18)
    return FALSE;

  if (ops[0] != GSK_PATH_MOVE ||
      ops[1] != GSK_PATH_LINE ||
      ops[2] != GSK_PATH_CUBIC ||
      ops[3] != GSK_PATH_LINE ||
      ops[4] != GSK_PATH_CUBIC ||
      ops[5] != GSK_PATH_LINE ||
      ops[6] != GSK_PATH_CUBIC ||
      ops[7] != GSK_PATH_LINE ||
      ops[8] != GSK_PATH_CUBIC ||
      ops[9] != GSK_PATH_CLOSE)
    return FALSE;

  return rounded_rect_from_points (points, rect);
}

static gboolean
path_is_rounded_rect2 (GskPathOperation *ops,
                       size_t            n_ops,
                       graphene_point_t *points,
                       size_t            n_points,
                       GskRoundedRect   *rect)
{
  graphene_point_t pts[18];

  if (n_ops != 10)
    return FALSE;

  if (n_points != 18)
    return FALSE;

  if (ops[0] != GSK_PATH_MOVE ||
      ops[1] != GSK_PATH_CUBIC ||
      ops[2] != GSK_PATH_LINE ||
      ops[3] != GSK_PATH_CUBIC ||
      ops[4] != GSK_PATH_LINE ||
      ops[5] != GSK_PATH_CUBIC ||
      ops[6] != GSK_PATH_LINE ||
      ops[7] != GSK_PATH_CUBIC ||
      ops[8] != GSK_PATH_LINE ||
      ops[9] != GSK_PATH_CLOSE)
    return FALSE;

  /* rotate the points to go from mclclclclz to mlclclclclz */
  pts[0] = points[15];
  memcpy (pts + 1, points, sizeof (graphene_point_t) * 16);
  pts[17] = pts[0];

  return rounded_rect_from_points (pts, rect);
}

static gboolean
path_is_pill (GskPathOperation *ops,
              size_t            n_ops,
              graphene_point_t *points,
              size_t            n_points,
              GskRoundedRect   *rect)
{
  graphene_point_t pts[18];

  /* Check for the 'horizontal pill' shape that results from
   * omitting the vertical lines in a rounded rect
   */

  if (n_ops != 8)
    return FALSE;

  if (n_points != 16)
    return FALSE;

  if (ops[0] != GSK_PATH_MOVE ||
      ops[1] != GSK_PATH_LINE ||
      ops[2] != GSK_PATH_CUBIC ||
      ops[3] != GSK_PATH_CUBIC ||
      ops[4] != GSK_PATH_LINE ||
      ops[5] != GSK_PATH_CUBIC ||
      ops[6] != GSK_PATH_CUBIC ||
      ops[7] != GSK_PATH_CLOSE)
    return FALSE;

  /* duplicate points 4 and 11 to from mlcclccz to mlclclclcz */
  memcpy (pts, points, sizeof (graphene_point_t) * 5);
  memcpy (pts + 5, points + 4, sizeof (graphene_point_t) * 8);
  memcpy (pts + 13, points + 11, sizeof (graphene_point_t) * 5);

  return rounded_rect_from_points (pts, rect);
}

static gboolean
path_is_pill2 (GskPathOperation *ops,
               size_t            n_ops,
               graphene_point_t *points,
               size_t            n_points,
               GskRoundedRect   *rect)
{
  graphene_point_t pts[18];

  if (n_ops != 8)
    return FALSE;

  if (n_points != 16)
    return FALSE;

  if (ops[0] != GSK_PATH_MOVE ||
      ops[1] != GSK_PATH_CUBIC ||
      ops[2] != GSK_PATH_CUBIC ||
      ops[3] != GSK_PATH_LINE ||
      ops[4] != GSK_PATH_CUBIC ||
      ops[5] != GSK_PATH_CUBIC ||
      ops[6] != GSK_PATH_LINE ||
      ops[7] != GSK_PATH_CLOSE)
    return FALSE;

  pts[0] = points[13];
  memcpy (pts + 1, points, sizeof (graphene_point_t) * 4);
  memcpy (pts + 5, points + 4, sizeof (graphene_point_t) * 8);
  memcpy (pts + 13, points + 11, sizeof (graphene_point_t) * 4);
  pts[17] = points[13];

  return rounded_rect_from_points (pts, rect);
}

static gboolean
path_is_pill3 (GskPathOperation *ops,
               size_t            n_ops,
               graphene_point_t *points,
               size_t            n_points,
               GskRoundedRect   *rect)
{
  graphene_point_t pts[18];

  if (n_ops != 8)
    return FALSE;

  if (n_points != 16)
    return FALSE;

  if (ops[0] != GSK_PATH_MOVE ||
      ops[1] != GSK_PATH_CUBIC ||
      ops[2] != GSK_PATH_LINE ||
      ops[3] != GSK_PATH_CUBIC ||
      ops[4] != GSK_PATH_CUBIC ||
      ops[5] != GSK_PATH_LINE ||
      ops[6] != GSK_PATH_CUBIC ||
      ops[7] != GSK_PATH_CLOSE)
    return FALSE;

  memcpy (pts, points + 3, sizeof (graphene_point_t) * 5);
  memcpy (pts + 5, points + 7, sizeof (graphene_point_t) * 8);
  memcpy (pts + 13, points, sizeof (graphene_point_t) * 4);
  pts[17] = points[3];

  return rounded_rect_from_points (pts, rect);
}

static PathClassification
classify_path (GskPath        *path,
               GskRoundedRect *rect)
{
  const GskContour *contour;
  graphene_point_t center;
  float radius;
  gboolean ccw;
  GskPathOperation ops[10];
  graphene_point_t points[18];
  size_t n_ops;
  size_t n_points;

  if (gsk_path_is_empty (path))
    return PATH_EMPTY;

  if (gsk_path_get_n_contours (path) > 2 ||
      (gsk_path_get_n_contours (path) == 2 &&
       gsk_contour_get_standard_ops (gsk_path_get_contour (path, 1), 0, NULL) > 1))
    return PATH_GENERAL;

  contour = gsk_path_get_contour (path, 0);

  if (gsk_contour_get_rect (contour, &rect->bounds))
    return PATH_RECT;
  else if (gsk_contour_get_rounded_rect (contour, rect))
    return PATH_ROUNDED_RECT;
  else if (gsk_contour_get_circle (contour, &center, &radius, &ccw))
    {
      graphene_rect_init (&rect->bounds,
                          center.x - radius,
                          center.y - radius,
                          2 * radius,
                          2 * radius);
      rect->corner[0].width = rect->corner[0].height = radius;
      rect->corner[1].width = rect->corner[1].height = radius;
      rect->corner[2].width = rect->corner[2].height = radius;
      rect->corner[3].width = rect->corner[3].height = radius;
      return PATH_CIRCLE;
    }

  n_ops = gsk_contour_get_standard_ops (contour, G_N_ELEMENTS (ops), ops);
  n_points = gsk_contour_get_standard_points (contour, G_N_ELEMENTS (points), points);

  if (path_is_rect (ops, n_ops, points, n_points, &rect->bounds))
    return PATH_RECT;
  else if (path_is_circle (ops, n_ops, points, n_points, rect) ||
           path_is_circle2 (ops, n_ops, points, n_points, rect))
    return PATH_CIRCLE;
  else if (path_is_rounded_rect (ops, n_ops, points, n_points, rect) ||
           path_is_rounded_rect2 (ops, n_ops, points, n_points, rect) ||
           path_is_pill (ops, n_ops, points, n_points, rect) ||
           path_is_pill2 (ops, n_ops, points, n_points, rect) ||
           path_is_pill3 (ops, n_ops, points, n_points, rect))
    return PATH_ROUNDED_RECT;

  return PATH_GENERAL;
}

static void
snapshot_push_fill (GtkSnapshot *snapshot,
                    GskPath     *path,
                    GskFillRule  rule)
{
  GskRoundedRect rect = { 0, };

  switch (classify_path (path, &rect))
    {
    case PATH_RECT:
      gtk_snapshot_push_clip (snapshot, &rect.bounds);
      break;
    case PATH_ROUNDED_RECT:
    case PATH_CIRCLE:
      gtk_snapshot_push_rounded_clip (snapshot, &rect);
      break;
    case PATH_GENERAL:
      gtk_snapshot_push_fill (snapshot, path, rule);
      break;
    case PATH_EMPTY:
      gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_INIT (0, 0, 0, 0));
      break;
    default:
      g_assert_not_reached ();
    }
}

/* }}} */
/* {{{ gpa things */

#define NO_STATES 0
#define ALL_STATES G_MAXUINT64

static gboolean
parse_states (const char *text,
              uint64_t   *states)
{
  GStrv str = NULL;

  if (strcmp (text, "all") == 0)
    {
      *states = ALL_STATES;
      return TRUE;
    }

  if (strcmp (text, "none") == 0)
    {
      *states = NO_STATES;
      return TRUE;
    }

  *states = 0;

  str = g_strsplit (text, " ", 0);
  for (unsigned int i = 0; str[i]; i++)
    {
      unsigned int u;
      char *end;

      u = (unsigned int) g_ascii_strtoull (str[i], &end, 10);
      if ((end && *end != '\0') || (u > 63))
        {
          *states = ALL_STATES;
          g_strfreev (str);
          return FALSE;
        }

      *states |= BIT (u);
    }

  g_strfreev (str);
  return TRUE;
}

static void
print_states (GString  *s,
              uint64_t  states)
{
  if (states == ALL_STATES)
    {
      g_string_append (s, "all");
    }
  else if (states == NO_STATES)
    {
      g_string_append (s, "none");
    }
  else
    {
      gboolean first = TRUE;
      for (unsigned int u = 0; u < 64; u++)
        {
          if ((states & BIT (u)) != 0)
            {
              if (!first)
                g_string_append_c (s, ' ');
              g_string_append_printf (s, "%u", u);
              first = FALSE;
            }
        }
    }
}

static gboolean
state_match (uint64_t     states,
             unsigned int state)
{
  if (state == GTK_SVG_STATE_EMPTY)
    return FALSE;

  if ((states & BIT (state)) != 0)
    return TRUE;

  return FALSE;
}

/* }}} */
/* {{{ Values */

typedef struct {
  GtkSvg *svg;
  const graphene_rect_t *viewport;
  Shape *parent; /* Can be different from the actual parent, for <use> */
  int64_t current_time;
  const GdkRGBA *colors;
  size_t n_colors;
  GdkColorState *interpolation;
} ComputeContext;

typedef struct _SvgValueClass SvgValueClass;

struct _SvgValueClass
{
  const char *name;

  void       (* free)        (SvgValue              *value);
  gboolean   (* equal)       (const SvgValue        *value0,
                              const SvgValue        *value1);
  SvgValue * (* interpolate) (const SvgValue        *value0,
                              const SvgValue        *value1,
                              ComputeContext        *context,
                              double                 t);
  SvgValue * (* accumulate)  (const SvgValue        *value0,
                              const SvgValue        *value1,
                              ComputeContext        *context,
                              int                    n);
  void       (* print)       (const SvgValue        *value,
                              GString               *string);
  double     (* distance)    (const SvgValue        *value0,
                              const SvgValue        *value1);
  SvgValue * (* resolve)     (const SvgValue        *value,
                              ShapeAttr              attr,
                              unsigned int           idx,
                              Shape                 *shape,
                              ComputeContext        *context);
};

struct _SvgValue
{
  const SvgValueClass *class;
  int ref_count;
};

static SvgValue *
svg_value_alloc (const SvgValueClass *class,
                 gsize                size)
{
  SvgValue *value;

  value = g_malloc0 (size);

  value->class = class;
  value->ref_count = 1;

  return value;
}

SvgValue *
svg_value_ref (SvgValue *value)
{
  if (value->ref_count == 0)
    return value;

  value->ref_count += 1;
  return value;
}

void
svg_value_unref (SvgValue *value)
{
  if (value->ref_count == 0)
    return;

  if (value->ref_count > 1)
    {
      value->ref_count -= 1;
      return;
    }

  value->class->free (value);
}

G_DEFINE_BOXED_TYPE (SvgValue, svg_value, svg_value_ref, svg_value_unref)

gboolean
svg_value_equal (const SvgValue *value0,
                 const SvgValue *value1)
{
  if (value0 == value1)
    return TRUE;

  if (value0->class != value1->class)
    return FALSE;

  return value0->class->equal (value0, value1);
}

/* Compute v = t * a + (1 - t) * b */
static SvgValue *
svg_value_interpolate (const SvgValue *value0,
                       const SvgValue *value1,
                       ComputeContext *context,
                       double          t)
{
  g_return_val_if_fail (value0->class == value1->class, svg_value_ref ((SvgValue *) value0));

  if (t == 0)
    return svg_value_ref ((SvgValue *) value0);

  if (t == 1)
    return svg_value_ref ((SvgValue *) value1);

  if (value0 == value1)
    return svg_value_ref ((SvgValue *) value0);

  return value1->class->interpolate (value0, value1, context, t);
}

/* Compute v = n * b + a */
static SvgValue *
svg_value_accumulate (const SvgValue *value0,
                      const SvgValue *value1,
                       ComputeContext *context,
                      int             n)
{
  g_return_val_if_fail (value0->class == value1->class, svg_value_ref ((SvgValue *) value0));

  if (n == 0)
    return svg_value_ref ((SvgValue *) value0);

  return value0->class->accumulate (value0, value1, context, n);
}

static void
svg_value_print (const SvgValue *value,
                 GString        *string)
{
  value->class->print (value, string);
}

char *
svg_value_to_string (const SvgValue *value)
{
  GString *s = g_string_new ("");
  svg_value_print (value, s);
  return g_string_free (s, FALSE);
}

static void
svg_value_default_free (SvgValue *value)
{
  g_free (value);
}

static double
svg_value_default_distance (const SvgValue *value0,
                            const SvgValue *value1)
{
  g_warning ("Can't determine distance between %s values",
             value0->class->name);
  return 1;
}

static double
svg_value_distance (const SvgValue *value0,
                    const SvgValue *value1)
{
  g_return_val_if_fail (value0->class == value1->class, 1);

  return value0->class->distance (value0, value1);
}

static SvgValue *
svg_value_default_resolve (const SvgValue  *value,
                           ShapeAttr        attr,
                           unsigned int     idx,
                           Shape           *shape,
                           ComputeContext  *context)
{
  return svg_value_ref ((SvgValue *) value);
}

static SvgValue *
svg_value_resolve (const SvgValue  *value,
                   ShapeAttr        attr,
                   unsigned int     idx,
                   Shape           *shape,
                   ComputeContext  *context)
{
  return value->class->resolve (value, attr, idx, shape, context);
}

/* }}} */
/* {{{ Keyword values */

enum
{
  SVG_INHERIT,
  SVG_INITIAL,
  SVG_CURRENT,
  SVG_AUTO,
};

typedef struct
{
  SvgValue base;
  unsigned int keyword;
} SvgKeyword;

static gboolean
svg_keyword_equal (const SvgValue *value0,
                   const SvgValue *value1)
{
  const SvgKeyword *k0 = (const SvgKeyword *) value0;
  const SvgKeyword *k1 = (const SvgKeyword *) value1;
  return k0->keyword == k1->keyword;
}

static SvgValue *
svg_keyword_interpolate (const SvgValue *value0,
                         const SvgValue *value1,
                         ComputeContext *context,
                         double          t)
{
  return NULL;
}

static SvgValue *
svg_keyword_accumulate (const SvgValue *value0,
                        const SvgValue *value1,
                        ComputeContext *context,
                        int             n)
{
  return NULL;
}

static void
svg_keyword_print (const SvgValue *value,
                   GString        *string)
{
  const SvgKeyword *k = (const SvgKeyword *) value;

  switch (k->keyword)
    {
    case SVG_INHERIT:
      g_string_append (string, "inherit");
      break;
    case SVG_INITIAL:
      g_string_append (string, "initial");
      break;
    case SVG_CURRENT:
      g_string_append (string, "current");
      break;
    case SVG_AUTO:
      g_string_append (string, "auto");
      break;
    default:
      g_assert_not_reached ();
    }
}

static const SvgValueClass SVG_KEYWORD_CLASS = {
  "SvgKeyword",
  svg_value_default_free,
  svg_keyword_equal,
  svg_keyword_interpolate,
  svg_keyword_accumulate,
  svg_keyword_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

static SvgValue *
svg_inherit_new (void)
{
  static SvgKeyword inherit = { { &SVG_KEYWORD_CLASS, 0 }, SVG_INHERIT };

  return (SvgValue *) &inherit;
}

static SvgValue *
svg_initial_new (void)
{
  static SvgKeyword initial = { { &SVG_KEYWORD_CLASS, 0 }, SVG_INITIAL };

  return (SvgValue *) &initial;
}

static SvgValue *
svg_current_new (void)
{
  static SvgKeyword current = { { &SVG_KEYWORD_CLASS, 0 }, SVG_CURRENT };

  return (SvgValue *) &current;
}

static SvgValue *
svg_auto_new (void)
{
  static SvgKeyword auto_value = { { &SVG_KEYWORD_CLASS, 0 }, SVG_AUTO };

  return (SvgValue *) &auto_value;
}

static gboolean
svg_value_is_inherit (const SvgValue *value)
{
  return value->class == &SVG_KEYWORD_CLASS &&
         ((const SvgKeyword *) value)->keyword == SVG_INHERIT;
}

static gboolean
svg_value_is_initial (const SvgValue *value)
{
  return value->class == &SVG_KEYWORD_CLASS &&
         ((const SvgKeyword *) value)->keyword == SVG_INITIAL;
}

static gboolean
svg_value_is_current (const SvgValue *value)
{
  return value->class == &SVG_KEYWORD_CLASS &&
         ((const SvgKeyword *) value)->keyword == SVG_CURRENT;
}

static gboolean
svg_value_is_auto (const SvgValue *value)
{
  return value->class == &SVG_KEYWORD_CLASS &&
         ((const SvgKeyword *) value)->keyword == SVG_AUTO;
}

/* }}} */
/* {{{ Numbers */

typedef struct
{
  SvgValue base;
  SvgUnit unit;
  double value;
} SvgNumber;

static gboolean
svg_number_equal (const SvgValue *value0,
                  const SvgValue *value1)
{
  const SvgNumber *n0 = (const SvgNumber *) value0;
  const SvgNumber *n1 = (const SvgNumber *) value1;

  return n0->unit == n1->unit && n0->value == n1->value;
}

static SvgValue * svg_number_new_full (SvgUnit unit,
                                       double  value);

static SvgValue *
svg_number_interpolate (const SvgValue *value0,
                        const SvgValue *value1,
                        ComputeContext *context,
                        double          t)
{
  const SvgNumber *n0 = (const SvgNumber *) value0;
  const SvgNumber *n1 = (const SvgNumber *) value1;

  if (n0->unit != n1->unit)
    return NULL;

  return svg_number_new_full (n0->unit, lerp (t, n0->value, n1->value));
}

static SvgValue *
svg_number_accumulate (const SvgValue *value0,
                       const SvgValue *value1,
                       ComputeContext *context,
                       int             n)
{
  const SvgNumber *n0 = (const SvgNumber *) value0;
  const SvgNumber *n1 = (const SvgNumber *) value1;

  if (n0->unit != n1->unit)
    return NULL;

  return svg_number_new_full (n0->unit, accumulate (n0->value, n1->value, n));
}

static void
svg_number_print (const SvgValue *value,
                  GString        *string)
{
  const SvgNumber *n = (const SvgNumber *) value;
  string_append_double (string, "", n->value);
  g_string_append (string, unit_names[n->unit]);
}

static double
svg_number_distance (const SvgValue *value0,
                     const SvgValue *value1)
{
  const SvgNumber *n0 = (const SvgNumber *) value0;
  const SvgNumber *n1 = (const SvgNumber *) value1;

  if (n0->unit != n1->unit)
    return 1;

  return fabs (n0->value - n1->value);
}

static gboolean
is_absolute_length (SvgUnit unit)
{
  switch ((unsigned int) unit)
    {
    case SVG_UNIT_PX:
    case SVG_UNIT_PT:
    case SVG_UNIT_IN:
    case SVG_UNIT_CM:
    case SVG_UNIT_MM:
      return TRUE;
    default:
      return FALSE;
    }
}

static double
absolute_length_to_px (double  value,
                       SvgUnit unit)
{
  switch ((unsigned int) unit)
    {
    case SVG_UNIT_PX: return value;
    case SVG_UNIT_PT: return value * 96 / 72;
    case SVG_UNIT_IN: return value * 96;
    case SVG_UNIT_CM: return value * 96 / 2.54;
    case SVG_UNIT_MM: return value * 96 / 25.4;
    default: g_assert_not_reached ();
    }
}

#if 0
static gboolean
is_angle (SvgUnit unit)
{
  switch ((unsigned int) unit)
    {
    case SVG_UNIT_DEG:
    case SVG_UNIT_RAD:
    case SVG_UNIT_GRAD:
    case SVG_UNIT_TURN:
      return TRUE;
    default:
      return FALSE;
    }
}
#endif

static double
angle_to_deg (double  value,
              SvgUnit unit)
{
  switch ((unsigned int) unit)
    {
    case SVG_UNIT_DEG: return value;
    case SVG_UNIT_RAD: return value * 180.0 / M_PI;
    case SVG_UNIT_GRAD: return value * 360.0 / 400.0;
    case SVG_UNIT_TURN: return value * 360.0;
    default: g_assert_not_reached ();
    }
}

#if 0
static gboolean
is_viewport_relative (SvgUnit unit)
{
  switch ((unsigned int) unit)
    {
    case SVG_UNIT_VW:
    case SVG_UNIT_VH:
    case SVG_UNIT_VMIN:
    case SVG_UNIT_VMAX:
      return TRUE;
    default:
      return FALSE;
    }
}
#endif

static double
viewport_relative_to_px (double                 value,
                         SvgUnit                unit,
                         const graphene_rect_t *viewport)
{
  switch ((unsigned int) unit)
    {
    case SVG_UNIT_VW: return value * viewport->size.width / 100;
    case SVG_UNIT_VH: return value * viewport->size.height / 100;
    case SVG_UNIT_VMIN: return value * MIN (viewport->size.width,
                                            viewport->size.height) / 100;
    case SVG_UNIT_VMAX: return value * MAX (viewport->size.width,
                                            viewport->size.height) / 100;
    default: g_assert_not_reached ();
    }
}

static double
shape_get_current_font_size (Shape          *shape,
                             ShapeAttr       attr,
                             ComputeContext *context)
{
  /* FIXME: units */
  if (attr != SHAPE_ATTR_FONT_SIZE)
    return ((SvgNumber *) shape->current[SHAPE_ATTR_FONT_SIZE])->value;
  else if (context->parent)
    return ((SvgNumber *) context->parent->current[SHAPE_ATTR_FONT_SIZE])->value;
  else
    return DEFAULT_FONT_SIZE;
}

static SvgValue *
svg_number_resolve (const SvgValue *value,
                    ShapeAttr       attr,
                    unsigned int    idx,
                    Shape          *shape,
                    ComputeContext *context)
{
  const SvgNumber *n = (const SvgNumber *) value;

  switch (n->unit)
    {
    case SVG_UNIT_NUMBER:
    case SVG_UNIT_PX:
      switch ((unsigned int) attr)
        {
        case SHAPE_ATTR_OPACITY:
        case SHAPE_ATTR_FILL_OPACITY:
        case SHAPE_ATTR_STROKE_OPACITY:
        case SHAPE_ATTR_STOP_OFFSET:
          return svg_number_new (CLAMP (n->value, 0, 1));
        default:
          return svg_value_ref ((SvgValue *) value);
        }
      break;
    case SVG_UNIT_PERCENTAGE:
      switch ((unsigned int) attr)
        {
        case SHAPE_ATTR_FONT_SIZE:
          {
            double parent_size;

            if (context->parent)
              parent_size = ((SvgNumber *) context->parent->current[SHAPE_ATTR_FONT_SIZE])->value;
            else
              parent_size = DEFAULT_FONT_SIZE;

            return svg_number_new (n->value * parent_size / 100);
          }
          break;
        case SHAPE_ATTR_OPACITY:
        case SHAPE_ATTR_FILL_OPACITY:
        case SHAPE_ATTR_STROKE_OPACITY:
        case SHAPE_ATTR_STOP_OFFSET:
          return svg_number_new (CLAMP (n->value / 100, 0, 1));
        case SHAPE_ATTR_FILTER:
          return svg_number_new (n->value / 100);
        case SHAPE_ATTR_STROKE_WIDTH:
        case SHAPE_ATTR_R:
          if (shape->type != SHAPE_RADIAL_GRADIENT)
            return svg_number_new_full (SVG_UNIT_PX, n->value * normalized_diagonal (context->viewport) / 100);
          else
            return svg_value_ref ((SvgValue *) value);
        case SHAPE_ATTR_CX:
        case SHAPE_ATTR_RX:
          if (shape->type != SHAPE_RADIAL_GRADIENT)
            return svg_number_new_full (SVG_UNIT_PX, n->value * context->viewport->size.width / 100);
          else
            return svg_value_ref ((SvgValue *) value);
        case SHAPE_ATTR_CY:
        case SHAPE_ATTR_RY:
          if (shape->type != SHAPE_RADIAL_GRADIENT)
            return svg_number_new_full (SVG_UNIT_PX, n->value * context->viewport->size.height / 100);
          else
            return svg_value_ref ((SvgValue *) value);
        case SHAPE_ATTR_Y:
        case SHAPE_ATTR_HEIGHT:
          if (shape->type != SHAPE_FILTER &&
              shape->type != SHAPE_PATTERN &&
              (shape->type != SHAPE_MASK ||
               svg_enum_get (shape->current[SHAPE_ATTR_BOUND_UNITS]) != COORD_UNITS_OBJECT_BOUNDING_BOX))
            return svg_number_new_full (SVG_UNIT_PX, n->value * context->viewport->size.height / 100);
          else
            return svg_value_ref ((SvgValue *) value);
        case SHAPE_ATTR_X:
        case SHAPE_ATTR_WIDTH:
          if (shape->type != SHAPE_FILTER &&
              shape->type != SHAPE_PATTERN &&
              (shape->type != SHAPE_MASK ||
               svg_enum_get (shape->current[SHAPE_ATTR_BOUND_UNITS]) != COORD_UNITS_OBJECT_BOUNDING_BOX))
            return svg_number_new_full (SVG_UNIT_PX, n->value * context->viewport->size.width / 100);
          else
            return svg_value_ref ((SvgValue *) value);
        case SHAPE_ATTR_X1:
        case SHAPE_ATTR_X2:
          if (shape->type == SHAPE_LINE)
            return svg_number_new_full (SVG_UNIT_PX, n->value * context->viewport->size.width / 100);
          else
            return svg_value_ref ((SvgValue *) value);
        case SHAPE_ATTR_Y1:
        case SHAPE_ATTR_Y2:
          if (shape->type == SHAPE_LINE)
            return svg_number_new_full (SVG_UNIT_PX, n->value * context->viewport->size.height / 100);
          else
            return svg_value_ref ((SvgValue *) value);
        default:
          return svg_value_ref ((SvgValue *) value);
        }
      break;
    case SVG_UNIT_PT:
    case SVG_UNIT_IN:
    case SVG_UNIT_CM:
    case SVG_UNIT_MM:
      return svg_number_new_full (SVG_UNIT_PX, absolute_length_to_px (n->value, n->unit));

    case SVG_UNIT_VW:
    case SVG_UNIT_VH:
    case SVG_UNIT_VMIN:
    case SVG_UNIT_VMAX:
      return svg_number_new_full (SVG_UNIT_PX, viewport_relative_to_px (n->value,
                                                                        n->unit,
                                                                        context->viewport));

    case SVG_UNIT_EM:
      return svg_number_new_full (SVG_UNIT_PX, n->value * shape_get_current_font_size (shape, attr, context));

    case SVG_UNIT_EX:
      return svg_number_new_full (SVG_UNIT_PX, n->value * 0.5 * shape_get_current_font_size (shape, attr, context));

    case SVG_UNIT_RAD:
    case SVG_UNIT_DEG:
    case SVG_UNIT_GRAD:
    case SVG_UNIT_TURN:
      return svg_number_new_full (SVG_UNIT_DEG, angle_to_deg (n->value, n->unit));

    default:
      g_assert_not_reached ();
    }
}

static const SvgValueClass SVG_NUMBER_CLASS = {
  "SvgNumber",
  svg_value_default_free,
  svg_number_equal,
  svg_number_interpolate,
  svg_number_accumulate,
  svg_number_print,
  svg_number_distance,
  svg_number_resolve,
};

SvgValue *
svg_number_new (double value)
{
  static SvgNumber singletons[] = {
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_NUMBER, .value = 0 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_NUMBER, .value = 1 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_NUMBER, .value = 2 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_NUMBER, .value = 4 }, /* default miter limit */
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_NUMBER, .value = DEFAULT_FONT_SIZE },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_NUMBER, .value = PANGO_WEIGHT_NORMAL },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_NUMBER, .value = -1 }, /* unset path length */
  };
  SvgValue *result;

  for (unsigned int i = 0; i < G_N_ELEMENTS (singletons); i++)
    {
      if (value == singletons[i].value)
        return (SvgValue *) &singletons[i];
    }

  result = svg_value_alloc (&SVG_NUMBER_CLASS, sizeof (SvgNumber));
  ((SvgNumber *) result)->unit = SVG_UNIT_NUMBER;
  ((SvgNumber *) result)->value = value;

  return result;
}

static SvgValue *
svg_percentage_new (double value)
{
  static SvgNumber singletons[] = {
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_PERCENTAGE, .value = -10 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_PERCENTAGE, .value = 0 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_PERCENTAGE, .value = 25 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_PERCENTAGE, .value = 50 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_PERCENTAGE, .value = 100 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_PERCENTAGE, .value = 120 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_PERCENTAGE, .value = 150 },
  };
  SvgValue *result;

  for (unsigned int i = 0; i < G_N_ELEMENTS (singletons); i++)
    {
      if (value == singletons[i].value)
        return (SvgValue *) &singletons[i];
    }

  result = svg_value_alloc (&SVG_NUMBER_CLASS, sizeof (SvgNumber));
  ((SvgNumber *) result)->unit = SVG_UNIT_PERCENTAGE;
  ((SvgNumber *) result)->value = value;

  return result;
}

static SvgValue *
svg_number_new_full (SvgUnit unit,
                     double  value)
{
  if (unit == SVG_UNIT_NUMBER)
    {
      return svg_number_new (value);
    }
  else if (unit == SVG_UNIT_PERCENTAGE)
    {
      return svg_percentage_new (value);
    }
  else
    {
      SvgNumber *result;

      result = (SvgNumber *) svg_value_alloc (&SVG_NUMBER_CLASS, sizeof (SvgNumber));
      result->value = value;
      result->unit = unit;

      return (SvgValue *) result;
    }
}

static SvgValue *
svg_number_parse (const char   *value,
                  double        min,
                  double        max,
                  unsigned int  flags)
{
  double f;
  SvgUnit unit;

  if (!parse_numeric (value, min, max, flags, &f, &unit))
    return NULL;

  return svg_number_new_full (unit, f);
}

static double
svg_number_get (const SvgValue *value,
                double          one_hundred_percent)
{
  const SvgNumber *n = (const SvgNumber *)value;

  g_assert (value->class == &SVG_NUMBER_CLASS);

  if (n->unit == SVG_UNIT_PERCENTAGE)
    return n->value / 100 * one_hundred_percent;
  else
    return n->value;
}

/* }}} */
/* {{{ Number sequences */

typedef struct
{
  SvgUnit unit;
  double value;
} Number;

static inline gboolean
number_equal (const Number *n0,
              const Number *n1)
{
  return n0->unit == n1->unit && n0->value == n1->value;
}

typedef struct
{
  SvgValue base;
  unsigned int n_values;
  Number values[1];
} SvgNumbers;

static unsigned int
svg_numbers_size (unsigned int n)
{
  return sizeof (SvgNumbers) + (n > 0 ? n - 1 : 0) * sizeof (Number);
}

static gboolean
svg_numbers_equal (const SvgValue *value0,
                   const SvgValue *value1)
{
  const SvgNumbers *p0 = (const SvgNumbers *) value0;
  const SvgNumbers *p1 = (const SvgNumbers *) value1;

  if (p0->n_values != p1->n_values)
    return FALSE;

  for (unsigned int i = 0; i < p0->n_values; i++)
    {
      if (!number_equal (&p0->values[i], &p1->values[i]))
        return FALSE;
    }

  return TRUE;
}

static SvgValue *
svg_numbers_accumulate (const SvgValue *value0,
                        const SvgValue *value1,
                        ComputeContext *context,
                        int             n)
{
  return NULL;
}

static void
svg_numbers_print (const SvgValue *value,
                   GString        *string)
{
  const SvgNumbers *p = (const SvgNumbers *) value;

  for (unsigned int i = 0; i < p->n_values; i++)
    {
      if (i > 0)
        g_string_append_c (string, ' ');
      string_append_double (string, "", p->values[i].value);
      g_string_append (string, unit_names[p->values[i].unit]);
    }
}

static SvgValue * svg_numbers_interpolate (const SvgValue *value0,
                                           const SvgValue *value1,
                                           ComputeContext *context,
                                           double          t);

static SvgValue * svg_numbers_resolve (const SvgValue *value,
                                       ShapeAttr       attr,
                                       unsigned int    idx,
                                       Shape          *shape,
                                       ComputeContext *context);

static const SvgValueClass SVG_NUMBERS_CLASS = {
  "SvgNumbers",
  svg_value_default_free,
  svg_numbers_equal,
  svg_numbers_interpolate,
  svg_numbers_accumulate,
  svg_numbers_print,
  svg_value_default_distance,
  svg_numbers_resolve,
};

SvgValue *
svg_numbers_new (double       *values,
                 unsigned int  n_values)
{
  SvgNumbers *result;

  result = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (n_values));
  result->n_values = n_values;

  for (unsigned int i = 0; i < n_values; i++)
    {
      result->values[i].unit = SVG_UNIT_NUMBER;
      result->values[i].value = values[i];
    }

  return (SvgValue *) result;
}

static SvgValue *
svg_numbers_new_identity_matrix (void)
{
  static SvgValue *id;

  if (id == NULL)
    {
      id = svg_numbers_new ((double []) { 1, 0, 0, 0, 0,
                                          0, 1, 0, 0, 0,
                                          0, 0, 1, 0, 0,
                                          0, 0, 0, 1, 0 }, 20);
      id->ref_count = 0;
    }

  return id;
}

static SvgValue *
svg_numbers_new_none (void)
{
  static SvgNumbers none = { { &SVG_NUMBERS_CLASS, 0 }, .n_values = 0, .values[0] = { SVG_UNIT_NUMBER, 0 } };

  return (SvgValue *) &none;
}

static SvgValue *
svg_numbers_new1 (double value)
{
  static SvgNumbers singletons[] = {
    { { &SVG_NUMBERS_CLASS, 0 }, .n_values = 1, .values[0] = { SVG_UNIT_NUMBER, 0 } },
    { { &SVG_NUMBERS_CLASS, 0 }, .n_values = 1, .values[0] = { SVG_UNIT_NUMBER, 1 } },
    { { &SVG_NUMBERS_CLASS, 0 }, .n_values = 1, .values[0] = { SVG_UNIT_NUMBER, 2 } },
  };
  SvgNumbers *p;

  for (unsigned int i = 0; i < G_N_ELEMENTS (singletons); i++)
    {
      if (singletons[i].values[0].value == value)
        return (SvgValue *) &singletons[i];
    }

  p = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (1));

  p->n_values = 1;
  p->values[0].unit = SVG_UNIT_NUMBER;
  p->values[0].value = value;

  return (SvgValue *) p;
}

static SvgValue *
svg_numbers_new_00 (void)
{
  static SvgNumbers *value = NULL;

  if (value == NULL)
    {
      value = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (2));
      ((SvgValue *) value)->ref_count = 0;
      value->values[0].value = value->values[1].value = 0;
      value->values[0].unit = value->values[1].unit = SVG_UNIT_NUMBER;
    }

  return (SvgValue *) value;
}

static SvgValue *
svg_numbers_parse (const char *value)
{
  GStrv strv;
  unsigned int n;
  SvgNumbers *p;

  strv = strsplit_set (value, ", ");
  n = g_strv_length (strv);

  p = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (n));
  p->n_values = n;

  for (unsigned int i = 0; i < n; i++)
    {
      if (!parse_numeric (strv[i], -DBL_MAX, DBL_MAX, NUMBER,
                          &p->values[i].value, &p->values[i].unit))

        {
          svg_value_unref ((SvgValue *) p);
          p = NULL;
          break;
        }
    }
  g_strfreev (strv);

  return (SvgValue *) p;
}

static SvgValue *
svg_numbers_interpolate (const SvgValue *value0,
                         const SvgValue *value1,
                         ComputeContext *context,
                         double          t)
{
  const SvgNumbers *p0 = (const SvgNumbers *) value0;
  const SvgNumbers *p1 = (const SvgNumbers *) value1;
  SvgNumbers *p;

  if (p0->n_values != p1->n_values)
    {
      if (t < 0.5)
        return svg_value_ref ((SvgValue *) value0);
      else
        return svg_value_ref ((SvgValue *) value1);
    }

  p = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (p0->n_values));
  p->n_values = p0->n_values;

  for (unsigned int i = 0; i < p0->n_values; i++)
    {
      g_assert (p0->values[i].unit != SVG_UNIT_PERCENTAGE);
      p->values[i].unit = SVG_UNIT_NUMBER;
      p->values[i].value = lerp (t, p0->values[i].value, p1->values[i].value);
    }

  return (SvgValue *) p;
}

static SvgValue *
svg_numbers_resolve (const SvgValue *value,
                     ShapeAttr       attr,
                     unsigned int    idx,
                     Shape          *shape,
                     ComputeContext *context)
{
  SvgNumbers *orig = (SvgNumbers *) value;
  SvgNumbers *p;
  double size;

  if (orig->n_values == 0)
    return svg_value_ref ((SvgValue *) orig);

  if (attr == SHAPE_ATTR_TRANSFORM_ORIGIN)
    {
      double font_size;

      if (value == svg_numbers_new_00 ())
        return svg_value_ref ((SvgValue *) value);

      p = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (2));
      memcpy (p->values, orig->values, sizeof (Number) * 2);

      font_size = shape_get_current_font_size (shape, attr, context);

      for (unsigned int i = 0; i < 2; i++)
        {
          switch ((unsigned int) p->values[i].unit)
            {
            case SVG_UNIT_NUMBER:
            case SVG_UNIT_PERCENTAGE:
              break;

            case SVG_UNIT_PX:
            case SVG_UNIT_PT:
            case SVG_UNIT_IN:
            case SVG_UNIT_CM:
            case SVG_UNIT_MM:
              p->values[i].value = absolute_length_to_px (p->values[i].value, p->values[i].unit);
              p->values[i].unit = SVG_UNIT_PX;
              break;
            case SVG_UNIT_VW:
            case SVG_UNIT_VH:
            case SVG_UNIT_VMIN:
            case SVG_UNIT_VMAX:
              p->values[i].value = viewport_relative_to_px (p->values[i].value, p->values[i].unit, context->viewport);
              p->values[i].unit = SVG_UNIT_PX;
              break;
            case SVG_UNIT_EM:
              p->values[i].value = p->values[i].value * font_size;
              p->values[i].unit = SVG_UNIT_PX;
              break;
            case SVG_UNIT_EX:
              p->values[i].value = p->values[i].value * 0.5 * font_size;
              p->values[i].unit = SVG_UNIT_PX;
              break;
            default:
              g_assert_not_reached ();
            }
        }

      return (SvgValue *) p;
    }

  p = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (orig->n_values));
  p->n_values = orig->n_values;

  size = normalized_diagonal (context->viewport);
  for (unsigned int i = 0; i < orig->n_values; i++)
    {
      if (orig->values[i].unit == SVG_UNIT_PERCENTAGE)
        {
          p->values[i].unit = SVG_UNIT_NUMBER;
          p->values[i].value = orig->values[i].value * size / 100;
        }
      else
        {
          p->values[i].value = orig->values[i].value;
        }
    }

  return (SvgValue *) p;
}

/* }}} */
/* {{{ Points */

/* Points are just like number, with an even number of them */

static SvgValue *
svg_points_parse (const char *value)
{
  if (match_str (value, "none"))
    {
      return svg_numbers_new_none ();
    }
  else
    {
      GStrv strv;
      unsigned int n;
      SvgNumbers *p;

      strv = strsplit_set (value, ", ");
      n = g_strv_length (strv);

      if (n % 2 == 1)
        n--;

      p = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (n));
      p->n_values = n;

      for (unsigned int i = 0; i < n; i++)
        {
          if (!parse_numeric (strv[i], -DBL_MAX, DBL_MAX, NUMBER|PERCENTAGE|LENGTH,
                              &p->values[i].value, &p->values[i].unit))

            {
              svg_value_unref ((SvgValue *) p);
              p = NULL;
              break;
            }
       }
      g_strfreev (strv);

      return (SvgValue *) p;
    }
}

/* }}} */
/* {{{ Strings */

typedef struct
{
  SvgValue base;
  char *value;
} SvgString;

static void
svg_string_free (SvgValue *value)
{
  SvgString *s = (SvgString *)value;
  g_free (s->value);
  g_free (s);
}

static gboolean
svg_string_equal (const SvgValue *value0,
                  const SvgValue *value1)
{
  const SvgString *s0 = (const SvgString *)value0;
  const SvgString *s1 = (const SvgString *)value1;

  return g_strcmp0 (s0->value, s1->value) == 0;
}

static SvgValue *
svg_string_interpolate (const SvgValue *value0,
                        const SvgValue *value1,
                        ComputeContext *context,
                        double          t)
{
  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_string_accumulate (const SvgValue *value0,
                       const SvgValue *value1,
                       ComputeContext *context,
                       int             n)
{
  return svg_value_ref ((SvgValue *)value0);
}

static void
svg_string_print (const SvgValue *value,
                  GString        *string)
{
  const SvgString *s = (const SvgString *)value;
  gchar *escaped = g_markup_escape_text (s->value, strlen (s->value));
  g_string_append (string, escaped);
  g_free (escaped);
}

static const char *
svg_string_get (const SvgValue *value)
{
  const SvgString *s = (const SvgString *)value;
  return s->value;
}

static const SvgValueClass SVG_STRING_CLASS = {
  "SvgString",
  svg_string_free,
  svg_string_equal,
  svg_string_interpolate,
  svg_string_accumulate,
  svg_string_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

static SvgValue *
svg_string_new (const char *str)
{
  static SvgString empty = { { &SVG_STRING_CLASS, 0 }, .value = (char *) "" };
  SvgString *result;

  if (str[0] == '\0')
    return svg_value_ref ((SvgValue *) &empty);

  result = (SvgString *) svg_value_alloc (&SVG_STRING_CLASS, sizeof (SvgString));
  result->value = g_strdup (str);
  return (SvgValue *) result;
}

/* }}} */
/* {{{ String Lists */

typedef struct
{
  SvgValue base;
  unsigned int len;
  char *values[1];
} SvgStringList;

static unsigned int
svg_string_list_size (unsigned int n)
{
  return sizeof (SvgStringList) + MAX (n - 1, 0) * sizeof (char *);
}

static void
svg_string_list_free (SvgValue *value)
{
  SvgStringList *s = (SvgStringList *)value;
  for (size_t i = 0; i < s->len; i++)
    g_free (s->values[i]);
  g_free (s);
}

static gboolean
svg_string_list_equal (const SvgValue *value0,
                       const SvgValue *value1)
{
  const SvgStringList *s0 = (const SvgStringList *)value0;
  const SvgStringList *s1 = (const SvgStringList *)value1;

  if (s0->len != s1->len)
    return FALSE;

  for (unsigned int i = 0; i < s0->len; i++)
    {
      if (strcmp (s0->values[i], s1->values[i]) != 0)
        return FALSE;
    }

  return TRUE;
}

static SvgValue *
svg_string_list_interpolate (const SvgValue *value0,
                             const SvgValue *value1,
                             ComputeContext *context,
                             double          t)
{
  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_string_list_accumulate (const SvgValue *value0,
                            const SvgValue *value1,
                            ComputeContext *context,
                            int             n)
{
  return svg_value_ref ((SvgValue *)value0);
}

static void
svg_string_list_print (const SvgValue *value,
                       GString        *string)
{
  const SvgStringList *s = (const SvgStringList *)value;

  for (unsigned int i = 0; i < s->len; i++)
    {
      gchar *escaped = g_markup_escape_text (s->values[i], strlen (s->values[i]));
      if (i > 0)
        g_string_append_c (string, ' ');
      g_string_append (string, escaped);
      g_free (escaped);
    }
}

static const SvgValueClass SVG_STRING_LIST_CLASS = {
  "SvgStringList",
  svg_string_list_free,
  svg_string_list_equal,
  svg_string_list_interpolate,
  svg_string_list_accumulate,
  svg_string_list_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

static SvgValue *
svg_string_list_new (GStrv strv)
{
  static SvgStringList empty = { { &SVG_STRING_LIST_CLASS, 0 }, .len = 0, .values[0] = NULL, };
  SvgStringList *result;
  unsigned int len;

  if (strv)
    len = g_strv_length (strv);
  else
    len = 0;

  if (len == 0)
    return svg_value_ref ((SvgValue *) &empty);

  result = (SvgStringList *) svg_value_alloc (&SVG_STRING_LIST_CLASS, svg_string_list_size (len));

  result->len = len;
  for (unsigned int i = 0; i < len; i++)
    result->values[i] = g_strdup (strv[i]);

  return (SvgValue *) result;
}

/* }}} */
/* {{{ Enums */

typedef struct
{
  SvgValue base;
  unsigned int value;
  const char *name;
  size_t len;
} SvgEnum;

static gboolean
svg_enum_equal (const SvgValue *value0,
                const SvgValue *value1)
{
  const SvgEnum *n0 = (const SvgEnum *) value0;
  const SvgEnum *n1 = (const SvgEnum *) value1;

  return n0->value == n1->value;
}

static SvgValue *
svg_enum_interpolate (const SvgValue *value0,
                      const SvgValue *value1,
                      ComputeContext *context,
                      double          t)
{
  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_enum_accumulate (const SvgValue *value0,
                     const SvgValue *value1,
                     ComputeContext *context,
                     int             n)
{
  return svg_value_ref ((SvgValue *) value0);
}

static void
svg_enum_print (const SvgValue *value,
                GString        *string)
{
  const SvgEnum *e = (const SvgEnum *) value;
  g_string_append (string, e->name);
}

static unsigned int
svg_enum_get (const SvgValue *value)
{
  const SvgEnum *e = (const SvgEnum *) value;
  return e->value;
}

static SvgValue *
svg_enum_parse (const SvgEnum  values[],
                unsigned int   n_values,
                const char    *string)
{
  for (unsigned int i = 0; i < n_values; i++)
    {
      if (values[i].name && match_str_len (string, values[i].name, values[i].len))
        return svg_value_ref ((SvgValue *) &values[i]);
    }
  return NULL;
}

/* Note that name must be a string literal */
#define DEFINE_ENUM_VALUE(CLASS_NAME, value, name) \
  { { & SVG_ ## CLASS_NAME ## _CLASS, 0 }, value, name, sizeof (name) - 1, }

#define DEFINE_ENUM_VALUE_NO_NAME(CLASS_NAME, value) \
  { { & SVG_ ## CLASS_NAME ## _CLASS, 0 }, value, NULL, 0, }

#define DEF_E(kw,CLASS_NAME, class_name, EnumType, resolve, ...) \
static const SvgValueClass SVG_ ## CLASS_NAME ## _CLASS = { \
  #CLASS_NAME, \
  svg_value_default_free, \
  svg_enum_equal, \
  svg_enum_interpolate, \
  svg_enum_accumulate, \
  svg_enum_print, \
  svg_value_default_distance, \
  resolve, \
}; \
\
static SvgEnum class_name ## _values[] = { \
  __VA_ARGS__ , \
}; \
\
kw SvgValue * \
svg_ ## class_name ## _new (EnumType value) \
{ \
  for (unsigned int i = 0; i < G_N_ELEMENTS (class_name ## _values); i++) \
    { \
      if (class_name ## _values[i].value == value) \
        return (SvgValue *) & class_name ## _values[i]; \
    } \
  g_assert_not_reached (); \
} \

#define DEF_E_PARSE(class_name) \
static SvgValue * \
svg_ ## class_name ## _parse (const char *string) \
{ \
  return svg_enum_parse (class_name ## _values, \
                         G_N_ELEMENTS (class_name ## _values), \
                         string); \
}

#define DEFINE_ENUM_PUBLIC(CLASS_NAME, class_name, EnumType, ...) \
  DEF_E(,CLASS_NAME, class_name, EnumType, svg_value_default_resolve, __VA_ARGS__) \
  DEF_E_PARSE(class_name)

#define DEFINE_ENUM(CLASS_NAME, class_name, EnumType, ...) \
  DEF_E(static, CLASS_NAME, class_name, EnumType, svg_value_default_resolve, __VA_ARGS__) \
  DEF_E_PARSE(class_name)

#define DEFINE_ENUM_PUBLIC_NO_PARSE(CLASS_NAME, class_name, EnumType, ...) \
  DEF_E(, CLASS_NAME, class_name, EnumType, svg_value_default_resolve, __VA_ARGS__)

#define DEFINE_ENUM_NO_PARSE(CLASS_NAME, class_name, EnumType, ...) \
  DEF_E(static, CLASS_NAME, class_name, EnumType, svg_value_default_resolve, __VA_ARGS__)

#define DEFINE_ENUM_CUSTOM_RESOLVE(CLASS_NAME, class_name, EnumType, resolve, ...) \
  DEF_E(static, CLASS_NAME, class_name, EnumType, resolve, __VA_ARGS__) \
  DEF_E_PARSE(class_name)

DEFINE_ENUM_PUBLIC (FILL_RULE, fill_rule, GskFillRule,
  DEFINE_ENUM_VALUE (FILL_RULE, GSK_FILL_RULE_WINDING, "nonzero"),
  DEFINE_ENUM_VALUE (FILL_RULE, GSK_FILL_RULE_EVEN_ODD, "evenodd")
)

DEFINE_ENUM (MASK_TYPE, mask_type, GskMaskMode,
  DEFINE_ENUM_VALUE (MASK_TYPE, GSK_MASK_MODE_ALPHA, "alpha"),
  DEFINE_ENUM_VALUE (MASK_TYPE, GSK_MASK_MODE_LUMINANCE, "luminance")
)

DEFINE_ENUM_PUBLIC (LINE_CAP, linecap, GskLineCap,
  DEFINE_ENUM_VALUE (LINE_CAP, GSK_LINE_CAP_BUTT, "butt"),
  DEFINE_ENUM_VALUE (LINE_CAP, GSK_LINE_CAP_ROUND, "round"),
  DEFINE_ENUM_VALUE (LINE_CAP, GSK_LINE_CAP_SQUARE, "square")
)

DEFINE_ENUM_PUBLIC (LINE_JOIN, linejoin, GskLineJoin,
  DEFINE_ENUM_VALUE (LINE_JOIN, GSK_LINE_JOIN_MITER, "miter"),
  DEFINE_ENUM_VALUE (LINE_JOIN, GSK_LINE_JOIN_ROUND, "round"),
  DEFINE_ENUM_VALUE (LINE_JOIN, GSK_LINE_JOIN_BEVEL, "bevel")
)

typedef enum
{
  VISIBILITY_HIDDEN,
  VISIBILITY_VISIBLE,
} Visibility;

DEFINE_ENUM_NO_PARSE (VISIBILITY, visibility, Visibility,
  DEFINE_ENUM_VALUE (VISIBILITY, VISIBILITY_HIDDEN, "hidden"),
  DEFINE_ENUM_VALUE (VISIBILITY, VISIBILITY_VISIBLE, "visible")
)

/* Accept other values too, but treat "collapse" as "hidden" */
static SvgValue *
svg_visibility_parse (const char *string)
{
  if (match_str (string, "hidden") ||
      match_str (string, "collapse"))
    return svg_visibility_new (VISIBILITY_HIDDEN);
  else
    return svg_visibility_new (VISIBILITY_VISIBLE);
}

typedef enum
{
  DISPLAY_NONE,
  DISPLAY_INLINE,
} SvgDisplay;

DEFINE_ENUM_NO_PARSE (DISPLAY, display, SvgDisplay,
  DEFINE_ENUM_VALUE (DISPLAY, DISPLAY_NONE, "none"),
  DEFINE_ENUM_VALUE (DISPLAY, DISPLAY_INLINE, "inline")
)

/* Accept other values too, but treat them all as "inline" */
static SvgValue *
svg_display_parse (const char *string)
{
  if (match_str (string, "none"))
    return svg_display_new (DISPLAY_NONE);
  else
    return svg_display_new (DISPLAY_INLINE);
}

DEFINE_ENUM (SPREAD_METHOD, spread_method, GskRepeat,
  DEFINE_ENUM_VALUE (SPREAD_METHOD, GSK_REPEAT_PAD, "pad"),
  DEFINE_ENUM_VALUE (SPREAD_METHOD, GSK_REPEAT_REFLECT, "reflect"),
  DEFINE_ENUM_VALUE (SPREAD_METHOD, GSK_REPEAT_REPEAT, "repeat")
)

DEFINE_ENUM (COORD_UNITS, coord_units, CoordUnits,
  DEFINE_ENUM_VALUE (COORD_UNITS, COORD_UNITS_USER_SPACE_ON_USE, "userSpaceOnUse"),
  DEFINE_ENUM_VALUE (COORD_UNITS, COORD_UNITS_OBJECT_BOUNDING_BOX, "objectBoundingBox")
)

DEFINE_ENUM_PUBLIC_NO_PARSE (PAINT_ORDER, paint_order, PaintOrder,
  DEFINE_ENUM_VALUE (PAINT_ORDER, PAINT_ORDER_FILL_STROKE_MARKERS, "normal"),
  DEFINE_ENUM_VALUE (PAINT_ORDER, PAINT_ORDER_FILL_STROKE_MARKERS, "fill stroke markers"),
  DEFINE_ENUM_VALUE (PAINT_ORDER, PAINT_ORDER_FILL_MARKERS_STROKE, "fill markers stroke"),
  DEFINE_ENUM_VALUE (PAINT_ORDER, PAINT_ORDER_STROKE_FILL_MARKERS, "stroke fill markers"),
  DEFINE_ENUM_VALUE (PAINT_ORDER, PAINT_ORDER_STROKE_MARKERS_FILL, "stroke markers fill"),
  DEFINE_ENUM_VALUE (PAINT_ORDER, PAINT_ORDER_MARKERS_FILL_STROKE, "markers fill stroke"),
  DEFINE_ENUM_VALUE (PAINT_ORDER, PAINT_ORDER_MARKERS_STROKE_FILL, "markers stroke fill")
)

static SvgValue *
svg_paint_order_parse (const char *string)
{
  GStrv strv;
  char *key;

  if (match_str (string, "normal"))
    return svg_paint_order_new (PAINT_ORDER_FILL_STROKE_MARKERS);

  strv = strsplit_set (string, " ");
  key = g_strjoinv (" ", strv);

  for (unsigned int i = 1; i < G_N_ELEMENTS (paint_order_values); i++)
    {
      if (g_str_has_prefix (paint_order_values[i].name, key))
        {
          g_strfreev (strv);
          g_free (key);
          return svg_paint_order_new (paint_order_values[i].value);
        }
    }

  g_strfreev (strv);
  g_free (key);

  return NULL;
}

typedef enum
{
  OVERFLOW_VISIBLE,
  OVERFLOW_HIDDEN,
  OVERFLOW_AUTO,
} SvgOverflow;

DEFINE_ENUM (OFLOW, overflow, SvgOverflow,
  DEFINE_ENUM_VALUE (OFLOW, OVERFLOW_VISIBLE, "visible"),
  DEFINE_ENUM_VALUE (OFLOW, OVERFLOW_HIDDEN, "hidden"),
  DEFINE_ENUM_VALUE (OFLOW, OVERFLOW_AUTO, "auto")
)

typedef enum {
  ISOLATION_AUTO,
  ISOLATION_ISOLATE,
} Isolation;

DEFINE_ENUM (ISOLATION, isolation, Isolation,
  DEFINE_ENUM_VALUE (ISOLATION, ISOLATION_AUTO, "auto"),
  DEFINE_ENUM_VALUE (ISOLATION, ISOLATION_ISOLATE, "isolate")
)

DEFINE_ENUM (BLEND_MODE, blend_mode, GskBlendMode,
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_DEFAULT, "normal"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_MULTIPLY, "multiply"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_SCREEN, "screen"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_OVERLAY, "overlay"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_DARKEN, "darken"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_LIGHTEN, "lighten"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_COLOR_DODGE, "color-dodge"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_COLOR_BURN, "color-burn"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_HARD_LIGHT, "hard-light"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_SOFT_LIGHT, "soft-light"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_DIFFERENCE, "difference"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_EXCLUSION, "exclusion"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_COLOR, "color"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_HUE, "hue"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_SATURATION, "saturation"),
  DEFINE_ENUM_VALUE (BLEND_MODE, GSK_BLEND_MODE_LUMINOSITY, "luminosity")
)

typedef enum {
  TEXT_ANCHOR_START,
  TEXT_ANCHOR_MIDDLE,
  TEXT_ANCHOR_END
} TextAnchor;

DEFINE_ENUM (TEXT_ANCHOR, text_anchor, TextAnchor,
  DEFINE_ENUM_VALUE (TEXT_ANCHOR, TEXT_ANCHOR_START, "start"),
  DEFINE_ENUM_VALUE (TEXT_ANCHOR, TEXT_ANCHOR_MIDDLE, "middle"),
  DEFINE_ENUM_VALUE (TEXT_ANCHOR, TEXT_ANCHOR_END, "end")
)

typedef enum
{
  MARKER_UNITS_STROKE_WIDTH,
  MARKER_UNITS_USER_SPACE_ON_USE,
} MarkerUnits;

DEFINE_ENUM (MARKER_UNITS, marker_units, MarkerUnits,
  DEFINE_ENUM_VALUE (MARKER_UNITS, MARKER_UNITS_STROKE_WIDTH, "strokeWidth"),
  DEFINE_ENUM_VALUE (MARKER_UNITS, MARKER_UNITS_USER_SPACE_ON_USE, "userSpaceOnUse")
)

typedef enum {
  UNICODE_BIDI_NORMAL,
  UNICODE_BIDI_EMBED,
  UNICODE_BIDI_OVERRIDE
} UnicodeBidi;

DEFINE_ENUM (UNICODE_BIDI, unicode_bidi, UnicodeBidi,
  DEFINE_ENUM_VALUE (UNICODE_BIDI, UNICODE_BIDI_NORMAL, "normal"),
  DEFINE_ENUM_VALUE (UNICODE_BIDI, UNICODE_BIDI_EMBED, "embed"),
  DEFINE_ENUM_VALUE (UNICODE_BIDI, UNICODE_BIDI_OVERRIDE, "bidi-override")
)

DEFINE_ENUM (DIRECTION, direction, PangoDirection,
  DEFINE_ENUM_VALUE (DIRECTION, PANGO_DIRECTION_LTR, "ltr"),
  DEFINE_ENUM_VALUE (DIRECTION, PANGO_DIRECTION_RTL, "rtl")
)

typedef enum {
  WRITING_MODE_HORIZONTAL_TB,
  WRITING_MODE_VERTICAL_RL,
  WRITING_MODE_VERTICAL_LR,

  /* SVG 1.1 legacy properties */
  WRITING_MODE_LEGACY_LR,
  WRITING_MODE_LEGACY_LR_TB,
  WRITING_MODE_LEGACY_RL,
  WRITING_MODE_LEGACY_RL_TB,
  WRITING_MODE_LEGACY_TB,
  WRITING_MODE_LEGACY_TB_RL,
} WritingMode;

static const gboolean is_vertical_writing_mode[] = {
  FALSE, TRUE, TRUE,
  FALSE, FALSE, FALSE, FALSE, TRUE, TRUE
};

DEFINE_ENUM (WRITING_MODE, writing_mode, WritingMode,
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_HORIZONTAL_TB, "horizontal-tb"),
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_VERTICAL_RL, "vertical-rl"),
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_VERTICAL_LR, "vertical-lr"),
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_LEGACY_LR, "lr"),
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_LEGACY_LR_TB, "lr-tb"),
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_LEGACY_RL, "rl"),
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_LEGACY_RL_TB, "rl-tb"),
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_LEGACY_TB, "tb"),
  DEFINE_ENUM_VALUE (WRITING_MODE, WRITING_MODE_LEGACY_TB_RL, "tb-rl")
)

DEFINE_ENUM (FONT_STYLE, font_style, PangoStyle,
  DEFINE_ENUM_VALUE (FONT_STYLE, PANGO_STYLE_NORMAL, "normal"),
  DEFINE_ENUM_VALUE (FONT_STYLE, PANGO_STYLE_OBLIQUE, "oblique"),
  DEFINE_ENUM_VALUE (FONT_STYLE, PANGO_STYLE_ITALIC, "italic")
)

DEFINE_ENUM (FONT_VARIANT, font_variant, PangoVariant,
  DEFINE_ENUM_VALUE (FONT_VARIANT, PANGO_VARIANT_NORMAL, "normal"),
  DEFINE_ENUM_VALUE (FONT_VARIANT, PANGO_VARIANT_SMALL_CAPS, "small-caps"),
  DEFINE_ENUM_VALUE (FONT_VARIANT, PANGO_VARIANT_ALL_SMALL_CAPS, "all-small-caps"),
  DEFINE_ENUM_VALUE (FONT_VARIANT, PANGO_VARIANT_PETITE_CAPS, "petite-caps"),
  DEFINE_ENUM_VALUE (FONT_VARIANT, PANGO_VARIANT_ALL_PETITE_CAPS, "all-petite-caps"),
  DEFINE_ENUM_VALUE (FONT_VARIANT, PANGO_VARIANT_UNICASE, "unicase"),
  DEFINE_ENUM_VALUE (FONT_VARIANT, PANGO_VARIANT_TITLE_CAPS, "titling-caps")
)

DEFINE_ENUM (FONT_STRETCH, font_stretch, PangoStretch,
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_ULTRA_CONDENSED, "ultra-condensed"),
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_EXTRA_CONDENSED, "extra-condensed"),
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_CONDENSED, "condensed"),
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_SEMI_CONDENSED, "semi-condensed"),
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_NORMAL, "normal"),
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_SEMI_EXPANDED, "semi-expanded"),
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_EXPANDED, "expanded"),
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_EXTRA_EXPANDED, "extra-expanded"),
  DEFINE_ENUM_VALUE (FONT_STRETCH, PANGO_STRETCH_ULTRA_EXPANDED, "ultra-expanded")
)

typedef enum
{
  EDGE_MODE_DUPLICATE,
  EDGE_MODE_WRAP,
  EDGE_MODE_NONE,
} EdgeMode;

DEFINE_ENUM (EDGE_MODE, edge_mode, EdgeMode,
  DEFINE_ENUM_VALUE (EDGE_MODE, EDGE_MODE_DUPLICATE, "duplicate"),
  DEFINE_ENUM_VALUE (EDGE_MODE, EDGE_MODE_WRAP, "wrap"),
  DEFINE_ENUM_VALUE (EDGE_MODE, EDGE_MODE_NONE, "none")
)

typedef enum
{
  BLEND_COMPOSITE,
  BLEND_NO_COMPOSITE,
} BlendComposite;

DEFINE_ENUM (BLEND_COMPOSITE, blend_composite, BlendComposite,
  DEFINE_ENUM_VALUE_NO_NAME (BLEND_COMPOSITE, BLEND_COMPOSITE),
  DEFINE_ENUM_VALUE (BLEND_COMPOSITE, BLEND_NO_COMPOSITE, "no-composite")
)

typedef enum
{
  COLOR_MATRIX_TYPE_MATRIX,
  COLOR_MATRIX_TYPE_SATURATE,
  COLOR_MATRIX_TYPE_HUE_ROTATE,
  COLOR_MATRIX_TYPE_LUMINANCE_TO_ALPHA,
} ColorMatrixType;

DEFINE_ENUM (COLOR_MATRIX_TYPE, color_matrix_type, ColorMatrixType,
  DEFINE_ENUM_VALUE (COLOR_MATRIX_TYPE, COLOR_MATRIX_TYPE_MATRIX, "matrix"),
  DEFINE_ENUM_VALUE (COLOR_MATRIX_TYPE, COLOR_MATRIX_TYPE_SATURATE, "saturate"),
  DEFINE_ENUM_VALUE (COLOR_MATRIX_TYPE, COLOR_MATRIX_TYPE_HUE_ROTATE, "hueRotate"),
  DEFINE_ENUM_VALUE (COLOR_MATRIX_TYPE, COLOR_MATRIX_TYPE_LUMINANCE_TO_ALPHA, "luminanceToAlpha")
)

typedef enum
{
  COMPOSITE_OPERATOR_OVER,
  COMPOSITE_OPERATOR_IN,
  COMPOSITE_OPERATOR_OUT,
  COMPOSITE_OPERATOR_ATOP,
  COMPOSITE_OPERATOR_XOR,
  COMPOSITE_OPERATOR_LIGHTER,
  COMPOSITE_OPERATOR_ARITHMETIC,
} CompositeOperator;

DEFINE_ENUM (COMPOSITE_OPERATOR, composite_operator, CompositeOperator,
  DEFINE_ENUM_VALUE (COMPOSITE_OPERATOR, COMPOSITE_OPERATOR_OVER, "over"),
  DEFINE_ENUM_VALUE (COMPOSITE_OPERATOR, COMPOSITE_OPERATOR_IN, "in"),
  DEFINE_ENUM_VALUE (COMPOSITE_OPERATOR, COMPOSITE_OPERATOR_OUT, "out"),
  DEFINE_ENUM_VALUE (COMPOSITE_OPERATOR, COMPOSITE_OPERATOR_ATOP, "atop"),
  DEFINE_ENUM_VALUE (COMPOSITE_OPERATOR, COMPOSITE_OPERATOR_XOR, "xor"),
  DEFINE_ENUM_VALUE (COMPOSITE_OPERATOR, COMPOSITE_OPERATOR_LIGHTER, "lighter"),
  DEFINE_ENUM_VALUE (COMPOSITE_OPERATOR, COMPOSITE_OPERATOR_ARITHMETIC, "arithmetic")
)

static GskPorterDuff
svg_composite_operator_to_gsk (CompositeOperator op)
{
  switch (op)
    {
    case COMPOSITE_OPERATOR_OVER: return GSK_PORTER_DUFF_SOURCE_OVER_DEST;
    case COMPOSITE_OPERATOR_IN: return GSK_PORTER_DUFF_SOURCE_IN_DEST;
    case COMPOSITE_OPERATOR_OUT: return GSK_PORTER_DUFF_SOURCE_OUT_DEST;
    case COMPOSITE_OPERATOR_ATOP: return GSK_PORTER_DUFF_SOURCE_ATOP_DEST;
    case COMPOSITE_OPERATOR_XOR: return GSK_PORTER_DUFF_XOR;
    case COMPOSITE_OPERATOR_LIGHTER: return GSK_PORTER_DUFF_SOURCE; /* Not used */
    case COMPOSITE_OPERATOR_ARITHMETIC: return GSK_PORTER_DUFF_SOURCE; /* Not used */
    default:
      g_assert_not_reached ();
    }
}

DEFINE_ENUM (RGBA_CHANNEL, rgba_channel, GdkColorChannel,
  DEFINE_ENUM_VALUE (RGBA_CHANNEL, GDK_COLOR_CHANNEL_RED, "R"),
  DEFINE_ENUM_VALUE (RGBA_CHANNEL, GDK_COLOR_CHANNEL_GREEN, "G"),
  DEFINE_ENUM_VALUE (RGBA_CHANNEL, GDK_COLOR_CHANNEL_BLUE, "B"),
  DEFINE_ENUM_VALUE (RGBA_CHANNEL, GDK_COLOR_CHANNEL_ALPHA, "A")
)

typedef enum
{
  COMPONENT_TRANSFER_IDENTITY,
  COMPONENT_TRANSFER_TABLE,
  COMPONENT_TRANSFER_DISCRETE,
  COMPONENT_TRANSFER_LINEAR,
  COMPONENT_TRANSFER_GAMMA,
} ComponentTransferType;

DEFINE_ENUM (COMPONENT_TRANSFER_TYPE, component_transfer_type, ComponentTransferType,
  DEFINE_ENUM_VALUE (COMPONENT_TRANSFER_TYPE, COMPONENT_TRANSFER_IDENTITY, "identity"),
  DEFINE_ENUM_VALUE (COMPONENT_TRANSFER_TYPE, COMPONENT_TRANSFER_TABLE, "table"),
  DEFINE_ENUM_VALUE (COMPONENT_TRANSFER_TYPE, COMPONENT_TRANSFER_DISCRETE, "discrete"),
  DEFINE_ENUM_VALUE (COMPONENT_TRANSFER_TYPE, COMPONENT_TRANSFER_LINEAR, "linear"),
  DEFINE_ENUM_VALUE (COMPONENT_TRANSFER_TYPE, COMPONENT_TRANSFER_GAMMA, "gamma")
)

typedef enum
{
  VECTOR_EFFECT_NONE,
  VECTOR_EFFECT_NON_SCALING_STROKE,
} VectorEffect;

DEFINE_ENUM (VECTOR_EFFECT, vector_effect, VectorEffect,
  DEFINE_ENUM_VALUE (VECTOR_EFFECT, VECTOR_EFFECT_NONE, "none"),
  DEFINE_ENUM_VALUE (VECTOR_EFFECT, VECTOR_EFFECT_NON_SCALING_STROKE, "non-scaling-stroke")
)

typedef enum
{
  FONT_SIZE_SMALLER,
  FONT_SIZE_LARGER,
  FONT_SIZE_XX_SMALL,
  FONT_SIZE_X_SMALL,
  FONT_SIZE_SMALL,
  FONT_SIZE_MEDIUM,
  FONT_SIZE_LARGE,
  FONT_SIZE_X_LARGE,
  FONT_SIZE_XX_LARGE,
} FontSize;

static SvgValue *
svg_font_size_resolve (const SvgValue *value,
                       ShapeAttr       attr,
                       unsigned int    idx,
                       Shape          *shape,
                       ComputeContext *context)
{
  const SvgEnum *font_size = (const SvgEnum *) value;

  g_assert (attr == SHAPE_ATTR_FONT_SIZE);

  if (font_size->value == FONT_SIZE_XX_SMALL)
    return svg_number_new (DEFAULT_FONT_SIZE * 3. / 5.);
  else if (font_size->value == FONT_SIZE_X_SMALL)
    return svg_number_new (DEFAULT_FONT_SIZE * 3. / 4.);
  else if (font_size->value == FONT_SIZE_SMALL)
    return svg_number_new (DEFAULT_FONT_SIZE * 8. / 9.);
  else if (font_size->value == FONT_SIZE_MEDIUM)
    return svg_number_new (DEFAULT_FONT_SIZE);
  else if (font_size->value == FONT_SIZE_LARGE)
    return svg_number_new (DEFAULT_FONT_SIZE * 6. / 5.);
  else if (font_size->value == FONT_SIZE_X_LARGE)
    return svg_number_new (DEFAULT_FONT_SIZE * 3. / 2.);
  else if (font_size->value == FONT_SIZE_XX_LARGE)
    return svg_number_new (DEFAULT_FONT_SIZE * 2.);
  else
    {
      double parent_size;

      if (context->parent)
        parent_size = ((SvgNumber *) context->parent->current[SHAPE_ATTR_FONT_SIZE])->value;
      else
        parent_size = DEFAULT_FONT_SIZE;

      if (font_size->value == FONT_SIZE_SMALLER)
        return svg_number_new (parent_size / 1.2);
      else if (font_size->value == FONT_SIZE_LARGER)
        return svg_number_new (parent_size * 1.2);
      else
        g_assert_not_reached ();
    }
}

DEFINE_ENUM_CUSTOM_RESOLVE (FONT_SIZE, font_size, FontSize, svg_font_size_resolve,
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_SMALLER,  "smaller"),
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_LARGER,   "larger"),
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_XX_SMALL, "xx-small"),
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_X_SMALL,  "x-small"),
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_SMALL,    "small"),
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_MEDIUM,   "medium"),
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_LARGE,    "larger"),
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_X_LARGE,  "x-large"),
  DEFINE_ENUM_VALUE (FONT_SIZE, FONT_SIZE_XX_LARGE, "xx-large")
)

typedef enum {
  FONT_WEIGHT_NORMAL,
  FONT_WEIGHT_BOLD,
  FONT_WEIGHT_BOLDER,
  FONT_WEIGHT_LIGHTER,
} FontWeight;

static SvgValue *
svg_font_weight_resolve (const SvgValue *value,
                         ShapeAttr       attr,
                         unsigned int    idx,
                         Shape          *shape,
                         ComputeContext *context)
{
  const SvgEnum *font_weight = (const SvgEnum *) value;

  g_assert (attr == SHAPE_ATTR_FONT_WEIGHT);

  if (font_weight->value == FONT_WEIGHT_NORMAL)
    return svg_number_new (PANGO_WEIGHT_NORMAL);
  else if (font_weight->value == FONT_WEIGHT_BOLD)
    return svg_number_new (PANGO_WEIGHT_BOLD);
  else
    {
      double parent_weight;

      if (context->parent)
        parent_weight = ((SvgNumber *) context->parent->current[SHAPE_ATTR_FONT_WEIGHT])->value;
      else
        parent_weight = PANGO_WEIGHT_NORMAL;

      if (font_weight->value == FONT_WEIGHT_BOLDER)
        {
          if (parent_weight < 350)
            return svg_number_new (400);
          else if (parent_weight < 550)
            return svg_number_new (700);
          else if (parent_weight < 900)
            return svg_number_new (900);
          else
            return svg_number_new (parent_weight);
        }
      else if (font_weight->value == FONT_WEIGHT_LIGHTER)
        {
          if (parent_weight > 750)
            return svg_number_new (700);
          else if (parent_weight > 550)
            return svg_number_new (400);
          else if (parent_weight > 100)
            return svg_number_new (100);
          else
            return svg_number_new (parent_weight);
        }
      else
        g_assert_not_reached ();
    }
}

DEFINE_ENUM_CUSTOM_RESOLVE (FONT_WEIGHT, font_weight, FontWeight, svg_font_weight_resolve,
  DEFINE_ENUM_VALUE (FONT_WEIGHT, FONT_WEIGHT_NORMAL,  "normal"),
  DEFINE_ENUM_VALUE (FONT_WEIGHT, FONT_WEIGHT_BOLD,    "bold"),
  DEFINE_ENUM_VALUE (FONT_WEIGHT, FONT_WEIGHT_BOLDER,  "bolder"),
  DEFINE_ENUM_VALUE (FONT_WEIGHT, FONT_WEIGHT_LIGHTER, "lighter")
)

typedef enum
{
  COLOR_INTERPOLATION_AUTO,
  COLOR_INTERPOLATION_SRGB,
  COLOR_INTERPOLATION_LINEAR,
} ColorInterpolation;

static SvgValue * svg_color_interpolation_resolve (const SvgValue *value,
                                                   ShapeAttr       attr,
                                                   unsigned int    idx,
                                                   Shape          *shape,
                                                   ComputeContext *context);

DEFINE_ENUM_CUSTOM_RESOLVE (COLOR_INTERPOLATION, color_interpolation, ColorInterpolation, svg_color_interpolation_resolve,
  DEFINE_ENUM_VALUE (COLOR_INTERPOLATION, COLOR_INTERPOLATION_AUTO,   "auto"),
  DEFINE_ENUM_VALUE (COLOR_INTERPOLATION, COLOR_INTERPOLATION_SRGB,   "sRGB"),
  DEFINE_ENUM_VALUE (COLOR_INTERPOLATION, COLOR_INTERPOLATION_LINEAR, "linearRGB")
)

static SvgValue *
svg_color_interpolation_resolve (const SvgValue *value,
                                 ShapeAttr       attr,
                                 unsigned int    idx,
                                 Shape          *shape,
                                 ComputeContext *context)
{
  if (svg_enum_get (value) == COLOR_INTERPOLATION_AUTO)
    return svg_color_interpolation_new (COLOR_INTERPOLATION_SRGB);
  else
    return svg_value_ref ((SvgValue *) value);
}

typedef enum
{
  TRANSFORM_BOX_CONTENT_BOX,
  TRANSFORM_BOX_BORDER_BOX,
  TRANSFORM_BOX_FILL_BOX,
  TRANSFORM_BOX_STROKE_BOX,
  TRANSFORM_BOX_VIEW_BOX,
} TransformBox;

DEFINE_ENUM (TRANSFORM_BOX, transform_box, TransformBox,
  DEFINE_ENUM_VALUE (TRANSFORM_BOX, TRANSFORM_BOX_CONTENT_BOX, "content-box"),
  DEFINE_ENUM_VALUE (TRANSFORM_BOX, TRANSFORM_BOX_BORDER_BOX, "border-box"),
  DEFINE_ENUM_VALUE (TRANSFORM_BOX, TRANSFORM_BOX_FILL_BOX, "fill-box"),
  DEFINE_ENUM_VALUE (TRANSFORM_BOX, TRANSFORM_BOX_STROKE_BOX, "stroke-box"),
  DEFINE_ENUM_VALUE (TRANSFORM_BOX, TRANSFORM_BOX_VIEW_BOX, "view-box")
)

typedef enum
{
  SHAPE_RENDERING_AUTO,
  SHAPE_RENDERING_OPTIMIZE_SPEED,
  SHAPE_RENDERING_CRISP_EDGES,
  SHAPE_RENDERING_GEOMETRIC_PRECISION,
} ShapeRendering;

DEFINE_ENUM (SHAPE_RENDERING, shape_rendering, ShapeRendering,
  DEFINE_ENUM_VALUE (SHAPE_RENDERING, SHAPE_RENDERING_AUTO, "auto"),
  DEFINE_ENUM_VALUE (SHAPE_RENDERING, SHAPE_RENDERING_OPTIMIZE_SPEED, "optimizeSpeed"),
  DEFINE_ENUM_VALUE (SHAPE_RENDERING, SHAPE_RENDERING_CRISP_EDGES, "crispEdges"),
  DEFINE_ENUM_VALUE (SHAPE_RENDERING, SHAPE_RENDERING_GEOMETRIC_PRECISION, "geometricPrecision")
)

typedef enum
{
  TEXT_RENDERING_AUTO,
  TEXT_RENDERING_OPTIMIZE_SPEED,
  TEXT_RENDERING_OPTIMIZE_LEGIBILITY,
  TEXT_RENDERING_GEOMETRIC_PRECISION,
} TextRendering;

DEFINE_ENUM (TEXT_RENDERING, text_rendering, TextRendering,
  DEFINE_ENUM_VALUE (TEXT_RENDERING, TEXT_RENDERING_AUTO, "auto"),
  DEFINE_ENUM_VALUE (TEXT_RENDERING, TEXT_RENDERING_OPTIMIZE_SPEED, "optimizeSpeed"),
  DEFINE_ENUM_VALUE (TEXT_RENDERING, TEXT_RENDERING_OPTIMIZE_LEGIBILITY, "optimizeLegibility"),
  DEFINE_ENUM_VALUE (TEXT_RENDERING, TEXT_RENDERING_GEOMETRIC_PRECISION , "geometricPrecision")
)

typedef enum
{
  IMAGE_RENDERING_AUTO,
  IMAGE_RENDERING_OPTIMIZE_QUALITY,
  IMAGE_RENDERING_OPTIMIZE_SPEED,
} ImageRendering;

DEFINE_ENUM (IMAGE_RENDERING, image_rendering, ImageRendering,
  DEFINE_ENUM_VALUE (IMAGE_RENDERING, IMAGE_RENDERING_AUTO, "auto"),
  DEFINE_ENUM_VALUE (IMAGE_RENDERING, IMAGE_RENDERING_OPTIMIZE_QUALITY, "optimizeQuality"),
  DEFINE_ENUM_VALUE (IMAGE_RENDERING, IMAGE_RENDERING_OPTIMIZE_SPEED, "optimizeSpeed")
)

/* }}} */
/* {{{ Filter primitive references */

typedef enum
{
  DEFAULT_SOURCE,
  SOURCE_GRAPHIC,
  SOURCE_ALPHA,
  BACKGROUND_IMAGE,
  BACKGROUND_ALPHA,
  FILL_PAINT,
  STROKE_PAINT,
  PRIMITIVE_REF,
} SvgFilterPrimitiveRefType;

typedef struct
{
  SvgValue base;
  SvgFilterPrimitiveRefType type;
  const char *ref;
} SvgFilterPrimitiveRef;

static void
svg_filter_primitive_ref_free (SvgValue *value)
{
  SvgFilterPrimitiveRef *f = (SvgFilterPrimitiveRef *) value;

  if (f->type == PRIMITIVE_REF)
    g_free ((gpointer) f->ref);

  g_free (f);
}

static gboolean
svg_filter_primitive_ref_equal (const SvgValue *value0,
                                const SvgValue *value1)
{
  const SvgFilterPrimitiveRef *f0 = (SvgFilterPrimitiveRef *) value0;
  const SvgFilterPrimitiveRef *f1 = (SvgFilterPrimitiveRef *) value1;

  if (f0->type != f1->type)
    return FALSE;

  if (f0->type == PRIMITIVE_REF)
    return strcmp (f0->ref, f1->ref) == 0;

  return TRUE;
}

static SvgValue *
svg_filter_primitive_ref_interpolate (const SvgValue *value0,
                                      const SvgValue *value1,
                                      ComputeContext *context,
                                      double          t)
{
  const SvgFilterPrimitiveRef *f0 = (const SvgFilterPrimitiveRef *) value0;
  const SvgFilterPrimitiveRef *f1 = (const SvgFilterPrimitiveRef *) value1;

  if (f0->type != f1->type)
    return NULL;

  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_filter_primitive_ref_accumulate (const SvgValue *value0,
                                     const SvgValue *value1,
                                     ComputeContext *context,
                                     int             n)
{
  return NULL;
}

static void
svg_filter_primitive_ref_print (const SvgValue *value,
                                GString        *string)
{
  const SvgFilterPrimitiveRef *f = (const SvgFilterPrimitiveRef *) value;

  if (f->type != DEFAULT_SOURCE)
    g_string_append (string, f->ref);
}

static const SvgValueClass SVG_FILTER_PRIMITIVE_REF_CLASS = {
  "SvgPrimitiveRef",
  svg_filter_primitive_ref_free,
  svg_filter_primitive_ref_equal,
  svg_filter_primitive_ref_interpolate,
  svg_filter_primitive_ref_accumulate,
  svg_filter_primitive_ref_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

static SvgFilterPrimitiveRef filter_primitive_ref_values[] = {
     { { &SVG_FILTER_PRIMITIVE_REF_CLASS, 0 }, .type = DEFAULT_SOURCE, .ref = NULL, },
     { { &SVG_FILTER_PRIMITIVE_REF_CLASS, 0 }, .type = SOURCE_GRAPHIC, .ref = "SourceGraphic", },
     { { &SVG_FILTER_PRIMITIVE_REF_CLASS, 0 }, .type = SOURCE_ALPHA, .ref = "SourceAlpha", },
     { { &SVG_FILTER_PRIMITIVE_REF_CLASS, 0 }, .type = BACKGROUND_IMAGE, .ref = "BackgroundImage", },
     { { &SVG_FILTER_PRIMITIVE_REF_CLASS, 0 }, .type = BACKGROUND_ALPHA, .ref = "BackgroundAlpha", },
     { { &SVG_FILTER_PRIMITIVE_REF_CLASS, 0 }, .type = FILL_PAINT, .ref = "FillPaint", },
     { { &SVG_FILTER_PRIMITIVE_REF_CLASS, 0 }, .type = STROKE_PAINT, .ref = "StrokePaint", },
  };

static SvgValue *
svg_filter_primitive_ref_new (SvgFilterPrimitiveRefType type)
{
  g_assert (type < PRIMITIVE_REF);

  return svg_value_ref ((SvgValue *) &filter_primitive_ref_values[type]);
}

static SvgValue *
svg_filter_primitive_ref_new_ref (const char *ref)
{
  SvgFilterPrimitiveRef *f = (SvgFilterPrimitiveRef *) svg_value_alloc (&SVG_FILTER_PRIMITIVE_REF_CLASS,
                                                                        sizeof (SvgFilterPrimitiveRef));

  f->type = PRIMITIVE_REF;
  f->ref = g_strdup (ref);

  return (SvgValue *) f;
}

static SvgValue *
svg_filter_primitive_ref_parse (const char *value)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (filter_primitive_ref_values); i++)
    {
      if (filter_primitive_ref_values[i].ref &&
          strcmp (value, filter_primitive_ref_values[i].ref) == 0)
        return svg_value_ref ((SvgValue *) &filter_primitive_ref_values[i]);
    }

  return svg_filter_primitive_ref_new_ref (value);
}

/* }}} */
/* {{{ Transforms */

typedef struct
{
  TransformType type;
  union {
    struct {
      double x, y;
    } translate, scale;
    struct {
      double angle;
      double x, y;
    } rotate;
    struct {
      double angle;
    } skew_x;
    struct {
      double angle;
    } skew_y;
    union {
      struct {
        double xx, yx, xy, yy, dx, dy;
      };
      double m[6];
    } matrix;
  };
} PrimitiveTransform;

typedef struct
{
  SvgValue base;
  unsigned int n_transforms;
  PrimitiveTransform transforms[1];
} SvgTransform;

static unsigned int
svg_transform_size (unsigned int n)
{
  return sizeof (SvgTransform) + (n - 1) * sizeof (PrimitiveTransform);
}

static gboolean
primitive_transform_equal (const PrimitiveTransform *t0,
                           const PrimitiveTransform *t1)
{
  if (t0->type != t1->type)
    return FALSE;

  switch (t0->type)
    {
    case TRANSFORM_NONE:
      return TRUE;
    case TRANSFORM_TRANSLATE:
      return t0->translate.x == t1->translate.x &&
             t0->translate.y == t1->translate.y;
    case TRANSFORM_SCALE:
      return t0->scale.x == t1->scale.x &&
             t0->scale.y == t1->scale.y;
    case TRANSFORM_ROTATE:
      return t0->rotate.angle == t1->rotate.angle &&
             t0->rotate.x == t1->rotate.x &&
             t0->rotate.y == t1->rotate.y;
    case TRANSFORM_SKEW_X:
      return t0->skew_x.angle == t1->skew_x.angle;
    case TRANSFORM_SKEW_Y:
      return t0->skew_y.angle == t1->skew_y.angle;
    case TRANSFORM_MATRIX:
      return t0->matrix.xx == t1->matrix.xx && t0->matrix.yx == t1->matrix.yx &&
             t0->matrix.xy == t1->matrix.xy && t0->matrix.yy == t1->matrix.yy &&
             t0->matrix.dx == t1->matrix.dx && t0->matrix.dy == t1->matrix.dy;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
svg_transform_equal (const SvgValue *value0,
                     const SvgValue *value1)
{
  const SvgTransform *t0 = (const SvgTransform *) value0;
  const SvgTransform *t1 = (const SvgTransform *) value1;

  if (t0->n_transforms != t1->n_transforms)
    return FALSE;

  for (unsigned int i = 0; i < t1->n_transforms; i++)
    {
      if (!primitive_transform_equal (&t0->transforms[i], &t1->transforms[i]))
        return FALSE;
    }

  return TRUE;
}

static SvgValue *svg_transform_interpolate (const SvgValue *v0,
                                            const SvgValue *v1,
                                            ComputeContext *context,
                                            double          t);
static SvgValue *svg_transform_accumulate  (const SvgValue *v0,
                                            const SvgValue *v1,
                                            ComputeContext *context,
                                            int             n);
static void      svg_transform_print       (const SvgValue *v,
                                            GString        *string);
static double    svg_transform_distance    (const SvgValue *v0,
                                            const SvgValue *v1);


static const SvgValueClass SVG_TRANSFORM_CLASS = {
  "SvgTransform",
  svg_value_default_free,
  svg_transform_equal,
  svg_transform_interpolate,
  svg_transform_accumulate,
  svg_transform_print,
  svg_transform_distance,
  svg_value_default_resolve,
};

static SvgTransform *
svg_transform_alloc (unsigned int n)
{
  SvgTransform *t;

  t = (SvgTransform *) svg_value_alloc (&SVG_TRANSFORM_CLASS, svg_transform_size (n));
  t->n_transforms = n;

  return t;
}

SvgValue *
svg_transform_new_none (void)
{
  static SvgTransform none = { { &SVG_TRANSFORM_CLASS, 0 }, 1, { { TRANSFORM_NONE, } }};
  return (SvgValue *) &none;
}

SvgValue *
svg_transform_new_translate (double x, double y)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_TRANSLATE;
  tf->transforms[0].translate.x = x;
  tf->transforms[0].translate.y = y;
  return (SvgValue *) tf;
}

SvgValue *
svg_transform_new_scale (double x, double y)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_SCALE;
  tf->transforms[0].scale.x = x;
  tf->transforms[0].scale.y = y;
  return (SvgValue *) tf;
}

SvgValue *
svg_transform_new_rotate (double angle, double x, double y)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_ROTATE;
  tf->transforms[0].rotate.angle = angle;
  tf->transforms[0].rotate.x = x;
  tf->transforms[0].rotate.y = y;
  return (SvgValue *) tf;
}

SvgValue *
svg_transform_new_skew_x (double angle)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_SKEW_X;
  tf->transforms[0].skew_x.angle = angle;
  return (SvgValue *) tf;
}

SvgValue *
svg_transform_new_skew_y (double angle)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_SKEW_Y;
  tf->transforms[0].skew_y.angle = angle;
  return (SvgValue *) tf;
}

SvgValue *
svg_transform_new_matrix (double params[6])
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_MATRIX;
  memcpy (tf->transforms[0].matrix.m, params, sizeof (double) * 6);
  return (SvgValue *) tf;
}

static SvgValue *
svg_transform_new_rotate_and_shift (double angle,
                                    graphene_point_t *orig,
                                    graphene_point_t *final)
{
  SvgTransform *tf = svg_transform_alloc (3);
  tf->transforms[0].type = TRANSFORM_TRANSLATE;
  tf->transforms[0].translate.x = final->x;
  tf->transforms[0].translate.y = final->y;
  tf->transforms[1].type = TRANSFORM_ROTATE;
  tf->transforms[1].rotate.angle = angle;
  tf->transforms[1].rotate.x = 0;
  tf->transforms[1].rotate.y = 0;
  tf->transforms[2].type = TRANSFORM_TRANSLATE;
  tf->transforms[2].translate.x = - orig->x;
  tf->transforms[2].translate.y = - orig->y;
  return (SvgValue *) tf;
}

static guint
css_parser_parse_number (GtkCssParser *parser,
                         guint         n,
                         gpointer      data)
{
  Number *val = data;
  double d;

  if (!gtk_css_parser_consume_number (parser, &d))
    return 0;

  val[n].value = d;
  val[n].unit = SVG_UNIT_NUMBER;

  return 1;
}

static guint
css_parser_parse_number_length (GtkCssParser *parser,
                                guint         n,
                                gpointer      data)
{
  Number *val = data;
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (parser);
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_NUMBER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_NUMBER))
    {
      val[n].value = ((const GtkCssNumberToken *) token)->number;
      val[n].unit = SVG_UNIT_NUMBER;
      gtk_css_parser_consume_token (parser);
      return 1;
    }

  if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_DIMENSION) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_DIMENSION))
    {
      for (unsigned int i = FIRST_LENGTH_UNIT; i <= LAST_LENGTH_UNIT; i++)
        {
          if (strcmp (unit_names[i], ((const GtkCssDimensionToken *) token)->dimension) == 0)
            {
              val[n].value = ((const GtkCssDimensionToken *) token)->value;
              val[n].unit = i;
              gtk_css_parser_consume_token (parser);
              return TRUE;
            }
        }
    }

  return 0;
}

static guint
css_parser_parse_number_angle (GtkCssParser *parser,
                               guint         n,
                               gpointer      data)
{
  Number *val = data;
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (parser);
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_NUMBER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_NUMBER))
    {
      val[n].value = ((const GtkCssNumberToken *) token)->number;
      val[n].unit = SVG_UNIT_NUMBER;
      gtk_css_parser_consume_token (parser);
      return 1;
    }

  if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_DIMENSION) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_DIMENSION))
    {
      for (unsigned int i = FIRST_ANGLE_UNIT; i <= LAST_ANGLE_UNIT; i++)
        {
          if (strcmp (unit_names[i], ((const GtkCssDimensionToken *) token)->dimension) == 0)
            {
              val[n].value = ((const GtkCssDimensionToken *) token)->value;
              val[n].unit = i;
              gtk_css_parser_consume_token (parser);
              return TRUE;
            }
        }
    }

  return 0;
}

static guint
css_parser_parse_number_percentage (GtkCssParser *parser,
                                    guint         n,
                                    gpointer      data)
{
  Number *val = data;
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (parser);
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_NUMBER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_NUMBER))
    {
      val[n].value = ((const GtkCssNumberToken *) token)->number;
      val[n].unit = SVG_UNIT_NUMBER;
      gtk_css_parser_consume_token (parser);
      return 1;
    }

  if (gtk_css_token_is (token, GTK_CSS_TOKEN_PERCENTAGE))
    {
      val[n].value = ((const GtkCssNumberToken *) token)->number;
      val[n].unit = SVG_UNIT_PERCENTAGE;
      gtk_css_parser_consume_token (parser);
      return 1;
    }

  return 0;
}

static gboolean
parse_transform_function (GtkCssParser *self,
                          guint         min_args,
                          guint         max_args,
                          double       *values)
{
  const GtkCssToken *token;
  gboolean result = FALSE;
  char function_name[64];
  guint arg;
  Number *num;

  num = g_newa (Number, max_args);

  token = gtk_css_parser_get_token (self);
  g_return_val_if_fail (gtk_css_token_is (token, GTK_CSS_TOKEN_FUNCTION), FALSE);

  g_strlcpy (function_name, gtk_css_token_get_string (token), 64);
  gtk_css_parser_start_block (self);

  arg = 0;
  while (TRUE)
    {
      guint parse_args = css_parser_parse_number (self, arg, num);
      if (parse_args == 0)
        break;
      values[arg] = num[arg].value;
      arg += parse_args;
      token = gtk_css_parser_get_token (self);
      if (gtk_css_token_is (token, GTK_CSS_TOKEN_EOF))
        {
          if (arg < min_args)
            {
              gtk_css_parser_error_syntax (self, "%s() requires at least %u arguments", function_name, min_args);
              break;
            }
          else
            {
              result = TRUE;
              break;
            }
        }
      else if (gtk_css_token_is (token, GTK_CSS_TOKEN_COMMA))
        {
          if (arg >= max_args)
            {
              gtk_css_parser_error_syntax (self, "Expected ')' at end of %s()", function_name);
              break;
            }

          gtk_css_parser_consume_token (self);
          continue;
        }
      else if (!gtk_css_parser_has_number (self))
        {
          gtk_css_parser_error_syntax (self, "Unexpected data at end of %s() argument", function_name);
          break;
        }
    }

  gtk_css_parser_end_block (self);

  return result;
}

static SvgValue *
transform_parser_parse (GtkCssParser *parser)
{
  SvgTransform *tf;
  GArray *array;

  if (gtk_css_parser_try_ident (parser, "none"))
    return svg_transform_new_none ();

  array = g_array_new (FALSE, FALSE, sizeof (PrimitiveTransform));

    while (TRUE)
    {
      PrimitiveTransform transform;

      memset (&transform, 0, sizeof (PrimitiveTransform));

      if (gtk_css_parser_has_function (parser, "rotate"))
        {
           double values[3] = { 0, 0, 0 };

          if (!parse_transform_function (parser, 1, 3, values))
            goto fail;

          transform.type = TRANSFORM_ROTATE;
          transform.rotate.angle = values[0];
          transform.rotate.x = values[1];
          transform.rotate.y = values[2];
        }
      else if (gtk_css_parser_has_function (parser, "scale"))
        {
          double values[2] = { 0, 0 };

          if (!parse_transform_function (parser, 1, 2, values))
            goto fail;

          transform.type = TRANSFORM_SCALE;
          transform.scale.x = values[0];
          if (values[1])
            transform.scale.y = values[1];
          else
            transform.scale.y = transform.scale.x;
        }
      else if (gtk_css_parser_has_function (parser, "translate"))
        {
          double values[2] = { 0, 0 };

          if (!parse_transform_function (parser, 1, 2, values))
            goto fail;

          transform.type = TRANSFORM_TRANSLATE;
          transform.translate.x = values[0];
          if (values[1])
            transform.translate.y = values[1];
          else
            transform.translate.y = 0;
        }
      else if (gtk_css_parser_has_function (parser, "skewX"))
        {
          double values[1];

          if (!parse_transform_function (parser, 1, 1, values))
            goto fail;

          transform.type = TRANSFORM_SKEW_X;
          transform.skew_x.angle = values[0];
        }
      else if (gtk_css_parser_has_function (parser, "skewY"))
        {
          double values[1];

          if (!parse_transform_function (parser, 1, 1, values))
            goto fail;

          transform.type = TRANSFORM_SKEW_Y;
          transform.skew_y.angle = values[0];
        }
      else if (gtk_css_parser_has_function (parser, "matrix"))
        {
          double values[6];

          if (!parse_transform_function (parser, 6, 6, values))
            goto fail;

          transform.type = TRANSFORM_MATRIX;
          memcpy (transform.matrix.m, values, sizeof (double) * 6);
        }
      else
        {
          break;
        }

      g_array_append_val (array, transform);

      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COMMA))
        gtk_css_parser_skip (parser);
    }

  if (array->len == 0)
    goto fail;

  tf = svg_transform_alloc (array->len);
  memcpy (tf->transforms, array->data, sizeof (PrimitiveTransform) * array->len);

  g_array_free (array, TRUE);

  return (SvgValue *) tf;

fail:
  g_array_free (array, TRUE);
  return NULL;
}

SvgValue *
svg_transform_parse (const char *value)
{
  SvgValue *tf;
  GtkCssParser *parser;
  GBytes *bytes;

  bytes = g_bytes_new_static (value, strlen (value));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL);

  tf = transform_parser_parse (parser);

  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    g_clear_pointer (&tf, svg_value_unref);
  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);

  return tf;
}

static SvgValue *
primitive_transform_parse (TransformType  type,
                           const char    *value)
{
  GStrv strv;
  unsigned int n;

  strv = strsplit_set (value, ", ");
  n = g_strv_length (strv);

  switch (type)
    {
    case TRANSFORM_TRANSLATE:
      {
        double x, y;

        if (n < 1 || n > 2 ||
            !parse_number (strv[0], -DBL_MAX, DBL_MAX, &x) ||
            ((n == 2) && !parse_number (strv[1], -DBL_MAX, DBL_MAX, &y)))
          {
            g_strfreev (strv);
            return NULL;
          }

        g_strfreev (strv);
        return svg_transform_new_translate (x, n == 2 ? y : 0);
      }

    case TRANSFORM_SCALE:
      {
        double x, y;

        if (n < 1 || n > 2 ||
            !parse_number (strv[0], -DBL_MAX, DBL_MAX, &x) ||
            ((n == 2) && !parse_number (strv[1], -DBL_MAX, DBL_MAX, &y)))
          {
            g_strfreev (strv);
            return NULL;
          }

        g_strfreev (strv);
        return svg_transform_new_scale (x, n == 2 ? y : x);
      }

    case TRANSFORM_ROTATE:
      {
        double angle, x, y;

        if ((n != 1 && n != 3) ||
            !parse_number (strv[0], -DBL_MAX, DBL_MAX, &angle) ||
            ((n == 3) &&
             (!parse_number (strv[1], -DBL_MAX, DBL_MAX, &x) ||
              !parse_number (strv[2], -DBL_MAX, DBL_MAX, &y))))
          {
            g_strfreev (strv);
            return NULL;
          }

        g_strfreev (strv);
        return svg_transform_new_rotate (angle, n == 3 ? x : 0, n == 3 ? y : 0);
      }

    case TRANSFORM_SKEW_X:
      {
        double angle;

        if (n != 1 ||
            !parse_number (strv[0], -DBL_MAX, DBL_MAX, &angle))
          {
            g_strfreev (strv);
            return NULL;
          }

        g_strfreev (strv);
        return svg_transform_new_skew_x (angle);
      }

    case TRANSFORM_SKEW_Y:
      {
        double angle;

        if (n != 1 ||
            !parse_number (strv[0], -DBL_MAX, DBL_MAX, &angle))
          {
            g_strfreev (strv);
            return NULL;
          }

        g_strfreev (strv);
        return svg_transform_new_skew_y (angle);
      }

    case TRANSFORM_NONE:
    case TRANSFORM_MATRIX:
    default:
      g_assert_not_reached ();
    }
}

static void
svg_primitive_transform_print (const SvgValue *value,
                               GString        *s)
{
  const SvgTransform *tf = (const SvgTransform *) value;

  g_assert (tf->n_transforms == 1);

  switch (tf->transforms[0].type)
    {
    case TRANSFORM_TRANSLATE:
      string_append_double (s, "", tf->transforms[0].translate.x);
      string_append_double (s, " ", tf->transforms[0].translate.y);
      break;

    case TRANSFORM_SCALE:
      string_append_double (s, "", tf->transforms[0].scale.x);
      string_append_double (s, " ", tf->transforms[0].scale.y);
      break;

    case TRANSFORM_ROTATE:
      string_append_double (s, "", tf->transforms[0].rotate.angle);
      string_append_double (s, " ", tf->transforms[0].rotate.x);
      string_append_double (s, " ", tf->transforms[0].rotate.y);
      break;

    case TRANSFORM_SKEW_X:
      string_append_double (s, "", tf->transforms[0].skew_x.angle);
      break;

    case TRANSFORM_SKEW_Y:
      string_append_double (s, "", tf->transforms[0].skew_y.angle);
      break;

    case TRANSFORM_MATRIX:
    case TRANSFORM_NONE:
    default:
      g_assert_not_reached ();
    }
}

static void
svg_transform_print (const SvgValue *value,
                     GString        *s)
{
  const SvgTransform *tf = (const SvgTransform *) value;

  for (unsigned int i = 0; i < tf->n_transforms; i++)
    {
      const PrimitiveTransform *t = &tf->transforms[i];

      if (i > 0)
        g_string_append_c (s, ' ');

      switch (t->type)
        {
        case TRANSFORM_TRANSLATE:
          string_append_double (s, "translate(", t->translate.x);
          string_append_double (s, ", ", t->translate.y);
          g_string_append (s, ")");
          break;
        case TRANSFORM_SCALE:
          string_append_double (s, "scale(", t->scale.x);
          string_append_double (s, ", ", t->scale.y);
          g_string_append (s, ")");
          break;
        case TRANSFORM_ROTATE:
          string_append_double (s, "rotate(", t->rotate.angle);
          string_append_double (s, ", ", t->rotate.x);
          string_append_double (s, ", ", t->rotate.y);
          g_string_append (s, ")");
          break;
        case TRANSFORM_SKEW_X:
          string_append_double (s, "skewX(",  t->skew_x.angle);
          g_string_append (s, ")");
          break;
        case TRANSFORM_SKEW_Y:
          string_append_double (s, "skewY(", t->skew_y.angle);
          g_string_append (s, ")");
          break;
        case TRANSFORM_MATRIX:
          string_append_double (s, "matrix(", t->matrix.xx);
          string_append_double (s, ", ", t->matrix.yx);
          string_append_double (s, ", ", t->matrix.xy);
          string_append_double (s, ", ", t->matrix.yy);
          string_append_double (s, ", ", t->matrix.dx);
          string_append_double (s, ", ", t->matrix.dy);
          g_string_append (s, ")");
          break;
        case TRANSFORM_NONE:
          g_string_append (s, "none");
          break;
        default:
          g_assert_not_reached ();
        }
    }
}

static void
interpolate_matrices (double       t,
                      const double m0[6],
                      const double m1[6],
                      double       m[6])
{
  graphene_matrix_t mat0, mat1, res;

  graphene_matrix_init_from_2d (&mat0, m0[0], m0[1], m0[2], m0[3], m0[4], m0[5]);
  graphene_matrix_init_from_2d (&mat1, m1[0], m1[1], m1[2], m1[3], m1[4], m1[5]);
  graphene_matrix_interpolate (&mat0, &mat1, t, &res);
  graphene_matrix_to_2d (&res, &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]);
}

static GskTransform *
primitive_transform_apply (const PrimitiveTransform *transform,
                           GskTransform             *next)
{
  switch (transform->type)
    {
    case TRANSFORM_NONE:
      return next;

    case TRANSFORM_TRANSLATE:
      return gsk_transform_translate (next,
                                      &GRAPHENE_POINT_INIT (
                                        transform->translate.x,
                                        transform->translate.y
                                      ));

    case TRANSFORM_SCALE:
      return gsk_transform_scale (next,
                                  transform->scale.x,
                                  transform->scale.y);

    case TRANSFORM_ROTATE:
      return gsk_transform_translate (
                  gsk_transform_rotate (
                      gsk_transform_translate (next,
                                               &GRAPHENE_POINT_INIT (
                                                 transform->rotate.x,
                                                 transform->rotate.y
                                               )),
                      transform->rotate.angle),
                  &GRAPHENE_POINT_INIT (
                    -transform->rotate.x,
                    -transform->rotate.y
                  ));

    case TRANSFORM_SKEW_X:
      return gsk_transform_skew (next, transform->skew_x.angle, 0);

    case TRANSFORM_SKEW_Y:
      return gsk_transform_skew (next, 0, transform->skew_y.angle);

    case TRANSFORM_MATRIX:
      return gsk_transform_matrix_2d (next,
                                      transform->matrix.xx, transform->matrix.yx,
                                      transform->matrix.xy, transform->matrix.yy,
                                      transform->matrix.dx, transform->matrix.dy);

    default:
      g_assert_not_reached ();
    }
}

static SvgValue *
svg_transform_interpolate (const SvgValue *value0,
                           const SvgValue *value1,
                           ComputeContext *context,
                           double          t)
{
  static const PrimitiveTransform identity[] = {
    { .type = TRANSFORM_TRANSLATE, .translate = { 0, 0 } },
    { .type = TRANSFORM_SCALE, .scale = { 1, 1 } },
    { .type = TRANSFORM_ROTATE, .rotate = { 0, 0, 0 } },
    { .type = TRANSFORM_SKEW_X, .skew_x = { 0 } },
    { .type = TRANSFORM_SKEW_Y, .skew_y = { 0 } },
    { .type = TRANSFORM_MATRIX, .matrix = { .m = { 1, 0, 0, 1, 0, 0 } } },
  };

  const SvgTransform *tf0 = (const SvgTransform *) value0;
  const SvgTransform *tf1 = (const SvgTransform *) value1;
  SvgTransform *tf;

  tf = svg_transform_alloc (MAX (tf0->n_transforms, tf1->n_transforms));

  for (unsigned int i = 0; i < tf->n_transforms; i++)
    {
      const PrimitiveTransform *p0;
      const PrimitiveTransform *p1;
      PrimitiveTransform *p = &tf->transforms[i];

      if (i < tf0->n_transforms)
        p0 = &tf0->transforms[i];
      else
        p0 = &identity[tf1->transforms[i].type];

      if (i < tf1->n_transforms)
        p1 = &tf1->transforms[i];
      else
        p1 = &identity[tf0->transforms[i].type];

      if (p0->type != p1->type)
        {
          GskTransform *transform = NULL;
          graphene_matrix_t mat1, mat2, res;

          transform = NULL;
          for (unsigned int j = i; j < tf0->n_transforms; j++)
            transform = primitive_transform_apply (&tf0->transforms[j], transform);
          gsk_transform_to_matrix (transform, &mat1);
          gsk_transform_unref (transform);

          transform = NULL;
          for (unsigned int j = i; j < tf1->n_transforms; j++)
            transform = primitive_transform_apply (&tf1->transforms[j], transform);
          gsk_transform_to_matrix (transform, &mat2);
          gsk_transform_unref (transform);

          graphene_matrix_interpolate (&mat1, &mat2, t, &res);

          p->type = TRANSFORM_MATRIX;
          graphene_matrix_to_2d (&res,
                                 &p->matrix.m[0], &p->matrix.m[1],
                                 &p->matrix.m[2], &p->matrix.m[3],
                                 &p->matrix.m[4], &p->matrix.m[5]);
          tf->n_transforms = i + 1;
          break;
        }

      p->type = p0->type;
      switch (p0->type)
        {
        case TRANSFORM_TRANSLATE:
          p->translate.x = lerp (t, p0->translate.x, p1->translate.x);
          p->translate.y = lerp (t, p0->translate.y, p1->translate.y);
          break;
        case TRANSFORM_SCALE:
          p->scale.x = lerp (t, p0->scale.x, p1->scale.x);
          p->scale.y = lerp (t, p0->scale.y, p1->scale.y);
          break;
        case TRANSFORM_ROTATE:
          p->rotate.angle = lerp (t, p0->rotate.angle, p1->rotate.angle);
          p->rotate.x = lerp (t, p0->rotate.x, p1->rotate.x);
          p->rotate.y = lerp (t, p0->rotate.y, p1->rotate.y);
          break;
        case TRANSFORM_SKEW_X:
          p->skew_x.angle = lerp (t, p0->skew_x.angle, p1->skew_x.angle);
          break;
        case TRANSFORM_SKEW_Y:
          p->skew_y.angle = lerp (t, p0->skew_y.angle, p1->skew_y.angle);
          break;
        case TRANSFORM_MATRIX:
          interpolate_matrices (t, p0->matrix.m, p1->matrix.m, p->matrix.m);
          break;
        case TRANSFORM_NONE:
          break;
        default:
          g_assert_not_reached ();
        }
    }

  return (SvgValue *) tf;
}

static SvgValue *
svg_transform_accumulate (const SvgValue *value0,
                          const SvgValue *value1,
                          ComputeContext *context,
                          int             n)
{
  const SvgTransform *tf0 = (const SvgTransform *) value0;
  const SvgTransform *tf1 = (const SvgTransform *) value1;
  SvgTransform *tf;
  int n0;

  if (tf1->n_transforms == 1 && tf1->transforms[0].type == TRANSFORM_NONE)
    return svg_value_ref ((SvgValue *) value0);

  if (tf0->n_transforms == 1 && tf0->transforms[0].type == TRANSFORM_NONE)
    {
      if (n == 1)
        return svg_value_ref ((SvgValue *) value1);

      n0 = 0;
    }
  else
    n0 = tf0->n_transforms;

  /* special-case this one */
  if (tf1->n_transforms == 1 && tf1->transforms[0].type != TRANSFORM_MATRIX)
    {
      PrimitiveTransform *p;

      tf = svg_transform_alloc (n0 + 1);
      p = &tf->transforms[0];
      memcpy (p, tf1->transforms, sizeof (PrimitiveTransform));

      switch (p->type)
        {
        case TRANSFORM_TRANSLATE:
          p->translate.x *= n;
          p->translate.y *= n;
          break;
        case TRANSFORM_SCALE:
          p->scale.x = pow (p->scale.x, n);
          p->scale.y = pow (p->scale.y, n);
          break;
        case TRANSFORM_ROTATE:
          p->rotate.angle *= n;
          break;
        case TRANSFORM_SKEW_X:
          p->skew_x.angle *= n;
          break;
        case TRANSFORM_SKEW_Y:
          p->skew_y.angle *= n;
          break;
        case TRANSFORM_NONE:
          break;
        case TRANSFORM_MATRIX:
        default:
          g_assert_not_reached ();
        }

      if (n0 > 0)
        memcpy (tf->transforms + 1,
                tf0->transforms,
                n0 * sizeof (PrimitiveTransform));

    }
  else
    {
      /* For the general case, simply concatenate all the transforms */
      tf = svg_transform_alloc (n0 + n * tf1->n_transforms);
      for (unsigned int i = 0; i < n; i++)
        memcpy (&tf->transforms[i * tf1->n_transforms],
                tf1->transforms,
                tf1->n_transforms * sizeof (PrimitiveTransform));

      if (n0 > 0)
        memcpy (&tf->transforms[n * tf1->n_transforms],
                tf0->transforms,
                n0 * sizeof (PrimitiveTransform));
    }

  return (SvgValue *) tf;
}

static GskTransform *
svg_transform_get_gsk (SvgTransform *tf)
{
  GskTransform *t = NULL;

  if (!tf)
    return NULL;

  for (unsigned int i = 0; i < tf->n_transforms; i++)
    t = primitive_transform_apply (&tf->transforms[i], t);

  return t;
}

static double
svg_transform_distance (const SvgValue *value0,
                        const SvgValue *value1)
{
  const SvgTransform *tf0 = (const SvgTransform *) value0;
  const SvgTransform *tf1 = (const SvgTransform *) value1;
  const PrimitiveTransform *p0 = &tf0->transforms[0];
  const PrimitiveTransform *p1 = &tf1->transforms[0];

  if (tf0->n_transforms > 1 || tf1->n_transforms > 1)
    {
      g_warning ("Can't determine distance between complex transforms");
      return 1;
    }

  if (p0->type != p1->type)
    {
      g_warning ("Can't determine distance between different "
                 "primitive transform types");
      return 1;
    }

  switch (p0->type)
    {
    case TRANSFORM_NONE:
      return 0;
    case TRANSFORM_TRANSLATE:
      return hypot (p0->translate.x - p1->translate.x,
                    p0->translate.y - p1->translate.y);
      break;
    case TRANSFORM_SCALE:
      return hypot (p0->scale.x - p1->scale.x,
                    p0->scale.y - p1->scale.y);
          break;
    case TRANSFORM_ROTATE:
      return hypot (p0->rotate.angle - p1->rotate.angle, 0);
    case TRANSFORM_SKEW_X:
    case TRANSFORM_SKEW_Y:
    case TRANSFORM_MATRIX:
      g_warning ("Can't determine distance between these "
                 "primitive transforms");
      return 1;
    default:
      g_assert_not_reached ();
    }
}

/* }}} */
/* {{{ Color */

typedef struct
{
  SvgValue base;
  gboolean current;
  GdkColor color;
} SvgColor;

static gboolean
svg_color_equal (const SvgValue *value0,
                 const SvgValue *value1)
{
  const SvgColor *c0 = (const SvgColor *) value0;
  const SvgColor *c1 = (const SvgColor *) value1;

  return c0->current == c1->current &&
         gdk_color_equal (&c0->color, &c1->color);
}

static SvgValue *svg_color_interpolate (const SvgValue *v0,
                                        const SvgValue *v1,
                                        ComputeContext *context,
                                        double          t);
static SvgValue *svg_color_accumulate  (const SvgValue *v0,
                                        const SvgValue *v1,
                                        ComputeContext *context,
                                        int             n);
static void      svg_color_print       (const SvgValue *v0,
                                        GString        *string);
static double    svg_color_distance    (const SvgValue *v0,
                                        const SvgValue *v1);
static SvgValue * svg_color_resolve    (const SvgValue *value,
                                        ShapeAttr       attr,
                                        unsigned int    idx,
                                        Shape          *shape,
                                        ComputeContext *context);

static void
svg_color_free (SvgValue *value)
{
  SvgColor *color = (SvgColor *) value;

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

static SvgColor black_color_value =
  { { &SVG_COLOR_CLASS, 0 }, .current = 0, { .color_state = GDK_COLOR_STATE_SRGB, .r = 0, .g = 0, .b = 0, .a = 1 } };

static SvgValue *
svg_color_new_black (void)
{
  return (SvgValue *) &black_color_value;
}

static SvgValue *
svg_color_new_color (const GdkColor *color)
{
  SvgColor *res;

  if (gdk_color_equal (&black_color_value.color, color))
    return (SvgValue *) &black_color_value;

  res = (SvgColor *) svg_value_alloc (&SVG_COLOR_CLASS, sizeof (SvgColor));
  res->current = FALSE;
  gdk_color_init_copy (&res->color, color);

  return (SvgValue *) res;
}

static SvgValue *
svg_color_new_rgba (const GdkRGBA *rgba)
{
  GdkColor color;
  SvgValue *result;

  gdk_color_init_from_rgba (&color, rgba);
  result = svg_color_new_color (&color);
  gdk_color_finish (&color);

  return result;
}

static SvgValue *
svg_color_new_current (void)
{
  SvgColor *res;

  res = (SvgColor *) svg_value_alloc (&SVG_COLOR_CLASS, sizeof (SvgColor));

  res->current = TRUE;
  res->color = GDK_COLOR_SRGB (0, 0, 0, 1);

  return (SvgValue *) res;
}

static gboolean
svg_color_is_current (SvgValue *value)
{
   return ((SvgColor *) value)->current;
}

static SvgValue *
svg_color_parse (const char *value)
{
  GdkRGBA rgba;
  GtkCssParser *parser;
  GBytes *bytes;
  SvgValue *result = NULL;

  bytes = g_bytes_new_static (value, strlen (value));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL);

  if (gtk_css_parser_try_ident (parser, "currentColor"))
    {
      gtk_css_parser_skip_whitespace (parser);
      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        result = svg_color_new_current ();
    }
  else if (gdk_rgba_parser_parse (parser, &rgba))
    {
      gtk_css_parser_skip_whitespace (parser);
      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        result = svg_color_new_rgba (&rgba);
    }

  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);

  return result;
}

static void
svg_color_print (const SvgValue *value,
                 GString        *s)
{
  const SvgColor *color = (const SvgColor *) value;

  if (color->current)
    g_string_append (s, "currentColor");
  else
    {
      GdkColor c;

      /* Don't use gdk_color_print here until we parse
       * modern css color syntax.
       */
      gdk_color_convert (&c, GDK_COLOR_STATE_SRGB, &color->color);
      gdk_rgba_print ((const GdkRGBA *) c.values, s);
      gdk_color_finish (&c);
    }
}

static SvgValue *
svg_color_resolve (const SvgValue *value,
                   ShapeAttr       attr,
                   unsigned int    idx,
                   Shape          *shape,
                   ComputeContext *context)
{
  if (((SvgColor *) value)->current)
    return svg_value_ref (shape->current[SHAPE_ATTR_COLOR]);
  else
    return svg_value_ref ((SvgValue *) value);
}

static SvgValue *
svg_color_interpolate (const SvgValue *value0,
                       const SvgValue *value1,
                       ComputeContext *context,
                       double          t)
{
  const SvgColor *c0 = (const SvgColor *) value0;
  const SvgColor *c1 = (const SvgColor *) value1;

  if (!c0->current && !c1->current)
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
svg_color_accumulate (const SvgValue *value0,
                      const SvgValue *value1,
                      ComputeContext *context,
                      int             n)
{
  const SvgColor *c0 = (const SvgColor *) value0;
  const SvgColor *c1 = (const SvgColor *) value1;

  if (c0->current != c1->current)
    return NULL;

  if (!c0->current)
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

  if (c0->current != c1->current)
    {
      g_warning ("Can't measure distance between "
                 "different kinds of color");
      return 1;
    }

  if (c0->current)
    return 0;
  else
    return color_distance (&c0->color, &c1->color);
}

/* }}} */
/* {{{ Paint */

static gboolean
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
      Shape *shape;
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
  else if (paint->kind == PAINT_SERVER ||
           paint->kind == PAINT_SERVER_WITH_FALLBACK)
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

static SvgValue *svg_paint_interpolate (const SvgValue *v0,
                                        const SvgValue *v1,
                                        ComputeContext *context,
                                        double          t);
static SvgValue *svg_paint_accumulate  (const SvgValue *v0,
                                        const SvgValue *v1,
                                        ComputeContext *context,
                                        int             n);
static void      svg_paint_print       (const SvgValue *v0,
                                        GString        *string);
static double    svg_paint_distance    (const SvgValue *v0,
                                        const SvgValue *v1);
static SvgValue * svg_paint_resolve    (const SvgValue *value,
                                        ShapeAttr       attr,
                                        unsigned int    idx,
                                        Shape          *shape,
                                        ComputeContext *context);

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

static gboolean
parse_symbolic_color (const char       *value,
                      GtkSymbolicColor *symbolic)
{
  unsigned int u = 0;

  if (!parse_enum (value, symbolic_colors, G_N_ELEMENTS (symbolic_colors), &u))
    return FALSE;

  *symbolic = u;
  return TRUE;
}

static SvgValue *
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

static SvgValue *
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

static SvgValue *
svg_paint_new_black (void)
{
  return (SvgValue *) &default_color[0];
}

static SvgValue *
svg_paint_new_transparent (void)
{
  return (SvgValue *) &default_color[1];
}

static SvgValue *
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

static SvgValue *
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

static SvgValue *
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

static SvgValue *
svg_paint_parse (const char *value)
{
  GdkRGBA color;
  GtkCssParser *parser;
  GBytes *bytes;
  SvgValue *paint = NULL;

  bytes = g_bytes_new_static (value, strlen (value));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL);

  if (gtk_css_parser_try_ident (parser, "none"))
    {
      gtk_css_parser_skip_whitespace (parser);
      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        paint = svg_paint_new_simple (PAINT_NONE);
    }
  else if (gtk_css_parser_try_ident (parser, "context-fill"))
    {
      gtk_css_parser_skip_whitespace (parser);
      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        paint = svg_paint_new_simple (PAINT_CONTEXT_FILL);
    }
  else if (gtk_css_parser_try_ident (parser, "context-stroke"))
    {
      gtk_css_parser_skip_whitespace (parser);
      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        paint = svg_paint_new_simple (PAINT_CONTEXT_STROKE);
    }
  else if (gtk_css_parser_try_ident (parser, "currentColor"))
    {
      gtk_css_parser_skip_whitespace (parser);
      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        paint = svg_paint_new_simple (PAINT_CURRENT_COLOR);
    }
  else if (gdk_rgba_parser_parse (parser, &color))
    {
      gtk_css_parser_skip_whitespace (parser);
      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        paint = svg_paint_new_rgba (&color);
    }
  else if (gtk_css_parser_has_url (parser))
    {
      GdkRGBA fallback = GDK_RGBA_TRANSPARENT;
      char *url;
      const char *ref;

      url = gtk_css_parser_consume_url (parser);

      if (url[0] == '#')
        ref = url + 1;
      else
        ref = url;

      gtk_css_parser_skip_whitespace (parser);
      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        {
          paint = svg_paint_new_server (ref);
        }
      else if (gtk_css_parser_try_ident (parser, "none") ||
               gdk_rgba_parser_parse (parser, &fallback))
        {
          gtk_css_parser_skip_whitespace (parser);
          if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
            {
              GdkColor c;
              gdk_color_init_from_rgba (&c, &fallback);
              paint = svg_paint_new_server_with_fallback (ref, &c);
              gdk_color_finish (&c);
            }
        }
      else if (gtk_css_parser_try_ident (parser, "currentColor"))
        {
          gtk_css_parser_skip_whitespace (parser);
          if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
            paint = svg_paint_new_server_with_current_color (ref);
        }

      g_free (url);
    }

  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);

  return paint;
}

static SvgValue *
svg_paint_parse_gpa (const char *value)
{
  GtkSymbolicColor symbolic;
  GdkRGBA rgba;

  if (match_str (value, "none"))
    return svg_paint_new_none ();
  else if (match_str (value, "context-fill"))
    return svg_paint_new_simple (PAINT_CONTEXT_FILL);
  else if (match_str (value, "context-stroke"))
    return svg_paint_new_simple (PAINT_CONTEXT_STROKE);
  else if (match_str (value, "currentColor"))
    return svg_paint_new_simple (PAINT_CURRENT_COLOR);
  else if (parse_symbolic_color (value, &symbolic))
    return svg_paint_new_symbolic (symbolic);
  else if (rgba_parse (&rgba, value))
    return svg_paint_new_rgba (&rgba);

  return NULL;
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

static gboolean
svg_paint_is_symbolic (const SvgPaint   *paint,
                       GtkSymbolicColor *symbolic)
{
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

static SvgValue *
svg_paint_resolve (const SvgValue *value,
                   ShapeAttr       attr,
                   unsigned int    idx,
                   Shape          *shape,
                   ComputeContext *context)
{
  const SvgPaint *paint = (const SvgPaint *) value;
  GtkSymbolicColor symbolic;

  g_assert (value->class == &SVG_PAINT_CLASS);

  if (paint->kind == PAINT_CURRENT_COLOR)
    {
      SvgColor *color = (SvgColor *) shape->current[SHAPE_ATTR_COLOR];
      return svg_paint_new_color (&color->color);
    }

  if ((context->svg->features & GTK_SVG_EXTENSIONS) != 0)
    {
      if (svg_paint_is_symbolic (paint, &symbolic))
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
      SvgColor *color = (SvgColor *) shape->current[SHAPE_ATTR_COLOR];
      return svg_paint_new_server_with_fallback (paint->server.ref, &color->color);
    }

  return svg_value_ref ((SvgValue *) value);
}

static SvgValue *
svg_paint_interpolate (const SvgValue *value0,
                       const SvgValue *value1,
                       ComputeContext *context,
                       double          t)
{
  const SvgPaint *p0 = (const SvgPaint *) value0;
  const SvgPaint *p1 = (const SvgPaint *) value1;

  if (p0->kind == PAINT_COLOR || p1->kind == PAINT_COLOR)
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
svg_paint_accumulate (const SvgValue *value0,
                      const SvgValue *value1,
                      ComputeContext *context,
                      int             n)
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

/* }}} */
/* {{{ Filter functions */

typedef enum
{
  FILTER_NONE,
  FILTER_BLUR,
  FILTER_BRIGHTNESS,
  FILTER_CONTRAST,
  FILTER_GRAYSCALE,
  FILTER_HUE_ROTATE,
  FILTER_INVERT,
  FILTER_OPACITY,
  FILTER_SATURATE,
  FILTER_SEPIA,
  FILTER_ALPHA_LEVEL,
  FILTER_REF,
  FILTER_DROPSHADOW,
} FilterKind;

typedef guint (* ArgParseFunc) (GtkCssParser *, guint, gpointer);

enum
{
  CLAMPED  = 1 << 0,
  POSITIVE = 1 << 1,
};

static struct {
  FilterKind kind;
  const char *name;
  double default_value;
  unsigned int flags;
  ArgParseFunc parse_arg;
} filter_desc[] = {
  { FILTER_NONE,        "none", 0, 0,                     NULL, },
  { FILTER_BLUR,        "blur", 1, POSITIVE,              css_parser_parse_number_length, },
  { FILTER_BRIGHTNESS,  "brightness", 1, POSITIVE,        css_parser_parse_number_percentage, },
  { FILTER_CONTRAST,    "contrast", 1, POSITIVE,          css_parser_parse_number_percentage, },
  { FILTER_GRAYSCALE,   "grayscale", 1, CLAMPED|POSITIVE, css_parser_parse_number_percentage, },
  { FILTER_HUE_ROTATE,  "hue-rotate", 0, 0,               css_parser_parse_number_angle, },
  { FILTER_INVERT,      "invert", 1, CLAMPED|POSITIVE,    css_parser_parse_number_percentage, },
  { FILTER_OPACITY,     "opacity", 1, CLAMPED|POSITIVE,   css_parser_parse_number_percentage, },
  { FILTER_SATURATE,    "saturate", 1, POSITIVE,          css_parser_parse_number_percentage, },
  { FILTER_SEPIA,       "sepia", 1, CLAMPED|POSITIVE,     css_parser_parse_number_percentage, },
  { FILTER_ALPHA_LEVEL, "alpha-level", 0.5, POSITIVE,     css_parser_parse_number, },
  /* FILTER_REF and FILTER_DROPSHADOW are handled separately */
};

typedef struct
{
  FilterKind kind;
  union {
    SvgValue *simple;
    struct {
      char *ref;
      Shape *shape;
    } ref;
    struct {
      SvgValue *color;
      SvgValue *dx;
      SvgValue *dy;
      SvgValue *std_dev;
    } dropshadow;
  };
} FilterFunction;

typedef struct {
  SvgValue base;
  unsigned int n_functions;
  FilterFunction functions[1];
} SvgFilter;

static unsigned int
svg_filter_size (unsigned int n)
{
  return sizeof (SvgFilter) + (n - 1) * sizeof (FilterFunction);
}

static void
svg_filter_free (SvgValue *value)
{
  SvgFilter *f = (SvgFilter *) value;
  for (unsigned int i = 0; i < f->n_functions; i++)
    {
      if (f->functions[i].kind == FILTER_REF)
        g_free (f->functions[i].ref.ref);
      else if (f->functions[i].kind == FILTER_DROPSHADOW)
        {
          svg_value_unref (f->functions[i].dropshadow.color);
          svg_value_unref (f->functions[i].dropshadow.dx);
          svg_value_unref (f->functions[i].dropshadow.dy);
          svg_value_unref (f->functions[i].dropshadow.std_dev);
        }
      else
        svg_value_unref (f->functions[i].simple);
    }

  g_free (value);
}

static gboolean
svg_filter_equal (const SvgValue *value0,
                  const SvgValue *value1)
{
  const SvgFilter *f0 = (const SvgFilter *) value0;
  const SvgFilter *f1 = (const SvgFilter *) value1;

  if (f0->n_functions != f1->n_functions)
    return FALSE;

  for (unsigned int i = 0; i < f0->n_functions; i++)
    {
      if (f0->functions[i].kind != f1->functions[i].kind)
        return FALSE;
      if (f0->functions[i].kind == FILTER_NONE)
        return TRUE;
      else if (f0->functions[i].kind == FILTER_REF)
        return f0->functions[i].ref.shape == f1->functions[i].ref.shape &&
               strcmp (f0->functions[i].ref.ref, f1->functions[i].ref.ref) == 0;
      else if (f0->functions[i].kind == FILTER_DROPSHADOW)
        return svg_value_equal (f0->functions[i].dropshadow.color, f1->functions[i].dropshadow.color) &&
               svg_value_equal (f0->functions[i].dropshadow.dx, f1->functions[i].dropshadow.dx) &&
               svg_value_equal (f0->functions[i].dropshadow.dy, f1->functions[i].dropshadow.dy) &&
               svg_value_equal (f0->functions[i].dropshadow.std_dev, f1->functions[i].dropshadow.std_dev);
      else
        return svg_value_equal (f0->functions[i].simple, f1->functions[i].simple);
    }

  return TRUE;
}

static SvgValue *svg_filter_interpolate (const SvgValue *v0,
                                         const SvgValue *v1,
                                         ComputeContext *context,
                                         double          t);
static SvgValue *svg_filter_accumulate  (const SvgValue *v0,
                                         const SvgValue *v1,
                                         ComputeContext *context,
                                         int             n);
static void      svg_filter_print       (const SvgValue *v0,
                                         GString        *string);

static SvgValue * svg_filter_resolve    (const SvgValue *value,
                                         ShapeAttr       attr,
                                         unsigned int    idx,
                                         Shape          *shape,
                                         ComputeContext *context);

static const SvgValueClass SVG_FILTER_CLASS = {
  "SvgFilter",
  svg_filter_free,
  svg_filter_equal,
  svg_filter_interpolate,
  svg_filter_accumulate,
  svg_filter_print,
  svg_value_default_distance,
  svg_filter_resolve,
};

static SvgFilter *
svg_filter_alloc (unsigned int n)
{
  SvgFilter *f;

  f = (SvgFilter *) svg_value_alloc (&SVG_FILTER_CLASS, svg_filter_size (n));
  f->n_functions = n;

  return f;
}

static SvgValue *
svg_filter_new_none (void)
{
  static SvgFilter none = { { &SVG_FILTER_CLASS, 0 }, 1, { { FILTER_NONE, } } };
  return (SvgValue *) &none;
}

static SvgValue *
svg_filter_resolve (const SvgValue *value,
                    ShapeAttr       attr,
                    unsigned int    idx,
                    Shape          *shape,
                    ComputeContext *context)
{
  const SvgFilter *filter = (const SvgFilter *) value;
  SvgFilter *result;

  if (filter->n_functions == 1 && filter->functions[0].kind == FILTER_NONE)
    return svg_value_ref ((SvgValue *) value);

  result = svg_filter_alloc (filter->n_functions);

  for (unsigned int i = 0; i < filter->n_functions; i++)
    {
      result->functions[i].kind = filter->functions[i].kind;

      if (filter->functions[i].kind == FILTER_REF)
        {
          result->functions[i].ref.ref = g_strdup (filter->functions[i].ref.ref);
          result->functions[i].ref.shape = filter->functions[i].ref.shape;
        }
      else if (filter->functions[i].kind == FILTER_DROPSHADOW)
        {
          result->functions[i].dropshadow.color = svg_value_resolve (filter->functions[i].dropshadow.color, attr, idx, shape, context);
          result->functions[i].dropshadow.dx = svg_value_resolve (filter->functions[i].dropshadow.dx, attr, idx, shape, context);
          result->functions[i].dropshadow.dy = svg_value_resolve (filter->functions[i].dropshadow.dy, attr, idx, shape, context);
          result->functions[i].dropshadow.std_dev = svg_value_resolve (filter->functions[i].dropshadow.std_dev, attr, idx, shape, context);
        }
      else
        {
          SvgValue *v = svg_value_resolve (filter->functions[i].simple, attr, idx, shape, context);

          if (filter_desc[filter->functions[i].kind].flags & CLAMPED)
            result->functions[i].simple = svg_number_new_full (((SvgNumber *) v)->unit, CLAMP (((SvgNumber *) v)->value, 0, 1));
          else
            result->functions[i].simple = svg_value_ref (v);
          svg_value_unref (v);
        }
    }

  return (SvgValue *) result;
}

static guint
parse_drop_shadow_arg (GtkCssParser *parser,
                       guint         n,
                       gpointer      data)
{
  SvgValue **vals = data;
  Number num;
  unsigned int i;

  if (vals[0] == NULL)
    {
      GdkRGBA color;

      if (gtk_css_parser_try_ident (parser, "currentColor"))
        {
          vals[0] = svg_color_new_current ();
          return 1;
        }
      else if (gdk_rgba_parser_parse (parser, &color))
        {
          vals[0] = svg_color_new_rgba (&color);
          return 1;
        }
    }

  if (!css_parser_parse_number_length (parser, 0, &num))
    return 0;

  for (i = 1; vals[i]; i++)
    ;

  g_assert (i < 4);

  vals[i] = svg_number_new_full (num.unit, num.value);
  return 1;
}

static SvgValue *
filter_parser_parse (GtkCssParser *parser)
{
  SvgFilter *filter;
  GArray *array;

  if (gtk_css_parser_try_ident (parser, "none"))
    return svg_filter_new_none ();

  array = g_array_new (FALSE, FALSE, sizeof (FilterFunction));

  while (TRUE)
    {
      FilterFunction function;
      unsigned int i;

      memset (&function, 0, sizeof (FilterFunction));

      if (gtk_css_parser_has_url (parser))
        {
          char *url = gtk_css_parser_consume_url (parser);
          function.kind = FILTER_REF;
          if (url[0] == '#')
            function.ref.ref = g_strdup (url + 1);
          else
            function.ref.ref = g_strdup (url);
          g_free (url);
        }
      else if (gtk_css_parser_has_function (parser, "drop-shadow"))
        {
          SvgValue *values[4] = { NULL, };

          gtk_css_parser_start_block (parser);
          for (i = 0; i < 4; i++)
            {
              guint parse_args = parse_drop_shadow_arg (parser, i, values);
              if (parse_args == 0)
                break;
            }

          const GtkCssToken *token = gtk_css_parser_get_token (parser);
          if (!gtk_css_token_is (token, GTK_CSS_TOKEN_EOF))
            {
              gtk_css_parser_error_syntax (parser, "Unexpected data at the end of drop-shadow()");
              g_clear_pointer (&values[0], svg_value_unref);
              g_clear_pointer (&values[1], svg_value_unref);
              g_clear_pointer (&values[2], svg_value_unref);
              g_clear_pointer (&values[3], svg_value_unref);

              gtk_css_parser_end_block (parser);

              goto fail;
            }

          gtk_css_parser_end_block (parser);

          if (!values[1] || !values[2])
            {
              gtk_css_parser_error_syntax (parser, "failed to parse drop-shadow() arguments");
              g_clear_pointer (&values[0], svg_value_unref);
              g_clear_pointer (&values[1], svg_value_unref);
              g_clear_pointer (&values[2], svg_value_unref);
              g_clear_pointer (&values[3], svg_value_unref);

              goto fail;
            }

          if (values[0])
            function.dropshadow.color = values[0];
          else
            function.dropshadow.color = svg_color_new_current ();

          function.dropshadow.dx = values[1];
          function.dropshadow.dy = values[2];

          if (values[3])
            function.dropshadow.std_dev = values[3];
          else
            function.dropshadow.std_dev = svg_number_new (0);

          function.kind = FILTER_DROPSHADOW;
        }
      else
        {
          for (i = 1; i < G_N_ELEMENTS (filter_desc); i++)
            {
              if (gtk_css_parser_has_function (parser, filter_desc[i].name))
                {
                  Number n;

                  n.value = filter_desc[i].default_value;
                  n.unit = SVG_UNIT_NUMBER;

                  if (!gtk_css_parser_consume_function (parser, 0, 1, filter_desc[i].parse_arg, &n))
                    goto fail;

                  if ((filter_desc[i].flags & POSITIVE) && n.value < 0)
                    goto fail;

                  function.simple = svg_number_new_full (n.unit, n.value);
                  function.kind = filter_desc[i].kind;
                  break;
                }
            }
          if (i == G_N_ELEMENTS (filter_desc))
            break;
        }

      g_array_append_val (array, function);
    }

  if (array->len == 0)
    goto fail;

  filter = svg_filter_alloc (array->len);
  memcpy (filter->functions, array->data, sizeof (FilterFunction) * array->len);

  g_array_free (array, TRUE);

  return (SvgValue *) filter;

fail:
  g_array_free (array, TRUE);
  return NULL;
}

SvgValue *
svg_filter_parse (const char *value)
{
  SvgValue *filter;
  GtkCssParser *parser;
  GBytes *bytes;

  bytes = g_bytes_new_static (value, strlen (value));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL);

  filter = filter_parser_parse (parser);

  if (filter && !gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    g_clear_pointer (&filter, svg_value_unref);
  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);

  return filter;
}

static void
svg_filter_print (const SvgValue *value,
                  GString        *s)
{
  const SvgFilter *filter = (const SvgFilter *) value;

  for (unsigned int i = 0; i < filter->n_functions; i++)
    {
      const FilterFunction *function = &filter->functions[i];

      if (i > 0)
        g_string_append_c (s, ' ');

      if (function->kind == FILTER_NONE)
        {
          g_string_append (s, "none");
        }
      else if (function->kind == FILTER_REF)
        {
          g_string_append_printf (s, "url(#%s)", function->ref.ref);
        }
      else if (function->kind == FILTER_DROPSHADOW)
        {
          g_string_append (s, "drop-shadow(");
          if (!svg_color_is_current (function->dropshadow.color))
            {
              svg_value_print (function->dropshadow.color, s);
              g_string_append (s, ", ");
            }
          svg_value_print (function->dropshadow.dx, s);
          g_string_append (s, ", ");
          svg_value_print (function->dropshadow.dy, s);
          g_string_append (s, ", ");
          svg_value_print (function->dropshadow.std_dev, s);
          g_string_append_c (s, ')');
        }
      else
        {
          g_string_append (s, filter_desc[function->kind].name);
          g_string_append_c (s, '(');
          svg_value_print (function->simple, s);
          g_string_append_c (s, ')');
        }
    }
}

static SvgValue *
svg_filter_interpolate (const SvgValue *value0,
                        const SvgValue *value1,
                        ComputeContext *context,
                        double          t)
{
  const SvgFilter *f0 = (const SvgFilter *) value0;
  const SvgFilter *f1 = (const SvgFilter *) value1;
  SvgFilter *f;

  if (f0->n_functions != f1->n_functions)
    return NULL;

  /* TODO: filter interpolation wording in the spec */
  for (unsigned int i = 0; i < f0->n_functions; i++)
    {
      if (f0->functions[i].kind != f1->functions[i].kind)
        return NULL;
      if (f0->functions[i].kind != FILTER_REF &&
          ((SvgNumber *) f0->functions[i].simple)->unit != ((SvgNumber *) f1->functions[i].simple)->unit)
        return NULL;
    }

  f = svg_filter_alloc (f0->n_functions);

  for (unsigned int i = 0; i < f0->n_functions; i++)
    {
      f->functions[i].kind = f0->functions[i].kind;
      if (f->functions[i].kind == FILTER_NONE)
        ;
      else if (f->functions[i].kind == FILTER_REF)
        {
        }
      else if (f->functions[i].kind == FILTER_DROPSHADOW)
        {
          f->functions[i].dropshadow.color = svg_value_interpolate (f0->functions[i].dropshadow.color, f1->functions[i].dropshadow.color, context, t);
          f->functions[i].dropshadow.dx = svg_value_interpolate (f0->functions[i].dropshadow.dx, f1->functions[i].dropshadow.dx, context, t);
          f->functions[i].dropshadow.dy = svg_value_interpolate (f0->functions[i].dropshadow.dy, f1->functions[i].dropshadow.dy, context, t);
          f->functions[i].dropshadow.std_dev = svg_value_interpolate (f0->functions[i].dropshadow.std_dev, f1->functions[i].dropshadow.std_dev, context, t);
        }
      else
        {
          f->functions[i].simple = svg_value_interpolate (f0->functions[i].simple, f1->functions[i].simple, context, t);
        }
    }

  return (SvgValue *) f;
}

static SvgValue *
svg_filter_accumulate (const SvgValue *value0,
                       const SvgValue *value1,
                       ComputeContext *context,
                       int             n)
{
  const SvgFilter *f0 = (const SvgFilter *) value0;
  const SvgFilter *f1 = (const SvgFilter *) value1;
  SvgFilter *f;

  f = svg_filter_alloc (f0->n_functions + n * f1->n_functions);

  for (unsigned int i = 0; i < n; i++)
    memcpy (&f->functions[i * f1->n_functions],
            f1->functions,
            f1->n_functions * sizeof (FilterFunction));

  memcpy (&f->functions[f0->n_functions + (n - 1) * f1->n_functions],
          f0->functions,
          f0->n_functions * sizeof (FilterFunction));

  for (unsigned int i = 0; i < f->n_functions; i++)
    {
      if (f->functions[i].kind == FILTER_NONE)
        ;
      else if (f->functions[i].kind == FILTER_REF)
        f->functions[i].ref.ref = g_strdup (f->functions[i].ref.ref);
      else if (f->functions[i].kind == FILTER_DROPSHADOW)
        {
          svg_value_ref (f->functions[i].dropshadow.color);
          svg_value_ref (f->functions[i].dropshadow.dx);
          svg_value_ref (f->functions[i].dropshadow.dy);
          svg_value_ref (f->functions[i].dropshadow.std_dev);
        }
      else
        svg_value_ref (f->functions[i].simple);
    }

  return (SvgValue *) f;
}

#define R 0.2126
#define G 0.7152
#define B 0.0722

static gboolean
filter_function_get_color_matrix (FilterKind         kind,
                                  double             v,
                                  graphene_matrix_t *matrix,
                                  graphene_vec4_t   *offset)
{
  double c, s;

  switch (kind)
    {
    case FILTER_NONE:
    case FILTER_BLUR:
    case FILTER_ALPHA_LEVEL:
    case FILTER_REF:
    case FILTER_DROPSHADOW:
      return FALSE;
    case FILTER_BRIGHTNESS:
      graphene_matrix_init_scale (matrix, v, v, v);
      graphene_vec4_init (offset, 0, 0, 0, 0);
      return TRUE;
    case FILTER_CONTRAST:
      graphene_matrix_init_scale (matrix, v, v, v);
      graphene_vec4_init (offset, 0.5 - 0.5 * v, 0.5 - 0.5 * v, 0.5 - 0.5 * v, 0);
      return TRUE;
    case FILTER_GRAYSCALE:
      graphene_matrix_init_from_float (matrix, (float[16]) {
          1 - (1 - R) * v, R * v,               R * v,           0,
          G * v,           1.0 - (1.0 - G) * v, G * v,           0,
          B * v,           B * v,               1 - (1 - B) * v, 0,
          0,               0,                   0,               1  });
      graphene_vec4_init (offset, 0.0, 0.0, 0.0, 0.0);
      return TRUE;
    case FILTER_HUE_ROTATE:
      _sincos (DEG_TO_RAD (v), &s, &c);
      graphene_matrix_init_from_float (matrix, (float[16]) {
          0.213 + 0.787 * c - 0.213 * s,
          0.213 - 0.213 * c + 0.143 * s,
          0.213 - 0.213 * c - 0.787 * s,
          0,
          0.715 - 0.715 * c - 0.715 * s,
          0.715 + 0.285 * c + 0.140 * s,
          0.715 - 0.715 * c + 0.715 * s,
          0,
          0.072 - 0.072 * c + 0.928 * s,
          0.072 - 0.072 * c - 0.283 * s,
          0.072 + 0.928 * c + 0.072 * s,
          0,
          0, 0, 0, 1
          });
        graphene_vec4_init (offset, 0.0, 0.0, 0.0, 0.0);
      return TRUE;
    case FILTER_INVERT:
      graphene_matrix_init_scale (matrix, 1 - 2 * v, 1 - 2 * v, 1 - 2 * v);

      graphene_vec4_init (offset, v, v, v, 0);
      return TRUE;
    case FILTER_OPACITY:
      graphene_matrix_init_from_float (matrix, (float[16]) {
                                       1, 0, 0, 0,
                                       0, 1, 0, 0,
                                       0, 0, 1, 0,
                                       0, 0, 0, v });
      graphene_vec4_init (offset, 0.0, 0.0, 0.0, 0.0);
      return TRUE;
    case FILTER_SATURATE:
      graphene_matrix_init_from_float (matrix, (float[16]) {
          R + (1 - R) * v, R - R * v,       R - R * v,       0,
          G - G * v,       G + (1 - G) * v, G - G * v,       0,
          B - B * v,       B - B * v,       B + (1 - B) * v, 0,
          0,               0,               0,               1 });
      graphene_vec4_init (offset, 0, 0, 0, 0);
      return TRUE;
    case FILTER_SEPIA:
      graphene_matrix_init_from_float (matrix, (float[16]) {
          1 - 0.607 * v, 0.349 * v,     0.272 * v,     0,
          0.769 * v,     1 - 0.314 * v, 0.534 * v,     0,
          0.189 * v,     0.168 * v,     1 - 0.869 * v, 0,
          0,             0,             0,             1 });
      graphene_vec4_init (offset, 0.0, 0.0, 0.0, 0.0);
      return TRUE;
    default:
      g_assert_not_reached ();
    }
}

#undef R
#undef G
#undef B

static void
color_matrix_type_get_color_matrix (ColorMatrixType    type,
                                    SvgValue          *values,
                                    graphene_matrix_t *matrix,
                                    graphene_vec4_t   *offset)
{
  Number *numbers = ((SvgNumbers *) values)->values;

  switch (type)
    {
    case COLOR_MATRIX_TYPE_MATRIX:
      {
        float m[16];
        float o[4];

        for (unsigned int j = 0; j < 4; j++)
          o[j] = numbers[5*j + 4].value;

        graphene_vec4_init_from_float (offset, o);

        for (unsigned int i = 0; i < 4; i++)
          for (unsigned int j = 0; j < 4; j++)
            m[4*i+j] = numbers[5 * j + i].value;

        graphene_matrix_init_from_float (matrix, m);
      }
      break;
    case COLOR_MATRIX_TYPE_SATURATE:
      filter_function_get_color_matrix (FILTER_SATURATE, numbers[0].value, matrix, offset);
      break;
    case COLOR_MATRIX_TYPE_HUE_ROTATE:
      filter_function_get_color_matrix (FILTER_HUE_ROTATE, numbers[0].value, matrix, offset);
      break;
    case COLOR_MATRIX_TYPE_LUMINANCE_TO_ALPHA:
        graphene_vec4_init (offset, 0, 0, 0, 0);
        graphene_matrix_init_from_float (matrix, (float[16]) {
            0, 0, 0, 0.2126,
            0, 0, 0, 0.7152,
            0, 0, 0, 0.0722,
            0, 0, 0, 0
            });
      break;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
svg_filter_is_none (SvgValue *value)
{
  SvgFilter *f = (SvgFilter *) value;

  for (unsigned int i = 0; i < f->n_functions; i++)
    {
      if (f->functions[i].kind != FILTER_NONE)
        return FALSE;
    }

  return TRUE;
}

static gboolean filter_needs_backdrop (Shape *filter);

static gboolean
svg_filter_needs_backdrop (SvgValue *value)
{
  SvgFilter *f = (SvgFilter *) value;

  for (unsigned int i = 0; i < f->n_functions; i++)
    {
      if (f->functions[i].kind != FILTER_REF)
        continue;

      if (f->functions[i].ref.shape == NULL)
        continue;

      if (filter_needs_backdrop (f->functions[i].ref.shape))
        return TRUE;
    }

  return FALSE;
}

/* }}} */
/* {{{ Dashes */

typedef enum
{
  DASH_ARRAY_NONE,
  DASH_ARRAY_DASHES,
} DashArrayKind;

typedef struct
{
  SvgValue base;
  DashArrayKind kind;
  unsigned int n_dashes;
  Number dashes[2];
} SvgDashArray;

static unsigned int
svg_dash_array_size (unsigned int n)
{
  return sizeof (SvgDashArray) + (n > 1 ? n - 2 : 0) * sizeof (Number);
}

static gboolean
svg_dash_array_equal (const SvgValue *value0,
                      const SvgValue *value1)
{
  const SvgDashArray *da0 = (const SvgDashArray *) value0;
  const SvgDashArray *da1 = (const SvgDashArray *) value1;

  if (da0->kind != da1->kind)
    return FALSE;

  if (da0->kind == DASH_ARRAY_NONE)
    return TRUE;

  if (da0->n_dashes != da1->n_dashes)
    return FALSE;

  for (unsigned int i = 0; i < da0->n_dashes; i++)
    {
      if (da0->dashes[i].unit != da1->dashes[i].unit)
        return FALSE;
      if (da0->dashes[i].value != da1->dashes[i].value)
        return FALSE;
    }

  return TRUE;
}

static SvgValue * svg_dash_array_interpolate (const SvgValue *value0,
                                              const SvgValue *value1,
                                              ComputeContext *context,
                                              double          t);

static void svg_dash_array_print (const SvgValue *value,
                                  GString        *s);

static SvgValue * svg_dash_array_accumulate (const SvgValue *value0,
                                             const SvgValue *value1,
                                             ComputeContext *context,
                                             int             n);
static SvgValue * svg_dash_array_resolve    (const SvgValue *value,
                                             ShapeAttr       attr,
                                             unsigned int    idx,
                                             Shape          *shape,
                                             ComputeContext *context);

static const SvgValueClass SVG_DASH_ARRAY_CLASS = {
  "SvgFilter",
  svg_value_default_free,
  svg_dash_array_equal,
  svg_dash_array_interpolate,
  svg_dash_array_accumulate,
  svg_dash_array_print,
  svg_value_default_distance,
  svg_dash_array_resolve,
};

static SvgDashArray *
svg_dash_array_alloc (unsigned int n)
{
  SvgDashArray *a;

  a = (SvgDashArray *) svg_value_alloc (&SVG_DASH_ARRAY_CLASS, svg_dash_array_size (n));
  a->n_dashes = n;

  return a;
}

static SvgValue *
svg_dash_array_new_none (void)
{
  static SvgDashArray none = { { &SVG_DASH_ARRAY_CLASS, 0 }, DASH_ARRAY_NONE, 0 };
  return (SvgValue *) &none;
}

static SvgValue *
svg_dash_array_new (double       *values,
                    unsigned int  n)
{
  static SvgDashArray da01 = { { &SVG_DASH_ARRAY_CLASS, 0 }, DASH_ARRAY_DASHES, 2, { { SVG_UNIT_NUMBER, 0 }, { SVG_UNIT_NUMBER, 1 } } };
  static SvgDashArray da10 = { { &SVG_DASH_ARRAY_CLASS, 0 }, DASH_ARRAY_DASHES, 2, { { SVG_UNIT_NUMBER, 1 }, { SVG_UNIT_NUMBER, 0 } } };

  if (n == 2 && values[0] == 0 && values[1] == 1)
    {
      return (SvgValue *) &da01;
    }
  else if (n == 2 && values[0] == 1 && values[1] == 0)
    {
      return (SvgValue *) &da10;
    }
  else
    {
      SvgDashArray *a = svg_dash_array_alloc (n);

      a->kind = DASH_ARRAY_DASHES;
      for (unsigned int i = 0; i < n; i++)
        {
          a->dashes[i].unit = SVG_UNIT_NUMBER;
          a->dashes[i].value = values[i];
        }

      return (SvgValue *) a;
    }
}

static SvgValue *
svg_dash_array_parse (const char *value)
{
  if (match_str (value, "none"))
    {
      return svg_dash_array_new_none ();
    }
  else
    {
      GStrv strv;
      unsigned int n;
      SvgDashArray *dashes;

      strv = strsplit_set (value, ", ");
      n = g_strv_length (strv);

      dashes = svg_dash_array_alloc (n);
      dashes->kind = DASH_ARRAY_DASHES;

      for (unsigned int i = 0; strv[i]; i++)
        {
          if (!parse_numeric (strv[i], -DBL_MAX, DBL_MAX, NUMBER|PERCENTAGE|LENGTH,
                              &dashes->dashes[i].value, &dashes->dashes[i].unit))

            {
              svg_value_unref ((SvgValue *) dashes);
              dashes = NULL;
              break;
            }
       }
      g_strfreev (strv);

      return (SvgValue *) dashes;
    }
}

static void
svg_dash_array_print (const SvgValue *value,
                      GString        *s)
{
  const SvgDashArray *dashes = (const SvgDashArray *) value;

  if (dashes->kind == DASH_ARRAY_NONE)
    {
      g_string_append (s, "none");
    }
  else
    {
      for (unsigned int i = 0; i < dashes->n_dashes; i++)
        {
          if (i > 0)
            g_string_append_c (s, ' ');
          string_append_double (s, "", dashes->dashes[i].value);
          g_string_append (s, unit_names[dashes->dashes[i].unit]);
        }
    }
}

static SvgValue *
svg_dash_array_interpolate (const SvgValue *value0,
                            const SvgValue *value1,
                            ComputeContext *context,
                            double          t)
{
  const SvgDashArray *a0 = (const SvgDashArray *) value0;
  const SvgDashArray *a1 = (const SvgDashArray *) value1;
  SvgDashArray *a = NULL;
  unsigned int n_dashes;

  if (a0->kind != a1->kind)
    goto out;

  if (a0->kind == DASH_ARRAY_NONE)
    return (SvgValue *) svg_dash_array_new_none ();

  n_dashes = lcm (a0->n_dashes, a1->n_dashes);

  a = svg_dash_array_alloc (n_dashes);
  a->kind = a0->kind;

  for (unsigned int i = 0; i < n_dashes; i++)
    {
      if (a0->dashes[i % a0->n_dashes].unit != a1->dashes[i %a1->n_dashes].unit)
        {
          g_clear_pointer ((SvgValue **)&a, svg_value_unref);
          break;
        }

      a->dashes[i].unit = a0->dashes[i % a0->n_dashes].unit;
      a->dashes[i].value = lerp (t, a0->dashes[i % a0->n_dashes].value,
                                    a1->dashes[i % a1->n_dashes].value);
    }

out:
  if (a)
    return (SvgValue *) a;

  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_dash_array_accumulate (const SvgValue *value0,
                           const SvgValue *value1,
                           ComputeContext *context,
                           int             n)
{
  return NULL;
}

static SvgValue *
svg_dash_array_resolve (const SvgValue *value,
                        ShapeAttr       attr,
                        unsigned int    idx,
                        Shape          *shape,
                        ComputeContext *context)
{
  SvgDashArray *orig = (SvgDashArray *) value;
  SvgDashArray *a;
  double size;

  g_assert (value->class == &SVG_DASH_ARRAY_CLASS);

  if (orig->kind == DASH_ARRAY_NONE)
    return svg_value_ref ((SvgValue *) orig);

  a = svg_dash_array_alloc (orig->n_dashes);
  a->kind = orig->kind;

  size = normalized_diagonal (context->viewport);
  for (unsigned int i = 0; i < orig->n_dashes; i++)
    {
      a->dashes[i].unit = SVG_UNIT_NUMBER;
      switch ((unsigned int) orig->dashes[i].unit)
        {
        case SVG_UNIT_NUMBER:
        case SVG_UNIT_PX:
          a->dashes[i].value = orig->dashes[i].value;
          break;
        case SVG_UNIT_PERCENTAGE:
          a->dashes[i].value = orig->dashes[i].value / 100 * size;
          break;
        case SVG_UNIT_PT:
        case SVG_UNIT_IN:
        case SVG_UNIT_CM:
        case SVG_UNIT_MM:
          a->dashes[i].value = absolute_length_to_px (orig->dashes[i].value, orig->dashes[i].unit);
          break;
        case SVG_UNIT_VW:
        case SVG_UNIT_VH:
        case SVG_UNIT_VMIN:
        case SVG_UNIT_VMAX:
          a->dashes[i].value = viewport_relative_to_px (orig->dashes[i].value, orig->dashes[i].unit, context->viewport);
          break;
        case SVG_UNIT_EM:
          a->dashes[i].value = orig->dashes[i].value * shape_get_current_font_size (shape, attr, context);
          break;
        case SVG_UNIT_EX:
          a->dashes[i].value = orig->dashes[i].value * 0.5 * shape_get_current_font_size (shape, attr, context);
          break;
        default:
          g_assert_not_reached ();
        }
    }

  return (SvgValue *) a;
}

/* }}} */
/* {{{ Paths */

typedef struct
{
  SvgValue base;
  SvgPathData *pdata;
  GskPath *path;
} SvgPath;

static void
svg_path_free (SvgValue *value)
{
  SvgPath *p = (SvgPath *) value;
  g_clear_pointer (&p->pdata, svg_path_data_free);
  g_clear_pointer (&p->path, gsk_path_unref);
  g_free (value);
}

static gboolean
svg_path_equal (const SvgValue *value0,
                const SvgValue *value1)
{
  const SvgPath *p0 = (const SvgPath *) value0;
  const SvgPath *p2 = (const SvgPath *) value1;

  if (p0->path == p2->path)
    return TRUE;

  if (!p0->path || !p2->path)
    return FALSE;

  return gsk_path_equal (p0->path, p2->path);
}

static SvgValue *
svg_path_accumulate (const SvgValue *value0,
                     const SvgValue *value1,
                     ComputeContext *context,
                     int             n)
{
  return NULL;
}

static void
svg_path_print (const SvgValue *value,
                GString        *string)
{
  const SvgPath *p = (const SvgPath *) value;

  if (p->pdata)
    svg_path_data_print (p->pdata, string);
  else
    g_string_append (string, "none");
}

static SvgValue * svg_path_interpolate (const SvgValue *value1,
                                        const SvgValue *value2,
                                        ComputeContext *context,
                                        double          t);

static const SvgValueClass SVG_PATH_CLASS = {
  "SvgPath",
  svg_path_free,
  svg_path_equal,
  svg_path_interpolate,
  svg_path_accumulate,
  svg_path_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

static SvgValue *
svg_path_new_none (void)
{
  static SvgPath none = { { &SVG_PATH_CLASS, 0 }, NULL, NULL };

  return (SvgValue *) &none;
}

static SvgValue *
svg_path_new_from_data (SvgPathData *pdata)
{
  SvgPath *result;

  if (pdata == NULL)
    return NULL;

  result = (SvgPath *) svg_value_alloc (&SVG_PATH_CLASS, sizeof (SvgPath));
  result->pdata = pdata;
  result->path = svg_path_data_to_gsk (pdata);

  return (SvgValue *) result;
}

SvgValue *
svg_path_new (GskPath *path)
{
  return svg_path_new_from_data (svg_path_data_collect (path));
}

static SvgValue *
svg_path_parse (const char *value)
{
  if (match_str (value, "none"))
    return svg_path_new_none ();
  else
    return svg_path_new_from_data (svg_path_data_parse (value));
}

static GskPath *
svg_path_get_gsk (const SvgValue *value)
{
  const SvgPath *p = (const SvgPath *) value;

  g_assert (value->class == &SVG_PATH_CLASS);

  return p->path;
}

static SvgValue *
svg_path_interpolate (const SvgValue *value0,
                      const SvgValue *value1,
                      ComputeContext *context,
                      double          t)
{
  const SvgPath *p0 = (const SvgPath *) value0;
  const SvgPath *p1 = (const SvgPath *) value1;
  SvgPathData *p;

  p = svg_path_data_interpolate (p0->pdata, p1->pdata, t);

  if (p != NULL)
    return svg_path_new_from_data (p);

  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

/* }}} */
/* {{{ Clips */

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
  else if (clip->kind == CLIP_REF)
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
    case CLIP_REF:
      return c0->ref.shape == c1->ref.shape;
    default:
      g_assert_not_reached ();
    }
}

static SvgValue * svg_clip_interpolate (const SvgValue *value0,
                                        const SvgValue *value1,
                                        ComputeContext *context,
                                        double          t);

static SvgValue *
svg_clip_accumulate (const SvgValue *value0,
                     const SvgValue *value1,
                     ComputeContext *context,
                     int             n)
{
  return NULL;
}

static void
svg_clip_print (const SvgValue *value,
                GString        *string)
{
  const SvgClip *c = (const SvgClip *) value;

  switch (c->kind)
    {
    case CLIP_NONE:
      g_string_append (string, "none");
      break;
    case CLIP_PATH:
      g_string_append (string, "path(");
      switch (c->path.fill_rule)
        {
        case GSK_FILL_RULE_WINDING:
          g_string_append (string, "nonzero, ");
          break;
        case GSK_FILL_RULE_EVEN_ODD:
          g_string_append (string, "evenodd, ");
          break;
        default:
          break;
        }
      g_string_append (string, "\"");
      svg_path_data_print (c->path.pdata, string);
      g_string_append (string, "\")");
      break;
    case CLIP_REF:
      g_string_append_printf (string, "url(#%s)", c->ref.ref);
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

static SvgValue *
svg_clip_new_ref (const char *ref)
{
  SvgClip *result;

  result = (SvgClip *) svg_value_alloc (&SVG_CLIP_CLASS, sizeof (SvgClip));
  result->kind = CLIP_REF;
  result->ref.ref = g_strdup (ref);
  result->ref.shape = NULL;

  return (SvgValue *) result;
}

static SvgValue *
svg_clip_interpolate (const SvgValue *value0,
                      const SvgValue *value1,
                      ComputeContext *context,
                      double          t)
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

        case CLIP_REF:
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

#define FILL_RULE_OMITTED 0xffff
typedef struct
{
  unsigned int fill_rule;
  char *string;
} ClipPathArgs;

static guint
parse_clip_path_arg (GtkCssParser *parser,
                     guint         n,
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

static SvgValue *
svg_clip_parse (const char *value)
{
  if (match_str (value, "none"))
    {
      return svg_clip_new_none ();
    }
  else
    {
      GtkCssParser *parser;
      GBytes *bytes;
      SvgValue *res = NULL;

      bytes = g_bytes_new_static (value, strlen (value));
      parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL);

      if (gtk_css_parser_has_function (parser, "path"))
        {
          ClipPathArgs data = { .fill_rule = FILL_RULE_OMITTED, .string = NULL, };

          if (gtk_css_parser_consume_function (parser, 1, 2, parse_clip_path_arg, &data))
            {
              res = svg_clip_new_path (data.string, data.fill_rule);
              g_free (data.string);
            }
        }
      else
        {
          char *url = gtk_css_parser_consume_url (parser);
          if (!url)
            res = NULL;
          else if (url[0] == '#')
            res = svg_clip_new_ref (url + 1);
          else
            res = svg_clip_new_ref (url);
          g_free (url);
        }

      gtk_css_parser_unref (parser);
      g_bytes_unref (bytes);

      return res;
    }
}

/* }}} */
/* {{{ Masks */

typedef enum
{
  MASK_NONE,
  MASK_REF,
} MaskKind;

typedef struct
{
  SvgValue base;
  MaskKind kind;

  char *ref;
  Shape *shape;
} SvgMask;

static void
svg_mask_free (SvgValue *value)
{
  const SvgMask *m = (const SvgMask *) value;

  if (m->kind == MASK_REF)
    g_free (m->ref);

  g_free (value);
}

static gboolean
svg_mask_equal (const SvgValue *value0,
                const SvgValue *value1)
{
  const SvgMask *m0 = (const SvgMask *) value0;
  const SvgMask *m1 = (const SvgMask *) value1;

  if (m0->kind != m1->kind)
    return FALSE;

  switch (m0->kind)
    {
    case MASK_NONE:
      return TRUE;
    case MASK_REF:
      return m0->shape == m1->shape;
    default:
      g_assert_not_reached ();
    }
}

static SvgValue *
svg_mask_interpolate (const SvgValue *value0,
                      const SvgValue *value1,
                      ComputeContext *context,
                      double          t)
{
  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_mask_accumulate (const SvgValue *value0,
                     const SvgValue *value1,
                     ComputeContext *context,
                     int             n)
{
  return NULL;
}

static void
svg_mask_print (const SvgValue *value,
                GString        *string)
{
  const SvgMask *m = (const SvgMask *) value;

  switch (m->kind)
    {
    case MASK_NONE:
      g_string_append (string, "none");
      break;
    case MASK_REF:
      g_string_append_printf (string, "url(#%s)", m->ref);
      break;
    default:
      g_assert_not_reached ();
    }
}

static const SvgValueClass SVG_MASK_CLASS = {
  "SvgMask",
  svg_mask_free,
  svg_mask_equal,
  svg_mask_interpolate,
  svg_mask_accumulate,
  svg_mask_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

static SvgValue *
svg_mask_new_none (void)
{
  static SvgMask none = { { &SVG_MASK_CLASS, 0 }, MASK_NONE };
  return (SvgValue *) &none;
}

static SvgValue *
svg_mask_new_ref (const char *ref)
{
  SvgMask *result;

  result = (SvgMask *) svg_value_alloc (&SVG_MASK_CLASS, sizeof (SvgMask));
  result->kind = MASK_REF;
  result->ref = g_strdup (ref);
  result->shape = NULL;

  return (SvgValue *) result;
}

static SvgValue *
svg_mask_parse (const char *value)
{
  if (match_str (value, "none"))
    {
      return svg_mask_new_none ();
    }
  else
    {
      GtkCssParser *parser;
      GBytes *bytes;
      char *url;
      SvgValue *res = NULL;

      bytes = g_bytes_new_static (value, strlen (value));
      parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL);

      url = gtk_css_parser_consume_url (parser);
      if (!url)
        res = NULL;
      else if (url[0] == '#')
        res = svg_mask_new_ref (url + 1);
      else
        res = svg_mask_new_ref (url);

      g_free (url);
      gtk_css_parser_unref (parser);
      g_bytes_unref (bytes);

      return res;
    }
}

/* }}} */
/* {{{ Viewbox */

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
svg_view_box_interpolate (const SvgValue *value0,
                          const SvgValue *value1,
                          ComputeContext *context,
                          double          t)
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
svg_view_box_accumulate (const SvgValue *value0,
                         const SvgValue *value1,
                         ComputeContext *context,
                         int             n)
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

static SvgValue *
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

static SvgValue *
svg_view_box_parse (const char *value)
{
  GStrv strv;
  unsigned int n;
  double x, y, w, h;
  SvgValue *res = NULL;

  strv = strsplit_set (value, ", ");
  n = g_strv_length (strv);
  if (n == 4 &&
      parse_number (strv[0], -DBL_MAX, DBL_MAX, &x) &&
      parse_number (strv[1], -DBL_MAX, DBL_MAX, &y) &&
      parse_number (strv[2], 0, DBL_MAX, &w) &&
      parse_number (strv[3], 0, DBL_MAX, &h))
    {
      res = (SvgValue *) svg_view_box_new (&GRAPHENE_RECT_INIT (x, y, w, h));
    }

  g_strfreev (strv);

  return res;
}

/* }}} */
/* {{{ ContentFit */

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
svg_content_fit_interpolate (const SvgValue *value0,
                             const SvgValue *value1,
                             ComputeContext *context,
                             double          t)
{
  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_content_fit_accumulate (const SvgValue *value0,
                            const SvgValue *value1,
                            ComputeContext *context,
                            int             n)
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

static SvgValue *
svg_content_fit_new_none (void)
{
  static SvgContentFit none = { { &SVG_CONTENT_FIT_CLASS, 0 }, 1, 0, 0, 0 };

  return (SvgValue *) &none;
}

static SvgValue *
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

static gboolean
parse_coord_align (const char *value,
                   Align      *align)
{
  if (strncmp (value, "Min", 3) == 0)
    *align = ALIGN_MIN;
  else if (strncmp (value, "Mid", 3) == 0)
    *align = ALIGN_MID;
  else if (strncmp (value, "Max", 3) == 0)
    *align = ALIGN_MAX;
  else
    return FALSE;

  return TRUE;
}

static gboolean
parse_align (const char *value,
             Align      *align_x,
             Align      *align_y)
{
  if (strlen (value) != 8)
    return FALSE;

  if (value[0] != 'x' || value[4] != 'Y')
    return FALSE;

  if (!parse_coord_align (value + 1, align_x) ||
      !parse_coord_align (value + 5, align_y))
    return FALSE;

  return TRUE;
}

static gboolean
parse_meet (const char  *value,
            MeetOrSlice *meet)
{
  if (match_str (value, "meet"))
    *meet = MEET;
  else if (match_str (value, "slice"))
    *meet = SLICE;
  else
    return FALSE;

  return TRUE;
}

static SvgValue *
svg_content_fit_parse (const char *value)
{
  GStrv strv;
  Align align_x;
  Align align_y;
  MeetOrSlice meet;

  if (match_str (value, "none"))
    return svg_content_fit_new_none ();

  strv = g_strsplit (value, " ", 0);
  if (g_strv_length (strv) > 2)
    {
      g_strfreev (strv);
      return NULL;
    }

  if (!parse_align (strv[0], &align_x, &align_y))
    {
      g_strfreev (strv);
      return NULL;
    }

  if (strv[1])
    {
      if (!parse_meet (strv[1], &meet))
        {
          g_strfreev (strv);
          return NULL;
        }
    }
  else
    {
      meet = MEET;
    }

  g_strfreev (strv);

  return svg_content_fit_new (align_x, align_y, meet);
}

/* }}} */
/* {{{ Orient */

typedef enum {
  ORIENT_AUTO,
  ORIENT_ANGLE,
} OrientKind;

typedef struct
{
  SvgValue base;
  OrientKind kind;
  gboolean start_reverse;
  double angle;
  SvgUnit unit;
} SvgOrient;

static gboolean
svg_orient_equal (const SvgValue *value0,
                  const SvgValue *value1)
{
  const SvgOrient *v0 = (const SvgOrient *) value0;
  const SvgOrient *v1 = (const SvgOrient *) value1;

  if (v0->kind != v1->kind)
    return FALSE;

  if (v0->kind == ORIENT_AUTO)
    return v0->start_reverse == v1->start_reverse;
  else
    return v0->angle == v1->angle &&
           v0->unit == v1->unit;
}

static SvgValue *svg_orient_new_angle (double  angle,
                                       SvgUnit unit);

static SvgValue *
svg_orient_interpolate (const SvgValue *value0,
                        const SvgValue *value1,
                        ComputeContext *context,
                        double          t)
{
  const SvgOrient *v0 = (const SvgOrient *) value0;
  const SvgOrient *v1 = (const SvgOrient *) value1;

  if (v0->kind == v1->kind &&
      v0->kind == ORIENT_ANGLE &&
      v0->unit == v1->unit)
    return svg_orient_new_angle (lerp (v0->angle, v1->angle, t), v0->unit);

  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_orient_accumulate (const SvgValue *value0,
                       const SvgValue *value1,
                       ComputeContext *context,
                       int             n)
{
  return NULL;
}

static void
svg_orient_print (const SvgValue *value,
                  GString        *string)
{
  const SvgOrient *v = (const SvgOrient *) value;

  if (v->kind == ORIENT_ANGLE)
    {
      string_append_double (string, "", v->angle);
      g_string_append (string, unit_names[v->unit]);
    }
  else if (v->start_reverse)
    {
      g_string_append (string, "auto-start-reverse");
    }
  else
    {
      g_string_append (string, "auto");
    }
}

static SvgValue * svg_orient_resolve (const SvgValue  *value,
                                      ShapeAttr        attr,
                                      unsigned int     idx,
                                      Shape           *shape,
                                      ComputeContext  *context);

static const SvgValueClass SVG_ORIENT_CLASS = {
  "SvgOrient",
  svg_value_default_free,
  svg_orient_equal,
  svg_orient_interpolate,
  svg_orient_accumulate,
  svg_orient_print,
  svg_value_default_distance,
  svg_orient_resolve,
};

static SvgValue *
svg_orient_new_angle (double  angle,
                      SvgUnit unit)
{
  static SvgOrient def = { { &SVG_ORIENT_CLASS, 0 }, .kind = ORIENT_ANGLE, .angle = 0, .unit = SVG_UNIT_NUMBER };
  SvgOrient *v;

  if (angle == 0 && unit == SVG_UNIT_NUMBER)
    return (SvgValue *) &def;

  v = (SvgOrient *) svg_value_alloc (&SVG_ORIENT_CLASS, sizeof (SvgOrient));

  v->kind = ORIENT_ANGLE;
  v->angle = angle;
  v->unit = unit;

  return (SvgValue *) v;
}

static SvgValue *
svg_orient_new_auto (gboolean start_reverse)
{
  SvgOrient *v;

  v = (SvgOrient *) svg_value_alloc (&SVG_ORIENT_CLASS, sizeof (SvgOrient));

  v->kind = ORIENT_AUTO;
  v->start_reverse = start_reverse;

  return (SvgValue *) v;
}

static SvgValue *
svg_orient_parse (const char *value)
{
  if (match_str (value, "auto"))
    return svg_orient_new_auto (FALSE);
  else if (match_str (value, "auto-start-reverse"))
    return svg_orient_new_auto (TRUE);
  else
    {
      double f;
      SvgUnit unit;

      if (!parse_numeric (value, -DBL_MAX, DBL_MAX, NUMBER|ANGLE, &f, &unit))
        return NULL;

      return svg_orient_new_angle (f, unit);
    }
}

static SvgValue *
svg_orient_resolve (const SvgValue  *value,
                    ShapeAttr        attr,
                    unsigned int     idx,
                    Shape           *shape,
                    ComputeContext  *context)
{
  const SvgOrient *v = (const SvgOrient *) value;

  if (v->kind == ORIENT_ANGLE)
    {
      switch ((unsigned int) v->unit)
        {
        case SVG_UNIT_NUMBER:
          return svg_value_ref ((SvgValue *) value);
        case SVG_UNIT_RAD:
          return svg_orient_new_angle (v->angle * 180.0 / M_PI, SVG_UNIT_NUMBER);
        case SVG_UNIT_DEG:
          return svg_orient_new_angle (v->angle, SVG_UNIT_NUMBER);
        case SVG_UNIT_GRAD:
          return svg_orient_new_angle (v->angle * 360.0 / 400.0, SVG_UNIT_NUMBER);
        case SVG_UNIT_TURN:
          return svg_orient_new_angle (v->angle * 360.0, SVG_UNIT_NUMBER);
        default:
          g_assert_not_reached ();
        }
    }
  else
    return svg_value_ref ((SvgValue *) value);
}

/* }}} */
/* {{{ Language */

typedef struct
{
  SvgValue base;
  unsigned int len;
  PangoLanguage *values[1];
} SvgLanguage;

static unsigned int
svg_language_size (unsigned int n)
{
  return sizeof (SvgLanguage) + (n > 0 ? n - 1 : 0) * sizeof (PangoLanguage *);
}

static gboolean
svg_language_equal (const SvgValue *value0,
                    const SvgValue *value1)
{
  const SvgLanguage *l0 = (const SvgLanguage *)value0;
  const SvgLanguage *l1 = (const SvgLanguage *)value1;

  if (l0->len != l1->len)
    return FALSE;

  for (unsigned int i = 0; i < l0->len; i++)
    {
      if (l0->values[i] != l1->values[i])
        return FALSE;
    }

  return TRUE;
}

static SvgValue *
svg_language_interpolate (const SvgValue *value0,
                          const SvgValue *value1,
                          ComputeContext *context,
                          double          t)
{
  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_language_accumulate (const SvgValue *value0,
                         const SvgValue *value1,
                         ComputeContext *context,
                         int             n)
{
  return svg_value_ref ((SvgValue *)value0);
}

static void
svg_language_print (const SvgValue *value,
                    GString        *string)
{
  const SvgLanguage *l = (const SvgLanguage *)value;

  for (unsigned int i = 0; i < l->len; i++)
    {
      if (i > 0)
        g_string_append_c (string, ' ');
      g_string_append (string, pango_language_to_string (l->values[i]));
    }
}

static PangoLanguage *
svg_language_get (const SvgValue *value)
{
  const SvgLanguage *l = (const SvgLanguage *)value;

  return l->values[0];
}

static const SvgValueClass SVG_LANGUAGE_CLASS = {
  "SvgLanguage",
  svg_value_default_free,
  svg_language_equal,
  svg_language_interpolate,
  svg_language_accumulate,
  svg_language_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

static SvgValue *
svg_language_new_list (unsigned int    len,
                       PangoLanguage **langs)
{
  static SvgLanguage empty = { { &SVG_LANGUAGE_CLASS, 0 }, .len = 0, .values[0] = NULL, };
  SvgLanguage *result;

  if (len == 0)
    return (SvgValue *) &empty;

  result = (SvgLanguage *) svg_value_alloc (&SVG_LANGUAGE_CLASS,
                                            svg_language_size (len));

  result->len = len;

  for (unsigned int i = 0; i < len; i++)
    result->values[i] = langs[i];

  return (SvgValue *) result;
}

static SvgValue *
svg_language_new (PangoLanguage *language)
{
  return svg_language_new_list (1, &language);
}

static SvgValue *
svg_language_new_default (void)
{
  static SvgLanguage def = { { &SVG_LANGUAGE_CLASS, 0 }, .len = 1, .values[0] = NULL, };

  if (def.values[0] == NULL)
    def.values[0] = gtk_get_default_language ();

  return (SvgValue *) &def;
}

/* }}} */
/* {{{ Text decoration */

typedef enum {
  TEXT_DECORATION_NONE        = 0,
  TEXT_DECORATION_UNDERLINE   = 1 << 0,
  TEXT_DECORATION_OVERLINE    = 1 << 1,
  TEXT_DECORATION_LINE_TROUGH = 1 << 2,
} TextDecoration;

static const struct {
  const char *name;
  TextDecoration value;
} text_decorations[] = {
  { .name = "underline", .value = TEXT_DECORATION_UNDERLINE },
  { .name = "overline", .value = TEXT_DECORATION_OVERLINE },
  { .name = "line-through", .value = TEXT_DECORATION_LINE_TROUGH },
};

typedef struct
{
  SvgValue base;
  TextDecoration value;
} SvgTextDecoration;

static gboolean
svg_text_decoration_equal (const SvgValue *value0,
                           const SvgValue *value1)
{
  const SvgTextDecoration *d0 = (const SvgTextDecoration *)value0;
  const SvgTextDecoration *d1 = (const SvgTextDecoration *)value1;

  return d0->value == d1->value;
}

static SvgValue *
svg_text_decoration_interpolate (const SvgValue *value0,
                                 const SvgValue *value1,
                                 ComputeContext *context,
                                 double          t)
{
  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_text_decoration_accumulate (const SvgValue *value0,
                                const SvgValue *value1,
                                ComputeContext *context,
                                int             n)
{
  return svg_value_ref ((SvgValue *)value0);
}

static void
svg_text_decoration_print (const SvgValue *value,
                           GString        *string)
{
  const SvgTextDecoration *d = (const SvgTextDecoration *)value;
  gboolean has_value = FALSE;
  for (size_t i = 0; i < G_N_ELEMENTS (text_decorations); i++)
    if (d->value & text_decorations[i].value)
      {
        if (has_value)
          g_string_append_c (string, ' ');
        g_string_append (string, text_decorations[i].name);
        has_value = TRUE;
      }
}

static TextDecoration
svg_text_decoration_get (const SvgValue *value)
{
  const SvgTextDecoration *d = (const SvgTextDecoration *)value;
  return d->value;
}

static SvgValue * svg_text_decoration_resolve (const SvgValue *value,
                                               ShapeAttr       attr,
                                               unsigned int    idx,
                                               Shape          *shape,
                                               ComputeContext *context);

static const SvgValueClass SVG_TEXT_DECORATION_CLASS = {
  "SvgTextDecoration",
  svg_value_default_free,
  svg_text_decoration_equal,
  svg_text_decoration_interpolate,
  svg_text_decoration_accumulate,
  svg_text_decoration_print,
  svg_value_default_distance,
  svg_text_decoration_resolve,
};

static SvgValue *
svg_text_decoration_new (TextDecoration decoration)
{
  static SvgTextDecoration none = { { &SVG_TEXT_DECORATION_CLASS, 0 }, .value = TEXT_DECORATION_NONE };
  SvgTextDecoration *result;

  if (decoration == TEXT_DECORATION_NONE)
    return (SvgValue *) &none;

  result = (SvgTextDecoration *) svg_value_alloc (&SVG_TEXT_DECORATION_CLASS, sizeof (SvgTextDecoration));
  result->value = decoration;
  return (SvgValue *) result;
}

static SvgValue *
svg_text_decoration_parse (const char *text)
{
  TextDecoration val = TEXT_DECORATION_NONE;
  char **decorations = g_strsplit (text, " ", 0);
  for (size_t i = 0; decorations[i]; i++)
    for (size_t j = 0; j < G_N_ELEMENTS (text_decorations); j++)
      if (g_strcmp0 (decorations[i], text_decorations[j].name) == 0)
        val |= text_decorations[j].value;
  g_strfreev (decorations);
  return svg_text_decoration_new (val);
}

static SvgValue *
svg_text_decoration_resolve (const SvgValue *value,
                             ShapeAttr       attr,
                             unsigned int    idx,
                             Shape          *shape,
                             ComputeContext *context)
{
  TextDecoration ret;

  if (!context->parent)
    return svg_value_ref ((SvgValue *)value);

  ret = svg_text_decoration_get (context->parent->current[attr]);
  if (ret == TEXT_DECORATION_NONE)
    return svg_value_ref ((SvgValue *)value);

  ret |= svg_text_decoration_get (value);

  return svg_text_decoration_new (ret);
}
/* }}} */
/* {{{ References */

/* The difference between HREF_REF and HREF_URL is about
 * the accepted syntax: plain fragment vs url()
 */

typedef enum
{
  HREF_NONE,
  HREF_REF,
  HREF_URL,
} HrefKind;

typedef struct
{
  SvgValue base;
  HrefKind kind;

  char *ref;
  Shape *shape;
  GdkTexture *texture;
} SvgHref;

static void
svg_href_free (SvgValue *value)
{
  const SvgHref *r = (const SvgHref *) value;

  if (r->kind != HREF_NONE)
    g_free (r->ref);

  g_free (value);
}

static gboolean
svg_href_equal (const SvgValue *value1,
                const SvgValue *value2)
{
  const SvgHref *r1 = (const SvgHref *) value1;
  const SvgHref *r2 = (const SvgHref *) value2;

  if (r1->kind != r2->kind)
    return FALSE;

  switch (r1->kind)
    {
    case HREF_NONE:
      return TRUE;
    case HREF_REF:
    case HREF_URL:
      return r1->shape == r2->shape &&
             r1->texture == r2->texture &&
             g_strcmp0 (r1->ref, r2->ref) == 0;
    default:
      g_assert_not_reached ();
    }
}

static SvgValue *
svg_href_interpolate (const SvgValue *value1,
                      const SvgValue *value2,
                      ComputeContext *context,
                      double          t)
{
  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value1);
  else
    return svg_value_ref ((SvgValue *) value2);
}

static SvgValue *
svg_href_accumulate (const SvgValue *value1,
                     const SvgValue *value2,
                     ComputeContext *context,
                     int             n)
{
  return NULL;
}

static void
svg_href_print (const SvgValue *value,
                GString        *string)
{
  const SvgHref *r = (const SvgHref *) value;

  switch (r->kind)
    {
    case HREF_NONE:
      g_string_append (string, "none");
      break;
    case HREF_REF:
      g_string_append_printf (string, "%s", r->ref);
      break;
    case HREF_URL:
      g_string_append_printf (string, "url(%s)", r->ref);
      break;
    default:
      g_assert_not_reached ();
    }
}

static const SvgValueClass SVG_HREF_CLASS = {
  "SvgHref",
  svg_href_free,
  svg_href_equal,
  svg_href_interpolate,
  svg_href_accumulate,
  svg_href_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

static SvgValue *
svg_href_new_none (void)
{
  static SvgHref none = { { &SVG_HREF_CLASS, 0 }, HREF_NONE };
  return (SvgValue *) &none;
}

static SvgValue *
svg_href_new_ref (const char *ref)
{
  SvgHref *result;

  result = (SvgHref *) svg_value_alloc (&SVG_HREF_CLASS, sizeof (SvgHref));
  result->kind = HREF_REF;
  result->ref = g_strdup (ref);
  result->shape = NULL;

  return (SvgValue *) result;
}

static SvgValue *
svg_href_new_url (const char *ref)
{
  SvgHref *result;

  result = (SvgHref *) svg_value_alloc (&SVG_HREF_CLASS, sizeof (SvgHref));
  result->kind = HREF_URL;
  result->ref = g_strdup (ref);
  result->shape = NULL;

  return (SvgValue *) result;
}

static SvgValue *
svg_href_parse (const char *value)
{
  if (match_str (value, "none"))
    return svg_href_new_none ();
  else
    return svg_href_new_ref (value);
}

static SvgValue *
svg_href_parse_url (const char *value)
{
  if (match_str (value, "none"))
    {
      return svg_href_new_none ();
    }
  else
    {
      GtkCssParser *parser;
      GBytes *bytes;
      char *url;
      SvgValue *res = NULL;

      bytes = g_bytes_new_static (value, strlen (value));
      parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL);

      url = gtk_css_parser_consume_url (parser);
      if (url)
        res = svg_href_new_url (url);

      g_free (url);
      gtk_css_parser_unref (parser);
      g_bytes_unref (bytes);

      return res;
    }
}

static const char *
svg_href_get_id (SvgHref *href)
{
  g_assert (((SvgValue *) href)->class == &SVG_HREF_CLASS);

  if (href->kind == HREF_NONE)
    return NULL;

  if (href->ref[0] == '#')
    return href->ref + 1;
  else
    return href->ref;
}

/* }}} */
/* {{{ Color stops */

typedef struct
{
  unsigned int attrs;
  SvgValue *base[N_STOP_ATTRS];
  SvgValue *current[N_STOP_ATTRS];
} ColorStop;

static void
color_stop_free (gpointer v)
{
  ColorStop *stop = v;

  for (unsigned int i = 0; i < N_STOP_ATTRS; i++)
    {
      g_clear_pointer (&stop->base[i], svg_value_unref);
      g_clear_pointer (&stop->current[i], svg_value_unref);
    }
  g_free (stop);
}

static inline unsigned int
color_stop_attr_idx (ShapeAttr attr)
{
  g_assert (attr >= FIRST_STOP_ATTR && attr <= LAST_STOP_ATTR);
  return attr - FIRST_STOP_ATTR;
}

static ColorStop *
color_stop_new (void)
{
  ColorStop *stop = g_new0 (ColorStop, 1);

  for (ShapeAttr attr = FIRST_STOP_ATTR; attr <= LAST_STOP_ATTR; attr++)
    stop->base[color_stop_attr_idx (attr)] = shape_attr_ref_initial_value (attr, SHAPE_LINEAR_GRADIENT, TRUE);

  return stop;
}

/* }}} */
/* {{{ Filters */

/* We flatten the tree between feMerge/feMergeNode and
 * feComponentTransfer/feFuncR... to avoid problems
 * with attribute addressing. The only places where
 * this matters are apply_filter_tree and serialize_filter.
 */

typedef enum
{
  FE_FLOOD,
  FE_BLUR,
  FE_BLEND,
  FE_COLOR_MATRIX,
  FE_COMPOSITE,
  FE_OFFSET,
  FE_DISPLACEMENT,
  FE_TILE,
  FE_IMAGE,
  FE_MERGE,
  FE_MERGE_NODE,
  FE_COMPONENT_TRANSFER,
  FE_FUNC_R,
  FE_FUNC_G,
  FE_FUNC_B,
  FE_FUNC_A,
  FE_DROPSHADOW,
} FilterPrimitiveType;

static ShapeAttr flood_attrs[] = {
  SHAPE_ATTR_FE_X, SHAPE_ATTR_FE_Y, SHAPE_ATTR_FE_WIDTH, SHAPE_ATTR_FE_HEIGHT, SHAPE_ATTR_FE_RESULT,
  SHAPE_ATTR_FE_COLOR, SHAPE_ATTR_FE_OPACITY,
};

static ShapeAttr blur_attrs[] = {
  SHAPE_ATTR_FE_X, SHAPE_ATTR_FE_Y, SHAPE_ATTR_FE_WIDTH, SHAPE_ATTR_FE_HEIGHT, SHAPE_ATTR_FE_RESULT,
  SHAPE_ATTR_FE_IN, SHAPE_ATTR_FE_STD_DEV, SHAPE_ATTR_FE_BLUR_EDGE_MODE,
};

static ShapeAttr blend_attrs[] = {
  SHAPE_ATTR_FE_X, SHAPE_ATTR_FE_Y, SHAPE_ATTR_FE_WIDTH, SHAPE_ATTR_FE_HEIGHT, SHAPE_ATTR_FE_RESULT,
  SHAPE_ATTR_FE_IN, SHAPE_ATTR_FE_IN2, SHAPE_ATTR_FE_BLEND_MODE,
  SHAPE_ATTR_FE_BLEND_COMPOSITE,
};

static ShapeAttr color_matrix_attrs[] = {
  SHAPE_ATTR_FE_X, SHAPE_ATTR_FE_Y, SHAPE_ATTR_FE_WIDTH, SHAPE_ATTR_FE_HEIGHT, SHAPE_ATTR_FE_RESULT,
  SHAPE_ATTR_FE_IN, SHAPE_ATTR_FE_COLOR_MATRIX_TYPE, SHAPE_ATTR_FE_COLOR_MATRIX_VALUES,
};

static ShapeAttr composite_attrs[] = {
  SHAPE_ATTR_FE_X, SHAPE_ATTR_FE_Y, SHAPE_ATTR_FE_WIDTH, SHAPE_ATTR_FE_HEIGHT, SHAPE_ATTR_FE_RESULT,
  SHAPE_ATTR_FE_IN, SHAPE_ATTR_FE_IN2, SHAPE_ATTR_FE_COMPOSITE_OPERATOR,
  SHAPE_ATTR_FE_COMPOSITE_K1, SHAPE_ATTR_FE_COMPOSITE_K2, SHAPE_ATTR_FE_COMPOSITE_K3, SHAPE_ATTR_FE_COMPOSITE_K4,
};

static ShapeAttr offset_attrs[] = {
  SHAPE_ATTR_FE_X, SHAPE_ATTR_FE_Y, SHAPE_ATTR_FE_WIDTH, SHAPE_ATTR_FE_HEIGHT, SHAPE_ATTR_FE_RESULT,
  SHAPE_ATTR_FE_IN, SHAPE_ATTR_FE_DX, SHAPE_ATTR_FE_DY,
};

static ShapeAttr displacement_attrs[] = {
  SHAPE_ATTR_FE_X, SHAPE_ATTR_FE_Y, SHAPE_ATTR_FE_WIDTH, SHAPE_ATTR_FE_HEIGHT, SHAPE_ATTR_FE_RESULT,
  SHAPE_ATTR_FE_IN, SHAPE_ATTR_FE_IN2, SHAPE_ATTR_FE_DISPLACEMENT_SCALE, SHAPE_ATTR_FE_DISPLACEMENT_X, SHAPE_ATTR_FE_DISPLACEMENT_Y,
};

static ShapeAttr tile_attrs[] = {
  SHAPE_ATTR_FE_X, SHAPE_ATTR_FE_Y, SHAPE_ATTR_FE_WIDTH, SHAPE_ATTR_FE_HEIGHT, SHAPE_ATTR_FE_RESULT,
  SHAPE_ATTR_FE_IN,
};

static ShapeAttr image_attrs[] = {
  SHAPE_ATTR_FE_X, SHAPE_ATTR_FE_Y, SHAPE_ATTR_FE_WIDTH, SHAPE_ATTR_FE_HEIGHT, SHAPE_ATTR_FE_RESULT,
  SHAPE_ATTR_FE_IMAGE_HREF, SHAPE_ATTR_FE_IMAGE_CONTENT_FIT,
};

static ShapeAttr merge_attrs[] = {
  SHAPE_ATTR_FE_X, SHAPE_ATTR_FE_Y, SHAPE_ATTR_FE_WIDTH, SHAPE_ATTR_FE_HEIGHT, SHAPE_ATTR_FE_RESULT,
};

static ShapeAttr merge_node_attrs[] = {
  SHAPE_ATTR_FE_IN,
};

static ShapeAttr component_transfer_attrs[] = {
  SHAPE_ATTR_FE_X, SHAPE_ATTR_FE_Y, SHAPE_ATTR_FE_WIDTH, SHAPE_ATTR_FE_HEIGHT, SHAPE_ATTR_FE_RESULT,
  SHAPE_ATTR_FE_IN,
};

static ShapeAttr func_attrs[] = {
  SHAPE_ATTR_FE_FUNC_TYPE, SHAPE_ATTR_FE_FUNC_VALUES, SHAPE_ATTR_FE_FUNC_SLOPE,
  SHAPE_ATTR_FE_FUNC_INTERCEPT, SHAPE_ATTR_FE_FUNC_AMPLITUDE, SHAPE_ATTR_FE_FUNC_EXPONENT,
  SHAPE_ATTR_FE_FUNC_OFFSET,
};

static ShapeAttr dropshadow_attrs[] = {
  SHAPE_ATTR_FE_X, SHAPE_ATTR_FE_Y, SHAPE_ATTR_FE_WIDTH, SHAPE_ATTR_FE_HEIGHT, SHAPE_ATTR_FE_RESULT,
  SHAPE_ATTR_FE_IN, SHAPE_ATTR_FE_STD_DEV, SHAPE_ATTR_FE_DX, SHAPE_ATTR_FE_DY,
  SHAPE_ATTR_FE_COLOR, SHAPE_ATTR_FE_OPACITY,
};

typedef struct
{
  const char *name;
  unsigned int n_attrs;
  ShapeAttr *attrs;
} FilterTypeInfo;

static FilterTypeInfo filter_types[] = {
  [FE_FLOOD] = {
    .name = "feFlood",
    .n_attrs = G_N_ELEMENTS (flood_attrs),
    .attrs = flood_attrs,
  },
  [FE_BLUR] = {
    .name = "feGaussianBlur",
    .n_attrs = G_N_ELEMENTS (blur_attrs),
    .attrs = blur_attrs,
  },
  [FE_BLEND] = {
    .name = "feBlend",
    .n_attrs = G_N_ELEMENTS (blend_attrs),
    .attrs = blend_attrs,
  },
  [FE_COLOR_MATRIX] = {
    .name = "feColorMatrix",
    .n_attrs = G_N_ELEMENTS (color_matrix_attrs),
    .attrs = color_matrix_attrs,
  },
  [FE_COMPOSITE] = {
    .name = "feComposite",
    .n_attrs = G_N_ELEMENTS (composite_attrs),
    .attrs = composite_attrs,
  },
  [FE_OFFSET] = {
    .name = "feOffset",
    .n_attrs = G_N_ELEMENTS (offset_attrs),
    .attrs = offset_attrs,
  },
  [FE_DISPLACEMENT] = {
    .name = "feDisplacementMap",
    .n_attrs = G_N_ELEMENTS (displacement_attrs),
    .attrs = displacement_attrs,
  },
  [FE_TILE] = {
    .name = "feTile",
    .n_attrs = G_N_ELEMENTS (tile_attrs),
    .attrs = tile_attrs,
  },
  [FE_IMAGE] = {
    .name = "feImage",
    .n_attrs = G_N_ELEMENTS (image_attrs),
    .attrs = image_attrs,
  },
  [FE_MERGE] = {
    .name = "feMerge",
    .n_attrs = G_N_ELEMENTS (merge_attrs),
    .attrs = merge_attrs,
  },
  [FE_MERGE_NODE] = {
    .name = "feMergeNode",
    .n_attrs = G_N_ELEMENTS (merge_node_attrs),
    .attrs = merge_node_attrs,
  },
  [FE_COMPONENT_TRANSFER] = {
    .name = "feComponentTransfer",
    .n_attrs = G_N_ELEMENTS (component_transfer_attrs),
    .attrs = component_transfer_attrs,
  },
  [FE_FUNC_R] = {
    .name = "feFuncR",
    .n_attrs = G_N_ELEMENTS (func_attrs),
    .attrs = func_attrs,
  },
  [FE_FUNC_G] = {
    .name = "feFuncG",
    .n_attrs = G_N_ELEMENTS (func_attrs),
    .attrs = func_attrs,
  },
  [FE_FUNC_B] = {
    .name = "feFuncB",
    .n_attrs = G_N_ELEMENTS (func_attrs),
    .attrs = func_attrs,
  },
  [FE_FUNC_A] = {
    .name = "feFuncA",
    .n_attrs = G_N_ELEMENTS (func_attrs),
    .attrs = func_attrs,
  },
  [FE_DROPSHADOW] = {
    .name = "feDropShadow",
    .n_attrs = G_N_ELEMENTS (dropshadow_attrs),
    .attrs = dropshadow_attrs,
  },
};

static guint
filter_type_hash (gconstpointer v)
{
  const FilterTypeInfo *t = (const FilterTypeInfo *) v;

  return g_str_hash (t->name);
}

static gboolean
filter_type_equal (gconstpointer v0,
                   gconstpointer v1)
{
  const FilterTypeInfo *t0 = (const FilterTypeInfo *) v0;
  const FilterTypeInfo *t1 = (const FilterTypeInfo *) v1;

  return strcmp (t0->name, t1->name) == 0;
}

static GHashTable *filter_type_lookup_table;

static void
filter_types_init (void)
{
  filter_type_lookup_table = g_hash_table_new (filter_type_hash,
                                               filter_type_equal);

  for (unsigned int i = 0; i < G_N_ELEMENTS (filter_types); i++)
    g_hash_table_add (filter_type_lookup_table, &filter_types[i]);
}

static gboolean
filter_type_lookup (const char          *name,
                    FilterPrimitiveType *type)
{
  FilterTypeInfo key;
  FilterTypeInfo *value;

  key.name = name;
  value = g_hash_table_lookup (filter_type_lookup_table, &key);
  if (!value)
    return FALSE;

  *type = value - filter_types;
  return TRUE;
}

typedef struct
{
  FilterPrimitiveType type;
  unsigned int attrs;
  SvgValue **current;
  SvgValue *base[1];
} FilterPrimitive;

static void
filter_primitive_free (gpointer v)
{
  FilterPrimitive *f = v;

  for (unsigned int i = 0; i < filter_types[f->type].n_attrs; i++)
    {
      g_clear_pointer (&f->base[i], svg_value_unref);
      g_clear_pointer (&f->current[i], svg_value_unref);
    }
  g_free (f);
}

static unsigned int
filter_primitive_get_n_attrs (FilterPrimitiveType type)
{
  return filter_types[type].n_attrs;
}

static ShapeAttr
filter_primitive_get_shape_attr (FilterPrimitiveType type,
                                 unsigned int        attr)
{
  return filter_types[type].attrs[attr];
}

static int
filter_attr_index (FilterPrimitiveType type,
                   ShapeAttr           attr)
{
  if (attr == 0)
    return -1;

  for (int i = 0; i < filter_types[type].n_attrs; i++)
    {
      if (attr == filter_types[type].attrs[i])
        return i;
    }

  return -1;
}

static unsigned int
filter_attr_idx (FilterPrimitiveType type,
                 ShapeAttr           attr)
{
  for (unsigned int i = 0; i < filter_types[type].n_attrs; i++)
    {
      if (filter_types[type].attrs[i] == attr)
        return i;
    }

  g_error ("Attempt to get the index of shape attribute %u in a %s filter",
           attr, filter_types[type].name);
  return 0;
}

static SvgValue *
filter_attr_ref_initial_value (FilterPrimitive *filter,
                               ShapeAttr        attr)
{
  if (filter->type == FE_COLOR_MATRIX && attr == SHAPE_ATTR_FE_COLOR_MATRIX_VALUES)
    {
      switch (svg_enum_get (filter->base[filter_attr_idx (filter->type, SHAPE_ATTR_FE_COLOR_MATRIX_TYPE)]))
        {
        case COLOR_MATRIX_TYPE_MATRIX: return svg_numbers_new_identity_matrix ();
        case COLOR_MATRIX_TYPE_SATURATE: return svg_numbers_new1 (1);
        case COLOR_MATRIX_TYPE_HUE_ROTATE: return svg_numbers_new1 (0);
        case COLOR_MATRIX_TYPE_LUMINANCE_TO_ALPHA: return svg_numbers_new_none ();
        default: g_assert_not_reached ();
        }
    }

  if (filter->type == FE_DROPSHADOW && attr == SHAPE_ATTR_FE_STD_DEV)
    return svg_number_new (2);

  if (filter->type == FE_DROPSHADOW &&
      (attr == SHAPE_ATTR_FE_DX || attr == SHAPE_ATTR_FE_DY))
    return svg_number_new (2);

  return shape_attr_ref_initial_value (attr, SHAPE_FILTER, TRUE);
}

static SvgValue *
filter_get_current_value (FilterPrimitive *filter,
                          ShapeAttr        attr)
{
  return filter->current[filter_attr_idx (filter->type, attr)];
}

static GskComponentTransfer *
filter_get_component_transfer (FilterPrimitive *filter)
{
  ComponentTransferType type;

  g_assert (filter->type == FE_FUNC_R ||
            filter->type == FE_FUNC_G ||
            filter->type == FE_FUNC_B ||
            filter->type == FE_FUNC_A);

  type = svg_enum_get (filter->current[filter_attr_idx (filter->type, SHAPE_ATTR_FE_FUNC_TYPE)]);
  switch (type)
    {
    case COMPONENT_TRANSFER_IDENTITY:
      return gsk_component_transfer_new_identity ();
    case COMPONENT_TRANSFER_TABLE:
    case COMPONENT_TRANSFER_DISCRETE:
      {
        SvgNumbers *numbers = (SvgNumbers *) filter->current [filter_attr_idx (filter->type, SHAPE_ATTR_FE_FUNC_VALUES)];
        float *values = g_newa (float, numbers->n_values);

        if (numbers->n_values == 0)
          return gsk_component_transfer_new_identity ();

        for (unsigned int i = 0; i < numbers->n_values; i++)
          values[i] = numbers->values[i].value;

        if (type == COMPONENT_TRANSFER_TABLE)
          return gsk_component_transfer_new_table (numbers->n_values, values);
        else
          return gsk_component_transfer_new_discrete (numbers->n_values, values);
      }
    case COMPONENT_TRANSFER_LINEAR:
      {
        double slope = svg_number_get (filter->current[filter_attr_idx (filter->type, SHAPE_ATTR_FE_FUNC_SLOPE)], 1);
        double intercept = svg_number_get (filter->current[filter_attr_idx (filter->type, SHAPE_ATTR_FE_FUNC_INTERCEPT)], 1);
        return gsk_component_transfer_new_linear (slope, intercept);
      }
    case COMPONENT_TRANSFER_GAMMA:
      {
        double amplitude = svg_number_get (filter->current[filter_attr_idx (filter->type, SHAPE_ATTR_FE_FUNC_AMPLITUDE)], 1);
        double exponent = svg_number_get (filter->current[filter_attr_idx (filter->type, SHAPE_ATTR_FE_FUNC_EXPONENT)], 1);
        double offset = svg_number_get (filter->current[filter_attr_idx (filter->type, SHAPE_ATTR_FE_FUNC_OFFSET)], 1);
        return gsk_component_transfer_new_gamma (amplitude, exponent, offset);
      }

    default:
      g_assert_not_reached ();
    }
}

static gboolean
filter_primitive_needs_backdrop (FilterPrimitive *f)
{
  ShapeAttr attr[] = { SHAPE_ATTR_FE_IN, SHAPE_ATTR_FE_IN2 };

  for (unsigned int i = 0; i < G_N_ELEMENTS (attr); i++)
    {
      int idx = filter_attr_index (f->type, attr[i]);

      if (idx != -1)
        {
          SvgFilterPrimitiveRef *ref = (SvgFilterPrimitiveRef *) f->current[idx];

          if (ref->type == BACKGROUND_IMAGE || ref->type == BACKGROUND_ALPHA)
            return TRUE;
        }
    }

  return FALSE;
}

static gboolean
filter_needs_backdrop (Shape *filter)
{
  for (unsigned int i = 0; i < filter->filters->len; i++)
    {
      FilterPrimitive *f = g_ptr_array_index (filter->filters, i);

      if (filter_primitive_needs_backdrop (f))
        return TRUE;
    }

  return FALSE;
}

static FilterPrimitive *
filter_primitive_new (FilterPrimitiveType type)
{
  FilterTypeInfo *ft = &filter_types[type];
  FilterPrimitive *f;

  f = g_malloc0 (sizeof (FilterPrimitive) + sizeof (SvgValue *) * (2 * ft->n_attrs - 1));

  f->type = type;
  f->current = f->base + ft->n_attrs;

  for (unsigned int i = 0; i < ft->n_attrs; i++)
    f->base[i] = filter_attr_ref_initial_value (f, ft->attrs[i]);

  return f;
}

/* }}} */
/* {{{ Attributes */

static SvgValue *
parse_language (const char *value)
{
  PangoLanguage *lang = pango_language_from_string (value);
  if (!lang)
    return NULL;
  return svg_language_new (lang);
}

static SvgValue *
parse_language_list (const char *value)
{
  GStrv strv;
  unsigned int len;
  PangoLanguage **langs;

  strv = strsplit_set (value, ", ");
  len = g_strv_length (strv);

  langs = g_newa (PangoLanguage *, len);
  for (unsigned int i = 0; i < len; i++)
    {
      langs[i] = pango_language_from_string (strv[i]);
      if (!langs[i])
        {
          g_strfreev (strv);
          return NULL;
        }
    }
  g_strfreev (strv);

  return svg_language_new_list (len, langs);
}

static SvgValue *
parse_string_list (const char *value)
{
  GStrv strv;
  SvgValue *result;

  strv = strsplit_set (value, " ");
  result = svg_string_list_new (strv);
  g_strfreev (strv);

  return result;
}

static SvgValue *
parse_opacity (const char *value)
{
  return svg_number_parse (value, -DBL_MAX, DBL_MAX, NUMBER|PERCENTAGE);
}

static SvgValue *
parse_stroke_width (const char *value)
{
  return svg_number_parse (value, 0, DBL_MAX, NUMBER|LENGTH|PERCENTAGE);
}

static SvgValue *
parse_miterlimit (const char *value)
{
  return svg_number_parse (value, 0, DBL_MAX, NUMBER);
}

static SvgValue *
parse_any_length (const char *value)
{
  return svg_number_parse (value, -DBL_MAX, DBL_MAX, NUMBER|LENGTH);
}

static SvgValue *
parse_positive_length (const char *value)
{
  return svg_number_parse (value, 0, DBL_MAX, NUMBER|LENGTH);
}

static SvgValue *
parse_length_percentage (const char *value)
{
  return svg_number_parse (value, -DBL_MAX, DBL_MAX, NUMBER|PERCENTAGE|LENGTH);
}

static SvgValue *
parse_positive_length_percentage (const char *value)
{
  return svg_number_parse (value, 0, DBL_MAX, NUMBER|PERCENTAGE|LENGTH);
}

static SvgValue *
parse_any_number (const char *value)
{
  return svg_number_parse (value, -DBL_MAX, DBL_MAX, NUMBER);
}

static SvgValue *
parse_font_weight (const char *value)
{
  SvgValue *v;

  v = svg_font_weight_parse (value);
  if (v)
    return v;

  return svg_number_parse (value, 1, 1000, NUMBER);
}

static SvgValue *
parse_font_size (const char *value)
{
  SvgValue *v;

  v = svg_font_size_parse (value);
  if (v)
    return v;

  return svg_number_parse (value, 0, DBL_MAX, NUMBER|PERCENTAGE|LENGTH);
}

static SvgValue *
parse_letter_spacing (const char *value)
{
  if (match_str (value, "normal"))
    return svg_number_new (0.);
  return svg_number_parse (value, -DBL_MAX, DBL_MAX, NUMBER|LENGTH);
}

static SvgValue *
parse_offset (const char *value)
{
  return svg_number_parse (value, -DBL_MAX, DBL_MAX, NUMBER|PERCENTAGE);
}

static SvgValue *
parse_ref_x (const char *value)
{
  if (match_str (value, "left"))
    return svg_percentage_new (0);
  else if (match_str (value, "center"))
    return svg_percentage_new (50);
  else if (match_str (value, "right"))
    return svg_percentage_new (100);
  else
    return svg_number_parse (value, -DBL_MAX, DBL_MAX, NUMBER|PERCENTAGE|LENGTH);
}

static SvgValue *
parse_ref_y (const char *value)
{
  if (match_str (value, "top"))
    return svg_percentage_new (0);
  else if (match_str (value, "center"))
    return svg_percentage_new (50);
  else if (match_str (value, "bottom"))
    return svg_percentage_new (100);
  else
    return svg_number_parse (value, -DBL_MAX, DBL_MAX, NUMBER|PERCENTAGE|LENGTH);
}

static SvgValue *
parse_positive_length_percentage_or_auto (const char *value)
{
  if (match_str (value, "auto"))
    return svg_auto_new ();
  else
    return parse_positive_length_percentage (value);
}

static SvgValue *
parse_number_optional_number (const char *value)
{
  SvgNumbers *numbers = (SvgNumbers *) svg_numbers_parse (value);

  if (numbers->n_values <= 2)
    {
      return (SvgValue *) numbers;
    }
  else
    {
      svg_value_unref ((SvgValue *) numbers);
      return NULL;
    }
}

static SvgValue *
parse_transform_origin (const char *value)
{
  GStrv strv;
  unsigned int n;
  SvgNumbers *p;
  const char *h_values[] = { "left", "center", "right" };
  const char *v_values[] = { "top", "center", "bottom" };

  strv = strsplit_set (value, " ");
  n = g_strv_length (strv);

  if (n > 2)
    {
      g_strfreev (strv);
      return NULL;
    }

  p = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (2));
  p->n_values = 2;

  if (n == 1)
    {
      unsigned int j;

      p->values[0].value = 50;
      p->values[0].unit = SVG_UNIT_PERCENTAGE;
      p->values[1].value = 50;
      p->values[1].unit = SVG_UNIT_PERCENTAGE;

      if (parse_enum (strv[0], h_values, G_N_ELEMENTS (h_values), &j))
        p->values[0].value = j * 50;
      else if (parse_enum (strv[0], v_values, G_N_ELEMENTS (v_values), &j))
        p->values[1].value = j * 50;
      else if (!parse_numeric (strv[0], -DBL_MAX, DBL_MAX, NUMBER|LENGTH|PERCENTAGE,
                               &p->values[0].value, &p->values[0].unit))
        g_clear_pointer ((gpointer *)&p, svg_value_unref);
    }
  else
    {
      unsigned int j;

      if (parse_enum (strv[0], h_values, G_N_ELEMENTS (h_values), &j))
        {
          p->values[0].value = j * 50;
          p->values[0].unit = SVG_UNIT_PERCENTAGE;
        }
      else if (!parse_numeric (strv[0], -DBL_MAX, DBL_MAX, NUMBER|LENGTH|PERCENTAGE,
                               &p->values[0].value, &p->values[0].unit))
        g_clear_pointer ((gpointer *)&p, svg_value_unref);

      if (p)
        {
          if (parse_enum (strv[1], v_values, G_N_ELEMENTS (v_values), &j))
            {
              p->values[1].value = j * 50;
              p->values[1].unit = SVG_UNIT_PERCENTAGE;
            }
          else if (!parse_numeric (strv[1], -DBL_MAX, DBL_MAX, NUMBER|LENGTH|PERCENTAGE,
                                   &p->values[1].value, &p->values[1].unit))
            g_clear_pointer ((gpointer *)&p, svg_value_unref);
        }
    }
  g_strfreev (strv);

  return (SvgValue *) p;
}

typedef enum
{
  SHAPE_ATTR_NONE      = 0,
  SHAPE_ATTR_INHERITED = 1 << 0,
  SHAPE_ATTR_DISCRETE  = 1 << 1,
  SHAPE_ATTR_NO_CSS    = 1 << 2,
  SHAPE_ATTR_ONLY_CSS  = 1 << 3,
} ShapeAttrFlags;

typedef struct
{
  ShapeAttrFlags flags;
  unsigned int applies_to;
  SvgValue * (* parse_value)      (const char *value);
  SvgValue * (* parse_for_values) (const char *value);
  SvgValue *initial_value;
} ShapeAttribute;

#define SHAPE_ANY \
  (BIT (SHAPE_CIRCLE) | BIT (SHAPE_ELLIPSE) | BIT (SHAPE_IMAGE) | \
   BIT (SHAPE_LINE) | BIT (SHAPE_PATH) | BIT (SHAPE_POLYGON) | \
   BIT (SHAPE_POLYLINE) | BIT (SHAPE_RECT) | BIT (SHAPE_TEXT) | \
   BIT (SHAPE_TSPAN) | \
   BIT (SHAPE_CLIP_PATH) | BIT (SHAPE_DEFS) | BIT (SHAPE_GROUP) | \
   BIT (SHAPE_MARKER) | BIT (SHAPE_MASK) | BIT (SHAPE_PATTERN) | \
   BIT (SHAPE_SVG) | BIT (SHAPE_SYMBOL) | \
   BIT (SHAPE_LINEAR_GRADIENT) | BIT (SHAPE_RADIAL_GRADIENT) | \
   BIT (SHAPE_FILTER) | BIT (SHAPE_USE) | BIT (SHAPE_SWITCH))

#define SHAPE_SHAPES \
  (BIT (SHAPE_CIRCLE) | BIT (SHAPE_ELLIPSE) | BIT (SHAPE_LINE) | \
   BIT (SHAPE_PATH) | BIT (SHAPE_POLYGON) | BIT (SHAPE_POLYLINE) | \
   BIT (SHAPE_RECT))

#define SHAPE_TEXTS (BIT (SHAPE_TEXT) | BIT (SHAPE_TSPAN))

#define SHAPE_GRAPHICS (SHAPE_SHAPES | SHAPE_TEXTS | BIT (SHAPE_IMAGE))

#define SHAPE_GRAPHICS_REF (BIT (SHAPE_USE) | BIT (SHAPE_IMAGE))

#define SHAPE_CONTAINERS \
  (BIT (SHAPE_CLIP_PATH) | BIT (SHAPE_DEFS) | BIT (SHAPE_GROUP) | \
   BIT (SHAPE_MARKER) | BIT (SHAPE_MASK) | BIT (SHAPE_PATTERN) | \
   BIT (SHAPE_SVG) | BIT (SHAPE_SYMBOL) | BIT (SHAPE_SWITCH))

#define SHAPE_NEVER_RENDERED \
  (BIT (SHAPE_CLIP_PATH) | BIT (SHAPE_DEFS) | BIT (SHAPE_LINEAR_GRADIENT) | \
   BIT (SHAPE_MARKER) | BIT (SHAPE_MASK) | BIT (SHAPE_PATTERN) | \
   BIT (SHAPE_RADIAL_GRADIENT) | BIT (SHAPE_SYMBOL))

#define SHAPE_RENDERABLE ((SHAPE_GRAPHICS | SHAPE_GRAPHICS_REF | SHAPE_CONTAINERS) & ~SHAPE_NEVER_RENDERED)

#define SHAPE_GRADIENTS \
  (BIT (SHAPE_LINEAR_GRADIENT) | BIT (SHAPE_RADIAL_GRADIENT))

#define SHAPE_PAINT_SERVERS (SHAPE_GRADIENTS | BIT (SHAPE_PATTERN))

#define SHAPE_VIEWPORTS (BIT (SHAPE_SVG) | BIT (SHAPE_SYMBOL))

#define CLIP_PATH_CONTENT (SHAPE_SHAPES | SHAPE_TEXTS)

static ShapeAttribute shape_attrs[] = {
  [SHAPE_ATTR_DISPLAY] = {
    .flags = SHAPE_ATTR_DISCRETE,
    .applies_to = SHAPE_ANY,
    .parse_value = svg_display_parse,
  },
  [SHAPE_ATTR_FONT_SIZE] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_ANY,
    .parse_value = parse_font_size,
  },
  [SHAPE_ATTR_VISIBILITY] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE,
    .applies_to = SHAPE_ANY,
    .parse_value = svg_visibility_parse,
  },
  [SHAPE_ATTR_TRANSFORM] = {
    .applies_to = (SHAPE_PAINT_SERVERS | BIT (SHAPE_CLIP_PATH) | SHAPE_RENDERABLE) & ~BIT (SHAPE_TSPAN),
    .parse_value = svg_transform_parse,
  },
  [SHAPE_ATTR_TRANSFORM_ORIGIN] = {
    .applies_to = (SHAPE_PAINT_SERVERS | BIT (SHAPE_CLIP_PATH) | SHAPE_RENDERABLE) & ~BIT (SHAPE_TSPAN),
    .parse_value = parse_transform_origin,
  },
  [SHAPE_ATTR_TRANSFORM_BOX] = {
    .applies_to = (SHAPE_PAINT_SERVERS | BIT (SHAPE_CLIP_PATH) | SHAPE_RENDERABLE) & ~BIT (SHAPE_TSPAN),
    .parse_value = svg_transform_box_parse,
  },
  [SHAPE_ATTR_OPACITY] = {
    .applies_to = SHAPE_ANY,
    .parse_value = parse_opacity,
  },
  [SHAPE_ATTR_COLOR] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_ANY,
    .parse_value = svg_color_parse,
  },
  [SHAPE_ATTR_COLOR_INTERPOLATION] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_CONTAINERS | SHAPE_GRAPHICS | SHAPE_GRADIENTS | BIT (SHAPE_USE),
    .parse_value = svg_color_interpolation_parse,
  },
  [SHAPE_ATTR_COLOR_INTERPOLATION_FILTERS] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = svg_color_interpolation_parse,
  },
  [SHAPE_ATTR_CLIP_PATH] = {
    .flags = SHAPE_ATTR_DISCRETE,
    .applies_to = (SHAPE_CONTAINERS & ~BIT (SHAPE_DEFS)) | SHAPE_GRAPHICS | BIT (SHAPE_USE),
    .parse_value = svg_clip_parse,
  },
  [SHAPE_ATTR_CLIP_RULE] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE,
    .applies_to =SHAPE_GRAPHICS,
    .parse_value = svg_fill_rule_parse,
  },
  [SHAPE_ATTR_MASK] = {
    .flags = SHAPE_ATTR_DISCRETE,
    .applies_to = (SHAPE_CONTAINERS & ~BIT (SHAPE_DEFS)) | SHAPE_GRAPHICS | BIT (SHAPE_USE),
    .parse_value = svg_mask_parse,
  },
  [SHAPE_ATTR_FONT_FAMILY] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE,
    .applies_to = SHAPE_ANY,
    .parse_value = svg_string_new,
  },
  [SHAPE_ATTR_FONT_STYLE] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_ANY,
    .parse_value = svg_font_style_parse,
  },
  [SHAPE_ATTR_FONT_VARIANT] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE,
    .applies_to = SHAPE_ANY,
    .parse_value = svg_font_variant_parse,
  },
  [SHAPE_ATTR_FONT_WEIGHT] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_ANY,
    .parse_value = parse_font_weight,
  },
  [SHAPE_ATTR_FONT_STRETCH] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_ANY,
    .parse_value = svg_font_stretch_parse,
  },
  [SHAPE_ATTR_FILTER] = {
    .applies_to = (SHAPE_CONTAINERS & ~BIT (SHAPE_DEFS)) | SHAPE_GRAPHICS | BIT (SHAPE_USE),
    .parse_value = svg_filter_parse,
  },
  [SHAPE_ATTR_FILL] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_SHAPES | SHAPE_TEXTS,
    .parse_value = svg_paint_parse,
  },
  [SHAPE_ATTR_FILL_OPACITY] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_SHAPES | SHAPE_TEXTS,
    .parse_value = parse_opacity,
  },
  [SHAPE_ATTR_FILL_RULE] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE,
    .applies_to = SHAPE_SHAPES | SHAPE_TEXTS,
    .parse_value = svg_fill_rule_parse,
  },
  [SHAPE_ATTR_STROKE] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_SHAPES | SHAPE_TEXTS,
    .parse_value = svg_paint_parse,
  },
  [SHAPE_ATTR_STROKE_OPACITY] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_SHAPES | SHAPE_TEXTS,
    .parse_value = parse_opacity,
  },
  [SHAPE_ATTR_STROKE_WIDTH] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_SHAPES | SHAPE_TEXTS,
    .parse_value = parse_stroke_width,
  },
  [SHAPE_ATTR_STROKE_LINECAP] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE,
    .applies_to = SHAPE_SHAPES | SHAPE_TEXTS,
    .parse_value = svg_linecap_parse,
  },
  [SHAPE_ATTR_STROKE_LINEJOIN] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE,
    .applies_to = SHAPE_SHAPES | SHAPE_TEXTS,
    .parse_value = svg_linejoin_parse,
  },
  [SHAPE_ATTR_STROKE_MITERLIMIT] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_SHAPES | SHAPE_TEXTS,
    .parse_value = parse_miterlimit,
  },
  [SHAPE_ATTR_STROKE_DASHARRAY] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_SHAPES | SHAPE_TEXTS,
    .parse_value = svg_dash_array_parse,
  },
  [SHAPE_ATTR_STROKE_DASHOFFSET] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_SHAPES | SHAPE_TEXTS,
    .parse_value = parse_length_percentage,
  },
  [SHAPE_ATTR_MARKER_START] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_SHAPES,
    .parse_value = svg_href_parse_url,
  },
  [SHAPE_ATTR_MARKER_MID] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_SHAPES,
    .parse_value = svg_href_parse_url,
  },
  [SHAPE_ATTR_MARKER_END] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_SHAPES,
    .parse_value = svg_href_parse_url,
  },
  [SHAPE_ATTR_PAINT_ORDER] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE,
    .applies_to = SHAPE_SHAPES | SHAPE_TEXTS,
    .parse_value = svg_paint_order_parse,
  },
  [SHAPE_ATTR_SHAPE_RENDERING] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE,
    .applies_to = SHAPE_SHAPES,
    .parse_value = svg_shape_rendering_parse,
  },
  [SHAPE_ATTR_TEXT_RENDERING] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE,
    .applies_to = SHAPE_TEXTS,
    .parse_value = svg_text_rendering_parse,
  },
  [SHAPE_ATTR_IMAGE_RENDERING] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE,
    .applies_to = BIT (SHAPE_IMAGE),
    .parse_value = svg_image_rendering_parse,
  },
  [SHAPE_ATTR_BLEND_MODE] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_ONLY_CSS,
    .applies_to = SHAPE_CONTAINERS |SHAPE_GRAPHICS | SHAPE_GRAPHICS_REF,
    .parse_value = svg_blend_mode_parse,
  },
  [SHAPE_ATTR_ISOLATION] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_ONLY_CSS,
    .applies_to = SHAPE_CONTAINERS |SHAPE_GRAPHICS | SHAPE_GRAPHICS_REF,
    .parse_value = svg_isolation_parse,
  },
  [SHAPE_ATTR_PATH_LENGTH] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = SHAPE_SHAPES,
    .parse_value = parse_positive_length,
    .parse_for_values = parse_any_length,
  },
  [SHAPE_ATTR_HREF] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = SHAPE_GRAPHICS_REF | SHAPE_PAINT_SERVERS,
    .parse_value = svg_href_parse,
  },
  [SHAPE_ATTR_OVERFLOW] = {
    .flags = SHAPE_ATTR_DISCRETE,
    .applies_to = SHAPE_ANY,
    .parse_value = svg_overflow_parse,
  },
  [SHAPE_ATTR_VECTOR_EFFECT] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = SHAPE_GRAPHICS | BIT (SHAPE_USE),
    .parse_value = svg_vector_effect_parse,
  },
  [SHAPE_ATTR_PATH] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_PATH),
    .parse_value = svg_path_parse,
  },
  [SHAPE_ATTR_CX] = {
    .applies_to = BIT (SHAPE_CIRCLE) | BIT (SHAPE_ELLIPSE) | BIT (SHAPE_RADIAL_GRADIENT),
    .parse_value = parse_length_percentage,
  },
  [SHAPE_ATTR_CY] = {
    .applies_to = BIT (SHAPE_CIRCLE) | BIT (SHAPE_ELLIPSE) | BIT (SHAPE_RADIAL_GRADIENT),
    .parse_value = parse_length_percentage,
  },
  [SHAPE_ATTR_R] = {
    .applies_to = BIT (SHAPE_CIRCLE) | BIT (SHAPE_RADIAL_GRADIENT),
    .parse_value = parse_positive_length_percentage,
    .parse_for_values = parse_length_percentage,
  },
  [SHAPE_ATTR_X] = {
    .applies_to = BIT (SHAPE_SVG) | BIT (SHAPE_RECT) | BIT (SHAPE_IMAGE) |
              BIT (SHAPE_USE) | BIT (SHAPE_FILTER) | BIT (SHAPE_PATTERN) |
              BIT (SHAPE_MASK) | SHAPE_TEXTS,
    .parse_value = parse_length_percentage,
  },
  [SHAPE_ATTR_Y] = {
    .applies_to = BIT (SHAPE_SVG) | BIT (SHAPE_RECT) | BIT (SHAPE_IMAGE) |
              BIT (SHAPE_USE) | BIT (SHAPE_FILTER) | BIT (SHAPE_PATTERN) |
              BIT (SHAPE_MASK) | SHAPE_TEXTS,
    .parse_value = parse_length_percentage,
  },
  [SHAPE_ATTR_WIDTH] = {
    .applies_to = BIT (SHAPE_SVG) | BIT (SHAPE_RECT) | BIT (SHAPE_IMAGE) |
              BIT (SHAPE_USE) | BIT (SHAPE_FILTER) | BIT (SHAPE_MARKER) |
              BIT (SHAPE_MASK) | BIT (SHAPE_PATTERN),
    .parse_value = parse_positive_length_percentage_or_auto,
    .parse_for_values = parse_length_percentage,
  },
  [SHAPE_ATTR_HEIGHT] = {
    .applies_to = BIT (SHAPE_SVG) | BIT (SHAPE_RECT) | BIT (SHAPE_IMAGE) |
              BIT (SHAPE_USE) | BIT (SHAPE_FILTER) | BIT (SHAPE_MARKER) |
              BIT (SHAPE_MASK) | BIT (SHAPE_PATTERN),
    .parse_value = parse_positive_length_percentage_or_auto,
    .parse_for_values = parse_length_percentage,
  },
  [SHAPE_ATTR_RX] = {
    .applies_to = BIT (SHAPE_RECT) | BIT (SHAPE_ELLIPSE) | BIT (SHAPE_RADIAL_GRADIENT),
    .parse_value = parse_positive_length_percentage_or_auto,
    .parse_for_values = parse_length_percentage,
  },
  [SHAPE_ATTR_RY] = {
    .applies_to = BIT (SHAPE_RECT) | BIT (SHAPE_ELLIPSE) | BIT (SHAPE_RADIAL_GRADIENT),
    .parse_value = parse_positive_length_percentage_or_auto,
    .parse_for_values = parse_length_percentage,
  },
  [SHAPE_ATTR_X1] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_LINE) | BIT (SHAPE_LINEAR_GRADIENT),
    .parse_value = parse_length_percentage,
  },
  [SHAPE_ATTR_Y1] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_LINE) | BIT (SHAPE_LINEAR_GRADIENT),
    .parse_value = parse_length_percentage,
  },
  [SHAPE_ATTR_X2] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_LINE) | BIT (SHAPE_LINEAR_GRADIENT),
    .parse_value = parse_length_percentage,
  },
  [SHAPE_ATTR_Y2] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_LINE) | BIT (SHAPE_LINEAR_GRADIENT),
    .parse_value = parse_length_percentage,
  },
  [SHAPE_ATTR_POINTS] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_POLYGON) | BIT (SHAPE_POLYLINE),
    .parse_value = svg_points_parse,
  },
  [SHAPE_ATTR_SPREAD_METHOD] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = SHAPE_GRADIENTS,
    .parse_value = svg_spread_method_parse,
  },
  [SHAPE_ATTR_CONTENT_UNITS] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = SHAPE_PAINT_SERVERS | BIT (SHAPE_CLIP_PATH) |
                  BIT (SHAPE_MASK) | BIT (SHAPE_FILTER),
    .parse_value = svg_coord_units_parse,
  },
  [SHAPE_ATTR_BOUND_UNITS] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_PATTERN) | BIT (SHAPE_MASK) | BIT (SHAPE_FILTER),
    .parse_value = svg_coord_units_parse,
  },
  [SHAPE_ATTR_FX] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_RADIAL_GRADIENT),
    .parse_value = parse_length_percentage,
  },
  [SHAPE_ATTR_FY] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_RADIAL_GRADIENT),
    .parse_value = parse_length_percentage,
  },
  [SHAPE_ATTR_FR] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_RADIAL_GRADIENT),
    .parse_value = parse_positive_length_percentage,
    .parse_for_values = parse_length_percentage,
  },
  [SHAPE_ATTR_MASK_TYPE] = {
    .flags = SHAPE_ATTR_DISCRETE,
    .applies_to = BIT (SHAPE_MASK),
    .parse_value = svg_mask_type_parse,
  },
  [SHAPE_ATTR_VIEW_BOX] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = SHAPE_VIEWPORTS | BIT (SHAPE_PATTERN) | BIT (SHAPE_MARKER),
    .parse_value = svg_view_box_parse,
  },
  [SHAPE_ATTR_CONTENT_FIT] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = SHAPE_VIEWPORTS | BIT (SHAPE_PATTERN) | BIT (SHAPE_MARKER) | BIT (SHAPE_IMAGE),
    .parse_value = svg_content_fit_parse,
  },
  [SHAPE_ATTR_REF_X] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_MARKER),
    .parse_value = parse_ref_x,
  },
  [SHAPE_ATTR_REF_Y] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_MARKER),
    .parse_value = parse_ref_y,
  },
  [SHAPE_ATTR_MARKER_UNITS] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_MARKER),
    .parse_value = svg_marker_units_parse,
  },
  [SHAPE_ATTR_MARKER_ORIENT] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_MARKER),
    .parse_value = svg_orient_parse,
  },
  [SHAPE_ATTR_LANG] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = SHAPE_ANY,
    .parse_value = parse_language,
  },
  [SHAPE_ATTR_TEXT_ANCHOR] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE,
    .applies_to = SHAPE_TEXTS,
    .parse_value = svg_text_anchor_parse,
  },
  [SHAPE_ATTR_DX] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = SHAPE_TEXTS,
    .parse_value = parse_length_percentage,
  },
  [SHAPE_ATTR_DY] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = SHAPE_TEXTS,
    .parse_value = parse_length_percentage,
  },
  [SHAPE_ATTR_UNICODE_BIDI] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE,
    .applies_to = SHAPE_TEXTS,
    .parse_value = svg_unicode_bidi_parse,
  },
  [SHAPE_ATTR_DIRECTION] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE,
    .applies_to = SHAPE_TEXTS,
    .parse_value = svg_direction_parse,
  },
  [SHAPE_ATTR_WRITING_MODE] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE,
    .applies_to = SHAPE_TEXTS,
    .parse_value = svg_writing_mode_parse,
  },
  [SHAPE_ATTR_LETTER_SPACING] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE,
    .applies_to = SHAPE_TEXTS,
    .parse_value = parse_letter_spacing,
  },
  [SHAPE_ATTR_TEXT_DECORATION] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_DISCRETE,
    .applies_to = SHAPE_TEXTS,
    .parse_value = svg_text_decoration_parse,
  },
  [SHAPE_ATTR_REQUIRED_EXTENSIONS] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = SHAPE_ANY,
    .parse_value = parse_string_list,
  },
  [SHAPE_ATTR_SYSTEM_LANGUAGE] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = SHAPE_ANY,
    .parse_value = parse_language_list,
  },
  [SHAPE_ATTR_STROKE_MINWIDTH] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_NO_CSS,
    .applies_to = SHAPE_SHAPES,
    .parse_value = parse_stroke_width,
  },
  [SHAPE_ATTR_STROKE_MAXWIDTH] = {
    .flags = SHAPE_ATTR_INHERITED | SHAPE_ATTR_NO_CSS,
    .applies_to = SHAPE_SHAPES,
    .parse_value = parse_stroke_width,
  },
  [SHAPE_ATTR_STOP_OFFSET] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = SHAPE_GRADIENTS,
    .parse_value = parse_offset,
  },
  [SHAPE_ATTR_STOP_COLOR] = {
    .applies_to = SHAPE_GRADIENTS,
    .parse_value = svg_color_parse,
  },
  [SHAPE_ATTR_STOP_OPACITY] = {
    .applies_to = SHAPE_GRADIENTS,
    .parse_value = parse_opacity,
  },
  [SHAPE_ATTR_FE_X] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_length_percentage,
  },
  [SHAPE_ATTR_FE_Y] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_length_percentage,
  },
  [SHAPE_ATTR_FE_WIDTH] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_positive_length_percentage,
    .parse_for_values = parse_length_percentage,
  },
  [SHAPE_ATTR_FE_HEIGHT] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_positive_length_percentage,
    .parse_for_values = parse_length_percentage,
  },
  [SHAPE_ATTR_FE_RESULT] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = svg_string_new,
  },
  [SHAPE_ATTR_FE_COLOR] = {
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = svg_color_parse,
  },
  [SHAPE_ATTR_FE_OPACITY] = {
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_opacity,
  },
  [SHAPE_ATTR_FE_IN] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = svg_filter_primitive_ref_parse,
  },
  [SHAPE_ATTR_FE_IN2] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = svg_filter_primitive_ref_parse,
  },
  [SHAPE_ATTR_FE_STD_DEV] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_number_optional_number,
  },
  [SHAPE_ATTR_FE_DX] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_any_length,
  },
  [SHAPE_ATTR_FE_DY] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_any_length,
  },
  [SHAPE_ATTR_FE_BLUR_EDGE_MODE] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = svg_edge_mode_parse,
  },
  [SHAPE_ATTR_FE_BLEND_MODE] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = svg_blend_mode_parse,
  },
  [SHAPE_ATTR_FE_BLEND_COMPOSITE] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = svg_blend_composite_parse,
  },
  [SHAPE_ATTR_FE_COLOR_MATRIX_TYPE] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = svg_color_matrix_type_parse,
  },
  [SHAPE_ATTR_FE_COLOR_MATRIX_VALUES] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = svg_numbers_parse,
  },
  [SHAPE_ATTR_FE_COMPOSITE_OPERATOR] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = svg_composite_operator_parse,
  },
  [SHAPE_ATTR_FE_COMPOSITE_K1] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_any_number,
  },
  [SHAPE_ATTR_FE_COMPOSITE_K2] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_any_number,
  },
  [SHAPE_ATTR_FE_COMPOSITE_K3] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_any_number,
  },
  [SHAPE_ATTR_FE_COMPOSITE_K4] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_any_number,
  },
  [SHAPE_ATTR_FE_DISPLACEMENT_SCALE] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_any_number,
  },
  [SHAPE_ATTR_FE_DISPLACEMENT_X] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = svg_rgba_channel_parse,
  },
  [SHAPE_ATTR_FE_DISPLACEMENT_Y] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = svg_rgba_channel_parse,
  },
  [SHAPE_ATTR_FE_IMAGE_HREF] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = svg_href_parse,
  },
  [SHAPE_ATTR_FE_IMAGE_CONTENT_FIT] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = svg_content_fit_parse,
  },
  [SHAPE_ATTR_FE_FUNC_TYPE] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = svg_component_transfer_type_parse,
  },
  [SHAPE_ATTR_FE_FUNC_VALUES] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = svg_numbers_parse,
  },
  [SHAPE_ATTR_FE_FUNC_SLOPE] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_any_number,
  },
  [SHAPE_ATTR_FE_FUNC_INTERCEPT] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_any_number,
  },
  [SHAPE_ATTR_FE_FUNC_AMPLITUDE] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_any_number,
  },
  [SHAPE_ATTR_FE_FUNC_EXPONENT] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_any_number,
  },
  [SHAPE_ATTR_FE_FUNC_OFFSET] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_any_number,
  },
};

static inline gboolean
shape_attr_is_inherited (ShapeAttr attr)
{
  return (shape_attrs[attr].flags & SHAPE_ATTR_INHERITED) != 0;
}

static inline gboolean
shape_attr_is_discrete (ShapeAttr attr)
{
  return (shape_attrs[attr].flags & SHAPE_ATTR_DISCRETE) != 0;
}

static inline gboolean
shape_attr_has_css (ShapeAttr attr)
{
  return (shape_attrs[attr].flags & SHAPE_ATTR_NO_CSS) == 0;
}

static inline gboolean
shape_attr_only_css (ShapeAttr attr)
{
  return (shape_attrs[attr].flags & SHAPE_ATTR_ONLY_CSS) != 0;
}

static gboolean
shape_has_attr (ShapeType type,
                ShapeAttr attr)
{
  return shape_attr_is_inherited (attr) ||
         (shape_attrs[attr].applies_to & BIT (type)) != 0;
}

static void
shape_attrs_init_default_values (void)
{
  shape_attrs[SHAPE_ATTR_LANG].initial_value = svg_language_new_default ();
  shape_attrs[SHAPE_ATTR_DISPLAY].initial_value = svg_display_new (DISPLAY_INLINE);
  shape_attrs[SHAPE_ATTR_VISIBILITY].initial_value = svg_visibility_new (VISIBILITY_VISIBLE);
  shape_attrs[SHAPE_ATTR_FONT_SIZE].initial_value = svg_font_size_new (FONT_SIZE_MEDIUM);
  shape_attrs[SHAPE_ATTR_TRANSFORM].initial_value = svg_transform_new_none ();
  shape_attrs[SHAPE_ATTR_TRANSFORM_ORIGIN].initial_value = svg_numbers_new_00 ();
 shape_attrs[SHAPE_ATTR_TRANSFORM_BOX].initial_value = svg_transform_box_new (TRANSFORM_BOX_VIEW_BOX);
  shape_attrs[SHAPE_ATTR_OPACITY].initial_value = svg_number_new (1);
  shape_attrs[SHAPE_ATTR_COLOR].initial_value = svg_color_new_black ();
  shape_attrs[SHAPE_ATTR_COLOR_INTERPOLATION].initial_value = svg_color_interpolation_new (COLOR_INTERPOLATION_SRGB);
  shape_attrs[SHAPE_ATTR_COLOR_INTERPOLATION_FILTERS].initial_value = svg_color_interpolation_new (COLOR_INTERPOLATION_LINEAR);
  shape_attrs[SHAPE_ATTR_OVERFLOW].initial_value = svg_overflow_new (OVERFLOW_VISIBLE);
  shape_attrs[SHAPE_ATTR_VECTOR_EFFECT].initial_value = svg_vector_effect_new (VECTOR_EFFECT_NONE);
  shape_attrs[SHAPE_ATTR_FILTER].initial_value = svg_filter_new_none ();
  shape_attrs[SHAPE_ATTR_CLIP_PATH].initial_value = svg_clip_new_none ();
  shape_attrs[SHAPE_ATTR_CLIP_RULE].initial_value = svg_fill_rule_new (GSK_FILL_RULE_WINDING);
  shape_attrs[SHAPE_ATTR_MASK].initial_value = svg_mask_new_none ();
  shape_attrs[SHAPE_ATTR_FONT_FAMILY].initial_value = svg_string_new ("");
  shape_attrs[SHAPE_ATTR_FONT_STYLE].initial_value = svg_font_style_new (PANGO_STYLE_NORMAL);
  shape_attrs[SHAPE_ATTR_FONT_VARIANT].initial_value = svg_font_variant_new (PANGO_VARIANT_NORMAL);
  shape_attrs[SHAPE_ATTR_FONT_WEIGHT].initial_value = svg_font_weight_new (FONT_WEIGHT_NORMAL);
  shape_attrs[SHAPE_ATTR_FONT_STRETCH].initial_value = svg_font_stretch_new (PANGO_STRETCH_NORMAL);
  shape_attrs[SHAPE_ATTR_FILL].initial_value = svg_paint_new_black ();
  shape_attrs[SHAPE_ATTR_FILL_OPACITY].initial_value = svg_number_new (1);
  shape_attrs[SHAPE_ATTR_FILL_RULE].initial_value = svg_fill_rule_new (GSK_FILL_RULE_WINDING);
  shape_attrs[SHAPE_ATTR_STROKE].initial_value = svg_paint_new_none ();
  shape_attrs[SHAPE_ATTR_STROKE_OPACITY].initial_value = svg_number_new (1);
  shape_attrs[SHAPE_ATTR_STROKE_WIDTH].initial_value = svg_number_new (1);
  shape_attrs[SHAPE_ATTR_STROKE_LINECAP].initial_value = svg_linecap_new (GSK_LINE_CAP_BUTT);
  shape_attrs[SHAPE_ATTR_STROKE_LINEJOIN].initial_value = svg_linejoin_new (GSK_LINE_JOIN_MITER);
  shape_attrs[SHAPE_ATTR_STROKE_MITERLIMIT].initial_value = svg_number_new (4);
  shape_attrs[SHAPE_ATTR_STROKE_DASHARRAY].initial_value = svg_dash_array_new_none ();
  shape_attrs[SHAPE_ATTR_STROKE_DASHOFFSET].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_PAINT_ORDER].initial_value = svg_paint_order_new (PAINT_ORDER_FILL_STROKE_MARKERS);
  shape_attrs[SHAPE_ATTR_SHAPE_RENDERING].initial_value = svg_shape_rendering_new (SHAPE_RENDERING_AUTO);
  shape_attrs[SHAPE_ATTR_TEXT_RENDERING].initial_value = svg_text_rendering_new (TEXT_RENDERING_AUTO);
  shape_attrs[SHAPE_ATTR_IMAGE_RENDERING].initial_value = svg_image_rendering_new (IMAGE_RENDERING_AUTO);
  shape_attrs[SHAPE_ATTR_BLEND_MODE].initial_value = svg_blend_mode_new (GSK_BLEND_MODE_DEFAULT);
  shape_attrs[SHAPE_ATTR_ISOLATION].initial_value = svg_isolation_new (ISOLATION_AUTO);
  shape_attrs[SHAPE_ATTR_HREF].initial_value = svg_href_new_none ();
  shape_attrs[SHAPE_ATTR_PATH_LENGTH].initial_value = svg_number_new (-1);
  shape_attrs[SHAPE_ATTR_PATH].initial_value = svg_path_new_none ();
  shape_attrs[SHAPE_ATTR_CX].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_CY].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_R].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_X].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_Y].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_WIDTH].initial_value = svg_auto_new ();
  shape_attrs[SHAPE_ATTR_HEIGHT].initial_value = svg_auto_new ();
  shape_attrs[SHAPE_ATTR_RX].initial_value = svg_auto_new ();
  shape_attrs[SHAPE_ATTR_RY].initial_value = svg_auto_new ();
  shape_attrs[SHAPE_ATTR_X1].initial_value = svg_percentage_new (0);
  shape_attrs[SHAPE_ATTR_Y1].initial_value = svg_percentage_new (0);
  shape_attrs[SHAPE_ATTR_X2].initial_value = svg_percentage_new (100);
  shape_attrs[SHAPE_ATTR_Y2].initial_value = svg_percentage_new (0);
  shape_attrs[SHAPE_ATTR_FX].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_FY].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_FR].initial_value = svg_percentage_new (0);
  shape_attrs[SHAPE_ATTR_TEXT_ANCHOR].initial_value = svg_text_anchor_new (TEXT_ANCHOR_START);
  shape_attrs[SHAPE_ATTR_DX].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_DY].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_UNICODE_BIDI].initial_value = svg_unicode_bidi_new (UNICODE_BIDI_NORMAL);
  shape_attrs[SHAPE_ATTR_DIRECTION].initial_value = svg_direction_new (PANGO_DIRECTION_LTR);
  shape_attrs[SHAPE_ATTR_WRITING_MODE].initial_value = svg_writing_mode_new (WRITING_MODE_HORIZONTAL_TB);
  shape_attrs[SHAPE_ATTR_LETTER_SPACING].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_TEXT_DECORATION].initial_value = svg_text_decoration_new (TEXT_DECORATION_NONE);
  shape_attrs[SHAPE_ATTR_POINTS].initial_value = svg_numbers_new_none ();
  shape_attrs[SHAPE_ATTR_SPREAD_METHOD].initial_value = svg_spread_method_new (GSK_REPEAT_PAD);
  shape_attrs[SHAPE_ATTR_CONTENT_UNITS].initial_value = svg_coord_units_new (COORD_UNITS_OBJECT_BOUNDING_BOX);
  shape_attrs[SHAPE_ATTR_BOUND_UNITS].initial_value = svg_coord_units_new (COORD_UNITS_OBJECT_BOUNDING_BOX);
  shape_attrs[SHAPE_ATTR_MASK_TYPE].initial_value = svg_mask_type_new (GSK_MASK_MODE_LUMINANCE);
  shape_attrs[SHAPE_ATTR_VIEW_BOX].initial_value = svg_view_box_new_unset ();
  shape_attrs[SHAPE_ATTR_CONTENT_FIT].initial_value = svg_content_fit_new (ALIGN_MID, ALIGN_MID, MEET);
  shape_attrs[SHAPE_ATTR_REF_X].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_REF_Y].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_MARKER_UNITS].initial_value = svg_marker_units_new (MARKER_UNITS_STROKE_WIDTH);
  shape_attrs[SHAPE_ATTR_MARKER_ORIENT].initial_value = svg_orient_new_angle (0, SVG_UNIT_NUMBER);
  shape_attrs[SHAPE_ATTR_MARKER_START].initial_value = svg_href_new_none ();
  shape_attrs[SHAPE_ATTR_MARKER_MID].initial_value = svg_href_new_none ();
  shape_attrs[SHAPE_ATTR_MARKER_END].initial_value = svg_href_new_none ();
  shape_attrs[SHAPE_ATTR_REQUIRED_EXTENSIONS].initial_value = svg_string_list_new (NULL);
  shape_attrs[SHAPE_ATTR_SYSTEM_LANGUAGE].initial_value = svg_language_new_list (0, NULL);
  shape_attrs[SHAPE_ATTR_STROKE_MINWIDTH].initial_value = svg_percentage_new (25);
  shape_attrs[SHAPE_ATTR_STROKE_MAXWIDTH].initial_value = svg_percentage_new (150);
  shape_attrs[SHAPE_ATTR_STOP_OFFSET].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_STOP_COLOR].initial_value = svg_color_new_black ();
  shape_attrs[SHAPE_ATTR_STOP_OPACITY].initial_value = svg_number_new (1);
  shape_attrs[SHAPE_ATTR_FE_X].initial_value = svg_percentage_new (0);
  shape_attrs[SHAPE_ATTR_FE_Y].initial_value = svg_percentage_new (0);
  shape_attrs[SHAPE_ATTR_FE_WIDTH].initial_value = svg_percentage_new (100);
  shape_attrs[SHAPE_ATTR_FE_HEIGHT].initial_value = svg_percentage_new (100);
  shape_attrs[SHAPE_ATTR_FE_RESULT].initial_value = svg_string_new ("");
  shape_attrs[SHAPE_ATTR_FE_COLOR].initial_value = svg_color_new_black ();
  shape_attrs[SHAPE_ATTR_FE_OPACITY].initial_value = svg_number_new (1);
  shape_attrs[SHAPE_ATTR_FE_IN].initial_value = svg_filter_primitive_ref_new (DEFAULT_SOURCE);
  shape_attrs[SHAPE_ATTR_FE_IN2].initial_value = svg_filter_primitive_ref_new (DEFAULT_SOURCE);
  shape_attrs[SHAPE_ATTR_FE_STD_DEV].initial_value = svg_numbers_new1 (2);
  shape_attrs[SHAPE_ATTR_FE_BLUR_EDGE_MODE].initial_value = svg_edge_mode_new (EDGE_MODE_NONE);
  shape_attrs[SHAPE_ATTR_FE_BLEND_MODE].initial_value = svg_blend_mode_new (GSK_BLEND_MODE_DEFAULT);
  shape_attrs[SHAPE_ATTR_FE_BLEND_COMPOSITE].initial_value = svg_blend_composite_new (BLEND_COMPOSITE);
  shape_attrs[SHAPE_ATTR_FE_COLOR_MATRIX_TYPE].initial_value = svg_color_matrix_type_new (COLOR_MATRIX_TYPE_MATRIX);
  shape_attrs[SHAPE_ATTR_FE_COLOR_MATRIX_VALUES].initial_value = svg_numbers_new_identity_matrix ();
  shape_attrs[SHAPE_ATTR_FE_COMPOSITE_OPERATOR].initial_value = svg_composite_operator_new (COMPOSITE_OPERATOR_OVER);
  shape_attrs[SHAPE_ATTR_FE_COMPOSITE_K1].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_FE_COMPOSITE_K2].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_FE_COMPOSITE_K3].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_FE_COMPOSITE_K4].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_FE_DX].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_FE_DY].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_FE_DISPLACEMENT_SCALE].initial_value = svg_number_new (1);
  shape_attrs[SHAPE_ATTR_FE_DISPLACEMENT_X].initial_value = svg_rgba_channel_new (GDK_COLOR_CHANNEL_ALPHA);
  shape_attrs[SHAPE_ATTR_FE_DISPLACEMENT_Y].initial_value = svg_rgba_channel_new (GDK_COLOR_CHANNEL_ALPHA);
  shape_attrs[SHAPE_ATTR_FE_IMAGE_HREF].initial_value = svg_href_new_none ();
  shape_attrs[SHAPE_ATTR_FE_IMAGE_CONTENT_FIT].initial_value = svg_content_fit_new (ALIGN_MID, ALIGN_MID, MEET);
  shape_attrs[SHAPE_ATTR_FE_FUNC_TYPE].initial_value = svg_component_transfer_type_new (COMPONENT_TRANSFER_IDENTITY);
  shape_attrs[SHAPE_ATTR_FE_FUNC_VALUES].initial_value = svg_numbers_new_none ();
  shape_attrs[SHAPE_ATTR_FE_FUNC_SLOPE].initial_value = svg_number_new (1);
  shape_attrs[SHAPE_ATTR_FE_FUNC_INTERCEPT].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_FE_FUNC_AMPLITUDE].initial_value = svg_number_new (1);
  shape_attrs[SHAPE_ATTR_FE_FUNC_EXPONENT].initial_value = svg_number_new (1);
  shape_attrs[SHAPE_ATTR_FE_FUNC_OFFSET].initial_value = svg_number_new (0);
}

static SvgValue *
shape_attr_ref_initial_value (ShapeAttr attr,
                              ShapeType shape_type,
                              gboolean  has_parent)
{
  if (shape_type == SHAPE_RADIAL_GRADIENT &&
      (attr == SHAPE_ATTR_CX || attr == SHAPE_ATTR_CY || attr == SHAPE_ATTR_R))
    return svg_percentage_new (50);

  if (shape_type == SHAPE_LINE &&
      (attr == SHAPE_ATTR_X1 || attr == SHAPE_ATTR_Y1 ||
       attr == SHAPE_ATTR_X2 || attr == SHAPE_ATTR_Y2))
    return svg_number_new (0);

  if ((shape_type == SHAPE_CLIP_PATH || shape_type == SHAPE_MASK ||
       shape_type == SHAPE_PATTERN || shape_type == SHAPE_FILTER) &&
      attr == SHAPE_ATTR_CONTENT_UNITS)
    return svg_coord_units_new (COORD_UNITS_USER_SPACE_ON_USE);

  if (shape_type == SHAPE_MASK || shape_type == SHAPE_FILTER)
    {
      if (attr == SHAPE_ATTR_X || attr == SHAPE_ATTR_Y)
        return svg_percentage_new (-10);
      if (attr == SHAPE_ATTR_WIDTH || attr == SHAPE_ATTR_HEIGHT)
        return svg_percentage_new (120);
    }

  if (shape_type == SHAPE_MARKER || shape_type == SHAPE_PATTERN ||
      shape_type == SHAPE_IMAGE)
    {
      if (attr == SHAPE_ATTR_OVERFLOW)
        return svg_overflow_new (OVERFLOW_HIDDEN);
    }

  if (shape_type == SHAPE_MARKER)
    {
      if (attr == SHAPE_ATTR_WIDTH || attr == SHAPE_ATTR_HEIGHT)
        return svg_number_new (3);
    }

  if (shape_type == SHAPE_SVG || shape_type == SHAPE_SYMBOL)
    {
      if (attr == SHAPE_ATTR_WIDTH || attr == SHAPE_ATTR_HEIGHT)
        return svg_percentage_new (100);
      if (attr == SHAPE_ATTR_OVERFLOW && has_parent)
        return svg_overflow_new (OVERFLOW_HIDDEN);
    }

  if ((shape_type == SHAPE_CLIP_PATH || shape_type == SHAPE_MASK ||
       shape_type == SHAPE_DEFS || shape_type == SHAPE_MARKER ||
       shape_type == SHAPE_PATTERN || shape_type == SHAPE_LINEAR_GRADIENT ||
       shape_type == SHAPE_RADIAL_GRADIENT) &&
      attr == SHAPE_ATTR_DISPLAY)
    return svg_display_new (DISPLAY_NONE);

  return svg_value_ref (shape_attrs[attr].initial_value);
}

typedef struct {
  const char *name;
  unsigned int shapes;
  unsigned int filters;
  ShapeAttr attr;
} ShapeAttrLookup;

#define ANY_FILTER \
  (BIT (FE_FLOOD) | BIT (FE_BLUR) | BIT (FE_BLEND) | BIT (FE_COLOR_MATRIX) | \
   BIT (FE_COMPOSITE) | BIT (FE_OFFSET) | BIT (FE_DISPLACEMENT) | \
   BIT (FE_TILE) | BIT (FE_IMAGE) | BIT (FE_MERGE) | \
   BIT (FE_COMPONENT_TRANSFER) | BIT (FE_DROPSHADOW))

#define FILTER_FUNCS \
  (BIT (FE_FUNC_R) | BIT (FE_FUNC_G) | BIT (FE_FUNC_B) | BIT (FE_FUNC_A))

#define DEPRECATED_BIT 0x10000

static ShapeAttrLookup shape_attr_lookups[] = {
  { "display", SHAPE_ANY, 0, SHAPE_ATTR_DISPLAY },
  { "visibility", SHAPE_ANY, 0, SHAPE_ATTR_VISIBILITY },
  { "font-size", SHAPE_ANY, 0, SHAPE_ATTR_FONT_SIZE },
  { "transform", ((SHAPE_PAINT_SERVERS | BIT (SHAPE_CLIP_PATH) | SHAPE_RENDERABLE) & ~BIT (SHAPE_TSPAN)) & ~SHAPE_PAINT_SERVERS, 0, SHAPE_ATTR_TRANSFORM },
  { "gradientTransform", SHAPE_GRADIENTS, 0, SHAPE_ATTR_TRANSFORM },
  { "patternTransform", BIT (SHAPE_PATTERN), 0, SHAPE_ATTR_TRANSFORM },
  { "transform-origin", ((SHAPE_PAINT_SERVERS | BIT (SHAPE_CLIP_PATH) | SHAPE_RENDERABLE) & ~BIT (SHAPE_TSPAN)), 0, SHAPE_ATTR_TRANSFORM_ORIGIN },
  { "transform-box", ((SHAPE_PAINT_SERVERS | BIT (SHAPE_CLIP_PATH) | SHAPE_RENDERABLE) & ~BIT (SHAPE_TSPAN)), 0, SHAPE_ATTR_TRANSFORM_BOX },
  { "opacity", SHAPE_ANY, 0, SHAPE_ATTR_OPACITY },
  { "color", SHAPE_ANY, 0, SHAPE_ATTR_COLOR },
  { "color-interpolation", SHAPE_CONTAINERS | SHAPE_GRAPHICS | SHAPE_GRADIENTS | SHAPE_USE, 0, SHAPE_ATTR_COLOR_INTERPOLATION },
  { "color-interpolation-filters", BIT (SHAPE_FILTER), 0, SHAPE_ATTR_COLOR_INTERPOLATION_FILTERS },
  { "clip-path", (SHAPE_CONTAINERS & ~BIT (SHAPE_DEFS)) |SHAPE_GRAPHICS | BIT (SHAPE_USE), 0, SHAPE_ATTR_CLIP_PATH },
  { "clip-rule", SHAPE_ANY, 0, SHAPE_ATTR_CLIP_RULE },
  { "mask", (SHAPE_CONTAINERS & ~BIT (SHAPE_DEFS)) |SHAPE_GRAPHICS | BIT (SHAPE_USE), 0, SHAPE_ATTR_MASK },
  { "font-family", SHAPE_ANY, 0, SHAPE_ATTR_FONT_FAMILY },
  { "font-style", SHAPE_ANY, 0, SHAPE_ATTR_FONT_STYLE },
  { "font-variant", SHAPE_ANY, 0, SHAPE_ATTR_FONT_VARIANT },
  { "font-weight", SHAPE_ANY, 0, SHAPE_ATTR_FONT_WEIGHT },
  { "font-stretch", SHAPE_ANY, 0, SHAPE_ATTR_FONT_STRETCH },
  { "filter", (SHAPE_CONTAINERS & ~BIT (SHAPE_DEFS)) |SHAPE_GRAPHICS | BIT (SHAPE_USE), 0, SHAPE_ATTR_FILTER },
  { "fill", SHAPE_ANY, 0, SHAPE_ATTR_FILL },
  { "fill-opacity", SHAPE_ANY, 0, SHAPE_ATTR_FILL_OPACITY },
  { "fill-rule", SHAPE_ANY, 0, SHAPE_ATTR_FILL_RULE },
  { "stroke", SHAPE_ANY, 0, SHAPE_ATTR_STROKE },
  { "stroke-opacity", SHAPE_ANY, 0, SHAPE_ATTR_STROKE_OPACITY },
  { "stroke-width", SHAPE_ANY, 0, SHAPE_ATTR_STROKE_WIDTH },
  { "stroke-linecap", SHAPE_ANY, 0, SHAPE_ATTR_STROKE_LINECAP },
  { "stroke-linejoin", SHAPE_ANY, 0, SHAPE_ATTR_STROKE_LINEJOIN },
  { "stroke-miterlimit", SHAPE_ANY, 0, SHAPE_ATTR_STROKE_MITERLIMIT },
  { "stroke-dasharray", SHAPE_ANY, 0, SHAPE_ATTR_STROKE_DASHARRAY },
  { "stroke-dashoffset", SHAPE_ANY, 0, SHAPE_ATTR_STROKE_DASHOFFSET },
  { "marker-start", SHAPE_ANY, 0, SHAPE_ATTR_MARKER_START },
  { "marker-mid", SHAPE_ANY, 0, SHAPE_ATTR_MARKER_MID },
  { "marker-end", SHAPE_ANY, 0, SHAPE_ATTR_MARKER_END },
  { "paint-order", SHAPE_ANY, 0, SHAPE_ATTR_PAINT_ORDER },
  { "shape-rendering", SHAPE_ANY, 0, SHAPE_ATTR_SHAPE_RENDERING },
  { "text-rendering", SHAPE_ANY, 0, SHAPE_ATTR_TEXT_RENDERING },
  { "image-rendering", SHAPE_ANY, 0, SHAPE_ATTR_IMAGE_RENDERING },
  { "mix-blend-mode", SHAPE_CONTAINERS | SHAPE_GRAPHICS | SHAPE_GRAPHICS_REF, 0, SHAPE_ATTR_BLEND_MODE },
  { "isolation", SHAPE_CONTAINERS | SHAPE_GRAPHICS | SHAPE_GRAPHICS_REF, 0, SHAPE_ATTR_ISOLATION },
  { "pathLength", SHAPE_SHAPES, 0, SHAPE_ATTR_PATH_LENGTH },
  { "href", SHAPE_GRAPHICS_REF | SHAPE_PAINT_SERVERS, 0, SHAPE_ATTR_HREF },
  { "xlink:href", SHAPE_GRAPHICS_REF | SHAPE_PAINT_SERVERS, 0, SHAPE_ATTR_HREF | DEPRECATED_BIT },
  { "overflow", SHAPE_ANY, 0, SHAPE_ATTR_OVERFLOW },
  { "vector-effect", SHAPE_GRAPHICS | BIT (SHAPE_USE), 0, SHAPE_ATTR_VECTOR_EFFECT },
  { "d", BIT (SHAPE_PATH), 0, SHAPE_ATTR_PATH },
  { "cx", BIT (SHAPE_CIRCLE) | BIT (SHAPE_ELLIPSE) | BIT (SHAPE_RADIAL_GRADIENT), 0, SHAPE_ATTR_CX },
  { "cy", BIT (SHAPE_CIRCLE) | BIT (SHAPE_ELLIPSE) | BIT (SHAPE_RADIAL_GRADIENT), 0, SHAPE_ATTR_CY },
  { "r", BIT (SHAPE_CIRCLE) | BIT (SHAPE_RADIAL_GRADIENT), 0, SHAPE_ATTR_R },
  { "x",  BIT (SHAPE_SVG) | BIT (SHAPE_RECT) | BIT (SHAPE_IMAGE) |
          BIT (SHAPE_USE) | BIT (SHAPE_FILTER) | BIT (SHAPE_PATTERN) |
          BIT (SHAPE_MASK) | SHAPE_TEXTS, 0, SHAPE_ATTR_X },
  { "y",  BIT (SHAPE_SVG) | BIT (SHAPE_RECT) | BIT (SHAPE_IMAGE) |
          BIT (SHAPE_USE) | BIT (SHAPE_FILTER) | BIT (SHAPE_PATTERN) |
          BIT (SHAPE_MASK) | SHAPE_TEXTS, 0, SHAPE_ATTR_Y },
  { "width", BIT (SHAPE_SVG) | BIT (SHAPE_RECT) | BIT (SHAPE_IMAGE) |
             BIT (SHAPE_USE) | BIT (SHAPE_FILTER) | BIT (SHAPE_MASK) |
             BIT (SHAPE_PATTERN), 0, SHAPE_ATTR_WIDTH },
  { "markerWidth", BIT (SHAPE_MARKER), 0, SHAPE_ATTR_WIDTH },
  { "height", BIT (SHAPE_SVG) | BIT (SHAPE_RECT) | BIT (SHAPE_IMAGE) |
              BIT (SHAPE_USE) | BIT (SHAPE_FILTER) | BIT (SHAPE_MASK) |
              BIT (SHAPE_PATTERN), 0, SHAPE_ATTR_HEIGHT },
  { "markerHeight", BIT (SHAPE_MARKER), 0, SHAPE_ATTR_HEIGHT },
  { "rx", BIT (SHAPE_RECT) | BIT (SHAPE_ELLIPSE) | BIT (SHAPE_RADIAL_GRADIENT), 0, SHAPE_ATTR_RX },
  { "ry", BIT (SHAPE_RECT) | BIT (SHAPE_ELLIPSE) | BIT (SHAPE_RADIAL_GRADIENT), 0, SHAPE_ATTR_RY },
  { "x1", BIT (SHAPE_LINE) | BIT (SHAPE_LINEAR_GRADIENT), 0, SHAPE_ATTR_X1 },
  { "y1", BIT (SHAPE_LINE) | BIT (SHAPE_LINEAR_GRADIENT), 0, SHAPE_ATTR_Y1 },
  { "x2", BIT (SHAPE_LINE) | BIT (SHAPE_LINEAR_GRADIENT), 0, SHAPE_ATTR_X2 },
  { "y2", BIT (SHAPE_LINE) | BIT (SHAPE_LINEAR_GRADIENT), 0, SHAPE_ATTR_Y2 },
  { "points", BIT (SHAPE_POLYGON) | BIT (SHAPE_POLYLINE), 0, SHAPE_ATTR_POINTS },
  { "spreadMethod", SHAPE_GRADIENTS, 0, SHAPE_ATTR_SPREAD_METHOD },
  { "gradientUnits", SHAPE_GRADIENTS, 0, SHAPE_ATTR_CONTENT_UNITS },
  { "clipPathUnits", BIT (SHAPE_CLIP_PATH), 0, SHAPE_ATTR_CONTENT_UNITS },
  { "maskContentUnits", BIT (SHAPE_MASK), 0, SHAPE_ATTR_CONTENT_UNITS },
  { "patternContentUnits", BIT (SHAPE_PATTERN), 0, SHAPE_ATTR_CONTENT_UNITS },
  { "primitiveUnits", BIT (SHAPE_FILTER), 0, SHAPE_ATTR_CONTENT_UNITS },
  { "maskUnits", BIT (SHAPE_MASK), 0, SHAPE_ATTR_BOUND_UNITS },
  { "patternUnits", BIT (SHAPE_PATTERN), 0, SHAPE_ATTR_BOUND_UNITS },
  { "filterUnits", BIT (SHAPE_FILTER), 0, SHAPE_ATTR_BOUND_UNITS },
  { "fx", BIT (SHAPE_RADIAL_GRADIENT), 0, SHAPE_ATTR_FX },
  { "fy", BIT (SHAPE_RADIAL_GRADIENT), 0, SHAPE_ATTR_FY },
  { "fr", BIT (SHAPE_RADIAL_GRADIENT), 0, SHAPE_ATTR_FR },
  { "mask-type", BIT (SHAPE_MASK), 0, SHAPE_ATTR_MASK_TYPE },
  { "viewBox", SHAPE_VIEWPORTS | BIT (SHAPE_PATTERN) | BIT (SHAPE_MARKER), 0, SHAPE_ATTR_VIEW_BOX },
  { "preserveAspectRatio", SHAPE_VIEWPORTS | BIT (SHAPE_PATTERN) | BIT (SHAPE_MARKER) | BIT (SHAPE_IMAGE), 0, SHAPE_ATTR_CONTENT_FIT },
  { "refX", BIT (SHAPE_MARKER), 0, SHAPE_ATTR_REF_X },
  { "refY", BIT (SHAPE_MARKER), 0, SHAPE_ATTR_REF_Y },
  { "markerUnits", BIT (SHAPE_MARKER), 0, SHAPE_ATTR_MARKER_UNITS },
  { "orient", BIT (SHAPE_MARKER), 0, SHAPE_ATTR_MARKER_ORIENT },
  { "lang", SHAPE_ANY, 0, SHAPE_ATTR_LANG },
  { "xml:lang", SHAPE_ANY, 0, SHAPE_ATTR_LANG | DEPRECATED_BIT },
  { "text-anchor", SHAPE_ANY, 0, SHAPE_ATTR_TEXT_ANCHOR },
  { "dx", SHAPE_TEXTS, 0, SHAPE_ATTR_DX },
  { "dy", SHAPE_TEXTS, 0, SHAPE_ATTR_DY },
  { "unicode-bidi", SHAPE_ANY, 0, SHAPE_ATTR_UNICODE_BIDI },
  { "direction", SHAPE_ANY, 0, SHAPE_ATTR_DIRECTION },
  { "writing-mode", SHAPE_ANY, 0, SHAPE_ATTR_WRITING_MODE },
  { "letter-spacing", SHAPE_ANY, 0, SHAPE_ATTR_LETTER_SPACING },
  { "text-decoration", SHAPE_ANY, 0, SHAPE_ATTR_TEXT_DECORATION },
  { "requiredExtensions", SHAPE_ANY, 0, SHAPE_ATTR_REQUIRED_EXTENSIONS },
  { "systemLanguage", SHAPE_ANY, 0, SHAPE_ATTR_SYSTEM_LANGUAGE },
  { "gpa:stroke-minwidth", SHAPE_ANY, 0, SHAPE_ATTR_STROKE_MINWIDTH },
  { "gpa:stroke-maxwidth", SHAPE_ANY, 0, SHAPE_ATTR_STROKE_MAXWIDTH },
  { "stop-offset", SHAPE_GRADIENTS, 0, SHAPE_ATTR_STOP_OFFSET },
  { "stop-color", SHAPE_ANY, 0, SHAPE_ATTR_STOP_COLOR },
  { "stop-opacity", SHAPE_ANY, 0, SHAPE_ATTR_STOP_OPACITY },
  { "x", BIT (SHAPE_FILTER), ANY_FILTER, SHAPE_ATTR_FE_X },
  { "y", BIT (SHAPE_FILTER), ANY_FILTER, SHAPE_ATTR_FE_Y },
  { "width", BIT (SHAPE_FILTER), ANY_FILTER, SHAPE_ATTR_FE_WIDTH },
  { "height", BIT (SHAPE_FILTER), ANY_FILTER, SHAPE_ATTR_FE_HEIGHT },
  { "result", BIT (SHAPE_FILTER), ANY_FILTER, SHAPE_ATTR_FE_RESULT },
  { "flood-color", BIT (SHAPE_FILTER), BIT (FE_FLOOD) | BIT (FE_DROPSHADOW), SHAPE_ATTR_FE_COLOR },
  { "flood-color", SHAPE_ANY, 0, SHAPE_ATTR_FE_COLOR },
  { "flood-opacity", BIT (SHAPE_FILTER), BIT (FE_FLOOD) | BIT (FE_DROPSHADOW), SHAPE_ATTR_FE_OPACITY },
  { "flood-opacity", SHAPE_ANY, 0, SHAPE_ATTR_FE_OPACITY },
  { "in", BIT (SHAPE_FILTER), (ANY_FILTER | BIT (FE_MERGE_NODE)) & ~(BIT (FE_FLOOD) | BIT (FE_IMAGE) | BIT (FE_MERGE)), SHAPE_ATTR_FE_IN },
  { "in2", BIT (SHAPE_FILTER), BIT (FE_BLEND) | BIT (FE_COMPOSITE) | BIT (FE_DISPLACEMENT), SHAPE_ATTR_FE_IN2 },
  { "stdDeviation", BIT (SHAPE_FILTER), BIT (FE_BLUR) | BIT (FE_DROPSHADOW), SHAPE_ATTR_FE_STD_DEV },
  { "dx", BIT (SHAPE_FILTER), BIT (FE_OFFSET) | BIT (FE_DROPSHADOW), SHAPE_ATTR_FE_DX },
  { "dy", BIT (SHAPE_FILTER), BIT (FE_OFFSET) | BIT (FE_DROPSHADOW), SHAPE_ATTR_FE_DY },
  { "edgeMode", BIT (SHAPE_FILTER), BIT (FE_BLUR), SHAPE_ATTR_FE_BLUR_EDGE_MODE },
  { "mode", BIT (SHAPE_FILTER), BIT (FE_BLEND), SHAPE_ATTR_FE_BLEND_MODE },
  { "no-composite", BIT (SHAPE_FILTER), BIT (FE_BLEND), SHAPE_ATTR_FE_BLEND_COMPOSITE },
  { "type", BIT (SHAPE_FILTER), BIT (FE_COLOR_MATRIX), SHAPE_ATTR_FE_COLOR_MATRIX_TYPE },
  { "values", BIT (SHAPE_FILTER), BIT (FE_COLOR_MATRIX), SHAPE_ATTR_FE_COLOR_MATRIX_VALUES },
  { "operator", BIT (SHAPE_FILTER), BIT (FE_COMPOSITE), SHAPE_ATTR_FE_COMPOSITE_OPERATOR },
  { "k1", BIT (SHAPE_FILTER), BIT (FE_COMPOSITE), SHAPE_ATTR_FE_COMPOSITE_K1 },
  { "k2", BIT (SHAPE_FILTER), BIT (FE_COMPOSITE), SHAPE_ATTR_FE_COMPOSITE_K2 },
  { "k3", BIT (SHAPE_FILTER), BIT (FE_COMPOSITE), SHAPE_ATTR_FE_COMPOSITE_K3 },
  { "k4", BIT (SHAPE_FILTER), BIT (FE_COMPOSITE), SHAPE_ATTR_FE_COMPOSITE_K4 },
  { "scale", BIT (SHAPE_FILTER), BIT (FE_DISPLACEMENT), SHAPE_ATTR_FE_DISPLACEMENT_SCALE },
  { "xChannelSelector", BIT (SHAPE_FILTER), BIT (FE_DISPLACEMENT), SHAPE_ATTR_FE_DISPLACEMENT_X },
  { "yChannelSelector", BIT (SHAPE_FILTER), BIT (FE_DISPLACEMENT), SHAPE_ATTR_FE_DISPLACEMENT_Y },
  { "href", BIT (SHAPE_FILTER), BIT (FE_IMAGE), SHAPE_ATTR_FE_IMAGE_HREF },
  { "xlink:href", BIT (SHAPE_FILTER), BIT (FE_IMAGE), SHAPE_ATTR_FE_IMAGE_HREF | DEPRECATED_BIT },
  { "preserveAspectRatio", BIT (SHAPE_FILTER), BIT (FE_IMAGE), SHAPE_ATTR_FE_IMAGE_CONTENT_FIT },
  { "type", BIT (SHAPE_FILTER), FILTER_FUNCS, SHAPE_ATTR_FE_FUNC_TYPE },
  { "tableValues", BIT (SHAPE_FILTER), FILTER_FUNCS, SHAPE_ATTR_FE_FUNC_VALUES },
  { "slope", BIT (SHAPE_FILTER), FILTER_FUNCS, SHAPE_ATTR_FE_FUNC_SLOPE },
  { "intercept", BIT (SHAPE_FILTER), FILTER_FUNCS, SHAPE_ATTR_FE_FUNC_INTERCEPT },
  { "amplitude", BIT (SHAPE_FILTER), FILTER_FUNCS, SHAPE_ATTR_FE_FUNC_AMPLITUDE },
  { "exponent", BIT (SHAPE_FILTER), FILTER_FUNCS, SHAPE_ATTR_FE_FUNC_EXPONENT },
  { "offset", BIT (SHAPE_FILTER), FILTER_FUNCS, SHAPE_ATTR_FE_FUNC_OFFSET },
};

static guint
shape_attr_lookup_hash (gconstpointer v)
{
  const ShapeAttrLookup *l = (const ShapeAttrLookup *) v;

  return g_str_hash (l->name);
}

/* This is a slightly tricky use of a hash table, since this relationship
 * is not transitive, in general. It is for our use case, by the way the
 * table is constructed.
 *
 * Names can occur multiple times in the table, but each (name, shape)
 * pair will only occur once. Attributes can also occur multiple times
 * (e.g. transform vs gradientTransform vs patternTransform).
 *
 * We use the array too, to find names for attributes.
 */
static gboolean
shape_attr_lookup_equal (gconstpointer v0,
                         gconstpointer v1)
{
  const ShapeAttrLookup *l0 = (const ShapeAttrLookup *) v0;
  const ShapeAttrLookup *l1 = (const ShapeAttrLookup *) v1;

  return strcmp (l0->name, l1->name) == 0 &&
         (l0->shapes & l1->shapes) != 0 &&
         ((l0->filters == 0 && l1->filters == 0) ||
          (l0->filters & l1->filters) != 0);
}

static GHashTable *shape_attr_lookup_table;

static void
shape_attrs_init (void)
{
  shape_attr_lookup_table = g_hash_table_new (shape_attr_lookup_hash,
                                              shape_attr_lookup_equal);

  for (unsigned int i = 0; i < G_N_ELEMENTS (shape_attr_lookups); i++)
    g_hash_table_add (shape_attr_lookup_table, &shape_attr_lookups[i]);
}

static gboolean
shape_attr_lookup (const char *name,
                   ShapeType   type,
                   ShapeAttr  *attr,
                   gboolean   *deprecated)
{
  ShapeAttrLookup key;
  ShapeAttrLookup *found;

  key.name = name;
  key.shapes = BIT (type);
  key.filters = 0;

  found = g_hash_table_lookup (shape_attr_lookup_table, &key);

  if (!found)
    return FALSE;

  g_assert ((found->shapes & BIT (type)) != 0);

  if (found->attr & DEPRECATED_BIT)
    {
      *attr = found->attr & ~DEPRECATED_BIT;
      *deprecated = TRUE;
    }
  else
    {
      *attr = found->attr;
      *deprecated = FALSE;
    }

  return TRUE;
}

static gboolean
filter_attr_lookup (FilterPrimitiveType  type,
                    const char          *name,
                    ShapeAttr           *attr,
                    gboolean            *deprecated)
{
  ShapeAttrLookup key;
  ShapeAttrLookup *found;

  key.name = name;
  key.shapes = BIT (SHAPE_FILTER);
  key.filters = BIT (type);

  found = g_hash_table_lookup (shape_attr_lookup_table, &key);

  if (!found)
    return FALSE;

  g_assert ((found->shapes & BIT (SHAPE_FILTER)) != 0);
  g_assert ((found->filters & BIT (type)) != 0);

  if (found->attr & DEPRECATED_BIT)
    {
      *attr = found->attr & ~DEPRECATED_BIT;
      *deprecated = TRUE;
    }
  else
    {
      *attr = found->attr;
      *deprecated = FALSE;
    }

  return TRUE;
}

static const char *
shape_attr_get_name (ShapeAttr attr)
{
  for (unsigned int i = attr; i < G_N_ELEMENTS (shape_attr_lookups); i++)
    {
      ShapeAttrLookup *l = &shape_attr_lookups[i];

      if (l->attr == attr)
        return l->name;
    }

  return NULL;
}

static const char *
shape_attr_get_presentation (ShapeAttr attr,
                             ShapeType type)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (shape_attr_lookups); i++)
    {
      ShapeAttrLookup *l = &shape_attr_lookups[i];

      if (l->attr == attr && (l->shapes & BIT (type)) != 0 && l->filters == 0)
        return l->name;
    }

  return NULL;
}

static const char *
filter_attr_get_presentation (ShapeAttr           attr,
                              FilterPrimitiveType type)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (shape_attr_lookups); i++)
    {
      ShapeAttrLookup *l = &shape_attr_lookups[i];

      if (l->attr == attr && (l->shapes & BIT (SHAPE_FILTER)) != 0 && (l->filters & BIT (type)) != 0)
        return l->name;
    }

  return NULL;
}

static SvgValue *
shape_attr_parse_value (ShapeAttr   attr,
                        const char *value)
{
  if (match_str (value, "inherit"))
    return svg_inherit_new ();
  else if (match_str (value, "initial"))
    return svg_initial_new ();

  return shape_attrs[attr].parse_value (value);
}

static SvgValue *
shape_attr_parse_for_values (ShapeAttr   attr,
                             const char *value)
{
  if (match_str (value, "inherit"))
    return svg_inherit_new ();
  else if (match_str (value, "initial"))
    return svg_initial_new ();

  if (shape_attrs[attr].parse_for_values)
    return shape_attrs[attr].parse_for_values (value);
  else
    return shape_attrs[attr].parse_value (value);
}

static GPtrArray *
shape_attr_parse_values (ShapeAttr      attr,
                         TransformType  transform_type,
                         const char    *value)
{
  GPtrArray *array;
  GStrv strv;

  strv = g_strsplit (value, ";", 0);

  array = g_ptr_array_new_with_free_func ((GDestroyNotify) svg_value_unref);

  for (unsigned int i = 0; strv[i]; i++)
    {
      char *s = g_strstrip (strv[i]);
      SvgValue *v;

      if (*s == '\0' && strv[i + 1] == NULL)
        break;

      if (attr == SHAPE_ATTR_TRANSFORM && transform_type != TRANSFORM_NONE)
        v = primitive_transform_parse (transform_type, s);
      else
        v = shape_attr_parse_for_values (attr, s);

      if (!v)
        {
          g_ptr_array_unref (array);
          g_strfreev (strv);
          return NULL;
        }

      g_ptr_array_add (array, v);
    }

  g_strfreev (strv);
  return array;
}

/*  }}} */
/* {{{ Shapes */

typedef enum
{
  SHAPE_TYPE_HAS_SHAPES      = 1 << 0,
  SHAPE_TYPE_HAS_COLOR_STOPS = 1 << 1,
  SHAPE_TYPE_HAS_FILTERS     = 1 << 2,
  SHAPE_TYPE_NEVER_RENDERED  = 1 << 3,
  SHAPE_TYPE_HAS_GPA_ATTRS   = 1 << 4,
} ShapeTypeFlags;

typedef struct
{
  const char *name;
  ShapeTypeFlags flags;
} ShapeTypeInfo;

static ShapeTypeInfo shape_types[] = {
  [SHAPE_LINE] = {
    .name = "line",
    .flags = SHAPE_TYPE_HAS_GPA_ATTRS,
  },
  [SHAPE_POLYLINE] = {
    .name = "polyline",
    .flags = SHAPE_TYPE_HAS_GPA_ATTRS,
  },
  [SHAPE_POLYGON] = {
    .name = "polygon",
    .flags = SHAPE_TYPE_HAS_GPA_ATTRS,
  },
  [SHAPE_RECT] = {
    .name = "rect",
    .flags = SHAPE_TYPE_HAS_GPA_ATTRS,
  },
  [SHAPE_CIRCLE] = {
    .name = "circle",
    .flags = SHAPE_TYPE_HAS_GPA_ATTRS,
  },
  [SHAPE_ELLIPSE] = {
    .name = "ellipse",
    .flags = SHAPE_TYPE_HAS_GPA_ATTRS,
  },
  [SHAPE_PATH] = {
    .name = "path",
    .flags = SHAPE_TYPE_HAS_GPA_ATTRS,
  },
  [SHAPE_GROUP] = {
    .name = "g",
    .flags = SHAPE_TYPE_HAS_SHAPES,
  },
  [SHAPE_CLIP_PATH] = {
    .name = "clipPath",
    .flags = SHAPE_TYPE_HAS_SHAPES | SHAPE_TYPE_NEVER_RENDERED,
  },
  [SHAPE_MASK] = {
    .name = "mask",
    .flags = SHAPE_TYPE_HAS_SHAPES | SHAPE_TYPE_NEVER_RENDERED,
  },
  [SHAPE_DEFS] = {
    .name = "defs",
    .flags = SHAPE_TYPE_HAS_SHAPES | SHAPE_TYPE_NEVER_RENDERED,
  },
  [SHAPE_USE] = {
    .name = "use",
  },
  [SHAPE_LINEAR_GRADIENT] = {
    .name = "linearGradient",
    .flags = SHAPE_TYPE_HAS_COLOR_STOPS | SHAPE_TYPE_NEVER_RENDERED,
  },
  [SHAPE_RADIAL_GRADIENT] = {
    .name = "radialGradient",
   .flags = SHAPE_TYPE_HAS_COLOR_STOPS | SHAPE_TYPE_NEVER_RENDERED,
  },
  [SHAPE_PATTERN] = {
    .name = "pattern",
    .flags = SHAPE_TYPE_HAS_SHAPES | SHAPE_TYPE_NEVER_RENDERED,
  },
  [SHAPE_MARKER] = {
    .name = "marker",
    .flags = SHAPE_TYPE_HAS_SHAPES | SHAPE_TYPE_NEVER_RENDERED,
  },
  [SHAPE_TEXT] = {
    .name = "text",
    .flags = SHAPE_TYPE_HAS_SHAPES,
  },
  [SHAPE_TSPAN] = {
    .name = "tspan",
    .flags = SHAPE_TYPE_HAS_SHAPES | SHAPE_TYPE_NEVER_RENDERED,
  },
  [SHAPE_SVG] = {
    .name = "svg",
    .flags = SHAPE_TYPE_HAS_SHAPES,
  },
  [SHAPE_IMAGE] = {
    .name = "image",
  },
  [SHAPE_FILTER] = {
    .name = "filter",
    .flags = SHAPE_TYPE_HAS_FILTERS | SHAPE_TYPE_NEVER_RENDERED,
  },
  [SHAPE_SYMBOL] = {
    .name = "symbol",
    .flags = SHAPE_TYPE_HAS_SHAPES | SHAPE_TYPE_NEVER_RENDERED,
   },
  [SHAPE_SWITCH] = {
    .name = "switch",
    .flags = SHAPE_TYPE_HAS_SHAPES,
  },
};

static inline gboolean
shape_type_has_shapes (ShapeType type)
{
  return (shape_types[type].flags & SHAPE_TYPE_HAS_SHAPES) != 0;
}

static inline gboolean
shape_type_has_color_stops (ShapeType type)
{
  return (shape_types[type].flags & SHAPE_TYPE_HAS_COLOR_STOPS) != 0;
}

static inline gboolean
shape_type_has_filters (ShapeType type)
{
  return (shape_types[type].flags & SHAPE_TYPE_HAS_FILTERS) != 0;
}

static inline gboolean
shape_type_has_gpa_attrs (ShapeType type)
{
  return (shape_types[type].flags & SHAPE_TYPE_HAS_GPA_ATTRS) != 0;
}

static inline gboolean
shape_type_is_never_rendered (ShapeType type)
{
  return (shape_types[type].flags & SHAPE_TYPE_NEVER_RENDERED) != 0;
}

static inline gboolean
shape_type_has_text (ShapeType type)
{
  return type == SHAPE_TEXT || type == SHAPE_TSPAN;
}

static guint
shape_type_hash (gconstpointer v)
{
  const ShapeTypeInfo *t = (const ShapeTypeInfo *) v;

  return g_str_hash (t->name);
}

static gboolean
shape_type_equal (gconstpointer v0,
                  gconstpointer v1)
{
  const ShapeTypeInfo *t0 = (const ShapeTypeInfo *) v0;
  const ShapeTypeInfo *t1 = (const ShapeTypeInfo *) v1;

  return strcmp (t0->name, t1->name) == 0;
}

static GHashTable *shape_type_lookup_table;

static void
shape_types_init (void)
{
  shape_type_lookup_table = g_hash_table_new (shape_type_hash,
                                              shape_type_equal);

  for (unsigned int i = 0; i < G_N_ELEMENTS (shape_types); i++)
    g_hash_table_add (shape_type_lookup_table, &shape_types[i]);
}

static gboolean
shape_type_lookup (const char *name,
                   ShapeType  *type)
{
  ShapeTypeInfo key;
  ShapeTypeInfo *value;

  key.name = name;

  value = g_hash_table_lookup (shape_type_lookup_table, &key);

  if (!value)
    return FALSE;

  *type = value - shape_types;
  return TRUE;
}

static void
shape_free (gpointer data)
{
  Shape *shape = data;

  g_clear_pointer (&shape->id, g_free);

  for (ShapeAttr attr = FIRST_SHAPE_ATTR; attr <= LAST_FILTER_ATTR; attr++)
    {
      g_clear_pointer (&shape->base[attr], svg_value_unref);
      g_clear_pointer (&shape->current[attr], svg_value_unref);
    }

  g_clear_pointer (&shape->shapes, g_ptr_array_unref);
  g_clear_pointer (&shape->animations, g_ptr_array_unref);
  g_clear_pointer (&shape->color_stops, g_ptr_array_unref);
  g_clear_pointer (&shape->filters, g_ptr_array_unref);
  g_clear_pointer (&shape->deps, g_ptr_array_unref);

  g_clear_pointer (&shape->path, gsk_path_unref);
  g_clear_pointer (&shape->measure, gsk_path_measure_unref);

  g_clear_pointer (&shape->text, g_array_unref);

  if (shape->type == SHAPE_POLYLINE || shape->type == SHAPE_POLYGON)
    g_clear_pointer (&shape->path_for.polyline.points, svg_value_unref);

  g_free (shape->gpa.attach.ref);

  _gtk_bitmask_free (shape->attrs);

  g_free (data);
}

static void animation_free (gpointer data);

static Shape *
shape_new (Shape     *parent,
           ShapeType  type)
{
  Shape *shape = g_new0 (Shape, 1);

  shape->parent = parent;
  shape->type = type;

  shape->attrs = _gtk_bitmask_new ();

  for (ShapeAttr attr = FIRST_SHAPE_ATTR; attr <= LAST_FILTER_ATTR; attr++)
    {
      shape->base[attr] = shape_attr_ref_initial_value (attr, type, parent != NULL);
      shape->current[attr] = svg_value_ref (shape->base[attr]);
    }

  if (shape_type_has_shapes (type))
    shape->shapes = g_ptr_array_new_with_free_func (shape_free);

  if (shape_type_has_color_stops (type))
    shape->color_stops = g_ptr_array_new_with_free_func (color_stop_free);

  if (shape_type_has_filters (type))
    shape->filters = g_ptr_array_new_with_free_func (filter_primitive_free);

  if (shape_type_has_text (type))
    {
      shape->text = g_array_new (FALSE, FALSE, sizeof (TextNode));
      g_array_set_clear_func (shape->text, (GDestroyNotify) text_node_clear);
    }

  return shape;
}

static gboolean
shape_has_ancestor (Shape     *shape,
                    ShapeType  type)
{
  for (Shape *p = shape->parent; p; p = p->parent)
    {
      if (p->type == type)
        return TRUE;
    }
  return FALSE;
}

/* {{{ Conditional exclusion */

static gboolean
shape_conditionally_excluded (Shape  *shape,
                              GtkSvg *svg)
{
  SvgStringList *required_extensions = (SvgStringList *) shape->current[SHAPE_ATTR_REQUIRED_EXTENSIONS];
  SvgLanguage *system_language = (SvgLanguage *) shape->current[SHAPE_ATTR_SYSTEM_LANGUAGE];
  PangoLanguage *lang;

  if (svg_value_equal ((SvgValue *) required_extensions, svg_string_list_new (NULL)) &&
      svg_value_equal ((SvgValue *) system_language, svg_language_new_list (0, NULL)))
    return FALSE;

  for (unsigned int i = 0; i < required_extensions->len; i++)
    {
      if ((svg->features & GTK_SVG_EXTENSIONS) == 0)
        return TRUE;

      if (strcmp (required_extensions->values[i], "http://www.gtk.org/grappa") != 0)
        return TRUE;
    }

  lang = gtk_get_default_language ();

  for (unsigned int i = 0; i < system_language->len; i++)
    {
      const char *l1;
      const char *l2;

      if (lang == system_language->values[i])
        return FALSE;

      l1 = pango_language_to_string (lang);
      l2 = pango_language_to_string (system_language->values[i]);

      for (unsigned int j = 0; l1[j]; j++)
        {
          if (l1[j] == '-')
            return FALSE;

          if (l1[j] != l2[j])
            break;
        }
    }

  return TRUE;
}

/* }}} */
static void
shape_resolve_rx (Shape                 *shape,
                  const graphene_rect_t *viewport,
                  gboolean               current,
                  double                *rx,
                  double                *ry)
{
  SvgValue **values;

  if (current)
    values = shape->current;
  else
    values = shape->base;

  if (shape->type == SHAPE_ELLIPSE)
    {
      if (svg_value_is_auto (values[SHAPE_ATTR_RX]))
        {
          if (svg_value_is_auto (values[SHAPE_ATTR_RY]))
            *ry = 0;
          else
            *ry = svg_number_get (values[SHAPE_ATTR_RY], viewport->size.height);
          *rx = *ry;
         }
       else
         {
           *rx = svg_number_get (values[SHAPE_ATTR_RX], viewport->size.width);
           if (svg_value_is_auto (values[SHAPE_ATTR_RY]))
             *ry = *rx;
           else
             *ry = svg_number_get (values[SHAPE_ATTR_RY], viewport->size.height);
         }
    }
  else if (shape->type == SHAPE_RECT)
    {
      if (svg_value_is_auto (values[SHAPE_ATTR_RX]) &&
         svg_value_is_auto (values[SHAPE_ATTR_RY]))
        {
          *rx = *ry = 0;
        }
      else if (svg_value_is_auto (values[SHAPE_ATTR_RY]))
        {
          *rx = svg_number_get (values[SHAPE_ATTR_RX], viewport->size.width);
          *ry = *rx;
        }
      else if (svg_value_is_auto (values[SHAPE_ATTR_RX]))
        {
          *ry = svg_number_get (values[SHAPE_ATTR_RY], viewport->size.height);
          *rx = *ry;
        }
      else
        {
          *rx = svg_number_get (values[SHAPE_ATTR_RX], viewport->size.width);
          *ry = svg_number_get (values[SHAPE_ATTR_RY], viewport->size.height);
        }

      double w = svg_number_get (values[SHAPE_ATTR_WIDTH], viewport->size.width);
      double h = svg_number_get (values[SHAPE_ATTR_HEIGHT], viewport->size.height);
      if (*rx > w / 2)
        *rx = w / 2;
      if (*ry > h / 2)
        *ry = h / 2;
    }
  else
    g_assert_not_reached ();
}

static GskPath *
shape_get_path (Shape                 *shape,
                const graphene_rect_t *viewport,
                gboolean               current)
{
  GskPathBuilder *builder;
  SvgValue **values;

  if (current)
    values = shape->current;
  else
    values = shape->base;

  switch (shape->type)
    {
    case SHAPE_LINE:
      builder = gsk_path_builder_new ();
      if (values[SHAPE_ATTR_X1] && values[SHAPE_ATTR_Y1] &&
          values[SHAPE_ATTR_X2] && values[SHAPE_ATTR_Y2])
        {
          double x1 = svg_number_get (values[SHAPE_ATTR_X1], viewport->size.width);
          double y1 = svg_number_get (values[SHAPE_ATTR_Y1], viewport->size.height);
          double x2 = svg_number_get (values[SHAPE_ATTR_X2], viewport->size.width);
          double y2 = svg_number_get (values[SHAPE_ATTR_Y2], viewport->size.height);
          gsk_path_builder_move_to (builder, x1, y1);
          gsk_path_builder_line_to (builder, x2, y2);
        }
      return gsk_path_builder_free_to_path (builder);

    case SHAPE_POLYLINE:
    case SHAPE_POLYGON:
      builder = gsk_path_builder_new ();
      if (values[SHAPE_ATTR_POINTS])
        {
          SvgNumbers *numbers = (SvgNumbers *) values[SHAPE_ATTR_POINTS];
          if (numbers->n_values > 0)
            {
              g_assert (numbers->values[0].unit != SVG_UNIT_PERCENTAGE);
              g_assert (numbers->values[1].unit != SVG_UNIT_PERCENTAGE);
              gsk_path_builder_move_to (builder,
                                        numbers->values[0].value, numbers->values[1].value);
              for (unsigned int i = 2; i < numbers->n_values; i += 2)
                {
                  g_assert (numbers->values[i].unit != SVG_UNIT_PERCENTAGE);
                  g_assert (numbers->values[i+1].unit != SVG_UNIT_PERCENTAGE);
                  gsk_path_builder_line_to (builder,
                                            numbers->values[i].value, numbers->values[i + 1].value);
                }
              if (shape->type == SHAPE_POLYGON)
                gsk_path_builder_close (builder);
            }
        }
      return gsk_path_builder_free_to_path (builder);

    case SHAPE_CIRCLE:
      builder = gsk_path_builder_new ();
      if (values[SHAPE_ATTR_CX] && values[SHAPE_ATTR_CY] && values[SHAPE_ATTR_R])
        {
          double cx = svg_number_get (values[SHAPE_ATTR_CX], viewport->size.width);
          double cy = svg_number_get (values[SHAPE_ATTR_CY], viewport->size.height);
          double r = svg_number_get (values[SHAPE_ATTR_R], normalized_diagonal (viewport));
          gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (cx, cy), r);
        }
      return gsk_path_builder_free_to_path (builder);

    case SHAPE_ELLIPSE:
      builder = gsk_path_builder_new ();
      if (values[SHAPE_ATTR_CX] && values[SHAPE_ATTR_CY] &&
          values[SHAPE_ATTR_RX] && values[SHAPE_ATTR_RY])
        {
          double cx = svg_number_get (values[SHAPE_ATTR_CX], viewport->size.width);
          double cy = svg_number_get (values[SHAPE_ATTR_CY], viewport->size.height);
          double rx, ry;
          shape_resolve_rx (shape, viewport, current, &rx, &ry);
          path_builder_add_ellipse (builder, cx, cy, rx, ry);
        }
      return gsk_path_builder_free_to_path (builder);

    case SHAPE_RECT:
      builder = gsk_path_builder_new ();
      if (values[SHAPE_ATTR_X] && values[SHAPE_ATTR_Y] &&
          values[SHAPE_ATTR_WIDTH] && values[SHAPE_ATTR_HEIGHT] &&
          values[SHAPE_ATTR_RX] && values[SHAPE_ATTR_RY])
        {
          double x = svg_number_get (values[SHAPE_ATTR_X], viewport->size.width);
          double y = svg_number_get (values[SHAPE_ATTR_Y], viewport->size.height);
          double width = svg_number_get (values[SHAPE_ATTR_WIDTH], viewport->size.width);
          double height = svg_number_get (values[SHAPE_ATTR_HEIGHT],viewport->size.height);
          double rx, ry;
          shape_resolve_rx (shape, viewport, current, &rx, &ry);
          if (rx == 0 || ry == 0)
            gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (x, y, width, height));
          else
            gsk_path_builder_add_rounded_rect (builder,
                                               &(GskRoundedRect) {
                                                 .bounds = GRAPHENE_RECT_INIT (x, y, width, height),
                                                 .corner = {
                                                   GRAPHENE_SIZE_INIT (rx, ry),
                                                   GRAPHENE_SIZE_INIT (rx, ry),
                                                   GRAPHENE_SIZE_INIT (rx, ry),
                                                   GRAPHENE_SIZE_INIT (rx, ry),
                                                 }
                                              });
        }
      return gsk_path_builder_free_to_path (builder);

    case SHAPE_PATH:
      if (values[SHAPE_ATTR_PATH] &&
          svg_path_get_gsk (values[SHAPE_ATTR_PATH]))
        {
          return gsk_path_ref (svg_path_get_gsk (values[SHAPE_ATTR_PATH]));
        }
      else
        {
          builder = gsk_path_builder_new ();
          return gsk_path_builder_free_to_path (builder);
        }
      break;

    case SHAPE_USE:
      {
        Shape *use_shape = ((SvgHref *) shape->current[SHAPE_ATTR_HREF])->shape;
        if (use_shape)
          {
            return shape_get_path (use_shape, viewport, current);
          }
        else
          {
            builder = gsk_path_builder_new ();
            return gsk_path_builder_free_to_path (builder);
          }
      }
    case SHAPE_GROUP:
    case SHAPE_CLIP_PATH:
    case SHAPE_MASK:
    case SHAPE_DEFS:
    case SHAPE_LINEAR_GRADIENT:
    case SHAPE_RADIAL_GRADIENT:
    case SHAPE_PATTERN:
    case SHAPE_MARKER:
    case SHAPE_TEXT:
    case SHAPE_TSPAN:
    case SHAPE_SVG:
    case SHAPE_IMAGE:
    case SHAPE_FILTER:
    case SHAPE_SYMBOL:
    case SHAPE_SWITCH:
      g_error ("Attempt to get the path of a %s", shape_types[shape->type].name);
      break;
    default:
      g_assert_not_reached ();
    }
}

static GskPath *
shape_get_current_path (Shape                 *shape,
                        const graphene_rect_t *viewport)
{
  if (shape->path)
    {
      double rx, ry;

      switch (shape->type)
        {
        case SHAPE_LINE:
          if (shape->path_for.line.x1 != svg_number_get (shape->current[SHAPE_ATTR_X1], viewport->size.width) ||
              shape->path_for.line.y1 != svg_number_get (shape->current[SHAPE_ATTR_Y1], viewport->size.height) ||
              shape->path_for.line.x2 != svg_number_get (shape->current[SHAPE_ATTR_X2], viewport->size.height) ||
              shape->path_for.line.y2 != svg_number_get (shape->current[SHAPE_ATTR_Y2], viewport->size.height))
            {
              g_clear_pointer (&shape->path, gsk_path_unref);
              g_clear_pointer (&shape->measure, gsk_path_measure_unref);
            }
          break;

        case SHAPE_POLYLINE:
        case SHAPE_POLYGON:
          if (!svg_value_equal (shape->path_for.polyline.points, shape->current[SHAPE_ATTR_POINTS]))
            {
              g_clear_pointer (&shape->path, gsk_path_unref);
              g_clear_pointer (&shape->measure, gsk_path_measure_unref);
            }
          break;

        case SHAPE_CIRCLE:
          if (shape->path_for.circle.cx != svg_number_get (shape->current[SHAPE_ATTR_CX], viewport->size.width) ||
              shape->path_for.circle.cy != svg_number_get (shape->current[SHAPE_ATTR_CY], viewport->size.height) ||
              shape->path_for.circle.r != svg_number_get (shape->current[SHAPE_ATTR_R], normalized_diagonal (viewport)))
            {
              g_clear_pointer (&shape->path, gsk_path_unref);
              g_clear_pointer (&shape->measure, gsk_path_measure_unref);
            }
          break;

        case SHAPE_ELLIPSE:
          shape_resolve_rx (shape, viewport, TRUE, &rx, &ry);
          if (shape->path_for.ellipse.cx != svg_number_get (shape->current[SHAPE_ATTR_CX], viewport->size.width) ||
              shape->path_for.ellipse.cy != svg_number_get (shape->current[SHAPE_ATTR_CY], viewport->size.height) ||
              shape->path_for.ellipse.rx != rx ||
              shape->path_for.ellipse.ry != ry)
            {
              g_clear_pointer (&shape->path, gsk_path_unref);
              g_clear_pointer (&shape->measure, gsk_path_measure_unref);
            }
          break;

        case SHAPE_RECT:
          shape_resolve_rx (shape, viewport, TRUE, &rx, &ry);
          if (shape->path_for.rect.x != svg_number_get (shape->current[SHAPE_ATTR_X], viewport->size.width) ||
              shape->path_for.rect.y != svg_number_get (shape->current[SHAPE_ATTR_Y], viewport->size.height) ||
              shape->path_for.rect.w != svg_number_get (shape->current[SHAPE_ATTR_WIDTH], viewport->size.width) ||
              shape->path_for.rect.h != svg_number_get (shape->current[SHAPE_ATTR_HEIGHT], viewport->size.height) ||
              shape->path_for.rect.rx != rx ||
              shape->path_for.rect.ry != ry)
            {
              g_clear_pointer (&shape->path, gsk_path_unref);
              g_clear_pointer (&shape->measure, gsk_path_measure_unref);
            }
          break;

        case SHAPE_PATH:
          if (shape->path != svg_path_get_gsk (shape->current[SHAPE_ATTR_PATH]))
            {
              g_clear_pointer (&shape->path, gsk_path_unref);
              g_clear_pointer (&shape->measure, gsk_path_measure_unref);
            }
          break;

        case SHAPE_USE:
          g_clear_pointer (&shape->path, gsk_path_unref);
          break;

        case SHAPE_GROUP:
        case SHAPE_CLIP_PATH:
        case SHAPE_MASK:
        case SHAPE_DEFS:
        case SHAPE_LINEAR_GRADIENT:
        case SHAPE_RADIAL_GRADIENT:
        case SHAPE_PATTERN:
        case SHAPE_MARKER:
        case SHAPE_TEXT:
        case SHAPE_TSPAN:
        case SHAPE_SVG:
        case SHAPE_IMAGE:
        case SHAPE_FILTER:
        case SHAPE_SYMBOL:
        case SHAPE_SWITCH:
          g_error ("Attempt to get the path of a %s", shape_types[shape->type].name);
          break;
        default:
          g_assert_not_reached ();
        }
    }

  if (!shape->path)
    {
      double rx, ry;

      shape->path = shape_get_path (shape, viewport, TRUE);
      shape->valid_bounds = FALSE;

      switch (shape->type)
        {
        case SHAPE_LINE:
          shape->path_for.line.x1 = svg_number_get (shape->current[SHAPE_ATTR_X1], viewport->size.width);
          shape->path_for.line.y1 = svg_number_get (shape->current[SHAPE_ATTR_Y1], viewport->size.height);
          shape->path_for.line.x2 = svg_number_get (shape->current[SHAPE_ATTR_X2], viewport->size.width);
          shape->path_for.line.y2 = svg_number_get (shape->current[SHAPE_ATTR_Y2], viewport->size.height);
          break;

        case SHAPE_POLYLINE:
        case SHAPE_POLYGON:
          g_clear_pointer (&shape->path_for.polyline.points, svg_value_unref);
          shape->path_for.polyline.points = svg_value_ref (shape->current[SHAPE_ATTR_POINTS]);
          break;

        case SHAPE_CIRCLE:
          shape->path_for.circle.cx = svg_number_get (shape->current[SHAPE_ATTR_CX], viewport->size.width);
          shape->path_for.circle.cy = svg_number_get (shape->current[SHAPE_ATTR_CY], viewport->size.height);
          shape->path_for.circle.r = svg_number_get (shape->current[SHAPE_ATTR_R], normalized_diagonal (viewport));
          break;

        case SHAPE_ELLIPSE:
          shape_resolve_rx (shape, viewport, TRUE, &rx, &ry);
          shape->path_for.ellipse.cx = svg_number_get (shape->current[SHAPE_ATTR_CX], viewport->size.width);
          shape->path_for.ellipse.cy = svg_number_get (shape->current[SHAPE_ATTR_CY], viewport->size.height);
          shape->path_for.ellipse.rx = rx;
          shape->path_for.ellipse.ry = ry;
          break;

        case SHAPE_RECT:
          shape_resolve_rx (shape, viewport, TRUE, &rx, &ry);
          shape->path_for.rect.x = svg_number_get (shape->current[SHAPE_ATTR_X], viewport->size.width);
          shape->path_for.rect.y = svg_number_get (shape->current[SHAPE_ATTR_Y], viewport->size.height);
          shape->path_for.rect.w = svg_number_get (shape->current[SHAPE_ATTR_WIDTH], viewport->size.width);
          shape->path_for.rect.h = svg_number_get (shape->current[SHAPE_ATTR_HEIGHT], viewport->size.height);
          shape->path_for.rect.rx = rx;
          shape->path_for.rect.ry = ry;
          break;

        case SHAPE_PATH:
        case SHAPE_USE:
          break;

        case SHAPE_GROUP:
        case SHAPE_CLIP_PATH:
        case SHAPE_MASK:
        case SHAPE_DEFS:
        case SHAPE_LINEAR_GRADIENT:
        case SHAPE_RADIAL_GRADIENT:
        case SHAPE_PATTERN:
        case SHAPE_MARKER:
        case SHAPE_TEXT:
        case SHAPE_TSPAN:
        case SHAPE_SVG:
        case SHAPE_IMAGE:
        case SHAPE_FILTER:
        case SHAPE_SYMBOL:
        case SHAPE_SWITCH:
        default:
          g_assert_not_reached ();
        }
    }

  return gsk_path_ref (shape->path);
}

static GskPathMeasure *
shape_get_current_measure (Shape                 *shape,
                           const graphene_rect_t *viewport)
{
  if (!shape->measure)
    {
      GskPath *path = shape_get_current_path (shape, viewport);
      shape->measure = gsk_path_measure_new (path);
      gsk_path_unref (path);
    }

  return gsk_path_measure_ref (shape->measure);
}

static gboolean
shape_get_current_bounds (Shape                 *shape,
                          const graphene_rect_t *viewport,
                          GtkSvg                *svg,
                          graphene_rect_t       *bounds)
{
  graphene_rect_t b;
  gboolean has_any;
  gboolean ret = FALSE;

  graphene_rect_init_from_rect (&b, graphene_rect_zero ());

  switch (shape->type)
    {
    case SHAPE_LINE:
    case SHAPE_POLYLINE:
    case SHAPE_POLYGON:
    case SHAPE_RECT:
    case SHAPE_CIRCLE:
    case SHAPE_ELLIPSE:
    case SHAPE_PATH:
      if (!shape->valid_bounds)
        {
          GskPath *path = shape_get_current_path (shape, viewport);
          if (!gsk_path_get_tight_bounds (shape->path, &shape->bounds))
            graphene_rect_init (&shape->bounds, 0, 0, 0, 0);
          shape->valid_bounds = TRUE;
          gsk_path_unref (path);
        }
      graphene_rect_init_from_rect (&b, &shape->bounds);
      ret = TRUE;
      break;
    case SHAPE_USE:
      {
        Shape *use_shape = ((SvgHref *) shape->current[SHAPE_ATTR_HREF])->shape;
        if (use_shape)
          ret = shape_get_current_bounds (use_shape, viewport, svg, &b);
        ret = TRUE;
      }
      break;
    case SHAPE_GROUP:
    case SHAPE_CLIP_PATH:
    case SHAPE_MASK:
    case SHAPE_PATTERN:
    case SHAPE_MARKER:
    case SHAPE_SVG:
    case SHAPE_SYMBOL:
    case SHAPE_SWITCH:
      has_any = FALSE;
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          Shape *sh = g_ptr_array_index (shape->shapes, i);
          graphene_rect_t b2;

          if (svg_enum_get (sh->current[SHAPE_ATTR_DISPLAY]) == DISPLAY_NONE)
            continue;

          if (shape_conditionally_excluded (sh, svg))
            continue;

          if (shape_get_current_bounds (sh, viewport, svg, &b2))
            {
              if (_gtk_bitmask_get (sh->attrs, SHAPE_ATTR_TRANSFORM))
                {
                   SvgTransform *tf = (SvgTransform *) sh->current[SHAPE_ATTR_TRANSFORM];
                   GskTransform *transform = svg_transform_get_gsk (tf);
                   gsk_transform_transform_bounds (transform, &b2, &b2);
                   gsk_transform_unref (transform);
                }

              if (!has_any)
                graphene_rect_init_from_rect (&b, &b2);
              else
                graphene_rect_union (&b, &b2, &b);
              has_any = TRUE;
            }

          if (shape->type == SHAPE_SWITCH)
            break;
        }
      if (!has_any)
        graphene_rect_init (&b, 0, 0, 0, 0);
      ret = TRUE;
      break;
    case SHAPE_TEXT:
    case SHAPE_TSPAN:
      if (!shape->valid_bounds)
        g_critical ("No valid bounds for text");
      graphene_rect_init_from_rect (&b, &shape->bounds);
      break;
    case SHAPE_IMAGE:
      {
        double x, y, width, height;

        x = viewport->origin.x + svg_number_get (shape->current[SHAPE_ATTR_X], viewport->size.width);
        y = viewport->origin.y + svg_number_get (shape->current[SHAPE_ATTR_Y], viewport->size.height);
        width = svg_number_get (shape->current[SHAPE_ATTR_WIDTH], viewport->size.width);
        height = svg_number_get (shape->current[SHAPE_ATTR_HEIGHT], viewport->size.height);

        graphene_rect_init (&b, x, y, width, height);
        ret = TRUE;
      }
      break;
    case SHAPE_DEFS:
    case SHAPE_LINEAR_GRADIENT:
    case SHAPE_RADIAL_GRADIENT:
    case SHAPE_FILTER:
      break;
    default:
      g_assert_not_reached ();
    }

  graphene_rect_init_from_rect (bounds, &b);

  return ret;
}

static GskStroke * shape_create_basic_stroke (Shape                 *shape,
                                              const graphene_rect_t *viewport,
                                              gboolean               allow_gpa,
                                              double                 weight);

static gboolean
shape_get_current_stroke_bounds (Shape                 *shape,
                                 const graphene_rect_t *viewport,
                                 GtkSvg                *svg,
                                 graphene_rect_t       *bounds)
{
  graphene_rect_t b;
  gboolean has_any;
  gboolean ret = FALSE;

  graphene_rect_init_from_rect (&b, graphene_rect_zero ());

  switch (shape->type)
    {
    case SHAPE_LINE:
    case SHAPE_POLYLINE:
    case SHAPE_POLYGON:
    case SHAPE_RECT:
    case SHAPE_CIRCLE:
    case SHAPE_ELLIPSE:
    case SHAPE_PATH:
      {
        GskPath *path = shape_get_current_path (shape, viewport);
        GskStroke *stroke = shape_create_basic_stroke (shape, viewport, TRUE, 400);
        if (!gsk_path_get_stroke_bounds (shape->path, stroke, &b))
          graphene_rect_init (&b, 0, 0, 0, 0);
        gsk_path_unref (path);
        gsk_stroke_free (stroke);
        ret = TRUE;
      }
      break;
    case SHAPE_USE:
      {
        Shape *use_shape = ((SvgHref *) shape->current[SHAPE_ATTR_HREF])->shape;
        if (use_shape)
          ret = shape_get_current_stroke_bounds (use_shape, viewport, svg, &b);
        ret = TRUE;
      }
      break;
    case SHAPE_GROUP:
    case SHAPE_CLIP_PATH:
    case SHAPE_MASK:
    case SHAPE_PATTERN:
    case SHAPE_MARKER:
    case SHAPE_SVG:
    case SHAPE_SYMBOL:
    case SHAPE_SWITCH:
      has_any = FALSE;
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          Shape *sh = g_ptr_array_index (shape->shapes, i);
          graphene_rect_t b2;

          if (svg_enum_get (sh->current[SHAPE_ATTR_DISPLAY]) == DISPLAY_NONE)
            continue;

          if (shape_conditionally_excluded (sh, svg))
            continue;

          if (shape_get_current_stroke_bounds (sh, viewport, svg, &b2))
            {
              if (!has_any)
                graphene_rect_init_from_rect (&b, &b2);
              else
                graphene_rect_union (&b, &b2, &b);
              has_any = TRUE;
            }

          if (shape->type == SHAPE_SWITCH)
            break;
        }
      if (!has_any)
        graphene_rect_init (&b, 0, 0, 0, 0);
      ret = TRUE;
      break;
    case SHAPE_TEXT:
    case SHAPE_TSPAN:
      if (!shape->valid_bounds)
        g_critical ("No valid bounds for text");
      graphene_rect_init_from_rect (&b, &shape->bounds);
      break;
    case SHAPE_IMAGE:
      {
        double x, y, width, height;

        x = viewport->origin.x + svg_number_get (shape->current[SHAPE_ATTR_X], viewport->size.width);
        y = viewport->origin.y + svg_number_get (shape->current[SHAPE_ATTR_Y], viewport->size.height);
        width = svg_number_get (shape->current[SHAPE_ATTR_WIDTH], viewport->size.width);
        height = svg_number_get (shape->current[SHAPE_ATTR_HEIGHT], viewport->size.height);

        graphene_rect_init (&b, x, y, width, height);
        ret = TRUE;
      }
      break;
    case SHAPE_DEFS:
    case SHAPE_LINEAR_GRADIENT:
    case SHAPE_RADIAL_GRADIENT:
    case SHAPE_FILTER:
      break;
    default:
      g_assert_not_reached ();
    }

  graphene_rect_init_from_rect (bounds, &b);

  return ret;
}

static unsigned int
shape_add_color_stop (Shape *shape)
{
  g_assert (shape_type_has_color_stops (shape->type));

  g_ptr_array_add (shape->color_stops, color_stop_new ());

  return shape->color_stops->len - 1;
}

static unsigned int
shape_add_filter (Shape               *shape,
                  FilterPrimitiveType  type)
{
  g_assert (shape_type_has_filters (shape->type));

  g_ptr_array_add (shape->filters, filter_primitive_new (type));

  return shape->filters->len - 1;
}

/* }}} */
/* {{{ Time handling */

/* How timing works:
 *
 * The timeline contains all TimeSpecs.
 *
 * spec->time is the resolved time for the spec (may be indefinite,
 * and may change in some cases).
 *
 * Time specs and animations are connected in a dependency tree
 * (via time specs of type 'sync').
 *
 * There are three ways in which time specs change. First, when the
 * timeline gets a start time, all 'offset' time specs get their
 * resolved time, and will never change afterwards. Second, when
 * the state changes, 'state' time specs will update their time.
 * Lastly, when a time spec changes, its dependent time specs are
 * updated, so the effects ripple through the timeline.
 *
 * Each animation has a list of begin and end time specs, as well as
 * an activation interval. The activation interval may have indefinite
 * start or end. When the animation is not playing, the activation
 * interval represents the next known activation (if any). Both
 * the start and the end time of that activation may change when
 * the animations begin or end time specs change.
 *
 * While the animation is playing, the activation interval represents
 * the current activation. In this case, the only way the start
 * time may change is when the animation is restarted. The end
 * time may still change due to time spec changes.
 */

typedef enum
{
  TIME_SPEC_TYPE_INDEFINITE,
  TIME_SPEC_TYPE_OFFSET,
  TIME_SPEC_TYPE_SYNC,
  TIME_SPEC_TYPE_STATES,
} TimeSpecType;

typedef enum
{
  TIME_SPEC_SIDE_BEGIN,
  TIME_SPEC_SIDE_END,
} TimeSpecSide;

typedef struct {
  TimeSpecType type;
  int64_t offset;
  union {
    struct {
      char *ref;
      Animation *base;
      TimeSpecSide side;
    } sync;
    struct {
      uint64_t states;
      TimeSpecSide side;
    } states;
  };
  int64_t time;
  GPtrArray *animations;
} TimeSpec;

static void
time_spec_clear (TimeSpec *t)
{
  if (t->type == TIME_SPEC_TYPE_SYNC)
    g_free (t->sync.ref);

  g_clear_pointer (&t->animations, g_ptr_array_unref);
}

static void
time_spec_free (void *p)
{
  TimeSpec *t = p;
  time_spec_clear (t);
  g_free (t);
}

static TimeSpec *
time_spec_copy (const TimeSpec *orig)
{
  TimeSpec *t = g_new0 (TimeSpec, 1);

  t->type = orig->type;
  t->offset = orig->offset;
  t->time = orig->time;

  switch (t->type)
    {
    case TIME_SPEC_TYPE_INDEFINITE:
      break;
    case TIME_SPEC_TYPE_OFFSET:
      break;
    case TIME_SPEC_TYPE_SYNC:
      t->sync.ref = g_strdup (orig->sync.ref);
      t->sync.base = orig->sync.base;
      t->sync.side = orig->sync.side;
      break;
    case TIME_SPEC_TYPE_STATES:
      t->states.states = orig->states.states;
      t->states.side = orig->states.side;
      break;
    default:
      g_assert_not_reached ();
    }

  return t;
}

static gboolean
time_spec_equal (const void *p1,
                 const void *p2)
{
  const TimeSpec *t1 = p1;
  const TimeSpec *t2 = p2;

  if (t1->type != t2->type)
    return FALSE;

  switch (t1->type)
    {
    case TIME_SPEC_TYPE_INDEFINITE:
      return TRUE;

    case TIME_SPEC_TYPE_OFFSET:
      return t1->offset == t2->offset;

    case TIME_SPEC_TYPE_SYNC:
      return t1->sync.base == t2->sync.base &&
             g_strcmp0 (t1->sync.ref, t2->sync.ref) == 0 &&
             t1->sync.side == t2->sync.side &&
             t1->offset == t2->offset;

    case TIME_SPEC_TYPE_STATES:
      return t1->states.states == t2->states.states &&
             t1->states.side == t2->states.side &&
             t1->offset == t2->offset;

    default:
      g_assert_not_reached ();
    }
}

static gboolean
time_spec_parse (TimeSpec   *spec,
                 const char *value)
{
  const char *side_str;
  const char *offset_str;
  TimeSpecSide side;

  spec->offset = 0;

  if (match_str (value, "indefinite"))
    {
      spec->type = TIME_SPEC_TYPE_INDEFINITE;
    }
  else if ((side_str = strstr (value, ".begin")) != NULL ||
           (side_str = strstr (value, ".end")) != NULL)
    {
      char *str;

      if (g_str_has_prefix (side_str, ".begin"))
        {
          side = TIME_SPEC_SIDE_BEGIN;
          offset_str = side_str + strlen (".begin");
        }
      else
        {
          side = TIME_SPEC_SIDE_END;
          offset_str = side_str + strlen (".end");
        }

      if (strlen (offset_str) > 0)
        {
          if (!parse_duration (offset_str, &spec->offset))
            return FALSE;
        }

      str = g_strndup (value, side_str - value);
      if (g_str_has_prefix (str, "gpa:states(") &&
          g_str_has_suffix (str, ")"))
        {
          uint64_t states;
          str[strlen (str) - 1] = '\0';
          if (!parse_states (str + strlen ("gpa:states("), &states))
            {
              g_free (str);
              return FALSE;
            }
          g_free (str);

          spec->type = TIME_SPEC_TYPE_STATES;
          spec->states.side = side;
          spec->states.states = states;
        }
      else
        {
          spec->type = TIME_SPEC_TYPE_SYNC;
          spec->sync.ref = str;
          spec->sync.base = NULL;
          spec->sync.side = side;
        }
    }
  else if (strlen (value) > 0)
    {
      if (!parse_duration (value, &spec->offset))
        return FALSE;

      spec->type = TIME_SPEC_TYPE_OFFSET;
    }

  return TRUE;
}

static void
time_spec_print (TimeSpec *spec,
                 GString  *s)
{
  gboolean only_nonzero = FALSE;
  const char *sides[] = { ".begin", ".end" };

  switch (spec->type)
    {
    case TIME_SPEC_TYPE_INDEFINITE:
      g_string_append (s, "indefinite");
      return;
    case TIME_SPEC_TYPE_OFFSET:
      break;
    case TIME_SPEC_TYPE_SYNC:
      g_string_append_printf (s, "%s", spec->sync.ref);
      g_string_append_printf (s, "%s", sides[spec->sync.side]);
      only_nonzero = TRUE;
      break;
    case TIME_SPEC_TYPE_STATES:
      {
        g_string_append (s, "gpa:states(");
        print_states (s, spec->states.states);
        g_string_append (s, ")");
        g_string_append_printf (s, "%s", sides[spec->states.side]);
        only_nonzero = TRUE;
      }
      break;
    default:
      g_assert_not_reached ();
    }

  if (!only_nonzero || spec->offset != 0)
    {
      string_append_double (s, only_nonzero ? " " : "", spec->offset / (double) G_TIME_SPAN_MILLISECOND);
      g_string_append (s, "ms");
    }
}

static void
time_specs_print (GPtrArray *specs,
                  GString   *s)
{
  for (unsigned int i = 0; i < specs->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (specs, i);
      if (i > 0)
        g_string_append (s, "; ");
      time_spec_print (spec, s);
    }
}

static void
time_spec_add_animation (TimeSpec  *spec,
                         Animation *a)
{
  if (!spec->animations)
    spec->animations = g_ptr_array_new ();
  g_ptr_array_add (spec->animations, a);
}

static void
time_spec_drop_animation (TimeSpec  *spec,
                          Animation *a)
{
  g_ptr_array_remove (spec->animations, a);
}

static void animation_update_for_spec (Animation *a,
                                       TimeSpec  *spec);

static void
time_spec_set_time (TimeSpec *spec,
                    int64_t   time)
{
  if (spec->time == time)
    return;

  spec->time = time;

  if (spec->animations)
    {
      for (unsigned int i = 0; i < spec->animations->len; i++)
        {
          Animation *a = g_ptr_array_index (spec->animations, i);
          animation_update_for_spec (a, spec);
        }
    }
}

static void
time_spec_update_for_load_time (TimeSpec *spec,
                                int64_t   load_time)
{
  if (spec->type == TIME_SPEC_TYPE_OFFSET && spec->time == INDEFINITE)
    time_spec_set_time (spec, load_time + spec->offset);
}

static void
time_spec_update_for_pause (TimeSpec *spec,
                            int64_t   duration)
{
  if (spec->time != INDEFINITE)
    time_spec_set_time (spec, spec->time + duration);
}

static void
time_spec_update_for_state (TimeSpec     *spec,
                            unsigned int  previous_state,
                            unsigned int  state,
                            int64_t       state_start_time)
{
  if (spec->type == TIME_SPEC_TYPE_STATES && previous_state != state)
    {
      gboolean was_in, is_in;
      int64_t time;

      time = spec->time;

      was_in = state_match (spec->states.states, previous_state);
      is_in = state_match (spec->states.states, state);

      if (was_in != is_in)
        {
          if (spec->states.side == TIME_SPEC_SIDE_BEGIN)
            {
              if (!was_in && is_in)
                time = state_start_time + spec->offset;
              else if (was_in && !is_in)
                time = INDEFINITE;
            }
          else if (spec->states.side == TIME_SPEC_SIDE_END)
            {
              if (!was_in && is_in)
                time = INDEFINITE;
              else if (was_in && !is_in)
                time = state_start_time + spec->offset;
            }
        }

      time_spec_set_time (spec, time);
    }
}

static int64_t
time_spec_get_state_change_delay (TimeSpec *spec)
{
  if (spec->type == TIME_SPEC_TYPE_STATES &&
      spec->states.side == TIME_SPEC_SIDE_END)
    return ABS (spec->offset);

  return 0;
}

/* }}} */
/* {{{ Timeline */

struct _Timeline
{
  GPtrArray *times;
};

static Timeline *
timeline_new (void)
{
  Timeline *timeline = g_new (Timeline, 1);
  timeline->times = g_ptr_array_new_with_free_func (time_spec_free);
  return timeline;
}

static void
timeline_free (Timeline *timeline)
{
  g_ptr_array_unref (timeline->times);
  g_free (timeline);
}

static TimeSpec *
timeline_get_time_spec (Timeline *timeline,
                        TimeSpec *spec)
{
  unsigned int idx;

  if (g_ptr_array_find_with_equal_func (timeline->times, spec, time_spec_equal, &idx))
    return g_ptr_array_index (timeline->times, idx);

  spec = time_spec_copy (spec);
  spec->time = INDEFINITE;
  spec->animations = NULL;

  g_ptr_array_add (timeline->times, spec);
  return g_ptr_array_index (timeline->times, timeline->times->len - 1);
}

static TimeSpec *
timeline_get_start_of_time (Timeline *timeline)
{
  TimeSpec spec = { .type = TIME_SPEC_TYPE_OFFSET, .offset = 0, };
  return timeline_get_time_spec (timeline, &spec);
}

static TimeSpec *
timeline_get_end_of_time (Timeline *timeline)
{
  TimeSpec spec = { .type = TIME_SPEC_TYPE_INDEFINITE, };
  return timeline_get_time_spec (timeline, &spec);
}

static TimeSpec *
timeline_get_fixed (Timeline *timeline,
                    int64_t   offset)
{
  TimeSpec spec = { .type = TIME_SPEC_TYPE_OFFSET, .offset = offset, };
  return timeline_get_time_spec (timeline, &spec);
}

static TimeSpec *
timeline_get_sync (Timeline     *timeline,
                   const char   *ref,
                   Animation    *base,
                   TimeSpecSide  side,
                   int64_t       offset)
{
  TimeSpec spec = { .type = TIME_SPEC_TYPE_SYNC,
                    .sync = { .ref = (char *) ref, .base = base, .side = side },
                    .offset = offset };
  return timeline_get_time_spec (timeline, &spec);
}

static TimeSpec *
timeline_get_states (Timeline     *timeline,
                     uint64_t      states,
                     TimeSpecSide  side,
                     int64_t       offset)
{
  TimeSpec spec = { .type = TIME_SPEC_TYPE_STATES,
                    .states = { .states = states, .side = side },
                    .offset = offset };
  return timeline_get_time_spec (timeline, &spec);
}

static void
timeline_set_load_time (Timeline *timeline,
                        int64_t   load_time)
{
  for (unsigned int i = 0; i < timeline->times->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (timeline->times, i);
      time_spec_update_for_load_time (spec, load_time);
    }
}

static void
timeline_update_for_pause (Timeline *timeline,
                           int64_t   duration)
{
  for (unsigned int i = 0; i < timeline->times->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (timeline->times, i);
      time_spec_update_for_pause (spec, duration);
    }
}

static void
timeline_update_for_state (Timeline     *timeline,
                           unsigned int  previous_state,
                           unsigned int  state,
                           int64_t       state_start_time)
{
  for (unsigned int i = 0; i < timeline->times->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (timeline->times, i);
      time_spec_update_for_state (spec, previous_state, state, state_start_time);
    }
}

static int64_t
timeline_get_state_change_delay (Timeline *timeline)
{
  int64_t delay = 0;

  for (unsigned int i = 0; i < timeline->times->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (timeline->times, i);
      delay = MAX (delay, time_spec_get_state_change_delay (spec));
    }

  return delay;
}

/* }}} */
/* {{{ Animation */

typedef enum
{
  ANIMATION_TYPE_SET,
  ANIMATION_TYPE_ANIMATE,
  ANIMATION_TYPE_MOTION,
  ANIMATION_TYPE_TRANSFORM,
} AnimationType;

typedef enum
{
  ANIMATION_FILL_FREEZE,
  ANIMATION_FILL_REMOVE,
} AnimationFill;

typedef enum
{
  ANIMATION_RESTART_ALWAYS,
  ANIMATION_RESTART_WHEN_NOT_ACTIVE,
  ANIMATION_RESTART_NEVER,
} AnimationRestart;

typedef enum
{
  ANIMATION_ADDITIVE_REPLACE,
  ANIMATION_ADDITIVE_SUM,
} AnimationAdditive;

typedef enum
{
  ANIMATION_ACCUMULATE_NONE,
  ANIMATION_ACCUMULATE_SUM,
} AnimationAccumulate;

typedef enum
{
  CALC_MODE_DISCRETE,
  CALC_MODE_LINEAR,
  CALC_MODE_PACED,
  CALC_MODE_SPLINE,
} CalcMode;

typedef enum
{
  ROTATE_AUTO,
  ROTATE_AUTO_REVERSE,
  ROTATE_FIXED,
} AnimationRotate;

typedef enum
{
  ANIMATION_STATUS_INACTIVE,
  ANIMATION_STATUS_RUNNING,
  ANIMATION_STATUS_DONE,
} AnimationStatus;

typedef struct {
  SvgValue *value;
  double time;
  double point;
  double params[4];
} Frame;

typedef struct
{
  int64_t begin;
  int64_t end;
} Activation;

struct _Animation
{
  AnimationType type;
  AnimationStatus status;
  char *id;
  char *href;
  int line; /* for resolving ties in order */

  /* shape, attr, and idx together identify the attribute
   * that this animation modifies. idx is only relevant
   * for gradients and filters, where it identifies the
   * index of the color stop or filter primitive. It is
   * shifted by one, so we can use the value use for the
   * shapes' own attributes without clashes.
   */
  Shape *shape;
  unsigned int attr;
  unsigned int idx;

  unsigned int has_simple_duration : 1;
  unsigned int has_repeat_count    : 1;
  unsigned int has_repeat_duration : 1;
  unsigned int has_begin           : 1;
  unsigned int has_end             : 1;

  GPtrArray *begin;
  GPtrArray *end;

  Activation current;
  Activation previous;

  int64_t simple_duration;
  double repeat_count;
  int64_t repeat_duration;
  ColorInterpolation color_interpolation;

  GtkSvgRunMode run_mode;
  int64_t next_invalidate;
  gboolean state_changed;

  AnimationFill fill;
  AnimationRestart restart;
  AnimationAdditive additive;
  AnimationAccumulate accumulate;

  CalcMode calc_mode;
  Frame *frames;
  unsigned int n_frames;

  struct {
    char *path_ref;
    Shape *path_shape;
    GskPath *path;
    GskPathMeasure *measure;
    AnimationRotate rotate;
    double angle;
  } motion;

  GPtrArray *deps;

  struct {
    unsigned int transition;
    unsigned int animation;
    unsigned int easing;
    double origin;
    double segment;
    double attach_pos;
  } gpa;
};

static CalcMode
default_calc_mode (unsigned int type)
{
  switch (type)
    {
    case ANIMATION_TYPE_SET: return CALC_MODE_DISCRETE;
    case ANIMATION_TYPE_MOTION: return CALC_MODE_PACED;
    default: return CALC_MODE_LINEAR;
    }
}

static void
animation_init (Animation     *a,
                AnimationType  type)
{
  a->status = ANIMATION_STATUS_INACTIVE;

  a->type = type;
  a->begin = NULL;
  a->end = NULL;

  a->simple_duration = INDEFINITE;
  a->repeat_count = REPEAT_FOREVER;
  a->repeat_duration = INDEFINITE;

  a->fill = ANIMATION_FILL_REMOVE;
  a->restart = ANIMATION_RESTART_ALWAYS;
  a->calc_mode = default_calc_mode (type);
  a->additive = ANIMATION_ADDITIVE_REPLACE;
  a->accumulate = ANIMATION_ACCUMULATE_NONE;

  a->color_interpolation = COLOR_INTERPOLATION_SRGB;

  a->begin = g_ptr_array_new ();
  a->end = g_ptr_array_new ();

  a->current.begin = INDEFINITE;
  a->current.end = INDEFINITE;
  a->previous.begin = 0;
  a->previous.end = 0;
}

static void
animation_add_dep (Animation *base,
                   Animation *a)
{
  if (!base->deps)
    base->deps = g_ptr_array_new ();

  g_ptr_array_add (base->deps, a);
}

static void
shape_add_animation (Shape     *shape,
                     Animation *a)
{
  a->shape = shape;
  if (!shape->animations)
    shape->animations = g_ptr_array_new_with_free_func (animation_free);
  g_ptr_array_add (shape->animations, a);
}

static Animation *
animation_set_new (void)
{
  Animation *a = g_new0 (Animation, 1);
  animation_init (a, ANIMATION_TYPE_SET);
  return a;
}

static Animation *
animation_animate_new (void)
{
  Animation *a = g_new0 (Animation, 1);
  animation_init (a, ANIMATION_TYPE_ANIMATE);
  return a;
}

static Animation *
animation_transform_new (void)
{
  Animation *a = g_new0 (Animation, 1);
  animation_init (a, ANIMATION_TYPE_TRANSFORM);
  return a;
}

static Animation *
animation_motion_new (void)
{
  Animation *a = g_new0 (Animation, 1);
  animation_init (a, ANIMATION_TYPE_MOTION);
  a->attr = SHAPE_ATTR_TRANSFORM;
  a->motion.rotate = ROTATE_FIXED;
  a->motion.angle = 0;
  return a;
}

static void
animation_free (gpointer data)
{
  Animation *a = data;

  g_free (a->id);
  g_free (a->href);

  g_clear_pointer (&a->begin, g_ptr_array_unref);
  g_clear_pointer (&a->end, g_ptr_array_unref);

  if (a->type != ANIMATION_TYPE_MOTION)
    {
      for (unsigned int i = 0; i < a->n_frames; i++)
        svg_value_unref (a->frames[i].value);
    }
  g_free (a->frames);

  g_free (a->motion.path_ref);
  g_clear_pointer (&a->motion.path, gsk_path_unref);
  g_clear_pointer (&a->motion.measure, gsk_path_measure_unref);

  g_clear_pointer (&a->deps, g_ptr_array_unref);

  g_free (a);
}

static void
animation_drop_and_free (Animation *a)
{
  for (unsigned int k = 0; k < 2; k++)
    {
      GPtrArray *specs = k == 0 ? a->begin : a->end;
      for (unsigned int i = 0; i < specs->len; i++)
        {
          TimeSpec *spec = g_ptr_array_index (specs, i);
          time_spec_drop_animation (spec, a);
        }
    }
  animation_free (a);
}

static TimeSpec *
animation_add_begin (Animation *a,
                     TimeSpec  *spec)
{
  g_ptr_array_add (a->begin, spec);
  return spec;
}

static TimeSpec *
animation_add_end (Animation *a,
                   TimeSpec  *spec)
{
  g_ptr_array_add (a->end, spec);
  return spec;
}

static gboolean
animation_has_begin (Animation *a,
                     TimeSpec  *spec)
{
  return g_ptr_array_find (a->begin, spec, NULL);
}

static gboolean
animation_has_end (Animation *a,
                   TimeSpec  *spec)
{
  return g_ptr_array_find (a->end, spec, NULL);
}

static void
fill_from_values (Animation     *a,
                  double        *times,
                  unsigned int   n_frames,
                  SvgValue     **values,
                  double        *params,
                  double        *points)
{
  double linear[] = { 0, 0, 1, 1 };

  g_assert (n_frames > 1);

  a->n_frames = n_frames;
  a->frames = g_new0 (Frame, n_frames);

  for (unsigned int i = 0; i < n_frames; i++)
    {
      a->frames[i].time = times[i];
      if (values)
        a->frames[i].value = svg_value_ref (values[i]);
      else
        a->frames[i].value = NULL;
      if (i + 1 < n_frames && params)
        memcpy (a->frames[i].params, &params[4 * i], sizeof (double) * 4);
      else
        memcpy (a->frames[i].params, linear, sizeof (double) * 4);
      if (points)
        a->frames[i].point = points[i];
      else
        a->frames[i].point = i / (double) (n_frames - 1);
    }
}

static void
fill_from_path (Animation *a,
                GskPath   *path)
{
  GArray *frames;
  Frame frame = { .value = NULL, .params = { 0, 0, 1, 1 } };
  Frame *last;
  GskPathPoint point;
  GskPathMeasure *measure;
  double length;
  double distance, previous_distance;

  if (!gsk_path_get_start_point (path, &point))
    return;

  measure = gsk_path_measure_new (path);

  frames = g_array_new (FALSE, FALSE, sizeof (Frame));

  frame.time = 0;
  frame.point = 0;
  g_array_append_val (frames, frame);

  length = gsk_path_measure_get_length (measure);

  previous_distance = 0;
  while (gsk_path_get_next (path, &point))
    {
      distance = gsk_path_point_get_distance (&point, measure);
      frame.point = distance / length;
      frame.time += (distance - previous_distance) / length;
      g_array_append_val (frames, frame);
      previous_distance = distance;
    }

  last = &g_array_index (frames, Frame, frames->len - 1);
  last->point = 1;
  last->time = 1;

  gsk_path_measure_unref (measure);

  a->n_frames = frames->len;
  a->frames = (Frame *) g_array_free (frames, FALSE);
}

static GskPath *
animation_motion_get_path (Animation             *a,
                           const graphene_rect_t *viewport,
                           gboolean               current)
{
  g_assert (a->type == ANIMATION_TYPE_MOTION);

  if (a->motion.path_shape)
    return shape_get_path (a->motion.path_shape, viewport, current);
  else if (a->motion.path)
    return gsk_path_ref (a->motion.path);
  else
    return NULL;
}

static GskPath *
animation_motion_get_base_path (Animation             *a,
                                const graphene_rect_t *viewport)
{
  return animation_motion_get_path (a, viewport, FALSE);
}

static GskPath *
animation_motion_get_current_path (Animation             *a,
                                   const graphene_rect_t *viewport)
{
  return animation_motion_get_path (a, viewport, TRUE);
}

static GskPathMeasure *
animation_motion_get_current_measure (Animation             *a,
                                      const graphene_rect_t *viewport)
{
  g_assert (a->type == ANIMATION_TYPE_MOTION);

  if (a->motion.path_shape)
    {
      return shape_get_current_measure (a->motion.path_shape, viewport);
    }
  else if (a->motion.path)
    {
      if (!a->motion.measure)
        a->motion.measure = gsk_path_measure_new (a->motion.path);
      return gsk_path_measure_ref (a->motion.measure);
    }
  else
    {
      return NULL;
    }
}

static void
animation_update_for_pause (Animation *a,
                            int64_t    duration)
{
  if (a->current.begin != INDEFINITE)
    a->current.begin += duration;
  if (a->current.end != INDEFINITE)
    a->current.end += duration;
  if (a->previous.begin != INDEFINITE)
    a->previous.begin += duration;
  if (a->previous.end != INDEFINITE)
    a->previous.end += duration;
}

static void
animations_update_for_pause (Shape   *shape,
                             int64_t  duration)
{
  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          Animation *a = g_ptr_array_index (shape->animations, i);
          animation_update_for_pause (a, duration);
        }
    }

  if (shape_type_has_shapes (shape->type))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          Shape *sh = g_ptr_array_index (shape->shapes, i);
          animations_update_for_pause (sh, duration);
        }
    }
}

/* }}} */
/* {{{ Animated attributes */

/* Animation works by
 * - updating the current time
 * - walking the shape tree and applying the relevant
 *   animations to produce current values
 * - rendering the next frame based on these values
 *
 * When walking the shape tree, we need to do so in an order
 * that ensures values are computed before they are used by
 * other animations (e.g. a motion animation needs to use the
 * updated path of the shape it uses to compute the next position).
 * This update order is determined at parse time.
 *
 * It is sometimes necessary to compute new values during
 * rendering (e.g. for <use>, where the <use> element replaces
 * the parent of the referred to shape for inheritance purposes).
 *
 * To drive the animation, we compute the next update time
 * based on the characteristics of the animations that we have.
 * E.g. <set> animations only need to update at the start and
 * at the end. Animating discrete values such as enums also
 * only update when the value changes, while continuous
 * interpolation requires an update for every frame.
 */
static SvgValue *
shape_get_current_value (Shape        *shape,
                         unsigned int  attr,
                         unsigned int  idx)
{
  if (idx == 0)
    {
      return shape->current[attr];
    }
  else if (FIRST_STOP_ATTR <= attr && attr <= LAST_STOP_ATTR)
    {
      ColorStop *stop;

      g_assert (shape_type_has_color_stops (shape->type));
      g_assert (idx <= shape->color_stops->len);

      stop = g_ptr_array_index (shape->color_stops, idx - 1);

      return stop->current[color_stop_attr_idx (attr)];
    }
  else if (FIRST_FILTER_ATTR <= attr && attr <= LAST_FILTER_ATTR)
    {
      FilterPrimitive *f;

      g_assert (shape_type_has_filters (shape->type));
      g_assert (idx <= shape->filters->len);

      f = g_ptr_array_index (shape->filters, idx - 1);

      return f->current[filter_attr_idx (f->type, attr)];
    }
  else
    g_assert_not_reached ();
}

/* We pass parent separately instead of relying
 * on shape->parent because <use> overrides parent
 */
static SvgValue *
shape_ref_base_value (Shape        *shape,
                      Shape        *parent,
                      ShapeAttr     attr,
                      unsigned int  idx)
{
  if (idx == 0)
    {
      if (!_gtk_bitmask_get (shape->attrs, attr))
        {
          if (shape->type == SHAPE_RADIAL_GRADIENT)
            {
              if (attr == SHAPE_ATTR_FX)
                return shape_ref_base_value (shape, parent, SHAPE_ATTR_CX, idx);
              else if (attr == SHAPE_ATTR_FY)
                return shape_ref_base_value (shape, parent, SHAPE_ATTR_CY, idx);
            }

          if (parent && shape_attr_is_inherited (attr))
            return svg_value_ref (parent->current[attr]);
          else
            return shape_attr_ref_initial_value (attr, shape->type, parent != NULL);
        }
      else if (svg_value_is_inherit (shape->base[attr]))
        {
          if (parent)
            return svg_value_ref (parent->current[attr]);
          else
            return shape_attr_ref_initial_value (attr, shape->type, parent != NULL);
        }
      else if (svg_value_is_initial (shape->base[attr]))
        {
          return shape_attr_ref_initial_value (attr, shape->type, parent != NULL);
        }
      else
        {
          return svg_value_ref (shape->base[attr]);
        }
    }
  else if (FIRST_STOP_ATTR <= attr && attr <= LAST_STOP_ATTR)
    {
      ColorStop *stop;
      unsigned int pos;
      SvgValue *value;

      g_assert (shape_type_has_color_stops (shape->type));
      g_assert (idx <= shape->color_stops->len);

      stop = g_ptr_array_index (shape->color_stops, idx - 1);

      pos = color_stop_attr_idx (attr);
      value = stop->base[pos];

      if ((stop->attrs & BIT (pos)) == 0)
        {
          if (shape_attr_is_inherited (attr))
            return svg_value_ref (shape->current[attr]);
          else
            return shape_attr_ref_initial_value (attr, shape->type, parent != NULL);
        }
      else if (svg_value_is_inherit (value))
        {
          return svg_value_ref (shape->current[attr]);
        }
      else if (svg_value_is_initial (value))
        {
          return shape_attr_ref_initial_value (attr, shape->type, parent != NULL);
        }
      else
        {
          return svg_value_ref (value);
        }
    }
  else if (FIRST_FILTER_ATTR <= attr && attr <= LAST_FILTER_ATTR)
    {
      FilterPrimitive *f;
      unsigned int pos;
      SvgValue *value;

      g_assert (shape_type_has_filters (shape->type));
      g_assert (idx <= shape->filters->len);

      f = g_ptr_array_index (shape->filters, idx - 1);

      pos = filter_attr_idx (f->type, attr);
      value = f->base[pos];

      if ((f->attrs & BIT (pos)) == 0)
        {
          if (shape_attr_is_inherited (attr))
            return svg_value_ref (shape_get_current_value (shape, attr, 0));
          else
            return shape_attr_ref_initial_value (attr, shape->type, parent != NULL);
        }
      else if (svg_value_is_inherit (value))
        {
          return svg_value_ref (shape_get_current_value (shape, attr, 0));
        }
      else if (svg_value_is_initial (value))
        {
          return shape_attr_ref_initial_value (attr, shape->type, parent != NULL);
        }
      else
        {
          return svg_value_ref (value);
        }
    }
  else
    g_assert_not_reached ();
}

static void
shape_set_base_value (Shape        *shape,
                      ShapeAttr     attr,
                      unsigned int  idx,
                      SvgValue     *value)
{
  if (idx == 0)
    {
      g_clear_pointer (&shape->base[attr], svg_value_unref);
      shape->base[attr] = svg_value_ref (value);
      shape->attrs = _gtk_bitmask_set (shape->attrs, attr, TRUE);
    }
  else if (FIRST_STOP_ATTR <= attr && attr <= LAST_STOP_ATTR)
    {
      ColorStop *stop;
      unsigned int pos;

      g_assert (shape_type_has_color_stops (shape->type));
      g_assert (idx <= shape->color_stops->len);

      stop = g_ptr_array_index (shape->color_stops, idx - 1);
      pos = color_stop_attr_idx (attr);
      g_clear_pointer (&stop->base[pos], svg_value_unref);
      stop->base[pos] = svg_value_ref (value);
      stop->attrs |= BIT (pos);
    }
  else if (FIRST_FILTER_ATTR <= attr && attr <= LAST_FILTER_ATTR)
    {
      FilterPrimitive *f;
      unsigned int pos;

      g_assert (shape_type_has_filters (shape->type));
      g_assert (idx <= shape->filters->len);

      f = g_ptr_array_index (shape->filters, idx - 1);
      pos = filter_attr_idx (f->type, attr);
      g_clear_pointer (&f->base[pos], svg_value_unref);
      f->base[pos] = svg_value_ref (value);
      f->attrs |= BIT (pos);
    }
  else
    g_assert_not_reached ();
}

static void
shape_set_current_value (Shape        *shape,
                         ShapeAttr     attr,
                         unsigned int  idx,
                         SvgValue     *value)
{
  if (idx == 0)
    {
      if (value)
        svg_value_ref (value);
      g_clear_pointer (&shape->current[attr], svg_value_unref);
      shape->current[attr] = value;
    }
  else if (FIRST_STOP_ATTR <= attr && attr <= LAST_STOP_ATTR)
    {
      ColorStop *stop;

      g_assert (shape_type_has_color_stops (shape->type));

      stop = g_ptr_array_index (shape->color_stops, idx - 1);

      if (value)
        svg_value_ref (value);
      g_clear_pointer (&stop->current[color_stop_attr_idx (attr)], svg_value_unref);
      stop->current[color_stop_attr_idx (attr)] = value;
    }
  else if (FIRST_FILTER_ATTR <= attr && attr <= LAST_FILTER_ATTR)
    {
      FilterPrimitive *f;

      g_assert (shape_type_has_filters (shape->type));

      f = g_ptr_array_index (shape->filters, idx - 1);

      if (value)
        svg_value_ref (value);
      g_clear_pointer (&f->current[filter_attr_idx (f->type, attr)], svg_value_unref);
      f->current[filter_attr_idx (f->type, attr)] = value;
    }
  else
    g_assert_not_reached ();
}

/* }}} */
/* {{{ Update computation */

static int64_t
determine_repeat_duration (Animation *a)
{
  if (a->repeat_duration < INDEFINITE)
    return a->repeat_duration;
  else if (a->simple_duration < INDEFINITE && a->repeat_count != REPEAT_FOREVER)
    return a->simple_duration * a->repeat_count;
  else if (a->current.end < INDEFINITE)
    return a->current.end - a->current.begin;
  else if (a->simple_duration < INDEFINITE)
    return a->simple_duration;

  return INDEFINITE;
}

static int64_t
determine_simple_duration (Animation *a)
{
  int64_t repeat_duration;

  if (a->simple_duration < INDEFINITE)
    return a->simple_duration;

  repeat_duration = determine_repeat_duration (a);

  if (repeat_duration < INDEFINITE && a->repeat_count != REPEAT_FOREVER)
    return (int64_t) (repeat_duration / a->repeat_count);

  return INDEFINITE;
}

/* Determine what repetition the time falls into,
 * relative to the animations current start time.
 * Also what frame we are on, and how far into that
 * frame we are.
 *
 * Note: this assumes that the duration is finite
 * and the animation runs forever.
 */
static void
find_current_cycle_and_frame (Animation      *a,
                              GtkSvg         *svg,
                              int64_t         time,
                              int            *out_rep,
                              unsigned int   *out_frame,
                              double         *out_frame_t,
                              int64_t        *out_frame_start,
                              int64_t        *out_frame_end)
{
  int64_t start;
  int64_t simple_duration;
  double t;
  int rep;
  int64_t cycle_start, cycle_end;
  int64_t frame_start, frame_end;
  unsigned int i;

  start = a->current.begin;
  //g_assert (start < INDEFINITE);

  simple_duration = determine_simple_duration (a);
  if (simple_duration == INDEFINITE)
    simple_duration = determine_repeat_duration (a);

  if (simple_duration == INDEFINITE || simple_duration == 0)
    {
      if (svg)
        gtk_svg_update_error (svg, "Not enough data to advance animation %s", a->id);
      *out_rep = 0;
      *out_frame = 0;
      *out_frame_t = 0;
      *out_frame_start = a->current.begin;
      *out_frame_end = a->current.end;
      return;
    }

  t = (time - start) / (double) simple_duration;
  rep = floor (t); /* number of completed repetitions */

  cycle_start = start + rep * simple_duration;
  cycle_end = cycle_start + simple_duration;

  frame_start = frame_end = cycle_start;
  for (i = 0; i + 1 < a->n_frames; i++)
    {
      frame_start = frame_end;
      frame_end = lerp (a->frames[i + 1].time, cycle_start, cycle_end);

      if (time < frame_end)
        break;
    }

  t = (time - frame_start) / (double) (frame_end - frame_start);

  *out_rep = rep;
  *out_frame = i;
  *out_frame_t = t;
  *out_frame_start = frame_start;
  *out_frame_end = frame_end;
}

static void
animation_update_run_mode (Animation *a,
                           int64_t    current_time)
{
  if (a->status == ANIMATION_STATUS_INACTIVE)
    {
      a->run_mode = GTK_SVG_RUN_MODE_DISCRETE;
      a->next_invalidate = a->current.begin;
    }
  else if (a->status == ANIMATION_STATUS_RUNNING)
    {
      int rep;
      unsigned int frame;
      double frame_t;
      int64_t frame_start, frame_end;

      if (a->type == ANIMATION_TYPE_SET)
        {
          a->run_mode = GTK_SVG_RUN_MODE_DISCRETE;
          a->next_invalidate = a->current.end;
          return;
        }

      /* FIXME: get svg here */
      find_current_cycle_and_frame (a, NULL, current_time,
                                    &rep, &frame, &frame_t, &frame_start, &frame_end);

      if (a->calc_mode == CALC_MODE_DISCRETE)
        {
          a->run_mode = GTK_SVG_RUN_MODE_DISCRETE;
          a->next_invalidate = frame_end;
        }
      else if (shape_attr_is_discrete (a->attr))
        {
          a->run_mode = GTK_SVG_RUN_MODE_DISCRETE;
          if (frame_t < 0.5)
            a->next_invalidate = (frame_start + frame_end) / 2;
          else
            a->next_invalidate = frame_end;
        }
      else
        {
          a->run_mode = GTK_SVG_RUN_MODE_CONTINUOUS;
          a->next_invalidate = a->current.end;
        }
    }
  else if (a->current.begin < INDEFINITE && current_time <= a->current.begin)
    {
      a->run_mode = GTK_SVG_RUN_MODE_DISCRETE;
      a->next_invalidate = a->current.begin;
    }
  else
    {
      a->run_mode = GTK_SVG_RUN_MODE_STOPPED;
      a->next_invalidate = INDEFINITE;
    }
}

static int64_t
find_first_time (GPtrArray *specs,
                 int64_t    after)
{
  int64_t first = INDEFINITE;
  int64_t slop = G_TIME_SPAN_MILLISECOND;

  for (unsigned int i = 0; i < specs->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (specs, i);

      if (after <= spec->time + slop && spec->time < first)
        first = spec->time;
    }

  return first;
}

static gboolean
animation_set_current_end (Animation *a,
                           int64_t    time)
{
  /* FIXME take min, max into account */
  if (time < a->current.begin)
    time = a->current.begin;

  if (a->current.begin < INDEFINITE && a->repeat_duration < INDEFINITE)
    time = MIN (time, a->current.begin + a->repeat_duration);

  if (a->current.end == time)
    return FALSE;

  dbg_print ("times", "current end time of %s set to %s\n", a->id, format_time (time));
  a->current.end = time;
  return TRUE;
}

static void
animation_update_state (Animation *a,
                        int64_t    current_time)
{
  AnimationStatus status = a->status;

  if (a->status == ANIMATION_STATUS_INACTIVE ||
      a->status == ANIMATION_STATUS_DONE)
    {
      if (current_time < a->current.begin)
        ;  /* remain inactive */
      else if (current_time <= a->current.end)
        status = ANIMATION_STATUS_RUNNING;
      else
        status = ANIMATION_STATUS_DONE;
    }
  else if (a->status == ANIMATION_STATUS_RUNNING)
    {
      if (current_time >= a->current.end)
        status = ANIMATION_STATUS_DONE;
    }

  if (a->status != status)
    {
      if (a->status == ANIMATION_STATUS_RUNNING) /* Ending this activation */
        a->previous = a->current;

      a->status = status;

      if (a->status != ANIMATION_STATUS_RUNNING)
        {
          a->current.begin = find_first_time (a->begin, current_time);
          animation_set_current_end (a, find_first_time (a->end, a->current.begin));
        }

      animation_update_run_mode (a, current_time);

      a->state_changed = TRUE;

#ifdef DEBUG
      if (strstr (g_getenv ("SVG_DEBUG") ?:"", "state"))
        {
          const char *name[] = { "INACTIVE", "RUNNING ", "DONE    " };
          GString *s = g_string_new ("");
          g_string_append_printf (s, "state of %s now %s [", a->id, name[status]);
          g_string_append_printf (s, "%s ", format_time (a->current.begin));
          g_string_append_printf (s, "%s] ", format_time (a->current.end));
          switch (a->run_mode)
            {
            case GTK_SVG_RUN_MODE_CONTINUOUS:
              g_string_append_printf (s, "--> %s\n", format_time (a->next_invalidate));
              break;
            case GTK_SVG_RUN_MODE_DISCRETE:
              g_string_append_printf (s, "> > %s\n", format_time (a->next_invalidate));
              break;
            case GTK_SVG_RUN_MODE_STOPPED:
              g_string_append (s, "\n");
              break;
            default:
              g_assert_not_reached ();
            }
          dbg_print ("state", "%s", s->str);
          g_string_free (s, TRUE);
        }
#endif
    }
  else
    animation_update_run_mode (a, current_time);
}

static void
time_spec_update_for_base (TimeSpec  *spec,
                           Animation *base)
{
  if (spec->type == TIME_SPEC_TYPE_SYNC && spec->sync.base == base)
    {
      if (spec->sync.side == TIME_SPEC_SIDE_BEGIN)
        time_spec_set_time (spec, base->current.begin + spec->offset);
      else
        time_spec_set_time (spec, base->current.end + spec->offset);
    }
}

static void
time_specs_update_for_base (GPtrArray *specs,
                            Animation *base)
{
  for (unsigned int i = 0; i < specs->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (specs, i);
      time_spec_update_for_base (spec, base);
    }
}

static gboolean
animation_can_start (Animation *a)
{
  /* Handling restart */
  switch (a->status)
    {
    case ANIMATION_STATUS_INACTIVE:
      break;
    case ANIMATION_STATUS_RUNNING:
      if (a->restart != ANIMATION_RESTART_ALWAYS)
        return FALSE;
      break;
    case ANIMATION_STATUS_DONE:
      if (a->restart == ANIMATION_RESTART_NEVER)
        return FALSE;
      break;
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
animation_update_for_spec (Animation *a,
                           TimeSpec  *spec)
{
  gboolean changed = FALSE;

  if (animation_has_begin (a, spec))
    {
      if (!animation_can_start (a))
        return;

      if (a->status == ANIMATION_STATUS_RUNNING)
        {
          if (a->current.begin < spec->time && spec->time < INDEFINITE)
            {
              dbg_print ("status", "Restarting %s at %s\n", a->id, format_time (spec->time));
              a->current.begin = spec->time;
              changed = TRUE;
            }
        }
      else
        {
          int64_t time;

          /* We need to allow starting in the past. But we don't allow
           * a post-dated start to fall into a previous activation.
           */

         time = find_first_time (a->begin, a->previous.end);

          if (a->current.begin != time)
            {
              dbg_print ("times", "Current start time of %s now %s\n", a->id, format_time (time));
              a->current.begin = time;
              changed = TRUE;

              animation_set_current_end (a, a->current.end);
            }
        }
    }

  if (animation_has_end (a, spec))
    {
      int64_t end = find_first_time (a->end, a->current.begin);

      changed = animation_set_current_end (a, end);
    }

  if (!changed)
    return;

  if (a->deps)
    {
      for (unsigned int i = 0; i < a->deps->len; i++)
        {
          Animation *dep = g_ptr_array_index (a->deps, i);
          time_specs_update_for_base (dep->begin, a);
          time_specs_update_for_base (dep->end, a);
        }
    }
}

static void frame_clock_connect    (GtkSvg *self);
static void frame_clock_disconnect (GtkSvg *self);

static void schedule_next_update (GtkSvg *self);

static void
invalidate_later (gpointer data)
{
  GtkSvg *self = data;

  self->pending_invalidate = 0;

  gtk_svg_advance (self, MAX (self->current_time, g_get_monotonic_time ()));
}

static void
schedule_next_update (GtkSvg *self)
{
  GtkSvgRunMode run_mode;

  if (self->clock == NULL || !self->playing)
    return;

  g_clear_handle_id (&self->pending_invalidate, g_source_remove);

  run_mode = self->run_mode;
#ifdef DEBUG
  if (strstr (g_getenv ("SVG_DEBUG") ?: "", "continuous"))
    run_mode = GTK_SVG_RUN_MODE_CONTINUOUS;
#endif

  if (run_mode == GTK_SVG_RUN_MODE_CONTINUOUS)
    {
      frame_clock_connect (self);
      return;
    }
  else
    {
      frame_clock_disconnect (self);
    }

  if (self->next_update <= self->current_time)
    {
      dbg_print ("times", "next update NOW (%s)\n", format_time (self->current_time));
      self->advance_after_snapshot = TRUE;
      return;
    }

  if (self->next_update < INDEFINITE)
    {
      int64_t interval = (self->next_update - self->current_time) / (double) G_TIME_SPAN_MILLISECOND;

      dbg_print ("times", "next update in %" G_GINT64_FORMAT "ms\n", interval);
      self->pending_invalidate = g_timeout_add_once (interval, invalidate_later, self);
    }
  else
    {
      dbg_print ("times", "next update NEVER\n");
    }
}

/* }}} */
/* {{{ Frame-clock driven updates */

static void
frame_clock_update (GdkFrameClock *clock,
                    GtkSvg        *self)
{
  int64_t time = gdk_frame_clock_get_frame_time (self->clock);
  dbg_print ("clock", "clock update, advancing to %s\n", format_time (time));
  gtk_svg_advance (self, time);
}

static void
frame_clock_connect (GtkSvg *self)
{
  if (self->clock && self->clock_update_id == 0)
    {
      self->clock_update_id = g_signal_connect (self->clock, "update",
                                                G_CALLBACK (frame_clock_update), self);
      gdk_frame_clock_begin_updating (self->clock);
    }
}

static void
frame_clock_disconnect (GtkSvg *self)
{
  if (self->clock && self->clock_update_id != 0)
    {
      gdk_frame_clock_end_updating (self->clock);
      g_clear_signal_handler (&self->clock_update_id, self->clock);
    }
}

/* }}} */
/* {{{ Value updates */

static void
get_transform_data_for_motion (GskPathMeasure   *measure,
                               double            offset,
                               AnimationRotate   rotate,
                               double           *angle,
                               graphene_point_t *pos)
{
  GskPath *path;
  GskPathPoint point;

  path = gsk_path_measure_get_path (measure);

  if (offset == 0)
    {
      gsk_path_get_start_point (path, &point);
    }
  else if (offset == 1)
    {
      gsk_path_get_end_point (path, &point);
    }
  else
    {
      double length = gsk_path_measure_get_length (measure);
      gsk_path_measure_get_point (measure, length * offset, &point);
    }

  gsk_path_point_get_position (&point, path, pos);

  switch (rotate)
    {
    case ROTATE_FIXED:
      break;
    case ROTATE_AUTO:
      *angle = gsk_path_point_get_rotation (&point, path, GSK_PATH_TO_END);
      break;
    case ROTATE_AUTO_REVERSE:
      *angle = gsk_path_point_get_rotation (&point, path, GSK_PATH_TO_START);
      break;
    default:
      g_assert_not_reached ();
    }
}

static SvgValue *
resolve_value (Shape           *shape,
               ComputeContext  *context,
               ShapeAttr       attr,
               unsigned int    idx,
               SvgValue       *value)
{
  if (svg_value_is_initial (value))
    {
      SvgValue *v, *ret;

      if (idx > 0 && shape->type == SHAPE_FILTER)
        v = filter_attr_ref_initial_value (g_ptr_array_index (shape->filters, idx - 1), attr);
      else
        v = shape_attr_ref_initial_value (attr, shape->type, context->parent != NULL);
      ret = svg_value_resolve (v, attr, idx, shape, context);
      svg_value_unref (v);
      return ret;
    }
  else if (svg_value_is_inherit (value))
    {
      if (idx > 0)
        return svg_value_ref (shape->current[attr]);
      else if (context->parent && shape_has_attr (context->parent->type, attr))
        return svg_value_ref (context->parent->current[attr]);
      else
        {
          SvgValue *v, *ret;

          v = shape_attr_ref_initial_value (attr, shape->type, context->parent != NULL);
          ret = svg_value_resolve (v, attr, idx, shape, context);
          svg_value_unref (v);
          return ret;
        }
    }
  else if (svg_value_is_current (value))
    {
      if (idx > 0)
        {
          if (shape->type == SHAPE_FILTER)
            {
              FilterPrimitive *fp = g_ptr_array_index (shape->filters, idx - 1);

              return svg_value_ref (fp->current[filter_attr_idx (fp->type, attr)]);
            }
          else
            {
              ColorStop *cs = g_ptr_array_index (shape->color_stops, idx - 1);
              return svg_value_ref (cs->current[color_stop_attr_idx (attr)]);
            }
        }
      else
        return svg_value_ref (shape->current[attr]);
    }
  else if (svg_value_is_auto (value))
    {
      if (attr == SHAPE_ATTR_WIDTH || attr == SHAPE_ATTR_HEIGHT)
        {
          if (shape->type == SHAPE_SVG)
            {
              return svg_value_resolve (svg_percentage_new (100), attr, idx, shape, context);
            }
          else if (shape->type == SHAPE_IMAGE)
            {
              SvgHref *href = (SvgHref *) shape->current[SHAPE_ATTR_HREF];

              if (!href->texture)
                return svg_number_new (0);
              else if (attr == SHAPE_ATTR_WIDTH)
                return svg_number_new (gdk_texture_get_width (href->texture));
              else
                return svg_number_new (gdk_texture_get_height (href->texture));
            }
          else
            {
              return svg_number_new (0);
            }
        }
      else if (attr == SHAPE_ATTR_RX || attr == SHAPE_ATTR_RY)
        {
          return svg_value_ref (value); /* handled in shape_get_current_path */
        }
      else
        {
          g_assert_not_reached ();
        }
    }
  else
    {
      return svg_value_resolve (value, attr, idx, shape, context);
    }
}

static SvgValue *
compute_animation_motion_value (Animation      *a,
                                unsigned int    rep,
                                unsigned int    frame,
                                double          frame_t,
                                ComputeContext *context)
{
  double angle;
  graphene_point_t orig_pos, final_pos;
  SvgValue *value;
  GskPathMeasure *measure;

  graphene_point_init (&orig_pos, 0, 0);

  /* svg does not have an origin for this,
   * but we want to have one for gpa
   */
  if (a->id && g_str_has_prefix (a->id, "gpa:"))
    {
      measure = shape_get_current_measure (a->shape, context->viewport);
      get_transform_data_for_motion (measure, a->gpa.origin, ROTATE_FIXED, &angle, &orig_pos);
      gsk_path_measure_unref (measure);
    }

  angle = a->motion.angle;

  measure = animation_motion_get_current_measure (a, context->viewport);
  if (measure)
    {
      double offset;

      if (frame + 1 < a->n_frames)
        offset = lerp (frame_t, a->frames[frame].point, a->frames[frame + 1].point);
      else
        offset = a->frames[frame].point;

      if (offset < 0 || offset > 1)
        {
          offset = fmod (offset, 1);
          if (offset < 9)
            offset += 1;
        }

      get_transform_data_for_motion (measure, offset, a->motion.rotate, &angle, &final_pos);
      value = svg_transform_new_rotate_and_shift (angle, &orig_pos, &final_pos);
    }
  else
    {
      if (frame + 1 == a->n_frames)
        {
          value = resolve_value (a->shape, context, a->attr, a->idx, a->frames[frame].value);
        }
      else if (a->frames[0].value)
        {
          SvgValue *v1 = resolve_value (a->shape, context, a->attr, a->idx, a->frames[frame].value);
          SvgValue *v2 = resolve_value (a->shape, context, a->attr, a->idx, a->frames[frame + 1].value);
          value = svg_value_interpolate (v1, v2, context, frame_t);
          svg_value_unref (v1);
          svg_value_unref (v2);
        }
      else
        {
          value = svg_transform_new_none ();
        }
    }

  if (a->accumulate == ANIMATION_ACCUMULATE_SUM && rep > 0)
    {
      SvgValue *end_val, *acc;

      if (measure)
        {
          get_transform_data_for_motion (measure, 1, a->motion.rotate, &angle, &final_pos);
          end_val = svg_transform_new_rotate_and_shift (angle, &orig_pos, &final_pos);
        }
      else if (a->frames[0].value)
        {
          end_val = svg_value_ref (a->frames[a->n_frames - 1].value);
        }
      else
        {
          end_val = svg_transform_new_none ();
        }
      acc = svg_value_accumulate (value, end_val, context, rep);
      svg_value_unref (end_val);
      svg_value_unref (value);
      value = acc;
    }

  g_clear_pointer (&measure, gsk_path_measure_unref);

  return value;
}

static SvgValue *
compute_value_at_time (Animation      *a,
                       ComputeContext *context)
{
  int rep;
  unsigned int frame;
  double frame_t;
  int64_t frame_start, frame_end;

  if (a->type == ANIMATION_TYPE_SET)
    return resolve_value (a->shape, context, a->attr, a->idx, a->frames[0].value);

  find_current_cycle_and_frame (a, context->svg, context->current_time,
                                &rep, &frame, &frame_t, &frame_start, &frame_end);

  if (a->calc_mode == CALC_MODE_DISCRETE || shape_attr_is_discrete (a->attr))
    {
      if (a->calc_mode != CALC_MODE_DISCRETE && frame_t > 0.5)
        frame = frame + 1;

      if (a->attr != SHAPE_ATTR_TRANSFORM || a->type != ANIMATION_TYPE_MOTION)
        return resolve_value (a->shape, context, a->attr, a->idx, a->frames[frame].value);
      else
        return compute_animation_motion_value (a, rep, frame, 0, context);
    }

  /* Now we are doing non-discrete linear or spline interpolation */
  if (a->calc_mode == CALC_MODE_SPLINE)
    frame_t = ease (a->frames[frame].params, frame_t);

  if (a->attr != SHAPE_ATTR_TRANSFORM || a->type != ANIMATION_TYPE_MOTION)
    {
      SvgValue *ival;

      if (frame + 1 == a->n_frames)
        ival = resolve_value (a->shape, context, a->attr, a->idx, a->frames[frame].value);
      else
        {
          SvgValue *v1 = resolve_value (a->shape, context, a->attr, a->idx, a->frames[frame].value);
          SvgValue *v2 = resolve_value (a->shape, context, a->attr, a->idx, a->frames[frame + 1].value);
          ival = svg_value_interpolate (v1, v2, context, frame_t);
          svg_value_unref (v1);
          svg_value_unref (v2);
        }
      if (ival == NULL)
        {
          gtk_svg_update_error (context->svg,
                                "Failed to interpolate %s value (animation %s)",
                                shape_attr_get_presentation (a->attr, a->shape->type),
                                a->id);
          ival = resolve_value (a->shape, context, a->attr, a->idx, a->frames[frame].value);
        }

      if (a->accumulate == ANIMATION_ACCUMULATE_SUM && rep > 0)
        {
          SvgValue *aval;

          SvgValue *v = resolve_value (a->shape, context, a->attr, a->idx, a->frames[a->n_frames - 1].value);
          aval = svg_value_accumulate (ival, v, context, rep);
          svg_value_unref (v);

          if (aval == NULL)
            {
              gtk_svg_update_error (context->svg,
                                    "Failed to accumulate %s value (animation %s)",
                                    shape_attr_get_presentation (a->attr, a->shape->type),
                                    a->id);
              aval = svg_value_ref (ival);
            }

          svg_value_unref (ival);

          return aval;
        }
      else
        {
          return ival;
        }
    }
  else
    {
      return compute_animation_motion_value (a, rep, frame, frame_t, context);
    }
}

static SvgValue *
compute_value_for_animation (Animation      *a,
                             ComputeContext *context)
{
  GdkColorState *previous = context->interpolation;
  SvgValue *value = NULL;

  if (a->color_interpolation == COLOR_INTERPOLATION_LINEAR)
    context->interpolation = GDK_COLOR_STATE_SRGB_LINEAR;
  else
    context->interpolation = GDK_COLOR_STATE_SRGB;

  if (a->status == ANIMATION_STATUS_INACTIVE)
    {
      /* keep the base value */
      dbg_print ("values", "%s: too early\n", a->id);
    }
  else if (a->status == ANIMATION_STATUS_RUNNING)
    {
      /* animation is active */
      dbg_print ("values", "%s: updating value\n", a->id);
      value = compute_value_at_time (a, context);
    }
  else if (a->fill == ANIMATION_FILL_FREEZE)
    {
      /* keep the last value */
      if (a->repeat_count == 1)
        {
          if (!(a->attr == SHAPE_ATTR_TRANSFORM && a->type == ANIMATION_TYPE_MOTION))
            {
              dbg_print ("values", "%s: frozen (fast)\n", a->id);
              value = resolve_value (a->shape, context, a->attr, a->idx, a->frames[a->n_frames - 1].value);
            }
          else
           {
              dbg_print ("values", "%s: frozen (motion)\n", a->id);
              value = compute_animation_motion_value (a, 1, a->n_frames - 1, 0, context);
           }
        }
      else
        {
          dbg_print ("values", "%s: frozen\n", a->id);
          value = compute_value_at_time (a, context);
        }
    }
  else
    {
      /* Back to base value */
      dbg_print ("values", "%s: back to base\n", a->id);
    }

  context->interpolation = previous;

  return value;
}

static int64_t
get_last_start (Animation *a)
{
  if (a->status == ANIMATION_STATUS_DONE)
    return a->previous.begin;
  else
    return a->current.begin;
}

static int
compare_anim (gconstpointer a,
              gconstpointer b)
{
  Animation *a1 = (Animation *) a;
  Animation *a2 = (Animation *) b;
  int64_t start1, start2;

  /* It doesn't matter, but we sort by attributes to get
   * a predictable order
   */
  if (a1->attr < a2->attr)
    return -1;
  else if (a1->attr > a2->attr)
    return 1;

  /* The situation with animateTransform sv animateMotion
   * is special: they don't add to each other, and
   * the accumulate animateMotion transform is always
   * applied after the accumulate animateTransform.
   */
  if (a1->attr == SHAPE_ATTR_TRANSFORM)
    {
      if (a1->type == ANIMATION_TYPE_MOTION && a2->type != ANIMATION_TYPE_MOTION)
        return 1;
      else if (a1->type != ANIMATION_TYPE_MOTION && a2->type == ANIMATION_TYPE_MOTION)
        return -1;
    }

  /* Animation priorities:
   * - started later > started earlier
   * - later in document > earlier in document
   *
   * Sort higher priority animations to the end
   * so their effect overrides lower priority ones.
   */
  start1 = get_last_start (a1);
  start2 = get_last_start (a2);

  if (start1 < start2)
    return -1;
  else if (start1 > start2)
    return 1;

  if (a1->line < a2->line)
    return -1;
  else if (a1->line > a2->line)
    return 1;

  return 0;
}

static void
shape_init_current_values (Shape          *shape,
                           ComputeContext *context)
{
  for (ShapeAttr attr = FIRST_SHAPE_ATTR; attr <= LAST_FILTER_ATTR; attr++)
    {
      if (shape_has_attr (shape->type, attr))
        {
          SvgValue *base;
          SvgValue *value;

          base = shape_ref_base_value (shape, context->parent, attr, 0);
          value = resolve_value (shape, context, attr, 0, base);
          shape_set_current_value (shape, attr, 0, value);
          svg_value_unref (value);
          svg_value_unref (base);
        }
    }

  if (shape_type_has_color_stops (shape->type))
    {
      for (unsigned int idx = 0; idx < shape->color_stops->len; idx++)
        {
          for (ShapeAttr attr = FIRST_STOP_ATTR; attr <= LAST_STOP_ATTR; attr++)
            {
              SvgValue *base;
              SvgValue *value;

              base = shape_ref_base_value (shape, context->parent, attr, idx + 1);
              value = resolve_value (shape, context, attr, idx + 1, base);
              shape_set_current_value (shape, attr, idx + 1, value);
              svg_value_unref (value);
              svg_value_unref (base);
            }
        }
    }

  if (shape_type_has_filters (shape->type))
    {
      for (unsigned int idx = 0; idx < shape->filters->len; idx++)
        {
          FilterPrimitive *f = g_ptr_array_index (shape->filters, idx);

          for (unsigned int i = 0; i < filter_primitive_get_n_attrs (f->type); i++)
            {
              ShapeAttr attr = filter_types[f->type].attrs[i];
              SvgValue *base;
              SvgValue *value;

              base = shape_ref_base_value (shape, context->parent, attr, idx + 1);
              value = resolve_value (shape, context, attr, idx + 1, base);
              shape_set_current_value (shape, attr, idx + 1, value);
              svg_value_unref (value);
              svg_value_unref (base);
            }
        }
    }
}

static void
mark_as_computed_for_use (Shape    *shape,
                          gboolean  computed_for_use)
{
  shape->computed_for_use = computed_for_use;

  if (shape_type_has_shapes (shape->type))
    {
      for (Shape *sh = shape->first; sh; sh = sh->next)
        mark_as_computed_for_use (sh, computed_for_use);
    }
}

static void
compute_current_values_for_shape (Shape          *shape,
                                  ComputeContext *context)
{
  const graphene_rect_t *old_viewport = context->viewport;
  graphene_rect_t viewport;

  shape_init_current_values (shape, context);

  if (shape->type == SHAPE_SVG || shape->type == SHAPE_SYMBOL)
    {
      SvgViewBox *vb = (SvgViewBox *) shape->current[SHAPE_ATTR_VIEW_BOX];
      double width, height;

      if (shape->parent == NULL)
        {
          width = svg_number_get (shape->current[SHAPE_ATTR_WIDTH], context->svg->current_width);
          height = svg_number_get (shape->current[SHAPE_ATTR_HEIGHT], context->svg->current_height);

        }
      else
        {
          // FIXME use parent override for symbol
          g_assert (context->viewport != NULL);

          width = svg_number_get (shape->current[SHAPE_ATTR_WIDTH], context->viewport->size.width);
          height = svg_number_get (shape->current[SHAPE_ATTR_HEIGHT], context->viewport->size.height);
        }

      if (vb->unset)
        graphene_rect_init (&viewport, 0, 0, width, height);
      else
        graphene_rect_init_from_rect (&viewport, &vb->view_box);

      context->viewport = &viewport;
    }

  if (shape->animations)
    {
      SvgValue *identity, *motion;

      identity = svg_transform_new_none ();
      motion = svg_value_ref (identity);

      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          Animation *a = g_ptr_array_index (shape->animations, i);
          SvgValue *val;

          val = compute_value_for_animation (a, context);
          if (val)
            {
              if (a->additive == ANIMATION_ADDITIVE_SUM)
                {
                  SvgValue *end_val;

                  if (a->type == ANIMATION_TYPE_MOTION)
                    {
                      end_val = svg_value_accumulate (val, motion, context, 1);
                      svg_value_unref (motion);
                      motion = end_val;
                    }
                  else
                    {
                      end_val = svg_value_accumulate (val, shape_get_current_value (shape, a->attr, a->idx), context, 1);
                      shape_set_current_value (shape, a->attr, a->idx, end_val);
                      svg_value_unref (end_val);
                    }
                }
              else
                {
                  if (a->type == ANIMATION_TYPE_MOTION)
                    {
                      svg_value_unref (motion);
                      motion = svg_value_ref (val);
                    }
                  else
                    {
                      shape_set_current_value (shape, a->attr, a->idx, val);
                    }
                }

              svg_value_unref (val);
            }
        }

      if (!svg_value_equal (motion, identity))
        {
          SvgValue *combined;

          combined = svg_value_accumulate (shape_get_current_value (shape, SHAPE_ATTR_TRANSFORM, 0), motion, context, 1);
          shape_set_current_value (shape, SHAPE_ATTR_TRANSFORM, 0, combined);
          svg_value_unref (combined);
        }
      svg_value_unref (identity);
      svg_value_unref (motion);
    }

  if (shape_type_has_shapes (shape->type))
    {
      Shape *parent = context->parent;
      context->parent = shape;

      for (Shape *sh = shape->first; sh; sh = sh->next)
        compute_current_values_for_shape (sh, context);

      context->parent = parent;
    }

  context->viewport = old_viewport;
}

/* }}} */
/* {{{ gpa things */

static struct {
  double params[4];
} easing_funcs[] = {
  { { 0, 0, 1, 1 } },
  { { 0.42, 0, 0.58, 1 } },
  { { 0.42, 0, 1, 1 } },
  { { 0, 0, 0.58, 1 } },
  { { 0.25, 0.1, 0.25, 1 } },
};

static void
apply_state (Shape        *shape,
             unsigned int  state)
{
  if (shape_type_has_gpa_attrs (shape->type))
    {
      SvgValue *value;

      if (state == GTK_SVG_STATE_EMPTY)
        value = svg_visibility_new (VISIBILITY_HIDDEN);
      else if (shape->gpa.states & BIT (state))
        value = svg_visibility_new (VISIBILITY_VISIBLE);
      else
        value = svg_visibility_new (VISIBILITY_HIDDEN);

      shape_set_base_value (shape, SHAPE_ATTR_VISIBILITY, 0, value);
      svg_value_unref (value);
    }

  if (shape_type_has_shapes (shape->type))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          Shape *sh = g_ptr_array_index (shape->shapes, i);
          apply_state (sh, state);
        }
    }
}

/* {{{ Weight variation */

static double
width_apply_weight (double width,
                    double minwidth,
                    double maxwidth,
                    double weight)
{
  if (weight < 1)
    {
      g_assert_not_reached ();
    }
  else if (weight < 400)
    {
      double f = (400 - weight) / (400 - 1);
      return lerp (f, width, minwidth);
    }
  else if (weight == 400)
    {
      return width;
    }
  else if (weight <= 1000)
    {
      double f = (weight - 400) / (1000 - 400);
      return lerp (f, width, maxwidth);
    }
  else
    {
      g_assert_not_reached ();
    }
}

/* }}} */
/* {{{ States */

static void
create_visibility_setter (Shape        *shape,
                          Timeline     *timeline,
                          uint64_t      states,
                          int64_t       delay,
                          unsigned int  initial_state)
{
  Animation *a = animation_set_new ();
  TimeSpec *begin, *end;
  Visibility initial_visibility;
  Visibility opposite_visibility;

  if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_VISIBILITY))
    initial_visibility = svg_enum_get (shape->base[SHAPE_ATTR_VISIBILITY]);
  else
    initial_visibility = VISIBILITY_VISIBLE;

  a->attr = SHAPE_ATTR_VISIBILITY;

  if (initial_visibility == VISIBILITY_VISIBLE)
    {
      a->id = g_strdup_printf ("gpa:out-of-state:%s", shape->id);
      begin = animation_add_begin (a, timeline_get_states (timeline, states, TIME_SPEC_SIDE_END, MAX (0, - delay)));
      time_spec_add_animation (begin, a);

      end = animation_add_end (a, timeline_get_states (timeline, states, TIME_SPEC_SIDE_BEGIN, - (MAX (0, - delay))));
      time_spec_add_animation (end, a);

      opposite_visibility = VISIBILITY_HIDDEN;
    }
  else
    {
      a->id = g_strdup_printf ("gpa:in-state:%s", shape->id);
      begin = animation_add_begin (a, timeline_get_states (timeline, states, TIME_SPEC_SIDE_BEGIN, MAX (0, - delay)));
      time_spec_add_animation (begin, a);

      end = animation_add_end (a, timeline_get_states (timeline, states, TIME_SPEC_SIDE_END, - (MAX (0, - delay))));
      time_spec_add_animation (end, a);

      opposite_visibility = VISIBILITY_VISIBLE;
    }

  if (state_match (states, initial_state) != (initial_visibility == VISIBILITY_VISIBLE))
    {
      begin = animation_add_begin (a, timeline_get_start_of_time (timeline));
      time_spec_add_animation (begin, a);
    }


  a->has_begin = 1;
  a->has_end = 1;

  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  a->frames[0].value = svg_visibility_new (opposite_visibility);
  a->frames[1].value = svg_visibility_new (opposite_visibility);

  a->fill = ANIMATION_FILL_REMOVE;

  shape_add_animation (shape, a);
}

static void
create_states (Shape        *shape,
               Timeline     *timeline,
               uint64_t      states,
               int64_t       delay,
               unsigned int  initial)
{
  create_visibility_setter (shape, timeline, states, delay, initial);
}

/* }}} */
/* {{{ Transitions */

static void
create_path_length (Shape    *shape,
                    Timeline *timeline)
{
  Animation *a = animation_set_new ();
  TimeSpec *begin, *end;

  a->attr = SHAPE_ATTR_PATH_LENGTH;

  a->id = g_strdup_printf ("gpa:path-length");
  begin = animation_add_begin (a, timeline_get_start_of_time (timeline));
  end = animation_add_end (a, timeline_get_end_of_time (timeline));
  time_spec_add_animation (begin, a);
  time_spec_add_animation (end, a);

  a->has_begin = 1;
  a->has_end = 1;

  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  a->frames[0].value = svg_number_new (1);
  a->frames[1].value = svg_number_new (1);

  a->fill = ANIMATION_FILL_REMOVE;

  shape_add_animation (shape, a);
}

static void
create_transition (Shape         *shape,
                   Timeline      *timeline,
                   uint64_t       states,
                   int64_t        duration,
                   int64_t        delay,
                   GpaEasing      easing,
                   double         origin,
                   GpaTransition  type,
                   ShapeAttr      attr,
                   SvgValue      *from,
                   SvgValue      *to)
{
  Animation *a;
  TimeSpec *begin;

  a = animation_animate_new ();
  a->simple_duration = duration;
  a->repeat_duration = duration;
  a->repeat_count = 1;

  a->has_begin = 1;
  a->has_simple_duration = 1;
  a->has_repeat_duration = 1;

  a->id = g_strdup_printf ("gpa:transition:fade-in:%s:%s", shape_attr_get_name (attr), shape->id);

  begin = animation_add_begin (a, timeline_get_states (timeline, states, TIME_SPEC_SIDE_BEGIN, delay));

  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  memcpy (a->frames[0].params, easing_funcs[easing].params, sizeof (double) * 4);
  a->calc_mode = CALC_MODE_SPLINE;

  a->attr = attr;
  a->frames[0].value = from;
  a->frames[1].value = to;

  a->fill = ANIMATION_FILL_FREEZE;

  shape_add_animation (shape, a);
  time_spec_add_animation (begin, a);

  a->gpa.transition = type;
  a->gpa.easing = easing;
  a->gpa.origin = origin;

  a = animation_animate_new ();
  a->simple_duration = duration;
  a->repeat_duration = duration;
  a->repeat_count = 1;

  a->has_begin = 1;
  a->has_simple_duration = 1;
  a->has_repeat_duration = 1;

  a->id = g_strdup_printf ("gpa:transition:fade-out:%s:%s", shape_attr_get_name (attr), shape->id);

  begin = animation_add_begin (a, timeline_get_states (timeline, states, TIME_SPEC_SIDE_END, - (duration + delay)));

  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  memcpy (a->frames[0].params, easing_funcs[easing].params, sizeof (double) * 4);
  a->calc_mode = CALC_MODE_SPLINE;

  a->attr = attr;
  a->frames[0].value = svg_value_ref (to);
  a->frames[1].value = svg_value_ref (from);

  a->fill = ANIMATION_FILL_FREEZE;

  shape_add_animation (shape, a);
  time_spec_add_animation (begin, a);

  a->gpa.transition = type;
  a->gpa.easing = easing;
  a->gpa.origin = origin;

  if (delay > 0)
    {
      a = animation_set_new ();
      a->attr = attr;
      a->simple_duration = duration;
      a->repeat_duration = duration;
      a->repeat_count = 1;

      a->id = g_strdup_printf ("gpa:transition:delay-in:%s:%s", shape_attr_get_name (attr), shape->id);
      begin = animation_add_begin (a, timeline_get_states (timeline, states, TIME_SPEC_SIDE_BEGIN, 0));
      time_spec_add_animation (begin, a);

      a->has_begin = 1;
      a->has_simple_duration = 1;
      a->has_repeat_duration = 1;

      a->n_frames = 2;
      a->frames = g_new0 (Frame, a->n_frames);
      a->frames[0].time = 0;
      a->frames[1].time = 1;
      a->frames[0].value = svg_value_ref (from);
      a->frames[1].value = svg_value_ref (from);

      a->fill = ANIMATION_FILL_FREEZE;

      shape_add_animation (shape, a);

      a = animation_set_new ();
      a->attr = attr;
      a->simple_duration = duration;
      a->repeat_duration = duration;
      a->repeat_count = 1;

      a->id = g_strdup_printf ("gpa:transition:delay-out:%s:%s", shape_attr_get_name (attr), shape->id);
      begin = animation_add_begin (a, timeline_get_states (timeline, states, TIME_SPEC_SIDE_END, 0));
      time_spec_add_animation (begin, a);

      a->has_begin = 1;
      a->has_simple_duration = 1;
      a->has_repeat_duration = 1;

      a->n_frames = 2;
      a->frames = g_new0 (Frame, a->n_frames);
      a->frames[0].time = 0;
      a->frames[1].time = 1;
      a->frames[0].value = svg_value_ref (to);
      a->frames[1].value = svg_value_ref (to);

      a->fill = ANIMATION_FILL_FREEZE;

      shape_add_animation (shape, a);
    }
}

static void
create_transition_delay (Shape     *shape,
                         Timeline  *timeline,
                         uint64_t   states,
                         int64_t    delay,
                         ShapeAttr  attr,
                         SvgValue  *value)
{
  Animation *a;
  TimeSpec *begin;

  a = animation_set_new ();
  a->simple_duration = delay;
  a->repeat_duration = delay;
  a->repeat_count = 1;

  a->has_begin = 1;
  a->has_simple_duration = 1;
  a->has_repeat_duration = 1;

  a->id = g_strdup_printf ("gpa:transition:fade-in-delay:%s:%s", shape_attr_get_name (attr), shape->id);

  begin = animation_add_begin (a, timeline_get_states (timeline, states, TIME_SPEC_SIDE_BEGIN, 0));

  a->attr = attr;
  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  a->frames[0].value = svg_value_ref (value);
  a->frames[1].value = svg_value_ref (value);

  a->fill = ANIMATION_FILL_REMOVE;

  shape_add_animation (shape, a);
  time_spec_add_animation (begin, a);

  a = animation_set_new ();
  a->simple_duration = delay;
  a->repeat_duration = delay;
  a->repeat_count = 1;

  a->has_begin = 1;
  a->has_simple_duration = 1;
  a->has_repeat_duration = 1;

  a->id = g_strdup_printf ("gpa:transition:fade-out-delay:%s:%s", shape_attr_get_name (attr), shape->id);

  begin = animation_add_begin (a, timeline_get_states (timeline, states, TIME_SPEC_SIDE_END, -delay));

  a->attr = attr;
  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  a->frames[0].value = svg_value_ref (value);
  a->frames[1].value = svg_value_ref (value);

  a->fill = ANIMATION_FILL_REMOVE;

  shape_add_animation (shape, a);
  time_spec_add_animation (begin, a);

  svg_value_unref (value);
}

static void
create_transitions (Shape         *shape,
                    Timeline      *timeline,
                    uint64_t       states,
                    GpaTransition  type,
                    int64_t        duration,
                    int64_t        delay,
                    GpaEasing      easing,
                    double         origin)
{
  switch (type)
    {
    case GPA_TRANSITION_NONE:
      break;
    case GPA_TRANSITION_ANIMATE:
      create_transition (shape, timeline, states,
                         duration, delay, easing,
                         origin, type,
                         SHAPE_ATTR_STROKE_DASHARRAY,
                         svg_dash_array_new ((double[]) { 0, 2 }, 2),
                         svg_dash_array_new ((double[]) { 1, 0 }, 2));
      if (delay != 0)
        create_transition_delay (shape, timeline, states, delay,
                                 SHAPE_ATTR_STROKE_DASHOFFSET,
                                 svg_number_new (0.5));
      if (!G_APPROX_VALUE (origin, 0, 0.001))
        create_transition (shape, timeline, states,
                           duration, delay, easing,
                         origin, type,
                           SHAPE_ATTR_STROKE_DASHOFFSET,
                           svg_number_new (-origin),
                           svg_number_new (0));
      break;
    case GPA_TRANSITION_MORPH:
      create_transition (shape, timeline, states,
                         duration, delay, easing,
                         origin, type,
                         SHAPE_ATTR_FILTER,
                         svg_filter_parse ("blur(32) alpha-level(0.2)"),
                         svg_filter_parse ("blur(0) alpha-level(0.2)"));
      break;
    case GPA_TRANSITION_FADE:
      create_transition (shape, timeline, states,
                         duration, delay, easing,
                         origin, type,
                         SHAPE_ATTR_OPACITY,
                         svg_number_new (0), svg_number_new (1));
      break;
    default:
      g_assert_not_reached ();
    }
}

/* }}} */
/* {{{ Animations */

static Animation *
create_animation (Shape        *shape,
                  Timeline     *timeline,
                  uint64_t      states,
                  unsigned int  initial,
                  double        repeat,
                  int64_t       duration,
                  CalcMode      calc_mode,
                  ShapeAttr     attr,
                  GArray       *frames)
{
  Animation *a;
  TimeSpec *begin, *end;

  a = animation_animate_new ();
  a->repeat_count = repeat;
  a->simple_duration = duration;
  if (repeat == REPEAT_FOREVER)
    a->repeat_duration = INDEFINITE;
  else
    a->repeat_duration =duration * repeat;

  a->has_begin = 1;
  a->has_end = 1;
  a->has_simple_duration = 1;
  a->has_repeat_count = 1;

  a->id = g_strdup_printf ("gpa:animation:%s-%s", shape->id, shape_attr_get_name (attr));

  begin = animation_add_begin (a, timeline_get_states (timeline, states, TIME_SPEC_SIDE_BEGIN, 0));
  time_spec_add_animation (begin, a);

  if (state_match (states, initial))
    {
      begin = animation_add_begin (a, timeline_get_start_of_time (timeline));
      time_spec_add_animation (begin, a);
    }

  end = animation_add_end (a, timeline_get_states (timeline, states, TIME_SPEC_SIDE_END, 0));
  time_spec_add_animation (end, a);

  a->attr = attr;
  a->n_frames = frames->len;
  a->frames = g_array_steal (frames, NULL);

  a->calc_mode = calc_mode;

  shape_add_animation (shape, a);

  return a;
}

static void
add_frame (GArray    *a,
           double     time,
           SvgValue  *value,
           GpaEasing  easing)
{
  Frame frame;
  frame.time = time;
  frame.value = value;
  frame.point = 0;
  memcpy (frame.params, easing_funcs[easing].params, 4 * sizeof (double));
  g_array_append_val (a, frame);
}

static void
add_point_frame (GArray    *a,
                 double     time,
                 double     point,
                 GpaEasing  easing)
{
  Frame frame;
  frame.time = time;
  frame.value = NULL;
  frame.point = point;
  memcpy (frame.params, easing_funcs[easing].params, 4 * sizeof (double));
  g_array_append_val (a, frame);
}

static void
construct_animation_frames (GpaAnimation  direction,
                            GpaEasing     easing,
                            double        segment,
                            double        origin,
                            GArray       *array,
                            GArray       *offset)
{
  switch (direction)
    {
    case GPA_ANIMATION_NORMAL:
      add_frame (array, 0, svg_dash_array_new ((double[]) { 0, 2 }, 2), easing);
      add_frame (array, 1, svg_dash_array_new ((double[]) { 1, 0 }, 2), easing);
      if (origin != 0)
        {
          add_frame (offset, 0, svg_number_new (-origin), easing);
          add_frame (offset, 1, svg_number_new (0), easing);
        }
      break;

    case GPA_ANIMATION_REVERSE:
      add_frame (array, 0, svg_dash_array_new ((double[]) { 1, 0 }, 2), easing);
      add_frame (array, 1, svg_dash_array_new ((double[]) { 0, 2 }, 2), easing);
      if (origin != 0)
        {
          add_frame (offset, 0, svg_number_new (0), easing);
          add_frame (offset, 1, svg_number_new (-origin), easing);
        }
      break;

    case GPA_ANIMATION_ALTERNATE:
      add_frame (array, 0,   svg_dash_array_new ((double[]) { 0, 2 }, 2), easing);
      add_frame (array, 0.5, svg_dash_array_new ((double[]) { 1, 0 }, 2), easing);
      add_frame (array, 1,   svg_dash_array_new ((double[]) { 0, 2 }, 2), easing);
      if (origin != 0)
        {
          add_frame (offset, 0,   svg_number_new (-origin), easing);
          add_frame (offset, 0.5, svg_number_new (0), easing);
          add_frame (offset, 1,   svg_number_new (-origin), easing);
        }
      break;

    case GPA_ANIMATION_REVERSE_ALTERNATE:
      add_frame (array, 0,   svg_dash_array_new ((double[]) { 1, 0 }, 2), easing);
      add_frame (array, 0.5, svg_dash_array_new ((double[]) { 0, 2 }, 2), easing);
      add_frame (array, 1,   svg_dash_array_new ((double[]) { 1, 0 }, 2), easing);
      if (origin != 0)
        {
          add_frame (offset, 0,   svg_number_new (0), easing);
          add_frame (offset, 0.5, svg_number_new (-origin), easing);
          add_frame (offset, 1,   svg_number_new (0), easing);
        }
      break;

    case GPA_ANIMATION_IN_OUT:
      add_frame (array, 0,   svg_dash_array_new ((double[]) { 0, 0, 0, 2 }, 4), easing);
      add_frame (array, 0.5, svg_dash_array_new ((double[]) { origin, 0, 1 - origin, 2 }, 4), easing);
      add_frame (array, 1,   svg_dash_array_new ((double[]) { 0, 1, 0, 2 }, 4), easing);
      add_frame (offset, 0,   svg_number_new (-origin), easing);
      add_frame (offset, 0.5, svg_number_new (0), easing);
      add_frame (offset, 1,   svg_number_new (0), easing);
      break;

    case GPA_ANIMATION_IN_OUT_REVERSE:
      add_frame (array, 0  , svg_dash_array_new ((double[]) { origin, 0, 1 - origin, 2 }, 4), easing);
      add_frame (array, 0.5, svg_dash_array_new ((double[]) { 0, 0, 0, 2 }, 4), easing);
      add_frame (array, 1,   svg_dash_array_new ((double[]) { origin, 0, 1 - origin, 2 }, 4), easing);
      add_frame (offset, 0,   svg_number_new (0), easing);
      add_frame (offset, 0.5, svg_number_new (-origin), easing);
      add_frame (offset, 1,   svg_number_new (0), easing);
      break;

    case GPA_ANIMATION_IN_OUT_ALTERNATE:
      add_frame (array, 0,    svg_dash_array_new ((double[]) { 0, 0, 0, 2 }, 4), easing);
      add_frame (array, 0.25, svg_dash_array_new ((double[]) { origin, 0, 1 - origin, 2 }, 4), easing);
      add_frame (array, 0.5,  svg_dash_array_new ((double[]) { 0, 1, 0, 2 }, 4), easing);
      add_frame (array, 0.75, svg_dash_array_new ((double[]) { origin, 0, 1 - origin, 2 }, 4), easing);
      add_frame (array, 1,    svg_dash_array_new ((double[]) { 0, 0, 0, 2 }, 4), easing);
      if (origin != 0)
        {
          add_frame (offset, 0,    svg_number_new (-origin), easing);
          add_frame (offset, 0.25, svg_number_new (0), easing);
          add_frame (offset, 0.5,  svg_number_new (0), easing);
          add_frame (offset, 0.75, svg_number_new (0), easing);
          add_frame (offset, 1,    svg_number_new (-origin), easing);
        }
      break;

    case GPA_ANIMATION_SEGMENT:
      add_frame (array, 0, svg_dash_array_new ((double[]) { segment, 1 - segment }, 2), easing);
      add_frame (array, 1, svg_dash_array_new ((double[]) { segment, 1 - segment }, 2), easing);
      add_frame (offset, 0, svg_number_new (0), easing);
      add_frame (offset, 1, svg_number_new (-1), easing);
      break;

    case GPA_ANIMATION_SEGMENT_ALTERNATE:
      add_frame (array, 0,   svg_dash_array_new ((double[]) { segment, 2 }, 2), easing);
      add_frame (array, 0.5, svg_dash_array_new ((double[]) { segment, 2 }, 2), easing);
      add_frame (array, 1,   svg_dash_array_new ((double[]) { segment, 2 }, 2), easing);
      add_frame (offset, 0,   svg_number_new (0), easing);
      add_frame (offset, 0.5, svg_number_new (segment - 1), easing);
      add_frame (offset, 1,   svg_number_new (0), easing);
      break;

    case GPA_ANIMATION_NONE:
    default:
      g_assert_not_reached ();
    }
}

static void
construct_moving_frames (GpaAnimation  direction,
                         GpaEasing     easing,
                         double        segment,
                         double        origin,
                         double        attach_pos,
                         GArray       *array)
{
  switch ((int)direction)
    {
    case GPA_ANIMATION_NORMAL:
      add_point_frame (array, 0, origin, easing);
      add_point_frame (array, 1, attach_pos, easing);
      break;

    case GPA_ANIMATION_ALTERNATE:
      add_point_frame (array, 0, origin, easing);
      add_point_frame (array, 0.5, attach_pos, easing);
      add_point_frame (array, 1, origin, easing);
      break;

    case GPA_ANIMATION_REVERSE:
      add_point_frame (array, 0, attach_pos, easing);
      add_point_frame (array, 1, origin, easing);
      break;

    case GPA_ANIMATION_REVERSE_ALTERNATE:
      add_point_frame (array, 0,   attach_pos, easing);
      add_point_frame (array, 0.5, origin, easing);
      add_point_frame (array, 1,   attach_pos, easing);
      break;

    case GPA_ANIMATION_IN_OUT:
      add_point_frame (array, 0,   origin, easing);
      add_point_frame (array, 0.5, attach_pos, easing);
      add_point_frame (array, 1,   1, easing);
      break;

    case GPA_ANIMATION_IN_OUT_REVERSE:
      add_point_frame (array, 0,   1, easing);
      add_point_frame (array, 0.5, attach_pos, easing);
      add_point_frame (array, 1,   origin, easing);
      break;

    case GPA_ANIMATION_IN_OUT_ALTERNATE:
      add_point_frame (array, 0,    origin, easing);
      add_point_frame (array, 0.25, attach_pos, easing);
      add_point_frame (array, 0.5,  1, easing);
      add_point_frame (array, 0.75, attach_pos, easing);
      add_point_frame (array, 1,    origin, easing);
      break;

    case GPA_ANIMATION_SEGMENT:
      add_point_frame (array, 0, attach_pos * segment, easing);
      add_point_frame (array, 1, 1 + attach_pos * segment, easing);
      break;

    case GPA_ANIMATION_SEGMENT_ALTERNATE:
      add_point_frame (array, 0, attach_pos * segment, easing);
      add_point_frame (array, 0.5, (1 - segment) + attach_pos * segment, easing);
      add_point_frame (array, 1, attach_pos * segment, easing);
      break;

    default:
      g_assert_not_reached ();
    }
}

static double
repeat_duration_for_direction (GpaAnimation direction,
                               double       duration)
{
  switch (direction)
    {
    case GPA_ANIMATION_NONE:              return 0;
    case GPA_ANIMATION_NORMAL:            return duration;
    case GPA_ANIMATION_ALTERNATE:         return 2 * duration;
    case GPA_ANIMATION_REVERSE:           return duration;
    case GPA_ANIMATION_REVERSE_ALTERNATE: return 2 * duration;
    case GPA_ANIMATION_IN_OUT:            return 2 * duration;
    case GPA_ANIMATION_IN_OUT_ALTERNATE:  return 4 * duration;
    case GPA_ANIMATION_IN_OUT_REVERSE:    return 2 * duration;
    case GPA_ANIMATION_SEGMENT:           return duration;
    case GPA_ANIMATION_SEGMENT_ALTERNATE: return 2 * duration;
    default: g_assert_not_reached ();
    }
}

static void
create_animations (Shape        *shape,
                   Timeline     *timeline,
                   uint64_t      states,
                   unsigned int  initial,
                   double        repeat,
                   int64_t       duration,
                   GpaAnimation  direction,
                   GpaEasing     easing,
                   double        segment,
                   double        origin)
{
  GArray *array, *offset;
  CalcMode calc_mode;
  Animation *a;
  double repeat_duration;

  if (direction == GPA_ANIMATION_NONE)
    return;

  if (duration == 0)
    {
      g_warning ("SVG: not creating zero-duration animations");
      return;
    }

  array = g_array_new (FALSE, FALSE, sizeof (Frame));
  offset = g_array_new (FALSE, FALSE, sizeof (Frame));

  construct_animation_frames (direction, easing, segment, origin, array, offset);
  repeat_duration = repeat_duration_for_direction (direction, duration);

  if (easing == GPA_EASING_LINEAR)
    calc_mode = CALC_MODE_LINEAR;
  else
    calc_mode = CALC_MODE_SPLINE;

  a = create_animation (shape, timeline, states, initial,
                        repeat, repeat_duration, calc_mode,
                        SHAPE_ATTR_STROKE_DASHARRAY,
                        array);

  a->gpa.animation = direction;
  a->gpa.easing = easing;
  a->gpa.origin = origin;
  a->gpa.segment = segment;

  if (offset->len > 0)
    a = create_animation (shape, timeline, states, initial,
                          repeat, repeat_duration, calc_mode,
                          SHAPE_ATTR_STROKE_DASHOFFSET,
                          offset);

  g_array_unref (array);
  g_array_unref (offset);
}

/* }}} */
/* {{{ Attachment */

static void
create_attachment (Shape      *shape,
                   Timeline   *timeline,
                   uint64_t    states,
                   const char *attach_to,
                   double      attach_pos,
                   double      origin)
{
  Animation *a;
  GArray *frames;
  TimeSpec *begin, *end;

  a = animation_motion_new ();

  a->has_begin = 1;
  a->has_end = 1;
  a->has_simple_duration = 1;

  a->simple_duration = 1;

  a->id = g_strdup_printf ("gpa:attachment:%s", shape->id);

  begin = animation_add_begin (a, timeline_get_start_of_time (timeline));
  end = animation_add_end (a, timeline_get_fixed (timeline, 1));

  frames = g_array_new (FALSE, FALSE, sizeof (Frame));
  add_point_frame (frames, 0, attach_pos, GPA_EASING_LINEAR);
  add_point_frame (frames, 1, attach_pos, GPA_EASING_LINEAR);

  a->n_frames = frames->len;
  a->frames = g_array_steal (frames, NULL);

  g_array_unref (frames);

  a->motion.path_ref = g_strdup (attach_to);

  a->calc_mode = CALC_MODE_LINEAR;
  a->fill = ANIMATION_FILL_FREEZE;
  a->motion.rotate = ROTATE_AUTO;

  a->gpa.origin = origin;
  a->gpa.attach_pos = attach_pos;

  shape_add_animation (shape, a);
  time_spec_add_animation (begin, a);
  time_spec_add_animation (end, a);
}

static void
create_attachment_connection_to (Animation *a,
                                 Animation *da,
                                 Timeline  *timeline)
{
  Animation *a2;
  GArray *frames;
  GpaAnimation direction;
  TimeSpec *begin, *end;

  a2 = animation_motion_new ();
  a2->simple_duration = da->simple_duration;
  a2->repeat_count = da->repeat_count;
  if (g_str_has_prefix (da->id, "gpa:animation:"))
    {
      a2->id = g_strdup_printf ("gpa:attachment-animation:%s", a->shape->id);
      direction = da->gpa.animation;
    }
  else if (g_str_has_prefix (da->id, "gpa:transition:fade-in:"))
    {
      a2->id = g_strdup_printf ("gpa:attachment-transition:fade-in:%s", a->shape->id);
      direction = GPA_ANIMATION_NORMAL;
    }
  else if (g_str_has_prefix (da->id, "gpa:transition:fade-out:"))
    {
      a2->id = g_strdup_printf ("gpa:attachment-transition:fade-out:%s", a->shape->id);
      direction = GPA_ANIMATION_REVERSE;
    }
  else
    g_assert_not_reached ();

  a2->has_begin = 1;
  a2->has_end = 1;
  a2->has_simple_duration = 1;

  begin = animation_add_begin (a2, timeline_get_sync (timeline, da->id, da, TIME_SPEC_SIDE_BEGIN, 0));
  end = animation_add_end (a2, timeline_get_sync (timeline, da->id, da, TIME_SPEC_SIDE_END, 0));

  frames = g_array_new (FALSE, FALSE, sizeof (Frame));

  construct_moving_frames (direction,
                           da->gpa.easing,
                           da->gpa.segment,
                           da->gpa.origin,
                           a->gpa.attach_pos,
                           frames);

  a2->n_frames = frames->len;
  a2->frames = g_array_steal (frames, NULL);

  g_array_unref (frames);

  a2->motion.path_shape = da->shape;

  a2->calc_mode = da->calc_mode;
  a2->fill = ANIMATION_FILL_FREEZE;
  a2->motion.rotate = ROTATE_AUTO;

  a2->gpa.origin = a->gpa.origin;

  shape_add_animation (a->shape, a2);
  time_spec_add_animation (begin, a2);
  time_spec_add_animation (end, a2);

  animation_add_dep (da, a2);
}

static void
create_attachment_connection (Animation *a,
                              Shape     *sh,
                              Timeline  *timeline)
{
  for (unsigned int i = 0; i < sh->animations->len; i++)
    {
      Animation *sha = g_ptr_array_index (sh->animations, i);
      if (sha->id &&
          (g_str_has_prefix (sha->id, "gpa:animation:") ||
           g_str_has_prefix (sha->id, "gpa:transition:")))
        {
          if (sha->attr == SHAPE_ATTR_STROKE_DASHARRAY)
            create_attachment_connection_to (a, sha, timeline);
        }
    }
}

/* }}} */
/* }}} */
/* {{{ Parser */

/* The parser creates the shape tree. We maintain a current shape,
 * and a current animation. Some things are done in a post-processing
 * step: finding the shape that an animation belongs to, resolving
 * other kinds of shape references, determining the proper order
 * for computing updated values.
 *
 * Note that we treat images, text, markers, gradients, filters as
 * shapes, but not color stops and filter primitives. Animations
 * are their own thing too.
 *
 * So each shapes can have multiple
 * - child shapes
 * - animations
 * - color stops
 * - filter primitives
 */
typedef struct
{
  GtkSvg *svg;
  Shape *current_shape;
  GSList *shape_stack;
  GHashTable *shapes;
  GHashTable *animations;
  Animation *current_animation;
  GPtrArray *pending_animations;
  GPtrArray *pending_refs;
  struct {
    const GSList *to;
    GtkSvgLocation start;
    char *reason;
    gboolean skip_over_target;
  } skip;
  gboolean collect_text;
  GString *text;
  uint64_t num_loaded_elements;
} ParserData;

/* {{{ Animation attributes */

static gboolean
parse_base_animation_attrs (Animation            *a,
                            const char           *element_name,
                            const char          **attr_names,
                            const char          **attr_values,
                            uint64_t             *handled,
                            ParserData           *data,
                            GMarkupParseContext  *context)
{
  const char *id_attr = NULL;
  const char *href_attr = NULL;
  const char *xlink_href_attr = NULL;
  const char *begin_attr = NULL;
  const char *end_attr = NULL;
  const char *dur_attr = NULL;
  const char *repeat_count_attr = NULL;
  const char *repeat_dur_attr = NULL;
  const char *fill_attr = NULL;
  const char *restart_attr = NULL;
  const char *attr_name_attr = NULL;
  const char *ignored = NULL;
  ShapeAttr attr;
  gboolean deprecated;
  Shape *current_shape = NULL;
  FilterPrimitiveType filter_type = 0;

  markup_filter_attributes (element_name,
                            attr_names, attr_values,
                            handled,
                            "id", &id_attr,
                            "href", &href_attr,
                            "xlink:href", &xlink_href_attr,
                            "begin", &begin_attr,
                            "end", &end_attr,
                            "dur", &dur_attr,
                            "repeatCount", &repeat_count_attr,
                            "repeatDur", &repeat_dur_attr,
                            "fill", &fill_attr,
                            "restart", &restart_attr,
                            "attributeName", &attr_name_attr,
                            "gpa:status", &ignored,
                            NULL);

  a->id = g_strdup (id_attr);

  if (xlink_href_attr && !href_attr)
    href_attr = xlink_href_attr;

  if (href_attr)
    {
      if (href_attr[0] != '#')
        gtk_svg_invalid_attribute (data->svg, context, "href", "Missing '#' in 'href'");
      else
        a->href = g_strdup (href_attr + 1);
    }

  if (a->href)
    current_shape = g_hash_table_lookup (data->shapes, a->href);

  if (!current_shape)
    current_shape = data->current_shape;

  if (begin_attr)
    {
      GStrv strv;

      strv = g_strsplit (begin_attr, ";", 0);
      for (unsigned int i = 0; strv[i]; i++)
        {
          TimeSpec spec = { 0, };
          TimeSpec *begin;
          GError *error = NULL;

          if (!time_spec_parse (&spec, strv[i]))
            {
              gtk_svg_invalid_attribute (data->svg, context, "begin", NULL);
              g_clear_error (&error);
              continue;
            }

          a->has_begin = 1;
          begin = animation_add_begin (a, timeline_get_time_spec (data->svg->timeline, &spec));
          time_spec_add_animation (begin, a);
          time_spec_clear (&spec);
          if (begin->type == TIME_SPEC_TYPE_STATES)
            {
              if (begin->states.states != NO_STATES)
                data->svg->max_state = MAX (data->svg->max_state, g_bit_nth_msf (begin->states.states, -1));
            }
        }
      g_strfreev (strv);
    }
  else
    {
      TimeSpec *begin;
      begin = animation_add_begin (a, timeline_get_start_of_time (data->svg->timeline));
      time_spec_add_animation (begin, a);
    }

  if (end_attr)
    {
      GStrv strv;

      strv = g_strsplit (end_attr, ";", 0);
      for (unsigned int i = 0; strv[i]; i++)
        {
          TimeSpec spec = { 0, };
          TimeSpec *end;
          GError *error = NULL;

          if (!time_spec_parse (&spec, strv[i]))
            {
              gtk_svg_invalid_attribute (data->svg, context, "end", NULL);
              g_clear_error (&error);
              continue;
            }
          a->has_end = 1;
          end = animation_add_end (a, timeline_get_time_spec (data->svg->timeline, &spec));
          time_spec_add_animation (end, a);
          time_spec_clear (&spec);
          if (end->type == TIME_SPEC_TYPE_STATES)
            {
              if (end->states.states != NO_STATES)
                data->svg->max_state = MAX (data->svg->max_state, g_bit_nth_msf (end->states.states, -1));
            }
        }
      g_strfreev (strv);
    }
  else
    {
      TimeSpec *end;
      end = animation_add_end (a, timeline_get_end_of_time (data->svg->timeline));
      time_spec_add_animation (end, a);
    }

  a->simple_duration = INDEFINITE;
  if (dur_attr)
    {
      a->has_simple_duration = 1;
      if (match_str (dur_attr, "indefinite"))
        a->simple_duration = INDEFINITE;
      else if (!parse_duration (dur_attr, &a->simple_duration))
        {
          gtk_svg_invalid_attribute (data->svg, context, "dur", NULL);
          a->has_simple_duration = 0;
        }
    }

  a->repeat_count = REPEAT_FOREVER;
  if (repeat_count_attr)
    {
      a->has_repeat_count = 1;
      if (match_str (repeat_count_attr, "indefinite"))
        a->repeat_count = REPEAT_FOREVER;
      else if (!parse_number (repeat_count_attr, 0, DBL_MAX, &a->repeat_count))
        {
          gtk_svg_invalid_attribute (data->svg, context, "repeatCount", NULL);
          a->has_repeat_count = 0;
        }
    }

  a->repeat_duration = INDEFINITE;
  if (repeat_dur_attr)
    {
      a->has_repeat_duration = 1;
      if (match_str (repeat_dur_attr, "indefinite"))
        a->repeat_duration = INDEFINITE;
      else if (!parse_duration (repeat_dur_attr, &a->repeat_duration))
        {
          gtk_svg_invalid_attribute (data->svg, context, "repeatDur", NULL);
          a->has_repeat_duration = 0;
        }
    }

  if (!a->has_repeat_duration && !a->has_repeat_count)
    {
      a->repeat_count = 1;
      a->repeat_duration = a->simple_duration;
    }
  else if (!a->has_repeat_count && !a->has_simple_duration)
    {
      a->repeat_count = 1;
      a->simple_duration = a->repeat_duration;
    }
  else if (a->has_repeat_count && a->has_simple_duration && !a->has_repeat_duration)
    {
      if (a->repeat_count == REPEAT_FOREVER)
        a->repeat_duration = INDEFINITE;
      else
        a->repeat_duration = a->simple_duration * a->repeat_count;
    }
  else if (a->has_repeat_duration && a->has_simple_duration && !a->has_repeat_count)
    {
      if (a->repeat_duration == INDEFINITE)
        a->repeat_count = REPEAT_FOREVER;
      else
        a->repeat_count = a->repeat_duration / a->simple_duration;
    }
  else if (a->has_repeat_duration && a->has_repeat_count && !a->has_simple_duration)
    {
      if (a->repeat_duration == INDEFINITE || a->repeat_count == REPEAT_FOREVER)
        a->simple_duration = INDEFINITE;
      else
        a->simple_duration = a->repeat_duration / a->repeat_count;
    }

  a->fill = ANIMATION_FILL_REMOVE;
  if (fill_attr)
    {
      unsigned int value;

      if (!parse_enum (fill_attr,
                       (const char *[]) { "freeze", "remove" }, 2,
                       &value))
        gtk_svg_invalid_attribute (data->svg, context, "fill", NULL);
      else
        a->fill = (AnimationFill) value;
    }

  a->restart = ANIMATION_RESTART_ALWAYS;
  if (restart_attr)
    {
      unsigned int value;

      if (!parse_enum (restart_attr,
                       (const char *[]) { "always", "whenNotActive", "never" }, 3,
                       &value))
        gtk_svg_invalid_attribute (data->svg, context, "restart", NULL);
      else
        a->restart = (AnimationRestart) value;
    }

  if (attr_name_attr && strcmp (attr_name_attr, "xlink:href") == 0)
    attr_name_attr = "href";

  if (current_shape->type == SHAPE_FILTER &&
      current_shape->filters->len > 0)
    {
      FilterPrimitive *fp;

      fp = g_ptr_array_index (current_shape->filters,
                              current_shape->filters->len - 1);
      filter_type = fp->type;
    }

  attr = a->attr;
  if (a->type == ANIMATION_TYPE_MOTION)
    {
      if (attr_name_attr)
        gtk_svg_invalid_attribute (data->svg, context, "attributeName",
                                   "can't have 'attributeName' on <animateMotion>");
    }
  else if (a->type == ANIMATION_TYPE_TRANSFORM)
    {
      const char *expected;

      /* FIXME: if href is set, current_shape might be the wrong shape */
      expected = shape_attr_get_presentation (SHAPE_ATTR_TRANSFORM, current_shape->type);
      if (attr_name_attr && strcmp (attr_name_attr, expected) != 0)
        gtk_svg_invalid_attribute (data->svg, context, "attributeName",
                                   "value must be '%s'", expected);
      a->attr = SHAPE_ATTR_TRANSFORM;
    }
  else if (!attr_name_attr)
    {
      gtk_svg_missing_attribute (data->svg, context, "attributeName", NULL);
      return FALSE;
    }
  /* FIXME: if href is set, current_shape might be the wrong shape */
  else if ((current_shape->type == SHAPE_FILTER &&
            filter_attr_lookup (filter_type, attr_name_attr, &attr, &deprecated)) ||
           (current_shape->type != SHAPE_FILTER &&
            shape_attr_lookup (attr_name_attr, current_shape->type, &attr, &deprecated)))
    {
      a->attr = attr;
      /* FIXME: if href is set, current_shape might be the wrong shape */
      if (has_ancestor (context, "stop"))
        a->idx = current_shape->color_stops->len;
      else if (has_ancestor (context, "filter"))
        a->idx = current_shape->filters->len;
    }
  else
    {
      gtk_svg_invalid_attribute (data->svg, context, "attributeName", "can't animate '%s'", attr_name_attr);
      return FALSE;
    }

  return TRUE;
}

static GArray *
create_default_times (CalcMode      calc_mode,
                      unsigned int  n_values)
{
  GArray *times;
  double n;

  if (calc_mode == CALC_MODE_DISCRETE)
    n = n_values;
  else
    n = n_values - 1;

  times = g_array_new (FALSE, FALSE, sizeof (double));

  for (unsigned int i = 0; i < n_values; i++)
    {
      double d = i / (double) n;
      g_array_append_val (times, d);
    }

  return times;
}

static gboolean
parse_value_animation_attrs (Animation            *a,
                             const char           *element_name,
                             const char          **attr_names,
                             const char          **attr_values,
                             uint64_t             *handled,
                             ParserData           *data,
                             GMarkupParseContext  *context)
{
  const char *type_attr = NULL;
  const char *calc_mode_attr = NULL;
  const char *values_attr = NULL;
  const char *from_attr = NULL;
  const char *to_attr = NULL;
  const char *by_attr = NULL;
  const char *key_times_attr = NULL;
  const char *splines_attr = NULL;
  const char *additive_attr = NULL;
  const char *accumulate_attr = NULL;
  const char *color_interpolation_attr = NULL;
  TransformType transform_type = TRANSFORM_NONE;
  GArray *times = NULL;
  GArray *points = NULL;
  GArray *params = NULL;
  GPtrArray *values = NULL;

  markup_filter_attributes (element_name,
                            attr_names, attr_values,
                            handled,
                            "type", &type_attr,
                            "calcMode", &calc_mode_attr,
                            "values", &values_attr,
                            "from", &from_attr,
                            "to", &to_attr,
                            "by", &by_attr,
                            "keyTimes", &key_times_attr,
                            "keySplines", &splines_attr,
                            "additive", &additive_attr,
                            "accumulate", &accumulate_attr,
                            "color-interpolation", &color_interpolation_attr,
                            NULL);

  if (a->type == ANIMATION_TYPE_MOTION)
    transform_type = TRANSFORM_TRANSLATE;

  if (a->type == ANIMATION_TYPE_TRANSFORM)
    {
      if (type_attr)
        {
          unsigned int value;

          if (!parse_enum (type_attr,
                           (const char *[]) { "translate", "scale", "rotate",
                                              "skewX", "skewY" }, 5,
                            &value))
            {
              gtk_svg_invalid_attribute (data->svg, context, "type", NULL);
              return FALSE;
            }
          else
            transform_type = (TransformType) (value + 1);
        }
      else
        {
          gtk_svg_missing_attribute (data->svg, context, "type", NULL);
          return FALSE;
        }
    }
  else if (type_attr)
    {
      gtk_svg_invalid_attribute (data->svg, context, "type", NULL);
    }

  if (calc_mode_attr)
    {
      unsigned int value;

      if (!parse_enum (calc_mode_attr,
                       (const char *[]) { "discrete", "linear", "paced", "spline" }, 4,
                       &value))
        gtk_svg_invalid_attribute (data->svg, context, "calcMode", NULL);
      else
        a->calc_mode = (CalcMode) value;
   }

  if (additive_attr)
    {
      unsigned int value;

      if (!parse_enum (additive_attr,
                       (const char *[]) { "replace", "sum", }, 2,
                       &value))
        gtk_svg_invalid_attribute (data->svg, context, "additive", NULL);
      else
        a->additive = (AnimationAdditive) value;
   }

  if (accumulate_attr)
    {
      unsigned int value;

      if (!parse_enum (accumulate_attr,
                       (const char *[]) { "none", "sum", }, 2,
                       &value))
        gtk_svg_invalid_attribute (data->svg, context, "accumulate", NULL);
      else
        a->accumulate = (AnimationAccumulate) value;
   }

  if (color_interpolation_attr)
    {
      SvgValue *v;

      v = svg_color_interpolation_parse (color_interpolation_attr);
      if (!v)
        {
          gtk_svg_invalid_attribute (data->svg, context, "color-interpolation", NULL);
        }
      else
        {
          a->color_interpolation = svg_enum_get (v);
          svg_value_unref (v);
        }
    }

  if (values_attr)
    {
      values = shape_attr_parse_values (a->attr, transform_type, values_attr);
      if (!values || values->len < 2)
        {
          gtk_svg_invalid_attribute (data->svg, context, "values", "failed to parse %s", values_attr);
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }
    }
  else if (from_attr && to_attr)
    {
      char *from_and_to = g_strconcat (from_attr, ";", to_attr, NULL);

      values = shape_attr_parse_values (a->attr, transform_type, from_and_to);
      if (!values || values->len != 2)
        {
          gtk_svg_invalid_attribute (data->svg, context, NULL,  "Failed to parse 'from' or 'to'");
          g_free (from_and_to);
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }

      g_free (from_and_to);
    }
  else if (from_attr && by_attr)
    {
      GPtrArray *byvals;
      SvgValue *from;
      SvgValue *by;
      SvgValue *to;
      ComputeContext ctx = { 0, };

      values = shape_attr_parse_values (a->attr, transform_type, from_attr);
      byvals = shape_attr_parse_values (a->attr, transform_type, by_attr);

      if (!values || values->len != 1 || !byvals || byvals->len != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, NULL,  "Failed to parse 'from' or 'by'");
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&byvals, g_ptr_array_unref);
          return FALSE;
        }

      from = g_ptr_array_index (values, 0);
      by = g_ptr_array_index (byvals, 0);
      ctx.interpolation = GDK_COLOR_STATE_SRGB; /* Nothing else is used */
      to = svg_value_accumulate (by, from, &ctx, 1);
      g_ptr_array_add (values, to);
      g_ptr_array_unref (byvals);
    }
  else if (to_attr)
    {
      values = shape_attr_parse_values (a->attr, transform_type, to_attr);

      if (!values || values->len != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, NULL,  "Failed to parse 'to'");
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }

      /* We use a special keyword for this */
      g_ptr_array_insert (values, 0, svg_current_new ());
    }
  else if (by_attr)
    {
      SvgValue *from;
      SvgValue *by;

      values = shape_attr_parse_values (a->attr, transform_type, by_attr);

      if (!values || values->len != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, NULL,  "Failed to parse 'by'");
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }

      by = g_ptr_array_index (values, 0);
      if (by->class == &SVG_NUMBER_CLASS)
        from = svg_number_new_full (((SvgNumber *)by)->unit, 0);
      else if (by->class == &SVG_TRANSFORM_CLASS)
        from = svg_transform_new_none ();
      else if (by->class == &SVG_PAINT_CLASS &&
               ((SvgPaint *) by)->kind == PAINT_COLOR)
        from = svg_paint_new_transparent ();
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, NULL,  "Don't know how to handle this 'by' value");
          g_ptr_array_unref (values);
          return FALSE;
        }

      g_ptr_array_insert (values, 0, from);

      a->additive = ANIMATION_ADDITIVE_SUM;
    }

  if (a->type == ANIMATION_TYPE_MOTION)
    {
      if (values != NULL)
        {
          GskPathBuilder *builder = gsk_path_builder_new ();
          double length;

          points = g_array_new (FALSE, FALSE, sizeof (double));

          for (unsigned int i = 0; i < values->len; i++)
            {
              SvgTransform *tf = g_ptr_array_index (values, i);
              PrimitiveTransform *f = &tf->transforms[0];

              g_assert (tf->n_transforms == 1);
              g_assert (f->type == TRANSFORM_TRANSLATE);

              if (i == 0)
                {
                  length = 0;
                  gsk_path_builder_move_to (builder, f->translate.x, f->translate.y);
                }
              else
                {
                  length += graphene_point_distance (gsk_path_builder_get_current_point (builder),
                                                     &GRAPHENE_POINT_INIT (f->translate.x, f->translate.y), NULL, NULL);
                  gsk_path_builder_line_to (builder, f->translate.x, f->translate.y);
                }
              g_array_append_val (points, length);
            }

          for (unsigned int i = 1; i < points->len; i++)
            g_array_index (points, double, i) /= length;

          a->motion.path = gsk_path_builder_free_to_path (builder);

          g_clear_pointer (&values, g_ptr_array_unref);
        }
    }
  else
    {
      if (values == NULL)
        {
          gtk_svg_invalid_attribute (data->svg, context, NULL,
                                     "Either values or from/to/by must be given");
          return FALSE;
        }
    }

  if (a->calc_mode == CALC_MODE_PACED)
    {
      if (values && values->len > 2)
        {
          double length, distance;
          const SvgValue *v0, *v1;

          /* use value distances to compute evenly paced times */
          times = g_array_new (FALSE, FALSE, sizeof (double));
          length = distance = 0;
          g_array_append_val (times, distance);
          for (unsigned int i = 1; i < values->len; i++)
            {
              v0 = g_ptr_array_index (values, i - 1);
              v1 = g_ptr_array_index (values, i);
              distance = svg_value_distance (v0, v1);
              g_array_append_val (times, distance);
              length += distance;
            }
           for (unsigned int i = 1; i < times->len; i++)
             {
               g_array_index (times, double, i) /= length;
             }
           for (unsigned int i = 1; i < times->len; i++)
             {
               g_array_index (times, double, i) += g_array_index (times, double, i - 1);
             }
        }
    }
  else if (key_times_attr)
    {
      times = parse_numbers2 (key_times_attr, ";", 0, 1);
      if (!times)
        {
          gtk_svg_invalid_attribute (data->svg, context, "keyTimes", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }
    }

  if (times != NULL)
    {
      if (values && times->len != values->len)
        {
          gtk_svg_invalid_attribute (data->svg, context, NULL,
                                     "The values and keyTimes attributes must "
                                     "have the same number of items");
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&times, g_array_unref);
          return FALSE;
        }

      if (g_array_index (times, double, 0) != 0)
        {
          gtk_svg_invalid_attribute (data->svg, context, "keyTimes",
                                     "The first keyTimes value must be 0");
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&times, g_array_unref);
          return FALSE;
        }

      if (a->calc_mode != CALC_MODE_DISCRETE && g_array_index (times, double, times->len - 1) != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, "keyTimes",
                                     "The last keyTimes value must be 1");
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&times, g_array_unref);
          return FALSE;
        }

      for (unsigned int i = 1; i < times->len; i++)
        {
          if (g_array_index (times, double, i) < g_array_index (times, double, i - 1))
            {
              gtk_svg_invalid_attribute (data->svg, context, "keyTimes",
                                         "The keyTimes values must be increasing");
              g_clear_pointer (&values, g_ptr_array_unref);
              g_clear_pointer (&times, g_array_unref);
              return FALSE;
            }
        }
    }

  if (a->calc_mode != CALC_MODE_PACED)
    {
      params = 0;
      if (splines_attr)
        {
          GStrv strv;
          unsigned int n;

          params = g_array_new (FALSE, FALSE, sizeof (double));

          strv = g_strsplit (splines_attr, ";", 0);
          n = g_strv_length (strv);
          for (unsigned int i = 0; i < n; i++)
            {
              double spline[4];
              unsigned int m;
              char *s = g_strstrip (strv[i]);

              if (*s == '\0' && strv[i+1] == NULL)
                {
                  n -= 1;
                  break;
                }

              if (!parse_numbers (s, ", ", 0, 1, spline, 4, &m) ||
                  m != 4)
                {
                  gtk_svg_invalid_attribute (data->svg, context, "keySplines", NULL);
                  g_clear_pointer (&values, g_ptr_array_unref);
                  g_clear_pointer (&times, g_array_unref);
                  g_clear_pointer (&params, g_array_unref);
                  return FALSE;
                }

              g_array_append_vals (params, spline, 4);
            }

          g_strfreev (strv);

          if (times == NULL)
            times = create_default_times (a->calc_mode, n + 1);

          if (n != times->len - 1)
            {
              gtk_svg_invalid_attribute (data->svg, context, "keySplines", "wrong number of values");
              g_clear_pointer (&values, g_ptr_array_unref);
              g_clear_pointer (&times, g_array_unref);
              g_clear_pointer (&params, g_array_unref);
              return FALSE;
            }
        }
    }

  if (times == NULL)
    {
      unsigned int n;

      if (values)
        n = values->len;
      else if (points)
        n = points->len;
      else
        n = 2;

      times = create_default_times (a->calc_mode, n);
    }

  g_assert (times != NULL);

  fill_from_values (a,
                    (double *) times->data,
                    times->len,
                    values ? (SvgValue **) values->pdata : NULL,
                    params ? (double *) params->data : NULL,
                    points ? (double *) points->data : NULL);

  g_clear_pointer (&values, g_ptr_array_unref);
  g_clear_pointer (&times, g_array_unref);
  g_clear_pointer (&points, g_array_unref);
  g_clear_pointer (&params, g_array_unref);

  return TRUE;
}

static gboolean
parse_motion_animation_attrs (Animation            *a,
                              const char           *element_name,
                              const char          **attr_names,
                              const char          **attr_values,
                              uint64_t             *handled,
                              ParserData           *data,
                              GMarkupParseContext  *context)
{
  const char *path_attr = NULL;
  const char *rotate_attr = NULL;
  const char *key_points_attr = NULL;

  markup_filter_attributes (element_name,
                            attr_names, attr_values,
                            handled,
                            "path", &path_attr,
                            "rotate", &rotate_attr,
                            "keyPoints", &key_points_attr,
                            NULL);

  if (path_attr)
    {
      g_clear_pointer (&a->motion.path, gsk_path_unref);
      a->motion.path = gsk_path_parse (path_attr);

      if (a->motion.path == NULL)
        {
          gtk_svg_invalid_attribute (data->svg, context, "path", "failed to parse: %s", path_attr);
          return FALSE;
        }
    }

  a->motion.rotate = ROTATE_FIXED;
  a->motion.angle = 0;
  if (rotate_attr)
    {
      unsigned int value;
      double fixed_value;

      if (parse_number (rotate_attr, 0, 360, &fixed_value))
        a->motion.angle = fixed_value;
      else if (parse_enum (rotate_attr,
                           (const char *[]) { "auto", "auto-reverse" }, 2,
                           &value))
        a->motion.rotate = (AnimationRotate) value;
      else
        gtk_svg_invalid_attribute (data->svg, context, "rotate", "failed to parse: %s", rotate_attr);
    }

  if (a->calc_mode != CALC_MODE_PACED)
    {
      if (key_points_attr)
        {
          GArray *points = parse_numbers2 (key_points_attr, ";", 0, 1);
          if (!points)
            {
              gtk_svg_invalid_attribute (data->svg, context, "keyPoints", "failed to parse: %s", key_points_attr);
              g_array_unref (points);
              return FALSE;
            }

          if (points->len != a->n_frames)
            {
              gtk_svg_invalid_attribute (data->svg, context, "keyPoints", "wrong number of values");
              g_array_unref (points);
              return FALSE;
            }

          for (unsigned int i = 0; i < a->n_frames; i++)
            a->frames[i].point = g_array_index (points, double, i);

          g_array_unref (points);
        }
    }

  if (a->calc_mode == CALC_MODE_PACED)
    {
      /* When paced, keyTimes, keyPoints and keySplines
       * are all ignored, and we calculate times and
       * points from the path to ensure an even pace
       */

      g_clear_pointer (&a->frames, g_free);
      a->n_frames = 0;

      if (a->motion.path)
        {
          fill_from_path (a, a->motion.path);
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, "calcMode", "calcMode='paced' with <mpath> is not implemented");
          return FALSE;
        }
    }

  return TRUE;
}

/* }}} */
/* {{{ Style */

/* Priority order for attributes:
 *
 * gpa:... > class > style > presentation attrs
 *
 * class is in here for backwards compat with traditional
 * symbolic icons.
 */

/* We parse the style attribute manually here, since
 * svg_attr_parse_value doesn't use GtkCssParser
 */
static void
skip_to_semicolon (const char **p)
{
  while (**p && **p != ';')
    (*p)++;
}

static void
skip_past_semicolon (const char **p)
{
  skip_to_semicolon (p);
  if (**p) (*p)++;
}

static gboolean
consume_colon (const char **p)
{
  skip_whitespace (p);
  if (**p != ':')
    return FALSE;

  (*p)++;
  skip_whitespace (p);
  return TRUE;
}

static inline gboolean
is_multibyte (char c)
{
  return c & 0x80;
}

static inline gboolean
is_name_start (char c)
{
   return is_multibyte (c) || g_ascii_isalpha (c) || c == '_';
}

static inline gboolean
is_name (char c)
{
  return is_name_start (c) || g_ascii_isdigit (c) || c == '-';
}

static char *
consume_ident (const char **p)
{
  const char *q;
  skip_whitespace (p);
  if (!is_name_start (**p))
    return NULL;
  q = *p;
  do {
    (*p)++;
  } while (is_name (**p));
  return g_strndup (q, *p - q);
}

static char *
consume_to_semicolon (const char **p)
{
  const char *q;
  skip_whitespace (p);
  q = *p;
  skip_to_semicolon (p);
  return g_strndup (q, *p - q);
}

static gboolean
attr_lookup (const char          *name,
             gboolean             for_stop,
             gboolean             for_filter,
             ShapeType            type,
             FilterPrimitiveType  filter_type,
             ShapeAttr           *attr,
             gboolean            *deprecated)
{
  *deprecated = FALSE;

  if (for_filter)
    return filter_attr_lookup (filter_type, name, attr, deprecated);

  if (!shape_attr_lookup (name, type, attr, deprecated))
    return FALSE;

  if (for_stop)
    return FIRST_STOP_ATTR <= *attr && *attr <= LAST_STOP_ATTR;

  return shape_has_attr (type, *attr);
}

static void
parse_style_attr (Shape               *shape,
                  gboolean             for_stop,
                  gboolean             for_filter,
                  const char          *style_attr,
                  ParserData          *data,
                  GMarkupParseContext *context)
{
  const char *p = style_attr;
  ShapeAttr attr;
  gboolean deprecated;
  char *name;
  char *prop_val;
  SvgValue *value;
  unsigned int idx = 0;
  gboolean is_marker_shorthand;
  FilterPrimitiveType filter_type = 0;

  if (for_stop)
    {
      g_assert (shape->color_stops->len > 0);
      idx = shape->color_stops->len;
    }
  else if (for_filter)
    {
      g_assert (shape->filters->len > 0);
      idx = shape->filters->len;
      FilterPrimitive *fp = g_ptr_array_index (shape->filters, idx - 1);
      filter_type = fp->type;
    }

  while (*p)
    {
      is_marker_shorthand = FALSE;
      skip_whitespace (&p);
      name = consume_ident (&p);

      if (!name)
        {
          gtk_svg_invalid_attribute (data->svg, context,
                                     "style", "while parsing 'style': expected identifier");
          skip_past_semicolon (&p);
          continue;
        }

      if (strcmp (name, "marker") == 0 &&
          shape_has_attr (shape->type, SHAPE_ATTR_MARKER_START))
        {
          attr = SHAPE_ATTR_MARKER_START;
          is_marker_shorthand = TRUE;
          deprecated = FALSE;
        }
      else if (!attr_lookup (name, for_stop, for_filter, shape->type, filter_type, &attr, &deprecated) ||
               !shape_attr_has_css (attr))
        {
          gtk_svg_invalid_attribute (data->svg, context,
                                     "style", "while parsing 'style': unsupported property '%s'",
                                     name);
          g_free (name);
          skip_past_semicolon (&p);
          continue;
        }

      g_free (name);

      if (!consume_colon (&p))
        {
          gtk_svg_invalid_attribute (data->svg, context,
                                     "style", "while parsing 'style': expected ':' after '%s'",
                                     shape_attr_get_presentation (attr, shape->type));
          skip_past_semicolon (&p);
          continue;
        }

      if (!*p)
        {
          gtk_svg_invalid_attribute (data->svg, context,
                                     "style", "while parsing 'style': value expected after ':'");
          break;
        }

      prop_val = consume_to_semicolon (&p);
      if (deprecated && _gtk_bitmask_get (shape->attrs, attr))
        {
          /* ignore */
        }
      else
        {
          value = shape_attr_parse_value (attr, prop_val);
          if (!value)
            {
              gtk_svg_invalid_attribute (data->svg, context,
                                         "style", "failed to parse '%s' value '%s'",
                                         shape_attr_get_presentation (attr, shape->type),
                                         prop_val);
            }
          else
            {
              shape_set_base_value (shape, attr, idx, value);
              if (is_marker_shorthand)
                {
                  shape_set_base_value (shape, SHAPE_ATTR_MARKER_MID, idx, value);
                  shape_set_base_value (shape, SHAPE_ATTR_MARKER_END, idx, value);
                }
              svg_value_unref (value);
            }
        }

      g_free (prop_val);
      skip_past_semicolon (&p);
    }
}

/* }}} */
/* {{{ Shape attributes */

static void
parse_shape_attrs (Shape                *shape,
                   const char           *element_name,
                   const char          **attr_names,
                   const char          **attr_values,
                   uint64_t             *handled,
                   ParserData           *data,
                   GMarkupParseContext  *context)
{
  const char *class_attr = NULL;
  const char *style_attr = NULL;

  if (data->svg->features & GTK_SVG_TRADITIONAL_SYMBOLIC)
    class_attr = "foreground-fill";

  for (unsigned int i = 0; attr_names[i]; i++)
    {
      ShapeAttr attr;
      gboolean deprecated;

      if (*handled & BIT (i))
        continue;

      if (shape->type == SHAPE_SVG &&
          (g_str_has_prefix (attr_names[i], "xmlns") ||
           strcmp (attr_names[i], "version") == 0 ||
           strcmp (attr_names[i], "baseProfile") == 0))
        {
          *handled |= BIT (i);
          continue;
        }

      /* We handle class and style after the loop to
       * enforce priority over fill/stroke
       */
      if (strcmp (attr_names[i], "class") == 0)
        {
          class_attr = attr_values[i];
          *handled |= BIT (i);
        }
      else if (strcmp (attr_names[i], "style") == 0)
        {
          style_attr = attr_values[i];
          *handled |= BIT (i);
        }
      else if (strcmp (attr_names[i], "id") == 0)
        {
          shape->id = g_strdup (attr_values[i]);
          *handled |= BIT (i);
        }
      else if (shape_attr_lookup (attr_names[i], shape->type, &attr, &deprecated) &&
               !shape_attr_only_css (attr))
        {
          if (shape_has_attr (shape->type, attr))
            {
              if (deprecated && _gtk_bitmask_get (shape->attrs, attr))
                {
                  /* ignore */
                }
              else
                {
                  SvgValue *value = shape_attr_parse_value (attr, attr_values[i]);
                  if (!value)
                    {
                      gtk_svg_invalid_attribute (data->svg, context, attr_names[i], NULL);
                    }
                  else
                    {
                      shape_set_base_value (shape, attr, 0, value);
                       svg_value_unref (value);
                    }
                }
            }
          else
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names[i], NULL);
            }

          *handled |= BIT (i);
        }
    }

  if (style_attr)
    parse_style_attr (shape, FALSE, FALSE, style_attr, data, context);

  if ((data->svg->features & (GTK_SVG_EXTENSIONS|GTK_SVG_TRADITIONAL_SYMBOLIC)) != 0 &&
      class_attr && *class_attr)
    {
      GStrv classes = g_strsplit (class_attr, " ", 0);
      SvgValue *value;
      gboolean has_stroke;

      if (g_strv_has (classes, "transparent-fill"))
        value = svg_paint_new_none ();
      else if (g_strv_has (classes, "foreground-fill"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_FOREGROUND);
      else if (g_strv_has (classes, "success") ||
               g_strv_has (classes, "success-fill"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_SUCCESS);
      else if (g_strv_has (classes, "warning") ||
               g_strv_has (classes, "warning-fill"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_WARNING);
      else if (g_strv_has (classes, "error") ||
               g_strv_has (classes, "error-fill"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_ERROR);
      else
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_FOREGROUND);

      if (!_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_FILL) ||
          (data->svg->features & GTK_SVG_TRADITIONAL_SYMBOLIC))
        shape_set_base_value (shape, SHAPE_ATTR_FILL, 0, value);
      svg_value_unref (value);

      if (g_strv_has (classes, "success-stroke"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_SUCCESS);
      else if (g_strv_has (classes, "warning-stroke"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_WARNING);
      else if (g_strv_has (classes, "error-stroke"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_ERROR);
      else if (g_strv_has (classes, "foreground-stroke"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_FOREGROUND);
      else
        value = svg_paint_new_none ();

      has_stroke = !svg_value_equal (value, svg_paint_new_none ());

      if (!_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_STROKE) ||
          (data->svg->features & GTK_SVG_TRADITIONAL_SYMBOLIC))
        shape_set_base_value (shape, SHAPE_ATTR_STROKE, 0, value);
      svg_value_unref (value);

      if (has_stroke)
        {
          if (!_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_STROKE_WIDTH) ||
              (data->svg->features & GTK_SVG_TRADITIONAL_SYMBOLIC))
            {
              value = svg_number_new (2);
              shape_set_base_value (shape, SHAPE_ATTR_STROKE_WIDTH, 0, value);
              svg_value_unref (value);
            }

          if (!_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_STROKE_LINEJOIN) ||
              (data->svg->features & GTK_SVG_TRADITIONAL_SYMBOLIC))
            {
              value = svg_linejoin_new (GSK_LINE_JOIN_ROUND);
              shape_set_base_value (shape, SHAPE_ATTR_STROKE_LINEJOIN, 0, value);
              svg_value_unref (value);
            }

          if (!_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_STROKE_LINECAP) ||
              (data->svg->features & GTK_SVG_TRADITIONAL_SYMBOLIC))
            {
              value = svg_linecap_new (GSK_LINE_CAP_ROUND);
              shape_set_base_value (shape, SHAPE_ATTR_STROKE_LINECAP, 0, value);
              svg_value_unref (value);
            }
        }

      g_strfreev (classes);
    }

  if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_CLIP_PATH) ||
      _gtk_bitmask_get (shape->attrs, SHAPE_ATTR_MASK) ||
      _gtk_bitmask_get (shape->attrs, SHAPE_ATTR_HREF) ||
      _gtk_bitmask_get (shape->attrs, SHAPE_ATTR_FILTER) ||
      _gtk_bitmask_get (shape->attrs, SHAPE_ATTR_MARKER_START) ||
      _gtk_bitmask_get (shape->attrs, SHAPE_ATTR_MARKER_MID) ||
      _gtk_bitmask_get (shape->attrs, SHAPE_ATTR_MARKER_END))
    g_ptr_array_add (data->pending_refs, shape);

  if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_FILL))
    {
      SvgPaint *paint = (SvgPaint *) shape->base[SHAPE_ATTR_FILL];
      if ((data->svg->features & GTK_SVG_EXTENSIONS) == 0 &&
          paint->kind == PAINT_SYMBOLIC)
        {
          SvgValue *value = svg_paint_new_black ();
          shape_set_base_value (shape, SHAPE_ATTR_FILL, 0, value);
          svg_value_unref (value);
        }
      else if (paint_is_server (paint->kind))
        {
          g_ptr_array_add (data->pending_refs, shape);
        }
    }

  if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_STROKE))
    {
      SvgPaint *paint = (SvgPaint *) shape->base[SHAPE_ATTR_STROKE];

      if ((data->svg->features & GTK_SVG_EXTENSIONS) == 0 &&
          paint->kind == PAINT_SYMBOLIC)
        {
          SvgValue *value = svg_paint_new_black ();
          shape_set_base_value (shape, SHAPE_ATTR_STROKE, 0, value);
          svg_value_unref (value);
        }
      else if (paint_is_server (paint->kind))
        {
          g_ptr_array_add (data->pending_refs, shape);
        }
    }

  if (shape_has_attr (shape->type, SHAPE_ATTR_FX) &&
      shape_has_attr (shape->type, SHAPE_ATTR_FY))
    {
      if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_CX) &&
          !_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_FX))
        shape_set_base_value (shape, SHAPE_ATTR_FX, 0, shape->base[SHAPE_ATTR_CX]);
      if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_CY) &&
          !_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_FY))
        shape_set_base_value (shape, SHAPE_ATTR_FY, 0, shape->base[SHAPE_ATTR_CY]);
    }
}

static void
parse_svg_gpa_attrs (GtkSvg               *svg,
                     const char           *element_name,
                     const char          **attr_names,
                     const char          **attr_values,
                     uint64_t             *handled,
                     ParserData           *data,
                     GMarkupParseContext  *context)
{
  const char *state_attr = NULL;
  const char *version_attr = NULL;
  const char *keywords_attr = NULL;

  markup_filter_attributes (element_name,
                            attr_names, attr_values,
                            handled,
                            "gpa:state", &state_attr,
                            "gpa:version", &version_attr,
                            "gpa:keywords", &keywords_attr,
                            NULL);

  if (state_attr)
    {
      double v;

      if (strcmp (state_attr, "empty") == 0)
        gtk_svg_set_state (svg, GTK_SVG_STATE_EMPTY);
      else if (!parse_number (state_attr, -1, 63, &v))
        gtk_svg_invalid_attribute (svg, context, "gpa:state", NULL);
      else if (v < 0)
        gtk_svg_set_state (svg, GTK_SVG_STATE_EMPTY);
      else
        gtk_svg_set_state (svg, (unsigned int) CLAMP (v, 0, 63));
    }

  if (version_attr)
    {
      unsigned int version;
      char *end;

      version = (unsigned int) g_ascii_strtoull (version_attr, &end, 10);
      if ((end && *end != '\0') || version != 1)
        gtk_svg_invalid_attribute (svg, context, "gpa:version", "must be 1");
      else
        svg->gpa_version = version;
    }

  if (keywords_attr)
    svg->keywords = g_strdup (keywords_attr);
}

static void
parse_shape_gpa_attrs (Shape                *shape,
                       const char           *element_name,
                       const char          **attr_names,
                       const char          **attr_values,
                       uint64_t             *handled,
                       ParserData           *data,
                       GMarkupParseContext  *context)
{
  const char *stroke_attr = NULL;
  const char *fill_attr = NULL;
  const char *strokewidth_attr = NULL;
  const char *states_attr = NULL;
  const char *transition_type_attr = NULL;
  const char *transition_duration_attr = NULL;
  const char *transition_delay_attr = NULL;
  const char *transition_easing_attr = NULL;
  const char *animation_type_attr = NULL;
  const char *animation_direction_attr = NULL;
  const char *animation_duration_attr = NULL;
  const char *animation_repeat_attr = NULL;
  const char *animation_segment_attr = NULL;
  const char *animation_easing_attr = NULL;
  const char *origin_attr = NULL;
  const char *attach_to_attr = NULL;
  const char *attach_pos_attr = NULL;
  SvgValue *value;
  uint64_t states;
  double origin;
  unsigned int transition_type;
  int64_t transition_duration;
  int64_t transition_delay;
  unsigned int transition_easing;
  unsigned int has_animation;
  unsigned int animation_direction;
  int64_t animation_duration;
  double animation_repeat;
  unsigned int animation_easing;
  double animation_segment;
  double attach_pos;

  if (!shape_type_has_gpa_attrs (shape->type))
    return;

  markup_filter_attributes (element_name,
                            attr_names, attr_values,
                            handled,
                            "gpa:stroke", &stroke_attr,
                            "gpa:fill", &fill_attr,
                            "gpa:stroke-width", &strokewidth_attr,
                            "gpa:states", &states_attr,
                            "gpa:origin", &origin_attr,
                            "gpa:transition-type", &transition_type_attr,
                            "gpa:transition-duration", &transition_duration_attr,
                            "gpa:transition-delay", &transition_delay_attr,
                            "gpa:transition-easing", &transition_easing_attr,
                            "gpa:animation-type", &animation_type_attr,
                            "gpa:animation-direction", &animation_direction_attr,
                            "gpa:animation-duration", &animation_duration_attr,
                            "gpa:animation-repeat", &animation_repeat_attr,
                            "gpa:animation-segment", &animation_segment_attr,
                            "gpa:animation-easing", &animation_easing_attr,
                            "gpa:attach-to", &attach_to_attr,
                            "gpa:attach-pos", &attach_pos_attr,
                            NULL);

  if (stroke_attr)
    {
      value = svg_paint_parse_gpa (stroke_attr);
      if (value)
        {
          shape_set_base_value (shape, SHAPE_ATTR_STROKE, 0, value);
          svg_value_unref (value);
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, "gpa:stroke", NULL);
        }
    }

  if (fill_attr)
    {
      value = svg_paint_parse_gpa (fill_attr);
      if (value)
        {
          shape_set_base_value (shape, SHAPE_ATTR_FILL, 0, value);
          svg_value_unref (value);
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, "gpa:fill", NULL);
        }
    }

  if (strokewidth_attr)
    {
      GStrv strv;
      SvgValue *values[3] = { NULL, };

      strv = g_strsplit (strokewidth_attr, " ", 0);
      if (g_strv_length (strv) == 3)
        {
          values[0] = svg_number_parse (strv[0], 0, DBL_MAX, NUMBER | LENGTH | PERCENTAGE);
          values[1] = svg_number_parse (strv[1], 0, DBL_MAX, NUMBER | LENGTH | PERCENTAGE);
          values[2] = svg_number_parse (strv[2], 0, DBL_MAX, NUMBER | LENGTH | PERCENTAGE);
        }

      if (values[0] && values[1] && values[2])
        {
          shape_set_base_value (shape, SHAPE_ATTR_STROKE_MINWIDTH, 0, values[0]);
          shape_set_base_value (shape, SHAPE_ATTR_STROKE_WIDTH, 0, values[1]);
          shape_set_base_value (shape, SHAPE_ATTR_STROKE_MAXWIDTH, 0, values[2]);
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, "gpa:stroke-width", NULL);
        }

      g_clear_pointer (&values[0], svg_value_unref);
      g_clear_pointer (&values[1], svg_value_unref);
      g_clear_pointer (&values[2], svg_value_unref);
      g_strfreev (strv);
    }

  states = ALL_STATES;
  if (states_attr)
    {
      if (!parse_states (states_attr, &states))
        {
          gtk_svg_invalid_attribute (data->svg, context, "gpa:states", NULL);
          states = ALL_STATES;
        }
      else
        {
          if (states != NO_STATES)
            data->svg->max_state = MAX (data->svg->max_state, g_bit_nth_msf (states, -1));
        }
    }

  origin = 0;
  if (origin_attr)
    {
      if (!parse_number (origin_attr, 0, 1, &origin))
        gtk_svg_invalid_attribute (data->svg, context, "gpa:origin", NULL);
    }

  transition_type = GPA_TRANSITION_NONE;
  if (transition_type_attr)
    {
      if (!parse_enum (transition_type_attr,
                       (const char *[]) { "none", "animate", "morph", "fade" }, 4,
                        &transition_type))
        gtk_svg_invalid_attribute (data->svg, context, "gpa:transition-type", NULL);
    }

  transition_duration = 0;
  if (transition_duration_attr)
    {
      if (!parse_duration (transition_duration_attr, &transition_duration))
        gtk_svg_invalid_attribute (data->svg, context, "gpa:transition-duration", NULL);
    }

  transition_delay = 0;
  if (transition_delay_attr)
    {
      if (!parse_duration (transition_delay_attr, &transition_delay))
        gtk_svg_invalid_attribute (data->svg, context, "gpa:transition-delay", NULL);
    }

  transition_easing = GPA_EASING_LINEAR;
  if (transition_easing_attr)
    {

      if (!parse_enum (transition_easing_attr,
                       (const char *[]) { "linear", "ease-in-out", "ease-in",
                                          "ease-out", "ease" }, 5,
                        &transition_easing))
        gtk_svg_invalid_attribute (data->svg, context, "gpa:transition-easing", NULL);
    }

  has_animation = 0;
  if (animation_type_attr)
    {
      if (!parse_enum (animation_type_attr,
                       (const char *[]) { "none", "automatic", }, 2,
                        &has_animation))
        gtk_svg_invalid_attribute (data->svg, context, "gpa:animation-type", NULL);
    }

  animation_direction = GPA_ANIMATION_NONE;
  if (has_animation && animation_direction_attr)
    {
      if (!parse_enum (animation_direction_attr,
                       (const char *[]) { "none", "normal", "alternate", "reverse",
                                          "reverse-alternate", "in-out",
                                          "in-out-alternate", "in-out-reverse",
                                          "segment", "segment-alternate" }, 10,
                        &animation_direction))
        gtk_svg_invalid_attribute (data->svg, context, "gpa:animation-direction", NULL);
    }

  animation_duration = 0;
  if (animation_duration_attr)
    {
      if (!parse_duration (animation_duration_attr, &animation_duration))
        gtk_svg_invalid_attribute (data->svg, context, "gpa:animation-duration", NULL);
    }

  animation_repeat = REPEAT_FOREVER;
  if (animation_repeat_attr)
    {
      if (match_str (animation_repeat_attr, "indefinite"))
        animation_repeat = REPEAT_FOREVER;
      else if (!parse_number (animation_repeat_attr, 0, DBL_MAX, &animation_repeat))
        gtk_svg_invalid_attribute (data->svg, context, "gpa:animation-repeat", NULL);
    }

  animation_segment = 0.2;
  if (animation_segment_attr)
    {
      if (!parse_number (animation_segment_attr, 0, 1, &animation_segment))
        gtk_svg_invalid_attribute (data->svg, context, "gpa:animation-segment", NULL);
    }

  animation_easing = GPA_EASING_LINEAR;
  if (animation_easing_attr)
    {
      if (!parse_enum (animation_easing_attr,
                       (const char *[]) { "linear", "ease-in-out", "ease-in",
                                          "ease-out", "ease" }, 5,
                        &animation_easing))
        gtk_svg_invalid_attribute (data->svg, context, "gpa:animation-easing", NULL);
    }

  attach_pos = 0;
  if (attach_pos_attr)
    {
      if (!parse_number (attach_pos_attr, 0, 1, &attach_pos))
        gtk_svg_invalid_attribute (data->svg, context, "gpa:attach-pos", NULL);
    }

  shape->gpa.states = states;
  shape->gpa.transition = transition_type;
  shape->gpa.transition_easing = transition_easing;
  shape->gpa.transition_duration = transition_duration;
  shape->gpa.transition_delay = transition_delay;
  shape->gpa.animation = animation_direction;
  shape->gpa.animation_easing = animation_easing;
  shape->gpa.animation_duration = animation_duration;
  shape->gpa.animation_repeat = animation_repeat;
  shape->gpa.animation_segment = animation_segment;
  shape->gpa.origin = origin;
  shape->gpa.attach.ref = g_strdup (attach_to_attr);
  shape->gpa.attach.shape = NULL;
  shape->gpa.attach.pos = attach_pos;

  if ((data->svg->features & GTK_SVG_ANIMATIONS) == 0)
    return;

  if (attach_to_attr)
    g_ptr_array_add (data->pending_refs, shape);

  if (shape->gpa.transition != GPA_TRANSITION_NONE ||
      shape->gpa.animation != GPA_ANIMATION_NONE)
    {
      /* our dasharray-based animations require unit path length */
      if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_PATH_LENGTH))
        gtk_svg_invalid_attribute (data->svg, context, NULL, "Can't set %s and use gpa features", "pathLength");

      if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_STROKE_DASHARRAY))
        gtk_svg_invalid_attribute (data->svg, context, NULL, "Can't set %s and use gpa features", "stroke-dasharray");

      if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_STROKE_DASHOFFSET))
        gtk_svg_invalid_attribute (data->svg, context, NULL, "Can't set %s and use gpa features", "stroke-dashoffset");
    }

  create_states (shape,
                 data->svg->timeline,
                 states,
                 transition_delay,
                 data->svg->state);

  if (attach_to_attr ||
      transition_type == GPA_TRANSITION_ANIMATE ||
      animation_direction != GPA_ANIMATION_NONE)
    create_path_length (shape, data->svg->timeline);

  if (attach_to_attr)
    create_attachment (shape,
                       data->svg->timeline,
                       states,
                       attach_to_attr,
                       attach_pos,
                       origin);

  create_transitions (shape,
                      data->svg->timeline,
                      states,
                      transition_type,
                      transition_duration,
                      transition_delay,
                      transition_easing,
                      origin);

  create_animations (shape,
                     data->svg->timeline,
                     states,
                     data->svg->state,
                     animation_repeat,
                     animation_duration,
                     animation_direction,
                     animation_easing,
                     animation_segment,
                     origin);
}

/* }}} */

G_GNUC_PRINTF (3, 4)
static void
skip_element (ParserData          *data,
              GMarkupParseContext *context,
              const char          *format,
              ...)
{
  va_list args;

  gtk_svg_location_init (&data->skip.start, context);
  data->skip.to = g_markup_parse_context_get_element_stack (context);
  data->skip.skip_over_target = TRUE;

  va_start (args, format);
  g_vasprintf (&data->skip.reason, format, args);
  va_end (args);
}

static void
start_element_cb (GMarkupParseContext  *context,
                  const char           *element_name,
                  const char          **attr_names,
                  const char          **attr_values,
                  gpointer              user_data,
                  GError              **gmarkup_error)
{
  ParserData *data = user_data;
  ShapeType shape_type;
  Shape *shape = NULL;
  uint64_t handled = 0;
  FilterPrimitiveType filter_type;

  if (data->skip.to)
    return;

  if (data->num_loaded_elements++ > LOADING_LIMIT)
    {
      gtk_svg_location_init (&data->skip.start, context);
      data->skip.to = g_markup_parse_context_get_element_stack (context)->next;
      data->skip.reason = g_strdup ("Loading limit exceeded");
      data->skip.skip_over_target = FALSE;
      return;
    }

  if (shape_type_lookup (element_name, &shape_type))
    {
      if (data->current_shape &&
          !shape_type_has_shapes (data->current_shape->type))
        {
          skip_element (data, context, "Parent element can't contain shapes");
          return;
        }

      if (shape_type != SHAPE_USE &&
          (BIT (shape_type) & CLIP_PATH_CONTENT) == 0 &&
          has_ancestor (context, "clipPath") &&
          shape_type != SHAPE_CLIP_PATH)
        {
          skip_element (data, context, "<clipPath> can only contain shapes, not %s", element_name);
          return;
        }

      shape = shape_new (data->current_shape, shape_type);
      g_markup_parse_context_get_position (context, &shape->line, NULL);

      if (data->current_shape == NULL && shape->type == SHAPE_SVG)
        {
          data->svg->content = shape;

          if (data->svg->features & GTK_SVG_EXTENSIONS)
            parse_svg_gpa_attrs (data->svg,
                                 element_name, attr_names, attr_values,
                                 &handled, data, context);
        }

      parse_shape_attrs (shape,
                         element_name, attr_names, attr_values,
                         &handled, data, context);

      if (data->svg->gpa_version > 0)
        parse_shape_gpa_attrs (shape,
                               element_name, attr_names, attr_values,
                               &handled, data, context);

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (data->current_shape)
        g_ptr_array_add (data->current_shape->shapes, shape);

      data->shape_stack = g_slist_prepend (data->shape_stack, data->current_shape);

      if (data->current_shape &&
          shape_type_has_text (data->current_shape->type) &&
          shape->type == SHAPE_TSPAN)
        {
          TextNode node = {
            .type = TEXT_NODE_SHAPE,
            .shape = { .shape = shape },
          };
          g_array_append_val (data->current_shape->text, node);
        }

      data->current_shape = shape;

      if (shape->id && !g_hash_table_contains (data->shapes, shape->id))
        g_hash_table_insert (data->shapes, shape->id, shape);

      return;
    }

  if (strcmp (element_name, "stop") == 0)
    {
      const char *parent = g_markup_parse_context_get_element_stack (context)->next->data;
      SvgValue *value;
      const char *style_attr = NULL;
      unsigned int idx;

      if (strcmp (parent, "linearGradient") != 0 &&
          strcmp (parent, "radialGradient") != 0)
        {
          skip_element (data, context, "<stop> only allowed in <linearGradient> or <radialGradient>");
          return;
        }

      shape_add_color_stop (data->current_shape);
      idx = data->current_shape->color_stops->len - 1;
      for (unsigned int i = 0; attr_names[i]; i++)
        {
          if (strcmp (attr_names[i], "offset") == 0)
            {
              handled |= BIT (i);
              value = shape_attr_parse_value (SHAPE_ATTR_STOP_OFFSET, attr_values[i]);
              if (value)
                {
                  shape_set_base_value (data->current_shape, SHAPE_ATTR_STOP_OFFSET, idx + 1, value);
                  svg_value_unref (value);
                }
              else
                gtk_svg_invalid_attribute (data->svg, context, "offset", NULL);
            }
          else if (strcmp (attr_names[i], "stop-color") == 0)
            {
              handled |= BIT (i);
              value = shape_attr_parse_value (SHAPE_ATTR_STOP_COLOR, attr_values[i]);
              if (value)
                {
                  shape_set_base_value (data->current_shape, SHAPE_ATTR_STOP_COLOR, idx + 1, value);
                  svg_value_unref (value);
                }
              else
                gtk_svg_invalid_attribute (data->svg, context, "stop-color", NULL);
            }
          else if (strcmp (attr_names[i], "stop-opacity") == 0)
            {
              handled |= BIT (i);
              value = shape_attr_parse_value (SHAPE_ATTR_STOP_OPACITY, attr_values[i]);
              if (value)
                {
                  shape_set_base_value (data->current_shape, SHAPE_ATTR_STOP_OPACITY, idx + 1, value);
                  svg_value_unref (value);
                }
              else
                gtk_svg_invalid_attribute (data->svg, context, "stop-opacity", NULL);
            }
          else if (strcmp (attr_names[i], "style") == 0)
            {
              handled |= BIT (i);
              style_attr = attr_values[i];
            }
        }

      if (style_attr)
        parse_style_attr (data->current_shape, TRUE, FALSE, style_attr, data, context);

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      return;
    }

  if (filter_type_lookup (element_name, &filter_type))
    {
      const char *style_attr = NULL;
      unsigned int idx;
      gboolean values_set = FALSE;
      FilterPrimitive *f;

      if (filter_type == FE_MERGE_NODE)
        {
          if (!check_ancestors (context, "feMerge", "filter", NULL))
            {
              skip_element (data, context, "<%s> only allowed in <feMerge>", element_name);
              return;
            }

        }
      else if (filter_type == FE_FUNC_R ||
               filter_type == FE_FUNC_G ||
               filter_type == FE_FUNC_B ||
               filter_type == FE_FUNC_A)
        {
          if (!check_ancestors (context, "feComponentTransfer", "filter", NULL))
            {
              skip_element (data, context, "<%s> only allowed in <feComponentTransfer>", element_name);
              return;
            }
        }
      else
        {
          if (!check_ancestors (context, "filter", NULL))
            {
              skip_element (data, context, "<%s> only allowed in <filter>", element_name);
              return;
            }
        }

      idx = shape_add_filter (data->current_shape, filter_type);
      f = g_ptr_array_index (data->current_shape->filters, idx);

      for (unsigned int i = 0; attr_names[i]; i++)
        {
          ShapeAttr attr;
          gboolean deprecated;

          if (strcmp (attr_names[i], "style") == 0)
            {
              handled |= BIT (i);
              style_attr = attr_values[i];
            }
          else if (filter_attr_lookup (filter_type, attr_names[i], &attr, &deprecated))
            {
              if (deprecated && (f->attrs & BIT (filter_attr_idx (filter_type, attr))) != 0)
                {
                  /* ignore */
                }
              else
                {
                  SvgValue *value = shape_attr_parse_value (attr, attr_values[i]);
                  handled |= BIT (i);
                  if (value)
                    {
                      shape_set_base_value (data->current_shape, attr, idx + 1, value);
                      svg_value_unref (value);
                    }
                  else
                    gtk_svg_invalid_attribute (data->svg, context, attr_names[i], NULL);
                  if (attr == SHAPE_ATTR_FE_COLOR_MATRIX_VALUES)
                    values_set = TRUE;
                }
            }
        }

      if (style_attr)
        parse_style_attr (data->current_shape, FALSE, TRUE, style_attr, data, context);

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (filter_type == FE_IMAGE)
        g_ptr_array_add (data->pending_refs, data->current_shape);

      if (filter_type == FE_COLOR_MATRIX)
        {
          SvgNumbers *values = (SvgNumbers *) f->base[filter_attr_idx (f->type, SHAPE_ATTR_FE_COLOR_MATRIX_VALUES)];
          SvgNumbers *initial = (SvgNumbers *) filter_attr_ref_initial_value (f, SHAPE_ATTR_FE_COLOR_MATRIX_VALUES);

          if (values->n_values != initial->n_values)
            {
              shape_set_base_value (data->current_shape, SHAPE_ATTR_FE_COLOR_MATRIX_VALUES, idx + 1, (SvgValue *) initial);
              if (values_set)
                {
                  /* If this wasn't user-provided, we quietly correct the initial
                   * value to match the type
                   */
                  gtk_svg_invalid_attribute (data->svg, context, "values", NULL);
                }
            }
          svg_value_unref ((SvgValue *) initial);
        }

      return;
    }

  if (strcmp (element_name, "metadata") == 0)
    {
      return;
    }

  if (strcmp (element_name, "rdf:RDF") == 0 ||
      strcmp (element_name, "cc:Work") == 0 ||
      strcmp (element_name, "dc:subject") == 0 ||
      strcmp (element_name, "dc:description") == 0 ||
      strcmp (element_name, "dc:creator") == 0 ||
      strcmp (element_name, "cc:Agent") == 0 ||
      strcmp (element_name, "dc:title") == 0 ||
      strcmp (element_name, "cc:license") == 0 ||
      strcmp (element_name, "rdf:Bag") == 0 ||
      strcmp (element_name, "rdf:li") == 0)
    {
      if (!has_ancestor (context, "metadata"))
        skip_element (data, context, "Ignoring RDF elements outside <metadata>: <%s>", element_name);

      if (strcmp (element_name, "rdf:li") == 0)
        {
          /* Verify we're in the right place */
          if (check_ancestors (context, "rdf:Bag", "dc:subject", "cc:Work", "rdf:RDF", "metadata", NULL))
            {
              data->collect_text = TRUE;
              g_string_set_size (data->text, 0);
            }
          else
            skip_element (data, context, "Ignoring RDF element in wrong context: <%s>", element_name);
        }
      else if (strcmp (element_name, "dc:description") == 0)
        {
          if (check_ancestors (context, "cc:Work", "rdf:RDF", "metadata", NULL))
            {
              data->collect_text = TRUE;
              g_string_set_size (data->text, 0);
            }
          else
            skip_element (data, context, "Ignoring RDF element in wrong context: <%s>", element_name);
        }
      else if (strcmp (element_name, "dc:title") == 0)
        {
          if (check_ancestors (context, "cc:Agent", "dc:creator", "cc:Work", "rdf:RDF", "metadata", NULL))
            {
              data->collect_text = TRUE;
              g_string_set_size (data->text, 0);
            }
          else
            skip_element (data, context, "Ignoring RDF element in wrong context: <%s>", element_name);
        }
      else if (strcmp (element_name, "cc:license") == 0)
        {
          const char *license = NULL;

          markup_filter_attributes (element_name,
                                    attr_names, attr_values,
                                    &handled,
                                    "rdf:resource", &license,
                                    NULL);

          data->svg->license = g_strdup (license);
        }

      return;
    }

  if (strcmp (element_name, "font-face") == 0 ||
      strcmp (element_name, "font-face-src") == 0)
    {
      return;
    }

  if (strcmp (element_name, "font-face-uri") == 0)
    {
      if (check_ancestors (context, "font-face-src", "font-face", NULL))
        {
          for (unsigned int i = 0; attr_names[i]; i++)
            {
              if (strcmp (attr_names[i], "href") == 0)
                {
                  if (!add_font_from_url (data->svg, context, attr_values[i]))
                    {
                      // too bad
                    }
                }
            }
        }
      else
        skip_element (data, context, "Ignoring font element in the wrong context: <%s>", element_name);

      return;
    }

  if (strcmp (element_name, "style") == 0 ||
      strcmp (element_name, "title") == 0 ||
      strcmp (element_name, "desc") == 0 ||
      g_str_has_prefix (element_name, "sodipodi:") ||
      g_str_has_prefix (element_name, "inkscape:"))
    {
      skip_element (data, context, "Ignoring metadata and style elements: <%s>", element_name);
      return;
    }

  if (strcmp (element_name, "set") == 0)
    {
      Animation *a;
      const char *to_attr = NULL;
      SvgValue *value;

      if ((data->svg->features & GTK_SVG_ANIMATIONS) == 0)
        {
          skip_element (data, context, "Animations are disabled");
          return;
        }

      if (data->current_animation)
        {
          skip_element (data, context, "Nested animation elements are not allowed: <set>");
          return;
        }

      a = animation_set_new ();

      markup_filter_attributes (element_name,
                                attr_names, attr_values,
                                &handled,
                                "to", &to_attr,
                                NULL);

      if (!parse_base_animation_attrs (a,
                                       element_name,
                                       attr_names, attr_values,
                                       &handled,
                                       data,
                                       context))
        {
          animation_drop_and_free (a);
          skip_element (data, context, "Skipping <%s> - bad attributes", element_name);
          return;
        }

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (!to_attr)
        {
          gtk_svg_missing_attribute (data->svg, context, "to", NULL);
          animation_drop_and_free (a);
          skip_element (data, context, "Dropping <set> without 'to'");
          return;
        }

      a->calc_mode = CALC_MODE_DISCRETE;
      a->frames = g_new0 (Frame, 2);
      a->frames[0].time = 0;
      a->frames[1].time = 1;

      value = shape_attr_parse_value (a->attr, to_attr);
      if (!value)
        {
          gtk_svg_invalid_attribute (data->svg, context, "to", "Failed to parse: %s", to_attr);
          animation_drop_and_free (a);
          skip_element (data, context, "Dropping <set> without 'to'");
          return;
        }

      a->frames[0].value = svg_value_ref (value);
      a->frames[1].value = svg_value_ref (value);
      a->n_frames = 2;

      svg_value_unref (value);

      if (!a->href || g_strcmp0 (a->href, data->current_shape->id) == 0)
        shape_add_animation (data->current_shape, a);
      else
        g_ptr_array_add (data->pending_animations, a);

      if (a->id)
        g_hash_table_insert (data->animations, a->id, a);

      data->current_animation = a;

      return;
    }

  if (strcmp (element_name, "animate") == 0 ||
      strcmp (element_name, "animateTransform") == 0 ||
      strcmp (element_name, "animateMotion") == 0)
    {
      Animation *a;

      if ((data->svg->features & GTK_SVG_ANIMATIONS) == 0)
        {
          skip_element (data, context, "Animations are disabled");
          return;
        }

      if (data->current_animation)
        {
          skip_element (data, context, "Nested animation elements are not allowed: <%s>", element_name);
          return;
        }

      if (strcmp (element_name, "animate") == 0)
        a = animation_animate_new ();
      else if (strcmp (element_name, "animateTransform") == 0)
        a = animation_transform_new ();
      else
        a = animation_motion_new ();

      g_markup_parse_context_get_position (context, &a->line, NULL);

      if (!parse_base_animation_attrs (a,
                                       element_name,
                                       attr_names, attr_values,
                                       &handled,
                                       data,
                                       context))
        {
          animation_drop_and_free (a);
          skip_element (data, context, "Skipping <%s> - bad attributes", element_name);
          return;
        }

      if (!parse_value_animation_attrs (a,
                                        element_name,
                                        attr_names, attr_values,
                                        &handled,
                                        data,
                                        context))
        {
          animation_drop_and_free (a);
          skip_element (data, context, "Skipping <%s> - bad attributes", element_name);
          return;
        }

      if (a->type == ANIMATION_TYPE_MOTION)
        {
          if (!parse_motion_animation_attrs (a,
                                             element_name,
                                             attr_names, attr_values,
                                             &handled,
                                             data,
                                             context))
            {
              animation_drop_and_free (a);
              skip_element (data, context, "Skipping <%s>: bad attributes", element_name);
              return;
            }
        }

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (!a->href || g_strcmp0 (a->href, data->current_shape->id) == 0)
        shape_add_animation (data->current_shape, a);
      else
        g_ptr_array_add (data->pending_animations, a);

      if (a->id)
        g_hash_table_insert (data->animations, a->id, a);

      data->current_animation = a;

      return;
    }

  if (strcmp (element_name, "mpath") == 0)
    {
      const char *xlink_href_attr = NULL;
      const char *href_attr = NULL;

      if (data->current_animation == NULL ||
          data->current_animation->type != ANIMATION_TYPE_MOTION ||
          data->current_animation->motion.path_ref != NULL)
        {
          skip_element (data, context, "<mpath> only allowed in <animateMotion>");
          return;
        }

      for (unsigned int i = 0; attr_names[i]; i++)
        {
          if (strcmp (attr_names[i], "xlink:href") == 0)
            {
              xlink_href_attr = attr_values[i];
              handled |= BIT (i);
            }
          else if (strcmp (attr_names[i], "href") == 0)
            {
              href_attr = attr_values[i];
              handled |= BIT (i);
            }
        }

      if (xlink_href_attr && !href_attr)
        href_attr = xlink_href_attr;

      if (href_attr[0] != '#')
        gtk_svg_invalid_attribute (data->svg, context, "href", "Missing '#' in href");
      else
        data->current_animation->motion.path_ref = g_strdup (href_attr + 1);

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (!data->current_animation->motion.path_ref)
        gtk_svg_missing_attribute (data->svg, context, "href", NULL);

      return;
    }

  /* If we get here, its all over */
  skip_element (data, context, "Unknown element: <%s>", element_name);
}

static void
end_element_cb (GMarkupParseContext *context,
                const gchar         *element_name,
                gpointer             user_data,
                GError             **gmarkup_error)
{
  ParserData *data = user_data;
  ShapeType shape_type;

  data->collect_text = FALSE;

  if (data->skip.to != NULL)
    {
      if (data->skip.to == g_markup_parse_context_get_element_stack (context))
        {
          GtkSvgLocation end;
          const char *parent;

          if (data->skip.to->next)
            parent = data->skip.to->next->data;
          else
            parent = NULL;

          gtk_svg_location_init (&end, context);

          gtk_svg_invalid_element (data->svg,
                                   parent,
                                   &data->skip.start,
                                   &end,
                                   "%s", data->skip.reason);
          g_clear_pointer (&data->skip.reason, g_free);
          data->skip.to = NULL;
          if (!data->skip.skip_over_target)
            goto do_target;
        }
      return;
    }

do_target:
  if (strcmp (element_name, "rdf:li") == 0)
    {
      g_set_str (&data->svg->keywords, data->text->str);
    }
  else if (strcmp (element_name, "dc:description") == 0)
    {
      g_set_str (&data->svg->description, data->text->str);
    }
  else if (strcmp (element_name, "dc:title") == 0)
    {
      g_set_str (&data->svg->author, data->text->str);
    }
  else if (shape_type_lookup (element_name, &shape_type))
    {
      GSList *tos = data->shape_stack;
      Shape *shape = data->current_shape;

      g_assert (shape_type == data->current_shape->type);

      if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_STROKE))
        {
          GtkSymbolicColor symbolic;

          if (((SvgPaint *) data->current_shape->base[SHAPE_ATTR_STROKE])->kind != PAINT_NONE)
            data->svg->used |= GTK_SVG_USES_STROKES;

          if (svg_paint_is_symbolic (((SvgPaint *) data->current_shape->base[SHAPE_ATTR_STROKE]), &symbolic))
            data->svg->used |= BIT (symbolic + 1);
        }

      if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_FILL))
        {
          GtkSymbolicColor symbolic;

          if (svg_paint_is_symbolic (((SvgPaint *) data->current_shape->base[SHAPE_ATTR_FILL]), &symbolic))
            data->svg->used |= BIT (symbolic + 1);
        }

      data->current_shape = tos->data;
      data->shape_stack = tos->next;
      g_slist_free_1 (tos);
    }
  else if (strcmp (element_name, "set") == 0 ||
           strcmp (element_name, "animate") == 0 ||
           strcmp (element_name, "animateTransform") == 0 ||
           strcmp (element_name, "animateMotion") == 0)
    {
      data->current_animation = NULL;
    }
}

static void
text_cb (GMarkupParseContext  *context,
         const char           *text,
         size_t                len,
         gpointer              user_data,
         GError              **error)
{
  ParserData *data = user_data;

  if (data->current_shape && shape_type_has_text (data->current_shape->type))
    {
      TextNode node = {
        .type = TEXT_NODE_CHARACTERS,
        .characters = { .text = g_strndup (text, len) }
      };
      g_array_append_val (data->current_shape->text, node);
      return;
    }

  if (!data->collect_text)
    return;

  g_string_append_len (data->text, text, len);
}

static void
error_cb (GMarkupParseContext *context,
          GError              *error,
          gpointer             user_data)
{
  ParserData *data = user_data;

  gtk_svg_markup_error (data->svg, context, error);
}

/* {{{ Href handling, dependency tracking */

static gboolean
shape_common_ancestor (Shape  *shape0,
                       Shape  *shape1,
                       Shape **ancestor0,
                       Shape **ancestor1)
{
  Shape *parent0, *parent1;
  unsigned int depth0, depth1;

  depth0 = 0;
  parent0 = shape0;
  while (parent0->parent)
    {
      parent0 = parent0->parent;
      depth0++;
    }

  depth1 = 0;
  parent1 = shape1;
  while (parent1->parent)
    {
      parent1 = parent1->parent;
      depth1++;
    }

  if (parent0 != parent1)
    return FALSE;

  while (depth0 > depth1)
    {
      shape0 = shape0->parent;
      depth0--;
    }

  while (depth1 > depth0)
    {
      shape1 = shape1->parent;
      depth1--;
    }

  while (shape0->parent != shape1->parent)
    {
      shape0 = shape0->parent;
      shape1 = shape1->parent;
    }

  *ancestor0 = shape0;
  *ancestor1 = shape1;
  return TRUE;
}

static void
add_dependency (Shape *shape0,
                Shape *shape1)
{
  if (!shape0->deps)
    shape0->deps = g_ptr_array_new ();
  g_ptr_array_add (shape0->deps, shape1);
}

/* Record the fact that when computing updated
 * values, shape2 must be handled before shape1
 */
static void
add_dependency_to_common_ancestor (Shape *shape0,
                                   Shape *shape1)
{
  Shape *anc0, *anc1;

  if (shape_common_ancestor (shape0, shape1, &anc0, &anc1))
    {
      g_assert (anc0->parent == anc1->parent);
      add_dependency (anc0, anc1);
    }
}

static void
resolve_clip_ref (SvgValue   *value,
                  Shape      *shape,
                  ParserData *data)
{
  SvgClip *clip = (SvgClip *) value;

  if (clip->kind == CLIP_REF && clip->ref.shape == NULL)
    {
      Shape *target = g_hash_table_lookup (data->shapes, clip->ref.ref);
      if (!target)
        gtk_svg_invalid_reference (data->svg,
                                   "No path with ID %s (resolving clip-path)",
                                   clip->ref.ref);
      else if (target->type != SHAPE_CLIP_PATH)
        gtk_svg_invalid_reference (data->svg,
                                   "Shape with ID %s not a <clipPath> (resolving clip-path)",
                                   clip->ref.ref);
      else
        {
          clip->ref.shape = target;
          add_dependency_to_common_ancestor (shape, target);
        }
    }
}

static void
resolve_mask_ref (SvgValue   *value,
                  Shape      *shape,
                  ParserData *data)
{
  SvgMask *mask = (SvgMask *) value;

  if (mask->kind == MASK_REF && mask->shape == NULL)
    {
      Shape *target = g_hash_table_lookup (data->shapes, mask->ref);
      if (!target)
        gtk_svg_invalid_reference (data->svg,
                                   "No shape with ID %s (resolving mask)",
                                   mask->ref);
      else if (target->type != SHAPE_MASK)
        gtk_svg_invalid_reference (data->svg,
                                   "Shape with ID %s not a <mask> (resolving mask)",
                                   mask->ref);
      else
        {
          mask->shape = target;
          add_dependency_to_common_ancestor (shape, target);
        }
    }
}

static void
resolve_href_ref (SvgValue   *value,
                  Shape      *shape,
                  ParserData *data)
{
  SvgHref *href = (SvgHref *) value;

  if (href->kind == HREF_NONE)
    return;

  if (shape->type == SHAPE_IMAGE || shape->type == SHAPE_FILTER)
    {
      GError *error = NULL;

      href->texture = get_texture (data->svg, href->ref, &error);

      if (href->texture != NULL)
        return;

      if (shape->type == SHAPE_IMAGE)
        {
          gtk_svg_invalid_reference (data->svg, "Failed to load %s (resolving <image>): %s", href->ref, error->message);
          g_error_free (error);
          return; /* Image href is always external */
        }
      g_error_free (error);
    }

  if (href->shape == NULL)
    {
      Shape *target = g_hash_table_lookup (data->shapes, svg_href_get_id (href));
      if (!target)
        {
          gtk_svg_invalid_reference (data->svg,
                                     "No shape with ID %s (resolving href in <%s>)",
                                     svg_href_get_id (href),
                                     shape_types[shape->type].name);
        }
      else if (shape->type == SHAPE_USE &&
               shape_has_ancestor (shape, SHAPE_CLIP_PATH) &&
               (BIT (target->type) & CLIP_PATH_CONTENT) == 0)
        {
          gtk_svg_invalid_reference (data->svg,
                                     "Can't include a <%s> in a <clipPath> via <use>",
                                     shape_types[target->type].name);
        }
      else
        {
          href->shape = target;
          add_dependency_to_common_ancestor (shape, target);
        }
    }
}

static void
resolve_marker_ref (SvgValue   *value,
                    Shape      *shape,
                    ParserData *data)
{
  SvgHref *href = (SvgHref *) value;

  if (href->kind != HREF_NONE && href->shape == NULL)
    {
      Shape *target = g_hash_table_lookup (data->shapes, svg_href_get_id (href));
      if (!target)
        {
          gtk_svg_invalid_reference (data->svg, "No shape with ID %s", href->ref);
        }
      else if (target->type != SHAPE_MARKER)
        {
          gtk_svg_invalid_reference (data->svg,
                                     "Shape with ID %s not a <marker>",
                                     href->ref);
        }
      else
        {
          href->shape = target;
          add_dependency_to_common_ancestor (shape, target);
        }
    }
}

static void
resolve_paint_ref (SvgValue   *value,
                   Shape      *shape,
                   ParserData *data)
{
  SvgPaint *paint = (SvgPaint *) value;

  if (paint_is_server (paint->kind) && paint->server.shape == NULL)
    {
      Shape *target = g_hash_table_lookup (data->shapes, paint->server.ref);

      if (!target)
        {
          GtkSymbolicColor symbolic;

          if ((data->svg->features & GTK_SVG_EXTENSIONS) != 0 &&
              g_str_has_prefix (paint->server.ref, "gpa:") &&
              parse_symbolic_color (paint->server.ref + strlen ("gpa:"), &symbolic))
            return; /* Handled later */

          gtk_svg_invalid_reference (data->svg, "No shape with ID %s (resolving fill or stroke)", paint->server.ref);
        }
      else if (target->type != SHAPE_LINEAR_GRADIENT &&
               target->type != SHAPE_RADIAL_GRADIENT &&
               target->type != SHAPE_PATTERN)
        {
          gtk_svg_invalid_reference (data->svg, "Shape with ID %s not a paint server (resolving fill or stroke)", paint->server.ref);
        }
      else
        {
          paint->server.shape = target;
          add_dependency_to_common_ancestor (shape, target);
        }
    }
}

static void
resolve_attach_ref (Shape      *shape,
                    ParserData *data)
{
  if (shape->gpa.attach.ref && shape->gpa.attach.shape == NULL)
    shape->gpa.attach.shape = g_hash_table_lookup (data->shapes, shape->gpa.attach.ref);
}

static void
resolve_filter_ref (SvgValue   *value,
                    Shape      *shape,
                    ParserData *data)
{
  SvgFilter *filter = (SvgFilter *) value;

  for (unsigned int i = 0; i < filter->n_functions; i++)
    {
      FilterFunction *f = &filter->functions[i];
      if (f->kind == FILTER_REF && f->ref.shape == NULL)
        {
          Shape *target = g_hash_table_lookup (data->shapes, f->ref.ref);
          if (!target)
            {
              gtk_svg_invalid_reference (data->svg,
                                         "No shape with ID %s (resolving filter)",
                                         f->ref.ref);
            }
          else if (target->type != SHAPE_FILTER)
            {
              gtk_svg_invalid_reference (data->svg,
                                         "Shape with ID %s not a filter (resolving filter)",
                                         f->ref.ref);
            }
          else
            {
              f->ref.shape = target;
              add_dependency_to_common_ancestor (shape, target);
            }
        }
    }
}

static void
resolve_refs_for_animation (Animation  *a,
                            ParserData *data)
{
  unsigned int first;

  for (int k = 0; k < 2; k++)
    {
      GPtrArray *specs = k == 0 ? a->begin : a->end;

      for (unsigned int j = 0; j < specs->len; j++)
        {
          TimeSpec *spec = g_ptr_array_index (specs, j);
          if (spec->type == TIME_SPEC_TYPE_SYNC && spec->sync.base == NULL)
            {
              g_assert (spec->sync.ref);
              spec->sync.base = g_hash_table_lookup (data->animations, spec->sync.ref);
              if (!spec->sync.base)
                gtk_svg_invalid_reference (data->svg, "No animation with ID %s", spec->sync.ref);
              else
                animation_add_dep (spec->sync.base, a);
            }
        }
    }

  /* The resolve functions don't know how to handle
   * our special current keyword, and it gets resolved
   * later anyway, so skip it.
   */
  if (a->frames[0].value && svg_value_is_current (a->frames[0].value))
    first = 1;
  else
    first = 0;

  if (a->attr == SHAPE_ATTR_CLIP_PATH)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_clip_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SHAPE_ATTR_MASK)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_mask_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SHAPE_ATTR_HREF)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_href_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SHAPE_ATTR_MARKER_START ||
           a->attr == SHAPE_ATTR_MARKER_MID ||
           a->attr == SHAPE_ATTR_MARKER_END)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_marker_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SHAPE_ATTR_FILL ||
           a->attr == SHAPE_ATTR_STROKE)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_paint_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SHAPE_ATTR_FILTER)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_filter_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SHAPE_ATTR_FE_IMAGE_HREF)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_href_ref (a->frames[j].value, a->shape, data);
    }

  if (a->motion.path_ref)
    {
      a->motion.path_shape = g_hash_table_lookup (data->shapes, a->motion.path_ref);
      if (a->motion.path_shape == NULL)
        gtk_svg_invalid_reference (data->svg,
                                   "No path with ID %s (resolving <mpath>",
                                   a->motion.path_ref);
      else
        {
          add_dependency_to_common_ancestor (a->shape, a->motion.path_shape);
          if (a->id && g_str_has_prefix (a->id, "gpa:attachment:"))
            {
              /* a's path is attached to a->motion.path_shape
               * Make sure it moves along with transitions and animations
               */
              create_attachment_connection (a, a->motion.path_shape, data->svg->timeline);
            }
        }
    }
}

static void
resolve_animation_refs (Shape      *shape,
                        ParserData *data)
{
  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          Animation *a = g_ptr_array_index (shape->animations, i);
          resolve_refs_for_animation (a, data);
        }
    }

  if (shape_type_has_shapes (shape->type))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          Shape *sh = g_ptr_array_index (shape->shapes, i);
          resolve_animation_refs (sh, data);
        }
    }
}

static void
resolve_filter_image_refs (Shape      *shape,
                           ParserData *data)
{
  if (shape->type != SHAPE_FILTER)
    return;

  for (unsigned int i = 0; i < shape->filters->len; i++)
    {
      FilterPrimitive *f = g_ptr_array_index (shape->filters, i);

      if (f->type == FE_IMAGE)
        resolve_href_ref (f->base[filter_attr_idx (f->type, SHAPE_ATTR_FE_IMAGE_HREF)], shape, data);
    }
}

static void
resolve_shape_refs (Shape      *shape,
                    ParserData *data)
{
  resolve_clip_ref (shape->base[SHAPE_ATTR_CLIP_PATH], shape, data);
  resolve_mask_ref (shape->base[SHAPE_ATTR_MASK], shape, data);
  resolve_href_ref (shape->base[SHAPE_ATTR_HREF], shape, data);
  resolve_marker_ref (shape->base[SHAPE_ATTR_MARKER_START], shape, data);
  resolve_marker_ref (shape->base[SHAPE_ATTR_MARKER_MID], shape, data);
  resolve_marker_ref (shape->base[SHAPE_ATTR_MARKER_END], shape, data);
  resolve_paint_ref (shape->base[SHAPE_ATTR_FILL], shape, data);
  resolve_paint_ref (shape->base[SHAPE_ATTR_STROKE], shape, data);
  resolve_filter_ref (shape->base[SHAPE_ATTR_FILTER], shape, data);
  resolve_attach_ref (shape, data);
  resolve_filter_image_refs (shape, data);
}

static gboolean
can_add (Shape      *shape,
         GHashTable *waiting)
{
  if (shape->deps)
    {
      for (unsigned int i = 0; i < shape->deps->len; i++)
        {
          Shape *dep = g_ptr_array_index (shape->deps, i);
          if (g_hash_table_contains (waiting, dep))
            return FALSE;
        }
    }

  return TRUE;
}

static void
do_compute_update_order (Shape      *shape,
                         GtkSvg     *svg,
                         GHashTable *waiting)
{
  unsigned int n_waiting;
  gboolean has_cycle = FALSE;
  Shape *last = NULL;

  if (!shape_type_has_shapes (shape->type))
    return;

  g_assert (g_hash_table_size (waiting) == 0);

  for (unsigned int i = 0; i < shape->shapes->len; i++)
    {
      Shape *sh = g_ptr_array_index (shape->shapes, i);
      do_compute_update_order (sh, svg, waiting);
    }

  for (unsigned int i = 0; i < shape->shapes->len; i++)
    {
      Shape *sh = g_ptr_array_index (shape->shapes, i);
      g_hash_table_add (waiting, sh);
    }

  n_waiting = g_hash_table_size (waiting);
  while (n_waiting > 0)
    {
      GHashTableIter iter;
      Shape *key;

      g_hash_table_iter_init (&iter, waiting);
      while (g_hash_table_iter_next (&iter, (gpointer *) &key, NULL))
        {
          if (can_add (key, waiting) || has_cycle)
            {
              if (last)
                last->next = key;
              else
                shape->first = key;
              last = key;
              last->next = NULL;
              g_hash_table_iter_remove (&iter);
            }
        }

      if (n_waiting == g_hash_table_size (waiting))
        {
          gtk_svg_update_error (svg, "Cyclic dependency detected");
          has_cycle = TRUE;
        }

      n_waiting = g_hash_table_size (waiting);
    }

  for (unsigned int i = 0; i < shape->shapes->len; i++)
    {
      Shape *sh = g_ptr_array_index (shape->shapes, i);
      g_clear_pointer (&sh->deps, g_ptr_array_unref);
    }
}

static void
compute_update_order (Shape  *shape,
                      GtkSvg *svg)
{
  GHashTable *waiting = g_hash_table_new (g_direct_hash, g_direct_equal);
  do_compute_update_order (shape, svg, waiting);
  g_hash_table_unref (waiting);
}

/* }}} */

static void
gtk_svg_init_from_bytes (GtkSvg *self,
                         GBytes *bytes)
{
  ParserData data;
  GMarkupParseContext *context;
  GMarkupParser parser = {
    start_element_cb,
    end_element_cb,
    text_cb,
    NULL,
    error_cb,
  };

  g_clear_pointer (&self->content, shape_free);

  if ((self->features & GTK_SVG_SYSTEM_RESOURCES) == 0)
    {
      g_assert (self->fontmap == NULL);
      ensure_fontmap (self);
    }

  data.svg = self;
  data.current_shape = NULL;
  data.shape_stack = NULL;
  data.shapes = g_hash_table_new (g_str_hash, g_str_equal);
  data.animations = g_hash_table_new (g_str_hash, g_str_equal);
  data.current_animation = NULL;
  data.pending_animations = g_ptr_array_new_with_free_func (animation_free);
  data.pending_refs = g_ptr_array_new ();
  data.skip.to = NULL;
  data.skip.reason = NULL;
  data.text = g_string_new ("");
  data.collect_text = FALSE;
  data.num_loaded_elements = 0;

  context = g_markup_parse_context_new (&parser, G_MARKUP_PREFIX_ERROR_POSITION, &data, NULL);
  if (!g_markup_parse_context_parse (context,
                                     g_bytes_get_data (bytes, NULL),
                                     g_bytes_get_size (bytes),
                                     NULL) ||
      !g_markup_parse_context_end_parse (context, NULL))
    {
      gtk_svg_clear_content (self);
      g_slist_free (data.shape_stack);
      g_clear_pointer (&data.skip.reason, g_free);

      g_ptr_array_set_size (data.pending_animations, 0);
      g_ptr_array_set_size (data.pending_refs, 0);
    }
  else
    {
      g_assert (data.current_shape == NULL);
      g_assert (data.shape_stack == NULL);
      g_assert (data.current_animation == NULL);
      g_assert (data.skip.to == NULL);
      g_assert (data.skip.reason == NULL);
    }

  g_markup_parse_context_free (context);

  if (self->content == NULL)
    self->content = shape_new (NULL, SHAPE_SVG);

  if (_gtk_bitmask_get (self->content->attrs, SHAPE_ATTR_VIEW_BOX))
    {
      SvgViewBox *vb = (SvgViewBox *) self->content->base[SHAPE_ATTR_VIEW_BOX];
      self->width = vb->view_box.size.width;
      self->height = vb->view_box.size.height;
    }

  if (_gtk_bitmask_get (self->content->attrs, SHAPE_ATTR_WIDTH))
    {
      SvgNumber *n = (SvgNumber *) self->content->base[SHAPE_ATTR_WIDTH];

      if (is_absolute_length (n->unit))
        self->width = absolute_length_to_px (n->value, n->unit);
      else if (n->unit != SVG_UNIT_PERCENTAGE)
        self->width = n->value;
    }

  if (_gtk_bitmask_get (self->content->attrs, SHAPE_ATTR_HEIGHT))
    {
      SvgNumber *n = (SvgNumber *) self->content->base[SHAPE_ATTR_HEIGHT];

      if (is_absolute_length (n->unit))
        self->height = absolute_length_to_px (n->value, n->unit);
      else if (n->unit != SVG_UNIT_PERCENTAGE)
        self->height = n->value;
    }

  if (!_gtk_bitmask_get (self->content->attrs, SHAPE_ATTR_VIEW_BOX) &&
      !_gtk_bitmask_get (self->content->attrs, SHAPE_ATTR_WIDTH) &&
      !_gtk_bitmask_get (self->content->attrs, SHAPE_ATTR_HEIGHT))
    {
      /* arbitrary */
      self->width = 200;
      self->height = 200;
    }

  for (unsigned int i = 0; i < data.pending_animations->len; i++)
    {
      Animation *a = g_ptr_array_index (data.pending_animations, i);
      Shape *shape;

      g_assert (a->href != NULL);
      g_assert (a->shape == NULL);

      shape = g_hash_table_lookup (data.shapes, a->href);
      if (!shape)
        {
          gtk_svg_invalid_reference (self,
                                     "No shape with ID %s (resolving begin or end attribute)",
                                     a->href);
          animation_free (a);
        }
      else
        {
          shape_add_animation (shape, a);
        }
    }

  /* Faster than stealing the items out of the array one-by-one */
  g_ptr_array_set_free_func (data.pending_animations, NULL);
  g_ptr_array_set_size (data.pending_animations, 0);

  for (unsigned int i = 0; i < data.pending_refs->len; i++)
    {
      Shape *sh = g_ptr_array_index (data.pending_refs, i);
      resolve_shape_refs (sh, &data);
    }

  resolve_animation_refs (self->content, &data);

  compute_update_order (self->content, self);

  self->state_change_delay = timeline_get_state_change_delay (self->timeline);

  g_hash_table_unref (data.shapes);
  g_hash_table_unref (data.animations);
  g_ptr_array_unref (data.pending_animations);
  g_ptr_array_unref (data.pending_refs);
  g_string_free (data.text, TRUE);

  if (self->gpa_version == 1 &&
      (self->features & GTK_SVG_ANIMATIONS) == 0)
    apply_state (self->content, self->state);
}

static void
gtk_svg_init_from_resource (GtkSvg     *self,
                            const char *path)
{
  GBytes *bytes;

  bytes = g_resources_lookup_data (path, 0, NULL);
  if (bytes)
    {
      gtk_svg_init_from_bytes (self, bytes);
      g_bytes_unref (bytes);
    }
}

/* }}} */
/* {{{ Serialization */

/* We don't aim for 100% accurate reproduction here, so
 * we allow values to be normalized, and we don't necessarily
 * preserve explicitly set default values. Animations are
 * always emitted as children of the shape they belong to,
 * regardless of where they were placed in the original svg.
 *
 * In addition to the original DOM values, we allow serializing
 * 'snapshots' of a running animation at a given time, which
 * is very useful for tests. When doing so, we can also write
 * out some internal state in custom attributes, which is,
 * again, useful for tests.
 */

static void
indent_for_elt (GString *s,
                int      indent)
{
  g_string_append_printf (s, "\n%*s", indent, " ");
}

static void
indent_for_attr (GString *s,
                 int      indent)
{
  g_string_append_printf (s, "\n%*s", indent + 8, " ");
}

static void
serialize_shape_attrs (GString              *s,
                       GtkSvg               *svg,
                       int                   indent,
                       Shape                *shape,
                       GtkSvgSerializeFlags  flags)
{
  GString *classes = g_string_new ("");
  GString *style = g_string_new ("");

  if (shape->id)
    {
      indent_for_attr (s, indent);
      g_string_append_printf (s, "id='%s'", shape->id);
    }

  for (ShapeAttr attr = FIRST_SHAPE_ATTR; attr <= LAST_SHAPE_ATTR; attr++)
    {
      if ((flags & GTK_SVG_SERIALIZE_NO_COMPAT) == 0 &&
          svg->gpa_version > 0 &&
          shape_type_has_gpa_attrs (shape->type) &&
          attr == SHAPE_ATTR_VISIBILITY)
        {
          if ((shape->gpa.states & BIT (svg->state)) == 0)
            {
              indent_for_attr (s, indent);
              g_string_append_printf (s, "visibility='hidden'");
              continue;
            }
        }

      if (_gtk_bitmask_get (shape->attrs, attr) ||
          (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME))
        {
          SvgValue *value, *initial;

          if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
            value = svg_value_ref (shape_get_current_value (shape, attr, 0));
          else
            value = shape_ref_base_value (shape, NULL, attr, 0);

          initial = shape_attr_ref_initial_value (attr, shape->type, shape->parent != NULL);

          if (_gtk_bitmask_get (shape->attrs, attr) || !svg_value_equal (value, initial))
            {
              if (!shape_attr_only_css (attr))
                {
                  indent_for_attr (s, indent);
                  g_string_append_printf (s, "%s='", shape_attr_get_presentation (attr, shape->type));
                  svg_value_print (value, s);
                  g_string_append_c (s, '\'');
                }
              else
                {
                  if (style->len > 0)
                    g_string_append (style, "; ");
                  g_string_append_printf (style, "%s: ", shape_attr_get_presentation (attr, shape->type));
                  svg_value_print (value, style);
                }
            }

          svg_value_unref (initial);

          if (attr == SHAPE_ATTR_FILL)
            {
              SvgPaint *paint = (SvgPaint *) value;
              GtkSymbolicColor symbolic;

              if (paint->kind == PAINT_NONE)
                {
                  g_string_append_printf (classes, "%stransparent-fill",
                                          classes->len > 0 ? " " : "");
                }
              else if (paint->kind == PAINT_SYMBOLIC)
                {
                  /* Accent color doesn't work with matrix recoloring */
                  if (paint->symbolic == GTK_SYMBOLIC_COLOR_ACCENT)
                    symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;
                  else
                    symbolic = paint->symbolic;

                  g_string_append_printf (classes, "%s%s %s-fill",
                                          classes->len > 0 ? " " : "",
                                          symbolic_colors[symbolic],
                                          symbolic_colors[symbolic]);
                }
              else if (paint_is_server (paint->kind) &&
                       g_str_has_prefix (paint->server.ref, "gpa:") &&
                       parse_symbolic_color (paint->server.ref + strlen ("gpa:"), &symbolic))
               {
                  if (symbolic == GTK_SYMBOLIC_COLOR_ACCENT)
                    symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;

                  g_string_append_printf (classes, "%s%s %s-fill",
                                          classes->len > 0 ? " " : "",
                                          symbolic_colors[symbolic],
                                          symbolic_colors[symbolic]);
               }
            }

          if (attr == SHAPE_ATTR_STROKE)
            {
              SvgPaint *paint = (SvgPaint *) value;
              GtkSymbolicColor symbolic;

              if (paint->kind == PAINT_SYMBOLIC)
                {
                  if (paint->symbolic == GTK_SYMBOLIC_COLOR_ACCENT)
                    symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;
                  else
                    symbolic = paint->symbolic;

                  g_string_append_printf (classes, "%s%s-stroke",
                                          classes->len > 0 ? " " : "",
                                          symbolic_colors[symbolic]);
                }
              else if (paint_is_server (paint->kind) &&
                       g_str_has_prefix (paint->server.ref, "gpa:") &&
                       parse_symbolic_color (paint->server.ref + strlen ("gpa:"), &symbolic))
                {
                  if (symbolic == GTK_SYMBOLIC_COLOR_ACCENT)
                    symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;

                  g_string_append_printf (classes, "%s%s-stroke",
                                          classes->len > 0 ? " " : "",
                                          symbolic_colors[symbolic]);
                }
            }

          svg_value_unref (value);
        }
    }

  if ((flags & GTK_SVG_SERIALIZE_NO_COMPAT) == 0 &&
      classes->len > 0)
    {
      indent_for_attr (s, indent);
      g_string_append_printf (s, "class='%s'", classes->str);
    }

  if (style->len > 0)
    {
      indent_for_attr (s, indent);
      g_string_append_printf (s, "style='%s'", style->str);
    }

  g_string_free (classes, TRUE);
  g_string_free (style, TRUE);
}

static void
states_to_string (GString  *s,
                  uint64_t  states)
{
  if (states == ALL_STATES)
    {
      g_string_append (s, "all");
    }
  else if (states == NO_STATES)
    {
      g_string_append (s, "none");
    }
  else
    {
      gboolean first = TRUE;

      for (unsigned int u = 0; u < 64; u++)
        {
          if ((states & (G_GUINT64_CONSTANT (1) << u)) != 0)
            {
              if (!first)
                g_string_append_c (s, ' ');
              g_string_append_printf (s, "%u", u);
              first = FALSE;
            }
        }
    }
}

static void
serialize_gpa_attrs (GString              *s,
                     GtkSvg               *svg,
                     int                   indent,
                     Shape                *shape,
                     GtkSvgSerializeFlags  flags)
{
  SvgValue **values;
  SvgPaint *paint;
  GtkSymbolicColor symbolic;

  if (svg->gpa_version == 0 || !shape_type_has_gpa_attrs (shape->type))
    return;

  if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
    values = shape->current;
  else
    values = shape->base;

  paint = (SvgPaint *) values[SHAPE_ATTR_STROKE];
  if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_STROKE) &&
      svg_paint_is_symbolic (paint, &symbolic))
    {
      indent_for_attr (s, indent);
      g_string_append (s, "gpa:stroke='");
      g_string_append (s, symbolic_colors[symbolic]);
      g_string_append_c (s, '\'');
    }

  paint = (SvgPaint *) values[SHAPE_ATTR_FILL];
  if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_FILL) &&
      svg_paint_is_symbolic (paint, &symbolic))
    {
      indent_for_attr (s, indent);
      g_string_append (s, "gpa:fill='");
      g_string_append (s, symbolic_colors[symbolic]);
      g_string_append_c (s, '\'');
    }

  if ((flags & GTK_SVG_SERIALIZE_EXPAND_GPA_ATTRS) == 0)
    {
      if (shape->gpa.states != ALL_STATES)
        {
          indent_for_attr (s, indent);
          g_string_append (s, "gpa:states='");
          states_to_string (s, shape->gpa.states);
          g_string_append_c (s, '\'');
        }

      if (shape->gpa.transition != GPA_TRANSITION_NONE)
        {
          const char *names[] = { "none", "animate", "morph", "fade" };
          indent_for_attr (s, indent);
          g_string_append_printf (s, "gpa:transition-type='%s'", names[shape->gpa.transition]);
        }

      if (shape->gpa.transition_easing != GPA_EASING_LINEAR)
        {
          const char *names[] = { "linear", "ease-in-out", "ease-in", "ease-out", "ease" };
          indent_for_attr (s, indent);
          g_string_append_printf (s, "gpa:transition-easing='%s'", names[shape->gpa.transition_easing]);
        }

      if (shape->gpa.transition_duration != 0)
        {
          char buffer[128];
          indent_for_attr (s, indent);
          g_string_append_printf (s, "gpa:transition-duration='%sms'",
                                  g_ascii_formatd (buffer, sizeof (buffer),
                                                   "%g", shape->gpa.transition_duration / (double) G_TIME_SPAN_MILLISECOND));
        }

      if (shape->gpa.transition_delay != 0)
        {
          char buffer[128];
          indent_for_attr (s, indent);
          g_string_append_printf (s, "gpa:transition-delay='%sms'",
                                  g_ascii_formatd (buffer, sizeof (buffer),
                                                   "%g", shape->gpa.transition_delay / (double) G_TIME_SPAN_MILLISECOND));
        }

      if (shape->gpa.animation != GPA_ANIMATION_NONE)
        {
          const char *names[] = { "none", "normal", "alternate", "reverse",
            "reverse-alternate", "in-out", "in-out-alternate", "in-out-reverse",
            "segment", "segment-alternate" };
          indent_for_attr (s, indent);
          g_string_append (s, "gpa:animation-type='automatic'");
          indent_for_attr (s, indent);
          g_string_append_printf (s, "gpa:animation-direction='%s'", names[shape->gpa.animation]);
          indent_for_attr (s, indent);
        }

      if (shape->gpa.animation_easing != GPA_EASING_LINEAR)
        {
          const char *names[] = { "linear", "ease-in-out", "ease-in", "ease-out", "ease" };
          indent_for_attr (s, indent);
          g_string_append_printf (s, "gpa:animation-easing='%s'", names[shape->gpa.animation_easing]);
        }

      if (shape->gpa.animation_duration != 0)
        {
          char buffer[128];
          indent_for_attr (s, indent);
          g_string_append_printf (s, "gpa:animation-duration='%sms'",
                                  g_ascii_formatd (buffer, sizeof (buffer),
                                                   "%g", shape->gpa.animation_duration / (double) G_TIME_SPAN_MILLISECOND));
        }

      if (shape->gpa.animation_repeat != REPEAT_FOREVER)
        {
          char buffer[128];
          indent_for_attr (s, indent);
          g_string_append_printf (s, "gpa:animation-repeat='%s'",
                                  g_ascii_formatd (buffer, sizeof (buffer),
                                                   "%g", shape->gpa.animation_repeat));
        }

      if (shape->gpa.animation_segment != 0.2)
        {
          char buffer[128];
          indent_for_attr (s, indent);
          g_string_append_printf (s, "gpa:animation-segment='%s'",
                                  g_ascii_formatd (buffer, sizeof (buffer),
                                                   "%g", shape->gpa.animation_segment));
        }

      if (shape->gpa.origin != 0)
        {
          char buffer[128];
          indent_for_attr (s, indent);
          g_string_append_printf (s, "gpa:origin='%s'",
                                  g_ascii_formatd (buffer, sizeof (buffer),
                                                   "%g", shape->gpa.origin));
        }

      if (shape->gpa.attach.ref)
        {
          indent_for_attr (s, indent);
          g_string_append_printf (s, "gpa:attach-to='%s'", shape->gpa.attach.ref);
        }

      if (shape->gpa.attach.pos != 0)
        {
          char buffer[128];
          indent_for_attr (s, indent);
          g_string_append_printf (s, "gpa:attach-pos='%s'",
                                  g_ascii_formatd (buffer, sizeof (buffer),
                                                   "%g", shape->gpa.attach.pos));
        }
    }
}

static void
serialize_base_animation_attrs (GString   *s,
                                GtkSvg    *svg,
                                int        indent,
                                Animation *a)
{
  if (a->id)
    {
      indent_for_attr (s, indent);
      g_string_append_printf (s, "id='%s'", a->id);
    }

  if (a->type != ANIMATION_TYPE_MOTION)
    {
      indent_for_attr (s, indent);
      g_string_append_printf (s, "attributeName='%s'",
                              shape_attr_get_presentation (a->attr, a->shape->type));
    }

  if (a->has_begin)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "begin='");
      time_specs_print (a->begin, s);
      g_string_append (s, "'");
    }

  if (a->has_end)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "end='");
      time_specs_print (a->end, s);
      g_string_append (s, "'");
    }

  if (a->has_simple_duration)
    {
      indent_for_attr (s, indent);
      if (a->simple_duration == INDEFINITE)
        {
          g_string_append (s, "dur='indefinite'");
        }
      else
        {
          string_append_double (s, "dur='", a->simple_duration / (double) G_TIME_SPAN_MILLISECOND);
          g_string_append (s, "ms'");
        }
    }

  if (a->has_repeat_count)
    {
      indent_for_attr (s, indent);
      if (a->repeat_count == REPEAT_FOREVER)
        {
          g_string_append (s, "repeatCount='indefinite'");
        }
      else
        {
          string_append_double (s, "repeatCount='", a->repeat_count);
          g_string_append (s, "'");
        }
    }

  if (a->has_repeat_duration)
    {
      indent_for_attr (s, indent);
      if (a->repeat_duration == INDEFINITE)
        {
          g_string_append (s, "repeatDur='indefinite'");
        }
      else
        {
          string_append_double (s, "repeatDur='", a->repeat_duration / (double) G_TIME_SPAN_MILLISECOND);
          g_string_append (s, "ms'");
        }
    }

  if (a->fill != ANIMATION_FILL_REMOVE)
    {
      const char *fill[] = { "freeze", "remove" };
      indent_for_attr (s, indent);
      g_string_append_printf (s, "fill='%s'", fill[a->fill]);
    }

  if (a->restart != ANIMATION_RESTART_ALWAYS)
    {
      const char *restart[] = { "always", "whenNotActive", "never" };
      indent_for_attr (s, indent);
      g_string_append_printf (s, "restart='%s'", restart[a->restart]);
    }
}

static void
serialize_value_animation_attrs (GString   *s,
                                 GtkSvg    *svg,
                                 int        indent,
                                 Animation *a)
{
  if (a->type != ANIMATION_TYPE_MOTION)
    {
      if (a->n_frames == 2)
        {
          if (a->type != ANIMATION_TYPE_SET &&
              !svg_value_is_current (a->frames[0].value))
            {
              indent_for_attr (s, indent);
              g_string_append (s, "from='");
              if (a->type == ANIMATION_TYPE_TRANSFORM &&
                  a->attr == SHAPE_ATTR_TRANSFORM)
                svg_primitive_transform_print (a->frames[0].value, s);
              else
                svg_value_print (a->frames[0].value, s);
              g_string_append (s, "'");
            }

          indent_for_attr (s, indent);
          g_string_append (s, "to='");
          if (a->type == ANIMATION_TYPE_TRANSFORM &&
              a->attr == SHAPE_ATTR_TRANSFORM)
            svg_primitive_transform_print (a->frames[1].value, s);
          else
            svg_value_print (a->frames[1].value, s);
          g_string_append_c (s, '\'');
        }
      else
        {
          indent_for_attr (s, indent);
          g_string_append (s, "values='");
          if (a->type == ANIMATION_TYPE_TRANSFORM &&
              a->attr == SHAPE_ATTR_TRANSFORM)
            {
              for (unsigned int i = 0; i < a->n_frames; i++)
                {
                  if (i > 0)
                    g_string_append (s, "; ");
                  svg_primitive_transform_print (a->frames[i].value, s);
                }
            }
          else
            {
              for (unsigned int i = 0; i < a->n_frames; i++)
                {
                  if (i > 0)
                    g_string_append (s, "; ");
                  svg_value_print (a->frames[i].value, s);
                }
            }
          g_string_append_c (s, '\'');
        }
    }

  if (a->calc_mode == CALC_MODE_SPLINE)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "keySplines='");
      for (unsigned int i = 0; i + 1 < a->n_frames; i++)
        {
          string_append_double (s, i > 0 ? "; " : "", a->frames[i].params[0]);
          string_append_double (s, " ", a->frames[i].params[1]);
          string_append_double (s, " ", a->frames[i].params[2]);
          string_append_double (s, " ", a->frames[i].params[3]);
        }
      g_string_append_c (s, '\'');
    }

  if (a->calc_mode != CALC_MODE_PACED)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "keyTimes='");
      for (unsigned int i = 0; i < a->n_frames; i++)
        string_append_double (s, i > 0 ? "; " : "", a->frames[i].time);
      g_string_append_c (s, '\'');
    }

  if (a->calc_mode != default_calc_mode (a->type))
    {
      const char *modes[] = { "discrete", "linear", "paced", "spline" };
      indent_for_attr (s, indent);
      g_string_append_printf (s, "calcMode='%s'", modes[a->calc_mode]);
    }

  if (a->additive != ANIMATION_ADDITIVE_REPLACE)
    {
      const char *additive[] = { "replace", "sum" };
      indent_for_attr (s, indent);
      g_string_append_printf (s, "additive='%s'", additive[a->additive]);
    }

  if (a->accumulate != ANIMATION_ACCUMULATE_NONE)
    {
      const char *accumulate[] = { "none", "sum" };
      indent_for_attr (s, indent);
      g_string_append_printf (s, "accumulate='%s'", accumulate[a->accumulate]);
    }

  if (a->color_interpolation != COLOR_INTERPOLATION_SRGB)
    {
      const char *vals[] = { "auto", "sRGB", "linearRGB" };
      indent_for_attr (s, indent);
      g_string_append_printf (s, "color-interpolation='%s'", vals[a->color_interpolation]);
    }
}

static void
serialize_animation_status (GString              *s,
                            GtkSvg               *svg,
                            int                   indent,
                            Animation            *a,
                            GtkSvgSerializeFlags  flags)
{
  if (flags & GTK_SVG_SERIALIZE_INCLUDE_STATE)
    {
      const char *status[] = { "inactive", "running", "done" };
      indent_for_attr (s, indent);
      g_string_append_printf (s, "gpa:status='%s'", status[a->status]);
      /* Not writing out start/end time, since that will be hard to compare */
 
      if (!a->has_simple_duration)
        {
          int64_t d = determine_simple_duration (a);
          indent_for_attr (s, indent);
          if (d == INDEFINITE)
            g_string_append (s, "gpa:computed-simple-duration='indefinite'");
          else
            {
              string_append_double (s,
                                    "gpa:computed-simple-duration='",
                                    d / (double) G_TIME_SPAN_MILLISECOND);
              g_string_append (s, "ms'");
            }
        }

      if (a->current.begin != INDEFINITE)
        {
          indent_for_attr (s, indent);
          string_append_double (s,
                                "gpa:current-start-time='",
                                (a->current.begin - svg->load_time) / (double) G_TIME_SPAN_MILLISECOND);
          g_string_append (s, "ms'");
        }

      if (a->current.end != INDEFINITE)
        {
          indent_for_attr (s, indent);
          string_append_double (s,
                                "gpa:current-end-time='",
                                (a->current.end - svg->load_time) / (double) G_TIME_SPAN_MILLISECOND);
          g_string_append (s, "ms'");
        }
    }
}

static void
serialize_animation_set (GString              *s,
                         GtkSvg               *svg,
                         int                   indent,
                         Animation            *a,
                         GtkSvgSerializeFlags  flags)
{
  indent_for_elt (s, indent);
  g_string_append (s, "<set");
  serialize_base_animation_attrs (s, svg, indent, a);
  indent_for_attr (s, indent);
  g_string_append (s, "to='");
  svg_value_print (a->frames[0].value, s);
  g_string_append_c (s, '\'');
  serialize_animation_status (s, svg, indent, a, flags);
  g_string_append (s, "/>");
}

static void
serialize_animation_animate (GString              *s,
                             GtkSvg               *svg,
                             int                   indent,
                             Animation            *a,
                             GtkSvgSerializeFlags  flags)
{
  indent_for_elt (s, indent);
  g_string_append (s, "<animate");
  serialize_base_animation_attrs (s, svg, indent, a);
  serialize_value_animation_attrs (s, svg, indent, a);
  serialize_animation_status (s, svg, indent, a, flags);
  g_string_append (s, "/>");
}

static void
serialize_animation_transform (GString              *s,
                               GtkSvg               *svg,
                               int                   indent,
                               Animation            *a,
                               GtkSvgSerializeFlags  flags)
{
  const char *types[] = { "none", "translate", "scale", "rotate", "any" };
  SvgTransform *tf = (SvgTransform *) a->frames[0].value;

  indent_for_elt (s, indent);
  g_string_append (s, "<animateTransform");
  serialize_base_animation_attrs (s, svg, indent, a);
  serialize_value_animation_attrs (s, svg, indent, a);
  indent_for_attr (s, indent);
  g_string_append_printf (s, "type='%s'", types[tf->transforms[0].type]);
  serialize_animation_status (s, svg, indent, a, flags);
  g_string_append (s, "/>");
}

static void
serialize_animation_motion (GString              *s,
                            GtkSvg               *svg,
                            int                   indent,
                            Animation            *a,
                            GtkSvgSerializeFlags  flags)
{
  indent_for_elt (s, indent);
  g_string_append (s, "<animateMotion");
  serialize_base_animation_attrs (s, svg, indent, a);
  serialize_value_animation_attrs (s, svg, indent, a);

  if (a->calc_mode != CALC_MODE_PACED)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "keyPoints='");
      for (unsigned int i = 0; i < a->n_frames; i++)
        string_append_double (s, i > 0 ? "; " : "", a->frames[i].point);
      g_string_append (s, "'");
    }

  if (a->motion.rotate != ROTATE_FIXED)
    {
      const char *values[] = { "auto", "auto-reverse" };
      indent_for_attr (s, indent);
      g_string_append_printf (s, "rotate='%s'", values[a->motion.rotate]);
    }
  else if (a->motion.angle != 0)
    {
      indent_for_attr (s, indent);
      string_append_double (s, "rotate='", a->motion.angle);
      g_string_append (s, "'");
    }

  serialize_animation_status (s, svg, indent, a, flags);

  if (a->motion.path_shape)
    {
      g_string_append (s, ">");
      indent_for_elt (s, indent + 2);
      g_string_append_printf (s, "<mpath href='%s'/>", a->motion.path_shape->id);
      indent_for_elt (s, indent);
      g_string_append (s, "</animateMotion>");
    }
  else
    {
      GskPath *path;

      if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
        path = animation_motion_get_current_path (a, NULL);
      else
        path = animation_motion_get_base_path (a, NULL);
      if (path)
        {
          indent_for_attr (s, indent);
          g_string_append (s, "path='");
          gsk_path_print (path, s);
          gsk_path_unref (path);
          g_string_append_c (s, '\'');
        }
      g_string_append (s, "/>");
    }
}

static void
serialize_animation (GString              *s,
                     GtkSvg               *svg,
                     int                   indent,
                     Animation            *a,
                     GtkSvgSerializeFlags  flags)
{
  if (flags & GTK_SVG_SERIALIZE_EXCLUDE_ANIMATION)
    return;

  if ((flags & GTK_SVG_SERIALIZE_EXPAND_GPA_ATTRS) == 0)
    {
      if (a->id && g_str_has_prefix (a->id, "gpa:"))
        return;
    }

  switch (a->type)
    {
    case ANIMATION_TYPE_SET:
      serialize_animation_set (s, svg, indent, a, flags);
      break;
    case ANIMATION_TYPE_ANIMATE:
      serialize_animation_animate (s, svg, indent, a, flags);
      break;
    case ANIMATION_TYPE_TRANSFORM:
      serialize_animation_transform (s, svg, indent, a, flags);
      break;
    case ANIMATION_TYPE_MOTION:
      serialize_animation_motion (s, svg, indent, a, flags);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
serialize_color_stop (GString              *s,
                      GtkSvg               *svg,
                      int                   indent,
                      Shape                *shape,
                      unsigned int          idx,
                      GtkSvgSerializeFlags  flags)
{
  ColorStop *stop;
  SvgValue **values;
  const char *names[] = { "offset", "stop-color", "stop-opacity" };

  stop = g_ptr_array_index (shape->color_stops, idx);

  indent_for_elt (s, indent);
  g_string_append (s, "<stop");

  if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
    values = stop->current;
  else
    values = stop->base;

  for (unsigned int i = 0; i < N_STOP_ATTRS; i++)
    {
      indent_for_attr (s, indent);
      g_string_append_printf (s, "%s='", names[i]);
      svg_value_print (values[i], s);
      g_string_append (s, "'");
    }
  g_string_append (s, ">");

  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          Animation *a = g_ptr_array_index (shape->animations, i);
          if (a->idx == idx + 1)
            {
              serialize_animation (s, svg, indent + 2, a, flags);
            }
        }
    }

  indent_for_elt (s, indent);
  g_string_append (s, "</stop>");
}

static void
serialize_filter_begin (GString              *s,
                        GtkSvg               *svg,
                        int                   indent,
                        Shape                *shape,
                        FilterPrimitive      *f,
                        unsigned int          idx,
                        GtkSvgSerializeFlags  flags)
{
  SvgValue **values;

  indent_for_elt (s, indent);
  g_string_append_printf (s, "<%s", filter_types[f->type].name);

  if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
    values = f->current;
  else
    values = f->base;

  for (unsigned int i = 0; i < filter_primitive_get_n_attrs (f->type); i++)
    {
      ShapeAttr attr = filter_primitive_get_shape_attr (f->type, i);
      SvgValue *initial;

      initial = filter_attr_ref_initial_value (f, attr);
      if (!svg_value_equal (values[i], initial))
        {
          indent_for_attr (s, indent);
          g_string_append_printf (s, "%s='", filter_attr_get_presentation (attr, f->type));
          svg_value_print (values[i], s);
          g_string_append (s, "'");
        }
      svg_value_unref (initial);
    }

  g_string_append (s, ">");

  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          Animation *a = g_ptr_array_index (shape->animations, i);
          if (a->idx == idx + 1)
            serialize_animation (s, svg, indent + 2, a, flags);
        }
    }
}

static void
serialize_filter_end (GString              *s,
                      GtkSvg               *svg,
                      int                   indent,
                      Shape                *shape,
                      FilterPrimitive      *f,
                      GtkSvgSerializeFlags  flags)
{
  indent_for_elt (s, indent);
  g_string_append_printf (s, "</%s>", filter_types[f->type].name);
}

static void
serialize_shape (GString              *s,
                 GtkSvg               *svg,
                 int                   indent,
                 Shape                *shape,
                 GtkSvgSerializeFlags  flags)
{
  if (shape->type == SHAPE_DEFS &&
      shape->shapes->len == 0)
    return;

  if (indent > 0) /* Hack: this is for <svg> */
    {
      indent_for_elt (s, indent);
      g_string_append_printf (s, "<%s", shape_types[shape->type].name);
      serialize_shape_attrs (s, svg, indent, shape, flags);
      serialize_gpa_attrs (s, svg, indent, shape, flags);
      g_string_append_c (s, '>');
    }

  if (shape_type_has_color_stops (shape->type))
    {
      for (unsigned int idx = 0; idx < shape->color_stops->len; idx++)
        serialize_color_stop (s, svg, indent + 2, shape, idx, flags);
    }

  if (shape_type_has_filters (shape->type))
    {
      for (unsigned int idx = 0; idx < shape->filters->len; idx++)
        {
          FilterPrimitive *f = g_ptr_array_index (shape->filters, idx);

          serialize_filter_begin (s, svg, indent + 2, shape, f, idx, flags);

          if (f->type == FE_MERGE)
            {
              for (idx++; idx < shape->filters->len; idx++)
                {
                  FilterPrimitive *f2 = g_ptr_array_index (shape->filters, idx);
                  if (f2->type != FE_MERGE_NODE)
                    {
                      idx--;
                      break;
                    }

                  serialize_filter_begin (s, svg, indent + 4, shape, f2, idx, flags);
                  serialize_filter_end (s, svg, indent + 4, shape, f2, flags);
                }
            }

          if (f->type == FE_COMPONENT_TRANSFER)
            {
              for (idx++; idx < shape->filters->len; idx++)
                {
                  FilterPrimitive *f2 = g_ptr_array_index (shape->filters, idx);
                  if (f2->type != FE_FUNC_R &&
                      f2->type != FE_FUNC_G &&
                      f2->type != FE_FUNC_B &&
                      f2->type != FE_FUNC_A)
                    {
                      idx--;
                      break;
                    }

                  serialize_filter_begin (s, svg, indent + 4, shape, f2, idx, flags);
                  serialize_filter_end (s, svg, indent + 4, shape, f2, flags);
                }
            }

          serialize_filter_end (s, svg, indent + 2, shape, f, flags);
        }
    }

  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          Animation *a = g_ptr_array_index (shape->animations, i);
          if (a->idx == 0)
            serialize_animation (s, svg, indent + 2, a, flags);
        }
    }

  if (shape_type_has_text (shape->type))
    {
      for (guint i = 0; i < shape->text->len; i++)
        {
          TextNode *node = &g_array_index (shape->text, TextNode, i);
          switch (node->type)
            {
            case TEXT_NODE_SHAPE:
              serialize_shape (s, svg, indent + 2, node->shape.shape, flags);
              break;
            case TEXT_NODE_CHARACTERS:
              {
                char *escaped = g_markup_escape_text (node->characters.text, -1);
                g_string_append (s, escaped);
                g_free (escaped);
              }
              break;
            default:
              g_assert_not_reached ();
            }
        }
      g_string_append_printf (s, "</%s>", shape_types[shape->type].name);
      return;
    }
  else if (shape_type_has_shapes (shape->type))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          Shape *sh = g_ptr_array_index (shape->shapes, i);
          serialize_shape (s, svg, indent + 2 , sh, flags);
        }
    }

  if (indent > 0)
    {
      indent_for_elt (s, indent);
      g_string_append_printf (s, "</%s>", shape_types[shape->type].name);
    }
}

/* }}} */
/* {{{ Rendering */

/* Rendering works by walking the shape tree from the top.
 * For each shape, we set up a 'compositing group' before
 * rendering the content of the shape. Establishing a group
 * involves handling transforms, opacity, filters, blending,
 * masking and clipping.
 *
 * This is the core of the rendering machinery:
 *
 * push_group (shape, context);
 * paint_shape (shape, context);
 * pop_group (shape, context);
 *
 * This process is highly recursive. For example obtaining
 * the clip path or mask may require rendering a different
 * part of the shape tree (in a special mode).
 */
typedef enum
{
  CLIPPING,
  MASKING,
  RENDERING,
  MARKERS,
} RenderOp;

typedef struct
{
  GtkSvg *svg;
  double width;
  double height;
  const graphene_rect_t *viewport;
  GSList *viewport_stack;
  GtkSnapshot *snapshot;
  GSList *transforms;
  int64_t current_time;
  const GdkRGBA *colors;
  size_t n_colors;
  double weight;
  RenderOp op;
  GSList *op_stack;
  gboolean op_changed;
  int depth;
  uint64_t instance_count;
  GSList *ctx_shape_stack;
} PaintContext;

/* Our paint machinery can be used in different modes - for
 * creating clip paths, masks, for rendering markers, and
 * regular rendering. Since these operation are mutually
 * recursive, we maintain a stack of modes, aka RenderOps.
 *
 * The op_changed flag is used to treat the topmost shape
 * in the new mode differently. This is relevant since
 * markers, clip paths, etc can be nested, and just because
 * we see a marker while in MARKERS mode does not mean
 * that we should render it.
 */
static void
push_op (PaintContext *context,
         RenderOp      op)
{
  context->op_stack = g_slist_prepend (context->op_stack, GUINT_TO_POINTER (context->op));
  context->op = op;
  context->op_changed = TRUE;
}

static void
pop_op (PaintContext *context)
{
  GSList *tos = context->op_stack;

  g_assert (tos != NULL);

  context->op = GPOINTER_TO_UINT (tos->data);
  context->op_stack = context->op_stack->next;
  g_slist_free_1 (tos);
}

/* In certain contexts (use, and markers), paint can be
 * context-fill or context-stroke. Again, this can be
 * recursive, so we have a stack of context shapes.
 */
static void
push_ctx_shape (PaintContext *context,
                Shape        *shape)
{
  context->ctx_shape_stack = g_slist_prepend (context->ctx_shape_stack, shape);
}

static void
pop_ctx_shape (PaintContext *context)
{
  GSList *tos = context->ctx_shape_stack;

  g_assert (tos != NULL);
  context->ctx_shape_stack = context->ctx_shape_stack->next;
  g_slist_free_1 (tos);
}

/* Viewports are used to resolve percentages in lengths.
 * Nested svg elements create new viewports, and we have
 * a stack of them.
 */
static void
push_viewport (PaintContext          *context,
               const graphene_rect_t *viewport)
{
  context->viewport_stack = g_slist_prepend (context->viewport_stack,
                                             (gpointer) context->viewport);
  context->viewport = viewport;
}

static const graphene_rect_t *
pop_viewport (PaintContext *context)
{
  GSList *tos = context->viewport_stack;
  const graphene_rect_t *viewport = context->viewport;

  g_assert (tos != NULL);

  context->viewport = (const graphene_rect_t *) tos->data;
  context->viewport_stack = tos->next;
  g_slist_free_1 (tos);

  return viewport;
}

static void
push_transform (PaintContext *context,
                GskTransform *transform)
{
  context->transforms = g_slist_prepend (context->transforms,
                                         gsk_transform_ref (transform));
}

static void
pop_transform (PaintContext *context)
{
  GSList *tos = context->transforms;

  g_assert (tos != NULL);

  context->transforms = tos->next;
  gsk_transform_unref ((GskTransform *) tos->data);
  g_slist_free_1 (tos);
}

static GskTransform *
context_get_host_transform (PaintContext *context)
{
  GskTransform *transform = NULL;

  for (GSList *l = context->transforms; l; l = l->next)
    {
      GskTransform *t = l->data;

      t = gsk_transform_invert (gsk_transform_ref (t));
      t = gsk_transform_transform (t, transform);
      gsk_transform_unref (transform);
      transform = t;
    }

  return transform;
}

static void push_group   (Shape        *shape,
                          PaintContext *context);
static void pop_group    (Shape        *shape,
                          PaintContext *context);
static void paint_shape  (Shape        *shape,
                          PaintContext *context);
static void render_shape (Shape        *shape,
                          PaintContext *context);
static void fill_shape   (Shape        *shape,
                          GskPath      *path,
                          SvgPaint     *paint,
                          double        opacity,
                          PaintContext *context);

/* {{{ Filters */

static GskRenderNode *
apply_alpha_only (GskRenderNode *child)
{
  GskComponentTransfer *zero;
  GskComponentTransfer *identity;
  GskRenderNode *node;

  zero = gsk_component_transfer_new_linear (0, 0);
  identity = gsk_component_transfer_new_identity ();
  node = gsk_component_transfer_node_new (child, zero, zero, zero, identity);
  gsk_component_transfer_free (identity);
  gsk_component_transfer_free (zero);

  gsk_render_node_unref (child);

  return node;
}

static GskRenderNode *
empty_node (void)
{
  return gsk_container_node_new (NULL, 0);
}

static GskRenderNode *
error_node (const graphene_rect_t *bounds)
{
  GdkRGBA pink = { 255 / 255., 105 / 255., 180 / 255., 1.0 };
  return gsk_color_node_new (&pink, bounds);
}

/* We need to store the filter subregion for each result node,
 * for the sole reason that feTile needs it to size its tile.
 */
typedef struct
{
  int ref_count;
  GskRenderNode *node;
  graphene_rect_t bounds;
} FilterResult;

static FilterResult *
filter_result_new (GskRenderNode         *node,
                   const graphene_rect_t *bounds)
{
  FilterResult *res = g_new0 (FilterResult, 1);
  res->ref_count = 1;
  res->node = gsk_render_node_ref (node);
  if (bounds)
    res->bounds = *bounds;
  else
    gsk_render_node_get_bounds (node, &res->bounds);

  return res;
}

static FilterResult *
filter_result_ref (FilterResult *res)
{
  res->ref_count++;
  return res;
}

static void
filter_result_unref (FilterResult *res)
{
  res->ref_count--;
  if (res->ref_count == 0)
    {
      gsk_render_node_unref (res->node);
      g_free (res);
    }
}

static FilterResult *
get_input_for_ref (SvgValue              *in,
                   const graphene_rect_t *subregion,
                   Shape                 *shape,
                   PaintContext          *context,
                   GskRenderNode         *source,
                   GHashTable            *results)
{
  SvgFilterPrimitiveRef *ref = (SvgFilterPrimitiveRef *) in;
  GskRenderNode *node;
  FilterResult *res;

  switch (ref->type)
    {
    case DEFAULT_SOURCE:
      return filter_result_ref (g_hash_table_lookup (results, ""));
    case SOURCE_GRAPHIC:
      return filter_result_new (source, NULL);
    case SOURCE_ALPHA:
      res = filter_result_new (source, NULL);
      res->node = apply_alpha_only (res->node);
      return res;
    case BACKGROUND_IMAGE:
      node = gsk_paste_node_new (subregion, 0);
      res = filter_result_new (node, subregion);
      gsk_render_node_unref (node);
      return res;
    case BACKGROUND_ALPHA:
      node = gsk_paste_node_new (subregion, 0);
      res = filter_result_new (node, subregion);
      gsk_render_node_unref (node);
      res->node = apply_alpha_only (res->node);
      return res;
    case PRIMITIVE_REF:
      res = g_hash_table_lookup (results, ref->ref);
      if (res)
        return filter_result_ref (res);
      else
        return filter_result_ref (g_hash_table_lookup (results, ""));
    case FILL_PAINT:
    case STROKE_PAINT:
      {
        GskPath *path;
        GskPathBuilder *builder;
        SvgPaint *paint;
        double opacity;

        if (ref->type == FILL_PAINT)
          {
            paint = (SvgPaint *) shape->current[SHAPE_ATTR_FILL];
            opacity = svg_number_get (shape->current[SHAPE_ATTR_FILL_OPACITY], 1);
          }
        else
          {
            paint = (SvgPaint *) shape->current[SHAPE_ATTR_STROKE];
            opacity = svg_number_get (shape->current[SHAPE_ATTR_STROKE_OPACITY], 1);
          }

        builder = gsk_path_builder_new ();
        gsk_path_builder_add_rect (builder, subregion);
        path = gsk_path_builder_free_to_path (builder);

        gtk_snapshot_push_collect (context->snapshot);
        fill_shape (shape, path, paint, opacity, context);
        node = gtk_snapshot_pop_collect (context->snapshot);

        if (!node)
          node = empty_node ();

        res = filter_result_new (node, subregion);

        gsk_render_node_unref (node);
        gsk_path_unref (path);
        return res;
      }
    default:
      g_assert_not_reached ();
    }

  return NULL;
}

static void
determine_filter_subregion_from_refs (SvgFilterPrimitiveRef **refs,
                                      unsigned int            n_refs,
                                      gboolean                is_first,
                                      const graphene_rect_t  *filter_region,
                                      GHashTable             *results,
                                      graphene_rect_t        *subregion)
{
  for (unsigned int i = 0; i < n_refs; i++)
    {
      SvgFilterPrimitiveRef *ref = refs[i];

      if (ref->type == SOURCE_GRAPHIC || ref->type == SOURCE_ALPHA ||
          ref->type == BACKGROUND_IMAGE || ref->type == BACKGROUND_ALPHA ||
          ref->type == FILL_PAINT || ref->type == STROKE_PAINT ||
          (ref->type == DEFAULT_SOURCE && is_first))
        {
          if (i == 0)
            graphene_rect_init_from_rect (subregion, filter_region);
          else
            graphene_rect_union (filter_region, subregion, subregion);
        }
      else
        {
          FilterResult *res;

          if (ref->type == DEFAULT_SOURCE)
            res = g_hash_table_lookup (results, "");
          else if (ref->type == PRIMITIVE_REF)
            res = g_hash_table_lookup (results, ref->ref);
          else
            g_assert_not_reached ();

          if (res)
            {
              if (i == 0)
                graphene_rect_init_from_rect (subregion, &res->bounds);
              else
                graphene_rect_union (&res->bounds, subregion, subregion);
            }
          else
            {
              if (i == 0)
                graphene_rect_init_from_rect (subregion, filter_region);
              else
                graphene_rect_union (filter_region, subregion, subregion);
            }
        }
    }
}

static gboolean
determine_filter_subregion (FilterPrimitive       *f,
                            Shape                 *filter,
                            unsigned int           idx,
                            const graphene_rect_t *bounds,
                            const graphene_rect_t *viewport,
                            const graphene_rect_t *filter_region,
                            GHashTable            *results,
                            graphene_rect_t       *subregion)
{
  if (f->attrs & (BIT (filter_attr_idx (f->type, SHAPE_ATTR_FE_X)) |
                  BIT (filter_attr_idx (f->type, SHAPE_ATTR_FE_Y)) |
                  BIT (filter_attr_idx (f->type, SHAPE_ATTR_FE_WIDTH)) |
                  BIT (filter_attr_idx (f->type, SHAPE_ATTR_FE_HEIGHT))))
    {
      graphene_rect_init_from_rect (subregion, filter_region);

      if (f->attrs & BIT (filter_attr_idx (f->type, SHAPE_ATTR_FE_X)))
        {
          SvgValue *n = filter_get_current_value (f, SHAPE_ATTR_FE_X);
          if (svg_enum_get (filter->current[SHAPE_ATTR_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
            subregion->origin.x = bounds->origin.x + svg_number_get (n, 1) * bounds->size.width;
          else
            subregion->origin.x = viewport->origin.x + svg_number_get (n, viewport->size.width);
        }

      if (f->attrs & BIT (filter_attr_idx (f->type, SHAPE_ATTR_FE_Y)))
        {
          SvgValue *n = filter_get_current_value (f, SHAPE_ATTR_FE_Y);
          if (svg_enum_get (filter->current[SHAPE_ATTR_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
            subregion->origin.y = bounds->origin.y + svg_number_get (n, 1) * bounds->size.height;
          else
            subregion->origin.y = viewport->origin.y + svg_number_get (n, viewport->size.height);
        }

      if (f->attrs & BIT (filter_attr_idx (f->type, SHAPE_ATTR_FE_WIDTH)))
        {
          SvgValue *n = filter_get_current_value (f, SHAPE_ATTR_FE_WIDTH);
          if (svg_enum_get (filter->current[SHAPE_ATTR_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
            subregion->size.width = svg_number_get (n, 1) * bounds->size.width;
          else
            subregion->size.width = svg_number_get (n, viewport->size.width);
        }

      if (f->attrs & BIT (filter_attr_idx (f->type, SHAPE_ATTR_FE_HEIGHT)))
        {
          SvgValue *n = filter_get_current_value (f, SHAPE_ATTR_FE_HEIGHT);
          if (svg_enum_get (filter->current[SHAPE_ATTR_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
            subregion->size.height = svg_number_get (n, 1) * bounds->size.height;
          else
            subregion->size.height = svg_number_get (n, viewport->size.height);
        }
    }
  else
    {
      gboolean is_first = idx == 0;

      switch (f->type)
        {
        case FE_FLOOD:
        case FE_IMAGE:
          /* zero inputs */
          graphene_rect_init_from_rect (subregion, filter_region);
          break;

        case FE_TILE:
        case FE_MERGE_NODE:
        case FE_FUNC_R:
        case FE_FUNC_G:
        case FE_FUNC_B:
        case FE_FUNC_A:
          /* special case */
          graphene_rect_init_from_rect (subregion, filter_region);
          break;

        case FE_BLUR:
        case FE_COLOR_MATRIX:
        case FE_COMPONENT_TRANSFER:
        case FE_DROPSHADOW:
        case FE_OFFSET:
          {
            SvgFilterPrimitiveRef *ref = (SvgFilterPrimitiveRef *) filter_get_current_value (f, SHAPE_ATTR_FE_IN);

            determine_filter_subregion_from_refs (&ref, 1, is_first, filter_region, results, subregion);
          }
          break;

        case FE_BLEND:
        case FE_COMPOSITE:
        case FE_DISPLACEMENT:
          {
            SvgFilterPrimitiveRef *refs[2];

            refs[0] = (SvgFilterPrimitiveRef *) filter_get_current_value (f, SHAPE_ATTR_FE_IN);
            refs[1] = (SvgFilterPrimitiveRef *) filter_get_current_value (f, SHAPE_ATTR_FE_IN2);

            determine_filter_subregion_from_refs (refs, 2, is_first, filter_region, results, subregion);
          }
          break;

        case FE_MERGE:
          {
            SvgFilterPrimitiveRef **refs;
            unsigned int n_refs;

            refs = g_newa (SvgFilterPrimitiveRef *, filter->filters->len);
            n_refs = 0;
            for (idx++; idx < filter->filters->len; idx++)
              {
                FilterPrimitive *ff = g_ptr_array_index (filter->filters, idx);

                if (ff->type != FE_MERGE_NODE)
                  break;

                refs[n_refs] = (SvgFilterPrimitiveRef *) filter_get_current_value (ff, SHAPE_ATTR_FE_IN);
                n_refs++;
              }

            determine_filter_subregion_from_refs (refs, n_refs, is_first, filter_region, results, subregion);
          }
          break;

        default:
          g_assert_not_reached ();
        }
    }

  return gsk_rect_intersection (filter_region, subregion, subregion);
}

static void recompute_current_values (Shape        *shape,
                                      Shape        *parent,
                                      PaintContext *context);

static GskRenderNode *
apply_filter_tree (Shape         *shape,
                   Shape         *filter,
                   PaintContext  *context,
                   GskRenderNode *source)
{
  graphene_rect_t filter_region;
  GHashTable *results;
  graphene_rect_t bounds, rect;
  GdkColorState *color_state;

  if (filter->filters->len == 0)
    return empty_node ();

  if (!shape_get_current_bounds (shape, context->viewport, context->svg, &bounds))
    return gsk_render_node_ref (source);

  if (svg_enum_get (filter->current[SHAPE_ATTR_BOUND_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
    {
      if (bounds.size.width == 0 || bounds.size.height == 0)
        return empty_node ();

      filter_region.origin.x = bounds.origin.x + svg_number_get (filter->current[SHAPE_ATTR_X], 1) * bounds.size.width;
      filter_region.origin.y = bounds.origin.y + svg_number_get (filter->current[SHAPE_ATTR_Y], 1) * bounds.size.height;
      filter_region.size.width = svg_number_get (filter->current[SHAPE_ATTR_WIDTH], 1) * bounds.size.width;
      filter_region.size.height = svg_number_get (filter->current[SHAPE_ATTR_HEIGHT], 1) * bounds.size.height;
    }
  else
    {
      filter_region.origin.x = svg_number_get (filter->current[SHAPE_ATTR_X], context->viewport->size.width);
      filter_region.origin.y = svg_number_get (filter->current[SHAPE_ATTR_Y], context->viewport->size.height);
      filter_region.size.width = svg_number_get (filter->current[SHAPE_ATTR_WIDTH], context->viewport->size.width);
      filter_region.size.height = svg_number_get (filter->current[SHAPE_ATTR_HEIGHT], context->viewport->size.height);
    }

  results = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) filter_result_unref);

  if (!gsk_rect_intersection (&filter_region, &source->bounds, &rect))
    return empty_node ();

  if (!gsk_rect_equal (&filter_region, &rect))
    {
      GskRenderNode *pad, *padded;

      pad = gsk_color_node_new (&GDK_RGBA_TRANSPARENT, &filter_region);
      padded = gsk_container_node_new ((GskRenderNode*[]) { pad, source }, 2);
      g_hash_table_insert (results, (gpointer) "", filter_result_new (padded, NULL));
      gsk_render_node_unref (padded);
      gsk_render_node_unref (pad);
    }
  else
    g_hash_table_insert (results, (gpointer) "", filter_result_new (source, NULL));

  if (svg_enum_get (filter->current[SHAPE_ATTR_COLOR_INTERPOLATION_FILTERS]) == COLOR_INTERPOLATION_LINEAR)
    color_state = GDK_COLOR_STATE_SRGB_LINEAR;
  else
    color_state = GDK_COLOR_STATE_SRGB;

  for (unsigned int i = 0; i < filter->filters->len; i++)
    {
      FilterPrimitive *f = g_ptr_array_index (filter->filters, i);
      graphene_rect_t subregion;
      GskRenderNode *result = NULL;

      if (!determine_filter_subregion (f, filter, i, &bounds, context->viewport, &filter_region, results, &subregion))
        {
          graphene_rect_init (&subregion, 0, 0, 0, 0);
          result = gsk_container_node_new (NULL, 0);
          goto got_result;
        }

      switch (f->type)
        {
        case FE_BLUR:
          {
            FilterResult *in;
            double std_dev;
            EdgeMode edge_mode;
            GskRenderNode *child;
            SvgNumbers *num;

            in = get_input_for_ref (filter_get_current_value (f, SHAPE_ATTR_FE_IN), &subregion, shape, context, source, results);

            num = (SvgNumbers *) filter_get_current_value (f, SHAPE_ATTR_FE_STD_DEV);
            if (num->n_values == 2 &&
                num->values[0].value != num->values[1].value)
              gtk_svg_rendering_error (context->svg,
                                       "Separate x/y values for stdDeviation not supported");
            std_dev = num->values[0].value;

            edge_mode = svg_enum_get (filter_get_current_value (f, SHAPE_ATTR_FE_BLUR_EDGE_MODE));

            switch (edge_mode)
              {
             case EDGE_MODE_DUPLICATE:
               child = gsk_repeat_node_new2 (&filter_region, in->node, &in->bounds, GSK_REPEAT_PAD);
               break;
             case EDGE_MODE_WRAP:
               child = gsk_repeat_node_new2 (&filter_region, in->node, &in->bounds, GSK_REPEAT_REPEAT);
               break;
             case EDGE_MODE_NONE:
               child = gsk_render_node_ref (in->node);
               break;
             default:
               g_assert_not_reached ();
             }
             if (std_dev < 0)
               {
                 gtk_svg_rendering_error (context->svg, "stdDeviation < 0");
                 result = gsk_render_node_ref (child);
               }
             else
               {
                 result = gsk_blur_node_new (child, 2 * std_dev);
               }
             gsk_render_node_unref (child);
             filter_result_unref (in);
          }
          break;

        case FE_FLOOD:
          {
            SvgColor *color = (SvgColor *) filter_get_current_value (f, SHAPE_ATTR_FE_COLOR);
            SvgValue *alpha = filter_get_current_value (f, SHAPE_ATTR_FE_OPACITY);
            GdkColor c;

            gdk_color_init_copy (&c, &color->color);
            c.alpha *= svg_number_get (alpha, 1);
            result = gsk_color_node_new2 (&c, &subregion);
            gdk_color_finish (&c);
          }
          break;

        case FE_MERGE:
          {
            GskRenderNode **children;
            unsigned int n_children;

            children = g_new0 (GskRenderNode *, filter->filters->len);
            n_children = 0;
            for (i++; i < filter->filters->len; i++)
              {
                FilterPrimitive *ff = g_ptr_array_index (filter->filters, i);
                FilterResult *in;

                if (ff->type != FE_MERGE_NODE)
                  {
                    i--;
                    break;
                  }

                in = get_input_for_ref (filter_get_current_value (ff, SHAPE_ATTR_FE_IN), &subregion, shape, context, source, results);

                children[n_children] = gsk_render_node_ref (in->node);
                n_children++;
                filter_result_unref (in);
              }
            result = gsk_container_node_new (children, n_children);
            for (unsigned int j = 0; j < n_children; j++)
              gsk_render_node_unref (children[j]);
            g_free (children);
          }
          break;

        case FE_MERGE_NODE:
          break;

        case FE_BLEND:
          {
            FilterResult *in, *in2;
            GskBlendMode blend_mode;

            in = get_input_for_ref (filter_get_current_value (f, SHAPE_ATTR_FE_IN), &subregion, shape, context, source, results);
            in2 = get_input_for_ref (filter_get_current_value (f, SHAPE_ATTR_FE_IN2), &subregion, shape, context, source, results);
            blend_mode = svg_enum_get (filter_get_current_value (f, SHAPE_ATTR_FE_BLEND_MODE));

            result = gsk_blend_node_new2 (in2->node, in->node, color_state, blend_mode);

            filter_result_unref (in);
            filter_result_unref (in2);
          }
          break;

        case FE_OFFSET:
          {
            FilterResult *in;
            double dx, dy;
            GskTransform *transform;

            in = get_input_for_ref (filter_get_current_value (f, SHAPE_ATTR_FE_IN), &subregion, shape, context, source, results);

            if (svg_enum_get (filter->current[SHAPE_ATTR_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
              {
                dx = svg_number_get (filter_get_current_value (f, SHAPE_ATTR_FE_DX), 1) * bounds.size.width;
                dy = svg_number_get (filter_get_current_value (f, SHAPE_ATTR_FE_DY), 1) * bounds.size.height;
              }
            else
              {
                dx = svg_number_get (filter_get_current_value (f, SHAPE_ATTR_FE_DX), 1);
                dy = svg_number_get (filter_get_current_value (f, SHAPE_ATTR_FE_DY), 1);
              }

            transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (dx, dy));
            result = apply_transform (in->node, transform);
            filter_result_unref (in);
            gsk_transform_unref (transform);
          }
          break;

        case FE_COMPOSITE:
          {
            FilterResult *in, *in2;
            CompositeOperator svg_op;

            in = get_input_for_ref (filter_get_current_value (f, SHAPE_ATTR_FE_IN), &subregion, shape, context, source, results);
            in2 = get_input_for_ref (filter_get_current_value (f, SHAPE_ATTR_FE_IN2), &subregion, shape, context, source, results);
            svg_op = svg_enum_get (filter_get_current_value (f, SHAPE_ATTR_FE_COMPOSITE_OPERATOR));

            if (svg_op == COMPOSITE_OPERATOR_LIGHTER)
              {
                gtk_svg_rendering_error (context->svg,
                                         "lighter composite operator not supported");
                result = gsk_container_node_new ((GskRenderNode*[]) { in2->node, in->node }, 2);
              }
            else if (svg_op == COMPOSITE_OPERATOR_ARITHMETIC)
              {
                float k1, k2, k3, k4;

                k1 = svg_number_get (filter_get_current_value (f, SHAPE_ATTR_FE_COMPOSITE_K1), 1);
                k2 = svg_number_get (filter_get_current_value (f, SHAPE_ATTR_FE_COMPOSITE_K2), 1);
                k3 = svg_number_get (filter_get_current_value (f, SHAPE_ATTR_FE_COMPOSITE_K3), 1);
                k4 = svg_number_get (filter_get_current_value (f, SHAPE_ATTR_FE_COMPOSITE_K4), 1);

                result = gsk_arithmetic_node_new (&subregion, in->node, in2->node, color_state, k1, k2, k3, k4);
              }
            else if (svg_op == COMPOSITE_OPERATOR_OVER)
              {
                result = gsk_container_node_new ((GskRenderNode*[]) { in2->node, in->node }, 2);
              }
            else if (svg_op == COMPOSITE_OPERATOR_IN)
              {
                result = gsk_mask_node_new (in->node, in2->node, GSK_MASK_MODE_ALPHA);
              }
            else if (svg_op == COMPOSITE_OPERATOR_OUT)
              {
                result = gsk_mask_node_new (in->node, in2->node, GSK_MASK_MODE_INVERTED_ALPHA);
              }
            else
              {
                graphene_rect_t mask_bounds;
                GskRenderNode *comp, *mask, *container;
                GskPorterDuff op;
                GskIsolation features;

                op = svg_composite_operator_to_gsk (svg_op);

                graphene_rect_union (&in->bounds, &in2->bounds, &mask_bounds);
                mask = gsk_color_node_new (&(GdkRGBA) { 0, 0, 0, 1 }, &mask_bounds);

                comp = gsk_composite_node_new (in->node, mask, op);
                container = gsk_container_node_new ((GskRenderNode*[]) { in2->node, comp }, 2);
                features = gsk_isolation_features_simplify_for_node (GSK_ISOLATION_ALL, container);
                if (features)
                  {
                    result = gsk_isolation_node_new (container, features);
                    gsk_render_node_unref (container);
                  }
                else
                  result = container;

                gsk_render_node_unref (comp);
                gsk_render_node_unref (mask);
              }

            filter_result_unref (in);
            filter_result_unref (in2);
          }
          break;

        case FE_TILE:
          {
            FilterResult *in;

            in = get_input_for_ref (filter_get_current_value (f, SHAPE_ATTR_FE_IN), &subregion, shape, context, source, results);

            result = gsk_repeat_node_new (&subregion, in->node, &in->bounds);

            filter_result_unref (in);
          }
          break;

        case FE_COMPONENT_TRANSFER:
          {
            GskComponentTransfer *r = gsk_component_transfer_new_identity ();
            GskComponentTransfer *g = gsk_component_transfer_new_identity ();
            GskComponentTransfer *b = gsk_component_transfer_new_identity ();
            GskComponentTransfer *a = gsk_component_transfer_new_identity ();
            FilterResult *in;

            in = get_input_for_ref (filter_get_current_value (f, SHAPE_ATTR_FE_IN), &subregion, shape, context, source, results);

            for (i++; i < filter->filters->len; i++)
              {
                FilterPrimitive *ff = g_ptr_array_index (filter->filters, i);
                if (ff->type == FE_FUNC_R)
                  {
                    gsk_component_transfer_free (r);
                    r = filter_get_component_transfer (ff);
                  }
                else if (ff->type == FE_FUNC_G)
                  {
                    gsk_component_transfer_free (g);
                    g = filter_get_component_transfer (ff);
                  }
                else if (ff->type == FE_FUNC_B)
                  {
                    gsk_component_transfer_free (b);
                    b = filter_get_component_transfer (ff);
                  }
                else if (ff->type == FE_FUNC_A)
                  {
                    gsk_component_transfer_free (a);
                    a = filter_get_component_transfer (ff);
                  }
                else
                  {
                    i--;
                    break;
                  }
              }

            result = gsk_component_transfer_node_new2 (in->node, color_state, r, g, b, a);

            gsk_component_transfer_free (r);
            gsk_component_transfer_free (g);
            gsk_component_transfer_free (b);
            gsk_component_transfer_free (a);
            filter_result_unref (in);
          }
          break;

        case FE_FUNC_R:
        case FE_FUNC_G:
        case FE_FUNC_B:
        case FE_FUNC_A:
          break;

        case FE_COLOR_MATRIX:
          {
            FilterResult *in;
            graphene_matrix_t matrix;
            graphene_vec4_t offset;
            GskRenderNode *node;

            in = get_input_for_ref (filter_get_current_value (f, SHAPE_ATTR_FE_IN), &subregion, shape, context, source, results);
            color_matrix_type_get_color_matrix (svg_enum_get (filter_get_current_value (f, SHAPE_ATTR_FE_COLOR_MATRIX_TYPE)),
                                                filter_get_current_value (f, SHAPE_ATTR_FE_COLOR_MATRIX_VALUES),
                                                &matrix, &offset);

            node = skip_debug_node (in->node);
            if (gsk_render_node_get_node_type (node) == GSK_COLOR_NODE)
              {
                const GdkColor *color = gsk_color_node_get_gdk_color (node);
                GdkColor new_color;

                apply_color_matrix (color_state, &matrix, &offset,  color, &new_color);
                result = gsk_color_node_new2 (&new_color, &node->bounds);
                gdk_color_finish (&new_color);
              }
            else
              result = gsk_color_matrix_node_new2 (in->node, color_state, &matrix, &offset);

            filter_result_unref (in);
          }
          break;

        case FE_DROPSHADOW:
          {
            FilterResult *in;
            double std_dev;
            double dx, dy;
            SvgColor *color;
            SvgValue *alpha;
            GskShadowEntry shadow;
            SvgNumbers *num;

            in = get_input_for_ref (filter_get_current_value (f, SHAPE_ATTR_FE_IN), &subregion, shape, context, source, results);
            num = (SvgNumbers *) filter_get_current_value (f, SHAPE_ATTR_FE_STD_DEV);
            if (num->n_values == 2 &&
                num->values[0].value != num->values[1].value)
              gtk_svg_rendering_error (context->svg,
                                       "Separate x/y values for stdDeviation not supported");
            std_dev = num->values[0].value;

            if (svg_enum_get (filter->current[SHAPE_ATTR_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
              {
                dx = svg_number_get (filter_get_current_value (f, SHAPE_ATTR_FE_DX), 1) * bounds.size.width;
                dy = svg_number_get (filter_get_current_value (f, SHAPE_ATTR_FE_DY), 1) * bounds.size.height;
              }
            else
              {
                dx = svg_number_get (filter_get_current_value (f, SHAPE_ATTR_FE_DX), 1);
                dy = svg_number_get (filter_get_current_value (f, SHAPE_ATTR_FE_DY), 1);
              }

            color = (SvgColor *) filter_get_current_value (f, SHAPE_ATTR_FE_COLOR);
            alpha = filter_get_current_value (f, SHAPE_ATTR_FE_OPACITY);

            gdk_color_init_copy (&shadow.color, &color->color);
            shadow.color.alpha *= svg_number_get (alpha, 1);
            shadow.offset.x = dx;
            shadow.offset.y = dy;
            shadow.radius = 2 * std_dev;

            result = gsk_shadow_node_new2 (in->node, &shadow, 1);

            gdk_color_finish (&shadow.color);

            filter_result_unref (in);
          }
          break;

        case FE_IMAGE:
          {
            SvgHref *href = (SvgHref *) filter_get_current_value (f, SHAPE_ATTR_FE_IMAGE_HREF);
            SvgContentFit *cf = (SvgContentFit *) filter_get_current_value (f, SHAPE_ATTR_FE_IMAGE_CONTENT_FIT);
            GskTransform *transform;
            GskRenderNode *node;

            if (href->kind == HREF_NONE)
              {
                result = empty_node ();
              }
            else if (href->texture != NULL)
              {
                graphene_rect_t vb;
                double sx, sy, tx, ty;

                graphene_rect_init (&vb,
                                    0, 0,
                                    gdk_texture_get_width (href->texture),
                                    gdk_texture_get_height (href->texture));

                compute_viewport_transform (cf->is_none,
                                            cf->align_x,
                                            cf->align_y,
                                            cf->meet,
                                            &vb,
                                            subregion.origin.x, subregion.origin.y,
                                            subregion.size.width, subregion.size.height,
                                            &sx, &sy, &tx, &ty);

                transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (tx, ty));
                transform = gsk_transform_scale (transform, sx, sy);

                node = gsk_texture_node_new (href->texture, &vb);
                result = apply_transform (node, transform);
                gsk_render_node_unref (node);
              }
            else if (href->shape != NULL)
              {
                gtk_snapshot_push_collect (context->snapshot);
                render_shape (href->shape, context);
                node = gtk_snapshot_pop_collect (context->snapshot);
                if (node)
                  {
                    transform = gsk_transform_translate (NULL, &subregion.origin);
                    result = apply_transform (node, transform);
                    gsk_render_node_unref (node);
                    gsk_transform_unref (transform);
                  }
                else
                  {
                    gtk_svg_rendering_error (context->svg, "No content for <feImage>");
                    result = error_node (&subregion);
                  }
              }
            else
              {
                gtk_svg_rendering_error (context->svg, "No content for <feImage>");
                result = error_node (&subregion);
              }
          }
          break;

        case FE_DISPLACEMENT:
          {
            FilterResult *in, *in2;
            GdkColorChannel channels[2];
            graphene_size_t max, scale;
            graphene_point_t offset;

            in = get_input_for_ref (filter_get_current_value (f, SHAPE_ATTR_FE_IN), &subregion, shape, context, source, results);
            in2 = get_input_for_ref (filter_get_current_value (f, SHAPE_ATTR_FE_IN2), &subregion, shape, context, source, results);
            channels[0] = svg_enum_get (filter_get_current_value (f, SHAPE_ATTR_FE_DISPLACEMENT_X));
            channels[1] = svg_enum_get (filter_get_current_value (f, SHAPE_ATTR_FE_DISPLACEMENT_Y));
            scale.width = svg_number_get (filter_get_current_value (f, SHAPE_ATTR_FE_DISPLACEMENT_SCALE), 1);
            scale.height = scale.width;

            max.width = scale.width / 2;
            max.height = scale.height / 2;
            offset.x = offset.y = 0.5;

            result = gsk_displacement_node_new (&subregion,
                                                in->node, in2->node,
                                                channels,
                                                &max, &scale, &offset);

            filter_result_unref (in);
            filter_result_unref (in2);
          }
          break;

        default:
          g_assert_not_reached ();
        }

got_result:
     if (result)
       {
         GskRenderNode *clipped;
         FilterResult *res;
         const char *id;

         if (graphene_rect_contains_rect (&subregion, &result->bounds))
           clipped = gsk_render_node_ref (result);
         else
           clipped = gsk_clip_node_new (result, &subregion);
         res = filter_result_new (clipped, &subregion);
         gsk_render_node_unref (clipped);

         g_hash_table_replace (results, (gpointer) "", filter_result_ref (res));

         id = svg_string_get (f->current[filter_attr_idx (f->type, SHAPE_ATTR_FE_RESULT)]);
         if (id && *id)
           {
             if (g_hash_table_lookup (results, id))
               gtk_svg_rendering_error (context->svg,
                                        "Duplicate filter result ID %s", id);
             else
               g_hash_table_insert (results, (gpointer) id, filter_result_ref (res));
           }

         filter_result_unref (res);
         gsk_render_node_unref (result);
       }
    }

  {
    FilterResult *out;
    GskRenderNode *result;

    out = g_hash_table_lookup (results, "");
    result = gsk_render_node_ref (out->node);

    g_hash_table_unref (results);

    return result;
  }
}

static GskRenderNode *
apply_filter_functions (SvgValue      *filter,
                        PaintContext  *context,
                        Shape         *shape,
                        GskRenderNode *source)
{
  SvgFilter *f = (SvgFilter *) filter;
  GskRenderNode *result = gsk_render_node_ref (source);

  for (unsigned int i = 0; i < f->n_functions; i++)
    {
      FilterFunction *ff = &f->functions[i];
      GskRenderNode *child = result;

      switch (ff->kind)
        {
        case FILTER_NONE:
          result = gsk_render_node_ref (child);
          break;
        case FILTER_BLUR:
          result = gsk_blur_node_new (child, 2 * svg_number_get (ff->simple, 1));
          break;
        case FILTER_OPACITY:
          result = gsk_opacity_node_new (child, svg_number_get (ff->simple, 1));
          break;
        case FILTER_BRIGHTNESS:
        case FILTER_CONTRAST:
        case FILTER_GRAYSCALE:
        case FILTER_HUE_ROTATE:
        case FILTER_INVERT:
        case FILTER_SATURATE:
        case FILTER_SEPIA:
          {
            graphene_matrix_t matrix;
            graphene_vec4_t offset;

            filter_function_get_color_matrix (ff->kind, svg_number_get (ff->simple, 1), &matrix, &offset);
            result = gsk_color_matrix_node_new (child, &matrix, &offset);
          }
          break;
        case FILTER_ALPHA_LEVEL:
          {
            GskComponentTransfer *identity, *alpha;
            float values[10];

            identity = gsk_component_transfer_new_identity ();
            for (unsigned int j = 0; j < 10; j++)
              {
                if ((j + 1) / 10.0 <= svg_number_get (ff->simple, 1))
                  values[j] = 0;
                else
                  values[j] = 1;
              }
            alpha = gsk_component_transfer_new_discrete (10, values);
            result = gsk_component_transfer_node_new (child, identity, identity, identity, alpha);
            gsk_component_transfer_free (identity);
            gsk_component_transfer_free (alpha);
          }
          break;
        case FILTER_DROPSHADOW:
          {
            GskShadowEntry shadow;

            gdk_color_init_copy (&shadow.color, &((SvgColor *) ff->dropshadow.color)->color);
            shadow.offset.x = svg_number_get (ff->dropshadow.dx, 1);
            shadow.offset.y = svg_number_get (ff->dropshadow.dy, 1);
            shadow.radius = 2 * svg_number_get (ff->dropshadow.std_dev, 1);

            result = gsk_shadow_node_new2 (child, &shadow, 1);

            gdk_color_finish (&shadow.color);
          }
          break;
        case FILTER_REF:
          if (ff->ref.shape == NULL)
            {
              gsk_render_node_unref (child);
              return gsk_render_node_ref (source);
            }

          result = apply_filter_tree (shape, ff->ref.shape, context, child);
          break;
        default:
          g_assert_not_reached ();
        }
      gsk_render_node_unref (child);
    }

  return result;
}

/* }}} */
/* {{{ Groups */

static gboolean
needs_copy (Shape         *shape,
            PaintContext  *context,
            const char   **reason)
{
  if (context->op != CLIPPING)
    {
      SvgValue *filter = shape->current[SHAPE_ATTR_FILTER];
      SvgValue *blend = shape->current[SHAPE_ATTR_BLEND_MODE];

      if (svg_filter_needs_backdrop (filter))
        {
          if (reason) *reason = "filter";
          return TRUE;
        }

      if (svg_enum_get (blend) != GSK_BLEND_MODE_DEFAULT)
        {
          if (reason) *reason = "blending";
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
needs_isolation (Shape         *shape,
                 PaintContext  *context,
                 const char   **reason)
{
  if (context->op != CLIPPING)
    {
      SvgValue *isolation = shape->current[SHAPE_ATTR_ISOLATION];
      SvgValue *opacity = shape->current[SHAPE_ATTR_OPACITY];
      SvgValue *blend = shape->current[SHAPE_ATTR_BLEND_MODE];
      SvgValue *filter = shape->current[SHAPE_ATTR_FILTER];
      SvgTransform *tf = (SvgTransform *) shape->current[SHAPE_ATTR_TRANSFORM];
      GskTransform *transform;

      if (shape->type == SHAPE_SVG && shape->parent == NULL)
        {
          if (reason) *reason = "toplevel <svg>";
          return TRUE;
        }

      if (context->op == MASKING && context->op_changed && shape->type == SHAPE_MASK)
        {
          if (reason) *reason = "<mask>";
          return TRUE;
        }

      if (svg_enum_get (isolation) == ISOLATION_ISOLATE)
        {
          if (reason) *reason = "isolate attribute";
          return TRUE;
        }

      if (svg_number_get (opacity, 1) != 1)
        {
          if (reason) *reason = "opacity";
          return TRUE;
        }

      if (svg_enum_get (blend) != GSK_BLEND_MODE_DEFAULT)
        {
          if (reason) *reason = "blending";
          return TRUE;
        }

      if (!svg_filter_is_none (filter))
        {
          if (reason) *reason = "filter";
          return TRUE;
        }

      transform = svg_transform_get_gsk (tf);
      if (gsk_transform_get_category (transform) <= GSK_TRANSFORM_CATEGORY_3D)
        {
          if (reason) *reason = "3D transform";
          gsk_transform_unref (transform);
          return TRUE;
        }
      gsk_transform_unref (transform);
    }

  return FALSE;
}

static gboolean
shape_is_use_target (Shape        *shape,
                     PaintContext *context)
{
  if (context->ctx_shape_stack)
    {
      Shape *ctx_shape = context->ctx_shape_stack->data;

      return ctx_shape->type == SHAPE_USE &&
             ((SvgHref *) ctx_shape->current[SHAPE_ATTR_HREF])->shape == shape;
    }

  return FALSE;
}

static void
push_group (Shape        *shape,
            PaintContext *context)
{
  SvgValue *filter = shape->current[SHAPE_ATTR_FILTER];
  SvgValue *opacity = shape->current[SHAPE_ATTR_OPACITY];
  SvgClip *clip = (SvgClip *) shape->current[SHAPE_ATTR_CLIP_PATH];
  SvgMask *mask = (SvgMask *) shape->current[SHAPE_ATTR_MASK];
  SvgTransform *tf = (SvgTransform *) shape->current[SHAPE_ATTR_TRANSFORM];
  SvgValue *blend = shape->current[SHAPE_ATTR_BLEND_MODE];
  SvgValue *fill_rule = shape->current[SHAPE_ATTR_FILL_RULE];

#ifdef DEBUG
  if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
    {
      if (shape->id)
        gtk_snapshot_push_debug (context->snapshot, "Group for <%s id='%s'> at line %d", shape_types[shape->type].name, shape->id, shape->line);
      else
        gtk_snapshot_push_debug (context->snapshot, "Group for <%s> at line %d", shape_types[shape->type].name, shape->line);
    }
#endif

  if (shape->type == SHAPE_SVG || shape->type == SHAPE_SYMBOL)
    {
      SvgViewBox *vb = (SvgViewBox *) shape->current[SHAPE_ATTR_VIEW_BOX];
      SvgContentFit *cf = (SvgContentFit *) shape->current[SHAPE_ATTR_CONTENT_FIT];
      SvgValue *overflow = shape->current[SHAPE_ATTR_OVERFLOW];
      double x, y, width, height;
      double sx, sy, tx, ty;
      graphene_rect_t view_box;
      graphene_rect_t *viewport;
      double w, h;

      if (shape->parent == NULL)
        {
          x = 0;
          y = 0;
          width = context->svg->current_width;
          height = context->svg->current_height;
          w = context->svg->width;
          h = context->svg->height;
        }
      else
        {
          g_assert (context->viewport != NULL);

          x = svg_number_get (shape->current[SHAPE_ATTR_X], context->viewport->size.width);
          y = svg_number_get (shape->current[SHAPE_ATTR_Y], context->viewport->size.height);

          if (shape_is_use_target (shape, context))
            {
              Shape *use = context->ctx_shape_stack->data;
              if (_gtk_bitmask_get (use->attrs, SHAPE_ATTR_WIDTH))
                width = svg_number_get (use->current[SHAPE_ATTR_WIDTH], context->viewport->size.width);
              else
                width = svg_number_get (shape->current[SHAPE_ATTR_WIDTH], context->viewport->size.width);
              if (_gtk_bitmask_get (use->attrs, SHAPE_ATTR_HEIGHT))
                height = svg_number_get (use->current[SHAPE_ATTR_HEIGHT], context->viewport->size.height);
              else
                height = svg_number_get (shape->current[SHAPE_ATTR_HEIGHT], context->viewport->size.height);
            }
          else
            {
              width = svg_number_get (shape->current[SHAPE_ATTR_WIDTH], context->viewport->size.width);
              height = svg_number_get (shape->current[SHAPE_ATTR_HEIGHT], context->viewport->size.height);
            }

          w = width;
          h = height;
        }

      if (svg_enum_get (overflow) == OVERFLOW_HIDDEN)
        gtk_snapshot_push_clip (context->snapshot, &GRAPHENE_RECT_INIT (x, y, width, height));

      if (vb->unset)
        graphene_rect_init (&view_box, 0, 0, w, h);
      else
        graphene_rect_init_from_rect (&view_box, &vb->view_box);

      viewport = g_new (graphene_rect_t, 1);
      graphene_rect_init_from_rect (viewport, &view_box);

      compute_viewport_transform (cf->is_none,
                                  cf->align_x,
                                  cf->align_y,
                                  cf->meet,
                                  &view_box,
                                  x, y, width, height,
                                  &sx, &sy, &tx, &ty);

      push_viewport (context, viewport);

      GskTransform *transform = NULL;
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (tx, ty));
      transform = gsk_transform_scale (transform, sx, sy);
      gtk_snapshot_save (context->snapshot);
      gtk_snapshot_transform (context->snapshot, transform);
      push_transform (context, transform);
      gsk_transform_unref (transform);
    }

  if (shape->type != SHAPE_CLIP_PATH &&
      tf->transforms[0].type != TRANSFORM_NONE)
    {
      GskTransform *transform = svg_transform_get_gsk (tf);

      if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_TRANSFORM_ORIGIN))
        {
          SvgNumbers *tfo = (SvgNumbers *) shape->current[SHAPE_ATTR_TRANSFORM_ORIGIN];
          SvgValue *tfb = shape->current[SHAPE_ATTR_TRANSFORM_BOX];
          graphene_rect_t bounds;
          double x, y;

          switch (svg_enum_get (tfb))
            {
            case TRANSFORM_BOX_CONTENT_BOX:
            case TRANSFORM_BOX_FILL_BOX:
              shape_get_current_bounds (shape, context->viewport, context->svg, &bounds);
              break;
            case TRANSFORM_BOX_BORDER_BOX:
            case TRANSFORM_BOX_STROKE_BOX:
              shape_get_current_stroke_bounds (shape, context->viewport, context->svg, &bounds);
              break;
            case TRANSFORM_BOX_VIEW_BOX:
              graphene_rect_init_from_rect (&bounds, context->viewport);
              break;
            default:
              g_assert_not_reached ();
            }

          if (tfo->values[0].unit == SVG_UNIT_PERCENTAGE)
            x = bounds.size.width * tfo->values[0].value / 100;
          else
            x = tfo->values[0].value;

          if (tfo->values[1].unit == SVG_UNIT_PERCENTAGE)
            y = bounds.size.height * tfo->values[1].value / 100;
          else
            y = tfo->values[1].value;

          transform = gsk_transform_translate (
                          gsk_transform_transform (
                              gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (x, y)),
                              transform),
                          &GRAPHENE_POINT_INIT (-x, -y));
        }

      push_transform (context, transform);
      gtk_snapshot_save (context->snapshot);
      gtk_snapshot_transform (context->snapshot, transform);
      gsk_transform_unref (transform);
    }

  if (shape->type == SHAPE_USE)
    {
      double x, y;

      x = svg_number_get (shape->current[SHAPE_ATTR_X], context->viewport->size.width);
      y = svg_number_get (shape->current[SHAPE_ATTR_Y], context->viewport->size.height);
      GskTransform *transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (x, y));
      push_transform (context, transform);
      gtk_snapshot_save (context->snapshot);
      gtk_snapshot_transform (context->snapshot, transform);
      gsk_transform_unref (transform);
    }

  if (context->op != CLIPPING)
    {
      const char *reason;

      if (needs_copy (shape, context, &reason))
        {
#ifdef DEBUG
          if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
            gtk_snapshot_push_debug (context->snapshot, "copy for %s", reason);
#endif
          gtk_snapshot_push_copy (context->snapshot);
        }

      if (needs_isolation (shape, context, &reason))
        {
#ifdef DEBUG
          if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
            gtk_snapshot_push_debug (context->snapshot, "isolate for %s", reason);
#endif
          gtk_snapshot_push_isolation (context->snapshot, GSK_ISOLATION_BACKGROUND);
        }

      if (svg_enum_get (blend) != GSK_BLEND_MODE_DEFAULT)
        {
          graphene_rect_t bounds;

          shape_get_current_bounds (shape, context->viewport, context->svg, &bounds);

          gtk_snapshot_push_blend (context->snapshot, svg_enum_get (blend));
          gtk_snapshot_append_paste (context->snapshot, &bounds, 0);
          gtk_snapshot_pop (context->snapshot);
        }
    }

  if (clip->kind == CLIP_PATH ||
      (clip->kind == CLIP_REF && clip->ref.shape != NULL))
    {
      push_op (context, CLIPPING);

      /* Clip mask - see language in the spec about 'raw geometry' */
      if (clip->kind == CLIP_PATH)
        {
          GskFillRule rule;

          switch (clip->path.fill_rule)
            {
            case GSK_FILL_RULE_WINDING:
            case GSK_FILL_RULE_EVEN_ODD:
              rule = clip->path.fill_rule;
              break;
            default:
              rule = svg_enum_get (fill_rule);
              break;
            }

#ifdef DEBUG
          if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
            gtk_snapshot_push_debug (context->snapshot, "fill or clip for clip");
#endif
          snapshot_push_fill (context->snapshot, clip->path.path, rule);
        }
      else
        {
          /* In the general case, we collect the clip geometry in a mask.
           * We special-case a single shape in the <clipPath> without
           * transforms and translate them to a clip or a fill.
           */

          SvgTransform *ctf = (SvgTransform *) clip->ref.shape->current[SHAPE_ATTR_TRANSFORM];

          Shape *child = NULL;

          if (clip->ref.shape->shapes->len > 0)
            child = g_ptr_array_index (clip->ref.shape->shapes, 0);

          if (ctf->transforms[0].type == TRANSFORM_NONE &&
              svg_enum_get (clip->ref.shape->current[SHAPE_ATTR_CONTENT_UNITS]) == COORD_UNITS_USER_SPACE_ON_USE &&
              clip->ref.shape->shapes->len == 1 &&
              (child->type == SHAPE_PATH || child->type == SHAPE_RECT || child->type == SHAPE_CIRCLE) &&
              svg_enum_get (child->current[SHAPE_ATTR_VISIBILITY]) != VISIBILITY_HIDDEN &&
              svg_enum_get (child->current[SHAPE_ATTR_DISPLAY]) != DISPLAY_NONE &&
              ((SvgClip *) child->current[SHAPE_ATTR_CLIP_PATH])->kind == CLIP_NONE &&
              ((SvgTransform *) child->current[SHAPE_ATTR_TRANSFORM])->transforms[0].type == TRANSFORM_NONE)
            {
              GskPath *path;

              path = shape_get_current_path (child, context->viewport);
#ifdef DEBUG
              if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
                gtk_snapshot_push_debug (context->snapshot, "fill or clip for clip");
#endif
              snapshot_push_fill (context->snapshot, path, GSK_FILL_RULE_WINDING);
              gsk_path_unref (path);
            }
          else
            {
#ifdef DEBUG
              if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
                gtk_snapshot_push_debug (context->snapshot, "mask for clip");
#endif
              gtk_snapshot_push_mask (context->snapshot, GSK_MASK_MODE_ALPHA);

              if (ctf->transforms[0].type != TRANSFORM_NONE)
                {
                  GskTransform *transform = svg_transform_get_gsk (ctf);

                  if (_gtk_bitmask_get (clip->ref.shape->attrs, SHAPE_ATTR_TRANSFORM_ORIGIN))
                    {
                      SvgNumbers *tfo = (SvgNumbers *) clip->ref.shape->current[SHAPE_ATTR_TRANSFORM_ORIGIN];
                      SvgValue *tfb = clip->ref.shape->current[SHAPE_ATTR_TRANSFORM_BOX];
                      graphene_rect_t bounds;
                      double x, y;

                      switch (svg_enum_get (tfb))
                        {
                        case TRANSFORM_BOX_CONTENT_BOX:
                        case TRANSFORM_BOX_FILL_BOX:
                          shape_get_current_bounds (shape, context->viewport, context->svg, &bounds);
                          break;
                        case TRANSFORM_BOX_BORDER_BOX:
                        case TRANSFORM_BOX_STROKE_BOX:
                          shape_get_current_stroke_bounds (shape, context->viewport, context->svg, &bounds);
                          break;
                        case TRANSFORM_BOX_VIEW_BOX:
                          graphene_rect_init_from_rect (&bounds, context->viewport);
                          break;
                        default:
                          g_assert_not_reached ();
                        }

                      if (tfo->values[0].unit == SVG_UNIT_PERCENTAGE)
                        x = bounds.size.width * tfo->values[0].value / 100;
                      else
                        x = tfo->values[0].value;

                      if (tfo->values[1].unit == SVG_UNIT_PERCENTAGE)
                        y = bounds.size.height * tfo->values[1].value / 100;
                      else
                        y = tfo->values[1].value;

                      transform = gsk_transform_translate (
                                      gsk_transform_transform (
                                          gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (x, y)),
                                          transform),
                                      &GRAPHENE_POINT_INIT (-x, -y));
                    }

                  push_transform (context, transform);
                  gtk_snapshot_save (context->snapshot);
                  gtk_snapshot_transform (context->snapshot, transform);
                  gsk_transform_unref (transform);
                }

              if (svg_enum_get (clip->ref.shape->current[SHAPE_ATTR_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
                {
                  graphene_rect_t bounds;

                  GskTransform *transform = NULL;

                  gtk_snapshot_save (context->snapshot);

                  if (shape_get_current_bounds (shape, context->viewport, context->svg, &bounds))
                    {
                      transform = gsk_transform_translate (NULL, &bounds.origin);
                      transform = gsk_transform_scale (transform, bounds.size.width, bounds.size.height);
                      gtk_snapshot_transform (context->snapshot, transform);
                    }
                  push_transform (context, transform);
                  gsk_transform_unref (transform);
                }

              render_shape (clip->ref.shape, context);

              if (svg_enum_get (clip->ref.shape->current[SHAPE_ATTR_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
                {
                  pop_transform (context);
                  gtk_snapshot_restore (context->snapshot);
                }

              if (ctf->transforms[0].type != TRANSFORM_NONE)
                {
                  pop_transform (context);
                  gtk_snapshot_restore (context->snapshot);
                }

              gtk_snapshot_pop (context->snapshot); /* mask */
            }
        }

      pop_op (context);
    }

  if (mask->kind != MASK_NONE && mask->shape != NULL &&
      context->op != CLIPPING)
    {
      gboolean has_clip = FALSE;

      push_op (context, MASKING);

#ifdef DEBUG
      if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
        gtk_snapshot_push_debug (context->snapshot, "mask for masking");
#endif
      gtk_snapshot_push_mask (context->snapshot, svg_enum_get (mask->shape->current[SHAPE_ATTR_MASK_TYPE]));

      if (_gtk_bitmask_get (mask->shape->attrs, SHAPE_ATTR_X) ||
          _gtk_bitmask_get (mask->shape->attrs, SHAPE_ATTR_Y) ||
          _gtk_bitmask_get (mask->shape->attrs, SHAPE_ATTR_WIDTH) ||
          _gtk_bitmask_get (mask->shape->attrs, SHAPE_ATTR_HEIGHT))
        {
           graphene_rect_t mask_clip;

          if (svg_enum_get (mask->shape->current[SHAPE_ATTR_BOUND_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
            {
              graphene_rect_t bounds;

              if (shape_get_current_bounds (shape, context->viewport, context->svg, &bounds))
                {
                  mask_clip.origin.x = bounds.origin.x + svg_number_get (mask->shape->current[SHAPE_ATTR_X], 1) * bounds.size.width;
                  mask_clip.origin.y = bounds.origin.y + svg_number_get (mask->shape->current[SHAPE_ATTR_Y], 1) * bounds.size.height;
                  mask_clip.size.width = svg_number_get (mask->shape->current[SHAPE_ATTR_WIDTH], 1) * bounds.size.width;
                  mask_clip.size.height = svg_number_get (mask->shape->current[SHAPE_ATTR_HEIGHT], 1) * bounds.size.height;
                  has_clip = TRUE;
                }
            }
          else
            {
              mask_clip.origin.x = svg_number_get (mask->shape->current[SHAPE_ATTR_X], context->viewport->size.width);
              mask_clip.origin.y = svg_number_get (mask->shape->current[SHAPE_ATTR_Y], context->viewport->size.height);
              mask_clip.size.width = svg_number_get (mask->shape->current[SHAPE_ATTR_WIDTH], context->viewport->size.width);
              mask_clip.size.height = svg_number_get (mask->shape->current[SHAPE_ATTR_HEIGHT], context->viewport->size.height);
              has_clip = TRUE;
            }

          if (has_clip)
            gtk_snapshot_push_clip (context->snapshot, &mask_clip);
        }

      if (svg_enum_get (mask->shape->current[SHAPE_ATTR_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
        {
          graphene_rect_t bounds;
          GskTransform *transform = NULL;

          gtk_snapshot_save (context->snapshot);

          if (shape_get_current_bounds (shape, context->viewport, context->svg, &bounds))
            {
              transform = gsk_transform_translate (NULL, &bounds.origin);
              transform = gsk_transform_scale (transform, bounds.size.width, bounds.size.height);
              gtk_snapshot_transform (context->snapshot, transform);
            }

          push_transform (context, transform);
          gsk_transform_unref (transform);
        }

      render_shape (mask->shape, context);

      if (svg_enum_get (mask->shape->current[SHAPE_ATTR_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
        {
          pop_transform (context);
          gtk_snapshot_restore (context->snapshot);
        }

      if (has_clip)
        gtk_snapshot_pop (context->snapshot);

      gtk_snapshot_pop (context->snapshot);

      pop_op (context);
    }

  if (context->op != CLIPPING && shape->type != SHAPE_MASK)
    {
      if (svg_number_get (opacity, 1) != 1)
        gtk_snapshot_push_opacity (context->snapshot, svg_number_get (opacity, 1));

      if (!svg_filter_is_none (filter))
        gtk_snapshot_push_collect (context->snapshot);
    }
}

static void
pop_group (Shape        *shape,
           PaintContext *context)
{
  SvgValue *filter = shape->current[SHAPE_ATTR_FILTER];
  SvgValue *opacity = shape->current[SHAPE_ATTR_OPACITY];
  SvgClip *clip = (SvgClip *) shape->current[SHAPE_ATTR_CLIP_PATH];
  SvgMask *mask = (SvgMask *) shape->current[SHAPE_ATTR_MASK];
  SvgTransform *tf = (SvgTransform *) shape->current[SHAPE_ATTR_TRANSFORM];
  SvgValue *blend = shape->current[SHAPE_ATTR_BLEND_MODE];

  if (context->op != CLIPPING && shape->type != SHAPE_MASK)
    {
      if (!svg_filter_is_none (filter))
        {
          GskRenderNode *node, *node2;

          node = gtk_snapshot_pop_collect (context->snapshot);

          if (!node)
            node = empty_node ();

          node2 = apply_filter_functions (filter, context, shape, node);

          gtk_snapshot_append_node (context->snapshot, node2);
          gsk_render_node_unref (node2);
          gsk_render_node_unref (node);
        }

      if (svg_number_get (opacity, 1) != 1)
        gtk_snapshot_pop (context->snapshot);
    }

  if (mask->kind != MASK_NONE && mask->shape != NULL &&
      context->op != CLIPPING)
    {
      gtk_snapshot_pop (context->snapshot);
#ifdef DEBUG
      if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
        gtk_snapshot_pop (context->snapshot);
#endif
    }

  if (clip->kind == CLIP_PATH ||
      (clip->kind == CLIP_REF && clip->ref.shape != NULL))
    {
      gtk_snapshot_pop (context->snapshot);
#ifdef DEBUG
      if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
        gtk_snapshot_pop (context->snapshot);
#endif
    }

  if (context->op != CLIPPING)
    {
      if (svg_enum_get (blend) != GSK_BLEND_MODE_DEFAULT)
        {
          gtk_snapshot_pop (context->snapshot);
        }

      if (needs_isolation (shape, context, NULL))
        {
          gtk_snapshot_pop (context->snapshot);
#ifdef DEBUG
          if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
            gtk_snapshot_pop (context->snapshot);
#endif
        }

      if (needs_copy (shape, context, NULL))
        {
          gtk_snapshot_pop (context->snapshot);
#ifdef DEBUG
          if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
            gtk_snapshot_pop (context->snapshot);
#endif
        }
    }

  if (shape->type == SHAPE_USE)
    {
      pop_transform (context);
      gtk_snapshot_restore (context->snapshot);
    }

  if (shape->type != SHAPE_CLIP_PATH &&
      tf->transforms[0].type != TRANSFORM_NONE)
    {
      pop_transform (context);
      gtk_snapshot_restore (context->snapshot);
    }

  if (shape->type == SHAPE_SVG || shape->type == SHAPE_SYMBOL)
    {
      SvgValue *overflow = shape->current[SHAPE_ATTR_OVERFLOW];

      g_free ((gpointer) pop_viewport (context));

      pop_transform (context);
      gtk_snapshot_restore (context->snapshot);

      if (svg_enum_get (overflow) == OVERFLOW_HIDDEN)
        gtk_snapshot_pop (context->snapshot);
   }

#ifdef DEBUG
  if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
    gtk_snapshot_pop (context->snapshot);
#endif
}

/* }}} */
/* {{{ Paint servers */

static gboolean
type_is_gradient (ShapeType type)
{
  return shape_type_has_color_stops (type);
}

static gboolean
template_type_compatible (ShapeType type1,
                          ShapeType type2)
{
  return type1 == type2 ||
         (type_is_gradient (type1) && type_is_gradient (type2));
}

/* Only return explicitly set values from a template.
 * If we don't have one, we use the initial value for
 * the original paint server.
 */
static SvgValue *
paint_server_get_template_value (Shape        *shape,
                                 ShapeAttr     attr,
                                 PaintContext *context)
{
  if (!_gtk_bitmask_get (shape->attrs, attr))
    {
      SvgHref *href = (SvgHref *) shape->current[SHAPE_ATTR_HREF];

      if (context->depth > NESTING_LIMIT)
        {
          gtk_svg_rendering_error (context->svg,
                                   "excessive rendering depth (> %d) while resolving href %s, aborting",
                                   NESTING_LIMIT,
                                   href->ref);
          goto fail;
        }

      if (href->shape)
        {
          if (template_type_compatible (href->shape->type, shape->type))
            {
              SvgValue *ret;

              context->depth++;
              ret = paint_server_get_template_value (href->shape, attr, context);
              context->depth--;

              return ret;
            }

          gtk_svg_invalid_reference (context->svg,
                                     "<%s> can not use a <%s> as template (while resolving href %s)",
                                     shape_types[shape->type].name,
                                     shape_types[href->shape->type].name,
                                     href->ref);
        }

      return NULL;
    }

fail:
  return shape->current[attr];
}

static SvgValue *
paint_server_get_current_value (Shape        *shape,
                                ShapeAttr     attr,
                                PaintContext *context)
{
  SvgValue *value = NULL;

  if (_gtk_bitmask_get (shape->attrs, attr))
    return shape->current[attr];

  value = paint_server_get_template_value (shape, attr, context);
  if (value)
    return value;

  return shape->current[attr];
}

static GPtrArray *
gradient_get_color_stops (Shape        *shape,
                          PaintContext *context)
{
  if (shape->color_stops->len == 0)
    {
      SvgHref *href = (SvgHref *) shape->current[SHAPE_ATTR_HREF];

      if (context->depth > NESTING_LIMIT)
        {
          gtk_svg_rendering_error (context->svg,
                                   "excessive rendering depth (> %d) while resolving href %s, aborting",
                                   NESTING_LIMIT,
                                   href->ref);
          goto fail;
        }

      if (href->shape)
        {
          if (template_type_compatible (href->shape->type, shape->type))
            {
              GPtrArray *ret;
              context->depth++;
              ret = gradient_get_color_stops (href->shape, context);
              context->depth--;
              return ret;
            }

          gtk_svg_invalid_reference (context->svg,
                                     "<%s> can not use a <%s> as template (while collecting color stops)",
                                     shape_types[shape->type].name,
                                     shape_types[href->shape->type].name);
        }
    }

fail:
  return shape->color_stops;
}

static GskGradient *
gradient_get_gsk_gradient (Shape        *gradient,
                           PaintContext *context)
{
  GskGradient *g;
  GPtrArray *color_stops;
  double offset;
  SvgValue *spread_method;
  SvgValue *color_interpolation;

  color_stops = gradient_get_color_stops (gradient, context);

  g = gsk_gradient_new ();
  offset = 0;
  for (unsigned int i = 0; i < color_stops->len; i++)
    {
      ColorStop *cs = g_ptr_array_index (color_stops, i);
      SvgColor *stop_color = (SvgColor *) cs->current[color_stop_attr_idx (SHAPE_ATTR_STOP_COLOR)];
      GdkColor color;

      offset = MAX (svg_number_get (cs->current[color_stop_attr_idx (SHAPE_ATTR_STOP_OFFSET)], 1), offset);
      gdk_color_init_copy (&color, &stop_color->color);
      color.alpha *= svg_number_get (cs->current[color_stop_attr_idx (SHAPE_ATTR_STOP_OPACITY)], 1);
      gsk_gradient_add_stop (g, offset, 0.5, &color);
      gdk_color_finish (&color);
    }

  spread_method = paint_server_get_current_value (gradient, SHAPE_ATTR_SPREAD_METHOD, context);
  gsk_gradient_set_repeat (g, svg_enum_get (spread_method));
  gsk_gradient_set_premultiplied (g, FALSE);

  color_interpolation = paint_server_get_current_value (gradient, SHAPE_ATTR_COLOR_INTERPOLATION, context);
  if (svg_enum_get (color_interpolation) == COLOR_INTERPOLATION_LINEAR)
    gsk_gradient_set_interpolation (g, GDK_COLOR_STATE_SRGB_LINEAR);
  else
    gsk_gradient_set_interpolation (g, GDK_COLOR_STATE_SRGB);

  return g;
}

static GPtrArray *
pattern_get_shapes (Shape        *shape,
                    PaintContext *context)
{
  if (shape->shapes->len == 0)
    {
      SvgHref *href = (SvgHref *) shape->current[SHAPE_ATTR_HREF];

      if (context->depth > NESTING_LIMIT)
        {
          gtk_svg_rendering_error (context->svg,
                                   "excessive rendering depth (> %d) while resolving href %s, aborting",
                                   NESTING_LIMIT,
                                   href->ref);

          goto fail;
        }

      if (href->shape)
        {
          if (template_type_compatible (href->shape->type, shape->type))
            {
              GPtrArray *ret;
              context->depth++;
              ret = pattern_get_shapes (href->shape, context);
              context->depth--;
              return ret;
            }

          gtk_svg_invalid_reference (context->svg,
                                     "<%s> can not use a <%s> as template (while collecting pattern content)",
                                     shape_types[shape->type].name,
                                     shape_types[href->shape->type].name);
        }
    }

fail:
  return shape->shapes;
}

static gboolean
paint_server_is_skipped (Shape                 *server,
                         const graphene_rect_t *bounds,
                         PaintContext          *context)
{
  SvgValue *units;

  if (server->type == SHAPE_PATTERN)
    {
      SvgValue *width = paint_server_get_current_value (server, SHAPE_ATTR_WIDTH, context);
      SvgValue *height = paint_server_get_current_value (server, SHAPE_ATTR_HEIGHT, context);

      if (svg_number_get (width, 1) == 0 || svg_number_get (height, 1) == 0)
        return TRUE;

      units = paint_server_get_current_value (server, SHAPE_ATTR_BOUND_UNITS, context);
    }
  else
    units = paint_server_get_current_value (server, SHAPE_ATTR_CONTENT_UNITS, context);

  if (svg_enum_get (units) == COORD_UNITS_OBJECT_BOUNDING_BOX)
    return bounds->size.width == 0 || bounds->size.height == 0;
  else
    return FALSE;
}

static void
paint_linear_gradient (Shape                 *gradient,
                       const graphene_rect_t *bounds,
                       const graphene_rect_t *paint_bounds,
                       PaintContext          *context)
{
  graphene_point_t start, end;
  GskTransform *transform, *gradient_transform;
  GskGradient *g;
  SvgValue *x1 = paint_server_get_current_value (gradient, SHAPE_ATTR_X1, context);
  SvgValue *y1 = paint_server_get_current_value (gradient, SHAPE_ATTR_Y1, context);
  SvgValue *x2 = paint_server_get_current_value (gradient, SHAPE_ATTR_X2, context);
  SvgValue *y2 = paint_server_get_current_value (gradient, SHAPE_ATTR_Y2, context);
  SvgValue *tf = paint_server_get_current_value (gradient, SHAPE_ATTR_TRANSFORM, context);
  SvgValue *units = paint_server_get_current_value (gradient, SHAPE_ATTR_CONTENT_UNITS, context);

  g = gradient_get_gsk_gradient (gradient, context);

  transform = NULL;
  if (svg_enum_get (units) == COORD_UNITS_OBJECT_BOUNDING_BOX)
    {
      transform = gsk_transform_translate (transform, &bounds->origin);
      transform = gsk_transform_scale (transform, bounds->size.width, bounds->size.height);
      graphene_point_init (&start,
                           svg_number_get (x1, 1),
                           svg_number_get (y1, 1));
      graphene_point_init (&end,
                           svg_number_get (x2, 1),
                           svg_number_get (y2, 1));
    }
  else
    {
      graphene_point_init (&start,
                           svg_number_get (x1, context->viewport->size.width),
                           svg_number_get (y1, context->viewport->size.height));
      graphene_point_init (&end,
                           svg_number_get (x2, context->viewport->size.width),
                           svg_number_get (y2, context->viewport->size.height));
    }

  gradient_transform = svg_transform_get_gsk ((SvgTransform *) tf);
  transform = gsk_transform_transform (transform, gradient_transform);
  gsk_transform_unref (gradient_transform);
  transform_gradient_line (transform, &start, &end, &start, &end);
  gsk_transform_unref (transform);

  gtk_snapshot_add_linear_gradient (context->snapshot, paint_bounds, &start, &end, g);

  gsk_gradient_free (g);
}

static void
paint_radial_gradient (Shape                 *gradient,
                       const graphene_rect_t *bounds,
                       const graphene_rect_t *paint_bounds,
                       PaintContext          *context)
{
  graphene_point_t start_center;
  graphene_point_t end_center;
  double start_radius, end_radius;
  GskTransform *gradient_transform;
  graphene_rect_t gradient_bounds;
  GskGradient *g;
  SvgValue *fx = paint_server_get_current_value (gradient, SHAPE_ATTR_FX, context);
  SvgValue *fy = paint_server_get_current_value (gradient, SHAPE_ATTR_FY, context);
  SvgValue *fr = paint_server_get_current_value (gradient, SHAPE_ATTR_FR, context);
  SvgValue *cx = paint_server_get_current_value (gradient, SHAPE_ATTR_CX, context);
  SvgValue *cy = paint_server_get_current_value (gradient, SHAPE_ATTR_CY, context);
  SvgValue *r = paint_server_get_current_value (gradient, SHAPE_ATTR_R, context);
  SvgValue *tf = paint_server_get_current_value (gradient, SHAPE_ATTR_TRANSFORM, context);
  SvgValue *units = paint_server_get_current_value (gradient, SHAPE_ATTR_CONTENT_UNITS, context);

  g = gradient_get_gsk_gradient (gradient, context);

  gtk_snapshot_save (context->snapshot);

  if (svg_enum_get (units) == COORD_UNITS_OBJECT_BOUNDING_BOX)
    {
      GskTransform *transform = NULL;

      graphene_point_init (&start_center,
                           svg_number_get (fx, 1),
                           svg_number_get (fy, 1));
      start_radius = svg_number_get (fr, 1);

      graphene_point_init (&end_center,
                           svg_number_get (cx, 1),
                           svg_number_get (cy, 1));
      end_radius = svg_number_get (r, 1);

      transform = gsk_transform_translate (transform, &bounds->origin);
      transform = gsk_transform_scale (transform, bounds->size.width, bounds->size.height);
      gtk_snapshot_transform (context->snapshot, transform);
      push_transform (context, transform);

      transform = gsk_transform_invert (transform);
      gsk_transform_transform_bounds (transform, paint_bounds, &gradient_bounds);
      gsk_transform_unref (transform);
    }
  else
    {
      graphene_point_init (&start_center,
                           svg_number_get (fx, context->viewport->size.width),
                           svg_number_get (fy, context->viewport->size.height));
      start_radius = svg_number_get (fr, normalized_diagonal (context->viewport));

      graphene_point_init (&end_center,
                           svg_number_get (cx, context->viewport->size.width),
                           svg_number_get (cy, context->viewport->size.height));
      end_radius = svg_number_get (r, normalized_diagonal (context->viewport));
      graphene_rect_init_from_rect (&gradient_bounds, paint_bounds);
      push_transform (context, NULL);
    }

  gradient_transform = svg_transform_get_gsk ((SvgTransform *) tf);

  if (_gtk_bitmask_get (gradient->attrs, SHAPE_ATTR_TRANSFORM_ORIGIN))
    {
      SvgNumbers *tfo = (SvgNumbers *) gradient->current[SHAPE_ATTR_TRANSFORM_ORIGIN];
      double x, y;

      if (tfo->values[0].unit == SVG_UNIT_PERCENTAGE)
        x = bounds->origin.x + bounds->size.width * tfo->values[0].value / 100;
      else
        x = tfo->values[0].value;

      if (tfo->values[1].unit == SVG_UNIT_PERCENTAGE)
        y = bounds->origin.y + bounds->size.height * tfo->values[1].value / 100;
      else
        y = tfo->values[1].value;

      gradient_transform = gsk_transform_translate (
                      gsk_transform_transform (
                          gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (x, y)),
                          gradient_transform),
                      &GRAPHENE_POINT_INIT (-x, -y));
    }

  gtk_snapshot_transform (context->snapshot, gradient_transform);
  push_transform (context, gradient_transform);

  if (gradient_transform)
    {
      gradient_transform = gsk_transform_invert (gradient_transform);
      if (gradient_transform)
        {
          gsk_transform_transform_bounds (gradient_transform, &gradient_bounds, &gradient_bounds);
          gsk_transform_unref (gradient_transform);
        }
      else
        graphene_rect_init (&gradient_bounds, 0, 0, 0, 0);
    }

  /* If the gradient transform is singular, we might end up with
   * nans or infs in the bounds :(
   */
  if (!isnormal (gradient_bounds.size.width) ||
      !isnormal (gradient_bounds.size.height))
    {
      GskRenderNode *node = gsk_container_node_new (NULL, 0);
      gtk_snapshot_append_node (context->snapshot, node);
      gsk_render_node_unref (node);
    }
  else
    {
      gtk_snapshot_add_radial_gradient (context->snapshot,
                                        &gradient_bounds,
                                        &start_center, start_radius,
                                        &end_center, end_radius,
                                        1,
                                        g);
    }

  gsk_gradient_free (g);

  pop_transform (context);
  pop_transform (context);
  gtk_snapshot_restore (context->snapshot);
}

static void
paint_pattern (Shape                 *pattern,
               const graphene_rect_t *bounds,
               const graphene_rect_t *paint_bounds,
               PaintContext          *context)
{
  graphene_rect_t pattern_bounds, child_bounds;
  double tx, ty;
  double sx, sy;
  GskTransform *transform;
  double dx, dy;
  SvgValue *bound_units = paint_server_get_current_value (pattern, SHAPE_ATTR_BOUND_UNITS, context);
  SvgValue *content_units = paint_server_get_current_value (pattern, SHAPE_ATTR_CONTENT_UNITS, context);
  SvgValue *x = paint_server_get_current_value (pattern, SHAPE_ATTR_X, context);
  SvgValue *y = paint_server_get_current_value (pattern, SHAPE_ATTR_Y, context);
  SvgValue *width = paint_server_get_current_value (pattern, SHAPE_ATTR_WIDTH, context);
  SvgValue *height = paint_server_get_current_value (pattern, SHAPE_ATTR_HEIGHT, context);
  SvgValue *tf = paint_server_get_current_value (pattern, SHAPE_ATTR_TRANSFORM, context);
  SvgViewBox *vb = (SvgViewBox *) paint_server_get_current_value (pattern, SHAPE_ATTR_VIEW_BOX, context);
  SvgContentFit *cf = (SvgContentFit *) paint_server_get_current_value (pattern, SHAPE_ATTR_CONTENT_FIT, context);
  GPtrArray *shapes;

  if (svg_enum_get (bound_units) == COORD_UNITS_OBJECT_BOUNDING_BOX)
    {
      dx = bounds->origin.x + svg_number_get (x, 1) * bounds->size.width;
      dy = bounds->origin.y + svg_number_get (y, 1) * bounds->size.height;
      child_bounds.origin.x = 0;
      child_bounds.origin.y = 0;
      child_bounds.size.width = svg_number_get (width, 1) * bounds->size.width;
      child_bounds.size.height = svg_number_get (height, 1) * bounds->size.height;
    }
  else
    {
      dx = svg_number_get (x, context->viewport->size.width);
      dy = svg_number_get (y, context->viewport->size.height);
      child_bounds.origin.x = 0;
      child_bounds.origin.y = 0;
      child_bounds.size.width = svg_number_get (width, context->viewport->size.width);
      child_bounds.size.height = svg_number_get (height, context->viewport->size.height);
    }

  if (!vb->unset)
    {
      compute_viewport_transform (cf->is_none,
                                  cf->align_x,
                                  cf->align_y,
                                  cf->meet,
                                  &vb->view_box,
                                  child_bounds.origin.x, child_bounds.origin.y,
                                  child_bounds.size.width, child_bounds.size.height,
                                  &sx, &sy, &tx, &ty);
    }
  else
    {
      if (svg_enum_get (content_units) == COORD_UNITS_OBJECT_BOUNDING_BOX)
        {
          tx = 0;
          ty = 0;
          sx = bounds->size.width;
          sy = bounds->size.height;
        }
      else
        {
          sx = sy = 1;
          tx = ty = 0;
        }
    }

  child_bounds.origin.x += dx;
  child_bounds.origin.y += dy;

  tx += dx;
  ty += dy;

  gtk_snapshot_save (context->snapshot);

  transform = svg_transform_get_gsk ((SvgTransform *) tf);

  if (_gtk_bitmask_get (pattern->attrs, SHAPE_ATTR_TRANSFORM_ORIGIN))
    {
      SvgNumbers *tfo = (SvgNumbers *) pattern->current[SHAPE_ATTR_TRANSFORM_ORIGIN];
      double xx, yy;

      if (tfo->values[0].unit == SVG_UNIT_PERCENTAGE)
        xx = child_bounds.size.width * tfo->values[0].value / 100;
      else
        xx = tfo->values[0].value;

      if (tfo->values[1].unit == SVG_UNIT_PERCENTAGE)
        yy = child_bounds.size.height * tfo->values[1].value / 100;
      else
        yy = tfo->values[1].value;

      transform = gsk_transform_translate (
                      gsk_transform_transform (
                          gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (xx, yy)),
                          transform),
                      &GRAPHENE_POINT_INIT (-xx, -yy));
     }

  gtk_snapshot_transform (context->snapshot, transform);
  push_transform (context, transform);

  transform = gsk_transform_invert (transform);
  gsk_transform_transform_bounds (transform, paint_bounds, &pattern_bounds);
  gsk_transform_unref (transform);

  /* If the gradient transform is singular, we might end up with
   * nans or infs in the bounds :(
   */
  if (!isfinite (pattern_bounds.size.width) || pattern_bounds.size.width == 0 ||
      !isfinite (pattern_bounds.size.height) || pattern_bounds.size.height == 0)
    {
      GskRenderNode *node = gsk_container_node_new (NULL, 0);
      gtk_snapshot_append_node (context->snapshot, node);
      gsk_render_node_unref (node);
    }
  else
    {
      gtk_snapshot_push_repeat (context->snapshot, &pattern_bounds, &child_bounds);

      gtk_snapshot_save (context->snapshot);
      transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (tx, ty));
      transform = gsk_transform_scale (transform, sx, sy);
      gtk_snapshot_transform (context->snapshot, transform);
      push_transform (context, transform);
      gsk_transform_unref (transform);

      shapes = pattern_get_shapes (pattern, context);
      for (int i = 0; i < shapes->len; i++)
        {
          Shape *s = g_ptr_array_index (shapes, i);
          render_shape (s, context);
        }

      pop_transform (context);
      gtk_snapshot_restore (context->snapshot);

      gtk_snapshot_pop (context->snapshot);
    }

  pop_transform (context);
  gtk_snapshot_restore (context->snapshot);
}

static void
paint_server (SvgPaint              *paint,
              const graphene_rect_t *bounds,
              const graphene_rect_t *paint_bounds,
              PaintContext          *context)
{
  Shape *server = paint->server.shape;

  if (server == NULL ||
      paint_server_is_skipped (server, bounds, context))
    {
      gtk_snapshot_add_color (context->snapshot,
                              &paint->server.fallback,
                              paint_bounds);
    }
  else if (server->type == SHAPE_LINEAR_GRADIENT ||
           server->type == SHAPE_RADIAL_GRADIENT)
    {
      GPtrArray *color_stops;

      color_stops = gradient_get_color_stops (server, context);

      if (color_stops->len == 0)
        return;

      if (color_stops->len == 1)
        {
          ColorStop *cs = g_ptr_array_index (color_stops, 0);
          SvgColor *stop_color = (SvgColor *) cs->current[color_stop_attr_idx (SHAPE_ATTR_STOP_COLOR)];
          GdkColor c;

          gdk_color_init_copy (&c, &stop_color->color);
          c.alpha *= svg_number_get (cs->current[color_stop_attr_idx (SHAPE_ATTR_STOP_OPACITY)], 1);
          gtk_snapshot_add_color (context->snapshot, &c, paint_bounds);
          gdk_color_finish (&c);
          return;
        }

      if (server->type == SHAPE_LINEAR_GRADIENT)
        paint_linear_gradient (server, bounds, paint_bounds, context);
      else
        paint_radial_gradient (server, bounds, paint_bounds, context);
    }
  else if (server->type == SHAPE_PATTERN)
    {
      paint_pattern (server, bounds, paint_bounds, context);
    }
}

/* }}} */
/* {{{ Shapes */

static GskStroke *
shape_create_basic_stroke (Shape                 *shape,
                           const graphene_rect_t *viewport,
                           gboolean               allow_gpa,
                           double                 weight)
{
  GskStroke *stroke;
  double width, min, max;

  width = svg_number_get (shape->current[SHAPE_ATTR_STROKE_WIDTH], normalized_diagonal (viewport));
  min = svg_number_get (shape->current[SHAPE_ATTR_STROKE_MINWIDTH], width);
  max = svg_number_get (shape->current[SHAPE_ATTR_STROKE_MAXWIDTH], width);

  if (allow_gpa)
    width = width_apply_weight (width, min, max, weight);

  stroke = gsk_stroke_new (width);

  gsk_stroke_set_line_cap (stroke, svg_enum_get (shape->current[SHAPE_ATTR_STROKE_LINECAP]));
  gsk_stroke_set_line_join (stroke, svg_enum_get (shape->current[SHAPE_ATTR_STROKE_LINEJOIN]));
  gsk_stroke_set_miter_limit (stroke, svg_number_get (shape->current[SHAPE_ATTR_STROKE_MITERLIMIT], 1));

  return stroke;
}

static GskStroke *
shape_create_stroke (Shape        *shape,
                     PaintContext *context)
{
  GskStroke *stroke;
  SvgDashArray *dasharray;

  stroke = shape_create_basic_stroke (shape, context->viewport, context->svg->features & GTK_SVG_EXTENSIONS, context->weight);

  dasharray = (SvgDashArray *) shape->current[SHAPE_ATTR_STROKE_DASHARRAY];
  if (dasharray->kind != DASH_ARRAY_NONE)
    {
      Number *dashes = dasharray->dashes;
      unsigned int len = dasharray->n_dashes;
      double path_length;
      double length;
      double offset;
      GskPathMeasure *measure;
      gboolean invalid = FALSE;

      if (shape_type_has_text (shape->type))
        {
          gtk_svg_rendering_error (context->svg,
                                   "Dashing of stroked text is not supported");
          return stroke;
        }

      measure = shape_get_current_measure (shape, context->viewport);
      length = gsk_path_measure_get_length (measure);
      gsk_path_measure_unref (measure);

      path_length = svg_number_get (shape->current[SHAPE_ATTR_PATH_LENGTH], 1);
      if (path_length < 0)
        path_length = length;

      offset = svg_number_get (shape->current[SHAPE_ATTR_STROKE_DASHOFFSET], normalized_diagonal (context->viewport));

      float *vals = g_newa (float, len);

      if (path_length > 0)
        {
          for (unsigned int i = 0; i < len; i++)
            {
              g_assert (dashes[i].unit != SVG_UNIT_PERCENTAGE);
              vals[i] = dashes[i].value / path_length * length;
              if (vals[i] < 0)
                invalid = TRUE;
            }

          offset = offset / path_length * length;
        }
      else
        {
          for (unsigned int i = 0; i < len; i++)
            {
              g_assert (dashes[i].unit != SVG_UNIT_PERCENTAGE);
              vals[i] = dashes[i].value;
              if (vals[i] < 0)
                invalid = TRUE;
            }
        }

      if (!invalid)
        {
          gsk_stroke_set_dash (stroke, vals, len);
          gsk_stroke_set_dash_offset (stroke, offset);
        }
    }

  return stroke;
}

static SvgPaint *
get_context_paint (SvgPaint *paint,
                   GSList   *ctx_stack)
{
  switch (paint->kind)
    {
    case PAINT_NONE:
    case PAINT_CURRENT_COLOR:
      break;
    case PAINT_COLOR:
    case PAINT_SERVER:
    case PAINT_SERVER_WITH_FALLBACK:
    case PAINT_SERVER_WITH_CURRENT_COLOR:
    case PAINT_SYMBOLIC:
      return paint;
    case PAINT_CONTEXT_FILL:
    case PAINT_CONTEXT_STROKE:
      if (ctx_stack)
        {
          Shape *shape = ctx_stack->data;

          if (paint->kind == PAINT_CONTEXT_FILL)
            paint = (SvgPaint *) shape->current[SHAPE_ATTR_FILL];
          else
            paint = (SvgPaint *) shape->current[SHAPE_ATTR_STROKE];

          return get_context_paint (paint, ctx_stack->next);
        }
      break;
    default:
      g_assert_not_reached ();
    }

  paint = (SvgPaint *) svg_paint_new_none ();
  svg_value_unref ((SvgValue *) paint);

  return paint;
}

static void
fill_shape (Shape        *shape,
            GskPath      *path,
            SvgPaint     *paint,
            double        opacity,
            PaintContext *context)
{
  graphene_rect_t bounds;
  GskFillRule fill_rule;

  paint = get_context_paint (paint, context->ctx_shape_stack);
  if (paint->kind == PAINT_NONE)
    return;

  if (!gsk_path_get_bounds (path, &bounds))
    return;

  fill_rule = svg_enum_get (shape->current[SHAPE_ATTR_FILL_RULE]);

  switch (paint->kind)
    {
    case PAINT_NONE:
      break;
    case PAINT_COLOR:
      {
        GdkColor color;

        gdk_color_init_copy (&color, &paint->color);
        color.alpha *= opacity;
        snapshot_push_fill (context->snapshot, path, fill_rule);
        gtk_snapshot_add_color (context->snapshot, &color, &bounds);
        gtk_snapshot_pop (context->snapshot);
        gdk_color_finish (&color);
      }
      break;
    case PAINT_SERVER:
    case PAINT_SERVER_WITH_FALLBACK:
    case PAINT_SERVER_WITH_CURRENT_COLOR:
      {
        if (opacity < 1)
          gtk_snapshot_push_opacity (context->snapshot, opacity);

        snapshot_push_fill (context->snapshot, path, fill_rule);
        paint_server (paint, &bounds, &bounds, context);
        gtk_snapshot_pop (context->snapshot);

        if (opacity < 1)
          gtk_snapshot_pop (context->snapshot);
      }
      break;
    case PAINT_CURRENT_COLOR:
    case PAINT_CONTEXT_FILL:
    case PAINT_CONTEXT_STROKE:
    case PAINT_SYMBOLIC:
    default:
      g_assert_not_reached ();
    }
}

static void
stroke_shape (Shape        *shape,
              GskPath      *path,
              PaintContext *context)
{
  SvgPaint *paint;
  GskStroke *stroke;
  graphene_rect_t bounds;
  graphene_rect_t paint_bounds;
  double opacity;
  VectorEffect effect;
  GskRenderNode *child;
  GskRenderNode *node;

  paint = (SvgPaint *) shape->current[SHAPE_ATTR_STROKE];
  paint = get_context_paint (paint, context->ctx_shape_stack);
  if (paint->kind == PAINT_NONE)
    return;

  if (!gsk_path_get_bounds (path, &bounds))
    return;

  stroke = shape_create_stroke (shape, context);
  if (!gsk_path_get_stroke_bounds (path, stroke, &paint_bounds))
    return;

  opacity = svg_number_get (shape->current[SHAPE_ATTR_STROKE_OPACITY], 1);
  effect = svg_enum_get (shape->current[SHAPE_ATTR_VECTOR_EFFECT]);

  switch (paint->kind)
    {
    case PAINT_COLOR:
      {
        GdkColor color;
        gdk_color_init_copy (&color, &paint->color);
        color.alpha *= opacity;
        opacity = 1;
        child = gsk_color_node_new2 (&color, &paint_bounds);
        gdk_color_finish (&color);
      }
      break;
    case PAINT_SERVER:
    case PAINT_SERVER_WITH_FALLBACK:
    case PAINT_SERVER_WITH_CURRENT_COLOR:
      gtk_snapshot_push_collect (context->snapshot);
      paint_server (paint, &bounds, &paint_bounds, context);
      child = gtk_snapshot_pop_collect (context->snapshot);
      break;
    case PAINT_NONE:
    case PAINT_CURRENT_COLOR:
    case PAINT_CONTEXT_FILL:
    case PAINT_CONTEXT_STROKE:
    case PAINT_SYMBOLIC:
    default:
      g_assert_not_reached ();
    }

  if (effect == VECTOR_EFFECT_NON_SCALING_STROKE)
    {
      GskTransform *host_transform = NULL;
      GskTransform *user_transform = NULL;
      GskPath *transformed_path = NULL;
      GskRenderNode *transformed_child;
      GskRenderNode *stroke_node;

      host_transform = context_get_host_transform (context);
      user_transform = gsk_transform_invert (gsk_transform_ref (host_transform));
      transformed_path = gsk_transform_transform_path (user_transform, path);

      transformed_child = gsk_transform_node_new (child, user_transform);
      stroke_node = gsk_stroke_node_new (transformed_child, transformed_path, stroke);

      node = gsk_transform_node_new (stroke_node, host_transform);

      gsk_render_node_unref (stroke_node);
      gsk_render_node_unref (transformed_child);

      gsk_path_unref (transformed_path);
      gsk_transform_unref (user_transform);
      gsk_transform_unref (host_transform);
    }
  else
    {
      node = gsk_stroke_node_new (child, path, stroke);
    }

  gsk_render_node_unref (child);

  if (opacity < 1)
    {
      GskRenderNode *node2 = gsk_opacity_node_new (node, opacity);
      gsk_render_node_unref (node);
      node = node2;
    }

  gtk_snapshot_append_node (context->snapshot, node);
  gsk_render_node_unref (node);

  gsk_stroke_free (stroke);
}

typedef enum {
  VERTEX_START,
  VERTEX_MID,
  VERTEX_END,
} VertexKind;

/* }}} */
/* {{{ Markers */

static void
paint_marker (Shape              *shape,
              GskPath            *path,
              PaintContext       *context,
              const GskPathPoint *point,
              VertexKind          kind)
{
  ShapeAttr attrs[] = { SHAPE_ATTR_MARKER_START, SHAPE_ATTR_MARKER_MID, SHAPE_ATTR_MARKER_END };
  SvgHref *href;
  SvgOrient *orient;
  SvgValue *units;
  Shape *marker;
  double scale;
  graphene_point_t vertex;
  double angle;
  double x, y, width, height;
  SvgViewBox *vb;
  SvgContentFit *cf;
  SvgValue *overflow;
  double sx, sy, tx, ty;
  graphene_rect_t view_box;
  SvgNumber *n;
  GskTransform *transform = NULL;

  gsk_path_point_get_position (point, path, &vertex);

  href = (SvgHref *) shape->current[attrs[kind]];
  if (href->kind == HREF_NONE)
    return;

  marker = href->shape;
  if (!marker)
    return;

  orient = (SvgOrient *) marker->current[SHAPE_ATTR_MARKER_ORIENT];
  units = marker->current[SHAPE_ATTR_MARKER_UNITS];

  if (svg_enum_get (units) == MARKER_UNITS_STROKE_WIDTH)
    scale = svg_number_get (shape->current[SHAPE_ATTR_STROKE_WIDTH], 1);
  else
    scale = 1;

  if (orient->kind == ORIENT_AUTO)
    {
      if (kind == VERTEX_START)
        {
          angle = gsk_path_point_get_rotation (point, path, GSK_PATH_TO_END);
          if (orient->start_reverse)
            angle += 180;
        }
      else if (kind == VERTEX_END)
        {
          angle = gsk_path_point_get_rotation (point, path, GSK_PATH_FROM_START);
        }
      else
        {
          angle = (gsk_path_point_get_rotation (point, path, GSK_PATH_TO_END) +
                   gsk_path_point_get_rotation (point, path, GSK_PATH_FROM_START)) / 2;
        }
    }
  else
    {
      angle = orient->angle;
    }

  vb = (SvgViewBox *) marker->current[SHAPE_ATTR_VIEW_BOX];
  cf = (SvgContentFit *) marker->current[SHAPE_ATTR_CONTENT_FIT];
  overflow = marker->current[SHAPE_ATTR_OVERFLOW];

  width = svg_number_get (marker->current[SHAPE_ATTR_WIDTH], context->viewport->size.width);
  height = svg_number_get (marker->current[SHAPE_ATTR_HEIGHT], context->viewport->size.height);

  if (width == 0 || height == 0)
    return;

  n = (SvgNumber *) marker->current[SHAPE_ATTR_REF_X];
  if (n->unit == SVG_UNIT_PERCENTAGE)
    x = n->value / 100 * width;
  else if (!vb->unset)
    x = n->value / vb->view_box.size.width * width;
  else
    x = n->value;

  n = (SvgNumber *) marker->current[SHAPE_ATTR_REF_Y];
  if (n->unit == SVG_UNIT_PERCENTAGE)
    y = n->value / 100 * height;
  else if (!vb->unset)
    y = n->value / vb->view_box.size.height * height;
  else
    y = n->value;

  width *= scale;
  height *= scale;
  x *= scale;
  y *= scale;

  if (vb->unset)
    graphene_rect_init (&view_box, 0, 0, width, height);
  else
    view_box = vb->view_box;

  compute_viewport_transform (cf->is_none,
                              cf->align_x,
                              cf->align_y,
                              cf->meet,
                              &view_box,
                              0, 0, width, height,
                              &sx, &sy, &tx, &ty);

  gtk_snapshot_save (context->snapshot);

  transform = gsk_transform_translate (NULL, &vertex);
  transform = gsk_transform_rotate (transform, angle);
  transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (-x, -y));
  transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (tx, ty));
  transform = gsk_transform_scale (transform, sx, sy);

  gtk_snapshot_transform (context->snapshot, transform);
  push_transform (context, transform);
  gsk_transform_unref (transform);

  if (svg_enum_get (overflow) == OVERFLOW_HIDDEN)
    gtk_snapshot_push_clip (context->snapshot, &GRAPHENE_RECT_INIT (0, 0, view_box.size.width, view_box.size.height));

  render_shape (marker, context);

  if (svg_enum_get (overflow) == OVERFLOW_HIDDEN)
    gtk_snapshot_pop (context->snapshot);

  pop_transform (context);
  gtk_snapshot_restore (context->snapshot);
}

static void
paint_markers (Shape        *shape,
               GskPath      *path,
               PaintContext *context)
{
  GskPathPoint point, point1;

  if (gsk_path_is_empty (path))
    return;

  if (((SvgHref *) shape->current[SHAPE_ATTR_MARKER_START])->kind == HREF_NONE &&
      ((SvgHref *) shape->current[SHAPE_ATTR_MARKER_MID])->kind == HREF_NONE &&
      ((SvgHref *) shape->current[SHAPE_ATTR_MARKER_END])->kind == HREF_NONE)
    return;

  if (!gsk_path_get_start_point (path, &point))
    return;

  push_ctx_shape (context, shape);

  push_op (context, MARKERS);
  paint_marker (shape, path, context, &point, VERTEX_START);

  if (gsk_path_get_next (path, &point))
    {
      while (1)
        {
          point1 = point;
          if (!gsk_path_get_next (path, &point1))
            {
              break;
            }

          paint_marker (shape, path, context, &point, VERTEX_MID);
          point = point1;
        }
    }

  paint_marker (shape, path, context, &point, VERTEX_END);

  pop_op (context);

  pop_ctx_shape (context);
}

/* }}} */
/* {{{ Text */

#define SVG_DEFAULT_DPI 96.0
static PangoLayout *
text_create_layout (Shape            *self,
                    PangoFontMap     *fontmap,
                    const char       *text,
                    graphene_point_t *origin,
                    graphene_rect_t  *bounds,
                    gboolean         *is_vertical,
                    double           *rotation)
{
  PangoContext *context;
  UnicodeBidi uni;
  PangoDirection direction;
  WritingMode wmode;
  PangoGravity gravity;
  PangoFontDescription *font_desc;
  const char *font_family;
  PangoLayout *layout;
  PangoAttrList *attr_list;
  PangoAttribute *attr;
  TextDecoration decoration;
  int w,h;
  PangoLayoutIter *iter;
  double offset;


  context = pango_font_map_create_context (fontmap);
  pango_context_set_language (context, svg_language_get (self->current[SHAPE_ATTR_LANG]));

  uni = svg_enum_get (self->current[SHAPE_ATTR_UNICODE_BIDI]);
  direction = svg_enum_get (self->current[SHAPE_ATTR_DIRECTION]);
  wmode = svg_enum_get (self->current[SHAPE_ATTR_WRITING_MODE]);
  switch (wmode)
    {
    case WRITING_MODE_HORIZONTAL_TB:
      gravity = PANGO_GRAVITY_SOUTH;
      break;
    case WRITING_MODE_VERTICAL_LR:
      // See note about having vertical-lr rotate -90° instead, below
    case WRITING_MODE_VERTICAL_RL:
      gravity = PANGO_GRAVITY_EAST;
      break;
    case WRITING_MODE_LEGACY_LR:
    case WRITING_MODE_LEGACY_LR_TB:
      direction = PANGO_DIRECTION_LTR;
      gravity = PANGO_GRAVITY_SOUTH;
      break;
    case WRITING_MODE_LEGACY_RL:
    case WRITING_MODE_LEGACY_RL_TB:
      direction = PANGO_DIRECTION_RTL;
      gravity = PANGO_GRAVITY_SOUTH;
      break;
    case WRITING_MODE_LEGACY_TB:
    case WRITING_MODE_LEGACY_TB_RL:
      direction = PANGO_DIRECTION_LTR;
      gravity = PANGO_GRAVITY_EAST;
      break;
    default:
      g_assert_not_reached ();
    }
  pango_context_set_base_gravity (context, gravity);

  if (uni == UNICODE_BIDI_OVERRIDE || uni == UNICODE_BIDI_EMBED)
    pango_context_set_base_dir (context, direction);

  font_desc = pango_font_description_copy (pango_context_get_font_description (context));

  font_family = svg_string_get (self->current[SHAPE_ATTR_FONT_FAMILY]);
  if (font_family && *font_family)
    pango_font_description_set_family_static (font_desc, font_family);

  pango_font_description_set_style (font_desc, svg_enum_get (self->current[SHAPE_ATTR_FONT_STYLE]));
  pango_font_description_set_variant (font_desc, svg_enum_get (self->current[SHAPE_ATTR_FONT_VARIANT]));
  pango_font_description_set_weight (font_desc, svg_number_get (self->current[SHAPE_ATTR_FONT_WEIGHT], 1000.));
  pango_font_description_set_stretch (font_desc, svg_enum_get (self->current[SHAPE_ATTR_FONT_STRETCH]));

  pango_font_description_set_size (font_desc,
                                   svg_number_get (self->current[SHAPE_ATTR_FONT_SIZE], 1.) *
                                   PANGO_SCALE / SVG_DEFAULT_DPI * 72);

  layout = pango_layout_new (context);
  pango_layout_set_font_description (layout, font_desc);
  pango_font_description_free (font_desc);

  attr_list = pango_attr_list_new ();

  attr = pango_attr_letter_spacing_new (svg_number_get (self->current[SHAPE_ATTR_LETTER_SPACING], 1.) * PANGO_SCALE);
  attr->start_index = 0;
  attr->end_index = G_MAXINT;
  pango_attr_list_insert (attr_list, attr);

  decoration = svg_text_decoration_get (self->current[SHAPE_ATTR_TEXT_DECORATION]);
  if (text)
    {
      if (decoration & TEXT_DECORATION_UNDERLINE)
        {
          attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
          attr->start_index = 0;
          attr->end_index = -1;
          pango_attr_list_insert (attr_list, attr);
        }
      if (decoration & TEXT_DECORATION_OVERLINE)
        {
          attr = pango_attr_overline_new (PANGO_OVERLINE_SINGLE);
          attr->start_index = 0;
          attr->end_index = -1;
          pango_attr_list_insert (attr_list, attr);
        }
      if (decoration & TEXT_DECORATION_LINE_TROUGH)
        {
          attr = pango_attr_strikethrough_new (TRUE);
          attr->start_index = 0;
          attr->end_index = -1;
          pango_attr_list_insert (attr_list, attr);
        }
    }

  pango_layout_set_attributes (layout, attr_list);
  pango_attr_list_unref (attr_list);

  pango_layout_set_text (layout, text, -1);

  pango_layout_set_alignment (layout, (direction == PANGO_DIRECTION_LTR) ? PANGO_ALIGN_LEFT : PANGO_ALIGN_RIGHT);

  pango_layout_get_size (layout, &w, &h);
  iter = pango_layout_get_iter (layout);
  offset = pango_layout_iter_get_baseline (iter) / (double)PANGO_SCALE;
  pango_layout_iter_free (iter);
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

  g_object_unref (context);
  return layout;
}

static gboolean
generate_layouts (Shape           *self,
                  PangoFontMap    *fontmap,
                  double          *x,
                  double          *y,
                  gboolean        *lastwasspace,
                  graphene_rect_t *bounds)
{
  double xs = 0.;
  double ys = 0.;
  gboolean lwss = TRUE;
  double dx, dy;

  g_assert (shape_type_has_text (self->type));

  if (svg_enum_get (self->current[SHAPE_ATTR_DISPLAY]) == DISPLAY_NONE)
    return FALSE;

  if (!x)
    x = &xs;
  if (!y)
    y = &ys;
  if (!lastwasspace)
    lastwasspace = &lwss;

  if (_gtk_bitmask_get (self->attrs, SHAPE_ATTR_X))
    *x = svg_number_get (self->current[SHAPE_ATTR_X], 1);
  if (_gtk_bitmask_get (self->attrs, SHAPE_ATTR_Y))
    *y = svg_number_get (self->current[SHAPE_ATTR_Y], 1);

  dx = svg_number_get (self->current[SHAPE_ATTR_DX], 1);
  dy = svg_number_get (self->current[SHAPE_ATTR_DY], 1);
  *x += dx;
  *y += dy;

  gboolean set_bounds = FALSE;
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

  for (guint i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);
      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          node->shape.has_bounds = generate_layouts (node->shape.shape, fontmap, x, y, lastwasspace, &node->shape.bounds);
          if (node->shape.has_bounds)
            ADD_BBOX (&node->shape.bounds)
          break;
        case TEXT_NODE_CHARACTERS:
          {
            graphene_point_t origin;
            graphene_rect_t cbounds;
            gboolean is_vertical;
            char *text = text_chomp (node->characters.text, lastwasspace);
            node->characters.layout = text_create_layout (self, fontmap, text, &origin, &cbounds, &is_vertical, &node->characters.r);
            g_free (text);

            node->characters.x = *x + origin.x;
            node->characters.y = *y + origin.y;

            graphene_rect_offset (&cbounds, *x, *y);
            ADD_BBOX (&cbounds)

            if (is_vertical)
              *y += cbounds.size.height;
            else
              *x += cbounds.size.width;
          }
          break;
        default:
          g_assert_not_reached ();
        }
    }

#undef ADD_BBOX
  return set_bounds;
}

static void
clear_layouts (Shape *self)
{
  for (guint i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);
      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          clear_layouts (node->shape.shape);
          break;
        case TEXT_NODE_CHARACTERS:
          g_clear_object (&node->characters.layout);
          break;
        default:
          g_assert_not_reached ();
        }
    }
}

static void
fill_text (Shape                 *self,
           PaintContext          *context,
           SvgPaint              *paint,
           const graphene_rect_t *bounds)
{
  g_assert (shape_type_has_text (self->type));

  for (guint i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);

      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          {
            SvgPaint *cpaint = paint;
            const graphene_rect_t *cbounds = bounds;

            if (svg_enum_get (node->shape.shape->current[SHAPE_ATTR_DISPLAY]) == DISPLAY_NONE)
              continue;

            if (svg_enum_get (node->shape.shape->current[SHAPE_ATTR_VISIBILITY]) == VISIBILITY_HIDDEN)
              continue;

            if (_gtk_bitmask_get (node->shape.shape->attrs, SHAPE_ATTR_FILL))
              {
                cpaint = (SvgPaint *) node->shape.shape->current[SHAPE_ATTR_FILL];
                if (node->shape.has_bounds)
                  cbounds = &node->shape.bounds;
              }
            fill_text (node->shape.shape, context, cpaint, cbounds);
          }
          break;
        case TEXT_NODE_CHARACTERS:
          {
            double opacity;

            if (paint->kind == PAINT_NONE)
              goto skip;

#if 0
            GskRoundedRect brd;
            gsk_rounded_rect_init_from_rect (&brd, bounds, 0.f);
            GdkRGBA red = (GdkRGBA){ .red = 1., .green = 0., .blue = 0., .alpha = 1. };
            gtk_snapshot_append_border (context->snapshot, &brd, (const float[4]){ 3.f, 3.f, 3.f, 3.f }, (const GdkRGBA[4]){ red, red, red, red });
#endif

            gtk_snapshot_save (context->snapshot);

            opacity = svg_number_get (self->current[SHAPE_ATTR_FILL_OPACITY], 1);
            if (paint->kind == PAINT_COLOR)
              {
                GdkColor color;

                gdk_color_init_copy (&color, &paint->color);
                color.alpha *= opacity;
                gtk_snapshot_translate (context->snapshot, &GRAPHENE_POINT_INIT (node->characters.x, node->characters.y));
                gtk_snapshot_rotate (context->snapshot, node->characters.r);
                gtk_snapshot_add_layout (context->snapshot, node->characters.layout, &color);
              }
            else if (paint_is_server (paint->kind))
              {
                if (opacity < 1)
                  gtk_snapshot_push_opacity (context->snapshot, opacity);

                gtk_snapshot_push_mask (context->snapshot, GSK_MASK_MODE_ALPHA);
                gtk_snapshot_translate (context->snapshot, &GRAPHENE_POINT_INIT (node->characters.x, node->characters.y));
                gtk_snapshot_rotate (context->snapshot, node->characters.r);
                gtk_snapshot_append_layout (context->snapshot, node->characters.layout, &GDK_RGBA_BLACK);
                gtk_snapshot_pop (context->snapshot);
                paint_server (paint, bounds, bounds, context);
                gtk_snapshot_pop (context->snapshot);

                if (opacity < 1)
                  gtk_snapshot_pop (context->snapshot);
              }

            gtk_snapshot_restore (context->snapshot);

skip: ;
          }
          break;
        default:
          g_assert_not_reached ();
        }
    }
}

static void
stroke_text (Shape                 *self,
             PaintContext          *context,
             SvgPaint              *paint,
             GskStroke             *stroke,
             const graphene_rect_t *bounds)
{
  g_assert (shape_type_has_text (self->type));

  for (guint i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);

      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          {
            SvgPaint *cpaint = paint;
            const graphene_rect_t *cbounds = bounds;

            if (svg_enum_get (node->shape.shape->current[SHAPE_ATTR_DISPLAY]) == DISPLAY_NONE)
              continue;

            if (svg_enum_get (node->shape.shape->current[SHAPE_ATTR_VISIBILITY]) == VISIBILITY_HIDDEN)
              continue;

            if (_gtk_bitmask_get (node->shape.shape->attrs, SHAPE_ATTR_STROKE))
              {
                cpaint = (SvgPaint *) node->shape.shape->current[SHAPE_ATTR_STROKE];
                if (node->shape.has_bounds)
                  cbounds = &node->shape.bounds;
              }
            stroke_text (node->shape.shape, context, cpaint, stroke, cbounds);
          }
          break;
        case TEXT_NODE_CHARACTERS:
          {
            GskPath *path;
            double opacity;

            if (paint->kind == PAINT_NONE)
              goto skip;

            gtk_snapshot_save (context->snapshot);
            opacity = svg_number_get (self->current[SHAPE_ATTR_STROKE_OPACITY], 1);
            if (paint->kind == PAINT_COLOR)
              {
                GdkColor color;

                gdk_color_init_copy (&color, &paint->color);
                color.alpha *= opacity;

                gtk_snapshot_push_mask (context->snapshot, GSK_MASK_MODE_ALPHA);
                gtk_snapshot_translate (context->snapshot, &GRAPHENE_POINT_INIT (node->characters.x, node->characters.y));
                gtk_snapshot_rotate (context->snapshot, node->characters.r);
                path = pango_layout_to_path (node->characters.layout);

                gtk_snapshot_append_stroke (context->snapshot, path, stroke, &GDK_RGBA_BLACK);
                gtk_snapshot_pop (context->snapshot);
                gtk_snapshot_add_color (context->snapshot, &color, bounds);
                gtk_snapshot_pop (context->snapshot);
                gsk_path_unref (path);
                gdk_color_finish (&color);
              }
            else if (paint_is_server (paint->kind))
              {
                if (opacity < 1)
                  gtk_snapshot_push_opacity (context->snapshot, opacity);

                gtk_snapshot_push_mask (context->snapshot, GSK_MASK_MODE_ALPHA);
                gtk_snapshot_translate (context->snapshot, &GRAPHENE_POINT_INIT (node->characters.x, node->characters.y));
                gtk_snapshot_rotate (context->snapshot, node->characters.r);
                path = pango_layout_to_path (node->characters.layout);
                gtk_snapshot_append_stroke (context->snapshot, path, stroke, &GDK_RGBA_BLACK);
                gtk_snapshot_pop (context->snapshot);
                paint_server (paint, bounds, bounds, context);
                gtk_snapshot_pop (context->snapshot);
                gsk_path_unref (path);

                if (opacity < 1)
                  gtk_snapshot_pop (context->snapshot);
              }
            gtk_snapshot_restore (context->snapshot);

skip: ;
          }
          break;
        default:
          g_assert_not_reached ();
        }
    }
}

/* }}} */
/* {{{ Images */

static void
render_image (Shape        *shape,
              PaintContext *context)
{
  SvgHref *href = (SvgHref *) shape->current[SHAPE_ATTR_HREF];
  SvgContentFit *content_fit = (SvgContentFit *) shape->current[SHAPE_ATTR_CONTENT_FIT];
  SvgValue *overflow = shape->current[SHAPE_ATTR_OVERFLOW];
  graphene_rect_t vb;
  double sx, sy, tx, ty;
  double x, y, width, height;

  if (!href->texture)
    {
      gtk_svg_rendering_error (context->svg,
                               "No content for <image>");
      return;
    }

  graphene_rect_init (&vb,
                      0, 0,
                      gdk_texture_get_width (href->texture),
                      gdk_texture_get_height (href->texture));

  x = context->viewport->origin.x + svg_number_get (shape->current[SHAPE_ATTR_X], context->viewport->size.width);
  y = context->viewport->origin.y + svg_number_get (shape->current[SHAPE_ATTR_Y], context->viewport->size.height);
  width = svg_number_get (shape->current[SHAPE_ATTR_WIDTH], context->viewport->size.width);
  height = svg_number_get (shape->current[SHAPE_ATTR_HEIGHT], context->viewport->size.height);

  compute_viewport_transform (content_fit->is_none,
                              content_fit->align_x,
                              content_fit->align_y,
                              content_fit->meet,
                              &vb,
                              x, y, width, height,
                              &sx, &sy, &tx, &ty);

  if (svg_enum_get (overflow) == OVERFLOW_HIDDEN)
    gtk_snapshot_push_clip (context->snapshot, &GRAPHENE_RECT_INIT (x, y, width, height));

  GskTransform *transform = NULL;
  gtk_snapshot_save (context->snapshot);

  transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (tx, ty));
  transform = gsk_transform_scale (transform, sx, sy);

  gtk_snapshot_transform (context->snapshot, transform);
  push_transform (context, transform);
  gsk_transform_unref (transform);

  gtk_snapshot_append_texture (context->snapshot, href->texture, &vb);

  pop_transform (context);
  gtk_snapshot_restore (context->snapshot);

  if (svg_enum_get (overflow) == OVERFLOW_HIDDEN)
    gtk_snapshot_pop (context->snapshot);
}

/* }}} */

static gboolean
shape_is_degenerate (Shape *shape)
{
  if (shape->type == SHAPE_RECT)
    return svg_number_get (shape->current[SHAPE_ATTR_WIDTH], 1) <= 0 ||
           svg_number_get (shape->current[SHAPE_ATTR_HEIGHT], 1) <= 0;
  else if (shape->type == SHAPE_CIRCLE)
    return svg_number_get (shape->current[SHAPE_ATTR_R], 1) <= 0;
  else if (shape->type == SHAPE_ELLIPSE)
    return (!svg_value_is_auto (shape->current[SHAPE_ATTR_RX]) &&
            svg_number_get (shape->current[SHAPE_ATTR_RX], 1) <= 0) ||
           (!svg_value_is_auto (shape->current[SHAPE_ATTR_RY]) &&
            svg_number_get (shape->current[SHAPE_ATTR_RY], 1) <= 0);
  else
    return FALSE;
}

static void
recompute_current_values (Shape        *shape,
                          Shape        *parent,
                          PaintContext *context)
{
  ComputeContext ctx;

  ctx.svg = context->svg;
  ctx.viewport = context->viewport;
  ctx.parent = parent;
  ctx.current_time = context->current_time;
  ctx.colors = context->colors;
  ctx.n_colors = context->n_colors;

  compute_current_values_for_shape (shape, &ctx);
}

static gboolean
shape_is_ancestor (Shape *parent, Shape *shape)
{
  for (Shape *p = shape->parent;  p; p = p->parent)
    {
      if (p == parent)
        return TRUE;
    }
  return FALSE;
}


static void
paint_shape (Shape        *shape,
             PaintContext *context)
{
  GskPath *path;

  if (shape->type == SHAPE_USE)
    {
      if (((SvgHref *) shape->current[SHAPE_ATTR_HREF])->shape != NULL)
        {
          Shape *use_shape = ((SvgHref *) shape->current[SHAPE_ATTR_HREF])->shape;

          if (shape_is_ancestor (use_shape, shape))
            {
              gtk_svg_rendering_error (context->svg, "not following invalid <use> href");
              return;
            }

          mark_as_computed_for_use (use_shape, FALSE);
          recompute_current_values (use_shape, shape, context);

          push_ctx_shape (context, shape);
          render_shape (use_shape, context);
          pop_ctx_shape (context);

          mark_as_computed_for_use (use_shape, TRUE);
        }
      return;
    }

  if (shape->computed_for_use)
    {
      recompute_current_values (shape, shape->parent, context);
      mark_as_computed_for_use (shape, FALSE);
    }

  if (shape->type == SHAPE_TEXT)
    {
      TextAnchor anchor;
      WritingMode wmode;
      graphene_rect_t bounds;
      float dx, dy;

      if (svg_enum_get (shape->current[SHAPE_ATTR_DISPLAY]) == DISPLAY_NONE)
        return;

      if (svg_enum_get (shape->current[SHAPE_ATTR_VISIBILITY]) == VISIBILITY_HIDDEN)
        return;

      anchor = svg_enum_get (shape->current[SHAPE_ATTR_TEXT_ANCHOR]);
      wmode = svg_enum_get (shape->current[SHAPE_ATTR_WRITING_MODE]);
      if (!generate_layouts (shape, get_fontmap (context->svg), NULL, NULL, NULL, &bounds))
        return;

      dx = dy = 0;
      switch (anchor)
        {
        case TEXT_ANCHOR_START:
          break;
        case TEXT_ANCHOR_MIDDLE:
          if (is_vertical_writing_mode[wmode])
            dy = - bounds.size.height / 2;
          else
            dx = - bounds.size.width / 2;
          break;
        case TEXT_ANCHOR_END:
          if (is_vertical_writing_mode[wmode])
            dy = - bounds.size.height;
          else
            dx = - bounds.size.width;
          break;
        default:
          g_assert_not_reached ();
        }

      graphene_rect_init (&shape->bounds,
                          bounds.origin.x + dx, bounds.origin.y + dy,
                          bounds.size.width, bounds.size.height);
      shape->valid_bounds = TRUE;

      GskTransform *transform = NULL;
      gtk_snapshot_save (context->snapshot);
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (dx, dy));

      gtk_snapshot_transform (context->snapshot, transform);
      push_transform (context, transform);
      gsk_transform_unref (transform);

      if (context->op == CLIPPING)
        {
          SvgValue *paint = svg_paint_new_black ();
          fill_text (shape, context, (SvgPaint *) paint, &bounds);
          svg_value_unref (paint);
        }
      else
        {
          GskStroke *stroke = shape_create_stroke (shape, context);

          switch (svg_enum_get (shape->current[SHAPE_ATTR_PAINT_ORDER]))
            {
            case PAINT_ORDER_FILL_STROKE_MARKERS:
            case PAINT_ORDER_FILL_MARKERS_STROKE:
            case PAINT_ORDER_MARKERS_FILL_STROKE:
              fill_text (shape, context, (SvgPaint *) shape->current[SHAPE_ATTR_FILL], &bounds);
              break;
            case PAINT_ORDER_STROKE_FILL_MARKERS:
            case PAINT_ORDER_STROKE_MARKERS_FILL:
            case PAINT_ORDER_MARKERS_STROKE_FILL:
              stroke_text (shape, context, (SvgPaint *) shape->current[SHAPE_ATTR_STROKE], stroke, &bounds);
              break;
            default:
              g_assert_not_reached ();
            }

          switch (svg_enum_get (shape->current[SHAPE_ATTR_PAINT_ORDER]))
            {
            case PAINT_ORDER_FILL_STROKE_MARKERS:
            case PAINT_ORDER_FILL_MARKERS_STROKE:
            case PAINT_ORDER_MARKERS_FILL_STROKE:
              stroke_text (shape, context, (SvgPaint *) shape->current[SHAPE_ATTR_STROKE], stroke, &bounds);
              break;
            case PAINT_ORDER_STROKE_FILL_MARKERS:
            case PAINT_ORDER_STROKE_MARKERS_FILL:
            case PAINT_ORDER_MARKERS_STROKE_FILL:
              fill_text (shape, context, (SvgPaint *) shape->current[SHAPE_ATTR_FILL], &bounds);
              break;
            default:
              g_assert_not_reached ();
            }

          gsk_stroke_free (stroke);
        }

      clear_layouts (shape);

      pop_transform (context);
      gtk_snapshot_restore (context->snapshot);

      return;
    }

  if (shape->type == SHAPE_IMAGE)
    {
      render_image (shape, context);

      return;
    }

  if (shape_type_has_shapes (shape->type))
    {
      for (int i = 0; i < shape->shapes->len; i++)
        {
          Shape *s = g_ptr_array_index (shape->shapes, i);

          render_shape (s, context);

          if (shape->type == SHAPE_SWITCH &&
              !shape_conditionally_excluded (s, context->svg))
            break;
        }

      return;
    }

  if (svg_enum_get (shape->current[SHAPE_ATTR_VISIBILITY]) == VISIBILITY_HIDDEN)
    return;

  if (shape_is_degenerate (shape))
    return;

  /* Below is where we render *actual* content (i.e. graphical
   * shapes that have paths). This involves filling, stroking
   * and placing markers, in the order determined by paint-order.
   * Unless we are clipping, in which case we just fill to
   * create a 1-bit mask of the 'raw geometry'.
   */
  path = shape_get_current_path (shape, context->viewport);

  if (context->op == CLIPPING)
    {
      graphene_rect_t bounds;

      if (gsk_path_get_bounds (path, &bounds))
        {
          GdkColor color = GDK_COLOR_SRGB (0, 0, 0, 1);
          GskFillRule clip_rule;

          clip_rule = svg_enum_get (shape->current[SHAPE_ATTR_CLIP_RULE]);
          gtk_snapshot_push_fill (context->snapshot, path, clip_rule);
          gtk_snapshot_add_color (context->snapshot, &color, &bounds);
          gtk_snapshot_pop (context->snapshot);
        }
    }
  else
    {
      SvgPaint *paint = (SvgPaint *) shape->current[SHAPE_ATTR_FILL];
      double opacity = svg_number_get (shape->current[SHAPE_ATTR_FILL_OPACITY], 1);

      switch (svg_enum_get (shape->current[SHAPE_ATTR_PAINT_ORDER]))
        {
        case PAINT_ORDER_FILL_STROKE_MARKERS:
        case PAINT_ORDER_FILL_MARKERS_STROKE:
          fill_shape (shape, path, paint, opacity, context);
          break;
        case PAINT_ORDER_STROKE_FILL_MARKERS:
        case PAINT_ORDER_STROKE_MARKERS_FILL:
          stroke_shape (shape, path, context);
          break;
        case PAINT_ORDER_MARKERS_FILL_STROKE:
        case PAINT_ORDER_MARKERS_STROKE_FILL:
          paint_markers (shape, path, context);
          break;
        default:
          g_assert_not_reached ();
        }

      switch (svg_enum_get (shape->current[SHAPE_ATTR_PAINT_ORDER]))
        {
        case PAINT_ORDER_MARKERS_FILL_STROKE:
        case PAINT_ORDER_STROKE_FILL_MARKERS:
          fill_shape (shape, path, paint, opacity, context);
          break;
        case PAINT_ORDER_FILL_STROKE_MARKERS:
        case PAINT_ORDER_MARKERS_STROKE_FILL:
          stroke_shape (shape, path, context);
          break;
        case PAINT_ORDER_FILL_MARKERS_STROKE:
        case PAINT_ORDER_STROKE_MARKERS_FILL:
          paint_markers (shape, path, context);
          break;
        default:
          g_assert_not_reached ();
        }

      switch (svg_enum_get (shape->current[SHAPE_ATTR_PAINT_ORDER]))
        {
        case PAINT_ORDER_MARKERS_STROKE_FILL:
        case PAINT_ORDER_STROKE_MARKERS_FILL:
          fill_shape (shape, path, paint, opacity, context);
          break;
        case PAINT_ORDER_MARKERS_FILL_STROKE:
        case PAINT_ORDER_FILL_MARKERS_STROKE:
          stroke_shape (shape, path, context);
          break;
        case PAINT_ORDER_STROKE_FILL_MARKERS:
        case PAINT_ORDER_FILL_STROKE_MARKERS:
          paint_markers (shape, path, context);
          break;
        default:
          g_assert_not_reached ();
        }
    }

  gsk_path_unref (path);
}

static gboolean
display_property_applies_to (Shape *shape)
{
  return shape->type != SHAPE_MASK &&
         shape->type != SHAPE_CLIP_PATH &&
         shape->type != SHAPE_MARKER &&
         shape->type != SHAPE_SYMBOL;
}

static void
render_shape (Shape        *shape,
              PaintContext *context)
{
  gboolean op_changed;

  if (shape->type == SHAPE_DEFS ||
      shape->type == SHAPE_LINEAR_GRADIENT ||
      shape->type == SHAPE_RADIAL_GRADIENT)
    return;

  if (shape_type_is_never_rendered (shape->type))
    {
      if (!((shape->type == SHAPE_SYMBOL && shape_is_use_target (shape, context)) ||
           (shape->type == SHAPE_CLIP_PATH && context->op == CLIPPING && context->op_changed) ||
           (shape->type == SHAPE_MASK && context->op == MASKING && context->op_changed) ||
           (shape->type == SHAPE_MARKER && context->op == MARKERS && context->op_changed)))
        return;
    }

  if (display_property_applies_to (shape))
    {
      if (svg_enum_get (shape->current[SHAPE_ATTR_DISPLAY]) == DISPLAY_NONE)
        return;
    }

  if (shape_conditionally_excluded (shape, context->svg))
    return;

  if (context->instance_count++ > DRAWING_LIMIT)
    {
      gtk_svg_rendering_error (context->svg, "excessive instance count, aborting");
      return;
    }

  context->depth++;

  if (context->depth > NESTING_LIMIT)
    {
      gtk_svg_rendering_error (context->svg,
                               "excessive rendering depth (> %d), aborting",
                               NESTING_LIMIT);
      return;
    }

  push_group (shape, context);

  op_changed = context->op_changed;
  context->op_changed = FALSE;

  paint_shape (shape, context);

  context->op_changed = op_changed;

  pop_group (shape, context);

  context->depth--;
}

/* }}} */
/* {{{ GtkSymbolicPaintable implementation */

static gboolean
can_reuse_node (GtkSvg        *self,
                double         width,
                double         height,
                const GdkRGBA *colors,
                size_t         n_colors,
                double         weight)
{
  if ((width != self->node_for.width || height != self->node_for.height))
    {
      dbg_print ("cache", "Can't reuse rendernode: size change");
      return FALSE;
    }

  if ((self->used & GTK_SVG_USES_STROKES) != 0 &&
      self->weight < 1 &&
      weight != self->node_for.weight)
    {
      dbg_print ("cache", "Can't reuse rendernode: stroke weight change");
      return FALSE;
    }

  if ((self->features & GTK_SVG_EXTENSIONS) != 0)
    {
      if ((self->used & GTK_SVG_USES_SYMBOLIC_FOREGROUND) != 0 &&
           self->node_for.n_colors > GTK_SYMBOLIC_COLOR_FOREGROUND &&
           n_colors > GTK_SYMBOLIC_COLOR_FOREGROUND &&
           !gdk_rgba_equal (&self->node_for.colors[GTK_SYMBOLIC_COLOR_FOREGROUND],
                            &colors[GTK_SYMBOLIC_COLOR_FOREGROUND]))
        {
          dbg_print ("cache", "Can't reuse rendernode: symbolic foreground change");
          return FALSE;
        }

      if ((self->used & GTK_SVG_USES_SYMBOLIC_ERROR) != 0 &&
          self->node_for.n_colors > GTK_SYMBOLIC_COLOR_ERROR &&
          n_colors > GTK_SYMBOLIC_COLOR_ERROR &&
          !gdk_rgba_equal (&self->node_for.colors[GTK_SYMBOLIC_COLOR_ERROR],
                          &colors[GTK_SYMBOLIC_COLOR_ERROR]))
        {
          dbg_print ("cache", "Can't reuse rendernode: symbolic error change");
          return FALSE;
        }

      if ((self->used & GTK_SVG_USES_SYMBOLIC_WARNING) != 0 &&
          self->node_for.n_colors > GTK_SYMBOLIC_COLOR_WARNING &&
          n_colors > GTK_SYMBOLIC_COLOR_WARNING &&
          !gdk_rgba_equal (&self->node_for.colors[GTK_SYMBOLIC_COLOR_WARNING],
                          &colors[GTK_SYMBOLIC_COLOR_WARNING]))
        {
          dbg_print ("cache", "Can't reuse rendernode: symbolic warning change");
          return FALSE;
        }

      if ((self->used & GTK_SVG_USES_SYMBOLIC_SUCCESS) != 0 &&
          self->node_for.n_colors > GTK_SYMBOLIC_COLOR_SUCCESS &&
          n_colors > GTK_SYMBOLIC_COLOR_SUCCESS &&
          !gdk_rgba_equal (&self->node_for.colors[GTK_SYMBOLIC_COLOR_SUCCESS],
                          &colors[GTK_SYMBOLIC_COLOR_SUCCESS]))
        {
          dbg_print ("cache", "Can't reuse rendernode: symbolic success change");
          return FALSE;
        }

      if ((self->used & GTK_SVG_USES_SYMBOLIC_ACCENT) != 0 &&
          self->node_for.n_colors > GTK_SYMBOLIC_COLOR_ACCENT &&
          n_colors > GTK_SYMBOLIC_COLOR_ACCENT &&
          !gdk_rgba_equal (&self->node_for.colors[GTK_SYMBOLIC_COLOR_ACCENT],
                          &colors[GTK_SYMBOLIC_COLOR_ACCENT]))
        {
          dbg_print ("cache", "Can't reuse rendernode: symbolic accent change");
          return FALSE;
        }
    }

  return TRUE;
}

/* Note that we are doing this in two passes:
 * 1. Update current values from animations
 * 2. Paint with the current values
 *
 * This is the easiest way to avoid complications due
 * to the fact that animations can have dependencies
 * on current values of other shapes (e.g animateMotion
 * -> path -> rect -> x, y, width, height -> viewport).
 *
 * To handle such dependencies correctly, we compute
 * an *update order* which may be different than the
 * paint order that is determined by the document
 * structure, and use that order in a separate pass
 * before we paint.
 */
static void
gtk_svg_snapshot_with_weight (GtkSymbolicPaintable  *paintable,
                              GtkSnapshot           *snapshot,
                              double                 width,
                              double                 height,
                              const GdkRGBA         *colors,
                              size_t                 n_colors,
                              double                 weight)
{
  GtkSvg *self = GTK_SVG (paintable);
  graphene_rect_t viewport = GRAPHENE_RECT_INIT (0, 0, width, height);

  if (self->width < 0 || self->height < 0)
    return;

#if 0
  if (self->node == NULL ||
      !can_reuse_node (self, width, height, colors, n_colors, weight))
#endif
    {
      ComputeContext compute_context;
      PaintContext paint_context;

      g_clear_pointer (&self->node, gsk_render_node_unref);

      self->current_width = width;
      self->current_height = height;

      compute_context.svg = self;
      compute_context.viewport = &viewport;
      compute_context.colors = colors;
      compute_context.n_colors = n_colors;
      compute_context.current_time = self->current_time;
      compute_context.parent = NULL;
      compute_context.interpolation = GDK_COLOR_STATE_SRGB;

      compute_current_values_for_shape (self->content, &compute_context);

      paint_context.svg = self;
      paint_context.viewport = &viewport;
      paint_context.viewport_stack = NULL;
      paint_context.snapshot = snapshot;
      paint_context.colors = colors;
      paint_context.n_colors = n_colors;
      paint_context.weight = self->weight >= 1 ? self->weight : weight;
      paint_context.op = RENDERING;
      paint_context.op_stack = NULL;
      paint_context.ctx_shape_stack = NULL;
      paint_context.current_time = self->current_time;
      paint_context.depth = 0;
      paint_context.transforms = NULL;
      paint_context.instance_count = 0;

      gtk_snapshot_push_collect (snapshot);

      if (self->overflow == GTK_OVERFLOW_HIDDEN)
        gtk_snapshot_push_clip (snapshot,
                                &GRAPHENE_RECT_INIT (0, 0, width, height));

      render_shape (self->content, &paint_context);

      if (self->overflow == GTK_OVERFLOW_HIDDEN)
        gtk_snapshot_pop (snapshot);

      /* Sanity checks. */
      g_assert (paint_context.viewport_stack == NULL);
      g_assert (paint_context.op_stack == NULL);
      g_assert (paint_context.ctx_shape_stack == NULL);
      g_assert (paint_context.transforms == NULL);

      self->node = gtk_snapshot_pop_collect (snapshot);

      self->node_for.width = width;
      self->node_for.height = height;
      memcpy (self->node_for.colors, colors, MIN (n_colors, 5) * sizeof (GdkRGBA));
      self->node_for.n_colors = n_colors;
      self->node_for.weight = weight;
    }

  if (self->node)
    gtk_snapshot_append_node (snapshot, self->node);

  if (self->advance_after_snapshot)
    {
      self->advance_after_snapshot = FALSE;
      g_clear_handle_id (&self->pending_invalidate, g_source_remove);
      self->pending_invalidate = g_idle_add_once (invalidate_later, self);
    }
}

static void
gtk_svg_snapshot_symbolic (GtkSymbolicPaintable *paintable,
                           GtkSnapshot          *snapshot,
                           double                width,
                           double                height,
                           const GdkRGBA        *colors,
                           size_t                 n_colors)
{
  gtk_svg_snapshot_with_weight (paintable,
                                snapshot,
                                width, height,
                                colors, n_colors,
                                400);
}

static void
gtk_svg_init_symbolic_paintable_interface (GtkSymbolicPaintableInterface *iface)
{
  iface->snapshot_symbolic = gtk_svg_snapshot_symbolic;
  iface->snapshot_with_weight = gtk_svg_snapshot_with_weight;
}

/* }}} */
/* {{{ GdkPaintable implementation */

static void
gtk_svg_snapshot (GdkPaintable *paintable,
                  GtkSnapshot  *snapshot,
                  double        width,
                  double        height)
{
  gtk_svg_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (paintable),
                             snapshot,
                             width, height,
                             NULL, 0);
}

static int
gtk_svg_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkSvg *self = GTK_SVG (paintable);

  return ceil (self->width);
}

static int
gtk_svg_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkSvg *self = GTK_SVG (paintable);

  return ceil (self->height);
}

static double
gtk_svg_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GtkSvg *self = GTK_SVG (paintable);
  SvgViewBox *vb;

  if (self->width > 0 && self->height > 0)
    return self->width / self->height;

  vb = (SvgViewBox *) self->content->current[SHAPE_ATTR_VIEW_BOX];
  if (!vb->unset)
    {
      if (vb->view_box.size.width > 0 && vb->view_box.size.height > 0)
        return vb->view_box.size.width / vb->view_box.size.height;
    }

  return 0;
}

static void
gtk_svg_init_paintable_interface (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_svg_snapshot;
  iface->get_intrinsic_width = gtk_svg_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_svg_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = gtk_svg_get_intrinsic_aspect_ratio;
}

/* }}} */
/* {{{ GObject boilerplate */

struct _GtkSvgClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_RESOURCE = 1,
  PROP_FEATURES,
  PROP_PLAYING,
  PROP_WEIGHT,
  PROP_STATE,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];


G_DEFINE_TYPE_WITH_CODE (GtkSvg, gtk_svg, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_svg_init_paintable_interface)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SYMBOLIC_PAINTABLE,
                                                gtk_svg_init_symbolic_paintable_interface))

static void
gtk_svg_init (GtkSvg *self)
{
  self->weight = -1;
  self->overflow = GTK_OVERFLOW_HIDDEN;
  self->state = GTK_SVG_STATE_EMPTY;
  self->load_time = INDEFINITE;
  self->state_change_delay = 0;
  self->next_update = INDEFINITE;
  self->playing = FALSE;
  self->run_mode = GTK_SVG_RUN_MODE_STOPPED;

  self->features = GTK_SVG_DEFAULT_FEATURES;

  self->content = shape_new (NULL, SHAPE_SVG);
  self->timeline = timeline_new ();

  self->images = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

static void
gtk_svg_dispose (GObject *object)
{
  GtkSvg *self = GTK_SVG (object);

  frame_clock_disconnect (self);
  g_clear_handle_id (&self->pending_invalidate, g_source_remove);

  g_clear_pointer (&self->content, shape_free);
  g_clear_pointer (&self->timeline, timeline_free);
  g_clear_pointer (&self->images, g_hash_table_unref);
  g_clear_object (&self->fontmap);
  g_clear_pointer (&self->font_files, g_ptr_array_unref);
  g_clear_pointer (&self->node, gsk_render_node_unref);

  g_clear_object (&self->clock);

  g_free (self->author);
  g_free (self->license);
  g_free (self->description);
  g_free (self->keywords);

  G_OBJECT_CLASS (gtk_svg_parent_class)->dispose (object);
}

static void
gtk_svg_get_property (GObject      *object,
                      unsigned int  property_id,
                      GValue       *value,
                      GParamSpec   *pspec)
{
  GtkSvg *self = GTK_SVG (object);

  switch (property_id)
    {
    case PROP_RESOURCE:
      g_value_set_string (value, self->resource);
      break;

    case PROP_FEATURES:
      g_value_set_flags (value, self->features);
      break;

    case PROP_PLAYING:
      g_value_set_boolean (value, self->playing);
      break;

    case PROP_WEIGHT:
      g_value_set_double (value, self->weight);
      break;

    case PROP_STATE:
      g_value_set_uint (value, self->state);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_svg_set_property (GObject      *object,
                      unsigned int  property_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  GtkSvg *self = GTK_SVG (object);

  switch (property_id)
    {
    case PROP_RESOURCE:
      gtk_svg_load_from_resource (self, g_value_get_string (value));
      break;

    case PROP_FEATURES:
      gtk_svg_set_features (self, g_value_get_flags (value));
      break;

    case PROP_PLAYING:
      gtk_svg_set_playing (self, g_value_get_boolean (value));
      break;

    case PROP_STATE:
      gtk_svg_set_state (self, g_value_get_uint (value));
      break;

    case PROP_WEIGHT:
      gtk_svg_set_weight (self, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_svg_class_init (GtkSvgClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  filter_types_init ();
  shape_types_init ();
  shape_attrs_init ();
  shape_attrs_init_default_values ();

  object_class->dispose = gtk_svg_dispose;
  object_class->get_property = gtk_svg_get_property;
  object_class->set_property = gtk_svg_set_property;

  /**
   * GtkSvg:resource:
   *
   * Resource to load SVG data from.
   *
   * This property is meant to create a paintable
   * from a resource in ui files.
   *
   * Since: 4.22
   */
  properties[PROP_RESOURCE] =
    g_param_spec_string ("resource", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSvg:features:
   *
   * Enabled features for this paintable.
   *
   * Note that features have to be set before
   * loading SVG data to take effect.
   *
   * Since: 4.22
   */
  properties[PROP_FEATURES] =
    g_param_spec_flags ("features", NULL, NULL,
                        GTK_TYPE_SVG_FEATURES, GTK_SVG_DEFAULT_FEATURES,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSvg:playing:
   *
   * Whether the paintable is currently animating its content.
   *
   * To set this property, use the [method@Gtk.Svg.play] and
   * [method@Gtk.Svg.pause] functions.
   *
   * Since: 4.22
   */
  properties[PROP_PLAYING] =
    g_param_spec_boolean ("playing", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSvg:weight:
   *
   * If not set to -1, this value overrides the weight used
   * when rendering the paintable.
   *
   * Since: 4.22
   */
  properties[PROP_WEIGHT] =
    g_param_spec_double ("weight", NULL, NULL,
                         -1, 1000, -1,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSvg:state:
   *
   * The current state of the renderer.
   *
   * This can be a number between 0 and 63, or the special value
   * `(unsigned int) -1` to indicate the 'empty' state in which
   * nothing is drawn.
   *
   * Since: 4.22
   */
  properties[PROP_STATE] =
    g_param_spec_uint ("state", NULL, NULL,
                       0, G_MAXUINT, GTK_SVG_STATE_EMPTY,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  /**
   * GtkSvg::error:
   * @svg: the SVG paintable
   * @error: the error
   *
   * Signals that an error occurred.
   *
   * Errors can occur both during parsing and during rendering.
   *
   * The expected error values are in the [error@Gtk.SvgError] enumeration,
   * context information about the location of parsing errors can
   * be obtained with the various `gtk_svg_error` functions.
   *
   * Parsing errors are never fatal, so the parsing will resume after
   * the error. Errors may however cause parts of the given data or
   * even all of it to not be parsed at all. So it is a useful idea
   * to check that the parsing succeeds by connecting to this signal.
   *
   * ::: note
   *     This signal is emitted in the middle of parsing or rendering,
   *     and if you handle it, you must be careful. Logging the errors
   *     you receive is fine, but modifying the widget hierarchy or
   *     changing the paintable state definitively isn't.
   *
   *     If in doubt, defer to an idle.
   *
   * Since: 4.22
   */
  error_signal =
    g_signal_new ("error",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, G_TYPE_ERROR);
  g_signal_set_va_marshaller (error_signal,
                              G_TYPE_FROM_CLASS (object_class),
                              g_cclosure_marshal_VOID__BOXEDv);

}

/* }}} */
/* {{{ Debug tools */

#ifdef DEBUG
static void
shape_dump_animation_state (Shape *shape, GString *string)
{
  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          Animation *a = g_ptr_array_index (shape->animations, i);

          if (a->status == ANIMATION_STATUS_RUNNING)
            g_string_append_printf (string, " %s", a->id);
        }
    }

  if (shape_type_has_shapes (shape->type))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          Shape *s = g_ptr_array_index (shape->shapes, i);
          shape_dump_animation_state (s, string);
        }
    }
}

static void
animation_state_dump (GtkSvg *self)
{
  GString *s = g_string_new ("running");
  shape_dump_animation_state (self->content, s);
  dbg_print ("times", "%s\n", s->str);
  g_string_free (s, TRUE);
}

static void
timeline_dump (Timeline *timeline)
{
  GString *s = g_string_new ("Timeline:\n");
  for (unsigned int i = 0; i < timeline->times->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (timeline->times, i);
      g_string_append (s, "  ");
      time_spec_print (spec, s);
      g_string_append (s, "\n");
    }
  g_print ("%s", s->str);
  g_string_free (s, TRUE);
}
#endif

/* }}} */
/* {{{ Private API */

GtkSvg *
gtk_svg_copy (GtkSvg *orig)
{
  GBytes *bytes = NULL;
  GtkSvg *svg;

  bytes = gtk_svg_serialize_full (orig, NULL, 0, GTK_SVG_SERIALIZE_DEFAULT);
  svg = gtk_svg_new_from_bytes (bytes);
  g_bytes_unref (bytes);

  gtk_svg_set_weight (svg, gtk_svg_get_weight (orig));
  gtk_svg_set_state (svg, gtk_svg_get_state (orig));

  return svg;
}

static gboolean
color_stop_equal (ColorStop *stop1,
                  ColorStop *stop2)
{
  for (unsigned int i = 0; i < N_STOP_ATTRS; i++)
    {
      if (!svg_value_equal (stop1->base[i], stop2->base[i]))
        return FALSE;
    }

  return TRUE;
}

static gboolean
frame_equal (Frame *f1,
             Frame *f2)
{
  return svg_value_equal (f1->value, f2->value) &&
         f1->time == f2->time &&
         f1->point == f2->point &&
         memcmp (f1->params, f2->params, sizeof (double) * 4) == 0;
}

static gboolean
animation_equal (Animation *a1,
                 Animation *a2)
{
  if (a1->type != a2->type ||
      g_strcmp0 (a1->id, a2->id) != 0 ||
      a1->attr != a2->attr ||
      a1->simple_duration != a2->simple_duration ||
      a1->repeat_count != a2->repeat_count ||
      a1->repeat_duration != a2->repeat_duration ||
      a1->fill != a2->fill ||
      a1->restart != a2->restart ||
      a1->additive != a2->additive ||
      a1->accumulate != a2->accumulate ||
      a1->calc_mode != a2->calc_mode ||
      a1->n_frames != a2->n_frames)
    return FALSE;

  for (unsigned int i = 0; i < a1->n_frames; i++)
    {
      if (!frame_equal (&a1->frames[i], &a2->frames[i]))
        return FALSE;
    }

  if (a1->type == ANIMATION_TYPE_MOTION)
    {
      if (g_strcmp0 (a1->motion.path_ref, a2->motion.path_ref) != 0 ||
          !(a1->motion.path == a2->motion.path ||
            (a1->motion.path && a2->motion.path && gsk_path_equal (a1->motion.path, a2->motion.path))) ||
          a1->motion.rotate != a2->motion.rotate ||
          a1->motion.angle != a2->motion.angle)
        return FALSE;
    }

  return TRUE;
}

static gboolean
shape_equal (Shape *shape1,
             Shape *shape2)
{
  if (shape1->type != shape2->type ||
      g_strcmp0 (shape1->id, shape2->id) != 0)
    return FALSE;

  for (unsigned int i = 0; i < N_SHAPE_ATTRS; i++)
    {
      if (!svg_value_equal (shape1->base[i], shape2->base[i]))
        return FALSE;
    }

  if (shape_type_has_shapes (shape1->type))
    {
      if (shape1->shapes->len != shape2->shapes->len)
        return FALSE;

      for (unsigned int i = 0; i < shape1->shapes->len; i++)
        {
          Shape *s1 = g_ptr_array_index (shape1->shapes, i);
          Shape *s2 = g_ptr_array_index (shape2->shapes, i);

          if (!shape_equal (s1, s2))
            return FALSE;
        }
    }

  if (shape_type_has_color_stops (shape1->type))
    {
      if (shape1->color_stops->len != shape2->color_stops->len)
        return FALSE;

      for (unsigned int i = 0; i < shape1->color_stops->len; i++)
        {
          ColorStop *s1 = g_ptr_array_index (shape1->color_stops, i);
          ColorStop *s2 = g_ptr_array_index (shape2->color_stops, i);

          if (!color_stop_equal (s1, s2))
            return FALSE;
        }
    }

  if (shape1->animations || shape2->animations)
    {
      if (!shape1->animations || !shape2->animations)
        return FALSE;

      if (shape1->animations->len != shape2->animations->len)
        return FALSE;

      for (unsigned int i = 0; i < shape1->animations->len; i++)
        {
          Animation *a1 = g_ptr_array_index (shape1->animations, i);
          Animation *a2 = g_ptr_array_index (shape2->animations, i);

          if (!animation_equal (a1, a2))
            return FALSE;
        }
    }

  return TRUE;
}

gboolean
gtk_svg_equal (GtkSvg *svg1,
               GtkSvg *svg2)
{
  if (svg1 == svg2)
    return TRUE;

  if (svg1->gpa_version != svg2->gpa_version ||
      g_strcmp0 (svg1->author, svg2->author) != 0 ||
      g_strcmp0 (svg1->license, svg2->license) != 0 ||
      g_strcmp0 (svg1->description, svg2->description) != 0 ||
      g_strcmp0 (svg1->keywords, svg2->keywords) != 0)
    return FALSE;

  return shape_equal (svg1->content, svg2->content);
}

/* {{{ Animation */

static void
collect_next_update_for_animation (Animation     *a,
                                   int64_t        current_time,
                                   GtkSvgRunMode *run_mode,
                                   int64_t       *next_update)
{
  animation_update_run_mode (a, current_time);

#ifdef DEBUG
  if (a->run_mode > *run_mode)
    {
      const char *mode_name[] = { "STOPPED", "DISCRETE", "CONTINUOUS" };
      dbg_print ("run", "%s updates run mode to %s\n", a->id, mode_name[a->run_mode]);
    }
  if (a->next_invalidate < *next_update)
    {
      dbg_print ("run", "%s updates next update to %s\n", a->id, format_time (a->next_invalidate));
    }
#endif

  *run_mode = MAX (*run_mode, a->run_mode);
  *next_update = MIN (*next_update, a->next_invalidate);

  if (a->state_changed)
    {
      *next_update = current_time;
      a->state_changed = FALSE;
    }
}

static void
collect_next_update_for_shape (Shape         *shape,
                               int64_t        current_time,
                               GtkSvgRunMode *run_mode,
                               int64_t       *next_update)
{
  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          Animation *a = g_ptr_array_index (shape->animations, i);
          collect_next_update_for_animation (a, current_time, run_mode, next_update);
        }
    }

  if (shape_type_has_shapes (shape->type))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          Shape *s = g_ptr_array_index (shape->shapes, i);
          collect_next_update_for_shape (s, current_time, run_mode, next_update);
        }
    }
}

static void
collect_next_update (GtkSvg *self)
{
  GtkSvgRunMode run_mode = GTK_SVG_RUN_MODE_STOPPED;
  int64_t next_update = INDEFINITE;

  collect_next_update_for_shape (self->content, self->current_time, &run_mode, &next_update);

  self->run_mode = run_mode;
  self->next_update = next_update;

#ifdef DEBUG
  const char *mode_name[] = { "STOPPED", "DISCRETE", "CONTINUOUS" };
  dbg_print ("run", "run mode %s\n", mode_name[self->run_mode]);
  dbg_print ("run", "next update %s\n", format_time (self->next_update));
#endif
}

static void
invalidate_for_next_update (GtkSvg *self)
{
  if (self->next_update <= self->current_time ||
      self->run_mode == GTK_SVG_RUN_MODE_CONTINUOUS)
    gtk_svg_invalidate_contents (self);
}

static void
shape_update_animation_state (Shape   *shape,
                              int64_t  current_time)
{
  if (shape->animations)
    {
      gboolean any_changed = FALSE;

      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          Animation *a = g_ptr_array_index (shape->animations, i);
          animation_update_state (a, current_time);
          any_changed |= a->state_changed;
        }

      if (any_changed)
        g_ptr_array_sort_values (shape->animations, compare_anim);
    }

  if (shape_type_has_shapes (shape->type))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          Shape *s = g_ptr_array_index (shape->shapes, i);
          shape_update_animation_state (s, current_time);
        }
    }
}

static void
update_animation_state (GtkSvg *self)
{
  shape_update_animation_state (self->content, self->current_time);
}

/*< private>
 * gtk_svg_set_load_time:
 * @self: an SVG paintable
 * @load_time: the load time
 *
 * Sets the load time of the SVG, which marks the
 * 'beginning of time' for any animations defined in
 * it.
 *
 * @load_time must be in microseconds and in the same
 * timescale as the times returned by [class@Gdk.FrameClock].
 *
 * Since: 4.22
 */
void
gtk_svg_set_load_time (GtkSvg  *self,
                       int64_t  load_time)
{
  g_return_if_fail (GTK_IS_SVG (self));
  g_return_if_fail (self->load_time == INDEFINITE);

  self->load_time = load_time;
  self->current_time = load_time;
  if (!self->playing)
    self->pause_time = load_time;

#ifdef DEBUG
  time_base = self->load_time;
  if (g_getenv ("SVG_DEBUG"))
    timeline_dump (self->timeline);
#endif

  timeline_set_load_time (self->timeline, load_time);

  update_animation_state (self);
  collect_next_update (self);
  invalidate_for_next_update (self);
  schedule_next_update (self);
}

/*< private >
 * gtk_svg_advance:
 * @self: an SVG paintable
 * @current_time: the time to advance to
 *
 * Advances the animation to the given value,
 * which must be in microseconds and in the same
 * timescale as the times returned by [class@Gdk.FrameClock].
 *
 * Note that this function is only useful when *not*
 * running the animations automatically via
 * [method@Gtk.Svg.play].
 *
 * Also note that moving time backwards is not
 * supported and may lead to unexpected results.
 *
 * Since: 4.22
 */
void
gtk_svg_advance (GtkSvg  *self,
                 int64_t  current_time)
{
  g_return_if_fail (GTK_IS_SVG (self));
  g_return_if_fail (self->load_time < INDEFINITE);
  g_return_if_fail (self->current_time <= current_time);

  dbg_print ("run", "advancing current time to %s\n", format_time (current_time));

  self->current_time = current_time;

  update_animation_state (self);
  collect_next_update (self);
  invalidate_for_next_update (self);
  schedule_next_update (self);

#ifdef DEBUG
  animation_state_dump (self);
#endif
}

/*< private >
 * gtk_svg_get_run_mode:
 * @self: an SVG paintable
 *
 * Returns the current 'run mode' of the animation.
 *
 * A 'continuous' run mode indicates that animations
 * are expected to provide different content for every
 * frame, in which case it is most practical to advance
 * the animation in a tick callback.
 *
 * A 'discrete' run mode indicates that it makes sense
 * to advance the animations in larger steps, using a
 * timeout. See [method@Gtk.Svg.get_next_update] for
 * determining how long the timeout should be.
 *
 * Note that this function is only useful when *not*
 * running the animations automatically via [method@Gtk.Svg.play].
 *
 * Since: 4.22
 */
GtkSvgRunMode
gtk_svg_get_run_mode (GtkSvg *self)
{
  return self->run_mode;
}

/*< private >
 * gtk_svg_get_next_update:
 * @self: an SVG paintable
 *
 * Returns the next time at which animations are
 * expected to provide different content.
 *
 * An economic way of handling the update is to schedule
 * a timeout for that time, and advance the animations then.
 *
 * Since: 4.22
 */
int64_t
gtk_svg_get_next_update (GtkSvg *self)
{
  return self->next_update;
}
/* }}} */
/* {{{ Serialization */

/* copied from gtksymbolicpaintable.c */
static const GdkRGBA *
pad_colors (GdkRGBA        col[5],
            const GdkRGBA *colors,
            guint          n_colors)
{
  GdkRGBA default_colors[5] = {
    [GTK_SYMBOLIC_COLOR_FOREGROUND] = { 0.745, 0.745, 0.745, 1.0 },
    [GTK_SYMBOLIC_COLOR_ERROR] = { 0.797, 0, 0, 1.0 },
    [GTK_SYMBOLIC_COLOR_WARNING] = { 0.957, 0.473, 0.242, 1.0 },
    [GTK_SYMBOLIC_COLOR_SUCCESS] = { 0.305, 0.602, 0.023, 1.0 },
    [GTK_SYMBOLIC_COLOR_ACCENT] = { 0.208, 0.518, 0.894, 1.0 },
  };

  memcpy (col, default_colors, sizeof (GdkRGBA) * 5);

  if (n_colors != 0)
    memcpy (col, colors, sizeof (GdkRGBA) * MIN (5, n_colors));

  return col;
}

/*< private >
 * gtk_svg_serialize_full:
 * @self: an SVG paintable
 * @colors (array length=n_colors): a pointer to an array of colors
 * @n_colors: the number of colors
 * @flags: flags that influence what content is
 *   included in the serialization
 *
 * Serializes the content of the renderer as SVG.
 *
 * The SVG will be similar to the orignally loaded one,
 * but is not guaranteed to be 100% identical.
 *
 * The @flags argument can be used together with
 * [method@Gtk.Svg.advance] to reproduce content
 * for specific times. For producing still images that
 * represent individual frames of the animation, you
 * have to exclude the animation elements, since their
 * values would otherwise interfere.
 *
 * Note that the 'shadow DOM' optionally includes some
 * custom attributes that represent internal state, e.g.
 * a 'gpa:status' attribute on animation elements that
 * reflects whether they are currently running.
 *
 * When serializing the 'shadow DOM', the viewport from the
 * most recently drawn frame is used to resolve percentages.
 *
 * Returns: the serialized contents
 *
 * Since: 4.22
 */
GBytes *
gtk_svg_serialize_full (GtkSvg               *self,
                        const GdkRGBA        *colors,
                        size_t                n_colors,
                        GtkSvgSerializeFlags  flags)
{
  GString *s = g_string_new ("");

  if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
    {
      GdkRGBA real_colors[5];
      const GdkRGBA *col = colors;
      size_t n_col = n_colors;

      if (n_colors >= 5)
        {
          col = colors;
          n_col = n_colors;
        }
      else
        {
          col = pad_colors (real_colors, colors, n_colors);
          n_col = 5;
        }

      ComputeContext context;
      context.svg = self;
      context.viewport = NULL;
      context.current_time = self->current_time;
      context.parent = NULL;
      context.colors = col;
      context.n_colors = n_col;

      compute_current_values_for_shape (self->content, &context);
    }

  g_string_append (s, "<svg");

  indent_for_attr (s, 0);
  g_string_append (s, "xmlns='http://www.w3.org/2000/svg'");
  indent_for_attr (s, 0);
  g_string_append (s, "xmlns:svg='http://www.w3.org/2000/svg'");
  if (self->keywords || self->description || self->author || self->license)
    {
      /* we only need these to write out keywords or description
       * in a way that inkscape understands
       */
      indent_for_attr (s, 0);
      g_string_append (s, "xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'");
      indent_for_attr (s, 0);
      g_string_append (s, "xmlns:cc='http://creativecommons.org/ns#'");
      indent_for_attr (s, 0);
      g_string_append (s, "xmlns:dc='http://purl.org/dc/elements/1.1/'");
    }

  if (self->gpa_version > 0 || (flags & GTK_SVG_SERIALIZE_INCLUDE_STATE))
    {
      indent_for_attr (s, 0);
      g_string_append (s, "xmlns:gpa='https://www.gtk.org/grappa'");
      indent_for_attr (s, 0);
      g_string_append_printf (s, "gpa:version='%u'", MAX (self->gpa_version, 1));
      indent_for_attr (s, 0);
      if (self->state == GTK_SVG_STATE_EMPTY)
        g_string_append (s, "gpa:state='empty'");
      else
        g_string_append_printf (s, "gpa:state='%u'", self->state);
    }

  if (flags & GTK_SVG_SERIALIZE_INCLUDE_STATE)
    {
      indent_for_attr (s, 0);
      string_append_double (s,
                            "gpa:state-change-delay='",
                            (self->state_change_delay) / (double) G_TIME_SPAN_MILLISECOND);
      g_string_append (s, "ms'");
      if (self->load_time != INDEFINITE)
        {
          indent_for_attr (s, 0);
          string_append_double (s,
                                "gpa:time-since-load='",
                                (self->current_time - self->load_time) / (double) G_TIME_SPAN_MILLISECOND);
          g_string_append (s, "ms'");
        }
    }

  serialize_shape_attrs (s, self, 0, self->content, flags);
  g_string_append (s, ">");

  if (self->keywords || self->description || self->author || self->license)
    {
      indent_for_elt (s, 2);
      g_string_append (s, "<metadata>");
      indent_for_elt (s, 4);
      g_string_append (s, "<rdf:RDF>");
      indent_for_elt (s, 6);
      g_string_append (s, "<cc:Work>");
      if (self->license)
        {
          indent_for_elt (s, 8);
          g_string_append (s, "<cc:license");
          indent_for_attr (s, 8);
          g_string_append (s, "rdf:resource='");
          g_string_append (s, self->license);
          g_string_append (s, "'/>");
        }
      if (self->author)
        {
          indent_for_elt (s, 8);
          g_string_append (s, "<dc:creator>");
          indent_for_elt (s, 10);
          g_string_append (s, "<cc:Agent>");
          indent_for_elt (s, 12);
          g_string_append_printf (s, "<dc:title>%s</dc:title>", self->author);
          indent_for_elt (s, 10);
          g_string_append (s, "</cc:Agent>");
          indent_for_elt (s, 8);
          g_string_append (s, "</dc:creator>");
        }
      if (self->description)
        {
          indent_for_elt (s, 8);
          g_string_append_printf (s, "<dc:description>%s</dc:description>", self->description);
        }
      if (self->keywords)
        {
          indent_for_elt (s, 8);
          g_string_append (s, "<dc:subject>");
          indent_for_elt (s, 10);
          g_string_append (s, "<rdf:Bag>");
          indent_for_elt (s, 12);
          g_string_append_printf (s, "<rdf:li>%s</rdf:li>", self->keywords);
          indent_for_elt (s, 10);
          g_string_append (s, "</rdf:Bag>");
          indent_for_elt (s, 8);
          g_string_append (s, "</dc:subject>");
        }
      indent_for_elt (s, 6);
      g_string_append (s, "</cc:Work>");
      indent_for_elt (s, 4);
      g_string_append (s, "</rdf:RDF>");
      indent_for_elt (s, 2);
      g_string_append (s, "</metadata>");
    }

  if (self->font_files)
    {
      for (unsigned int i = 0; i < self->font_files->len; i++)
        {
          const char *file;
          char *data;
          gsize len;
          GBytes *bytes;

          file = g_ptr_array_index (self->font_files, i);

          if (!g_file_get_contents (file, &data, &len, NULL))
            continue;

          bytes = g_bytes_new_take (data, len);

          indent_for_elt (s, 2);
          g_string_append (s, "<font-face>");
          indent_for_elt (s, 4);
          g_string_append (s, "<font-face-src>");
          indent_for_elt (s, 6);
          g_string_append (s, "<font-face-uri");
          indent_for_attr (s, 6);
          g_string_append (s, "href='");
          append_bytes_href (s, bytes, "font/ttf");
          g_string_append (s, "'/>");
          indent_for_elt (s, 4);
          g_string_append (s, "</font-face-src>");
          indent_for_elt (s, 2);
          g_string_append (s, "</font-face>");

          g_bytes_unref (bytes);
        }
    }

  serialize_shape (s, self, 0, self->content, flags);
  g_string_append (s, "\n</svg>\n");

  return g_string_free_to_bytes (s);
}

/* }}} */
/* {{{ Getters and setters */

unsigned int
svg_transform_get_n_transforms (const SvgValue *value)
{
  SvgTransform *tf = (SvgTransform *) value;

  return tf->n_transforms;
}

SvgValue *
svg_transform_get_transform (const SvgValue *value,
                             unsigned int    pos)
{
  SvgTransform *tf = (SvgTransform *) value;

  switch (tf->transforms[pos].type)
    {
    case TRANSFORM_NONE:
      return svg_transform_new_none ();
    case TRANSFORM_TRANSLATE:
      return svg_transform_new_translate (tf->transforms[pos].translate.x,
                                          tf->transforms[pos].translate.y);
    case TRANSFORM_SCALE:
      return svg_transform_new_scale (tf->transforms[pos].scale.x,
                                      tf->transforms[pos].scale.y);
    case TRANSFORM_ROTATE:
      return svg_transform_new_rotate (tf->transforms[pos].rotate.angle,
                                       tf->transforms[pos].rotate.x,
                                       tf->transforms[pos].rotate.y);
    case TRANSFORM_SKEW_X:
      return svg_transform_new_skew_x (tf->transforms[pos].skew_x.angle);
    case TRANSFORM_SKEW_Y:
      return svg_transform_new_skew_y (tf->transforms[pos].skew_y.angle);
    case TRANSFORM_MATRIX:
      return svg_transform_new_matrix (tf->transforms[pos].matrix.m);
    default:
      g_assert_not_reached ();
    }
}

TransformType
svg_transform_get_primitive (const SvgValue *value,
                             unsigned int    pos,
                             double          params[6])
{
  SvgTransform *tf = (SvgTransform *) value;

  switch (tf->transforms[pos].type)
    {
    case TRANSFORM_NONE:
      break;
    case TRANSFORM_TRANSLATE:
      params[0] = tf->transforms[pos].translate.x;
      params[1] = tf->transforms[pos].translate.y;
      break;
    case TRANSFORM_SCALE:
      params[0] = tf->transforms[pos].scale.x;
      params[1] = tf->transforms[pos].scale.y;
      break;
    case TRANSFORM_ROTATE:
      params[0] = tf->transforms[pos].rotate.angle;
      params[1] = tf->transforms[pos].rotate.x;
      params[2] = tf->transforms[pos].rotate.y;
      break;
    case TRANSFORM_SKEW_X:
      params[0] = tf->transforms[pos].skew_x.angle;
      break;
    case TRANSFORM_SKEW_Y:
      params[0] = tf->transforms[pos].skew_y.angle;
      break;
    case TRANSFORM_MATRIX:
      memcpy (params, tf->transforms[pos].matrix.m, sizeof (double) * 6);
      break;
    default:
      g_assert_not_reached ();
    }

  return tf->transforms[pos].type;
}

double
svg_shape_attr_get_number (Shape                 *shape,
                           ShapeAttr              attr,
                           const graphene_rect_t *viewport)
{
  g_return_val_if_fail (shape_has_attr (shape->type, attr), 0);
  SvgValue *value;

  if (_gtk_bitmask_get (shape->attrs, attr))
    value = shape_ref_base_value (shape, NULL, attr, 0);
  else
    value = shape_attr_ref_initial_value (attr, shape->type, shape->parent != NULL);

  if (svg_value_equal (value, svg_auto_new ()))
    {
      if (attr == SHAPE_ATTR_RX || attr == SHAPE_ATTR_RY)
        return 0;
      else if (attr == SHAPE_ATTR_WIDTH)
        return viewport->size.width;
      else if (attr == SHAPE_ATTR_HEIGHT)
        return viewport->size.height;
      else
        g_assert_not_reached ();
    }

  switch ((unsigned int) attr)
    {
    case SHAPE_ATTR_X:
    case SHAPE_ATTR_X1:
    case SHAPE_ATTR_X2:
    case SHAPE_ATTR_WIDTH:
    case SHAPE_ATTR_RX:
    case SHAPE_ATTR_CX:
      g_assert (viewport);
      return svg_number_get (value, viewport->size.width);
    case SHAPE_ATTR_Y:
    case SHAPE_ATTR_Y1:
    case SHAPE_ATTR_Y2:
    case SHAPE_ATTR_HEIGHT:
    case SHAPE_ATTR_RY:
    case SHAPE_ATTR_CY:
      g_assert (viewport);
      return svg_number_get (value, viewport->size.height);
    case SHAPE_ATTR_R:
      g_assert (viewport);
      return svg_number_get (value, normalized_diagonal (viewport));
    case SHAPE_ATTR_STROKE_WIDTH:
    case SHAPE_ATTR_STROKE_MITERLIMIT:
    case SHAPE_ATTR_STROKE_OPACITY:
    case SHAPE_ATTR_FILL_OPACITY:
    case SHAPE_ATTR_OPACITY:
      return svg_number_get (value, 1);
    case SHAPE_ATTR_STROKE_MINWIDTH:
    case SHAPE_ATTR_STROKE_MAXWIDTH:
      return svg_number_get (value, 1);
    default:
      g_assert_not_reached ();
    }

  svg_value_unref (value);

  return 0;
}

GskPath *
svg_shape_attr_get_path (Shape     *shape,
                         ShapeAttr  attr)
{
  g_return_val_if_fail (shape_has_attr (shape->type, attr), NULL);
  SvgValue *value;
  GskPath *path;

  if (_gtk_bitmask_get (shape->attrs, attr))
    value = shape_ref_base_value (shape, NULL, attr, 0);
  else
    value = shape_attr_ref_initial_value (attr, shape->type, shape->parent != NULL);

  path = svg_path_get_gsk (value);
  if (path)
    gsk_path_ref (path);
  else
    path = gsk_path_builder_free_to_path (gsk_path_builder_new ());

  svg_value_unref (value);

  return path;
}

unsigned int
svg_shape_attr_get_enum (Shape     *shape,
                         ShapeAttr  attr)
{
  g_return_val_if_fail (shape_has_attr (shape->type, attr), 0);
  SvgValue *value;
  unsigned int ret;

  if (_gtk_bitmask_get (shape->attrs, attr))
    value = shape_ref_base_value (shape, NULL, attr, 0);
  else
    value = shape_attr_ref_initial_value (attr, shape->type, shape->parent != NULL);

  ret = svg_enum_get (value);

  svg_value_unref (value);

  return ret;
}

PaintKind
svg_shape_attr_get_paint (Shape            *shape,
                          ShapeAttr         attr,
                          GtkSymbolicColor *symbolic,
                          GdkRGBA          *color)
{
  g_return_val_if_fail (shape_has_attr (shape->type, attr), PAINT_NONE);
  SvgValue *value;
  SvgPaint *paint;

  if (_gtk_bitmask_get (shape->attrs, attr))
    value = shape_ref_base_value (shape, NULL, attr, 0);
  else
    value = shape_attr_ref_initial_value (attr, shape->type, shape->parent != NULL);

  paint = (SvgPaint *) value;

  *symbolic = 0xffff;
  *color = (GdkRGBA) { 0, 0, 0, 1 };

  switch (paint->kind)
    {
    case PAINT_NONE:
    case PAINT_CONTEXT_FILL:
    case PAINT_CONTEXT_STROKE:
    case PAINT_CURRENT_COLOR:
    case PAINT_SERVER:
    case PAINT_SERVER_WITH_FALLBACK:
    case PAINT_SERVER_WITH_CURRENT_COLOR:
      break;
    case PAINT_SYMBOLIC:
      *symbolic = paint->symbolic;
      break;
    case PAINT_COLOR:
      *color = *(const GdkRGBA *) paint->color.values;
      break;
    default:
      g_assert_not_reached ();
    }

  svg_value_unref (value);

  return paint->kind;
}

double *
svg_shape_attr_get_points (Shape        *shape,
                           ShapeAttr     attr,
                           unsigned int *n_params)
{
  g_return_val_if_fail (shape_has_attr (shape->type, attr), NULL);
  SvgValue *value;
  SvgNumbers *numbers;
  double *ret;

  if (_gtk_bitmask_get (shape->attrs, attr))
    value = shape_ref_base_value (shape, NULL, attr, 0);
  else
    value = shape_attr_ref_initial_value (attr, shape->type, shape->parent != NULL);

  numbers = (SvgNumbers *) value;

  *n_params = numbers->n_values;

  ret = g_new0 (double, numbers->n_values);

  for (unsigned int i = 0; i < numbers->n_values; i++)
    {
      /* FIXME: What about the dimension ? */
      ret[i] = numbers->values[i].value;
    }

  svg_value_unref (value);

  return ret;
}

GskPath *
svg_shape_get_path (Shape                 *shape,
                    const graphene_rect_t *viewport)
{
  return shape_get_path (shape, viewport, FALSE);
}

ClipKind
svg_shape_attr_get_clip (Shape      *shape,
                         ShapeAttr   attr,
                         GskPath   **path)
{
  g_return_val_if_fail (shape_has_attr (shape->type, attr), CLIP_NONE);
  SvgValue *value;
  SvgClip *clip;

  if (_gtk_bitmask_get (shape->attrs, attr))
    value = shape_ref_base_value (shape, NULL, attr, 0);
  else
    value = shape_attr_ref_initial_value (attr, shape->type, shape->parent != NULL);

  clip = (SvgClip *) value;

  if (clip->kind == CLIP_PATH)
    *path = clip->path.path;
  else
    *path = NULL;

  svg_value_unref (value);

  return clip->kind;
}

char *
svg_shape_attr_get_transform (Shape     *shape,
                              ShapeAttr  attr)
{
  g_return_val_if_fail (shape_has_attr (shape->type, attr), NULL);
  SvgValue *value;
  GString *s = g_string_new ("");

  if (_gtk_bitmask_get (shape->attrs, attr))
    value = shape_ref_base_value (shape, NULL, attr, 0);
  else
    value = shape_attr_ref_initial_value (attr, shape->type, shape->parent != NULL);

  svg_value_print (value, s);
  svg_value_unref (value);

  return g_string_free (s, FALSE);
}

char *
svg_shape_attr_get_filter (Shape     *shape,
                           ShapeAttr  attr)
{
  g_return_val_if_fail (shape_has_attr (shape->type, attr), NULL);
  SvgValue *value;
  GString *s = g_string_new ("");

  if (_gtk_bitmask_get (shape->attrs, attr))
    value = shape_ref_base_value (shape, NULL, attr, 0);
  else
    value = shape_attr_ref_initial_value (attr, shape->type, shape->parent != NULL);

  svg_value_print (value, s);
  svg_value_unref (value);

  return g_string_free (s, FALSE);
}

void
svg_shape_attr_set (Shape     *shape,
                    ShapeAttr  attr,
                    SvgValue  *value)
{
  g_return_if_fail (value != NULL);

  if (_gtk_bitmask_get (shape->attrs, attr))
    svg_value_unref (shape->base[attr]);
  shape->base[attr] = value;
  shape->attrs = _gtk_bitmask_set (shape->attrs, attr, TRUE);
}

Shape *
svg_shape_add (Shape     *parent,
               ShapeType  type)
{
  Shape *shape = shape_new (parent, type);

  g_ptr_array_add (parent->shapes, shape);

  if (shape_type_has_text (parent->type) && type == SHAPE_TSPAN)
    {
      TextNode node = {
        .type = TEXT_NODE_SHAPE,
        .shape = { .shape = shape }
      };
      g_array_append_val (parent->text, node);
    }

  return shape;
}

void
svg_shape_delete (Shape *shape)
{
  if (shape->text)
    for (guint i = 0; i < shape->text->len; i++)
      {
        TextNode *node = &g_array_index (shape->text, TextNode, i);
        if (node->type == TEXT_NODE_SHAPE && node->shape.shape == shape)
          {
            g_array_remove_index (shape->text, i);
            break;
          }
      }

  g_ptr_array_remove (shape->parent->shapes, shape);
}

/* }}} */

/*< private>
 * gtk_svg_set_playing:
 * @self: an SVG paintable
 * @playing: the new state
 *
 * Sets whether the paintable is animating its content.
 */
void
gtk_svg_set_playing (GtkSvg   *self,
                     gboolean  playing)
{
  int64_t current_time;

  if (self->playing == playing)
    return;

  self->playing = playing;

  if (self->clock)
    current_time = gdk_frame_clock_get_frame_time (self->clock);
  else
    current_time = 0;

  current_time = MAX (self->current_time, current_time);

  if (self->playing)
    {
      if (self->load_time != INDEFINITE)
        {
          int64_t duration = current_time - self->pause_time;

          /* A simple-minded implementation of pausing:
           * Forward all definite times by the duration of
           * the pause, to make it as if that time never
           * happened.
           */
          if (self->clock)
            self->current_time = current_time;

          self->load_time += duration;

          animations_update_for_pause (self->content, duration);
          timeline_update_for_pause (self->timeline, duration);
          invalidate_for_next_update (self);
          schedule_next_update (self);
        }
      else
        {
          if (self->clock)
            gtk_svg_set_load_time (self, current_time);
        }
    }
  else
    {
      if (self->load_time != INDEFINITE)
        self->pause_time = current_time;

      if (self->clock)
        frame_clock_disconnect (self);

      g_clear_handle_id (&self->pending_invalidate, g_source_remove);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PLAYING]);
}

/*< private >
 * gtk_svg_clear_content:
 * @self: an SVG paintable
 *
 * Resets the paintable to the initial, empty state.
 */
void
gtk_svg_clear_content (GtkSvg *self)
{
  g_clear_pointer (&self->timeline, timeline_free);
  g_clear_pointer (&self->content, shape_free);
  g_clear_pointer (&self->images, g_hash_table_unref);
  g_clear_object (&self->fontmap);
  g_clear_pointer (&self->font_files, g_ptr_array_unref);
  g_clear_pointer (&self->node, gsk_render_node_unref);

  self->content = shape_new (NULL, SHAPE_SVG);
  self->timeline = timeline_new ();
  self->images = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  self->width = 0;
  self->height = 0;
  self->current_width = 0;
  self->current_height = 0;
  self->state = 0;
  self->max_state = 0;
  self->state_change_delay = 0;
  self->used = 0;

  self->gpa_version = 0;
}

/*< private >
 * gtk_svg_set_overflow:
 * @self: an SVG paintable
 * @overflow: the new overflow value
 *
 * Sets whether the rendering will be clipped
 * to the bounds.
 *
 * Clipping is expected for [interface@Gdk.Paintable]
 * semantics, so this property should not be
 * changed when using a `GtkSvg` as a paintable.
 */
void
gtk_svg_set_overflow (GtkSvg      *self,
                      GtkOverflow  overflow)
{
  g_return_if_fail (GTK_IS_SVG (self));

  if (self->overflow == overflow)
    return;

  self->overflow = overflow;
  gtk_svg_invalidate_contents (self);
}

/*< private >
 * gtk_svg_get_overflow:
 * @self: an SVG paintable
 *
 * Gets the current overflow value.
 *
 * Returns: the current overflow value
 */
GtkOverflow
gtk_svg_get_overflow (GtkSvg *self)
{
  g_return_val_if_fail (GTK_IS_SVG (self), GTK_OVERFLOW_HIDDEN);

  return self->overflow;
}

static Shape *
find_filter (Shape      *shape,
             const char *filter_id)
{
  if (shape->type == SHAPE_FILTER)
    {
      if (g_strcmp0 (shape->id, filter_id) == 0)
        return shape;
      else
        return NULL;
    }

  if (shape_type_has_shapes (shape->type))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          Shape *sh = g_ptr_array_index (shape->shapes, i);
          Shape *res;

          res = find_filter (sh, filter_id);
          if (res)
            return res;
        }
    }

  return NULL;
}

GskRenderNode *
gtk_svg_apply_filter (GtkSvg                *svg,
                      const char            *filter_id,
                      const graphene_rect_t *bounds,
                      GskRenderNode         *source)
{
  Shape *filter;
  Shape *shape;
  PaintContext paint_context;
  GskRenderNode *result;
  G_GNUC_UNUSED GskRenderNode *node;

  filter = find_filter (svg->content, filter_id);
  if (!filter)
    return gsk_render_node_ref (source);

  /* This is a bit iffy. We create an extra shape,
   * and treat it as if it was part of the svg.
   */

  shape = shape_new (NULL, SHAPE_RECT);

  shape->valid_bounds = TRUE;
  shape->bounds = *bounds;

  paint_context.svg = svg;
  paint_context.viewport = bounds;
  paint_context.viewport_stack = NULL;
  paint_context.snapshot = gtk_snapshot_new ();
  paint_context.colors = NULL;
  paint_context.n_colors = 0;
  paint_context.weight = 400;
  paint_context.op = RENDERING;
  paint_context.op_stack = NULL;
  paint_context.ctx_shape_stack = NULL;
  paint_context.current_time = svg->current_time;
  paint_context.depth = 0;
  paint_context.transforms = NULL;
  paint_context.instance_count = 0;

  /* This is necessary so the filter has current values.
   * Also, any other part of the svg that the filter might
   * refer to.
   */
  recompute_current_values (svg->content, NULL, &paint_context);

  /* This is necessary, so the shape itself has current values */
  recompute_current_values (shape, NULL, &paint_context);

  result = apply_filter_tree (shape, filter, &paint_context, source);

  shape_free (shape);

  node = gtk_snapshot_free_to_node (paint_context.snapshot);
  g_assert (node == NULL);

  return result;
}

/* }}} */
/* {{{ Public API */
/* {{{ Constructors */

/**
 * gtk_svg_new:
 *
 * Creates a new, empty SVG paintable.
 *
 * Returns: the paintable
 *
 * Since: 4.22
 */
GtkSvg *
gtk_svg_new (void)
{
  return g_object_new (GTK_TYPE_SVG, NULL);
}

/**
 * gtk_svg_new_from_bytes:
 * @bytes: the data
 *
 * Parses the SVG data in @bytes and creates a paintable.
 *
 * Returns: the paintable
 *
 * Since: 4.22
 */
GtkSvg *
gtk_svg_new_from_bytes (GBytes *bytes)
{
  GtkSvg *self = g_object_new (GTK_TYPE_SVG, NULL);

  gtk_svg_init_from_bytes (self, bytes);

  return self;
}

/**
 * gtk_svg_new_from_resource:
 * @path: the resource path
 *
 * Parses the SVG data in the resource and creates a paintable.
 *
 * Returns: the paintable
 *
 * Since: 4.22
 */
GtkSvg *
gtk_svg_new_from_resource (const char *path)
{
  GtkSvg *self = g_object_new (GTK_TYPE_SVG, NULL);

  gtk_svg_init_from_resource (self, path);

  return self;
}

/* }}} */
/* {{{ Loading */

/**
 * gtk_svg_load_from_bytes:
 * @self: an SVG paintable
 * @bytes: the data to load
 *
 * Loads SVG content into an existing SVG paintable.
 *
 * To track errors while loading SVG content,
 * connect to the [signal@Gtk.Svg::error] signal.
 *
 * This clears any previously loaded content.
 *
 * Since: 4.22
 */
void
gtk_svg_load_from_bytes (GtkSvg *self,
                         GBytes *bytes)
{
  g_return_if_fail (GTK_IS_SVG (self));

  gtk_svg_set_playing (self, FALSE);
  gtk_svg_clear_content (self);

  gtk_svg_init_from_bytes (self, bytes);

  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

/**
 * gtk_svg_load_from_resource:
 * @self: an SVG paintable
 * @path: the resource path
 *
 * Loads SVG content into an existing SVG paintable.
 *
 * To track errors while loading SVG content,
 * connect to the [signal@Gtk.Svg::error] signal.
 *
 * This clears any previously loaded content.
 *
 * Since: 4.22
 */
void
gtk_svg_load_from_resource (GtkSvg     *self,
                            const char *path)
{
  g_return_if_fail (GTK_IS_SVG (self));

  g_set_str (&self->resource, path);

  gtk_svg_set_playing (self, FALSE);
  gtk_svg_clear_content (self);

  if (path)
    gtk_svg_init_from_resource (self, path);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RESOURCE]);
}

/* }}} */
/* {{{ Serialization */

/**
 * gtk_svg_serialize:
 * @self: an SVG paintable
 *
 * Serializes the content of the renderer as SVG.
 *
 * The SVG will be similar to the orignally loaded one,
 * but is not guaranteed to be 100% identical.
 *
 * This function serializes the DOM, i.e. the results
 * of parsing the SVG. It does not reflect the effect
 * of applying animations.
 *
 * Returns: the serialized contents
 *
 * Since: 4.22
 */
GBytes *
gtk_svg_serialize (GtkSvg *self)
{
  return gtk_svg_serialize_full (self, NULL, 0, GTK_SVG_SERIALIZE_DEFAULT);
}

/**
 * gtk_svg_write_to_file:
 * @self: an SVG paintable
 * @filename: the file to save to
 * @error: return location for an error
 *
 * Serializes the paintable, and saves the result to a file.
 *
 * Returns: true, unless an error occurred
 *
 * Since: 4.22
 */
gboolean
gtk_svg_write_to_file (GtkSvg      *self,
                       const char  *filename,
                       GError     **error)
{
  GBytes *bytes;
  gboolean ret;

  bytes = gtk_svg_serialize (self);
  ret = g_file_set_contents (filename,
                             g_bytes_get_data (bytes, NULL),
                             g_bytes_get_size (bytes),
                             error);
  g_bytes_unref (bytes);

  return ret;
}

/* }}} */
/* {{{ Setters and getters */

/**
 * gtk_svg_set_weight:
 * @self: an SVG paintable
 * @weight: the font weight, as a value between -1 and 1000
 *
 * Sets the weight that is used when rendering.
 *
 * The default value of -1 means to use the font weight
 * from CSS.
 *
 * Since: 4.22
 */
void
gtk_svg_set_weight (GtkSvg *self,
                    double  weight)
{
  g_return_if_fail (GTK_IS_SVG (self));
  g_return_if_fail (-1 <= weight && weight <= 1000);

  if (self->weight == weight)
    return;

  self->weight = weight;

  gtk_svg_invalidate_contents (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WEIGHT]);
}

/**
 * gtk_svg_get_weight:
 * @self: an SVG paintable
 *
 * Gets the value of the weight property.
 *
 * Returns: the weight
 *
 * Since: 4.22
 */
double
gtk_svg_get_weight (GtkSvg *self)
{
  g_return_val_if_fail (GTK_IS_SVG (self), -1);

  return self->weight;
}

/**
 * gtk_svg_set_state:
 * @self: an SVG paintable
 * @state: the state to set, as a value between 0 and 63,
 *   or `(unsigned int) -1`
 *
 * Sets the state of the paintable.
 *
 * Use [method@Gtk.Svg.get_n_states] to find out
 * what states @self has.
 *
 * If the paintable is currently playing, the state change
 * will apply transitions that are defined in the SVG. If
 * the paintable is not playing, the state change will take
 * effect instantaneously.
 *
 * Since: 4.22
 */
void
gtk_svg_set_state (GtkSvg       *self,
                   unsigned int  state)
{
  unsigned int previous_state;

  g_return_if_fail (GTK_IS_SVG (self));
  g_return_if_fail (state == GTK_SVG_STATE_EMPTY || state <= 63);

  if (self->state == state)
    return;

  if (self->clock && self->playing)
    self->current_time = MAX (self->current_time, gdk_frame_clock_get_frame_time (self->clock));

  previous_state = self->state;

  self->state = state;

  if ((self->features & GTK_SVG_EXTENSIONS) == 0)
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
      return;
    }

  if ((self->features & GTK_SVG_ANIMATIONS) == 0)
    {
      if (self->gpa_version > 0)
        {
          apply_state (self->content, state);
          gtk_svg_invalidate_contents (self);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
      return;
    }

  /* Don't jiggle things while we're still loading */
  if (self->load_time != INDEFINITE)
    {
      dbg_print ("state", "renderer state %u -> %u\n", previous_state, state);

      timeline_update_for_state (self->timeline,
                                 previous_state, self->state,
                                 self->current_time + self->state_change_delay);

      update_animation_state (self);
      collect_next_update (self);
      invalidate_for_next_update (self);
      schedule_next_update (self);

#ifdef DEBUG
      animation_state_dump (self);
#endif
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
}

/**
 * gtk_svg_get_state:
 * @self: an SVG paintable
 *
 * Gets the current state of the paintable.
 *
 * Returns: the state
 *
 * Since: 4.22
 */
unsigned int
gtk_svg_get_state (GtkSvg *self)
{
  g_return_val_if_fail (GTK_IS_SVG (self), -1);

  return self->state;
}

/**
 * gtk_svg_get_n_states:
 * @self: an SVG paintable
 *
 * Gets the number of states defined in the SVG.
 *
 * Note that there is always an empty state, which does
 * not count towards this number. If this function returns
 * the value N, the meaningful states of the SVG are
 * 0, 1, ..., N - 1 and `GTK_SVG_STATE_EMPTY`.
 *
 * Returns: the number of states
 *
 * Since: 4.22
 */
unsigned int
gtk_svg_get_n_states (GtkSvg *self)
{
  g_return_val_if_fail (GTK_IS_SVG (self), 0);

  return self->max_state + 1;
}

/**
 * gtk_svg_set_features:
 * @self: an SVG paintable
 * @features: features to enable
 *
 * Enables or disables features of the SVG paintable.
 *
 * By default, all features are enabled.
 *
 * Note that this call only has an effect before the
 * SVG is loaded.
 *
 * Since: 4.22
 */
void
gtk_svg_set_features (GtkSvg         *self,
                      GtkSvgFeatures  features)
{
  g_return_if_fail (GTK_IS_SVG (self));

  if (self->features == features)
    return;

  self->features = features;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FEATURES]);
}

/**
 * gtk_svg_get_features:
 * @self: an SVG paintable
 *
 * Returns the currently enabled features.
 *
 * Returns: the enabled features
 *
 * Since: 4.22
 */
GtkSvgFeatures
gtk_svg_get_features (GtkSvg *self)
{
  g_return_val_if_fail (GTK_IS_SVG (self), GTK_SVG_DEFAULT_FEATURES);

  return self->features;
}

/* }}} */
/* {{{ Animation */

/**
 * gtk_svg_set_frame_clock:
 * @self: an SVG paintable
 * @clock: the frame clock
 *
 * Sets a frame clock.
 *
 * Without a frame clock, GtkSvg will not advance animations.
 *
 * Since: 4.22
 */
void
gtk_svg_set_frame_clock (GtkSvg        *self,
                         GdkFrameClock *clock)
{
  g_return_if_fail (GTK_IS_SVG (self));

  gboolean was_connected;

  if (self->clock == clock)
    return;

  was_connected = self->clock_update_id != 0;

  frame_clock_disconnect (self);

  g_set_object (&self->clock, clock);

  if (self->clock && self->playing)
    {
      if (self->load_time == INDEFINITE)
        gtk_svg_set_load_time (self, gdk_frame_clock_get_frame_time (clock));
      else
        gtk_svg_advance (self, gdk_frame_clock_get_frame_time (clock));
    }

  /* FIXME should account for the difference between old and
   * new clock even when paused
   */

  if (was_connected)
    frame_clock_connect (self);
}

/**
 * gtk_svg_play:
 * @self: an SVG paintable
 *
 * Start playing animations and state transitions.
 *
 * Animations can be paused and started repeatedly.
 *
 * Since: 4.22
 */
void
gtk_svg_play (GtkSvg *self)
{
  g_return_if_fail (GTK_IS_SVG (self));

  gtk_svg_set_playing (self, TRUE);
}

/**
 * gtk_svg_pause:
 * @self: an SVG paintable
 *
 * Stop any playing animations and state transitions.
 *
 * Animations can be paused and started repeatedly.
 *
 * Since: 4.22
 */
void
gtk_svg_pause (GtkSvg *self)
{
  g_return_if_fail (GTK_IS_SVG (self));

  gtk_svg_set_playing (self, FALSE);
}

/* }}} */
/* {{{ Errors */

/**
 * GtkSvgError:
 * @GTK_SVG_ERROR_INVALID_SYNTAX: The XML syntax is broken
 *   in some way
 * @GTK_SVG_ERROR_INVALID_ELEMENT: An XML element is invalid
 *   (either because it is not part of SVG, or because it is
 *   in the wrong place, or because it not implemented in GTK)
 * @GTK_SVG_ERROR_INVALID_ATTRIBUTE: An attribute is invalid
 *   (either because it is not part of SVG, or because it is
 *   not implemented in GTK, or its value is problematic)
 * @GTK_SVG_ERROR_MISSING_ATTRIBUTE: A required attribute is missing
 * @GTK_SVG_ERROR_INVALID_REFERENCE: A reference does not point to
 *   a suitable element
 * @GTK_SVG_ERROR_FAILED_UPDATE: An animation could not be updated
 * @GTK_SVG_ERROR_FAILED_RENDERING: Rendering is not according to
 *   expecations
 *
 * Error codes in the `GTK_SVG_ERROR` domain for errors
 * that happen during parsing or rendering of SVG.
 *
 * Since: 4.22
 */

/**
 * GtkSvgLocation:
 * @bytes: the byte index in document. If unknown, this will
 *   be zero (which is also a valid value, but only if all
 *   three values are zero)
 * @lines: the line index in the document, 0-based
 * @line_chars: the char index in the line, 0-based
 *
 * Provides information about a location in an SVG document.
 *
 * The information should be considered approximate; it is
 * meant to provide feedback for errors in an editor.
 *
 * Since: 4.22
 */

/**
 * gtk_svg_error_get_element:
 * @error: an error in the [error@Gtk.SvgError] domain
 *
 * Returns context information about what XML element
 * the parsing error occurred in.
 *
 * Returns: (nullable): the element name
 *
 * Since: 4.22
 */
const char *
gtk_svg_error_get_element (const GError *error)
{
  GtkSvgErrorPrivate *priv = gtk_svg_error_get_private (error);

  if (priv)
    return priv->element;
  else
    return NULL;
}

/**
 * gtk_svg_error_get_attribute:
 * @error: an error in the [error@Gtk.SvgError] domain
 *
 * Returns context information about what XML attribute
 * the parsing error occurred in.
 *
 * Returns: (nullable): the attribute name
 *
 * Since: 4.22
 */
const char *
gtk_svg_error_get_attribute (const GError *error)
{
  GtkSvgErrorPrivate *priv = gtk_svg_error_get_private (error);

  if (priv)
    return priv->attribute;
  else
    return NULL;
}

/**
 * gtk_svg_error_get_start:
 * @error: an error in the [error@Gtk.SvgError] domain
 *
 * Returns context information about the start position
 * in the document where the parsing error occurred.
 *
 * Returns: (nullable): the [struct@Gtk.SvgLocation]
 *
 * Since: 4.22
 */
const GtkSvgLocation *
gtk_svg_error_get_start (const GError *error)
{
  GtkSvgErrorPrivate *priv = gtk_svg_error_get_private (error);

  if (priv)
    return &priv->start;
  else
    return NULL;
}

/**
 * gtk_svg_error_get_end:
 * @error: an error in the [error@Gtk.SvgError] domain
 *
 * Returns context information about the end position
 * in the document where the parsing error occurred.
 *
 * Returns: (nullable): the [struct@Gtk.SvgLocation]
 *
 * Since: 4.22
 */
const GtkSvgLocation *
gtk_svg_error_get_end (const GError *error)
{
  GtkSvgErrorPrivate *priv = gtk_svg_error_get_private (error);

  if (priv)
    return &priv->end;
  else
    return NULL;
}

/* }}} */
/* }}} */

/* vim:set foldmethod=marker: */

