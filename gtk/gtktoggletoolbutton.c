 /* gtktoggletoolbutton.c
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@gnome.org>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
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
#include "gtktoggletoolbutton.h"
#include "gtkcheckmenuitem.h"
#include "gtklabel.h"
#include "gtktogglebutton.h"
#include "gtkintl.h"
#include "gtkradiotoolbutton.h"
#include "gtkprivate.h"


/**
 * SECTION:gtktoggletoolbutton
 * @Short_description: A GtkToolItem containing a toggle button
 * @Title: GtkToggleToolButton
 * @See_also: #GtkToolbar, #GtkToolButton, #GtkSeparatorToolItem
 *
 * A #GtkToggleToolButton is a #GtkToolItem that contains a toggle
 * button.
 *
 * Use gtk_toggle_tool_button_new() to create a new GtkToggleToolButton.
 *
 * # CSS nodes
 *
 * GtkToggleToolButton has a single CSS node with name togglebutton.
 */


#define MENU_ID "gtk-toggle-tool-button-menu-id"

enum {
  TOGGLED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ACTIVE
};


struct _GtkToggleToolButtonPrivate
{
  guint active : 1;
};
  

static void     gtk_toggle_tool_button_set_property        (GObject      *object,
							    guint         prop_id,
							    const GValue *value,
							    GParamSpec   *pspec);
static void     gtk_toggle_tool_button_get_property        (GObject      *object,
							    guint         prop_id,
							    GValue       *value,
							    GParamSpec   *pspec);

static gboolean gtk_toggle_tool_button_create_menu_proxy (GtkToolItem *button);

static void button_toggled      (GtkWidget           *widget,
				 GtkToggleToolButton *button);
static void menu_item_activated (GtkWidget           *widget,
				 GtkToggleToolButton *button);


static guint                toggle_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkToggleToolButton, gtk_toggle_tool_button, GTK_TYPE_TOOL_BUTTON,
                         G_ADD_PRIVATE (GtkToggleToolButton));

