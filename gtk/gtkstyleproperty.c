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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gtkstylepropertyprivate.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo-gobject.h>

#include "gtkcssprovider.h"
#include "gtkcssparserprivate.h"
#include "gtkcsstypesprivate.h"

/* the actual parsers we have */
#include "gtkanimationdescription.h"
#include "gtkbindings.h"
#include "gtkborderimageprivate.h"
#include "gtkgradient.h"
#include "gtkshadowprivate.h"
#include "gtkthemingengine.h"
#include "gtktypebuiltins.h"

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

static GHashTable *parse_funcs = NULL;
static GHashTable *print_funcs = NULL;
static GHashTable *properties = NULL;

static void
register_conversion_function (GType             type,
                              GtkStyleParseFunc parse,
                              GtkStylePrintFunc print)
{
  if (parse)
    g_hash_table_insert (parse_funcs, GSIZE_TO_POINTER (type), parse);
  if (print)
    g_hash_table_insert (print_funcs, GSIZE_TO_POINTER (type), print);
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
  gsize len;

  g_string_append_c (str, '"');

  do {
    len = strcspn (string, "\"\n\r\f");
    g_string_append (str, string);
    string += len;
    switch (*string)
      {
      case '\0':
        break;
      case '\n':
        g_string_append (str, "\\A ");
        break;
      case '\r':
        g_string_append (str, "\\D ");
        break;
      case '\f':
        g_string_append (str, "\\C ");
        break;
      case '\"':
        g_string_append (str, "\\\"");
        break;
      default:
        g_assert_not_reached ();
        break;
      }
  } while (*string);

  g_string_append_c (str, '"');
}

/*** IMPLEMENTATIONS ***/

static gboolean 
rgba_value_parse (GtkCssParser *parser,
                  GFile        *base,
                  GValue       *value)
{
  GtkSymbolicColor *symbolic;
  GdkRGBA rgba;

  symbolic = _gtk_css_parser_read_symbolic_color (parser);
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

static gboolean 
color_value_parse (GtkCssParser *parser,
                   GFile        *base,
                   GValue       *value)
{
  GtkSymbolicColor *symbolic;
  GdkRGBA rgba;

  symbolic = _gtk_css_parser_read_symbolic_color (parser);
  if (symbolic == NULL)
    return FALSE;

  if (gtk_symbolic_color_resolve (symbolic, NULL, &rgba))
    {
      GdkColor color;

      color.red = rgba.red * 65535. + 0.5;
      color.green = rgba.green * 65535. + 0.5;
      color.blue = rgba.blue * 65535. + 0.5;

      g_value_set_boxed (value, &color);
    }
  else
    {
      g_value_unset (value);
      g_value_init (value, GTK_TYPE_SYMBOLIC_COLOR);
      g_value_take_boxed (value, symbolic);
    }

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
      char *s = gdk_color_to_string (color);
      g_string_append (string, s);
      g_free (s);
    }
}

static gboolean
symbolic_color_value_parse (GtkCssParser *parser,
                            GFile        *base,
                            GValue       *value)
{
  GtkSymbolicColor *symbolic;

  symbolic = _gtk_css_parser_read_symbolic_color (parser);
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

  if (symbolic == NULL)
    g_string_append (string, "none");
  else
    {
      char *s = gtk_symbolic_color_to_string (symbolic);
      g_string_append (string, s);
      g_free (s);
    }
}

static gboolean
font_family_parse (GtkCssParser *parser,
                   GFile        *base,
                   GValue       *value)
{
  GPtrArray *names;
  char *name;

  /* We don't special case generic families. Pango should do
   * that for us */

  names = g_ptr_array_new ();

  do {
    name = _gtk_css_parser_try_ident (parser, TRUE);
    if (name)
      {
        GString *string = g_string_new (name);
        g_free (name);
        while ((name = _gtk_css_parser_try_ident (parser, TRUE)))
          {
            g_string_append_c (string, ' ');
            g_string_append (string, name);
            g_free (name);
          }
        name = g_string_free (string, FALSE);
      }
    else 
      {
        name = _gtk_css_parser_read_string (parser);
        if (name == NULL)
          {
            g_ptr_array_free (names, TRUE);
            return FALSE;
          }
      }

    g_ptr_array_add (names, name);
  } while (_gtk_css_parser_try (parser, ",", TRUE));

  /* NULL-terminate array */
  g_ptr_array_add (names, NULL);
  g_value_set_boxed (value, g_ptr_array_free (names, FALSE));
  return TRUE;
}

static void
font_family_value_print (const GValue *value,
                         GString      *string)
{
  const char **names = g_value_get_boxed (value);

  if (names == NULL || *names == NULL)
    {
      g_string_append (string, "none");
      return;
    }

  string_append_string (string, *names);
  names++;
  while (*names)
    {
      g_string_append (string, ", ");
      string_append_string (string, *names);
      names++;
    }
}

