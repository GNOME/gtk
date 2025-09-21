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

struct _ColorEditor
{
  GtkWidget parent_instance;

  unsigned int symbolic;
  GdkRGBA color;

  GtkDropDown *paint;
  GtkStack *stack;
  GtkWidget *indicator;
  GtkColorDialogButton *custom;
};

enum
{
  PROP_SYMBOLIC = 1,
  PROP_COLOR,
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
    case PROP_SYMBOLIC:
      color_editor_set_symbolic (self, g_value_get_uint (value));
      break;

    case PROP_COLOR:
      color_editor_set_color (self, g_value_get_boxed (value));
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
    case PROP_SYMBOLIC:
      g_value_set_uint (value, self->symbolic);
      break;

    case PROP_COLOR:
      g_value_set_boxed (value, &self->color);
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

  object_class->set_property = color_editor_set_property;
  object_class->get_property = color_editor_get_property;
  object_class->dispose = color_editor_dispose;
  object_class->finalize = color_editor_finalize;

  properties[PROP_SYMBOLIC] =
    g_param_spec_uint ("symbolic", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_COLOR] =
    g_param_spec_boxed ("color", NULL, NULL,
                        GDK_TYPE_RGBA,
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
color_editor_set_symbolic (ColorEditor  *self,
                           unsigned int  symbolic)
{
  g_return_if_fail (COLOR_IS_EDITOR (self));

  if (self->symbolic == symbolic)
    return;

  self->symbolic = symbolic;

  gtk_drop_down_set_selected (self->paint, symbolic);

  if (symbolic == 0 || symbolic - 1 <= GTK_SYMBOLIC_COLOR_ACCENT)
    {
      const char *cls[] = { "none", "foreground", "error", "warning", "success", "accent" };
      const char *classes[3] = { "indicator", NULL, NULL };

      gtk_stack_set_visible_child_name (self->stack, "indicator");

      classes[1] = cls[symbolic];
      gtk_widget_set_css_classes (self->indicator, classes);
    }
  else
    {
      gtk_stack_set_visible_child_name (self->stack, "custom");
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SYMBOLIC]);
}

unsigned int
color_editor_get_symbolic (ColorEditor *self)
{
  g_return_val_if_fail (COLOR_IS_EDITOR (self), 0);

  return self->symbolic;
}

void
color_editor_set_color (ColorEditor   *self,
                        const GdkRGBA *color)
{
  g_return_if_fail (COLOR_IS_EDITOR (self));

  self->color = *color;

  gtk_color_dialog_button_set_rgba (self->custom, &self->color);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLOR]);
}

const GdkRGBA *
color_editor_get_color (ColorEditor *self)
{
  g_return_val_if_fail (COLOR_IS_EDITOR (self), NULL);

  return &self->color;
}

/* }}} */

/* vim:set foldmethod=marker: */
