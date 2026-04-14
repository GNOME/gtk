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

#include "gtksvgprivate.h"
#include "gtksvgserializeprivate.h"
#include "gtksvgparserprivate.h"

#include "gtkenums.h"
#include "gtksymbolicpaintable.h"
#include "gtktypebuiltins.h"
#include "gtkmarshalers.h"
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
#include "gtksvgfilterrefprivate.h"
#include "gtksvgtransformprivate.h"
#include "gtksvgcolorprivate.h"
#include "gtksvgpaintprivate.h"
#include "gtksvgfilterfunctionsprivate.h"
#include "gtksvgdasharrayprivate.h"
#include "gtksvgpathprivate.h"
#include "gtksvgpathdataprivate.h"
#include "gtksvgclipprivate.h"
#include "gtksvgmaskprivate.h"
#include "gtksvgviewboxprivate.h"
#include "gtksvgcontentfitprivate.h"
#include "gtksvgorientprivate.h"
#include "gtksvglanguageprivate.h"
#include "gtksvghrefprivate.h"
#include "gtksvgtextdecorationprivate.h"
#include "gtksvgpathutilsprivate.h"
#include "gtksvgcolorutilsprivate.h"
#include "gtksvgpangoutilsprivate.h"
#include "gtksvglocationprivate.h"
#include "gtksvgerrorprivate.h"
#include "gtksvgelementtypeprivate.h"
#include "gtksvgpropertyprivate.h"
#include "gtksvgfiltertypeprivate.h"
#include "gtksvgfilterprivate.h"
#include "gtksvgcolorstopprivate.h"
#include "gtksvgelementinternal.h"
#include "gtksvganimationprivate.h"

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
 * The paintable supports much of [SVG 2](https://svgwg.org/svg2-draft/),
 * including [animations](https://svgwg.org/specs/animations/), with some
 * exceptions.
 *
 * Among the graphical elements, `<textPath>` and `<foreignObject>` are
 * not supported.
 *
 * In the `<filter>` element, the following primitives are not supported:
 * feConvolveMatrix, feDiffuseLighting, feMorphology, feSpecularLighting
 * and feTurbulence.
 *
 * Support for the `mask` attribute is limited to just a url referring to
 * the `<mask>` element by ID.
 *
 * In animation elements, the parsing of `begin` and `end` attributes is
 * limited, and the `min` and `max` attributes are not supported.
 *
 * The interactive aspects of SVG are supported by [class@Gtk.SvgWidget].
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
 *
 * <image src="svg-renderer1.svg">
 *
 * Note that the generated animations are implemented using standard
 * SVG attributes (`visibility`, `stroke-dasharray, `stroke-dashoffset`,
 * `pathLength` and `filter`). Setting these attributes in your SVG
 * is therefore going to interfere with generated animations.
 *
 * To connect general SVG animations to the states of the paintable,
 * use the custom `StateChange(...)` condition in the `begin` and `end`
 * attributes of SVG animation elements. For example,
 *
 *     <animate href='path1'
 *              attributeName='fill'
 *              begin='StateChange(1 2 3, 0)'
 *              dur='300ms'
 *              fill='freeze'
 *              from='black'
 *              to='magenta'/>
 *
 * will make the fill color of path1 transition from black to
 * magenta when the renderer enters state 0 from states 1, 2, or 3.
 *
 * <image src="svg-renderer2.svg">
 *
 * The `StateChange(...)` condition triggers for upcoming state changes
 * as well, to support fade-out transitions. For example,
 *
 *     <animate href='path1'
 *              attributeName='opacity'
 *              begin='StateChange(0, 1 2 3) -300ms'
 *              dur='300ms'
 *              fill='freeze'
 *              from='1'
 *              to='0'/>
 *
 * will start a fade-out of path1 300ms before a transition from state
 * 0 to 1, 2 or 3.
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

/* This is a mitigation for SVG files which create deeply nested
 * <use> elements or deeply nested references of patterns.
 */
#define DRAWING_LIMIT 150000

#ifndef _MSC_VER
#undef DEBUG
#endif /* _MSC_VER */

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
    return g_ascii_formatd (buf, size, "%.3f", (time - time_base) / (double) G_TIME_SPAN_SECOND);
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

  gtk_svg_error_set_input (error, "svg");
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

  gtk_svg_error_set_input (error, "svg");
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

  gtk_svg_error_set_input (error, "svg");
  gtk_svg_error_set_element (error, g_markup_parse_context_get_element (context));
  gtk_svg_location_init_tag_range (&start, &end, context);
  gtk_svg_error_set_location (error, &start, &end);

  gtk_svg_emit_error (self, error);
  g_clear_error (&error);
}

G_GNUC_PRINTF (4, 5)
static void
gtk_svg_missing_attribute (GtkSvg              *self,
                           GMarkupParseContext *context,
                           const char          *attr_name,
                           const char          *format,
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

  gtk_svg_error_set_input (error, "svg");
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
gtk_svg_check_unhandled_attributes (GtkSvg               *self,
                                    GMarkupParseContext  *context,
                                    const char          **attr_names,
                                    uint64_t              handled)
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

static int64_t
get_current_time (GtkSvg *self)
{
  if (self->clock && self->playing && self->load_time != INDEFINITE)
    return MAX (self->current_time, gdk_frame_clock_get_frame_time (self->clock));
  return self->current_time;
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
        g_hash_table_insert (svg->images, g_strdup (string), g_object_ref (texture));
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

/* }}} */
/* {{{ States */

static gboolean
parse_states_css (GtkCssParser *parser,
                  GtkSvg       *svg,
                  uint64_t     *states)
{
  gtk_css_parser_skip_whitespace (parser);

  if (gtk_css_parser_try_ident (parser, "all"))
    {
      *states = ALL_STATES;
      return TRUE;
    }
  else if (gtk_css_parser_try_ident (parser, "none"))
    {
      *states = NO_STATES;
      return TRUE;
    }

  *states = NO_STATES;
  while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_skip_whitespace (parser);
      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
        {
          char *id = gtk_css_parser_consume_ident (parser);
          unsigned int u;

          if (!find_named_state (svg, id, &u))
            {
              *states = NO_STATES;
              g_free (id);
              return FALSE;
            }

          g_free (id);

          *states |= BIT (u);
        }
      else if (gtk_css_parser_has_integer (parser))
        {
          int i;

          gtk_css_parser_consume_integer (parser, &i);
          if (i < 0 || i > 63)
            {
              *states = NO_STATES;
              return FALSE;
            }

          *states |= BIT ((unsigned int) i);
        }
      else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COMMA))
        return TRUE;
      else
        return FALSE;
    }

  return TRUE;
}

static gboolean
parse_states (GtkSvg     *svg,
              const char *text,
              uint64_t   *states)
{
  GtkCssParser *parser = parser_new_for_string (text);
  gboolean ret;

  gtk_css_parser_skip_whitespace (parser);
  ret = parse_states_css (parser, svg, states);
  gtk_css_parser_skip_whitespace (parser);
  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    ret = FALSE;
  gtk_css_parser_unref (parser);

  return ret;
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
  TIME_SPEC_TYPE_EVENT,
} TimeSpecType;

typedef enum
{
  EVENT_TYPE_FOCUS,
  EVENT_TYPE_BLUR,
  EVENT_TYPE_MOUSE_ENTER,
  EVENT_TYPE_MOUSE_LEAVE,
  EVENT_TYPE_CLICK,
} EventType;

typedef enum
{
  TIME_SPEC_SIDE_BEGIN,
  TIME_SPEC_SIDE_END,
} TimeSpecSide;

struct _TimeSpec
{
  TimeSpecType type;
  int64_t offset;
  union {
    struct {
      char *ref;
      SvgAnimation *base;
      TimeSpecSide side;
    } sync;
    struct {
      uint64_t from;
      uint64_t to;
    } states;
    struct {
      char *ref;
      SvgElement *shape;
      EventType event;
    } event;
  };
  int64_t time;
  GPtrArray *animations;
};

static void
time_spec_clear (TimeSpec *t)
{
  if (t->type == TIME_SPEC_TYPE_SYNC)
    g_free (t->sync.ref);
  else if (t->type == TIME_SPEC_TYPE_EVENT)
    g_free (t->event.ref);

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
    case TIME_SPEC_TYPE_EVENT:
      t->event.ref = g_strdup (orig->event.ref);
      t->event.shape = orig->event.shape;
      t->event.event = orig->event.event;
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

    case TIME_SPEC_TYPE_EVENT:
      return ((t1->event.shape != NULL && t1->event.shape == t2->event.shape) ||
              (t1->event.ref != NULL && g_strcmp0 (t1->event.ref, t2->event.ref) == 0)) &&
             t1->event.event == t2->event.event &&
             t1->offset == t2->offset;

    default:
      g_assert_not_reached ();
    }
}

typedef struct
{
  GtkSvg *svg;
  uint64_t states[2];
  gboolean set[2];
} ParseStatesArg;

static unsigned int
parse_states_arg (GtkCssParser *parser,
                  unsigned int  n,
                  gpointer      data)
{
  ParseStatesArg *arg = data;
  uint64_t states;

  if (!parse_states_css (parser, arg->svg, &states))
    return 0;

  arg->states[n] = states;
  arg->set[n] = TRUE;
  return 1;
}

static gboolean
time_spec_parse (GtkCssParser  *parser,
                 GtkSvg        *svg,
                 SvgElement    *default_event_target,
                 TimeSpec      *spec,
                 GError       **error)
{
  const char *event_types[] = { "focus", "blur", "mouseenter", "mouseleave", "click" };
  memset (spec, 0, sizeof (TimeSpec));

  gtk_css_parser_skip_whitespace (parser);
  if (gtk_css_parser_try_ident (parser, "indefinite"))
    {
      spec->type = TIME_SPEC_TYPE_INDEFINITE;
    }
  else if (parser_try_duration (parser, &spec->offset))
    {
      spec->type = TIME_SPEC_TYPE_OFFSET;
    }
  else if (gtk_css_parser_has_function (parser, "StateChange"))
    {
      ParseStatesArg arg = {
        .svg = svg,
        .states = { NO_STATES, NO_STATES },
        .set = { FALSE, FALSE },
      };

      if (gtk_css_parser_consume_function (parser, 2, 2, parse_states_arg, &arg))
        {
          spec->type = TIME_SPEC_TYPE_STATES;
          spec->states.from = arg.states[0];
          spec->states.to = arg.states[1];

          gtk_css_parser_skip_whitespace (parser);
          parser_try_duration (parser, &spec->offset);
        }
      else
        return FALSE;
    }
  else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
    {
      char *id = gtk_css_parser_consume_ident (parser);

      if (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COLON) &&
          strcmp (id, "gpa") == 0)
        {
          g_free (id);

          if (gtk_css_parser_has_function (parser, "states"))
            {
              ParseStatesArg arg = {
                .svg = svg,
                .states = { NO_STATES, NO_STATES },
                .set = { FALSE, FALSE },
              };

              if (gtk_css_parser_consume_function (parser, 1, 2, parse_states_arg, &arg))
                {
                  spec->type = TIME_SPEC_TYPE_STATES;

                  if (!arg.set[1])
                    {
                      TimeSpecSide side;

                      if (!gtk_css_parser_try_delim (parser, '.'))
                        return FALSE;
                      if (gtk_css_parser_try_ident (parser, "begin"))
                        side = TIME_SPEC_SIDE_BEGIN;
                      else if (gtk_css_parser_try_ident (parser, "end"))
                        side = TIME_SPEC_SIDE_END;
                      else
                        return FALSE;

                      if (side == TIME_SPEC_SIDE_BEGIN)
                        {
                          spec->states.from = ALL_STATES & ~arg.states[0];
                          spec->states.to = arg.states[0];
                        }
                      else
                        {
                          spec->states.from = arg.states[0];
                          spec->states.to = ALL_STATES & ~arg.states[0];
                        }
                    }
                  else
                    {
                      spec->states.from = arg.states[0];
                      spec->states.to = arg.states[1];
                    }

                  gtk_css_parser_skip_whitespace (parser);
                  parser_try_duration (parser, &spec->offset);
                }

              if (*error == NULL)
                g_set_error (error,
                             GTK_SVG_ERROR, GTK_SVG_ERROR_INVALID_SYNTAX,
                             "gpa:states() is deprecated. Use StateChange()");
            }
          else
            return FALSE;
        }
      else if (gtk_css_parser_try_delim (parser, '.'))
        {
          unsigned int value;

          if (parser_try_enum (parser, (const char *[]) { "begin", "end" }, 2, &value))
            {
              spec->type = TIME_SPEC_TYPE_SYNC;
              spec->sync.ref = id;
              spec->sync.side = value;
              spec->sync.base = NULL;
            }
          else if (parser_try_enum (parser, event_types, G_N_ELEMENTS (event_types), &value))
            {
              spec->type = TIME_SPEC_TYPE_EVENT;
              spec->event.ref = id;
              spec->event.event = value;
              spec->event.shape = NULL;
            }
          else
            {
              g_free (id);
              return FALSE;
            }

          gtk_css_parser_skip_whitespace (parser);
          parser_try_duration (parser, &spec->offset);
        }
      else
        {
          unsigned int value;

          if (parse_enum (id, event_types, G_N_ELEMENTS (event_types), &value))
            {
              g_free (id);
              spec->type = TIME_SPEC_TYPE_EVENT;
              spec->event.ref = NULL;
              spec->event.event = value;
              spec->event.shape = default_event_target;
            }
          else
            {
              g_free (id);
              return FALSE;
            }

          gtk_css_parser_skip_whitespace (parser);
          parser_try_duration (parser, &spec->offset);
        }
    }
  else
    return FALSE;

  return TRUE;
}

void
time_spec_add_animation (TimeSpec     *spec,
                         SvgAnimation *a)
{
  if (!spec->animations)
    spec->animations = g_ptr_array_new ();
  g_ptr_array_add (spec->animations, a);
}

void
time_spec_drop_animation (TimeSpec     *spec,
                          SvgAnimation *a)
{
  g_ptr_array_remove (spec->animations, a);
}

static void animation_update_for_spec (SvgAnimation *a,
                                       TimeSpec     *spec);

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
          SvgAnimation *a = g_ptr_array_index (spec->animations, i);
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

static gboolean
matches_shape (SvgElement *shape,
               const char *ref,
               SvgElement *target)
{
  if (shape != NULL)
    return shape == target;
  if (ref != NULL)
    return g_strcmp0 (ref, target->id) == 0;
  return FALSE;
}

static void
time_spec_update_for_event (TimeSpec   *spec,
                            SvgElement *shape,
                            EventType   event,
                            int64_t     event_time)
{
  if (spec->type == TIME_SPEC_TYPE_EVENT)
    {
      if (matches_shape (spec->event.shape, spec->event.ref, shape) &&
          spec->event.event == event)
        time_spec_set_time (spec, event_time + spec->offset);
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
                   SvgAnimation *base,
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

static void
timeline_update_for_event (Timeline   *timeline,
                           SvgElement *shape,
                           EventType   event,
                           int64_t     event_time)
{
  for (unsigned int i = 0; i < timeline->times->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (timeline->times, i);
      time_spec_update_for_event (spec, shape, event, event_time);
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

static void
animations_update_for_pause (SvgElement *shape,
                             int64_t     duration)
{
  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          SvgAnimation *a = g_ptr_array_index (shape->animations, i);
          svg_animation_update_for_pause (a, duration);
        }
    }

  if (svg_element_type_is_path (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          SvgElement *sh = g_ptr_array_index (shape->shapes, i);
          animations_update_for_pause (sh, duration);
        }
    }
}

/* }}} */
/* {{{ Animated attributes */

/* SvgAnimation works by
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
shape_get_current_value (SvgElement   *shape,
                         unsigned int  attr,
                         unsigned int  idx)
{
  if (idx == 0)
    {
      return svg_element_get_current_value (shape, attr);
    }
  else if (FIRST_STOP_PROPERTY <= attr && attr <= LAST_STOP_PROPERTY)
    {
      SvgColorStop *stop;

      g_assert (svg_element_type_is_gradient (svg_element_get_type (shape)));
      g_assert (idx <= shape->color_stops->len);

      stop = g_ptr_array_index (shape->color_stops, idx - 1);

      return svg_color_stop_get_current_value (stop, attr);
    }
  else if (FIRST_FILTER_PROPERTY <= attr && attr <= LAST_FILTER_PROPERTY)
    {
      SvgFilter *f;

      g_assert (svg_element_type_is_filter (svg_element_get_type (shape)));
      g_assert (idx <= shape->filters->len);

      f = g_ptr_array_index (shape->filters, idx - 1);

      return svg_filter_get_current_value (f, attr);
    }
  else
    g_assert_not_reached ();
}

/* We pass parent separately instead of relying
 * on svg_element_get_parent (shape) because <use> overrides parent
 */
static SvgValue *
shape_ref_base_value (SvgElement   *shape,
                      SvgElement   *parent,
                      SvgProperty   attr,
                      unsigned int  idx)
{
  if (idx == 0)
    {
      SvgValue *value;

      value = svg_element_get_base_value (shape, attr);

      if (svg_value_is_unset (value))
        {
          if (svg_element_get_type (shape) == SVG_ELEMENT_RADIAL_GRADIENT)
            {
              if (attr == SVG_PROPERTY_FX)
                return shape_ref_base_value (shape, parent, SVG_PROPERTY_CX, idx);
              else if (attr == SVG_PROPERTY_FY)
                return shape_ref_base_value (shape, parent, SVG_PROPERTY_CY, idx);
            }

          if (parent && svg_property_inherited (attr))
            return svg_value_ref (parent->current[attr]);
          else
            return svg_property_ref_initial_value (attr, svg_element_get_type (shape), parent != NULL);
        }
      else if (svg_value_is_inherit (value))
        {
          if (parent)
            return svg_value_ref (svg_element_get_current_value (parent, attr));
          else
            return svg_property_ref_initial_value (attr, svg_element_get_type (shape), parent != NULL);
        }
      else if (svg_value_is_initial (value))
        {
          return svg_property_ref_initial_value (attr, svg_element_get_type (shape), parent != NULL);
        }
      else
        {
          return svg_value_ref (value);
        }
    }
  else if (FIRST_STOP_PROPERTY <= attr && attr <= LAST_STOP_PROPERTY)
    {
      SvgColorStop *stop;
      SvgValue *value;

      g_assert (svg_element_type_is_gradient (svg_element_get_type (shape)));
      g_assert (idx <= shape->color_stops->len);

      stop = g_ptr_array_index (shape->color_stops, idx - 1);

      value = svg_color_stop_get_base_value (stop, attr);
      if (svg_value_is_unset (value))
        {
          if (svg_property_inherited (attr))
            return svg_value_ref (svg_element_get_current_value (shape, attr));
          else
            return svg_property_ref_initial_value (attr, svg_element_get_type (shape), parent != NULL);
        }
      else if (svg_value_is_inherit (value))
        {
          return svg_value_ref (svg_element_get_current_value (shape, attr));
        }
      else if (svg_value_is_initial (value))
        {
          return svg_property_ref_initial_value (attr, svg_element_get_type (shape), parent != NULL);
        }
      else
        {
          return svg_value_ref (value);
        }
    }
  else if (FIRST_FILTER_PROPERTY <= attr && attr <= LAST_FILTER_PROPERTY)
    {
      SvgFilter *f;
      SvgValue *value;

      g_assert (svg_element_type_is_filter (svg_element_get_type (shape)));
      g_assert (idx <= shape->filters->len);

      f = g_ptr_array_index (shape->filters, idx - 1);

      value = svg_filter_get_base_value (f, attr);
      if (svg_value_is_unset (value))
        {
          if (svg_property_inherited (attr))
            return svg_value_ref (shape_get_current_value (shape, attr, 0));
          else
            return svg_property_ref_initial_value (attr, svg_element_get_type (shape), parent != NULL);
        }
      else if (svg_value_is_inherit (value))
        {
          return svg_value_ref (shape_get_current_value (shape, attr, 0));
        }
      else if (svg_value_is_initial (value))
        {
          return svg_property_ref_initial_value (attr, svg_element_get_type (shape), parent != NULL);
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
shape_set_base_value (SvgElement   *shape,
                      SvgProperty   attr,
                      unsigned int  idx,
                      SvgValue     *value)
{
  if (idx == 0)
    {
      svg_element_set_base_value (shape, attr, value);
    }
  else if (FIRST_STOP_PROPERTY <= attr && attr <= LAST_STOP_PROPERTY)
    {
      SvgColorStop *stop;

      g_assert (svg_element_type_is_gradient (svg_element_get_type (shape)));
      g_assert (idx <= shape->color_stops->len);

      stop = g_ptr_array_index (shape->color_stops, idx - 1);
      svg_color_stop_set_base_value (stop, attr, value);
    }
  else if (FIRST_FILTER_PROPERTY <= attr && attr <= LAST_FILTER_PROPERTY)
    {
      SvgFilter *f;

      g_assert (svg_element_type_is_filter (svg_element_get_type (shape)));
      g_assert (idx <= shape->filters->len);

      f = g_ptr_array_index (shape->filters, idx - 1);
      svg_filter_set_base_value (f, attr, value);
    }
  else
    g_assert_not_reached ();
}

static void
shape_set_current_value (SvgElement   *shape,
                         SvgProperty   attr,
                         unsigned int  idx,
                         SvgValue     *value)
{
  if (idx == 0)
    {
      svg_element_set_current_value (shape, attr, value);
    }
  else if (FIRST_STOP_PROPERTY <= attr && attr <= LAST_STOP_PROPERTY)
    {
      SvgColorStop *stop;

      g_assert (svg_element_type_is_gradient (svg_element_get_type (shape)));

      stop = g_ptr_array_index (shape->color_stops, idx - 1);
      svg_color_stop_set_current_value (stop, attr, value);
    }
  else if (FIRST_FILTER_PROPERTY <= attr && attr <= LAST_FILTER_PROPERTY)
    {
      SvgFilter *f;

      g_assert (svg_element_type_is_filter (svg_element_get_type (shape)));

      f = g_ptr_array_index (shape->filters, idx - 1);
      svg_filter_set_current_value (f, attr, value);
    }
  else
    g_assert_not_reached ();
}

/* }}} */
/* {{{ Update computation */

static int64_t
determine_repeat_duration (SvgAnimation *a)
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
determine_simple_duration (SvgAnimation *a)
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
find_current_cycle_and_frame (SvgAnimation *a,
                              GtkSvg       *svg,
                              int64_t       time,
                              int          *out_rep,
                              unsigned int *out_frame,
                              double       *out_frame_t,
                              int64_t      *out_frame_start,
                              int64_t      *out_frame_end)
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
animation_update_run_mode (SvgAnimation *a,
                           int64_t       current_time)
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
      else if (svg_property_discrete (a->attr))
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
animation_set_current_end (SvgAnimation *a,
                           int64_t       time)
{
  /* FIXME take min, max into account */
  if (time < a->current.begin)
    time = a->current.begin;

  if (a->current.begin < INDEFINITE && a->repeat_duration < INDEFINITE)
    time = MIN (time, a->current.begin + a->repeat_duration);

  if (a->current.end == time)
    return FALSE;

  dbg_print ("times", "current end time of %s set to %s", a->id, format_time (time));
  a->current.end = time;
  return TRUE;
}

static void
animation_update_state (SvgAnimation *a,
                        int64_t       current_time)
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
time_spec_update_for_base (TimeSpec     *spec,
                           SvgAnimation *base)
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
time_specs_update_for_base (GPtrArray    *specs,
                            SvgAnimation *base)
{
  for (unsigned int i = 0; i < specs->len; i++)
    {
      TimeSpec *spec = g_ptr_array_index (specs, i);
      time_spec_update_for_base (spec, base);
    }
}

static gboolean
animation_can_start (SvgAnimation *a)
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
animation_update_for_spec (SvgAnimation *a,
                           TimeSpec     *spec)
{
  gboolean changed = FALSE;

  if (svg_animation_has_begin (a, spec))
    {
      if (!animation_can_start (a))
        return;

      if (a->status == ANIMATION_STATUS_RUNNING)
        {
          if (a->current.begin < spec->time && spec->time < INDEFINITE)
            {
              dbg_print ("status", "Restarting %s at %s", a->id, format_time (spec->time));
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
              dbg_print ("times", "current start time of %s now %s", a->id, format_time (time));
              a->current.begin = time;
              changed = TRUE;

              animation_set_current_end (a, a->current.end);
            }
        }
    }

  if (svg_animation_has_end (a, spec))
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
          SvgAnimation *dep = g_ptr_array_index (a->deps, i);
          time_specs_update_for_base (dep->begin, a);
          time_specs_update_for_base (dep->end, a);
        }
    }
}

static void
animation_set_begin (SvgAnimation *a,
                     int64_t       current_time)
{
  gboolean changed = FALSE;

  if (!animation_can_start (a))
    return;

  if (a->status == ANIMATION_STATUS_RUNNING)
    {
      if (a->current.begin < current_time)
        {
          dbg_print ("status", "Restarting %s at %s", a->id, format_time (current_time));
          a->current.begin = current_time;
          changed = TRUE;
        }
    }
  else
    {
      if (a->current.begin != current_time)
        {
          dbg_print ("times", "current start time of %s now %s", a->id, format_time (current_time));
          a->current.begin = current_time;
          changed = TRUE;

          animation_set_current_end (a, a->current.end);
        }
    }

  if (!changed)
    return;

  if (a->deps)
    {
      for (unsigned int i = 0; i < a->deps->len; i++)
        {
          SvgAnimation *dep = g_ptr_array_index (a->deps, i);
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
  int64_t current_time;

  self->pending_advance = 0;

  current_time = self->current_time + 1;
  if (self->clock)
    current_time = MAX (current_time, gdk_frame_clock_get_frame_time (self->clock));

  gtk_svg_advance (self, current_time);
}

static void
schedule_next_update (GtkSvg *self)
{
  if (!self->playing)
    return;

  g_clear_handle_id (&self->pending_advance, g_source_remove);

  if (self->run_mode == GTK_SVG_RUN_MODE_CONTINUOUS)
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
      dbg_print ("times", "next update NOW (%s)", format_time (self->current_time));
      self->advance_after_snapshot = TRUE;
      return;
    }

  if (self->next_update < INDEFINITE)
    {
      int64_t interval = (self->next_update - self->current_time) / (double) G_TIME_SPAN_MILLISECOND;

      dbg_print ("times", "next update in %" G_GINT64_FORMAT "ms", interval);
      self->pending_advance = g_timeout_add_once (interval, advance_later, self);
    }
  else
    {
      dbg_print ("times", "next update NEVER");
    }
}

/* }}} */
/* {{{ Frame-clock driven updates */

static void
frame_clock_update (GdkFrameClock *clock,
                    GtkSvg        *self)
{
  int64_t time = gdk_frame_clock_get_frame_time (self->clock);
  dbg_print ("clock", "clock update, advancing to %s", format_time (time));
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
resolve_value (SvgElement        *shape,
               SvgComputeContext *context,
               SvgProperty        attr,
               unsigned int       idx,
               SvgValue          *value)
{
  if (svg_value_is_initial (value))
    {
      SvgValue *v, *ret;

      if (idx > 0 && svg_element_get_type (shape) == SVG_ELEMENT_FILTER)
        v = svg_filter_ref_initial_value (g_ptr_array_index (shape->filters, idx - 1), attr);
      else
        v = svg_property_ref_initial_value (attr, svg_element_get_type (shape), context->parent != NULL);
      ret = svg_value_resolve (v, attr, idx, shape, context);
      svg_value_unref (v);
      return ret;
    }
  else if (svg_value_is_inherit (value))
    {
      if (idx > 0)
        return svg_value_ref (svg_element_get_current_value (shape, attr));
      else if (context->parent && svg_property_applies_to (attr, svg_element_get_type (context->parent)))
        return svg_value_ref (svg_element_get_current_value (context->parent, attr));
      else
        {
          SvgValue *v, *ret;

          v = svg_property_ref_initial_value (attr, svg_element_get_type (shape), context->parent != NULL);
          ret = svg_value_resolve (v, attr, idx, shape, context);
          svg_value_unref (v);
          return ret;
        }
    }
  else if (svg_value_is_current (value))
    {
      if (idx > 0)
        {
          if (svg_element_get_type (shape) == SVG_ELEMENT_FILTER)
            {
              SvgFilter *f = g_ptr_array_index (shape->filters, idx - 1);

              return svg_value_ref (svg_filter_get_current_value (f, attr));
            }
          else
            {
              SvgColorStop *stop = g_ptr_array_index (shape->color_stops, idx - 1);
              return svg_value_ref (svg_color_stop_get_current_value (stop, attr));
            }
        }
      else
        return svg_value_ref (svg_element_get_current_value (shape, attr));
    }
  else if (svg_value_is_auto (value))
    {
      if (attr == SVG_PROPERTY_WIDTH || attr == SVG_PROPERTY_HEIGHT)
        {
          if (svg_element_get_type (shape) == SVG_ELEMENT_SVG)
            {
              return svg_value_resolve (svg_percentage_new (100), attr, idx, shape, context);
            }
          else if (svg_element_get_type (shape) == SVG_ELEMENT_IMAGE)
            {
              GdkTexture *texture = svg_href_get_texture (svg_element_get_current_value (shape, SVG_PROPERTY_HREF));

              if (!texture)
                return svg_number_new (0);
              else if (attr == SVG_PROPERTY_WIDTH)
                return svg_number_new (gdk_texture_get_width (texture));
              else
                return svg_number_new (gdk_texture_get_height (texture));
            }
          else
            {
              return svg_number_new (0);
            }
        }
      else if (attr == SVG_PROPERTY_RX || attr == SVG_PROPERTY_RY)
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
compute_animation_motion_value (SvgAnimation      *a,
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
      measure = svg_element_get_current_measure (a->shape, context->viewport);
      get_transform_data_for_motion (measure, a->gpa.origin, ROTATE_FIXED, &angle, &orig_pos);
      gsk_path_measure_unref (measure);
    }

  angle = a->motion.angle;

  measure = svg_animation_motion_get_current_measure (a, context->viewport);
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
compute_value_at_time (SvgAnimation      *a,
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

  if (a->calc_mode == CALC_MODE_DISCRETE || svg_property_discrete (a->attr))
    {
      if (a->calc_mode != CALC_MODE_DISCRETE && frame_t > 0.5)
        frame = frame + 1;

      if (a->attr != SVG_PROPERTY_TRANSFORM || a->type != ANIMATION_TYPE_MOTION)
        return resolve_value (a->shape, context, a->attr, a->idx, a->frames[frame].value);
      else
        return compute_animation_motion_value (a, rep, frame, 0, context);
    }

  /* Now we are doing non-discrete linear or spline interpolation */
  if (a->calc_mode == CALC_MODE_SPLINE)
    frame_t = ease (a->frames[frame].params, frame_t);

  if (a->attr != SVG_PROPERTY_TRANSFORM || a->type != ANIMATION_TYPE_MOTION)
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
                                svg_property_get_presentation (a->attr, svg_element_get_type (a->shape)),
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
                                    svg_property_get_presentation (a->attr, svg_element_get_type (a->shape)),
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
compute_value_for_animation (SvgAnimation      *a,
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
      dbg_print ("values", "%s: too early", a->id);
    }
  else if (a->status == ANIMATION_STATUS_RUNNING)
    {
      /* animation is active */
      dbg_print ("values", "%s: updating value", a->id);
      value = compute_value_at_time (a, context);
    }
  else if (a->fill == ANIMATION_FILL_FREEZE)
    {
      /* keep the last value */
      if (a->repeat_count == 1)
        {
          if (!(a->attr == SVG_PROPERTY_TRANSFORM && a->type == ANIMATION_TYPE_MOTION))
            {
              dbg_print ("values", "%s: frozen (fast)", a->id);
              value = resolve_value (a->shape, context, a->attr, a->idx, a->frames[a->n_frames - 1].value);
            }
          else
           {
              dbg_print ("values", "%s: frozen (motion)", a->id);
              value = compute_animation_motion_value (a, 1, a->n_frames - 1, 0, context);
           }
        }
      else
        {
          dbg_print ("values", "%s: frozen", a->id);
          value = compute_value_at_time (a, context);
        }
    }
  else
    {
      /* Back to base value */
      dbg_print ("values", "%s: back to base", a->id);
    }

  context->interpolation = previous;

  return value;
}

static int64_t
get_last_start (SvgAnimation *a)
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
  SvgAnimation *a1 = (SvgAnimation *) a;
  SvgAnimation *a2 = (SvgAnimation *) b;
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
  if (a1->attr == SVG_PROPERTY_TRANSFORM)
    {
      if (a1->type == ANIMATION_TYPE_MOTION && a2->type != ANIMATION_TYPE_MOTION)
        return 1;
      else if (a1->type != ANIMATION_TYPE_MOTION && a2->type == ANIMATION_TYPE_MOTION)
        return -1;
    }

  /* SvgAnimation priorities:
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
shape_init_current_values (SvgElement        *shape,
                           SvgComputeContext *context)
{
  for (SvgProperty attr = FIRST_SVG_PROPERTY; attr <= LAST_SVG_PROPERTY; attr++)
    {
      if (svg_property_applies_to (attr, svg_element_get_type (shape)))
        {
          SvgValue *base;
          SvgValue *value;

          base = shape_ref_base_value (shape, context->parent, attr, 0);
          value = resolve_value (shape, context, attr, 0, base);
          svg_element_set_current_value (shape, attr, value);
          svg_value_unref (value);
          svg_value_unref (base);
        }
    }

  if (svg_element_type_is_gradient (svg_element_get_type (shape)))
    {
      for (unsigned int idx = 0; idx < shape->color_stops->len; idx++)
        {
          SvgColorStop *stop = g_ptr_array_index (shape->color_stops, idx);

          for (SvgProperty attr = FIRST_STOP_PROPERTY; attr <= LAST_STOP_PROPERTY; attr++)
            {
              SvgValue *base;
              SvgValue *value;

              base = shape_ref_base_value (shape, context->parent, attr, idx + 1);
              value = resolve_value (shape, context, attr, idx + 1, base);
              svg_color_stop_set_current_value (stop, attr, value);
              svg_value_unref (value);
              svg_value_unref (base);
            }
        }
    }

  if (svg_element_type_is_filter (svg_element_get_type (shape)))
    {
      for (unsigned int idx = 0; idx < shape->filters->len; idx++)
        {
          SvgFilter *f = g_ptr_array_index (shape->filters, idx);
          SvgFilterType type = svg_filter_get_type (f);

          for (unsigned int i = 0; i < svg_filter_type_get_n_attrs (type); i++)
            {
              SvgValue *base;
              SvgValue *value;
              SvgProperty attr;

              attr = svg_filter_type_get_property (type, i);
              base = shape_ref_base_value (shape, context->parent, attr, idx + 1);
              value = resolve_value (shape, context, attr, idx + 1, base);
              svg_filter_set_current_value (f, attr, value);
              svg_value_unref (value);
              svg_value_unref (base);
            }
        }
    }
}

static void
apply_view (SvgElement *content,
            SvgElement *view)
{
  SvgValue *vb;
  SvgValue *cf;

  if (!view)
    view = content;

  vb = svg_element_get_specified_value (view, SVG_PROPERTY_VIEW_BOX);
  cf = svg_element_get_specified_value (view, SVG_PROPERTY_CONTENT_FIT);

  if (!vb)
    vb = svg_element_get_specified_value (content, SVG_PROPERTY_VIEW_BOX);
  if (!cf)
    cf = svg_element_get_specified_value (content, SVG_PROPERTY_CONTENT_FIT);

  svg_element_set_base_value (content, SVG_PROPERTY_VIEW_BOX, vb);
  svg_element_set_base_value (content, SVG_PROPERTY_CONTENT_FIT, cf);
}

static void
compute_current_values_for_shape (SvgElement        *shape,
                                  SvgComputeContext *context)
{
  const graphene_rect_t *old_viewport = context->viewport;
  graphene_rect_t viewport;

  shape_init_current_values (shape, context);

  if (svg_element_get_type (shape) == SVG_ELEMENT_SVG || svg_element_get_type (shape) == SVG_ELEMENT_SYMBOL)
    {
      SvgValue *vb = svg_element_get_current_value (shape, SVG_PROPERTY_VIEW_BOX);
      double width, height;

      if (svg_element_get_parent (shape) == NULL)
        {
          width = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_WIDTH), context->svg->current_width);
          height = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_HEIGHT), context->svg->current_height);

        }
      else
        {
          // FIXME use parent override for symbol
          g_assert (context->viewport != NULL);

          width = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_WIDTH), context->viewport->size.width);
          height = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_HEIGHT), context->viewport->size.height);
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
          SvgAnimation *a = g_ptr_array_index (shape->animations, i);
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

          combined = svg_value_accumulate (shape_get_current_value (shape, SVG_PROPERTY_TRANSFORM, 0), motion, context, 1);
          svg_element_set_current_value (shape, SVG_PROPERTY_TRANSFORM, combined);
          svg_value_unref (combined);
        }
      svg_value_unref (identity);
      svg_value_unref (motion);
    }

  if (svg_element_get_type (shape) == SVG_ELEMENT_USE)
    svg_element_ensure_shadow_tree (shape, context->svg);

  if (shape->shapes)
    {
      SvgElement *parent = context->parent;
      context->parent = shape;
      for (SvgElement *sh = shape->first; sh; sh = sh->next)
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
                   SvgElement   *shape,
                   unsigned int  state)
{
  if (svg_element_type_is_path (svg_element_get_type (shape)))
    {
      Visibility visibility;

      if (svg_element_get_states (shape) & BIT (state))
        visibility = VISIBILITY_VISIBLE;
      else
        visibility = VISIBILITY_HIDDEN;

      if ((self->features & GTK_SVG_ANIMATIONS) == 0)
        {
          SvgValue *value = svg_visibility_new (visibility);
          svg_element_set_base_value (shape, SVG_PROPERTY_VISIBILITY, value);
          svg_value_unref (value);
        }

      if (!self->playing && shape->animations)
        {
          for (unsigned int i = shape->animations->len; i > 0; i--)
            {
              SvgAnimation *a = g_ptr_array_index (shape->animations, i - 1);

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

  if (svg_element_type_is_container (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          SvgElement *sh = g_ptr_array_index (shape->shapes, i);
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
  GtkCssParser *parser = parser_new_for_string (value);
  unsigned int i;
  gboolean retval = TRUE;

  for (i = 0; i < 3; i++)
    {
      gtk_css_parser_skip_whitespace (parser);
      values[i] = svg_number_parse (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER|SVG_PARSE_LENGTH|SVG_PARSE_PERCENTAGE);
      if (!values[i])
        retval = FALSE;
    }

  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    retval = FALSE;

  gtk_css_parser_unref (parser);

  return retval;
}

/* {{{ States */

static void
create_visibility_setter (SvgElement   *shape,
                          Timeline     *timeline,
                          uint64_t      states,
                          int64_t       delay,
                          unsigned int  initial_state)
{
  SvgAnimation *a = svg_animation_new (ANIMATION_TYPE_SET);
  TimeSpec *begin, *end;
  Visibility initial_visibility;
  Visibility opposite_visibility;

  if (svg_element_is_specified (shape, SVG_PROPERTY_VISIBILITY))
    initial_visibility = svg_enum_get (svg_element_get_specified_value (shape, SVG_PROPERTY_VISIBILITY));
  else
    initial_visibility = VISIBILITY_VISIBLE;

  a->attr = SVG_PROPERTY_VISIBILITY;

  if (initial_visibility == VISIBILITY_VISIBLE)
    {
      a->id = g_strdup_printf ("gpa:out-of-state:%s", svg_element_get_id (shape));
      begin = svg_animation_add_begin (a, timeline_get_states (timeline, states, ALL_STATES & ~states, MAX (0, - delay)));
      time_spec_add_animation (begin, a);

      end = svg_animation_add_end (a, timeline_get_states (timeline, ALL_STATES & ~states, states, - (MAX (0, - delay))));
      time_spec_add_animation (end, a);

      opposite_visibility = VISIBILITY_HIDDEN;
    }
  else
    {
      a->id = g_strdup_printf ("gpa:in-state:%s", svg_element_get_id (shape));
      begin = svg_animation_add_begin (a, timeline_get_states (timeline, ALL_STATES & ~states, states, MAX (0, - delay)));
      time_spec_add_animation (begin, a);

      end = svg_animation_add_end (a, timeline_get_states (timeline, states, ALL_STATES & ~states, - (MAX (0, - delay))));
      time_spec_add_animation (end, a);

      opposite_visibility = VISIBILITY_VISIBLE;
    }

  if (state_match (states, initial_state) != (initial_visibility == VISIBILITY_VISIBLE))
    {
      begin = svg_animation_add_begin (a, timeline_get_start_of_time (timeline));
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
  a->shape = shape;

  svg_element_add_animation (shape, a);
}

static void
create_states (SvgElement   *shape,
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
create_path_length (SvgElement *shape,
                    Timeline   *timeline)
{
  SvgAnimation *a = svg_animation_new (ANIMATION_TYPE_SET);
  TimeSpec *begin, *end;

  a->attr = SVG_PROPERTY_PATH_LENGTH;

  a->id = g_strdup_printf ("gpa:path-length:%s", svg_element_get_id (shape));
  begin = svg_animation_add_begin (a, timeline_get_start_of_time (timeline));
  end = svg_animation_add_end (a, timeline_get_end_of_time (timeline));
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
  a->shape = shape;

  svg_element_add_animation (shape, a);
}

static void
create_transition (SvgElement    *shape,
                   unsigned int   idx,
                   Timeline      *timeline,
                   uint64_t       states,
                   int64_t        duration,
                   int64_t        delay,
                   GpaEasing      easing,
                   double         origin,
                   GpaTransition  type,
                   SvgProperty    attr,
                   SvgValue      *from,
                   SvgValue      *to)
{
  SvgAnimation *a;
  TimeSpec *begin;

  a = svg_animation_new (ANIMATION_TYPE_ANIMATE);
  a->idx = idx;
  a->simple_duration = duration;
  a->repeat_duration = duration;
  a->repeat_count = 1;

  a->has_begin = 1;
  a->has_simple_duration = 1;
  a->has_repeat_duration = 1;

  a->id = g_strdup_printf ("gpa:transition:fade-in:%u:%s:%s", idx, svg_property_get_name (attr), svg_element_get_id (shape));

  begin = svg_animation_add_begin (a, timeline_get_states (timeline, ALL_STATES & ~states, states, delay));

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
  a->shape = shape;

  svg_element_add_animation (shape, a);
  time_spec_add_animation (begin, a);

  a->gpa.transition = type;
  a->gpa.easing = easing;
  a->gpa.origin = origin;

  a = svg_animation_new (ANIMATION_TYPE_ANIMATE);
  a->idx = idx;
  a->simple_duration = duration;
  a->repeat_duration = duration;
  a->repeat_count = 1;

  a->has_begin = 1;
  a->has_simple_duration = 1;
  a->has_repeat_duration = 1;

  a->id = g_strdup_printf ("gpa:transition:fade-out:%u:%s:%s", idx, svg_property_get_name (attr), svg_element_get_id (shape));

  begin = svg_animation_add_begin (a, timeline_get_states (timeline, states, ALL_STATES & ~states, - (duration + delay)));

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
  a->shape = shape;

  svg_element_add_animation (shape, a);
  time_spec_add_animation (begin, a);

  a->gpa.transition = type;
  a->gpa.easing = easing;
  a->gpa.origin = origin;

  if (delay > 0)
    {
      a = svg_animation_new (ANIMATION_TYPE_SET);
      a->idx = idx;
      a->attr = attr;
      a->simple_duration = duration;
      a->repeat_duration = duration;
      a->repeat_count = 1;

      a->id = g_strdup_printf ("gpa:transition:delay-in:%u:%s:%s", idx, svg_property_get_name (attr), svg_element_get_id (shape));
      begin = svg_animation_add_begin (a, timeline_get_states (timeline, ALL_STATES & ~states, states, 0));
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
      a->shape = shape;

      svg_element_add_animation (shape, a);

      a = svg_animation_new (ANIMATION_TYPE_SET);
      a->idx = idx;
      a->attr = attr;
      a->simple_duration = duration;
      a->repeat_duration = duration;
      a->repeat_count = 1;

      a->id = g_strdup_printf ("gpa:transition:delay-out:%u:%s:%s", idx, svg_property_get_name (attr), svg_element_get_id (shape));
      begin = svg_animation_add_begin (a, timeline_get_states (timeline, states, ALL_STATES & ~states, 0));
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
      a->shape = shape;

      svg_element_add_animation (shape, a);
    }
}

static void
create_transition_delay (SvgElement  *shape,
                         Timeline    *timeline,
                         uint64_t     states,
                         int64_t      delay,
                         SvgProperty  attr,
                         SvgValue    *value)
{
  SvgAnimation *a;
  TimeSpec *begin;

  a = svg_animation_new (ANIMATION_TYPE_SET);
  a->simple_duration = delay;
  a->repeat_duration = delay;
  a->repeat_count = 1;

  a->has_begin = 1;
  a->has_simple_duration = 1;
  a->has_repeat_duration = 1;

  a->id = g_strdup_printf ("gpa:transition:fade-in-delay:%s:%s", svg_property_get_name (attr), svg_element_get_id (shape));

  begin = svg_animation_add_begin (a, timeline_get_states (timeline, ALL_STATES & ~states, states, 0));

  a->attr = attr;
  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  a->frames[0].value = svg_value_ref (value);
  a->frames[1].value = svg_value_ref (value);

  a->fill = ANIMATION_FILL_REMOVE;
  a->shape = shape;

  svg_element_add_animation (shape, a);
  time_spec_add_animation (begin, a);

  a = svg_animation_new (ANIMATION_TYPE_SET);
  a->simple_duration = delay;
  a->repeat_duration = delay;
  a->repeat_count = 1;

  a->has_begin = 1;
  a->has_simple_duration = 1;
  a->has_repeat_duration = 1;

  a->id = g_strdup_printf ("gpa:transition:fade-out-delay:%s:%s", svg_property_get_name (attr), svg_element_get_id (shape));

  begin = svg_animation_add_begin (a, timeline_get_states (timeline, states, ALL_STATES & ~states, -delay));

  a->attr = attr;
  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  a->frames[0].value = svg_value_ref (value);
  a->frames[1].value = svg_value_ref (value);

  a->fill = ANIMATION_FILL_REMOVE;
  a->shape = shape;

  svg_element_add_animation (shape, a);
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
create_morph_filter (SvgElement *shape,
                     Timeline   *timeline,
                     GHashTable *shapes,
                     uint64_t    states,
                     int64_t     duration,
                     int64_t     delay,
                     GpaEasing   easing)
{
  SvgElement *parent = NULL;
  SvgElement *filter;
  SvgFilter *f;
  SvgValue *value;
  SvgAnimation *a;
  char *str;
  TimeSpec *begin;
  TimeSpec *end;

  for (unsigned int i = 0; i < svg_element_get_parent (shape)->shapes->len; i++)
    {
      SvgElement *sh = g_ptr_array_index (svg_element_get_parent (shape)->shapes, i);

      if (sh == shape)
        break;

      if (sh->type == SVG_ELEMENT_DEFS)
        {
          parent = sh;
          break;
        }
    }

  if (parent == NULL)
    {
      parent = svg_element_new (svg_element_get_parent (shape), SVG_ELEMENT_DEFS);
      g_ptr_array_insert (svg_element_get_parent (shape)->shapes, 0, parent);
    }

  filter = svg_element_new (parent, SVG_ELEMENT_FILTER);
  svg_element_add_child (parent, filter);
  filter->id = g_strdup_printf ("gpa:morph-filter:%s", svg_element_get_id (shape));

  g_hash_table_insert (shapes, filter->id, filter);

  value = svg_percentage_new (-50);
  svg_element_set_specified_value (filter, SVG_PROPERTY_X, value);
  svg_element_set_specified_value (filter, SVG_PROPERTY_Y, value);
  svg_value_unref (value);
  value = svg_percentage_new (200);
  svg_element_set_specified_value (filter, SVG_PROPERTY_WIDTH, value);
  svg_element_set_specified_value (filter, SVG_PROPERTY_HEIGHT, value);
  svg_value_unref (value);

  f = svg_filter_new (filter, SVG_FILTER_BLUR);
  svg_element_add_filter (filter, f);
  str = g_strdup_printf ("gpa:morph-filter:%s:blurred", svg_element_get_id (shape));
  svg_filter_take_specified_value (f, SVG_PROPERTY_FE_RESULT, svg_string_new_take (str));

  create_transition (filter, filter->filters->len, timeline, states,
                     duration, delay, easing,
                     0, GPA_TRANSITION_MORPH,
                     SVG_PROPERTY_FE_STD_DEV,
                     svg_numbers_new1 (4),
                     svg_numbers_new1 (0));

  f = svg_filter_new (filter, SVG_FILTER_COMPONENT_TRANSFER);
  svg_element_add_filter (filter, f);
  svg_filter_take_specified_value (f, SVG_PROPERTY_FE_IN, svg_filter_ref_new (FILTER_REF_SOURCE_GRAPHIC));

  f = svg_filter_new (filter, SVG_FILTER_FUNC_A);
  svg_element_add_filter (filter, f);
  svg_filter_take_specified_value (f, SVG_PROPERTY_FE_FUNC_TYPE, svg_component_transfer_type_new (COMPONENT_TRANSFER_LINEAR));
  svg_filter_take_specified_value (f, SVG_PROPERTY_FE_FUNC_SLOPE, svg_number_new (100));
  svg_filter_take_specified_value (f, SVG_PROPERTY_FE_FUNC_INTERCEPT, svg_number_new (0));

  f = svg_filter_new (filter, SVG_FILTER_BLUR);
  svg_element_add_filter (filter, f);

  create_transition (filter, filter->filters->len, timeline, states,
                     duration, delay, easing,
                     0, GPA_TRANSITION_MORPH,
                     SVG_PROPERTY_FE_STD_DEV,
                     svg_numbers_new1 (4),
                     svg_numbers_new1 (0));

  f = svg_filter_new (filter, SVG_FILTER_COMPONENT_TRANSFER);
  svg_element_add_filter (filter, f);

  for (unsigned int func = SVG_FILTER_FUNC_R; func <= SVG_FILTER_FUNC_B; func++)
    {
      f = svg_filter_new (filter, func);
      svg_element_add_filter (filter, f);
      svg_filter_take_specified_value (f, SVG_PROPERTY_FE_FUNC_TYPE, svg_component_transfer_type_new (COMPONENT_TRANSFER_LINEAR));
      svg_filter_take_specified_value (f, SVG_PROPERTY_FE_FUNC_SLOPE, svg_number_new (0));
      svg_filter_take_specified_value (f, SVG_PROPERTY_FE_FUNC_INTERCEPT, svg_number_new (1));
    }

  f = svg_filter_new (filter, SVG_FILTER_FUNC_A);
  svg_element_add_filter (filter, f);
  svg_filter_set_specified_value (f, SVG_PROPERTY_FE_FUNC_TYPE, svg_component_transfer_type_new (COMPONENT_TRANSFER_LINEAR));

  create_transition (filter, filter->filters->len, timeline, states,
                     duration, delay, easing,
                     0, GPA_TRANSITION_MORPH,
                     SVG_PROPERTY_FE_FUNC_SLOPE,
                     svg_number_new (100),
                     svg_number_new (1));

  create_transition (filter, filter->filters->len, timeline, states,
                     duration, delay, easing,
                     0, GPA_TRANSITION_MORPH,
                     SVG_PROPERTY_FE_FUNC_INTERCEPT,
                     svg_number_new (-20),
                     svg_number_new (0));

  f = svg_filter_new (filter, SVG_FILTER_COMPOSITE);
  svg_element_add_filter (filter, f);
  svg_filter_take_specified_value (f, SVG_PROPERTY_FE_COMPOSITE_OPERATOR, svg_composite_operator_new (COMPOSITE_OPERATOR_ARITHMETIC));
  svg_filter_take_specified_value (f, SVG_PROPERTY_FE_COMPOSITE_K1, svg_number_new (1));
  str = g_strdup_printf ("gpa:morph-filter:%s:blurred", svg_element_get_id (shape));
  svg_filter_take_specified_value (f, SVG_PROPERTY_FE_IN2, svg_filter_ref_new_ref (str));
  g_free (str);

  a = svg_animation_new (ANIMATION_TYPE_SET);
  a->id = g_strdup_printf ("gpa:set:morph:%s", svg_element_get_id (shape));
  a->attr = SVG_PROPERTY_FILTER;

  begin = svg_animation_add_begin (a, timeline_get_start_of_time (timeline));
  time_spec_add_animation (begin, a);
  end = svg_animation_add_end (a, timeline_get_end_of_time (timeline));
  time_spec_add_animation (end, a);

  a->has_begin = 1;
  a->has_end = 1;

  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  str = g_strdup_printf ("url(#%s)", filter->id);
  a->frames[0].value = svg_filter_functions_parse (str);
  a->frames[1].value = svg_value_ref (a->frames[0].value);
  g_free (str);
  a->shape = shape;

  svg_element_add_animation (shape, a);
}

static void
create_transitions (SvgElement    *shape,
                    Timeline      *timeline,
                    GHashTable    *shapes,
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
                         SVG_PROPERTY_STROKE_DASHARRAY,
                         svg_dash_array_new ((double[]) { 0, 2 }, 2),
                         svg_dash_array_new ((double[]) { 1, 0 }, 2));
      if (delay != 0)
        create_transition_delay (shape, timeline, states, delay,
                                 SVG_PROPERTY_STROKE_DASHOFFSET,
                                 svg_number_new (0.5));
      if (!G_APPROX_VALUE (origin, 0, 0.001))
        create_transition (shape, 0, timeline, states,
                           duration, delay, easing,
                           origin, type,
                           SVG_PROPERTY_STROKE_DASHOFFSET,
                           svg_number_new (-origin),
                           svg_number_new (0));
      break;
    case GPA_TRANSITION_MORPH:
      create_morph_filter (shape, timeline, shapes, states,
                           duration, delay, easing);
      break;
    case GPA_TRANSITION_FADE:
      create_transition (shape, 0, timeline, states,
                         duration, delay, easing,
                         origin, type,
                         SVG_PROPERTY_OPACITY,
                         svg_number_new (0), svg_number_new (1));
      break;
    default:
      g_assert_not_reached ();
    }
}

/* }}} */
/* {{{ Animations */

static SvgAnimation *
create_animation (SvgElement   *shape,
                  Timeline     *timeline,
                  uint64_t      states,
                  unsigned int  initial,
                  double        repeat,
                  int64_t       duration,
                  CalcMode      calc_mode,
                  SvgProperty   attr,
                  GArray       *frames)
{
  SvgAnimation *a;
  TimeSpec *begin, *end;

  a = svg_animation_new (ANIMATION_TYPE_ANIMATE);
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

  a->id = g_strdup_printf ("gpa:animation:%s-%s", svg_element_get_id (shape), svg_property_get_name (attr));

  begin = svg_animation_add_begin (a, timeline_get_states (timeline, ALL_STATES & ~states, states, 0));
  time_spec_add_animation (begin, a);

  if (state_match (states, initial))
    {
      begin = svg_animation_add_begin (a, timeline_get_start_of_time (timeline));
      time_spec_add_animation (begin, a);
    }

  end = svg_animation_add_end (a, timeline_get_states (timeline, states, ALL_STATES & ~states, 0));
  time_spec_add_animation (end, a);

  a->attr = attr;
  a->n_frames = frames->len;
  a->frames = g_array_steal (frames, NULL);

  a->calc_mode = calc_mode;
  a->shape = shape;

  svg_element_add_animation (shape, a);

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
create_animations (SvgElement   *shape,
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
  SvgAnimation *a;
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
                        SVG_PROPERTY_STROKE_DASHARRAY,
                        array);

  a->gpa.animation = direction;
  a->gpa.easing = easing;
  a->gpa.origin = origin;
  a->gpa.segment = segment;

  if (offset->len > 0)
    a = create_animation (shape, timeline, states, initial,
                          repeat, repeat_duration, calc_mode,
                          SVG_PROPERTY_STROKE_DASHOFFSET,
                          offset);

  g_array_unref (array);
  g_array_unref (offset);
}

/* }}} */
/* {{{ Attachment */

static void
create_attachment (SvgElement *shape,
                   Timeline   *timeline,
                   uint64_t    states,
                   const char *attach_to,
                   double      attach_pos,
                   double      origin)
{
  SvgAnimation *a;
  GArray *frames;
  TimeSpec *begin, *end;

  a = svg_animation_new (ANIMATION_TYPE_MOTION);

  a->has_begin = 1;
  a->has_end = 1;
  a->has_simple_duration = 1;

  a->simple_duration = 1;

  a->id = g_strdup_printf ("gpa:attachment:%s", svg_element_get_id (shape));

  begin = svg_animation_add_begin (a, timeline_get_start_of_time (timeline));
  end = svg_animation_add_end (a, timeline_get_fixed (timeline, 1));

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
  a->shape = shape;

  svg_element_add_animation (shape, a);
  time_spec_add_animation (begin, a);
  time_spec_add_animation (end, a);
}

static void
create_attachment_connection_to (SvgAnimation *a,
                                 SvgAnimation *da,
                                 Timeline     *timeline)
{
  SvgAnimation *a2;
  GArray *frames;
  GpaAnimation direction;
  TimeSpec *begin, *end;

  a2 = svg_animation_new (ANIMATION_TYPE_MOTION);
  a2->simple_duration = da->simple_duration;
  a2->repeat_count = da->repeat_count;
  if (g_str_has_prefix (da->id, "gpa:animation:"))
    {
      a2->id = g_strdup_printf ("gpa:attachment-animation:%s", svg_element_get_id (a->shape));
      direction = da->gpa.animation;
    }
  else if (g_str_has_prefix (da->id, "gpa:transition:fade-in:"))
    {
      a2->id = g_strdup_printf ("gpa:attachment-transition:fade-in:%s", svg_element_get_id (a->shape));
      direction = GPA_ANIMATION_NORMAL;
    }
  else if (g_str_has_prefix (da->id, "gpa:transition:fade-out:"))
    {
      a2->id = g_strdup_printf ("gpa:attachment-transition:fade-out:%s", svg_element_get_id (a->shape));
      direction = GPA_ANIMATION_REVERSE;
    }
  else
    g_assert_not_reached ();

  a2->has_begin = 1;
  a2->has_end = 1;
  a2->has_simple_duration = 1;

  begin = svg_animation_add_begin (a2, timeline_get_sync (timeline, da->id, da, TIME_SPEC_SIDE_BEGIN, 0));
  end = svg_animation_add_end (a2, timeline_get_sync (timeline, da->id, da, TIME_SPEC_SIDE_END, 0));

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
  a2->shape = a->shape;

  svg_element_add_animation (a->shape, a2);
  time_spec_add_animation (begin, a2);
  time_spec_add_animation (end, a2);

  svg_animation_add_dep (da, a2);
}

static void
create_attachment_connection (SvgAnimation *a,
                              SvgElement   *sh,
                              Timeline     *timeline)
{
  if (sh->animations == NULL)
    return;

  for (unsigned int i = 0; i < sh->animations->len; i++)
    {
      SvgAnimation *sha = g_ptr_array_index (sh->animations, i);
      if (sha->id &&
          (g_str_has_prefix (sha->id, "gpa:animation:") ||
           g_str_has_prefix (sha->id, "gpa:transition:")))
        {
          if (sha->attr == SVG_PROPERTY_STROKE_DASHARRAY)
            create_attachment_connection_to (a, sha, timeline);
        }
    }
}

/* }}} */
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
 * This process is highly recursive. For example, obtaining
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
  struct {
    gboolean picking;
    graphene_point_t p;
    GSList *points;
    gboolean done;
    SvgElement *clipped;
    SvgElement *picked;
  } picking;
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
                SvgElement   *shape)
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
  if (context->picking.picking)
    {
      GskTransform *t;

      graphene_point_t *p = g_new (graphene_point_t, 1);
      graphene_point_init_from_point (p, &context->picking.p);
      context->picking.points = g_slist_prepend (context->picking.points, p);

      t = gsk_transform_invert (gsk_transform_ref (transform));
      gsk_transform_transform_point (t, p, &context->picking.p);
      gsk_transform_unref (t);
      //g_print ("%f %f -> %f %f\n", p->x, p->y, context->picking.p.x, context->picking.p.y);
    }

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

  if (context->picking.picking)
    {
      tos = context->picking.points;
      context->picking.points = tos->next;

      context->picking.p = *(graphene_point_t *) tos->data;
      g_free (tos->data);
      g_slist_free_1 (tos);
    }
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

static void push_group   (SvgElement   *shape,
                          PaintContext *context);
static void pop_group    (SvgElement   *shape,
                          PaintContext *context);
static void paint_shape  (SvgElement   *shape,
                          PaintContext *context);
static void render_shape (SvgElement   *shape,
                          PaintContext *context);
static void fill_shape   (SvgElement   *shape,
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
                   SvgElement            *shape,
                   PaintContext          *context,
                   GskRenderNode         *source,
                   GHashTable            *results)
{
  GskRenderNode *node;
  FilterResult *res;

  switch (svg_filter_ref_get_type (in))
    {
    case FILTER_REF_DEFAULT_SOURCE:
      return filter_result_ref (g_hash_table_lookup (results, ""));
    case FILTER_REF_SOURCE_GRAPHIC:
      return filter_result_new (source, NULL);
    case FILTER_REF_SOURCE_ALPHA:
      res = filter_result_new (source, NULL);
      res->node = apply_alpha_only (res->node);
      return res;
    case FILTER_REF_BACKGROUND_IMAGE:
      node = gsk_paste_node_new (subregion, 0);
      res = filter_result_new (node, subregion);
      gsk_render_node_unref (node);
      return res;
    case FILTER_REF_BACKGROUND_ALPHA:
      node = gsk_paste_node_new (subregion, 0);
      res = filter_result_new (node, subregion);
      gsk_render_node_unref (node);
      res->node = apply_alpha_only (res->node);
      return res;
    case FILTER_REF_BY_NAME:
      res = g_hash_table_lookup (results, svg_filter_ref_get_ref (in));
      if (res)
        return filter_result_ref (res);
      else
        return filter_result_ref (g_hash_table_lookup (results, ""));
    case FILTER_REF_FILL_PAINT:
    case FILTER_REF_STROKE_PAINT:
      {
        GskPath *path;
        GskPathBuilder *builder;
        SvgValue *paint;
        double opacity;

        if (svg_filter_ref_get_type (in))
          {
            paint = svg_element_get_current_value (shape, SVG_PROPERTY_FILL);
            opacity = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_FILL_OPACITY), 1);
          }
        else
          {
            paint = svg_element_get_current_value (shape, SVG_PROPERTY_STROKE);
            opacity = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_STROKE_OPACITY), 1);
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
      SvgFilterRefType type = svg_filter_ref_get_type (refs[i]);

      if (type == FILTER_REF_SOURCE_GRAPHIC || type == FILTER_REF_SOURCE_ALPHA ||
          type == FILTER_REF_BACKGROUND_IMAGE || type == FILTER_REF_BACKGROUND_ALPHA ||
          type == FILTER_REF_FILL_PAINT || type == FILTER_REF_STROKE_PAINT ||
          (type == FILTER_REF_DEFAULT_SOURCE && is_first))
        {
          if (i == 0)
            graphene_rect_init_from_rect (subregion, filter_region);
          else
            graphene_rect_union (filter_region, subregion, subregion);
        }
      else
        {
          FilterResult *res;

          if (type == FILTER_REF_DEFAULT_SOURCE)
            res = g_hash_table_lookup (results, "");
          else if (type == FILTER_REF_BY_NAME)
            res = g_hash_table_lookup (results, svg_filter_ref_get_ref (refs[i]));
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
determine_filter_subregion (SvgFilter             *f,
                            SvgElement            *filter,
                            unsigned int           idx,
                            const graphene_rect_t *bounds,
                            const graphene_rect_t *viewport,
                            const graphene_rect_t *filter_region,
                            GHashTable            *results,
                            graphene_rect_t       *subregion)
{
  SvgFilterType type = svg_filter_get_type (f);
  gboolean x_set, y_set, w_set, h_set;

  if (type == SVG_FILTER_MERGE_NODE ||
      type == SVG_FILTER_FUNC_R ||
      type == SVG_FILTER_FUNC_G ||
      type == SVG_FILTER_FUNC_B ||
      type == SVG_FILTER_FUNC_A)
    {
      g_error ("Can't get subregion for %s\n", svg_filter_type_get_name (type));
      return FALSE;
    }

  x_set = svg_filter_is_specified (f, SVG_PROPERTY_FE_X);
  y_set = svg_filter_is_specified (f, SVG_PROPERTY_FE_Y);
  w_set = svg_filter_is_specified (f, SVG_PROPERTY_FE_WIDTH);
  h_set = svg_filter_is_specified (f, SVG_PROPERTY_FE_HEIGHT);

  if (x_set || y_set || w_set || h_set)
    {
      graphene_rect_init_from_rect (subregion, filter_region);

      if (x_set)
        {
          SvgValue *n = svg_filter_get_current_value (f, SVG_PROPERTY_FE_X);
          if (svg_enum_get (filter->current[SVG_PROPERTY_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
            subregion->origin.x = bounds->origin.x + svg_number_get (n, 1) * bounds->size.width;
          else
            subregion->origin.x = viewport->origin.x + svg_number_get (n, viewport->size.width);
        }

      if (y_set)
        {
          SvgValue *n = svg_filter_get_current_value (f, SVG_PROPERTY_FE_Y);
          if (svg_enum_get (filter->current[SVG_PROPERTY_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
            subregion->origin.y = bounds->origin.y + svg_number_get (n, 1) * bounds->size.height;
          else
            subregion->origin.y = viewport->origin.y + svg_number_get (n, viewport->size.height);
        }

      if (w_set)
        {
          SvgValue *n = svg_filter_get_current_value (f, SVG_PROPERTY_FE_WIDTH);
          if (svg_enum_get (filter->current[SVG_PROPERTY_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
            subregion->size.width = svg_number_get (n, 1) * bounds->size.width;
          else
            subregion->size.width = svg_number_get (n, viewport->size.width);
        }

      if (h_set)
        {
          SvgValue *n = svg_filter_get_current_value (f, SVG_PROPERTY_FE_HEIGHT);
          if (svg_enum_get (filter->current[SVG_PROPERTY_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
            subregion->size.height = svg_number_get (n, 1) * bounds->size.height;
          else
            subregion->size.height = svg_number_get (n, viewport->size.height);
        }
    }
  else
    {
      gboolean is_first = idx == 0;

      switch (type)
        {
        case SVG_FILTER_FLOOD:
        case SVG_FILTER_IMAGE:
          /* zero inputs */
          graphene_rect_init_from_rect (subregion, filter_region);
          break;

        case SVG_FILTER_TILE:
        case SVG_FILTER_MERGE_NODE:
        case SVG_FILTER_FUNC_R:
        case SVG_FILTER_FUNC_G:
        case SVG_FILTER_FUNC_B:
        case SVG_FILTER_FUNC_A:
          /* special case */
          graphene_rect_init_from_rect (subregion, filter_region);
          break;

        case SVG_FILTER_BLUR:
        case SVG_FILTER_COLOR_MATRIX:
        case SVG_FILTER_COMPONENT_TRANSFER:
        case SVG_FILTER_DROPSHADOW:
        case SVG_FILTER_OFFSET:
          {
            SvgValue *ref = svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN);

            determine_filter_subregion_from_refs (&ref, 1, is_first, filter_region, results, subregion);
          }
          break;

        case SVG_FILTER_BLEND:
        case SVG_FILTER_COMPOSITE:
        case SVG_FILTER_DISPLACEMENT:
          {
            SvgValue *refs[2];

            refs[0] = svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN);
            refs[1] = svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN2);

            determine_filter_subregion_from_refs (refs, 2, is_first, filter_region, results, subregion);
          }
          break;

        case SVG_FILTER_MERGE:
          {
            SvgValue **refs;
            unsigned int n_refs;

            refs = g_newa (SvgValue *, filter->filters->len);
            n_refs = 0;
            for (idx++; idx < filter->filters->len; idx++)
              {
                SvgFilter *ff = g_ptr_array_index (filter->filters, idx);

                if (svg_filter_get_type (ff) != SVG_FILTER_MERGE_NODE)
                  break;

                refs[n_refs] = svg_filter_get_current_value (ff, SVG_PROPERTY_FE_IN);
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

static void recompute_current_values (SvgElement   *shape,
                                      SvgElement   *parent,
                                      PaintContext *context);

static GskRenderNode *
apply_filter_tree (SvgElement    *shape,
                   SvgElement    *filter,
                   PaintContext  *context,
                   GskRenderNode *source)
{
  graphene_rect_t filter_region;
  GHashTable *results;
  graphene_rect_t bounds, rect;
  GdkColorState *color_state;

  if (filter->filters->len == 0)
    return empty_node ();

  if (!svg_element_get_current_bounds (shape, context->viewport, context->svg, &bounds))
    return gsk_render_node_ref (source);

  if (svg_enum_get (filter->current[SVG_PROPERTY_BOUND_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
    {
      if (bounds.size.width == 0 || bounds.size.height == 0)
        return empty_node ();

      filter_region.origin.x = bounds.origin.x + svg_number_get (filter->current[SVG_PROPERTY_X], 1) * bounds.size.width;
      filter_region.origin.y = bounds.origin.y + svg_number_get (filter->current[SVG_PROPERTY_Y], 1) * bounds.size.height;
      filter_region.size.width = svg_number_get (filter->current[SVG_PROPERTY_WIDTH], 1) * bounds.size.width;
      filter_region.size.height = svg_number_get (filter->current[SVG_PROPERTY_HEIGHT], 1) * bounds.size.height;
    }
  else
    {
      filter_region.origin.x = svg_number_get (filter->current[SVG_PROPERTY_X], context->viewport->size.width);
      filter_region.origin.y = svg_number_get (filter->current[SVG_PROPERTY_Y], context->viewport->size.height);
      filter_region.size.width = svg_number_get (filter->current[SVG_PROPERTY_WIDTH], context->viewport->size.width);
      filter_region.size.height = svg_number_get (filter->current[SVG_PROPERTY_HEIGHT], context->viewport->size.height);
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

  if (svg_enum_get (filter->current[SVG_PROPERTY_COLOR_INTERPOLATION_FILTERS]) == COLOR_INTERPOLATION_LINEAR)
    color_state = GDK_COLOR_STATE_SRGB_LINEAR;
  else
    color_state = GDK_COLOR_STATE_SRGB;

  for (unsigned int i = 0; i < filter->filters->len; i++)
    {
      SvgFilter *f = g_ptr_array_index (filter->filters, i);
      graphene_rect_t subregion;
      GskRenderNode *result = NULL;

      if (!determine_filter_subregion (f, filter, i, &bounds, context->viewport, &filter_region, results, &subregion))
        {
          graphene_rect_init (&subregion, 0, 0, 0, 0);
          result = gsk_container_node_new (NULL, 0);

          /* Skip dependent filters */
          if (svg_filter_get_type (f) == SVG_FILTER_MERGE)
            {
              for (i++; i < filter->filters->len; i++)
                {
                  SvgFilter *ff = g_ptr_array_index (filter->filters, i);
                  if (svg_filter_get_type (ff) != SVG_FILTER_MERGE_NODE)
                    {
                      i--;
                      break;
                    }
                }
            }
          else if (svg_filter_get_type (f) == SVG_FILTER_COMPONENT_TRANSFER)
            {
              for (i++; i < filter->filters->len; i++)
                {
                  SvgFilter *ff = g_ptr_array_index (filter->filters, i);
                  SvgFilterType t = svg_filter_get_type (ff);
                  if (t != SVG_FILTER_FUNC_R &&
                      t != SVG_FILTER_FUNC_G &&
                      t != SVG_FILTER_FUNC_B &&
                      t != SVG_FILTER_FUNC_A)
                    {
                      i--;
                      break;
                    }
                }
            }

          goto got_result;
        }

      switch (svg_filter_get_type (f))
        {
        case SVG_FILTER_BLUR:
          {
            FilterResult *in;
            double std_dev;
            EdgeMode edge_mode;
            GskRenderNode *child;
            SvgValue *num;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);

            num = svg_filter_get_current_value (f, SVG_PROPERTY_FE_STD_DEV);
            if (svg_numbers_get_length (num) == 2 &&
                svg_numbers_get (num, 0, 1) != svg_numbers_get (num, 1, 1))
              gtk_svg_rendering_error (context->svg,
                                       "Separate x/y values for stdDeviation not supported");
            std_dev = svg_numbers_get (num, 0, 1);

            edge_mode = svg_enum_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_BLUR_EDGE_MODE));

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

        case SVG_FILTER_FLOOD:
          {
            SvgValue *color = svg_filter_get_current_value (f, SVG_PROPERTY_FE_COLOR);
            SvgValue *alpha = svg_filter_get_current_value (f, SVG_PROPERTY_FE_OPACITY);
            GdkColor c;

            gdk_color_init_copy (&c, svg_color_get_color (color));
            c.alpha *= svg_number_get (alpha, 1);
            result = gsk_color_node_new2 (&c, &subregion);
            gdk_color_finish (&c);
          }
          break;

        case SVG_FILTER_MERGE:
          {
            GskRenderNode **children;
            unsigned int n_children;

            children = g_new0 (GskRenderNode *, filter->filters->len);
            n_children = 0;
            for (i++; i < filter->filters->len; i++)
              {
                SvgFilter *ff = g_ptr_array_index (filter->filters, i);
                FilterResult *in;

                if (svg_filter_get_type (ff) != SVG_FILTER_MERGE_NODE)
                  {
                    i--;
                    break;
                  }

                in = get_input_for_ref (svg_filter_get_current_value (ff, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);

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

        case SVG_FILTER_MERGE_NODE:
          break;

        case SVG_FILTER_BLEND:
          {
            FilterResult *in, *in2;
            GskBlendMode blend_mode;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);
            in2 = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN2), &subregion, shape, context, source, results);
            blend_mode = svg_enum_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_BLEND_MODE));

            result = gsk_blend_node_new2 (in2->node, in->node, color_state, blend_mode);

            filter_result_unref (in);
            filter_result_unref (in2);
          }
          break;

        case SVG_FILTER_OFFSET:
          {
            FilterResult *in;
            double dx, dy;
            GskTransform *transform;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);

            if (svg_enum_get (filter->current[SVG_PROPERTY_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
              {
                dx = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DX), 1) * bounds.size.width;
                dy = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DY), 1) * bounds.size.height;
              }
            else
              {
                dx = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DX), 1);
                dy = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DY), 1);
              }

            transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (dx, dy));
            result = apply_transform (in->node, transform);
            filter_result_unref (in);
            gsk_transform_unref (transform);
          }
          break;

        case SVG_FILTER_COMPOSITE:
          {
            FilterResult *in, *in2;
            CompositeOperator svg_op;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);
            in2 = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN2), &subregion, shape, context, source, results);
            svg_op = svg_enum_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_COMPOSITE_OPERATOR));

            if (svg_op == COMPOSITE_OPERATOR_LIGHTER)
              {
                gtk_svg_rendering_error (context->svg,
                                         "lighter composite operator not supported");
                result = gsk_container_node_new ((GskRenderNode*[]) { in2->node, in->node }, 2);
              }
            else if (svg_op == COMPOSITE_OPERATOR_ARITHMETIC)
              {
                float k1, k2, k3, k4;

                k1 = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_COMPOSITE_K1), 1);
                k2 = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_COMPOSITE_K2), 1);
                k3 = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_COMPOSITE_K3), 1);
                k4 = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_COMPOSITE_K4), 1);

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

        case SVG_FILTER_TILE:
          {
            FilterResult *in;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);

            result = gsk_repeat_node_new (&subregion, in->node, &in->bounds);

            filter_result_unref (in);
          }
          break;

        case SVG_FILTER_COMPONENT_TRANSFER:
          {
            GskComponentTransfer *r = gsk_component_transfer_new_identity ();
            GskComponentTransfer *g = gsk_component_transfer_new_identity ();
            GskComponentTransfer *b = gsk_component_transfer_new_identity ();
            GskComponentTransfer *a = gsk_component_transfer_new_identity ();
            FilterResult *in;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);

            for (i++; i < filter->filters->len; i++)
              {
                SvgFilter *ff = g_ptr_array_index (filter->filters, i);
                SvgFilterType t = svg_filter_get_type (ff);
                if (t == SVG_FILTER_FUNC_R)
                  {
                    gsk_component_transfer_free (r);
                    r = svg_filter_get_component_transfer (ff);
                  }
                else if (t == SVG_FILTER_FUNC_G)
                  {
                    gsk_component_transfer_free (g);
                    g = svg_filter_get_component_transfer (ff);
                  }
                else if (t == SVG_FILTER_FUNC_B)
                  {
                    gsk_component_transfer_free (b);
                    b = svg_filter_get_component_transfer (ff);
                  }
                else if (t == SVG_FILTER_FUNC_A)
                  {
                    gsk_component_transfer_free (a);
                    a = svg_filter_get_component_transfer (ff);
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

        case SVG_FILTER_FUNC_R:
        case SVG_FILTER_FUNC_G:
        case SVG_FILTER_FUNC_B:
        case SVG_FILTER_FUNC_A:
          break;

        case SVG_FILTER_COLOR_MATRIX:
          {
            FilterResult *in;
            graphene_matrix_t matrix;
            graphene_vec4_t offset;
            GskRenderNode *node;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);
            color_matrix_type_get_color_matrix (svg_enum_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_COLOR_MATRIX_TYPE)),
                                                svg_filter_get_current_value (f, SVG_PROPERTY_FE_COLOR_MATRIX_VALUES),
                                                &matrix, &offset);

            node = skip_debug_node (in->node);
            if (gsk_render_node_get_node_type (node) == GSK_COLOR_NODE)
              {
                const GdkColor *color = gsk_color_node_get_gdk_color (node);
                GdkColor new_color;

                color_apply_color_matrix (color, color_state, &matrix, &offset,  &new_color);
                result = gsk_color_node_new2 (&new_color, &node->bounds);
                gdk_color_finish (&new_color);
              }
            else
              result = gsk_color_matrix_node_new2 (in->node, color_state, &matrix, &offset);

            filter_result_unref (in);
          }
          break;

        case SVG_FILTER_DROPSHADOW:
          {
            FilterResult *in;
            double std_dev;
            double dx, dy;
            SvgValue *color;
            SvgValue *alpha;
            GskShadowEntry shadow;
            SvgValue *num;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);
            num = svg_filter_get_current_value (f, SVG_PROPERTY_FE_STD_DEV);
            if (svg_numbers_get_length (num) == 2 &&
                svg_numbers_get (num, 0, 1) != svg_numbers_get (num, 1, 1))
              gtk_svg_rendering_error (context->svg,
                                       "Separate x/y values for stdDeviation not supported");
            std_dev = svg_numbers_get (num, 0, 1);

            if (svg_enum_get (filter->current[SVG_PROPERTY_CONTENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
              {
                dx = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DX), 1) * bounds.size.width;
                dy = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DY), 1) * bounds.size.height;
              }
            else
              {
                dx = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DX), 1);
                dy = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DY), 1);
              }

            color = svg_filter_get_current_value (f, SVG_PROPERTY_FE_COLOR);
            alpha = svg_filter_get_current_value (f, SVG_PROPERTY_FE_OPACITY);

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

        case SVG_FILTER_IMAGE:
          {
            SvgValue *href = svg_filter_get_current_value (f, SVG_PROPERTY_FE_IMAGE_HREF);
            SvgValue *cf = svg_filter_get_current_value (f, SVG_PROPERTY_FE_IMAGE_CONTENT_FIT);
            GskTransform *transform;
            GskRenderNode *node;

            if (svg_href_get_kind (href) == HREF_NONE)
              {
                result = empty_node ();
              }
            else if (svg_href_get_texture (href) != NULL)
              {
                graphene_rect_t vb;
                double sx, sy, tx, ty;

                graphene_rect_init (&vb,
                                    0, 0,
                                    gdk_texture_get_width (svg_href_get_texture (href)),
                                    gdk_texture_get_height (svg_href_get_texture (href)));

                compute_viewport_transform (svg_content_fit_is_none (cf),
                                            svg_content_fit_get_align_x (cf),
                                            svg_content_fit_get_align_y (cf),
                                            svg_content_fit_get_meet (cf),
                                            &vb,
                                            subregion.origin.x, subregion.origin.y,
                                            subregion.size.width, subregion.size.height,
                                            &sx, &sy, &tx, &ty);

                transform = gsk_transform_scale (gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (tx, ty)), sx, sy);

                node = gsk_texture_node_new (svg_href_get_texture (href), &vb);
                result = apply_transform (node, transform);
                gsk_render_node_unref (node);
                gsk_transform_unref (transform);
              }
            else if (svg_href_get_shape (href) != NULL)
              {
                gtk_snapshot_push_collect (context->snapshot);
                render_shape (svg_href_get_shape (href), context);
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

        case SVG_FILTER_DISPLACEMENT:
          {
            FilterResult *in, *in2;
            GdkColorChannel channels[2];
            graphene_size_t max, scale;
            graphene_point_t offset;

            in = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN), &subregion, shape, context, source, results);
            in2 = get_input_for_ref (svg_filter_get_current_value (f, SVG_PROPERTY_FE_IN2), &subregion, shape, context, source, results);
            channels[0] = svg_enum_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DISPLACEMENT_X));
            channels[1] = svg_enum_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DISPLACEMENT_Y));
            scale.width = svg_number_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_DISPLACEMENT_SCALE), 1);
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

         id = svg_string_get (svg_filter_get_current_value (f, SVG_PROPERTY_FE_RESULT));
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
                        SvgElement    *shape,
                        GskRenderNode *source)
{
  GskRenderNode *result = gsk_render_node_ref (source);

  for (unsigned int i = 0; i < svg_filter_functions_get_length (filter); i++)
    {
      GskRenderNode *child = result;

      switch (svg_filter_functions_get_kind (filter, i))
        {
        case FILTER_NONE:
          result = gsk_render_node_ref (child);
          break;
        case FILTER_BLUR:
          result = gsk_blur_node_new (child, 2 * svg_filter_functions_get_simple (filter, i));
          break;
        case FILTER_OPACITY:
          result = gsk_opacity_node_new (child, svg_filter_functions_get_simple (filter, i));
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

            svg_filter_functions_get_color_matrix (filter, i, &matrix, &offset);
            result = gsk_color_matrix_node_new (child, &matrix, &offset);
          }
          break;
        case FILTER_DROPSHADOW:
          {
            GskShadowEntry shadow;
            double x, y, std_dev;

            svg_filter_functions_get_dropshadow (filter, i, &shadow.color, &x, &y, &std_dev);
            shadow.offset.x = x;
            shadow.offset.y = y;
            shadow.radius = 2 * std_dev;

            result = gsk_shadow_node_new2 (child, &shadow, 1);

            gdk_color_finish (&shadow.color);
          }
          break;
        case FILTER_REF:
          if (svg_filter_functions_get_shape (filter, i) == NULL)
            {
              gsk_render_node_unref (child);
              return gsk_render_node_ref (source);
            }

          result = apply_filter_tree (shape, svg_filter_functions_get_shape (filter, i), context, child);
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
needs_copy (SvgElement    *shape,
            PaintContext  *context,
            const char   **reason)
{
  SvgValue *filter = svg_element_get_current_value (shape, SVG_PROPERTY_FILTER);
  SvgValue *blend = svg_element_get_current_value (shape, SVG_PROPERTY_BLEND_MODE);

  if (context->op == CLIPPING)
    return FALSE;

  if (svg_filter_functions_need_backdrop (filter))
    {
      if (reason) *reason = "filter";
      return TRUE;
    }

  if (svg_enum_get (blend) != GSK_BLEND_MODE_DEFAULT)
    {
      if (reason) *reason = "blending";
      return TRUE;
    }

  return FALSE;
}

static gboolean
needs_isolation (SvgElement    *shape,
                 PaintContext  *context,
                 const char   **reason)
{
  SvgValue *isolation = svg_element_get_current_value (shape, SVG_PROPERTY_ISOLATION);
  SvgValue *opacity = svg_element_get_current_value (shape, SVG_PROPERTY_OPACITY);
  SvgValue *blend = svg_element_get_current_value (shape, SVG_PROPERTY_BLEND_MODE);
  SvgValue *filter = svg_element_get_current_value (shape, SVG_PROPERTY_FILTER);
  SvgValue *tf = svg_element_get_current_value (shape, SVG_PROPERTY_TRANSFORM);

  if (context->op == CLIPPING)
    return FALSE;

  if (svg_element_get_type (shape) == SVG_ELEMENT_SVG && svg_element_get_parent (shape) == NULL)
    {
      if (reason) *reason = "toplevel <svg>";
      return TRUE;
    }

  if (context->op == MASKING && context->op_changed && svg_element_get_type (shape) == SVG_ELEMENT_MASK)
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

  if (!svg_filter_functions_is_none (filter))
    {
      if (reason) *reason = "filter";
      return TRUE;
    }

  if (!svg_transform_is_none (tf))
    {
      GskTransform *transform = svg_transform_get_gsk (tf);

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
shape_is_use_target (SvgElement *shape)
{
  return shape->parent != NULL &&
         svg_element_get_type (shape->parent) == SVG_ELEMENT_USE;
}

static void
push_group (SvgElement   *shape,
            PaintContext *context)
{
  SvgValue *filter = svg_element_get_current_value (shape, SVG_PROPERTY_FILTER);
  SvgValue *opacity = svg_element_get_current_value (shape, SVG_PROPERTY_OPACITY);
  SvgValue *clip = svg_element_get_current_value (shape, SVG_PROPERTY_CLIP_PATH);
  SvgValue *mask = svg_element_get_current_value (shape, SVG_PROPERTY_MASK);
  SvgValue *tf = svg_element_get_current_value (shape, SVG_PROPERTY_TRANSFORM);
  SvgValue *blend = svg_element_get_current_value (shape, SVG_PROPERTY_BLEND_MODE);
  SvgValue *fill_rule = svg_element_get_current_value (shape, SVG_PROPERTY_FILL_RULE);

#ifdef DEBUG
  if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
    {
      GtkSvgLocation loc;

      svg_element_get_origin (shape, &loc);
      if (svg_element_get_id (shape))
        gtk_snapshot_push_debug (context->snapshot, "Group for <%s id='%s'> at line %" G_GSIZE_FORMAT, svg_element_type_get_name (svg_element_get_type (shape)), svg_element_get_id (shape), loc.lines);
      else
        gtk_snapshot_push_debug (context->snapshot, "Group for <%s> at line %" G_GSIZE_FORMAT, svg_element_type_get_name (svg_element_get_type (shape)), loc.lines);
    }
#endif

  if (svg_element_get_type (shape) == SVG_ELEMENT_SVG || svg_element_get_type (shape) == SVG_ELEMENT_SYMBOL)
    {
      SvgValue *cf = svg_element_get_current_value (shape, SVG_PROPERTY_CONTENT_FIT);
      SvgValue *overflow = svg_element_get_current_value (shape, SVG_PROPERTY_OVERFLOW);
      double x, y, width, height;
      double sx, sy, tx, ty;
      graphene_rect_t view_box;
      graphene_rect_t *viewport;
      double w, h;

      if (svg_element_get_parent (shape) == NULL)
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

          x = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_X), context->viewport->size.width);
          y = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_Y), context->viewport->size.height);

          if (shape_is_use_target (shape))
            {
              SvgElement *use = shape->parent;
              if (svg_element_is_specified (use, SVG_PROPERTY_WIDTH))
                width = svg_number_get (use->current[SVG_PROPERTY_WIDTH], context->viewport->size.width);
              else
                width = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_WIDTH), context->viewport->size.width);
              if (svg_element_is_specified (use, SVG_PROPERTY_HEIGHT))
                height = svg_number_get (use->current[SVG_PROPERTY_HEIGHT], context->viewport->size.height);
              else
                height = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_HEIGHT), context->viewport->size.height);
            }
          else
            {
              width = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_WIDTH), context->viewport->size.width);
              height = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_HEIGHT), context->viewport->size.height);
            }

          w = width;
          h = height;
        }

      if (svg_enum_get (overflow) == OVERFLOW_HIDDEN)
        {
          if (context->picking.picking)
            {
              if (!gsk_rect_contains_point (&GRAPHENE_RECT_INIT (x, y, width, height), &context->picking.p))
                {
                  if (!context->picking.done)
                    context->picking.done = TRUE;
                }
            }

          gtk_snapshot_push_clip (context->snapshot, &GRAPHENE_RECT_INIT (x, y, width, height));
        }

      if (!svg_view_box_get (svg_element_get_current_value (shape, SVG_PROPERTY_VIEW_BOX), &view_box))
        graphene_rect_init (&view_box, 0, 0, w, h);

      viewport = g_new (graphene_rect_t, 1);
      graphene_rect_init_from_rect (viewport, &view_box);

      compute_viewport_transform (svg_content_fit_is_none (cf),
                                  svg_content_fit_get_align_x (cf),
                                  svg_content_fit_get_align_y (cf),
                                  svg_content_fit_get_meet (cf),
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

  if (svg_element_get_type (shape) != SVG_ELEMENT_CLIP_PATH && !svg_transform_is_none (tf))
    {
      GskTransform *transform = svg_transform_get_gsk (tf);

      if (svg_element_is_specified (shape, SVG_PROPERTY_TRANSFORM_ORIGIN))
        {
          SvgValue *tfo = svg_element_get_current_value (shape, SVG_PROPERTY_TRANSFORM_ORIGIN);
          SvgValue *tfb = svg_element_get_current_value (shape, SVG_PROPERTY_TRANSFORM_BOX);
          graphene_rect_t bounds;
          double x, y;

          switch (svg_enum_get (tfb))
            {
            case TRANSFORM_BOX_CONTENT_BOX:
            case TRANSFORM_BOX_FILL_BOX:
              svg_element_get_current_bounds (shape, context->viewport, context->svg, &bounds);
              break;
            case TRANSFORM_BOX_BORDER_BOX:
            case TRANSFORM_BOX_STROKE_BOX:
              svg_element_get_current_stroke_bounds (shape, context->viewport, context->svg, &bounds);
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

  if (svg_element_get_type (shape) == SVG_ELEMENT_USE)
    {
      double x, y;

      x = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_X), context->viewport->size.width);
      y = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_Y), context->viewport->size.height);
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

          svg_element_get_current_bounds (shape, context->viewport, context->svg, &bounds);

          gtk_snapshot_push_blend (context->snapshot, svg_enum_get (blend));
          gtk_snapshot_append_paste (context->snapshot, &bounds, 0);
          gtk_snapshot_pop (context->snapshot);
        }
    }

  if (svg_clip_get_kind (clip) == CLIP_PATH ||
      (svg_clip_get_kind (clip) == CLIP_URL && svg_clip_get_shape (clip) != NULL))
    {
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

          if (context->picking.picking)
            {
              if (!gsk_path_in_fill (svg_clip_get_path (clip), &context->picking.p, rule))
                {
                  if (!context->picking.done)
                    context->picking.clipped = shape;
                }
            }

#ifdef DEBUG
          if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
            gtk_snapshot_push_debug (context->snapshot, "fill or clip for clip");
#endif
          svg_snapshot_push_fill (context->snapshot, svg_clip_get_path (clip), rule);
        }
      else
        {
          SvgElement *clip_shape = svg_clip_get_shape (clip);
          SvgValue *ctf = svg_element_get_current_value (clip_shape, SVG_PROPERTY_TRANSFORM);
          SvgElement *child = NULL;

          /* In the general case, we collect the clip geometry in a mask.
           * We special-case a single shape in the <clipPath> without
           * transforms and translate them to a clip or a fill.
           */
          if (clip_shape->shapes->len > 0)
            child = g_ptr_array_index (clip_shape->shapes, 0);

          if (svg_transform_is_none (ctf) &&
              svg_enum_get (svg_element_get_current_value (clip_shape, SVG_PROPERTY_CONTENT_UNITS)) == COORD_UNITS_USER_SPACE_ON_USE &&
              clip_shape->shapes->len == 1 &&
              (child->type == SVG_ELEMENT_PATH || child->type == SVG_ELEMENT_RECT || child->type == SVG_ELEMENT_CIRCLE) &&
              svg_enum_get (svg_element_get_current_value (child, SVG_PROPERTY_VISIBILITY)) != VISIBILITY_HIDDEN &&
              svg_enum_get (svg_element_get_current_value (child, SVG_PROPERTY_DISPLAY)) != DISPLAY_NONE &&
              svg_clip_get_kind (svg_element_get_current_value (child, SVG_PROPERTY_CLIP_PATH)) == CLIP_NONE &&
              svg_transform_is_none (svg_element_get_current_value (child, SVG_PROPERTY_TRANSFORM)))
            {
              GskPath *path;

              path = svg_element_get_current_path (child, context->viewport);

              if (context->picking.picking)
                {
                  if (!gsk_path_in_fill (path, &context->picking.p, GSK_FILL_RULE_WINDING))
                    {
                      if (!context->picking.done)
                        context->picking.clipped = shape;
                    }
                }

#ifdef DEBUG
              if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
                gtk_snapshot_push_debug (context->snapshot, "fill or clip for clip");
#endif
              svg_snapshot_push_fill (context->snapshot, path, GSK_FILL_RULE_WINDING);

              gsk_path_unref (path);
            }
          else
            {
              push_op (context, CLIPPING);
#ifdef DEBUG
              if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
                gtk_snapshot_push_debug (context->snapshot, "mask for clip");
#endif
              gtk_snapshot_push_mask (context->snapshot, GSK_MASK_MODE_ALPHA);

              if (!svg_transform_is_none (ctf))
                {
                  GskTransform *transform = svg_transform_get_gsk (ctf);

                  if (svg_element_is_specified (clip_shape, SVG_PROPERTY_TRANSFORM_ORIGIN))
                    {
                      SvgValue *tfo = svg_element_get_current_value (clip_shape, SVG_PROPERTY_TRANSFORM_ORIGIN);
                      SvgValue *tfb = svg_element_get_current_value (clip_shape, SVG_PROPERTY_TRANSFORM_BOX);
                      graphene_rect_t bounds;
                      double x, y;

                      switch (svg_enum_get (tfb))
                        {
                        case TRANSFORM_BOX_CONTENT_BOX:
                        case TRANSFORM_BOX_FILL_BOX:
                          svg_element_get_current_bounds (shape, context->viewport, context->svg, &bounds);
                          break;
                        case TRANSFORM_BOX_BORDER_BOX:
                        case TRANSFORM_BOX_STROKE_BOX:
                          svg_element_get_current_stroke_bounds (shape, context->viewport, context->svg, &bounds);
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

              if (svg_enum_get (svg_element_get_current_value (clip_shape, SVG_PROPERTY_CONTENT_UNITS)) == COORD_UNITS_OBJECT_BOUNDING_BOX)
                {
                  graphene_rect_t bounds;

                  GskTransform *transform = NULL;

                  gtk_snapshot_save (context->snapshot);

                  if (svg_element_get_current_bounds (shape, context->viewport, context->svg, &bounds))
                    {
                      transform = gsk_transform_translate (NULL, &bounds.origin);
                      transform = gsk_transform_scale (transform, bounds.size.width, bounds.size.height);
                      gtk_snapshot_transform (context->snapshot, transform);
                    }
                  push_transform (context, transform);
                  gsk_transform_unref (transform);
                }

              render_shape (clip_shape, context);

              if (svg_enum_get (svg_element_get_current_value (clip_shape, SVG_PROPERTY_CONTENT_UNITS)) == COORD_UNITS_OBJECT_BOUNDING_BOX)
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

              pop_op (context);

              if (context->picking.picking)
                {
                  if (context->picking.done && context->picking.picked != NULL)
                    context->picking.picked = NULL;
                  else
                    context->picking.clipped = shape;
                  context->picking.done = FALSE;
                }
            }
        }
    }

  if (svg_mask_get_kind (mask) != MASK_NONE &&
      svg_mask_get_shape (mask) != NULL &&
      context->op != CLIPPING)
    {
      SvgElement *mask_shape = svg_mask_get_shape (mask);
      gboolean has_clip = FALSE;

      push_op (context, MASKING);

#ifdef DEBUG
      if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
        gtk_snapshot_push_debug (context->snapshot, "mask for masking");
#endif
      gtk_snapshot_push_mask (context->snapshot, svg_enum_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_MASK_TYPE)));

      if (svg_element_is_specified (mask_shape, SVG_PROPERTY_X) ||
          svg_element_is_specified (mask_shape, SVG_PROPERTY_Y) ||
          svg_element_is_specified (mask_shape, SVG_PROPERTY_WIDTH) ||
          svg_element_is_specified (mask_shape, SVG_PROPERTY_HEIGHT))
        {
           graphene_rect_t mask_clip;

          if (svg_enum_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_BOUND_UNITS)) == COORD_UNITS_OBJECT_BOUNDING_BOX)
            {
              graphene_rect_t bounds;

              if (svg_element_get_current_bounds (shape, context->viewport, context->svg, &bounds))
                {
                  mask_clip.origin.x = bounds.origin.x + svg_number_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_X), 1) * bounds.size.width;
                  mask_clip.origin.y = bounds.origin.y + svg_number_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_Y), 1) * bounds.size.height;
                  mask_clip.size.width = svg_number_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_WIDTH), 1) * bounds.size.width;
                  mask_clip.size.height = svg_number_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_HEIGHT), 1) * bounds.size.height;
                  has_clip = TRUE;
                }
            }
          else
            {
              mask_clip.origin.x = svg_number_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_X), context->viewport->size.width);
              mask_clip.origin.y = svg_number_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_Y), context->viewport->size.height);
              mask_clip.size.width = svg_number_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_WIDTH), context->viewport->size.width);
              mask_clip.size.height = svg_number_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_HEIGHT), context->viewport->size.height);
              has_clip = TRUE;
            }

          if (has_clip)
            gtk_snapshot_push_clip (context->snapshot, &mask_clip);
        }

      if (svg_enum_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_CONTENT_UNITS)) == COORD_UNITS_OBJECT_BOUNDING_BOX)
        {
          graphene_rect_t bounds;
          GskTransform *transform = NULL;

          gtk_snapshot_save (context->snapshot);

          if (svg_element_get_current_bounds (shape, context->viewport, context->svg, &bounds))
            {
              transform = gsk_transform_translate (NULL, &bounds.origin);
              transform = gsk_transform_scale (transform, bounds.size.width, bounds.size.height);
              gtk_snapshot_transform (context->snapshot, transform);
            }

          push_transform (context, transform);
          gsk_transform_unref (transform);
        }

      render_shape (mask_shape, context);

      if (svg_enum_get (svg_element_get_current_value (mask_shape, SVG_PROPERTY_CONTENT_UNITS)) == COORD_UNITS_OBJECT_BOUNDING_BOX)
        {
          pop_transform (context);
          gtk_snapshot_restore (context->snapshot);
        }

      if (has_clip)
        gtk_snapshot_pop (context->snapshot);

      gtk_snapshot_pop (context->snapshot);

      pop_op (context);
    }

  if (!context->picking.picking &&
      context->op != CLIPPING &&
      svg_element_get_type (shape) != SVG_ELEMENT_MASK)
    {
      if (svg_number_get (opacity, 1) != 1)
        gtk_snapshot_push_opacity (context->snapshot, svg_number_get (opacity, 1));

      if (!svg_filter_functions_is_none (filter))
        gtk_snapshot_push_collect (context->snapshot);
    }
}

static void
pop_group (SvgElement   *shape,
           PaintContext *context)
{
  SvgValue *filter = svg_element_get_current_value (shape, SVG_PROPERTY_FILTER);
  SvgValue *opacity = svg_element_get_current_value (shape, SVG_PROPERTY_OPACITY);
  SvgValue *clip = svg_element_get_current_value (shape, SVG_PROPERTY_CLIP_PATH);
  SvgValue *mask = svg_element_get_current_value (shape, SVG_PROPERTY_MASK);
  SvgValue *tf = svg_element_get_current_value (shape, SVG_PROPERTY_TRANSFORM);
  SvgValue *blend = svg_element_get_current_value (shape, SVG_PROPERTY_BLEND_MODE);

  if (!context->picking.picking &&
      context->op != CLIPPING &&
      svg_element_get_type (shape) != SVG_ELEMENT_MASK)
    {
      if (!svg_filter_functions_is_none (filter))
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

  if (svg_element_get_type (shape) == SVG_ELEMENT_USE)
    {
      pop_transform (context);
      gtk_snapshot_restore (context->snapshot);
    }

  if (svg_element_get_type (shape) != SVG_ELEMENT_CLIP_PATH && !svg_transform_is_none (tf))
    {
      pop_transform (context);
      gtk_snapshot_restore (context->snapshot);
    }

  if (svg_element_get_type (shape) == SVG_ELEMENT_SVG || svg_element_get_type (shape) == SVG_ELEMENT_SYMBOL)
    {
      SvgValue *overflow = svg_element_get_current_value (shape, SVG_PROPERTY_OVERFLOW);

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
template_type_compatible (SvgElementType type1,
                          SvgElementType type2)
{
  return type1 == type2 ||
         (svg_element_type_is_gradient (type1) && svg_element_type_is_gradient (type2));
}

/* Only return explicitly set values from a template.
 * If we don't have one, we use the initial value for
 * the original paint server.
 */
static SvgValue *
paint_server_get_template_value (SvgElement   *shape,
                                 SvgProperty   attr,
                                 PaintContext *context)
{
  if (!svg_element_is_specified (shape, attr))
    {
      SvgValue *href = svg_element_get_current_value (shape, SVG_PROPERTY_HREF);
      const char *ref = svg_href_get_id (href);

      if (context->depth > NESTING_LIMIT)
        {
          gtk_svg_rendering_error (context->svg,
                                   "excessive rendering depth (> %d) while resolving href %s, aborting",
                                   NESTING_LIMIT,
                                   ref);
          goto fail;
        }

      if (svg_href_get_shape (href))
        {
          SvgElement *template = svg_href_get_shape (href);
          if (template_type_compatible (template->type, svg_element_get_type (shape)))
            {
              SvgValue *ret;

              context->depth++;
              ret = paint_server_get_template_value (template, attr, context);
              context->depth--;

              return ret;
            }

          gtk_svg_invalid_reference (context->svg,
                                     "<%s> can not use a <%s> as template (while resolving href %s)",
                                     svg_element_type_get_name (svg_element_get_type (shape)),
                                     svg_element_type_get_name (template->type),
                                     ref);
        }

      return NULL;
    }

fail:
  return svg_element_get_current_value (shape, attr);
}

static SvgValue *
paint_server_get_current_value (SvgElement   *shape,
                                SvgProperty   attr,
                                PaintContext *context)
{
  SvgValue *value = NULL;

  if (svg_element_is_specified (shape, attr))
    return svg_element_get_current_value (shape, attr);

  value = paint_server_get_template_value (shape, attr, context);
  if (value)
    return value;

  return svg_element_get_current_value (shape, attr);
}

static GPtrArray *
gradient_get_color_stops (SvgElement   *shape,
                          PaintContext *context)
{
  if (shape->color_stops->len == 0)
    {
      SvgValue *href = svg_element_get_current_value (shape, SVG_PROPERTY_HREF);
      const char *ref = svg_href_get_id (href);

      if (context->depth > NESTING_LIMIT)
        {
          gtk_svg_rendering_error (context->svg,
                                   "excessive rendering depth (> %d) while resolving href %s, aborting",
                                   NESTING_LIMIT,
                                   ref);
          goto fail;
        }

      if (svg_href_get_shape (href))
        {
          SvgElement *template = svg_href_get_shape (href);
          if (template_type_compatible (template->type, svg_element_get_type (shape)))
            {
              GPtrArray *ret;
              context->depth++;
              ret = gradient_get_color_stops (template, context);
              context->depth--;
              return ret;
            }

          gtk_svg_invalid_reference (context->svg,
                                     "<%s> can not use a <%s> as template (while collecting color stops)",
                                     svg_element_type_get_name (svg_element_get_type (shape)),
                                     svg_element_type_get_name (template->type));
        }
    }

fail:
  return shape->color_stops;
}

static GskGradient *
gradient_get_gsk_gradient (SvgElement   *gradient,
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
      SvgColorStop *stop = g_ptr_array_index (color_stops, i);
      SvgValue *stop_color = svg_color_stop_get_current_value (stop, SVG_PROPERTY_STOP_COLOR);
      GdkColor color;

      offset = MAX (svg_number_get (svg_color_stop_get_current_value (stop, SVG_PROPERTY_STOP_OFFSET), 1), offset);
      gdk_color_init_copy (&color, svg_color_get_color (stop_color));
      color.alpha *= svg_number_get (svg_color_stop_get_current_value (stop, SVG_PROPERTY_STOP_OPACITY), 1);
      gsk_gradient_add_stop (g, offset, 0.5, &color);
      gdk_color_finish (&color);
    }

  spread_method = paint_server_get_current_value (gradient, SVG_PROPERTY_SPREAD_METHOD, context);
  gsk_gradient_set_repeat (g, svg_enum_get (spread_method));
  gsk_gradient_set_premultiplied (g, FALSE);

  color_interpolation = paint_server_get_current_value (gradient, SVG_PROPERTY_COLOR_INTERPOLATION, context);
  if (svg_enum_get (color_interpolation) == COLOR_INTERPOLATION_LINEAR)
    gsk_gradient_set_interpolation (g, GDK_COLOR_STATE_SRGB_LINEAR);
  else
    gsk_gradient_set_interpolation (g, GDK_COLOR_STATE_SRGB);

  return g;
}

static GPtrArray *
pattern_get_shapes (SvgElement   *shape,
                    PaintContext *context)
{
  if (shape->shapes->len == 0)
    {
      SvgValue *href = svg_element_get_current_value (shape, SVG_PROPERTY_HREF);
      const char *ref = svg_href_get_id (href);

      if (context->depth > NESTING_LIMIT)
        {
          gtk_svg_rendering_error (context->svg,
                                   "excessive rendering depth (> %d) while resolving href %s, aborting",
                                   NESTING_LIMIT,
                                   ref);

          goto fail;
        }

      if (svg_href_get_shape (href))
        {
          SvgElement *template = svg_href_get_shape (href);
          if (template_type_compatible (template->type, svg_element_get_type (shape)))
            {
              GPtrArray *ret;
              context->depth++;
              ret = pattern_get_shapes (template, context);
              context->depth--;
              return ret;
            }

          gtk_svg_invalid_reference (context->svg,
                                     "<%s> can not use a <%s> as template (while collecting pattern content)",
                                     svg_element_type_get_name (svg_element_get_type (shape)),
                                     svg_element_type_get_name (template->type));
        }
    }

fail:
  return shape->shapes;
}

static gboolean
paint_server_is_skipped (SvgElement            *server,
                         const graphene_rect_t *bounds,
                         PaintContext          *context)
{
  SvgValue *units;

  if (server->type == SVG_ELEMENT_PATTERN)
    {
      SvgValue *width = paint_server_get_current_value (server, SVG_PROPERTY_WIDTH, context);
      SvgValue *height = paint_server_get_current_value (server, SVG_PROPERTY_HEIGHT, context);

      if (svg_number_get (width, 1) == 0 || svg_number_get (height, 1) == 0)
        return TRUE;

      units = paint_server_get_current_value (server, SVG_PROPERTY_BOUND_UNITS, context);
    }
  else
    units = paint_server_get_current_value (server, SVG_PROPERTY_CONTENT_UNITS, context);

  if (svg_enum_get (units) == COORD_UNITS_OBJECT_BOUNDING_BOX)
    return bounds->size.width == 0 || bounds->size.height == 0;
  else
    return FALSE;
}

static void
paint_linear_gradient (SvgElement            *gradient,
                       const graphene_rect_t *bounds,
                       const graphene_rect_t *paint_bounds,
                       PaintContext          *context)
{
  graphene_point_t start, end;
  GskTransform *transform, *gradient_transform;
  GskGradient *g;
  SvgValue *x1 = paint_server_get_current_value (gradient, SVG_PROPERTY_X1, context);
  SvgValue *y1 = paint_server_get_current_value (gradient, SVG_PROPERTY_Y1, context);
  SvgValue *x2 = paint_server_get_current_value (gradient, SVG_PROPERTY_X2, context);
  SvgValue *y2 = paint_server_get_current_value (gradient, SVG_PROPERTY_Y2, context);
  SvgValue *tf = paint_server_get_current_value (gradient, SVG_PROPERTY_TRANSFORM, context);
  SvgValue *units = paint_server_get_current_value (gradient, SVG_PROPERTY_CONTENT_UNITS, context);

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
paint_radial_gradient (SvgElement            *gradient,
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
  SvgValue *fx = paint_server_get_current_value (gradient, SVG_PROPERTY_FX, context);
  SvgValue *fy = paint_server_get_current_value (gradient, SVG_PROPERTY_FY, context);
  SvgValue *fr = paint_server_get_current_value (gradient, SVG_PROPERTY_FR, context);
  SvgValue *cx = paint_server_get_current_value (gradient, SVG_PROPERTY_CX, context);
  SvgValue *cy = paint_server_get_current_value (gradient, SVG_PROPERTY_CY, context);
  SvgValue *r = paint_server_get_current_value (gradient, SVG_PROPERTY_R, context);
  SvgValue *tf = paint_server_get_current_value (gradient, SVG_PROPERTY_TRANSFORM, context);
  SvgValue *units = paint_server_get_current_value (gradient, SVG_PROPERTY_CONTENT_UNITS, context);

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

  if (svg_element_is_specified (gradient, SVG_PROPERTY_TRANSFORM_ORIGIN))
    {
      SvgValue *tfo = gradient->current[SVG_PROPERTY_TRANSFORM_ORIGIN];
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
paint_pattern (SvgElement            *pattern,
               const graphene_rect_t *bounds,
               const graphene_rect_t *paint_bounds,
               PaintContext          *context)
{
  graphene_rect_t pattern_bounds, child_bounds;
  double tx, ty;
  double sx, sy;
  GskTransform *transform;
  double dx, dy;
  SvgValue *bound_units = paint_server_get_current_value (pattern, SVG_PROPERTY_BOUND_UNITS, context);
  SvgValue *content_units = paint_server_get_current_value (pattern, SVG_PROPERTY_CONTENT_UNITS, context);
  SvgValue *x = paint_server_get_current_value (pattern, SVG_PROPERTY_X, context);
  SvgValue *y = paint_server_get_current_value (pattern, SVG_PROPERTY_Y, context);
  SvgValue *width = paint_server_get_current_value (pattern, SVG_PROPERTY_WIDTH, context);
  SvgValue *height = paint_server_get_current_value (pattern, SVG_PROPERTY_HEIGHT, context);
  SvgValue *tf = paint_server_get_current_value (pattern, SVG_PROPERTY_TRANSFORM, context);
  SvgValue *vb = paint_server_get_current_value (pattern, SVG_PROPERTY_VIEW_BOX, context);
  SvgValue *cf = paint_server_get_current_value (pattern, SVG_PROPERTY_CONTENT_FIT, context);
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
      compute_viewport_transform (svg_content_fit_is_none (cf),
                                  svg_content_fit_get_align_x (cf),
                                  svg_content_fit_get_align_y (cf),
                                  svg_content_fit_get_meet (cf),
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

  if (svg_element_is_specified (pattern, SVG_PROPERTY_TRANSFORM_ORIGIN))
    {
      SvgValue *tfo = pattern->current[SVG_PROPERTY_TRANSFORM_ORIGIN];
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
          SvgElement *s = g_ptr_array_index (shapes, i);
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
  SvgElement *server = svg_paint_get_server_shape (paint);

  if (server == NULL ||
      paint_server_is_skipped (server, bounds, context))
    {
      gtk_snapshot_add_color (context->snapshot,
                              svg_paint_get_server_fallback (paint),
                              paint_bounds);
    }
  else if (server->type == SVG_ELEMENT_LINEAR_GRADIENT ||
           server->type == SVG_ELEMENT_RADIAL_GRADIENT)
    {
      GPtrArray *color_stops;

      color_stops = gradient_get_color_stops (server, context);

      if (color_stops->len == 0)
        return;

      if (color_stops->len == 1)
        {
          SvgColorStop *stop = g_ptr_array_index (color_stops, 0);
          SvgValue *stop_color = svg_color_stop_get_current_value (stop, SVG_PROPERTY_STOP_COLOR);
          GdkColor c;

          gdk_color_init_copy (&c, svg_color_get_color (stop_color));
          c.alpha *= svg_number_get (svg_color_stop_get_current_value (stop, SVG_PROPERTY_STOP_OPACITY), 1);
          gtk_snapshot_add_color (context->snapshot, &c, paint_bounds);
          gdk_color_finish (&c);
          return;
        }

      if (server->type == SVG_ELEMENT_LINEAR_GRADIENT)
        paint_linear_gradient (server, bounds, paint_bounds, context);
      else
        paint_radial_gradient (server, bounds, paint_bounds, context);
    }
  else if (server->type == SVG_ELEMENT_PATTERN)
    {
      paint_pattern (server, bounds, paint_bounds, context);
    }
}

/* }}} */
/* {{{ Shapes */

static GskStroke *
shape_create_stroke (SvgElement   *shape,
                     PaintContext *context)
{
  GskStroke *stroke;
  SvgValue *da;

  stroke = svg_element_create_basic_stroke (shape, context->viewport, context->svg->features & GTK_SVG_EXTENSIONS, context->weight);

  da = svg_element_get_current_value (shape, SVG_PROPERTY_STROKE_DASHARRAY);
  if (svg_dash_array_get_kind (da) != DASH_ARRAY_NONE)
    {
      unsigned int len;
      double path_length;
      double length;
      double offset;
      GskPathMeasure *measure;
      gboolean invalid = FALSE;
      float *vals;

      if (svg_element_type_is_text (svg_element_get_type (shape)))
        {
          gtk_svg_rendering_error (context->svg,
                                   "Dashing of stroked text is not supported");
          return stroke;
        }

      measure = svg_element_get_current_measure (shape, context->viewport);
      length = gsk_path_measure_get_length (measure);
      gsk_path_measure_unref (measure);

      path_length = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_PATH_LENGTH), 1);
      if (path_length < 0)
        path_length = length;

      offset = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_STROKE_DASHOFFSET), normalized_diagonal (context->viewport));

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
          SvgElement *shape = ctx_stack->data;

          if (svg_paint_get_kind (paint) == PAINT_CONTEXT_FILL)
            paint = svg_element_get_current_value (shape, SVG_PROPERTY_FILL);
          else
            paint = svg_element_get_current_value (shape, SVG_PROPERTY_STROKE);

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
fill_shape (SvgElement   *shape,
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

  fill_rule = svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_FILL_RULE));

  switch (svg_paint_get_kind (paint))
    {
    case PAINT_NONE:
      break;
    case PAINT_COLOR:
      {
        GdkColor color;

        gdk_color_init_copy (&color, svg_paint_get_color (paint));
        color.alpha *= opacity;
        svg_snapshot_push_fill (context->snapshot, path, fill_rule);
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

        svg_snapshot_push_fill (context->snapshot, path, fill_rule);
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
stroke_shape (SvgElement   *shape,
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

  paint = svg_element_get_current_value (shape, SVG_PROPERTY_STROKE);
  paint = get_context_paint (paint, context->ctx_shape_stack);
  if (svg_paint_get_kind (paint) == PAINT_NONE)
    return;

  if (!gsk_path_get_bounds (path, &bounds))
    return;

  stroke = shape_create_stroke (shape, context);
  if (!gsk_path_get_stroke_bounds (path, stroke, &paint_bounds))
    return;

  opacity = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_STROKE_OPACITY), 1);
  effect = svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_VECTOR_EFFECT));

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
      transformed_path = svg_transform_path (user_transform, path);

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
paint_marker (SvgElement         *shape,
              GskPath            *path,
              PaintContext       *context,
              const GskPathPoint *point,
              VertexKind          kind)
{
  SvgProperty attrs[] = { SVG_PROPERTY_MARKER_START, SVG_PROPERTY_MARKER_MID, SVG_PROPERTY_MARKER_END };
  SvgValue *href;
  SvgValue *orient;
  SvgValue *units;
  SvgElement *marker;
  double scale;
  graphene_point_t vertex;
  double angle;
  double x, y, width, height;
  SvgValue *vb;
  SvgValue *cf;
  SvgValue *overflow;
  double sx, sy, tx, ty;
  graphene_rect_t view_box;
  SvgValue *v;
  GskTransform *transform = NULL;

  gsk_path_point_get_position (point, path, &vertex);

  href = svg_element_get_current_value (shape, attrs[kind]);
  if (svg_href_get_kind (href) == HREF_NONE)
    return;

  marker = svg_href_get_shape (href);
  if (!marker)
    return;

  orient = marker->current[SVG_PROPERTY_MARKER_ORIENT];
  units = marker->current[SVG_PROPERTY_MARKER_UNITS];

  if (svg_enum_get (units) == MARKER_UNITS_STROKE_WIDTH)
    scale = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_STROKE_WIDTH), 1);
  else
    scale = 1;

  if (svg_orient_get_kind (orient) == ORIENT_AUTO)
    {
      if (kind == VERTEX_START)
        {
          angle = gsk_path_point_get_rotation (point, path, GSK_PATH_TO_END);
          if (svg_orient_get_start_reverse (orient))
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
      angle = svg_orient_get_angle (orient);
    }

  vb = marker->current[SVG_PROPERTY_VIEW_BOX];
  cf = marker->current[SVG_PROPERTY_CONTENT_FIT];
  overflow = marker->current[SVG_PROPERTY_OVERFLOW];

  width = svg_number_get (marker->current[SVG_PROPERTY_WIDTH], context->viewport->size.width);
  height = svg_number_get (marker->current[SVG_PROPERTY_HEIGHT], context->viewport->size.height);

  if (width == 0 || height == 0)
    return;

  v = marker->current[SVG_PROPERTY_REF_X];
  if (svg_number_get_unit (v) == SVG_UNIT_PERCENTAGE)
    x = svg_number_get (v, width);
  else if (svg_view_box_get (vb, &view_box))
    x = svg_number_get (v, 100) / view_box.size.width * width;
  else
    x = svg_number_get (v, 100);

  v = marker->current[SVG_PROPERTY_REF_Y];
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

  compute_viewport_transform (svg_content_fit_is_none (cf),
                              svg_content_fit_get_align_x (cf),
                              svg_content_fit_get_align_y (cf),
                              svg_content_fit_get_meet (cf),
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
paint_markers (SvgElement   *shape,
               GskPath      *path,
               PaintContext *context)
{
  GskPathPoint point, point1;

  if (gsk_path_is_empty (path))
    return;

  if (svg_href_get_kind (svg_element_get_current_value (shape, SVG_PROPERTY_MARKER_START)) == HREF_NONE &&
      svg_href_get_kind (svg_element_get_current_value (shape, SVG_PROPERTY_MARKER_MID)) == HREF_NONE &&
      svg_href_get_kind (svg_element_get_current_value (shape, SVG_PROPERTY_MARKER_END)) == HREF_NONE)
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
text_create_layout (SvgElement       *self,
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
  pango_context_set_language (context, svg_language_get (self->current[SVG_PROPERTY_LANG], 0));

  uni = svg_enum_get (self->current[SVG_PROPERTY_UNICODE_BIDI]);
  direction = svg_enum_get (self->current[SVG_PROPERTY_DIRECTION]);
  wmode = svg_enum_get (self->current[SVG_PROPERTY_WRITING_MODE]);
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

  font_family = self->current[SVG_PROPERTY_FONT_FAMILY];
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

  pango_font_description_set_style (font_desc, svg_enum_get (self->current[SVG_PROPERTY_FONT_STYLE]));
  pango_font_description_set_variant (font_desc, svg_enum_get (self->current[SVG_PROPERTY_FONT_VARIANT]));
  pango_font_description_set_weight (font_desc, svg_number_get (self->current[SVG_PROPERTY_FONT_WEIGHT], 1000.));
  pango_font_description_set_stretch (font_desc, svg_enum_get (self->current[SVG_PROPERTY_FONT_STRETCH]));

  pango_font_description_set_size (font_desc,
                                   svg_number_get (self->current[SVG_PROPERTY_FONT_SIZE], 1.) *
                                   PANGO_SCALE / SVG_DEFAULT_DPI * 72);

  layout = pango_layout_new (context);
  pango_layout_set_font_description (layout, font_desc);
  pango_font_description_free (font_desc);

  attr_list = pango_attr_list_new ();

  attr = pango_attr_letter_spacing_new (svg_number_get (self->current[SVG_PROPERTY_LETTER_SPACING], 1.) * PANGO_SCALE);
  attr->start_index = 0;
  attr->end_index = G_MAXINT;
  pango_attr_list_insert (attr_list, attr);

  decoration = svg_text_decoration_get (self->current[SVG_PROPERTY_TEXT_DECORATION]);
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
generate_layouts (SvgElement      *self,
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

  g_assert (svg_element_type_is_text (self->type));

  if (svg_enum_get (self->current[SVG_PROPERTY_DISPLAY]) == DISPLAY_NONE)
    return FALSE;

  if (!x)
    x = &xs;
  if (!y)
    y = &ys;
  if (!lastwasspace)
    lastwasspace = &lwss;

  if (svg_element_is_specified (self, SVG_PROPERTY_X))
    *x = svg_number_get (self->current[SVG_PROPERTY_X], 1);
  if (svg_element_is_specified (self, SVG_PROPERTY_Y))
    *y = svg_number_get (self->current[SVG_PROPERTY_Y], 1);

  dx = svg_number_get (self->current[SVG_PROPERTY_DX], 1);
  dy = svg_number_get (self->current[SVG_PROPERTY_DY], 1);
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
clear_layouts (SvgElement *self)
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
fill_text (SvgElement            *self,
           PaintContext          *context,
           SvgValue              *paint,
           const graphene_rect_t *bounds)
{
  g_assert (svg_element_type_is_text (self->type));

  for (unsigned int i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);

      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          {
            SvgValue *cpaint = paint;
            const graphene_rect_t *cbounds = bounds;

            if (svg_enum_get (svg_element_get_current_value (node->shape.shape, SVG_PROPERTY_DISPLAY)) == DISPLAY_NONE)
              continue;

            if (svg_enum_get (svg_element_get_current_value (node->shape.shape, SVG_PROPERTY_VISIBILITY)) == VISIBILITY_HIDDEN)
              continue;

            if (svg_element_is_specified (node->shape.shape, SVG_PROPERTY_FILL))
              {
                cpaint = svg_element_get_current_value (node->shape.shape, SVG_PROPERTY_FILL);
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

            opacity = svg_number_get (self->current[SVG_PROPERTY_FILL_OPACITY], 1);
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
stroke_text (SvgElement            *self,
             PaintContext          *context,
             SvgValue              *paint,
             GskStroke             *stroke,
             const graphene_rect_t *bounds)
{
  g_assert (svg_element_type_is_text (self->type));

  for (unsigned int i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);

      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          {
            SvgValue *cpaint = paint;
            const graphene_rect_t *cbounds = bounds;

            if (svg_enum_get (svg_element_get_current_value (node->shape.shape, SVG_PROPERTY_DISPLAY)) == DISPLAY_NONE)
              continue;

            if (svg_enum_get (svg_element_get_current_value (node->shape.shape, SVG_PROPERTY_VISIBILITY)) == VISIBILITY_HIDDEN)
              continue;

            if (svg_element_is_specified (node->shape.shape, SVG_PROPERTY_STROKE))
              {
                cpaint = svg_element_get_current_value (node->shape.shape, SVG_PROPERTY_STROKE);
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
            opacity = svg_number_get (self->current[SVG_PROPERTY_STROKE_OPACITY], 1);
            if (svg_paint_get_kind (paint) == PAINT_COLOR)
              {
                GdkColor color;

                gdk_color_init_copy (&color, svg_paint_get_color (paint));
                color.alpha *= opacity;

                gtk_snapshot_push_mask (context->snapshot, GSK_MASK_MODE_ALPHA);
                gtk_snapshot_translate (context->snapshot, &GRAPHENE_POINT_INIT (node->characters.x, node->characters.y));
                gtk_snapshot_rotate (context->snapshot, node->characters.r);
                path = svg_pango_layout_to_path (node->characters.layout);

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
                path = svg_pango_layout_to_path (node->characters.layout);
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

static gboolean
point_in_pango_rect (PangoRectangle         *rect,
                     const graphene_point_t *p)
{
  pango_extents_to_pixels (rect, NULL);
  return rect->x <= p->x && p->x <= rect->x + rect->width &&
         rect->y <= p->y && p->y <= rect->y + rect->height;
}

static gboolean
point_in_layout (PangoLayout            *layout,
                 const graphene_point_t *p)
{
  PangoLayoutIter *iter;
  PangoRectangle rect;

  iter = pango_layout_get_iter (layout);
  do {
    pango_layout_iter_get_line_extents (iter, &rect, NULL);
    if (point_in_pango_rect (&rect, p))
      {
        do {
          pango_layout_iter_get_char_extents (iter, &rect);
          if (point_in_pango_rect (&rect, p))
            {
              pango_layout_iter_free (iter);
              return TRUE;
            }
        } while (pango_layout_iter_next_char (iter));
      }
  } while (pango_layout_iter_next_line (iter));

  pango_layout_iter_free (iter);

  return FALSE;
}

static void
pick_text (SvgElement   *self,
           PaintContext *context)
{
  g_assert (svg_element_type_is_text (self->type));

  for (unsigned int i = 0; i < self->text->len; i++)
    {
      TextNode *node = &g_array_index (self->text, TextNode, i);

      if (context->picking.done)
        break;

      switch (node->type)
        {
        case TEXT_NODE_SHAPE:
          {
            if (svg_enum_get (svg_element_get_current_value (node->shape.shape, SVG_PROPERTY_DISPLAY)) == DISPLAY_NONE)
              continue;

            pick_text (node->shape.shape, context);
          }
          break;
        case TEXT_NODE_CHARACTERS:
          {
            GskTransform *transform;
            transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (node->characters.x, node->characters.y));
            push_transform (context, transform);
            gsk_transform_unref (transform);
            transform = gsk_transform_rotate (NULL, node->characters.r);
            push_transform (context, transform);
            gsk_transform_unref (transform);
            if (point_in_layout (node->characters.layout, &context->picking.p))
              {
                context->picking.picked = self;
                context->picking.done = TRUE;
              }
            pop_transform (context);
            pop_transform (context);
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
render_image (SvgElement   *shape,
              PaintContext *context)
{
  SvgValue *href = svg_element_get_current_value (shape, SVG_PROPERTY_HREF);
  SvgValue *cf = svg_element_get_current_value (shape, SVG_PROPERTY_CONTENT_FIT);
  SvgValue *overflow = svg_element_get_current_value (shape, SVG_PROPERTY_OVERFLOW);
  graphene_rect_t vb;
  double sx, sy, tx, ty;
  double x, y, width, height;
  GdkTexture *texture;

  if (context->picking.picking && context->picking.done)
    return;

  if (svg_href_get_texture (href) == NULL)
    {
      gtk_svg_rendering_error (context->svg,
                               "No content for <image>");
      return;
    }

  texture = svg_href_get_texture (href);
  graphene_rect_init (&vb,
                      0, 0,
                      gdk_texture_get_width (texture),
                      gdk_texture_get_height (texture));

  x = context->viewport->origin.x + svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_X), context->viewport->size.width);
  y = context->viewport->origin.y + svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_Y), context->viewport->size.height);
  width = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_WIDTH), context->viewport->size.width);
  height = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_HEIGHT), context->viewport->size.height);

  compute_viewport_transform (svg_content_fit_is_none (cf),
                              svg_content_fit_get_align_x (cf),
                              svg_content_fit_get_align_y (cf),
                              svg_content_fit_get_meet (cf),
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

  if (context->picking.picking)
    {
      switch (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_POINTER_EVENTS)))
        {
        case POINTER_EVENTS_AUTO:
        case POINTER_EVENTS_VISIBLE_PAINTED:
        case POINTER_EVENTS_VISIBLE_FILL:
        case POINTER_EVENTS_VISIBLE_STROKE:
        case POINTER_EVENTS_VISIBLE:
          if (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_VISIBILITY)) == VISIBILITY_HIDDEN)
            break;
          G_GNUC_FALLTHROUGH;

        case POINTER_EVENTS_BOUNDING_BOX:
        case POINTER_EVENTS_PAINTED:
        case POINTER_EVENTS_FILL:
        case POINTER_EVENTS_STROKE:
        case POINTER_EVENTS_ALL:
          if (gsk_rect_contains_point (&vb, &context->picking.p))
            {
              context->picking.picked = shape;
              context->picking.done = TRUE;
            }
          break;

        case POINTER_EVENTS_NONE:
          break;
        default:
          g_assert_not_reached ();
        }

    }
  else
    gtk_snapshot_append_texture (context->snapshot, texture, &vb);

  pop_transform (context);
  gtk_snapshot_restore (context->snapshot);

  if (svg_enum_get (overflow) == OVERFLOW_HIDDEN)
    gtk_snapshot_pop (context->snapshot);
}

/* }}} */

static gboolean
shape_is_degenerate (SvgElement *shape)
{
  if (svg_element_get_type (shape) == SVG_ELEMENT_RECT)
    return svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_WIDTH), 1) <= 0 ||
           svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_HEIGHT), 1) <= 0;
  else if (svg_element_get_type (shape) == SVG_ELEMENT_CIRCLE)
    return svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_R), 1) <= 0;
  else if (svg_element_get_type (shape) == SVG_ELEMENT_ELLIPSE)
    return (!svg_value_is_auto (svg_element_get_current_value (shape, SVG_PROPERTY_RX)) &&
            svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_RX), 1) <= 0) ||
           (!svg_value_is_auto (svg_element_get_current_value (shape, SVG_PROPERTY_RY)) &&
            svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_RY), 1) <= 0);
  else
    return FALSE;
}

static void
recompute_current_values (SvgElement   *shape,
                          SvgElement   *parent,
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

static void
paint_shape (SvgElement   *shape,
             PaintContext *context)
{
  GskPath *path;

  if (context->picking.picking &&
      (context->picking.done ||
       context->picking.clipped == shape))
    return;

  if (svg_element_get_type (shape) == SVG_ELEMENT_USE)
    {
      if (shape->shapes->len > 0)
        {
          SvgElement *use_shape = g_ptr_array_index (shape->shapes, 0);

          push_ctx_shape (context, shape);
          render_shape (use_shape, context);
          pop_ctx_shape (context);
        }

      return;
    }

  if (svg_element_get_type (shape) == SVG_ELEMENT_TEXT)
    {
      TextAnchor anchor;
      WritingMode wmode;
      graphene_rect_t bounds;
      float dx, dy;
      GskTransform *transform = NULL;

      if (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_DISPLAY)) == DISPLAY_NONE)
        return;

      if (!context->picking.picking)
        {
          if (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_VISIBILITY)) == VISIBILITY_HIDDEN)
            return;
        }

      anchor = svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_TEXT_ANCHOR));
      wmode = svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_WRITING_MODE));
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

      if (context->picking.picking &&
          svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_POINTER_EVENTS)) == POINTER_EVENTS_BOUNDING_BOX)
        {
          if (gsk_rect_contains_point (&shape->bounds, &context->picking.p))
            {
              context->picking.picked = shape;
              context->picking.done = TRUE;
            }
          return;
        }

      gtk_snapshot_save (context->snapshot);
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (dx, dy));

      gtk_snapshot_transform (context->snapshot, transform);
      push_transform (context, transform);
      gsk_transform_unref (transform);

      if (context->picking.picking)
        {
          switch (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_POINTER_EVENTS)))
            {
            case POINTER_EVENTS_NONE:
              break;

            case POINTER_EVENTS_AUTO:
            case POINTER_EVENTS_VISIBLE_PAINTED:
              if (svg_paint_get_kind (svg_element_get_current_value (shape, SVG_PROPERTY_FILL)) == PAINT_NONE &&
                  svg_paint_get_kind (svg_element_get_current_value (shape, SVG_PROPERTY_STROKE)) == PAINT_NONE)
                break;
              G_GNUC_FALLTHROUGH;

            case POINTER_EVENTS_VISIBLE_FILL:
            case POINTER_EVENTS_VISIBLE_STROKE:
            case POINTER_EVENTS_VISIBLE:
              if (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_VISIBILITY)) == VISIBILITY_HIDDEN)
                break;
              G_GNUC_FALLTHROUGH;

            case POINTER_EVENTS_FILL:
            case POINTER_EVENTS_STROKE:
            case POINTER_EVENTS_ALL:
              pick_text (shape, context);
              break;

            case POINTER_EVENTS_PAINTED:
              if (svg_paint_get_kind (svg_element_get_current_value (shape, SVG_PROPERTY_FILL)) == PAINT_NONE &&
                  svg_paint_get_kind (svg_element_get_current_value (shape, SVG_PROPERTY_STROKE)) == PAINT_NONE)
                break;
              pick_text (shape, context);
              break;

            case POINTER_EVENTS_BOUNDING_BOX: /* handled earlier */
            default:
              g_assert_not_reached ();
            }
        }
      else if (context->op == CLIPPING)
        {
          SvgValue *paint = svg_paint_new_black ();
          fill_text (shape, context, paint, &bounds);
          svg_value_unref (paint);
        }
      else
        {
          GskStroke *stroke = shape_create_stroke (shape, context);

          switch (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_PAINT_ORDER)))
            {
            case PAINT_ORDER_FILL_STROKE_MARKERS:
            case PAINT_ORDER_FILL_MARKERS_STROKE:
            case PAINT_ORDER_MARKERS_FILL_STROKE:
              fill_text (shape, context, svg_element_get_current_value (shape, SVG_PROPERTY_FILL), &bounds);
              break;
            case PAINT_ORDER_STROKE_FILL_MARKERS:
            case PAINT_ORDER_STROKE_MARKERS_FILL:
            case PAINT_ORDER_MARKERS_STROKE_FILL:
              stroke_text (shape, context, svg_element_get_current_value (shape, SVG_PROPERTY_STROKE), stroke, &bounds);
              break;
            default:
              g_assert_not_reached ();
            }

          switch (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_PAINT_ORDER)))
            {
            case PAINT_ORDER_FILL_STROKE_MARKERS:
            case PAINT_ORDER_FILL_MARKERS_STROKE:
            case PAINT_ORDER_MARKERS_FILL_STROKE:
              stroke_text (shape, context, svg_element_get_current_value (shape, SVG_PROPERTY_STROKE), stroke, &bounds);
              break;
            case PAINT_ORDER_STROKE_FILL_MARKERS:
            case PAINT_ORDER_STROKE_MARKERS_FILL:
            case PAINT_ORDER_MARKERS_STROKE_FILL:
              fill_text (shape, context, svg_element_get_current_value (shape, SVG_PROPERTY_FILL), &bounds);
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

  if (svg_element_get_type (shape) == SVG_ELEMENT_IMAGE)
    {
      render_image (shape, context);
      return;
    }

  if (shape->shapes)
    {
      if (context->picking.picking)
        {
          for (int i = 0; i < shape->shapes->len; i++)
            {
              SvgElement *s = g_ptr_array_index (shape->shapes, shape->shapes->len - 1 - i);

              if (context->picking.done)
                break;

              render_shape (s, context);

              if (svg_element_get_type (shape) == SVG_ELEMENT_SWITCH &&
                  !svg_element_conditionally_excluded (s, context->svg))
                break;
            }
        }
      else
        {
          for (int i = 0; i < shape->shapes->len; i++)
            {
              SvgElement *s = g_ptr_array_index (shape->shapes, i);

              render_shape (s, context);

              if (svg_element_get_type (shape) == SVG_ELEMENT_SWITCH &&
                  !svg_element_conditionally_excluded (s, context->svg))
                break;
            }
        }

      return;
    }

  if (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_VISIBILITY)) == VISIBILITY_HIDDEN)
    return;

  if (shape_is_degenerate (shape))
    return;

  if (context->picking.picking)
    {
      if (svg_element_contains (shape, context->viewport, context->svg, &context->picking.p))
        {
          if (!context->picking.done)
            {
              context->picking.picked = shape;
              context->picking.done = TRUE;
            }
        }

      return;
    }

  /* Below is where we render *actual* content (i.e. graphical
   * shapes that have paths). This involves filling, stroking
   * and placing markers, in the order determined by paint-order.
   * Unless we are clipping, in which case we just fill to
   * create a 1-bit mask of the 'raw geometry'.
   */
  path = svg_element_get_current_path (shape, context->viewport);

  if (context->op == CLIPPING)
    {
      graphene_rect_t bounds;

      if (gsk_path_get_bounds (path, &bounds))
        {
          GdkColor color = GDK_COLOR_SRGB (0, 0, 0, 1);
          GskFillRule clip_rule;

          clip_rule = svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_CLIP_RULE));
          gtk_snapshot_push_fill (context->snapshot, path, clip_rule);
          gtk_snapshot_add_color (context->snapshot, &color, &bounds);
          gtk_snapshot_pop (context->snapshot);
        }
    }
  else
    {
      SvgValue *paint = svg_element_get_current_value (shape, SVG_PROPERTY_FILL);
      double opacity = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_FILL_OPACITY), 1);

      switch (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_PAINT_ORDER)))
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

      switch (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_PAINT_ORDER)))
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

      switch (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_PAINT_ORDER)))
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
display_property_applies_to (SvgElement *shape)
{
  return svg_element_get_type (shape) != SVG_ELEMENT_MASK &&
         svg_element_get_type (shape) != SVG_ELEMENT_CLIP_PATH &&
         svg_element_get_type (shape) != SVG_ELEMENT_MARKER &&
         svg_element_get_type (shape) != SVG_ELEMENT_SYMBOL;
}

static void
render_shape (SvgElement   *shape,
              PaintContext *context)
{
  gboolean op_changed;

  if (svg_element_get_type (shape) == SVG_ELEMENT_DEFS ||
      svg_element_get_type (shape) == SVG_ELEMENT_LINEAR_GRADIENT ||
      svg_element_get_type (shape) == SVG_ELEMENT_RADIAL_GRADIENT)
    return;

  if (svg_element_type_never_rendered (svg_element_get_type (shape)))
    {
      if (!((svg_element_get_type (shape) == SVG_ELEMENT_SYMBOL && shape_is_use_target (shape)) ||
           (svg_element_get_type (shape) == SVG_ELEMENT_CLIP_PATH && context->op == CLIPPING && context->op_changed) ||
           (svg_element_get_type (shape) == SVG_ELEMENT_MASK && context->op == MASKING && context->op_changed) ||
           (svg_element_get_type (shape) == SVG_ELEMENT_MARKER && context->op == MARKERS && context->op_changed)))
        return;
    }

  if (display_property_applies_to (shape))
    {
      if (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_DISPLAY)) == DISPLAY_NONE)
        return;
    }

  if (svg_element_conditionally_excluded (shape, context->svg))
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
  if (self->node == NULL)
    return FALSE;

  if (self->style_changed)
    {
      dbg_print ("cache", "Can't reuse rendernode: %s", "style change");
      return FALSE;
    }

  if (self->view_changed)
    {
      dbg_print ("cache", "Can't reuse rendernode: %s", "view change");
      return FALSE;
    }

  if (self->state != self->node_for.state)
    {
      dbg_print ("cache", "Can't reuse rendernode: %s", "state change");
      return FALSE;
    }

  if (self->current_time != self->node_for.time)
    {
      dbg_print ("cache", "Can't reuse rendernode: %s", "current_time change");
      return FALSE;
    }

  if ((width != self->node_for.width || height != self->node_for.height))
    {
      dbg_print ("cache", "Can't reuse rendernode: %s", "size change");
      return FALSE;
    }

  if ((self->used & GTK_SVG_USES_STROKES) != 0 &&
      self->weight < 1 &&
      weight != self->node_for.weight)
    {
      dbg_print ("cache", "Can't reuse rendernode: %s", "stroke weight change");
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
          dbg_print ("cache", "Can't reuse rendernode: %s", "symbolic foreground change");
          return FALSE;
        }

      if ((self->used & GTK_SVG_USES_SYMBOLIC_ERROR) != 0 &&
          self->node_for.n_colors > GTK_SYMBOLIC_COLOR_ERROR &&
          n_colors > GTK_SYMBOLIC_COLOR_ERROR &&
          !gdk_rgba_equal (&self->node_for.colors[GTK_SYMBOLIC_COLOR_ERROR],
                          &colors[GTK_SYMBOLIC_COLOR_ERROR]))
        {
          dbg_print ("cache", "Can't reuse rendernode: %s", "symbolic error change");
          return FALSE;
        }

      if ((self->used & GTK_SVG_USES_SYMBOLIC_WARNING) != 0 &&
          self->node_for.n_colors > GTK_SYMBOLIC_COLOR_WARNING &&
          n_colors > GTK_SYMBOLIC_COLOR_WARNING &&
          !gdk_rgba_equal (&self->node_for.colors[GTK_SYMBOLIC_COLOR_WARNING],
                          &colors[GTK_SYMBOLIC_COLOR_WARNING]))
        {
          dbg_print ("cache", "Can't reuse rendernode: %s", "symbolic warning change");
          return FALSE;
        }

      if ((self->used & GTK_SVG_USES_SYMBOLIC_SUCCESS) != 0 &&
          self->node_for.n_colors > GTK_SYMBOLIC_COLOR_SUCCESS &&
          n_colors > GTK_SYMBOLIC_COLOR_SUCCESS &&
          !gdk_rgba_equal (&self->node_for.colors[GTK_SYMBOLIC_COLOR_SUCCESS],
                          &colors[GTK_SYMBOLIC_COLOR_SUCCESS]))
        {
          dbg_print ("cache", "Can't reuse rendernode: %s", "symbolic success change");
          return FALSE;
        }

      if ((self->used & GTK_SVG_USES_SYMBOLIC_ACCENT) != 0 &&
          self->node_for.n_colors > GTK_SYMBOLIC_COLOR_ACCENT &&
          n_colors > GTK_SYMBOLIC_COLOR_ACCENT &&
          !gdk_rgba_equal (&self->node_for.colors[GTK_SYMBOLIC_COLOR_ACCENT],
                          &colors[GTK_SYMBOLIC_COLOR_ACCENT]))
        {
          dbg_print ("cache", "Can't reuse rendernode: %s", "symbolic accent change");
          return FALSE;
        }
    }

  return TRUE;
}

SvgElement *
gtk_svg_pick_element (GtkSvg                 *self,
                      const graphene_point_t *p)
{
  SvgComputeContext compute_context;
  PaintContext paint_context;
  graphene_rect_t viewport;
  GtkSnapshot *snapshot;
  GskRenderNode *node;

  if (self->width < 0 || self->height < 0)
    return NULL;

  viewport = GRAPHENE_RECT_INIT (0, 0, self->current_width, self->current_height);

  compute_context.svg = self;
  compute_context.viewport = &viewport;
  compute_context.colors = self->node_for.colors;
  compute_context.n_colors = self->node_for.n_colors;
  compute_context.current_time = self->current_time;
  compute_context.parent = NULL;
  compute_context.interpolation = GDK_COLOR_STATE_SRGB;

  compute_current_values_for_shape (self->content, &compute_context);

  snapshot = gtk_snapshot_new ();

  paint_context.svg = self;
  paint_context.viewport = &viewport;
  paint_context.viewport_stack = NULL;
  paint_context.snapshot = snapshot;
  paint_context.colors = self->node_for.colors;
  paint_context.n_colors = self->node_for.n_colors;
  paint_context.weight = self->node_for.weight;
  paint_context.op = RENDERING;
  paint_context.op_stack = NULL;
  paint_context.ctx_shape_stack = NULL;
  paint_context.current_time = self->current_time;
  paint_context.depth = 0;
  paint_context.transforms = NULL;
  paint_context.instance_count = 0;
  paint_context.picking.picking = TRUE;
  paint_context.picking.p = *p;
  paint_context.picking.points = NULL;
  paint_context.picking.done = FALSE;
  paint_context.picking.picked = NULL;

  if (self->overflow == GTK_OVERFLOW_HIDDEN)
    gtk_snapshot_push_clip (snapshot, &viewport);

  render_shape (self->content, &paint_context);

  if (self->overflow == GTK_OVERFLOW_HIDDEN)
    gtk_snapshot_pop (snapshot);

  node = gtk_snapshot_free_to_node (snapshot);
  g_clear_pointer (&node, gsk_render_node_unref);

  return paint_context.picking.picked;
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
       * not rendering an animation properly, we just do a snapshot.
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

      if (self->style_changed)
        {
          apply_styles_to_shape (self->content, self);
          self->style_changed = FALSE;
        }

      if (self->view_changed)
        {
          apply_view (self->content, self->view);
          self->view_changed = FALSE;
        }

      /* Traditional symbolics often have overlapping shapes,
       * causing things to look wrong when using colors with
       * alpha. To work around that, we always draw them with
       * solid colors and apply foreground opacity globally.
       *
       * Non-symbolic icons are responsible for dealing with
       * overlaps themselves, using the full svg machinery.
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
      paint_context.picking.picking = FALSE;

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

  return self->aspect_ratio;
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
  PROP_STYLESHEET,
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
  self->content = svg_element_new (NULL, SVG_ELEMENT_SVG);
  self->timeline = timeline_new ();

  self->images = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  self->user_styles = array_new_with_clear_func (sizeof (SvgCssRuleset), (GDestroyNotify) svg_css_ruleset_clear);
  self->author_styles = array_new_with_clear_func (sizeof (SvgCssRuleset), (GDestroyNotify) svg_css_ruleset_clear);
}

static void
gtk_svg_dispose (GObject *object)
{
  GtkSvg *self = GTK_SVG (object);

  frame_clock_disconnect (self);
  g_clear_handle_id (&self->pending_advance, g_source_remove);

  g_clear_pointer (&self->content, svg_element_free);
  g_clear_pointer (&self->timeline, timeline_free);
  g_clear_pointer (&self->images, g_hash_table_unref);
  g_clear_object (&self->fontmap);
  g_clear_pointer (&self->font_files, g_ptr_array_unref);
  g_clear_pointer (&self->node, gsk_render_node_unref);

  g_clear_object (&self->clock);

  g_clear_pointer (&self->author, g_free);
  g_clear_pointer (&self->license, g_free);
  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->keywords, g_free);
  g_clear_pointer (&self->resource, g_free);

  g_clear_pointer (&self->state_names, g_strfreev);

  g_clear_pointer (&self->stylesheet, g_bytes_unref);

  g_clear_pointer (&self->user_styles, g_array_unref);
  g_clear_pointer (&self->author_styles, g_array_unref);

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

    case PROP_STYLESHEET:
      g_value_set_boxed (value, self->stylesheet);
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

    case PROP_STYLESHEET:
      gtk_svg_set_stylesheet (self, g_value_get_boxed (value));
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

  svg_element_type_init ();
  svg_filter_type_init ();
  svg_properties_init ();

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
                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

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
                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

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
                          G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

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
                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

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
                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

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
                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSvg:stylesheet:
   *
   * A CSS stylesheet to apply to the SVG.
   *
   * Since: 4.24
   */
  properties[PROP_STYLESHEET] =
    g_param_spec_boxed ("stylesheet", NULL, NULL,
                        G_TYPE_BYTES,
                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

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
shape_dump_animation_state (SvgElement *shape, GString *string)
{
  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          SvgAnimation *a = g_ptr_array_index (shape->animations, i);

          if (a->status == ANIMATION_STATUS_RUNNING)
            g_string_append_printf (string, " %s", a->id);
        }
    }

  if (svg_element_type_is_container (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          SvgElement *s = g_ptr_array_index (shape->shapes, i);
          shape_dump_animation_state (s, string);
        }
    }
}

static void
animation_state_dump (GtkSvg *self)
{
  GString *s = g_string_new ("running");
  shape_dump_animation_state (self->content, s);
  dbg_print ("times", "%s", s->str);
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

  return svg_element_equal (svg1->content, svg2->content);
}

void
gtk_svg_set_activate_callback (GtkSvg             *svg,
                               SvgElementCallback  callback,
                               gpointer            data)
{
  svg->activate_callback = callback;
  svg->activate_data = data;
}

void
gtk_svg_set_hover_callback (GtkSvg             *svg,
                            SvgElementCallback  callback,
                            gpointer            data)
{
  svg->hover_callback = callback;
  svg->hover_data = data;
}

/* {{{ Animation */

static void
collect_next_update_for_animation (SvgAnimation  *a,
                                   int64_t        current_time,
                                   GtkSvgRunMode *run_mode,
                                   int64_t       *next_update)
{
  animation_update_run_mode (a, current_time);

#ifdef DEBUG
  if (a->run_mode > *run_mode)
    {
      const char *mode_name[] = { "STOPPED", "DISCRETE", "CONTINUOUS" };
      dbg_print ("run", "%s updates run mode to %s", a->id, mode_name[a->run_mode]);
    }
  if (a->next_invalidate < *next_update)
    {
      dbg_print ("run", "%s updates next update to %s", a->id, format_time (a->next_invalidate));
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
collect_next_update_for_shape (SvgElement    *shape,
                               int64_t        current_time,
                               GtkSvgRunMode *run_mode,
                               int64_t       *next_update)
{
  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          SvgAnimation *a = g_ptr_array_index (shape->animations, i);
          collect_next_update_for_animation (a, current_time, run_mode, next_update);
        }
    }

  if (svg_element_type_is_container (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          SvgElement *s = g_ptr_array_index (shape->shapes, i);
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

#ifdef DEBUG
  if (strstr (g_getenv ("SVG_DEBUG") ?: "", "continuous"))
    run_mode = GTK_SVG_RUN_MODE_CONTINUOUS;
#endif

  self->run_mode = run_mode;
  self->next_update = next_update;

#ifdef DEBUG
  const char *mode_name[] = { "STOPPED", "DISCRETE", "CONTINUOUS" };
  dbg_print ("run", "run mode %s", mode_name[self->run_mode]);
  dbg_print ("run", "next update %s", format_time (self->next_update));
#endif
}

static void
invalidate_for_next_update (GtkSvg *self)
{
  if (self->next_update <= self->current_time ||
      self->run_mode == GTK_SVG_RUN_MODE_CONTINUOUS)
    {
      dbg_print ("run", "invalidating for update");
      gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
    }
#ifdef DEBUG
  else
    {
      GString *s = g_string_new ("not invalidating (");
      g_string_append_printf (s, "%s", format_time (self->next_update));
      g_string_append_printf (s, " > %s)", format_time (self->current_time));
      dbg_print ("run", "%s", s->str);
      g_string_free (s, TRUE);
    }
#endif
}

static void
shape_update_animation_state (SvgElement *shape,
                              int64_t     current_time)
{
  if (shape->animations)
    {
      gboolean any_changed = FALSE;

      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          SvgAnimation *a = g_ptr_array_index (shape->animations, i);
          animation_update_state (a, current_time);
          any_changed |= a->state_changed;
        }

      if (any_changed)
        g_ptr_array_sort_values (shape->animations, compare_anim);
    }

  if (svg_element_type_is_container (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          SvgElement *s = g_ptr_array_index (shape->shapes, i);
          shape_update_animation_state (s, current_time);
        }
    }
}

static void
update_animation_state (GtkSvg *self)
{
  shape_update_animation_state (self->content, self->current_time);

  collect_next_update (self);
  invalidate_for_next_update (self);
  schedule_next_update (self);
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

  dbg_print ("run", "advancing current time to %s", format_time (current_time));

  self->current_time = current_time;

  update_animation_state (self);

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
#ifdef DEBUG
          time_base = self->load_time;
#endif

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
  g_clear_pointer (&self->content, svg_element_free);
  g_clear_pointer (&self->images, g_hash_table_unref);
  g_clear_object (&self->fontmap);
  g_clear_pointer (&self->font_files, g_ptr_array_unref);
  g_clear_pointer (&self->node, gsk_render_node_unref);

  self->content = svg_element_new (NULL, SVG_ELEMENT_SVG);
  self->timeline = timeline_new ();
  self->images = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  self->width = 0;
  self->height = 0;
  self->aspect_ratio = 0;
  self->current_width = 0;
  self->current_height = 0;
  self->initial_state = 0;
  self->state = 0;
  self->max_state = 0;
  self->state_change_delay = 0;
  self->used = 0;
  self->has_use_cycle = FALSE;

  self->gpa_version = 0;

  g_clear_pointer (&self->author, g_free);
  g_clear_pointer (&self->license, g_free);
  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->keywords, g_free);
  g_clear_pointer (&self->resource, g_free);

  g_clear_pointer (&self->state_names, g_strfreev);
  self->n_state_names = 0;

  g_array_set_size (self->user_styles, 0);
  g_array_set_size (self->author_styles, 0);

  /* Note: we intentionally keep the stylesheet */

  self->focus = NULL;
  self->initial_focus = NULL;
  self->view = NULL;
  self->hover = NULL;
  self->active = NULL;
}

static SvgElement *
find_filter (SvgElement *shape,
             const char *filter_id)
{
  if (svg_element_get_type (shape) == SVG_ELEMENT_FILTER)
    {
      if (g_strcmp0 (svg_element_get_id (shape), filter_id) == 0)
        return shape;
      else
        return NULL;
    }

  if (svg_element_type_is_container (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          SvgElement *sh = g_ptr_array_index (shape->shapes, i);
          SvgElement *res;

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
  SvgElement *filter;
  SvgElement *shape;
  PaintContext paint_context;
  GskRenderNode *result;
  G_GNUC_UNUSED GskRenderNode *node;

  filter = find_filter (svg->content, filter_id);
  if (!filter)
    return gsk_render_node_ref (source);

  /* This is a bit iffy. We create an extra shape,
   * and treat it as if it was part of the svg.
   */

  shape = svg_element_new (NULL, SVG_ELEMENT_RECT);

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
  paint_context.picking.picking = FALSE;

  /* This is necessary so the filter has current values.
   * Also, any other part of the svg that the filter might
   * refer to.
   */
  recompute_current_values (svg->content, NULL, &paint_context);

  /* This is necessary, so the shape itself has current values */
  recompute_current_values (shape, NULL, &paint_context);

  result = apply_filter_tree (shape, filter, &paint_context, source);

  svg_element_free (shape);

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

/* {{{ Input */

void
gtk_svg_activate_element (GtkSvg     *self,
                          SvgElement *link)
{
  SvgValue *value = svg_element_get_current_value (link, SVG_PROPERTY_HREF);
  SvgAnimation *animation = svg_href_get_animation (value);
  SvgElement *target = svg_href_get_shape (value);
  int64_t current_time = get_current_time (self);

  g_assert (svg_element_get_type (link) == SVG_ELEMENT_LINK);

  if (animation)
    {
      animation_set_begin (animation, current_time);
      animation_update_state (animation, current_time);
      collect_next_update (self);
      invalidate_for_next_update (self);
      schedule_next_update (self);
    }
  else if (target && svg_element_get_type (target) == SVG_ELEMENT_VIEW)
    {
      gtk_svg_set_view (self, target);
    }
  else
    {
      if (self->activate_callback)
        self->activate_callback (link, self->activate_data);
    }

  /* For links, browsers treat 'click' as 'device independent'
   * and emit it regardless whether key or button press.
   */
  timeline_update_for_event (self->timeline, link, EVENT_TYPE_CLICK, current_time);
  gtk_svg_advance (self, current_time);

  if (!svg_element_get_visited (link))
    {
      svg_element_set_visited (link, TRUE);
      self->style_changed = TRUE;
      gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
    }
}

static void
gtk_svg_set_hover (GtkSvg     *self,
                   SvgElement *target)
{
  int64_t current_time = get_current_time (self);

  if (self->hover == target)
    return;

  if (self->hover)
    {
      timeline_update_for_event (self->timeline, self->hover, EVENT_TYPE_MOUSE_LEAVE, current_time);
      svg_element_set_hover (self->hover, FALSE);
    }

  if (target)
    {
      timeline_update_for_event (self->timeline, target, EVENT_TYPE_MOUSE_ENTER, current_time);
      svg_element_set_hover (target, TRUE);
    }

  self->hover = target;
  self->style_changed = TRUE;

  gtk_svg_advance (self, current_time);

  self->style_changed = TRUE;
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  if (self->hover_callback)
    self->hover_callback (self->hover, self->hover_data);
}

void
gtk_svg_set_active (GtkSvg     *self,
                    SvgElement *target)
{
  if (self->active == target)
    return;

  if (self->active)
    svg_element_set_active (self->active, FALSE);

  if (target)
    svg_element_set_active (target, TRUE);

  self->active = target;
  self->style_changed = TRUE;
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

SvgElement *
gtk_svg_get_active (GtkSvg *self)
{
  return self->active;
}

gboolean
gtk_svg_handle_event (GtkSvg   *self,
                      GdkEvent *event,
                      double    x,
                      double    y)
{
  graphene_point_t p = GRAPHENE_POINT_INIT (x, y);
  SvgElement *target;

  /* Don't handle events before we're loaded */
  if (self->load_time == INDEFINITE)
    return FALSE;

  switch ((unsigned int) gdk_event_get_event_type (event))
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      if (self->focus)
        return svg_element_propagate_event (self->focus, event, self);
      break;

    case GDK_MOTION_NOTIFY:
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      gtk_svg_set_hover (self, gtk_svg_pick_element (self, &p));
      break;

    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      target = gtk_svg_pick_element (self, &p);
      if (target)
        svg_element_propagate_event (target, event, self);
      break;

    default:
      break;
    }

  if (gdk_event_get_event_type (event) == GDK_BUTTON_RELEASE &&
      gdk_button_event_get_button (event) == GDK_BUTTON_PRIMARY &&
      gtk_svg_get_active (self) != NULL)
    {
      gtk_svg_set_active (self, NULL);
    }

  return FALSE;
}

void
gtk_svg_handle_crossing (GtkSvg                *self,
                         const GtkCrossingData *crossing,
                         double                 x,
                         double                 y)
{
  /* Don't handle events before we're loaded */
  if (self->load_time == INDEFINITE)
    return;

  if (crossing->type == GTK_CROSSING_FOCUS ||
      crossing->type == GTK_CROSSING_ACTIVE)
    {
      if (crossing->direction == GTK_CROSSING_IN)
        gtk_svg_grab_focus (self);
      else
        gtk_svg_lose_focus (self);
    }
  else if (crossing->type == GTK_CROSSING_POINTER)
    {
      if (crossing->direction == GTK_CROSSING_IN)
        gtk_svg_set_hover (self, gtk_svg_pick_element (self, &GRAPHENE_POINT_INIT (x, y)));
      else
        {
          gtk_svg_set_hover (self, NULL);
          gtk_svg_set_active (self, NULL);
        }
    }
}

gboolean
gtk_svg_grab_focus (GtkSvg *self)
{
  SvgElement *focus = NULL;
  int64_t current_time = get_current_time (self);

  /* Don't handle events before we're loaded */
  if (self->load_time == INDEFINITE)
    return FALSE;

  if (self->focus)
    return TRUE;

  if (self->initial_focus)
    {
      focus = self->initial_focus;
      self->initial_focus = NULL;
    }
  else
    {
      do
        {
          if (focus)
            focus = svg_element_next (focus);
          else
            focus = self->content;

          if (focus && svg_element_get_focusable (focus))
            break;
        }
      while (focus);
    }

  if (!focus)
    return FALSE;

  svg_element_set_focus (focus, TRUE);

  timeline_update_for_event (self->timeline, focus, EVENT_TYPE_FOCUS, current_time);
  gtk_svg_advance (self, current_time);

  self->focus = focus;

  self->style_changed = TRUE;
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  return TRUE;
}

gboolean
gtk_svg_lose_focus (GtkSvg *self)
{
  int64_t current_time = get_current_time (self);

  /* Don't handle events before we're loaded */
  if (self->load_time == INDEFINITE)
    return FALSE;

  if (!self->focus)
    return TRUE;

  svg_element_set_focus (self->focus, FALSE);

  timeline_update_for_event (self->timeline, self->focus, EVENT_TYPE_BLUR, current_time);
  gtk_svg_advance (self, current_time);

  self->focus = NULL;

  self->style_changed = TRUE;
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  return TRUE;
}

gboolean
gtk_svg_move_focus (GtkSvg           *self,
                    GtkDirectionType  direction)
{
  SvgElement *focus = NULL;
  int64_t current_time;

  /* Don't handle events before we're loaded */
  if (self->load_time == INDEFINITE)
    return FALSE;

  if (direction == GTK_DIR_TAB_FORWARD)
    {
      SvgElement *next = self->focus;

      do
        {
          if (next)
            next = svg_element_next (next);
          else
            next = svg_element_first (self->content);

          if (next == NULL || svg_element_get_focusable (next))
            {
              focus = next;
              break;
            }
        }
      while (next);
    }
  else if (direction == GTK_DIR_TAB_BACKWARD)
    {
      SvgElement *next = self->focus;

      do
        {
          if (next)
            next = svg_element_previous (next);
          else
            next = svg_element_last (self->content);

          if (next == NULL || svg_element_get_focusable (next))
            {
              focus = next;
              break;
            }
        } while (next);
    }
  else
    {
      focus = self->focus;
    }

  if (self->focus == focus)
    return FALSE;

  current_time = get_current_time (self);

  if (self->focus)
    {
      timeline_update_for_event (self->timeline, self->focus, EVENT_TYPE_BLUR, current_time);
      svg_element_set_focus (self->focus, FALSE);
    }

  if (focus)
    {
      timeline_update_for_event (self->timeline, focus, EVENT_TYPE_FOCUS, current_time);
      svg_element_set_focus (focus, TRUE);
    }

  gtk_svg_advance (self, current_time);

  self->focus = focus;
  self->style_changed = TRUE;
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  return self->focus != NULL;
}

/* }}} */
/* {{{ Views */

void
gtk_svg_set_view (GtkSvg     *self,
                  SvgElement *view)
{
  g_assert (view == NULL || view->type == SVG_ELEMENT_VIEW);

  if (self->view == view)
    return;

  self->view = view;
  self->view_changed = TRUE;

  determine_size (self);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

/* }}} */
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
      int64_t current_time = get_current_time (self);
      dbg_print ("state", "renderer state %u -> %u", previous_state, state);

      timeline_update_for_state (self->timeline,
                                 previous_state, self->state,
                                 current_time + self->state_change_delay);

      gtk_svg_advance (self, current_time);

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
  g_return_val_if_fail (GTK_IS_SVG (self), 0);

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

/**
 * gtk_svg_set_stylesheet:
 * @self: an SVG paintable
 * @bytes: (nullable): CSS data
 *
 * Sets a CSS user stylesheet to use.
 *
 * Note that styles are applied at load time,
 * so this function must be called before
 * loading SVG.
 *
 * Since: 4.24
 */
void
gtk_svg_set_stylesheet (GtkSvg *self,
                        GBytes *bytes)
{
  g_return_if_fail (GTK_IS_SVG (self));

  g_clear_pointer (&self->stylesheet, g_bytes_unref);
  if (bytes)
    self->stylesheet = g_bytes_ref (bytes);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STYLESHEET]);
}

/**
 * gtk_svg_get_stylesheet:
 * @self: an SVG paintable
 *
 * Gets the CSS user stylesheet.
 *
 * Returns: (nullable): a `GBytes` with the CSS data
 *
 * Since: 4.24
 */
GBytes *
gtk_svg_get_stylesheet (GtkSvg *self)
{
  g_return_val_if_fail (GTK_IS_SVG (self), NULL);

  return self->stylesheet;
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
 * SvgAnimations can be paused and started repeatedly.
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
 * SvgAnimations can be paused and started repeatedly.
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
/* }}} */

/* vim:set foldmethod=marker: */
