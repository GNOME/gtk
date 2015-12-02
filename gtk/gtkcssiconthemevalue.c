/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
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

#include "gtkcssiconthemevalueprivate.h"

#include "gtkicontheme.h"
#include "gtksettingsprivate.h"
#include "gtkstyleproviderprivate.h"

/*
 * The idea behind this value (and the '-gtk-icon-theme' CSS property) is
 * to track changes to the icon theme.
 *
 * We create a new instance of this value whenever the icon theme changes
 * (via emitting the changed signal). So as long as the icon theme does
 * not change, we will compute the same value. We can then compare values
 * by pointer to see if the icon theme changed.
 */

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GtkIconTheme *icontheme;
  guint changed_id;
};

static void
gtk_css_value_icon_theme_disconnect_handler (GtkCssValue *value)
{
  if (value->changed_id == 0)
    return;

  g_object_set_data (G_OBJECT (value->icontheme), "-gtk-css-value", NULL);

  g_signal_handler_disconnect (value->icontheme, value->changed_id);
  value->changed_id = 0;
}

static void
gtk_css_value_icon_theme_changed_cb (GtkIconTheme *icontheme,
                                     GtkCssValue  *value)
{
  gtk_css_value_icon_theme_disconnect_handler (value);
}

static void
gtk_css_value_icon_theme_free (GtkCssValue *value)
{
  gtk_css_value_icon_theme_disconnect_handler (value);

  if (value->icontheme)
    g_object_unref (value->icontheme);

  g_slice_free (GtkCssValue, value);
}

static GtkCssValue *
gtk_css_value_icon_theme_compute (GtkCssValue             *icon_theme,
                                  guint                    property_id,
                                  GtkStyleProviderPrivate *provider,
                                  GtkCssStyle             *style,
                                  GtkCssStyle             *parent_style)
{
  GtkIconTheme *icontheme;

  if (icon_theme->icontheme)
    icontheme = icon_theme->icontheme;
  else
    icontheme = gtk_icon_theme_get_for_screen (_gtk_settings_get_screen (_gtk_style_provider_private_get_settings (provider)));

  return gtk_css_icon_theme_value_new (icontheme);
}

static gboolean
gtk_css_value_icon_theme_equal (const GtkCssValue *value1,
                                const GtkCssValue *value2)
{
  return FALSE;
}

static GtkCssValue *
gtk_css_value_icon_theme_transition (GtkCssValue *start,
                                     GtkCssValue *end,
                                     guint        property_id,
                                     double       progress)
{
  return NULL;
}

static void
gtk_css_value_icon_theme_print (const GtkCssValue *icon_theme,
                                GString           *string)
{
  g_string_append (string, "initial");
}

static const GtkCssValueClass GTK_CSS_VALUE_ICON_THEME = {
  gtk_css_value_icon_theme_free,
  gtk_css_value_icon_theme_compute,
  gtk_css_value_icon_theme_equal,
  gtk_css_value_icon_theme_transition,
  gtk_css_value_icon_theme_print
};

static GtkCssValue default_icon_theme_value = { &GTK_CSS_VALUE_ICON_THEME, 1, NULL, 0 };

GtkCssValue *
gtk_css_icon_theme_value_new (GtkIconTheme *icontheme)
{
  GtkCssValue *result;

  if (icontheme == NULL)
    return _gtk_css_value_ref (&default_icon_theme_value);

  result = g_object_get_data (G_OBJECT (icontheme), "-gtk-css-value");
  if (result)
    return _gtk_css_value_ref (result);

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_ICON_THEME);
  result->icontheme = g_object_ref (icontheme);

  g_object_set_data (G_OBJECT (icontheme), "-gtk-css-value", result);
  result->changed_id = g_signal_connect (icontheme, "changed", G_CALLBACK (gtk_css_value_icon_theme_changed_cb), result);

  return result;
}

GtkCssValue *
gtk_css_icon_theme_value_parse (GtkCssParser *parser)
{
  GtkIconTheme *icontheme;
  GtkCssValue *result;
  char *s;

  s = _gtk_css_parser_read_string (parser);
  if (s == NULL)
    return NULL;

  icontheme = gtk_icon_theme_new ();
  gtk_icon_theme_set_custom_theme (icontheme, s);

  result = gtk_css_icon_theme_value_new (icontheme);

  g_object_unref (icontheme);
  g_free (s);

  return result;
}

GtkIconTheme *
gtk_css_icon_theme_value_get_icon_theme (GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_ICON_THEME, NULL);

  return value->icontheme;
}
