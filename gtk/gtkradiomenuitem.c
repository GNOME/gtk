/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gtkaccellabel.h"
#include "gtkradiomenuitem.h"


static void gtk_radio_menu_item_class_init     (GtkRadioMenuItemClass *klass);
static void gtk_radio_menu_item_init           (GtkRadioMenuItem      *radio_menu_item);
static void gtk_radio_menu_item_destroy        (GtkObject             *object);
static void gtk_radio_menu_item_activate       (GtkMenuItem           *menu_item);
static void gtk_radio_menu_item_draw_indicator (GtkCheckMenuItem      *check_menu_item,
						GdkRectangle          *area);

static GtkCheckMenuItemClass *parent_class = NULL;

GtkType
gtk_radio_menu_item_get_type (void)
{
  static GtkType radio_menu_item_type = 0;

  if (!radio_menu_item_type)
    {
      static const GtkTypeInfo radio_menu_item_info =
      {
        "GtkRadioMenuItem",
        sizeof (GtkRadioMenuItem),
        sizeof (GtkRadioMenuItemClass),
        (GtkClassInitFunc) gtk_radio_menu_item_class_init,
        (GtkObjectInitFunc) gtk_radio_menu_item_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      radio_menu_item_type = gtk_type_unique (gtk_check_menu_item_get_type (), &radio_menu_item_info);
    }

  return radio_menu_item_type;
}

GtkWidget*
gtk_radio_menu_item_new (GSList *group)
{
  GtkRadioMenuItem *radio_menu_item;

  radio_menu_item = gtk_type_new (gtk_radio_menu_item_get_type ());

  gtk_radio_menu_item_set_group (radio_menu_item, group);

  return GTK_WIDGET (radio_menu_item);
}

void
gtk_radio_menu_item_set_group (GtkRadioMenuItem *radio_menu_item,
			       GSList           *group)
{
  g_return_if_fail (radio_menu_item != NULL);
  g_return_if_fail (GTK_IS_RADIO_MENU_ITEM (radio_menu_item));
  g_return_if_fail (!g_slist_find (group, radio_menu_item));
  
  if (radio_menu_item->group)
    {
      GSList *slist;
      
      radio_menu_item->group = g_slist_remove (radio_menu_item->group, radio_menu_item);
      
      for (slist = radio_menu_item->group; slist; slist = slist->next)
	{
	  GtkRadioMenuItem *tmp_item;
	  
	  tmp_item = slist->data;
	  
	  tmp_item->group = radio_menu_item->group;
	}
    }
  
  radio_menu_item->group = g_slist_prepend (group, radio_menu_item);
  
  if (group)
    {
      GSList *slist;
      
      for (slist = group; slist; slist = slist->next)
	{
	  GtkRadioMenuItem *tmp_item;
	  
	  tmp_item = slist->data;
	  
	  tmp_item->group = radio_menu_item->group;
	}
    }
  else
    {
      GTK_CHECK_MENU_ITEM (radio_menu_item)->active = TRUE;
      /* gtk_widget_set_state (GTK_WIDGET (radio_menu_item), GTK_STATE_ACTIVE);
       */
    }
}

GtkWidget*
gtk_radio_menu_item_new_with_label (GSList *group,
				    const gchar *label)
{
  GtkWidget *radio_menu_item;
  GtkWidget *accel_label;

  radio_menu_item = gtk_radio_menu_item_new (group);
  accel_label = gtk_accel_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);
  gtk_container_add (GTK_CONTAINER (radio_menu_item), accel_label);
  gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label), radio_menu_item);
  gtk_widget_show (accel_label);

  return radio_menu_item;
}

GSList*
gtk_radio_menu_item_group (GtkRadioMenuItem *radio_menu_item)
{
  g_return_val_if_fail (radio_menu_item != NULL, NULL);
  g_return_val_if_fail (GTK_IS_RADIO_MENU_ITEM (radio_menu_item), NULL);

  return radio_menu_item->group;
}


