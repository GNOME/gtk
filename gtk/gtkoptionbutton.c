/* GTK - The GIMP Toolkit
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

#include <string.h>

#include "gtkoptionbutton.h"

#include "gtkoptionlist.h"
#include "gtktogglebutton.h"
#include "gtkbox.h"
#include "gtklabel.h"
#include "gtkintl.h"
#include "gtkprivate.h"


enum {
  PROP_PLACEHOLDER_TEXT = 1
};

struct _GtkOptionButton
{
  GtkBin parent;

  GtkWidget *label;
  GtkWidget *popover;
  GtkWidget *list;

  gchar *placeholder_text;
};

struct _GtkOptionButtonClass
{
  GtkBinClass parent_class;
};

G_DEFINE_TYPE (GtkOptionButton, gtk_option_button, GTK_TYPE_BIN)

static void
gtk_option_button_init (GtkOptionButton *combo)
{
  gtk_widget_init_template (GTK_WIDGET (combo));
}

static void
gtk_option_button_finalize (GObject *object)
{
  GtkOptionButton *button = GTK_OPTION_BUTTON (object);

  g_free (button->placeholder_text);

  G_OBJECT_CLASS (gtk_option_button_parent_class)->finalize (object);
}

static void
gtk_option_button_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkOptionButton *button = (GtkOptionButton *)object;

  switch (prop_id)
    {
    case PROP_PLACEHOLDER_TEXT:
      gtk_option_button_set_placeholder_text (button, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_option_button_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkOptionButton *button = (GtkOptionButton *)object;

  switch (prop_id)
    {
    case PROP_PLACEHOLDER_TEXT:
      g_value_set_string (value, gtk_option_button_get_placeholder_text (button));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean popover_key_press (GtkWidget       *widget,
                                   GdkEvent        *event,
                                   GtkOptionButton *button);
static gboolean button_key_press  (GtkWidget       *widget,
                                   GdkEvent        *event,
                                   GtkOptionButton *button);
static void     selected_changed  (GObject         *list,
                                   GParamSpec      *pspec,
                                   GtkOptionButton *button);

static void
gtk_option_button_class_init (GtkOptionButtonClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = gtk_option_button_finalize;
  object_class->set_property = gtk_option_button_set_property;
  object_class->get_property = gtk_option_button_get_property;

  /**
   * GtkOptionButton:placeholder-text:
   *
   * The text that is displayed if no item is selected.
   */
  g_object_class_install_property (object_class,
                                   PROP_PLACEHOLDER_TEXT,
                                   g_param_spec_string ("placeholder-text",
                                                        P_("Placeholder text"),
                                                        P_("Text to show when no item is selected"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkoptionbutton.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkOptionButton, label);
  gtk_widget_class_bind_template_child (widget_class, GtkOptionButton, popover);
  gtk_widget_class_bind_template_child_internal (widget_class, GtkOptionButton, list);
  gtk_widget_class_bind_template_callback (widget_class, popover_key_press);
  gtk_widget_class_bind_template_callback (widget_class, button_key_press);
  gtk_widget_class_bind_template_callback (widget_class, selected_changed);
}


/***/


static gboolean
popover_key_press (GtkWidget        *widget,
                   GdkEvent         *event,
                   GtkOptionButton  *button)
{
  return gtk_option_list_handle_key_event (GTK_OPTION_LIST (button->list), event);
}

static gboolean
button_key_press (GtkWidget        *widget,
                  GdkEvent         *event,
                  GtkOptionButton  *button)
{
  gboolean handled;

  handled = gtk_option_list_handle_key_event (GTK_OPTION_LIST (button->list), event);

  if (handled == GDK_EVENT_STOP)
    gtk_widget_show (button->popover);

  return handled;
}

static void
selected_changed (GObject         *list,
                  GParamSpec      *pspec,
                  GtkOptionButton *button)
{
  const gchar **selected;

  selected = gtk_option_list_get_selected_items (GTK_OPTION_LIST (button->list));
  if (selected[0] == NULL)
    {
      gtk_label_set_text (GTK_LABEL (button->label), button->placeholder_text);
    }
  else
    {
      GString *str;
      const gchar *text;
      gint i;

      str = g_string_new ("");
      for (i = 0; selected[i]; i++)
        {
          if (i > 0)
            g_string_append (str, ", ");

          text = gtk_option_list_item_get_text (GTK_OPTION_LIST (button->list), selected[i]);
          g_string_append (str, text);
        }
      gtk_label_set_text (GTK_LABEL (button->label), str->str);
      g_string_free (str, TRUE);
    }
}


/***/


/**
 * gtk_option_button_new:
 *
 * Creates a new #GtkOptionButton.
 *
 * Returns: A new #GtkOptionButton
 *
 * Since: 3.16
 */
GtkWidget *
gtk_option_button_new (void)
{
  return g_object_new (GTK_TYPE_OPTION_BUTTON, NULL);
}

GtkWidget *
gtk_option_button_get_option_list (GtkOptionButton *button)
{
  g_return_val_if_fail (GTK_IS_OPTION_BUTTON (button), NULL);

  return button->list;
}

/**
 * gtk_option_button_set_placeholder_text:
 * @button: a #GtkOptionButton
 * @text: the placeholder text to use
 *
 * Sets the placeholder text that is displayed in the button
 * if no item is currently selected.
 *
 * Since: 3.16
 */
void
gtk_option_button_set_placeholder_text (GtkOptionButton *button,
                                        const gchar     *text)
{
  g_return_if_fail (GTK_IS_OPTION_BUTTON (button));
  g_return_if_fail (text != NULL);

  g_free (button->placeholder_text);
  button->placeholder_text = g_strdup (text);

  if (button->label != NULL && button->list != NULL)
    {
      const gchar **selected;

      selected = gtk_option_list_get_selected_items (GTK_OPTION_LIST (button->list));
      if (selected[0] == NULL)
        gtk_label_set_text (GTK_LABEL (button->label), button->placeholder_text);
    }

  g_object_notify (G_OBJECT (button), "placeholder-text");
}

/**
 * gtk_option_button_get_placeholder_text:
 * @button: a #GtkOptionButton
 *
 * Gets the placeholder text that is displayed in the combo
 * if no item is currently selected.
 *
 * Returns: (transfer none): the placeholder text
 *
 * Since: 3.16
 */
const gchar *
gtk_option_button_get_placeholder_text (GtkOptionButton *button)
{
  g_return_val_if_fail (GTK_IS_OPTION_BUTTON (button), NULL);

  return button->placeholder_text;
}
