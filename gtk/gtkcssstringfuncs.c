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

#include "gtkcssstringfuncsprivate.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo-gobject.h>

#include "gtkcssprovider.h"

/* the actual parsers we have */
#include "gtkanimationdescription.h"
#include "gtk9slice.h"
#include "gtkgradient.h"
#include "gtkthemingengine.h"

typedef gboolean (* FromStringFunc)   (const char    *str,
                                       GFile         *base,
                                       GValue        *value,
                                       GError       **error);
typedef char *   (* ToStringFunc)     (const GValue  *value);

static GHashTable *from_string_funcs = NULL;
static GHashTable *to_string_funcs = NULL;

static void
register_conversion_function (GType          type,
                              FromStringFunc from_string,
                              ToStringFunc   to_string)
{
  if (from_string)
    g_hash_table_insert (from_string_funcs, GSIZE_TO_POINTER (type), from_string);
  if (to_string)
    g_hash_table_insert (to_string_funcs, GSIZE_TO_POINTER (type), to_string);
}

static gboolean
set_default_error (GError **error,
                   GType    type)
{
  g_set_error (error,
               GTK_CSS_PROVIDER_ERROR,
               GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
               "Could not convert property value to type '%s'",
               g_type_name (type));
  return FALSE;
}

/*** IMPLEMENTATIONS ***/

#define SKIP_SPACES(s) while (g_ascii_isspace (*(s))) (s)++
#define SKIP_SPACES_BACK(s) while (g_ascii_isspace (*(s))) (s)--

static GtkSymbolicColor *
symbolic_color_parse_str (const gchar  *string,
                          gchar       **end_ptr)
{
  GtkSymbolicColor *symbolic_color = NULL;
  gchar *str;

  str = (gchar *) string;
  *end_ptr = str;

  if (str[0] == '@')
    {
      const gchar *end;
      gchar *name;

      str++;
      end = str;

      while (*end == '-' || *end == '_' || g_ascii_isalnum (*end))
        end++;

      name = g_strndup (str, end - str);
      symbolic_color = gtk_symbolic_color_new_name (name);
      g_free (name);

      *end_ptr = (gchar *) end;
    }
  else if (g_str_has_prefix (str, "lighter") ||
           g_str_has_prefix (str, "darker"))
    {
      GtkSymbolicColor *param_color;
      gboolean is_lighter = FALSE;

      is_lighter = g_str_has_prefix (str, "lighter");

      if (is_lighter)
        str += strlen ("lighter");
      else
        str += strlen ("darker");

      SKIP_SPACES (str);

      if (*str != '(')
        {
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);
      param_color = symbolic_color_parse_str (str, end_ptr);

      if (!param_color)
        return NULL;

      str = *end_ptr;
      SKIP_SPACES (str);
      *end_ptr = (gchar *) str;

      if (*str != ')')
        {
          gtk_symbolic_color_unref (param_color);
          return NULL;
        }

      if (is_lighter)
        symbolic_color = gtk_symbolic_color_new_shade (param_color, 1.3);
      else
        symbolic_color = gtk_symbolic_color_new_shade (param_color, 0.7);

      gtk_symbolic_color_unref (param_color);
      (*end_ptr)++;
    }
  else if (g_str_has_prefix (str, "shade") ||
           g_str_has_prefix (str, "alpha"))
    {
      GtkSymbolicColor *param_color;
      gboolean is_shade = FALSE;
      gdouble factor;

      is_shade = g_str_has_prefix (str, "shade");

      if (is_shade)
        str += strlen ("shade");
      else
        str += strlen ("alpha");

      SKIP_SPACES (str);

      if (*str != '(')
        {
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);
      param_color = symbolic_color_parse_str (str, end_ptr);

      if (!param_color)
        return NULL;

      str = *end_ptr;
      SKIP_SPACES (str);

      if (str[0] != ',')
        {
          gtk_symbolic_color_unref (param_color);
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);
      factor = g_ascii_strtod (str, end_ptr);

      str = *end_ptr;
      SKIP_SPACES (str);
      *end_ptr = (gchar *) str;

      if (str[0] != ')')
        {
          gtk_symbolic_color_unref (param_color);
          return NULL;
        }

      if (is_shade)
        symbolic_color = gtk_symbolic_color_new_shade (param_color, factor);
      else
        symbolic_color = gtk_symbolic_color_new_alpha (param_color, factor);

      gtk_symbolic_color_unref (param_color);
      (*end_ptr)++;
    }
  else if (g_str_has_prefix (str, "mix"))
    {
      GtkSymbolicColor *color1, *color2;
      gdouble factor;

      str += strlen ("mix");
      SKIP_SPACES (str);

      if (*str != '(')
        {
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);
      color1 = symbolic_color_parse_str (str, end_ptr);

      if (!color1)
        return NULL;

      str = *end_ptr;
      SKIP_SPACES (str);

      if (str[0] != ',')
        {
          gtk_symbolic_color_unref (color1);
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);
      color2 = symbolic_color_parse_str (str, end_ptr);

      if (!color2 || *end_ptr[0] != ',')
        {
          gtk_symbolic_color_unref (color1);
          return NULL;
        }

      str = *end_ptr;
      SKIP_SPACES (str);

      if (str[0] != ',')
        {
          gtk_symbolic_color_unref (color1);
          gtk_symbolic_color_unref (color2);
          *end_ptr = (gchar *) str;
          return NULL;
        }

      str++;
      SKIP_SPACES (str);
      factor = g_ascii_strtod (str, end_ptr);

      str = *end_ptr;
      SKIP_SPACES (str);
      *end_ptr = (gchar *) str;

      if (str[0] != ')')
        {
          gtk_symbolic_color_unref (color1);
          gtk_symbolic_color_unref (color2);
          return NULL;
        }

      symbolic_color = gtk_symbolic_color_new_mix (color1, color2, factor);
      gtk_symbolic_color_unref (color1);
      gtk_symbolic_color_unref (color2);
      (*end_ptr)++;
    }
  else
    {
      GdkRGBA color;
      gchar *color_str;
      const gchar *end;

      end = str + 1;

      if (str[0] == '#')
        {
          /* Color in hex format */
          while (g_ascii_isxdigit (*end))
            end++;
        }
      else if (g_str_has_prefix (str, "rgb"))
        {
          /* color in rgb/rgba format */
          while (*end != ')' && *end != '\0')
            end++;

          if (*end == ')')
            end++;
        }
      else
        {
          /* Color name */
          while (*end != '\0' &&
                 (g_ascii_isalnum (*end) || *end == ' '))
            end++;
        }

      color_str = g_strndup (str, end - str);
      *end_ptr = (gchar *) end;

      if (!gdk_rgba_parse (&color, color_str))
        {
          g_free (color_str);
          return NULL;
        }

      symbolic_color = gtk_symbolic_color_new_literal (&color);
      g_free (color_str);
    }

  return symbolic_color;
}

