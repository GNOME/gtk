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

#include "gtkradiobutton.h"

#include "gtkbuttonprivate.h"
#include "gtktogglebuttonprivate.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "a11y/gtkradiobuttonaccessible.h"

/**
 * SECTION:gtkradiobutton
 * @Short_description: A choice from multiple check buttons
 * @Title: GtkRadioButton
 * @See_also: #GtkComboBox
 *
 * A single radio button performs the same basic function as a #GtkCheckButton,
 * as its position in the object hierarchy reflects. It is only when multiple
 * radio buttons are grouped together that they become a different user
 * interface component in their own right.
 *
 * Every radio button is a member of some group of radio buttons. When one is
 * selected, all other radio buttons in the same group are deselected. A
 * #GtkRadioButton is one way of giving the user a choice from many options.
 *
 * Radio button widgets are created with gtk_radio_button_new(), passing %NULL
 * as the argument if this is the first radio button in a group. In subsequent
 * calls, the group you wish to add this button to should be passed as an
 * argument. Optionally, gtk_radio_button_new_with_label() can be used if you
 * want a text label on the radio button.
 *
 * Alternatively, when adding widgets to an existing group of radio buttons,
 * use gtk_radio_button_new_from_widget() with a #GtkRadioButton that already
 * has a group assigned to it. The convenience function
 * gtk_radio_button_new_with_label_from_widget() is also provided.
 *
 * To retrieve the group a #GtkRadioButton is assigned to, use
 * gtk_radio_button_get_group().
 *
 * To remove a #GtkRadioButton from one group and make it part of a new one,
 * use gtk_radio_button_set_group().
 *
 * The group list does not need to be freed, as each #GtkRadioButton will remove
 * itself and its list item when it is destroyed.
 *
 * ## How to create a group of two radio buttons.
 *
 * |[<!-- language="C" -->
 * void create_radio_buttons (void) {
 *
 *    GtkWidget *window, *radio1, *radio2, *box, *entry;
 *    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
 *    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
 *    gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
 *
 *    // Create a radio button with a GtkEntry widget
 *    radio1 = gtk_radio_button_new (NULL);
 *    entry = gtk_entry_new ();
 *    gtk_container_add (GTK_CONTAINER (radio1), entry);
 *
 *
 *    // Create a radio button with a label
 *    radio2 = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio1),
 *                                                          "I’m the second radio button.");
 *
 *    // Pack them into a box, then show all the widgets
 *    gtk_box_pack_start (GTK_BOX (box), radio1, TRUE, TRUE, 2);
 *    gtk_box_pack_start (GTK_BOX (box), radio2, TRUE, TRUE, 2);
 *    gtk_container_add (GTK_CONTAINER (window), box);
 *    gtk_widget_show_all (window);
 *    return;
 * }
 * ]|
 *
 * When an unselected button in the group is clicked the clicked button
 * receives the #GtkToggleButton::toggled signal, as does the previously
 * selected button.
 * Inside the #GtkToggleButton::toggled handler, gtk_toggle_button_get_active()
 * can be used to determine if the button has been selected or deselected.
 */


struct _GtkRadioButtonPrivate
{
  GSList *group;
};

enum {
  PROP_0,
  PROP_GROUP
};


static void     gtk_radio_button_destroy        (GtkWidget           *widget);
static gboolean gtk_radio_button_focus          (GtkWidget           *widget,
						 GtkDirectionType     direction);
static void     gtk_radio_button_clicked        (GtkButton           *button);
static void     gtk_radio_button_draw_indicator (GtkCheckButton      *check_button,
						 cairo_t             *cr);
static void     gtk_radio_button_set_property   (GObject             *object,
						 guint                prop_id,
						 const GValue        *value,
						 GParamSpec          *pspec);
static void     gtk_radio_button_get_property   (GObject             *object,
						 guint                prop_id,
						 GValue              *value,
						 GParamSpec          *pspec);

G_DEFINE_TYPE_WITH_PRIVATE (GtkRadioButton, gtk_radio_button, GTK_TYPE_CHECK_BUTTON)

static guint group_changed_signal = 0;

