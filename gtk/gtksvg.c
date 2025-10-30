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
#include "gtksymbolicpaintable.h"
#include "gtktypebuiltins.h"
#include "gtk/css/gtkcss.h"
#include "gtk/css/gtkcssparserprivate.h"
#include <glib/gstdio.h>

#include <math.h>
#include <stdint.h>

/**
 * GtkSvg:
 *
 * A paintable implementation that renders (a subset of) SVG,
 * with animations.
 *
 * `GtkSvg` objects are created by parsing a subset of SVG,
 * including SVG animations.
 *
 * The `GtkSvg` fills or strokes paths with symbolic or fixed
 * colors. It can have multiple states, and paths can be included
 * in a subset of the states. The special 'empty' state is always
 * available. States can have animation, and the transition between
 * different states can also be animated.
 *
 * To find out what states a `GtkSvg` has, use [method@Gtk.Svg.get_n_states].
 * To set the current state, use [method@Gtk.Svg.set_state].
 *
 * To play the animations in an SVG file, use
 * [method@Gtk.Svg.set_frame_clock] to connect the paintable to a frame clock,
 * and then use [method@Gtk.Svg.play] to start the animation.
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
 * defines the circle to be shown in states 0 and 1,
 * and animates a segment of the circle.
 *
 * <image src="svg-renderer1.svg">
 *
 * Note that the generated animations assume a `pathLengh` value of 1.
 * Setting `pathLength` in your SVG is therefore going to break such
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
 * Symbolic colors can also be specified as a custom paint server
 * reference, like this: `url(gpa:#warning)`. This works in `fill`
 * and `stroke` attributes, but also when specifying colors in SVG
 * animation attributes like `to` or `values`. Note that the SVG
 * syntax allows for a fallback RGB color to be specified after the
 * url, for compatibility with other SVG consumers:
 *
 *     fill='url(gpa:#warning) orange'
 *
 * In contrast to SVG 1.1 and 2.0, we allow the `transform` attribute
 * to be animated with `<animate>`.
 *
 * ## The supported subset of SVG
 *
 * The renderer does not support text or images, only shapes and paths.
 *
 * In `<defs>`, only `<clipPath>`, `<mask>`, gradients and shapes are
 * supported, not `<filter>`, `<pattern>` or other things.
 *
 * Gradient templating is not implemented, and radial gradients with
 * `fx,fy != cx,cy` are not supported.
 *
 * The support for filters is limited to filter functions minus
 * `drop-shadow()` plus a custom `alpha-level()` function, which
 * implements one particular case of feComponentTransfer.
 *
 * The `transform-origin` and `transform-box` attributes are not supported.
 *
 * The support for the `mask` attribute is limited to just a url
 * referring to the `<mask>` element by ID.
 *
 * In animation elements, the parsing of `begin` and `end` attributes
 * is limited, and the `by`, `min` and `max` attributes are not supported.
 *
 * Lastly, there is no CSS support, and no interactivity.
 *
 * ## Error handling
 *
 * Loading an SVG into `GtkSvg` will always produce a (possibly empty)
 * paintable. GTK will drop things that it can't handle and try to make
 * sense of the rest.
 *
 * To track errors during parsing or rednering, connect to the
 * [signal@Gtk.Svg::error] signal.
 *
 * For parsing errors in the `GTK_SVG_ERROR` domain, the functions
 * [func@Gtk.SvgError.get_start], [func@Gtk.SvgError.get_end],
 * [func@Gtk.SvgError.get_element] and [func@Gtk.SvgError.get_attribute]
 * can be used to obtain information about where the error occurred.
 *
 * Since: 4.22
 */

#define INDEFINITE G_MAXINT64

/* Max. nesting level of paint calls we allow */
#define MAX_DEPTH 256

#define DEBUG

typedef enum
{
  VISIBILITY_HIDDEN,
  VISIBILITY_VISIBLE,
} Visibility;

typedef enum
{
  SHAPE_LINE,
  SHAPE_POLY_LINE,
  SHAPE_POLYGON,
  SHAPE_RECT,
  SHAPE_CIRCLE,
  SHAPE_ELLIPSE,
  SHAPE_PATH,
  SHAPE_GROUP,
  SHAPE_CLIP_PATH,
  SHAPE_MASK,
  SHAPE_DEFS,
  SHAPE_USE,
  SHAPE_LINEAR_GRADIENT,
  SHAPE_RADIAL_GRADIENT,
} ShapeType;

typedef enum
{
  SHAPE_ATTR_VISIBILITY,
  SHAPE_ATTR_TRANSFORM,
  SHAPE_ATTR_OPACITY,
  SHAPE_ATTR_FILTER,
  SHAPE_ATTR_CLIP_PATH,
  SHAPE_ATTR_CLIP_RULE,
  SHAPE_ATTR_MASK,
  SHAPE_ATTR_MASK_TYPE,
  SHAPE_ATTR_FILL,
  SHAPE_ATTR_FILL_OPACITY,
  SHAPE_ATTR_FILL_RULE,
  SHAPE_ATTR_STROKE,
  SHAPE_ATTR_STROKE_OPACITY,
  SHAPE_ATTR_STROKE_WIDTH,
  SHAPE_ATTR_STROKE_LINECAP,
  SHAPE_ATTR_STROKE_LINEJOIN,
  SHAPE_ATTR_STROKE_MITERLIMIT,
  SHAPE_ATTR_STROKE_DASHARRAY,
  SHAPE_ATTR_STROKE_DASHOFFSET,
  SHAPE_ATTR_HREF,
  SHAPE_ATTR_PATH_LENGTH,
  SHAPE_ATTR_PATH,
  SHAPE_ATTR_CX,
  SHAPE_ATTR_CY,
  SHAPE_ATTR_R,
  SHAPE_ATTR_X,
  SHAPE_ATTR_Y,
  SHAPE_ATTR_WIDTH,
  SHAPE_ATTR_HEIGHT,
  SHAPE_ATTR_RX,
  SHAPE_ATTR_RY,
  SHAPE_ATTR_X1,
  SHAPE_ATTR_Y1,
  SHAPE_ATTR_X2,
  SHAPE_ATTR_Y2,
  SHAPE_ATTR_POINTS,
  SHAPE_ATTR_SPREAD_METHOD,
  SHAPE_ATTR_GRADIENT_UNITS,
  SHAPE_ATTR_FX,
  SHAPE_ATTR_FY,
  SHAPE_ATTR_FR,
  /* Things below are custom */
  SHAPE_ATTR_STROKE_MINWIDTH,
  SHAPE_ATTR_STROKE_MAXWIDTH,
  SHAPE_ATTR_STOP_OFFSET,
  SHAPE_ATTR_STOP_COLOR,
  SHAPE_ATTR_STOP_OPACITY,
} ShapeAttr;

typedef enum
{
  ALIGN_MIN = 1 << 0,
  ALIGN_MID = 1 << 1,
  ALIGN_MAX = 1 << 2,
} Align;

#define ALIGN_XY(x,y) ((x) | ((y) << 3))

#define ALIGN_GET_X(x) ((x) & 7)
#define ALIGN_GET_Y(x) ((x) >> 3)

typedef enum
{
  MEET,
  SLICE,
} MeetOrSlice;

typedef struct _Animation Animation;
typedef struct _Shape Shape;
typedef struct _Timeline Timeline;

struct _GtkSvg
{
  GObject parent_instance;
  Shape *content;

  double width, height;
  graphene_rect_t view_box;
  graphene_rect_t bounds;

  Align align;
  MeetOrSlice meet_or_slice;

  double weight;
  unsigned int state;
  unsigned int max_state;
  int64_t state_change_delay;

  int64_t load_time;
  int64_t current_time;

  gboolean playing;
  GtkSvgRunMode run_mode;
  GdkFrameClock *clock;
  unsigned long clock_update_id;
  unsigned int periodic_update_id;

  int64_t next_update;
  unsigned int pending_invalidate;
  gboolean advance_after_snapshot;

  unsigned int gpa_version;

  Timeline *timeline;
};

#define BIT(n) (G_GUINT64_CONSTANT (1) << (n))

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

#define dbg_print(cond, fmt,...) \
  G_STMT_START { \
    if (strstr (g_getenv ("SVG_DEBUG") ?:"", cond)) \
      { \
        char buf[64]; \
        g_print ("%s: ", format_time_buf (buf, 64, g_get_monotonic_time ())); \
        g_print (fmt __VA_OPT__(,) __VA_ARGS__); \
      } \
  } G_STMT_END

#else
#define dbg_print(cond,fmt,...)
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
#if GLIB_CHECK_VERSION (2, 87, 0)
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
normalized_diagonal (const graphene_size_t *viewport)
{
  return hypot (viewport->width, viewport->height) / M_SQRT2;
}

static inline double
lerp (double t, double a, double b)
{
  return a + (b - a) * t;
}

static inline double
accumulate (double a, double b, int n)
{
  return a + b * n;
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

      *ptr = NULL;
      for (unsigned int i = 0; attr_names[i]; i++)
        {
          if (strcmp (attr_names[i], name) == 0)
            {
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
string_append_double (GString *s,
                      double   value)
{
  char buf[64];

  g_ascii_formatd (buf, sizeof (buf), "%g", value);
  g_string_append (s, buf);
}

static gboolean
parse_number (const char  *value,
              double       min,
              double       max,
              double      *f)
{
  char *end = NULL;

  *f = g_ascii_strtod (value, &end);
  if (end && *end != '\0')
    return FALSE;
  if (*f < min || *f > max)
    return FALSE;

  return TRUE;
}

static GArray *
parse_numbers2 (const char *value,
                const char *sep,
                double      min,
                double      max)
{
  GArray *array;
  GStrv strv;

  strv = g_strsplit (value, sep, 0);

  array = g_array_new (FALSE, FALSE, sizeof (double));

  for (unsigned int i = 0; strv[i]; i++)
    {
      char *s = g_strstrip (strv[i]);
      double v;

      if (*s == '\0' && strv[i] == NULL)
        break;

      if (!parse_number (s, min, max, &v))
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

  strv = g_strsplit (value, sep, 0);
  *n_values = g_strv_length (strv);

  for (unsigned int i = 0; strv[i]; i++)
    {
      char *s = g_strstrip (strv[i]);

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

      if (!parse_number (s, min, max, &values[i]))
        {
          g_strfreev (strv);
          return FALSE;
        }
    }

  g_strfreev (strv);
  return TRUE;
}

static gboolean
parse_length (const char *value,
              double      min,
              double      max,
              double     *f)
{
  char *end = NULL;

  *f = g_ascii_strtod (value, &end);
  if (end && *end != '\0')
    {
      if (strcmp (end, "px") == 0)
        ; /* only unit we support atm */
      else
        return FALSE;
    }

 if (*f < min || *f > max)
   return FALSE;

  return TRUE;
}

static gboolean
parse_duration (const char *value,
                int64_t    *f)
{
  double v;
  char *end;

  v = g_ascii_strtod (value, &end);
  if (end && *end != '\0')
    {
      if (strcmp (end, "ms") == 0)
        *f = (int64_t) round (v * G_TIME_SPAN_MILLISECOND);
      else if (strcmp (end, "s") == 0)
        *f = (int64_t) round (v * G_TIME_SPAN_SECOND);
      else
        return FALSE;
    }
  else
    *f = (int64_t) round (v * G_TIME_SPAN_SECOND);

  return TRUE;
}

static gboolean
parse_enum (const char    *value,
            const char   **values,
            size_t         n_values,
            unsigned int  *result)
{
  for (unsigned int i = 0; i < n_values; i++)
    {
      if (values[i] == NULL)
        continue;

      if (strcmp (value, values[i]) == 0)
        {
          *result = i;
          return TRUE;
        }
    }
  return FALSE;
}

static GskPath *
make_ellipse_path (double cx, double cy,
                   double rx, double ry)
{
  GskPathBuilder *builder;

  builder = gsk_path_builder_new ();

  gsk_path_builder_move_to  (builder, cx + rx, cy);
  gsk_path_builder_conic_to (builder, cx + rx, cy + ry,
                                      cx,      cy + ry, M_SQRT1_2);
  gsk_path_builder_conic_to (builder, cx - rx, cy + ry,
                                      cy - rx, cy,      M_SQRT1_2);
  gsk_path_builder_conic_to (builder, cx - rx, cy - ry,
                                      cx,      cy - ry, M_SQRT1_2);
  gsk_path_builder_conic_to (builder, cx + rx, cy - ry,
                                      cx + rx, cy,      M_SQRT1_2);
  gsk_path_builder_close    (builder);

  return gsk_path_builder_free_to_path (builder);
}

static gboolean
parse_align (const char   *value,
             unsigned int *res)
{
  struct {
    const char *name;
    unsigned int value;
  } values[] = {
    { "none", 0 },
    { "xMinYMin", ALIGN_XY (ALIGN_MIN, ALIGN_MIN) },
    { "xMidYMin", ALIGN_XY (ALIGN_MID, ALIGN_MIN) },
    { "xMaxYMin", ALIGN_XY (ALIGN_MAX, ALIGN_MIN) },
    { "xMinYMid", ALIGN_XY (ALIGN_MIN, ALIGN_MID) },
    { "xMidYMid", ALIGN_XY (ALIGN_MID, ALIGN_MID) },
    { "xMaxYMid", ALIGN_XY (ALIGN_MAX, ALIGN_MID) },
    { "xMinYMax", ALIGN_XY (ALIGN_MIN, ALIGN_MAX) },
    { "xMidYMax", ALIGN_XY (ALIGN_MID, ALIGN_MAX) },
    { "xMaxYMax", ALIGN_XY (ALIGN_MAX, ALIGN_MAX) },
  };

  for (unsigned int i = 0; i < G_N_ELEMENTS (values); i++)
    {
      if (strcmp (values[i].name, value) == 0)
        {
          *res = values[i].value;
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
parse_meet (const char   *value,
            unsigned int *res)
{
  if (strcmp (value, "meet") == 0)
    {
      *res = MEET;
      return TRUE;
    }
  else if (strcmp (value, "slice") == 0)
    {
      *res = SLICE;
      return TRUE;
    }
  return FALSE;
}

static inline gboolean
g_strv_has (GStrv       strv,
            const char *s)
{
  return g_strv_contains ((const char * const *) strv, s);
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

typedef struct _SvgValue SvgValue;
typedef struct _SvgValueClass SvgValueClass;

struct _SvgValueClass
{
  const char *name;

  void       (* free)        (SvgValue       *value);
  gboolean   (* equal)       (const SvgValue *value0,
                              const SvgValue *value1);
  SvgValue * (* interpolate) (const SvgValue *value0,
                              const SvgValue *value1,
                              double          t);
  SvgValue * (* accumulate)  (const SvgValue *value0,
                              const SvgValue *value1,
                              int             n);
  void       (* print)       (const SvgValue *value,
                              GString        *string);
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

static SvgValue *
svg_value_ref (SvgValue *value)
{
  value->ref_count += 1;
  return value;
}

static void
svg_value_unref (SvgValue *value)
{
  if (value->ref_count > 1)
    {
      value->ref_count -= 1;
      return;
    }

  value->class->free (value);
}

static gboolean
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
                       double          t)
{
  if (value0->class != value1->class)
    return NULL;

  if (t == 0)
    return svg_value_ref ((SvgValue *) value0);

  if (t == 1)
    return svg_value_ref ((SvgValue *) value1);

  if (value0 == value1)
    return svg_value_ref ((SvgValue *) value0);

  return value1->class->interpolate (value0, value1, t);
}

/* Compute v = a + n * b */
static SvgValue *
svg_value_accumulate (const SvgValue *value0,
                      const SvgValue *value1,
                      int             n)
{
  if (value0->class != value1->class)
    return NULL;

  if (n == 0)
    return svg_value_ref ((SvgValue *) value0);

  return value0->class->accumulate (value0, value1, n);
}

static void
svg_value_print (const SvgValue *value,
                 GString        *string)
{
  value->class->print (value, string);
}

/* }}} */
/* {{{ Keyword values */

enum
{
  SVG_INHERIT,
  SVG_INITIAL,
};

typedef struct
{
  SvgValue base;
  unsigned int keyword;
} SvgKeyword;

G_GNUC_NORETURN
static void
svg_keyword_free (SvgValue *value)
{
  g_assert_not_reached ();
}

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
                         double          t)
{
  return NULL;
}

static SvgValue *
svg_keyword_accumulate (const SvgValue *value0,
                        const SvgValue *value1,
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
    default:
      g_assert_not_reached ();
    }
}

static const SvgValueClass SVG_KEYWORD_CLASS = {
  "SvgKeyword",
  svg_keyword_free,
  svg_keyword_equal,
  svg_keyword_interpolate,
  svg_keyword_accumulate,
  svg_keyword_print,
};

static SvgValue *
svg_inherit_new (void)
{
  static SvgKeyword inherit = { { &SVG_KEYWORD_CLASS, 1 }, SVG_INHERIT };
  return svg_value_ref ((SvgValue *) &inherit);
}

static SvgValue *
svg_initial_new (void)
{
  static SvgKeyword initial = { { &SVG_KEYWORD_CLASS, 1 }, SVG_INITIAL };
  return svg_value_ref ((SvgValue *) &initial);
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

/* }}} */
/* {{{ Numbers */

typedef enum
{
  UNIT_NONE,
  UNIT_PERCENT,
} Unit;

typedef struct
{
  SvgValue base;
  Unit unit;
  double value;
} SvgNumber;

static void
svg_number_free (SvgValue *value)
{
  g_free (value);
}

static gboolean
svg_number_equal (const SvgValue *value0,
                  const SvgValue *value1)
{
  const SvgNumber *n0 = (const SvgNumber *) value0;
  const SvgNumber *n1 = (const SvgNumber *) value1;

  return n0->unit == n1->unit && n0->value == n1->value;
}

static SvgValue * svg_number_new_full (Unit   unit,
                                       double value);

static SvgValue *
svg_number_interpolate (const SvgValue *value0,
                        const SvgValue *value1,
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

  string_append_double (string, n->value);
  if (n->unit == UNIT_PERCENT)
    g_string_append_c (string, '%');
}

static const SvgValueClass SVG_NUMBER_CLASS = {
  "SvgNumber",
  svg_number_free,
  svg_number_equal,
  svg_number_interpolate,
  svg_number_accumulate,
  svg_number_print
};

static SvgValue *
svg_number_new (double value)
{
  static SvgNumber singletons[] = {
    { { &SVG_NUMBER_CLASS, 1 }, .unit = UNIT_NONE, .value = 0 },
    { { &SVG_NUMBER_CLASS, 1 }, .unit = UNIT_NONE, .value = 1 },
  };
  SvgNumber *result;

  if (value == 0 || value == 1)
    return svg_value_ref ((SvgValue *) &singletons[(int) value]);

  result = (SvgNumber *) svg_value_alloc (&SVG_NUMBER_CLASS, sizeof (SvgNumber));
  result->unit = UNIT_NONE;
  result->value = value;

  return (SvgValue *) result;
}

static SvgValue *
svg_percentage_new (double value)
{
  static SvgNumber singletons[] = {
    { { &SVG_NUMBER_CLASS, 1 }, .unit = UNIT_PERCENT, .value = 0 },
    { { &SVG_NUMBER_CLASS, 1 }, .unit = UNIT_PERCENT, .value = 50 },
    { { &SVG_NUMBER_CLASS, 1 }, .unit = UNIT_PERCENT, .value = 100 },
  };
  SvgNumber *result;

  if (value == 0)
    return svg_value_ref ((SvgValue *) &singletons[0]);
  else if (value == 50)
    return svg_value_ref ((SvgValue *) &singletons[1]);
  else if (value == 100)
    return svg_value_ref ((SvgValue *) &singletons[2]);

  result = (SvgNumber *) svg_value_alloc (&SVG_NUMBER_CLASS, sizeof (SvgNumber));
  result->value = value;
  result->unit = UNIT_PERCENT;

  return (SvgValue *) result;
}

static SvgValue *
svg_number_new_full (Unit   unit,
                     double value)
{
  if (unit == UNIT_NONE)
    return svg_number_new (value);
  else
    return svg_percentage_new (value);
}

enum
{
  PERCENTAGE = 1 << 0,
  LENGTH     = 1 << 1,
};

static SvgValue *
svg_number_parse (const char   *value,
                  double        min,
                  double        max,
                  unsigned int  flags)
{
  char *end = NULL;
  double f;
  Unit unit = UNIT_NONE;

  f = g_ascii_strtod (value, &end);
  if (end && *end != '\0')
    {
      if (*end == '%' && (flags & PERCENTAGE))
        unit = UNIT_PERCENT;
      else if (strcmp (end, "px") == 0 && (flags & LENGTH))
        unit = UNIT_NONE;
      else
        return NULL;
    }
  if (unit == UNIT_PERCENT)
    {
      if (f < -100 || f > 100)
        return NULL;
    }
  else
    {
      if (f < min || f > max)
        return NULL;
    }

  return svg_number_new_full (unit, f);
}

static double
svg_number_get (const SvgValue *value,
                double          one_hundred_percent)
{
  const SvgNumber *n = (const SvgNumber *)value;
  if (n->unit == UNIT_PERCENT)
    return n->value / 100 * one_hundred_percent;
  else
    return n->value;
}

/* }}} */
/* {{{ Enums */

typedef struct
{
  SvgValue base;
  unsigned int value;
  const char *name;
} SvgEnum;

static void
svg_enum_free (SvgValue *value)
{
  g_free (value);
}

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

static const SvgValueClass SVG_FILL_RULE_CLASS = {
  "SvgFillRule",
  svg_enum_free,
  svg_enum_equal,
  svg_enum_interpolate,
  svg_enum_accumulate,
  svg_enum_print,
};

static SvgEnum fill_rule_values[] = {
  { { &SVG_FILL_RULE_CLASS, 1 }, GSK_FILL_RULE_WINDING, "nonzero" },
  { { &SVG_FILL_RULE_CLASS, 1 }, GSK_FILL_RULE_EVEN_ODD, "evenodd" },
};

static SvgValue *
svg_fill_rule_new (GskFillRule value)
{
  return svg_value_ref ((SvgValue *) &fill_rule_values[value]);
}

static SvgValue *
svg_fill_rule_parse (const char *string)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (fill_rule_values); i++)
    {
      if (strcmp (string, fill_rule_values[i].name) == 0)
        return svg_value_ref ((SvgValue *) &fill_rule_values[i]);
    }
  return NULL;
}

static const SvgValueClass SVG_MASK_TYPE_CLASS = {
  "SvgMaskType",
  svg_enum_free,
  svg_enum_equal,
  svg_enum_interpolate,
  svg_enum_accumulate,
  svg_enum_print,
};

static SvgEnum mask_type_values[] = {
 { { &SVG_MASK_TYPE_CLASS, 1 }, GSK_MASK_MODE_ALPHA, "alpha" },
 { { &SVG_MASK_TYPE_CLASS, 1 }, GSK_MASK_MODE_LUMINANCE, "luminance" },
};

static SvgValue *
svg_mask_type_new (GskMaskMode value)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (mask_type_values); i++)
    {
      if (mask_type_values[i].value == value)
        return svg_value_ref ((SvgValue *) &mask_type_values[i]);
    }
  return NULL;
}

static SvgValue *
svg_mask_type_parse (const char *string)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (mask_type_values); i++)
    {
      if (strcmp (string, mask_type_values[i].name) == 0)
        return svg_value_ref ((SvgValue *) &mask_type_values[i]);
    }
  return NULL;
}

static const SvgValueClass SVG_LINE_CAP_CLASS = {
  "SvgLineCap",
  svg_enum_free,
  svg_enum_equal,
  svg_enum_interpolate,
  svg_enum_accumulate,
  svg_enum_print,
};

static SvgEnum line_cap_values[] = {
  { { &SVG_LINE_CAP_CLASS, 1 }, GSK_LINE_CAP_BUTT, "butt" },
  { { &SVG_LINE_CAP_CLASS, 1 }, GSK_LINE_CAP_ROUND, "round" },
  { { &SVG_LINE_CAP_CLASS, 1 }, GSK_LINE_CAP_SQUARE, "square" },
};

static SvgValue *
svg_linecap_new (GskLineCap value)
{
  return svg_value_ref ((SvgValue *) &line_cap_values[value]);
}

static SvgValue *
svg_linecap_parse (const char *string)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (line_cap_values); i++)
    {
      if (strcmp (string, line_cap_values[i].name) == 0)
        return svg_value_ref ((SvgValue *) &line_cap_values[i]);
    }
  return NULL;
}

static const SvgValueClass SVG_LINE_JOIN_CLASS = {
  "SvgLineJoin",
  svg_enum_free,
  svg_enum_equal,
  svg_enum_interpolate,
  svg_enum_accumulate,
  svg_enum_print,
};

static SvgEnum line_join_values[] = {
  { { &SVG_LINE_JOIN_CLASS, 1 }, GSK_LINE_JOIN_MITER, "miter" },
  { { &SVG_LINE_JOIN_CLASS, 1 }, GSK_LINE_JOIN_ROUND, "round" },
  { { &SVG_LINE_JOIN_CLASS, 1 }, GSK_LINE_JOIN_BEVEL, "bevel" },
};

static SvgValue *
svg_linejoin_new (GskLineJoin value)
{
  return svg_value_ref ((SvgValue *) &line_join_values[value]);
}

static SvgValue *
svg_linejoin_parse (const char *string)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (line_join_values); i++)
    {
      if (strcmp (string, line_join_values[i].name) == 0)
        return svg_value_ref ((SvgValue *) &line_join_values[i]);
    }
  return NULL;
}

static const SvgValueClass SVG_VISIBILITY_CLASS = {
  "SvgVisibility",
  svg_enum_free,
  svg_enum_equal,
  svg_enum_interpolate,
  svg_enum_accumulate,
  svg_enum_print,
};

static SvgEnum visibility_values[] = {
  { { &SVG_VISIBILITY_CLASS, 1 }, VISIBILITY_HIDDEN, "hidden" },
  { { &SVG_VISIBILITY_CLASS, 1 }, VISIBILITY_VISIBLE, "visible" },
};

static SvgValue *
svg_visibility_new (Visibility value)
{
  return svg_value_ref ((SvgValue *) &visibility_values[value]);
}

static SvgValue *
svg_visibility_parse (const char *string)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (visibility_values); i++)
    {
      if (strcmp (string, visibility_values[i].name) == 0)
        return svg_value_ref ((SvgValue *) &visibility_values[i]);
    }
  return NULL;
}

typedef enum
{
  SPREAD_METHOD_PAD,
  SPREAD_METHOD_REFLECT,
  SPREAD_METHOD_REPEAT,
} SpreadMethod;

