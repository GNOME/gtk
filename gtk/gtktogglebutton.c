/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gtktogglebutton.h"

#include "gtkbuttonprivate.h"
#include "gtktogglebuttonprivate.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "a11y/gtktogglebuttonaccessible.h"


/**
 * SECTION:gtktogglebutton
 * @Short_description: Create buttons which retain their state
 * @Title: GtkToggleButton
 * @See_also: #GtkButton, #GtkCheckButton, #GtkCheckMenuItem
 *
 * A #GtkToggleButton is a #GtkButton which will remain “pressed-in” when
 * clicked. Clicking again will cause the toggle button to return to its
 * normal state.
 *
 * A toggle button is created by calling either gtk_toggle_button_new() or
 * gtk_toggle_button_new_with_label(). If using the former, it is advisable to
 * pack a widget, (such as a #GtkLabel and/or a #GtkImage), into the toggle
 * button’s container. (See #GtkButton for more information).
 *
 * The state of a #GtkToggleButton can be set specifically using
 * gtk_toggle_button_set_active(), and retrieved using
 * gtk_toggle_button_get_active().
 *
 * To simply switch the state of a toggle button, use gtk_toggle_button_toggled().
 *
 * # CSS nodes
 *
 * GtkToggleButton has a single CSS node with name button. To differentiate
 * it from a plain #GtkButton, it gets the .toggle style class.
 *
 * ## Creating two #GtkToggleButton widgets.
 *
 * |[<!-- language="C" -->
 * void make_toggles (void) {
 *    GtkWidget *dialog, *toggle1, *toggle2;
 *    GtkWidget *content_area;
 *    const char *text;
 *
 *    dialog = gtk_dialog_new (text);
 *    content_area = gtk_dialog_get_content_area ();
 *
 *    text = "Hi, i’m a toggle button.";
 *    toggle1 = gtk_toggle_button_new_with_label (text);
 *
 *    g_signal_connect (toggle1, "toggled",
 *                      G_CALLBACK (output_state),
 *                      NULL);
 *    gtk_box_pack_start (GTK_BOX (content_area),
 *                        toggle1, FALSE, FALSE, 2);
 *
 *    text = "Hi, i’m a toggle button.";
 *    toggle2 = gtk_toggle_button_new_with_label (text);
 *    g_signal_connect (toggle2, "toggled",
 *                      G_CALLBACK (output_state),
 *                      NULL);
 *    gtk_box_pack_start (GTK_BOX (content_area),
 *                        toggle2, FALSE, FALSE, 2);
 *
 *    gtk_widget_show (dialog);
 * }
 * ]|
 */

struct _GtkToggleButtonPrivate
{
  guint active         : 1;
};

enum {
  TOGGLED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ACTIVE,
  NUM_PROPERTIES
};

static GParamSpec *toggle_button_props[NUM_PROPERTIES] = { NULL, };

static gboolean gtk_toggle_button_mnemonic_activate  (GtkWidget            *widget,
                                                      gboolean              group_cycling);
static void gtk_toggle_button_clicked       (GtkButton            *button);
static void gtk_toggle_button_set_property  (GObject              *object,
					     guint                 prop_id,
					     const GValue         *value,
					     GParamSpec           *pspec);
static void gtk_toggle_button_get_property  (GObject              *object,
					     guint                 prop_id,
					     GValue               *value,
					     GParamSpec           *pspec);


static guint                toggle_button_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkToggleButton, gtk_toggle_button, GTK_TYPE_BUTTON,
                         G_ADD_PRIVATE (GtkToggleButton))