static gboolean 
rgba_value_from_string (const char  *str,
                        GFile       *base,
                        GValue      *value,
                        GError     **error)
{
  GtkSymbolicColor *symbolic;
  GdkRGBA rgba;

  if (gdk_rgba_parse (&rgba, str))
    {
      g_value_set_boxed (value, &rgba);
      return TRUE;
    }

  symbolic = _gtk_css_parse_symbolic_color (str, error);
  if (symbolic == NULL)
    return FALSE;

  g_value_unset (value);
  g_value_init (value, GTK_TYPE_SYMBOLIC_COLOR);
  g_value_take_boxed (value, symbolic);
  return TRUE;
}

static char *
rgba_value_to_string (const GValue *value)
{
  const GdkRGBA *rgba = g_value_get_boxed (value);

  if (rgba == NULL)
    return g_strdup ("none");

  return gdk_rgba_to_string (rgba);
}

static gboolean 
color_value_from_string (const char  *str,
                         GFile       *base,
                         GValue      *value,
                         GError     **error)
{
  GtkSymbolicColor *symbolic;
  GdkColor color;

  if (gdk_color_parse (str, &color))
    {
      g_value_set_boxed (value, &color);
      return TRUE;
    }

  symbolic = _gtk_css_parse_symbolic_color (str, error);
  if (symbolic == NULL)
    return FALSE;

  g_value_unset (value);
  g_value_init (value, GTK_TYPE_SYMBOLIC_COLOR);
  g_value_take_boxed (value, symbolic);
  return TRUE;
}

static char *
color_value_to_string (const GValue *value)
{
  const GdkColor *color = g_value_get_boxed (value);

  if (color == NULL)
    return g_strdup ("none");

  return gdk_color_to_string (color);
}

static gboolean 
symbolic_color_value_from_string (const char  *str,
                                  GFile       *base,
                                  GValue      *value,
                                  GError     **error)
{
  GtkSymbolicColor *symbolic;

  symbolic = _gtk_css_parse_symbolic_color (str, error);
  if (symbolic == NULL)
    return FALSE;

  g_value_take_boxed (value, symbolic);
  return TRUE;
}

static char *
symbolic_color_value_to_string (const GValue *value)
{
  GtkSymbolicColor *symbolic = g_value_get_boxed (value);

  if (symbolic == NULL)
    return g_strdup ("none");

  return gtk_symbolic_color_to_string (symbolic);
}

static gboolean 
font_description_value_from_string (const char  *str,
                                    GFile       *base,
                                    GValue      *value,
                                    GError     **error)
{
  PangoFontDescription *font_desc;

  font_desc = pango_font_description_from_string (str);
  g_value_take_boxed (value, font_desc);
  return TRUE;
}