static const SvgValueClass SVG_SPREAD_METHOD_CLASS = {
  "SvgSpreadMethod",
  svg_enum_free,
  svg_enum_equal,
  svg_enum_interpolate,
  svg_enum_accumulate,
  svg_enum_print,
};

static SvgEnum spread_method_values[] = {
  { { &SVG_SPREAD_METHOD_CLASS, 1 }, SPREAD_METHOD_PAD, "pad" },
  { { &SVG_SPREAD_METHOD_CLASS, 1 }, SPREAD_METHOD_REFLECT, "reflect" },
  { { &SVG_SPREAD_METHOD_CLASS, 1 }, SPREAD_METHOD_REPEAT, "repeat" },
};

static SvgValue *
svg_spread_method_new (SpreadMethod value)
{
  return svg_value_ref ((SvgValue *) &visibility_values[value]);
}

static SvgValue *
svg_spread_method_parse (const char *string)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (spread_method_values); i++)
    {
      if (strcmp (string, spread_method_values[i].name) == 0)
        return svg_value_ref ((SvgValue *) &spread_method_values[i]);
    }
  return NULL;
}

typedef enum
{
  COORD_UNITS_USER_SPACE_ON_USE,
  COORD_UNITS_OBJECT_BOUNDING_BOX,
} CoordUnits;

static const SvgValueClass SVG_COORD_UNITS_CLASS = {
  "SvgCoordUnits",
  svg_enum_free,
  svg_enum_equal,
  svg_enum_interpolate,
  svg_enum_accumulate,
  svg_enum_print,
};

static SvgEnum coord_units_values[] = {
  { { &SVG_COORD_UNITS_CLASS, 1 }, COORD_UNITS_USER_SPACE_ON_USE, "userSpaceOnUse" },
  { { &SVG_COORD_UNITS_CLASS, 1 }, COORD_UNITS_OBJECT_BOUNDING_BOX, "objectBoundingBox" },
};

static SvgValue *
svg_coord_units_new (CoordUnits value)
{
  return svg_value_ref ((SvgValue *) &coord_units_values[value]);
}

static SvgValue *
svg_coord_units_parse (const char *string)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (coord_units_values); i++)
    {
      if (strcmp (string, coord_units_values[i].name) == 0)
        return svg_value_ref ((SvgValue *) &coord_units_values[i]);
    }
  return NULL;
}

/* }}} */
/* {{{ Transforms */

typedef enum
{
  TRANSFORM_NONE,
  TRANSFORM_TRANSLATE,
  TRANSFORM_SCALE,
  TRANSFORM_ROTATE,
  TRANSFORM_SKEW_X,
  TRANSFORM_SKEW_Y,
  TRANSFORM_MATRIX,
} TransformType;

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

static void
svg_transform_free (SvgValue *value)
{
  g_free (value);
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
                                            double          t);
static SvgValue *svg_transform_accumulate  (const SvgValue *v0,
                                            const SvgValue *v1,
                                            int             n);
static void      svg_transform_print       (const SvgValue *v,
                                            GString        *string);

static const SvgValueClass SVG_TRANSFORM_CLASS = {
  "SvgTransform",
  svg_transform_free,
  svg_transform_equal,
  svg_transform_interpolate,
  svg_transform_accumulate,
  svg_transform_print
};

static SvgTransform *
svg_transform_alloc (unsigned int n)
{
  SvgTransform *t;

  t = (SvgTransform *) svg_value_alloc (&SVG_TRANSFORM_CLASS, svg_transform_size (n));
  t->n_transforms = n;

  return t;
}

static SvgValue *
svg_transform_new_none (void)
{
  static SvgTransform none = { { &SVG_TRANSFORM_CLASS, 1 }, 1, { { TRANSFORM_NONE, } }};
  return svg_value_ref ((SvgValue *) &none);
}

static SvgValue *
svg_transform_new_translate (double x, double y)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_TRANSLATE;
  tf->transforms[0].translate.x = x;
  tf->transforms[0].translate.y = y;
  return (SvgValue *) tf;
}

static SvgValue *
svg_transform_new_scale (double x, double y)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_SCALE;
  tf->transforms[0].scale.x = x;
  tf->transforms[0].scale.y = y;
  return (SvgValue *) tf;
}

static SvgValue *
svg_transform_new_rotate (double angle, double x, double y)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_ROTATE;
  tf->transforms[0].rotate.angle = angle;
  tf->transforms[0].rotate.x = x;
  tf->transforms[0].rotate.y = y;
  return (SvgValue *) tf;
}

static SvgValue *
svg_transform_new_skew_x (double angle)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_SKEW_X;
  tf->transforms[0].skew_x.angle = angle;
  return (SvgValue *) tf;
}

static SvgValue *
svg_transform_new_skew_y (double angle)
{
  SvgTransform *tf = svg_transform_alloc (1);
  tf->transforms[0].type = TRANSFORM_SKEW_Y;
  tf->transforms[0].skew_y.angle = angle;
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
  double *f = data;
  double d;

  if (!gtk_css_parser_consume_number (parser, &d))
    return 0;

  f[n] = d;
  return 1;
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

  token = gtk_css_parser_get_token (self);
  g_return_val_if_fail (gtk_css_token_is (token, GTK_CSS_TOKEN_FUNCTION), FALSE);

  g_strlcpy (function_name, gtk_css_token_get_string (token), 64);
  gtk_css_parser_start_block (self);

  arg = 0;
  while (TRUE)
    {
      guint parse_args = css_parser_parse_number (self, arg, values);
      if (parse_args == 0)
        break;
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
          if (values[1])
            {
              transform.rotate.x = values[1];
              transform.rotate.y = values[2];
            }
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

static SvgValue *
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

  if (strchr (value, ','))
    strv = g_strsplit (value, ",", 0);
  else
    strv = g_strsplit (value, " ", 0);

  n = g_strv_length (strv);

  switch (type)
    {
    case TRANSFORM_TRANSLATE:
      {
        double x, y;

        if (n < 1 || n > 2 ||
            !parse_length (strv[0], -DBL_MAX, DBL_MAX, &x) ||
            ((n == 2) && !parse_length (strv[1], -DBL_MAX, DBL_MAX, &y)))
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
            !parse_length (strv[0], -DBL_MAX, DBL_MAX, &x) ||
            ((n == 2) && !parse_length (strv[1], -DBL_MAX, DBL_MAX, &y)))
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
             (!parse_length (strv[1], -DBL_MAX, DBL_MAX, &x) ||
              !parse_length (strv[2], -DBL_MAX, DBL_MAX, &y))))
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
            !parse_length (strv[0], -DBL_MAX, DBL_MAX, &angle))
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
            !parse_length (strv[0], -DBL_MAX, DBL_MAX, &angle))
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
      string_append_double (s, tf->transforms[0].translate.x);
      g_string_append_c (s, ' ');
      string_append_double (s, tf->transforms[0].translate.y);
      break;

    case TRANSFORM_SCALE:
      string_append_double (s, tf->transforms[0].scale.x);
      g_string_append_c (s, ' ');
      string_append_double (s, tf->transforms[0].scale.y);
      break;

    case TRANSFORM_ROTATE:
      string_append_double (s, tf->transforms[0].rotate.angle);
      g_string_append_c (s, ' ');
      string_append_double (s, tf->transforms[0].rotate.x);
      g_string_append_c (s, ' ');
      string_append_double (s, tf->transforms[0].rotate.y);
      break;

    case TRANSFORM_SKEW_X:
      string_append_double (s, tf->transforms[0].skew_x.angle);
      break;

    case TRANSFORM_SKEW_Y:
      string_append_double (s, tf->transforms[0].skew_y.angle);
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
          g_string_append (s, "translate(");
          string_append_double (s, t->translate.x);
          g_string_append (s, ", ");
          string_append_double (s, t->translate.y);
          g_string_append (s, ")");
          break;
        case TRANSFORM_SCALE:
          g_string_append (s, "scale(");
          string_append_double (s, t->scale.x);
          g_string_append (s, ", ");
          string_append_double (s, t->scale.y);
          g_string_append (s, ")");
          break;
        case TRANSFORM_ROTATE:
          g_string_append (s, "rotate(");
          string_append_double (s, t->rotate.angle);
          g_string_append (s, ", ");
          string_append_double (s, t->rotate.x);
          g_string_append (s, ", ");
          string_append_double (s, t->rotate.y);
          g_string_append (s, ")");
          break;
        case TRANSFORM_SKEW_X:
          g_string_append (s, "skewX(");
          string_append_double (s, t->skew_x.angle);
          g_string_append (s, ")");
          break;
        case TRANSFORM_SKEW_Y:
          g_string_append (s, "skewY(");
          string_append_double (s, t->skew_y.angle);
          g_string_append (s, ")");
          break;
        case TRANSFORM_MATRIX:
          g_string_append (s, "matrix(");
          string_append_double (s, t->matrix.xx);
          g_string_append (s, ", ");
          string_append_double (s, t->matrix.yx);
          g_string_append (s, ", ");
          string_append_double (s, t->matrix.xy);
          g_string_append (s, ", ");
          string_append_double (s, t->matrix.yy);
          g_string_append (s, ", ");
          string_append_double (s, t->matrix.dx);
          g_string_append (s, ", ");
          string_append_double (s, t->matrix.dy);
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
                          int             n)
{
  const SvgTransform *tf0 = (const SvgTransform *) value0;
  const SvgTransform *tf1 = (const SvgTransform *) value1;
  SvgTransform *tf;

  /* special-case this one */
  if (tf1->n_transforms == 1 && tf1->transforms[0].type != TRANSFORM_MATRIX)
    {
      PrimitiveTransform *p;

      tf = svg_transform_alloc (tf0->n_transforms + 1);
      memcpy (tf->transforms,
              tf0->transforms,
              tf0->n_transforms * sizeof (PrimitiveTransform));

      p = &tf->transforms[tf0->n_transforms];
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
    }
  else
    {
      /* For the general case, simply concatenate all the transforms */
      tf = svg_transform_alloc (tf0->n_transforms + n * tf1->n_transforms);
      for (unsigned int i = 0; i < n; i++)
        memcpy (&tf->transforms[tf0->n_transforms + i * tf1->n_transforms],
                tf1->transforms,
                tf1->n_transforms * sizeof (PrimitiveTransform));

      memcpy (&tf->transforms[tf0->n_transforms + (n - 1) * tf1->n_transforms],
              tf0->transforms,
              tf0->n_transforms * sizeof (PrimitiveTransform));
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

/* }}} */
/* {{{ Paint */

typedef enum
{
  PAINT_NONE,
  PAINT_COLOR,
  PAINT_SYMBOLIC,
  PAINT_GRADIENT,
} PaintKind;

typedef struct
{
  SvgValue base;
  PaintKind kind;
  union {
    GtkSymbolicColor symbolic;
    GdkRGBA color;
    struct {
      char *ref;
      Shape *shape;
    } gradient;
  };
} SvgPaint;

static void
svg_paint_free (SvgValue *value)
{
  SvgPaint *paint = (SvgPaint *) value;

  if (paint->kind == PAINT_GRADIENT)
    g_free (paint->gradient.ref);

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
      return TRUE;
    case PAINT_SYMBOLIC:
      return paint0->symbolic == paint1->symbolic;
    case PAINT_COLOR:
      return gdk_rgba_equal (&paint0->color, &paint1->color);
    case PAINT_GRADIENT:
      return paint0->gradient.shape == paint1->gradient.shape &&
             g_strcmp0 (paint0->gradient.ref, paint1->gradient.ref) == 0;
    default:
      g_assert_not_reached ();
    }
}

static SvgValue *svg_paint_interpolate (const SvgValue *v0,
                                        const SvgValue *v1,
                                        double          t);
static SvgValue *svg_paint_accumulate  (const SvgValue *v0,
                                        const SvgValue *v1,
                                        int             n);
static void      svg_paint_print       (const SvgValue *v0,
                                        GString        *string);

static const SvgValueClass SVG_PAINT_CLASS = {
  "SvgPaint",
  svg_paint_free,
  svg_paint_equal,
  svg_paint_interpolate,
  svg_paint_accumulate,
  svg_paint_print
};

static gboolean
parse_symbolic_color (const char       *value,
                      GtkSymbolicColor *symbolic)
{
  const char *sym[] = {
    "foreground", "error", "warning", "success", "accent"
  };
  unsigned int u = 0;

  if (!parse_enum (value, sym, G_N_ELEMENTS (sym), &u))
    return FALSE;

  *symbolic = u;
  return TRUE;
}

static SvgValue *
svg_paint_new_none (void)
{
  static SvgPaint none = { { &SVG_PAINT_CLASS, 1 }, .kind = PAINT_NONE };

  return svg_value_ref ((SvgValue *) &none);
}

static SvgValue *
svg_paint_new_symbolic (GtkSymbolicColor symbolic)
{
  static SvgPaint sym[] = {
    { { &SVG_PAINT_CLASS, 1 }, .kind = PAINT_SYMBOLIC, .symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND },
    { { &SVG_PAINT_CLASS, 1 }, .kind = PAINT_SYMBOLIC, .symbolic = GTK_SYMBOLIC_COLOR_ERROR },
    { { &SVG_PAINT_CLASS, 1 }, .kind = PAINT_SYMBOLIC, .symbolic = GTK_SYMBOLIC_COLOR_WARNING },
    { { &SVG_PAINT_CLASS, 1 }, .kind = PAINT_SYMBOLIC, .symbolic = GTK_SYMBOLIC_COLOR_SUCCESS },
    { { &SVG_PAINT_CLASS, 1 }, .kind = PAINT_SYMBOLIC, .symbolic = GTK_SYMBOLIC_COLOR_ACCENT },
  };

  return svg_value_ref ((SvgValue *) &sym[symbolic]);
}

static SvgPaint default_rgba[] = {
  { { &SVG_PAINT_CLASS, 1 }, .kind = PAINT_COLOR, .color = { 0, 0, 0, 1 } },
  { { &SVG_PAINT_CLASS, 1 }, .kind = PAINT_COLOR, .color = { 1, 1, 1, 1 } },
};

static SvgValue *
svg_paint_new_rgba (const GdkRGBA *rgba)
{
  if (gdk_rgba_equal (rgba, &default_rgba[0].color))
    return svg_value_ref ((SvgValue *) &default_rgba[0]);
  else if (gdk_rgba_equal (rgba, &default_rgba[1].color))
    return svg_value_ref ((SvgValue *) &default_rgba[1]);
  else
    {
      SvgPaint *paint = (SvgPaint *) svg_value_alloc (&SVG_PAINT_CLASS,
                                                      sizeof (SvgPaint));
      paint->color = *rgba;
      paint->kind = PAINT_COLOR;

      return (SvgValue *) paint;
    }
}

static SvgValue *
svg_paint_new_black (void)
{
  return svg_value_ref ((SvgValue *) &default_rgba[0]);
}

static SvgValue *
svg_paint_new_gradient (Shape      *shape,
                        const char *ref)
{
  SvgPaint *paint = (SvgPaint *) svg_value_alloc (&SVG_PAINT_CLASS,
                                                  sizeof (SvgPaint));
  paint->kind = PAINT_GRADIENT;
  paint->gradient.shape = shape;
  paint->gradient.ref = g_strdup (ref);

  return (SvgValue *) paint;
}

static SvgValue *
svg_paint_parse (const char *value)
{
  GdkRGBA color;

  if (strcmp (value, "none") == 0)
    {
      return svg_paint_new_none ();
    }
  else if (gdk_rgba_parse (&color, value))
    {
      return svg_paint_new_rgba (&color);
    }
  else
    {
      GtkCssParser *parser;
      GBytes *bytes;
      char *url;
      GtkSymbolicColor symbolic;
      SvgValue *paint = NULL;

      bytes = g_bytes_new_static (value, strlen (value));
      parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, NULL, NULL);

      url = gtk_css_parser_consume_url (parser);
      if (url)
        {
          if (g_str_has_prefix (url, "#gpa:") &&
              parse_symbolic_color (url + strlen ("#gpa:"), &symbolic))
            paint = svg_paint_new_symbolic (symbolic);
          else if (url[0] == '#')
            paint = svg_paint_new_gradient (NULL, url + 1);
          else
            paint = svg_paint_new_gradient (NULL, url);
        }

      g_free (url);
      gtk_css_parser_unref (parser);
      g_bytes_unref (bytes);

      return paint;
    }
}

static SvgValue *
svg_paint_parse_gpa (const char *value)
{
  GtkSymbolicColor symbolic;
  GdkRGBA rgba;

  if (strcmp (value, "none") == 0)
    return svg_paint_new_none ();
  else if (parse_symbolic_color (value, &symbolic))
    return svg_paint_new_symbolic (symbolic);
  else if (gdk_rgba_parse (&rgba, value))
    return svg_paint_new_rgba (&rgba);

  return NULL;
}

static void
svg_paint_print (const SvgValue *value,
                 GString        *s)
{
  const SvgPaint *paint = (const SvgPaint *) value;
  struct {
    const char *symbolic;
    const char *fallback;
  } colors[] = {
    { "foreground", "rgb(0,0,0)", },
    { "error",      "rgb(204,0,0)", },
    { "warning",    "rgb(245,121,0)", },
    { "success",    "rgb(51,209,122)", },
    { "accent",     "rgb(0,34,255)", },
  };

  switch (paint->kind)
    {
    case PAINT_NONE:
      g_string_append (s, "none");
      break;

    case PAINT_COLOR:
      gdk_rgba_print (&paint->color, s);
      break;

    case PAINT_SYMBOLIC:
      g_string_append_printf (s, "url(\"#gpa:%s\") %s",
                              colors[paint->symbolic].symbolic,
                              colors[paint->symbolic].fallback);
      break;

    case PAINT_GRADIENT:
      g_string_append_printf (s, "url(#%s)", paint->gradient.ref);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
svg_paint_print_gpa (const SvgValue *value,
                     GString        *s)
{
  const SvgPaint *paint = (const SvgPaint *) value;
  const char *symbolic[] = {
    "foreground", "error", "warning", "success", "accent",
  };

  switch (paint->kind)
    {
    case PAINT_NONE:
      g_string_append (s, "none");
      break;

    case PAINT_COLOR:
      gdk_rgba_print (&paint->color, s);
      break;

    case PAINT_SYMBOLIC:
      g_string_append (s, symbolic[paint->symbolic]);
      break;

    case PAINT_GRADIENT:
      g_string_append_printf  (s, "url(#%s)", paint->gradient.ref);
      break;

    default:
      g_assert_not_reached ();
    }
}

static SvgValue *
svg_paint_resolve (SvgValue      *value,
                   const GdkRGBA *colors,
                   size_t         n_colors)
{
  const SvgPaint *paint = (const SvgPaint *) value;

  if (paint->kind == PAINT_SYMBOLIC)
    {
      if (paint->symbolic < n_colors)
        return svg_paint_new_rgba (&colors[paint->symbolic]);
      else
        return svg_paint_new_rgba (&colors[GTK_SYMBOLIC_COLOR_FOREGROUND]);
    }
  else
    {
      return svg_value_ref (value);
    }
}

static SvgValue *
svg_paint_interpolate (const SvgValue *value0,
                       const SvgValue *value1,
                       double          t)
{
  const SvgPaint *p0 = (const SvgPaint *) value0;
  const SvgPaint *p1 = (const SvgPaint *) value1;

  if (p0->kind == PAINT_COLOR || p1->kind == PAINT_COLOR)
    {
      SvgPaint *paint;

      paint = (SvgPaint *) svg_value_alloc (&SVG_PAINT_CLASS, sizeof (SvgPaint));

      paint->kind = PAINT_COLOR;
      paint->color.red = lerp (t, p0->color.red, p1->color.red);
      paint->color.green = lerp (t, p0->color.green, p1->color.green);
      paint->color.blue = lerp (t, p0->color.blue, p1->color.blue);
      paint->color.alpha = lerp (t, p0->color.alpha, p1->color.alpha);

      return (SvgValue *) paint;
    }

  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_paint_accumulate (const SvgValue *value0,
                      const SvgValue *value1,
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
      paint->color.red = accumulate (p0->color.red, p1->color.red, n);
      paint->color.green = accumulate (p0->color.green, p1->color.green, n);
      paint->color.blue = accumulate (p0->color.blue, p1->color.blue, n);
      paint->color.alpha = accumulate (p0->color.alpha, p1->color.alpha, n);

      return (SvgValue *) paint;
    }

  return svg_value_ref ((SvgValue *) value0);
}

/* }}} */
/* {{{ Filters */

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
} FilterKind;

static struct {
  FilterKind kind;
  const char *name;
} filter_desc[] = {
  { FILTER_NONE,        "none"        },
  { FILTER_BLUR,        "blur"        },
  { FILTER_BRIGHTNESS,  "brightness"  },
  { FILTER_CONTRAST,    "contrast"    },
  { FILTER_GRAYSCALE,   "grayscale"   },
  { FILTER_HUE_ROTATE,  "hue-rotate"  },
  { FILTER_INVERT,      "invert"      },
  { FILTER_OPACITY,     "opacity"     },
  { FILTER_SATURATE,    "saturate"    },
  { FILTER_SEPIA,       "sepia"       },
  { FILTER_ALPHA_LEVEL, "alpha-level" },
};

typedef struct
{
  FilterKind kind;
  double value;
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
      else if (f0->functions[i].kind == FILTER_NONE)
        return TRUE;
      else
        return f0->functions[i].value == f1->functions[i].value;
    }

  return TRUE;
}

static SvgValue *svg_filter_interpolate (const SvgValue *v0,
                                         const SvgValue *v1,
                                         double          t);
static SvgValue *svg_filter_accumulate  (const SvgValue *v0,
                                         const SvgValue *v1,
                                         int             n);
static void      svg_filter_print       (const SvgValue *v0,
                                         GString        *string);

static const SvgValueClass SVG_FILTER_CLASS = {
  "SvgFilter",
  svg_filter_free,
  svg_filter_equal,
  svg_filter_interpolate,
  svg_filter_accumulate,
  svg_filter_print
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
  static SvgFilter none = { { &SVG_FILTER_CLASS, 1 }, 1, { { FILTER_NONE, } } };
  return svg_value_ref ((SvgValue *) &none);
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

      for (i = 1; i < G_N_ELEMENTS (filter_desc); i++)
        {
          if (gtk_css_parser_has_function (parser, filter_desc[i].name))
            {
              if (!gtk_css_parser_consume_function (parser, 1, 1, css_parser_parse_number, &function.value))
                goto fail;
              function.kind = filter_desc[i].kind;
              break;
            }
        }
      if (i == G_N_ELEMENTS (filter_desc))
        break;

      g_array_append_val (array, function);
    }

  if (array->len == 0)
    {
      gtk_css_parser_error_syntax (parser, "Expected a filter");
      goto fail;
    }

  filter = svg_filter_alloc (array->len);
  memcpy (filter->functions, array->data, sizeof (FilterFunction) * array->len);

  g_array_free (array, TRUE);

  return (SvgValue *) filter;

fail:
  g_array_free (array, TRUE);
  return NULL;
}

static SvgValue *
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
      else
        {
          g_string_append_printf (s, "%s(", filter_desc[function->kind].name);
          string_append_double (s, function->value);
          g_string_append (s, ")");
        }
    }
}

static SvgValue *
svg_filter_interpolate (const SvgValue *value0,
                        const SvgValue *value1,
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
    }

  f = svg_filter_alloc (f0->n_functions);

  for (unsigned int i = 0; i < f0->n_functions; i++)
    {
      f->functions[i].kind = f0->functions[i].kind;
      if (f->functions[i].kind != FILTER_NONE)
        f->functions[i].value = lerp (t, f0->functions[i].value, f1->functions[i].value);
    }

  return (SvgValue *) f;
}

static SvgValue *
svg_filter_accumulate (const SvgValue *value0,
                       const SvgValue *value1,
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

  return (SvgValue *) f;
}

#define R 0.2126
#define G 0.7152
#define B 0.0722

