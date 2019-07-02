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

#include "constraint-editor.h"

struct _ConstraintEditor
{
  GtkWidget parent_instance;

  GtkWidget *grid;
  GtkWidget *name;
  GtkWidget *target;
  GtkWidget *target_attr;
  GtkWidget *relation;
  GtkWidget *source;
  GtkWidget *source_attr;
  GtkWidget *multiplier;
  GtkWidget *constant;
  GtkWidget *strength;
  GtkWidget *preview;
  GtkWidget *button;

  GtkConstraint *constraint;
  GListModel *model;

  gboolean constructed;
};

enum {
  PROP_MODEL = 1,
  PROP_CONSTRAINT,
  LAST_PROP
};

static GParamSpec *pspecs[LAST_PROP];

enum {
  DONE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE(ConstraintEditor, constraint_editor, GTK_TYPE_WIDGET);

static const char *
get_target_name (GtkConstraintTarget *target)
{
  if (target == NULL)
    return "super";
  else
    return (const char *)g_object_get_data (G_OBJECT (target), "name");
}

static void
constraint_target_combo (GListModel *model,
                         GtkWidget  *combo,
                         gboolean    is_source)
{
  int i;

  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "super", "Super");

  if (model)
    {
      for (i = 0; i < g_list_model_get_n_items (model); i++)
        {
          GObject *item = g_list_model_get_object (model, i);
          const char *name;

          if (GTK_IS_CONSTRAINT (item))
            continue;

          name = get_target_name (GTK_CONSTRAINT_TARGET (item));

          gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), name, name);
          g_object_unref (item);
        }
    }
}

static void
constraint_attribute_combo (GtkWidget *combo,
                            gboolean is_source)
{
  if (is_source)
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "none", "None");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "left", "Left");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "right", "Right");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "top", "Top");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "bottom", "Bottom");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "start", "Start");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "end", "End");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "width", "Width");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "height", "Height");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "center-x", "Center X");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "center-y", "Center Y");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "baseline", "Baseline");
}

static void
constraint_relation_combo (GtkWidget *combo)
{
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "le", "≤");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "eq", "=");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "ge", "≥");
}

static void
constraint_strength_combo (GtkWidget *combo)
{
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "weak", "Weak");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "medium", "Medium");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "strong", "Strong");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "required", "Required");
}

static gpointer
get_target (GListModel *model,
            const char *id)
{
  int i;

  if (strcmp ("super", id) == 0)
    return NULL;

  for (i = 0; i < g_list_model_get_n_items (model); i++)
    {
      GObject *item = g_list_model_get_object (model, i);
      const char *name;
      if (GTK_IS_CONSTRAINT (item))
        continue;
      name = (const char *)g_object_get_data (item, "name");
      g_object_unref (item);
      if (strcmp (name, id) == 0)
        return item;
    }

  return NULL;
}

static GtkConstraintAttribute
get_target_attr (const char *id)
{
  GtkConstraintAttribute attr;
  GEnumClass *class = g_type_class_ref (GTK_TYPE_CONSTRAINT_ATTRIBUTE);
  GEnumValue *value = g_enum_get_value_by_nick (class, id);
  attr = value->value;
  g_type_class_unref (class);

  return attr;
}

static const char *
get_attr_nick (GtkConstraintAttribute attr)
{
  GEnumClass *class = g_type_class_ref (GTK_TYPE_CONSTRAINT_ATTRIBUTE);
  GEnumValue *value = g_enum_get_value (class, attr);
  const char *nick = value->value_nick;
  g_type_class_unref (class);

  return nick;
}

static GtkConstraintRelation
get_relation (const char *id)
{
  GtkConstraintRelation relation;
  GEnumClass *class = g_type_class_ref (GTK_TYPE_CONSTRAINT_RELATION);
  GEnumValue *value = g_enum_get_value_by_nick (class, id);
  relation = value->value;
  g_type_class_unref (class);

  return relation;
}