static gboolean 
font_description_value_parse (GtkCssParser *parser,
                              GFile        *base,
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
                     GFile        *base,
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
                 GFile        *base,
                 GValue       *value)
{
  gint i;

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
                  GFile        *base,
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
                    GFile        *base,
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
                   GFile        *base,
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
                    GFile        *base,
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
                            GFile        *base,
                            GValue       *value)
{
  GtkThemingEngine *engine;
  char *str;

  str = _gtk_css_parser_try_ident (parser, TRUE);
  if (str == NULL)
    {
      _gtk_css_parser_error (parser, "Expected a valid theme name");
      return FALSE;
    }

  engine = gtk_theming_engine_load (str);
  if (engine == NULL)
    {
      _gtk_css_parser_error (parser, "Themeing engine '%s' not found", str);
      g_free (str);
      return FALSE;
    }

  g_value_set_object (value, engine);
  g_free (str);
  return TRUE;
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
animation_description_value_parse (GtkCssParser *parser,
                                   GFile        *base,
                                   GValue       *value)
{
  GtkAnimationDescription *desc;
  char *str;

  str = _gtk_css_parser_read_value (parser);
  if (str == NULL)
    return FALSE;

  desc = _gtk_animation_description_from_string (str);
  g_free (str);

  if (desc == NULL)
    {
      _gtk_css_parser_error (parser, "Invalid animation description");
      return FALSE;
    }
  
  g_value_take_boxed (value, desc);
  return TRUE;
}

static void
animation_description_value_print (const GValue *value,
                                   GString      *string)
{
  GtkAnimationDescription *desc = g_value_get_boxed (value);

  if (desc == NULL)
    g_string_append (string, "none");
  else
    _gtk_animation_description_print (desc, string);
}

static gboolean 
border_value_parse (GtkCssParser *parser,
                    GFile        *base,
                    GValue       *value)
{
  GtkBorder border = { 0, };
  guint i, numbers[4];

  for (i = 0; i < G_N_ELEMENTS (numbers); i++)
    {
      if (!_gtk_css_parser_try_uint (parser, &numbers[i]))
        break;

      /* XXX: shouldn't allow spaces here? */
      _gtk_css_parser_try (parser, "px", TRUE);
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
                      GFile        *base,
                      GValue       *value)
{
  GtkGradient *gradient;
  cairo_pattern_type_t type;
  gdouble coords[6];
  guint i;

  if (!_gtk_css_parser_try (parser, "-gtk-gradient", TRUE))
    {
      _gtk_css_parser_error (parser,
                             "Expected '-gtk-gradient'");
      return FALSE;
    }

  if (!_gtk_css_parser_try (parser, "(", TRUE))
    {
      _gtk_css_parser_error (parser,
                             "Expected '(' after '-gtk-gradient'");
      return FALSE;
    }

  /* Parse gradient type */
  if (_gtk_css_parser_try (parser, "linear", TRUE))
    type = CAIRO_PATTERN_TYPE_LINEAR;
  else if (_gtk_css_parser_try (parser, "radial", TRUE))
    type = CAIRO_PATTERN_TYPE_RADIAL;
  else
    {
      _gtk_css_parser_error (parser,
                             "Gradient type must be 'radial' or 'linear'");
      return FALSE;
    }

  /* Parse start/stop position parameters */
  for (i = 0; i < 2; i++)
    {
      if (! _gtk_css_parser_try (parser, ",", TRUE))
        {
          _gtk_css_parser_error (parser,
                                 "Expected ','");
          return FALSE;
        }

      if (_gtk_css_parser_try (parser, "left", TRUE))
        coords[i * 3] = 0;
      else if (_gtk_css_parser_try (parser, "right", TRUE))
        coords[i * 3] = 1;
      else if (_gtk_css_parser_try (parser, "center", TRUE))
        coords[i * 3] = 0.5;
      else if (!_gtk_css_parser_try_double (parser, &coords[i * 3]))
        {
          _gtk_css_parser_error (parser,
                                 "Expected a valid X coordinate");
          return FALSE;
        }

      if (_gtk_css_parser_try (parser, "top", TRUE))
        coords[i * 3 + 1] = 0;
      else if (_gtk_css_parser_try (parser, "bottom", TRUE))
        coords[i * 3 + 1] = 1;
      else if (_gtk_css_parser_try (parser, "center", TRUE))
        coords[i * 3 + 1] = 0.5;
      else if (!_gtk_css_parser_try_double (parser, &coords[i * 3 + 1]))
        {
          _gtk_css_parser_error (parser,
                                 "Expected a valid Y coordinate");
          return FALSE;
        }

      if (type == CAIRO_PATTERN_TYPE_RADIAL)
        {
          /* Parse radius */
          if (! _gtk_css_parser_try (parser, ",", TRUE))
            {
              _gtk_css_parser_error (parser,
                                     "Expected ','");
              return FALSE;
            }

          if (! _gtk_css_parser_try_double (parser, &coords[(i * 3) + 2]))
            {
              _gtk_css_parser_error (parser,
                                     "Expected a numer for the radius");
              return FALSE;
            }
        }
    }

  if (type == CAIRO_PATTERN_TYPE_LINEAR)
    gradient = gtk_gradient_new_linear (coords[0], coords[1], coords[3], coords[4]);
  else
    gradient = gtk_gradient_new_radial (coords[0], coords[1], coords[2],
                                        coords[3], coords[4], coords[5]);

  while (_gtk_css_parser_try (parser, ",", TRUE))
    {
      GtkSymbolicColor *color;
      gdouble position;

      if (_gtk_css_parser_try (parser, "from", TRUE))
        {
          position = 0;

          if (!_gtk_css_parser_try (parser, "(", TRUE))
            {
              gtk_gradient_unref (gradient);
              _gtk_css_parser_error (parser,
                                     "Expected '('");
              return FALSE;
            }

        }
      else if (_gtk_css_parser_try (parser, "to", TRUE))
        {
          position = 1;

          if (!_gtk_css_parser_try (parser, "(", TRUE))
            {
              gtk_gradient_unref (gradient);
              _gtk_css_parser_error (parser,
                                     "Expected '('");
              return FALSE;
            }

        }
      else if (_gtk_css_parser_try (parser, "color-stop", TRUE))
        {
          if (!_gtk_css_parser_try (parser, "(", TRUE))
            {
              gtk_gradient_unref (gradient);
              _gtk_css_parser_error (parser,
                                     "Expected '('");
              return FALSE;
            }

          if (!_gtk_css_parser_try_double (parser, &position))
            {
              gtk_gradient_unref (gradient);
              _gtk_css_parser_error (parser,
                                     "Expected a valid number");
              return FALSE;
            }

          if (!_gtk_css_parser_try (parser, ",", TRUE))
            {
              gtk_gradient_unref (gradient);
              _gtk_css_parser_error (parser,
                                     "Expected a comma");
              return FALSE;
            }
        }
      else
        {
          gtk_gradient_unref (gradient);
          _gtk_css_parser_error (parser,
                                 "Not a valid color-stop definition");
          return FALSE;
        }

      color = _gtk_css_parser_read_symbolic_color (parser);
      if (color == NULL)
        {
          gtk_gradient_unref (gradient);
          return FALSE;
        }

      gtk_gradient_add_color_stop (gradient, position, color);
      gtk_symbolic_color_unref (color);

      if (!_gtk_css_parser_try (parser, ")", TRUE))
        {
          gtk_gradient_unref (gradient);
          _gtk_css_parser_error (parser,
                                 "Expected ')'");
          return FALSE;
        }
    }

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      gtk_gradient_unref (gradient);
      _gtk_css_parser_error (parser,
                             "Expected ')'");
      return FALSE;
    }

  g_value_take_boxed (value, gradient);
  return TRUE;
}

static void
gradient_value_print (const GValue *value,
                      GString      *string)
{
  GtkGradient *gradient = g_value_get_boxed (value);

  if (gradient == NULL)
    g_string_append (string, "none");
  else
    {
      char *s = gtk_gradient_to_string (gradient);
      g_string_append (string, s);
      g_free (s);
    }
}

static GFile *
gtk_css_parse_url (GtkCssParser *parser,
                   GFile        *base)
{
  gchar *path;
  GFile *file;

  if (_gtk_css_parser_try (parser, "url", FALSE))
    {
      if (!_gtk_css_parser_try (parser, "(", TRUE))
        {
          _gtk_css_parser_skip_whitespace (parser);
          if (_gtk_css_parser_try (parser, "(", TRUE))
            {
              GError *error;
              
              error = g_error_new_literal (GTK_CSS_PROVIDER_ERROR,
                                           GTK_CSS_PROVIDER_ERROR_DEPRECATED,
                                           "Whitespace between 'url' and '(' is deprecated");
                             
              _gtk_css_parser_take_error (parser, error);
            }
          else
            {
              _gtk_css_parser_error (parser, "Expected '(' after 'url'");
              return NULL;
            }
        }

      path = _gtk_css_parser_read_string (parser);
      if (path == NULL)
        return NULL;

      if (!_gtk_css_parser_try (parser, ")", TRUE))
        {
          _gtk_css_parser_error (parser, "No closing ')' found for 'url'");
          g_free (path);
          return NULL;
        }
    }
  else
    {
      path = _gtk_css_parser_try_name (parser, TRUE);
      if (path == NULL)
        {
          _gtk_css_parser_error (parser, "Not a valid url");
          return NULL;
        }
    }

  file = g_file_resolve_relative_path (base, path);
  g_free (path);

  return file;
}

static gboolean 
pattern_value_parse (GtkCssParser *parser,
                     GFile        *base,
                     GValue       *value)
{
  if (_gtk_css_parser_begins_with (parser, '-'))
    {
      g_value_unset (value);
      g_value_init (value, GTK_TYPE_GRADIENT);
      return gradient_value_parse (parser, base, value);
    }
  else
    {
      GError *error = NULL;
      gchar *path;
      GdkPixbuf *pixbuf;
      GFile *file;
      cairo_surface_t *surface;
      cairo_pattern_t *pattern;
      cairo_t *cr;
      cairo_matrix_t matrix;

      file = gtk_css_parse_url (parser, base);
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

      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                            gdk_pixbuf_get_width (pixbuf),
                                            gdk_pixbuf_get_height (pixbuf));
      cr = cairo_create (surface);
      gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
      cairo_paint (cr);
      pattern = cairo_pattern_create_for_surface (surface);

      cairo_matrix_init_scale (&matrix,
                               gdk_pixbuf_get_width (pixbuf),
                               gdk_pixbuf_get_height (pixbuf));
      cairo_pattern_set_matrix (pattern, &matrix);

      cairo_surface_destroy (surface);
      cairo_destroy (cr);
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
    case CAIRO_PATTERN_TYPE_SOLID:
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
    default:
      g_assert_not_reached ();
      break;
    }
}

static gboolean
shadow_value_parse (GtkCssParser *parser,
                    GFile *base,
                    GValue *value)
{
  gboolean have_inset, have_color, have_lengths;
  gdouble hoffset, voffset, blur, spread;
  GtkSymbolicColor *color;
  GtkShadow *shadow;
  guint i;

  shadow = _gtk_shadow_new ();

  do
    {
      have_inset = have_lengths = have_color = FALSE;

      for (i = 0; i < 3; i++)
        {
          if (!have_inset && 
              _gtk_css_parser_try (parser, "inset", TRUE))
            {
              have_inset = TRUE;
              continue;
            }
            
          if (!have_lengths &&
              _gtk_css_parser_try_double (parser, &hoffset))
            {
              have_lengths = TRUE;

              if (!_gtk_css_parser_try_double (parser, &voffset))
                {
                  _gtk_css_parser_error (parser, "Horizontal and vertical offsets are required");
                  _gtk_shadow_unref (shadow);
                  return FALSE;
                }

              if (!_gtk_css_parser_try_double (parser, &blur))
                blur = 0;

              if (!_gtk_css_parser_try_double (parser, &spread))
                spread = 0;

              continue;
            }

          if (!have_color)
            {
              have_color = TRUE;

              /* XXX: the color is optional and UA-defined if it's missing,
               * but it doesn't really make sense for us...
               */
              color = _gtk_css_parser_read_symbolic_color (parser);

              if (color == NULL)
                {
                  _gtk_shadow_unref (shadow);
                  return FALSE;
                }
            }
        }

      if (!have_color || !have_lengths)
        {
          _gtk_css_parser_error (parser, "Must specify at least color and offsets");
          _gtk_shadow_unref (shadow);
          return FALSE;
        }

      _gtk_shadow_append (shadow,
                          hoffset, voffset,
                          blur, spread,
                          have_inset, color);

      gtk_symbolic_color_unref (color);

    }
  while (_gtk_css_parser_try (parser, ",", TRUE));

  g_value_take_boxed (value, shadow);
  return TRUE;
}

