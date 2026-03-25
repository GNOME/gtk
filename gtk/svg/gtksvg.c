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
#include "gtk/gtkcssselectorprivate.h"
#include <glib/gstdio.h>
#include "gtksvgenumtypes.h"
#include "gtksvgutilsprivate.h"
#include "gtksvgstringutilsprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgkeywordprivate.h"
#include "gtksvgnumberprivate.h"
#include "gtksvgnumbersprivate.h"
#include "gtksvgstringprivate.h"
#include "gtksvgstringlistprivate.h"
#include "gtksvgenumprivate.h"
#include "gtksvgfilterprimitiverefprivate.h"
#include "gtksvgtransformprivate.h"
#include "gtksvgcolorprivate.h"
#include "gtksvgpaintprivate.h"
#include "gtksvgfilterprivate.h"
#include "gtksvgdasharrayprivate.h"
#include "gtksvgpathprivate.h"
#include "gtksvgpathdataprivate.h"
#include "gtksvgclipprivate.h"
#include "gtksvgmaskprivate.h"
#include "gtksvgviewboxprivate.h"

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
 * `GtkSvg` fills or strokes paths with symbolic or fixed colors.
 * It can have multiple states, and paths can be included in a subset
 * of the states. States can have animations, and the transition
 * between different states can also be animated.
 *
 * To show a static SVG image, it is enough to load the
 * the SVG and use it like any other paintable.
 *
 * To play an SVG animation, use [method@Gtk.Svg.set_frame_clock]
 * to connect the paintable to a frame clock, and call
 * [method@Gtk.Svg.play] after loading the SVG. The animation can
 * be paused using [method@Gtk.Svg.pause].
 *
 * To set the current state, use [method@Gtk.Svg.set_state].
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
 * Among the structural elements, `<view>` is not supported.
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
 * Lastly, there is no interactivity, so links can't be activated
 * and pseudo-classes like :hover have no effect in CSS.
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
 * Note that the generated animations are implemented using standard
 * SVG attributes (`visibility`, `stroke-dasharray, `stroke-dashoffset`,
 * `pathLength` and `filter`). Setting these attributes in your SVG
 * is therefore going to interfere with generated animations.
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
 * A variant of the `gpa:states(...)` condition allows specifying
 * both before and after states:
 *
 *     <animate href='path1'
 *              attributeName='opacity'
 *              begin='gpa:states(0, 1 2)'
 *              dur='300ms'
 *              fill='freeze'
 *              from='1'
 *              to='0'/>
 *
 * will start the animation when the state changes from 0 to 1 or
 * from 0 to 2, but not when it changes from 0 to 3.
 *
 * In addition to the `gpa:fill` and `gpa:stroke` attributes, symbolic
 * colors can also be specified as a custom paint server reference,
 * like this: `url(#gpa:warning)`. This works in `fill` and `stroke`
 * attributes, but also when specifying colors in SVG animation
 * attributes like `to` or `values`.
 *
 * Note that the SVG syntax allows for a fallback RGB color to be
 * specified after the url, for compatibility with other SVG consumers:
 *
 *     fill='url(#gpa:warning) orange'
 *
 * GtkSvg also allows to refer to symbolic colors like system colors
 * in CSS, with names like SymbolicForeground, SymbolicSuccess, etc.
 * These can be used whenever a color is required.
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

#ifndef _MSC_VER
#define DEBUG
#endif /* _MSC_VER */

#define ALIGN_XY(x,y) ((x) | ((y) << 2))

#define ALIGN_GET_X(x) ((x) & 3)
#define ALIGN_GET_Y(x) ((x) >> 2)

typedef struct _Animation Animation;

#define BIT(n) (G_GUINT64_CONSTANT (1) << (n))

static SvgValue *shape_attr_ref_initial_value (ShapeAttr  attr,
                                               ShapeType  type,
                                               gboolean   has_parent);

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
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  location->bytes = g_markup_parse_context_get_offset (context);
G_GNUC_END_IGNORE_DEPRECATIONS
#else
  location->bytes = 0;
#endif
}

static void
gtk_svg_location_init_tag_start (GtkSvgLocation      *location,
                                 GMarkupParseContext *context)
{
#if GLIB_CHECK_VERSION (2, 88, 0)
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_markup_parse_context_get_tag_start (context,
                                        &location->lines,
                                        &location->line_chars,
                                        &location->bytes);
G_GNUC_END_IGNORE_DEPRECATIONS
#else
  gtk_svg_location_init (location, context);
#endif
}

static void
gtk_svg_location_init_tag_end (GtkSvgLocation      *location,
                               GMarkupParseContext *context)
{
  gtk_svg_location_init (location, context);
}

static void
gtk_svg_location_init_tag_range (GtkSvgLocation      *start,
                                 GtkSvgLocation      *end,
                                 GMarkupParseContext *context)
{
  gtk_svg_location_init_tag_start (start, context);
  gtk_svg_location_init_tag_end (end, context);
}

static void
gtk_svg_location_init_attr_range (GtkSvgLocation      *start,
                                  GtkSvgLocation      *end,
                                  GMarkupParseContext *context,
                                  unsigned int         attr)
{
#if GLIB_CHECK_VERSION (2, 89, 0)
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_markup_parse_context_get_attr_location (context, attr,
                                            &start->lines, &start->line_chars, &start->bytes,
                                            &end->lines, &end->line_chars, &end->bytes);
G_GNUC_END_IGNORE_DEPRECATIONS
#else
  gtk_svg_location_init_tag_range (start, end, context);
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

G_GNUC_PRINTF (6, 7)
static void
gtk_svg_skipped_element (GtkSvg               *self,
                         const char           *parent_element,
                         const GtkSvgLocation *start,
                         const GtkSvgLocation *end,
                         GtkSvgError           code,
                         const char           *format,
                         ...)
{
  GError *error;
  va_list args;

  va_start (args, format);
  error = g_error_new_valist (GTK_SVG_ERROR, code, format, args);
  va_end (args);

  gtk_svg_error_set_element (error, parent_element);
  gtk_svg_error_set_location (error, start, end);

  gtk_svg_emit_error (self, error);
  g_clear_error (&error);
}

G_GNUC_PRINTF (5, 6)
static void
gtk_svg_invalid_attribute (GtkSvg               *self,
                           GMarkupParseContext  *context,
                           const char          **attr_names,
                           const char           *attr_name,
                           const char           *format,
                           ...)
{
  GError *error;
  GtkSvgLocation start, end;

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
  gtk_svg_location_init_tag_range (&start, &end, context);

  if (attr_names && attr_name)
    {
      for (unsigned int i = 0; attr_names[i]; i++)
        {
          if (strcmp (attr_names[i], attr_name) == 0)
            {
              gtk_svg_location_init_attr_range (&start, &end, context, i);
              break;
            }
        }
    }

  gtk_svg_error_set_location (error, &start, &end);

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
  gtk_svg_location_init_tag_range (&start, &end, context);
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
  GtkSvgLocation start, end;

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
  gtk_svg_location_init_tag_range (&start, &end, context);
  gtk_svg_error_set_location (error, &start, &end);

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
        gtk_svg_invalid_attribute (self, context, attr_names, attr_names[i],
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

static inline gboolean
is_state_name_start (char c)
{
  return g_ascii_isalpha (c);
}

static inline gboolean
is_state_name (char c)
{
  return c == '-' || g_ascii_isalnum (c);
}

static gboolean
valid_state_name (const char *name)
{
  if (strcmp (name, "all") == 0 ||
      strcmp (name, "none") == 0)
    return FALSE;

  if (!is_state_name_start (name[0]))
    return FALSE;

  for (unsigned int i = 0; name[i]; i++)
    {
      if (!is_state_name (name[i]))
        return FALSE;
    }

  return TRUE;
}

static gboolean
find_named_state (GtkSvg       *svg,
                  const char   *name,
                  unsigned int *state)
{
  for (unsigned int i = 0; i < svg->n_state_names; i++)
    {
      if (strcmp (name, svg->state_names[i]) == 0)
        {
          *state = i;
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
strv_unique (GStrv strv)
{
  if (strv)
    for (unsigned int i = 0; strv[i]; i++)
      for (unsigned int j = i + 1; strv[j]; j++)
        if (strcmp (strv[i], strv[j]) == 0)
          return FALSE;

  return TRUE;
}

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

static gboolean
svg_writing_mode_is_vertical (WritingMode mode)
{
  const gboolean is_vertical[] = {
    FALSE, TRUE, TRUE,
    FALSE, FALSE, FALSE, FALSE, TRUE, TRUE
  };

  return is_vertical[mode];
}

/* }}} */
/* {{{ Image loading */

static GdkTexture *
load_texture (const char  *string,
              gboolean     allow_external,
              gboolean    *cache_this,
              GError     **error)
{
  GdkTexture *texture = NULL;

  *cache_this = TRUE;

  if (g_str_has_prefix (string, "data:"))
    {
      GBytes *bytes;

      bytes = gtk_css_data_url_parse (string, NULL, error);

      if (bytes == NULL)
        return NULL;

      *cache_this = FALSE;
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
  else
    {
      g_set_error (error, GTK_SVG_ERROR, GTK_SVG_ERROR_FEATURE_DISABLED,
                   "Not loading texture from %s: External resources disabled", string);
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
      gboolean cache_this;

      texture = load_texture (string,
                              (svg->features & GTK_SVG_EXTERNAL_RESOURCES) != 0,
                              &cache_this,
                              error);
      if (texture && cache_this)
        g_hash_table_insert (svg->images, g_strdup (string), texture);
    }

  return texture;
}

/* }}} */
/* {{{ Font handling */

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
add_font_from_url (GtkSvg               *svg,
                   GMarkupParseContext  *context,
                   const char          **attr_names,
                   const char           *attr_name,
                   const char           *url)
{
  char *scheme;
  GBytes *bytes;
  char *mimetype = NULL;
  GError *error = NULL;

  scheme = g_uri_parse_scheme (url);
  if (!scheme || g_ascii_strcasecmp (scheme, "data") != 0)
    {
      char *start = g_utf8_make_valid (url, 20);
      gtk_svg_invalid_attribute (svg, context, attr_names, attr_name,
                                 "Unsupported uri scheme for font: %s…", start);
      g_free (start);
      return FALSE;
    }

  g_free (scheme);

  bytes = gtk_css_data_url_parse (url, &mimetype, &error);
  if (!bytes)
    {
      gtk_svg_invalid_attribute (svg, context, attr_names, attr_name,
                                 "Can't parse font data: %s", error->message);
      g_error_free (error);
      g_free (mimetype);
      return FALSE;
    }

  if (strcmp (mimetype, "font/ttf") != 0)
    {
      gtk_svg_invalid_attribute (svg, context, attr_names, attr_name,
                                 "Unsupported mime type for font data: %s", mimetype);
      g_bytes_unref (bytes);
      g_free (mimetype);
      return FALSE;
    }

  g_free (mimetype);

  if (!add_font_from_bytes (svg, bytes, &error))
    {
      g_bytes_unref (bytes);
      gtk_svg_invalid_attribute (svg, context, attr_names, attr_name,
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
/* {{{ Style utilities */

typedef struct {
  GBytes *content;
  GtkSvgLocation location;
} StyleElt;

static void
style_elt_free (gpointer p)
{
  StyleElt *e = (StyleElt *) p;

  g_bytes_unref (e->content);
  g_free (e);
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
              size_t                   n_pts,
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
parse_states (GtkSvg     *svg,
              const char *text,
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

  str = strsplit_set (text, " ");
  for (unsigned int i = 0; str[i]; i++)
    {
      unsigned int u;
      char *end;

      u = (unsigned int) g_ascii_strtoull (str[i], &end, 10);
      if ((end && *end != '\0') || (u > 63))
        {
          if (!find_named_state (svg, str[i], &u))
            {
              *states = NO_STATES;
              g_strfreev (str);
              return FALSE;
            }
        }

      *states |= BIT (u);
    }

  g_strfreev (str);
  return TRUE;
}

static void
print_states (GString  *s,
              GtkSvg   *svg,
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
      unsigned int n_state_names = 0;
      const char **state_names = NULL;
      if (svg)
        {
          n_state_names = svg->n_state_names;
          state_names = (const char **) svg->state_names;
        }
      for (unsigned int u = 0; u < 64; u++)
        {
          if ((states & BIT (u)) != 0)
            {
              if (!first)
                g_string_append_c (s, ' ');
              first = FALSE;
              if (u < n_state_names)
                g_string_append (s, state_names[u]);
              else
                g_string_append_printf (s, "%u", u);
            }
        }
    }
}

static gboolean
state_match (uint64_t     states,
             unsigned int state)
{
  if ((states & BIT (state)) != 0)
    return TRUE;

  return FALSE;
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

static SvgValue *
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
svg_orient_interpolate (const SvgValue    *value0,
                        const SvgValue    *value1,
                        SvgComputeContext *context,
                        double             t)
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
svg_orient_accumulate (const SvgValue    *value0,
                       const SvgValue    *value1,
                       SvgComputeContext *context,
                       int                n)
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
      g_string_append (string, svg_unit_name (v->unit));
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

static SvgValue * svg_orient_resolve (const SvgValue    *value,
                                      ShapeAttr          attr,
                                      unsigned int       idx,
                                      Shape             *shape,
                                      SvgComputeContext *context);

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
svg_orient_parse (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "auto"))
    return svg_orient_new_auto (FALSE);
  else if (gtk_css_parser_try_ident (parser, "auto-start-reverse"))
    return svg_orient_new_auto (TRUE);
  else
    {
      double f;
      SvgUnit unit;

      if (!svg_number_parse2 (parser, -DBL_MAX, DBL_MAX, NUMBER|ANGLE, &f, &unit))
        return NULL;

      return svg_orient_new_angle (f, unit);
    }
}

static SvgValue *
svg_orient_resolve (const SvgValue     *value,
                    ShapeAttr           attr,
                    unsigned int        idx,
                    Shape              *shape,
                    SvgComputeContext  *context)
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
svg_language_interpolate (const SvgValue    *value0,
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
svg_language_accumulate (const SvgValue    *value0,
                         const SvgValue    *value1,
                         SvgComputeContext *context,
                         int                n)
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
svg_text_decoration_interpolate (const SvgValue    *value0,
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
svg_text_decoration_accumulate (const SvgValue    *value0,
                                const SvgValue    *value1,
                                SvgComputeContext *context,
                                int                n)
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

static SvgValue * svg_text_decoration_resolve (const SvgValue    *value,
                                               ShapeAttr          attr,
                                               unsigned int       idx,
                                               Shape             *shape,
                                               SvgComputeContext *context);

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
svg_text_decoration_parse (GtkCssParser *parser)
{
  TextDecoration val = TEXT_DECORATION_NONE;

  while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      unsigned int i;

      gtk_css_parser_skip_whitespace (parser);

      for (i = 0; i < G_N_ELEMENTS (text_decorations); i++)
        {
          if (gtk_css_parser_try_ident (parser, text_decorations[i].name))
            {
              val |= text_decorations[i].value;
              break;
            }
        }

      if (i == G_N_ELEMENTS (text_decorations))
        return NULL;
    }

  return svg_text_decoration_new (val);
}

static SvgValue *
svg_text_decoration_resolve (const SvgValue    *value,
                             ShapeAttr          attr,
                             unsigned int       idx,
                             Shape             *shape,
                             SvgComputeContext *context)
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
svg_href_interpolate (const SvgValue    *value1,
                      const SvgValue    *value2,
                      SvgComputeContext *context,
                      double             t)
{
  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value1);
  else
    return svg_value_ref ((SvgValue *) value2);
}

static SvgValue *
svg_href_accumulate (const SvgValue    *value1,
                     const SvgValue    *value2,
                     SvgComputeContext *context,
                     int                n)
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
svg_href_new_url_take (const char *ref)
{
  SvgHref *result;

  result = (SvgHref *) svg_value_alloc (&SVG_HREF_CLASS, sizeof (SvgHref));
  result->kind = HREF_URL;
  result->ref = (char *) ref;
  result->shape = NULL;

  return (SvgValue *) result;
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
  size_t line;
  char *id;
  char *style;
  char **classes;
  GtkCssNode *css_node;
} ColorStop;

static void
color_stop_free (gpointer v)
{
  ColorStop *stop = v;

  g_free (stop->id);
  g_free (stop->style);
  g_strfreev (stop->classes);
  g_object_unref (stop->css_node);

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
color_stop_new (Shape *parent)
{
  ColorStop *stop = g_new0 (ColorStop, 1);

  for (ShapeAttr attr = FIRST_STOP_ATTR; attr <= LAST_STOP_ATTR; attr++)
    stop->base[color_stop_attr_idx (attr)] = shape_attr_ref_initial_value (attr, SHAPE_LINEAR_GRADIENT, TRUE);

  stop->css_node = gtk_css_node_new ();
  gtk_css_node_set_name (stop->css_node, g_quark_from_static_string ("stop"));
  gtk_css_node_set_parent (stop->css_node, parent->css_node);

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

static unsigned int
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
  size_t line;
  char *id;
  char *style;
  char **classes;
  GtkCssNode *css_node;
  SvgValue **current;
  SvgValue *base[1];
} FilterPrimitive;

static void
filter_primitive_free (gpointer v)
{
  FilterPrimitive *f = v;

  g_free (f->id);
  g_free (f->style);
  g_strfreev (f->classes);
  g_object_unref (f->css_node);

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
        SvgValue *v = filter->current [filter_attr_idx (filter->type, SHAPE_ATTR_FE_FUNC_VALUES)];
        unsigned int n_values = svg_numbers_get_length (v);
        float *values = g_newa (float, n_values);

        if (n_values == 0)
          return gsk_component_transfer_new_identity ();

        for (unsigned int i = 0; i < n_values; i++)
          values[i] = svg_numbers_get (v, i, 100);

        if (type == COMPONENT_TRANSFER_TABLE)
          return gsk_component_transfer_new_table (n_values, values);
        else
          return gsk_component_transfer_new_discrete (n_values, values);
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
          SvgFilterPrimitiveRefType type;

          type = svg_filter_primitive_ref_get_type (f->current[idx]);
          if (type == BACKGROUND_IMAGE || type == BACKGROUND_ALPHA)
            return TRUE;
        }
    }

  return FALSE;
}

gboolean
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
filter_primitive_new (Shape               *parent,
                      FilterPrimitiveType  type)
{
  FilterTypeInfo *ft = &filter_types[type];
  FilterPrimitive *f;

  f = g_malloc0 (sizeof (FilterPrimitive) + sizeof (SvgValue *) * (2 * ft->n_attrs - 1));

  f->type = type;
  f->current = f->base + ft->n_attrs;

  for (unsigned int i = 0; i < ft->n_attrs; i++)
    f->base[i] = filter_attr_ref_initial_value (f, ft->attrs[i]);

  f->css_node = gtk_css_node_new ();
  gtk_css_node_set_name (f->css_node, g_quark_from_static_string (ft->name));
  gtk_css_node_set_parent (f->css_node, parent->css_node);

  return f;
}

/* }}} */
/* {{{ Attributes */

static SvgValue *
parse_points (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "none"))
    return svg_numbers_new_none ();
  else
    {
      SvgValue *p = svg_numbers_parse2 (parser, NUMBER|PERCENTAGE|LENGTH);

      if (p != NULL && svg_numbers_get_length (p) % 2 == 1)
        {
          gtk_css_parser_error_syntax (parser, "Odd number of coordinates");
          p = svg_numbers_drop_last (p);
        }

      return p;
    }
}

static SvgValue *
parse_language (GtkCssParser *parser)
{
  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
    {
      const GtkCssToken *token;
      PangoLanguage *lang;

      token = gtk_css_parser_get_token (parser);
      lang = pango_language_from_string (gtk_css_token_get_string (token));
      gtk_css_parser_skip (parser);

      if (lang)
        return svg_language_new (lang);
    }

  return NULL;
}

static SvgValue *
parse_custom_ident (GtkCssParser *parser)
{
  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
    {
      const GtkCssToken *token = gtk_css_parser_get_token (parser);
      const char *string = gtk_css_token_get_string (token);
      SvgValue *value;

      if (strcmp (string, "initial") == 0 ||
          strcmp (string, "inherited") == 0)
        {
          gtk_css_parser_error_value (parser, "Can't use %s here", string);
          return NULL;
        }

      value = svg_string_new (string);

      gtk_css_parser_skip (parser);

      return value;
    }

  gtk_css_parser_error_syntax (parser, "Expected an identifier");
  return NULL;
}

static SvgValue *
parse_language_list (GtkCssParser *parser)
{
  GPtrArray *langs;
  SvgValue *value;

  langs = g_ptr_array_new ();

  while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      const GtkCssToken *token;
      PangoLanguage *lang;

      gtk_css_parser_skip_whitespace (parser);

      if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
        {
          g_ptr_array_unref (langs);
          return NULL;
        }

      token = gtk_css_parser_get_token (parser);
      lang = pango_language_from_string (gtk_css_token_get_string (token));
      gtk_css_parser_skip (parser);

      if (!lang)
        {
          g_ptr_array_unref (langs);
          return NULL;
        }
      g_ptr_array_add (langs, lang);

      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COMMA))
        gtk_css_parser_skip (parser);
    }

  value = svg_language_new_list (langs->len, (PangoLanguage **) langs->pdata);
  g_ptr_array_unref (langs);

  return value;
}

static SvgValue *
parse_path (const char  *string,
            GError     **error)
{
  if (strcmp (string, "none") == 0)
    return svg_path_new_none ();
  else
    {
      SvgPathData *data = NULL;

      if (!svg_path_data_parse_full (string, &data))
        g_set_error (error,
                     GTK_CSS_PARSER_ERROR, GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                     "Path data is invalid");

      return svg_path_new_from_data (data);
    }
}

static SvgValue *
parse_href (const char  *string,
            GError     **error)
{
   return svg_href_new_ref (string);
}

static SvgValue *
parse_string_list (const char  *value,
                   GError     **error)
{
  return svg_string_list_new_take (strsplit_set (value, " "), ' ');
}

static SvgValue *
parse_opacity (GtkCssParser *parser)
{
  return svg_number_parse (parser, -DBL_MAX, DBL_MAX, NUMBER|PERCENTAGE);
}

static SvgValue *
parse_stroke_width (GtkCssParser *parser)
{
  return svg_number_parse (parser, 0, DBL_MAX, NUMBER|LENGTH|PERCENTAGE);
}

static SvgValue *
parse_miterlimit (GtkCssParser *parser)
{
  return svg_number_parse (parser, 0, DBL_MAX, NUMBER);
}

static SvgValue *
parse_any_length (GtkCssParser *parser)
{
  return svg_number_parse (parser, -DBL_MAX, DBL_MAX, NUMBER|LENGTH);
}

static SvgValue *
parse_length_percentage (GtkCssParser *parser)
{
  return svg_number_parse (parser, -DBL_MAX, DBL_MAX, NUMBER|PERCENTAGE|LENGTH);
}

static SvgValue *
parse_any_number (GtkCssParser *parser)
{
  return svg_number_parse (parser, -DBL_MAX, DBL_MAX, NUMBER);
}

static SvgValue *
parse_font_weight (GtkCssParser *parser)
{
  SvgValue *v;

  v = svg_font_weight_parse (parser);
  if (v)
    return v;

  return svg_number_parse (parser, 1, 1000, NUMBER);
}

static SvgValue *
parse_font_size (GtkCssParser *parser)
{
  SvgValue *v;

  v = svg_font_size_parse (parser);
  if (v)
    return v;

  return svg_number_parse (parser, 0, DBL_MAX, NUMBER|PERCENTAGE|LENGTH);
}

static SvgValue *
parse_letter_spacing (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "normal"))
    return svg_number_new (0.);

  return svg_number_parse (parser, -DBL_MAX, DBL_MAX, NUMBER|LENGTH);
}

static SvgValue *
parse_offset (GtkCssParser *parser)
{
  return svg_number_parse (parser, -DBL_MAX, DBL_MAX, NUMBER|PERCENTAGE);
}

static SvgValue *
parse_ref_x (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "left"))
    return svg_percentage_new (0);
  else if (gtk_css_parser_try_ident (parser, "center"))
    return svg_percentage_new (50);
  else if (gtk_css_parser_try_ident (parser, "right"))
    return svg_percentage_new (100);

  return svg_number_parse (parser, -DBL_MAX, DBL_MAX, NUMBER|PERCENTAGE|LENGTH);
}

