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

#include <gobject/gvaluecollector.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo-gobject.h>

#include "gtkcssparserprivate.h"
#include "gtkcssstylefuncsprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkintl.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkstylepropertiesprivate.h"

/* the actual parsers we have */
#include "gtkanimationdescription.h"
#include "gtkbindings.h"
#include "gtkgradient.h"
#include "gtkshadowprivate.h"
#include "gtkthemingengine.h"
#include "gtktypebuiltins.h"
#include "gtkwin32themeprivate.h"

/*** REGISTRATION ***/

static void
_gtk_style_property_register (const char *                 name,
                              GType                        value_type,
                              GtkStylePropertyFlags        flags,
                              GtkCssStylePropertyParseFunc parse_value,
                              GtkCssStylePropertyPrintFunc print_value,
                              const GValue *               initial_value)
{
  GtkCssStyleProperty *node;

  node = g_object_new (GTK_TYPE_CSS_STYLE_PROPERTY,
                       "inherit", (flags & GTK_STYLE_PROPERTY_INHERIT) ? TRUE : FALSE,
                       "initial-value", initial_value,
                       "name", name,
                       "value-type", value_type,
                       NULL);

  if (parse_value)
    node->parse_value = parse_value;
  if (print_value)
    node->print_value = print_value;
}

static void
gtk_style_property_register (const char *                 name,
                             GType                        value_type,
                             GtkStylePropertyFlags        flags,
                             GtkCssStylePropertyParseFunc parse_value,
                             GtkCssStylePropertyPrintFunc print_value,
                             ...)
{
  GValue initial_value = G_VALUE_INIT;
  char *error = NULL;
  va_list args;

  va_start (args, print_value);
  G_VALUE_COLLECT_INIT (&initial_value, value_type,
                        args, 0, &error);
  if (error)
    {
      g_error ("property `%s' initial value is broken: %s", name, error);
      g_value_unset (&initial_value);
      return;
    }

  va_end (args);

  _gtk_style_property_register (name, value_type, flags, parse_value, print_value, &initial_value);

  g_value_unset (&initial_value);
}

/*** HELPERS ***/

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
font_family_parse (GtkCssStyleProperty *property,
                   GValue              *value,
                   GtkCssParser        *parser,
                   GFile               *base)
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
font_family_value_print (GtkCssStyleProperty *property,
                         const GValue        *value,
                         GString             *string)
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
bindings_value_parse (GtkCssStyleProperty *property,
                      GValue              *value,
                      GtkCssParser        *parser,
                      GFile               *base)
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
bindings_value_print (GtkCssStyleProperty *property,
                      const GValue        *value,
                      GString             *string)
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
border_corner_radius_value_parse (GtkCssStyleProperty *property,
                                  GValue              *value,
                                  GtkCssParser        *parser,
                                  GFile               *base)
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
border_corner_radius_value_print (GtkCssStyleProperty *property,
                                  const GValue        *value,
                                  GString             *string)
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

/*** REGISTRATION ***/

