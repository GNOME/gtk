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

#include "gtkintl.h"

enum {
  PROP_0,
  PROP_SUBPROPERTIES,
};

G_DEFINE_TYPE (GtkCssShorthandProperty, _gtk_css_shorthand_property, GTK_TYPE_STYLE_PROPERTY)

static void
gtk_css_shorthand_property_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GtkCssShorthandProperty *property = GTK_CSS_SHORTHAND_PROPERTY (object);
  const char **subproperties;
  guint i;

  switch (prop_id)
    {
    case PROP_SUBPROPERTIES:
      subproperties = g_value_get_boxed (value);
      g_assert (subproperties);
      for (i = 0; subproperties[i] != NULL; i++)
        {
          GtkStyleProperty *subproperty = _gtk_style_property_lookup (subproperties[i]);
          g_assert (GTK_IS_CSS_STYLE_PROPERTY (subproperty));
          g_ptr_array_add (property->subproperties, subproperty);
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_gtk_css_shorthand_property_assign (GtkStyleProperty   *property,
                                    GtkStyleProperties *props,
                                    GtkStateFlags       state,
                                    const GValue       *value)
{
  GParameter *parameters;
  guint i, n_parameters;

  parameters = _gtk_style_property_unpack (property, value, &n_parameters);

  for (i = 0; i < n_parameters; i++)
    {
      _gtk_style_property_assign (_gtk_style_property_lookup (parameters[i].name),
                                  props,
                                  state,
                                  &parameters[i].value);
      g_value_unset (&parameters[i].value);
    }
  g_free (parameters);
}

static void
_gtk_css_shorthand_property_query (GtkStyleProperty   *property,
                                   GtkStyleProperties *props,
                                   GtkStateFlags       state,
			           GtkStylePropertyContext *context,
                                   GValue             *value)
{
  property->pack_func (value, props, state, context);
}

static void
_gtk_css_shorthand_property_class_init (GtkCssShorthandPropertyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkStylePropertyClass *property_class = GTK_STYLE_PROPERTY_CLASS (klass);

  object_class->set_property = gtk_css_shorthand_property_set_property;

  g_object_class_install_property (object_class,
                                   PROP_SUBPROPERTIES,
                                   g_param_spec_boxed ("subproperties",
                                                       P_("Subproperties"),
                                                       P_("The list of subproperties"),
                                                       G_TYPE_STRV,
                                                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  property_class->assign = _gtk_css_shorthand_property_assign;
  property_class->query = _gtk_css_shorthand_property_query;
}

static void
_gtk_css_shorthand_property_init (GtkCssShorthandProperty *shorthand)
{
  shorthand->subproperties = g_ptr_array_new_with_free_func (g_object_unref);
}

GtkCssStyleProperty *
_gtk_css_shorthand_property_get_subproperty (GtkCssShorthandProperty *shorthand,
                                             guint                    property)
{
  g_return_val_if_fail (GTK_IS_CSS_SHORTHAND_PROPERTY (shorthand), NULL);
  g_return_val_if_fail (property < shorthand->subproperties->len, NULL);

  return g_ptr_array_index (shorthand->subproperties, property);
}

guint
_gtk_css_shorthand_property_get_n_subproperties (GtkCssShorthandProperty *shorthand)
{
  g_return_val_if_fail (GTK_IS_CSS_SHORTHAND_PROPERTY (shorthand), 0);
  
  return shorthand->subproperties->len;
}