static SvgValue *
parse_ref_y (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "top"))
    return svg_percentage_new (0);
  else if (gtk_css_parser_try_ident (parser, "center"))
    return svg_percentage_new (50);
  else if (gtk_css_parser_try_ident (parser, "bottom"))
    return svg_percentage_new (100);

  return svg_number_parse (parser, -DBL_MAX, DBL_MAX, NUMBER|PERCENTAGE|LENGTH);
}

static SvgValue *
parse_length_percentage_or_auto (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "auto"))
    return svg_auto_new ();

  return parse_length_percentage (parser);
}

static SvgValue *
parse_number_optional_number (GtkCssParser *parser)
{
  SvgValue *values;

  values = svg_numbers_parse (parser);

  if (values == NULL)
    {
      return NULL;
    }
  else if (svg_numbers_get_length (values) <= 2)
    {
      return values;
    }
  else
    {
      svg_value_unref (values);
      return NULL;
    }
}

static SvgValue *
parse_transform_origin (GtkCssParser *parser)
{
  double d[2];
  SvgUnit u[2];
  enum { HORIZONTAL, VERTICAL, NEUTRAL, };
  unsigned int set[2];
  unsigned int i = 0;

  d[0] = d[1] = 50;
  u[0] = u[1] = SVG_UNIT_PERCENTAGE;
  set[0] = set[1] = NEUTRAL;

  for (i = 0; i < 2; i++)
    {
      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        break;

      if (gtk_css_parser_try_ident (parser, "left"))
        {
          set[i] = HORIZONTAL;
          d[i] = 0;
          u[i] = SVG_UNIT_PERCENTAGE;
        }
      else if (gtk_css_parser_try_ident (parser, "right"))
        {
          set[i] = HORIZONTAL;
          d[i] = 100;
          u[i] = SVG_UNIT_PERCENTAGE;
        }
      else if (gtk_css_parser_try_ident (parser, "top"))
        {
          set[i] = VERTICAL;
          d[i] = 0;
          u[i] = SVG_UNIT_PERCENTAGE;
        }
      else if (gtk_css_parser_try_ident (parser, "bottom"))
        {
          set[i] = VERTICAL;
          d[i] = 100;
          u[i] = SVG_UNIT_PERCENTAGE;
        }
      else if (gtk_css_parser_try_ident (parser, "center"))
        {
          /* nothing to do */
        }
      else if (!svg_number_parse2 (parser, -DBL_MAX, DBL_MAX, LENGTH|PERCENTAGE, &d[i], &u[i]))
        {
          return NULL;
        }
    }

  if (set[0] == set[1] && set[0] != NEUTRAL)
    return NULL;

  if (set[0] == HORIZONTAL)
    return svg_numbers_new2 (d[0], u[0], d[1], u[1]);
  else
    return svg_numbers_new2 (d[1], u[1], d[0], u[0]);
}

static SvgValue *
parse_font_family (GtkCssParser *parser)
{
  GStrvBuilder *builder;
  const GtkCssToken *token;
  SvgValue *result;

  builder = g_strv_builder_new ();

  while (1)
    {
      gtk_css_parser_skip_whitespace (parser);
      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
        {
          GString *string = g_string_new (NULL);
          token = gtk_css_parser_get_token (parser);

          g_string_append (string, gtk_css_token_get_string (token));
          gtk_css_parser_skip (parser);

          gtk_css_parser_skip_whitespace (parser);
          while (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
            {
              token = gtk_css_parser_get_token (parser);
              g_string_append_c (string, ' ');
              g_string_append (string, gtk_css_token_get_string (token));
              gtk_css_parser_skip (parser);
            }
          g_strv_builder_take (builder, g_string_free (string, FALSE));
        }
      else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_STRING))
        {
          token = gtk_css_parser_get_token (parser);
          g_strv_builder_add (builder, gtk_css_token_get_string (token));
          gtk_css_parser_skip (parser);
        }
      else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COMMA))
        {
          gtk_css_parser_skip (parser);
        }
      else
        break;
    }

  result = svg_string_list_new_take (g_strv_builder_unref_to_strv (builder), ',');

  return result;
}

static SvgValue *
marker_ref_parse (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "none"))
    {
      return svg_href_new_none ();
    }
  else
    {
      char *url = gtk_css_parser_consume_url (parser);

      if (url)
        return svg_href_new_url_take (url);
    }

  gtk_css_parser_error_syntax (parser, "Expected a marker reference");
  return NULL;
}

