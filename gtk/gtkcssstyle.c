/*
 * Copyright © 2012 Red Hat Inc.
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkprivate.h"
#include "gtkcssstyleprivate.h"

#include "gtkcssanimationprivate.h"
#include "gtkcssarrayvalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssinheritvalueprivate.h"
#include "gtkcssinitialvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcssstringvalueprivate.h"
#include "gtkcssfontfeaturesvalueprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstransitionprivate.h"
#include "gtkstyleanimationprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstyleproviderprivate.h"

G_DEFINE_ABSTRACT_TYPE (GtkCssStyle, gtk_css_style, G_TYPE_OBJECT)

static GtkCssSection *
gtk_css_style_real_get_section (GtkCssStyle *style,
                                guint        id)
{
  return NULL;
}

static gboolean
gtk_css_style_real_is_static (GtkCssStyle *style)
{
  return TRUE;
}

static void
gtk_css_style_class_init (GtkCssStyleClass *klass)
{
  klass->get_section = gtk_css_style_real_get_section;
  klass->is_static = gtk_css_style_real_is_static;
}

static void
gtk_css_style_init (GtkCssStyle *style)
{
}

GtkCssValue *
gtk_css_style_get_value (GtkCssStyle *style,
                          guint        id)
{
  return GTK_CSS_STYLE_GET_CLASS (style)->get_value (style, id);
}

GtkCssSection *
gtk_css_style_get_section (GtkCssStyle *style,
                           guint        id)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE (style), NULL);

  return GTK_CSS_STYLE_GET_CLASS (style)->get_section (style, id);
}

gboolean
gtk_css_style_is_static (GtkCssStyle *style)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE (style), TRUE);

  return GTK_CSS_STYLE_GET_CLASS (style)->is_static (style);
}

/*
 * gtk_css_style_print:
 * @style: a #GtkCssStyle
 * @string: the #GString to print to
 * @indent: level of indentation to use
 * @skip_initial: %TRUE to skip properties that have their initial value
 *
 * Print the @style to @string, in CSS format. Every property is printed
 * on a line by itself, indented by @indent spaces. If @skip_initial is
 * %TRUE, properties are only printed if their value in @style is different
 * from the initial value of the property.
 *
 * Returns: %TRUE is any properties have been printed
 */
gboolean
gtk_css_style_print (GtkCssStyle *style,
                     GString     *string,
                     guint        indent,
                     gboolean     skip_initial)
{
  guint i;
  gboolean retval = FALSE;

  g_return_val_if_fail (GTK_IS_CSS_STYLE (style), FALSE);
  g_return_val_if_fail (string != NULL, FALSE);

  for (i = 0; i < _gtk_css_style_property_get_n_properties (); i++)
    {
      GtkCssSection *section;
      GtkCssStyleProperty *prop;
      GtkCssValue *value;
      const char *name;

      section = gtk_css_style_get_section (style, i);
      if (!section && skip_initial)
        continue;

      prop = _gtk_css_style_property_lookup_by_id (i);
      name = _gtk_style_property_get_name (GTK_STYLE_PROPERTY (prop));
      value = gtk_css_style_get_value (style, i);

      g_string_append_printf (string, "%*s%s: ", indent, "", name);
      _gtk_css_value_print (value, string);
      g_string_append_c (string, ';');

      if (section)
        {
          g_string_append (string, " /* ");
          gtk_css_section_print (section, string);
          g_string_append (string, " */");
        }

      g_string_append_c (string, '\n');

      retval = TRUE;
    }

  return retval;
}

char *
gtk_css_style_to_string (GtkCssStyle *style)
{
  GString *string;

  g_return_val_if_fail (GTK_IS_CSS_STYLE (style), NULL);

  string = g_string_new ("");

  gtk_css_style_print (style, string, 0, FALSE);

  return g_string_free (string, FALSE);
}

static PangoUnderline
get_pango_underline_from_style (GtkTextDecorationStyle style)
{
  switch (style)
    {
    case GTK_CSS_TEXT_DECORATION_STYLE_DOUBLE:
      return PANGO_UNDERLINE_DOUBLE;
    case GTK_CSS_TEXT_DECORATION_STYLE_WAVY:
      return PANGO_UNDERLINE_ERROR;
    case GTK_CSS_TEXT_DECORATION_STYLE_SOLID:
    default:
      return PANGO_UNDERLINE_SINGLE;
    }

  g_return_val_if_reached (PANGO_UNDERLINE_SINGLE);
}

static PangoAttrList *
add_pango_attr (PangoAttrList  *attrs,
                PangoAttribute *attr)
{
  if (attrs == NULL)
    attrs = pango_attr_list_new ();

  pango_attr_list_insert (attrs, attr);

  return attrs;
}