static void
gtk_toggle_button_class_init (GtkToggleButtonClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkButtonClass *button_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;
  button_class = (GtkButtonClass*) class;

  gobject_class->set_property = gtk_toggle_button_set_property;
  gobject_class->get_property = gtk_toggle_button_get_property;

  widget_class->mnemonic_activate = gtk_toggle_button_mnemonic_activate;

  button_class->clicked = gtk_toggle_button_clicked;

  class->toggled = NULL;

  toggle_button_props[PROP_ACTIVE] =
      g_param_spec_boolean ("active",
                            P_("Active"),
                            P_("If the toggle button should be pressed in"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, toggle_button_props);

  /**
   * GtkToggleButton::toggled:
   * @togglebutton: the object which received the signal.
   *
   * Should be connected if you wish to perform an action whenever the
   * #GtkToggleButton's state is changed.
   */
  toggle_button_signals[TOGGLED] =
    g_signal_new (I_("toggled"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkToggleButtonClass, toggled),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_TOGGLE_BUTTON_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, "button");
}

static void
gtk_toggle_button_init (GtkToggleButton *toggle_button)
{
  GtkStyleContext *context;

  toggle_button->priv = gtk_toggle_button_get_instance_private (toggle_button);
  toggle_button->priv->active = FALSE;

  context = gtk_widget_get_style_context (GTK_WIDGET (toggle_button));
  gtk_style_context_add_class (context, "toggle");
}


/**
 * gtk_toggle_button_new:
 *
 * Creates a new toggle button. A widget should be packed into the button, as in gtk_button_new().
 *
 * Returns: a new toggle button.
 */
GtkWidget*
gtk_toggle_button_new (void)
{
  return g_object_new (GTK_TYPE_TOGGLE_BUTTON, NULL);
}

/**
 * gtk_toggle_button_new_with_label:
 * @label: a string containing the message to be placed in the toggle button.
 *
 * Creates a new toggle button with a text label.
 *
 * Returns: a new toggle button.
 */
GtkWidget*
gtk_toggle_button_new_with_label (const gchar *label)
{
  return g_object_new (GTK_TYPE_TOGGLE_BUTTON, "label", label, NULL);
}

/**
 * gtk_toggle_button_new_with_mnemonic:
 * @label: the text of the button, with an underscore in front of the
 *         mnemonic character
 *
 * Creates a new #GtkToggleButton containing a label. The label
 * will be created using gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the button.
 *
 * Returns: a new #GtkToggleButton
 */
GtkWidget*
gtk_toggle_button_new_with_mnemonic (const gchar *label)
{
  return g_object_new (GTK_TYPE_TOGGLE_BUTTON, 
		       "label", label, 
		       "use-underline", TRUE, 
		       NULL);
}

static void
gtk_toggle_button_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
  GtkToggleButton *tb;

  tb = GTK_TOGGLE_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      gtk_toggle_button_set_active (tb, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_toggle_button_get_property (GObject      *object,
				guint         prop_id,
				GValue       *value,
				GParamSpec   *pspec)
{
  GtkToggleButton *tb = GTK_TOGGLE_BUTTON (object);
  GtkToggleButtonPrivate *priv = tb->priv;

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, priv->active);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_toggle_button_set_active:
 * @toggle_button: a #GtkToggleButton.
 * @is_active: %TRUE or %FALSE.
 *
 * Sets the status of the toggle button. Set to %TRUE if you want the
 * GtkToggleButton to be “pressed in”, and %FALSE to raise it.
 * This action causes the #GtkToggleButton::toggled signal and the
 * #GtkButton::clicked signal to be emitted.
 */
void
gtk_toggle_button_set_active (GtkToggleButton *toggle_button,
			      gboolean         is_active)
{
  GtkToggleButtonPrivate *priv;

  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

  priv = toggle_button->priv;

  is_active = is_active != FALSE;

  if (priv->active != is_active)
    {
      gtk_button_clicked (GTK_BUTTON (toggle_button));
      g_object_notify_by_pspec (G_OBJECT (toggle_button), toggle_button_props[PROP_ACTIVE]);
    }
}

void
_gtk_toggle_button_set_active (GtkToggleButton *toggle_button,
                               gboolean         is_active)
{
  toggle_button->priv->active = is_active;

  if (is_active)
    gtk_widget_set_state_flags (GTK_WIDGET (toggle_button), GTK_STATE_FLAG_CHECKED, FALSE);
  else
    gtk_widget_unset_state_flags (GTK_WIDGET (toggle_button), GTK_STATE_FLAG_CHECKED);

}

/**
 * gtk_toggle_button_get_active:
 * @toggle_button: a #GtkToggleButton.
 *
 * Queries a #GtkToggleButton and returns its current state. Returns %TRUE if
 * the toggle button is pressed in and %FALSE if it is raised.
 *
 * Returns: a #gboolean value.
 */
gboolean
gtk_toggle_button_get_active (GtkToggleButton *toggle_button)
{
  g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button), FALSE);

  return toggle_button->priv->active;
}

/**
 * gtk_toggle_button_toggled:
 * @toggle_button: a #GtkToggleButton.
 *
 * Emits the #GtkToggleButton::toggled signal on the
 * #GtkToggleButton. There is no good reason for an
 * application ever to call this function.
 */
void
gtk_toggle_button_toggled (GtkToggleButton *toggle_button)
{
  g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

  g_signal_emit (toggle_button, toggle_button_signals[TOGGLED], 0);
}

static gboolean
gtk_toggle_button_mnemonic_activate (GtkWidget *widget,
                                     gboolean   group_cycling)
{
  /*
   * We override the standard implementation in 
   * gtk_widget_real_mnemonic_activate() in order to focus the widget even
   * if there is no mnemonic conflict.
   */
  if (gtk_widget_get_can_focus (widget))
    gtk_widget_grab_focus (widget);

  if (!group_cycling)
    gtk_widget_activate (widget);

  return TRUE;
}

static void
gtk_toggle_button_clicked (GtkButton *button)
{
  GtkToggleButton *toggle_button = GTK_TOGGLE_BUTTON (button);
  GtkToggleButtonPrivate *priv = toggle_button->priv;

  _gtk_toggle_button_set_active (toggle_button, !priv->active);

  gtk_toggle_button_toggled (toggle_button);

  g_object_notify_by_pspec (G_OBJECT (toggle_button), toggle_button_props[PROP_ACTIVE]);

  if (GTK_BUTTON_CLASS (gtk_toggle_button_parent_class)->clicked)
    GTK_BUTTON_CLASS (gtk_toggle_button_parent_class)->clicked (button);
}

