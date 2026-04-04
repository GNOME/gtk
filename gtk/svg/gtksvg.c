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

#define SECRET
#include "gtksvgprivate.h"

#include "gtkenums.h"
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
#define DEBUG
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
  };
  int64_t time;
  GPtrArray *animations;
};

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
                 TimeSpec      *spec,
                 GError       **error)
{
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
          TimeSpecSide side;

          spec->type = TIME_SPEC_TYPE_SYNC;

          if (gtk_css_parser_try_ident (parser, "begin"))
            side = TIME_SPEC_SIDE_BEGIN;
          else if (gtk_css_parser_try_ident (parser, "end"))
            side = TIME_SPEC_SIDE_END;
          else
            {
              g_free (id);
              return FALSE;
            }

          spec->sync.side = side;
          spec->sync.ref = id;
          spec->sync.base = NULL;

          gtk_css_parser_skip_whitespace (parser);
          parser_try_duration (parser, &spec->offset);
        }
      else
        return FALSE;
    }
  else
    return FALSE;

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
        g_string_append (s, "StateChange(");
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
      if (!svg_element_property_is_set (shape, attr))
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
      else if (svg_value_is_inherit (svg_element_get_base_value (shape, attr)))
        {
          if (parent)
            return svg_value_ref (svg_element_get_current_value (parent, attr));
          else
            return svg_property_ref_initial_value (attr, svg_element_get_type (shape), parent != NULL);
        }
      else if (svg_value_is_initial (svg_element_get_base_value (shape, attr)))
        {
          return svg_property_ref_initial_value (attr, svg_element_get_type (shape), parent != NULL);
        }
      else
        {
          return svg_value_ref (svg_element_get_base_value (shape, attr));
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
      if (!svg_color_stop_property_is_set (stop, attr))
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
      if (!svg_filter_property_is_set (f, attr))
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
  if (self->clock == NULL || !self->playing)
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
mark_as_computed_for_use (SvgElement *shape,
                          gboolean    computed_for_use)
{
  shape->computed_for_use = computed_for_use;

  if (svg_element_type_is_container (svg_element_get_type (shape)))
    {
      for (SvgElement *sh = shape->first; sh; sh = sh->next)
        mark_as_computed_for_use (sh, computed_for_use);
    }
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

  if (svg_element_type_is_container (svg_element_get_type (shape)))
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

  if (svg_element_property_is_set (shape, SVG_PROPERTY_VISIBILITY))
    initial_visibility = svg_enum_get (svg_element_get_base_value (shape, SVG_PROPERTY_VISIBILITY));
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
  svg_element_set_base_value (filter, SVG_PROPERTY_X, value);
  svg_element_set_base_value (filter, SVG_PROPERTY_Y, value);
  svg_value_unref (value);
  value = svg_percentage_new (200);
  svg_element_set_base_value (filter, SVG_PROPERTY_WIDTH, value);
  svg_element_set_base_value (filter, SVG_PROPERTY_HEIGHT, value);
  svg_value_unref (value);

  f = svg_filter_new (filter, SVG_FILTER_BLUR);
  svg_element_add_filter (filter, f);
  str = g_strdup_printf ("gpa:morph-filter:%s:blurred", svg_element_get_id (shape));
  value = svg_string_new_take (str);
  svg_filter_set_base_value (f, SVG_PROPERTY_FE_RESULT, value);
  svg_value_unref (value);

  create_transition (filter, filter->filters->len, timeline, states,
                     duration, delay, easing,
                     0, GPA_TRANSITION_MORPH,
                     SVG_PROPERTY_FE_STD_DEV,
                     svg_numbers_new1 (4),
                     svg_numbers_new1 (0));

  f = svg_filter_new (filter, SVG_FILTER_COMPONENT_TRANSFER);
  svg_element_add_filter (filter, f);
  value = svg_filter_ref_new (FILTER_REF_SOURCE_GRAPHIC);
  svg_filter_set_base_value (f, SVG_PROPERTY_FE_IN, value);
  svg_value_unref (value);

  f = svg_filter_new (filter, SVG_FILTER_FUNC_A);
  svg_element_add_filter (filter, f);
  value = svg_component_transfer_type_new (COMPONENT_TRANSFER_LINEAR);
  svg_filter_set_base_value (f, SVG_PROPERTY_FE_FUNC_TYPE, value);
  svg_value_unref (value);
  value = svg_number_new (100);
  svg_filter_set_base_value (f, SVG_PROPERTY_FE_FUNC_SLOPE, value);
  svg_value_unref (value);
  value = svg_number_new (0);
  svg_filter_set_base_value (f, SVG_PROPERTY_FE_FUNC_INTERCEPT, value);
  svg_value_unref (value);

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
      value = svg_component_transfer_type_new (COMPONENT_TRANSFER_LINEAR);
      svg_filter_set_base_value (f, SVG_PROPERTY_FE_FUNC_TYPE, value);
      svg_value_unref (value);
      value = svg_number_new (0);
      svg_filter_set_base_value (f, SVG_PROPERTY_FE_FUNC_SLOPE, value);
      svg_value_unref (value);
      value = svg_number_new (1);
      svg_filter_set_base_value (f, SVG_PROPERTY_FE_FUNC_INTERCEPT, value);
      svg_value_unref (value);
    }

  f = svg_filter_new (filter, SVG_FILTER_FUNC_A);
  svg_element_add_filter (filter, f);
  value = svg_component_transfer_type_new (COMPONENT_TRANSFER_LINEAR);
  svg_filter_set_base_value (f, SVG_PROPERTY_FE_FUNC_TYPE, value);
  svg_value_unref (value);

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
  value = svg_composite_operator_new (COMPOSITE_OPERATOR_ARITHMETIC);
  svg_filter_set_base_value (f, SVG_PROPERTY_FE_COMPOSITE_OPERATOR, value);
  svg_value_unref (value);
  value = svg_number_new (1);
  svg_filter_set_base_value (f, SVG_PROPERTY_FE_COMPOSITE_K1, value);
  svg_value_unref (value);
  str = g_strdup_printf ("gpa:morph-filter:%s:blurred", svg_element_get_id (shape));
  value = svg_filter_ref_new_ref (str);
  g_free (str);
  svg_filter_set_base_value (f, SVG_PROPERTY_FE_IN2, value);
  svg_value_unref (value);

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
      g_hash_table_add (pending_refs, shape);
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
/* {{{ Parser */

/* The parser creates the shape tree. We maintain a current shape,
 * and a current animation. Some things are done in a post-processing
 * step: finding the shape that an animation belongs to, resolving
 * other kinds of shape references, determining the proper order
 * for computing updated values.
 *
 * Note that we treat images, text, markers, gradients, filters as
 * shapes, but not color stops and filter primitives. SvgAnimations
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
  SvgElement *current_shape;
  GSList *shape_stack;
  GHashTable *shapes;
  GHashTable *animations;
  SvgAnimation *current_animation;
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
  GArray *rulesets_important;
} ParserData;

/* {{{ SvgAnimation attributes */

typedef struct
{
  GtkSvg *svg;
  GArray *array;
  GError *error;
} Specs;

static gboolean
time_spec_parse_one (GtkCssParser *parser,
                     gpointer      user_data)
{
  Specs *specs = user_data;
  TimeSpec *spec;

  g_array_set_size (specs->array, specs->array->len + 1);
  spec = &g_array_index (specs->array, TimeSpec, specs->array->len - 1);

  return time_spec_parse (parser, specs->svg, spec, &specs->error);
}

static gboolean
parse_base_animation_attrs (SvgAnimation         *a,
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
  SvgProperty attr;
  SvgElement *current_shape = NULL;
  SvgElementType current_type;
  SvgFilterType filter_type = 0;
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

  current_type = svg_element_get_type (current_shape);

  if (begin_attr)
    {
      GtkCssParser *parser = parser_new_for_string (begin_attr);
      Specs specs = { 0, };

      specs.svg = data->svg;
      specs.array = g_array_new (FALSE, TRUE, sizeof (TimeSpec));
      g_array_set_clear_func (specs.array, (GDestroyNotify) time_spec_clear);

      if (parser_parse_list (parser, time_spec_parse_one, &specs))
        {
          for (unsigned int i = 0; i < specs.array->len; i++)
            {
              TimeSpec *spec = &g_array_index (specs.array, TimeSpec, i);
              TimeSpec *begin;

              a->has_begin = 1;
              begin = svg_animation_add_begin (a, timeline_get_time_spec (data->svg->timeline, spec));
              time_spec_add_animation (begin, a);
            }
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "begin", NULL);
        }

      gtk_css_parser_unref (parser);
      g_array_unref (specs.array);

      if (specs.error)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "begin", "%s", specs.error->message);
          g_error_free (specs.error);
        }
    }
  else
    {
      TimeSpec *begin;
      begin = svg_animation_add_begin (a, timeline_get_start_of_time (data->svg->timeline));
      time_spec_add_animation (begin, a);
    }

  if (end_attr)
    {
      GtkCssParser *parser = parser_new_for_string (end_attr);
      Specs specs = { 0, };

      specs.svg = data->svg;
      specs.array = g_array_new (FALSE, TRUE, sizeof (TimeSpec));
      g_array_set_clear_func (specs.array, (GDestroyNotify) time_spec_clear);

      if (parser_parse_list (parser, time_spec_parse_one, &specs))
        {
          for (unsigned int i = 0; i < specs.array->len; i++)
            {
              TimeSpec *spec = &g_array_index (specs.array, TimeSpec, i);
              TimeSpec *end;

              a->has_end = 1;
              end = svg_animation_add_end (a, timeline_get_time_spec (data->svg->timeline, spec));
              time_spec_add_animation (end, a);
            }
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "end", NULL);
        }

      gtk_css_parser_unref (parser);
      g_array_unref (specs.array);

      if (specs.error)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "end", "%s", specs.error->message);
          g_error_free (specs.error);
        }
    }
  else
    {
      TimeSpec *end;
      end = svg_animation_add_end (a, timeline_get_end_of_time (data->svg->timeline));
      time_spec_add_animation (end, a);
    }

  a->simple_duration = INDEFINITE;
  if (dur_attr)
    {
      a->has_simple_duration = 1;
      if (!parse_duration (dur_attr, TRUE, &a->simple_duration))
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "dur", NULL);
          a->has_simple_duration = 0;
        }
    }

  a->repeat_count = REPEAT_FOREVER;
  if (repeat_count_attr)
    {
      a->has_repeat_count = 1;
      if (!parse_number_or_named (repeat_count_attr, 0, DBL_MAX, "indefinite", REPEAT_FOREVER, &a->repeat_count))
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "repeatCount", NULL);
          a->has_repeat_count = 0;
        }
    }

  a->repeat_duration = INDEFINITE;
  if (repeat_dur_attr)
    {
      a->has_repeat_duration = 1;
      if (!parse_duration (repeat_dur_attr, TRUE, &a->repeat_duration))
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
      current_type == SVG_ELEMENT_FILTER &&
      current_shape->filters->len > 0)
    {
      SvgFilter *f;

      f = g_ptr_array_index (current_shape->filters,
                              current_shape->filters->len - 1);
      filter_type = svg_filter_get_type (f);
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
          expected = svg_property_get_presentation (SVG_PROPERTY_TRANSFORM, current_type);
          if (expected == NULL)
            gtk_svg_invalid_attribute (data->svg, context, attr_names, "attributeName",
                                       "No transform attribute");
          else if (attr_name_attr && strcmp (attr_name_attr, expected) != 0)
            gtk_svg_invalid_attribute (data->svg, context, attr_names, "attributeName",
                                       "Value must be '%s'", expected);
        }

      a->attr = SVG_PROPERTY_TRANSFORM;
    }
  else if (!attr_name_attr)
    {
      gtk_svg_missing_attribute (data->svg, context, "attributeName", NULL);
      return FALSE;
    }
  /* FIXME: if href is set, current_shape might be the wrong shape */
  else if (current_shape != NULL &&
           ((current_type == SVG_ELEMENT_FILTER &&
             svg_property_lookup_for_filter (attr_name_attr, current_type, filter_type, &attr)) ||
            (current_type != SVG_ELEMENT_FILTER &&
             svg_property_lookup (attr_name_attr, current_type, &attr))))
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
parse_spline (GtkCssParser *parser,
              gpointer      user_data)
{
  GArray *array = user_data;

  for (unsigned int i = 0; i < 4; i++)
    {
      double v;

      gtk_css_parser_skip_whitespace (parser);

      if (!gtk_css_parser_consume_number (parser, &v))
        return FALSE;

      if (v < 0 || v > 1)
        return FALSE;

      g_array_append_val (array, v);

      skip_whitespace_and_optional_comma (parser);
    }

  return TRUE;
}

