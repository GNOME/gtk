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
#ifndef __GTK_RADIO_MENU_ITEM_H__
#define __GTK_RADIO_MENU_ITEM_H__


#include <gdk/gdk.h>
#include <gtk/gtkcheckmenuitem.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_RADIO_MENU_ITEM(obj)          GTK_CHECK_CAST (obj, gtk_radio_menu_item_get_type (), GtkRadioMenuItem)
#define GTK_RADIO_MENU_ITEM_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_radio_menu_item_get_type (), GtkRadioMenuItemClass)
#define GTK_IS_RADIO_MENU_ITEM(obj)       GTK_CHECK_TYPE (obj, gtk_radio_menu_item_get_type ())


typedef struct _GtkRadioMenuItem       GtkRadioMenuItem;
typedef struct _GtkRadioMenuItemClass  GtkRadioMenuItemClass;

struct _GtkRadioMenuItem
{
  GtkCheckMenuItem check_menu_item;

  GSList *group;
};

struct _GtkRadioMenuItemClass
{
  GtkCheckMenuItemClass parent_class;
};


guint      gtk_radio_menu_item_get_type       (void);
GtkWidget* gtk_radio_menu_item_new            (GSList           *group);
GtkWidget* gtk_radio_menu_item_new_with_label (GSList           *group,
					       const gchar      *label);
GSList*    gtk_radio_menu_item_group          (GtkRadioMenuItem *radio_menu_item);
void       gtk_radio_menu_item_set_group      (GtkRadioMenuItem *radio_menu_item,
					       GSList           *group);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_RADIO_MENU_ITEM_H__ */
