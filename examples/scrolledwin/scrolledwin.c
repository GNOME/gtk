
#include <stdio.h>
#include <gtk/gtk.h>

static void destroy( GtkWidget *widget,
                     gpointer   data )
{
    gtk_main_quit ();
}

int main( int   argc,
          char *argv[] )
{
    static GtkWidget *window;
    GtkWidget *scrolled_window;
    GtkWidget *table;
    GtkWidget *button;
    char buffer[32];
    int i, j;
    
    gtk_init (&argc, &argv);
    
    /* Create a new dialog window for the scrolled window to be
     * packed into.  */
    window = gtk_dialog_new ();
    g_signal_connect (G_OBJECT (window), "destroy",
		      G_CALLBACK (destroy), NULL);
    gtk_window_set_title (GTK_WINDOW (window), "GtkScrolledWindow example");
    gtk_container_set_border_width (GTK_CONTAINER (window), 0);
    gtk_widget_set_size_request (window, 300, 300);
    
    /* create a new scrolled window. */
    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    
    gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 10);
    
    /* the policy is one of GTK_POLICY AUTOMATIC, or GTK_POLICY_ALWAYS.
     * GTK_POLICY_AUTOMATIC will automatically decide whether you need
     * scrollbars, whereas GTK_POLICY_ALWAYS will always leave the scrollbars
     * there.  The first one is the horizontal scrollbar, the second, 
     * the vertical. */
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    /* The dialog window is created with a vbox packed into it. */								
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(window)->vbox), scrolled_window, 
			TRUE, TRUE, 0);
    gtk_widget_show (scrolled_window);
    
    /* create a table of 10 by 10 squares. */
    table = gtk_table_new (10, 10, FALSE);
    
    /* set the spacing to 10 on x and 10 on y */
    gtk_table_set_row_spacings (GTK_TABLE (table), 10);
    gtk_table_set_col_spacings (GTK_TABLE (table), 10);
    
    /* pack the table into the scrolled window */
    gtk_scrolled_window_add_with_viewport (
                   GTK_SCROLLED_WINDOW (scrolled_window), table);
    gtk_widget_show (table);
    
    /* this simply creates a grid of toggle buttons on the table
     * to demonstrate the scrolled window. */
    for (i = 0; i < 10; i++)
       for (j = 0; j < 10; j++) {
          sprintf (buffer, "button (%d,%d)\n", i, j);
	  button = gtk_toggle_button_new_with_label (buffer);
	  gtk_table_attach_defaults (GTK_TABLE (table), button,
	                             i, i+1, j, j+1);
          gtk_widget_show (button);
       }
    
    /* Add a "close" button to the bottom of the dialog */
    button = gtk_button_new_with_label ("close");
    g_signal_connect_swapped (G_OBJECT (button), "clicked",
			      G_CALLBACK (gtk_widget_destroy),
			      G_OBJECT (window));
    
    /* this makes it so the button is the default. */
    
    gtk_widget_set_can_default (button, TRUE);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->action_area), button, TRUE, TRUE, 0);
    
    /* This grabs this button to be the default button. Simply hitting
     * the "Enter" key will cause this button to activate. */
    gtk_widget_grab_default (button);
    gtk_widget_show (button);
    
    gtk_widget_show (window);
    
    gtk_main();
    
    return 0;
}