static gboolean
parse_value_animation_attrs (SvgAnimation         *a,
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

      v = svg_property_parse (SVG_PROPERTY_COLOR_INTERPOLATION, color_interpolation_attr, &error);
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
      values = svg_property_parse_values (a->attr, transform_type, values_attr);
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

      values = svg_property_parse_values (a->attr, transform_type, from_attr);
      if (!values || values->len != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "from", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }

      tovals = svg_property_parse_values (a->attr, transform_type, to_attr);
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

      values = svg_property_parse_values (a->attr, transform_type, from_attr);

      if (!values || values->len != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "from", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }

      byvals = svg_property_parse_values (a->attr, transform_type, by_attr);
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
      values = svg_property_parse_values (a->attr, transform_type, to_attr);

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

      values = svg_property_parse_values (a->attr, transform_type, by_attr);

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
      times = parse_number_list (key_times_attr, 0, 1);
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
      params = NULL;
      if (splines_attr)
        {
          GtkCssParser *parser = parser_new_for_string (splines_attr);
          unsigned int n;

          params = g_array_new (FALSE, TRUE, sizeof (double));
          if (!parser_parse_list (parser, parse_spline, params))
            {
              gtk_css_parser_unref (parser);
              gtk_svg_invalid_attribute (data->svg, context, attr_names, "keySplines", NULL);
              g_clear_pointer (&values, g_ptr_array_unref);
              g_clear_pointer (&times, g_array_unref);
              g_clear_pointer (&params, g_array_unref);
              g_clear_pointer (&points, g_array_unref);
              return FALSE;
            }
          gtk_css_parser_unref (parser);

          n = params->len / 4;

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

  svg_animation_fill_from_values (a,
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
parse_motion_animation_attrs (SvgAnimation         *a,
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
      SvgValue *value = NULL;

      if (strcmp (path_attr, "none") == 0)
        value = svg_path_new_none ();
      else
        {
          SvgPathData *path_data = NULL;

          if (svg_path_data_parse_full (path_attr, &path_data))
            value = svg_path_new_from_data (path_data);
          else
            gtk_svg_invalid_attribute (data->svg, context, attr_names, "path", "Path data is invalid");
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
      GtkCssParser *parser = parser_new_for_string (rotate_attr);
      SvgValue *value;

      gtk_css_parser_skip_whitespace (parser);
      value = svg_number_parse (parser, -DBL_MAX, DBL_MAX, 0);
      if (value)
        {
          a->motion.angle = svg_number_get (value, 100);
          a->motion.rotate = ROTATE_FIXED;
          svg_value_unref (value);
        }
      else if (!parser_try_enum (parser,
                                 (const char *[]) { "auto", "auto-reverse" }, 2,
                                 (unsigned int *) &a->motion.rotate))
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "rotate", NULL);
        }

      gtk_css_parser_skip_whitespace (parser);
      if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "rotate",
                                   "Junk at end of value");

      gtk_css_parser_unref (parser);
    }

  if (a->calc_mode != CALC_MODE_PACED)
    {
      if (key_points_attr)
        {
          GArray *points = parse_number_list (key_points_attr, 0, 1);
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
          svg_animation_motion_fill_from_path (a, a->motion.path);
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
/* {{{ Attributes */

static void
parse_shape_attrs (SvgElement           *shape,
                   const char           *element_name,
                   const char          **attr_names,
                   const char          **attr_values,
                   uint64_t             *handled,
                   ParserData           *data,
                   GMarkupParseContext  *context)
{
  for (unsigned int i = 0; attr_names[i]; i++)
    {
      SvgProperty attr;

      if (*handled & BIT (i))
        continue;

      if (svg_element_get_type (shape) == SVG_ELEMENT_SVG &&
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
          svg_element_parse_classes (shape, attr_values[i]);
        }
      else if (strcmp (attr_names[i], "style") == 0)
        {
          GtkSvgLocation start, end;

          gtk_svg_location_init_attr_range (&start, &end, context, i);
          *handled |= BIT (i);
          svg_element_set_style (shape, attr_values[i], &start);
        }
      else if (strcmp (attr_names[i], "id") == 0)
        {
          *handled |= BIT (i);
          svg_element_set_id (shape, attr_values[i]);
        }
      else if (svg_property_lookup (attr_names[i], svg_element_get_type (shape), &attr) &&
               svg_property_has_presentation (attr))
        {
          if (svg_property_applies_to (attr, svg_element_get_type (shape)))
            {
              if (svg_element_property_is_set (shape, attr) &&
                  svg_attr_is_deprecated (attr_names[i]))
                {
                  /* ignore */
                }
              else
                {
                  SvgValue *value;
                  GError *error = NULL;

                  value = svg_property_parse_and_validate (attr, attr_values[i], &error);
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
                      svg_element_set_base_value (shape, attr, value);
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

  if (svg_element_property_is_set (shape, SVG_PROPERTY_CLIP_PATH) ||
      svg_element_property_is_set (shape, SVG_PROPERTY_MASK) ||
      svg_element_property_is_set (shape, SVG_PROPERTY_HREF) ||
      svg_element_property_is_set (shape, SVG_PROPERTY_FILTER) ||
      svg_element_property_is_set (shape, SVG_PROPERTY_MARKER_START) ||
      svg_element_property_is_set (shape, SVG_PROPERTY_MARKER_MID) ||
      svg_element_property_is_set (shape, SVG_PROPERTY_MARKER_END))
    g_hash_table_add (data->pending_refs, shape);

  if (svg_property_applies_to (SVG_PROPERTY_FX, svg_element_get_type (shape)) &&
      svg_property_applies_to (SVG_PROPERTY_FX, svg_element_get_type (shape)))
    {
      if (svg_element_property_is_set (shape, SVG_PROPERTY_CX) &&
          !svg_element_property_is_set (shape, SVG_PROPERTY_FX))
        svg_element_set_base_value (shape, SVG_PROPERTY_FX, svg_element_get_base_value (shape, SVG_PROPERTY_CX));
      if (svg_element_property_is_set (shape, SVG_PROPERTY_CY) &&
          !svg_element_property_is_set (shape, SVG_PROPERTY_FY))
        svg_element_set_base_value (shape, SVG_PROPERTY_FY, svg_element_get_base_value (shape, SVG_PROPERTY_CY));
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
      GStrv strv = parse_strv (state_names_attr);

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
parse_shape_gpa_attrs (SvgElement           *shape,
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

  if (!svg_element_type_is_path (svg_element_get_type (shape)))
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
          svg_element_set_base_value (shape, SVG_PROPERTY_STROKE, value);
          svg_element_set_gpa_stroke (shape, value);
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
          svg_element_set_base_value (shape, SVG_PROPERTY_FILL, value);
          svg_element_set_gpa_fill (shape, value);
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
          svg_element_set_base_value (shape, SVG_PROPERTY_STROKE_MINWIDTH, values[0]);
          svg_element_set_base_value (shape, SVG_PROPERTY_STROKE_WIDTH, values[1]);
          svg_element_set_base_value (shape, SVG_PROPERTY_STROKE_MAXWIDTH, values[2]);
          svg_element_set_gpa_width (shape, values[1]);
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
      if (!parse_duration (transition_duration_attr, FALSE, &transition_duration))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:transition-duration", NULL);
    }

  transition_delay = 0;
  if (transition_delay_attr)
    {
      if (!parse_duration (transition_delay_attr, FALSE, &transition_delay))
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
      if (!parse_duration (animation_duration_attr, FALSE, &animation_duration))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:animation-duration", NULL);
    }

  animation_repeat = REPEAT_FOREVER;
  if (animation_repeat_attr)
    {
      if (!parse_number_or_named (animation_repeat_attr, 0, DBL_MAX, "indefinite", REPEAT_FOREVER, &animation_repeat))
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

  svg_element_set_states (shape, states);
  svg_element_set_gpa_transition (shape,
                                  transition_type,
                                  transition_easing,
                                  transition_duration,
                                  transition_delay);
  svg_element_set_gpa_animation (shape,
                                 animation_direction,
                                 animation_easing,
                                 animation_duration,
                                 animation_repeat,
                                 animation_segment);
  svg_element_set_gpa_origin (shape, origin);
  svg_element_set_gpa_attachment (shape, attach_to_attr, attach_pos, NULL);

  if ((data->svg->features & GTK_SVG_ANIMATIONS) == 0)
    return;

  if (attach_to_attr)
    g_hash_table_add (data->pending_refs, shape);

  if (transition_type != GPA_TRANSITION_NONE ||
      animation_direction != GPA_ANIMATION_NONE)
    {
      /* our dasharray-based animations require unit path length */
      if (svg_element_property_is_set (shape, SVG_PROPERTY_PATH_LENGTH))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "pathLength",
                                   "Can't set '%s' and use gpa features", "pathLength");

      if (svg_element_property_is_set (shape, SVG_PROPERTY_STROKE_DASHARRAY))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "stroke-dasharray",
                                   "Can't set '%s' and use gpa features", "stroke-dasharray");

      if (svg_element_property_is_set (shape, SVG_PROPERTY_STROKE_DASHOFFSET))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "stroke-dashoffset",
                                   "Can't set '%s' and use gpa features", "stroke-dashoffset");

      if (svg_element_property_is_set (shape, SVG_PROPERTY_FILTER) &&
          transition_type == GPA_TRANSITION_MORPH)
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
parse_color_stop_attrs (SvgElement           *shape,
                        SvgColorStop         *stop,
                        const char           *element_name,
                        const char          **attr_names,
                        const char          **attr_values,
                        uint64_t             *handled,
                        ParserData           *data,
                        GMarkupParseContext  *context)
{
  for (unsigned int i = 0; attr_names[i]; i++)
    {
      SvgProperty attr;

      if (strcmp (attr_names[i], "id") == 0)
        {
          *handled |= BIT (i);
          svg_color_stop_set_id (stop, attr_values[i]);
        }
      else if (strcmp (attr_names[i], "style") == 0)
        {
          *handled |= BIT (i);
          svg_color_stop_set_style (stop, attr_values[i]);
        }
      else if (strcmp (attr_names[i], "class") == 0)
        {
          *handled |= BIT (i);
          svg_color_stop_parse_classes (stop, attr_values[i]);
        }

      else if (svg_property_lookup_for_stop (attr_names[i], svg_element_get_type (shape), &attr))
        {
          SvgValue *value;
          GError *error = NULL;

          *handled |= BIT (i);
          value = svg_property_parse_and_validate (attr, attr_values[i], &error);
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
              svg_color_stop_set_base_value (stop, attr, value);
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
parse_filter_attrs (SvgElement           *shape,
                    SvgFilter            *f,
                    const char           *element_name,
                    const char          **attr_names,
                    const char          **attr_values,
                    uint64_t             *handled,
                    ParserData           *data,
                    GMarkupParseContext  *context)
{
  SvgFilterType type = svg_filter_get_type (f);

  for (unsigned int i = 0; attr_names[i]; i++)
    {
      SvgProperty attr;

      if (strcmp (attr_names[i], "id") == 0)
        {
          *handled |= BIT (i);
          svg_filter_set_id (f, attr_values[i]);
        }
      else if (strcmp (attr_names[i], "style") == 0)
        {
          *handled |= BIT (i);
          svg_filter_set_style (f, attr_values[i]);
        }
      else if (strcmp (attr_names[i], "class") == 0)
        {
          *handled |= BIT (i);
          svg_filter_parse_classes (f, attr_values[i]);
        }
      else if (svg_property_lookup_for_filter (attr_names[i], svg_element_get_type (shape), type, &attr))
        {
          if (svg_filter_property_is_set (f, attr) && svg_attr_is_deprecated (attr_names[i]))
            {
              /* ignore */
            }
          else
            {
              SvgValue *value;
              GError *error = NULL;

              *handled |= BIT (i);
              value = svg_property_parse_and_validate (attr, attr_values[i], &error);
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
                  svg_filter_set_base_value (f, attr, value);
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
  SvgElementType shape_type;
  uint64_t handled = 0;
  SvgFilterType filter_type;
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

  if (svg_element_type_lookup (element_name, &shape_type))
    {
      SvgElement *shape = NULL;
      const char *id;

      if (data->current_shape &&
          !svg_element_type_is_container (svg_element_get_type (data->current_shape)))
        {
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Parent element can't contain shapes");
          return;
        }

      if (shape_type != SVG_ELEMENT_USE &&
          !svg_element_type_is_clip_path_content (shape_type) &&
          has_ancestor (context, "clipPath") &&
          shape_type != SVG_ELEMENT_CLIP_PATH)
        {
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "<clipPath> can only contain shapes, not %s", element_name);
          return;
        }

      shape = svg_element_new (data->current_shape, shape_type);
      svg_element_set_origin (shape, &location);

      if (data->current_shape == NULL && svg_element_get_type (shape) == SVG_ELEMENT_SVG)
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
        svg_element_add_child (data->current_shape, shape);

      data->shape_stack = g_slist_prepend (data->shape_stack, data->current_shape);

      data->current_shape = shape;

      id = svg_element_get_id (shape);
      if (id && !g_hash_table_contains (data->shapes, id))
        g_hash_table_insert (data->shapes, (gpointer) id, shape);

      return;
    }

  if (strcmp (element_name, "stop") == 0)
    {
      SvgColorStop *stop;

      if (data->current_shape == NULL ||
          (!check_ancestors (context, "linearGradient", NULL) &&
           !check_ancestors (context, "radialGradient", NULL)))
        {
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "<stop> only allowed in <linearGradient> or <radialGradient>");
          return;
        }

      stop = svg_color_stop_new (data->current_shape);
      svg_color_stop_set_origin (stop, &location);

      parse_color_stop_attrs (data->current_shape, stop,
                              element_name, attr_names, attr_values,
                              &handled, data, context);

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      svg_element_add_color_stop (data->current_shape, stop);

      return;
    }

  if (svg_filter_type_lookup (element_name, &filter_type))
    {
      SvgFilter *f;

      if (filter_type == SVG_FILTER_MERGE_NODE)
        {
          if (!check_ancestors (context, "feMerge", "filter", NULL))
            {
              skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "<%s> only allowed in <feMerge>", element_name);
              return;
            }

        }
      else if (filter_type == SVG_FILTER_FUNC_R ||
               filter_type == SVG_FILTER_FUNC_G ||
               filter_type == SVG_FILTER_FUNC_B ||
               filter_type == SVG_FILTER_FUNC_A)
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

      f = svg_filter_new (data->current_shape, filter_type);
      svg_filter_set_origin (f, &location);

      parse_filter_attrs (data->current_shape, f,
                          element_name, attr_names, attr_values,
                          &handled, data, context);

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (filter_type == SVG_FILTER_IMAGE)
        g_hash_table_add (data->pending_refs, data->current_shape);

      if (filter_type == SVG_FILTER_COLOR_MATRIX)
        {
          SvgValue *values = svg_filter_get_base_value (f, SVG_PROPERTY_FE_COLOR_MATRIX_VALUES);
          SvgValue *initial = svg_filter_ref_initial_value (f, SVG_PROPERTY_FE_COLOR_MATRIX_VALUES);

          if (svg_numbers_get_length (values) != svg_numbers_get_length (initial))
            {
              svg_filter_set_base_value (f, SVG_PROPERTY_FE_COLOR_MATRIX_VALUES, initial);
              if (!svg_filter_property_is_set (f, SVG_PROPERTY_FE_COLOR_MATRIX_VALUES))
                {
                  /* If this wasn't user-provided, we quietly correct the initial
                   * value to match the type
                   */
                  gtk_svg_invalid_attribute (data->svg, context, attr_names, "values", NULL);
                }
            }
          svg_value_unref (initial);
        }

      svg_element_add_filter (data->current_shape, f);

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
      SvgAnimation *a;
      const char *to_attr = NULL;
      SvgValue *value;
      GError *error = NULL;

      if ((data->svg->features & GTK_SVG_ANIMATIONS) == 0)
        {
          skip_element (data, context, GTK_SVG_ERROR_FEATURE_DISABLED, "SvgAnimations are disabled");
          return;
        }

      if (data->current_animation)
        {
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Nested animation elements are not allowed: <set>");
          return;
        }

      a = svg_animation_new (ANIMATION_TYPE_SET);

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
          svg_animation_drop_and_free (a);
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Skipping <%s> - bad attributes", element_name);
          return;
        }

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (!to_attr)
        {
          gtk_svg_missing_attribute (data->svg, context, "to", NULL);
          svg_animation_drop_and_free (a);
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Dropping <set> without 'to'");
          return;
        }

      a->calc_mode = CALC_MODE_DISCRETE;
      a->frames = g_new0 (Frame, 2);
      a->frames[0].time = 0;
      a->frames[1].time = 1;

      value = svg_property_parse_and_validate (a->attr, to_attr, &error);
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
          svg_animation_drop_and_free (a);
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Dropping <set> without 'to'");
          return;
        }

      a->frames[0].value = svg_value_ref (value);
      a->frames[1].value = svg_value_ref (value);
      a->n_frames = 2;

      svg_value_unref (value);

      if (!a->href ||
          (data->current_shape != NULL &&
           g_strcmp0 (a->href, svg_element_get_id (data->current_shape)) == 0))
        {
          a->shape = data->current_shape;
          svg_element_add_animation (data->current_shape, a);
        }
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
      SvgAnimation *a;

      if ((data->svg->features & GTK_SVG_ANIMATIONS) == 0)
        {
          skip_element (data, context, GTK_SVG_ERROR_IGNORED_ELEMENT, "SvgAnimations are disabled");
          return;
        }

      if (data->current_animation)
        {
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Nested animation elements are not allowed: <%s>", element_name);
          return;
        }

      if (strcmp (element_name, "animate") == 0)
        a = svg_animation_new (ANIMATION_TYPE_ANIMATE);
      else if (strcmp (element_name, "animateTransform") == 0)
        a = svg_animation_new (ANIMATION_TYPE_TRANSFORM);
      else
        a = svg_animation_new (ANIMATION_TYPE_MOTION);

      a->line = location.lines;

      if (!parse_base_animation_attrs (a,
                                       element_name,
                                       attr_names, attr_values,
                                       &handled,
                                       data,
                                       context))
        {
          svg_animation_drop_and_free (a);
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
          svg_animation_drop_and_free (a);
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
              svg_animation_drop_and_free (a);
              skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Skipping <%s>: bad attributes", element_name);
              return;
            }
        }

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (data->current_shape != NULL &&
          (a->href == NULL ||
           g_strcmp0 (a->href, svg_element_get_id (data->current_shape)) == 0))
        {
          a->shape = data->current_shape;
          svg_element_add_animation (data->current_shape, a);
        }
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
  SvgElementType shape_type;

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
  else if (svg_element_type_lookup (element_name, &shape_type))
    {
      GSList *tos = data->shape_stack;

      g_assert (shape_type == svg_element_get_type (data->current_shape));

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
  SvgElement *text_parent = NULL;

  if (!data->current_shape)
    return;

  if (svg_element_type_is_text (svg_element_get_type (data->current_shape)))
    text_parent = data->current_shape;
  else
    {
      SvgElement *parent = svg_element_get_parent (data->current_shape);
      if (parent &&
          svg_element_get_type (data->current_shape) == SVG_ELEMENT_LINK &&
           svg_element_type_is_text (svg_element_get_type (parent)))
        text_parent = parent;
    }

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

static void
add_dependency (SvgElement *shape0,
                SvgElement *shape1)
{
  if (!shape0->deps)
    shape0->deps = g_ptr_array_new ();
  g_ptr_array_add (shape0->deps, shape1);
}

/* Record the fact that when computing updated
 * values, shape2 must be handled before shape1
 */
static void
add_dependency_to_common_ancestor (SvgElement *shape0,
                                   SvgElement *shape1)
{
  SvgElement *anc0, *anc1;

  if (svg_element_common_ancestor (shape0, shape1, &anc0, &anc1))
    add_dependency (anc0, anc1);
}

static void
resolve_clip_ref (SvgValue   *value,
                  SvgElement *shape,
                  ParserData *data)
{
  if (svg_clip_get_kind (value) == CLIP_URL &&
      svg_clip_get_shape (value) == NULL)
    {
      const char *ref = svg_clip_get_id (value);
      SvgElement *target = g_hash_table_lookup (data->shapes, ref);
      if (!target)
        gtk_svg_invalid_reference (data->svg,
                                   "No path with ID %s (resolving clip-path)",
                                   ref);
      else if (target->type != SVG_ELEMENT_CLIP_PATH)
        gtk_svg_invalid_reference (data->svg,
                                   "SvgElement with ID %s not a <clipPath> (resolving clip-path)",
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
                  SvgElement *shape,
                  ParserData *data)
{
  if (svg_mask_get_kind (value) == MASK_URL && svg_mask_get_shape (value) == NULL)
    {
      const char *id = svg_mask_get_id (value);
      SvgElement *target = g_hash_table_lookup (data->shapes, id);
      if (!target)
        gtk_svg_invalid_reference (data->svg, "No shape with ID %s (resolving mask)", id);
      else if (target->type != SVG_ELEMENT_MASK)
        gtk_svg_invalid_reference (data->svg, "SvgElement with ID %s not a <mask> (resolving mask)", id);
      else
        {
          svg_mask_set_shape (value, target);
          add_dependency_to_common_ancestor (shape, target);
        }
    }
}

static gboolean
shape_has_ancestor_type (SvgElement     *shape,
                         SvgElementType  type)
{
  for (SvgElement *p = svg_element_get_parent (shape); p; p = p->parent)
    {
      if (p->type == type)
        return TRUE;
    }
  return FALSE;
}

static void
resolve_href_ref (SvgValue   *value,
                  SvgElement *shape,
                  ParserData *data)
{
  const char *ref;

  if (svg_href_get_kind (value) == HREF_NONE)
    return;

  ref = svg_href_get_id (value);
  if (svg_element_get_type (shape) == SVG_ELEMENT_IMAGE || svg_element_get_type (shape) == SVG_ELEMENT_FILTER)
    {
      GError *error = NULL;
      GdkTexture *texture;

      texture = get_texture (data->svg, ref, &error);
      svg_href_set_texture (value, texture);
      if (texture != NULL)
        return;

      if (svg_element_get_type (shape) == SVG_ELEMENT_IMAGE)
        {
          if (g_error_matches (error, GTK_SVG_ERROR, GTK_SVG_ERROR_FEATURE_DISABLED))
            gtk_svg_emit_error (data->svg, error);
          else
            gtk_svg_invalid_reference (data->svg, "Failed to load %s (resolving <image>): %s", ref, error->message);
          g_error_free (error);
          return; /* Image href is always external */
        }
      g_error_free (error);
    }

  if (svg_href_get_shape (value) == NULL)
    {
      SvgElement *target = g_hash_table_lookup (data->shapes, ref);
      if (!target)
        {
          gtk_svg_invalid_reference (data->svg,
                                     "No shape with ID %s (resolving href in <%s>)",
                                     ref,
                                     svg_element_type_get_name (svg_element_get_type (shape)));
        }
      else if (svg_element_get_type (shape) == SVG_ELEMENT_USE &&
               shape_has_ancestor_type (shape, SVG_ELEMENT_CLIP_PATH) &&
               !svg_element_type_is_clip_path_content (target->type))
        {
          gtk_svg_invalid_reference (data->svg,
                                     "Can't include a <%s> in a <clipPath> via <use>",
                                     svg_element_type_get_name (target->type));
        }
      else
        {
          svg_href_set_shape (value, target);
          add_dependency_to_common_ancestor (shape, target);
        }
    }
}

static void
resolve_marker_ref (SvgValue   *value,
                    SvgElement *shape,
                    ParserData *data)
{
  if (svg_href_get_kind (value) != HREF_NONE && svg_href_get_shape (value) == NULL)
    {
      const char *ref = svg_href_get_id (value);
      SvgElement *target = g_hash_table_lookup (data->shapes, ref);
      if (!target)
        {
          gtk_svg_invalid_reference (data->svg, "No shape with ID %s", ref);
        }
      else if (target->type != SVG_ELEMENT_MARKER)
        {
          gtk_svg_invalid_reference (data->svg,
                                     "SvgElement with ID %s not a <marker>",
                                     ref);
        }
      else
        {
          svg_href_set_shape (value, target);
          add_dependency_to_common_ancestor (shape, target);
        }
    }
}

static void
resolve_paint_ref (SvgValue   *value,
                   SvgElement *shape,
                   ParserData *data)
{
  SvgValue *paint = value;

  if (paint_is_server (svg_paint_get_kind (paint)) &&
      svg_paint_get_server_shape (paint) == NULL)
    {
      const char *ref = svg_paint_get_server_ref (paint);
      SvgElement *target = g_hash_table_lookup (data->shapes, ref);

      if (!target)
        {
          GtkSymbolicColor symbolic;

          if ((data->svg->features & GTK_SVG_EXTENSIONS) != 0 &&
              g_str_has_prefix (ref, "gpa:") &&
              parse_symbolic_color (ref + strlen ("gpa:"), &symbolic))
            return; /* Handled later */

          gtk_svg_invalid_reference (data->svg, "No shape with ID %s (resolving fill or stroke)", ref);
        }
      else if (target->type != SVG_ELEMENT_LINEAR_GRADIENT &&
               target->type != SVG_ELEMENT_RADIAL_GRADIENT &&
               target->type != SVG_ELEMENT_PATTERN)
        {
          gtk_svg_invalid_reference (data->svg, "SvgElement with ID %s not a paint server (resolving fill or stroke)", ref);
        }
      else
        {
          svg_paint_set_server_shape (paint, target);
          add_dependency_to_common_ancestor (shape, target);
        }
    }
}

static void
resolve_attach_ref (SvgElement *shape,
                    ParserData *data)
{
  const char *id;
  double position;
  SvgElement *sh;

  svg_element_get_gpa_attachment (shape, &id, &position, &sh);
  if (id != NULL && sh == NULL)
    {
      sh = g_hash_table_lookup (data->shapes, id);
      svg_element_set_gpa_attachment (shape, id, position, sh);
    }
}

static void
resolve_filter_ref (SvgValue   *value,
                    SvgElement *shape,
                    ParserData *data)
{
  for (unsigned int i = 0; i < svg_filter_functions_get_length (value); i++)
    {
      if (svg_filter_functions_get_kind (value, i) == FILTER_REF &&
          svg_filter_functions_get_shape (value, i) == NULL)
        {
          const char *ref = svg_filter_functions_get_ref (value, i);
          SvgElement *target = g_hash_table_lookup (data->shapes, ref);
          if (!target)
            {
              gtk_svg_invalid_reference (data->svg,
                                         "No shape with ID %s (resolving filter)",
                                         ref);
            }
          else if (target->type != SVG_ELEMENT_FILTER)
            {
              gtk_svg_invalid_reference (data->svg,
                                         "SvgElement with ID %s not a filter (resolving filter)",
                                         ref);
            }
          else
            {
              svg_filter_functions_set_shape (value, i, target);
              add_dependency_to_common_ancestor (shape, target);
            }
        }
    }
}

static void
resolve_refs_for_animation (SvgAnimation  *a,
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
                svg_animation_add_dep (spec->sync.base, a);
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

  if (a->attr == SVG_PROPERTY_CLIP_PATH)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_clip_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SVG_PROPERTY_MASK)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_mask_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SVG_PROPERTY_HREF)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_href_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SVG_PROPERTY_MARKER_START ||
           a->attr == SVG_PROPERTY_MARKER_MID ||
           a->attr == SVG_PROPERTY_MARKER_END)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_marker_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SVG_PROPERTY_FILL ||
           a->attr == SVG_PROPERTY_STROKE)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_paint_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SVG_PROPERTY_FILTER)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_filter_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SVG_PROPERTY_FE_IMAGE_HREF)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_href_ref (a->frames[j].value, a->shape, data);
    }

  if (a->motion.path_ref)
    {
      SvgElement *shape = g_hash_table_lookup (data->shapes, a->motion.path_ref);
      if (shape == NULL)
        gtk_svg_invalid_reference (data->svg,
                                   "No path with ID %s (resolving <mpath>",
                                   a->motion.path_ref);
      else if (!svg_element_type_is_path (svg_element_get_type (shape)))
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
resolve_animation_refs (SvgElement *shape,
                        ParserData *data)
{
  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          SvgAnimation *a = g_ptr_array_index (shape->animations, i);
          resolve_refs_for_animation (a, data);
        }
    }

  if (svg_element_type_is_container (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          SvgElement *sh = g_ptr_array_index (shape->shapes, i);
          resolve_animation_refs (sh, data);
        }
    }
}

static void
resolve_filter_image_refs (SvgElement *shape,
                           ParserData *data)
{
  if (svg_element_get_type (shape) != SVG_ELEMENT_FILTER)
    return;

  for (unsigned int i = 0; i < shape->filters->len; i++)
    {
      SvgFilter *f = g_ptr_array_index (shape->filters, i);
      SvgFilterType type = svg_filter_get_type (f);

      if (type == SVG_FILTER_IMAGE)
        resolve_href_ref (svg_filter_get_base_value (f, SVG_PROPERTY_FE_IMAGE_HREF), shape, data);
    }
}

static void
resolve_shape_refs (SvgElement *shape,
                    ParserData *data)
{
  resolve_clip_ref (svg_element_get_base_value (shape, SVG_PROPERTY_CLIP_PATH), shape, data);
  resolve_mask_ref (svg_element_get_base_value (shape, SVG_PROPERTY_MASK), shape, data);
  resolve_href_ref (svg_element_get_base_value (shape, SVG_PROPERTY_HREF), shape, data);
  resolve_marker_ref (svg_element_get_base_value (shape, SVG_PROPERTY_MARKER_START), shape, data);
  resolve_marker_ref (svg_element_get_base_value (shape, SVG_PROPERTY_MARKER_MID), shape, data);
  resolve_marker_ref (svg_element_get_base_value (shape, SVG_PROPERTY_MARKER_END), shape, data);
  resolve_paint_ref (svg_element_get_base_value (shape, SVG_PROPERTY_FILL), shape, data);
  resolve_paint_ref (svg_element_get_base_value (shape, SVG_PROPERTY_STROKE), shape, data);
  resolve_filter_ref (svg_element_get_base_value (shape, SVG_PROPERTY_FILTER), shape, data);
  resolve_attach_ref (shape, data);
  resolve_filter_image_refs (shape, data);
}

static gboolean
can_add (SvgElement *shape,
         GHashTable *waiting)
{
  if (shape->deps)
    {
      for (unsigned int i = 0; i < shape->deps->len; i++)
        {
          SvgElement *dep = g_ptr_array_index (shape->deps, i);
          if (g_hash_table_contains (waiting, dep))
            return FALSE;
        }
    }

  return TRUE;
}

static void
do_compute_update_order (SvgElement *shape,
                         GtkSvg     *svg,
                         GHashTable *waiting)
{
  unsigned int n_waiting;
  gboolean has_cycle = FALSE;
  SvgElement *last = NULL;

  if (!svg_element_type_is_container (svg_element_get_type (shape)))
    return;

  g_assert (g_hash_table_size (waiting) == 0);

  for (unsigned int i = 0; i < shape->shapes->len; i++)
    {
      SvgElement *sh = g_ptr_array_index (shape->shapes, i);
      do_compute_update_order (sh, svg, waiting);
    }

  for (unsigned int i = 0; i < shape->shapes->len; i++)
    {
      SvgElement *sh = g_ptr_array_index (shape->shapes, i);
      g_hash_table_add (waiting, sh);
    }

  n_waiting = g_hash_table_size (waiting);
  while (n_waiting > 0)
    {
      GHashTableIter iter;
      SvgElement *key;

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
      SvgElement *sh = g_ptr_array_index (shape->shapes, i);
      g_clear_pointer (&sh->deps, g_ptr_array_unref);
    }
}

static void
compute_update_order (SvgElement *shape,
                      GtkSvg     *svg)
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
  SvgProperty attr;
  SvgValue *value;
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
                     SvgProperty    attr,
                     SvgValue      *value)
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
          gtk_svg_error_set_element (error, svg_element_type_get_name (svg_element_get_type (data->current_shape)));
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

  return _gtk_css_selector_compare (a->selector, b->selector);
}

static void
postprocess_styles (ParserData *data)
{
  g_array_sort (data->rulesets, svg_css_compare_rule);
  g_array_sort (data->rulesets_important, svg_css_compare_rule);
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

static gboolean
lookup_property (const char  *name,
                 SvgProperty *result,
                 gboolean    *marker)
{
  if (strcmp (name, "marker") == 0)
    {
      *marker = TRUE;
      *result = SVG_PROPERTY_MARKER_START;
      return TRUE;
    }

  *marker = FALSE;
  return svg_property_lookup_for_css (name, result);
}

static void
parse_declaration (SvgCssScanner *scanner,
                   SvgCssRuleset *ruleset,
                   SvgCssRuleset *ruleset_important)
{
  char *name;
  SvgProperty attr;
  gboolean marker;

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

  if (lookup_property (name, &attr, &marker))
    {
      SvgCssRuleset *rs = ruleset;
      SvgValue *value;

      if (!gtk_css_parser_try_token (scanner->parser, GTK_CSS_TOKEN_COLON))
        {
          gtk_css_parser_error_syntax (scanner->parser, "Expected ':'");
          goto out;
        }

      value = svg_property_parse_css (attr, scanner->parser);

      if (value == NULL)
        goto out;

      if (gtk_css_parser_try_delim (scanner->parser, '!') &&
          gtk_css_parser_try_ident (scanner->parser, "important"))
        {
          rs = ruleset_important;
        }

      if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
        {
          gtk_css_parser_error_syntax (scanner->parser, "Junk at end of value for %s", name);
          svg_value_unref (value);
          goto out;
        }

      svg_css_ruleset_add (rs, attr, value);
      if (marker)
        {
          svg_css_ruleset_add (rs, SVG_PROPERTY_MARKER_MID, svg_value_ref (value));
          svg_css_ruleset_add (rs, SVG_PROPERTY_MARKER_END, svg_value_ref (value));
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
                    SvgCssRuleset *ruleset,
                    SvgCssRuleset *important)
{
  while (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    {
      parse_declaration (scanner, ruleset, important);
    }
}

static void
parse_style_into_ruleset (SvgCssRuleset *ruleset,
                          SvgCssRuleset *important,
                          const char    *style,
                          ParserData    *data)
{
  GBytes *bytes;
  SvgCssScanner *scanner;

  bytes = g_bytes_new_static (style, strlen (style));
  scanner = svg_css_scanner_new (data, NULL, NULL, bytes);
  parse_declarations (scanner, ruleset, important);
  svg_css_scanner_destroy (scanner);
  g_bytes_unref (bytes);
}

static void
commit_ruleset (GArray          *rulesets,
                SvgCssSelectors *selectors,
                SvgCssRuleset   *ruleset)
{
  if (ruleset->styles == NULL)
    return;

  for (unsigned int i = 0; i < svg_css_selectors_get_size (selectors); i++)
    {
      SvgCssRuleset *new;

      g_array_set_size (rulesets, rulesets->len + 1);
      new = &g_array_index (rulesets, SvgCssRuleset, rulesets->len - 1);
      svg_css_ruleset_init_copy (new, ruleset, gtk_css_selector_copy (svg_css_selectors_get (selectors, i)));
    }
}

static void
parse_ruleset (SvgCssScanner *scanner)
{
  SvgCssSelectors selectors;
  SvgCssRuleset ruleset = { 0, };
  SvgCssRuleset important = { 0, };

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
  parse_declarations (scanner, &ruleset, &important);
  gtk_css_parser_end_block (scanner->parser);
  commit_ruleset (scanner->data->rulesets, &selectors, &ruleset);
  commit_ruleset (scanner->data->rulesets_important, &selectors, &important);
  svg_css_ruleset_clear (&ruleset);
  svg_css_ruleset_clear (&important);
  clear_selectors (&selectors);

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
load_styles_for_shape (SvgElement      *shape,
                       ParserData *data)
{
  for (unsigned int i = 0; i < shape->styles->len; i++)
    {
      StyleElt *elt = g_ptr_array_index (shape->styles, i);
      data->text.start = elt->location;
      load_internal (data, NULL, NULL, elt->content);
    }

  if (svg_element_type_is_container (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          SvgElement *sh = g_ptr_array_index (shape->shapes, i);
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
          g_string_append (s, svg_property_get_name (p->attr));
          g_string_append (s, ": ");
          svg_value_print (p->value, s);
          g_string_append (s, ";");
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
                        SvgElement    *shape,
                        unsigned int   idx,
                        GtkBitmask    *set,
                        ParserData    *data)
{
  for (unsigned int j = 0; j < r->n_styles; j++)
    {
      PropertyValue *p = &r->styles[j];

      if (set && _gtk_bitmask_get (set, p->attr))
        continue;

      if (svg_property_applies_to (p->attr, svg_element_get_type (shape)))
        {
          shape_set_base_value (shape, p->attr, idx, p->value);

          if (p->attr == SVG_PROPERTY_CLIP_PATH ||
              p->attr == SVG_PROPERTY_MASK ||
              p->attr == SVG_PROPERTY_HREF ||
              p->attr == SVG_PROPERTY_FILTER ||
              p->attr == SVG_PROPERTY_MARKER_START ||
              p->attr == SVG_PROPERTY_MARKER_MID ||
              p->attr == SVG_PROPERTY_MARKER_END ||
              p->attr == SVG_PROPERTY_FILL ||
              p->attr == SVG_PROPERTY_STROKE)
            {
              g_hash_table_add (data->pending_refs, shape);
            }
        }

      if (set)
        set = _gtk_bitmask_set (set, p->attr, TRUE);
    }
}

static void
apply_styles_here (SvgElement   *shape,
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
      if (svg_element_type_is_gradient (svg_element_get_type (shape)))
        {
          SvgColorStop *stop = g_ptr_array_index (shape->color_stops, idx - 1);
          node = svg_color_stop_get_css_node (stop);
          style = svg_color_stop_get_style (stop);
          svg_color_stop_get_origin (stop, &loc);
        }
      else
        {
          SvgFilter *ff = g_ptr_array_index (shape->filters, idx - 1);
          node = svg_filter_get_css_node (ff);
          style = svg_filter_get_style (ff);
          svg_filter_get_origin (ff, &loc);
        }
    }
  else if (svg_element_get_css_node (shape))
    {
      node = svg_element_get_css_node (shape);
      style = svg_element_get_style (shape, &loc);
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

  SvgCssRuleset style_ruleset = { 0, };
  SvgCssRuleset style_important = { 0, };

  if (style)
    {
      data->text.start = loc;
      data->text.start.line_chars += strlen ("style='");
      data->text.start.bytes += strlen ("style='");
      data->current_shape = shape;

      parse_style_into_ruleset (&style_ruleset, &style_important, style, data);
      apply_ruleset_to_shape (&style_ruleset, shape, idx, NULL, data);
    }

  set = _gtk_bitmask_new ();

  for (unsigned int i = 0; i < data->rulesets_important->len; i++)
    {
      SvgCssRuleset *r = &g_array_index (data->rulesets_important, SvgCssRuleset, i);
      if (gtk_css_selector_matches (r->selector, node))
        apply_ruleset_to_shape (r, shape, idx, set, data);
    }

  _gtk_bitmask_free (set);

  if (style)
    {
      apply_ruleset_to_shape (&style_important, shape, idx, NULL, data);
      svg_css_ruleset_clear (&style_ruleset);
      svg_css_ruleset_clear (&style_important);
    }
}

static void
apply_styles_to_shape (SvgElement *shape,
                       ParserData *data)
{
  apply_styles_here (shape, 0, data);

  if (svg_element_type_is_container (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          SvgElement *sh = g_ptr_array_index (shape->shapes, i);
          apply_styles_to_shape (sh, data);
        }
    }

  if (svg_element_type_is_gradient (svg_element_get_type (shape)))
    {
      for (unsigned int idx = 0; idx < shape->color_stops->len; idx++)
        apply_styles_here (shape, idx + 1, data);
    }

  if (svg_element_type_is_filter (svg_element_get_type (shape)))
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
      GStrv classes = svg_element_get_classes (shape);
      SvgValue *value;
      gboolean has_stroke;

      if (!classes)
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_FOREGROUND);
      else if (g_strv_has (classes, "transparent-fill"))
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

      svg_element_set_base_value (shape, SVG_PROPERTY_FILL, value);
      svg_value_unref (value);

      if (!classes)
        value = svg_paint_new_none ();
      else if (g_strv_has (classes, "success-stroke"))
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
      svg_element_set_base_value (shape, SVG_PROPERTY_STROKE, value);
      svg_value_unref (value);

      if (has_stroke)
        {
          value = svg_number_new (2);
          svg_element_set_base_value (shape, SVG_PROPERTY_STROKE_WIDTH, value);
          svg_value_unref (value);

          value = svg_linejoin_new (GSK_LINE_JOIN_ROUND);
          svg_element_set_base_value (shape, SVG_PROPERTY_STROKE_LINEJOIN, value);
          svg_value_unref (value);

          value = svg_linecap_new (GSK_LINE_CAP_ROUND);
          svg_element_set_base_value (shape, SVG_PROPERTY_STROKE_LINECAP, value);
          svg_value_unref (value);
        }
    }

  /* gpa attrs are supported to have higher priority than
   * style and CSS, so re-set them here
   */
  if (data->svg->gpa_version > 0)
    {
      if (svg_element_get_gpa_fill (shape))
        svg_element_set_base_value (shape, SVG_PROPERTY_FILL, svg_element_get_gpa_fill (shape));
      if (svg_element_get_gpa_stroke (shape))
        svg_element_set_base_value (shape, SVG_PROPERTY_STROKE, svg_element_get_gpa_stroke (shape));
      if (svg_element_get_gpa_width (shape))
        svg_element_set_base_value (shape, SVG_PROPERTY_STROKE_WIDTH, svg_element_get_gpa_width (shape));
    }

  if (svg_element_property_is_set (shape, SVG_PROPERTY_COLOR))
    {
      SvgValue *color = svg_element_get_base_value (shape, SVG_PROPERTY_COLOR);

      if ((data->svg->features & GTK_SVG_EXTENSIONS) == 0 &&
          svg_color_get_kind (color) == COLOR_SYMBOLIC)
        {
          SvgValue *value = svg_color_new_black ();
          svg_element_set_base_value (shape, SVG_PROPERTY_COLOR, value);
          svg_value_unref (value);
          color = svg_element_get_base_value (shape, SVG_PROPERTY_COLOR);
        }

      if (svg_color_get_kind (color) == COLOR_SYMBOLIC)
        data->svg->used |= BIT (svg_color_get_symbolic (color) + 1);
    }

  if (svg_element_property_is_set (shape, SVG_PROPERTY_FILL))
    {
      SvgValue *paint = svg_element_get_base_value (shape, SVG_PROPERTY_FILL);
      GtkSymbolicColor symbolic;

      if ((data->svg->features & GTK_SVG_EXTENSIONS) == 0 &&
          svg_paint_get_kind (paint) == PAINT_SYMBOLIC)
        {
          SvgValue *value = svg_paint_new_black ();
          svg_element_set_base_value (shape, SVG_PROPERTY_FILL, value);
          svg_value_unref (value);
          paint = svg_element_get_base_value (shape, SVG_PROPERTY_FILL);
        }

      if (paint_is_server (svg_paint_get_kind (paint)))
        g_hash_table_add (data->pending_refs, shape);

      if (svg_paint_is_symbolic (paint, &symbolic))
        data->svg->used |= BIT (symbolic + 1);
    }

  if (svg_element_property_is_set (shape, SVG_PROPERTY_STROKE))
    {
      SvgValue *paint = svg_element_get_base_value (shape, SVG_PROPERTY_STROKE);
      GtkSymbolicColor symbolic;

      if ((data->svg->features & GTK_SVG_EXTENSIONS) == 0 &&
          svg_paint_get_kind (paint) == PAINT_SYMBOLIC)
        {
          SvgValue *value = svg_paint_new_black ();
          svg_element_set_base_value (shape, SVG_PROPERTY_STROKE, value);
          svg_value_unref (value);
          paint = svg_element_get_base_value (shape, SVG_PROPERTY_STROKE);
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

  g_clear_pointer (&self->content, svg_element_free);

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
  data.pending_animations = g_ptr_array_new_with_free_func ((GDestroyNotify) svg_animation_free);
  data.pending_refs = g_hash_table_new (g_direct_hash, g_direct_equal);
  data.skip.to = NULL;
  data.skip.reason = NULL;
  data.text.text = g_string_new ("");
  data.text.collect = FALSE;
  data.num_loaded_elements = 0;
  data.rulesets = g_array_new (FALSE, FALSE, sizeof (SvgCssRuleset));
  data.rulesets_important = g_array_new (FALSE, FALSE, sizeof (SvgCssRuleset));

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
    self->content = svg_element_new (NULL, SVG_ELEMENT_SVG);

  load_styles (&data);
#if 0
  dump_styles (&data);
#endif
  apply_styles (&data);

  if (svg_element_property_is_set (self->content, SVG_PROPERTY_VIEW_BOX))
    {
      graphene_rect_t vb;

      svg_view_box_get (self->content->base[SVG_PROPERTY_VIEW_BOX], &vb);
      self->width = vb.size.width;
      self->height = vb.size.height;
    }

  if (svg_element_property_is_set (self->content, SVG_PROPERTY_WIDTH))
    {
      SvgUnit unit = svg_number_get_unit (self->content->base[SVG_PROPERTY_WIDTH]);
      double value = svg_number_get (self->content->base[SVG_PROPERTY_WIDTH], 100);

      if (is_absolute_length (unit))
        self->width = absolute_length_to_px (value, unit);
      else if (unit != SVG_UNIT_PERCENTAGE)
        self->width = value;
    }

  if (svg_element_property_is_set (self->content, SVG_PROPERTY_HEIGHT))
    {
      SvgUnit unit = svg_number_get_unit (self->content->base[SVG_PROPERTY_HEIGHT]);
      double value = svg_number_get (self->content->base[SVG_PROPERTY_HEIGHT], 100);

      if (is_absolute_length (unit))
        self->height = absolute_length_to_px (value, unit);
      else if (unit != SVG_UNIT_PERCENTAGE)
        self->height = value;
    }

  if (!svg_element_property_is_set (self->content, SVG_PROPERTY_VIEW_BOX) &&
      !svg_element_property_is_set (self->content, SVG_PROPERTY_WIDTH) &&
      !svg_element_property_is_set (self->content, SVG_PROPERTY_HEIGHT))
    {
      /* arbitrary */
      self->width = 200;
      self->height = 200;
    }

  for (unsigned int i = 0; i < data.pending_animations->len; i++)
    {
      SvgAnimation *a = g_ptr_array_index (data.pending_animations, i);
      SvgElement *shape;

      g_assert (a->href != NULL);
      g_assert (a->shape == NULL);

      shape = g_hash_table_lookup (data.shapes, a->href);
      if (!shape)
        {
          gtk_svg_invalid_reference (self,
                                     "No shape with ID %s (resolving begin or end attribute)",
                                     a->href);
          svg_animation_free (a);
        }
      else
        {
          a->shape = shape;
          svg_element_add_animation (shape, a);
        }
    }

  /* Faster than stealing the items out of the array one-by-one */
  g_ptr_array_set_free_func (data.pending_animations, NULL);
  g_ptr_array_set_size (data.pending_animations, 0);

  if (g_hash_table_size (data.pending_refs) > 0)
    {
      GHashTableIter iter;
      SvgElement *shape;

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
  for (unsigned int i = 0; i < data.rulesets_important->len; i++)
    svg_css_ruleset_clear (&g_array_index (data.rulesets_important, SvgCssRuleset, i));
  g_array_unref (data.rulesets_important);

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
 * preserve explicitly set default values. SvgAnimations are
 * always emitted as children of the shape they belong to,
 * regardless of where they were placed in the original svg.
 *
 * In addition to the original DOM values, we allow serializing
 * 'snapshots' of a running animation at a given time, which
 * is very useful for tests. When doing so, we can also write
 * out some internal state in custom attributes, which is,
 * again, useful for tests.
 */

#define BASE_INDENT 2
#define ATTR_INDENT 8

static void
append_string_attr (GString    *s,
                    int         indent,
                    const char *name,
                    const char *value)
{
  string_indent (s, indent + ATTR_INDENT);
  g_string_append (s, name);
  g_string_append (s, "='");
  string_append_escaped (s, value);
  g_string_append_c (s, '\'');
}

static void
append_double_attr (GString    *s,
                    int         indent,
                    const char *name,
                    double      value,
                    const char *units)
{
  string_indent (s, indent + ATTR_INDENT);
  g_string_append (s, name);
  string_append_double (s, "='", value);
  if (units)
    g_string_append (s, units);
  g_string_append_c (s, '\'');
}

static void
append_time_attr (GString    *s,
                  int         indent,
                  const char *name,
                  int64_t     time)
{
  append_double_attr (s, indent, name, time / (double) G_TIME_SPAN_MILLISECOND, "ms");
}

static void
serialize_shape_attrs (GString              *s,
                       GtkSvg               *svg,
                       int                   indent,
                       SvgElement           *shape,
                       GtkSvgSerializeFlags  flags)
{
  GtkSvgLocation loc;

  if (svg_element_get_id (shape))
    append_string_attr (s, indent, "id", svg_element_get_id (shape));

  if (svg_element_get_classes (shape))
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "class='");
      string_append_strv (s, svg_element_get_classes (shape));
      g_string_append_c (s, '\'');
    }

  if (svg_element_get_style (shape, &loc))
    append_string_attr (s, indent, "style", svg_element_get_style (shape, &loc));

  for (SvgProperty attr = FIRST_SHAPE_PROPERTY; attr <= LAST_SHAPE_PROPERTY; attr++)
    {
      if ((flags & GTK_SVG_SERIALIZE_NO_COMPAT) == 0 &&
          svg->gpa_version > 0 &&
          svg_element_type_is_path (svg_element_get_type (shape)) &&
          attr == SVG_PROPERTY_VISIBILITY)
        {
          unsigned int state;

          if (svg->gpa_version == 0 &&
              (attr == SVG_PROPERTY_STROKE_MINWIDTH ||
               attr == SVG_PROPERTY_STROKE_MAXWIDTH))
            continue;

          if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
            state = svg->state;
          else
            state = svg->initial_state;

          if ((svg_element_get_states (shape) & BIT (state)) == 0)
            {
              append_string_attr (s, indent, "visibility", "hidden");
              continue;
            }
        }

      if (svg_element_property_is_set (shape, attr) ||
          (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME))
        {
          SvgValue *value, *initial;

          if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
            value = svg_value_ref (shape_get_current_value (shape, attr, 0));
          else
            value = shape_ref_base_value (shape, NULL, attr, 0);

          initial = svg_property_ref_initial_value (attr, svg_element_get_type (shape), svg_element_get_parent (shape) != NULL);

          if (svg_element_property_is_set (shape, attr) || !svg_value_equal (value, initial))
            {
              if (svg_property_has_presentation (attr))
                {
                  string_indent (s, indent + ATTR_INDENT);
                  g_string_append_printf (s, "%s='", svg_property_get_presentation (attr, svg_element_get_type (shape)));
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
                     SvgElement           *shape,
                     GtkSvgSerializeFlags  flags)
{
  if (svg->gpa_version == 0 || !svg_element_type_is_path (svg_element_get_type (shape)))
    return;

  if (svg_element_get_gpa_stroke (shape))
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "gpa:stroke='");
      svg_paint_print_gpa (svg_element_get_gpa_stroke (shape), s);
      g_string_append_c (s, '\'');
    }

  if (svg_element_get_gpa_fill (shape))
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "gpa:fill='");
      svg_paint_print_gpa (svg_element_get_gpa_fill (shape), s);
      g_string_append_c (s, '\'');
    }

  if (svg_element_get_states (shape) != ALL_STATES)
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "gpa:states='");
      print_states (s, svg, svg_element_get_states (shape));
      g_string_append_c (s, '\'');
    }

  if ((flags & GTK_SVG_SERIALIZE_EXPAND_GPA_ATTRS) == 0)
    {
      GpaTransition transition;
      GpaAnimation animation;
      GpaEasing easing;
      int64_t duration;
      int64_t delay;
      double repeat;
      double segment;
      const char *id;
      double pos;

      svg_element_get_gpa_transition (shape, &transition, &easing, &duration, &delay);
      if (transition != GPA_TRANSITION_NONE)
        {
          const char *names[] = { "none", "animate", "morph", "fade" };
          append_string_attr (s, indent, "gpa:transition-type", names[transition]);
        }

      if (easing != GPA_EASING_LINEAR)
        {
          const char *names[] = { "linear", "ease-in-out", "ease-in", "ease-out", "ease" };
          append_string_attr (s, indent, "gpa:transition-easing", names[easing]);
        }

      if (duration != 0)
        append_time_attr (s, indent, "gpa:transition-duration", duration);

      if (delay != 0)
        append_time_attr (s, indent, "gpa:transition-delay", delay);

      svg_element_get_gpa_animation (shape, &animation, &easing, &duration, &repeat, &segment);
      if (animation != GPA_ANIMATION_NONE)
        {
          const char *names[] = { "none", "normal", "alternate", "reverse",
            "reverse-alternate", "in-out", "in-out-alternate", "in-out-reverse",
            "segment", "segment-alternate" };
          append_string_attr (s, indent, "gpa:animation-type", "automatic");
          append_string_attr (s, indent, "gpa:animation-direction", names[animation]);
        }

      if (easing != GPA_EASING_LINEAR)
        {
          const char *names[] = { "linear", "ease-in-out", "ease-in", "ease-out", "ease" };
          append_string_attr (s, indent, "gpa:animation-easing", names[easing]);
        }

      if (duration != 0)
        append_time_attr (s, indent, "gpa:animation-duration", duration);

      if (repeat != REPEAT_FOREVER)
        append_double_attr (s, indent, "gpa:animation-repeat", repeat, NULL);

      if (segment != 0.2)
        append_double_attr (s, indent, "gpa:animation-segment", segment, NULL);

      if (svg_element_get_gpa_origin (shape) != 0)
        append_double_attr (s, indent, "gpa:origin", svg_element_get_gpa_origin (shape), NULL);

      svg_element_get_gpa_attachment (shape, &id, &pos, NULL);
      if (id)
        append_string_attr (s, indent, "gpa:attach-to", id);

      if (pos != 0)
        append_double_attr (s, indent, "gpa:attach-pos", pos, NULL);
    }
}

static void
serialize_base_animation_attrs (GString      *s,
                                GtkSvg       *svg,
                                int           indent,
                                SvgAnimation *a)
{
  if (a->id)
    append_string_attr (s, indent, "id", a->id);

  if (a->type != ANIMATION_TYPE_MOTION)
    append_string_attr (s, indent, "attributeName", svg_property_get_presentation (a->attr, svg_element_get_type (a->shape)));

  if (a->has_begin)
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "begin='");
      time_specs_print (a->begin, svg, s);
      g_string_append (s, "'");
    }

  if (a->has_end)
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "end='");
      time_specs_print (a->end, svg, s);
      g_string_append (s, "'");
    }

  if (a->has_simple_duration)
    {
      if (a->simple_duration == INDEFINITE)
        append_string_attr (s, indent, "dur", "indefinite");
      else
        append_time_attr (s, indent, "dur", a->simple_duration);
    }

  if (a->has_repeat_count)
    {
      if (a->repeat_count == REPEAT_FOREVER)
        append_string_attr (s, indent, "repeatCount", "indefinite");
      else
        append_double_attr (s, indent, "repeatCount", a->repeat_count, NULL);
    }

  if (a->has_repeat_duration)
    {
      if (a->repeat_duration == INDEFINITE)
        append_string_attr (s, indent, "repeatDur", "indefinite");
      else
        append_time_attr (s, indent, "repeatDur", a->repeat_duration);
    }

  if (a->fill != ANIMATION_FILL_REMOVE)
    {
      const char *fill[] = { "freeze", "remove" };
      append_string_attr (s, indent, "fill", fill[a->fill]);
    }

  if (a->restart != ANIMATION_RESTART_ALWAYS)
    {
      const char *restart[] = { "always", "whenNotActive", "never" };
      append_string_attr (s, indent, "restart", restart[a->restart]);
    }
}

static void
serialize_value_animation_attrs (GString      *s,
                                 GtkSvg       *svg,
                                 int           indent,
                                 SvgAnimation *a)
{
  if (a->type != ANIMATION_TYPE_MOTION)
    {
      if (a->n_frames == 2)
        {
          if (a->type != ANIMATION_TYPE_SET &&
              !svg_value_is_current (a->frames[0].value))
            {
              string_indent (s, indent + ATTR_INDENT);
              g_string_append (s, "from='");
              if (a->type == ANIMATION_TYPE_TRANSFORM && a->attr == SVG_PROPERTY_TRANSFORM)
                svg_primitive_transform_print (a->frames[0].value, s);
              else
                svg_value_print (a->frames[0].value, s);
              g_string_append (s, "'");
            }

          string_indent (s, indent + ATTR_INDENT);
          g_string_append (s, "to='");
          if (a->type == ANIMATION_TYPE_TRANSFORM && a->attr == SVG_PROPERTY_TRANSFORM)
            svg_primitive_transform_print (a->frames[1].value, s);
          else
            svg_value_print (a->frames[1].value, s);
          g_string_append_c (s, '\'');
        }
      else
        {
          string_indent (s, indent + ATTR_INDENT);
          g_string_append (s, "values='");
          if (a->type == ANIMATION_TYPE_TRANSFORM && a->attr == SVG_PROPERTY_TRANSFORM)
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
      string_indent (s, indent + ATTR_INDENT);
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
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "keyTimes='");
      for (unsigned int i = 0; i < a->n_frames; i++)
        string_append_double (s, i > 0 ? "; " : "", a->frames[i].time);
      g_string_append_c (s, '\'');
    }

  if (a->calc_mode != svg_animation_type_default_calc_mode (a->type))
    {
      const char *modes[] = { "discrete", "linear", "paced", "spline" };
      append_string_attr (s, indent, "calcMode",  modes[a->calc_mode]);
    }

  if (a->additive != ANIMATION_ADDITIVE_REPLACE)
    {
      const char *additive[] = { "replace", "sum" };
      append_string_attr (s, indent, "additive", additive[a->additive]);
    }

  if (a->accumulate != ANIMATION_ACCUMULATE_NONE)
    {
      const char *accumulate[] = { "none", "sum" };
      append_string_attr (s, indent, "accumulate", accumulate[a->accumulate]);
    }

  if (a->color_interpolation != COLOR_INTERPOLATION_SRGB)
    {
      const char *vals[] = { "auto", "sRGB", "linearRGB" };
      append_string_attr (s, indent, "color-interpolation", vals[a->color_interpolation]);
    }
}

static void
serialize_animation_status (GString              *s,
                            GtkSvg               *svg,
                            int                   indent,
                            SvgAnimation         *a,
                            GtkSvgSerializeFlags  flags)
{
  if (flags & GTK_SVG_SERIALIZE_INCLUDE_STATE)
    {
      const char *status[] = { "inactive", "running", "done" };
      append_string_attr (s, indent, "gpa:status", status[a->status]);

      /* Not writing out start/end time, since that will be hard to compare */

      if (!a->has_simple_duration)
        {
          int64_t d = determine_simple_duration (a);
          if (d == INDEFINITE)
            append_string_attr (s, indent, "gpa:computed-simple-duration", "indefinite");
          else
            append_time_attr (s, indent, "gpa:computed-simple-duration", d);
        }

      if (a->current.begin != INDEFINITE)
        append_time_attr (s, indent, "gpa:current-start-time", a->current.begin - svg->load_time);

      if (a->current.end != INDEFINITE)
        append_time_attr (s, indent, "gpa:current-end-time", a->current.end - svg->load_time);
    }
}

static void
serialize_animation_set (GString              *s,
                         GtkSvg               *svg,
                         int                   indent,
                         SvgAnimation         *a,
                         GtkSvgSerializeFlags  flags)
{
  string_indent (s, indent);
  g_string_append (s, "<set");
  serialize_base_animation_attrs (s, svg, indent, a);
  string_indent (s, indent + ATTR_INDENT);
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
                             SvgAnimation         *a,
                             GtkSvgSerializeFlags  flags)
{
  string_indent (s, indent);
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
                               SvgAnimation         *a,
                               GtkSvgSerializeFlags  flags)
{
  const char *types[] = { "none", "translate", "scale", "rotate", "any" };

  string_indent (s, indent);
  g_string_append (s, "<animateTransform");
  serialize_base_animation_attrs (s, svg, indent, a);
  serialize_value_animation_attrs (s, svg, indent, a);
  append_string_attr (s, indent, "type", types[svg_transform_get_type (a->frames[0].value, 0)]);
  serialize_animation_status (s, svg, indent, a, flags);
  g_string_append (s, "/>");
}

static void
serialize_animation_motion (GString              *s,
                            GtkSvg               *svg,
                            int                   indent,
                            SvgAnimation         *a,
                            GtkSvgSerializeFlags  flags)
{
  string_indent (s, indent);
  g_string_append (s, "<animateMotion");
  serialize_base_animation_attrs (s, svg, indent, a);
  serialize_value_animation_attrs (s, svg, indent, a);

  if (a->calc_mode != CALC_MODE_PACED)
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "keyPoints='");
      for (unsigned int i = 0; i < a->n_frames; i++)
        string_append_double (s, i > 0 ? "; " : "", a->frames[i].point);
      g_string_append (s, "'");
    }

  if (a->motion.rotate != ROTATE_FIXED)
    {
      const char *values[] = { "auto", "auto-reverse" };
      append_string_attr (s, indent, "rotate", values[a->motion.rotate]);
    }
  else if (a->motion.angle != 0)
    append_double_attr (s, indent, "rotate", a->motion.angle, NULL);

  serialize_animation_status (s, svg, indent, a, flags);

  if (a->motion.path_shape)
    {
      g_string_append (s, ">");
      string_indent (s, indent + BASE_INDENT);
      g_string_append_printf (s, "<mpath href='#%s'/>", svg_element_get_id (a->motion.path_shape));
      string_indent (s, indent);
      g_string_append (s, "</animateMotion>");
    }
  else
    {
      GskPath *path;

      if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
        path = svg_animation_motion_get_current_path (a, NULL);
      else
        path = svg_animation_motion_get_base_path (a, NULL);
      if (path)
        {
          string_indent (s, indent + ATTR_INDENT);
          g_string_append (s, "path='");
          gsk_path_print (path, s);
          g_string_append_c (s, '\'');
          gsk_path_unref (path);
        }
      g_string_append (s, "/>");
    }
}

static void
serialize_animation (GString              *s,
                     GtkSvg               *svg,
                     int                   indent,
                     SvgAnimation         *a,
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
                      SvgElement           *shape,
                      unsigned int          idx,
                      GtkSvgSerializeFlags  flags)
{
  SvgColorStop *stop;
  const char *names[] = { "offset", "stop-color", "stop-opacity" };

  stop = g_ptr_array_index (shape->color_stops, idx);

  string_indent (s, indent);
  g_string_append (s, "<stop");

  if (svg_color_stop_get_id (stop))
    append_string_attr (s, indent, "id", svg_color_stop_get_id (stop));

  if (svg_color_stop_get_classes (stop))
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "class='");
      string_append_strv (s, svg_color_stop_get_classes (stop));
      g_string_append_c (s, '\'');
    }

  if (svg_color_stop_get_style (stop))
    append_string_attr (s, indent, "style", svg_color_stop_get_style (stop));

  for (unsigned int i = 0; i < N_STOP_PROPERTIES; i++)
    {
      SvgProperty attr = svg_color_stop_get_property (i);
      SvgValue *value;

      string_indent (s, indent + ATTR_INDENT);
      g_string_append_printf (s, "%s='", names[i]);

      if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
        value = svg_color_stop_get_current_value (stop, attr);
      else
        value = svg_color_stop_get_base_value (stop, attr);

      svg_value_print (value, s);
      g_string_append (s, "'");
    }
  g_string_append (s, ">");

  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          SvgAnimation *a = g_ptr_array_index (shape->animations, i);
          if (a->idx == idx + 1)
            serialize_animation (s, svg, indent + BASE_INDENT, a, flags);
        }
    }

  string_indent (s, indent);
  g_string_append (s, "</stop>");
}

