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

#include "gtkactiongroup.h"
#include "gtktoggleaction.h"
#include "gtkradioaction.h"
#include "gtkaccelmap.h"
#include "gtkintl.h"

#define GTK_ACTION_GROUP_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_ACTION_GROUP, GtkActionGroupPrivate))

struct _GtkActionGroupPrivate 
{
  gchar           *name;
  GHashTable      *actions;

  GtkTranslateFunc translate_func;
  gpointer         translate_data;
  GtkDestroyNotify translate_notify;   
};

static void gtk_action_group_init       (GtkActionGroup *self);
static void gtk_action_group_class_init (GtkActionGroupClass *class);

GType
gtk_action_group_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo type_info =
      {
        sizeof (GtkActionGroupClass),
	NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gtk_action_group_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GtkActionGroup),
        0, /* n_preallocs */
        (GInstanceInitFunc) gtk_action_group_init,
      };

      type = g_type_register_static (G_TYPE_OBJECT, "GtkActionGroup",
				     &type_info, 0);
    }

  return type;
}

static GObjectClass *parent_class = NULL;
static void       gtk_action_group_finalize        (GObject        *object);
static GtkAction *gtk_action_group_real_get_action (GtkActionGroup *self,
						    const gchar    *name);

static void
gtk_action_group_class_init (GtkActionGroupClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gtk_action_group_finalize;
  klass->get_action = gtk_action_group_real_get_action;

  g_type_class_add_private (gobject_class, sizeof (GtkActionGroupPrivate));
}

static void
gtk_action_group_init (GtkActionGroup *self)
{
  self->private_data = GTK_ACTION_GROUP_GET_PRIVATE (self);
  self->private_data->name = NULL;
  self->private_data->actions = g_hash_table_new_full (g_str_hash, g_str_equal,
						       (GDestroyNotify) g_free,
						       (GDestroyNotify) g_object_unref);
  self->private_data->translate_func = NULL;
  self->private_data->translate_data = NULL;
  self->private_data->translate_notify = NULL;
}

/**
 * gtk_action_group_new:
 * @name: the name of the action group
 *
 * Creates a new #GtkActionGroup object.
 *
 * Returns: the new #GtkActionGroup
 *
 * Since: 2.4
 */
GtkActionGroup *
gtk_action_group_new (const gchar *name)
{
  GtkActionGroup *self;

  self = g_object_new (GTK_TYPE_ACTION_GROUP, NULL);
  self->private_data->name = g_strdup (name);

  return self;
}

static void
gtk_action_group_finalize (GObject *object)
{
  GtkActionGroup *self;

  self = GTK_ACTION_GROUP (object);

  g_free (self->private_data->name);
  self->private_data->name = NULL;

  g_hash_table_destroy (self->private_data->actions);
  self->private_data->actions = NULL;

  if (self->private_data->translate_notify)
    self->private_data->translate_notify (self->private_data->translate_data);

  if (parent_class->finalize)
    (* parent_class->finalize) (object);
}

static GtkAction *
gtk_action_group_real_get_action (GtkActionGroup *self,
				  const gchar    *action_name)
{
  return g_hash_table_lookup (self->private_data->actions, action_name);
}

/**
 * gtk_action_group_get_name:
 * @action_group: the action group
 *
 * Gets the name of the action group.
 *
 * Returns: the name of the action group.
 * 
 * Since: 2.4
 */
const gchar *
gtk_action_group_get_name (GtkActionGroup *action_group)
{
  g_return_val_if_fail (GTK_IS_ACTION_GROUP (action_group), NULL);

  return action_group->private_data->name;
}

/**
 * gtk_action_group_get_action:
 * @action_group: the action group
 * @action_name: the name of the action
 *
 * Looks up an action in the action group by name.
 *
 * Returns: the action, or %NULL if no action by that name exists
 *
 * Since: 2.4
 */
GtkAction *
gtk_action_group_get_action (GtkActionGroup *action_group,
			     const gchar    *action_name)
{
  g_return_val_if_fail (GTK_IS_ACTION_GROUP (action_group), NULL);
  g_return_val_if_fail (GTK_ACTION_GROUP_GET_CLASS (action_group)->get_action != NULL, NULL);

  return (* GTK_ACTION_GROUP_GET_CLASS (action_group)->get_action)
    (action_group, action_name);
}

/**
 * gtk_action_group_add_action:
 * @action_group: the action group
 * @action: an action
 *
 * Adds an action object to the action group.
 *
 * Since: 2.4
 */
void
gtk_action_group_add_action (GtkActionGroup *action_group,
			     GtkAction      *action)
{
  g_return_if_fail (GTK_IS_ACTION_GROUP (action_group));
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (gtk_action_get_name (action) != NULL);

  g_hash_table_insert (action_group->private_data->actions, 
		       g_strdup (gtk_action_get_name (action)),
                       g_object_ref (action));
}

/**
 * gtk_action_group_removes_action:
 * @action_group: the action group
 * @action: an action
 *
 * Removes an action object from the action group.
 *
 * Since: 2.4
 */