static gboolean
svg_filter_get_matrix (FilterFunction    *f,
                       graphene_matrix_t *matrix,
                       graphene_vec4_t   *offset)
{
  double v, c, s;

  v = f->value;
  switch (f->kind)
    {
    case FILTER_NONE:
    case FILTER_BLUR:
    case FILTER_ALPHA_LEVEL:
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
      c = cos (v);
      s = sin (v);
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
  double dashes[2];
} SvgDashArray;

static unsigned int
svg_dash_array_size (unsigned int n)
{
  return sizeof (SvgDashArray) + (n - 2) * sizeof (double);
}

static void
svg_dash_array_free (SvgValue *da)
{
  g_free (da);
}

static gboolean
svg_dash_array_equal (const SvgValue *value1,
                      const SvgValue *value2)
{
  const SvgDashArray *da1 = (const SvgDashArray *) value1;
  const SvgDashArray *da2 = (const SvgDashArray *) value2;

  if (da1->kind != da2->kind)
    return FALSE;

  if (da1->kind == DASH_ARRAY_NONE)
    return TRUE;

  if (da1->n_dashes != da2->n_dashes)
    return FALSE;

  for (unsigned int i = 0; i < da1->n_dashes; i++)
    {
      if (da1->dashes[i] != da2->dashes[i])
        return FALSE;
    }

  return TRUE;
}

static SvgValue * svg_dash_array_interpolate (const SvgValue *value1,
                                              const SvgValue *value2,
                                              double          t);

static void svg_dash_array_print (const SvgValue *value,
                                  GString        *s);

static SvgValue * svg_dash_array_accumulate (const SvgValue *value1,
                                             const SvgValue *value2,
                                             int             n);

static const SvgValueClass SVG_DASH_ARRAY_CLASS = {
  "SvgFilter",
  svg_dash_array_free,
  svg_dash_array_equal,
  svg_dash_array_interpolate,
  svg_dash_array_accumulate,
  svg_dash_array_print
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
  static SvgDashArray none = { { &SVG_DASH_ARRAY_CLASS, 1 }, DASH_ARRAY_NONE, 0 };
  return svg_value_ref ((SvgValue *) &none);
}

static SvgValue *
svg_dash_array_new (double       *values,
                    unsigned int  n)
{
  static SvgDashArray da01 = { { &SVG_DASH_ARRAY_CLASS, 1 }, DASH_ARRAY_DASHES, 2, { 0, 1 } };
  static SvgDashArray da10 = { { &SVG_DASH_ARRAY_CLASS, 1 }, DASH_ARRAY_DASHES, 2, { 1, 0 } };

  if (n == 2 && values[0] == 0 && values[1] == 1)
    return svg_value_ref ((SvgValue *) &da01);
  else if (n == 2 && values[0] == 1 && values[1] == 0)
    return svg_value_ref ((SvgValue *) &da10);
  else
    {
      SvgDashArray *a = svg_dash_array_alloc (n);
      memcpy (a->dashes, values, sizeof (double) * n);
      a->kind = DASH_ARRAY_DASHES;
      return (SvgValue *) a;
    }
}

static SvgValue *
svg_dash_array_parse (const char *value)
{
  if (strcmp (value, "none") == 0)
    {
      return svg_dash_array_new_none ();
    }
  else
    {
      GStrv strv;
      unsigned int n;
      SvgDashArray *dashes;

      strv = g_strsplit (value, " ", 0);
      n = g_strv_length (strv);

      dashes = svg_dash_array_alloc (n);
      dashes->kind = DASH_ARRAY_DASHES;

      for (unsigned int i = 0; strv[i]; i++)
        {
          if (!parse_number (strv[i], -DBL_MAX, DBL_MAX, &dashes->dashes[i]))
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
          string_append_double (s, dashes->dashes[i]);
        }
    }
}

static SvgValue *
svg_dash_array_interpolate (const SvgValue *value0,
                            const SvgValue *value1,
                            double          t)
{
  const SvgDashArray *a0 = (const SvgDashArray *) value0;
  const SvgDashArray *a1 = (const SvgDashArray *) value1;
  SvgDashArray *a;
  unsigned int n_dashes;

  if (a0->kind != a1->kind)
    {
      if (t < 0.5)
        return svg_value_ref ((SvgValue *) value0);
      else
        return svg_value_ref ((SvgValue *) value1);
    }

  if (a0->kind == DASH_ARRAY_NONE)
    return (SvgValue *) svg_dash_array_new_none ();

  n_dashes = lcm (a0->n_dashes, a1->n_dashes);

  a = svg_dash_array_alloc (n_dashes);
  a->kind = a0->kind;

  for (unsigned int i = 0; i < n_dashes; i++)
    a->dashes[i] = lerp (t, a0->dashes[i % a0->n_dashes],
                            a1->dashes[i % a1->n_dashes]);

  return (SvgValue *) a;
}

static SvgValue *
svg_dash_array_accumulate (const SvgValue *value0,
                           const SvgValue *value1,
                           int             n)
{
  return NULL;
}

/* }}} */
/* {{{ Paths */

typedef struct
{
  SvgValue base;
  GskPath *path;
} SvgPath;

static void
svg_path_free (SvgValue *value)
{
  SvgPath *p = (SvgPath *) value;
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
                     int             n)
{
  return NULL;
}

static void
svg_path_print (const SvgValue *value,
                GString        *string)
{
  const SvgPath *p = (const SvgPath *) value;

  if (p->path)
    gsk_path_print (p->path, string);
  else
    g_string_append (string, "none");
}

static SvgValue * svg_path_interpolate (const SvgValue *value1,
                                        const SvgValue *value2,
                                        double          t);

static const SvgValueClass SVG_PATH_CLASS = {
  "SvgPath",
  svg_path_free,
  svg_path_equal,
  svg_path_interpolate,
  svg_path_accumulate,
  svg_path_print
};

static SvgValue *
svg_path_new_none (void)
{
  static SvgPath none = { { &SVG_PATH_CLASS, 1 }, NULL };
  return svg_value_ref ((SvgValue *) &none);
}

static SvgValue *
svg_path_new (GskPath *path)
{
  SvgPath *result;

  result = (SvgPath *) svg_value_alloc (&SVG_PATH_CLASS, sizeof (SvgPath));
  result->path = gsk_path_ref (path);

  return (SvgValue *) result;
}

static SvgValue *
svg_path_parse (const char *value)
{
  GskPath *path;
  SvgValue *v;

  if (strcmp (value, "none") == 0)
    return svg_path_new_none ();

  path = gsk_path_parse (value);
  if (!path)
    return NULL;

  v = svg_path_new (path);
  gsk_path_unref (path);

  return v;
}

static GskPath *
svg_path_get (const SvgValue *value)
{
  const SvgPath *p = (const SvgPath *) value;

  return p->path;
}

typedef struct
{
  GskPathOperation op;
  graphene_point_t pts[4];
  float weight;
} PathOp;

static gboolean
collect_op (GskPathOperation        op,
            const graphene_point_t *pts,
            gsize                   n_pts,
            float                   weight,
            gpointer                user_data)
{
  GArray *array = user_data;
  PathOp path_op;

  path_op.op = op;
  memset (path_op.pts, 0, sizeof (graphene_point_t) * 4);
  memcpy (path_op.pts, pts, sizeof (graphene_point_t) * n_pts);
  path_op.weight = weight;

  g_array_append_val (array, path_op);

  return TRUE;
}

static GArray *
path_explode (GskPath *path)
{
  GArray *array = g_array_new (FALSE, FALSE, sizeof (PathOp));

  gsk_path_foreach (path,
                    GSK_PATH_FOREACH_ALLOW_QUAD  |
                    GSK_PATH_FOREACH_ALLOW_CUBIC |
                    GSK_PATH_FOREACH_ALLOW_CONIC,
                    collect_op,
                    array);

  return array;
}

static GskPath *
path_interpolate (GskPath *p0,
                  GskPath *p1,
                  double   t)
{
  GskPathBuilder *builder;
  GArray *a0, *a1;

  a0 = path_explode (p0);
  a1 = path_explode (p1);

  if (a0->len != a1->len)
    {
      g_array_unref (a0);
      g_array_unref (a1);
      return NULL;
    }

  builder = gsk_path_builder_new ();

  for (unsigned int i = 0; i < a0->len; i++)
    {
      PathOp *op0 = &g_array_index (a0, PathOp, i);
      PathOp *op1 = &g_array_index (a1, PathOp, i);

      if (op0->op != op1->op)
        {
          g_array_unref (a0);
          g_array_unref (a1);
          gsk_path_builder_unref (builder);
          return NULL;
        }

      switch (op0->op)
        {
        case GSK_PATH_MOVE:
          gsk_path_builder_move_to (builder,
                                    lerp (t, op0->pts[0].x, op1->pts[0].x),
                                    lerp (t, op0->pts[0].y, op1->pts[0].y));
          break;
        case GSK_PATH_CLOSE:
          gsk_path_builder_close (builder);
          break;
        case GSK_PATH_LINE:
          gsk_path_builder_line_to (builder,
                                    lerp (t, op0->pts[1].x, op1->pts[1].x),
                                    lerp (t, op0->pts[1].y, op1->pts[1].y));
          break;
        case GSK_PATH_QUAD:
          gsk_path_builder_quad_to (builder,
                                    lerp (t, op0->pts[1].x, op1->pts[1].x),
                                    lerp (t, op0->pts[1].y, op1->pts[1].y),
                                    lerp (t, op0->pts[2].x, op1->pts[2].x),
                                    lerp (t, op0->pts[2].y, op1->pts[2].y));
          break;
        case GSK_PATH_CUBIC:
          gsk_path_builder_cubic_to (builder,
                                     lerp (t, op0->pts[1].x, op1->pts[1].x),
                                     lerp (t, op0->pts[1].y, op1->pts[1].y),
                                     lerp (t, op0->pts[2].x, op1->pts[2].x),
                                     lerp (t, op0->pts[2].y, op1->pts[2].y),
                                     lerp (t, op0->pts[3].x, op1->pts[3].x),
                                     lerp (t, op0->pts[3].y, op1->pts[3].y));
          break;
        case GSK_PATH_CONIC:
          gsk_path_builder_conic_to (builder,
                                     lerp (t, op0->pts[1].x, op1->pts[1].x),
                                     lerp (t, op0->pts[1].y, op1->pts[1].y),
                                     lerp (t, op0->pts[2].x, op1->pts[2].x),
                                     lerp (t, op0->pts[2].y, op1->pts[2].y),
                                     lerp (t, op0->weight, op1->weight));
          break;
        default:
          g_assert_not_reached ();
        }
    }

  g_array_unref (a0);
  g_array_unref (a1);

  return gsk_path_builder_free_to_path (builder);
}

static SvgValue *
svg_path_interpolate (const SvgValue *value0,
                      const SvgValue *value1,
                      double          t)
{
  const SvgPath *p0 = (const SvgPath *) value0;
  const SvgPath *p1 = (const SvgPath *) value1;
  GskPath *path;

  path = path_interpolate (p0->path, p1->path, t);

  if (path)
    {
      SvgValue *res = svg_path_new (path);
      gsk_path_unref (path);
      return res;
    }

  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

/* }}} */
/* {{{ Points */

typedef struct
{
  SvgValue base;
  unsigned int n_values;
  double values[1];
} SvgPoints;

static unsigned int
svg_points_size (unsigned int n)
{
  return sizeof (SvgPoints) + (n - 1) * sizeof (double);
}

static void
svg_points_free (SvgValue *value)
{
  g_free (value);
}

static gboolean
svg_points_equal (const SvgValue *value0,
                  const SvgValue *value1)
{
  const SvgPoints *p0 = (const SvgPoints *) value0;
  const SvgPoints *p1 = (const SvgPoints *) value1;

  if (p0->n_values != p1->n_values)
    return FALSE;

  for (unsigned int i = 0; i < p0->n_values; i++)
    {
      if (p0->values[i] != p1->values[i])
        return FALSE;
    }

  return TRUE;
}

static SvgValue *
svg_points_accumulate (const SvgValue *value0,
                       const SvgValue *value1,
                       int             n)
{
  return NULL;
}

static void
svg_points_print (const SvgValue *value,
                  GString        *string)
{
  const SvgPoints *p = (const SvgPoints *) value;

  for (unsigned int i = 0; i < p->n_values; i++)
    {
      if (i > 0)
        g_string_append_c (string, ' ');
      string_append_double (string, p->values[i]);
    }
}

static SvgValue * svg_points_interpolate (const SvgValue *value0,
                                          const SvgValue *value1,
                                          double          t);

static const SvgValueClass SVG_POINTS_CLASS = {
  "SvgPoints",
  svg_points_free,
  svg_points_equal,
  svg_points_interpolate,
  svg_points_accumulate,
  svg_points_print
};

static SvgValue *
svg_points_new_none (void)
{
  SvgPoints *result;

  result = (SvgPoints *) svg_value_alloc (&SVG_POINTS_CLASS, svg_points_size (1));
  result->n_values = 0;

  return (SvgValue *) result;
}

static SvgValue *
svg_points_parse (const char *value)
{
  SvgPoints *result;
  GArray *values;

  if (strcmp (value, "none") == 0)
    return svg_path_new_none ();

  values = parse_numbers2 (value, " ", -DBL_MAX, DBL_MAX);
  if (values->len % 2 != 0)
    g_array_remove_index (values, values->len - 1);

  result = (SvgPoints *) svg_value_alloc (&SVG_POINTS_CLASS, svg_points_size (values->len));
  result->n_values = values->len;

  memcpy (result->values, values->data, sizeof (double) * values->len);

  g_array_unref (values);

  return (SvgValue *) result;
}

static SvgValue *
svg_points_interpolate (const SvgValue *value0,
                        const SvgValue *value1,
                        double          t)
{
  const SvgPoints *p0 = (const SvgPoints *) value0;
  const SvgPoints *p1 = (const SvgPoints *) value1;
  SvgPoints *p;

  if (p0->n_values != p1->n_values)
    {
      if (t < 0.5)
        return svg_value_ref ((SvgValue *) value0);
      else
        return svg_value_ref ((SvgValue *) value1);
    }

  p = (SvgPoints *) svg_value_alloc (&SVG_POINTS_CLASS, svg_points_size (p0->n_values));
  p->n_values = p0->n_values;

  for (unsigned int i = 0; i < p0->n_values; i++)
    p->values[i] = lerp (t, p0->values[i], p1->values[i]);

  return (SvgValue *) p;
}

/* }}} */
/* {{{ Clips */

typedef enum
{
  CLIP_NONE,
  CLIP_PATH,
  CLIP_REF,
} ClipKind;

typedef struct
{
  SvgValue base;
  ClipKind kind;

  union {
    GskPath *path;
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
    g_clear_pointer (&clip->path, gsk_path_unref);
  else if (clip->kind == CLIP_REF)
    g_free (clip->ref.ref);

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
      return gsk_path_equal (c0->path, c1->path);
    case CLIP_REF:
      return c0->ref.shape == c1->ref.shape;
    default:
      g_assert_not_reached ();
    }
}

static SvgValue * svg_clip_interpolate (const SvgValue *value0,
                                        const SvgValue *value1,
                                        double          t);

static SvgValue *
svg_clip_accumulate (const SvgValue *value0,
                     const SvgValue *value1,
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
      g_string_append (string, "path(\"");
      gsk_path_print (c->path, string);
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
  svg_clip_print
};

static SvgValue *
svg_clip_new_none (void)
{
  static SvgClip none = { { &SVG_CLIP_CLASS, 1 }, CLIP_NONE };
  return svg_value_ref ((SvgValue *) &none);
}

static SvgValue *
svg_clip_new_path (GskPath *path)
{
  SvgClip *result;

  result = (SvgClip *) svg_value_alloc (&SVG_CLIP_CLASS, sizeof (SvgClip));
  result->kind = CLIP_PATH;
  result->path = gsk_path_ref (path);

  return (SvgValue *) result;
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
                      double          t)
{
  const SvgClip *c0 = (const SvgClip *) value0;
  const SvgClip *c1 = (const SvgClip *) value1;
  GskPath *path;

  if (c0->kind == c1->kind)
    {
      switch (c0->kind)
        {
        case CLIP_NONE:
          return svg_clip_new_none ();

        case CLIP_PATH:
          path = path_interpolate (c0->path, c1->path, t);
          if (path)
            {
              SvgValue *v = svg_clip_new_path (path);
              gsk_path_unref (path);
              return v;
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

static guint
parse_clip_path_arg (GtkCssParser *parser,
                     guint         n,
                     gpointer      data)
{
  char *string;

  string = gtk_css_parser_consume_string (parser);
  if (string)
    {
      GskPath *path = gsk_path_parse (string);
      if (path)
        {
          *((GskPath **) data) = path;
          g_free (string);
          return 1;
        }
      g_free (string);
    }

  return 0;
}

static SvgValue *
svg_clip_parse (const char *value)
{
  if (strcmp (value, "none") == 0)
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
          GskPath *path = NULL;

          if (gtk_css_parser_consume_function (parser, 1, 1, parse_clip_path_arg, &path))
            {
              res = svg_clip_new_path (path);
              gsk_path_unref (path);
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
  svg_mask_print
};

static SvgValue *
svg_mask_new_none (void)
{
  static SvgMask none = { { &SVG_MASK_CLASS, 1 }, MASK_NONE };
  return svg_value_ref ((SvgValue *) &none);
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
  if (strcmp (value, "none") == 0)
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
/* {{{ References */

typedef enum
{
  HREF_NONE,
  HREF_REF,
} HrefKind;

typedef struct
{
  SvgValue base;
  HrefKind kind;

  char *ref;
  Shape *shape;
} SvgHref;

static void
svg_href_free (SvgValue *value)
{
  const SvgHref *r = (const SvgHref *) value;

  if (r->kind == HREF_REF)
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
    case MASK_NONE:
      return TRUE;
    case MASK_REF:
      return r1->shape == r2->shape &&
             g_strcmp0 (r1->ref, r2->ref) == 0;
    default:
      g_assert_not_reached ();
    }
}

static SvgValue *
svg_href_interpolate (const SvgValue *value1,
                      const SvgValue *value2,
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
      g_string_append_printf (string, "#%s", r->ref);
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
  svg_href_print
};

static SvgValue *
svg_href_new_none (void)
{
  static SvgHref none = { { &SVG_HREF_CLASS, 1 }, HREF_NONE };
  return svg_value_ref ((SvgValue *) &none);
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
svg_href_parse (const char *value)
{
  if (strcmp (value, "none") == 0)
    return svg_href_new_none ();
  else if (value[0] == '#')
    return svg_href_new_ref (value + 1);
  else
    return svg_href_new_ref (value);
}

/* }}} */
/* {{{ Color stops */

typedef enum
{
  COLOR_STOP_OFFSET,
  COLOR_STOP_COLOR,
  COLOR_STOP_OPACITY,
} StopProperties;

#define N_STOP_PROPS (COLOR_STOP_OPACITY + 1)

typedef struct
{
  SvgValue *base[N_STOP_PROPS];
  SvgValue *current[N_STOP_PROPS];
} ColorStop;

static void
color_stop_free (gpointer v)
{
  ColorStop *stop = v;

  for (unsigned int i = 0; i < N_STOP_PROPS; i++)
    {
      g_clear_pointer (&stop->base[i], svg_value_unref);
      g_clear_pointer (&stop->current[i], svg_value_unref);
    }
  g_free (stop);
}

/* }}} */
/* {{{ Attributes */

static SvgValue *
parse_opacity (const char *value)
{
  return svg_number_parse (value, 0, 1, PERCENTAGE);
}

static SvgValue *
parse_stroke_width (const char *value)
{
  return svg_number_parse (value, 0, DBL_MAX, LENGTH);
}

static SvgValue *
parse_miterlimit (const char *value)
{
  return svg_number_parse (value, 0, DBL_MAX, 0);
}

static SvgValue *
parse_dash_offset (const char *value)
{
  return svg_number_parse (value, -DBL_MAX, DBL_MAX, PERCENTAGE);
}

static SvgValue *
parse_any_length (const char *value)
{
  return svg_number_parse (value, -DBL_MAX, DBL_MAX, LENGTH);
}

static SvgValue *
parse_positive_length (const char *value)
{
  return svg_number_parse (value, 0, DBL_MAX, LENGTH);
}

static SvgValue *
parse_length_percentage (const char *value)
{
  return svg_number_parse (value, -DBL_MAX, DBL_MAX, PERCENTAGE | LENGTH);
}

static SvgValue *
parse_positive_length_percentage (const char *value)
{
  return svg_number_parse (value, 0, DBL_MAX, PERCENTAGE | LENGTH);
}

static SvgValue *
parse_gradient_pos (const char *value)
{
  return svg_number_parse (value, 0, DBL_MAX, PERCENTAGE);
}

static SvgValue *
parse_offset (const char *value)
{
  return svg_number_parse (value, 0, 1, PERCENTAGE);
}

typedef struct
{
  const char *name;
  ShapeAttr id;
  unsigned int inherited : 1;
  unsigned int discrete  : 1;
  unsigned int presentation : 1;
  SvgValue * (* parse_value)      (const char *value);
  SvgValue * (* parse_for_values) (const char *value);
  SvgValue *initial_value;
} ShapeAttribute;

static ShapeAttribute shape_attrs[] = {
  { .id = SHAPE_ATTR_VISIBILITY,
    .name = "visibility",
    .inherited = 1,
    .discrete = 1,
    .presentation = 0,
    .parse_value = svg_visibility_parse,
  },
  { .id = SHAPE_ATTR_TRANSFORM,
    .name = "transform",
    .inherited = 0,
    .discrete = 0,
    .presentation = 1,
    .parse_value = svg_transform_parse,
  },
  { .id = SHAPE_ATTR_OPACITY,
    .name = "opacity",
    .inherited = 0,
    .discrete = 0,
    .presentation = 0,
    .parse_value = parse_opacity,
  },
  { .id = SHAPE_ATTR_FILTER,
    .name = "filter",
    .inherited = 0,
    .discrete = 0,
    .presentation = 0,
    .parse_value = svg_filter_parse,
  },
  { .id = SHAPE_ATTR_CLIP_PATH,
    .name = "clip-path",
    .inherited = 0,
    .discrete = 1,
    .presentation = 1,
    .parse_value = svg_clip_parse,
  },
  { .id = SHAPE_ATTR_CLIP_RULE,
    .name = "clip-rule",
    .inherited = 1,
    .discrete = 1,
    .presentation = 1,
    .parse_value = svg_fill_rule_parse,
  },
  { .id = SHAPE_ATTR_MASK,
    .name = "mask",
    .inherited = 0,
    .discrete = 1,
    .presentation = 1,
    .parse_value = svg_mask_parse,
  },
  { .id = SHAPE_ATTR_MASK_TYPE,
    .name = "mask-type",
    .inherited = 0,
    .discrete = 1,
    .presentation = 0,
    .parse_value = svg_mask_type_parse,
  },
  { .id = SHAPE_ATTR_FILL,
    .name = "fill",
    .inherited = 1,
    .discrete = 0,
    .presentation = 1,
    .parse_value = svg_paint_parse,
  },
  { .id = SHAPE_ATTR_FILL_OPACITY,
    .name = "fill-opacity",
    .inherited = 1,
    .discrete = 0,
    .presentation = 1,
    .parse_value = parse_opacity,
  },
  { .id = SHAPE_ATTR_FILL_RULE,
    .name = "fill-rule",
    .inherited = 1,
    .discrete = 1,
    .presentation = 1,
    .parse_value = svg_fill_rule_parse,
  },
  { .id = SHAPE_ATTR_STROKE,
    .name = "stroke",
    .inherited = 1,
    .discrete = 0,
    .presentation = 1,
    .parse_value = svg_paint_parse,
  },
  { .id = SHAPE_ATTR_STROKE_OPACITY,
    .name = "stroke-opacity",
    .inherited = 1,
    .discrete = 0,
    .presentation = 1,
    .parse_value = parse_opacity,
  },
  { .id = SHAPE_ATTR_STROKE_WIDTH,
    .name = "stroke-width",
    .inherited = 1,
    .discrete = 0,
    .presentation = 1,
    .parse_value = parse_stroke_width,
  },
  { .id = SHAPE_ATTR_STROKE_LINECAP,
    .name = "stroke-linecap",
    .inherited = 1,
    .discrete = 1,
    .presentation = 1,
    .parse_value = svg_linecap_parse,
  },
  { .id = SHAPE_ATTR_STROKE_LINEJOIN,
    .name = "stroke-linejoin",
    .inherited = 1,
    .discrete = 1,
    .presentation = 1,
    .parse_value = svg_linejoin_parse,
  },
  { .id = SHAPE_ATTR_STROKE_MITERLIMIT,
    .name = "stroke-miterlimit",
    .inherited = 1,
    .discrete = 0,
    .presentation = 1,
    .parse_value = parse_miterlimit,
  },
  { .id = SHAPE_ATTR_STROKE_DASHARRAY,
    .name = "stroke-dasharray",
    .inherited = 1,
    .discrete = 0,
    .presentation = 1,
    .parse_value = svg_dash_array_parse,
  },
  { .id = SHAPE_ATTR_STROKE_DASHOFFSET,
    .name = "stroke-dashoffset",
    .inherited = 1,
    .discrete = 0,
    .presentation = 1,
    .parse_value = parse_dash_offset,
  },
  { .id = SHAPE_ATTR_HREF,
    .name = "href",
    .inherited = 0,
    .discrete = 1,
    .presentation = 0,
    .parse_value = svg_href_parse,
  },
  { .id = SHAPE_ATTR_PATH_LENGTH,
    .name = "pathLength",
    .inherited = 0,
    .discrete = 0,
    .presentation = 0,
    .parse_value = parse_positive_length,
    .parse_for_values = parse_any_length,
  },
  { .id = SHAPE_ATTR_PATH,
    .name = "d",
    .inherited = 0,
    .discrete = 0,
    .presentation = 1,
    .parse_value = svg_path_parse,
  },
  { .id = SHAPE_ATTR_CX,
    .name = "cx",
    .inherited = 0,
    .discrete = 0,
    .presentation = 1,
    .parse_value = parse_length_percentage,
  },
  { .id = SHAPE_ATTR_CY,
    .name = "cy",
    .inherited = 0,
    .discrete = 0,
    .presentation = 1,
    .parse_value = parse_length_percentage,
  },
  { .id = SHAPE_ATTR_R,
    .name = "r",
    .inherited = 0,
    .discrete = 0,
    .presentation = 0,
    .parse_value = parse_positive_length_percentage,
    .parse_for_values = parse_length_percentage,
  },
  { .id = SHAPE_ATTR_X,
    .name = "x",
    .inherited = 0,
    .discrete = 0,
    .presentation = 1,
    .parse_value = parse_length_percentage,
  },
  { .id = SHAPE_ATTR_Y,
    .name = "y",
    .inherited = 0,
    .discrete = 0,
    .presentation = 1,
    .parse_value = parse_length_percentage,
  },
  { .id = SHAPE_ATTR_WIDTH,
    .name = "width",
    .inherited = 0,
    .discrete = 0,
    .presentation = 1,
    .parse_value = parse_positive_length_percentage,
    .parse_for_values = parse_length_percentage,
  },
  { .id = SHAPE_ATTR_HEIGHT,
    .name = "height",
    .inherited = 0,
    .discrete = 0,
    .presentation = 1,
    .parse_value = parse_positive_length_percentage,
    .parse_for_values = parse_length_percentage,
  },
  { .id = SHAPE_ATTR_RX,
    .name = "rx",
    .inherited = 0,
    .discrete = 0,
    .presentation = 1,
    .parse_value = parse_positive_length_percentage,
    .parse_for_values = parse_length_percentage,
  },
  { .id = SHAPE_ATTR_RY,
    .name = "ry",
    .inherited = 0,
    .discrete = 0,
    .presentation = 1,
    .parse_value = parse_positive_length_percentage,
    .parse_for_values = parse_length_percentage,
  },
  { .id = SHAPE_ATTR_X1,
    .name = "x1",
    .inherited = 0,
    .discrete = 0,
    .presentation = 0,
    .parse_value = parse_gradient_pos,
  },
  { .id = SHAPE_ATTR_Y1,
    .name = "y1",
    .inherited = 0,
    .discrete = 0,
    .presentation = 0,
    .parse_value = parse_gradient_pos,
  },
  { .id = SHAPE_ATTR_X2,
    .name = "x2",
    .inherited = 0,
    .discrete = 0,
    .presentation = 0,
    .parse_value = parse_gradient_pos,
  },
  { .id = SHAPE_ATTR_Y2,
    .name = "y2",
    .inherited = 0,
    .discrete = 0,
    .presentation = 0,
    .parse_value = parse_gradient_pos,
  },
  { .id = SHAPE_ATTR_POINTS,
    .name = "points",
    .inherited = 0,
    .discrete = 0,
    .presentation = 0,
    .parse_value = svg_points_parse,
  },
  { .id = SHAPE_ATTR_SPREAD_METHOD,
    .name = "spreadMethod",
    .inherited = 0,
    .discrete = 1,
    .presentation = 0,
    .parse_value = svg_spread_method_parse,
  },
  { .id = SHAPE_ATTR_GRADIENT_UNITS,
    .name = "gradientUnits",
    .inherited = 0,
    .discrete = 1,
    .presentation = 0,
    .parse_value = svg_coord_units_parse,
  },
  { .id = SHAPE_ATTR_FX,
    .name = "fx",
    .inherited = 0,
    .discrete = 0,
    .parse_value = parse_length_percentage,
  },
  { .id = SHAPE_ATTR_FY,
    .name = "fy",
    .inherited = 0,
    .discrete = 0,
    .parse_value = parse_length_percentage,
  },
  { .id = SHAPE_ATTR_FR,
    .name = "fr",
    .inherited = 0,
    .discrete = 0,
    .parse_value = parse_positive_length_percentage,
    .parse_for_values = parse_length_percentage,
  },
  { .id = SHAPE_ATTR_STROKE_MINWIDTH,
    .name = "gpa:stroke-minwidth",
    .inherited = 1,
    .discrete = 0,
    .presentation = 0,
    .parse_value = parse_stroke_width,
  },
  { .id = SHAPE_ATTR_STROKE_MAXWIDTH,
    .name = "gpa:stroke-maxwidth",
    .inherited = 1,
    .discrete = 0,
    .presentation = 0,
    .parse_value = parse_stroke_width,
  },
  { .id = SHAPE_ATTR_STOP_OFFSET,
    .name = "offset",
    .inherited = 0,
    .discrete = 0,
    .presentation = 0,
    .parse_value = parse_offset,
  },
  { .id = SHAPE_ATTR_STOP_COLOR,
    .name = "stop-color",
    .inherited = 0,
    .discrete = 0,
    .presentation = 0,
    .parse_value = svg_paint_parse,
  },
  { .id = SHAPE_ATTR_STOP_OPACITY,
    .name = "stop-opacity",
    .inherited = 0,
    .discrete = 0,
    .presentation = 0,
    .parse_value = parse_opacity,
  },
};

static void
shape_attr_init_default_values (void)
{
  shape_attrs[SHAPE_ATTR_VISIBILITY].initial_value = svg_visibility_new (VISIBILITY_VISIBLE);
  shape_attrs[SHAPE_ATTR_TRANSFORM].initial_value = svg_transform_new_none ();
  shape_attrs[SHAPE_ATTR_OPACITY].initial_value = svg_number_new (1);
  shape_attrs[SHAPE_ATTR_FILTER].initial_value = svg_filter_new_none ();
  shape_attrs[SHAPE_ATTR_CLIP_PATH].initial_value = svg_clip_new_none ();
  shape_attrs[SHAPE_ATTR_CLIP_RULE].initial_value = svg_fill_rule_new (GSK_FILL_RULE_WINDING);
  shape_attrs[SHAPE_ATTR_MASK].initial_value = svg_mask_new_none ();
  shape_attrs[SHAPE_ATTR_MASK_TYPE].initial_value = svg_mask_type_new (GSK_MASK_MODE_LUMINANCE);
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
  shape_attrs[SHAPE_ATTR_HREF].initial_value = svg_href_new_none ();
  shape_attrs[SHAPE_ATTR_PATH_LENGTH].initial_value = svg_number_new (-1);
  shape_attrs[SHAPE_ATTR_PATH].initial_value = svg_path_new_none ();
  shape_attrs[SHAPE_ATTR_CX].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_CY].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_R].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_X].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_Y].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_WIDTH].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_HEIGHT].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_RX].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_RY].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_X1].initial_value = svg_percentage_new (0);
  shape_attrs[SHAPE_ATTR_Y1].initial_value = svg_percentage_new (0);
  shape_attrs[SHAPE_ATTR_X2].initial_value = svg_percentage_new (100);
  shape_attrs[SHAPE_ATTR_Y2].initial_value = svg_percentage_new (0);
  shape_attrs[SHAPE_ATTR_FX].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_FY].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_FR].initial_value = svg_percentage_new (0);
  shape_attrs[SHAPE_ATTR_POINTS].initial_value = svg_points_new_none ();
  shape_attrs[SHAPE_ATTR_SPREAD_METHOD].initial_value = svg_spread_method_new (SPREAD_METHOD_PAD);
  shape_attrs[SHAPE_ATTR_GRADIENT_UNITS].initial_value = svg_coord_units_new (COORD_UNITS_OBJECT_BOUNDING_BOX);
  shape_attrs[SHAPE_ATTR_STROKE_MINWIDTH].initial_value = svg_number_new (0.25);
  shape_attrs[SHAPE_ATTR_STROKE_MAXWIDTH].initial_value = svg_number_new (1.5);
  shape_attrs[SHAPE_ATTR_STOP_OFFSET].initial_value = svg_number_new (0);
  shape_attrs[SHAPE_ATTR_STOP_COLOR].initial_value = svg_paint_new_black ();
  shape_attrs[SHAPE_ATTR_STOP_OPACITY].initial_value = svg_number_new (1);
}