#define rgba_init(rgba, r, g, b, a) G_STMT_START{ \
  (rgba)->red = (r); \
  (rgba)->green = (g); \
  (rgba)->blue = (b); \
  (rgba)->alpha = (a); \
}G_STMT_END
void
_gtk_css_style_property_init_properties (void)
{
  GValue value = { 0, };
  char *default_font_family[] = { "Sans", NULL };
  GdkRGBA rgba;
  GtkCssBorderCornerRadius no_corner_radius = { 0, };
  GtkBorder border_of_ones = { 1, 1, 1, 1 };
  GtkCssBackgroundRepeat background_repeat = { GTK_CSS_BACKGROUND_REPEAT_STYLE_REPEAT };
  GtkCssBorderImageRepeat border_image_repeat = { GTK_CSS_REPEAT_STYLE_STRETCH, GTK_CSS_REPEAT_STYLE_STRETCH };

  /* note that gtk_style_properties_register_property() calls this function,
   * so make sure we're sanely inited to avoid infloops */

  rgba_init (&rgba, 1, 1, 1, 1);
  gtk_style_property_register            ("color",
                                          GDK_TYPE_RGBA,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          &rgba);
  rgba_init (&rgba, 0, 0, 0, 0);
  gtk_style_property_register            ("background-color",
                                          GDK_TYPE_RGBA,
                                          0,
                                          NULL,
                                          NULL,
                                          &rgba);

  gtk_style_property_register            ("font-family",
                                          G_TYPE_STRV,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          font_family_parse,
                                          font_family_value_print,
                                          default_font_family);
  gtk_style_property_register            ("font-style",
                                          PANGO_TYPE_STYLE,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          PANGO_STYLE_NORMAL);
  gtk_style_property_register            ("font-variant",
                                          PANGO_TYPE_VARIANT,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          PANGO_VARIANT_NORMAL);
  /* xxx: need to parse this properly, ie parse the numbers */
  gtk_style_property_register            ("font-weight",
                                          PANGO_TYPE_WEIGHT,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          PANGO_WEIGHT_NORMAL);
  gtk_style_property_register            ("font-size",
                                          G_TYPE_DOUBLE,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          10.0);

  gtk_style_property_register            ("text-shadow",
                                          GTK_TYPE_SHADOW,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL);

  gtk_style_property_register            ("icon-shadow",
                                          GTK_TYPE_SHADOW,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL);

  gtk_style_property_register            ("box-shadow",
                                          GTK_TYPE_SHADOW,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL);

  gtk_style_property_register            ("margin-top",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("margin-left",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("margin-bottom",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("margin-right",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("padding-top",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("padding-left",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("padding-bottom",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("padding-right",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("border-top-width",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("border-left-width",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("border-bottom-width",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("border-right-width",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          0);

  gtk_style_property_register            ("border-top-left-radius",
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          0,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          &no_corner_radius);
  gtk_style_property_register            ("border-top-right-radius",
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          0,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          &no_corner_radius);
  gtk_style_property_register            ("border-bottom-right-radius",
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          0,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          &no_corner_radius);
  gtk_style_property_register            ("border-bottom-left-radius",
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          0,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          &no_corner_radius);

  gtk_style_property_register            ("border-style",
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          NULL,
                                          NULL,
                                          GTK_BORDER_STYLE_NONE);
  gtk_style_property_register            ("background-clip",
                                          GTK_TYPE_CSS_AREA,
                                          0,
                                          NULL,
                                          NULL,
                                          GTK_CSS_AREA_BORDER_BOX);
                                        
  gtk_style_property_register            ("background-origin",
                                          GTK_TYPE_CSS_AREA,
                                          0,
                                          NULL,
                                          NULL,
                                          GTK_CSS_AREA_PADDING_BOX);

  g_value_init (&value, GTK_TYPE_CSS_SPECIAL_VALUE);
  g_value_set_enum (&value, GTK_CSS_CURRENT_COLOR);
  _gtk_style_property_register           ("border-top-color",
                                          GDK_TYPE_RGBA,
                                          0,
                                          NULL,
                                          NULL,
                                          &value);
  _gtk_style_property_register           ("border-right-color",
                                          GDK_TYPE_RGBA,
                                          0,
                                          NULL,
                                          NULL,
                                          &value);
  _gtk_style_property_register           ("border-bottom-color",
                                          GDK_TYPE_RGBA,
                                          0,
                                          NULL,
                                          NULL,
                                          &value);
  _gtk_style_property_register           ("border-left-color",
                                          GDK_TYPE_RGBA,
                                          0,
                                          NULL,
                                          NULL,
                                          &value);
  g_value_unset (&value);

  gtk_style_property_register            ("background-image",
                                          CAIRO_GOBJECT_TYPE_PATTERN,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL);
  gtk_style_property_register            ("background-repeat",
                                          GTK_TYPE_CSS_BACKGROUND_REPEAT,
                                          0,
                                          NULL,
                                          NULL,
                                          &background_repeat);

  gtk_style_property_register            ("border-image-source",
                                          CAIRO_GOBJECT_TYPE_PATTERN,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL);
  gtk_style_property_register            ("border-image-repeat",
                                          GTK_TYPE_CSS_BORDER_IMAGE_REPEAT,
                                          0,
                                          NULL,
                                          NULL,
                                          &border_image_repeat);

  /* XXX: The initial vaue is wrong, it should be 100% */
  gtk_style_property_register            ("border-image-slice",
                                          GTK_TYPE_BORDER,
                                          0,
                                          NULL,
                                          NULL,
                                          &border_of_ones);
  gtk_style_property_register            ("border-image-width",
                                          GTK_TYPE_BORDER,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL);
  gtk_style_property_register            ("engine",
                                          GTK_TYPE_THEMING_ENGINE,
                                          0,
                                          NULL,
                                          NULL,
                                          gtk_theming_engine_load (NULL));
  gtk_style_property_register            ("transition",
                                          GTK_TYPE_ANIMATION_DESCRIPTION,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL);

  /* Private property holding the binding sets */
  gtk_style_property_register            ("gtk-key-bindings",
                                          G_TYPE_PTR_ARRAY,
                                          0,
                                          bindings_value_parse,
                                          bindings_value_print,
                                          NULL);
}

