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

#include "gtkwidgetprivate.h"
#include "gtktogglebuttonprivate.h"
#include "gtkcheckbuttonprivate.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "a11y/gtkradiobuttonaccessible.h"
#include "gtkstylecontextprivate.h"

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
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * radiobutton
 * ├── radio
 * ╰── <child>
 * ]|
 *
 * A GtkRadioButton with indicator (see gtk_check_button_set_draw_indicator())) has a
 * main CSS node with name radiobutton and a subnode with name radio.
 *
 * |[<!-- language="plain" -->
 * button.radio
 * ├── radio
 * ╰── <child>
 * ]|
 *
 * A GtkRadioButton without indicator changes the name of its main node
 * to button and adds a .radio style class to it. The subnode is invisible
 * in this case.
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
 *    gtk_container_add (GTK_CONTAINER (box), radio1);
 *    gtk_container_add (GTK_CONTAINER (box), radio2);
 *    gtk_container_add (GTK_CONTAINER (window), box);
 *    gtk_widget_show (window);
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

typedef struct _GtkRadioButtonClass         GtkRadioButtonClass;

struct _GtkRadioButton
{
  GtkCheckButton parent_instance;
};

struct _GtkRadioButtonClass
{
  GtkCheckButtonClass parent_class;

  void (*group_changed) (GtkRadioButton *radio_button);
};

typedef struct
{
  GSList *group;
} GtkRadioButtonPrivate;

enum {
  PROP_0,
  PROP_GROUP,
  LAST_PROP
};

enum {
  GROUP_CHANGED,
  N_SIGNALS
};

static GParamSpec *radio_button_props[LAST_PROP] = { NULL, };
static guint signals[N_SIGNALS] = { 0 };

static void     gtk_radio_button_dispose        (GObject             *object);
static gboolean gtk_radio_button_focus          (GtkWidget           *widget,
						 GtkDirectionType     direction);
static void     gtk_radio_button_clicked        (GtkButton           *button);
static void     gtk_radio_button_set_property   (GObject             *object,
						 guint                prop_id,
						 const GValue        *value,
						 GParamSpec          *pspec);
static void     gtk_radio_button_get_property   (GObject             *object,
						 guint                prop_id,
						 GValue              *value,
						 GParamSpec          *pspec);

G_DEFINE_TYPE_WITH_PRIVATE (GtkRadioButton, gtk_radio_button, GTK_TYPE_CHECK_BUTTON)

static void
gtk_radio_button_class_init (GtkRadioButtonClass *class)
{
  GObjectClass *gobject_class;
  GtkButtonClass *button_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;
  button_class = (GtkButtonClass*) class;

  gobject_class->dispose = gtk_radio_button_dispose;
  gobject_class->set_property = gtk_radio_button_set_property;
  gobject_class->get_property = gtk_radio_button_get_property;

  /**
   * GtkRadioButton:group:
   *
   * Sets a new group for a radio button.
   */
  radio_button_props[PROP_GROUP] =
      g_param_spec_object ("group",
                           P_("Group"),
                           P_("The radio button whose group this widget belongs to."),
                           GTK_TYPE_RADIO_BUTTON,
                           GTK_PARAM_WRITABLE);

  g_object_class_install_properties (gobject_class, LAST_PROP, radio_button_props);

  widget_class->focus = gtk_radio_button_focus;

  button_class->clicked = gtk_radio_button_clicked;

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
   */
  signals[GROUP_CHANGED] = g_signal_new (I_("group-changed"),
                                         G_OBJECT_CLASS_TYPE (gobject_class),
                                         G_SIGNAL_RUN_FIRST,
                                         G_STRUCT_OFFSET (GtkRadioButtonClass, group_changed),
                                         NULL, NULL,
                                         NULL,
                                         G_TYPE_NONE, 0);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_RADIO_BUTTON_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("radiobutton"));
}

