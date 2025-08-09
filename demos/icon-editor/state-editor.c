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

#include "state-editor.h"
#include "path-paintable-private.h"

struct _StateEditor
{
  GtkWidget parent_instance;

  PathPaintable *paintable;
  guint state;
  GtkSpinButton *discrete_state;
};

enum {
  PROP_PAINTABLE = 1,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

/* {{{ Callbacks */
static void
update_state (StateEditor *self)
{
  int value;
  guint state;

  value = gtk_spin_button_get_value_as_int (self->discrete_state);
  if (value < 0)
    state = STATE_UNSET;
  else
    state = (guint) value;

  if (self->state == state)
    return;

  self->state = state;

  if (self->paintable)
    path_paintable_set_state (self->paintable, self->state);
}

static void
state_changed (StateEditor *self)
{
  guint state;

  if (self->paintable)
    state = path_paintable_get_state (self->paintable);
  else
    state = STATE_UNSET;

  if (state == STATE_UNSET)
    gtk_spin_button_set_value (self->discrete_state, -1);
  else
    gtk_spin_button_set_value (self->discrete_state, (double) state);
}

static void
max_state_changed (StateEditor *self)
{
  double max_state;
  GtkAdjustment *adj;

  if (self->paintable)
    max_state = path_paintable_get_max_state (self->paintable);
  else
    max_state = 0;

  adj = gtk_spin_button_get_adjustment (self->discrete_state);
  gtk_adjustment_set_upper (adj, max_state);
}

/* }}} */
/* {{{ GObject boilerplate */

struct _StateEditorClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (StateEditor, state_editor, GTK_TYPE_WIDGET)

static void
state_editor_init (StateEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
state_editor_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  StateEditor *self = STATE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      state_editor_set_paintable (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
state_editor_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  StateEditor *self = STATE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, self->paintable);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
state_editor_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), STATE_EDITOR_TYPE);

  G_OBJECT_CLASS (state_editor_parent_class)->dispose (object);
}

static void
state_editor_finalize (GObject *object)
{
  StateEditor *self = STATE_EDITOR (object);

  g_clear_object (&self->paintable);

  G_OBJECT_CLASS (state_editor_parent_class)->finalize (object);
}

static void
state_editor_class_init (StateEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = state_editor_set_property;
  object_class->get_property = state_editor_get_property;
  object_class->dispose = state_editor_dispose;
  object_class->finalize = state_editor_finalize;

  properties[PROP_PAINTABLE] =
    g_param_spec_object ("paintable", NULL, NULL,
                         PATH_PAINTABLE_TYPE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/icon-editor/state-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, StateEditor, discrete_state);

  gtk_widget_class_bind_template_callback (widget_class, update_state);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

/* }}} */
/* {{{ Public API */

StateEditor *
state_editor_new (void)
{
  return g_object_new (STATE_EDITOR_TYPE, NULL);
}

void
state_editor_set_paintable (StateEditor   *self,
                            PathPaintable *paintable)
{
  g_return_if_fail (STATE_IS_EDITOR (self));
  g_return_if_fail (paintable == NULL || PATH_IS_PAINTABLE (paintable));

  if (self->paintable == paintable)
    return;

  if (self->paintable)
    {
      g_signal_handlers_disconnect_by_func (self->paintable, state_changed, self);
      g_signal_handlers_disconnect_by_func (self->paintable, max_state_changed, self);
      gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
    }

  g_set_object (&self->paintable, paintable);

  if (self->paintable)
    {
      g_signal_connect_swapped (self->paintable, "notify::state",
                                G_CALLBACK (state_changed), self);
      g_signal_connect_swapped (self->paintable, "notify::max-state",
                                G_CALLBACK (max_state_changed), self);
      gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
    }

  state_changed (self);
  max_state_changed (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAINTABLE]);
}

PathPaintable *
state_editor_get_paintable (StateEditor *self)
{
  g_return_val_if_fail (STATE_IS_EDITOR (self), NULL);

  return self->paintable;
}

/* }}} */

/* vim:set foldmethod=marker: */
