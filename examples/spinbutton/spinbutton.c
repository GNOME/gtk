/* example-start spinbutton spinbutton.c */

#include <gtk/gtk.h>

static GtkWidget *spinner1;

void toggle_snap( GtkWidget     *widget,
		  GtkSpinButton *spin )
{
  gtk_spin_button_set_snap_to_ticks (spin, GTK_TOGGLE_BUTTON (widget)->active);
}

void toggle_numeric( GtkWidget *widget,
		     GtkSpinButton *spin )
{
  gtk_spin_button_set_numeric (spin, GTK_TOGGLE_BUTTON (widget)->active);
}

void change_digits( GtkWidget *widget,
	            GtkSpinButton *spin )
{
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spinner1),
                              gtk_spin_button_get_value_as_int (spin));
}

void get_value( GtkWidget *widget,
		gpointer data )
{
  gchar buf[32];
  GtkLabel *label;
  GtkSpinButton *spin;

  spin = GTK_SPIN_BUTTON (spinner1);
  label = GTK_LABEL (gtk_object_get_user_data (GTK_OBJECT (widget)));
  if (GPOINTER_TO_INT (data) == 1)
    sprintf (buf, "%d", gtk_spin_button_get_value_as_int (spin));
  else
    sprintf (buf, "%0.*f", spin->digits,
             gtk_spin_button_get_value_as_float (spin));
  gtk_label_set_text (label, buf);
}


int main( int   argc,
          char *argv[] )
{
  GtkWidget *window;
  GtkWidget *frame;
  GtkWidget *hbox;
  GtkWidget *main_vbox;
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *spinner2;
  GtkWidget *spinner;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *val_label;
  GtkAdjustment *adj;

  /* Initialise GTK */
  gtk_init(&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC (gtk_main_quit),
		      NULL);

  gtk_window_set_title (GTK_WINDOW (window), "Spin Button");

  main_vbox = gtk_vbox_new (FALSE, 5);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 10);
  gtk_container_add (GTK_CONTAINER (window), main_vbox);
  
  frame = gtk_frame_new ("Not accelerated");
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, TRUE, TRUE, 0);
  
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  
  /* Day, month, year spinners */
  
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 5);
  
  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);
  
  label = gtk_label_new ("Day :");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);
  
  adj = (GtkAdjustment *) gtk_adjustment_new (1.0, 1.0, 31.0, 1.0,
					      5.0, 0.0);
  spinner = gtk_spin_button_new (adj, 0, 0);
  gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner), TRUE);
  gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner),
				   GTK_SHADOW_OUT);
  gtk_box_pack_start (GTK_BOX (vbox2), spinner, FALSE, TRUE, 0);
  
  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);
  
  label = gtk_label_new ("Month :");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);
  
  adj = (GtkAdjustment *) gtk_adjustment_new (1.0, 1.0, 12.0, 1.0,
					      5.0, 0.0);
  spinner = gtk_spin_button_new (adj, 0, 0);
  gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner), TRUE);
  gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner),
				   GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (vbox2), spinner, FALSE, TRUE, 0);
  
  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);
  
  label = gtk_label_new ("Year :");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);
  
  adj = (GtkAdjustment *) gtk_adjustment_new (1998.0, 0.0, 2100.0,
					      1.0, 100.0, 0.0);
  spinner = gtk_spin_button_new (adj, 0, 0);
  gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner), FALSE);
  gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner),
				   GTK_SHADOW_IN);
  gtk_widget_set_usize (spinner, 55, 0);
  gtk_box_pack_start (GTK_BOX (vbox2), spinner, FALSE, TRUE, 0);
  
  frame = gtk_frame_new ("Accelerated");
  gtk_box_pack_start (GTK_BOX (main_vbox), frame, TRUE, TRUE, 0);
  
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 5);
  
  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);
  
  label = gtk_label_new ("Value :");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);
  
  adj = (GtkAdjustment *) gtk_adjustment_new (0.0, -10000.0, 10000.0,
					      0.5, 100.0, 0.0);
  spinner1 = gtk_spin_button_new (adj, 1.0, 2);
  gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner1), TRUE);
  gtk_widget_set_usize (spinner1, 100, 0);
  gtk_box_pack_start (GTK_BOX (vbox2), spinner1, FALSE, TRUE, 0);
  
  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 5);
  
  label = gtk_label_new ("Digits :");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, TRUE, 0);
  
  adj = (GtkAdjustment *) gtk_adjustment_new (2, 1, 5, 1, 1, 0);
  spinner2 = gtk_spin_button_new (adj, 0.0, 0);
  gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner2), TRUE);
  gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
		      GTK_SIGNAL_FUNC (change_digits),
		      (gpointer) spinner2);
  gtk_box_pack_start (GTK_BOX (vbox2), spinner2, FALSE, TRUE, 0);
  
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 5);
  
  button = gtk_check_button_new_with_label ("Snap to 0.5-ticks");
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC (toggle_snap),
		      spinner1);
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  
  button = gtk_check_button_new_with_label ("Numeric only input mode");
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC (toggle_numeric),
		      spinner1);
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  
  val_label = gtk_label_new ("");
  
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 5);
  button = gtk_button_new_with_label ("Value as Int");
  gtk_object_set_user_data (GTK_OBJECT (button), val_label);
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC (get_value),
		      GINT_TO_POINTER (1));
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
  
  button = gtk_button_new_with_label ("Value as Float");
  gtk_object_set_user_data (GTK_OBJECT (button), val_label);
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC (get_value),
		      GINT_TO_POINTER (2));
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
  
  gtk_box_pack_start (GTK_BOX (vbox), val_label, TRUE, TRUE, 0);
  gtk_label_set_text (GTK_LABEL (val_label), "0");
  
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, TRUE, 0);
  
  button = gtk_button_new_with_label ("Close");
  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
			     GTK_SIGNAL_FUNC (gtk_widget_destroy),
			     GTK_OBJECT (window));
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);

  gtk_widget_show_all (window);

  /* Enter the event loop */
  gtk_main ();
    
  return(0);
}
/* example-end */