static gboolean
shape_attr_lookup (const char *name,
                   ShapeAttr  *attr,
                   ShapeType   type)
{
  if ((type == SHAPE_LINEAR_GRADIENT || type == SHAPE_RADIAL_GRADIENT) &&
      strcmp (name, "gradientTransform") == 0)
    name = "transform";

  for (unsigned int i = 0; i < G_N_ELEMENTS (shape_attrs); i++)
    {
      if (strcmp (name, shape_attrs[i].name) == 0)
        {
          *attr = i;
          return TRUE;
        }
    }
  return FALSE;
}

static const char *
shape_attr_get_presentation (ShapeAttr attr,
                             ShapeType type)
{
  if ((type == SHAPE_LINEAR_GRADIENT || type == SHAPE_RADIAL_GRADIENT) &&
      attr == SHAPE_ATTR_TRANSFORM)
    return "gradientTransform";

  return shape_attrs[attr].name;
}

static SvgValue *
shape_attr_parse_value (ShapeAttr   attr,
                        const char *value)
{
  if (shape_attrs[attr].presentation)
    {
      if (strcmp (value, "inherit") == 0)
        return svg_inherit_new ();
      else if (strcmp (value, "initial") == 0)
        return svg_initial_new ();
    }

  return shape_attrs[attr].parse_value (value);
}

static SvgValue *
shape_attr_parse_for_values (ShapeAttr   attr,
                             const char *value)
{
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

      if (*s == '\0' && strv[i] == NULL)
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

/* Animations can apply either to shape attributes or to color
 * stop attributes. Since there can be an arbitrary number of
 * color stops, we encode the attr and the color stop index in
 * an unsigned int.
 *
 * Attributes < SHAPE_ATTR_STOP_OFFSET are stored in the Shape,
 * attr values above that refer to values stored in color stops.
 */

typedef struct
{
  unsigned int idx;
  unsigned int attr;
} StopRef;

static StopRef
stop_ref (unsigned int attr)
{
  return (StopRef) { (attr - SHAPE_ATTR_STOP_OFFSET) / N_STOP_PROPS,
                     (attr - SHAPE_ATTR_STOP_OFFSET) % N_STOP_PROPS };
}

static ShapeAttr
shape_attr_ref (unsigned int attr)
{
  StopRef s;

  if (attr < SHAPE_ATTR_STOP_OFFSET)
    return attr;

  s = stop_ref (attr);
  return SHAPE_ATTR_STOP_OFFSET + s.attr;
}

/*  }}} */
/* {{{ Shapes */

struct {
  const char *name;
  unsigned int has_shapes      : 1;
  unsigned int never_rendered  : 1;
  unsigned int has_gpa_attrs   : 1;
  unsigned int has_color_stops : 1;
} shape_types[] = {
  { .name = "line",
    .has_shapes = 0,
    .never_rendered = 0,
    .has_gpa_attrs = 1,
    .has_color_stops = 0,
  },
  { .name = "polyline",
    .has_shapes = 0,
    .never_rendered = 0,
    .has_gpa_attrs = 1,
    .has_color_stops = 0,
  },
  { .name = "polygon",
    .has_shapes = 0,
    .never_rendered = 0,
    .has_gpa_attrs = 1,
    .has_color_stops = 0,
  },
  { .name = "rect",
    .has_shapes = 0,
    .never_rendered = 0,
    .has_gpa_attrs = 1,
    .has_color_stops = 0,
  },
  { .name = "circle",
    .has_shapes = 0,
    .never_rendered = 0,
    .has_gpa_attrs = 1,
    .has_color_stops = 0,
  },
  { .name = "ellipse",
    .has_shapes = 0,
    .never_rendered = 0,
    .has_gpa_attrs = 1,
    .has_color_stops = 0,
  },
  { .name = "path",
    .has_shapes = 0,
    .never_rendered = 0,
    .has_gpa_attrs = 1,
    .has_color_stops = 0,
  },
  { .name = "g",
    .has_shapes = 1,
    .never_rendered = 0,
    .has_gpa_attrs = 0,
    .has_color_stops = 0,
  },
  { .name = "clipPath",
    .has_shapes = 1,
    .never_rendered = 1,
    .has_gpa_attrs = 0,
    .has_color_stops = 0,
  },
  { .name = "mask",
    .has_shapes = 1,
    .never_rendered = 1,
    .has_gpa_attrs = 0,
    .has_color_stops = 0,
  },
  { .name = "defs",
    .has_shapes = 1,
    .never_rendered = 1,
    .has_gpa_attrs = 0,
    .has_color_stops = 0,
  },
  { .name = "use",
    .has_shapes = 0,
    .never_rendered = 0,
    .has_gpa_attrs = 0,
    .has_color_stops = 0,
  },
  { .name = "linearGradient",
    .has_shapes = 0,
    .never_rendered = 1,
    .has_gpa_attrs = 0,
    .has_color_stops = 1,
  },
  { .name = "radialGradient",
    .has_shapes = 0,
    .never_rendered = 1,
    .has_gpa_attrs = 0,
    .has_color_stops = 1,
  },
};

static gboolean
shape_type_lookup (const char *name,
                   ShapeType  *type)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (shape_types); i++)
    {
      if (strcmp (name, shape_types[i].name) == 0)
        {
          *type = i;
          return TRUE;
        }
    }
  return FALSE;
}

struct _Shape
{
  ShapeType type;
  uint64_t attrs;
  char *id;
  gboolean display;

  SvgValue *base[G_N_ELEMENTS (shape_attrs)];
  SvgValue *current[G_N_ELEMENTS (shape_attrs)];

  GPtrArray *shapes;
  GPtrArray *animations;
  GPtrArray *color_stops;

  GskPath *path;
  GskPathMeasure *measure;
  union {
    struct {
      double cx, cy, r;
    } circle;
    struct {
      double cx, cy, rx, ry;
    } ellipse;
    struct {
      double x, y, w, h, rx, ry;
    } rect;
    struct {
      double x1, y1, x2, y2;
    } line;
    struct {
      SvgValue *points;
    } polyline;
  } path_for;
};

static void
shape_free (gpointer data)
{
  Shape *shape = data;

  g_clear_pointer (&shape->id, g_free);

  for (unsigned int i = 0; i < G_N_ELEMENTS (shape_attrs); i++)
    {
      g_clear_pointer (&shape->base[i], svg_value_unref);
      g_clear_pointer (&shape->current[i], svg_value_unref);
    }

  g_clear_pointer (&shape->shapes, g_ptr_array_unref);
  g_clear_pointer (&shape->animations, g_ptr_array_unref);
  g_clear_pointer (&shape->color_stops, g_ptr_array_unref);

  g_clear_pointer (&shape->path, gsk_path_unref);
  g_clear_pointer (&shape->measure, gsk_path_measure_unref);

  if (shape->type == SHAPE_POLY_LINE || shape->type == SHAPE_POLYGON)
    g_clear_pointer (&shape->path_for.polyline.points, svg_value_unref);

  g_free (data);
}

static void animation_free (gpointer data);

static SvgValue *
shape_attr_get_initial_value (ShapeAttr attr,
                              ShapeType type)
{
  static SvgValue *default_radial_value;
  static SvgValue *default_line_value;

  if (type == SHAPE_RADIAL_GRADIENT)
    {
      /* Radial gradients have conflicting initial values. Yay */
      if (attr == SHAPE_ATTR_CX ||
          attr == SHAPE_ATTR_CY ||
          attr == SHAPE_ATTR_R)
        {
          if (!default_radial_value)
            default_radial_value = svg_percentage_new (50);

          return default_radial_value;
        }
    }

  if (type == SHAPE_LINE)
    {
      if (attr == SHAPE_ATTR_X1 ||
          attr == SHAPE_ATTR_Y1 ||
          attr == SHAPE_ATTR_X2 ||
          attr == SHAPE_ATTR_Y2)
        {
          if (!default_line_value)
            default_line_value = svg_number_new (0);

          return default_line_value;

        }
    }

  return shape_attrs[attr].initial_value;
}

static Shape *
shape_new (ShapeType type)
{
  Shape *shape = g_new0 (Shape, 1);

  memset (shape, 0, sizeof (Shape));

  shape->type = type;
  shape->attrs = 0;
  shape->display = TRUE;

  for (ShapeAttr attr = 0; attr < SHAPE_ATTR_STOP_OFFSET; attr++)
    shape->base[attr] = svg_value_ref (shape_attr_get_initial_value (attr, type));

  shape->animations = g_ptr_array_new_with_free_func (animation_free);

  if (shape_types[type].has_shapes)
    shape->shapes = g_ptr_array_new_with_free_func (shape_free);

  if (shape_types[type].has_color_stops)
    shape->color_stops = g_ptr_array_new_with_free_func (color_stop_free);

  return shape;
}

static gboolean
shape_has_attr (ShapeType type,
                ShapeAttr attr)
{
  switch ((unsigned int) attr)
    {
    case SHAPE_ATTR_HREF:
      return type == SHAPE_USE;
    case SHAPE_ATTR_CX:
    case SHAPE_ATTR_CY:
      return type == SHAPE_CIRCLE || type == SHAPE_ELLIPSE || type == SHAPE_RADIAL_GRADIENT;
    case SHAPE_ATTR_R:
      return type == SHAPE_CIRCLE || type == SHAPE_RADIAL_GRADIENT;
    case SHAPE_ATTR_X:
    case SHAPE_ATTR_Y:
    case SHAPE_ATTR_WIDTH:
    case SHAPE_ATTR_HEIGHT:
      return type == SHAPE_RECT || type == SHAPE_USE;
    case SHAPE_ATTR_RX:
    case SHAPE_ATTR_RY:
      return type == SHAPE_RECT || type == SHAPE_ELLIPSE;
    case SHAPE_ATTR_PATH_LENGTH:
      return type == SHAPE_LINE ||
             type == SHAPE_RECT || type == SHAPE_CIRCLE ||
             type == SHAPE_ELLIPSE || type == SHAPE_PATH;
    case SHAPE_ATTR_PATH:
      return type == SHAPE_PATH;
    case SHAPE_ATTR_STROKE_MINWIDTH:
    case SHAPE_ATTR_STROKE_MAXWIDTH:
      return FALSE;
    case SHAPE_ATTR_X1:
    case SHAPE_ATTR_Y1:
    case SHAPE_ATTR_X2:
    case SHAPE_ATTR_Y2:
      return type== SHAPE_LINE || type == SHAPE_LINEAR_GRADIENT || type == SHAPE_RADIAL_GRADIENT;
    case SHAPE_ATTR_POINTS:
      return type == SHAPE_POLY_LINE || type == SHAPE_POLYGON;
    case SHAPE_ATTR_SPREAD_METHOD:
    case SHAPE_ATTR_GRADIENT_UNITS:
      return type == SHAPE_LINEAR_GRADIENT || type == SHAPE_RADIAL_GRADIENT;
    case SHAPE_ATTR_STOP_OFFSET:
    case SHAPE_ATTR_STOP_COLOR:
    case SHAPE_ATTR_STOP_OPACITY:
      return FALSE;
    case SHAPE_ATTR_TRANSFORM:
      return TRUE;
    case SHAPE_ATTR_FX:
    case SHAPE_ATTR_FY:
    case SHAPE_ATTR_FR:
      return type == SHAPE_RADIAL_GRADIENT;
    default:
      return type != SHAPE_LINEAR_GRADIENT && type != SHAPE_RADIAL_GRADIENT;
    }
}

static GskPath *
shape_get_path (Shape                 *shape,
                const graphene_size_t *viewport,
                gboolean               current)
{
  GskPathBuilder *builder;
  double cx, cy, r, x, y, width, height, rx, ry;
  double x1, y1, x2, y2;
  SvgValue **values;
  SvgPoints *points;

  if (current)
    values = shape->current;
  else
    values = shape->base;

  switch (shape->type)
    {
    case SHAPE_LINE:
      x1 = svg_number_get (values[SHAPE_ATTR_X1], viewport->width);
      y1 = svg_number_get (values[SHAPE_ATTR_Y1], viewport->height);
      x2 = svg_number_get (values[SHAPE_ATTR_X2], viewport->width);
      y2 = svg_number_get (values[SHAPE_ATTR_Y2], viewport->height);
      builder = gsk_path_builder_new ();
      gsk_path_builder_move_to (builder, x1, y1);
      gsk_path_builder_line_to (builder, x2, y2);
      return gsk_path_builder_free_to_path (builder);

    case SHAPE_POLY_LINE:
    case SHAPE_POLYGON:
      builder = gsk_path_builder_new ();
      points = (SvgPoints *) values[SHAPE_ATTR_POINTS];
      if (points->n_values > 0)
        {
          gsk_path_builder_move_to (builder,
                                    points->values[0], points->values[1]);
          for (unsigned int i = 2; i < points->n_values; i += 2)
            {
              gsk_path_builder_line_to (builder,
                                        points->values[i], points->values[i + 1]);
            }
          if (shape->type == SHAPE_POLYGON)
            gsk_path_builder_close (builder);
        }
      return gsk_path_builder_free_to_path (builder);

    case SHAPE_CIRCLE:
      cx = svg_number_get (values[SHAPE_ATTR_CX], viewport->width);
      cy = svg_number_get (values[SHAPE_ATTR_CY], viewport->height);
      r = svg_number_get (values[SHAPE_ATTR_R], normalized_diagonal (viewport));
      builder = gsk_path_builder_new ();
      gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (cx, cy), r);
      return gsk_path_builder_free_to_path (builder);

    case SHAPE_ELLIPSE:
      cx = svg_number_get (values[SHAPE_ATTR_CX], viewport->width);
      cy = svg_number_get (values[SHAPE_ATTR_CY], viewport->height);
      rx = svg_number_get (values[SHAPE_ATTR_RX], viewport->width);
      ry = svg_number_get (values[SHAPE_ATTR_RY], viewport->height);
      return make_ellipse_path (cx, cy, rx, ry);

    case SHAPE_RECT:
      x = svg_number_get (values[SHAPE_ATTR_X], viewport->width);
      y = svg_number_get (values[SHAPE_ATTR_Y], viewport->height);
      width = svg_number_get (values[SHAPE_ATTR_WIDTH], viewport->width);
      height = svg_number_get (values[SHAPE_ATTR_HEIGHT],viewport->height);
      rx = svg_number_get (values[SHAPE_ATTR_RX], viewport->width);
      ry = svg_number_get (values[SHAPE_ATTR_RY], viewport->height);
      builder = gsk_path_builder_new ();
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
      return gsk_path_builder_free_to_path (builder);

    case SHAPE_PATH:
      if (svg_path_get (values[SHAPE_ATTR_PATH]))
        {
          return gsk_path_ref (svg_path_get (values[SHAPE_ATTR_PATH]));
        }
      else
        {
          builder = gsk_path_builder_new ();
          return gsk_path_builder_free_to_path (builder);
        }
      break;

    case SHAPE_GROUP:
    case SHAPE_CLIP_PATH:
    case SHAPE_MASK:
    case SHAPE_DEFS:
    case SHAPE_USE:
    case SHAPE_LINEAR_GRADIENT:
    case SHAPE_RADIAL_GRADIENT:
    default:
      g_assert_not_reached ();
    }
}

static GskPath *
shape_get_current_path (Shape                 *shape,
                        const graphene_size_t *viewport)
{
  if (shape->path)
    {
      switch (shape->type)
        {
        case SHAPE_LINE:
          if (shape->path_for.line.x1 != svg_number_get (shape->current[SHAPE_ATTR_X1], viewport->width) ||
              shape->path_for.line.y1 != svg_number_get (shape->current[SHAPE_ATTR_Y1], viewport->height) ||
              shape->path_for.line.x2 != svg_number_get (shape->current[SHAPE_ATTR_X2], viewport->height) ||
              shape->path_for.line.y2 != svg_number_get (shape->current[SHAPE_ATTR_Y2], viewport->height))
            {
              g_clear_pointer (&shape->path, gsk_path_unref);
              g_clear_pointer (&shape->measure, gsk_path_measure_unref);
            }
          break;

        case SHAPE_POLY_LINE:
        case SHAPE_POLYGON:
          if (!svg_value_equal (shape->path_for.polyline.points, shape->current[SHAPE_ATTR_POINTS]))
            {
              g_clear_pointer (&shape->path, gsk_path_unref);
              g_clear_pointer (&shape->measure, gsk_path_measure_unref);
            }
          break;

        case SHAPE_CIRCLE:
          if (shape->path_for.circle.cx != svg_number_get (shape->current[SHAPE_ATTR_CX], viewport->width) ||
              shape->path_for.circle.cy != svg_number_get (shape->current[SHAPE_ATTR_CY], viewport->height) ||
              shape->path_for.circle.r != svg_number_get (shape->current[SHAPE_ATTR_R], normalized_diagonal (viewport)))
            {
              g_clear_pointer (&shape->path, gsk_path_unref);
              g_clear_pointer (&shape->measure, gsk_path_measure_unref);
            }
          break;

        case SHAPE_ELLIPSE:
          if (shape->path_for.ellipse.cx != svg_number_get (shape->current[SHAPE_ATTR_CX], viewport->width) ||
              shape->path_for.ellipse.cy != svg_number_get (shape->current[SHAPE_ATTR_CY], viewport->height) ||
              shape->path_for.ellipse.rx != svg_number_get (shape->current[SHAPE_ATTR_RX], viewport->width) ||
              shape->path_for.ellipse.ry != svg_number_get (shape->current[SHAPE_ATTR_RY], viewport->height))
            {
              g_clear_pointer (&shape->path, gsk_path_unref);
              g_clear_pointer (&shape->measure, gsk_path_measure_unref);
            }
          break;

        case SHAPE_RECT:
          if (shape->path_for.rect.x != svg_number_get (shape->current[SHAPE_ATTR_X], viewport->width) ||
              shape->path_for.rect.y != svg_number_get (shape->current[SHAPE_ATTR_Y], viewport->height) ||
              shape->path_for.rect.w != svg_number_get (shape->current[SHAPE_ATTR_WIDTH], viewport->width) ||
              shape->path_for.rect.h != svg_number_get (shape->current[SHAPE_ATTR_HEIGHT], viewport->height) ||
              shape->path_for.rect.rx != svg_number_get (shape->current[SHAPE_ATTR_RX], viewport->width) ||
              shape->path_for.rect.ry != svg_number_get (shape->current[SHAPE_ATTR_RY], viewport->height))
            {
              g_clear_pointer (&shape->path, gsk_path_unref);
              g_clear_pointer (&shape->measure, gsk_path_measure_unref);
            }
          break;

        case SHAPE_PATH:
          if (shape->path != svg_path_get (shape->current[SHAPE_ATTR_PATH]))
            {
              g_clear_pointer (&shape->path, gsk_path_unref);
              g_clear_pointer (&shape->measure, gsk_path_measure_unref);
            }
          break;

        case SHAPE_GROUP:
        case SHAPE_CLIP_PATH:
        case SHAPE_MASK:
        case SHAPE_DEFS:
        case SHAPE_USE:
        case SHAPE_LINEAR_GRADIENT:
        case SHAPE_RADIAL_GRADIENT:
        default:
          g_assert_not_reached ();
        }
    }

  if (!shape->path)
    {
      shape->path = shape_get_path (shape, viewport, TRUE);

      switch (shape->type)
        {
        case SHAPE_LINE:
          shape->path_for.line.x1 = svg_number_get (shape->current[SHAPE_ATTR_X1], viewport->width);
          shape->path_for.line.y1 = svg_number_get (shape->current[SHAPE_ATTR_Y1], viewport->height);
          shape->path_for.line.x2 = svg_number_get (shape->current[SHAPE_ATTR_X2], viewport->width);
          shape->path_for.line.y2 = svg_number_get (shape->current[SHAPE_ATTR_Y2], viewport->height);
          break;

        case SHAPE_POLY_LINE:
        case SHAPE_POLYGON:
          g_clear_pointer (&shape->path_for.polyline.points, svg_value_unref);
          shape->path_for.polyline.points = svg_value_ref (shape->current[SHAPE_ATTR_POINTS]);
          break;

        case SHAPE_CIRCLE:
          shape->path_for.circle.cx = svg_number_get (shape->current[SHAPE_ATTR_CX], viewport->width);
          shape->path_for.circle.cy = svg_number_get (shape->current[SHAPE_ATTR_CY], viewport->height);
          shape->path_for.circle.r = svg_number_get (shape->current[SHAPE_ATTR_R], normalized_diagonal (viewport));
          break;

        case SHAPE_ELLIPSE:
          shape->path_for.ellipse.cx = svg_number_get (shape->current[SHAPE_ATTR_CX], viewport->width);
          shape->path_for.ellipse.cy = svg_number_get (shape->current[SHAPE_ATTR_CY], viewport->height);
          shape->path_for.ellipse.rx = svg_number_get (shape->current[SHAPE_ATTR_RX], viewport->width);
          shape->path_for.ellipse.ry = svg_number_get (shape->current[SHAPE_ATTR_RY], viewport->height);
          break;

        case SHAPE_RECT:
          shape->path_for.rect.x = svg_number_get (shape->current[SHAPE_ATTR_X], viewport->width);
          shape->path_for.rect.y = svg_number_get (shape->current[SHAPE_ATTR_Y], viewport->height);
          shape->path_for.rect.w = svg_number_get (shape->current[SHAPE_ATTR_WIDTH], viewport->width);
          shape->path_for.rect.h = svg_number_get (shape->current[SHAPE_ATTR_HEIGHT], viewport->height);
          shape->path_for.rect.rx = svg_number_get (shape->current[SHAPE_ATTR_RX], viewport->width);
          shape->path_for.rect.ry = svg_number_get (shape->current[SHAPE_ATTR_RY], viewport->height);
          break;

        case SHAPE_PATH:
          break;

        case SHAPE_GROUP:
        case SHAPE_CLIP_PATH:
        case SHAPE_MASK:
        case SHAPE_DEFS:
        case SHAPE_USE:
        case SHAPE_LINEAR_GRADIENT:
        case SHAPE_RADIAL_GRADIENT:
        default:
          g_assert_not_reached ();
        }
    }

  return gsk_path_ref (shape->path);
}

