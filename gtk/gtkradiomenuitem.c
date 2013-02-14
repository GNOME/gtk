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
#include "gtkaccellabel.h"
#include "gtkmarshalers.h"
#include "gtkradiomenuitem.h"
#include "gtkactivatable.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "a11y/gtkradiomenuitemaccessible.h"

/**
 * SECTION:gtkradiomenuitem
 * @Short_description: A choice from multiple check menu items
 * @Title: GtkRadioMenuItem
 * @See_also: #GtkMenuItem, #GtkCheckMenuItem
 *
 * A radio menu item is a check menu item that belongs to a group. At each
 * instant exactly one of the radio menu items from a group is selected.
 *
 * The group list does not need to be freed, as each #GtkRadioMenuItem will
 * remove itself and its list item when it is destroyed.
 *
 * The correct way to create a group of radio menu items is approximatively
 * this:
 *
 * <example>
 * <title>How to create a group of radio menu items.</title>
 * <programlisting>
 * GSList *group = NULL;
 * GtkWidget *item;
 * gint i;
 *
 * for (i = 0; i < 5; i++)
 * {
 *   item = gtk_radio_menu_item_new_with_label (group, "This is an example");
 *   group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
 *   if (i == 1)
 *     gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
 * }
 * </programlisting>
 * </example>
 */


struct _GtkRadioMenuItemPrivate
{
  GSList *group;
};

enum {
  PROP_0,
  PROP_GROUP
};


static void gtk_radio_menu_item_destroy        (GtkWidget             *widget);
static void gtk_radio_menu_item_activate       (GtkMenuItem           *menu_item);
static void gtk_radio_menu_item_set_property   (GObject               *object,
						guint                  prop_id,
						const GValue          *value,
						GParamSpec            *pspec);
static void gtk_radio_menu_item_get_property   (GObject               *object,
						guint                  prop_id,
						GValue                *value,
						GParamSpec            *pspec);

static guint group_changed_signal = 0;

G_DEFINE_TYPE (GtkRadioMenuItem, gtk_radio_menu_item, GTK_TYPE_CHECK_MENU_ITEM)

/**
 * gtk_radio_menu_item_new:
 * @group: (element-type GtkRadioMenuItem): the group to which the
 *    radio menu item is to be attached
 *
 * Creates a new #GtkRadioMenuItem.
 *
 * Returns: a new #GtkRadioMenuItem
 */
GtkWidget*
gtk_radio_menu_item_new (GSList *group)
{
  GtkRadioMenuItem *radio_menu_item;

  radio_menu_item = g_object_new (GTK_TYPE_RADIO_MENU_ITEM, NULL);

  gtk_radio_menu_item_set_group (radio_menu_item, group);

  return GTK_WIDGET (radio_menu_item);
}

