/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcssstylefuncsprivate.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo-gobject.h>

#include "gtkcsscolorvalueprivate.h"
#include "gtkcssimagegradientprivate.h"
#include "gtkcssprovider.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkcsstypedvalueprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkstylecontextprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwin32themeprivate.h"

#include "deprecated/gtkthemingengine.h"
#include "deprecated/gtkgradientprivate.h"
#include "deprecated/gtksymboliccolorprivate.h"

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

static GHashTable *parse_funcs = NULL;
static GHashTable *print_funcs = NULL;
static GHashTable *compute_funcs = NULL;

typedef gboolean         (* GtkStyleParseFunc)             (GtkCssParser            *parser,
                                                            GValue                  *value);
typedef void             (* GtkStylePrintFunc)             (const GValue            *value,
                                                            GString                 *string);
typedef GtkCssValue *    (* GtkStyleComputeFunc)           (GtkStyleProviderPrivate *provider,
                                                            GtkCssStyle             *values,
                                                            GtkCssStyle             *parent_values,
                                                            GtkCssValue             *specified,
                                                            GtkCssDependencies      *dependencies);

static void
register_conversion_function (GType               type,
                              GtkStyleParseFunc   parse,
                              GtkStylePrintFunc   print,
                              GtkStyleComputeFunc compute)
{
  if (parse)
    g_hash_table_insert (parse_funcs, GSIZE_TO_POINTER (type), parse);
  if (print)
    g_hash_table_insert (print_funcs, GSIZE_TO_POINTER (type), print);
  if (compute)
    g_hash_table_insert (compute_funcs, GSIZE_TO_POINTER (type), compute);
}

static void
string_append_double (GString *string,
                      double   d)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr (buf, sizeof (buf), d);
  g_string_append (string, buf);
}

static void
string_append_string (GString    *str,
                      const char *string)
{
  _gtk_css_print_string (str, string);
}

/*** IMPLEMENTATIONS ***/

static gboolean 
enum_parse (GtkCssParser *parser,
	    GType         type,
	    int          *res)
{
  char *str;

  if (_gtk_css_parser_try_enum (parser, type, res))
    return TRUE;

  str = _gtk_css_parser_try_ident (parser, TRUE);
  if (str == NULL)
    {
      _gtk_css_parser_error (parser, "Expected an identifier");
      return FALSE;
    }

  _gtk_css_parser_error (parser,
			 "Unknown value '%s' for enum type '%s'",
			 str, g_type_name (type));
  g_free (str);

  return FALSE;
}

static void
enum_print (int         value,
	    GType       type,
	    GString    *string)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  enum_class = g_type_class_ref (type);
  enum_value = g_enum_get_value (enum_class, value);

  g_string_append (string, enum_value->value_nick);

  g_type_class_unref (enum_class);
}

static gboolean
rgba_value_parse (GtkCssParser *parser,
                  GValue       *value)
{
  GtkSymbolicColor *symbolic;
  GdkRGBA rgba;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  symbolic = _gtk_css_symbolic_value_new (parser);
  if (symbolic == NULL)
    return FALSE;

  if (gtk_symbolic_color_resolve (symbolic, NULL, &rgba))
    {
      g_value_set_boxed (value, &rgba);
      gtk_symbolic_color_unref (symbolic);
    }
  else
    {
      g_value_unset (value);
      g_value_init (value, GTK_TYPE_SYMBOLIC_COLOR);
      g_value_take_boxed (value, symbolic);
    }

  G_GNUC_END_IGNORE_DEPRECATIONS;

  return TRUE;
}

static void
rgba_value_print (const GValue *value,
                  GString      *string)
{
  const GdkRGBA *rgba = g_value_get_boxed (value);

  if (rgba == NULL)
    g_string_append (string, "none");
  else
    {
      char *s = gdk_rgba_to_string (rgba);
      g_string_append (string, s);
      g_free (s);
    }
}