static const char *
get_relation_nick (GtkConstraintRelation relation)
{
  GEnumClass *class = g_type_class_ref (GTK_TYPE_CONSTRAINT_RELATION);
  GEnumValue *value = g_enum_get_value (class, relation);
  const char *nick = value->value_nick;
  g_type_class_unref (class);

  return nick;
}

static GtkConstraintStrength
get_strength (const char *id)
{
  GtkConstraintStrength strength;
  GEnumClass *class = g_type_class_ref (GTK_TYPE_CONSTRAINT_STRENGTH);
  GEnumValue *value = g_enum_get_value_by_nick (class, id);
  strength = value->value;
  g_type_class_unref (class);

  return strength;
}

static const char *
get_strength_nick (GtkConstraintStrength strength)
{
  GEnumClass *class = g_type_class_ref (GTK_TYPE_CONSTRAINT_STRENGTH);
  GEnumValue *value = g_enum_get_value (class, strength);
  const char *nick = value->value_nick;
  g_type_class_unref (class);

  return nick;
}

static void
create_constraint (GtkButton        *button,
                   ConstraintEditor *editor)
{
  const char *id;
  gpointer target;
  GtkConstraintAttribute target_attr;
  gpointer source;
  GtkConstraintAttribute source_attr;
  GtkConstraintRelation relation;
  double multiplier;
  double constant;
  int strength;
  GtkConstraint *constraint;
  const char *name;

  id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (editor->target));
  target = get_target (editor->model, id);
  id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (editor->target_attr));
  target_attr = get_target_attr (id);

  id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (editor->source));
  source = get_target (editor->model, id);
  id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (editor->source_attr));
  source_attr = get_target_attr (id);

  id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (editor->relation));
  relation = get_relation (id);

  multiplier = g_ascii_strtod (gtk_editable_get_text (GTK_EDITABLE (editor->multiplier)), NULL);

  constant = g_ascii_strtod (gtk_editable_get_text (GTK_EDITABLE (editor->constant)), NULL);

  id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (editor->strength));
  strength = get_strength (id);

  name = gtk_editable_get_text (GTK_EDITABLE (editor->name));

  constraint = gtk_constraint_new (target, target_attr,
                                   relation,
                                   source, source_attr,
                                   multiplier,
                                   constant,
                                   strength);
  g_object_set_data_full (G_OBJECT (constraint), "name", g_strdup (name), g_free);
  g_signal_emit (editor, signals[DONE], 0, constraint);
  g_object_unref (constraint);
}

static void
source_attr_changed (ConstraintEditor *editor)
{
  const char *id;

  id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (editor->source_attr));
  if (strcmp (id, "none") == 0)
    {
      gtk_combo_box_set_active (GTK_COMBO_BOX (editor->source), -1);
      gtk_editable_set_text (GTK_EDITABLE (editor->multiplier), "");
      gtk_widget_set_sensitive (editor->source, FALSE);
      gtk_widget_set_sensitive (editor->multiplier, FALSE);
    }
  else
    {
      gtk_widget_set_sensitive (editor->source, TRUE);
      gtk_widget_set_sensitive (editor->multiplier, TRUE);
    }
}

static void
update_preview (ConstraintEditor *editor)
{
  GString *str;
  const char *name;
  const char *attr;
  char *relation;
  const char *multiplier;
  const char *constant;
  double c, m;

  if (!editor->constructed)
    return;

  str = g_string_new ("");

  name = gtk_combo_box_get_active_id (GTK_COMBO_BOX (editor->target));
  attr = gtk_combo_box_get_active_id (GTK_COMBO_BOX (editor->target_attr));
  relation = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (editor->relation));

  if (name == NULL)
    name = "[ ]";

  g_string_append_printf (str, "%s.%s %s ", name, attr, relation);
  g_free (relation);

  constant = gtk_editable_get_text (GTK_EDITABLE (editor->constant));
  c = g_ascii_strtod (constant, NULL);

  attr = gtk_combo_box_get_active_id (GTK_COMBO_BOX (editor->source_attr));
  if (strcmp (attr, "none") != 0)
    {
      name = gtk_combo_box_get_active_id (GTK_COMBO_BOX (editor->source));
      multiplier = gtk_editable_get_text (GTK_EDITABLE (editor->multiplier));
      m = g_ascii_strtod (multiplier, NULL);

      if (name == NULL)
        name = "[ ]";

      g_string_append_printf (str, "%s.%s", name, attr);

      if (m != 1.0)
        g_string_append_printf (str, " × %g", m);

      if (c > 0.0)
        g_string_append_printf (str, " + %g", c);
      else if (c < 0.0)
        g_string_append_printf (str, " - %g", -c);
    }
  else
    g_string_append_printf (str, "%g", c);

  gtk_label_set_label (GTK_LABEL (editor->preview), str->str);

  g_string_free (str, TRUE);
}

