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
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcssstringvalueprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstransitionprivate.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtksettings.h"
#include "gtkstyleanimationprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstyleproviderprivate.h"
#include "gtkcssdimensionvalueprivate.h"

G_DEFINE_TYPE (GtkCssStaticStyle, gtk_css_static_style, GTK_TYPE_CSS_STYLE)

static GtkCssValue *
gtk_css_static_style_get_value (GtkCssStyle *style,
                                guint        id)
{
  /* This is called a lot, so we avoid a dynamic type check here */
  GtkCssStaticStyle *sstyle = (GtkCssStaticStyle *) style;

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

static GtkCssStaticStyle *
gtk_css_static_style_get_static_style (GtkCssStyle *style)
{
  return (GtkCssStaticStyle *)style;
}

static void
gtk_css_static_style_class_init (GtkCssStaticStyleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCssStyleClass *style_class = GTK_CSS_STYLE_CLASS (klass);

  object_class->dispose = gtk_css_static_style_dispose;

  style_class->get_value = gtk_css_static_style_get_value;
  style_class->get_section = gtk_css_static_style_get_section;
  style_class->get_static_style = gtk_css_static_style_get_static_style;
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
      default_style = gtk_css_static_style_new_compute (GTK_STYLE_PROVIDER (settings),
                                                        NULL,
                                                        NULL,
                                                        TRUE);
      g_object_set_data_full (G_OBJECT (settings), I_("gtk-default-style"),
                              default_style, clear_default_style);
    }

  return default_style;
}

G_STATIC_ASSERT (GTK_CSS_PROPERTY_BORDER_TOP_STYLE == GTK_CSS_PROPERTY_BORDER_TOP_WIDTH - 1);
G_STATIC_ASSERT (GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE == GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH - 1);
G_STATIC_ASSERT (GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE == GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH - 1);
G_STATIC_ASSERT (GTK_CSS_PROPERTY_BORDER_LEFT_STYLE == GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH - 1);
G_STATIC_ASSERT (GTK_CSS_PROPERTY_OUTLINE_STYLE == GTK_CSS_PROPERTY_OUTLINE_WIDTH - 1);

static inline gboolean
is_border_style_special_case (guint id)
{
  switch (id)
    {
    case GTK_CSS_PROPERTY_BORDER_TOP_STYLE:
    case GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE:
    case GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE:
    case GTK_CSS_PROPERTY_BORDER_LEFT_STYLE:
    case GTK_CSS_PROPERTY_OUTLINE_STYLE:
      return TRUE;
    default:
      return FALSE;
   }
}

static void
gtk_css_static_style_compute_values (GtkCssStaticStyle *style,
                                     GtkStyleProvider  *provider,
                                     GtkCssStyle       *parent_style,
                                     GtkCssLookup      *lookup)
{
  GtkCssLookupValue *values = lookup->values;
  guint id;
  GtkCssValue **parent_values;
  guint border_prop[] = {
    GTK_CSS_PROPERTY_BORDER_TOP_STYLE,
    GTK_CSS_PROPERTY_BORDER_LEFT_STYLE,
    GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE,
    GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE,
    GTK_CSS_PROPERTY_OUTLINE_STYLE,
    0
  };
  int b = 0;

  if (GTK_IS_CSS_STATIC_STYLE (parent_style))
    parent_values = GTK_CSS_STATIC_STYLE (parent_style)->values;
  else
    parent_values = NULL;

  for (id = 0; id < GTK_CSS_PROPERTY_N_PROPERTIES; id++)
    {
      GtkCssValue *specified = values[id].value;

      /* http://www.w3.org/TR/css3-cascade/#cascade
       * Then, for every element, the value for each property can be found
       * by following this pseudo-algorithm:
       * 1) Identify all declarations that apply to the element
       */
      if (specified)
        {
          style->values[id] = _gtk_css_value_compute (specified, id, provider, (GtkCssStyle *)style, parent_style);

          /* special case according to http://dev.w3.org/csswg/css-backgrounds/#the-border-width */
          if (id == border_prop[b])
            {
              b++;
              /* We have them ordered in gtkcssstylepropertyimpl.c accordingly, so the
               * border styles are computed before the border widths.
               * Note that we rely on ..._WIDTH == ..._STYLE + 1 here.
               */
              GtkBorderStyle border_style = _gtk_css_border_style_value_get (style->values[id]);
              if (border_style == GTK_BORDER_STYLE_NONE || border_style == GTK_BORDER_STYLE_HIDDEN)
                {
                  id++;
                  style->values[id] = gtk_css_dimension_value_new (0, GTK_CSS_NUMBER);
                }
            }
        }
      else if (parent_values && gtk_css_style_property_is_inherit (id))
        {
          style->values[id] = _gtk_css_value_ref (parent_values[id]);

          /* the border properties are not inherit, so no need to check the special case here */
        }
      else if (parent_style && gtk_css_style_property_is_inherit (id))
        {
          style->values[id] = _gtk_css_value_ref (gtk_css_style_get_value (parent_style, id));

          /* the border properties are not inherit, so no need to check the special case here */
        }
      else
        {
          style->values[id] = _gtk_css_initial_value_new_compute (id, provider, (GtkCssStyle *)style, parent_style);

          /* special case according to http://dev.w3.org/csswg/css-backgrounds/#the-border-width */
          if (id == border_prop[b])
            {
              b++;
              /* no need to check the value. The initial value of the border-style properties is none */
              id++;
              style->values[id] = gtk_css_dimension_value_new (0, GTK_CSS_NUMBER);
            }
        }
    }

  if (lookup->has_section)
    {
      style->sections = g_ptr_array_new_full (GTK_CSS_PROPERTY_N_PROPERTIES, maybe_unref_section);
      for (id = 0; id < GTK_CSS_PROPERTY_N_PROPERTIES; id++)
        {
          GtkCssSection *section = values[id].section;
          if (section)
            g_ptr_array_index (style->sections, id) = gtk_css_section_ref (section);
        }
    }
}

GtkCssStyle *
gtk_css_static_style_new_compute (GtkStyleProvider    *provider,
                                  const GtkCssMatcher *matcher,
                                  GtkCssStyle         *parent,
                                  GtkCssChange         change)
{
  GtkCssStaticStyle *result;
  GtkCssLookup lookup;

  _gtk_css_lookup_init (&lookup);

  if (matcher)
    gtk_style_provider_lookup (provider,
                               matcher,
                               &lookup,
                               change == 0 ? &change : NULL);

  result = g_object_new (GTK_TYPE_CSS_STATIC_STYLE, NULL);

  result->change = change;

  gtk_css_static_style_compute_values (result, provider, parent, &lookup);

  _gtk_css_lookup_destroy (&lookup);

  return GTK_CSS_STYLE (result);
}

GtkCssChange
gtk_css_static_style_get_change (GtkCssStaticStyle *style)
{
  g_return_val_if_fail (GTK_IS_CSS_STATIC_STYLE (style), GTK_CSS_CHANGE_ANY);

  return style->change;
}