static void
shadow_value_print (const GValue *value,
                    GString      *string)
{
  GtkShadow *shadow;

  shadow = g_value_get_boxed (value);

  if (shadow == NULL)
    g_string_append (string, "none");
  else
    _gtk_shadow_print (shadow, string);
}

static gboolean
border_image_repeat_value_parse (GtkCssParser *parser,
                                 GFile *file,
                                 GValue *value)
{
  GtkCssBorderImageRepeat image_repeat;
  GtkCssRepeatStyle styles[2];
  gint i;

  for (i = 0; i < 2; i++)
    {
      if (_gtk_css_parser_try (parser, "stretch", TRUE))
        styles[i] = GTK_CSS_REPEAT_STYLE_NONE;
      else if (_gtk_css_parser_try (parser, "repeat", TRUE))
        styles[i] = GTK_CSS_REPEAT_STYLE_REPEAT;
      else if (_gtk_css_parser_try (parser, "round", TRUE))
        styles[i] = GTK_CSS_REPEAT_STYLE_ROUND;
      else if (_gtk_css_parser_try (parser, "space", TRUE))
        styles[i] = GTK_CSS_REPEAT_STYLE_SPACE;
      else if (i == 0)
        {
          styles[1] = styles[0] = GTK_CSS_REPEAT_STYLE_NONE;
          break;
        }
      else
        styles[i] = styles[0];
    }

  image_repeat.hrepeat = styles[0];
  image_repeat.vrepeat = styles[1];

  g_value_set_boxed (value, &image_repeat);

  return TRUE;
}

static const gchar *
border_image_repeat_style_to_string (GtkCssRepeatStyle repeat)
{
  switch (repeat)
    {
    case GTK_CSS_REPEAT_STYLE_NONE:
      return "stretch";
    case GTK_CSS_REPEAT_STYLE_REPEAT:
      return "repeat";
    case GTK_CSS_REPEAT_STYLE_ROUND:
      return "round";
    case GTK_CSS_REPEAT_STYLE_SPACE:
      return "space";
    default:
      return NULL;
    }
}

static void
border_image_repeat_value_print (const GValue *value,
                                 GString      *string)
{
  GtkCssBorderImageRepeat *image_repeat;

  image_repeat = g_value_get_boxed (value);

  g_string_append (string, border_image_repeat_style_to_string (image_repeat->hrepeat));
  if (image_repeat->hrepeat != image_repeat->vrepeat)
    {
      g_string_append (string, " ");
      g_string_append (string, border_image_repeat_style_to_string (image_repeat->vrepeat));
    }
}

static gboolean
border_image_value_parse (GtkCssParser *parser,
                          GFile *base,
                          GValue *value)
{
  GValue temp = { 0, };
  cairo_pattern_t *pattern = NULL;
  GtkGradient *gradient = NULL;
  GtkBorder slice, *width = NULL, *parsed_slice;
  GtkCssBorderImageRepeat repeat, *parsed_repeat;
  gboolean retval = FALSE;
  GtkBorderImage *image = NULL;

  g_value_init (&temp, CAIRO_GOBJECT_TYPE_PATTERN);

  if (!pattern_value_parse (parser, base, &temp))
    return FALSE;

  if (G_VALUE_TYPE (&temp) == GTK_TYPE_GRADIENT)
    gradient = g_value_dup_boxed (&temp);
  else
    pattern = g_value_dup_boxed (&temp);

  g_value_unset (&temp);
  g_value_init (&temp, GTK_TYPE_BORDER);

  if (!border_value_parse (parser, base, &temp))
    goto out;

  parsed_slice = g_value_get_boxed (&temp);
  slice = *parsed_slice;

  if (_gtk_css_parser_try (parser, "/", TRUE))
    {
      g_value_unset (&temp);
      g_value_init (&temp, GTK_TYPE_BORDER);

      if (!border_value_parse (parser, base, &temp))
        goto out;

      width = g_value_dup_boxed (&temp);
    }

  g_value_unset (&temp);
  g_value_init (&temp, GTK_TYPE_CSS_BORDER_IMAGE_REPEAT);

  if (!border_image_repeat_value_parse (parser, base, &temp))
    goto out;

  parsed_repeat = g_value_get_boxed (&temp);
  repeat = *parsed_repeat;

  g_value_unset (&temp);

  if (gradient != NULL)
    image = _gtk_border_image_new_for_gradient (gradient, &slice, width, &repeat);
  else if (pattern != NULL)
    image = _gtk_border_image_new (pattern, &slice, width, &repeat);

  if (image != NULL)
    {
      retval = TRUE;
      g_value_take_boxed (value, image);
    }

 out:
  if (pattern != NULL)
    cairo_pattern_destroy (pattern);

  if (gradient != NULL)
    gtk_gradient_unref (gradient);

  if (width != NULL)
    gtk_border_free (width);

  return retval;
}

static gboolean 
enum_value_parse (GtkCssParser *parser,
                  GFile        *base,
                  GValue       *value)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;
  char *str;

  str = _gtk_css_parser_try_ident (parser, TRUE);
  if (str == NULL)
    {
      _gtk_css_parser_error (parser, "Expected an identifier");
      return FALSE;
    }

  enum_class = g_type_class_ref (G_VALUE_TYPE (value));
  enum_value = g_enum_get_value_by_nick (enum_class, str);

  if (enum_value)
    g_value_set_enum (value, enum_value->value);
  else
    _gtk_css_parser_error (parser,
                           "Unknown value '%s' for enum type '%s'",
                           str, g_type_name (G_VALUE_TYPE (value)));
  
  g_type_class_unref (enum_class);
  g_free (str);

  return enum_value != NULL;
}

static void
enum_value_print (const GValue *value,
                  GString      *string)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  enum_class = g_type_class_ref (G_VALUE_TYPE (value));
  enum_value = g_enum_get_value (enum_class, g_value_get_enum (value));

  g_string_append (string, enum_value->value_nick);

  g_type_class_unref (enum_class);
}

