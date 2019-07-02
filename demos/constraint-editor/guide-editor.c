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

#include "guide-editor.h"

struct _GuideEditor
{
  GtkWidget parent_instance;

  GtkWidget *grid;
  GtkWidget *name;
  GtkWidget *min_width;
  GtkWidget *min_height;
  GtkWidget *nat_width;
  GtkWidget *nat_height;
  GtkWidget *max_width;
  GtkWidget *max_height;
  GtkWidget *strength;
  GtkWidget *button;

  GtkConstraintGuide *guide;

  gboolean constructed;
};

enum {
  PROP_GUIDE = 1,
  LAST_PROP
};

static GParamSpec *pspecs[LAST_PROP];

enum {
  DONE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE(GuideEditor, guide_editor, GTK_TYPE_WIDGET);

static void
guide_strength_combo (GtkWidget *combo)
{
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "weak", "Weak");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "medium", "Medium");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "strong", "Strong");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "required", "Required");
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

const char *
get_strength_nick (GtkConstraintStrength strength)
{
  GEnumClass *class = g_type_class_ref (GTK_TYPE_CONSTRAINT_STRENGTH);
  GEnumValue *value = g_enum_get_value (class, strength);
  const char *nick = value->value_nick;
  g_type_class_unref (class);

  return nick;
}

static void
create_guide (GtkButton   *button,
              GuideEditor *editor)
{
  const char *id;
  int strength;
  const char *name;
  int w, h;
  GtkConstraintGuide *guide;

  if (editor->guide)
    guide = g_object_ref (editor->guide);
  else
    guide = gtk_constraint_guide_new ();

  name = gtk_editable_get_text (GTK_EDITABLE (editor->name));
  gtk_constraint_guide_set_name (guide, name);

  w = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (editor->min_width));
  h = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (editor->min_height));
  gtk_constraint_guide_set_min_size (guide, w, h);

  w = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (editor->nat_width));
  h = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (editor->nat_height));
  gtk_constraint_guide_set_nat_size (guide, w, h);

  w = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (editor->max_width));
  h = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (editor->max_height));
  gtk_constraint_guide_set_max_size (guide, w, h);

  id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (editor->strength));
  strength = get_strength (id);
  gtk_constraint_guide_set_strength (guide, strength);

  g_signal_emit (editor, signals[DONE], 0, guide);
  g_object_unref (guide);
}

static void
guide_editor_init (GuideEditor *editor)
{
  gtk_widget_init_template (GTK_WIDGET (editor));
}

static int guide_counter;

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

static int
max_input (GtkSpinButton *spin_button,
           double        *new_val)
{
  if (strcmp (gtk_editable_get_text (GTK_EDITABLE (spin_button)), "") == 0)
    {
      *new_val = G_MAXINT;
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

  if (value == 0.0)
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

static gboolean
max_output (GtkSpinButton *spin_button)
{
  GtkAdjustment *adjustment;
  double value;
  GtkWidget *box, *text;

  adjustment = gtk_spin_button_get_adjustment (spin_button);
  value = gtk_adjustment_get_value (adjustment);

  box = gtk_widget_get_first_child (GTK_WIDGET (spin_button));
  text = gtk_widget_get_first_child (box);

  if (value == (double)G_MAXINT)
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
guide_editor_constructed (GObject *object)
{
  GuideEditor *editor = GUIDE_EDITOR (object);

  guide_strength_combo (editor->strength);

  g_signal_connect (editor->min_width, "input", G_CALLBACK (min_input), NULL);
  g_signal_connect (editor->min_width, "output", G_CALLBACK (min_output), NULL);

  g_signal_connect (editor->min_height, "input", G_CALLBACK (min_input), NULL);
  g_signal_connect (editor->min_height, "output", G_CALLBACK (min_output), NULL);

  g_signal_connect (editor->max_width, "input", G_CALLBACK (max_input), NULL);
  g_signal_connect (editor->max_width, "output", G_CALLBACK (max_output), NULL);

  g_signal_connect (editor->max_height, "input", G_CALLBACK (max_input), NULL);
  g_signal_connect (editor->max_height, "output", G_CALLBACK (max_output), NULL);

  if (editor->guide)
    {
      GtkConstraintStrength strength;
      const char *nick;
      int w, h;

      nick = gtk_constraint_guide_get_name (editor->guide);
      if (nick)
        gtk_editable_set_text (GTK_EDITABLE (editor->name), nick);

      gtk_constraint_guide_get_min_size (editor->guide, &w, &h);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->min_width), w);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->min_height), h);

      gtk_constraint_guide_get_nat_size (editor->guide, &w, &h);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->nat_width), w);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->nat_height), h);

      gtk_constraint_guide_get_max_size (editor->guide, &w, &h);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->max_width), w);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->max_height), h);

      strength = gtk_constraint_guide_get_strength (editor->guide);
      nick = get_strength_nick (strength);
      gtk_combo_box_set_active_id (GTK_COMBO_BOX (editor->strength), nick);

      gtk_button_set_label (GTK_BUTTON (editor->button), "Apply");
    }
  else
    {
      char *name;

      guide_counter++;
      name = g_strdup_printf ("Guide %d", guide_counter);
      gtk_editable_set_text (GTK_EDITABLE (editor->name), name);
      g_free (name);

      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->min_width), 0.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->min_height), 0.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->nat_width), 0.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->nat_height), 0.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->max_width), G_MAXINT);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->max_height), G_MAXINT);

      gtk_combo_box_set_active_id (GTK_COMBO_BOX (editor->strength), "medium");

      gtk_button_set_label (GTK_BUTTON (editor->button), "Create");
    }

  editor->constructed = TRUE;
}

static void
guide_editor_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GuideEditor *self = GUIDE_EDITOR (object);

  switch (property_id)
    {
    case PROP_GUIDE:
      self->guide = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
guide_editor_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GuideEditor *self = GUIDE_EDITOR (object);

  switch (property_id)
    {
    case PROP_GUIDE:
      g_value_set_object (value, self->guide);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
guide_editor_dispose (GObject *object)
{
  GuideEditor *self = (GuideEditor *)object;

  g_clear_pointer (&self->grid, gtk_widget_unparent);
  g_clear_object (&self->guide);

  G_OBJECT_CLASS (guide_editor_parent_class)->dispose (object);
}

static void
guide_editor_class_init (GuideEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->constructed = guide_editor_constructed;
  object_class->dispose = guide_editor_dispose;
  object_class->set_property = guide_editor_set_property;
  object_class->get_property = guide_editor_get_property;

  pspecs[PROP_GUIDE] =
    g_param_spec_object ("guide", "guide", "guide",
                         GTK_TYPE_CONSTRAINT_GUIDE,
                         G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, LAST_PROP, pspecs);

  signals[DONE] =
    g_signal_new ("done",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, GTK_TYPE_CONSTRAINT_GUIDE);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/gtk4/constraint-editor/guide-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, GuideEditor, grid);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, name);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, min_width);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, min_height);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, nat_width);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, nat_height);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, max_width);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, max_height);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, strength);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, button);

  gtk_widget_class_bind_template_callback (widget_class, create_guide);
}

GuideEditor *
guide_editor_new (GtkConstraintGuide *guide)
{
  return g_object_new (GUIDE_EDITOR_TYPE,
                       "guide", guide,
                       NULL);
}
