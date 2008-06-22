/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/* testsocket.c
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#undef GTK_DISABLE_DEPRECATED

#include "config.h"
#include <gtk/gtk.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int n_children = 0;

GSList *sockets = NULL;

GtkWidget *window;
GtkWidget *box;

typedef struct 
{
  GtkWidget *box;
  GtkWidget *frame;
  GtkWidget *socket;
} Socket;

extern guint32 create_child_plug (guint32  xid,
				  gboolean local);

static void
quit_cb (gpointer        callback_data,
	 guint           callback_action,
	 GtkWidget      *widget)
{
  GtkWidget *message_dialog = gtk_message_dialog_new (GTK_WINDOW (window), 0,
						      GTK_MESSAGE_QUESTION,
						      GTK_BUTTONS_YES_NO,
						      "Really Quit?");
  gtk_dialog_set_default_response (GTK_DIALOG (message_dialog), GTK_RESPONSE_NO);

  if (gtk_dialog_run (GTK_DIALOG (message_dialog)) == GTK_RESPONSE_YES)
    gtk_widget_destroy (window);

  gtk_widget_destroy (message_dialog);
}

static GtkItemFactoryEntry menu_items[] =
{
  { "/_File",            NULL,         NULL,                  0, "<Branch>" },
  { "/File/_Quit",       "<control>Q", quit_cb,               0 },
};

static void
socket_destroyed (GtkWidget *widget,
		  Socket    *socket)
{
  sockets = g_slist_remove (sockets, socket);
  g_free (socket);
}

static void
plug_added (GtkWidget *widget,
	    Socket    *socket)
{
  g_print ("Plug added to socket\n");
  
  gtk_widget_show (socket->socket);
  gtk_widget_hide (socket->frame);
}

static gboolean
plug_removed (GtkWidget *widget,
	      Socket    *socket)
{
  g_print ("Plug removed from socket\n");
  
  gtk_widget_hide (socket->socket);
  gtk_widget_show (socket->frame);
  
  return TRUE;
}

static Socket *
create_socket (void)
{
  GtkWidget *label;
  
  Socket *socket = g_new (Socket, 1);
  
  socket->box = gtk_vbox_new (FALSE, 0);

  socket->socket = gtk_socket_new ();
  
  gtk_box_pack_start (GTK_BOX (socket->box), socket->socket, TRUE, TRUE, 0);
  
  socket->frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (socket->frame), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (socket->box), socket->frame, TRUE, TRUE, 0);
  gtk_widget_show (socket->frame);
  
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), "<span color=\"red\">Empty</span>");
  gtk_container_add (GTK_CONTAINER (socket->frame), label);
  gtk_widget_show (label);

  sockets = g_slist_prepend (sockets, socket);


  g_signal_connect (socket->socket, "destroy",
		    G_CALLBACK (socket_destroyed), socket);
  g_signal_connect (socket->socket, "plug_added",
		    G_CALLBACK (plug_added), socket);
  g_signal_connect (socket->socket, "plug_removed",
		    G_CALLBACK (plug_removed), socket);

  return socket;
}

void
steal (GtkWidget *window, GtkEntry *entry)
{
  guint32 xid;
  const gchar *text;
  Socket *socket;

  text = gtk_entry_get_text (entry);

  xid = strtol (text, NULL, 0);
  if (xid == 0)
    {
      g_warning ("Invalid window id '%s'\n", text);
      return;
    }

  socket = create_socket ();
  gtk_box_pack_start (GTK_BOX (box), socket->box, TRUE, TRUE, 0);
  gtk_widget_show (socket->box);

  gtk_socket_steal (GTK_SOCKET (socket->socket), xid);
}

void
remove_child (GtkWidget *window)
{
  if (sockets)
    {
      Socket *socket = sockets->data;
      gtk_widget_destroy (socket->box);
    }
}

static gboolean
child_read_watch (GIOChannel *channel, GIOCondition cond, gpointer data)
{
  GIOStatus status;
  GError *error = NULL;
  char *line;
  gsize term;
  int xid;
  
  status = g_io_channel_read_line (channel, &line, NULL, &term, &error);
  switch (status)
    {
    case G_IO_STATUS_NORMAL:
      line[term] = '\0';
      xid = strtol (line, NULL, 0);
      if (xid == 0)
	{
	  fprintf (stderr, "Invalid window id '%s'\n", line);
	}
      else
	{
	  Socket *socket = create_socket ();
	  gtk_box_pack_start (GTK_BOX (box), socket->box, TRUE, TRUE, 0);
	  gtk_widget_show (socket->box);
	  
	  gtk_socket_add_id (GTK_SOCKET (socket->socket), xid);
	}
      g_free (line);
      return TRUE;
    case G_IO_STATUS_AGAIN:
      return TRUE;
    case G_IO_STATUS_EOF:
      n_children--;
      return FALSE;
    case G_IO_STATUS_ERROR:
      fprintf (stderr, "Error reading fd from child: %s\n", error->message);
      exit (1);
      return FALSE;
    default:
      g_assert_not_reached ();
      return FALSE;
    }
  
}

void
add_child (GtkWidget *window,
	   gboolean   active)
{
  Socket *socket;
  char *argv[3] = { "./testsocket_child", NULL, NULL };
  char buffer[20];
  int out_fd;
  GIOChannel *channel;
  GError *error = NULL;

  if (active)
    {
      socket = create_socket ();
      gtk_box_pack_start (GTK_BOX (box), socket->box, TRUE, TRUE, 0);
      gtk_widget_show (socket->box);
      sprintf(buffer, "%#lx", (gulong) gtk_socket_get_id (GTK_SOCKET (socket->socket)));
      argv[1] = buffer;
    }
  
  if (!g_spawn_async_with_pipes (NULL, argv, NULL, 0, NULL, NULL, NULL, NULL, &out_fd, NULL, &error))
    {
      fprintf (stderr, "Can't exec testsocket_child: %s\n", error->message);
      exit (1);
    }

  n_children++;
  channel = g_io_channel_unix_new (out_fd);
  g_io_channel_set_close_on_unref (channel, TRUE);
  g_io_channel_set_flags (channel, G_IO_FLAG_NONBLOCK, &error);
  if (error)
    {
      fprintf (stderr, "Error making channel non-blocking: %s\n", error->message);
      exit (1);
    }
  
  g_io_add_watch (channel, G_IO_IN | G_IO_HUP, child_read_watch, NULL);
  g_io_channel_unref (channel);
}

void
add_active_child (GtkWidget *window)
{
  add_child (window, TRUE);
}

void
add_passive_child (GtkWidget *window)
{
  add_child (window, FALSE);
}

void
add_local_active_child (GtkWidget *window)
{
  Socket *socket;

  socket = create_socket ();
  gtk_box_pack_start (GTK_BOX (box), socket->box, TRUE, TRUE, 0);
  gtk_widget_show (socket->box);

  create_child_plug (gtk_socket_get_id (GTK_SOCKET (socket->socket)), TRUE);
}

void
add_local_passive_child (GtkWidget *window)
{
  Socket *socket;
  GdkNativeWindow xid;

  socket = create_socket ();
  gtk_box_pack_start (GTK_BOX (box), socket->box, TRUE, TRUE, 0);
  gtk_widget_show (socket->box);

  xid = create_child_plug (0, TRUE);
  gtk_socket_add_id (GTK_SOCKET (socket->socket), xid);
}

static const char *
grab_string (int status)
{
  switch (status) {
  case GDK_GRAB_SUCCESS:          return "GrabSuccess";
  case GDK_GRAB_ALREADY_GRABBED:  return "AlreadyGrabbed";
  case GDK_GRAB_INVALID_TIME:     return "GrabInvalidTime";
  case GDK_GRAB_NOT_VIEWABLE:     return "GrabNotViewable";
  case GDK_GRAB_FROZEN:           return "GrabFrozen";
  default:
    {
      static char foo [255];
      sprintf (foo, "unknown status: %d", status);
      return foo;
    }
  }
}

static void
grab_window_toggled (GtkToggleButton *button,
		     GtkWidget       *widget)
{

  if (gtk_toggle_button_get_active (button))
    {
      int status;

      status = gdk_keyboard_grab (widget->window, FALSE, GDK_CURRENT_TIME);

      if (status != GDK_GRAB_SUCCESS)
	g_warning ("Could not grab keyboard!  (%s)", grab_string (status));

    } 
  else 
    {
      gdk_keyboard_ungrab (GDK_CURRENT_TIME);
    }
}

int
main (int argc, char *argv[])
{
  GtkWidget *button;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *entry;
  GtkWidget *checkbutton;
  GtkAccelGroup *accel_group;
  GtkItemFactory *item_factory;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "destroy",
		    G_CALLBACK (gtk_main_quit), NULL);
  
  gtk_window_set_title (GTK_WINDOW (window), "Socket Test");
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  accel_group = gtk_accel_group_new ();
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
  item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group);

  
  gtk_item_factory_create_items (item_factory,
				 G_N_ELEMENTS (menu_items), menu_items,
				 NULL);
      
  gtk_box_pack_start (GTK_BOX (vbox),
		      gtk_item_factory_get_widget (item_factory, "<main>"),
		      FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Add Active Child");
  gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, FALSE, 0);

  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (add_active_child), vbox);

  button = gtk_button_new_with_label ("Add Passive Child");
  gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, FALSE, 0);

  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (add_passive_child), vbox);

  button = gtk_button_new_with_label ("Add Local Active Child");
  gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, FALSE, 0);

  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (add_local_active_child), vbox);

  button = gtk_button_new_with_label ("Add Local Passive Child");
  gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, FALSE, 0);

  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (add_local_passive_child), vbox);

  button = gtk_button_new_with_label ("Remove Last Child");
  gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, FALSE, 0);

  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (remove_child), vbox);

  checkbutton = gtk_check_button_new_with_label ("Grab keyboard");
  gtk_box_pack_start (GTK_BOX (vbox), checkbutton, FALSE, FALSE, 0);

  g_signal_connect (checkbutton, "toggled",
		    G_CALLBACK (grab_window_toggled),
		    window);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX(hbox), entry, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Steal");
  gtk_box_pack_start (GTK_BOX(hbox), button, FALSE, FALSE, 0);

  g_signal_connect (button, "clicked",
		    G_CALLBACK (steal),
		    entry);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  box = hbox;
  
  gtk_widget_show_all (window);

  gtk_main ();

  if (n_children)
    {
      g_print ("Waiting for children to exit\n");

      while (n_children)
	g_main_context_iteration (NULL, TRUE);
    }

  return 0;
}