static void
serialize_filter_begin (GString              *s,
                        GtkSvg               *svg,
                        int                   indent,
                        SvgElement           *shape,
                        SvgFilter            *f,
                        unsigned int          idx,
                        GtkSvgSerializeFlags  flags)
{
  SvgFilterType type = svg_filter_get_type (f);

  string_indent (s, indent);
  g_string_append_printf (s, "<%s", svg_filter_type_get_name (type));

  if (svg_filter_get_id (f))
    append_string_attr (s, indent, "id", svg_filter_get_id (f));

  if (svg_filter_get_classes (f))
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "class='");
      string_append_strv (s, svg_filter_get_classes (f));
      g_string_append_c (s, '\'');
    }

  if (svg_filter_get_style (f))
    append_string_attr (s, indent, "style", svg_filter_get_style (f));

  for (unsigned int i = 0; i < svg_filter_type_get_n_attrs (type); i++)
    {
      SvgProperty attr = svg_filter_type_get_property (type, i);
      SvgValue *initial;
      SvgValue *value;

      initial = svg_filter_ref_initial_value (f, attr);
      if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
        value = svg_filter_get_current_value (f, attr);
      else
        value = svg_filter_get_base_value (f, attr);

      if (!svg_value_equal (value, initial))
        {
          string_indent (s, indent + ATTR_INDENT);
          g_string_append_printf (s, "%s='", svg_property_get_presentation (attr, svg_element_get_type (shape)));
          svg_value_print (value, s);
          g_string_append (s, "'");
        }

      svg_value_unref (initial);
    }

  g_string_append (s, ">");

  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          SvgAnimation *a = g_ptr_array_index (shape->animations, i);
          if (a->idx == idx + 1)
            serialize_animation (s, svg, indent + BASE_INDENT, a, flags);
        }
    }
}