static GtkCssValue *
rgba_value_compute (GtkStyleProviderPrivate *provider,
                    GtkCssStyle             *values,
                    GtkCssStyle             *parent_values,
                    GtkCssValue             *specified,
                    GtkCssDependencies      *dependencies)
{
  GdkRGBA white = { 1, 1, 1, 1 };
  const GValue *value;

  value = _gtk_css_typed_value_get (specified);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (G_VALUE_HOLDS (value, GTK_TYPE_SYMBOLIC_COLOR))
    {
      GtkSymbolicColor *symbolic = g_value_get_boxed (value);
      GtkCssValue *val;
      GValue new_value = G_VALUE_INIT;
      GdkRGBA rgba;

      val = _gtk_css_color_value_resolve (_gtk_symbolic_color_get_css_value (symbolic),
                                          provider,
                                          gtk_css_style_get_value (values, GTK_CSS_PROPERTY_COLOR),
                                          GTK_CSS_DEPENDS_ON_COLOR,
                                          dependencies,
                                          NULL);
      if (val != NULL)
        {
          rgba = *_gtk_css_rgba_value_get_rgba (val);
          _gtk_css_value_unref (val);
        }
      else
        rgba = white;

      g_value_init (&new_value, GDK_TYPE_RGBA);
      g_value_set_boxed (&new_value, &rgba);
      return _gtk_css_typed_value_new_take (&new_value);
    }
  else
    return _gtk_css_value_ref (specified);

  G_GNUC_END_IGNORE_DEPRECATIONS;
}

static gboolean
color_value_parse (GtkCssParser *parser,
                   GValue       *value)
{
  GtkSymbolicColor *symbolic;
  GdkRGBA rgba;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  symbolic = _gtk_css_symbolic_value_new (parser);
  if (symbolic == NULL)
    return FALSE;

  if (gtk_symbolic_color_resolve (symbolic, NULL, &rgba))
    {
      GdkColor color;

      color.red = rgba.red * 65535. + 0.5;
      color.green = rgba.green * 65535. + 0.5;
      color.blue = rgba.blue * 65535. + 0.5;

      g_value_set_boxed (value, &color);
      gtk_symbolic_color_unref (symbolic);
    }
  else
    {
      g_value_unset (value);
      g_value_init (value, GTK_TYPE_SYMBOLIC_COLOR);
      g_value_take_boxed (value, symbolic);
    }

  G_GNUC_END_IGNORE_DEPRECATIONS;

  return TRUE;
}

static void
color_value_print (const GValue *value,
                   GString      *string)
{
  const GdkColor *color = g_value_get_boxed (value);

  if (color == NULL)
    g_string_append (string, "none");
  else
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      char *s = gdk_color_to_string (color);
G_GNUC_END_IGNORE_DEPRECATIONS
      g_string_append (string, s);
      g_free (s);
    }
}

static GtkCssValue *
color_value_compute (GtkStyleProviderPrivate *provider,
                     GtkCssStyle             *values,
                     GtkCssStyle             *parent_values,
                     GtkCssValue             *specified,
                     GtkCssDependencies      *dependencies)
{
  GdkColor color = { 0, 65535, 65535, 65535 };
  const GValue *value;

  value = _gtk_css_typed_value_get (specified);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (G_VALUE_HOLDS (value, GTK_TYPE_SYMBOLIC_COLOR))
    {
      GValue new_value = G_VALUE_INIT;
      GtkCssValue *val;

      val = _gtk_css_color_value_resolve (_gtk_symbolic_color_get_css_value (g_value_get_boxed (value)),
                                          provider,
                                          gtk_css_style_get_value (values, GTK_CSS_PROPERTY_COLOR),
                                          GTK_CSS_DEPENDS_ON_COLOR,
                                          dependencies,
                                          NULL);
      if (val != NULL)
        {
          const GdkRGBA *rgba = _gtk_css_rgba_value_get_rgba (val);
          color.red = rgba->red * 65535. + 0.5;
          color.green = rgba->green * 65535. + 0.5;
          color.blue = rgba->blue * 65535. + 0.5;
          _gtk_css_value_unref (val);
        }

      g_value_init (&new_value, GDK_TYPE_COLOR);
      g_value_set_boxed (&new_value, &color);
      return _gtk_css_typed_value_new_take (&new_value);
    }
  else
    return _gtk_css_value_ref (specified);

  G_GNUC_END_IGNORE_DEPRECATIONS;
}

static gboolean
symbolic_color_value_parse (GtkCssParser *parser,
                            GValue       *value)
{
  GtkSymbolicColor *symbolic;

  symbolic = _gtk_css_symbolic_value_new (parser);
  if (symbolic == NULL)
    return FALSE;

  g_value_take_boxed (value, symbolic);
  return TRUE;
}

