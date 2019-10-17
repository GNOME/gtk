/* GTK - The GIMP Toolkit
 * Copyright (C) 2017 Red Hat, Inc.
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
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include "gtkcsstypesprivate.h"
#include "gtkcssparserprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssfontfeaturesvalueprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GHashTable *features;
};

static GtkCssValue *default_font_features;

static GtkCssValue *gtk_css_font_features_value_new_empty (void);

static void
gtk_css_font_features_value_add_feature (GtkCssValue *value,
                                      const char  *name,
                                      GtkCssValue *val)
{
  g_hash_table_insert (value->features, g_strdup (name), val);
}


static void
gtk_css_value_font_features_free (GtkCssValue *value)
{
  g_hash_table_unref (value->features);

  g_slice_free (GtkCssValue, value);
}

static GtkCssValue *
gtk_css_value_font_features_compute (GtkCssValue      *specified,
                                     guint             property_id,
                                     GtkStyleProvider *provider,
                                     GtkCssStyle      *style,
                                     GtkCssStyle      *parent_style)
{
  GHashTableIter iter;
  gpointer name, val;
  GtkCssValue *computed_val;
  GtkCssValue *result;
  gboolean changes = FALSE;

  result = gtk_css_font_features_value_new_empty ();

  g_hash_table_iter_init (&iter, specified->features);
  while (g_hash_table_iter_next (&iter, &name, &val))
    {
      computed_val = _gtk_css_value_compute (val, property_id, provider, style, parent_style);
      changes |= computed_val != val;
      gtk_css_font_features_value_add_feature (result, name, computed_val);
    }

  if (!changes)
    {
      _gtk_css_value_unref (result);
      result = _gtk_css_value_ref (specified);
    }

  return result;
}

static gboolean
gtk_css_value_font_features_equal (const GtkCssValue *value1,
                                   const GtkCssValue *value2)
{
  gpointer name, val1, val2;
  GHashTableIter iter;

  if (g_hash_table_size (value1->features) != g_hash_table_size (value2->features))
    return FALSE;

  g_hash_table_iter_init (&iter, value1->features);
  while (g_hash_table_iter_next (&iter, &name, &val1))
    {
      val2 = g_hash_table_lookup (value2->features, name);
      if (val2 == NULL)
        return FALSE;

      if (!_gtk_css_value_equal (val1, val2))
        return FALSE;
    }

  return TRUE;
}

static GtkCssValue *
gtk_css_value_font_features_transition (GtkCssValue *start,
                                        GtkCssValue *end,
                                        guint        property_id,
                                        double       progress)
{
  const char *name;
  GtkCssValue *start_val, *end_val;
  GHashTableIter iter;
  GtkCssValue *result, *transition;

  /* XXX: For value that are only in start or end but not both,
   * we don't transition but just keep the value.
   * That causes an abrupt transition to the new value at the end.
   */

  result = gtk_css_font_features_value_new_empty ();

  g_hash_table_iter_init (&iter, start->features);
  while (g_hash_table_iter_next (&iter, (gpointer *)&name, (gpointer *)&start_val))
    {
      end_val = g_hash_table_lookup (end->features, name);
      if (end_val == NULL)
        transition = _gtk_css_value_ref (start_val);
      else
        transition = _gtk_css_value_transition (start_val, end_val, property_id, progress);

      gtk_css_font_features_value_add_feature (result, name, transition);
    }

  g_hash_table_iter_init (&iter, end->features);
  while (g_hash_table_iter_next (&iter, (gpointer *)&name, (gpointer *)&end_val))
    {
      start_val = g_hash_table_lookup (start->features, name);
      if (start_val != NULL)
        continue;

      gtk_css_font_features_value_add_feature (result, name, _gtk_css_value_ref (end_val));
    }

  return result;
}

