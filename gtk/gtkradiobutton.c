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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkradiobutton.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"


enum {
  PROP_0,
  PROP_GROUP
};


static void     gtk_radio_button_destroy        (GtkObject           *object);
static gboolean gtk_radio_button_focus          (GtkWidget           *widget,
						 GtkDirectionType     direction);
static void     gtk_radio_button_clicked        (GtkButton           *button);
static void     gtk_radio_button_draw_indicator (GtkCheckButton      *check_button,
						 GdkRectangle        *area);
static void     gtk_radio_button_set_property   (GObject             *object,
						 guint                prop_id,
						 const GValue        *value,
						 GParamSpec          *pspec);
static void     gtk_radio_button_get_property   (GObject             *object,
						 guint                prop_id,
						 GValue              *value,
						 GParamSpec          *pspec);

G_DEFINE_TYPE (GtkRadioButton, gtk_radio_button, GTK_TYPE_CHECK_BUTTON)

static guint group_changed_signal = 0;

static void
gtk_radio_button_class_init (GtkRadioButtonClass *class)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkButtonClass *button_class;
  GtkCheckButtonClass *check_button_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  button_class = (GtkButtonClass*) class;
  check_button_class = (GtkCheckButtonClass*) class;

  gobject_class->set_property = gtk_radio_button_set_property;
  gobject_class->get_property = gtk_radio_button_get_property;

  g_object_class_install_property (gobject_class,
				   PROP_GROUP,
				   g_param_spec_object ("group",
							P_("Group"),
							P_("The radio button whose group this widget belongs to."),
							GTK_TYPE_RADIO_BUTTON,
							GTK_PARAM_WRITABLE));
  object_class->destroy = gtk_radio_button_destroy;

  widget_class->focus = gtk_radio_button_focus;

  button_class->clicked = gtk_radio_button_clicked;

  check_button_class->draw_indicator = gtk_radio_button_draw_indicator;

  class->group_changed = NULL;

  /**
   * GtkRadioButton::group-changed:
   * @style: the object which received the signal
   *
   * Emitted when the group of radio buttons that a radio button belongs
   * to changes. This is emitted when a radio button switches from
   * being alone to being part of a group of 2 or more buttons, or
   * vice-versa, and when a buttton is moved from one group of 2 or
   * more buttons to a different one, but not when the composition
   * of the group that a button belongs to changes.
   *
   * Since: 2.4
   */
  group_changed_signal = g_signal_new (I_("group-changed"),
				       G_OBJECT_CLASS_TYPE (object_class),
				       G_SIGNAL_RUN_FIRST,
				       G_STRUCT_OFFSET (GtkRadioButtonClass, group_changed),
				       NULL, NULL,
				       _gtk_marshal_VOID__VOID,
				       G_TYPE_NONE, 0);
}

