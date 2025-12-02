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

#include "transform-editor.h"

struct _TransformEditor
{
  GtkWidget parent_instance;

  SvgValue *value;

  GtkWidget *box;
  GtkDropDown *primitive_transform;
  GtkSpinButton *transform_angle;
  GtkWidget *center_row;
  GtkSpinButton *transform_x;
  GtkSpinButton *transform_y;
  GtkWidget *transform_params;
  GtkSpinButton *transform_param0;
  GtkSpinButton *transform_param1;
  GtkSpinButton *transform_param2;
  GtkSpinButton *transform_param3;
  GtkSpinButton *transform_param4;
  GtkSpinButton *transform_param5;
};

enum
{
  PROP_TRANSFORM = 1,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

/* {{{ Callbacks */

static GdkPaintable *
transform_get_icon (GObject      *object,
                    unsigned int  position)
{
  g_autofree char *path = NULL;
  const char *names[] = {
    "identity-symbolic",
    "translate-symbolic",
    "scale-symbolic",
    "rotate-symbolic",
    "shear-x-symbolic",
    "shear-y-symbolic",
    "transform-symbolic",
  };

  if (position == GTK_INVALID_LIST_POSITION)
    return NULL;

  path = g_strdup_printf ("/org/gtk/Shaper/%s.svg", names[position]);

  return g_object_new (GTK_TYPE_SVG,
                       "resource", path,
                       "playing", 1,
                       NULL);
}

static GtkText *
get_text (GtkSpinButton *spin)
{
  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (spin));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (GTK_IS_TEXT (child))
        return GTK_TEXT (child);
    }

  return NULL;
}

static void
transform_type_changed (TransformEditor *self)
{
  gtk_widget_set_visible (GTK_WIDGET (self->center_row), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->transform_angle), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->transform_params), FALSE);
  gtk_spin_button_set_value (self->transform_x, 0);
  gtk_spin_button_set_value (self->transform_y, 0);
  gtk_spin_button_set_value (self->transform_angle, 0);
  gtk_spin_button_set_value (self->transform_param0, 0);
  gtk_spin_button_set_value (self->transform_param1, 1);
  gtk_spin_button_set_value (self->transform_param2, 1);
  gtk_spin_button_set_value (self->transform_param3, 0);
  gtk_spin_button_set_value (self->transform_param4, 0);
  gtk_spin_button_set_value (self->transform_param5, 0);

  switch (gtk_drop_down_get_selected (self->primitive_transform))
    {
    case TRANSFORM_NONE:
      break;
    case TRANSFORM_SKEW_X:
    case TRANSFORM_SKEW_Y:
      gtk_widget_set_visible (GTK_WIDGET (self->transform_angle), TRUE);
      gtk_text_set_placeholder_text (get_text (self->transform_angle), "Angle…");
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->transform_angle), "Angle");
      break;
    case TRANSFORM_ROTATE:
      gtk_widget_set_visible (GTK_WIDGET (self->transform_angle), TRUE);
      gtk_widget_set_visible (GTK_WIDGET (self->center_row), TRUE);
      gtk_text_set_placeholder_text (get_text (self->transform_angle), "Angle…");
      gtk_text_set_placeholder_text (get_text (self->transform_x), "Center X…");
      gtk_text_set_placeholder_text (get_text (self->transform_y), "Center Y‥");
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->transform_angle), "Angle");
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->transform_x), "Center X");
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->transform_y), "Center X");
      break;
    case TRANSFORM_TRANSLATE:
      gtk_widget_set_visible (GTK_WIDGET (self->center_row), TRUE);
      gtk_text_set_placeholder_text (get_text (self->transform_x), "X Shift…");
      gtk_text_set_placeholder_text (get_text (self->transform_y), "Y Shift‥");
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->transform_x), "X Shift");
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->transform_y), "Y Shift");
      break;
    case TRANSFORM_SCALE:
      gtk_spin_button_set_value (self->transform_x, 1);
      gtk_spin_button_set_value (self->transform_y, 1);
      gtk_widget_set_visible (GTK_WIDGET (self->center_row), TRUE);
      gtk_text_set_placeholder_text (get_text (self->transform_x), "X Scale‥");
      gtk_text_set_placeholder_text (get_text (self->transform_y), "Y Scale‥");
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->transform_x), "X Scale");
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->transform_y), "Y Scale");
      break;
    case TRANSFORM_MATRIX:
      gtk_widget_set_visible (GTK_WIDGET (self->transform_params), TRUE);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