static gboolean
number_is_positive_or_auto (const SvgValue *value)
{
  return svg_value_equal (value, svg_auto_new ()) ||
         svg_value_is_positive_number (value);
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
  SvgValue * (* parse_value)        (GtkCssParser    *parser);
  SvgValue * (* parse_presentation) (const char      *string,
                                     GError         **error);
  gboolean   (* is_valid)           (const SvgValue  *value);
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
   BIT (SHAPE_FILTER) | BIT (SHAPE_USE) | BIT (SHAPE_SWITCH) | \
   BIT (SHAPE_LINK))

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
   BIT (SHAPE_SVG) | BIT (SHAPE_SYMBOL) | BIT (SHAPE_SWITCH) | \
   BIT (SHAPE_LINK))

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
    .parse_value = svg_transform_parse_css,
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
    .parse_value = parse_font_family,
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
    .parse_value = svg_filter_parse_css,
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
    .parse_value = marker_ref_parse,
  },
  [SHAPE_ATTR_MARKER_MID] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_SHAPES,
    .parse_value = marker_ref_parse,
  },
  [SHAPE_ATTR_MARKER_END] = {
    .flags = SHAPE_ATTR_INHERITED,
    .applies_to = SHAPE_SHAPES,
    .parse_value = marker_ref_parse,
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
    .parse_value = parse_any_length,
    .is_valid = svg_value_is_positive_number,
  },
  [SHAPE_ATTR_HREF] = {
    .flags = SHAPE_ATTR_DISCRETE | SHAPE_ATTR_NO_CSS,
    .applies_to = SHAPE_GRAPHICS_REF | SHAPE_PAINT_SERVERS | BIT (SHAPE_LINK),
    .parse_presentation = parse_href,
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
    .applies_to = BIT (SHAPE_PATH),
    .parse_value = svg_path_parse,
    .parse_presentation = parse_path,
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
    .parse_value = parse_length_percentage,
    .is_valid = svg_value_is_positive_number,
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
    .parse_value = parse_length_percentage_or_auto,
    .is_valid = number_is_positive_or_auto,
  },
  [SHAPE_ATTR_HEIGHT] = {
    .applies_to = BIT (SHAPE_SVG) | BIT (SHAPE_RECT) | BIT (SHAPE_IMAGE) |
              BIT (SHAPE_USE) | BIT (SHAPE_FILTER) | BIT (SHAPE_MARKER) |
              BIT (SHAPE_MASK) | BIT (SHAPE_PATTERN),
    .parse_value = parse_length_percentage_or_auto,
    .is_valid = number_is_positive_or_auto,
  },
  [SHAPE_ATTR_RX] = {
    .applies_to = BIT (SHAPE_RECT) | BIT (SHAPE_ELLIPSE) | BIT (SHAPE_RADIAL_GRADIENT),
    .parse_value = parse_length_percentage_or_auto,
    .is_valid = number_is_positive_or_auto,
  },
  [SHAPE_ATTR_RY] = {
    .applies_to = BIT (SHAPE_RECT) | BIT (SHAPE_ELLIPSE) | BIT (SHAPE_RADIAL_GRADIENT),
    .parse_value = parse_length_percentage_or_auto,
    .is_valid = number_is_positive_or_auto,
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
    .parse_value = parse_points,
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
    .parse_value = parse_length_percentage,
    .is_valid = svg_value_is_positive_number,
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
    .parse_presentation = parse_string_list,
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
    .parse_value = parse_length_percentage,
    .is_valid = svg_value_is_positive_number,
  },
  [SHAPE_ATTR_FE_HEIGHT] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_length_percentage,
    .is_valid = svg_value_is_positive_number,
  },
  [SHAPE_ATTR_FE_RESULT] = {
    .flags = SHAPE_ATTR_NO_CSS,
    .applies_to = BIT (SHAPE_FILTER),
    .parse_value = parse_custom_ident,
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
    .parse_presentation = parse_href,
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
  shape_attrs[SHAPE_ATTR_FONT_FAMILY].initial_value = svg_string_list_new (NULL);
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
  shape_attrs[SHAPE_ATTR_FE_STD_DEV].initial_value = svg_numbers_new1 (0);
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

#ifndef G_DISABLE_ASSERT
  for (unsigned int i = 0; i < G_N_ELEMENTS (shape_attrs); i++)
    {
      g_assert (svg_value_is_immortal (shape_attrs[i].initial_value));
      g_assert (shape_attrs[i].parse_value != NULL ||
                ((shape_attrs[i].flags & SHAPE_ATTR_NO_CSS) != 0 &&
                 shape_attrs[i].parse_presentation != NULL));
    }
#endif
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

#define FILTER_ANY \
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
  { "href", SHAPE_GRAPHICS_REF | SHAPE_PAINT_SERVERS | BIT (SHAPE_LINK), 0, SHAPE_ATTR_HREF },
  { "xlink:href", SHAPE_GRAPHICS_REF | SHAPE_PAINT_SERVERS | BIT (SHAPE_LINK), 0, SHAPE_ATTR_HREF | DEPRECATED_BIT },
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
  { "offset", SHAPE_GRADIENTS, 0, SHAPE_ATTR_STOP_OFFSET },
  { "stop-color", SHAPE_ANY, 0, SHAPE_ATTR_STOP_COLOR },
  { "stop-opacity", SHAPE_ANY, 0, SHAPE_ATTR_STOP_OPACITY },
  { "x", BIT (SHAPE_FILTER), FILTER_ANY, SHAPE_ATTR_FE_X },
  { "y", BIT (SHAPE_FILTER), FILTER_ANY, SHAPE_ATTR_FE_Y },
  { "width", BIT (SHAPE_FILTER), FILTER_ANY, SHAPE_ATTR_FE_WIDTH },
  { "height", BIT (SHAPE_FILTER), FILTER_ANY, SHAPE_ATTR_FE_HEIGHT },
  { "result", BIT (SHAPE_FILTER), FILTER_ANY, SHAPE_ATTR_FE_RESULT },
  { "flood-color", BIT (SHAPE_FILTER), BIT (FE_FLOOD) | BIT (FE_DROPSHADOW), SHAPE_ATTR_FE_COLOR },
  { "flood-color", SHAPE_ANY, 0, SHAPE_ATTR_FE_COLOR },
  { "flood-opacity", BIT (SHAPE_FILTER), BIT (FE_FLOOD) | BIT (FE_DROPSHADOW), SHAPE_ATTR_FE_OPACITY },
  { "flood-opacity", SHAPE_ANY, 0, SHAPE_ATTR_FE_OPACITY },
  { "in", BIT (SHAPE_FILTER), (FILTER_ANY | BIT (FE_MERGE_NODE)) & ~(BIT (FE_FLOOD) | BIT (FE_IMAGE) | BIT (FE_MERGE)), SHAPE_ATTR_FE_IN },
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

static unsigned int
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
shape_attr_lookup_for_css (const char *name,
                           ShapeAttr  *attr,
                           gboolean   *deprecated,
                           gboolean   *is_marker_shorthand)
{
  ShapeAttrLookup key;
  ShapeAttrLookup *found;

  *is_marker_shorthand = FALSE;

  if (strcmp (name, "marker") == 0)
    {
      *attr = SHAPE_ATTR_MARKER_START;
      *deprecated = FALSE;
      *is_marker_shorthand = TRUE;
      return TRUE;
    }

  key.name = name;
  key.shapes = SHAPE_ANY;
  key.filters = 0;
  found = g_hash_table_lookup (shape_attr_lookup_table, &key);

  if (!found)
    {
      key.shapes = SHAPE_FILTER;
      key.filters = FILTER_ANY;
      found = g_hash_table_lookup (shape_attr_lookup_table, &key);
    }

  if (!found)
    return FALSE;

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

  return shape_attr_has_css (*attr);
}

static gboolean
color_stop_attr_lookup (const char *name,
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

  if (*attr < FIRST_STOP_ATTR || *attr > LAST_STOP_ATTR)
    return FALSE;

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

static void
parse_value_error (GtkCssParser         *parser,
                   const GtkCssLocation *css_start,
                   const GtkCssLocation *css_end,
                   const GError         *css_error,
                   gpointer              user_data)
{
  /* FIXME: locations */
  if (user_data)
    *((GError **) user_data) = g_error_copy (css_error);
}

static SvgValue *
shape_attr_parse_value (ShapeAttr    attr,
                        const char  *string,
                        GError     **error)
{
  if (match_str (string, "inherit"))
    return svg_inherit_new ();
  else if (match_str (string, "initial"))
    return svg_initial_new ();

  if (shape_attrs[attr].parse_presentation)
    {
      return shape_attrs[attr].parse_presentation (string, error);
    }
  else
    {
      GBytes *bytes;
      GtkCssParser *parser;
      SvgValue *value;

      bytes = g_bytes_new_static (string, strlen (string));
      parser = gtk_css_parser_new_for_bytes (bytes, NULL, parse_value_error, error, NULL);

      gtk_css_parser_skip_whitespace (parser);
      value = shape_attrs[attr].parse_value (parser);

      gtk_css_parser_skip_whitespace (parser);
      if (value && !gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        {
          g_clear_pointer (&value, svg_value_unref);
          g_clear_error (error);
          g_set_error (error, GTK_SVG_ERROR, GTK_SVG_ERROR_INVALID_SYNTAX,
                       "Junk at end of value");
        }

      gtk_css_parser_unref (parser);
      g_bytes_unref (bytes);

      return value;
    }
}

static gboolean
shape_attr_value_is_valid (ShapeAttr  attr,
                           SvgValue  *value)
{
  if (shape_attrs[attr].is_valid)
    return shape_attrs[attr].is_valid (value);
  return TRUE;
}

static SvgValue *
shape_attr_parse_and_validate (ShapeAttr    attr,
                               const char  *string,
                               GError     **error)
{
  SvgValue *value;

  value = shape_attr_parse_value (attr, string, error);
  if (value && !shape_attr_value_is_valid (attr, value))
    {
      g_clear_pointer (&value, svg_value_unref);
      g_set_error (error,
                   GTK_SVG_ERROR, GTK_SVG_ERROR_INVALID_ATTRIBUTE,
                   "Value is not valid");
    }

  return value;
}

static SvgValue *
shape_attr_parse_css (ShapeAttr     attr,
                      GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "inherit"))
    return svg_inherit_new ();
  else if (gtk_css_parser_try_ident (parser, "initial"))
    return svg_initial_new ();
  else
    {
      SvgValue *value;

      value = shape_attrs[attr].parse_value (parser);
      if (value && !shape_attr_value_is_valid (attr, value))
        g_clear_pointer (&value, svg_value_unref);

      return value;
    }
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
        v = shape_attr_parse_value (attr, s, NULL);

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
  [SHAPE_LINK] = {
    .name = "a",
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

static unsigned int
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
  g_clear_pointer (&shape->style, g_free);
  g_clear_pointer (&shape->classes, g_strfreev);
  g_clear_object (&shape->css_node);

  for (ShapeAttr attr = FIRST_SHAPE_ATTR; attr <= LAST_FILTER_ATTR; attr++)
    {
      g_clear_pointer (&shape->base[attr], svg_value_unref);
      g_clear_pointer (&shape->current[attr], svg_value_unref);
    }

  g_clear_pointer (&shape->styles, g_ptr_array_unref);
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

  g_clear_pointer (&shape->gpa.fill, svg_value_unref);
  g_clear_pointer (&shape->gpa.stroke, svg_value_unref);
  g_clear_pointer (&shape->gpa.width, svg_value_unref);
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

  if (shape_type_has_gpa_attrs (type))
    {
      shape->gpa.states = ALL_STATES;
    }

  shape->styles = g_ptr_array_new_with_free_func (style_elt_free);

  shape->css_node = gtk_css_node_new ();
  gtk_css_node_set_name (shape->css_node, g_quark_from_static_string (shape_types[type].name));
  if (parent)
    gtk_css_node_set_parent (shape->css_node, parent->css_node);

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
  SvgValue *required_extensions = shape->current[SHAPE_ATTR_REQUIRED_EXTENSIONS];
  SvgLanguage *system_language = (SvgLanguage *) shape->current[SHAPE_ATTR_SYSTEM_LANGUAGE];
  PangoLanguage *lang;

  if (svg_value_equal (required_extensions, svg_string_list_new (NULL)) &&
      svg_value_equal ((SvgValue *) system_language, svg_language_new_list (0, NULL)))
    return FALSE;

  for (unsigned int i = 0; i < svg_string_list_get_length (required_extensions); i++)
    {
      if ((svg->features & GTK_SVG_EXTENSIONS) == 0)
        return TRUE;

      if (strcmp (svg_string_list_get (required_extensions, i), "http://www.gtk.org/grappa") != 0)
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
          SvgValue *v = values[SHAPE_ATTR_POINTS];
          if (svg_numbers_get_length (v) > 0)
            {
              g_assert (svg_numbers_get_unit (v, 0) != SVG_UNIT_PERCENTAGE);
              g_assert (svg_numbers_get_unit (v, 1) != SVG_UNIT_PERCENTAGE);

              gsk_path_builder_move_to (builder,
                                        svg_numbers_get (v, 0, 1),
                                        svg_numbers_get (v, 1, 1));

              for (unsigned int i = 2; i + 1 < svg_numbers_get_length (v); i += 2)
                {
                  g_assert (svg_numbers_get_unit (v, i) != SVG_UNIT_PERCENTAGE);
                  g_assert (svg_numbers_get_unit (v, i + 1) != SVG_UNIT_PERCENTAGE);

                  gsk_path_builder_line_to (builder,
                                            svg_numbers_get (v, i, 1),
                                            svg_numbers_get (v, i + 1, 1));
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
    case SHAPE_LINK:
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
        case SHAPE_LINK:
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
        case SHAPE_LINK:
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
    case SHAPE_LINK:
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
                   GskTransform *transform = svg_transform_get_gsk (sh->current[SHAPE_ATTR_TRANSFORM]);
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
    case SHAPE_LINK:
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

  g_ptr_array_add (shape->color_stops, color_stop_new (shape));

  return shape->color_stops->len - 1;
}

static unsigned int
shape_add_filter (Shape               *shape,
                  FilterPrimitiveType  type)
{
  g_assert (shape_type_has_filters (shape->type));

  g_ptr_array_add (shape->filters, filter_primitive_new (shape, type));

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
      uint64_t from;
      uint64_t to;
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
      t->states.from = orig->states.from;
      t->states.to = orig->states.to;
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
      return t1->states.from == t2->states.from &&
             t1->states.to == t2->states.to &&
             t1->offset == t2->offset;

    default:
      g_assert_not_reached ();
    }
}

static gboolean
time_spec_parse (GtkSvg     *svg,
                 TimeSpec   *spec,
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
          if (!parse_states (svg, str + strlen ("gpa:states("), &states))
            {
              g_free (str);
              return FALSE;
            }
          g_free (str);

          spec->type = TIME_SPEC_TYPE_STATES;
          if (side == TIME_SPEC_SIDE_BEGIN)
            {
              spec->states.from = ALL_STATES & ~states;
              spec->states.to = states;
            }
          else
            {
              spec->states.from = states;
              spec->states.to = ALL_STATES & ~states;
            }
        }
      else
        {
          spec->type = TIME_SPEC_TYPE_SYNC;
          spec->sync.ref = str;
          spec->sync.base = NULL;
          spec->sync.side = side;
        }
    }
  else if (g_str_has_prefix (value, "gpa:states("))
    {
      const char *v, *end;
      char *str;
      uint64_t from, to;

      v = value + strlen ("gpa:states(");
      end = strchr (v, ',');
      if (!end)
        return FALSE;

      str = g_strndup (v, end - v);
      if (!parse_states (svg, str, &from))
        {
          g_free (str);
          return FALSE;
        }
      g_free (str);

      v = end + 1;
      end = strchr (v, ')');
      if (!end)
        return FALSE;

      str = g_strndup (v, end - v);
      if (!parse_states (svg, str, &to))
        {
          g_free (str);
          return FALSE;
        }
      g_free (str);

      spec->type = TIME_SPEC_TYPE_STATES;
      spec->states.from = from;
      spec->states.to = to;
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
                 GtkSvg   *svg,
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
        print_states (s, svg, spec->states.from);
        g_string_append (s, ", ");
        print_states (s, svg, spec->states.to);
        g_string_append (s, ")");
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
                  GtkSvg    *svg,
                  GString   *s)
{
  for (unsigned int i = 0; i < specs->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (specs, i);
      if (i > 0)
        g_string_append (s, "; ");
      time_spec_print (spec, svg, s);
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
      if (state_match (spec->states.from & ~spec->states.to, previous_state) &&
          state_match (spec->states.to & ~spec->states.from, state))
        time_spec_set_time (spec, state_start_time + spec->offset);
    }
}

static int64_t
time_spec_get_state_change_delay (TimeSpec *spec)
{
  if (spec->type == TIME_SPEC_TYPE_STATES && spec->offset < 0)
    return - spec->offset;

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
timeline_get_states (Timeline *timeline,
                     uint64_t  from,
                     uint64_t  to,
                     int64_t   offset)
{
  TimeSpec spec = { .type = TIME_SPEC_TYPE_STATES,
                    .states = { .from = from, .to = to },
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
  size_t line; /* for resolving ties in order */

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
              dbg_print ("times", "current start time of %s now %s\n", a->id, format_time (time));
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
advance_later (gpointer data)
{
  GtkSvg *self = data;

  self->pending_advance = 0;

  gtk_svg_advance (self, MAX (self->current_time, gdk_frame_clock_get_frame_time (self->clock)));
}

static void
schedule_next_update (GtkSvg *self)
{
  GtkSvgRunMode run_mode;

  if (self->clock == NULL || !self->playing)
    return;

  g_clear_handle_id (&self->pending_advance, g_source_remove);

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
      self->pending_advance = g_timeout_add_once (interval, advance_later, self);
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
resolve_value (Shape             *shape,
               SvgComputeContext *context,
               ShapeAttr          attr,
               unsigned int       idx,
               SvgValue          *value)
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
compute_animation_motion_value (Animation         *a,
                                unsigned int       rep,
                                unsigned int       frame,
                                double             frame_t,
                                SvgComputeContext *context)
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
compute_value_at_time (Animation         *a,
                       SvgComputeContext *context)
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
compute_value_for_animation (Animation         *a,
                             SvgComputeContext *context)
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

  /* The situation with animateTransform vs. animateMotion
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
shape_init_current_values (Shape             *shape,
                           SvgComputeContext *context)
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
compute_current_values_for_shape (Shape             *shape,
                                  SvgComputeContext *context)
{
  const graphene_rect_t *old_viewport = context->viewport;
  graphene_rect_t viewport;

  shape_init_current_values (shape, context);

  if (shape->type == SHAPE_SVG || shape->type == SHAPE_SYMBOL)
    {
      SvgValue *vb = shape->current[SHAPE_ATTR_VIEW_BOX];
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

      if (!svg_view_box_get (vb, &viewport))
        graphene_rect_init (&viewport, 0, 0, width, height);

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
shape_apply_state (GtkSvg       *self,
                   Shape        *shape,
                   unsigned int  state)
{
  if (shape_type_has_gpa_attrs (shape->type))
    {
      Visibility visibility;

      if (shape->gpa.states & BIT (state))
        visibility = VISIBILITY_VISIBLE;
      else
        visibility = VISIBILITY_HIDDEN;

      if ((self->features & GTK_SVG_ANIMATIONS) == 0)
        {
          SvgValue *value = svg_visibility_new (visibility);
          shape_set_base_value (shape, SHAPE_ATTR_VISIBILITY, 0, value);
          svg_value_unref (value);
        }

      if (!self->playing && shape->animations)
        {
          for (unsigned int i = shape->animations->len; i > 0; i--)
            {
              Animation *a = g_ptr_array_index (shape->animations, i - 1);

              if ((visibility == VISIBILITY_VISIBLE &&
                   g_str_has_prefix (a->id, "gpa:transition:fade-in")) ||
                  (visibility == VISIBILITY_HIDDEN &&
                   g_str_has_prefix (a->id, "gpa:transition:fade-out")))
                {
                  a->status = ANIMATION_STATUS_DONE;
                  a->previous.begin = self->current_time;
                  a->current.begin = INDEFINITE;
                  a->current.end = INDEFINITE;
                  a->state_changed = TRUE;
                  g_ptr_array_steal_index (shape->animations, i - 1);
                  g_ptr_array_add (shape->animations, a);
                }
              if (g_str_has_prefix (a->id, "gpa:out-of-state"))
                {
                  if (visibility == VISIBILITY_HIDDEN)
                    {
                      a->status = ANIMATION_STATUS_RUNNING;
                      a->previous.begin = self->current_time;
                      a->current.begin = self->current_time;
                      a->current.end = INDEFINITE;
                    }
                  else
                    {
                      a->status = ANIMATION_STATUS_DONE;
                      a->previous.begin = self->current_time;
                      a->current.begin = INDEFINITE;
                      a->current.end = INDEFINITE;
                    }
                  a->state_changed = TRUE;
                  g_ptr_array_steal_index (shape->animations, i - 1);
                  g_ptr_array_add (shape->animations, a);
                }
              if (g_str_has_prefix (a->id, "gpa:in-state"))
                {
                  if (visibility == VISIBILITY_VISIBLE)
                    {
                      a->status = ANIMATION_STATUS_RUNNING;
                      a->previous.begin = self->current_time;
                      a->current.begin = self->current_time;
                      a->current.end = INDEFINITE;
                    }
                  else
                    {
                      a->status = ANIMATION_STATUS_DONE;
                      a->previous.begin = self->current_time;
                      a->current.begin = INDEFINITE;
                      a->current.end = INDEFINITE;
                    }
                  a->state_changed = TRUE;
                  g_ptr_array_steal_index (shape->animations, i - 1);
                  g_ptr_array_add (shape->animations, a);
                }
            }
        }
    }

  if (shape_type_has_shapes (shape->type))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          Shape *sh = g_ptr_array_index (shape->shapes, i);
          shape_apply_state (self, sh, state);
        }
    }
}

static void
apply_state (GtkSvg   *self,
             uint64_t  state)
{
  shape_apply_state (self, self->content, state);
}

static gboolean
strokewidth_parse (const char  *value,
                   SvgValue   **values)
{
  GBytes *bytes;
  GtkCssParser *parser;
  unsigned int i;
  gboolean retval = TRUE;

  bytes = g_bytes_new_static (value, strlen (value));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL);

  for (i = 0; i < 3; i++)
    {
      gtk_css_parser_skip_whitespace (parser);
      values[i] = svg_number_parse (parser, -DBL_MAX, DBL_MAX, NUMBER|LENGTH|PERCENTAGE);
      if (!values[i])
        retval = FALSE;
    }

  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    retval = FALSE;

  gtk_css_parser_unref (parser);
  g_bytes_unref (bytes);

  return retval;
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
      begin = animation_add_begin (a, timeline_get_states (timeline, states, ALL_STATES & ~states, MAX (0, - delay)));
      time_spec_add_animation (begin, a);

      end = animation_add_end (a, timeline_get_states (timeline, ALL_STATES & ~states, states, - (MAX (0, - delay))));
      time_spec_add_animation (end, a);

      opposite_visibility = VISIBILITY_HIDDEN;
    }
  else
    {
      a->id = g_strdup_printf ("gpa:in-state:%s", shape->id);
      begin = animation_add_begin (a, timeline_get_states (timeline, ALL_STATES & ~states, states, MAX (0, - delay)));
      time_spec_add_animation (begin, a);

      end = animation_add_end (a, timeline_get_states (timeline, states, ALL_STATES & ~states, - (MAX (0, - delay))));
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
  if (states != ALL_STATES)
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

  a->id = g_strdup_printf ("gpa:path-length:%s", shape->id);
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
                   unsigned int   idx,
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
  a->idx = idx;
  a->simple_duration = duration;
  a->repeat_duration = duration;
  a->repeat_count = 1;

  a->has_begin = 1;
  a->has_simple_duration = 1;
  a->has_repeat_duration = 1;

  a->id = g_strdup_printf ("gpa:transition:fade-in:%u:%s:%s", idx, shape_attr_get_name (attr), shape->id);

  begin = animation_add_begin (a, timeline_get_states (timeline, ALL_STATES & ~states, states, delay));

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
  a->idx = idx;
  a->simple_duration = duration;
  a->repeat_duration = duration;
  a->repeat_count = 1;

  a->has_begin = 1;
  a->has_simple_duration = 1;
  a->has_repeat_duration = 1;

  a->id = g_strdup_printf ("gpa:transition:fade-out:%u:%s:%s", idx, shape_attr_get_name (attr), shape->id);

  begin = animation_add_begin (a, timeline_get_states (timeline, states, ALL_STATES & ~states, - (duration + delay)));

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
      a->idx = idx;
      a->attr = attr;
      a->simple_duration = duration;
      a->repeat_duration = duration;
      a->repeat_count = 1;

      a->id = g_strdup_printf ("gpa:transition:delay-in:%u:%s:%s", idx, shape_attr_get_name (attr), shape->id);
      begin = animation_add_begin (a, timeline_get_states (timeline, ALL_STATES & ~states, states, 0));
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
      a->idx = idx;
      a->attr = attr;
      a->simple_duration = duration;
      a->repeat_duration = duration;
      a->repeat_count = 1;

      a->id = g_strdup_printf ("gpa:transition:delay-out:%u:%s:%s", idx, shape_attr_get_name (attr), shape->id);
      begin = animation_add_begin (a, timeline_get_states (timeline, states, ALL_STATES & ~states, 0));
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

  begin = animation_add_begin (a, timeline_get_states (timeline, ALL_STATES & ~states, states, 0));

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

  begin = animation_add_begin (a, timeline_get_states (timeline, states, ALL_STATES & ~states, -delay));

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

/* The filter we create here looks roughly like this:
 *
 * <feGaussianBlur -> blurred
 * <feComponentTransfer in=SourceGraphic ... make source alpha solid
 * <feGaussianBlur
 * <feComponentTransfer threshold alpha, white-out color -> blobbed
 * <feComposite ...multiply blurred and blobbed
 *
 * The blurs and the second component transfer are animated
 * from their full effect to identity.
 */
static void
create_morph_filter (Shape      *shape,
                     Timeline   *timeline,
                     GHashTable *shapes,
                     uint64_t    states,
                     int64_t     duration,
                     int64_t     delay,
                     GpaEasing   easing)
{
  Shape *parent = NULL;
  Shape *filter;
  SvgValue *value;
  Animation *a;
  unsigned int idx;
  char *str;
  TimeSpec *begin;
  TimeSpec *end;

  for (unsigned int i = 0; i < shape->parent->shapes->len; i++)
    {
      Shape *sh = g_ptr_array_index (shape->parent->shapes, i);

      if (sh == shape)
        break;

      if (sh->type == SHAPE_DEFS)
        {
          parent = sh;
          break;
        }
    }

  if (parent == NULL)
    {
      parent = shape_new (shape->parent, SHAPE_DEFS);
      g_ptr_array_insert (shape->parent->shapes, 0, parent);
    }

  filter = svg_shape_add (parent, SHAPE_FILTER);
  filter->id = g_strdup_printf ("gpa:morph-filter:%s", shape->id);

  g_hash_table_insert (shapes, filter->id, filter);

  value = svg_percentage_new (-50);
  shape_set_base_value (filter, SHAPE_ATTR_X, 0, value);
  shape_set_base_value (filter, SHAPE_ATTR_Y, 0, value);
  svg_value_unref (value);
  value = svg_percentage_new (200);
  shape_set_base_value (filter, SHAPE_ATTR_WIDTH, 0, value);
  shape_set_base_value (filter, SHAPE_ATTR_HEIGHT, 0, value);
  svg_value_unref (value);

  idx = shape_add_filter (filter, FE_BLUR);
  str = g_strdup_printf ("gpa:morph-filter:%s:blurred", shape->id);
  value = svg_string_new_take (str);
  shape_set_base_value (filter, SHAPE_ATTR_FE_RESULT, idx + 1, value);
  svg_value_unref (value);

  create_transition (filter, idx + 1, timeline, states,
                     duration, delay, easing,
                     0, GPA_TRANSITION_MORPH,
                     SHAPE_ATTR_FE_STD_DEV,
                     svg_numbers_new1 (4),
                     svg_numbers_new1 (0));

  idx = shape_add_filter (filter, FE_COMPONENT_TRANSFER);
  value = svg_filter_primitive_ref_new (SOURCE_GRAPHIC);
  shape_set_base_value (filter, SHAPE_ATTR_FE_IN, idx + 1, value);
  svg_value_unref (value);

  idx = shape_add_filter (filter, FE_FUNC_A);
  value = svg_component_transfer_type_new (COMPONENT_TRANSFER_LINEAR);
  shape_set_base_value (filter, SHAPE_ATTR_FE_FUNC_TYPE, idx + 1, value);
  svg_value_unref (value);
  value = svg_number_new (100);
  shape_set_base_value (filter, SHAPE_ATTR_FE_FUNC_SLOPE, idx + 1, value);
  svg_value_unref (value);
  value = svg_number_new (0);
  shape_set_base_value (filter, SHAPE_ATTR_FE_FUNC_INTERCEPT, idx + 1, value);
  svg_value_unref (value);

  idx = shape_add_filter (filter, FE_BLUR);

  create_transition (filter, idx + 1, timeline, states,
                     duration, delay, easing,
                     0, GPA_TRANSITION_MORPH,
                     SHAPE_ATTR_FE_STD_DEV,
                     svg_numbers_new1 (4),
                     svg_numbers_new1 (0));

  idx = shape_add_filter (filter, FE_COMPONENT_TRANSFER);

  for (unsigned int func = FE_FUNC_R; func <= FE_FUNC_B; func++)
    {
      idx = shape_add_filter (filter, func);
      value = svg_component_transfer_type_new (COMPONENT_TRANSFER_LINEAR);
      shape_set_base_value (filter, SHAPE_ATTR_FE_FUNC_TYPE, idx + 1, value);
      svg_value_unref (value);
      value = svg_number_new (0);
      shape_set_base_value (filter, SHAPE_ATTR_FE_FUNC_SLOPE, idx + 1, value);
      svg_value_unref (value);
      value = svg_number_new (1);
      shape_set_base_value (filter, SHAPE_ATTR_FE_FUNC_INTERCEPT, idx + 1, value);
      svg_value_unref (value);
    }

  idx = shape_add_filter (filter, FE_FUNC_A);
  value = svg_component_transfer_type_new (COMPONENT_TRANSFER_LINEAR);
  shape_set_base_value (filter, SHAPE_ATTR_FE_FUNC_TYPE, idx + 1, value);
  svg_value_unref (value);

  create_transition (filter, idx + 1, timeline, states,
                     duration, delay, easing,
                     0, GPA_TRANSITION_MORPH,
                     SHAPE_ATTR_FE_FUNC_SLOPE,
                     svg_number_new (100),
                     svg_number_new (1));

  create_transition (filter, idx + 1, timeline, states,
                     duration, delay, easing,
                     0, GPA_TRANSITION_MORPH,
                     SHAPE_ATTR_FE_FUNC_INTERCEPT,
                     svg_number_new (-20),
                     svg_number_new (0));

  idx = shape_add_filter (filter, FE_COMPOSITE);
  value = svg_composite_operator_new (COMPOSITE_OPERATOR_ARITHMETIC);
  shape_set_base_value (filter, SHAPE_ATTR_FE_COMPOSITE_OPERATOR, idx + 1, value);
  svg_value_unref (value);
  value = svg_number_new (1);
  shape_set_base_value (filter, SHAPE_ATTR_FE_COMPOSITE_K1, idx + 1, value);
  svg_value_unref (value);
  str = g_strdup_printf ("gpa:morph-filter:%s:blurred", shape->id);
  value = svg_filter_primitive_ref_new_ref (str);
  g_free (str);
  shape_set_base_value (filter, SHAPE_ATTR_FE_IN2, idx + 1, value);
  svg_value_unref (value);

  a = animation_set_new ();
  a->id = g_strdup_printf ("gpa:set:morph:%s", shape->id);
  a->attr = SHAPE_ATTR_FILTER;

  begin = animation_add_begin (a, timeline_get_start_of_time (timeline));
  time_spec_add_animation (begin, a);
  end = animation_add_end (a, timeline_get_end_of_time (timeline));
  time_spec_add_animation (end, a);

  a->has_begin = 1;
  a->has_end = 1;

  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  str = g_strdup_printf ("url(#%s)", filter->id);
  a->frames[0].value = svg_filter_parse (str);
  a->frames[1].value = svg_value_ref (a->frames[0].value);
  g_free (str);

  shape_add_animation (shape, a);
}

static void
create_transitions (Shape         *shape,
                    Timeline      *timeline,
                    GHashTable    *shapes,
                    GHashTable    *pending_refs,
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
      create_transition (shape, 0, timeline, states,
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
        create_transition (shape, 0, timeline, states,
                           duration, delay, easing,
                           origin, type,
                           SHAPE_ATTR_STROKE_DASHOFFSET,
                           svg_number_new (-origin),
                           svg_number_new (0));
      break;
    case GPA_TRANSITION_MORPH:
      create_morph_filter (shape, timeline, shapes, states,
                           duration, delay, easing);
      g_hash_table_add (pending_refs, shape);
      break;
    case GPA_TRANSITION_FADE:
      create_transition (shape, 0, timeline, states,
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

  begin = animation_add_begin (a, timeline_get_states (timeline, ALL_STATES & ~states, states, 0));
  time_spec_add_animation (begin, a);

  if (state_match (states, initial))
    {
      begin = animation_add_begin (a, timeline_get_start_of_time (timeline));
      time_spec_add_animation (begin, a);
    }

  end = animation_add_end (a, timeline_get_states (timeline, states, ALL_STATES & ~states, 0));
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
  if (sh->animations == NULL)
    return;

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
  GHashTable *pending_refs;
  struct {
    const GSList *to;
    GtkSvgLocation start;
    GtkSvgError code;
    char *reason;
    gboolean skip_over_target;
  } skip;
  struct {
    GtkSvgLocation start;
    gboolean collect;
    GString *text;
  } text;
  uint64_t num_loaded_elements;
  GArray *rulesets;
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
  const char *href_attr_name = "href";

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
    {
      href_attr = xlink_href_attr;
      href_attr_name = "xlink:href";
    }

  if (href_attr)
    {
      if (href_attr[0] != '#')
        gtk_svg_invalid_attribute (data->svg, context, attr_names, href_attr_name,
                                   "Missing '#' in 'href'");
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

          if (!time_spec_parse (data->svg, &spec, strv[i]))
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names, "begin", NULL);
              g_clear_error (&error);
              continue;
            }

          a->has_begin = 1;
          begin = animation_add_begin (a, timeline_get_time_spec (data->svg->timeline, &spec));
          time_spec_add_animation (begin, a);
          time_spec_clear (&spec);
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

          if (!time_spec_parse (data->svg, &spec, strv[i]))
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names, "end", NULL);
              g_clear_error (&error);
              continue;
            }
          a->has_end = 1;
          end = animation_add_end (a, timeline_get_time_spec (data->svg->timeline, &spec));
          time_spec_add_animation (end, a);
          time_spec_clear (&spec);
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
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "dur", NULL);
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
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "repeatCount", NULL);
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
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "repeatDur", NULL);
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
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "fill", NULL);
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
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "restart", NULL);
      else
        a->restart = (AnimationRestart) value;
    }

  if (attr_name_attr && strcmp (attr_name_attr, "xlink:href") == 0)
    attr_name_attr = "href";

  if (current_shape != NULL &&
      current_shape->type == SHAPE_FILTER &&
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
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "attributeName",
                                   "Not allowed on <animateMotion>");
    }
  else if (a->type == ANIMATION_TYPE_TRANSFORM)
    {
      if (current_shape != NULL)
        {
          const char *expected;

          /* FIXME: if href is set, current_shape might be the wrong shape */
          expected = shape_attr_get_presentation (SHAPE_ATTR_TRANSFORM, current_shape->type);
          if (expected == NULL)
            gtk_svg_invalid_attribute (data->svg, context, attr_names, "attributeName",
                                       "No transform attribute");
          else if (attr_name_attr && strcmp (attr_name_attr, expected) != 0)
            gtk_svg_invalid_attribute (data->svg, context, attr_names, "attributeName",
                                       "Value must be '%s'", expected);
        }

      a->attr = SHAPE_ATTR_TRANSFORM;
    }
  else if (!attr_name_attr)
    {
      gtk_svg_missing_attribute (data->svg, context, "attributeName", NULL);
      return FALSE;
    }
  /* FIXME: if href is set, current_shape might be the wrong shape */
  else if (current_shape != NULL &&
           ((current_shape->type == SHAPE_FILTER &&
             filter_attr_lookup (filter_type, attr_name_attr, &attr, &deprecated)) ||
            (current_shape->type != SHAPE_FILTER &&
             shape_attr_lookup (attr_name_attr, current_shape->type, &attr, &deprecated))))
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
      gtk_svg_invalid_attribute (data->svg, context, attr_names, "attributeName",
                                 "Can't animate '%s'", attr_name_attr);
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
              gtk_svg_invalid_attribute (data->svg, context, attr_names, "type", NULL);
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
      gtk_svg_invalid_attribute (data->svg, context, attr_names, "type", NULL);
    }

  if (calc_mode_attr)
    {
      unsigned int value;

      if (!parse_enum (calc_mode_attr,
                       (const char *[]) { "discrete", "linear", "paced", "spline" }, 4,
                       &value))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "calcMode", NULL);
      else
        a->calc_mode = (CalcMode) value;
   }

  if (additive_attr)
    {
      unsigned int value;

      if (!parse_enum (additive_attr,
                       (const char *[]) { "replace", "sum", }, 2,
                       &value))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "additive", NULL);
      else
        a->additive = (AnimationAdditive) value;
   }

  if (accumulate_attr)
    {
      unsigned int value;

      if (!parse_enum (accumulate_attr,
                       (const char *[]) { "none", "sum", }, 2,
                       &value))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "accumulate", NULL);
      else
        a->accumulate = (AnimationAccumulate) value;
   }

  if (color_interpolation_attr)
    {
      SvgValue *v;
      GError *error = NULL;

      v = shape_attr_parse_value (SHAPE_ATTR_COLOR_INTERPOLATION, color_interpolation_attr, &error);
      if (!v)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "color-interpolation", "%s", error->message);
          g_error_free (error);
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
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "values", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }
    }
  else if (from_attr && to_attr)
    {
      GPtrArray *tovals;

      values = shape_attr_parse_values (a->attr, transform_type, from_attr);
      if (!values || values->len != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "from", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }

      tovals = shape_attr_parse_values (a->attr, transform_type, to_attr);
      if (!tovals || tovals->len != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "to", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&tovals, g_ptr_array_unref);
          return FALSE;
        }

      g_ptr_array_extend_and_steal (values, tovals);
    }
  else if (from_attr && by_attr)
    {
      GPtrArray *byvals;
      SvgValue *from;
      SvgValue *by;
      SvgValue *to;
      SvgComputeContext ctx = { 0, };

      values = shape_attr_parse_values (a->attr, transform_type, from_attr);

      if (!values || values->len != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "from", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }

      byvals = shape_attr_parse_values (a->attr, transform_type, by_attr);
      if (!byvals || byvals->len != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "by", NULL);
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
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "to", NULL);
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
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "by", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }

      by = g_ptr_array_index (values, 0);
      if (svg_value_is_number (by))
        from = svg_number_new_full (svg_number_get_unit (by), 0);
      else if (svg_value_is_transform (by))
        from = svg_transform_new_none ();
      else if (svg_value_is_paint (by) &&
               svg_paint_get_kind (by) == PAINT_COLOR)
        from = svg_paint_new_transparent ();
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "by",
                                     "Don't know how to handle this value");
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
          double vals[6];

          points = g_array_new (FALSE, FALSE, sizeof (double));

          for (unsigned int i = 0; i < values->len; i++)
            {
              SvgValue *tf = g_ptr_array_index (values, i);
              TransformType type;

              type = svg_transform_get_primitive (tf, 0, vals);

              if (svg_transform_get_length (tf) != 1 || type != TRANSFORM_TRANSLATE)
                {
                  gtk_svg_invalid_attribute (data->svg, context, attr_names, NULL,
                                             "Transform is not a translation");
                  g_ptr_array_unref (values);
                  g_array_unref (points);
                  return FALSE;
                }

              if (i == 0)
                {
                  length = 0;
                  gsk_path_builder_move_to (builder, vals[0], vals[1]);
                }
              else
                {
                  length += graphene_point_distance (gsk_path_builder_get_current_point (builder),
                                                     &GRAPHENE_POINT_INIT (vals[0], vals[1]), NULL, NULL);
                  gsk_path_builder_line_to (builder, vals[0], vals[1]);
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
          gtk_svg_invalid_attribute (data->svg, context, attr_names, NULL,
                                     "Either 'values' or 'from', 'to' or "
                                     "'by' must be given");
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
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "keyTimes", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&points, g_array_unref);
          return FALSE;
        }
    }

  if (times != NULL)
    {
      if (values && times->len != values->len)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, NULL,
                                     "'values' and 'keyTimes' must have "
                                     "the same number of items");
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&times, g_array_unref);
          g_clear_pointer (&points, g_array_unref);
          return FALSE;
        }

      if (times->len == 0)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "keyTimes",
                                     "No values");
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&times, g_array_unref);
          g_clear_pointer (&points, g_array_unref);
          return FALSE;
        }

      if (g_array_index (times, double, 0) != 0)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "keyTimes",
                                     "First value must be 0");
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&times, g_array_unref);
          g_clear_pointer (&points, g_array_unref);
          return FALSE;
        }

      if (a->calc_mode != CALC_MODE_DISCRETE && g_array_index (times, double, times->len - 1) != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "keyTimes",
                                     "Last value must be 1");
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&times, g_array_unref);
          g_clear_pointer (&points, g_array_unref);
          return FALSE;
        }

      for (unsigned int i = 1; i < times->len; i++)
        {
          if (g_array_index (times, double, i) < g_array_index (times, double, i - 1))
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names, "keyTimes",
                                         "Values must be increasing");
              g_clear_pointer (&values, g_ptr_array_unref);
              g_clear_pointer (&times, g_array_unref);
              g_clear_pointer (&points, g_array_unref);
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
                  gtk_svg_invalid_attribute (data->svg, context, attr_names, "keySplines", NULL);
                  g_clear_pointer (&values, g_ptr_array_unref);
                  g_clear_pointer (&times, g_array_unref);
                  g_clear_pointer (&params, g_array_unref);
                  g_clear_pointer (&points, g_array_unref);
                  return FALSE;
                }

              g_array_append_vals (params, spline, 4);
            }

          g_strfreev (strv);

          if (times == NULL)
            times = create_default_times (a->calc_mode, n + 1);

          if (n != times->len - 1)
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names, "keySplines",
                                         "Wrong number of values");
              g_clear_pointer (&values, g_ptr_array_unref);
              g_clear_pointer (&times, g_array_unref);
              g_clear_pointer (&params, g_array_unref);
              g_clear_pointer (&points, g_array_unref);
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

  if (times->len < 2 ||
      (values && times->len != values->len) ||
      (params && 4 * (times->len - 1) != params->len) ||
      (points && times->len != points->len))
    {
      gtk_svg_invalid_attribute (data->svg, context, attr_names, NULL,
                                 "Invalid value attributes");
      g_clear_pointer (&values, g_ptr_array_unref);
      g_clear_pointer (&times, g_array_unref);
      g_clear_pointer (&params, g_array_unref);
      g_clear_pointer (&points, g_array_unref);
      return FALSE;
    }

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
      SvgValue *value;
      GError *error = NULL;

      value = parse_path (path_attr, &error);
      if (error)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "path", "%s", error->message);
          g_error_free (error);
        }
      else if (!value)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "path", NULL);
        }

      if (value)
        {
          g_clear_pointer (&a->motion.path, gsk_path_unref);
          a->motion.path = gsk_path_ref (svg_path_get_gsk (value));
          svg_value_unref (value);
        }
      else
        return FALSE;
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
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "rotate", NULL);
    }

  if (a->calc_mode != CALC_MODE_PACED)
    {
      if (key_points_attr)
        {
          GArray *points = parse_numbers2 (key_points_attr, ";", 0, 1);
          if (!points)
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names, "keyPoints", NULL);
              g_array_unref (points);
              return FALSE;
            }

          if (points->len != a->n_frames)
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names, "keyPoints",
                                         "Wrong number of values");
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

      if (a->motion.path && !gsk_path_is_empty (a->motion.path))
        {
          fill_from_path (a, a->motion.path);
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "calcMode",
                                     "Paced animation without explicit path is not implemented");
          return FALSE;
        }
    }

  return TRUE;
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

      if (strcmp (attr_names[i], "class") == 0)
        {
          *handled |= BIT (i);
          shape->classes = g_strsplit (attr_values[i], " ", 0);
          gtk_css_node_set_classes (shape->css_node, (const char **) shape->classes);
        }
      else if (strcmp (attr_names[i], "style") == 0)
        {
          GtkSvgLocation end;

          gtk_svg_location_init_attr_range (&shape->style_loc, &end, context, i);
          *handled |= BIT (i);
          shape->style = g_strdup (attr_values[i]);
        }
      else if (strcmp (attr_names[i], "id") == 0)
        {
          *handled |= BIT (i);
          shape->id = g_strdup (attr_values[i]);
          gtk_css_node_set_id (shape->css_node, g_quark_from_string (shape->id));
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
                  SvgValue *value;
                  GError *error = NULL;

                  value = shape_attr_parse_and_validate (attr, attr_values[i], &error);
                  /* It is possible that a value is returned and error
                   * is still set, e.g. for 'd' or 'points'
                   */
                  if (error)
                    {
                      gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], "%s", error->message);
                      g_error_free (error);
                    }
                  else if (!value)
                    {
                      gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], NULL);
                    }

                  if (value)
                    {
                      shape_set_base_value (shape, attr, 0, value);
                      svg_value_unref (value);
                    }
                }
            }
          else
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], "Unknown attribute: %s", attr_names[i]);
            }

          *handled |= BIT (i);
        }
    }

  if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_CLIP_PATH) ||
      _gtk_bitmask_get (shape->attrs, SHAPE_ATTR_MASK) ||
      _gtk_bitmask_get (shape->attrs, SHAPE_ATTR_HREF) ||
      _gtk_bitmask_get (shape->attrs, SHAPE_ATTR_FILTER) ||
      _gtk_bitmask_get (shape->attrs, SHAPE_ATTR_MARKER_START) ||
      _gtk_bitmask_get (shape->attrs, SHAPE_ATTR_MARKER_MID) ||
      _gtk_bitmask_get (shape->attrs, SHAPE_ATTR_MARKER_END))
    g_hash_table_add (data->pending_refs, shape);

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
  const char *state_names_attr = NULL;
  const char *state_attr = NULL;
  const char *version_attr = NULL;
  const char *keywords_attr = NULL;

  markup_filter_attributes (element_name,
                            attr_names, attr_values,
                            handled,
                            "gpa:state-names", &state_names_attr,
                            "gpa:state", &state_attr,
                            "gpa:version", &version_attr,
                            "gpa:keywords", &keywords_attr,
                            NULL);

  if (state_names_attr)
    {
      GStrv strv = strsplit_set (state_names_attr, " ");

      if (strv == NULL)
        {
          gtk_svg_invalid_attribute (svg, context, attr_names, "gpa:state-names", NULL);
        }
      else
        {
          if (!gtk_svg_set_state_names (svg, (const char **) strv))
            gtk_svg_invalid_attribute (svg, context, attr_names, "gpa:state-names", NULL);
          g_strfreev (strv);
        }
    }

  if (state_attr)
    {
      double v;
      unsigned int state;

      if (parse_number (state_attr, 0, 63, &v))
        {
          svg->initial_state = (unsigned int) v;
          gtk_svg_set_state (svg, (unsigned int) v);
        }
      else if (find_named_state (svg, state_attr, &state))
        {
          svg->initial_state = state;
          gtk_svg_set_state (svg, state);
        }
      else
        gtk_svg_invalid_attribute (svg, context, attr_names, "gpa:state", NULL);
    }

  if (version_attr)
    {
      unsigned int version;
      char *end;

      version = (unsigned int) g_ascii_strtoull (version_attr, &end, 10);
      if ((end && *end != '\0') || version != 1)
        gtk_svg_invalid_attribute (svg, context, attr_names, "gpa:version",
                                   "Must be 1");
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
          shape->gpa.stroke = svg_value_ref (value);
          svg_value_unref (value);
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:stroke", NULL);
        }
    }

  if (fill_attr)
    {
      value = svg_paint_parse_gpa (fill_attr);
      if (value)
        {
          shape_set_base_value (shape, SHAPE_ATTR_FILL, 0, value);
          shape->gpa.fill = svg_value_ref (value);
          svg_value_unref (value);
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:fill", NULL);
        }
    }

  if (strokewidth_attr)
    {
      SvgValue *values[3] = { NULL, };

      if (strokewidth_parse (strokewidth_attr, values) &&
          values[0] && values[1] && values[2])
        {
          shape_set_base_value (shape, SHAPE_ATTR_STROKE_MINWIDTH, 0, values[0]);
          shape_set_base_value (shape, SHAPE_ATTR_STROKE_WIDTH, 0, values[1]);
          shape_set_base_value (shape, SHAPE_ATTR_STROKE_MAXWIDTH, 0, values[2]);
          shape->gpa.width = svg_value_ref (values[1]);
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:stroke-width", NULL);
        }

      g_clear_pointer (&values[0], svg_value_unref);
      g_clear_pointer (&values[1], svg_value_unref);
      g_clear_pointer (&values[2], svg_value_unref);
    }

  states = ALL_STATES;
  if (states_attr)
    {
      if (!parse_states (data->svg, states_attr, &states))
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:states", NULL);
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
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:origin", NULL);
    }

  transition_type = GPA_TRANSITION_NONE;
  if (transition_type_attr)
    {
      if (!parse_enum (transition_type_attr,
                       (const char *[]) { "none", "animate", "morph", "fade" }, 4,
                        &transition_type))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:transition-type", NULL);
    }

  transition_duration = 0;
  if (transition_duration_attr)
    {
      if (!parse_duration (transition_duration_attr, &transition_duration))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:transition-duration", NULL);
    }

  transition_delay = 0;
  if (transition_delay_attr)
    {
      if (!parse_duration (transition_delay_attr, &transition_delay))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:transition-delay", NULL);
    }

  transition_easing = GPA_EASING_LINEAR;
  if (transition_easing_attr)
    {

      if (!parse_enum (transition_easing_attr,
                       (const char *[]) { "linear", "ease-in-out", "ease-in",
                                          "ease-out", "ease" }, 5,
                        &transition_easing))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:transition-easing", NULL);
    }

  has_animation = 1;
  if (animation_type_attr)
    {
      if (!parse_enum (animation_type_attr,
                       (const char *[]) { "none", "automatic", }, 2,
                        &has_animation))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:animation-type", NULL);
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
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:animation-direction", NULL);
    }

  animation_duration = 0;
  if (animation_duration_attr)
    {
      if (!parse_duration (animation_duration_attr, &animation_duration))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:animation-duration", NULL);
    }

  animation_repeat = REPEAT_FOREVER;
  if (animation_repeat_attr)
    {
      if (match_str (animation_repeat_attr, "indefinite"))
        animation_repeat = REPEAT_FOREVER;
      else if (!parse_number (animation_repeat_attr, 0, DBL_MAX, &animation_repeat))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:animation-repeat", NULL);
    }

  animation_segment = 0.2;
  if (animation_segment_attr)
    {
      if (!parse_number (animation_segment_attr, 0, 1, &animation_segment))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:animation-segment", NULL);
    }

  animation_easing = GPA_EASING_LINEAR;
  if (animation_easing_attr)
    {
      if (!parse_enum (animation_easing_attr,
                       (const char *[]) { "linear", "ease-in-out", "ease-in",
                                          "ease-out", "ease" }, 5,
                        &animation_easing))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:animation-easing", NULL);
    }

  attach_pos = 0;
  if (attach_pos_attr)
    {
      if (!parse_number (attach_pos_attr, 0, 1, &attach_pos))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:attach-pos", NULL);
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
    g_hash_table_add (data->pending_refs, shape);

  if (shape->gpa.transition != GPA_TRANSITION_NONE ||
      shape->gpa.animation != GPA_ANIMATION_NONE)
    {
      /* our dasharray-based animations require unit path length */
      if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_PATH_LENGTH))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "pathLength",
                                   "Can't set '%s' and use gpa features", "pathLength");

      if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_STROKE_DASHARRAY))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "stroke-dasharray",
                                   "Can't set '%s' and use gpa features", "stroke-dasharray");

      if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_STROKE_DASHOFFSET))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "stroke-dashoffset",
                                   "Can't set '%s' and use gpa features", "stroke-dashoffset");

      if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_FILTER) && shape->gpa.transition == GPA_TRANSITION_MORPH)
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "filter",
                                   "Can't set '%s' and use gpa features", "filter");
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
                      data->shapes,
                      data->pending_refs,
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
/* {{{ Color Stop attributes */

