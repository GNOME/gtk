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
#include "gtktext.h"
#include "gtkbinlayout.h"
#include "gtkgestureclick.h"
#include "gtkprivate.h"
#include "gtkshortcut.h"
#include "gtkshortcuttrigger.h"
#include "gtkwidgetprivate.h"
#include "gtkintl.h"

/**
 * SECTION:gtkeditablelabel
 * @Short_description: A label that can be edited
 * @Title: GtkEditableLabel
 * @See_also: #GtkEditable, #GtkLabel, #GtkEntry
 *
 * A GtkEditableLabel is a #GtkLabel that allows users to
 * edit the text by switching the widget to an “edit mode”.
 *
 * GtkEditableLabel does not have API of its own, but it
 * implements the #GtkEditable interface.
 *
 * The default bindings for activating the edit mode is
 * to click or press the Enter key. The default bindings
 * for leaving the edit mode are the Enter key (to save
 * the results) or the Escape key (to cancel the editing).
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

enum
{
  PROP_EDITING = 1,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

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
start_editing (GtkWidget  *widget,
               const char *action_name,
               GVariant   *parameters)
{
  gtk_editable_label_start_editing (GTK_EDITABLE_LABEL (widget));
}

static void
stop_editing (GtkWidget  *widget,
              const char *action_name,
              GVariant   *parameters)
{
  gtk_editable_label_stop_editing (GTK_EDITABLE_LABEL (widget),
                                   g_variant_get_boolean (parameters));
}

static void
clicked_cb (GtkWidget *self)
{
  gtk_widget_activate_action (self, "editing.start", NULL);
}

static void
activate_cb (GtkWidget *self)
{
  gtk_widget_activate_action (self, "editing.stop", "b", TRUE);
}

static void
text_changed (GtkEditableLabel *self)
{
  /* Sync the entry text to the label, unless we are editing.
   *
   * This is necessary to catch apis like gtk_editable_insert_text(),
   * which don't go through the ::text property.
   */
  if (!gtk_editable_label_get_editing (self))
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
  self->entry = gtk_text_new ();

  gtk_stack_add_named (GTK_STACK (self->stack), self->label, "label");
  gtk_stack_add_named (GTK_STACK (self->stack), self->entry, "entry");

  gtk_widget_set_parent (self->stack, GTK_WIDGET (self));

  gesture = gtk_gesture_click_new ();
  g_signal_connect_swapped (gesture, "pressed", G_CALLBACK (clicked_cb), self);
  gtk_widget_add_controller (self->label, GTK_EVENT_CONTROLLER (gesture));

  g_signal_connect_swapped (self->entry, "activate", G_CALLBACK (activate_cb), self);
  g_signal_connect_swapped (self->entry, "notify::text", G_CALLBACK (text_changed), self);

  gtk_editable_init_delegate (GTK_EDITABLE (self));
}