transform_changed (TransformEditor *self)
{
  SvgValue *value;
  double angle, x, y;
  double params[6];

  angle = gtk_spin_button_get_value (self->transform_angle);
  x = gtk_spin_button_get_value (self->transform_x);
  y = gtk_spin_button_get_value (self->transform_y);
  params[0] = gtk_spin_button_get_value (self->transform_param0);
  params[1] = gtk_spin_button_get_value (self->transform_param1);
  params[2] = gtk_spin_button_get_value (self->transform_param2);
  params[3] = gtk_spin_button_get_value (self->transform_param3);
  params[4] = gtk_spin_button_get_value (self->transform_param4);
  params[5] = gtk_spin_button_get_value (self->transform_param5);

  switch (gtk_drop_down_get_selected (self->primitive_transform))
    {
    case TRANSFORM_NONE:
      value = svg_transform_new_none ();
      break;
    case TRANSFORM_TRANSLATE:
      value = svg_transform_new_translate (x, y);
      break;
    case TRANSFORM_ROTATE:
      value = svg_transform_new_rotate (angle, x, y);
      break;
    case TRANSFORM_SCALE:
      value = svg_transform_new_scale (x, y);
      break;
    case TRANSFORM_SKEW_X:
      value = svg_transform_new_skew_x (angle);
      break;
    case TRANSFORM_SKEW_Y:
      value = svg_transform_new_skew_y (angle);
      break;
    case TRANSFORM_MATRIX:
      value = svg_transform_new_matrix (params);
      break;
    default:
      g_assert_not_reached ();
    }

  transform_editor_set_transform (self, value);

  svg_value_unref (value);
}

/* }}} */
/* {{{ GObject boilerplate */

struct _TransformEditorClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (TransformEditor, transform_editor, GTK_TYPE_WIDGET)

static void
unbutton_spin (GtkSpinButton *spin)
{
  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (spin));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (GTK_IS_BUTTON (child))
        gtk_widget_set_visible (child, FALSE);
    }
}

static void
transform_editor_init (TransformEditor *self)
{
  self->value = svg_transform_parse ("none");

  gtk_widget_init_template (GTK_WIDGET (self));

  /* We want numeric entries, but there's no space
   * for buttons, so...
   */
  unbutton_spin (self->transform_x);
  unbutton_spin (self->transform_y);
  unbutton_spin (self->transform_angle);
  unbutton_spin (self->transform_param0);
  unbutton_spin (self->transform_param1);
  unbutton_spin (self->transform_param2);
  unbutton_spin (self->transform_param3);
  unbutton_spin (self->transform_param4);
  unbutton_spin (self->transform_param5);

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->primitive_transform));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (GTK_IS_TOGGLE_BUTTON (child))
        {
          for (GtkWidget *grand_child = gtk_widget_get_first_child (child);
               grand_child != NULL;
               grand_child = gtk_widget_get_next_sibling (grand_child))
            {
              if (GTK_IS_BOX (grand_child))
                {
                  for (GtkWidget *gg_child = gtk_widget_get_first_child (grand_child);
                       gg_child != NULL;
                       gg_child = gtk_widget_get_next_sibling (gg_child))
                    {
                      if (strcmp (G_OBJECT_TYPE_NAME (gg_child), "GtkBuiltinIcon") == 0)
                        {
                          gtk_widget_set_visible (gg_child, FALSE);
                          break;
                        }
                    }
                  break;
                }
            }
          break;
        }
    }

  transform_type_changed (self);
}

