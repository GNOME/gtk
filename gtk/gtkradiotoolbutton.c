/* gtkradiotoolbutton.c
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@gnome.og>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
 * Copyright (C) 2003 Soeren Sandmann <sandmann@daimi.au.dk>
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
#include "gtkradiotoolbutton.h"
#include "gtkradiobutton.h"
#include "gtkintl.h"
#include "gtkprivate.h"


/**
 * SECTION:gtkradiotoolbutton
 * @Short_description: A toolbar item that contains a radio button
 * @Title: GtkRadioToolButton
 * @See_also: #GtkToolbar, #GtkToolButton
 *
 * A #GtkRadioToolButton is a #GtkToolItem that contains a radio button,
 * that is, a button that is part of a group of toggle buttons where only
 * one button can be active at a time.
 *
 * Use gtk_radio_tool_button_new() to create a new GtkRadioToolButton. Use
 * gtk_radio_tool_button_new_from_widget() to create a new GtkRadioToolButton
 * that is part of the same group as an existing GtkRadioToolButton.
 *
 * # CSS nodes
 *
 * GtkRadioToolButton has a single CSS node with name toolbutton.
 */

typedef struct _GtkRadioToolButtonClass GtkRadioToolButtonClass;

struct _GtkRadioToolButton
{
  GtkToggleToolButton parent;
};

struct _GtkRadioToolButtonClass
{
  GtkToggleToolButtonClass parent_class;
};


enum {
  PROP_0,
  PROP_GROUP
};

static void gtk_radio_tool_button_set_property (GObject         *object,
						guint            prop_id,
						const GValue    *value,
						GParamSpec      *pspec);

G_DEFINE_TYPE (GtkRadioToolButton, gtk_radio_tool_button, GTK_TYPE_TOGGLE_TOOL_BUTTON)

static void
gtk_radio_tool_button_class_init (GtkRadioToolButtonClass *klass)
{
  GObjectClass *object_class;
  GtkToolButtonClass *toolbutton_class;

  object_class = (GObjectClass *)klass;
  toolbutton_class = (GtkToolButtonClass *)klass;

  object_class->set_property = gtk_radio_tool_button_set_property;
  
  toolbutton_class->button_type = GTK_TYPE_RADIO_BUTTON;  

  /**
   * GtkRadioToolButton:group:
   *
   * Sets a new group for a radio tool button.
   */
  g_object_class_install_property (object_class,
				   PROP_GROUP,
				   g_param_spec_object ("group",
							P_("Group"),
							P_("The radio tool button whose group this button belongs to."),
							GTK_TYPE_RADIO_TOOL_BUTTON,
							GTK_PARAM_WRITABLE));

}

static void
gtk_radio_tool_button_init (GtkRadioToolButton *button)
{
  GtkToolButton *tool_button = GTK_TOOL_BUTTON (button);
  gtk_check_button_set_draw_indicator (GTK_CHECK_BUTTON (_gtk_tool_button_get_button (tool_button)), FALSE);
}

static void
gtk_radio_tool_button_set_property (GObject         *object,
				    guint            prop_id,
				    const GValue    *value,
				    GParamSpec      *pspec)
{
  GtkRadioToolButton *button;

  button = GTK_RADIO_TOOL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_GROUP:
      {
	GtkRadioToolButton *arg;
	GSList *slist = NULL;
	if (G_VALUE_HOLDS_OBJECT (value)) 
	  {
	    arg = GTK_RADIO_TOOL_BUTTON (g_value_get_object (value));
	    if (arg)
	      slist = gtk_radio_tool_button_get_group (arg);
	    gtk_radio_tool_button_set_group (button, slist);
	  }
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_radio_tool_button_new:
 * @group: (allow-none) (element-type GtkRadioButton): An
 *   existing radio button group, or %NULL if you are creating a new group
 * 
 * Creates a new #GtkRadioToolButton, adding it to @group.
 * 
 * Returns: The new #GtkRadioToolButton
 **/
GtkToolItem *
gtk_radio_tool_button_new (GSList *group)
{
  GtkRadioToolButton *button;
  
  button = g_object_new (GTK_TYPE_RADIO_TOOL_BUTTON,
			 NULL);

  gtk_radio_tool_button_set_group (button, group);
  
  return GTK_TOOL_ITEM (button);
}

/**
 * gtk_radio_tool_button_new_from_widget: (constructor)
 * @group: (allow-none): An existing #GtkRadioToolButton, or %NULL
 *
 * Creates a new #GtkRadioToolButton adding it to the same group as @gruup
 *
 * Returns: (transfer none): The new #GtkRadioToolButton
 **/
GtkToolItem *
gtk_radio_tool_button_new_from_widget (GtkRadioToolButton *group)
{
  GSList *list = NULL;
  
  g_return_val_if_fail (group == NULL || GTK_IS_RADIO_TOOL_BUTTON (group), NULL);

  if (group != NULL)
    list = gtk_radio_tool_button_get_group (GTK_RADIO_TOOL_BUTTON (group));
  
  return gtk_radio_tool_button_new (list);
}

static GtkRadioButton *
get_radio_button (GtkRadioToolButton *button)
{
  return GTK_RADIO_BUTTON (_gtk_tool_button_get_button (GTK_TOOL_BUTTON (button)));
}

/**
 * gtk_radio_tool_button_get_group:
 * @button: a #GtkRadioToolButton
 *
 * Returns the radio button group @button belongs to.
 *
 * Returns: (transfer none) (element-type GtkRadioButton): The group @button belongs to.
 */
GSList *
gtk_radio_tool_button_get_group (GtkRadioToolButton *button)
{
  g_return_val_if_fail (GTK_IS_RADIO_TOOL_BUTTON (button), NULL);

  return gtk_radio_button_get_group (get_radio_button (button));
}

/**
 * gtk_radio_tool_button_set_group:
 * @button: a #GtkRadioToolButton
 * @group: (element-type GtkRadioButton) (allow-none): an existing radio button group, or %NULL
 * 
 * Adds @button to @group, removing it from the group it belonged to before.
 **/
void
gtk_radio_tool_button_set_group (GtkRadioToolButton *button,
				 GSList             *group)
{
  g_return_if_fail (GTK_IS_RADIO_TOOL_BUTTON (button));

  gtk_radio_button_set_group (get_radio_button (button), group);
}