static void
update_button (ConstraintEditor *editor)
{
  if (gtk_combo_box_get_active_id (GTK_COMBO_BOX (editor->target)) != NULL &&
      gtk_combo_box_get_active_id (GTK_COMBO_BOX (editor->source)) != NULL)
    gtk_widget_set_sensitive (editor->button, TRUE);
  else
    gtk_widget_set_sensitive (editor->button, FALSE);
}

static void
constraint_editor_init (ConstraintEditor *editor)
{
  gtk_widget_init_template (GTK_WIDGET (editor));
}

static int constraint_counter;

static void
constraint_editor_constructed (GObject *object)
{
  ConstraintEditor *editor = CONSTRAINT_EDITOR (object);

  constraint_target_combo (editor->model, editor->target, FALSE);
  constraint_attribute_combo (editor->target_attr, FALSE);
  constraint_relation_combo (editor->relation);
  constraint_target_combo (editor->model, editor->source, TRUE);
  constraint_attribute_combo (editor->source_attr, TRUE);

  constraint_strength_combo (editor->strength);

  if (editor->constraint)
    {
      GtkConstraintTarget *target;
      GtkConstraintAttribute attr;
      GtkConstraintRelation relation;
      GtkConstraintStrength strength;
      const char *nick;
      char *val;
      double multiplier;
      double constant;

      nick = (char *)g_object_get_data (G_OBJECT (editor->constraint), "name");
      gtk_editable_set_text (GTK_EDITABLE (editor->name), nick);

      target = gtk_constraint_get_target (editor->constraint);
      nick = get_target_name (target);
      gtk_combo_box_set_active_id (GTK_COMBO_BOX (editor->target), nick);

      attr = gtk_constraint_get_target_attribute (editor->constraint);
      nick = get_attr_nick (attr);
      gtk_combo_box_set_active_id (GTK_COMBO_BOX (editor->target_attr), nick);

      target = gtk_constraint_get_source (editor->constraint);
      nick = get_target_name (target);
      gtk_combo_box_set_active_id (GTK_COMBO_BOX (editor->source), nick);

      attr = gtk_constraint_get_source_attribute (editor->constraint);
      nick = get_attr_nick (attr);
      gtk_combo_box_set_active_id (GTK_COMBO_BOX (editor->source_attr), nick);

      relation = gtk_constraint_get_relation (editor->constraint);
      nick = get_relation_nick (relation);
      gtk_combo_box_set_active_id (GTK_COMBO_BOX (editor->relation), nick);

      multiplier = gtk_constraint_get_multiplier (editor->constraint);
      val = g_strdup_printf ("%g", multiplier);
      gtk_editable_set_text (GTK_EDITABLE (editor->multiplier), val);
      g_free (val);

      constant = gtk_constraint_get_constant (editor->constraint);
      val = g_strdup_printf ("%g", constant);
      gtk_editable_set_text (GTK_EDITABLE (editor->constant), val);
      g_free (val);

      strength = gtk_constraint_get_strength (editor->constraint);
      nick = get_strength_nick (strength);
      gtk_combo_box_set_active_id (GTK_COMBO_BOX (editor->strength), nick);

      gtk_button_set_label (GTK_BUTTON (editor->button), "Apply");
    }
  else
    {
      char *name;

      constraint_counter++;
      name = g_strdup_printf ("Constraint %d", constraint_counter);
      gtk_editable_set_text (GTK_EDITABLE (editor->name), name);
      g_free (name);

      gtk_combo_box_set_active_id (GTK_COMBO_BOX (editor->target_attr), "left");
      gtk_combo_box_set_active_id (GTK_COMBO_BOX (editor->source_attr), "left");
      gtk_combo_box_set_active_id (GTK_COMBO_BOX (editor->relation), "eq");
      gtk_combo_box_set_active_id (GTK_COMBO_BOX (editor->strength), "required");

      gtk_editable_set_text (GTK_EDITABLE (editor->multiplier), "1.0");
      gtk_editable_set_text (GTK_EDITABLE (editor->constant), "0.0");

      gtk_button_set_label (GTK_BUTTON (editor->button), "Create");
    }

  editor->constructed = TRUE;
  update_preview (editor);
  update_button (editor);
}