static GskPathMeasure *
shape_get_current_measure (Shape                 *shape,
                           const graphene_size_t *viewport)
{
  if (!shape->measure)
    {
      GskPath *path = shape_get_current_path (shape, viewport);
      shape->measure = gsk_path_measure_new (path);
      gsk_path_unref (path);
    }

  return gsk_path_measure_ref (shape->measure);
}

static unsigned int
shape_add_color_stop (Shape *shape)
{
  ColorStop *stop = g_new0 (ColorStop, 1);

  g_assert (shape_types[shape->type].has_color_stops);

  stop->base[COLOR_STOP_OFFSET] =
    svg_value_ref (shape_attr_get_initial_value (SHAPE_ATTR_STOP_OFFSET, shape->type));
  stop->base[COLOR_STOP_COLOR] =
    svg_value_ref (shape_attr_get_initial_value (SHAPE_ATTR_STOP_COLOR, shape->type));
  stop->base[COLOR_STOP_OPACITY] =
    svg_value_ref (shape_attr_get_initial_value (SHAPE_ATTR_STOP_OPACITY, shape->type));

  g_ptr_array_add (shape->color_stops, stop);

  return shape->color_stops->len - 1;
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
time_spec_parse (TimeSpec    *spec,
                 const char  *value,
                 GError     **error)
{
  const char *side_str;
  const char *offset_str;
  TimeSpecSide side;

  spec->offset = 0;

  if (strcmp (value, "indefinite") == 0)
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
      if (only_nonzero)
        g_string_append (s, " ");
      string_append_double (s, spec->offset / (double) G_TIME_SPAN_MILLISECOND);
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
      spec->states.side == TIME_SPEC_SIDE_END &&
      spec->offset < 0)
    return -spec->offset;

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
                    .sync = { .base = base, .side = side },
                    .offset = offset };
  spec.sync.ref = g_strdup (ref);
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
  Shape *shape;
  unsigned int attr;

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
    unsigned int transition_easing;
    unsigned int animation;
    unsigned int easing;
    double origin;
    double segment;
    double attach_pos;
  } gpa;
};

static void
animation_init (Animation *a)
{
  a->status = ANIMATION_STATUS_INACTIVE;

  a->begin = NULL;
  a->end = NULL;

  a->simple_duration = INDEFINITE;
  a->repeat_count = DBL_MAX;
  a->repeat_duration = INDEFINITE;

  a->fill = ANIMATION_FILL_REMOVE;
  a->restart = ANIMATION_RESTART_ALWAYS;
  a->calc_mode = CALC_MODE_LINEAR;
  a->additive = ANIMATION_ADDITIVE_REPLACE;
  a->accumulate = ANIMATION_ACCUMULATE_NONE;


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

static Animation *
animation_set_new (void)
{
  Animation *a = g_new0 (Animation, 1);
  animation_init (a);
  a->type = ANIMATION_TYPE_SET;
  a->calc_mode = CALC_MODE_DISCRETE;
  return a;
}

static Animation *
animation_animate_new (void)
{
  Animation *a = g_new0 (Animation, 1);
  animation_init (a);
  a->type = ANIMATION_TYPE_ANIMATE;
  return a;
}

static Animation *
animation_transform_new (void)
{
  Animation *a = g_new0 (Animation, 1);
  animation_init (a);
  a->type = ANIMATION_TYPE_TRANSFORM;
  return a;
}

static Animation *
animation_motion_new (void)
{
  Animation *a = g_new0 (Animation, 1);
  animation_init (a);
  a->type = ANIMATION_TYPE_MOTION;
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
                  SvgValue     **values,
                  double        *params,
                  unsigned int   n_values)
{
  double linear[] = { 0, 0, 1, 1 };

  a->n_frames = n_values;
  a->frames = g_new0 (Frame, n_values);

  for (unsigned int i = 0; i < n_values; i++)
    {
      if (a->type != ANIMATION_TYPE_MOTION)
        a->frames[i].value = svg_value_ref (values[i]);
      a->frames[i].time = times[i];
      if (i + 1 < n_values && params)
        memcpy (a->frames[i].params, &params[4 * i], sizeof (double) * 4);
      else
        memcpy (a->frames[i].params, linear, sizeof (double) * 4);
    }
}

static GskPath *
animation_motion_get_path (Animation             *a,
                           const graphene_size_t *viewport,
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
                                const graphene_size_t *viewport)
{
  return animation_motion_get_path (a, viewport, FALSE);
}

static GskPath *
animation_motion_get_current_path (Animation             *a,
                                   const graphene_size_t *viewport)
{
  return animation_motion_get_path (a, viewport, TRUE);
}

static GskPathMeasure *
animation_motion_get_current_measure (Animation             *a,
                                      const graphene_size_t *viewport)
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
      GskPathBuilder *builder = gsk_path_builder_new ();
      gsk_path_builder_move_to (builder, 0, 0);
      GskPath *path = gsk_path_builder_free_to_path (builder);
      GskPathMeasure *measure = gsk_path_measure_new (path);
      gsk_path_unref (path);
      return measure;
    }
}

/* }}} */
/* {{{ Animated attributes */

static SvgValue *
shape_get_current_value (Shape        *shape,
                         unsigned int  attr)
{
  if (attr < SHAPE_ATTR_STOP_OFFSET)
    {
      return shape->current[attr];
    }
  else
    {
      StopRef s;
      ColorStop *stop;

      g_assert (shape_types[shape->type].has_color_stops);

      s = stop_ref (attr);
      stop = g_ptr_array_index (shape->color_stops, s.idx);

      return stop->current[s.attr];
    }
}

static SvgValue *
shape_get_base_value (Shape        *shape,
                      Shape        *parent,
                      unsigned int  attr)
{
  if (attr < SHAPE_ATTR_STOP_OFFSET)
    {
      if ((shape->attrs & BIT (attr)) == 0)
        {
          if (parent && shape_attrs[attr].inherited)
            return parent->current[attr];
          else
            return shape_attr_get_initial_value (attr, shape->type);
        }
      else if (svg_value_is_inherit (shape->base[attr]))
        {
          if (parent)
            return parent->current[attr];
          else
            return shape_attr_get_initial_value (attr, shape->type);
        }
      else if (svg_value_is_initial (shape->base[attr]))
        {
          return shape_attr_get_initial_value (attr, shape->type);
        }

      return shape->base[attr];
    }
  else
    {
      StopRef s;
      ColorStop *stop;

      g_assert (shape_types[shape->type].has_color_stops);

      s = stop_ref (attr);
      stop = g_ptr_array_index (shape->color_stops, s.idx);

      return stop->base[s.attr];
    }
}

static void
shape_set_base_value (Shape        *shape,
                      unsigned int  attr,
                      SvgValue     *value)
{
  if (attr < SHAPE_ATTR_STOP_OFFSET)
    {
      g_clear_pointer (&shape->base[attr], svg_value_unref);
      shape->base[attr] = svg_value_ref (value);
      shape->attrs |= BIT (attr);
    }
  else
    {
      ColorStop *stop;

      g_assert (shape_types[shape->type].has_color_stops);
      /* set the values of the last color stop */
      attr -= SHAPE_ATTR_STOP_OFFSET;
      stop = g_ptr_array_index (shape->color_stops, shape->color_stops->len - 1);
      g_clear_pointer (&stop->base[attr], svg_value_unref);
      stop->base[attr] = svg_value_ref (value);
    }
}

static void
shape_set_current_value (Shape        *shape,
                         unsigned int  attr,
                         SvgValue     *value)
{
  if (attr < SHAPE_ATTR_STOP_OFFSET)
    {
      if (value)
        svg_value_ref (value);
      g_clear_pointer (&shape->current[attr], svg_value_unref);
      shape->current[attr] = value;
    }
  else
    {
      StopRef s;
      ColorStop *stop;

      g_assert (shape_types[shape->type].has_color_stops);
      s = stop_ref (attr);
      stop = g_ptr_array_index (shape->color_stops, s.idx);

      if (value)
        svg_value_ref (value);
      g_clear_pointer (&stop->current[s.attr], svg_value_unref);
      stop->current[s.attr] = value;
    }
}

/* }}} */
/* {{{ Update computation */

