/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 1998, 1999 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Author: James Henstridge <james@daa.com.au>
 *
 * Modified by the GTK+ Team and others 2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>

#include "gtkradioaction.h"
#include "gtktoggleactionprivate.h"

#define GTK_RADIO_ACTION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_RADIO_ACTION, GtkRadioActionPrivate))

struct _GtkRadioActionPrivate 
{
  GSList *group;
};

static void gtk_radio_action_init       (GtkRadioAction *action);
static void gtk_radio_action_class_init (GtkRadioActionClass *class);

GType
gtk_radio_action_get_type (void)
{
  static GtkType type = 0;

  if (!type)
    {
      static const GTypeInfo type_info =
      {
        sizeof (GtkRadioActionClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gtk_radio_action_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,
        
        sizeof (GtkRadioAction),
        0, /* n_preallocs */
        (GInstanceInitFunc) gtk_radio_action_init,
      };

      type = g_type_register_static (GTK_TYPE_TOGGLE_ACTION,
                                     "GtkRadioAction",
                                     &type_info, 0);
    }
  return type;
}

static void gtk_radio_action_finalize (GObject *object);
static void gtk_radio_action_activate (GtkAction *action);

static GObjectClass *parent_class = NULL;

static void
gtk_radio_action_class_init (GtkRadioActionClass *klass)
{
  GObjectClass *gobject_class;
  GtkActionClass *action_class;

  parent_class = g_type_class_peek_parent (klass);
  gobject_class = G_OBJECT_CLASS (klass);
  action_class = GTK_ACTION_CLASS (klass);

  gobject_class->finalize = gtk_radio_action_finalize;

  action_class->activate = gtk_radio_action_activate;

  g_type_class_add_private (gobject_class, sizeof (GtkRadioActionPrivate));
}

static void
gtk_radio_action_init (GtkRadioAction *action)
{
  action->private_data = GTK_RADIO_ACTION_GET_PRIVATE (action);
  action->private_data->group = g_slist_prepend (NULL, action);
}

static void
gtk_radio_action_finalize (GObject *object)
{
  GtkRadioAction *action;
  GSList *tmp_list;

  action = GTK_RADIO_ACTION (object);

  action->private_data->group = g_slist_remove (action->private_data->group, action);

  tmp_list = action->private_data->group;

  while (tmp_list)
    {
      GtkRadioAction *tmp_action = tmp_list->data;

      tmp_list = tmp_list->next;
      tmp_action->private_data->group = action->private_data->group;
    }

  if (parent_class->finalize)
    (* parent_class->finalize) (object);
}

static void
gtk_radio_action_activate (GtkAction *action)
{
  GtkRadioAction *radio_action;
  GtkToggleAction *toggle_action;
  GtkToggleAction *tmp_action;
  GSList *tmp_list;

  radio_action = GTK_RADIO_ACTION (action);
  toggle_action = GTK_TOGGLE_ACTION (action);

  if (toggle_action->private_data->active)
    {
      tmp_list = radio_action->private_data->group;

      while (tmp_list)
	{
	  tmp_action = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (tmp_action->private_data->active && (tmp_action != toggle_action)) 
	    {
	      toggle_action->private_data->active = !toggle_action->private_data->active;
	      break;
	    }
	}
    }
  else
    {
      toggle_action->private_data->active = !toggle_action->private_data->active;

      tmp_list = radio_action->private_data->group;
      while (tmp_list)
	{
	  tmp_action = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (tmp_action->private_data->active && (tmp_action != toggle_action))
	    {
	      gtk_action_activate (GTK_ACTION (tmp_action));
	      break;
	    }
	}
    }

  gtk_toggle_action_toggled (toggle_action);
}

/**
 * gtk_radio_action_get_group:
 * @action: the action object
 *
 * Returns the list representing the radio group for this object
 *
 * Returns: the list representing the radio group for this object
 *
 * Since: 2.4
 */
GSList *
gtk_radio_action_get_group (GtkRadioAction *action)
{
  g_return_val_if_fail (GTK_IS_RADIO_ACTION (action), NULL);

  return action->private_data->group;
}

/**
 * gtk_radio_action_set_group:
 * @action: the action object
 * @group: a list representing a radio group
 *
 * Sets the radio group for the radio action object.
 *
 * Since: 2.4
 */
void
gtk_radio_action_set_group (GtkRadioAction *action, 
			    GSList         *group)
{
  g_return_if_fail (GTK_IS_RADIO_ACTION (action));
  g_return_if_fail (!g_slist_find (group, action));

  if (action->private_data->group)
    {
      GSList *slist;

      action->private_data->group = g_slist_remove (action->private_data->group, action);

      for (slist = action->private_data->group; slist; slist = slist->next)
	{
	  GtkRadioAction *tmp_action = slist->data;

	  tmp_action->private_data->group = action->private_data->group;
	}
    }

  action->private_data->group = g_slist_prepend (group, action);

  if (group)
    {
      GSList *slist;

      for (slist = action->private_data->group; slist; slist = slist->next)
	{
	  GtkRadioAction *tmp_action = slist->data;

	  tmp_action->private_data->group = action->private_data->group;
	}
    }
  else
    {
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
    }
}