static void
parse_color_stop_attrs (Shape                *shape,
                        unsigned int          idx,
                        ColorStop            *stop,
                        const char           *element_name,
                        const char          **attr_names,
                        const char          **attr_values,
                        uint64_t             *handled,
                        ParserData           *data,
                        GMarkupParseContext  *context)
{
  for (unsigned int i = 0; attr_names[i]; i++)
    {
      ShapeAttr attr;
      gboolean deprecated;

      if (strcmp (attr_names[i], "id") == 0)
        {
          *handled |= BIT (i);
          stop->id = g_strdup (attr_values[i]);
          gtk_css_node_set_id (stop->css_node, g_quark_from_string (stop->id));
        }
      else if (strcmp (attr_names[i], "style") == 0)
        {
          *handled |= BIT (i);
          stop->style = g_strdup (attr_values[i]);
        }
      else if (strcmp (attr_names[i], "class") == 0)
        {
          *handled |= BIT (i);
          stop->classes = g_strsplit (attr_values[i], " ", 0);
          gtk_css_node_set_classes (stop->css_node, (const char **) stop->classes);
        }

      else if (color_stop_attr_lookup (attr_names[i], shape->type, &attr, &deprecated))
        {
          SvgValue *value;
          GError *error = NULL;

          *handled |= BIT (i);
          value = shape_attr_parse_and_validate (attr, attr_values[i], &error);
          if (error)
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], "%s", error->message);
              g_error_free (error);
            }
          else if (!value)
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], NULL);
            }

          if (value)
            {
              shape_set_base_value (data->current_shape, attr, idx, value);
              svg_value_unref (value);
            }
        }
      else
        gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], "Unknown attribute: %s", attr_names[i]);
    }
}

/* }}} */
/* {{{ Filter attributes */

static void
parse_filter_attrs (Shape                *shape,
                    unsigned int          idx,
                    FilterPrimitive      *f,
                    const char           *element_name,
                    const char          **attr_names,
                    const char          **attr_values,
                    uint64_t             *handled,
                    ParserData           *data,
                    GMarkupParseContext  *context)
{
  for (unsigned int i = 0; attr_names[i]; i++)
    {
      ShapeAttr attr;
      gboolean deprecated;

      if (strcmp (attr_names[i], "id") == 0)
        {
          *handled |= BIT (i);
          f->id = g_strdup (attr_values[i]);
          gtk_css_node_set_id (f->css_node, g_quark_from_string (f->id));
        }
      else if (strcmp (attr_names[i], "style") == 0)
        {
          *handled |= BIT (i);
          f->style = g_strdup (attr_values[i]);
        }
      else if (strcmp (attr_names[i], "class") == 0)
        {
          *handled |= BIT (i);
          f->classes = g_strsplit (attr_values[i], " ", 0);
          gtk_css_node_set_classes (f->css_node, (const char **) f->classes);
        }
      else if (filter_attr_lookup (f->type, attr_names[i], &attr, &deprecated))
        {
          if (deprecated && (f->attrs & BIT (filter_attr_idx (f->type, attr))) != 0)
            {
              /* ignore */
            }
          else
            {
              SvgValue *value;
              GError *error = NULL;

              *handled |= BIT (i);
              value = shape_attr_parse_and_validate (attr, attr_values[i], &error);
              if (error)
                {
                  gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], "%s", error->message);
                  g_error_free (error);
                }
              else if (!value)
                {
                  gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], NULL);
                }

              if (value)
                {
                  shape_set_base_value (data->current_shape, attr, idx, value);
                  svg_value_unref (value);
                }
            }
        }
      else
        gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], "Unknown attribute: %s", attr_names[i]);
    }
}

/* }}} */

G_GNUC_PRINTF (4, 5)
static void
skip_element (ParserData          *data,
              GMarkupParseContext *context,
              GtkSvgError          code,
              const char          *format,
              ...)
{
  va_list args;

  gtk_svg_location_init_tag_start (&data->skip.start, context);
  data->skip.to = g_markup_parse_context_get_element_stack (context);
  data->skip.skip_over_target = TRUE;
  data->skip.code = code;

  va_start (args, format);
  g_vasprintf (&data->skip.reason, format, args);
  va_end (args);
}

static void
start_collect_text (ParserData          *data,
                    GMarkupParseContext *context)
{
  gtk_svg_location_init (&data->text.start, context);
  data->text.collect = TRUE;
  g_string_set_size (data->text.text, 0);
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
  uint64_t handled = 0;
  FilterPrimitiveType filter_type;
  GtkSvgLocation location;

  if (data->skip.to)
    return;

  gtk_svg_location_init_tag_start (&location, context);

  if (data->num_loaded_elements++ > LOADING_LIMIT)
    {
      data->skip.start = location;
      data->skip.to = g_markup_parse_context_get_element_stack (context)->next;
      data->skip.code = GTK_SVG_ERROR_LIMITS_EXCEEDED;
      data->skip.reason = g_strdup ("Loading limit exceeded");
      data->skip.skip_over_target = FALSE;
      return;
    }

  if (shape_type_lookup (element_name, &shape_type))
    {
      Shape *shape = NULL;

      if (data->current_shape &&
          !shape_type_has_shapes (data->current_shape->type))
        {
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Parent element can't contain shapes");
          return;
        }

      if (shape_type != SHAPE_USE &&
          (BIT (shape_type) & CLIP_PATH_CONTENT) == 0 &&
          has_ancestor (context, "clipPath") &&
          shape_type != SHAPE_CLIP_PATH)
        {
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "<clipPath> can only contain shapes, not %s", element_name);
          return;
        }

      shape = shape_new (data->current_shape, shape_type);
      shape->line = location.lines;

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


      if (shape->type == SHAPE_TSPAN)
        {
          Shape *text_parent = NULL;

          if (data->current_shape &&
              shape_type_has_text (data->current_shape->type))
            text_parent = data->current_shape;
          else if (data->current_shape && data->current_shape->parent &&
                   data->current_shape->type == SHAPE_LINK &&
                   shape_type_has_text (data->current_shape->parent->type))
            text_parent = data->current_shape->parent;

          if (text_parent)
            {
              TextNode node = {
                .type = TEXT_NODE_SHAPE,
                .shape = { .shape = shape },
              };
              g_array_append_val (text_parent->text, node);
            }
        }

      data->current_shape = shape;

      if (shape->id && !g_hash_table_contains (data->shapes, shape->id))
        g_hash_table_insert (data->shapes, shape->id, shape);

      return;
    }

  if (strcmp (element_name, "stop") == 0)
    {
      unsigned int idx;
      ColorStop *stop;

      if (data->current_shape == NULL ||
          (!check_ancestors (context, "linearGradient", NULL) &&
           !check_ancestors (context, "radialGradient", NULL)))
        {
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "<stop> only allowed in <linearGradient> or <radialGradient>");
          return;
        }

      idx = shape_add_color_stop (data->current_shape);
      stop = g_ptr_array_index (data->current_shape->color_stops, idx);
      stop->line = location.lines;

      parse_color_stop_attrs (data->current_shape, idx + 1, stop,
                              element_name, attr_names, attr_values,
                              &handled, data, context);

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      return;
    }

  if (filter_type_lookup (element_name, &filter_type))
    {
      unsigned int idx;
      FilterPrimitive *f;

      if (filter_type == FE_MERGE_NODE)
        {
          if (!check_ancestors (context, "feMerge", "filter", NULL))
            {
              skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "<%s> only allowed in <feMerge>", element_name);
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
              skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "<%s> only allowed in <feComponentTransfer>", element_name);
              return;
            }
        }
      else
        {
          if (!check_ancestors (context, "filter", NULL))
            {
              skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "<%s> only allowed in <filter>", element_name);
              return;
            }
        }

      idx = shape_add_filter (data->current_shape, filter_type);
      f = g_ptr_array_index (data->current_shape->filters, idx);
      f->line = location.lines;

      parse_filter_attrs (data->current_shape, idx + 1, f,
                          element_name, attr_names, attr_values,
                          &handled, data, context);

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (filter_type == FE_IMAGE)
        g_hash_table_add (data->pending_refs, data->current_shape);

      if (filter_type == FE_COLOR_MATRIX)
        {
          unsigned int pos = filter_attr_idx (f->type, SHAPE_ATTR_FE_COLOR_MATRIX_VALUES);
          SvgValue *values = f->base[pos];
          SvgValue *initial = filter_attr_ref_initial_value (f, SHAPE_ATTR_FE_COLOR_MATRIX_VALUES);

          if (svg_numbers_get_length (values) != svg_numbers_get_length (initial))
            {
              shape_set_base_value (data->current_shape, SHAPE_ATTR_FE_COLOR_MATRIX_VALUES, idx + 1, initial);
              if ((f->attrs & BIT (pos)) == 0)
                {
                  /* If this wasn't user-provided, we quietly correct the initial
                   * value to match the type
                   */
                  gtk_svg_invalid_attribute (data->svg, context, attr_names, "values", NULL);
                }
            }
          svg_value_unref (initial);
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
        skip_element (data, context, GTK_SVG_ERROR_IGNORED_ELEMENT, "Ignoring RDF elements outside <metadata>: <%s>", element_name);

      if (strcmp (element_name, "rdf:li") == 0)
        {
          /* Verify we're in the right place */
          if (check_ancestors (context, "rdf:Bag", "dc:subject", "cc:Work", "rdf:RDF", "metadata", NULL))
            start_collect_text (data, context);
          else
            skip_element (data, context, GTK_SVG_ERROR_IGNORED_ELEMENT, "Ignoring RDF element in wrong context: <%s>", element_name);
        }
      else if (strcmp (element_name, "dc:description") == 0)
        {
          if (check_ancestors (context, "cc:Work", "rdf:RDF", "metadata", NULL))
            start_collect_text (data, context);
          else
            skip_element (data, context, GTK_SVG_ERROR_IGNORED_ELEMENT, "Ignoring RDF element in wrong context: <%s>", element_name);
        }
      else if (strcmp (element_name, "dc:title") == 0)
        {
          if (check_ancestors (context, "cc:Agent", "dc:creator", "cc:Work", "rdf:RDF", "metadata", NULL))
            start_collect_text (data, context);
          else
            skip_element (data, context, GTK_SVG_ERROR_IGNORED_ELEMENT, "Ignoring RDF element in wrong context: <%s>", element_name);
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
                  if (!add_font_from_url (data->svg, context, attr_names, attr_names[i], attr_values[i]))
                    {
                      // too bad
                    }
                }
            }
        }
      else
        skip_element (data, context, GTK_SVG_ERROR_IGNORED_ELEMENT, "Ignoring font element in the wrong context: <%s>", element_name);

      return;
    }

  if (strcmp (element_name, "style") == 0)
    {
      gboolean is_css = TRUE;

      for (unsigned int i = 0; attr_names[i]; i++)
        {
          if (strcmp (attr_names[i], "type") == 0)
            {
              if (strcmp (attr_values[i], "text/css") != 0)
                is_css = FALSE;
              break;
            }
        }

      g_string_set_size (data->text.text, 0);
      if (is_css)
        start_collect_text (data, context);

      return;
    }

  if (strcmp (element_name, "textPath") == 0 ||
      strcmp (element_name, "feConvolveMatrix") == 0 ||
      strcmp (element_name, "feDiffuseLighting") == 0 ||
      strcmp (element_name, "feMorphology") == 0 ||
      strcmp (element_name, "feSpecularLighting") == 0 ||
      strcmp (element_name, "feTurbulence") == 0)
    {
      skip_element (data, context, GTK_SVG_ERROR_NOT_IMPLEMENTED, "<%s> is not supported", element_name);
      return;
    }

  if (strcmp (element_name, "title") == 0 ||
      strcmp (element_name, "desc") == 0 ||
      g_str_has_prefix (element_name, "sodipodi:") ||
      g_str_has_prefix (element_name, "inkscape:"))
    {
      skip_element (data, context, GTK_SVG_ERROR_IGNORED_ELEMENT, "Ignoring metadata and non-standard elements: <%s>", element_name);
      return;
    }

  if (strcmp (element_name, "set") == 0)
    {
      Animation *a;
      const char *to_attr = NULL;
      SvgValue *value;
      GError *error = NULL;

      if ((data->svg->features & GTK_SVG_ANIMATIONS) == 0)
        {
          skip_element (data, context, GTK_SVG_ERROR_FEATURE_DISABLED, "Animations are disabled");
          return;
        }

      if (data->current_animation)
        {
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Nested animation elements are not allowed: <set>");
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
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Skipping <%s> - bad attributes", element_name);
          return;
        }

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (!to_attr)
        {
          gtk_svg_missing_attribute (data->svg, context, "to", NULL);
          animation_drop_and_free (a);
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Dropping <set> without 'to'");
          return;
        }

      a->calc_mode = CALC_MODE_DISCRETE;
      a->frames = g_new0 (Frame, 2);
      a->frames[0].time = 0;
      a->frames[1].time = 1;

      value = shape_attr_parse_and_validate (a->attr, to_attr, &error);
      if (error)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "to", "%s", error->message);
          g_error_free (error);
        }
      else if (!value)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "to", NULL);
        }

      if (!value)
        {
          animation_drop_and_free (a);
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Dropping <set> without 'to'");
          return;
        }

      a->frames[0].value = svg_value_ref (value);
      a->frames[1].value = svg_value_ref (value);
      a->n_frames = 2;

      svg_value_unref (value);

      if (!a->href ||
          (data->current_shape != NULL &&
           g_strcmp0 (a->href, data->current_shape->id) == 0))
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
          skip_element (data, context, GTK_SVG_ERROR_IGNORED_ELEMENT, "Animations are disabled");
          return;
        }

      if (data->current_animation)
        {
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Nested animation elements are not allowed: <%s>", element_name);
          return;
        }

      if (strcmp (element_name, "animate") == 0)
        a = animation_animate_new ();
      else if (strcmp (element_name, "animateTransform") == 0)
        a = animation_transform_new ();
      else
        a = animation_motion_new ();

      a->line = location.lines;

      if (!parse_base_animation_attrs (a,
                                       element_name,
                                       attr_names, attr_values,
                                       &handled,
                                       data,
                                       context))
        {
          animation_drop_and_free (a);
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Skipping <%s> - bad attributes", element_name);
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
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Skipping <%s> - bad attributes", element_name);
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
              skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Skipping <%s>: bad attributes", element_name);
              return;
            }
        }

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (data->current_shape != NULL &&
          (a->href == NULL ||
           g_strcmp0 (a->href, data->current_shape->id) == 0))        shape_add_animation (data->current_shape, a);
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
      const char *href_attr_name = "href";

      if (data->current_animation == NULL ||
          data->current_animation->type != ANIMATION_TYPE_MOTION ||
          data->current_animation->motion.path_ref != NULL)
        {
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "<mpath> only allowed in <animateMotion>");
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
        {
          href_attr = xlink_href_attr;
          href_attr_name = "xlink:href";
        }

      if (href_attr != NULL)
        {
          if (href_attr[0] != '#')
            gtk_svg_invalid_attribute (data->svg, context, attr_names, href_attr_name,
                                       "Missing '#' in href");
          else
            data->current_animation->motion.path_ref = g_strdup (href_attr + 1);
        }

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (!data->current_animation->motion.path_ref)
        gtk_svg_missing_attribute (data->svg, context, "href", NULL);

      return;
    }

  /* If we get here, its all over */
  skip_element (data, context, GTK_SVG_ERROR_IGNORED_ELEMENT, "Unknown element: <%s>", element_name);
}