static int64_t
determine_repeat_duration (Animation *a)
{
  if (a->repeat_duration < INDEFINITE)
    return a->repeat_duration;
  else if (a->simple_duration < INDEFINITE && a->repeat_count < DBL_MAX)
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

  if (repeat_duration < INDEFINITE && a->repeat_count < DBL_MAX)
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
      else if (shape_attrs[shape_attr_ref (a->attr)].discrete)
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
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

static void
schedule_next_update (GtkSvg *self)
{
  GtkSvgRunMode run_mode = self->run_mode;

  g_clear_handle_id (&self->pending_invalidate, g_source_remove);

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
      gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
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
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

static gboolean
periodic_update (GtkSvg *self)
{
  int64_t time = g_get_monotonic_time ();
  dbg_print ("clock", "periodic update, advancing to %s\n", format_time (time));
  gtk_svg_advance (self, time);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  return G_SOURCE_CONTINUE;
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
  else if (!self->clock && !self->periodic_update_id)
    {
      self->periodic_update_id = g_timeout_add (16, (GSourceFunc) periodic_update, self);
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
  else if (!self->clock && self->periodic_update_id)
    {
      g_clear_handle_id (&self->periodic_update_id, g_source_remove);
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

typedef struct {
  GtkSvg *svg;
  const graphene_size_t *viewport;
  Shape *parent; /* Can be different from the actual parent, for <use> */
  int64_t current_time;
  const GdkRGBA *colors;
  size_t n_colors;
} ComputeContext;

static SvgValue *
resolve_value (Shape           *shape,
               ComputeContext  *context,
               ShapeAttr       attr,
               SvgValue       *value)
{
  if (svg_value_is_initial (value))
    {
      return svg_value_ref (shape_attr_get_initial_value (attr, shape->type));
    }
  else if (svg_value_is_inherit (value))
    {
      if (context->parent)
        return svg_value_ref (context->parent->current[attr]);
      else
        return svg_value_ref (shape_attr_get_initial_value (attr, shape->type));
    }
  else if (attr == SHAPE_ATTR_STROKE || attr == SHAPE_ATTR_FILL)
    {
      return svg_paint_resolve (value, context->colors, context->n_colors);
    }
  else
   {
     return svg_value_ref (value);
   }
}

static SvgValue *
compute_animation_motion_value (Animation      *a,
                                unsigned int    rep,
                                unsigned int    frame,
                                double          frame_t,
                                ComputeContext *context)
{
  double offset;
  double angle;
  graphene_point_t orig_pos, final_pos;
  SvgValue *value;
  GskPathMeasure *measure;

  measure = shape_get_current_measure (a->shape, context->viewport);
  get_transform_data_for_motion (measure, a->gpa.origin, ROTATE_FIXED, &angle, &orig_pos);
  gsk_path_measure_unref (measure);

  if (frame + 1 < a->n_frames)
    offset = lerp (frame_t, a->frames[frame].point, a->frames[frame + 1].point);
  else
    offset = a->frames[frame].point;

  measure = animation_motion_get_current_measure (a, context->viewport);
  angle = a->motion.angle;
  get_transform_data_for_motion (measure, offset, a->motion.rotate, &angle, &final_pos);
  value = svg_transform_new_rotate_and_shift (angle, &orig_pos, &final_pos);

  if (a->accumulate == ANIMATION_ACCUMULATE_SUM)
    {
      SvgValue *end_val, *acc;

      get_transform_data_for_motion (measure, 1, a->motion.rotate, &angle, &final_pos);
      end_val = svg_transform_new_rotate_and_shift (angle, &orig_pos, &final_pos);
      acc = svg_value_accumulate (value, end_val, rep);
      svg_value_unref (end_val);
      svg_value_unref (value);
      value = acc;
    }

  gsk_path_measure_unref (measure);

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
    return resolve_value (a->shape, context, a->attr, a->frames[0].value);

  find_current_cycle_and_frame (a, context->svg, context->current_time,
                                &rep, &frame, &frame_t, &frame_start, &frame_end);

  if (a->calc_mode == CALC_MODE_DISCRETE)
    return resolve_value (a->shape, context, a->attr, a->frames[frame].value);

  if (shape_attrs[shape_attr_ref (a->attr)].discrete)
    return resolve_value (a->shape, context, a->attr,
                          frame_t < 0.5 ? a->frames[frame].value
                                        : a->frames[frame + 1].value);

  /* Now we are doing non-discrete linear or spline interpolation */
  if (a->calc_mode == CALC_MODE_SPLINE)
    frame_t = ease (a->frames[frame].params, frame_t);

  if (a->attr != SHAPE_ATTR_TRANSFORM || a->type != ANIMATION_TYPE_MOTION)
    {
      SvgValue *ival;

      if (frame + 1 == a->n_frames)
        ival = resolve_value (a->shape, context, a->attr, a->frames[frame].value);
      else
        {
          SvgValue *v1 = resolve_value (a->shape, context, a->attr, a->frames[frame].value);
          SvgValue *v2 = resolve_value (a->shape, context, a->attr, a->frames[frame + 1].value);
          ival = svg_value_interpolate (v1, v2, frame_t);
          svg_value_unref (v1);
          svg_value_unref (v2);
        }
      if (ival == NULL)
        {
          gtk_svg_update_error (context->svg,
                                "Failed to interpolate %s value (animation %s)",
                                shape_attr_get_presentation (a->attr, a->shape->type),
                                a->id);
          ival = resolve_value (a->shape, context, a->attr, a->frames[frame].value);
        }

      if (a->accumulate == ANIMATION_ACCUMULATE_SUM)
        {
          SvgValue *aval;

          SvgValue *v = resolve_value (a->shape, context, a->attr, a->frames[a->n_frames - 1].value);
          aval = svg_value_accumulate (ival, v, rep);
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
  if (a->status == ANIMATION_STATUS_INACTIVE)
    {
      /* keep the base value */
      dbg_print ("values", "%s: too early\n", a->id);
      return NULL;
    }
  else if (a->status == ANIMATION_STATUS_RUNNING)
    {
      /* animation is active */
      dbg_print ("values", "%s: updating value\n", a->id);
      return compute_value_at_time (a, context);
    }
  else if (a->fill == ANIMATION_FILL_FREEZE)
    {
      /* keep the last value */
      if (a->repeat_count == 1)
        {
          if (!(a->attr == SHAPE_ATTR_TRANSFORM && a->type == ANIMATION_TYPE_MOTION))
            {
              dbg_print ("values", "%s: frozen (fast)\n", a->id);
              return resolve_value (a->shape, context, a->attr, a->frames[a->n_frames - 1].value);
            }
          else
           {
              dbg_print ("values", "%s: frozen (motion)\n", a->id);
              return compute_animation_motion_value (a, 1, a->n_frames - 1, 0, context);
           }
        }
      else
        {
          dbg_print ("values", "%s: frozen\n", a->id);
          return compute_value_at_time (a, context);
        }
    }
  else
    {
      /* Back to base value */
      dbg_print ("values", "%s: back to base\n", a->id);
      return NULL;
    }
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

  if (a1->attr < a2->attr)
    return -1;
  else if (a1->attr > a2->attr)
    return 1;

  if (a1->attr == SHAPE_ATTR_TRANSFORM)
    {
      if (a1->type == ANIMATION_TYPE_MOTION && a2->type != ANIMATION_TYPE_MOTION)
        return 1;
      else if (a1->type != ANIMATION_TYPE_MOTION && a2->type == ANIMATION_TYPE_MOTION)
        return -1;
    }

  /* sort higher priority animations later, so
   * their effect overrides lower priority ones
   */
  start1 = get_last_start (a1);
  start2 = get_last_start (a2);

  if (start1 < start2)
    return -1;
  else if (start1 > start2)
    return 1;

  return 0;
}

static void
shape_init_current_values (Shape          *shape,
                           ComputeContext *context)
{
  for (ShapeAttr attr = 0; attr < SHAPE_ATTR_STOP_OFFSET; attr++)
    {
      if (shape_has_attr (shape->type, attr) || shape_attrs[attr].inherited)
        {
          SvgValue *value;

          value = resolve_value (shape, context, attr,
                                 shape_get_base_value (shape, context->parent, attr));
          shape_set_current_value (shape, attr, value);
          svg_value_unref (value);
        }
    }

  if (shape_types[shape->type].has_color_stops)
    {
      unsigned int attr = SHAPE_ATTR_STOP_OFFSET;
      for (unsigned int idx = 0; idx < shape->color_stops->len; idx++)
        {
          for (StopProperties prop = 0; prop < N_STOP_PROPS; prop++)
            {
              SvgValue *value;

              value = resolve_value (shape, context, attr,
                                     shape_get_base_value (shape, NULL, attr));
              shape_set_current_value (shape, attr, value);
              svg_value_unref (value);
              attr++;
            }
        }
    }

}

static void
compute_current_values_for_shape (Shape          *shape,
                                  ComputeContext *context)
{
  if (!shape->display)
    return;

  shape_init_current_values (shape, context);

  g_ptr_array_sort_values (shape->animations, compare_anim);

  for (unsigned int i = 0; i < shape->animations->len; i++)
    {
      Animation *a = g_ptr_array_index (shape->animations, i);
      ShapeAttr attr = a->attr;
      SvgValue *val;

      if (a->status == ANIMATION_STATUS_INACTIVE)
        continue;

      val = compute_value_for_animation (a, context);
      if (val)
        {
          if (a->additive == ANIMATION_ADDITIVE_SUM)
            {
              SvgValue *end_val;

              end_val = svg_value_accumulate (val, shape_get_current_value (shape, attr), 1);
              shape_set_current_value (shape, attr, end_val);

              svg_value_unref (end_val);
            }
          else
            {
              shape_set_current_value (shape, attr, val);
            }

          svg_value_unref (val);
        }
    }

  if (shape_types[shape->type].has_shapes)
    {
      Shape *parent = context->parent;
      context->parent = shape;
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          Shape *sh = g_ptr_array_index (shape->shapes, i);
          compute_current_values_for_shape (sh, context);
        }
      context->parent = parent;
    }
}

/* }}} */
/* {{{ gpa things */

typedef enum
{
  TRANSITION_TYPE_NONE,
  TRANSITION_TYPE_ANIMATE,
  TRANSITION_TYPE_MORPH,
  TRANSITION_TYPE_FADE,
} GpaTransition;

typedef enum
{
  EASING_FUNCTION_LINEAR,
  EASING_FUNCTION_EASE_IN_OUT,
  EASING_FUNCTION_EASE_IN,
  EASING_FUNCTION_EASE_OUT,
  EASING_FUNCTION_EASE
} GpaEasingFunction;

typedef enum
{
  ANIMATION_DIRECTION_NONE,
  ANIMATION_DIRECTION_NORMAL,
  ANIMATION_DIRECTION_ALTERNATE,
  ANIMATION_DIRECTION_REVERSE,
  ANIMATION_DIRECTION_REVERSE_ALTERNATE,
  ANIMATION_DIRECTION_IN_OUT,
  ANIMATION_DIRECTION_IN_OUT_ALTERNATE,
  ANIMATION_DIRECTION_IN_OUT_REVERSE,
  ANIMATION_DIRECTION_SEGMENT,
  ANIMATION_DIRECTION_SEGMENT_ALTERNATE,
} GpaAnimationDirection;

static struct {
  double params[4];
} easing_funcs[] = {
  { { 0, 0, 1, 1 } },
  { { 0.42, 0, 0.58, 1 } },
  { { 0.42, 0, 1, 1 } },
  { { 0, 0, 0.58, 1 } },
  { { 0.25, 0.1, 0.25, 1 } },
};

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
                          unsigned int  initial)
{
  Animation *a = animation_set_new ();
  TimeSpec *begin, *end;

  a->attr = SHAPE_ATTR_VISIBILITY;

  a->id = g_strdup_printf ("gpa:out-of-state:%s", shape->id);
  begin = animation_add_begin (a, timeline_get_states (timeline, states, TIME_SPEC_SIDE_END, 0));
  time_spec_add_animation (begin, a);

  if (!state_match (states, initial))
    {
      begin = animation_add_begin (a, timeline_get_start_of_time (timeline));
      time_spec_add_animation (begin, a);
    }

  end = animation_add_end (a, timeline_get_states (timeline, states, TIME_SPEC_SIDE_BEGIN, 0));
  time_spec_add_animation (end, a);

  a->has_begin = 1;
  a->has_end = 1;

  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  a->frames[0].value = svg_visibility_new (VISIBILITY_HIDDEN);
  a->frames[1].value = svg_visibility_new (VISIBILITY_HIDDEN);

  a->fill = ANIMATION_FILL_REMOVE;

  a->shape = shape;
  g_ptr_array_add (shape->animations, a);
}

static void
create_states (Shape        *shape,
               Timeline     *timeline,
               uint64_t      states,
               unsigned int  initial)
{
  create_visibility_setter (shape, timeline, states, initial);
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

  a->shape = shape;
  g_ptr_array_add (shape->animations, a);
}

static void
create_transition (Shape             *shape,
                   Timeline          *timeline,
                   uint64_t           states,
                   int64_t            duration,
                   int64_t            delay,
                   GpaEasingFunction  easing,
                   double             origin,
                   GpaTransition      type,
                   ShapeAttr          attr,
                   SvgValue          *from,
                   SvgValue          *to)
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

  a->id = g_strdup_printf ("gpa:transition:fade-in:%s:%s", shape_attrs[attr].name, shape->id);

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

  a->shape = shape;
  g_ptr_array_add (shape->animations, a);

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

  a->id = g_strdup_printf ("gpa:transition:fade-out:%s:%s", shape_attrs[attr].name, shape->id);

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

  a->shape = shape;
  g_ptr_array_add (shape->animations, a);

  time_spec_add_animation (begin, a);

  a->gpa.transition = type;
  a->gpa.easing = easing;
  a->gpa.origin = origin;
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

  a->id = g_strdup_printf ("gpa:transition:fade-in-delay:%s:%s", shape_attrs[attr].name, shape->id);

  begin = animation_add_begin (a, timeline_get_states (timeline, states, TIME_SPEC_SIDE_BEGIN, 0));

  a->attr = attr;
  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  a->frames[0].value = svg_value_ref (value);
  a->frames[1].value = svg_value_ref (value);

  a->fill = ANIMATION_FILL_REMOVE;

  a->shape = shape;
  g_ptr_array_add (shape->animations, a);
  time_spec_add_animation (begin, a);

  a = animation_set_new ();
  a->simple_duration = delay;
  a->repeat_duration = delay;
  a->repeat_count = 1;

  a->has_begin = 1;
  a->has_simple_duration = 1;
  a->has_repeat_duration = 1;

  a->id = g_strdup_printf ("gpa:transition:fade-out-delay:%s:%s", shape_attrs[attr].name, shape->id);

  begin = animation_add_begin (a, timeline_get_states (timeline, states, TIME_SPEC_SIDE_END, -delay));

  a->attr = attr;
  a->n_frames = 2;
  a->frames = g_new0 (Frame, a->n_frames);
  a->frames[0].time = 0;
  a->frames[1].time = 1;
  a->frames[0].value = svg_value_ref (value);
  a->frames[1].value = svg_value_ref (value);

  a->fill = ANIMATION_FILL_REMOVE;

  a->shape = shape;
  g_ptr_array_add (shape->animations, a);
  time_spec_add_animation (begin, a);

  svg_value_unref (value);
}

static void
create_transitions (Shape             *shape,
                    Timeline          *timeline,
                    uint64_t           states,
                    GpaTransition      type,
                    int64_t            duration,
                    int64_t            delay,
                    GpaEasingFunction  easing,
                    double             origin)
{
  switch (type)
    {
    case TRANSITION_TYPE_NONE:
      break;
    case TRANSITION_TYPE_ANIMATE:
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
    case TRANSITION_TYPE_MORPH:
      create_transition (shape, timeline, states,
                         duration, delay, easing,
                         origin, type,
                         SHAPE_ATTR_FILTER,
                         svg_filter_parse ("blur(32) alpha-level(0.2)"),
                         svg_filter_parse ("blur(0) alpha-level(0.2)"));
      break;
    case TRANSITION_TYPE_FADE:
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

  a->has_begin = 1;
  a->has_end = 1;
  a->has_simple_duration = 1;

  a->id = g_strdup_printf ("gpa:animation:%s-%s", shape->id, shape_attrs[attr].name);

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

  a->shape = shape;
  g_ptr_array_add (shape->animations, a);

  return a;
}

static void
add_frame (GArray            *a,
           double             time,
           SvgValue          *value,
           GpaEasingFunction  easing)
{
  Frame frame;
  frame.time = time;
  frame.value = value;
  frame.point = 0;
  memcpy (frame.params, easing_funcs[easing].params, 4 * sizeof (double));
  g_array_append_val (a, frame);
}

static void
add_point_frame (GArray            *a,
                 double             time,
                 double             point,
                 GpaEasingFunction  easing)
{
  Frame frame;
  frame.time = time;
  frame.value = NULL;
  frame.point = point;
  memcpy (frame.params, easing_funcs[easing].params, 4 * sizeof (double));
  g_array_append_val (a, frame);
}

static void
construct_animation_frames (GpaAnimationDirection  direction,
                            GpaEasingFunction      easing,
                            double                 segment,
                            double                 origin,
                            GArray                *array,
                            GArray                *offset)
{
  switch (direction)
    {
    case ANIMATION_DIRECTION_NORMAL:
      add_frame (array, 0, svg_dash_array_new ((double[]) { 0, 2 }, 2), easing);
      add_frame (array, 1, svg_dash_array_new ((double[]) { 1, 0 }, 2), easing);
      if (origin != 0)
        {
          add_frame (offset, 0, svg_number_new (-origin), easing);
          add_frame (offset, 1, svg_number_new (0), easing);
        }
      break;

    case ANIMATION_DIRECTION_REVERSE:
      add_frame (array, 0, svg_dash_array_new ((double[]) { 1, 0 }, 2), easing);
      add_frame (array, 1, svg_dash_array_new ((double[]) { 0, 2 }, 2), easing);
      if (origin != 0)
        {
          add_frame (offset, 0, svg_number_new (0), easing);
          add_frame (offset, 1, svg_number_new (-origin), easing);
        }
      break;

    case ANIMATION_DIRECTION_ALTERNATE:
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

    case ANIMATION_DIRECTION_REVERSE_ALTERNATE:
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

    case ANIMATION_DIRECTION_IN_OUT:
      add_frame (array, 0,   svg_dash_array_new ((double[]) { 0, 0, 0, 2 }, 4), easing);
      add_frame (array, 0.5, svg_dash_array_new ((double[]) { origin, 0, 1 - origin, 2 }, 4), easing);
      add_frame (array, 1,   svg_dash_array_new ((double[]) { 0, 1, 0, 2 }, 4), easing);
      add_frame (offset, 0,   svg_number_new (-origin), easing);
      add_frame (offset, 0.5, svg_number_new (0), easing);
      add_frame (offset, 1,   svg_number_new (0), easing);
      break;

    case ANIMATION_DIRECTION_IN_OUT_REVERSE:
      add_frame (array, 0  , svg_dash_array_new ((double[]) { origin, 0, 1 - origin, 2 }, 4), easing);
      add_frame (array, 0.5, svg_dash_array_new ((double[]) { 0, 0, 0, 2 }, 4), easing);
      add_frame (array, 1,   svg_dash_array_new ((double[]) { origin, 0, 1 - origin, 2 }, 4), easing);
      add_frame (offset, 0,   svg_number_new (0), easing);
      add_frame (offset, 0.5, svg_number_new (-origin), easing);
      add_frame (offset, 1,   svg_number_new (0), easing);
      break;

    case ANIMATION_DIRECTION_IN_OUT_ALTERNATE:
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

    case ANIMATION_DIRECTION_SEGMENT:
      add_frame (array, 0, svg_dash_array_new ((double[]) { segment, 1 - segment }, 2), easing);
      add_frame (array, 1, svg_dash_array_new ((double[]) { segment, 1 - segment }, 2), easing);
      add_frame (offset, 0, svg_number_new (0), easing);
      add_frame (offset, 1, svg_number_new (-1), easing);
      break;

    case ANIMATION_DIRECTION_SEGMENT_ALTERNATE:
      add_frame (array, 0,   svg_dash_array_new ((double[]) { segment, 2 }, 2), easing);
      add_frame (array, 0.5, svg_dash_array_new ((double[]) { segment, 2 }, 2), easing);
      add_frame (array, 1,   svg_dash_array_new ((double[]) { segment, 2 }, 2), easing);
      add_frame (offset, 0,   svg_number_new (0), easing);
      add_frame (offset, 0.5, svg_number_new (segment - 1), easing);
      add_frame (offset, 1,   svg_number_new (0), easing);
      break;

    case ANIMATION_DIRECTION_NONE:
    default:
      g_assert_not_reached ();
    }
}

static void
construct_moving_frames (GpaAnimationDirection  direction,
                         GpaEasingFunction      easing,
                         double                 segment,
                         double                 origin,
                         double                 attach_pos,
                         GArray                *array)
{
  switch ((int)direction)
    {
    case ANIMATION_DIRECTION_NORMAL:
      add_point_frame (array, 0, origin, easing);
      add_point_frame (array, 1, attach_pos, easing);
      break;

    case ANIMATION_DIRECTION_ALTERNATE:
      add_point_frame (array, 0, origin, easing);
      add_point_frame (array, 0.5, attach_pos, easing);
      add_point_frame (array, 1, origin, easing);
      break;

    case ANIMATION_DIRECTION_REVERSE:
      add_point_frame (array, 0, attach_pos, easing);
      add_point_frame (array, 1, origin, easing);
      break;

    case ANIMATION_DIRECTION_REVERSE_ALTERNATE:
      add_point_frame (array, 0,   attach_pos, easing);
      add_point_frame (array, 0.5, origin, easing);
      add_point_frame (array, 1,   attach_pos, easing);
      break;

    case ANIMATION_DIRECTION_IN_OUT:
      add_point_frame (array, 0,   origin, easing);
      add_point_frame (array, 0.5, attach_pos, easing);
      add_point_frame (array, 1,   1, easing);
      break;

    case ANIMATION_DIRECTION_IN_OUT_REVERSE:
      add_point_frame (array, 0,   1, easing);
      add_point_frame (array, 0.5, attach_pos, easing);
      add_point_frame (array, 1,   origin, easing);
      break;

    case ANIMATION_DIRECTION_IN_OUT_ALTERNATE:
      add_point_frame (array, 0,    origin, easing);
      add_point_frame (array, 0.25, attach_pos, easing);
      add_point_frame (array, 0.5,  1, easing);
      add_point_frame (array, 0.75, attach_pos, easing);
      add_point_frame (array, 1,    origin, easing);
      break;

    case ANIMATION_DIRECTION_SEGMENT:
      add_point_frame (array, 0, attach_pos * segment, easing);
      add_point_frame (array, 1, 1 + attach_pos * segment, easing);
      break;

    case ANIMATION_DIRECTION_SEGMENT_ALTERNATE:
      add_point_frame (array, 0, attach_pos * segment, easing);
      add_point_frame (array, 0.5, (1 - segment) + attach_pos * segment, easing);
      add_point_frame (array, 1, attach_pos * segment, easing);
      break;

    default:
      g_assert_not_reached ();
    }
}

static double
repeat_duration_for_direction (GpaAnimationDirection direction,
                               double                duration)
{
  switch (direction)
    {
    case ANIMATION_DIRECTION_NONE:              return 0;
    case ANIMATION_DIRECTION_NORMAL:            return duration;
    case ANIMATION_DIRECTION_ALTERNATE:         return 2 * duration;
    case ANIMATION_DIRECTION_REVERSE:           return duration;
    case ANIMATION_DIRECTION_REVERSE_ALTERNATE: return 2 * duration;
    case ANIMATION_DIRECTION_IN_OUT:            return 2 * duration;
    case ANIMATION_DIRECTION_IN_OUT_ALTERNATE:  return 4 * duration;
    case ANIMATION_DIRECTION_IN_OUT_REVERSE:    return 2 * duration;
    case ANIMATION_DIRECTION_SEGMENT:           return duration;
    case ANIMATION_DIRECTION_SEGMENT_ALTERNATE: return 2 * duration;
    default: g_assert_not_reached ();
    }
}

static void
create_animations (Shape                 *shape,
                   Timeline              *timeline,
                   uint64_t               states,
                   unsigned int           initial,
                   double                 repeat,
                   int64_t                duration,
                   GpaAnimationDirection  direction,
                   GpaEasingFunction      easing,
                   double                 segment,
                   double                 origin)
{
  GArray *array, *offset;
  CalcMode calc_mode;
  Animation *a;
  double repeat_duration;

  if (direction == ANIMATION_DIRECTION_NONE)
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

  if (easing == EASING_FUNCTION_LINEAR)
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
  add_point_frame (frames, 0, attach_pos, EASING_FUNCTION_LINEAR);
  add_point_frame (frames, 1, attach_pos, EASING_FUNCTION_LINEAR);

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
  g_ptr_array_add (shape->animations, a);
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
  GpaAnimationDirection direction;
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
      direction = ANIMATION_DIRECTION_NORMAL;
    }
  else if (g_str_has_prefix (da->id, "gpa:transition:fade-out:"))
    {
      a2->id = g_strdup_printf ("gpa:attachment-transition:fade-out:%s", a->shape->id);
      direction = ANIMATION_DIRECTION_REVERSE;
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

  a2->shape = a->shape;
  g_ptr_array_add (a2->shape->animations, a2);
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
  } skip;
} ParserData;

/* {{{ Animation attribute */

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
  const char *begin_attr = NULL;
  const char *end_attr = NULL;
  const char *dur_attr = NULL;
  const char *repeat_count_attr = NULL;
  const char *repeat_dur_attr = NULL;
  const char *fill_attr = NULL;
  const char *restart_attr = NULL;
  const char *attr_name_attr = NULL;
  const char *ignored = NULL;

  markup_filter_attributes (element_name,
                            attr_names, attr_values,
                            handled,
                            "id", &id_attr,
                            "href", &href_attr,
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

  if (href_attr && href_attr[0] == '#')
    a->href = g_strdup (href_attr + 1);
  else
    a->href = g_strdup (href_attr);

  if (begin_attr)
    {
      GStrv strv;

      strv = g_strsplit (begin_attr, ";", 0);
      for (unsigned int i = 0; strv[i]; i++)
        {
          TimeSpec spec = { 0, };
          TimeSpec *begin;
          GError *error = NULL;

          if (!time_spec_parse (&spec, strv[i], &error))
            {
              gtk_svg_invalid_attribute (data->svg, context, "begin", "%s", error->message);
              g_clear_error (&error);
              continue;
            }

          a->has_begin = 1;
          begin = animation_add_begin (a, timeline_get_time_spec (data->svg->timeline, &spec));
          time_spec_add_animation (begin, a);
          time_spec_clear (&spec);
          if (begin->type == TIME_SPEC_TYPE_STATES)
            data->svg->max_state = MAX (data->svg->max_state, g_bit_nth_msf (begin->states.states, -1));
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

          if (!time_spec_parse (&spec, strv[i], &error))
            {
              gtk_svg_invalid_attribute (data->svg, context, "end", "%s", error->message);
              g_clear_error (&error);
              continue;
            }
          a->has_end = 1;
          end = animation_add_end (a, timeline_get_time_spec (data->svg->timeline, &spec));
          time_spec_add_animation (end, a);
          time_spec_clear (&spec);
          if (end->type == TIME_SPEC_TYPE_STATES)
            data->svg->max_state = MAX (data->svg->max_state, g_bit_nth_msf (end->states.states, -1));
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
      if (strcmp (dur_attr, "indefinite") == 0)
        a->simple_duration = INDEFINITE;
      else if (!parse_duration (dur_attr, &a->simple_duration))
        {
          gtk_svg_invalid_attribute (data->svg, context, "dur", NULL);
          a->has_simple_duration = 0;
        }
    }

  a->repeat_count = DBL_MAX;
  if (repeat_count_attr)
    {
      a->has_repeat_count = 1;
      if (strcmp (repeat_count_attr, "indefinite") == 0)
        a->repeat_count = DBL_MAX;
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
      if (strcmp (repeat_dur_attr, "indefinite") == 0)
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
      if (a->repeat_count == DBL_MAX)
        a->repeat_duration = INDEFINITE;
      else
        a->repeat_duration = a->simple_duration * a->repeat_count;
    }
  else if (a->has_repeat_duration && a->has_simple_duration && !a->has_repeat_count)
    {
      if (a->repeat_duration == INDEFINITE)
        a->repeat_count = DBL_MAX;
      else
        a->repeat_count = a->repeat_duration / a->simple_duration;
    }
  else if (a->has_repeat_duration && a->has_repeat_count && !a->has_simple_duration)
    {
      if (a->repeat_duration == INDEFINITE || a->repeat_count == DBL_MAX)
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

  if (a->type == ANIMATION_TYPE_MOTION)
    {
      if (attr_name_attr)
        gtk_svg_invalid_attribute (data->svg, context, "attributeName",
                                   "can't have 'attributeName' on <animateMotion>");
    }
  else if (a->type == ANIMATION_TYPE_TRANSFORM)
    {
      const char *expected;

      expected = shape_attr_get_presentation (SHAPE_ATTR_TRANSFORM, data->current_shape->type);
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
  else if (!shape_attr_lookup (attr_name_attr, &a->attr, data->current_shape->type))
    {
      gtk_svg_missing_attribute (data->svg, context, "attributeName", "can't animate '%s'", attr_name_attr);
      return FALSE;
    }

  return TRUE;
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
  const char *key_times_attr = NULL;
  const char *splines_attr = NULL;
  const char *additive_attr = NULL;
  const char *accumulate_attr = NULL;
  TransformType transform_type = TRANSFORM_NONE;
  GArray *times;
  GArray *params;
  GPtrArray *values = NULL;
  unsigned int n_values;
  unsigned int n_times;

  markup_filter_attributes (element_name,
                            attr_names, attr_values,
                            handled,
                            "type", &type_attr,
                            "calcMode", &calc_mode_attr,
                            "values", &values_attr,
                            "from", &from_attr,
                            "to", &to_attr,
                            "keyTimes", &key_times_attr,
                            "keySplines", &splines_attr,
                            "additive", &additive_attr,
                            "accumulate", &accumulate_attr,
                            NULL);

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
                       (const char *[]) { "discrete", "linear", "spline" }, 3,
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

  values = NULL;
  n_values = 0;
  if (values_attr)
    {
      values = shape_attr_parse_values (a->attr, transform_type, values_attr);
      if (!values || values->len < 2)
        {
          gtk_svg_invalid_attribute (data->svg, context, "values", "failed to parse %s", values_attr);
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }
      n_values = values->len;
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
      n_values = 2;
    }

  times = NULL;
  n_times = 0;
  if (key_times_attr)
    {
      times = parse_numbers2 (key_times_attr, ";", 0, 1);
      if (!times)
        {
          gtk_svg_invalid_attribute (data->svg, context, "keyTimes", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }
      n_times = times->len;
    }

  if (a->type == ANIMATION_TYPE_MOTION)
    {
      if (n_times == 0)
        {
          gtk_svg_missing_attribute (data->svg, context, "keyTimes", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&times, g_array_unref);
          return FALSE;
        }

      if (n_values > 0)
        {
          gtk_svg_missing_attribute (data->svg, context, "values", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&times, g_array_unref);
          return FALSE;
        }

      n_values = times->len;
    }

  if (n_times == 0 && n_values == 0)
    {
      gtk_svg_invalid_attribute (data->svg, context, NULL,
                                 "Either values or from and to must be given");
      g_clear_pointer (&values, g_ptr_array_unref);
      g_clear_pointer (&times, g_array_unref);
      return FALSE;
    }

  if (n_times == 0)
    {
      double n;

      if (a->calc_mode == CALC_MODE_DISCRETE)
        n = values->len;
      else
        n = values->len - 1;

      n_times = values->len;

      times = g_array_new (FALSE, FALSE, sizeof (double));

      for (unsigned int i = 0; i < n_times; i++)
        {
          double d = i / (double) n;
          g_array_append_val (times, d);
        }
    }

  if (n_times != n_values)
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

  if (a->calc_mode != CALC_MODE_DISCRETE && g_array_index (times, double, n_times - 1) != 1)
    {
      gtk_svg_invalid_attribute (data->svg, context, "keyTimes",
                                 "The last keyTimes value must be 1");
      g_clear_pointer (&values, g_ptr_array_unref);
      g_clear_pointer (&times, g_array_unref);
      return FALSE;
    }

  for (unsigned int i = 1; i < n_times; i++)
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

          if (!parse_numbers (s, " ", 0, 1, spline, 4, &m) ||
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

      if (n != n_values - 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, "keySplines", "wrong number of values");
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&times, g_array_unref);
          g_clear_pointer (&params, g_array_unref);
          return FALSE;
        }
    }

  fill_from_values (a,
                    (double *) times->data,
                    values ? (SvgValue **) values->pdata : NULL,
                    params ? (double *) params->data : NULL, n_values);

  g_clear_pointer (&values, g_ptr_array_unref);
  g_clear_pointer (&times, g_array_unref);
  g_clear_pointer (&params, g_array_unref);

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
skip_whitespace (const char **p)
{
  while (g_ascii_isspace (**p))
    (*p)++;
}

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

static void
parse_style_attr (Shape               *shape,
                  const char          *style_attr,
                  ParserData          *data,
                  GMarkupParseContext *context)
{
  const char *p = style_attr;
  ShapeAttr attr;
  char *name;
  char *prop_val;
  SvgValue *value;

  while (*p)
    {
      skip_whitespace (&p);
      name = consume_ident (&p);
      if (!name)
        {
          gtk_svg_invalid_attribute (data->svg, context,
                                     "style", "while parsing 'style': expected identifier");
          skip_past_semicolon (&p);
          continue;
        }

      if (!shape_attr_lookup (name, &attr, shape->type))
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
          shape_set_base_value (shape, attr, value);
          svg_value_unref (value);
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

  for (unsigned int i = 0; attr_names[i]; i++)
    {
      ShapeAttr attr;

      if (*handled & BIT (i))
        continue;

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
      else if (strcmp (attr_names[i], "display") == 0)
        {
          shape->display = strcmp (attr_values[i], "none") != 0;
          *handled |= BIT (i);
        }
      else if (shape_attr_lookup (attr_names[i], &attr, shape->type))
        {
          if (!shape_has_attr (shape->type, attr))
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names[i], NULL);
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
                  shape_set_base_value (shape, attr, value);
                  svg_value_unref (value);
                }
            }
          *handled |= BIT (i);
        }
    }

  if (style_attr)
    parse_style_attr (shape, style_attr, data, context);

  if (class_attr)
    {
      GStrv classes = g_strsplit (class_attr, " ", 0);
      SvgValue *value;

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

      shape_set_base_value (shape, SHAPE_ATTR_FILL, value);
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

      shape_set_base_value (shape, SHAPE_ATTR_STROKE, value);
      svg_value_unref (value);

      g_strfreev (classes);
    }

  if (shape->attrs & BIT (SHAPE_ATTR_STROKE_WIDTH))
    {
      if (!(shape->attrs & BIT (SHAPE_ATTR_STROKE_MINWIDTH)))
        {
          SvgValue *v;
          v = svg_number_new (0.25 * svg_number_get (shape->base[SHAPE_ATTR_STROKE_WIDTH], 1));
          shape_set_base_value (shape, SHAPE_ATTR_STROKE_MINWIDTH, v);
          svg_value_unref (v);
        }
      if (!(shape->attrs & BIT (SHAPE_ATTR_STROKE_MAXWIDTH)))
        {
          SvgValue *v;
          v = svg_number_new (1.5 * svg_number_get (shape->base[SHAPE_ATTR_STROKE_WIDTH], 1));
          shape_set_base_value (shape, SHAPE_ATTR_STROKE_MAXWIDTH, v);
          svg_value_unref (v);
        }
    }

  if (shape->attrs & (BIT (SHAPE_ATTR_CLIP_PATH) | BIT (SHAPE_ATTR_MASK) | BIT (SHAPE_ATTR_HREF)))
    g_ptr_array_add (data->pending_refs, shape);

  if (shape->attrs & BIT (SHAPE_ATTR_FILL))
    {
      SvgPaint *paint = (SvgPaint *) shape->base[SHAPE_ATTR_FILL];
      if (paint->kind == PAINT_GRADIENT)
        g_ptr_array_add (data->pending_refs, shape);
    }

  if (shape->attrs & BIT (SHAPE_ATTR_STROKE))
    {
      SvgPaint *paint = (SvgPaint *) shape->base[SHAPE_ATTR_STROKE];
      if (paint->kind == PAINT_GRADIENT)
        g_ptr_array_add (data->pending_refs, shape);
    }

  if (shape_has_attr (shape->type, SHAPE_ATTR_RX) &&
      shape_has_attr (shape->type, SHAPE_ATTR_RY))
    {
      if ((shape->attrs & (BIT (SHAPE_ATTR_RX) | BIT (SHAPE_ATTR_RY))) == BIT (SHAPE_ATTR_RX))
        shape_set_base_value (shape, SHAPE_ATTR_RY, shape->base[SHAPE_ATTR_RX]);
      else if ((shape->attrs & (BIT (SHAPE_ATTR_RX) | BIT (SHAPE_ATTR_RY))) == BIT (SHAPE_ATTR_RY))
        shape_set_base_value (shape, SHAPE_ATTR_RX, shape->base[SHAPE_ATTR_RY]);
    }
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

  if (!shape_types[shape->type].has_gpa_attrs)
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
          shape_set_base_value (shape, SHAPE_ATTR_STROKE, value);
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
          shape_set_base_value (shape, SHAPE_ATTR_FILL, value);
          svg_value_unref (value);
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, "gpa:fill", NULL);
        }
    }

  if (strokewidth_attr)
    {
      double v[3];
      unsigned int len;

      if (parse_numbers (strokewidth_attr, " ", 0, DBL_MAX, v, 3, &len) &&
          len == 3)
        {
          value = svg_number_new (v[0]);
          shape_set_base_value (shape, SHAPE_ATTR_STROKE_MINWIDTH, value);
          svg_value_unref (value);
          value = svg_number_new (v[1]);
          shape_set_base_value (shape, SHAPE_ATTR_STROKE_WIDTH, value);
          svg_value_unref (value);
          value = svg_number_new (v[2]);
          shape_set_base_value (shape, SHAPE_ATTR_STROKE_MAXWIDTH, value);
          svg_value_unref (value);
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, "gpa:stroke-width", NULL);
        }
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
          data->svg->max_state = MAX (data->svg->max_state, g_bit_nth_msf (states, -1));
        }
    }

  origin = 0;
  if (origin_attr)
    {
      if (!parse_number (origin_attr, 0, 1, &origin))
        gtk_svg_invalid_attribute (data->svg, context, "gpa:origin", NULL);
    }

  transition_type = TRANSITION_TYPE_NONE;
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

  transition_easing = EASING_FUNCTION_LINEAR;
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

  animation_direction = ANIMATION_DIRECTION_NONE;
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

  animation_repeat = INFINITY;
  if (animation_repeat_attr)
    {
      if (strcmp (animation_repeat_attr, "indefinite") == 0)
        animation_repeat = INFINITY;
      else if (!parse_number (animation_repeat_attr, 0, DBL_MAX, &animation_repeat))
        gtk_svg_invalid_attribute (data->svg, context, "gpa:animation-repeat", NULL);
    }

  animation_segment = 0.2;
  if (animation_segment_attr)
    {
      if (!parse_number (animation_segment_attr, 0, 1, &animation_segment))
        gtk_svg_invalid_attribute (data->svg, context, "gpa:animation-segment", NULL);
    }

  animation_easing = EASING_FUNCTION_LINEAR;
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

  /* our dasharray-based animations require unit path length */
  if (shape->attrs & BIT (SHAPE_ATTR_PATH_LENGTH))
    gtk_svg_invalid_attribute (data->svg, context, NULL, "Can't set pathLength and use gpa features");

  create_states (shape,
                 data->svg->timeline,
                 states,
                 data->svg->state);

  if (attach_to_attr ||
      transition_type == TRANSITION_TYPE_ANIMATE ||
      animation_direction != ANIMATION_DIRECTION_NONE)
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
  Shape *shape;
  uint64_t handled = 0;

  if (data->skip.to)
    return;

  if (strcmp (element_name, "svg") == 0)
    {
      const char *width_attr = NULL;
      const char *height_attr = NULL;
      const char *viewbox_attr = NULL;
      const char *state_attr = NULL;
      const char *version_attr = NULL;
      const char *preserve_aspect_ratio_attr = NULL;
      double width, height;

      if (data->current_shape != data->svg->content)
        {
          skip_element (data, context, "Nested <svg> elements are not supported");
          return;
        }

      markup_filter_attributes (element_name,
                                attr_names, attr_values,
                                &handled,
                                "width", &width_attr,
                                "height", &height_attr,
                                "viewBox", &viewbox_attr,
                                "preserveAspectRatio", &preserve_aspect_ratio_attr,
                                "gpa:state", &state_attr,
                                "gpa:version", &version_attr,
                                NULL);

      if (viewbox_attr)
        {
          GStrv strv;
          unsigned int n;
          double x, y, w, h;

          strv = g_strsplit (viewbox_attr, " ", 0);
          n = g_strv_length (strv);
          if (n != 4 ||
              !parse_length (strv[0], -DBL_MAX, DBL_MAX, &x) ||
              !parse_length (strv[1], -DBL_MAX, DBL_MAX, &y) ||
              !parse_length (strv[2], 0, DBL_MAX, &w) ||
              !parse_length (strv[3], 0, DBL_MAX, &h))
            gtk_svg_invalid_attribute (data->svg, context, "viewBox", NULL);
          else
            graphene_rect_init (&data->svg->view_box, x, y, w, h);
          g_strfreev (strv);
        }

      width = data->svg->view_box.size.width;
      if (width_attr)
        {
          if (!parse_length (width_attr, 0, DBL_MAX, &width))
            gtk_svg_invalid_attribute (data->svg, context, "width", NULL);
        }

      height = data->svg->view_box.size.height;
      if (height_attr)
        {
          if (!parse_length (height_attr, 0, DBL_MAX, &height))
            gtk_svg_invalid_attribute (data->svg, context, "height", NULL);
        }

      data->svg->width = width;
      data->svg->height = height;

      if (preserve_aspect_ratio_attr)
        {
          GStrv strv;
          unsigned int align;
          unsigned int meet;

          strv = g_strsplit (preserve_aspect_ratio_attr, " ", 0);
          if (g_strv_length (strv) == 1)
            {
              if (!parse_align (strv[0], &align))
                {
                  gtk_svg_invalid_attribute (data->svg, context, "preserveAspectRatio", NULL);
                }
              else
                {
                  data->svg->align = align;
                  data->svg->meet_or_slice = MEET;
                }
            }
          else if (g_strv_length (strv) == 2)
            {

              if (parse_align (strv[0], &align) &&
                  parse_meet (strv[1], &meet))
                {
                  data->svg->align = align;
                  data->svg->meet_or_slice = meet;
                }
              else
                gtk_svg_invalid_attribute (data->svg, context, "preserveAspectRatio", NULL);
            }
          else
            gtk_svg_invalid_attribute (data->svg, context, "preserveAspectRatio", NULL);
        }

      if (state_attr)
        {
          double v;

          if (strcmp (state_attr, "empty") == 0)
            gtk_svg_set_state (data->svg, GTK_SVG_STATE_EMPTY);
          else if (!parse_number (state_attr, -1, 63, &v))
            gtk_svg_invalid_attribute (data->svg, context, "gpa:state", NULL);
          else if (v < 0)
            gtk_svg_set_state (data->svg, GTK_SVG_STATE_EMPTY);
          else
            gtk_svg_set_state (data->svg, (unsigned int) CLAMP (v, 0, 63));
        }

      if (version_attr)
        {
          unsigned int version;
          char *end;

          version = (unsigned int) g_ascii_strtoull (version_attr, &end, 10);
          if ((end && *end != '\0') || version != 1)
            gtk_svg_invalid_attribute (data->svg, context, "gpa:version", "must be 1");
          else
            data->svg->gpa_version = version;
        }
      return;
    }
  else if (strcmp (element_name, "style") == 0 ||
           strcmp (element_name, "title") == 0 ||
           strcmp (element_name, "desc") == 0 ||
           strcmp (element_name, "metadata") == 0 ||
           g_str_has_prefix (element_name, "sodipodi:") ||
           g_str_has_prefix (element_name, "inkscape:"))
    {
      skip_element (data, context, "Ignoring metadata and style elements: <%s>", element_name);
      return;
    }
  else if (strcmp (element_name, "filter") == 0 ||
           strcmp (element_name, "pattern") == 0)
    {
      skip_element (data, context, "Ignoring unsupported element: <%s>", element_name);
      return;
    }
  else if (strcmp (element_name, "set") == 0)
    {
      Animation *a = animation_set_new ();
      const char *to_attr = NULL;
      SvgValue *value;

      if (data->current_animation)
        {
          skip_element (data, context, "Nested animation elements are not allowed: <set>");
          return;
        }

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
        {
          a->shape = data->current_shape;
          g_ptr_array_add (data->current_shape->animations, a);
        }
      else
        {
          g_ptr_array_add (data->pending_animations, a);
        }

      if (a->id)
        g_hash_table_insert (data->animations, a->id, a);

      data->current_animation = a;

      return;
    }
  else if (strcmp (element_name, "animate") == 0 ||
           strcmp (element_name, "animateTransform") == 0)
    {
      Animation *a;

      if (data->current_animation)
        {
          skip_element (data, context, "Nested animation elements are not allowed: <%s>", element_name);
          return;
        }

      if (strcmp (element_name, "animate") == 0)
        a = animation_animate_new ();
      else
        a = animation_transform_new ();

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

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (!a->href || g_strcmp0 (a->href, data->current_shape->id) == 0)
        {
          a->shape = data->current_shape;
          g_ptr_array_add (data->current_shape->animations, a);
        }
      else
        {
          g_ptr_array_add (data->pending_animations, a);
        }

      if (a->id)
        g_hash_table_insert (data->animations, a->id, a);

      data->current_animation = a;

      return;
    }
  else if (strcmp (element_name, "animateMotion") == 0)
    {
      Animation *a = animation_motion_new ();
      const char *path_attr = NULL;
      const char *rotate_attr = NULL;
      const char *key_points_attr = NULL;

      if (data->current_animation)
        {
          skip_element (data, context, "Nested animation elements are not allowed: <%s>", element_name);
          return;
        }

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
          skip_element (data, context, "Skipping <%s>: bad attributes", element_name);
          return;
        }

      markup_filter_attributes (element_name,
                                attr_names, attr_values,
                                &handled,
                                "path", &path_attr,
                                "rotate", &rotate_attr,
                                "keyPoints", &key_points_attr,
                                NULL);

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (path_attr)
        {
          a->motion.path = gsk_path_parse (path_attr);

          if (!a->motion.path)
            {
              gtk_svg_invalid_attribute (data->svg, context, "path", "failed to parse: %s", path_attr);
              animation_drop_and_free (a);
              skip_element (data, context, "Skipping <%s>: bad 'path' attribute", element_name);
              return;
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

      if (key_points_attr)
        {
          GArray *points = parse_numbers2 (key_points_attr, ";", 0, 1);
          if (!points)
            {
              gtk_svg_invalid_attribute (data->svg, context, "keyPoints", "failed to parse: %s", key_points_attr);
              g_array_unref (points);
              animation_drop_and_free (a);
              skip_element (data, context, "Skipping <%s>: bad 'keyPoints' attribute", element_name);
              return;
            }

          if (points->len != a->n_frames)
            {
              gtk_svg_invalid_attribute (data->svg, context, "keyPoints", "wrong number of values");
              g_array_unref (points);
              animation_drop_and_free (a);
              skip_element (data, context, "Skipping <%s>: bad 'keyPoints' attribute", element_name);

              return;
            }

          for (unsigned int i = 0; i < MIN (points->len, a->n_frames); i++)
            a->frames[i].point = g_array_index (points, double, i);

          g_array_unref (points);
        }

      if (!a->href || g_strcmp0 (a->href, data->current_shape->id) == 0)
        {
          a->shape = data->current_shape;
          g_ptr_array_add (data->current_shape->animations, a);
        }
      else
        {
          g_ptr_array_add (data->pending_animations, a);
        }

      if (a->id)
        g_hash_table_insert (data->animations, a->id, a);

      data->current_animation = a;

      return;
    }
  else if (strcmp (element_name, "mpath") == 0)
    {
      if (data->current_animation == NULL ||
          data->current_animation->type != ANIMATION_TYPE_MOTION ||
          data->current_animation->motion.path_ref != NULL)
        {
          skip_element (data, context, "<mpath> only allowed in <animateMotion>");
          return;
        }

      for (unsigned int i = 0; attr_names[i]; i++)
        {
          if (strcmp (attr_names[i], "href") == 0)
            {
              handled |= BIT (i);

              if (attr_values[i][0] == '#')
                data->current_animation->motion.path_ref = g_strdup (attr_values[i] + 1);
              else
                data->current_animation->motion.path_ref = g_strdup (attr_values[i]);
            }
        }

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (!data->current_animation->motion.path_ref)
        gtk_svg_missing_attribute (data->svg, context, "href", NULL);

      return;
    }
  else if (shape_type_lookup (element_name, &shape_type))
    {
      shape = shape_new (shape_type);
    }
  else if (strcmp (element_name, "stop") == 0)
    {
      const char *parent = g_markup_parse_context_get_element_stack (context)->next->data;
      SvgValue *value;
      const char *style_attr = NULL;

      if (strcmp (parent, "linearGradient") != 0 &&
          strcmp (parent, "radialGradient") != 0)
        {
          skip_element (data, context, "<stop> only allowed in <linearGradient> or <radialGradient>");
          return;
        }

      shape_add_color_stop (data->current_shape);
      for (unsigned int i = 0; attr_names[i]; i++)
        {
          if (strcmp (attr_names[i], "offset") == 0)
            {
              handled |= BIT (i);
              value = shape_attr_parse_value (SHAPE_ATTR_STOP_OFFSET, attr_values[i]);
              if (value)
                {
                  shape_set_base_value (data->current_shape, SHAPE_ATTR_STOP_OFFSET, value);
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
                  shape_set_base_value (data->current_shape, SHAPE_ATTR_STOP_COLOR, value);
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
                  shape_set_base_value (data->current_shape, SHAPE_ATTR_STOP_OPACITY, value);
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
        parse_style_attr (data->current_shape, style_attr, data, context);

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      return;
    }
  else
    {
      skip_element (data, context, "Unknown element: <%s>", element_name);
      return;
    }

  if (!shape_types[data->current_shape->type].has_shapes)
    {
      shape_free (shape);
      skip_element (data, context, "Parent element can't contain shapes");
      return;
    }

  parse_shape_attrs (shape,
                     element_name, attr_names, attr_values,
                     &handled, data, context);

  if (data->svg->gpa_version > 0)
    parse_shape_gpa_attrs (shape,
                           element_name, attr_names, attr_values,
                           &handled, data, context);

  gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

  g_ptr_array_add (data->current_shape->shapes, shape);
  data->shape_stack = g_slist_prepend (data->shape_stack, data->current_shape);
  data->current_shape = shape;

  if (shape->id)
    g_hash_table_insert (data->shapes, shape->id, shape);
}

static void
end_element_cb (GMarkupParseContext *context,
                const gchar         *element_name,
                gpointer             user_data,
                GError             **gmarkup_error)
{
  ParserData *data = user_data;
  ShapeType shape_type;

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
        }
      return;
    }

  if (shape_type_lookup (element_name, &shape_type))
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
resolve_clip_ref (SvgValue   *value,
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
        clip->ref.shape = target;
    }
}

static void
resolve_mask_ref (SvgValue   *value,
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
        mask->shape = target;
    }
}

static void
resolve_href_ref (SvgValue   *value,
                  ParserData *data)
{
  SvgHref *href = (SvgHref *) value;

  if (href->kind == HREF_REF && href->shape == NULL)
    {
      Shape *target = g_hash_table_lookup (data->shapes, href->ref);
      if (!target)
        gtk_svg_invalid_reference (data->svg, "No shape with ID %s (resolving <use>)", href->ref);
      else
        href->shape = target;
    }
}

static void
resolve_paint_ref (SvgValue   *value,
                   ParserData *data)
{
  SvgPaint *paint = (SvgPaint *) value;

  if (paint->kind == PAINT_GRADIENT && paint->gradient.shape == NULL)
    {
      Shape *target = g_hash_table_lookup (data->shapes, paint->gradient.ref);
      if (!target)
        gtk_svg_invalid_reference (data->svg, "No shape with ID %s (resolving fill or stroke)", paint->gradient.ref);
      else if (target->type != SHAPE_LINEAR_GRADIENT &&
               target->type != SHAPE_RADIAL_GRADIENT)
        gtk_svg_invalid_reference (data->svg, "Shape with ID %s not a gradient (resolving fill or stroke)", paint->gradient.ref);
      else
        paint->gradient.shape = target;
    }
}

static void
resolve_animation_refs (Shape      *shape,
                        ParserData *data)
{
  /* TODO: detect cycles */
  for (unsigned int i = 0; i < shape->animations->len; i++)
    {
      Animation *a = g_ptr_array_index (shape->animations, i);
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

      if (a->attr == SHAPE_ATTR_CLIP_PATH)
        {
          for (unsigned int j = 0; j < a->n_frames; j++)
            resolve_clip_ref (a->frames[j].value, data);
        }
      else if (a->attr == SHAPE_ATTR_MASK)
        {
          for (unsigned int j = 0; j < a->n_frames; j++)
            resolve_mask_ref (a->frames[j].value, data);
        }
      else if (a->attr == SHAPE_ATTR_HREF)
        {
          for (unsigned int j = 0; j < a->n_frames; j++)
            resolve_href_ref (a->frames[j].value, data);
        }
      else if (a->attr == SHAPE_ATTR_FILL ||
               a->attr == SHAPE_ATTR_STROKE)
        {
          for (unsigned int j = 0; j < a->n_frames; j++)
            resolve_paint_ref (a->frames[j].value, data);
        }

      if (a->motion.path_ref)
        {
          a->motion.path_shape = g_hash_table_lookup (data->shapes, a->motion.path_ref);
          if (a->motion.path_shape == NULL)
            gtk_svg_invalid_reference (data->svg,
                                       "No path with ID %s (resolving <mpath>",
                                       a->motion.path_ref);
          else if (a->id && g_str_has_prefix (a->id, "gpa:attachment:"))
            /* a's path is attached to a->motion.path_shape
             * Make sure it moves along with transitions and animations
             */
            create_attachment_connection (a, a->motion.path_shape, data->svg->timeline);
        }
    }

  if (shape_types[shape->type].has_shapes)
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          Shape *sh = g_ptr_array_index (shape->shapes, i);
          resolve_animation_refs (sh, data);
        }
    }
}