static void
symbolic_color_value_print (const GValue *value,
                            GString      *string)
{
  GtkSymbolicColor *symbolic = g_value_get_boxed (value);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (symbolic == NULL)
    g_string_append (string, "none");
  else
    {
      char *s = gtk_symbolic_color_to_string (symbolic);
      g_string_append (string, s);
      g_free (s);
    }

  G_GNUC_END_IGNORE_DEPRECATIONS;
}

static gboolean 
font_description_value_parse (GtkCssParser *parser,
                              GValue       *value)
{
  PangoFontDescription *font_desc;
  guint mask;
  char *str;

  str = _gtk_css_parser_read_value (parser);
  if (str == NULL)
    return FALSE;

  font_desc = pango_font_description_from_string (str);
  mask = pango_font_description_get_set_fields (font_desc);
  /* These values are not really correct,
   * but the fields must be set, so we set them to something */
  if ((mask & PANGO_FONT_MASK_FAMILY) == 0)
    pango_font_description_set_family_static (font_desc, "Sans");
  if ((mask & PANGO_FONT_MASK_SIZE) == 0)
    pango_font_description_set_size (font_desc, 10 * PANGO_SCALE);
  g_free (str);
  g_value_take_boxed (value, font_desc);
  return TRUE;
}

static void
font_description_value_print (const GValue *value,
                              GString      *string)
{
  const PangoFontDescription *desc = g_value_get_boxed (value);

  if (desc == NULL)
    g_string_append (string, "none");
  else
    {
      char *s = pango_font_description_to_string (desc);
      g_string_append (string, s);
      g_free (s);
    }
}

static gboolean 
boolean_value_parse (GtkCssParser *parser,
                     GValue       *value)
{
  if (_gtk_css_parser_try (parser, "true", TRUE) ||
      _gtk_css_parser_try (parser, "1", TRUE))
    {
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  else if (_gtk_css_parser_try (parser, "false", TRUE) ||
           _gtk_css_parser_try (parser, "0", TRUE))
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }
  else
    {
      _gtk_css_parser_error (parser, "Expected a boolean value");
      return FALSE;
    }
}

static void
boolean_value_print (const GValue *value,
                     GString      *string)
{
  if (g_value_get_boolean (value))
    g_string_append (string, "true");
  else
    g_string_append (string, "false");
}

static gboolean 
int_value_parse (GtkCssParser *parser,
                 GValue       *value)
{
  gint i;

  if (_gtk_css_parser_begins_with (parser, '-'))
    {
      int res = _gtk_win32_theme_int_parse (parser, &i);
      if (res >= 0)
	{
	  g_value_set_int (value, i);
	  return res > 0;
	}
      /* < 0 => continue */
    }

  if (!_gtk_css_parser_try_int (parser, &i))
    {
      _gtk_css_parser_error (parser, "Expected a valid integer value");
      return FALSE;
    }

  g_value_set_int (value, i);
  return TRUE;
}

static void
int_value_print (const GValue *value,
                 GString      *string)
{
  g_string_append_printf (string, "%d", g_value_get_int (value));
}

static gboolean 
uint_value_parse (GtkCssParser *parser,
                  GValue       *value)
{
  guint u;

  if (!_gtk_css_parser_try_uint (parser, &u))
    {
      _gtk_css_parser_error (parser, "Expected a valid unsigned value");
      return FALSE;
    }

  g_value_set_uint (value, u);
  return TRUE;
}

static void
uint_value_print (const GValue *value,
                  GString      *string)
{
  g_string_append_printf (string, "%u", g_value_get_uint (value));
}

static gboolean 
double_value_parse (GtkCssParser *parser,
                    GValue       *value)
{
  gdouble d;

  if (!_gtk_css_parser_try_double (parser, &d))
    {
      _gtk_css_parser_error (parser, "Expected a number");
      return FALSE;
    }

  g_value_set_double (value, d);
  return TRUE;
}

static void
double_value_print (const GValue *value,
                    GString      *string)
{
  string_append_double (string, g_value_get_double (value));
}

static gboolean 
float_value_parse (GtkCssParser *parser,
                   GValue       *value)
{
  gdouble d;

  if (!_gtk_css_parser_try_double (parser, &d))
    {
      _gtk_css_parser_error (parser, "Expected a number");
      return FALSE;
    }

  g_value_set_float (value, d);
  return TRUE;
}

