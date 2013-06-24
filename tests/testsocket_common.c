/* testsocket_common.c
 * Copyright (C) 2001 Red Hat, Inc
 * Author: Owen Taylor
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
#if defined (GDK_WINDOWING_X11)
#include <gtk/gtkx.h>
#elif defined (GDK_WINDOWING_WIN32)
#include "win32/gdkwin32.h"
#endif

enum
{
  ACTION_FILE_NEW,
  ACTION_FILE_OPEN,
  ACTION_OK,
  ACTION_HELP_ABOUT
};

static void
print_hello (GtkWidget *w,
	     guint      action)
{
  switch (action)
    {
    case ACTION_FILE_NEW:
      g_message ("File New activated");
      break;
    case ACTION_FILE_OPEN:
      g_message ("File Open activated");
      break;
    case ACTION_OK:
      g_message ("OK activated");
      break;
    case ACTION_HELP_ABOUT:
      g_message ("Help About activated ");
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
remove_buttons (GtkWidget *widget, GtkWidget *other_button)
{
  gtk_widget_destroy (other_button);
  gtk_widget_destroy (widget);
}

static gboolean
blink_cb (gpointer data)
{
  GtkWidget *widget = data;

  gtk_widget_show (widget);
  g_object_set_data (G_OBJECT (widget), "blink", NULL);

  return FALSE;
}

static void
blink (GtkWidget *widget,
       GtkWidget *window)
{
  guint blink_timeout = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (window), "blink"));
  
  if (!blink_timeout)
    {
      blink_timeout = gdk_threads_add_timeout (1000, blink_cb, window);
      gtk_widget_hide (window);

      g_object_set_data (G_OBJECT (window), "blink", GUINT_TO_POINTER (blink_timeout));
    }
}

static void
local_destroy (GtkWidget *window)
{
  guint blink_timeout = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (window), "blink"));
  if (blink_timeout)
    g_source_remove (blink_timeout);
}

static void
remote_destroy (GtkWidget *window)
{
  local_destroy (window);
  gtk_main_quit ();
}

static void
add_buttons (GtkWidget *widget, GtkWidget *box)
{
  GtkWidget *add_button;
  GtkWidget *remove_button;

  add_button = gtk_button_new_with_mnemonic ("_Add");
  gtk_box_pack_start (GTK_BOX (box), add_button, TRUE, TRUE, 0);
  gtk_widget_show (add_button);

  g_signal_connect (add_button, "clicked",
		    G_CALLBACK (add_buttons),
		    box);

  remove_button = gtk_button_new_with_mnemonic ("_Remove");
  gtk_box_pack_start (GTK_BOX (box), remove_button, TRUE, TRUE, 0);
  gtk_widget_show (remove_button);

  g_signal_connect (remove_button, "clicked",
		    G_CALLBACK (remove_buttons),
		    add_button);
}

static GtkWidget *
create_combo (void)
{
  GtkComboBoxText *combo;
  GtkWidget *entry;

  combo = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new_with_entry ());

  gtk_combo_box_text_append_text (combo, "item0");
  gtk_combo_box_text_append_text (combo, "item1 item1");
  gtk_combo_box_text_append_text (combo, "item2 item2 item2");
  gtk_combo_box_text_append_text (combo, "item3 item3 item3 item3");
  gtk_combo_box_text_append_text (combo, "item4 item4 item4 item4 item4");
  gtk_combo_box_text_append_text (combo, "item5 item5 item5 item5 item5 item5");
  gtk_combo_box_text_append_text (combo, "item6 item6 item6 item6 item6");
  gtk_combo_box_text_append_text (combo, "item7 item7 item7 item7");
  gtk_combo_box_text_append_text (combo, "item8 item8 item8");
  gtk_combo_box_text_append_text (combo, "item9 item9");

  entry = gtk_bin_get_child (GTK_BIN (combo));
  gtk_entry_set_text (GTK_ENTRY (entry), "hello world");
  gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);

  return GTK_WIDGET (combo);
}

static GtkWidget *
create_menubar (GtkWindow *window)
{
  GtkAccelGroup *accel_group=NULL;
  GtkWidget *menubar;
  GtkWidget *menuitem;
  GtkWidget *menu;

  accel_group = gtk_accel_group_new ();
  gtk_window_add_accel_group (window, accel_group);

  menubar = gtk_menu_bar_new ();

  menuitem = gtk_menu_item_new_with_mnemonic ("_File");
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
  menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
  menuitem = gtk_menu_item_new_with_mnemonic ("_New");
  g_signal_connect (menuitem, "activate", G_CALLBACK (print_hello), window);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  menuitem = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  menuitem = gtk_menu_item_new_with_mnemonic ("_Quit");
  g_signal_connect (menuitem, "activate", G_CALLBACK (gtk_main_quit), window);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

  menuitem = gtk_menu_item_new_with_mnemonic ("O_K");
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);

  menuitem = gtk_menu_item_new_with_mnemonic ("_Help");
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
  menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
  menuitem = gtk_menu_item_new_with_mnemonic ("_About");
  g_signal_connect (menuitem, "activate", G_CALLBACK (print_hello), window);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

  return menubar;
}

static GtkWidget *
create_combo_box (void)
{
  GtkComboBoxText *combo_box = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new ());

  gtk_combo_box_text_append_text (combo_box, "This");
  gtk_combo_box_text_append_text (combo_box, "Is");
  gtk_combo_box_text_append_text (combo_box, "A");
  gtk_combo_box_text_append_text (combo_box, "ComboBox");
  
  return GTK_WIDGET (combo_box);
}

static GtkWidget *
create_content (GtkWindow *window, gboolean local)
{
  GtkWidget *vbox;
  GtkWidget *button;
  GtkWidget *frame;

  frame = gtk_frame_new (local? "Local" : "Remote");
  gtk_container_set_border_width (GTK_CONTAINER (frame), 3);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_set_homogeneous (GTK_BOX (vbox), TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);

  gtk_container_add (GTK_CONTAINER (frame), vbox);
  
  /* Combo */
  gtk_box_pack_start (GTK_BOX (vbox), create_combo(), TRUE, TRUE, 0);

  /* Entry */
  gtk_box_pack_start (GTK_BOX (vbox), gtk_entry_new(), TRUE, TRUE, 0);

  /* Close Button */
  button = gtk_button_new_with_mnemonic ("_Close");
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (gtk_widget_destroy), window);

  /* Blink Button */
  button = gtk_button_new_with_mnemonic ("_Blink");
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (blink),
		    window);

  /* Menubar */
  gtk_box_pack_start (GTK_BOX (vbox), create_menubar (GTK_WINDOW (window)),
		      TRUE, TRUE, 0);

  /* Combo Box */
  gtk_box_pack_start (GTK_BOX (vbox), create_combo_box (), TRUE, TRUE, 0);
  
  add_buttons (NULL, vbox);

  return frame;
}

guint32
create_child_plug (guint32  xid,
		   gboolean local)
{
  GtkWidget *window;
  GtkWidget *content;

  window = gtk_plug_new (xid);

  g_signal_connect (window, "destroy",
		    local ? G_CALLBACK (local_destroy)
			  : G_CALLBACK (remote_destroy),
		    NULL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);

  content = create_content (GTK_WINDOW (window), local);
  
  gtk_container_add (GTK_CONTAINER (window), content);

  gtk_widget_show_all (window);

  if (gtk_widget_get_realized (window))
#if defined (GDK_WINDOWING_X11)
    return GDK_WINDOW_XID (gtk_widget_get_window (window));
#elif defined (GDK_WINDOWING_WIN32)
    return (guint32) GDK_WINDOW_HWND (gtk_widget_get_window (window));
#elif defined (GDK_WINDOWING_BROADWAY)
    return (guint32) 0; /* Child windows not supported */
#endif
  else
    return 0;
}
