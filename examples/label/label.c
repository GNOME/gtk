
#include <gtk/gtk.h>

int main( int   argc,
          char *argv[] )
{
  static GtkWidget *window = NULL;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *frame;
  GtkWidget *label;

  /* Initialise GTK */
  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "destroy",
		    G_CALLBACK (gtk_main_quit),
		    NULL);

  gtk_window_set_title (GTK_WINDOW (window), "Label");
  vbox = gtk_vbox_new (FALSE, 5);
  hbox = gtk_hbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (window), hbox);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (window), 5);
  
  frame = gtk_frame_new ("Normal Label");
  label = gtk_label_new ("This is a Normal label");
  gtk_container_add (GTK_CONTAINER (frame), label);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  
  frame = gtk_frame_new ("Multi-line Label");
  label = gtk_label_new ("This is a Multi-line label.\nSecond line\n" \
			 "Third line");
  gtk_container_add (GTK_CONTAINER (frame), label);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  
  frame = gtk_frame_new ("Left Justified Label");
  label = gtk_label_new ("This is a Left-Justified\n" \
			 "Multi-line label.\nThird      line");
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_container_add (GTK_CONTAINER (frame), label);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  
  frame = gtk_frame_new ("Right Justified Label");
  label = gtk_label_new ("This is a Right-Justified\nMulti-line label.\n" \
			 "Fourth line, (j/k)");
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);
  gtk_container_add (GTK_CONTAINER (frame), label);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

  vbox = gtk_vbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
  frame = gtk_frame_new ("Line wrapped label");
  label = gtk_label_new ("This is an example of a line-wrapped label.  It " \
			 "should not be taking up the entire             " /* big space to test spacing */\
			 "width allocated to it, but automatically " \
			 "wraps the words to fit.  " \
			 "The time has come, for all good men, to come to " \
			 "the aid of their party.  " \
			 "The sixth sheik's six sheep's sick.\n" \
			 "     It supports multiple paragraphs correctly, " \
			 "and  correctly   adds "\
			 "many          extra  spaces. ");
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_container_add (GTK_CONTAINER (frame), label);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  
  frame = gtk_frame_new ("Filled, wrapped label");
  label = gtk_label_new ("This is an example of a line-wrapped, filled label.  " \
			 "It should be taking "\
			 "up the entire              width allocated to it.  " \
			 "Here is a sentence to prove "\
			 "my point.  Here is another sentence. "\
			 "Here comes the sun, do de do de do.\n"\
			 "    This is a new paragraph.\n"\
			 "    This is another newer, longer, better " \
			 "paragraph.  It is coming to an end, "\
			 "unfortunately.");
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_FILL);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_container_add (GTK_CONTAINER (frame), label);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  
  frame = gtk_frame_new ("Underlined label");
  label = gtk_label_new ("This label is underlined!\n"
			 "This one is underlined in quite a funky fashion");
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_label_set_pattern (GTK_LABEL (label),
			 "_________________________ _ _________ _ ______     __ _______ ___");
  gtk_container_add (GTK_CONTAINER (frame), label);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  
  gtk_widget_show_all (window);

  gtk_main ();
  
  return 0;
}
