#include <gtk/gtk.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int n_children = 0;

GSList *sockets = NULL;

GtkWidget *window;
GtkWidget *vbox;

typedef struct 
{
  GtkWidget *box;
  GtkWidget *frame;
  GtkWidget *socket;
} Socket;

extern guint32 create_child_plug (GdkScreen *screen,
				  guint32  xid,
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
  { "/_File",            NULL,         0,                     0, "<Branch>" },
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


  g_signal_connect (G_OBJECT (socket->socket), "destroy",
		    G_CALLBACK (socket_destroyed), socket);
  g_signal_connect (G_OBJECT (socket->socket), "plug_added",
		    G_CALLBACK (plug_added), socket);
  g_signal_connect (G_OBJECT (socket->socket), "plug_removed",
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
  gtk_box_pack_start (GTK_BOX (vbox), socket->box, TRUE, TRUE, 0);
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
	  gtk_box_pack_start (GTK_BOX (vbox), socket->box, TRUE, TRUE, 0);
	  gtk_widget_show (socket->box);
	  
	  gtk_socket_add_id (GTK_SOCKET (socket->socket), xid);
	}
      g_free (line);
      return TRUE;
    case G_IO_STATUS_AGAIN:
      return TRUE;
    case G_IO_STATUS_EOF:
      n_children--;
      g_io_channel_close (channel);
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
  char *argv[4] = { "./testsocket_child", NULL, NULL, NULL };
  char buffer[20];
  char dpy_buf[60];
  char* screen_separator;
  int out_fd;
  GIOChannel *channel;
  GError *error = NULL;

  sprintf (dpy_buf, "--display=%s", 
	   gdk_display_get_name (gtk_widget_get_display (window)));
  screen_separator = strrchr (dpy_buf, '.');
  sprintf (screen_separator, ".%d", gdk_screen_get_number (gtk_widget_get_screen (window)));
  
  argv[1] = dpy_buf;

  if (active)
    {
      socket = create_socket ();
      gtk_box_pack_start (GTK_BOX (vbox), socket->box, TRUE, TRUE, 0);
      gtk_widget_show (socket->box);
      sprintf(buffer, "%#lx", (gulong) gtk_socket_get_id (GTK_SOCKET (socket->socket)));
      argv[2] = buffer;
    }
  
  if (!g_spawn_async_with_pipes (NULL, argv, NULL, 0, NULL, NULL, NULL, NULL, &out_fd, NULL, &error))
    {
      fprintf (stderr, "Can't exec testsocket_child: %s\n", error->message);
      exit (1);
    }

  n_children++;
  channel = g_io_channel_unix_new (out_fd);
  g_io_channel_set_flags (channel, G_IO_FLAG_NONBLOCK, &error);
  if (error)
    {
      fprintf (stderr, "Error making channel non-blocking: %s\n", error->message);
      exit (1);
    }
  
  g_io_add_watch (channel, G_IO_IN | G_IO_HUP, child_read_watch, NULL);
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
  gtk_box_pack_start (GTK_BOX (vbox), socket->box, TRUE, TRUE, 0);
  gtk_widget_show (socket->box);

  create_child_plug (NULL,
		     gtk_socket_get_id (GTK_SOCKET (socket->socket)), TRUE);
}

void
add_local_passive_child (GtkWidget *window)
{
  Socket *socket;
  GdkNativeWindow xid;

  socket = create_socket ();
  gtk_box_pack_start (GTK_BOX (vbox), socket->box, TRUE, TRUE, 0);
  gtk_widget_show (socket->box);

  xid = create_child_plug (gtk_widget_get_screen (window), 0, TRUE);
  gtk_socket_add_id (GTK_SOCKET (socket->socket), xid);
}

/* Create a new toplevel and reparent  */
void change_screen (GdkScreen *new_screen, GtkWidget *toplevel)
{
  GtkWidget *child = gtk_bin_get_child (GTK_BIN (toplevel));
  GtkWidget *new_toplevel;

  GdkGeometry geometry;

  new_toplevel= gtk_window_new (GTK_WINDOW_TOPLEVEL);
  
  gtk_window_set_screen (GTK_WINDOW (new_toplevel), new_screen);

  gtk_widget_set_name (new_toplevel, "main window");

  geometry.min_width = -1;
  geometry.min_height = -1;
  geometry.max_width = -1;
  geometry.max_height = G_MAXSHORT;
  gtk_window_set_geometry_hints (GTK_WINDOW (new_toplevel), NULL,
				 &geometry,
				 GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE);

  gtk_signal_connect (GTK_OBJECT (new_toplevel), "destroy",
		      GTK_SIGNAL_FUNC(gtk_main_quit),
		      NULL);
  gtk_signal_connect (GTK_OBJECT (new_toplevel), "delete-event",
		      GTK_SIGNAL_FUNC (gtk_false),
		      NULL);

  gtk_widget_reparent (GTK_WIDGET (child), new_toplevel);
  gtk_widget_show_all (new_toplevel);
  window = new_toplevel;

  g_signal_handlers_disconnect_by_func( G_OBJECT (toplevel), 
					G_CALLBACK (gtk_main_quit), NULL);
  g_signal_handlers_disconnect_by_func( G_OBJECT (toplevel), 
					G_CALLBACK (gtk_false), NULL);
  gtk_widget_destroy (toplevel);
}