static void
float_value_print (const GValue *value,
                   GString      *string)
{
  string_append_double (string, g_value_get_float (value));
}

static gboolean 
string_value_parse (GtkCssParser *parser,
                    GValue       *value)
{
  char *str = _gtk_css_parser_read_string (parser);

  if (str == NULL)
    return FALSE;

  g_value_take_string (value, str);
  return TRUE;
}

static void
string_value_print (const GValue *value,
                    GString      *str)
{
  string_append_string (str, g_value_get_string (value));
}

static gboolean 
theming_engine_value_parse (GtkCssParser *parser,
                            GValue       *value)
{
  GtkThemingEngine *engine;
  char *str;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  if (_gtk_css_parser_try (parser, "none", TRUE))
    {
      g_value_set_object (value, gtk_theming_engine_load (NULL));
      return TRUE;
    }

  str = _gtk_css_parser_try_ident (parser, TRUE);
  if (str == NULL)
    {
      _gtk_css_parser_error (parser, "Expected a valid theme name");
      return FALSE;
    }

  engine = gtk_theming_engine_load (str);

  if (engine == NULL)
    {
      _gtk_css_parser_error (parser, "Theming engine '%s' not found", str);
      g_free (str);
      return FALSE;
    }

  g_value_set_object (value, engine);
  g_free (str);
  return TRUE;

G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
theming_engine_value_print (const GValue *value,
                            GString      *string)
{
  GtkThemingEngine *engine;
  char *name;

  engine = g_value_get_object (value);
  if (engine == NULL)
    g_string_append (string, "none");
  else
    {
      /* XXX: gtk_theming_engine_get_name()? */
      g_object_get (engine, "name", &name, NULL);
      g_string_append (string, name ? name : "none");
      g_free (name);
    }
}

static gboolean 
border_value_parse (GtkCssParser *parser,
                    GValue       *value)
{
  GtkBorder border = { 0, };
  guint i;
  int numbers[4];

  for (i = 0; i < G_N_ELEMENTS (numbers); i++)
    {
      if (_gtk_css_parser_begins_with (parser, '-'))
	{
	  /* These are strictly speaking signed, but we want to be able to use them
	     for unsigned types too, as the actual ranges of values make this safe */
	  int res = _gtk_win32_theme_int_parse (parser, &numbers[i]);

	  if (res == 0) /* Parse error, report */
	    return FALSE;

	  if (res < 0) /* Nothing known to expand */
	    break;
	}
      else
        {
          if (!_gtk_css_parser_try_length (parser, &numbers[i]))
            break;
        }
    }

  if (i == 0)
    {
      _gtk_css_parser_error (parser, "Expected valid border");
      return FALSE;
    }

  border.top = numbers[0];
  if (i > 1)
    border.right = numbers[1];
  else
    border.right = border.top;
  if (i > 2)
    border.bottom = numbers[2];
  else
    border.bottom = border.top;
  if (i > 3)
    border.left = numbers[3];
  else
    border.left = border.right;

  g_value_set_boxed (value, &border);
  return TRUE;
}

static void
border_value_print (const GValue *value, GString *string)
{
  const GtkBorder *border = g_value_get_boxed (value);

  if (border == NULL)
    g_string_append (string, "none");
  else if (border->left != border->right)
    g_string_append_printf (string, "%d %d %d %d", border->top, border->right, border->bottom, border->left);
  else if (border->top != border->bottom)
    g_string_append_printf (string, "%d %d %d", border->top, border->right, border->bottom);
  else if (border->top != border->left)
    g_string_append_printf (string, "%d %d", border->top, border->right);
  else
    g_string_append_printf (string, "%d", border->top);
}

static gboolean 
gradient_value_parse (GtkCssParser *parser,
                      GValue       *value)
{
  GtkGradient *gradient;

  gradient = _gtk_gradient_parse (parser);
  if (gradient == NULL)
    return FALSE;

  g_value_take_boxed (value, gradient);
  return TRUE;
}

static void
gradient_value_print (const GValue *value,
                      GString      *string)
{
  GtkGradient *gradient = g_value_get_boxed (value);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (gradient == NULL)
    g_string_append (string, "none");
  else
    {
      char *s = gtk_gradient_to_string (gradient);
      g_string_append (string, s);
      g_free (s);
    }

  G_GNUC_END_IGNORE_DEPRECATIONS;
}

static gboolean 
pattern_value_parse (GtkCssParser *parser,
                     GValue       *value)
{
  if (_gtk_css_parser_try (parser, "none", TRUE))
    {
      /* nothing to do here */
    }
  else if (_gtk_css_parser_begins_with (parser, '-'))
    {
      g_value_unset (value);

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

      g_value_init (value, GTK_TYPE_GRADIENT);
      return gradient_value_parse (parser, value);

      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
  else
    {
      GError *error = NULL;
      gchar *path;
      GdkPixbuf *pixbuf;
      GFile *file;
      cairo_surface_t *surface;
      cairo_pattern_t *pattern;
      cairo_matrix_t matrix;

      file = _gtk_css_parser_read_url (parser);
      if (file == NULL)
        return FALSE;

      path = g_file_get_path (file);
      g_object_unref (file);

      pixbuf = gdk_pixbuf_new_from_file (path, &error);
      g_free (path);
      if (pixbuf == NULL)
        {
          _gtk_css_parser_take_error (parser, error);
          return FALSE;
        }

      surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, 1, NULL);
      pattern = cairo_pattern_create_for_surface (surface);
      cairo_surface_destroy (surface);

      cairo_matrix_init_scale (&matrix,
                               gdk_pixbuf_get_width (pixbuf),
                               gdk_pixbuf_get_height (pixbuf));
      cairo_pattern_set_matrix (pattern, &matrix);

      g_object_unref (pixbuf);

      g_value_take_boxed (value, pattern);
    }
  
  return TRUE;
}

static cairo_status_t
surface_write (void                *closure,
               const unsigned char *data,
               unsigned int         length)
{
  g_byte_array_append (closure, data, length);

  return CAIRO_STATUS_SUCCESS;
}

static void
surface_print (cairo_surface_t *surface,
               GString *        string)
{
#if CAIRO_HAS_PNG_FUNCTIONS
  GByteArray *array;
  char *base64;
  
  array = g_byte_array_new ();
  cairo_surface_write_to_png_stream (surface, surface_write, array);
  base64 = g_base64_encode (array->data, array->len);
  g_byte_array_free (array, TRUE);

  g_string_append (string, "url(\"data:image/png;base64,");
  g_string_append (string, base64);
  g_string_append (string, "\")");

  g_free (base64);
#else
  g_string_append (string, "none /* you need cairo png functions enabled to make this work */");
#endif
}

static void
pattern_value_print (const GValue *value,
                     GString      *string)
{
  cairo_pattern_t *pattern;
  cairo_surface_t *surface;

  pattern = g_value_get_boxed (value);

  if (pattern == NULL)
    {
      g_string_append (string, "none");
      return;
    }

  switch (cairo_pattern_get_type (pattern))
    {
    case CAIRO_PATTERN_TYPE_SURFACE:
      if (cairo_pattern_get_surface (pattern, &surface) != CAIRO_STATUS_SUCCESS)
        {
          g_assert_not_reached ();
        }
      surface_print (surface, string);
      break;
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
      g_string_append (string, "none /* FIXME: add support for printing gradients */");
      break;
    case CAIRO_PATTERN_TYPE_SOLID:
    default:
      g_assert_not_reached ();
      break;
    }
}

static GtkCssValue *
pattern_value_compute (GtkStyleProviderPrivate *provider,
                       GtkCssStyle             *values,
                       GtkCssStyle             *parent_values,
                       GtkCssValue             *specified,
                       GtkCssDependencies      *dependencies)
{
  const GValue *value = _gtk_css_typed_value_get (specified);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (G_VALUE_HOLDS (value, GTK_TYPE_GRADIENT))
    {
      GValue new_value = G_VALUE_INIT;
      cairo_pattern_t *gradient;

      gradient = _gtk_gradient_resolve_full (g_value_get_boxed (value), provider, values, parent_values, dependencies);

      g_value_init (&new_value, CAIRO_GOBJECT_TYPE_PATTERN);
      g_value_take_boxed (&new_value, gradient);
      return _gtk_css_typed_value_new_take (&new_value);
    }
  else
    return _gtk_css_value_ref (specified);

  G_GNUC_END_IGNORE_DEPRECATIONS;
}

static gboolean 
enum_value_parse (GtkCssParser *parser,
                  GValue       *value)
{
  int v;

  if (enum_parse (parser, G_VALUE_TYPE (value), &v))
    {
      g_value_set_enum (value, v);
      return TRUE;
    }

  return FALSE;
}

static void
enum_value_print (const GValue *value,
                  GString      *string)
{
  enum_print (g_value_get_enum (value), G_VALUE_TYPE (value), string);
}

static gboolean 
flags_value_parse (GtkCssParser *parser,
                   GValue       *value)
{
  GFlagsClass *flags_class;
  GFlagsValue *flag_value;
  guint flags = 0;
  char *str;

  flags_class = g_type_class_ref (G_VALUE_TYPE (value));

  do {
    str = _gtk_css_parser_try_ident (parser, TRUE);
    if (str == NULL)
      {
        _gtk_css_parser_error (parser, "Expected an identifier");
        g_type_class_unref (flags_class);
        return FALSE;
      }

      flag_value = g_flags_get_value_by_nick (flags_class, str);
      if (!flag_value)
        {
          _gtk_css_parser_error (parser,
                                 "Unknown flag value '%s' for type '%s'",
                                 str, g_type_name (G_VALUE_TYPE (value)));
          /* XXX Do we want to return FALSE here? We can get
           * forward-compatibility for new values this way
           */
          g_free (str);
          g_type_class_unref (flags_class);
          return FALSE;
        }

      g_free (str);
    }
  while (_gtk_css_parser_try (parser, ",", FALSE));

  g_type_class_unref (flags_class);

  g_value_set_enum (value, flags);

  return TRUE;
}

static void
flags_value_print (const GValue *value,
                   GString      *string)
{
  GFlagsClass *flags_class;
  guint i, flags;

  flags_class = g_type_class_ref (G_VALUE_TYPE (value));
  flags = g_value_get_flags (value);

  for (i = 0; i < flags_class->n_values; i++)
    {
      GFlagsValue *flags_value = &flags_class->values[i];

      if (flags & flags_value->value)
        {
          if (string->len != 0)
            g_string_append (string, ", ");

          g_string_append (string, flags_value->value_nick);
        }
    }

  g_type_class_unref (flags_class);
}

/*** API ***/

static void
gtk_css_style_funcs_init (void)
{
  if (G_LIKELY (parse_funcs != NULL))
    return;

  parse_funcs = g_hash_table_new (NULL, NULL);
  print_funcs = g_hash_table_new (NULL, NULL);
  compute_funcs = g_hash_table_new (NULL, NULL);

  register_conversion_function (GDK_TYPE_RGBA,
                                rgba_value_parse,
                                rgba_value_print,
                                rgba_value_compute);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  register_conversion_function (GDK_TYPE_COLOR,
                                color_value_parse,
                                color_value_print,
                                color_value_compute);

  register_conversion_function (GTK_TYPE_SYMBOLIC_COLOR,
                                symbolic_color_value_parse,
                                symbolic_color_value_print,
                                NULL);

  G_GNUC_END_IGNORE_DEPRECATIONS;

  register_conversion_function (PANGO_TYPE_FONT_DESCRIPTION,
                                font_description_value_parse,
                                font_description_value_print,
                                NULL);
  register_conversion_function (G_TYPE_BOOLEAN,
                                boolean_value_parse,
                                boolean_value_print,
                                NULL);
  register_conversion_function (G_TYPE_INT,
                                int_value_parse,
                                int_value_print,
                                NULL);
  register_conversion_function (G_TYPE_UINT,
                                uint_value_parse,
                                uint_value_print,
                                NULL);
  register_conversion_function (G_TYPE_DOUBLE,
                                double_value_parse,
                                double_value_print,
                                NULL);
  register_conversion_function (G_TYPE_FLOAT,
                                float_value_parse,
                                float_value_print,
                                NULL);
  register_conversion_function (G_TYPE_STRING,
                                string_value_parse,
                                string_value_print,
                                NULL);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  register_conversion_function (GTK_TYPE_THEMING_ENGINE,
                                theming_engine_value_parse,
                                theming_engine_value_print,
                                NULL);

  G_GNUC_END_IGNORE_DEPRECATIONS

  register_conversion_function (GTK_TYPE_BORDER,
                                border_value_parse,
                                border_value_print,
                                NULL);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  register_conversion_function (GTK_TYPE_GRADIENT,
                                gradient_value_parse,
                                gradient_value_print,
                                NULL);

  G_GNUC_END_IGNORE_DEPRECATIONS;

  register_conversion_function (CAIRO_GOBJECT_TYPE_PATTERN,
                                pattern_value_parse,
                                pattern_value_print,
                                pattern_value_compute);
  register_conversion_function (G_TYPE_ENUM,
                                enum_value_parse,
                                enum_value_print,
                                NULL);
  register_conversion_function (G_TYPE_FLAGS,
                                flags_value_parse,
                                flags_value_print,
                                NULL);
}

/**
 * _gtk_css_style_parse_value:
 * @value: the value to parse into. Must be a valid initialized #GValue
 * @parser: the parser to parse from
 *
 * This is the generic parsing function used for CSS values. If the
 * function fails to parse a value, it will emit an error on @parser,
 * return %FALSE and not touch @value.
 *
 * Returns: %TRUE if parsing succeeded.
 **/
gboolean
_gtk_css_style_funcs_parse_value (GValue       *value,
                                  GtkCssParser *parser)
{
  GtkStyleParseFunc func;

  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (parser != NULL, FALSE);

  gtk_css_style_funcs_init ();

  func = g_hash_table_lookup (parse_funcs,
                              GSIZE_TO_POINTER (G_VALUE_TYPE (value)));
  if (func == NULL)
    func = g_hash_table_lookup (parse_funcs,
                                GSIZE_TO_POINTER (g_type_fundamental (G_VALUE_TYPE (value))));

  if (func == NULL)
    {
      _gtk_css_parser_error (parser,
                             "Cannot convert to type '%s'",
                             g_type_name (G_VALUE_TYPE (value)));
      return FALSE;
    }

  return (*func) (parser, value);
}

/**
 * _gtk_css_style_print_value:
 * @value: an initialized GValue returned from _gtk_css_style_parse()
 * @string: the string to print into
 *
 * Prints @value into @string as a CSS value. If @value is not a
 * valid value, a random string will be printed instead.
 **/
void
_gtk_css_style_funcs_print_value (const GValue *value,
                                  GString      *string)
{
  GtkStylePrintFunc func;

  gtk_css_style_funcs_init ();

  func = g_hash_table_lookup (print_funcs,
                              GSIZE_TO_POINTER (G_VALUE_TYPE (value)));
  if (func == NULL)
    func = g_hash_table_lookup (print_funcs,
                                GSIZE_TO_POINTER (g_type_fundamental (G_VALUE_TYPE (value))));

  if (func == NULL)
    {
      char *s = g_strdup_value_contents (value);
      g_string_append (string, s);
      g_free (s);
      return;
    }
  
  func (value, string);
}

/**
 * _gtk_css_style_compute_value:
 * @provider: Style provider to look up information from
 * @values: The values to compute for
 * @parent_values: Values to look up inherited values from
 * @target_type: Type the resulting value should have
 * @specified: the value to use for the computation
 * @dependencies: (out): Value initialized with 0 to take the dependencies
 *     of the returned value
 *
 * Converts the @specified value into the @computed value using the
 * information in @context. The values must have matching types, ie
 * @specified must be a result of a call to
 * _gtk_css_style_parse_value() with the same type as @computed.
 *
 * Returns: the resulting value
 **/
GtkCssValue *
_gtk_css_style_funcs_compute_value (GtkStyleProviderPrivate *provider,
                                    GtkCssStyle             *style,
                                    GtkCssStyle             *parent_style,
                                    GType                    target_type,
                                    GtkCssValue             *specified,
                                    GtkCssDependencies      *dependencies)
{
  GtkStyleComputeFunc func;

  g_return_val_if_fail (GTK_IS_STYLE_PROVIDER (provider), NULL);
  g_return_val_if_fail (GTK_IS_CSS_STYLE (style), NULL);
  g_return_val_if_fail (parent_style == NULL || GTK_IS_CSS_STYLE (parent_style), NULL);
  g_return_val_if_fail (*dependencies == 0, NULL);

  gtk_css_style_funcs_init ();

  func = g_hash_table_lookup (compute_funcs,
                              GSIZE_TO_POINTER (target_type));
  if (func == NULL)
    func = g_hash_table_lookup (compute_funcs,
                                GSIZE_TO_POINTER (g_type_fundamental (target_type)));

  if (func)
    return func (provider, style, parent_style, specified, dependencies);
  else
    return _gtk_css_value_ref (specified);
}