static void
gtk_radio_button_init (GtkRadioButton *radio_button)
{
  GtkRadioButtonPrivate *priv = gtk_radio_button_get_instance_private (radio_button);
  GtkCssNode *css_node;

  gtk_widget_set_receives_default (GTK_WIDGET (radio_button), FALSE);

  _gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), TRUE);

  priv->group = g_slist_prepend (NULL, radio_button);

  css_node = gtk_check_button_get_indicator_node (GTK_CHECK_BUTTON (radio_button));
  gtk_css_node_set_name (css_node, I_("radio"));
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
  GtkRadioButtonPrivate *priv = gtk_radio_button_get_instance_private (radio_button);
  GtkWidget *old_group_singleton = NULL;
  GtkWidget *new_group_singleton = NULL;

  g_return_if_fail (GTK_IS_RADIO_BUTTON (radio_button));

  if (g_slist_find (group, radio_button))
    return;

  if (priv->group)
    {
      GSList *slist;

      priv->group = g_slist_remove (priv->group, radio_button);

      if (priv->group && !priv->group->next)
	old_group_singleton = g_object_ref (priv->group->data);

      for (slist = priv->group; slist; slist = slist->next)
        {
          GtkRadioButton *tmp_button = slist->data;
          GtkRadioButtonPrivate *tmp_priv = gtk_radio_button_get_instance_private (tmp_button);

          tmp_priv->group = priv->group;
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
          GtkRadioButton *tmp_button = slist->data;
          GtkRadioButtonPrivate *tmp_priv = gtk_radio_button_get_instance_private (tmp_button);

          tmp_priv->group = priv->group;
        }
    }

  g_object_ref (radio_button);
  
  g_object_notify_by_pspec (G_OBJECT (radio_button), radio_button_props[PROP_GROUP]);
  g_signal_emit (radio_button, signals[GROUP_CHANGED], 0);
  if (old_group_singleton)
    {
      g_signal_emit (old_group_singleton, signals[GROUP_CHANGED], 0);
      g_object_unref (old_group_singleton);
    }
  if (new_group_singleton)
    {
      g_signal_emit (new_group_singleton, signals[GROUP_CHANGED], 0);
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
 *   while (some_condition)
 *     {
 *        radio_button = GTK_RADIO_BUTTON (gtk_radio_button_new (NULL));
 *
 *        gtk_radio_button_join_group (radio_button, last_button);
 *        last_button = radio_button;
 *     }
 * ]|
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
  GtkRadioButtonPrivate *priv = gtk_radio_button_get_instance_private (radio_button);

  g_return_val_if_fail (GTK_IS_RADIO_BUTTON (radio_button), NULL);

  return priv->group;
}


static void
gtk_radio_button_dispose (GObject *object)
{
  GtkWidget *old_group_singleton = NULL;
  GtkRadioButton *radio_button = GTK_RADIO_BUTTON (object);
  GtkRadioButtonPrivate *priv = gtk_radio_button_get_instance_private (radio_button);
  GSList *tmp_list;
  gboolean was_in_group;

  was_in_group = priv->group && priv->group->next;

  priv->group = g_slist_remove (priv->group, radio_button);
  if (priv->group && !priv->group->next)
    old_group_singleton = priv->group->data;

  tmp_list = priv->group;

  while (tmp_list)
    {
      GtkRadioButton *tmp_button = tmp_list->data;
      GtkRadioButtonPrivate *tmp_priv = gtk_radio_button_get_instance_private (tmp_button);

      tmp_list = tmp_list->next;

      tmp_priv->group = priv->group;
    }

  /* this button is no longer in the group */
  priv->group = NULL;

  if (old_group_singleton)
    g_signal_emit (old_group_singleton, signals[GROUP_CHANGED], 0);
  if (was_in_group)
    g_signal_emit (radio_button, signals[GROUP_CHANGED], 0);

  G_OBJECT_CLASS (gtk_radio_button_parent_class)->dispose (object);
}

static gboolean
gtk_radio_button_focus (GtkWidget         *widget,
			GtkDirectionType   direction)
{
  GtkRadioButton *radio_button = GTK_RADIO_BUTTON (widget);
  GtkRadioButtonPrivate *priv = gtk_radio_button_get_instance_private (radio_button);
  GSList *tmp_slist;

  /* Radio buttons with draw_indicator unset focus "normally", since
   * they look like buttons to the user.
   */
  if (!gtk_check_button_get_draw_indicator (GTK_CHECK_BUTTON (widget)))
    return GTK_WIDGET_CLASS (gtk_radio_button_parent_class)->focus (widget, direction);

  if (gtk_widget_is_focus (widget))
    {
      GPtrArray *child_array;
      GtkWidget *new_focus = NULL;
      GSList *l;
      guint index;
      gboolean found;
      guint i;

      if (direction == GTK_DIR_TAB_FORWARD ||
          direction == GTK_DIR_TAB_BACKWARD)
        return FALSE;

      child_array = g_ptr_array_sized_new (g_slist_length (priv->group));
      for (l = priv->group; l; l = l->next)
        g_ptr_array_add (child_array, l->data);

      gtk_widget_focus_sort (widget, direction, child_array);
      found = g_ptr_array_find (child_array, widget, &index);

      if (found)
        {
          /* Start at the *next* widget in the list */
          if (index < child_array->len - 1)
            index ++;
        }
      else
        {
          /* Search from the start of the list */
          index = 0;
        }

      for (i = index; i < child_array->len; i ++)
        {
          GtkWidget *child = g_ptr_array_index (child_array, i);

          if (gtk_widget_get_mapped (child) && gtk_widget_is_sensitive (child))
            {
              new_focus = child;
              break;
            }
        }


      if (new_focus)
        {
          gtk_widget_grab_focus (new_focus);
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (new_focus), TRUE);
        }

      g_ptr_array_free (child_array, TRUE);

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
	  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (tmp_slist->data)) &&
	      gtk_widget_get_visible (tmp_slist->data))
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
  GtkRadioButtonPrivate *priv = gtk_radio_button_get_instance_private (radio_button);
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
              g_signal_emit_by_name (tmp_button, "clicked");
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
