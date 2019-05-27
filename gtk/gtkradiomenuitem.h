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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#ifndef __GTK_RADIO_MENU_ITEM_H__
#define __GTK_RADIO_MENU_ITEM_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkcheckmenuitem.h>


G_BEGIN_DECLS

#define GTK_TYPE_RADIO_MENU_ITEM	      (gtk_radio_menu_item_get_type ())
#define GTK_RADIO_MENU_ITEM(obj)	      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_RADIO_MENU_ITEM, GtkRadioMenuItem))
#define GTK_IS_RADIO_MENU_ITEM(obj)	      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_RADIO_MENU_ITEM))

typedef struct _GtkRadioMenuItem              GtkRadioMenuItem;

GDK_AVAILABLE_IN_ALL
GType      gtk_radio_menu_item_get_type	         (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkWidget* gtk_radio_menu_item_new                           (GSList           *group);
GDK_AVAILABLE_IN_ALL
GtkWidget* gtk_radio_menu_item_new_with_label                (GSList           *group,
							      const gchar      *label);
GDK_AVAILABLE_IN_ALL
GtkWidget* gtk_radio_menu_item_new_with_mnemonic             (GSList           *group,
							      const gchar      *label);
GDK_AVAILABLE_IN_ALL
GtkWidget* gtk_radio_menu_item_new_from_widget               (GtkRadioMenuItem *group);
GDK_AVAILABLE_IN_ALL
GtkWidget *gtk_radio_menu_item_new_with_mnemonic_from_widget (GtkRadioMenuItem *group,
							      const gchar      *label);
GDK_AVAILABLE_IN_ALL
GtkWidget *gtk_radio_menu_item_new_with_label_from_widget    (GtkRadioMenuItem *group,
							      const gchar      *label);
GDK_AVAILABLE_IN_ALL
GSList*    gtk_radio_menu_item_get_group                     (GtkRadioMenuItem *radio_menu_item);
GDK_AVAILABLE_IN_ALL
void       gtk_radio_menu_item_set_group                     (GtkRadioMenuItem *radio_menu_item,
							      GSList           *group);

GDK_AVAILABLE_IN_ALL
void       gtk_radio_menu_item_join_group                    (GtkRadioMenuItem *radio_menu_item,
                                                              GtkRadioMenuItem *group_source);

G_END_DECLS

#endif /* __GTK_RADIO_MENU_ITEM_H__ */
