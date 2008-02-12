
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "gtkdial.h"

void value_changed( GtkAdjustment *adjustment,
                    GtkWidget     *label )
{
  char buffer[16];

  sprintf(buffer,"%4.2f",adjustment->value);
  gtk_label_set_text (GTK_LABEL (label), buffer);
}

int main( int   argc,
          char *argv[])
{
  GtkWidget *window;
  GtkAdjustment *adjustment;
  GtkWidget *dial;
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *label;
  
  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  
  gtk_window_set_title (GTK_WINDOW (window), "Dial");
  
  g_signal_connect (G_OBJECT (window), "destroy",
		    G_CALLBACK (exit), NULL);
  
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);

  vbox = gtk_vbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show (vbox);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (vbox), frame);
  gtk_widget_show (frame); 
 
  adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 100, 0.01, 0.1, 0));
  
  dial = gtk_dial_new (adjustment);
  gtk_dial_set_update_policy (GTK_DIAL (dial), GTK_UPDATE_DELAYED);
  /*  gtk_widget_set_size_request (dial, 100, 100); */
  
  gtk_container_add (GTK_CONTAINER (frame), dial);
  gtk_widget_show (dial);

  label = gtk_label_new ("0.00");
  gtk_box_pack_end (GTK_BOX (vbox), label, 0, 0, 0);
  gtk_widget_show (label);

  g_signal_connect (G_OBJECT (adjustment), "value_changed",
		    G_CALLBACK (value_changed), (gpointer) label);
  
  gtk_widget_show (window);
  
  gtk_main ();
  
  return 0;
}
