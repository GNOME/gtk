
#include <gtk/gtk.h>

/* Get the selected filename and print it to the console */
void file_ok_sel( GtkWidget        *w,
                  GtkFileSelection *fs )
{
    g_print ("%s\n", gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
}

void destroy( GtkWidget *widget,
              gpointer   data )
{
    gtk_main_quit ();
}

int main( int   argc,
          char *argv[] )
{
    GtkWidget *filew;
    
    gtk_init (&argc, &argv);
    
    /* Create a new file selection widget */
    filew = gtk_file_selection_new ("File selection");
    
    g_signal_connect (GTK_OBJECT (filew), "destroy",
			GTK_SIGNAL_FUNC (destroy), &filew);
    /* Connect the ok_button to file_ok_sel function */
    g_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
			"clicked", GTK_SIGNAL_FUNC (file_ok_sel), filew );
    
    /* Connect the cancel_button to destroy the widget */
    g_signal_connect_swapped (GTK_OBJECT (GTK_FILE_SELECTION
                                            (filew)->cancel_button),
			       "clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
			       GTK_OBJECT (filew));
    
    /* Lets set the filename, as if this were a save dialog, and we are giving
     a default filename */
    gtk_file_selection_set_filename (GTK_FILE_SELECTION(filew), 
				     "penguin.png");
    
    gtk_widget_show(filew);
    gtk_main ();
    return 0;
}