static char *
font_description_value_to_string (const GValue *value)
{
  const PangoFontDescription *desc = g_value_get_boxed (value);

  if (desc == NULL)
    return g_strdup ("none");

  return pango_font_description_to_string (desc);
}

static gboolean 
boolean_value_from_string (const char  *str,
                           GFile       *base,
                           GValue      *value,
                           GError     **error)
{
  if (g_ascii_strcasecmp (str, "true") == 0 ||
      g_ascii_strcasecmp (str, "1") == 0)
    {
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  else if (g_ascii_strcasecmp (str, "false") == 0 ||
           g_ascii_strcasecmp (str, "0") == 0)
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }

  return set_default_error (error, G_VALUE_TYPE (value));
}

static char *
boolean_value_to_string (const GValue *value)
{
  if (g_value_get_boolean (value))
    return g_strdup ("true");
  else
    return g_strdup ("false");
}

static gboolean 
int_value_from_string (const char  *str,
                       GFile       *base,
                       GValue      *value,
                       GError     **error)
{
  gint64 i;
  char *end;

  i = g_ascii_strtoll (str, &end, 10);

  if (*end != '\0')
    return set_default_error (error, G_VALUE_TYPE (value));

  if (i > G_MAXINT || i < G_MININT)
    {
      g_set_error_literal (error,
                           GTK_CSS_PROVIDER_ERROR,
                           GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                           "Number too big");
      return FALSE;
    }

  g_value_set_int (value, i);
  return TRUE;
}

static char *
int_value_to_string (const GValue *value)
{
  return g_strdup_printf ("%d", g_value_get_int (value));
}

static gboolean 
uint_value_from_string (const char  *str,
                        GFile       *base,
                        GValue      *value,
                        GError     **error)
{
  guint64 u;
  char *end;

  u = g_ascii_strtoull (str, &end, 10);

  if (*end != '\0')
    return set_default_error (error, G_VALUE_TYPE (value));

  if (u > G_MAXUINT)
    {
      g_set_error_literal (error,
                           GTK_CSS_PROVIDER_ERROR,
                           GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                           "Number too big");
      return FALSE;
    }

  g_value_set_uint (value, u);
  return TRUE;
}

static char *
uint_value_to_string (const GValue *value)
{
  return g_strdup_printf ("%u", g_value_get_uint (value));
}

static gboolean 
double_value_from_string (const char  *str,
                          GFile       *base,
                          GValue      *value,
                          GError     **error)
{
  double d;
  char *end;

  d = g_ascii_strtod (str, &end);

  if (*end != '\0')
    return set_default_error (error, G_VALUE_TYPE (value));

  if (errno == ERANGE)
    {
      g_set_error_literal (error,
                           GTK_CSS_PROVIDER_ERROR,
                           GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                           "Number not representable");
      return FALSE;
    }

  g_value_set_double (value, d);
  return TRUE;
}

static char *
double_value_to_string (const GValue *value)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr (buf, sizeof (buf), g_value_get_double (value));

  return g_strdup (buf);
}

static gboolean 
float_value_from_string (const char  *str,
                         GFile       *base,
                         GValue      *value,
                         GError     **error)
{
  double d;
  char *end;

  d = g_ascii_strtod (str, &end);

  if (*end != '\0')
    return set_default_error (error, G_VALUE_TYPE (value));

  if (errno == ERANGE)
    {
      g_set_error_literal (error,
                           GTK_CSS_PROVIDER_ERROR,
                           GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                           "Number not representable");
      return FALSE;
    }

  g_value_set_float (value, d);
  return TRUE;
}

static char *
float_value_to_string (const GValue *value)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr (buf, sizeof (buf), g_value_get_float (value));

  return g_strdup (buf);
}