static void
gtk_radio_menu_item_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  GtkRadioMenuItem *radio_menu_item;

  radio_menu_item = GTK_RADIO_MENU_ITEM (object);

  switch (prop_id)
    {
      GSList *slist;

    case PROP_GROUP:
      slist = g_value_get_object (value);
      if (slist)
        slist = gtk_radio_menu_item_get_group ((GtkRadioMenuItem*) g_value_get_object (value));
      gtk_radio_menu_item_set_group (radio_menu_item, slist);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_radio_menu_item_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_radio_menu_item_set_group:
 * @radio_menu_item: a #GtkRadioMenuItem.
 * @group: (element-type GtkRadioMenuItem): the new group.
 *
 * Sets the group of a radio menu item, or changes it.
 */
void
gtk_radio_menu_item_set_group (GtkRadioMenuItem *radio_menu_item,
			       GSList           *group)
{
  GtkRadioMenuItemPrivate *priv;
  GtkWidget *old_group_singleton = NULL;
  GtkWidget *new_group_singleton = NULL;
  
  g_return_if_fail (GTK_IS_RADIO_MENU_ITEM (radio_menu_item));
  g_return_if_fail (!g_slist_find (group, radio_menu_item));

  priv = radio_menu_item->priv;

  if (priv->group)
    {
      GSList *slist;

      priv->group = g_slist_remove (priv->group, radio_menu_item);

      if (priv->group && !priv->group->next)
	old_group_singleton = g_object_ref (priv->group->data);

      for (slist = priv->group; slist; slist = slist->next)
	{
	  GtkRadioMenuItem *tmp_item;
	  
	  tmp_item = slist->data;

	  tmp_item->priv->group = priv->group;
	}
    }
  
  if (group && !group->next)
    new_group_singleton = g_object_ref (group->data);

  priv->group = g_slist_prepend (group, radio_menu_item);

  if (group)
    {
      GSList *slist;
      
      for (slist = group; slist; slist = slist->next)
	{
	  GtkRadioMenuItem *tmp_item;
	  
	  tmp_item = slist->data;

	  tmp_item->priv->group = priv->group;
	}
    }
  else
    {
      _gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (radio_menu_item), TRUE);
      /* gtk_widget_set_state (GTK_WIDGET (radio_menu_item), GTK_STATE_ACTIVE);
       */
    }

  g_object_ref (radio_menu_item);

  g_object_notify (G_OBJECT (radio_menu_item), "group");
  g_signal_emit (radio_menu_item, group_changed_signal, 0);
  if (old_group_singleton)
    {
      g_signal_emit (old_group_singleton, group_changed_signal, 0);
      g_object_unref (old_group_singleton);
    }
  if (new_group_singleton)
    {
      g_signal_emit (new_group_singleton, group_changed_signal, 0);
      g_object_unref (new_group_singleton);
    }

  g_object_unref (radio_menu_item);
}


/**
 * gtk_radio_menu_item_new_with_label:
 * @group: (element-type GtkRadioMenuItem) (transfer full):
 * @label: the text for the label
 *
 * Creates a new #GtkRadioMenuItem whose child is a simple #GtkLabel.
 *
 * Returns: (transfer none): A new #GtkRadioMenuItem
 */
GtkWidget*
gtk_radio_menu_item_new_with_label (GSList *group,
				    const gchar *label)
{
  return g_object_new (GTK_TYPE_RADIO_MENU_ITEM,
          "group", (group) ? group->data : NULL,
          "label", label,
          NULL);
}


/**
 * gtk_radio_menu_item_new_with_mnemonic:
 * @group: (element-type GtkRadioMenuItem): group the radio menu item is inside
 * @label: the text of the button, with an underscore in front of the
 *         mnemonic character
 *
 * Creates a new #GtkRadioMenuItem containing a label. The label
 * will be created using gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the menu item.
 *
 * Returns: a new #GtkRadioMenuItem
 */
GtkWidget*
gtk_radio_menu_item_new_with_mnemonic (GSList *group,
				       const gchar *label)
{
  return g_object_new (GTK_TYPE_RADIO_MENU_ITEM,
          "group", (group) ? group->data : NULL,
          "label", label,
          "use-underline", TRUE,
          NULL);
}