static void
gtk_radio_button_class_init (GtkRadioButtonClass *class)
{
  GObjectClass *gobject_class;
  GtkButtonClass *button_class;
  GtkCheckButtonClass *check_button_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;
  button_class = (GtkButtonClass*) class;
  check_button_class = (GtkCheckButtonClass*) class;

  gobject_class->set_property = gtk_radio_button_set_property;
  gobject_class->get_property = gtk_radio_button_get_property;

  /**
   * GtkRadioButton:group:
   *
   * Sets a new group for a radio button.
   */
  g_object_class_install_property (gobject_class,
				   PROP_GROUP,
				   g_param_spec_object ("group",
							P_("Group"),
							P_("The radio button whose group this widget belongs to."),
							GTK_TYPE_RADIO_BUTTON,
							GTK_PARAM_WRITABLE));
  widget_class->destroy = gtk_radio_button_destroy;
  widget_class->focus = gtk_radio_button_focus;

  button_class->clicked = gtk_radio_button_clicked;

  check_button_class->draw_indicator = gtk_radio_button_draw_indicator;

  class->group_changed = NULL;

  /**
   * GtkRadioButton::group-changed:
   * @button: the object which received the signal
   *
   * Emitted when the group of radio buttons that a radio button belongs
   * to changes. This is emitted when a radio button switches from
   * being alone to being part of a group of 2 or more buttons, or
   * vice-versa, and when a button is moved from one group of 2 or
   * more buttons to a different one, but not when the composition
   * of the group that a button belongs to changes.
   *
   * Since: 2.4
   */
  group_changed_signal = g_signal_new (I_("group-changed"),
				       G_OBJECT_CLASS_TYPE (gobject_class),
				       G_SIGNAL_RUN_FIRST,
				       G_STRUCT_OFFSET (GtkRadioButtonClass, group_changed),
				       NULL, NULL,
				       _gtk_marshal_VOID__VOID,
				       G_TYPE_NONE, 0);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_RADIO_BUTTON_ACCESSIBLE);
}

static void
gtk_radio_button_init (GtkRadioButton *radio_button)
{
  GtkRadioButtonPrivate *priv;

  radio_button->priv = gtk_radio_button_get_instance_private (radio_button);
  priv = radio_button->priv;

  gtk_widget_set_receives_default (GTK_WIDGET (radio_button), FALSE);

  _gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), TRUE);

  priv->group = g_slist_prepend (NULL, radio_button);
}

