#include <gtk/gtk.h>
#include "gtkdial.h"

void
value_changed (GtkAdjustment *adjustment, GtkWidget *label)
{
  char buffer[16];

  sprintf(buffer,"%4.2f",adjustment->value);
  gtk_label_set (GTK_LABEL (label), buffer);
}

int 
main (int argc, char *argv[])
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
  
  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC (gtk_exit), NULL);
  
  gtk_container_border_width (GTK_CONTAINER (window), 10);

  vbox = gtk_vbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show(vbox);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX(vbox), frame, TRUE, TRUE, 0);
  gtk_widget_show (frame); 
 
  adjustment = GTK_ADJUSTMENT(gtk_adjustment_new (0, -1.0, 1.0, 0.01, 0.1, 0));
  
  dial = gtk_dial_new(adjustment);
  gtk_dial_set_update_policy (GTK_DIAL(dial), GTK_UPDATE_DELAYED);
  /*  gtk_widget_set_usize (dial, 100, 100); */
  
  gtk_container_add (GTK_CONTAINER (frame), dial);
  gtk_widget_show (dial);

  label = gtk_label_new("0.00");
  gtk_box_pack_start (GTK_BOX(vbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  gtk_signal_connect (GTK_OBJECT(adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (value_changed), label);
  
  gtk_widget_show (window);
  
  gtk_main ();
  
  return 0;
}
