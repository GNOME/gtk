/*
 * Copyright © 2026 Red Hat, Inc
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "number-editor.h"

struct _NumberEditor
{
  GtkWidget parent_instance;

  GtkSpinButton *value_spin;
  GtkDropDown *unit_dropdown;
  double min, max, value;
  unsigned int unit;
};

enum
{
  PROP_MIN = 1,
  PROP_MAX,
  PROP_VALUE,
  PROP_UNIT,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

/* {{{ GObject boilerplate */

struct _NumberEditorClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (NumberEditor, number_editor, GTK_TYPE_WIDGET)

static void
number_editor_init (NumberEditor *self)
{
  self->unit = SVG_UNIT_NUMBER;
  self->min = -DBL_MAX;
  self->max = DBL_MAX;
  self->value = 0;

  gtk_widget_init_template (GTK_WIDGET (self));

  /* We want a numeric entry, but there's no space
   * for buttons, so...
   */
  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->value_spin));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (GTK_IS_BUTTON (child))
        gtk_widget_set_visible (child, FALSE);
    }
}

static void
number_editor_set_property (GObject      *object,
                            unsigned int  prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  NumberEditor *self = NUMBER_EDITOR (object);

  switch (prop_id)
    {
    case PROP_MIN:
      number_editor_set_min (self, g_value_get_double (value));
      break;

    case PROP_MAX:
      number_editor_set_max (self, g_value_get_double (value));
      break;

    case PROP_VALUE:
      number_editor_set_value (self, g_value_get_double (value));
      break;

    case PROP_UNIT:
      number_editor_set_unit (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
number_editor_get_property (GObject      *object,
                            unsigned int  prop_id,
                            GValue       *value,
                            GParamSpec   *pspec)
{
  NumberEditor *self = NUMBER_EDITOR (object);

  switch (prop_id)
    {
    case PROP_MIN:
      g_value_set_double (value, self->min);
      break;

    case PROP_MAX:
      g_value_set_double (value, self->max);
      break;

    case PROP_VALUE:
      g_value_set_double (value, self->value);
      break;

    case PROP_UNIT:
      g_value_set_uint (value, self->unit);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
number_editor_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), number_editor_get_type ());

  G_OBJECT_CLASS (number_editor_parent_class)->dispose (object);
}

static void
number_editor_finalize (GObject *object)
{
  //NumberEditor *self = NUMBER_EDITOR (object);

  G_OBJECT_CLASS (number_editor_parent_class)->finalize (object);
}

static void
number_editor_class_init (NumberEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = number_editor_set_property;
  object_class->get_property = number_editor_get_property;
  object_class->dispose = number_editor_dispose;
  object_class->finalize = number_editor_finalize;

  properties[PROP_MIN] =
    g_param_spec_double ("min", NULL, NULL,
                         -DBL_MAX, DBL_MAX, -DBL_MAX,
                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_MAX] =
    g_param_spec_double ("max", NULL, NULL,
                         -DBL_MAX, DBL_MAX, DBL_MAX,
                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_VALUE] =
    g_param_spec_double ("value", NULL, NULL,
                         -DBL_MAX, DBL_MAX, 0,
                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_UNIT] =
    g_param_spec_uint ("unit", NULL, NULL,
                       0, SVG_UNIT_TURN, SVG_UNIT_NUMBER,
                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/Shaper/number-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, NumberEditor, value_spin);
  gtk_widget_class_bind_template_child (widget_class, NumberEditor, unit_dropdown);

  gtk_widget_class_set_css_name (widget_class, "NumberEditor");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

/* }}} */
/* {{{ Public API */

NumberEditor *
number_editor_new (void)
{
  return g_object_new (number_editor_get_type (), NULL);
}

void
number_editor_set_min (NumberEditor  *self,
                       double         min)
{
  g_return_if_fail (NUMBER_IS_EDITOR (self));

  if (self->min == min)
    return;

  self->min = min;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MIN]);
}

double
number_editor_get_min (NumberEditor *self)
{
  g_return_val_if_fail (NUMBER_IS_EDITOR (self), -DBL_MAX);

  return self->min;
}

void
number_editor_set_max (NumberEditor  *self,
                       double         max)
{
  g_return_if_fail (NUMBER_IS_EDITOR (self));

  if (self->max == max)
    return;

  self->max = max;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX]);
}

double
number_editor_get_max (NumberEditor *self)
{
  g_return_val_if_fail (NUMBER_IS_EDITOR (self), DBL_MAX);

  return self->max;
}

void
number_editor_set_value (NumberEditor  *self,
                         double         value)
{
  g_return_if_fail (NUMBER_IS_EDITOR (self));

  if (self->value == value)
    return;

  self->value = value;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE]);
}

double
number_editor_get_value (NumberEditor *self)
{
  g_return_val_if_fail (NUMBER_IS_EDITOR (self), 0);

  return self->value;
}

void
number_editor_set_unit (NumberEditor *self,
                        SvgUnit       unit)
{
  g_return_if_fail (NUMBER_IS_EDITOR (self));

  if (self->unit == unit)
    return;

  self->unit = unit;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UNIT]);
}

SvgUnit
number_editor_get_unit (NumberEditor *self)
{
  g_return_val_if_fail (NUMBER_IS_EDITOR (self), SVG_UNIT_NUMBER);

  return (SvgUnit) self->unit;
}

void
number_editor_set (NumberEditor *self,
                   double        value,
                   SvgUnit       unit)
{
  g_return_if_fail (NUMBER_IS_EDITOR (self));

  self->value = value;
  self->unit = unit;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UNIT]);
}

void
number_editor_get (NumberEditor *self,
                   double       *value,
                   SvgUnit      *unit)
{
  g_return_if_fail (NUMBER_IS_EDITOR (self));

  *value = self->value;
  *unit = self->unit;
}

/* }}} */
/* vim:set foldmethod=marker: */