static void
gtk_radio_button_set_property (GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
  GtkRadioButton *radio_button;

  radio_button = GTK_RADIO_BUTTON (object);

  switch (prop_id)
    {
      GSList *slist;
      GtkRadioButton *button;

    case PROP_GROUP:
        button = g_value_get_object (value);

      if (button)
	slist = gtk_radio_button_get_group (button);
      else
	slist = NULL;
      gtk_radio_button_set_group (radio_button, slist);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_radio_button_get_property (GObject    *object,
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
 * gtk_radio_button_set_group:
 * @radio_button: a #GtkRadioButton.
 * @group: (element-type GtkRadioButton) (allow-none): an existing radio
 *     button group, such as one returned from gtk_radio_button_get_group(), or %NULL.
 *
 * Sets a #GtkRadioButton’s group. It should be noted that this does not change
 * the layout of your interface in any way, so if you are changing the group,
 * it is likely you will need to re-arrange the user interface to reflect these
 * changes.
 */
void
gtk_radio_button_set_group (GtkRadioButton *radio_button,
			    GSList         *group)
{
  GtkRadioButtonPrivate *priv;
  GtkWidget *old_group_singleton = NULL;
  GtkWidget *new_group_singleton = NULL;

  g_return_if_fail (GTK_IS_RADIO_BUTTON (radio_button));

  if (g_slist_find (group, radio_button))
    return;

  priv = radio_button->priv;

  if (priv->group)
    {
      GSList *slist;

      priv->group = g_slist_remove (priv->group, radio_button);

      if (priv->group && !priv->group->next)
	old_group_singleton = g_object_ref (priv->group->data);

      for (slist = priv->group; slist; slist = slist->next)
	{
	  GtkRadioButton *tmp_button;
	  
	  tmp_button = slist->data;

	  tmp_button->priv->group = priv->group;
	}
    }
  
  if (group && !group->next)
    new_group_singleton = g_object_ref (group->data);

  priv->group = g_slist_prepend (group, radio_button);

  if (group)
    {
      GSList *slist;
      
      for (slist = group; slist; slist = slist->next)
	{
	  GtkRadioButton *tmp_button;
	  
	  tmp_button = slist->data;

	  tmp_button->priv->group = priv->group;
	}
    }

  g_object_ref (radio_button);
  
  g_object_notify (G_OBJECT (radio_button), "group");
  g_signal_emit (radio_button, group_changed_signal, 0);
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

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), group == NULL);

  g_object_unref (radio_button);
}

/**
 * gtk_radio_button_join_group:
 * @radio_button: the #GtkRadioButton object
 * @group_source: (allow-none): a radio button object whos group we are 
 *   joining, or %NULL to remove the radio button from its group
 *
 * Joins a #GtkRadioButton object to the group of another #GtkRadioButton object
 *
 * Use this in language bindings instead of the gtk_radio_button_get_group() 
 * and gtk_radio_button_set_group() methods
 *
 * A common way to set up a group of radio buttons is the following:
 * |[<!-- language="C" -->
 *   GtkRadioButton *radio_button;
 *   GtkRadioButton *last_button;
 *
 *   while ( ...more buttons to add... )
 *     {
 *        radio_button = gtk_radio_button_new (...);
 *
 *        gtk_radio_button_join_group (radio_button, last_button);
 *        last_button = radio_button;
 *     }
 * ]|
 *
 * Since: 3.0
 */
void
gtk_radio_button_join_group (GtkRadioButton *radio_button, 
			     GtkRadioButton *group_source)
{
  g_return_if_fail (GTK_IS_RADIO_BUTTON (radio_button));
  g_return_if_fail (group_source == NULL || GTK_IS_RADIO_BUTTON (group_source));

  if (group_source)
    {
      GSList *group;
      group = gtk_radio_button_get_group (group_source);

      if (!group)
        {
          /* if we are not already part of a group we need to set up a new one
             and then get the newly created group */
          gtk_radio_button_set_group (group_source, NULL);
          group = gtk_radio_button_get_group (group_source);
        }

      gtk_radio_button_set_group (radio_button, group);
    }
  else
    {
      gtk_radio_button_set_group (radio_button, NULL);
    }
}

/**
 * gtk_radio_button_new:
 * @group: (element-type GtkRadioButton) (allow-none): an existing
 *         radio button group, or %NULL if you are creating a new group.
 *
 * Creates a new #GtkRadioButton. To be of any practical value, a widget should
 * then be packed into the radio button.
 *
 * Returns: a new radio button
 */
GtkWidget*
gtk_radio_button_new (GSList *group)
{
  GtkRadioButton *radio_button;

  radio_button = g_object_new (GTK_TYPE_RADIO_BUTTON, NULL);

  if (group)
    gtk_radio_button_set_group (radio_button, group);

  return GTK_WIDGET (radio_button);
}

/**
 * gtk_radio_button_new_with_label:
 * @group: (element-type GtkRadioButton) (allow-none): an existing
 *         radio button group, or %NULL if you are creating a new group.
 * @label: the text label to display next to the radio button.
 *
 * Creates a new #GtkRadioButton with a text label.
 *
 * Returns: a new radio button.
 */
GtkWidget*
gtk_radio_button_new_with_label (GSList      *group,
				 const gchar *label)
{
  GtkWidget *radio_button;

  radio_button = g_object_new (GTK_TYPE_RADIO_BUTTON, "label", label, NULL) ;

  if (group)
    gtk_radio_button_set_group (GTK_RADIO_BUTTON (radio_button), group);

  return radio_button;
}


/**
 * gtk_radio_button_new_with_mnemonic:
 * @group: (element-type GtkRadioButton) (allow-none): the radio button
 *         group, or %NULL
 * @label: the text of the button, with an underscore in front of the
 *         mnemonic character
 *
 * Creates a new #GtkRadioButton containing a label, adding it to the same
 * group as @group. The label will be created using
 * gtk_label_new_with_mnemonic(), so underscores in @label indicate the
 * mnemonic for the button.
 *
 * Returns: a new #GtkRadioButton
 */
GtkWidget*
gtk_radio_button_new_with_mnemonic (GSList      *group,
				    const gchar *label)
{
  GtkWidget *radio_button;

  radio_button = g_object_new (GTK_TYPE_RADIO_BUTTON, 
			       "label", label, 
			       "use-underline", TRUE, 
			       NULL);

  if (group)
    gtk_radio_button_set_group (GTK_RADIO_BUTTON (radio_button), group);

  return radio_button;
}

/**
 * gtk_radio_button_new_from_widget: (constructor)
 * @radio_group_member: (allow-none): an existing #GtkRadioButton.
 *
 * Creates a new #GtkRadioButton, adding it to the same group as
 * @radio_group_member. As with gtk_radio_button_new(), a widget
 * should be packed into the radio button.
 *
 * Returns: (transfer none): a new radio button.
 */
GtkWidget*
gtk_radio_button_new_from_widget (GtkRadioButton *radio_group_member)
{
  GSList *l = NULL;
  if (radio_group_member)
    l = gtk_radio_button_get_group (radio_group_member);
  return gtk_radio_button_new (l);
}

/**
 * gtk_radio_button_new_with_label_from_widget: (constructor)
 * @radio_group_member: (allow-none): widget to get radio group from or %NULL
 * @label: a text string to display next to the radio button.
 *
 * Creates a new #GtkRadioButton with a text label, adding it to
 * the same group as @radio_group_member.
 *
 * Returns: (transfer none): a new radio button.
 */
GtkWidget*
gtk_radio_button_new_with_label_from_widget (GtkRadioButton *radio_group_member,
					     const gchar    *label)
{
  GSList *l = NULL;
  if (radio_group_member)
    l = gtk_radio_button_get_group (radio_group_member);
  return gtk_radio_button_new_with_label (l, label);
}

/**
 * gtk_radio_button_new_with_mnemonic_from_widget: (constructor)
 * @radio_group_member: (allow-none): widget to get radio group from or %NULL
 * @label: the text of the button, with an underscore in front of the
 *         mnemonic character
 *
 * Creates a new #GtkRadioButton containing a label. The label
 * will be created using gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the button.
 *
 * Returns: (transfer none): a new #GtkRadioButton
 **/
GtkWidget*
gtk_radio_button_new_with_mnemonic_from_widget (GtkRadioButton *radio_group_member,
					        const gchar    *label)
{
  GSList *l = NULL;
  if (radio_group_member)
    l = gtk_radio_button_get_group (radio_group_member);
  return gtk_radio_button_new_with_mnemonic (l, label);
}


/**
 * gtk_radio_button_get_group:
 * @radio_button: a #GtkRadioButton.
 *
 * Retrieves the group assigned to a radio button.
 *
 * Returns: (element-type GtkRadioButton) (transfer none): a linked list
 * containing all the radio buttons in the same group
 * as @radio_button. The returned list is owned by the radio button
 * and must not be modified or freed.
 */
GSList*
gtk_radio_button_get_group (GtkRadioButton *radio_button)
{
  g_return_val_if_fail (GTK_IS_RADIO_BUTTON (radio_button), NULL);

  return radio_button->priv->group;
}


static void
gtk_radio_button_destroy (GtkWidget *widget)
{
  GtkWidget *old_group_singleton = NULL;
  GtkRadioButton *radio_button = GTK_RADIO_BUTTON (widget);
  GtkRadioButtonPrivate *priv = radio_button->priv;
  GtkRadioButton *tmp_button;
  GSList *tmp_list;
  gboolean was_in_group;

  was_in_group = priv->group && priv->group->next;

  priv->group = g_slist_remove (priv->group, radio_button);
  if (priv->group && !priv->group->next)
    old_group_singleton = priv->group->data;

  tmp_list = priv->group;

  while (tmp_list)
    {
      tmp_button = tmp_list->data;
      tmp_list = tmp_list->next;

      tmp_button->priv->group = priv->group;
    }

  /* this button is no longer in the group */
  priv->group = NULL;

  if (old_group_singleton)
    g_signal_emit (old_group_singleton, group_changed_signal, 0);
  if (was_in_group)
    g_signal_emit (radio_button, group_changed_signal, 0);

  GTK_WIDGET_CLASS (gtk_radio_button_parent_class)->destroy (widget);
}

static void
get_coordinates (GtkWidget    *widget,
		 GtkWidget    *reference,
		 gint         *x,
		 gint         *y)
{
  GtkAllocation allocation;

  gtk_widget_get_allocation (widget, &allocation);
  *x = allocation.x + allocation.width / 2;
  *y = allocation.y + allocation.height / 2;

  gtk_widget_translate_coordinates (widget, reference, *x, *y, x, y);
}

static gint
left_right_compare (gconstpointer a,
		    gconstpointer b,
		    gpointer      data)
{
  gint x1, y1, x2, y2;

  get_coordinates ((GtkWidget *)a, data, &x1, &y1);
  get_coordinates ((GtkWidget *)b, data, &x2, &y2);

  if (y1 == y2)
    return (x1 < x2) ? -1 : ((x1 == x2) ? 0 : 1);
  else
    return (y1 < y2) ? -1 : 1;
}

static gint
up_down_compare (gconstpointer a,
		 gconstpointer b,
		 gpointer      data)
{
  gint x1, y1, x2, y2;
  
  get_coordinates ((GtkWidget *)a, data, &x1, &y1);
  get_coordinates ((GtkWidget *)b, data, &x2, &y2);
  
  if (x1 == x2)
    return (y1 < y2) ? -1 : ((y1 == y2) ? 0 : 1);
  else
    return (x1 < x2) ? -1 : 1;
}

static gboolean
gtk_radio_button_focus (GtkWidget         *widget,
			GtkDirectionType   direction)
{
  GtkRadioButton *radio_button = GTK_RADIO_BUTTON (widget);
  GtkRadioButtonPrivate *priv = radio_button->priv;
  GSList *tmp_slist;

  /* Radio buttons with draw_indicator unset focus "normally", since
   * they look like buttons to the user.
   */
  if (!gtk_toggle_button_get_mode (GTK_TOGGLE_BUTTON (widget)))
    return GTK_WIDGET_CLASS (gtk_radio_button_parent_class)->focus (widget, direction);
  
  if (gtk_widget_is_focus (widget))
    {
      GSList *focus_list, *tmp_list;
      GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
      GtkWidget *new_focus = NULL;

      switch (direction)
	{
	case GTK_DIR_LEFT:
	case GTK_DIR_RIGHT:
	  focus_list = g_slist_copy (priv->group);
	  focus_list = g_slist_sort_with_data (focus_list, left_right_compare, toplevel);
	  break;
	case GTK_DIR_UP:
	case GTK_DIR_DOWN:
	  focus_list = g_slist_copy (priv->group);
	  focus_list = g_slist_sort_with_data (focus_list, up_down_compare, toplevel);
	  break;
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_TAB_BACKWARD:
          /* fall through */
        default:
	  return FALSE;
	}

      if (direction == GTK_DIR_LEFT || direction == GTK_DIR_UP)
	focus_list = g_slist_reverse (focus_list);

      tmp_list = g_slist_find (focus_list, widget);

      if (tmp_list)
	{
	  tmp_list = tmp_list->next;
	  
	  while (tmp_list)
	    {
	      GtkWidget *child = tmp_list->data;
	      
	      if (gtk_widget_get_mapped (child) && gtk_widget_is_sensitive (child))
		{
		  new_focus = child;
		  break;
		}

	      tmp_list = tmp_list->next;
	    }
	}

      if (!new_focus)
	{
	  tmp_list = focus_list;

	  while (tmp_list)
	    {
	      GtkWidget *child = tmp_list->data;
	      
	      if (gtk_widget_get_mapped (child) && gtk_widget_is_sensitive (child))
		{
		  new_focus = child;
		  break;
		}
	      
	      tmp_list = tmp_list->next;
	    }
	}
      
      g_slist_free (focus_list);

      if (new_focus)
	{
	  gtk_widget_grab_focus (new_focus);

          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (new_focus), TRUE);
	}

      return TRUE;
    }
  else
    {
      GtkRadioButton *selected_button = NULL;
      
      /* We accept the focus if, we don't have the focus and
       *  - we are the currently active button in the group
       *  - there is no currently active radio button.
       */
      
      tmp_slist = priv->group;
      while (tmp_slist)
	{
	  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (tmp_slist->data)))
	    selected_button = tmp_slist->data;
	  tmp_slist = tmp_slist->next;
	}
      
      if (selected_button && selected_button != radio_button)
	return FALSE;

      gtk_widget_grab_focus (widget);
      return TRUE;
    }
}