static gboolean 
flags_value_parse (GtkCssParser *parser,
                   GFile        *base,
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

static gboolean 
bindings_value_parse (GtkCssParser *parser,
                      GFile        *base,
                      GValue       *value)
{
  GPtrArray *array;
  GtkBindingSet *binding_set;
  char *name;

  array = g_ptr_array_new ();

  do {
      name = _gtk_css_parser_try_ident (parser, TRUE);
      if (name == NULL)
        {
          _gtk_css_parser_error (parser, "Not a valid binding name");
          g_ptr_array_free (array, TRUE);
          return FALSE;
        }

      binding_set = gtk_binding_set_find (name);

      if (!binding_set)
        {
          _gtk_css_parser_error (parser, "No binding set named '%s'", name);
          g_free (name);
          continue;
        }

      g_ptr_array_add (array, binding_set);
      g_free (name);
    }
  while (_gtk_css_parser_try (parser, ",", TRUE));

  g_value_take_boxed (value, array);

  return TRUE;
}

static void
bindings_value_print (const GValue *value,
                      GString      *string)
{
  GPtrArray *array;
  guint i;

  array = g_value_get_boxed (value);

  for (i = 0; i < array->len; i++)
    {
      GtkBindingSet *binding_set = g_ptr_array_index (array, i);

      if (i > 0)
        g_string_append (string, ", ");
      g_string_append (string, binding_set->set_name);
    }
}

static gboolean 
border_corner_radius_value_parse (GtkCssParser *parser,
                                  GFile        *base,
                                  GValue       *value)
{
  GtkCssBorderCornerRadius corner;

  if (!_gtk_css_parser_try_double (parser, &corner.horizontal))
    {
      _gtk_css_parser_error (parser, "Expected a number");
      return FALSE;
    }
  else if (corner.horizontal < 0)
    goto negative;

  if (!_gtk_css_parser_try_double (parser, &corner.vertical))
    corner.vertical = corner.horizontal;
  else if (corner.vertical < 0)
    goto negative;

  g_value_set_boxed (value, &corner);
  return TRUE;

negative:
  _gtk_css_parser_error (parser, "Border radius values cannot be negative");
  return FALSE;
}

static void
border_corner_radius_value_print (const GValue *value,
                                  GString      *string)
{
  GtkCssBorderCornerRadius *corner;

  corner = g_value_get_boxed (value);

  if (corner == NULL)
    {
      g_string_append (string, "none");
      return;
    }

  string_append_double (string, corner->horizontal);
  if (corner->horizontal != corner->vertical)
    {
      g_string_append_c (string, ' ');
      string_append_double (string, corner->vertical);
    }
}

static gboolean 
border_radius_value_parse (GtkCssParser *parser,
                           GFile        *base,
                           GValue       *value)
{
  GtkCssBorderRadius border;

  if (!_gtk_css_parser_try_double (parser, &border.top_left.horizontal))
    {
      _gtk_css_parser_error (parser, "Expected a number");
      return FALSE;
    }
  else if (border.top_left.horizontal < 0)
    goto negative;

  if (_gtk_css_parser_try_double (parser, &border.top_right.horizontal))
    {
      if (border.top_right.horizontal < 0)
        goto negative;
      if (_gtk_css_parser_try_double (parser, &border.bottom_right.horizontal))
        {
          if (border.bottom_right.horizontal < 0)
            goto negative;
          if (!_gtk_css_parser_try_double (parser, &border.bottom_left.horizontal))
            border.bottom_left.horizontal = border.top_right.horizontal;
          else if (border.bottom_left.horizontal < 0)
            goto negative;
        }
      else
        {
          border.bottom_right.horizontal = border.top_left.horizontal;
          border.bottom_left.horizontal = border.top_right.horizontal;
        }
    }
  else
    {
      border.top_right.horizontal = border.top_left.horizontal;
      border.bottom_right.horizontal = border.top_left.horizontal;
      border.bottom_left.horizontal = border.top_left.horizontal;
    }

  if (_gtk_css_parser_try (parser, "/", TRUE))
    {
      if (!_gtk_css_parser_try_double (parser, &border.top_left.vertical))
        {
          _gtk_css_parser_error (parser, "Expected a number");
          return FALSE;
        }
      else if (border.top_left.vertical < 0)
        goto negative;

      if (_gtk_css_parser_try_double (parser, &border.top_right.vertical))
        {
          if (border.top_right.vertical < 0)
            goto negative;
          if (_gtk_css_parser_try_double (parser, &border.bottom_right.vertical))
            {
              if (border.bottom_right.vertical < 0)
                goto negative;
              if (!_gtk_css_parser_try_double (parser, &border.bottom_left.vertical))
                border.bottom_left.vertical = border.top_right.vertical;
              else if (border.bottom_left.vertical < 0)
                goto negative;
            }
          else
            {
              border.bottom_right.vertical = border.top_left.vertical;
              border.bottom_left.vertical = border.top_right.vertical;
            }
        }
      else
        {
          border.top_right.vertical = border.top_left.vertical;
          border.bottom_right.vertical = border.top_left.vertical;
          border.bottom_left.vertical = border.top_left.vertical;
        }
    }
  else
    {
      border.top_left.vertical = border.top_left.horizontal;
      border.top_right.vertical = border.top_right.horizontal;
      border.bottom_right.vertical = border.bottom_right.horizontal;
      border.bottom_left.vertical = border.bottom_left.horizontal;
    }

  /* border-radius is an int property for backwards-compat reasons */
  g_value_unset (value);
  g_value_init (value, GTK_TYPE_CSS_BORDER_RADIUS);
  g_value_set_boxed (value, &border);

  return TRUE;

negative:
  _gtk_css_parser_error (parser, "Border radius values cannot be negative");
  return FALSE;
}

static void
border_radius_value_print (const GValue *value,
                           GString      *string)
{
  GtkCssBorderRadius *border;

  border = g_value_get_boxed (value);

  if (border == NULL)
    {
      g_string_append (string, "none");
      return;
    }

  string_append_double (string, border->top_left.horizontal);
  if (border->top_left.horizontal != border->top_right.horizontal ||
      border->top_left.horizontal != border->bottom_right.horizontal ||
      border->top_left.horizontal != border->bottom_left.horizontal)
    {
      g_string_append_c (string, ' ');
      string_append_double (string, border->top_right.horizontal);
      if (border->top_left.horizontal != border->bottom_right.horizontal ||
          border->top_right.horizontal != border->bottom_left.horizontal)
        {
          g_string_append_c (string, ' ');
          string_append_double (string, border->bottom_right.horizontal);
          if (border->top_right.horizontal != border->bottom_left.horizontal)
            {
              g_string_append_c (string, ' ');
              string_append_double (string, border->bottom_left.horizontal);
            }
        }
    }

  if (border->top_left.horizontal != border->top_left.vertical ||
      border->top_right.horizontal != border->top_right.vertical ||
      border->bottom_right.horizontal != border->bottom_right.vertical ||
      border->bottom_left.horizontal != border->bottom_left.vertical)
    {
      g_string_append (string, " / ");
      string_append_double (string, border->top_left.vertical);
      if (border->top_left.vertical != border->top_right.vertical ||
          border->top_left.vertical != border->bottom_right.vertical ||
          border->top_left.vertical != border->bottom_left.vertical)
        {
          g_string_append_c (string, ' ');
          string_append_double (string, border->top_right.vertical);
          if (border->top_left.vertical != border->bottom_right.vertical ||
              border->top_right.vertical != border->bottom_left.vertical)
            {
              g_string_append_c (string, ' ');
              string_append_double (string, border->bottom_right.vertical);
              if (border->top_right.vertical != border->bottom_left.vertical)
                {
                  g_string_append_c (string, ' ');
                  string_append_double (string, border->bottom_left.vertical);
                }
            }
        }

    }
}

static gboolean 
transparent_color_value_parse (GtkCssParser *parser,
                               GFile        *base,
                               GValue       *value)
{
  if (_gtk_css_parser_try (parser, "transparent", TRUE))
    {
      GdkRGBA transparent = { 0, 0, 0, 0 };
          
      g_value_set_boxed (value, &transparent);

      return TRUE;
    }

  return rgba_value_parse (parser, base, value);
}

static gboolean 
border_color_shorthand_value_parse (GtkCssParser *parser,
                                    GFile        *base,
                                    GValue       *value)
{
  GtkSymbolicColor *symbolic;
  GPtrArray *array;

  array = g_ptr_array_new_with_free_func ((GDestroyNotify) gtk_symbolic_color_unref);

  do
    {
      if (_gtk_css_parser_try (parser, "transparent", TRUE))
        {
          GdkRGBA transparent = { 0, 0, 0, 0 };
          
          symbolic = gtk_symbolic_color_new_literal (&transparent);
        }
      else
        {
          symbolic = _gtk_css_parser_read_symbolic_color (parser);
      
          if (symbolic == NULL)
            return FALSE;
        }
      
      g_ptr_array_add (array, symbolic);
    }
  while (array->len < 4 && 
         !_gtk_css_parser_is_eof (parser) &&
         !_gtk_css_parser_begins_with (parser, ';') &&
         !_gtk_css_parser_begins_with (parser, '}'));

  switch (array->len)
    {
      default:
        g_assert_not_reached ();
        break;
      case 1:
        g_ptr_array_add (array, gtk_symbolic_color_ref (g_ptr_array_index (array, 0)));
        /* fall through */
      case 2:
        g_ptr_array_add (array, gtk_symbolic_color_ref (g_ptr_array_index (array, 0)));
        /* fall through */
      case 3:
        g_ptr_array_add (array, gtk_symbolic_color_ref (g_ptr_array_index (array, 1)));
        /* fall through */
      case 4:
        break;
    }

  g_value_unset (value);
  g_value_init (value, G_TYPE_PTR_ARRAY);
  g_value_take_boxed (value, array);

  return TRUE;
}

/*** PACKING ***/

static GParameter *
unpack_border (const GValue *value,
               guint        *n_params,
               const char   *top,
               const char   *left,
               const char   *bottom,
               const char   *right)
{
  GParameter *parameter = g_new0 (GParameter, 4);
  GtkBorder *border = g_value_get_boxed (value);

  parameter[0].name = top;
  g_value_init (&parameter[0].value, G_TYPE_INT);
  g_value_set_int (&parameter[0].value, border->top);
  parameter[1].name = left;
  g_value_init (&parameter[1].value, G_TYPE_INT);
  g_value_set_int (&parameter[1].value, border->left);
  parameter[2].name = bottom;
  g_value_init (&parameter[2].value, G_TYPE_INT);
  g_value_set_int (&parameter[2].value, border->bottom);
  parameter[3].name = right;
  g_value_init (&parameter[3].value, G_TYPE_INT);
  g_value_set_int (&parameter[3].value, border->right);

  *n_params = 4;
  return parameter;
}

static void
pack_border (GValue             *value,
             GtkStyleProperties *props,
             GtkStateFlags       state,
             const char         *top,
             const char         *left,
             const char         *bottom,
             const char         *right)
{
  GtkBorder border;
  int t, l, b, r;

  gtk_style_properties_get (props,
                            state,
                            top, &t,
                            left, &l,
                            bottom, &b,
                            right, &r,
                            NULL);

  border.top = t;
  border.left = l;
  border.bottom = b;
  border.right = r;

  g_value_set_boxed (value, &border);
}

static GParameter *
unpack_border_width (const GValue *value,
                     guint        *n_params)
{
  return unpack_border (value, n_params,
                        "border-top-width", "border-left-width",
                        "border-bottom-width", "border-right-width");
}

static void
pack_border_width (GValue             *value,
                   GtkStyleProperties *props,
                   GtkStateFlags       state)
{
  pack_border (value, props, state,
               "border-top-width", "border-left-width",
               "border-bottom-width", "border-right-width");
}

static GParameter *
unpack_padding (const GValue *value,
                guint        *n_params)
{
  return unpack_border (value, n_params,
                        "padding-top", "padding-left",
                        "padding-bottom", "padding-right");
}

static void
pack_padding (GValue             *value,
              GtkStyleProperties *props,
              GtkStateFlags       state)
{
  pack_border (value, props, state,
               "padding-top", "padding-left",
               "padding-bottom", "padding-right");
}

static GParameter *
unpack_margin (const GValue *value,
               guint        *n_params)
{
  return unpack_border (value, n_params,
                        "margin-top", "margin-left",
                        "margin-bottom", "margin-right");
}

static void
pack_margin (GValue             *value,
             GtkStyleProperties *props,
             GtkStateFlags       state)
{
  pack_border (value, props, state,
               "margin-top", "margin-left",
               "margin-bottom", "margin-right");
}

static GParameter *
unpack_border_radius (const GValue *value,
                      guint        *n_params)
{
  GParameter *parameter = g_new0 (GParameter, 4);
  GtkCssBorderRadius *border;
  
  if (G_VALUE_HOLDS_BOXED (value))
    border = g_value_get_boxed (value);
  else
    border = NULL;

  parameter[0].name = "border-top-left-radius";
  g_value_init (&parameter[0].value, GTK_TYPE_CSS_BORDER_CORNER_RADIUS);
  parameter[1].name = "border-top-right-radius";
  g_value_init (&parameter[1].value, GTK_TYPE_CSS_BORDER_CORNER_RADIUS);
  parameter[2].name = "border-bottom-right-radius";
  g_value_init (&parameter[2].value, GTK_TYPE_CSS_BORDER_CORNER_RADIUS);
  parameter[3].name = "border-bottom-left-radius";
  g_value_init (&parameter[3].value, GTK_TYPE_CSS_BORDER_CORNER_RADIUS);
  if (border)
    {
      g_value_set_boxed (&parameter[0].value, &border->top_left);
      g_value_set_boxed (&parameter[1].value, &border->top_right);
      g_value_set_boxed (&parameter[2].value, &border->bottom_right);
      g_value_set_boxed (&parameter[3].value, &border->bottom_left);
    }

  *n_params = 4;
  return parameter;
}

static void
pack_border_radius (GValue             *value,
                    GtkStyleProperties *props,
                    GtkStateFlags       state)
{
  GtkCssBorderCornerRadius *top_left;

  /* NB: We are an int property, so we have to resolve to an int here.
   * So we just resolve to an int. We pick one and stick to it.
   * Lesson learned: Don't query border-radius shorthand, query the 
   * real properties instead. */
  gtk_style_properties_get (props,
                            state,
                            "border-top-left-radius", &top_left,
                            NULL);

  if (top_left)
    g_value_set_int (value, top_left->horizontal);

  g_free (top_left);
}

static GParameter *
unpack_font_description (const GValue *value,
                         guint        *n_params)
{
  GParameter *parameter = g_new0 (GParameter, 5);
  PangoFontDescription *description;
  PangoFontMask mask;
  guint n;
  
  /* For backwards compat, we only unpack values that are indeed set.
   * For strict CSS conformance we need to unpack all of them.
   * Note that we do set all of them in the parse function, so it
   * will not have effects when parsing CSS files. It will though
   * for custom style providers.
   */

  description = g_value_get_boxed (value);
  n = 0;

  if (description)
    mask = pango_font_description_get_set_fields (description);
  else
    mask = 0;

  if (mask & PANGO_FONT_MASK_FAMILY)
    {
      GPtrArray *strv = g_ptr_array_new ();

      g_ptr_array_add (strv, g_strdup (pango_font_description_get_family (description)));
      g_ptr_array_add (strv, NULL);
      parameter[n].name = "font-family";
      g_value_init (&parameter[n].value, G_TYPE_STRV);
      g_value_take_boxed (&parameter[n].value,
                          g_ptr_array_free (strv, FALSE));
      n++;
    }

  if (mask & PANGO_FONT_MASK_STYLE)
    {
      parameter[n].name = "font-style";
      g_value_init (&parameter[n].value, PANGO_TYPE_STYLE);
      g_value_set_enum (&parameter[n].value,
                        pango_font_description_get_style (description));
      n++;
    }

  if (mask & PANGO_FONT_MASK_VARIANT)
    {
      parameter[n].name = "font-variant";
      g_value_init (&parameter[n].value, PANGO_TYPE_VARIANT);
      g_value_set_enum (&parameter[n].value,
                        pango_font_description_get_variant (description));
      n++;
    }

  if (mask & PANGO_FONT_MASK_WEIGHT)
    {
      parameter[n].name = "font-weight";
      g_value_init (&parameter[n].value, PANGO_TYPE_WEIGHT);
      g_value_set_enum (&parameter[n].value,
                        pango_font_description_get_weight (description));
      n++;
    }

  if (mask & PANGO_FONT_MASK_SIZE)
    {
      parameter[n].name = "font-size";
      g_value_init (&parameter[n].value, G_TYPE_DOUBLE);
      g_value_set_double (&parameter[n].value,
                          (double) pango_font_description_get_size (description) / PANGO_SCALE);
      n++;
    }

  *n_params = n;

  return parameter;
}

static void
pack_font_description (GValue             *value,
                       GtkStyleProperties *props,
                       GtkStateFlags       state)
{
  PangoFontDescription *description;
  char **families;
  PangoStyle style;
  PangoVariant variant;
  PangoWeight weight;
  double size;

  gtk_style_properties_get (props,
                            state,
                            "font-family", &families,
                            "font-style", &style,
                            "font-variant", &variant,
                            "font-weight", &weight,
                            "font-size", &size,
                            NULL);

  description = pango_font_description_new ();
  /* xxx: Can we set all the families here somehow? */
  if (families)
    pango_font_description_set_family (description, families[0]);
  pango_font_description_set_size (description, round (size * PANGO_SCALE));
  pango_font_description_set_style (description, style);
  pango_font_description_set_variant (description, variant);
  pango_font_description_set_weight (description, weight);

  g_strfreev (families);

  g_value_take_boxed (value, description);
}

static GParameter *
unpack_border_color (const GValue *value,
                     guint        *n_params)
{
  GParameter *parameter = g_new0 (GParameter, 4);
  GType type;
  
  type = G_VALUE_TYPE (value);
  if (type == G_TYPE_PTR_ARRAY)
    type = GTK_TYPE_SYMBOLIC_COLOR;

  parameter[0].name = "border-top-color";
  g_value_init (&parameter[0].value, type);
  parameter[1].name = "border-right-color";
  g_value_init (&parameter[1].value, type);
  parameter[2].name = "border-bottom-color";
  g_value_init (&parameter[2].value, type);
  parameter[3].name = "border-left-color";
  g_value_init (&parameter[3].value, type);

  if (G_VALUE_TYPE (value) == G_TYPE_PTR_ARRAY)
    {
      GPtrArray *array = g_value_get_boxed (value);
      guint i;

      for (i = 0; i < 4; i++)
        g_value_set_boxed (&parameter[i].value, g_ptr_array_index (array, i));
    }
  else
    {
      /* can be RGBA or symbolic color */
      gpointer p = g_value_get_boxed (value);

      g_value_set_boxed (&parameter[0].value, p);
      g_value_set_boxed (&parameter[1].value, p);
      g_value_set_boxed (&parameter[2].value, p);
      g_value_set_boxed (&parameter[3].value, p);
    }

  *n_params = 4;
  return parameter;
}

static void
pack_border_color (GValue             *value,
                   GtkStyleProperties *props,
                   GtkStateFlags       state)
{
  /* NB: We are a color property, so we have to resolve to a color here.
   * So we just resolve to a color. We pick one and stick to it.
   * Lesson learned: Don't query border-color shorthand, query the 
   * real properties instead. */
  g_value_unset (value);
  gtk_style_properties_get_property (props, "border-top-color", state, value);
}

/*** UNSET FUNCS ***/

static void
unset_font_description (GtkStyleProperties *props,
                        GtkStateFlags       state)
{
  gtk_style_properties_unset_property (props, "font-family", state);
  gtk_style_properties_unset_property (props, "font-style", state);
  gtk_style_properties_unset_property (props, "font-variant", state);
  gtk_style_properties_unset_property (props, "font-weight", state);
  gtk_style_properties_unset_property (props, "font-size", state);
}

static void
unset_margin (GtkStyleProperties *props,
              GtkStateFlags       state)
{
  gtk_style_properties_unset_property (props, "margin-top", state);
  gtk_style_properties_unset_property (props, "margin-right", state);
  gtk_style_properties_unset_property (props, "margin-bottom", state);
  gtk_style_properties_unset_property (props, "margin-left", state);
}

static void
unset_padding (GtkStyleProperties *props,
               GtkStateFlags       state)
{
  gtk_style_properties_unset_property (props, "padding-top", state);
  gtk_style_properties_unset_property (props, "padding-right", state);
  gtk_style_properties_unset_property (props, "padding-bottom", state);
  gtk_style_properties_unset_property (props, "padding-left", state);
}

static void
unset_border_width (GtkStyleProperties *props,
                    GtkStateFlags       state)
{
  gtk_style_properties_unset_property (props, "border-top-width", state);
  gtk_style_properties_unset_property (props, "border-right-width", state);
  gtk_style_properties_unset_property (props, "border-bottom-width", state);
  gtk_style_properties_unset_property (props, "border-left-width", state);
}

static void
unset_border_radius (GtkStyleProperties *props,
                     GtkStateFlags       state)
{
  gtk_style_properties_unset_property (props, "border-top-right-radius", state);
  gtk_style_properties_unset_property (props, "border-bottom-right-radius", state);
  gtk_style_properties_unset_property (props, "border-bottom-left-radius", state);
  gtk_style_properties_unset_property (props, "border-top-left-radius", state);
}

static void
unset_border_color (GtkStyleProperties *props,
                    GtkStateFlags       state)
{
  gtk_style_properties_unset_property (props, "border-top-color", state);
  gtk_style_properties_unset_property (props, "border-right-color", state);
  gtk_style_properties_unset_property (props, "border-bottom-color", state);
  gtk_style_properties_unset_property (props, "border-left-color", state);
}

static void
unset_border_image (GtkStyleProperties *props,
                    GtkStateFlags       state)
{
  gtk_style_properties_unset_property (props, "border-image-source", state);
  gtk_style_properties_unset_property (props, "border-image-slice", state);
  gtk_style_properties_unset_property (props, "border-image-repeat", state);
  gtk_style_properties_unset_property (props, "border-image-width", state);
}

/*** default values ***/

static void
border_image_width_default_value (GtkStyleProperties *props,
                                  GtkStateFlags       state,
                                  GValue             *value)
{
}

static void
background_color_default_value (GtkStyleProperties *props,
                                GtkStateFlags       state,
                                GValue             *value)
{
  GdkRGBA transparent_black = { 0, 0, 0, 0 };

  g_value_set_boxed (value, &transparent_black);
}

static void
border_color_default_value (GtkStyleProperties *props,
                            GtkStateFlags       state,
                            GValue             *value)
{
  g_value_unset (value);
  gtk_style_properties_get_property (props, "color", state, value);
}

/*** API ***/

static void
css_string_funcs_init (void)
{
  if (G_LIKELY (parse_funcs != NULL))
    return;

  parse_funcs = g_hash_table_new (NULL, NULL);
  print_funcs = g_hash_table_new (NULL, NULL);

  register_conversion_function (GDK_TYPE_RGBA,
                                rgba_value_parse,
                                rgba_value_print);
  register_conversion_function (GDK_TYPE_COLOR,
                                color_value_parse,
                                color_value_print);
  register_conversion_function (GTK_TYPE_SYMBOLIC_COLOR,
                                symbolic_color_value_parse,
                                symbolic_color_value_print);
  register_conversion_function (PANGO_TYPE_FONT_DESCRIPTION,
                                font_description_value_parse,
                                font_description_value_print);
  register_conversion_function (G_TYPE_BOOLEAN,
                                boolean_value_parse,
                                boolean_value_print);
  register_conversion_function (G_TYPE_INT,
                                int_value_parse,
                                int_value_print);
  register_conversion_function (G_TYPE_UINT,
                                uint_value_parse,
                                uint_value_print);
  register_conversion_function (G_TYPE_DOUBLE,
                                double_value_parse,
                                double_value_print);
  register_conversion_function (G_TYPE_FLOAT,
                                float_value_parse,
                                float_value_print);
  register_conversion_function (G_TYPE_STRING,
                                string_value_parse,
                                string_value_print);
  register_conversion_function (GTK_TYPE_THEMING_ENGINE,
                                theming_engine_value_parse,
                                theming_engine_value_print);
  register_conversion_function (GTK_TYPE_ANIMATION_DESCRIPTION,
                                animation_description_value_parse,
                                animation_description_value_print);
  register_conversion_function (GTK_TYPE_BORDER,
                                border_value_parse,
                                border_value_print);
  register_conversion_function (GTK_TYPE_GRADIENT,
                                gradient_value_parse,
                                gradient_value_print);
  register_conversion_function (CAIRO_GOBJECT_TYPE_PATTERN,
                                pattern_value_parse,
                                pattern_value_print);
  register_conversion_function (GTK_TYPE_BORDER_IMAGE,
                                border_image_value_parse,
                                NULL);
  register_conversion_function (GTK_TYPE_CSS_BORDER_IMAGE_REPEAT,
                                border_image_repeat_value_parse,
                                border_image_repeat_value_print);
  register_conversion_function (GTK_TYPE_SHADOW,
                                shadow_value_parse,
                                shadow_value_print);
  register_conversion_function (G_TYPE_ENUM,
                                enum_value_parse,
                                enum_value_print);
  register_conversion_function (G_TYPE_FLAGS,
                                flags_value_parse,
                                flags_value_print);
}

gboolean
_gtk_style_property_parse_value (const GtkStyleProperty *property,
                                 GValue                 *value,
                                 GtkCssParser           *parser,
                                 GFile                  *base)
{
  GtkStyleParseFunc func;

  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (parser != NULL, FALSE);

  css_string_funcs_init ();

  if (property)
    {
      if (_gtk_css_parser_try (parser, "none", TRUE))
        {
          /* Insert the default value, so it has an opportunity
           * to override other style providers when merged
           */
          g_param_value_set_default (property->pspec, value);
          return TRUE;
        }
      else if (property->property_parse_func)
        {
          GError *error = NULL;
          char *value_str;
          gboolean success;
          
          value_str = _gtk_css_parser_read_value (parser);
          if (value_str == NULL)
            return FALSE;
          
          success = (*property->property_parse_func) (value_str, value, &error);

          g_free (value_str);

          return success;
        }

      func = property->parse_func;
    }
  else
    func = NULL;

  if (func == NULL)
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

  return (*func) (parser, base, value);
}

void
_gtk_style_property_print_value (const GtkStyleProperty *property,
                                 const GValue           *value,
                                 GString                *string)
{
  GtkStylePrintFunc func;

  css_string_funcs_init ();

  if (property)
    func = property->print_func;
  else
    func = NULL;

  if (func == NULL)
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

void
_gtk_style_property_default_value (const GtkStyleProperty *property,
                                   GtkStyleProperties     *properties,
                                   GtkStateFlags           state,
                                   GValue                 *value)
{
  if (property->default_value_func)
    property->default_value_func (properties, state, value);
  else if (property->pspec->value_type == GTK_TYPE_THEMING_ENGINE)
    g_value_set_object (value, gtk_theming_engine_load (NULL));
  else if (property->pspec->value_type == PANGO_TYPE_FONT_DESCRIPTION)
    g_value_take_boxed (value, pango_font_description_from_string ("Sans 10"));
  else if (property->pspec->value_type == GDK_TYPE_RGBA)
    {
      GdkRGBA color;
      gdk_rgba_parse (&color, "pink");
      g_value_set_boxed (value, &color);
    }
  else if (property->pspec->value_type == GTK_TYPE_BORDER)
    {
      g_value_take_boxed (value, gtk_border_new ());
    }
  else
    g_param_value_set_default (property->pspec, value);
}

gboolean
_gtk_style_property_is_inherit (const GtkStyleProperty *property)
{
  g_return_val_if_fail (property != NULL, FALSE);

  return property->flags & GTK_STYLE_PROPERTY_INHERIT ? TRUE : FALSE;
}

static gboolean
resolve_color (GtkStyleProperties *props,
	       GValue             *value)
{
  GdkRGBA color;

  /* Resolve symbolic color to GdkRGBA */
  if (!gtk_symbolic_color_resolve (g_value_get_boxed (value), props, &color))
    return FALSE;

  /* Store it back, this is where GdkRGBA caching happens */
  g_value_unset (value);
  g_value_init (value, GDK_TYPE_RGBA);
  g_value_set_boxed (value, &color);

  return TRUE;
}

static gboolean
resolve_color_rgb (GtkStyleProperties *props,
                   GValue             *value)
{
  GdkColor color = { 0 };
  GdkRGBA rgba;

  if (!gtk_symbolic_color_resolve (g_value_get_boxed (value), props, &rgba))
    return FALSE;

  color.red = rgba.red * 65535. + 0.5;
  color.green = rgba.green * 65535. + 0.5;
  color.blue = rgba.blue * 65535. + 0.5;

  g_value_unset (value);
  g_value_init (value, GDK_TYPE_COLOR);
  g_value_set_boxed (value, &color);

  return TRUE;
}

static gboolean
resolve_gradient (GtkStyleProperties *props,
                  GValue             *value)
{
  cairo_pattern_t *gradient;

  if (!gtk_gradient_resolve (g_value_get_boxed (value), props, &gradient))
    return FALSE;

  /* Store it back, this is where cairo_pattern_t caching happens */
  g_value_unset (value);
  g_value_init (value, CAIRO_GOBJECT_TYPE_PATTERN);
  g_value_take_boxed (value, gradient);

  return TRUE;
}

static gboolean
resolve_shadow (GtkStyleProperties *props,
                GValue *value)
{
  GtkShadow *resolved, *base;

  base = g_value_get_boxed (value);

  if (base == NULL)
    return TRUE;
  
  if (_gtk_shadow_get_resolved (base))
    return TRUE;

  resolved = _gtk_shadow_resolve (base, props);
  if (resolved == NULL)
    return FALSE;

  g_value_take_boxed (value, resolved);

  return TRUE;
}

void
_gtk_style_property_resolve (const GtkStyleProperty *property,
                             GtkStyleProperties     *props,
                             GtkStateFlags           state,
                             GValue                 *val)
{
  if (G_VALUE_TYPE (val) == GTK_TYPE_SYMBOLIC_COLOR)
    {
      if (property->pspec->value_type == GDK_TYPE_RGBA)
        {
          if (resolve_color (props, val))
            return;
        }
      else if (property->pspec->value_type == GDK_TYPE_COLOR)
        {
          if (resolve_color_rgb (props, val))
            return;
        }
      
      g_value_unset (val);
      g_value_init (val, property->pspec->value_type);
      _gtk_style_property_default_value (property, props, state, val);
    }
  else if (G_VALUE_TYPE (val) == GDK_TYPE_RGBA)
    {
      if (g_value_get_boxed (val) == NULL)
        _gtk_style_property_default_value (property, props, state, val);
    }
  else if (G_VALUE_TYPE (val) == GTK_TYPE_GRADIENT)
    {
      g_return_if_fail (property->pspec->value_type == CAIRO_GOBJECT_TYPE_PATTERN);

      if (!resolve_gradient (props, val))
        {
          g_value_unset (val);
          g_value_init (val, CAIRO_GOBJECT_TYPE_PATTERN);
          _gtk_style_property_default_value (property, props, state, val);
        }
    }
  else if (G_VALUE_TYPE (val) == GTK_TYPE_SHADOW)
    {
      if (!resolve_shadow (props, val))
        _gtk_style_property_default_value (property, props, state, val);
    }
}

gboolean
_gtk_style_property_is_shorthand  (const GtkStyleProperty *property)
{
  g_return_val_if_fail (property != NULL, FALSE);

  return property->pack_func != NULL;
}

GParameter *
_gtk_style_property_unpack (const GtkStyleProperty *property,
                            const GValue           *value,
                            guint                  *n_params)
{
  g_return_val_if_fail (property != NULL, NULL);
  g_return_val_if_fail (property->unpack_func != NULL, NULL);
  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (n_params != NULL, NULL);

  return property->unpack_func (value, n_params);
}

void
_gtk_style_property_pack (const GtkStyleProperty *property,
                          GtkStyleProperties     *props,
                          GtkStateFlags           state,
                          GValue                 *value)
{
  g_return_if_fail (property != NULL);
  g_return_if_fail (property->pack_func != NULL);
  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));
  g_return_if_fail (G_IS_VALUE (value));

  property->pack_func (value, props, state);
}