static char *
gtk_css_string_unescape (const char  *string,
                         GError     **error)
{
  GString *str;
  char quote;
  gsize len;

  quote = string[0];
  string++;
  if (quote != '\'' && quote != '"')
    {
      g_set_error_literal (error,
                           GTK_CSS_PROVIDER_ERROR,
                           GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                           "String value not properly quoted.");
      return NULL;
    }

  str = g_string_new (NULL);

  while (TRUE)
    {
      len = strcspn (string, "\\'\"\n\r\f");

      g_string_append_len (str, string, len);

      string += len;

      switch (string[0])
        {
        case '\\':
          string++;
          if (string[0] >= '0' && string[0] <= '9' &&
              string[0] >= 'a' && string[0] <= 'f' &&
              string[0] >= 'A' && string[0] <= 'F')
            {
              g_set_error_literal (error,
                                   GTK_CSS_PROVIDER_ERROR,
                                   GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                                   "FIXME: Implement unicode escape sequences.");
              g_string_free (str, TRUE);
              return NULL;
            }
          else if (string[0] == '\r' && string[1] == '\n')
            string++;
          else if (string[0] != '\r' && string[0] != '\n' && string[0] != '\f')
            g_string_append_c (str, string[0]);
          break;
        case '"':
        case '\'':
          if (string[0] != quote)
            {
              g_string_append_c (str, string[0]);
            }
          else
            {
              if (string[1] == 0)
                {
                  return g_string_free (str, FALSE);
                }
              else
                {
                  g_set_error_literal (error,
                                       GTK_CSS_PROVIDER_ERROR,
                                       GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                                       "Junk after end of string.");
                  g_string_free (str, TRUE);
                  return NULL;
                }
            }
          break;
        case '\0':
          g_set_error_literal (error,
                               GTK_CSS_PROVIDER_ERROR,
                               GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                               "Missing end quote in string.");
          g_string_free (str, TRUE);
          return NULL;
        default:
          g_set_error_literal (error,
                               GTK_CSS_PROVIDER_ERROR,
                               GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                               "Invalid character in string. Must be escaped.");
          g_string_free (str, TRUE);
          return NULL;
        }

      string++;
    }

  g_assert_not_reached ();
  return NULL;
}

static gboolean 
string_value_from_string (const char  *str,
                          GFile       *base,
                          GValue      *value,
                          GError     **error)
{
  char *unescaped = gtk_css_string_unescape (str, error);

  if (unescaped == NULL)
    return FALSE;

  g_value_take_string (value, unescaped);
  return TRUE;
}

static char *
string_value_to_string (const GValue *value)
{
  const char *string;
  gsize len;
  GString *str;

  string = g_value_get_string (value);
  str = g_string_new ("\"");

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
  return g_string_free (str, FALSE);
}

static gboolean 
theming_engine_value_from_string (const char  *str,
                                  GFile       *base,
                                  GValue      *value,
                                  GError     **error)
{
  GtkThemingEngine *engine;

  engine = gtk_theming_engine_load (str);
  if (engine == NULL)
    {
      g_set_error (error,
                   GTK_CSS_PROVIDER_ERROR,
                   GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                   "Themeing engine '%s' not found", str);
      return FALSE;
    }

  g_value_set_object (value, engine);
  return TRUE;
}

static char *
theming_engine_value_to_string (const GValue *value)
{
  GtkThemingEngine *engine;
  char *name;

  engine = g_value_get_object (value);
  if (engine == NULL)
    return g_strdup ("none");

  /* XXX: gtk_theming_engine_get_name()? */
  g_object_get (engine, "name", &name, NULL);

  return name;
}

static gboolean 
animation_description_value_from_string (const char  *str,
                                         GFile       *base,
                                         GValue      *value,
                                         GError     **error)
{
  GtkAnimationDescription *desc;

  desc = _gtk_animation_description_from_string (str);

  if (desc == NULL)
    return set_default_error (error, G_VALUE_TYPE (value));
  
  g_value_take_boxed (value, desc);
  return TRUE;
}

static char *
animation_description_value_to_string (const GValue *value)
{
  GtkAnimationDescription *desc = g_value_get_boxed (value);

  if (desc == NULL)
    return g_strdup ("none");

  return _gtk_animation_description_to_string (desc);
}

static gboolean
parse_border_value (const char  *str,
                    gint16      *value,
                    const char **end,
                    GError     **error)
{
  gint64 d;

  d = g_ascii_strtoll (str, (char **) end, 10);

  if (d > G_MAXINT16 || d < 0)
    {
      g_set_error_literal (error,
                           GTK_CSS_PROVIDER_ERROR,
                           GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                           "Number out of range for border");
      return FALSE;
    }

  if (str == *end)
    {
      g_set_error_literal (error,
                           GTK_CSS_PROVIDER_ERROR,
                           GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                           "No number given for border value");
      return FALSE;
    }

  /* Skip optional unit type.
   * We only handle pixels at the moment.
   */
  if (strncmp (*end, "px", 2) == 0)
    *end += 2;

  if (**end != '\0' &&
      !g_ascii_isspace (**end))
    {
      g_set_error_literal (error,
                           GTK_CSS_PROVIDER_ERROR,
                           GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                           "Junk at end of border value");
      return FALSE;
    }

  SKIP_SPACES (*end);

  *value = d;
  return TRUE;
}

