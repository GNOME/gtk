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
#include "gtkstyleanimationprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstyleproviderprivate.h"

G_DEFINE_TYPE (GtkCssStaticStyle, gtk_css_static_style, GTK_TYPE_CSS_STYLE)

static GtkCssValue *
gtk_css_static_style_get_value (GtkCssStyle *style,
                                guint        id)
{
  GtkCssStaticStyle *sstyle = GTK_CSS_STATIC_STYLE (style);

  if (sstyle->values == NULL ||
      id >= sstyle->values->len)
    return NULL;

  return g_ptr_array_index (sstyle->values, id);
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

/* Compute the bitmask of potentially changed properties if the parent has changed
 * the passed in ones.
 * This is for example needed when changes in the "color" property will affect
 * all properties using "currentColor" as a color.
 */
static GtkBitmask *
gtk_css_static_style_compute_dependencies (GtkCssStaticStyle *style,
                                           const GtkBitmask  *parent_changes)
{
  GtkCssStaticStyle *sstyle = GTK_CSS_STATIC_STYLE (style);
  GtkBitmask *changes;

  changes = _gtk_bitmask_copy (parent_changes);
  changes = _gtk_bitmask_intersect (changes, sstyle->depends_on_parent);
  if (_gtk_bitmask_get (changes, GTK_CSS_PROPERTY_COLOR))
    changes = _gtk_bitmask_union (changes, sstyle->depends_on_color);
  if (_gtk_bitmask_get (changes, GTK_CSS_PROPERTY_FONT_SIZE))
    changes = _gtk_bitmask_union (changes, sstyle->depends_on_font_size);

  return changes;
}

static void
gtk_css_static_style_dispose (GObject *object)
{
  GtkCssStaticStyle *style = GTK_CSS_STATIC_STYLE (object);

  if (style->values)
    {
      g_ptr_array_unref (style->values);
      style->values = NULL;
    }
  if (style->sections)
    {
      g_ptr_array_unref (style->sections);
      style->sections = NULL;
    }

  G_OBJECT_CLASS (gtk_css_static_style_parent_class)->dispose (object);
}

static void
gtk_css_static_style_finalize (GObject *object)
{
  GtkCssStaticStyle *style = GTK_CSS_STATIC_STYLE (object);

  _gtk_bitmask_free (style->depends_on_parent);
  _gtk_bitmask_free (style->equals_parent);
  _gtk_bitmask_free (style->depends_on_color);
  _gtk_bitmask_free (style->depends_on_font_size);

  G_OBJECT_CLASS (gtk_css_static_style_parent_class)->finalize (object);
}

static void
gtk_css_static_style_class_init (GtkCssStaticStyleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCssStyleClass *style_class = GTK_CSS_STYLE_CLASS (klass);

  object_class->dispose = gtk_css_static_style_dispose;
  object_class->finalize = gtk_css_static_style_finalize;

  style_class->get_value = gtk_css_static_style_get_value;
  style_class->get_section = gtk_css_static_style_get_section;
}

static void
gtk_css_static_style_init (GtkCssStaticStyle *style)
{
  style->depends_on_parent = _gtk_bitmask_new ();
  style->equals_parent = _gtk_bitmask_new ();
  style->depends_on_color = _gtk_bitmask_new ();
  style->depends_on_font_size = _gtk_bitmask_new ();
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
  if (style->values == NULL)
    style->values = g_ptr_array_new_with_free_func ((GDestroyNotify)_gtk_css_value_unref);
  if (id >= style->values->len)
   g_ptr_array_set_size (style->values, id + 1);

  if (g_ptr_array_index (style->values, id))
    _gtk_css_value_unref (g_ptr_array_index (style->values, id));
  g_ptr_array_index (style->values, id) = _gtk_css_value_ref (value);

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

GtkCssStyle *
gtk_css_static_style_new_compute (GtkStyleProviderPrivate *provider,
                                  const GtkCssMatcher     *matcher,
                                  int                      scale,
                                  GtkCssStyle             *parent,
                                  GtkCssChange            *out_change)
{
  GtkCssStaticStyle *result;
  GtkCssLookup *lookup;

  lookup = _gtk_css_lookup_new (NULL);

  if (matcher)
    _gtk_style_provider_private_lookup (provider,
                                        matcher,
                                        lookup,
                                        out_change);

  result = g_object_new (GTK_TYPE_CSS_STATIC_STYLE, NULL);

  _gtk_css_lookup_resolve (lookup, 
                           provider,
			   scale,
                           result,
                           parent);

  _gtk_css_lookup_free (lookup);

  return GTK_CSS_STYLE (result);
}

GtkCssStyle *
gtk_css_static_style_new_update (GtkCssStaticStyle       *style,
                                 const GtkBitmask        *parent_changes,
                                 GtkStyleProviderPrivate *provider,
                                 const GtkCssMatcher     *matcher,
                                 int                      scale,
                                 GtkCssStyle             *parent)
{
  GtkCssStaticStyle *result;
  GtkCssLookup *lookup;
  GtkBitmask *changes;
  guint i;

  gtk_internal_return_val_if_fail (GTK_IS_CSS_STATIC_STYLE (style), NULL);
  gtk_internal_return_val_if_fail (parent_changes != NULL, NULL);
  gtk_internal_return_val_if_fail (GTK_IS_STYLE_PROVIDER_PRIVATE (provider), NULL);
  gtk_internal_return_val_if_fail (matcher != NULL, NULL);

  changes = gtk_css_static_style_compute_dependencies (style, parent_changes);
  if (_gtk_bitmask_is_empty (changes))
    {
      _gtk_bitmask_free (changes);
      return g_object_ref (style);
    }

  result = g_object_new (GTK_TYPE_CSS_STATIC_STYLE, NULL);

  result->depends_on_parent = _gtk_bitmask_subtract (_gtk_bitmask_union (result->depends_on_parent, style->depends_on_parent),
                                                     changes);
  result->equals_parent = _gtk_bitmask_subtract (_gtk_bitmask_union (result->equals_parent, style->equals_parent),
                                                 changes);
  result->depends_on_color = _gtk_bitmask_subtract (_gtk_bitmask_union (result->depends_on_color, style->depends_on_color),
                                                    changes);
  result->depends_on_font_size = _gtk_bitmask_subtract (_gtk_bitmask_union (result->depends_on_font_size, style->depends_on_font_size),
                                                        changes);

  for (i = 0; i < style->values->len; i++)
    {
      if (_gtk_bitmask_get (changes, i))
        continue;

      gtk_css_static_style_set_value (result,
                                      i,
                                      gtk_css_static_style_get_value (GTK_CSS_STYLE (style), i),
                                      gtk_css_static_style_get_section (GTK_CSS_STYLE (style), i));
    }

  lookup = _gtk_css_lookup_new (changes);

  _gtk_style_provider_private_lookup (provider,
                                      matcher,
                                      lookup,
                                      NULL);

  _gtk_css_lookup_resolve (lookup, 
                           provider,
                           scale,
                           result,
                           parent);

  _gtk_css_lookup_free (lookup);
  _gtk_bitmask_free (changes);

  return GTK_CSS_STYLE (result);
}

void
gtk_css_static_style_compute_value (GtkCssStaticStyle       *style,
                                    GtkStyleProviderPrivate *provider,
                                    int                      scale,
                                    GtkCssStyle             *parent_style,
                                    guint                    id,
                                    GtkCssValue             *specified,
                                    GtkCssSection           *section)
{
  GtkCssDependencies dependencies;
  GtkCssValue *value;

  gtk_internal_return_if_fail (GTK_IS_CSS_STATIC_STYLE (style));
  gtk_internal_return_if_fail (GTK_IS_STYLE_PROVIDER_PRIVATE (provider));
  gtk_internal_return_if_fail (parent_style == NULL || GTK_IS_CSS_STYLE (parent_style));

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

  value = _gtk_css_value_compute (specified, id, provider, scale, GTK_CSS_STYLE (style), parent_style, &dependencies);

  gtk_css_static_style_set_value (style, id, value, section);

  if (dependencies & (GTK_CSS_DEPENDS_ON_PARENT | GTK_CSS_EQUALS_PARENT))
    style->depends_on_parent = _gtk_bitmask_set (style->depends_on_parent, id, TRUE);
  if (dependencies & (GTK_CSS_EQUALS_PARENT))
    style->equals_parent = _gtk_bitmask_set (style->equals_parent, id, TRUE);
  if (dependencies & (GTK_CSS_DEPENDS_ON_COLOR))
    style->depends_on_color = _gtk_bitmask_set (style->depends_on_color, id, TRUE);
  if (dependencies & (GTK_CSS_DEPENDS_ON_FONT_SIZE))
    style->depends_on_font_size = _gtk_bitmask_set (style->depends_on_font_size, id, TRUE);

  _gtk_css_value_unref (value);
  _gtk_css_value_unref (specified);
}