void move_screen (GtkWidget *widget)
{
  GdkScreen *screen = gtk_widget_get_screen (window);
  GdkDisplay *display = gtk_widget_get_display (window);

  if (g_slist_length (sockets) != 0)
    {
      GtkWidget *dialog= gtk_message_dialog_new 
	(GTK_WINDOW (gtk_widget_get_toplevel (window)),
	 GTK_DIALOG_DESTROY_WITH_PARENT,
	 GTK_MESSAGE_ERROR,
	 GTK_BUTTONS_OK,
	 "Please remove all sockets before changing screen");
      
      gtk_window_set_screen (GTK_WINDOW (dialog), screen);
      gtk_widget_show (dialog);
      g_signal_connect (G_OBJECT (dialog), "response",
			G_CALLBACK (gtk_widget_destroy),
			NULL);
    }
  else
    {
      if (gdk_display_get_n_screens(display) > 1)
	{
	  GdkScreen *new_screen;
	  int number_of_screens = gdk_display_get_n_screens (display);
	  int screen_num = gdk_screen_get_number (screen);
	  if ((screen_num +1) < number_of_screens)
	    new_screen = gdk_display_get_screen (display, screen_num + 1);
	  else
	    new_screen = gdk_display_get_screen (display, 0);
	  change_screen (new_screen, gtk_widget_get_toplevel (window)); 
	}
    }
}

int
main (int argc, char *argv[])
{
  GtkWidget *button;
  GtkWidget *hbox;
  GtkWidget *entry;
  GtkAccelGroup *accel_group;
  GtkItemFactory *item_factory;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      (GtkSignalFunc) gtk_main_quit, NULL);
  
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
  
  if (gdk_display_get_n_screens (gtk_widget_get_display (window)) > 1)
    {
      button = gtk_button_new_with_label ("Move the next screen");
      gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, FALSE, 0);
      gtk_signal_connect_object (GTK_OBJECT(button), "clicked",
				 GTK_SIGNAL_FUNC(move_screen),
				 GTK_OBJECT(vbox));
    }
  
  button = gtk_button_new_with_label ("Add Active Child");
  gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, FALSE, 0);

  gtk_signal_connect_object (GTK_OBJECT(button), "clicked",
			     GTK_SIGNAL_FUNC(add_active_child),
			     GTK_OBJECT(vbox));

  button = gtk_button_new_with_label ("Add Passive Child");
  gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, FALSE, 0);

  gtk_signal_connect_object (GTK_OBJECT(button), "clicked",
			     GTK_SIGNAL_FUNC(add_passive_child),
			     GTK_OBJECT(vbox));

  button = gtk_button_new_with_label ("Add Local Active Child");
  gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, FALSE, 0);

  gtk_signal_connect_object (GTK_OBJECT(button), "clicked",
			     GTK_SIGNAL_FUNC(add_local_active_child),
			     GTK_OBJECT(vbox));

  button = gtk_button_new_with_label ("Add Local Passive Child");
  gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, FALSE, 0);

  gtk_signal_connect_object (GTK_OBJECT(button), "clicked",
			     GTK_SIGNAL_FUNC(add_local_passive_child),
			     GTK_OBJECT(vbox));

  button = gtk_button_new_with_label ("Remove Last Child");
  gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, FALSE, 0);

  gtk_signal_connect_object (GTK_OBJECT(button), "clicked",
			     GTK_SIGNAL_FUNC(remove_child),
			     GTK_OBJECT(vbox));

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX(hbox), entry, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Steal");
  gtk_box_pack_start (GTK_BOX(hbox), button, FALSE, FALSE, 0);

  gtk_signal_connect (GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(steal),
		      entry);

  gtk_widget_show_all (window);

  gtk_main ();

  if (n_children)
    {
      g_print ("Waiting for children to exit\n");

      while (n_children)
	g_main_iteration (TRUE);
    }

  return 0;
}
