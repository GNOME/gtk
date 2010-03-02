/* testmenubars.c -- test different packing directions
 * Copyright (C) 2005  Red Hat, Inc.
 * Author: Matthias Clasen
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

#include <gtk/gtk.h>

static GtkWidget *
create_menu (depth)
{
    GtkWidget *menu;
    GtkWidget *menuitem;

    if (depth < 1)
        return NULL;

    menu = gtk_menu_new ();

    menuitem = gtk_menu_item_new_with_label ("One");
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
			       create_menu (depth - 1));

    menuitem = gtk_menu_item_new_with_mnemonic ("Two");
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
			       create_menu (depth - 1));

    menuitem = gtk_menu_item_new_with_mnemonic ("Three");
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
			       create_menu (depth - 1));

    return menu;
}

static GtkWidget*
create_menubar (GtkPackDirection pack_dir,
		GtkPackDirection child_pack_dir,
		gdouble          angle)
{
  GtkWidget *menubar;
  GtkWidget *menuitem;
  GtkWidget *menu;

  menubar = gtk_menu_bar_new ();
  gtk_menu_bar_set_pack_direction (GTK_MENU_BAR (menubar), 
				   pack_dir);
  gtk_menu_bar_set_child_pack_direction (GTK_MENU_BAR (menubar),
					 child_pack_dir);
  
  menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_HOME, NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
  gtk_label_set_angle (GTK_LABEL (GTK_BIN (menuitem)->child), angle);
  menu = create_menu (2, TRUE);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);

  menuitem = gtk_menu_item_new_with_label ("foo");
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
  gtk_label_set_angle (GTK_LABEL (GTK_BIN (menuitem)->child), angle);
  menu = create_menu (2, TRUE);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);

  menuitem = gtk_menu_item_new_with_label ("bar");
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
  gtk_label_set_angle (GTK_LABEL (GTK_BIN (menuitem)->child), angle);
  menu = create_menu (2, TRUE);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);

  return menubar;
}

int 
main (int argc, char **argv)
{
  static GtkWidget *window = NULL;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *box3;
  GtkWidget *button;
  GtkWidget *separator;

  gtk_init (&argc, &argv);
  
  if (!window)
    {
      GtkWidget *menubar1;
      GtkWidget *menubar2;
      GtkWidget *menubar3;
      GtkWidget *menubar4;
      GtkWidget *menubar5;
      GtkWidget *menubar6;
      GtkAccelGroup *accel_group;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      g_signal_connect (window, "destroy",
			G_CALLBACK(gtk_main_quit), NULL);
      g_signal_connect (window, "delete-event",
			G_CALLBACK (gtk_true), NULL);
      
      accel_group = gtk_accel_group_new ();
      gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

      gtk_window_set_title (GTK_WINDOW (window), "menus");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);
      
      box1 = gtk_vbox_new (FALSE, 0);
      box2 = gtk_hbox_new (FALSE, 0);
      box3 = gtk_vbox_new (FALSE, 0);
      
      /* Rotation by 0 and 180 degrees is broken in Pango, #166832 */
      menubar1 = create_menubar (GTK_PACK_DIRECTION_LTR, GTK_PACK_DIRECTION_LTR, 0.01);
      menubar2 = create_menubar (GTK_PACK_DIRECTION_BTT, GTK_PACK_DIRECTION_BTT, 90);
      menubar3 = create_menubar (GTK_PACK_DIRECTION_TTB, GTK_PACK_DIRECTION_TTB, 270);
      menubar4 = create_menubar (GTK_PACK_DIRECTION_RTL, GTK_PACK_DIRECTION_RTL, 180.01);
      menubar5 = create_menubar (GTK_PACK_DIRECTION_LTR, GTK_PACK_DIRECTION_BTT, 90);
      menubar6 = create_menubar (GTK_PACK_DIRECTION_BTT, GTK_PACK_DIRECTION_LTR, 0.01);

      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_box_pack_start (GTK_BOX (box1), menubar1, FALSE, TRUE, 0);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_box_pack_start (GTK_BOX (box2), menubar2, FALSE, TRUE, 0);
      gtk_box_pack_start (GTK_BOX (box2), box3, TRUE, TRUE, 0);
      gtk_box_pack_start (GTK_BOX (box2), menubar3, FALSE, TRUE, 0);
      gtk_box_pack_start (GTK_BOX (box1), menubar4, FALSE, TRUE, 0);
      gtk_box_pack_start (GTK_BOX (box3), menubar5, TRUE, TRUE, 0);
      gtk_box_pack_start (GTK_BOX (box3), menubar6, TRUE, TRUE, 0);

      gtk_widget_show_all (box1);
            
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);

      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
				G_CALLBACK(gtk_widget_destroy), window);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }

  if (!gtk_widget_get_visible (window))
    {
      gtk_widget_show (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  gtk_main ();

  return 0;
}