static gboolean
gtk_editable_label_grab_focus (GtkWidget *widget)
{
  GtkEditableLabel *self = GTK_EDITABLE_LABEL (widget);

  if (gtk_editable_label_get_editing (self))
    return gtk_widget_grab_focus (self->entry);
  else
    return gtk_widget_grab_focus_self (widget);
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
      switch (prop_id)
        {
        case NUM_PROPERTIES + GTK_EDITABLE_PROP_TEXT:
          gtk_label_set_label (GTK_LABEL (self->label), g_value_get_string (value));
          break;

        case NUM_PROPERTIES + GTK_EDITABLE_PROP_WIDTH_CHARS:
          gtk_label_set_width_chars (GTK_LABEL (self->label), g_value_get_int (value));
          break;

        case NUM_PROPERTIES + GTK_EDITABLE_PROP_MAX_WIDTH_CHARS:
          gtk_label_set_max_width_chars (GTK_LABEL (self->label), g_value_get_int (value));
          break;

        case NUM_PROPERTIES + GTK_EDITABLE_PROP_XALIGN:
          gtk_label_set_xalign (GTK_LABEL (self->label), g_value_get_float (value));
          break;

        case NUM_PROPERTIES + GTK_EDITABLE_PROP_EDITABLE:
          {
            gboolean editable;

            editable = g_value_get_boolean (value);
            if (!editable)
              gtk_editable_label_stop_editing (self, FALSE);

            gtk_widget_action_set_enabled (GTK_WIDGET (self), "editing.start", editable);
            gtk_widget_action_set_enabled (GTK_WIDGET (self), "editing.stop", editable);
          }
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
  GtkEditableLabel *self = GTK_EDITABLE_LABEL (object);

  if (gtk_editable_delegate_get_property (object, prop_id, value, pspec))
    return;

  switch (prop_id)
    {
    case PROP_EDITING:
      g_value_set_boolean (value, gtk_editable_label_get_editing (self));
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

  /**
   * GtkEditableLabel:editing:
   *
   * This property is %TRUE while the widget is in edit mode.
   */
  properties[PROP_EDITING] =
    g_param_spec_boolean ("editing",
                          P_("Editing"),
                          P_("Whether the widget is in editing mode"),
                          FALSE,
                          GTK_PARAM_READABLE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_editable_install_properties (object_class, NUM_PROPERTIES);

  /**
   * GtkEditableLabel|editing.start:
   *
   * Switch the widget into editing mode, so that the
   * user can make changes to the text.
   *
   * The default bindings for this action are clicking
   * on the widget and the Enter key.
   *
   * This action is disabled when #GtkEditable:editing
   * is %FALSE.
   */
  gtk_widget_class_install_action (widget_class, "editing.start", NULL, start_editing);

  /**
   * GtkEditableLabel|editing.stop:
   * @commit: Whether the make changes permanent
   *
   * Switch the widget out of editing mode. If @commit
   * is %TRUE, then the results of the editing are taken
   * as the new value of #GtkEditable:text.
   *
   * The default binding for this action is the Escape
   * key.
   *
   * This action is disabled when #GtkEditable:editing
   * is %FALSE.
   */
  gtk_widget_class_install_action (widget_class, "editing.stop", "b", stop_editing);

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
  gtk_widget_class_set_css_name (widget_class, "editablelabel");
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

/**
 * gtk_editable_label_get_editing:
 * @self: a #GtkEditableLabel
 *
 * Returns whether the label is currently in “editing mode”.
 *
 * Returns: %TRUE if @self is currently in editing mode
 */
gboolean
gtk_editable_label_get_editing (GtkEditableLabel *self)
{
  g_return_val_if_fail (GTK_IS_EDITABLE_LABEL (self), FALSE);

  return gtk_stack_get_visible_child (GTK_STACK (self->stack)) == self->entry;
}

/**
 * gtk_editable_label_start_editing:
 * @self: a #GtkEditableLabel
 *
 * Switches the label into “editing mode”.
 */
void
gtk_editable_label_start_editing (GtkEditableLabel *self)
{
  g_return_if_fail (GTK_IS_EDITABLE_LABEL (self));

  if (gtk_editable_label_get_editing (self))
    return;

  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "entry");
  gtk_widget_grab_focus (self->entry);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EDITING]);
}

/**
 * gtk_editable_label_stop_editing:
 * @self: a #GtkEditableLabel
 * @commit: whether to set the edited text on the label
 *
 * Switches the label out of “editing mode”. If @commit is %TRUE,
 * the resulting text is kept as the #GtkEditableLabel:text property
 * value, otherwise the resulting text is discarded and the label
 * will keep its previous #GtkEditableLabel:text property value.
 */
void
gtk_editable_label_stop_editing (GtkEditableLabel *self,
                                 gboolean          commit)
{
  g_return_if_fail (GTK_IS_EDITABLE_LABEL (self));

  if (!gtk_editable_label_get_editing (self))
    return;

  if (commit)
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

  gtk_widget_grab_focus (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EDITING]);
}
