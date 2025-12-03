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

#include "alpha-editor.h"

struct _AlphaEditor
{
  GtkWidget parent_instance;

  GtkSpinButton *alpha_spin;
  GtkScale *alpha_scale;

  double alpha;
};

enum
{
  PROP_ALPHA = 1,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

/* {{{ GObject boilerplate */

struct _AlphaEditorClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (AlphaEditor, alpha_editor, GTK_TYPE_WIDGET)

static void
alpha_editor_init (AlphaEditor *self)
{
  self->alpha = 1;

  gtk_widget_init_template (GTK_WIDGET (self));

  /* We want a numeric entry, but there's no space
   * for buttons, so...
   */
  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->alpha_spin));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (GTK_IS_BUTTON (child))
        gtk_widget_set_visible (child, FALSE);
    }
}

static void
alpha_editor_set_property (GObject      *object,
                           unsigned int  prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  AlphaEditor *self = ALPHA_EDITOR (object);

  switch (prop_id)
    {
    case PROP_ALPHA:
      alpha_editor_set_alpha (self, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
alpha_editor_get_property (GObject      *object,
                           unsigned int  prop_id,
                           GValue       *value,
                           GParamSpec   *pspec)
{
  AlphaEditor *self = ALPHA_EDITOR (object);

  switch (prop_id)
    {
    case PROP_ALPHA:
      g_value_set_double (value, self->alpha);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
alpha_editor_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), alpha_editor_get_type ());

  G_OBJECT_CLASS (alpha_editor_parent_class)->dispose (object);
}

static void
alpha_editor_finalize (GObject *object)
{
  //AlphaEditor *self = ALPHA_EDITOR (object);

  G_OBJECT_CLASS (alpha_editor_parent_class)->finalize (object);
}

static void
alpha_editor_class_init (AlphaEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = alpha_editor_set_property;
  object_class->get_property = alpha_editor_get_property;
  object_class->dispose = alpha_editor_dispose;
  object_class->finalize = alpha_editor_finalize;

  properties[PROP_ALPHA] =
    g_param_spec_double ("alpha", NULL, NULL,
                         0, 1, 1,
                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/Shaper/alpha-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, AlphaEditor, alpha_spin);
  gtk_widget_class_bind_template_child (widget_class, AlphaEditor, alpha_scale);

  gtk_widget_class_set_css_name (widget_class, "AlphaEditor");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

/* }}} */
/* {{{ Public API */

AlphaEditor *
alpha_editor_new (void)
{
  return g_object_new (alpha_editor_get_type (), NULL);
}

void
alpha_editor_set_alpha (AlphaEditor *self,
                        double       alpha)
{
  g_return_if_fail (ALPHA_IS_EDITOR (self));

  if (self->alpha == alpha)
    return;

  self->alpha = alpha;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ALPHA]);
}

double
alpha_editor_get_alpha (AlphaEditor *self)
{
  g_return_val_if_fail (ALPHA_IS_EDITOR (self), 1);

  return self->alpha;
}

/* }}} */

/* vim:set foldmethod=marker: */
