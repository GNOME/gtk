/*
 * Copyright © 2019 Red Hat, Inc.
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
 * Authors: Matthias Clasen
 */

#include "config.h"

#include "child-editor.h"

struct _ChildEditor
{
  GtkWidget parent_instance;

  GtkWidget *grid;
  GtkWidget *name;
  GtkWidget *min_width;
  GtkWidget *min_height;
  GtkWidget *button;

  GtkWidget *child;

  gboolean constructed;
};

enum {
  PROP_CHILD = 1,
  LAST_PROP
};

static GParamSpec *pspecs[LAST_PROP];

enum {
  DONE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE(ChildEditor, child_editor, GTK_TYPE_WIDGET);

void
child_editor_serialize_child (GString   *str,
                              int        indent,
                              GtkWidget *child)
{
  int min_width, min_height;
  const char *name;

  name = gtk_widget_get_name (child);
  if (!name)
    name = "";

  gtk_widget_get_size_request (child, &min_width, &min_height);

  g_string_append_printf (str, "%*s<child>\n", indent, "");
  g_string_append_printf (str, "%*s  <object class=\"GtkLabel\" id=\"%s\">\n", indent, "", name);
  g_string_append_printf (str, "%*s    <property name=\"label\">%s</property>\n", indent, "", name);
  if (min_width != -1)
    g_string_append_printf (str, "%*s    <property name=\"width-request\">%d</property>\n", indent, "", min_width);
  if (min_height != -1)
    g_string_append_printf (str, "%*s    <property name=\"height-request\">%d</property>\n", indent, "", min_height);
  g_string_append_printf (str, "%*s  </object>\n", indent, "");
  g_string_append_printf (str, "%*s</child>\n", indent, "");
}

static void
apply (GtkButton   *button,
       ChildEditor *editor)
{
  const char *name;
  int w, h;

  name = gtk_editable_get_text (GTK_EDITABLE (editor->name));
  w = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (editor->min_width));
  h = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (editor->min_height));

  gtk_widget_set_size_request (editor->child, w, h);
  gtk_widget_set_name (editor->child, name);

  g_signal_emit (editor, signals[DONE], 0, editor->child);
}

static void
child_editor_init (ChildEditor *editor)
{
  gtk_widget_init_template (GTK_WIDGET (editor));
}

static int
min_input (GtkSpinButton *spin_button,
           double        *new_val)
{
  if (strcmp (gtk_editable_get_text (GTK_EDITABLE (spin_button)), "") == 0)
    {
      *new_val = 0.0;
      return TRUE;
    }

  return FALSE;
}

static gboolean
min_output (GtkSpinButton *spin_button)
{
  GtkAdjustment *adjustment;
  double value;
  GtkWidget *box, *text;

  adjustment = gtk_spin_button_get_adjustment (spin_button);
  value = gtk_adjustment_get_value (adjustment);

  box = gtk_widget_get_first_child (GTK_WIDGET (spin_button));
  text = gtk_widget_get_first_child (box);

  if (value <= 0.0)
    {
      gtk_editable_set_text (GTK_EDITABLE (spin_button), "");
      gtk_text_set_placeholder_text (GTK_TEXT (text), "unset");
      return TRUE;
    }
  else
    {
      gtk_text_set_placeholder_text (GTK_TEXT (text), "");
      return FALSE;
    }
}

static void
child_editor_constructed (GObject *object)
{
  ChildEditor *editor = CHILD_EDITOR (object);
  const char *nick;
  int w, h;

  g_signal_connect (editor->min_width, "input", G_CALLBACK (min_input), NULL);
  g_signal_connect (editor->min_width, "output", G_CALLBACK (min_output), NULL);

  g_signal_connect (editor->min_height, "input", G_CALLBACK (min_input), NULL);
  g_signal_connect (editor->min_height, "output", G_CALLBACK (min_output), NULL);

  nick = gtk_widget_get_name (editor->child);
  if (nick)
    gtk_editable_set_text (GTK_EDITABLE (editor->name), nick);

  gtk_widget_get_size_request (editor->child, &w, &h);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->min_width), w);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->min_height), h);

  editor->constructed = TRUE;
}

static void
child_editor_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  ChildEditor *self = CHILD_EDITOR (object);

  switch (property_id)
    {
    case PROP_CHILD:
      self->child = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
child_editor_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  ChildEditor *self = CHILD_EDITOR (object);

  switch (property_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, self->child);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
child_editor_dispose (GObject *object)
{
  ChildEditor *self = (ChildEditor *)object;

  g_clear_pointer (&self->grid, gtk_widget_unparent);
  g_clear_object (&self->child);

  G_OBJECT_CLASS (child_editor_parent_class)->dispose (object);
}

static void
child_editor_class_init (ChildEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->constructed = child_editor_constructed;
  object_class->dispose = child_editor_dispose;
  object_class->set_property = child_editor_set_property;
  object_class->get_property = child_editor_get_property;

  pspecs[PROP_CHILD] =
    g_param_spec_object ("child", "child", "child",
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, LAST_PROP, pspecs);

  signals[DONE] =
    g_signal_new ("done",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, GTK_TYPE_WIDGET);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/gtk4/constraint-editor/child-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, ChildEditor, grid);
  gtk_widget_class_bind_template_child (widget_class, ChildEditor, name);
  gtk_widget_class_bind_template_child (widget_class, ChildEditor, min_width);
  gtk_widget_class_bind_template_child (widget_class, ChildEditor, min_height);
  gtk_widget_class_bind_template_child (widget_class, ChildEditor, button);

  gtk_widget_class_bind_template_callback (widget_class, apply);
}

ChildEditor *
child_editor_new (GtkWidget *child)
{
  return g_object_new (CHILD_EDITOR_TYPE,
                       "child", child,
                       NULL);
}
