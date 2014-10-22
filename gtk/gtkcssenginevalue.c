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

#define GDK_DISABLE_DEPRECATION_WARNINGS
#include "gtkcssenginevalueprivate.h"

#include "gtkstylepropertyprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GtkThemingEngine *engine;
};

static void
gtk_css_value_engine_free (GtkCssValue *value)
{
  g_object_unref (value->engine);

  g_slice_free (GtkCssValue, value);
}

static GtkCssValue *
gtk_css_value_engine_compute (GtkCssValue             *value,
                              guint                    property_id,
                              GtkStyleProviderPrivate *provider,
			      int                      scale,
                              GtkCssStyle    *values,
                              GtkCssStyle    *parent_values,
                              GtkCssDependencies      *dependencies)
{
  return _gtk_css_value_ref (value);
}

static gboolean
gtk_css_value_engine_equal (const GtkCssValue *value1,
                            const GtkCssValue *value2)
{
  return value1->engine == value2->engine;
}

static GtkCssValue *
gtk_css_value_engine_transition (GtkCssValue *start,
                                 GtkCssValue *end,
                                 guint        property_id,
                                 double       progress)
{
  return NULL;
}

static void
gtk_css_value_engine_print (const GtkCssValue *value,
                            GString           *string)
{
  char *name;

  g_object_get (value->engine, "name", &name, NULL);

  if (name)
    g_string_append (string, name);
  else
    g_string_append (string, "none");

  g_free (name);
}

static const GtkCssValueClass GTK_CSS_VALUE_ENGINE = {
  gtk_css_value_engine_free,
  gtk_css_value_engine_compute,
  gtk_css_value_engine_equal,
  gtk_css_value_engine_transition,
  gtk_css_value_engine_print
};

GtkCssValue *
_gtk_css_engine_value_new (GtkThemingEngine *engine)
{
  GtkCssValue *result;

  g_return_val_if_fail (GTK_IS_THEMING_ENGINE (engine), NULL);

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_ENGINE);
  result->engine = g_object_ref (engine);

  return result;
}

GtkCssValue *
_gtk_css_engine_value_parse (GtkCssParser *parser)
{
  GtkThemingEngine *engine;
  char *str;

  g_return_val_if_fail (parser != NULL, NULL);

  if (_gtk_css_parser_try (parser, "none", TRUE))
    return _gtk_css_engine_value_new (gtk_theming_engine_load (NULL));

  str = _gtk_css_parser_try_ident (parser, TRUE);
  if (str == NULL)
    {
      _gtk_css_parser_error (parser, "Expected a valid theme name");
      return NULL;
    }

  engine = gtk_theming_engine_load (str);

  if (engine == NULL)
    {
      _gtk_css_parser_error (parser, "Theming engine '%s' not found", str);
      g_free (str);
      return NULL;
    }

  g_free (str);

  return _gtk_css_engine_value_new (engine);
}

GtkThemingEngine *
_gtk_css_engine_value_get_engine (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_ENGINE, NULL);

  return value->engine;
}