void
gtk_action_group_remove_action (GtkActionGroup *action_group,
				GtkAction      *action)
{
  g_return_if_fail (GTK_IS_ACTION_GROUP (action_group));
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (gtk_action_get_name (action) != NULL);

  /* extra protection to make sure action->name is valid */
  g_object_ref (action);
  g_hash_table_remove (action_group->private_data->actions, gtk_action_get_name (action));
  g_object_unref (action);
}

static void
add_single_action (gpointer key, 
		   gpointer value, 
		   gpointer user_data)
{
  GList **list = user_data;

  *list = g_list_prepend (*list, value);
}

/**
 * gtk_action_group_list_actions:
 * @action_group: the action group
 *
 * Lists the actions in the action group.
 *
 * Returns: an allocated list of the action objects in the action group
 * 
 * Since: 2.4
 */
GList *
gtk_action_group_list_actions (GtkActionGroup *action_group)
{
  GList *actions = NULL;
  g_return_val_if_fail (GTK_IS_ACTION_GROUP (action_group), NULL);
  
  g_hash_table_foreach (action_group->private_data->actions, add_single_action, &actions);

  return g_list_reverse (actions);
}


/**
 * gtk_action_group_add_actions:
 * @action_group: the action group
 * @entries: an array of action descriptions
 * @n_entries: the number of entries
 * @user_data: data to pass to the action callbacks
 *
 * This is a convenience routine to create a number of actions and add
 * them to the action group.  Each member of the array describes an
 * action to create.
 * 
 * Since: 2.4
 */
void
gtk_action_group_add_actions (GtkActionGroup *action_group,
			      GtkActionEntry *entries,
			      guint           n_entries,
			      gpointer        user_data)
{
  gtk_action_group_add_actions_full (action_group, 
				     entries, n_entries, 
				     user_data, NULL);
}


/**
 * gtk_action_group_add_actions_full:
 * @action_group: the action group
 * @entries: an array of action descriptions
 * @n_entries: the number of entries
 * @user_data: data to pass to the action callbacks
 * @destroy: destroy notification callback for @user_data
 *
 * This variant of gtk_action_group_add_actions() adds a #GDestroyNotify
 * callback for @user_data. 
 * 
 * Since: 2.4
 */
void
gtk_action_group_add_actions_full (GtkActionGroup *action_group,
				   GtkActionEntry *entries,
				   guint           n_entries,
				   gpointer        user_data,
				   GDestroyNotify  destroy)
{
  guint i;
  GtkTranslateFunc translate_func;
  gpointer translate_data;

  g_return_if_fail (GTK_IS_ACTION_GROUP (action_group));

  translate_func = action_group->private_data->translate_func;
  translate_data = action_group->private_data->translate_data;

  for (i = 0; i < n_entries; i++)
    {
      GtkAction *action;
      GType action_type;
      gchar *accel_path;
      gchar *label;
      gchar *tooltip;

      if (entries[i].is_toggle)
	action_type = GTK_TYPE_TOGGLE_ACTION;
      else
	action_type = GTK_TYPE_ACTION;

      if (translate_func)
	{
	  label = translate_func (entries[i].label, translate_data);
	  tooltip = translate_func (entries[i].tooltip, translate_data);
	}
      else
	{
	  label = entries[i].label;
	  tooltip = entries[i].tooltip;
	}

      action = g_object_new (action_type,
			     "name", entries[i].name,
			     "label", label,
			     "tooltip", tooltip,
			     "stock_id", entries[i].stock_id,
			     NULL);

      if (entries[i].callback)
	g_signal_connect_data (action, "activate",
			       entries[i].callback, 
			       user_data, (GClosureNotify)destroy, 0);

      /* set the accel path for the menu item */
      accel_path = g_strconcat ("<Actions>/", action_group->private_data->name, "/",
				entries[i].name, NULL);
      if (entries[i].accelerator)
	{
	  guint accel_key = 0;
	  GdkModifierType accel_mods;

	  gtk_accelerator_parse (entries[i].accelerator, &accel_key,
				 &accel_mods);
	  if (accel_key)
	    gtk_accel_map_add_entry (accel_path, accel_key, accel_mods);
	}

      gtk_action_set_accel_path (action, accel_path);
      g_free (accel_path);

      gtk_action_group_add_action (action_group, action);
      g_object_unref (action);
    }
}

/**
 * gtk_action_group_add_radio_actions:
 * @action_group: the action group
 * @entries: an array of radio action descriptions
 * @n_entries: the number of entries
 * @on_change: the callback to connect to the changed signal
 * @user_data: data to pass to the action callbacks
 * 
 * This is a convenience routine to create a group of radio actions and 
 * add them to the action group.  Each member of the array describes a
 * radio action to create.
 * 
 * Since: 2.4
 **/
void            
gtk_action_group_add_radio_actions (GtkActionGroup      *action_group,
				    GtkRadioActionEntry *entries,
				    guint                n_entries,
				    GCallback            on_change,
				    gpointer             user_data)
{
  gtk_action_group_add_radio_actions_full (action_group, 
					   entries, n_entries, 
					   on_change, user_data, NULL);
}

