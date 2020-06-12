/* GTK - The GIMP Toolkit
 * Copyright (C) 2020, Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkeditablelabel.h"
#include "gtklabel.h"
#include "gtkstack.h"
#include "gtkentry.h"
#include "gtkbinlayout.h"
#include "gtkgestureclick.h"
#include "gtkprivate.h"
#include "gtkintl.h"

struct _GtkEditableLabel
{
  GtkWidget parent_instance;

  GtkWidget *stack;
  GtkWidget *label;
  GtkWidget *entry;
};

struct _GtkEditableLabelClass
{
  GtkWidgetClass parent_class;
};

enum {
  PROP_LABEL = 1,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (GtkEditableLabel, gtk_editable_label, GTK_TYPE_WIDGET)

static void
gtk_editable_label_stop_editing (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *parameters)
{
  GtkEditableLabel *self = GTK_EDITABLE_LABEL (widget);

  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "label");
  gtk_editable_set_text (GTK_EDITABLE (self->entry),
                         gtk_label_get_label (GTK_LABEL (self->label)));
}

static void
clicked_cb (GtkGestureClick  *gesture,
            int               n_pressed,
            double            x,
            double            y,
            GtkEditableLabel *self)
{
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "entry");
  gtk_widget_grab_focus (self->entry);
}

static void
activate_cb (GtkEntry         *entry,
             GtkEditableLabel *self)
{
  gtk_label_set_label (GTK_LABEL (self->label),
                       gtk_editable_get_text (GTK_EDITABLE (entry)));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "label");
}

static void
gtk_editable_label_init (GtkEditableLabel *self)
{
  GtkGesture *gesture;

  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);

  self->stack = gtk_stack_new ();
  self->label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (self->label), 0.0);
  self->entry = gtk_entry_new ();

  gtk_stack_add_named (GTK_STACK (self->stack), self->label, "label");
  gtk_stack_add_named (GTK_STACK (self->stack), self->entry, "entry");

  gtk_widget_set_parent (self->stack, GTK_WIDGET (self));

  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "pressed", G_CALLBACK (clicked_cb), self);
  gtk_widget_add_controller (self->label, GTK_EVENT_CONTROLLER (gesture));

  g_signal_connect (self->entry, "activate", G_CALLBACK (activate_cb), self);
}

static void
gtk_editable_label_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkEditableLabel *self = GTK_EDITABLE_LABEL (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_editable_label_set_label (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_editable_label_get_property (GObject     *object,
                                 guint        prop_id,
                                 GValue      *value,
                                 GParamSpec  *pspec)
{
  GtkEditableLabel *self = GTK_EDITABLE_LABEL (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, gtk_editable_label_get_label (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_editable_label_dispose (GObject *object)
{
  GtkEditableLabel *self = GTK_EDITABLE_LABEL (object);

  g_clear_pointer (&self->stack, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_editable_label_parent_class)->dispose (object);
}

static void
gtk_editable_label_class_init (GtkEditableLabelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = gtk_editable_label_set_property;
  object_class->get_property = gtk_editable_label_get_property;
  object_class->dispose = gtk_editable_label_dispose;

  properties[PROP_LABEL] = g_param_spec_string ("label",
                                               P_("Label"),
                                               P_("The text of the label"),
                                               "",
                                               GTK_PARAM_READWRITE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_install_action (widget_class, "editing.stop", NULL, gtk_editable_label_stop_editing);

  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Escape, 0,
                                       "editing.stop",
                                       NULL);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

GtkWidget *
gtk_editable_label_new (const char *str)
{
  return g_object_new (GTK_TYPE_EDITABLE_LABEL,
                       "label", str,
                       NULL);
}

void
gtk_editable_label_set_label (GtkEditableLabel *self,
                              const char       *str)
{
  g_return_if_fail (GTK_IS_EDITABLE_LABEL (self));

  gtk_label_set_label (GTK_LABEL (self->label), str);
  gtk_editable_set_text (GTK_EDITABLE (self->entry), str);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LABEL]);
}

const char *
gtk_editable_label_get_label (GtkEditableLabel *self)
{
  g_return_val_if_fail (GTK_IS_EDITABLE_LABEL (self), NULL);

  return gtk_label_get_label (GTK_LABEL (self->label));
}