static void
gtk_toggle_tool_button_class_init (GtkToggleToolButtonClass *klass)
{
  GObjectClass *object_class;
  GtkToolItemClass *toolitem_class;
  GtkToolButtonClass *toolbutton_class;

  object_class = (GObjectClass *)klass;
  toolitem_class = (GtkToolItemClass *)klass;
  toolbutton_class = (GtkToolButtonClass *)klass;

  object_class->set_property = gtk_toggle_tool_button_set_property;
  object_class->get_property = gtk_toggle_tool_button_get_property;

  toolitem_class->create_menu_proxy = gtk_toggle_tool_button_create_menu_proxy;
  toolbutton_class->button_type = GTK_TYPE_TOGGLE_BUTTON;

  /**
   * GtkToggleToolButton:active:
   *
   * If the toggle tool button should be pressed in.
   */
  g_object_class_install_property (object_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         P_("Active"),
                                                         P_("If the toggle button should be pressed in"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

/**
 * GtkToggleToolButton::toggled:
 * @toggle_tool_button: the object that emitted the signal
 *
 * Emitted whenever the toggle tool button changes state.
 **/
  toggle_signals[TOGGLED] =
    g_signal_new (I_("toggled"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkToggleToolButtonClass, toggled),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);
}

static void
gtk_toggle_tool_button_init (GtkToggleToolButton *button)
{
  GtkToolButton *tool_button = GTK_TOOL_BUTTON (button);
  GtkToggleButton *toggle_button = GTK_TOGGLE_BUTTON (_gtk_tool_button_get_button (tool_button));

  button->priv = gtk_toggle_tool_button_get_instance_private (button);

  /* If the real button is a radio button, it may have been
   * active at the time it was created.
   */
  button->priv->active = gtk_toggle_button_get_active (toggle_button);
    
  g_signal_connect_object (toggle_button,
			   "toggled", G_CALLBACK (button_toggled), button, 0);
}

static void
gtk_toggle_tool_button_set_property (GObject      *object,
				     guint         prop_id,
				     const GValue *value,
				     GParamSpec   *pspec)
{
  GtkToggleToolButton *button = GTK_TOGGLE_TOOL_BUTTON (object);

  switch (prop_id)
    {
      case PROP_ACTIVE:
	gtk_toggle_tool_button_set_active (button, 
					   g_value_get_boolean (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_toggle_tool_button_get_property (GObject    *object,
				     guint       prop_id,
				     GValue     *value,
				     GParamSpec *pspec)
{
  GtkToggleToolButton *button = GTK_TOGGLE_TOOL_BUTTON (object);

  switch (prop_id)
    {
      case PROP_ACTIVE:
        g_value_set_boolean (value, gtk_toggle_tool_button_get_active (button));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static gboolean
gtk_toggle_tool_button_create_menu_proxy (GtkToolItem *item)
{
  GtkToolButton *tool_button = GTK_TOOL_BUTTON (item);
  GtkToggleToolButton *toggle_tool_button = GTK_TOGGLE_TOOL_BUTTON (item);
  GtkWidget *menu_item = NULL;
  gboolean use_mnemonic = TRUE;
  const char *label;
  GtkWidget *label_widget;
  const gchar *label_text;

  if (_gtk_tool_item_create_menu_proxy (item))
    return TRUE;

  label_widget = gtk_tool_button_get_label_widget (tool_button);
  label_text = gtk_tool_button_get_label (tool_button);

  if (GTK_IS_LABEL (label_widget))
    {
      label = gtk_label_get_label (GTK_LABEL (label_widget));
      use_mnemonic = gtk_label_get_use_underline (GTK_LABEL (label_widget));
    }
  else if (label_text)
    {
      label = label_text;
      use_mnemonic = gtk_tool_button_get_use_underline (tool_button);
    }
  else
    {
      label = "";
    }

  if (use_mnemonic)
    menu_item = gtk_check_menu_item_new_with_mnemonic (label);
  else
    menu_item = gtk_check_menu_item_new_with_label (label);

  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
				  toggle_tool_button->priv->active);

  if (GTK_IS_RADIO_TOOL_BUTTON (toggle_tool_button))
    {
      gtk_check_menu_item_set_draw_as_radio (GTK_CHECK_MENU_ITEM (menu_item),
					     TRUE);
    }

  g_signal_connect_closure_by_id (menu_item,
				  g_signal_lookup ("activate", G_OBJECT_TYPE (menu_item)), 0,
				  g_cclosure_new_object (G_CALLBACK (menu_item_activated),
							 G_OBJECT (toggle_tool_button)),
				  FALSE);

  gtk_tool_item_set_proxy_menu_item (item, MENU_ID, menu_item);
  
  return TRUE;
}

/* There are two activatable widgets, a toggle button and a menu item.
 *
 * If a widget is activated and the state of the tool button is the same as
 * the new state of the activated widget, then the other widget was the one
 * that was activated by the user and updated the tool button’s state.
 *
 * If the state of the tool button is not the same as the new state of the
 * activated widget, then the activation was activated by the user, and the
 * widget needs to make sure the tool button is updated before the other
 * widget is activated. This will make sure the other widget a tool button
 * in a state that matches its own new state.
 */
static void
menu_item_activated (GtkWidget           *menu_item,
		     GtkToggleToolButton *toggle_tool_button)
{
  GtkToolButton *tool_button = GTK_TOOL_BUTTON (toggle_tool_button);
  gboolean menu_active = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menu_item));

  if (toggle_tool_button->priv->active != menu_active)
    {
      toggle_tool_button->priv->active = menu_active;

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (_gtk_tool_button_get_button (tool_button)),
				    toggle_tool_button->priv->active);

      g_object_notify (G_OBJECT (toggle_tool_button), "active");
      g_signal_emit (toggle_tool_button, toggle_signals[TOGGLED], 0);
    }
}

static void
button_toggled (GtkWidget           *widget,
		GtkToggleToolButton *toggle_tool_button)
{
  gboolean toggle_active;

  toggle_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

  if (toggle_tool_button->priv->active != toggle_active)
    {
      GtkWidget *menu_item;
      
      toggle_tool_button->priv->active = toggle_active;
       
      if ((menu_item =
	   gtk_tool_item_get_proxy_menu_item (GTK_TOOL_ITEM (toggle_tool_button), MENU_ID)))
	{
	  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
					  toggle_tool_button->priv->active);
	}

      g_object_notify (G_OBJECT (toggle_tool_button), "active");
      g_signal_emit (toggle_tool_button, toggle_signals[TOGGLED], 0);
    }
}

/**
 * gtk_toggle_tool_button_new:
 * 
 * Returns a new #GtkToggleToolButton
 * 
 * Returns: a newly created #GtkToggleToolButton
 **/
GtkToolItem *
gtk_toggle_tool_button_new (void)
{
  GtkToolButton *button;

  button = g_object_new (GTK_TYPE_TOGGLE_TOOL_BUTTON,
			 NULL);

  return GTK_TOOL_ITEM (button);
}

/**
 * gtk_toggle_tool_button_set_active:
 * @button: a #GtkToggleToolButton
 * @is_active: whether @button should be active
 * 
 * Sets the status of the toggle tool button. Set to %TRUE if you
 * want the GtkToggleButton to be “pressed in”, and %FALSE to raise it.
 * This action causes the toggled signal to be emitted.
 **/
void
gtk_toggle_tool_button_set_active (GtkToggleToolButton *button,
                                   gboolean             is_active)
{
  g_return_if_fail (GTK_IS_TOGGLE_TOOL_BUTTON (button));

  is_active = is_active != FALSE;

  if (button->priv->active != is_active)
    {
      g_signal_emit_by_name (_gtk_tool_button_get_button (GTK_TOOL_BUTTON (button)), "clicked");
      g_object_notify (G_OBJECT (button), "active");
    }
}

/**
 * gtk_toggle_tool_button_get_active:
 * @button: a #GtkToggleToolButton
 * 
 * Queries a #GtkToggleToolButton and returns its current state.
 * Returns %TRUE if the toggle button is pressed in and %FALSE if it is raised.
 * 
 * Returns: %TRUE if the toggle tool button is pressed in, %FALSE if not
 **/
gboolean
gtk_toggle_tool_button_get_active (GtkToggleToolButton *button)
{
  g_return_val_if_fail (GTK_IS_TOGGLE_TOOL_BUTTON (button), FALSE);

  return button->priv->active;
}