/**
 * gtk_action_group_add_radio_actions_full:
 * @action_group: the action group
 * @entries: an array of radio action descriptions
 * @n_entries: the number of entries
 * @on_change: the callback to connect to the changed signal
 * @user_data: data to pass to the action callbacks
 * @destroy: destroy notification callback for @user_data
 *
 * This variant of gtk_action_group_add_radio_actions() adds a 
 * #GDestroyNotify callback for @user_data. 
 * 
 * Since: 2.4
 **/
void            
gtk_action_group_add_radio_actions_full (GtkActionGroup      *action_group,
					 GtkRadioActionEntry *entries,
					 guint                n_entries,
					 GCallback            on_change,
					 gpointer             user_data,
					 GDestroyNotify       destroy)
{
  guint i;
  GtkTranslateFunc translate_func;
  gpointer translate_data;
  GSList *group = NULL;

  g_return_if_fail (GTK_IS_ACTION_GROUP (action_group));

  translate_func = action_group->private_data->translate_func;
  translate_data = action_group->private_data->translate_data;

  for (i = 0; i < n_entries; i++)
    {
      GtkAction *action;
      gchar *accel_path;
      gchar *label;
      gchar *tooltip; 

      if (translate_func)
	{
	  label = translate_func (entries[i].label, translate_data);
	  tooltip = translate_func (entries[i].tooltip, translate_data);
	}
      else
	{
	  label = entries[i].label;
	  tooltip = entries[i].tooltip;
	}

      action = g_object_new (GTK_TYPE_RADIO_ACTION,
			     "name", entries[i].name,
			     "label", label,
			     "tooltip", tooltip,
			     "stock_id", entries[i].stock_id,
			     "value", entries[i].value,
			     NULL);
      
      if (i == 0) 
	{
	  if (on_change)
	    g_signal_connect_data (action, "changed",
				   on_change, user_data, 
				   (GClosureNotify)destroy, 0);
	}
      gtk_radio_action_set_group (GTK_RADIO_ACTION (action), group);
      group = gtk_radio_action_get_group (GTK_RADIO_ACTION (action));

      /* set the accel path for the menu item */
      accel_path = g_strconcat ("<Actions>/", action_group->private_data->name, "/",
				entries[i].name, NULL);
      if (entries[i].accelerator)
	{
	  guint accel_key = 0;
	  GdkModifierType accel_mods;

	  gtk_accelerator_parse (entries[i].accelerator, &accel_key,
				 &accel_mods);
	  if (accel_key)
	    gtk_accel_map_add_entry (accel_path, accel_key, accel_mods);
	}

      gtk_action_set_accel_path (action, accel_path);
      g_free (accel_path);
      
      gtk_action_group_add_action (action_group, action);
      g_object_unref (action);
    }
}

/**
 * gtk_action_group_set_translate_func:
 * @action_group: a #GtkActionGroup
 * @func: a #GtkTranslateFunc
 * @data: data to be passed to @func and @notify
 * @notify: a #GtkDestroyNotify function to be called when @action_group is 
 *   destroyed and when the translation function is changed again
 * 
 * Sets a function to be used for translating the @label and @tooltip of 
 * #GtkActionGroupEntry<!-- -->s added by gtk_action_group_add_actions().
 *
 * If you're using gettext(), it is enough to set the translation domain
 * with gtk_action_group_set_translation_domain().
 *
 * Since: 2.4 
 **/
void
gtk_action_group_set_translate_func (GtkActionGroup      *action_group,
				     GtkTranslateFunc     func,
				     gpointer             data,
				     GtkDestroyNotify     notify)
{
  g_return_if_fail (GTK_IS_ACTION_GROUP (action_group));
  
  if (action_group->private_data->translate_notify)
    action_group->private_data->translate_notify (action_group->private_data->translate_data);
      
  action_group->private_data->translate_func = func;
  action_group->private_data->translate_data = data;
  action_group->private_data->translate_notify = notify;
}

static gchar *
dgettext_swapped (const gchar *msgid, 
		  const gchar *domainname)
{
  return dgettext (domainname, msgid);
}

/**
 * gtk_action_group_set_translation_domain:
 * @action_group: a #GtkActionGroup
 * @domain: the translation domain to use for dgettext() calls
 * 
 * Sets the translation domain and uses dgettext() for translating the 
 * @label and @tooltip of #GtkActionEntry<!-- -->s added by 
 * gtk_action_group_add_actions().
 *
 * If you're not using gettext() for localization, see 
 * gtk_action_group_set_translate_func().
 *
 * Since: 2.4
 **/
void 
gtk_action_group_set_translation_domain (GtkActionGroup *action_group,
					 const gchar    *domain)
{
  g_return_if_fail (GTK_IS_ACTION_GROUP (action_group));

  gtk_action_group_set_translate_func (action_group, 
				       (GtkTranslateFunc)dgettext_swapped,
				       g_strdup (domain),
				       g_free);
} 
