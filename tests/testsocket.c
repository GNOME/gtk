#include <gtk/gtk.h>
#include "x11/gdkx.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

GtkWidget *window;
GtkWidget *vbox;
GtkWidget *lastsocket = NULL;

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
  if(lastsocket)
    gtk_widget_destroy (lastsocket);
  lastsocket = NULL;
}

void
add_child (GtkWidget *window)
{
  GtkWidget *socket;
  char *argv[3] = { "./testsocket_child", NULL, NULL };
  char buffer[20];
  GError *error = NULL;

  socket = gtk_socket_new ();
  gtk_box_pack_start (GTK_BOX (vbox), socket, TRUE, TRUE, 0);
  gtk_widget_show (socket);

  lastsocket = socket;

  sprintf(buffer, "%#lx", GDK_WINDOW_XWINDOW (socket->window));
  argv[1] = buffer;
  
#if 1
  if (!g_spawn_async (NULL, argv, NULL, 0, NULL, NULL, NULL, &error))
    {
      fprintf (stderr, "Can't exec testsocket_child: %s\n", error->message);
      exit (1);
    }
#else
  fprintf(stderr,"%s\n", buffer);
#endif
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

  button = gtk_button_new_with_label ("Add Child");
  gtk_box_pack_start (GTK_BOX(vbox), button, FALSE, FALSE, 0);

  gtk_signal_connect_object (GTK_OBJECT(button), "clicked",
			     GTK_SIGNAL_FUNC(add_child),
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

  return 0;
}