static gboolean 
border_value_from_string (const char  *str,
                          GFile       *base,
                          GValue      *value,
                          GError     **error)
{
  GtkBorder *border;

  border = gtk_border_new ();

  if (!parse_border_value (str, &border->top, &str, error))
    return FALSE;

  if (*str == '\0')
    border->right = border->top;
  else
    if (!parse_border_value (str, &border->right, &str, error))
      return FALSE;

  if (*str == '\0')
    border->bottom = border->top;
  else
    if (!parse_border_value (str, &border->bottom, &str, error))
      return FALSE;

  if (*str == '\0')
    border->left = border->right;
  else
    if (!parse_border_value (str, &border->left, &str, error))
      return FALSE;

  if (*str != '\0')
    {
      g_set_error_literal (error,
                           GTK_CSS_PROVIDER_ERROR,
                           GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                           "Junk at end of border value");
      return FALSE;
    }

  g_value_take_boxed (value, border);
  return TRUE;
}

static char *
border_value_to_string (const GValue *value)
{
  const GtkBorder *border = g_value_get_boxed (value);

  if (border == NULL)
    return g_strdup ("none");
  else if (border->left != border->right)
    return g_strdup_printf ("%d %d %d %d", border->top, border->right, border->bottom, border->left);
  else if (border->top != border->bottom)
    return g_strdup_printf ("%d %d %d", border->top, border->right, border->bottom);
  else if (border->top != border->left)
    return g_strdup_printf ("%d %d", border->top, border->right);
  else
    return g_strdup_printf ("%d", border->top);
}

static gboolean 
gradient_value_from_string (const char  *str,
                            GFile       *base,
                            GValue      *value,
                            GError     **error)
{
  GtkGradient *gradient;
  cairo_pattern_type_t type;
  gdouble coords[6];
  gchar *end;
  guint i;

  str += strlen ("-gtk-gradient");
  SKIP_SPACES (str);

  if (*str != '(')
    {
      g_set_error_literal (error,
                           GTK_CSS_PROVIDER_ERROR,
                           GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                           "Expected '(' after '-gtk-gradient'");
      return FALSE;
    }

  str++;
  SKIP_SPACES (str);

  /* Parse gradient type */
  if (g_str_has_prefix (str, "linear"))
    {
      type = CAIRO_PATTERN_TYPE_LINEAR;
      str += strlen ("linear");
    }
  else if (g_str_has_prefix (str, "radial"))
    {
      type = CAIRO_PATTERN_TYPE_RADIAL;
      str += strlen ("radial");
    }
  else
    {
      g_set_error_literal (error,
                           GTK_CSS_PROVIDER_ERROR,
                           GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                           "Gradient type must be 'radial' or 'linear'");
      return FALSE;
    }

  SKIP_SPACES (str);

  /* Parse start/stop position parameters */
  for (i = 0; i < 2; i++)
    {
      if (*str != ',')
        {
          g_set_error_literal (error,
                               GTK_CSS_PROVIDER_ERROR,
                               GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                               "Expected ','");
          return FALSE;
        }

      str++;
      SKIP_SPACES (str);

      if (strncmp (str, "left", 4) == 0)
        {
          coords[i * 3] = 0;
          str += strlen ("left");
        }
      else if (strncmp (str, "right", 5) == 0)
        {
          coords[i * 3] = 1;
          str += strlen ("right");
        }
      else if (strncmp (str, "center", 6) == 0)
        {
          coords[i * 3] = 0.5;
          str += strlen ("center");
        }
      else
        {
          coords[i * 3] = g_ascii_strtod (str, &end);

          if (str == end)
            return set_default_error (error, G_VALUE_TYPE (value));

          str = end;
        }

      SKIP_SPACES (str);

      if (strncmp (str, "top", 3) == 0)
        {
          coords[(i * 3) + 1] = 0;
          str += strlen ("top");
        }
      else if (strncmp (str, "bottom", 6) == 0)
        {
          coords[(i * 3) + 1] = 1;
          str += strlen ("bottom");
        }
      else if (strncmp (str, "center", 6) == 0)
        {
          coords[(i * 3) + 1] = 0.5;
          str += strlen ("center");
        }
      else
        {
          coords[(i * 3) + 1] = g_ascii_strtod (str, &end);

          if (str == end)
            return set_default_error (error, G_VALUE_TYPE (value));

          str = end;
        }

      SKIP_SPACES (str);

      if (type == CAIRO_PATTERN_TYPE_RADIAL)
        {
          /* Parse radius */
          if (*str != ',')
            {
              g_set_error_literal (error,
                                   GTK_CSS_PROVIDER_ERROR,
                                   GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                                   "Expected ','");
              return FALSE;
            }

          str++;
          SKIP_SPACES (str);

          coords[(i * 3) + 2] = g_ascii_strtod (str, &end);
          str = end;

          SKIP_SPACES (str);
        }
    }

  if (type == CAIRO_PATTERN_TYPE_LINEAR)
    gradient = gtk_gradient_new_linear (coords[0], coords[1], coords[3], coords[4]);
  else
    gradient = gtk_gradient_new_radial (coords[0], coords[1], coords[2],
                                        coords[3], coords[4], coords[5]);

  while (*str == ',')
    {
      GtkSymbolicColor *color;
      gdouble position;

      str++;
      SKIP_SPACES (str);

      if (g_str_has_prefix (str, "from"))
        {
          position = 0;
          str += strlen ("from");
          SKIP_SPACES (str);

          if (*str != '(')
            {
              g_object_unref (gradient);
              return set_default_error (error, G_VALUE_TYPE (value));
            }
        }
      else if (g_str_has_prefix (str, "to"))
        {
          position = 1;
          str += strlen ("to");
          SKIP_SPACES (str);

          if (*str != '(')
            {
              g_object_unref (gradient);
              return set_default_error (error, G_VALUE_TYPE (value));
            }
        }
      else if (g_str_has_prefix (str, "color-stop"))
        {
          str += strlen ("color-stop");
          SKIP_SPACES (str);

          if (*str != '(')
            {
              g_object_unref (gradient);
              return set_default_error (error, G_VALUE_TYPE (value));
            }

          str++;
          SKIP_SPACES (str);

          position = g_ascii_strtod (str, &end);

          str = end;
          SKIP_SPACES (str);

          if (*str != ',')
            {
              g_object_unref (gradient);
              return set_default_error (error, G_VALUE_TYPE (value));
            }
        }
      else
        {
          g_object_unref (gradient);
          return set_default_error (error, G_VALUE_TYPE (value));
        }

      str++;
      SKIP_SPACES (str);

      color = symbolic_color_parse_str (str, &end);

      str = end;
      SKIP_SPACES (str);

      if (*str != ')')
        {
          if (color)
            gtk_symbolic_color_unref (color);
          g_object_unref (gradient);
          return set_default_error (error, G_VALUE_TYPE (value));
        }

      str++;
      SKIP_SPACES (str);

      if (color)
        {
          gtk_gradient_add_color_stop (gradient, position, color);
          gtk_symbolic_color_unref (color);
        }
    }

  if (*str != ')')
    {
      g_object_unref (gradient);
      return set_default_error (error, G_VALUE_TYPE (value));
    }

  g_value_take_boxed (value, gradient);
  return TRUE;
}