/**
 * gtk_radio_menu_item_new_from_widget: (constructor)
 * @group: An existing #GtkRadioMenuItem
 *
 * Creates a new #GtkRadioMenuItem adding it to the same group as @group.
 *
 * Return value: (transfer none): The new #GtkRadioMenuItem
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_radio_menu_item_new_from_widget (GtkRadioMenuItem *group)
{
  GSList *list = NULL;
  
  g_return_val_if_fail (GTK_IS_RADIO_MENU_ITEM (group), NULL);

  if (group)
    list = gtk_radio_menu_item_get_group (group);
  
  return gtk_radio_menu_item_new (list);
}

/**
 * gtk_radio_menu_item_new_with_mnemonic_from_widget: (constructor)
 * @group: An existing #GtkRadioMenuItem
 * @label: the text of the button, with an underscore in front of the
 *         mnemonic character
 *
 * Creates a new GtkRadioMenuItem containing a label. The label will be
 * created using gtk_label_new_with_mnemonic(), so underscores in label
 * indicate the mnemonic for the menu item.
 *
 * The new #GtkRadioMenuItem is added to the same group as @group.
 *
 * Return value: (transfer none): The new #GtkRadioMenuItem
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_radio_menu_item_new_with_mnemonic_from_widget (GtkRadioMenuItem *group,
						   const gchar *label)
{
  GSList *list = NULL;

  g_return_val_if_fail (GTK_IS_RADIO_MENU_ITEM (group), NULL);

  if (group)
    list = gtk_radio_menu_item_get_group (group);

  return gtk_radio_menu_item_new_with_mnemonic (list, label);
}

/**
 * gtk_radio_menu_item_new_with_label_from_widget: (constructor)
 * @group: an existing #GtkRadioMenuItem
 * @label: the text for the label
 *
 * Creates a new GtkRadioMenuItem whose child is a simple GtkLabel.
 * The new #GtkRadioMenuItem is added to the same group as @group.
 *
 * Return value: (transfer none): The new #GtkRadioMenuItem
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_radio_menu_item_new_with_label_from_widget (GtkRadioMenuItem *group,
						const gchar *label)
{
  GSList *list = NULL;

  g_return_val_if_fail (GTK_IS_RADIO_MENU_ITEM (group), NULL);

  if (group)
    list = gtk_radio_menu_item_get_group (group);

  return gtk_radio_menu_item_new_with_label (list, label);
}

/**
 * gtk_radio_menu_item_get_group:
 * @radio_menu_item: a #GtkRadioMenuItem
 *
 * Returns the group to which the radio menu item belongs, as a #GList of
 * #GtkRadioMenuItem. The list belongs to GTK+ and should not be freed.
 *
 * Returns: (element-type GtkRadioMenuItem) (transfer none): the group
 *     of @radio_menu_item
 */
GSList*
gtk_radio_menu_item_get_group (GtkRadioMenuItem *radio_menu_item)
{
  g_return_val_if_fail (GTK_IS_RADIO_MENU_ITEM (radio_menu_item), NULL);

  return radio_menu_item->priv->group;
}

static void
gtk_radio_menu_item_class_init (GtkRadioMenuItemClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkMenuItemClass *menu_item_class;

  gobject_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);
  menu_item_class = GTK_MENU_ITEM_CLASS (klass);

  gobject_class->set_property = gtk_radio_menu_item_set_property;
  gobject_class->get_property = gtk_radio_menu_item_get_property;

  widget_class->destroy = gtk_radio_menu_item_destroy;

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_RADIO_MENU_ITEM_ACCESSIBLE);

  menu_item_class->activate = gtk_radio_menu_item_activate;

  /**
   * GtkRadioMenuItem:group:
   * 
   * The radio menu item whose group this widget belongs to.
   * 
   * Since: 2.8
   */
  g_object_class_install_property (gobject_class,
				   PROP_GROUP,
				   g_param_spec_object ("group",
							P_("Group"),
							P_("The radio menu item whose group this widget belongs to."),
							GTK_TYPE_RADIO_MENU_ITEM,
							GTK_PARAM_WRITABLE));

  /**
   * GtkStyle::group-changed:
   * @style: the object which received the signal
   *
   * Emitted when the group of radio menu items that a radio menu item belongs
   * to changes. This is emitted when a radio menu item switches from
   * being alone to being part of a group of 2 or more menu items, or
   * vice-versa, and when a button is moved from one group of 2 or
   * more menu items ton a different one, but not when the composition
   * of the group that a menu item belongs to changes.
   *
   * Since: 2.4
   */
  group_changed_signal = g_signal_new (I_("group-changed"),
				       G_OBJECT_CLASS_TYPE (gobject_class),
				       G_SIGNAL_RUN_FIRST,
				       G_STRUCT_OFFSET (GtkRadioMenuItemClass, group_changed),
				       NULL, NULL,
				       _gtk_marshal_VOID__VOID,
				       G_TYPE_NONE, 0);

  g_type_class_add_private (klass, sizeof (GtkRadioMenuItemPrivate));
}

