/* gtkstatusicon-x11.c:
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
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
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <gtk/gtk.h>

typedef enum
{
  TEST_STATUS_INFO,
  TEST_STATUS_QUESTION
} TestStatus;

static TestStatus status = TEST_STATUS_INFO;
static gint timeout = 0;

static void
update_icon (GtkStatusIcon *status_icon)
{
  gchar *icon_name;
  gchar *tooltip;

  if (status == TEST_STATUS_INFO)
    {
      icon_name = GTK_STOCK_DIALOG_INFO;
      tooltip = "Some Infromation ...";
    }
  else
    {
      icon_name = GTK_STOCK_DIALOG_QUESTION;
      tooltip = "Some Question ...";
    }

  gtk_status_icon_set_from_icon_name (status_icon, icon_name);
  gtk_status_icon_set_tooltip (status_icon, tooltip);
}

static gboolean
timeout_handler (gpointer data)
{
  GtkStatusIcon *icon = GTK_STATUS_ICON (data);

  if (status == TEST_STATUS_INFO)
    status = TEST_STATUS_QUESTION;
  else
    status = TEST_STATUS_INFO;

  update_icon (icon);

  return TRUE;
}

static void
blink_toggle_toggled (GtkToggleButton *toggle,
		      GtkStatusIcon   *icon)
{
  gtk_status_icon_set_blinking (icon, 
				gtk_toggle_button_get_active (toggle));
}

static void
visible_toggle_toggled (GtkToggleButton *toggle,
			GtkStatusIcon   *icon)
{
  gtk_status_icon_set_visible (icon, 
			       gtk_toggle_button_get_active (toggle));
}

static void
timeout_toggle_toggled (GtkToggleButton *toggle,
			GtkStatusIcon   *icon)
{
  if (timeout)
    {
      g_source_remove (timeout);
      timeout = 0;
    }
  else
    {
      timeout = g_timeout_add (2000, timeout_handler, icon);
    }
}

static void
icon_activated (GtkStatusIcon *icon)
{
  GtkWidget *dialog;
  GtkWidget *toggle;

  dialog = g_object_get_data (G_OBJECT (icon), "test-status-icon-dialog");
  if (dialog == NULL)
    {
      dialog = gtk_message_dialog_new (NULL, 0,
				       GTK_MESSAGE_QUESTION,
				       GTK_BUTTONS_CLOSE,
				       "You wanna test the status icon ?");

      gtk_window_set_position (dialog, GTK_WIN_POS_CENTER);

      g_object_set_data_full (G_OBJECT (icon), "test-status-icon-dialog",
			      dialog, (GDestroyNotify) gtk_widget_destroy);

      g_signal_connect (dialog, "response", 
			G_CALLBACK (gtk_widget_hide), NULL);
      g_signal_connect (dialog, "delete_event", 
			G_CALLBACK (gtk_widget_hide_on_delete), NULL);

      toggle = gtk_toggle_button_new_with_mnemonic ("_Show the icon");
      gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->vbox), toggle, TRUE, TRUE, 6);
      gtk_widget_show (toggle);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
				    gtk_status_icon_get_visible (icon));
      g_signal_connect (toggle, "toggled", 
			G_CALLBACK (visible_toggle_toggled), icon);

      toggle = gtk_toggle_button_new_with_mnemonic ("_Blink the icon");
      gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->vbox), toggle, TRUE, TRUE, 6);
      gtk_widget_show (toggle);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
				    gtk_status_icon_get_blinking (icon));
      g_signal_connect (toggle, "toggled", 
			G_CALLBACK (blink_toggle_toggled), icon);

      toggle = gtk_toggle_button_new_with_mnemonic ("_Change images");
      gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->vbox), toggle, TRUE, TRUE, 6);
      gtk_widget_show (toggle);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
				    timeout != 0);
      g_signal_connect (toggle, "toggled", 
			G_CALLBACK (timeout_toggle_toggled), icon);
    }

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
check_activated (GtkCheckMenuItem *item,
		 GtkStatusIcon    *icon)
{
  gtk_status_icon_set_blinking (icon, 
				gtk_check_menu_item_get_active (item));
}

static void
do_quit (GtkMenuItem   *item,
	 GtkStatusIcon *icon)
{
  gtk_status_icon_set_visible (icon, FALSE);
  g_object_unref (icon);
  gtk_main_quit ();
}

static void 
popup_menu (GtkStatusIcon *icon,
	    guint          button,
	    guint32        activate_time)
{
  GtkWidget *menu, *menuitem;

  menu = gtk_menu_new ();

  menuitem = gtk_check_menu_item_new_with_label ("Blink");
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem), 
				  gtk_status_icon_get_blinking (icon));
  g_signal_connect (menuitem, "activate", G_CALLBACK (check_activated), icon);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("Quit");
  g_signal_connect (menuitem, "activate", G_CALLBACK (do_quit), icon);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

  gtk_widget_show (menuitem);

  gtk_menu_popup (GTK_MENU (menu), 
		  NULL, NULL, NULL, NULL, 
		  button, activate_time);
}

int
main (int argc, char **argv)
{
  GtkStatusIcon *icon;

  gtk_init (&argc, &argv);

  icon = gtk_status_icon_new ();
  update_icon (icon);

  gtk_status_icon_set_blinking (GTK_STATUS_ICON (icon), TRUE);

  g_signal_connect (icon, "activate",
		    G_CALLBACK (icon_activated), NULL);

  g_signal_connect (icon, "popup-menu",
		    G_CALLBACK (popup_menu), NULL);
 
  timeout = g_timeout_add (2000, timeout_handler, icon);

  gtk_main ();

  return 0;
}