static char *
gradient_value_to_string (const GValue *value)
{
  GtkGradient *gradient = g_value_get_boxed (value);

  if (gradient == NULL)
    return g_strdup ("none");

  return gtk_gradient_to_string (gradient);
}

static gboolean 
pattern_value_from_string (const char  *str,
                           GFile       *base,
                           GValue      *value,
                           GError     **error)
{
  if (g_str_has_prefix (str, "-gtk-gradient"))
    {
      g_value_unset (value);
      g_value_init (value, GTK_TYPE_GRADIENT);
      return gradient_value_from_string (str, base, value, error);
    }
  else
    {
      gchar *path;
      GdkPixbuf *pixbuf;
      GFile *file;

      file = _gtk_css_parse_url (base, str, NULL, error);
      if (file == NULL)
        return FALSE;

      path = g_file_get_path (file);
      g_object_unref (file);

      pixbuf = gdk_pixbuf_new_from_file (path, error);
      g_free (path);
      if (pixbuf == NULL)
        return FALSE;

      cairo_surface_t *surface;
      cairo_pattern_t *pattern;
      cairo_t *cr;
      cairo_matrix_t matrix;

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

static gboolean 
slice_value_from_string (const char  *str,
                         GFile       *base,
                         GValue      *value,
                         GError     **error)
{
  gdouble distance_top, distance_bottom;
  gdouble distance_left, distance_right;
  GtkSliceSideModifier mods[2];
  GdkPixbuf *pixbuf;
  Gtk9Slice *slice;
  GFile *file;
  gint i = 0;
  char *path;

  SKIP_SPACES (str);

  /* Parse image url */
  file = _gtk_css_parse_url (base, str, (char **) &str, error);
  if (!file)
      return FALSE;

  SKIP_SPACES (str);

  /* Parse top/left/bottom/right distances */
  distance_top = g_ascii_strtod (str, (char **) &str);

  SKIP_SPACES (str);

  distance_right = g_ascii_strtod (str, (char **) &str);

  SKIP_SPACES (str);

  distance_bottom = g_ascii_strtod (str, (char **) &str);

  SKIP_SPACES (str);

  distance_left = g_ascii_strtod (str, (char **) &str);

  SKIP_SPACES (str);

  while (*str && i < 2)
    {
      if (g_str_has_prefix (str, "stretch"))
        {
          str += strlen ("stretch");
          mods[i] = GTK_SLICE_STRETCH;
        }
      else if (g_str_has_prefix (str, "repeat"))
        {
          str += strlen ("repeat");
          mods[i] = GTK_SLICE_REPEAT;
        }
      else
        {
          g_object_unref (file);
          return set_default_error (error, G_VALUE_TYPE (value));
        }

      SKIP_SPACES (str);
      i++;
    }

  if (*str != '\0')
    {
      g_object_unref (file);
      return set_default_error (error, G_VALUE_TYPE (value));
    }

  if (i != 2)
    {
      /* Fill in second modifier, same as the first */
      mods[1] = mods[0];
    }

  path = g_file_get_path (file);
  pixbuf = gdk_pixbuf_new_from_file (path, error);
  g_free (path);
  if (!pixbuf)
    return FALSE;

  slice = _gtk_9slice_new (pixbuf,
                           distance_top, distance_bottom,
                           distance_left, distance_right,
                           mods[0], mods[1]);
  g_object_unref (pixbuf);

  g_value_take_boxed (value, slice);
  return TRUE;
}

static gboolean 
enum_value_from_string (const char  *str,
                        GFile       *base,
                        GValue      *value,
                        GError     **error)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  enum_class = g_type_class_ref (G_VALUE_TYPE (value));
  enum_value = g_enum_get_value_by_nick (enum_class, str);

  if (!enum_value)
    {
      g_set_error (error,
                   GTK_CSS_PROVIDER_ERROR,
                   GTK_CSS_PROVIDER_ERROR_FAILED,
                   "Unknown value '%s' for enum type '%s'",
                   str, g_type_name (G_VALUE_TYPE (value)));
      g_type_class_unref (enum_class);
      return FALSE;
    }
  
  g_value_set_enum (value, enum_value->value);
  g_type_class_unref (enum_class);
  return TRUE;
}

static char *
enum_value_to_string (const GValue *value)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;
  char *s;

  enum_class = g_type_class_ref (G_VALUE_TYPE (value));
  enum_value = g_enum_get_value (enum_class, g_value_get_enum (value));

  s = g_strdup (enum_value->value_nick);

  g_type_class_unref (enum_class);

  return s;
}