static void
end_element_cb (GMarkupParseContext *context,
                const char          *element_name,
                gpointer             user_data,
                GError             **gmarkup_error)
{
  ParserData *data = user_data;
  ShapeType shape_type;

  data->text.collect = FALSE;

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

          gtk_svg_location_init_tag_end (&end, context);

          gtk_svg_skipped_element (data->svg,
                                   parent,
                                   &data->skip.start,
                                   &end,
                                   data->skip.code,
                                   "%s", data->skip.reason);
          g_clear_pointer (&data->skip.reason, g_free);
          data->skip.to = NULL;
          if (!data->skip.skip_over_target)
            goto do_target;
        }
      return;
    }

do_target:
  if (strcmp (element_name, "style") == 0)
    {
      if (data->text.text->len > 0)
        {
          char *string;
          StyleElt *elt;

          string = g_strdup (data->text.text->str);
          elt = g_new0 (StyleElt, 1);
          elt->content = g_bytes_new_take (string, strlen (string));
          elt->location = data->text.start;
          g_ptr_array_add (data->current_shape->styles, elt);
        }
    }
  else if (strcmp (element_name, "rdf:li") == 0)
    {
      g_set_str (&data->svg->keywords, data->text.text->str);
    }
  else if (strcmp (element_name, "dc:description") == 0)
    {
      g_set_str (&data->svg->description, data->text.text->str);
    }
  else if (strcmp (element_name, "dc:title") == 0)
    {
      g_set_str (&data->svg->author, data->text.text->str);
    }
  else if (shape_type_lookup (element_name, &shape_type))
    {
      GSList *tos = data->shape_stack;

      g_assert (shape_type == data->current_shape->type);

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
  Shape *text_parent = NULL;

  if (!data->current_shape)
    return;

  if (shape_type_has_text (data->current_shape->type))
    text_parent = data->current_shape;
  else if (data->current_shape->parent &&
           data->current_shape->type == SHAPE_LINK &&
           shape_type_has_text (data->current_shape->parent->type))
    text_parent = data->current_shape->parent;

  if (text_parent)
    {
      TextNode node = {
        .type = TEXT_NODE_CHARACTERS,
        .characters = { .text = g_strndup (text, len) }
      };
      g_array_append_val (text_parent->text, node);
      return;
    }

  if (!data->text.collect)
    return;

  g_string_append_len (data->text.text, text, len);
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
  if (svg_clip_get_kind (value) == CLIP_URL &&
      svg_clip_get_shape (value) == NULL)
    {
      const char *ref = svg_clip_get_id (value);
      Shape *target = g_hash_table_lookup (data->shapes, ref);
      if (!target)
        gtk_svg_invalid_reference (data->svg,
                                   "No path with ID %s (resolving clip-path)",
                                   ref);
      else if (target->type != SHAPE_CLIP_PATH)
        gtk_svg_invalid_reference (data->svg,
                                   "Shape with ID %s not a <clipPath> (resolving clip-path)",
                                   ref);
      else
        {
          svg_clip_set_shape (value, target);
          add_dependency_to_common_ancestor (shape, target);
        }
    }
}

static void
resolve_mask_ref (SvgValue   *value,
                  Shape      *shape,
                  ParserData *data)
{
  if (svg_mask_get_kind (value) == MASK_URL && svg_mask_get_shape (value) == NULL)
    {
      const char *id = svg_mask_get_id (value);
      Shape *target = g_hash_table_lookup (data->shapes, id);
      if (!target)
        gtk_svg_invalid_reference (data->svg, "No shape with ID %s (resolving mask)", id);
      else if (target->type != SHAPE_MASK)
        gtk_svg_invalid_reference (data->svg, "Shape with ID %s not a <mask> (resolving mask)", id);
      else
        {
          svg_mask_set_shape (value, target);
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
          if (g_error_matches (error, GTK_SVG_ERROR, GTK_SVG_ERROR_FEATURE_DISABLED))
            gtk_svg_emit_error (data->svg, error);
          else
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
  SvgValue *paint = value;

  if (paint_is_server (svg_paint_get_kind (paint)) &&
      svg_paint_get_server_shape (paint) == NULL)
    {
      const char *ref = svg_paint_get_server_ref (paint);
      Shape *target = g_hash_table_lookup (data->shapes, ref);

      if (!target)
        {
          GtkSymbolicColor symbolic;

          if ((data->svg->features & GTK_SVG_EXTENSIONS) != 0 &&
              g_str_has_prefix (ref, "gpa:") &&
              parse_symbolic_color (ref + strlen ("gpa:"), &symbolic))
            return; /* Handled later */

          gtk_svg_invalid_reference (data->svg, "No shape with ID %s (resolving fill or stroke)", ref);
        }
      else if (target->type != SHAPE_LINEAR_GRADIENT &&
               target->type != SHAPE_RADIAL_GRADIENT &&
               target->type != SHAPE_PATTERN)
        {
          gtk_svg_invalid_reference (data->svg, "Shape with ID %s not a paint server (resolving fill or stroke)", ref);
        }
      else
        {
          svg_paint_set_server_shape (paint, target);
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
  for (unsigned int i = 0; i < svg_filter_get_length (value); i++)
    {
      if (svg_filter_get_kind (value, i) == FILTER_REF &&
          svg_filter_get_shape (value, i) == NULL)
        {
          const char *ref = svg_filter_get_ref (value, i);
          Shape *target = g_hash_table_lookup (data->shapes, ref);
          if (!target)
            {
              gtk_svg_invalid_reference (data->svg,
                                         "No shape with ID %s (resolving filter)",
                                         ref);
            }
          else if (target->type != SHAPE_FILTER)
            {
              gtk_svg_invalid_reference (data->svg,
                                         "Shape with ID %s not a filter (resolving filter)",
                                         ref);
            }
          else
            {
              svg_filter_set_shape (value, i, target);
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
      Shape *shape = g_hash_table_lookup (data->shapes, a->motion.path_ref);
      if (shape == NULL)
        gtk_svg_invalid_reference (data->svg,
                                   "No path with ID %s (resolving <mpath>",
                                   a->motion.path_ref);
      else if ((BIT (shape->type) & SHAPE_SHAPES) == 0)
        gtk_svg_invalid_reference (data->svg,
                                   "Element with ID %s is not a shape (resolving <mpath>",
                                   a->motion.path_ref);
      else
        {
          a->motion.path_shape = shape;
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
/* {{{ CSS handling */

/* Most of this is adapted from gtkcssprovider.c */

#define GDK_ARRAY_NAME svg_css_selectors
#define GDK_ARRAY_TYPE_NAME SvgCssSelectors
#define GDK_ARRAY_ELEMENT_TYPE GtkCssSelector *
#define GDK_ARRAY_PREALLOC 64
#include "gdk/gdkarrayimpl.c"

typedef struct _PropertyValue PropertyValue;
struct _PropertyValue {
  ShapeAttr attr;
  SvgValue *value;
  gboolean deprecated;
};

typedef struct _SvgCssRuleset SvgCssRuleset;
struct _SvgCssRuleset
{
  GtkCssSelector *selector;
  PropertyValue *styles;
  guint n_styles;
  guint owns_styles;
};

static void
svg_css_ruleset_init_copy (SvgCssRuleset  *new,
                           SvgCssRuleset  *ruleset,
                           GtkCssSelector *selector)
{
  memcpy (new, ruleset, sizeof (SvgCssRuleset));

  new->selector = selector;
  if (ruleset->owns_styles)
    ruleset->owns_styles = FALSE;
}

static void
svg_css_ruleset_clear (SvgCssRuleset *ruleset)
{
  if (ruleset->owns_styles)
    {
      guint i;

      for (i = 0; i < ruleset->n_styles; i++)
        {
          svg_value_unref (ruleset->styles[i].value);
          ruleset->styles[i].value = NULL;
        }
      g_free (ruleset->styles);
    }

  if (ruleset->selector)
    _gtk_css_selector_free (ruleset->selector);

  memset (ruleset, 0, sizeof (SvgCssRuleset));
}

static void
svg_css_ruleset_add (SvgCssRuleset *ruleset,
                     ShapeAttr      attr,
                     SvgValue      *value,
                     gboolean       deprecated)
{
  guint i;

  g_return_if_fail (ruleset->owns_styles || ruleset->n_styles == 0);

  ruleset->owns_styles = TRUE;

  for (i = 0; i < ruleset->n_styles; i++)
    {
      if (ruleset->styles[i].attr == attr)
        {
          svg_value_unref (ruleset->styles[i].value);
          ruleset->styles[i].value = NULL;
          break;
        }
    }

  if (i == ruleset->n_styles)
    {
      ruleset->n_styles++;
      ruleset->styles = g_realloc (ruleset->styles, ruleset->n_styles * sizeof (PropertyValue));
      ruleset->styles[i].attr = attr;
    }

  ruleset->styles[i].value = value;
  ruleset->styles[i].deprecated = deprecated;
}

typedef struct _SvgCssScanner SvgCssScanner;
struct _SvgCssScanner
{
  ParserData *data;
  GtkCssParser *parser;
  SvgCssScanner *parent;
};

static void
svg_css_scanner_parser_error (GtkCssParser         *parser,
                              const GtkCssLocation *css_start,
                              const GtkCssLocation *css_end,
                              const GError         *css_error,
                              gpointer              user_data)
{
  ParserData *data = user_data;
  if (css_error->domain == GTK_CSS_PARSER_ERROR)
    {
      GError *error;
      GtkSvgLocation start, end;

      error = g_error_new_literal (GTK_SVG_ERROR,
                                   GTK_SVG_ERROR_INVALID_SYNTAX,
                                   css_error->message);

      if (data->current_shape)
        {
          gtk_svg_error_set_element (error, shape_types[data->current_shape->type].name);
          gtk_svg_error_set_attribute (error, "style");
        }

      start = data->text.start;
      end = data->text.start;

      start.bytes += css_start->bytes;
      start.lines += css_start->lines;
      if (css_start->lines == 0)
        start.line_chars += css_start->line_chars;
      else
        start.line_chars = css_start->line_chars;

      end.bytes += css_end->bytes;
      end.lines += css_end->lines;
      if (css_end->lines == 0)
        end.line_chars += css_end->line_chars;
      else
        end.line_chars = css_end->line_chars;

      gtk_svg_error_set_location (error, &start, &end);

      gtk_svg_emit_error (data->svg, error);
      g_clear_error (&error);
    }
}


static SvgCssScanner *
svg_css_scanner_new (ParserData    *data,
                     SvgCssScanner *parent,
                     GFile         *file,
                     GBytes        *bytes)
{
  SvgCssScanner *scanner = g_new0 (SvgCssScanner, 1);

  scanner->data = data;
  scanner->parent = parent;
  scanner->parser = gtk_css_parser_new_for_bytes (bytes, file, svg_css_scanner_parser_error, data, NULL);

  return scanner;
}

static void
svg_css_scanner_destroy (SvgCssScanner *scanner)
{
  gtk_css_parser_unref (scanner->parser);
  g_free (scanner);
}

static int
svg_css_compare_rule (gconstpointer a_,
                      gconstpointer b_)
{
  const SvgCssRuleset *a = (const SvgCssRuleset *) a_;
  const SvgCssRuleset *b = (const SvgCssRuleset *) b_;
  int compare;

  compare = _gtk_css_selector_compare (a->selector, b->selector);
  if (compare != 0)
    return compare;

  return 0;
}

static void
postprocess_styles (ParserData *data)
{
  g_array_sort (data->rulesets, svg_css_compare_rule);
}

static void load_internal (ParserData    *data,
                           SvgCssScanner *scanner,
                           GFile         *file,
                           GBytes        *bytes);

static gboolean
svg_css_scanner_would_recurse (SvgCssScanner *scanner,
                               GFile         *file)
{
  while (scanner)
    {
      GFile *parser_file = gtk_css_parser_get_file (scanner->parser);
      if (parser_file && g_file_equal (parser_file, file))
        return TRUE;

      scanner = scanner->parent;
    }

  return FALSE;
}

static gboolean
parse_import (SvgCssScanner *scanner)
{
  GFile *file;

  if (!gtk_css_parser_try_at_keyword (scanner->parser, "import"))
    return FALSE;

  if (gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_STRING))
    {
      char *url;

      url = gtk_css_parser_consume_string (scanner->parser);
      if (url)
        {
          if (gtk_css_parser_get_file (scanner->parser))
            file = gtk_css_parser_resolve_url (scanner->parser, url);
          else
            file = g_file_new_for_path (url);
          if (file == NULL)
            {
              gtk_css_parser_error_import (scanner->parser,
                                           "Could not resolve \"%s\" to a valid URL",
                                           url);
            }
          g_free (url);
        }
      else
        file = NULL;
    }
  else
    {
      char *url = gtk_css_parser_consume_url (scanner->parser);
      if (url)
        {
          if (gtk_css_parser_get_file (scanner->parser))
            file = gtk_css_parser_resolve_url (scanner->parser, url);
          else
            file = g_file_new_for_uri (url);
          g_free (url);
        }
      else
        file = NULL;
    }

  if (file == NULL)
    {
      /* nothing to do; */
    }
  else if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_error_syntax (scanner->parser, "Expected ';'");
    }
  else if ((scanner->data->svg->features & GTK_SVG_EXTERNAL_RESOURCES) == 0)
    {
      /* Not allowed; TODO: emit an error */
    }
  else if (svg_css_scanner_would_recurse (scanner, file))
    {
       char *path = g_file_get_path (file);
       gtk_css_parser_error (scanner->parser,
                             GTK_CSS_PARSER_ERROR_IMPORT,
                             gtk_css_parser_get_block_location (scanner->parser),
                             gtk_css_parser_get_end_location (scanner->parser),
                             "Loading '%s' would recurse",
                             path);
       g_free (path);
    }
  else
    {
      GError *load_error = NULL;

      GBytes *bytes = g_file_load_bytes (file, NULL, NULL, &load_error);

      if (bytes == NULL)
        {
          gtk_css_parser_error (scanner->parser,
                                GTK_CSS_PARSER_ERROR_IMPORT,
                                gtk_css_parser_get_block_location (scanner->parser),
                                gtk_css_parser_get_end_location (scanner->parser),
                                "Failed to import: %s",
                                load_error->message);
          g_error_free (load_error);
        }
      else
        {
          load_internal (scanner->data, scanner, file, bytes);
          g_bytes_unref (bytes);
        }
    }

  g_clear_object (&file);

  return TRUE;
}


static void
parse_at_keyword (SvgCssScanner *scanner)
{
  gtk_css_parser_start_semicolon_block (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY);
  if (!parse_import (scanner))
    gtk_css_parser_error_syntax (scanner->parser, "Unknown @ rule");
  gtk_css_parser_end_block (scanner->parser);
}

static void
clear_selectors (SvgCssSelectors *selectors)
{
  for (int i = 0; i < svg_css_selectors_get_size (selectors); i++)
    _gtk_css_selector_free (svg_css_selectors_get (selectors, i));
}

static void
parse_selector_list (SvgCssScanner   *scanner,
                     SvgCssSelectors *selectors)
{
  do
    {
      GtkCssSelector *selector = _gtk_css_selector_parse (scanner->parser);

      if (selector == NULL)
        {
          clear_selectors (selectors);
          svg_css_selectors_clear (selectors);
          return;
        }

      svg_css_selectors_append (selectors, selector);
    }
  while (gtk_css_parser_try_token (scanner->parser, GTK_CSS_TOKEN_COMMA));
}

static void
parse_declaration (SvgCssScanner *scanner,
                   SvgCssRuleset *ruleset)
{
  char *name;
  ShapeAttr attr;
  gboolean deprecated;
  gboolean is_marker_shorthand;

  /* advance the location over whitespace */
  gtk_css_parser_get_token (scanner->parser);
  gtk_css_parser_start_semicolon_block (scanner->parser, GTK_CSS_TOKEN_EOF);

  if (gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_warn_syntax (scanner->parser, "Empty declaration");
      gtk_css_parser_end_block (scanner->parser);
      return;
    }

  name = gtk_css_parser_consume_ident (scanner->parser);
  if (name == NULL)
    goto out;

  if (shape_attr_lookup_for_css (name, &attr, &deprecated, &is_marker_shorthand))
    {
      SvgValue *value;

      if (!gtk_css_parser_try_token (scanner->parser, GTK_CSS_TOKEN_COLON))
        {
          gtk_css_parser_error_syntax (scanner->parser, "Expected ':'");
          goto out;
        }

      value = shape_attr_parse_css (attr, scanner->parser);

      if (value == NULL)
        goto out;

      if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
        {
          gtk_css_parser_error_syntax (scanner->parser, "Junk at end of value for %s", name);
          svg_value_unref (value);
          goto out;
        }

      svg_css_ruleset_add (ruleset, attr, value, deprecated);
      if (is_marker_shorthand)
        {
          svg_css_ruleset_add (ruleset, SHAPE_ATTR_MARKER_MID, svg_value_ref (value), deprecated);
          svg_css_ruleset_add (ruleset, SHAPE_ATTR_MARKER_END, svg_value_ref (value), deprecated);
        }
    }
  else
    {
      gtk_css_parser_error_value (scanner->parser, "No property named \"%s\"", name);
    }

out:
  g_free (name);

  gtk_css_parser_end_block (scanner->parser);
}

static void
parse_declarations (SvgCssScanner *scanner,
                    SvgCssRuleset *ruleset)
{
  while (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    {
      parse_declaration (scanner, ruleset);
    }
}

static void
parse_style_into_ruleset (SvgCssRuleset *ruleset,
                          const char    *style,
                          ParserData    *data)
{
  GBytes *bytes;
  SvgCssScanner *scanner;

  bytes = g_bytes_new_static (style, strlen (style));
  scanner = svg_css_scanner_new (data, NULL, NULL, bytes);
  parse_declarations (scanner, ruleset);
  svg_css_scanner_destroy (scanner);
  g_bytes_unref (bytes);
}

static void
commit_ruleset (ParserData      *data,
                SvgCssSelectors *selectors,
                SvgCssRuleset   *ruleset)
{
  if (ruleset->styles == NULL)
    {
      clear_selectors (selectors);
      return;
    }

  for (unsigned int i = 0; i < svg_css_selectors_get_size (selectors); i++)
    {
      SvgCssRuleset *new;

      g_array_set_size (data->rulesets, data->rulesets->len + 1);
      new = &g_array_index (data->rulesets, SvgCssRuleset, data->rulesets->len - 1);
      svg_css_ruleset_init_copy (new, ruleset, svg_css_selectors_get (selectors, i));
    }

}

static void
parse_ruleset (SvgCssScanner *scanner)
{
  SvgCssSelectors selectors;
  SvgCssRuleset ruleset = { 0, };

  svg_css_selectors_init (&selectors);

  parse_selector_list (scanner, &selectors);
  if (svg_css_selectors_get_size (&selectors) == 0)
    {
      gtk_css_parser_skip_until (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY);
      gtk_css_parser_skip (scanner->parser);
      goto out;
    }

  if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY))
    {
      gtk_css_parser_error_syntax (scanner->parser, "Expected '{' after selectors");
      clear_selectors (&selectors);
      gtk_css_parser_skip_until (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY);
      gtk_css_parser_skip (scanner->parser);
      goto out;
    }

  gtk_css_parser_start_block (scanner->parser);
  parse_declarations (scanner, &ruleset);
  gtk_css_parser_end_block (scanner->parser);
  commit_ruleset (scanner->data, &selectors, &ruleset);
  svg_css_ruleset_clear (&ruleset);

out:
  svg_css_selectors_clear (&selectors);
}

static void
parse_statement (SvgCssScanner *scanner)
{
  if (gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_AT_KEYWORD))
    parse_at_keyword (scanner);
  else
    parse_ruleset (scanner);
}

static void
parse_stylesheet (SvgCssScanner *scanner)
{
  while (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    {
      if (gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_CDO) ||
          gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_CDC))
        {
          gtk_css_parser_consume_token (scanner->parser);
          continue;
        }

      parse_statement (scanner);
    }
}

static void
load_internal (ParserData    *data,
               SvgCssScanner *parent,
               GFile         *file,
               GBytes        *bytes)
{
  SvgCssScanner *scanner;

  scanner = svg_css_scanner_new (data, parent, NULL, bytes);

  parse_stylesheet (scanner);

  if (parent == NULL)
    postprocess_styles (data);

  svg_css_scanner_destroy (scanner);
}

static void
load_styles_for_shape (Shape      *shape,
                       ParserData *data)
{
  for (unsigned int i = 0; i < shape->styles->len; i++)
    {
      StyleElt *elt = g_ptr_array_index (shape->styles, i);
      data->text.start = elt->location;
      load_internal (data, NULL, NULL, elt->content);
    }

  if (shape_type_has_shapes (shape->type))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          Shape *sh = g_ptr_array_index (shape->shapes, i);
          load_styles_for_shape (sh, data);
        }
    }
}

static void
load_styles (ParserData *data)
{
  data->current_shape = NULL;
  load_styles_for_shape (data->svg->content, data);
}

#if 0
static void
dump_styles (ParserData *data)
{
  GString *s = g_string_new ("");

  for (unsigned int i = 0; i < data->rulesets->len; i++)
    {
      SvgCssRuleset *r = &g_array_index (data->rulesets, SvgCssRuleset, i);

      _gtk_css_selector_print (r->selector, s);
      g_string_append (s, " {\n");
      for (unsigned int j = 0; j < r->n_styles; j++)
        {
          PropertyValue *p = &r->styles[j];
          g_string_append (s, "  ");
          g_string_append (s, shape_attr_get_name (p->attr));
          g_string_append (s, ": ");
          svg_value_print (p->value, s);
          g_string_append (s, ";");
          if (p->deprecated)
            g_string_append (s, " /* deprecated */");
          g_string_append (s, "\n");
        }
      g_string_append (s, "}\n");
    }

  g_print ("%s\n", s->str);
  g_string_free (s, TRUE);
}
#endif

static void
apply_ruleset_to_shape (SvgCssRuleset *r,
                        Shape         *shape,
                        unsigned int   idx,
                        GtkBitmask    *set,
                        ParserData    *data)
{
  for (unsigned int j = 0; j < r->n_styles; j++)
    {
      PropertyValue *p = &r->styles[j];

      if (set && _gtk_bitmask_get (set, p->attr))
        continue;

      if (_gtk_bitmask_get (shape->attrs, p->attr) &&
          p->deprecated)
        continue;

      if (shape_has_attr (shape->type, p->attr))
        {
          shape_set_base_value (shape, p->attr, idx, p->value);

          if (p->attr == SHAPE_ATTR_CLIP_PATH ||
              p->attr == SHAPE_ATTR_MASK ||
              p->attr == SHAPE_ATTR_HREF ||
              p->attr == SHAPE_ATTR_FILTER ||
              p->attr == SHAPE_ATTR_MARKER_START ||
              p->attr == SHAPE_ATTR_MARKER_MID ||
              p->attr == SHAPE_ATTR_MARKER_END ||
              p->attr == SHAPE_ATTR_FILL ||
              p->attr == SHAPE_ATTR_STROKE)
            {
              g_hash_table_add (data->pending_refs, shape);
            }
        }

      if (set)
        set = _gtk_bitmask_set (set, p->attr, TRUE);
    }
}

static void
apply_styles_here (Shape        *shape,
                   unsigned int  idx,
                   ParserData   *data)
{
  GtkCssNode *node;
  GtkCssNode *freeme = NULL;
  GtkBitmask *set;
  const char *style = NULL;
  GtkSvgLocation loc;

  if (idx > 0)
    {
      if (shape_type_has_color_stops (shape->type))
        {
          ColorStop *stop = g_ptr_array_index (shape->color_stops, idx - 1);
          node = stop->css_node;
          style = stop->style;
          loc.lines = stop->line;
          loc.line_chars = 0;
          loc.bytes = 0;
        }
      else
        {
          FilterPrimitive *ff = g_ptr_array_index (shape->filters, idx - 1);
          node = ff->css_node;
          style = ff->style;
          loc.lines = ff->line;
          loc.line_chars = 0;
          loc.bytes = 0;
        }
    }
  else if (shape->css_node)
    {
      node = shape->css_node;
      style = shape->style;
      loc = shape->style_loc;
    }
  else
    {
      freeme = node = gtk_css_node_new ();
      style = NULL;
      loc.lines = 0;
      loc.line_chars = 0;
      loc.bytes = 0;
    }

  set = _gtk_bitmask_new ();

  for (unsigned int i = 0; i < data->rulesets->len; i++)
    {
      SvgCssRuleset *r = &g_array_index (data->rulesets, SvgCssRuleset, i);
      if (gtk_css_selector_matches (r->selector, node))
        apply_ruleset_to_shape (r, shape, idx, set, data);
    }

  _gtk_bitmask_free (set);
  g_clear_object (&freeme);

  if (style)
    {
      SvgCssRuleset r = { 0, };

      data->text.start = loc;
      data->text.start.line_chars += strlen ("style='");
      data->text.start.bytes += strlen ("style='");
      data->current_shape = shape;
      parse_style_into_ruleset (&r, style, data);
      apply_ruleset_to_shape (&r, shape, idx, NULL, data);
      svg_css_ruleset_clear (&r);
    }
}

static void
apply_styles_to_shape (Shape      *shape,
                       ParserData *data)
{
  apply_styles_here (shape, 0, data);

  if (shape_type_has_shapes (shape->type))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          Shape *sh = g_ptr_array_index (shape->shapes, i);
          apply_styles_to_shape (sh, data);
        }
    }

  if (shape_type_has_color_stops (shape->type))
    {
      for (unsigned int idx = 0; idx < shape->color_stops->len; idx++)
        apply_styles_here (shape, idx + 1, data);
    }

  if (shape_type_has_filters (shape->type))
    {
      for (unsigned int idx = 0; idx < shape->filters->len; idx++)
        apply_styles_here (shape, idx + 1, data);
    }

  /* Apply traditional symbolic heuristics *after*
   * CSS and styles, so that these take precedence.
   */
  if (data->svg->gpa_version == 0 &&
      ((data->svg->features & GTK_SVG_TRADITIONAL_SYMBOLIC) != 0))
    {
      SvgValue *value;
      gboolean has_stroke;

      if (!shape->classes)
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_FOREGROUND);
      else if (g_strv_has (shape->classes, "transparent-fill"))
        value = svg_paint_new_none ();
      else if (g_strv_has (shape->classes, "foreground-fill"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_FOREGROUND);
      else if (g_strv_has (shape->classes, "success") ||
               g_strv_has (shape->classes, "success-fill"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_SUCCESS);
      else if (g_strv_has (shape->classes, "warning") ||
               g_strv_has (shape->classes, "warning-fill"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_WARNING);
      else if (g_strv_has (shape->classes, "error") ||
               g_strv_has (shape->classes, "error-fill"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_ERROR);
      else
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_FOREGROUND);

      shape_set_base_value (shape, SHAPE_ATTR_FILL, 0, value);
      svg_value_unref (value);

      if (!shape->classes)
        value = svg_paint_new_none ();
      else if (g_strv_has (shape->classes, "success-stroke"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_SUCCESS);
      else if (g_strv_has (shape->classes, "warning-stroke"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_WARNING);
      else if (g_strv_has (shape->classes, "error-stroke"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_ERROR);
      else if (g_strv_has (shape->classes, "foreground-stroke"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_FOREGROUND);
      else
        value = svg_paint_new_none ();

      has_stroke = !svg_value_equal (value, svg_paint_new_none ());
      shape_set_base_value (shape, SHAPE_ATTR_STROKE, 0, value);
      svg_value_unref (value);

      if (has_stroke)
        {
          value = svg_number_new (2);
          shape_set_base_value (shape, SHAPE_ATTR_STROKE_WIDTH, 0, value);
          svg_value_unref (value);

          value = svg_linejoin_new (GSK_LINE_JOIN_ROUND);
          shape_set_base_value (shape, SHAPE_ATTR_STROKE_LINEJOIN, 0, value);
          svg_value_unref (value);

          value = svg_linecap_new (GSK_LINE_CAP_ROUND);
          shape_set_base_value (shape, SHAPE_ATTR_STROKE_LINECAP, 0, value);
          svg_value_unref (value);
        }
    }

  /* gpa attrs are supported to have higher priority than
   * style and CSS, so re-set them here
   */
  if (data->svg->gpa_version > 0)
    {
      if (shape->gpa.fill)
        shape_set_base_value (shape, SHAPE_ATTR_FILL, 0, shape->gpa.fill);
      if (shape->gpa.stroke)
        shape_set_base_value (shape, SHAPE_ATTR_STROKE, 0, shape->gpa.stroke);
      if (shape->gpa.width)
        shape_set_base_value (shape, SHAPE_ATTR_STROKE_WIDTH, 0, shape->gpa.width);
    }

  if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_COLOR))
    {
      SvgValue *color = shape->base[SHAPE_ATTR_COLOR];

      if ((data->svg->features & GTK_SVG_EXTENSIONS) == 0 &&
          svg_color_get_kind (color) == COLOR_SYMBOLIC)
        {
          SvgValue *value = svg_color_new_black ();
          shape_set_base_value (shape, SHAPE_ATTR_COLOR, 0, value);
          svg_value_unref (value);
          color = shape->base[SHAPE_ATTR_COLOR];
        }

      if (svg_color_get_kind (color) == COLOR_SYMBOLIC)
        data->svg->used |= BIT (svg_color_get_symbolic (color) + 1);
    }

  if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_FILL))
    {
      SvgValue *paint = shape->base[SHAPE_ATTR_FILL];
      GtkSymbolicColor symbolic;

      if ((data->svg->features & GTK_SVG_EXTENSIONS) == 0 &&
          svg_paint_get_kind (paint) == PAINT_SYMBOLIC)
        {
          SvgValue *value = svg_paint_new_black ();
          shape_set_base_value (shape, SHAPE_ATTR_FILL, 0, value);
          svg_value_unref (value);
          paint = shape->base[SHAPE_ATTR_FILL];
        }

      if (paint_is_server (svg_paint_get_kind (paint)))
        g_hash_table_add (data->pending_refs, shape);

      if (svg_paint_is_symbolic (paint, &symbolic))
        data->svg->used |= BIT (symbolic + 1);
    }

  if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_STROKE))
    {
      SvgValue *paint = shape->base[SHAPE_ATTR_STROKE];
      GtkSymbolicColor symbolic;

      if ((data->svg->features & GTK_SVG_EXTENSIONS) == 0 &&
          svg_paint_get_kind (paint) == PAINT_SYMBOLIC)
        {
          SvgValue *value = svg_paint_new_black ();
          shape_set_base_value (shape, SHAPE_ATTR_STROKE, 0, value);
          svg_value_unref (value);
          paint = shape->base[SHAPE_ATTR_STROKE];
        }

      if (paint_is_server (svg_paint_get_kind (paint)))
        g_hash_table_add (data->pending_refs, shape);

      if (svg_paint_is_symbolic (paint, &symbolic))
        data->svg->used |= BIT (symbolic + 1);

      if (svg_paint_get_kind (paint) != PAINT_NONE)
        data->svg->used |= GTK_SVG_USES_STROKES;
    }
}

