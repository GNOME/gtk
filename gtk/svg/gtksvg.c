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
#include "gtksvgrendererprivate.h"
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
#include "gtksvgtimespecprivate.h"
#include "gtksvggpaprivate.h"

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

/* {{{ Errors */

static unsigned int error_signal;

void
gtk_svg_emit_error (GtkSvg       *svg,
                    const GError *error)
{
  g_signal_emit (svg, error_signal, 0, error);
}

void
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

void
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

void
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

void
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
strv_unique (GStrv strv)
{
  if (strv)
    for (unsigned int i = 0; strv[i]; i++)
      for (unsigned int j = i + 1; strv[j]; j++)
        if (strcmp (strv[i], strv[j]) == 0)
          return FALSE;

  return TRUE;
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

GdkTexture *
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

void
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

gboolean
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

void
gtk_svg_advance_after_snapshot (GtkSvg *self)
{
  if (self->advance_after_snapshot)
    {
      self->advance_after_snapshot = FALSE;
      g_clear_handle_id (&self->pending_advance, g_source_remove);
      self->pending_advance = g_idle_add_once (advance_later, self);
    }
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

void
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

void
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
/* {{{ GtkSymbolicPaintable implementation */

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

  gtk_svg_snapshot_full (self, snapshot, width, height, colors, n_colors, weight);
}

static void
gtk_svg_snapshot_symbolic (GtkSymbolicPaintable *paintable,
                           GtkSnapshot          *snapshot,
                           double                width,
                           double                height,
                           const GdkRGBA        *colors,
                           size_t                n_colors)
{
  GtkSvg *self = GTK_SVG (paintable);

  gtk_svg_snapshot_full (self, snapshot, width, height, colors, n_colors, 400);
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
  GtkSvg *self = GTK_SVG (paintable);

  gtk_svg_snapshot_full (self, snapshot, width, height, NULL, 0, 400);
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

  if (shape->shapes)
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

  if (shape->shapes)
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

  if (shape->shapes)
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
