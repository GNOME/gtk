#include <gtk/gtk.h>

#if defined (GDK_WINDOWING_X11)
#include "x11/gdkx.h"
#elif defined (GDK_WINDOWING_WIN32)
#include "win32/gdkwin32.h"
#define GDK_WINDOW_XWINDOW(w) (guint)GDK_WINDOW_HWND(w)
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int n_children = 0;

GtkWidget *window;
GtkWidget *vbox;
GtkWidget *lastsocket = NULL;

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
  { "/_File",            NULL,         0,                     0, "<Branch>" },
  { "/File/_Quit",       "<control>Q", quit_cb,               0 },
};

void
steal (GtkWidget *window, GtkEntry *entry)
{
  guint32 xid;
  const gchar *text;
  GtkWidget *socket;

  text = gtk_entry_get_text (entry);

  xid = strtol (text, NULL, 0);
  if (xid == 0)
    {
      g_warning ("Invalid window id '%s'\n", text);
      return;
    }

  socket = gtk_socket_new ();
  gtk_box_pack_start (GTK_BOX (vbox), socket, TRUE, TRUE, 0);
  gtk_widget_show (socket);

  gtk_socket_steal (GTK_SOCKET (socket), xid);
}

void
remove_child (GtkWidget *window)
{
  if (lastsocket)
    gtk_widget_destroy (lastsocket);
  lastsocket = NULL;
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
	  GtkWidget *socket = gtk_socket_new ();
	  gtk_box_pack_start (GTK_BOX (vbox), socket, TRUE, TRUE, 0);
	  gtk_widget_show (socket);
	  
	  gtk_socket_steal (GTK_SOCKET (socket), xid);
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
  GtkWidget *socket;
  char *argv[3] = { "./testsocket_child", NULL, NULL };
  char buffer[20];
  int out_fd;
  GIOChannel *channel;
  GError *error = NULL;

  socket = gtk_socket_new ();
  gtk_box_pack_start (GTK_BOX (vbox), socket, TRUE, TRUE, 0);
  gtk_widget_show (socket);

  lastsocket = socket;

  if (active)
    {
      sprintf(buffer, "%#lx", GDK_WINDOW_XWINDOW (socket->window));
      argv[1] = buffer;
    }
  
#if 1
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
  
#else
  fprintf(stderr,"%s\n", buffer);
#endif
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
add_local_child (GtkWidget *window)
{
  GtkWidget *socket;

  socket = gtk_socket_new ();
  gtk_box_pack_start (GTK_BOX (vbox), socket, TRUE, TRUE, 0);
  gtk_widget_show (socket);

  lastsocket = socket;

  create_child_plug (GDK_WINDOW_XWINDOW (socket->window), TRUE);
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

  button = gtk_button_new_with_label ("Add Local Child");
  gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, FALSE, 0);

  gtk_signal_connect_object (GTK_OBJECT(button), "clicked",
			     GTK_SIGNAL_FUNC(add_local_child),
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