static void
apply_styles (ParserData *data)
{
  apply_styles_to_shape (data->svg->content, data);
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
  data.pending_refs = g_hash_table_new (g_direct_hash, g_direct_equal);
  data.skip.to = NULL;
  data.skip.reason = NULL;
  data.text.text = g_string_new ("");
  data.text.collect = FALSE;
  data.num_loaded_elements = 0;
  data.rulesets = g_array_new (FALSE, FALSE, sizeof (SvgCssRuleset));

  context = g_markup_parse_context_new (&parser,
                                        G_MARKUP_PREFIX_ERROR_POSITION |
                                        G_MARKUP_TREAT_CDATA_AS_TEXT,
                                        &data, NULL);
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
      g_hash_table_remove_all (data.pending_refs);
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

  load_styles (&data);
#if 0
  dump_styles (&data);
#endif
  apply_styles (&data);

  if (_gtk_bitmask_get (self->content->attrs, SHAPE_ATTR_VIEW_BOX))
    {
      graphene_rect_t vb;

      svg_view_box_get (self->content->base[SHAPE_ATTR_VIEW_BOX], &vb);
      self->width = vb.size.width;
      self->height = vb.size.height;
    }

  if (_gtk_bitmask_get (self->content->attrs, SHAPE_ATTR_WIDTH))
    {
      SvgUnit unit = svg_number_get_unit (self->content->base[SHAPE_ATTR_WIDTH]);
      double value = svg_number_get (self->content->base[SHAPE_ATTR_WIDTH], 100);

      if (is_absolute_length (unit))
        self->width = absolute_length_to_px (value, unit);
      else if (unit != SVG_UNIT_PERCENTAGE)
        self->width = value;
    }

  if (_gtk_bitmask_get (self->content->attrs, SHAPE_ATTR_HEIGHT))
    {
      SvgUnit unit = svg_number_get_unit (self->content->base[SHAPE_ATTR_HEIGHT]);
      double value = svg_number_get (self->content->base[SHAPE_ATTR_HEIGHT], 100);

      if (is_absolute_length (unit))
        self->height = absolute_length_to_px (value, unit);
      else if (unit != SVG_UNIT_PERCENTAGE)
        self->height = value;
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

  if (g_hash_table_size (data.pending_refs) > 0)
    {
      GHashTableIter iter;
      Shape *shape;

      g_hash_table_iter_init (&iter, data.pending_refs);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &shape))
        resolve_shape_refs (shape, &data);
    }

  resolve_animation_refs (self->content, &data);

  compute_update_order (self->content, self);

  self->state_change_delay = timeline_get_state_change_delay (self->timeline);

  g_hash_table_unref (data.shapes);
  g_hash_table_unref (data.animations);
  g_ptr_array_unref (data.pending_animations);
  g_hash_table_unref (data.pending_refs);
  g_string_free (data.text.text, TRUE);
  for (unsigned int i = 0; i < data.rulesets->len; i++)
    svg_css_ruleset_clear (&g_array_index (data.rulesets, SvgCssRuleset, i));
  g_array_unref (data.rulesets);

  if (self->gpa_version > 0 &&
      (self->features & GTK_SVG_ANIMATIONS) == 0)
    apply_state (self, self->state);
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
  if (s->str[s->len - 1] != '\n')
    g_string_append_c (s, '\n');
  g_string_append_printf (s, "%*s", indent, " ");
}

static void
indent_for_attr (GString *s,
                 int      indent)
{
  if (s->str[s->len - 1] != '\n')
    g_string_append_c (s, '\n');
  g_string_append_printf (s, "%*s", indent + 8, " ");
}

static void
g_markup_append (GString    *s,
                 const char *text)
{
  char *escaped = g_markup_escape_text (text, strlen (text));
  g_string_append (s, escaped);
  g_free (escaped);
}

