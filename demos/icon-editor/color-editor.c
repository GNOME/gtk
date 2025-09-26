/*
 * Copyright © 2025 Red Hat, Inc
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

#include "color-editor.h"
#include "color-paintable.h"

struct _ColorEditor
{
  GtkWidget parent_instance;

  unsigned int color_type;
  GdkRGBA color;

  GtkDropDown *paint;
  GtkStack *stack;
  ColorPaintable *indicator;
  GtkColorDialogButton *custom;
};

enum
{
  PROP_COLOR_TYPE = 1,
  PROP_SYMBOLIC,
  PROP_COLOR,
  PROP_ALPHA,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

/* {{{ Callbacks */


/* }}} */
/* {{{ GObject boilerplate */

struct _ColorEditorClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (ColorEditor, color_editor, GTK_TYPE_WIDGET)

static void
color_editor_init (ColorEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
color_editor_set_property (GObject      *object,
                           unsigned int  prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  ColorEditor *self = COLOR_EDITOR (object);

  switch (prop_id)
    {
    case PROP_COLOR_TYPE:
      color_editor_set_color_type (self, g_value_get_uint (value));
      break;

    case PROP_COLOR:
      color_editor_set_color (self, g_value_get_boxed (value));
      break;

    case PROP_ALPHA:
      color_editor_set_alpha (self, g_value_get_float (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
color_editor_get_property (GObject      *object,
                           unsigned int  prop_id,
                           GValue       *value,
                           GParamSpec   *pspec)
{
  ColorEditor *self = COLOR_EDITOR (object);

  switch (prop_id)
    {
    case PROP_COLOR_TYPE:
      g_value_set_uint (value, self->color_type);
      break;

    case PROP_SYMBOLIC:
      g_value_set_enum (value, CLAMP (self->color_type - 1, GTK_SYMBOLIC_COLOR_FOREGROUND, GTK_SYMBOLIC_COLOR_ACCENT));
      break;

    case PROP_COLOR:
      g_value_set_boxed (value, &self->color);
      break;

    case PROP_ALPHA:
      g_value_set_float (value, self->color.alpha);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
color_editor_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), COLOR_EDITOR_TYPE);

  G_OBJECT_CLASS (color_editor_parent_class)->dispose (object);
}

static void
color_editor_finalize (GObject *object)
{
  //ColorEditor *self = COLOR_EDITOR (object);

  G_OBJECT_CLASS (color_editor_parent_class)->finalize (object);
}

static void
color_editor_class_init (ColorEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  g_type_ensure (color_paintable_get_type ());

  object_class->set_property = color_editor_set_property;
  object_class->get_property = color_editor_get_property;
  object_class->dispose = color_editor_dispose;
  object_class->finalize = color_editor_finalize;

  properties[PROP_COLOR_TYPE] =
    g_param_spec_uint ("color-type", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_SYMBOLIC] =
    g_param_spec_enum ("symbolic", NULL, NULL,
                       GTK_TYPE_SYMBOLIC_COLOR, GTK_SYMBOLIC_COLOR_FOREGROUND,
                       G_PARAM_READABLE | G_PARAM_STATIC_NAME);

  properties[PROP_COLOR] =
    g_param_spec_boxed ("color", NULL, NULL,
                        GDK_TYPE_RGBA,
                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_ALPHA] =
    g_param_spec_float ("alpha", NULL, NULL,
                        0, 1, 1,
                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/Shaper/color-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, ColorEditor, paint);
  gtk_widget_class_bind_template_child (widget_class, ColorEditor, stack);
  gtk_widget_class_bind_template_child (widget_class, ColorEditor, indicator);
  gtk_widget_class_bind_template_child (widget_class, ColorEditor, custom);

  gtk_widget_class_set_css_name (widget_class, "ColorEditor");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

/* }}} */
/* {{{ Public API */

ColorEditor *
color_editor_new (void)
{
  return g_object_new (COLOR_EDITOR_TYPE, NULL);
}

void
color_editor_set_color_type (ColorEditor  *self,
                             unsigned int  color_type)
{
  g_return_if_fail (COLOR_IS_EDITOR (self));

  if (self->color_type == color_type)
    return;

  self->color_type = color_type;

  gtk_drop_down_set_selected (self->paint, color_type);

  if (color_type == 0)
    gtk_stack_set_visible_child_name (self->stack, "none");
  else if (color_type - 1 <= GTK_SYMBOLIC_COLOR_ACCENT)
    gtk_stack_set_visible_child_name (self->stack, "indicator");
  else
    gtk_stack_set_visible_child_name (self->stack, "custom");

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLOR_TYPE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SYMBOLIC]);
}

unsigned int
color_editor_get_color_type (ColorEditor *self)
{
  g_return_val_if_fail (COLOR_IS_EDITOR (self), 0);

  return self->color_type;
}

void
color_editor_set_color (ColorEditor   *self,
                        const GdkRGBA *color)
{
  g_return_if_fail (COLOR_IS_EDITOR (self));

  self->color = *color;

  gtk_color_dialog_button_set_rgba (self->custom, &self->color);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLOR]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ALPHA]);
}

const GdkRGBA *
color_editor_get_color (ColorEditor *self)
{
  g_return_val_if_fail (COLOR_IS_EDITOR (self), NULL);

  return &self->color;
}

void
color_editor_set_alpha (ColorEditor *self,
                        float        alpha)
{
  g_return_if_fail (COLOR_IS_EDITOR (self));

  if (self->color.alpha == alpha)
    return;

  self->color.alpha = alpha;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ALPHA]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLOR]);
}

float
color_editor_get_alpha (ColorEditor *self)
{
  g_return_val_if_fail (COLOR_IS_EDITOR (self), 1);

  return self->color.alpha;
}

/* }}} */

/* vim:set foldmethod=marker: */
