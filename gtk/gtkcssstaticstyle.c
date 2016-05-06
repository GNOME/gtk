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

#include "gtkcssstaticstyleprivate.h"

#include "gtkcssanimationprivate.h"
#include "gtkcssarrayvalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcssinheritvalueprivate.h"
#include "gtkcssinitialvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcsssectionprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcssstringvalueprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstransitionprivate.h"
#include "gtkprivate.h"
#include "gtksettings.h"
#include "gtkstyleanimationprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstyleproviderprivate.h"

G_DEFINE_TYPE (GtkCssStaticStyle, gtk_css_static_style, GTK_TYPE_CSS_STYLE)

static GtkCssValue *
gtk_css_static_style_get_value (GtkCssStyle *style,
                                guint        id)
{
  GtkCssStaticStyle *sstyle = GTK_CSS_STATIC_STYLE (style);

  if (G_UNLIKELY (id >= GTK_CSS_PROPERTY_N_PROPERTIES))
    {
      GtkCssStyleProperty *prop = _gtk_css_style_property_lookup_by_id (id);

      return _gtk_css_style_property_get_initial_value (prop);
    }

  return sstyle->values[id];
}

static GtkCssSection *
gtk_css_static_style_get_section (GtkCssStyle *style,
                                    guint        id)
{
  GtkCssStaticStyle *sstyle = GTK_CSS_STATIC_STYLE (style);

  if (sstyle->sections == NULL ||
      id >= sstyle->sections->len)
    return NULL;

  return g_ptr_array_index (sstyle->sections, id);
}

static void
gtk_css_static_style_dispose (GObject *object)
{
  GtkCssStaticStyle *style = GTK_CSS_STATIC_STYLE (object);
  guint i;

  for (i = 0; i < GTK_CSS_PROPERTY_N_PROPERTIES; i++)
    {
      if (style->values[i])
        _gtk_css_value_unref (style->values[i]);
    }
  if (style->sections)
    {
      g_ptr_array_unref (style->sections);
      style->sections = NULL;
    }

  G_OBJECT_CLASS (gtk_css_static_style_parent_class)->dispose (object);
}

static void
gtk_css_static_style_class_init (GtkCssStaticStyleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCssStyleClass *style_class = GTK_CSS_STYLE_CLASS (klass);

  object_class->dispose = gtk_css_static_style_dispose;

  style_class->get_value = gtk_css_static_style_get_value;
  style_class->get_section = gtk_css_static_style_get_section;
}

static void
gtk_css_static_style_init (GtkCssStaticStyle *style)
{
}

static void
maybe_unref_section (gpointer section)
{
  if (section)
    gtk_css_section_unref (section);
}

static void
gtk_css_static_style_set_value (GtkCssStaticStyle *style,
                                guint              id,
                                GtkCssValue       *value,
                                GtkCssSection     *section)
{
  if (style->values[id])
    _gtk_css_value_unref (style->values[id]);
  style->values[id] = _gtk_css_value_ref (value);

  if (style->sections && style->sections->len > id && g_ptr_array_index (style->sections, id))
    {
      gtk_css_section_unref (g_ptr_array_index (style->sections, id));
      g_ptr_array_index (style->sections, id) = NULL;
    }

  if (section)
    {
      if (style->sections == NULL)
        style->sections = g_ptr_array_new_with_free_func (maybe_unref_section);
      if (style->sections->len <= id)
        g_ptr_array_set_size (style->sections, id + 1);

      g_ptr_array_index (style->sections, id) = gtk_css_section_ref (section);
    }
}

static GtkCssStyle *default_style;

static void
clear_default_style (gpointer data)
{
  g_set_object (&default_style, NULL);
}

GtkCssStyle *
gtk_css_static_style_get_default (void)
{
  /* FIXME: This really depends on the screen, but we don't have
   * a screen at hand when we call this function, and in practice,
   * the default style is always replaced by something else
   * before we use it.
   */
  if (default_style == NULL)
    {
      GtkSettings *settings;

      settings = gtk_settings_get_default ();
      default_style = gtk_css_static_style_new_compute (GTK_STYLE_PROVIDER_PRIVATE (settings),
                                                        NULL,
                                                        NULL);
      g_object_set_data_full (G_OBJECT (settings), "gtk-default-style",
                              default_style, clear_default_style);
    }

  return default_style;
}

GtkCssStyle *
gtk_css_static_style_new_compute (GtkStyleProviderPrivate *provider,
                                  const GtkCssMatcher     *matcher,
                                  GtkCssStyle             *parent)
{
  GtkCssStaticStyle *result;
  GtkCssLookup *lookup;
  GtkCssChange change = GTK_CSS_CHANGE_ANY_SELF | GTK_CSS_CHANGE_ANY_SIBLING | GTK_CSS_CHANGE_ANY_PARENT;

  lookup = _gtk_css_lookup_new (NULL);

  if (matcher)
    _gtk_style_provider_private_lookup (provider,
                                        matcher,
                                        lookup,
                                        &change);

  result = g_object_new (GTK_TYPE_CSS_STATIC_STYLE, NULL);

  result->change = change;

  _gtk_css_lookup_resolve (lookup,
                           provider,
                           result,
                           parent);

  _gtk_css_lookup_free (lookup);

  return GTK_CSS_STYLE (result);
}

void
gtk_css_static_style_compute_value (GtkCssStaticStyle       *style,
                                    GtkStyleProviderPrivate *provider,
                                    GtkCssStyle             *parent_style,
                                    guint                    id,
                                    GtkCssValue             *specified,
                                    GtkCssSection           *section)
{
  GtkCssValue *value;

  gtk_internal_return_if_fail (GTK_IS_CSS_STATIC_STYLE (style));
  gtk_internal_return_if_fail (GTK_IS_STYLE_PROVIDER_PRIVATE (provider));
  gtk_internal_return_if_fail (parent_style == NULL || GTK_IS_CSS_STYLE (parent_style));
  gtk_internal_return_if_fail (id < GTK_CSS_PROPERTY_N_PROPERTIES);

  /* http://www.w3.org/TR/css3-cascade/#cascade
   * Then, for every element, the value for each property can be found
   * by following this pseudo-algorithm:
   * 1) Identify all declarations that apply to the element
   */
  if (specified == NULL)
    {
      GtkCssStyleProperty *prop = _gtk_css_style_property_lookup_by_id (id);

      if (_gtk_css_style_property_is_inherit (prop))
        specified = _gtk_css_inherit_value_new ();
      else
        specified = _gtk_css_initial_value_new ();
    }
  else
    _gtk_css_value_ref (specified);

  value = _gtk_css_value_compute (specified, id, provider, GTK_CSS_STYLE (style), parent_style);

  gtk_css_static_style_set_value (style, id, value, section);

  _gtk_css_value_unref (value);
  _gtk_css_value_unref (specified);
}

GtkCssChange
gtk_css_static_style_get_change (GtkCssStaticStyle *style)
{
  g_return_val_if_fail (GTK_IS_CSS_STATIC_STYLE (style), GTK_CSS_CHANGE_ANY);

  return style->change;
}