static void
resolve_shape_refs (Shape      *shape,
                    ParserData *data)
{
  resolve_clip_ref (shape->base[SHAPE_ATTR_CLIP_PATH], data);
  resolve_mask_ref (shape->base[SHAPE_ATTR_MASK], data);
  resolve_href_ref (shape->base[SHAPE_ATTR_HREF], data);
  resolve_paint_ref (shape->base[SHAPE_ATTR_FILL], data);
  resolve_paint_ref (shape->base[SHAPE_ATTR_STROKE], data);
}

static void gtk_svg_clear_content (GtkSvg *self);

static void
gtk_svg_init_from_bytes (GtkSvg *self,
                         GBytes *bytes)
{
  ParserData data;
  GMarkupParseContext *context;
  GMarkupParser parser = {
    start_element_cb,
    end_element_cb,
    NULL,
    NULL,
    NULL,
  };
  GError *error = NULL;

  data.svg = self;
  data.current_shape = self->content;
  data.shape_stack = NULL;
  data.shapes = g_hash_table_new (g_str_hash, g_str_equal);
  data.animations = g_hash_table_new (g_str_hash, g_str_equal);
  data.current_animation = NULL;
  data.pending_animations = g_ptr_array_new_with_free_func (animation_free);
  data.pending_refs = g_ptr_array_new ();
  data.skip.to = NULL;
  data.skip.reason = NULL;

  context = g_markup_parse_context_new (&parser, G_MARKUP_PREFIX_ERROR_POSITION, &data, NULL);
  if (!g_markup_parse_context_parse (context,
                                     g_bytes_get_data (bytes, NULL),
                                     g_bytes_get_size (bytes),
                                     &error) ||
      !g_markup_parse_context_end_parse (context, &error))
    {
      gtk_svg_emit_error (self, error);
      g_clear_error (&error);
      gtk_svg_clear_content (self);
      g_slist_free (data.shape_stack);
      g_clear_pointer (&data.skip.reason, g_free);

      g_ptr_array_set_size (data.pending_animations, 0);
      g_ptr_array_set_size (data.pending_refs, 0);
    }
  else
    {
      g_assert (data.current_shape == self->content);
      g_assert (data.shape_stack == NULL);
      g_assert (data.current_animation == NULL);
      g_assert (data.skip.to == NULL);
      g_assert (data.skip.reason == NULL);
    }

  g_markup_parse_context_free (context);

  for (unsigned int i = 0; i < data.pending_animations->len; i++)
    {
      Animation *a = g_ptr_array_index (data.pending_animations, i);

      g_assert (a->href != NULL);
      g_assert (a->shape == NULL);

      a->shape = g_hash_table_lookup (data.shapes, a->href);
      if (!a->shape)
        {
          g_set_error (&error, GTK_SVG_ERROR, GTK_SVG_ERROR_INVALID_REFERENCE,
                       "No shape with ID %s (resolving begin or end attribute)", a->href);
          gtk_svg_emit_error (self, error);
          g_clear_error (&error);
        }

      g_ptr_array_add (a->shape->animations, a);
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

  self->state_change_delay = timeline_get_state_change_delay (self->timeline);

  g_hash_table_unref (data.shapes);
  g_hash_table_unref (data.animations);
  g_ptr_array_unref (data.pending_animations);
  g_ptr_array_unref (data.pending_refs);
}

/* }}} */
/* {{{ Serialization */

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
  if (shape->id)
    {
      indent_for_attr (s, indent);
      g_string_append_printf (s, "id='%s'", shape->id);
    }

  if (!shape->display)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "display='none'");
    }

  for (ShapeAttr attr = 0; attr < G_N_ELEMENTS (shape_attrs); attr++)
    {
      if (!shape_has_attr (shape->type, attr))
        continue;

      if ((shape->attrs & BIT (attr)) || (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME))
        {
          SvgValue *value;

          if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
            value = shape_get_current_value (shape, attr);
          else
            value = shape_get_base_value (shape, NULL, attr);

          if (value &&
              ((shape->attrs & BIT (attr)) != 0 ||
               !svg_value_equal (value, shape_attr_get_initial_value (attr, shape->type))))
            {
              indent_for_attr (s, indent);
              g_string_append_printf (s, "%s='", shape_attr_get_presentation (attr, shape->type));
              svg_value_print (value, s);
              g_string_append_c (s, '\'');
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

  if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
    values = shape->current;
  else
    values = shape->base;

  if ((shape->attrs & BIT (SHAPE_ATTR_STROKE_MINWIDTH)) ||
      (shape->attrs & BIT (SHAPE_ATTR_STROKE_MAXWIDTH)))
    {
      indent_for_attr (s, indent);
      g_string_append (s, "gpa:stroke-width='");
      svg_value_print (values[SHAPE_ATTR_STROKE_MINWIDTH], s);
      g_string_append_c (s, ' ');
      svg_value_print (values[SHAPE_ATTR_STROKE_WIDTH], s);
      g_string_append_c (s, ' ');
      svg_value_print (values[SHAPE_ATTR_STROKE_MAXWIDTH], s);
      g_string_append_c (s, '\'');
    }

  paint = (SvgPaint *) values[SHAPE_ATTR_STROKE];
  if (shape->attrs & BIT (SHAPE_ATTR_STROKE) && paint->kind == PAINT_SYMBOLIC)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "gpa:stroke='");
      svg_paint_print_gpa (values[SHAPE_ATTR_STROKE], s);
      g_string_append_c (s, '\'');
    }

  paint = (SvgPaint *) values[SHAPE_ATTR_STROKE];
  if (shape->attrs & BIT (SHAPE_ATTR_FILL) && paint->kind == PAINT_SYMBOLIC)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "gpa:fill='");
      svg_paint_print_gpa (values[SHAPE_ATTR_FILL], s);
      g_string_append_c (s, '\'');
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
          g_string_append (s, "dur='");
          string_append_double (s, a->simple_duration / (double) G_TIME_SPAN_MILLISECOND);
          g_string_append (s, "ms'");
        }
    }

  if (a->has_repeat_count)
    {
      indent_for_attr (s, indent);
      if (a->repeat_count == DBL_MAX)
        {
          g_string_append (s, "repeatCount='indefinite'");
        }
      else
        {
          g_string_append (s, "repeatCount='");
          string_append_double (s, a->repeat_count);
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
          g_string_append (s, "repeatDur='");
          string_append_double (s, a->repeat_duration / (double) G_TIME_SPAN_MILLISECOND);
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
          if (a->type != ANIMATION_TYPE_SET)
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
          if (i > 0)
            g_string_append (s, "; ");
          for (unsigned int j = 0; j < 4; j++)
            {
              if (j > 0)
                g_string_append_c (s, ' ');
              string_append_double (s, a->frames[i].params[j]);
            }
        }
      g_string_append_c (s, '\'');
    }

  indent_for_attr (s, indent);
  g_string_append (s, "keyTimes='");
  for (unsigned int i = 0; i < a->n_frames; i++)
    {
      if (i > 0)
        g_string_append (s, "; ");
      string_append_double (s, a->frames[i].time);
    }
  g_string_append_c (s, '\'');

  if (a->calc_mode != CALC_MODE_LINEAR)
    {
      const char *modes[] = { "discrete", "linear", "spline" };
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
              g_string_append (s, "gpa:computed-simple-duration='");
              string_append_double (s, d / (double) G_TIME_SPAN_MILLISECOND);
              g_string_append (s, "ms'");
            }
        }

      if (a->current.begin != INDEFINITE)
        {
          indent_for_attr (s, indent);
          g_string_append (s, "gpa:current-start-time='");
          string_append_double (s, (a->current.begin - svg->load_time) / (double) G_TIME_SPAN_MILLISECOND);
          g_string_append (s, "ms'");
        }

      if (a->current.end != INDEFINITE)
        {
          indent_for_attr (s, indent);
          g_string_append (s, "gpa:current-end-time='");
          string_append_double (s, (a->current.end - svg->load_time) / (double) G_TIME_SPAN_MILLISECOND);
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

  indent_for_attr (s, indent);
  g_string_append (s, "keyPoints='");
  for (unsigned int i = 0; i < a->n_frames; i++)
    {
      if (i > 0)
        g_string_append (s, "; ");
      string_append_double (s, a->frames[i].point);
    }
  g_string_append (s, "'");

  if (a->motion.rotate != ROTATE_FIXED)
    {
      const char *values[] = { "auto", "auto-reverse" };
      indent_for_attr (s, indent);
      g_string_append_printf (s, "rotate='%s'", values[a->motion.rotate]);
    }
  else if (a->motion.angle != 0)
    {
      indent_for_attr (s, indent);
      g_string_append (s, "rotate='");
      string_append_double (s, a->motion.angle);
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
        path = animation_motion_get_current_path (a, &svg->view_box.size);
      else
        path = animation_motion_get_base_path (a, &svg->view_box.size);
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
  if ((flags & GTK_SVG_SERIALIZE_EXCLUDE_ANIMATION))
    return;

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

  for (unsigned int i = 0; i < N_STOP_PROPS; i++)
    {
      indent_for_attr (s, indent);
      g_string_append_printf (s, "%s='", names[i]);
      svg_value_print (values[i], s);
      g_string_append (s, "'");
    }
  g_string_append (s, ">");

  for (unsigned int i = 0; i < shape->animations->len; i++)
    {
      Animation *a = g_ptr_array_index (shape->animations, i);
      if (SHAPE_ATTR_STOP_OPACITY + N_STOP_PROPS * idx <= a->attr &&
          a->attr < SHAPE_ATTR_STOP_OPACITY + N_STOP_PROPS * (idx + 1))
        {
          serialize_animation (s, svg, indent + 2, a, flags);
        }
    }

  indent_for_elt (s, indent);
  g_string_append (s, "</stop>");
}

static void
serialize_shape (GString              *s,
                 GtkSvg               *svg,
                 int                   indent,
                 Shape                *shape,
                 GtkSvgSerializeFlags  flags)
{
  if (indent > 0) /* Hack: this is for <svg> */
    {
      indent_for_elt (s, indent);
      g_string_append_printf (s, "<%s", shape_types[shape->type].name);
      serialize_shape_attrs (s, svg, indent, shape, flags);
      serialize_gpa_attrs (s, svg, indent, shape, flags);
      g_string_append_c (s, '>');
    }

  if (shape_types[shape->type].has_color_stops)
    {
      for (unsigned int idx = 0; idx < shape->color_stops->len; idx++)
        serialize_color_stop (s, svg, indent + 2, shape, idx, flags);
    }

  for (unsigned int i = 0; i < shape->animations->len; i++)
    {
      Animation *a = g_ptr_array_index (shape->animations, i);
      if (a->attr < SHAPE_ATTR_STOP_OFFSET)
        serialize_animation (s, svg, indent, a, flags);
    }

  if (shape_types[shape->type].has_shapes)
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

typedef enum
{
  RENDERING,
  CLIPPING,
  MASKING,
} RenderOp;

typedef struct
{
  GtkSvg *svg;
  const graphene_size_t *viewport;
  GtkSnapshot *snapshot;
  graphene_rect_t bounds;
  int64_t current_time;
  const GdkRGBA *colors;
  size_t n_colors;
  double weight;
  RenderOp op;
  GskFillRule clip_rule;
  GSList *op_stack;
  int depth;
} PaintContext;

static void
push_op (PaintContext *context,
         RenderOp      op,
         GskFillRule   clip_rule)
{
  unsigned int val = ((context->op << 16) | context->clip_rule);
  context->op_stack = g_slist_prepend (context->op_stack, GUINT_TO_POINTER (val));
  context->op = op;
  context->clip_rule = clip_rule;
}

static void
pop_op (PaintContext *context)
{
  GSList *tos = context->op_stack;
  guint val;

  g_assert (tos != NULL);

  val = GPOINTER_TO_UINT (tos->data);
  context->op = val >> 16;
  context->clip_rule = val & 0xffff;

  context->op_stack = context->op_stack->next;
  g_slist_free_1 (tos);
}

static void paint_shape (Shape        *shape,
                         PaintContext *context);
static void pop_context (Shape        *shape,
                         PaintContext *context);
static void render_shape (Shape        *shape,
                          PaintContext *context);


static void
push_context (Shape        *shape,
              PaintContext *context)
{
  SvgFilter *filter = (SvgFilter *) shape->current[SHAPE_ATTR_FILTER];
  SvgValue *opacity = shape->current[SHAPE_ATTR_OPACITY];
  SvgClip *clip = (SvgClip *) shape->current[SHAPE_ATTR_CLIP_PATH];
  SvgMask *mask = (SvgMask *) shape->current[SHAPE_ATTR_MASK];
  SvgTransform *tf = (SvgTransform *) shape->current[SHAPE_ATTR_TRANSFORM];

  for (unsigned int i = filter->n_functions; i > 0; i--)
    {
      FilterFunction *f = &filter->functions[i - 1];

      switch (f->kind)
        {
        case FILTER_NONE:
          break;
        case FILTER_BLUR:
          gtk_snapshot_push_blur (context->snapshot, f->value);
          break;
        case FILTER_OPACITY:
          gtk_snapshot_push_opacity (context->snapshot, f->value);
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
            svg_filter_get_matrix (f, &matrix, &offset);
            gtk_snapshot_push_color_matrix (context->snapshot, &matrix, &offset);
          }
          break;
        case FILTER_ALPHA_LEVEL:
          {
            GskComponentTransfer *identity, *alpha;
            float values[10];

            identity = gsk_component_transfer_new_identity ();
            for (unsigned int j = 0; j < 10; j++)
              {
                if ((j + 1) / 10.0 <= f->value)
                  values[j] = 0;
                else
                  values[j] = 1;
              }
            alpha = gsk_component_transfer_new_discrete (10, values);
            gtk_snapshot_push_component_transfer (context->snapshot, identity, identity, identity, alpha);
            gsk_component_transfer_free (identity);
            gsk_component_transfer_free (alpha);
          }
          break;
        default:
          g_assert_not_reached ();
        }
    }

  if (svg_number_get (opacity, 1) != 1)
    gtk_snapshot_push_opacity (context->snapshot, svg_number_get (opacity, 1));

  if (clip->kind == CLIP_PATH ||
      (clip->kind == CLIP_REF && clip->ref.shape != NULL))
    {
      push_op (context, CLIPPING, svg_enum_get (shape->current[SHAPE_ATTR_CLIP_RULE]));
      gtk_snapshot_push_mask (context->snapshot, GSK_MASK_MODE_ALPHA);

      if (clip->kind == CLIP_PATH)
        {
          gtk_snapshot_append_fill (context->snapshot,
                                    clip->path,
                                    GSK_FILL_RULE_WINDING, /* FIXME fill rule */
                                    &(GdkRGBA) { 1, 1, 1, 1 });
        }
      else
        {
          render_shape (clip->ref.shape, context);
        }

      gtk_snapshot_pop (context->snapshot);

      pop_op (context);
    }

  if (mask->kind != MASK_NONE && (mask->shape != NULL))
    {
      push_op (context, MASKING, context->clip_rule);

      gtk_snapshot_push_mask (context->snapshot, svg_enum_get (shape->current[SHAPE_ATTR_MASK_TYPE]));

      render_shape (mask->shape, context);

      gtk_snapshot_pop (context->snapshot);

      pop_op (context);
    }

  if (tf->transforms[0].type != TRANSFORM_NONE)
    {
      GskTransform *transform;

      transform = svg_transform_get_gsk (tf);
      gtk_snapshot_save (context->snapshot);
      gtk_snapshot_transform (context->snapshot, transform);
      gsk_transform_unref (transform);
    }
}

static void
pop_context (Shape        *shape,
             PaintContext *context)
{
  SvgFilter *filter = (SvgFilter *) shape->current[SHAPE_ATTR_FILTER];
  SvgValue *opacity = shape->current[SHAPE_ATTR_OPACITY];
  SvgClip *clip = (SvgClip *) shape->current[SHAPE_ATTR_CLIP_PATH];
  SvgMask *mask = (SvgMask *) shape->current[SHAPE_ATTR_MASK];
  SvgTransform *tf = (SvgTransform *) shape->current[SHAPE_ATTR_TRANSFORM];

  if (tf->transforms[0].type != TRANSFORM_NONE)
    gtk_snapshot_restore (context->snapshot);

  if (svg_number_get (opacity, 1) != 1)
    gtk_snapshot_pop (context->snapshot);

  for (unsigned int i = 0; i < filter->n_functions; i++)
    if (filter->functions[i].kind != FILTER_NONE)
      gtk_snapshot_pop (context->snapshot);

  if (clip->kind == CLIP_PATH ||
      (clip->kind == CLIP_REF && clip->ref.shape != NULL))
    gtk_snapshot_pop (context->snapshot);

  if (mask->kind != MASK_NONE && (mask->shape != NULL))
    gtk_snapshot_pop (context->snapshot);
}

static void
paint_linear_gradient (Shape                 *gradient,
                       const graphene_rect_t *bounds,
                       PaintContext          *context)
{
  graphene_point_t start, end;
  GskColorStop *stops;
  double offset;
  GskTransform *transform, *gradient_transform;

  stops = g_newa (GskColorStop, gradient->color_stops->len);
  offset = 0;
  for (unsigned int i = 0; i < gradient->color_stops->len; i++)
    {
      ColorStop *cs = g_ptr_array_index (gradient->color_stops, i);
      SvgPaint *stop_color = (SvgPaint *) cs->current[COLOR_STOP_COLOR];

      g_assert (stop_color->kind == PAINT_COLOR);
      stops[i].offset = MAX (svg_number_get (cs->current[COLOR_STOP_OFFSET], 1), offset);
      stops[i].color = stop_color->color;
      stops[i].color.alpha *= svg_number_get (cs->current[COLOR_STOP_OPACITY], 1);
      offset = stops[i].offset;
    }

  transform = NULL;
  if (svg_enum_get (gradient->current[SHAPE_ATTR_GRADIENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
    {
      transform = gsk_transform_translate (transform, &bounds->origin);
      transform = gsk_transform_scale (transform, bounds->size.width, bounds->size.height);
      graphene_point_init (&start,
                           svg_number_get (gradient->current[SHAPE_ATTR_X1], 1),
                           svg_number_get (gradient->current[SHAPE_ATTR_Y1], 1));
      graphene_point_init (&end,
                           svg_number_get (gradient->current[SHAPE_ATTR_X2], 1),
                           svg_number_get (gradient->current[SHAPE_ATTR_Y2], 1));
    }
  else
    {
      graphene_point_init (&start,
                           svg_number_get (gradient->current[SHAPE_ATTR_X1], context->viewport->width),
                           svg_number_get (gradient->current[SHAPE_ATTR_Y1], context->viewport->height));
      graphene_point_init (&end,
                           svg_number_get (gradient->current[SHAPE_ATTR_X2], context->viewport->width),
                           svg_number_get (gradient->current[SHAPE_ATTR_Y2], context->viewport->height));
    }

  gradient_transform = svg_transform_get_gsk ((SvgTransform *) gradient->current[SHAPE_ATTR_TRANSFORM]);
  transform = gsk_transform_transform (transform, gradient_transform);
  gsk_transform_unref (gradient_transform);
  transform_gradient_line (transform, &start, &end, &start, &end);
  gsk_transform_unref (transform);

  switch (svg_enum_get (gradient->current[SHAPE_ATTR_SPREAD_METHOD]))
    {
    case SPREAD_METHOD_PAD:
      gtk_snapshot_append_linear_gradient (context->snapshot,
                                           bounds, &start, &end,
                                           stops, gradient->color_stops->len);
      break;
    case SPREAD_METHOD_REFLECT:
      gtk_svg_rendering_error (context->svg, "the 'reflect' spreadMethod is not implemented");
      G_GNUC_FALLTHROUGH;
    case SPREAD_METHOD_REPEAT:
      gtk_snapshot_append_repeating_linear_gradient (context->snapshot,
                                                     bounds, &start, &end,
                                                     stops, gradient->color_stops->len);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
paint_radial_gradient (Shape                 *gradient,
                       const graphene_rect_t *bounds,
                       PaintContext          *context)
{
  graphene_point_t center;
  double r, fr;
  double radius, start, end;
  GskColorStop *stops;
  double offset;
  GskTransform *gradient_transform;
  graphene_rect_t gradient_bounds;

  graphene_point_init (&center, svg_number_get (gradient->current[SHAPE_ATTR_CX], context->viewport->width),
                                svg_number_get (gradient->current[SHAPE_ATTR_CY], context->viewport->height));

  if ((((gradient->attrs & BIT (SHAPE_ATTR_FX)) != 0 &&
        svg_number_get (gradient->current[SHAPE_ATTR_FX], context->viewport->width) != center.x)) ||
      (((gradient->attrs & BIT (SHAPE_ATTR_FY)) != 0 &&
        svg_number_get (gradient->current[SHAPE_ATTR_FY], context->viewport->height) != center.y)))
    gtk_svg_rendering_error (context->svg, "non-concentric radial gradients are not implemented");

  r = svg_number_get (gradient->current[SHAPE_ATTR_R], normalized_diagonal (context->viewport));
  fr = svg_number_get (gradient->current[SHAPE_ATTR_FR], normalized_diagonal (context->viewport));

  radius = MAX (r, fr);
  start = fr / radius;
  end = r / radius;

  stops = g_newa (GskColorStop, gradient->color_stops->len);
  offset = 0;
  for (unsigned int i = 0; i < gradient->color_stops->len; i++)
    {
      ColorStop *cs = g_ptr_array_index (gradient->color_stops, i);
      SvgPaint *stop_color = (SvgPaint *) cs->current[COLOR_STOP_COLOR];

      g_assert (stop_color->kind == PAINT_COLOR);
      stops[i].offset = MAX (svg_number_get (cs->current[COLOR_STOP_OFFSET], 1), offset);
      stops[i].color = stop_color->color;
      stops[i].color.alpha *= svg_number_get (cs->current[COLOR_STOP_OPACITY], 1);
      offset = stops[i].offset;
    }

  gtk_snapshot_save (context->snapshot);

  if (svg_enum_get (gradient->current[SHAPE_ATTR_GRADIENT_UNITS]) == COORD_UNITS_OBJECT_BOUNDING_BOX)
    {
      gtk_snapshot_translate (context->snapshot, &bounds->origin);
      gtk_snapshot_scale (context->snapshot,  bounds->size.width, bounds->size.height);
      graphene_rect_init (&gradient_bounds, 0, 0, 1, 1);
    }
  else
    graphene_rect_init_from_rect (&gradient_bounds, bounds);

  gradient_transform = svg_transform_get_gsk ((SvgTransform *) gradient->current[SHAPE_ATTR_TRANSFORM]);
  gtk_snapshot_transform (context->snapshot, gradient_transform);

  gradient_transform = gsk_transform_invert (gradient_transform);
  gsk_transform_transform_bounds (gradient_transform, &gradient_bounds, &gradient_bounds);
  gsk_transform_unref (gradient_transform);

  switch (svg_enum_get (gradient->current[SHAPE_ATTR_SPREAD_METHOD]))
    {
    case SPREAD_METHOD_PAD:
      gtk_snapshot_append_radial_gradient (context->snapshot,
                                           &gradient_bounds,
                                           &center, radius, radius,
                                           start, end,
                                           stops, gradient->color_stops->len);
      break;
    case SPREAD_METHOD_REFLECT:
      gtk_svg_rendering_error (context->svg, "the 'reflect' spreadMethod is not implemented");
      G_GNUC_FALLTHROUGH;
    case SPREAD_METHOD_REPEAT:
      gtk_snapshot_append_repeating_radial_gradient (context->snapshot,
                                                     &gradient_bounds,
                                                     &center, radius, radius,
                                                     start, end,
                                                     stops, gradient->color_stops->len);
      break;
    default:
      g_assert_not_reached ();
    }

  gtk_snapshot_restore (context->snapshot);
}

static void
paint_gradient (Shape                 *gradient,
                const graphene_rect_t *bounds,
                PaintContext          *context)
{
  if (!gradient)
    return;

  if (gradient->color_stops->len == 0)
    return;

  if (gradient->color_stops->len == 1)
    {
      ColorStop *cs = g_ptr_array_index (gradient->color_stops, 0);
      SvgPaint *stop_color = (SvgPaint *) cs->current[COLOR_STOP_COLOR];
      GdkRGBA color = stop_color->color;
      color.alpha *= svg_number_get (cs->current[COLOR_STOP_OPACITY], 1);
      gtk_snapshot_append_color (context->snapshot, &color, bounds);
      return;
    }

  if (gradient->type == SHAPE_LINEAR_GRADIENT)
    paint_linear_gradient (gradient, bounds, context);
  else
    paint_radial_gradient (gradient, bounds, context);
}

static GskStroke *
shape_create_stroke (Shape        *shape,
                     PaintContext *context)
{
  GskStroke *stroke;
  double width;
  SvgDashArray *dasharray;

  width = width_apply_weight (svg_number_get (shape->current[SHAPE_ATTR_STROKE_WIDTH], 1),
                              svg_number_get (shape->current[SHAPE_ATTR_STROKE_MINWIDTH], 1),
                              svg_number_get (shape->current[SHAPE_ATTR_STROKE_MAXWIDTH], 1),
                              context->weight);

  stroke = gsk_stroke_new (width);

  gsk_stroke_set_line_cap (stroke, svg_enum_get (shape->current[SHAPE_ATTR_STROKE_LINECAP]));
  gsk_stroke_set_line_join (stroke, svg_enum_get (shape->current[SHAPE_ATTR_STROKE_LINEJOIN]));
  gsk_stroke_set_miter_limit (stroke, svg_number_get (shape->current[SHAPE_ATTR_STROKE_MITERLIMIT], 1));

  dasharray = (SvgDashArray *) shape->current[SHAPE_ATTR_STROKE_DASHARRAY];
  if (dasharray->kind != DASH_ARRAY_NONE)
    {
      double *dashes = (double *) dasharray->dashes;
      unsigned int len = dasharray->n_dashes;
      double path_length;
      double offset;

      if (shape->attrs & BIT (SHAPE_ATTR_PATH_LENGTH))
        path_length = svg_number_get (shape->current[SHAPE_ATTR_PATH_LENGTH], 1);
      else /* FIXME makes mixing animations hard */
        path_length = 1;

      offset = svg_number_get (shape->current[SHAPE_ATTR_STROKE_DASHOFFSET], path_length);

      float *vals = g_newa (float, len);

      if (path_length > 0)
        {
          GskPathMeasure *measure;
          double length;

          measure = shape_get_current_measure (shape, context->viewport);
          length = gsk_path_measure_get_length (measure);
          gsk_path_measure_unref (measure);

          for (unsigned int i = 0; i < len; i++)
            vals[i] = dashes[i] / path_length * length;

          offset = offset / path_length * length;
        }
      else
        {
          for (unsigned int i = 0; i < len; i++)
            vals[i] = dashes[i];
        }

      gsk_stroke_set_dash (stroke, vals, len);
      gsk_stroke_set_dash_offset (stroke, offset);
    }

  return stroke;
}

static void
paint_shape (Shape        *shape,
             PaintContext *context)
{
  GskPath *path;
  graphene_rect_t bounds;

  if (shape->type == SHAPE_USE)
    {
      if (((SvgHref *) shape->current[SHAPE_ATTR_HREF])->shape != NULL)
        {
          Shape *use_shape = ((SvgHref *) shape->current[SHAPE_ATTR_HREF])->shape;
          ComputeContext use_context;
          double x, y;
          x = svg_number_get (shape->current[SHAPE_ATTR_X], context->viewport->width);
          y = svg_number_get (shape->current[SHAPE_ATTR_Y], context->viewport->height);
          gtk_snapshot_save (context->snapshot);
          gtk_snapshot_translate (context->snapshot, &GRAPHENE_POINT_INIT (x, y));

          /* FIXME: this isn't the best way of doing this */
          use_context.svg = context->svg;
          use_context.viewport = &context->svg->view_box.size;
          use_context.parent = shape;
          use_context.current_time = context->current_time;
          use_context.colors = context->colors;
          use_context.n_colors = context->n_colors;
          compute_current_values_for_shape (use_shape, &use_context);

          render_shape (use_shape, context);

          gtk_snapshot_restore (context->snapshot);
        }
      return;
    }

  if (shape_types[shape->type].has_shapes)
    {
      for (int i = 0; i < shape->shapes->len; i++)
        {
          Shape *s = g_ptr_array_index (shape->shapes, i);
          render_shape (s, context);
        }

      return;
    }

  if (svg_enum_get (shape->current[SHAPE_ATTR_VISIBILITY]) == VISIBILITY_HIDDEN)
    return;

  path = shape_get_current_path (shape, context->viewport);

  if (context->op == RENDERING || context->op == MASKING)
    {
      SvgPaint *paint;

      paint = (SvgPaint *) shape->current[SHAPE_ATTR_FILL];
      if (paint->kind != PAINT_NONE && gsk_path_get_bounds (path, &bounds))
        {
          GskFillRule fill_rule;
          double opacity;

          fill_rule = svg_enum_get (shape->current[SHAPE_ATTR_FILL_RULE]);
          opacity = svg_number_get (shape->current[SHAPE_ATTR_FILL_OPACITY], 1);
          if (paint->kind == PAINT_COLOR)
            {
              GdkRGBA color = paint->color;
              color.alpha *= opacity;
              gtk_snapshot_append_fill (context->snapshot, path, fill_rule, &color);
            }
          else if (paint->kind == PAINT_GRADIENT)
            {
              if (opacity < 1)
                gtk_snapshot_push_opacity (context->snapshot, opacity);

              gtk_snapshot_push_fill (context->snapshot, path, fill_rule);
              paint_gradient (paint->gradient.shape, &bounds, context);
              gtk_snapshot_pop (context->snapshot);

              if (opacity < 1)
                gtk_snapshot_pop (context->snapshot);
            }
          else
            g_assert_not_reached ();
        }

      paint = (SvgPaint *) shape->current[SHAPE_ATTR_STROKE];
      if (paint->kind != PAINT_NONE)
        {
          GskStroke *stroke;

          stroke = shape_create_stroke (shape, context);
          if (gsk_path_get_stroke_bounds (path, stroke, &bounds))
            {
              double opacity;

              opacity = svg_number_get (shape->current[SHAPE_ATTR_STROKE_OPACITY], 1);
              if (paint->kind == PAINT_COLOR)
                {
                  GdkRGBA color = paint->color;
                  color.alpha *= opacity;
                  gtk_snapshot_append_stroke (context->snapshot, path, stroke, &color);
                }
              else if (paint->kind == PAINT_GRADIENT)
                {
                  if (opacity < 1)
                    gtk_snapshot_push_opacity (context->snapshot, opacity);

                  gtk_snapshot_push_stroke (context->snapshot, path, stroke);
                  paint_gradient (paint->gradient.shape, &bounds, context);
                  gtk_snapshot_pop (context->snapshot);

                  if (opacity < 1)
                    gtk_snapshot_pop (context->snapshot);
                }
              else
                g_assert_not_reached ();
            }
          gsk_stroke_free (stroke);
        }
    }
  else if (context->op == CLIPPING)
    {
      /* Clip mask - see language in the spec about 'raw geometry' */
      if (gsk_path_get_bounds (path, &bounds))
        {
          GdkRGBA color = { 0, 0, 0, 1 };

          gtk_snapshot_push_fill (context->snapshot, path, context->clip_rule);
          gtk_snapshot_append_color (context->snapshot, &color, &bounds);
          gtk_snapshot_pop (context->snapshot);
        }

    }

  gsk_path_unref (path);
}

static void
render_shape (Shape        *shape,
              PaintContext *context)
{
  if (!shape->display)
    return;

  if (context->op == RENDERING && shape_types[shape->type].never_rendered)
    return;

  context->depth++;
  if (context->depth > MAX_DEPTH)
    {
      gtk_svg_rendering_error (context->svg,
                               "excessive rendering depth (> %d), aborting",
                               MAX_DEPTH);
      return;
    }

  push_context (shape, context);
  paint_shape (shape, context);
  pop_context (shape, context);

  context->depth--;
}

/* }}} */
/* {{{ GtkSymbolicPaintable implementation */

static void
compute_viewport_transform (GtkSvg *self,
                            const graphene_rect_t *vb,
                            double e_x, double e_y,
                            double e_width, double e_height,
                            double *scale_x, double *scale_y,
                            double *translate_x, double *translate_y)
{
  double sx, sy, tx, ty;

  sx = e_width / vb->size.width;
  sy = e_height / vb->size.height;

  if (self->align != 0 && self->meet_or_slice == MEET)
    sx = sy = MIN (sx, sy);
  else if (self->align != 0 && self->meet_or_slice == SLICE)
    sx = sy = MAX (sx, sy);

  tx = e_x - vb->origin.x * sx;
  ty = e_y - vb->origin.y * sy;

  if (ALIGN_GET_X (self->align) == ALIGN_MID)
    tx += (e_width - vb->size.width * sx) / 2;
  else if (ALIGN_GET_X (self->align) == ALIGN_MAX)
    tx += (e_width - vb->size.width * sx);

  if (ALIGN_GET_Y (self->align) == ALIGN_MID)
    ty += (e_height - vb->size.height * sy) / 2;
  else if (ALIGN_GET_Y (self->align) == ALIGN_MAX)
    ty += (e_height - vb->size.height * sy);

  *scale_x = sx;
  *scale_y = sy;
  *translate_x = tx;
  *translate_y = ty;
}

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
  ComputeContext compute_context;
  PaintContext paint_context;
  graphene_rect_t view_box;
  double sx, sy, tx, ty;

  if (self->view_box.size.width == 0 || self->view_box.size.height == 0)
    graphene_rect_init (&view_box, 0, 0, self->width, self->height);
  else
    view_box = self->view_box;

  compute_viewport_transform (self, &view_box,
                              0, 0, width, height,
                              &sx, &sy, &tx, &ty);

  compute_context.svg = self;
  compute_context.viewport = &view_box.size;
  compute_context.colors = colors;
  compute_context.n_colors = n_colors;
  compute_context.current_time = self->current_time;
  compute_context.parent = NULL;

  compute_current_values_for_shape (self->content, &compute_context);

  paint_context.svg = self;
  paint_context.viewport = &view_box.size;
  paint_context.snapshot = snapshot;
  paint_context.bounds = self->bounds;
  paint_context.colors = colors;
  paint_context.n_colors = n_colors;
  paint_context.weight = self->weight >= 1 ? self->weight : weight;
  paint_context.op = RENDERING;
  paint_context.op_stack = NULL;
  paint_context.clip_rule = GSK_FILL_RULE_WINDING;
  paint_context.current_time = self->current_time;
  paint_context.depth = 0;

  gtk_snapshot_save (snapshot);
  gtk_snapshot_scale (snapshot, sx, sy);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (tx, ty));

  render_shape (self->content, &paint_context);

  gtk_snapshot_restore (snapshot);

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

  if (self->width > 0 && self->height > 0)
    return self->width / self->height;

  if (self->view_box.size.width > 0 && self->view_box.size.height > 0)
    return self->view_box.size.width / self->view_box.size.height;

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
  self->state = GTK_SVG_STATE_EMPTY;
  self->load_time = INDEFINITE;
  self->state_change_delay = 0;
  self->next_update = INDEFINITE;
  self->playing = FALSE;
  self->run_mode = GTK_SVG_RUN_MODE_STOPPED;

  self->timeline = timeline_new ();
  self->content = shape_new (SHAPE_GROUP);
}

static void
gtk_svg_dispose (GObject *object)
{
  GtkSvg *self = GTK_SVG (object);

  frame_clock_disconnect (self);
  g_clear_handle_id (&self->pending_invalidate, g_source_remove);

  shape_free (self->content);
  timeline_free (self->timeline);

  g_clear_object (&self->clock);

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
      {
        const char *path = g_value_get_string (value);
        if (path)
          {
            GBytes *bytes = g_resources_lookup_data (path, 0, NULL);
            gtk_svg_init_from_bytes (self, bytes);
            g_bytes_unref (bytes);
          }
      }
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

  shape_attr_init_default_values ();

  object_class->dispose = gtk_svg_dispose;
  object_class->get_property = gtk_svg_get_property;
  object_class->set_property = gtk_svg_set_property;

  /**
   * GtkSvg:resource:
   *
   * Construct-only property to create a paintable from
   * a resource in ui files.
   *
   * Since: 4.22
   */
  properties[PROP_RESOURCE] =
    g_param_spec_string ("resource", NULL, NULL,
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

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
  for (unsigned int i = 0; i < shape->animations->len; i++)
    {
      Animation *a = g_ptr_array_index (shape->animations, i);

      if (a->status == ANIMATION_STATUS_RUNNING)
        g_string_append_printf (string, " %s", a->id);
    }

  if (shape_types[shape->type].has_shapes)
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
  for (unsigned int i = 0; i < shape->animations->len; i++)
    {
      Animation *a = g_ptr_array_index (shape->animations, i);
      collect_next_update_for_animation (a, current_time, run_mode, next_update);
    }

  if (shape_types[shape->type].has_shapes)
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
shape_update_animation_state (Shape   *shape,
                              int64_t  current_time)
{
  for (unsigned int i = 0; i < shape->animations->len; i++)
    {
      Animation *a = g_ptr_array_index (shape->animations, i);
      animation_update_state (a, current_time);
    }

  if (shape_types[shape->type].has_shapes)
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
 * timescale as g_get_monotonic_time().
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

#ifdef DEBUG
  time_base = self->load_time;
  if (g_getenv ("SVG_DEBUG"))
    timeline_dump (self->timeline);
#endif

  timeline_set_load_time (self->timeline, load_time);

  update_animation_state (self);
  collect_next_update (self);
}

/*< private >
 * gtk_svg_advance:
 * @self: an SVG paintable
 * @current_time: the time to advance to
 *
 * Advances the animation to the given value,
 * which must be in microseconds and in the same
 * timescale as g_get_monotonic_time().
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

#ifdef DEBUG
  animation_state_dump (self);
#endif

  if (self->playing)
    schedule_next_update (self);
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
      context.viewport = &self->view_box.size;
      context.current_time = self->current_time;
      context.parent = NULL;
      context.colors = col;
      context.n_colors = n_col;

      compute_current_values_for_shape (self->content, &context);
    }

  g_string_append (s, "<svg");
  indent_for_attr (s, 0);
  g_string_append (s, "width='");
  string_append_double (s, self->width);
  g_string_append_c (s, '\'');
  indent_for_attr (s, 0);
  g_string_append (s, "height='");
  string_append_double (s, self->height);
  g_string_append_c (s, '\'');

  if (self->view_box.size.width != 0 && self->view_box.size.height != 0)
    {
      indent_for_attr (s, 0);
      g_string_append (s, "viewBox='");
      string_append_double (s, self->view_box.origin.x);
      g_string_append_c (s, ' ');
      string_append_double (s, self->view_box.origin.y);
      g_string_append_c (s, ' ');
      string_append_double (s, self->view_box.size.width);
      g_string_append_c (s, ' ');
      string_append_double (s, self->view_box.size.height);
      g_string_append_c (s, '\'');
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
      g_string_append (s, "gpa:state-change-delay='");
      string_append_double (s, (self->state_change_delay) / (double) G_TIME_SPAN_MILLISECOND);
      g_string_append (s, "ms'");
      if (self->load_time != INDEFINITE)
        {
          indent_for_attr (s, 0);
          g_string_append (s, "gpa:time-since-load='");
          string_append_double (s, (self->current_time - self->load_time) / (double) G_TIME_SPAN_MILLISECOND);
          g_string_append (s, "ms'");
        }
    }

  g_string_append (s, ">");

  serialize_shape (s, self, 0, self->content, flags);
  g_string_append (s, "\n</svg>\n");

  return g_string_free_to_bytes (s);
}

/**
 * gtk_svg_set_playing:
 * @self: an SVG paintable
 * @playing: the new state
 *
 * Sets whether the paintable is animating its content.
 *
 * Since: 4.22
 */
void
gtk_svg_set_playing (GtkSvg   *self,
                     gboolean  playing)
{
  if (self->playing == playing)
    return;

  self->playing = playing;

  if (playing)
    {
      if (self->load_time == INDEFINITE)
        gtk_svg_set_load_time (self, g_get_monotonic_time ());
      schedule_next_update (self);
    }
  else
    {
      frame_clock_disconnect (self);
      g_clear_handle_id (&self->pending_invalidate, g_source_remove);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PLAYING]);
}

/**
 * gtk_svg_clear_content:
 * @self: an SVG paintable
 *
 * Resets the paintable to the initial, empty state.
 *
 * Since: 4.22
 */
static void
gtk_svg_clear_content (GtkSvg *self)
{
  timeline_free (self->timeline);
  shape_free (self->content);

  self->timeline = timeline_new ();
  self->content = shape_new (SHAPE_GROUP);

  self->width = 0;
  self->height = 0;
  graphene_rect_init (&self->view_box, 0, 0, 0, 0);
  self->align = ALIGN_XY (ALIGN_MID, ALIGN_MID);
  self->meet_or_slice = MEET;

  self->state = 0;
  self->max_state = 0;
  self->state_change_delay = 0;

  self->gpa_version = 0;
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
  return g_object_new (GTK_TYPE_SVG, "resource", path, NULL);
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
 * @state: the state to set, as a value between 0 and 63,
 *   or `(unsigned int) -1`
 *
 * Sets the state of the paintable.
 *
 * Use [method@Gtk.Svg.get_n_states] to find out
 * what states @self has.
 *
 * Note that [method@Gtk.Svg.play] must have been
 * called for the SVG paintable to react to state changes.
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

  if (self->playing)
    {
      /* FIXME frame time */
      self->current_time = MAX (self->current_time, g_get_monotonic_time ());
    }

  previous_state = self->state;

  self->state = state;

  /* Don't jiggle things while we're still loading */
  if (self->load_time != INDEFINITE)
    {
      dbg_print ("state", "renderer state %u -> %u\n", previous_state, state);

      timeline_update_for_state (self->timeline,
                                 previous_state, self->state,
                                 self->current_time + self->state_change_delay);

      update_animation_state (self);
      collect_next_update (self);

#ifdef DEBUG
      animation_state_dump (self);
#endif

      if (self->playing)
        schedule_next_update (self);
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

/* }}} */
/* {{{ Animation */

/**
 * gtk_svg_set_frame_clock:
 * @self: an SVG paintable
 * @clock: the frame clock
 *
 * Sets a frame clock.
 *
 * Without a frame clock, GTK has to rely
 * on simple timeouts to run animations.
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

  if (was_connected)
    frame_clock_connect (self);
}

/**
 * gtk_svg_play:
 * @self: an SVG paintable
 *
 * Start playing animations.
 *
 * Note that this is necessary for state changes as
 * well.
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
 * Stop any playing animations.
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
 * @GTK_SVG_ERROR_INVALID_ELEMENT: An XML element is invalid
 *   (either because it is not part of SVG, or because it is
 *   in the wrong place, or because it not implemented in GTK)
 * @GTK_SVG_ERROR_INVALID_ATTRIBUTE: An XML attribute is invalid
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
