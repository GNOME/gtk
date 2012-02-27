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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <gtk/gtk.h>
#include <stdlib.h>

#include "prop-editor.h"

typedef enum
{
  TEST_STATUS_INFO,
  TEST_STATUS_QUESTION
} TestStatus;

static TestStatus status = TEST_STATUS_INFO;
static gint timeout = 0;
static GSList *icons = NULL;

static void
size_changed_cb (GtkStatusIcon *icon,
		 int size)
{
  g_print ("status icon %p size-changed size = %d\n", icon, size);
}

static void
embedded_changed_cb (GtkStatusIcon *icon)
{
  g_print ("status icon %p embedded changed to %d\n", icon,
	   gtk_status_icon_is_embedded (icon));
}

static void
orientation_changed_cb (GtkStatusIcon *icon)
{
  GtkOrientation orientation;

  g_object_get (icon, "orientation", &orientation, NULL);
  g_print ("status icon %p orientation changed to %d\n", icon, orientation);
}

static void
screen_changed_cb (GtkStatusIcon *icon)
{
  g_print ("status icon %p screen changed to %p\n", icon,
	   gtk_status_icon_get_screen (icon));
}

static void
update_icons (void)
{
  GSList *l;
  gchar *icon_name;
  gchar *tooltip;

  if (status == TEST_STATUS_INFO)
    {
      icon_name = "dialog-information";
      tooltip = "Some Information ...";
    }
  else
    {
      icon_name = "dialog-question";
      tooltip = "Some Question ...";
    }

  for (l = icons; l; l = l->next)
    {
      GtkStatusIcon *status_icon = l->data;

      gtk_status_icon_set_from_icon_name (status_icon, icon_name);
      gtk_status_icon_set_tooltip_text (status_icon, tooltip);
    }
}

static gboolean
timeout_handler (gpointer data)
{
  if (status == TEST_STATUS_INFO)
    status = TEST_STATUS_QUESTION;
  else
    status = TEST_STATUS_INFO;

  update_icons ();

  return TRUE;
}

static void
visible_toggle_toggled (GtkToggleButton *toggle)
{
  GSList *l;

  for (l = icons; l; l = l->next)
    gtk_status_icon_set_visible (GTK_STATUS_ICON (l->data),
                                 gtk_toggle_button_get_active (toggle));
}

static void
timeout_toggle_toggled (GtkToggleButton *toggle)
{
  if (timeout)
    {
      g_source_remove (timeout);
      timeout = 0;
    }
  else
    {
      timeout = gdk_threads_add_timeout (2000, timeout_handler, NULL);
    }
}

static void
icon_activated (GtkStatusIcon *icon)
{
  GtkWidget *content_area;
  GtkWidget *dialog;
  GtkWidget *toggle;

  dialog = g_object_get_data (G_OBJECT (icon), "test-status-icon-dialog");
  if (dialog == NULL)
    {
      dialog = gtk_message_dialog_new (NULL, 0,
				       GTK_MESSAGE_QUESTION,
				       GTK_BUTTONS_CLOSE,
				       "You wanna test the status icon ?");

      gtk_window_set_screen (GTK_WINDOW (dialog), gtk_status_icon_get_screen (icon));
      gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

      g_object_set_data_full (G_OBJECT (icon), "test-status-icon-dialog",
			      dialog, (GDestroyNotify) gtk_widget_destroy);

      g_signal_connect (dialog, "response", 
			G_CALLBACK (gtk_widget_hide), NULL);
      g_signal_connect (dialog, "delete_event", 
			G_CALLBACK (gtk_widget_hide_on_delete), NULL);

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

      toggle = gtk_toggle_button_new_with_mnemonic ("_Show the icon");
      gtk_box_pack_end (GTK_BOX (content_area), toggle, TRUE, TRUE, 6);
      gtk_widget_show (toggle);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
				    gtk_status_icon_get_visible (icon));
      g_signal_connect (toggle, "toggled", 
			G_CALLBACK (visible_toggle_toggled), NULL);

      toggle = gtk_toggle_button_new_with_mnemonic ("_Change images");
      gtk_box_pack_end (GTK_BOX (content_area), toggle, TRUE, TRUE, 6);
      gtk_widget_show (toggle);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
				    timeout != 0);
      g_signal_connect (toggle, "toggled", 
			G_CALLBACK (timeout_toggle_toggled), NULL);
    }

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
do_properties (GtkMenuItem   *item,
	       GtkStatusIcon *icon)
{
	static GtkWidget *editor = NULL;

	if (editor == NULL) {
		editor = create_prop_editor (G_OBJECT (icon), GTK_TYPE_STATUS_ICON);
		g_signal_connect (editor, "destroy", G_CALLBACK (gtk_widget_destroyed), &editor);
	}

	gtk_window_present (GTK_WINDOW (editor));
}

static void
do_quit (GtkMenuItem *item)
{
  GSList *l;

  for (l = icons; l; l = l->next)
    {
      GtkStatusIcon *icon = l->data;

      gtk_status_icon_set_visible (icon, FALSE);
      g_object_unref (icon);
    }

  g_slist_free (icons);
  icons = NULL;

  gtk_main_quit ();
}

static void
do_exit (GtkMenuItem *item)
{
  exit (0);
}

static void 
popup_menu (GtkStatusIcon *icon,
	    guint          button,
	    guint32        activate_time)
{
  GtkWidget *menu, *menuitem;

  menu = gtk_menu_new ();

  gtk_menu_set_screen (GTK_MENU (menu),
                       gtk_status_icon_get_screen (icon));

  menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_PROPERTIES, NULL);
  g_signal_connect (menuitem, "activate", G_CALLBACK (do_properties), icon);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("Quit");
  g_signal_connect (menuitem, "activate", G_CALLBACK (do_quit), NULL);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

  gtk_widget_show (menuitem);

  menuitem = gtk_menu_item_new_with_label ("Exit abruptly");
  g_signal_connect (menuitem, "activate", G_CALLBACK (do_exit), NULL);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

  gtk_widget_show (menuitem);

  gtk_menu_popup (GTK_MENU (menu), 
		  NULL, NULL,
		  gtk_status_icon_position_menu, icon,
		  button, activate_time);
}

int
main (int argc, char **argv)
{
  GdkDisplay *display;
  guint n_screens, i;

  gtk_init (&argc, &argv);

  display = gdk_display_get_default ();

  n_screens = gdk_display_get_n_screens (display);

  for (i = 0; i < n_screens; i++)
    {
      GtkStatusIcon *icon;

      icon = gtk_status_icon_new ();
      gtk_status_icon_set_screen (icon, gdk_display_get_screen (display, i));
      update_icons ();

      g_signal_connect (icon, "size-changed", G_CALLBACK (size_changed_cb), NULL);
      g_signal_connect (icon, "notify::embedded", G_CALLBACK (embedded_changed_cb), NULL);
      g_signal_connect (icon, "notify::orientation", G_CALLBACK (orientation_changed_cb), NULL);
      g_signal_connect (icon, "notify::screen", G_CALLBACK (screen_changed_cb), NULL);
      g_print ("icon size %d\n", gtk_status_icon_get_size (icon));

      g_signal_connect (icon, "activate",
                        G_CALLBACK (icon_activated), NULL);

      g_signal_connect (icon, "popup-menu",
                        G_CALLBACK (popup_menu), NULL);

      icons = g_slist_append (icons, icon);
 
      update_icons ();

      timeout = gdk_threads_add_timeout (2000, timeout_handler, icon);
    }

  gtk_main ();

  return 0;
}