static void
gtk_style_property_init (void)
{
  if (G_LIKELY (properties))
    return;

  /* stuff is never freed, so no need for free functions */
  properties = g_hash_table_new (g_str_hash, g_str_equal);

  /* note that gtk_style_properties_register_property() calls this function,
   * so make sure we're sanely inited to avoid infloops */

  _gtk_style_property_register           (g_param_spec_boxed ("color",
                                          "Foreground color",
                                          "Foreground color",
                                          GDK_TYPE_RGBA, 0),
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_boxed ("background-color",
                                          "Background color",
                                          "Background color",
                                          GDK_TYPE_RGBA, 0),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          transparent_color_value_parse,
                                          NULL,
                                          background_color_default_value,
                                          NULL);

  _gtk_style_property_register           (g_param_spec_boxed ("font-family",
                                                              "Font family",
                                                              "Font family",
                                                              G_TYPE_STRV, 0),
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          font_family_parse,
                                          font_family_value_print,
                                          NULL,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_enum ("font-style",
                                                             "Font style",
                                                             "Font style",
                                                             PANGO_TYPE_STYLE,
                                                             PANGO_STYLE_NORMAL, 0),
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_enum ("font-variant",
                                                             "Font variant",
                                                             "Font variant",
                                                             PANGO_TYPE_VARIANT,
                                                             PANGO_VARIANT_NORMAL, 0),
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);
  /* xxx: need to parse this properly, ie parse the numbers */
  _gtk_style_property_register           (g_param_spec_enum ("font-weight",
                                                             "Font weight",
                                                             "Font weight",
                                                             PANGO_TYPE_WEIGHT,
                                                             PANGO_WEIGHT_NORMAL, 0),
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_double ("font-size",
                                                               "Font size",
                                                               "Font size",
                                                               0, G_MAXDOUBLE, 0, 0),
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_boxed ("font",
                                                              "Font Description",
                                                              "Font Description",
                                                              PANGO_TYPE_FONT_DESCRIPTION, 0),
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          unpack_font_description,
                                          pack_font_description,
                                          font_description_value_parse,
                                          font_description_value_print,
                                          NULL,
                                          unset_font_description);

  _gtk_style_property_register           (g_param_spec_boxed ("text-shadow",
                                                              "Text shadow",
                                                              "Text shadow",
                                                              GTK_TYPE_SHADOW, 0),
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);

  _gtk_style_property_register           (g_param_spec_boxed ("icon-shadow",
                                                              "Icon shadow",
                                                              "Icon shadow",
                                                              GTK_TYPE_SHADOW, 0),
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);

  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("box-shadow",
                                                              "Box shadow",
                                                              "Box shadow",
                                                              GTK_TYPE_SHADOW, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("margin-top",
                                                            "margin top",
                                                            "Margin at top",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("margin-left",
                                                            "margin left",
                                                            "Margin at left",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("margin-bottom",
                                                            "margin bottom",
                                                            "Margin at bottom",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("margin-right",
                                                            "margin right",
                                                            "Margin at right",
                                                            0, G_MAXINT, 0, 0));
  _gtk_style_property_register           (g_param_spec_boxed ("margin",
                                                              "Margin",
                                                              "Margin",
                                                              GTK_TYPE_BORDER, 0),
                                          0,
                                          NULL,
                                          unpack_margin,
                                          pack_margin,
                                          NULL,
                                          NULL,
                                          NULL,
                                          unset_margin);
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("padding-top",
                                                            "padding top",
                                                            "Padding at top",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("padding-left",
                                                            "padding left",
                                                            "Padding at left",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("padding-bottom",
                                                            "padding bottom",
                                                            "Padding at bottom",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("padding-right",
                                                            "padding right",
                                                            "Padding at right",
                                                            0, G_MAXINT, 0, 0));
  _gtk_style_property_register           (g_param_spec_boxed ("padding",
                                                              "Padding",
                                                              "Padding",
                                                              GTK_TYPE_BORDER, 0),
                                          0,
                                          NULL,
                                          unpack_padding,
                                          pack_padding,
                                          NULL,
                                          NULL,
                                          NULL,
                                          unset_padding);
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("border-top-width",
                                                            "border top width",
                                                            "Border width at top",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("border-left-width",
                                                            "border left width",
                                                            "Border width at left",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("border-bottom-width",
                                                            "border bottom width",
                                                            "Border width at bottom",
                                                            0, G_MAXINT, 0, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_int ("border-right-width",
                                                            "border right width",
                                                            "Border width at right",
                                                            0, G_MAXINT, 0, 0));
  _gtk_style_property_register           (g_param_spec_boxed ("border-width",
                                                              "Border width",
                                                              "Border width, in pixels",
                                                              GTK_TYPE_BORDER, 0),
                                          0,
                                          NULL,
                                          unpack_border_width,
                                          pack_border_width,
                                          NULL,
                                          NULL,
                                          NULL,
                                          unset_border_width);

  _gtk_style_property_register           (g_param_spec_boxed ("border-top-left-radius",
                                                              "Border top left radius",
                                                              "Border radius of top left corner, in pixels",
                                                              GTK_TYPE_CSS_BORDER_CORNER_RADIUS, 0),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_boxed ("border-top-right-radius",
                                                              "Border top right radius",
                                                              "Border radius of top right corner, in pixels",
                                                              GTK_TYPE_CSS_BORDER_CORNER_RADIUS, 0),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_boxed ("border-bottom-right-radius",
                                                              "Border bottom right radius",
                                                              "Border radius of bottom right corner, in pixels",
                                                              GTK_TYPE_CSS_BORDER_CORNER_RADIUS, 0),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_boxed ("border-bottom-left-radius",
                                                              "Border bottom left radius",
                                                              "Border radius of bottom left corner, in pixels",
                                                              GTK_TYPE_CSS_BORDER_CORNER_RADIUS, 0),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_int ("border-radius",
                                                            "Border radius",
                                                            "Border radius, in pixels",
                                                            0, G_MAXINT, 0, 0),
                                          0,
                                          NULL,
                                          unpack_border_radius,
                                          pack_border_radius,
                                          border_radius_value_parse,
                                          border_radius_value_print,
                                          NULL,
                                          unset_border_radius);

  gtk_style_properties_register_property (NULL,
                                          g_param_spec_enum ("border-style",
                                                             "Border style",
                                                             "Border style",
                                                             GTK_TYPE_BORDER_STYLE,
                                                             GTK_BORDER_STYLE_NONE, 0));
  _gtk_style_property_register           (g_param_spec_boxed ("border-top-color",
                                                              "Border top color",
                                                              "Border top color",
                                                              GDK_TYPE_RGBA, 0),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          transparent_color_value_parse,
                                          NULL,
                                          border_color_default_value,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_boxed ("border-right-color",
                                                              "Border right color",
                                                              "Border right color",
                                                              GDK_TYPE_RGBA, 0),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          transparent_color_value_parse,
                                          NULL,
                                          border_color_default_value,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_boxed ("border-bottom-color",
                                                              "Border bottom color",
                                                              "Border bottom color",
                                                              GDK_TYPE_RGBA, 0),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          transparent_color_value_parse,
                                          NULL,
                                          border_color_default_value,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_boxed ("border-left-color",
                                                              "Border left color",
                                                              "Border left color",
                                                              GDK_TYPE_RGBA, 0),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          transparent_color_value_parse,
                                          NULL,
                                          border_color_default_value,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_boxed ("border-color",
                                                              "Border color",
                                                              "Border color",
                                                              GDK_TYPE_RGBA, 0),
                                          0,
                                          NULL,
                                          unpack_border_color,
                                          pack_border_color,
                                          border_color_shorthand_value_parse,
                                          NULL,
                                          NULL,
                                          unset_border_color);

  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("background-image",
                                                              "Background Image",
                                                              "Background Image",
                                                              CAIRO_GOBJECT_TYPE_PATTERN, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("border-image-source",
                                                              "Border image source",
                                                              "Border image source",
                                                              CAIRO_GOBJECT_TYPE_PATTERN, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("border-image-repeat",
                                                              "Border image repeat",
                                                              "Border image repeat",
                                                              GTK_TYPE_CSS_BORDER_IMAGE_REPEAT, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("border-image-slice",
                                                              "Border image slice",
                                                              "Border image slice",
                                                              GTK_TYPE_BORDER, 0));
  _gtk_style_property_register           (g_param_spec_boxed ("border-image-width",
                                                              "Border image width",
                                                              "Border image width",
                                                              GTK_TYPE_BORDER, 0),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          border_image_width_default_value,
                                          NULL);
  _gtk_style_property_register           (g_param_spec_boxed ("border-image",
                                                              "Border Image",
                                                              "Border Image",
                                                              GTK_TYPE_BORDER_IMAGE, 0),
                                          0,
                                          NULL,
                                          _gtk_border_image_unpack,
                                          _gtk_border_image_pack,
                                          NULL,
                                          NULL,
                                          NULL,
                                          unset_border_image);
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_object ("engine",
                                                               "Theming Engine",
                                                               "Theming Engine",
                                                               GTK_TYPE_THEMING_ENGINE, 0));
  gtk_style_properties_register_property (NULL,
                                          g_param_spec_boxed ("transition",
                                                              "Transition animation description",
                                                              "Transition animation description",
                                                              GTK_TYPE_ANIMATION_DESCRIPTION, 0));

  /* Private property holding the binding sets */
  _gtk_style_property_register           (g_param_spec_boxed ("gtk-key-bindings",
                                                              "Key bindings",
                                                              "Key bindings",
                                                              G_TYPE_PTR_ARRAY, 0),
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          bindings_value_parse,
                                          bindings_value_print,
                                          NULL,
                                          NULL);
}

const GtkStyleProperty *
_gtk_style_property_lookup (const char *name)
{
  gtk_style_property_init ();

  return g_hash_table_lookup (properties, name);
}

void
_gtk_style_property_register (GParamSpec               *pspec,
                              GtkStylePropertyFlags     flags,
                              GtkStylePropertyParser    property_parse_func,
                              GtkStyleUnpackFunc        unpack_func,
                              GtkStylePackFunc          pack_func,
                              GtkStyleParseFunc         parse_func,
                              GtkStylePrintFunc         print_func,
                              GtkStyleDefaultValueFunc  default_value_func,
                              GtkStyleUnsetFunc         unset_func)
{
  const GtkStyleProperty *existing;
  GtkStyleProperty *node;

  g_return_if_fail ((pack_func == NULL) == (unpack_func == NULL));

  gtk_style_property_init ();

  existing = _gtk_style_property_lookup (pspec->name);
  if (existing != NULL)
    {
      g_warning ("Property \"%s\" was already registered with type %s",
                 pspec->name, g_type_name (existing->pspec->value_type));
      return;
    }

  node = g_slice_new0 (GtkStyleProperty);
  node->flags = flags;
  node->pspec = pspec;
  node->property_parse_func = property_parse_func;
  node->pack_func = pack_func;
  node->unpack_func = unpack_func;
  node->parse_func = parse_func;
  node->print_func = print_func;
  node->default_value_func = default_value_func;
  node->unset_func = unset_func;

  /* pspec owns name */
  g_hash_table_insert (properties, (gchar *)pspec->name, node);
}