static void
gtk_radio_button_clicked (GtkButton *button)
{
  GtkRadioButton *radio_button = GTK_RADIO_BUTTON (button);
  GtkRadioButtonPrivate *priv = radio_button->priv;
  GtkToggleButton *toggle_button = GTK_TOGGLE_BUTTON (button);
  GtkToggleButton *tmp_button;
  GSList *tmp_list;
  gint toggled;

  toggled = FALSE;

  g_object_ref (GTK_WIDGET (button));

  if (gtk_toggle_button_get_active (toggle_button))
    {
      tmp_button = NULL;
      tmp_list = priv->group;

      while (tmp_list)
	{
	  tmp_button = tmp_list->data;
	  tmp_list = tmp_list->next;

          if (tmp_button != toggle_button &&
              gtk_toggle_button_get_active (tmp_button))
	    break;

	  tmp_button = NULL;
	}

      if (tmp_button)
	{
	  toggled = TRUE;
          _gtk_toggle_button_set_active (toggle_button,
                                         !gtk_toggle_button_get_active (toggle_button));
	}
    }
  else
    {
      toggled = TRUE;
      _gtk_toggle_button_set_active (toggle_button,
                                     !gtk_toggle_button_get_active (toggle_button));

      tmp_list = priv->group;
      while (tmp_list)
	{
	  tmp_button = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (gtk_toggle_button_get_active (tmp_button) && (tmp_button != toggle_button))
	    {
	      gtk_button_clicked (GTK_BUTTON (tmp_button));
	      break;
	    }
	}
    }

  if (toggled)
    {
      gtk_toggle_button_toggled (toggle_button);

      g_object_notify (G_OBJECT (toggle_button), "active");
    }

  gtk_widget_queue_draw (GTK_WIDGET (button));

  g_object_unref (button);
}

