/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2015 Red Hat, Inc.
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

#include "gtkmultichoiceprivate.h"
#include "gtkbox.h"
#include "gtklabel.h"
#include "gtkstack.h"
#include "gtkwidgetprivate.h"
#include "gtkmarshalers.h"

#include "gtkintl.h"
#include "gtkprivate.h"

struct _GtkMultiChoice
{
  GtkBox parent;
  GtkWidget *down_button;
  GtkWidget *stack;
  GtkWidget *up_button;
  gint value;
  gint min_value;
  gint max_value;
  gboolean wrap;
  gboolean animate;
  GtkWidget **choices;
  gint n_choices;
  guint click_id;
  GtkWidget *active;
  GtkWidget *label1;
  GtkWidget *label2;
  GtkMultiChoiceFormatCallback format_cb;
  gpointer                     format_data;
  GDestroyNotify               format_destroy;
};

struct _GtkMultiChoiceClass
{
  GtkBoxClass parent_class;
};

enum {
  PROP_VALUE = 1,
  PROP_MIN_VALUE,
  PROP_MAX_VALUE,
  PROP_WRAP,
  PROP_ANIMATE,
  PROP_CHOICES,
  NUM_PROPERTIES
};

static GParamSpec *choice_properties[NUM_PROPERTIES] = { NULL, };

enum {
  WRAPPED,
  LAST_SIGNAL
};

static guint choice_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GtkMultiChoice, gtk_multi_choice, GTK_TYPE_BOX)

static gchar *
get_value_string (GtkMultiChoice *choice,
                  gint            value)
{
  if (choice->format_cb)
    return choice->format_cb (choice, value, choice->format_data);
  else if (0 <= value && value < choice->n_choices)
    return g_strdup (gtk_label_get_label (GTK_LABEL (choice->choices[value])));
  else
    return g_strdup_printf ("%d", value);
}

static void
set_value (GtkMultiChoice         *choice,
           gint                    value,
           GtkStackTransitionType  transition)
{
  gchar *text;
  const gchar *name;
  GtkWidget *label;

  value = CLAMP (value, choice->min_value, choice->max_value);

  if (choice->value == value)
    return;

  choice->value = value;

  if (gtk_stack_get_visible_child (GTK_STACK (choice->stack)) == choice->label1)
    {
      name = "label2";
      label = choice->label2;
    }
  else
    {
      name = "label1";
      label = choice->label1;
    }

  text = get_value_string (choice, value);
  gtk_label_set_text (GTK_LABEL (label), text);
  g_free (text);

  if (!choice->animate)
    transition = GTK_STACK_TRANSITION_TYPE_NONE;

  gtk_stack_set_visible_child_full (GTK_STACK (choice->stack), name, transition);

  gtk_widget_set_sensitive (choice->down_button,
                            choice->wrap || choice->value > choice->min_value);
  gtk_widget_set_sensitive (choice->up_button,
                            choice->wrap || choice->value < choice->max_value);

  g_object_notify_by_pspec (G_OBJECT (choice), choice_properties[PROP_VALUE]);
}

static void
go_up (GtkMultiChoice *choice)
{
  gint value;
  gboolean wrapped = FALSE;

  value = choice->value + 1;
  if (value > choice->max_value)
    {
      if (!choice->wrap)
        return;

      value = choice->min_value;
      wrapped = TRUE;
    }

  set_value (choice, value, GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);

  if (wrapped)
    g_signal_emit (choice, choice_signals[WRAPPED], 0);
}

static void
go_down (GtkMultiChoice *choice)
{
  gint value;
  gboolean wrapped = FALSE;

  value = choice->value - 1;
  if (value < choice->min_value)
    {
      if (!choice->wrap)
        return;

      value = choice->max_value;
      wrapped = TRUE;
    }

  set_value (choice, value, GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);

  if (wrapped)
    g_signal_emit (choice, choice_signals[WRAPPED], 0);
}