static void
append_separated (GString *s, const char *text)
{
  if (s->len > 0)
    g_string_append (s, ", ");
  g_string_append (s, text);
}

PangoAttrList *
gtk_css_style_get_pango_attributes (GtkCssStyle *style)
{
  PangoAttrList *attrs = NULL;
  GtkTextDecorationLine decoration_line;
  GtkTextDecorationStyle decoration_style;
  const GdkRGBA *color;
  const GdkRGBA *decoration_color;
  gint letter_spacing;
  GtkCssValue *value;
  GtkCssFontVariantLigature ligatures;
  GtkCssFontVariantNumeric numeric;
  GtkCssFontVariantEastAsian east_asian;
  GString *s;
  char *settings;

  /* text-decoration */
  decoration_line = _gtk_css_text_decoration_line_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_TEXT_DECORATION_LINE));
  decoration_style = _gtk_css_text_decoration_style_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_TEXT_DECORATION_STYLE));
  color = _gtk_css_rgba_value_get_rgba (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_COLOR));
  decoration_color = _gtk_css_rgba_value_get_rgba (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_TEXT_DECORATION_COLOR));

  switch (decoration_line)
    {
    case GTK_CSS_TEXT_DECORATION_LINE_UNDERLINE:
      attrs = add_pango_attr (attrs, pango_attr_underline_new (get_pango_underline_from_style (decoration_style)));
      if (!gdk_rgba_equal (color, decoration_color))
        attrs = add_pango_attr (attrs, pango_attr_underline_color_new (decoration_color->red * 65535. + 0.5,
                                                                       decoration_color->green * 65535. + 0.5,
                                                                       decoration_color->blue * 65535. + 0.5));
      break;
    case GTK_CSS_TEXT_DECORATION_LINE_LINE_THROUGH:
      attrs = add_pango_attr (attrs, pango_attr_strikethrough_new (TRUE));
      if (!gdk_rgba_equal (color, decoration_color))
        attrs = add_pango_attr (attrs, pango_attr_strikethrough_color_new (decoration_color->red * 65535. + 0.5,
                                                                           decoration_color->green * 65535. + 0.5,
                                                                           decoration_color->blue * 65535. + 0.5));
      break;
    case GTK_CSS_TEXT_DECORATION_LINE_NONE:
    default:
      break;
    }

  /* letter-spacing */
  letter_spacing = _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_LETTER_SPACING), 100);
  if (letter_spacing != 0)
    {
      attrs = add_pango_attr (attrs, pango_attr_letter_spacing_new (letter_spacing * PANGO_SCALE));
    }

  /* OpenType features */

  s = g_string_new ("");

  value = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_FONT_KERNING);
  switch (_gtk_css_font_kerning_value_get (value))
    {
    case GTK_CSS_FONT_KERNING_NORMAL:
      append_separated (s, "kern 1");
      break;
    case GTK_CSS_FONT_KERNING_NONE:
      append_separated (s, "kern 0");
      break;
    case GTK_CSS_FONT_KERNING_AUTO:
    default:
      break;
    }

  value = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_FONT_VARIANT_LIGATURES);
  ligatures = _gtk_css_font_variant_ligature_value_get (value);
  if (ligatures == GTK_CSS_FONT_VARIANT_LIGATURE_NORMAL)
    {
      /* all defaults */
    }
  else if (ligatures == GTK_CSS_FONT_VARIANT_LIGATURE_NONE)
    append_separated (s, "liga 0, clig 0, dlig 0, hlig 0, calt 0");
  else
    {
      if (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_COMMON_LIGATURES)
        append_separated (s, "liga 1, clig 1");
      if (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_NO_COMMON_LIGATURES)
        append_separated (s, "liga 0, clig 0");
      if (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_DISCRETIONARY_LIGATURES)
        append_separated (s, "dlig 1");
      if (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_NO_DISCRETIONARY_LIGATURES)
        append_separated (s, "dlig 0");
      if (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_HISTORICAL_LIGATURES)
        append_separated (s, "hlig 1");
      if (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_NO_HISTORICAL_LIGATURES)
        append_separated (s, "hlig 0");
      if (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_CONTEXTUAL)
        append_separated (s, "calt 1");
      if (ligatures & GTK_CSS_FONT_VARIANT_LIGATURE_NO_CONTEXTUAL)
        append_separated (s, "calt 0");
    }

  value = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_FONT_VARIANT_POSITION);
  switch (_gtk_css_font_variant_position_value_get (value))
    {
    case GTK_CSS_FONT_VARIANT_POSITION_SUB:
      append_separated (s, "subs 1");
      break;
    case GTK_CSS_FONT_VARIANT_POSITION_SUPER:
      append_separated (s, "sups 1");
      break;
    case GTK_CSS_FONT_VARIANT_POSITION_NORMAL:
    default:
      break;
    }

  value = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_FONT_VARIANT_CAPS);
  switch (_gtk_css_font_variant_caps_value_get (value))
    {
    case GTK_CSS_FONT_VARIANT_CAPS_SMALL_CAPS:
      append_separated (s, "smcp 1");
      break;
    case GTK_CSS_FONT_VARIANT_CAPS_ALL_SMALL_CAPS:
      append_separated (s, "c2sc 1, smcp 1");
      break;
    case GTK_CSS_FONT_VARIANT_CAPS_PETITE_CAPS:
      append_separated (s, "pcap 1");
      break;
    case GTK_CSS_FONT_VARIANT_CAPS_ALL_PETITE_CAPS:
      append_separated (s, "c2pc 1, pcap 1");
      break;
    case GTK_CSS_FONT_VARIANT_CAPS_UNICASE:
      append_separated (s, "unic 1");
      break;
    case GTK_CSS_FONT_VARIANT_CAPS_TITLING_CAPS:
      append_separated (s, "titl 1");
      break;
    case GTK_CSS_FONT_VARIANT_CAPS_NORMAL:
    default:
      break;
    }

  value = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_FONT_VARIANT_NUMERIC);
  numeric = _gtk_css_font_variant_numeric_value_get (value);
  if (numeric == GTK_CSS_FONT_VARIANT_NUMERIC_NORMAL)
    {
      /* all defaults */
    }
  else
    {
      if (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_LINING_NUMS)
        append_separated (s, "lnum 1");
      if (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_OLDSTYLE_NUMS)
        append_separated (s, "onum 1");
      if (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_PROPORTIONAL_NUMS)
        append_separated (s, "pnum 1");
      if (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_TABULAR_NUMS)
        append_separated (s, "tnum 1");
      if (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_DIAGONAL_FRACTIONS)
        append_separated (s, "frac 1");
      if (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_STACKED_FRACTIONS)
        append_separated (s, "afrc 1");
      if (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_ORDINAL)
        append_separated (s, "ordn 1");
      if (numeric & GTK_CSS_FONT_VARIANT_NUMERIC_SLASHED_ZERO)
        append_separated (s, "zero 1");
    }

  value = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_FONT_VARIANT_ALTERNATES);
  switch (_gtk_css_font_variant_alternate_value_get (value))
    {
    case GTK_CSS_FONT_VARIANT_ALTERNATE_HISTORICAL_FORMS:
      append_separated (s, "hist 1");
      break;
    case GTK_CSS_FONT_VARIANT_ALTERNATE_NORMAL:
    default:
      break;
    }

  value = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_FONT_VARIANT_EAST_ASIAN);
  east_asian = _gtk_css_font_variant_east_asian_value_get (value);
  if (east_asian == GTK_CSS_FONT_VARIANT_EAST_ASIAN_NORMAL)
    {
      /* all defaults */
    }
  else
    {
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS78)
        append_separated (s, "jp78 1");
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS83)
        append_separated (s, "jp83 1");
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS90)
        append_separated (s, "jp90 1");
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_JIS04)
        append_separated (s, "jp04 1");
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_SIMPLIFIED)
        append_separated (s, "smpl 1");
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_TRADITIONAL)
        append_separated (s, "trad 1");
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_FULL_WIDTH)
        append_separated (s, "fwid 1");
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_PROPORTIONAL)
        append_separated (s, "pwid 1");
      if (east_asian & GTK_CSS_FONT_VARIANT_EAST_ASIAN_RUBY)
        append_separated (s, "ruby 1");
    }

  value = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_FONT_FEATURE_SETTINGS);
  settings = gtk_css_font_features_value_get_features (value);
  if (settings)
    {
      append_separated (s, settings);
      g_free (settings);
    }

  attrs = add_pango_attr (attrs, pango_attr_font_features_new (s->str));
  g_string_free (s, TRUE);

  return attrs;
}

static GtkCssValue *
query_func (guint    id,
            gpointer values)
{
  return gtk_css_style_get_value (values, id);
}

PangoFontDescription *
gtk_css_style_get_pango_font (GtkCssStyle *style)
{
  GtkStyleProperty *prop;
  GValue value = { 0, };

  prop = _gtk_style_property_lookup ("font");
  _gtk_style_property_query (prop, &value, query_func, style);

  return (PangoFontDescription *)g_value_get_boxed (&value);
}