static void
gtk_radio_menu_item_init (GtkRadioMenuItem *radio_menu_item)
{
  GtkRadioMenuItemPrivate *priv;

  radio_menu_item->priv = G_TYPE_INSTANCE_GET_PRIVATE (radio_menu_item,
                                                       GTK_TYPE_RADIO_MENU_ITEM,
                                                       GtkRadioMenuItemPrivate);
  priv = radio_menu_item->priv;

  priv->group = g_slist_prepend (NULL, radio_menu_item);
  gtk_check_menu_item_set_draw_as_radio (GTK_CHECK_MENU_ITEM (radio_menu_item), TRUE);
}

static void
gtk_radio_menu_item_destroy (GtkWidget *widget)
{
  GtkRadioMenuItem *radio_menu_item = GTK_RADIO_MENU_ITEM (widget);
  GtkRadioMenuItemPrivate *priv = radio_menu_item->priv;
  GtkWidget *old_group_singleton = NULL;
  GtkRadioMenuItem *tmp_menu_item;
  GSList *tmp_list;
  gboolean was_in_group;

  was_in_group = priv->group && priv->group->next;

  priv->group = g_slist_remove (priv->group, radio_menu_item);
  if (priv->group && !priv->group->next)
    old_group_singleton = priv->group->data;

  tmp_list = priv->group;

  while (tmp_list)
    {
      tmp_menu_item = tmp_list->data;
      tmp_list = tmp_list->next;

      tmp_menu_item->priv->group = priv->group;
    }

  /* this radio menu item is no longer in the group */
  priv->group = NULL;
  
  if (old_group_singleton)
    g_signal_emit (old_group_singleton, group_changed_signal, 0);
  if (was_in_group)
    g_signal_emit (radio_menu_item, group_changed_signal, 0);

  GTK_WIDGET_CLASS (gtk_radio_menu_item_parent_class)->destroy (widget);
}

static void
gtk_radio_menu_item_activate (GtkMenuItem *menu_item)
{
  GtkRadioMenuItem *radio_menu_item = GTK_RADIO_MENU_ITEM (menu_item);
  GtkRadioMenuItemPrivate *priv = radio_menu_item->priv;
  GtkCheckMenuItem *check_menu_item = GTK_CHECK_MENU_ITEM (menu_item);
  GtkCheckMenuItem *tmp_menu_item;
  GtkAction        *action;
  GSList *tmp_list;
  gboolean active;
  gint toggled;

  action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (menu_item));
  if (action && gtk_menu_item_get_submenu (menu_item) == NULL)
    gtk_action_activate (action);

  toggled = FALSE;

  active = gtk_check_menu_item_get_active (check_menu_item);
  if (active)
    {
      tmp_menu_item = NULL;
      tmp_list = priv->group;

      while (tmp_list)
	{
	  tmp_menu_item = tmp_list->data;
	  tmp_list = tmp_list->next;

          if (gtk_check_menu_item_get_active (tmp_menu_item) &&
              tmp_menu_item != check_menu_item)
	    break;

	  tmp_menu_item = NULL;
	}

      if (tmp_menu_item)
	{
	  toggled = TRUE;
          _gtk_check_menu_item_set_active (check_menu_item, !active);
	}
    }
  else
    {
      toggled = TRUE;
      _gtk_check_menu_item_set_active (check_menu_item, !active);

      tmp_list = priv->group;
      while (tmp_list)
	{
	  tmp_menu_item = tmp_list->data;
	  tmp_list = tmp_list->next;

          if (gtk_check_menu_item_get_active (tmp_menu_item) &&
              tmp_menu_item != check_menu_item)
	    {
              gtk_menu_item_activate (GTK_MENU_ITEM (tmp_menu_item));
	      break;
	    }
	}
    }

  if (toggled)
    {
      gtk_check_menu_item_toggled (check_menu_item);
    }

  gtk_widget_queue_draw (GTK_WIDGET (radio_menu_item));
}