static gboolean
button_activate (GtkMultiChoice *choice,
                 GtkWidget      *button)
{
  if (button == choice->down_button)
    go_down (choice);
  else if (button == choice->up_button)
    go_up (choice);
  else
    g_assert_not_reached ();

  return TRUE;
}

static gboolean
button_timeout (gpointer user_data)
{
  GtkMultiChoice *choice = GTK_MULTI_CHOICE (user_data);
  gboolean res;

  if (choice->click_id == 0)
    return G_SOURCE_REMOVE;

  res = button_activate (choice, choice->active);
  if (!res)
    {
      g_source_remove (choice->click_id);
      choice->click_id = 0;
    }

  return res;
}

static gboolean
button_press_cb (GtkWidget      *widget,
                 GdkEventButton *button,
                 GtkMultiChoice *choice)
{
  gint double_click_time;

  if (button->type != GDK_BUTTON_PRESS)
    return TRUE;

  g_object_get (gtk_widget_get_settings (widget),
                "gtk-double-click-time", &double_click_time,
                NULL);

  if (choice->click_id != 0)
    g_source_remove (choice->click_id);

  choice->active = widget;

  choice->click_id = gdk_threads_add_timeout (double_click_time,
                                              button_timeout,
                                              choice);
  g_source_set_name_by_id (choice->click_id, "[gtk+] button_timeout");
  button_timeout (choice);

  return TRUE;
}

static gboolean
button_release_cb (GtkWidget      *widget,
                   GdkEventButton *event,
                   GtkMultiChoice *choice)
{
  if (choice->click_id != 0)
    {
      g_source_remove (choice->click_id);
      choice->click_id = 0;
    }

  choice->active = NULL;

  return TRUE;
}

static void
button_clicked_cb (GtkWidget      *button,
                   GtkMultiChoice *choice)
{
  if (choice->click_id != 0)
    return;

  button_activate (choice, button);
}

static void
gtk_multi_choice_init (GtkMultiChoice *choice)
{
  gtk_widget_init_template (GTK_WIDGET (choice));
}

static void
gtk_multi_choice_dispose (GObject *object)
{
  GtkMultiChoice *choice = GTK_MULTI_CHOICE (object);

  if (choice->click_id != 0)
    {
      g_source_remove (choice->click_id);
      choice->click_id = 0;
    }

  g_free (choice->choices);
  choice->choices = NULL;

  if (choice->format_destroy)
    {
      choice->format_destroy (choice->format_data);
      choice->format_destroy = NULL;
    }

  G_OBJECT_CLASS (gtk_multi_choice_parent_class)->dispose (object);
}