static void
gtk_radio_button_init (GtkRadioButton *radio_button)
{
  GTK_WIDGET_SET_FLAGS (radio_button, GTK_NO_WINDOW);
  GTK_WIDGET_UNSET_FLAGS (radio_button, GTK_RECEIVES_DEFAULT);

  GTK_TOGGLE_BUTTON (radio_button)->active = TRUE;

  GTK_BUTTON (radio_button)->depress_on_activate = FALSE;

  radio_button->group = g_slist_prepend (NULL, radio_button);

  _gtk_button_set_depressed (GTK_BUTTON (radio_button), TRUE);
  gtk_widget_set_state (GTK_WIDGET (radio_button), GTK_STATE_ACTIVE);
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

    case PROP_GROUP:
      if (G_VALUE_HOLDS_OBJECT (value))
	slist = gtk_radio_button_get_group ((GtkRadioButton*) g_value_get_object (value));
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

void
gtk_radio_button_set_group (GtkRadioButton *radio_button,
			    GSList         *group)
{
  GtkWidget *old_group_singleton = NULL;
  GtkWidget *new_group_singleton = NULL;
  
  g_return_if_fail (GTK_IS_RADIO_BUTTON (radio_button));
  g_return_if_fail (!g_slist_find (group, radio_button));

  if (radio_button->group)
    {
      GSList *slist;

      radio_button->group = g_slist_remove (radio_button->group, radio_button);
      
      if (radio_button->group && !radio_button->group->next)
	old_group_singleton = g_object_ref (radio_button->group->data);
	  
      for (slist = radio_button->group; slist; slist = slist->next)
	{
	  GtkRadioButton *tmp_button;
	  
	  tmp_button = slist->data;
	  
	  tmp_button->group = radio_button->group;
	}
    }
  
  if (group && !group->next)
    new_group_singleton = g_object_ref (group->data);
  
  radio_button->group = g_slist_prepend (group, radio_button);
  
  if (group)
    {
      GSList *slist;
      
      for (slist = group; slist; slist = slist->next)
	{
	  GtkRadioButton *tmp_button;
	  
	  tmp_button = slist->data;
	  
	  tmp_button->group = radio_button->group;
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

GtkWidget*
gtk_radio_button_new (GSList *group)
{
  GtkRadioButton *radio_button;

  radio_button = g_object_new (GTK_TYPE_RADIO_BUTTON, NULL);

  if (group)
    gtk_radio_button_set_group (radio_button, group);

  return GTK_WIDGET (radio_button);
}

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
 * @group: the radio button group
 * @label: the text of the button, with an underscore in front of the
 *         mnemonic character
 * @returns: a new #GtkRadioButton
 *
 * Creates a new #GtkRadioButton containing a label, adding it to the same 
 * group as @group. The label will be created using 
 * gtk_label_new_with_mnemonic(), so underscores in @label indicate the 
 * mnemonic for the button.
 **/
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

GtkWidget*
gtk_radio_button_new_from_widget (GtkRadioButton *group)
{
  GSList *l = NULL;
  if (group)
    l = gtk_radio_button_get_group (group);
  return gtk_radio_button_new (l);
}


GtkWidget*
gtk_radio_button_new_with_label_from_widget (GtkRadioButton *group,
					     const gchar    *label)
{
  GSList *l = NULL;
  if (group)
    l = gtk_radio_button_get_group (group);
  return gtk_radio_button_new_with_label (l, label);
}

/**
 * gtk_radio_button_new_with_mnemonic_from_widget:
 * @group: widget to get radio group from
 * @label: the text of the button, with an underscore in front of the
 *         mnemonic character
 * @returns: a new #GtkRadioButton
 *
 * Creates a new #GtkRadioButton containing a label. The label
 * will be created using gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the button.
 **/
GtkWidget*
gtk_radio_button_new_with_mnemonic_from_widget (GtkRadioButton *group,
					        const gchar    *label)
{
  GSList *l = NULL;
  if (group)
    l = gtk_radio_button_get_group (group);
  return gtk_radio_button_new_with_mnemonic (l, label);
}

GSList*
gtk_radio_button_get_group (GtkRadioButton *radio_button)
{
  g_return_val_if_fail (GTK_IS_RADIO_BUTTON (radio_button), NULL);

  return radio_button->group;
}


static void
gtk_radio_button_destroy (GtkObject *object)
{
  GtkWidget *old_group_singleton = NULL;
  GtkRadioButton *radio_button;
  GtkRadioButton *tmp_button;
  GSList *tmp_list;
  gboolean was_in_group;
  
  radio_button = GTK_RADIO_BUTTON (object);

  was_in_group = radio_button->group && radio_button->group->next;
  
  radio_button->group = g_slist_remove (radio_button->group, radio_button);
  if (radio_button->group && !radio_button->group->next)
    old_group_singleton = radio_button->group->data;

  tmp_list = radio_button->group;

  while (tmp_list)
    {
      tmp_button = tmp_list->data;
      tmp_list = tmp_list->next;

      tmp_button->group = radio_button->group;
    }

  /* this button is no longer in the group */
  radio_button->group = NULL;

  if (old_group_singleton)
    g_signal_emit (old_group_singleton, group_changed_signal, 0);
  if (was_in_group)
    g_signal_emit (radio_button, group_changed_signal, 0);
  
  if (GTK_OBJECT_CLASS (gtk_radio_button_parent_class)->destroy)
    (* GTK_OBJECT_CLASS (gtk_radio_button_parent_class)->destroy) (object);
}

static void
get_coordinates (GtkWidget    *widget,
		 GtkWidget    *reference,
		 gint         *x,
		 gint         *y)
{
  *x = widget->allocation.x + widget->allocation.width / 2;
  *y = widget->allocation.y + widget->allocation.height / 2;
  
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
  GSList *tmp_slist;

  /* Radio buttons with draw_indicator unset focus "normally", since
   * they look like buttons to the user.
   */
  if (!GTK_TOGGLE_BUTTON (widget)->draw_indicator)
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
	  focus_list = g_slist_copy (radio_button->group);
	  focus_list = g_slist_sort_with_data (focus_list, left_right_compare, toplevel);
	  break;
	case GTK_DIR_UP:
	case GTK_DIR_DOWN:
	  focus_list = g_slist_copy (radio_button->group);
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
	      
	      if (GTK_WIDGET_REALIZED (child) && GTK_WIDGET_IS_SENSITIVE (child))
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
	      
	      if (GTK_WIDGET_REALIZED (child) && GTK_WIDGET_IS_SENSITIVE (child))
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
      
      tmp_slist = radio_button->group;
      while (tmp_slist)
	{
	  if (GTK_TOGGLE_BUTTON (tmp_slist->data)->active)
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
  GtkToggleButton *toggle_button;
  GtkRadioButton *radio_button;
  GtkToggleButton *tmp_button;
  GtkStateType new_state;
  GSList *tmp_list;
  gint toggled;
  gboolean depressed;

  radio_button = GTK_RADIO_BUTTON (button);
  toggle_button = GTK_TOGGLE_BUTTON (button);
  toggled = FALSE;

  g_object_ref (GTK_WIDGET (button));

  if (toggle_button->active)
    {
      tmp_button = NULL;
      tmp_list = radio_button->group;

      while (tmp_list)
	{
	  tmp_button = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (tmp_button->active && tmp_button != toggle_button)
	    break;

	  tmp_button = NULL;
	}

      if (!tmp_button)
	{
	  new_state = (button->in_button ? GTK_STATE_PRELIGHT : GTK_STATE_ACTIVE);
	}
      else
	{
	  toggled = TRUE;
	  toggle_button->active = !toggle_button->active;
	  new_state = (button->in_button ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL);
	}
    }
  else
    {
      toggled = TRUE;
      toggle_button->active = !toggle_button->active;
      
      tmp_list = radio_button->group;
      while (tmp_list)
	{
	  tmp_button = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (tmp_button->active && (tmp_button != toggle_button))
	    {
	      gtk_button_clicked (GTK_BUTTON (tmp_button));
	      break;
	    }
	}

      new_state = (button->in_button ? GTK_STATE_PRELIGHT : GTK_STATE_ACTIVE);
    }

  if (toggle_button->inconsistent)
    depressed = FALSE;
  else if (button->in_button && button->button_down)
    depressed = !toggle_button->active;
  else
    depressed = toggle_button->active;

  if (GTK_WIDGET_STATE (button) != new_state)
    gtk_widget_set_state (GTK_WIDGET (button), new_state);

  if (toggled)
    {
      gtk_toggle_button_toggled (toggle_button);

      g_object_notify (G_OBJECT (toggle_button), "active");
    }

  _gtk_button_set_depressed (button, depressed);

  gtk_widget_queue_draw (GTK_WIDGET (button));

  g_object_unref (button);
}

static void
gtk_radio_button_draw_indicator (GtkCheckButton *check_button,
				 GdkRectangle   *area)
{
  GtkWidget *widget;
  GtkWidget *child;
  GtkButton *button;
  GtkToggleButton *toggle_button;
  GtkStateType state_type;
  GtkShadowType shadow_type;
  gint x, y;
  gint indicator_size, indicator_spacing;
  gint focus_width;
  gint focus_pad;
  gboolean interior_focus;

  if (GTK_WIDGET_DRAWABLE (check_button))
    {
      widget = GTK_WIDGET (check_button);
      button = GTK_BUTTON (check_button);
      toggle_button = GTK_TOGGLE_BUTTON (check_button);

      gtk_widget_style_get (widget,
			    "interior-focus", &interior_focus,
			    "focus-line-width", &focus_width,
			    "focus-padding", &focus_pad,
			    NULL);

      _gtk_check_button_get_props (check_button, &indicator_size, &indicator_spacing);

      x = widget->allocation.x + indicator_spacing + GTK_CONTAINER (widget)->border_width;
      y = widget->allocation.y + (widget->allocation.height - indicator_size) / 2;

      child = GTK_BIN (check_button)->child;
      if (!interior_focus || !(child && GTK_WIDGET_VISIBLE (child)))
	x += focus_width + focus_pad;      

      if (toggle_button->inconsistent)
	shadow_type = GTK_SHADOW_ETCHED_IN;
      else if (toggle_button->active)
	shadow_type = GTK_SHADOW_IN;
      else
	shadow_type = GTK_SHADOW_OUT;

      if (button->activate_timeout || (button->button_down && button->in_button))
	state_type = GTK_STATE_ACTIVE;
      else if (button->in_button)
	state_type = GTK_STATE_PRELIGHT;
      else if (!GTK_WIDGET_IS_SENSITIVE (widget))
	state_type = GTK_STATE_INSENSITIVE;
      else
	state_type = GTK_STATE_NORMAL;

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
	x = widget->allocation.x + widget->allocation.width - (indicator_size + x - widget->allocation.x);

      if (GTK_WIDGET_STATE (toggle_button) == GTK_STATE_PRELIGHT)
	{
	  GdkRectangle restrict_area;
	  GdkRectangle new_area;
	      
	  restrict_area.x = widget->allocation.x + GTK_CONTAINER (widget)->border_width;
	  restrict_area.y = widget->allocation.y + GTK_CONTAINER (widget)->border_width;
	  restrict_area.width = widget->allocation.width - (2 * GTK_CONTAINER (widget)->border_width);
	  restrict_area.height = widget->allocation.height - (2 * GTK_CONTAINER (widget)->border_width);
	  
	  if (gdk_rectangle_intersect (area, &restrict_area, &new_area))
	    {
	      gtk_paint_flat_box (widget->style, widget->window, GTK_STATE_PRELIGHT,
				  GTK_SHADOW_ETCHED_OUT, 
				  area, widget, "checkbutton",
				  new_area.x, new_area.y,
				  new_area.width, new_area.height);
	    }
	}

      gtk_paint_option (widget->style, widget->window,
			state_type, shadow_type,
			area, widget, "radiobutton",
			x, y, indicator_size, indicator_size);
    }
}

#define __GTK_RADIO_BUTTON_C__
#include "gtkaliasdef.c"
