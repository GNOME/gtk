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
#include "gtkeditable.h"
#include "gtklabel.h"
#include "gtkstack.h"
#include "gtkentry.h"
#include "gtkbinlayout.h"
#include "gtkgestureclick.h"
#include "gtkprivate.h"
#include "gtkshortcut.h"
#include "gtkshortcuttrigger.h"
#include "gtkintl.h"

/**
 * SECTION:gtkeditablelabel
 * @Short_description: A label that can be edited
 * @Title: GtkEditableLabel
 * @See_also: #GtkLabel, #GtkEntry
 *
 * A GtkEditableLabel is a #GtkLabel that allows users to
 * edit the text by clicking on the text.
 */

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

static GtkEditable *
gtk_editable_label_get_delegate (GtkEditable *editable)
{
  GtkEditableLabel *self = GTK_EDITABLE_LABEL (editable);

  return GTK_EDITABLE (self->entry);
}

static void
gtk_editable_label_editable_init (GtkEditableInterface *iface)
{
  iface->get_delegate = gtk_editable_label_get_delegate;
}


G_DEFINE_TYPE_WITH_CODE (GtkEditableLabel, gtk_editable_label, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                gtk_editable_label_editable_init))

static void
gtk_editable_label_start_editing (GtkWidget  *widget,
                                  const char *action_name,
                                  GVariant   *parameters)
{
  GtkEditableLabel *self = GTK_EDITABLE_LABEL (widget);

  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "entry");
  gtk_widget_grab_focus (self->entry);
}

static void
gtk_editable_label_stop_editing (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *parameters)
{
  GtkEditableLabel *self = GTK_EDITABLE_LABEL (widget);

  if (g_variant_get_boolean (parameters))
    {
      gtk_label_set_label (GTK_LABEL (self->label),
                           gtk_editable_get_text (GTK_EDITABLE (self->entry)));
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "label");
    }
  else
    {
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "label");
      gtk_editable_set_text (GTK_EDITABLE (self->entry),
                             gtk_label_get_label (GTK_LABEL (self->label)));
    }
}

static void
clicked_cb (GtkGestureClick *gesture,
            int              n_pressed,
            double           x,
            double           y,
            GtkWidget       *self)
{
  gtk_widget_activate_action (self, "editing.start", NULL);
}

static void
activate_cb (GtkEntry  *entry,
             GtkWidget *self)
{
  gtk_widget_activate_action (self, "editing.stop", "b", TRUE);
}

static void
text_changed (GObject          *entry,
              GParamSpec       *pspec,
              GtkEditableLabel *self)
{
  /* sync the entry text to the label, unless we are editing */
  if (strcmp (gtk_stack_get_visible_child_name (GTK_STACK (self->stack)), "label") == 0)
    {
      const char *text = gtk_editable_get_text (GTK_EDITABLE (self->entry));
      gtk_label_set_label (GTK_LABEL (self->label), text);
    }
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
  g_signal_connect (self->entry, "notify::text", G_CALLBACK (text_changed), self);

  gtk_editable_init_delegate (GTK_EDITABLE (self));
}

static gboolean
gtk_editable_label_grab_focus (GtkWidget *widget)
{
  gtk_editable_label_start_editing (widget, "editing.start", NULL);
  return TRUE;
}

static void
gtk_editable_label_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkEditableLabel *self = GTK_EDITABLE_LABEL (object);

  if (gtk_editable_delegate_set_property (object, prop_id, value, pspec))
    {
      switch (prop_id - 1)
        {
        case GTK_EDITABLE_PROP_TEXT:
          gtk_label_set_label (GTK_LABEL (self->label), g_value_get_string (value));
          break;

        case GTK_EDITABLE_PROP_WIDTH_CHARS:
          gtk_label_set_width_chars (GTK_LABEL (self->label), g_value_get_int (value));
          break;

        case GTK_EDITABLE_PROP_MAX_WIDTH_CHARS:
          gtk_label_set_max_width_chars (GTK_LABEL (self->label), g_value_get_int (value));
          break;

        case GTK_EDITABLE_PROP_XALIGN:
          gtk_label_set_xalign (GTK_LABEL (self->label), g_value_get_float (value));
          break;

        case GTK_EDITABLE_PROP_EDITABLE:
          gtk_widget_action_set_enabled (GTK_WIDGET (self), "editing.start", g_value_get_boolean (value));
          if (!g_value_get_boolean (value))
            gtk_editable_label_stop_editing (GTK_WIDGET (self), "editing.stop", g_variant_new_boolean (FALSE));
          break;

         default: ;
        }
      return;
    }

  switch (prop_id)
    {
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
  if (gtk_editable_delegate_get_property (object, prop_id, value, pspec))
    return;

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_editable_label_dispose (GObject *object)
{
  GtkEditableLabel *self = GTK_EDITABLE_LABEL (object);

  gtk_editable_finish_delegate (GTK_EDITABLE (self));

  g_clear_pointer (&self->stack, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_editable_label_parent_class)->dispose (object);
}

static void
gtk_editable_label_class_init (GtkEditableLabelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkShortcut *shortcut;
  GtkShortcutTrigger *trigger;
  GtkShortcutAction *action;

  object_class->set_property = gtk_editable_label_set_property;
  object_class->get_property = gtk_editable_label_get_property;
  object_class->dispose = gtk_editable_label_dispose;

  widget_class->grab_focus = gtk_editable_label_grab_focus;

  gtk_editable_install_properties (object_class, 1);

  gtk_widget_class_install_action (widget_class, "editing.start", NULL, gtk_editable_label_start_editing);
  gtk_widget_class_install_action (widget_class, "editing.stop", "b", gtk_editable_label_stop_editing);

  trigger = gtk_alternative_trigger_new (
                gtk_alternative_trigger_new (
                    gtk_keyval_trigger_new (GDK_KEY_Return, 0),
                    gtk_keyval_trigger_new (GDK_KEY_ISO_Enter, 0)),
                    gtk_keyval_trigger_new (GDK_KEY_KP_Enter, 0));
  action = gtk_named_action_new ("editing.start");
  shortcut = gtk_shortcut_new (trigger, action);
  gtk_widget_class_add_shortcut (widget_class, shortcut);
  g_object_unref (shortcut);

  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Escape, 0,
                                       "editing.stop",
                                       "b", FALSE);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

/**
 * gtk_editable_label_new:
 * @str: the text for the label
 *
 * Creates a new #GtkEditableLabel widget.
 *
 * Returns: the new #GtkEditableLabel
 */
GtkWidget *
gtk_editable_label_new (const char *str)
{
  return g_object_new (GTK_TYPE_EDITABLE_LABEL,
                       "text", str,
                       NULL);
}