static void
gtk_multi_choice_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkMultiChoice *choice = GTK_MULTI_CHOICE (object);

  switch (property_id)
    {
    case PROP_VALUE:
      g_value_set_int (value, choice->value);
      break;
    case PROP_MIN_VALUE:
      g_value_set_int (value, choice->min_value);
      break;
    case PROP_MAX_VALUE:
      g_value_set_int (value, choice->max_value);
      break;
    case PROP_WRAP:
      g_value_set_boolean (value, choice->wrap);
      break;
    case PROP_ANIMATE:
      g_value_set_boolean (value, choice->animate);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_multi_choice_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkMultiChoice *choice = GTK_MULTI_CHOICE (object);

  switch (property_id)
    {
    case PROP_VALUE:
      gtk_multi_choice_set_value (choice, g_value_get_int (value));
      break;
    case PROP_MIN_VALUE:
      choice->min_value = g_value_get_int (value);
      g_object_notify_by_pspec (object, choice_properties[PROP_MIN_VALUE]);
      gtk_multi_choice_set_value (choice, choice->value);
      break;
    case PROP_MAX_VALUE:
      choice->max_value = g_value_get_int (value);
      g_object_notify_by_pspec (object, choice_properties[PROP_MAX_VALUE]);
      gtk_multi_choice_set_value (choice, choice->value);
      break;
    case PROP_WRAP:
      choice->wrap = g_value_get_boolean (value);
      g_object_notify_by_pspec (object, choice_properties[PROP_WRAP]);
      break;
    case PROP_ANIMATE:
      choice->animate = g_value_get_boolean (value);
      g_object_notify_by_pspec (object, choice_properties[PROP_ANIMATE]);
      break;
    case PROP_CHOICES:
      gtk_multi_choice_set_choices (choice, (const gchar **)g_value_get_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_multi_choice_class_init (GtkMultiChoiceClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = gtk_multi_choice_dispose;
  object_class->set_property = gtk_multi_choice_set_property;
  object_class->get_property = gtk_multi_choice_get_property;

  choice_properties[PROP_VALUE] =
      g_param_spec_int ("value", P_("Value"), P_("Value"),
                        G_MININT, G_MAXINT, 0,
                        G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  choice_properties[PROP_MIN_VALUE] =
      g_param_spec_int ("min-value", P_("Minimum Value"), P_("Minimum Value"),
                        G_MININT, G_MAXINT, 0,
                        G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  choice_properties[PROP_MAX_VALUE] =
      g_param_spec_int ("max-value", P_("Maximum Value"), P_("Maximum Value"),
                        G_MININT, G_MAXINT, 0,
                        G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  choice_properties[PROP_WRAP] =
      g_param_spec_boolean ("wrap", P_("Wrap"), P_("Wrap"),
                            FALSE,
                            G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  choice_properties[PROP_ANIMATE] =
      g_param_spec_boolean ("animate", P_("Animate"), P_("Animate"),
                            FALSE,
                            G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  choice_properties[PROP_CHOICES] =
      g_param_spec_boxed ("choices", P_("Choices"), P_("Choices"),
                          G_TYPE_STRV,
                          G_PARAM_WRITABLE|G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_properties (object_class, NUM_PROPERTIES, choice_properties);

  choice_signals[WRAPPED] =
    g_signal_new (I_("wrapped"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkmultichoice.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkMultiChoice, down_button);
  gtk_widget_class_bind_template_child (widget_class, GtkMultiChoice, up_button);
  gtk_widget_class_bind_template_child (widget_class, GtkMultiChoice, stack);
  gtk_widget_class_bind_template_child (widget_class, GtkMultiChoice, label1);
  gtk_widget_class_bind_template_child (widget_class, GtkMultiChoice, label2);

  gtk_widget_class_bind_template_callback (widget_class, button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, button_press_cb);
  gtk_widget_class_bind_template_callback (widget_class, button_release_cb);
}

GtkWidget *
gtk_multi_choice_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_MULTI_CHOICE, NULL));
}

void
gtk_multi_choice_set_value (GtkMultiChoice *choice,
                            gint            value)
{
  set_value (choice, value, GTK_STACK_TRANSITION_TYPE_NONE);
}

gint
gtk_multi_choice_get_value (GtkMultiChoice *choice)
{
  return choice->value;
}

void
gtk_multi_choice_set_choices (GtkMultiChoice  *choice,
                              const gchar    **choices)
{
  gint i;

  for (i = 0; i < choice->n_choices; i++)
    gtk_container_remove (GTK_CONTAINER (choice->stack), choice->choices[i]);
  g_free (choice->choices);

  choice->n_choices = g_strv_length ((gchar **)choices);
  choice->choices = g_new (GtkWidget *, choice->n_choices);
  for (i = 0; i < choice->n_choices; i++)
    {
      choice->choices[i] = gtk_label_new (choices[i]);
      gtk_widget_show (choice->choices[i]);
      gtk_stack_add_named (GTK_STACK (choice->stack),
                           choice->choices[i],
                           choices[i]);
    }

  g_object_notify_by_pspec (G_OBJECT (choice), choice_properties[PROP_CHOICES]);
}

void
gtk_multi_choice_set_format_callback (GtkMultiChoice               *choice,
                                      GtkMultiChoiceFormatCallback  callback,
                                      gpointer                      user_data,
                                      GDestroyNotify                destroy)
{
  if (choice->format_destroy)
    choice->format_destroy (choice->format_data);

  choice->format_cb = callback;
  choice->format_data = user_data;
  choice->format_destroy = destroy;
}
