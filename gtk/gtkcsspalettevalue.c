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

#include "gtkcsscolorvalueprivate.h"
#include "gtkcssrgbavalueprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GHashTable *colors;
};

static GtkCssValue *default_palette;

static GtkCssValue *gtk_css_palette_value_new_empty (void);

static void
gtk_css_palette_value_add_color (GtkCssValue *value,
                                 const char  *name,
                                 GtkCssValue *color)
{
  g_hash_table_insert (value->colors, g_strdup (name), color);
}

static void
gtk_css_value_palette_free (GtkCssValue *value)
{
  g_hash_table_unref (value->colors);

  g_slice_free (GtkCssValue, value);
}

static GtkCssValue *
gtk_css_value_palette_compute (GtkCssValue             *specified,
                               guint                    property_id,
                               GtkStyleProviderPrivate *provider,
                               GtkCssStyle             *style,
                               GtkCssStyle             *parent_style)
{
  GHashTableIter iter;
  gpointer name, value;
  GtkCssValue *computed_color;
  GtkCssValue *result;
  gboolean changes = FALSE;

  result = gtk_css_palette_value_new_empty ();

  g_hash_table_iter_init (&iter, specified->colors);
  while (g_hash_table_iter_next (&iter, &name, &value))
    {
      computed_color = _gtk_css_value_compute (value, property_id, provider, style, parent_style);
      changes |= computed_color != value;
      gtk_css_palette_value_add_color (result, name, computed_color);
    }

  if (!changes)
    {
      _gtk_css_value_unref (result);
      result = _gtk_css_value_ref (specified);
    }

  return result;
}

static gboolean
gtk_css_value_palette_equal (const GtkCssValue *value1,
                             const GtkCssValue *value2)
{
  gpointer name, color1, color2;
  GHashTableIter iter;

  if (g_hash_table_size (value1->colors) != g_hash_table_size (value2->colors))
    return FALSE;

  g_hash_table_iter_init (&iter, value1->colors);
  while (g_hash_table_iter_next (&iter, &name, &color1))
    {
      color2 = g_hash_table_lookup (value2->colors, name);
      if (color2 == NULL)
        return FALSE;

      if (!_gtk_css_value_equal (color1, color2))
        return FALSE;
    }

  return TRUE;
}

static GtkCssValue *
gtk_css_value_palette_transition (GtkCssValue *start,
                                  GtkCssValue *end,
                                  guint        property_id,
                                  double       progress)
{
  gpointer name, start_color, end_color;
  GHashTableIter iter;
  GtkCssValue *result, *transition;

  /* XXX: For colors that are only in start or end but not both,
   * we don't transition but just keep the value.
   * That causes an abrupt transition to currentColor at the end.
   */

  result = gtk_css_palette_value_new_empty ();

  g_hash_table_iter_init (&iter, start->colors);
  while (g_hash_table_iter_next (&iter, &name, &start_color))
    {
      end_color = g_hash_table_lookup (end->colors, name);
      if (end_color == NULL)
        transition = _gtk_css_value_ref (start_color);
      else
        transition = _gtk_css_value_transition (start_color, end_color, property_id, progress);

      gtk_css_palette_value_add_color (result, name, transition);
    }

  g_hash_table_iter_init (&iter, end->colors);
  while (g_hash_table_iter_next (&iter, &name, &end_color))
    {
      start_color = g_hash_table_lookup (start->colors, name);
      if (start_color != NULL)
        continue;

      gtk_css_palette_value_add_color (result, name, _gtk_css_value_ref (end_color));
    }

  return result;
}

static void
gtk_css_value_palette_print (const GtkCssValue *value,
                             GString           *string)
{
  GHashTableIter iter;
  gpointer name, color;
  gboolean first = TRUE;

  if (value == default_palette)
    {
      g_string_append (string, "default");
      return;
    }

  g_hash_table_iter_init (&iter, value->colors);
  while (g_hash_table_iter_next (&iter, &name, &color))
    {
      if (first)
        first = FALSE;
      else
        g_string_append (string, ", ");
      g_string_append (string, name);
      g_string_append_c (string, ' ');
      _gtk_css_value_print (color, string);
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_PALETTE = {
  gtk_css_value_palette_free,
  gtk_css_value_palette_compute,
  gtk_css_value_palette_equal,
  gtk_css_value_palette_transition,
  gtk_css_value_palette_print
};

static GtkCssValue *
gtk_css_palette_value_new_empty (void)
{
  GtkCssValue *result;

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_PALETTE);
  result->colors = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          g_free,
                                          (GDestroyNotify) _gtk_css_value_unref);

  return result;
}

GtkCssValue *
gtk_css_palette_value_new_default (void)
{
  if (default_palette == NULL)
    {
      default_palette = gtk_css_palette_value_new_empty ();
      gtk_css_palette_value_add_color (default_palette, "error", _gtk_css_color_value_new_name ("error_color"));
      gtk_css_palette_value_add_color (default_palette, "warning", _gtk_css_color_value_new_name ("warning_color"));
      gtk_css_palette_value_add_color (default_palette, "success", _gtk_css_color_value_new_name ("success_color"));
    }

  return _gtk_css_value_ref (default_palette);
}

GtkCssValue *
gtk_css_palette_value_parse (GtkCssParser *parser)
{
  GtkCssValue *result, *color;
  char *ident;

  if (_gtk_css_parser_try (parser, "default", TRUE))
    return gtk_css_palette_value_new_default ();
  
  result = gtk_css_palette_value_new_empty ();

  do {
    ident = _gtk_css_parser_try_ident (parser, TRUE);
    if (ident == NULL)
      {
        _gtk_css_parser_error (parser, "expected color name");
        _gtk_css_value_unref (result);
        return NULL;
      }
    
    color = _gtk_css_color_value_parse (parser);
    if (color == NULL)
      {
        g_free (ident);
        _gtk_css_value_unref (result);
        return NULL;
      }

    gtk_css_palette_value_add_color (result, ident, color);
    g_free (ident);
  } while (_gtk_css_parser_try (parser, ",", TRUE));

  return result;
}

const GdkRGBA *
gtk_css_palette_value_get_color (GtkCssValue *value,
                                 const char  *name)
{
  GtkCssValue *color;

  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_PALETTE, NULL);

  color = g_hash_table_lookup (value->colors, name);
  if (color == NULL)
    return NULL;

  return _gtk_css_rgba_value_get_rgba (color);
}
