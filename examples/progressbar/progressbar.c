/* example-start progressbar progressbar.c */

#include <gtk/gtk.h>

typedef struct _ProgressData {
    GtkWidget *window;
    GtkWidget *pbar;
    int timer;
} ProgressData;

/* Update the value of the progress bar so that we get
 * some movement */
gint progress_timeout( gpointer data )
{
    gfloat new_val;
    GtkAdjustment *adj;

    /* Calculate the value of the progress bar using the
     * value range set in the adjustment object */

    new_val = gtk_progress_get_value( GTK_PROGRESS(data) ) + 1;

    adj = GTK_PROGRESS (data)->adjustment;
    if (new_val > adj->upper)
      new_val = adj->lower;

    /* Set the new value */
    gtk_progress_set_value (GTK_PROGRESS (data), new_val);

    /* As this is a timeout function, return TRUE so that it
     * continues to get called */
    return(TRUE);
} 

/* Callback that toggles the text display within the progress
 * bar trough */
void toggle_show_text( GtkWidget    *widget,
		       ProgressData *pdata )
{
    gtk_progress_set_show_text (GTK_PROGRESS (pdata->pbar),
                                GTK_TOGGLE_BUTTON (widget)->active);
}

/* Callback that toggles the activity mode of the progress
 * bar */
void toggle_activity_mode( GtkWidget    *widget,
			   ProgressData *pdata )
{
    gtk_progress_set_activity_mode (GTK_PROGRESS (pdata->pbar),
                                    GTK_TOGGLE_BUTTON (widget)->active);
}

/* Callback that toggles the continuous mode of the progress
 * bar */
void set_continuous_mode( GtkWidget    *widget,
			  ProgressData *pdata )
{
    gtk_progress_bar_set_bar_style (GTK_PROGRESS_BAR (pdata->pbar),
                                    GTK_PROGRESS_CONTINUOUS);
}

/* Callback that toggles the discrete mode of the progress
 * bar */
void set_discrete_mode( GtkWidget    *widget,
			ProgressData *pdata )
{
    gtk_progress_bar_set_bar_style (GTK_PROGRESS_BAR (pdata->pbar),
                                    GTK_PROGRESS_DISCRETE);
}
 
/* Clean up allocated memory and remove the timer */
void destroy_progress( GtkWidget     *widget,
		       ProgressData *pdata)
{
    gtk_timeout_remove (pdata->timer);
    pdata->timer = 0;
    pdata->window = NULL;
    g_free(pdata);
    gtk_main_quit();
}

int main( int   argc,
          char *argv[])
{
    ProgressData *pdata;
    GtkWidget *align;
    GtkWidget *separator;
    GtkWidget *table;
    GtkAdjustment *adj;
    GtkWidget *button;
    GtkWidget *check;
    GtkWidget *vbox;

    gtk_init (&argc, &argv);

    /* Allocate memory for the data that is passwd to the callbacks */
    pdata = g_malloc( sizeof(ProgressData) );
  
    pdata->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_policy (GTK_WINDOW (pdata->window), FALSE, FALSE, TRUE);

    gtk_signal_connect (GTK_OBJECT (pdata->window), "destroy",
	                GTK_SIGNAL_FUNC (destroy_progress),
                        pdata);
    gtk_window_set_title (GTK_WINDOW (pdata->window), "GtkProgressBar");
    gtk_container_set_border_width (GTK_CONTAINER (pdata->window), 0);

    vbox = gtk_vbox_new (FALSE, 5);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
    gtk_container_add (GTK_CONTAINER (pdata->window), vbox);
    gtk_widget_show(vbox);
  
    /* Create a centering alignment object */
    align = gtk_alignment_new (0.5, 0.5, 0, 0);
    gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 5);
    gtk_widget_show(align);

    /* Create a Adjusment object to hold the range of the
     * progress bar */
    adj = (GtkAdjustment *) gtk_adjustment_new (0, 1, 150, 0, 0, 0);

    /* Create the GtkProgressBar using the adjustment */
    pdata->pbar = gtk_progress_bar_new_with_adjustment (adj);

    /* Set the format of the string that can be displayed in the
     * trough of the progress bar:
     * %p - percentage
     * %v - value
     * %l - lower range value
     * %u - upper range value */
    gtk_progress_set_format_string (GTK_PROGRESS (pdata->pbar),
	                            "%v from [%l-%u] (=%p%%)");
    gtk_container_add (GTK_CONTAINER (align), pdata->pbar);
    gtk_widget_show(pdata->pbar);

    /* Add a timer callback to update the value of the progress bar */
    pdata->timer = gtk_timeout_add (100, progress_timeout, pdata->pbar);

    separator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, FALSE, 0);
    gtk_widget_show(separator);

    /* rows, columns, homogeneous */
    table = gtk_table_new (2, 3, FALSE);
    gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, TRUE, 0);
    gtk_widget_show(table);

    /* Add a check button to select displaying of the trough text */
    check = gtk_check_button_new_with_label ("Show text");
    gtk_table_attach (GTK_TABLE (table), check, 0, 1, 0, 1,
                      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
                      5, 5);
    gtk_signal_connect (GTK_OBJECT (check), "clicked",
                        GTK_SIGNAL_FUNC (toggle_show_text),
                        pdata);
    gtk_widget_show(check);

    /* Add a check button to toggle activity mode */
    check = gtk_check_button_new_with_label ("Activity mode");
    gtk_table_attach (GTK_TABLE (table), check, 0, 1, 1, 2,
                      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
                      5, 5);
    gtk_signal_connect (GTK_OBJECT (check), "clicked",
                        GTK_SIGNAL_FUNC (toggle_activity_mode),
                        pdata);
    gtk_widget_show(check);

    separator = gtk_vseparator_new ();
    gtk_table_attach (GTK_TABLE (table), separator, 1, 2, 0, 2,
                      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
                      5, 5);
    gtk_widget_show(separator);

    /* Add a radio button to select continuous display mode */
    button = gtk_radio_button_new_with_label (NULL, "Continuous");
    gtk_table_attach (GTK_TABLE (table), button, 2, 3, 0, 1,
                      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
                      5, 5);
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
                        GTK_SIGNAL_FUNC (set_continuous_mode),
                        pdata);
    gtk_widget_show (button);

    /* Add a radio button to select discrete display mode */
    button = gtk_radio_button_new_with_label(
               gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
               "Discrete");
    gtk_table_attach (GTK_TABLE (table), button, 2, 3, 1, 2,
                      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
                      5, 5);
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
                        GTK_SIGNAL_FUNC (set_discrete_mode),
                        pdata);
    gtk_widget_show (button);

    separator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, FALSE, 0);
    gtk_widget_show(separator);

    /* Add a button to exit the program */
    button = gtk_button_new_with_label ("close");
    gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                               (GtkSignalFunc) gtk_widget_destroy,
                               GTK_OBJECT (pdata->window));
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

    /* This makes it so the button is the default. */
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);

    /* This grabs this button to be the default button. Simply hitting
     * the "Enter" key will cause this button to activate. */
    gtk_widget_grab_default (button);
    gtk_widget_show(button);

    gtk_widget_show (pdata->window);

    gtk_main ();
    
    return(0);
}
/* example-end */