static void
constraint_editor_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ConstraintEditor *self = CONSTRAINT_EDITOR (object);

  switch (property_id)
    {
    case PROP_MODEL:
      self->model = g_value_dup_object (value);
      break;

    case PROP_CONSTRAINT:
      self->constraint = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
constraint_editor_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ConstraintEditor *self = CONSTRAINT_EDITOR (object);

  switch (property_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    case PROP_CONSTRAINT:
      g_value_set_object (value, self->constraint);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
constraint_editor_dispose (GObject *object)
{
  ConstraintEditor *self = (ConstraintEditor *)object;

  g_clear_pointer (&self->grid, gtk_widget_unparent);
  g_clear_object (&self->model);
  g_clear_object (&self->constraint);

  G_OBJECT_CLASS (constraint_editor_parent_class)->dispose (object);
}

static void
constraint_editor_class_init (ConstraintEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->constructed = constraint_editor_constructed;
  object_class->dispose = constraint_editor_dispose;
  object_class->set_property = constraint_editor_set_property;
  object_class->get_property = constraint_editor_get_property;

  pspecs[PROP_CONSTRAINT] =
    g_param_spec_object ("constraint", "constraint", "constraint",
                         GTK_TYPE_CONSTRAINT,
                         G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);

  pspecs[PROP_MODEL] =
    g_param_spec_object ("model", "model", "model",
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, LAST_PROP, pspecs);

  signals[DONE] =
    g_signal_new ("done",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, GTK_TYPE_CONSTRAINT);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/gtk4/constraint-editor/constraint-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, ConstraintEditor, grid);
  gtk_widget_class_bind_template_child (widget_class, ConstraintEditor, name);
  gtk_widget_class_bind_template_child (widget_class, ConstraintEditor, target);
  gtk_widget_class_bind_template_child (widget_class, ConstraintEditor, target_attr);
  gtk_widget_class_bind_template_child (widget_class, ConstraintEditor, relation);
  gtk_widget_class_bind_template_child (widget_class, ConstraintEditor, source);
  gtk_widget_class_bind_template_child (widget_class, ConstraintEditor, source_attr);
  gtk_widget_class_bind_template_child (widget_class, ConstraintEditor, multiplier);
  gtk_widget_class_bind_template_child (widget_class, ConstraintEditor, constant);
  gtk_widget_class_bind_template_child (widget_class, ConstraintEditor, strength);
  gtk_widget_class_bind_template_child (widget_class, ConstraintEditor, preview);
  gtk_widget_class_bind_template_child (widget_class, ConstraintEditor, button);

  gtk_widget_class_bind_template_callback (widget_class, update_preview);
  gtk_widget_class_bind_template_callback (widget_class, update_button);
  gtk_widget_class_bind_template_callback (widget_class, create_constraint);
  gtk_widget_class_bind_template_callback (widget_class, source_attr_changed);
}

ConstraintEditor *
constraint_editor_new (GListModel    *model,
                       GtkConstraint *constraint)
{
  return g_object_new (CONSTRAINT_EDITOR_TYPE,
                       "model", model,
                       "constraint", constraint,
                       NULL);
}