static void
serialize_filter_end (GString              *s,
                      GtkSvg               *svg,
                      int                   indent,
                      SvgElement           *shape,
                      SvgFilter            *f,
                      GtkSvgSerializeFlags  flags)
{
  SvgFilterType type = svg_filter_get_type (f);

  string_indent (s, indent);
  g_string_append_printf (s, "</%s>", svg_filter_type_get_name (type));
}

static void
serialize_shape (GString              *s,
                 GtkSvg               *svg,
                 int                   indent,
                 SvgElement           *shape,
                 GtkSvgSerializeFlags  flags)
{
  if (svg_element_get_type (shape) == SVG_ELEMENT_DEFS &&
      shape->shapes->len == 0)
    return;

  if (indent > 0) /* Hack: this is for <svg> */
    {
      string_indent (s, indent);
      g_string_append_printf (s, "<%s", svg_element_type_get_name (svg_element_get_type (shape)));
      serialize_shape_attrs (s, svg, indent, shape, flags);
      serialize_gpa_attrs (s, svg, indent, shape, flags);
      g_string_append_c (s, '>');
    }

  for (unsigned int i = 0; i < shape->styles->len; i++)
    {
      StyleElt *elt = g_ptr_array_index (shape->styles, i);
      string_indent (s, indent + BASE_INDENT);
      g_string_append (s, "<style type='text/css'>");
      g_string_append (s, g_bytes_get_data (elt->content, NULL));
      g_string_append (s, "</style>");
    }

  if (svg_element_type_is_gradient (svg_element_get_type (shape)))
    {
      for (unsigned int idx = 0; idx < shape->color_stops->len; idx++)
        serialize_color_stop (s, svg, indent + BASE_INDENT, shape, idx, flags);
    }

  if (svg_element_type_is_filter (svg_element_get_type (shape)))
    {
      for (unsigned int idx = 0; idx < shape->filters->len; idx++)
        {
          SvgFilter *f = g_ptr_array_index (shape->filters, idx);

          serialize_filter_begin (s, svg, indent + BASE_INDENT, shape, f, idx, flags);

          if (svg_filter_get_type (f) == SVG_FILTER_MERGE)
            {
              for (idx++; idx < shape->filters->len; idx++)
                {
                  SvgFilter *f2 = g_ptr_array_index (shape->filters, idx);
                  if (svg_filter_get_type (f2) != SVG_FILTER_MERGE_NODE)
                    {
                      idx--;
                      break;
                    }

                  serialize_filter_begin (s, svg, indent + 2 * BASE_INDENT, shape, f2, idx, flags);
                  serialize_filter_end (s, svg, indent + 2 * BASE_INDENT, shape, f2, flags);
                }
            }

          if (svg_filter_get_type (f) == SVG_FILTER_COMPONENT_TRANSFER)
            {
              for (idx++; idx < shape->filters->len; idx++)
                {
                  SvgFilter *f2 = g_ptr_array_index (shape->filters, idx);
                  SvgFilterType t = svg_filter_get_type (f2);
                  if (t != SVG_FILTER_FUNC_R &&
                      t != SVG_FILTER_FUNC_G &&
                      t != SVG_FILTER_FUNC_B &&
                      t != SVG_FILTER_FUNC_A)
                    {
                      idx--;
                      break;
                    }

                  serialize_filter_begin (s, svg, indent + 2 * BASE_INDENT, shape, f2, idx, flags);
                  serialize_filter_end (s, svg, indent + 2 * BASE_INDENT, shape, f2, flags);
                }
            }

          serialize_filter_end (s, svg, indent + BASE_INDENT, shape, f, flags);
        }
    }

  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          SvgAnimation *a = g_ptr_array_index (shape->animations, i);
          if (a->idx == 0)
            serialize_animation (s, svg, indent + BASE_INDENT, a, flags);
        }
    }

  if (svg_element_type_is_text (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->text->len; i++)
        {
          TextNode *node = &g_array_index (shape->text, TextNode, i);
          switch (node->type)
            {
            case TEXT_NODE_SHAPE:
              serialize_shape (s, svg, indent + BASE_INDENT, node->shape.shape, flags);
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
      g_string_append_printf (s, "</%s>", svg_element_type_get_name (svg_element_get_type (shape)));
      return;
    }
  else if (svg_element_type_is_container (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          SvgElement *sh = g_ptr_array_index (shape->shapes, i);
          serialize_shape (s, svg, indent + BASE_INDENT, sh, flags);
        }
    }

  if (indent > 0)
    {
      string_indent (s, indent);
      g_string_append_printf (s, "</%s>", svg_element_type_get_name (svg_element_get_type (shape)));
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

  x_set = svg_filter_property_is_set (f, SVG_PROPERTY_FE_X);
  y_set = svg_filter_property_is_set (f, SVG_PROPERTY_FE_Y);
  w_set = svg_filter_property_is_set (f, SVG_PROPERTY_FE_WIDTH);
  h_set = svg_filter_property_is_set (f, SVG_PROPERTY_FE_HEIGHT);

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

                transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (tx, ty));
                transform = gsk_transform_scale (transform, sx, sy);

                node = gsk_texture_node_new (svg_href_get_texture (href), &vb);
                result = apply_transform (node, transform);
                gsk_render_node_unref (node);
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
  if (context->op != CLIPPING)
    {
      SvgValue *filter = svg_element_get_current_value (shape, SVG_PROPERTY_FILTER);
      SvgValue *blend = svg_element_get_current_value (shape, SVG_PROPERTY_BLEND_MODE);

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
    }

  return FALSE;
}

static gboolean
needs_isolation (SvgElement    *shape,
                 PaintContext  *context,
                 const char   **reason)
{
  if (context->op != CLIPPING)
    {
      SvgValue *isolation = svg_element_get_current_value (shape, SVG_PROPERTY_ISOLATION);
      SvgValue *opacity = svg_element_get_current_value (shape, SVG_PROPERTY_OPACITY);
      SvgValue *blend = svg_element_get_current_value (shape, SVG_PROPERTY_BLEND_MODE);
      SvgValue *filter = svg_element_get_current_value (shape, SVG_PROPERTY_FILTER);
      SvgValue *tf = svg_element_get_current_value (shape, SVG_PROPERTY_TRANSFORM);
      GskTransform *transform;

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
shape_is_use_target (SvgElement   *shape,
                     PaintContext *context)
{
  if (context->ctx_shape_stack)
    {
      SvgElement *ctx_shape = context->ctx_shape_stack->data;

      return svg_element_get_type (ctx_shape) == SVG_ELEMENT_USE &&
             svg_href_get_shape (svg_element_get_current_value (ctx_shape, SVG_PROPERTY_HREF)) == shape;
    }

  return FALSE;
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

          if (shape_is_use_target (shape, context))
            {
              SvgElement *use = context->ctx_shape_stack->data;
              if (svg_element_property_is_set (use, SVG_PROPERTY_WIDTH))
                width = svg_number_get (use->current[SVG_PROPERTY_WIDTH], context->viewport->size.width);
              else
                width = svg_number_get (svg_element_get_current_value (shape, SVG_PROPERTY_WIDTH), context->viewport->size.width);
              if (svg_element_property_is_set (use, SVG_PROPERTY_HEIGHT))
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
        gtk_snapshot_push_clip (context->snapshot, &GRAPHENE_RECT_INIT (x, y, width, height));

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

      if (svg_element_property_is_set (shape, SVG_PROPERTY_TRANSFORM_ORIGIN))
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
          svg_snapshot_push_fill (context->snapshot, svg_clip_get_path (clip), rule);
        }
      else
        {
          /* In the general case, we collect the clip geometry in a mask.
           * We special-case a single shape in the <clipPath> without
           * transforms and translate them to a clip or a fill.
           */
          SvgElement *clip_shape = svg_clip_get_shape (clip);
          SvgValue *ctf = svg_element_get_current_value (clip_shape, SVG_PROPERTY_TRANSFORM);
          SvgElement *child = NULL;

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
#ifdef DEBUG
              if (strstr (g_getenv ("SVG_DEBUG") ?:"", "nodes"))
                gtk_snapshot_push_debug (context->snapshot, "fill or clip for clip");
#endif
              svg_snapshot_push_fill (context->snapshot, path, GSK_FILL_RULE_WINDING);
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

                  if (svg_element_property_is_set (clip_shape, SVG_PROPERTY_TRANSFORM_ORIGIN))
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
            }
        }

      pop_op (context);
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

      if (svg_element_property_is_set (mask_shape, SVG_PROPERTY_X) ||
          svg_element_property_is_set (mask_shape, SVG_PROPERTY_Y) ||
          svg_element_property_is_set (mask_shape, SVG_PROPERTY_WIDTH) ||
          svg_element_property_is_set (mask_shape, SVG_PROPERTY_HEIGHT))
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

  if (context->op != CLIPPING && svg_element_get_type (shape) != SVG_ELEMENT_MASK)
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

  if (context->op != CLIPPING && svg_element_get_type (shape) != SVG_ELEMENT_MASK)
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
  if (!svg_element_property_is_set (shape, attr))
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

  if (svg_element_property_is_set (shape, attr))
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

  if (svg_element_property_is_set (gradient, SVG_PROPERTY_TRANSFORM_ORIGIN))
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

  if (svg_element_property_is_set (pattern, SVG_PROPERTY_TRANSFORM_ORIGIN))
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

  if (svg_element_property_is_set (self, SVG_PROPERTY_X))
    *x = svg_number_get (self->current[SVG_PROPERTY_X], 1);
  if (svg_element_property_is_set (self, SVG_PROPERTY_Y))
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

            if (svg_element_property_is_set (node->shape.shape, SVG_PROPERTY_FILL))
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

            if (svg_element_property_is_set (node->shape.shape, SVG_PROPERTY_STROKE))
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

static gboolean
shape_is_ancestor (SvgElement *parent,
                   SvgElement *shape)
{
  for (SvgElement *p = svg_element_get_parent (shape);  p; p = p->parent)
    {
      if (p == parent)
        return TRUE;
    }
  return FALSE;
}


static void
paint_shape (SvgElement   *shape,
             PaintContext *context)
{
  GskPath *path;

  if (svg_element_get_type (shape) == SVG_ELEMENT_USE)
    {
      if (svg_href_get_shape (svg_element_get_current_value (shape, SVG_PROPERTY_HREF)) != NULL)
        {
          SvgElement *use_shape = svg_href_get_shape (svg_element_get_current_value (shape, SVG_PROPERTY_HREF));

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
      recompute_current_values (shape, svg_element_get_parent (shape), context);
      mark_as_computed_for_use (shape, FALSE);
    }

  if (svg_element_get_type (shape) == SVG_ELEMENT_TEXT)
    {
      TextAnchor anchor;
      WritingMode wmode;
      graphene_rect_t bounds;
      float dx, dy;

      if (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_DISPLAY)) == DISPLAY_NONE)
        return;

      if (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_VISIBILITY)) == VISIBILITY_HIDDEN)
        return;

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

  if (svg_element_type_is_container (svg_element_get_type (shape)))
    {
      for (int i = 0; i < shape->shapes->len; i++)
        {
          SvgElement *s = g_ptr_array_index (shape->shapes, i);

          render_shape (s, context);

          if (svg_element_get_type (shape) == SVG_ELEMENT_SWITCH &&
              !svg_element_conditionally_excluded (s, context->svg))
            break;
        }

      return;
    }

  if (svg_enum_get (svg_element_get_current_value (shape, SVG_PROPERTY_VISIBILITY)) == VISIBILITY_HIDDEN)
    return;

  if (shape_is_degenerate (shape))
    return;

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
      if (!((svg_element_get_type (shape) == SVG_ELEMENT_SYMBOL && shape_is_use_target (shape, context)) ||
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
  return FALSE;

  if (self->node == NULL)
    return FALSE;

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

  vb = self->content->current[SVG_PROPERTY_VIEW_BOX];
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

  self->content = svg_element_new (NULL, SVG_ELEMENT_SVG);
  self->timeline = timeline_new ();

  self->images = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
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

  dbg_print ("run", "advancing current time to %s", format_time (current_time));

  self->current_time = current_time;

  invalidate_for_next_update (self);
  update_animation_state (self);
  collect_next_update (self);
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

  string_indent (s, ATTR_INDENT);
  g_string_append (s, "xmlns='http://www.w3.org/2000/svg'");
  string_indent (s, ATTR_INDENT);
  g_string_append (s, "xmlns:svg='http://www.w3.org/2000/svg'");
  if (self->keywords || self->description || self->author || self->license)
    {
      /* we only need these to write out keywords or description
       * in a way that inkscape understands
       */
      string_indent (s, ATTR_INDENT);
      g_string_append (s, "xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'");
      string_indent (s, ATTR_INDENT);
      g_string_append (s, "xmlns:cc='http://creativecommons.org/ns#'");
      string_indent (s, ATTR_INDENT);
      g_string_append (s, "xmlns:dc='http://purl.org/dc/elements/1.1/'");
    }

  if (self->gpa_version > 0 || (flags & GTK_SVG_SERIALIZE_INCLUDE_STATE))
    {
      string_indent (s, ATTR_INDENT);
      g_string_append (s, "xmlns:gpa='https://www.gtk.org/grappa'");
      string_indent (s, ATTR_INDENT);
      g_string_append_printf (s, "gpa:version='%u'", MAX (self->gpa_version, 1));
      if (self->n_state_names > 0)
        {
          string_indent (s, ATTR_INDENT);
          g_string_append (s, "gpa:state-names='");
          for (unsigned int i = 0; i < self->n_state_names; i++)
            {
              if (i > 0)
                g_string_append_c (s, ' ');
              g_string_append (s, self->state_names[i]);
            }
          g_string_append (s, "'");
        }

      string_indent (s, ATTR_INDENT);
      if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
        g_string_append_printf (s, "gpa:state='%u'", self->state);
      else
        g_string_append_printf (s, "gpa:state='%u'", self->initial_state);
    }

  if (flags & GTK_SVG_SERIALIZE_INCLUDE_STATE)
    {
      string_indent (s, ATTR_INDENT);
      string_append_double (s,
                            "gpa:state-change-delay='",
                            (self->state_change_delay) / (double) G_TIME_SPAN_MILLISECOND);
      g_string_append (s, "ms'");
      if (self->load_time != INDEFINITE)
        {
          string_indent (s, ATTR_INDENT);
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
      string_indent (s, BASE_INDENT);
      g_string_append (s, "<metadata>");
      string_indent (s, 2 * BASE_INDENT);
      g_string_append (s, "<rdf:RDF>");
      string_indent (s, 3 * BASE_INDENT);
      g_string_append (s, "<cc:Work>");
      if (self->license)
        {
          string_indent (s, 4 * BASE_INDENT);
          g_string_append (s, "<cc:license");
          string_indent (s, 4 * BASE_INDENT + ATTR_INDENT);
          g_string_append (s, "rdf:resource='");
          g_string_append (s, self->license);
          g_string_append (s, "'/>");
        }
      if (self->author)
        {
          string_indent (s, 4 * BASE_INDENT);
          g_string_append (s, "<dc:creator>");
          string_indent (s, 5 * BASE_INDENT);
          g_string_append (s, "<cc:Agent>");
          string_indent (s, 6 * BASE_INDENT);
          g_string_append_printf (s, "<dc:title>%s</dc:title>", self->author);
          string_indent (s, 5 * BASE_INDENT);
          g_string_append (s, "</cc:Agent>");
          string_indent (s, 4 * BASE_INDENT);
          g_string_append (s, "</dc:creator>");
        }
      if (self->description)
        {
          string_indent (s, 4 * BASE_INDENT);
          g_string_append_printf (s, "<dc:description>%s</dc:description>", self->description);
        }
      if (self->keywords)
        {
          string_indent (s, 4 * BASE_INDENT);
          g_string_append (s, "<dc:subject>");
          string_indent (s, 5 * BASE_INDENT);
          g_string_append (s, "<rdf:Bag>");
          string_indent (s, 6 * BASE_INDENT);
          g_string_append_printf (s, "<rdf:li>%s</rdf:li>", self->keywords);
          string_indent (s, 5 * BASE_INDENT);
          g_string_append (s, "</rdf:Bag>");
          string_indent (s, 4 * BASE_INDENT);
          g_string_append (s, "</dc:subject>");
        }
      string_indent (s, 3 * BASE_INDENT);
      g_string_append (s, "</cc:Work>");
      string_indent (s, 2 * BASE_INDENT);
      g_string_append (s, "</rdf:RDF>");
      string_indent (s, BASE_INDENT);
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

          string_indent (s, BASE_INDENT);
          g_string_append (s, "<font-face>");
          string_indent (s, 2 * BASE_INDENT);
          g_string_append (s, "<font-face-src>");
          string_indent (s, 3 * BASE_INDENT);
          g_string_append (s, "<font-face-uri");
          string_indent (s, 3 * BASE_INDENT + ATTR_INDENT);
          g_string_append (s, "href='data:font/ttf;base64,\\\n");
          string_append_base64 (s, bytes);
          g_string_append (s, "'/>");
          string_indent (s, 2 * BASE_INDENT);
          g_string_append (s, "</font-face-src>");
          string_indent (s, BASE_INDENT);
          g_string_append (s, "</font-face>");

          g_bytes_unref (bytes);
        }
    }

  serialize_shape (s, self, 0, self->content, flags);
  g_string_append (s, "\n</svg>\n");

  return g_string_free_to_bytes (s);
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
      dbg_print ("state", "renderer state %u -> %u", previous_state, state);

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