static gboolean 
flags_value_from_string (const char  *str,
                         GFile       *base,
                         GValue      *value,
                         GError     **error)
{
  GFlagsClass *flags_class;
  GFlagsValue *flag_value;
  guint flags = 0;
  char **strv;
  guint i;

  strv = g_strsplit (str, ",", -1);

  flags_class = g_type_class_ref (G_VALUE_TYPE (value));

  for (i = 0; strv[i]; i++)
    {
      strv[i] = g_strstrip (strv[i]);

      flag_value = g_flags_get_value_by_nick (flags_class, strv[i]);
      if (!flag_value)
        {
          g_set_error (error,
                       GTK_CSS_PROVIDER_ERROR,
                       GTK_CSS_PROVIDER_ERROR_PROPERTY_NAME,
                       "Unknown flag value '%s' for type '%s'",
                       strv[i], g_type_name (G_VALUE_TYPE (value)));
          g_type_class_unref (flags_class);
          return FALSE;
        }
      
      flags |= flag_value->value;
    }

  g_strfreev (strv);
  g_type_class_unref (flags_class);

  g_value_set_enum (value, flags);

  return TRUE;
}

static char *
flags_value_to_string (const GValue *value)
{
  GFlagsClass *flags_class;
  GString *string;
  guint i, flags;

  flags_class = g_type_class_ref (G_VALUE_TYPE (value));
  flags = g_value_get_flags (value);
  string = g_string_new (NULL);

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

  return g_string_free (string, FALSE);
}

/*** API ***/

static void
css_string_funcs_init (void)
{
  if (G_LIKELY (from_string_funcs != NULL))
    return;

  from_string_funcs = g_hash_table_new (NULL, NULL);
  to_string_funcs = g_hash_table_new (NULL, NULL);

  register_conversion_function (GDK_TYPE_RGBA,
                                rgba_value_from_string,
                                rgba_value_to_string);
  register_conversion_function (GDK_TYPE_COLOR,
                                color_value_from_string,
                                color_value_to_string);
  register_conversion_function (GTK_TYPE_SYMBOLIC_COLOR,
                                symbolic_color_value_from_string,
                                symbolic_color_value_to_string);
  register_conversion_function (PANGO_TYPE_FONT_DESCRIPTION,
                                font_description_value_from_string,
                                font_description_value_to_string);
  register_conversion_function (G_TYPE_BOOLEAN,
                                boolean_value_from_string,
                                boolean_value_to_string);
  register_conversion_function (G_TYPE_INT,
                                int_value_from_string,
                                int_value_to_string);
  register_conversion_function (G_TYPE_UINT,
                                uint_value_from_string,
                                uint_value_to_string);
  register_conversion_function (G_TYPE_DOUBLE,
                                double_value_from_string,
                                double_value_to_string);
  register_conversion_function (G_TYPE_FLOAT,
                                float_value_from_string,
                                float_value_to_string);
  register_conversion_function (G_TYPE_STRING,
                                string_value_from_string,
                                string_value_to_string);
  register_conversion_function (GTK_TYPE_THEMING_ENGINE,
                                theming_engine_value_from_string,
                                theming_engine_value_to_string);
  register_conversion_function (GTK_TYPE_ANIMATION_DESCRIPTION,
                                animation_description_value_from_string,
                                animation_description_value_to_string);
  register_conversion_function (GTK_TYPE_BORDER,
                                border_value_from_string,
                                border_value_to_string);
  register_conversion_function (GTK_TYPE_GRADIENT,
                                gradient_value_from_string,
                                gradient_value_to_string);
  register_conversion_function (CAIRO_GOBJECT_TYPE_PATTERN,
                                pattern_value_from_string,
                                NULL);
  register_conversion_function (GTK_TYPE_9SLICE,
                                slice_value_from_string,
                                NULL);
  register_conversion_function (G_TYPE_ENUM,
                                enum_value_from_string,
                                enum_value_to_string);
  register_conversion_function (G_TYPE_FLAGS,
                                flags_value_from_string,
                                flags_value_to_string);
}

