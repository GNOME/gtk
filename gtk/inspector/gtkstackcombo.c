/*
 * Copyright (c) 2016 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "gtkstackcombo.h"
#include "gtkbox.h"
#include "gtkstack.h"
#include "gtkcomboboxtext.h"
#include "gtkprivate.h"
#include "gtkintl.h"

struct _GtkStackCombo
{
  GtkBox box;

  GtkComboBox *combo;
  GtkStack *stack;
  GBinding *binding;
};

struct _GtkStackComboClass {
  GtkBoxClass parent_class;
};

enum {
  PROP_0,
  PROP_STACK
};

G_DEFINE_TYPE (GtkStackCombo, gtk_stack_combo, GTK_TYPE_BOX)

static void
gtk_stack_combo_init (GtkStackCombo *self)
{
  self->stack = NULL;
  self->combo = GTK_COMBO_BOX (gtk_combo_box_text_new ());
  gtk_widget_show (GTK_WIDGET (self->combo));
  gtk_box_pack_start (GTK_BOX (self), GTK_WIDGET (self->combo), FALSE, FALSE, 0);
}

static void gtk_stack_combo_set_stack (GtkStackCombo *self,
                                       GtkStack      *stack);

static void
rebuild_combo (GtkStackCombo *self)
{
  gtk_stack_combo_set_stack (self, self->stack);
}

static void
on_child_visible_changed (GtkStackCombo *self)
{
  rebuild_combo (self);
}

static void
add_child (GtkWidget     *widget,
           GtkStackCombo *self)
{
  g_signal_handlers_disconnect_by_func (widget, G_CALLBACK (on_child_visible_changed), self);
  g_signal_connect_swapped (widget, "notify::visible", G_CALLBACK (on_child_visible_changed), self);

  if (gtk_widget_get_visible (widget))
    {
      char *name, *title;

      gtk_container_child_get (GTK_CONTAINER (self->stack), widget,
                               "name", &name,
                              "title", &title,
                               NULL);

      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (self->combo), name, title);

      g_free (name);
      g_free (title);
    }
}

static void
populate_combo (GtkStackCombo *self)
{
  gtk_container_foreach (GTK_CONTAINER (self->stack), (GtkCallback)add_child, self);
}

static void
clear_combo (GtkStackCombo *self)
{
  gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (self->combo));
}

static void
on_stack_child_added (GtkContainer  *container,
                      GtkWidget     *widget,
                      GtkStackCombo *self)
{
  rebuild_combo (self);
}

static void
on_stack_child_removed (GtkContainer  *container,
                        GtkWidget     *widget,
                        GtkStackCombo *self)
{
  g_signal_handlers_disconnect_by_func (widget, G_CALLBACK (on_child_visible_changed), self);
  rebuild_combo (self);
}

static void
disconnect_stack_signals (GtkStackCombo *self)
{
  g_binding_unbind (self->binding);
  self->binding = NULL;
  g_signal_handlers_disconnect_by_func (self->stack, on_stack_child_added, self);
  g_signal_handlers_disconnect_by_func (self->stack, on_stack_child_removed, self);
  g_signal_handlers_disconnect_by_func (self->stack, disconnect_stack_signals, self);
}

static void
connect_stack_signals (GtkStackCombo *self)
{
  g_signal_connect_after (self->stack, "add", G_CALLBACK (on_stack_child_added), self);
  g_signal_connect_after (self->stack, "remove", G_CALLBACK (on_stack_child_removed), self);
  g_signal_connect_swapped (self->stack, "destroy", G_CALLBACK (disconnect_stack_signals), self);
  self->binding = g_object_bind_property (self->stack, "visible-child-name", self->combo, "active-id", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
}

static void
gtk_stack_combo_set_stack (GtkStackCombo *self,
                           GtkStack      *stack)
{
  if (stack)
    g_object_ref (stack);

  if (self->stack)
    {
      disconnect_stack_signals (self);
      clear_combo (self);
      g_clear_object (&self->stack);
    }

  if (stack)
    {
      self->stack = stack;
      populate_combo (self);
      connect_stack_signals (self);
    }
}

static void
gtk_stack_combo_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkStackCombo *self = GTK_STACK_COMBO (object);

  switch (prop_id)
    {
    case PROP_STACK:
      g_value_set_object (value, self->stack);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_stack_combo_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkStackCombo *self = GTK_STACK_COMBO (object);

  switch (prop_id)
    {
    case PROP_STACK:
      if (self->stack != g_value_get_object (value))
        {
          gtk_stack_combo_set_stack (self, g_value_get_object (value));
          g_object_notify (G_OBJECT (self), "stack");
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_stack_combo_dispose (GObject *object)
{
  GtkStackCombo *self = GTK_STACK_COMBO (object);

  gtk_stack_combo_set_stack (self, NULL);

  G_OBJECT_CLASS (gtk_stack_combo_parent_class)->dispose (object);
}

static void
gtk_stack_combo_class_init (GtkStackComboClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->get_property = gtk_stack_combo_get_property;
  object_class->set_property = gtk_stack_combo_set_property;
  object_class->dispose = gtk_stack_combo_dispose;

  g_object_class_install_property (object_class,
                                   PROP_STACK,
                                   g_param_spec_object ("stack",
                                                        P_("Stack"),
                                                        P_("Stack"),
                                                        GTK_TYPE_STACK,
                                                        GTK_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));

  gtk_widget_class_set_css_name (widget_class, "stackcombo");
}