static void
gtk_radio_menu_item_class_init (GtkRadioMenuItemClass *klass)
{
  GtkObjectClass *object_class;
  GtkMenuItemClass *menu_item_class;
  GtkCheckMenuItemClass *check_menu_item_class;

  object_class = (GtkObjectClass*) klass;
  menu_item_class = (GtkMenuItemClass*) klass;
  check_menu_item_class = (GtkCheckMenuItemClass*) klass;

  parent_class = gtk_type_class (gtk_check_menu_item_get_type ());

  object_class->destroy = gtk_radio_menu_item_destroy;

  menu_item_class->activate = gtk_radio_menu_item_activate;

  check_menu_item_class->draw_indicator = gtk_radio_menu_item_draw_indicator;
}

static void
gtk_radio_menu_item_init (GtkRadioMenuItem *radio_menu_item)
{
  radio_menu_item->group = g_slist_prepend (NULL, radio_menu_item);
}

static void
gtk_radio_menu_item_destroy (GtkObject *object)
{
  GtkRadioMenuItem *radio_menu_item;
  GtkRadioMenuItem *tmp_menu_item;
  GSList *tmp_list;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_RADIO_MENU_ITEM (object));

  radio_menu_item = GTK_RADIO_MENU_ITEM (object);

  radio_menu_item->group = g_slist_remove (radio_menu_item->group,
					   radio_menu_item);
  tmp_list = radio_menu_item->group;

  while (tmp_list)
    {
      tmp_menu_item = tmp_list->data;
      tmp_list = tmp_list->next;

      tmp_menu_item->group = radio_menu_item->group;
    }

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_radio_menu_item_activate (GtkMenuItem *menu_item)
{
  GtkRadioMenuItem *radio_menu_item;
  GtkCheckMenuItem *check_menu_item;
  GtkCheckMenuItem *tmp_menu_item;
  GSList *tmp_list;
  gint toggled;

  g_return_if_fail (menu_item != NULL);
  g_return_if_fail (GTK_IS_RADIO_MENU_ITEM (menu_item));

  radio_menu_item = GTK_RADIO_MENU_ITEM (menu_item);
  check_menu_item = GTK_CHECK_MENU_ITEM (menu_item);
  toggled = FALSE;

  if (check_menu_item->active)
    {
      tmp_menu_item = NULL;
      tmp_list = radio_menu_item->group;

      while (tmp_list)
	{
	  tmp_menu_item = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (tmp_menu_item->active && (tmp_menu_item != check_menu_item))
	    break;

	  tmp_menu_item = NULL;
	}

      if (tmp_menu_item)
	{
	  toggled = TRUE;
	  check_menu_item->active = !check_menu_item->active;
	}
    }
  else
    {
      toggled = TRUE;
      check_menu_item->active = !check_menu_item->active;

      tmp_list = radio_menu_item->group;
      while (tmp_list)
	{
	  tmp_menu_item = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (tmp_menu_item->active && (tmp_menu_item != check_menu_item))
	    {
	      gtk_menu_item_activate (GTK_MENU_ITEM (tmp_menu_item));
	      break;
	    }
	}
    }

  if (toggled)
    gtk_check_menu_item_toggled (check_menu_item);
  gtk_widget_queue_draw (GTK_WIDGET (radio_menu_item));
}

static void
gtk_radio_menu_item_draw_indicator (GtkCheckMenuItem *check_menu_item,
				    GdkRectangle     *area)
{
  GtkWidget *widget;
  GtkStateType state_type;
  GtkShadowType shadow_type;
  gint width, height;
  gint x, y;

  g_return_if_fail (check_menu_item != NULL);
  g_return_if_fail (GTK_IS_RADIO_MENU_ITEM (check_menu_item));

  if (GTK_WIDGET_DRAWABLE (check_menu_item))
    {
      widget = GTK_WIDGET (check_menu_item);

      width = 8;
      height = 8;
      x = (GTK_CONTAINER (check_menu_item)->border_width +
	   widget->style->klass->xthickness + 2);
      y = (widget->allocation.height - height) / 2;

      if (check_menu_item->active ||
	  check_menu_item->always_show_toggle ||
	  (GTK_WIDGET_STATE (check_menu_item) == GTK_STATE_PRELIGHT))
	{
	  state_type = GTK_WIDGET_STATE (widget);
	  if (check_menu_item->active ||
	      !check_menu_item->always_show_toggle)
	    shadow_type = GTK_SHADOW_IN;
	  else
	    shadow_type = GTK_SHADOW_OUT;

	  gtk_paint_option (widget->style, widget->window,
			    state_type, shadow_type,
			    area, widget, "option",
			    x, y, width, height);
	}
    }
}

