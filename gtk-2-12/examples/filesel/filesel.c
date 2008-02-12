
#include <gtk/gtk.h>

/* Get the selected filename and print it to the console */
static void file_ok_sel( GtkWidget        *w,
                         GtkFileSelection *fs )
{
    g_print ("%s\n", gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs)));
}

int main( int   argc,
          char *argv[] )
{
    GtkWidget *filew;
    
    gtk_init (&argc, &argv);
    
    /* Create a new file selection widget */
    filew = gtk_file_selection_new ("File selection");
    
    g_signal_connect (G_OBJECT (filew), "destroy",
	              G_CALLBACK (gtk_main_quit), NULL);
    /* Connect the ok_button to file_ok_sel function */
    g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
		      "clicked", G_CALLBACK (file_ok_sel), (gpointer) filew);
    
    /* Connect the cancel_button to destroy the widget */
    g_signal_connect_swapped (G_OBJECT (GTK_FILE_SELECTION (filew)->cancel_button),
	                      "clicked", G_CALLBACK (gtk_widget_destroy),
			      G_OBJECT (filew));
    
    /* Lets set the filename, as if this were a save dialog, and we are giving
     a default filename */
    gtk_file_selection_set_filename (GTK_FILE_SELECTION(filew), 
				     "penguin.png");
    
    gtk_widget_show (filew);
    gtk_main ();
    return 0;
}