static void
serialize_shape_attrs (GString              *s,
                       GtkSvg               *svg,
                       int                   indent,
                       Shape                *shape,
                       GtkSvgSerializeFlags  flags)
{
  if (shape->id)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "id='");
      g_markup_append (s, shape->id);
      g_string_append_c (s, '\'');
    }

  if (shape->classes && shape->classes[0])
    {
      indent_for_attr (s, indent);
      g_string_append (s, "class='");
      for (unsigned int i = 0; shape->classes[i]; i++)
        {
          if (i > 0)
            g_string_append_c (s, ' ');
          g_markup_append (s, shape->classes[i]);
        }
      g_string_append_c (s, '\'');
    }

  if (shape->style)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "style='");
      g_markup_append (s, shape->style);
      g_string_append_c (s, '\'');
    }

  for (ShapeAttr attr = FIRST_SHAPE_ATTR; attr <= LAST_SHAPE_ATTR; attr++)
    {
      if ((flags & GTK_SVG_SERIALIZE_NO_COMPAT) == 0 &&
          svg->gpa_version > 0 &&
          shape_type_has_gpa_attrs (shape->type) &&
          attr == SHAPE_ATTR_VISIBILITY)
        {
          unsigned int state;

          if (svg->gpa_version == 0 &&
              (attr == SHAPE_ATTR_STROKE_MINWIDTH ||
               attr == SHAPE_ATTR_STROKE_MAXWIDTH))
            continue;

          if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
            state = svg->state;
          else
            state = svg->initial_state;

          if ((shape->gpa.states & BIT (state)) == 0)
            {
              indent_for_attr (s, indent);
              g_string_append (s, "visibility='hidden'");
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
            }

          svg_value_unref (initial);
          svg_value_unref (value);
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
  if (svg->gpa_version == 0 || !shape_type_has_gpa_attrs (shape->type))
    return;

  if (shape->gpa.stroke)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "gpa:stroke='");
      svg_paint_print_gpa (shape->gpa.stroke, s);
      g_string_append_c (s, '\'');
    }

  if (shape->gpa.fill)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "gpa:fill='");
      svg_paint_print_gpa (shape->gpa.fill, s);
      g_string_append_c (s, '\'');
    }

  if (shape->gpa.states != ALL_STATES)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "gpa:states='");
      print_states (s, svg, shape->gpa.states);
      g_string_append_c (s, '\'');
    }

  if ((flags & GTK_SVG_SERIALIZE_EXPAND_GPA_ATTRS) == 0)
    {
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
      const char *name;

      indent_for_attr (s, indent);
      if (a->shape->type == SHAPE_FILTER && a->idx > 0)
        {
          FilterPrimitive *f = g_ptr_array_index (a->shape->filters, a->idx - 1);
          name = filter_attr_get_presentation (a->attr, f->type);
        }
      else
        name = shape_attr_get_presentation (a->attr, a->shape->type);
      g_string_append_printf (s, "attributeName='%s'", name);
    }

  if (a->has_begin)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "begin='");
      time_specs_print (a->begin, svg, s);
      g_string_append (s, "'");
    }

  if (a->has_end)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "end='");
      time_specs_print (a->end, svg, s);
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

  indent_for_elt (s, indent);
  g_string_append (s, "<animateTransform");
  serialize_base_animation_attrs (s, svg, indent, a);
  serialize_value_animation_attrs (s, svg, indent, a);
  indent_for_attr (s, indent);
  g_string_append_printf (s, "type='%s'", types[svg_transform_get_type (a->frames[0].value, 0)]);
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
      g_string_append_printf (s, "<mpath href='#%s'/>", a->motion.path_shape->id);
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

  if (stop->id)
    {
      indent_for_attr (s, indent);
      g_string_append_printf (s, "id='%s'", stop->id);
    }

  if (stop->classes)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "class='");
      for (unsigned int i = 0; stop->classes[i]; i++)
        {
          if (i > 0)
            g_string_append_c (s, ' ');
          g_string_append (s, stop->classes[i]);
        }
      g_string_append_c (s, '\'');
    }

  if (stop->style)
    {
      indent_for_attr (s, indent);
      g_string_append_printf (s, "style='%s'", stop->style);
    }

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

  if (f->id)
    {
      indent_for_attr (s, indent);
      g_string_append_printf (s, "id='%s'", f->id);
    }

  if (f->classes)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "class='");
      for (unsigned int i = 0; f->classes[i]; i++)
        {
          if (i > 0)
            g_string_append_c (s, ' ');
          g_string_append (s, f->classes[i]);
        }
      g_string_append_c (s, '\'');
    }

  if (f->style)
    {
      indent_for_attr (s, indent);
      g_string_append_printf (s, "style='%s'", f->style);
    }

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

  for (unsigned int i = 0; i < shape->styles->len; i++)
    {
      StyleElt *elt = g_ptr_array_index (shape->styles, i);
      indent_for_elt (s, indent + 2);
      g_string_append (s, "<style type='text/css'>");
      g_string_append (s, g_bytes_get_data (elt->content, NULL));
      g_string_append (s, "</style>");
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
      for (unsigned int i = 0; i < shape->text->len; i++)
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
                          SvgValue     *paint,
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
  GskRenderNode *node;
  FilterResult *res;

  switch (svg_filter_primitive_ref_get_type (in))
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
      res = g_hash_table_lookup (results, svg_filter_primitive_ref_get_ref (in));
      if (res)
        return filter_result_ref (res);
      else
        return filter_result_ref (g_hash_table_lookup (results, ""));
    case FILL_PAINT:
    case STROKE_PAINT:
      {
        GskPath *path;
        GskPathBuilder *builder;
        SvgValue *paint;
        double opacity;

        if (svg_filter_primitive_ref_get_type (in))
          {
            paint = shape->current[SHAPE_ATTR_FILL];
            opacity = svg_number_get (shape->current[SHAPE_ATTR_FILL_OPACITY], 1);
          }
        else
          {
            paint = shape->current[SHAPE_ATTR_STROKE];
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
determine_filter_subregion_from_refs (SvgValue              **refs,
                                      unsigned int            n_refs,
                                      gboolean                is_first,
                                      const graphene_rect_t  *filter_region,
                                      GHashTable             *results,
                                      graphene_rect_t        *subregion)
{
  for (unsigned int i = 0; i < n_refs; i++)
    {
      SvgFilterPrimitiveRefType type = svg_filter_primitive_ref_get_type (refs[i]);

      if (type == SOURCE_GRAPHIC || type == SOURCE_ALPHA ||
          type == BACKGROUND_IMAGE || type == BACKGROUND_ALPHA ||
          type == FILL_PAINT || type == STROKE_PAINT ||
          (type == DEFAULT_SOURCE && is_first))
        {
          if (i == 0)
            graphene_rect_init_from_rect (subregion, filter_region);
          else
            graphene_rect_union (filter_region, subregion, subregion);
        }
      else
        {
          FilterResult *res;

          if (type == DEFAULT_SOURCE)
            res = g_hash_table_lookup (results, "");
          else if (type == PRIMITIVE_REF)
            res = g_hash_table_lookup (results, svg_filter_primitive_ref_get_ref (refs[i]));
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
  if (f->type == FE_MERGE_NODE ||
      f->type == FE_FUNC_R ||
      f->type == FE_FUNC_G ||
      f->type == FE_FUNC_B ||
      f->type == FE_FUNC_A)
    {
      g_error ("Can't get subregion for %s\n", filter_types[f->type].name);
      return FALSE;
    }

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
            SvgValue *ref = filter_get_current_value (f, SHAPE_ATTR_FE_IN);

            determine_filter_subregion_from_refs (&ref, 1, is_first, filter_region, results, subregion);
          }
          break;

        case FE_BLEND:
        case FE_COMPOSITE:
        case FE_DISPLACEMENT:
          {
            SvgValue *refs[2];

            refs[0] = filter_get_current_value (f, SHAPE_ATTR_FE_IN);
            refs[1] = filter_get_current_value (f, SHAPE_ATTR_FE_IN2);

            determine_filter_subregion_from_refs (refs, 2, is_first, filter_region, results, subregion);
          }
          break;

        case FE_MERGE:
          {
            SvgValue **refs;
            unsigned int n_refs;

            refs = g_newa (SvgValue *, filter->filters->len);
            n_refs = 0;
            for (idx++; idx < filter->filters->len; idx++)
              {
                FilterPrimitive *ff = g_ptr_array_index (filter->filters, idx);

                if (ff->type != FE_MERGE_NODE)
                  break;

                refs[n_refs] = filter_get_current_value (ff, SHAPE_ATTR_FE_IN);
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

          /* Skip dependent filters */
          if (f->type == FE_MERGE)
            {
              for (i++; i < filter->filters->len; i++)
                {
                  FilterPrimitive *ff = g_ptr_array_index (filter->filters, i);
                  if (ff->type != FE_MERGE_NODE)
                    {
                      i--;
                      break;
                    }
                }
            }
          else if (f->type == FE_COMPONENT_TRANSFER)
            {
              for (i++; i < filter->filters->len; i++)
                {
                  FilterPrimitive *ff = g_ptr_array_index (filter->filters, i);
                  if (ff->type != FE_FUNC_R &&
                      ff->type != FE_FUNC_G &&
                      ff->type != FE_FUNC_B &&
                      ff->type != FE_FUNC_A)
                    {
                      i--;
                      break;
                    }
                }
            }

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
            SvgValue *num;

            in = get_input_for_ref (filter_get_current_value (f, SHAPE_ATTR_FE_IN), &subregion, shape, context, source, results);

            num = filter_get_current_value (f, SHAPE_ATTR_FE_STD_DEV);
            if (svg_numbers_get_length (num) == 2 &&
                svg_numbers_get (num, 0, 1) != svg_numbers_get (num, 1, 1))
              gtk_svg_rendering_error (context->svg,
                                       "Separate x/y values for stdDeviation not supported");
            std_dev = svg_numbers_get (num, 0, 1);

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
            SvgValue *color = filter_get_current_value (f, SHAPE_ATTR_FE_COLOR);
            SvgValue *alpha = filter_get_current_value (f, SHAPE_ATTR_FE_OPACITY);
            GdkColor c;

            gdk_color_init_copy (&c, svg_color_get_color (color));
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
            SvgValue *color;
            SvgValue *alpha;
            GskShadowEntry shadow;
            SvgValue *num;

            in = get_input_for_ref (filter_get_current_value (f, SHAPE_ATTR_FE_IN), &subregion, shape, context, source, results);
            num = filter_get_current_value (f, SHAPE_ATTR_FE_STD_DEV);
            if (svg_numbers_get_length (num) == 2 &&
                svg_numbers_get (num, 0, 1) != svg_numbers_get (num, 1, 1))
              gtk_svg_rendering_error (context->svg,
                                       "Separate x/y values for stdDeviation not supported");
            std_dev = svg_numbers_get (num, 0, 1);

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

            color = filter_get_current_value (f, SHAPE_ATTR_FE_COLOR);
            alpha = filter_get_current_value (f, SHAPE_ATTR_FE_OPACITY);

            gdk_color_init_copy (&shadow.color, svg_color_get_color (color));
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
  GskRenderNode *result = gsk_render_node_ref (source);

  for (unsigned int i = 0; i < svg_filter_get_length (filter); i++)
    {
      GskRenderNode *child = result;

      switch (svg_filter_get_kind (filter, i))
        {
        case FILTER_NONE:
          result = gsk_render_node_ref (child);
          break;
        case FILTER_BLUR:
          result = gsk_blur_node_new (child, 2 * svg_filter_get_simple (filter, i));
          break;
        case FILTER_OPACITY:
          result = gsk_opacity_node_new (child, svg_filter_get_simple (filter, i));
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

            svg_filter_get_color_matrix (filter, i, &matrix, &offset);
            result = gsk_color_matrix_node_new (child, &matrix, &offset);
          }
          break;
        case FILTER_DROPSHADOW:
          {
            GskShadowEntry shadow;
            double x, y, std_dev;

            svg_filter_get_dropshadow (filter, i, &shadow.color, &x, &y, &std_dev);
            shadow.offset.x = x;
            shadow.offset.y = y;
            shadow.radius = 2 * std_dev;

            result = gsk_shadow_node_new2 (child, &shadow, 1);

            gdk_color_finish (&shadow.color);
          }
          break;
        case FILTER_REF:
          if (svg_filter_get_shape (filter, i) == NULL)
            {
              gsk_render_node_unref (child);
              return gsk_render_node_ref (source);
            }

          result = apply_filter_tree (shape, svg_filter_get_shape (filter, i), context, child);
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
      SvgValue *tf = shape->current[SHAPE_ATTR_TRANSFORM];
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
  SvgValue *clip = shape->current[SHAPE_ATTR_CLIP_PATH];
  SvgValue *mask = shape->current[SHAPE_ATTR_MASK];
  SvgValue *tf = shape->current[SHAPE_ATTR_TRANSFORM];
  SvgValue *blend = shape->current[SHAPE_ATTR_BLEND_MODE];
  SvgValue *fill_rule = shape->current[SHAPE_ATTR_FILL_RULE];

#ifdef DEBUG
  if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
    {
      if (shape->id)
        gtk_snapshot_push_debug (context->snapshot, "Group for <%s id='%s'> at line %" G_GSIZE_FORMAT, shape_types[shape->type].name, shape->id, shape->line);
      else
        gtk_snapshot_push_debug (context->snapshot, "Group for <%s> at line %" G_GSIZE_FORMAT, shape_types[shape->type].name, shape->line);
    }
#endif

  if (shape->type == SHAPE_SVG || shape->type == SHAPE_SYMBOL)
    {
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

      if (!svg_view_box_get (shape->current[SHAPE_ATTR_VIEW_BOX], &view_box))
        graphene_rect_init (&view_box, 0, 0, w, h);

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

  if (shape->type != SHAPE_CLIP_PATH && !svg_transform_is_none (tf))
    {
      GskTransform *transform = svg_transform_get_gsk (tf);

      if (_gtk_bitmask_get (shape->attrs, SHAPE_ATTR_TRANSFORM_ORIGIN))
        {
          SvgValue *tfo = shape->current[SHAPE_ATTR_TRANSFORM_ORIGIN];
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

          x = svg_numbers_get (tfo, 0, bounds.size.width);
          y = svg_numbers_get (tfo, 1, bounds.size.height);

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

  if (svg_clip_get_kind (clip) == CLIP_PATH ||
      (svg_clip_get_kind (clip) == CLIP_URL && svg_clip_get_shape (clip) != NULL))
    {
      push_op (context, CLIPPING);

      /* Clip mask - see language in the spec about 'raw geometry' */
      if (svg_clip_get_kind (clip) == CLIP_PATH)
        {
          GskFillRule rule;

          switch (svg_clip_get_fill_rule (clip))
            {
            case GSK_FILL_RULE_WINDING:
            case GSK_FILL_RULE_EVEN_ODD:
              rule = svg_clip_get_fill_rule (clip);
              break;
            default:
              rule = svg_enum_get (fill_rule);
              break;
            }

#ifdef DEBUG
          if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
            gtk_snapshot_push_debug (context->snapshot, "fill or clip for clip");
#endif
          snapshot_push_fill (context->snapshot, svg_clip_get_path (clip), rule);
        }
      else
        {
          /* In the general case, we collect the clip geometry in a mask.
           * We special-case a single shape in the <clipPath> without
           * transforms and translate them to a clip or a fill.
           */
          Shape *clip_shape = svg_clip_get_shape (clip);
          SvgValue *ctf = clip_shape->current[SHAPE_ATTR_TRANSFORM];
          Shape *child = NULL;

          if (clip_shape->shapes->len > 0)
            child = g_ptr_array_index (clip_shape->shapes, 0);

          if (svg_transform_is_none (ctf) &&
              svg_enum_get (clip_shape->current[SHAPE_ATTR_CONTENT_UNITS]) == COORD_UNITS_USER_SPACE_ON_USE &&
              clip_shape->shapes->len == 1 &&
              (child->type == SHAPE_PATH || child->type == SHAPE_RECT || child->type == SHAPE_CIRCLE) &&
              svg_enum_get (child->current[SHAPE_ATTR_VISIBILITY]) != VISIBILITY_HIDDEN &&
              svg_enum_get (child->current[SHAPE_ATTR_DISPLAY]) != DISPLAY_NONE &&
              svg_clip_get_kind (child->current[SHAPE_ATTR_CLIP_PATH]) == CLIP_NONE &&
              svg_transform_is_none (child->current[SHAPE_ATTR_TRANSFORM]))
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

              if (!svg_transform_is_none (ctf))
                {
                  GskTransform *transform = svg_transform_get_gsk (ctf);

                  if (_gtk_bitmask_get (clip_shape->attrs, SHAPE_ATTR_TRANSFORM_ORIGIN))
                    {
                      SvgValue *tfo = clip_shape->current[SHAPE_ATTR_TRANSFORM_ORIGIN];
                      SvgValue *tfb = clip_shape->current[SHAPE_ATTR_TRANSFORM_BOX];
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

                      x = svg_numbers_get (tfo, 0, bounds.size.width);
                      y = svg_numbers_get (tfo, 1, bounds.size.width);

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

              if (svg_enum_get (clip_shape->current[SHAPE_ATTR_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
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

              render_shape (clip_shape, context);

              if (svg_enum_get (clip_shape->current[SHAPE_ATTR_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
                {
                  pop_transform (context);
                  gtk_snapshot_restore (context->snapshot);
                }

              if (!svg_transform_is_none (ctf))
                {
                  pop_transform (context);
                  gtk_snapshot_restore (context->snapshot);
                }

              gtk_snapshot_pop (context->snapshot); /* mask */
            }
        }

      pop_op (context);
    }

  if (svg_mask_get_kind (mask) != MASK_NONE &&
      svg_mask_get_shape (mask) != NULL &&
      context->op != CLIPPING)
    {
      Shape *mask_shape = svg_mask_get_shape (mask);
      gboolean has_clip = FALSE;

      push_op (context, MASKING);

#ifdef DEBUG
      if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
        gtk_snapshot_push_debug (context->snapshot, "mask for masking");
#endif
      gtk_snapshot_push_mask (context->snapshot, svg_enum_get (mask_shape->current[SHAPE_ATTR_MASK_TYPE]));

      if (_gtk_bitmask_get (mask_shape->attrs, SHAPE_ATTR_X) ||
          _gtk_bitmask_get (mask_shape->attrs, SHAPE_ATTR_Y) ||
          _gtk_bitmask_get (mask_shape->attrs, SHAPE_ATTR_WIDTH) ||
          _gtk_bitmask_get (mask_shape->attrs, SHAPE_ATTR_HEIGHT))
        {
           graphene_rect_t mask_clip;

          if (svg_enum_get (mask_shape->current[SHAPE_ATTR_BOUND_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
            {
              graphene_rect_t bounds;

              if (shape_get_current_bounds (shape, context->viewport, context->svg, &bounds))
                {
                  mask_clip.origin.x = bounds.origin.x + svg_number_get (mask_shape->current[SHAPE_ATTR_X], 1) * bounds.size.width;
                  mask_clip.origin.y = bounds.origin.y + svg_number_get (mask_shape->current[SHAPE_ATTR_Y], 1) * bounds.size.height;
                  mask_clip.size.width = svg_number_get (mask_shape->current[SHAPE_ATTR_WIDTH], 1) * bounds.size.width;
                  mask_clip.size.height = svg_number_get (mask_shape->current[SHAPE_ATTR_HEIGHT], 1) * bounds.size.height;
                  has_clip = TRUE;
                }
            }
          else
            {
              mask_clip.origin.x = svg_number_get (mask_shape->current[SHAPE_ATTR_X], context->viewport->size.width);
              mask_clip.origin.y = svg_number_get (mask_shape->current[SHAPE_ATTR_Y], context->viewport->size.height);
              mask_clip.size.width = svg_number_get (mask_shape->current[SHAPE_ATTR_WIDTH], context->viewport->size.width);
              mask_clip.size.height = svg_number_get (mask_shape->current[SHAPE_ATTR_HEIGHT], context->viewport->size.height);
              has_clip = TRUE;
            }

          if (has_clip)
            gtk_snapshot_push_clip (context->snapshot, &mask_clip);
        }

      if (svg_enum_get (mask_shape->current[SHAPE_ATTR_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
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

      render_shape (mask_shape, context);

      if (svg_enum_get (mask_shape->current[SHAPE_ATTR_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
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
  SvgValue *clip = shape->current[SHAPE_ATTR_CLIP_PATH];
  SvgValue *mask = shape->current[SHAPE_ATTR_MASK];
  SvgValue *tf = shape->current[SHAPE_ATTR_TRANSFORM];
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

  if (svg_mask_get_kind (mask) != MASK_NONE &&
      svg_mask_get_shape (mask) != NULL &&
      context->op != CLIPPING)
    {
      gtk_snapshot_pop (context->snapshot);
#ifdef DEBUG
      if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
        gtk_snapshot_pop (context->snapshot);
#endif
    }

  if (svg_clip_get_kind (clip) == CLIP_PATH ||
      (svg_clip_get_kind (clip) == CLIP_URL && svg_clip_get_shape (clip) != NULL))
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

  if (shape->type != SHAPE_CLIP_PATH && !svg_transform_is_none (tf))
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
      SvgValue *stop_color = cs->current[color_stop_attr_idx (SHAPE_ATTR_STOP_COLOR)];
      GdkColor color;

      offset = MAX (svg_number_get (cs->current[color_stop_attr_idx (SHAPE_ATTR_STOP_OFFSET)], 1), offset);
      gdk_color_init_copy (&color, svg_color_get_color (stop_color));
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

  gradient_transform = svg_transform_get_gsk (tf);
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

  gradient_transform = svg_transform_get_gsk (tf);

  if (_gtk_bitmask_get (gradient->attrs, SHAPE_ATTR_TRANSFORM_ORIGIN))
    {
      SvgValue *tfo = gradient->current[SHAPE_ATTR_TRANSFORM_ORIGIN];
      double x, y;

      if (svg_numbers_get_unit (tfo, 0) == SVG_UNIT_PERCENTAGE)
        x = bounds->origin.x + svg_numbers_get (tfo, 0, bounds->size.width);
      else
        x = svg_numbers_get (tfo, 0, 1);

      if (svg_numbers_get_unit (tfo, 1) == SVG_UNIT_PERCENTAGE)
        y = bounds->origin.y + svg_numbers_get (tfo, 1, bounds->size.width);
      else
        y = svg_numbers_get (tfo, 1, 1);

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
  SvgValue *vb = paint_server_get_current_value (pattern, SHAPE_ATTR_VIEW_BOX, context);
  SvgContentFit *cf = (SvgContentFit *) paint_server_get_current_value (pattern, SHAPE_ATTR_CONTENT_FIT, context);
  GPtrArray *shapes;
  graphene_rect_t view_box;

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

  if (svg_view_box_get (vb, &view_box))
    {
      compute_viewport_transform (cf->is_none,
                                  cf->align_x,
                                  cf->align_y,
                                  cf->meet,
                                  &view_box,
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

  transform = svg_transform_get_gsk (tf);

  if (_gtk_bitmask_get (pattern->attrs, SHAPE_ATTR_TRANSFORM_ORIGIN))
    {
      SvgValue *tfo = pattern->current[SHAPE_ATTR_TRANSFORM_ORIGIN];
      double xx, yy;

      xx = svg_numbers_get (tfo, 0, child_bounds.size.width);
      yy = svg_numbers_get (tfo, 1, child_bounds.size.width);

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
paint_server (SvgValue              *paint,
              const graphene_rect_t *bounds,
              const graphene_rect_t *paint_bounds,
              PaintContext          *context)
{
  Shape *server = svg_paint_get_server_shape (paint);

  if (server == NULL ||
      paint_server_is_skipped (server, bounds, context))
    {
      gtk_snapshot_add_color (context->snapshot,
                              svg_paint_get_server_fallback (paint),
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
          SvgValue *stop_color = cs->current[color_stop_attr_idx (SHAPE_ATTR_STOP_COLOR)];
          GdkColor c;

          gdk_color_init_copy (&c, svg_color_get_color (stop_color));
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
  SvgValue *da;

  stroke = shape_create_basic_stroke (shape, context->viewport, context->svg->features & GTK_SVG_EXTENSIONS, context->weight);

  da = shape->current[SHAPE_ATTR_STROKE_DASHARRAY];
  if (svg_dash_array_get_kind (da) != DASH_ARRAY_NONE)
    {
      unsigned int len;
      double path_length;
      double length;
      double offset;
      GskPathMeasure *measure;
      gboolean invalid = FALSE;
      float *vals;

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

      len = svg_dash_array_get_length (da);
      vals = g_newa (float, len);

      if (path_length > 0)
        {
          for (unsigned int i = 0; i < len; i++)
            {
              g_assert (svg_dash_array_get_unit (da, i) != SVG_UNIT_PERCENTAGE);
              vals[i] = svg_dash_array_get (da, i, 1) / path_length * length;
              if (vals[i] < 0)
                invalid = TRUE;
            }

          offset = offset / path_length * length;
        }
      else
        {
          for (unsigned int i = 0; i < len; i++)
            {
              g_assert (svg_dash_array_get_unit (da, i) != SVG_UNIT_PERCENTAGE);
              vals[i] = svg_dash_array_get (da, i, 1);
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

static SvgValue *
get_context_paint (SvgValue *paint,
                   GSList   *ctx_stack)
{
  switch (svg_paint_get_kind (paint))
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

          if (svg_paint_get_kind (paint) == PAINT_CONTEXT_FILL)
            paint = shape->current[SHAPE_ATTR_FILL];
          else
            paint = shape->current[SHAPE_ATTR_STROKE];

          return get_context_paint (paint, ctx_stack->next);
        }
      break;
    default:
      g_assert_not_reached ();
    }

  paint = svg_paint_new_none ();
  svg_value_unref (paint);

  return paint;
}

static void
fill_shape (Shape        *shape,
            GskPath      *path,
            SvgValue     *paint,
            double        opacity,
            PaintContext *context)
{
  graphene_rect_t bounds;
  GskFillRule fill_rule;

  paint = get_context_paint (paint, context->ctx_shape_stack);
  if (svg_paint_get_kind (paint) == PAINT_NONE)
    return;

  if (!gsk_path_get_bounds (path, &bounds))
    return;

  fill_rule = svg_enum_get (shape->current[SHAPE_ATTR_FILL_RULE]);

  switch (svg_paint_get_kind (paint))
    {
    case PAINT_NONE:
      break;
    case PAINT_COLOR:
      {
        GdkColor color;

        gdk_color_init_copy (&color, svg_paint_get_color (paint));
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
  SvgValue *paint;
  GskStroke *stroke;
  graphene_rect_t bounds;
  graphene_rect_t paint_bounds;
  double opacity;
  VectorEffect effect;
  GskRenderNode *child;
  GskRenderNode *node;

  paint = shape->current[SHAPE_ATTR_STROKE];
  paint = get_context_paint (paint, context->ctx_shape_stack);
  if (svg_paint_get_kind (paint) == PAINT_NONE)
    return;

  if (!gsk_path_get_bounds (path, &bounds))
    return;

  stroke = shape_create_stroke (shape, context);
  if (!gsk_path_get_stroke_bounds (path, stroke, &paint_bounds))
    return;

  opacity = svg_number_get (shape->current[SHAPE_ATTR_STROKE_OPACITY], 1);
  effect = svg_enum_get (shape->current[SHAPE_ATTR_VECTOR_EFFECT]);

  switch (svg_paint_get_kind (paint))
    {
    case PAINT_COLOR:
      {
        GdkColor color;
        gdk_color_init_copy (&color, svg_paint_get_color (paint));
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

      if (!child)
        child = empty_node ();
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
  SvgValue *vb;
  SvgContentFit *cf;
  SvgValue *overflow;
  double sx, sy, tx, ty;
  graphene_rect_t view_box;
  SvgValue *v;
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

  vb = marker->current[SHAPE_ATTR_VIEW_BOX];
  cf = (SvgContentFit *) marker->current[SHAPE_ATTR_CONTENT_FIT];
  overflow = marker->current[SHAPE_ATTR_OVERFLOW];

  width = svg_number_get (marker->current[SHAPE_ATTR_WIDTH], context->viewport->size.width);
  height = svg_number_get (marker->current[SHAPE_ATTR_HEIGHT], context->viewport->size.height);

  if (width == 0 || height == 0)
    return;

  v = marker->current[SHAPE_ATTR_REF_X];
  if (svg_number_get_unit (v) == SVG_UNIT_PERCENTAGE)
    x = svg_number_get (v, width);
  else if (svg_view_box_get (vb, &view_box))
    x = svg_number_get (v, 100) / view_box.size.width * width;
  else
    x = svg_number_get (v, 100);

  v = marker->current[SHAPE_ATTR_REF_Y];
  if (svg_number_get_unit (v) == SVG_UNIT_PERCENTAGE)
    y = svg_number_get (v, height);
  else if (svg_view_box_get (vb, &view_box))
    y = svg_number_get (v, 100) / view_box.size.height * height;
  else
    y = svg_number_get (v, 100);

  width *= scale;
  height *= scale;
  x *= scale;
  y *= scale;

  if (!svg_view_box_get (vb, &view_box))
    graphene_rect_init (&view_box, 0, 0, width, height);

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
  PangoLayout *layout;
  PangoAttrList *attr_list;
  PangoAttribute *attr;
  TextDecoration decoration;
  int w,h;
  PangoLayoutIter *iter;
  double offset;
  SvgValue *font_family;

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

  font_family = self->current[SHAPE_ATTR_FONT_FAMILY];
  if (svg_string_list_get_length (font_family) > 0)
    {
      GString *s = g_string_new ("");

      for (unsigned int i = 0; i < svg_string_list_get_length (font_family); i++)
        {
          if (i > 0)
            g_string_append_c (s, ',');
          g_string_append (s, svg_string_list_get (font_family, i));
        }
      pango_font_description_set_family (font_desc, s->str);
      g_string_free (s, TRUE);
    }

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

  for (unsigned int i = 0; i < self->text->len; i++)
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
  for (unsigned int i = 0; i < self->text->len; i++)
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
           SvgValue              *paint,
           const graphene_rect_t *bounds)
{
  g_assert (shape_type_has_text (self->type));

  for (unsigned int i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);

      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          {
            SvgValue *cpaint = paint;
            const graphene_rect_t *cbounds = bounds;

            if (svg_enum_get (node->shape.shape->current[SHAPE_ATTR_DISPLAY]) == DISPLAY_NONE)
              continue;

            if (svg_enum_get (node->shape.shape->current[SHAPE_ATTR_VISIBILITY]) == VISIBILITY_HIDDEN)
              continue;

            if (_gtk_bitmask_get (node->shape.shape->attrs, SHAPE_ATTR_FILL))
              {
                cpaint = node->shape.shape->current[SHAPE_ATTR_FILL];
                if (node->shape.has_bounds)
                  cbounds = &node->shape.bounds;
              }
            fill_text (node->shape.shape, context, cpaint, cbounds);
          }
          break;
        case TEXT_NODE_CHARACTERS:
          {
            double opacity;

            if (svg_paint_get_kind (paint) == PAINT_NONE)
              goto skip;

#if 0
            GskRoundedRect brd;
            gsk_rounded_rect_init_from_rect (&brd, bounds, 0.f);
            GdkRGBA red = (GdkRGBA){ .red = 1., .green = 0., .blue = 0., .alpha = 1. };
            gtk_snapshot_append_border (context->snapshot, &brd, (const float[4]){ 3.f, 3.f, 3.f, 3.f }, (const GdkRGBA[4]){ red, red, red, red });
#endif

            gtk_snapshot_save (context->snapshot);

            opacity = svg_number_get (self->current[SHAPE_ATTR_FILL_OPACITY], 1);
            if (svg_paint_get_kind (paint) == PAINT_COLOR)
              {
                GdkColor color;

                gdk_color_init_copy (&color, svg_paint_get_color (paint));
                color.alpha *= opacity;
                gtk_snapshot_translate (context->snapshot, &GRAPHENE_POINT_INIT (node->characters.x, node->characters.y));
                gtk_snapshot_rotate (context->snapshot, node->characters.r);
                gtk_snapshot_add_layout (context->snapshot, node->characters.layout, &color);
              }
            else if (paint_is_server (svg_paint_get_kind (paint)))
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
             SvgValue              *paint,
             GskStroke             *stroke,
             const graphene_rect_t *bounds)
{
  g_assert (shape_type_has_text (self->type));

  for (unsigned int i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);

      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          {
            SvgValue *cpaint = paint;
            const graphene_rect_t *cbounds = bounds;

            if (svg_enum_get (node->shape.shape->current[SHAPE_ATTR_DISPLAY]) == DISPLAY_NONE)
              continue;

            if (svg_enum_get (node->shape.shape->current[SHAPE_ATTR_VISIBILITY]) == VISIBILITY_HIDDEN)
              continue;

            if (_gtk_bitmask_get (node->shape.shape->attrs, SHAPE_ATTR_STROKE))
              {
                cpaint = node->shape.shape->current[SHAPE_ATTR_STROKE];
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

            if (svg_paint_get_kind (paint) == PAINT_NONE)
              goto skip;

            gtk_snapshot_save (context->snapshot);
            opacity = svg_number_get (self->current[SHAPE_ATTR_STROKE_OPACITY], 1);
            if (svg_paint_get_kind (paint) == PAINT_COLOR)
              {
                GdkColor color;

                gdk_color_init_copy (&color, svg_paint_get_color (paint));
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
            else if (paint_is_server (svg_paint_get_kind (paint)))
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
  SvgComputeContext ctx;

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
          if (svg_writing_mode_is_vertical (wmode))
            dy = - bounds.size.height / 2;
          else
            dx = - bounds.size.width / 2;
          break;
        case TEXT_ANCHOR_END:
          if (svg_writing_mode_is_vertical (wmode))
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
          fill_text (shape, context, paint, &bounds);
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
              fill_text (shape, context, shape->current[SHAPE_ATTR_FILL], &bounds);
              break;
            case PAINT_ORDER_STROKE_FILL_MARKERS:
            case PAINT_ORDER_STROKE_MARKERS_FILL:
            case PAINT_ORDER_MARKERS_STROKE_FILL:
              stroke_text (shape, context, shape->current[SHAPE_ATTR_STROKE], stroke, &bounds);
              break;
            default:
              g_assert_not_reached ();
            }

          switch (svg_enum_get (shape->current[SHAPE_ATTR_PAINT_ORDER]))
            {
            case PAINT_ORDER_FILL_STROKE_MARKERS:
            case PAINT_ORDER_FILL_MARKERS_STROKE:
            case PAINT_ORDER_MARKERS_FILL_STROKE:
              stroke_text (shape, context, shape->current[SHAPE_ATTR_STROKE], stroke, &bounds);
              break;
            case PAINT_ORDER_STROKE_FILL_MARKERS:
            case PAINT_ORDER_STROKE_MARKERS_FILL:
            case PAINT_ORDER_MARKERS_STROKE_FILL:
              fill_text (shape, context, shape->current[SHAPE_ATTR_FILL], &bounds);
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
      SvgValue *paint = shape->current[SHAPE_ATTR_FILL];
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
  return FALSE;

  if (self->node == NULL)
    return FALSE;

  if (self->state != self->node_for.state)
    {
      dbg_print ("cache", "Can't reuse rendernode: state change");
      return FALSE;
    }

  if (self->current_time != self->node_for.time)
    {
      dbg_print ("cache", "Can't reuse rendernode: current_time change");
      return FALSE;
    }

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
  const GdkRGBA *used_colors;
  GdkRGBA solid_colors[5];
  size_t n_used_colors;
  float used_opacity;

  if (self->width < 0 || self->height < 0)
    return;

  if (self->load_time == INDEFINITE)
    {
      int64_t current_time;

      /* If we get here and load_time is still INDEFINITE, we are
       * rendering an animation properly, we just do a snapshot.
       *
       * But we still need to get initial animation state applied.
       */
      if (self->clock)
        current_time = gdk_frame_clock_get_frame_time (self->clock);
      else
        current_time = 0;

      gtk_svg_set_load_time (self, current_time);
    }

  if (!can_reuse_node (self, width, height, colors, n_colors, weight))
    {
      SvgComputeContext compute_context;
      PaintContext paint_context;

      g_clear_pointer (&self->node, gsk_render_node_unref);

      /* Traditional symbolics often have overlapping shapes,
       * causing things to look wrong when using colors with
       * alpha. To work around that, we always draw them with
       * solid colors and apply foreground opacity globally.
       */
      if (self->gpa_version == 0 &&
          (self->features & GTK_SVG_TRADITIONAL_SYMBOLIC) != 0 &&
          colors[GTK_SYMBOLIC_COLOR_FOREGROUND].alpha < 1)
        {
          used_opacity = colors[GTK_SYMBOLIC_COLOR_FOREGROUND].alpha;
          n_used_colors = MIN (n_colors, 5);
          used_colors = solid_colors;
          for (unsigned int i = 0; i < n_used_colors; i++)
            {
              solid_colors[i] = colors[i];
              solid_colors[i].alpha = 1;
            }
        }
      else
        {
          used_opacity = 1;
          used_colors = colors;
          n_used_colors = n_colors;
        }

      self->current_width = width;
      self->current_height = height;

      compute_context.svg = self;
      compute_context.viewport = &viewport;
      compute_context.colors = used_colors;
      compute_context.n_colors = n_used_colors;
      compute_context.current_time = self->current_time;
      compute_context.parent = NULL;
      compute_context.interpolation = GDK_COLOR_STATE_SRGB;

      compute_current_values_for_shape (self->content, &compute_context);

      gtk_snapshot_push_collect (snapshot);

      if (self->gpa_version == 0 &&
          (self->features & GTK_SVG_TRADITIONAL_SYMBOLIC) != 0 &&
          used_opacity < 1)
        {
          gtk_snapshot_push_opacity (snapshot, used_opacity);
        }

      paint_context.svg = self;
      paint_context.viewport = &viewport;
      paint_context.viewport_stack = NULL;
      paint_context.snapshot = snapshot;
      paint_context.colors = used_colors;
      paint_context.n_colors = n_used_colors;
      paint_context.weight = self->weight >= 1 ? self->weight : weight;
      paint_context.op = RENDERING;
      paint_context.op_stack = NULL;
      paint_context.ctx_shape_stack = NULL;
      paint_context.current_time = self->current_time;
      paint_context.depth = 0;
      paint_context.transforms = NULL;
      paint_context.instance_count = 0;

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

      if (self->gpa_version == 0 &&
          (self->features & GTK_SVG_TRADITIONAL_SYMBOLIC) != 0 &&
          used_opacity < 1)
        {
          gtk_snapshot_pop (snapshot);
        }

      self->node = gtk_snapshot_pop_collect (snapshot);

      self->node_for.width = width;
      self->node_for.height = height;
      memcpy (self->node_for.colors, colors, n_colors * sizeof (GdkRGBA));
      self->node_for.n_colors = n_colors;
      self->node_for.weight = weight;
    }

  if (self->node)
    gtk_snapshot_append_node (snapshot, self->node);

  if (self->advance_after_snapshot)
    {
      self->advance_after_snapshot = FALSE;
      g_clear_handle_id (&self->pending_advance, g_source_remove);
      self->pending_advance = g_idle_add_once (advance_later, self);
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
  SvgValue *vb;
  graphene_rect_t view_box;

  if (self->width > 0 && self->height > 0)
    return self->width / self->height;

  vb = self->content->current[SHAPE_ATTR_VIEW_BOX];
  if (svg_view_box_get (vb, &view_box))
    {
      if (view_box.size.width > 0 && view_box.size.height > 0)
        return view_box.size.width / view_box.size.height;
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
  PROP_OVERFLOW,
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
  self->initial_state = 0;
  self->state = 0;
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
  g_clear_handle_id (&self->pending_advance, g_source_remove);

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

  g_strfreev (self->state_names);

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

    case PROP_OVERFLOW:
      g_value_set_enum (value, self->overflow);
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

    case PROP_OVERFLOW:
      gtk_svg_set_overflow (self, g_value_get_enum (value));
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
   * This can be a number between 0 and 63.
   *
   * Since: 4.22
   */
  properties[PROP_STATE] =
    g_param_spec_uint ("state", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSvg:overflow:
   *
   * Whether the rendering will be clipped to the bounds.
   *
   * Since: 4.24
   */
  properties[PROP_OVERFLOW] =
    g_param_spec_enum ("overflow", NULL, NULL,
                       GTK_TYPE_OVERFLOW, GTK_OVERFLOW_HIDDEN,
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
      time_spec_print (spec, NULL, s);
      g_string_append (s, "\n");
    }
  g_print ("%s", s->str);
  g_string_free (s, TRUE);
}
#endif

/* }}} */
/* {{{ Private API */

void
svg_foreach_shape (Shape         *shape,
                   ShapeCallback  callback,
                   gpointer       user_data)
{
  callback (shape, user_data);

  if (shape->shapes)
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          Shape *sh = g_ptr_array_index (shape->shapes, i);
          svg_foreach_shape (sh, callback, user_data);
        }
    }
}

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
    gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
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
 * 'beginning of time' for any animations defined
 * in it.
 *
 * The load time will be set as a side-effect when
 * the animation starts runnning due to `playing`
 * being set to `TRUE`.
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
 * Advances the animation to the given value, which must
 * be in microseconds and in the same timescale as the
 * times returned by [class@Gdk.FrameClock].
 *
 * Note that this function is only useful when *not*
 * running the animations automatically with a frame
 * clock, via [method@Gtk.Svg.play].
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
 * a timeout for that time, and advance the animation then.
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
            unsigned int   n_colors)
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

      SvgComputeContext context;
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
      if (self->n_state_names > 0)
        {
          indent_for_attr (s, 0);
          g_string_append (s, "gpa:state-names='");
          for (unsigned int i = 0; i < self->n_state_names; i++)
            {
              if (i > 0)
                g_string_append_c (s, ' ');
              g_string_append (s, self->state_names[i]);
            }
          g_string_append (s, "'");
        }

      indent_for_attr (s, 0);
      if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
        g_string_append_printf (s, "gpa:state='%u'", self->state);
      else
        g_string_append_printf (s, "gpa:state='%u'", self->initial_state);
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
          size_t len;
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
          g_string_append (s, "href='data:font/ttf;base64,\\\n");
          string_append_base64 (s, bytes);
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
      return svg_number_get (value,
                             svg_shape_attr_get_number (shape,
                                                        SHAPE_ATTR_STROKE_WIDTH,
                                                        viewport));
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

  if (_gtk_bitmask_get (shape->attrs, attr))
    value = shape_ref_base_value (shape, NULL, attr, 0);
  else
    value = shape_attr_ref_initial_value (attr, shape->type, shape->parent != NULL);

  *symbolic = 0xffff;
  *color = (GdkRGBA) { 0, 0, 0, 1 };

  switch (svg_paint_get_kind (value))
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
      *symbolic = svg_paint_get_symbolic (value);
      break;
    case PAINT_COLOR:
      *color = *(const GdkRGBA *) &(svg_paint_get_color (value)->values);
      break;
    default:
      g_assert_not_reached ();
    }

  svg_value_unref (value);

  return svg_paint_get_kind (value);
}

double *
svg_shape_attr_get_points (Shape        *shape,
                           ShapeAttr     attr,
                           unsigned int *n_params)
{
  g_return_val_if_fail (shape_has_attr (shape->type, attr), NULL);
  SvgValue *value;
  double *ret;

  if (_gtk_bitmask_get (shape->attrs, attr))
    value = shape_ref_base_value (shape, NULL, attr, 0);
  else
    value = shape_attr_ref_initial_value (attr, shape->type, shape->parent != NULL);

  *n_params = svg_numbers_get_length (value);

  ret = g_new0 (double, *n_params);

  for (unsigned int i = 0; i < *n_params; i++)
    {
      /* FIXME: What about the dimension ? */
      ret[i] = svg_numbers_get (value, i, 1);
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
svg_shape_attr_get_clip (Shape       *shape,
                         ShapeAttr    attr,
                         GskPath    **path,
                         const char **ref)
{
  g_return_val_if_fail (shape_has_attr (shape->type, attr), CLIP_NONE);
  SvgValue *value;

  if (_gtk_bitmask_get (shape->attrs, attr))
    value = shape_ref_base_value (shape, NULL, attr, 0);
  else
    value = shape_attr_ref_initial_value (attr, shape->type, shape->parent != NULL);

  switch (svg_clip_get_kind (value))
    {
    case CLIP_NONE:
      *path = NULL;
      *ref = NULL;
      break;
    case CLIP_PATH:
      *path = svg_clip_get_path (value);
      *ref = NULL;
      break;
    case CLIP_URL:
      *path = NULL;
      *ref = svg_clip_get_id (value);
      break;
    default:
      g_assert_not_reached ();
    }

  svg_value_unref (value);

  return svg_clip_get_kind (value);
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

const char *
svg_shape_attr_get_mask (Shape     *shape,
                         ShapeAttr  attr)
{
  g_return_val_if_fail (shape_has_attr (shape->type, attr), NULL);
  SvgValue *mask;
  const char *id;

  if (_gtk_bitmask_get (shape->attrs, attr))
    mask = shape_ref_base_value (shape, NULL, attr, 0);
  else
    mask = shape_attr_ref_initial_value (attr, shape->type, shape->parent != NULL);

  if (svg_mask_get_kind (mask) == MASK_NONE)
    id = NULL;
  else
    id = svg_mask_get_id (mask);

  svg_value_unref (mask);

  return id;
}

gboolean
svg_shape_attr_get_viewbox (Shape           *shape,
                            ShapeAttr        attr,
                            graphene_rect_t *rect)
{
  g_return_val_if_fail (shape_has_attr (shape->type, attr), FALSE);
  SvgValue *vb;
  gboolean result;

  if (_gtk_bitmask_get (shape->attrs, attr))
    vb = shape_ref_base_value (shape, NULL, attr, 0);
  else
    vb = shape_attr_ref_initial_value (attr, shape->type, shape->parent != NULL);

  result = svg_view_box_get (vb, rect);
  svg_value_unref (vb);

  return result;
}

void
svg_shape_attr_set (Shape     *shape,
                    ShapeAttr  attr,
                    SvgValue  *value)
{
  svg_value_unref (shape->base[attr]);
  if (value)
    shape->base[attr] = value;
  else
    shape->base[attr] = shape_attr_ref_initial_value (attr, shape->type, shape->parent != NULL);
  shape->attrs = _gtk_bitmask_set (shape->attrs, attr, value != NULL);
}

gboolean
svg_shape_has_attr (Shape     *shape,
                    ShapeAttr  attr)
{
  return shape_has_attr (shape->type, attr);
}

Shape *
svg_shape_add (Shape     *parent,
               ShapeType  type)
{
  Shape *shape = shape_new (parent, type);

  g_ptr_array_add (parent->shapes, shape);

  if (type == SHAPE_TSPAN)
    {
      Shape *text_parent = NULL;

      if (shape_type_has_text (parent->type))
        text_parent = parent;
      else if (parent->parent &&
               parent->type == SHAPE_LINK &&
               shape_type_has_text (parent->parent->type))
        text_parent = parent->parent;

      if (text_parent)
        {
          TextNode node = {
            .type = TEXT_NODE_SHAPE,
            .shape = { .shape = shape }
          };
          g_array_append_val (parent->text, node);
        }
    }

  return shape;
}

void
svg_shape_delete (Shape *shape)
{
  if (shape->text)
    for (unsigned int i = 0; i < shape->text->len; i++)
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

const char *
svg_shape_get_name (ShapeType type)
{
  return shape_types[type].name;
}

/* }}} */

/*< private>
 * gtk_svg_set_playing:
 * @self: an SVG paintable
 * @playing: the new state
 *
 * Sets whether the paintable is animating its content.
 *
 * If `playing` is set to true, and the paintable has
 * a frame clock, then it will automatically advance
 * the animation.
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

      g_clear_handle_id (&self->pending_advance, g_source_remove);
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
  self->initial_state = 0;
  self->state = 0;
  self->max_state = 0;
  self->state_change_delay = 0;
  self->used = 0;

  self->gpa_version = 0;

  g_clear_pointer (&self->state_names, g_strfreev);
  self->n_state_names = 0;
}

/**
 * gtk_svg_set_overflow:
 * @self: an SVG paintable
 * @overflow: the new overflow value
 *
 * Sets whether the rendering will be clipped
 * to the bounds.
 *
 * Clipping is expected for [iface@Gdk.Paintable]
 * semantics, so this property should not be
 * changed when using a `GtkSvg` as a paintable.
 *
 * Since: 4.24
 */
void
gtk_svg_set_overflow (GtkSvg      *self,
                      GtkOverflow  overflow)
{
  g_return_if_fail (GTK_IS_SVG (self));

  if (self->overflow == overflow)
    return;

  self->overflow = overflow;
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OVERFLOW]);
}

/**
 * gtk_svg_get_overflow:
 * @self: an SVG paintable
 *
 * Gets the current overflow value.
 *
 * Returns: the current overflow value
 *
 * Since: 4.24
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

/*< private>
 * gtk_svg_set_state_names:
 * @svg: a `GtkSvg`
 * @names: (array zero-terminated=1): a `NULL`-terminated arrayt
 *   of strings
 *
 * Sets names for states.
 *
 * Returns: true if the state names were set successfully
 */
gboolean
gtk_svg_set_state_names (GtkSvg      *svg,
                         const char **names)
{
  for (unsigned int i = 0; names[i]; i++)
    {
      if (!valid_state_name (names[i]))
        return FALSE;
    }

  if (!strv_unique ((GStrv) names))
    return FALSE;

  g_strfreev (svg->state_names);
  svg->state_names = g_strdupv ((char **) names);
  svg->n_state_names = g_strv_length ((char **) names);
  return TRUE;
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
 * The weight affects the effective linewidth when stroking
 * paths.
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

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
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
 * @state: the state to set, as a value between 0 and 63
 *
 * Sets the state of the paintable.
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
  g_return_if_fail (state <= 63);

  if (self->state == state)
    return;

  previous_state = self->state;
  self->state = state;

  if (self->clock && self->playing)
    self->current_time = MAX (self->current_time, gdk_frame_clock_get_frame_time (self->clock));

  if ((self->features & GTK_SVG_EXTENSIONS) == 0)
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
      return;
    }

  if ((self->features & GTK_SVG_ANIMATIONS) == 0 ||
      !self->playing)
    {
      if (self->gpa_version > 0)
        {
          apply_state (self, state);
          gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
        }
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
 * gtk_svg_get_state_names:
 * @self: an SVG paintable
 * @length: (out) (nullable): return location for the number
 *   of strings that are returned
 *
 * Returns a `NULL`-terminated array of
 * state names, if available.
 *
 * Note that the returned array and the strings
 * contained in it will only be valid until the
 * `GtkSvg` is cleared or reloaded, so if you
 * want to keep it around, you should make a copy.
 *
 * Returns: (nullable) (transfer none): the state names
 *
 * Since: 4.22
 */
const char **
gtk_svg_get_state_names (GtkSvg       *self,
                         unsigned int *length)
{
  g_return_val_if_fail (GTK_IS_SVG (self), NULL);

  if (length)
    *length = self->n_state_names;

  return (const char **) self->state_names;
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
 * @GTK_SVG_ERROR_IGNORED_ELEMENT: An XML element is ignored,
 *   but it should not affect rendering (this error code is used
 *   for metadata and exension elements)
 * @GTK_SVG_ERROR_LIMITS_EXCEEDED: An implementation limit has
 *   been hit, such as the number of loaded shapes.
 * @GTK_SVG_ERROR_NOT_IMPLEMENTED: The SVG uses features that
 *   are not supported by `GtkSvg`. It may be advisable to use
 *   a different SVG renderer.
 *
 * Error codes in the `GTK_SVG_ERROR` domain for errors
 * that happen during parsing or rendering of SVG.
 *
 * Since: 4.22
 */

/* GTK_SVG_ERROR_FEATURE_DISABLED:
 *
 * The SVG uses features that have been disabled with `gtk_svg_set_features`.
 *
 * Since: 4.24
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