static void
transform_editor_set_property (GObject      *object,
                               unsigned int  prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  TransformEditor *self = TRANSFORM_EDITOR (object);

  switch (prop_id)
    {
    case PROP_TRANSFORM:
      transform_editor_set_transform (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
transform_editor_get_property (GObject      *object,
                               unsigned int  prop_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  TransformEditor *self = TRANSFORM_EDITOR (object);

  switch (prop_id)
    {
    case PROP_TRANSFORM:
      g_value_set_boxed (value, self->value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
transform_editor_dispose (GObject *object)
{
  //TransformEditor *self = TRANSFORM_EDITOR (object);

  gtk_widget_dispose_template (GTK_WIDGET (object), transform_editor_get_type ());

  G_OBJECT_CLASS (transform_editor_parent_class)->dispose (object);
}

static void
transform_editor_finalize (GObject *object)
{
  TransformEditor *self = TRANSFORM_EDITOR (object);

  svg_value_unref (self->value);

  G_OBJECT_CLASS (transform_editor_parent_class)->finalize (object);
}

static void
transform_editor_class_init (TransformEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = transform_editor_set_property;
  object_class->get_property = transform_editor_get_property;
  object_class->dispose = transform_editor_dispose;
  object_class->finalize = transform_editor_finalize;

  properties[PROP_TRANSFORM] =
    g_param_spec_boxed ("transform", NULL, NULL,
                        svg_value_get_type (),
                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/Shaper/transform-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, TransformEditor, box);
  gtk_widget_class_bind_template_child (widget_class, TransformEditor, primitive_transform);
  gtk_widget_class_bind_template_child (widget_class, TransformEditor, center_row);
  gtk_widget_class_bind_template_child (widget_class, TransformEditor, transform_x);
  gtk_widget_class_bind_template_child (widget_class, TransformEditor, transform_y);
  gtk_widget_class_bind_template_child (widget_class, TransformEditor, transform_angle);
  gtk_widget_class_bind_template_child (widget_class, TransformEditor, transform_params);
  gtk_widget_class_bind_template_child (widget_class, TransformEditor, transform_param0);
  gtk_widget_class_bind_template_child (widget_class, TransformEditor, transform_param1);
  gtk_widget_class_bind_template_child (widget_class, TransformEditor, transform_param2);
  gtk_widget_class_bind_template_child (widget_class, TransformEditor, transform_param3);
  gtk_widget_class_bind_template_child (widget_class, TransformEditor, transform_param4);
  gtk_widget_class_bind_template_child (widget_class, TransformEditor, transform_param5);

  gtk_widget_class_bind_template_callback (widget_class, transform_changed);
  gtk_widget_class_bind_template_callback (widget_class, transform_type_changed);
  gtk_widget_class_bind_template_callback (widget_class, transform_get_icon);

  gtk_widget_class_set_css_name (widget_class, "TransformEditor");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

/*  }}} */
/* {{{ Public API */

TransformEditor *
transform_editor_new (void)
{
  return g_object_new (transform_editor_get_type (), NULL);
}

void
transform_editor_set_transform (TransformEditor *self,
                                SvgValue        *value)
{
  TransformType type;
  double params[6] = { 0, };

  g_return_if_fail (TRANSFORM_IS_EDITOR (self));

  if (svg_value_equal (self->value, value))
    return;

  svg_value_unref (self->value);
  self->value = svg_value_ref (value);

  type = svg_transform_get_primitive (value, 0, params);

  gtk_drop_down_set_selected (self->primitive_transform, type);
  switch (type)
    {
    case TRANSFORM_NONE:
      break;
    case TRANSFORM_TRANSLATE:
    case TRANSFORM_SCALE:
      gtk_spin_button_set_value (self->transform_x, params[0]);
      gtk_spin_button_set_value (self->transform_y, params[1]);
      break;
    case TRANSFORM_ROTATE:
      gtk_spin_button_set_value (self->transform_angle, params[0]);
      gtk_spin_button_set_value (self->transform_x, params[1]);
      gtk_spin_button_set_value (self->transform_y, params[2]);
      break;
    case TRANSFORM_SKEW_X:
    case TRANSFORM_SKEW_Y:
      gtk_spin_button_set_value (self->transform_angle, params[0]);
      break;
    case TRANSFORM_MATRIX:
      gtk_spin_button_set_value (self->transform_param0, params[0]);
      gtk_spin_button_set_value (self->transform_param1, params[1]);
      gtk_spin_button_set_value (self->transform_param2, params[2]);
      gtk_spin_button_set_value (self->transform_param3, params[3]);
      gtk_spin_button_set_value (self->transform_param4, params[4]);
      gtk_spin_button_set_value (self->transform_param5, params[5]);
      break;
    default:
      g_assert_not_reached ();
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TRANSFORM]);
}

SvgValue *
transform_editor_get_transform (TransformEditor *self)
{
  g_return_val_if_fail (TRANSFORM_IS_EDITOR (self), NULL);

  return self->value;
}

/* }}} */

/* vim:set foldmethod=marker: */