gboolean
_gtk_css_value_from_string (GValue        *value,
                            GFile         *base,
                            const char    *string,
                            GError       **error)
{
  FromStringFunc func;

  g_return_val_if_fail (string != NULL, FALSE);
  g_return_val_if_fail (string[0] != 0, FALSE);

  css_string_funcs_init ();

  func = g_hash_table_lookup (from_string_funcs,
                              GSIZE_TO_POINTER (G_VALUE_TYPE (value)));
  if (func == NULL)
    func = g_hash_table_lookup (from_string_funcs,
                                GSIZE_TO_POINTER (g_type_fundamental (G_VALUE_TYPE (value))));

  if (func == NULL)
    {
      g_set_error (error,
                   GTK_CSS_PROVIDER_ERROR,
                   GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                   "Cannot convert to type '%s'",
                   g_type_name (G_VALUE_TYPE (value)));
      return FALSE;
    }

  return (*func) (string, base, value, error);
}

char *
_gtk_css_value_to_string (const GValue *value)
{
  ToStringFunc func;

  css_string_funcs_init ();

  func = g_hash_table_lookup (to_string_funcs,
                              GSIZE_TO_POINTER (G_VALUE_TYPE (value)));
  if (func == NULL)
    func = g_hash_table_lookup (to_string_funcs,
                                GSIZE_TO_POINTER (g_type_fundamental (G_VALUE_TYPE (value))));

  if (func)
    return func (value);

  return g_strdup_value_contents (value);
}

GtkSymbolicColor *
_gtk_css_parse_symbolic_color (const char    *str,
                               GError       **error)
{
  GtkSymbolicColor *color;
  gchar *end;

  color = symbolic_color_parse_str (str, &end);

  if (*end != '\0')
    {
      if (color)
        {
          gtk_symbolic_color_unref (color);
          color = NULL;
        }

      g_set_error_literal (error,
                           GTK_CSS_PROVIDER_ERROR,
                           GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                           "Failed to parse symbolic color");
    }

  return color;
}

GFile *
_gtk_css_parse_url (GFile       *base,
                    const char  *str,
                    char       **end,
                    GError     **error)
{
  gchar *path, *chr;
  GFile *file;

  if (g_str_has_prefix (str, "url"))
    {
      str += strlen ("url");
      SKIP_SPACES (str);

      if (*str != '(')
        {
          g_set_error_literal (error,
                               GTK_CSS_PROVIDER_ERROR,
                               GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                               "Expected '(' after 'url'");
          return NULL;
        }

      chr = strchr (str, ')');
      if (!chr)
        {
          g_set_error_literal (error,
                               GTK_CSS_PROVIDER_ERROR,
                               GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                               "No closing ')' found for 'url'");
          return NULL;
        }

      if (end)
        *end = chr + 1;

      str++;
      SKIP_SPACES (str);

      if (*str == '"' || *str == '\'')
        {
          const gchar *p;
          p = str;

          str++;
          chr--;
          SKIP_SPACES_BACK (chr);

          if (*chr != *p || chr == p)
            {
              g_set_error_literal (error,
                                   GTK_CSS_PROVIDER_ERROR,
                                   GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                                   "Did not find closing quote for url");
              return NULL;
            }
        }
      else
        {
          g_set_error_literal (error,
                               GTK_CSS_PROVIDER_ERROR,
                               GTK_CSS_PROVIDER_ERROR_PROPERTY_VALUE,
                               "url not properly escaped");
          return NULL;
        }

      path = g_strndup (str, chr - str);
      g_strstrip (path);
    }
  else
    {
      path = g_strdup (str);
      if (end)
        *end = (gchar *) str + strlen (str);
    }

  file = g_file_resolve_relative_path (base, path);
  g_free (path);

  return file;
}
