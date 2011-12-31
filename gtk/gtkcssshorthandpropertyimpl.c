/*
 * Copyright © 2011 Red Hat Inc.
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkcssshorthandpropertyprivate.h"

#include <cairo-gobject.h>
#include <math.h>

#include "gtkborderimageprivate.h"
#include "gtkcsstypesprivate.h"

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

/*** PARSING ***/

static gboolean
border_image_value_parse (GtkCssParser *parser,
                          GFile *base,
                          GValue *value)
{
  GValue temp = G_VALUE_INIT;
  cairo_pattern_t *pattern = NULL;
  gconstpointer *boxed = NULL;
  GType boxed_type;
  GtkBorder slice, *width = NULL, *parsed_slice;
  GtkCssBorderImageRepeat repeat, *parsed_repeat;
  gboolean retval = FALSE;
  GtkBorderImage *image = NULL;

  if (_gtk_css_parser_try (parser, "none", TRUE))
    return TRUE;

  g_value_init (&temp, CAIRO_GOBJECT_TYPE_PATTERN);

  if (!_gtk_style_property_parse_value (NULL, &temp, parser, base))
    return FALSE;

  boxed_type = G_VALUE_TYPE (&temp);
  if (boxed_type != CAIRO_GOBJECT_TYPE_PATTERN)
    boxed = g_value_dup_boxed (&temp);
  else
    pattern = g_value_dup_boxed (&temp);

  g_value_unset (&temp);
  g_value_init (&temp, GTK_TYPE_BORDER);

  if (!_gtk_style_property_parse_value (NULL, &temp, parser, base))
    goto out;

  parsed_slice = g_value_get_boxed (&temp);
  slice = *parsed_slice;

  if (_gtk_css_parser_try (parser, "/", TRUE))
    {
      g_value_unset (&temp);
      g_value_init (&temp, GTK_TYPE_BORDER);

      if (!_gtk_style_property_parse_value (NULL, &temp, parser, base))
        goto out;

      width = g_value_dup_boxed (&temp);
    }

  g_value_unset (&temp);
  g_value_init (&temp, GTK_TYPE_CSS_BORDER_IMAGE_REPEAT);

  if (!_gtk_style_property_parse_value (NULL, &temp, parser, base))
    goto out;

  parsed_repeat = g_value_get_boxed (&temp);
  repeat = *parsed_repeat;

  g_value_unset (&temp);

  if (boxed != NULL)
    image = _gtk_border_image_new_for_boxed (boxed_type, boxed, &slice, width, &repeat);
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

  if (boxed != NULL)
    g_boxed_free (boxed_type, boxed);

  if (width != NULL)
    gtk_border_free (width);

  return retval;
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

/*** PRINTING ***/

static void
string_append_double (GString *string,
                      double   d)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr (buf, sizeof (buf), d);
  g_string_append (string, buf);
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
                   GtkStateFlags       state,
		   GtkStylePropertyContext *context)
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
              GtkStateFlags       state,
	      GtkStylePropertyContext *context)
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
             GtkStateFlags       state,
	     GtkStylePropertyContext *context)
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
                    GtkStateFlags       state,
		    GtkStylePropertyContext *context)
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
                       GtkStateFlags       state,
		       GtkStylePropertyContext *context)
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
                   GtkStateFlags       state,
		   GtkStylePropertyContext *context)
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

static void
_gtk_css_shorthand_property_register (GParamSpec               *pspec,
                                      const char              **subproperties,
                                      GtkStylePropertyFlags     flags,
                                      GtkStylePropertyParser    property_parse_func,
                                      GtkStyleUnpackFunc        unpack_func,
                                      GtkStylePackFunc          pack_func,
                                      GtkStyleParseFunc         parse_func,
                                      GtkStylePrintFunc         print_func,
                                      const GValue *            initial_value,
                                      GtkStyleUnsetFunc         unset_func)
{
  GtkStyleProperty *node;

  g_return_if_fail (pack_func != NULL);
  g_return_if_fail (unpack_func != NULL);

  node = g_object_new (GTK_TYPE_CSS_SHORTHAND_PROPERTY,
                       "name", pspec->name,
                       "value-type", pspec->value_type,
                       "subproperties", subproperties,
                       NULL);

  node->flags = flags;
  node->pspec = pspec;
  node->property_parse_func = property_parse_func;
  node->pack_func = pack_func;
  node->unpack_func = unpack_func;
  node->parse_func = parse_func;
  node->print_func = print_func;
  node->unset_func = unset_func;
}

