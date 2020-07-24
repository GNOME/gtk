/*
 * Copyright (c) 2014 Red Hat, Inc.
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
#include <glib/gi18n-lib.h>

#include "action-editor.h"

#include "gtksizegroup.h"
#include "gtktogglebutton.h"
#include "gtkentry.h"
#include "gtklabel.h"
#include "gtkbox.h"
#include "gtkboxlayout.h"
#include "gtkorientable.h"
#include "gtkactionmuxerprivate.h"

struct _GtkInspectorActionEditor
{
  GtkWidget parent;

  GObject *owner;
  char *name;
  gboolean enabled;
  const GVariantType *parameter_type;
  GVariantType *state_type;
  GtkWidget *activate_button;
  GtkWidget *parameter_entry;
  GtkWidget *state_entry;
  GtkSizeGroup *sg;
};

typedef struct
{
  GtkWidgetClass parent;
} GtkInspectorActionEditorClass;

enum
{
  PROP_0,
  PROP_OWNER,
  PROP_NAME,
  PROP_SIZEGROUP
};

G_DEFINE_TYPE (GtkInspectorActionEditor, gtk_inspector_action_editor, GTK_TYPE_WIDGET)

static void
gtk_inspector_action_editor_init (GtkInspectorActionEditor *editor)
{
  GtkBoxLayout *layout;

  layout = GTK_BOX_LAYOUT (gtk_widget_get_layout_manager (GTK_WIDGET (editor)));
  gtk_orientable_set_orientation (GTK_ORIENTABLE (layout), GTK_ORIENTATION_HORIZONTAL);
  gtk_box_layout_set_spacing (layout, 10);
}

typedef void (*VariantEditorChanged) (GtkWidget *editor, gpointer data);

typedef struct
{
  GtkWidget *editor;
  VariantEditorChanged callback;
  gpointer   data;
} VariantEditorData;

static void
variant_editor_changed_cb (GObject           *obj,
                           GParamSpec        *pspec,
                           VariantEditorData *data)
{
  data->callback (data->editor, data->data);
}

static GtkWidget *
variant_editor_new (const GVariantType   *type,
                    VariantEditorChanged  callback,
                    gpointer              data)
{
  GtkWidget *editor;
  GtkWidget *label;
  GtkWidget *entry;
  VariantEditorData *d;

  d = g_new (VariantEditorData, 1);
  d->callback = callback;
  d->data = data;

  if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    {
      editor = gtk_toggle_button_new_with_label ("FALSE");
      g_signal_connect (editor, "notify::active", G_CALLBACK (variant_editor_changed_cb), d);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
    {
      editor = gtk_entry_new ();
      gtk_editable_set_width_chars (GTK_EDITABLE (editor), 10);
      g_signal_connect (editor, "notify::text", G_CALLBACK (variant_editor_changed_cb), d);
    }
  else
    {
      editor = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      entry = gtk_entry_new ();
      gtk_editable_set_width_chars (GTK_EDITABLE (entry), 10);
      gtk_box_append (GTK_BOX (editor), entry);
      label = gtk_label_new (g_variant_type_peek_string (type));
      gtk_box_append (GTK_BOX (editor), label);
      g_signal_connect (entry, "notify::text", G_CALLBACK (variant_editor_changed_cb), d);
    }

  g_object_set_data (G_OBJECT (editor), "type", (gpointer)type);
  d->editor = editor;
  g_object_set_data_full (G_OBJECT (editor), "callback", d, g_free);

  return editor;
}

static void
variant_editor_set_value (GtkWidget *editor,
                          GVariant  *value)
{
  const GVariantType *type;
  gpointer data;

  data = g_object_get_data (G_OBJECT (editor), "callback");
  g_signal_handlers_block_by_func (editor, variant_editor_changed_cb, data);

  type = g_variant_get_type (value);
  if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    {
      GtkToggleButton *tb = GTK_TOGGLE_BUTTON (editor);
      GtkWidget *child;

      gtk_toggle_button_set_active (tb, g_variant_get_boolean (value));
      child = gtk_button_get_child (GTK_BUTTON (tb));
      gtk_label_set_text (GTK_LABEL (child),
                          g_variant_get_boolean (value) ? "TRUE" : "FALSE");
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
    {
      GtkEntry *entry = GTK_ENTRY (editor);
      gtk_editable_set_text (GTK_EDITABLE (entry), g_variant_get_string (value, NULL));
    }
  else
    {
      GtkWidget *entry;
      char *text;

      entry = gtk_widget_get_first_child (editor);

      text = g_variant_print (value, FALSE);
      gtk_editable_set_text (GTK_EDITABLE (entry), text);
      g_free (text);
    }

  g_signal_handlers_unblock_by_func (editor, variant_editor_changed_cb, data);
}

static GVariant *
variant_editor_get_value (GtkWidget *editor)
{
  const GVariantType *type;
  GVariant *value;

  type = (const GVariantType *) g_object_get_data (G_OBJECT (editor), "type");
  if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    {
      GtkToggleButton *tb = GTK_TOGGLE_BUTTON (editor);
      value = g_variant_new_boolean (gtk_toggle_button_get_active (tb));
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
    {
      GtkEntry *entry = GTK_ENTRY (editor);
      value = g_variant_new_string (gtk_editable_get_text (GTK_EDITABLE (entry)));
    }
  else
    {
      GtkWidget *entry;
      const char *text;

      entry = gtk_widget_get_first_child (editor);
      text = gtk_editable_get_text (GTK_EDITABLE (entry));

      value = g_variant_parse (type, text, NULL, NULL, NULL);
    }

  return value;
}

static void
activate_action (GtkWidget                *button,
                 GtkInspectorActionEditor *r)
{
  GVariant *parameter = NULL;

  if (r->parameter_entry)
    parameter = variant_editor_get_value (r->parameter_entry);
  if (G_IS_ACTION_GROUP (r->owner))
    g_action_group_activate_action (G_ACTION_GROUP (r->owner), r->name, parameter);
  else if (GTK_IS_ACTION_MUXER (r->owner))
    gtk_action_muxer_activate_action (GTK_ACTION_MUXER (r->owner), r->name, parameter);
}

static void
parameter_changed (GtkWidget *editor,
                   gpointer   data)
{
  GtkInspectorActionEditor *r = data;
  GVariant *value;

  value = variant_editor_get_value (editor);
  gtk_widget_set_sensitive (r->activate_button, r->enabled && value != NULL);
  if (value)
    g_variant_unref (value);
}

static void
state_changed (GtkWidget *editor,
               gpointer   data)
{
  GtkInspectorActionEditor *r = data;
  GVariant *value;

  value = variant_editor_get_value (editor);
  if (value)
    {
      if (G_IS_ACTION_GROUP (r->owner))
        g_action_group_change_action_state (G_ACTION_GROUP (r->owner), r->name, value);
      else if (GTK_IS_ACTION_MUXER (r->owner))
        gtk_action_muxer_change_action_state (GTK_ACTION_MUXER (r->owner), r->name, value);
    }
}

static void
update_enabled (GtkInspectorActionEditor *r,
                gboolean                  enabled)
{
  r->enabled = enabled;
  if (r->parameter_entry)
    {
      gtk_widget_set_sensitive (r->parameter_entry, enabled);
      parameter_changed (r->parameter_entry, r);
    }
  if (r->activate_button)
    gtk_widget_set_sensitive (r->activate_button, enabled);
}

static void
action_enabled_changed_cb (GActionGroup             *group,
                           const char               *action_name,
                           gboolean                  enabled,
                           GtkInspectorActionEditor *r)
{
  if (g_str_equal (action_name, r->name))
    update_enabled (r, enabled);
}

static void
update_state (GtkInspectorActionEditor *r,
              GVariant                 *state)
{
  if (r->state_entry)
    variant_editor_set_value (r->state_entry, state);
}

static void
action_state_changed_cb (GActionGroup             *group,
                         const char               *action_name,
                         GVariant                 *state,
                         GtkInspectorActionEditor *r)
{
  if (g_str_equal (action_name, r->name))
    update_state (r, state);
}

static void
constructed (GObject *object)
{
  GtkInspectorActionEditor *r = GTK_INSPECTOR_ACTION_EDITOR (object);
  GVariant *state;
  GtkWidget *row;
  GtkWidget *activate;
  GtkWidget *label;

  if (G_IS_ACTION_GROUP (r->owner))
    g_action_group_query_action (G_ACTION_GROUP (r->owner), r->name,
                                 &r->enabled, &r->parameter_type, NULL, NULL,
                                 &state);
  else if (GTK_IS_ACTION_MUXER (r->owner))
    gtk_action_muxer_query_action (GTK_ACTION_MUXER (r->owner), r->name,
                                   &r->enabled, &r->parameter_type, NULL, NULL,
                                   &state);
  else
    state = NULL;

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  activate = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_append (GTK_BOX (row), activate);
  if (r->sg)
    gtk_size_group_add_widget (r->sg, activate);

  r->activate_button = gtk_button_new_with_label (_("Activate"));
  g_signal_connect (r->activate_button, "clicked", G_CALLBACK (activate_action), r);

  gtk_widget_set_sensitive (r->activate_button, r->enabled);
  gtk_box_append (GTK_BOX (activate), r->activate_button);

  if (r->parameter_type)
    {
      r->parameter_entry = variant_editor_new (r->parameter_type, parameter_changed, r);
      gtk_widget_set_sensitive (r->parameter_entry, r->enabled);
      gtk_box_append (GTK_BOX (activate), r->parameter_entry);
    }

  gtk_widget_set_parent (row, GTK_WIDGET (r));

  if (state)
    {
      r->state_type = g_variant_type_copy (g_variant_get_type (state));
      row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      label = gtk_label_new (_("Set State"));
      if (r->sg)
        gtk_size_group_add_widget (r->sg, label);
      gtk_box_append (GTK_BOX (row), label);
      r->state_entry = variant_editor_new (r->state_type, state_changed, r);
      variant_editor_set_value (r->state_entry, state);
      gtk_box_append (GTK_BOX (row), r->state_entry);
      gtk_widget_set_parent (row, GTK_WIDGET (r));
    }

  if (G_IS_ACTION_GROUP (r->owner))
    {
      g_signal_connect (r->owner, "action-enabled-changed",
                        G_CALLBACK (action_enabled_changed_cb), r);
      g_signal_connect (r->owner, "action-state-changed",
                        G_CALLBACK (action_state_changed_cb), r);
    }
}

static void
dispose (GObject *object)
{
  GtkInspectorActionEditor *r = GTK_INSPECTOR_ACTION_EDITOR (object);
  GtkWidget *child;

  g_free (r->name);
  g_clear_object (&r->sg);
  if (r->state_type)
    g_variant_type_free (r->state_type);
  g_signal_handlers_disconnect_by_func (r->owner, action_enabled_changed_cb, r);
  g_signal_handlers_disconnect_by_func (r->owner, action_state_changed_cb, r);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (r))))
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (gtk_inspector_action_editor_parent_class)->dispose (object);
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GtkInspectorActionEditor *r = GTK_INSPECTOR_ACTION_EDITOR (object);

  switch (param_id)
    {
    case PROP_OWNER:
      g_value_set_object (value, r->owner);
      break;

    case PROP_NAME:
      g_value_set_string (value, r->name);
      break;

    case PROP_SIZEGROUP:
      g_value_set_object (value, r->sg);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
set_property (GObject      *object,
              guint         param_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GtkInspectorActionEditor *r = GTK_INSPECTOR_ACTION_EDITOR (object);

  switch (param_id)
    {
    case PROP_OWNER:
      r->owner = g_value_get_object (value);
      break;

    case PROP_NAME:
      g_free (r->name);
      r->name = g_value_dup_string (value);
      break;

    case PROP_SIZEGROUP:
      r->sg = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
      break;
    }
}

static void
gtk_inspector_action_editor_class_init (GtkInspectorActionEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = constructed;
  object_class->dispose = dispose;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  g_object_class_install_property (object_class, PROP_OWNER,
      g_param_spec_object ("owner", "Owner", "The owner of the action",
                           G_TYPE_OBJECT, G_PARAM_READWRITE|G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_NAME,
      g_param_spec_string ("name", "Name", "The action name",
                           NULL, G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class, PROP_SIZEGROUP,
      g_param_spec_object ("sizegroup", "Size Group", "The Size Group for activate", 
                           GTK_TYPE_SIZE_GROUP, G_PARAM_READWRITE|G_PARAM_CONSTRUCT));

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

GtkWidget *
gtk_inspector_action_editor_new (GObject      *owner,
                                 const char   *name,
                                 GtkSizeGroup *activate)
{
  return g_object_new (GTK_TYPE_INSPECTOR_ACTION_EDITOR,
                       "owner", owner,
                       "name", name,
                       "sizegroup", activate,
                       NULL);
}

void
gtk_inspector_action_editor_update (GtkInspectorActionEditor *r,
                                    gboolean                  enabled,
                                    GVariant                 *state)
{
  update_enabled (r, enabled);
  update_state (r, state);
}