static void
gtk_radio_button_draw_indicator (GtkCheckButton *check_button,
				 cairo_t        *cr)
{
  GtkAllocation allocation;
  GtkWidget *widget;
  GtkButton *button;
  GtkStyleContext *context;
  gint x, y;
  gint indicator_size, indicator_spacing;
  gint baseline;
  guint border_width;

  widget = GTK_WIDGET (check_button);
  button = GTK_BUTTON (check_button);
  context = gtk_widget_get_style_context (widget);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
  _gtk_check_button_get_props (check_button, &indicator_size, &indicator_spacing);

  gtk_widget_get_allocation (widget, &allocation);
  baseline = gtk_widget_get_allocated_baseline (widget);

  x = indicator_spacing + border_width;
  if (baseline == -1)
    y = (allocation.height - indicator_size) / 2;
  else
    y = CLAMP (baseline - indicator_size * button->priv->baseline_align,
	       0, allocation.height - indicator_size);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    x = allocation.width - (indicator_size + x);

  gtk_style_context_save (context);

  gtk_render_background (context, cr,
                         border_width, border_width,
                         allocation.width - (2 * border_width),
                         allocation.height - (2 * border_width));

  gtk_style_context_add_class (context, GTK_STYLE_CLASS_RADIO);

  gtk_render_option (context, cr,
                     x, y, indicator_size, indicator_size);

  gtk_style_context_restore (context);
}