static void
gtk_css_value_font_features_print (const GtkCssValue *value,
                                   GString           *string)
{
  GHashTableIter iter;
  const char *name;
  GtkCssValue *val;
  gboolean first = TRUE;

  if (value == default_font_features)
    {
      g_string_append (string, "normal");
      return;
    }

  g_hash_table_iter_init (&iter, value->features);
  while (g_hash_table_iter_next (&iter, (gpointer *)&name, (gpointer *)&val))
    {
      if (first)
        first = FALSE;
      else
        g_string_append (string, ", ");
      g_string_append_printf (string, "\"%s\" ", name);
      _gtk_css_value_print (val, string);
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_FONT_FEATURES = {
  gtk_css_value_font_features_free,
  gtk_css_value_font_features_compute,
  gtk_css_value_font_features_equal,
  gtk_css_value_font_features_transition,
  NULL,
  NULL,
  gtk_css_value_font_features_print
};

static GtkCssValue *
gtk_css_font_features_value_new_empty (void)
{
  GtkCssValue *result;

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_FONT_FEATURES);
  result->features = g_hash_table_new_full (g_str_hash, g_str_equal,
                                        g_free,
                                        (GDestroyNotify) _gtk_css_value_unref);

  return result;
}

GtkCssValue *
gtk_css_font_features_value_new_default (void)
{
  if (default_font_features == NULL)
    default_font_features = gtk_css_font_features_value_new_empty ();

  return _gtk_css_value_ref (default_font_features);
}

static gboolean
is_valid_opentype_tag (const char *s)
{
  if (strlen (s) != 4)
    return FALSE;

  if (s[0] < 0x20 || s[0] > 0x7e ||
      s[1] < 0x20 || s[1] > 0x7e ||
      s[2] < 0x20 || s[2] > 0x7e ||
      s[3] < 0x20 || s[3] > 0x7e)
    return FALSE;

  return TRUE;
}

GtkCssValue *
gtk_css_font_features_value_parse (GtkCssParser *parser)
{
  GtkCssValue *result, *val;
  char *name;
  int num;

  if (gtk_css_parser_try_ident (parser, "normal"))
    return gtk_css_font_features_value_new_default ();

  result = gtk_css_font_features_value_new_empty ();

  do {
    name = gtk_css_parser_consume_string (parser);
    if (name == NULL)
      {
        _gtk_css_value_unref (result);
        return NULL;
      }

    if (!is_valid_opentype_tag (name))
      {
        gtk_css_parser_error_value (parser, "Not a valid OpenType tag.");
        g_free (name);
        _gtk_css_value_unref (result);
        return NULL;
      }

    if (gtk_css_parser_try_ident (parser, "on"))
      val = _gtk_css_number_value_new (1.0, GTK_CSS_NUMBER);
    else if (gtk_css_parser_try_ident (parser, "off"))
      val = _gtk_css_number_value_new (0.0, GTK_CSS_NUMBER);
    else if (gtk_css_parser_has_integer (parser))
      {
        if (gtk_css_parser_consume_integer (parser, &num))
          {
            val = _gtk_css_number_value_new ((double)num, GTK_CSS_NUMBER);
          }
        else
          {
            g_free (name);
            _gtk_css_value_unref (result);
            return NULL;
          }
      }
    else
      val = _gtk_css_number_value_new (1.0, GTK_CSS_NUMBER);

    gtk_css_font_features_value_add_feature (result, name, val);
    g_free (name);
  } while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA));

  return result;
}

char *
gtk_css_font_features_value_get_features (GtkCssValue *value)
{
  GtkCssValue *val;
  GHashTableIter iter;
  gboolean first = TRUE;
  const char *name;
  GString *string;

  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_FONT_FEATURES, NULL);

  if (value == default_font_features)
    return NULL;

  string = g_string_new ("");

  g_hash_table_iter_init (&iter, value->features);
  while (g_hash_table_iter_next (&iter, (gpointer *)&name, (gpointer *)&val))
    {
      if (first)
        first = FALSE;
      else
        g_string_append (string, ", ");
      g_string_append_printf (string, "%s %d", name, (int)_gtk_css_number_value_get (val, 100));
    }

  return g_string_free (string, FALSE);
}