void
_gtk_css_shorthand_property_init_properties (void)
{
  const char *font_subproperties[] = { "font-family", "font-style", "font-variant", "font-weight", "font-size", NULL };
  const char *margin_subproperties[] = { "margin-top", "margin-right", "margin-bottom", "margin-left", NULL };
  const char *padding_subproperties[] = { "padding-top", "padding-right", "padding-bottom", "padding-left", NULL };
  const char *border_width_subproperties[] = { "border-top-width", "border-right-width", "border-bottom-width", "border-left-width", NULL };
  const char *border_radius_subproperties[] = { "border-top-left-radius", "border-top-right-radius",
                                                "border-bottom-right-radius", "border-bottom-left-radius", NULL };
  const char *border_color_subproperties[] = { "border-top-color", "border-right-color", "border-bottom-color", "border-left-color", NULL };
  const char *border_image_subproperties[] = { "border-image-source", "border-image-slice", "border-image-width", "border-image-repeat", NULL };

  _gtk_css_shorthand_property_register   (g_param_spec_boxed ("font",
                                                              "Font Description",
                                                              "Font Description",
                                                              PANGO_TYPE_FONT_DESCRIPTION, 0),
                                          font_subproperties,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          unpack_font_description,
                                          pack_font_description,
                                          NULL,
                                          NULL,
                                          NULL,
                                          unset_font_description);
  _gtk_css_shorthand_property_register   (g_param_spec_boxed ("margin",
                                                              "Margin",
                                                              "Margin",
                                                              GTK_TYPE_BORDER, 0),
                                          margin_subproperties,
                                          0,
                                          NULL,
                                          unpack_margin,
                                          pack_margin,
                                          NULL,
                                          NULL,
                                          NULL,
                                          unset_margin);
  _gtk_css_shorthand_property_register   (g_param_spec_boxed ("padding",
                                                              "Padding",
                                                              "Padding",
                                                              GTK_TYPE_BORDER, 0),
                                          padding_subproperties,
                                          0,
                                          NULL,
                                          unpack_padding,
                                          pack_padding,
                                          NULL,
                                          NULL,
                                          NULL,
                                          unset_padding);
  _gtk_css_shorthand_property_register   (g_param_spec_boxed ("border-width",
                                                              "Border width",
                                                              "Border width, in pixels",
                                                              GTK_TYPE_BORDER, 0),
                                          border_width_subproperties,
                                          0,
                                          NULL,
                                          unpack_border_width,
                                          pack_border_width,
                                          NULL,
                                          NULL,
                                          NULL,
                                          unset_border_width);
  _gtk_css_shorthand_property_register   (g_param_spec_int ("border-radius",
                                                            "Border radius",
                                                            "Border radius, in pixels",
                                                            0, G_MAXINT, 0, 0),
                                          border_radius_subproperties,
                                          0,
                                          NULL,
                                          unpack_border_radius,
                                          pack_border_radius,
                                          border_radius_value_parse,
                                          border_radius_value_print,
                                          NULL,
                                          unset_border_radius);
  _gtk_css_shorthand_property_register   (g_param_spec_boxed ("border-color",
                                                              "Border color",
                                                              "Border color",
                                                              GDK_TYPE_RGBA, 0),
                                          border_color_subproperties,
                                          0,
                                          NULL,
                                          unpack_border_color,
                                          pack_border_color,
                                          border_color_shorthand_value_parse,
                                          NULL,
                                          NULL,
                                          unset_border_color);
  _gtk_css_shorthand_property_register   (g_param_spec_boxed ("border-image",
                                                              "Border Image",
                                                              "Border Image",
                                                              GTK_TYPE_BORDER_IMAGE, 0),
                                          border_image_subproperties,
                                          0,
                                          NULL,
                                          _gtk_border_image_unpack,
                                          _gtk_border_image_pack,
                                          border_image_value_parse,
                                          NULL,
                                          NULL,
                                          unset_border_image);
}
