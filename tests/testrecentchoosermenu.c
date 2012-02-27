/* testrecentchoosermenu.c - Test GtkRecentChooserMenu
 * Copyright (C) 2007  Emmanuele Bassi  <ebassi@gnome.org>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <gtk/gtk.h>

static GtkRecentManager *manager = NULL;
static GtkWidget *window = NULL;
static GtkWidget *label = NULL;

static void
item_activated_cb (GtkRecentChooser *chooser,
                   gpointer          data)
{
  GtkRecentInfo *info;
  GString *text;
  gchar *label_text;

  info = gtk_recent_chooser_get_current_item (chooser);
  if (!info)
    {
      g_warning ("Unable to retrieve the current item, aborting...");
      return;
    }

  text = g_string_new ("Selected recent item:\n");
  g_string_append_printf (text, "  URI: %s\n",
                          gtk_recent_info_get_uri (info));
  g_string_append_printf (text, "  MIME Type: %s\n",
                          gtk_recent_info_get_mime_type (info));

  label_text = g_string_free (text, FALSE);
  gtk_label_set_text (GTK_LABEL (label), label_text);
  
  gtk_recent_info_unref (info);
  g_free (label_text);
}

static GtkWidget *
create_recent_chooser_menu (gint limit)
{
  GtkWidget *menu;
  GtkRecentFilter *filter;
  GtkWidget *menuitem;
  
  menu = gtk_recent_chooser_menu_new_for_manager (manager);

  if (limit > 0)
    gtk_recent_chooser_set_limit (GTK_RECENT_CHOOSER (menu), limit);
  gtk_recent_chooser_set_local_only (GTK_RECENT_CHOOSER (menu), TRUE);
  gtk_recent_chooser_set_show_icons (GTK_RECENT_CHOOSER (menu), TRUE);
  gtk_recent_chooser_set_show_tips (GTK_RECENT_CHOOSER (menu), TRUE);
  gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER (menu),
                                    GTK_RECENT_SORT_MRU);
  gtk_recent_chooser_menu_set_show_numbers (GTK_RECENT_CHOOSER_MENU (menu),
                                            TRUE);

  filter = gtk_recent_filter_new ();
  gtk_recent_filter_set_name (filter, "Gedit files");
  gtk_recent_filter_add_application (filter, "gedit");
  gtk_recent_chooser_add_filter (GTK_RECENT_CHOOSER (menu), filter);
  gtk_recent_chooser_set_filter (GTK_RECENT_CHOOSER (menu), filter);

  g_signal_connect (menu, "item-activated",
                    G_CALLBACK (item_activated_cb),
                    NULL);

  menuitem = gtk_separator_menu_item_new ();
  gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("Test prepend");
  gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  menuitem = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("Test append");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLEAR, NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  gtk_widget_show_all (menu);

  return menu;
}

static GtkWidget *
create_file_menu (GtkAccelGroup *accelgroup)
{
  GtkWidget *menu;
  GtkWidget *menuitem;
  GtkWidget *recentmenu;

  menu = gtk_menu_new ();

  menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_NEW, accelgroup);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_OPEN, accelgroup);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_mnemonic ("_Open Recent");
  recentmenu = create_recent_chooser_menu (-1);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), recentmenu);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  menuitem = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT, accelgroup);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  gtk_widget_show (menu);

  return menu;
}

int
main (int argc, char *argv[])
{
  GtkWidget *box;
  GtkWidget *menubar;
  GtkWidget *menuitem;
  GtkWidget *menu;
  GtkWidget *button;
  GtkAccelGroup *accel_group;

  gtk_init (&argc, &argv);

  manager = gtk_recent_manager_get_default ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), -1, -1);
  gtk_window_set_title (GTK_WINDOW (window), "Recent Chooser Menu Test");
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  accel_group = gtk_accel_group_new ();
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
  
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_widget_show (box);

  menubar = gtk_menu_bar_new ();
  gtk_box_pack_start (GTK_BOX (box), menubar, FALSE, TRUE, 0);
  gtk_widget_show (menubar);

  menu = create_file_menu (accel_group);
  menuitem = gtk_menu_item_new_with_mnemonic ("_File");
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
  gtk_widget_show (menuitem);

  menu = create_recent_chooser_menu (4);
  menuitem = gtk_menu_item_new_with_mnemonic ("_Recently Used");
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
  gtk_widget_show (menuitem);

  label = gtk_label_new ("No recent item selected");
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  button = gtk_button_new_with_label ("Close");
  g_signal_connect_swapped (button, "clicked",
                            G_CALLBACK (gtk_widget_destroy),
                            window);
  gtk_box_pack_end (GTK_BOX (box), button, TRUE, TRUE, 0);
  gtk_widget_set_can_default (button, TRUE);
  gtk_widget_grab_default (button);
  gtk_widget_show (button);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
