#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

void
remove_buttons (GtkWidget *widget, GtkWidget *other_button)
{
  gtk_widget_destroy (other_button);
  gtk_widget_destroy (widget);
}

void
add_buttons (GtkWidget *widget, GtkWidget *box)
{
  GtkWidget *add_button;
  GtkWidget *remove_button;
  GtkWidget *toplevel = gtk_widget_get_toplevel (box);

  add_button = gtk_button_new_with_mnemonic ("_Add");
  gtk_box_pack_start (GTK_BOX (box), add_button, TRUE, TRUE, 0);
  gtk_widget_show (add_button);

  gtk_signal_connect (GTK_OBJECT (add_button), "clicked",
		      GTK_SIGNAL_FUNC (add_buttons),
		      box);

  remove_button = gtk_button_new_with_mnemonic ("_Remove");
  gtk_box_pack_start (GTK_BOX (box), remove_button, TRUE, TRUE, 0);
  gtk_widget_show (remove_button);

  gtk_signal_connect (GTK_OBJECT (remove_button), "clicked",
		      GTK_SIGNAL_FUNC (remove_buttons),
		      add_button);
}

int
main (int argc, char *argv[])
{
  guint32 xid;

  GtkWidget *window;
  GtkWidget *hbox;
  GtkWidget *entry;
  GtkWidget *button;
  gtk_init (&argc, &argv);

  if (argc < 2)
    {
      fprintf (stderr, "Usage: testsocket_child WINDOW_ID\n");
      exit (1);
    }

  xid = strtol (argv[1], NULL, 0);
  if (xid == 0)
    {
      fprintf (stderr, "Invalid window id '%s'\n", argv[1]);
      exit (1);
    }

  window = gtk_plug_new (xid);
  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      (GtkSignalFunc) gtk_exit, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), hbox);
  gtk_widget_show (hbox);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
  gtk_widget_show (entry);

  button = gtk_button_new_with_mnemonic ("_Close");
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);

  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
			     GTK_SIGNAL_FUNC (gtk_widget_destroy),
			     GTK_OBJECT (window));

  add_buttons (NULL, hbox);
  
  gtk_widget_show (window);

  gtk_main ();

  return 0;
}


